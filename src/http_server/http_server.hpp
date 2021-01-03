//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <dlib/server.h>

namespace mtconnect
{
  namespace http_server
  {
    struct OutgoingThings : public dlib::outgoing_things
    {
      OutgoingThings() = default;
      std::ostream *m_out { nullptr };
      std::string m_format { nullptr };
    };
    
    using IncomingThings = dlib::incoming_things;

    class HttpServer : public dlib::server_http
    {
      class ParameterError
      {
       public:
        ParameterError(const std::string &code, const std::string &message)
        {
          m_code = code;
          m_message = message;
        }

        ParameterError(const ParameterError &anotherError)
        {
          m_code = anotherError.m_code;
          m_message = anotherError.m_message;
        }

        ParameterError &operator=(const ParameterError &anotherError) = default;

        std::string m_code;
        std::string m_message;
      };

          // Overridden method that is called per web request – not used
      // using httpRequest which is called from our own on_connect method.
      const std::string on_request(const dlib::incoming_things &incoming,
                                   dlib::outgoing_things &outgoing) override
      {
        throw std::logic_error("Not Implemented");
        return "";
      }

      const std::string httpRequest(const IncomingThings &incoming,
                                    OutgoingThings &outgoing);

      // Shutdown
      void clear();

      void registerFile(const std::string &uri, const std::string &path);
      void addMimeType(const std::string &ext, const std::string &type) { m_mimeTypes[ext] = type; }

      // PUT and POST handling
      void enablePut(bool flag = true) { m_putEnabled = flag; }
      bool isPutEnabled() const { return m_putEnabled; }
      void allowPutFrom(const std::string &host) { m_putAllowedHosts.insert(host); }
      bool isPutAllowedFrom(const std::string &host) const
      {
        return m_putAllowedHosts.find(host) != m_putAllowedHosts.end();
      }

    protected:
      // HTTP Protocol
      void on_connect(std::istream &in, std::ostream &out, const std::string &foreign_ip,
                      const std::string &local_ip, unsigned short foreign_port,
                      unsigned short local_port, dlib::uint64) override;

      // Get a file
      std::string handleFile(const std::string &uri, OutgoingThings &outgoing);

      bool isFile(const std::string &uri) const { return m_fileCache->hasFile(uri); }

      // Perform a check on parameter and return a value or a code
      int checkAndGetParam(const dlib::key_value_map &queries, const std::string &param,
                           const int defaultValue, const int minValue = NO_VALUE32,
                           bool minError = false, const int maxValue = NO_VALUE32,
                           bool positive = true);

      // Perform a check on parameter and return a value or a code
      uint64_t checkAndGetParam64(const dlib::key_value_map &queries, const std::string &param,
                                  const uint64_t defaultValue, const uint64_t minValue = NO_VALUE64,
                                  bool minError = false, const uint64_t maxValue = NO_VALUE64);

    protected:
      // For file handling, small files will be cached
      std::map<std::string, std::string> m_mimeTypes;

      // Put handling controls
      bool m_putEnabled;
      std::set<std::string> m_putAllowedHosts;
      
      std::unique_ptr<FileCache> m_fileCache;
    };
  }
}
