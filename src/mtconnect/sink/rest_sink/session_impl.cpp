//
// Copyright Copyright 2009-2022, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "session_impl.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/chunk_encode.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/version.hpp>
#include <boost/bind/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "mtconnect/logging.hpp"
#include "request.hpp"
#include "response.hpp"
#include "tls_dector.hpp"

namespace mtconnect::sink::rest_sink {
  namespace beast = boost::beast;  // from <boost/beast.hpp>
  namespace http = beast::http;    // from <boost/beast/http.hpp>
  namespace asio = boost::asio;
  namespace ip = boost::asio::ip;
  using tcp = boost::asio::ip::tcp;
  namespace algo = boost::algorithm;
  namespace sys = boost::system;
  namespace ssl = boost::asio::ssl;

  using namespace std;
  using std::placeholders::_1;
  using std::placeholders::_2;

  inline unsigned char hex(unsigned char x) { return x + (x > 9 ? ('A' - 10) : '0'); }

  const string urlencode(const string &s)
  {
    ostringstream os;
    for (const auto ci : s)
    {
      if (std::isalnum(ci))
      {
        os << ci;
      }
      else if (ci == ' ')
      {
        os << '+';
      }
      else
      {
        os << '%' << hex(ci >> 4) << hex(ci & 0x0F);
      }
    }

    return os.str();
  }

  inline unsigned char unhex(unsigned char ch)
  {
    if (ch <= '9' && ch >= '0')
      ch -= '0';
    else if (ch <= 'f' && ch >= 'a')
      ch -= 'a' - 10;
    else if (ch <= 'F' && ch >= 'A')
      ch -= 'A' - 10;
    else
      ch = 0;
    return ch;
  }

  const string urldecode(const string_view str)
  {
    ostringstream result;
    for (auto ch = str.cbegin(); ch != str.end(); ch++)
    {
      if (*ch == '+')
      {
        result << ' ';
      }
      else if (*ch == '%')
      {
        if (++ch == str.end())
          break;
        auto cb = unhex(*ch);
        if (++ch == str.end())
          break;
        result << char(cb << 4 | unhex(*ch));
      }
      else
      {
        result << *ch;
      }
    }
    return result.str();
  }

  void parseQueries(string qp, map<string, string> &queries)
  {
    vector<boost::iterator_range<string::iterator>> toks;
    algo::split(toks, qp, boost::is_any_of("&"));
    for (auto tok : toks)
    {
      auto qv = string_view(&*tok.begin(), tok.size());
      auto eq = qv.find('=');
      if (eq != string_view::npos)
      {
        string f(urldecode(qv.substr(0, eq)));
        string s(urldecode(qv.substr(eq + 1)));
        auto pair = std::make_pair(f, s);
        queries.insert(pair);
      }
    }
  }

  string parseUrl(string url, map<string, string> &queries)
  {
    auto pos = url.find('?');
    if (pos != string::npos)
    {
      auto path = urldecode(url.substr(0, pos));
      auto qp = url.substr(pos + 1);
      parseQueries(qp, queries);
      return path;
    }
    else
    {
      return urldecode(url);
    }
  }

  template <class Derived>
  void SessionImpl<Derived>::reset()
  {
    m_response.reset();
    m_request.reset();
    m_serializer.reset();
    m_boundary.clear();
    m_mimeType.clear();

    m_parser.emplace();
  }

  template <class Derived>
  void SessionImpl<Derived>::run()
  {
    NAMED_SCOPE("SessionImpl::run");
    asio::dispatch(derived().stream().get_executor(),
                   beast::bind_front_handler(&SessionImpl::read, shared_ptr()));
  }

  template <class Derived>
  void SessionImpl<Derived>::read()
  {
    NAMED_SCOPE("SessionImpl::read");
    reset();
    m_parser->body_limit(100000);
    beast::get_lowest_layer(derived().stream()).expires_after(30s);
    http::async_read(derived().stream(), m_buffer, *m_parser,
                     beast::bind_front_handler(&SessionImpl::requested, shared_ptr()));
  }

  template <class Derived>
  void SessionImpl<Derived>::requested(boost::system::error_code ec, size_t len)
  {
    NAMED_SCOPE("SessionImpl::requested");

    if (ec)
    {
      fail(status::internal_server_error, "Could not read request", ec);
      return;
    }

    if (m_unauthorized)
    {
      fail(status::unauthorized, m_message, ec);
      return;
    }

    auto &msg = m_parser->get();
    const auto &remote = beast::get_lowest_layer(derived().stream()).socket().remote_endpoint();

    // Check for put, post, or delete
    if (msg.method() != http::verb::get)
    {
      if (!m_allowPuts)
      {
        fail(http::status::bad_request,
             "PUT, POST, and DELETE are not allowed. MTConnect Agent is read only and only GET "
             "is allowed.");
        return;
      }
      else if (!m_allowPutsFrom.empty() &&
               m_allowPutsFrom.find(remote.address()) == m_allowPutsFrom.end())
      {
        fail(http::status::bad_request,
             "PUT, POST, and DELETE are not allowed from " + remote.address().to_string());
        return;
      }
    }

    m_request = make_shared<Request>();
    m_request->m_verb = msg.method();
    m_request->m_path = parseUrl(string(msg.target()), m_request->m_query);

    if (auto a = msg.find(http::field::accept); a != msg.end())
      m_request->m_accepts = string(a->value());
    if (auto a = msg.find(http::field::content_type); a != msg.end())
      m_request->m_contentType = string(a->value());
    if (auto a = msg.find(http::field::accept_encoding); a != msg.end())
      m_request->m_acceptsEncoding = string(a->value());
    m_request->m_body = msg.body();

    if (auto f = msg.find(http::field::content_type);
        f != msg.end() && f->value() == "application/x-www-form-urlencoded" &&
        m_request->m_body[0] != '<')
    {
      parseQueries(m_request->m_body, m_request->m_query);
    }

    m_request->m_foreignIp = remote.address().to_string();
    m_request->m_foreignPort = remote.port();
    if (auto a = msg.find(http::field::connection); a != msg.end())
      m_close = a->value() == "close";

    LOG(info) << "ReST Request: From [" << m_request->m_foreignIp << ':' << remote.port()
              << "]: " << msg.method() << " " << msg.target();

    if (!m_dispatch(shared_ptr(), m_request))
    {
      ostringstream txt;
      txt << "Failed to find handler for " << msg.method() << " " << msg.target();
      LOG(error) << txt.str();
    }
  }

  template <class Derived>
  void SessionImpl<Derived>::sent(boost::system::error_code ec, size_t len)
  {
    NAMED_SCOPE("SessionImpl::sent");

    if (m_outgoing)
    {
      m_outgoing.reset();
    }

    if (ec)
    {
      fail(status::internal_server_error, "Error sending message - ", ec);
    }
    else if (m_complete)
    {
      m_complete();
    }
    if (!m_streaming)
    {
      if (!m_close)
        read();
      else
        close();
    }
  }

  template <class Derived>
  void SessionImpl<Derived>::beginStreaming(const std::string &mimeType, Complete complete)
  {
    NAMED_SCOPE("SessionImpl::beginStreaming");

    beast::get_lowest_layer(derived().stream()).expires_after(30s);

    using namespace http;
    using namespace boost::uuids;
    random_generator gen;
    m_boundary = to_string(gen());
    m_complete = complete;
    m_mimeType = mimeType;
    m_streaming = true;

    auto res = make_shared<http::response<empty_body>>(status::ok, 11);
    m_response = res;
    res->chunked(true);
    res->set(field::server, "MTConnectAgent");
    res->set(field::connection, "close");
    res->set(field::content_type, "multipart/mixed;boundary=" + m_boundary);
    res->set(field::expires, "-1");
    res->set(field::cache_control, "no-cache, no-store, max-age=0");
    for (const auto &f : m_fields)
    {
      res->set(f.first, f.second);
    }

    auto sr = make_shared<response_serializer<empty_body>>(*res);
    m_serializer = sr;
    async_write_header(derived().stream(), *sr,
                       beast::bind_front_handler(&SessionImpl::sent, shared_ptr()));
  }

  template <class Derived>
  void SessionImpl<Derived>::writeChunk(const std::string &body, Complete complete)
  {
    NAMED_SCOPE("SessionImpl::writeChunk");

    using namespace http;

    beast::get_lowest_layer(derived().stream()).expires_after(30s);

    m_complete = complete;
    m_streamBuffer.emplace();
    ostream str(&m_streamBuffer.value());

    str << "--" + m_boundary << "\r\n"
        << to_string(field::content_type) << ": " << m_mimeType << "\r\n"
        << to_string(field::content_length) << ": " << to_string(body.length()) << "\r\n\r\n"
        << body << "\r\n";

    async_write(derived().stream(), http::make_chunk(m_streamBuffer->data()),
                beast::bind_front_handler(&SessionImpl::sent, shared_ptr()));
  }

  template <class Derived>
  void SessionImpl<Derived>::closeStream()
  {
    NAMED_SCOPE("SessionImpl::closeStream");

    m_complete = [this]() { close(); };
    http::fields trailer;
    async_write(derived().stream(), http::make_chunk_last(trailer),
                beast::bind_front_handler(&SessionImpl::sent, shared_ptr()));
  }

  template <class Derived>
  template <typename Message>
  void SessionImpl<Derived>::addHeaders(const Response &response, Message &res)
  {
    res->set(http::field::server, "MTConnectAgent");
    if (response.m_close || m_close)
      res->set(http::field::connection, "close");
    if (response.m_expires == 0s)
    {
      res->set(http::field::expires, "-1");
      res->set(http::field::cache_control, "no-store, max-age=0");
    }
    res->set(http::field::content_type, response.m_mimeType);
    for (const auto &f : m_fields)
    {
      res->set(f.first, f.second);
    }
    if (response.m_location)
    {
      res->set(http::field::location, *response.m_location);
    }
  }

  template <class Derived>
  void SessionImpl<Derived>::writeResponse(ResponsePtr &&responsePtr, Complete complete)
  {
    NAMED_SCOPE("SessionImpl::writeResponse");

    using namespace std;
    using namespace http;
    namespace fs = std::filesystem;
    using std::move;

    m_complete = complete;
    m_outgoing = std::move(responsePtr);

    if (m_outgoing->m_file && !m_outgoing->m_file->m_cached)
    {
      beast::error_code ec;
      http::file_body::value_type body;
      fs::path path;
      optional<string> encoding;
      if (m_request->m_acceptsEncoding.find("gzip") != string::npos && m_outgoing->m_file->m_pathGz)
      {
        encoding.emplace("gzip");
        path = *m_outgoing->m_file->m_pathGz;
      }
      else
      {
        path = m_outgoing->m_file->m_path;
      }

      body.open(path.string().c_str(), beast::file_mode::scan, ec);

      // Handle the case where the file doesn't exist
      if (ec == beast::errc::no_such_file_or_directory)
        return fail(boost::beast::http::status::not_found, "File Not Found", ec);

      auto size = body.size();
      auto res = make_shared<http::response<http::file_body>>(
          std::piecewise_construct, std::make_tuple(std::move(body)),
          std::make_tuple(m_outgoing->m_status, 11));
      res->set(http::field::content_type, m_outgoing->m_mimeType);
      res->content_length(size);
      if (encoding)
        res->set(http::field::content_encoding, "gzip");
      addHeaders(*m_outgoing, res);

      m_response = res;

      async_write(derived().stream(), *res,
                  beast::bind_front_handler(&SessionImpl::sent, shared_ptr()));
    }
    else
    {
      const char *bp;
      size_t size;
      if (m_outgoing->m_file)
      {
        bp = m_outgoing->m_file->m_buffer;
        size = m_outgoing->m_file->m_size;
      }
      else
      {
        bp = m_outgoing->m_body.c_str();
        size = m_outgoing->m_body.size();
      }

      auto res = make_shared<http::response<http::span_body<const char>>>(
          std::piecewise_construct, std::make_tuple(bp, size),
          std::make_tuple(m_outgoing->m_status, 11));

      addHeaders(*m_outgoing, res);
      res->chunked(false);
      res->content_length(size);

      m_response = res;

      async_write(derived().stream(), *res,
                  beast::bind_front_handler(&SessionImpl::sent, shared_ptr()));
    }
  }

  template <class Derived>
  void SessionImpl<Derived>::writeFailureResponse(ResponsePtr &&response, Complete complete)
  {
    if (m_streaming)
    {
      m_outgoing = std::move(response);
      writeChunk(m_outgoing->m_body, [this] { closeStream(); });
    }
    else
    {
      writeResponse(std::move(response));
    }
  }

  /// @brief A secure https session
  class HttpsSession : public SessionImpl<HttpsSession>
  {
  public:
    /// @brief Create a secure https session
    /// @param socket tcp socket (takes ownership)
    /// @param context the asio ssl context
    /// @param buffer the buffer (takes ownership)
    /// @param list the header fieldlist
    /// @param dispatch dispatch function
    /// @param error error function
    HttpsSession(boost::beast::tcp_stream &&socket, boost::asio::ssl::context &context,
                 boost::beast::flat_buffer &&buffer, const FieldList &list, Dispatch dispatch,
                 ErrorFunction error)
      : SessionImpl<HttpsSession>(std::move(buffer), list, dispatch, error),
        m_stream(std::move(socket), context)
    {
      m_remote = beast::get_lowest_layer(m_stream).socket().remote_endpoint();
    }
    /// @brief get a shared pointer to the https session
    /// @return shared pointer
    std::shared_ptr<HttpsSession> shared_ptr()
    {
      return std::dynamic_pointer_cast<HttpsSession>(shared_from_this());
    }
    /// @brief close this session
    virtual ~HttpsSession() { close(); }
    auto &stream() { return m_stream; }

    void run() override
    {
      beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));

      // Perform the SSL handshake
      // Note, this is the buffered version of the handshake.
      m_stream.async_handshake(ssl::stream_base::server, m_buffer.data(),
                               beast::bind_front_handler(&HttpsSession::handshake, shared_ptr()));
    }

    /// @brief return the stream and hand over ownership
    /// @return the stream
    beast::ssl_stream<beast::tcp_stream> releaseStream() { return std::move(m_stream); }

    /// @brief shutdown the stream asyncronously closing the secure stream
    void close() override
    {
      if (!m_closing)
      {
        m_closing = true;
        // Set the timeout.
        beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));

        // Perform the SSL shutdown
        m_stream.async_shutdown(beast::bind_front_handler(&HttpsSession::shutdown, shared_ptr()));
      }
    }

  protected:
    void handshake(beast::error_code ec, size_t bytes_used)
    {
      if (ec)
        return fail(boost::beast::http::status::internal_server_error, "handshake", ec);

      // Consume the portion of the buffer used by the handshake
      m_buffer.consume(bytes_used);

      SessionImpl<HttpsSession>::run();
    }

    void shutdown(beast::error_code ec)
    {
      if (ec)
        return fail(boost::beast::http::status::internal_server_error, "shutdown", ec);
    }

  protected:
    beast::ssl_stream<beast::tcp_stream> m_stream;
    bool m_closing {false};
  };

  void TlsDector::run()
  {
    boost::asio::dispatch(
        m_stream.get_executor(),
        boost::beast::bind_front_handler(&TlsDector::detect, this->shared_from_this()));
  }

  void TlsDector::detect()
  {
    m_stream.expires_after(std::chrono::seconds(30));
    boost::beast::async_detect_ssl(
        m_stream, m_buffer,
        boost::beast::bind_front_handler(&TlsDector::detected, this->shared_from_this()));
  }

  void TlsDector::detected(boost::beast::error_code ec, bool isTls)
  {
    NAMED_SCOPE("TlsDector::detected");

    if (ec)
    {
      fail(ec, "Failed to detect TLS Connection");
    }
    else
    {
      shared_ptr<Session> session;
      if (isTls)
      {
        LOG(debug) << "Received HTTPS request";
        // Create https session
        session =
            std::make_shared<HttpsSession>(std::move(m_stream), m_tlsContext, std::move(m_buffer),
                                           m_fields, m_dispatch, m_errorFunction);
      }
      else
      {
        LOG(debug) << "Received HTTP request";
        // Create http session
        session = std::make_shared<HttpSession>(std::move(m_stream), std::move(m_buffer), m_fields,
                                                m_dispatch, m_errorFunction);

        // Start the session, but set
        if (m_tlsOnly)
        {
          LOG(debug) << "Rejecting HTTP request. Only allow TLS";
          session->setUnauthorized("Only TLS (https) connections allowed");
          session->run();
          return;
        }
      }

      if (!m_allowPutsFrom.empty())
        session->allowPutsFrom(m_allowPutsFrom);
      else if (m_allowPuts)
        session->allowPuts();

      session->run();
    }
  }
}  // namespace mtconnect::sink::rest_sink
