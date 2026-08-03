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
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/asset/asset.hpp"
#include "mtconnect/asset/process.hpp"
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
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class ProcessAssetTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    ProcessArchetype::registerAsset();
    Process::registerAsset();
    m_writer = make_unique<XmlWriter>(true);
  }

  void TearDown() override { m_writer.reset(); }

  std::unique_ptr<XmlWriter> m_writer;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

TEST_F(ProcessAssetTest, should_parse_a_process_archetype)
{
  const auto doc =
      R"DOC(<ProcessArchetype assetId="PROCESS_ARCH_ID" revision="1">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="PART_ID" assetType="PART_ARCHETYPE" id="reference_id" type="PEER"/>
    </Relationships>
  </Configuration>
  <Routings>
    <Routing precedence="1" routingId="routng1">
      <ProcessStep stepId="10">
        <Description>Process Step 10</Description>
        <StartTime>2025-11-24T00:00:00Z</StartTime>
        <Duration>23000</Duration>
        <Targets>
          <TargetRef groupIdRef="group1"/>
        </Targets>
        <ActivityGroups>
          <ActivityGroup activityGroupId="act1">
            <Activity activityId="a1" sequence="1">
              <Description>First Activity</Description>
            </Activity>
          </ActivityGroup>
        </ActivityGroups>
      </ProcessStep>
    </Routing>
  </Routings>
  <Targets>
    <TargetDevice deviceUuid="device1"/>
    <TargetGroup groupId="group1">
      <TargetDevice deviceUuid="device2"/>
      <TargetDevice deviceUuid="device3"/>
    </TargetGroup>
  </Targets>
</ProcessArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset *>(entity.get());
  ASSERT_NE(nullptr, asset);

  ASSERT_EQ("ProcessArchetype", asset->getName());
  ASSERT_EQ("PROCESS_ARCH_ID", asset->getAssetId());
  ASSERT_EQ("1", asset->get<string>("revision"));

  auto configuration = asset->get<EntityPtr>("Configuration");
  ASSERT_TRUE(configuration);

  auto relationships = configuration->getList("Relationships");
  ASSERT_TRUE(relationships);
  ASSERT_EQ(1, relationships->size());

  {
    auto it = relationships->begin();
    ASSERT_EQ("reference_id", (*it)->get<string>("id"));
    ASSERT_EQ("PART_ID", (*it)->get<string>("assetIdRef"));
    ASSERT_EQ("PEER", (*it)->get<string>("type"));
    ASSERT_EQ("PART_ARCHETYPE", (*it)->get<string>("assetType"));
  }

  auto routings = asset->getList("Routings");
  ASSERT_TRUE(routings);
  ASSERT_EQ(1, routings->size());

  {
    auto routing = routings->front();
    ASSERT_EQ("routng1", routing->get<string>("routingId"));
    ASSERT_EQ(1, routing->get<int64_t>("precedence"));

    auto processSteps = routing->get<EntityList>("ProcessStep");
    ASSERT_EQ(1, processSteps.size());

    auto step = processSteps.front();
    ASSERT_EQ("10", step->get<string>("stepId"));
    ASSERT_EQ("Process Step 10", step->get<string>("Description"));

    auto st = step->get<Timestamp>("StartTime");
    ASSERT_EQ("2025-11-24T00:00:00Z", getCurrentTime(st, GMT));
    ASSERT_EQ(23000, step->get<double>("Duration"));

    auto targets = step->getList("Targets");
    ASSERT_TRUE(targets);
    ASSERT_EQ(1, targets->size());

    {
      auto it = targets->begin();
      ASSERT_EQ("group1", (*it)->get<string>("groupIdRef"));
    }

    auto activityGroups = step->getList("ActivityGroups");
    ASSERT_TRUE(activityGroups);
    ASSERT_EQ(1, activityGroups->size());

    {
      auto it = activityGroups->begin();
      auto activityGroup = *it;
      ASSERT_EQ("act1", activityGroup->get<string>("activityGroupId"));

      auto activities = activityGroup->get<EntityList>("Activity");
      ASSERT_EQ(1, activities.size());

      auto activity = activities.front();
      ASSERT_EQ("a1", activity->get<string>("activityId"));
      ASSERT_EQ(1, activity->get<int64_t>("sequence"));
      ASSERT_EQ("First Activity", activity->get<string>("Description"));
    }
  }

  auto targets = asset->getList("Targets");
  ASSERT_TRUE(targets);
  ASSERT_EQ(2, targets->size());

  {
    auto it = targets->begin();
    ASSERT_EQ("TargetDevice", (*it)->getName());
    ASSERT_EQ("device1", (*it)->get<string>("deviceUuid"));

    it++;
    auto targetGroup = *it;
    ASSERT_EQ("TargetGroup", targetGroup->getName());
    ASSERT_EQ("group1", targetGroup->get<string>("groupId"));

    auto targetDevices = targetGroup->get<EntityList>("LIST");
    ASSERT_EQ(2, targetDevices.size());

    {
      auto dit = targetDevices.begin();
      ASSERT_EQ("TargetDevice", (*dit)->getName());
      ASSERT_EQ("device2", (*dit)->get<string>("deviceUuid"));
      dit++;
      ASSERT_EQ("TargetDevice", (*dit)->getName());
      ASSERT_EQ("device3", (*dit)->get<string>("deviceUuid"));
    }
  }

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}

TEST_F(ProcessAssetTest, process_archetype_can_have_multiple_routings)
{
  const auto doc =
      R"DOC(<ProcessArchetype assetId="PROCESS_ARCH_ID" revision="1">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="PART_ID" assetType="PART_ARCHETYPE" id="reference_id" type="PEER"/>
    </Relationships>
  </Configuration>
  <Routings>
    <Routing precedence="1" routingId="routng1">
      <ProcessStep stepId="10">
        <Description>Process Step 10</Description>
        <StartTime>2025-11-24T00:00:00Z</StartTime>
        <Duration>23000</Duration>
      </ProcessStep>
    </Routing>
    <Routing precedence="2" routingId="routng2">
      <ProcessStep stepId="11">
        <Description>Process Step 11</Description>
        <StartTime>2025-11-25T00:00:00Z</StartTime>
        <Duration>20000</Duration>
      </ProcessStep>
    </Routing>
  </Routings>
  <Targets>
    <TargetDevice deviceUuid="device1"/>
    <TargetGroup groupId="group1">
      <TargetDevice deviceUuid="device2"/>
      <TargetDevice deviceUuid="device3"/>
    </TargetGroup>
  </Targets>
</ProcessArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset *>(entity.get());
  ASSERT_NE(nullptr, asset);

  auto routings = asset->getList("Routings");
  ASSERT_TRUE(routings);
  ASSERT_EQ(2, routings->size());

  auto it = routings->begin();
  {
    auto routing = *it;
    ASSERT_EQ("routng1", routing->get<string>("routingId"));
    ASSERT_EQ(1, routing->get<int64_t>("precedence"));

    auto processSteps = routing->get<EntityList>("ProcessStep");
    ASSERT_EQ(1, processSteps.size());

    auto step = processSteps.front();
    ASSERT_EQ("10", step->get<string>("stepId"));
    ASSERT_EQ("Process Step 10", step->get<string>("Description"));

    auto st = step->get<Timestamp>("StartTime");
    ASSERT_EQ("2025-11-24T00:00:00Z", getCurrentTime(st, GMT));
    ASSERT_EQ(23000, step->get<double>("Duration"));
  }
  it++;
  {
    auto routing = *it;
    ASSERT_EQ("routng2", routing->get<string>("routingId"));
    ASSERT_EQ(2, routing->get<int64_t>("precedence"));

    auto processSteps = routing->get<EntityList>("ProcessStep");
    ASSERT_EQ(1, processSteps.size());

    auto step = processSteps.front();
    ASSERT_EQ("11", step->get<string>("stepId"));
    ASSERT_EQ("Process Step 11", step->get<string>("Description"));

    auto st = step->get<Timestamp>("StartTime");
    ASSERT_EQ("2025-11-25T00:00:00Z", getCurrentTime(st, GMT));
    ASSERT_EQ(20000, step->get<double>("Duration"));
  }

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}

TEST_F(ProcessAssetTest, process_steps_can_be_optional)
{
  const auto doc =
      R"DOC(<ProcessArchetype assetId="PROCESS_ARCH_ID" revision="1">
  <Routings>
    <Routing precedence="1" routingId="routng1">
      <ProcessStep optional="true" sequence="5" stepId="10">
        <Description>Process Step 10</Description>
        <StartTime>2025-11-24T00:00:00Z</StartTime>
        <Duration>23000</Duration>
      </ProcessStep>
    </Routing>
  </Routings>
</ProcessArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset *>(entity.get());
  ASSERT_NE(nullptr, asset);

  auto routings = asset->getList("Routings");
  ASSERT_TRUE(routings);
  ASSERT_EQ(1, routings->size());

  auto routing = routings->front();
  ASSERT_EQ("routng1", routing->get<string>("routingId"));

  auto processSteps = routing->get<EntityList>("ProcessStep");
  ASSERT_EQ(1, processSteps.size());
  auto step = processSteps.front();

  ASSERT_EQ("10", step->get<string>("stepId"));
  ASSERT_EQ(5, step->get<int64_t>("sequence"));
  ASSERT_EQ(true, step->get<bool>("optional"));

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}

TEST_F(ProcessAssetTest, process_archetype_must_have_at_least_one_routing)
{
  const auto doc =
      R"DOC(<ProcessArchetype assetId="PROCESS_ARCH_ID" revision="1">
  <Targets>
    <TargetDevice deviceUuid="device1"/>
    <TargetGroup groupId="group1">
      <TargetDevice deviceUuid="device2"/>
      <TargetDevice deviceUuid="device3"/>
    </TargetGroup>
  </Targets>
</ProcessArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(1, errors.size());
  auto error = dynamic_cast<PropertyError *>(errors.front().get());

  ASSERT_EQ("ProcessArchetype(Routings): Property Routings is required and not provided"s,
            error->what());
  ASSERT_EQ("ProcessArchetype", error->getEntity());
  ASSERT_EQ("Routings", error->getProperty());
}

TEST_F(ProcessAssetTest, process_archetype_routing_must_have_a_process_step)
{
  const auto doc =
      R"DOC(<ProcessArchetype assetId="PROCESS_ARCH_ID" revision="1">
  <Routings>
    <Routing precedence="1" routingId="routng1">
    </Routing>
  </Routings>
  <Targets>
    <TargetDevice deviceUuid="device1"/>
    <TargetGroup groupId="group1">
      <TargetDevice deviceUuid="device2"/>
      <TargetDevice deviceUuid="device3"/>
    </TargetGroup>
  </Targets>
</ProcessArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(5, errors.size());

  auto it = errors.begin();
  {
    auto error = dynamic_cast<PropertyError *>(it->get());
    ASSERT_TRUE(error);
    EXPECT_EQ("Routing(ProcessStep): Property ProcessStep is required and not provided"s,
              error->what());
    EXPECT_EQ("Routing", error->getEntity());
    EXPECT_EQ("ProcessStep", error->getProperty());
  }

  it++;
  {
    auto error = it->get();
    ASSERT_TRUE(error);
    EXPECT_EQ("Routings: Invalid element 'Routing'"s, error->what());
    EXPECT_EQ("Routings", error->getEntity());
  }

  it++;
  {
    auto error = dynamic_cast<PropertyError *>(it->get());
    ASSERT_TRUE(error);
    EXPECT_EQ(
        "Routings(Routing): Entity list requirement Routing must have at least 1 entries, 0 found"s,
        error->what());
    EXPECT_EQ("Routings", error->getEntity());
    EXPECT_EQ("Routing", error->getProperty());
  }

  it++;
  {
    auto error = it->get();
    ASSERT_TRUE(error);
    EXPECT_EQ("ProcessArchetype: Invalid element 'Routings'"s, error->what());
    EXPECT_EQ("ProcessArchetype", error->getEntity());
  }

  it++;
  {
    auto error = dynamic_cast<PropertyError *>(it->get());
    ASSERT_TRUE(error);
    EXPECT_EQ("ProcessArchetype(Routings): Property Routings is required and not provided"s,
              error->what());
    EXPECT_EQ("ProcessArchetype", error->getEntity());
    EXPECT_EQ("Routings", error->getProperty());
  }
}

TEST_F(ProcessAssetTest, activity_can_have_a_sequence_precedence_and_be_optional)
{
  const auto doc =
      R"DOC(<ProcessArchetype assetId="PROCESS_ARCH_ID" revision="1">
  <Routings>
    <Routing precedence="1" routingId="routng1">
      <ProcessStep optional="true" sequence="5" stepId="10">
        <ActivityGroups>
          <ActivityGroup activityGroupId="act1" name="fred">
            <Activity activityId="a1" optional="true" precedence="3" sequence="2">
              <Description>First Activity</Description>
            </Activity>
          </ActivityGroup>
        </ActivityGroups>
      </ProcessStep>
    </Routing>
  </Routings>
</ProcessArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset *>(entity.get());
  ASSERT_NE(nullptr, asset);

  auto routings = asset->getList("Routings");
  ASSERT_TRUE(routings);
  ASSERT_EQ(1, routings->size());

  auto routing = routings->front();
  ASSERT_EQ("routng1", routing->get<string>("routingId"));

  auto processSteps = routing->get<EntityList>("ProcessStep");
  ASSERT_EQ(1, processSteps.size());
  auto step = processSteps.front();

  auto activityGroups = step->getList("ActivityGroups");
  ASSERT_TRUE(activityGroups);
  ASSERT_EQ(1, activityGroups->size());

  auto activityGroup = activityGroups->front();
  ASSERT_EQ("act1", activityGroup->get<string>("activityGroupId"));
  ASSERT_EQ("fred", activityGroup->get<string>("name"));

  auto activities = activityGroup->get<EntityList>("Activity");
  ASSERT_EQ(1, activities.size());

  auto activity = activities.front();
  ASSERT_EQ("a1", activity->get<string>("activityId"));
  ASSERT_EQ(2, activity->get<int64_t>("sequence"));
  ASSERT_EQ("First Activity", activity->get<string>("Description"));
  ASSERT_EQ(true, activity->get<bool>("optional"));
  ASSERT_EQ(3, activity->get<int64_t>("precedence"));

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}

TEST_F(ProcessAssetTest, should_generate_json)
{
  const auto doc =
      R"DOC(<ProcessArchetype assetId="PROCESS_ARCH_ID" revision="1">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="PART_ID" assetType="PART_ARCHETYPE" id="reference_id" type="PEER"/>
    </Relationships>
  </Configuration>
  <Routings>
    <Routing precedence="1" routingId="routng1">
      <ProcessStep stepId="10">
        <Description>Process Step 10</Description>
        <StartTime>2025-11-24T00:00:00Z</StartTime>
        <Duration>23000</Duration>
        <Targets>
          <TargetRef groupIdRef="group1"/>
        </Targets>
        <ActivityGroups>
          <ActivityGroup activityGroupId="act1" name="fred">
            <Activity activityId="a1" sequence="1" optional="true" precedence="2">
              <Description>First Activity</Description>
            </Activity>
          </ActivityGroup>
        </ActivityGroups>
      </ProcessStep>
    </Routing>
  </Routings>
  <Targets>
    <TargetDevice deviceUuid="device1"/>
    <TargetGroup groupId="group1">
      <TargetDevice deviceUuid="device2"/>
      <TargetDevice deviceUuid="device3"/>
    </TargetGroup>
  </Targets>
</ProcessArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  entity::JsonEntityPrinter jprinter(2, true);

  auto sdoc = jprinter.print(entity);
  EXPECT_EQ(R"({
  "ProcessArchetype": {
    "Configuration": {
      "Relationships": {
        "AssetRelationship": [
          {
            "assetIdRef": "PART_ID",
            "assetType": "PART_ARCHETYPE",
            "id": "reference_id",
            "type": "PEER"
          }
        ]
      }
    },
    "Routings": {
      "Routing": [
        {
          "ProcessStep": [
            {
              "ActivityGroups": {
                "ActivityGroup": [
                  {
                    "Activity": [
                      {
                        "Description": "First Activity",
                        "activityId": "a1",
                        "optional": true,
                        "precedence": 2,
                        "sequence": 1
                      }
                    ],
                    "activityGroupId": "act1",
                    "name": "fred"
                  }
                ]
              },
              "Description": "Process Step 10",
              "Duration": 23000.0,
              "StartTime": "2025-11-24T00:00:00Z",
              "Targets": {
                "TargetRef": [
                  {
                    "groupIdRef": "group1"
                  }
                ]
              },
              "stepId": "10"
            }
          ],
          "precedence": 1,
          "routingId": "routng1"
        }
      ]
    },
    "Targets": {
      "TargetDevice": [
        {
          "deviceUuid": "device1"
        }
      ],
      "TargetGroup": [
        {
          "TargetDevice": [
            {
              "deviceUuid": "device2"
            },
            {
              "deviceUuid": "device3"
            }
          ],
          "groupId": "group1"
        }
      ]
    },
    "assetId": "PROCESS_ARCH_ID",
    "revision": "1"
  }
})",
            sdoc);
}

TEST_F(ProcessAssetTest, should_parse_and_generate_a_process)
{
  const auto doc =
      R"DOC(<Process assetId="PROCESS_ARCH_ID" revision="1">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="PART_ID" assetType="PART_ARCHETYPE" id="reference_id" type="PEER"/>
    </Relationships>
  </Configuration>
  <Routings>
    <Routing precedence="1" routingId="routng1">
      <ProcessStep stepId="10">
        <Description>Process Step 10</Description>
        <StartTime>2025-11-24T00:00:00Z</StartTime>
        <Duration>23000</Duration>
        <Targets>
          <TargetRef groupIdRef="group1"/>
        </Targets>
        <ActivityGroups>
          <ActivityGroup activityGroupId="act1">
            <Activity activityId="a1" sequence="1">
              <Description>First Activity</Description>
            </Activity>
          </ActivityGroup>
        </ActivityGroups>
      </ProcessStep>
    </Routing>
  </Routings>
  <Targets>
    <TargetDevice deviceUuid="device1"/>
    <TargetGroup groupId="group1">
      <TargetDevice deviceUuid="device2"/>
      <TargetDevice deviceUuid="device3"/>
    </TargetGroup>
  </Targets>
</Process>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}

TEST_F(ProcessAssetTest, process_can_only_have_one_routings)
{
  const auto doc =
      R"DOC(<Process assetId="PROCESS_ARCH_ID" revision="1">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="PART_ID" assetType="PART_ARCHETYPE" id="reference_id" type="PEER"/>
    </Relationships>
  </Configuration>
  <Routings>
    <Routing precedence="1" routingId="routng1">
      <ProcessStep stepId="10">
        <Description>Process Step 10</Description>
        <StartTime>2025-11-24T00:00:00Z</StartTime>
        <Duration>23000</Duration>
      </ProcessStep>
    </Routing>
    <Routing precedence="2" routingId="routng2">
      <ProcessStep stepId="11">
        <Description>Process Step 11</Description>
        <StartTime>2025-11-25T00:00:00Z</StartTime>
        <Duration>20000</Duration>
      </ProcessStep>
    </Routing>
  </Routings>
  <Targets>
    <TargetDevice deviceUuid="device1"/>
    <TargetGroup groupId="group1">
      <TargetDevice deviceUuid="device2"/>
      <TargetDevice deviceUuid="device3"/>
    </TargetGroup>
  </Targets>
</Process>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(3, errors.size());

  auto it = errors.begin();
  {
    auto error = dynamic_cast<PropertyError *>(it->get());
    ASSERT_TRUE(error);
    EXPECT_EQ(
        "Routings(Routing): Entity list requirement Routing must have at least 1 and no more than 1 entries, 2 found"s,
        error->what());
    EXPECT_EQ("Routings", error->getEntity());
    EXPECT_EQ("Routing", error->getProperty());
  }

  it++;
  {
    auto error = it->get();
    ASSERT_TRUE(error);
    EXPECT_EQ("Process: Invalid element 'Routings'"s, error->what());
    EXPECT_EQ("Process", error->getEntity());
  }

  it++;
  {
    auto error = dynamic_cast<PropertyError *>(it->get());
    ASSERT_TRUE(error);
    EXPECT_EQ("Process(Routings): Property Routings is required and not provided"s, error->what());
    EXPECT_EQ("Process", error->getEntity());
    EXPECT_EQ("Routings", error->getProperty());
  }
}
