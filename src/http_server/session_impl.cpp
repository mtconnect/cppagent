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

#include "session_impl.hpp"

#include <boost/bind/bind.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/chunk_encode.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>

#include "logging.hpp"
#include "response.hpp"
#include "request.hpp"

namespace mtconnect
{
  namespace http_server
  {
    namespace beast = boost::beast;         // from <boost/beast.hpp>
    namespace http = beast::http;           // from <boost/beast/http.hpp>
    namespace net = boost::asio;            // from <boost/asio.hpp>
    namespace asio = boost::asio;
    namespace ip = boost::asio::ip;
    using tcp = boost::asio::ip::tcp;
    namespace algo = boost::algorithm;
    namespace sys = boost::system;
    
    using namespace std;
    using boost::placeholders::_1;
    using boost::placeholders::_2;

    inline unsigned char hex( unsigned char x )
    {
        return x + (x > 9 ? ('A'-10) : '0');
    }

    const string urlencode(const string& s )
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
        auto qv = string_view(tok.begin().base(), tok.size());
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
    
    void SessionImpl::fail(boost::beast::http::status status,
                           const std::string &message,
                           sys::error_code ec)
    {
      NAMED_SCOPE("SessionImpl::fail");

      LOG(warning) << "Operation failed: " << message;
      if (ec)
      {
        LOG(warning) << "Closing: " << ec.category().message(ec.value()) << " - "
            << ec.message();
        close();
      }
      else
      {
        m_errorFunction(shared_this_ptr(), status, message);
      }
    }
    
    void SessionImpl::reset()
    {
      m_response.reset();
      m_request.reset();
      m_serializer.reset();
      m_buffer.clear();
      m_boundary.clear();
      m_mimeType.clear();
      
      m_parser.emplace();
    }


    void SessionImpl::run()
    {
      NAMED_SCOPE("SessionImpl::run");
      net::dispatch(m_stream.get_executor(),
                    beast::bind_front_handler(&SessionImpl::read,
                                              shared_this_ptr()));
    }
      
    void SessionImpl::read()
    {
      NAMED_SCOPE("SessionImpl::read");
      reset();
      m_parser->body_limit(100000);
      m_stream.expires_after(30s);
      http::async_read(m_stream,
                       m_buffer,
                       *m_parser,
                       beast::bind_front_handler(&SessionImpl::requested,
                                                 shared_this_ptr()));
      
    }
      
    void SessionImpl::requested(boost::system::error_code ec, size_t len)
    {
      NAMED_SCOPE("SessionImpl::requested");
      if (ec)
      {
        fail(status::internal_server_error, "Could not read request", ec);
      }
      else
      {
        auto msg = m_parser->get();
        auto remote = m_stream.socket().remote_endpoint();

        // Check for put, post, or delete
        if (msg.method() != http::verb::get)
        {
          if (!m_allowPuts)
          {
            fail(http::status::bad_request, "PUT, POST, and DELETE are not allowed. MTConnect Agent is read only and only GET is allowed.");
            return;
          }
          else if (!m_allowPutsFrom.empty() &&
                   m_allowPutsFrom.find(remote.address()) == m_allowPutsFrom.end())
          {
            fail(http::status::bad_request, "PUT, POST, and DELETE are not allowed from "
                 + remote.address().to_string());
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
        m_request->m_body = msg.body();
        
        if (auto f = msg.find(http::field::content_type); f != msg.end() && f->value() == "application/x-www-form-urlencoded" && m_request->m_body[0] != '<')
        {
          parseQueries(m_request->m_body, m_request->m_query);
        }
        
        m_request->m_foreignIp = remote.address().to_string();
        m_request->m_foreignPort = remote.port();
        if (auto a = msg.find(http::field::connection); a != msg.end())
          m_close = a->value() == "close";
        
        m_request->m_session = shared_this_ptr();
        
        try
        {
          if (!m_dispatch(m_request))
          {
            ostringstream txt;
            txt << "Failed to find handler for " << msg.method() << " " <<
                  msg.target();
            LOG(error) << txt.str();
            fail(status::not_found, txt.str());
          }
        }
        catch (RequestError &e)
        {
          LOG(error) << "Error processing request from: " << remote.address() << " - "
              << e.what();
          fail(status::bad_request, e.what());
        }
        catch (ParameterError &e)
        {
          stringstream txt;
          txt << "Parameter Error processing request from: " << remote.address() << " - "
            << e.what();
          LOG(error) << txt.str();
          fail(status::not_found, txt.str());
        }
      }
    }

    void SessionImpl::sent(boost::system::error_code ec, size_t len)
    {
      NAMED_SCOPE("SessionImpl::sent");

      if (ec)
      {
        fail(status::internal_server_error, "Error sending message - ", ec);
      }
      else if (m_complete)
      {
        if (m_complete)
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
      
    void SessionImpl::close()
    {
      m_request.reset();
      beast::error_code ec;
      m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    }
        
    void SessionImpl::beginStreaming(const std::string &mimeType, Complete complete)
    {
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
      res->set(field::content_type, "multipart/x-mixed-replace;boundary=" + m_boundary);
      res->set(field::expires, "-1");
      res->set(field::cache_control, "private, max-age=0");
//      for (const auto &f : m_fields)
//      {
//        //res.set(f.first, f.second);
//      }
      
      auto sr = make_shared<response_serializer<empty_body>>(*res);
      m_serializer = sr;
      async_write_header(m_stream, *sr,
                         beast::bind_front_handler(&SessionImpl::sent,
                                                   shared_this_ptr()));
                         
    }
    
    void SessionImpl::writeChunk(const std::string &body, Complete complete)
    {
      using namespace std;
      m_complete = complete;

      http::chunk_extensions ext;
      
      ostringstream str;
      str << "--" + m_boundary << "\r\n"
             "Content-type: " <<  m_mimeType << "\r\n"
             "Content-length: " << to_string(body.length()) << ";\r\n\r\n" <<
             body;
      
      net::const_buffers_1 buf(str.str().c_str(), str.str().size());
      async_write(m_stream, http::make_chunk(buf),
                  beast::bind_front_handler(&SessionImpl::sent,
                                            shared_this_ptr()));
    }
    
    void SessionImpl::closeStream()
    {
      m_complete = [this]() { close(); };
      http::fields trailer;
      async_write(m_stream, http::make_chunk_last(trailer),
                  beast::bind_front_handler(&SessionImpl::sent,
                                            shared_this_ptr()));
    }

    void SessionImpl::writeResponse(const Response &response, Complete complete)
    {
      using namespace std;
      using namespace http;

      m_complete = complete;
      auto res = make_shared<http::response<http::string_body>>(response.m_status, 11);
      res->body() = response.m_body;
      res->set(http::field::server, "MTConnectAgent");
      if (response.m_close || m_close)
        res->set(http::field::connection, "close");
      if (response.m_expires == 0s)
      {
        res->set(http::field::expires, "-1");
        res->set(http::field::cache_control, "private, max-age=0");
      }
      res->set(http::field::content_type, response.m_mimeType);
//      for (const auto &f : m_fields)
//      {
//        //res.set(k, v);
//      }

      res->content_length(response.m_body.size());
      res->prepare_payload();
      
      m_response = res;

      async_write(m_stream, *res,
                  beast::bind_front_handler(&SessionImpl::sent,
                                            shared_this_ptr()));
    }
  }
}
