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

#include "agent_test_helper.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/pipeline/deliver.hpp"
#include "mtconnect/pipeline/delta_filter.hpp"
#include "mtconnect/pipeline/duplicate_filter.hpp"
#include "mtconnect/pipeline/pipeline.hpp"
#include "mtconnect/pipeline/shdr_token_mapper.hpp"
#include "mtconnect/source/adapter/adapter.hpp"

using namespace mtconnect;
using namespace mtconnect::source::adapter;
using namespace mtconnect::pipeline;
using namespace mtconnect::observation;
using namespace std;
using namespace std::literals;
using namespace std::chrono_literals;
using namespace mtconnect::sink::rest_sink;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class PipelineDeliverTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/SimpleDevlce.xml", 8, 4, "1.7", 25);
    m_agentId = to_string(getCurrentTimeInSec());
    m_device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");
  }

  void TearDown() override { m_agentTestHelper.reset(); }

  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
  std::string m_agentId;
  DevicePtr m_device {nullptr};
};

TEST_F(PipelineDeliverTest, test_simple_flow)
{
  m_agentTestHelper->addAdapter();
  auto &circ = m_agentTestHelper->getAgent()->getCircularBuffer();
  auto seq = circ.getSequence();
  m_agentTestHelper->m_adapter->processData("2021-01-22T12:33:45.123Z|Xpos|100.0");
  ASSERT_EQ(seq + 1, circ.getSequence());
  auto obs = circ.getFromBuffer(seq);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Xpos", obs->getDataItem()->getName());
  ASSERT_EQ(100.0, obs->getValue<double>());
  ASSERT_EQ("2021-01-22T12:33:45.123Z", format(obs->getTimestamp()));
}

TEST_F(PipelineDeliverTest, filter_duplicates)
{
  ConfigOptions options {{configuration::FilterDuplicates, true}};
  m_agentTestHelper->addAdapter(options);
  auto &circ = m_agentTestHelper->getAgent()->getCircularBuffer();
  auto seq = circ.getSequence();
  m_agentTestHelper->m_adapter->processData("2021-01-22T12:33:45.123Z|Xpos|100.0");
  ASSERT_EQ(seq + 1, circ.getSequence());

  auto obs = circ.getFromBuffer(seq);
  ASSERT_TRUE(obs);
  ASSERT_EQ("Xpos", obs->getDataItem()->getName());
  ASSERT_EQ(100.0, obs->getValue<double>());

  m_agentTestHelper->m_adapter->processData("2021-01-22T12:33:45.123Z|Xpos|100.0");
  ASSERT_EQ(seq + 1, circ.getSequence());

  m_agentTestHelper->m_adapter->processData("2021-01-22T12:33:45.123Z|Xpos|101.0");
  ASSERT_EQ(seq + 2, circ.getSequence());
  auto obs2 = circ.getFromBuffer(seq + 1);
  ASSERT_EQ(101.0, obs2->getValue<double>());
}

// a01c7f30
TEST_F(PipelineDeliverTest, filter_upcase)
{
  ConfigOptions options {{configuration::UpcaseDataItemValue, true}};
  m_agentTestHelper->addAdapter(options);
  auto &circ = m_agentTestHelper->getAgent()->getCircularBuffer();
  auto seq = circ.getSequence();
  m_agentTestHelper->m_adapter->processData("2021-01-22T12:33:45.123Z|a01c7f30|active");
  ASSERT_EQ(seq + 1, circ.getSequence());

  auto obs = circ.getFromBuffer(seq);
  ASSERT_TRUE(obs);
  ASSERT_EQ("a01c7f30", obs->getDataItem()->getId());
  ASSERT_EQ("ACTIVE", obs->getValue<string>());

  m_agentTestHelper->m_adapter->processData("2021-01-22T12:33:45.123Z|Xpos|101.0");
  ASSERT_EQ(seq + 2, circ.getSequence());
  auto obs2 = circ.getFromBuffer(seq + 1);
  ASSERT_EQ(101.0, obs2->getValue<double>());
}
