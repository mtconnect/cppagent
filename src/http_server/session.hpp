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

#include "routing.hpp"
#include <boost/beast/http/status.hpp>
#include <boost/asio/ip/address.hpp>

#include <memory>
#include <functional>

namespace mtconnect
{
  namespace http_server
  {
    struct Response;
    class Session;
    using SessionPtr = std::shared_ptr<Session>;
    using ErrorFunction = std::function<void(SessionPtr,
                                             boost::beast::http::status status,
                                             const std::string &msg)>;

    using Dispatch = std::function<bool(RequestPtr)>;
    using Complete = std::function<void()>;
    using FieldList = std::list<std::pair<std::string, std::string>>;

    class Session : public std::enable_shared_from_this<Session>
    {
    public:
      Session(Dispatch dispatch, ErrorFunction func)
      : m_dispatch(dispatch), m_errorFunction(func) {}
      virtual ~Session() {}

      virtual void run() = 0;
      virtual void writeResponse(const Response &response, Complete complete = nullptr) = 0;
      virtual void beginStreaming(const std::string &mimeType, Complete complete) = 0;
      virtual void writeChunk(const std::string &chunk, Complete complete) = 0;
      virtual void close() = 0;
      virtual void closeStream() = 0;
      void allowPuts(bool allow = true)
      {
        m_allowPuts = allow;
      }
      void allowPutsFrom(std::set<boost::asio::ip::address> &hosts)
      {
        m_allowPuts = true;
        m_allowPutsFrom = hosts;
      }

    protected:
      Dispatch m_dispatch;
      ErrorFunction m_errorFunction;
      
      bool m_allowPuts{false};
      std::set<boost::asio::ip::address> m_allowPutsFrom;
    };
    
  }
}
