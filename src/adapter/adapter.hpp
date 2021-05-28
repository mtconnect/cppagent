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

#include "adapter_pipeline.hpp"
#include "source.hpp"

namespace mtconnect
{
  namespace adapter
  {
    class Adapter : public Source
    {
    public:
      Adapter(const std::string &name, const ConfigOptions &options)
        : Source(name), m_options(options)
      {
      }
      virtual ~Adapter() {}

      virtual const std::string &getHost() const = 0;
      virtual const std::string &getUrl() const { return m_url; }
      virtual const std::string &getIdentity() const { return m_identity; }
      virtual unsigned int getPort() const = 0;
      virtual const ConfigOptions &getOptions() const { return m_options; }

    protected:
      std::string m_name;
      std::string m_identity;
      std::string m_url;
      std::unique_ptr<Handler> m_handler;
      ConfigOptions m_options;
    };

    using AdapterPtr = std::shared_ptr<Adapter>;
  }  // namespace adapter
}  // namespace mtconnect
