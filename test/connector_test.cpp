//
// Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the AMT nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// DISCLAIMER OF WARRANTY. ALL MTCONNECT MATERIALS AND SPECIFICATIONS PROVIDED
// BY AMT, MTCONNECT OR ANY PARTICIPANT TO YOU OR ANY PARTY ARE PROVIDED "AS IS"
// AND WITHOUT ANY WARRANTY OF ANY KIND. AMT, MTCONNECT, AND EACH OF THEIR
// RESPECTIVE MEMBERS, OFFICERS, DIRECTORS, AFFILIATES, SPONSORS, AND AGENTS
// (COLLECTIVELY, THE "AMT PARTIES") AND PARTICIPANTS MAKE NO REPRESENTATION OR
// WARRANTY OF ANY KIND WHATSOEVER RELATING TO THESE MATERIALS, INCLUDING, WITHOUT
// LIMITATION, ANY EXPRESS OR IMPLIED WARRANTY OF NONINFRINGEMENT,
// MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
//
// LIMITATION OF LIABILITY. IN NO EVENT SHALL AMT, MTCONNECT, ANY OTHER AMT
// PARTY, OR ANY PARTICIPANT BE LIABLE FOR THE COST OF PROCURING SUBSTITUTE GOODS
// OR SERVICES, LOST PROFITS, LOSS OF USE, LOSS OF DATA OR ANY INCIDENTAL,
// CONSEQUENTIAL, INDIRECT, SPECIAL OR PUNITIVE DAMAGES OR OTHER DIRECT DAMAGES,
// WHETHER UNDER CONTRACT, TORT, WARRANTY OR OTHERWISE, ARISING IN ANY WAY OUT OF
// THIS AGREEMENT, USE OR INABILITY TO USE MTCONNECT MATERIALS, WHETHER OR NOT
// SUCH PARTY HAD ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.
//
#include "connector_test.hpp"

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(ConnectorTest);

using namespace std;

// ConnectorTest public methods
void ConnectorTest::setUp()
{
	CPPUNIT_ASSERT(create_listener(m_server, 0, "127.0.0.1") == 0);
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

// ConnectorTest protected methods
void ConnectorTest::testConnection()
{
	CPPUNIT_ASSERT(m_connector->m_disconnected);
	start();

	CPPUNIT_ASSERT_EQUAL(0, m_server->accept(m_serverSocket));
	dlib::sleep(100);
	CPPUNIT_ASSERT(m_serverSocket.get() != NULL);
	CPPUNIT_ASSERT(!m_connector->m_disconnected);
}

void ConnectorTest::testDataCapture()
{
	// Start the accept thread
	start();

	CPPUNIT_ASSERT_EQUAL(0, m_server->accept(m_serverSocket));
	CPPUNIT_ASSERT(m_serverSocket.get() != NULL);

	string command("Hello Connector\n");
	CPPUNIT_ASSERT((size_t) m_serverSocket->write(command.c_str(),
		   command.length()) == command.length());
	dlib::sleep(1000);

	// \n is stripped from the posted data.
	CPPUNIT_ASSERT_EQUAL(command.substr(0, command.length() - 1), m_connector->m_data);
}

void ConnectorTest::testDisconnect()
{
	// Start the accept thread
	start();

	CPPUNIT_ASSERT_EQUAL(0, m_server->accept(m_serverSocket));
	dlib::sleep(1000);
	CPPUNIT_ASSERT(m_serverSocket.get() != NULL);
	CPPUNIT_ASSERT(!m_connector->m_disconnected);
	m_serverSocket.reset();
	dlib::sleep(1000);
	CPPUNIT_ASSERT(m_connector->m_disconnected);
}

void ConnectorTest::testProtocolCommand()
{
	// Start the accept thread
	start();

	CPPUNIT_ASSERT_EQUAL(0, m_server->accept(m_serverSocket));
	CPPUNIT_ASSERT(m_serverSocket.get() != NULL);

	const char *cmd = "* Hello Connector\n";
	CPPUNIT_ASSERT_EQUAL(strlen(cmd), (size_t) m_serverSocket->write(cmd, strlen(cmd)));
	dlib::sleep(1000);

	// \n is stripped from the posted data.
	CPPUNIT_ASSERT(strncmp(cmd, m_connector->m_command.c_str(), strlen(cmd) - 1) == 0);
}

void ConnectorTest::testHeartbeat()
{
	// Start the accept thread
	start();

	CPPUNIT_ASSERT_EQUAL(0, m_server->accept(m_serverSocket));
	CPPUNIT_ASSERT(m_serverSocket.get() != NULL);

	// Receive initial heartbeat request "* PING\n"
	char buf[1024];
	CPPUNIT_ASSERT_EQUAL(7L, m_serverSocket->read(buf, 1023, 5000));
	buf[7] = '\0';
	CPPUNIT_ASSERT(strcmp(buf, "* PING\n") == 0);

	// Respond to the heartbeat of 1/2 second
	const char *pong = "* PONG 1000\n";
	CPPUNIT_ASSERT_EQUAL(strlen(pong), (size_t) m_serverSocket->write(pong, strlen(pong)));
	dlib::sleep(1000);

	CPPUNIT_ASSERT(m_connector->heartbeats());
	CPPUNIT_ASSERT_EQUAL(1000, m_connector->heartbeatFrequency());
}

void ConnectorTest::testHeartbeatPong()
{
	testHeartbeat();

	dlib::timestamper stamper;
	uint64 last_heartbeat = stamper.get_timestamp();

	// Test to make sure we can send and receive 5 heartbeats
	for (int i = 0; i < 5; i++)
	{
	// Receive initial heartbeat request "* PING\n"

	char buf[1024];
	CPPUNIT_ASSERT(m_serverSocket->read(buf, 1023, 1100) > 0);
	buf[7] = '\0';
	CPPUNIT_ASSERT(strcmp(buf, "* PING\n") == 0);

	uint64 now = stamper.get_timestamp();
	CPPUNIT_ASSERT(now - last_heartbeat < (uint64)(1000 * 2000));
	last_heartbeat = now;

	// Respond to the heartbeat of 1/2 second
	const char *pong = "* PONG 1000\n";
	CPPUNIT_ASSERT_EQUAL(strlen(pong), (size_t) m_serverSocket->write(pong, strlen(pong)));
	dlib::sleep(10);

	CPPUNIT_ASSERT(!m_connector->m_disconnected);
	}
}

void ConnectorTest::testHeartbeatTimeout()
{
	testHeartbeat();
	dlib::sleep(2100);

	CPPUNIT_ASSERT(m_connector->m_disconnected);
}

void ConnectorTest::testLegacyTimeout()
{
	// Start the accept thread
	start();

	CPPUNIT_ASSERT_EQUAL(0, m_server->accept(m_serverSocket));
	CPPUNIT_ASSERT(m_serverSocket.get() != NULL);

	char buf[1024];
	CPPUNIT_ASSERT_EQUAL(7L, m_serverSocket->read(buf, 1023, 5000));
	buf[7] = '\0';
	CPPUNIT_ASSERT(strcmp(buf, "* PING\n") == 0);

	// Write some data...
	const char *cmd = "* Hello Connector\n";
	CPPUNIT_ASSERT_EQUAL(strlen(cmd), (size_t) m_serverSocket->write(cmd, strlen(cmd)));

	// No pings, but timeout after 5 seconds of silence
	dlib::sleep(11000);

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
	CPPUNIT_ASSERT(m_serverSocket.get() != NULL);

	// Receive initial heartbeat request "* PING\n"
	char buf[1024];
	CPPUNIT_ASSERT_EQUAL(7L, m_serverSocket->read(buf, 1023, 1000));
	buf[7] = '\0';
	CPPUNIT_ASSERT(strcmp(buf, "* PING\n") == 0);

	m_connector->sendCommand("Hello There;");

	CPPUNIT_ASSERT_EQUAL(15L, m_serverSocket->read(buf, 1023, 1000));
	buf[15] = '\0';
	CPPUNIT_ASSERT(strcmp(buf, "* Hello There;\n") == 0);
}

void ConnectorTest::testIPV6Connection()
{
#if !defined(WIN32) || (NTDDI_VERSION >= NTDDI_VISTA)
	m_connector.reset();

	CPPUNIT_ASSERT(create_listener(m_server, 0, "::1") == 0);
	m_port = m_server->get_listening_port();
	m_connector.reset(new TestConnector("::1", m_port));
	m_connector->m_disconnected = true;

	CPPUNIT_ASSERT(m_connector->m_disconnected);
	start();

	CPPUNIT_ASSERT_EQUAL(0, m_server->accept(m_serverSocket));
	dlib::sleep(100);
	CPPUNIT_ASSERT(m_serverSocket.get() != NULL);
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
	CPPUNIT_ASSERT_EQUAL(123, m_connector->heartbeatFrequency());

	m_connector->resetHeartbeats();

	line = "* PONG       456 ";
	m_connector->startHeartbeats(line);

	CPPUNIT_ASSERT(m_connector->heartbeats());
	CPPUNIT_ASSERT_EQUAL(456, m_connector->heartbeatFrequency());

	line = "* PONG 323";
	m_connector->startHeartbeats(line);

	CPPUNIT_ASSERT(m_connector->heartbeats());
	CPPUNIT_ASSERT_EQUAL(323, m_connector->heartbeatFrequency());
}
