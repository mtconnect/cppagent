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

#include "file_cache.hpp"
#include "response.hpp"
#include "routing.hpp"
#include "config_options.hpp"
#include <dlib/server.h>

namespace mtconnect
{
  namespace http_server
  {
    using IncomingThings = dlib::incoming_things;

    class RequestError : public std::logic_error
    {
    public:
      RequestError(const char *w) : std::logic_error::logic_error(w) {}
      RequestError(const char *w, const std::string &body, const std::string &type,
                   ResponseCode code)
        : std::logic_error::logic_error(w), m_contentType(type), m_body(body), m_code(code)
      {
      }
      RequestError(const RequestError &) = default;
      ~RequestError() override = default;

      std::string m_contentType;
      std::string m_body;
      ResponseCode m_code;
    };

    using ErrorFunction = std::function<void(const std::string &accepts, Response &response,
                                             const std::string &msg, const ResponseCode code)>;

    class Server : public dlib::server_http
    {
    public:
      Server(unsigned short port = 5000, const std::string &inter = "0.0.0.0",
             const ConfigOptions &options = {})
      {
        set_listening_port(port);
        set_listening_ip(inter);
        const auto fields = GetOption<StringList>(options, configuration::HttpHeaders);
        if (fields)
          m_fields = *fields;

        m_errorFunction = [](const std::string &accepts, Response &response, const std::string &msg,
                             const ResponseCode code) {
          response.writeResponse(msg, code);
          return true;
        };
      }

      // Overridden method that is called per web request – not used
      // using httpRequest which is called from our own on_connect method.
      const std::string on_request(const dlib::incoming_things &incoming,
                                   dlib::outgoing_things &outgoing) override
      {
        throw std::logic_error("Not Implemented");
        return "";
      }

      // Start the http server
      void start();

      // Shutdown
      void stop() { server::http_1a::clear(); }
      
      // Additional header fields
      void setHttpHeaders(const StringList &fields)
      {
        m_fields = fields;
      }
      const auto &getHttpHeaders() const { return m_fields; }

      // PUT and POST handling
      void enablePut(bool flag = true) { m_putEnabled = flag; }
      bool isPutEnabled() const { return m_putEnabled; }
      void allowPutFrom(const std::string &host) { m_putAllowedHosts.insert(host); }
      bool isPutAllowedFrom(const std::string &host) const
      {
        return m_putAllowedHosts.find(host) != m_putAllowedHosts.end();
      }

      bool dispatch(Routing::Request &request, Response &response)
      {
        for (auto &r : m_routings)
        {
          if (r.matches(request, response))
            return true;
        }

        return false;
      }

      bool handleRequest(Routing::Request &request, Response &response);

      void addRouting(const Routing &routing) { m_routings.emplace_back(routing); }

      void setErrorFunction(const ErrorFunction &func) { m_errorFunction = func; }

    protected:
      // HTTP Protocol
      void on_connect(std::istream &in, std::ostream &out, const std::string &foreign_ip,
                      const std::string &local_ip, unsigned short foreign_port,
                      unsigned short local_port, dlib::uint64) override;

    protected:
      // Put handling controls
      bool m_putEnabled;
      std::set<std::string> m_putAllowedHosts;
      std::list<Routing> m_routings;
      std::unique_ptr<FileCache> m_fileCache;
      ErrorFunction m_errorFunction;
      StringList m_fields;
    };
  }  // namespace http_server
}  // namespace mtconnect
