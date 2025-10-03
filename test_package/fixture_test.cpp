//
// Copyright Copyright 2009-2025, AMT – The Association For Manufacturing Technology (“AMT”)
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
#include "mtconnect/asset/fixture.hpp"
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

class FixtureTest : public testing::Test
{
protected:
  void SetUp() override
  {
    Fixture::registerAsset();
    m_writer = make_unique<printer::XmlWriter>(true);
  }

  void TearDown() override { m_writer.reset(); }

  std::unique_ptr<printer::XmlWriter> m_writer;
};

TEST_F(FixtureTest, minimal_fixture_definition)
{
  using namespace date;
  const auto doc = R"DOC(
<Fixture assetId="7ae770f0-c11e-013a-c34c-4e7f553bbb76">
  <ManufactureDate>2022-05-20</ManufactureDate>
  <CalibrationDate>2022-05-21</CalibrationDate>
  <InspectionDate>2022-05-22</InspectionDate>
  <NextInspectionDate>2022-05-23</NextInspectionDate>
  <Measurements>
    <Length maximum="5.2" minimum="4.95" nominal="5" units="MILLIMETER">5.1</Length>
    <Diameter maximum="1.4" minimum="0.95" nominal="1.25" units="MILLIMETER">1.27</Diameter>
  </Measurements>
  <FixtureId>XXXYYY</FixtureId>
  <FixtureNumber>12345</FixtureNumber>
  <ClampingMethod>CLAMP</ClampingMethod>
  <MountingMethod>MOUNT</MountingMethod>
</Fixture>
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

  auto date = asset->get<Timestamp>("ManufactureDate");
  auto ymd = year_month_day(floor<days>(date));
  ASSERT_EQ(2022, int(ymd.year()));
  ASSERT_EQ(5, unsigned(ymd.month()));
  ASSERT_EQ(20, unsigned(ymd.day()));

  date = asset->get<Timestamp>("CalibrationDate");
  ymd = year_month_day(floor<days>(date));
  ASSERT_EQ(2022, int(ymd.year()));
  ASSERT_EQ(5, unsigned(ymd.month()));
  ASSERT_EQ(21, unsigned(ymd.day()));

  date = asset->get<Timestamp>("InspectionDate");
  ymd = year_month_day(floor<days>(date));
  ASSERT_EQ(2022, int(ymd.year()));
  ASSERT_EQ(5, unsigned(ymd.month()));
  ASSERT_EQ(22, unsigned(ymd.day()));

  date = asset->get<Timestamp>("NextInspectionDate");
  ymd = year_month_day(floor<days>(date));
  ASSERT_EQ(2022, int(ymd.year()));
  ASSERT_EQ(5, unsigned(ymd.month()));
  ASSERT_EQ(23, unsigned(ymd.day()));

  ASSERT_EQ("XXXYYY", asset->get<string>("FixtureId"));
  ASSERT_EQ(12345, asset->get<int64_t>("FixtureNumber"));
  ASSERT_EQ("CLAMP", asset->get<string>("ClampingMethod"));
  ASSERT_EQ("MOUNT", asset->get<string>("MountingMethod"));

  auto meas = asset->getList("Measurements");
  ASSERT_TRUE(meas);
  ASSERT_EQ(2, meas->size());

  auto it = meas->begin();
  ASSERT_EQ("Length", (*it)->getName());
  ASSERT_EQ("MILLIMETER", get<string>((*it)->getProperty("units")));
  ASSERT_EQ(5.0, get<double>((*it)->getProperty("nominal")));
  ASSERT_EQ(4.95, get<double>((*it)->getProperty("minimum")));
  ASSERT_EQ(5.2, get<double>((*it)->getProperty("maximum")));
  ASSERT_EQ(5.1, get<double>((*it)->getProperty("VALUE")));

  it++;
  ASSERT_EQ("Diameter", (*it)->getName());
  ASSERT_EQ("MILLIMETER", get<string>((*it)->getProperty("units")));
  ASSERT_EQ(1.25, get<double>((*it)->getProperty("nominal")));
  ASSERT_EQ(0.95, get<double>((*it)->getProperty("minimum")));
  ASSERT_EQ(1.4, get<double>((*it)->getProperty("maximum")));
  ASSERT_EQ(1.27, get<double>((*it)->getProperty("VALUE")));
}

TEST_F(FixtureTest, should_round_trip_xml)
{
  using namespace date;
  const auto doc =
      R"DOC(<Fixture assetId="7ae770f0-c11e-013a-c34c-4e7f553bbb76">
  <ManufactureDate>2022-05-20T00:00:00Z</ManufactureDate>
  <CalibrationDate>2022-05-21T00:00:00Z</CalibrationDate>
  <InspectionDate>2022-05-22T00:00:00Z</InspectionDate>
  <NextInspectionDate>2022-05-23T00:00:00Z</NextInspectionDate>
  <Measurements>
    <Length maximum="5.2" minimum="4.95" nominal="5" units="MILLIMETER">5.1</Length>
    <Diameter maximum="1.4" minimum="0.95" nominal="1.25" units="MILLIMETER">1.27</Diameter>
  </Measurements>
  <FixtureId>XXXYYY</FixtureId>
  <FixtureNumber>12345</FixtureNumber>
  <ClampingMethod>CLAMP</ClampingMethod>
  <MountingMethod>MOUNT</MountingMethod>
</Fixture>
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

TEST_F(FixtureTest, should_generate_json)
{
  using namespace date;
  const auto doc =
      R"DOC(<Fixture assetId="7ae770f0-c11e-013a-c34c-4e7f553bbb76">
  <ManufactureDate>2022-05-20T00:00:00Z</ManufactureDate>
  <CalibrationDate>2022-05-21T00:00:00Z</CalibrationDate>
  <InspectionDate>2022-05-22T00:00:00Z</InspectionDate>
  <NextInspectionDate>2022-05-23T00:00:00Z</NextInspectionDate>
  <Measurements>
    <Length maximum="5.2" minimum="4.95" nominal="5" units="MILLIMETER">5.1</Length>
    <Diameter maximum="1.4" minimum="0.95" nominal="1.25" units="MILLIMETER">1.27</Diameter>
  </Measurements>
  <FixtureId>XXXYYY</FixtureId>
  <FixtureNumber>12345</FixtureNumber>
  <ClampingMethod>CLAMP</ClampingMethod>
  <MountingMethod>MOUNT</MountingMethod>
</Fixture>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  entity::JsonEntityPrinter jsonPrinter(2, true);
  auto json = jsonPrinter.print(entity);

  ASSERT_EQ(R"({
  "Fixture": {
    "CalibrationDate": "2022-05-21T00:00:00Z",
    "ClampingMethod": "CLAMP",
    "FixtureId": "XXXYYY",
    "FixtureNumber": 12345,
    "InspectionDate": "2022-05-22T00:00:00Z",
    "ManufactureDate": "2022-05-20T00:00:00Z",
    "Measurements": {
      "Diameter": [
        {
          "value": 1.27,
          "maximum": 1.4,
          "minimum": 0.95,
          "nominal": 1.25,
          "units": "MILLIMETER"
        }
      ],
      "Length": [
        {
          "value": 5.1,
          "maximum": 5.2,
          "minimum": 4.95,
          "nominal": 5.0,
          "units": "MILLIMETER"
        }
      ]
    },
    "MountingMethod": "MOUNT",
    "NextInspectionDate": "2022-05-23T00:00:00Z",
    "assetId": "7ae770f0-c11e-013a-c34c-4e7f553bbb76"
  }
})",
            json);
}
