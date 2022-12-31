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

#include "mtconnect/configuration/parser.hpp"

using namespace mtconnect;
using namespace configuration;
using namespace std;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

TEST(ConfigParserTest, parse_simple_properties)
{
  string cfg = R"DOC(
dog = cat
mellon = water
)DOC";

  auto tree = Parser::parse(cfg);
  ASSERT_EQ(2, tree.size());
  ASSERT_EQ("cat", tree.get<string>("dog"));
  ASSERT_EQ("water", tree.get<string>("mellon"));
}

TEST(ConfigParserTest, parse_with_subtree)
{
  string cfg = R"DOC(
food = beverage
animals {
  ducks = row
  cows = bench
}
)DOC";

  auto tree = Parser::parse(cfg);

  ASSERT_EQ(2, tree.size());
  ASSERT_EQ("beverage", tree.get<string>("food"));
  auto animals = tree.find("animals");
  ASSERT_EQ(2, animals->second.size());
  ASSERT_EQ("row", animals->second.get<string>("ducks"));
  ASSERT_EQ("bench", animals->second.get<string>("cows"));
}

TEST(ConfigParserTest, skip_comments_starting_with_hash)
{
  string cfg = R"DOC(
food = beverage
# this is a comment
animals {
  ducks = row # This comment comes at the end
  cows = bench
# so is this
}
)DOC";

  auto tree = Parser::parse(cfg);

  ASSERT_EQ(2, tree.size());
  ASSERT_EQ("beverage", tree.get<string>("food"));
  auto animals = tree.find("animals");
  ASSERT_EQ(2, animals->second.size());
  ASSERT_EQ("row", animals->second.get<string>("ducks"));
  ASSERT_EQ("bench", animals->second.get<string>("cows"));
}

TEST(ConfigParserTest, invalid_config)
{
  string cfg = R"DOC(
a = b
  }}}
)DOC";

  ASSERT_THROW(Parser::parse(cfg), ParseError);
}

TEST(ConfigParserTest, no_closing_curly)
{
  string cfg = R"DOC(
r = 2
a { dog=cat
)DOC";

  ASSERT_THROW(Parser::parse(cfg), ParseError);
}

TEST(ConfigParserTest, missing_value)
{
  string cfg = R"DOC(
r =
cow = bull
)DOC";

  ASSERT_THROW(Parser::parse(cfg), ParseError);
}

TEST(ConfigParserTest, last_property_ending_with_curly)
{
  string cfg = R"DOC(
food = beverage
# this is a comment
animals {
  ducks = row # This comment comes at the end
  cows = bench}
)DOC";

  auto tree = Parser::parse(cfg);

  ASSERT_EQ(2, tree.size());
  ASSERT_EQ("beverage", tree.get<string>("food"));
  auto animals = tree.find("animals");
  ASSERT_EQ(2, animals->second.size());
  ASSERT_EQ("row", animals->second.get<string>("ducks"));
  ASSERT_EQ("bench", animals->second.get<string>("cows"));
}

TEST(ConfigParserTest, single_line_block)
{
  string cfg = "parents { mother = father }";

  auto tree = Parser::parse(cfg);

  ASSERT_EQ(1, tree.size());
  auto animals = tree.find("parents");
  ASSERT_EQ(1, animals->second.size());
  auto mother = tree.get<string>("parents.mother");
  ASSERT_EQ("father", mother);
}
