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

#include <strstream>

#include "mtconnect/entity/entity.hpp"
#include "mtconnect/entity/factory.hpp"
#include <boost/beast/http/status.hpp>
#include "mtconnect/printer/printer.hpp"

namespace mtconnect::sink::rest_sink {
  using status = boost::beast::http::status;

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
    
    void setURI(const std::string &uri) { setProperty("URI", uri); }
    void setRequest(const std::string &request) { setProperty("Request", request); }
    void setErrorMessage(const std::string &message) { setProperty("ErrorMessage", message); }
    
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
    
    static entity::EntityPtr make(const ErrorCode code,
                                  std::optional<std::string> errorMessage = std::nullopt,
                                  std::optional<std::string> request = std::nullopt)
    {
      using namespace std;

      entity::Properties props {{"errorCode", enumForCode(code)}};
      if (errorMessage)
        props["ErrorMessage"] = *errorMessage;
      if (request)
        props["Request"] = *request;

      return std::make_shared<Error>(nameForCode(code), props);
    }

    entity::EntityPtr makeLegacyError() const
    {
      return std::make_shared<Error>("Error",
                                     entity::Properties {{"errorCode", getProperty("errorCode")},
                                                         {"VALUE", getProperty("ErrorMessage")}});
    }
  };

  using ErrorPtr = std::shared_ptr<Error>;
  
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

    static entity::EntityPtr make(const std::string &name,
                                  const std::string &value, const std::string &type,
                                  const std::string &format,
                                  std::optional<std::string> errorMessage  = std::nullopt,
                                  std::optional<std::string> request = std::nullopt)
    {
      using namespace std;

      entity::Properties props {
          {"errorCode", enumForCode(Error::ErrorCode::INVALID_PARAMTER_VALUE)}};
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

    static entity::EntityPtr make(const std::string &name, int64_t value,
                                  int64_t min, int64_t max, std::optional<std::string> errorMessage  = std::nullopt,
                                  std::optional<std::string> request = std::nullopt)
    {
      using namespace std;

      entity::Properties props {{"errorCode", enumForCode(Error::ErrorCode::OUT_OF_RANGE)}};
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

    static entity::EntityPtr make(const std::string &assetId,
                                  std::optional<std::string> errorMessage  = std::nullopt,
                                  std::optional<std::string> request  = std::nullopt)
    {
      using namespace std;

      entity::Properties props {{"errorCode", enumForCode(Error::ErrorCode::ASSET_NOT_FOUND)},
                                {"AssetId", assetId}};
      if (errorMessage)
        props["ErrorMessage"] = *errorMessage;
      if (request)
        props["Request"] = *request;

      return std::make_shared<AssetNotFound>(props);
    }
  };
  
  class AGENT_LIB_API RestError
  {
  public:
    /// @brief An exception that gets thrown during REST processing with an error and a status
    /// @param error the error entity
    /// @param accepts the accepted mime types, defaults to application/xml
    /// @param status the HTTP status code, defaults to 400 Bad Request
    /// @param format optional format for the error
    RestError(entity::EntityPtr error,
              std::string accepts = "application/xml",
              status st = status::bad_request,
              std::optional<std::string> format = std::nullopt,
              std::optional<std::string> request = std::nullopt) :  m_errors({error}), m_accepts(accepts), m_status(st), m_format(format)
    {
      if (request)
        setRequest(*request);
    }
    
    /// @brief An exception that gets thrown during REST processing with an error and a status
    /// @param errors a list of errors
    /// @param accepts the accepted mime types, defaults to application/xml
    /// @param status the HTTP status code, defaults to 400 Bad Request
    /// @param format optional format for the error
    RestError(entity::EntityList &errors,
              std::string accepts = "application/xml",
              status st = status::bad_request,
              std::optional<std::string> format = std::nullopt,
              std::optional<std::string> request = std::nullopt) : m_errors(errors),
              m_accepts(accepts), m_status(st), m_format(format)
    {
      if (request)
        setRequest(*request);
    }

    /// @brief An exception that gets thrown during REST processing with an error and a status
    /// @param error the error entity
    /// @param printer the printer to generate the error document
    /// @param status the HTTP status code, defaults to 400 Bad Request
    /// @param format optional format for the error
    RestError(entity::EntityPtr error,
              const printer::Printer* printer = nullptr,
              status st = status::bad_request,
              std::optional<std::string> format = std::nullopt,
              std::optional<std::string> request = std::nullopt) :
    m_errors({error}), m_status(st), m_format(format), m_printer(printer)
    {
      if (request)
        setRequest(*request);
    }
    
    /// @brief An exception that gets thrown during REST processing with an error and a status
    /// @param errors a list of errors
    /// @param printer the printer to generate the error document
    /// @param status the HTTP status code, defaults to 400 Bad Request
    /// @param format optional format for the error
    RestError(entity::EntityList &errors,
              const printer::Printer* printer = nullptr,
              status st = status::bad_request,
              std::optional<std::string> format = std::nullopt,
              std::optional<std::string> request = std::nullopt) : m_errors(errors),
    m_status(st), m_format(format), m_printer(printer)
    {
      if (request)
        setRequest(*request);
    }

    
    
    ~RestError() = default;
    RestError(const RestError &o) = default;
    
    /// @brief set the URI for all errors
    /// @param uri the URI
    void setUri(const std::string &uri)
    {
      for (auto &e : m_errors)
        e->setProperty("URI", uri);
    }
    
    /// @brief set the Request for all errors
    /// @param request the Request
    void setRequest(const std::string &request)
    {
      m_request = request;
      for (auto &e : m_errors)
        e->setProperty("Request", request);
    }

    const auto &getErrors() const { return m_errors; }
    void setStatus(status st) { m_status = st; }
    const auto &getStatus() const { return m_status; }
    void setFormat(const std::string &format) { m_format = format; }
    const auto &getFormat() const { return m_format; }
    const auto &getAccepts() const { return m_accepts; }
    const auto &getRequest() const { return m_request; }
    auto getPrinter() const { return m_printer; }
    
    std::string what() const
    {
      std::stringstream ss;
      for (const auto &e : m_errors)
      {
        ss << e->getName() << ": ";
        auto message = e->maybeGet<std::string>("ErrorMessage");
        if (message)
          ss << *message;
        ss << ", ";
      }
      
      return ss.str();
    }

  protected:
    entity::EntityList m_errors;
    std::string m_accepts { "application/xml" };
    status m_status;
    std::optional<std::string> m_format;
    std::optional<std::string> m_request;
    const printer::Printer* m_printer { nullptr };
  };

}  // namespace mtconnect::sink::rest_sink
