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
#include <date/date.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/asset/asset.hpp"
#include "mtconnect/asset/raw_material.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/entity/json_printer.hpp"
#include "mtconnect/entity/xml_parser.hpp"
#include "mtconnect/entity/xml_printer.hpp"
#include "mtconnect/printer//xml_printer.hpp"
#include "mtconnect/printer//xml_printer_helper.hpp"
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

class RawMaterialTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "2.0", 25);
    m_agentId = to_string(getCurrentTimeInSec());
    m_device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");

    // Asset types are registered in the agent.
    m_device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");

    m_writer = make_unique<printer::XmlWriter>(true);
  }

  void TearDown() override
  {
    m_agentTestHelper.reset();
    m_writer.reset();
  }

  void addAdapter() { m_agentTestHelper->addAdapter(); }

  std::string m_agentId;
  DevicePtr m_device {nullptr};

  std::unique_ptr<printer::XmlWriter> m_writer;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

TEST_F(RawMaterialTest, minimal_raw_material_definition)
{
  using namespace date;
  const auto doc = R"DOC(
<RawMaterial assetId="7ae770f0-c11e-013a-c34c-4e7f553bbb76"
   name="bob" containerType="bucket">
  <HasMaterial>true</HasMaterial>
  <Form>GEL</Form>
  <ManufacturingDate>2022-05-20</ManufacturingDate>
</RawMaterial>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset *>(entity.get());
  ASSERT_NE(nullptr, asset);

  ASSERT_EQ("7ae770f0-c11e-013a-c34c-4e7f553bbb76", asset->getAssetId());

  ASSERT_FALSE(asset->getTimestamp());
  ASSERT_FALSE(asset->getDeviceUuid());

  ASSERT_EQ("bob", asset->get<string>("name"));
  ASSERT_EQ("bucket", asset->get<string>("containerType"));
  ASSERT_EQ("GEL", asset->get<string>("Form"));
  ASSERT_TRUE(asset->get<bool>("HasMaterial"));

  auto date = asset->get<Timestamp>("ManufacturingDate");
  auto ymd = year_month_day(floor<days>(date));
  ASSERT_EQ(2022, int(ymd.year()));
  ASSERT_EQ(5, unsigned(ymd.month()));
  ASSERT_EQ(20, unsigned(ymd.day()));
}

TEST_F(RawMaterialTest, should_parse_raw_material_and_material)
{
  using namespace date;
  const auto doc = R"DOC(
<RawMaterial assetId="7ae770f0-c11e-013a-c34c-4e7f553bbb76"
   name="bob" containerType="bucket">
  <HasMaterial>true</HasMaterial>
  <Form>GEL</Form>
  <ManufacturingDate>2022-05-20</ManufacturingDate>
  <Material id="XXX" type="floop">
   <ManufacturingDate>2022-01-10</ManufacturingDate>
   <Manufacturer>acme</Manufacturer>
  </Material>
</RawMaterial>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset *>(entity.get());
  ASSERT_NE(nullptr, asset);

  ASSERT_EQ("7ae770f0-c11e-013a-c34c-4e7f553bbb76", asset->getAssetId());

  auto material = asset->get<EntityPtr>("Material");
  ASSERT_NE(nullptr, material);

  ASSERT_EQ("XXX", material->get<string>("id"));
  ASSERT_EQ("floop", material->get<string>("type"));
  ASSERT_EQ("acme", material->get<string>("Manufacturer"));

  auto date = material->get<Timestamp>("ManufacturingDate");
  auto ymd = year_month_day(floor<days>(date));
  ASSERT_EQ(2022, int(ymd.year()));
  ASSERT_EQ(1, unsigned(ymd.month()));
  ASSERT_EQ(10, unsigned(ymd.day()));
}

TEST_F(RawMaterialTest, should_round_trip_xml)
{
  using namespace date;
  const auto doc =
      R"DOC(<RawMaterial assetId="7ae770f0-c11e-013a-c34c-4e7f553bbb76" containerType="bucket" name="bob" processKind="FLA" serialNumber="21345">
  <HasMaterial>true</HasMaterial>
  <Form>GEL</Form>
  <ManufacturingDate>2022-05-20T00:00:00Z</ManufacturingDate>
  <FirstUseDate>2022-05-26T00:00:00Z</FirstUseDate>
  <LastUseDate>2022-05-26T00:00:00Z</LastUseDate>
  <InitialVolume>123.4</InitialVolume>
  <InitialDimension>555.4</InitialDimension>
  <InitialQuantity>10</InitialQuantity>
  <CurrentVolume>13.4</CurrentVolume>
  <CurrentDimension>255.8</CurrentDimension>
  <CurrentQuantity>5</CurrentQuantity>
  <Material id="XXX" name="abc" type="floop">
    <Lot>TTT</Lot>
    <Manufacturer>acme</Manufacturer>
    <ManufacturingDate>2022-01-10T00:00:00Z</ManufacturingDate>
    <ManufacturingCode>GGG</ManufacturingCode>
    <MaterialCode>AAA</MaterialCode>
  </Material>
</RawMaterial>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {"x"});
  string content = m_writer->getContent();

  ASSERT_EQ(doc, content);
}

TEST_F(RawMaterialTest, should_generate_json)
{
  using namespace date;
  const auto doc =
      R"DOC(<RawMaterial assetId="7ae770f0-c11e-013a-c34c-4e7f553bbb76" containerType="bucket" name="bob" processKind="FLA" serialNumber="21345">
  <HasMaterial>true</HasMaterial>
  <Form>GEL</Form>
  <ManufacturingDate>2022-05-20T00:00:00Z</ManufacturingDate>
  <FirstUseDate>2022-05-26T00:00:00Z</FirstUseDate>
  <LastUseDate>2022-05-26T00:00:00Z</LastUseDate>
  <InitialVolume>123.4</InitialVolume>
  <InitialDimension>555.4</InitialDimension>
  <InitialQuantity>10</InitialQuantity>
  <CurrentVolume>13.4</CurrentVolume>
  <CurrentDimension>255.8</CurrentDimension>
  <CurrentQuantity>5</CurrentQuantity>
  <Material id="XXX" name="abc" type="floop">
    <Lot>TTT</Lot>
    <Manufacturer>acme</Manufacturer>
    <ManufacturingDate>2022-01-10T00:00:00Z</ManufacturingDate>
    <ManufacturingCode>GGG</ManufacturingCode>
    <MaterialCode>AAA</MaterialCode>
  </Material>
</RawMaterial>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  entity::JsonEntityPrinter jsonPrinter(1, true);
  auto json = jsonPrinter.print(entity);

  ASSERT_EQ(R"({
  "RawMaterial": {
    "CurrentDimension": 255.8,
    "CurrentQuantity": 5,
    "CurrentVolume": 13.4,
    "FirstUseDate": "2022-05-26T00:00:00Z",
    "Form": "GEL",
    "HasMaterial": true,
    "InitialDimension": 555.4,
    "InitialQuantity": 10,
    "InitialVolume": 123.4,
    "LastUseDate": "2022-05-26T00:00:00Z",
    "ManufacturingDate": "2022-05-20T00:00:00Z",
    "Material": {
      "Lot": "TTT",
      "Manufacturer": "acme",
      "ManufacturingCode": "GGG",
      "ManufacturingDate": "2022-01-10T00:00:00Z",
      "MaterialCode": "AAA",
      "id": "XXX",
      "name": "abc",
      "type": "floop"
    },
    "assetId": "7ae770f0-c11e-013a-c34c-4e7f553bbb76",
    "containerType": "bucket",
    "name": "bob",
    "processKind": "FLA",
    "serialNumber": "21345"
  }
})",
            json);
}
