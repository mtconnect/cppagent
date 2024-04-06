
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

#include <boost/beast/core/error.hpp>

#include <memory>

#include "mtconnect/config.hpp"
#include "mtconnect/entity/entity.hpp"

namespace mtconnect::source::adapter {
  struct Handler;
}
namespace mtconnect::pipeline {
  struct ResponseDocument;
}

namespace mtconnect::source::adapter::agent_adapter {
  struct AgentHandler;
  /// @brief Abstract interface for an HTTP or HTTPS session
  class Session : public std::enable_shared_from_this<Session>
  {
  public:
    using Next = std::function<bool()>;
    using Failure = std::function<void(std::error_code &ec)>;
    using UpdateAssets = std::function<void()>;

    /// @brief An HTTP Request wrapper
    struct Request
    {
      /// @brief Create a request
      /// @param device optional device this request is targeting
      /// @param operation the REST operation
      /// @param query The URL query parameters
      /// @param stream `true` if HTTP x-multipart-replace streaming is desired
      /// @param next Function to determine what to do on successful read
      Request(const std::optional<std::string> &device, const std::string &operation,
              const url::UrlQuery &query, bool stream, Next next)
        : m_sourceDevice(device),
          m_operation(operation),
          m_query(query),
          m_stream(stream),
          m_next(next)
      {}

      Request(const Request &request) = default;

      std::optional<std::string> m_sourceDevice;  ///< optional source device
      std::string m_operation;     ///< The REST operation (probe, current, sample, asset)
      url::UrlQuery m_query;       ///< URL Query parameters
      bool m_stream;               ///< `true` if using HTTP long pull
      Next m_next;                 ///< function to call on successful read
      int32_t m_agentVersion = 0;  ///< agent version if required > 0 for asset requests

      /// @brief Given a url, get a formatted target for a given operation
      /// @param url The base url
      /// @return a string with a new URL path and query (for the GET)
      auto getTarget(const url::Url &url)
      {
        return url.getTarget(m_sourceDevice, m_operation, m_query);
      }
    };

    virtual ~Session() {}

    /// @name Session interface
    ///@{

    /// @brief Is the current connection open
    /// @return `true` if it is open
    virtual bool isOpen() const = 0;
    /// @brief Stop the connection
    virtual void stop() = 0;
    /// @brief Method called with something fails
    /// @param ec the error code
    /// @param what descriptive message
    virtual void failed(std::error_code ec, const char *what) = 0;
    /// @brief close the connection
    virtual void close() = 0;

    /// @brief Make a request of the remote agent
    /// @param request the request
    /// @return `true` if successful
    virtual bool makeRequest(const Request &request) = 0;
    ///@}

    Handler *m_handler = nullptr;
    std::string m_identity;
    Failure m_failed;
    UpdateAssets m_updateAssets;
    bool m_closeConnectionAfterResponse = false;
    std::chrono::milliseconds m_timeout = std::chrono::milliseconds(30000);
    bool m_closeOnRead = false;
  };

}  // namespace mtconnect::source::adapter::agent_adapter
