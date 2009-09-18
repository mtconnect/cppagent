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

#include "xml_printer_test.hpp"

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(XmlPrinterTest);

using namespace std;

void XmlPrinterTest::setUp()
{
  config = new XmlParser("../samples/test_config.xml");
  devices = config->getDevices();
}

void XmlPrinterTest::tearDown()
{
  delete config;
}

void XmlPrinterTest::testInitXmlDoc()
{
  // Devices
  xmlpp::Document *initXml1 =
    XmlPrinter::initXmlDoc("Devices", 12345, 100000, 10);
  
  // Root
  xmlpp::Element *root1 = initXml1->get_root_node();
  CPPUNIT_ASSERT_EQUAL(
    (Glib::ustring) "urn:mtconnect.com:MTConnectDevices:1.0",
    root1->get_attribute_value("xmlns:m")
  );
  CPPUNIT_ASSERT_EQUAL(
    (Glib::ustring) "http://www.w3.org/2001/XMLSchema-instance",
    root1->get_attribute_value("xmlns:xsi")
  );
  CPPUNIT_ASSERT_EQUAL(
    (Glib::ustring) "urn:mtconnect.com:MTConnectDevices:1.0",
    root1->get_attribute_value("xmlns")
  );
  CPPUNIT_ASSERT_EQUAL(
    (Glib::ustring) "urn:mtconnect.com:MTConnectDevices:1.0"
      + " /schemas/MTConnectDevices.xsd",
    root1->get_attribute_value("xsi:schemaLocation")
  );
  
  // Header
  xmlpp::Node::NodeList rootChildren1 = root1->get_children("Header");
  CPPUNIT_ASSERT_EQUAL((size_t) 1, rootChildren1.size());
  
  xmlpp::Element *header1 =
    dynamic_cast<xmlpp::Element *>(rootChildren1.front());
  CPPUNIT_ASSERT(header1);
  
  CPPUNIT_ASSERT_EQUAL((Glib::ustring) "localhost",
    header1->get_attribute_value("sender"));
  CPPUNIT_ASSERT_EQUAL((Glib::ustring) "12345",
    header1->get_attribute_value("instanceId"));
  CPPUNIT_ASSERT_EQUAL((Glib::ustring) "100000",
    header1->get_attribute_value("bufferSize"));
  CPPUNIT_ASSERT_EQUAL((Glib::ustring) "1.0",
    header1->get_attribute_value("version"));
  
  CPPUNIT_ASSERT(header1->get_attribute_value("nextSequence").empty());
  CPPUNIT_ASSERT(header1->get_attribute_value("firstSequence").empty());
  CPPUNIT_ASSERT(header1->get_attribute_value("lastSequence").empty());
  
  // Streams
  xmlpp::Document *initXml2 =
    XmlPrinter::initXmlDoc("Streams", 54321, 100000, 10, 1);
  
  // Root
  xmlpp::Element *root2 = initXml2->get_root_node();
  CPPUNIT_ASSERT_EQUAL(
    (Glib::ustring) "urn:mtconnect.com:MTConnectStreams:1.0",
    root2->get_attribute_value("xmlns:m")
  );
  CPPUNIT_ASSERT_EQUAL(
    (Glib::ustring) "http://www.w3.org/2001/XMLSchema-instance",
    root2->get_attribute_value("xmlns:xsi")
  );
  CPPUNIT_ASSERT_EQUAL(
    (Glib::ustring) "urn:mtconnect.com:MTConnectStreams:1.0",
    root2->get_attribute_value("xmlns")
  );
  CPPUNIT_ASSERT_EQUAL(
    (Glib::ustring) "urn:mtconnect.com:MTConnectStreams:1.0"
      + " /schemas/MTConnectStreams.xsd",
    root2->get_attribute_value("xsi:schemaLocation")
  );
  
  // Header
  xmlpp::Node::NodeList rootChildren2 = root2->get_children("Header");
  CPPUNIT_ASSERT_EQUAL((size_t) 1, rootChildren2.size());
  
  xmlpp::Element *header2 =
    dynamic_cast<xmlpp::Element *>(rootChildren2.front());
  CPPUNIT_ASSERT(header2);
  
  CPPUNIT_ASSERT_EQUAL((Glib::ustring) "localhost",
    header2->get_attribute_value("sender"));
  CPPUNIT_ASSERT_EQUAL((Glib::ustring) "54321",
    header2->get_attribute_value("instanceId"));
  CPPUNIT_ASSERT_EQUAL((Glib::ustring) "100000",
    header2->get_attribute_value("bufferSize"));
  CPPUNIT_ASSERT_EQUAL((Glib::ustring) "1.0",
    header2->get_attribute_value("version"));
  
  CPPUNIT_ASSERT_EQUAL((Glib::ustring) "10",
    header2->get_attribute_value("nextSequence"));
  CPPUNIT_ASSERT_EQUAL((Glib::ustring) "1",
    header2->get_attribute_value("firstSequence"));
  CPPUNIT_ASSERT_EQUAL((Glib::ustring) "9",
    header2->get_attribute_value("lastSequence"));
  
  delete initXml1;
  delete initXml2;
}

void XmlPrinterTest::testPrintNode()
{
  xmlpp::Document doc;
  
  xmlpp::Element *root = doc.create_root_node("Root");
  xmlpp::Element *child1 = root->add_child("Child1");
  xmlpp::Element *child2 = root->add_child("Child2");
  xmlpp::Element *grandChild = child2->add_child("GrandChild");
  
  child1->set_attribute("abc", "def");
  child1->set_attribute("ABC", "DEF");
  grandChild->set_child_text("TEXT!!!");
  
  string expected;
  
  expected += "<Root>\n";
  expected += "  <Child1 abc=\"def\" ABC=\"DEF\" />\n";
  expected += "  <Child2>\n";
  expected += "    <GrandChild>TEXT!!!</GrandChild>\n";
  expected += "  </Child2>\n";
  expected += "</Root>\n";
  
  CPPUNIT_ASSERT_EQUAL(expected, XmlPrinter::printNode(root));
}

void XmlPrinterTest::testPrintIndentation()
{
  CPPUNIT_ASSERT(XmlPrinter::printIndentation(0).empty());
  CPPUNIT_ASSERT_EQUAL((string) "  ",
    XmlPrinter::printIndentation(2));
  CPPUNIT_ASSERT_EQUAL((string) "    ",
    XmlPrinter::printIndentation(4));
  CPPUNIT_ASSERT_EQUAL((string) "      ",
    XmlPrinter::printIndentation(6));
  CPPUNIT_ASSERT_EQUAL((string) "        ",
    XmlPrinter::printIndentation(8));
  CPPUNIT_ASSERT_EQUAL((string) "                    ",
    XmlPrinter::printIndentation(20));
}

void XmlPrinterTest::testAddAttributes()
{
  xmlpp::Document doc;
  
  xmlpp::Element *element = doc.create_root_node("Root");
  
  CPPUNIT_ASSERT(element->get_attribute_value("a").empty());
  CPPUNIT_ASSERT(element->get_attribute_value("key1").empty());
  CPPUNIT_ASSERT(element->get_attribute_value("key2").empty());
  
  map<string, string> attributes;
  attributes["a"] = "A";
  attributes["key1"] = "value1";
  attributes["key2"] = "value2";
  
  XmlPrinter::addAttributes(element, attributes);
  
  CPPUNIT_ASSERT_EQUAL((Glib::ustring) "A", element->get_attribute_value("a"));
  CPPUNIT_ASSERT_EQUAL((Glib::ustring) "value1",
    element->get_attribute_value("key1"));
  CPPUNIT_ASSERT_EQUAL((Glib::ustring) "value2",
    element->get_attribute_value("key2"));
}

void XmlPrinterTest::testSearchParentForId()
{
  xmlpp::Document doc;
  xmlpp::Element *root = doc.create_root_node("Root");
  
  xmlpp::Element *child1 = root->add_child("child1");
  xmlpp::Element *child2 = child1->add_child("child2");
  xmlpp::Element *child3 = child2->add_child("child3");
  xmlpp::Element *child4 = child3->add_child("child4");
  
  root->set_attribute("componentId", "1");
  child1->set_attribute("componentId", "2");
  child2->set_attribute("componentId", "3");
  child3->set_attribute("componentId", "4");
  child4->set_attribute("componentId", "5");
  
  list<xmlpp::Element *> elements;
  elements.push_back(child1);
  elements.push_back(child2);
  elements.push_back(child3);
  elements.push_back(child4);
  
  CPPUNIT_ASSERT_EQUAL(child1, XmlPrinter::searchParentsForId(elements, "1"));
  CPPUNIT_ASSERT_EQUAL(child2, XmlPrinter::searchParentsForId(elements, "2"));
  CPPUNIT_ASSERT_EQUAL(child3, XmlPrinter::searchParentsForId(elements, "3"));
  CPPUNIT_ASSERT_EQUAL(child4, XmlPrinter::searchParentsForId(elements, "4"));
}

void XmlPrinterTest::testGetDeviceStream()
{
  xmlpp::Document doc;
  
  xmlpp::Element *root = doc.create_root_node("Root");
  
  xmlpp::Element *deviceStream1 = root->add_child("DeviceStream");
  xmlpp::Element *deviceStream2 = root->add_child("DeviceStream");
  
  deviceStream1->set_attribute("name", "Device1");
  deviceStream2->set_attribute("name", "Device2");
  
  map<string, string> attributes1;
  attributes1["id"] = "3";
  attributes1["name"] = "Device1";
  attributes1["uuid"] = "UniversalUniqueIdA";
  attributes1["sampleRate"] = "100.11";
  attributes1["iso841Class"] = "12";
  Device device1(attributes1);
  
  map<string, string> attributes2;
  attributes2["id"] = "5";
  attributes2["name"] = "Device2";
  attributes2["uuid"] = "UniversalUniqueIdA";
  attributes2["sampleRate"] = "100.11";
  attributes2["iso841Class"] = "12";
  Device device2(attributes2);
  
  
  CPPUNIT_ASSERT_EQUAL((size_t) 2, root->get_children("DeviceStream").size());
  
  CPPUNIT_ASSERT_EQUAL(deviceStream1,
    XmlPrinter::getDeviceStream(root, &device1));
  CPPUNIT_ASSERT_EQUAL(deviceStream2,
    XmlPrinter::getDeviceStream(root, &device2));
  
  map<string, string> attributes3;
  attributes3["id"] = "5";
  attributes3["name"] = "Device3";
  attributes3["uuid"] = "UniversalUniqueIdA";
  attributes3["sampleRate"] = "100.11";
  attributes3["iso841Class"] = "12";
  Device device3(attributes3);
  
  xmlpp::Element *deviceStream3 = XmlPrinter::getDeviceStream(root, &device3);
  
  CPPUNIT_ASSERT_EQUAL((size_t) 3, root->get_children("DeviceStream").size());
  CPPUNIT_ASSERT_EQUAL((Glib::ustring) "Device3",
    deviceStream3->get_attribute_value("name"));
}

void XmlPrinterTest::testPrintError()
{
  string errorString = getFile("../samples/test_error.xml");
  
  fillAttribute(errorString, "instanceId", "123");
  fillAttribute(errorString, "bufferSize", "9999");
  fillAttribute(errorString, "creationTime", getCurrentTime(GMT));
  
  fillAttribute(errorString, "errorCode", "ERROR_CODE");
  fillErrorText(errorString, "ERROR TEXT!");
  
  CPPUNIT_ASSERT_EQUAL(errorString,
    XmlPrinter::printError(123, 9999, 1, "ERROR_CODE", "ERROR TEXT!"));
}

void XmlPrinterTest::testPrintProbe()
{
  string probeString = getFile("../samples/test_probe.xml");
  
  fillAttribute(probeString, "instanceId", "123");
  fillAttribute(probeString, "bufferSize", "9999");
  fillAttribute(probeString, "creationTime", getCurrentTime(GMT));
  
  CPPUNIT_ASSERT_EQUAL(probeString,
    XmlPrinter::printProbe(123, 9999, 1, devices));
}

void XmlPrinterTest::testPrintCurrent()
{
  string currentString = getFile("../samples/test_current.xml");
  
  list<ComponentEvent *> events;
  
  events.push_back(addEventToDataItem("6", 10254804, "0"));
  events.push_back(addEventToDataItem("25", 15, "100"));
  events.push_back(addEventToDataItem("12", 10254803, "0"));
  events.push_back(addEventToDataItem("13", 16, "100"));
  events.push_back(addEventToDataItem("14", 10254797, "0.00199"));
  events.push_back(addEventToDataItem("15", 10254800, "0.00199"));
  events.push_back(addEventToDataItem("16", 10254798, "0.0002"));
  events.push_back(addEventToDataItem("17", 10254801, "0.0002"));
  events.push_back(addEventToDataItem("18", 10254799, "1"));
  events.push_back(addEventToDataItem("19", 10254802, "1"));
  events.push_back(addEventToDataItem("20", 10254789, "x-0.132010 y-0.158143"));
  events.push_back(addEventToDataItem("21", 13, "AUTOMATIC"));
  events.push_back(addEventToDataItem("22", 10254796, "0"));
  events.push_back(addEventToDataItem("23", 12,
    "/home/mtconnect/simulator/spiral.ngc"));
  events.push_back(addEventToDataItem("24", 10254795, "READY"));
  events.push_back(addEventToDataItem("1", 1, "ON"));
  
  list<DataItem *> dataItems;
  map<string, DataItem *> dataItemsMap = devices.front()->getDeviceDataItems();
  
  map<string, DataItem*>::iterator data;
  for (data = dataItemsMap.begin(); data != dataItemsMap.end(); data++)
  {
    dataItems.push_back(data->second);
  }
  
  fillAttribute(currentString, "instanceId", "123");
  fillAttribute(currentString, "bufferSize", "9999");
  fillAttribute(currentString, "creationTime", getCurrentTime(GMT));
  
  CPPUNIT_ASSERT_EQUAL(currentString,
    XmlPrinter::printCurrent(123, 9999, 10254805, 10123733, dataItems));
  
  clearEvents(events);
}

void XmlPrinterTest::testPrintSample()
{
  string sampleString = getFile("../samples/test_sample.xml");
  
  list<ComponentEvent *> events;
  
  events.push_back(addEventToDataItem("17", 10843512, "0.553472"));
  events.push_back(addEventToDataItem("16", 10843514, "0.551123"));
  events.push_back(addEventToDataItem("17", 10843516, "0.556826"));
  events.push_back(addEventToDataItem("16", 10843518, "0.55582"));
  events.push_back(addEventToDataItem("17", 10843520, "0.560181"));
  events.push_back(addEventToDataItem("14", 10843513, "-0.900624"));
  events.push_back(addEventToDataItem("15", 10843515, "-0.89692"));
  events.push_back(addEventToDataItem("14", 10843517, "-0.897574"));
  events.push_back(addEventToDataItem("15", 10843519, "-0.894742"));
  events.push_back(addEventToDataItem("14", 10843521, "-0.895613"));
  events.push_back(addEventToDataItem("22", 11351720, "229"));
  events.push_back(addEventToDataItem("20", 11351726, "x-1.149250 y1.048981"));
  
  fillAttribute(sampleString, "instanceId", "123");
  fillAttribute(sampleString, "bufferSize", "9999");
  fillAttribute(sampleString, "creationTime", getCurrentTime(GMT));
  
  CPPUNIT_ASSERT_EQUAL(sampleString,
    XmlPrinter::printSample(123, 9999, 10974584, 10843512, events));
  
  clearEvents(events);
}

DataItem * XmlPrinterTest::getDataItemById(const char *id)
{
  map<string, DataItem*> dataItems = devices.front()->getDeviceDataItems();
  
  map<string, DataItem*>::iterator dataItem;
  for (dataItem = dataItems.begin(); dataItem != dataItems.end(); dataItem++)
  {
    if ((dataItem->second)->getId() == id)
    {
      return (dataItem->second);
    }
  }
  return NULL;
}

ComponentEvent * XmlPrinterTest::addEventToDataItem(
    const char *dataItemId,
    unsigned int sequence,
    string value
  )
{
  string time("TIME");
  
  // Make sure the data item is there
  DataItem *d = getDataItemById(dataItemId);
  CPPUNIT_ASSERT(d);
  
  ComponentEvent *event = new ComponentEvent(*d, sequence, time, value);
  d->setLatestEvent(*event);
  return event;
}

void XmlPrinterTest::clearEvents(list<ComponentEvent *> events)
{
  list<ComponentEvent *>::iterator event;
  for (event = events.begin(); event != events.end(); event++)
  {
    delete (*event);
  }
}

