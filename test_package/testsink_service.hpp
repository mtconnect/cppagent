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

#include <boost/dll/alias.hpp>

#include "mtconnect/configuration/agent_config.hpp"

using namespace std;

namespace mtconnect {
  class sink_plugin_test : public sink::Sink
  {
  public:
    sink_plugin_test(const string &name, boost::asio::io_context &context,
                     sink::SinkContractPtr &&contract, const ConfigOptions &config)
      : sink::Sink(name, std::move(contract))
    {}

    ~sink_plugin_test() = default;

    // Sink Methods
    void start() override {}
    void stop() override {}

    bool publish(observation::ObservationPtr &observation) override { return false; }
    bool publish(asset::AssetPtr asset) override { return false; }

    static sink::SinkPtr create(const std::string &name, boost::asio::io_context &io,
                                sink::SinkContractPtr &&contract, const ConfigOptions &options,
                                const boost::property_tree::ptree &block)
    {
      return std::make_shared<sink_plugin_test>(name, io, std::move(contract), options);
    }

    static void register_factory(const boost::property_tree::ptree &block,
                                 configuration::AgentConfiguration &config)
    {
      mtconnect::configuration::gAgentLogger = config.getLogger();
      PLUGIN_LOG(debug) << "Registering sink factory for sink_plugin_test";
      config.getSinkFactory().registerFactory("sink_plugin_test", &sink_plugin_test::create);
    }
  };

  BOOST_DLL_ALIAS(sink_plugin_test::register_factory, initialize_plugin)

}  // namespace mtconnect
