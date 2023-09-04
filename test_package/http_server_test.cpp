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

// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include <boost/algorithm/string.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "mtconnect/logging.hpp"
#include "mtconnect/sink/rest_sink/server.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::sink::rest_sink;

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;
using tcp = boost::asio::ip::tcp;

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

    // Set the timeout.
    m_stream.expires_after(std::chrono::seconds(30));

    // Make the connection on the IP address we get from a lookup
    m_stream.async_connect(server, yield[ec]);
    if (ec)
    {
      return fail(ec, "connect");
    }
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
    m_stream.expires_after(std::chrono::seconds(30));
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

        EXPECT_EQ("--" + m_boundary, boundary);

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

    while (!m_done && m_context.run_for(20ms) > 0)
      ;
  }

  void close()
  {
    beast::error_code ec;

    // Gracefully close the socket
    m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);
  }

  bool m_connected {false};
  int m_status;
  std::string m_result;
  asio::io_context& m_context;
  bool m_done {false};
  beast::tcp_stream m_stream;
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

class RestServiceTest : public testing::Test
{
protected:
  void SetUp() override
  {
    using namespace mtconnect::configuration;
    m_server = make_unique<Server>(m_context, ConfigOptions {{Port, 0}, {ServerIp, "127.0.0.1"s}});
  }

  void createServer(const ConfigOptions& options)
  {
    using namespace mtconnect::configuration;
    ConfigOptions opts {{Port, 0}, {ServerIp, "127.0.0.1"s}};
    opts.merge(ConfigOptions(options));
    m_server = make_unique<Server>(m_context, opts);
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

TEST_F(RestServiceTest, simple_request_response)
{
  weak_ptr<Session> savedSession;

  auto probe = [&](SessionPtr session, RequestPtr request) -> bool {
    savedSession = session;
    ResponsePtr resp = make_unique<Response>(status::ok);
    if (request->m_parameters.count("device") > 0)
      resp->m_body =
          string("Device given as: ") + get<string>(request->m_parameters.find("device")->second);
    else
      resp->m_body = "All Devices";
    session->writeResponse(std::move(resp), []() { cout << "Written" << endl; });
    return true;
  };

  m_server->addRouting({boost::beast::http::verb::get, "/probe", probe});
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
  ASSERT_TRUE(savedSession.expired());
}

TEST_F(RestServiceTest, request_response_with_query_parameters)
{
  auto handler = [&](SessionPtr session, RequestPtr request) -> bool {
    EXPECT_EQ("device1", get<string>(request->m_parameters["device"]));
    EXPECT_EQ("//DataItem[@type='POSITION' and @subType='ACTUAL']",
              get<string>(request->m_parameters["path"]));
    EXPECT_EQ(123456, get<uint64_t>(request->m_parameters["from"]));
    EXPECT_EQ(1000, get<int>(request->m_parameters["count"]));
    EXPECT_EQ(10000, get<int>(request->m_parameters["heartbeat"]));

    ResponsePtr resp = make_unique<Response>(status::ok);
    resp->m_body = "Done";
    session->writeResponse(std::move(resp), []() { cout << "Written" << endl; });
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

TEST_F(RestServiceTest, request_put_when_put_not_allowed)
{
  auto probe = [&](SessionPtr session, RequestPtr request) -> bool {
    EXPECT_TRUE(false);
    return true;
  };

  m_server->addRouting({boost::beast::http::verb::put, "/probe", probe});

  start();
  startClient();

  m_client->spawnRequest(http::verb::put, "/probe");
  ASSERT_TRUE(m_client->m_done);
  EXPECT_EQ(int(http::status::bad_request), m_client->m_status);
  EXPECT_EQ(
      "PUT, POST, and DELETE are not allowed. MTConnect Agent is read only and only GET is "
      "allowed.",
      m_client->m_result);
}

TEST_F(RestServiceTest, request_put_when_put_allowed)
{
  auto handler = [&](SessionPtr session, RequestPtr request) -> bool {
    EXPECT_EQ(http::verb::put, request->m_verb);
    ResponsePtr resp = make_unique<Response>(status::ok, "Put ok");
    session->writeResponse(std::move(resp), []() {
      cout << "Written" << endl;
      return true;
    });
    return true;
  };

  m_server->addRouting({boost::beast::http::verb::put, "/probe", handler});
  m_server->allowPuts();

  start();
  startClient();

  m_client->spawnRequest(http::verb::put, "/probe");
  ASSERT_TRUE(m_client->m_done);
  EXPECT_EQ(int(http::status::ok), m_client->m_status);
  EXPECT_EQ("Put ok", m_client->m_result);
}

TEST_F(RestServiceTest, request_put_when_put_not_allowed_from_ip_address)
{
  weak_ptr<Session> session;

  auto probe = [&](SessionPtr session, RequestPtr request) -> bool {
    EXPECT_TRUE(false);
    return true;
  };

  m_server->addRouting({boost::beast::http::verb::put, "/probe", probe});
  m_server->allowPutFrom("1.1.1.1");

  start();
  startClient();

  m_client->spawnRequest(http::verb::put, "/probe");
  ASSERT_TRUE(m_client->m_done);
  EXPECT_EQ(int(http::status::bad_request), m_client->m_status);
  EXPECT_EQ("PUT, POST, and DELETE are not allowed from 127.0.0.1", m_client->m_result);
}

TEST_F(RestServiceTest, request_put_when_put_allowed_from_ip_address)
{
  auto handler = [&](SessionPtr session, RequestPtr request) -> bool {
    EXPECT_EQ(http::verb::put, request->m_verb);
    ResponsePtr resp = make_unique<Response>(status::ok, "Put ok");
    session->writeResponse(std::move(resp), []() {
      cout << "Written" << endl;
      return true;
    });
    return true;
  };

  m_server->addRouting({boost::beast::http::verb::put, "/probe", handler});
  m_server->allowPutFrom("127.0.0.1");

  start();
  startClient();

  m_client->spawnRequest(http::verb::put, "/probe");
  ASSERT_TRUE(m_client->m_done);
  EXPECT_EQ(int(http::status::ok), m_client->m_status);
  EXPECT_EQ("Put ok", m_client->m_result);
}

TEST_F(RestServiceTest, request_with_connect_close)
{
  weak_ptr<Session> savedSession;

  auto handler = [&](SessionPtr session, RequestPtr request) -> bool {
    savedSession = session;
    ResponsePtr resp = make_unique<Response>(status::ok, "Probe");
    session->writeResponse(std::move(resp), []() {
      cout << "Written" << endl;
      return true;
    });
    return true;
  };

  m_server->addRouting(Routing {boost::beast::http::verb::get, "/probe", handler});

  start();
  startClient();

  m_client->spawnRequest(http::verb::get, "/probe", "", true);
  ASSERT_TRUE(m_client->m_done);
  EXPECT_EQ(int(http::status::ok), m_client->m_status);
  EXPECT_EQ("Probe", m_client->m_result);

  m_client->spawnRequest(http::verb::get, "/probe", "", true);
  EXPECT_TRUE(m_client->m_ec);
  EXPECT_FALSE(savedSession.lock());
}

TEST_F(RestServiceTest, put_content_to_server)
{
  string body;
  auto handler = [&](SessionPtr session, RequestPtr request) -> bool {
    EXPECT_EQ("Body Content", request->m_body);
    body = request->m_body;

    ResponsePtr resp = make_unique<Response>(status::ok);
    session->writeResponse(std::move(resp), []() { cout << "Written" << endl; });
    return true;
  };

  m_server->addRouting({boost::beast::http::verb::get, "/probe", handler});

  start();
  startClient();

  m_client->spawnRequest(http::verb::get, "/probe", "Body Content", false);
  ASSERT_TRUE(m_client->m_done);
  ASSERT_EQ("Body Content", body);
}

TEST_F(RestServiceTest, put_content_with_put_values)
{
  string body, ct;
  auto handler = [&](SessionPtr session, RequestPtr request) -> bool {
    EXPECT_EQ("fish=%27chips%27&time=%27money%27", request->m_body);
    EXPECT_EQ("'chips'", request->m_query["fish"]);
    EXPECT_EQ("'money'", request->m_query["time"]);
    body = request->m_body;
    ct = request->m_contentType;

    ResponsePtr resp = make_unique<Response>(status::ok);
    session->writeResponse(std::move(resp), []() { cout << "Written" << endl; });
    return true;
  };

  m_server->addRouting({boost::beast::http::verb::get, "/probe", handler});

  start();
  startClient();

  m_client->spawnRequest(http::verb::get, "/probe", "fish=%27chips%27&time=%27money%27", false,
                         "application/x-www-form-urlencoded");
  ASSERT_TRUE(m_client->m_done);
  ASSERT_EQ("fish=%27chips%27&time=%27money%27", body);
  ASSERT_EQ("application/x-www-form-urlencoded", ct);
}

TEST_F(RestServiceTest, streaming_response)
{
  struct context
  {
    context(RequestPtr r, SessionPtr s) : m_request(r), m_session(s) {}
    RequestPtr m_request;
    SessionPtr m_session;
    bool m_written {false};
  };

  int count = 0;
  function<void(shared_ptr<context>)> chunk = [&](shared_ptr<context> ctx) {
    // cout << "Wrote chunk" << endl;
    ctx->m_written = true;
    // Stop after 5 chunks
  };

  function<void(shared_ptr<context>)> begun = [&](shared_ptr<context> ctx) {
    // cout << "Begun chunking" << endl;
    ctx->m_written = true;
  };

  shared_ptr<context> ctx;
  auto begin = [&](SessionPtr session, RequestPtr request) -> bool {
    ctx = make_shared<context>(request, session);
    ctx->m_written = false;
    session->beginStreaming("plain/text", bind(begun, ctx));
    return true;
  };

  m_server->addRouting({boost::beast::http::verb::get, "/sample", begin});

  start();
  startClient();

  m_client->spawnRequest(http::verb::get, "/sample");
  while (!ctx->m_written && m_context.run_for(20ms) > 0)
    ;
  m_client->spawnReadChunk();
  while (m_context.run_for(20ms) > 0)
    ;

  m_client->m_done = false;
  ctx->m_written = false;
  ctx->m_session->writeChunk("Chunk Content #" + to_string(++count), bind(chunk, ctx));
  while ((!ctx->m_written || !m_client->m_done) && m_context.run_for(20ms) > 0)
    ;
  cout << "Done: " << m_client->m_done << endl;
  EXPECT_EQ("Chunk Content #1", m_client->m_result);

  ctx->m_written = false;
  m_client->m_done = false;
  ctx->m_session->writeChunk("Chunk Content #" + to_string(++count), bind(chunk, ctx));
  while ((!ctx->m_written || !m_client->m_done) && m_context.run_for(200ms) > 0)
    ;
  EXPECT_EQ("Chunk Content #2", m_client->m_result);

  ctx->m_written = false;
  m_client->m_done = false;
  ctx->m_session->writeChunk("Chunk Content #" + to_string(++count), bind(chunk, ctx));
  while ((!ctx->m_written || !m_client->m_done) && m_context.run_for(20ms) > 0)
    ;
  EXPECT_EQ("Chunk Content #3", m_client->m_result);

  ctx->m_written = false;
  m_client->m_done = false;
  ctx->m_session->writeChunk("Chunk Content #" + to_string(++count), bind(chunk, ctx));
  while ((!ctx->m_written || !m_client->m_done) && m_context.run_for(20ms) > 0)
    ;
  EXPECT_EQ("Chunk Content #4", m_client->m_result);

  ctx->m_written = false;
  m_client->m_done = false;
  ctx->m_session->writeChunk("Chunk Content #" + to_string(++count), bind(chunk, ctx));
  while ((!ctx->m_written || !m_client->m_done) && m_context.run_for(20ms) > 0)
    ;
  EXPECT_EQ("Chunk Content #5", m_client->m_result);

  ctx->m_session->closeStream();
  while (m_context.run_for(20ms) > 0)
    ;
}

TEST_F(RestServiceTest, additional_header_fields)
{
  m_server->setHttpHeaders({"Access-Control-Allow-Origin:*", "Origin:https://foo.example"});

  auto probe = [&](SessionPtr session, RequestPtr request) -> bool {
    ResponsePtr resp = make_unique<Response>(status::ok, "Done");
    session->writeResponse(std::move(resp), []() { cout << "Written" << endl; });

    return true;
  };

  m_server->addRouting({boost::beast::http::verb::get, "/probe", probe});

  start();
  startClient();

  m_client->spawnRequest(http::verb::get, "/probe");
  ASSERT_TRUE(m_client->m_done);

  EXPECT_EQ(200, m_client->m_status);
  auto f1 = m_client->m_fields.find("Access-Control-Allow-Origin");
  ASSERT_NE(m_client->m_fields.end(), f1);
  ASSERT_EQ("*", f1->second);

  auto f2 = m_client->m_fields.find("Origin");
  ASSERT_NE(m_client->m_fields.end(), f2);
  ASSERT_EQ("https://foo.example", f2->second);
}

const string CertFile(TEST_RESOURCE_DIR "/user.crt");
const string KeyFile {TEST_RESOURCE_DIR "/user.key"};
const string DhFile {TEST_RESOURCE_DIR "/dh2048.pem"};
const string RootCertFile(TEST_RESOURCE_DIR "/rootca.crt");

TEST_F(RestServiceTest, failure_when_tls_only)
{
  using namespace mtconnect::configuration;
  ConfigOptions options {{TlsCertificateChain, CertFile},
                         {TlsPrivateKey, KeyFile},
                         {TlsDHKey, DhFile},
                         {TlsCertificatePassword, "mtconnect"s},
                         {TlsOnly, true}};

  createServer(options);

  auto probe = [&](SessionPtr session, RequestPtr request) -> bool {
    ResponsePtr resp = make_unique<Response>(status::ok, "Done");
    session->writeResponse(std::move(resp), []() { cout << "Written" << endl; });

    return true;
  };

  m_server->addRouting({boost::beast::http::verb::get, "/probe", probe});

  start();
  startClient();

  m_client->spawnRequest(http::verb::get, "/probe");
  ASSERT_TRUE(m_client->m_done);

  EXPECT_EQ((unsigned)boost::beast::http::status::unauthorized, m_client->m_status);
}
