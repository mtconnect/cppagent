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

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/asset/asset.hpp"
#include "mtconnect/asset/component_configuration_parameters.hpp"
#include "mtconnect/entity/xml_parser.hpp"
#include "mtconnect/entity/xml_printer.hpp"
#include "mtconnect/printer//xml_printer_helper.hpp"
#include "mtconnect/source/adapter/adapter.hpp"

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;
using namespace mtconnect::entity;
using namespace mtconnect::source::adapter;
using namespace mtconnect::asset;
using namespace mtconnect::printer;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class ComponentParametersTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    ComponentConfigurationParameters::registerAsset();
    m_writer = make_unique<XmlWriter>(true);
  }

  void TearDown() override { m_writer.reset(); }

  std::unique_ptr<XmlWriter> m_writer;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

TEST_F(ComponentParametersTest, should_parse_simple_parameter_set)
{
  const auto doc =
      R"DOC(<ComponentConfigurationParameters assetId="PARAMS2" deviceUuid="XXX">
  <ParameterSet name="SET1">
    <Parameter identifier="1" maximum="650" minimum="-650" name="Output Frequency" units="HERTZ">60.0</Parameter>
    <Parameter identifier="2" name="Motor Ctrl Mode">InductionVHz</Parameter>
  </ParameterSet>
</ComponentConfigurationParameters>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset *>(entity.get());
  ASSERT_NE(nullptr, asset);

  ASSERT_EQ("PARAMS2", asset->getAssetId());
  ASSERT_EQ("XXX", get<string>(asset->getProperty("deviceUuid")));

  auto sets = asset->getProperty("ParameterSet");
  auto setList = get<EntityList>(sets);

  ASSERT_EQ(1, setList.size());

  ASSERT_EQ("SET1", setList.front()->get<string>("name"));
  auto params = setList.front()->get<EntityList>("LIST");

  ASSERT_EQ(2, params.size());

  auto it = params.begin();

  ASSERT_EQ("1", (*it)->get<string>("identifier"));
  ASSERT_EQ("Output Frequency", (*it)->get<string>("name"));
  ASSERT_EQ(-650.0, (*it)->get<double>("minimum"));
  ASSERT_EQ(650.0, (*it)->get<double>("maximum"));
  ASSERT_EQ("HERTZ", (*it)->get<string>("units"));
  ASSERT_EQ("60.0", (*it)->getValue<string>());

  it++;

  ASSERT_EQ("2", (*it)->get<string>("identifier"));
  ASSERT_EQ("Motor Ctrl Mode", (*it)->get<string>("name"));
  ASSERT_EQ("InductionVHz", (*it)->getValue<string>());

  auto hash1 = entity->hash();
  entity->addHash();

  auto &hv = entity->getProperty("hash");
  ASSERT_NE(EMPTY, hv.index());

  auto hash2 = entity->hash();
  ASSERT_EQ(hash1, hash2);

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();

  string hdoc(doc);
  hdoc.insert(68, string(" hash=\"" + hash1 + "\""));
  ASSERT_EQ(content, hdoc);

  auto entity2 = parser.parse(Asset::getRoot(), content, errors);

  ASSERT_EQ(hash1, entity2->hash());

  auto e2 = ComponentConfigurationParameters::getFactory();
  auto it2 = params.begin();
  (*it2)->setValue("XXX");

  ASSERT_NE(hash1, entity->hash());
}

TEST_F(ComponentParametersTest, should_parse_two_parameter_sets)
{
  const auto doc =
      R"DOC(<ComponentConfigurationParameters assetId="PARAMS2" deviceUuid="XXX">
  <ParameterSet name="SET1">
    <Parameter identifier="1" maximum="650" minimum="-650" name="Output Frequency" units="HERTZ">60.0</Parameter>
    <Parameter identifier="2" name="Motor Ctrl Mode">InductionVHz</Parameter>
  </ParameterSet>
  <ParameterSet name="SET2">
    <Parameter identifier="1" maximum="550" minimum="-550" name="Output Frequency" units="HERTZ">50.0</Parameter>
    <Parameter identifier="2" name="Motor Ctrl Mode">InductionVHz-1</Parameter>
  </ParameterSet>
</ComponentConfigurationParameters>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset *>(entity.get());
  ASSERT_NE(nullptr, asset);

  ASSERT_EQ("PARAMS2", asset->getAssetId());
  ASSERT_EQ("XXX", get<string>(asset->getProperty("deviceUuid")));

  auto sets = asset->getProperty("ParameterSet");
  auto setList = get<EntityList>(sets);

  ASSERT_EQ(2, setList.size());

  auto si = setList.begin();

  {
    ASSERT_EQ("SET1", (*si)->get<string>("name"));
    auto params = (*si)->get<EntityList>("LIST");

    ASSERT_EQ(2, params.size());

    auto it = params.begin();

    ASSERT_EQ("1", (*it)->get<string>("identifier"));
    ASSERT_EQ("Output Frequency", (*it)->get<string>("name"));
    ASSERT_EQ(-650.0, (*it)->get<double>("minimum"));
    ASSERT_EQ(650.0, (*it)->get<double>("maximum"));
    ASSERT_EQ("HERTZ", (*it)->get<string>("units"));
    ASSERT_EQ("60.0", (*it)->getValue<string>());

    it++;

    ASSERT_EQ("2", (*it)->get<string>("identifier"));
    ASSERT_EQ("Motor Ctrl Mode", (*it)->get<string>("name"));
    ASSERT_EQ("InductionVHz", (*it)->getValue<string>());
  }

  si++;

  {
    ASSERT_EQ("SET2", (*si)->get<string>("name"));
    auto params = (*si)->get<EntityList>("LIST");

    ASSERT_EQ(2, params.size());

    auto it = params.begin();

    ASSERT_EQ("1", (*it)->get<string>("identifier"));
    ASSERT_EQ("Output Frequency", (*it)->get<string>("name"));
    ASSERT_EQ(-550.0, (*it)->get<double>("minimum"));
    ASSERT_EQ(550.0, (*it)->get<double>("maximum"));
    ASSERT_EQ("HERTZ", (*it)->get<string>("units"));
    ASSERT_EQ("50.0", (*it)->getValue<string>());

    it++;

    ASSERT_EQ("2", (*it)->get<string>("identifier"));
    ASSERT_EQ("Motor Ctrl Mode", (*it)->get<string>("name"));
    ASSERT_EQ("InductionVHz-1", (*it)->getValue<string>());
  }

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}
