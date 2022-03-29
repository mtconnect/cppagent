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

using namespace std;
using namespace mtconnect;

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

namespace mtconnect::adapter::agent {
  class Client
  {
  public:
    Client(asio::io_context& ioc, ssl::context& context) : m_context(ioc), m_stream(ioc, context) {}

    ~Client() { close(); }

    void fail(beast::error_code ec, char const* what)
    {
      LOG(error) << what << ": " << ec.message() << "\n";
      m_done = true;
      m_failed = true;
      m_ec = ec;
    }

    void connect(unsigned short port, asio::yield_context yield)
    {
      beast::error_code ec;

      SSL_set_tlsext_host_name(m_stream.native_handle(), "localhost");

      // These objects perform our I/O
      tcp::endpoint server(asio::ip::address_v4::from_string("127.0.0.1"), port);

      // Set the timeout.
      beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));

      // Make the connection on the IP address we get from a lookup
      beast::get_lowest_layer(m_stream).async_connect(server, yield[ec]);
      if (ec)
      {
        return fail(ec, "connect");
      }

      beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));
      m_stream.async_handshake(ssl::stream_base::client, yield[ec]);
      if (ec)
        return fail(ec, "handshake");

      m_connected = true;
    }

    void request(boost::beast::http::verb verb, std::string const& target, std::string const& body,
                 bool close, string const& contentType, asio::yield_context yield)
    {
      beast::error_code ec;

      cout << "request: done: false" << endl;
      m_done = false;
      m_status = -1;
      m_result.clear();

      // Set up an HTTP GET request message
      http::request<http::string_body> req {verb, target, 11};
      req.set(http::field::host, "localhost");
      req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
      req.set(http::field::content_type, contentType);
      if (close)
        req.set(http::field::connection, "close");
      req.body() = body;

      // Set the timeout.
      beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));
      req.prepare_payload();

      // Send the HTTP request to the remote host
      http::async_write(m_stream, req, yield[ec]);
      if (ec)
        return fail(ec, "write");

      m_parser.emplace();

      // Receive the HTTP response
      http::async_read_header(m_stream, m_b, *m_parser, yield[ec]);
      if (ec)
        return fail(ec, "async_read_header");
      m_status = m_parser->get().result_int();

      for (const auto& f : m_parser->get())
        m_fields.insert({string(f.name_string()), string(f.value())});

      auto f = m_parser->get().find(http::field::content_type);
      if (f != m_parser->get().end())
      {
        m_contentType = string(f->value());
        auto i = m_contentType.find(';');
        if (i != string::npos)
        {
          auto b = m_contentType.substr(i + 1);
          m_contentType = m_contentType.substr(0, i);
          auto p = b.find("=");
          if (p != string::npos)
          {
            if (b.substr(0, p) == "boundary")
            {
              m_boundary = b.substr(p + 1);
            }
          }
        }
      }

      m_bodyParser.emplace(std::move(*m_parser));
      if (m_parser->chunked())
      {
        using std::placeholders::_1;
        using std::placeholders::_2;
        using std::placeholders::_3;

        auto header = [this](std::uint64_t size, boost::string_view extensions,
                             boost::system::error_code& ev) {
          // cout << "Reading header" << endl;
          if (ev)
            fail(ev, "Failed in chunked header");

          http::chunk_extensions ce;
          ce.parse(extensions, ev);
          for (auto& c : ce)
            cout << "Ext: " << c.first << ": " << c.second << endl;

          if (ev)
            fail(ev, "Failed in chunked extension parse");
        };
        m_headerHandler = header;
        m_bodyParser->on_chunk_header(m_headerHandler);

        auto body = [this](std::uint64_t remain, boost::string_view body,
                           boost::system::error_code& ev) -> unsigned long {
          // cout << "Reading body" << endl;
          m_count++;
          m_ec = ev;
          if (ev)
            fail(ev, "Failed in chunked body");

          string b(body.cbegin(), body.cend());
          auto le = b.find("\r\n");
          if (le == string::npos)
            return 0;
          auto boundary = b.substr(0, le);

          le += 2;
          auto he = b.find("\r\n\r\n", le);
          if (he == string::npos)
            return 0;
          auto header = b.substr(le, he - le);

          he += 4;
          auto be = b.find("\r\n", he);
          if (be == string::npos)
            return 0;
          auto bd = b.substr(he, be - he);

          m_result = bd;
          m_done = true;
          cout << "Read " << m_count << ": " << m_result << endl;
          return body.size();
        };
        m_chunkHandler = body;
        m_bodyParser->on_chunk_body(m_chunkHandler);
      }
      else
      {
        http::async_read(m_stream, m_b, *m_bodyParser, yield[ec]);
        if (ec)
          return fail(ec, "async_read");

        auto msg = m_bodyParser->get();
        m_result = beast::buffers_to_string(msg.body().data());
      }
      // Write the message to standard out
      // std::cout << res << std::endl;

      m_done = true;
    }

    void readChunk(asio::yield_context yield)
    {
      boost::system::error_code ec;
      http::async_read(m_stream, m_b, *m_bodyParser, yield[ec]);
      if (ec == http::error::end_of_chunk)
        cout << "End of chunk";
      else if (ec)
        fail(ec, "async_read chunk");
    }

    void spawnReadChunk()
    {
      asio::spawn(m_context, std::bind(&Client::readChunk, this, std::placeholders::_1));
    }

    void spawnRequest(boost::beast::http::verb verb, std::string const& target,
                      std::string const& body = "", bool close = false,
                      string const& contentType = "text/plain")
    {
      cout << "spawnRequest: done: false" << endl;
      m_done = false;
      m_count = 0;
      asio::spawn(m_context, std::bind(&Client::request, this, verb, target, body, close, contentType,
                                       std::placeholders::_1));

      while (!m_done && !m_failed && m_context.run_for(20ms) > 0)
        ;
    }

    void close()
    {
      m_done = false;
      auto closeStream = [this](asio::yield_context yield) {
        beast::error_code ec;

        // Gracefully close the socket
        m_stream.async_shutdown(yield[ec]);
        if (ec)
          fail(ec, "shutdown");
        m_done = true;
      };

      asio::spawn(m_context, std::bind(closeStream, std::placeholders::_1));
      while (!m_done && m_context.run_for(100ms) > 0)
        ;
    }

    bool m_clientCert {false};
    bool m_connected {false};
    bool m_failed {false};
    int m_status;
    std::string m_result;
    asio::io_context& m_context;
    bool m_done {false};
    beast::ssl_stream<beast::tcp_stream> m_stream;
    boost::beast::error_code m_ec;
    beast::flat_buffer m_b;
    int m_count {0};
    optional<http::response_parser<http::empty_body>> m_parser;
    optional<http::response_parser<http::dynamic_body>> m_bodyParser;
    string m_boundary;
    map<string, string> m_fields;
    string m_contentType;

    std::function<std::uint64_t(std::uint64_t, boost::string_view, boost::system::error_code&)>
        m_chunkHandler;
    std::function<void(std::uint64_t, boost::string_view, boost::system::error_code&)>
        m_headerHandler;
  };

}

