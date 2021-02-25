//
// Copyright Copyright 2009-2020, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <set>
#include <string>

namespace mtconnect
{
  struct Definition
  {
    Definition() = default;
    virtual ~Definition() = default;

    std::string m_description;
  };

  struct AbstractDefinition : public Definition
  {
    AbstractDefinition() = default;
    AbstractDefinition(const AbstractDefinition &) = default;
    virtual ~AbstractDefinition() = default;

    std::string m_key;
    std::string m_keyType;
    std::string m_units;
    std::string m_type;
    std::string m_subType;

    const std::string &getKey() const
    {
      if (m_key.empty())
        return m_keyType;
      else
        return m_key;
    }

    bool operator<(const AbstractDefinition &other) const { return getKey() < other.getKey(); }
    bool operator==(const AbstractDefinition &other) const { return getKey() == other.getKey(); }
  };

  struct CellDefinition : public AbstractDefinition
  {
    CellDefinition() = default;
    CellDefinition(const CellDefinition &) = default;
    virtual ~CellDefinition() = default;
  };

  struct EntryDefinition : public AbstractDefinition
  {
    EntryDefinition() = default;
    EntryDefinition(const EntryDefinition &other)
      : AbstractDefinition(other), m_cells(other.m_cells)
    {
    }
    virtual ~EntryDefinition() = default;

    std::set<CellDefinition> m_cells;
  };

  struct DataItemDefinition : public Definition
  {
    DataItemDefinition() = default;
    DataItemDefinition(const DataItemDefinition &) = default;
    virtual ~DataItemDefinition() = default;

    std::set<EntryDefinition> m_entries;
    std::set<CellDefinition>  m_cells;
  };
}  // namespace mtconnect
