
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

#include <boost/algorithm/string.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>

#include "session.hpp"

namespace mtconnect::adapter::agent {
  using namespace std;
  namespace asio = boost::asio;
  namespace beast = boost::beast;
  namespace http = boost::beast::http;
  namespace ssl = boost::asio::ssl;
  using tcp = boost::asio::ip::tcp;

  // Report a failure
  void fail(beast::error_code ec, char const *what)
  {
    LOG(error) << what << ": " << ec.message() << "\n";
  }

  template <class Derived>
  class SessionImpl : public Session
  {
  public:
    Derived &derived() { return static_cast<Derived &>(*this); }
    const Derived &derived() const { return static_cast<const Derived &>(*this); }

    // Objects are constructed with a strand to
    // ensure that handlers do not execute concurrently.
    SessionImpl(boost::asio::io_context::strand &strand)
      : m_resolver(strand.context()), m_strand(strand)
    {}

    virtual ~SessionImpl() { stop(); }

    bool isOpen() const override { return derived().lowestLayer().socket().is_open(); }

    void stop() override
    {
      if (isOpen())
        derived().lowestLayer().close();
    }

    // Start the asynchronous operation
    void connect(const Url &url, Connected cb) override
    {
      m_url = url;
      m_connected = cb;
      
      // If the address is an IP address, we do not need to resolve.
      if (holds_alternative<asio::ip::address>(m_url.m_host))
      {
        asio::ip::tcp::endpoint ep(get<asio::ip::address>(m_url.m_host), m_url.m_port.value_or(80));
                
        // Create the results type and call on resolve directly.
        using results_type = tcp::resolver::results_type;
        auto results = results_type::create(ep,
                                            m_url.getHost(),
                                            m_url.getService());
        
        beast::error_code ec;
        onResolve(ec, results);
      }
      else
      {
        derived().lowestLayer().expires_after(std::chrono::seconds(30));

        // Do an async resolution of the address.
        m_resolver.async_resolve(get<string>(m_url.m_host), m_url.getService(),
            asio::bind_executor(
                m_strand, beast::bind_front_handler(&SessionImpl::onResolve, derived().getptr())));
      }
    }

    void onResolve(beast::error_code ec, tcp::resolver::results_type results)
    {
      if (ec)
      {
        // If we can't resolve, then shut down the adapter
        return fail(ec, "resolve");
      }

      // Set a timeout on the operation
      derived().lowestLayer().expires_after(std::chrono::seconds(30));

      // Make the connection on the IP address we get from a lookup
      derived().lowestLayer().async_connect(
          results, asio::bind_executor(m_strand, beast::bind_front_handler(&Derived::onConnect,
                                                                           derived().getptr())));
    }
    
    void connected(beast::error_code ec)
    {
      m_connected(ec);
    }
    
    bool makeRequest(const std::string &suffix,
                     const UrlQuery &query,
                     bool stream,
                     Result cb) override
    {
      m_result = cb;
      string target = m_url.m_path + suffix;
      auto uq = m_url.m_query;
      if (!query.empty())
        uq.merge(query);
      if (!uq.empty())
        target += ("?" + uq.join());
      m_stream = stream;
      //
      auto request = [this, target](beast::error_code ec) {
        // Set up an HTTP GET request message
        m_req.version(11);
        m_req.method(http::verb::get);
        m_req.target(target);
        m_req.set(http::field::host, m_url.getHost());
        m_req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        
        http::async_write(derived().stream(), m_req,
                          beast::bind_front_handler(&SessionImpl::onWrite,
                                                    derived().getptr()));
      };
      
      // Check if we are discussected.
      if (!derived().lowestLayer().socket().is_open())
      {
        // Try to reconnect executiong the request on successful
        // connection. ÷
        connect(m_url, request);
        return false;
      }
      else
      {
        beast::error_code ec;
        request(ec);
        return true;
      }
    }

    void onWrite(beast::error_code ec, std::size_t bytes_transferred)
    {
      boost::ignore_unused(bytes_transferred);

      if (ec)
      {
        m_result(ec, "");
        return fail(ec, "write");
      }
      
      derived().lowestLayer().expires_after(std::chrono::seconds(30));

      // Receive the HTTP response
      http::async_read(derived().stream(), m_buffer, m_res,
                       asio::bind_executor(m_strand, beast::bind_front_handler(
                                                         &Derived::onRead, derived().getptr())));
    }

    void onRead(beast::error_code ec, std::size_t bytes_transferred)
    {
      boost::ignore_unused(bytes_transferred);

      if (ec)
      {
        m_result(ec, "");
        return fail(ec, "write");
      }
    
      derived().lowestLayer().expires_after(std::chrono::seconds(30));

      if (!derived().lowestLayer().socket().is_open())
        derived().disconnect();
      
      m_result(ec, m_res.body());
    }

  protected:
    tcp::resolver m_resolver;
    beast::flat_buffer m_buffer;  // (Must persist between reads)
    http::request<http::empty_body> m_req;
    http::response<http::string_body> m_res;
    asio::io_context::strand m_strand;
    Url m_url;
    Connected m_connected;
    Result m_result;
    bool m_stream = false;
  };

}  // namespace mtconnect::adapter::agent
