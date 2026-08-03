//
// Copyright 2009-2026, AMT – The Association For Manufacturing Technology (“AMT”)
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
#include <date/date.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "json_helper.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/asset/asset.hpp"
#include "mtconnect/asset/target.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/entity/json_printer.hpp"
#include "mtconnect/entity/xml_parser.hpp"
#include "mtconnect/entity/xml_printer.hpp"
#include "mtconnect/printer/xml_printer.hpp"
#include "mtconnect/printer/xml_printer_helper.hpp"
#include "mtconnect/source/adapter/adapter.hpp"

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;
using namespace mtconnect::entity;
using namespace mtconnect::source::adapter;
using namespace mtconnect::asset;
using namespace mtconnect::printer;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class TargetTest : public testing::Test
{
protected:
  void SetUp() override { m_writer = make_unique<printer::XmlWriter>(true); }

  void TearDown() override { m_writer.reset(); }

  std::unique_ptr<printer::XmlWriter> m_writer;
};

TEST_F(TargetTest, simple_target_device)
{
  const auto doc = R"DOC(
<Root>
  <Targets>
    <TargetDevice deviceUuid="device-1234"/>
  </Targets>
</Root>
)DOC";

  auto root = make_shared<Factory>();
  auto tf = make_shared<Factory>(
      Requirements {{"Targets", ValueType::ENTITY_LIST, Target::getDeviceTargetsFactory(), false}});
  root->registerFactory("Root", tf);

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(root, doc, errors);
  ASSERT_EQ(0, errors.size());
  ASSERT_TRUE(entity);

  auto targets = entity->getList("Targets");
  ASSERT_TRUE(targets);
  ASSERT_EQ(1, targets->size());
  auto it = targets->begin();

  ASSERT_EQ("TargetDevice", (*it)->getName());
  ASSERT_EQ("device-1234", (*it)->get<string>("deviceUuid"));
}

TEST_F(TargetTest, target_device_and_device_group)
{
  const auto doc = R"DOC(
<Root>
  <Targets>
    <TargetDevice deviceUuid="device-1234"/>
    <TargetGroup groupId="group_id">
      <TargetDevice deviceUuid="device-5678"/>
      <TargetDevice deviceUuid="device-9999"/>
    </TargetGroup>
  </Targets>
</Root>
)DOC";

  auto root = make_shared<Factory>();
  auto tf = make_shared<Factory>(
      Requirements {{"Targets", ValueType::ENTITY_LIST, Target::getDeviceTargetsFactory(), false}});
  root->registerFactory("Root", tf);

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(root, doc, errors);
  ASSERT_EQ(0, errors.size());
  ASSERT_TRUE(entity);

  auto targets = entity->getList("Targets");
  ASSERT_TRUE(targets);
  ASSERT_EQ(2, targets->size());
  auto it = targets->begin();

  ASSERT_EQ("TargetDevice", (*it)->getName());
  ASSERT_EQ("device-1234", (*it)->get<string>("deviceUuid"));

  it++;
  auto group = *it;

  ASSERT_EQ("TargetGroup", group->getName());
  ASSERT_EQ("group_id", group->get<string>("groupId"));

  ASSERT_TRUE(group->hasProperty("LIST"));
  const auto groupTargets = group->getListProperty();
  ASSERT_EQ(2, groupTargets.size());

  auto git = groupTargets.begin();
  ASSERT_EQ("TargetDevice", (*git)->getName());
  ASSERT_EQ("device-5678", (*git)->get<string>("deviceUuid"));

  git++;
  ASSERT_EQ("TargetDevice", (*git)->getName());
  ASSERT_EQ("device-9999", (*git)->get<string>("deviceUuid"));
}

TEST_F(TargetTest, target_device_and_device_group_json)
{
  const auto doc = R"DOC(
<Root>
  <Targets>
    <TargetDevice deviceUuid="device-1234"/>
    <TargetGroup groupId="group_id">
      <TargetDevice deviceUuid="device-5678"/>
      <TargetDevice deviceUuid="device-9999"/>
    </TargetGroup>
  </Targets>
</Root>
)DOC";

  auto root = make_shared<Factory>();
  auto tf = make_shared<Factory>(
      Requirements {{"Targets", ValueType::ENTITY_LIST, Target::getDeviceTargetsFactory(), false}});
  root->registerFactory("Root", tf);

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(root, doc, errors);
  ASSERT_EQ(0, errors.size());
  ASSERT_TRUE(entity);

  entity::JsonEntityPrinter jsonPrinter(2, true);
  auto json = jsonPrinter.print(entity);

  ASSERT_EQ(R"JSON({
  "Root": {
    "Targets": {
      "TargetDevice": [
        {
          "deviceUuid": "device-1234"
        }
      ],
      "TargetGroup": [
        {
          "TargetDevice": [
            {
              "deviceUuid": "device-5678"
            },
            {
              "deviceUuid": "device-9999"
            }
          ],
          "groupId": "group_id"
        }
      ]
    }
  }
})JSON",
            json);
}

TEST_F(TargetTest, nested_target_groups_with_target_refs)
{
  const auto doc = R"DOC(
<Root>
  <Targets>
    <TargetDevice deviceUuid="device-1234"/>
    <TargetGroup groupId="A">
      <TargetDevice deviceUuid="device-5678"/>
      <TargetDevice deviceUuid="device-9999"/>
    </TargetGroup>
    <TargetGroup groupId="B">
      <TargetDevice deviceUuid="device-2222"/>
      <TargetRef groupIdRef="A"/>
    </TargetGroup>
  </Targets>
</Root>
)DOC";

  auto root = make_shared<Factory>();
  auto tf = make_shared<Factory>(
      Requirements {{"Targets", ValueType::ENTITY_LIST, Target::getDeviceTargetsFactory(), false}});
  root->registerFactory("Root", tf);

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(root, doc, errors);
  ASSERT_EQ(0, errors.size());
  ASSERT_TRUE(entity);

  auto targets = entity->getList("Targets");
  ASSERT_TRUE(targets);
  ASSERT_EQ(3, targets->size());
  auto it = targets->begin();

  ASSERT_EQ("TargetDevice", (*it)->getName());
  ASSERT_EQ("device-1234", (*it)->get<string>("deviceUuid"));

  it++;
  auto group = *it;

  ASSERT_EQ("TargetGroup", group->getName());
  ASSERT_EQ("A", group->get<string>("groupId"));

  ASSERT_TRUE(group->hasProperty("LIST"));
  const auto groupTargets = group->getListProperty();
  ASSERT_EQ(2, groupTargets.size());

  auto git = groupTargets.begin();
  ASSERT_EQ("TargetDevice", (*git)->getName());
  ASSERT_EQ("device-5678", (*git)->get<string>("deviceUuid"));

  git++;
  ASSERT_EQ("TargetDevice", (*git)->getName());
  ASSERT_EQ("device-9999", (*git)->get<string>("deviceUuid"));

  group = *(++it);
  ASSERT_EQ("TargetGroup", group->getName());
  ASSERT_EQ("B", group->get<string>("groupId"));

  ASSERT_TRUE(group->hasProperty("LIST"));
  const auto groupTargets2 = group->getListProperty();
  ASSERT_EQ(2, groupTargets2.size());

  git = groupTargets2.begin();
  ASSERT_EQ("TargetDevice", (*git)->getName());
  ASSERT_EQ("device-2222", (*git)->get<string>("deviceUuid"));

  git++;
  ASSERT_EQ("TargetRef", (*git)->getName());
  ASSERT_EQ("A", (*git)->get<string>("groupIdRef"));
}

TEST_F(TargetTest, reject_empty_groups)
{
  using namespace date;
  const auto doc = R"DOC(
<Root>
  <Targets>
    <TargetDevice deviceUuid="device-1234"/>
    <TargetGroup groupId="A">
    </TargetGroup>
  </Targets>
</Root>
)DOC";

  auto root = make_shared<Factory>();
  auto tf = make_shared<Factory>(
      Requirements {{"Targets", ValueType::ENTITY_LIST, Target::getDeviceTargetsFactory(), false}});
  root->registerFactory("Root", tf);

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(root, doc, errors);
  ASSERT_EQ(2, errors.size());
  ASSERT_TRUE(entity);

  auto targets = entity->getList("Targets");
  ASSERT_TRUE(targets);
  ASSERT_EQ(1, targets->size());
  auto it = targets->begin();

  ASSERT_EQ("TargetDevice", (*it)->getName());
  ASSERT_EQ("device-1234", (*it)->get<string>("deviceUuid"));
}

TEST_F(TargetTest, verify_target_requirement)
{
  const auto doc = R"DOC(
<Root>
  <Targets>
    <TargetRequirementTable requirementId="req1">
      <Entry key="R1"><Cell key="C1">ABC</Cell></Entry>
      <Entry key="R2"><Cell key="C2">123</Cell></Entry>
    </TargetRequirementTable>
  </Targets>
</Root>
)DOC";

  auto root = make_shared<Factory>();
  auto tf = make_shared<Factory>(Requirements {
      {"Targets", ValueType::ENTITY_LIST, Target::getRequirementTargetsFactory(), false}});
  root->registerFactory("Root", tf);

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(root, doc, errors);
  ASSERT_EQ(0, errors.size());
  ASSERT_TRUE(entity);

  auto targets = entity->getList("Targets");
  ASSERT_TRUE(targets);
  ASSERT_EQ(1, targets->size());
  auto it = targets->begin();

  ASSERT_EQ("TargetRequirementTable", (*it)->getName());
  ASSERT_EQ("req1", (*it)->get<string>("requirementId"));

  const auto table = (*it)->getValue<DataSet>();
  ASSERT_EQ(2, table.size());

  auto rowIt = table.begin();
  ASSERT_EQ("R1", rowIt->m_key);
  ASSERT_TRUE(holds_alternative<TableRow>(rowIt->m_value));
  auto &row = get<TableRow>(rowIt->m_value);
  ASSERT_EQ(1, row.size());

  auto cellIt = row.begin();
  ASSERT_EQ("C1", cellIt->m_key);
  ASSERT_TRUE(holds_alternative<string>(cellIt->m_value));
  ASSERT_EQ("ABC", get<string>(cellIt->m_value));

  rowIt++;
  ASSERT_EQ("R2", rowIt->m_key);
  ASSERT_TRUE(holds_alternative<TableRow>(rowIt->m_value));
  auto &row2 = get<TableRow>(rowIt->m_value);
  ASSERT_EQ(1, row2.size());

  cellIt = row2.begin();
  ASSERT_EQ("C2", cellIt->m_key);
  ASSERT_TRUE(holds_alternative<int64_t>(cellIt->m_value));
  ASSERT_EQ(123, get<int64_t>(cellIt->m_value));
}

TEST_F(TargetTest, verify_target_requirement_in_json)
{
  const auto doc = R"DOC(
<Root>
  <Targets>
    <TargetRequirementTable requirementId="req1">
      <Entry key="R1"><Cell key="C1">ABC</Cell></Entry>
      <Entry key="R2"><Cell key="C2">123</Cell></Entry>
    </TargetRequirementTable>
  </Targets>
</Root>
)DOC";

  auto root = make_shared<Factory>();
  auto tf = make_shared<Factory>(Requirements {
      {"Targets", ValueType::ENTITY_LIST, Target::getRequirementTargetsFactory(), false}});
  root->registerFactory("Root", tf);

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(root, doc, errors);
  ASSERT_EQ(0, errors.size());
  ASSERT_TRUE(entity);

  entity::JsonEntityPrinter jsonPrinter(2, true);
  auto json = jsonPrinter.print(entity);

  ASSERT_EQ(R"JSON({
  "Root": {
    "Targets": {
      "TargetRequirementTable": [
        {
          "value": {
            "R1": {
              "C1": "ABC"
            },
            "R2": {
              "C2": 123
            }
          },
          "requirementId": "req1"
        }
      ]
    }
  }
})JSON",
            json);
}
