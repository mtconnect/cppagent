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

#include "json_printer.hpp"
#include <nlohmann/json.hpp>
#include <sstream>

using namespace std;
using json = nlohmann::json;

namespace mtconnect {
  std::string JsonPrinter::printError(
                                 const unsigned int instanceId,
                                 const unsigned int bufferSize,
                                 const uint64_t nextSeq,
                                 const std::string &errorCode,
                                 const std::string &errorText
                                 )  const
  {
    json doc = {"MTConnectError", ""};
    
    stringstream buffer;
    buffer << std::setw(4) << doc << '\n';
    return buffer.str();
  }
  
  std::string JsonPrinter::printProbe(
                                 const unsigned int instanceId,
                                 const unsigned int bufferSize,
                                 const uint64_t nextSeq,
                                 const unsigned int assetBufferSize,
                                 const unsigned int assetCount,
                                 const std::vector<Device *> &devices,
                                 const std::map<std::string, int> *count)  const
  {
    json doc = {"MTConnectDevices", ""};
    stringstream buffer;
    buffer << std::setw(4) << doc << '\n';
    return buffer.str();
  }
  
  std::string JsonPrinter::printSample(
                                  const unsigned int instanceId,
                                  const unsigned int bufferSize,
                                  const uint64_t nextSeq,
                                  const uint64_t firstSeq,
                                  const uint64_t lastSeq,
                                  ComponentEventPtrArray &results
                                  ) const
  {
    json doc = {"MTConnectStreams", ""};
    stringstream buffer;
    buffer << std::setw(4) << doc << '\n';
    return buffer.str();
  }
  
  std::string JsonPrinter::printAssets(
                                  const unsigned int anInstanceId,
                                  const unsigned int bufferSize,
                                  const unsigned int assetCount,
                                  std::vector<AssetPtr> const &assets) const
  {
    json doc = {"MTConnectAssets", ""};
    stringstream buffer;
    buffer << std::setw(4) << doc << '\n';
    return buffer.str();
  }
  
  std::string JsonPrinter::printCuttingTool(CuttingToolPtr const tool) const
  {
    json doc = {"CuttingTool", ""};
    stringstream buffer;
    buffer << std::setw(4) << doc << '\n';
    return buffer.str();
  }
}

