// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "adapter/adapter.hpp"
#include "agent.hpp"
#include "agent_test_helper.hpp"
#include "json_helper.hpp"
#include "asset/asset.hpp"
#include "entity.hpp"
#include "entity/xml_parser.hpp"
#include "entity/xml_printer.hpp"
#include "xml_printer_helper.hpp"

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
using namespace mtconnect::asset;

class AssetTest : public testing::Test
{
 protected:
protected:
 void SetUp() override
 {
   m_writer = make_unique<XmlWriter>(true);
 }

 void TearDown() override
 {
   m_writer.reset();
 }

  std::unique_ptr<XmlWriter> m_writer;
};

TEST_F(AssetTest, TestExtendedAsset)
{
  auto doc = R"DOC(<ExtendedAsset assetId="EXT1" deviceUuid="local" timestamp="2020-12-20T12:00:00Z">
  <SomeContent>
    <WithSubNodes/>
  </SomeContent>
  <AndOtherContent/>
</ExtendedAsset>
)DOC";

  ErrorList errors;
  entity::XmlParser parser;
  
  auto entity = parser.parse(Asset::getRoot(), doc, "1.7", errors);
  ASSERT_EQ(0, errors.size());
  auto asset = dynamic_cast<Asset*>(entity.get());
  ASSERT_NE(nullptr, asset);
  
  entity::XmlPrinter printer;
  printer.print(*m_writer, entity, {});

  auto expected = R"DOC(<ExtendedAsset assetId="EXT1" deviceUuid="local" timestamp="2020-12-20T12:00:00Z"><SomeContent><WithSubNodes/></SomeContent><AndOtherContent/></ExtendedAsset>
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
  
  auto entity = parser.parse(Asset::getRoot(), doc, "1.7", errors);
  ASSERT_EQ(0, errors.size());
  auto asset = dynamic_cast<Asset*>(entity.get());
  ASSERT_NE(nullptr, asset);
}
