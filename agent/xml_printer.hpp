/*
* Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the AMT nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* DISCLAIMER OF WARRANTY. ALL MTCONNECT MATERIALS AND SPECIFICATIONS PROVIDED
* BY AMT, MTCONNECT OR ANY PARTICIPANT TO YOU OR ANY PARTY ARE PROVIDED "AS IS"
* AND WITHOUT ANY WARRANTY OF ANY KIND. AMT, MTCONNECT, AND EACH OF THEIR
* RESPECTIVE MEMBERS, OFFICERS, DIRECTORS, AFFILIATES, SPONSORS, AND AGENTS
* (COLLECTIVELY, THE "AMT PARTIES") AND PARTICIPANTS MAKE NO REPRESENTATION OR
* WARRANTY OF ANY KIND WHATSOEVER RELATING TO THESE MATERIALS, INCLUDING, WITHOUT
* LIMITATION, ANY EXPRESS OR IMPLIED WARRANTY OF NONINFRINGEMENT,
* MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. 

* LIMITATION OF LIABILITY. IN NO EVENT SHALL AMT, MTCONNECT, ANY OTHER AMT
* PARTY, OR ANY PARTICIPANT BE LIABLE FOR THE COST OF PROCURING SUBSTITUTE GOODS
* OR SERVICES, LOST PROFITS, LOSS OF USE, LOSS OF DATA OR ANY INCIDENTAL,
* CONSEQUENTIAL, INDIRECT, SPECIAL OR PUNITIVE DAMAGES OR OTHER DIRECT DAMAGES,
* WHETHER UNDER CONTRACT, TORT, WARRANTY OR OTHERWISE, ARISING IN ANY WAY OUT OF
* THIS AGREEMENT, USE OR INABILITY TO USE MTCONNECT MATERIALS, WHETHER OR NOT
* SUCH PARTY HAD ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.
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

  // For testing
  void clearDevicesNamespaces();
  void clearErrorNamespaces();
  void clearStreamsNamespaces();
  void clearAssetsNamespaces();

  const std::string getDevicesUrn(const std::string &aPrefix);
  const std::string getErrorUrn(const std::string &aPrefix);
  const std::string getStreamsUrn(const std::string &aPrefix);
  const std::string getAssetsUrn(const std::string &aPrefix);
};

#endif

