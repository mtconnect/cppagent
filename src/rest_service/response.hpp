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
#include <boost/beast/http/status.hpp>

#include <unordered_map>

#include "request.hpp"
#include "utilities.hpp"

namespace mtconnect
{
  class Printer;

  namespace rest_service
  {
    using status = boost::beast::http::status;

    struct Response
    {
      Response(status status = status::ok, const std::string &body = "",
               const std::string &mimeType = "text/xml")
        : m_status(status), m_body(body), m_mimeType(mimeType), m_expires(0)
      {
      }
      Response(RequestError &e) : m_status(e.m_code), m_body(e.m_body), m_mimeType(e.m_contentType)
      {
      }

      status m_status;
      std::string m_body;
      std::string m_mimeType;
      std::chrono::seconds m_expires;
      bool m_close {false};
    };
  }  // namespace rest_service
}  // namespace mtconnect
