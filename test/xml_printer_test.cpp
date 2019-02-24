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

#include "xml_printer_test.hpp"
#include "asset.hpp"

using namespace std;

namespace mtconnect {
  namespace test {
    // Registers the fixture into the 'registry'
    CPPUNIT_TEST_SUITE_REGISTRATION(XmlPrinterTest);
    
    void XmlPrinterTest::setUp()
    {
      m_config = new XmlParser();
      XmlPrinter::setSchemaVersion("");
      m_devices = m_config->parseFile("../samples/test_config.xml");
    }
    
    
    void XmlPrinterTest::tearDown()
    {
      delete m_config; m_config = nullptr;
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
      PARSE_XML(XmlPrinter::printProbe(123, 9999, 1, 1024, 10, m_devices));
      
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@instanceId", "123");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@bufferSize", "9999");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetBufferSize", "1024");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "10");
      
      
      // Check Description
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Description@manufacturer", "NIST");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Description@serialNumber", "1122");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Description", "Linux CNC Device");
      
      // Check Axes
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Axes@name", "Axes");
      
      // Check Spindle
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Rotary@name", "C");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Rotary/m:DataItems/m:DataItem@type", "SPINDLE_SPEED");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Rotary/m:DataItems/m:DataItem[@type='ROTARY_MODE']@name", "Smode");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Rotary/m:DataItems/m:DataItem[@type='ROTARY_MODE']/m:Constraints/m:Value", "SPINDLE");
      
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
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Controller//m:Path/m:DataItems/m:DataItem[@type='PATH_POSITION']@name", "Ppos");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='clc']@category", "CONDITION");
      
      // Check for composition ids
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='zt1']@compositionId", "zmotor");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='zt2']@compositionId", "zamp");
      
      // check for compositions
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Composition[@id='zmotor']@type", "MOTOR");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Composition[@id='zmotor']@uuid", "12345");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Composition[@id='zmotor']@name", "motor_name");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Composition[@id='zmotor']/m:Description", "Hello There");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Composition[@id='zmotor']/m:Description@manufacturer", "open");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Composition[@id='zmotor']/m:Description@model", "vroom");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Composition[@id='zmotor']/m:Description@serialNumber", "12356");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Composition[@id='zmotor']/m:Description@station", "A");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Composition[@id='zamp']@type", "AMPLIFIER");
    }
    
    
    void XmlPrinterTest::testPrintDataItemElements()
    {
      PARSE_XML(XmlPrinter::printProbe(123, 9999, 1, 1024, 10, m_devices));
      
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='y1']/m:Filters/m:Filter[1]@type",
                                        "MINIMUM_DELTA");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='y1']/m:Filters/m:Filter[1]", "2");
      
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='ylc']/m:Filters/m:Filter[1]@type",
                                        "PERIOD");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='ylc']/m:Filters/m:Filter[1]", "1");
      
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='pcount']/m:InitialValue", "0");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='pcount']/m:ResetTrigger", "DAY");
      
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='pcountrem']/m:ResetTrigger", "SHIFT");
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
      
      ComponentEventPtrArray list;
      checkpoint.getComponentEvents(list);
      PARSE_XML(XmlPrinter::printSample(123, 131072, 10254805, 10123733, 10123800, list));
      
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xact']", "0");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='C']/m:Samples/m:SpindleSpeed[@name='Sovr']", "100");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xcom']", "0");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='C']/m:Samples/m:SpindleSpeed[@name='Sspeed']", "100");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='Y']/m:Samples/m:Position[@name='Yact']", "0.00199");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='Y']/m:Samples/m:Position[@name='Ycom']", "0.00189");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='Z']/m:Samples/m:Position[@name='Zact']", "0.0002");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='Z']/m:Samples/m:Position[@name='Zcom']", "0.0003");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='path']/m:Events/m:Block",
                                        "x-0.132010 y-0.158143");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='path']/m:Events/m:Execution",
                                        "READY");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='path']/m:Events/m:ControllerMode",
                                        "AUTOMATIC");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='path']/m:Events/m:Line",
                                        "0");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='path']/m:Events/m:Program",
                                        "/home/mtconnect/simulator/spiral.ngc");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='power']/m:Events/m:PowerState",
                                        "ON");
    }
    
    
    void XmlPrinterTest::testChangeDevicesNamespace()
    {
      // Devices
      XmlPrinter::clearDevicesNamespaces();
      
      {
        PARSE_XML(XmlPrinter::printProbe(123, 9999, 1024, 10, 1, m_devices));
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                          "/m:MTConnectDevices@schemaLocation",
                                          "urn:mtconnect.org:MTConnectDevices:1.2 http://schemas.mtconnect.org/schemas/MTConnectDevices_1.2.xsd");
      }
      
      {
        XmlPrinter::addDevicesNamespace("urn:machine.com:MachineDevices:1.3",
                                        "http://www.machine.com/schemas/MachineDevices_1.3.xsd",
                                        "e");
        
        PARSE_XML(XmlPrinter::printProbe(123, 9999, 1024, 10, 1, m_devices));
        
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                          "/m:MTConnectDevices@schemaLocation",
                                          "urn:machine.com:MachineDevices:1.3 http://www.machine.com/schemas/MachineDevices_1.3.xsd");
        
        XmlPrinter::clearDevicesNamespaces();
      }
      
      {
        XmlParser ext;
        std::vector<Device *> extdevs = ext.parseFile("../samples/extension.xml");
        PARSE_XML(XmlPrinter::printProbe(123, 9999, 1024, 10, 1, extdevs));
        
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                          "/m:MTConnectDevices@schemaLocation",
                                          "urn:example.com:ExampleDevices:1.1 ExtensionDevices_1.1.xsd");
        
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                          "//m:Device//x:Pump@name",
                                          "pump");
      }
      
      XmlPrinter::clearDevicesNamespaces();
    }
    
    
    void XmlPrinterTest::testChangeStreamsNamespace()
    {
      XmlPrinter::clearStreamsNamespaces();
      
      Checkpoint checkpoint;
      addEventToCheckpoint(checkpoint, "Xact", 10254804, "0");
      addEventToCheckpoint(checkpoint, "SspeedOvr", 15, "100");
      addEventToCheckpoint(checkpoint, "Xcom", 10254803, "0");
      
      // Streams
      {
        ComponentEventPtrArray list;
        checkpoint.getComponentEvents(list);
        
        PARSE_XML(XmlPrinter::printSample(123, 131072, 10254805, 10123733, 10123800, list));
        
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                          "/m:MTConnectStreams@schemaLocation",
                                          "urn:mtconnect.org:MTConnectStreams:1.2 http://schemas.mtconnect.org/schemas/MTConnectStreams_1.2.xsd");
      }
      
      XmlPrinter::clearStreamsNamespaces();
      
      {
        
        XmlPrinter::addStreamsNamespace("urn:machine.com:MachineStreams:1.3",
                                        "http://www.machine.com/schemas/MachineStreams_1.3.xsd",
                                        "e");
        
        ComponentEventPtrArray list;
        checkpoint.getComponentEvents(list);
        PARSE_XML(XmlPrinter::printSample(123, 131072, 10254805, 10123733, 10123800, list));
        
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                          "/m:MTConnectStreams@schemaLocation",
                                          "urn:machine.com:MachineStreams:1.3 http://www.machine.com/schemas/MachineStreams_1.3.xsd");
      }
      
      XmlPrinter::clearStreamsNamespaces();
      
      {
        XmlParser ext;
        m_devices = ext.parseFile("../samples/extension.xml");
        
        XmlPrinter::addStreamsNamespace("urn:example.com:ExampleDevices:1.3",
                                        "ExtensionDevices_1.3.xsd",
                                        "x");
        
        Checkpoint checkpoint2;
        addEventToCheckpoint(checkpoint2, "flow", 10254804, "100");
        
        ComponentEventPtrArray list;
        checkpoint2.getComponentEvents(list);
        
        PARSE_XML(XmlPrinter::printSample(123, 131072, 10254805, 10123733, 10123800, list));
        
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//x:Flow", "100");
      }
      
      XmlPrinter::clearStreamsNamespaces();
      
      {
        XmlParser ext;
        m_devices = ext.parseFile("../samples/extension.xml");
        
        XmlPrinter::addStreamsNamespace("urn:example.com:ExampleDevices:1.3",
                                        "ExtensionDevices_1.3.xsd",
                                        "x");
        
        Checkpoint checkpoint2;
        addEventToCheckpoint(checkpoint2, "flow", 10254804, "100");
        
        ComponentEventPtrArray list;
        checkpoint2.getComponentEvents(list);
        
        PARSE_XML(XmlPrinter::printSample(123, 131072, 10254805, 10123733, 10123800, list));
        
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//x:Flow", "100");
      }
      
      
      XmlPrinter::clearStreamsNamespaces();
      XmlPrinter::clearDevicesNamespaces();
      
    }
    
    
    void XmlPrinterTest::testChangeErrorNamespace()
    {
      // Error
      
      {
        PARSE_XML(XmlPrinter::printError(123, 9999, 1, "ERROR_CODE", "ERROR TEXT!"));
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                          "/m:MTConnectError@schemaLocation",
                                          "urn:mtconnect.org:MTConnectError:1.2 http://schemas.mtconnect.org/schemas/MTConnectError_1.2.xsd");
      }
      
      {
        XmlPrinter::addErrorNamespace("urn:machine.com:MachineError:1.3",
                                      "http://www.machine.com/schemas/MachineError_1.3.xsd",
                                      "e");
        
        PARSE_XML(XmlPrinter::printError(123, 9999, 1, "ERROR_CODE", "ERROR TEXT!"));
        
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                          "/m:MTConnectError@schemaLocation",
                                          "urn:machine.com:MachineError:1.3 http://www.machine.com/schemas/MachineError_1.3.xsd");
      }
    }
    
    
    void XmlPrinterTest::testPrintSample()
    {
      ComponentEventPtrArray events;
      
      ComponentEventPtr ptr;
      ptr = newEvent("Xact", 10843512, "0.553472");
      events.push_back(ptr);
      ptr = newEvent("Xcom", 10843514, "0.551123");
      events.push_back(ptr);
      ptr = newEvent("Xact", 10843516, "0.556826");
      events.push_back(ptr);
      ptr = newEvent("Xcom", 10843518, "0.55582");
      events.push_back(ptr);
      ptr = newEvent("Xact", 10843520, "0.560181");
      events.push_back(ptr);
      ptr = newEvent("Yact", 10843513, "-0.900624");
      events.push_back(ptr);
      ptr = newEvent("Ycom", 10843515, "-0.89692");
      events.push_back(ptr);
      ptr = newEvent("Yact", 10843517, "-0.897574");
      events.push_back(ptr);
      ptr = newEvent("Ycom", 10843519, "-0.894742");
      events.push_back(ptr);
      ptr = newEvent("Xact", 10843521, "-0.895613");
      events.push_back(ptr);
      ptr = newEvent("line", 11351720, "229");
      events.push_back(ptr);
      ptr = newEvent("block", 11351726, "x-1.149250 y1.048981");
      events.push_back(ptr);
      
      PARSE_XML(XmlPrinter::printSample(123, 131072, 10974584, 10843512, 10123800, events));
      
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "/m:MTConnectStreams/m:Streams/m:DeviceStream/m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xact'][1]",
                                        "0.553472");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xact'][2]",
                                        "0.556826");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xcom'][1]",
                                        "0.551123");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xcom'][2]",
                                        "0.55582");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xact'][3]",
                                        "0.560181");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xact'][4]",
                                        "-0.895613");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='Y']/m:Samples/m:Position[@name='Yact'][1]",
                                        "-0.900624");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='Y']/m:Samples/m:Position[@name='Yact'][2]",
                                        "-0.897574");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='Y']/m:Samples/m:Position[@name='Ycom'][1]",
                                        "-0.89692");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='Y']/m:Samples/m:Position[@name='Ycom'][2]",
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
      
      ComponentEventPtrArray list;
      checkpoint.getComponentEvents(list);
      PARSE_XML(XmlPrinter::printSample(123, 131072, 10254805, 10123733, 10123800, list));
      
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='C']/m:Condition/m:Warning",
                                        "Spindle Overtemp");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='C']/m:Condition/m:Warning@type",
                                        "TEMPERATURE");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='C']/m:Condition/m:Warning@qualifier",
                                        "HIGH");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='C']/m:Condition/m:Warning@nativeCode",
                                        "OTEMP");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='C']/m:Condition/m:Warning@nativeSeverity",
                                        "1");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='path']/m:Condition/m:Normal",
                                        0);
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='path']/m:Condition/m:Normal@qualifier",
                                        0);
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='path']/m:Condition/m:Normal@nativeCode",
                                        0);
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='Controller']/m:Condition/m:Fault@nativeCode",
                                        "LOGIC");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='Controller']/m:Condition/m:Fault",
                                        "PLC Error");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='Controller']/m:Condition/m:Fault@qualifier",
                                        0);
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='Controller']/m:Condition/m:Fault@nativeSeverity",
                                        "2");
    }
    
    
    void XmlPrinterTest::testVeryLargeSequence()
    {
      Checkpoint checkpoint;
      addEventToCheckpoint(checkpoint, "Xact", (((uint64_t)1) << 48) + 1, "0");
      addEventToCheckpoint(checkpoint, "Xcom", (((uint64_t) 1) << 48) + 3, "123");
      
      ComponentEventPtrArray list;
      checkpoint.getComponentEvents(list);
      PARSE_XML(XmlPrinter::printSample(123, 131072, (((uint64_t)1) << 48) + 3,
                                        (((uint64_t)1) << 48) + 1, (((uint64_t)1) << 48) + 1024, list));
      
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xact']",
                                        "0");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xact']@sequence",
                                        "281474976710657");
      
      
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xcom']",
                                        "123");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xcom']@sequence",
                                        "281474976710659");
      
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@firstSequence", "281474976710657");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence", "281474976710659");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@lastSequence", "281474976711680");
    }
    
    
    void XmlPrinterTest::testChangeDeviceAttributes()
    {
      auto device = m_devices.front();
      
      string v = "Some_Crazy_Uuid";
      device->setUuid(v);
      v = "Big Tool MFG";
      device->setManufacturer(v);
      v = "111999333444";
      device->setSerialNumber(v);
      v = "99999999";
      device->setStation(v);
      
      device = m_devices.front();
      
      PARSE_XML(XmlPrinter::printProbe(123, 9999, 1024, 10, 1, m_devices));
      
      // Check Description
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Device@uuid", "Some_Crazy_Uuid");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Description@manufacturer", "Big Tool MFG");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Description@serialNumber", "111999333444");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Description@station", "99999999");
    }
    
    
    void XmlPrinterTest::testStatisticAndTimeSeriesProbe()
    {
      PARSE_XML(XmlPrinter::printProbe(123, 9999, 1024, 10, 1, m_devices));
      
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='Xact']@statistic", "AVERAGE");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='Xts']@representation", "TIME_SERIES");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='Xts']@sampleRate", "46000");
    }
    
    
    void XmlPrinterTest::testTimeSeries()
    {
      ComponentEventPtr ptr;
      {
        ComponentEventPtrArray events;
        ptr = newEvent("Xts", 10843512, "6|||1.1 2.2 3.3 4.4 5.5 6.6 ");
        events.push_back(ptr);
        
        PARSE_XML(XmlPrinter::printSample(123, 131072, 10974584, 10843512, 10123800, events));
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                          "//m:ComponentStream[@name='X']/m:Samples/m:PositionTimeSeries@sampleRate",
                                          0);
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                          "//m:ComponentStream[@name='X']/m:Samples/m:PositionTimeSeries@sampleCount",
                                          "6");
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                          "//m:ComponentStream[@name='X']/m:Samples/m:PositionTimeSeries[@name='Xts']",
                                          "1.1 2.2 3.3 4.4 5.5 6.6");
      }
      {
        ComponentEventPtrArray events;
        ptr = newEvent("Xts", 10843512, "6|46200|1.1 2.2 3.3 4.4 5.5 6.6 ");
        events.push_back(ptr);
        
        PARSE_XML(XmlPrinter::printSample(123, 131072, 10974584, 10843512, 10123800, events));
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                          "//m:ComponentStream[@name='X']/m:Samples/m:PositionTimeSeries@sampleRate",
                                          "46200");
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                          "//m:ComponentStream[@name='X']/m:Samples/m:PositionTimeSeries@sampleCount",
                                          "6");
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                          "//m:ComponentStream[@name='X']/m:Samples/m:PositionTimeSeries",
                                          "1.1 2.2 3.3 4.4 5.5 6.6");
      }
    }
    
    
    void XmlPrinterTest::testNonPrintableCharacters()
    {
      ComponentEventPtrArray events;
      ComponentEventPtr ptr = newEvent("zlc", 10843512, "zlc|fault|500|||OVER TRAVEL : +Z? ");
      events.push_back(ptr);
      PARSE_XML(XmlPrinter::printSample(123, 131072, 10974584, 10843512, 10123800, events));
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:DeviceStream//m:ComponentStream[@name='Z']/m:Condition//*[1]"
                                        , "OVER TRAVEL : +Z?");
    }
    
    
    void XmlPrinterTest::testEscapedXMLCharacters()
    {
      ComponentEventPtrArray events;
      ComponentEventPtr ptr = newEvent("zlc", 10843512, "fault|500|||A duck > a foul & < cat '");
      events.push_back(ptr);
      PARSE_XML(XmlPrinter::printSample(123, 131072, 10974584, 10843512, 10123800, events));
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:DeviceStream//m:ComponentStream[@name='Z']/m:Condition//*[1]"
                                        , "A duck > a foul & < cat '");
      
    }
    
    
    void XmlPrinterTest::testPrintAsset()
    {
      // Add the xml to the agent...
      vector<AssetPtr> assets;
      Asset asset((string) "123", (string) "TEST", (string) "HELLO");
      assets.push_back(asset);
      
      {
        PARSE_XML(XmlPrinter::printAssets(123, 4, 2, assets));
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "/m:MTConnectAssets/m:Header@instanceId", "123");
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "/m:MTConnectAssets/m:Header@assetCount", "2");
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "/m:MTConnectAssets/m:Header@assetBufferSize", "4");
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Assets", "HELLO");
      }
    }
    
    
    void XmlPrinterTest::testPrintAssetProbe()
    {
      // Add the xml to the agent...
      map<string, int> counts;
      counts["CuttingTool"] = 10;
      
      PARSE_XML(XmlPrinter::printProbe(123, 9999, 1024, 10, 1, m_devices, &counts));
      
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCounts/m:AssetCount", "10");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCounts/m:AssetCount@assetType", "CuttingTool");
    }
    
    
    void XmlPrinterTest::testConfiguration()
    {
      PARSE_XML(XmlPrinter::printProbe(123, 9999, 1, 1024, 10, m_devices));
      
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:Power/m:Configuration/m:SensorConfiguration/m:CalibrationDate",
                                        "2011-08-10");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:SensorConfiguration/m:Channels/m:Channel@number",
                                        "1");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:SensorConfiguration/m:Channels/m:Channel/m:Description",
                                        "Power Channel");
    }
    
    
    // Schema tests
    void XmlPrinterTest::testChangeVersion()
    {
      // Devices
      XmlPrinter::clearDevicesNamespaces();
      
      {
        PARSE_XML(XmlPrinter::printProbe(123, 9999, 1024, 10, 1, m_devices));
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                          "/m:MTConnectDevices@schemaLocation",
                                          "urn:mtconnect.org:MTConnectDevices:1.2 http://schemas.mtconnect.org/schemas/MTConnectDevices_1.2.xsd");
      }
      
      XmlPrinter::setSchemaVersion("1.4");
      
      {
        PARSE_XML(XmlPrinter::printProbe(123, 9999, 1024, 10, 1, m_devices));
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                          "/m:MTConnectDevices@schemaLocation",
                                          "urn:mtconnect.org:MTConnectDevices:1.4 http://schemas.mtconnect.org/schemas/MTConnectDevices_1.4.xsd");
      }
      
      XmlPrinter::setSchemaVersion("1.3");
    }
    
    
    void XmlPrinterTest::testChangeMTCLocation()
    {
      XmlPrinter::clearDevicesNamespaces();
      
      XmlPrinter::setSchemaVersion("1.3");
      
      XmlPrinter::addDevicesNamespace("urn:mtconnect.org:MTConnectDevices:1.3",
                                      "/schemas/MTConnectDevices_1.3.xsd",
                                      "m");
      
      
      {
        PARSE_XML(XmlPrinter::printProbe(123, 9999, 1024, 10, 1, m_devices));
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                          "/m:MTConnectDevices@schemaLocation",
                                          "urn:mtconnect.org:MTConnectDevices:1.3 /schemas/MTConnectDevices_1.3.xsd");
      }
      
      XmlPrinter::clearDevicesNamespaces();
      XmlPrinter::setSchemaVersion("1.3");
    }
    
    void XmlPrinterTest::testProbeWithFilter13()
    {
      delete m_config;
      
      m_config = new XmlParser();
      m_devices = m_config->parseFile("../samples/filter_example_1.3.xml");
      
      PARSE_XML(XmlPrinter::printProbe(123, 9999, 1024, 10, 1, m_devices));
      
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='load']/m:Filters/m:Filter", "5");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='load']/m:Filters/m:Filter@type", "MINIMUM_DELTA");
    }
    
    void XmlPrinterTest::testProbeWithFilter()
    {
      delete m_config; m_config = nullptr;
      
      m_config = new XmlParser();
      m_devices = m_config->parseFile("../samples/filter_example.xml");
      
      PARSE_XML(XmlPrinter::printProbe(123, 9999, 1024, 10, 1, m_devices));
      
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='load']/m:Filters/m:Filter", "5");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='load']/m:Filters/m:Filter@type", "MINIMUM_DELTA");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='pos']/m:Filters/m:Filter", "10");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='pos']/m:Filters/m:Filter@type", "PERIOD");
    }
    
    
    void XmlPrinterTest::testReferences()
    {
      XmlPrinter::setSchemaVersion("1.4");
      delete m_config; m_config = nullptr;
      
      m_config = new XmlParser();
      m_devices = m_config->parseFile("../samples/reference_example.xml");
      
      PARSE_XML(XmlPrinter::printProbe(123, 9999, 1024, 10, 1, m_devices));
      
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:BarFeederInterface/m:References/m:DataItemRef@idRef",
                                        "c4");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:BarFeederInterface/m:References/m:DataItemRef@name",
                                        "chuck");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:BarFeederInterface/m:References/m:ComponentRef@idRef",
                                        "ele");
    }
    
    void XmlPrinterTest::testLegacyReferences()
    {
      XmlPrinter::setSchemaVersion("1.3");
      delete m_config; m_config = nullptr;
      
      m_config = new XmlParser();
      m_devices = m_config->parseFile("../samples/reference_example.xml");
      
      PARSE_XML(XmlPrinter::printProbe(123, 9999, 1024, 10, 1, m_devices));
      
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:BarFeederInterface/m:References/m:Reference@dataItemId",
                                        "c4");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:BarFeederInterface/m:References/m:Reference@name",
                                        "chuck");
    }
    
    
    void XmlPrinterTest::testSourceReferences()
    {
      delete m_config; m_config = nullptr;
      
      m_config = new XmlParser();
      m_devices = m_config->parseFile("../samples/reference_example.xml");
      
      PARSE_XML(XmlPrinter::printProbe(123, 9999, 1024, 10, 1, m_devices));
      
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:DataItem[@id='bfc']/m:Source@dataItemId",
                                        "mf");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:DataItem[@id='bfc']/m:Source@componentId",
                                        "ele");
      CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                        "//m:DataItem[@id='bfc']/m:Source@compositionId",
                                        "xxx");
    }
    
    void XmlPrinterTest::testStreamsStyle()
    {
      XmlPrinter::setStreamStyle("/styles/Streams.xsl");
      Checkpoint checkpoint;
      addEventToCheckpoint(checkpoint, "Xact", 10254804, "0");
      addEventToCheckpoint(checkpoint, "SspeedOvr", 15, "100");
      addEventToCheckpoint(checkpoint, "Xcom", 10254803, "0");
      addEventToCheckpoint(checkpoint, "spindle_speed", 16, "100");
      
      ComponentEventPtrArray list;
      checkpoint.getComponentEvents(list);
      PARSE_XML(XmlPrinter::printSample(123, 131072, 10254805, 10123733, 10123800, list));
      
      xmlNodePtr pi = doc->children;
      CPPUNIT_ASSERT_EQUAL(string("xml-stylesheet"), string((const char *) pi->name));
      CPPUNIT_ASSERT_EQUAL(string("type=\"text/xsl\" href=\"/styles/Streams.xsl\""),
                           string((const char *) pi->content));
      
      XmlPrinter::setStreamStyle("");
    }
    
    
    void XmlPrinterTest::testDevicesStyle()
    {
      XmlPrinter::setDevicesStyle("/styles/Devices.xsl");
      
      PARSE_XML(XmlPrinter::printProbe(123, 9999, 1, 1024, 10, m_devices));
      
      xmlNodePtr pi = doc->children;
      CPPUNIT_ASSERT_EQUAL(string("xml-stylesheet"), string((const char *) pi->name));
      CPPUNIT_ASSERT_EQUAL(string("type=\"text/xsl\" href=\"/styles/Devices.xsl\""),
                           string((const char *) pi->content));
      
      XmlPrinter::setDevicesStyle("");
    }
    
    
    void XmlPrinterTest::testErrorStyle()
    {
      XmlPrinter::setErrorStyle("/styles/Error.xsl");
      
      PARSE_XML(XmlPrinter::printError(123, 9999, 1, "ERROR_CODE", "ERROR TEXT!"));
      
      xmlNodePtr pi = doc->children;
      CPPUNIT_ASSERT_EQUAL(string("xml-stylesheet"), string((const char *) pi->name));
      CPPUNIT_ASSERT_EQUAL(string("type=\"text/xsl\" href=\"/styles/Error.xsl\""),
                           string((const char *) pi->content));
      
      XmlPrinter::setErrorStyle("");
    }
    
    
    void XmlPrinterTest::testAssetsStyle()
    {
      XmlPrinter::setAssetsStyle("/styles/Assets.xsl");
      
      vector<AssetPtr> assets;
      Asset asset((string) "123", (string) "TEST", (string) "HELLO");
      assets.push_back(asset);
      
      PARSE_XML(XmlPrinter::printAssets(123, 4, 2, assets));
      
      xmlNodePtr pi = doc->children;
      CPPUNIT_ASSERT_EQUAL(string("xml-stylesheet"), string((const char *) pi->name));
      CPPUNIT_ASSERT_EQUAL(string("type=\"text/xsl\" href=\"/styles/Assets.xsl\""),
                           string((const char *) pi->content));
      
      XmlPrinter::setAssetsStyle("");
    }
    
    
    DataItem *XmlPrinterTest::getDataItem(const char *name)
    {
      const auto device = m_devices.front();
      CPPUNIT_ASSERT(device);
      
      return device->getDeviceDataItem(name);
    }
    
    
    ComponentEvent *XmlPrinterTest::newEvent(
                                             const char *name,
                                             uint64_t sequence,
                                             string value
                                             )
    {
      string time("TIME");
      
      // Make sure the data item is there
      const auto d = getDataItem(name);
      CPPUNIT_ASSERT_MESSAGE((string) "Could not find data item " + name, d);
      
      return new ComponentEvent(*d, sequence, time, value);
    }
    
    
    ComponentEvent *XmlPrinterTest::addEventToCheckpoint(
                                                         Checkpoint &checkpoint,
                                                         const char *name,
                                                         uint64_t sequence,
                                                         string value
                                                         )
    {
      ComponentEvent *event = newEvent(name, sequence, value);
      checkpoint.addComponentEvent(event);
      return event;
    }
    
    
    // CuttingTool tests
    void XmlPrinterTest::testPrintCuttingTool()
    {
      auto document = getFile("asset1.xml");
      auto asset = m_config->parseAsset("KSSP300R4SD43L240.1", "CuttingTool", document);
      CuttingToolPtr tool = (CuttingTool *) asset.getObject();
      
      
      vector<AssetPtr> assets;
      assets.push_back(asset);
      
      {
        PARSE_XML(XmlPrinter::printAssets(123, 4, 2, assets));
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Assets//m:CuttingTool@toolId", "KSSP300R4SD43L240");
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Assets//m:CuttingTool@removed", nullptr);
      }
    }
    
    
    void XmlPrinterTest::testPrintRemovedCuttingTool()
    {
      auto document = getFile("asset1.xml");
      auto asset = m_config->parseAsset("KSSP300R4SD43L240.1", "CuttingTool", document);
      asset->setRemoved(true);
      CuttingToolPtr tool = (CuttingTool *) asset.getObject();
      
      
      vector<AssetPtr> assets;
      assets.push_back(asset);
      
      {
        PARSE_XML(XmlPrinter::printAssets(123, 4, 2, assets));
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Assets//m:CuttingTool@removed", "true");
      }
    }
    
    
    // CuttingTool tests
    void XmlPrinterTest::testPrintExtendedCuttingTool()
    {
      XmlPrinter::addAssetsNamespace("urn:Example.com:Assets:1.3",
                                     "/schemas/MTConnectAssets_1.3.xsd",
                                     "x");
      
      auto document = getFile("ext_asset.xml");
      auto asset = m_config->parseAsset("B732A08500HP.1", "CuttingTool", document);
      CuttingToolPtr tool = (CuttingTool *) asset.getObject();
      
      vector<AssetPtr> assets;
      assets.push_back(asset);
      
      {
        PARSE_XML(XmlPrinter::printAssets(123, 4, 2, assets));
        CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Assets//x:Color", "BLUE");
      }
      
      XmlPrinter::clearAssetsNamespaces();
    }
  }
}
