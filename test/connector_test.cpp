/*
* Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the AMT nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* DISCLAIMER OF WARRANTY. ALL MTCONNECT MATERIALS AND SPECIFICATIONS PROVIDED
* BY AMT, MTCONNECT OR ANY PARTICIPANT TO YOU OR ANY PARTY ARE PROVIDED "AS IS"
* AND WITHOUT ANY WARRANTY OF ANY KIND. AMT, MTCONNECT, AND EACH OF THEIR
* RESPECTIVE MEMBERS, OFFICERS, DIRECTORS, AFFILIATES, SPONSORS, AND AGENTS
* (COLLECTIVELY, THE "AMT PARTIES") AND PARTICIPANTS MAKE NO REPRESENTATION OR
* WARRANTY OF ANY KIND WHATSOEVER RELATING TO THESE MATERIALS, INCLUDING, WITHOUT
* LIMITATION, ANY EXPRESS OR IMPLIED WARRANTY OF NONINFRINGEMENT,
* MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. 

* LIMITATION OF LIABILITY. IN NO EVENT SHALL AMT, MTCONNECT, ANY OTHER AMT
* PARTY, OR ANY PARTICIPANT BE LIABLE FOR THE COST OF PROCURING SUBSTITUTE GOODS
* OR SERVICES, LOST PROFITS, LOSS OF USE, LOSS OF DATA OR ANY INCIDENTAL,
* CONSEQUENTIAL, INDIRECT, SPECIAL OR PUNITIVE DAMAGES OR OTHER DIRECT DAMAGES,
* WHETHER UNDER CONTRACT, TORT, WARRANTY OR OTHERWISE, ARISING IN ANY WAY OUT OF
* THIS AGREEMENT, USE OR INABILITY TO USE MTCONNECT MATERIALS, WHETHER OR NOT
* SUCH PARTY HAD ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.
*/

#include "connector_test.hpp"

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(ConnectorTest);

using namespace std;

/* ConnectorTest public methods */
void ConnectorTest::setUp()
{
  CPPUNIT_ASSERT(create_listener(mServer, 0, "127.0.0.1") == 0);
  mPort = mServer->get_listening_port();
  mConnector.reset(new TestConnector("127.0.0.1", mPort));
}

void ConnectorTest::thread()
{
  mConnector->connect();
}

void ConnectorTest::tearDown()
{
  mServer.reset();
  mServerSocket.reset();
  stop();
  wait();
  mConnector.reset();
}

/* ConnectorTest protected methods */
void ConnectorTest::testConnection()
{
  start();

  CPPUNIT_ASSERT_EQUAL(0, mServer->accept(mServerSocket));
  CPPUNIT_ASSERT(mServerSocket.get() != NULL);
}

void ConnectorTest::testDataCapture()
{
  // Start the accept thread
  start();
  
  CPPUNIT_ASSERT_EQUAL(0, mServer->accept(mServerSocket));
  CPPUNIT_ASSERT(mServerSocket.get() != NULL);

  string command("Hello Connector\n");
  CPPUNIT_ASSERT((size_t) mServerSocket->write(command.c_str(), command.length()) == command.length());
  dlib::sleep(1000);

  // \n is stripped from the posted data.
  CPPUNIT_ASSERT_EQUAL(command.substr(0, command.length() - 1), mConnector->mData); 
}

void ConnectorTest::testDisconnect()
{
  // Start the accept thread
  start();
  
  CPPUNIT_ASSERT_EQUAL(0, mServer->accept(mServerSocket));
  CPPUNIT_ASSERT(mServerSocket.get() != NULL);
  CPPUNIT_ASSERT(!mConnector->mDisconnected);
  mServerSocket.reset();
  dlib::sleep(500);
  CPPUNIT_ASSERT(mConnector->mDisconnected);
}

void ConnectorTest::testProtocolCommand()
{
  // Start the accept thread
  start();
  
  CPPUNIT_ASSERT_EQUAL(0, mServer->accept(mServerSocket));
  CPPUNIT_ASSERT(mServerSocket.get() != NULL);

  const char *cmd ="* Hello Connector\n";
  CPPUNIT_ASSERT_EQUAL(strlen(cmd), (size_t) mServerSocket->write(cmd, strlen(cmd)));
  dlib::sleep(1000);

  // \n is stripped from the posted data.
  CPPUNIT_ASSERT(strncmp(cmd, mConnector->mCommand.c_str(), strlen(cmd) - 1) == 0); 
}

void ConnectorTest::testHeartbeat()
{
  // Start the accept thread
  start();
  
  CPPUNIT_ASSERT_EQUAL(0, mServer->accept(mServerSocket));
  CPPUNIT_ASSERT(mServerSocket.get() != NULL);

  // Receive initial heartbeat request "* PING\n"
  char buf[1024];
  CPPUNIT_ASSERT_EQUAL(7L, mServerSocket->read(buf, 1023, 5000));
  buf[7] = '\0';
  CPPUNIT_ASSERT(strcmp(buf, "* PING\n") == 0);

  // Respond to the heartbeat of 1/2 second
  const char *pong = "* PONG 1000\n";
  CPPUNIT_ASSERT_EQUAL(strlen(pong), (size_t) mServerSocket->write(pong, strlen(pong)));
  dlib::sleep(1000);
  
  CPPUNIT_ASSERT(mConnector->heartbeats());
  CPPUNIT_ASSERT_EQUAL(1000, mConnector->heartbeatFrequency());
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
    CPPUNIT_ASSERT(mServerSocket->read(buf, 1023, 1000) > 0);
    buf[7] = '\0';
    CPPUNIT_ASSERT(strcmp(buf, "* PING\n") == 0);

    uint64 now = stamper.get_timestamp();
    CPPUNIT_ASSERT(now - last_heartbeat < (uint64) (1000 * 2000));
    last_heartbeat = now;
    
    // Respond to the heartbeat of 1/2 second
    const char *pong = "* PONG 1000\n";
    CPPUNIT_ASSERT_EQUAL(strlen(pong), (size_t) mServerSocket->write(pong, strlen(pong)));
    dlib::sleep(10);

    CPPUNIT_ASSERT(!mConnector->mDisconnected);
  }
}

void ConnectorTest::testHeartbeatTimeout()
{
  testHeartbeat();
  dlib::sleep(2100);
  
  CPPUNIT_ASSERT(mConnector->mDisconnected);
}


void ConnectorTest::testParseBuffer()
{
  // Test data fragmentation
  mConnector->pushData("Hello");
  CPPUNIT_ASSERT_EQUAL((string) "", mConnector->mData);

  mConnector->pushData(" There\n");
  CPPUNIT_ASSERT_EQUAL((string) "Hello There", mConnector->mData);
  mConnector->mData.clear();

  mConnector->pushData("Hello");
  CPPUNIT_ASSERT_EQUAL((string) "", mConnector->mData);

  mConnector->pushData(" There\nAnd ");
  CPPUNIT_ASSERT_EQUAL((string) "Hello There", mConnector->mData);

  mConnector->pushData("Again\nXXX");
  CPPUNIT_ASSERT_EQUAL((string) "And Again", mConnector->mData);
}
  
void ConnectorTest::testParseBufferFraming()
{
  mConnector->mList.clear();
  mConnector->pushData("first\nseco");
  mConnector->pushData("nd\nthird\nfourth\nfifth");
  CPPUNIT_ASSERT_EQUAL((size_t) 4, mConnector->mList.size());
  CPPUNIT_ASSERT_EQUAL((string) "first", mConnector->mList[0]);
  CPPUNIT_ASSERT_EQUAL((string) "second", mConnector->mList[1]);
  CPPUNIT_ASSERT_EQUAL((string) "third", mConnector->mList[2]);
  CPPUNIT_ASSERT_EQUAL((string) "fourth", mConnector->mList[3]);
}

void ConnectorTest::testSendCommand()
{
  // Start the accept thread
  start();
  
  CPPUNIT_ASSERT_EQUAL(0, mServer->accept(mServerSocket));
  CPPUNIT_ASSERT(mServerSocket.get() != NULL);
  
  // Receive initial heartbeat request "* PING\n"
  char buf[1024];
  CPPUNIT_ASSERT_EQUAL(7L, mServerSocket->read(buf, 1023, 5000));
  buf[7] = '\0';
  CPPUNIT_ASSERT(strcmp(buf, "* PING\n") == 0);
  
  mConnector->sendCommand("Hello There;");

  CPPUNIT_ASSERT_EQUAL(15L, mServerSocket->read(buf, 1023, 5000));
  buf[15] = '\0';
  CPPUNIT_ASSERT(strcmp(buf, "* Hello There;\n") == 0);  
}
