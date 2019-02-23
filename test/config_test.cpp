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

namespace date {};
using namespace date;

#include <sstream>
#include <chrono>
#include <date/date.h>

#include "config_test.hpp"
#include "agent.hpp"
#include "adapter.hpp"
#include "rolling_file_logger.hpp"
#include <dlib/dir_nav.h>

static dlib::logger g_logger("config_test");

using namespace std;
using namespace std::chrono;

namespace mtconnect {
  namespace test {
    // Registers the fixture into the 'registry'
    CPPUNIT_TEST_SUITE_REGISTRATION(ConfigTest);
    

    void ConfigTest::setUp()
    {
      m_config = make_unique<AgentConfiguration>();
    }
    
    
    void ConfigTest::tearDown()
    {
      m_config.reset();
    }
    
    
    void ConfigTest::testBlankConfig()
    {
      istringstream str("");
      m_config->loadConfig(str);
      
      const auto agent = m_config->getAgent();
      CPPUNIT_ASSERT(agent);
      CPPUNIT_ASSERT_EQUAL((size_t)1, agent->getDevices().size());
    }
    
    
    void ConfigTest::testBufferSize()
    {
      istringstream str("BufferSize = 4\n");
      m_config->loadConfig(str);
      
      const auto agent = m_config->getAgent();
      CPPUNIT_ASSERT(agent);
      CPPUNIT_ASSERT_EQUAL(16, (int) agent->getBufferSize());
    }
    
    
    void ConfigTest::testDevice()
    {
      istringstream str("Devices = ../samples/test_config.xml\n");
      m_config->loadConfig(str);
      
      const auto agent = m_config->getAgent();
      CPPUNIT_ASSERT(agent);
      const auto device = agent->getDevices()[0];
      const auto adapter = device->m_adapters[0];
      
      CPPUNIT_ASSERT_EQUAL((string) "LinuxCNC", device->getName());
      CPPUNIT_ASSERT(!adapter->isDupChecking());
      CPPUNIT_ASSERT(!adapter->isAutoAvailable());
      CPPUNIT_ASSERT(!adapter->isIgnoringTimestamps());
      CPPUNIT_ASSERT(device->m_preserveUuid);
    }
    
    
    void ConfigTest::testAdapter()
    {
      istringstream str(
                        "Devices = ../samples/test_config.xml\n"
                        "Adapters { LinuxCNC { \n"
                        "Port = 23\n"
                        "Host = 10.211.55.1\n"
                        "FilterDuplicates = true\n"
                        "AutoAvailable = true\n"
                        "IgnoreTimestamps = true\n"
                        "PreserveUUID = true\n"
                        "LegacyTimeout = 2000\n"
                        "} }\n");
      m_config->loadConfig(str);
      
      const auto agent = m_config->getAgent();
      CPPUNIT_ASSERT(agent);
      const auto device = agent->getDevices()[0];
      const auto adapter = device->m_adapters[0];
      
      CPPUNIT_ASSERT_EQUAL(23, (int) adapter->getPort());
      CPPUNIT_ASSERT_EQUAL((string) "10.211.55.1", adapter->getServer());
      CPPUNIT_ASSERT(adapter->isDupChecking());
      CPPUNIT_ASSERT(adapter->isAutoAvailable());
      CPPUNIT_ASSERT(adapter->isIgnoringTimestamps());
      CPPUNIT_ASSERT_EQUAL(2000s, adapter->getLegacyTimeout());
      CPPUNIT_ASSERT(device->m_preserveUuid);
    }
    
    
    void ConfigTest::testDefaultPreserveUUID()
    {
      istringstream str(
                        "Devices = ../samples/test_config.xml\n"
                        "PreserveUUID = true\n");
      m_config->loadConfig(str);
      
      const auto agent = m_config->getAgent();
      CPPUNIT_ASSERT(agent);
      const auto device = agent->getDevices()[0];
      
      CPPUNIT_ASSERT(device->m_preserveUuid);
    }
    
    
    void ConfigTest::testDefaultPreserveOverride()
    {
      istringstream str(
                        "Devices = ../samples/test_config.xml\n"
                        "PreserveUUID = true\n"
                        "Adapters { LinuxCNC { \n"
                        "PreserveUUID = false\n"
                        "} }\n");
      m_config->loadConfig(str);
      
      const auto agent = m_config->getAgent();
      CPPUNIT_ASSERT(agent);
      const auto device = agent->getDevices()[0];
      
      CPPUNIT_ASSERT(!device->m_preserveUuid);
    }
    
    
    void ConfigTest::testDisablePut()
    {
      istringstream str(
                        "Devices = ../samples/test_config.xml\n"
                        "AllowPut = true\n");
      m_config->loadConfig(str);
      
      const auto agent = m_config->getAgent();
      CPPUNIT_ASSERT(agent);
      
      CPPUNIT_ASSERT(agent->isPutEnabled());
    }
    
    
    void ConfigTest::testLimitPut()
    {
      istringstream str(
                        "Devices = ../samples/test_config.xml\n"
                        "AllowPutFrom = localhost\n");
      m_config->loadConfig(str);
      
      const auto agent = m_config->getAgent();
      CPPUNIT_ASSERT(agent);
      CPPUNIT_ASSERT(agent->isPutEnabled());
      CPPUNIT_ASSERT(agent->isPutAllowedFrom((string) "127.0.0.1"));
    }
    
    
    void ConfigTest::testLimitPutFromHosts()
    {
      istringstream str(
                        "Devices = ../samples/test_config.xml\n"
                        "AllowPutFrom = localhost, 192.168.0.1\n");
      m_config->loadConfig(str);
      
      const auto agent = m_config->getAgent();
      CPPUNIT_ASSERT(agent);
      CPPUNIT_ASSERT(agent->isPutEnabled());
      CPPUNIT_ASSERT(agent->isPutAllowedFrom((string) "127.0.0.1"));
      CPPUNIT_ASSERT(agent->isPutAllowedFrom((string) "192.168.0.1"));
    }
    
    
    void ConfigTest::testNamespaces()
    {
      istringstream streams(
                            "StreamsNamespaces {\n"
                            "x {\n"
                            "Urn = urn:example.com:ExampleStreams:1.2\n"
                            "Location = /schemas/ExampleStreams_1.2.xsd\n"
                            "Path = ./ExampleStreams_1.2.xsd\n"
                            "}\n"
                            "}\n");
      
      m_config->loadConfig(streams);
      auto path = XmlPrinter::getStreamsUrn("x");
      CPPUNIT_ASSERT_EQUAL((string) "urn:example.com:ExampleStreams:1.2", path);
      
      istringstream devices(
                            "DevicesNamespaces {\n"
                            "y {\n"
                            "Urn = urn:example.com:ExampleDevices:1.2\n"
                            "Location = /schemas/ExampleDevices_1.2.xsd\n"
                            "Path = ./ExampleDevices_1.2.xsd\n"
                            "}\n"
                            "}\n");
      
      m_config->loadConfig(devices);
      path = XmlPrinter::getDevicesUrn("y");
      CPPUNIT_ASSERT_EQUAL((string) "urn:example.com:ExampleDevices:1.2", path);
      
      
      istringstream assets(
                           "AssetsNamespaces {\n"
                           "z {\n"
                           "Urn = urn:example.com:ExampleAssets:1.2\n"
                           "Location = /schemas/ExampleAssets_1.2.xsd\n"
                           "Path = ./ExampleAssets_1.2.xsd\n"
                           "}\n"
                           "}\n");
      
      m_config->loadConfig(assets);
      path = XmlPrinter::getAssetsUrn("z");
      CPPUNIT_ASSERT_EQUAL((string) "urn:example.com:ExampleAssets:1.2", path);
      
      istringstream errors(
                           "ErrorNamespaces {\n"
                           "a {\n"
                           "Urn = urn:example.com:ExampleErrors:1.2\n"
                           "Location = /schemas/ExampleErrors_1.2.xsd\n"
                           "Path = ./ExampleErrorss_1.2.xsd\n"
                           "}\n"
                           "}\n");
      
      m_config->loadConfig(errors);
      path = XmlPrinter::getErrorUrn("a");
      CPPUNIT_ASSERT_EQUAL((string) "urn:example.com:ExampleErrors:1.2", path);
      
      XmlPrinter::clearDevicesNamespaces();
      XmlPrinter::clearErrorNamespaces();
      XmlPrinter::clearStreamsNamespaces();
      XmlPrinter::clearAssetsNamespaces();
    }
    
    
    void ConfigTest::testLegacyTimeout()
    {
      istringstream str(
                        "Devices = ../samples/test_config.xml\n"
                        "LegacyTimeout = 2000\n");
      m_config->loadConfig(str);
      
      const auto agent = m_config->getAgent();
      CPPUNIT_ASSERT(agent);
      const auto device = agent->getDevices()[0];
      const auto adapter = device->m_adapters[0];
      
      CPPUNIT_ASSERT_EQUAL(2000s, adapter->getLegacyTimeout());
    }
    
    
    void ConfigTest::testIgnoreTimestamps()
    {
      istringstream str(
                        "Devices = ../samples/test_config.xml\n"
                        "IgnoreTimestamps = true\n");
      m_config->loadConfig(str);
      
      const auto agent = m_config->getAgent();
      CPPUNIT_ASSERT(agent);
      const auto device = agent->getDevices()[0];
      const auto adapter = device->m_adapters[0];
      
      CPPUNIT_ASSERT(adapter->isIgnoringTimestamps());
      
    }
    
    
    void ConfigTest::testIgnoreTimestampsOverride()
    {
      istringstream str(
                        "Devices = ../samples/test_config.xml\n"
                        "IgnoreTimestamps = true\n"
                        "Adapters { LinuxCNC { \n"
                        "IgnoreTimestamps = false\n"
                        "} }\n");
      m_config->loadConfig(str);
      
      const auto agent = m_config->getAgent();
      CPPUNIT_ASSERT(agent);
      const auto device = agent->getDevices()[0];
      const auto adapter = device->m_adapters[0];
      
      CPPUNIT_ASSERT(!adapter->isIgnoringTimestamps());
      
    }
    
    
    void ConfigTest::testSpecifyMTCNamespace()
    {
      istringstream streams(
                            "StreamsNamespaces {\n"
                            "m {\n"
                            "Location = /schemas/MTConnectStreams_1.2.xsd\n"
                            "Path = ./MTConnectStreams_1.2.xsd\n"
                            "}\n"
                            "}\n");
      
      m_config->loadConfig(streams);
      auto path = XmlPrinter::getStreamsUrn("m");
      CPPUNIT_ASSERT_EQUAL((string) "", path);
      auto location = XmlPrinter::getStreamsLocation("m");
      CPPUNIT_ASSERT_EQUAL((string) "/schemas/MTConnectStreams_1.2.xsd", location);
      
      XmlPrinter::clearStreamsNamespaces();
    }
    
    
    void ConfigTest::testSetSchemaVersion()
    {
      istringstream streams("SchemaVersion = 1.4\n");
      
      m_config->loadConfig(streams);
      auto version = XmlPrinter::getSchemaVersion();
      CPPUNIT_ASSERT_EQUAL((string) "1.4", version);
      
      XmlPrinter::setSchemaVersion("1.3");
    }
    
    
    void ConfigTest::testSchemaDirectory()
    {
      istringstream schemas(
                            "SchemaVersion = 1.3\n"
                            "Files {\n"
                            "schemas {\n"
                            "Location = /schemas\n"
                            "Path = ../schemas\n"
                            "}\n"
                            "}\n");
      
      m_config->loadConfig(schemas);
      auto path = XmlPrinter::getStreamsUrn("m");
      CPPUNIT_ASSERT_EQUAL((string) "urn:mtconnect.org:MTConnectStreams:1.3", path);
      auto location = XmlPrinter::getStreamsLocation("m");
      CPPUNIT_ASSERT_EQUAL((string) "/schemas/MTConnectStreams_1.3.xsd", location);
      
      path = XmlPrinter::getDevicesUrn("m");
      CPPUNIT_ASSERT_EQUAL((string) "urn:mtconnect.org:MTConnectDevices:1.3", path);
      location = XmlPrinter::getDevicesLocation("m");
      CPPUNIT_ASSERT_EQUAL((string) "/schemas/MTConnectDevices_1.3.xsd", location);
      
      path = XmlPrinter::getAssetsUrn("m");
      CPPUNIT_ASSERT_EQUAL((string) "urn:mtconnect.org:MTConnectAssets:1.3", path);
      location = XmlPrinter::getAssetsLocation("m");
      CPPUNIT_ASSERT_EQUAL((string) "/schemas/MTConnectAssets_1.3.xsd", location);
      
      path = XmlPrinter::getErrorUrn("m");
      CPPUNIT_ASSERT_EQUAL((string) "urn:mtconnect.org:MTConnectError:1.3", path);
      location = XmlPrinter::getErrorLocation("m");
      CPPUNIT_ASSERT_EQUAL((string) "/schemas/MTConnectError_1.3.xsd", location);
      
      XmlPrinter::clearDevicesNamespaces();
      XmlPrinter::clearErrorNamespaces();
      XmlPrinter::clearStreamsNamespaces();
      XmlPrinter::clearAssetsNamespaces();
    }
    
    
    void ConfigTest::testLogFileRollover()
    {
      // This test/rollover is fragile on Windows due to file caching.
      // Whilst the agent uses standard C runtime file functions
      // this can not easily be worked around. Better to disable the test.
#ifndef _WINDOWS
      istringstream logger(
                           "logger_config {"
                           "logging_level = ERROR\n"
                           "max_size = 150\n"
                           "max_index = 5\n"
                           "output = file agent.log"
                           "}\n");
      char buffer[64] = {0};
      ::remove("agent.log");
      
      for (int i = 1; i <= 10; i++)
      {
        sprintf(buffer, "agent.log.%d", i);
        ::remove(buffer);
      }
      
      m_config->loadConfig(logger);
      
      CPPUNIT_ASSERT(file_exists("agent.log"));
      CPPUNIT_ASSERT(!file_exists("agent.log.1"));
      CPPUNIT_ASSERT(!file_exists("agent.log.2"));
      CPPUNIT_ASSERT(!file_exists("agent.log.3"));
      CPPUNIT_ASSERT(!file_exists("agent.log.4"));
      CPPUNIT_ASSERT(!file_exists("agent.log.5"));
      
      g_logger << LERROR << "12345678901234567890";
      g_logger << LERROR << "12345678901234567890";
      g_logger << LERROR << "12345678901234567890";
      CPPUNIT_ASSERT(file_exists("agent.log.1"));
      CPPUNIT_ASSERT(!file_exists("agent.log.2"));
      
      g_logger << LERROR << "12345678901234567890";
      g_logger << LERROR << "12345678901234567890";
      CPPUNIT_ASSERT(file_exists("agent.log.2"));
      CPPUNIT_ASSERT(!file_exists("agent.log.3"));
      
      g_logger << LERROR << "12345678901234567890";
      g_logger << LERROR << "12345678901234567890";
      CPPUNIT_ASSERT(file_exists("agent.log.3"));
      CPPUNIT_ASSERT(!file_exists("agent.log.4"));
      
      g_logger << LERROR << "12345678901234567890";
      g_logger << LERROR << "12345678901234567890";
      CPPUNIT_ASSERT(file_exists("agent.log.4"));
      CPPUNIT_ASSERT(!file_exists("agent.log.5"));
      
      g_logger << LERROR << "12345678901234567890";
      g_logger << LERROR << "12345678901234567890";
      CPPUNIT_ASSERT(file_exists("agent.log.5"));
      CPPUNIT_ASSERT(!file_exists("agent.log.6"));
      
      g_logger << LERROR << "12345678901234567890";
      g_logger << LERROR << "12345678901234567890";
      CPPUNIT_ASSERT(file_exists("agent.log.5"));
      CPPUNIT_ASSERT(!file_exists("agent.log.6"));
#endif
    }
    
    
    void ConfigTest::testMaxSize()
    {
      istringstream logger(
                           "logger_config {"
                           "max_size = 150\n"
                           "}\n");
      m_config->loadConfig(logger);
      auto fl = m_config->getLogger();
      CPPUNIT_ASSERT_EQUAL(150ull, fl->getMaxSize());
      m_config.reset();
      
      m_config = make_unique<AgentConfiguration>();
      istringstream logger2(
                            "logger_config {"
                            "max_size = 15K\n"
                            "}\n");
      m_config->loadConfig(logger2);
      
      fl = m_config->getLogger();
      CPPUNIT_ASSERT_EQUAL(15ull * 1024ull, fl->getMaxSize());
      m_config.reset();
      
      m_config = make_unique<AgentConfiguration>();
      istringstream logger3(
                            "logger_config {"
                            "max_size = 15M\n"
                            "}\n");
      m_config->loadConfig(logger3);
      
      fl = m_config->getLogger();
      CPPUNIT_ASSERT_EQUAL(15ull * 1024ull * 1024ull, fl->getMaxSize());
      m_config.reset();
      
      m_config = make_unique<AgentConfiguration>();
      istringstream logger4(
                            "logger_config {"
                            "max_size = 15G\n"
                            "}\n");
      m_config->loadConfig(logger4);
      
      fl = m_config->getLogger();
      CPPUNIT_ASSERT_EQUAL(15ull * 1024ull * 1024ull * 1024ull, fl->getMaxSize());
      
    }
  }
}
