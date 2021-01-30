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

#include "source/shdr_token_mapper.hpp"
#include "source/duplicate_filter.hpp"
#include "source/rate_filter.hpp"
#include "observation/observation.hpp"
#include <chrono>

using namespace mtconnect;
using namespace mtconnect::source;
using namespace mtconnect::observation;
using namespace std;

class DuplicateFilterTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_mapper = make_shared<ShdrTokenMapper>();
    m_mapper->m_getDevice = [](const std::string &uuid) { return nullptr; };
    m_mapper->m_getDataItem = [this](const Device *, const std::string &name) { return m_dataItems[name].get(); };
  }


  void TearDown() override
  {
    m_dataItems.clear();
  }
  
  DataItem *makeDataItem(std::map<string,string> attributes)
  {
    auto di = make_unique<DataItem>(attributes);
    DataItem *r = di.get();
    m_dataItems.emplace(attributes["id"], move(di));
    
    return r;
  }
  
  
  const EntityPtr observe(TokenList tokens)
  {
    auto ts = make_shared<Timestamped>();
    ts->m_tokens = tokens;
    ts->m_timestamp = chrono::system_clock::now();
    ts->setProperty("timestamp", ts->m_timestamp);

    return (*m_mapper)(ts);
  }
 
  shared_ptr<ShdrTokenMapper> m_mapper;
  std::map<string,unique_ptr<DataItem>> m_dataItems;
};

TEST_F(DuplicateFilterTest, test_simple_event)
{
  makeDataItem({{"id", "a"}, {"type", "EXECUTION"}, {"category", "EVENT"}});

  auto filter = make_shared<DuplicateFilter>();
  filter->bindTo(m_mapper);

  auto os1 = observe({"a", "READY"});
  auto list1 = os1->getValue<EntityList>();
  ASSERT_EQ(1, list1.size());
  
  auto os2 = observe({"a", "READY"});
  auto list2 = os2->getValue<EntityList>();
  ASSERT_EQ(0, list2.size());

  auto os3 = observe({"a", "ACTIVE"});
  auto list3 = os3->getValue<EntityList>();
  ASSERT_EQ(1, list3.size());
}

TEST_F(DuplicateFilterTest, test_simple_sample)
{
  makeDataItem({{"id", "a"}, {"type", "POSITION"}, {"category", "SAMPLE"},
    {"units", "MILLIMETER"}
  });

  auto filter = make_shared<DuplicateFilter>();
  filter->bindTo(m_mapper);

  auto os1 = observe({"a", "1.5"});
  auto list1 = os1->getValue<EntityList>();
  ASSERT_EQ(1, list1.size());
  
  auto os2 = observe({"a", "1.5"});
  auto list2 = os2->getValue<EntityList>();
  ASSERT_EQ(0, list2.size());

  auto os3 = observe({"a", "1.6"});
  auto list3 = os3->getValue<EntityList>();
  ASSERT_EQ(1, list3.size());
}

TEST_F(DuplicateFilterTest, test_minimum_delta)
{
  auto a = makeDataItem({{"id", "a"}, {"type", "POSITION"}, {"category", "SAMPLE"},
    {"units", "MILLIMETER"}
  });
  a->setMinmumDelta(1.0);
  
  auto filter = make_shared<DuplicateFilter>();
  filter->bindTo(m_mapper);

  auto rate = make_shared<RateFilter>();
  rate->bindTo(filter);

  auto os1 = observe({"a", "1.5"});
  auto list1 = os1->getValue<EntityList>();
  ASSERT_EQ(1, list1.size());
  
  auto os2 = observe({"a", "1.5"});
  auto list2 = os2->getValue<EntityList>();
  ASSERT_EQ(0, list2.size());

  auto os3 = observe({"a", "1.6"});
  auto list3 = os3->getValue<EntityList>();
  ASSERT_EQ(1, list3.size());
}
