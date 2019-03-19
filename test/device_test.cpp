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

#include "Cuti.h"
#include "device.hpp"

using namespace std;
using namespace mtconnect;


TEST_CLASS(DeviceTest)
{
protected:
  Device *m_devA, *m_devB;
  
public:
  void testGetters();
  void testGetAttributes();
  void testDescription();
  void testRelationships();
  void testDataItems();
  void testDeviceDataItem();
  void testGetDataItem();
  
  SET_UP();
  TEAR_DOWN();
  
  CPPUNIT_TEST_SUITE(DeviceTest);
  CPPUNIT_TEST(testGetters);
  CPPUNIT_TEST(testGetAttributes);
  CPPUNIT_TEST(testDescription);
  CPPUNIT_TEST(testRelationships);
  CPPUNIT_TEST(testDataItems);
  CPPUNIT_TEST(testDeviceDataItem);
  CPPUNIT_TEST(testGetDataItem);
  CPPUNIT_TEST_SUITE_END();
};


// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(DeviceTest);

void DeviceTest::setUp()
{
  std::map<string, string> attributes1;
  attributes1["id"] = "1";
  attributes1["name"] = "DeviceTest1";
  attributes1["uuid"] = "UnivUniqId1";
  attributes1["iso841Class"] = "4";
  m_devA = new Device(attributes1);
  
  std::map<string, string> attributes2;
  attributes2["id"] = "3";
  attributes2["name"] = "DeviceTest2";
  attributes2["uuid"] = "UnivUniqId2";
  attributes2["sampleRate"] = "123.4";
  attributes2["iso841Class"] = "6";
  m_devB = new Device(attributes2);
}


void DeviceTest::tearDown()
{
  delete m_devA; m_devA = nullptr;
  delete m_devB; m_devB = nullptr;
}


void DeviceTest::testGetters()
{
  CPPUNIT_ASSERT_EQUAL((string) "Device", m_devA->getClass());
  CPPUNIT_ASSERT_EQUAL((string) "1", m_devA->getId());
  CPPUNIT_ASSERT_EQUAL((string) "DeviceTest1", m_devA->getName());
  CPPUNIT_ASSERT_EQUAL((string) "UnivUniqId1", m_devA->getUuid());
  
  CPPUNIT_ASSERT_EQUAL((string) "Device", m_devB->getClass());
  CPPUNIT_ASSERT_EQUAL((string) "3", m_devB->getId());
  CPPUNIT_ASSERT_EQUAL((string) "DeviceTest2", m_devB->getName());
  CPPUNIT_ASSERT_EQUAL((string) "UnivUniqId2", m_devB->getUuid());
}


void DeviceTest::testGetAttributes()
{
  const auto &attributes1 = m_devA->getAttributes();
  
  CPPUNIT_ASSERT_EQUAL((string) "1", attributes1.at("id"));
  CPPUNIT_ASSERT_EQUAL((string) "DeviceTest1", attributes1.at("name"));
  CPPUNIT_ASSERT_EQUAL((string) "UnivUniqId1", attributes1.at("uuid"));
  CPPUNIT_ASSERT(attributes1.find("sampleRate") == attributes1.end());
  CPPUNIT_ASSERT_EQUAL((string) "4", attributes1.at("iso841Class"));
  
  const auto &attributes2 = m_devB->getAttributes();
  
  CPPUNIT_ASSERT_EQUAL((string) "3", attributes2.at("id"));
  CPPUNIT_ASSERT_EQUAL((string) "DeviceTest2", attributes2.at("name"));
  CPPUNIT_ASSERT_EQUAL((string) "UnivUniqId2", attributes2.at("uuid"));
  CPPUNIT_ASSERT_EQUAL((string) "123.4", attributes2.at("sampleInterval"));
  CPPUNIT_ASSERT_EQUAL((string) "6", attributes2.at("iso841Class"));
}


void DeviceTest::testDescription()
{
  map<string, string> attributes;
  attributes["manufacturer"] = "MANUFACTURER";
  attributes["serialNumber"] = "SERIAL_NUMBER";
  
  m_devA->addDescription((string) "Machine 1", attributes);
  auto description1 = m_devA->getDescription();
  
  CPPUNIT_ASSERT_EQUAL((string) "MANUFACTURER", description1["manufacturer"]);
  CPPUNIT_ASSERT_EQUAL((string) "SERIAL_NUMBER", description1["serialNumber"]);
  CPPUNIT_ASSERT(description1["station"].empty());
  
  CPPUNIT_ASSERT_EQUAL((string) "Machine 1", m_devA->getDescriptionBody());
  
  attributes["station"] = "STATION";
  m_devB->addDescription((string) "Machine 2", attributes);
  auto description2 = m_devB->getDescription();
  
  CPPUNIT_ASSERT_EQUAL((string) "MANUFACTURER", description2["manufacturer"]);
  CPPUNIT_ASSERT_EQUAL((string) "SERIAL_NUMBER", description2["serialNumber"]);
  CPPUNIT_ASSERT_EQUAL((string) "STATION", description2["station"]);
  
  CPPUNIT_ASSERT_EQUAL((string) "Machine 2", m_devB->getDescriptionBody());
}


void DeviceTest::testRelationships()
{
  // Test get/set parents
  map<string, string> dummy;
  mtconnect::Component linear("Linear", dummy);
  
  auto devPointer = dynamic_cast<mtconnect::Component *>(m_devA);
  CPPUNIT_ASSERT(devPointer);
  
  linear.setParent(*m_devA);
  CPPUNIT_ASSERT_EQUAL(devPointer, linear.getParent());
  
  mtconnect::Component controller("Controller", dummy);
  controller.setParent(*m_devA);
  CPPUNIT_ASSERT_EQUAL(devPointer, controller.getParent());
  
  // Test get device
  CPPUNIT_ASSERT_EQUAL(m_devA, m_devA->getDevice());
  CPPUNIT_ASSERT_EQUAL(m_devA, linear.getDevice());
  CPPUNIT_ASSERT_EQUAL(m_devA, controller.getDevice());
  
  // Test add/get children
  CPPUNIT_ASSERT(m_devA->getChildren().empty());
  
  mtconnect::Component axes("Axes", dummy), thermostat("Thermostat", dummy);
  m_devA->addChild(axes);
  m_devA->addChild(thermostat);
  
  CPPUNIT_ASSERT_EQUAL((size_t) 2, m_devA->getChildren().size());
  CPPUNIT_ASSERT_EQUAL(&axes, m_devA->getChildren().front());
  CPPUNIT_ASSERT_EQUAL(&thermostat, m_devA->getChildren().back());
}


void DeviceTest::testDataItems()
{
  CPPUNIT_ASSERT(m_devA->getDataItems().empty());
  
  map<string, string> dummy;
  
  DataItem data1(dummy), data2(dummy);
  m_devA->addDataItem(data1);
  m_devA->addDataItem(data2);
  
  CPPUNIT_ASSERT_EQUAL((size_t) 2, m_devA->getDataItems().size());
  CPPUNIT_ASSERT_EQUAL(&data1, m_devA->getDataItems().front());
  CPPUNIT_ASSERT_EQUAL(&data2, m_devA->getDataItems().back());
}


void DeviceTest::testDeviceDataItem()
{
  CPPUNIT_ASSERT(m_devA->getDeviceDataItems().empty());
  CPPUNIT_ASSERT(!m_devA->getDeviceDataItem("DataItemTest1"));
  CPPUNIT_ASSERT(!m_devA->getDeviceDataItem("DataItemTest2"));
  
  map<string, string> attributes;
  attributes["id"] = "DataItemTest1";
  
  DataItem data1(attributes);
  m_devA->addDeviceDataItem(data1);
  
  attributes["id"] = "DataItemTest2";
  DataItem data2(attributes);
  m_devA->addDeviceDataItem(data2);
  
  CPPUNIT_ASSERT_EQUAL((size_t) 2, m_devA->getDeviceDataItems().size());
  CPPUNIT_ASSERT_EQUAL(&data1, m_devA->getDeviceDataItem("DataItemTest1"));
  CPPUNIT_ASSERT_EQUAL(&data2, m_devA->getDeviceDataItem("DataItemTest2"));
}


void DeviceTest::testGetDataItem()
{
  map<string, string> attributes;
  attributes["id"] = "by_id";
  DataItem data1(attributes);
  m_devA->addDeviceDataItem(data1);
  
  map<string, string> attributes2;
  attributes2["id"] = "by_id2";
  attributes2["name"] = "by_name2";
  DataItem data2(attributes2);
  m_devA->addDeviceDataItem(data2);
  
  map<string, string> attributes3;
  attributes3["id"] = "by_id3";
  attributes3["name"] = "by_name3";
  DataItem data3(attributes3);
  data3.addSource("by_source3", "", "", "");
  m_devA->addDeviceDataItem(data3);
  
  
  CPPUNIT_ASSERT_EQUAL(&data1, m_devA->getDeviceDataItem("by_id"));
  CPPUNIT_ASSERT_EQUAL((DataItem *)nullptr, m_devA->getDeviceDataItem("by_name"));
  CPPUNIT_ASSERT_EQUAL((DataItem *)nullptr, m_devA->getDeviceDataItem("by_source"));
  
  CPPUNIT_ASSERT_EQUAL(&data2, m_devA->getDeviceDataItem("by_id2"));
  CPPUNIT_ASSERT_EQUAL(&data2, m_devA->getDeviceDataItem("by_name2"));
  CPPUNIT_ASSERT_EQUAL((DataItem *)nullptr, m_devA->getDeviceDataItem("by_source2"));
  
  CPPUNIT_ASSERT_EQUAL(&data3, m_devA->getDeviceDataItem("by_id3"));
  CPPUNIT_ASSERT_EQUAL(&data3, m_devA->getDeviceDataItem("by_name3"));
  CPPUNIT_ASSERT_EQUAL(&data3, m_devA->getDeviceDataItem("by_source3"));
}
