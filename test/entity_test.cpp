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
  EntityFactoryPtr root = make_shared<EntityFactory>();
  EntityFactoryPtr simpleFact = make_shared<EntityFactory>(Requirements({
    Requirement("name", true ),
    Requirement("id", true),
    Requirement("size", false, Requirement::INTEGER) }));
  root->registerFactory("simple", simpleFact);

  Entity::Properties simple { { "id", "abc" }, { "name", "xxx" }, {"size", 10 }};
  
  auto entity = root->create("simple", simple);
  ASSERT_TRUE(entity);
  ASSERT_EQ(3, entity->getProperties().size());
  ASSERT_EQ("simple", entity->getName());
  ASSERT_EQ("abc", get<std::string>(entity->getProperty("id")));
  ASSERT_EQ("xxx", get<std::string>(entity->getProperty("name")));
  ASSERT_EQ(10, get<int>(entity->getProperty("size")));
}

TEST_F(EntityTest, TestSimpleTwoLevelFactory)
{
  auto root = make_shared<EntityFactory>();
  
  auto second = make_shared<EntityFactory>(Requirements({
    Requirement("key", true ),
    Requirement("value", true) }));
  
  auto simple = make_shared<EntityFactory>(Requirements{
    Requirement("name", true ),
    Requirement("id", true),
    Requirement("size", false, Requirement::INTEGER),
    Requirement("second", Requirement::ENTITY, second)
  });
  root->registerFactory("simple",  simple);
  
  auto fact = root->factoryFor("simple");
  ASSERT_TRUE(fact);
  
  auto sfact = fact->factoryFor("second");
  ASSERT_TRUE(sfact);
  
  Entity::Properties sndp { {"key", "1" }, {"value", "arf"}};
  auto se = fact->create("second", sndp);
  ASSERT_TRUE(se);
  ASSERT_EQ(2, se->getProperties().size());
  ASSERT_EQ("1", get<std::string>(se->getProperty("key")));
  ASSERT_EQ("arf", get<std::string>(se->getProperty("value")));

  Entity::Properties simpp {
    { "id", "abc" }, { "name", "xxx" }, {"size", 10 },
    { "second", se }
  };
  
  auto entity = root->create("simple", simpp);
  ASSERT_TRUE(entity);
  ASSERT_EQ(4, entity->getProperties().size());
  ASSERT_EQ("simple", entity->getName());
  ASSERT_EQ("abc", get<std::string>(entity->getProperty("id")));
  ASSERT_EQ("xxx", get<std::string>(entity->getProperty("name")));
  ASSERT_EQ(10, get<int>(entity->getProperty("size")));
  
  auto v = get<EntityPtr>(entity->getProperty("second"));
  ASSERT_TRUE(v);
  ASSERT_EQ(2, v->getProperties().size());
  ASSERT_EQ("1", get<std::string>(v->getProperty("key")));
  ASSERT_EQ("arf", get<std::string>(v->getProperty("value")));

}
