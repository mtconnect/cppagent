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
//#include <boost/beast/ssl.hpp>
//#include <boost/asio/ssl/stream.hpp>

#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
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

    class Body
    {};

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
      net::ip::address address;
      unsigned short mPort{5000};
      std::shared_ptr<std::string> doc_root;
      //tcp::acceptor acceptor{0};
      std::thread* httpProcess;
      bool run{false};
      bool enableSSL{false};

      //using alloc_t = fields_alloc<char>;
      // The allocator used for the fields in the request and reply.
      // The allocator used for the fields in the request and reply.
      //alloc_t alloc_{8192};

      // The string-based response message.
      //boost::optional<http::response<http::string_body, http::basic_fields<alloc_t>>> string_response_;
      // The string-based response serializer.
      //boost::optional<http::response_serializer<http::string_body, http::basic_fields<alloc_t>>> string_serializer_;


    public:
      Server(unsigned short port=5000, const std::string &inter="0.0.0.0");
//      {
//        m_errorFunction = [](const std::string &accepts, Response &response, const std::string &msg,
//                             const ResponseCode code) {
//          response.writeResponse(msg, code);
//          return true;
//        };
//      }

      // Overridden method that is called per web request – not used
      // using httpRequest which is called from our own on_connect method.
//      const std::string on_request(const dlib::incoming_things &incoming,
//                                   dlib::outgoing_things &outgoing) override
//      {
//        throw std::logic_error("Not Implemented");
//        return "";
//      }

      // Start the http server
      void start();

      // Shutdown
      void stop(){run=false;};

      void listen();

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

      //template<class Body, class Allocator, class Send>
      template<class Body, class Allocator, class Send>
      void handle_request(http::request<Body, http::basic_fields<Allocator>> &&req ,Send &&send);

      void fail(beast::error_code ec, char const *what);

      void addRouting(const Routing &routing) { m_routings.emplace_back(routing); }

      void setErrorFunction(const ErrorFunction &func) { m_errorFunction = func; }

    protected:
      // HTTP Protocol
      void on_connect(std::istream &in, std::ostream &out, const std::string &foreign_ip,
                      const std::string &local_ip, unsigned short foreign_port,
                      unsigned short local_port, uint64_t);

    protected:
      // Put handling controls
      bool m_putEnabled;
      std::set<std::string> m_putAllowedHosts;
      std::list<Routing> m_routings;
      std::unique_ptr<FileCache> m_fileCache;
      ErrorFunction m_errorFunction;
      Routing::Request getRequest(const http::request<http::string_body>& req, const tcp::socket& socket);
      Routing::QueryMap getQueries(const std::string& queries);
      void send_response( http::status status, std::string const& error, tcp::socket socket);

    };
  }  // namespace http_server
}  // namespace mtconnect
