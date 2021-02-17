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

class DataItemTest : public testing::Test
{
 protected:
  void SetUp() override
  {
    std::map<string, string> attributes1, attributes2, attributes3;

    attributes1["id"] = "1";
    attributes1["name"] = "DataItemTest1";
    attributes1["type"] = "ACCELERATION";
    attributes1["category"] = "SAMPLE";
    attributes1["nativeUnits"] = "PERCENT";
    m_dataItemA = make_unique<DataItem>(attributes1);

    attributes2["id"] = "3";
    attributes2["name"] = "DataItemTest2";
    attributes2["type"] = "ACCELERATION";
    attributes2["subType"] = "ACTUAL";
    attributes2["category"] = "EVENT";
    attributes2["units"] = "REVOLUTION/MINUTE";
    attributes2["nativeScale"] = "1.0";
    attributes2["significantDigits"] = "1";
    attributes2["coordinateSystem"] = "testCoordinateSystem";
    m_dataItemB = make_unique<DataItem>(attributes2);

    attributes3["id"] = "4";
    attributes3["name"] = "DataItemTest3";
    attributes3["type"] = "LOAD";
    attributes3["category"] = "CONDITION";
    m_dataItemC = make_unique<DataItem>(attributes3);
  }

  void TearDown() override
  {
    m_dataItemA.reset();
    m_dataItemB.reset();
    m_dataItemC.reset();
  }

  std::unique_ptr<DataItem> m_dataItemA;
  std::unique_ptr<DataItem> m_dataItemB;
  std::unique_ptr<DataItem> m_dataItemC;
};

TEST_F(DataItemTest, Getters)
{
  ASSERT_EQ((string) "1", m_dataItemA->getId());
  ASSERT_EQ((string) "DataItemTest1", m_dataItemA->getName());
  ASSERT_EQ((string) "ACCELERATION", m_dataItemA->getType());
  ASSERT_EQ((string) "Acceleration", m_dataItemA->getElementName());
  ASSERT_EQ((string) "PERCENT", m_dataItemA->getNativeUnits());
  ASSERT_TRUE(m_dataItemA->getSubType().empty());
  ASSERT_TRUE(!m_dataItemA->hasNativeScale());

  ASSERT_EQ((string) "3", m_dataItemB->getId());
  ASSERT_EQ((string) "DataItemTest2", m_dataItemB->getName());
  ASSERT_EQ((string) "ACCELERATION", m_dataItemB->getType());
  ASSERT_EQ((string) "Acceleration", m_dataItemB->getElementName());
  ASSERT_EQ((string) "ACTUAL", m_dataItemB->getSubType());
  ASSERT_EQ(m_dataItemB->getNativeUnits(), m_dataItemB->getUnits());
  ASSERT_EQ(1.0f, m_dataItemB->getNativeScale());
}

TEST_F(DataItemTest, GetAttributes)
{
  const auto &attributes1 = m_dataItemA->getAttributes();
  ASSERT_EQ((string) "1", attributes1.at("id"));
  ASSERT_EQ((string) "DataItemTest1", attributes1.at("name"));
  ASSERT_EQ((string) "ACCELERATION", attributes1.at("type"));
  ASSERT_TRUE(attributes1.find("subType") == attributes1.end());
  ASSERT_EQ((string) "PERCENT", attributes1.at("nativeUnits"));
  ASSERT_TRUE(attributes1.find("getNativeScale") == attributes1.end());
  ASSERT_TRUE(attributes1.find("coordinateSystem") == attributes1.end());

  const auto &attributes2 = m_dataItemB->getAttributes();
  ASSERT_EQ((string) "3", attributes2.at("id"));
  ASSERT_EQ((string) "DataItemTest2", attributes2.at("name"));
  ASSERT_EQ((string) "ACCELERATION", attributes2.at("type"));
  ASSERT_EQ((string) "ACTUAL", attributes2.at("subType"));
  ASSERT_EQ(attributes2.at("nativeUnits"), attributes2.at("units"));
  ASSERT_EQ((string) "1", attributes2.at("nativeScale"));
  ASSERT_EQ((string) "testCoordinateSystem", attributes2.at("coordinateSystem"));
}

TEST_F(DataItemTest, HasNameAndSource)
{
  ASSERT_TRUE(m_dataItemA->hasName("DataItemTest1"));
  ASSERT_TRUE(m_dataItemB->hasName("DataItemTest2"));

  ASSERT_TRUE(m_dataItemA->getSource().empty());
  ASSERT_TRUE(m_dataItemB->getSource().empty());

  ASSERT_TRUE(!m_dataItemB->hasName("DataItemTest2Source"));
  ASSERT_EQ((string) "DataItemTest2", m_dataItemB->getSourceOrName());

  m_dataItemB->addSource("DataItemTest2Source", "", "", "");
  ASSERT_TRUE(m_dataItemB->hasName("DataItemTest2Source"));
  ASSERT_EQ((string) "DataItemTest2Source", m_dataItemB->getSource());
  ASSERT_EQ((string) "DataItemTest2Source", m_dataItemB->getSourceOrName());
}

TEST_F(DataItemTest, IsSample)
{
  ASSERT_TRUE(m_dataItemA->isSample());
  ASSERT_TRUE(!m_dataItemB->isSample());
}

TEST_F(DataItemTest, Component)
{
  std::map<string, string> attributes1;
  attributes1["id"] = "3";
  attributes1["name"] = "AxesTestA";
  attributes1["uuid"] = "UniversalUniqueIdA";
  attributes1["sampleRate"] = "100.11";

  mtconnect::Component axes("Axes", attributes1);
  m_dataItemA->setComponent(axes);

  ASSERT_TRUE(&axes == m_dataItemA->getComponent());
}

TEST_F(DataItemTest, GetCamel)
{
  string prefix;
  ASSERT_TRUE(DataItem::getCamelType("", prefix).empty());
  ASSERT_EQ((string) "Camels", DataItem::getCamelType("CAMELS", prefix));
  ASSERT_TRUE(prefix.empty());

  // Test the one exception to the rules...
  ASSERT_EQ((string) "PH", DataItem::getCamelType("PH", prefix));
  ASSERT_EQ((string) "VoltageDC", DataItem::getCamelType("VOLTAGE_DC", prefix));
  ASSERT_EQ((string) "VoltageAC", DataItem::getCamelType("VOLTAGE_AC", prefix));
  ASSERT_EQ((string) "MTConnectVersion", DataItem::getCamelType("MTCONNECT_VERSION", prefix));
  ASSERT_EQ((string) "DeviceURI", DataItem::getCamelType("DEVICE_URI", prefix));

  ASSERT_EQ((string) "CamelCase", DataItem::getCamelType("CAMEL_CASE", prefix));
  ASSERT_EQ((string) "ABCc", DataItem::getCamelType("A_B_CC", prefix));

  prefix.clear();
  ASSERT_EQ((string) "CamelCase", DataItem::getCamelType("x:CAMEL_CASE", prefix));
  ASSERT_EQ((string) "x", prefix);
}

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

TEST_F(DataItemTest, Condition)
{
  ASSERT_EQ(DataItem::CONDITION, m_dataItemC->getCategory());
}

TEST_F(DataItemTest, TimeSeries)
{
  std::map<string, string> attributes1;

  attributes1["id"] = "1";
  attributes1["name"] = "DataItemTest1";
  attributes1["type"] = "POSITION";
  attributes1["category"] = "SAMPLE";
  attributes1["units"] = "MILLIMETER";
  attributes1["nativeUnits"] = "MILLIMETER";
  attributes1["representation"] = "TIME_SERIES";
  auto d = make_unique<DataItem>(attributes1);

  ASSERT_EQ(string("PositionTimeSeries"), d->getElementName());
  d.reset();

  attributes1.clear();
  attributes1["id"] = "1";
  attributes1["name"] = "DataItemTest1";
  attributes1["type"] = "POSITION";
  attributes1["category"] = "SAMPLE";
  attributes1["units"] = "MILLIMETER";
  attributes1["nativeUnits"] = "MILLIMETER";
  attributes1["representation"] = "VALUE";
  d = make_unique<DataItem>(attributes1);

  ASSERT_EQ(string("Position"), d->getElementName());
  d.reset();
}

TEST_F(DataItemTest, Statistic)
{
  std::map<string, string> attributes1;

  attributes1["id"] = "1";
  attributes1["name"] = "DataItemTest1";
  attributes1["type"] = "POSITION";
  attributes1["category"] = "SAMPLE";
  attributes1["units"] = "MILLIMETER";
  attributes1["nativeUnits"] = "MILLIMETER";
  attributes1["statistic"] = "AVERAGE";
  auto d = make_unique<DataItem>(attributes1);

  ASSERT_EQ(string("AVERAGE"), d->getStatistic());
  d.reset();
}

TEST_F(DataItemTest, SampleRate)
{
  std::map<string, string> attributes1;

  attributes1["id"] = "1";
  attributes1["name"] = "DataItemTest1";
  attributes1["type"] = "POSITION";
  attributes1["category"] = "SAMPLE";
  attributes1["units"] = "MILLIMETER";
  attributes1["nativeUnits"] = "MILLIMETER";
  attributes1["statistic"] = "AVERAGE";
  attributes1["representation"] = "TIME_SERIES";
  attributes1["sampleRate"] = "42000";
  auto d = make_unique<DataItem>(attributes1);

  ASSERT_EQ(string("42000"), d->getSampleRate());
  d.reset();
}
