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

#include "mtconnect/configuration/config_options.hpp"
#include "mtconnect/logging.hpp"
#include "mtconnect/utilities.hpp"

using namespace std;
using namespace mtconnect;

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

    public:
      MqttTopicPermission(const std::string& topic)
      {
        m_topic = topic;
        m_type = AuthorizationType::Allow;
        m_mode = TopicMode::Subscribe;
      }

      MqttTopicPermission(const std::string& topic, AuthorizationType type)
      {
        m_topic = topic;
        m_type = type;
        m_mode = TopicMode::Subscribe;
      }

      MqttTopicPermission(const std::string& topic, AuthorizationType type, TopicMode mode)
      {
        m_topic = topic;
        m_type = type;
        m_mode = mode;
      }

      bool hasAuthorization()
      {
        if (m_type == AuthorizationType::Allow)
          return true;

        // AuthorizationType::Deny
        return false;
      }

    protected:
      TopicMode m_mode;
      AuthorizationType m_type;
      std::string m_topic;

    };  // namespace MqttTopicPermission

    class MqttAuthorization
    {
    public:
      MqttAuthorization(const ConfigOptions& options) : m_options(options)
      {
        m_clientId = *GetOption<std::string>(options, configuration::MqttClientId);
        m_username = GetOption<std::string>(options, configuration::MqttUserName);
        m_password = GetOption<std::string>(options, configuration::MqttPassword);
      }

      virtual ~MqttAuthorization() = default;

      MqttTopicPermission getPermissionsForClient(const std::string& topic)
      {
        MqttTopicPermission mqttTopicPerm = *new MqttTopicPermission(topic);
        return mqttTopicPerm;
      }

      list<MqttTopicPermission> getPermissionsForClient(const std::list<std::string>& topics)
      {
        list<MqttTopicPermission> mqttTopicPermissions;

        for (auto& topic : topics)
        {
          mqttTopicPermissions.push_back(*new MqttTopicPermission(topic));
        }

        return mqttTopicPermissions;
      }

    protected:
      std::optional<std::string> m_username;
      std::optional<std::string> m_password;
      std::string m_clientId;
      ConfigOptions m_options;

    };  // namespace MqttAuthorization

    class MqttAuthentication
    {
    public:
      MqttAuthentication(const ConfigOptions& options) : m_options(options)
      {
        m_clientId = *GetOption<std::string>(options, configuration::MqttClientId);
        m_username = GetOption<std::string>(options, configuration::MqttUserName);
        m_password = GetOption<std::string>(options, configuration::MqttPassword);
      }

      virtual ~MqttAuthentication() = default;

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

    };  // namespace MqttAuthentication

  }  // namespace mqtt_client
}  // namespace mtconnect
