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

#include "mtconnect/entity/entity.hpp"
#include "mtconnect/entity/factory.hpp"

namespace mtconnect::sink::rest_sink {

  class AGENT_LIB_API Error : public mtconnect::entity::Entity
  {
  public:
    enum class ErrorCode
    {
      ASSET_NOT_FOUND,
      INTERNAL_ERROR,
      INVALID_REQUEST,
      INVALID_URI,
      INVALID_XPATH,
      NO_DEVICE,
      OUT_OF_RANGE,
      QUERY_ERROR,
      TOO_MANY,
      UNAUTHORIZED,
      UNSUPPORTED,
      INVALID_PARAMTER_VALUE,
      INVALID_QUERY_PARAMETER
    };

    Error(const std::string &name, const entity::Properties &props) : entity::Entity(name, props) {}
    ~Error() override = default;

    /// @brief get the static error factory
    /// @return shared pointer to the factory
    static entity::FactoryPtr getFactory()
    {
      static auto error = std::make_shared<entity::Factory>(
          entity::Requirements {
              {"errorCode", false}, {"URI", false}, {"Request", false}, {"ErrorMessage", false}},
          [](const std::string &name, entity::Properties &props) -> entity::EntityPtr {
            return std::make_shared<Error>(name, props);
          });

      return error;
    }

    static const std::string nameForCode(ErrorCode code);
    static const std::string enumForCode(ErrorCode code);

    static entity::EntityPtr make(const ErrorCode code, const std::string &uri,
                                  std::optional<std::string> errorMessage,
                                  std::optional<std::string> request)
    {
      using namespace std;

      entity::Properties props {{"errorCode", enumForCode(code)}, {"URI", uri}};
      if (errorMessage)
        props["ErrorMessage"] = *errorMessage;
      if (request)
        props["Request"] = *request;

      return std::make_shared<Error>(nameForCode(code), props);
    }

    entity::EntityPtr makeLegacy()
    {
      return std::make_shared<Error>("Error",
                                     entity::Properties {{"errorCode", getProperty_("errorCode")},
                                                         {"VALUE", getProperty_("ErrorMessage")}});
    }
  };

  class AGENT_LIB_API QueryParameter : public entity::Entity
  {
  public:
    QueryParameter(const entity::Properties &props) : entity::Entity("QueryParameter", props) {}
    QueryParameter(const std::string &name, const entity::Properties &props)
      : entity::Entity(name, props)
    {}

    /// @brief get the static error factory
    /// @return shared pointer to the factory
    static entity::FactoryPtr getFactory()
    {
      static auto qp = std::make_shared<entity::Factory>(
          entity::Requirements {{"name", true},
                                {"Value", false},
                                {"Type", false},
                                {"Format", false},
                                {"Minimum", entity::ValueType::INTEGER, false},
                                {"Maximum", entity::ValueType::INTEGER, false}},
          [](const std::string &name, entity::Properties &props) -> entity::EntityPtr {
            return std::make_shared<QueryParameter>(name, props);
          });

      return qp;
    }

    static entity::EntityPtr make(const entity::Properties &properties)
    {
      return std::make_shared<QueryParameter>(properties);
    }
  };

  class AGENT_LIB_API InvalidParameterValue : public Error
  {
  public:
    InvalidParameterValue(const entity::Properties &props) : Error("InvalidParameterValue", props)
    {}
    InvalidParameterValue(const std::string &name, const entity::Properties &props)
      : Error(name, props)
    {}
    ~InvalidParameterValue() override = default;

    /// @brief get the static error factory
    /// @return shared pointer to the factory
    static entity::FactoryPtr getFactory()
    {
      static entity::FactoryPtr factory;
      if (!factory)
      {
        factory = std::make_shared<entity::Factory>(*Error::getFactory());
        factory->addRequirements(
            entity::Requirements {{"InvalidParameterValue", entity::ValueType::ENTITY,
                                   QueryParameter::getFactory(), true}});
        factory->setFunction(
            [](const std::string &name, entity::Properties &props) -> entity::EntityPtr {
              return std::make_shared<InvalidParameterValue>(name, props);
            });
      }
      return factory;
    }

    static entity::EntityPtr make(const std::string &uri, const std::string &name,
                                  const std::string &value, const std::string &type,
                                  const std::string &format,
                                  std::optional<std::string> errorMessage,
                                  std::optional<std::string> request = std::nullopt)
    {
      using namespace std;

      entity::Properties props {
          {"errorCode", enumForCode(Error::ErrorCode::INVALID_PARAMTER_VALUE)}, {"URI", uri}};
      if (errorMessage)
        props["ErrorMessage"] = *errorMessage;
      if (request)
        props["Request"] = *request;

      auto qp = std::make_shared<QueryParameter>(entity::Properties {
          {"name", name}, {"Type", type}, {"Format", format}, {"Value", value}});
      props["QueryParameter"] = qp;

      return std::make_shared<InvalidParameterValue>(props);
    }
  };

  class AGENT_LIB_API OutOfRange : public Error
  {
  public:
    OutOfRange(const entity::Properties &props) : Error("OutOfRange", props) {}
    OutOfRange(const std::string &name, const entity::Properties &props) : Error(name, props) {}
    ~OutOfRange() override = default;

    /// @brief get the static error factory
    /// @return shared pointer to the factory
    static entity::FactoryPtr getFactory()
    {
      static entity::FactoryPtr factory;
      if (!factory)
      {
        factory = std::make_shared<entity::Factory>(*Error::getFactory());
        factory->addRequirements(entity::Requirements {
            {"QueryParameters", entity::ValueType::ENTITY, QueryParameter::getFactory(), true}});
        factory->setFunction(
            [](const std::string &name, entity::Properties &props) -> entity::EntityPtr {
              return std::make_shared<OutOfRange>(name, props);
            });
      }
      return factory;
    }

    static entity::EntityPtr make(const std::string &uri, const std::string &name, int64_t value,
                                  int64_t min, int64_t max, std::optional<std::string> errorMessage,
                                  std::optional<std::string> request)
    {
      using namespace std;

      entity::Properties props {{"errorCode", enumForCode(Error::ErrorCode::OUT_OF_RANGE)},
                                {"URI", uri}};
      if (errorMessage)
        props["ErrorMessage"] = *errorMessage;
      if (request)
        props["Request"] = *request;

      auto qp = std::make_shared<QueryParameter>(entity::Properties {
          {"name", name}, {"Minimum", min}, {"Maximum", max}, {"Value", value}});
      props["QueryParameter"] = qp;

      return std::make_shared<OutOfRange>(props);
    }
  };

  class AGENT_LIB_API AssetNotFound : public Error
  {
  public:
    AssetNotFound(const entity::Properties &props) : Error("AssetNotFound", props) {}
    AssetNotFound(const std::string &name, const entity::Properties &props) : Error(name, props) {}
    ~AssetNotFound() override = default;

    /// @brief get the static error factory
    /// @return shared pointer to the factory
    static entity::FactoryPtr getFactory()
    {
      static entity::FactoryPtr factory;
      if (!factory)
      {
        factory = std::make_shared<entity::Factory>(*Error::getFactory());
        factory->addRequirements(entity::Requirements {{"AssetId", true}});
        factory->setFunction(
            [](const std::string &name, entity::Properties &props) -> entity::EntityPtr {
              return std::make_shared<AssetNotFound>(name, props);
            });
      }
      return factory;
    }

    static entity::EntityPtr make(const std::string &uri, const std::string &assetId,
                                  std::optional<std::string> errorMessage,
                                  std::optional<std::string> request)
    {
      using namespace std;

      entity::Properties props {{"errorCode", enumForCode(Error::ErrorCode::ASSET_NOT_FOUND)},
                                {"URI", uri},
                                {"AssetId", assetId}};
      if (errorMessage)
        props["ErrorMessage"] = *errorMessage;
      if (request)
        props["Request"] = *request;

      return std::make_shared<AssetNotFound>(props);
    }
  };
  
  class AGENT_LIB_API RestError : public std::logic_error
  {
  public:
    RestError(entity::EntityPtr error) : std::logic_error("REST Error"), m_error(error) {}
    RestError(const char *what, entity::EntityPtr error = nullptr) : std::logic_error(what), m_error(error) {}
    RestError(const std::string &what, entity::EntityPtr error = nullptr) : std::logic_error(what), m_error(error) {}

    auto getError() const { return m_error; }
    void setError(entity::EntityPtr error) { m_error = error; }
    
  protected:
    entity::EntityPtr m_error;
  };

}  // namespace mtconnect::sink::rest_sink
