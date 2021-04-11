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

// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/spawn.hpp>

#include "http_server/server.hpp"
#include "logging.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using namespace std;
using namespace mtconnect;
using namespace mtconnect::http_server;

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;
using tcp = boost::asio::ip::tcp;

class Client
{
public:
  Client(asio::io_context& ioc)
  : m_context(ioc), m_stream(ioc)
  {
  }
  
  ~Client() { close(); }
  
  void fail(beast::error_code ec, char const* what)
  {
    LOG(error) << what << ": " << ec.message() << "\n";
    m_done = true;
    ASSERT_TRUE(false);
  }
  
  void connect(unsigned short port, asio::yield_context yield)
  {
    beast::error_code ec;

    // These objects perform our I/O
    tcp::endpoint server(asio::ip::address_v4::from_string("127.0.0.1"), port);
        
    // Set the timeout.
    m_stream.expires_after(std::chrono::seconds(30));
    
    // Make the connection on the IP address we get from a lookup
    m_stream.async_connect(server, yield[ec]);
    if(ec)
    {
      return fail(ec, "connect");
    }
    m_connected = true;
  }

  void request(boost::beast::http::verb verb,
               std::string const& target,
               std::string const& body,
               asio::yield_context yield)
  {
    beast::error_code ec;
    
    m_done = false;
    m_status = -1;
    m_result.clear();
    
    
    // Set up an HTTP GET request message
    http::request<http::string_body> req{verb, target, 11};
    req.set(http::field::host, "localhost");
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    
    // Set the timeout.
    m_stream.expires_after(std::chrono::seconds(30));
    
    // Send the HTTP request to the remote host
    http::async_write(m_stream, req, yield[ec]);
    if(ec)
      return fail(ec, "write");
    
    // This buffer is used for reading and must be persisted
    beast::flat_buffer b;
    
    // Declare a container to hold the response
    http::response<http::string_body> res;
    
    // Receive the HTTP response
    http::async_read(m_stream, b, res, yield[ec]);
    if(ec)
      return fail(ec, "read");
    
    m_result = res.body();
    m_status = res.result_int();
    
    // Write the message to standard out
    std::cout << res << std::endl;
        
    m_done = true;    
  }
  
  void spawnRequest(boost::beast::http::verb verb,
                    std::string const& target,
                    std::string const &body = "")
  {
    m_done = false;
    asio::spawn(m_context,
                std::bind(&Client::request,
                          this, verb, target, body,
                          std::placeholders::_1));
    
    while (!m_done && m_context.run_for(100ms) > 0)
      ;
  }

  void close()
  {
    beast::error_code ec;

    // Gracefully close the socket
    m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);
  }
  

  bool m_connected{false};
  int m_status;
  std::string m_result;
  asio::io_context &m_context;
  bool m_done{false};
  beast::tcp_stream m_stream;
};

class HttpServerTest : public testing::Test
{
 protected:
  void SetUp() override
  {
    m_server = make_unique<Server>(m_context, 0, "127.0.0.1");
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
                std::bind(&Client::connect,
                          m_client.get(),
                          static_cast<unsigned short>(m_server->getPort()),
                          std::placeholders::_1));

    while (!m_client->m_connected)
      m_context.run_one();
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

TEST_F(HttpServerTest, simple_request_response)
{
  weak_ptr<Session> session;
  
  auto probe = [&](RequestPtr request) -> bool {
    session = request->m_session;
    Response resp(status::ok);
    if (request->m_parameters.count("device") > 0)
      resp.m_body = string("Device given as: ") + get<string>(request->m_parameters.find("device")->second);
    else
      resp.m_body = "All Devices";
    request->m_session->writeResponse(resp, [](){
      cout << "Written" << endl;
    });
    return true;
  };
  
  m_server->addRouting(Routing{boost::beast::http::verb::get, "/probe", probe});
  m_server->addRouting({boost::beast::http::verb::get, "/{device}/probe", probe});
  
  start();
  startClient();

  m_client->spawnRequest(http::verb::get, "/probe");
    
  EXPECT_EQ("All Devices", m_client->m_result);
  EXPECT_EQ(200, m_client->m_status);
  
  m_client->spawnRequest(http::verb::get, "/device1/probe");

  EXPECT_EQ("Device given as: device1", m_client->m_result);
  EXPECT_EQ(200, m_client->m_status);
  
  // Make sure session closes when client closes the connection
  m_client->close();
  m_context.run_for(2ms);
  ASSERT_TRUE(session.expired());
}

TEST_F(HttpServerTest, request_response_with_query_parameters)
{
  auto handler = [&](RequestPtr request) -> bool {
    EXPECT_EQ("device1", get<string>(request->m_parameters["device"]));
    EXPECT_EQ("//DataItem[@type='POSITION' and @subType='ACTUAL']", get<string>(request->m_parameters["path"]));
    EXPECT_EQ(123456, get<uint64_t>(request->m_parameters["from"]));
    EXPECT_EQ(1000, get<int>(request->m_parameters["count"]));
    EXPECT_EQ(10000, get<int>(request->m_parameters["heartbeat"]));

    Response resp(status::ok);
    resp.m_body = "Done";
    request->m_session->writeResponse(resp, [](){
      cout << "Written" << endl;
    });
    return true;
  };
  
  string qp(
      "path={string}&from={unsigned_integer}&"
      "interval={integer}&count={integer:100}&"
      "heartbeat={integer:10000}&to={unsigned_integer}");
  m_server->addRouting({boost::beast::http::verb::get, "/sample?" + qp, handler});
  m_server->addRouting({boost::beast::http::verb::get, "/{device}/sample?" + qp, handler});

  start();
  startClient();

  m_client->spawnRequest(http::verb::get,
                         "/device1/sample"
                         "?path=//DataItem[@type=%27POSITION%27%20and%20@subType=%27ACTUAL%27]"
                         "&from=123456&count=1000");
  ASSERT_TRUE(m_client->m_done);
  EXPECT_EQ("Done", m_client->m_result);
  EXPECT_EQ(200, m_client->m_status);
}

TEST_F(HttpServerTest, request_put_when_put_not_allowed)
{
  weak_ptr<Session> session;
  
  auto probe = [&](RequestPtr request) -> bool {
    EXPECT_TRUE(false);
    return true;
  };
  
  m_server->addRouting(Routing{boost::beast::http::verb::put, "/probe", probe});
  
  start();
  startClient();

  m_client->spawnRequest(http::verb::put,
                         "/probe");
  ASSERT_TRUE(m_client->m_done);
  EXPECT_EQ(int(http::status::bad_request), m_client->m_status);
  EXPECT_EQ("PUT, POST, and DELETE are not allowed. MTConnect Agent is read only and only GET is allowed.", m_client->m_result);
}

TEST_F(HttpServerTest, request_put_when_put_allowed)
{
  weak_ptr<Session> session;
  
  auto handler = [&](RequestPtr request) -> bool {
    EXPECT_EQ(http::verb::put, request->m_verb);
    Response resp(status::ok);
    resp.m_body = "Put ok";
    request->m_session->writeResponse(resp, [](){
      cout << "Written" << endl;
      return true;
    });
    return true;
  };
  
  m_server->addRouting(Routing{boost::beast::http::verb::put, "/probe", handler});
  m_server->allowPuts();
  
  start();
  startClient();

  m_client->spawnRequest(http::verb::put,
                         "/probe");
  ASSERT_TRUE(m_client->m_done);
  EXPECT_EQ(int(http::status::ok), m_client->m_status);
  EXPECT_EQ("Put ok", m_client->m_result);

}

TEST_F(HttpServerTest, request_put_when_put_not_allowed_from_ip_address)
{
  weak_ptr<Session> session;
  
  auto probe = [&](RequestPtr request) -> bool {
    EXPECT_TRUE(false);
    return true;
  };
  
  m_server->addRouting(Routing{boost::beast::http::verb::put, "/probe", probe});
  m_server->allowPutFrom("1.1.1.1");
  
  start();
  startClient();

  m_client->spawnRequest(http::verb::put,
                         "/probe");
  ASSERT_TRUE(m_client->m_done);
  EXPECT_EQ(int(http::status::bad_request), m_client->m_status);
  EXPECT_EQ("PUT, POST, and DELETE are not allowed from 127.0.0.1", m_client->m_result);
}

TEST_F(HttpServerTest, request_put_when_put_allowed_from_ip_address)
{
  weak_ptr<Session> session;
  
  auto handler = [&](RequestPtr request) -> bool {
    EXPECT_EQ(http::verb::put, request->m_verb);
    Response resp(status::ok);
    resp.m_body = "Put ok";
    request->m_session->writeResponse(resp, [](){
      cout << "Written" << endl;
      return true;
    });
    return true;
  };
  
  m_server->addRouting(Routing{boost::beast::http::verb::put, "/probe", handler});
  m_server->allowPutFrom("127.0.0.1");
  
  start();
  startClient();

  m_client->spawnRequest(http::verb::put,
                         "/probe");
  ASSERT_TRUE(m_client->m_done);
  EXPECT_EQ(int(http::status::ok), m_client->m_status);
  EXPECT_EQ("Put ok", m_client->m_result);
}

TEST_F(HttpServerTest, request_with_connect_close)
{
  
}

TEST_F(HttpServerTest, content_payload)
{
  
}

TEST_F(HttpServerTest, content_with_put_values)
{
  
}

TEST_F(HttpServerTest, streaming_response)
{
  
}

TEST_F(HttpServerTest, additional_header_fields)
{
  
}
