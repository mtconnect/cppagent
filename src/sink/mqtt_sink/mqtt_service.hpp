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

#include "boost/asio/io_context.hpp"
#include <boost/dll/alias.hpp>

#include "mqtt/mqtt_client.hpp"
#include "configuration/agent_config.hpp"
#include "entity/json_printer.hpp"
#include "printer/printer.hpp"
#include "printer/xml_printer_helper.hpp"
#include "sink/sink.hpp"
#include "utilities.hpp"

using namespace std;
using namespace mtconnect::entity;
using namespace mtconnect::mqtt_client;

namespace mtconnect {
  class XmlPrinter;

  namespace sink {
    namespace mqtt_sink {
      class MqttService : public sink::Sink
      {
        // dynamic loading of sink

      public:
        MqttService(boost::asio::io_context &context, sink::SinkContractPtr &&contract,
                    const ConfigOptions &options, const boost::property_tree::ptree &config);

        ~MqttService() = default;

        // Sink Methods
        void start() override;

        void stop() override;

        uint64_t publish(observation::ObservationPtr &observation) override;

        bool publish(asset::AssetPtr asset) override;

        static void registerFactory(SinkFactory &factory);

        std::shared_ptr<MqttClient> getClient();

      protected:

        void loadTopics(const boost::property_tree::ptree &tree, ConfigOptions &options);

      protected:
        boost::asio::io_context &m_context;
        ConfigOptions m_options;
        std::unique_ptr<JsonPrinter> m_jsonPrinter;
        std::shared_ptr<MqttClient> m_client;
      };

    }  // namespace mqtt_sink
  }    // namespace sink
}  // namespace mtconnect
