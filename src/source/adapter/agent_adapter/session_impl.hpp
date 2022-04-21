
//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "pipeline/response_document.hpp"
#include "pipeline/mtconnect_xml_transform.hpp"
#include "session.hpp"

namespace mtconnect::source::adapter::agent_adapter {
  using namespace std;
  namespace asio = boost::asio;
  namespace beast = boost::beast;
  namespace http = boost::beast::http;
  namespace ssl = boost::asio::ssl;
  using tcp = boost::asio::ip::tcp;

  // Report a failure
  template <class Derived>
  class SessionImpl : public Session
  {
    struct Request
    {
      Request(const std::string &suffix, const UrlQuery &query, bool stream, Next next)
        : m_suffix(suffix), m_query(query), m_stream(stream), m_next(next)
      {}

      std::string m_suffix;
      UrlQuery m_query;
      bool m_stream;
      Next m_next;
    };

    using RequestQueue = std::list<Request>;

  public:
    Derived &derived() { return static_cast<Derived &>(*this); }
    const Derived &derived() const { return static_cast<const Derived &>(*this); }

    // Objects are constructed with a strand to
    // ensure that handlers do not execute concurrently.
    SessionImpl(boost::asio::io_context::strand &strand, const Url &url, int count, int heartbeat)
      : m_resolver(strand.context()),
        m_strand(strand),
        m_url(url),
        m_chunk(1 * 1024 * 1024),
        m_count(count),
        m_heartbeat(heartbeat)
    {}

    virtual ~SessionImpl() { stop(); }

    bool isOpen() const override { return derived().lowestLayer().socket().is_open(); }

    void failed(boost::beast::error_code ec, const char *what, bool reconnect = true) override
    {
      derived().lowestLayer().socket().close();
      m_idle = true;

      LOG(error) << what << ": " << ec.message() << "\n";
      if (!reconnect)
      {
        LOG(error) << "Cannot reconnect";
        if (m_failed)
          m_failed();
      }
      else if (m_reconnect)
      {
        LOG(info) << "Reconnecting";
        m_reconnect();
      }
    }

    void stop() override
    {
      m_idle = true;
      if (isOpen())
        derived().lowestLayer().close();
    }

    // Start the asynchronous operation
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
        asio::ip::tcp::endpoint ep(get<asio::ip::address>(m_url.m_host), m_url.m_port.value_or(80));

        // Create the results type and call on resolve directly.
        using results_type = tcp::resolver::results_type;
        auto results = results_type::create(ep, m_url.getHost(), m_url.getService());

        beast::error_code ec;
        onResolve(ec, results);
      }
      else
      {
        derived().lowestLayer().expires_after(std::chrono::seconds(30));

        // Do an async resolution of the address.
        m_resolver.async_resolve(
            get<string>(m_url.m_host), m_url.getService(),
            asio::bind_executor(
                m_strand, beast::bind_front_handler(&SessionImpl::onResolve, derived().getptr())));
      }
    }

    void onResolve(beast::error_code ec, tcp::resolver::results_type results)
    {
      if (ec)
      {
        // If we can't resolve, then shut down the adapter
        return failed(ec, "resolve", false);
      }

      if (!m_resolution)
        m_resolution.emplace(results);

      if (m_handler && m_handler->m_connecting)
        m_handler->m_connecting(m_identity);

      // Set a timeout on the operation
      derived().lowestLayer().expires_after(std::chrono::seconds(30));

      // Make the connection on the IP address we get from a lookup
      derived().lowestLayer().async_connect(
          results, asio::bind_executor(m_strand, beast::bind_front_handler(&Derived::onConnect,
                                                                           derived().getptr())));
    }

    bool makeRequest(const std::string &suffix, const UrlQuery &query, bool stream,
                     Next next) override
    {
      if (m_idle)
      {
        m_idle = false;

        m_next = next;
        if (suffix[0] == '/')
          m_target = suffix;
        else
          m_target = m_url.m_path + suffix;
        auto uq = m_url.m_query;
        if (!query.empty())
          uq.merge(query);
        if (!uq.empty())
          m_target += ("?" + uq.join());
        m_streaming = stream;

        // Clean out any previous data
        m_buffer.clear();
        m_contentType.clear();
        m_boundary.clear();
        m_headerParser.reset();
        m_chunkParser.reset();
        m_textParser.reset();
        m_req.reset();
        m_hasHeader = false;
        m_closeOnRead = false;
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
          // If still connected, go to the next request
          request();
          return true;
        }
      }
      else
      {
        m_queue.emplace_back(suffix, query, stream, next);
        return false;
      }
    }
    
    void processData(const std::string &data)
    {
      try
      {
        if (m_handler && m_handler->m_processData)
          m_handler->m_processData(data, m_identity);
      }
      catch(pipeline::InstanceIdChanged &e)
      {
        LOG(warning) << "Instance id changed from " << e.m_oldInstanceId << " to " <<
                        e.m_instanceId;
        if (m_resync)
          m_resync();
      }
      catch (std::exception &e)
      {
        beast::error_code ec;
        LOG(error) << "AgentAdapter::processData: " << e.what();
        failed(ec, "Exception occurred in AgentAdapter::processData", false);
      }
      catch (...)
      {
        beast::error_code ec;
        failed(ec, "Unknown exception in AgentAdapter::processData", false);
      }
    }

    void request()
    {
      // Set up an HTTP GET request message
      m_req.emplace();
      m_req->version(11);
      m_req->method(http::verb::get);
      m_req->target(m_target);
      m_req->set(http::field::host, m_url.getHost());
      m_req->set(http::field::user_agent, "MTConnect Agent/2.0");
      if (m_closeConnectionAfterResponse)
      {
        m_req->set(http::field::connection, "close");
      }

      http::async_write(derived().stream(), *m_req,
                        beast::bind_front_handler(&SessionImpl::onWrite, derived().getptr()));
    }

    void onWrite(beast::error_code ec, std::size_t bytes_transferred)
    {
      boost::ignore_unused(bytes_transferred);

      if (ec)
      {
        return failed(ec, "write");
      }

      derived().lowestLayer().expires_after(std::chrono::seconds(30));

      // Receive the HTTP response
      m_headerParser.emplace();
      http::async_read_header(
          derived().stream(), m_buffer, *m_headerParser,
          asio::bind_executor(m_strand,
                              beast::bind_front_handler(&Derived::onHeader, derived().getptr())));
    }

    void onHeader(beast::error_code ec, std::size_t bytes_transferred)
    {
      if (ec)
      {
        if (ec == asio::error::connection_reset)
        {
          LOG(debug) << "Connection reset, reconnecting";
          derived().lowestLayer().close();
          connect();
          return;
        }
        else
        {
          LOG(error) << "Need to handle fallback if sreaming fails";
          failed(ec, "header");
          return;
        }
      }

      auto &msg = m_headerParser->get();
      if (auto a = msg.find(beast::http::field::connection); a != msg.end())
        m_closeOnRead = a->value() == "close";

      if (m_streaming && m_headerParser->chunked())
      {
        onChunkedContent();
      }
      else if (m_streaming)
      {
        LOG(error) << "Need to handle fallback if sreaming fails";
      }
      else
      {
        derived().lowestLayer().expires_after(std::chrono::seconds(30));

        m_textParser.emplace(std::move(*m_headerParser));
        http::async_read(derived().stream(), m_buffer, *m_textParser,
                         asio::bind_executor(m_strand, beast::bind_front_handler(
                                                           &Derived::onRead, derived().getptr())));
      }
    }

    void onRead(beast::error_code ec, std::size_t bytes_transferred)
    {
      boost::ignore_unused(bytes_transferred);

      if (ec)
      {
        return failed(ec, "read");
      }

      derived().lowestLayer().expires_after(std::chrono::seconds(30));

      if (!derived().lowestLayer().socket().is_open())
        derived().disconnect();

      processData(m_textParser->get().body());

      m_textParser.reset();
      m_idle = true;

      m_req.reset();

      if (m_closeOnRead)
        derived().disconnect();

      if (m_next)
      {
        m_next();
      }
      else if (!m_queue.empty())
      {
        Request req = m_queue.front();
        m_queue.pop_front();
        makeRequest(req.m_suffix, req.m_query, req.m_stream, req.m_next);
      }
    }

    // Streaming related methods

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

    void createChunkHeaderHandler()
    {
      m_chunkHeaderHandler = [this](std::uint64_t size, boost::string_view extensions,
                                    boost::system::error_code &ec) {
        http::chunk_extensions ce;
        ce.parse(extensions, ec);
        for (auto &c : ce)
          cout << "Ext: " << c.first << ": " << c.second << endl;

        if (ec)
        {
          return failed(ec, "Failed in chunked extension parse");
        }
      };

      m_chunkParser->on_chunk_header(m_chunkHeaderHandler);
    }

    void parseMimeHeader()
    {
      auto start = static_cast<const char *>(m_chunk.data().data());
      std::string_view view(start, m_chunk.data().size());

      auto bp = view.find(m_boundary.c_str());
      if (bp == boost::string_view::npos)
      {
        LOG(error) << "Cannot find the boundary";
        derived().lowestLayer().close();
        throw runtime_error("cannot find boundary");
      }

      auto ep = view.find("\r\n\r\n", bp);
      if (bp == boost::string_view::npos)
      {
        LOG(error) << "Cannot find the header separator";
        derived().lowestLayer().close();
        throw runtime_error("cannot find header separator");
      }
      ep += 4;

      auto lp = strcasestr(view.data() + bp, "content-length");
      if (lp == NULL)
      {
        LOG(error) << "Cannot find the content-length";
        derived().lowestLayer().close();
        throw runtime_error("cannot find the content-length");
      }

      m_hasHeader = true;
      m_chunkLength = atoi(lp + 16);
      m_chunk.consume(ep);
    }
    
    void createChunkBodyHandler()
    {
      m_chunkHandler = [this](std::uint64_t remain, boost::string_view body,
                              boost::system::error_code &ev) -> unsigned long {
        {
          std::ostream cstr(&m_chunk);
          cstr << body;
        }

        LOG(trace) << "Received: -------- " << m_chunk.size() << " " << remain << "\n"
                   << body << "\n-------------";

        if (!m_hasHeader)
        {
          parseMimeHeader();
        }

        auto len = m_chunk.size();
        if (len >= m_chunkLength)
        {
          auto start = static_cast<const char *>(m_chunk.data().data());
          string_view sbuf(start, m_chunkLength);

          LOG(debug) << "Received Chunk: --------\n" << sbuf << "\n-------------";

          processData(string(sbuf));
          
          m_chunk.consume(m_chunkLength);
          m_hasHeader = false;
        }

        return body.size();
      };

      m_chunkParser->on_chunk_body(m_chunkHandler);
    }

    void onChunkedContent()
    {
      m_boundary = findBoundary();
      if (m_boundary.empty())
      {
        beast::error_code ec;
        failed(ec, "Cannot find boundary");
        return;
      }

      LOG(info) << "Found boundary: " << m_boundary;

      m_chunkParser.emplace(std::move(*m_headerParser));
      createChunkHeaderHandler();
      createChunkBodyHandler();

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
    Url m_url;

    std::function<std::uint64_t(std::uint64_t, boost::string_view, boost::system::error_code &)>
        m_chunkHandler;
    std::function<void(std::uint64_t, boost::string_view, boost::system::error_code &)>
        m_chunkHeaderHandler;

    std::string m_boundary;
    std::string m_contentType;

    size_t m_chunkLength;
    bool m_hasHeader;
    boost::asio::streambuf m_chunk;

    // For request queuing
    RequestQueue m_queue;
    bool m_idle = true;

    std::string m_target;
    Next m_next;
    int m_count;
    int m_heartbeat;
    bool m_closeOnRead = false;

    bool m_streaming = false;
  };

}  // namespace mtconnect::source::adapter::agent_adapter
