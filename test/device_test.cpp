//
// Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the AMT nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// DISCLAIMER OF WARRANTY. ALL MTCONNECT MATERIALS AND SPECIFICATIONS PROVIDED
// BY AMT, MTCONNECT OR ANY PARTICIPANT TO YOU OR ANY PARTY ARE PROVIDED "AS IS"
// AND WITHOUT ANY WARRANTY OF ANY KIND. AMT, MTCONNECT, AND EACH OF THEIR
// RESPECTIVE MEMBERS, OFFICERS, DIRECTORS, AFFILIATES, SPONSORS, AND AGENTS
// (COLLECTIVELY, THE "AMT PARTIES") AND PARTICIPANTS MAKE NO REPRESENTATION OR
// WARRANTY OF ANY KIND WHATSOEVER RELATING TO THESE MATERIALS, INCLUDING, WITHOUT
// LIMITATION, ANY EXPRESS OR IMPLIED WARRANTY OF NONINFRINGEMENT,
// MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
//
// LIMITATION OF LIABILITY. IN NO EVENT SHALL AMT, MTCONNECT, ANY OTHER AMT
// PARTY, OR ANY PARTICIPANT BE LIABLE FOR THE COST OF PROCURING SUBSTITUTE GOODS
// OR SERVICES, LOST PROFITS, LOSS OF USE, LOSS OF DATA OR ANY INCIDENTAL,
// CONSEQUENTIAL, INDIRECT, SPECIAL OR PUNITIVE DAMAGES OR OTHER DIRECT DAMAGES,
// WHETHER UNDER CONTRACT, TORT, WARRANTY OR OTHERWISE, ARISING IN ANY WAY OUT OF
// THIS AGREEMENT, USE OR INABILITY TO USE MTCONNECT MATERIALS, WHETHER OR NOT
// SUCH PARTY HAD ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.
//
#include "device_test.hpp"

using namespace std;

namespace mtconnect {
  namespace test {
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
      Component linear("Linear", dummy);
      
      auto devPointer = dynamic_cast<Component *>(m_devA);
      CPPUNIT_ASSERT(devPointer);
      
      linear.setParent(*m_devA);
      CPPUNIT_ASSERT_EQUAL(devPointer, linear.getParent());
      
      Component controller("Controller", dummy);
      controller.setParent(*m_devA);
      CPPUNIT_ASSERT_EQUAL(devPointer, controller.getParent());
      
      // Test get device
      CPPUNIT_ASSERT_EQUAL(m_devA, m_devA->getDevice());
      CPPUNIT_ASSERT_EQUAL(m_devA, linear.getDevice());
      CPPUNIT_ASSERT_EQUAL(m_devA, controller.getDevice());
      
      // Test add/get children
      CPPUNIT_ASSERT(m_devA->getChildren().empty());
      
      Component axes("Axes", dummy), thermostat("Thermostat", dummy);
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
  }
}
