//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "agent.hpp"
#include "entity/entity.hpp"
#include "pipeline/mtconnect_xml_transform.hpp"
#include "xml_printer.hpp"

using namespace mtconnect;
using namespace mtconnect::pipeline;
using namespace mtconnect::observation;
using namespace mtconnect::asset;
using namespace std;
using namespace date::literals;
using namespace std::literals;

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
  void deliverAssetCommand(entity::EntityPtr) override {}
  void deliverCommand(entity::EntityPtr) override {}
  void deliverConnectStatus(entity::EntityPtr, const StringList &, bool) override {}
  void sourceFailed(const std::string &id) override {}

  DevicePtr m_device;
};

class MTConnectXmlTransformTest : public testing::Test
{
protected:
  void SetUp() override
  {
    auto printer = make_unique<XmlPrinter>();
    auto parser = make_unique<XmlParser>();

    m_device =
        parser->parseFile(PROJECT_ROOT_DIR "/samples/test_config.xml", printer.get()).front();

    m_context = make_shared<PipelineContext>();
    m_context->m_contract = make_unique<MockPipelineContract>(m_device);
    m_xform = make_shared<MTConnectXmlTransform>(m_context);
    m_xform->bind(make_shared<NullTransform>(TypeGuard<Entity>(RUN)));
  }

  void TearDown() override { m_xform.reset(); }

  DevicePtr m_device;
  shared_ptr<MTConnectXmlTransform> m_xform;
  shared_ptr<PipelineContext> m_context;
};

TEST_F(MTConnectXmlTransformTest, should_add_next_to_the_context) { GTEST_SKIP(); }

TEST_F(MTConnectXmlTransformTest, should_create_list_of_assets) { GTEST_SKIP(); }
