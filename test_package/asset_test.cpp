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
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/entity/xml_parser.hpp"
#include "mtconnect/entity/xml_printer.hpp"
#include "mtconnect/printer//xml_printer_helper.hpp"
#include "mtconnect/source/adapter/adapter.hpp"

using json = nlohmann::json;
using namespace std;
using namespace mtconnect;
using namespace mtconnect::entity;
using namespace mtconnect::asset;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class AssetTest : public testing::Test
{
protected:
protected:
  void SetUp() override { m_writer = make_unique<printer::XmlWriter>(true); }

  void TearDown() override { m_writer.reset(); }

  std::unique_ptr<printer::XmlWriter> m_writer;
};

TEST_F(AssetTest, TestExtendedAsset)
{
  auto doc =
      R"DOC(<ExtendedAsset assetId="EXT1" deviceUuid="local" timestamp="2020-12-20T12:00:00Z">
  <SomeContent>
    <WithSubNodes/>
  </SomeContent>
  <AndOtherContent/>
</ExtendedAsset>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());
  auto asset = dynamic_cast<Asset *>(entity.get());
  ASSERT_NE(nullptr, asset);

  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  auto expected =
      R"DOC(<ExtendedAsset assetId="EXT1" deviceUuid="local" timestamp="2020-12-20T12:00:00Z"><SomeContent><WithSubNodes/></SomeContent><AndOtherContent/></ExtendedAsset>
)DOC";

  ASSERT_EQ(expected, m_writer->getContent());
}

TEST_F(AssetTest, asset_should_parse_and_load_if_asset_id_is_missing_from_xml)
{
  auto doc = R"DOC(<PlexHeader>
  <PlexContainerNumber>72626</PlexContainerNumber>
  <Command>1</Command>
  <PartStatus>2</PartStatus>
  <PlexStatus>1</PlexStatus>
  <LastOperationCompleted>30</LastOperationCompleted>
</PlexHeader>)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());
  auto asset = dynamic_cast<Asset *>(entity.get());
  ASSERT_NE(nullptr, asset);
}
