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

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http/status.hpp>

#include <functional>
#include <memory>

#include "mtconnect/config.hpp"
#include "mtconnect/observation/change_observer.hpp"
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

  /// @brief An abstract Session for an HTTP connection to a client
  ///
  /// The HTTP or HTTPS connections are subclasses of the session
  class Session : public std::enable_shared_from_this<Session>
  {
  public:
    /// @brief Create a session
    /// @param dispatch dispatching function to handle the request
    /// @param func error function to format the error response
    Session(Dispatch dispatch, ErrorFunction func) : m_dispatch(dispatch), m_errorFunction(func) {}
    virtual ~Session() {}

    /// @brief start the session
    virtual void run() = 0;
    /// @brief write the response to the client
    /// @param response the response
    /// @param complete optional completion callback
    virtual void writeResponse(ResponsePtr &&response, Complete complete = nullptr) = 0;
    /// @brief write a failure response to the client
    /// @param response the response
    /// @param complete optional completion callback
    virtual void writeFailureResponse(ResponsePtr &&response, Complete complete = nullptr) = 0;
    /// @brief begin streaming data to the client using x-multipart-replace
    /// @param mimeType the mime type of the response
    /// @param complete completion callback
    virtual void beginStreaming(const std::string &mimeType, Complete complete,
                                std::optional<std::string> requestId = std::nullopt) = 0;
    /// @brief write a chunk for a streaming session
    /// @param chunk the chunk to write
    /// @param complete a completion callback
    virtual void writeChunk(const std::string &chunk, Complete complete,
                            std::optional<std::string> requestId = std::nullopt) = 0;
    /// @brief close the session
    virtual void close() = 0;
    /// @brief close the stream
    virtual void closeStream() = 0;
    /// @brief Log a failure and close the session
    /// @param status the HTTP status
    /// @param message the message
    /// @param ec an optional error code
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

    /// @brief enable puts for the session
    /// @param allow `true` if puts are allowed
    void allowPuts(bool allow = true) { m_allowPuts = allow; }
    /// @brief allow puts from a set of hosts
    /// @note also sets allow puts to `true`
    /// @param hosts set of hosts
    void allowPutsFrom(std::set<boost::asio::ip::address> &hosts)
    {
      m_allowPuts = true;
      m_allowPutsFrom = hosts;
    }
    /// @brief get the remote endpoint
    /// @return the asio tcp endpoint
    auto &getRemote() const { return m_remote; }
    /// @brief set the request as unauthorized
    /// @param msg the rational message
    void setUnauthorized(const std::string &msg)
    {
      m_message = msg;
      m_unauthorized = true;
    }

    /// @brief Add an observer to the list for cleanup later.
    void addObserver(std::weak_ptr<observation::AsyncResponse> observer)
    {
      m_observers.push_back(observer);
    }

    bool cancelRequest(const std::string &requestId)
    {
      for (auto &obs : m_observers)
      {
        auto pobs = obs.lock();
        if (pobs && pobs->getRequestId() == requestId)
        {
          pobs->cancel();
          return true;
        }
      }
      return false;
    }

  protected:
    Dispatch m_dispatch;
    ErrorFunction m_errorFunction;

    std::string m_message;
    bool m_unauthorized {false};
    bool m_allowPuts {false};
    std::set<boost::asio::ip::address> m_allowPutsFrom;
    boost::asio::ip::tcp::endpoint m_remote;
    std::list<std::weak_ptr<observation::AsyncResponse>> m_observers;
  };

}  // namespace mtconnect::sink::rest_sink
