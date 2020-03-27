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
    Definition()
    {
    }
    virtual ~Definition()
    {
    }
    std::string m_description;
  };

  struct AbstractDefinition : public Definition
  {
    AbstractDefinition()
    {
    }
    virtual ~AbstractDefinition()
    {
    }
    std::string m_key;
    std::string m_units;
    std::string m_type;
    std::string m_subType;

    bool operator<(const AbstractDefinition &other)
    {
      return m_key < other.m_key;
    }
    bool operator==(const AbstractDefinition &other)
    {
      return m_key == other.m_key;
    }
  };

  inline bool operator<(const AbstractDefinition &a, const AbstractDefinition &b)
  {
    return a.m_key < b.m_key;
  }

  struct CellDefinition : public AbstractDefinition
  {
    CellDefinition()
    {
    }
    virtual ~CellDefinition()
    {
    }
  };

  struct EntryDefinition : public AbstractDefinition
  {
    EntryDefinition()
    {
    }
    EntryDefinition(const EntryDefinition &other)
        : AbstractDefinition(other), m_cells(other.m_cells)
    {
    }
    virtual ~EntryDefinition()
    {
    }

    std::set<CellDefinition> m_cells;
  };

  struct DataItemDefinition : public Definition
  {
    DataItemDefinition()
    {
    }
    virtual ~DataItemDefinition()
    {
    }

    std::set<EntryDefinition> m_entries;
    std::set<CellDefinition> m_cells;
  };
}  // namespace mtconnect
