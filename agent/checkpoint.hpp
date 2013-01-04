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

#ifndef CHECKPOINT_HPP
#define CHECKPOINT_HPP

#include "component_event.hpp"
#include <map>
#include <string>
#include <vector>
#include <set>
#include <dlib/array.h>

class Checkpoint {
public:
  Checkpoint();
  Checkpoint(Checkpoint &aCheckpoint, std::set<std::string> *aFilter = NULL);
  ~Checkpoint();
  
  void addComponentEvent(ComponentEvent *aEvent);
  void copy(Checkpoint &aCheckpoint, std::set<std::string> *aFilter = NULL);
  void clear();
  void filter(std::set<std::string> &aFilter);

  std::map<std::string, ComponentEventPtr*> &getEvents() { return mEvents; }
  void getComponentEvents(ComponentEventPtrArray &list,
                          std::set<std::string> *aFilter = NULL);
  ComponentEventPtr *getEventPtr(std::string anId) { return mEvents[anId]; }
  
protected:
  std::map<std::string, ComponentEventPtr*> mEvents;
  std::set<std::string> mFilter;
  bool mHasFilter;
};

#endif
