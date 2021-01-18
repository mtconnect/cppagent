// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "http_server/server.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using namespace std;
using namespace mtconnect;
using namespace mtconnect::http_server;


class TestResponse : public Response
{
public:
  using Response::Response;
  void writeResponse(const char *body,
                     const size_t size,
                     const ResponseCode code,
                     const std::string &mimeType = "text/plain",
                     const std::chrono::seconds expires = std::chrono::seconds(0)) override
  {
    m_body = std::string(body, size);
    m_code = code;
    Response::writeResponse(body, size, code, mimeType, expires);
  }

  std::string getHeaderDate() override
  {
    return "TIME+DATE";
  }


  std::string m_body;
  uint16_t m_code;
};

class HttpServerTest : public testing::Test
{
 protected:
  void SetUp() override
  {
    m_server = make_unique<Server>();
  }

  void TearDown() override
  {
    m_server.reset();
  }

  unique_ptr<Server> m_server;
};

TEST_F(HttpServerTest, TestSimpleRouting)
{
  auto probe = [&](const Routing::Request &request,
                                Response &response) -> bool {
    if (request.m_parameters.count("device") > 0)
      response.writeResponse(string("Device given as: ") + get<string>(request.m_parameters.find("device")->second));
    else
      response.writeResponse("All Devices");
    
    return true;
  };
  
  m_server->addRouting({"GET", "/probe", probe});
  m_server->addRouting({"GET", "/{device}/probe", probe});
  stringstream out;
  TestResponse response(out);
  Routing::Request request;
  
  request.m_verb = "GET";
  request.m_path = "/probe";
  ASSERT_TRUE(m_server->dispatch(request, response));
  EXPECT_EQ("All Devices", response.m_body);
  string r1 = "HTTP/1.1 200 OK\r\n"
       "Date: TIME+DATE\r\n"
       "Server: MTConnectAgent\r\n"
       "Connection: close\r\n"
       "Expires: -1\r\n"
       "Cache-Control: private, max-age=0\r\nContent-Length: 11\r\n"
       "Content-Type: text/plain\r\n"
       "\r\n"
  "All Devices";
  out.flush();
  EXPECT_EQ(r1, out.str());
  EXPECT_EQ(200, response.m_code);

  request.m_path = "/Device1/probe";
  ASSERT_TRUE(m_server->dispatch(request, response));
  EXPECT_EQ("Device given as: Device1", response.m_body);
  EXPECT_EQ(200, response.m_code);
  
  request.m_path = "/Device1/current";
  ASSERT_FALSE(m_server->dispatch(request, response));
}

// Test diabling of HTTP PUT or POST
TEST_F(HttpServerTest, PutBlocking)
{
  Routing::QueryMap queries;
  string body;

  queries["time"] = "TIME";
  queries["line"] = "205";
  queries["power"] = "ON";

  
}

// Test diabling of HTTP PUT or POST
TEST_F(HttpServerTest, PutBlockingFrom)
{
  Routing::QueryMap queries;
  string body;
  m_server->enablePut();

  m_server->allowPutFrom("192.168.0.1");

  queries["time"] = "TIME";
  queries["line"] = "205";
  
#if 0
  m_agentTestHelper->m_path = "/LinuxCNC";

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "HTTP PUT, POST, and DELETE are not allowed from 127.0.0.1");
  }

  m_agentTestHelper->m_path = "/LinuxCNC/current";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Line", "UNAVAILABLE");
  }

  // Retry request after adding ip address
  m_agentTestHelper->m_path = "/LinuxCNC";
  m_agent->allowPutFrom("127.0.0.1");

  {
    PARSE_XML_RESPONSE_PUT(body, queries);
  }

  m_agentTestHelper->m_path = "/LinuxCNC/current";

  {
    PARSE_XML_RESPONSE;
    ASSERT_XML_PATH_EQUAL(doc, "//m:Line", "205");
  }
#endif
}
