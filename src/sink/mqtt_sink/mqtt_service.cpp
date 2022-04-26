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

#include "mqtt_service.hpp"
#include "configuration/config_options.hpp"
#include "entity/xml_parser.hpp"
#include "xml_printer.hpp"
#include "entity.hpp"

using json = nlohmann::json;
using ptree = boost::property_tree::ptree;

using namespace std;
using namespace mtconnect::asset;

namespace asio = boost::asio;
namespace config = ::mtconnect::configuration;

namespace mtconnect {
  namespace sink {
    namespace mqtt_sink {
      // get obeservation in
      // create a json printer
      // call print

      MqttService::MqttService(boost::asio::io_context &context, sink::SinkContractPtr &&contract,
                               const ConfigOptions &options, const ptree &config)
        : Sink("MqttService", move(contract)), m_context(context), m_options(options)
      {
        auto jsonPrinter = dynamic_cast<JsonPrinter *>(m_sinkContract->getPrinter("json"));
        m_jsonPrinter = std::unique_ptr<JsonPrinter>(jsonPrinter);
      }

      void MqttService::start()
      {
        // mqtt client side not a server side...
      }

      void MqttService::stop()
      {
        // stop client side
      }

      uint64_t MqttService::publish(observation::ObservationPtr &observation)
      {
        // get the data item from observation
        auto dataItem = observation->getDataItem();

        // convert to json and send by mqtt
        auto json = m_jsonPrinter->print(dataItem);

        return 0;
      }

      bool MqttService::publish(asset::AssetPtr asset) { return false; }

      // Register the service with the sink factory
      void MqttService::registerFactory(SinkFactory &factory)
      {
        factory.registerFactory(
            "MqttService",
            [](const std::string &name, boost::asio::io_context &io, SinkContractPtr &&contract,
               const ConfigOptions &options, const boost::property_tree::ptree &block) -> SinkPtr {
              auto sink = std::make_shared<MqttService>(io, std::move(contract), options, block);
              return sink;
            });
      }
    }  // namespace mqtt_sink
  }    // namespace sink
}  // namespace mtconnect

