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

#include "config_test.hpp"
#include "agent.hpp"
#include "adapter.hpp"
#include <sstream>

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(ConfigTest);

using namespace std;

/* ComponentTest public methods */
void ConfigTest::setUp()
{
  mConfig = new AgentConfiguration();
}

void ConfigTest::tearDown()
{
  delete mConfig;
  mConfig = NULL;
}

/* ComponentTest protected methods */
void ConfigTest::testBlankConfig()
{
  istringstream str("");
  mConfig->loadConfig(str);
  
  Agent *agent = mConfig->getAgent();
  CPPUNIT_ASSERT(agent);
  CPPUNIT_ASSERT_EQUAL(1, (int) agent->getDevices().size());
}

void ConfigTest::testBufferSize()
{
  istringstream str("BufferSize = 4\n");
  mConfig->loadConfig(str);
  
  Agent *agent = mConfig->getAgent();
  CPPUNIT_ASSERT(agent);
  CPPUNIT_ASSERT_EQUAL(16, (int) agent->getBufferSize());
}

void ConfigTest::testDevice()
{
  istringstream str("Devices = ../samples/test_config.xml\n");
  mConfig->loadConfig(str);
  
  Agent *agent = mConfig->getAgent();
  CPPUNIT_ASSERT(agent);
  Device *device = agent->getDevices()[0];
  Adapter *adapter = device->mAdapters[0];

  CPPUNIT_ASSERT_EQUAL((string) "LinuxCNC", device->getName());
  CPPUNIT_ASSERT(!adapter->isDupChecking());
  CPPUNIT_ASSERT(!adapter->isAutoAvailable());
  CPPUNIT_ASSERT(!adapter->isIgnoringTimestamps());
  CPPUNIT_ASSERT(!device->mPreserveUuid);
}

void ConfigTest::testAdapter()
{
  istringstream str("Devices = ../samples/test_config.xml\n"
                    "Adapters { LinuxCNC { \n"
                    "Port = 23\n"
                    "Host = 10.211.55.1\n"
                    "FilterDuplicates = true\n"
                    "AutoAvailable = true\n"
                    "IgnoreTimestamps = true\n"
                    "PreserveUUID = true\n"
                    "} }\n");
  mConfig->loadConfig(str);
  
  Agent *agent = mConfig->getAgent();
  CPPUNIT_ASSERT(agent);
  Device *device = agent->getDevices()[0];
  Adapter *adapter = device->mAdapters[0];

  CPPUNIT_ASSERT_EQUAL(23, (int) adapter->getPort());
  CPPUNIT_ASSERT_EQUAL((string) "10.211.55.1", adapter->getServer());
  CPPUNIT_ASSERT(adapter->isDupChecking());
  CPPUNIT_ASSERT(adapter->isAutoAvailable());
  CPPUNIT_ASSERT(adapter->isIgnoringTimestamps());
  CPPUNIT_ASSERT(device->mPreserveUuid);
}

void ConfigTest::testDefaultPreserveUUID()
{
  istringstream str("Devices = ../samples/test_config.xml\n"
                    "PreserveUUID = true\n");
  mConfig->loadConfig(str);
  
  Agent *agent = mConfig->getAgent();
  CPPUNIT_ASSERT(agent);
  Device *device = agent->getDevices()[0];

  CPPUNIT_ASSERT(device->mPreserveUuid);
}

void ConfigTest::testDefaultPreserveOverride()
{
  istringstream str("Devices = ../samples/test_config.xml\n"
                    "PreserveUUID = true\n"
                    "Adapters { LinuxCNC { \n"
                    "PreserveUUID = false\n"
                    "} }\n");
  mConfig->loadConfig(str);
  
  Agent *agent = mConfig->getAgent();
  CPPUNIT_ASSERT(agent);
  Device *device = agent->getDevices()[0];

  CPPUNIT_ASSERT(!device->mPreserveUuid);
}

void ConfigTest::testDisablePut()
{
  istringstream str("Devices = ../samples/test_config.xml\n"
                    "AllowPut = true\n");
  mConfig->loadConfig(str);
  
  Agent *agent = mConfig->getAgent();
  CPPUNIT_ASSERT(agent);

  CPPUNIT_ASSERT(agent->isPutEnabled());
}

void ConfigTest::testLimitPut()
{
  istringstream str("Devices = ../samples/test_config.xml\n"
                    "AllowPutFrom = localhost\n");
  mConfig->loadConfig(str);
  
  Agent *agent = mConfig->getAgent();
  CPPUNIT_ASSERT(agent);
  
  CPPUNIT_ASSERT(agent->isPutAllowedFrom((string) "127.0.0.1"));
}

