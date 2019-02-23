//
// Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the AMT nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// DISCLAIMER OF WARRANTY. ALL MTCONNECT MATERIALS AND SPECIFICATIONS PROVIDED
// BY AMT, MTCONNECT OR ANY PARTICIPANT TO YOU OR ANY PARTY ARE PROVIDED "AS IS"
// AND WITHOUT ANY WARRANTY OF ANY KIND. AMT, MTCONNECT, AND EACH OF THEIR
// RESPECTIVE MEMBERS, OFFICERS, DIRECTORS, AFFILIATES, SPONSORS, AND AGENTS
// (COLLECTIVELY, THE "AMT PARTIES") AND PARTICIPANTS MAKE NO REPRESENTATION OR
// WARRANTY OF ANY KIND WHATSOEVER RELATING TO THESE MATERIALS, INCLUDING, WITHOUT
// LIMITATION, ANY EXPRESS OR IMPLIED WARRANTY OF NONINFRINGEMENT,
// MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
//
// LIMITATION OF LIABILITY. IN NO EVENT SHALL AMT, MTCONNECT, ANY OTHER AMT
// PARTY, OR ANY PARTICIPANT BE LIABLE FOR THE COST OF PROCURING SUBSTITUTE GOODS
// OR SERVICES, LOST PROFITS, LOSS OF USE, LOSS OF DATA OR ANY INCIDENTAL,
// CONSEQUENTIAL, INDIRECT, SPECIAL OR PUNITIVE DAMAGES OR OTHER DIRECT DAMAGES,
// WHETHER UNDER CONTRACT, TORT, WARRANTY OR OTHERWISE, ARISING IN ANY WAY OUT OF
// THIS AGREEMENT, USE OR INABILITY TO USE MTCONNECT MATERIALS, WHETHER OR NOT
// SUCH PARTY HAD ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.
//
#include "adapter_test.hpp"

#include "adapter.hpp"

#include <sstream>
#include <string>
#include <vector>


using namespace std;
using namespace mtconnect;
namespace mtconnect {
  namespace test {
    
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
  }
}
