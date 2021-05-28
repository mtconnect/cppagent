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

#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <memory>

#include <boost/asio.hpp>

#include "adapter/mqtt/mqtt_adapter.hpp"
#include "pipeline/pipeline_context.hpp"


using namespace std;
using namespace mtconnect;
using namespace mtconnect::adapter;
using namespace mtconnect::adapter::mqtt_adapter;
namespace asio = boost::asio;

class MqttAdapterTest : public testing::Test
{
protected:
 void SetUp() override
 {
 }
  
  void TearDown() override
  {
    
  }
};

TEST_F(MqttAdapterTest, test_parsing_of_simple_data)
{
  
}

