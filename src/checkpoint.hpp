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

#include "globals.hpp"

#include "observation.hpp"
#include <map>
#include <string>
#include <vector>
#include <set>

namespace mtconnect {
  class Checkpoint
  {
  public:
    Checkpoint();
    Checkpoint(const Checkpoint &checkpoint, const std::set<std::string> *filterSet = nullptr);
    ~Checkpoint();
    
    void addObservation(Observation *event);
    bool dataSetDifference(Observation *event) const;
    void copy(Checkpoint const &checkpoint, const std::set<std::string> *filterSet = nullptr);
    void clear();
    void filter(std::set<std::string> const &filterSet);
    
    const std::map<std::string, ObservationPtr *> &getEvents() const {
      return m_events; }
    
    void getObservations(ObservationPtrArray &list,
                            std::set<std::string> const *filterSet = nullptr) const;
    
    ObservationPtr *getEventPtr(const std::string &id)
    {
      auto pos = m_events.find(id);
      if(pos != m_events.end())
        return pos->second;
      return nullptr;
    }
    
  protected:
    std::map<std::string, ObservationPtr *> m_events;
    std::set<std::string> m_filter;
    bool m_hasFilter;
  };
}
