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
#include "Cuti.h"

#include <sstream>
#include <chrono>
#include <date/date.h> // This file is to allow std::chrono types to be output to a stream
#include <thread>
#include "connector.hpp"


using namespace std;
using namespace std::chrono;
using namespace mtconnect;

class TestConnector : public Connector
{
public:
  TestConnector(
                const std::string &server,
                unsigned int port,
                std::chrono::seconds legacyTimeout = std::chrono::seconds{5})
  :
  Connector(server, port, legacyTimeout),
  m_disconnected(false)
  {
  }
  
  void processData(const std::string &data) override
  {
    m_data = data;
    m_list.push_back(m_data);
  }
  
  void protocolCommand(const std::string &data) override
  {
    m_command = data;
  }
  
  void disconnected() override {
    m_disconnected = true; }
  void connected() override {
    m_disconnected = false; }
  bool heartbeats() {
    return m_heartbeats; }
  
  void pushData(const char *data) {
    parseBuffer(data); }
  
  void startHeartbeats(std::string &aString) {
    Connector::startHeartbeats(aString); }
  void resetHeartbeats() {
    m_heartbeats = false; }
  
public:
  std::vector<std::string> m_list;
  std::string m_data;
  std::string m_command;
  bool m_disconnected;
};

TEST_CLASS(ConnectorTest), dlib::threaded_object
{
protected:
  dlib::scoped_ptr<dlib::listener> m_server;
  dlib::scoped_ptr<dlib::connection> m_serverSocket;
  dlib::scoped_ptr<TestConnector> m_connector;
  unsigned short m_port;
  
public:
  SET_UP();
  TEAR_DOWN();
  virtual void thread();
  
  void testConnection();
  void testDataCapture();
  void testDisconnect();
  void testProtocolCommand();
  void testHeartbeat();
  void testHeartbeatPong();
  void testHeartbeatTimeout();
  void testParseBuffer();
  void testParseBufferFraming();
  void testSendCommand();
  void testLegacyTimeout();
  void testIPV6Connection();
  void testStartHeartbeats();
  
  CPPUNIT_TEST_SUITE(ConnectorTest);
  CPPUNIT_TEST(testConnection);
  CPPUNIT_TEST(testDataCapture);
  CPPUNIT_TEST(testDisconnect);
  CPPUNIT_TEST(testProtocolCommand);
  CPPUNIT_TEST(testHeartbeat);
  CPPUNIT_TEST(testHeartbeatPong);
  CPPUNIT_TEST(testHeartbeatTimeout);
  CPPUNIT_TEST(testParseBuffer);
  CPPUNIT_TEST(testParseBufferFraming);
  CPPUNIT_TEST(testSendCommand);
  CPPUNIT_TEST(testLegacyTimeout);
  CPPUNIT_TEST(testIPV6Connection);
  CPPUNIT_TEST(testStartHeartbeats);
  CPPUNIT_TEST_SUITE_END();
};

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(ConnectorTest);

void ConnectorTest::setUp()
{
  CPPUNIT_ASSERT(!create_listener(m_server, 0, "127.0.0.1"));
  m_port = m_server->get_listening_port();
  m_connector.reset(new TestConnector("127.0.0.1", m_port));
  m_connector->m_disconnected = true;
}


void ConnectorTest::thread()
{
  m_connector->connect();
}


void ConnectorTest::tearDown()
{
  m_server.reset();
  m_serverSocket.reset();
  stop();
  wait();
  m_connector.reset();
}


void ConnectorTest::testConnection()
{
  CPPUNIT_ASSERT(m_connector->m_disconnected);
  start();
  
  CPPUNIT_ASSERT_EQUAL(0, m_server->accept(m_serverSocket));
  this_thread::sleep_for(100ms);
  CPPUNIT_ASSERT(m_serverSocket.get());
  CPPUNIT_ASSERT(!m_connector->m_disconnected);
}


void ConnectorTest::testDataCapture()
{
  // Start the accept thread
  start();
  
  CPPUNIT_ASSERT_EQUAL(0, m_server->accept(m_serverSocket));
  CPPUNIT_ASSERT(m_serverSocket.get());
  
  string command("Hello Connector\n");
  CPPUNIT_ASSERT((size_t) m_serverSocket->write(command.c_str(),
                                                command.length()) == command.length());
  this_thread::sleep_for(1000ms);
  
  // \n is stripped from the posted data.
  CPPUNIT_ASSERT_EQUAL(command.substr(0, command.length() - 1), m_connector->m_data);
}


void ConnectorTest::testDisconnect()
{
  // Start the accept thread
  start();
  
  CPPUNIT_ASSERT_EQUAL(0, m_server->accept(m_serverSocket));
  this_thread::sleep_for(1000ms);
  CPPUNIT_ASSERT(m_serverSocket.get());
  CPPUNIT_ASSERT(!m_connector->m_disconnected);
  m_serverSocket.reset();
  this_thread::sleep_for(1000ms);
  CPPUNIT_ASSERT(m_connector->m_disconnected);
}


void ConnectorTest::testProtocolCommand()
{
  // Start the accept thread
  start();
  
  CPPUNIT_ASSERT_EQUAL(0, m_server->accept(m_serverSocket));
  CPPUNIT_ASSERT(m_serverSocket.get());
  
  const auto cmd = "* Hello Connector\n";
  CPPUNIT_ASSERT_EQUAL(strlen(cmd), (size_t) m_serverSocket->write(cmd, strlen(cmd)));
  this_thread::sleep_for(1000ms);
  
  // \n is stripped from the posted data.
  CPPUNIT_ASSERT(strncmp(cmd, m_connector->m_command.c_str(), strlen(cmd) - 1) == 0);
}


void ConnectorTest::testHeartbeat()
{
  // Start the accept thread
  start();
  
  CPPUNIT_ASSERT_EQUAL(0, m_server->accept(m_serverSocket));
  CPPUNIT_ASSERT(m_serverSocket.get());
  
  // Receive initial heartbeat request "* PING\n"
  char buf[1024] = {0};
  CPPUNIT_ASSERT_EQUAL(7L, m_serverSocket->read(buf, 1023, 5000));
  buf[7] = '\0';
  CPPUNIT_ASSERT(strcmp(buf, "* PING\n") == 0);
  
  // Respond to the heartbeat of 1 second
  const auto pong = "* PONG 1000\n";
  CPPUNIT_ASSERT_EQUAL(strlen(pong), (size_t) m_serverSocket->write(pong, strlen(pong)));
  this_thread::sleep_for(1000ms);
  
  CPPUNIT_ASSERT(m_connector->heartbeats());
  CPPUNIT_ASSERT_EQUAL(std::chrono::milliseconds{1000}, m_connector->heartbeatFrequency());
}


void ConnectorTest::testHeartbeatPong()
{
  testHeartbeat();
  
  auto last_heartbeat = system_clock::now();
  
  // Test to make sure we can send and receive 5 heartbeats
  for (int i = 0; i < 5; i++)
  {
    // Receive initial heartbeat request "* PING\n"
    
    char buf[1024] = {0};
    CPPUNIT_ASSERT(m_serverSocket->read(buf, 1023, 1100) > 0);
    buf[7] = '\0';
    CPPUNIT_ASSERT(!strcmp(buf, "* PING\n"));
    
    auto now = system_clock::now();
    CPPUNIT_ASSERT(now - last_heartbeat < 2000ms);
    last_heartbeat = now;
    
    // Respond to the heartbeat of 1 second
    const auto pong = "* PONG 1000\n";
    CPPUNIT_ASSERT_EQUAL(strlen(pong), (size_t) m_serverSocket->write(pong, strlen(pong)));
    this_thread::sleep_for(10ms);
    
    CPPUNIT_ASSERT(!m_connector->m_disconnected);
  }
}


void ConnectorTest::testHeartbeatTimeout()
{
  testHeartbeat();
  this_thread::sleep_for(2100ms);
  
  CPPUNIT_ASSERT(m_connector->m_disconnected);
}


void ConnectorTest::testLegacyTimeout()
{
  // Start the accept thread
  start();
  
  CPPUNIT_ASSERT_EQUAL(0, m_server->accept(m_serverSocket));
  CPPUNIT_ASSERT(m_serverSocket.get());
  
  char buf[1024] = {0};
  CPPUNIT_ASSERT_EQUAL(7L, m_serverSocket->read(buf, 1023, 5000));
  buf[7] = '\0';
  CPPUNIT_ASSERT(!strcmp(buf, "* PING\n"));
  
  // Write some data...
  const auto cmd = "* Hello Connector\n";
  CPPUNIT_ASSERT_EQUAL(strlen(cmd), (size_t) m_serverSocket->write(cmd, strlen(cmd)));
  
  // No pings, but timeout after 5 seconds of silence
  this_thread::sleep_for(11000ms);
  
  CPPUNIT_ASSERT(m_connector->m_disconnected);
}


void ConnectorTest::testParseBuffer()
{
  // Test data fragmentation
  m_connector->pushData("Hello");
  CPPUNIT_ASSERT_EQUAL((string) "", m_connector->m_data);
  
  m_connector->pushData(" There\n");
  CPPUNIT_ASSERT_EQUAL((string) "Hello There", m_connector->m_data);
  m_connector->m_data.clear();
  
  m_connector->pushData("Hello");
  CPPUNIT_ASSERT_EQUAL((string) "", m_connector->m_data);
  
  m_connector->pushData(" There\nAnd ");
  CPPUNIT_ASSERT_EQUAL((string) "Hello There", m_connector->m_data);
  
  m_connector->pushData("Again\nXXX");
  CPPUNIT_ASSERT_EQUAL((string) "And Again", m_connector->m_data);
}


void ConnectorTest::testParseBufferFraming()
{
  m_connector->m_list.clear();
  m_connector->pushData("first\nseco");
  m_connector->pushData("nd\nthird\nfourth\nfifth");
  CPPUNIT_ASSERT_EQUAL((size_t) 4, m_connector->m_list.size());
  CPPUNIT_ASSERT_EQUAL((string) "first", m_connector->m_list[0]);
  CPPUNIT_ASSERT_EQUAL((string) "second", m_connector->m_list[1]);
  CPPUNIT_ASSERT_EQUAL((string) "third", m_connector->m_list[2]);
  CPPUNIT_ASSERT_EQUAL((string) "fourth", m_connector->m_list[3]);
}


void ConnectorTest::testSendCommand()
{
  // Start the accept thread
  start();
  
  CPPUNIT_ASSERT_EQUAL(0, m_server->accept(m_serverSocket));
  CPPUNIT_ASSERT(m_serverSocket.get());
  
  // Receive initial heartbeat request "* PING\n"
  char buf[1024];
  CPPUNIT_ASSERT_EQUAL(7L, m_serverSocket->read(buf, 1023, 1000));
  buf[7] = '\0';
  CPPUNIT_ASSERT(!strcmp(buf, "* PING\n"));
  
  m_connector->sendCommand("Hello There;");
  this_thread::sleep_for(200ms);
  auto len = m_serverSocket->read(buf, 1023, 1000);
  CPPUNIT_ASSERT_EQUAL(15L, len);
  buf[15] = '\0';
  CPPUNIT_ASSERT(!strcmp(buf, "* Hello There;\n"));
}


void ConnectorTest::testIPV6Connection()
{
#if !defined(WIN32) || (NTDDI_VERSION >= NTDDI_VISTA)
  m_connector.reset();
  
  CPPUNIT_ASSERT(!create_listener(m_server, 0, "::1"));
  m_port = m_server->get_listening_port();
  m_connector.reset(new TestConnector("::1", m_port));
  m_connector->m_disconnected = true;
  
  CPPUNIT_ASSERT(m_connector->m_disconnected);
  start();
  
  CPPUNIT_ASSERT_EQUAL(0, m_server->accept(m_serverSocket));
  this_thread::sleep_for(100ms);
  CPPUNIT_ASSERT(m_serverSocket.get());
  CPPUNIT_ASSERT(!m_connector->m_disconnected);
#endif
}


void ConnectorTest::testStartHeartbeats()
{
  CPPUNIT_ASSERT(!m_connector->heartbeats());
  
  string line = "* PONG ";
  m_connector->startHeartbeats(line);
  
  CPPUNIT_ASSERT(!m_connector->heartbeats());
  
  line = "* PONK ";
  m_connector->startHeartbeats(line);
  
  CPPUNIT_ASSERT(!m_connector->heartbeats());
  
  line = "* PONG      ";
  m_connector->startHeartbeats(line);
  
  CPPUNIT_ASSERT(!m_connector->heartbeats());
  
  line = "* PONG FLAB";
  m_connector->startHeartbeats(line);
  
  CPPUNIT_ASSERT(!m_connector->heartbeats());
  
  line = "* PONG       123";
  m_connector->startHeartbeats(line);
  
  CPPUNIT_ASSERT(m_connector->heartbeats());
  CPPUNIT_ASSERT_EQUAL(std::chrono::milliseconds{123}, m_connector->heartbeatFrequency());
  
  m_connector->resetHeartbeats();
  
  line = "* PONG       456 ";
  m_connector->startHeartbeats(line);
  
  CPPUNIT_ASSERT(m_connector->heartbeats());
  CPPUNIT_ASSERT_EQUAL(std::chrono::milliseconds{456}, m_connector->heartbeatFrequency());
  
  line = "* PONG 323";
  m_connector->startHeartbeats(line);
  
  CPPUNIT_ASSERT(m_connector->heartbeats());
  CPPUNIT_ASSERT_EQUAL(std::chrono::milliseconds{323}, m_connector->heartbeatFrequency());
}
