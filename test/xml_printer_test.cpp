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

void XmlPrinterTest::testPrintError()
{
  PARSE_XML(XmlPrinter::printError(123, 9999, 1, "ERROR_CODE", "ERROR TEXT!"));
  
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@instanceId", "123");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@bufferSize", "9999");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "ERROR_CODE");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "ERROR TEXT!");
}

void XmlPrinterTest::testPrintProbe()
{  
  PARSE_XML(XmlPrinter::printProbe(123, 9999, 1, devices));
    
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@instanceId", "123");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@bufferSize", "9999");
  
  // Check Description
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Description@manufacturer", "NIST");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Description@serialNumber", "1122");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Description", "Linux CNC Device");
  
  // Check Axes
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Axes@name", "Axes");
  
  // Check Spindle
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Rotary@name", "C");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Rotary/m:DataItems/m:DataItem@type", "SPINDLE_SPEED");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Rotary/m:DataItems/m:DataItem[@type='ROTARY_MODE']@name",
                                    "Smode");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Rotary/m:DataItems/m:DataItem[@type='ROTARY_MODE']/m:Constraints/m:Value",
                                    "SPINDLE");
  
  // Check Linear Axis
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Linear[@name='X']/m:DataItems/m:DataItem@type", "POSITION");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Linear[@name='X']/m:DataItems/m:DataItem@name", "Xact");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Linear[@name='X']/m:DataItems/m:DataItem@significantDigits", "6");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Linear[@name='X']/m:DataItems/m:DataItem[@type='LOAD']//m:Maximum", "200");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Linear[@name='X']/m:DataItems/m:DataItem[@type='LOAD']/m:Constraints/m:Minimum", "0");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Linear[@name='X']/m:DataItems/m:DataItem[@type='LOAD']/m:Constraints/m:Maximum", "200");

  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Linear[@name='Z']/m:DataItems/m:DataItem@type", "POSITION");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Linear[@name='Z']/m:DataItems/m:DataItem@name", "Zact");
  
  // Check for Path component
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Controller//m:Path/m:DataItems/m:DataItem[@type='PATH_POSITION']@name",
                                    "Ppos");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='clc']@category", "CONDITION");

}

void XmlPrinterTest::testPrintCurrent()
{
  Checkpoint checkpoint;
  addEventToCheckpoint(checkpoint, "Xact", 10254804, "0");
  addEventToCheckpoint(checkpoint, "SspeedOvr", 15, "100");
  addEventToCheckpoint(checkpoint, "Xcom", 10254803, "0");
  addEventToCheckpoint(checkpoint, "spindle_speed", 16, "100");
  addEventToCheckpoint(checkpoint, "Yact", 10254797, "0.00199");
  addEventToCheckpoint(checkpoint, "Ycom", 10254800, "0.00189");
  addEventToCheckpoint(checkpoint, "Zact", 10254798, "0.0002");
  addEventToCheckpoint(checkpoint, "Zcom", 10254801, "0.0003");
  addEventToCheckpoint(checkpoint, "block", 10254789, "x-0.132010 y-0.158143");
  addEventToCheckpoint(checkpoint, "mode", 13, "AUTOMATIC");
  addEventToCheckpoint(checkpoint, "line", 10254796, "0");
  addEventToCheckpoint(checkpoint, "program", 12, "/home/mtconnect/simulator/spiral.ngc");
  addEventToCheckpoint(checkpoint, "execution", 10254795, "READY");
  addEventToCheckpoint(checkpoint, "power", 1, "ON");
  
  vector<ComponentEventPtr> list;
  checkpoint.getComponentEvents(list);
  PARSE_XML(XmlPrinter::printSample(123, 131072, 10254805, 10123733, list));
  
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xact']", "0");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='C']/m:Samples/m:SpindleSpeed[@name='Sovr']", "100");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xcom']", "0");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='C']/m:Samples/m:SpindleSpeed[@name='Sspeed']", "100");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Y']/m:Samples/m:Position[@name='Yact']", "0.00199");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Y']/m:Samples/m:Position[@name='Ycom']", "0.00189");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Z']/m:Samples/m:Position[@name='Zact']", "0.0002");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Z']/m:Samples/m:Position[@name='Zcom']", "0.0003");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='path']/m:Events/m:Block", 
                                    "x-0.132010 y-0.158143");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='path']/m:Events/m:Execution",
                                    "READY");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='path']/m:Events/m:ControllerMode",
                                    "AUTOMATIC");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='path']/m:Events/m:Line",
                                    "0");    
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='path']/m:Events/m:Program",
                                    "/home/mtconnect/simulator/spiral.ngc");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='power']/m:Events/m:PowerState",
                                    "ON");
}

void XmlPrinterTest::testPrintSample()
{
  vector<ComponentEventPtr> events;
  
  events.push_back(newEvent("Xact", 10843512, "0.553472"));
  events.push_back(newEvent("Xcom", 10843514, "0.551123"));
  events.push_back(newEvent("Xact", 10843516, "0.556826"));
  events.push_back(newEvent("Xcom", 10843518, "0.55582"));
  events.push_back(newEvent("Xact", 10843520, "0.560181"));
  events.push_back(newEvent("Yact", 10843513, "-0.900624"));
  events.push_back(newEvent("Ycom", 10843515, "-0.89692"));
  events.push_back(newEvent("Yact", 10843517, "-0.897574"));
  events.push_back(newEvent("Ycom", 10843519, "-0.894742"));
  events.push_back(newEvent("Xact", 10843521, "-0.895613"));
  events.push_back(newEvent("line", 11351720, "229"));
  events.push_back(newEvent("block", 11351726, "x-1.149250 y1.048981"));
    
  PARSE_XML(XmlPrinter::printSample(123, 131072, 10974584, 10843512, events));
  
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "/m:MTConnectStreams/m:Streams/m:DeviceStream/m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xact'][1]", 
                                    "0.553472");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xact'][2]", 
                                    "0.556826");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xcom'][1]", 
                                    "0.551123");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xcom'][2]", 
                                    "0.55582");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xact'][3]", 
                                    "0.560181");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xact'][4]", 
                                    "-0.895613");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Y']/m:Samples/m:Position[@name='Yact'][1]", 
                                    "-0.900624");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Y']/m:Samples/m:Position[@name='Yact'][2]", 
                                    "-0.897574");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Y']/m:Samples/m:Position[@name='Ycom'][1]", 
                                    "-0.89692");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Y']/m:Samples/m:Position[@name='Ycom'][2]", 
                                    "-0.894742");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='path']/m:Events/m:Line",
                                    "229");    
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='path']/m:Events/m:Block",
                                    "x-1.149250 y1.048981");    
}

void XmlPrinterTest::testCondition()
{

  Checkpoint checkpoint;
  addEventToCheckpoint(checkpoint, "Xact", 10254804, "0");
  addEventToCheckpoint(checkpoint, "SspeedOvr", 15, "100");
  addEventToCheckpoint(checkpoint, "Xcom", 10254803, "0");
  addEventToCheckpoint(checkpoint, "spindle_speed", 16, "100");
  addEventToCheckpoint(checkpoint, "Yact", 10254797, "0.00199");
  addEventToCheckpoint(checkpoint, "Ycom", 10254800, "0.00189");
  addEventToCheckpoint(checkpoint, "Zact", 10254798, "0.0002");
  addEventToCheckpoint(checkpoint, "Zcom", 10254801, "0.0003");
  addEventToCheckpoint(checkpoint, "block", 10254789, "x-0.132010 y-0.158143");
  addEventToCheckpoint(checkpoint, "mode", 13, "AUTOMATIC");
  addEventToCheckpoint(checkpoint, "line", 10254796, "0");
  addEventToCheckpoint(checkpoint, "program", 12, "/home/mtconnect/simulator/spiral.ngc");
  addEventToCheckpoint(checkpoint, "execution", 10254795, "READY");
  addEventToCheckpoint(checkpoint, "power", 1, "ON");
  addEventToCheckpoint(checkpoint, "ctmp", 18, "WARNING|OTEMP|1|HIGH|Spindle Overtemp");
  addEventToCheckpoint(checkpoint, "cmp", 18, "NORMAL||||");
  addEventToCheckpoint(checkpoint, "lp", 18, "FAULT|LOGIC|2||PLC Error");
  
  vector<ComponentEventPtr> list;
  checkpoint.getComponentEvents(list);
  PARSE_XML(XmlPrinter::printSample(123, 131072, 10254805, 10123733, list));

  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='C']/m:Condition/m:Warning",
                                    "Spindle Overtemp");    
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='C']/m:Condition/m:Warning@type",
                                    "TEMPERATURE");    
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='C']/m:Condition/m:Warning@qualifier",
                                    "HIGH");    
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='C']/m:Condition/m:Warning@nativeCode",
                                    "OTEMP");    
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='C']/m:Condition/m:Warning@nativeSeverity",
                                    "1");    

  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='path']/m:Condition/m:Normal",
                                    0);
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='path']/m:Condition/m:Normal@qualifier",
                                    0);    
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='path']/m:Condition/m:Normal@nativeCode",
                                    0);    

  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Controller']/m:Condition/m:Fault@nativeCode",
                                    "LOGIC");    
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Controller']/m:Condition/m:Fault",
                                    "PLC Error");    
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Controller']/m:Condition/m:Fault@qualifier",
                                    0);    
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Controller']/m:Condition/m:Fault@nativeSeverity",
                                    "2");
}

void XmlPrinterTest::testVeryLargeSequence()
{
  Checkpoint checkpoint;
  addEventToCheckpoint(checkpoint, "Xact", (((Int64)1) << 48) + 1, "0");
  addEventToCheckpoint(checkpoint, "Xcom", (((Int64) 1) << 48) + 3, "123");
  
  vector<ComponentEventPtr> list;
  checkpoint.getComponentEvents(list);
  PARSE_XML(XmlPrinter::printSample(123, 131072, (((Int64)1) << 48) + 3, (((Int64)1) << 48) + 1, list));
  
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xact']", 
                                    "0");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xact']@sequence", 
                                    "281474976710657");
  

  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xcom']", 
                                    "123");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xcom']@sequence", 
                                    "281474976710659");
  
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@firstSequence", "281474976710657");
  CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence", "281474976710659");

  
}

DataItem * XmlPrinterTest::getDataItem(const char *name)
{
  Device *device = devices.front();
  CPPUNIT_ASSERT(device);
  
  return device->getDeviceDataItem(name);
}

ComponentEvent *XmlPrinterTest::newEvent(
  const char *name,
  Int64 sequence,
  string value
  )
{
  string time("TIME");
  
  // Make sure the data item is there
  DataItem *d = getDataItem(name);
  CPPUNIT_ASSERT_MESSAGE((string) "Could not find data item " + name, d); 
  
  return new ComponentEvent(*d, sequence, time, value);
}


ComponentEvent * XmlPrinterTest::addEventToCheckpoint(
  Checkpoint &aCheckpoint,
  const char *name,
  Int64 sequence,
  string value
  )
{
  ComponentEvent *event = newEvent(name, sequence, value);
  aCheckpoint.addComponentEvent(event);
  return event;
}


