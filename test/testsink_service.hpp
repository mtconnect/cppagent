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

#include "sink.hpp"
#include <boost/dll/alias.hpp>

using namespace std;

namespace mtconnect
{
    class sink_plugin_test : public Sink
    {
    public:
      sink_plugin_test(const string &name, boost::asio::io_context &context, SinkContractPtr &&contract,
                  const ConfigOptions &config)
          : Sink(name, move(contract))
      {
      }

      ~sink_plugin_test() = default;

      // Sink Methods
      void start() override {}
      void stop() override {}

      uint64_t publish(observation::ObservationPtr &observation) override { return 0; }
      bool publish(asset::AssetPtr asset) override { return false; }

      // Factory method
      static boost::shared_ptr<sink_plugin_test> create(const string &name, boost::asio::io_context &context, SinkContractPtr &&contract,
                                                    const ConfigOptions &config) {
        return boost::shared_ptr<sink_plugin_test>(
                    new sink_plugin_test(name, context, move(contract), config) );
      }
    };

    BOOST_DLL_ALIAS(
        sink_plugin_test::create,
        create_plugin
    )

}  // namespace mtconnect

