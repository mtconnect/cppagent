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

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <functional>
#include <memory>

#include "configuration/config_options.hpp"
#include "session.hpp"
#include "utilities.hpp"

namespace mtconnect
{
  class Printer;

  namespace http_server
  {
    class SessionImpl : public Session
    {
    public:
      SessionImpl(boost::asio::ip::tcp::socket &socket, const FieldList &list, Dispatch dispatch,
                  ErrorFunction error)
        : Session(dispatch, error), m_stream(std::move(socket)), m_fields(list)
      {
      }
      SessionImpl(const SessionImpl &) = delete;
      virtual ~SessionImpl() { close(); }
      std::shared_ptr<SessionImpl> shared_ptr()
      {
        return std::dynamic_pointer_cast<SessionImpl>(shared_from_this());
      }

      void run() override;
      void writeResponse(const Response &response, Complete complete = nullptr) override;
      void beginStreaming(const std::string &mimeType, Complete complete) override;
      void writeChunk(const std::string &chunk, Complete complete) override;
      void fail(boost::beast::http::status status, const std::string &message,
                boost::system::error_code ec = boost::system::error_code {});
      void close() override;
      void closeStream() override;

    protected:
      void requested(boost::system::error_code ec, size_t len);
      void sent(boost::system::error_code ec, size_t len);
      void read();
      void reset();

    protected:
      using RequestParser = boost::beast::http::request_parser<boost::beast::http::string_body>;

      boost::beast::tcp_stream m_stream;
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
      std::optional<RequestParser> m_parser;
      std::shared_ptr<void> m_response;
      std::shared_ptr<void> m_serializer;
    };
  }  // namespace http_server
}  // namespace mtconnect
