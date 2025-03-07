//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "json_helper.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/entity/xml_parser.hpp"
#include "mtconnect/source/adapter/adapter.hpp"

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;
using namespace mtconnect::entity;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class EntityParserTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
  }

  void TearDown() override {}

  FactoryPtr components()
  {
    auto component = make_shared<Factory>(Requirements {
        Requirement("id", true),
        Requirement("name", false),
        Requirement("uuid", false),
    });

    auto components = make_shared<Factory>(Requirements(
        {Requirement("Component", ValueType::ENTITY, component, 1, Requirement::Infinite)}));
    components->registerMatchers();
    components->registerFactory(regex(".+"), component);

    component->addRequirements(
        {Requirement("Components", ValueType::ENTITY_LIST, components, false)});

    auto device = make_shared<Factory>(*component);
    device->addRequirements(Requirements {
        Requirement("name", true),
        Requirement("uuid", true),
    });

    auto root =
        make_shared<Factory>(Requirements {Requirement("Device", ValueType::ENTITY, device)});

    return root;
  }
};

TEST_F(EntityParserTest, TestParseSimpleDocument)
{
  auto fileProperty =
      make_shared<Factory>(Requirements({Requirement("name", true), Requirement("VALUE", true)}));

  auto fileProperties = make_shared<Factory>(Requirements(
      {Requirement("FileProperty", ValueType::ENTITY, fileProperty, 1, Requirement::Infinite)}));
  fileProperties->registerMatchers();

  auto fileComment = make_shared<Factory>(
      Requirements({Requirement("timestamp", true), Requirement("VALUE", true)}));

  auto fileComments = make_shared<Factory>(Requirements(
      {Requirement("FileComment", ValueType::ENTITY, fileComment, 1, Requirement::Infinite)}));
  fileComments->registerMatchers();

  auto fileArchetype = make_shared<Factory>(Requirements {
      Requirement("assetId", true), Requirement("deviceUuid", true), Requirement("timestamp", true),
      Requirement("removed", false), Requirement("name", true), Requirement("mediaType", true),
      Requirement("applicationCategory", true), Requirement("applicationType", true),
      Requirement("FileComments", ValueType::ENTITY_LIST, fileComments, false),
      Requirement("FileProperties", ValueType::ENTITY_LIST, fileProperties, false)});

  auto root = make_shared<Factory>(
      Requirements {Requirement("FileArchetype", ValueType::ENTITY, fileArchetype)});

  auto doc = string {
      "<FileArchetype name='xxxx' assetId='uuid' deviceUuid='duid' timestamp='2020-12-01T10:00Z' \n"
      "     mediaType='json' applicationCategory='ASSEMBLY' applicationType='DATA' >\n"
      "  <FileProperties>\n"
      "    <FileProperty name='one'>Round</FileProperty>\n"
      "    <FileProperty name='two'>Flat</FileProperty>\n"
      "  </FileProperties>\n"
      "</FileArchetype>"};

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(root, doc, errors);
  ASSERT_EQ(0, errors.size());

  ASSERT_EQ("FileArchetype", entity->getName());
  ASSERT_EQ("xxxx", get<string>(entity->getProperty("name")));
  ASSERT_EQ("uuid", get<string>(entity->getProperty("assetId")));
  ASSERT_EQ("2020-12-01T10:00Z", get<string>(entity->getProperty("timestamp")));
  ASSERT_EQ("json", get<string>(entity->getProperty("mediaType")));
  ASSERT_EQ("ASSEMBLY", get<string>(entity->getProperty("applicationCategory")));
  ASSERT_EQ("DATA", get<string>(entity->getProperty("applicationType")));

  auto fps = entity->getList("FileProperties");
  ASSERT_TRUE(fps);
  ASSERT_EQ(2, fps->size());

  auto it = fps->begin();
  ASSERT_EQ("FileProperty", (*it)->getName());
  ASSERT_EQ("one", get<string>((*it)->getProperty("name")));
  ASSERT_EQ("Round", get<string>((*it)->getProperty("VALUE")));

  it++;
  ASSERT_EQ("FileProperty", (*it)->getName());
  ASSERT_EQ("two", get<string>((*it)->getProperty("name")));
  ASSERT_EQ("Flat", get<string>((*it)->getProperty("VALUE")));
}

TEST_F(EntityParserTest, TestRecursiveEntityLists)
{
  auto root = components();

  auto doc = string {
      "<Device id='d1' name='foo' uuid='xxx'>\n"
      "  <Components>\n"
      "    <Systems id='s1'>\n"
      "       <Components>\n"
      "         <Electric id='e1'/>\n"
      "         <Heating id='h1'/>\n"
      "       </Components>\n"
      "    </Systems>\n"
      "  </Components>\n"
      "</Device>"};

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(root, doc, errors);
  ASSERT_EQ(0, errors.size());

  ASSERT_EQ("Device", entity->getName());
  ASSERT_EQ("d1", get<string>(entity->getProperty("id")));
  ASSERT_EQ("foo", get<string>(entity->getProperty("name")));
  ASSERT_EQ("xxx", get<string>(entity->getProperty("uuid")));

  auto l = entity->getList("Components");
  ASSERT_TRUE(l);
  ASSERT_EQ(1, l->size());

  auto systems = l->front();
  ASSERT_EQ("Systems", systems->getName());
  ASSERT_EQ("s1", get<string>(systems->getProperty("id")));

  auto sl = systems->getList("Components");
  ASSERT_TRUE(sl);
  ASSERT_EQ(2, sl->size());

  auto sli = sl->begin();

  ASSERT_EQ("Electric", (*sli)->getName());
  ASSERT_EQ("e1", get<string>((*sli)->getProperty("id")));

  sli++;
  ASSERT_EQ("Heating", (*sli)->getName());
  ASSERT_EQ("h1", get<string>((*sli)->getProperty("id")));
}

TEST_F(EntityParserTest, TestRecursiveEntityListFailure)
{
  auto root = components();

  auto doc = string {
      "<Device id='d1' name='foo'>\n"
      "  <Components>\n"
      "    <Systems id='s1'>\n"
      "       <Components>\n"
      "         <Electric id='e1'/>\n"
      "         <Heating id='h1'/>\n"
      "       </Components>\n"
      "    </Systems>\n"
      "  </Components>\n"
      "</Device>"};

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(root, doc, errors);
  ASSERT_EQ(1, errors.size());
  ASSERT_FALSE(entity);
  ASSERT_EQ(string("Device(uuid): Property uuid is required and not provided"),
            errors.front()->what());
}

TEST_F(EntityParserTest, TestRecursiveEntityListMissingComponents)
{
  auto root = components();

  auto doc = string {
      "<Device id='d1' uuid='xxx' name='foo'>\n"
      "  <Components>\n"
      "    <Systems id='s1'>\n"
      "       <Components>\n"
      "       </Components>\n"
      "    </Systems>\n"
      "  </Components>\n"
      "</Device>"};

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(root, doc, errors);
  ASSERT_EQ(2, errors.size());
  ASSERT_TRUE(entity);
  ASSERT_EQ(string("Components(Component): Entity list requirement Component must have at least 1 "
                   "entries, 0 found"),
            errors.front()->what());
  ASSERT_EQ("Device", entity->getName());
  ASSERT_EQ("d1", get<string>(entity->getProperty("id")));
  ASSERT_EQ("foo", get<string>(entity->getProperty("name")));
  ASSERT_EQ("xxx", get<string>(entity->getProperty("uuid")));

  auto l = entity->getList("Components");
  ASSERT_TRUE(l);
  ASSERT_EQ(1, l->size());

  auto systems = l->front();
  ASSERT_EQ("Systems", systems->getName());
  ASSERT_EQ("s1", get<string>(systems->getProperty("id")));

  auto sl = systems->getList("Components");
  ASSERT_FALSE(sl);
}

TEST_F(EntityParserTest, TestRawContent)
{
  auto definition =
      make_shared<Factory>(Requirements({Requirement("format", false), Requirement("RAW", true)}));

  auto root = make_shared<Factory>(
      Requirements({Requirement("Definition", ValueType::ENTITY, definition, true)}));

  auto doc = R"DOC(
<Definition format="XML">
  <SomeContent with="stuff">
    And some text
  </SomeContent>
  <AndMoreContent/>
  And random text as well.
</Definition>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(root, doc, errors);

  auto expected = R"DOC(<SomeContent with="stuff">
    And some text
  </SomeContent><AndMoreContent/>
  And random text as well.
)DOC";

  ASSERT_EQ("XML", get<string>(entity->getProperty("format")));
  ASSERT_EQ(expected, get<string>(entity->getProperty("RAW")));
}

TEST_F(EntityParserTest, check_proper_line_truncation)
{
  auto description = make_shared<Factory>(
      Requirements {Requirement("manufacturer", false), Requirement("model", false),
                    Requirement("serialNumber", false), Requirement("station", false),
                    Requirement("VALUE", false)});

  auto root =
      make_shared<Factory>(Requirements {{{"Description", ValueType::ENTITY, description, false}}});

  auto doc = R"DOC(
  <Description>
      And some text
  </Description>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(root, doc, errors);
  ASSERT_EQ("Description", entity->getName());
  ASSERT_EQ("And some text", entity->getValue<string>());
}

TEST_F(EntityParserTest, should_parse_data_sets)
{
  auto ds = make_shared<Factory>(
             Requirements {
               Requirement("DataSet", ValueType::DATA_SET,
                           true)});
  auto root = make_shared<Factory>(Requirements {{{"Root",
    ValueType::ENTITY, ds, true}}});
  
  ErrorList errors;
  entity::XmlParser parser;
  
  auto doc = R"DOC(
<Root>
  <DataSet>
    <Entry key="text">abc</Entry>
    <Entry key="int">101</Entry>
    <Entry key="double">50.5</Entry>
  </DataSet>
</Root>
)DOC";

  auto entity = parser.parse(root, doc, errors);
  ASSERT_EQ("Root", entity->getName());
  auto set = entity->get<DataSet>("DataSet");
  ASSERT_EQ("abc", set.get<string>("text"));
  ASSERT_EQ(101, set.get<int64_t>("int"));
  ASSERT_EQ(50.5, set.get<double>("double"));
}

TEST_F(EntityParserTest, should_parse_tables)
{
  auto table = make_shared<Factory>(
                                 Requirements {
                                   Requirement("Table", ValueType::TABLE,
                                               true)});
  auto root = make_shared<Factory>(Requirements {{{"Root",
    ValueType::ENTITY, table, true}}});
  
  ErrorList errors;
  entity::XmlParser parser;
  
  auto doc = R"DOC(
<Root>
  <Table>
    <Entry key="A">
      <Cell key="text">abc</Cell>
      <Cell key="int">101</Cell>
      <Cell key="double">50.5</Cell>
    </Entry>
    <Entry key="B">
      <Cell key="text2">def</Cell>
      <Cell key="int2">102</Cell>
      <Cell key="double2">100.5</Cell>
    </Entry>
  </Table>
</Root>
)DOC";
  
  auto entity = parser.parse(root, doc, errors);
  ASSERT_EQ("Root", entity->getName());
  auto set = entity->get<DataSet>("Table");
  auto e1 = set.get<DataSet>("A");
  
  ASSERT_EQ("abc", e1.get<string>("text"));
  ASSERT_EQ(101, e1.get<int64_t>("int"));
  ASSERT_EQ(50.5, e1.get<double>("double"));
  
  auto e2 = set.get<DataSet>("B");
  
  ASSERT_EQ("def", e2.get<string>("text2"));
  ASSERT_EQ(102, e2.get<int64_t>("int2"));
  ASSERT_EQ(100.5, e2.get<double>("double2"));
}
