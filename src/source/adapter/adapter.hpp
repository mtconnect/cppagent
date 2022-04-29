//
// Copyright Copyright 2009-2022, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "source/adapter/adapter_pipeline.hpp"
#include "source/source.hpp"

namespace mtconnect::source::adapter {
  class Adapter : public Source
  {
  public:
    Adapter(const std::string &name, boost::asio::io_context &io, const ConfigOptions &options)
      : Source(name, io), m_options(options)
    {}
    virtual ~Adapter() {}

    virtual const std::string &getHost() const = 0;
    const std::string &getIdentity() const override { return m_identity; }
    virtual unsigned int getPort() const = 0;
    virtual const ConfigOptions &getOptions() const { return m_options; }

    void setHandler(std::unique_ptr<Handler> &h) { m_handler = std::move(h); }

  protected:
    std::string m_identity;
    std::unique_ptr<Handler> m_handler;
    ConfigOptions m_options;
  };

  using AdapterPtr = std::shared_ptr<Adapter>;
}  // namespace mtconnect::source::adapter
