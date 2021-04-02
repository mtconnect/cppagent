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

#include <boost/asio/ip/tcp.hpp>

#include "utilities.hpp"
#include <unordered_map>

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include <chrono>
#include <map>
#include <ostream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>

namespace net = boost::asio;            // from <boost/asio.hpp>

namespace mtconnect
{
  namespace http_server
  {

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

      bool good() const { return m_socket.is_open(); }
      void setBad() { m_socket.close(); }
      static std::string getStatus(uint16_t code)
      {
        auto cm = m_status.find(code);
        if (cm == m_status.end())
          return "Unknown";
        else
          return cm->second;
      }

      virtual void beginMultipartStream();
      virtual void writeMultipartChunk(const std::string &body, const std::string &mimeType);
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
                                 const std::chrono::seconds expires = std::chrono::seconds(0));
    protected:
      tcp::socket m_socket;
      std::string m_boundary;
      static const std::unordered_map<uint16_t, std::string> m_status;
      static const std::unordered_map<std::string, uint16_t> m_codes;
      StringList m_fields;
      boost::system::error_code m_ec;
    };
  }  // namespace http_server
}  // namespace mtconnect
