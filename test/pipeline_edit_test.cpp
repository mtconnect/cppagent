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
#include "observation/observation.hpp"
#include "pipeline/deliver.hpp"
#include "pipeline/delta_filter.hpp"
#include "pipeline/duplicate_filter.hpp"
#include "pipeline/pipeline.hpp"
#include "pipeline/shdr_token_mapper.hpp"
#include "source/adapter/adapter.hpp"

using namespace mtconnect;
using namespace mtconnect::source::adapter;
using namespace mtconnect::pipeline;
using namespace mtconnect::observation;
using namespace mtconnect::entity;
using namespace std;
using namespace std::literals;
using namespace std::chrono_literals;
using namespace mtconnect::sink::rest_sink;

using TransformFun = std::function<const EntityPtr(const EntityPtr entity)>;
class TestTransform : public Transform
{
public:
  TestTransform(const std::string &name, TransformFun fun, Guard guard)
    : Transform(name), m_function(fun)
  {
    m_guard = guard;
  }
  TestTransform(const std::string &name, Guard guard) : Transform(name) { m_guard = guard; }
  TestTransform(const std::string &name) : Transform(name) {}

  const EntityPtr operator()(const EntityPtr ptr) override { return m_function(ptr); }

  void setGuard(Guard &guard) { m_guard = guard; }
  TransformFun m_function;
};
using TestTransformPtr = shared_ptr<TestTransform>;

class TestPipeline : public Pipeline
{
public:
  using Pipeline::Pipeline;

  void build(const ConfigOptions &options) override {}

  TransformPtr getStart() { return m_start; }
};

class PipelineEditTest : public testing::Test
{
protected:
  void SetUp() override
  {
    boost::asio::io_context::strand strand(m_ioContext);
    m_pipeline = make_unique<TestPipeline>(m_context, strand);

    TestTransformPtr ta = make_shared<TestTransform>("A"s, EntityNameGuard("X", RUN));
    ta->m_function = [ta](const EntityPtr entity) {
      EntityPtr ret = shared_ptr<Entity>(new Entity(*entity));
      ret->setValue(ret->getValue<string>() + "A"s);
      return ta->next(ret);
    };

    TestTransformPtr tb = make_shared<TestTransform>("B"s, EntityNameGuard("X", RUN));
    tb->m_function = [tb](const EntityPtr entity) {
      EntityPtr ret = shared_ptr<Entity>(new Entity(*entity));
      ret->setValue(ret->getValue<string>() + "B"s);
      return tb->next(ret);
    };

    TestTransformPtr tc = make_shared<TestTransform>("C"s, EntityNameGuard("X", RUN));
    tc->m_function = [](const EntityPtr entity) {
      EntityPtr ret = shared_ptr<Entity>(new Entity(*entity));
      ret->setValue(ret->getValue<string>() + "C"s);
      return ret;
    };

    m_pipeline->getStart()->bind(ta);
    ta->bind(tb);
    tb->bind(tc);
  }

  void TearDown() override
  {
    m_pipeline.reset();
    m_context.reset();
  }

  boost::asio::io_context m_ioContext;
  PipelineContextPtr m_context;
  std::unique_ptr<TestPipeline> m_pipeline;
};

TEST_F(PipelineEditTest, run_three_transforms)
{
  auto entity = shared_ptr<Entity>(new Entity("X", Properties {{"VALUE", "S"s}}));
  auto result = m_pipeline->run(entity);

  ASSERT_EQ("SABC", result->getValue<string>());
}

TEST_F(PipelineEditTest, insert_R_before_B)
{
  TestTransformPtr tr = make_shared<TestTransform>("R"s, EntityNameGuard("X", RUN));
  tr->m_function = [&tr](const EntityPtr entity) {
    EntityPtr ret = shared_ptr<Entity>(new Entity(*entity));
    ret->setValue(ret->getValue<string>() + "R"s);
    return tr->next(ret);
  };

  ASSERT_TRUE(m_pipeline->spliceBefore("B", tr));

  auto entity = shared_ptr<Entity>(new Entity("X", Properties {{"VALUE", "S"s}}));
  auto result = m_pipeline->run(entity);

  ASSERT_EQ("SARBC", result->getValue<string>());
}

TEST_F(PipelineEditTest, insert_R_after_B)
{
  TestTransformPtr tr = make_shared<TestTransform>("R"s, EntityNameGuard("X", RUN));
  tr->m_function = [&tr](const EntityPtr entity) {
    EntityPtr ret = shared_ptr<Entity>(new Entity(*entity));
    ret->setValue(ret->getValue<string>() + "R"s);
    return tr->next(ret);
  };

  ASSERT_TRUE(m_pipeline->spliceAfter("B", tr));

  auto entity = shared_ptr<Entity>(new Entity("X", Properties {{"VALUE", "S"s}}));
  auto result = m_pipeline->run(entity);

  ASSERT_EQ("SABRC", result->getValue<string>());
}

TEST_F(PipelineEditTest, append_R_first_after_B)
{
  TestTransformPtr tr = make_shared<TestTransform>("R"s, EntityNameGuard("X", RUN));
  tr->m_function = [](const EntityPtr entity) {
    EntityPtr ret = shared_ptr<Entity>(new Entity(*entity));
    ret->setValue(ret->getValue<string>() + "R"s);
    return ret;
  };

  ASSERT_TRUE(m_pipeline->firstAfter("B", tr));

  auto entity = shared_ptr<Entity>(new Entity("X", Properties {{"VALUE", "S"s}}));
  auto result = m_pipeline->run(entity);

  ASSERT_EQ("SABR", result->getValue<string>());
}

TEST_F(PipelineEditTest, append_R_last_after_B)
{
  TestTransformPtr tr = make_shared<TestTransform>("R"s, EntityNameGuard("X", RUN));
  tr->m_function = [](const EntityPtr entity) {
    EntityPtr ret = shared_ptr<Entity>(new Entity(*entity));
    ret->setValue(ret->getValue<string>() + "R"s);
    return ret;
  };

  ASSERT_TRUE(m_pipeline->lastAfter("B"s, tr));

  auto entity = shared_ptr<Entity>(new Entity("X", Properties {{"VALUE", "S"s}}));
  auto result = m_pipeline->run(entity);

  ASSERT_EQ("SABC", result->getValue<string>());
}
