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
  auto model = clc->get<EntityPtr>("ImageFile");

  EXPECT_EQ("ImageFile", model->getName());

  ASSERT_EQ("if", model->get<string>("id"));
  ASSERT_EQ("PNG", model->get<string>("mediaType"));
  ASSERT_EQ("/pictures/machine.png", model->get<string>("href"));
}

#define DEVICE_CONFIGURATION_PATH "//m:Device/m:Configuration"
#define DEVICE_SOLID_MODEL_PATH DEVICE_CONFIGURATION_PATH "/m:ImageFile"

TEST_F(ImageFileTest, should_print_configuration_with_image_file)
{
  {
    PARSE_XML_RESPONSE("/LinuxCNC/probe");

    ASSERT_XML_PATH_COUNT(doc, DEVICE_SOLID_MODEL_PATH, 1);
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_SOLID_MODEL_PATH "@id", "if");
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_SOLID_MODEL_PATH "@mediaType", "PNG");
    ASSERT_XML_PATH_EQUAL(doc, DEVICE_SOLID_MODEL_PATH "@href", "/pictures/machine.png");
  }
}

TEST_F(ImageFileTest, should_print_configuration_with_image_file_in_json)
{
  {
    PARSE_JSON_RESPONSE("/LinuxCNC/probe");

    auto devices = doc.at("/MTConnectDevices/Devices"_json_pointer);
    auto device = devices.at(0).at("/Device"_json_pointer);

    auto model = device.at("/Configuration/ImageFile"_json_pointer);
    ASSERT_TRUE(model.is_object());

    ASSERT_EQ(3, model.size());
    EXPECT_EQ("if", model["id"]);
    EXPECT_EQ("PNG", model["mediaType"]);
    EXPECT_EQ("/pictures/machine.png", model["href"]);
  }
}
