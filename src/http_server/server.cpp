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

#include "server.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/tokenizer.hpp>

#include <thread>

#include "logging.hpp"
#include "session_impl.hpp"

namespace mtconnect
{
  namespace http_server
  {
    namespace beast = boost::beast;  // from <boost/beast.hpp>
    namespace http = beast::http;    // from <boost/beast/http.hpp>
    namespace net = boost::asio;     // from <boost/asio.hpp>
    namespace asio = boost::asio;
    namespace ip = boost::asio::ip;
    using tcp = boost::asio::ip::tcp;
    namespace algo = boost::algorithm;

    using namespace std;
    using boost::placeholders::_1;
    using boost::placeholders::_2;

    void Server::start()
    {
      try
      {
        m_run = true;
        listen();
      }
      catch (exception &e)
      {
        LOG(fatal) << "Cannot start server: " << e.what();
        std::exit(1);
      }
    }

    // Listen for an HTTP server connection
    void Server::listen()
    {
      NAMED_SCOPE("Server::listen");

      beast::error_code ec;

      //            if(enableSSL) {
      //                // The SSL context is required, and holds certificates
      //                ssl::context ctx{ssl::context::tlsv12};
      //
      //                // This holds the self-signed certificate used by the server
      //                load_server_certificate(ctx);
      //            }

      // Blocking call to listen for a connection
      tcp::endpoint ep(m_address, m_port);
      m_acceptor.open(ep.protocol(), ec);
      if (ec)
      {
        fail(ec, "Cannot open server socket");
        return;
      }
      m_acceptor.set_option(boost::asio::socket_base::reuse_address(true), ec);
      if (ec)
      {
        fail(ec, "Cannot set reuse address");
        return;
      }
      m_acceptor.bind(ep, ec);
      if (ec)
      {
        fail(ec, "Cannot bind to server address");
        return;
      }
      if (m_port == 0)
      {
        m_port = m_acceptor.local_endpoint().port();
      }

      m_acceptor.listen(net::socket_base::max_listen_connections, ec);
      if (ec)
      {
        fail(ec, "Cannot set listen queue length");
        return;
      }

      m_listening = true;
      m_acceptor.async_accept(net::make_strand(m_context),
                              beast::bind_front_handler(&Server::accept, this));
    }

    bool Server::allowPutFrom(const std::string &host)
    {
      NAMED_SCOPE("Server::allowPutFrom");

      // Resolve the host to an ip address to verify remote addr
      beast::error_code ec;
      ip::tcp::resolver resolve(m_context);
      auto results = resolve.resolve(host, "0", ec);
      if (ec)
      {
        LOG(error) << "Cannot resolve address: " << host;
        LOG(error) << ec.category().message(ec.value()) << ": " << ec.message();
        return false;
      }

      // Add the results to the set of allowed hosts
      for (auto &addr : results)
      {
        m_allowPutsFrom.insert(addr.endpoint().address());
      }
      m_allowPuts = true;

      return true;
    }

    void Server::accept(beast::error_code ec, tcp::socket socket)
    {
      NAMED_SCOPE("Server::accept");

      if (ec)
      {
        LOG(error) << ec.category().message(ec.value()) << ": " << ec.message();

        fail(ec, "Accept failed");
      }
      else
      {
        auto session = make_shared<SessionImpl>(
            socket, m_fields,
            [this](SessionPtr session, RequestPtr request) {
              dispatch(session, request);
              return true;
            },
            m_errorFunction);
        if (!m_allowPutsFrom.empty())
          session->allowPutsFrom(m_allowPutsFrom);
        else if (m_allowPuts)
          session->allowPuts();
        session->run();
        m_acceptor.async_accept(net::make_strand(m_context),
                                beast::bind_front_handler(&Server::accept, this));
      }
    }
    
    //------------------------------------------------------------------------------

    // Report a failure
    void Server::fail(beast::error_code ec, char const *what)
    {
      LOG(error) << " error: " << ec.message();
    }

  }  // namespace http_server
}  // namespace mtconnect
