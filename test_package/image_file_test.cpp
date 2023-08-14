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

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/source/adapter/adapter.hpp"

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;
using namespace mtconnect::source::adapter;
using namespace entity;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class ImageFileTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/solid_model.xml", 8, 4, "2.2", 25);
    m_agentId = to_string(getCurrentTimeInSec());
    m_device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");
  }

  void TearDown() override { m_agentTestHelper.reset(); }

  Adapter *m_adapter {nullptr};
  std::string m_agentId;
  DevicePtr m_device;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

TEST_F(ImageFileTest, should_parse_configuration_with_image_file)
{
  ASSERT_NE(nullptr, m_device);

  auto &clc = m_device->get<EntityPtr>("Configuration");
  ASSERT_TRUE(clc);

  const auto &ifs = clc->getList("ImageFiles");
  ASSERT_TRUE(ifs);
  ASSERT_EQ(2, ifs->size());

  auto it = ifs->begin();
  ASSERT_EQ("front", (*it)->get<string>("name"));
  ASSERT_EQ("fif", (*it)->get<string>("id"));
  ASSERT_EQ("PNG", (*it)->get<string>("mediaType"));
  ASSERT_EQ("/pictures/front.png", (*it)->get<string>("href"));

  it++;
  ASSERT_EQ("back", (*it)->get<string>("name"));
  ASSERT_EQ("bif", (*it)->get<string>("id"));
  ASSERT_EQ("PNG", (*it)->get<string>("mediaType"));
  ASSERT_EQ("/pictures/back.png", (*it)->get<string>("href"));
}

#define DEVICE_CONFIGURATION_PATH "//m:Device/m:Configuration/m:ImageFiles"
#define DEVICE_IMAGE_FILE_PATH_1 DEVICE_CONFIGURATION_PATH "/m:ImageFile[@id='fif']"
#define DEVICE_IMAGE_FILE_PATH_2 DEVICE_CONFIGURATION_PATH "/m:ImageFile[@id='bif']"

TEST_F(ImageFileTest, should_print_configuration_with_image_file)
{
  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");

    ASSERT_XML_PATH_COUNT(doc, DEVICE_CONFIGURATION_PATH "/*", 2);

    ASSERT_XML_PATH_EQUAL(doc, DEVICE_IMAGE_FILE_PATH_1 "@name", "front");
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_IMAGE_FILE_PATH_1 "@id", "fif");
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_IMAGE_FILE_PATH_1 "@mediaType", "PNG");
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_IMAGE_FILE_PATH_1 "@href", "/pictures/front.png");

    ASSERT_XML_PATH_EQUAL(doc, DEVICE_IMAGE_FILE_PATH_2 "@name", "back");
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_IMAGE_FILE_PATH_2 "@id", "bif");
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_IMAGE_FILE_PATH_2 "@mediaType", "PNG");
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_IMAGE_FILE_PATH_2 "@href", "/pictures/back.png");
  }
}

TEST_F(ImageFileTest, should_print_configuration_with_image_file_in_json)
{
  {
    PARSE_JSON_RESPONSE("/LinuxCNC/probe");

    auto devices = doc.at("/MTConnectDevices/Devices"_json_pointer);
    auto device = devices.at(0).at("/Device"_json_pointer);

    auto files = device.at("/Configuration/ImageFiles"_json_pointer);
    ASSERT_TRUE(files.is_array());

    ASSERT_EQ(2, files.size());
    auto it = files.begin();

    auto image = it->at("ImageFile");
    EXPECT_EQ("fif", image["id"]);
    EXPECT_EQ("front", image["name"]);
    EXPECT_EQ("PNG", image["mediaType"]);
    EXPECT_EQ("/pictures/front.png", image["href"]);

    it++;
    image = it->at("ImageFile");
    EXPECT_EQ("bif", image["id"]);
    EXPECT_EQ("back", image["name"]);
    EXPECT_EQ("PNG", image["mediaType"]);
    EXPECT_EQ("/pictures/back.png", image["href"]);
  }
}

TEST_F(ImageFileTest, should_print_configuration_with_image_file_in_json_v2)
{
  m_agentTestHelper->createAgent("/samples/solid_model.xml", 8, 4, "2.2", 25, false, false,
                                 {{configuration::JsonVersion, 2}});

  {
    PARSE_JSON_RESPONSE("/LinuxCNC/probe");

    auto devices = doc.at("/MTConnectDevices/Devices"_json_pointer);
    auto device = devices.at("/Device/0"_json_pointer);

    auto files = device.at("/Configuration/ImageFiles/ImageFile"_json_pointer);
    ASSERT_TRUE(files.is_array());

    ASSERT_EQ(2, files.size());
    auto it = files.begin();

    auto image = *it;
    EXPECT_EQ("fif", image["id"]);
    EXPECT_EQ("front", image["name"]);
    EXPECT_EQ("PNG", image["mediaType"]);
    EXPECT_EQ("/pictures/front.png", image["href"]);

    it++;
    image = *it;
    EXPECT_EQ("bif", image["id"]);
    EXPECT_EQ("back", image["name"]);
    EXPECT_EQ("PNG", image["mediaType"]);
    EXPECT_EQ("/pictures/back.png", image["href"]);
  }
}
