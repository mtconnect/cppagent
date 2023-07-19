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

#include "mtconnect/device_model/component.hpp"
#include "mtconnect/device_model/device.hpp"

using namespace std;
using namespace mtconnect;
using namespace device_model;
using namespace data_item;
using namespace entity;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class ComponentTest : public testing::Test
{
protected:
  void SetUp() override
  {
    ErrorList errors;

    m_compA = Component::make("Axes",
                              {{"id", "1"s},
                               {"name", "ComponentTest1"s},
                               {"nativeName", "NativeName"s},
                               {"uuid", "UnivUniqId1"s}},
                              errors);

    m_compB = Component::make("Controller",
                              {{"id", "3"s},
                               {"name", "ComponentTest2"s},
                               {"uuid", "UnivUniqId2"s},
                               {"sampleRate", "123.4"s}},
                              errors);
  }

  void TearDown() override
  {
    m_compA.reset();
    m_compB.reset();
  }

  ComponentPtr m_compA;
  ComponentPtr m_compB;
};

TEST_F(ComponentTest, Getters)
{
  ASSERT_EQ((string) "Axes", string(m_compA->getName()));
  ASSERT_EQ((string) "1", m_compA->getId());
  ASSERT_EQ((string) "ComponentTest1", m_compA->get<string>("name"));
  ASSERT_EQ((string) "UnivUniqId1", m_compA->getUuid());
  ASSERT_EQ((string) "NativeName", m_compA->get<string>("nativeName"));

  ASSERT_EQ((string) "Controller", string(m_compB->getName()));
  ASSERT_EQ((string) "3", m_compB->getId());
  ASSERT_EQ((string) "ComponentTest2", m_compB->get<string>("name"));
  ASSERT_EQ((string) "UnivUniqId2", m_compB->getUuid());
  ASSERT_FALSE(m_compB->hasProperty("nativeName"));
}

// Workning our way through
TEST_F(ComponentTest, Description)
{
  map<string, string> attributes;
  attributes["manufacturer"] = "MANUFACTURER";
  attributes["serialNumber"] = "SERIAL_NUMBER";

  m_compA->setManufacturer("MANUFACTURER");
  m_compA->setSerialNumber("SERIAL_NUMBER");
  m_compA->setDescriptionValue("Machine 1");

  auto d1 = m_compA->getDescription();

  ASSERT_EQ((string) "MANUFACTURER", d1->get<string>("manufacturer"));
  ASSERT_EQ((string) "SERIAL_NUMBER", d1->get<string>("serialNumber"));
  ASSERT_FALSE(d1->hasProperty("station"));
  ASSERT_EQ((string) "Machine 1", d1->getValue<string>());

  m_compB->setManufacturer("MANUFACTURER");
  m_compB->setSerialNumber("SERIAL_NUMBER");
  m_compB->setStation("STATION");
  auto d2 = m_compB->getDescription();

  ASSERT_EQ((string) "MANUFACTURER", d2->get<string>("manufacturer"));
  ASSERT_EQ((string) "SERIAL_NUMBER", d2->get<string>("serialNumber"));
  ASSERT_EQ((string) "STATION", d2->get<string>("station"));
  ASSERT_FALSE(d2->hasValue());
}

TEST_F(ComponentTest, Relationships)
{
  // Test get/set parents
  ErrorList errors;

  auto linear = Component::make("Linear", {{"id", "x"s}}, errors);
  ASSERT_TRUE(errors.empty());
  linear->addChild(m_compA, errors);
  ASSERT_TRUE(errors.empty());
  ASSERT_EQ(linear, m_compA->getParent());

  Properties dps {{"id", "d"s}, {"name", "d"s}, {"uuid", "d"s}};
  auto device = dynamic_pointer_cast<Device>(Device::getFactory()->make("Device", dps, errors));
  ASSERT_TRUE(errors.empty());
  ASSERT_TRUE(device);
  device->addChild(linear, errors);
  ASSERT_TRUE(device == linear->getParent());

  // Test get device
  ASSERT_TRUE(device == m_compA->getDevice());
  ASSERT_TRUE(device == linear->getDevice());
  ASSERT_TRUE(device == device->getDevice());

  // Test add/get children
  ASSERT_FALSE(m_compA->getChildren());

  ASSERT_EQ(1, device.use_count());
  ASSERT_EQ(2, linear.use_count());
  ASSERT_EQ(2, m_compA.use_count());
}

TEST_F(ComponentTest, DataItems)
{
  ASSERT_FALSE(m_compA->getDataItems());

  ErrorList errors;
  auto data1 = DataItem::make({{"id", "a"s}, {"type", "A"s}, {"category", "EVENT"s}}, errors);
  ASSERT_TRUE(errors.empty());
  auto data2 = DataItem::make({{"id", "b"s}, {"type", "A"s}, {"category", "EVENT"s}}, errors);
  ASSERT_TRUE(errors.empty());
  m_compA->addDataItem(data1, errors);
  ASSERT_TRUE(errors.empty());
  m_compA->addDataItem(data2, errors);
  ASSERT_TRUE(errors.empty());

  auto dataItems = m_compA->getDataItems();
  ASSERT_TRUE(m_compA->getDataItems());

  ASSERT_EQ((size_t)2, dataItems->size());
  ASSERT_EQ(data1, dataItems->front());
  ASSERT_EQ(data2, dataItems->back());
}

// Not relevant right now. Tested as references in other places
#if 0
TEST_F(ComponentTest, References)
{
  
  
  string id("a"), name("xxx");
  mtconnect::Component::Reference ref(id, name, mtconnect::Component::Reference::DATA_ITEM);

  m_compA->addReference(ref);
  ASSERT_EQ((size_t)1, m_compA->getReferences().size());

  ASSERT_EQ((string) "xxx", m_compA->getReferences().front().m_name);
  ASSERT_EQ((string) "a", m_compA->getReferences().front().m_id);
}
#endif
