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

class MTConnectXmlTransformTest : public testing::Test
{
protected:
  void SetUp() override
  {
    auto printer = make_unique<printer::XmlPrinter>();
    auto parser = make_unique<parser::XmlParser>();

    m_device =
        parser->parseFile(TEST_RESOURCE_DIR "/samples/test_config.xml", printer.get()).front();

    m_context = make_shared<PipelineContext>();
    m_context->m_contract = make_unique<MockPipelineContract>(m_device);
    m_xform = make_shared<MTConnectXmlTransform>(m_context, m_feedback);
    m_xform->bind(make_shared<NullTransform>(TypeGuard<Entity>(RUN)));
  }

  void TearDown() override { m_xform.reset(); }

  pipeline::XmlTransformFeedback m_feedback;
  DevicePtr m_device;
  shared_ptr<MTConnectXmlTransform> m_xform;
  shared_ptr<PipelineContext> m_context;
};

TEST_F(MTConnectXmlTransformTest, should_add_next_to_the_context)
{
  string data {R"(<?xml version="1.0" encoding="UTF-8"?>
<MTConnectStreams xmlns:m="urn:mtconnect.org:MTConnectStreams:1.7" xmlns="urn:mtconnect.org:MTConnectStreams:1.7" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="urn:mtconnect.org:MTConnectStreams:1.7 /schemas/MTConnectStreams_1.7.xsd">
  <Header creationTime="2022-04-21T05:54:56Z" sender="IntelAgent" instanceId="1649989201" version="2.0.0.1" deviceModelChangeTime="2022-04-21T03:21:32.630619Z" bufferSize="131072" nextSequence="4992049" firstSequence="4860977" lastSequence="4992048"/>
    <Streams/>
</MTConnectStreams>
)"};

  auto entity = make_shared<Entity>("Data", Properties {{"VALUE", data}, {"source", "adapter"s}});

  auto res1 = (*m_xform)(std::move(entity));

  ASSERT_EQ(1649989201, m_feedback.m_instanceId);
  ASSERT_EQ(4992049, m_feedback.m_next);
}

TEST_F(MTConnectXmlTransformTest, should_return_errors)
{
  string data {R"(<?xml version="1.0" encoding="UTF-8"?>
<MTConnectError xmlns:m="urn:mtconnect.org:MTConnectError:1.7" xmlns="urn:mtconnect.org:MTConnectError:1.7" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="urn:mtconnect.org:MTConnectError:1.7 /schemas/MTConnectError_1.7.xsd">
  <Header creationTime="2022-04-21T06:13:20Z" sender="IntelAgent" instanceId="1649989201" version="2.0.0.1" deviceModelChangeTime="2022-04-21T03:21:32.630619Z" bufferSize="131072"/>
  <Errors>
    <Error errorCode="OUT_OF_RANGE">'at' must be greater than 4871368</Error>
  </Errors>
</MTConnectError>)"};

  auto entity = make_shared<Entity>("Data", Properties {{"VALUE", data}, {"source", "adapter"s}});

  EXPECT_THROW((*m_xform)(std::move(entity)), std::system_error);

  ASSERT_EQ(1, m_feedback.m_errors.size());
  auto &error = m_feedback.m_errors.front();
  ASSERT_EQ("OUT_OF_RANGE", error.m_code);
  ASSERT_EQ("'at' must be greater than 4871368", error.m_message);
}

TEST_F(MTConnectXmlTransformTest, should_throw_when_instances_change)
{
  string data {R"(<?xml version="1.0" encoding="UTF-8"?>
<MTConnectStreams xmlns:m="urn:mtconnect.org:MTConnectStreams:1.7" xmlns="urn:mtconnect.org:MTConnectStreams:1.7" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="urn:mtconnect.org:MTConnectStreams:1.7 /schemas/MTConnectStreams_1.7.xsd">
  <Header creationTime="2022-04-21T05:54:56Z" sender="IntelAgent" instanceId="1649989201" version="2.0.0.1" deviceModelChangeTime="2022-04-21T03:21:32.630619Z" bufferSize="131072" nextSequence="4992049" firstSequence="4860977" lastSequence="4992048"/>
    <Streams/>
</MTConnectStreams>
)"};

  auto entity = make_shared<Entity>("Data", Properties {{"VALUE", data}, {"source", "adapter"s}});

  auto res1 = (*m_xform)(std::move(entity));

  ASSERT_EQ(1649989201, m_feedback.m_instanceId);
  ASSERT_EQ(4992049, m_feedback.m_next);

  string recover {R"(<?xml version="1.0" encoding="UTF-8"?>
<MTConnectStreams xmlns:m="urn:mtconnect.org:MTConnectStreams:1.7" xmlns="urn:mtconnect.org:MTConnectStreams:1.7" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="urn:mtconnect.org:MTConnectStreams:1.7 /schemas/MTConnectStreams_1.7.xsd">
  <Header creationTime="2022-04-21T05:54:56Z" sender="IntelAgent" instanceId="1649989202" version="2.0.0.1" deviceModelChangeTime="2022-04-21T03:21:32.630619Z" bufferSize="131072" nextSequence="12" firstSequence="1" lastSequence="11"/>
    <Streams/>
</MTConnectStreams>
)"};

  entity = make_shared<Entity>("Data", Properties {{"VALUE", recover}, {"source", "adapter"s}});

  EXPECT_THROW((*m_xform)(std::move(entity)), std::system_error);
  ASSERT_EQ(1649989201, m_feedback.m_instanceId);
  ASSERT_EQ(4992049, m_feedback.m_next);
}
