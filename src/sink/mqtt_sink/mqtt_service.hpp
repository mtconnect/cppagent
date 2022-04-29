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

#include "boost/asio/io_context.hpp"
#include <boost/dll/alias.hpp>

#include "configuration/agent_config.hpp"
#include "sink/sink.hpp"
#include "utilities.hpp"

using namespace std;

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

        void printjSonEntity();

        // Get the printer for a type
        const std::string acceptFormat(const std::string &accepts) const;

        const printer::Printer *printerForAccepts(const std::string &accepts) const;

        // Output an XML Error
        std::string printError(const printer::Printer *printer, const std::string &errorCode,
                               const std::string &text) const;

      protected:
        boost::asio::io_context &m_context;

        ConfigOptions m_options;
      };
      //
      //      BOOST_DLL_ALIAS(MqttService::register_factory, initialize_plugin)
      //
    }  // namespace mqtt_sink
  }    // namespace sink
}  // namespace mtconnect
