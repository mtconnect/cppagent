// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "adapter.hpp"
#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "entity.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;

class EntityTest : public testing::Test
{
 protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agent = make_unique<Agent>(PROJECT_ROOT_DIR "/samples/solid_model.xml", 4, 4, "1.7");
    m_agentId = int64ToString(getCurrentTimeInSec());

    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->m_agent = m_agent.get();

    m_device = m_agent->getDeviceByName("LinuxCNC");
    
    
  }

  void TearDown() override
  {
    EntityFactory::clear();
    m_agent.reset();
    m_agentTestHelper.reset();
  }

  std::unique_ptr<Agent> m_agent;
  std::string m_agentId;
  Device *m_device{nullptr};

  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};


TEST_F(EntityTest, TestSimpleFactory)
{
  EntityFactory::registerFactory(string("simple"),
                                 Factory({ Requirement("name", true ),
                                           Requirement("id", true),
                                           Requirement("size", false, Requirement::INTEGER) } ));

  Entity::Properties simple { { "id", "abc" }, { "name", "xxx" }, {"size", 10 }};
  
  auto entity = EntityFactory::create("simple", simple);
  ASSERT_TRUE(entity);
  ASSERT_EQ(3, entity->getProperties().size());
  ASSERT_EQ("simple", entity->getName());
  ASSERT_EQ("abc", get<std::string>(entity->getProperty("id")));
  ASSERT_EQ("xxx", get<std::string>(entity->getProperty("name")));
  ASSERT_EQ(10, get<int>(entity->getProperty("size")));
}
