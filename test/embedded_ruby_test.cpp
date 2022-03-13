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
#include <filesystem>
#include <string>

#include "adapter/shdr/shdr_adapter.hpp"
#include "agent.hpp"
#include "configuration/agent_config.hpp"
#include "configuration/config_options.hpp"
#include "rest_sink/rest_service.hpp"
#include "xml_printer.hpp"

#include <rice/rice.hpp>
#include <rice/stl.hpp>
#include <ruby/thread.h>

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#define chdir _chdir
#endif

using namespace std;
using namespace mtconnect;
using namespace configuration;
using namespace observation;
using namespace device_model;
using namespace entity;
namespace fs = std::filesystem;

namespace {
  class EmbeddedRubyTest : public testing::Test
  {
  protected:
    void SetUp() override
    {
      m_config = std::make_unique<AgentConfiguration>();
      m_config->setDebug(true);
      m_cwd = std::filesystem::current_path();
    }

    void TearDown() override
    {
      m_config.reset();
      chdir(m_cwd.string().c_str());
    }

    std::unique_ptr<AgentConfiguration> m_config;
    std::filesystem::path m_cwd;
  };
  
  TEST_F(EmbeddedRubyTest, create_transform)
  {
    string str("Devices = " PROJECT_ROOT_DIR "/samples/test_config.xml\n"
               "Ruby {\n"
               "  module = " PROJECT_ROOT_DIR "/test/resources/ruby/create_transform.rb\n"
               "}\n");
    m_config->loadConfig(str);
    
    const Agent *agent = m_config->getAgent();
    DataItemPtr exec = agent->getDataItemForDevice("LinuxCNC", "execution");
    ASSERT_TRUE(exec);
    
    ErrorList errors;
    Timestamp now { std::chrono::system_clock::now() };
    ObservationPtr obser = Observation::make(exec, {{"VALUE", "1"}}, now, errors);
    
    ASSERT_TRUE(obser);
    ASSERT_EQ(0, errors.size());
    
    Rice::Object trans = Rice::protect(rb_eval_string, "FixExecution.new");
    Rice::Data_Object<Entity> out = trans.call("transform", obser);
    ASSERT_TRUE(out);

    ASSERT_EQ("READY", out->getValue<string>());
  }

}
