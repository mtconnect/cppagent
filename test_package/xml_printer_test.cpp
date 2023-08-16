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

#include "mtconnect/asset/asset.hpp"
#include "mtconnect/buffer/checkpoint.hpp"
#include "mtconnect/device_model/data_item/data_item.hpp"
#include "mtconnect/device_model/device.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/parser/xml_parser.hpp"
#include "mtconnect/printer//xml_printer.hpp"
#include "mtconnect/utilities.hpp"
#include "test_utilities.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::buffer;
using namespace mtconnect::observation;
using namespace mtconnect::entity;
using namespace mtconnect::printer;
using namespace mtconnect::parser;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

Properties operator"" _value(const char *value, size_t s)
{
  return Properties {{"VALUE", string(value)}};
}

class XmlPrinterTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_config = new XmlParser();
    m_printer = new printer::XmlPrinter(true);
    m_printer->setSchemaVersion("1.2");
    m_devices = m_config->parseFile(TEST_RESOURCE_DIR "/samples/test_config.xml", m_printer);
  }

  void TearDown() override
  {
    delete m_config;
    m_config = nullptr;
    delete m_printer;
    m_printer = nullptr;
  }

  mtconnect::parser::XmlParser *m_config {nullptr};
  mtconnect::printer::XmlPrinter *m_printer {nullptr};
  std::list<mtconnect::DevicePtr> m_devices;

  // Construct a component event and set it as the data item's latest event
  ObservationPtr addEventToCheckpoint(Checkpoint &checkpoint, const char *name, uint64_t sequence,
                                      const Properties &props);

  ObservationPtr newEvent(const char *name, uint64_t sequence, const Properties &props);
};

ObservationPtr XmlPrinterTest::newEvent(const char *name, uint64_t sequence,
                                        const Properties &props)
{
  // Make sure the data item is there
  const auto device = m_devices.front();
  EXPECT_TRUE(device);

  const auto d = device->getDeviceDataItem(name);
  EXPECT_TRUE(d) << "Could not find data item " << name;
  entity::ErrorList errors;
  auto now = chrono::system_clock::now();
  auto o = Observation::make(d, props, now, errors);
  o->setSequence(sequence);
  return o;
}

ObservationPtr XmlPrinterTest::addEventToCheckpoint(Checkpoint &checkpoint, const char *name,
                                                    uint64_t sequence, const Properties &props)
{
  auto event = newEvent(name, sequence, props);
  checkpoint.addObservation(event);
  return event;
}

TEST_F(XmlPrinterTest, PrintError)
{
  PARSE_XML(m_printer->printError(123, 9999, 1, "ERROR_CODE", "ERROR TEXT!"));

  ASSERT_XML_PATH_EQUAL(doc, "//m:Header@instanceId", "123");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Header@bufferSize", "9999");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "ERROR_CODE");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "ERROR TEXT!");
}

TEST_F(XmlPrinterTest, PrintProbe)
{
  PARSE_XML(m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices));

  ASSERT_XML_PATH_EQUAL(doc, "//m:Header@instanceId", "123");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Header@bufferSize", "9999");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetBufferSize", "1024");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "10");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Header@deviceModelChangeTime", 0);

  // Check Description
  ASSERT_XML_PATH_EQUAL(doc, "//m:Description@manufacturer", "NIST");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Description@serialNumber", "1122");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Description", "Linux CNC Device");

  // Check Axes
  ASSERT_XML_PATH_EQUAL(doc, "//m:Axes@name", "Axes");

  // Check Spindle
  ASSERT_XML_PATH_EQUAL(doc, "//m:Rotary@name", "C");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Rotary/m:DataItems/m:DataItem@type", "SPINDLE_SPEED");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Rotary/m:DataItems/m:DataItem[@type='ROTARY_MODE']@name",
                        "Smode");
  ASSERT_XML_PATH_EQUAL(
      doc, "//m:Rotary/m:DataItems/m:DataItem[@type='ROTARY_MODE']/m:Constraints/m:Value",
      "SPINDLE");

  // Check Linear Axis
  ASSERT_XML_PATH_EQUAL(doc, "//m:Linear[@name='X']/m:DataItems/m:DataItem@type", "POSITION");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Linear[@name='X']/m:DataItems/m:DataItem@name", "Xact");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Linear[@name='X']/m:DataItems/m:DataItem@significantDigits", "6");
  ASSERT_XML_PATH_EQUAL(
      doc, "//m:Linear[@name='X']/m:DataItems/m:DataItem[@type='LOAD']//m:Maximum", "200");
  ASSERT_XML_PATH_EQUAL(
      doc, "//m:Linear[@name='X']/m:DataItems/m:DataItem[@type='LOAD']/m:Constraints/m:Minimum",
      "0");
  ASSERT_XML_PATH_EQUAL(
      doc, "//m:Linear[@name='X']/m:DataItems/m:DataItem[@type='LOAD']/m:Constraints/m:Maximum",
      "200");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Linear[@name='Z']/m:DataItems/m:DataItem@type", "POSITION");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Linear[@name='Z']/m:DataItems/m:DataItem@name", "Zact");

  // Check for Path component
  ASSERT_XML_PATH_EQUAL(
      doc, "//m:Controller//m:Path/m:DataItems/m:DataItem[@type='PATH_POSITION']@name", "Ppos");
  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='clc']@category", "CONDITION");

  // Check for composition ids
  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='zt1']@compositionId", "zmotor");
  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='zt2']@compositionId", "zamp");

  // check for compositions
  ASSERT_XML_PATH_EQUAL(doc, "//m:Composition[@id='zmotor']@type", "MOTOR");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Composition[@id='zmotor']@uuid", "12345");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Composition[@id='zmotor']@name", "motor_name");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Composition[@id='zmotor']/m:Description", "Hello There");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Composition[@id='zmotor']/m:Description@manufacturer", "open");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Composition[@id='zmotor']/m:Description@model", "vroom");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Composition[@id='zmotor']/m:Description@serialNumber", "12356");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Composition[@id='zmotor']/m:Description@station", "A");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Composition[@id='zamp']@type", "AMPLIFIER");
}

TEST_F(XmlPrinterTest, PrintDataItemElements)
{
  PARSE_XML(m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices));

  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='y1']/m:Filters/m:Filter[1]@type", "MINIMUM_DELTA");
  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='y1']/m:Filters/m:Filter[1]", "2");

  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='ylc']/m:Filters/m:Filter[1]@type", "PERIOD");
  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='ylc']/m:Filters/m:Filter[1]", "1");

  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='pcount']/m:InitialValue", "0");
  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='pcount']/m:ResetTrigger", "DAY");

  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='pcountrem']/m:ResetTrigger", "SHIFT");
}

TEST_F(XmlPrinterTest, PrintCurrent)
{
  Checkpoint checkpoint;
  addEventToCheckpoint(checkpoint, "Xact", 10254804, "0"_value);
  addEventToCheckpoint(checkpoint, "SspeedOvr", 15, "100"_value);
  addEventToCheckpoint(checkpoint, "Xcom", 10254803, "0"_value);
  addEventToCheckpoint(checkpoint, "spindle_speed", 16, "100"_value);
  addEventToCheckpoint(checkpoint, "Yact", 10254797, "0.00199"_value);
  addEventToCheckpoint(checkpoint, "Ycom", 10254800, "0.00189"_value);
  addEventToCheckpoint(checkpoint, "Zact", 10254798, "0.0002"_value);
  addEventToCheckpoint(checkpoint, "Zcom", 10254801, "0.0003"_value);
  addEventToCheckpoint(checkpoint, "block", 10254789, "x-0.132010 y-0.158143"_value);
  addEventToCheckpoint(checkpoint, "mode", 13, "AUTOMATIC"_value);
  addEventToCheckpoint(checkpoint, "line", 10254796, "0"_value);
  addEventToCheckpoint(checkpoint, "program", 12, "/home/mtconnect/simulator/spiral.ngc"_value);
  addEventToCheckpoint(checkpoint, "execution", 10254795, "READY"_value);
  addEventToCheckpoint(checkpoint, "power", 1, "ON"_value);

  ObservationList list;
  checkpoint.getObservations(list);
  PARSE_XML(m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list));

  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xact']",
                        "0");
  ASSERT_XML_PATH_EQUAL(
      doc, "//m:ComponentStream[@name='C']/m:Samples/m:SpindleSpeed[@name='Sovr']", "100");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xcom']",
                        "0");
  ASSERT_XML_PATH_EQUAL(
      doc, "//m:ComponentStream[@name='C']/m:Samples/m:SpindleSpeed[@name='Sspeed']", "100");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Y']/m:Samples/m:Position[@name='Yact']",
                        "0.00199");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Y']/m:Samples/m:Position[@name='Ycom']",
                        "0.00189");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Z']/m:Samples/m:Position[@name='Zact']",
                        "0.0002");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Z']/m:Samples/m:Position[@name='Zcom']",
                        "0.0003");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@componentId='path']/m:Events/m:Block",
                        "x-0.132010 y-0.158143");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@componentId='path']/m:Events/m:Execution",
                        "READY");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@componentId='path']/m:Events/m:ControllerMode",
                        "AUTOMATIC");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@componentId='path']/m:Events/m:Line", "0");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@componentId='path']/m:Events/m:Program",
                        "/home/mtconnect/simulator/spiral.ngc");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='power']/m:Events/m:PowerState", "ON");
}

TEST_F(XmlPrinterTest, ChangeDevicesNamespace)
{
  // Devices
  m_printer->clearDevicesNamespaces();

  {
    PARSE_XML(m_printer->printProbe(123, 9999, 1024, 10, 1, m_devices));
    ASSERT_XML_PATH_EQUAL(doc, "/m:MTConnectDevices@schemaLocation",
                          "urn:mtconnect.org:MTConnectDevices:1.2 "
                          "http://schemas.mtconnect.org/schemas/"
                          "MTConnectDevices_1.2.xsd");
  }

  {
    m_printer->addDevicesNamespace("urn:machine.com:MachineDevices:1.3",
                                   "http://www.machine.com/schemas/MachineDevices_1.3.xsd", "e");

    PARSE_XML(m_printer->printProbe(123, 9999, 1024, 10, 1, m_devices));

    ASSERT_XML_PATH_EQUAL(
        doc, "/m:MTConnectDevices@schemaLocation",
        "urn:machine.com:MachineDevices:1.3 http://www.machine.com/schemas/MachineDevices_1.3.xsd");

    m_printer->clearDevicesNamespaces();
  }

  {
    XmlParser ext;
    std::list<DevicePtr> extdevs =
        ext.parseFile(TEST_RESOURCE_DIR "/samples/extension.xml", m_printer);
    PARSE_XML(m_printer->printProbe(123, 9999, 1024, 10, 1, extdevs));

    ASSERT_XML_PATH_EQUAL(doc, "/m:MTConnectDevices@schemaLocation",
                          "urn:example.com:ExampleDevices:1.1 ExtensionDevices_1.1.xsd");

    ASSERT_XML_PATH_EQUAL(doc, "//m:Device//x:Pump@name", "pump");
  }

  m_printer->clearDevicesNamespaces();
}

TEST_F(XmlPrinterTest, ChangeStreamsNamespace)
{
  m_printer->clearStreamsNamespaces();

  Checkpoint checkpoint;
  addEventToCheckpoint(checkpoint, "Xact", 10254804, "0"_value);
  addEventToCheckpoint(checkpoint, "SspeedOvr", 15, "100"_value);
  addEventToCheckpoint(checkpoint, "Xcom", 10254803, "0"_value);

  // Streams
  {
    ObservationList list;
    checkpoint.getObservations(list);

    PARSE_XML(m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list));

    ASSERT_XML_PATH_EQUAL(doc, "/m:MTConnectStreams@schemaLocation",
                          "urn:mtconnect.org:MTConnectStreams:1.2 "
                          "http://schemas.mtconnect.org/schemas/"
                          "MTConnectStreams_1.2.xsd");
  }

  m_printer->clearStreamsNamespaces();

  {
    m_printer->addStreamsNamespace("urn:machine.com:MachineStreams:1.3",
                                   "http://www.machine.com/schemas/MachineStreams_1.3.xsd", "e");

    ObservationList list;
    checkpoint.getObservations(list);
    PARSE_XML(m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list));

    ASSERT_XML_PATH_EQUAL(
        doc, "/m:MTConnectStreams@schemaLocation",
        "urn:machine.com:MachineStreams:1.3 http://www.machine.com/schemas/MachineStreams_1.3.xsd");
  }

  m_printer->clearStreamsNamespaces();

  {
    XmlParser ext;
    m_devices = ext.parseFile(TEST_RESOURCE_DIR "/samples/extension.xml", m_printer);

    m_printer->addStreamsNamespace("urn:example.com:ExampleDevices:1.3", "ExtensionDevices_1.3.xsd",
                                   "x");

    Checkpoint checkpoint2;
    addEventToCheckpoint(checkpoint2, "flow", 10254804, "100"_value);

    ObservationList list;
    checkpoint2.getObservations(list);

    PARSE_XML(m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list));

    ASSERT_XML_PATH_EQUAL(doc, "//x:Flow", "100");
  }

  m_printer->clearStreamsNamespaces();

  {
    XmlParser ext;
    m_devices = ext.parseFile(TEST_RESOURCE_DIR "/samples/extension.xml", m_printer);

    m_printer->addStreamsNamespace("urn:example.com:ExampleDevices:1.3", "ExtensionDevices_1.3.xsd",
                                   "x");

    Checkpoint checkpoint2;
    addEventToCheckpoint(checkpoint2, "flow", 10254804, "100"_value);

    ObservationList list;
    checkpoint2.getObservations(list);

    PARSE_XML(m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list));

    ASSERT_XML_PATH_EQUAL(doc, "//x:Flow", "100");
  }

  m_printer->clearStreamsNamespaces();
  m_printer->clearDevicesNamespaces();
}

TEST_F(XmlPrinterTest, ChangeErrorNamespace)
{
  // Error

  {
    PARSE_XML(m_printer->printError(123, 9999, 1, "ERROR_CODE", "ERROR TEXT!"));
    ASSERT_XML_PATH_EQUAL(doc, "/m:MTConnectError@schemaLocation",
                          "urn:mtconnect.org:MTConnectError:1.2 "
                          "http://schemas.mtconnect.org/schemas/"
                          "MTConnectError_1.2.xsd");
  }

  {
    m_printer->addErrorNamespace("urn:machine.com:MachineError:1.3",
                                 "http://www.machine.com/schemas/MachineError_1.3.xsd", "e");

    PARSE_XML(m_printer->printError(123, 9999, 1, "ERROR_CODE", "ERROR TEXT!"));

    ASSERT_XML_PATH_EQUAL(
        doc, "/m:MTConnectError@schemaLocation",
        "urn:machine.com:MachineError:1.3 http://www.machine.com/schemas/MachineError_1.3.xsd");
  }
}

TEST_F(XmlPrinterTest, PrintSample)
{
  ObservationList events;

  ObservationPtr ptr;
  ptr = newEvent("Xact", 10843512, "0.553472"_value);
  events.push_back(ptr);
  ptr = newEvent("Xcom", 10843514, "0.551123"_value);
  events.push_back(ptr);
  ptr = newEvent("Xact", 10843516, "0.556826"_value);
  events.push_back(ptr);
  ptr = newEvent("Xcom", 10843518, "0.55582"_value);
  events.push_back(ptr);
  ptr = newEvent("Xact", 10843520, "0.560181"_value);
  events.push_back(ptr);
  ptr = newEvent("Yact", 10843513, "-0.900624"_value);
  events.push_back(ptr);
  ptr = newEvent("Ycom", 10843515, "-0.89692"_value);
  events.push_back(ptr);
  ptr = newEvent("Yact", 10843517, "-0.897574"_value);
  events.push_back(ptr);
  ptr = newEvent("Ycom", 10843519, "-0.894742"_value);
  events.push_back(ptr);
  ptr = newEvent("Xact", 10843521, "-0.895613"_value);
  events.push_back(ptr);
  ptr = newEvent("line", 11351720, "229"_value);
  events.push_back(ptr);
  ptr = newEvent("block", 11351726, "x-1.149250 y1.048981"_value);
  events.push_back(ptr);

  PARSE_XML(m_printer->printSample(123, 131072, 10974584, 10843512, 10123800, events));

  ASSERT_XML_PATH_EQUAL(doc,
                        "/m:MTConnectStreams/m:Streams/m:DeviceStream/"
                        "m:ComponentStream[@name='X']/m:Samples/"
                        "m:Position[@name='Xact'][1]",
                        "0.553472");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xact'][2]",
                        "0.556826");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xcom'][1]",
                        "0.551123");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xcom'][2]",
                        "0.55582");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xact'][3]",
                        "0.560181");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xact'][4]",
                        "-0.895613");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Y']/m:Samples/m:Position[@name='Yact'][1]",
                        "-0.900624");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Y']/m:Samples/m:Position[@name='Yact'][2]",
                        "-0.897574");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Y']/m:Samples/m:Position[@name='Ycom'][1]",
                        "-0.89692");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Y']/m:Samples/m:Position[@name='Ycom'][2]",
                        "-0.894742");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@componentId='path']/m:Events/m:Line", "229");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@componentId='path']/m:Events/m:Block",
                        "x-1.149250 y1.048981");
}

TEST_F(XmlPrinterTest, Condition)
{
  Checkpoint checkpoint;
  addEventToCheckpoint(checkpoint, "Xact", 10254804, "0"_value);
  addEventToCheckpoint(checkpoint, "SspeedOvr", 15, "100"_value);
  addEventToCheckpoint(checkpoint, "Xcom", 10254803, "0"_value);
  addEventToCheckpoint(checkpoint, "spindle_speed", 16, "100"_value);
  addEventToCheckpoint(checkpoint, "Yact", 10254797, "0.00199"_value);
  addEventToCheckpoint(checkpoint, "Ycom", 10254800, "0.00189"_value);
  addEventToCheckpoint(checkpoint, "Zact", 10254798, "0.0002"_value);
  addEventToCheckpoint(checkpoint, "Zcom", 10254801, "0.0003"_value);
  addEventToCheckpoint(checkpoint, "block", 10254789, "x-0.132010 y-0.158143"_value);
  addEventToCheckpoint(checkpoint, "mode", 13, "AUTOMATIC"_value);
  addEventToCheckpoint(checkpoint, "line", 10254796, "0"_value);
  addEventToCheckpoint(checkpoint, "program", 12, "/home/mtconnect/simulator/spiral.ngc"_value);
  addEventToCheckpoint(checkpoint, "execution", 10254795, "READY"_value);
  addEventToCheckpoint(checkpoint, "power", 1, "ON"_value);

  addEventToCheckpoint(checkpoint, "ctmp", 18,
                       Properties {{{"level", "WARNING"s},
                                    {"nativeCode", "OTEMP"s},
                                    {"nativeSeverity", "1"s},
                                    {"qualifier", "HIGH"s},
                                    {"VALUE", "Spindle Overtemp"s}}});
  addEventToCheckpoint(checkpoint, "cmp", 18,
                       Properties {{
                           {"level", "NORMAL"s},
                       }});
  addEventToCheckpoint(checkpoint, "lp", 18,
                       Properties {{{"level", "FAULT"s},
                                    {"nativeCode", "LOGIC"s},
                                    {"nativeSeverity", "2"s},
                                    {"VALUE", "PLC Error"s}}});

  ObservationList list;
  checkpoint.getObservations(list);
  PARSE_XML(m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list));

  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='C']/m:Condition/m:Warning",
                        "Spindle Overtemp");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='C']/m:Condition/m:Warning@type",
                        "TEMPERATURE");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='C']/m:Condition/m:Warning@qualifier",
                        "HIGH");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='C']/m:Condition/m:Warning@nativeCode",
                        "OTEMP");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='C']/m:Condition/m:Warning@nativeSeverity",
                        "1");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@componentId='path']/m:Condition/m:Normal",
                        nullptr);
  ASSERT_XML_PATH_EQUAL(
      doc, "//m:ComponentStream[@componentId='path']/m:Condition/m:Normal@qualifier", nullptr);
  ASSERT_XML_PATH_EQUAL(
      doc, "//m:ComponentStream[@componentId='path']/m:Condition/m:Normal@nativeCode", nullptr);
  ASSERT_XML_PATH_EQUAL(
      doc, "//m:ComponentStream[@name='Controller']/m:Condition/m:Fault@nativeCode", "LOGIC");
  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='Controller']/m:Condition/m:Fault",
                        "PLC Error");
  ASSERT_XML_PATH_EQUAL(
      doc, "//m:ComponentStream[@name='Controller']/m:Condition/m:Fault@qualifier", nullptr);
  ASSERT_XML_PATH_EQUAL(
      doc, "//m:ComponentStream[@name='Controller']/m:Condition/m:Fault@nativeSeverity", "2");
}

TEST_F(XmlPrinterTest, VeryLargeSequence)
{
  Checkpoint checkpoint;
  addEventToCheckpoint(checkpoint, "Xact", (((uint64_t)1) << 48) + 1, "0"_value);
  addEventToCheckpoint(checkpoint, "Xcom", (((uint64_t)1) << 48) + 3, "123"_value);

  ObservationList list;
  checkpoint.getObservations(list);
  PARSE_XML(m_printer->printSample(123, 131072, (((uint64_t)1) << 48) + 3,
                                   (((uint64_t)1) << 48) + 1, (((uint64_t)1) << 48) + 1024, list));

  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xact']",
                        "0");
  ASSERT_XML_PATH_EQUAL(
      doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xact']@sequence",
      "281474976710657");

  ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xcom']",
                        "123");
  ASSERT_XML_PATH_EQUAL(
      doc, "//m:ComponentStream[@name='X']/m:Samples/m:Position[@name='Xcom']@sequence",
      "281474976710659");

  ASSERT_XML_PATH_EQUAL(doc, "//m:Header@firstSequence", "281474976710657");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence", "281474976710659");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Header@lastSequence", "281474976711680");
}

TEST_F(XmlPrinterTest, ChangeDeviceAttributes)
{
  auto device = m_devices.front();

  string v = "Some_Crazy_Uuid";
  device->setUuid(v);
  ErrorList errors;
  auto description =
      mtconnect::device_model::Device::getFactory()->create("Description",
                                                            {{"manufacturer", "Big Tool MFG"s},
                                                             {"serialNumber", "111999333444"s},
                                                             {"station", "99999999"s}},
                                                            errors);
  ASSERT_TRUE(errors.empty());
  device->setProperty("Description", description);

  PARSE_XML(m_printer->printProbe(123, 9999, 1024, 10, 1, m_devices));

  // Check Description
  ASSERT_XML_PATH_EQUAL(doc, "//m:Device@uuid", "Some_Crazy_Uuid");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Description@manufacturer", "Big Tool MFG");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Description@serialNumber", "111999333444");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Description@station", "99999999");
}

TEST_F(XmlPrinterTest, StatisticAndTimeSeriesProbe)
{
  PARSE_XML(m_printer->printProbe(123, 9999, 1024, 10, 1, m_devices));

  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='Xact']@statistic", "AVERAGE");
  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='Xts']@representation", "TIME_SERIES");
  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='Xts']@sampleRate", "46000");
}

TEST_F(XmlPrinterTest, TimeSeries)
{
  ObservationPtr ptr;
  {
    ObservationList events;
    ptr = newEvent("Xts", 10843512,
                   Properties {{"sampleCount", int64_t(6)}, {"VALUE", "1.1 2.2 3.3 4.4 5.5 6.6"s}});
    events.push_back(ptr);

    PARSE_XML(m_printer->printSample(123, 131072, 10974584, 10843512, 10123800, events));
    ASSERT_XML_PATH_EQUAL(
        doc, "//m:ComponentStream[@name='X']/m:Samples/m:PositionTimeSeries@sampleRate", nullptr);
    ASSERT_XML_PATH_EQUAL(
        doc, "//m:ComponentStream[@name='X']/m:Samples/m:PositionTimeSeries@sampleCount", "6");
    ASSERT_XML_PATH_EQUAL(
        doc, "//m:ComponentStream[@name='X']/m:Samples/m:PositionTimeSeries[@name='Xts']",
        "1.1 2.2 3.3 4.4 5.5 6.6");
  }
  {
    ObservationList events;
    ptr = newEvent("Xts", 10843512,
                   Properties {{"sampleCount", int64_t(6)},
                               {"sampleRate", 46200.0},
                               {"VALUE", "1.1 2.2 3.3 4.4 5.5 6.6"s}});

    events.push_back(ptr);

    PARSE_XML(m_printer->printSample(123, 131072, 10974584, 10843512, 10123800, events));
    ASSERT_XML_PATH_EQUAL(
        doc, "//m:ComponentStream[@name='X']/m:Samples/m:PositionTimeSeries@sampleRate", "46200");
    ASSERT_XML_PATH_EQUAL(
        doc, "//m:ComponentStream[@name='X']/m:Samples/m:PositionTimeSeries@sampleCount", "6");
    ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@name='X']/m:Samples/m:PositionTimeSeries",
                          "1.1 2.2 3.3 4.4 5.5 6.6");
  }
}

TEST_F(XmlPrinterTest, NonPrintableCharacters)
{
  ObservationList events;
  ObservationPtr ptr = newEvent(
      "zlc", 10843512,
      Properties {{{"level", "fault"s}, {"nativeCode", "500"s}, {"VALUE", "OVER TRAVEL : +Z? "s}}});

  events.push_back(ptr);
  PARSE_XML(m_printer->printSample(123, 131072, 10974584, 10843512, 10123800, events));
  ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@name='Z']/m:Condition//*[1]",
                        "OVER TRAVEL : +Z?");
}

TEST_F(XmlPrinterTest, EscapedXMLCharacters)
{
  ObservationList events;
  ObservationPtr ptr = newEvent(
      "zlc", 10843512,
      Properties {
          {{"level", "fault"s}, {"nativeCode", "500"s}, {"VALUE", "A duck > a foul & < cat '"s}}});

  events.push_back(ptr);
  PARSE_XML(m_printer->printSample(123, 131072, 10974584, 10843512, 10123800, events));
  ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@name='Z']/m:Condition//*[1]",
                        "A duck > a foul & < cat '");
}

TEST_F(XmlPrinterTest, PrintAssetProbe)
{
  // Add the xml to the agent...
  map<string, size_t> counts;
  counts["CuttingTool"] = 10;

  PARSE_XML(m_printer->printProbe(123, 9999, 1024, 10, 1, m_devices, &counts));

  ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCounts/m:AssetCount", "10");
  ASSERT_XML_PATH_EQUAL(doc, "//m:AssetCounts/m:AssetCount@assetType", "CuttingTool");
}

TEST_F(XmlPrinterTest, Configuration)
{
  PARSE_XML(m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices));

  ASSERT_XML_PATH_EQUAL(doc, "//m:Power/m:Configuration/m:SensorConfiguration/m:CalibrationDate",
                        "2011-08-10");
  ASSERT_XML_PATH_EQUAL(doc, "//m:SensorConfiguration/m:Channels/m:Channel@number", "1");
  ASSERT_XML_PATH_EQUAL(doc, "//m:SensorConfiguration/m:Channels/m:Channel/m:Description",
                        "Power Channel");
}

// Schema tests
TEST_F(XmlPrinterTest, ChangeVersion)
{
  // Devices
  m_printer->clearDevicesNamespaces();

  {
    PARSE_XML(m_printer->printProbe(123, 9999, 1024, 10, 1, m_devices));
    ASSERT_XML_PATH_EQUAL(doc, "/m:MTConnectDevices@schemaLocation",
                          "urn:mtconnect.org:MTConnectDevices:1.2 "
                          "http://schemas.mtconnect.org/schemas/"
                          "MTConnectDevices_1.2.xsd");
  }

  m_printer->setSchemaVersion("1.4");

  {
    PARSE_XML(m_printer->printProbe(123, 9999, 1024, 10, 1, m_devices));
    ASSERT_XML_PATH_EQUAL(doc, "/m:MTConnectDevices@schemaLocation",
                          "urn:mtconnect.org:MTConnectDevices:1.4 "
                          "http://schemas.mtconnect.org/schemas/"
                          "MTConnectDevices_1.4.xsd");
  }

  m_printer->setSchemaVersion("1.3");
}

TEST_F(XmlPrinterTest, ChangeMTCLocation)
{
  m_printer->clearDevicesNamespaces();

  m_printer->setSchemaVersion("1.3");

  m_printer->addDevicesNamespace("urn:mtconnect.org:MTConnectDevices:1.3",
                                 "/schemas/MTConnectDevices_1.3.xsd", "m");

  {
    PARSE_XML(m_printer->printProbe(123, 9999, 1024, 10, 1, m_devices));
    ASSERT_XML_PATH_EQUAL(
        doc, "/m:MTConnectDevices@schemaLocation",
        "urn:mtconnect.org:MTConnectDevices:1.3 /schemas/MTConnectDevices_1.3.xsd");
  }

  m_printer->clearDevicesNamespaces();
  m_printer->setSchemaVersion("1.3");
}

TEST_F(XmlPrinterTest, ProbeWithFilter13)
{
  delete m_config;

  m_config = new XmlParser();
  m_devices = m_config->parseFile(TEST_RESOURCE_DIR "/samples/filter_example_1.3.xml", m_printer);

  PARSE_XML(m_printer->printProbe(123, 9999, 1024, 10, 1, m_devices));

  // TODO: Should we auto-upgrade?
  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='load']/m:Constraints/m:Filter", "5");
  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='load']/m:Constraints/m:Filter@type",
                        "MINIMUM_DELTA");
}

TEST_F(XmlPrinterTest, ProbeWithFilter)
{
  delete m_config;
  m_config = nullptr;

  m_config = new XmlParser();
  m_devices = m_config->parseFile(TEST_RESOURCE_DIR "/samples/filter_example.xml", m_printer);

  PARSE_XML(m_printer->printProbe(123, 9999, 1024, 10, 1, m_devices));

  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='load']/m:Filters/m:Filter", "5");
  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='load']/m:Filters/m:Filter@type", "MINIMUM_DELTA");
  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='pos']/m:Filters/m:Filter", "10");
  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@name='pos']/m:Filters/m:Filter@type", "PERIOD");
}

TEST_F(XmlPrinterTest, References)
{
  m_printer->setSchemaVersion("1.4");
  delete m_config;
  m_config = nullptr;

  m_config = new XmlParser();
  m_devices = m_config->parseFile(TEST_RESOURCE_DIR "/samples/reference_example.xml", m_printer);

  PARSE_XML(m_printer->printProbe(123, 9999, 1024, 10, 1, m_devices));

  ASSERT_XML_PATH_EQUAL(doc, "//m:BarFeederInterface/m:References/m:DataItemRef@idRef", "c4");
  ASSERT_XML_PATH_EQUAL(doc, "//m:BarFeederInterface/m:References/m:DataItemRef@name", "chuck");
  ASSERT_XML_PATH_EQUAL(doc, "//m:BarFeederInterface/m:References/m:ComponentRef@idRef", "ele");
}

TEST_F(XmlPrinterTest, LegacyReferences)
{
  m_printer->setSchemaVersion("1.3");
  delete m_config;
  m_config = nullptr;

  m_config = new XmlParser();
  m_devices = m_config->parseFile(TEST_RESOURCE_DIR "/samples/reference_example.xml", m_printer);

  PARSE_XML(m_printer->printProbe(123, 9999, 1024, 10, 1, m_devices));

  // TODO: LegacyReferences with entities
  // ASSERT_XML_PATH_EQUAL(doc, "//m:BarFeederInterface/m:References/m:Reference@dataItemId", "c4");
  // ASSERT_XML_PATH_EQUAL(doc, "//m:BarFeederInterface/m:References/m:Reference@name", "chuck");
}

TEST_F(XmlPrinterTest, CheckDeviceChangeTime)
{
  m_printer = new XmlPrinter(true);
  m_printer->setSchemaVersion("1.7");
  m_devices = m_config->parseFile(TEST_RESOURCE_DIR "/samples/test_config.xml", m_printer);
  m_printer->setModelChangeTime(getCurrentTime(GMT_UV_SEC));
  ASSERT_FALSE(m_printer->getModelChangeTime().empty());

  {
    PARSE_XML(m_printer->printProbe(123, 9999, 1024, 10, 1, m_devices));
    ASSERT_XML_PATH_EQUAL(doc, "/m:MTConnectDevices@schemaLocation",
                          "urn:mtconnect.org:MTConnectDevices:1.7 "
                          "http://schemas.mtconnect.org/schemas/"
                          "MTConnectDevices_1.7.xsd");
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@deviceModelChangeTime",
                          m_printer->getModelChangeTime().c_str());
  }
}

TEST_F(XmlPrinterTest, SourceReferences)
{
  delete m_config;
  m_config = nullptr;

  m_config = new XmlParser();
  m_devices = m_config->parseFile(TEST_RESOURCE_DIR "/samples/reference_example.xml", m_printer);

  PARSE_XML(m_printer->printProbe(123, 9999, 1024, 10, 1, m_devices));

  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='bfc']/m:Source@dataItemId", "mf");
  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='bfc']/m:Source@componentId", "ele");
  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='bfc']/m:Source@compositionId", "xxx");
}

TEST_F(XmlPrinterTest, StreamsStyle)
{
  m_printer->setStreamStyle("/styles/Streams.xsl");
  Checkpoint checkpoint;
  addEventToCheckpoint(checkpoint, "Xact", 10254804, "0"_value);
  addEventToCheckpoint(checkpoint, "SspeedOvr", 15, "100"_value);
  addEventToCheckpoint(checkpoint, "Xcom", 10254803, "0"_value);
  addEventToCheckpoint(checkpoint, "spindle_speed", 16, "100"_value);

  ObservationList list;
  checkpoint.getObservations(list);
  PARSE_XML(m_printer->printSample(123, 131072, 10254805, 10123733, 10123800, list));

  xmlNodePtr pi = doc->children;
  ASSERT_EQ(string("xml-stylesheet"), string((const char *)pi->name));
  ASSERT_EQ(string("type=\"text/xsl\" href=\"/styles/Streams.xsl\""),
            string((const char *)pi->content));

  m_printer->setStreamStyle("");
}

TEST_F(XmlPrinterTest, DevicesStyle)
{
  m_printer->setDevicesStyle("/styles/Devices.xsl");

  PARSE_XML(m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices));

  xmlNodePtr pi = doc->children;
  ASSERT_EQ(string("xml-stylesheet"), string((const char *)pi->name));
  ASSERT_EQ(string("type=\"text/xsl\" href=\"/styles/Devices.xsl\""),
            string((const char *)pi->content));

  m_printer->setDevicesStyle("");
}

TEST_F(XmlPrinterTest, ErrorStyle)
{
  m_printer->setErrorStyle("/styles/Error.xsl");

  PARSE_XML(m_printer->printError(123, 9999, 1, "ERROR_CODE", "ERROR TEXT!"));

  xmlNodePtr pi = doc->children;
  ASSERT_EQ(string("xml-stylesheet"), string((const char *)pi->name));
  ASSERT_EQ(string("type=\"text/xsl\" href=\"/styles/Error.xsl\""),
            string((const char *)pi->content));

  m_printer->setErrorStyle("");
}

TEST_F(XmlPrinterTest, PrintDeviceMTConnectVersion)
{
  PARSE_XML(m_printer->printProbe(123, 9999, 1, 1024, 10, m_devices));

  ASSERT_XML_PATH_EQUAL(doc, "//m:Device@mtconnectVersion", "1.7");
}

TEST_F(XmlPrinterTest, PrintDataItemRelationships)
{
  delete m_config;
  m_config = nullptr;

  m_config = new XmlParser();
  m_devices = m_config->parseFile(TEST_RESOURCE_DIR "/samples/relationship_test.xml", m_printer);

  PARSE_XML(m_printer->printProbe(123, 9999, 1024, 10, 1, m_devices));

  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='xlc']/m:Relationships/m:DataItemRelationship@name",
                        "archie");
  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='xlc']/m:Relationships/m:DataItemRelationship@type",
                        "LIMIT");
  ASSERT_XML_PATH_EQUAL(doc, "//m:DataItem[@id='xlc']/m:Relationships/m:DataItemRelationship@idRef",
                        "xlcpl");

  ASSERT_XML_PATH_EQUAL(
      doc, "//m:DataItem[@id='xlc']/m:Relationships/m:SpecificationRelationship@name", nullptr);
  ASSERT_XML_PATH_EQUAL(
      doc, "//m:DataItem[@id='xlc']/m:Relationships/m:SpecificationRelationship@type", "LIMIT");
  ASSERT_XML_PATH_EQUAL(
      doc, "//m:DataItem[@id='xlc']/m:Relationships/m:SpecificationRelationship@idRef", "spec1");

  ASSERT_XML_PATH_EQUAL(
      doc, "//m:DataItem[@id='xlcpl']/m:Relationships/m:DataItemRelationship@name", "bob");
  ASSERT_XML_PATH_EQUAL(
      doc, "//m:DataItem[@id='xlcpl']/m:Relationships/m:DataItemRelationship@type", "OBSERVATION");
  ASSERT_XML_PATH_EQUAL(
      doc, "//m:DataItem[@id='xlcpl']/m:Relationships/m:DataItemRelationship@idRef", "xlc");
}
