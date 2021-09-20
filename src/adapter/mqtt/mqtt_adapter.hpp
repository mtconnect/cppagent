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
#include "adapter/adapter_pipeline.hpp"

namespace mtconnect {
  namespace adapter {
    namespace mqtt_adapter {
      class MqttAdapterImpl : public std::enable_shared_from_this<MqttAdapterImpl>
      {
      public:
        MqttAdapterImpl(boost::asio::io_context &ioc) : m_ioContext(ioc) {}
        virtual ~MqttAdapterImpl() = default;
        const auto &getIdentity() const { return m_identity; }
        const auto &getUrl() const { return m_url; }

        virtual bool start() = 0;
        virtual void stop() = 0;

      protected:
        boost::asio::io_context &m_ioContext;
        std::string m_url;
        std::string m_identity;
      };

      class MqttPipeline : public adapter::AdapterPipeline
      {
      public:
        MqttPipeline(pipeline::PipelineContextPtr context, boost::asio::io_context::strand &strand)
          : AdapterPipeline(context, strand)
        {}

        const auto &getContract() { return m_context->m_contract; }

        void build(const ConfigOptions &options) override;

      protected:
        ConfigOptions m_options;
      };

      class MqttAdapter : public Adapter
      {
      public:
        MqttAdapter(boost::asio::io_context &io, pipeline::PipelineContextPtr pipelineContext,
                    const ConfigOptions &options, const boost::property_tree::ptree &block);
        ~MqttAdapter() override {}

        const std::string &getHost() const override { return m_host; };
        unsigned int getPort() const override { return m_port; }

        bool start() override
        {
          m_pipeline.start();
          return m_client->start();
        }
        void stop() override { m_client->stop(); }

        pipeline::Pipeline *getPipeline() override { return &m_pipeline; }

      protected:
        void loadTopics(const boost::property_tree::ptree &tree, ConfigOptions &options);

      protected:
        boost::asio::io_context &m_ioContext;
        boost::asio::io_context::strand m_strand;
        // If the connector has been running
        bool m_running;

        std::string m_host;
        unsigned int m_port;

        MqttPipeline m_pipeline;
        std::shared_ptr<MqttAdapterImpl> m_client;
      };
    };  // namespace mqtt_adapter
  }     // namespace adapter
}  // namespace mtconnect
