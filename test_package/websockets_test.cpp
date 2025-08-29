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

// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include <boost/algorithm/string.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket.hpp>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "mtconnect/logging.hpp"
#include "mtconnect/sink/rest_sink/server.hpp"
#include "test_utilities.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::sink::rest_sink;

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;
using tcp = boost::asio::ip::tcp;
namespace websocket = beast::websocket;

// main
int main(int argc, char* argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class Client
{
public:
  Client(asio::io_context& ioc) : m_context(ioc), m_stream(ioc) {}

  ~Client() { close(); }

  void fail(beast::error_code ec, char const* what)
  {
    LOG(error) << what << ": " << ec.message() << "\n";
    m_done = true;
    m_ec = ec;
  }

  void connect(unsigned short port, asio::yield_context yield)
  {
    beast::error_code ec;

    // These objects perform our I/O
    tcp::endpoint server(asio::ip::address_v4::from_string("127.0.0.1"), port);

    // Make the connection on the IP address we get from a lookup
    beast::get_lowest_layer(m_stream).async_connect(server, yield[ec]);

    if (ec)
    {
      return fail(ec, "connect");
    }

    m_stream.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

    m_stream.set_option(websocket::stream_base::decorator([](websocket::request_type& req) {
      req.set(http::field::user_agent,
              std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client");
    }));

    string host = "127.0.0.1:" + std::to_string(port);
    m_stream.async_handshake(host, "/", yield[ec]);

    if (ec)
    {
      return fail(ec, "connect");
    }

    m_connected = true;

    m_stream.async_read(m_buffer, beast::bind_front_handler(&Client::onRead, this));
  }

  void onRead(beast::error_code ec, std::size_t bytes_transferred)
  {
    m_result = beast::buffers_to_string(m_buffer.data());
    m_buffer.consume(m_buffer.size());

    m_done = true;
  }

  void request(const string& payload, asio::yield_context yield)
  {
    cout << "spawnRequest: done: false" << endl;
    m_done = false;
    beast::error_code ec;

    m_stream.async_write(asio::buffer(payload), yield[ec]);

    waitFor(2s, [this]() { return m_done; });
  }

  template <typename Rep, typename Period>
  bool waitFor(const chrono::duration<Rep, Period>& time, function<bool()> pred)
  {
    boost::asio::steady_timer timer(m_context);
    timer.expires_from_now(time);
    bool timeout = false;
    timer.async_wait([&timeout](boost::system::error_code ec) {
      if (!ec)
      {
        timeout = true;
      }
    });

    while (!timeout && !pred())
    {
      m_context.run_for(500ms);
    }
    timer.cancel();

    return pred();
  }

  void close()
  {
    beast::error_code ec;

    // Gracefully close the socket
    m_stream.next_layer().shutdown(tcp::socket::shutdown_both, ec);
  }

  bool m_connected {false};
  int m_status;
  std::string m_result;
  asio::io_context& m_context;
  bool m_done {false};
  websocket::stream<tcp::socket> m_stream;
  beast::flat_buffer m_buffer;
  boost::beast::error_code m_ec;
  beast::flat_buffer m_b;
  int m_count {0};
};

class WebsocketsTest : public testing::Test
{
protected:
  void SetUp() override { createServer({}); }

  void createServer(const ConfigOptions& options)
  {
    using namespace mtconnect::configuration;
    ConfigOptions opts {{Port, 0}, {ServerIp, "127.0.0.1"s}};
    opts.merge(ConfigOptions(options));
    m_server = make_unique<Server>(m_context, opts);
    m_server->setErrorFunction([](SessionPtr session, const RestError& error) {
      ResponsePtr resp = std::make_unique<Response>(error.getStatus(), error.what(), "plain/text");
      if (error.getRequestId())
        resp->m_requestId = error.getRequestId();

      session->writeFailureResponse(std::move(resp));
    });
  }

  void start()
  {
    m_server->start();
    while (!m_server->isListening())
      m_context.run_one();
    m_client = make_unique<Client>(m_context);
  }

  void startClient()
  {
    m_client->m_connected = false;
    asio::spawn(m_context,
                std::bind(&Client::connect, m_client.get(),
                          static_cast<unsigned short>(m_server->getPort()), std::placeholders::_1));

    m_client->waitFor(1s, [this]() { return m_client->m_connected; });
  }

  void TearDown() override
  {
    m_server.reset();
    m_client.reset();
  }

  asio::io_context m_context;
  unique_ptr<Server> m_server;
  unique_ptr<Client> m_client;
};

TEST_F(WebsocketsTest, should_connect_to_server)
{
  start();
  startClient();

  ASSERT_TRUE(m_client->m_connected);
}

TEST_F(WebsocketsTest, should_make_simple_request)
{
  weak_ptr<Session> savedSession;

  auto probe = [&](SessionPtr session, RequestPtr request) -> bool {
    savedSession = session;
    ResponsePtr resp = make_unique<Response>(status::ok);
    resp->m_body = "All Devices for "s + *request->m_requestId;
    resp->m_requestId = request->m_requestId;
    session->writeResponse(std::move(resp), []() { cout << "Written" << endl; });
    return true;
  };

  m_server->addRouting({boost::beast::http::verb::get, "/probe", probe}).command("probe");
  m_server->addCommands();

  start();
  startClient();

  asio::spawn(m_context, std::bind(&Client::request, m_client.get(),
                                   "{\"id\":\"1\",\"request\":\"probe\"}"s, std::placeholders::_1));

  m_client->waitFor(2s, [this]() { return m_client->m_done; });

  ASSERT_TRUE(m_client->m_done);
  ASSERT_EQ("All Devices for 1", m_client->m_result);
}

TEST_F(WebsocketsTest, should_return_error_when_there_is_no_id)
{
  weak_ptr<Session> savedSession;

  auto probe = [&](SessionPtr session, RequestPtr request) -> bool {
    savedSession = session;
    ResponsePtr resp = make_unique<Response>(status::ok);
    resp->m_body = "All Devices for "s + *request->m_requestId;
    resp->m_requestId = request->m_requestId;
    session->writeResponse(std::move(resp), []() { cout << "Written" << endl; });
    return true;
  };

  m_server->addRouting({boost::beast::http::verb::get, "/probe", probe}).command("probe");
  m_server->addCommands();

  start();
  startClient();

  asio::spawn(m_context, std::bind(&Client::request, m_client.get(), "{\"request\":\"probe\"}"s,
                                   std::placeholders::_1));

  m_client->waitFor(2s, [this]() { return m_client->m_done; });

  ASSERT_TRUE(m_client->m_done);
  ASSERT_EQ("InvalidParameterValue: No id given", m_client->m_result);
}

TEST_F(WebsocketsTest, should_return_error_when_there_is_no_request)
{
  weak_ptr<Session> savedSession;

  auto probe = [&](SessionPtr session, RequestPtr request) -> bool {
    savedSession = session;
    ResponsePtr resp = make_unique<Response>(status::ok);
    resp->m_body = "All Devices for "s + *request->m_requestId;
    resp->m_requestId = request->m_requestId;
    session->writeResponse(std::move(resp), []() { cout << "Written" << endl; });
    return true;
  };

  m_server->addRouting({boost::beast::http::verb::get, "/probe", probe}).command("probe");
  m_server->addCommands();

  start();
  startClient();

  asio::spawn(m_context,
              std::bind(&Client::request, m_client.get(), "{\"id\": 3}"s, std::placeholders::_1));

  m_client->waitFor(2s, [this]() { return m_client->m_done; });

  ASSERT_TRUE(m_client->m_done);
  ASSERT_EQ("InvalidParameterValue: No request given", m_client->m_result);
}

TEST_F(WebsocketsTest, should_return_error_when_a_parameter_is_invalid)
{
  weak_ptr<Session> savedSession;

  auto sample = [&](SessionPtr session, RequestPtr request) -> bool {
    savedSession = session;
    ResponsePtr resp = make_unique<Response>(status::ok);
    resp->m_body = "All Devices for "s + *request->m_requestId;
    resp->m_requestId = request->m_requestId;
    session->writeResponse(std::move(resp), []() { cout << "Written" << endl; });
    return true;
  };

  m_server->addRouting({boost::beast::http::verb::get, "/sample?interval={integer}", sample})
      .command("sample");
  m_server->addCommands();

  start();
  startClient();

  asio::spawn(m_context,
              std::bind(&Client::request, m_client.get(),
                        "{\"id\": 3, \"request\": \"sample\", \"interval\": 99999999999}"s,
                        std::placeholders::_1));

  m_client->waitFor(2s, [this]() { return m_client->m_done; });

  ASSERT_TRUE(m_client->m_done);
  ASSERT_EQ("InvalidParameterValue: query parameter 'interval': invalid type, expected int32",
            m_client->m_result);
}

TEST_F(WebsocketsTest, should_return_error_when_bad_json_is_sent)
{
  weak_ptr<Session> savedSession;

  auto sample = [&](SessionPtr session, RequestPtr request) -> bool {
    savedSession = session;
    ResponsePtr resp = make_unique<Response>(status::ok);
    resp->m_body = "All Devices for "s + *request->m_requestId;
    resp->m_requestId = request->m_requestId;
    session->writeResponse(std::move(resp), []() { cout << "Written" << endl; });
    return true;
  };

  m_server->addRouting({boost::beast::http::verb::get, "/sample?interval={integer}", sample})
      .command("sample");
  m_server->addCommands();

  start();
  startClient();

  asio::spawn(m_context,
              std::bind(&Client::request, m_client.get(), "!}}"s, std::placeholders::_1));

  m_client->waitFor(2s, [this]() { return m_client->m_done; });

  ASSERT_TRUE(m_client->m_done);
  ASSERT_EQ("InvalidRequest: Websocket Read Error(offset (0)): Invalid value.", m_client->m_result);
}

TEST_F(WebsocketsTest, should_return_multiple_errors_when_parameters_are_invalid)
{
  weak_ptr<Session> savedSession;

  auto sample = [&](SessionPtr session, RequestPtr request) -> bool {
    savedSession = session;
    ResponsePtr resp = make_unique<Response>(status::ok);
    resp->m_body = "All Devices for "s + *request->m_requestId;
    resp->m_requestId = request->m_requestId;
    session->writeResponse(std::move(resp), []() { cout << "Written" << endl; });
    return true;
  };

  m_server
      ->addRouting({boost::beast::http::verb::get,
                    "/sample?interval={integer}&to={unsigned_integer}", sample})
      .command("sample");
  m_server->addCommands();

  start();
  startClient();

  asio::spawn(
      m_context,
      std::bind(&Client::request, m_client.get(),
                R"DOC({"id": 3, "request": "sample", "interval": 99999999999,"to": -1 })DOC",
                std::placeholders::_1));

  m_client->waitFor(2s, [this]() { return m_client->m_done; });

  ASSERT_TRUE(m_client->m_done);
  ASSERT_EQ(
      "InvalidParameterValue: query parameter 'interval': invalid type, expected int32, "
      "InvalidParameterValue: query parameter 'to': invalid type, expected uint64",
      m_client->m_result);
}

TEST_F(WebsocketsTest, should_return_error_for_an_invalid_command)
{
  weak_ptr<Session> savedSession;

  auto probe = [&](SessionPtr session, RequestPtr request) -> bool {
    savedSession = session;
    ResponsePtr resp = make_unique<Response>(status::ok);
    resp->m_body = "All Devices for "s + *request->m_requestId;
    resp->m_requestId = request->m_requestId;
    session->writeResponse(std::move(resp), []() { cout << "Written" << endl; });
    return true;
  };

  m_server->addRouting({boost::beast::http::verb::get, "/probe", probe}).command("probe");
  m_server->addCommands();

  start();
  startClient();

  asio::spawn(m_context,
              std::bind(&Client::request, m_client.get(), "{\"id\":\"1\",\"request\":\"sample\"}"s,
                        std::placeholders::_1));

  m_client->waitFor(2s, [this]() { return m_client->m_done; });

  ASSERT_TRUE(m_client->m_done);
  ASSERT_EQ("InvalidURI: 0.0.0.0: Command failed: sample", m_client->m_result);
}
