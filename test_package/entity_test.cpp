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

#include "mtconnect/entity/entity.hpp"
#include "mtconnect/entity/factory.hpp"

using namespace std;
using namespace std::literals;
using namespace mtconnect;
using namespace mtconnect::entity;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

static inline int64_t operator"" _i64(unsigned long long int i) { return int64_t(i); }

class EntityTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
  }

  void TearDown() override {}
};

TEST_F(EntityTest, TestSimpleFactory)
{
  FactoryPtr root = make_shared<Factory>();
  FactoryPtr simpleFact = make_shared<Factory>(Requirements(
      {Requirement("name", true), Requirement("id", true), Requirement("size", false, INTEGER)}));
  root->registerFactory("simple", simpleFact);

  Properties simple({{"id", "abc"s}, {"name", "xxx"s}, {"size", 10_i64}});

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

  auto second =
      make_shared<Factory>(Requirements({Requirement("key", true), Requirement("VALUE", true)}));

  auto simple = make_shared<Factory>(
      Requirements {Requirement("name", true), Requirement("id", true),
                    Requirement("size", false, INTEGER), Requirement("second", ENTITY, second)});
  root->registerFactory("simple", simple);

  auto fact = root->factoryFor("simple");
  ASSERT_TRUE(fact);

  auto sfact = fact->factoryFor("second");
  ASSERT_TRUE(sfact);

  Properties sndp {{"key", "1"s}, {"VALUE", "arf"s}};
  auto se = fact->create("second", sndp);
  ASSERT_TRUE(se);
  ASSERT_EQ(2, se->getProperties().size());
  ASSERT_EQ("1", get<std::string>(se->getProperty("key")));
  ASSERT_EQ("arf", get<std::string>(se->getProperty("VALUE")));

  Properties simpp {{"id", "abc"s}, {"name", "xxx"s}, {"size", 10_i64}, {"second", se}};

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

  auto second =
      make_shared<Factory>(Requirements({Requirement("key", true), Requirement("VALUE", true)}));

  auto seconds = make_shared<Factory>(
      Requirements({Requirement("second", ENTITY, second, 1, Requirement::Infinite)}));
  seconds->registerMatchers();

  auto simple = make_shared<Factory>(Requirements {
      Requirement("name", true), Requirement("id", true), Requirement("size", false, INTEGER),
      Requirement("seconds", ENTITY_LIST, seconds)});
  root->registerFactory("simple", simple);

  auto fact = root->factoryFor("simple");
  ASSERT_TRUE(fact);

  auto secondsFact = fact->factoryFor("seconds");
  ASSERT_TRUE(secondsFact);
  ASSERT_TRUE(secondsFact->isList());

  auto secondFact = secondsFact->factoryFor("second");
  ASSERT_TRUE(secondFact);

  Properties sndp1 {{"key", "1"s}, {"VALUE", "arf"s}};
  auto se1 = secondsFact->create("second", sndp1);
  ASSERT_TRUE(se1);
  ASSERT_EQ(2, se1->getProperties().size());
  ASSERT_EQ("1", get<std::string>(se1->getProperty("key")));
  ASSERT_EQ("arf", get<std::string>(se1->getProperty("VALUE")));

  Properties sndp2 {{"key", "2"s}, {"VALUE", "meow"s}};
  auto se2 = secondsFact->create("second", sndp2);
  ASSERT_TRUE(se2);
  ASSERT_EQ(2, se2->getProperties().size());
  ASSERT_EQ("2", get<std::string>(se2->getProperty("key")));
  ASSERT_EQ("meow", get<std::string>(se2->getProperty("VALUE")));

  ErrorList errors;
  EntityList list {se1, se2};
  auto se3 = fact->create("seconds", list, errors);
  ASSERT_EQ(0, errors.size());
  ASSERT_TRUE(se3);

  Properties simpp {{"id", "abc"s}, {"name", "xxx"s}, {"size", 10_i64}, {"seconds", se3}};

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
  FactoryPtr simpleFact = make_shared<Factory>(Requirements(
      {Requirement("name", true), Requirement("id", true), Requirement("size", false, INTEGER)}));
  root->registerFactory("simple", simpleFact);

  Properties simple {{"name", "xxx"s}, {"size", 10_i64}};

  ErrorList errors;
  auto entity = root->create("simple", simple, errors);
  ASSERT_FALSE(entity);

  ASSERT_EQ(1, errors.size());
  ASSERT_EQ(string("simple(id): Property id is required and not provided"),
            string(errors.front()->what()));
}

TEST_F(EntityTest, MissingOptionalProperty)
{
  FactoryPtr root = make_shared<Factory>();
  FactoryPtr simpleFact = make_shared<Factory>(Requirements(
      {Requirement("name", true), Requirement("id", true), Requirement("size", false, INTEGER)}));
  root->registerFactory("simple", simpleFact);

  Properties simple {{"id", "abc"s}, {"name", "xxx"s}};

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
  FactoryPtr simpleFact = make_shared<Factory>(Requirements(
      {Requirement("name", true), Requirement("id", true), Requirement("size", false, INTEGER)}));
  root->registerFactory("simple", simpleFact);

  Properties simple {{"id", "abc"s}, {"name", "xxx"s}, {"junk", "junk"s}};

  ErrorList errors;
  auto entity = root->create("simple", simple, errors);
  ASSERT_FALSE(entity);

  ASSERT_EQ(1, errors.size());
  ASSERT_EQ(string("simple(): The following keys were present and not expected: junk,"),
            string(errors.front()->what()));
}

TEST_F(EntityTest, EntityListAnyEntities)
{
  auto root = make_shared<Factory>();

  auto second =
      make_shared<Factory>(Requirements({Requirement("key", true), Requirement("VALUE", true)}));

  auto seconds = make_shared<Factory>(
      Requirements({Requirement("something", ENTITY, second, 1, Requirement::Infinite)}));
  seconds->registerFactory(regex(".+"), second);
  seconds->registerMatchers();

  auto simple = make_shared<Factory>(Requirements {
      Requirement("name", true), Requirement("id", true), Requirement("size", false, INTEGER),
      Requirement("seconds", ENTITY_LIST, seconds)});
  root->registerFactory("simple", simple);

  auto fact = root->factoryFor("simple");
  ASSERT_TRUE(fact);

  auto secondsFact = fact->factoryFor("seconds");
  ASSERT_TRUE(secondsFact);

  auto secondFact = secondsFact->factoryFor("dog");
  ASSERT_TRUE(secondFact);

  ErrorList errors;

  Properties sndp1 {{"key", "1"s}, {"VALUE", "arf"s}};
  auto se1 = secondsFact->create("dog", sndp1, errors);
  ASSERT_EQ(0, errors.size());
  ASSERT_TRUE(se1);
  ASSERT_EQ(2, se1->getProperties().size());
  ASSERT_EQ("1", get<std::string>(se1->getProperty("key")));
  ASSERT_EQ("arf", get<std::string>(se1->getProperty("VALUE")));

  Properties sndp2 {{"key", "2"s}, {"VALUE", "meow"s}};
  auto se2 = secondsFact->create("cat", sndp2, errors);
  ASSERT_EQ(0, errors.size());
  ASSERT_TRUE(se2);
  ASSERT_EQ(2, se2->getProperties().size());
  ASSERT_EQ("2", get<std::string>(se2->getProperty("key")));
  ASSERT_EQ("meow", get<std::string>(se2->getProperty("VALUE")));

  EntityList list {se1, se2};
  auto se3 = fact->create("seconds", list, errors);
  ASSERT_EQ(0, errors.size());
  ASSERT_TRUE(se3);

  Properties simpp {{"id", "abc"s}, {"name", "xxx"s}, {"size", 10_i64}, {"seconds", se3}};

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
  Value v("123"s);
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

  v = "aaa"s;
  ASSERT_THROW(r1.convertType(v), PropertyError);
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
  ASSERT_THROW(r4.convertType(v), PropertyError);

  Requirement r5("entity_list", ENTITY_LIST);
  ASSERT_THROW(r5.convertType(v), PropertyError);

  v = 1234.0;
  ASSERT_TRUE(holds_alternative<double>(v));
  ASSERT_TRUE(r1.convertType(v));
  ASSERT_TRUE(holds_alternative<int64_t>(v));
  ASSERT_EQ(1234_i64, get<int64_t>(v));

  v = nullptr;
  ASSERT_THROW(r1.convertType(v), PropertyError);
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

  v = Vector {1.123, 2.345, 6.789};
  ASSERT_TRUE(holds_alternative<Vector>(v));
  ASSERT_TRUE(r1.convertType(v));
  ASSERT_TRUE(holds_alternative<string>(v));
  ASSERT_EQ("1.123 2.345 6.789", get<string>(v));

  ASSERT_FALSE(r1.convertType(v));
}

TEST_F(EntityTest, TestRequirementDoubleConversions)
{
  Value v("123.24"s);
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
  ASSERT_THROW(r4.convertType(v), PropertyError);

  Requirement r5("entity_list", ENTITY_LIST);
  ASSERT_THROW(r5.convertType(v), PropertyError);

  v = "aaa"s;
  ASSERT_THROW(r1.convertType(v), PropertyError);
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
  Value v("1.234 3.456 6.7889"s);
  ASSERT_TRUE(holds_alternative<string>(v));
  Requirement r1("vector", VECTOR);
  ASSERT_TRUE(r1.convertType(v));
  ASSERT_TRUE(holds_alternative<Vector>(v));
  EXPECT_EQ(3, get<Vector>(v).size());
  EXPECT_EQ(1.234, get<Vector>(v)[0]);
  EXPECT_EQ(3.456, get<Vector>(v)[1]);
  EXPECT_EQ(6.7889, get<Vector>(v)[2]);

  v = "aaaa bbb cccc"s;
  EXPECT_THROW(r1.convertType(v), PropertyError);

  v = "  1.234     3.456       6.7889    "s;
  ASSERT_TRUE(r1.convertType(v));
  ASSERT_TRUE(holds_alternative<Vector>(v));
  EXPECT_EQ(3, get<Vector>(v).size());
  EXPECT_EQ(1.234, get<Vector>(v)[0]);
  EXPECT_EQ(3.456, get<Vector>(v)[1]);
  EXPECT_EQ(6.7889, get<Vector>(v)[2]);

  Requirement r2("entity", ENTITY);
  EXPECT_THROW(r2.convertType(v), PropertyError);

  Requirement r3("entity_list", ENTITY_LIST);
  EXPECT_THROW(r3.convertType(v), PropertyError);

  Requirement r4("entity_list", entity::DOUBLE);
  EXPECT_THROW(r4.convertType(v), PropertyError);

  Requirement r6("entity_list", INTEGER);
  EXPECT_THROW(r6.convertType(v), PropertyError);
}

TEST_F(EntityTest, TestRequirementUpperCaseStringConversion)
{
  Value v("hello kitty"s);
  ASSERT_TRUE(holds_alternative<string>(v));
  Requirement r1("string", USTRING);
  ASSERT_TRUE(r1.convertType(v));
  ASSERT_EQ("HELLO KITTY", get<string>(v));
}

TEST_F(EntityTest, TestControlledVocabulary)
{
  FactoryPtr root = make_shared<Factory>();
  FactoryPtr simpleFact = make_shared<Factory>(
      Requirements({{"name", true}, {"id", true}, {"type", {"BIG", "SMALL", "OTHER"}, true}}));
  root->registerFactory("simple", simpleFact);

  Properties simple {{"id", "abc"s}, {"name", "xxx"s}, {"type", "BIG"s}};

  ErrorList errors;
  auto entity = root->create("simple", simple, errors);
  ASSERT_EQ(0, errors.size());

  ASSERT_TRUE(entity);
  ASSERT_EQ(3, entity->getProperties().size());
  ASSERT_EQ("simple", entity->getName());
  ASSERT_EQ("abc", get<std::string>(entity->getProperty("id")));
  ASSERT_EQ("xxx", get<std::string>(entity->getProperty("name")));
  ASSERT_EQ("BIG", get<std::string>(entity->getProperty("type")));

  Properties fail {{"id", "abc"s}, {"name", "xxx"s}, {"type", "BAD"s}};

  auto entity2 = root->create("simple", fail, errors);
  ASSERT_EQ(1, errors.size());
  ASSERT_EQ("simple(type): Invalid value for 'type': 'BAD' is not allowed",
            string(errors.front()->what()));
}

TEST_F(EntityTest, entity_list_requirements_need_with_at_least_one_requiremenet)
{
  auto ref1 = make_shared<Factory>(Requirements {{"id", true}, {"name", false}, {"type", true}});
  auto ref2 = make_shared<Factory>(
      Requirements {{"id", true}, {"name", false}, {"type", true}, {"size", INTEGER, true}});

  auto refs = make_shared<Factory>(Requirements {
      {"Reference1", ENTITY, ref1, 0, 1}, {"Reference2", ENTITY, ref2, 0, Requirement::Infinite}});
  refs->setMinListSize(1);

  auto agg = make_shared<Factory>(Requirements {{"References", ENTITY_LIST, refs, true}});

  ErrorList errors;
  auto r1 = refs->create("Reference1", {{"id", "a"s}, {"type", "REF1"s}}, errors);
  ASSERT_EQ(0, errors.size());
  auto r2 =
      refs->create("Reference2", {{"id", "b"s}, {"type", "REF2"s}, {"size", int64_t(10)}}, errors);
  ASSERT_EQ(0, errors.size());
  auto r3 =
      refs->create("Reference2", {{"id", "c"s}, {"type", "REF2"s}, {"size", int64_t(10)}}, errors);
  ASSERT_EQ(0, errors.size());

  EntityList list {r1, r2, r3};
  auto top = agg->create("References", list, errors);
  ASSERT_EQ(0, errors.size());

  ASSERT_EQ(3, top->get<EntityList>("LIST").size());

  EntityList empty;
  auto bad = agg->create("References", empty, errors);
  ASSERT_EQ(1, errors.size());
  ASSERT_FALSE(bad);

  errors.clear();
  auto r4 = refs->create("Reference1", {{"id", "d"s}, {"type", "REF1"s}}, errors);
  ASSERT_EQ(0, errors.size());

  EntityList list2 {r1, r2, r3, r4};
  auto bad2 = agg->create("References", list2, errors);
  ASSERT_EQ(1, errors.size());
  ASSERT_TRUE(bad2);
}

TEST_F(EntityTest, entities_should_compare_for_equality)
{
  auto root = make_shared<Factory>();

  auto second =
      make_shared<Factory>(Requirements({Requirement("key", true), Requirement("VALUE", true)}));

  auto simple = make_shared<Factory>(
      Requirements {Requirement("name", true), Requirement("id", true),
                    Requirement("size", false, INTEGER), Requirement("second", ENTITY, second)});
  root->registerFactory("simple", simple);

  auto createEnt = [&]() -> EntityPtr {
    auto fact = root->factoryFor("simple");
    auto sfact = fact->factoryFor("second");

    Properties sndp {{"key", "1"s}, {"VALUE", "arf"s}};
    auto se = fact->create("second", sndp);

    Properties simpp {{"id", "abc"s}, {"name", "xxx"s}, {"size", 10_i64}, {"second", se}};

    auto entity = root->create("simple", simpp);
    return entity;
  };

  auto v1 = createEnt();
  ASSERT_TRUE(v1);

  auto v2 = createEnt();
  ASSERT_TRUE(v2);

  ASSERT_EQ(*(v1.get()), *(v2.get()));
}

TEST_F(EntityTest, entities_should_compare_for_inequality)
{
  auto root = make_shared<Factory>();

  auto second =
      make_shared<Factory>(Requirements({Requirement("key", true), Requirement("VALUE", true)}));

  auto simple = make_shared<Factory>(
      Requirements {Requirement("name", true), Requirement("id", true),
                    Requirement("size", false, INTEGER), Requirement("second", ENTITY, second)});
  root->registerFactory("simple", simple);

  auto createEnt = [&](auto v) -> EntityPtr {
    auto fact = root->factoryFor("simple");
    auto sfact = fact->factoryFor("second");

    Properties sndp {{"key", "1"s}, {"VALUE", v}};
    auto se = fact->create("second", sndp);

    Properties simpp {{"id", "abc"s}, {"name", "xxx"s}, {"size", 10_i64}, {"second", se}};

    auto entity = root->create("simple", simpp);
    return entity;
  };

  auto v1 = createEnt("woof"s);
  ASSERT_TRUE(v1);

  auto v2 = createEnt("meow"s);
  ASSERT_TRUE(v2);

  ASSERT_NE(*(v1.get()), *(v2.get()));
}

TEST_F(EntityTest, entities_should_compare_for_equality_with_entity_list)
{
  auto root = make_shared<Factory>();

  auto second =
      make_shared<Factory>(Requirements({Requirement("key", true), Requirement("VALUE", true)}));

  auto seconds = make_shared<Factory>(
      Requirements({Requirement("second", ENTITY, second, 1, Requirement::Infinite)}));
  seconds->registerMatchers();

  auto simple = make_shared<Factory>(Requirements {
      Requirement("name", true), Requirement("id", true), Requirement("size", false, INTEGER),
      Requirement("seconds", ENTITY_LIST, seconds)});
  root->registerFactory("simple", simple);

  auto createEnt = [&]() -> EntityPtr {
    auto fact = root->factoryFor("simple");
    auto secondsFact = fact->factoryFor("seconds");
    auto secondFact = secondsFact->factoryFor("second");

    Properties sndp1 {{"key", "1"s}, {"VALUE", "arf"s}};
    auto se1 = secondsFact->create("second", sndp1);

    Properties sndp2 {{"key", "2"s}, {"VALUE", "meow"s}};
    auto se2 = secondsFact->create("second", sndp2);

    ErrorList errors;
    EntityList list {se1, se2};
    auto se3 = fact->create("seconds", list, errors);

    Properties simpp {{"id", "abc"s}, {"name", "xxx"s}, {"size", 10_i64}, {"seconds", se3}};

    auto entity = root->create("simple", simpp);
    return entity;
  };

  auto v1 = createEnt();
  ASSERT_TRUE(v1);

  auto v2 = createEnt();
  ASSERT_TRUE(v2);

  ASSERT_EQ(*(v1.get()), *(v2.get()));
}

TEST_F(EntityTest, entities_should_compare_for_inequality_with_entity_list)
{
  auto root = make_shared<Factory>();

  auto second =
      make_shared<Factory>(Requirements({Requirement("key", true), Requirement("VALUE", true)}));

  auto seconds = make_shared<Factory>(
      Requirements({Requirement("second", ENTITY, second, 1, Requirement::Infinite)}));
  seconds->registerMatchers();

  auto simple = make_shared<Factory>(Requirements {
      Requirement("name", true), Requirement("id", true), Requirement("size", false, INTEGER),
      Requirement("seconds", ENTITY_LIST, seconds)});
  root->registerFactory("simple", simple);

  auto createEnt = [&](auto t) -> EntityPtr {
    auto fact = root->factoryFor("simple");
    auto secondsFact = fact->factoryFor("seconds");
    auto secondFact = secondsFact->factoryFor("second");

    Properties sndp1 {{"key", "1"s}, {"VALUE", "arf"s}};
    auto se1 = secondsFact->create("second", sndp1);

    Properties sndp2 {{"key", "2"s}, {"VALUE", t}};
    auto se2 = secondsFact->create("second", sndp2);

    ErrorList errors;
    EntityList list {se1, se2};
    auto se3 = fact->create("seconds", list, errors);

    Properties simpp {{"id", "abc"s}, {"name", "xxx"s}, {"size", 10_i64}, {"seconds", se3}};

    auto entity = root->create("simple", simpp);
    return entity;
  };

  auto v1 = createEnt("woof"s);
  ASSERT_TRUE(v1);

  auto v2 = createEnt("meow"s);
  ASSERT_TRUE(v2);

  ASSERT_NE(*(v1.get()), *(v2.get()));
}

TEST_F(EntityTest, entities_should_merge)
{
  auto root = make_shared<Factory>();

  auto second =
      make_shared<Factory>(Requirements({Requirement("key", true), Requirement("VALUE", true)}));

  auto simple = make_shared<Factory>(
      Requirements {Requirement("name", true), Requirement("id", true),
                    Requirement("size", false, INTEGER), Requirement("second", ENTITY, second)});
  root->registerFactory("simple", simple);

  auto createEnt = [&](auto v, auto k) -> EntityPtr {
    auto fact = root->factoryFor("simple");
    auto sfact = fact->factoryFor("second");

    Properties sndp {{"key", "1"s}, {"VALUE", v}};
    auto se = fact->create("second", sndp);

    Properties simpp {{"id", "abc"s}, {"name", "xxx"s}, {"size", k}, {"second", se}};

    auto entity = root->create("simple", simpp);
    return entity;
  };

  auto v1 = createEnt("woof"s, 10_i64);
  ASSERT_TRUE(v1);
  ASSERT_EQ(10_i64, v1->get<int64_t>("size"));

  auto sec1 = v1->get<EntityPtr>("second");
  ASSERT_TRUE(sec1);
  ASSERT_EQ("woof"s, sec1->getValue<string>());

  auto v2 = createEnt("meow"s, 20_i64);
  ASSERT_TRUE(v2);

  v1->reviseTo(v2);
  ASSERT_EQ(20_i64, v1->get<int64_t>("size"));
  ASSERT_EQ("meow"s, sec1->getValue<string>());

  ASSERT_EQ(*(v1.get()), *(v2.get()));
}

TEST_F(EntityTest, entities_should_merge_entity_list)
{
  auto root = make_shared<Factory>();

  auto second = make_shared<Factory>(
      Requirements({Requirement("id", true), Requirement("VALUE", true, INTEGER)}));

  auto seconds = make_shared<Factory>(
      Requirements({Requirement("second", ENTITY, second, 1, Requirement::Infinite)}));
  seconds->registerMatchers();

  auto simple = make_shared<Factory>(Requirements {
      Requirement("name", true), Requirement("id", true), Requirement("size", false, INTEGER),
      Requirement("seconds", ENTITY_LIST, seconds)});
  root->registerFactory("simple", simple);

  auto createEnt = [&](auto v, auto s) -> EntityPtr {
    auto fact = root->factoryFor("simple");
    auto secondsFact = fact->factoryFor("seconds");
    auto secondFact = secondsFact->factoryFor("second");

    Properties sndp1 {{"id", "1"s}, {"VALUE", s + 1}};
    auto se1 = secondsFact->create("second", sndp1);

    Properties sndp2 {{"id", "2"s}, {"VALUE", s + 2}};
    auto se2 = secondsFact->create("second", sndp2);

    ErrorList errors;
    EntityList list {se1, se2};
    auto se3 = fact->create("seconds", list, errors);

    Properties simpp {{"id", "abc"s}, {"name", "xxx"s}, {"size", s}, {"seconds", se3}};

    auto entity = root->create("simple", simpp);
    return entity;
  };

  auto v1 = createEnt("woof"s, 10_i64);
  ASSERT_TRUE(v1);

  auto list = v1->getList("seconds");
  ASSERT_TRUE(list);
  ASSERT_EQ(2, list->size());

  auto v2 = createEnt("meow"s, 20_i64);
  ASSERT_TRUE(v2);

  v1->reviseTo(v2);
  auto it = list->begin();
  ASSERT_EQ(21_i64, (*it)->getValue<int64_t>());
  it++;
  ASSERT_EQ(22_i64, (*it)->getValue<int64_t>());

  ASSERT_EQ(*(v1.get()), *(v2.get()));
}

TEST_F(EntityTest, entities_should_merge_entity_list_with_new_item)
{
  auto root = make_shared<Factory>();

  auto second = make_shared<Factory>(
      Requirements({Requirement("id", true), Requirement("VALUE", true, INTEGER)}));

  auto seconds = make_shared<Factory>(
      Requirements({Requirement("second", ENTITY, second, 1, Requirement::Infinite)}));
  seconds->registerMatchers();

  auto simple = make_shared<Factory>(Requirements {
      Requirement("name", true), Requirement("id", true), Requirement("size", false, INTEGER),
      Requirement("seconds", ENTITY_LIST, seconds)});
  root->registerFactory("simple", simple);

  auto createEnt = [&](auto v, auto s) -> EntityPtr {
    auto fact = root->factoryFor("simple");
    auto secondsFact = fact->factoryFor("seconds");
    auto secondFact = secondsFact->factoryFor("second");

    Properties sndp1 {{"id", "1"s}, {"VALUE", 1_i64}};
    auto se1 = secondsFact->create("second", sndp1);
    EntityList list {se1};

    if (s)
    {
      Properties sndp2 {{"id", "2"s}, {"VALUE", 2_i64}};
      auto se2 = secondsFact->create("second", sndp2);
      list.push_back(se2);
    }

    ErrorList errors;
    auto se3 = fact->create("seconds", list, errors);

    Properties simpp {{"id", "abc"s}, {"name", "xxx"s}, {"size", s + 1}, {"seconds", se3}};

    auto entity = root->create("simple", simpp);
    return entity;
  };

  auto v1 = createEnt("woof"s, 0_i64);
  ASSERT_TRUE(v1);

  auto const &list1 = v1->getList("seconds");
  ASSERT_TRUE(list1);
  EXPECT_EQ(1, list1->size());

  auto v2 = createEnt("meow"s, 1_i64);
  ASSERT_TRUE(v2);

  auto const &list2 = v2->getList("seconds");
  ASSERT_TRUE(list2);
  EXPECT_EQ(2, list2->size());

  ASSERT_TRUE(v1->reviseTo(v2));
  // EXPECT_EQ(2, list1->size());

  auto const &list3 = v1->getList("seconds");
  EXPECT_EQ(2, list3->size());

  auto it = list3->begin();
  EXPECT_EQ(1_i64, (*it)->getValue<int64_t>());
  it++;
  EXPECT_EQ(2_i64, (*it)->getValue<int64_t>());

  EXPECT_EQ(*(v1.get()), *(v2.get()));
}

TEST_F(EntityTest, should_remove_missing_entities)
{
  auto root = make_shared<Factory>();

  auto second = make_shared<Factory>(
      Requirements({Requirement("id", true), Requirement("VALUE", true, INTEGER)}));

  auto seconds = make_shared<Factory>(
      Requirements({Requirement("second", ENTITY, second, 1, Requirement::Infinite)}));
  seconds->registerMatchers();

  auto simple = make_shared<Factory>(Requirements {
      Requirement("name", true), Requirement("id", true), Requirement("size", false, INTEGER),
      Requirement("seconds", ENTITY_LIST, seconds)});
  root->registerFactory("simple", simple);

  auto createEnt = [&](auto v, auto s) -> EntityPtr {
    auto fact = root->factoryFor("simple");
    auto secondsFact = fact->factoryFor("seconds");
    auto secondFact = secondsFact->factoryFor("second");

    Properties sndp1 {{"id", "1"s}, {"VALUE", 1_i64}};
    auto se1 = secondsFact->create("second", sndp1);

    EntityList list {se1};
    if (s)
    {
      Properties sndp2 {{"id", "2"s}, {"VALUE", 2_i64}};
      auto se2 = secondsFact->create("second", sndp2);
      list.push_back(se2);
    }

    ErrorList errors;
    auto se3 = fact->create("seconds", list, errors);

    Properties simpp {{"id", "abc"s}, {"name", "xxx"s}, {"size", 10_i64}, {"seconds", se3}};

    auto entity = root->create("simple", simpp);
    return entity;
  };

  auto v1 = createEnt("woof"s, true);
  ASSERT_TRUE(v1);

  auto list = v1->getList("seconds");
  ASSERT_TRUE(list);
  ASSERT_EQ(2, list->size());

  auto v2 = createEnt("meow"s, false);
  ASSERT_TRUE(v2);

  auto list2 = v2->getList("seconds");
  ASSERT_TRUE(list2);
  ASSERT_EQ(1, list2->size());

  v1->reviseTo(v2);
  auto list3 = v1->getList("seconds");
  ASSERT_TRUE(list3);
  ASSERT_EQ(1, list3->size());

  ASSERT_EQ(*(v1.get()), *(v2.get()));
}

TEST_F(EntityTest, should_ignore_certain_entities_with_specific_ids)
{
  auto root = make_shared<Factory>();

  auto second = make_shared<Factory>(
      Requirements({Requirement("id", true), Requirement("VALUE", true, INTEGER)}));

  auto seconds = make_shared<Factory>(
      Requirements({Requirement("second", ENTITY, second, 1, Requirement::Infinite)}));
  seconds->registerMatchers();

  auto simple = make_shared<Factory>(Requirements {
      Requirement("name", true), Requirement("id", true), Requirement("size", false, INTEGER),
      Requirement("seconds", ENTITY_LIST, seconds)});
  root->registerFactory("simple", simple);

  auto createEnt = [&](auto v, auto s) -> EntityPtr {
    auto fact = root->factoryFor("simple");
    auto secondsFact = fact->factoryFor("seconds");
    auto secondFact = secondsFact->factoryFor("second");

    Properties sndp1 {{"id", "1"s}, {"VALUE", 1_i64}};
    auto se1 = secondsFact->create("second", sndp1);

    EntityList list {se1};
    if (s)
    {
      Properties sndp2 {{"id", "2"s}, {"VALUE", 2_i64}};
      auto se2 = secondsFact->create("second", sndp2);
      list.push_back(se2);
    }

    ErrorList errors;
    auto se3 = fact->create("seconds", list, errors);

    Properties simpp {{"id", "abc"s}, {"name", "xxx"s}, {"size", 10_i64}, {"seconds", se3}};

    auto entity = root->create("simple", simpp);
    return entity;
  };

  auto v1 = createEnt("woof"s, true);
  ASSERT_TRUE(v1);

  auto list = v1->getList("seconds");
  ASSERT_TRUE(list);
  ASSERT_EQ(2, list->size());

  auto v2 = createEnt("meow"s, false);
  ASSERT_TRUE(v2);

  auto list2 = v2->getList("seconds");
  ASSERT_TRUE(list2);
  ASSERT_EQ(1, list2->size());

  v1->reviseTo(v2, {"2"s});
  auto list3 = v1->getList("seconds");
  ASSERT_TRUE(list3);
  ASSERT_EQ(2, list3->size());

  ASSERT_NE(*(v1.get()), *(v2.get()));
}

TEST_F(EntityTest, should_ignore_certain_entities_with_changes_and_removals)
{
  auto root = make_shared<Factory>();

  auto second = make_shared<Factory>(
      Requirements({Requirement("id", true), Requirement("VALUE", true, INTEGER)}));

  auto seconds = make_shared<Factory>(
      Requirements({Requirement("second", ENTITY, second, 1, Requirement::Infinite)}));
  seconds->registerMatchers();

  auto simple = make_shared<Factory>(Requirements {
      Requirement("name", true), Requirement("id", true), Requirement("size", false, INTEGER),
      Requirement("seconds", ENTITY_LIST, seconds)});
  root->registerFactory("simple", simple);

  auto createEnt = [&](auto v, auto s, int i) -> EntityPtr {
    auto fact = root->factoryFor("simple");
    auto secondsFact = fact->factoryFor("seconds");
    auto secondFact = secondsFact->factoryFor("second");

    Properties sndp1 {{"id", "1"s}, {"VALUE", 10_i64 + i}};
    auto se1 = secondsFact->create("second", sndp1);

    EntityList list {se1};
    if (s)
    {
      Properties sndp2 {{"id", "2"s}, {"VALUE", 20_i64 + i}};
      auto se2 = secondsFact->create("second", sndp2);
      list.push_back(se2);
    }

    ErrorList errors;
    auto se3 = fact->create("seconds", list, errors);

    Properties simpp {{"id", "abc"s}, {"name", "xxx"s}, {"size", 10_i64}, {"seconds", se3}};

    auto entity = root->create("simple", simpp);
    return entity;
  };

  auto v1 = createEnt("woof"s, true, 1);
  ASSERT_TRUE(v1);

  auto list = v1->getList("seconds");
  ASSERT_TRUE(list);
  ASSERT_EQ(2, list->size());

  auto v2 = createEnt("meow"s, false, 2);
  ASSERT_TRUE(v2);

  auto list2 = v2->getList("seconds");
  ASSERT_TRUE(list2);
  ASSERT_EQ(1, list2->size());

  v1->reviseTo(v2);
  auto list3 = v1->getList("seconds");
  ASSERT_TRUE(list3);
  ASSERT_EQ(1, list3->size());
  ASSERT_EQ(12_i64, list->front()->getValue<int64_t>());

  ASSERT_EQ(*(v1.get()), *(v2.get()));
}

TEST_F(EntityTest, entities_should_merge_entity_lists_without_identity) { GTEST_SKIP(); }
