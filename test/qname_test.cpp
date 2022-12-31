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

#include "mtconnect/entity/qname.hpp"

using namespace mtconnect::entity;
using namespace std;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

TEST(QNameTest, should_handle_simple_name_without_prefix)
{
  QName qname("SomeName");

  ASSERT_EQ("SomeName", qname);
  ASSERT_EQ("SomeName", qname.getName());
  ASSERT_EQ("SomeName", qname.getQName());
  ASSERT_TRUE(qname.getNs().empty());
}

TEST(QNameTest, should_split_name_with_prefix)
{
  QName qname("x:SomeName");

  ASSERT_EQ("x:SomeName", qname);
  ASSERT_EQ("SomeName", qname.getName());
  ASSERT_FALSE(qname.getNs().empty());
  ASSERT_EQ("x", qname.getNs());
}

TEST(QNameTest, should_construct_with_name_and_prefix)
{
  QName qname("SomeName", "x");

  ASSERT_EQ("x:SomeName", qname);
  ASSERT_EQ("SomeName", qname.getName());
  ASSERT_FALSE(qname.getNs().empty());
  ASSERT_EQ("x", qname.getNs());
}

TEST(QNameTest, should_set_name_and_keep_namespace)
{
  QName qname("SomeName", "x");

  ASSERT_EQ("x", qname.getNs());

  qname.setName("Dog");
  ASSERT_EQ("x:Dog", qname);
  ASSERT_EQ("Dog", qname.getName());
}

TEST(QNameTest, should_set_namespace_and_keep_name)
{
  QName qname("SomeName");
  ASSERT_TRUE(qname.getNs().empty());

  qname.setNs("x");
  ASSERT_EQ("x:SomeName", qname);
  ASSERT_EQ("SomeName", qname.getName());
}

TEST(QNameTest, should_clear)
{
  QName qname("x:SomeName");
  ASSERT_FALSE(qname.empty());
  ASSERT_FALSE(qname.getNs().empty());
  ASSERT_FALSE(qname.getName().empty());

  qname.clear();
  ASSERT_TRUE(qname.empty());
  ASSERT_TRUE(qname.getNs().empty());
  ASSERT_TRUE(qname.getName().empty());
}
