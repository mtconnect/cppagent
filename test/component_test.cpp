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

#include "component_test.hpp"

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(ComponentTest);

using namespace std;

/* ComponentTest public methods */
void ComponentTest::setUp()
{
  std::map<string, string> attributes1;
  attributes1["id"] = "1";
  attributes1["name"] = "ComponentTest1";
  attributes1["uuid"] = "UnivUniqId1";
  a = new Component("Axes", attributes1);
  
  std::map<string, string> attributes2;
  attributes2["id"] = "3";
  attributes2["name"] = "ComponentTest2";
  attributes2["uuid"] = "UnivUniqId2";
  attributes2["sampleRate"] = "123.4";
  b = new Component("Controller", attributes2);
}

void ComponentTest::tearDown()
{
  delete a;
  delete b;
}

/* ComponentTest protected methods */
void ComponentTest::testGetters()
{
  CPPUNIT_ASSERT_EQUAL((string) "Axes", a->getClass());
  CPPUNIT_ASSERT_EQUAL((string) "1", a->getId());
  CPPUNIT_ASSERT_EQUAL((string) "ComponentTest1", a->getName());
  CPPUNIT_ASSERT_EQUAL((string) "UnivUniqId1", a->getUuid());
  
  CPPUNIT_ASSERT_EQUAL((string) "Controller", b->getClass());
  CPPUNIT_ASSERT_EQUAL((string) "3", b->getId());
  CPPUNIT_ASSERT_EQUAL((string) "ComponentTest2", b->getName());
  CPPUNIT_ASSERT_EQUAL((string) "UnivUniqId2", b->getUuid());
}

void ComponentTest::testGetAttributes()
{
  map<string, string> attributes1 = a->getAttributes();
  
  CPPUNIT_ASSERT_EQUAL((string) "1",attributes1["id"]);
  CPPUNIT_ASSERT_EQUAL((string) "ComponentTest1", attributes1["name"]);
  CPPUNIT_ASSERT_EQUAL((string) "UnivUniqId1", attributes1["uuid"]);
  CPPUNIT_ASSERT(attributes1["sampleRate"].empty());
  
  map<string, string> attributes2 = b->getAttributes();
  
  CPPUNIT_ASSERT_EQUAL((string) "3",attributes2["id"]);
  CPPUNIT_ASSERT_EQUAL((string) "ComponentTest2", attributes2["name"]);
  CPPUNIT_ASSERT_EQUAL((string) "UnivUniqId2", attributes2["uuid"]);
  CPPUNIT_ASSERT_EQUAL((string) "123.4", attributes2["sampleRate"]);
}

void ComponentTest::testDescription()
{
  map<string, string> attributes;
  attributes["manufacturer"] = "MANUFACTURER";
  attributes["serialNumber"] = "SERIAL_NUMBER";
  
  a->addDescription(attributes);
  map<string, string> description1 = a->getDescription();
  
  CPPUNIT_ASSERT_EQUAL((string) "MANUFACTURER", description1["manufacturer"]);
  CPPUNIT_ASSERT_EQUAL((string) "SERIAL_NUMBER", description1["serialNumber"]);
  CPPUNIT_ASSERT(description1["station"].empty());
  
  attributes["station"] = "STATION";
  b->addDescription(attributes);
  map<string, string> description2 = b->getDescription();
  
  CPPUNIT_ASSERT_EQUAL((string) "MANUFACTURER", description2["manufacturer"]);
  CPPUNIT_ASSERT_EQUAL((string) "SERIAL_NUMBER", description2["serialNumber"]);
  CPPUNIT_ASSERT_EQUAL((string) "STATION", description2["station"]);
}

void ComponentTest::testRelationships()
{
  // Test get/set parents
  map<string, string> dummy;
  Component linear("Linear", dummy);
  
  a->setParent(linear);
  CPPUNIT_ASSERT_EQUAL(&linear, a->getParent());
  
  Device device(dummy);
  Component * devPointer = dynamic_cast<Component *>(&device);
  
  CPPUNIT_ASSERT(devPointer);  
  linear.setParent(*devPointer);
  CPPUNIT_ASSERT_EQUAL(devPointer, linear.getParent());
  
  // Test get device
  CPPUNIT_ASSERT_EQUAL(&device, a->getDevice());
  CPPUNIT_ASSERT_EQUAL(&device, linear.getDevice());
  CPPUNIT_ASSERT_EQUAL(&device, device.getDevice());
  
  // Test add/get children
  CPPUNIT_ASSERT(a->getChildren().empty());
  
  Component axes("Axes", dummy), thermostat("Thermostat", dummy);
  a->addChild(axes);
  a->addChild(thermostat);
  
  CPPUNIT_ASSERT_EQUAL((size_t) 2, a->getChildren().size());
  CPPUNIT_ASSERT_EQUAL(&axes, a->getChildren().front());
  CPPUNIT_ASSERT_EQUAL(&thermostat, a->getChildren().back());
}

void ComponentTest::testDataItems()
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

