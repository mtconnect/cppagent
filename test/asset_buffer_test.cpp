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
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "agent_test_helper.hpp"
#include "mtconnect/asset/asset_buffer.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/printer//xml_printer.hpp"
#include "test_utilities.hpp"

#if defined(WIN32) && _MSC_VER < 1500
typedef __int64 int64_t;
#endif

// Registers the fixture into the 'registry'
using namespace std;
using namespace std::chrono;
using namespace mtconnect;
using namespace mtconnect::entity;
using namespace mtconnect::asset;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class AssetBufferTest : public testing::Test
{
protected:
  void SetUp() override { m_assetBuffer = make_unique<AssetBuffer>(10); }

  void TearDown() override { m_assetBuffer.reset(); }

  AssetPtr makeAsset(const string &type, const string &uuid, const string &device, const string &ts,
                     ErrorList &errors)
  {
    Properties props {{"assetId", uuid}, {"deviceUuid", device}, {"timestamp", ts}};
    auto asset = Asset::getFactory()->make(type, props, errors);
    return dynamic_pointer_cast<Asset>(asset);
  }

  void makeTypeAssets()
  {
    ErrorList errors;
    auto asset = makeAsset("Asset1", "A1", "D1", "2020-12-01T12:00:00Z", errors);
    ASSERT_EQ(0, errors.size());
    m_assetBuffer->addAsset(asset);

    asset = makeAsset("Asset1", "A2", "D1", "2020-12-01T12:00:00Z", errors);
    ASSERT_EQ(0, errors.size());
    m_assetBuffer->addAsset(asset);

    asset = makeAsset("Asset1", "A3", "D2", "2020-12-01T12:00:00Z", errors);
    ASSERT_EQ(0, errors.size());
    m_assetBuffer->addAsset(asset);

    asset = makeAsset("Asset1", "A4", "D2", "2020-12-01T12:00:00Z", errors);
    ASSERT_EQ(0, errors.size());
    m_assetBuffer->addAsset(asset);

    asset = makeAsset("Asset1", "A5", "D2", "2020-12-01T12:00:00Z", errors);
    ASSERT_EQ(0, errors.size());
    m_assetBuffer->addAsset(asset);

    asset = makeAsset("Asset2", "A6", "D1", "2020-12-01T12:00:00Z", errors);
    ASSERT_EQ(0, errors.size());
    m_assetBuffer->addAsset(asset);

    asset = makeAsset("Asset2", "A7", "D2", "2020-12-01T12:00:00Z", errors);
    ASSERT_EQ(0, errors.size());
    m_assetBuffer->addAsset(asset);

    asset = makeAsset("Asset2", "A8", "D2", "2020-12-01T12:00:00Z", errors);
    ASSERT_EQ(0, errors.size());
    m_assetBuffer->addAsset(asset);

    asset = makeAsset("Asset2", "A9", "D2", "2020-12-01T12:00:00Z", errors);
    ASSERT_EQ(0, errors.size());
    m_assetBuffer->addAsset(asset);

    asset = makeAsset("Asset2", "A10", "D2", "2020-12-01T12:00:00Z", errors);
    ASSERT_EQ(0, errors.size());
    m_assetBuffer->addAsset(asset);

    asset = makeAsset("Asset2", "A11", "D2", "2020-12-01T12:00:00Z", errors);
    ASSERT_EQ(0, errors.size());
    m_assetBuffer->addAsset(asset);
  }

public:
  std::unique_ptr<AssetBuffer> m_assetBuffer;
};

TEST_F(AssetBufferTest, AddAsset)
{
  ErrorList errors;
  auto asset = makeAsset("Asset", "A1", "D1", "2020-12-01T12:00:00Z", errors);
  ASSERT_EQ(0, errors.size());

  m_assetBuffer->addAsset(asset);
  ASSERT_EQ(1, m_assetBuffer->getCount());
  ASSERT_EQ(1, m_assetBuffer->getCountForType("Asset"));
  ASSERT_EQ(1, m_assetBuffer->getCountForDevice("D1"));
}

TEST_F(AssetBufferTest, ReplaceAsset)
{
  ErrorList errors;
  auto asset1 = makeAsset("Asset", "A1", "D1", "2020-12-01T12:00:00Z", errors);
  ASSERT_EQ(0, errors.size());

  m_assetBuffer->addAsset(asset1);
  ASSERT_EQ(1, m_assetBuffer->getCount());

  auto asset2 = makeAsset("Asset", "A1", "D2", "2020-12-01T12:00:00Z", errors);
  ASSERT_EQ(0, errors.size());

  m_assetBuffer->addAsset(asset2);
  ASSERT_EQ(1, m_assetBuffer->getCount());
  ASSERT_EQ(1, m_assetBuffer->getCountForType("Asset"));
  ASSERT_EQ(0, m_assetBuffer->getCountForDevice("D1"));
  ASSERT_EQ(1, m_assetBuffer->getCountForDevice("D2"));
}

TEST_F(AssetBufferTest, TestOverflow)
{
  ErrorList errors;
  for (int i = 0; i < 10; i++)
  {
    auto asset = makeAsset("Asset", "A" + to_string(i), "D" + to_string(i % 3),
                           "2020-12-01T12:00:00Z", errors);
    ASSERT_EQ(0, errors.size());
    m_assetBuffer->addAsset(asset);
  }

  ASSERT_EQ(10, m_assetBuffer->getCount());
  ASSERT_EQ(10, m_assetBuffer->getCountForType("Asset"));
  ASSERT_EQ(4, m_assetBuffer->getCountForDevice("D0"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D1"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D2"));

  auto asset = makeAsset("Asset", "A11", "D3", "2020-12-01T12:00:00Z", errors);
  ASSERT_EQ(0, errors.size());
  m_assetBuffer->addAsset(asset);
  ASSERT_EQ(10, m_assetBuffer->getCount());
  ASSERT_EQ(10, m_assetBuffer->getCountForType("Asset"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D0"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D1"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D2"));
  ASSERT_EQ(1, m_assetBuffer->getCountForDevice("D3"));
}

TEST_F(AssetBufferTest, RemovedAsset)
{
  ErrorList errors;
  for (int i = 0; i < 10; i++)
  {
    auto asset = makeAsset("Asset", "A" + to_string(i), "D" + to_string(i % 3),
                           "2020-12-01T12:00:00Z", errors);
    ASSERT_EQ(0, errors.size());
    m_assetBuffer->addAsset(asset);
  }

  ASSERT_EQ(10, m_assetBuffer->getCount());
  ASSERT_EQ(9, m_assetBuffer->getIndex("A0"));
  ASSERT_EQ(0, m_assetBuffer->getIndex("A9"));
  ASSERT_EQ(10, m_assetBuffer->getCountForType("Asset"));
  ASSERT_EQ(4, m_assetBuffer->getCountForDevice("D0"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D1"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D2"));

  auto a0 = m_assetBuffer->getAsset("A0");
  m_assetBuffer->removeAsset(a0->getAssetId());
  ASSERT_EQ(9, m_assetBuffer->getIndex("A0"));

  ASSERT_EQ(10, m_assetBuffer->getCount(false));
  ASSERT_EQ(9, m_assetBuffer->getCount());
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D0"));
  ASSERT_EQ(4, m_assetBuffer->getCountForDevice("D0", false));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D1"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D2"));

  auto asset = makeAsset("Asset", "A11", "D3", "2020-12-01T12:00:00Z", errors);
  ASSERT_EQ(0, errors.size());
  m_assetBuffer->addAsset(asset);

  ASSERT_EQ(-1, m_assetBuffer->getIndex("A0"));

  ASSERT_EQ(10, m_assetBuffer->getCount());
  ASSERT_EQ(10, m_assetBuffer->getCount(false));
  ASSERT_EQ(10, m_assetBuffer->getCountForType("Asset"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D0"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D1"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D2"));
  ASSERT_EQ(1, m_assetBuffer->getCountForDevice("D3"));
}

TEST_F(AssetBufferTest, verify_asset_counts_by_type)
{
  m_assetBuffer = make_unique<AssetBuffer>(12);
  makeTypeAssets();

  ASSERT_EQ(11, m_assetBuffer->getCount());
  ASSERT_EQ(5, m_assetBuffer->getCountForType("Asset1"));
  ASSERT_EQ(6, m_assetBuffer->getCountForType("Asset2"));

  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D1"));
  ASSERT_EQ(8, m_assetBuffer->getCountForDevice("D2"));

  auto counts1 = m_assetBuffer->getCountsByType();
  ASSERT_EQ(2, counts1.size());
  ASSERT_EQ(5, counts1["Asset1"]);
  ASSERT_EQ(6, counts1["Asset2"]);

  auto counts2 = m_assetBuffer->getCountsByTypeForDevice("D1");
  ASSERT_EQ(2, counts2.size());
  ASSERT_EQ(2, counts2["Asset1"]);
  ASSERT_EQ(1, counts2["Asset2"]);

  auto counts3 = m_assetBuffer->getCountsByTypeForDevice("D2");
  ASSERT_EQ(2, counts3.size());
  ASSERT_EQ(3, counts3["Asset1"]);
  ASSERT_EQ(5, counts3["Asset2"]);
}

TEST_F(AssetBufferTest, verify_asset_counts_with_removal)
{
  m_assetBuffer = make_unique<AssetBuffer>(12);
  makeTypeAssets();

  m_assetBuffer->removeAsset("A2");
  auto counts1 = m_assetBuffer->getCountsByTypeForDevice("D1");
  ASSERT_EQ(2, counts1.size());
  ASSERT_EQ(1, counts1["Asset1"]);
  ASSERT_EQ(1, counts1["Asset2"]);

  auto counts2 = m_assetBuffer->getCountsByTypeForDevice("D1", false);
  ASSERT_EQ(2, counts1.size());
  ASSERT_EQ(1, counts1["Asset1"]);
  ASSERT_EQ(1, counts1["Asset2"]);

  m_assetBuffer->removeAsset("A7");
  auto counts3 = m_assetBuffer->getCountsByTypeForDevice("D2");
  ASSERT_EQ(2, counts3.size());
  ASSERT_EQ(3, counts3["Asset1"]);
  ASSERT_EQ(4, counts3["Asset2"]);

  auto counts4 = m_assetBuffer->getCountsByTypeForDevice("D2", false);
  ASSERT_EQ(2, counts4.size());
  ASSERT_EQ(3, counts4["Asset1"]);
  ASSERT_EQ(5, counts4["Asset2"]);

  m_assetBuffer->removeAsset("A1");
  auto counts5 = m_assetBuffer->getCountsByTypeForDevice("D1");
  ASSERT_EQ(1, counts5.size());
  ASSERT_EQ(1, counts5["Asset2"]);

  auto counts6 = m_assetBuffer->getCountsByTypeForDevice("D1", false);
  ASSERT_EQ(2, counts6.size());
  ASSERT_EQ(2, counts6["Asset1"]);
  ASSERT_EQ(1, counts6["Asset2"]);

  ErrorList errors;
  auto asset = makeAsset("Asset3", "A20", "D1", "2020-12-01T12:00:00Z", errors);
  ASSERT_EQ(0, errors.size());
  m_assetBuffer->addAsset(asset);

  asset = makeAsset("Asset3", "A21", "D2", "2020-12-01T12:00:00Z", errors);
  ASSERT_EQ(0, errors.size());
  m_assetBuffer->addAsset(asset);

  ASSERT_EQ(10, m_assetBuffer->getCount());
  ASSERT_EQ(12, m_assetBuffer->getCount(false));

  AssetList list;
  m_assetBuffer->getAssets(list, 20);
  ASSERT_EQ(10, list.size());
  ASSERT_EQ(list.back()->getAssetId(), "A3");
  ASSERT_EQ(list.front()->getAssetId(), "A21");

  list.clear();
  m_assetBuffer->getAssets(list, 20, false);
  ASSERT_EQ(12, list.size());
  ASSERT_EQ(list.back()->getAssetId(), "A2");
  ASSERT_EQ(list.front()->getAssetId(), "A21");

  auto counts7 = m_assetBuffer->getCountsByTypeForDevice("D1");
  ASSERT_EQ(2, counts7.size());
  ASSERT_EQ(1, counts7["Asset2"]);
  ASSERT_EQ(1, counts7["Asset3"]);

  auto counts8 = m_assetBuffer->getCountsByTypeForDevice("D2");
  ASSERT_EQ(3, counts8.size());
  ASSERT_EQ(3, counts8["Asset1"]);
  ASSERT_EQ(4, counts8["Asset2"]);
  ASSERT_EQ(1, counts8["Asset3"]);

  auto counts9 = m_assetBuffer->getCountsByType();
  ASSERT_EQ(3, counts9.size());
  ASSERT_EQ(3, counts9["Asset1"]);
  ASSERT_EQ(5, counts9["Asset2"]);
  ASSERT_EQ(2, counts9["Asset3"]);

  auto counts10 = m_assetBuffer->getCountsByType(false);
  ASSERT_EQ(3, counts10.size());
  ASSERT_EQ(4, counts10["Asset1"]);
  ASSERT_EQ(6, counts10["Asset2"]);
  ASSERT_EQ(2, counts10["Asset3"]);
}
