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

#pragma once

#include "utilities.hpp"

namespace mtconnect {
  namespace pipeline {
    class Pipeline;
  }
  class Source
  {
  public:
    Source(const std::string &name) : m_name(name) {}
    virtual ~Source() {}

    virtual bool start() = 0;
    virtual void stop() = 0;

    virtual pipeline::Pipeline *getPipeline() = 0;

    const auto &getName() { return m_name; }

  protected:
    std::string m_name;
  };

  using SourcePtr = std::shared_ptr<Source>;
  using SourceList = std::list<SourcePtr>;
}  // namespace mtconnect
