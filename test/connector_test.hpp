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

#ifndef COMPONENT_EVENT_TEST_HPP
#define COMPONENT_EVENT_TEST_HPP

#include <map>
#include <string>
#include <vector>
#include <dlib/sockets.h>

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "connector.hpp"

// Simple subclass for testing
class TestConnector : public Connector 
{
public:
  TestConnector(const std::string& server, unsigned int port,
                int aLegacyTimeout = 5)
    : Connector(server, port, aLegacyTimeout), mDisconnected(false) {}
  
  virtual void processData(const std::string& data) {
    mData = data;
    mList.push_back(mData);
  }

  virtual void protocolCommand(const std::string& data) {
    mCommand = data;
  }

  virtual void disconnected() { mDisconnected = true; }
  virtual void connected() { mDisconnected = false; }
  bool heartbeats() { return mHeartbeats; }

  void pushData(const char *data) { parseBuffer(data); }
  
  void startHeartbeats(std::string &aString) { Connector::startHeartbeats(aString); }
  void resetHeartbeats() { mHeartbeats = false; }

public:
  std::vector<std::string> mList;
  std::string mData;
  std::string mCommand;
  bool mDisconnected;
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
  dlib::scoped_ptr<dlib::listener> mServer;
  dlib::scoped_ptr<dlib::connection> mServerSocket;
  dlib::scoped_ptr<TestConnector> mConnector;
  unsigned short mPort;
  
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

#endif

