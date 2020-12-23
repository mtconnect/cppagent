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
using namespace mtconnect::entity;

static inline int64_t operator "" _i64( unsigned long long int i )
{
  return int64_t(i);
}

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
  FactoryPtr root = make_shared<Factory>();
  FactoryPtr simpleFact = make_shared<Factory>(Requirements({
    Requirement("name", true ),
    Requirement("id", true),
    Requirement("size", false, INTEGER) }));
  root->registerFactory("simple", simpleFact);

  Properties simple({ { "id", "abc" }, { "name", "xxx" }, {"size", 10_i64 }});
  
  auto entity = root->create("simple", simple);
  ASSERT_TRUE(entity);
  ASSERT_EQ(3, entity->getProperties().size());
  ASSERT_EQ("simple", entity->getName());
  ASSERT_EQ("abc", get<std::string>(entity->getProperty("id")));
  ASSERT_EQ("xxx", get<std::string>(entity->getProperty("name")));
  ASSERT_EQ(10, get<int64_t>(entity->getProperty("size")));
}

TEST_F(EntityTest, TestSimpleTwoLevelFactory)
{
  auto root = make_shared<Factory>();
  
  auto second = make_shared<Factory>(Requirements({
    Requirement("key", true ),
    Requirement("VALUE", true) }));
  
  auto simple = make_shared<Factory>(Requirements{
    Requirement("name", true ),
    Requirement("id", true),
    Requirement("size", false, INTEGER),
    Requirement("second", ENTITY, second)
  });
  root->registerFactory("simple",  simple);
  
  auto fact = root->factoryFor("simple");
  ASSERT_TRUE(fact);
  
  auto sfact = fact->factoryFor("second");
  ASSERT_TRUE(sfact);
  
  Properties sndp { {"key", "1" }, {"VALUE", "arf"}};
  auto se = fact->create("second", sndp);
  ASSERT_TRUE(se);
  ASSERT_EQ(2, se->getProperties().size());
  ASSERT_EQ("1", get<std::string>(se->getProperty("key")));
  ASSERT_EQ("arf", get<std::string>(se->getProperty("VALUE")));

  Properties simpp {
    { "id", "abc" }, { "name", "xxx" }, { "size", 10_i64 },
    { "second", se }
  };
  
  auto entity = root->create("simple", simpp);
  ASSERT_TRUE(entity);
  ASSERT_EQ(4, entity->getProperties().size());
  ASSERT_EQ("simple", entity->getName());
  ASSERT_EQ("abc", get<std::string>(entity->getProperty("id")));
  ASSERT_EQ("xxx", get<std::string>(entity->getProperty("name")));
  ASSERT_EQ(10, get<int64_t>(entity->getProperty("size")));
  
  auto v = get<EntityPtr>(entity->getProperty("second"));
  ASSERT_TRUE(v);
  ASSERT_EQ(2, v->getProperties().size());
  ASSERT_EQ("1", get<std::string>(v->getProperty("key")));
  ASSERT_EQ("arf", get<std::string>(v->getProperty("VALUE")));

}

TEST_F(EntityTest, TestSimpleEntityList)
{
  auto root = make_shared<Factory>();
  
  auto second = make_shared<Factory>(Requirements({
    Requirement("key", true ),
    Requirement("VALUE", true) }));
  
  auto seconds = make_shared<Factory>(Requirements({
    Requirement("second", ENTITY, second,
                1, Requirement::Infinite) }));
  seconds->registerMatchers();

  auto simple = make_shared<Factory>(Requirements{
    Requirement("name", true ),
    Requirement("id", true),
    Requirement("size", false, INTEGER),
    Requirement("seconds", ENTITY_LIST, seconds)
  });
  root->registerFactory("simple",  simple);
  
  auto fact = root->factoryFor("simple");
  ASSERT_TRUE(fact);
  
  auto secondsFact = fact->factoryFor("seconds");
  ASSERT_TRUE(secondsFact);
  ASSERT_TRUE(secondsFact->isList());

  auto secondFact = secondsFact->factoryFor("second");
  ASSERT_TRUE(secondFact);
  
  Properties sndp1 { {"key", "1" }, {"VALUE", "arf"}};
  auto se1 = secondsFact->create("second", sndp1);
  ASSERT_TRUE(se1);
  ASSERT_EQ(2, se1->getProperties().size());
  ASSERT_EQ("1", get<std::string>(se1->getProperty("key")));
  ASSERT_EQ("arf", get<std::string>(se1->getProperty("VALUE")));
  
  Properties sndp2 { {"key", "2" }, {"VALUE", "meow"}};
  auto se2 = secondsFact->create("second", sndp2);
  ASSERT_TRUE(se2);
  ASSERT_EQ(2, se2->getProperties().size());
  ASSERT_EQ("2", get<std::string>(se2->getProperty("key")));
  ASSERT_EQ("meow", get<std::string>(se2->getProperty("VALUE")));

  ErrorList errors;
  EntityList list { se1, se2 };
  auto se3 = fact->create("seconds", list, errors);
  ASSERT_EQ(0, errors.size());
  ASSERT_TRUE(se3);
  
  Properties simpp {
    { "id", "abc" }, { "name", "xxx" }, {"size", 10_i64 },
    { "seconds", se3 }
  };
  
  auto entity = root->create("simple", simpp);
  ASSERT_TRUE(entity);
  ASSERT_EQ(4, entity->getProperties().size());
  ASSERT_EQ("simple", entity->getName());
  ASSERT_EQ("abc", get<std::string>(entity->getProperty("id")));
  ASSERT_EQ("xxx", get<std::string>(entity->getProperty("name")));
  ASSERT_EQ(10, get<int64_t>(entity->getProperty("size")));
  
  auto l = entity->getList("seconds");
  
  ASSERT_TRUE(l);
  ASSERT_EQ(2, l->size());

  auto it = l->begin();
  ASSERT_NE(l->end(), it);
  ASSERT_EQ(2, (*it)->getProperties().size());
  ASSERT_EQ("1", get<std::string>((*it)->getProperty("key")));
  ASSERT_EQ("arf", get<std::string>((*it)->getProperty("VALUE")));
  
  it++;
  ASSERT_NE(l->end(), it);
  ASSERT_EQ(2, (*it)->getProperties().size());
  ASSERT_EQ("2", get<std::string>((*it)->getProperty("key")));
  ASSERT_EQ("meow", get<std::string>((*it)->getProperty("VALUE")));
}

TEST_F(EntityTest, MissingProperty)
{
  FactoryPtr root = make_shared<Factory>();
  FactoryPtr simpleFact = make_shared<Factory>(Requirements({
    Requirement("name", true ),
    Requirement("id", true),
    Requirement("size", false, INTEGER) }));
  root->registerFactory("simple", simpleFact);
  
  Properties simple { { "name", "xxx" }, {"size", 10_i64 }};
  
  ErrorList errors;
  auto entity = root->create("simple", simple, errors);
  ASSERT_FALSE(entity);
  
  ASSERT_EQ(1, errors.size());
  ASSERT_EQ(string("Property id is required and not provided"),
            errors.begin()->what());
}

TEST_F(EntityTest, MissingOptionalProperty)
{
  FactoryPtr root = make_shared<Factory>();
  FactoryPtr simpleFact = make_shared<Factory>(Requirements({
    Requirement("name", true ),
    Requirement("id", true),
    Requirement("size", false, INTEGER) }));
  root->registerFactory("simple", simpleFact);
  
  Properties simple { { "id", "abc" }, { "name", "xxx" }};
  
  ErrorList errors;
  auto entity = root->create("simple", simple, errors);
  ASSERT_TRUE(entity);
  ASSERT_EQ(2, entity->getProperties().size());
  ASSERT_EQ("simple", entity->getName());
  ASSERT_EQ("abc", get<std::string>(entity->getProperty("id")));
  ASSERT_EQ("xxx", get<std::string>(entity->getProperty("name")));
  
  ASSERT_EQ(0, errors.size());
}

TEST_F(EntityTest, UnexpectedProperty)
{
  FactoryPtr root = make_shared<Factory>();
  FactoryPtr simpleFact = make_shared<Factory>(Requirements({
    Requirement("name", true ),
    Requirement("id", true),
    Requirement("size", false, INTEGER) }));
  root->registerFactory("simple", simpleFact);
  
  Properties simple { { "id", "abc" }, { "name", "xxx" }, { "junk", "junk" }};
  
  ErrorList errors;
  auto entity = root->create("simple", simple, errors);
  ASSERT_FALSE(entity);
  
  ASSERT_EQ(1, errors.size());
  ASSERT_EQ(string("The following keys were present and not expected: junk,"),
            errors.begin()->what());
}

TEST_F(EntityTest, EntityListAnyEntities)
{
  auto root = make_shared<Factory>();
  
  auto second = make_shared<Factory>(Requirements({
    Requirement("key", true ),
    Requirement("VALUE", true) }));
  
  auto seconds = make_shared<Factory>(Requirements({
    Requirement("something", ENTITY, second,
                1, Requirement::Infinite) }));
  seconds->registerFactory(regex(".+"), second);
  seconds->registerMatchers();

  auto simple = make_shared<Factory>(Requirements{
    Requirement("name", true ),
    Requirement("id", true),
    Requirement("size", false, INTEGER),
    Requirement("seconds", ENTITY_LIST, seconds)
  });
  root->registerFactory("simple",  simple);
  
  auto fact = root->factoryFor("simple");
  ASSERT_TRUE(fact);
  
  auto secondsFact = fact->factoryFor("seconds");
  ASSERT_TRUE(secondsFact);
  
  auto secondFact = secondsFact->factoryFor("dog");
  ASSERT_TRUE(secondFact);
  
  ErrorList errors;

  Properties sndp1 { {"key", "1" }, {"VALUE", "arf"}};
  auto se1 = secondsFact->create("dog", sndp1, errors);
  ASSERT_EQ(0, errors.size());
  ASSERT_TRUE(se1);
  ASSERT_EQ(2, se1->getProperties().size());
  ASSERT_EQ("1", get<std::string>(se1->getProperty("key")));
  ASSERT_EQ("arf", get<std::string>(se1->getProperty("VALUE")));
  
  Properties sndp2 { {"key", "2" }, {"VALUE", "meow"}};
  auto se2 = secondsFact->create("cat", sndp2, errors);
  ASSERT_EQ(0, errors.size());
  ASSERT_TRUE(se2);
  ASSERT_EQ(2, se2->getProperties().size());
  ASSERT_EQ("2", get<std::string>(se2->getProperty("key")));
  ASSERT_EQ("meow", get<std::string>(se2->getProperty("VALUE")));
  
  EntityList list { se1, se2 };
  auto se3 = fact->create("seconds", list, errors);
  ASSERT_EQ(0, errors.size());
  ASSERT_TRUE(se3);
  
  Properties simpp {
    { "id", "abc" }, { "name", "xxx" }, {"size", 10_i64 },
    { "seconds", se3 }
  };
  
  auto entity = root->create("simple", simpp, errors);
  ASSERT_EQ(0, errors.size());
  ASSERT_TRUE(entity);
  ASSERT_EQ(4, entity->getProperties().size());
  ASSERT_EQ("simple", entity->getName());
  ASSERT_EQ("abc", get<std::string>(entity->getProperty("id")));
  ASSERT_EQ("xxx", get<std::string>(entity->getProperty("name")));
  ASSERT_EQ(10, get<int64_t>(entity->getProperty("size")));
  
  auto l = entity->getList("seconds");
  ASSERT_EQ(2, l->size());
  
  auto it = l->begin();
  ASSERT_NE(l->end(), it);
  ASSERT_EQ(2, (*it)->getProperties().size());
  ASSERT_EQ("dog", (*it)->getName());
  ASSERT_EQ("1", get<std::string>((*it)->getProperty("key")));
  ASSERT_EQ("arf", get<std::string>((*it)->getProperty("VALUE")));
  
  it++;
  ASSERT_NE(l->end(), it);
  ASSERT_EQ(2, (*it)->getProperties().size());
  ASSERT_EQ("cat", (*it)->getName());
  ASSERT_EQ("2", get<std::string>((*it)->getProperty("key")));
  ASSERT_EQ("meow", get<std::string>((*it)->getProperty("VALUE")));
}

TEST_F(EntityTest, TestRequirementIntegerConversions)
{
  Value v("123");
  ASSERT_TRUE(holds_alternative<string>(v));
  Requirement r1("integer", INTEGER);
  ASSERT_TRUE(r1.convertType(v));
  ASSERT_TRUE(holds_alternative<int64_t>(v));
  ASSERT_EQ(123, get<int64_t>(v));
  
  ASSERT_FALSE(r1.convertType(v));
  
  Requirement r2("string", STRING);
  ASSERT_TRUE(r2.convertType(v));
  ASSERT_TRUE(holds_alternative<string>(v));
  ASSERT_EQ("123", get<string>(v));

  v = "aaa";
  ASSERT_THROW(r1.convertType(v), PropertyConversionError);
  ASSERT_TRUE(holds_alternative<string>(v));
  ASSERT_EQ("aaa", get<string>(v));

  Requirement r3("vector", VECTOR);
  v = 123_i64;
  ASSERT_TRUE(holds_alternative<int64_t>(v));
  ASSERT_TRUE(r3.convertType(v));
  ASSERT_TRUE(holds_alternative<Vector>(v));
  ASSERT_EQ(1, get<Vector>(v).size());
  ASSERT_EQ(123.0, get<Vector>(v)[0]);

  v = 123_i64;
  Requirement r4("entity", ENTITY);
  ASSERT_THROW(r4.convertType(v), PropertyConversionError);

  Requirement r5("entity_list", ENTITY_LIST);
  ASSERT_THROW(r5.convertType(v), PropertyConversionError);
  
  v = 1234.0;
  ASSERT_TRUE(holds_alternative<double>(v));
  ASSERT_TRUE(r1.convertType(v));
  ASSERT_TRUE(holds_alternative<int64_t>(v));
  ASSERT_EQ(1234_i64, get<int64_t>(v));
  
  v = nullptr;
  ASSERT_FALSE(r1.convertType(v));
}

TEST_F(EntityTest, TestRequirementStringConversion)
{
  Value v(1234567890_i64);
  Requirement r1("string", STRING);
  ASSERT_TRUE(holds_alternative<int64_t>(v));
  ASSERT_TRUE(r1.convertType(v));
  ASSERT_TRUE(holds_alternative<string>(v));
  ASSERT_EQ("1234567890", get<string>(v));
  
  v = 1234.56;
  ASSERT_TRUE(holds_alternative<double>(v));
  ASSERT_TRUE(r1.convertType(v));
  ASSERT_TRUE(holds_alternative<string>(v));
  ASSERT_EQ("1234.56", get<string>(v));

  v = Vector{1.123, 2.345, 6.789};
  ASSERT_TRUE(holds_alternative<Vector>(v));
  ASSERT_TRUE(r1.convertType(v));
  ASSERT_TRUE(holds_alternative<string>(v));
  ASSERT_EQ("1.123 2.345 6.789", get<string>(v));

  ASSERT_FALSE(r1.convertType(v));

}

TEST_F(EntityTest, TestRequirementDoubleConversions)
{
  Value v("123.24");
  ASSERT_TRUE(holds_alternative<string>(v));
  Requirement r1("double", entity::DOUBLE);
  ASSERT_TRUE(r1.convertType(v));
  ASSERT_TRUE(holds_alternative<double>(v));
  ASSERT_EQ(123.24, get<double>(v));
  
  ASSERT_FALSE(r1.convertType(v));
    
  Requirement r6("integer", INTEGER);
  ASSERT_TRUE(r6.convertType(v));
  ASSERT_TRUE(holds_alternative<int64_t>(v));
  ASSERT_EQ(123, get<int64_t>(v));

  v = 123.24;
  Requirement r4("entity", ENTITY);
  ASSERT_THROW(r4.convertType(v), PropertyConversionError);
  
  Requirement r5("entity_list", ENTITY_LIST);
  ASSERT_THROW(r5.convertType(v), PropertyConversionError);
  
  v = "aaa";
  ASSERT_THROW(r1.convertType(v), PropertyConversionError);
  ASSERT_TRUE(holds_alternative<string>(v));
  ASSERT_EQ("aaa", get<string>(v));

  v = 123.24;
  Requirement r3("vector", VECTOR);
  ASSERT_TRUE(holds_alternative<double>(v));
  ASSERT_TRUE(r3.convertType(v));
  ASSERT_TRUE(holds_alternative<Vector>(v));
  ASSERT_EQ(1, get<Vector>(v).size());
  ASSERT_EQ(123.24, get<Vector>(v)[0]);
}

TEST_F(EntityTest, TestRequirementVectorConversions)
{
  Value v("1.234 3.456 6.7889");
  ASSERT_TRUE(holds_alternative<string>(v));
  Requirement r1("vector", VECTOR);
  ASSERT_TRUE(r1.convertType(v));
  ASSERT_TRUE(holds_alternative<Vector>(v));
  ASSERT_EQ(3, get<Vector>(v).size());
  ASSERT_EQ(1.234, get<Vector>(v)[0]);
  ASSERT_EQ(3.456, get<Vector>(v)[1]);
  ASSERT_EQ(6.7889, get<Vector>(v)[2]);
  
  v = "aaaa bbb cccc";
  ASSERT_THROW(r1.convertType(v), PropertyConversionError);

  v = "  1.234     3.456       6.7889    ";
  ASSERT_TRUE(r1.convertType(v));
  ASSERT_TRUE(holds_alternative<Vector>(v));
  ASSERT_EQ(3, get<Vector>(v).size());
  ASSERT_EQ(1.234, get<Vector>(v)[0]);
  ASSERT_EQ(3.456, get<Vector>(v)[1]);
  ASSERT_EQ(6.7889, get<Vector>(v)[2]);

  Requirement r2("entity", ENTITY);
  ASSERT_THROW(r2.convertType(v), PropertyConversionError);
  
  Requirement r3("entity_list", ENTITY_LIST);
  ASSERT_THROW(r3.convertType(v), PropertyConversionError);

  Requirement r4("entity_list", entity::DOUBLE);
  ASSERT_THROW(r4.convertType(v), PropertyConversionError);

  Requirement r6("entity_list", INTEGER);
  ASSERT_THROW(r6.convertType(v), PropertyConversionError);
}
