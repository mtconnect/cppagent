//
// Copyright Copyright 2009-2026, AMT – The Association For Manufacturing Technology ("AMT")
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

#include <iostream>
#include <memory>
#include <string>

#include "mtconnect/device_model/component.hpp"
#include "mtconnect/device_model/data_item/data_item.hpp"
#include "mtconnect/device_model/device.hpp"
#include "mtconnect/parser/json_parser.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::device_model;
using namespace mtconnect::device_model::data_item;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class JsonDeviceParserTest : public testing::Test
{
protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(JsonDeviceParserTest, should_parse_v1_mtconnect_devices_document)
{
  auto doc = R"(
{
  "MTConnectDevices": {
    "schemaVersion": "2.3",
    "Header": {"instanceId": 123, "bufferSize": 9999, "sender": "test"},
    "Devices": [
      {"Device": {"id": "d1", "name": "Machine1", "uuid": "aaa-111",
        "Components": [
          {"Axes": {"id": "a1"}}
        ]
      }},
      {"Device": {"id": "d2", "name": "Machine2", "uuid": "bbb-222"}}
    ]
  }
}
)";

  parser::JsonParser parser(1);
  auto devices = parser.parseDocument(doc);
  ASSERT_EQ(2, devices.size());

  ASSERT_EQ("2.3", *parser.getSchemaVersion());

  auto it = devices.begin();
  ASSERT_EQ("d1", (*it)->get<string>("id"));
  ASSERT_EQ("Machine1", (*it)->get<string>("name"));

  auto components = (*it)->getList("Components");
  ASSERT_TRUE(components);
  ASSERT_EQ(1, components->size());
  ASSERT_EQ("Axes", string(components->front()->getName()));

  it++;
  ASSERT_EQ("d2", (*it)->get<string>("id"));
  ASSERT_EQ("Machine2", (*it)->get<string>("name"));
}

TEST_F(JsonDeviceParserTest, should_parse_v2_mtconnect_devices_document)
{
  auto doc = R"(
{
  "MTConnectDevices": {
    "schemaVersion": "2.3",
    "Header": {"instanceId": 123, "bufferSize": 9999, "sender": "test"},
    "Devices": {
      "Device": [
        {"id": "d1", "name": "Machine1", "uuid": "aaa-111",
          "Components": {
            "Axes": [{"id": "a1"}]
          }
        },
        {"id": "d2", "name": "Machine2", "uuid": "bbb-222"}
      ]
    }
  }
}
)";

  parser::JsonParser parser(2);
  auto devices = parser.parseDocument(doc);
  ASSERT_EQ(2, devices.size());

  ASSERT_EQ("2.3", *parser.getSchemaVersion());

  auto it = devices.begin();
  ASSERT_EQ("d1", (*it)->get<string>("id"));
  ASSERT_EQ("Machine1", (*it)->get<string>("name"));

  auto components = (*it)->getList("Components");
  ASSERT_TRUE(components);
  ASSERT_EQ(1, components->size());
  ASSERT_EQ("Axes", string(components->front()->getName()));

  it++;
  ASSERT_EQ("d2", (*it)->get<string>("id"));
  ASSERT_EQ("Machine2", (*it)->get<string>("name"));
}

TEST_F(JsonDeviceParserTest, should_detect_json_version_from_document)
{
  // V2 document with jsonVersion field — parser should auto-detect version 2
  auto doc = R"(
{
  "MTConnectDevices": {
    "jsonVersion": 2,
    "schemaVersion": "2.3",
    "Header": {"instanceId": 123},
    "Devices": {
      "Device": [
        {"id": "d1", "name": "Machine1", "uuid": "aaa-111",
          "Components": {
            "Axes": [{"id": "a1"}]
          }
        }
      ]
    }
  }
}
)";

  // Default version is 1, but jsonVersion in the document overrides it
  parser::JsonParser parser;
  auto devices = parser.parseDocument(doc);
  ASSERT_EQ(1, devices.size());

  auto device = devices.front();
  ASSERT_EQ("d1", device->get<string>("id"));

  auto components = device->getList("Components");
  ASSERT_TRUE(components);
  ASSERT_EQ(1, components->size());
  ASSERT_EQ("Axes", string(components->front()->getName()));
}

TEST_F(JsonDeviceParserTest, should_default_to_v1_when_json_version_absent)
{
  auto doc = R"(
{
  "MTConnectDevices": {
    "schemaVersion": "2.3",
    "Header": {"instanceId": 123},
    "Devices": [
      {"Device": {"id": "d1", "name": "Machine1", "uuid": "aaa-111"}}
    ]
  }
}
)";

  // No jsonVersion in document, should use default (v1)
  parser::JsonParser parser;
  auto devices = parser.parseDocument(doc);
  ASSERT_EQ(1, devices.size());
  ASSERT_EQ("d1", devices.front()->get<string>("id"));
}

TEST_F(JsonDeviceParserTest, should_override_v1_default_with_v2_from_document)
{
  // Parser defaults to v1, but the document specifies jsonVersion 2
  auto doc = R"(
{
  "MTConnectDevices": {
    "jsonVersion": 2,
    "Devices": {
      "Device": [
        {"id": "d1", "name": "Machine1", "uuid": "aaa-111",
          "Components": {
            "Axes": [{"id": "a1"}]
          }
        }
      ]
    }
  }
}
)";

  parser::JsonParser parser(1);
  auto devices = parser.parseDocument(doc);
  ASSERT_EQ(1, devices.size());

  auto device = devices.front();
  ASSERT_EQ("d1", device->get<string>("id"));

  auto components = device->getList("Components");
  ASSERT_TRUE(components);
  ASSERT_EQ(1, components->size());
  ASSERT_EQ("Axes", string(components->front()->getName()));
}

TEST_F(JsonDeviceParserTest, should_override_v2_default_with_v1_from_document)
{
  // Parser defaults to v2, but the document specifies jsonVersion 1
  auto doc = R"(
{
  "MTConnectDevices": {
    "jsonVersion": 1,
    "Devices": [
      {"Device": {"id": "d1", "name": "Machine1", "uuid": "aaa-111",
        "Components": [
          {"Axes": {"id": "a1"}}
        ]
      }}
    ]
  }
}
)";

  parser::JsonParser parser(2);
  auto devices = parser.parseDocument(doc);
  ASSERT_EQ(1, devices.size());

  auto device = devices.front();
  ASSERT_EQ("d1", device->get<string>("id"));

  auto components = device->getList("Components");
  ASSERT_TRUE(components);
  ASSERT_EQ(1, components->size());
  ASSERT_EQ("Axes", string(components->front()->getName()));
}

TEST_F(JsonDeviceParserTest, should_not_mutate_default_version_after_override)
{
  // First parse a v2 document that overrides the default
  auto v2Doc = R"(
{
  "MTConnectDevices": {
    "jsonVersion": 2,
    "Devices": {
      "Device": [{"id": "d1", "name": "M1", "uuid": "aaa"}]
    }
  }
}
)";

  parser::JsonParser parser(1);
  auto devices = parser.parseDocument(v2Doc);
  ASSERT_EQ(1, devices.size());

  // Now parse a v1 document without jsonVersion — should still use the original default (v1)
  auto v1Doc = R"(
{
  "MTConnectDevices": {
    "Devices": [
      {"Device": {"id": "d2", "name": "M2", "uuid": "bbb"}}
    ]
  }
}
)";

  devices = parser.parseDocument(v1Doc);
  ASSERT_EQ(1, devices.size());
  ASSERT_EQ("d2", devices.front()->get<string>("id"));
}

TEST_F(JsonDeviceParserTest, should_throw_when_document_missing_mtconnect_devices)
{
  auto doc = R"({"SomethingElse": {}})";

  parser::JsonParser parser(1);
  ASSERT_THROW(parser.parseDocument(doc), FatalException);
}

TEST_F(JsonDeviceParserTest, should_return_empty_list_when_no_devices)
{
  auto doc = R"(
{
  "MTConnectDevices": {
    "Header": {"instanceId": 123}
  }
}
)";

  parser::JsonParser parser(1);
  auto devices = parser.parseDocument(doc);
  ASSERT_TRUE(devices.empty());
}

TEST_F(JsonDeviceParserTest, should_parse_v1_device_with_components)
{
  auto doc = R"(
{
  "Device": {
    "id": "d1", "name": "MyDevice", "uuid": "xxx-yyy-zzz",
    "Components": [
      {"Systems": {"id": "s1",
        "Components": [
          {"Electric": {"id": "e1"}},
          {"Hydraulic": {"id": "h1"}}
        ]
      }}
    ]
  }
}
)";

  parser::JsonParser parser(1);
  auto device = parser.parseDevice(doc);
  ASSERT_TRUE(device);
  ASSERT_EQ("Device", string(device->getName()));
  ASSERT_EQ("d1", device->get<string>("id"));
  ASSERT_EQ("MyDevice", device->get<string>("name"));
  ASSERT_EQ("xxx-yyy-zzz", device->get<string>("uuid"));

  auto components = device->getList("Components");
  ASSERT_TRUE(components);
  ASSERT_EQ(1, components->size());

  auto systems = dynamic_pointer_cast<Component>(components->front());
  ASSERT_TRUE(systems);
  ASSERT_EQ("Systems", string(systems->getName()));
  ASSERT_EQ("s1", systems->get<string>("id"));

  auto subComponents = systems->getList("Components");
  ASSERT_TRUE(subComponents);
  ASSERT_EQ(2, subComponents->size());

  auto it = subComponents->begin();
  ASSERT_EQ("Electric", string((*it)->getName()));
  ASSERT_EQ("e1", (*it)->get<string>("id"));

  it++;
  ASSERT_EQ("Hydraulic", string((*it)->getName()));
  ASSERT_EQ("h1", (*it)->get<string>("id"));
}

TEST_F(JsonDeviceParserTest, should_parse_v2_device_with_components)
{
  auto doc = R"(
{
  "Device": {
    "id": "d1", "name": "MyDevice", "uuid": "xxx-yyy-zzz",
    "Components": {
      "Systems": [{"id": "s1",
        "Components": {
          "Electric": [{"id": "e1"}],
          "Hydraulic": [{"id": "h1"}]
        }
      }]
    }
  }
}
)";

  parser::JsonParser parser(2);
  auto device = parser.parseDevice(doc);
  ASSERT_TRUE(device);
  ASSERT_EQ("Device", string(device->getName()));
  ASSERT_EQ("d1", device->get<string>("id"));
  ASSERT_EQ("MyDevice", device->get<string>("name"));
  ASSERT_EQ("xxx-yyy-zzz", device->get<string>("uuid"));

  auto components = device->getList("Components");
  ASSERT_TRUE(components);
  ASSERT_EQ(1, components->size());

  auto systems = dynamic_pointer_cast<Component>(components->front());
  ASSERT_TRUE(systems);
  ASSERT_EQ("Systems", string(systems->getName()));
  ASSERT_EQ("s1", systems->get<string>("id"));

  auto subComponents = systems->getList("Components");
  ASSERT_TRUE(subComponents);
  ASSERT_EQ(2, subComponents->size());

  auto it = subComponents->begin();
  ASSERT_EQ("Electric", string((*it)->getName()));
  ASSERT_EQ("e1", (*it)->get<string>("id"));

  it++;
  ASSERT_EQ("Hydraulic", string((*it)->getName()));
  ASSERT_EQ("h1", (*it)->get<string>("id"));
}

TEST_F(JsonDeviceParserTest, should_parse_v1_device_with_data_items)
{
  auto doc = R"(
{
  "Device": {
    "id": "d1", "name": "MyDevice", "uuid": "xxx-yyy-zzz",
    "DataItems": [
      {"DataItem": {"id": "avail", "type": "AVAILABILITY", "category": "EVENT"}},
      {"DataItem": {"id": "xpos", "type": "POSITION", "category": "SAMPLE",
                    "subType": "ACTUAL", "units": "MILLIMETER"}}
    ]
  }
}
)";

  parser::JsonParser parser(1);
  auto device = parser.parseDevice(doc);
  ASSERT_TRUE(device);
  ASSERT_EQ("d1", device->get<string>("id"));

  auto dataItems = device->getList("DataItems");
  ASSERT_TRUE(dataItems);
  ASSERT_EQ(2, dataItems->size());

  auto it = dataItems->begin();
  auto di = dynamic_pointer_cast<DataItem>(*it);
  ASSERT_TRUE(di);
  ASSERT_EQ("avail", di->getId());
  ASSERT_EQ("AVAILABILITY", di->getType());

  it++;
  di = dynamic_pointer_cast<DataItem>(*it);
  ASSERT_TRUE(di);
  ASSERT_EQ("xpos", di->getId());
  ASSERT_EQ("POSITION", di->getType());
  ASSERT_EQ("MILLIMETER", di->get<string>("units"));
}

TEST_F(JsonDeviceParserTest, should_parse_v2_device_with_data_items)
{
  auto doc = R"(
{
  "Device": {
    "id": "d1", "name": "MyDevice", "uuid": "xxx-yyy-zzz",
    "DataItems": {
      "DataItem": [
        {"id": "avail", "type": "AVAILABILITY", "category": "EVENT"},
        {"id": "xpos", "type": "POSITION", "category": "SAMPLE",
         "subType": "ACTUAL", "units": "MILLIMETER"}
      ]
    }
  }
}
)";

  parser::JsonParser parser(2);
  auto device = parser.parseDevice(doc);
  ASSERT_TRUE(device);
  ASSERT_EQ("d1", device->get<string>("id"));

  auto dataItems = device->getList("DataItems");
  ASSERT_TRUE(dataItems);
  ASSERT_EQ(2, dataItems->size());

  auto it = dataItems->begin();
  auto di = dynamic_pointer_cast<DataItem>(*it);
  ASSERT_TRUE(di);
  ASSERT_EQ("avail", di->getId());
  ASSERT_EQ("AVAILABILITY", di->getType());

  it++;
  di = dynamic_pointer_cast<DataItem>(*it);
  ASSERT_TRUE(di);
  ASSERT_EQ("xpos", di->getId());
  ASSERT_EQ("POSITION", di->getType());
  ASSERT_EQ("MILLIMETER", di->get<string>("units"));
}

TEST_F(JsonDeviceParserTest, should_parse_v1_device_with_description)
{
  auto doc = R"(
{
  "Device": {
    "id": "d1", "name": "MyDevice", "uuid": "xxx-yyy-zzz",
    "Description": {"manufacturer": "ACME", "model": "X100",
                    "value": "A test device"}
  }
}
)";

  parser::JsonParser parser(1);
  auto device = parser.parseDevice(doc);
  ASSERT_TRUE(device);

  auto &description = device->get<entity::EntityPtr>("Description");
  ASSERT_TRUE(description);
  ASSERT_EQ("ACME", description->get<string>("manufacturer"));
  ASSERT_EQ("X100", description->get<string>("model"));
  ASSERT_EQ("A test device", description->getValue<string>());
}

TEST_F(JsonDeviceParserTest, should_fail_when_required_properties_missing)
{
  // Missing uuid and name which are required for Device
  auto doc = R"(
{
  "Device": {
    "id": "d1"
  }
}
)";

  parser::JsonParser parser(1);
  auto device = parser.parseDevice(doc);
  ASSERT_FALSE(device);
}

TEST_F(JsonDeviceParserTest, should_parse_v2_device_with_multiple_component_types)
{
  auto doc = R"(
{
  "Device": {
    "id": "d1", "name": "MyDevice", "uuid": "xxx-yyy-zzz",
    "Components": {
      "Axes": [{"id": "a1",
        "Components": {
          "Linear": [
            {"id": "x1", "name": "X"},
            {"id": "y1", "name": "Y"}
          ],
          "Rotary": [
            {"id": "c1", "name": "C"}
          ]
        }
      }],
      "Controller": [{"id": "ct1"}]
    }
  }
}
)";

  parser::JsonParser parser(2);
  auto device = parser.parseDevice(doc);
  ASSERT_TRUE(device);

  auto components = device->getList("Components");
  ASSERT_TRUE(components);
  ASSERT_EQ(2, components->size());

  auto it = components->begin();
  auto axes = dynamic_pointer_cast<Component>(*it);
  ASSERT_TRUE(axes);
  ASSERT_EQ("Axes", string(axes->getName()));

  auto axesChildren = axes->getList("Components");
  ASSERT_TRUE(axesChildren);
  ASSERT_EQ(3, axesChildren->size());

  auto cit = axesChildren->begin();
  ASSERT_EQ("Linear", string((*cit)->getName()));
  ASSERT_EQ("x1", (*cit)->get<string>("id"));

  cit++;
  ASSERT_EQ("Linear", string((*cit)->getName()));
  ASSERT_EQ("y1", (*cit)->get<string>("id"));

  cit++;
  ASSERT_EQ("Rotary", string((*cit)->getName()));
  ASSERT_EQ("c1", (*cit)->get<string>("id"));

  it++;
  auto controller = dynamic_pointer_cast<Component>(*it);
  ASSERT_TRUE(controller);
  ASSERT_EQ("Controller", string(controller->getName()));
  ASSERT_EQ("ct1", controller->get<string>("id"));
}

TEST_F(JsonDeviceParserTest, should_parse_v1_and_v2_produce_same_device_structure)
{
  auto v1Doc = R"(
{
  "Device": {
    "id": "d1", "name": "Machine", "uuid": "aaa-bbb",
    "Components": [
      {"Axes": {"id": "a1",
        "Components": [
          {"Linear": {"id": "x1", "name": "X"}},
          {"Linear": {"id": "y1", "name": "Y"}}
        ]
      }}
    ]
  }
}
)";

  auto v2Doc = R"(
{
  "Device": {
    "id": "d1", "name": "Machine", "uuid": "aaa-bbb",
    "Components": {
      "Axes": [{"id": "a1",
        "Components": {
          "Linear": [
            {"id": "x1", "name": "X"},
            {"id": "y1", "name": "Y"}
          ]
        }
      }]
    }
  }
}
)";

  parser::JsonParser parserV1(1);
  auto deviceV1 = parserV1.parseDevice(v1Doc);
  ASSERT_TRUE(deviceV1);

  parser::JsonParser parserV2(2);
  auto deviceV2 = parserV2.parseDevice(v2Doc);
  ASSERT_TRUE(deviceV2);

  // Both should produce the same device structure
  ASSERT_EQ(deviceV1->get<string>("id"), deviceV2->get<string>("id"));
  ASSERT_EQ(deviceV1->get<string>("name"), deviceV2->get<string>("name"));
  ASSERT_EQ(deviceV1->get<string>("uuid"), deviceV2->get<string>("uuid"));

  auto compsV1 = deviceV1->getList("Components");
  auto compsV2 = deviceV2->getList("Components");
  ASSERT_TRUE(compsV1);
  ASSERT_TRUE(compsV2);
  ASSERT_EQ(compsV1->size(), compsV2->size());

  auto axesV1 = compsV1->front();
  auto axesV2 = compsV2->front();
  ASSERT_EQ(string(axesV1->getName()), string(axesV2->getName()));

  auto childrenV1 = axesV1->getList("Components");
  auto childrenV2 = axesV2->getList("Components");
  ASSERT_TRUE(childrenV1);
  ASSERT_TRUE(childrenV2);
  ASSERT_EQ(childrenV1->size(), childrenV2->size());

  auto itV1 = childrenV1->begin();
  auto itV2 = childrenV2->begin();
  for (; itV1 != childrenV1->end(); ++itV1, ++itV2)
  {
    ASSERT_EQ(string((*itV1)->getName()), string((*itV2)->getName()));
    ASSERT_EQ((*itV1)->get<string>("id"), (*itV2)->get<string>("id"));
    ASSERT_EQ((*itV1)->get<string>("name"), (*itV2)->get<string>("name"));
  }
}
