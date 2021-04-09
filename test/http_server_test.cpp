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
  Client(net::io_context& ioc)
  : m_context(ioc)
  {
  }
  
  void fail(beast::error_code ec, char const* what)
  {
    LOG(error) << what << ": " << ec.message() << "\n";
    m_done = true;
  }

  void request(unsigned short port, boost::beast::http::verb verb,
               std::string const& target,
               net::yield_context yield)
  {
    beast::error_code ec;
    
    m_done = false;
    m_status = -1;
    m_result.clear();
    
    // These objects perform our I/O
    beast::tcp_stream stream(m_context);
    tcp::endpoint server(net::ip::address_v4::from_string("127.0.0.1"), port);
        
    // Set the timeout.
    stream.expires_after(std::chrono::seconds(30));
    
    // Make the connection on the IP address we get from a lookup
    stream.async_connect(server, yield[ec]);
    if(ec)
      return fail(ec, "connect");
    
    // Set up an HTTP GET request message
    http::request<http::string_body> req{verb, target, 11};
    req.set(http::field::host, "localhost");
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    
    // Set the timeout.
    stream.expires_after(std::chrono::seconds(30));
    
    // Send the HTTP request to the remote host
    http::async_write(stream, req, yield[ec]);
    if(ec)
      return fail(ec, "write");
    
    // This buffer is used for reading and must be persisted
    beast::flat_buffer b;
    
    // Declare a container to hold the response
    http::response<http::string_body> res;
    
    // Receive the HTTP response
    http::async_read(stream, b, res, yield[ec]);
    if(ec)
      return fail(ec, "read");
    
    m_result = res.body();
    m_status = res.result_int();
    
    // Write the message to standard out
    std::cout << res << std::endl;
    
    // Gracefully close the socket
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    
    m_done = true;
    
    // not_connected happens sometimes
    // so don't bother reporting it.
    //
    if(ec && ec != beast::errc::not_connected)
      return fail(ec, "shutdown");
    
    // If we get here then the connection is closed gracefully
  }
  
  int m_status;
  std::string m_result;
  asio::io_context &m_context;
  bool m_done{false};
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
    m_client = make_unique<Client>(m_context);
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

TEST_F(HttpServerTest, TestSimpleRouting)
{
  auto probe = [&](RequestPtr request) -> bool {
    Response resp(status::ok);
    if (request->m_parameters.count("device") > 0)
      resp.m_body = string("Device given as: ") + get<string>(request->m_parameters.find("device")->second);
    else
      resp.m_body = "All Devices";
    request->m_session->writeResponse(resp);
    return true;
  };
  
  m_server->addRouting(Routing{boost::beast::http::verb::get, "/probe", probe});
  m_server->addRouting({boost::beast::http::verb::get, "/{device}/probe", probe});
  
  start();
  m_client->m_done = false;

  while (!m_server->isListening())
    m_context.run_one();
  
  asio::spawn(m_context,
              std::bind(&Client::request,
                        m_client.get(),
                        static_cast<unsigned short>(m_server->getPort()),
                        http::verb::get,
                        "/probe",
                        std::placeholders::_1));
  
  while (!m_client->m_done)
    m_context.run_one();
  
  EXPECT_EQ("All Devices", m_client->m_result);
  EXPECT_EQ(200, m_client->m_status);
}

#if 0
// Test diabling of HTTP PUT or POST
TEST_F(HttpServerTest, PutBlocking)
{
  Routing::QueryMap queries;
  string body;

  queries["time"] = "TIME";
  queries["line"] = "205";
  queries["power"] = "ON";
}

TEST_F(HttpServerTest, test_additional_server_fields)
{
  m_server->setHttpHeaders({"Access-Control-Origin: *"});
  auto probe = [&](const Routing::Request &request,
                                Response &response) -> bool {
    if (request.m_parameters.count("device") > 0)
      response.writeResponse(string("Device given as: ") + get<string>(request.m_parameters.find("device")->second));
    else
      response.writeResponse("All Devices");
    
    return true;
  };
  stringstream out;
  TestResponse response(out, m_server->getHttpHeaders());
  Routing::Request request;

  m_server->addRouting({"GET", "/probe", probe});
  request.m_verb = "GET";
  request.m_path = "/probe";
  ASSERT_TRUE(m_server->dispatch(request, response));

  string r1 = "HTTP/1.1 200 OK\r\n"
       "Date: TIME+DATE\r\n"
       "Server: MTConnectAgent\r\n"
       "Connection: close\r\n"
       "Expires: -1\r\n"
       "Cache-Control: private, max-age=0\r\nContent-Length: 11\r\n"
       "Content-Type: text/plain\r\n"
       "Access-Control-Origin: *\r\n"
       "\r\n"
  "All Devices";
  out.flush();
  EXPECT_EQ(r1, out.str());
  EXPECT_EQ(200, response.m_code);
}

TEST_F(HttpServerTest, test_simple_get_response)
{

}
#endif
