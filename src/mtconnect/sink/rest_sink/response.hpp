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

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http/status.hpp>

#include <filesystem>
#include <unordered_map>

#include "cached_file.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/utilities.hpp"
#include "request.hpp"

namespace mtconnect {
  namespace printer {
    class Printer;
  }
  namespace sink::rest_sink {
    using status = boost::beast::http::status;

    /// @brief A response for a simple request request returning some content
    struct Response
    {
      /// @brief Create a response with a status and a body
      /// @param[in] status the status
      /// @param[in] body the body of the response
      /// @param[in] mimeType the mime type of the response
      Response(status status = status::ok, const std::string &body = "",
               const std::string &mimeType = "text/xml")
        : m_status(status), m_body(body), m_mimeType(mimeType), m_expires(0)
      {}
      /// @brief Create a response with a status and a cached file
      /// @param[in] status the status of the response
      /// @param[in] file the file
      Response(status status, CachedFilePtr file)
        : m_status(status), m_mimeType(file->m_mimeType), m_expires(0), m_file(file)
      {}
      /// @brief Creates a response with an error
      /// @param[in] e the error
      Response(RequestError &e) : m_status(e.m_code), m_body(e.m_body), m_mimeType(e.m_contentType)
      {}

      status m_status;                        ///< The return status
      std::string m_body;                     ///< The body of the response
      std::string m_mimeType;                 ///< The mime type of the response
      std::optional<std::string> m_location;  ///< optional location
      std::chrono::seconds
          m_expires;         ///< how long should this session should stay open before it is closed
      bool m_close {false};  ///< `true` if this session should closed after it responds
      std::optional<std::string> m_requestId;  ///< Request id from websocket sub

      CachedFilePtr m_file;  ///< Cached file if a file is being returned
    };

    using ResponsePtr = std::unique_ptr<Response>;
  }  // namespace sink::rest_sink
}  // namespace mtconnect
