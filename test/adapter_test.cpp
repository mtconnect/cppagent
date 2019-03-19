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

#include "Cuti.h"
#include "adapter.hpp"

#include <sstream>
#include <string>
#include <vector>


using namespace std;
using namespace mtconnect;


TEST_CLASS(AdapterTest)
{
public:
  SET_UP();
  TEAR_DOWN();
  
  void testAdapter();
  void testEscapedLine();
  
  CPPUNIT_TEST_SUITE(AdapterTest);
  CPPUNIT_TEST(testAdapter);
  CPPUNIT_TEST(testEscapedLine);
  CPPUNIT_TEST_SUITE_END();

};

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(AdapterTest);

// ComponentTest public methods
void AdapterTest::setUp()
{
}

void AdapterTest::tearDown()
{
}

// ComponentTest protected methods
void AdapterTest::testAdapter()
{
}

void AdapterTest::testEscapedLine()
{
  std::map<std::string, std::vector<std::string> > data;
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
  
  for (auto it = data.begin(); it != data.end(); ++it)
  {
    std::string value;
    std::istringstream toParse(it->first);
    for (const std::string &expected : it->second)
    {
      Adapter::getEscapedLine(toParse, value);
      CPPUNIT_ASSERT_EQUAL_MESSAGE("Source  : " + it->first, expected, value);
    }
  }
}
