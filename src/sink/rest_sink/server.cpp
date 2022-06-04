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

#include "server.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/tokenizer.hpp>

#include <thread>

#include "logging.hpp"
#include "session_impl.hpp"

namespace mtconnect::sink::rest_sink {
  namespace beast = boost::beast;  // from <boost/beast.hpp>
  namespace http = beast::http;    // from <boost/beast/http.hpp>
  namespace net = boost::asio;     // from <boost/asio.hpp>
  namespace asio = boost::asio;
  namespace ip = boost::asio::ip;
  using tcp = boost::asio::ip::tcp;
  namespace algo = boost::algorithm;
  namespace ssl = boost::asio::ssl;

  using namespace std;
  using boost::placeholders::_1;
  using boost::placeholders::_2;

  void Server::loadTlsCertificate()
  {
    if (HasOption(m_options, configuration::TlsCertificateChain) &&
        HasOption(m_options, configuration::TlsPrivateKey) &&
        HasOption(m_options, configuration::TlsDHKey))
    {
      LOG(info) << "Initializing TLS support";
      if (HasOption(m_options, configuration::TlsCertificatePassword))
      {
        m_sslContext.set_password_callback(
            [this](size_t, boost::asio::ssl::context_base::password_purpose) -> string {
              return *GetOption<string>(m_options, configuration::TlsCertificatePassword);
            });
      }

      m_sslContext.set_options(ssl::context::default_workarounds | asio::ssl::context::no_sslv2 |
                               asio::ssl::context::single_dh_use);
      m_sslContext.use_certificate_chain_file(
          *GetOption<string>(m_options, configuration::TlsCertificateChain));
      m_sslContext.use_private_key_file(*GetOption<string>(m_options, configuration::TlsPrivateKey),
                                        asio::ssl::context::file_format::pem);
      m_sslContext.use_tmp_dh_file(*GetOption<string>(m_options, configuration::TlsDHKey));

      m_tlsEnabled = true;

      m_tlsOnly = IsOptionSet(m_options, configuration::TlsOnly);

      if (IsOptionSet(m_options, configuration::TlsVerifyClientCertificate))
      {
        LOG(info) << "Will only accept client connections with valid certificates";

        m_sslContext.set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);
        if (HasOption(m_options, configuration::TlsClientCAs))
        {
          LOG(info) << "Adding Client Certificates.";
          m_sslContext.load_verify_file(*GetOption<string>(m_options, configuration::TlsClientCAs));
        }
      }
    }
  }

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
    else if (m_run)
    {
      auto dispatcher = [this](SessionPtr session, RequestPtr request) {
        if (!m_run)
          return false;

        if (m_lastSession)
          m_lastSession(session);
        dispatch(session, request);
        return true;
      };
      if (m_tlsEnabled)
      {
        auto dectector =
            make_shared<TlsDector>(move(socket), m_sslContext, m_tlsOnly, m_allowPuts,
                                   m_allowPutsFrom, m_fields, dispatcher, m_errorFunction);

        dectector->run();
      }
      else
      {
        boost::beast::flat_buffer buffer;
        boost::beast::tcp_stream stream(move(socket));
        auto session = make_shared<HttpSession>(move(stream), move(buffer), m_fields, dispatcher,
                                                m_errorFunction);

        if (!m_allowPutsFrom.empty())
          session->allowPutsFrom(m_allowPutsFrom);
        else if (m_allowPuts)
          session->allowPuts();

        session->run();
      }
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

}  // namespace mtconnect::sink::rest_sink
