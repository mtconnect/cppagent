
//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//

#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>

#include "mtconnect/config.hpp"
#include "mtconnect/pipeline/mtconnect_xml_transform.hpp"
#include "mtconnect/pipeline/response_document.hpp"
#include "session.hpp"

namespace mtconnect::source::adapter::agent_adapter {
  using namespace std;
  namespace asio = boost::asio;
  namespace beast = boost::beast;
  namespace http = boost::beast::http;
  namespace ssl = boost::asio::ssl;
  using tcp = boost::asio::ip::tcp;

  /// @brief A session implementation that where the derived classes can support HTTP or HTTPS
  /// @tparam Derived
  template <class Derived>
  class SessionImpl : public Session
  {
    /// @brief A list of HTTP requests
    using RequestQueue = std::list<Request>;

  public:
    /// @brief Cast this class as the derived class
    /// @return reference to the derived class
    Derived &derived() { return static_cast<Derived &>(*this); }
    /// @brief Immutably cast this class as its derived subclass
    /// @return const reference to the derived class
    const Derived &derived() const { return static_cast<const Derived &>(*this); }

    // Objects are constructed with a strand to
    // ensure that handlers do not execute concurrently.
    SessionImpl(boost::asio::io_context::strand &strand, const url::Url &url)
      : m_resolver(strand.context()), m_strand(strand), m_url(url), m_chunk(1 * 1024 * 1024)
    {}

    virtual ~SessionImpl() { stop(); }

    /// @brief see if the socket is connected
    /// @return `true` if the socket is open
    bool isOpen() const override { return derived().lowestLayer().socket().is_open(); }

    /// @brief close the connection
    void close() override { derived().lowestLayer().socket().close(); }

    /// @brief Method called when a request fails
    ///
    /// Closes the socket and resets the request
    /// @param ec error code to report
    /// @param what the reason why the failure occurred
    void failed(std::error_code ec, const char *what) override
    {
      derived().lowestLayer().socket().close();

      LOG(error) << "Agent Adapter Connection Failed: " << m_url.getUrlText(nullopt);
      if (m_request)
        LOG(error) << "Agent Adapter Target: " << m_request->getTarget(m_url);
      LOG(error) << "Agent Adapter " << what << ": " << ec.message() << "\n";
      m_request.reset();
      if (m_failed)
        m_failed(ec);
    }

    void stop() override { m_request.reset(); }

    bool makeRequest(const Request &req) override
    {
      if (!m_request)
      {
        m_request.emplace(req);

        // Clean out any previous data
        m_buffer.clear();
        m_contentType.clear();
        m_boundary.clear();
        m_headerParser.reset();
        m_chunkParser.reset();
        m_textParser.reset();
        m_req.reset();
        m_hasHeader = false;
        if (m_chunk.size() > 0)
          m_chunk.consume(m_chunk.size());

        // Check if we are discussected.
        if (!derived().lowestLayer().socket().is_open())
        {
          // Try to connect
          connect();
          return false;
        }
        else
        {
          request();
          return true;
        }
      }
      else
      {
        m_queue.emplace_back(req);
        return false;
      }
    }

    /// @brief Process data from the remote agent
    /// @param data the payload from the agent
    void processData(const std::string &data)
    {
      try
      {
        if (m_handler && m_handler->m_processData)
          m_handler->m_processData(data, m_identity);
      }
      catch (std::system_error &e)
      {
        LOG(warning) << "AgentAdapter - Error occurred processing data: " << e.what();
        if (e.code().category() == TheErrorCategory())
          failed(e.code(), "Error occurred processing data");
        else
          failed(source::make_error_code(ErrorCode::RETRY_REQUEST),
                 "Exception occurred in AgentAdapter::processData");
      }
      catch (std::exception &e)
      {
        LOG(error) << "AgentAdapter - Error occurred processing data: " << e.what();
        failed(source::make_error_code(ErrorCode::RETRY_REQUEST),
               "Exception occurred in AgentAdapter::processData");
      }
      catch (...)
      {
        failed(source::make_error_code(ErrorCode::RETRY_REQUEST),
               "Unknown exception in AgentAdapter::processData");
      }
    }

    /// @brief Connect to the remote agent
    virtual void connect()
    {
      // If the address is an IP address, we do not need to resolve.
      if (m_resolution)
      {
        beast::error_code ec;
        onResolve(ec, *m_resolution);
      }
      else if (holds_alternative<asio::ip::address>(m_url.m_host))
      {
        asio::ip::tcp::endpoint ep(get<asio::ip::address>(m_url.m_host), m_url.getPort());

        // Create the results type and call on resolve directly.
        using results_type = tcp::resolver::results_type;
        auto results = results_type::create(ep, m_url.getHost(), m_url.getService());

        beast::error_code ec;
        onResolve(ec, results);
      }
      else
      {
        derived().lowestLayer().expires_after(m_timeout);

        // Do an async resolution of the address.
        m_resolver.async_resolve(
            get<string>(m_url.m_host), m_url.getService(),
            asio::bind_executor(
                m_strand, beast::bind_front_handler(&SessionImpl::onResolve, derived().getptr())));
      }
    }

    /// @brief Callback when the host name needs to be resolved
    /// @param ec error code if resultion fails
    /// @param results the resolution results
    void onResolve(beast::error_code ec, tcp::resolver::results_type results)
    {
      if (ec)
      {
        // If we can't resolve, then shut down the adapter
        LOG(error) << "Cannot resolve address " << m_url.getHost() << ", shutting down";
        LOG(error) << "  Reason: " << ec.category().name() << " " << ec.message();
        return failed(source::make_error_code(source::ErrorCode::ADAPTER_FAILED), "resolve");
      }

      if (!m_request)
      {
        LOG(error) << "Resolved but no request";
        return;
      }

      if (!m_resolution)
        m_resolution.emplace(results);

      if (m_handler && m_handler->m_connecting)
        m_handler->m_connecting(m_identity);

      // Set a timeout on the operation
      derived().lowestLayer().expires_after(m_timeout);

      // Make the connection on the IP address we get from a lookup
      derived().lowestLayer().async_connect(
          results, asio::bind_executor(m_strand, beast::bind_front_handler(&Derived::onConnect,
                                                                           derived().getptr())));
    }

    /// @brief Write a request to the remote agent
    void request()
    {
      if (!m_request)
        return;

      // Set up an HTTP GET request message
      m_req.emplace();
      m_req->version(11);
      m_req->method(http::verb::get);
      m_req->target(m_request->getTarget(m_url));
      m_req->set(http::field::host, m_url.getHost());
      m_req->set(http::field::user_agent, "MTConnect Agent/2.0");
      m_req->set(http::field::connection, "keep-alive");

      if (m_closeConnectionAfterResponse)
      {
        m_req->set(http::field::connection, "close");
      }

      derived().lowestLayer().expires_after(m_timeout);

      LOG(debug) << "Agent adapter making request: " << m_url.getUrlText(nullopt) << " target "
                 << m_request->getTarget(m_url);

      http::async_write(derived().stream(), *m_req,
                        beast::bind_front_handler(&SessionImpl::onWrite, derived().getptr()));
    }

    /// @brief Callback on successful write to the agent
    /// @param ec error code if something failed
    /// @param bytes_transferred number of bytes transferred (unused)
    void onWrite(beast::error_code ec, std::size_t bytes_transferred)
    {
      boost::ignore_unused(bytes_transferred);

      if (ec)
      {
        LOG(error) << "Cannot send request: " << ec.category().name() << " " << ec.message();
        return failed(source::make_error_code(ErrorCode::RETRY_REQUEST), "write");
      }

      if (!m_request)
      {
        LOG(error) << "Wrote but no request";
        return failed(source::make_error_code(ErrorCode::RETRY_REQUEST), "write");
      }

      derived().lowestLayer().expires_after(m_timeout);

      // Receive the HTTP response
      m_headerParser.emplace();
      http::async_read_header(
          derived().stream(), m_buffer, *m_headerParser,
          asio::bind_executor(m_strand,
                              beast::bind_front_handler(&Derived::onHeader, derived().getptr())));
    }

    /// @brief Callback after write to process the message header
    /// @param ec error code if something failed
    /// @param bytes_transferred number of bytes transferred
    void onHeader(beast::error_code ec, std::size_t bytes_transferred)
    {
      if (ec)
      {
        LOG(error) << "Agent Adapter Error getting request header: " << ec.category().name() << " "
                   << ec.message();
        derived().lowestLayer().close();
        if (m_request->m_stream && ec == beast::error::timeout)
        {
          LOG(warning) << "Switching to polling";
          return failed(source::make_error_code(ErrorCode::MULTIPART_STREAM_FAILED), "header");
        }
        else
        {
          return failed(source::make_error_code(ErrorCode::RETRY_REQUEST), "header");
        }

        return;
      }

      if (!m_request)
      {
        LOG(error) << "Received a header but no request";
        return failed(source::make_error_code(ErrorCode::RETRY_REQUEST), "header");
      }

      auto &msg = m_headerParser->get();
      if (msg.version() < 11)
      {
        LOG(trace) << "Agent adapter: HTTP 10 requires close on read";
        m_closeOnRead = true;
      }
      else if (auto a = msg.find(beast::http::field::connection); a != msg.end())
      {
        m_closeOnRead = a->value() == "close";
      }

      if (m_request->m_stream && m_headerParser->chunked())
      {
        onChunkedContent();
      }
      else
      {
        derived().lowestLayer().expires_after(m_timeout);

        m_textParser.emplace(std::move(*m_headerParser));
        http::async_read(derived().stream(), m_buffer, *m_textParser,
                         asio::bind_executor(m_strand, beast::bind_front_handler(
                                                           &Derived::onRead, derived().getptr())));
      }
    }

    /// @brief Callback after header processing to read the body of the response
    /// @param ec error code if something failed
    /// @param bytes_transferred number of bytes transferred (unused)
    void onRead(beast::error_code ec, std::size_t bytes_transferred)
    {
      boost::ignore_unused(bytes_transferred);

      if (ec)
      {
        LOG(error) << "Error getting response: " << ec.category().name() << " " << ec.message();
        return failed(source::make_error_code(ErrorCode::RETRY_REQUEST), "read");
      }

      if (!m_request)
      {
        LOG(error) << "read data but no request";
        return failed(source::make_error_code(ErrorCode::RETRY_REQUEST), "header");
      }

      derived().lowestLayer().expires_after(m_timeout);

      if (!derived().lowestLayer().socket().is_open())
        derived().disconnect();

      processData(m_textParser->get().body());

      m_textParser.reset();
      m_req.reset();

      auto next = m_request->m_next;
      m_request.reset();

      if (m_closeOnRead)
        close();

      if (next)
      {
        next();
      }
      else if (!m_queue.empty())
      {
        Request req = m_queue.front();
        m_queue.pop_front();
        makeRequest(req);
      }
    }

    /// @name Streaming related methods
    ///@{

    /// @brief Find the x-multipart-replace MIME boundary
    /// @return the boundary string
    inline string findBoundary()
    {
      auto f = m_headerParser->get().find(http::field::content_type);
      if (f != m_headerParser->get().end())
      {
        m_contentType = string(f->value());
        auto i = m_contentType.find(';');
        if (i != string::npos)
        {
          auto b = m_contentType.substr(i + 1);
          m_contentType = m_contentType.substr(0, i);
          auto p = b.find("=");
          if (p != string::npos)
          {
            if (b.substr(0, p) == "boundary")
            {
              return "--"s + b.substr(p + 1);
            }
          }
        }
      }

      return "";
    }

    /// @brief Create a function to handle the chunk header
    ///
    /// Sets the chunk parser on chunk header
    void createChunkHeaderHandler()
    {
      m_chunkHeaderHandler = [this](std::uint64_t size, boost::string_view extensions,
                                    boost::system::error_code &ec) {
#if 0
        http::chunk_extensions ce;
        ce.parse(extensions, ec);
        for (auto &c : ce)
          cout << "Ext: " << c.first << ": " << c.second << endl;
#endif
        derived().lowestLayer().expires_after(m_timeout);

        if (ec)
        {
          return failed(ec, "Failed in chunked extension parse");
        }
      };

      m_chunkParser->on_chunk_header(m_chunkHeaderHandler);
    }

    /// @brief Parse the header and get the size and type
    /// @return `true` if successful
    bool parseMimeHeader()
    {
      using namespace boost;
      namespace algo = boost::algorithm;

      if (m_chunk.data().size() < 128)
      {
        LOG(trace) << "Not enough data for mime header: " << m_chunk.data().size();
        return false;
      }

      auto start = static_cast<const char *>(m_chunk.data().data());
      boost::string_view view(start, m_chunk.data().size());

      auto bp = view.find(m_boundary.c_str());
      if (bp == boost::string_view::npos)
      {
        LOG(warning) << "Cannot find the boundary";
        derived().lowestLayer().close();
        failed(source::make_error_code(source::ErrorCode::RESTART_STREAM),
               "Framing error in streaming data: no content length");
        return false;
      }

      auto ep = view.find("\r\n\r\n", bp);
      if (bp == boost::string_view::npos)
      {
        LOG(warning) << "Cannot find the header separator";
        derived().lowestLayer().close();
        failed(source::make_error_code(source::ErrorCode::RESTART_STREAM),
               "Framing error in streaming data: no content length");
        return false;
      }
      ep += 4;

      using string_view_range = boost::iterator_range<boost::string_view::iterator>;
      auto svi = string_view_range(view.begin() + bp, view.end());
      auto lp = boost::ifind_first(svi, boost::string_view("content-length:"));

      if (lp.empty())
      {
        LOG(warning) << "Cannot find the content-length";
        derived().lowestLayer().close();
        failed(source::make_error_code(source::ErrorCode::RESTART_STREAM),
               "Framing error in streaming data: no content length");
        return false;
      }

      boost::string_view length(lp.end());
      auto digits = length.substr(0, length.find("\n"));
      auto finder = boost::token_finder(algo::is_digit(), algo::token_compress_on);
      auto rng = finder(digits.begin(), digits.end());
      if (rng.empty())
      {
        LOG(warning) << "Cannot find the length in chunk";
        derived().lowestLayer().close();
        failed(source::make_error_code(source::ErrorCode::RESTART_STREAM),
               "Framing error in streaming data: no content length");
        return false;
      }

      m_chunkLength = boost::lexical_cast<size_t>(rng);
      m_hasHeader = true;
      m_chunk.consume(ep);

      return true;
    }

    /// @brief Creates the function to handle chunk body
    ///
    /// Sets the chunk parse on chunk body.
    void createChunkBodyHandler()
    {
      m_chunkHandler = [this](std::uint64_t remain, boost::string_view body,
                              boost::system::error_code &ev) -> unsigned long {
        if (!m_request)
        {
          derived().lowestLayer().close();
          failed(source::make_error_code(source::ErrorCode::RETRY_REQUEST),
                 "Stream body but no request");
          return body.size();
        }

        {
          std::ostream cstr(&m_chunk);
          cstr << body;
        }

        LOG(trace) << "Received: -------- " << m_chunk.size() << " " << remain << "\n"
                   << body << "\n-------------";

        if (!m_hasHeader)
        {
          if (!parseMimeHeader())
          {
            LOG(trace) << "Insufficient data to parse chunk header, wait for more data";
            return body.size();
          }
        }

        auto len = m_chunk.size();
        if (len >= m_chunkLength)
        {
          auto start = static_cast<const char *>(m_chunk.data().data());
          string_view sbuf(start, m_chunkLength);

          LOG(trace) << "Received Chunk: --------\n" << sbuf << "\n-------------";

          processData(string(sbuf));

          m_chunk.consume(m_chunkLength);
          m_hasHeader = false;
        }

        return body.size();
      };

      m_chunkParser->on_chunk_body(m_chunkHandler);
    }

    /// @brief Begins the async chunk reading if the boundry is found
    void onChunkedContent()
    {
      m_boundary = findBoundary();
      if (m_boundary.empty())
      {
        beast::error_code ec;
        failed(ec, "Cannot find boundary");
        return;
      }

      LOG(trace) << "Found boundary: " << m_boundary;

      m_chunkParser.emplace(std::move(*m_headerParser));
      createChunkHeaderHandler();
      createChunkBodyHandler();

      derived().lowestLayer().expires_after(m_timeout);

      http::async_read(derived().stream(), m_buffer, *m_chunkParser,
                       asio::bind_executor(m_strand, beast::bind_front_handler(
                                                         &Derived::onRead, derived().getptr())));
    }

  protected:
    tcp::resolver m_resolver;
    std::optional<tcp::resolver::results_type> m_resolution;
    beast::flat_buffer m_buffer;  // (Must persist between reads)
    std::optional<http::request<http::empty_body>> m_req;

    // Chunked content handling.
    std::optional<http::response_parser<http::empty_body>> m_headerParser;
    std::optional<http::response_parser<http::dynamic_body>> m_chunkParser;
    std::optional<http::response_parser<http::string_body>> m_textParser;
    asio::io_context::strand m_strand;
    url::Url m_url;

    std::function<std::uint64_t(std::uint64_t, boost::string_view, boost::system::error_code &)>
        m_chunkHandler;
    std::function<void(std::uint64_t, boost::string_view, boost::system::error_code &)>
        m_chunkHeaderHandler;

    std::string m_boundary;
    std::string m_contentType;

    size_t m_chunkLength;
    bool m_hasHeader = false;
    boost::asio::streambuf m_chunk;

    // For request queuing
    std::optional<Request> m_request;
    RequestQueue m_queue;
  };

}  // namespace mtconnect::source::adapter::agent_adapter
