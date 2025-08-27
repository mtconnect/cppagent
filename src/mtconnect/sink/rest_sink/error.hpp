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

#include <boost/beast/http/status.hpp>

#include <strstream>

#include "mtconnect/entity/entity.hpp"
#include "mtconnect/entity/factory.hpp"
#include "mtconnect/printer/printer.hpp"

namespace mtconnect::sink::rest_sink {
  using status = boost::beast::http::status;

  /// @brief An Error entity for error reporting
  /// Builds an Error entity with errorCode, URI, Request, and ErrorMessage.
  /// Subclasses can add additional information
  class AGENT_LIB_API Error : public mtconnect::entity::Entity
  {
  public:
    /// @brief Error codes for MTConnect REST API
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
      INVALID_PARAMETER_VALUE,
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

    /// @brief Get the name for an error code
    /// @param code the error code
    static const std::string nameForCode(ErrorCode code);
    /// @brief Get the controlled vocabulary enumeration string for an error code
    /// @param code the error code
    static const std::string enumForCode(ErrorCode code);

    /// @brief static error factory method
    /// @param code the error code
    /// @param errorMessage optional error message
    /// @param request optional request string
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

  /// @brief A QueryParameter entity for error reporting
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

    /// @brief static factory method
    /// @param properties the properties for the QueryParameter
    static entity::EntityPtr make(const entity::Properties &properties)
    {
      return std::make_shared<QueryParameter>(properties);
    }
  };

  /// @brief An InvalidParameterValue error
  /// Builds a QueryParameter with name, value, type, and format
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

    /// @brief static factory method
    /// @param name the name of the parameter
    /// @param value the value of the parameter
    /// @param type the type of the parameter
    /// @param format the format of the parameter
    /// @param errorMessage optional error message
    /// @param request optional request string
    static entity::EntityPtr make(const std::string &name, const std::string &value,
                                  const std::string &type, const std::string &format,
                                  std::optional<std::string> errorMessage = std::nullopt,
                                  std::optional<std::string> request = std::nullopt)
    {
      using namespace std;

      entity::Properties props {
          {"errorCode", enumForCode(Error::ErrorCode::INVALID_PARAMETER_VALUE)}};
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

  /// @brief An OutOfRange error
  /// Builds a QueryParameter with name, value, min, and max
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

    /// @brief static factory method
    /// @param name the name of the parameter
    /// @param value the value of the parameter
    /// @param min the minimum value of the parameter
    /// @param max the maximum value of the parameter
    /// @param errorMessage optional error message
    /// @param request optional request string
    static entity::EntityPtr make(const std::string &name, int64_t value, int64_t min, int64_t max,
                                  std::optional<std::string> errorMessage = std::nullopt,
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

  /// @brief An AssetNotFound error
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

    /// @brief static factory method
    /// @param assetId the asset ID that was not found
    /// @param errorMessage optional error message
    /// @param request optional request string
    static entity::EntityPtr make(const std::string &assetId,
                                  std::optional<std::string> errorMessage = std::nullopt,
                                  std::optional<std::string> request = std::nullopt)
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

  /// @brief An exception that gets thrown during REST processing with an error and a status
  class AGENT_LIB_API RestError
  {
  public:
    /// @brief An exception that gets thrown during REST processing with an error and a status
    /// @param error the error entity
    /// @param accepts the accepted mime types, defaults to application/xml
    /// @param status the HTTP status code, defaults to 400 Bad Request
    /// @param format optional format for the error
    RestError(entity::EntityPtr error, std::string accepts = "application/xml",
              status st = status::bad_request, std::optional<std::string> format = std::nullopt,
              std::optional<std::string> requestId = std::nullopt)
      : m_errors({error}),
        m_accepts(accepts),
        m_status(st),
        m_format(format),
        m_requestId(requestId)
    {}

    /// @brief An exception that gets thrown during REST processing with an error and a status
    /// @param errors a list of errors
    /// @param accepts the accepted mime types, defaults to application/xml
    /// @param status the HTTP status code, defaults to 400 Bad Request
    /// @param format optional format for the error
    RestError(entity::EntityList &errors, std::string accepts = "application/xml",
              status st = status::bad_request, std::optional<std::string> format = std::nullopt,
              std::optional<std::string> requestId = std::nullopt)
      : m_errors(errors), m_accepts(accepts), m_status(st), m_format(format), m_requestId(requestId)
    {}

    /// @brief An exception that gets thrown during REST processing with an error and a status
    /// @param error the error entity
    /// @param printer the printer to generate the error document
    /// @param status the HTTP status code, defaults to 400 Bad Request
    /// @param format optional format for the error
    RestError(entity::EntityPtr error, const printer::Printer *printer = nullptr,
              status st = status::bad_request, std::optional<std::string> format = std::nullopt,
              std::optional<std::string> requestId = std::nullopt)
      : m_errors({error}),
        m_status(st),
        m_format(format),
        m_requestId(requestId),
        m_printer(printer)

    {}

    /// @brief An exception that gets thrown during REST processing with an error and a status
    /// @param errors a list of errors
    /// @param printer the printer to generate the error document
    /// @param status the HTTP status code, defaults to 400 Bad Request
    /// @param format optional format for the error
    RestError(entity::EntityList &errors, const printer::Printer *printer = nullptr,
              status st = status::bad_request, std::optional<std::string> format = std::nullopt,
              std::optional<std::string> requestId = std::nullopt)
      : m_errors(errors), m_status(st), m_format(format), m_requestId(requestId), m_printer(printer)
    {}

    ~RestError() = default;
    RestError(const RestError &o) = default;

    /// @brief set the URI for all errors
    /// @param uri the URI
    void setUri(const std::string &uri)
    {
      for (auto &e : m_errors)
        e->setProperty("URI", uri);
    }

    /// @brief set the Request ID for the websocket responses
    /// @param requestId the Request ID
    void setRequestId(const std::string &requestId) { m_requestId = requestId; }
    const auto &getRequestId() const { return m_requestId; }

    /// @brief The response document type for the request (e.g. MTConnectDevices)
    /// @param request the Request
    void setRequest(const std::string &request)
    {
      for (auto &e : m_errors)
        e->setProperty("Request", request);
    }

    const auto &getErrors() const { return m_errors; }

    void setStatus(status st) { m_status = st; }
    const auto &getStatus() const { return m_status; }

    void setFormat(const std::string &format) { m_format = format; }
    const auto &getFormat() const { return m_format; }

    const auto &getAccepts() const { return m_accepts; }

    auto getPrinter() const { return m_printer; }

    /// @brief Get a string representation of the error(s) concating the error messages.
    /// @return a string representation of the error(s)
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
      auto s = ss.str();
      s.erase(s.length() - 2);
      return s;
    }

  protected:
    entity::EntityList m_errors;                ///< The list of errors to be reported
    std::string m_accepts {"application/xml"};  ///< The accepted mime types for the response
    status m_status;                            ///< The HTTP status code
    std::optional<std::string> m_format;  ///< The format for the error response overriding accepts
    std::optional<std::string> m_requestId;  ///< The request id for the response
    const printer::Printer *m_printer {
        nullptr};  ///< The printer to use for the response. If the printer is not specified it will
                   ///< be inferred from the accepts or format.
  };

}  // namespace mtconnect::sink::rest_sink
