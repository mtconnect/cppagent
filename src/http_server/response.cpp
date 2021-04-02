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

#include "response.hpp"

#include <boost/beast.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/bind/bind.hpp>
#include <boost/beast/http/chunk_encode.hpp>

using namespace std;
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>

namespace mtconnect
{
  namespace http_server
  {
    const std::unordered_map<uint16_t, std::string> Response::m_status = {
        {100, "Continue"},
        {101, "Switching Protocols"},
        {200, "OK"},
        {201, "Created"},
        {202, "Accepted"},
        {203, "Non-Authoritative Information"},
        {204, "No Content"},
        {205, "Reset Content"},
        {206, "Partial Content"},
        {300, "Multiple Choices"},
        {301, "Moved Permanently"},
        {302, "Found"},
        {303, "See Other"},
        {304, "Not Modified"},
        {305, "Use Proxy"},
        {307, "Temporary Redirect"},
        {400, "Bad Request"},
        {401, "Unauthorized"},
        {402, "Payment Required"},
        {403, "Forbidden"},
        {404, "Not Found"},
        {405, "Method Not Allowed"},
        {406, "Not Acceptable"},
        {407, "Proxy Authentication Required"},
        {408, "Request Timeout"},
        {409, "Conflict"},
        {410, "Gone"},
        {411, "Length Required"},
        {412, "Precondition Failed"},
        {413, "Payload Too Large"},
        {414, "URI Too Long"},
        {415, "Unsupported Media Type"},
        {416, "Range Not Satisfiable"},
        {417, "Expectation Failed"},
        {426, "Upgrade Required"},
        {500, "Internal Server Error"},
        {501, "Not Implemented"},
        {502, "Bad Gateway"},
        {503, "Service Unavailable"},
        {504, "Gateway Timeout"},
        {505, "HTTP Version Not Supported"}};

    template <typename F, typename S>
    static std::unordered_map<S, F> flip_map(const std::unordered_map<F, S> &src)
    {
      std::unordered_map<S, F> dst;
      transform(src.begin(), src.end(), std::inserter(dst, dst.begin()),
                [](const auto &p) { return make_pair(p.second, p.first); });
      return dst;
    }

    const std::unordered_map<std::string, uint16_t> Response::m_codes = flip_map(m_status);
    
    void Response::beginMultipartStream()
    {
      using namespace http;
      
      if (good())
      {
        using namespace boost::uuids;
        random_generator gen;
        m_boundary = to_string(gen());

        response<empty_body> res{status::ok, 11};
        res.chunked(true);
        res.set(field::server, "MTConnectAgent");
        res.set(field::date, getHeaderDate());
        res.set(field::connection, "close");
        res.set(field::expires, "-1");
        res.set(field::cache_control, "private, max-age=0\r\n");
        string content_type = "multipart/x-mixed-replace;boundary=" + m_boundary;
        res.set(field::content_type, content_type);
        // TODO: Add additional fields
        
        response_serializer<empty_body> sr{res};
        write_header(m_socket, sr);
      }
    }
    
    void Response::writeMultipartChunk(const std::string &body, const std::string &mimeType)
    {
      if (good())
      {
        using namespace std;

        http::chunk_extensions ext;
        ext.insert("\r\n--"+m_boundary);
        ext.insert("\r\nContent-type: "+ mimeType);
        ext.insert("\r\nContent-length: "+ to_string(body.length())+";\r\n\r\n");

        net::const_buffers_1 buf(body.c_str(), body.size());
        http::chunk_body chunk{http::make_chunk(buf,move(ext))};
        net::write(m_socket, chunk, m_ec);

        if (m_ec)
        {
          m_socket.close();
          
//          string errorMsg = "Error writing chunk - ";
//          errorMsg.append(m_ec.message());
//          auto const textSize = errorMsg.length();
//          http::response<http::string_body> errorRes{
//              std::piecewise_construct, std::make_tuple(std::move(errorMsg.c_str())),
//              std::make_tuple(http::status::ok, 11)  // m_req.version())
//          };
//          errorRes.set(http::field::server, "MTConnectAgent");
//          errorRes.set(http::field::date, getHeaderDate());
//          errorRes.set(http::field::connection, "close");
//          errorRes.set(http::field::expires, "-1");
//          errorRes.set(http::field::content_type, "text/xml");
//          errorRes.content_length(textSize);
//          http::serializer<false, http::string_body, http::fields> sr{errorRes};
//          http::write(m_socket, sr, m_ec);
        }
      }
    }

    void Response::writeResponse(const char *body, const size_t size, const ResponseCode code,
                                 const std::string &mimeType, const std::chrono::seconds expires)
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
        http::response<http::string_body> res
        {
            std::piecewise_construct,
            std::make_tuple(body),
            std::make_tuple(http::status::ok, 11)// m_req.version())
        };
        res.set(http::field::server, "MTConnectAgent");
        res.set(http::field::date, getHeaderDate());
        res.set(http::field::connection, "close");
        res.set(http::field::expires, "-1");
        res.set(http::field::content_type, mimeType);
        //res.set(http::status::continue_, 100, 100);
        res.content_length(size);
        http::serializer<false, http::string_body , http::fields> sr{res};
        http::write(m_socket, sr, m_ec);
      }
    }

  }  // namespace http_server
}  // namespace mtconnect
