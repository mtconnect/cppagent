//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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
#include "configuration/agent_config.hpp"
#include "configuration/config_options.hpp"
#include "xml_printer.hpp"
#include "rest_sink/rest_service.hpp"
#include "adapter/shdr/shdr_adapter.hpp"

#include <chrono>
#include <string>
#include <filesystem>

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#define chdir _chdir
#endif

using namespace std;
using namespace mtconnect;
using namespace mtconnect::configuration;
namespace fs = std::filesystem;

namespace
{
  class ConfigTest : public testing::Test
  {
   protected:
    void SetUp() override
    {
      m_config = std::make_unique<AgentConfiguration>();
      m_cwd = std::filesystem::current_path();
    }

    void TearDown() override
    {
      m_config.reset();
      chdir(m_cwd.string().c_str());
    }

    std::unique_ptr<AgentConfiguration> m_config;
    std::filesystem::path m_cwd;
  };

  TEST_F(ConfigTest, BlankConfig)
  {
    chdir(TEST_BIN_ROOT_DIR);
    m_config->updateWorkingDirectory();
    m_config->loadConfig("");

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    ASSERT_EQ(size_t(2), agent->getDevices().size());
  }

  TEST_F(ConfigTest, BufferSize)
  {
    chdir(TEST_BIN_ROOT_DIR);
    m_config->updateWorkingDirectory();
    m_config->loadConfig("BufferSize = 4\n");

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    const auto sink = agent->findSink("RestService");
    ASSERT_TRUE(sink);
    const auto rest = dynamic_pointer_cast<rest_sink::RestService>(sink);
    ASSERT_TRUE(rest);
    ASSERT_EQ(16U, rest->getBufferSize());
  }

  TEST_F(ConfigTest, Device)
  {
    string str("Devices = " PROJECT_ROOT_DIR "/samples/test_config.xml\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    const auto source = agent->getSources().front();
    const auto adapter = dynamic_pointer_cast<adapter::Adapter>(source);

    auto deviceName = GetOption<string>(adapter->getOptions(), configuration::Device);
    ASSERT_TRUE(deviceName);
    ASSERT_EQ("LinuxCNC", *deviceName);

    ASSERT_FALSE(IsOptionSet(adapter->getOptions(), configuration::FilterDuplicates));
    ASSERT_FALSE(IsOptionSet(adapter->getOptions(), configuration::AutoAvailable));
    ASSERT_FALSE(IsOptionSet(adapter->getOptions(), configuration::IgnoreTimestamps));
    
    auto device = agent->findDeviceByUUIDorName(*deviceName);
    ASSERT_TRUE(device->preserveUuid());
  }

  TEST_F(ConfigTest, Adapter)
  {
    using namespace std::chrono_literals;

    string str("Devices = " PROJECT_ROOT_DIR
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
    const auto source = agent->getSources().front();
    const auto adapter = dynamic_pointer_cast<adapter::shdr::ShdrAdapter>(source);

    ASSERT_EQ(23, (int)adapter->getPort());
    ASSERT_EQ(std::string("10.211.55.1"), adapter->getServer());
    ASSERT_TRUE(IsOptionSet(adapter->getOptions(), configuration::FilterDuplicates));
    ASSERT_TRUE(IsOptionSet(adapter->getOptions(), configuration::AutoAvailable));
    ASSERT_TRUE(IsOptionSet(adapter->getOptions(), configuration::IgnoreTimestamps));
    
    ASSERT_EQ(2000s, adapter->getLegacyTimeout());
    
    // TODO: Need to link to device to the adapter.
    //ASSERT_TRUE(device->m_preserveUuid);
  }

  TEST_F(ConfigTest, DefaultPreserveUUID)
  {
    string str("Devices = " PROJECT_ROOT_DIR
                           "/samples/test_config.xml\n"
                           "PreserveUUID = true\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    const auto device = agent->getDevices().front();

    ASSERT_TRUE(device->preserveUuid());
  }

  TEST_F(ConfigTest, DefaultPreserveOverride)
  {
    string str("Devices = " PROJECT_ROOT_DIR
                           "/samples/test_config.xml\n"
                           "PreserveUUID = true\n"
                           "Adapters { LinuxCNC { \n"
                           "PreserveUUID = false\n"
                           "} }\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    const auto device = agent->findDeviceByUUIDorName("LinuxCNC");

    ASSERT_FALSE(device->preserveUuid());
  }

  TEST_F(ConfigTest, DisablePut)
  {
    string str("Devices = " PROJECT_ROOT_DIR
                           "/samples/test_config.xml\n"
                           "AllowPut = true\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    const auto sink = agent->findSink("RestService");
    ASSERT_TRUE(sink);
    const auto rest = dynamic_pointer_cast<rest_sink::RestService>(sink);

    ASSERT_TRUE(rest->getServer()->arePutsAllowed());
  }

  TEST_F(ConfigTest, LimitPut)
  {
    string str("Devices = " PROJECT_ROOT_DIR
                           "/samples/test_config.xml\n"
                           "AllowPutFrom = localhost\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    const auto sink = agent->findSink("RestService");
    ASSERT_TRUE(sink);
    const auto rest = dynamic_pointer_cast<rest_sink::RestService>(sink);
    ASSERT_TRUE(rest);

    ASSERT_TRUE(rest->getServer()->arePutsAllowed());
    ASSERT_TRUE(rest->getServer()->allowPutFrom(std::string("127.0.0.1")));
  }

  TEST_F(ConfigTest, LimitPutFromHosts)
  {
    string str("Devices = " PROJECT_ROOT_DIR
                           "/samples/test_config.xml\n"
                           "AllowPutFrom = localhost, 192.168.0.1\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    const auto sink = agent->findSink("RestService");
    ASSERT_TRUE(sink);
    const auto rest = dynamic_pointer_cast<rest_sink::RestService>(sink);
    ASSERT_TRUE(rest);

    ASSERT_TRUE(rest->getServer()->arePutsAllowed());
    ASSERT_TRUE(rest->getServer()->allowPutFrom(std::string("127.0.0.1")));
    ASSERT_TRUE(rest->getServer()->allowPutFrom(std::string("192.168.0.1")));
  }

  TEST_F(ConfigTest, Namespaces)
  {
    chdir(TEST_BIN_ROOT_DIR);
    m_config->updateWorkingDirectory();
    string streams(
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

    string devices(
        "DevicesNamespaces {\n"
        "y {\n"
        "Urn = urn:example.com:ExampleDevices:1.2\n"
        "Location = /schemas/ExampleDevices_1.2.xsd\n"
        "Path = ./ExampleDevices_1.2.xsd\n"
        "}\n"
        "}\n");

    m_config->loadConfig(devices);
    agent = const_cast<mtconnect::Agent *>(m_config->getAgent());
    ASSERT_TRUE(agent);
    printer = dynamic_cast<mtconnect::XmlPrinter *>(agent->getPrinter("xml"));
    ASSERT_TRUE(printer);
    path = printer->getDevicesUrn("y");
    ASSERT_EQ(std::string("urn:example.com:ExampleDevices:1.2"), path);

    string asset(
        "AssetsNamespaces {\n"
        "z {\n"
        "Urn = urn:example.com:ExampleAssets:1.2\n"
        "Location = /schemas/ExampleAssets_1.2.xsd\n"
        "Path = ./ExampleAssets_1.2.xsd\n"
        "}\n"
        "}\n");

    m_config->loadConfig(asset);
    agent = const_cast<mtconnect::Agent *>(m_config->getAgent());
    ASSERT_TRUE(agent);
    printer = dynamic_cast<mtconnect::XmlPrinter *>(agent->getPrinter("xml"));
    ASSERT_TRUE(printer);
    path = printer->getAssetsUrn("z");
    ASSERT_EQ(std::string("urn:example.com:ExampleAssets:1.2"), path);

    string errors(
        "ErrorNamespaces {\n"
        "a {\n"
        "Urn = urn:example.com:ExampleErrors:1.2\n"
        "Location = /schemas/ExampleErrors_1.2.xsd\n"
        "Path = ./ExampleErrorss_1.2.xsd\n"
        "}\n"
        "}\n");

    m_config->loadConfig(errors);
    agent = const_cast<mtconnect::Agent *>(m_config->getAgent());
    ASSERT_TRUE(agent);
    printer = dynamic_cast<mtconnect::XmlPrinter *>(agent->getPrinter("xml"));
    ASSERT_TRUE(printer);
    path = printer->getErrorUrn("a");
    ASSERT_EQ(std::string("urn:example.com:ExampleErrors:1.2"), path);
  }

  TEST_F(ConfigTest, LegacyTimeout)
  {
    using namespace std::chrono_literals;

    string str("Devices = " PROJECT_ROOT_DIR
                           "/samples/test_config.xml\n"
                           "LegacyTimeout = 2000\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    const auto source = agent->getSources().front();
    const auto adapter = dynamic_pointer_cast<adapter::shdr::ShdrAdapter>(source);

    ASSERT_EQ(2000s, adapter->getLegacyTimeout());
  }

  TEST_F(ConfigTest, IgnoreTimestamps)
  {
    string str("Devices = " PROJECT_ROOT_DIR
                           "/samples/test_config.xml\n"
                           "IgnoreTimestamps = true\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    const auto source = agent->getSources().front();
    const auto adapter = dynamic_pointer_cast<adapter::Adapter>(source);

    ASSERT_TRUE(IsOptionSet(adapter->getOptions(), configuration::IgnoreTimestamps));
  }

  TEST_F(ConfigTest, IgnoreTimestampsOverride)
  {
    string str("Devices = " PROJECT_ROOT_DIR
                           "/samples/test_config.xml\n"
                           "IgnoreTimestamps = true\n"
                           "Adapters { LinuxCNC { \n"
                           "IgnoreTimestamps = false\n"
                           "} }\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    const auto source = agent->getSources().front();
    const auto adapter = dynamic_pointer_cast<adapter::Adapter>(source);

    ASSERT_FALSE(IsOptionSet(adapter->getOptions(), configuration::IgnoreTimestamps));
  }

  TEST_F(ConfigTest, SpecifyMTCNamespace)
  {
    chdir(TEST_BIN_ROOT_DIR);
    m_config->updateWorkingDirectory();
    string streams(
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
    m_config->updateWorkingDirectory();
    string streams("SchemaVersion = 1.4\n");

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
    m_config->updateWorkingDirectory();
    string schemas(
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
  }

  TEST_F(ConfigTest, check_http_headers)
  {
    chdir(TEST_BIN_ROOT_DIR);
    m_config->updateWorkingDirectory();

    string str("HttpHeaders {\n"
                           "  Access-Control-Allow-Origin = *\n"
                           "\n"
                           "}\n");
    m_config->loadConfig(str);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());
    ASSERT_TRUE(agent);

    ASSERT_TRUE(agent);
    const auto sink = agent->findSink("RestService");
    ASSERT_TRUE(sink);
    const auto rest = dynamic_pointer_cast<rest_sink::RestService>(sink);
    ASSERT_TRUE(rest);
    const auto server = rest->getServer();
    
    // TODO: Get headers working again
    const auto &headers = server->getHttpHeaders();
    
    ASSERT_EQ(1, headers.size());
    const auto &first = headers.front();
    ASSERT_EQ("Access-Control-Allow-Origin", first.first);
    ASSERT_EQ(" *", first.second);
  }

  TEST_F(ConfigTest, dynamic_load_sinks_bad)
  {
    chdir(TEST_BIN_ROOT_DIR);
    m_config->updateWorkingDirectory();

    string str("Sinks {\n"
                    "TestBADService {\n"
                    "}\n"
               "}\n");
    m_config->loadConfig(str);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());

    ASSERT_TRUE(agent);
    const auto sink = agent->findSink("TestBADService");
    ASSERT_TRUE(sink == nullptr);
  }

  TEST_F(ConfigTest, dynamic_load_sinks_simple)
  {
    chdir(TEST_BIN_ROOT_DIR);

    m_config->updateWorkingDirectory();

    string str("Sinks {\n"
                    "sink_plugin_test {\n"
                    "}\n"
               "}\n");
    m_config->loadConfig(str);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());

    ASSERT_TRUE(agent);
    const auto sink = agent->findSink("sink_plugin_test");
    ASSERT_TRUE(sink != nullptr);
  }

  TEST_F(ConfigTest, dynamic_load_sinks_assigned_name)
  {
    chdir(TEST_BIN_ROOT_DIR);

    m_config->updateWorkingDirectory();

    string str("Sinks {\n"
                    "sink_plugin_test:Sink1 {\n"
                    "}\n"
               "}\n");
    m_config->loadConfig(str);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());

    ASSERT_TRUE(agent);
    const auto sink1 = agent->findSink("sink_plugin_test");
    ASSERT_TRUE(sink1 == nullptr);

    const auto sink2 = agent->findSink("Sink1");
    ASSERT_TRUE(sink2 != nullptr);
  }

  //
  TEST_F(ConfigTest, dynamic_load_adapter_bad)
  {
    chdir(TEST_BIN_ROOT_DIR);
    m_config->updateWorkingDirectory();

    string str("Adapters {\n"
                    "BadAdapter:Test {\n"
                     "Host=Host1 \n"
                     "Port=7878 \n"
                    "}\n"
               "}\n");

    m_config->loadConfig(str);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());

    ASSERT_TRUE(agent);
    const auto adapter = agent->findSource("_Host1_7878");
    ASSERT_TRUE(adapter == nullptr);
  }

  TEST_F(ConfigTest, dynamic_load_adapter_simple)
  {
    chdir(TEST_BIN_ROOT_DIR);
    m_config->updateWorkingDirectory();

    string str("Adapters {\n"
                  "adapter_plugin_test:Test {\n"
                     "Host=Host1 \n"
                     "Port=7878 \n"
                  "}\n"
             "}\n");

    m_config->loadConfig(str);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());

    ASSERT_TRUE(agent);
    const auto adapter = agent->findSource("Simplest");
    ASSERT_TRUE(adapter != nullptr);
  }
}  // namespace
