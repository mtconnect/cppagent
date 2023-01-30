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

#include "source/adapter/adapter.hpp"
#include "source/adapter/adapter_pipeline.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::configuration;

namespace mtconnect {
  namespace mqtt_client {

    class MqttTopicPermission
    {
      enum AuthorizationType
      {
        Allow,
        Deny
      };

      enum TopicMode
      {
        Subscribe,
        Publish,
        Both
      };

      void MqttTopicPermission(const std::string& topic, const std::string& clientId)
      {
        m_topic = topic;
        m_clientId = clientId;
        m_type = AuthorizationType::Allow;
        m_mode = TopicMode::Subscribe;
      }

      void MqttTopicPermission(const std::string& topic, const std::string& clientId,
                               AuthorizationType type)
      {
        m_topic = topic;
        m_clientId = clientId;
        m_type = type;
        m_mode = TopicMode::Subscribe;
      }

      void MqttTopicPermission(const std::string& topic, const std::string& clientId,
                               AuthorizationType type, TopicMode mode)
      {
        m_topic = topic;
        m_clientId = clientId;
        m_type = type;
        m_mode = mode;
      }

    protected:
      TopicMode m_mode;
      AuthorizationType m_type;
      std::string m_clientId;
      std::string m_topic;
    };

    class MqttAuthorization : public std::enable_shared_from_this<MqttAuthorization>
    {
    public:
      MqttAuthorization(const ConfigOptions& options) : m_options(options)
      {
        m_clientId = GetOption<std::string>(options, configuration::MqttClientId);
        m_username = GetOption<std::string>(options, configuration::MqttUserName);
        m_password = GetOption<std::string>(options, configuration::MqttPassword);
      }

      virtual ~MqttAuthorization() = default;

      bool checkCredentials()
      {
        if (!m_username && !m_password)
        {
          LOG(error) << "MQTT USERNAME_OR_PASSWORD are Not Available";
          return false;
        }

        return true;
      }

    protected:
      std::optional<std::string> m_username;
      std::optional<std::string> m_password;
      std::string m_clientId;
      ConfigOptions m_options;
    };
  }  // namespace mqtt_client
}  // namespace mtconnect
