/*
 * Copyright Copyright 2012, System Insights, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef XML_PRINTER_HPP
#define XML_PRINTER_HPP

#include <map>
#include <list>
#include <string>
#include <vector>
#include <dlib/array.h>

#include <libxml/xmlwriter.h>

#include "component_event.hpp"
#include "device.hpp"
#include "globals.hpp"
#include "asset.hpp"
#include "cutting_tool.hpp"

class DataItem;

namespace XmlPrinter
{  
  /***** Main methods to call *****/
  std::string printError(
    const unsigned int instanceId,
    const unsigned int bufferSize,
    const uint64_t nextSeq,
    const std::string& errorCode,
    const std::string& errorText
  );
  
  std::string printProbe(const unsigned int instanceId,
                         const unsigned int bufferSize,
                         const uint64_t nextSeq,
                         const unsigned int aAssetBufferSize,
                         const unsigned int aAssetCount,
                         std::vector<Device *>& devices,
                         const std::map<std::string, int> *aCounts = NULL);
  
  std::string printSample(const unsigned int instanceId,
                          const unsigned int bufferSize,
                          const uint64_t nextSeq,
                          const uint64_t firstSeq,
                          const uint64_t lastSeq,
                          ComponentEventPtrArray & results
  );

  std::string printAssets(const unsigned int anInstanceId,
                          const unsigned int aBufferSize,
                          const unsigned int anAssetCount,
                          std::vector<AssetPtr> &anAssets);
  
  std::string printCuttingTool(CuttingToolPtr aTool);

  
  void addDevicesNamespace(const std::string &aUrn, const std::string &aLocation, 
                           const std::string &aPrefix);
  void addErrorNamespace(const std::string &aUrn, const std::string &aLocation, 
                         const std::string &aPrefix);
  void addStreamsNamespace(const std::string &aUrn, const std::string &aLocation, 
                           const std::string &aPrefix);
  void addAssetsNamespace(const std::string &aUrn, const std::string &aLocation, 
                         const std::string &aPrefix);
  void setSchemaVersion(const std::string &aVersion);
  const std::string &getSchemaVersion();

  // For testing
  void clearDevicesNamespaces();
  void clearErrorNamespaces();
  void clearStreamsNamespaces();
  void clearAssetsNamespaces();

  const std::string getDevicesUrn(const std::string &aPrefix);
  const std::string getErrorUrn(const std::string &aPrefix);
  const std::string getStreamsUrn(const std::string &aPrefix);
  const std::string getAssetsUrn(const std::string &aPrefix);
  
  const std::string getDevicesLocation(const std::string &aPrefix);
  const std::string getErrorLocation(const std::string &aPrefix);
  const std::string getStreamsLocation(const std::string &aPrefix);
  const std::string getAssetsLocation(const std::string &aPrefix);
};

#endif

