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

#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <dlib/sockets.h>

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "connector.hpp"

namespace mtconnect {
  namespace test {
// Simple subclass for testing
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

class ConnectorTest : public CppUnit::TestFixture, dlib::threaded_object
{
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

public:
	void setUp();
	void tearDown();
	virtual void thread();

protected:
	dlib::scoped_ptr<dlib::listener> m_server;
	dlib::scoped_ptr<dlib::connection> m_serverSocket;
	dlib::scoped_ptr<TestConnector> m_connector;
	unsigned short m_port;

protected:
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
};
  }
}
