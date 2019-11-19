//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "component.hpp"
#include "device.hpp"

using namespace std;
using namespace mtconnect;

class ComponentTest : public testing::Test
{
 protected:
  void SetUp() override
  {
    std::map<string, string> attributes1;
    attributes1["id"] = "1";
    attributes1["name"] = "ComponentTest1";
    attributes1["nativeName"] = "NativeName";
    attributes1["uuid"] = "UnivUniqId1";
    m_compA = new mtconnect::Component("Axes", attributes1);

    std::map<string, string> attributes2;
    attributes2["id"] = "3";
    attributes2["name"] = "ComponentTest2";
    attributes2["uuid"] = "UnivUniqId2";
    attributes2["sampleRate"] = "123.4";
    m_compB = new mtconnect::Component("Controller", attributes2);
  }

  void TearDown() override
  {
    if (m_compA != nullptr)
    {
      delete m_compA;
      m_compA = nullptr;
    }
    if (m_compB != nullptr)
    {
      delete m_compB;
      m_compB = nullptr;
    }
  }

  mtconnect::Component *m_compA;
  mtconnect::Component *m_compB;
};

TEST_F(ComponentTest, Getters)
{
  ASSERT_EQ((string) "Axes", m_compA->getClass());
  ASSERT_EQ((string) "1", m_compA->getId());
  ASSERT_EQ((string) "ComponentTest1", m_compA->getName());
  ASSERT_EQ((string) "UnivUniqId1", m_compA->getUuid());
  ASSERT_EQ((string) "NativeName", m_compA->getNativeName());

  ASSERT_EQ((string) "Controller", m_compB->getClass());
  ASSERT_EQ((string) "3", m_compB->getId());
  ASSERT_EQ((string) "ComponentTest2", m_compB->getName());
  ASSERT_EQ((string) "UnivUniqId2", m_compB->getUuid());
  ASSERT_TRUE(m_compB->getNativeName().empty());
}

TEST_F(ComponentTest, GetAttributes)
{
  const auto &attributes1 = m_compA->getAttributes();

  ASSERT_EQ((string) "1", attributes1.at("id"));
  ASSERT_EQ((string) "ComponentTest1", attributes1.at("name"));
  ASSERT_EQ((string) "UnivUniqId1", attributes1.at("uuid"));
  ASSERT_TRUE(attributes1.find("sampleRate") == attributes1.end());

  const auto &attributes2 = m_compB->getAttributes();

  ASSERT_EQ((string) "3", attributes2.at("id"));
  ASSERT_EQ((string) "ComponentTest2", attributes2.at("name"));
  ASSERT_EQ((string) "UnivUniqId2", attributes2.at("uuid"));
  ASSERT_EQ((string) "123.4", attributes2.at("sampleInterval"));
}

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
  mtconnect::Component *linear = new mtconnect::Component("Linear", dummy);

  m_compA->setParent(*linear);
  ASSERT_TRUE(linear == m_compA->getParent());

  Device *device = new Device(dummy);
  auto devPointer = dynamic_cast<mtconnect::Component *>(device);

  ASSERT_TRUE(devPointer);
  linear->setParent(*devPointer);
  ASSERT_TRUE(devPointer == linear->getParent());

  // Test get device
  ASSERT_TRUE(device == m_compA->getDevice());
  ASSERT_TRUE(device == linear->getDevice());
  ASSERT_TRUE(device == device->getDevice());

  // Test add/get children
  ASSERT_TRUE(m_compA->getChildren().empty());

  mtconnect::Component *axes = new mtconnect::Component("Axes", dummy),
                       *thermostat = new mtconnect::Component("Thermostat", dummy);
  m_compA->addChild(*axes);
  m_compA->addChild(*thermostat);

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

  map<string, string> dummy;

  DataItem *data1 = new DataItem(dummy), *data2 = new DataItem(dummy);
  m_compA->addDataItem(*data1);
  m_compA->addDataItem(*data2);

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
