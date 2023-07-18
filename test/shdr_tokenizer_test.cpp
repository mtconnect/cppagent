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

#include "mtconnect/entity/entity.hpp"
#include "mtconnect/pipeline/shdr_tokenizer.hpp"
#include "mtconnect/pipeline/timestamp_extractor.hpp"

using namespace mtconnect;
using namespace mtconnect::pipeline;
using namespace mtconnect::observation;
using namespace std;
using namespace date::literals;
using namespace std::literals;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class ShdrTokenizerTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_tokenizer = make_shared<ShdrTokenizer>();
    m_tokenizer->bind(make_shared<NullTransform>(TypeGuard<Entity>(RUN)));
  }

  void TearDown() override { m_tokenizer.reset(); }

  shared_ptr<ShdrTokenizer> m_tokenizer;
};

inline std::list<std::string> extract(const Properties &props)
{
  std::list<string> list;
  for (auto &p : props)
    list.emplace_back(get<string>(p.second));

  return list;
}

template <typename T>
inline bool isOfType(const EntityPtr &p)
{
  const auto &o = *p;
  return typeid(T) == typeid(o);
}

TEST_F(ShdrTokenizerTest, SimpleTokens)
{
  std::map<std::string, std::list<std::string>> data {
      {"   |hello   |   kitty| cat | ", {"", "hello", "kitty", "cat", ""}},
      {"hello|kitty", {"hello", "kitty"}},
      {"hello|kitty|", {"hello", "kitty", ""}},
      {"|hello|kitty|", {"", "hello", "kitty", ""}},
      {R"D(hello|xxx={b="12345", c="xxxxx"}}|bbb)D",
       {"hello", R"D(xxx={b="12345", c="xxxxx"}})D", "bbb"}},
  };

  for (const auto &test : data)
  {
    auto data = std::make_shared<entity::Entity>("Data", Properties {{"VALUE", test.first}});
    auto entity = (*m_tokenizer)(std::move(data));
    ASSERT_TRUE(entity);
    auto tokens = dynamic_pointer_cast<Tokens>(entity);
    ASSERT_TRUE(tokens);
    EXPECT_EQ(test.second, tokens->m_tokens) << " given text: " << test.first;
  }
}

TEST_F(ShdrTokenizerTest, escaped_line)
{
  std::map<std::string, std::list<std::string>> data;
  // correctly escaped
  data[R"("a\|b")"] = {"a|b"};
  data[R"("a\|b"|z)"] = {"a|b", "z"};
  data[R"(y|"a\|b")"] = {"y", "a|b"};
  data[R"(y|"a\|b"|z)"] = {"y", "a|b", "z"};

  // correctly escaped with multiple pipes
  data[R"("a\|b\|c")"] = {"a|b|c"};
  data[R"("a\|b\|c"|z)"] = {"a|b|c", "z"};
  data[R"(y|"a\|b\|c")"] = {"y", "a|b|c"};
  data[R"(y|"a\|b\|c"|z)"] = {"y", "a|b|c", "z"};

  // correctly escaped with pipe at front
  data[R"("\|b\|c")"] = {"|b|c"};
  data[R"("\|b\|c"|z)"] = {"|b|c", "z"};
  data[R"(y|"\|b\|c")"] = {"y", "|b|c"};
  data[R"(y|"\|b\|c"|z)"] = {"y", "|b|c", "z"};

  // correctly escaped with pipes at end
  data[R"("a\|b\|")"] = {"a|b|"};
  data[R"("a\|b\|"|z)"] = {"a|b|", "z"};
  data[R"(y|"a\|b\|")"] = {"y", "a|b|"};
  data[R"(y|"a\|b\|"|z)"] = {"y", "a|b|", "z"};

  // missing first quote
  data["a\\|b\""] = {"a\\", "b\""};
  data["a\\|b\"|z"] = {"a\\", "b\"", "z"};
  data["y|a\\|b\""] = {"y", "a\\", "b\""};
  data["y|a\\|b\"|z"] = {"y", "a\\", "b\"", "z"};

  // missing first quote and multiple pipes
  data[R"(a\|b\|c")"] = {"a\\", "b\\", "c\""};
  data[R"(a\|b\|c"|z)"] = {"a\\", "b\\", "c\"", "z"};
  data[R"(y|a\|b\|c")"] = {"y", "a\\", "b\\", "c\""};
  data[R"(y|a\|b\|c"|z)"] = {"y", "a\\", "b\\", "c\"", "z"};

  // missing last quote
  data["\"a\\|b"] = {"\"a\\", "b"};
  data["\"a\\|b|z"] = {"\"a\\", "b", "z"};
  data["y|\"a\\|b"] = {"y", "\"a\\", "b"};
  data["y|\"a\\|b|z"] = {"y", "\"a\\", "b", "z"};

  // missing last quote and pipe at end et al.
  data["\"a\\|"] = {"\"a\\", ""};
  data["y|\"a\\|"] = {"y", "\"a\\", ""};
  data["y|\"a\\|z"] = {"y", "\"a\\", "z"};
  data[R"(y|"a\|"z)"] = {"y", "\"a\\", "\"z"};
  data["x|y||z"] = {"x", "y", "", "z"};

  for (const auto &test : data)
  {
    auto data = std::make_shared<entity::Entity>("Data", Properties {{"VALUE", test.first}});
    auto entity = (*m_tokenizer)(std::move(data));
    ASSERT_TRUE(entity);
    auto tokens = dynamic_pointer_cast<Tokens>(entity);
    ASSERT_TRUE(tokens);
    EXPECT_EQ(test.second, tokens->m_tokens) << " given text: " << test.first;
  }
}
