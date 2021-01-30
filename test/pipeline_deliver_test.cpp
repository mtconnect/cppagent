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

#include "agent_test_helper.hpp"
#include "pipeline/shdr_token_mapper.hpp"
#include "pipeline/duplicate_filter.hpp"
#include "pipeline/rate_filter.hpp"
#include "pipeline/deliver.hpp"
#include "pipeline/pipeline.hpp"
#include "observation/observation.hpp"
#include <chrono>

using namespace mtconnect;
using namespace mtconnect::pipeline;
using namespace mtconnect::observation;
using namespace std;
using namespace std::literals;
using namespace std::chrono_literals;

class PipelineTest : public testing::Test
{
 protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/solid_model.xml",
                                   8, 4, "1.7", 25);
    m_agentId = to_string(getCurrentTimeInSec());
    m_device = m_agentTestHelper->m_agent->getDeviceByName("LinuxCNC");
  }

  void TearDown() override
  {
    m_agentTestHelper.reset();
  }

  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
  std::string m_agentId;
  Device *m_device{nullptr};
};
