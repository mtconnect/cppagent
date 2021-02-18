//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "adapter/adapter.hpp"
#include "device_model/data_item/data_item.hpp"

#include <dlib/misc_api.h>

using namespace std;
using namespace mtconnect;
using namespace mtconnect::adapter;
using namespace mtconnect::entity;
using namespace mtconnect::device_model::data_item;

class DataItemTest : public testing::Test
{
 protected:
  void SetUp() override
  {
    {
      Properties props{
        {"id", "1"s},
        {"name", "DataItemTest1"s},
        {"type", "ACCELERATION"s},
        {"category", "SAMPLE"s},
        {"nativeUnits", "PERCENT"s}
      };
      ErrorList errors;
      m_dataItemA = DataItem::make(props, errors);
      EXPECT_EQ(0, errors.size());
    }

    {
      Properties props{
        {"id", "3"s},
        {"name", "DataItemTest2"s},
        {"type", "ACCELERATION"s},
        {"subType", "ACTUAL"s},
        {"category", "SAMPLE"s},
        {"units", "REVOLUTION/MINUTE"s},
        {"nativeScale", "1.0"s},
        {"significantDigits", "1"s},
        {"coordinateSystem", "WORK"s}
      };
      ErrorList errors;
      m_dataItemB = DataItem::make(props, errors);
      EXPECT_EQ(0, errors.size());

    }
    
    {
      Properties props{
        {"id", "4"s},
        {"name", "DataItemTest1"s},
        {"type", "LOAD"s},
        {"category", "CONDITION"s}
      };
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
  ASSERT_EQ("Acceleration", m_dataItemA->getObservationType());
  ASSERT_EQ("PERCENT", m_dataItemA->get<string>("nativeUnits"));
  ASSERT_FALSE(m_dataItemA->maybeGet<string>("subType"));
  ASSERT_FALSE(m_dataItemA->maybeGet<string>("nativeScale"));

  ASSERT_EQ("3", m_dataItemB->getId());
  ASSERT_EQ("DataItemTest2", m_dataItemB->getName());
  ASSERT_EQ("ACCELERATION", m_dataItemB->getType());
  ASSERT_EQ("Acceleration", m_dataItemB->getObservationType());
  ASSERT_EQ("ACTUAL", m_dataItemB->get<string>("subType"));
  ASSERT_FALSE(m_dataItemB->maybeGet<string>("nativeUnits"));
  ASSERT_EQ(1.0, m_dataItemB->get<double>("nativeScale"));
}

TEST_F(DataItemTest, HasNameAndSource)
{
  ASSERT_TRUE(m_dataItemA->hasName("DataItemTest1"));
  ASSERT_TRUE(m_dataItemB->hasName("DataItemTest2"));

  ASSERT_FALSE(m_dataItemA->getSource());
  ASSERT_FALSE(m_dataItemB->getSource());

  ASSERT_FALSE(m_dataItemB->hasName("DataItemTest2Source"));
  ASSERT_EQ("DataItemTest2", m_dataItemB->getSourceOrName());
  
  Properties sp{{"VALUE", "DataItemTest2Source"}};
  ErrorList errors;
  auto source = Source::getFactory()->make("Source", sp, errors);
  ASSERT_TRUE(errors.empty());
  
  Properties props{
    {"id", "1"s},
    {"name", "DataItemTest1"s},
    {"type", "ACCELERATION"s},
    {"category", "SAMPLE"s},
    {"nativeUnits", "PERCENT"s},
    {"Source", source}
  };
  auto dataItem = DataItem::make(props, errors);
  ASSERT_TRUE(errors.empty());

  ASSERT_TRUE(dataItem->hasName("DataItemTest2Source"));
  ASSERT_EQ("DataItemTest2Source", *dataItem->getSource());
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

#if 0
TEST_F(DataItemTest, Conversion)
{
  std::map<string, string> attributes1;
  attributes1["id"] = "p";
  attributes1["name"] = "position";
  attributes1["type"] = "POSITION";
  attributes1["category"] = "SAMPLE";
  attributes1["units"] = "MILLIMETER_3D";
  attributes1["nativeUnits"] = "INCH_3D";
  attributes1["coordinateSystem"] = "testCoordinateSystem";
  DataItem item1(attributes1);
  item1.conversionRequired();

  {
    Value value{Vector{1.0, 2.0, 3.0}};
    item1.convertValue(value);
    auto vec = get<Vector>(value);
    EXPECT_NEAR(25.4, vec[0], 0.0001);
    EXPECT_NEAR(50.8, vec[1], 0.0001);
    EXPECT_NEAR(76.2, vec[2], 0.0001);
  }
  
  std::map<string, string> attributes2;
  attributes2["id"] = "p";
  attributes2["name"] = "position";
  attributes2["type"] = "POSITION";
  attributes2["category"] = "SAMPLE";
  attributes2["units"] = "DEGREE_3D";
  attributes2["nativeUnits"] = "RADIAN_3D";
  attributes2["coordinateSystem"] = "testCoordinateSystem";
  DataItem item2(attributes2);
  item2.conversionRequired();

  {
    Value value{Vector{1.0, 2.0, 3.0}};
    item2.convertValue(value);
    auto vec = get<Vector>(value);
    EXPECT_NEAR(57.29578, vec[0], 0.0001);
    EXPECT_NEAR(114.5916, vec[1], 0.0001);
    EXPECT_NEAR(171.8873, vec[2], 0.0001);
  }
  
  std::map<string, string> attributes3;
  attributes3["id"] = "p";
  attributes3["name"] = "position";
  attributes3["type"] = "POSITION";
  attributes3["category"] = "SAMPLE";
  attributes3["units"] = "MILLIMETER";
  attributes3["nativeUnits"] = "MILLIMETER";
  attributes3["nativeScale"] = "10";
  attributes3["coordinateSystem"] = "testScale";
  DataItem item3(attributes3);
  item3.conversionRequired();

  EXPECT_NEAR(1.3, item3.convertValue(13.0), 0.0001);

  std::map<string, string> attributes4;
  attributes4["id"] = "p";
  attributes4["name"] = "position";
  attributes4["type"] = "POSITION";
  attributes4["category"] = "SAMPLE";
  attributes4["units"] = "AMPERE";
  attributes4["nativeUnits"] = "KILOAMPERE";
  attributes4["coordinateSystem"] = "testScale";
  DataItem item4(attributes4);
  item4.conversionRequired();

  EXPECT_NEAR(130.0, item4.convertValue(0.13), 0.0001);

  DataItem item5(attributes4);

  item5.setConversionRequired(false);
  ASSERT_TRUE(!item5.conversionRequired());

  EXPECT_DOUBLE_EQ(0.13, item5.convertValue(0.13));
}
#endif


TEST_F(DataItemTest, Condition)
{
  ASSERT_EQ(DataItem::CONDITION, m_dataItemC->getCategory());
}

TEST_F(DataItemTest, TimeSeries)
{
  {
    Properties props{
      {"id", "1"s},
      {"name", "DataItemTest1"s},
      {"type", "POSITION"s},
      {"category", "SAMPLE"s},
      {"units", "MILLIMETER"s},
      {"nativeUnits", "MILLIMETER"s},
      {"representation", "TIME_SERIES"s}
    };
    ErrorList errors;
    auto d = DataItem::make(props, errors);
    EXPECT_EQ(0, errors.size());
    
    ASSERT_EQ(string("PositionTimeSeries"), d->getObservationType());
  }

  {
    Properties props{
      {"id", "1"s},
      {"name", "DataItemTest1"s},
      {"type", "POSITION"s},
      {"category", "SAMPLE"s},
      {"units", "MILLIMETER"s},
      {"nativeUnits", "MILLIMETER"s},
      {"representation", "VALUE"s}
    };
    ErrorList errors;
    auto d = DataItem::make(props, errors);
    EXPECT_EQ(0, errors.size());
    
    ASSERT_EQ(string("Position"), d->getObservationType());
  }
}

TEST_F(DataItemTest, Statistic)
{
  Properties props{
    {"id", "1"s},
    {"name", "DataItemTest1"s},
    {"type", "POSITION"s},
    {"category", "SAMPLE"s},
    {"units", "MILLIMETER"s},
    {"nativeUnits", "MILLIMETER"s},
    {"statistic", "AVERAGE"s}
  };
  ErrorList errors;
  auto d = DataItem::make(props, errors);
  EXPECT_EQ(0, errors.size());
  
  ASSERT_EQ("AVERAGE", d->get<string>("statistic"));
}

TEST_F(DataItemTest, SampleRate)
{
  Properties props{
    {"id", "1"s},
    {"name", "DataItemTest1"s},
    {"type", "POSITION"s},
    {"category", "SAMPLE"s},
    {"units", "MILLIMETER"s},
    {"nativeUnits", "MILLIMETER"s},
    {"sampleRate", "42000"s}
  };
  ErrorList errors;
  auto d = DataItem::make(props, errors);
  EXPECT_EQ(0, errors.size());
  
  ASSERT_EQ(42000, d->get<double>("sampleRate"));
}

