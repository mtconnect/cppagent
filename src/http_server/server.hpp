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
//#include "fields_alloc.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/bind/bind.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
//#include <boost/beast/ssl.hpp>
//#include <boost/asio/ssl/stream.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <regex>
#include "file_cache.hpp"
#include "response.hpp"
#include "routing.hpp"
#include "config_options.hpp"
#include "utilities.hpp"

//#include "http_server.hpp"
//#include <dlib/server.h>

namespace mtconnect
{
  namespace http_server
  {
    //using IncomingThings = dlib::incoming_things;
    namespace beast = boost::beast;         // from <boost/beast.hpp>
    namespace http = beast::http;           // from <boost/beast/http.hpp>
    namespace net = boost::asio;            // from <boost/asio.hpp>
    //namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
    using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>


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

    class Server
    {

    public:
      Server(unsigned short port = 5000, const std::string &inter = "0.0.0.0", const ConfigOptions &options = {}):
      mPort(port)
      {
        address = net::ip::make_address(inter);
        const auto fields = GetOption<StringList>(options, configuration::HttpHeaders);
        m_errorFunction = [](const std::string &accepts, Response &response, const std::string &msg,
                             const ResponseCode code) {
          response.writeResponse(msg, code);
          return true;
        };
      }

      // Start the http server
      void start();

      // Shutdown
      void stop(){run=false;};

      void listen();

      void setHttpHeaders(const StringList &fields)
      {
        m_fields = fields;
      }

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


      //template<class Body, class Allocator, class Send>
      template<class Body, class Allocator, class Send>
      void handle_request(http::request<Body, http::basic_fields<Allocator>> &&req ,Send &&send);

      void fail(beast::error_code ec, char const *what);

      void addRouting(const Routing &routing) { m_routings.emplace_back(routing); }

      void setErrorFunction(const ErrorFunction &func) { m_errorFunction = func; }


    protected:
      net::ip::address address;
      unsigned short mPort{5000};
      std::thread* httpProcess;
      bool run{false};
      bool enableSSL{false};
      ConfigOptions options;
      // Put handling controls
      bool m_putEnabled;
      std::set<std::string> m_putAllowedHosts;
      std::list<Routing> m_routings;
      std::unique_ptr<FileCache> m_fileCache;
      ErrorFunction m_errorFunction;
      StringList m_fields;
      Routing::Request getRequest(const http::request<http::string_body>& req, const tcp::socket& socket);
      Routing::QueryMap getQueries(const std::string& queries);
    };
  }  // namespace http_server
}  // namespace mtconnect
