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

// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "agent_device.hpp"
#include "adapter.hpp"
#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "json_helper.hpp"

using namespace std;
using namespace mtconnect;

class AgentDeviceTest : public testing::Test
{
 protected:

  void SetUp() override
  {
    m_agent = make_unique<Agent>(PROJECT_ROOT_DIR "/samples/test_config.xml", 8, 4, "1.3", 25);
    m_agentId = intToString(getCurrentTimeInSec());
    m_adapter = nullptr;

    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->m_agent = m_agent.get();
    m_agentTestHelper->m_queries.clear();
  }

  void TearDown() override
  {
    m_agent.reset();
    m_adapter = nullptr;
    m_agentTestHelper.reset();
  }

  void addAdapter()
  {
    ASSERT_FALSE(m_adapter);
    m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
    ASSERT_TRUE(m_adapter);
  }

 public:
  std::unique_ptr<Agent> m_agent;
  Adapter *m_adapter{nullptr};
  std::string m_agentId;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

