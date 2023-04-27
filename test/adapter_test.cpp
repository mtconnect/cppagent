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

#include <boost/asio.hpp>

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "mtconnect/configuration/config_options.hpp"
#include "mtconnect/pipeline/pipeline_context.hpp"
#include "mtconnect/source/adapter/shdr/shdr_adapter.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::source::adapter;
using namespace mtconnect::source::adapter::shdr;
namespace asio = boost::asio;
using namespace std::literals;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

TEST(AdapterTest, MultilineData)
{
  asio::io_context ioc;
  asio::io_context::strand strand(ioc);
  ConfigOptions options {{configuration::Host, "localhost"s}, {configuration::Port, 7878}};
  boost::property_tree::ptree tree;
  pipeline::PipelineContextPtr context = make_shared<pipeline::PipelineContext>();
  auto adapter = make_unique<ShdrAdapter>(ioc, context, options, tree);

  auto handler = make_unique<Handler>();
  string data;
  handler->m_processData = [&](const string &d, const string &s) { data = d; };
  adapter->setHandler(handler);

  adapter->processData("Simple Pass Through");
  EXPECT_EQ("Simple Pass Through", data);

  EXPECT_FALSE(adapter->getTerminator());
  adapter->processData("A multiline message: --multiline--ABC1234");
  EXPECT_TRUE(adapter->getTerminator());
  EXPECT_EQ("--multiline--ABC1234", *adapter->getTerminator());
  adapter->processData("Another Line...");
  adapter->processData("--multiline--ABC---");
  adapter->processData("--multiline--ABC1234");

  const auto exp = R"DOC(A multiline message: 
Another Line...
--multiline--ABC---)DOC";
  EXPECT_EQ(exp, data);
}

TEST(AdapterTest, should_forward_multiline_command)
{
  asio::io_context ioc;
  asio::io_context::strand strand(ioc);
  ConfigOptions options {{configuration::Host, "localhost"s}, {configuration::Port, 7878}};
  boost::property_tree::ptree tree;
  pipeline::PipelineContextPtr context = make_shared<pipeline::PipelineContext>();
  auto adapter = make_unique<ShdrAdapter>(ioc, context, options, tree);

  auto handler = make_unique<Handler>();
  string command, value;
  handler->m_command = [&](const string &c, const string &v, const string &s) {
    command = c;
    value = v;
  };
  adapter->setHandler(handler);

  adapter->processData("* deviceModel: --multiline--ABC1234");
  EXPECT_TRUE(adapter->getTerminator());
  EXPECT_EQ("--multiline--ABC1234", *adapter->getTerminator());
  adapter->processData("<Device id='x' uuid='y'>");
  adapter->processData("  <something/>");
  adapter->processData("</Device>");
  adapter->processData("--multiline--ABC1234");

  const auto exp = R"DOC(<Device id='x' uuid='y'>
  <something/>
</Device>)DOC";
  EXPECT_EQ("devicemodel", command);
  EXPECT_EQ(exp, value);
}

TEST(AdapterTest, should_set_options_from_commands)
{
  asio::io_context ioc;
  asio::io_context::strand strand(ioc);
  ConfigOptions options {{configuration::Host, "localhost"s}, {configuration::Port, 7878}};
  boost::property_tree::ptree tree;
  pipeline::PipelineContextPtr context = make_shared<pipeline::PipelineContext>();
  auto adapter = make_unique<ShdrAdapter>(ioc, context, options, tree);

  adapter->processData("* shdrVersion: 3");

  auto v = GetOption<int>(adapter->getOptions(), "ShdrVersion");
  ASSERT_EQ(int64_t(3), *v);
}
