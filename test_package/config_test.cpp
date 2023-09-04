//
// Copyright Copyright 2009-2022, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <boost/algorithm/string.hpp>

#include <chrono>
#include <filesystem>
#include <string>

#include "mtconnect/agent.hpp"
#include "mtconnect/configuration/agent_config.hpp"
#include "mtconnect/configuration/config_options.hpp"
#include "mtconnect/printer//xml_printer.hpp"
#include "mtconnect/sink/rest_sink/rest_service.hpp"
#include "mtconnect/source/adapter/shdr/shdr_adapter.hpp"

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#define chdir _chdir
#endif

using namespace std;
using namespace mtconnect;
using namespace mtconnect::configuration;
namespace fs = std::filesystem;
using namespace std::chrono_literals;
using namespace boost::algorithm;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

namespace {
  class ConfigTest : public testing::Test
  {
  protected:
    void SetUp() override
    {
      m_config = std::make_unique<AgentConfiguration>();
      m_config->setDebug(true);
      m_cwd = std::filesystem::current_path();

      chdir(TEST_BIN_ROOT_DIR);
      m_config->updateWorkingDirectory();
    }

    void TearDown() override
    {
      m_config.reset();
      chdir(m_cwd.string().c_str());
    }

    fs::path createTempDirectory(const string &ext)
    {
      fs::path root {fs::path(TEST_BIN_ROOT_DIR) / ("config_test_" + ext)};
      if (fs::exists(root))
      {
        fs::remove_all(root);
      }

      fs::create_directory(root);
      chdir(root.string().c_str());
      m_config->updateWorkingDirectory();
      // m_config->setDebug(false);

      return root;
    }

    fs::path copyFile(const std::string &src, fs::path target, chrono::seconds delta)
    {
      fs::path file {fs::path(TEST_RESOURCE_DIR) / "samples" / src};

      fs::copy_file(file, target, fs::copy_options::overwrite_existing);
      auto t = fs::last_write_time(target);
      if (delta.count() != 0)
        fs::last_write_time(target, t - delta);

      return target;
    }

    void replaceTextInFile(fs::path file, const std::string &from, const std::string &to)
    {
      ifstream is {file.string(), ios::binary | ios::ate};
      auto size = is.tellg();
      string str(size, '\0');  // construct string to stream size
      is.seekg(0);
      is.read(&str[0], size);
      is.close();

      replace_all(str, from, to);

      ofstream os(file.string());
      os << str;
      os.close();
    }

    std::unique_ptr<AgentConfiguration> m_config;
    std::filesystem::path m_cwd;
  };

  TEST_F(ConfigTest, BlankConfig)
  {
    m_config->loadConfig("");

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    ASSERT_EQ(size_t(1), agent->getDevices().size());
    ASSERT_EQ("1.1", *agent->getSchemaVersion());
  }

  TEST_F(ConfigTest, BufferSize)
  {
    m_config->loadConfig("BufferSize = 4\n");

    auto agent = m_config->getAgent();
    auto &circ = agent->getCircularBuffer();

    ASSERT_TRUE(agent);
    ASSERT_EQ(16U, circ.getBufferSize());
  }

  TEST_F(ConfigTest, Device)
  {
    string str("Devices = " TEST_RESOURCE_DIR "/samples/test_config.xml\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    const auto source = agent->getSources().back();
    const auto adapter = dynamic_pointer_cast<source::adapter::Adapter>(source);

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

    string str("Devices = " TEST_RESOURCE_DIR
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
    const auto source = agent->getSources().back();
    const auto adapter = dynamic_pointer_cast<source::adapter::shdr::ShdrAdapter>(source);

    ASSERT_EQ(23, (int)adapter->getPort());
    ASSERT_EQ(std::string("10.211.55.1"), adapter->getServer());
    ASSERT_TRUE(IsOptionSet(adapter->getOptions(), configuration::FilterDuplicates));
    ASSERT_TRUE(IsOptionSet(adapter->getOptions(), configuration::AutoAvailable));
    ASSERT_TRUE(IsOptionSet(adapter->getOptions(), configuration::IgnoreTimestamps));

    ASSERT_EQ(2000s, adapter->getLegacyTimeout());

    // TODO: Need to link to device to the adapter.
    // ASSERT_TRUE(device->m_preserveUuid);
  }

  TEST_F(ConfigTest, DefaultPreserveUUID)
  {
    string str("Devices = " TEST_RESOURCE_DIR
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
    string str("Devices = " TEST_RESOURCE_DIR
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
    string str("Devices = " TEST_RESOURCE_DIR
               "/samples/test_config.xml\n"
               "AllowPut = true\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    const auto sink = agent->findSink("RestService");
    ASSERT_TRUE(sink);
    const auto rest = dynamic_pointer_cast<sink::rest_sink::RestService>(sink);

    ASSERT_TRUE(rest->getServer()->arePutsAllowed());
  }

  TEST_F(ConfigTest, LimitPut)
  {
    string str("Devices = " TEST_RESOURCE_DIR
               "/samples/test_config.xml\n"
               "AllowPutFrom = localhost\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    const auto sink = agent->findSink("RestService");
    ASSERT_TRUE(sink);
    const auto rest = dynamic_pointer_cast<sink::rest_sink::RestService>(sink);
    ASSERT_TRUE(rest);

    ASSERT_TRUE(rest->getServer()->arePutsAllowed());
    ASSERT_TRUE(rest->getServer()->allowPutFrom(std::string("127.0.0.1")));
  }

  TEST_F(ConfigTest, LimitPutFromHosts)
  {
    string str("Devices = " TEST_RESOURCE_DIR
               "/samples/test_config.xml\n"
               "AllowPutFrom = localhost, 192.168.0.1\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    const auto sink = agent->findSink("RestService");
    ASSERT_TRUE(sink);
    const auto rest = dynamic_pointer_cast<sink::rest_sink::RestService>(sink);
    ASSERT_TRUE(rest);

    ASSERT_TRUE(rest->getServer()->arePutsAllowed());
    ASSERT_TRUE(rest->getServer()->allowPutFrom(std::string("127.0.0.1")));
    ASSERT_TRUE(rest->getServer()->allowPutFrom(std::string("192.168.0.1")));
  }

  TEST_F(ConfigTest, Namespaces)
  {
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
    auto printer = dynamic_cast<printer::XmlPrinter *>(agent->getPrinter("xml"));
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
    printer = dynamic_cast<printer::XmlPrinter *>(agent->getPrinter("xml"));
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
    printer = dynamic_cast<printer::XmlPrinter *>(agent->getPrinter("xml"));
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
    printer = dynamic_cast<printer::XmlPrinter *>(agent->getPrinter("xml"));
    ASSERT_TRUE(printer);
    path = printer->getErrorUrn("a");
    ASSERT_EQ(std::string("urn:example.com:ExampleErrors:1.2"), path);
  }

  TEST_F(ConfigTest, LegacyTimeout)
  {
    using namespace std::chrono_literals;

    string str("Devices = " TEST_RESOURCE_DIR
               "/samples/test_config.xml\n"
               "LegacyTimeout = 2000\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    const auto source = agent->getSources().back();
    const auto adapter = dynamic_pointer_cast<source::adapter::shdr::ShdrAdapter>(source);

    ASSERT_EQ(2000s, adapter->getLegacyTimeout());
  }

  TEST_F(ConfigTest, IgnoreTimestamps)
  {
    string str("Devices = " TEST_RESOURCE_DIR
               "/samples/test_config.xml\n"
               "IgnoreTimestamps = true\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    const auto source = agent->getSources().back();
    const auto adapter = dynamic_pointer_cast<source::adapter::Adapter>(source);

    ASSERT_TRUE(IsOptionSet(adapter->getOptions(), configuration::IgnoreTimestamps));
  }

  TEST_F(ConfigTest, IgnoreTimestampsOverride)
  {
    string str("Devices = " TEST_RESOURCE_DIR
               "/samples/test_config.xml\n"
               "IgnoreTimestamps = true\n"
               "Adapters { LinuxCNC { \n"
               "IgnoreTimestamps = false\n"
               "} }\n");
    m_config->loadConfig(str);

    const auto agent = m_config->getAgent();
    ASSERT_TRUE(agent);
    const auto source = agent->getSources().back();
    const auto adapter = dynamic_pointer_cast<source::adapter::Adapter>(source);

    ASSERT_FALSE(IsOptionSet(adapter->getOptions(), configuration::IgnoreTimestamps));
  }

  TEST_F(ConfigTest, SpecifyMTCNamespace)
  {
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
    auto printer = dynamic_cast<printer::XmlPrinter *>(agent->getPrinter("xml"));
    ASSERT_TRUE(printer);

    auto path = printer->getStreamsUrn("m");
    ASSERT_EQ(std::string(""), path);
    auto location = printer->getStreamsLocation("m");
    ASSERT_EQ(std::string("/schemas/MTConnectStreams_1.2.xsd"), location);

    printer->clearStreamsNamespaces();
  }

  TEST_F(ConfigTest, SetSchemaVersion)
  {
    string streams("SchemaVersion = 1.4\n");

    m_config->loadConfig(streams);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());
    ASSERT_TRUE(agent);
    auto printer = dynamic_cast<printer::XmlPrinter *>(agent->getPrinter("xml"));
    ASSERT_TRUE(printer);

    auto version = printer->getSchemaVersion();
    ASSERT_EQ(std::string("1.4"), version);

    printer->setSchemaVersion("1.3");
  }

  TEST_F(ConfigTest, SchemaDirectory)
  {
    string schemas(
        "SchemaVersion = 1.3\n"
        "Files {\n"
        "schemas {\n"
        "Location = /schemas\n"
        "Path = " PROJECT_ROOT_DIR
        "/schemas\n"
        "}\n"
        "}\n"
        "logger_config {\n"
        "output = cout\n"
        "}\n");

    m_config->setDebug(true);
    m_config->loadConfig(schemas);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());
    ASSERT_TRUE(agent);
    auto printer = dynamic_cast<printer::XmlPrinter *>(agent->getPrinter("xml"));
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
    string str(
        "HttpHeaders {\n"
        "  Access-Control-Allow-Origin = *\n"
        "\n"
        "}\n");
    m_config->loadConfig(str);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());
    ASSERT_TRUE(agent);

    ASSERT_TRUE(agent);
    const auto sink = agent->findSink("RestService");
    ASSERT_TRUE(sink);
    const auto rest = dynamic_pointer_cast<sink::rest_sink::RestService>(sink);
    ASSERT_TRUE(rest);
    const auto server = rest->getServer();

    const auto &headers = server->getHttpHeaders();

    ASSERT_EQ(1, headers.size());
    const auto &first = headers.front();
    ASSERT_EQ("Access-Control-Allow-Origin", first.first);
    ASSERT_EQ(" *", first.second);
  }

  TEST_F(ConfigTest, dynamic_load_sinks_bad)
  {
    string str(R"(
Plugins {
    TestBADService {
    }
}
Sinks {
    TestBADService {
    }
}
)");

    m_config->loadConfig(str);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());

    ASSERT_TRUE(agent);
    const auto sink = agent->findSink("TestBADService");
    ASSERT_TRUE(sink == nullptr);
  }

  TEST_F(ConfigTest, dynamic_load_sinks_simple)
  {
    string str(R"(
Sinks {
      sink_plugin_test {
    }
}
)");

    m_config->loadConfig(str);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());

    ASSERT_TRUE(agent);

    const auto sink = agent->findSink("sink_plugin_test");
    ASSERT_TRUE(sink != nullptr);
  }

  TEST_F(ConfigTest, dynamic_load_sinks_with_plugin_block)
  {
    string str(R"(
Plugins {
   sink_plugin_test {
   }
}
Sinks {
      sink_plugin_test {
    }
}
)");

    m_config->loadConfig(str);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());

    ASSERT_TRUE(agent);

    const auto sink = agent->findSink("sink_plugin_test");
    ASSERT_TRUE(sink != nullptr);
  }

  TEST_F(ConfigTest, dynamic_load_sinks_assigned_name)
  {
    string str(R"(
Sinks {
      sink_plugin_test:Sink1 {
    }
}
)");

    m_config->loadConfig(str);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());

    ASSERT_TRUE(agent);
    const auto sink1 = agent->findSink("sink_plugin_test");
    ASSERT_TRUE(sink1 == nullptr);

    const auto sink2 = agent->findSink("Sink1");
    ASSERT_TRUE(sink2 != nullptr);
  }

  TEST_F(ConfigTest, dynamic_load_sinks_assigned_name_tag)
  {
    string str(R"(
Sinks {
      sink_plugin_test {
        Name = Sink1
    }
}
)");

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
    string str(R"(
Adapters {
  BadAdapter:Test {
    Host=Host1
    Port=7878
  }
}
)");

    m_config->loadConfig(str);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());

    ASSERT_TRUE(agent);
    const auto adapter = agent->findSource("_Host1_7878");
    ASSERT_TRUE(adapter == nullptr);
  }

  TEST_F(ConfigTest, dynamic_load_adapter_simple)
  {
    string str(R"(
Adapters {
    adapter_plugin_test:Test {
    Host=Host1
    Port=7878
  }
}
)");

    m_config->loadConfig(str);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());

    ASSERT_TRUE(agent);
    const auto adapter = agent->findSource("Test");
    ASSERT_TRUE(adapter != nullptr);
  }

  TEST_F(ConfigTest, dynamic_load_adapter_with_plugin_block)
  {
    string str(R"(
Plugins {
    adapter_plugin_test {
    }
}
Adapters {
  Test {
    Host=Host1
    Port=7878
    Protocol = adapter_plugin_test
  }
}
)");

    m_config->loadConfig(str);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());

    ASSERT_TRUE(agent);
    const auto adapter = agent->findSource("Test");
    ASSERT_TRUE(adapter != nullptr);
  }

  TEST_F(ConfigTest, max_cache_size_in_no_units)
  {
    string str(R"(
MaxCachedFileSize = 2000
)");

    m_config->loadConfig(str);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());

    ASSERT_TRUE(agent);
    const auto rest =
        dynamic_pointer_cast<sink::rest_sink::RestService>(agent->findSink("RestService"));
    ASSERT_TRUE(rest != nullptr);

    auto cache = rest->getFileCache();
    ASSERT_EQ(2000, cache->getMaxCachedFileSize());
  }

  TEST_F(ConfigTest, max_cache_size_in_kb)
  {
    string str(R"(
MaxCachedFileSize = 2k
)");

    m_config->loadConfig(str);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());

    ASSERT_TRUE(agent);
    const auto rest =
        dynamic_pointer_cast<sink::rest_sink::RestService>(agent->findSink("RestService"));
    ASSERT_TRUE(rest != nullptr);

    auto cache = rest->getFileCache();
    ASSERT_EQ(2048, cache->getMaxCachedFileSize());
  }

  TEST_F(ConfigTest, max_cache_size_in_Kb_in_uppercase)
  {
    string str(R"(
MaxCachedFileSize = 2K
)");

    m_config->loadConfig(str);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());

    ASSERT_TRUE(agent);
    const auto rest =
        dynamic_pointer_cast<sink::rest_sink::RestService>(agent->findSink("RestService"));
    ASSERT_TRUE(rest != nullptr);

    auto cache = rest->getFileCache();
    ASSERT_EQ(2048, cache->getMaxCachedFileSize());
  }

  TEST_F(ConfigTest, max_cache_size_in_mb)
  {
    string str(R"(
MaxCachedFileSize = 2m
)");

    m_config->loadConfig(str);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());

    ASSERT_TRUE(agent);
    const auto rest =
        dynamic_pointer_cast<sink::rest_sink::RestService>(agent->findSink("RestService"));
    ASSERT_TRUE(rest != nullptr);

    auto cache = rest->getFileCache();
    ASSERT_EQ(2 * 1024 * 1024, cache->getMaxCachedFileSize());
  }

  TEST_F(ConfigTest, max_cache_size_in_gb)
  {
    string str(R"(
MaxCachedFileSize = 2g
)");

    m_config->loadConfig(str);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());

    ASSERT_TRUE(agent);
    const auto rest =
        dynamic_pointer_cast<sink::rest_sink::RestService>(agent->findSink("RestService"));
    ASSERT_TRUE(rest != nullptr);

    auto cache = rest->getFileCache();
    ASSERT_EQ(2ull * 1024 * 1024 * 1024, cache->getMaxCachedFileSize());
  }

#define EXPECT_PATH_EQ(p1, p2) \
  EXPECT_EQ(std::filesystem::weakly_canonical(p1), std::filesystem::weakly_canonical(p2))

  TEST_F(ConfigTest, log_output_should_set_archive_file_pattern)
  {
    m_config->setDebug(false);

    string str(R"(
logger_config {
  output = file agent.log
}
)");

    m_config->loadConfig(str);

    auto sink = m_config->getLoggerSink();
    ASSERT_TRUE(sink);

    EXPECT_EQ("agent_%Y-%m-%d_%H-%M-%S_%N.log", m_config->getLogArchivePattern().filename());
    EXPECT_EQ("agent.log", m_config->getLogFileName().filename());
    EXPECT_PATH_EQ(TEST_BIN_ROOT_DIR, m_config->getLogDirectory());
  }

  TEST_F(ConfigTest, log_output_should_configure_file_name)
  {
    m_config->setDebug(false);

    string str(R"(
logger_config {
  output = file logging.log logging_%N.log
}
)");

    m_config->loadConfig(str);

    auto sink = m_config->getLoggerSink();
    ASSERT_TRUE(sink);

    EXPECT_EQ("logging_%N.log", m_config->getLogArchivePattern().filename());
    EXPECT_EQ("logging.log", m_config->getLogFileName().filename());
    EXPECT_PATH_EQ(TEST_BIN_ROOT_DIR, m_config->getLogDirectory());
  }

  TEST_F(ConfigTest, log_should_configure_file_name)
  {
    m_config->setDebug(false);

    string str(R"(
logger_config {
  file_name = logging.log
  archive_pattern = logging_%N.log
}
)");

    m_config->loadConfig(str);

    auto sink = m_config->getLoggerSink();
    ASSERT_TRUE(sink);

    EXPECT_EQ("logging_%N.log", m_config->getLogArchivePattern().filename());
    EXPECT_EQ("logging.log", m_config->getLogFileName().filename());
    EXPECT_PATH_EQ(TEST_BIN_ROOT_DIR, m_config->getLogDirectory());
  }

  TEST_F(ConfigTest, log_should_specify_relative_directory)
  {
    m_config->setDebug(false);

    string str(R"(
logger_config {
  file_name = logging.log
  archive_pattern = logs/logging_%N.log
}
)");

    m_config->loadConfig(str);

    auto sink = m_config->getLoggerSink();
    ASSERT_TRUE(sink);

    fs::path path {std::filesystem::canonical(TEST_BIN_ROOT_DIR) / "logs"};

    EXPECT_PATH_EQ(path / "logging_%N.log", m_config->getLogArchivePattern());
    EXPECT_PATH_EQ(path / "logging.log", m_config->getLogFileName());
    EXPECT_PATH_EQ(path, m_config->getLogDirectory());
  }

  TEST_F(ConfigTest, log_should_specify_relative_directory_with_active_in_parent)
  {
    m_config->setDebug(false);

    string str(R"(
logger_config {
  file_name = ./logging.log
  archive_pattern = logs/logging_%N.log
}
)");

    m_config->loadConfig(str);

    auto sink = m_config->getLoggerSink();
    ASSERT_TRUE(sink);

    fs::path path {std::filesystem::canonical(TEST_BIN_ROOT_DIR)};

    EXPECT_PATH_EQ(path / "logs" / "logging_%N.log", m_config->getLogArchivePattern());
    EXPECT_PATH_EQ(path / "logging.log", m_config->getLogFileName());
    EXPECT_PATH_EQ(path / "logs", m_config->getLogDirectory());
  }

  TEST_F(ConfigTest, log_should_specify_max_file_and_rotation_size)
  {
    m_config->setDebug(false);
    using namespace boost::log::trivial;

    string str(R"(
logger_config {
  max_size = 1gb
  rotation_size = 20gb
}
)");

    m_config->loadConfig(str);

    auto sink = m_config->getLoggerSink();
    ASSERT_TRUE(sink);

    EXPECT_EQ(severity_level::info, m_config->getLogLevel());
    EXPECT_EQ(1ll * 1024 * 1024 * 1024, m_config->getMaxLogFileSize());
    EXPECT_EQ(20ll * 1024 * 1024 * 1024, m_config->getLogRotationSize());
  }

  TEST_F(ConfigTest, log_should_configure_logging_level)
  {
    m_config->setDebug(false);

    using namespace boost::log::trivial;

    string str(R"(
logger_config {
   level = fatal
}
)");

    m_config->loadConfig(str);

    auto sink = m_config->getLoggerSink();
    ASSERT_TRUE(sink);

    EXPECT_EQ(severity_level::fatal, m_config->getLogLevel());

    m_config->setLoggingLevel("all");
    EXPECT_EQ(severity_level::trace, m_config->getLogLevel());
    m_config->setLoggingLevel("none");
    EXPECT_EQ(severity_level::fatal, m_config->getLogLevel());
    m_config->setLoggingLevel("trace");
    EXPECT_EQ(severity_level::trace, m_config->getLogLevel());
    m_config->setLoggingLevel("debug");
    EXPECT_EQ(severity_level::debug, m_config->getLogLevel());
    m_config->setLoggingLevel("info");
    EXPECT_EQ(severity_level::info, m_config->getLogLevel());
    m_config->setLoggingLevel("lwarn");
    EXPECT_EQ(severity_level::warning, m_config->getLogLevel()) << "lwarn";
    m_config->setLoggingLevel("lwarning");
    EXPECT_EQ(severity_level::warning, m_config->getLogLevel()) << "lwarning";
    m_config->setLoggingLevel("warning");
    EXPECT_EQ(severity_level::warning, m_config->getLogLevel()) << "warning";
    m_config->setLoggingLevel("error");
    EXPECT_EQ(severity_level::error, m_config->getLogLevel());
    m_config->setLoggingLevel("fatal");
    EXPECT_EQ(severity_level::fatal, m_config->getLogLevel());

    m_config->setLoggingLevel("ALL");
    EXPECT_EQ(severity_level::trace, m_config->getLogLevel());
    m_config->setLoggingLevel("NONE");
    EXPECT_EQ(severity_level::fatal, m_config->getLogLevel());
    m_config->setLoggingLevel("TRACE");
    EXPECT_EQ(severity_level::trace, m_config->getLogLevel());
    m_config->setLoggingLevel("DEBUG");
    EXPECT_EQ(severity_level::debug, m_config->getLogLevel());
    m_config->setLoggingLevel("INFO");
    EXPECT_EQ(severity_level::info, m_config->getLogLevel());
    m_config->setLoggingLevel("LWARN");
    EXPECT_EQ(severity_level::warning, m_config->getLogLevel()) << "LWARN";
    m_config->setLoggingLevel("LWARNING");
    EXPECT_EQ(severity_level::warning, m_config->getLogLevel()) << "LWARNING";
    m_config->setLoggingLevel("WARNING");
    EXPECT_EQ(severity_level::warning, m_config->getLogLevel()) << "WARNING";
    m_config->setLoggingLevel("ERROR");
    EXPECT_EQ(severity_level::error, m_config->getLogLevel());
    m_config->setLoggingLevel("FATAL");
    EXPECT_EQ(severity_level::fatal, m_config->getLogLevel());
  }

  TEST_F(ConfigTest, should_reload_device_xml_file)
  {
    auto root {createTempDirectory("1")};

    fs::path devices(root / "Devices.xml");
    fs::path config {root / "agent.cfg"};
    {
      ofstream cfg(config.string());
      cfg << R"DOC(
MonitorConfigFiles = true
MonitorInterval = 1
MinimumConfigReloadAge = 1
Port = 0
)DOC";
      cfg << "Devices = " << devices << endl;
    }

    copyFile("min_config.xml", devices, 60min);

    boost::program_options::variables_map options;
    boost::program_options::variable_value value(boost::optional<string>(config.string()), false);
    options.insert(make_pair("config-file"s, value));

    m_config->initialize(options);
    auto &context = m_config->getAsyncContext();

    auto agent = m_config->getAgent();
    const auto &printer = agent->getPrinter("xml");
    ASSERT_NE(nullptr, printer);

    auto chg = printer->getModelChangeTime();
    auto device = agent->getDeviceByName("LinuxCNC");

    auto dataItem = device->getDeviceDataItem("c1");
    ASSERT_TRUE(dataItem);
    ASSERT_EQ("SPINDLE_SPEED", dataItem->getType());

    boost::asio::steady_timer timer1(context.get());
    timer1.expires_from_now(1s);
    timer1.async_wait([this, &devices, agent](boost::system::error_code ec) {
      if (ec)
      {
        m_config->stop();
      }
      else
      {
        auto di = agent->getDataItemById("c1");
        EXPECT_TRUE(di);
        EXPECT_EQ("SPINDLE_SPEED", di->getType());
        di.reset();

        // Modify devices
        replaceTextInFile(devices, "SPINDLE_SPEED", "ROTARY_VELOCITY");
      }
    });

    boost::asio::steady_timer timer2(context.get());
    timer2.expires_from_now(6s);
    timer2.async_wait([this, agent, &chg](boost::system::error_code ec) {
      if (!ec)
      {
        auto device = agent->getDeviceByName("LinuxCNC");
        auto dataItem = agent->getDataItemById("c1");
        EXPECT_TRUE(dataItem);
        EXPECT_EQ("ROTARY_VELOCITY", dataItem->getType());

        EXPECT_FALSE(dataItem->isOrphan());

        auto agent = m_config->getAgent();
        const auto &printer = agent->getPrinter("xml");
        EXPECT_NE(nullptr, printer);
        EXPECT_NE(chg, printer->getModelChangeTime());
      }
      m_config->stop();
    });

    m_config->start();
  }

  TEST_F(ConfigTest, should_reload_device_xml_and_skip_unchanged_devices)
  {
    fs::path root {createTempDirectory("2")};

    fs::path devices(root / "Devices.xml");
    fs::path config {root / "agent.cfg"};
    {
      ofstream cfg(config.string());
      cfg << R"DOC(
MonitorConfigFiles = true
MonitorInterval = 1
MinimumConfigReloadAge = 1
Port = 0
)DOC";
      cfg << "Devices = " << devices << endl;
    }

    copyFile("min_config.xml", devices, 1min);

    boost::program_options::variables_map options;
    boost::program_options::variable_value value(boost::optional<string>(config.string()), false);
    options.insert(make_pair("config-file"s, value));

    m_config->initialize(options);
    auto &context = m_config->getAsyncContext();

    auto agent = m_config->getAgent();
    const auto &printer = agent->getPrinter("xml");
    ASSERT_NE(nullptr, printer);

    auto chg = printer->getModelChangeTime();
    auto device = agent->getDeviceByName("LinuxCNC");

    auto dataItem = device->getDeviceDataItem("c1");
    ASSERT_TRUE(dataItem);
    ASSERT_EQ("SPINDLE_SPEED", dataItem->getType());

    boost::asio::steady_timer timer1(context.get());
    timer1.expires_from_now(1s);
    timer1.async_wait([this, &devices](boost::system::error_code ec) {
      if (ec)
      {
        m_config->stop();
      }
      else
      {
        fs::last_write_time(devices, fs::file_time_type::clock::now());
      }
    });

    boost::asio::steady_timer timer2(context.get());
    timer2.expires_from_now(6s);
    timer2.async_wait([this, &chg](boost::system::error_code ec) {
      if (!ec)
      {
        auto agent = m_config->getAgent();
        const auto &printer = agent->getPrinter("xml");
        EXPECT_NE(nullptr, printer);
        EXPECT_EQ(chg, printer->getModelChangeTime());
      }
      m_config->stop();
    });

    m_config->start();
  }

  TEST_F(ConfigTest, should_restart_agent_when_config_file_changes)
  {
    fs::path root {createTempDirectory("3")};
    auto &context = m_config->getAsyncContext();

    fs::path devices(root / "Devices.xml");
    fs::path config {root / "agent.cfg"};
    {
      ofstream cfg(config.string());
      cfg << R"DOC(
MonitorConfigFiles = true
MonitorInterval = 1
MinimumConfigReloadAge = 1
Port = 0
)DOC";
      cfg << "Devices = " << devices << endl;
    }

    copyFile("min_config.xml", devices, 0s);

    boost::program_options::variables_map options;
    boost::program_options::variable_value value(boost::optional<string>(config.string()), false);
    options.insert(make_pair("config-file"s, value));

    auto t = fs::last_write_time(config);
    fs::last_write_time(config, t - 1min);

    m_config->initialize(options);

    auto agent = m_config->getAgent();
    const auto sink = agent->findSink("RestService");
    ASSERT_TRUE(sink);
    const auto rest = dynamic_pointer_cast<sink::rest_sink::RestService>(sink);
    ASSERT_TRUE(rest);

    auto instance = rest->instanceId();

    boost::asio::steady_timer timer1(context.get());
    timer1.expires_from_now(1s);
    timer1.async_wait([this, &config](boost::system::error_code ec) {
      if (ec)
      {
        m_config->stop();
      }
      else
      {
        fs::last_write_time(config, fs::file_time_type::clock::now());
      }
    });

    auto th = thread([this, agent, instance, &context]() {
      this_thread::sleep_for(5s);

      boost::asio::steady_timer timer1(context.get());
      timer1.expires_from_now(1s);
      timer1.async_wait([this, agent, instance](boost::system::error_code ec) {
        if (!ec)
        {
          auto agent2 = m_config->getAgent();
          const auto sink = agent2->findSink("RestService");
          EXPECT_TRUE(sink);
          const auto rest = dynamic_pointer_cast<sink::rest_sink::RestService>(sink);
          EXPECT_TRUE(rest);

          EXPECT_NE(agent, agent2);
          EXPECT_NE(instance, rest->instanceId());
        }
      });
      m_config->stop();
    });

    m_config->start();
    th.join();
  }

  TEST_F(ConfigTest, should_reload_device_xml_and_add_new_devices)
  {
    fs::path root {createTempDirectory("4")};

    fs::path devices(root / "Devices.xml");
    fs::path config {root / "agent.cfg"};
    {
      ofstream cfg(config.string());
      cfg << R"DOC(
MonitorConfigFiles = true
MonitorInterval = 1
MinimumConfigReloadAge = 1
Port = 0
)DOC";
      cfg << "Devices = " << devices << endl;
    }

    copyFile("min_config.xml", devices, 1min);

    boost::program_options::variables_map options;
    boost::program_options::variable_value value(boost::optional<string>(config.string()), false);
    options.insert(make_pair("config-file"s, value));

    m_config->initialize(options);
    auto &context = m_config->getAsyncContext();

    auto agent = m_config->getAgent();
    const auto &printer = agent->getPrinter("xml");
    ASSERT_NE(nullptr, printer);

    auto chg = printer->getModelChangeTime();
    auto device = agent->getDeviceByName("LinuxCNC");

    auto dataItem = device->getDeviceDataItem("c1");
    ASSERT_TRUE(dataItem);
    ASSERT_EQ("SPINDLE_SPEED", dataItem->getType());

    DataItemPtr di;

    boost::asio::steady_timer timer1(context.get());
    timer1.expires_from_now(1s);
    timer1.async_wait([this, &devices](boost::system::error_code ec) {
      if (ec)
      {
        m_config->stop();
      }
      else
      {
        fs::copy_file(fs::path(TEST_RESOURCE_DIR) / "samples" / "min_config2.xml", devices,
                      fs::copy_options::overwrite_existing);
      }
    });

    boost::asio::steady_timer timer2(context.get());
    timer2.expires_from_now(6s);
    timer2.async_wait([this](boost::system::error_code ec) {
      if (!ec)
      {
        auto agent = m_config->getAgent();
        auto devices = agent->getDevices();
        EXPECT_EQ(3, devices.size());

        auto last = devices.back();
        EXPECT_TRUE(last);
        EXPECT_EQ("001", last->getUuid());

        const auto &dis = last->getDeviceDataItems();
        EXPECT_EQ(5, dis.size());

        EXPECT_TRUE(last->getDeviceDataItem("xd1"));
        EXPECT_TRUE(last->getDeviceDataItem("xex"));
        EXPECT_TRUE(last->getDeviceDataItem("o1_asset_chg"));
        EXPECT_TRUE(last->getDeviceDataItem("o1_asset_rem"));
        EXPECT_TRUE(last->getDeviceDataItem("o1_asset_count"));

        EXPECT_TRUE(agent->getDataItemById("xd1"));
        EXPECT_TRUE(agent->getDataItemById("xex"));
        EXPECT_TRUE(agent->getDataItemById("o1_asset_rem"));
        EXPECT_TRUE(agent->getDataItemById("o1_asset_chg"));
        EXPECT_TRUE(agent->getDataItemById("o1_asset_count"));
      }
      m_config->stop();
    });

    m_config->start();
  }

  TEST_F(ConfigTest, should_disable_agent_device)
  {
    string streams("SchemaVersion = 2.0\nDisableAgentDevice = true\n");

    m_config->loadConfig(streams);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());
    ASSERT_TRUE(agent);

    auto devices = agent->getDevices();
    ASSERT_EQ(1, devices.size());

    auto device = devices.front();
    ASSERT_EQ("Device", device->getName());
  }

  TEST_F(ConfigTest, should_default_not_disable_agent_device)
  {
    string streams("SchemaVersion = 2.0\n");

    m_config->loadConfig(streams);
    auto agent = const_cast<mtconnect::Agent *>(m_config->getAgent());
    ASSERT_TRUE(agent);

    auto devices = agent->getDevices();
    ASSERT_EQ(2, devices.size());

    auto device = devices.front();
    ASSERT_EQ("Agent", device->getName());
  }

  TEST_F(ConfigTest, should_update_schema_version_when_device_file_updates)
  {
    auto root {createTempDirectory("5")};

    fs::path devices(root / "Devices.xml");
    fs::path config {root / "agent.cfg"};
    {
      ofstream cfg(config.string());
      cfg << R"DOC(
MonitorConfigFiles = true
MonitorInterval = 1
MinimumConfigReloadAge = 1
Port = 0
)DOC";
      cfg << "Devices = " << devices << endl;
    }

    copyFile("min_config.xml", devices, 10min);
    replaceTextInFile(devices, "2.0", "1.2");

    boost::program_options::variables_map options;
    boost::program_options::variable_value value(boost::optional<string>(config.string()), false);
    options.insert(make_pair("config-file"s, value));

    m_config->initialize(options);
    auto agent = m_config->getAgent();
    auto &context = m_config->getAsyncContext();
    auto sink = agent->findSink("RestService");
    auto rest = dynamic_pointer_cast<sink::rest_sink::RestService>(sink);
    ASSERT_TRUE(rest);

    auto instance = rest->instanceId();
    sink.reset();
    rest.reset();

    const auto &printer = agent->getPrinter("xml");
    ASSERT_NE(nullptr, printer);
    ASSERT_EQ("1.2", *printer->getSchemaVersion());

    boost::asio::steady_timer timer1(context.get());
    timer1.expires_from_now(1s);
    timer1.async_wait([this, &devices, agent](boost::system::error_code ec) {
      if (ec)
      {
        m_config->stop();
      }
      else
      {
        auto di = agent->getDataItemById("c1");
        EXPECT_TRUE(di);
        EXPECT_EQ("SPINDLE_SPEED", di->getType());
        di.reset();

        // Modify devices
        replaceTextInFile(devices, "SPINDLE_SPEED", "ROTARY_VELOCITY");
        replaceTextInFile(devices, "1.2", "1.3");
      }
    });

    auto th = thread([this, agent, instance, &context]() {
      this_thread::sleep_for(5s);

      boost::asio::steady_timer timer1(context.get());
      timer1.expires_from_now(1s);
      timer1.async_wait([this, agent, instance](boost::system::error_code ec) {
        if (!ec)
        {
          auto agent2 = m_config->getAgent();
          const auto sink = agent2->findSink("RestService");
          EXPECT_TRUE(sink);
          const auto rest = dynamic_pointer_cast<sink::rest_sink::RestService>(sink);
          EXPECT_TRUE(rest);

          EXPECT_NE(agent, agent2);
          EXPECT_NE(instance, rest->instanceId());

          auto dataItem = agent2->getDataItemById("c1");
          EXPECT_TRUE(dataItem);
          EXPECT_EQ("ROTARY_VELOCITY", dataItem->getType());

          const auto &printer = agent2->getPrinter("xml");
          EXPECT_NE(nullptr, printer);
          ASSERT_EQ("1.3", *printer->getSchemaVersion());
        }
      });

      m_config->stop();
    });

    m_config->start();
    th.join();
  }

  TEST_F(ConfigTest, should_add_a_new_device_when_deviceModel_received_from_adapter)
  {
    using namespace mtconnect::source::adapter;

    fs::path root {createTempDirectory("6")};

    fs::path devices(root / "Devices.xml");
    fs::path config {root / "agent.cfg"};
    {
      ofstream cfg(config.string());
      cfg << R"DOC(
VersionDeviceXml = true
EnableSourceDeviceModels = true
Port = 0

Adapters {
  Device {
  }
}
)DOC";
      cfg << "Devices = " << devices << endl;
    }

    copyFile("empty.xml", devices, 0min);

    boost::program_options::variables_map options;
    boost::program_options::variable_value value(boost::optional<string>(config.string()), false);
    options.insert(make_pair("config-file"s, value));

    m_config->initialize(options);
    auto &asyncContext = m_config->getAsyncContext();

    auto agent = m_config->getAgent();
    const auto &printer = agent->getPrinter("xml");
    ASSERT_NE(nullptr, printer);

    auto sp = agent->findSource("_localhost_7878");
    ASSERT_TRUE(sp);

    auto adapter = dynamic_pointer_cast<shdr::ShdrAdapter>(sp);
    ASSERT_TRUE(adapter);

    auto validate = [&](boost::system::error_code ec) {
      using namespace std::filesystem;
      using namespace std::chrono;
      using namespace boost::algorithm;

      if (!ec)
      {
        // Check for backup file
        auto ext = date::format(".%Y-%m-%dT%H+", date::floor<seconds>(system_clock::now()));
        auto dit = directory_iterator(".");
        std::list<directory_entry> entries;
        copy_if(dit, end(dit), back_inserter(entries),
                [&ext](const auto &de) { return contains(de.path().string(), ext); });
        ASSERT_EQ(1, entries.size());

        auto device = agent->getDeviceByName("LinuxCNC");
        ASSERT_TRUE(device) << "Cannot find LinuxCNC device";

        const auto &components = device->getChildren();
        ASSERT_EQ(1, components->size());

        auto cont = device->getComponentById("cont");
        ASSERT_TRUE(cont) << "Cannot find Component with id cont";

        auto exec = device->getDeviceDataItem("exec");
        ASSERT_TRUE(exec) << "Cannot find DataItem with id exec";

        auto pipeline = dynamic_cast<AdapterPipeline *>(adapter->getPipeline());
        ASSERT_EQ("LinuxCNC", pipeline->getDevice());
      }
      m_config->stop();
    };

    boost::asio::steady_timer timer2(asyncContext.get());

    auto send = [this, &adapter, &timer2, validate](boost::system::error_code ec) {
      if (ec)
      {
        m_config->stop();
      }
      else
      {
        adapter->processData("* deviceModel: --multiline--AAAAA");
        adapter->processData(R"(
<Device uuid="000" name="LinuxCNC" sampleInterval="10.0" id="d">
  <Description manufacturer="NIST" serialNumber=""/>
  <DataItems>
    <DataItem type="AVAILABILITY" category="EVENT" id="a" name="avail"/>
  </DataItems>
  <Components>
    <Controller id="cont">
      <DataItems>
        <DataItem type="EXECUTION" category="EVENT" id="exec"/>
        <DataItem type="CONTROLLER_MODE" category="EVENT" id="mode" name="mode"/>
      </DataItems>
    </Controller>
  </Components>
</Device>
)");
        adapter->processData("--multiline--AAAAA");

        timer2.expires_from_now(500ms);
        timer2.async_wait(validate);
      }
    };

    boost::asio::steady_timer timer1(asyncContext.get());
    timer1.expires_from_now(100ms);
    timer1.async_wait(send);

    m_config->start();
  }

  TEST_F(ConfigTest, should_update_a_device_when_received_from_adapter)
  {
    using namespace mtconnect::source::adapter;

    fs::path root {createTempDirectory("7")};

    fs::path devices(root / "Devices.xml");
    fs::path config {root / "agent.cfg"};
    {
      ofstream cfg(config.string());
      cfg << R"DOC(
VersionDeviceXml = true
CreateUniqueIds = true
EnableSourceDeviceModels = true

Port = 0
)DOC";
      cfg << "Devices = " << devices << endl;
    }

    copyFile("dyn_load.xml", devices, 0min);

    boost::program_options::variables_map options;
    boost::program_options::variable_value value(boost::optional<string>(config.string()), false);
    options.insert(make_pair("config-file"s, value));

    m_config->initialize(options);
    auto &asyncContext = m_config->getAsyncContext();

    auto agent = m_config->getAgent();
    auto device = agent->getDeviceByName("LinuxCNC");
    ASSERT_TRUE(device);

    const auto &printer = agent->getPrinter("xml");
    ASSERT_NE(nullptr, printer);

    auto sp = agent->findSource("_localhost_7878");
    ASSERT_TRUE(sp);

    auto adapter = dynamic_pointer_cast<shdr::ShdrAdapter>(sp);
    ASSERT_TRUE(adapter);

    auto validate = [&](boost::system::error_code ec) {
      using namespace std::filesystem;
      using namespace std::chrono;
      using namespace boost::algorithm;

      if (!ec)
      {
        // Check for backup file
        auto ext = date::format(".%Y-%m-%dT%H+", date::floor<seconds>(system_clock::now()));
        auto dit = directory_iterator(".");
        std::list<directory_entry> entries;
        copy_if(dit, end(dit), back_inserter(entries),
                [&ext](const auto &de) { return contains(de.path().string(), ext); });
        ASSERT_EQ(2, entries.size());

        auto device = agent->getDeviceByName("LinuxCNC");
        ASSERT_TRUE(device) << "Cannot find LinuxCNC device";

        const auto &components = device->getChildren();
        ASSERT_EQ(1, components->size());

        auto conts = device->getComponentByType("Controller");
        ASSERT_EQ(1, conts.size()) << "Cannot find Component with id cont";
        auto cont = conts.front();

        auto devDIs = device->getDataItems();
        ASSERT_TRUE(devDIs);
        ASSERT_EQ(5, devDIs->size());

        auto dataItems = cont->getDataItems();
        ASSERT_TRUE(dataItems);
        ASSERT_EQ(2, dataItems->size());

        auto it = dataItems->begin();
        ASSERT_EQ("exc", (*it)->get<std::string>("originalId"));
        it++;
        ASSERT_EQ("mode", (*it)->get<std::string>("originalId"));

        auto estop = device->getDeviceDataItem("estop");
        ASSERT_TRUE(estop) << "Cannot find DataItem with id estop";

        auto exec = device->getDeviceDataItem("exc");
        ASSERT_TRUE(exec) << "Cannot find DataItem with id exc";

        auto pipeline = dynamic_cast<AdapterPipeline *>(adapter->getPipeline());
        ASSERT_EQ("LinuxCNC", pipeline->getDevice());
      }
      m_config->stop();
    };

    boost::asio::steady_timer timer2(asyncContext.get());

    auto send = [this, &adapter, &timer2, validate](boost::system::error_code ec) {
      if (ec)
      {
        m_config->stop();
      }
      else
      {
        adapter->processData("* deviceModel: --multiline--AAAAA");
        adapter->processData(R"(
<Device uuid="000" name="LinuxCNC" sampleInterval="10.0" id="d">
  <Description manufacturer="NIST" serialNumber=""/>
  <DataItems>
    <DataItem type="AVAILABILITY" category="EVENT" id="a" name="avail"/>
    <DataItem type="EMERGENCY_STOP" category="EVENT" id="estop" name="es"/>
  </DataItems>
  <Components>
    <Controller id="cont">
      <DataItems>
        <DataItem type="EXECUTION" category="EVENT" id="exc"/>
        <DataItem type="CONTROLLER_MODE" category="EVENT" id="mode" name="mode"/>
      </DataItems>
    </Controller>
  </Components>
</Device>
)");
        adapter->processData("--multiline--AAAAA");

        timer2.expires_from_now(500ms);
        timer2.async_wait(validate);
      }
    };

    boost::asio::steady_timer timer1(asyncContext.get());
    timer1.expires_from_now(100ms);
    timer1.async_wait(send);

    m_config->start();
  }

  TEST_F(ConfigTest, should_update_the_ids_of_all_entities)
  {
    fs::path root {createTempDirectory("8")};

    fs::path devices(root / "Devices.xml");
    fs::path config {root / "agent.cfg"};
    {
      ofstream cfg(config.string());
      cfg << R"DOC(
VersionDeviceXml = true
CreateUniqueIds = true
Port = 0
)DOC";
      cfg << "Devices = " << devices << endl;
    }

    copyFile("dyn_load.xml", devices, 0min);

    boost::program_options::variables_map options;
    boost::program_options::variable_value value(boost::optional<string>(config.string()), false);
    options.insert(make_pair("config-file"s, value));

    m_config->initialize(options);

    auto agent = m_config->getAgent();
    auto device = agent->getDeviceByName("LinuxCNC");
    ASSERT_TRUE(device);

    auto deviceId = device->getId();

    ASSERT_NE("d", deviceId);
    ASSERT_EQ("d", device->get<string>("originalId"));

    // Get the data item by its old id
    auto exec = device->getDeviceDataItem("exec");
    ASSERT_TRUE(exec);
    ASSERT_TRUE(exec->getOriginalId());
    ASSERT_EQ("exec", *exec->getOriginalId());

    // Re-initialize the agent with the modified device.xml with the unique ids aready created
    // This tests if the originalId in the device xml file does the ritght thing when mapping ids
    m_config = std::make_unique<AgentConfiguration>();
    m_config->setDebug(true);
    m_config->initialize(options);

    auto agent2 = m_config->getAgent();

    auto device2 = agent2->getDeviceByName("LinuxCNC");
    ASSERT_TRUE(device2);
    ASSERT_EQ(deviceId, device2->getId());

    auto exec2 = device->getDeviceDataItem("exec");
    ASSERT_TRUE(exec2);
    ASSERT_EQ(exec->getId(), exec2->getId());
    ASSERT_TRUE(exec2->getOriginalId());
    ASSERT_EQ("exec", *exec2->getOriginalId());
  }

  TEST_F(ConfigTest, should_add_a_new_device_with_duplicate_ids)
  {
    using namespace mtconnect::source::adapter;

    fs::path root {createTempDirectory("9")};

    fs::path devices(root / "Devices.xml");
    fs::path config {root / "agent.cfg"};
    {
      ofstream cfg(config.string());
      cfg << R"DOC(
VersionDeviceXml = true
CreateUniqueIds = true
EnableSourceDeviceModels = true

Port = 0
)DOC";
      cfg << "Devices = " << devices << endl;
    }

    copyFile("dyn_load.xml", devices, 0min);

    boost::program_options::variables_map options;
    boost::program_options::variable_value value(boost::optional<string>(config.string()), false);
    options.insert(make_pair("config-file"s, value));

    m_config->initialize(options);
    auto &asyncContext = m_config->getAsyncContext();

    auto agent = m_config->getAgent();
    auto device = agent->getDeviceByName("LinuxCNC");
    ASSERT_TRUE(device);

    const auto &printer = agent->getPrinter("xml");
    ASSERT_NE(nullptr, printer);

    auto sp = agent->findSource("_localhost_7878");
    ASSERT_TRUE(sp);

    auto adapter = dynamic_pointer_cast<shdr::ShdrAdapter>(sp);
    ASSERT_TRUE(adapter);

    auto validate = [&](boost::system::error_code ec) {
      using namespace std::filesystem;
      using namespace std::chrono;
      using namespace boost::algorithm;

      if (!ec)
      {
        // Check for backup file
        auto ext = date::format(".%Y-%m-%dT%H+", date::floor<seconds>(system_clock::now()));
        auto dit = directory_iterator(".");
        std::list<directory_entry> entries;
        copy_if(dit, end(dit), back_inserter(entries),
                [&ext](const auto &de) { return contains(de.path().string(), ext); });
        ASSERT_EQ(2, entries.size());

        ASSERT_EQ(3, agent->getDevices().size());

        auto device1 = agent->getDeviceByName("LinuxCNC");
        ASSERT_TRUE(device1) << "Cannot find LinuxCNC device";

        auto device2 = agent->getDeviceByName("AnotherCNC");
        ASSERT_TRUE(device2) << "Cannot find LinuxCNC device";

        auto pipeline = dynamic_cast<AdapterPipeline *>(adapter->getPipeline());
        ASSERT_EQ("AnotherCNC", pipeline->getDevice());
      }
      m_config->stop();
    };

    boost::asio::steady_timer timer2(asyncContext.get());

    auto send = [this, &adapter, &timer2, validate](boost::system::error_code ec) {
      if (ec)
      {
        m_config->stop();
      }
      else
      {
        auto pipeline = dynamic_cast<AdapterPipeline *>(adapter->getPipeline());
        ASSERT_EQ("LinuxCNC", pipeline->getDevice());

        adapter->processData("* deviceModel: --multiline--AAAAA");
        adapter->processData(R"(
<Device uuid="001" name="AnotherCNC" sampleInterval="10.0" id="d">
  <Description manufacturer="NIST" serialNumber=""/>
  <DataItems>
    <DataItem type="AVAILABILITY" category="EVENT" id="a" name="avail"/>
    <DataItem type="EMERGENCY_STOP" category="EVENT" id="estop" name="es"/>
  </DataItems>
  <Components>
    <Controller id="cont">
      <DataItems>
        <DataItem type="EXECUTION" category="EVENT" id="exec"/>
        <DataItem type="CONTROLLER_MODE" category="EVENT" id="mode" name="mode"/>
      </DataItems>
    </Controller>
  </Components>
</Device>
)");
        adapter->processData("--multiline--AAAAA");

        timer2.expires_from_now(500ms);
        timer2.async_wait(validate);
      }
    };

    boost::asio::steady_timer timer1(asyncContext.get());
    timer1.expires_from_now(100ms);
    timer1.async_wait(send);

    m_config->start();
  }

  TEST_F(ConfigTest, should_ignore_xmlns_when_parsing_device_xml)
  {
    using namespace mtconnect::source::adapter;

    fs::path root {createTempDirectory("10")};

    fs::path devices(root / "Devices.xml");
    fs::path config {root / "agent.cfg"};
    {
      ofstream cfg(config.string());
      cfg << R"DOC(
VersionDeviceXml = true
EnableSourceDeviceModels = true
Port = 0

Adapters {
  Device {
  }
}
)DOC";
      cfg << "Devices = " << devices << endl;
    }

    copyFile("empty.xml", devices, 0min);

    boost::program_options::variables_map options;
    boost::program_options::variable_value value(boost::optional<string>(config.string()), false);
    options.insert(make_pair("config-file"s, value));

    m_config->initialize(options);
    auto &asyncContext = m_config->getAsyncContext();

    auto agent = m_config->getAgent();
    const auto &printer = agent->getPrinter("xml");
    ASSERT_NE(nullptr, printer);

    auto sp = agent->findSource("_localhost_7878");
    ASSERT_TRUE(sp);

    auto adapter = dynamic_pointer_cast<shdr::ShdrAdapter>(sp);
    ASSERT_TRUE(adapter);

    auto validate = [&](boost::system::error_code ec) {
      using namespace std::filesystem;
      using namespace std::chrono;
      using namespace boost::algorithm;

      if (!ec)
      {
        // Check for backup file
        auto ext = date::format(".%Y-%m-%dT%H+", date::floor<seconds>(system_clock::now()));
        auto dit = directory_iterator(".");
        std::list<directory_entry> entries;
        copy_if(dit, end(dit), back_inserter(entries),
                [&ext](const auto &de) { return contains(de.path().string(), ext); });
        ASSERT_EQ(1, entries.size());

        auto device = agent->getDeviceByName("LinuxCNC");
        ASSERT_TRUE(device) << "Cannot find LinuxCNC device";

        ASSERT_FALSE(device->maybeGet<string>("xmlns"));
        ASSERT_FALSE(device->maybeGet<string>("xmlns:m"));
      }
      m_config->stop();
    };

    boost::asio::steady_timer timer2(asyncContext.get());

    auto send = [this, &adapter, &timer2, validate](boost::system::error_code ec) {
      if (ec)
      {
        m_config->stop();
      }
      else
      {
        adapter->processData("* deviceModel: --multiline--AAAAA");
        adapter->processData(R"(
<Device uuid="000" name="LinuxCNC" sampleInterval="10.0" id="d" xmlns:m="urn:mtconnect.org:MTConnectDevices:2.2" xmlns="urn:mtconnect.org:MTConnectDevices:2.2">
  <Description manufacturer="NIST" serialNumber=""/>
  <DataItems>
    <DataItem type="AVAILABILITY" category="EVENT" id="a" name="avail"/>
  </DataItems>
  <Components>
    <Controller id="cont">
      <DataItems>
        <DataItem type="EXECUTION" category="EVENT" id="exec"/>
        <DataItem type="CONTROLLER_MODE" category="EVENT" id="mode" name="mode"/>
      </DataItems>
    </Controller>
  </Components>
</Device>
)");
        adapter->processData("--multiline--AAAAA");

        timer2.expires_from_now(500ms);
        timer2.async_wait(validate);
      }
    };

    boost::asio::steady_timer timer1(asyncContext.get());
    timer1.expires_from_now(100ms);
    timer1.async_wait(send);

    m_config->start();
  }

  TEST_F(ConfigTest, should_not_reload_when_monitor_files_is_on)
  {
    using namespace mtconnect::source::adapter;

    fs::path root {createTempDirectory("11")};

    fs::path devices(root / "Devices.xml");
    fs::path config {root / "agent.cfg"};
    {
      ofstream cfg(config.string());
      cfg << R"DOC(
VersionDeviceXml = true
CreateUniqueIds = true
MonitorConfigFiles = true
MonitorInterval = 1
MinimumConfigReloadAge = 1
EnableSourceDeviceModels = true
Port = 0
)DOC";
      cfg << "Devices = " << devices << endl;
    }

    copyFile("dyn_load.xml", devices, 0min);

    boost::program_options::variables_map options;
    boost::program_options::variable_value value(boost::optional<string>(config.string()), false);
    options.insert(make_pair("config-file"s, value));

    m_config->initialize(options);
    auto &asyncContext = m_config->getAsyncContext();

    auto agent = m_config->getAgent();
    auto device = agent->getDeviceByName("LinuxCNC");
    ASSERT_TRUE(device);

    const auto &printer = agent->getPrinter("xml");
    ASSERT_NE(nullptr, printer);

    auto sp = agent->findSource("_localhost_7878");
    ASSERT_TRUE(sp);

    auto adapter = dynamic_pointer_cast<shdr::ShdrAdapter>(sp);
    ASSERT_TRUE(adapter);

    boost::asio::steady_timer shutdownTimer(asyncContext.get());

    auto shudown = [this](boost::system::error_code ec) {
      LOG(info) << "Shutting down the configuration";
      m_config->stop();
    };

    auto validate = [&](boost::system::error_code ec) {
      using namespace std::filesystem;
      using namespace std::chrono;
      using namespace boost::algorithm;

      if (!ec)
      {
        // Check for backup file
        auto ext = date::format(".%Y-%m-%dT%H+", date::floor<seconds>(system_clock::now()));
        auto dit = directory_iterator(".");
        std::list<directory_entry> entries;
        copy_if(dit, end(dit), back_inserter(entries),
                [&ext](const auto &de) { return contains(de.path().string(), ext); });
        ASSERT_EQ(2, entries.size());

        ASSERT_EQ(3, agent->getDevices().size());

        auto device1 = agent->getDeviceByName("LinuxCNC");
        ASSERT_TRUE(device1) << "Cannot find LinuxCNC device";

        auto device2 = agent->getDeviceByName("AnotherCNC");
        ASSERT_TRUE(device2) << "Cannot find LinuxCNC device";

        auto pipeline = dynamic_cast<AdapterPipeline *>(adapter->getPipeline());
        ASSERT_EQ("AnotherCNC", pipeline->getDevice());
      }

      shutdownTimer.expires_from_now(3s);
      shutdownTimer.async_wait(shudown);
    };

    boost::asio::steady_timer timer2(asyncContext.get());

    auto send = [this, &adapter, &timer2, validate](boost::system::error_code ec) {
      if (ec)
      {
        m_config->stop();
      }
      else
      {
        auto pipeline = dynamic_cast<AdapterPipeline *>(adapter->getPipeline());
        ASSERT_EQ("LinuxCNC", pipeline->getDevice());

        adapter->processData("* deviceModel: --multiline--AAAAA");
        adapter->processData(R"(
<Device uuid="001" name="AnotherCNC" sampleInterval="10.0" id="d">
  <Description manufacturer="NIST" serialNumber=""/>
  <DataItems>
    <DataItem type="AVAILABILITY" category="EVENT" id="a" name="avail"/>
    <DataItem type="EMERGENCY_STOP" category="EVENT" id="estop" name="es"/>
  </DataItems>
  <Components>
    <Controller id="cont">
      <DataItems>
        <DataItem type="EXECUTION" category="EVENT" id="exec"/>
        <DataItem type="CONTROLLER_MODE" category="EVENT" id="mode" name="mode"/>
      </DataItems>
    </Controller>
  </Components>
</Device>
)");
        adapter->processData("--multiline--AAAAA");

        timer2.expires_from_now(500ms);
        timer2.async_wait(validate);
      }
    };

    boost::asio::steady_timer timer1(asyncContext.get());
    timer1.expires_from_now(100ms);
    timer1.async_wait(send);

    m_config->start();
  }

  TEST_F(ConfigTest, should_not_crash_when_there_are_no_devices_and_receives_data)
  {
    using namespace mtconnect::source::adapter;

    fs::path root {createTempDirectory("12")};

    fs::path devices(root / "Devices.xml");
    fs::path config {root / "agent.cfg"};
    {
      ofstream cfg(config.string());
      cfg << R"DOC(
VersionDeviceXml = true
EnableSourceDeviceModels = true
Port = 0

Adapters {
  Device {
  }
}
)DOC";
      cfg << "Devices = " << devices << endl;
    }

    copyFile("empty.xml", devices, 0min);

    boost::program_options::variables_map options;
    boost::program_options::variable_value value(boost::optional<string>(config.string()), false);
    options.insert(make_pair("config-file"s, value));

    m_config->initialize(options);
    auto &asyncContext = m_config->getAsyncContext();

    auto agent = m_config->getAgent();
    const auto &printer = agent->getPrinter("xml");
    ASSERT_NE(nullptr, printer);

    auto sp = agent->findSource("_localhost_7878");
    ASSERT_TRUE(sp);

    auto adapter = dynamic_pointer_cast<shdr::ShdrAdapter>(sp);
    ASSERT_TRUE(adapter);

    auto validate = [&](boost::system::error_code ec) {
      using namespace std::filesystem;
      using namespace std::chrono;
      using namespace boost::algorithm;

      if (!ec)
      {
      }
      m_config->stop();
    };

    boost::asio::steady_timer timer2(asyncContext.get());

    auto send = [this, &adapter, &timer2, validate](boost::system::error_code ec) {
      if (ec)
      {
        m_config->stop();
      }
      else
      {
        adapter->processData("* device: none");
        adapter->processData("* uuid: 12345");

        timer2.expires_from_now(500ms);
        timer2.async_wait(validate);
      }
    };

    boost::asio::steady_timer timer1(asyncContext.get());
    timer1.expires_from_now(100ms);
    timer1.async_wait(send);

    m_config->start();
  }

  // Environment variable tests
  TEST_F(ConfigTest, should_expand_environment_variables)
  {
    putenv(strdup("CONFIG_TEST=TestValue"));

    string config(R"DOC(
ServiceName=$CONFIG_TEST
)DOC");

    m_config->setDebug(true);
    m_config->loadConfig(config);

    const auto &options = m_config->getAgent()->getOptions();
    ASSERT_EQ("TestValue", *GetOption<string>(options, configuration::ServiceName));
  }

  TEST_F(ConfigTest, should_expand_options)
  {
    putenv(strdup("CONFIG_TEST=ShouldNotMatch"));

    string config(R"DOC(
TestVariable=TestValue
ServiceName=$TestVariable
)DOC");

    m_config->setDebug(true);
    m_config->loadConfig(config);

    const auto &options = m_config->getAgent()->getOptions();
    ASSERT_EQ("TestValue", *GetOption<string>(options, configuration::ServiceName));
  }

  // Environment variable tests
  TEST_F(ConfigTest, should_expand_with_prefix_and_suffix)
  {
    putenv(strdup("CONFIG_TEST=TestValue"));

    string config(R"DOC(
ServiceName=/some/prefix/$CONFIG_TEST:suffix
)DOC");

    m_config->setDebug(true);
    m_config->loadConfig(config);

    const auto &options = m_config->getAgent()->getOptions();
    ASSERT_EQ("/some/prefix/TestValue:suffix",
              *GetOption<string>(options, configuration::ServiceName));
  }

  TEST_F(ConfigTest, should_expand_with_prefix_and_suffix_with_curly)
  {
    putenv(strdup("CONFIG_TEST=TestValue"));

    string config(R"DOC(
ServiceName="some_prefix_${CONFIG_TEST}_suffix"
)DOC");

    m_config->setDebug(true);
    m_config->loadConfig(config);

    const auto &options = m_config->getAgent()->getOptions();
    ASSERT_EQ("some_prefix_TestValue_suffix",
              *GetOption<string>(options, configuration::ServiceName));
  }

  TEST_F(ConfigTest, should_find_device_file_in_config_path)
  {
    fs::path root {createTempDirectory("13")};
    copyFile("empty.xml", root / "test.xml", 0min);
    chdir(m_cwd.string().c_str());
    m_config->updateWorkingDirectory();

    string config("ConfigPath=\"/junk/folder," + root.string() +
                  "\"\n"
                  "Devices=test.xml\n");

    m_config->setDebug(true);
    m_config->loadConfig(config);

    ASSERT_TRUE(m_config->getAgent());
  }

}  // namespace
