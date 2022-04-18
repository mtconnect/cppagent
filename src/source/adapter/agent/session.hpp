
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

#include <boost/beast/core/error.hpp>

#include <memory>

#include "url_parser.hpp"

namespace mtconnect::source::adapter {
  struct Handler;
}

namespace mtconnect::source::adapter::agent {

  struct ResponseDocument;
  struct AgentHandler;
  class Session : public std::enable_shared_from_this<Session>
  {
  public:
    using Next = std::function<bool()>;
    using Reconnect = std::function<void()>;
    using Failure = std::function<void()>;

    virtual ~Session() {}
    virtual bool isOpen() const = 0;
    virtual void stop() = 0;
    virtual void failed(boost::beast::error_code ec, const char *what, bool reconnect = true) = 0;

    virtual bool makeRequest(const std::string &suffix, const UrlQuery &query, bool stream,
                             Next next) = 0;

    Handler *m_handler = nullptr;
    std::string m_identity;
    Reconnect m_reconnect;
    Failure m_failed;
  };

}  // namespace mtconnect::source::adapter::agent
