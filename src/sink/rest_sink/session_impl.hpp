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
#include <optional>

#include "configuration/config_options.hpp"
#include "session.hpp"
#include "utilities.hpp"

namespace mtconnect {
  namespace printer {
    class Printer;
  }

  namespace sink::rest_sink {
    template <class Derived>
    class SessionImpl : public Session
    {
    public:
      SessionImpl(boost::beast::flat_buffer &&buffer, const FieldList &list, Dispatch dispatch,
                  ErrorFunction error)
        : Session(dispatch, error), m_fields(list), m_buffer(std::move(buffer))
      {}
      SessionImpl(const SessionImpl &) = delete;
      virtual ~SessionImpl() {}
      std::shared_ptr<SessionImpl> shared_ptr()
      {
        return std::dynamic_pointer_cast<SessionImpl>(shared_from_this());
      }
      Derived &derived() { return static_cast<Derived &>(*this); }

      void run() override;
      void writeResponse(ResponsePtr &&response, Complete complete = nullptr) override;
      void writeFailureResponse(ResponsePtr &&response, Complete complete = nullptr) override;
      void beginStreaming(const std::string &mimeType, Complete complete) override;
      void writeChunk(const std::string &chunk, Complete complete) override;
      void closeStream() override;

    protected:
      template <typename T>
      void addHeaders(const Response &response, T &res);

      void requested(boost::system::error_code ec, size_t len);
      void sent(boost::system::error_code ec, size_t len);
      void read();
      void reset();

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

    class HttpSession : public SessionImpl<HttpSession>
    {
    public:
      HttpSession(boost::beast::tcp_stream &&stream, boost::beast::flat_buffer &&buffer,
                  const FieldList &list, Dispatch dispatch, ErrorFunction error)
        : SessionImpl<HttpSession>(move(buffer), list, dispatch, error), m_stream(std::move(stream))
      {
        m_remote = m_stream.socket().remote_endpoint();
      }
      std::shared_ptr<HttpSession> shared_ptr()
      {
        return std::dynamic_pointer_cast<HttpSession>(shared_from_this());
      }
      virtual ~HttpSession() { close(); }

      auto &stream() { return m_stream; }

      void close() override
      {
        NAMED_SCOPE("HttpSession::close");

        m_request.reset();
        boost::beast::error_code ec;
        m_stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
      }

    protected:
      boost::beast::tcp_stream m_stream;
    };
  }  // namespace sink::rest_sink
}  // namespace mtconnect
