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
using namespace device_model;
using namespace mtconnect::source::adapter;
using namespace entity;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class ReferencesTest : public testing::Test
{
protected:
  void SetUp() override {}

  void createAgent(ConfigOptions options)
  {
    // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/reference_example.xml", 8, 4, "1.6", 25, false, true,
                                   options);
    m_agentId = to_string(getCurrentTimeInSec());
    m_device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");
  }

  void TearDown() override
  {
    m_device.reset();
    m_component.reset();
    m_agentTestHelper.reset();
  }

  Adapter *m_adapter {nullptr};
  std::string m_agentId;
  DevicePtr m_device;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
  ComponentPtr m_component;
};

TEST_F(ReferencesTest, References)
{
  createAgent({});
  m_component = m_device->getComponentById("bf");

  ASSERT_NE(nullptr, m_component);

  const auto &references = m_component->getList("References");
  ASSERT_TRUE(references);
  ASSERT_EQ(3, references->size());
  auto reference = references->begin();

  EXPECT_EQ("DataItemRef", (*reference)->getName());

  EXPECT_EQ("chuck", get<string>((*reference)->getProperty("name")));
  EXPECT_EQ("c4", get<string>((*reference)->getProperty("idRef")));
}

TEST_F(ReferencesTest, should_map_references_to_new_ids)
{
  using namespace mtconnect::configuration;
  createAgent({{CreateUniqueIds, true}});
  m_component = m_device->getComponentByName("barfeeder");

  ASSERT_NE(nullptr, m_component);

  const auto &references = m_component->getList("References");
  ASSERT_TRUE(references);
  ASSERT_EQ(3, references->size());
  auto reference = references->begin();

  EXPECT_EQ("DataItemRef", (*reference)->getName());

  EXPECT_EQ("chuck", get<string>((*reference)->getProperty("name")));
  auto di = m_device->getDeviceDataItem("c4");
  ASSERT_TRUE(di);
  ASSERT_NE("c4", di->getId());

  EXPECT_EQ(di->getId(), get<string>((*reference)->getProperty("idRef")));

  reference++;

  EXPECT_EQ("DataItemRef", (*reference)->getName());

  EXPECT_EQ("door", get<string>((*reference)->getProperty("name")));
  di = m_device->getDeviceDataItem("d2");
  ASSERT_TRUE(di);
  ASSERT_NE("d2", di->getId());

  EXPECT_EQ(di->getId(), get<string>((*reference)->getProperty("idRef")));

  reference++;
  auto cref = *reference;

  EXPECT_EQ("ComponentRef", cref->getName());

  EXPECT_EQ("electric", get<string>(cref->getProperty("name")));
  auto id = get<string>(cref->getProperty("idRef"));
  auto comp = m_device->getComponentById(id);
  ASSERT_TRUE(comp);
  ASSERT_EQ("Electric", comp->getName());

  di = m_device->getDeviceDataItem("fsys");
  ASSERT_TRUE(di);

  auto source = di->getSource();
  ASSERT_TRUE(source);

  auto diId = source->get<string>("dataItemId");
  ASSERT_NE("mf", diId);
  di = m_device->getDeviceDataItem("feed");
  ASSERT_EQ(di->getId(), diId);
  di = m_device->getDeviceDataItem(diId);
  ASSERT_TRUE(di);

  auto compId = source->get<string>("componentId");
  ASSERT_NE("ele", compId);
  comp = m_device->getComponentById(compId);
  ASSERT_TRUE(comp);

  ASSERT_EQ("xxx", source->get<string>("compositionId"));
}
