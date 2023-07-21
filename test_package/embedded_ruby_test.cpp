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

#include <boost/algorithm/string.hpp>

#include <chrono>
#include <date/date.h>
#include <filesystem>
#include <iostream>
#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/compile.h>
#include <mruby/data.h>
#include <mruby/dump.h>
#include <mruby/error.h>
#include <mruby/numeric.h>
#include <mruby/presym.h>
#include <mruby/proc.h>
#include <mruby/string.h>
#include <mruby/variable.h>
#include <string>

#include "mtconnect/agent.hpp"
#include "mtconnect/configuration/agent_config.hpp"
#include "mtconnect/configuration/config_options.hpp"
#include "mtconnect/device_model/data_item/data_item.hpp"
#include "mtconnect/pipeline/shdr_tokenizer.hpp"
#include "mtconnect/printer//xml_printer.hpp"
#include "mtconnect/ruby/ruby_smart_ptr.hpp"
#include "mtconnect/ruby/ruby_vm.hpp"
#include "mtconnect/sink/rest_sink/rest_service.hpp"
#include "mtconnect/source/adapter/shdr/shdr_adapter.hpp"

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#define chdir _chdir
#endif

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

namespace {
  using namespace std;
  using namespace mtconnect;
  using namespace configuration;
  using namespace observation;
  using namespace device_model;
  using namespace entity;
  using namespace data_item;
  using namespace pipeline;
  using namespace asset;
  using namespace ruby;
  namespace fs = std::filesystem;

  class MockPipelineContract : public PipelineContract
  {
  public:
    MockPipelineContract(const Agent *agent) : m_agent(agent) {}
    DevicePtr findDevice(const std::string &device) override { return nullptr; }
    DataItemPtr findDataItem(const std::string &device, const std::string &name) override
    {
      return m_agent->getDataItemForDevice(device, name);
    }
    void eachDataItem(EachDataItem fun) override {}
    void deliverObservation(observation::ObservationPtr obs) override { m_observation = obs; }
    void deliverAsset(AssetPtr a) override { m_asset = a; }
    void deliverDevice(DevicePtr) override {}
    void deliverAssetCommand(entity::EntityPtr c) override { m_command = c; }
    void deliverCommand(entity::EntityPtr c) override { m_command = c; }
    void deliverConnectStatus(entity::EntityPtr, const StringList &, bool) override {}
    void sourceFailed(const std::string &id) override {}
    const ObservationPtr checkDuplicate(const ObservationPtr &obs) const override { return obs; }

    const Agent *m_agent;
    ObservationPtr m_observation;
    entity::EntityPtr m_command;
    AssetPtr m_asset;
  };

  class EmbeddedRubyTest : public testing::Test
  {
  protected:
    void SetUp() override
    {
      m_config = std::make_unique<AgentConfiguration>();
      m_config->setDebug(true);
      m_cwd = std::filesystem::current_path();

      m_context = make_shared<PipelineContext>();
      m_context->m_contract = make_unique<MockPipelineContract>(m_config->getAgent());
    }

    void load(const char *file)
    {
      string str("Devices = " TEST_RESOURCE_DIR
                 "/samples/test_config.xml\n"
                 "Ruby {\n"
                 "  module = " TEST_RESOURCE_DIR "/ruby/" +
                 string(file) +
                 "\n"
                 "}\n");
      m_config->loadConfig(str);
    }

    void TearDown() override
    {
      m_config.reset();
      chdir(m_cwd.string().c_str());
    }

    shared_ptr<PipelineContext> m_context;
    std::unique_ptr<AgentConfiguration> m_config;
    std::filesystem::path m_cwd;
  };

  TEST_F(EmbeddedRubyTest, should_initialize)
  {
    load("should_initialize.rb");

    auto mrb = RubyVM::rubyVM().state();
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
      ASSERT_NE(nullptr, dynamic_cast<pipeline::Pipeline *>(pipeline));
    }
  }

  TEST_F(EmbeddedRubyTest, should_support_entities)
  {
    using namespace std::chrono;
    using namespace std::chrono_literals;

    load("should_support_entities.rb");

    auto mrb = RubyVM::rubyVM().state();
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

  TEST_F(EmbeddedRubyTest, entity_should_support_data_sets)
  {
    using namespace std::chrono;
    using namespace std::chrono_literals;

    load("entity_should_support_data_sets.rb");

    auto mrb = RubyVM::rubyVM().state();
    ASSERT_NE(nullptr, mrb);

    mrb_value ent1 = mrb_gv_get(mrb, mrb_intern_lit(mrb, "$ent1"));
    ASSERT_FALSE(mrb_nil_p(ent1));

    auto cent1 = MRubySharedPtr<Entity>::unwrap(mrb, ent1);
    ASSERT_TRUE(cent1);

    const DataSet &ds = cent1->getValue<DataSet>();
    ASSERT_EQ(3, ds.size());

    ASSERT_EQ("value1", ds.get<string>("string"));
    ASSERT_EQ(100, ds.get<int64_t>("int"));
    ASSERT_NEAR(123.4, ds.get<double>("float"), 0.000001);
  }

  TEST_F(EmbeddedRubyTest, entity_should_support_tables)
  {
    using namespace std::chrono;
    using namespace std::chrono_literals;

    load("entity_should_support_tables.rb");

    auto mrb = RubyVM::rubyVM().state();
    ASSERT_NE(nullptr, mrb);

    mrb_value ent1 = mrb_gv_get(mrb, mrb_intern_lit(mrb, "$ent1"));
    ASSERT_FALSE(mrb_nil_p(ent1));

    auto cent1 = MRubySharedPtr<Entity>::unwrap(mrb, ent1);
    ASSERT_TRUE(cent1);

    const DataSet &ds = cent1->getValue<DataSet>();
    ASSERT_EQ(2, ds.size());

    const DataSet &row1 = ds.get<DataSet>("row1");
    ASSERT_EQ(2, row1.size());

    ASSERT_EQ("text1", row1.get<string>("string"));
    ASSERT_NEAR(1.0, row1.get<double>("float"), 0.000001);

    const DataSet &row2 = ds.get<DataSet>("row2");
    ASSERT_EQ(2, row2.size());

    ASSERT_EQ("text2", row2.get<string>("string"));
    ASSERT_NEAR(2.0, row2.get<double>("float"), 0.000001);
  }

  TEST_F(EmbeddedRubyTest, should_transform)
  {
    using namespace std::chrono;
    using namespace std::chrono_literals;

    load("should_transform.rb");

    auto mrb = RubyVM::rubyVM().state();
    ASSERT_NE(nullptr, mrb);

    ConfigOptions options;
    boost::asio::io_context::strand strand(m_config->getContext());
    auto loopback =
        std::make_shared<source::LoopbackSource>("RubySource", strand, m_context, options);

    mrb_value source = MRubySharedPtr<mtconnect::source::Source>::wrap(mrb, "Source", loopback);
    mrb_gv_set(mrb, mrb_intern_lit(mrb, "$source"), source);

    mrb_value ent1 = mrb_gv_get(mrb, mrb_intern_lit(mrb, "$trans"));
    ASSERT_FALSE(mrb_nil_p(ent1));

    mrb_load_string(mrb, R"(
p $source
$source.pipeline.splice_after('Start', $trans)
)");

    auto di = m_config->getAgent()->getDataItemForDevice("LinuxCNC", "execution");
    [[maybe_unused]] auto out = loopback->receive(di, "1"s);

    auto contract = static_cast<MockPipelineContract *>(m_context->m_contract.get());
    ASSERT_TRUE(contract->m_observation);
    ASSERT_EQ("READY", contract->m_observation->getValue<string>());
  }

  TEST_F(EmbeddedRubyTest, should_transform_with_subclass)
  {
    using namespace std::chrono;
    using namespace std::chrono_literals;

    load("should_transform_with_subclass.rb");

    auto mrb = RubyVM::rubyVM().state();
    ASSERT_NE(nullptr, mrb);

    ConfigOptions options;
    boost::asio::io_context::strand strand(m_config->getContext());
    auto loopback =
        std::make_shared<source::LoopbackSource>("RubySource", strand, m_context, options);

    mrb_value source = MRubySharedPtr<mtconnect::source::Source>::wrap(mrb, "Source", loopback);
    mrb_gv_set(mrb, mrb_intern_lit(mrb, "$source"), source);

    mrb_load_string(mrb, R"(
p $source
$source.pipeline.splice_after('Start', FixExecution.new('FixExec', :Event))
)");

    auto di = m_config->getAgent()->getDataItemForDevice("LinuxCNC", "execution");
    [[maybe_unused]] auto out = loopback->receive(di, "1"s);

    auto contract = static_cast<MockPipelineContract *>(m_context->m_contract.get());
    ASSERT_TRUE(contract->m_observation);
    ASSERT_EQ("READY", contract->m_observation->getValue<string>());
  }

  TEST_F(EmbeddedRubyTest, should_create_sample)
  {
    using namespace std::chrono;
    using namespace std::chrono_literals;

    load("should_create_sample.rb");

    auto mrb = RubyVM::rubyVM().state();
    ASSERT_NE(nullptr, mrb);

    ConfigOptions options;
    boost::asio::io_context::strand strand(m_config->getContext());
    auto loopback =
        std::make_shared<source::LoopbackSource>("RubySource", strand, m_context, options);

    mrb_value source = MRubySharedPtr<mtconnect::source::Source>::wrap(mrb, "Source", loopback);
    mrb_gv_set(mrb, mrb_intern_lit(mrb, "$source"), source);

    mrb_load_string(mrb, R"(
$source.pipeline.splice_after('Start', $trans)
)");

    auto tokens = make_shared<pipeline::Tokens>();
    tokens->m_tokens = {"Xact"s, "100.0"s};

    loopback->getPipeline()->run(tokens);

    auto contract = static_cast<MockPipelineContract *>(m_context->m_contract.get());
    ASSERT_TRUE(contract->m_observation);
    ASSERT_EQ(100.0, contract->m_observation->getValue<double>());
    ASSERT_EQ("Xact", contract->m_observation->getDataItem()->getName());
  }

  TEST_F(EmbeddedRubyTest, should_create_event)
  {
    using namespace std::chrono;
    using namespace std::chrono_literals;

    load("should_create_event.rb");

    auto mrb = RubyVM::rubyVM().state();
    ASSERT_NE(nullptr, mrb);

    ConfigOptions options;
    boost::asio::io_context::strand strand(m_config->getContext());
    auto loopback =
        std::make_shared<source::LoopbackSource>("RubySource", strand, m_context, options);

    mrb_value source = MRubySharedPtr<mtconnect::source::Source>::wrap(mrb, "Source", loopback);
    mrb_gv_set(mrb, mrb_intern_lit(mrb, "$source"), source);

    mrb_load_string(mrb, R"(
$source.pipeline.splice_after('Start', $trans)
)");

    Properties props {{"VALUE", R"(
{
  "name": "block",
  "value": "G0X100Y100"
}
)"s}};
    auto entity = make_shared<Entity>("Data", props);

    loopback->getPipeline()->run(std::move(entity));

    auto contract = static_cast<MockPipelineContract *>(m_context->m_contract.get());
    ASSERT_TRUE(contract->m_observation);
    ASSERT_EQ("G0X100Y100", contract->m_observation->getValue<string>());
    ASSERT_EQ("block", contract->m_observation->getDataItem()->getName());
  }

  TEST_F(EmbeddedRubyTest, should_create_condition)
  {
    using namespace std::chrono;
    using namespace std::chrono_literals;

    load("should_create_condition.rb");

    auto mrb = RubyVM::rubyVM().state();
    ASSERT_NE(nullptr, mrb);

    ConfigOptions options;
    boost::asio::io_context::strand strand(m_config->getContext());
    auto loopback =
        std::make_shared<source::LoopbackSource>("RubySource", strand, m_context, options);

    mrb_value source = MRubySharedPtr<mtconnect::source::Source>::wrap(mrb, "Source", loopback);
    mrb_gv_set(mrb, mrb_intern_lit(mrb, "$source"), source);

    mrb_load_string(mrb, R"(
$source.pipeline.splice_after('Start', $trans)
)");

    Properties props {{"VALUE", "PLC1002:MACHINE ON FIRE"s}};
    auto entity = make_shared<Entity>("Data", props);

    loopback->getPipeline()->run(std::move(entity));

    auto contract = static_cast<MockPipelineContract *>(m_context->m_contract.get());

    ASSERT_TRUE(contract->m_observation);
    auto cond = dynamic_pointer_cast<Condition>(contract->m_observation);
    ASSERT_TRUE(cond);
    ASSERT_EQ("lp", cond->getDataItem()->getId());
    ASSERT_EQ("MACHINE ON FIRE", cond->getValue<string>());
    ASSERT_EQ("PLC1002", cond->getCode());
    ASSERT_EQ(Condition::FAULT, cond->getLevel());

    Properties props2 {{"VALUE", "NC155:SORRY, I DON'T WANT TO"s}};
    entity = make_shared<Entity>("Data", props2);

    loopback->getPipeline()->run(std::move(entity));

    ASSERT_TRUE(contract->m_observation);
    cond = dynamic_pointer_cast<Condition>(contract->m_observation);
    ASSERT_TRUE(cond);
    ASSERT_EQ("cmp", cond->getDataItem()->getId());
    ASSERT_EQ("SORRY, I DON'T WANT TO", cond->getValue<string>());
    ASSERT_EQ("NC155", cond->getCode());
    ASSERT_EQ(Condition::FAULT, cond->getLevel());
  }

  TEST_F(EmbeddedRubyTest, should_change_data_item_topic)
  {
    load("should_rename_data_item_topic.rb");

    auto mrb = RubyVM::rubyVM().state();
    ASSERT_NE(nullptr, mrb);

    auto agent = m_config->getAgent();
    auto device = agent->getDefaultDevice();
    ASSERT_TRUE(device);

    auto di = agent->getDataItemForDevice("000", "a");
    ASSERT_TRUE(di);
    ASSERT_EQ("000/States/Alarm[alarm]", di->getTopic());

    di = agent->getDataItemForDevice("000", "block");
    ASSERT_TRUE(di);
    ASSERT_EQ("000/Controller[Controller]/Path/States/Block[block]", di->getTopic());

    di = agent->getDataItemForDevice("000", "mode");
    ASSERT_TRUE(di);
    ASSERT_EQ("000/Controller:Controller/Path/Events/ControllerMode:mode", di->getTopic());
  }
}  // namespace
