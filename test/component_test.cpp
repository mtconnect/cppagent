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

#include "device_model/component.hpp"
#include "device_model/device.hpp"

using namespace std;
using namespace mtconnect;
using namespace device_model;
using namespace data_item;
using namespace entity;

class ComponentTest : public testing::Test
{
 protected:
  void SetUp() override
  {
    ErrorList errors;
    
    m_compA = Component::make("Axes", {{"id", "1"s}, {"name", "ComponentTest1"s},
      {"nativeName", "NativeName"s}, {"uuid", "UnivUniqId1"s}
    }, errors);

    m_compB = Component::make("Controller", {{"id", "3"s}, {"name", "ComponentTest2"s},
      {"uuid", "UnivUniqId2"s},
      {"sampleRate", "123.4"s}
    }, errors);
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

#if 0
TEST_F(ComponentTest, Description)
{
  map<string, string> attributes;
  attributes["manufacturer"] = "MANUFACTURER";
  attributes["serialNumber"] = "SERIAL_NUMBER";

  m_compA->addDescription((string) "Machine 1", attributes);
  auto description1 = m_compA->getDescription();

  ASSERT_EQ((string) "MANUFACTURER", description1["manufacturer"]);
  ASSERT_EQ((string) "SERIAL_NUMBER", description1["serialNumber"]);
  ASSERT_TRUE(description1["station"].empty());
  ASSERT_EQ((string) "Machine 1", m_compA->getDescriptionBody());

  attributes["station"] = "STATION";
  m_compB->addDescription((string) "", attributes);
  auto description2 = m_compB->getDescription();

  ASSERT_EQ((string) "MANUFACTURER", description2["manufacturer"]);
  ASSERT_EQ((string) "SERIAL_NUMBER", description2["serialNumber"]);
  ASSERT_EQ((string) "STATION", description2["station"]);
  ASSERT_TRUE(m_compB->getDescriptionBody().empty());
}

TEST_F(ComponentTest, Relationships)
{
  // Test get/set parents
  map<string, string> dummy;
  auto *linear = new mtconnect::Component("Linear", dummy);
  linear->addChild(m_compA);
  ASSERT_TRUE(linear == m_compA->getParent());

  auto *device = new Device(dummy);
  auto devPointer = dynamic_cast<mtconnect::Component *>(device);

  ASSERT_TRUE(devPointer);
  devPointer->addChild(linear);
  ASSERT_TRUE(devPointer == linear->getParent());

  // Test get device
  ASSERT_TRUE(device == m_compA->getDevice());
  ASSERT_TRUE(device == linear->getDevice());
  ASSERT_TRUE(device == device->getDevice());

  // Test add/get children
  ASSERT_TRUE(m_compA->getChildren().empty());

  auto *axes = new mtconnect::Component("Axes", dummy),
       *thermostat = new mtconnect::Component("Thermostat", dummy);
  m_compA->addChild(axes);
  m_compA->addChild(thermostat);

  ASSERT_EQ((size_t)2, m_compA->getChildren().size());
  ASSERT_TRUE(axes == m_compA->getChildren().front());
  ASSERT_TRUE(thermostat == m_compA->getChildren().back());

  delete device;

  m_compA = nullptr;
  m_compB = nullptr;
}

TEST_F(ComponentTest, DataItems)
{
  ASSERT_TRUE(m_compA->getDataItems().empty());

  ErrorList errors;
  auto data1 = DataItem::make({{"id", "a"s}, {"type", "A"s}, {"category", "EVENT"s}}, errors);
  ASSERT_TRUE(errors.empty());
  auto data2 = DataItem::make({{"id", "b"s}, {"type", "A"s}, {"category", "EVENT"s}}, errors);
  ASSERT_TRUE(errors.empty());
  m_compA->addDataItem(data1);
  m_compA->addDataItem(data2);

  ASSERT_EQ((size_t)2, m_compA->getDataItems().size());
  ASSERT_TRUE(data1 == m_compA->getDataItems().front());
  ASSERT_TRUE(data2 == m_compA->getDataItems().back());
}

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
