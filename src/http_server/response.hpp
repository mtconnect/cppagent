//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "globals.hpp"

#include <string>
#include <map>
#include <strstream>
#include <variant>
#include <ostream>
#include <chrono>

#include <dlib/logger.h>
#include <dlib/md5.h>

namespace mtconnect 
{
  namespace http_server
  {
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
      Response(std::ostream &out)
      : m_out(out)
      {
      }
      
      virtual ~Response() = default;
      
      virtual std::string getHeaderDate()
      {
        return getCurrentTime(HUM_READ);
      }
      
      virtual void beginMultipartStream()
      {
        if (m_out.ios_base::good())
        {
          m_boundary = dlib::md5(intToString(time(nullptr)));
          
          m_out << "HTTP/1.1 200 OK\r\n"
                   "Date: " << getHeaderDate() << "\r\n"
                   "Server: MTConnectAgent\r\n"
                   "Expires: -1\r\n"
                   "Connection: close\r\n"
                   "Cache-Control: private, max-age=0\r\n"
                   "Content-Type: multipart/x-mixed-replace;boundary="
                << m_boundary
                << "\r\n"
                   "Transfer-Encoding: chunked\r\n\r\n";
        }        
      }
      
      virtual void writeChunk(const std::string &chunk)
      {
        if (m_out.ios_base::good())
        {
          using namespace std;
          m_out.setf(ios::hex, ios::basefield);
          m_out << chunk.length() << "\r\n";
          m_out << chunk << "\r\n";
          m_out.flush();
        }
      }
      
      virtual void writeMultipartChunk(const std::string &body, const std::string &mimeType)
      {
        if (m_out.ios_base::good())
        {
          using namespace std;
          ostringstream str;
          m_out.setf(ios::dec, ios::basefield);
          str << "--" << m_boundary << "\r\n"
                 "Content-type: " << mimeType << "\r\n"
                 "Content-length: " << body.length() << "\r\n\r\n"
              << body;

          writeChunk(str.str());
        }
      }
      
      void writeResponse(const std::string &body,
                         const std::string &mimeType = "text/plain",
                         const ResponseCode code = OK,
                         const std::chrono::seconds expires = std::chrono::seconds(0))
      {
        writeResponse(body.c_str(), body.size(), code, mimeType, expires);
      }

      
      void writeResponse(const std::string &body,
                         const ResponseCode code,
                         const std::string &mimeType = "text/plain",
                         const std::chrono::seconds expires = std::chrono::seconds(0))
      {
        writeResponse(body.c_str(), body.size(), code, mimeType, expires);
      }
      
      virtual void writeResponse(const char *body,
                                 const size_t size,
                                 const ResponseCode code,
                                 const std::string &mimeType = "text/plain",
                                 const std::chrono::seconds expires = std::chrono::seconds(0))
      {
        if (m_out.ios_base::good())
        {
          using namespace std;
          string expiry;
          if (expires == 0s)
          {
            expiry = "Expires: -1\r\n"
                     "Cache-Control: private, max-age=0\r\n";
          }
          else
          {
            expiry = getCurrentTime(chrono::system_clock::now() + expires,
                                    HUM_READ);
          }

          m_out << "HTTP/1.1 " << code << " ";
          auto cm = m_status.find(code);
          if (cm == m_status.end())
            m_out << "Unknown";
          else
            m_out << cm->second;
          m_out << "\r\n"
                << "Date: " << getHeaderDate() << "\r\n"
                   "Server: MTConnectAgent\r\n"
                   "Connection: close\r\n" <<
                   expiry <<
                   "Content-Length: " << size << "\r\n"
                   "Content-Type: " << mimeType << "\r\n\r\n";
          m_out.write(body, size);
        }
      }

      
    protected:
      std::ostream &m_out;
      std::string m_boundary;
      static const std::map<uint16_t, std::string> m_status;
      static const std::map<std::string, uint16_t> m_codes;
    };
  }
}

    
