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
#include <boost/algorithm/string.hpp>

#include <iostream>
#include <string>
#include <date/date.h>
#include <filesystem>

#include "adapter/shdr/shdr_adapter.hpp"
#include "agent.hpp"
#include "configuration/agent_config.hpp"
#include "configuration/config_options.hpp"
#include "rest_sink/rest_service.hpp"
#include "xml_printer.hpp"
#include "device_model/data_item/data_item.hpp"

#include <mruby.h>
#include <mruby/data.h>
#include <mruby/array.h>
#include <mruby/compile.h>
#include <mruby/dump.h>
#include <mruby/variable.h>
#include <mruby/proc.h>
#include <mruby/class.h>
#include <mruby/numeric.h>
#include <mruby/string.h>
#include <mruby/presym.h>
#include <mruby/error.h>

#include "ruby/ruby_vm.hpp"
#include "ruby/ruby_smart_ptr.hpp"

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#define chdir _chdir
#endif

namespace {
  using namespace std;
  using namespace mtconnect;
  using namespace configuration;
  using namespace observation;
  using namespace device_model;
  using namespace entity;
  using namespace data_item;
  using namespace ruby;
  namespace fs = std::filesystem;

  class EmbeddedRubyTest : public testing::Test
  {
  protected:
    void SetUp() override
    {
      m_config = std::make_unique<AgentConfiguration>();
      m_config->setDebug(true);
      m_cwd = std::filesystem::current_path();
    }
    
    void load(const char *file)
    {
      string str("Devices = " PROJECT_ROOT_DIR "/samples/test_config.xml\n"
                 "Ruby {\n"
                 "  module = " PROJECT_ROOT_DIR "/test/resources/ruby/" +
                 string(file) + "\n"
                 "}\n");
      m_config->loadConfig(str);

    }

    void TearDown() override
    {
      m_config.reset();
      chdir(m_cwd.string().c_str());
    }

    std::unique_ptr<AgentConfiguration> m_config;
    std::filesystem::path m_cwd;
  };
  
  TEST_F(EmbeddedRubyTest, should_initialize)
  {
    load("should_initialize.rb");
    
    auto mrb = RubyVM::rubyVM()->state();
    ASSERT_NE(nullptr, mrb);
    
    mrb_value pipelines = mrb_gv_get(mrb, mrb_intern_lit(mrb, "$pipelines"));
    ASSERT_FALSE(mrb_nil_p(pipelines));
    ASSERT_TRUE(mrb_array_p(pipelines));

    auto array = mrb_ary_ptr(pipelines);
    auto size = ARY_LEN(array);
    ASSERT_EQ(2, size);
    
    auto values = ARY_PTR(array);
    ASSERT_NE(nullptr, values);
    for (int i = 0; i < size; i++)
    {
      auto pipeline = MRubyPtr<pipeline::Pipeline>::unwrap(mrb, values[i]);
      ASSERT_NE(nullptr, dynamic_cast<pipeline::Pipeline*>(pipeline));
    }
  }
  
  TEST_F(EmbeddedRubyTest, should_support_entities)
  {
    using namespace std::chrono;
    using namespace std::chrono_literals;
    
    load("should_support_entities.rb");
    
    auto mrb = RubyVM::rubyVM()->state();
    ASSERT_NE(nullptr, mrb);

    mrb_value ent1 = mrb_gv_get(mrb, mrb_intern_lit(mrb, "$ent1"));
    ASSERT_FALSE(mrb_nil_p(ent1));
    
    auto cent1 = MRubySharedPtr<Entity>::unwrap(mrb, ent1);
    ASSERT_TRUE(cent1);
    
    ASSERT_EQ("TestEntity", cent1->getName());
    ASSERT_EQ("Simple Value", cent1->getValue<string>());
    
    mrb_value ent2 = mrb_gv_get(mrb, mrb_intern_lit(mrb, "$ent2"));
    ASSERT_FALSE(mrb_nil_p(ent2));
    
    auto cent2 = MRubySharedPtr<Entity>::unwrap(mrb, ent2);
    ASSERT_TRUE(cent2);
    
    ASSERT_EQ("HashEntity", cent2->getName());
    ASSERT_EQ("Simple Value", cent2->getValue<string>());
    ASSERT_EQ(10, cent2->get<int64_t>("int"));
    ASSERT_NEAR(123.4, cent2->get<double>("float"), 0.000001);
    
    Timestamp ts = cent2->get<Timestamp>("time");
    ASSERT_EQ(1577836800s, ts.time_since_epoch());
  }
  
#if 0
  template<typename T>
  T EntityValue(VALUE value, const string name)
  {
    Rice::Data_Object<Entity> ent(value);
    EntityPtr ptr(ent.get()->getptr());
    EXPECT_TRUE(ptr);
    EXPECT_EQ(name, ptr->getName());
    
    T v = ptr->getValue<T>();
    return v;
  }
  
  TEST_F(EmbeddedRubyTest, should_convert_values_from_ruby)
  {
    string str("Devices = " PROJECT_ROOT_DIR "/samples/test_config.xml\n"
               "Ruby {\n"
               "  module = " PROJECT_ROOT_DIR "/test/resources/ruby/should_convert_values.rb\n"
               "}\n");
    m_config->loadConfig(str);
    
    VALUE e1 = Rice::protect(rb_eval_string, "ConvertValuesFromRuby.create_entity_1");
    ASSERT_EQ(10, EntityValue<int64_t>(e1, "TestEntity1"));
    
    VALUE e2 = Rice::protect(rb_eval_string, "ConvertValuesFromRuby.create_entity_2");
    ASSERT_EQ("Hello"s, EntityValue<string>(e2,  "TestEntity2"));

    VALUE e3 = Rice::protect(rb_eval_string, "ConvertValuesFromRuby.create_entity_3");
    ASSERT_EQ(123.5, EntityValue<double>(e3,  "TestEntity3"));
  }

#endif
}
