//
// Copyright 2009-2026, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "mtconnect/config.hpp"
#include "mtconnect/mqtt/mqtt_client.hpp"
#include "mtconnect/source/adapter/adapter.hpp"
#include "mtconnect/source/adapter/mqtt/mqtt_adapter.hpp"

namespace mtconnect::source::adapter::mqtt_adapter {
  class MqttBroker;

  /// @brief An adapter that hosts an embedded MQTT broker.
  ///
  /// Devices publish to the agent directly. Every publication is routed to any other
  /// subscribers of the broker, and publications matching `Topics` are ingested into the
  /// agent using the same pipeline as the MQTT client adapter (`Protocol = mqtt`).
  ///
  /// Configuration:
  /// - `Protocol = mqtt_broker`
  /// - `Topics`: topic filters to ingest (required)
  /// - `MqttHost`/`Host`: bind address, defaults to `0.0.0.0`
  /// - `MqttPort`/`Port`: listen port, defaults to 1883 (8883 with TLS)
  /// - `MqttTls`, `MqttWs`: transport selection
  /// - `TlsCertificateChain`, `TlsPrivateKey`, `TlsCertificatePassword`, `TlsDHKey`,
  ///   `TlsVerifyClientCertificate`, `TlsClientCAs`: TLS server settings
  /// - `MqttUserName`, `MqttPassword`: single user with full access
  /// - `MqttBrokerSecurity`: path to an mqtt_cpp broker security JSON file for multiple users,
  ///   salted SHA256 passwords, groups and topic ACLs
  class AGENT_LIB_API MqttBrokerAdapter : public Adapter
  {
  public:
    /// @brief Create an Mqtt broker adapter
    /// @param io boost asio io context
    /// @param context pipeline context
    /// @param options configation options
    /// @param block additional configuration options
    MqttBrokerAdapter(boost::asio::io_context& io, pipeline::PipelineContextPtr pipelineContext,
                      const ConfigOptions& options, const boost::property_tree::ptree& block);

    ~MqttBrokerAdapter() override;

    /// @brief Register the Mqtt broker adapter with the factory as `mqtt_broker`
    /// @param factory source factory
    static void registerFactory(SourceFactory& factory);

    /// @name Agent Device methods
    ///@{
    const std::string& getHost() const override { return m_host; }

    unsigned int getPort() const override;
    ///@}

    /// @name Source interface
    ///@{
    bool start() override;

    void stop() override;

    pipeline::Pipeline* getPipeline() override { return &m_pipeline; }
    ///@}

    /// @brief is the internal ingest subscriber connected to the broker
    bool isIngesting() const { return m_client && m_client->isConnected(); }

  protected:
    void subscribeToTopics();

  protected:
    boost::asio::io_context& m_ioContext;

    std::string m_host;
    unsigned int m_port;

    MqttPipeline m_pipeline;

    std::shared_ptr<MqttBroker> m_broker;
    std::shared_ptr<mqtt_client::MqttClient> m_client;
  };
}  // namespace mtconnect::source::adapter::mqtt_adapter
