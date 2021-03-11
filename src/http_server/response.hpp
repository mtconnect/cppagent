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

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/bind/bind.hpp>
#include <boost/beast/http/chunk_encode.hpp>
//#include <boost/beast/ssl.hpp>
//#include <boost/asio/ssl/stream.hpp>

#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "utilities.hpp"
#include <unordered_map>

#include <dlib/logger.h>
#include <dlib/md5.h>

#include <chrono>
#include <map>
#include <ostream>
#include <iostream>
#include <sstream>
#include <string>
#include <variant>


namespace mtconnect
{
  namespace http_server
  {

    namespace beast = boost::beast;         // from <boost/beast.hpp>
    namespace http = beast::http;           // from <boost/beast/http.hpp>
    namespace net = boost::asio;            // from <boost/asio.hpp>
    //namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
    using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>


    enum ResponseCode
    {
      CONTINUE = 100,
      SWITCHING_PROTOCOLS = 101,
      OK = 200,
      CREATED = 201,
      ACCEPTED = 202,
      NON_AUTHORITATIVE_INFORMATION = 203,
      NO_CONTENT = 204,
      RESET_CONTENT = 205,
      PARTIAL_CONTENT = 206,
      MULTIPLE_CHOICES = 300,
      MOVED_PERMANENTLY = 301,
      FOUND = 302,
      SEE_OTHER = 303,
      NOT_MODIFIED = 304,
      USE_PROXY = 305,
      TEMPORARY_REDIRECT = 307,
      BAD_REQUEST = 400,
      UNAUTHORIZED = 401,
      PAYMENT_REQUIRED = 402,
      FORBIDDEN = 403,
      NOT_FOUND = 404,
      METHOD_NOT_ALLOWED = 405,
      NOT_ACCEPTABLE = 406,
      PROXY_AUTHENTICATION_REQUIRED = 407,
      REQUEST_TIMEOUT = 408,
      CONFLICT = 409,
      GONE = 410,
      LENGTH_REQUIRED = 411,
      PRECONDITION_FAILED = 412,
      PAYLOAD_TOO_LARGE = 413,
      URI_TOO_LONG = 414,
      UNSUPPORTED_MEDIA_TYPE = 415,
      RANGE_NOT_SATISFIABLE = 416,
      EXPECTATION_FAILED = 417,
      UPGRADE_REQUIRED = 426,
      INTERNAL_SERVER_ERROR = 500,
      NOT_IMPLEMENTED = 501,
      BAD_GATEWAY = 502,
      SERVICE_UNAVAILABLE = 503,
      GATEWAY_TIMEOUT = 504,
      HTTP_VERSION_NOT_SUPPORTED = 505
    };

    class Response
    {
    public:
      //Response(tcp::socket &socket, std::ostream &out, const StringList &fields = {}) : m_socket(std::move(socket)) ,m_out(out), m_fields(fields)
      Response(tcp::socket &socket,  const StringList &fields = {}) : m_socket(std::move(socket)), m_fields(fields)
      {
      }

      virtual ~Response() = default;

      virtual std::string getHeaderDate() { return getCurrentTime(HUM_READ); }

      bool good() const { return m_out.good(); }
      void setBad() { m_out.setstate(std::ios::badbit); }
      void flush() { m_out.flush(); }
      static std::string getStatus(uint16_t code)
      {
        auto cm = m_status.find(code);
        if (cm == m_status.end())
          return "Unknown";
        else
          return cm->second;
      }

      virtual void beginMultipartStream()
      {
        if (good())
        {
          m_boundary = dlib::md5(std::to_string(time(nullptr)));

          http::response<http::empty_body> res{http::status::ok, 11};
          res.set(http::field::server, "MTConnectAgent");
          res.set(http::field::date, getHeaderDate());
          res.set(http::field::connection, "close");
          res.set(http::field::expires, "-1");
          res.set(http::field::cache_control, "private, max-age=0\r\n" );
          // Set Transfer-Encoding to "chunked".
          // If a Content-Length was present, it is removed.
          std::string content_type = "multipart/x-mixed-replace;boundary=";
          content_type.append(m_boundary);
          res.set(http::field::content_type ,content_type);
          res.chunked(true);

          // Set up the serializer
          http::response_serializer<http::empty_body> sr{res};
          // Write the header first
          write_header(m_socket, sr);
//
//          std::string content_type = "multipart/x-mixed-replace;boundary=";
//          content_type.append(m_boundary);
//          http::response<http::string_body> res
//          {
//              std::piecewise_construct,
//              std::make_tuple(std::move("")),
//              std::make_tuple(http::status::ok, 11)// m_req.version())
//          };
//          res.set(http::field::server, "MTConnectAgent");
//          res.set(http::field::date, getHeaderDate());
//          res.set(http::field::connection, "close");
//          res.set(http::field::expires, "-1");
//          res.set(http::field::content_type, content_type);
//          http::serializer<false, http::string_body , http::fields> sr{res};
//          http::write_header(m_socket, sr, ec);

//          m_out << "HTTP/1.1 200 OK\r\n"
//                   "Date: "
//                << getHeaderDate()
//                << "\r\n"
//                   "Server: MTConnectAgent\r\n"
//                   "Expires: -1\r\n"
//                   "Connection: close\r\n"
//                   "Cache-Control: private, max-age=0\r\n"
//                   "Content-Type: multipart/x-mixed-replace;boundary="
//                << m_boundary<< "\r\n"
//                  "Transfer-Encoding: chunked\r\n";
//          for (auto &f : m_fields)
//            m_out << f << "\r\n";
//          m_out << "\r\n";
        }
      }

      virtual void writeChunk(const std::string &chunk)
      {
        if (good())
        {
//          std::string content_type{"multipart/x-mixed-replace;boundary="};
          http::response<http::string_body> res
              {
                  std::piecewise_construct,
                  std::make_tuple(std::move(m_out.str().c_str())),
                  std::make_tuple(http::status::ok, 11)// m_req.version())
              };
          res.set(http::field::server, "MTConnectAgent");
          res.set(http::field::date, getHeaderDate());
          res.set(http::field::connection, "close");
          res.set(http::field::expires, "-1");
          res.set(http::field::content_type, "text/xml");
          res.content_length(0);
          http::serializer<false, http::string_body , http::fields> sr{res};
          http::write(m_socket, sr, ec);

          using namespace std;
          m_out.setf(ios::hex, ios::basefield);
          m_out << chunk.length() << "\r\n" << chunk << "\r\n";
          m_out.flush();
        }
      }

      virtual void writeMultipartChunk(const std::string &body, const std::string &mimeType)
      {
        if (good())
        {
          using namespace http;
          using namespace std;
          using chunk_extensions = basic_chunk_extensions< std::allocator< char > >;
          char buffer[2048];
          ostringstream str;
          str << "--" << m_boundary
              << "\r\n"
                 "Content-type: "
              << mimeType
              << "\r\n"
                 "Content-length: "
              << body.length() << "\r\n\r\n"
              << body << "\r\n\r\n";
          str.setf(ios::hex, ios::basefield);
          //str << str.str().length() << "\r\n" << str.str() << "\r\n";
          //string temp{str.str()};
          //m_out << temp;

          http::response<http::string_body> res;//{std::piecewise_construct,std::make_tuple(std::move(str.str()))};
          res.clear();
          res.body() = str.str();
          //res.body().size = str.str().size();
          //res.body().more = false;
//
//        // Write everything in the body buffer
          http::serializer< false, http::string_body , http::fields> sr{res};
          write(m_socket, sr, ec);
          if (ec)
          {
            string errorMsg = "Error writing chunk - ";
            errorMsg.append(ec.message());
            auto const textSize = errorMsg.length();
            http::response<http::string_body> errorRes{
                std::piecewise_construct, std::make_tuple(std::move(errorMsg.c_str())),
                std::make_tuple(http::status::ok, 11)  // m_req.version())
            };
            errorRes.set(http::field::server, "MTConnectAgent");
            errorRes.set(http::field::date, getHeaderDate());
            errorRes.set(http::field::connection, "close");
            errorRes.set(http::field::expires, "-1");
            res.set(http::field::content_type, "text/xml");
            errorRes.content_length(textSize);
            http::serializer<false, http::string_body, http::fields> sr{errorRes};
            http::write(m_socket, sr, ec);
          }


//          ostringstream str;
//          m_out.setf(ios::dec, ios::basefield);
//          response<buffer_body> res;
//          str << "--" << m_boundary
//              << "\r\n"
//                 "Content-type: "
//              << mimeType
//              << "\r\n"
//                 "Content-length: "
//              << body.length() << "\r\n\r\n"
//              << body << "\r\n\r\n";
//          fields trailer;
//          trailer.set(field::content_md5, m_boundary);
//          trailer.set(field::expires, "never");
//          chunk_body cBody(make_chunk(net::const_buffer(&body,body.size())));
//          net::write(m_socket, chunk_header{1024, "d"});
//          string buf{body.data()};
//          net::write(m_socket, make_chunk(net::const_buffer(&buf,buf.length())));
//          net::write(m_socket, make_chunk_last(trailer));


//          http::response<http::string_body> res
//              {
//                  std::piecewise_construct,
//                  std::make_tuple(std::move(str.str().c_str())),
//                  std::make_tuple(http::status::ok, 11)// m_req.version())
//              };
          //res.set(http::field::server, "MTConnectAgent");
          //res.set(http::field::date, getHeaderDate());
          //res.set(http::field::connection, "close");
          //res.set(http::field::expires, "-1");
          //res.set(http::field::content_type, mimeType);
          //res.content_length(0);
//          http::serializer<false, http::buffer_body , http::fields> sr{res_c};
//          http::write(m_socket, sr, ec);

          //writeChunk(str.str());
        }
      }

      net::const_buffer get_next_chunk_body(const std::string& body)
      {
        std::string tesmp = "eoriwoeirow";
         return net::const_buffer(&body,body.size());
      }

      void writeResponse(const std::string &body, const std::string &mimeType = "text/plain",
                         const ResponseCode code = OK,
                         const std::chrono::seconds expires = std::chrono::seconds(0))
      {
        writeResponse(body.c_str(), body.size(), code, mimeType, expires);
      }

      void writeResponse(const std::string &body, const ResponseCode code,
                         const std::string &mimeType = "text/plain",
                         const std::chrono::seconds expires = std::chrono::seconds(0))
      {
        writeResponse(body.c_str(), body.size(), code, mimeType, expires);
      }

      virtual void writeResponse(const char *body, const size_t size, const ResponseCode code,
                                 const std::string &mimeType = "text/plain",
                                 const std::chrono::seconds expires = std::chrono::seconds(0))
      {
        if (good())
        {
          using namespace std;
          string expiry;
          if (expires == 0s)
          {
            expiry =
                "Expires: -1\r\n"
                "Cache-Control: private, max-age=0\r\n";
          }
          else
          {
            expiry = getCurrentTime(chrono::system_clock::now() + expires, HUM_READ);
          }
          m_out.write(body, size);
          auto const textSize = m_out.str().size();
          http::response<http::string_body> res
          {
              std::piecewise_construct,
              std::make_tuple(std::move(m_out.str().c_str())),
              std::make_tuple(http::status::ok, 11)// m_req.version())
          };
          res.set(http::field::server, "MTConnectAgent");
          res.set(http::field::date, getHeaderDate());
          res.set(http::field::connection, "close");
          res.set(http::field::expires, "-1");
          res.set(http::field::content_type, "text/xml");
          res.content_length(textSize);
          http::serializer<false, http::string_body , http::fields> sr{res};
          http::write(m_socket, sr, ec);
        }
      }

    protected:
      std::stringstream m_out;
      tcp::socket m_socket;
      std::string m_boundary;
      std::string content_type;
      http::response<http::buffer_body> res_c{http::status::ok, 11};
      static const std::unordered_map<uint16_t, std::string> m_status;
      static const std::unordered_map<std::string, uint16_t> m_codes;
      StringList m_fields;
      beast::error_code ec;
      bool close{false};
    };
  }  // namespace http_server
}  // namespace mtconnect
