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
#include "printer/xml_printer.hpp"

namespace asio = boost::asio;
using namespace std;
namespace config = ::mtconnect::configuration;
using ptree = boost::property_tree::ptree;

namespace mtconnect {
  namespace sink {
    namespace mqtt_sink {
      // get obeservation in
      // create a json printer
      // call print

      MqttService::MqttService(boost::asio::io_context &context, sink::SinkContractPtr &&contract,
                               const ConfigOptions &options, const ptree &config)
        : Sink("MqttService", move(contract)), m_context(context), m_options(options)
      {}

      void MqttService::start() {}

      void MqttService::stop() {}

      uint64_t MqttService::publish(observation::ObservationPtr &observation)
      {
        // get the data item from observation
        auto dataItem = observation->getDataItem();

        // convert to json and send by mqtt

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

      void MqttService::printjSonEntity()
      {
        // create a json printer
        //
        // print json/xml particular entity
      }

      // Get the printer for a type
      const std::string MqttService::acceptFormat(const std::string &accepts) const
      {
        std::stringstream list(accepts);
        std::string accept;
        while (std::getline(list, accept, ','))
        {
          for (const auto &p : m_sinkContract->getPrinters())
          {
            if (ends_with(accept, p.first))
              return p.first;
          }
        }
        return "xml";
      }

      const Printer *MqttService::printerForAccepts(const std::string &accepts) const
      {
        return m_sinkContract->getPrinter(acceptFormat(accepts));
      }

      string MqttService::printError(const Printer *printer, const string &errorCode,
                                     const string &text) const
      {
        LOG(debug) << "Returning error " << errorCode << ": " << text;
        /* if (printer)
           return printer->printError(m_instanceId, m_circularBuffer.getBufferSize(),
                                      m_circularBuffer.getSequence(), errorCode, text);
         else*/

        return errorCode + ": " + text;
      }

      //      /* std::shared_ptr<mtconnect::sink::mqtt_sink::MqttService> getMqttService()
      //       {
      //         using namespace mtconnect;
      //         std::unique_ptr<mtconnect::Agent> agent;
      //         sink::SinkPtr sink = agent->findSink("MqttService");
      //         std::shared_ptr<mtconnect::sink::mqtt_sink::MqttService> mqttSink =
      //             std::dynamic_pointer_cast<mtconnect::sink::mqtt_sink::MqttService>(sink);
      //         return mqttSink;
      //       }*/
      //
    }  // namespace mqtt_sink
  }    // namespace sink
}  // namespace mtconnect
