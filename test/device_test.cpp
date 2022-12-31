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

#include "mtconnect/device_model/device.hpp"
using namespace std;
using namespace mtconnect;
using namespace entity;
using namespace device_model;
using namespace data_item;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class DeviceTest : public testing::Test
{
protected:
  void SetUp() override
  {
    ErrorList errors;
    Properties d1 {
        {"id", "1"s}, {"name", "DeviceTest1"s}, {"uuid", "UnivUniqId1"s}, {"iso841Class", "4"s}};
    m_devA = dynamic_pointer_cast<Device>(Device::getFactory()->make("Device", d1, errors));

    Properties d2 {
        {"id", "3"s}, {"name", "DeviceTest2"s}, {"uuid", "UnivUniqId2"s}, {"iso841Class", "6"s}};
    m_devB = dynamic_pointer_cast<Device>(Device::getFactory()->make("Device", d2, errors));
  }

  void TearDown() override
  {
    m_devA.reset();
    m_devB.reset();
  }

  DevicePtr m_devA, m_devB;
};

TEST_F(DeviceTest, Getters)
{
  ASSERT_EQ((string) "Device", string(m_devA->getName()));
  ASSERT_EQ((string) "1", m_devA->getId());
  ASSERT_EQ((string) "DeviceTest1", m_devA->getComponentName());
  ASSERT_EQ((string) "UnivUniqId1", m_devA->getUuid());

  ASSERT_EQ((string) "Device", string(m_devB->getName()));
  ASSERT_EQ((string) "3", m_devB->getId());
  ASSERT_EQ((string) "DeviceTest2", m_devB->getComponentName());
  ASSERT_EQ((string) "UnivUniqId2", m_devB->getUuid());
}

TEST_F(DeviceTest, Description)
{
  ErrorList errors;
  Properties psA {{"manufacturer", "MANUFACTURER"s},
                  {"serialNumber", "SERIAL_NUMBER"s},
                  {"VALUE", "Machine 1"s}};
  auto descriptionA = Device::getFactory()->create("Description", psA, errors);
  ASSERT_TRUE(errors.empty());

  m_devA->setProperty("Description", descriptionA);
  auto descA = m_devA->get<EntityPtr>("Description");

  ASSERT_EQ((string) "MANUFACTURER", descA->get<string>("manufacturer"));
  ASSERT_EQ((string) "SERIAL_NUMBER", descA->get<string>("serialNumber"));
  ASSERT_FALSE(descA->hasProperty("station"));
  ASSERT_EQ((string) "Machine 1", descA->getValue<string>());

  Properties psB {{"manufacturer", "MANUFACTURER"s},
                  {"serialNumber", "SERIAL_NUMBER"s},
                  {"VALUE", "Machine 2"s},
                  {"station", "STATION"s}};
  auto descriptionB = Device::getFactory()->create("Description", psB, errors);
  ASSERT_TRUE(errors.empty());

  m_devB->setProperty("Description", descriptionB);
  auto descB = m_devB->get<EntityPtr>("Description");

  ASSERT_EQ((string) "MANUFACTURER", descB->get<string>("manufacturer"));
  ASSERT_EQ((string) "SERIAL_NUMBER", descB->get<string>("serialNumber"));
  ASSERT_EQ((string) "STATION", descB->get<string>("station"));
  ASSERT_EQ((string) "Machine 2", descB->getValue<string>());
}

TEST_F(DeviceTest, DataItems)
{
  ASSERT_FALSE(m_devA->getDataItems());

  ErrorList errors;
  auto data1 = DataItem::make({{"id", "a"s}, {"type", "A"s}, {"category", "EVENT"s}}, errors);
  ASSERT_TRUE(errors.empty());
  auto data2 = DataItem::make({{"id", "b"s}, {"type", "A"s}, {"category", "EVENT"s}}, errors);
  ASSERT_TRUE(errors.empty());

  m_devA->addDataItem(data1, errors);
  m_devA->addDataItem(data2, errors);

  const auto &items = m_devA->getDataItems();
  ASSERT_EQ(2, items->size());

  ASSERT_TRUE(data1 == items->front());
  ASSERT_TRUE(data2 == items->back());
}

TEST_F(DeviceTest, DeviceDataItem)
{
  ASSERT_FALSE(m_devA->getDataItems());
  ASSERT_FALSE(m_devA->getDeviceDataItem("DataItemTest1"));
  ASSERT_FALSE(m_devA->getDeviceDataItem("DataItemTest2"));

  ErrorList errors;
  auto data1 =
      DataItem::make({{"id", "DataItemTest1"s}, {"type", "A"s}, {"category", "EVENT"s}}, errors);
  ASSERT_TRUE(errors.empty());
  auto data2 =
      DataItem::make({{"id", "DataItemTest2"s}, {"type", "A"s}, {"category", "EVENT"s}}, errors);
  ASSERT_TRUE(errors.empty());

  m_devA->addDataItem(data1, errors);
  m_devA->addDataItem(data2, errors);

  ASSERT_EQ((size_t)2, m_devA->getDeviceDataItems().size());
  ASSERT_TRUE(data1 == m_devA->getDeviceDataItem("DataItemTest1"));
  ASSERT_TRUE(data2 == m_devA->getDeviceDataItem("DataItemTest2"));
}

TEST_F(DeviceTest, GetDataItem)
{
  ErrorList errors;
  Properties sp1 {{"VALUE", "by_source"s}};
  auto source1 = Source::getFactory()->make("Source", sp1, errors);
  auto data1 = DataItem::make({{"id", "by_id"s},
                               {"type", "A"s},
                               {"category", "EVENT"s},
                               {"name", "by_name"s},
                               {"Source", source1}},
                              errors);
  ASSERT_TRUE(errors.empty());
  m_devA->addDataItem(data1, errors);

  Properties sp2 {{"VALUE", "by_source2"s}};
  auto source2 = Source::getFactory()->make("Source", sp2, errors);
  auto data2 = DataItem::make({{"id", "by_id2"s},
                               {"type", "A"s},
                               {"name", "by_name2"s},
                               {"category", "EVENT"s},
                               {"Source", source2}},
                              errors);
  ASSERT_TRUE(errors.empty());
  m_devA->addDataItem(data2, errors);

  Properties sp {{"VALUE", "by_source3"s}};
  auto source = Source::getFactory()->make("Source", sp, errors);
  Properties p3({{"id", "by_id3"s},
                 {"type", "A"s},
                 {"name", "by_name3"s},
                 {"category", "EVENT"s},
                 {"Source", source}});
  auto data3 = DataItem::make(p3, errors);
  ASSERT_TRUE(errors.empty());

  m_devA->addDataItem(data3, errors);

  ASSERT_TRUE(data1 == m_devA->getDeviceDataItem("by_id"));
  ASSERT_TRUE(m_devA->getDeviceDataItem("by_name"));
  ASSERT_TRUE(m_devA->getDeviceDataItem("by_source"));

  ASSERT_TRUE(data2 == m_devA->getDeviceDataItem("by_id2"));
  ASSERT_TRUE(data2 == m_devA->getDeviceDataItem("by_name2"));
  ASSERT_TRUE(m_devA->getDeviceDataItem("by_source2"));

  ASSERT_TRUE(data3 == m_devA->getDeviceDataItem("by_id3"));
  ASSERT_TRUE(data3 == m_devA->getDeviceDataItem("by_name3"));
  ASSERT_TRUE(data3 == m_devA->getDeviceDataItem("by_source3"));
}

TEST_F(DeviceTest, should_create_data_item_topic)
{
  ErrorList errors;
  auto data1 =
      DataItem::make({{"id", "id"s}, {"type", "AVAILABILITY"s}, {"category", "EVENT"s}}, errors);
  ASSERT_TRUE(errors.empty());
  m_devA->addDataItem(data1, errors);
  data1->makeTopic();

  DataItemPtr di = dynamic_pointer_cast<DataItem>(data1);

  ASSERT_EQ("UnivUniqId1/Events/Availability", di->getTopic());
  ASSERT_EQ("Availability", di->getTopicName());
}

TEST_F(DeviceTest, should_create_component_and_data_item_topic)
{
  ErrorList errors;
  auto axes = Component::make("Axes", {{"id", "ax"s}}, errors);
  ASSERT_TRUE(errors.empty());
  m_devA->addChild(axes, errors);
  ASSERT_TRUE(errors.empty());

  auto linear = Component::make("Linear", {{"id", "ax"s}, {"name", "X"s}}, errors);
  ASSERT_TRUE(errors.empty());
  axes->addChild(linear, errors);
  ASSERT_TRUE(errors.empty());

  auto data1 = DataItem::make({{"id", "id"s},
                               {"name", "Xact"s},
                               {"type", "POSITION"s},
                               {"subType", "ACTUAL"s},
                               {"category", "SAMPLE"s}},
                              errors);
  ASSERT_TRUE(errors.empty());
  linear->addDataItem(data1, errors);

  DataItemPtr di = dynamic_pointer_cast<DataItem>(data1);
  di->makeTopic();

  ASSERT_EQ("UnivUniqId1/Axes/Linear[X]/Samples/Position.Actual[Xact]", di->getTopic());
  ASSERT_EQ("Position.Actual[Xact]", di->getTopicName());
}

TEST_F(DeviceTest, should_create_topic_with_composition)
{
  ErrorList errors;
  auto axes = Component::make("Axes", {{"id", "ax"s}}, errors);
  ASSERT_TRUE(errors.empty());
  m_devA->addChild(axes, errors);
  ASSERT_TRUE(errors.empty());

  auto linear = Component::make("Linear", {{"id", "ax"s}, {"name", "X"s}}, errors);
  ASSERT_TRUE(errors.empty());
  axes->addChild(linear, errors);
  ASSERT_TRUE(errors.empty());

  auto motor = Composition::getFactory()->create(
      "Composition", {{"id", "mtr"s}, {"name", "mot"s}, {"type", "MOTOR"s}}, errors);
  ASSERT_TRUE(motor);
  ASSERT_TRUE(errors.empty());
  linear->addToList("Compositions", Component::getFactory(), motor, errors);
  ASSERT_TRUE(errors.empty());

  auto data1 = DataItem::make({{"id", "id"s},
                               {"name", "Xact"s},
                               {"type", "POSITION"s},
                               {"subType", "ACTUAL"s},
                               {"category", "SAMPLE"s},
                               {"compositionId", "mtr"s}},
                              errors);

  ASSERT_TRUE(errors.empty());
  linear->addDataItem(data1, errors);
  linear->initialize();

  auto mtr = linear->getComposition("mtr");
  ASSERT_TRUE(mtr);
  ASSERT_TRUE(mtr->getComponent());

  ASSERT_TRUE(data1->getComposition());

  DataItemPtr di = dynamic_pointer_cast<DataItem>(data1);
  di->makeTopic();

  ASSERT_EQ("UnivUniqId1/Axes/Linear[X]/Motor[mot]/Samples/Position.Actual[Xact]", di->getTopic());
}
