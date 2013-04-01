/*
* Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the AMT nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* DISCLAIMER OF WARRANTY. ALL MTCONNECT MATERIALS AND SPECIFICATIONS PROVIDED
* BY AMT, MTCONNECT OR ANY PARTICIPANT TO YOU OR ANY PARTY ARE PROVIDED "AS IS"
* AND WITHOUT ANY WARRANTY OF ANY KIND. AMT, MTCONNECT, AND EACH OF THEIR
* RESPECTIVE MEMBERS, OFFICERS, DIRECTORS, AFFILIATES, SPONSORS, AND AGENTS
* (COLLECTIVELY, THE "AMT PARTIES") AND PARTICIPANTS MAKE NO REPRESENTATION OR
* WARRANTY OF ANY KIND WHATSOEVER RELATING TO THESE MATERIALS, INCLUDING, WITHOUT
* LIMITATION, ANY EXPRESS OR IMPLIED WARRANTY OF NONINFRINGEMENT,
* MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. 

* LIMITATION OF LIABILITY. IN NO EVENT SHALL AMT, MTCONNECT, ANY OTHER AMT
* PARTY, OR ANY PARTICIPANT BE LIABLE FOR THE COST OF PROCURING SUBSTITUTE GOODS
* OR SERVICES, LOST PROFITS, LOSS OF USE, LOSS OF DATA OR ANY INCIDENTAL,
* CONSEQUENTIAL, INDIRECT, SPECIAL OR PUNITIVE DAMAGES OR OTHER DIRECT DAMAGES,
* WHETHER UNDER CONTRACT, TORT, WARRANTY OR OTHERWISE, ARISING IN ANY WAY OUT OF
* THIS AGREEMENT, USE OR INABILITY TO USE MTCONNECT MATERIALS, WHETHER OR NOT
* SUCH PARTY HAD ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.
*/

#include "device_test.hpp"

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(DeviceTest);

using namespace std;

/* DeviceTest public methods */
void DeviceTest::setUp()
{
  std::map<string, string> attributes1;
  attributes1["id"] = "1";
  attributes1["name"] = "DeviceTest1";
  attributes1["uuid"] = "UnivUniqId1";
  attributes1["iso841Class"] = "4";
  a = new Device(attributes1);
  
  std::map<string, string> attributes2;
  attributes2["id"] = "3";
  attributes2["name"] = "DeviceTest2";
  attributes2["uuid"] = "UnivUniqId2";
  attributes2["sampleRate"] = "123.4";
  attributes2["iso841Class"] = "6";
  b = new Device(attributes2);
}

void DeviceTest::tearDown()
{
  delete a;
  delete b;
}

/* DeviceTest protected methods */
void DeviceTest::testGetters()
{
  CPPUNIT_ASSERT_EQUAL((string) "Device", a->getClass());
  CPPUNIT_ASSERT_EQUAL((string) "1", a->getId());
  CPPUNIT_ASSERT_EQUAL((string) "DeviceTest1", a->getName());
  CPPUNIT_ASSERT_EQUAL((string) "UnivUniqId1", a->getUuid());
  
  CPPUNIT_ASSERT_EQUAL((string) "Device", b->getClass());
  CPPUNIT_ASSERT_EQUAL((string) "3", b->getId());
  CPPUNIT_ASSERT_EQUAL((string) "DeviceTest2", b->getName());
  CPPUNIT_ASSERT_EQUAL((string) "UnivUniqId2", b->getUuid());
}

void DeviceTest::testGetAttributes()
{
  map<string, string> &attributes1 = *a->getAttributes();
  
  CPPUNIT_ASSERT_EQUAL((string) "1",attributes1["id"]);
  CPPUNIT_ASSERT_EQUAL((string) "DeviceTest1", attributes1["name"]);
  CPPUNIT_ASSERT_EQUAL((string) "UnivUniqId1", attributes1["uuid"]);
  CPPUNIT_ASSERT(attributes1["sampleRate"].empty());
  CPPUNIT_ASSERT_EQUAL((string) "4", attributes1["iso841Class"]);
  
  map<string, string> &attributes2 = *b->getAttributes();
  
  CPPUNIT_ASSERT_EQUAL((string) "3",attributes2["id"]);
  CPPUNIT_ASSERT_EQUAL((string) "DeviceTest2", attributes2["name"]);
  CPPUNIT_ASSERT_EQUAL((string) "UnivUniqId2", attributes2["uuid"]);
  CPPUNIT_ASSERT_EQUAL((string) "123.4", attributes2["sampleInterval"]);
  CPPUNIT_ASSERT_EQUAL((string) "6", attributes2["iso841Class"]);
}

void DeviceTest::testDescription()
{
  map<string, string> attributes;
  attributes["manufacturer"] = "MANUFACTURER";
  attributes["serialNumber"] = "SERIAL_NUMBER";
  
  a->addDescription((string) "Machine 1", attributes);
  map<string, string> description1 = a->getDescription();
  
  CPPUNIT_ASSERT_EQUAL((string) "MANUFACTURER", description1["manufacturer"]);
  CPPUNIT_ASSERT_EQUAL((string) "SERIAL_NUMBER", description1["serialNumber"]);
  CPPUNIT_ASSERT(description1["station"].empty());
  
  CPPUNIT_ASSERT_EQUAL((string) "Machine 1", a->getDescriptionBody());
  
  attributes["station"] = "STATION";
  b->addDescription((string) "Machine 2", attributes);
  map<string, string> description2 = b->getDescription();
  
  CPPUNIT_ASSERT_EQUAL((string) "MANUFACTURER", description2["manufacturer"]);
  CPPUNIT_ASSERT_EQUAL((string) "SERIAL_NUMBER", description2["serialNumber"]);
  CPPUNIT_ASSERT_EQUAL((string) "STATION", description2["station"]);

  CPPUNIT_ASSERT_EQUAL((string) "Machine 2", b->getDescriptionBody());
}

void DeviceTest::testRelationships()
{
  // Test get/set parents
  map<string, string> dummy;
  Component linear("Linear", dummy);
  
  Component *devPointer = dynamic_cast<Component *>(a);
  CPPUNIT_ASSERT(devPointer);
  
  linear.setParent(*a);
  CPPUNIT_ASSERT_EQUAL(devPointer, linear.getParent());
  
  Component controller("Controller", dummy);
  controller.setParent(*a);
  CPPUNIT_ASSERT_EQUAL(devPointer, controller.getParent());
  
  // Test get device
  CPPUNIT_ASSERT_EQUAL(a, a->getDevice());
  CPPUNIT_ASSERT_EQUAL(a, linear.getDevice());
  CPPUNIT_ASSERT_EQUAL(a, controller.getDevice());
  
  // Test add/get children
  CPPUNIT_ASSERT(a->getChildren().empty());
  
  Component axes("Axes", dummy), thermostat("Thermostat", dummy);
  a->addChild(axes);
  a->addChild(thermostat);
  
  CPPUNIT_ASSERT_EQUAL((size_t) 2, a->getChildren().size());
  CPPUNIT_ASSERT_EQUAL(&axes, a->getChildren().front());
  CPPUNIT_ASSERT_EQUAL(&thermostat, a->getChildren().back());
}

void DeviceTest::testDataItems()
{
  CPPUNIT_ASSERT(a->getDataItems().empty());

  map<string, string> dummy;
  
  DataItem data1(dummy), data2(dummy);
  a->addDataItem(data1);
  a->addDataItem(data2);
  
  CPPUNIT_ASSERT_EQUAL((size_t) 2, a->getDataItems().size());
  CPPUNIT_ASSERT_EQUAL(&data1, a->getDataItems().front());
  CPPUNIT_ASSERT_EQUAL(&data2, a->getDataItems().back());
}

void DeviceTest::testDeviceDataItem()
{
  CPPUNIT_ASSERT(a->getDeviceDataItems().empty());
  CPPUNIT_ASSERT(a->getDeviceDataItem("DataItemTest1") == NULL);
  CPPUNIT_ASSERT(a->getDeviceDataItem("DataItemTest2") == NULL);

  map<string, string> attributes;
  attributes["id"] = "DataItemTest1";
  
  DataItem data1(attributes);
  a->addDeviceDataItem(data1);
  
  attributes["id"] = "DataItemTest2";
  DataItem data2(attributes);
  a->addDeviceDataItem(data2);
  
  CPPUNIT_ASSERT_EQUAL((size_t) 2, a->getDeviceDataItems().size());
  CPPUNIT_ASSERT_EQUAL(&data1, a->getDeviceDataItem("DataItemTest1"));
  CPPUNIT_ASSERT_EQUAL(&data2, a->getDeviceDataItem("DataItemTest2"));
}

void DeviceTest::testGetDataItem()
{
  map<string, string> attributes;
  attributes["id"] = "by_id";
  DataItem data1(attributes);
  a->addDeviceDataItem(data1);
  
  map<string, string> attributes2;
  attributes2["id"] = "by_id2";
  attributes2["name"] = "by_name2";
  DataItem data2(attributes2);
  a->addDeviceDataItem(data2);
  
  map<string, string> attributes3;
  attributes3["id"] = "by_id3";
  attributes3["name"] = "by_name3";
  DataItem data3(attributes3);
  data3.addSource("by_source3");
  a->addDeviceDataItem(data3);
  
  
  CPPUNIT_ASSERT_EQUAL(&data1, a->getDeviceDataItem("by_id"));
  CPPUNIT_ASSERT_EQUAL((DataItem*) 0, a->getDeviceDataItem("by_name"));
  CPPUNIT_ASSERT_EQUAL((DataItem*) 0, a->getDeviceDataItem("by_source"));
  
  CPPUNIT_ASSERT_EQUAL(&data2, a->getDeviceDataItem("by_id2"));
  CPPUNIT_ASSERT_EQUAL(&data2, a->getDeviceDataItem("by_name2"));
  CPPUNIT_ASSERT_EQUAL((DataItem*) 0, a->getDeviceDataItem("by_source2"));

  CPPUNIT_ASSERT_EQUAL(&data3, a->getDeviceDataItem("by_id3"));
  CPPUNIT_ASSERT_EQUAL(&data3, a->getDeviceDataItem("by_name3"));
  CPPUNIT_ASSERT_EQUAL(&data3, a->getDeviceDataItem("by_source3"));
}

