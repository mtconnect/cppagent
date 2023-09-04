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

#include <chrono>

#include "mtconnect/agent.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/pipeline/mtconnect_xml_transform.hpp"
#include "mtconnect/printer//xml_printer.hpp"

using namespace mtconnect;
using namespace mtconnect::pipeline;
using namespace mtconnect::observation;
using namespace mtconnect::asset;
using namespace mtconnect::printer;
using namespace mtconnect::parser;
using namespace std;
using namespace date::literals;
using namespace std::literals;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class MockPipelineContract : public PipelineContract
{
public:
  MockPipelineContract(DevicePtr device) : m_device(device) {}
  DevicePtr findDevice(const std::string &) override { return m_device; }
  DataItemPtr findDataItem(const std::string &device, const std::string &name) override
  {
    return m_device->getDeviceDataItem(name);
  }
  void eachDataItem(EachDataItem fun) override {}
  void deliverObservation(observation::ObservationPtr obs) override {}
  void deliverAsset(AssetPtr) override {}
  void deliverDevice(DevicePtr) override {}
  void deliverAssetCommand(entity::EntityPtr) override {}
  void deliverCommand(entity::EntityPtr) override {}
  void deliverConnectStatus(entity::EntityPtr, const StringList &, bool) override {}
  void sourceFailed(const std::string &id) override {}
  const ObservationPtr checkDuplicate(const ObservationPtr &obs) const override { return obs; }

  DevicePtr m_device;
};

class ResponseDocumentTest : public testing::Test
{
protected:
  void SetUp() override
  {
    auto printer = make_unique<XmlPrinter>();
    auto parser = make_unique<XmlParser>();

    m_doc.emplace();

    m_device = parser->parseFile(TEST_RESOURCE_DIR "/samples/data_set.xml", printer.get()).front();

    m_context = make_shared<PipelineContext>();
    m_context->m_contract = make_unique<MockPipelineContract>(m_device);
  }

  void TearDown() override { m_doc.reset(); }

  DevicePtr m_device;
  std::optional<ResponseDocument> m_doc;
  shared_ptr<PipelineContext> m_context;
};

TEST_F(ResponseDocumentTest, should_parse_observations)
{
  string data {R"(<?xml version="1.0" encoding="UTF-8"?>
<MTConnectStreams xmlns:m="urn:mtconnect.org:MTConnectStreams:1.8"
    xmlns="urn:mtconnect.org:MTConnectStreams:1.8"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xsi:schemaLocation="urn:mtconnect.org:MTConnectStreams:1.8 https://schemas.mtconnect.org/schemas/MTConnectStreams_1.8.xsd">
    <Header creationTime="2022-04-22T04:06:21Z" sender="IntelAgent" instanceId="1649989201" version="2.0.0.1" deviceModelChangeTime="2022-04-21T21:32:38.042794Z" bufferSize="131072" nextSequence="5741581" firstSequence="5610509" lastSequence="5741580"/>
    <Streams>
        <DeviceStream name="LinuxCNC" uuid="000">
            <ComponentStream componentId="d" component="Device">
                <Events>
                    <AssetChanged sequence="5741550" assetType="CuttingTool"
                        timestamp="2022-04-22T04:06:21Z" dataItemId="d_asset_chg">TOOLABC</AssetChanged>
                    <AssetRemoved sequence="5741551" assetType="CuttingTool"
                        timestamp="2022-04-22T04:06:21Z" dataItemId="d_asset_rem">TOOLDEF</AssetRemoved>
                </Events>
            </ComponentStream>
            <ComponentStream componentId="path1" component="Path">
                <Events>
                    <ControllerMode name="mode" sequence="5741552" timestamp="2022-04-22T04:06:21Z" dataItemId="px">AUTOMATIC</ControllerMode>
                </Events>
            </ComponentStream>
            <ComponentStream componentId="c" component="Rotary">
                <Samples>
                    <RotaryVelocity sequence="5741553" timestamp="2022-04-22T04:06:21Z" dataItemId="c1">1556.33</RotaryVelocity>
                </Samples>
            </ComponentStream>
        </DeviceStream>
    </Streams>
</MTConnectStreams>
)"};

  m_doc.emplace();
  ResponseDocument::parse(data, *m_doc, m_context);

  ASSERT_EQ(5741581, m_doc->m_next);
  ASSERT_EQ(1649989201, m_doc->m_instanceId);

  ASSERT_EQ(3, m_doc->m_entities.size());
  auto ent = m_doc->m_entities.begin();

  ASSERT_EQ("AssetCommand", (*ent)->getName());
  ASSERT_EQ("RemoveAsset", (*ent)->getValue<string>());
  ASSERT_EQ("TOOLDEF", (*ent)->get<string>("assetId"));

  ent++;
  ASSERT_EQ("ControllerMode", (*ent)->getName());
  ASSERT_EQ("AUTOMATIC", (*ent)->getValue<string>());
  ASSERT_EQ("p2", (*ent)->get<string>("dataItemId"));
  ASSERT_EQ("mode", (*ent)->get<string>("name"));

  ent++;
  ASSERT_EQ("RotaryVelocity", (*ent)->getName());
  ASSERT_EQ(1556.33, (*ent)->getValue<double>());
  ASSERT_EQ("c1", (*ent)->get<string>("dataItemId"));

  ASSERT_EQ(1, m_doc->m_assetEvents.size());
  auto aent = m_doc->m_assetEvents.begin();

  ASSERT_EQ("AssetChanged", (*aent)->getName());
  ASSERT_EQ("TOOLABC", (*aent)->getValue<string>());
  ASSERT_EQ("d_asset_chg", (*aent)->get<string>("dataItemId"));
}

TEST_F(ResponseDocumentTest, should_parse_data_sets)
{
  string data {R"(<?xml version="1.0" encoding="UTF-8"?>
<MTConnectStreams xmlns:m="urn:mtconnect.org:MTConnectStreams:1.8"
    xmlns="urn:mtconnect.org:MTConnectStreams:1.8"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xsi:schemaLocation="urn:mtconnect.org:MTConnectStreams:1.8 https://schemas.mtconnect.org/schemas/MTConnectStreams_1.8.xsd">
    <Header creationTime="2022-04-22T04:06:21Z" sender="IntelAgent" instanceId="1649989201" version="2.0.0.1" deviceModelChangeTime="2022-04-21T21:32:38.042794Z" bufferSize="131072" nextSequence="5741581" firstSequence="5610509" lastSequence="5741580"/>
    <Streams>
        <DeviceStream name="LinuxCNC" uuid="000">
            <ComponentStream componentId="path1" component="Path">
                <Events>
                    <VariableDataSet name="vars" sequence="5741552" timestamp="2022-04-22T04:06:21Z" dataItemId="v1" count="0">
                        UNAVAILABLE
                    </VariableDataSet>
                    <VariableDataSet name="vars" sequence="5741552" timestamp="2022-04-22T04:06:21Z" dataItemId="v1" count="4">
                        <Entry key="X100">66</Entry>
                        <Entry key="X101">ABC</Entry>
                        <Entry key="X102">44.6</Entry>
                        <Entry key="X103" removed="true"/>
                    </VariableDataSet>
                </Events>
            </ComponentStream>
        </DeviceStream>
    </Streams>
</MTConnectStreams>
)"};

  m_doc.emplace();
  ResponseDocument::parse(data, *m_doc, m_context);

  ASSERT_EQ(5741581, m_doc->m_next);
  ASSERT_EQ(1649989201, m_doc->m_instanceId);

  ASSERT_EQ(2, m_doc->m_entities.size());
  auto ent = m_doc->m_entities.begin();

  ASSERT_EQ("VariableDataSet", (*ent)->getName());
  ASSERT_EQ("v1", (*ent)->get<string>("dataItemId"));
  ASSERT_EQ("vars", (*ent)->get<string>("name"));

  ObservationPtr obs = dynamic_pointer_cast<Observation>(*ent);
  ASSERT_TRUE(obs->isUnavailable());

  ent++;
  ASSERT_EQ(4, (*ent)->get<int64_t>("count"));
  const auto &ds = (*ent)->getValue<DataSet>();
  ASSERT_EQ(4, ds.size());

  auto dse = ds.begin();
  ASSERT_EQ("X100", dse->m_key);
  ASSERT_FALSE(dse->m_removed);
  ASSERT_EQ(66, get<int64_t>(dse->m_value));

  dse++;
  ASSERT_EQ("X101", dse->m_key);
  ASSERT_FALSE(dse->m_removed);
  ASSERT_EQ("ABC", get<string>(dse->m_value));

  dse++;
  ASSERT_EQ("X102", dse->m_key);
  ASSERT_FALSE(dse->m_removed);
  ASSERT_EQ(44.6, get<double>(dse->m_value));

  dse++;
  ASSERT_EQ("X103", dse->m_key);
  ASSERT_TRUE(dse->m_removed);
  ASSERT_EQ(monostate(), get<monostate>(dse->m_value));
}

TEST_F(ResponseDocumentTest, should_parse_tables)
{
  string data {R"(<?xml version="1.0" encoding="UTF-8"?>
<MTConnectStreams xmlns:m="urn:mtconnect.org:MTConnectStreams:1.8"
    xmlns="urn:mtconnect.org:MTConnectStreams:1.8"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xsi:schemaLocation="urn:mtconnect.org:MTConnectStreams:1.8 https://schemas.mtconnect.org/schemas/MTConnectStreams_1.8.xsd">
    <Header creationTime="2022-04-22T04:06:21Z" sender="IntelAgent" instanceId="1649989201" version="2.0.0.1" deviceModelChangeTime="2022-04-21T21:32:38.042794Z" bufferSize="131072" nextSequence="5741581" firstSequence="5610509" lastSequence="5741580"/>
    <Streams>
        <DeviceStream name="LinuxCNC" uuid="000">
            <ComponentStream componentId="path1" component="Path">
                <Events>
                    <WorkOffsetTable name="wpo" sequence="5741552" timestamp="2022-04-22T04:06:21Z" dataItemId="wp1" count="0">
                        UNAVAILABLE
                    </WorkOffsetTable>
                    <WorkOffsetTable name="wpo" sequence="5741552" timestamp="2022-04-22T04:06:21Z" dataItemId="wp1" count="3">
                        <Entry key="W1">
                            <Cell key="X">1.0</Cell>
                            <Cell key="Y">2.0</Cell>
                            <Cell key="Z">3.0</Cell>
                        </Entry>
                        <Entry key="W2">
                            <Cell key="X2">4.0</Cell>
                            <Cell key="Y2">5.0</Cell>
                            <Cell key="Z2">6.0</Cell>
                        </Entry>
                        <Entry key="W3">
                            <Cell key="A">A</Cell>
                            <Cell key="B">B</Cell>
                            <Cell key="C">C</Cell>
                        </Entry>
                        <Entry key="W4" removed="true"/>
                    </WorkOffsetTable>
                </Events>
            </ComponentStream>
        </DeviceStream>
    </Streams>
</MTConnectStreams>
)"};

  m_doc.emplace();
  ResponseDocument::parse(data, *m_doc, m_context);

  ASSERT_EQ(5741581, m_doc->m_next);
  ASSERT_EQ(1649989201, m_doc->m_instanceId);

  ASSERT_EQ(2, m_doc->m_entities.size());
  auto ent = m_doc->m_entities.begin();

  ASSERT_EQ("WorkOffsetTable", (*ent)->getName());
  ASSERT_EQ("wp1", (*ent)->get<string>("dataItemId"));
  ASSERT_EQ("wpo", (*ent)->get<string>("name"));

  ObservationPtr obs = dynamic_pointer_cast<Observation>(*ent);
  ASSERT_TRUE(obs->isUnavailable());

  ent++;
  const auto &ds = (*ent)->getValue<DataSet>();
  ASSERT_EQ(4, ds.size());

  auto dse = ds.begin();
  ASSERT_EQ("W1", dse->m_key);
  ASSERT_FALSE(dse->m_removed);

  const auto &v1 = get<DataSet>(dse->m_value);
  ASSERT_EQ(3, v1.size());

  auto v1i = v1.begin();
  ASSERT_EQ("X", v1i->m_key);
  ASSERT_EQ(1.0, get<double>(v1i->m_value));
  v1i++;
  ASSERT_EQ("Y", v1i->m_key);
  ASSERT_EQ(2.0, get<double>(v1i->m_value));
  v1i++;
  ASSERT_EQ("Z", v1i->m_key);
  ASSERT_EQ(3.0, get<double>(v1i->m_value));

  dse++;
  ASSERT_EQ("W2", dse->m_key);
  ASSERT_FALSE(dse->m_removed);

  const auto &v2 = get<DataSet>(dse->m_value);
  ASSERT_EQ(3, v2.size());

  auto v2i = v2.begin();
  ASSERT_EQ("X2", v2i->m_key);
  ASSERT_EQ(4.0, get<double>(v2i->m_value));
  v2i++;
  ASSERT_EQ("Y2", v2i->m_key);
  ASSERT_EQ(5.0, get<double>(v2i->m_value));
  v2i++;
  ASSERT_EQ("Z2", v2i->m_key);
  ASSERT_EQ(6.0, get<double>(v2i->m_value));

  dse++;
  ASSERT_EQ("W3", dse->m_key);
  ASSERT_FALSE(dse->m_removed);

  const auto &v3 = get<DataSet>(dse->m_value);
  ASSERT_EQ(3, v3.size());

  auto v3i = v3.begin();
  ASSERT_EQ("A", v3i->m_key);
  ASSERT_EQ("A", get<string>(v3i->m_value));
  v3i++;
  ASSERT_EQ("B", v3i->m_key);
  ASSERT_EQ("B", get<string>(v3i->m_value));
  v3i++;
  ASSERT_EQ("C", v3i->m_key);
  ASSERT_EQ("C", get<string>(v3i->m_value));

  dse++;
  ASSERT_EQ("W4", dse->m_key);
  ASSERT_TRUE(dse->m_removed);
  ASSERT_EQ(monostate(), get<monostate>(dse->m_value));
}

TEST_F(ResponseDocumentTest, should_parse_assets)
{
  CuttingToolArchetype::registerAsset();
  CuttingTool::registerAsset();

  ifstream str(TEST_RESOURCE_DIR "/ext_asset.xml");
  ASSERT_TRUE(str.is_open());
  str.seekg(0, std::ios_base::end);
  size_t size = str.tellg();
  str.seekg(0);
  char *buffer = new char[size + 1];
  memset(buffer, 0, size);
  str.read(buffer, size);
  buffer[size] = '\0';

  m_doc.emplace();
  ResponseDocument::parse(buffer, *m_doc, m_context);

  ASSERT_EQ(1, m_doc->m_entities.size());
  auto asset = m_doc->m_entities.front();

  ASSERT_EQ("CuttingTool", asset->getName());
}

TEST_F(ResponseDocumentTest, should_parse_errors)
{
  string data {R"(<?xml version="1.0" encoding="UTF-8"?>
<MTConnectError xmlns:m="urn:mtconnect.org:MTConnectError:1.7" xmlns="urn:mtconnect.org:MTConnectError:1.7" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="urn:mtconnect.org:MTConnectError:1.7 /schemas/MTConnectError_1.7.xsd">
  <Header creationTime="2022-04-21T06:13:20Z" sender="IntelAgent" instanceId="1649989201" version="2.0.0.1" deviceModelChangeTime="2022-04-21T03:21:32.630619Z" bufferSize="131072"/>
  <Errors>
    <Error errorCode="OUT_OF_RANGE">'at' must be greater than 4871368</Error>
    <Error errorCode="FAILURE">Something went wrong</Error>
  </Errors>
</MTConnectError>)"};

  m_doc.emplace();
  ResponseDocument::parse(data, *m_doc, m_context);

  auto err = m_doc->m_errors.begin();

  ASSERT_EQ(2, m_doc->m_errors.size());
  ASSERT_EQ("OUT_OF_RANGE", err->m_code);
  ASSERT_EQ("'at' must be greater than 4871368", err->m_message);

  err++;
  ASSERT_EQ("FAILURE", err->m_code);
  ASSERT_EQ("Something went wrong", err->m_message);
}

TEST_F(ResponseDocumentTest, should_parse_legacy_error)
{
  string data {R"(<?xml version="1.0" encoding="UTF-8"?>
<MTConnectError xmlns:m="urn:mtconnect.org:MTConnectError:1.7" xmlns="urn:mtconnect.org:MTConnectError:1.7" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="urn:mtconnect.org:MTConnectError:1.7 /schemas/MTConnectError_1.7.xsd">
  <Header creationTime="2022-04-21T06:13:20Z" sender="IntelAgent" instanceId="1649989201" version="2.0.0.1" deviceModelChangeTime="2022-04-21T03:21:32.630619Z" bufferSize="131072"/>
    <Error errorCode="OUT_OF_RANGE">'at' must be greater than 4871368</Error>
</MTConnectError>)"};

  m_doc.emplace();
  ResponseDocument::parse(data, *m_doc, m_context);

  ASSERT_EQ(1, m_doc->m_errors.size());
  auto &error = m_doc->m_errors.front();
  ASSERT_EQ("OUT_OF_RANGE", error.m_code);
  ASSERT_EQ("'at' must be greater than 4871368", error.m_message);
}
