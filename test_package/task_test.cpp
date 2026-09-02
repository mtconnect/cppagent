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
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/asset/asset.hpp"
#include "mtconnect/asset/task.hpp"
#include "mtconnect/entity/xml_parser.hpp"
#include "mtconnect/entity/xml_printer.hpp"
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
int main(int argc, char* argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class TaskAssetTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    Task::registerAsset();
    TaskArchetype::registerAsset();
    m_writer = make_unique<XmlWriter>(true);
  }

  void TearDown() override { m_writer.reset(); }

  std::unique_ptr<XmlWriter> m_writer;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

/// @section TaskArchetype tests

TEST_F(TaskAssetTest, should_parse_a_part_archetype)
{
  const auto doc =
      R"DOC(<TaskArchetype assetId="1aa7eece248093" deviceUuid="mxi_m001" hash="fCI1rCQv8BcHbzZeoMxt3kHmb9k=" timestamp="2024-12-10T05:17:05.531454Z">
  <TaskType>MATERIAL_UNLOAD</TaskType>
  <Priority>10</Priority>
  <Targets>
    <TargetDevice deviceUuid="Mazak123"/>
    <TargetDevice deviceUuid="Mazak456"/>
    <TargetGroup groupId="MyRobots">
      <TargetDevice deviceUuid="UR123"/>
      <TargetDevice deviceUuid="UR456"/>
    </TargetGroup>
  </Targets>
  <Coordinator>
    <Collaborator collaboratorId="machine" type="CNC">
      <Targets>
        <TargetDevice deviceUuid="Mazak123"/>
        <TargetDevice deviceUuid="Mazak456"/>
      </Targets>
    </Collaborator>
  </Coordinator>
  <Collaborators>
    <Collaborator collaboratorId="Robot" type="ROBOT">
      <Targets>
        <TargetRequirementTable requirementId="ab">
          <Entry key="PAYLOAD">
            <Cell key="maximum">1000</Cell>
          </Entry>
          <Entry key="REACH">
            <Cell key="minimum">1500</Cell>
          </Entry>
        </TargetRequirementTable>
        <TargetRef groupIdRef="MyRobots"/>
      </Targets>
    </Collaborator>
    <Collaborator collaboratorId="robot2" type="ROBOT">
      <Targets>
        <TargetDevice deviceUuid="UR890"/>
      </Targets>
    </Collaborator>
  </Collaborators>
  <SubTaskRefs>
    <SubTaskRef order="1">UnloadConv</SubTaskRef>
    <SubTaskRef order="2">LoadCnc</SubTaskRef>
  </SubTaskRefs>
</TaskArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset*>(entity.get());
  ASSERT_NE(nullptr, asset);

  EXPECT_EQ("TaskArchetype", asset->getName());
  EXPECT_EQ("1aa7eece248093", asset->getAssetId());
  EXPECT_EQ("mxi_m001", asset->get<string>("deviceUuid"));

  auto targets = asset->getList("Targets");
  ASSERT_TRUE(targets);
  ASSERT_EQ(3, targets->size());

  {
    auto it = targets->begin();
    EXPECT_EQ("TargetDevice", (*it)->getName());
    EXPECT_EQ("Mazak123", (*it)->get<string>("deviceUuid"));

    it++;
    EXPECT_EQ("TargetDevice", (*it)->getName());
    EXPECT_EQ("Mazak456", (*it)->get<string>("deviceUuid"));

    auto targetGroup = *++it;
    EXPECT_EQ("TargetGroup", targetGroup->getName());
    EXPECT_EQ("MyRobots", targetGroup->get<string>("groupId"));

    auto targetDevices = targetGroup->get<EntityList>("LIST");
    ASSERT_EQ(2, targetDevices.size());

    {
      auto dit = targetDevices.begin();
      EXPECT_EQ("TargetDevice", (*dit)->getName());
      EXPECT_EQ("UR123", (*dit)->get<string>("deviceUuid"));
      dit++;

      EXPECT_EQ("TargetDevice", (*dit)->getName());
      EXPECT_EQ("UR456", (*dit)->get<string>("deviceUuid"));
    }
  }

  auto coordinator = asset->get<EntityPtr>("Coordinator");
  ASSERT_TRUE(coordinator);
  EXPECT_EQ("Coordinator", coordinator->getName());
  auto collaborator = coordinator->get<EntityPtr>("Collaborator");
  ASSERT_TRUE(collaborator);
  EXPECT_EQ("machine", collaborator->get<string>("collaboratorId"));
  EXPECT_EQ("CNC", collaborator->get<string>("type"));

  auto collTargets = collaborator->getList("Targets");
  ASSERT_EQ(2, collTargets->size());
  {
    auto it = collTargets->begin();
    EXPECT_EQ("TargetDevice", (*it)->getName());
    EXPECT_EQ("Mazak123", (*it)->get<string>("deviceUuid"));
    it++;
    EXPECT_EQ("TargetDevice", (*it)->getName());
    EXPECT_EQ("Mazak456", (*it)->get<string>("deviceUuid"));
  }

  auto collaborators = asset->getList("Collaborators");
  ASSERT_EQ(2, collaborators->size());
  {
    auto it = collaborators->begin();
    auto collab1 = *it;
    EXPECT_EQ("Robot", collab1->get<string>("collaboratorId"));
    EXPECT_EQ("ROBOT", collab1->get<string>("type"));
    auto targets = collab1->getList("Targets");
    ASSERT_EQ(2, targets->size());
    {
      auto tit = targets->begin();
      EXPECT_EQ("TargetRequirementTable", (*tit)->getName());
      EXPECT_EQ("ab", (*tit)->get<string>("requirementId"));

      auto table = (*tit)->getValue<DataSet>();
      ASSERT_EQ(2, table.size());

      const auto& row1 = get<TableRow>(table.find(DataSetEntry("PAYLOAD"))->m_value);
      ASSERT_EQ(1, row1.size());
      EXPECT_EQ(1000, get<int64_t>(row1.find(TableCell("maximum"))->m_value));

      const auto& row2 = get<TableRow>(table.find(DataSetEntry("REACH"))->m_value);
      ASSERT_EQ(1, row2.size());
      EXPECT_EQ(1500, get<int64_t>(row2.find(TableCell("minimum"))->m_value));

      tit++;
      ASSERT_EQ("TargetRef", (*tit)->getName());
      ASSERT_EQ("MyRobots", (*tit)->get<string>("groupIdRef"));
    }

    it++;
    auto collab2 = *it;
    EXPECT_EQ("robot2", collab2->get<string>("collaboratorId"));
    EXPECT_EQ("ROBOT", collab2->get<string>("type"));

    auto targets2 = collab2->getList("Targets");
    ASSERT_EQ(1, targets2->size());
    {
      auto target = targets2->front();
      EXPECT_EQ("TargetDevice", target->getName());
      EXPECT_EQ("UR890", target->get<string>("deviceUuid"));
    }
  }

  auto subtasks = asset->getList("SubTaskRefs");
  ASSERT_TRUE(subtasks);
  ASSERT_EQ(2, subtasks->size());

  {
    auto it = subtasks->begin();
    EXPECT_EQ("SubTaskRef", (*it)->getName());
    EXPECT_EQ(1, (*it)->get<int64_t>("order"));
    EXPECT_EQ("UnloadConv", (*it)->getValue<string>());

    it++;
    EXPECT_EQ("SubTaskRef", (*it)->getName());
    EXPECT_EQ(2, (*it)->get<int64_t>("order"));
    EXPECT_EQ("LoadCnc", (*it)->getValue<string>());
  }

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}

TEST_F(TaskAssetTest, task_archetype_should_produce_json)
{
  const auto doc =
      R"DOC(<TaskArchetype assetId="1aa7eece248093" deviceUuid="mxi_m001" hash="fCI1rCQv8BcHbzZeoMxt3kHmb9k=" timestamp="2024-12-10T05:17:05.531454Z">
  <TaskType>MATERIAL_UNLOAD</TaskType>
  <Priority>10</Priority>
  <Targets>
    <TargetDevice deviceUuid="Mazak123"/>
    <TargetDevice deviceUuid="Mazak456"/>
    <TargetGroup groupId="MyRobots">
      <TargetDevice deviceUuid="UR123"/>
      <TargetDevice deviceUuid="UR456"/>
    </TargetGroup>
  </Targets>
  <Coordinator>
    <Collaborator collaboratorId="machine" type="CNC">
      <Targets>
        <TargetDevice deviceUuid="Mazak123"/>
        <TargetDevice deviceUuid="Mazak456"/>
      </Targets>
    </Collaborator>
  </Coordinator>
  <Collaborators>
    <Collaborator collaboratorId="Robot" type="ROBOT">
      <Targets>
        <TargetRequirementTable requirementId="ab">
          <Entry key="PAYLOAD">
            <Cell key="maximum">1000</Cell>
          </Entry>
          <Entry key="REACH">
            <Cell key="minimum">1500</Cell>
          </Entry>
        </TargetRequirementTable>
        <TargetRef groupIdRef="MyRobots"/>
      </Targets>
    </Collaborator>
    <Collaborator collaboratorId="robot2" type="ROBOT">
      <Targets>
        <TargetDevice deviceUuid="UR890"/>
      </Targets>
    </Collaborator>
  </Collaborators>
  <SubTaskRefs>
    <SubTaskRef order="1">UnloadConv</SubTaskRef>
    <SubTaskRef order="2">LoadCnc</SubTaskRef>
  </SubTaskRefs>
</TaskArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  entity::JsonEntityPrinter jprinter(2, true);

  auto jdoc = jprinter.print(entity);
  EXPECT_EQ(R"({
  "TaskArchetype": {
    "Collaborators": {
      "Collaborator": [
        {
          "Targets": {
            "TargetRef": [
              {
                "groupIdRef": "MyRobots"
              }
            ],
            "TargetRequirementTable": [
              {
                "value": {
                  "PAYLOAD": {
                    "maximum": 1000
                  },
                  "REACH": {
                    "minimum": 1500
                  }
                },
                "requirementId": "ab"
              }
            ]
          },
          "collaboratorId": "Robot",
          "type": "ROBOT"
        },
        {
          "Targets": {
            "TargetDevice": [
              {
                "deviceUuid": "UR890"
              }
            ]
          },
          "collaboratorId": "robot2",
          "type": "ROBOT"
        }
      ]
    },
    "Coordinator": {
      "Collaborator": {
        "Targets": {
          "TargetDevice": [
            {
              "deviceUuid": "Mazak123"
            },
            {
              "deviceUuid": "Mazak456"
            }
          ]
        },
        "collaboratorId": "machine",
        "type": "CNC"
      }
    },
    "Priority": 10,
    "SubTaskRefs": {
      "SubTaskRef": [
        {
          "value": "UnloadConv",
          "order": 1
        },
        {
          "value": "LoadCnc",
          "order": 2
        }
      ]
    },
    "Targets": {
      "TargetDevice": [
        {
          "deviceUuid": "Mazak123"
        },
        {
          "deviceUuid": "Mazak456"
        }
      ],
      "TargetGroup": [
        {
          "TargetDevice": [
            {
              "deviceUuid": "UR123"
            },
            {
              "deviceUuid": "UR456"
            }
          ],
          "groupId": "MyRobots"
        }
      ]
    },
    "TaskType": "MATERIAL_UNLOAD",
    "assetId": "1aa7eece248093",
    "deviceUuid": "mxi_m001",
    "hash": "fCI1rCQv8BcHbzZeoMxt3kHmb9k=",
    "timestamp": "2024-12-10T05:17:05.531454Z"
  }
})",
            jdoc);
}

TEST_F(TaskAssetTest, task_archetype_must_have_collaborators)
{
  const auto doc =
      R"DOC(<TaskArchetype assetId="1aa7eece248093" deviceUuid="mxi_m001" hash="fCI1rCQv8BcHbzZeoMxt3kHmb9k=" timestamp="2024-12-10T05:17:05.531454Z">
  <TaskType>MATERIAL_UNLOAD</TaskType>
  <Priority>10</Priority>
  <Targets>
    <TargetDevice deviceUuid="Mazak123"/>
    <TargetDevice deviceUuid="Mazak456"/>
    <TargetGroup groupId="MyRobots">
      <TargetDevice deviceUuid="UR123"/>
      <TargetDevice deviceUuid="UR456"/>
    </TargetGroup>
  </Targets>
  <Coordinator>
    <Collaborator collaboratorId="machine" type="CNC">
      <Targets>
        <TargetDevice deviceUuid="Mazak123"/>
        <TargetDevice deviceUuid="Mazak456"/>
      </Targets>
    </Collaborator>
  </Coordinator>
  <SubTaskRefs>
    <SubTaskRef order="1">UnloadConv</SubTaskRef>
    <SubTaskRef order="2">LoadCnc</SubTaskRef>
  </SubTaskRefs>
</TaskArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(1, errors.size());

  EXPECT_EQ("TaskArchetype(Collaborators): Property Collaborators is required and not provided"s,
            errors.front()->what());
}

TEST_F(TaskAssetTest, task_archetype_must_have_collaborators_with_at_least_one_collaborator)
{
  const auto doc =
      R"DOC(<TaskArchetype assetId="1aa7eece248093" deviceUuid="mxi_m001" hash="fCI1rCQv8BcHbzZeoMxt3kHmb9k=" timestamp="2024-12-10T05:17:05.531454Z">
  <TaskType>MATERIAL_UNLOAD</TaskType>
  <Priority>10</Priority>
  <Targets>
    <TargetDevice deviceUuid="Mazak123"/>
    <TargetDevice deviceUuid="Mazak456"/>
    <TargetGroup groupId="MyRobots">
      <TargetDevice deviceUuid="UR123"/>
      <TargetDevice deviceUuid="UR456"/>
    </TargetGroup>
  </Targets>
  <Coordinator>
    <Collaborator collaboratorId="machine" type="CNC">
      <Targets>
        <TargetDevice deviceUuid="Mazak123"/>
        <TargetDevice deviceUuid="Mazak456"/>
      </Targets>
    </Collaborator>
  </Coordinator>
  <Collaborators/>
  <SubTaskRefs>
    <SubTaskRef order="1">UnloadConv</SubTaskRef>
    <SubTaskRef order="2">LoadCnc</SubTaskRef>
  </SubTaskRefs>
</TaskArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(3, errors.size());

  EXPECT_EQ(
      "Collaborators(Collaborator): Entity list requirement Collaborator must have at least 1 entries, 0 found"s,
      errors.front()->what());
}

TEST_F(TaskAssetTest, task_archetype_must_have_a_coordinator)
{
  const auto doc =
      R"DOC(<TaskArchetype assetId="1aa7eece248093" deviceUuid="mxi_m001" hash="fCI1rCQv8BcHbzZeoMxt3kHmb9k=" timestamp="2024-12-10T05:17:05.531454Z">
  <TaskType>MATERIAL_UNLOAD</TaskType>
  <Priority>10</Priority>
  <Targets>
    <TargetDevice deviceUuid="Mazak123"/>
    <TargetDevice deviceUuid="Mazak456"/>
    <TargetGroup groupId="MyRobots">
      <TargetDevice deviceUuid="UR123"/>
      <TargetDevice deviceUuid="UR456"/>
    </TargetGroup>
  </Targets>
  <Collaborators>
    <Collaborator collaboratorId="Robot" type="ROBOT">
      <Targets>
        <TargetRequirementTable requirementId="ab">
          <Entry key="PAYLOAD">
            <Cell key="maximum">1000</Cell>
          </Entry>
          <Entry key="REACH">
            <Cell key="minimum">1500</Cell>
          </Entry>
        </TargetRequirementTable>
        <TargetRef groupIdRef="MyRobots"/>
      </Targets>
    </Collaborator>
    <Collaborator collaboratorId="robot2" type="ROBOT">
      <Targets>
        <TargetDevice deviceUuid="UR890"/>
      </Targets>
    </Collaborator>
  </Collaborators>
  <SubTaskRefs>
    <SubTaskRef order="1">UnloadConv</SubTaskRef>
    <SubTaskRef order="2">LoadCnc</SubTaskRef>
  </SubTaskRefs>
</TaskArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(1, errors.size());

  EXPECT_EQ("TaskArchetype(Coordinator): Property Coordinator is required and not provided"s,
            errors.front()->what());
}

TEST_F(TaskAssetTest, task_archetype_must_have_a_coordinator_with_a_collaborator)
{
  const auto doc =
      R"DOC(<TaskArchetype assetId="1aa7eece248093" deviceUuid="mxi_m001" hash="fCI1rCQv8BcHbzZeoMxt3kHmb9k=" timestamp="2024-12-10T05:17:05.531454Z">
  <TaskType>MATERIAL_UNLOAD</TaskType>
  <Priority>10</Priority>
  <Targets>
    <TargetDevice deviceUuid="Mazak123"/>
    <TargetDevice deviceUuid="Mazak456"/>
    <TargetGroup groupId="MyRobots">
      <TargetDevice deviceUuid="UR123"/>
      <TargetDevice deviceUuid="UR456"/>
    </TargetGroup>
  </Targets>
  <Coordinator>
  </Coordinator>
  <Collaborators>
    <Collaborator collaboratorId="Robot" type="ROBOT">
      <Targets>
        <TargetRequirementTable requirementId="ab">
          <Entry key="PAYLOAD">
            <Cell key="maximum">1000</Cell>
          </Entry>
          <Entry key="REACH">
            <Cell key="minimum">1500</Cell>
          </Entry>
        </TargetRequirementTable>
        <TargetRef groupIdRef="MyRobots"/>
      </Targets>
    </Collaborator>
    <Collaborator collaboratorId="robot2" type="ROBOT">
      <Targets>
        <TargetDevice deviceUuid="UR890"/>
      </Targets>
    </Collaborator>
  </Collaborators>
  <SubTaskRefs>
    <SubTaskRef order="1">UnloadConv</SubTaskRef>
    <SubTaskRef order="2">LoadCnc</SubTaskRef>
  </SubTaskRefs>
</TaskArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(3, errors.size());

  EXPECT_EQ("Coordinator(Collaborator): Property Collaborator is required and not provided"s,
            errors.front()->what());
}

TEST_F(TaskAssetTest, task_archetype_should_have_optional_fields_for_sub_task_refs)
{
  const auto doc =
      R"DOC(<TaskArchetype assetId="1aa7eece248093" deviceUuid="mxi_m001" hash="fCI1rCQv8BcHbzZeoMxt3kHmb9k=" timestamp="2024-12-10T05:17:05.531454Z">
  <TaskType>MATERIAL_UNLOAD</TaskType>
  <Priority>10</Priority>
  <Targets>
    <TargetDevice deviceUuid="Mazak123"/>
    <TargetDevice deviceUuid="Mazak456"/>
    <TargetGroup groupId="MyRobots">
      <TargetDevice deviceUuid="UR123"/>
      <TargetDevice deviceUuid="UR456"/>
    </TargetGroup>
  </Targets>
  <Coordinator>
    <Collaborator collaboratorId="machine" type="CNC">
      <Targets>
        <TargetDevice deviceUuid="Mazak123"/>
        <TargetDevice deviceUuid="Mazak456"/>
      </Targets>
    </Collaborator>
  </Coordinator>
  <Collaborators>
    <Collaborator collaboratorId="Robot" type="ROBOT">
      <Targets>
        <TargetRequirementTable requirementId="ab">
          <Entry key="PAYLOAD">
            <Cell key="maximum">1000</Cell>
          </Entry>
          <Entry key="REACH">
            <Cell key="minimum">1500</Cell>
          </Entry>
        </TargetRequirementTable>
        <TargetRef groupIdRef="MyRobots"/>
      </Targets>
    </Collaborator>
    <Collaborator collaboratorId="robot2" type="ROBOT">
      <Targets>
        <TargetDevice deviceUuid="UR890"/>
      </Targets>
    </Collaborator>
  </Collaborators>
  <SubTaskRefs>
    <SubTaskRef group="g1" optional="false" order="1" parallel="true">UnloadConv</SubTaskRef>
    <SubTaskRef group="g1" optional="true" order="2" parallel="false">LoadCnc</SubTaskRef>
  </SubTaskRefs>
</TaskArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset*>(entity.get());
  ASSERT_NE(nullptr, asset);

  auto subtasks = asset->getList("SubTaskRefs");
  ASSERT_TRUE(subtasks);
  ASSERT_EQ(2, subtasks->size());

  {
    auto it = subtasks->begin();
    EXPECT_EQ("SubTaskRef", (*it)->getName());
    EXPECT_EQ("g1", (*it)->get<string>("group"));
    EXPECT_EQ(1, (*it)->get<int64_t>("order"));
    EXPECT_EQ(false, (*it)->get<bool>("optional"));
    EXPECT_EQ(true, (*it)->get<bool>("parallel"));
    EXPECT_EQ("UnloadConv", (*it)->getValue<string>());

    it++;
    EXPECT_EQ("SubTaskRef", (*it)->getName());
    EXPECT_EQ(2, (*it)->get<int64_t>("order"));
    EXPECT_EQ("g1", (*it)->get<string>("group"));
    EXPECT_EQ(true, (*it)->get<bool>("optional"));
    EXPECT_EQ(false, (*it)->get<bool>("parallel"));
    EXPECT_EQ("LoadCnc", (*it)->getValue<string>());
  }

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}

/// @section Task tests

TEST_F(TaskAssetTest, should_parse_simple_task)
{
  const auto doc =
      R"DOC(<Task assetId="2aa7eece24" deviceUuid="mxi_m001" hash="fCI1rCQv8BcHbzZeoMxt3kHmb9k=" timestamp="2024-12-10T05:17:05.531454Z">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="1aa7eece248093" assetType="TASK_ARCHETYPE" id="A" type="PEER"/>
    </Relationships>
  </Configuration>
  <TaskType>MATERIAL_UNLOAD</TaskType>
  <TaskState>COMMITTED</TaskState>
  <ParentTaskAssetId>dfgfdghfkj</ParentTaskAssetId>
  <Coordinator>
    <Collaborator collaboratorDeviceUuid="xyz" collaboratorId="machine"/>
  </Coordinator>
  <Collaborators>
    <Collaborator collaboratorDeviceUuid="abc" collaboratorId="robot1" requirementId="ab"/>
    <Collaborator collaboratorDeviceUuid="Mazak123" collaboratorId="robot2"/>
  </Collaborators>
</Task>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset*>(entity.get());
  ASSERT_NE(nullptr, asset);

  EXPECT_EQ("Task", asset->getName());
  EXPECT_EQ("2aa7eece24", asset->getAssetId());
  EXPECT_EQ("mxi_m001", asset->get<string>("deviceUuid"));

  auto configuration = asset->get<EntityPtr>("Configuration");
  ASSERT_TRUE(configuration);

  auto relationships = configuration->getList("Relationships");
  ASSERT_TRUE(relationships);
  ASSERT_EQ(1, relationships->size());

  {
    auto it = relationships->begin();
    ASSERT_EQ("A", (*it)->get<string>("id"));
    ASSERT_EQ("1aa7eece248093", (*it)->get<string>("assetIdRef"));
    ASSERT_EQ("PEER", (*it)->get<string>("type"));
    ASSERT_EQ("TASK_ARCHETYPE", (*it)->get<string>("assetType"));
  }

  auto coordinator = asset->get<EntityPtr>("Coordinator");
  ASSERT_TRUE(coordinator);

  auto collab = coordinator->get<EntityPtr>("Collaborator");
  ASSERT_TRUE(collab);
  EXPECT_EQ("machine", collab->get<string>("collaboratorId"));
  EXPECT_EQ("xyz", collab->get<string>("collaboratorDeviceUuid"));

  auto collaborators = asset->getList("Collaborators");
  ASSERT_TRUE(collaborators);

  {
    auto it = collaborators->begin();
    auto collab1 = *it;
    EXPECT_EQ("robot1", collab1->get<string>("collaboratorId"));
    EXPECT_EQ("abc", collab1->get<string>("collaboratorDeviceUuid"));
    EXPECT_EQ("ab", collab1->get<string>("requirementId"));

    it++;
    auto collab2 = *it;
    EXPECT_EQ("robot2", collab2->get<string>("collaboratorId"));
    EXPECT_EQ("Mazak123", collab2->get<string>("collaboratorDeviceUuid"));
  }

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}

TEST_F(TaskAssetTest, should_parse_simple_task_with_subtasks)
{
  const auto doc =
      R"DOC(<Task assetId="2aa7eece24" deviceUuid="mxi_m001" hash="fCI1rCQv8BcHbzZeoMxt3kHmb9k=" timestamp="2024-12-10T05:17:05.531454Z">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="1aa7eece248093" assetType="TASK_ARCHETYPE" id="A" type="PEER"/>
    </Relationships>
  </Configuration>
  <TaskType>MATERIAL_UNLOAD</TaskType>
  <TaskState>COMMITTED</TaskState>
  <ParentTaskAssetId>dfgfdghfkj</ParentTaskAssetId>
  <Coordinator>
    <Collaborator collaboratorDeviceUuid="CNC" collaboratorId="machine"/>
  </Coordinator>
  <Collaborators>
    <Collaborator collaboratorDeviceUuid="UR543" collaboratorId="robot1" requirementId="ab"/>
    <Collaborator collaboratorDeviceUuid="UR012" collaboratorId="robot2"/>
  </Collaborators>
  <SubTasks>
    <Task assetId="4afb7fc0" deviceUuid="mxi_m001" timestamp="2024-12-10T05:17:05.531454Z">
      <TaskType>OPEN_DOOR</TaskType>
      <TaskState>COMMITTED</TaskState>
      <ParentTaskAssetId>2aa7eece24</ParentTaskAssetId>
      <Coordinator>
        <Collaborator collaboratorDeviceUuid="UR012" collaboratorId="robot1"/>
      </Coordinator>
      <Collaborators>
        <Collaborator collaboratorDeviceUuid="CNC" collaboratorId="machine" requirementId="ab"/>
        <Collaborator collaboratorDeviceUuid="UR543" collaboratorId="robot2"/>
      </Collaborators>
    </Task>
    <Task assetId="a9ef8c40" deviceUuid="mxi_m001" timestamp="2024-12-10T05:17:05.531454Z">
      <TaskType>OPEN_CHUCK</TaskType>
      <TaskState>COMMITTED</TaskState>
      <ParentTaskAssetId>2aa7eece24</ParentTaskAssetId>
      <Coordinator>
        <Collaborator collaboratorDeviceUuid="CNC" collaboratorId="machine"/>
      </Coordinator>
      <Collaborators>
        <Collaborator collaboratorDeviceUuid="UR543" collaboratorId="robot2" requirementId="ab"/>
      </Collaborators>
    </Task>
  </SubTasks>
</Task>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset*>(entity.get());
  ASSERT_NE(nullptr, asset);

  auto subtasks = asset->getList("SubTasks");
  ASSERT_TRUE(subtasks);
  ASSERT_EQ(2, subtasks->size());

  auto it = subtasks->begin();
  {
    auto task = *it;
    EXPECT_EQ("Task", task->getName());
    EXPECT_EQ("4afb7fc0", task->get<string>("assetId"));
    EXPECT_EQ("mxi_m001", task->get<string>("deviceUuid"));
    EXPECT_EQ("OPEN_DOOR", task->get<string>("TaskType"));
    EXPECT_EQ("COMMITTED", task->get<string>("TaskState"));
    EXPECT_EQ("2aa7eece24", task->get<string>("ParentTaskAssetId"));

    auto coordinator = task->get<EntityPtr>("Coordinator");
    ASSERT_TRUE(coordinator);

    auto collab = coordinator->get<EntityPtr>("Collaborator");
    ASSERT_TRUE(collab);

    EXPECT_EQ("robot1", collab->get<string>("collaboratorId"));
    EXPECT_EQ("UR012", collab->get<string>("collaboratorDeviceUuid"));

    auto collaborators = task->getList("Collaborators");
    EXPECT_TRUE(collaborators);
    EXPECT_EQ(2, collaborators->size());
    {
      auto cit = collaborators->begin();
      EXPECT_EQ("machine", (*cit)->get<string>("collaboratorId"));
      EXPECT_EQ("CNC", (*cit)->get<string>("collaboratorDeviceUuid"));
      EXPECT_EQ("ab", (*cit)->get<string>("requirementId"));

      cit++;
      EXPECT_EQ("robot2", (*cit)->get<string>("collaboratorId"));
      EXPECT_EQ("UR543", (*cit)->get<string>("collaboratorDeviceUuid"));
    }
  }

  it++;
  {
    auto task = *it;
    EXPECT_EQ("Task", task->getName());
    EXPECT_EQ("a9ef8c40", task->get<string>("assetId"));
    EXPECT_EQ("mxi_m001", task->get<string>("deviceUuid"));
    EXPECT_EQ("OPEN_CHUCK", task->get<string>("TaskType"));
    EXPECT_EQ("COMMITTED", task->get<string>("TaskState"));
    EXPECT_EQ("2aa7eece24", task->get<string>("ParentTaskAssetId"));

    auto coordinator = task->get<EntityPtr>("Coordinator");
    ASSERT_TRUE(coordinator);

    auto collab = coordinator->get<EntityPtr>("Collaborator");
    ASSERT_TRUE(collab);

    EXPECT_EQ("machine", collab->get<string>("collaboratorId"));
    EXPECT_EQ("CNC", collab->get<string>("collaboratorDeviceUuid"));

    auto collaborators = task->getList("Collaborators");
    EXPECT_TRUE(collaborators);
    EXPECT_EQ(1, collaborators->size());
    {
      auto cit = collaborators->begin();
      EXPECT_EQ("robot2", (*cit)->get<string>("collaboratorId"));
      EXPECT_EQ("UR543", (*cit)->get<string>("collaboratorDeviceUuid"));
      EXPECT_EQ("ab", (*cit)->get<string>("requirementId"));
    }
  }

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}

TEST_F(TaskAssetTest, task_should_produce_json)
{
  const auto doc =
      R"DOC(<Task assetId="2aa7eece24" deviceUuid="mxi_m001" hash="fCI1rCQv8BcHbzZeoMxt3kHmb9k=" timestamp="2024-12-10T05:17:05.531454Z">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="1aa7eece248093" assetType="TASK_ARCHETYPE" id="A" type="PEER"/>
    </Relationships>
  </Configuration>
  <TaskType>MATERIAL_UNLOAD</TaskType>
  <TaskState>COMMITTED</TaskState>
  <ParentTaskAssetId>dfgfdghfkj</ParentTaskAssetId>
  <Coordinator>
    <Collaborator collaboratorDeviceUuid="CNC" collaboratorId="machine"/>
  </Coordinator>
  <Collaborators>
    <Collaborator collaboratorDeviceUuid="UR543" collaboratorId="robot1" requirementId="ab"/>
    <Collaborator collaboratorDeviceUuid="UR012" collaboratorId="robot2"/>
  </Collaborators>
  <SubTasks>
    <Task assetId="4afb7fc0" deviceUuid="mxi_m001" timestamp="2024-12-10T05:17:05.531454Z">
      <TaskType>OPEN_DOOR</TaskType>
      <TaskState>COMMITTED</TaskState>
      <ParentTaskAssetId>2aa7eece24</ParentTaskAssetId>
      <Coordinator>
        <Collaborator collaboratorDeviceUuid="UR012" collaboratorId="robot1"/>
      </Coordinator>
      <Collaborators>
        <Collaborator collaboratorDeviceUuid="CNC" collaboratorId="machine" requirementId="ab"/>
        <Collaborator collaboratorDeviceUuid="UR543" collaboratorId="robot2"/>
      </Collaborators>
    </Task>
    <Task assetId="a9ef8c40" deviceUuid="mxi_m001" timestamp="2024-12-10T05:17:05.531454Z">
      <TaskType>OPEN_CHUCK</TaskType>
      <TaskState>COMMITTED</TaskState>
      <ParentTaskAssetId>2aa7eece24</ParentTaskAssetId>
      <Coordinator>
        <Collaborator collaboratorDeviceUuid="CNC" collaboratorId="machine"/>
      </Coordinator>
      <Collaborators>
        <Collaborator collaboratorDeviceUuid="UR543" collaboratorId="robot2" requirementId="ab"/>
      </Collaborators>
    </Task>
  </SubTasks>
</Task>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  entity::JsonEntityPrinter jprinter(2, true);

  auto jdoc = jprinter.print(entity);
  EXPECT_EQ(R"({
  "Task": {
    "Collaborators": {
      "Collaborator": [
        {
          "collaboratorDeviceUuid": "UR543",
          "collaboratorId": "robot1",
          "requirementId": "ab"
        },
        {
          "collaboratorDeviceUuid": "UR012",
          "collaboratorId": "robot2"
        }
      ]
    },
    "Configuration": {
      "Relationships": {
        "AssetRelationship": [
          {
            "assetIdRef": "1aa7eece248093",
            "assetType": "TASK_ARCHETYPE",
            "id": "A",
            "type": "PEER"
          }
        ]
      }
    },
    "Coordinator": {
      "Collaborator": {
        "collaboratorDeviceUuid": "CNC",
        "collaboratorId": "machine"
      }
    },
    "ParentTaskAssetId": "dfgfdghfkj",
    "SubTasks": {
      "Task": [
        {
          "Collaborators": {
            "Collaborator": [
              {
                "collaboratorDeviceUuid": "CNC",
                "collaboratorId": "machine",
                "requirementId": "ab"
              },
              {
                "collaboratorDeviceUuid": "UR543",
                "collaboratorId": "robot2"
              }
            ]
          },
          "Coordinator": {
            "Collaborator": {
              "collaboratorDeviceUuid": "UR012",
              "collaboratorId": "robot1"
            }
          },
          "ParentTaskAssetId": "2aa7eece24",
          "TaskState": "COMMITTED",
          "TaskType": "OPEN_DOOR",
          "assetId": "4afb7fc0",
          "deviceUuid": "mxi_m001",
          "timestamp": "2024-12-10T05:17:05.531454Z"
        },
        {
          "Collaborators": {
            "Collaborator": [
              {
                "collaboratorDeviceUuid": "UR543",
                "collaboratorId": "robot2",
                "requirementId": "ab"
              }
            ]
          },
          "Coordinator": {
            "Collaborator": {
              "collaboratorDeviceUuid": "CNC",
              "collaboratorId": "machine"
            }
          },
          "ParentTaskAssetId": "2aa7eece24",
          "TaskState": "COMMITTED",
          "TaskType": "OPEN_CHUCK",
          "assetId": "a9ef8c40",
          "deviceUuid": "mxi_m001",
          "timestamp": "2024-12-10T05:17:05.531454Z"
        }
      ]
    },
    "TaskState": "COMMITTED",
    "TaskType": "MATERIAL_UNLOAD",
    "assetId": "2aa7eece24",
    "deviceUuid": "mxi_m001",
    "hash": "fCI1rCQv8BcHbzZeoMxt3kHmb9k=",
    "timestamp": "2024-12-10T05:17:05.531454Z"
  }
})",
            jdoc);
}

TEST_F(TaskAssetTest, task_must_have_a_coordinator)
{
  const auto doc =
      R"DOC(<Task assetId="2aa7eece24" deviceUuid="mxi_m001" hash="fCI1rCQv8BcHbzZeoMxt3kHmb9k=" timestamp="2024-12-10T05:17:05.531454Z">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="1aa7eece248093" assetType="TASK_ARCHETYPE" id="A" type="PEER"/>
    </Relationships>
  </Configuration>
  <TaskType>MATERIAL_UNLOAD</TaskType>
  <TaskState>COMMITTED</TaskState>
  <ParentTaskAssetId>dfgfdghfkj</ParentTaskAssetId>
  <Collaborators>
    <Collaborator collaboratorDeviceUuid="abc" collaboratorId="robot1" requirementId="ab"/>
    <Collaborator collaboratorDeviceUuid="Mazak123" collaboratorId="robot2"/>
  </Collaborators>
</Task>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(1, errors.size());

  EXPECT_EQ("Task(Coordinator): Property Coordinator is required and not provided"s,
            errors.front()->what());
}

TEST_F(TaskAssetTest, task_must_have_a_coordinator_with_a_collaborator)
{
  const auto doc =
      R"DOC(<Task assetId="2aa7eece24" deviceUuid="mxi_m001" hash="fCI1rCQv8BcHbzZeoMxt3kHmb9k=" timestamp="2024-12-10T05:17:05.531454Z">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="1aa7eece248093" assetType="TASK_ARCHETYPE" id="A" type="PEER"/>
    </Relationships>
  </Configuration>
  <TaskType>MATERIAL_UNLOAD</TaskType>
  <TaskState>COMMITTED</TaskState>
  <ParentTaskAssetId>dfgfdghfkj</ParentTaskAssetId>
  <Coordinator/>
  <Collaborators>
    <Collaborator collaboratorDeviceUuid="abc" collaboratorId="robot1" requirementId="ab"/>
    <Collaborator collaboratorDeviceUuid="Mazak123" collaboratorId="robot2"/>
  </Collaborators>
</Task>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(3, errors.size());

  EXPECT_EQ("Coordinator(Collaborator): Property Collaborator is required and not provided"s,
            errors.front()->what());
}

TEST_F(TaskAssetTest, task_must_have_collaborators)
{
  const auto doc =
      R"DOC(<Task assetId="2aa7eece24" deviceUuid="mxi_m001" hash="fCI1rCQv8BcHbzZeoMxt3kHmb9k=" timestamp="2024-12-10T05:17:05.531454Z">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="1aa7eece248093" assetType="TASK_ARCHETYPE" id="A" type="PEER"/>
    </Relationships>
  </Configuration>
  <TaskType>MATERIAL_UNLOAD</TaskType>
  <TaskState>COMMITTED</TaskState>
  <ParentTaskAssetId>dfgfdghfkj</ParentTaskAssetId>
  <Coordinator>
    <Collaborator collaboratorDeviceUuid="xyz" collaboratorId="machine"/>
  </Coordinator>
</Task>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(1, errors.size());

  EXPECT_EQ("Task(Collaborators): Property Collaborators is required and not provided"s,
            errors.front()->what());
}

TEST_F(TaskAssetTest, task_must_have_collaborators_with_at_least_one_collaborator)
{
  const auto doc =
      R"DOC(<Task assetId="2aa7eece24" deviceUuid="mxi_m001" hash="fCI1rCQv8BcHbzZeoMxt3kHmb9k=" timestamp="2024-12-10T05:17:05.531454Z">
<Configuration>
  <Relationships>
    <AssetRelationship assetIdRef="1aa7eece248093" assetType="TASK_ARCHETYPE" id="A" type="PEER"/>
  </Relationships>
</Configuration>
<TaskType>MATERIAL_UNLOAD</TaskType>
<TaskState>COMMITTED</TaskState>
<ParentTaskAssetId>dfgfdghfkj</ParentTaskAssetId>
<Coordinator>
  <Collaborator collaboratorDeviceUuid="xyz" collaboratorId="machine"/>
</Coordinator>
<Collaborators/>
</Task>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(3, errors.size());

  EXPECT_EQ(
      "Collaborators(Collaborator): Entity list requirement Collaborator must have at least 1 entries, 0 found"s,
      errors.front()->what());
}

TEST_F(TaskAssetTest, task_must_have_a_task_state)
{
  constexpr auto doc =
      R"DOC(<Task assetId="2aa7eece24" deviceUuid="mxi_m001" hash="fCI1rCQv8BcHbzZeoMxt3kHmb9k=" timestamp="2024-12-10T05:17:05.531454Z">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="1aa7eece248093" assetType="TASK_ARCHETYPE" id="A" type="PEER"/>
    </Relationships>
  </Configuration>
  <TaskType>MATERIAL_UNLOAD</TaskType>
  <ParentTaskAssetId>dfgfdghfkj</ParentTaskAssetId>
  <Coordinator>
    <Collaborator collaboratorDeviceUuid="xyz" collaboratorId="machine"/>
  </Coordinator>
  <Collaborators>
    <Collaborator collaboratorDeviceUuid="Mazak123" collaboratorId="robot2"/>
  </Collaborators>
</Task>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(1, errors.size());

  EXPECT_EQ("Task(TaskState): Property TaskState is required and not provided"s,
            errors.front()->what());
}

TEST_F(TaskAssetTest, task_must_have_a_task_type)
{
  constexpr auto doc =
      R"DOC(<Task assetId="2aa7eece24" deviceUuid="mxi_m001" hash="fCI1rCQv8BcHbzZeoMxt3kHmb9k=" timestamp="2024-12-10T05:17:05.531454Z">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="1aa7eece248093" assetType="TASK_ARCHETYPE" id="A" type="PEER"/>
    </Relationships>
  </Configuration>
  <TaskState>COMMITTED</TaskState>
  <ParentTaskAssetId>dfgfdghfkj</ParentTaskAssetId>
  <Coordinator>
    <Collaborator collaboratorDeviceUuid="xyz" collaboratorId="machine"/>
  </Coordinator>
  <Collaborators>
    <Collaborator collaboratorDeviceUuid="Mazak123" collaboratorId="robot2"/>
  </Collaborators>
</Task>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(1, errors.size());

  EXPECT_EQ("Task(TaskType): Property TaskType is required and not provided"s,
            errors.front()->what());
}

TEST_F(TaskAssetTest, task_should_accept_all_task_states)
{
  constexpr auto doc =
      R"DOC(<Task assetId="2aa7eece24" deviceUuid="mxi_m001" hash="fCI1rCQv8BcHbzZeoMxt3kHmb9k=" timestamp="2024-12-10T05:17:05.531454Z">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="1aa7eece248093" assetType="TASK_ARCHETYPE" id="A" type="PEER"/>
    </Relationships>
  </Configuration>
  <TaskType>MATERIAL_UNLOAD</TaskType>
  <TaskState>{}</TaskState>
  <ParentTaskAssetId>dfgfdghfkj</ParentTaskAssetId>
  <Coordinator>
    <Collaborator collaboratorDeviceUuid="xyz" collaboratorId="machine"/>
  </Coordinator>
  <Collaborators>
    <Collaborator collaboratorDeviceUuid="Mazak123" collaboratorId="robot2"/>
  </Collaborators>
</Task>
)DOC";

  list<string> states {"INACTIVE", "PREPARING", "COMMITTING", "COMMITTED", "COMPLETE", "FAIL"};

  for (const auto& state : states)
  {
    ErrorList errors;
    entity::XmlParser parser;

    auto sd = format(doc, state);
    auto entity = parser.parse(Asset::getRoot(), sd, errors);
    EXPECT_EQ(0, errors.size()) << "should accept task state: " << state;
  }
}

TEST_F(TaskAssetTest, task_should_not_accept_invalid_task_states)
{
  constexpr auto doc =
      R"DOC(<Task assetId="2aa7eece24" deviceUuid="mxi_m001" hash="fCI1rCQv8BcHbzZeoMxt3kHmb9k=" timestamp="2024-12-10T05:17:05.531454Z">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="1aa7eece248093" assetType="TASK_ARCHETYPE" id="A" type="PEER"/>
    </Relationships>
  </Configuration>
  <TaskType>MATERIAL_UNLOAD</TaskType>
  <TaskState>{}</TaskState>
  <ParentTaskAssetId>dfgfdghfkj</ParentTaskAssetId>
  <Coordinator>
    <Collaborator collaboratorDeviceUuid="xyz" collaboratorId="machine"/>
  </Coordinator>
  <Collaborators>
    <Collaborator collaboratorDeviceUuid="Mazak123" collaboratorId="robot2"/>
  </Collaborators>
</Task>
)DOC";

  list<string> states {"BAD", "STATE", "123", "DONE", ""};

  for (const auto& state : states)
  {
    ErrorList errors;
    entity::XmlParser parser;

    auto sd = format(doc, state);
    auto entity = parser.parse(Asset::getRoot(), sd, errors);
    EXPECT_EQ(1, errors.size()) << "Should fail task state: " << state;
  }
}
