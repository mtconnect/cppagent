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

namespace mtconnect::sink::rest_sink {
  namespace beast = boost::beast;

  struct WebsocketRequest
  {
    WebsocketRequest(const std::string &id) : m_requestId(id) {}
    std::string m_requestId;
    std::optional<boost::asio::streambuf> m_streamBuffer;
    Complete m_complete;
    bool m_streaming {false};
    RequestPtr m_request;
  };

  /// @brief A websocket session that provides a pubsub interface using REST parameters
  template <class Derived>
  class WebsocketSession : public Session
  {
  protected:
    struct Message
    {
      Message(const std::string &body, Complete &complete, const std::string &requestId)
        : m_body(body), m_complete(complete), m_requestId(requestId)
      {}

      std::string m_body;
      Complete m_complete;
      std::string m_requestId;
    };

  public:
    using RequestMessage = boost::beast::http::request<boost::beast::http::string_body>;

    WebsocketSession(RequestPtr &&request, RequestMessage &&msg, Dispatch dispatch,
                     ErrorFunction func)
      : Session(dispatch, func), m_request(std::move(request)), m_msg(std::move(msg))
    {}

    /// @brief Session cannot be copied.
    WebsocketSession(const WebsocketSession &) = delete;
    ~WebsocketSession() = default;

    /// @brief get this as the `Derived` type
    /// @return the subclass
    Derived &derived() { return static_cast<Derived &>(*this); }

    void run() override
    {
      using namespace boost::beast;

      // Set suggested timeout settings for the websocket
      derived().stream().set_option(
          websocket::stream_base::timeout::suggested(beast::role_type::server));

      // Set a decorator to change the Server of the handshake
      derived().stream().set_option(
          websocket::stream_base::decorator([](websocket::response_type &res) {
            res.set(http::field::server, GetAgentVersion() + " MTConnectAgent");
          }));

      // Accept the websocket handshake
      derived().stream().async_accept(
          m_msg, boost::asio::bind_executor(derived().getExecutor(),
                                            beast::bind_front_handler(&WebsocketSession::onAccept,
                                                                      derived().shared_ptr())));
    }

    void close() override
    {
      NAMED_SCOPE("PlainWebsocketSession::close");
      if (!m_isOpen)
        return;

      m_isOpen = false;

      auto wptr = weak_from_this();
      std::shared_ptr<Session> ptr;
      if (!wptr.expired())
      {
        ptr = wptr.lock();
      }

      m_request.reset();
      m_requests.clear();
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

    void writeResponse(ResponsePtr &&response, Complete complete = nullptr) override
    {
      NAMED_SCOPE("WebsocketSession::writeResponse");
      if (!response->m_requestId)
      {
        boost::system::error_code ec;
        return fail(status::bad_request, "Missing request Id", ec);
      }

      writeChunk(response->m_body, complete, response->m_requestId);
    }

    void writeFailureResponse(ResponsePtr &&response, Complete complete = nullptr) override
    {
      NAMED_SCOPE("WebsocketSession::writeFailureResponse");
      writeChunk(response->m_body, complete, response->m_requestId);
    }

    void beginStreaming(const std::string &mimeType, Complete complete,
                        std::optional<std::string> requestId = std::nullopt) override
    {
      if (requestId)
      {
        auto id = *(requestId);
        auto it = m_requests.find(id);
        if (it != m_requests.end())
        {
          auto &req = it->second;
          req.m_streaming = true;

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

    void writeChunk(const std::string &chunk, Complete complete,
                    std::optional<std::string> requestId = std::nullopt) override
    {
      NAMED_SCOPE("WebsocketSession::writeChunk");

      if (!derived().stream().is_open())
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
    void onAccept(boost::beast::error_code ec)
    {
      if (ec)
      {
        fail(status::internal_server_error, "Error occurred in accpet", ec);
        return;
      }

      m_isOpen = true;

      derived().stream().async_read(
          m_buffer, beast::bind_front_handler(&WebsocketSession::onRead, derived().shared_ptr()));
    }

    void send(const std::string body, Complete complete, const std::string &requestId)
    {
      NAMED_SCOPE("WebsocketSession::send");

      using namespace std::placeholders;

      auto it = m_requests.find(requestId);
      if (it != m_requests.end())
      {
        auto &req = it->second;
        req.m_complete = std::move(complete);
        req.m_streamBuffer.emplace();
        std::ostream str(&req.m_streamBuffer.value());

        str << body;

        auto ref = derived().shared_ptr();

        LOG(debug) << "writing chunk for ws: " << requestId;

        m_busy = true;

        derived().stream().text(derived().stream().got_text());
        derived().stream().async_write(req.m_streamBuffer->data(),
                                       beast::bind_handler(
                                           [ref, requestId](beast::error_code ec, std::size_t len) {
                                             ref->sent(ec, len, requestId);
                                           },
                                           _1, _2));
      }
      else
      {
        LOG(error) << "Cannot find request for id: " << requestId;
      }
    }

    void sent(beast::error_code ec, std::size_t len, const std::string &id)
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

        auto it = m_requests.find(id);
        if (it != m_requests.end())
        {
          auto &req = it->second;
          if (req.m_complete)
          {
            boost::asio::post(derived().stream().get_executor(), req.m_complete);
            req.m_complete = nullptr;
          }

          if (!req.m_streaming)
          {
            m_requests.erase(id);
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
          auto &msg = m_messageQueue.front();
          send(msg.m_body, msg.m_complete, msg.m_requestId);
          m_messageQueue.pop_front();
        }
      }
    }

    void onRead(beast::error_code ec, std::size_t len)
    {
      NAMED_SCOPE("PlainWebsocketSession::onRead");

      if (ec)
        return fail(boost::beast::http::status::internal_server_error, "shutdown", ec);

      using namespace rapidjson;
      using namespace std;

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

      LOG(debug) << "Received :" << buffer.c_str();

      Document doc;
      doc.Parse(buffer.c_str(), len);

      if (doc.HasParseError())
      {
        LOG(warning) << "Websocket Read Error(offset (" << doc.GetErrorOffset()
                     << "): " << GetParseError_En(doc.GetParseError());
        LOG(warning) << "  " << buffer;
      }
      if (!doc.IsObject())
      {
        LOG(warning) << "Websocket Read Error: JSON message does not have a top level object";
        LOG(warning) << "  " << buffer;
      }
      else
      {
        // Extract the parameters from the json doc to map them to the REST
        // protocol parameters
        auto request = make_unique<Request>(*m_request);

        request->m_verb = beast::http::verb::get;
        request->m_parameters.clear();
#ifdef GetObject
#define __GOSave__ GetObject
#undef GetObject
#endif

        const auto &object = doc.GetObject();
#ifdef __GOSave__
#define GetObject __GOSave__
#endif

        for (auto &it : object)
        {
          switch (it.value.GetType())
          {
            case rapidjson::kNullType:
              // Skip nulls
              break;
            case rapidjson::kFalseType:
              request->m_parameters.emplace(
                  make_pair(string(it.name.GetString()), ParameterValue(false)));
              break;
            case rapidjson::kTrueType:
              request->m_parameters.emplace(
                  make_pair(string(it.name.GetString()), ParameterValue(true)));
              break;
            case rapidjson::kObjectType:
              break;
            case rapidjson::kArrayType:
              break;
            case rapidjson::kStringType:
              request->m_parameters.emplace(
                  make_pair(it.name.GetString(), ParameterValue(string(it.value.GetString()))));

              break;
            case rapidjson::kNumberType:
              if (it.value.IsInt())
                request->m_parameters.emplace(
                    make_pair(it.name.GetString(), ParameterValue(it.value.GetInt())));
              else if (it.value.IsUint())
                request->m_parameters.emplace(
                    make_pair(it.name.GetString(), ParameterValue(uint64_t(it.value.GetUint()))));
              else if (it.value.IsInt64())
                request->m_parameters.emplace(
                    make_pair(it.name.GetString(), ParameterValue(uint64_t(it.value.GetInt64()))));
              else if (it.value.IsUint64())
                request->m_parameters.emplace(
                    make_pair(it.name.GetString(), ParameterValue(it.value.GetUint64())));
              else if (it.value.IsDouble())
                request->m_parameters.emplace(
                    make_pair(it.name.GetString(), ParameterValue(double(it.value.GetDouble()))));

              break;
          }
        }

        if (request->m_parameters.count("request") > 0)
        {
          request->m_command = get<string>(request->m_parameters["request"]);
          request->m_parameters.erase("request");
        }
        if (request->m_parameters.count("id") > 0)
        {
          auto &v = request->m_parameters["id"];
          string id = visit(overloaded {[](monostate m) { return ""s; },
                                        [](auto v) { return boost::lexical_cast<string>(v); }},
                            v);
          request->m_requestId = id;
          request->m_parameters.erase("id");
        }

        auto &id = *(request->m_requestId);
        auto res = m_requests.emplace(id, id);
        if (!res.second)
        {
          LOG(error) << "Duplicate request id: " << id;
          boost::system::error_code ec;
          fail(status::bad_request, "Duplicate request Id", ec);
        }
        else
        {
          LOG(debug) << "Received request id: " << id;

          res.first->second.m_request = std::move(request);
          if (!m_dispatch(derived().shared_ptr(), res.first->second.m_request))
          {
            ostringstream txt;
            txt << "Failed to find handler for " << buffer;
            LOG(error) << txt.str();
            boost::system::error_code ec;
            fail(status::bad_request, "Duplicate request Id", ec);
          }
        }
      }

      derived().stream().async_read(
          m_buffer, beast::bind_front_handler(&WebsocketSession::onRead, derived().shared_ptr()));
    }

  protected:
    RequestPtr m_request;
    RequestMessage m_msg;
    beast::flat_buffer m_buffer;
    std::map<std::string, WebsocketRequest> m_requests;
    std::mutex m_mutex;
    std::atomic_bool m_busy {false};
    std::deque<Message> m_messageQueue;
    bool m_isOpen {false};
  };

  template <class Derived>
  using WebsocketSessionPtr = std::shared_ptr<WebsocketSession<Derived>>;

  class PlainWebsocketSession : public WebsocketSession<PlainWebsocketSession>
  {
  public:
    using Stream = beast::websocket::stream<beast::tcp_stream>;

    PlainWebsocketSession(beast::tcp_stream &&stream, RequestPtr &&request, RequestMessage &&msg,
                          Dispatch dispatch, ErrorFunction func)
      : WebsocketSession(std::move(request), std::move(msg), dispatch, func),
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

    auto getExecutor() { return m_stream.get_executor(); }

    auto &stream() { return m_stream; }

    /// @brief Get a pointer cast as an Websocket Session
    /// @return shared pointer to an Websocket session
    std::shared_ptr<PlainWebsocketSession> shared_ptr()
    {
      return std::dynamic_pointer_cast<PlainWebsocketSession>(shared_from_this());
    }

  protected:
    Stream m_stream;
  };

  class TlsWebsocketSession : public WebsocketSession<TlsWebsocketSession>
  {
  public:
    using Stream = beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>;

    TlsWebsocketSession(beast::ssl_stream<beast::tcp_stream> &&stream, RequestPtr &&request,
                        RequestMessage &&msg, Dispatch dispatch, ErrorFunction func)
      : WebsocketSession(std::move(request), std::move(msg), dispatch, func),
        m_stream(std::move(stream))
    {
      beast::get_lowest_layer(m_stream).expires_never();
    }
    ~TlsWebsocketSession()
    {
      if (m_isOpen)
        close();
    }

    auto &stream() { return m_stream; }

    auto getExecutor() { return m_stream.get_executor(); }

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
