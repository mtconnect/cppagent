
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

#pragma once

#include <boost/beast/core/error.hpp>

#include <memory>

#include "entity/entity.hpp"
#include "url_parser.hpp"

namespace mtconnect::source::adapter {
  struct Handler;
}
namespace mtconnect::pipeline {
  struct ResponseDocument;
}

namespace mtconnect::source::adapter::agent_adapter {
  struct AgentHandler;
  class Session : public std::enable_shared_from_this<Session>
  {
  public:
    using Next = std::function<bool()>;
    using Failure = std::function<void(std::error_code &ec)>;
    using UpdateAssets = std::function<void()>;

    struct Request
    {
      Request(const std::optional<std::string> &device, const std::string &suffix,
              const UrlQuery &query, bool stream, Next next)
        : m_sourceDevice(device), m_suffix(suffix), m_query(query), m_stream(stream), m_next(next)
      {}

      Request(const Request &request) = default;

      std::optional<std::string> m_sourceDevice;
      std::string m_suffix;
      UrlQuery m_query;
      bool m_stream;
      Next m_next;

      auto getTarget(const Url &url) { return url.getTarget(m_sourceDevice, m_suffix, m_query); }
    };

    virtual ~Session() {}
    virtual bool isOpen() const = 0;
    virtual void stop() = 0;
    virtual void failed(std::error_code ec, const char *what) = 0;

    virtual bool makeRequest(const Request &request) = 0;

    Handler *m_handler = nullptr;
    std::string m_identity;
    Failure m_failed;
    UpdateAssets m_updateAssets;
    bool m_closeConnectionAfterResponse = false;
    std::chrono::milliseconds m_timeout = std::chrono::milliseconds(30000);
  };

}  // namespace mtconnect::source::adapter::agent_adapter
