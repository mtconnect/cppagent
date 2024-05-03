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

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <functional>
#include <memory>
#include <optional>

#include "mtconnect/config.hpp"
#include "mtconnect/configuration/config_options.hpp"
#include "mtconnect/utilities.hpp"
#include "session.hpp"

namespace mtconnect {
  namespace printer {
    class Printer;
  }

  namespace sink::rest_sink {
    template <class Derived>
    class WebsocketSession;
    template <class Derived>
    using WebsocketSessionPtr = std::shared_ptr<WebsocketSession<Derived>>;

    /// @brief A session implementation `Derived` subclass pattern
    /// @tparam subclass of this class to use the same methods with http or https protocol streams
    template <class Derived>
    class SessionImpl : public Session
    {
    public:
      /// @brief Create a Session
      /// @param buffer buffer (takes ownership)
      /// @param list http fields
      /// @param dispatch dispatch method
      /// @param error error function
      SessionImpl(boost::beast::flat_buffer &&buffer, const FieldList &list, Dispatch dispatch,
                  ErrorFunction error)
        : Session(dispatch, error), m_fields(list), m_buffer(std::move(buffer))
      {}
      /// @brief Sessions cannot be copied
      SessionImpl(const SessionImpl &) = delete;
      virtual ~SessionImpl() {}

      /// @brief get a shared pointer to this
      /// @return shared session impl
      std::shared_ptr<SessionImpl> shared_ptr()
      {
        return std::dynamic_pointer_cast<SessionImpl>(shared_from_this());
      }
      /// @brief get this as the `Derived` type
      /// @return the subclass
      Derived &derived() { return static_cast<Derived &>(*this); }

      /// @name Session Interface
      ///@{
      void run() override;
      void writeResponse(ResponsePtr &&response, Complete complete = nullptr) override;
      void writeFailureResponse(ResponsePtr &&response, Complete complete = nullptr) override;
      void beginStreaming(const std::string &mimeType, Complete complete,
                          std::optional<std::string> requestId = std::nullopt) override;
      void writeChunk(const std::string &chunk, Complete complete,
                      std::optional<std::string> requestId = std::nullopt) override;
      void closeStream() override;
      ///@}
    protected:
      using RequestMessage = boost::beast::http::request<boost::beast::http::string_body>;

      template <typename T>
      void addHeaders(const Response &response, T &res);

      void requested(boost::system::error_code ec, size_t len);
      void sent(boost::system::error_code ec, size_t len);
      void read();
      void reset();
      void upgrade(RequestMessage &&msg);

    protected:
      using RequestParser = boost::beast::http::request_parser<boost::beast::http::string_body>;

      Complete m_complete;
      bool m_streaming {false};

      // For Streaming
      std::string m_boundary;
      std::string m_mimeType;
      bool m_close {false};

      // Additional fields
      FieldList m_fields;

      // References to retain lifecycle for callbacks.
      RequestPtr m_request;
      boost::beast::flat_buffer m_buffer;
      std::optional<boost::asio::streambuf> m_streamBuffer;
      std::optional<RequestParser> m_parser;
      std::shared_ptr<void> m_response;
      std::shared_ptr<void> m_serializer;
      ResponsePtr m_outgoing;
    };

    /// @brief An HTTP Session for communication without TLS
    class HttpSession : public SessionImpl<HttpSession>
    {
    public:
      /// @brief Create an http session with a plain tcp stream
      /// @param stream the stream (takes ownership)
      /// @param buffer the communiction buffer (takes ownership)
      /// @param list list of fields
      /// @param dispatch dispatch function
      /// @param error error format function
      HttpSession(boost::beast::tcp_stream &&stream, boost::beast::flat_buffer &&buffer,
                  const FieldList &list, Dispatch dispatch, ErrorFunction error)
        : SessionImpl<HttpSession>(std::move(buffer), list, dispatch, error),
          m_stream(std::move(stream))
      {
        m_remote = m_stream.socket().remote_endpoint();
      }
      /// @brief Get a pointer cast as an HTTP Session
      /// @return shared pointer to an http session
      std::shared_ptr<HttpSession> shared_ptr()
      {
        return std::dynamic_pointer_cast<HttpSession>(shared_from_this());
      }
      /// @brief destruct and close the session
      virtual ~HttpSession() { close(); }
      /// @brief get the stream
      /// @return the stream
      auto &stream() { return m_stream; }

      /// @brief close the session and shutdown the socket
      void close() override
      {
        NAMED_SCOPE("HttpSession::close");

        m_request.reset();
        boost::beast::error_code ec;
        m_stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
      }

      /// @brief Upgrade the current connection to a websocket connection.
      SessionPtr upgradeToWebsocket(RequestMessage &&msg);

    protected:
      boost::beast::tcp_stream m_stream;
    };
  }  // namespace sink::rest_sink
}  // namespace mtconnect
