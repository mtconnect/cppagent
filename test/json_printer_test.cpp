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

#include "adapter.hpp"
#include "agent.hpp"
#include "xml_printer_helper.hpp"
#include "entity.hpp"
#include "entity/xml_parser.hpp"
#include "entity/xml_printer.hpp"
#include "entity/json_printer.hpp"
#include "entity/json_printer.cpp"
#include <nlohmann/json.hpp>

#include <libxml/xmlwriter.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;
using namespace mtconnect::entity;

class JsonPrinterTest : public testing::Test
{
 protected:
  void SetUp() override
  {
  }

  void TearDown() override
  {
  }
  FactoryPtr createFileArchetypeFactory()
  {
    auto fileProperty =
        make_shared<Factory>(Requirements({Requirement("name", true), Requirement("value", true)}));

    auto fileProperties = make_shared<Factory>(Requirements(
        {Requirement("FileProperty", ENTITY, fileProperty, 1, Requirement::Infinite)}));
    fileProperties->registerMatchers();

    auto fileComment = make_shared<Factory>(
        Requirements({Requirement("timestamp", true), Requirement("value", true)}));

    auto fileComments = make_shared<Factory>(
        Requirements({Requirement("FileComment", ENTITY, fileComment, 1, Requirement::Infinite)}));
    fileComments->registerMatchers();

    auto fileArchetype = make_shared<Factory>(Requirements{
        Requirement("assetId", true), Requirement("deviceUuid", true),
        Requirement("timestamp", true), Requirement("removed", false), Requirement("name", true),
        Requirement("mediaType", true), Requirement("applicationCategory", true),
        Requirement("applicationType", true), Requirement("Description", false),
        Requirement("FileComments", ENTITY_LIST, fileComments, false),
        Requirement("FileProperties", ENTITY_LIST, fileProperties, false)});

    auto root =
        make_shared<Factory>(Requirements{Requirement("FileArchetype", ENTITY, fileArchetype)});

    return root;
  }
};

TEST_F(JsonPrinterTest, TestParseSimpleDocument)
{
  auto root = createFileArchetypeFactory();

  auto doc = string{
      "<FileArchetype applicationCategory=\"ASSEMBLY\" applicationType=\"DATA\" assetId=\"123\" "
      "deviceUuid=\"duid\" mediaType=\"json\" name=\"xxxx\" timestamp=\"2020-12-01T10:00Z\">\n"
      "  <Description>Hello there Shaurabh</Description>\n"
      "  <FileProperties>\n"
      "    <FileProperty name=\"one\">Round</FileProperty>\n"
      "    <FileProperty name=\"two\">Flat</FileProperty>\n"
      "  </FileProperties>\n"
      "</FileArchetype>\n"};

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(root, doc, "1.7", errors);
  ASSERT_EQ(0, errors.size());

  entity::JsonPrinter jprinter;

  json jdoc;
  
  jdoc = jprinter.print(entity);

  ASSERT_EQ("123", jdoc.at("/FileArchetype/assetId"_json_pointer).get<string>());
  ASSERT_EQ("one", jdoc.at("/FileArchetype/FileProperties/0/FileProperty/name"_json_pointer).get<string>());
  ASSERT_EQ("Round", jdoc.at("/FileArchetype/FileProperties/0/FileProperty/value"_json_pointer).get<string>());
}
