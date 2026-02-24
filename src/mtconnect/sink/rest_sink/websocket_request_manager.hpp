
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

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <functional>
#include <optional>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <sstream>

#include "session.hpp"

namespace mtconnect::sink::rest_sink {
  class WebsocketRequestManager
  {
  public:
    /// @brief Wrapper around a request with additional infomation required for a WebSocket request
    struct WebsocketRequest
    {
      WebsocketRequest(const std::string &id) : m_requestId(id) {}
      std::string m_requestId;                               //! The id of the request
      std::optional<boost::asio::streambuf> m_streamBuffer;  //! The streambuffer used in responses
      Complete m_complete;       //! A complete function when the request has finished
      bool m_streaming {false};  //! A flag to indicate the request is a streaming request
      RequestPtr m_request;      //! A pointer to the underlying incoming request
    };

    /// @brief Create a request dispatcher
    /// @param httpRequest a copy of the incoming HTTP request
    /// @param dispatch the dispatch function to call
    WebsocketRequestManager(RequestPtr &&httpRequest, Dispatch dispatch)
      : m_httpRequest(std::move(httpRequest)), m_dispatch(dispatch)
    {}

    /// @brief Destroy the request manager
    ~WebsocketRequestManager() {}

    /// @brief clear the request and the set of requests
    void reset()
    {
      m_httpRequest.reset();
      m_requests.clear();
    }

    /// @brief Set the current request (used for testing).
    /// @param request the request that is owned by the manager
    void setHttpRequest(RequestPtr &&request) { m_httpRequest = std::move(request); }

    /// @brief Get the current HTTP request
    /// @returns a pointer to the HTTP request
    RequestPtr getHttpRequest() const { return m_httpRequest; }

    /// @brief Finds the request for a given id
    /// @param id the id to search for
    /// @returns a pointer to the request structure or null if it is not found
    WebsocketRequest *findRequest(const std::string &id)
    {
      auto it = m_requests.find(id);
      if (it != m_requests.end())
      {
        return &(it->second);
      }
      else
      {
        return nullptr;
      }
    }

    /// @brief finds or creates a WebSocketRequest structure and return it.
    /// @param id the id of the request to create
    /// @returns a pointer to the new websocket request or the existing one.
    WebsocketRequest *findOrCreateRequest(const std::string &id)
    {
      auto res = m_requests.emplace(id, id);
      return &res.first->second;
    }

    /// @brief finds or creates a WebSocketRequest structure and return it.
    /// @param id the id of the request to create
    /// @returns a pointer to the new websocket request or the existing one.
    WebsocketRequest *createRequest(const std::string &id)
    {
      auto it = m_requests.find(id);
      if (it == m_requests.end())
      {
        auto res = m_requests.emplace(id, id);
        return &res.first->second;
      }
      else
      {
        return nullptr;
      }
    }

    /// @brief Remove a request from the known requests
    /// @param id the id of the request to remove
    void remove(const std::string &id) { m_requests.erase(id); }

    /// @brief Parse a JSON request buffer and create a new request ptr.
    /// @param buffer the text to parse
    RequestPtr parse(const std::string &buffer)
    {
      using namespace rapidjson;
      using namespace std;

      Document doc;
      doc.Parse(buffer.c_str());

      RequestPtr request;

      if (doc.HasParseError())
      {
        stringstream err;
        err << "Websocket Read Error(offset (" << doc.GetErrorOffset()
            << ")): " << GetParseError_En(doc.GetParseError());
        LOG(warning) << err.str();
        LOG(warning) << "  " << buffer;
        auto error = Error::make(Error::ErrorCode::INVALID_REQUEST, err.str());
        throw RestError(error, m_httpRequest->m_accepts, rest_sink::status::bad_request,
                        std::nullopt, "ERROR");
      }
      if (!doc.IsObject())
      {
        LOG(warning) << "Websocket Read Error: JSON message does not have a top level object";
        LOG(warning) << "  " << buffer;
        auto error = Error::make(Error::ErrorCode::INVALID_REQUEST,
                                 "JSON message does not have a top level object");
        throw RestError(error, m_httpRequest->m_accepts, rest_sink::status::bad_request,
                        std::nullopt, "ERROR");
      }
      else
      {
        // Extract the parameters from the json doc to map them to the REST
        // protocol parameters
        request = make_unique<Request>(*m_httpRequest);

        request->m_verb = boost::beast::http::verb::get;
        request->m_parameters.clear();

#ifdef GetObject
#define __GOSave__ GetObject
#undef GetObject
#endif
        const auto &object = doc.GetObject();
#ifdef __GOSave__
#define GetObject __GOSave__
#endif

        for (auto &it : object)
        {
          switch (it.value.GetType())
          {
            case rapidjson::kNullType:
              // Skip nulls
              break;
            case rapidjson::kFalseType:
              request->m_parameters.emplace(
                  make_pair(string(it.name.GetString()), ParameterValue(false)));
              break;
            case rapidjson::kTrueType:
              request->m_parameters.emplace(
                  make_pair(string(it.name.GetString()), ParameterValue(true)));
              break;
            case rapidjson::kObjectType:
              break;
            case rapidjson::kArrayType:
            {
              const auto &array = it.value.GetArray();
              std::stringstream buf;
              for (const auto &s : array)
                buf << s.GetString() << ";";
              string str = buf.str();
              str.erase(str.length() - 1);  // Remove last ;
              request->m_parameters.emplace(make_pair(it.name.GetString(), ParameterValue(str)));
              break;
            }
            case rapidjson::kStringType:
              request->m_parameters.emplace(
                  make_pair(it.name.GetString(), ParameterValue(string(it.value.GetString()))));
              break;
            case rapidjson::kNumberType:
              if (it.value.IsInt())
                request->m_parameters.emplace(
                    make_pair(it.name.GetString(), ParameterValue(it.value.GetInt())));
              else if (it.value.IsUint())
                request->m_parameters.emplace(
                    make_pair(it.name.GetString(), ParameterValue(uint64_t(it.value.GetUint()))));
              else if (it.value.IsInt64())
                request->m_parameters.emplace(
                    make_pair(it.name.GetString(), ParameterValue(uint64_t(it.value.GetInt64()))));
              else if (it.value.IsUint64())
                request->m_parameters.emplace(
                    make_pair(it.name.GetString(), ParameterValue(it.value.GetUint64())));
              else if (it.value.IsDouble())
                request->m_parameters.emplace(
                    make_pair(it.name.GetString(), ParameterValue(double(it.value.GetDouble()))));
              break;
          }
        }
      }

      return request;
    }

    /// @brief dispatch a JSON request buffer for a session
    /// @param session A shared pointer to the session that we are dispatching for
    /// @param buffer the JSON request string
    /// @param outId optional pointer to a string to receive the request id
    /// @returns `true` if the dispatch was successful.
    bool dispatch(SessionPtr session, const std::string &buffer, std::string *outId = nullptr)
    {
      using namespace std;

      auto request = parse(buffer);
      if (!request)
      {
        return false;
      }

      if (request->m_parameters.count("id") > 0)
      {
        auto &v = request->m_parameters["id"];
        string id = visit(overloaded {[](monostate m) { return ""s; },
                                      [](auto v) { return boost::lexical_cast<string>(v); }},
                          v);
        request->m_requestId = id;
        request->m_parameters.erase("id");
      }
      else
      {
        auto error = InvalidParameterValue::make("id", "", "string", "string", "No id given");
        throw RestError(error, request->m_accepts, rest_sink::status::bad_request, std::nullopt,
                        "ERROR");
      }

      auto &id = *(request->m_requestId);

      if (request->m_parameters.count("request") > 0)
      {
        request->m_command = get<string>(request->m_parameters["request"]);
        request->m_parameters.erase("request");
      }
      else
      {
        auto error =
            InvalidParameterValue::make("request", "", "string", "string", "No request given");
        throw RestError(error, request->m_accepts, rest_sink::status::bad_request, std::nullopt,
                        id);
      }

      if (outId)
        *outId = id;
      if (request->m_command != "cancel")
      {
        auto res = m_requests.emplace(id, id);
        if (!res.second)
        {
          LOG(error) << "Duplicate request id: " << id;
          auto error = InvalidParameterValue::make("id", *request->m_requestId, "string", "string",
                                                   "Duplicate id given");
          throw RestError(error, request->m_accepts, rest_sink::status::bad_request, std::nullopt,
                          "ERROR");
        }

        // Check parameters for command
        LOG(debug) << "Received request id: " << id;

        res.first->second.m_request = std::move(request);
        request = res.first->second.m_request;
      }
      else
      {
        LOG(debug) << "Cancel request id: " << id;
      }
      try
      {
        return m_dispatch(session, request);
      }

      catch (RestError &re)
      {
        re.setRequestId(id);
        throw re;
      }

      return false;
    }

  protected:
    RequestPtr m_httpRequest;                            //! A pointer to the original HTTP request
    Dispatch m_dispatch;                                 //! The dispatch function
    std::map<std::string, WebsocketRequest> m_requests;  //! The map of requests this class manages
  };
}  // namespace mtconnect::sink::rest_sink
