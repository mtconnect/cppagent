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
#include "rolling_file_logger.hpp"
#include <sstream>
#include <dlib/dir_nav.h>

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(ConfigTest);

static dlib::logger sLogger("config_test");

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
  CPPUNIT_ASSERT(device->mPreserveUuid);
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
                    "LegacyTimeout = 2000\n"
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
  CPPUNIT_ASSERT_EQUAL(2000, adapter->getLegacyTimeout());
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
  CPPUNIT_ASSERT(agent->isPutEnabled());
  CPPUNIT_ASSERT(agent->isPutAllowedFrom((string) "127.0.0.1"));
}

void ConfigTest::testLimitPutFromHosts()
{
  istringstream str("Devices = ../samples/test_config.xml\n"
                    "AllowPutFrom = localhost, 192.168.0.1\n");
  mConfig->loadConfig(str);
  
  Agent *agent = mConfig->getAgent();
  CPPUNIT_ASSERT(agent);
  CPPUNIT_ASSERT(agent->isPutEnabled());
  CPPUNIT_ASSERT(agent->isPutAllowedFrom((string) "127.0.0.1"));
  CPPUNIT_ASSERT(agent->isPutAllowedFrom((string) "192.168.0.1"));
}

void ConfigTest::testNamespaces()
{
  istringstream streams("StreamsNamespaces {\n"
                    "x {\n"
                    "Urn = urn:example.com:ExampleStreams:1.2\n"
                    "Location = /schemas/ExampleStreams_1.2.xsd\n"
                    "Path = ./ExampleStreams_1.2.xsd\n"
                    "}\n"
                    "}\n");
  
  mConfig->loadConfig(streams);  
  string path = XmlPrinter::getStreamsUrn("x");
  CPPUNIT_ASSERT_EQUAL((string) "urn:example.com:ExampleStreams:1.2", path);

  istringstream devices("DevicesNamespaces {\n"
                        "y {\n"
                        "Urn = urn:example.com:ExampleDevices:1.2\n"
                        "Location = /schemas/ExampleDevices_1.2.xsd\n"
                        "Path = ./ExampleDevices_1.2.xsd\n"
                        "}\n"
                        "}\n");
  
  mConfig->loadConfig(devices);  
  path = XmlPrinter::getDevicesUrn("y");
  CPPUNIT_ASSERT_EQUAL((string) "urn:example.com:ExampleDevices:1.2", path);


  istringstream assets("AssetsNamespaces {\n"
                        "z {\n"
                        "Urn = urn:example.com:ExampleAssets:1.2\n"
                        "Location = /schemas/ExampleAssets_1.2.xsd\n"
                        "Path = ./ExampleAssets_1.2.xsd\n"
                        "}\n"
                        "}\n");
  
  mConfig->loadConfig(assets);  
  path = XmlPrinter::getAssetsUrn("z");
  CPPUNIT_ASSERT_EQUAL((string) "urn:example.com:ExampleAssets:1.2", path);

  istringstream errors("ErrorNamespaces {\n"
                       "a {\n"
                       "Urn = urn:example.com:ExampleErrors:1.2\n"
                       "Location = /schemas/ExampleErrors_1.2.xsd\n"
                       "Path = ./ExampleErrorss_1.2.xsd\n"
                       "}\n"
                       "}\n");
  
  mConfig->loadConfig(errors);  
  path = XmlPrinter::getErrorUrn("a");
  CPPUNIT_ASSERT_EQUAL((string) "urn:example.com:ExampleErrors:1.2", path);

  XmlPrinter::clearDevicesNamespaces();
  XmlPrinter::clearErrorNamespaces();
  XmlPrinter::clearStreamsNamespaces();
  XmlPrinter::clearAssetsNamespaces();
}

void ConfigTest::testLegacyTimeout()
{
  istringstream str("Devices = ../samples/test_config.xml\n"
                    "LegacyTimeout = 2000\n");
  mConfig->loadConfig(str);
  
  Agent *agent = mConfig->getAgent();
  CPPUNIT_ASSERT(agent);
  Device *device = agent->getDevices()[0];
  Adapter *adapter = device->mAdapters[0];
  
  CPPUNIT_ASSERT_EQUAL(2000, adapter->getLegacyTimeout());
}

void ConfigTest::testIgnoreTimestamps()
{
  istringstream str("Devices = ../samples/test_config.xml\n"
                    "IgnoreTimestamps = true\n");
  mConfig->loadConfig(str);
  
  Agent *agent = mConfig->getAgent();
  CPPUNIT_ASSERT(agent);
  Device *device = agent->getDevices()[0];
  Adapter *adapter = device->mAdapters[0];
  
  CPPUNIT_ASSERT(adapter->isIgnoringTimestamps());

}

void ConfigTest::testIgnoreTimestampsOverride()
{
  istringstream str("Devices = ../samples/test_config.xml\n"
                    "IgnoreTimestamps = true\n"
                    "Adapters { LinuxCNC { \n"
                    "IgnoreTimestamps = false\n"
                    "} }\n");
  mConfig->loadConfig(str);
  
  Agent *agent = mConfig->getAgent();
  CPPUNIT_ASSERT(agent);
  Device *device = agent->getDevices()[0];
  Adapter *adapter = device->mAdapters[0];
  
  CPPUNIT_ASSERT(!adapter->isIgnoringTimestamps());
  
}

void ConfigTest::testSpecifyMTCNamespace()
{
  istringstream streams("StreamsNamespaces {\n"
                        "m {\n"
                        "Location = /schemas/MTConnectStreams_1.2.xsd\n"
                        "Path = ./MTConnectStreams_1.2.xsd\n"
                        "}\n"
                        "}\n");
  
  mConfig->loadConfig(streams);  
  string path = XmlPrinter::getStreamsUrn("m");
  CPPUNIT_ASSERT_EQUAL((string) "", path);
  string location = XmlPrinter::getStreamsLocation("m");
  CPPUNIT_ASSERT_EQUAL((string) "/schemas/MTConnectStreams_1.2.xsd", location);
  
  XmlPrinter::clearStreamsNamespaces();
}

void ConfigTest::testSetSchemaVersion()
{
  istringstream streams("SchemaVersion = 1.4\n");
  
  mConfig->loadConfig(streams);  
  string version = XmlPrinter::getSchemaVersion();
  CPPUNIT_ASSERT_EQUAL((string) "1.4", version);
  
  XmlPrinter::setSchemaVersion("1.3");
}

void ConfigTest::testSchemaDirectory()
{
  istringstream schemas("SchemaVersion = 1.3\n"
                        "Files {\n"
                        "schemas {\n"
                        "Location = /schemas\n"
                        "Path = ../schemas\n"
                        "}\n"
                        "}\n");
  
  mConfig->loadConfig(schemas);
  string path = XmlPrinter::getStreamsUrn("m");
  CPPUNIT_ASSERT_EQUAL((string) "urn:mtconnect.org:MTConnectStreams:1.3", path);
  string location = XmlPrinter::getStreamsLocation("m");
  CPPUNIT_ASSERT_EQUAL((string) "/schemas/MTConnectStreams_1.2.xsd", location);

  path = XmlPrinter::getDevicesUrn("m");
  CPPUNIT_ASSERT_EQUAL((string) "urn:mtconnect.org:MTConnectDevices:1.3", path);
  location = XmlPrinter::getDevicesLocation("m");
  CPPUNIT_ASSERT_EQUAL((string) "/schemas/MTConnectDevices_1.2.xsd", location);

  path = XmlPrinter::getAssetsUrn("m");
  CPPUNIT_ASSERT_EQUAL((string) "urn:mtconnect.org:MTConnectAssets:1.3", path);
  location = XmlPrinter::getAssetsLocation("m");
  CPPUNIT_ASSERT_EQUAL((string) "/schemas/MTConnectAssets_1.2.xsd", location);

  path = XmlPrinter::getErrorUrn("m");
  CPPUNIT_ASSERT_EQUAL((string) "urn:mtconnect.org:MTConnectError:1.3", path);
  location = XmlPrinter::getErrorLocation("m");
  CPPUNIT_ASSERT_EQUAL((string) "/schemas/MTConnectError_1.2.xsd", location);
  
  XmlPrinter::clearDevicesNamespaces();
  XmlPrinter::clearErrorNamespaces();
  XmlPrinter::clearStreamsNamespaces();
  XmlPrinter::clearAssetsNamespaces();
}

void ConfigTest::testLogFileRollover()
{
  istringstream logger("logger_config {"
                        "logging_level = ALL\n"
                        "max_size = 150\n"
                        "max_index = 5\n"
                        "output = file agent.log"
                        "}\n");
  char buffer[64];
  ::remove("agent.log");
  for (int i = 1; i <= 10; i++) {
    sprintf(buffer, "agent.log.%d", i);
    ::remove(buffer);
  }
  
  mConfig->loadConfig(logger);
  
  CPPUNIT_ASSERT(file_exists("agent.log"));
  CPPUNIT_ASSERT(file_exists("agent.log.1"));
  CPPUNIT_ASSERT(!file_exists("agent.log.2"));
  CPPUNIT_ASSERT(!file_exists("agent.log.3"));
  CPPUNIT_ASSERT(!file_exists("agent.log.4"));
  CPPUNIT_ASSERT(!file_exists("agent.log.5"));

  sLogger << LERROR << "12345678901234567890";
  CPPUNIT_ASSERT(file_exists("agent.log.2"));
  CPPUNIT_ASSERT(!file_exists("agent.log.3"));
  
  sLogger << LERROR << "12345678901234567890";
  CPPUNIT_ASSERT(file_exists("agent.log.2"));
  CPPUNIT_ASSERT(!file_exists("agent.log.3"));

  sLogger << LERROR << "12345678901234567890";
  CPPUNIT_ASSERT(file_exists("agent.log.3"));
  CPPUNIT_ASSERT(!file_exists("agent.log.4"));

  sLogger << LERROR << "12345678901234567890";
  sLogger << LERROR << "12345678901234567890";
  CPPUNIT_ASSERT(file_exists("agent.log.4"));
  CPPUNIT_ASSERT(!file_exists("agent.log.5"));

  sLogger << LERROR << "12345678901234567890";
  sLogger << LERROR << "12345678901234567890";
  CPPUNIT_ASSERT(file_exists("agent.log.5"));
  CPPUNIT_ASSERT(!file_exists("agent.log.6"));
}

void ConfigTest::testMaxSize()
{
  istringstream logger("logger_config {"
                        "max_size = 150\n"
                        "}\n");
  mConfig->loadConfig(logger);
  
  RollingFileLogger *fl = mConfig->getLogger();
  CPPUNIT_ASSERT_EQUAL(150, fl->getMaxSize());


  istringstream logger2("logger_config {"
                       "max_size = 15K\n"
                       "}\n");
  mConfig->loadConfig(logger2);
  
  fl = mConfig->getLogger();
  CPPUNIT_ASSERT_EQUAL(15 * 1024, fl->getMaxSize());


  istringstream logger3("logger_config {"
                        "max_size = 15M\n"
                        "}\n");
  mConfig->loadConfig(logger3);
  
  fl = mConfig->getLogger();
  CPPUNIT_ASSERT_EQUAL(15 * 1024 * 1024, fl->getMaxSize());


  istringstream logger4("logger_config {"
                        "max_size = 15G\n"
                        "}\n");
  mConfig->loadConfig(logger4);
  
  fl = mConfig->getLogger();
  CPPUNIT_ASSERT_EQUAL(15 * 1024 * 1024 * 1024, fl->getMaxSize());

}