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

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http/status.hpp>

#include <functional>
#include <memory>

#include "routing.hpp"

namespace mtconnect::sink::rest_sink {
  struct Response;
  using ResponsePtr = std::unique_ptr<Response>;
  class Session;
  using SessionPtr = std::shared_ptr<Session>;
  using ErrorFunction =
      std::function<void(SessionPtr, boost::beast::http::status status, const std::string &msg)>;

  using Dispatch = std::function<bool(SessionPtr, RequestPtr)>;
  using Complete = std::function<void()>;
  using FieldList = std::list<std::pair<std::string, std::string>>;

  class Session : public std::enable_shared_from_this<Session>
  {
  public:
    Session(Dispatch dispatch, ErrorFunction func) : m_dispatch(dispatch), m_errorFunction(func) {}
    virtual ~Session() {}

    virtual void run() = 0;
    virtual void writeResponse(ResponsePtr &&response, Complete complete = nullptr) = 0;
    virtual void writeFailureResponse(ResponsePtr &&response, Complete complete = nullptr) = 0;
    virtual void beginStreaming(const std::string &mimeType, Complete complete) = 0;
    virtual void writeChunk(const std::string &chunk, Complete complete) = 0;
    virtual void close() = 0;
    virtual void closeStream() = 0;
    virtual void fail(boost::beast::http::status status, const std::string &message,
                      boost::system::error_code ec = boost::system::error_code {})
    {
      NAMED_SCOPE("Session::fail");

      LOG(warning) << "Operation failed: " << message;
      if (ec)
      {
        LOG(warning) << "Closing: " << ec.category().message(ec.value()) << " - " << ec.message();
        close();
      }
      else
      {
        m_errorFunction(shared_from_this(), status, message);
      }
    }

    void allowPuts(bool allow = true) { m_allowPuts = allow; }
    void allowPutsFrom(std::set<boost::asio::ip::address> &hosts)
    {
      m_allowPuts = true;
      m_allowPutsFrom = hosts;
    }
    auto getRemote() const { return m_remote; }
    void setUnauthorized(const std::string &msg)
    {
      m_message = msg;
      m_unauthorized = true;
    }

  protected:
    Dispatch m_dispatch;
    ErrorFunction m_errorFunction;

    std::string m_message;
    bool m_unauthorized {false};
    bool m_allowPuts {false};
    std::set<boost::asio::ip::address> m_allowPutsFrom;
    boost::asio::ip::tcp::endpoint m_remote;
  };

}  // namespace mtconnect::sink::rest_sink
