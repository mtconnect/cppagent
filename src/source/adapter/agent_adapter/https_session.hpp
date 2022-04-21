
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

#include <boost/beast/ssl.hpp>

#include "session_impl.hpp"

namespace mtconnect::source::adapter::agent_adapter {

  // HTTPS Session
  class HttpsSession : public SessionImpl<HttpsSession>
  {
  public:
    using super = SessionImpl<HttpsSession>;

    explicit HttpsSession(boost::asio::io_context::strand &ex, const Url &url, ssl::context &ctx)
      : super(ex, url), m_stream(ex.context(), ctx)
    {}
    ~HttpsSession() {}

    auto &stream() { return m_stream; }
    auto &lowestLayer() { return beast::get_lowest_layer(m_stream); }
    const auto &lowestLayer() const { return beast::get_lowest_layer(m_stream); }

    shared_ptr<HttpsSession> getptr()
    {
      return std::static_pointer_cast<HttpsSession>(shared_from_this());
    }

    // Start the asynchronous operation
    void connect() override
    {
      if (!SSL_set_tlsext_host_name(m_stream.native_handle(), m_url.getHost().c_str()))
      {
        beast::error_code ec {static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category()};
        std::cerr << ec.message() << "\n";
        return;
      }

      super::connect();
    }

    void onConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type)
    {
      if (ec)
        return failed(ec, "connect");

      // Set a timeout on the operation
      beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));

      // Perform the SSL handshake
      m_stream.async_handshake(
          ssl::stream_base::client,
          asio::bind_executor(m_strand,
                              beast::bind_front_handler(&HttpsSession::onHandshake, getptr())));
    }

    void onHandshake(beast::error_code ec)
    {
      if (ec)
        failed(ec, "handshake", false);

      if (m_handler && m_handler->m_connected)
        m_handler->m_connected(m_identity);

      request();
    }

    void disconnect()
    {
      // Set a timeout on the operation
      beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));

      // Gracefully close the stream
      m_stream.async_shutdown(asio::bind_executor(
          m_strand, beast::bind_front_handler(&HttpsSession::onShutdown, getptr())));
    }

    void onShutdown(beast::error_code ec)
    {
      if (ec == asio::error::eof)
      {
        // Rationale:
        // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
        ec = {};
      }

      if (m_handler && m_handler->m_disconnected)
        m_handler->m_disconnected(m_identity);

      if (ec)
        return failed(ec, "shutdown");

      // If we get here then the connection is closed gracefully
      lowestLayer().close();
    }

  protected:
    beast::ssl_stream<beast::tcp_stream> m_stream;
  };
}  // namespace mtconnect::source::adapter::agent_adapter
