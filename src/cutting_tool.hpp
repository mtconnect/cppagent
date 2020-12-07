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

#pragma once

#include "asset.hpp"
#include "globals.hpp"

#include <map>
#include <utility>
#include <vector>

namespace mtconnect
{
  class Printer;
  class CuttingTool;
  using CuttingToolPtr = RefCountedPtr<CuttingTool>;

  class CuttingToolValue;
  using CuttingToolValuePtr = RefCountedPtr<CuttingToolValue>;

  class CuttingItem;
  using CuttingItemPtr = RefCountedPtr<CuttingItem>;

  class CuttingToolValue : public RefCounted
  {
   public:
    CuttingToolValue(std::string aKey, std::string aValue)
        : m_key(std::move(aKey)), m_value(std::move(aValue))
    {
    }

    CuttingToolValue() = default;

    CuttingToolValue(const CuttingToolValue &another) = default;

    ~CuttingToolValue() override;

   public:
    std::map<std::string, std::string> m_properties;
    std::string m_key;
    std::string m_value;
  };

  class CuttingItem : public RefCounted
  {
   public:
    ~CuttingItem() override;

   public:
    std::map<std::string, std::string> m_identity;
    std::map<std::string, CuttingToolValuePtr> m_values;
    std::map<std::string, CuttingToolValuePtr> m_measurements;
    std::vector<CuttingToolValuePtr> m_lives;
  };

  class CuttingTool : public Asset
  {
   public:
    CuttingTool(const std::string &assetId, const std::string &type, const std::string &content,
                bool removed = false)
        : Asset(assetId, type, content, removed)
    {
    }
    ~CuttingTool() override;

    void addIdentity(const std::string &key, const std::string &value) override;
    void addValue(const CuttingToolValuePtr value);
    void updateValue(const std::string &key, const std::string &value);

    std::string &getContent(const Printer *aPrinter) override;
    void changed() override { m_content.clear(); }

   public:
    std::vector<std::string> m_status;
    std::map<std::string, CuttingToolValuePtr> m_values;
    std::map<std::string, CuttingToolValuePtr> m_measurements;
    std::string m_itemCount;
    std::vector<CuttingItemPtr> m_items;
    std::vector<CuttingToolValuePtr> m_lives;
  };
}  // namespace mtconnect
