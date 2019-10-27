//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
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

namespace date {};
using namespace date;

#include "gtest/gtest.h"

#include <date/date.h>  // This file is to allow std::chrono types to be output to a stream
#include <chrono>
#include <sstream>
#include <thread>
#include "connector.hpp"

using namespace std;
using namespace std::chrono;
using namespace mtconnect;

class TestConnector : public Connector {
 public:
  TestConnector(const std::string &server, unsigned int port,
                std::chrono::seconds legacyTimeout = std::chrono::seconds{5})
      : Connector(server, port, legacyTimeout), m_disconnected(false) {}

  void processData(const std::string &data) override {
    m_data = data;
    m_list.push_back(m_data);
  }

  void protocolCommand(const std::string &data) override { m_command = data; }

  void disconnected() override { m_disconnected = true; }
  void connected() override { m_disconnected = false; }
  bool heartbeats() { return m_heartbeats; }

  void pushData(const char *data) { parseBuffer(data); }

  void startHeartbeats(std::string &aString) { Connector::startHeartbeats(aString); }
  void resetHeartbeats() { m_heartbeats = false; }

 public:
  std::vector<std::string> m_list;
  std::string m_data;
  std::string m_command;
  bool m_disconnected;
};

class ConnectorTest : public testing::Test, public dlib::threaded_object {
 protected:
  void SetUp() override {
    ASSERT_TRUE(!create_listener(m_server, 0, "127.0.0.1"));
    m_port = m_server->get_listening_port();
    m_connector.reset(new TestConnector("127.0.0.1", m_port));
    m_connector->m_disconnected = true;
  }

  void TearDown() override {
    m_server.reset();
    m_serverSocket.reset();
    stop();
    wait();
    m_connector.reset();
  }

  void thread() override { m_connector->connect(); }

  dlib::scoped_ptr<dlib::listener> m_server;
  dlib::scoped_ptr<dlib::connection> m_serverSocket;
  dlib::scoped_ptr<TestConnector> m_connector;
  unsigned short m_port;
};

TEST_F(ConnectorTest, Connection) {
  ASSERT_TRUE(m_connector->m_disconnected);
  start();

  auto res = m_server->accept(m_serverSocket);
  ASSERT_EQ(0, res);
  this_thread::sleep_for(100ms);
  ASSERT_TRUE(m_serverSocket.get());
  ASSERT_TRUE(!m_connector->m_disconnected);
}

TEST_F(ConnectorTest, DataCapture) {
  // Start the accept thread
  start();

  auto res = m_server->accept(m_serverSocket);
  ASSERT_EQ(0, res);
  ASSERT_TRUE(m_serverSocket.get());

  string command("Hello Connector\n");
  ASSERT_TRUE((size_t)m_serverSocket->write(command.c_str(), command.length()) == command.length());
  this_thread::sleep_for(1000ms);

  // \n is stripped from the posted data.
  ASSERT_EQ(command.substr(0, command.length() - 1), m_connector->m_data);
}

TEST_F(ConnectorTest, Disconnect) {
  // Start the accept thread
  start();

  auto res = m_server->accept(m_serverSocket);
  ASSERT_EQ(0, res);
  this_thread::sleep_for(1000ms);
  ASSERT_TRUE(m_serverSocket.get());
  ASSERT_TRUE(!m_connector->m_disconnected);
  m_serverSocket.reset();
  this_thread::sleep_for(1000ms);
  ASSERT_TRUE(m_connector->m_disconnected);
}

TEST_F(ConnectorTest, ProtocolCommand) {
  // Start the accept thread
  start();

  auto res = m_server->accept(m_serverSocket);
  ASSERT_EQ(0, res);
  ASSERT_TRUE(m_serverSocket.get());

  const auto cmd = "* Hello Connector\n";
  ASSERT_EQ(strlen(cmd), (size_t)m_serverSocket->write(cmd, strlen(cmd)));
  this_thread::sleep_for(1000ms);

  // \n is stripped from the posted data.
  ASSERT_TRUE(strncmp(cmd, m_connector->m_command.c_str(), strlen(cmd) - 1) == 0);
}

TEST_F(ConnectorTest, Heartbeat) {
  // Start the accept thread
  start();

  auto res = m_server->accept(m_serverSocket);
  ASSERT_EQ(0, res);
  ASSERT_TRUE(m_serverSocket.get());

  // Receive initial heartbeat request "* PING\n"
  char buf[1024] = {0};
  auto numRead = m_serverSocket->read(buf, 1023, 5000);
  ASSERT_EQ(7L, numRead);
  buf[7] = '\0';
  ASSERT_TRUE(strcmp(buf, "* PING\n") == 0);

  // Respond to the heartbeat of 1 second
  const auto pong = "* PONG 1000\n";
  auto written = (size_t)m_serverSocket->write(pong, strlen(pong));
  ASSERT_EQ(strlen(pong), written);
  this_thread::sleep_for(1000ms);

  ASSERT_TRUE(m_connector->heartbeats());
  ASSERT_EQ(std::chrono::milliseconds{1000}, m_connector->heartbeatFrequency());
}

TEST_F(ConnectorTest, HeartbeatPong) {
  // TODO Copy&Paste from Heartbeat
  // Start the accept thread
  start();

  auto res = m_server->accept(m_serverSocket);
  ASSERT_EQ(0, res);
  ASSERT_TRUE(m_serverSocket.get());

  // Receive initial heartbeat request "* PING\n"
  char buf[1024] = {0};
  auto numRead = m_serverSocket->read(buf, 1023, 5000);
  ASSERT_EQ(7L, numRead);
  buf[7] = '\0';
  ASSERT_TRUE(strcmp(buf, "* PING\n") == 0);

  // Respond to the heartbeat of 1 second
  const auto pong = "* PONG 1000\n";
  auto written = (size_t)m_serverSocket->write(pong, strlen(pong));
  ASSERT_EQ(strlen(pong), written);
  this_thread::sleep_for(1000ms);

  ASSERT_TRUE(m_connector->heartbeats());
  ASSERT_EQ(std::chrono::milliseconds{1000}, m_connector->heartbeatFrequency());
  // TODO END Copy&Paste from Heartbeat

  auto last_heartbeat = system_clock::now();

  // Test to make sure we can send and receive 5 heartbeats
  for (int i = 0; i < 5; i++) {
    // Receive initial heartbeat request "* PING\n"

    char buf[1024] = {0};
    ASSERT_TRUE(m_serverSocket->read(buf, 1023, 1100) > 0);
    buf[7] = '\0';
    ASSERT_TRUE(!strcmp(buf, "* PING\n"));

    auto now = system_clock::now();
    ASSERT_TRUE(now - last_heartbeat < 2000ms);
    last_heartbeat = now;

    // Respond to the heartbeat of 1 second
    const auto pong = "* PONG 1000\n";
    auto written = (size_t)m_serverSocket->write(pong, strlen(pong));
    ASSERT_EQ(strlen(pong), written);
    this_thread::sleep_for(10ms);

    ASSERT_TRUE(!m_connector->m_disconnected);
  }
}

TEST_F(ConnectorTest, HeartbeatTimeout) {
  // TODO Copy&Paste from Heartbeat
  // Start the accept thread
  start();

  auto res = m_server->accept(m_serverSocket);
  ASSERT_EQ(0, res);
  ASSERT_TRUE(m_serverSocket.get());

  // Receive initial heartbeat request "* PING\n"
  char buf[1024] = {0};
  auto numRead = m_serverSocket->read(buf, 1023, 5000);
  ASSERT_EQ(7L, numRead);
  buf[7] = '\0';
  ASSERT_TRUE(strcmp(buf, "* PING\n") == 0);

  // Respond to the heartbeat of 1 second
  const auto pong = "* PONG 1000\n";
  auto written = (size_t)m_serverSocket->write(pong, strlen(pong));
  ASSERT_EQ(strlen(pong), written);
  this_thread::sleep_for(1000ms);

  ASSERT_TRUE(m_connector->heartbeats());
  ASSERT_EQ(std::chrono::milliseconds{1000}, m_connector->heartbeatFrequency());
  // TODO END Copy&Paste from Heartbeat

  this_thread::sleep_for(2100ms);

  ASSERT_TRUE(m_connector->m_disconnected);
}

TEST_F(ConnectorTest, LegacyTimeout) {
  // Start the accept thread
  start();

  auto res = m_server->accept(m_serverSocket);
  ASSERT_EQ(0, res);
  ASSERT_TRUE(m_serverSocket.get());

  char buf[1024] = {0};
  auto numRead = m_serverSocket->read(buf, 1023, 5000);
  ASSERT_EQ(7L, numRead);
  buf[7] = '\0';
  ASSERT_TRUE(!strcmp(buf, "* PING\n"));

  // Write some data...
  const auto cmd = "* Hello Connector\n";
  auto written = (size_t)m_serverSocket->write(cmd, strlen(cmd));
  ASSERT_EQ(strlen(cmd), written);

  // No pings, but timeout after 5 seconds of silence
  this_thread::sleep_for(11000ms);

  ASSERT_TRUE(m_connector->m_disconnected);
}

TEST_F(ConnectorTest, ParseBuffer) {
  // Test data fragmentation
  m_connector->pushData("Hello");
  ASSERT_EQ((string) "", m_connector->m_data);

  m_connector->pushData(" There\n");
  ASSERT_EQ((string) "Hello There", m_connector->m_data);
  m_connector->m_data.clear();

  m_connector->pushData("Hello");
  ASSERT_EQ((string) "", m_connector->m_data);

  m_connector->pushData(" There\nAnd ");
  ASSERT_EQ((string) "Hello There", m_connector->m_data);

  m_connector->pushData("Again\nXXX");
  ASSERT_EQ((string) "And Again", m_connector->m_data);
}

TEST_F(ConnectorTest, ParseBufferFraming) {
  m_connector->m_list.clear();
  m_connector->pushData("first\nseco");
  m_connector->pushData("nd\nthird\nfourth\nfifth");
  ASSERT_EQ((size_t)4, m_connector->m_list.size());
  ASSERT_EQ((string) "first", m_connector->m_list[0]);
  ASSERT_EQ((string) "second", m_connector->m_list[1]);
  ASSERT_EQ((string) "third", m_connector->m_list[2]);
  ASSERT_EQ((string) "fourth", m_connector->m_list[3]);
}

TEST_F(ConnectorTest, SendCommand) {
  // Start the accept thread
  start();

  auto res = m_server->accept(m_serverSocket);
  ASSERT_EQ(0, res);
  ASSERT_TRUE(m_serverSocket.get());

  // Receive initial heartbeat request "* PING\n"
  char buf[1024];
  auto numRead = m_serverSocket->read(buf, 1023, 1000);
  ASSERT_EQ(7L, numRead);
  buf[7] = '\0';
  ASSERT_TRUE(!strcmp(buf, "* PING\n"));
  this_thread::sleep_for(200ms);

  ASSERT_TRUE(m_connector->isConnected());
  m_connector->sendCommand("Hello There;");

  auto len = m_serverSocket->read(buf, 1023, 1000);
  ASSERT_EQ(15L, len);
  buf[15] = '\0';
  ASSERT_TRUE(!strcmp(buf, "* Hello There;\n"));
}

TEST_F(ConnectorTest, IPV6Connection) {
// TODO: Need to port to Windows > VISTA
#if !defined(WIN32)
  m_connector.reset();

  ASSERT_TRUE(!create_listener(m_server, 0, "::1"));
  m_port = m_server->get_listening_port();
  m_connector.reset(new TestConnector("::1", m_port));
  m_connector->m_disconnected = true;

  ASSERT_TRUE(m_connector->m_disconnected);
  start();

  auto res = m_server->accept(m_serverSocket);
  ASSERT_EQ(0, res);
  this_thread::sleep_for(100ms);
  ASSERT_TRUE(m_serverSocket.get());
  ASSERT_TRUE(!m_connector->m_disconnected);
#endif
}

TEST_F(ConnectorTest, StartHeartbeats) {
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
  ASSERT_EQ(std::chrono::milliseconds{123}, m_connector->heartbeatFrequency());

  m_connector->resetHeartbeats();

  line = "* PONG       456 ";
  m_connector->startHeartbeats(line);

  ASSERT_TRUE(m_connector->heartbeats());
  ASSERT_EQ(std::chrono::milliseconds{456}, m_connector->heartbeatFrequency());

  line = "* PONG 323";
  m_connector->startHeartbeats(line);

  ASSERT_TRUE(m_connector->heartbeats());
  ASSERT_EQ(std::chrono::milliseconds{323}, m_connector->heartbeatFrequency());
}
