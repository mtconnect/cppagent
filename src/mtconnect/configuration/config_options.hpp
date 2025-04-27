//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

/// @file config_options.hpp
/// @brief Contains all the known configuration options

#include "mtconnect/config.hpp"

namespace mtconnect {
  namespace configuration {

/// @brief creates an const char * from the name as a string
///
///   stringizes `name`
///
/// @param name name of configuration parameter
#define DECLARE_CONFIGURATION(name) inline const char *name = #name;

    /// @name Global Configuration Options
    ///@{
    DECLARE_CONFIGURATION(WorkingDirectory);
    DECLARE_CONFIGURATION(DataPath);
    DECLARE_CONFIGURATION(ConfigPath);
    DECLARE_CONFIGURATION(PluginPath);
    ///@}

    /// @name Agent Configuration
    ///@{
    DECLARE_CONFIGURATION(AgentDeviceUUID);
    DECLARE_CONFIGURATION(DisableAgentDevice);
    DECLARE_CONFIGURATION(AllowPut);
    DECLARE_CONFIGURATION(AllowPutFrom);
    DECLARE_CONFIGURATION(BufferSize);
    DECLARE_CONFIGURATION(CheckpointFrequency);
    DECLARE_CONFIGURATION(Devices);
    DECLARE_CONFIGURATION(HttpHeaders);
    DECLARE_CONFIGURATION(JsonVersion);
    DECLARE_CONFIGURATION(LogStreams);
    DECLARE_CONFIGURATION(MaxAssets);
    DECLARE_CONFIGURATION(MaxCachedFileSize);
    DECLARE_CONFIGURATION(MinCompressFileSize);
    DECLARE_CONFIGURATION(MinimumConfigReloadAge);
    DECLARE_CONFIGURATION(MonitorConfigFiles);
    DECLARE_CONFIGURATION(MonitorInterval);
    DECLARE_CONFIGURATION(PidFile);
    DECLARE_CONFIGURATION(Port);
    DECLARE_CONFIGURATION(Pretty);
    DECLARE_CONFIGURATION(SchemaVersion);
    DECLARE_CONFIGURATION(ServerIp);
    DECLARE_CONFIGURATION(ServiceName);
    DECLARE_CONFIGURATION(Sender);
    DECLARE_CONFIGURATION(TlsCertificateChain);
    DECLARE_CONFIGURATION(TlsCertificatePassword);
    DECLARE_CONFIGURATION(TlsClientCAs);
    DECLARE_CONFIGURATION(TlsDHKey);
    DECLARE_CONFIGURATION(TlsOnly);
    DECLARE_CONFIGURATION(TlsPrivateKey);
    DECLARE_CONFIGURATION(TlsVerifyClientCertificate);
    DECLARE_CONFIGURATION(CreateUniqueIds);
    DECLARE_CONFIGURATION(VersionDeviceXml);
    DECLARE_CONFIGURATION(EnableSourceDeviceModels);
    DECLARE_CONFIGURATION(WorkerThreads);
    DECLARE_CONFIGURATION(Validation);
    DECLARE_CONFIGURATION(CorrectTimestamps);
    ///@}

    /// @name MQTT Configuration
    ///@{
    DECLARE_CONFIGURATION(ProbeTopic);
    DECLARE_CONFIGURATION(DeviceTopic);
    DECLARE_CONFIGURATION(AssetTopic);
    DECLARE_CONFIGURATION(ObservationTopic);
    DECLARE_CONFIGURATION(CurrentTopic);
    DECLARE_CONFIGURATION(SampleTopic);
    DECLARE_CONFIGURATION(MqttCurrentInterval);
    DECLARE_CONFIGURATION(MqttSampleInterval);
    DECLARE_CONFIGURATION(MqttSampleCount);
    DECLARE_CONFIGURATION(MqttCaCert);
    DECLARE_CONFIGURATION(MqttCert);
    DECLARE_CONFIGURATION(MqttPrivateKey);
    DECLARE_CONFIGURATION(MqttClientId);
    DECLARE_CONFIGURATION(MqttTls);
    DECLARE_CONFIGURATION(MqttPort);
    DECLARE_CONFIGURATION(MqttHost);
    DECLARE_CONFIGURATION(MqttRetain);
    DECLARE_CONFIGURATION(MqttQOS);
    DECLARE_CONFIGURATION(MqttWs);
    DECLARE_CONFIGURATION(MqttConnectInterval);
    DECLARE_CONFIGURATION(MqttUserName);
    DECLARE_CONFIGURATION(MqttPassword);
    DECLARE_CONFIGURATION(MqttMaxTopicDepth);
    DECLARE_CONFIGURATION(MqttLastWillTopic);
    DECLARE_CONFIGURATION(MqttXPath);
    ///@}

    /// @name Adapter Configuration
    ///@{
    DECLARE_CONFIGURATION(AdapterIdentity);
    DECLARE_CONFIGURATION(AdditionalDevices);
    DECLARE_CONFIGURATION(AutoAvailable);
    DECLARE_CONFIGURATION(ConversionRequired);
    DECLARE_CONFIGURATION(Count);
    DECLARE_CONFIGURATION(Device);
    DECLARE_CONFIGURATION(FilterDuplicates);
    DECLARE_CONFIGURATION(Heartbeat);
    DECLARE_CONFIGURATION(Host);
    DECLARE_CONFIGURATION(IgnoreTimestamps);
    DECLARE_CONFIGURATION(LegacyTimeout);
    DECLARE_CONFIGURATION(Manufacturer);
    DECLARE_CONFIGURATION(Path);
    DECLARE_CONFIGURATION(PollingInterval);
    DECLARE_CONFIGURATION(PreserveUUID);
    DECLARE_CONFIGURATION(Protocol);
    DECLARE_CONFIGURATION(RealTime);
    DECLARE_CONFIGURATION(ReconnectInterval);
    DECLARE_CONFIGURATION(RelativeTime);
    DECLARE_CONFIGURATION(SerialNumber);
    DECLARE_CONFIGURATION(ShdrVersion);
    DECLARE_CONFIGURATION(SourceDevice);
    DECLARE_CONFIGURATION(Station);
    DECLARE_CONFIGURATION(SuppressIPAddress);
    DECLARE_CONFIGURATION(Topics);
    DECLARE_CONFIGURATION(UUID);
    DECLARE_CONFIGURATION(Uuid);
    DECLARE_CONFIGURATION(UpcaseDataItemValue);
    DECLARE_CONFIGURATION(Url);
    DECLARE_CONFIGURATION(UsePolling);
    ///@}

  }  // namespace configuration
}  // namespace mtconnect
