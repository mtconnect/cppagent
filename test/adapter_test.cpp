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

#include <map>
#include <sstream>
#include <string>
#include <vector>

TEST(AdapterTest, EscapedLine)
{
  std::map<std::string, std::vector<std::string>> data;
  // correctly escaped
  data["\"a\\|b\""] = {"a|b"};
  data["\"a\\|b\"|z"] = {"a|b", "z"};
  data["y|\"a\\|b\""] = {"y", "a|b"};
  data["y|\"a\\|b\"|z"] = {"y", "a|b", "z"};

  // correctly escaped with multiple pipes
  data["\"a\\|b\\|c\""] = {"a|b|c"};
  data["\"a\\|b\\|c\"|z"] = {"a|b|c", "z"};
  data["y|\"a\\|b\\|c\""] = {"y", "a|b|c"};
  data["y|\"a\\|b\\|c\"|z"] = {"y", "a|b|c", "z"};

  // correctly escaped with pipe at front
  data["\"\\|b\\|c\""] = {"|b|c"};
  data["\"\\|b\\|c\"|z"] = {"|b|c", "z"};
  data["y|\"\\|b\\|c\""] = {"y", "|b|c"};
  data["y|\"\\|b\\|c\"|z"] = {"y", "|b|c", "z"};

  // correctly escaped with pipes at end
  data["\"a\\|b\\|\""] = {"a|b|"};
  data["\"a\\|b\\|\"|z"] = {"a|b|", "z"};
  data["y|\"a\\|b\\|\""] = {"y", "a|b|"};
  data["y|\"a\\|b\\|\"|z"] = {"y", "a|b|", "z"};

  // missing first quote
  data["a\\|b\""] = {"a\\", "b\""};
  data["a\\|b\"|z"] = {"a\\", "b\"", "z"};
  data["y|a\\|b\""] = {"y", "a\\", "b\""};
  data["y|a\\|b\"|z"] = {"y", "a\\", "b\"", "z"};

  // missing first quote and multiple pipes
  data["a\\|b\\|c\""] = {"a\\", "b\\", "c\""};
  data["a\\|b\\|c\"|z"] = {"a\\", "b\\", "c\"", "z"};
  data["y|a\\|b\\|c\""] = {"y", "a\\", "b\\", "c\""};
  data["y|a\\|b\\|c\"|z"] = {"y", "a\\", "b\\", "c\"", "z"};

  // missing last quote
  data["\"a\\|b"] = {"\"a\\", "b"};
  data["\"a\\|b|z"] = {"\"a\\", "b", "z"};
  data["y|\"a\\|b"] = {"y", "\"a\\", "b"};
  data["y|\"a\\|b|z"] = {"y", "\"a\\", "b", "z"};

  // missing last quote and pipe at end et al.
  data["\"a\\|"] = {"\"a\\", ""};
  data["y|\"a\\|"] = {"y", "\"a\\", ""};
  data["y|\"a\\|z"] = {"y", "\"a\\", "z"};
  data["y|\"a\\|\"z"] = {"y", "\"a\\", "\"z"};

  for (const auto &test : data)
  {
    std::string value;
    std::istringstream toParse(test.first);
    for (const std::string &expected : test.second)
    {
      mtconnect::Adapter::getEscapedLine(toParse, value);
      ASSERT_EQ(expected, value);
    }
  }
}
