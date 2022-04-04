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

#include "agent_adapter.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "logging.hpp"
#include "configuration/config_options.hpp"

using namespace std;
using namespace mtconnect;

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

namespace mtconnect::adapter::agent {

//------------------------------------------------------------------------------

  // Report a failure
  void
  fail(beast::error_code ec, char const* what)
  {
    LOG(error) << what << ": " << ec.message() << "\n";
  }
  
  class Session : public std::enable_shared_from_this<Session>
  {
  public:
    
  };
  
  template<class Derived>
  class SessionImpl : public Session
  {
  public:
    Derived &derived() { return static_cast<Derived &>(*this); }
    
      // Objects are constructed with a strand to
      // ensure that handlers do not execute concurrently.
      explicit
    SessionImpl(asio::any_io_executor& ioc)
          : m_resolver(asio::make_strand(ioc))
      {
      }
    virtual ~SessionImpl() {}

      // Start the asynchronous operation
      void run(
          char const* host,
          char const* port,
          char const* target,
          int version)
      {
          // Set up an HTTP GET request message
          m_req.version(version);
          m_req.method(http::verb::get);
          m_req.target(target);
          m_req.set(http::field::host, host);
          m_req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

          // Look up the domain name
          m_resolver.async_resolve(
              host,
              port,
              beast::bind_front_handler(
                  &SessionImpl::onResolve,
                                        derived().getptr()));
      }

      void onResolve(
          beast::error_code ec,
          tcp::resolver::results_type results)
      {
          if(ec)
              return fail(ec, "resolve");

          // Set a timeout on the operation
        derived().lowestLayer().expires_after(std::chrono::seconds(30));

          // Make the connection on the IP address we get from a lookup
        derived().lowestLayer().async_connect(
              results,
              beast::bind_front_handler(
                  &Derived::onConnect,
                  derived().getptr()));
      }

      void onWrite(
          beast::error_code ec,
          std::size_t bytes_transferred)
      {
          boost::ignore_unused(bytes_transferred);

          if(ec)
              return fail(ec, "write");
          
          // Receive the HTTP response
          http::async_read(derived().stream(), m_buffer, m_res,
              beast::bind_front_handler(
                  &Derived::onRead,
                                        derived().getptr()));
      }

    void onRead(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return fail(ec, "read");

        // Write the message to standard out
        std::cout << m_res << std::endl;
      
      derived().completeRead();
    }

  protected:
    tcp::resolver m_resolver;
    beast::flat_buffer m_buffer; // (Must persist between reads)
    http::request<http::empty_body> m_req;
    http::response<http::string_body> m_res;
  };
  
  // HTTP Session
  class HttpSession : public SessionImpl<HttpSession>
  {
  public:
    using super = SessionImpl<HttpSession>;
    
    HttpSession(asio::any_io_executor& ioc)
    : super(ioc), m_stream(ioc)
    {
    }
    
    ~HttpSession() override {}

    shared_ptr<HttpSession> getptr() { return static_pointer_cast<HttpSession>(shared_from_this()); }
    
    auto &stream() { return m_stream; }
    auto &lowestLayer() { return beast::get_lowest_layer(m_stream); }
    
    void
    onConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type)
    {
        if(ec)
            return fail(ec, "connect");

        // Set a timeout on the operation
      m_stream.expires_after(std::chrono::seconds(30));

        // Send the HTTP request to the remote host
        http::async_write(m_stream, m_req,
            beast::bind_front_handler(
                &super::onWrite,
                                      getptr()));
    }

    void completeRead()
    {
      beast::error_code ec;
      
      // Gracefully close the socket
      m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);

      // not_connected happens sometimes so don't bother reporting it.
      if(ec && ec != beast::errc::not_connected)
          return fail(ec, "shutdown");

      // If we get here then the connection is closed gracefully
    }

    
  protected:
    beast::tcp_stream m_stream;
  };

  // HTTPS Session
  class HttpsSession : public SessionImpl<HttpsSession>
  {
  public:
    using super = SessionImpl<HttpsSession>;
    
    explicit
    HttpsSession(asio::any_io_executor ex,
            ssl::context& ctx)
    : super(ex), m_stream(ex, ctx)
    {
    }
    ~HttpsSession() override {}

    auto &stream() { return m_stream; }
    auto &lowestLayer() { return beast::get_lowest_layer(m_stream); }

    shared_ptr<HttpsSession> getptr() { return std::static_pointer_cast<HttpsSession>(shared_from_this()); }

    // Start the asynchronous operation
    void run(
        char const* host,
        char const* port,
        char const* target,
        int version)
    {
      if(! SSL_set_tlsext_host_name(m_stream.native_handle(), host))
      {
        beast::error_code ec{static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category()};
        std::cerr << ec.message() << "\n";
        return;
      }
      
      super::run(host, port, target, version);
    }
    
    void onConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type)
    {
      if(ec)
        return fail(ec, "connect");
      
      // Set a timeout on the operation
      beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));
      
      // Perform the SSL handshake
      m_stream.async_handshake(
                              ssl::stream_base::client,
                              beast::bind_front_handler(
                                                        &HttpsSession::onHandshake,
                                                        getptr()));
    }
    
    void onHandshake(beast::error_code ec)
    {
      if(ec)
        return fail(ec, "handshake");
      
      // Set a timeout on the operation
      beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));
      
      // Send the HTTP request to the remote host
      http::async_write(m_stream, m_req,
                        beast::bind_front_handler(
                                                  &HttpsSession::onWrite,
                                                  getptr()));
    }
        
    void completeRead()
    {
      // Set a timeout on the operation
      beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));
      
      // Gracefully close the stream
      m_stream.async_shutdown(
                             beast::bind_front_handler(
                                                       &HttpsSession::onShutdown,
                                                       getptr()));
    }
    
    void onShutdown(beast::error_code ec)
    {
      if(ec == asio::error::eof)
      {
        // Rationale:
        // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
        ec = {};
      }
      if(ec)
        return fail(ec, "shutdown");
      
      // If we get here then the connection is closed gracefully
    }
    
  protected:
    beast::ssl_stream<beast::tcp_stream> m_stream;
  };
  
  AgentAdapter::AgentAdapter(boost::asio::io_context &io,
               pipeline::PipelineContextPtr context,
               const ConfigOptions &options,
               const boost::property_tree::ptree &block)
    : Adapter("AgentAdapter", io, options), m_pipeline(context, Source::m_strand)
  {
    GetOptions(block, m_options, options);
    AddOptions(block, m_options,
               {{configuration::UUID, string()},
                {configuration::Manufacturer, string()},
                {configuration::Station, string()},
                {configuration::Url, string()}});

    AddDefaultedOptions(block, m_options,
                        {{configuration::Host, "localhost"s},
                         {configuration::Port, 5000},
                         {configuration::AutoAvailable, false},
                         {configuration::RealTime, false},
                         {configuration::RelativeTime, false}});
    
    auto urlOpt = GetOption<std::string>(m_options, configuration::Url);
    if (urlOpt)
    {
      m_url = Url::parse(*urlOpt);
    }
    else
    {
      m_url.m_protocol = "http";
      m_url.m_host = *GetOption<string>(m_options, configuration::Host);
      m_url.m_port = GetOption<int>(m_options, configuration::Port);
      m_url.m_path = GetOption<string>(m_options, configuration::Device).value_or("/");
    }
    
    
  }

  AgentAdapter::~AgentAdapter()
  {
    
  }
  
  bool AgentAdapter::start()
  {
#if 0
  //------------------------------------------------------------------------------
  
  int main(int argc, char** argv)
  {
    // Check command line arguments.
    if(argc != 4 && argc != 5)
    {
      std::cerr <<
      "Usage: http-client-async-ssl <host> <port> <target> [<HTTP version: 1.0 or 1.1(default)>]\n" <<
      "Example:\n" <<
      "    http-client-async-ssl www.example.com 443 /\n" <<
      "    http-client-async-ssl www.example.com 443 / 1.0\n";
      return EXIT_FAILURE;
    }
    auto const host = argv[1];
    auto const port = argv[2];
    auto const target = argv[3];
    int version = argc == 5 && !std::strcmp("1.0", argv[4]) ? 10 : 11;
    
    // The io_context is required for all I/O
    asio::io_context ioc;
    
    // The SSL context is required, and holds certificates
    ssl::context ctx{ssl::context::tlsv12_client};
    
    // This holds the root certificate used for verification
    //load_root_certificates(ctx);
    
    // Verify the remote server's certificate
    ctx.set_verify_mode(ssl::verify_peer);
    
    // Launch the asynchronous operation
    // The session is constructed with a strand to
    // ensure that handlers do not execute concurrently.
    std::make_shared<Session>(
                              asio::make_strand(ioc),
                              ctx
                              )->run(host, port, target, version);
    
    // Run the I/O service. The call will return when
    // the get operation is complete.
    ioc.run();
    
    return EXIT_SUCCESS;
  }
#endif

    
    return true;
  }
  
  void stop()
  {
    
  }

}

