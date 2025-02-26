
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

#include "mtconnect/config.hpp"
#include "session_impl.hpp"

namespace mtconnect::source::adapter::agent_adapter {

  /// @brief HTTP Agent Adapter Session
  class HttpSession : public SessionImpl<HttpSession>
  {
  public:
    /// @brief The superclass is using a Derived class pattern
    using super = SessionImpl<HttpSession>;

    /// @brief Create a session to connect to the remote agent
    /// @param ioc the asio strand to run in
    /// @param url URL to connect to
    HttpSession(boost::asio::io_context::strand &ioc, const url::Url &url)
      : super(ioc, url), m_stream(ioc.context())
    {}

    ~HttpSession() override
    {
      if (isOpen())
        beast::get_lowest_layer(m_stream).close();
    }

    /// @brief Get a shared pointer to this
    /// @return shared pointer to this
    shared_ptr<HttpSession> getptr()
    {
      return static_pointer_cast<HttpSession>(shared_from_this());
    }

    /// @brief Get the boost asio tcp stream
    /// @return reference to the stream
    auto &stream() { return m_stream; }
    /// @brief Get the lowest protocol layer to the tcp stream
    /// @return lowest protocol layer
    auto &lowestLayer() { return beast::get_lowest_layer(m_stream); }
    /// @brief Get an immutable lowest protocol layer to the tcp stream
    /// @return const lowest protocol layer
    const auto &lowestLayer() const { return beast::get_lowest_layer(m_stream); }

    /// @brief method called asynchonously when the source connects to the agent
    /// @param ec an error code
    /// @param endpoint_type unused
    void onConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type)
    {
      if (ec)
      {
        LOG(error) << "Cannot connect to " << m_url.getHost() << ", shutting down";
        LOG(error) << "  Reason: " << ec.category().name() << " " << ec.message();
        return failed(source::make_error_code(ErrorCode::RETRY_REQUEST), "connect");
      }

      if (!m_request)
      {
        m_stream.close();
        LOG(error) << "Connected and no reqiest";
        return failed(source::make_error_code(ErrorCode::RETRY_REQUEST), "connect");
      }

      if (m_handler && m_handler->m_connected)
        m_handler->m_connected(m_identity);

      request();
    }

    /// @brief disconnnect from the agent
    void disconnect()
    {
      beast::error_code ec;

      // Gracefully close the socket
      m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);

      if (m_handler && m_handler->m_disconnected)
        m_handler->m_disconnected(m_identity);

      // Close the connection anyway, but feedback the error.
      m_stream.close();

      // not_connected happens sometimes so don't bother reporting it.
      if (ec && ec != beast::errc::not_connected)
      {
        LOG(error) << "Shutdown error: " << ec.category().name() << " " << ec.message();
      }

      m_request.reset();
    }

  protected:
    beast::tcp_stream m_stream;
  };

}  // namespace mtconnect::source::adapter::agent_adapter
