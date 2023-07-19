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

#include "mtconnect/device_model/data_item/data_item.hpp"
#include "mtconnect/source/adapter/adapter.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::source::adapter;
using namespace mtconnect::entity;
using namespace mtconnect::device_model::data_item;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class DataItemTest : public testing::Test
{
protected:
  void SetUp() override
  {
    {
      Properties props {{"id", "1"s},
                        {"name", "DataItemTest1"s},
                        {"type", "ACCELERATION"s},
                        {"category", "SAMPLE"s},
                        {"units", "PERCENT"s},
                        {"nativeUnits", "PERCENT"s}};
      ErrorList errors;
      m_dataItemA = DataItem::make(props, errors);
      EXPECT_EQ(0, errors.size());
    }

    {
      Properties props {{"id", "3"s},
                        {"name", "DataItemTest2"s},
                        {"type", "ACCELERATION"s},
                        {"subType", "ACTUAL"s},
                        {"category", "SAMPLE"s},
                        {"units", "REVOLUTION/MINUTE"s},
                        {"nativeScale", "1.0"s},
                        {"significantDigits", "1"s},
                        {"coordinateSystem", "WORK"s}};
      ErrorList errors;
      m_dataItemB = DataItem::make(props, errors);
      EXPECT_EQ(0, errors.size());
    }

    {
      Properties props {
          {"id", "4"s}, {"name", "DataItemTest1"s}, {"type", "LOAD"s}, {"category", "CONDITION"s}};
      ErrorList errors;
      m_dataItemC = DataItem::make(props, errors);
      EXPECT_EQ(0, errors.size());
    }
  }

  void TearDown() override
  {
    m_dataItemA.reset();
    m_dataItemB.reset();
    m_dataItemC.reset();
  }

  DataItemPtr m_dataItemA;
  DataItemPtr m_dataItemB;
  DataItemPtr m_dataItemC;
};

TEST_F(DataItemTest, Getters)
{
  ASSERT_EQ("1", m_dataItemA->getId());
  ASSERT_EQ("DataItemTest1", m_dataItemA->getName());
  ASSERT_EQ("ACCELERATION", m_dataItemA->getType());
  ASSERT_EQ("Acceleration", m_dataItemA->getObservationName());
  ASSERT_EQ("PERCENT", m_dataItemA->get<string>("nativeUnits"));
  ASSERT_FALSE(m_dataItemA->maybeGet<string>("subType"));
  ASSERT_FALSE(m_dataItemA->maybeGet<string>("nativeScale"));

  ASSERT_EQ("3", m_dataItemB->getId());
  ASSERT_EQ("DataItemTest2", m_dataItemB->getName());
  ASSERT_EQ("ACCELERATION", m_dataItemB->getType());
  ASSERT_EQ("Acceleration", m_dataItemB->getObservationName());
  ASSERT_EQ("ACTUAL", m_dataItemB->get<string>("subType"));
  ASSERT_FALSE(m_dataItemB->maybeGet<string>("nativeUnits"));
  ASSERT_EQ(1.0, m_dataItemB->get<double>("nativeScale"));
}

TEST_F(DataItemTest, HasNameAndSource)
{
  namespace di = mtconnect::device_model::data_item;

  ASSERT_TRUE(m_dataItemA->hasName("DataItemTest1"));
  ASSERT_TRUE(m_dataItemB->hasName("DataItemTest2"));

  ASSERT_FALSE(m_dataItemA->hasProperty("Source"));
  ASSERT_FALSE(m_dataItemB->hasProperty("Source"));

  ASSERT_FALSE(m_dataItemB->hasName("DataItemTest2Source"));
  ASSERT_EQ("DataItemTest2", m_dataItemB->getSourceOrName());

  Properties sp {{"VALUE", "DataItemTest2Source"s}};
  ErrorList errors;
  auto source = di::Source::getFactory()->make("Source", sp, errors);
  ASSERT_TRUE(errors.empty());

  Properties props {{"id", "1"s},
                    {"name", "DataItemTest1"s},
                    {"type", "ACCELERATION"s},
                    {"category", "SAMPLE"s},
                    {"units", "PERCENT"s},
                    {"nativeUnits", "PERCENT"s},
                    {"Source", source}};
  auto dataItem = DataItem::make(props, errors);
  ASSERT_TRUE(errors.empty());

  ASSERT_TRUE(dataItem->hasName("DataItemTest2Source"));
  ASSERT_EQ("DataItemTest2Source", dataItem->getSource()->getValue<string>());
  ASSERT_EQ("DataItemTest2Source", dataItem->getPreferredName());
}

TEST_F(DataItemTest, IsSample)
{
  ASSERT_TRUE(m_dataItemA->isSample());
  ASSERT_FALSE(m_dataItemC->isSample());
}

TEST_F(DataItemTest, GetCamel)
{
  std::optional<string> prefix;
  ASSERT_TRUE(pascalize("", prefix).empty());
  ASSERT_EQ((string) "Camels", pascalize("CAMELS", prefix));
  ASSERT_FALSE(prefix);

  // Test the one exception to the rules...
  ASSERT_EQ((string) "PH", pascalize("PH", prefix));
  ASSERT_EQ((string) "VoltageDC", pascalize("VOLTAGE_DC", prefix));
  ASSERT_EQ((string) "VoltageAC", pascalize("VOLTAGE_AC", prefix));
  ASSERT_EQ((string) "MTConnectVersion", pascalize("MTCONNECT_VERSION", prefix));
  ASSERT_EQ((string) "DeviceURI", pascalize("DEVICE_URI", prefix));

  ASSERT_EQ((string) "CamelCase", pascalize("CAMEL_CASE", prefix));
  ASSERT_EQ((string) "ABCc", pascalize("A_B_CC", prefix));

  prefix.reset();
  ASSERT_EQ((string) "CamelCase", pascalize("x:CAMEL_CASE", prefix));
  ASSERT_EQ((string) "x", *prefix);
}

TEST_F(DataItemTest, Condition) { ASSERT_EQ(DataItem::CONDITION, m_dataItemC->getCategory()); }

TEST_F(DataItemTest, TimeSeries)
{
  {
    Properties props {{"id", "1"s},
                      {"name", "DataItemTest1"s},
                      {"type", "POSITION"s},
                      {"category", "SAMPLE"s},
                      {"units", "MILLIMETER"s},
                      {"nativeUnits", "MILLIMETER"s},
                      {"representation", "TIME_SERIES"s}};
    ErrorList errors;
    auto d = DataItem::make(props, errors);
    EXPECT_EQ(0, errors.size());

    ASSERT_EQ(string("PositionTimeSeries"), d->getObservationName());
  }

  {
    Properties props {{"id", "1"s},
                      {"name", "DataItemTest1"s},
                      {"type", "POSITION"s},
                      {"category", "SAMPLE"s},
                      {"units", "MILLIMETER"s},
                      {"nativeUnits", "MILLIMETER"s},
                      {"representation", "VALUE"s}};
    ErrorList errors;
    auto d = DataItem::make(props, errors);
    EXPECT_EQ(0, errors.size());

    ASSERT_EQ(string("Position"), d->getObservationName());
  }
}

TEST_F(DataItemTest, Statistic)
{
  Properties props {{"id", "1"s},
                    {"name", "DataItemTest1"s},
                    {"type", "POSITION"s},
                    {"category", "SAMPLE"s},
                    {"units", "MILLIMETER"s},
                    {"nativeUnits", "MILLIMETER"s},
                    {"statistic", "AVERAGE"s}};
  ErrorList errors;
  auto d = DataItem::make(props, errors);
  EXPECT_EQ(0, errors.size());

  ASSERT_EQ("AVERAGE", d->get<string>("statistic"));
}

TEST_F(DataItemTest, SampleRate)
{
  Properties props {{"id", "1"s},
                    {"name", "DataItemTest1"s},
                    {"type", "POSITION"s},
                    {"category", "SAMPLE"s},
                    {"units", "MILLIMETER"s},
                    {"nativeUnits", "MILLIMETER"s},
                    {"sampleRate", "42000"s}};
  ErrorList errors;
  auto d = DataItem::make(props, errors);
  EXPECT_EQ(0, errors.size());

  ASSERT_EQ(42000, d->get<double>("sampleRate"));
}
