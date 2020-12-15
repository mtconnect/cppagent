// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "adapter.hpp"
#include "agent.hpp"
#include "json_helper.hpp"
#include "entity/parser.hpp"

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

class EntityParserTest : public testing::Test
{
 protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
  }

  void TearDown() override
  {
  }

};

TEST_F(EntityParserTest, TestParseSimpleDocument)
{
  auto fileProperty = make_shared<Factory>(Requirements({
    Requirement("name", true ),
    Requirement("value", true) }));
  
  auto fileProperties = make_shared<Factory>(Requirements({
    Requirement("FileProperty", Requirement::ENTITY, fileProperty,
                1, Requirement::Infinite) }));
  
  auto fileComment = make_shared<Factory>(Requirements({
    Requirement("timestamp", true ),
    Requirement("value", true) }));
  
  auto fileComments = make_shared<Factory>(Requirements({
    Requirement("FileComment", Requirement::ENTITY, fileComment,
                1, Requirement::Infinite) }));
  
  auto fileArchetype = make_shared<Factory>(Requirements{
    Requirement("assetId", true ),
    Requirement("deviceUuid", true ),
    Requirement("timestamp", true ),
    Requirement("removed", false ),
    Requirement("name", true ),
    Requirement("mediaType", true),
    Requirement("applicationCategory", true),
    Requirement("applicationType", true),
    Requirement("FileComments", Requirement::ENTITY_LIST, fileComments, false),
    Requirement("FileProperties", Requirement::ENTITY_LIST, fileProperties, false)
  });
  
  auto root = make_shared<Factory>(Requirements{
    Requirement("FileArchetype", Requirement::ENTITY, fileArchetype)
  });

  auto doc = string {
    "<FileArchetype name='xxxx' assetId='uuid' deviceUuid='duid' timestamp='2020-12-01T10:00Z' \n"
    "     mediaType='json' applicationCategory='ASSEMBLY' applicationType='DATA' >\n"
    "  <FileProperties>\n"
    "    <FileProperty name='one'>Round</FileProperty>\n"
    "    <FileProperty name='two'>Flat</FileProperty>\n"
    "  </FileProperties>\n"
    "</FileArchetype>"
  };
  
  ErrorList errors;
  entity::XmlParser parser;
  
  auto entity = parser.parse(root, doc, "1.7", errors);
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
  ASSERT_EQ("Round", get<string>((*it)->getProperty("value")));
  
  it++;
  ASSERT_EQ("FileProperty", (*it)->getName());
  ASSERT_EQ("two", get<string>((*it)->getProperty("name")));
  ASSERT_EQ("Flat", get<string>((*it)->getProperty("value")));
}
