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
#include "mtconnect/asset/part.hpp"
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

class PartAssetTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    Part::registerAsset();
    PartArchetype::registerAsset();
    m_writer = make_unique<XmlWriter>(true);
  }

  void TearDown() override { m_writer.reset(); }

  std::unique_ptr<XmlWriter> m_writer;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

/// @section PartArchetype tests

TEST_F(PartAssetTest, should_parse_a_part_archetype)
{
  const auto doc =
      R"DOC(<PartArchetype assetId="PART1234" drawing="STEP222" family="HHH" revision="5">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="MATERIAL" assetType="RawMaterial" id="A" type="PEER"/>
      <AssetRelationship assetIdRef="PROCESS" assetType="ProcessArchetype" id="B" type="PEER"/>
    </Relationships>
  </Configuration>
  <Customers>
    <Customer customerId="C00241" name="customer name">
      <Address>100 Fruitstand Rd, Ork Arkansas, 11111</Address>
      <Description>Some customer</Description>
    </Customer>
  </Customers>
</PartArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset*>(entity.get());
  ASSERT_NE(nullptr, asset);

  ASSERT_EQ("PartArchetype", asset->getName());
  ASSERT_EQ("PART1234", asset->getAssetId());
  ASSERT_EQ("5", asset->get<string>("revision"));
  ASSERT_EQ("STEP222", asset->get<string>("drawing"));
  ASSERT_EQ("HHH", asset->get<string>("family"));

  auto configuration = asset->get<EntityPtr>("Configuration");
  ASSERT_TRUE(configuration);

  auto relationships = configuration->getList("Relationships");
  ASSERT_TRUE(relationships);
  ASSERT_EQ(2, relationships->size());

  {
    auto it = relationships->begin();
    ASSERT_EQ("A", (*it)->get<string>("id"));
    ASSERT_EQ("MATERIAL", (*it)->get<string>("assetIdRef"));
    ASSERT_EQ("PEER", (*it)->get<string>("type"));
    ASSERT_EQ("RawMaterial", (*it)->get<string>("assetType"));

    it++;
    ASSERT_EQ("B", (*it)->get<string>("id"));
    ASSERT_EQ("PROCESS", (*it)->get<string>("assetIdRef"));
    ASSERT_EQ("PEER", (*it)->get<string>("type"));
    ASSERT_EQ("ProcessArchetype", (*it)->get<string>("assetType"));
  }

  auto customers = asset->getList("Customers");
  ASSERT_TRUE(customers);
  ASSERT_EQ(1, customers->size());

  {
    auto customer = customers->front();
    ASSERT_EQ("C00241", customer->get<string>("customerId"));
    ASSERT_EQ("customer name", customer->get<string>("name"));
    ASSERT_EQ("100 Fruitstand Rd, Ork Arkansas, 11111", customer->get<string>("Address"));
    ASSERT_EQ("Some customer", customer->get<string>("Description"));
  }

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}

TEST_F(PartAssetTest, process_archetype_can_have_multiple_customers)
{
  const auto doc =
      R"DOC(<PartArchetype assetId="PART1234" drawing="STEP222" family="HHH" revision="5">
  <Customers>
    <Customer customerId="C00241" name="customer name">
      <Address>100 Fruitstand Rd, Ork Arkansas, 11111</Address>
      <Description>Some customer</Description>
    </Customer>
    <Customer customerId="C1111" name="another customer">
      <Address>Somewhere in Austrailia</Address>
      <Description>Another customer</Description>
    </Customer>
  </Customers>
</PartArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset*>(entity.get());
  ASSERT_NE(nullptr, asset);
  ASSERT_EQ("PartArchetype", asset->getName());

  auto customers = asset->getList("Customers");
  ASSERT_TRUE(customers);
  ASSERT_EQ(2, customers->size());

  {
    auto it = customers->begin();
    auto customer = *it;
    EXPECT_EQ("C00241", customer->get<string>("customerId"));
    EXPECT_EQ("customer name", customer->get<string>("name"));
    EXPECT_EQ("100 Fruitstand Rd, Ork Arkansas, 11111", customer->get<string>("Address"));
    EXPECT_EQ("Some customer", customer->get<string>("Description"));

    it++;
    customer = *it;
    EXPECT_EQ("C1111", customer->get<string>("customerId"));
    EXPECT_EQ("another customer", customer->get<string>("name"));
    EXPECT_EQ("Somewhere in Austrailia", customer->get<string>("Address"));
    EXPECT_EQ("Another customer", customer->get<string>("Description"));
  }

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}

TEST_F(PartAssetTest, customers_are_optional)
{
  const auto doc =
      R"DOC(<PartArchetype assetId="PART1234" drawing="STEP222" family="HHH" revision="5"/>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset*>(entity.get());
  ASSERT_NE(nullptr, asset);

  ASSERT_EQ("PartArchetype", asset->getName());
  ASSERT_EQ("PART1234", asset->getAssetId());
  ASSERT_EQ("5", asset->get<string>("revision"));
  ASSERT_EQ("STEP222", asset->get<string>("drawing"));
  ASSERT_EQ("HHH", asset->get<string>("family"));

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}

TEST_F(PartAssetTest, should_generate_json)
{
  const auto doc =
      R"DOC(<PartArchetype assetId="PART1234" drawing="STEP222" family="HHH" revision="5">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="MATERIAL" assetType="RawMaterial" id="A" type="PEER"/>
      <AssetRelationship assetIdRef="PROCESS" assetType="ProcessArchetype" id="B" type="PEER"/>
    </Relationships>
  </Configuration>
  <Customers>
    <Customer customerId="C00241" name="customer name">
      <Address>100 Fruitstand Rd, Ork Arkansas, 11111</Address>
      <Description>Some customer</Description>
    </Customer>
  </Customers>
</PartArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset*>(entity.get());
  ASSERT_NE(nullptr, asset);

  ASSERT_EQ("PartArchetype", asset->getName());

  // Round trip test
  entity::JsonEntityPrinter jprinter(2, true);

  auto jdoc = jprinter.print(entity);
  EXPECT_EQ(R"({
  "PartArchetype": {
    "Configuration": {
      "Relationships": {
        "AssetRelationship": [
          {
            "assetIdRef": "MATERIAL",
            "assetType": "RawMaterial",
            "id": "A",
            "type": "PEER"
          },
          {
            "assetIdRef": "PROCESS",
            "assetType": "ProcessArchetype",
            "id": "B",
            "type": "PEER"
          }
        ]
      }
    },
    "Customers": {
      "Customer": [
        {
          "Address": "100 Fruitstand Rd, Ork Arkansas, 11111",
          "Description": "Some customer",
          "customerId": "C00241",
          "name": "customer name"
        }
      ]
    },
    "assetId": "PART1234",
    "drawing": "STEP222",
    "family": "HHH",
    "revision": "5"
  }
})",
            jdoc);
}

TEST_F(PartAssetTest, part_archetype_should_be_extensible)
{
  const auto doc =
      R"DOC(<PartArchetype assetId="PART1234" drawing="STEP222" family="HHH" revision="5">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="MATERIAL" assetType="RawMaterial" id="A" type="PEER"/>
      <AssetRelationship assetIdRef="PROCESS" assetType="ProcessArchetype" id="B" type="PEER"/>
    </Relationships>
  </Configuration>
  <Customers>
    <Customer customerId="C00241" name="customer name">
      <Address>100 Fruitstand Rd, Ork Arkansas, 11111</Address>
      <Description>Some customer</Description>
    </Customer>
  </Customers>
  <Properties>
    <Property name="CustomProperty1" value="Value1"/>
    <Property name="CustomProperty2" value="Value2"/>
  </Properties>
  <SimpleExtension>Some simple extension value</SimpleExtension>
</PartArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset*>(entity.get());
  ASSERT_NE(nullptr, asset);

  ASSERT_EQ("PartArchetype", asset->getName());

  auto properties = asset->getList("Properties");
  ASSERT_TRUE(properties);

  ASSERT_EQ(2, properties->size());
  {
    auto it = properties->begin();
    EXPECT_EQ("CustomProperty1", (*it)->get<string>("name"));
    EXPECT_EQ("Value1", (*it)->get<string>("value"));
    it++;
    EXPECT_EQ("CustomProperty2", (*it)->get<string>("name"));
    EXPECT_EQ("Value2", (*it)->get<string>("value"));
  }

  auto sext = asset->get<string>("SimpleExtension");
  ASSERT_EQ("Some simple extension value", sext);
}

/// @section Part asset tests

TEST_F(PartAssetTest, should_parse_a_part)
{
  const auto doc =
      R"DOC(<Part assetId="PART1234" drawing="STEP222" family="HHH" nativeId="NATIVE001" revision="5">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="MATERIAL" assetType="RawMaterial" id="A" type="PEER"/>
      <AssetRelationship assetIdRef="PROCESS" assetType="ProcessArchetype" id="B" type="PEER"/>
    </Relationships>
  </Configuration>
  <PartIdentifiers>
    <Identifier stepIdRef="10" timestamp="2025-11-28T00:01:00Z" type="UNIQUE_IDENTIFIER">UID123456</Identifier>
    <Identifier stepIdRef="11" timestamp="2025-11-28T00:02:00Z" type="GROUP_IDENTIFIER">GID1235</Identifier>
  </PartIdentifiers>
</Part>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset*>(entity.get());
  ASSERT_NE(nullptr, asset);

  ASSERT_EQ("Part", asset->getName());
  EXPECT_EQ("PART1234", asset->getAssetId());
  EXPECT_EQ("5", asset->get<string>("revision"));
  EXPECT_EQ("STEP222", asset->get<string>("drawing"));
  EXPECT_EQ("HHH", asset->get<string>("family"));
  EXPECT_EQ("NATIVE001", asset->get<string>("nativeId"));

  auto configuration = asset->get<EntityPtr>("Configuration");
  ASSERT_TRUE(configuration);

  auto relationships = configuration->getList("Relationships");
  ASSERT_TRUE(relationships);
  ASSERT_EQ(2, relationships->size());

  {
    auto it = relationships->begin();
    EXPECT_EQ("A", (*it)->get<string>("id"));
    EXPECT_EQ("MATERIAL", (*it)->get<string>("assetIdRef"));
    EXPECT_EQ("PEER", (*it)->get<string>("type"));
    EXPECT_EQ("RawMaterial", (*it)->get<string>("assetType"));

    it++;
    EXPECT_EQ("B", (*it)->get<string>("id"));
    EXPECT_EQ("PROCESS", (*it)->get<string>("assetIdRef"));
    EXPECT_EQ("PEER", (*it)->get<string>("type"));
    EXPECT_EQ("ProcessArchetype", (*it)->get<string>("assetType"));
  }

  auto identifiers = asset->getList("PartIdentifiers");
  ASSERT_TRUE(identifiers);
  ASSERT_EQ(2, identifiers->size());

  {
    auto it = identifiers->begin();
    auto identifier = *it;
    EXPECT_EQ("UNIQUE_IDENTIFIER", identifier->get<string>("type"));
    EXPECT_EQ("10", identifier->get<string>("stepIdRef"));
    auto st = identifier->get<Timestamp>("timestamp");
    EXPECT_EQ("2025-11-28T00:01:00Z", getCurrentTime(st, GMT));
    EXPECT_EQ("UID123456", identifier->getValue<string>());

    it++;
    identifier = *it;
    EXPECT_EQ("GROUP_IDENTIFIER", identifier->get<string>("type"));
    EXPECT_EQ("11", identifier->get<string>("stepIdRef"));
    st = identifier->get<Timestamp>("timestamp");
    EXPECT_EQ("2025-11-28T00:02:00Z", getCurrentTime(st, GMT));
    EXPECT_EQ("GID1235", identifier->getValue<string>());
  }

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}

TEST_F(PartAssetTest, part_identifiers_are_optional)
{
  const auto doc =
      R"DOC(<Part assetId="PART1234" drawing="STEP222" family="HHH" nativeId="NATIVE001" revision="5"/>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset*>(entity.get());
  ASSERT_NE(nullptr, asset);

  ASSERT_EQ("Part", asset->getName());
  EXPECT_EQ("PART1234", asset->getAssetId());
  EXPECT_EQ("5", asset->get<string>("revision"));
  EXPECT_EQ("STEP222", asset->get<string>("drawing"));
  EXPECT_EQ("HHH", asset->get<string>("family"));
  EXPECT_EQ("NATIVE001", asset->get<string>("nativeId"));

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}

TEST_F(PartAssetTest, part_identifiers_type_must_be_unique_or_group)
{
  const auto doc =
      R"DOC(<Part assetId="PART1234" drawing="STEP222" family="HHH" nativeId="NATIVE001" revision="5">
  <PartIdentifiers>
    <Identifier stepIdRef="10" timestamp="2025-11-28T00:01:00Z" type="UNIQUE_IDENTIFIER">UID123456</Identifier>
    <Identifier stepIdRef="11" timestamp="2025-11-28T00:02:00Z" type="OTHER_IDENTIFIER">GID1235</Identifier>
  </PartIdentifiers>
</Part>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(2, errors.size());

  auto it = errors.begin();
  {
    auto error = dynamic_cast<PropertyError*>(it->get());
    ASSERT_TRUE(error);
    EXPECT_EQ("Identifier(type): Invalid value for 'type': 'OTHER_IDENTIFIER' is not allowed"s,
              error->what());
    EXPECT_EQ("Identifier", error->getEntity());
    EXPECT_EQ("type", error->getProperty());
  }

  it++;
  {
    auto error = it->get();
    ASSERT_TRUE(error);
    EXPECT_EQ("PartIdentifiers: Invalid element 'Identifier'"s, error->what());
    EXPECT_EQ("PartIdentifiers", error->getEntity());
  }
}

TEST_F(PartAssetTest, part_should_be_extensible)
{
  const auto doc =
      R"DOC(<Part assetId="PART1234" drawing="STEP222" family="HHH" nativeId="NATIVE001" revision="5">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="MATERIAL" assetType="RawMaterial" id="A" type="PEER"/>
      <AssetRelationship assetIdRef="PROCESS" assetType="ProcessArchetype" id="B" type="PEER"/>
    </Relationships>
  </Configuration>
  <PartIdentifiers>
    <Identifier stepIdRef="10" timestamp="2025-11-28T00:01:00Z" type="UNIQUE_IDENTIFIER">UID123456</Identifier>
    <Identifier stepIdRef="11" timestamp="2025-11-28T00:02:00Z" type="GROUP_IDENTIFIER">GID1235</Identifier>
  </PartIdentifiers>
  <WorkOrder number="WO12345">
    <OrderDate>2025-12-01T00:00:00Z</OrderDate>
    <DueDate>2025-12-20T00:00:00Z</DueDate>
    <PlannedQuantity>100</PlannedQuantity>
  </WorkOrder>
</Part>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset*>(entity.get());
  ASSERT_NE(nullptr, asset);

  auto workOrder = asset->get<EntityPtr>("WorkOrder");
  ASSERT_TRUE(workOrder);

  ASSERT_EQ("WO12345", workOrder->get<string>("number"));
  ASSERT_EQ("2025-12-01T00:00:00Z", workOrder->get<string>("OrderDate"));
  ASSERT_EQ("2025-12-20T00:00:00Z", workOrder->get<string>("DueDate"));
  ASSERT_EQ("100", workOrder->get<string>("PlannedQuantity"));
}

TEST_F(PartAssetTest, part_should_generate_json)
{
  const auto doc =
      R"DOC(<Part assetId="PART1234" drawing="STEP222" family="HHH" nativeId="NATIVE001" revision="5">
  <Configuration>
    <Relationships>
      <AssetRelationship assetIdRef="MATERIAL" assetType="RawMaterial" id="A" type="PEER"/>
      <AssetRelationship assetIdRef="PROCESS" assetType="ProcessArchetype" id="B" type="PEER"/>
    </Relationships>
  </Configuration>
  <PartIdentifiers>
    <Identifier stepIdRef="10" timestamp="2025-11-28T00:01:00Z" type="UNIQUE_IDENTIFIER">UID123456</Identifier>
    <Identifier stepIdRef="11" timestamp="2025-11-28T00:02:00Z" type="GROUP_IDENTIFIER">GID1235</Identifier>
  </PartIdentifiers>
  <WorkOrder number="WO12345">
    <OrderDate>2025-12-01T00:00:00Z</OrderDate>
    <DueDate>2025-12-20T00:00:00Z</DueDate>
    <PlannedQuantity>100</PlannedQuantity>
  </WorkOrder>
</Part>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset*>(entity.get());
  ASSERT_NE(nullptr, asset);

  // Round trip test
  entity::JsonEntityPrinter jprinter(2, true);

  auto jdoc = jprinter.print(entity);
  EXPECT_EQ(R"({
  "Part": {
    "Configuration": {
      "Relationships": {
        "AssetRelationship": [
          {
            "assetIdRef": "MATERIAL",
            "assetType": "RawMaterial",
            "id": "A",
            "type": "PEER"
          },
          {
            "assetIdRef": "PROCESS",
            "assetType": "ProcessArchetype",
            "id": "B",
            "type": "PEER"
          }
        ]
      }
    },
    "PartIdentifiers": {
      "Identifier": [
        {
          "value": "UID123456",
          "stepIdRef": "10",
          "timestamp": "2025-11-28T00:01:00Z",
          "type": "UNIQUE_IDENTIFIER"
        },
        {
          "value": "GID1235",
          "stepIdRef": "11",
          "timestamp": "2025-11-28T00:02:00Z",
          "type": "GROUP_IDENTIFIER"
        }
      ]
    },
    "WorkOrder": {
      "DueDate": "2025-12-20T00:00:00Z",
      "OrderDate": "2025-12-01T00:00:00Z",
      "PlannedQuantity": "100",
      "number": "WO12345"
    },
    "assetId": "PART1234",
    "drawing": "STEP222",
    "family": "HHH",
    "nativeId": "NATIVE001",
    "revision": "5"
  }
})",
            jdoc);
}
