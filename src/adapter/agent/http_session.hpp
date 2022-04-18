
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

#include "session_impl.hpp"

namespace mtconnect::adapter::agent {

  // HTTP Session
  class HttpSession : public SessionImpl<HttpSession>
  {
  public:
    using super = SessionImpl<HttpSession>;

    HttpSession(boost::asio::io_context::strand &ioc, const Url &url, int count, int heartbeat)
      : super(ioc, url, count, heartbeat), m_stream(ioc.context())
    {}

    ~HttpSession() override {}

    shared_ptr<HttpSession> getptr()
    {
      return static_pointer_cast<HttpSession>(shared_from_this());
    }

    auto &stream() { return m_stream; }
    auto &lowestLayer() { return beast::get_lowest_layer(m_stream); }
    const auto &lowestLayer() const { return beast::get_lowest_layer(m_stream); }

    void onConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type)
    {
      if (ec)
        return failed(ec, "connect");

      if (m_handler && m_handler->m_connected)
        m_handler->m_connected(m_identity);

      request();
    }

    void disconnect()
    {
      beast::error_code ec;

      // Gracefully close the socket
      m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);

      if (m_handler && m_handler->m_disconnected)
        m_handler->m_disconnected(m_identity);

      // not_connected happens sometimes so don't bother reporting it.
      if (ec && ec != beast::errc::not_connected)
        return failed(ec, "shutdown");

      // If we get here then the connection is closed gracefully
    }

  protected:
    beast::tcp_stream m_stream;
  };

}  // namespace mtconnect::adapter::agent
