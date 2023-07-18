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

#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>

#include <chrono>
#include <date/date.h>  // This file is to allow std::chrono types to be output to a stream
#include <memory>
#include <sstream>
#include <thread>

#include "mtconnect/source/adapter/shdr/connector.hpp"

namespace date {};
using namespace date;

using namespace std;
using namespace std::chrono;
using namespace mtconnect;
using namespace mtconnect::source::adapter;
using namespace mtconnect::source::adapter::shdr;

namespace asio = boost::asio;
using tcp = boost::asio::ip::tcp;
namespace ip = boost::asio::ip;
namespace sys = boost::system;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class TestConnector : public Connector
{
public:
  TestConnector(boost::asio::io_context::strand &strand, const std::string &server,
                unsigned int port, std::chrono::seconds legacyTimeout = std::chrono::seconds {5})
    : Connector(strand, server, port, legacyTimeout), m_disconnected(false), m_strand(strand)
  {}

  bool start(unsigned short port)
  {
    m_port = port;
    return Connector::start();
  }

  void processData(const std::string &data) override
  {
    if (data[0] == '*')
      protocolCommand(data);
    else
    {
      m_data = data;
      m_list.emplace_back(m_data);
    }
  }

  void protocolCommand(const std::string &data) override { m_command = data; }

  void connecting() override {}

  void disconnected() override { m_disconnected = true; }
  void connected() override { m_disconnected = false; }
  bool heartbeats() { return m_heartbeats; }

  void startHeartbeats(std::string &aString) { Connector::startHeartbeats(aString); }

  void resetHeartbeats() { m_heartbeats = false; }

public:
  std::vector<std::string> m_list;
  std::string m_data;
  std::string m_command;
  bool m_disconnected;

  boost::asio::io_context::strand &m_strand;
};

class ConnectorTest : public testing::Test
{
protected:
  void SetUp() override
  {
    boost::asio::io_context::strand strand(m_context);
    m_connector = std::make_unique<TestConnector>(strand, "127.0.0.1", m_port);
    m_connector->m_disconnected = true;
    m_connected = false;
  }

  void TearDown() override
  {
    m_context.stop();
    m_connector.reset();
    if (m_server)
      m_server->close();
    m_server.reset();
    m_acceptor.reset();
  }

  void startServer(const std::string addr = "127.0.0.1")
  {
    tcp::endpoint sp(ip::make_address(addr), 0);
    m_connected = false;
    m_acceptor = make_unique<tcp::acceptor>(m_context, sp);
    ASSERT_TRUE(m_acceptor->is_open());
    tcp::endpoint ep = m_acceptor->local_endpoint();
    m_port = ep.port();

    m_acceptor->async_accept([this](sys::error_code ec, tcp::socket socket) {
      if (ec)
        cout << ec.category().message(ec.value()) << ": " << ec.message() << endl;
      ASSERT_FALSE(ec);
      ASSERT_TRUE(socket.is_open());
      m_connected = true;
      m_server = make_unique<tcp::socket>(std::move(socket));
    });
  }

  template <typename Rep, typename Period>
  void runUntil(chrono::duration<Rep, Period> to, function<bool()> pred)
  {
    int runs;
    for (runs = 0; runs < 50 && !pred(); runs++)
    {
      if (m_context.run_one_for(to) == 0)
        break;
    }

    EXPECT_TRUE(pred());
  }

  void send(const std::string &s)
  {
    ASSERT_TRUE(m_server);

    asio::streambuf outoging;
    std::ostream os(&outoging);
    os << s;

    sys::error_code ec;
    asio::write(*m_server, outoging, ec);
    ASSERT_FALSE(ec);
  }

  template <typename Rep, typename Period>
  string read(chrono::duration<Rep, Period> dur)
  {
    m_line.clear();

    asio::streambuf data;
    asio::async_read_until(*m_server, data, '\n', [this, &data](sys::error_code ec, size_t len) {
      EXPECT_FALSE(ec);
      if (ec)
      {
        cout << ec.category().message(ec.value()) << ": " << ec.message() << endl;
      }
      else
      {
        istream is(&data);
        string line;
        getline(is, m_line);
      }
    });

    runUntil(dur, [this]() { return !m_line.empty(); });

    return m_line;
  }

  std::unique_ptr<TestConnector> m_connector;
  unsigned short m_port {0};
  boost::asio::io_context m_context;

  std::unique_ptr<asio::ip::tcp::socket> m_server;
  std::unique_ptr<tcp::acceptor> m_acceptor;
  bool m_connected;
  string m_line;
};

TEST_F(ConnectorTest, Connection)
{
  ASSERT_TRUE(m_connector->m_disconnected);

  startServer();

  m_connector->start(m_port);

  runUntil(5s, [this]() -> bool { return m_connected && m_connector->isConnected(); });

  EXPECT_FALSE(m_connector->m_disconnected);
  auto line = read(1s);
  ASSERT_EQ("* PING", line);
}

TEST_F(ConnectorTest, DataCapture)
{
  // Start the accept thread
  ASSERT_TRUE(m_connector->m_disconnected);

  startServer();

  m_connector->start(m_port);
  runUntil(2s, [this]() -> bool { return m_connected && m_connector->isConnected(); });

  send("Hello Connector\n");

  runUntil(1s, [this]() -> bool { return !m_connector->m_data.empty(); });

  ASSERT_EQ("Hello Connector", m_connector->m_data);
}

TEST_F(ConnectorTest, Disconnect)
{
  // Start the accept thread
  startServer();

  m_connector->start(m_port);
  runUntil(2s, [this]() -> bool { return m_connected && m_connector->isConnected(); });

  ASSERT_FALSE(m_connector->m_disconnected);

  m_server->close();

  runUntil(2s, [this]() -> bool { return m_connector->m_disconnected; });

  ASSERT_TRUE(m_connector->m_disconnected);
}

TEST_F(ConnectorTest, ProtocolCommand)
{
  // Start the accept thread
  startServer();

  m_connector->start(m_port);
  runUntil(2s, [this]() -> bool { return m_connected && m_connector->isConnected(); });

  send("* Hello Connector\n");

  runUntil(1s, [this]() -> bool { return !m_connector->m_command.empty(); });

  ASSERT_EQ("* Hello Connector", m_connector->m_command);
}

TEST_F(ConnectorTest, Heartbeat)
{
  // Start the accept thread
  startServer();

  m_connector->start(m_port);
  runUntil(2s, [this]() -> bool { return m_connected && m_connector->isConnected(); });

  auto line = read(1s);
  ASSERT_EQ("* PING", line);

  send("* PONG 1000\n");

  runUntil(2s, [this]() -> bool { return m_connector->heartbeats(); });

  // Respond to the heartbeat of 1 second
  ASSERT_TRUE(m_connector->heartbeats());
  ASSERT_EQ(std::chrono::milliseconds {1000}, m_connector->heartbeatFrequency());
}

TEST_F(ConnectorTest, HeartbeatPong)
{
  // TODO Copy&Paste from Heartbeat
  // Start the accept thread
  startServer();

  m_connector->start(m_port);
  runUntil(2s, [this]() -> bool { return m_connected && m_connector->isConnected(); });

  auto line = read(1s);
  ASSERT_EQ("* PING", line);

  send("* PONG 1000\n");
  runUntil(2s, [this]() -> bool { return m_connector->heartbeats(); });

  // Respond to the heartbeat of 1 second
  ASSERT_TRUE(m_connector->heartbeats());
  ASSERT_EQ(std::chrono::milliseconds {1000}, m_connector->heartbeatFrequency());

  auto last_heartbeat = system_clock::now();

  // Test to make sure we can send and receive 5 heartbeats
  for (int i = 0; i < 5; i++)
  {
    auto line = read(2s);

    auto now = system_clock::now();
    ASSERT_TRUE(now - last_heartbeat < 2000ms);
    last_heartbeat = now;

    // Respond to the heartbeat of 1 second
    send("* PONG 1000\n");
    ASSERT_FALSE(m_connector->m_disconnected);
  }
}

TEST_F(ConnectorTest, HeartbeatDataKeepAlive)
{
  startServer();

  m_connector->start(m_port);
  runUntil(2s, [this]() -> bool { return m_connected && m_connector->isConnected(); });

  auto line = read(1s);
  ASSERT_EQ("* PING", line);

  send("* PONG 1000\n");
  runUntil(2s, [this]() -> bool { return m_connector->heartbeats(); });

  // Respond to the heartbeat of 1 second
  ASSERT_TRUE(m_connector->heartbeats());
  ASSERT_EQ(std::chrono::milliseconds {1000}, m_connector->heartbeatFrequency());

  auto last_heartbeat = system_clock::now();

  // Test to make sure we can send and receive 5 heartbeats
  for (int i = 0; i < 5; i++)
  {
    auto line = read(2s);

    auto now = system_clock::now();
    ASSERT_TRUE(now - last_heartbeat < 2000ms);
    last_heartbeat = now;

    // Respond to the heartbeat of 1 second
    send("Some Data\n");
    ASSERT_FALSE(m_connector->m_disconnected);
  }
}

TEST_F(ConnectorTest, HeartbeatTimeout)
{
  startServer();

  m_connector->start(m_port);
  runUntil(2s, [this]() -> bool { return m_connected && m_connector->isConnected(); });

  auto line = read(1s);
  ASSERT_EQ("* PING", line);

  send("* PONG 1000\n");
  runUntil(2s, [this]() -> bool { return m_connector->heartbeats(); });

  // Respond to the heartbeat of 1 second
  ASSERT_TRUE(m_connector->heartbeats());
  ASSERT_EQ(std::chrono::milliseconds {1000}, m_connector->heartbeatFrequency());

  // Respond to the heartbeat of 1 second
  m_context.run_for(2200ms);

  ASSERT_TRUE(m_connector->m_disconnected);
}

TEST_F(ConnectorTest, LegacyTimeout)
{
  startServer();

  m_connector->start(m_port);
  runUntil(2s, [this]() -> bool { return m_connected && m_connector->isConnected(); });

  auto line = read(1s);
  ASSERT_EQ("* PING", line);

  // Write some data...
  send("Hello connector\n");

  // No pings, but timeout after 5 seconds of silence
  // Respond to the heartbeat of 1 second
  m_context.run_for(5200ms);

  ASSERT_TRUE(m_connector->m_disconnected);
}

TEST_F(ConnectorTest, ParseBuffer)
{
  startServer();

  m_connector->start(m_port);
  runUntil(2s, [this]() -> bool { return m_connected && m_connector->isConnected(); });

  // Test data fragmentation
  send("Hello");
  ASSERT_EQ((string) "", m_connector->m_data);
  m_context.run_for(2ms);

  send(" There\n");
  runUntil(1s, [this]() { return !m_connector->m_data.empty(); });
  ASSERT_EQ((string) "Hello There", m_connector->m_data);
  m_connector->m_data.clear();

  send("Hello");
  m_context.run_for(2ms);
  ASSERT_EQ((string) "", m_connector->m_data);

  send(" There\nAnd ");
  runUntil(1s, [this]() { return !m_connector->m_data.empty(); });
  ASSERT_EQ((string) "Hello There", m_connector->m_data);
  m_connector->m_data.clear();

  send("Again\nXXX");
  runUntil(1s, [this]() { return !m_connector->m_data.empty(); });
  ASSERT_EQ((string) "And Again", m_connector->m_data);
}

TEST_F(ConnectorTest, ParseBufferFraming)
{
  startServer();

  m_connector->start(m_port);
  runUntil(2s, [this]() -> bool { return m_connected && m_connector->isConnected(); });

  m_connector->m_list.clear();
  send("first\nseco");
  m_context.run_for(2ms);
  send("nd\nthird\nfourth\nfifth");
  m_context.run_for(2ms);
  ASSERT_EQ((size_t)4, m_connector->m_list.size());
  ASSERT_EQ((string) "first", m_connector->m_list[0]);
  ASSERT_EQ((string) "second", m_connector->m_list[1]);
  ASSERT_EQ((string) "third", m_connector->m_list[2]);
  ASSERT_EQ((string) "fourth", m_connector->m_list[3]);
}

TEST_F(ConnectorTest, SendCommand)
{
  startServer();

  m_connector->start(m_port);
  runUntil(2s, [this]() -> bool { return m_connected && m_connector->isConnected(); });

  auto line = read(1s);
  ASSERT_EQ("* PING", line);

  ASSERT_TRUE(m_connector->isConnected());
  m_connector->sendCommand("Hello There;");

  line = read(1s);
  ASSERT_EQ("* Hello There;", line);
}

TEST_F(ConnectorTest, IPV6Connection)
{
// TODO: Need to port to Windows > VISTA
#if !defined(WIN32) && !defined(AGENT_WITHOUT_IPV6)
  m_connector.reset();

  startServer("::1");
  boost::asio::io_context::strand strand(m_context);
  m_connector = std::make_unique<TestConnector>(strand, "::1", m_port);

  m_connector->start(m_port);
  runUntil(2s, [this]() -> bool { return m_connected && m_connector->isConnected(); });

  ASSERT_FALSE(m_connector->m_disconnected);
#endif
}

TEST_F(ConnectorTest, StartHeartbeats)
{
  ASSERT_TRUE(!m_connector->heartbeats());

  string line = "* PONG ";
  m_connector->startHeartbeats(line);

  ASSERT_TRUE(!m_connector->heartbeats());

  line = "* PONK ";
  m_connector->startHeartbeats(line);

  ASSERT_TRUE(!m_connector->heartbeats());

  line = "* PONG      ";
  m_connector->startHeartbeats(line);

  ASSERT_TRUE(!m_connector->heartbeats());

  line = "* PONG FLAB";
  m_connector->startHeartbeats(line);

  ASSERT_TRUE(!m_connector->heartbeats());

  line = "* PONG       123";
  m_connector->startHeartbeats(line);

  ASSERT_TRUE(m_connector->heartbeats());
  ASSERT_EQ(std::chrono::milliseconds {123}, m_connector->heartbeatFrequency());

  m_connector->resetHeartbeats();

  line = "* PONG       456 ";
  m_connector->startHeartbeats(line);

  ASSERT_TRUE(m_connector->heartbeats());
  ASSERT_EQ(std::chrono::milliseconds {456}, m_connector->heartbeatFrequency());

  line = "* PONG 323";
  m_connector->startHeartbeats(line);

  ASSERT_TRUE(m_connector->heartbeats());
  ASSERT_EQ(std::chrono::milliseconds {323}, m_connector->heartbeatFrequency());
}

TEST_F(ConnectorTest, test_trimming_trailing_white_space)
{
  startServer();

  m_connector->start(m_port);
  runUntil(2s, [this]() -> bool { return m_connected && m_connector->isConnected(); });

  m_connector->m_list.clear();
  send("first    \r\nseco");
  m_context.run_for(2ms);
  send("nd  \t\r\n   \t  \r\n\n  third    \v\r\t\nfourth   \f\t\r\nr  \nfifth");
  m_context.run_for(2ms);
  ASSERT_EQ((size_t)5, m_connector->m_list.size());
  ASSERT_EQ((string) "first", m_connector->m_list[0]);
  ASSERT_EQ((string) "second", m_connector->m_list[1]);
  ASSERT_EQ((string) "  third", m_connector->m_list[2]);
  ASSERT_EQ((string) "fourth", m_connector->m_list[3]);
  ASSERT_EQ((string) "r", m_connector->m_list[4]);
}
