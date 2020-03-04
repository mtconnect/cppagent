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

#include <string>
#include <set>

namespace mtconnect
{
  class AbstractDefinition {
  public:
    std::string m_key;
    std::string m_units;
    std::string m_type;
    std::string m_description;
    
    bool operator<(const AbstractDefinition &other) { return m_key < other.m_key; }
    bool operator==(const AbstractDefinition &other) { return m_key == other.m_key; }
  };
  
  class CellDefinition : public AbstractDefinition {
    
  };
  
  class EntryDefinition : public AbstractDefinition {
  public:
    std::set<CellDefinition> m_cells;
  };
  
  class DataItemDefinition {
  public:
    std::string m_description;

    std::set<EntryDefinition> m_entries;
    std::set<CellDefinition> m_cells;
  };
}

