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

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include "mtconnect/config.hpp"
#include "mtconnect/configuration/config_options.hpp"
#include "mtconnect/utilities.hpp"
#include "session.hpp"
#include "websocket_request_manager.hpp"

namespace mtconnect::sink::rest_sink {
  namespace beast = boost::beast;

  /// @brief A websocket session that abstracts out the reading and writing to the stream for
  /// testing. This uses the Curiously Recurring Template Pattern (CRTP) to allow the derived class
  /// to implement stream methods for performance.
  template <class Derived>
  class WebsocketSession : public Session
  {
  protected:
    struct Message
    {
      Message(const std::string& body, Complete& complete, const std::string& requestId)
        : m_body(body), m_complete(complete), m_requestId(requestId)
      {}

      std::string m_body;
      Complete m_complete;
      std::string m_requestId;
    };

  public:
    WebsocketSession(RequestPtr&& request, Dispatch dispatch, ErrorFunction func)
      : Session(dispatch, func), m_requestManager(std::move(request), dispatch)
    {}

    /// @brief Session cannot be copied.
    WebsocketSession(const WebsocketSession&) = delete;
    ~WebsocketSession() = default;

    Derived& derived() { return static_cast<Derived&>(*this); }

    auto& getRequestManager() { return m_requestManager; }

    void close() override
    {
      NAMED_SCOPE("WebsocketSession::close");
      if (!m_isOpen)
        return;

      m_isOpen = false;

      auto wptr = weak_from_this();
      std::shared_ptr<Session> ptr;
      if (!wptr.expired())
      {
        ptr = wptr.lock();
      }

      m_requestManager.reset();
      for (auto obs : m_observers)
      {
        auto optr = obs.lock();
        if (optr)
        {
          optr->cancel();
        }
      }
      closeStream();
    }

    void writeResponse(ResponsePtr&& response, Complete complete = nullptr) override
    {
      NAMED_SCOPE("WebsocketSession::writeResponse");
      if (!response->m_requestId)
      {
        boost::system::error_code ec;
        return fail(status::bad_request, "Missing request Id", ec);
      }

      writeChunk(response->m_body, complete, response->m_requestId);
    }

    void writeFailureResponse(ResponsePtr&& response, Complete complete = nullptr) override
    {
      NAMED_SCOPE("WebsocketSession::writeFailureResponse");
      writeChunk(response->m_body, complete, response->m_requestId);
    }

    void beginStreaming(const std::string& mimeType, Complete complete,
                        std::optional<std::string> requestId = std::nullopt) override
    {
      if (requestId)
      {
        auto id = *(requestId);
        auto req = m_requestManager.findRequest(id);
        if (req != nullptr)
        {
          req->m_streaming = true;

          if (complete)
          {
            complete();
          }
        }
        else
        {
          LOG(error) << "Cannot find request for id: " << id;
        }
      }
      else
      {
        LOG(error) << "No request id for websocket";
      }
    }

    void writeChunk(const std::string& chunk, Complete complete,
                    std::optional<std::string> requestId = std::nullopt) override
    {
      NAMED_SCOPE("WebsocketSession::writeChunk");

      if (!derived().isStreamOpen())
      {
        return;
      }

      if (requestId)
      {
        LOG(trace) << "Waiting for mutex";
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_busy || m_messageQueue.size() > 0)
        {
          LOG(debug) << "Queuing Chunk for " << *requestId;
          m_messageQueue.emplace_back(chunk, complete, *requestId);
        }
        else
        {
          LOG(debug) << "Writing Chunk for " << *requestId;
          send(chunk, complete, *requestId);
        }
      }
      else
      {
        LOG(error) << "No request id for websocket";
      }
    }

  protected:
    void send(const std::string body, Complete complete, const std::string& requestId)
    {
      NAMED_SCOPE("WebsocketSession::send");

      using namespace std::placeholders;

      auto req = m_requestManager.findRequest(requestId);
      if (req != nullptr)
      {
        req->m_complete = std::move(complete);
        req->m_streamBuffer.emplace();
        std::ostream str(&req->m_streamBuffer.value());

        str << body;

        LOG(debug) << "writing chunk for ws: " << requestId;

        m_busy = true;
        derived().asyncSend(req);
      }
      else
      {
        LOG(error) << "Cannot find request for id: " << requestId;
      }
    }

    void sent(beast::error_code ec, std::size_t len, const std::string& id)
    {
      NAMED_SCOPE("WebsocketSession::sent");

      if (ec)
      {
        return fail(status::bad_request, "Missing request Id", ec);
      }

      {
        LOG(trace) << "Waiting for mutex";
        std::lock_guard<std::mutex> lock(m_mutex);

        LOG(trace) << "sent chunk for ws: " << id;

        auto req = m_requestManager.findRequest(id);
        if (req != nullptr)
        {
          if (req->m_complete)
          {
            boost::asio::post(derived().getExecutor(), req->m_complete);
            req->m_complete = nullptr;
          }

          if (!req->m_streaming)
          {
            m_requestManager.remove(id);
          }

          if (m_messageQueue.size() == 0)
          {
            m_busy = false;
          }
        }
        else
        {
          LOG(error) << "WebsocketSession::sent: Cannot find request for id: " << id;
        }
      }

      {
        LOG(trace) << "Waiting for mutex to send next";
        std::lock_guard<std::mutex> lock(m_mutex);

        // Check for queued messages
        if (m_messageQueue.size() > 0)
        {
          auto& msg = m_messageQueue.front();
          send(msg.m_body, msg.m_complete, msg.m_requestId);
          m_messageQueue.pop_front();
        }
      }
    }

  protected:
    WebsocketRequestManager m_requestManager;
    std::mutex m_mutex;
    std::atomic_bool m_busy {false};
    std::deque<Message> m_messageQueue;
    bool m_isOpen {false};
  };

  /// @brief An intermediary class to implement a websocket stream connect, read, and write
  /// semantics. This uses the Curiously Recurring Template Pattern (CRTP) to allow the derived
  /// class to implement plain or SSL connections.
  template <class Derived>
  class WebsocketSessionImpl : public WebsocketSession<Derived>
  {
  public:
    using RequestMessage = boost::beast::http::request<boost::beast::http::string_body>;
    using super = WebsocketSession<Derived>;

    WebsocketSessionImpl(RequestPtr&& request, RequestMessage&& msg, Dispatch dispatch,
                         ErrorFunction func)
      : super(std::move(request), dispatch, func), m_msg(std::move(msg))
    {}

    /// @brief Session cannot be copied.
    WebsocketSessionImpl(const WebsocketSessionImpl&) = delete;
    ~WebsocketSessionImpl() = default;

    /// @brief get this as the `Derived` type
    /// @return the subclass
    Derived& derived() { return static_cast<Derived&>(*this); }

    bool isStreamOpen() { return derived().stream().is_open(); }

    auto getExecutor() { return derived().stream().get_executor(); }

    void run() override
    {
      using namespace boost::beast;

      // Set suggested timeout settings for the websocket
      derived().stream().set_option(
          websocket::stream_base::timeout::suggested(beast::role_type::server));

      // Set a decorator to change the Server of the handshake
      derived().stream().set_option(
          websocket::stream_base::decorator([](websocket::response_type& res) {
            res.set(http::field::server, GetAgentVersion() + " MTConnectAgent");
          }));

      // Accept the websocket handshake
      derived().stream().async_accept(
          m_msg,
          boost::asio::bind_executor(
              derived().getExecutor(),
              beast::bind_front_handler(&WebsocketSessionImpl::onAccept, derived().shared_ptr())));
    }

  protected:
    friend class WebsocketSession<Derived>;

    void onAccept(boost::beast::error_code ec)
    {
      if (ec)
      {
        super::fail(status::internal_server_error, "Error occurred in accpet", ec);
        return;
      }

      super::m_isOpen = true;

      derived().stream().async_read(
          m_buffer,
          beast::bind_front_handler(&WebsocketSessionImpl::onRead, derived().shared_ptr()));
    }

    void asyncSend(WebsocketRequestManager::WebsocketRequest* request)
    {
      NAMED_SCOPE("WebsocketSessionImpl::asyncSend");

      using namespace std::placeholders;

      auto ref = derived().shared_ptr();

      auto& requestId = request->m_requestId;
      derived().stream().text(derived().stream().got_text());
      derived().stream().async_write(
          request->m_streamBuffer->data(),
          beast::bind_handler([ref, requestId](beast::error_code ec,
                                               std::size_t len) { ref->sent(ec, len, requestId); },
                              _1, _2));
    }

    void onRead(beast::error_code ec, std::size_t len)
    {
      NAMED_SCOPE("PlainWebsocketSession::onRead");

      if (ec)
        return super::fail(boost::beast::http::status::internal_server_error, "shutdown", ec);

      if (len == 0)
      {
        LOG(debug) << "Empty message received";
        return;
      }

      // Parse the buffer as a JSON request with parameters matching
      // REST API
      derived().stream().text(derived().stream().got_text());
      auto buffer = beast::buffers_to_string(m_buffer.data());
      m_buffer.consume(m_buffer.size());

      LOG(debug) << "Received :" << buffer;

      try
      {
        if (!super::m_requestManager.dispatch(derived().shared_ptr(), buffer))
        {
          std::stringstream txt;
          txt << super::getRemote().address() << ": Dispatch failed for: " << buffer;
          LOG(error) << txt.str();
        }
      }

      catch (RestError& re)
      {
        auto id = re.getRequestId();
        if (!id)
        {
          id = "ERROR";
          re.setRequestId(*id);
        }

        super::m_requestManager.findOrCreateRequest(*id);
        super::m_errorFunction(derived().shared_ptr(), re);
      }

      catch (std::logic_error& le)
      {
        std::stringstream txt;
        txt << super::getRemote().address() << ": Logic Error: " << le.what();
        LOG(error) << txt.str();
        super::fail(boost::beast::http::status::not_found, txt.str());
      }

      catch (...)
      {
        std::stringstream txt;
        txt << super::getRemote().address() << ": Unknown Error thrown";
        LOG(error) << txt.str();
        super::fail(boost::beast::http::status::not_found, txt.str());
      }

      derived().stream().async_read(
          m_buffer,
          beast::bind_front_handler(&WebsocketSessionImpl::onRead, derived().shared_ptr()));
    }

  protected:
    RequestMessage m_msg;
    beast::flat_buffer m_buffer;
  };

  /// @brief Plain Websocket Session for HTTP connection
  class PlainWebsocketSession : public WebsocketSessionImpl<PlainWebsocketSession>
  {
  public:
    using Stream = beast::websocket::stream<beast::tcp_stream>;

    PlainWebsocketSession(beast::tcp_stream&& stream, RequestPtr&& request, RequestMessage&& msg,
                          Dispatch dispatch, ErrorFunction func)
      : WebsocketSessionImpl(std::move(request), std::move(msg), dispatch, func),
        m_stream(std::move(stream))
    {
      beast::get_lowest_layer(m_stream).expires_never();
    }
    ~PlainWebsocketSession()
    {
      if (m_isOpen)
        close();
    }

    void closeStream() override
    {
      if (m_isOpen && m_stream.is_open())
        m_stream.close(beast::websocket::close_code::none);
    }

    auto& stream() { return m_stream; }

    /// @brief Get a pointer cast as an Websocket Session
    /// @return shared pointer to an Websocket session
    std::shared_ptr<PlainWebsocketSession> shared_ptr()
    {
      return std::dynamic_pointer_cast<PlainWebsocketSession>(shared_from_this());
    }

  protected:
    Stream m_stream;
  };

  /// @brief SSL Websocket Session for HTTPS connection
  class TlsWebsocketSession : public WebsocketSessionImpl<TlsWebsocketSession>
  {
  public:
    using Stream = beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>;

    TlsWebsocketSession(beast::ssl_stream<beast::tcp_stream>&& stream, RequestPtr&& request,
                        RequestMessage&& msg, Dispatch dispatch, ErrorFunction func)
      : WebsocketSessionImpl(std::move(request), std::move(msg), dispatch, func),
        m_stream(std::move(stream))
    {
      beast::get_lowest_layer(m_stream).expires_never();
    }
    ~TlsWebsocketSession()
    {
      if (m_isOpen)
        close();
    }

    auto& stream() { return m_stream; }

    void closeStream() override
    {
      if (m_isOpen && m_stream.is_open())
        m_stream.close(beast::websocket::close_code::none);
    }

    /// @brief Get a pointer cast as an TLS Websocket Session
    /// @return shared pointer to an TLS Websocket session
    std::shared_ptr<TlsWebsocketSession> shared_ptr()
    {
      return std::dynamic_pointer_cast<TlsWebsocketSession>(shared_from_this());
    }

  protected:
    Stream m_stream;
  };

}  // namespace mtconnect::sink::rest_sink
