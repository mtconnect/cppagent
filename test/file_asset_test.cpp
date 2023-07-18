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
#include "mtconnect/asset/file_asset.hpp"
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

class FileAssetTest : public testing::Test
{
protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
    FileArchetypeAsset::registerAsset();
    FileAsset::registerAsset();
    m_writer = make_unique<XmlWriter>(true);
  }

  void TearDown() override { m_writer.reset(); }

  std::unique_ptr<XmlWriter> m_writer;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
};

TEST_F(FileAssetTest, TestMinmalArchetype)
{
  const auto doc =
      R"DOC(<FileArchetype applicationCategory="PROCESS" applicationType="INSTRUCTIONS" assetId="F1" mediaType="application/json" name="flickus.json">
  <FileProperties>
    <FileProperty name="user">Mary</FileProperty>
  </FileProperties>
  <FileComments>
    <FileComment timestamp="2020-12-12T10:33:00Z">Created</FileComment>
  </FileComments>
</FileArchetype>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset *>(entity.get());
  ASSERT_NE(nullptr, asset);

  ASSERT_EQ("F1", asset->getAssetId());
  ASSERT_EQ("PROCESS", get<string>(asset->getProperty("applicationCategory")));
  ASSERT_EQ("INSTRUCTIONS", get<string>(asset->getProperty("applicationType")));
  ASSERT_EQ("application/json", get<string>(asset->getProperty("mediaType")));
  ASSERT_EQ("flickus.json", get<string>(asset->getProperty("name")));

  ASSERT_FALSE(asset->getTimestamp());
  ASSERT_FALSE(asset->getDeviceUuid());

  auto properties = asset->getList("FileProperties");
  ASSERT_EQ(1, properties->size());
  ASSERT_EQ("user", get<string>(properties->front()->getProperty("name")));
  ASSERT_EQ("Mary", get<string>(properties->front()->getValue()));

  auto comments = asset->getList("FileComments");
  ASSERT_EQ(1, properties->size());
  ASSERT_EQ("2020-12-12T10:33:00Z", get<string>(comments->front()->getProperty("timestamp")));
  ASSERT_EQ("Created", get<string>(comments->front()->getValue()));

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}

TEST_F(FileAssetTest, TestMinmalFile)
{
  const auto doc =
      R"DOC(<File applicationCategory="PROCESS" applicationType="INSTRUCTIONS" assetId="F1" mediaType="application/json" name="flickus.json" size="1024" state="PRODUCTION" versionId="11.0">
  <FileProperties>
    <FileProperty name="user">Mary</FileProperty>
  </FileProperties>
  <FileComments>
    <FileComment timestamp="2020-12-12T10:33:00Z">Created</FileComment>
  </FileComments>
  <FileLocation href="http://example.com/flickus.json"/>
  <Signature>f572d396fae9206628714fb2ce00f72e94f2258f</Signature>
  <PublicKey>a2f888a51dbb060ad4a0e4be6880549dfd033cbfd0c4f7c132fc90f7ddd146d62f5430471be4f1ce80593315d9927a62590bcad4e0bf09c6d396d82e906be5e2</PublicKey>
  <Destinations>
    <Destination>DEV001</Destination>
    <Destination>DEV002</Destination>
  </Destinations>
  <CreationTime>2020-12-20T10:12:00Z</CreationTime>
  <ModificationTime>2020-12-21T10:12:00Z</ModificationTime>
</File>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;

  auto entity = parser.parse(Asset::getRoot(), doc, errors);
  ASSERT_EQ(0, errors.size());

  auto asset = dynamic_cast<Asset *>(entity.get());
  ASSERT_NE(nullptr, asset);

  EXPECT_EQ("F1", asset->getAssetId());
  EXPECT_EQ("PROCESS", get<string>(asset->getProperty("applicationCategory")));
  EXPECT_EQ("INSTRUCTIONS", get<string>(asset->getProperty("applicationType")));
  EXPECT_EQ("application/json", get<string>(asset->getProperty("mediaType")));
  EXPECT_EQ("flickus.json", get<string>(asset->getProperty("name")));
  EXPECT_EQ(1024, get<int64_t>(asset->getProperty("size")));
  EXPECT_EQ("11.0", get<string>(asset->getProperty("versionId")));
  EXPECT_EQ("PRODUCTION", get<string>(asset->getProperty("state")));
  EXPECT_EQ("f572d396fae9206628714fb2ce00f72e94f2258f",
            get<string>(asset->getProperty("Signature")));
  EXPECT_EQ(
      "a2f888a51dbb060ad4a0e4be6880549dfd033cbfd0c4f7c132fc90f7ddd146d62f5430471be4f1ce80593315d992"
      "7a62590bcad4e0bf09c6d396d82e906be5e2",
      get<string>(asset->getProperty("PublicKey")));
  EXPECT_EQ("2020-12-20T10:12:00Z", get<string>(asset->getProperty("CreationTime")));
  EXPECT_EQ("2020-12-21T10:12:00Z", get<string>(asset->getProperty("ModificationTime")));

  ASSERT_FALSE(asset->getTimestamp());
  ASSERT_FALSE(asset->getDeviceUuid());

  auto properties = asset->getList("FileProperties");
  ASSERT_EQ(1, properties->size());
  EXPECT_EQ("user", get<string>(properties->front()->getProperty("name")));
  EXPECT_EQ("Mary", get<string>(properties->front()->getValue()));

  auto comments = asset->getList("FileComments");
  ASSERT_EQ(1, properties->size());
  ASSERT_EQ("2020-12-12T10:33:00Z", get<string>(comments->front()->getProperty("timestamp")));
  ASSERT_EQ("Created", get<string>(comments->front()->getValue()));

  auto location = get<EntityPtr>(asset->getProperty("FileLocation"));
  EXPECT_TRUE(location);
  EXPECT_EQ("http://example.com/flickus.json", get<string>(location->getProperty("href")));

  auto dests = asset->getList("Destinations");
  ASSERT_EQ(2, dests->size());
  EXPECT_EQ("DEV001", get<string>(dests->front()->getValue()));
  EXPECT_EQ("DEV002", get<string>(dests->back()->getValue()));

  // Round trip test
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  string content = m_writer->getContent();
  ASSERT_EQ(content, doc);
}
