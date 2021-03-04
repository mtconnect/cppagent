// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "adapter/adapter.hpp"
#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "json_helper.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;
using namespace mtconnect::adapter;
using namespace entity;

class ReferencesTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/reference_example.xml", 8, 4, "1.6", 25);
    m_agentId = to_string(getCurrentTimeInSec());
    m_device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");
    m_component = m_device->getComponentById("bf");
  }

  void TearDown() override { m_agentTestHelper.reset(); }

  Adapter *m_adapter {nullptr};
  std::string m_agentId;
  Device *m_device {nullptr};
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
  Component *m_component {nullptr};
};

TEST_F(ReferencesTest, References)
{
  ASSERT_NE(nullptr, m_component);

  auto &references_list = m_component->getReferences();  
  ASSERT_TRUE(references_list);

  auto references_entity = references_list->getEntity();
  auto &references = references_entity->maybeGet<EntityList>("LIST");
  
  ASSERT_TRUE(references);
  ASSERT_EQ(3, references->size());
  auto reference = references->begin();

  EXPECT_EQ("DataItemRef", (*reference)->getName());

  EXPECT_EQ("chuck", get<string>((*reference)->getProperty("name")));
  EXPECT_EQ("c4", get<string>((*reference)->getProperty("idRef")));
}
