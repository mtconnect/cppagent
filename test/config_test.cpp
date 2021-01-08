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

// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "agent.hpp"
#include "config.hpp"
#include "rolling_file_logger.hpp"
#include "xml_printer.hpp"

#include <chrono>
#include <string>

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#define chdir _chdir
#endif

static dlib::logger g_logger("config_test");

namespace
{
  class ConfigTest : public testing::Test
  {
   protected:
    void SetUp() override
    {
      m_config = std::make_unique<mtconnect::AgentConfiguration>();
      char buf[1024 * 4];
      getcwd(buf, sizeof(buf));
      m_cwd = buf;
    }

    void TearDown() override
    {
      m_config.reset();
      if (!m_cwd.empty())
        chdir(m_cwd.c_str());
    }

    std::unique_ptr<mtconnect::AgentConfiguration> m_config;
    std::string m_cwd;
  };

  TEST_F(ConfigTest, BlankConfig)
  {
    chdir(TEST_BIN_ROOT_DIR);
    std::istringstream str("");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    ASSERT_EQ(size_t(1), agent->getDevices().size());
  }

  TEST_F(ConfigTest, BufferSize)
  {
    chdir(TEST_BIN_ROOT_DIR);
    std::istringstream str("BufferSize = 4\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    ASSERT_EQ(16U, agent->getBufferSize());
  }

  TEST_F(ConfigTest, Device)
  {
    std::istringstream str("Devices = " PROJECT_ROOT_DIR "/samples/test_config.xml\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    const auto device = agent->getDevices().front();
    const auto adapter = device->m_adapters.front();

    ASSERT_EQ(std::string("LinuxCNC"), device->getName());
    ASSERT_FALSE(adapter->isDupChecking());
    ASSERT_FALSE(adapter->isAutoAvailable());
    ASSERT_FALSE(adapter->isIgnoringTimestamps());
    ASSERT_TRUE(device->m_preserveUuid);
  }

  TEST_F(ConfigTest, Adapter)
  {
    using namespace std::chrono_literals;

    std::istringstream str("Devices = " PROJECT_ROOT_DIR
                           "/samples/test_config.xml\n"
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
    ASSERT_TRUE(agent);
    const auto device = agent->getDevices().front();
    const auto adapter = device->m_adapters.front();

    ASSERT_EQ(23, (int)adapter->getPort());
    ASSERT_EQ(std::string("10.211.55.1"), adapter->getServer());
    ASSERT_TRUE(adapter->isDupChecking());
    ASSERT_TRUE(adapter->isAutoAvailable());
    ASSERT_TRUE(adapter->isIgnoringTimestamps());
    ASSERT_EQ(2000s, adapter->getLegacyTimeout());
    ASSERT_TRUE(device->m_preserveUuid);
  }

  TEST_F(ConfigTest, DefaultPreserveUUID)
  {
    std::istringstream str("Devices = " PROJECT_ROOT_DIR
                           "/samples/test_config.xml\n"
                           "PreserveUUID = true\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    const auto device = agent->getDevices().front();

    ASSERT_TRUE(device->m_preserveUuid);
  }

  TEST_F(ConfigTest, DefaultPreserveOverride)
  {
    std::istringstream str("Devices = " PROJECT_ROOT_DIR
                           "/samples/test_config.xml\n"
                           "PreserveUUID = true\n"
                           "Adapters { LinuxCNC { \n"
                           "PreserveUUID = false\n"
                           "} }\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    const auto device = agent->getDevices().front();

    ASSERT_FALSE(device->m_preserveUuid);
  }

  TEST_F(ConfigTest, DisablePut)
  {
    std::istringstream str("Devices = " PROJECT_ROOT_DIR
                           "/samples/test_config.xml\n"
                           "AllowPut = true\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);

    ASSERT_TRUE(agent->getServer()->isPutEnabled());
  }

  TEST_F(ConfigTest, LimitPut)
  {
    std::istringstream str("Devices = " PROJECT_ROOT_DIR
                           "/samples/test_config.xml\n"
                           "AllowPutFrom = localhost\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    ASSERT_TRUE(agent->getServer()->isPutEnabled());
    ASSERT_TRUE(agent->getServer()->isPutAllowedFrom(std::string("127.0.0.1")));
  }

  TEST_F(ConfigTest, LimitPutFromHosts)
  {
    std::istringstream str("Devices = " PROJECT_ROOT_DIR
                           "/samples/test_config.xml\n"
                           "AllowPutFrom = localhost, 192.168.0.1\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    ASSERT_TRUE(agent->getServer()->isPutEnabled());
    ASSERT_TRUE(agent->getServer()->isPutAllowedFrom(std::string("127.0.0.1")));
    ASSERT_TRUE(agent->getServer()->isPutAllowedFrom(std::string("192.168.0.1")));
  }

  TEST_F(ConfigTest, Namespaces)
  {
    chdir(TEST_BIN_ROOT_DIR);
    std::istringstream streams(
        "StreamsNamespaces {\n"
        "x {\n"
        "Urn = urn:example.com:ExampleStreams:1.2\n"
        "Location = /schemas/ExampleStreams_1.2.xsd\n"
        "Path = ./ExampleStreams_1.2.xsd\n"
        "}\n"
        "}\n");

    m_config->loadConfig(streams);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());
    ASSERT_TRUE(agent);
    auto printer = dynamic_cast<mtconnect::XmlPrinter *>(agent->getPrinter("xml"));
    ASSERT_TRUE(printer);

    auto path = printer->getStreamsUrn("x");
    ASSERT_EQ(std::string("urn:example.com:ExampleStreams:1.2"), path);

    std::istringstream devices(
        "DevicesNamespaces {\n"
        "y {\n"
        "Urn = urn:example.com:ExampleDevices:1.2\n"
        "Location = /schemas/ExampleDevices_1.2.xsd\n"
        "Path = ./ExampleDevices_1.2.xsd\n"
        "}\n"
        "}\n");

    m_config->loadConfig(devices);
    path = printer->getDevicesUrn("y");
    ASSERT_EQ(std::string("urn:example.com:ExampleDevices:1.2"), path);

    std::istringstream assets(
        "AssetsNamespaces {\n"
        "z {\n"
        "Urn = urn:example.com:ExampleAssets:1.2\n"
        "Location = /schemas/ExampleAssets_1.2.xsd\n"
        "Path = ./ExampleAssets_1.2.xsd\n"
        "}\n"
        "}\n");

    m_config->loadConfig(assets);
    path = printer->getAssetsUrn("z");
    ASSERT_EQ(std::string("urn:example.com:ExampleAssets:1.2"), path);

    std::istringstream errors(
        "ErrorNamespaces {\n"
        "a {\n"
        "Urn = urn:example.com:ExampleErrors:1.2\n"
        "Location = /schemas/ExampleErrors_1.2.xsd\n"
        "Path = ./ExampleErrorss_1.2.xsd\n"
        "}\n"
        "}\n");

    m_config->loadConfig(errors);
    path = printer->getErrorUrn("a");
    ASSERT_EQ(std::string("urn:example.com:ExampleErrors:1.2"), path);

    printer->clearDevicesNamespaces();
    printer->clearErrorNamespaces();
    printer->clearStreamsNamespaces();
    printer->clearAssetsNamespaces();
  }

  TEST_F(ConfigTest, LegacyTimeout)
  {
    using namespace std::chrono_literals;

    std::istringstream str("Devices = " PROJECT_ROOT_DIR
                           "/samples/test_config.xml\n"
                           "LegacyTimeout = 2000\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    const auto device = agent->getDevices().front();
    const auto adapter = device->m_adapters.front();

    ASSERT_EQ(2000s, adapter->getLegacyTimeout());
  }

  TEST_F(ConfigTest, IgnoreTimestamps)
  {
    std::istringstream str("Devices = " PROJECT_ROOT_DIR
                           "/samples/test_config.xml\n"
                           "IgnoreTimestamps = true\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    const auto device = agent->getDevices().front();
    const auto adapter = device->m_adapters.front();

    ASSERT_TRUE(adapter->isIgnoringTimestamps());
  }

  TEST_F(ConfigTest, IgnoreTimestampsOverride)
  {
    std::istringstream str("Devices = " PROJECT_ROOT_DIR
                           "/samples/test_config.xml\n"
                           "IgnoreTimestamps = true\n"
                           "Adapters { LinuxCNC { \n"
                           "IgnoreTimestamps = false\n"
                           "} }\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    const auto device = agent->getDevices().front();
    const auto adapter = device->m_adapters.front();

    ASSERT_FALSE(adapter->isIgnoringTimestamps());
  }

  TEST_F(ConfigTest, SpecifyMTCNamespace)
  {
    chdir(TEST_BIN_ROOT_DIR);
    std::istringstream streams(
        "StreamsNamespaces {\n"
        "m {\n"
        "Location = /schemas/MTConnectStreams_1.2.xsd\n"
        "Path = ./MTConnectStreams_1.2.xsd\n"
        "}\n"
        "}\n");

    m_config->loadConfig(streams);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());
    ASSERT_TRUE(agent);
    auto printer = dynamic_cast<mtconnect::XmlPrinter *>(agent->getPrinter("xml"));
    ASSERT_TRUE(printer);

    auto path = printer->getStreamsUrn("m");
    ASSERT_EQ(std::string(""), path);
    auto location = printer->getStreamsLocation("m");
    ASSERT_EQ(std::string("/schemas/MTConnectStreams_1.2.xsd"), location);

    printer->clearStreamsNamespaces();
  }

  TEST_F(ConfigTest, SetSchemaVersion)
  {
    chdir(TEST_BIN_ROOT_DIR);
    std::istringstream streams("SchemaVersion = 1.4\n");

    m_config->loadConfig(streams);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());
    ASSERT_TRUE(agent);
    auto printer = dynamic_cast<mtconnect::XmlPrinter *>(agent->getPrinter("xml"));
    ASSERT_TRUE(printer);

    auto version = printer->getSchemaVersion();
    ASSERT_EQ(std::string("1.4"), version);

    printer->setSchemaVersion("1.3");
  }

  TEST_F(ConfigTest, SchemaDirectory)
  {
    chdir(PROJECT_ROOT_DIR "/test");
    std::istringstream schemas(
        "SchemaVersion = 1.3\n"
        "Files {\n"
        "schemas {\n"
        "Location = /schemas\n"
        "Path = ../schemas\n"
        "}\n"
        "}\n"
        "logger_config {\n"
        "output = cout\n"
        "}\n");

    m_config->setDebug(true);
    m_config->loadConfig(schemas);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());
    ASSERT_TRUE(agent);
    auto printer = dynamic_cast<mtconnect::XmlPrinter *>(agent->getPrinter("xml"));
    ASSERT_TRUE(printer);

    auto path = printer->getStreamsUrn("m");
    ASSERT_EQ(std::string("urn:mtconnect.org:MTConnectStreams:1.3"), path);
    auto location = printer->getStreamsLocation("m");
    ASSERT_EQ(std::string("/schemas/MTConnectStreams_1.3.xsd"), location);

    path = printer->getDevicesUrn("m");
    ASSERT_EQ(std::string("urn:mtconnect.org:MTConnectDevices:1.3"), path);
    location = printer->getDevicesLocation("m");
    ASSERT_EQ(std::string("/schemas/MTConnectDevices_1.3.xsd"), location);

    path = printer->getAssetsUrn("m");
    ASSERT_EQ(std::string("urn:mtconnect.org:MTConnectAssets:1.3"), path);
    location = printer->getAssetsLocation("m");
    ASSERT_EQ(std::string("/schemas/MTConnectAssets_1.3.xsd"), location);

    path = printer->getErrorUrn("m");
    ASSERT_EQ(std::string("urn:mtconnect.org:MTConnectError:1.3"), path);
    location = printer->getErrorLocation("m");
    ASSERT_EQ(std::string("/schemas/MTConnectError_1.3.xsd"), location);

    printer->clearDevicesNamespaces();
    printer->clearErrorNamespaces();
    printer->clearStreamsNamespaces();
    printer->clearAssetsNamespaces();
  }

  TEST_F(ConfigTest, LogFileRollover)
  {
    chdir(TEST_BIN_ROOT_DIR);
// This test/rollover is fragile on Windows due to file caching.
// Whilst the agent uses standard C runtime file functions
// this can not easily be worked around. Better to disable the test.
#ifndef _WINDOWS
    std::istringstream logger(
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

    ASSERT_TRUE(file_exists("agent.log"));
    ASSERT_FALSE(file_exists("agent.log.1"));
    ASSERT_FALSE(file_exists("agent.log.2"));
    ASSERT_FALSE(file_exists("agent.log.3"));
    ASSERT_FALSE(file_exists("agent.log.4"));
    ASSERT_FALSE(file_exists("agent.log.5"));

    g_logger << LERROR << "12345678901234567890";
    g_logger << LERROR << "12345678901234567890";
    g_logger << LERROR << "12345678901234567890";
    ASSERT_TRUE(file_exists("agent.log.1"));
    ASSERT_FALSE(file_exists("agent.log.2"));

    g_logger << LERROR << "12345678901234567890";
    g_logger << LERROR << "12345678901234567890";
    ASSERT_TRUE(file_exists("agent.log.2"));
    ASSERT_FALSE(file_exists("agent.log.3"));

    g_logger << LERROR << "12345678901234567890";
    g_logger << LERROR << "12345678901234567890";
    ASSERT_TRUE(file_exists("agent.log.3"));
    ASSERT_FALSE(file_exists("agent.log.4"));

    g_logger << LERROR << "12345678901234567890";
    g_logger << LERROR << "12345678901234567890";
    ASSERT_TRUE(file_exists("agent.log.4"));
    ASSERT_FALSE(file_exists("agent.log.5"));

    g_logger << LERROR << "12345678901234567890";
    g_logger << LERROR << "12345678901234567890";
    ASSERT_TRUE(file_exists("agent.log.5"));
    ASSERT_FALSE(file_exists("agent.log.6"));

    g_logger << LERROR << "12345678901234567890";
    g_logger << LERROR << "12345678901234567890";
    ASSERT_TRUE(file_exists("agent.log.5"));
    ASSERT_FALSE(file_exists("agent.log.6"));
#endif
  }

  TEST_F(ConfigTest, MaxSize)
  {
    chdir(TEST_BIN_ROOT_DIR);
    std::istringstream logger(
        "logger_config {"
        "max_size = 150\n"
        "}\n");
    m_config->loadConfig(logger);
    auto fl = m_config->getLogger();
    ASSERT_EQ(150ULL, fl->getMaxSize());
    m_config.reset();

    m_config = std::make_unique<mtconnect::AgentConfiguration>();
    std::istringstream logger2(
        "logger_config {"
        "max_size = 15K\n"
        "}\n");
    m_config->loadConfig(logger2);

    fl = m_config->getLogger();
    ASSERT_EQ(15ULL * 1024ULL, fl->getMaxSize());
    m_config.reset();

    m_config = std::make_unique<mtconnect::AgentConfiguration>();
    std::istringstream logger3(
        "logger_config {"
        "max_size = 15M\n"
        "}\n");
    m_config->loadConfig(logger3);

    fl = m_config->getLogger();
    ASSERT_EQ(15ULL * 1024ULL * 1024ULL, fl->getMaxSize());
    m_config.reset();

    m_config = std::make_unique<mtconnect::AgentConfiguration>();
    std::istringstream logger4(
        "logger_config {"
        "max_size = 15G\n"
        "}\n");
    m_config->loadConfig(logger4);

    fl = m_config->getLogger();
    ASSERT_EQ(15ULL * 1024ULL * 1024ULL * 1024ULL, fl->getMaxSize());
  }
}  // namespace
