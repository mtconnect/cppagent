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

#include <boost/beast/http/status.hpp>
#include <boost/beast/http/verb.hpp>

#include <optional>

#include "mtconnect/config.hpp"
#include "parameter.hpp"

namespace mtconnect::sink::rest_sink {
  /// @brief An error that occurred during a request
  class AGENT_LIB_API RequestError : public std::logic_error
  {
  public:
    /// @brief Create a simple error message related to a request
    RequestError(const char *w) : std::logic_error::logic_error(w) {}
    /// @brief Create a request error
    /// @param w the message
    /// @param body the body of the request
    /// @param type the request type
    /// @param code the boost status code
    RequestError(const char *w, const std::string &body, const std::string &type,
                 boost::beast::http::status code)
      : std::logic_error::logic_error(w), m_contentType(type), m_body(body), m_code(code)
    {}
    RequestError(const RequestError &) = default;
    ~RequestError() override = default;

    std::string m_contentType;
    std::string m_body;
    boost::beast::http::status m_code;
  };

  class Session;
  using SessionPtr = std::shared_ptr<Session>;

  /// @brief A wrapper around an incoming HTTP request
  ///
  /// The request can be a simple reply response or streaming request
  struct Request
  {
    Request() = default;
    Request(const Request &request) = default;

    boost::beast::http::verb m_verb;  ///< GET, PUT, POST, or DELETE
    std::string m_body;               ///< The body of the request
    std::string m_accepts;            ///< The accepts header
    std::string m_acceptsEncoding;    ///< Encodings that can be returned
    std::string m_contentType;        ///< The content type for the body
    std::string m_path;               ///< The URI for the request
    std::string m_foreignIp;          ///< The requestors IP Address
    uint16_t m_foreignPort;           ///< The requestors Port
    QueryMap m_query;                 ///< The parsed query parameters
    ParameterMap m_parameters;        ///< The parsed path parameters

    std::optional<std::string> m_requestId;  ///< Request id from websocket sub
    std::optional<std::string> m_command;    ///< Specific request from websocket

    /// @brief Find a parameter by type
    /// @tparam T the type of the parameter
    /// @param s the name of the parameter
    /// @return an option type `T` if the parameter is found
    template <typename T>
    std::optional<T> parameter(const std::string &s) const
    {
      auto v = m_parameters.find(s);
      if (v == m_parameters.end())
        return std::nullopt;
      else
        return std::get<T>(v->second);
    }
  };

  using RequestPtr = std::shared_ptr<Request>;
}  // namespace mtconnect::sink::rest_sink
