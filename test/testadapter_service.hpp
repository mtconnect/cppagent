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

#include "adapter/adapter.hpp"
#include <boost/dll/alias.hpp>

using namespace std;
using namespace mtconnect::adapter;

namespace mtconnect
{
    class adapter_plugin_test : public Adapter
    {
    public:
      adapter_plugin_test(const string &name, boost::asio::io_context &context, const std::string &server, const unsigned int port,
                         const mtconnect::ConfigOptions &options, std::unique_ptr<AdapterPipeline> &pipeline)
          :Adapter(context, server, port, options, pipeline)
      {
      }

      ~adapter_plugin_test() = default;

      bool start() override { return true; }
      void stop() override {}

      void processData(const std::string &data) override {}
      void protocolCommand(const std::string &data) override {}

      // Factory method
      static std::shared_ptr<adapter_plugin_test> create(const string &name, boost::asio::io_context &context, const std::string &server, const unsigned int port,
                                                          const mtconnect::ConfigOptions &options, std::unique_ptr<AdapterPipeline> &pipeline) {
          return std::make_shared<adapter_plugin_test>(name, context, server, port, options, pipeline);
      }
    };

    BOOST_DLL_ALIAS(
          adapter_plugin_test::create,
          create_adapter_plugin                               // <-- ...this alias name
          )
}  // namespace mtconnect

