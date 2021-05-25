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

#include <sstream>

#include <mqtt_client_cpp.hpp>
#include <boost/uuid/name_generator_sha1.hpp>

#include "pipeline/pipeline.hpp"


namespace mtconnect
{
  class source
  {
    class MqttPipeline : public pipeline::Pipeline
    {
    public:
      MqttPipeline(pipeline::PipelineContextPtr context) : Pipeline(context) {}

      const auto &getContract() { return m_context->m_contract; }
      
      void build(const ConfigOptions &options) override;

    protected:
      ConfigOptions m_options;
    };

    class MqttSource : public Source
    {
    public:
      MqttSource(boost::asio::io_context &context, const ConfigOptions &options,
                 std::unique_ptr<MqttPipeline> &&pipeline)
      : Source("MQTT"), m_pipeline(std::move(pipeline))
      {
        std::stringstream url;
        url << "mqtt://" << m_server << ':' << m_port;
        m_url = url.str();

        std::stringstream identity;
        identity << '_' << m_server << '_' << m_port;
        m_name = identity.str();
        
        boost::uuids::detail::sha1 sha1;
        sha1.process_bytes(identity.str().c_str(), identity.str().length());
        boost::uuids::detail::sha1::digest_type digest;
        sha1.get_digest(digest);

        identity.str("");
        identity << std::hex << digest[0] << digest[1] << digest[2];
        m_identity = std::string("_") + (identity.str()).substr(0, 10);
      }
      ~MqttSource() {}
      
      bool start() override
      {
        return true;
      }
      
      void stop() override
      {
        
      }
    
    protected:
      // Name of device associated with adapter
      std::string m_url;
      std::string m_identity;
      
      std::string m_server;
      unsigned int m_port;

      // If the connector has been running
      bool m_running;

      std::unique_ptr<MqttPipeline> m_pipeline;
    };
  };
}

      
      
