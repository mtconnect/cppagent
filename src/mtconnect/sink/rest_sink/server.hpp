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

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/bind/bind.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "file_cache.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/configuration/config_options.hpp"
#include "mtconnect/utilities.hpp"
#include "response.hpp"
#include "routing.hpp"
#include "session.hpp"
#include "tls_dector.hpp"

namespace mtconnect::sink::rest_sink {
  /// @brief An HTTP Server for REST
  class AGENT_LIB_API Server
  {
  public:
    /// @brief Create an HTTP server with an asio context and options
    /// @param context a boost asio context
    /// @param options configuration options
    /// - Port, defaults to 5000
    /// - AllowPut, defaults to false
    /// - ServerIp, defaults to 0.0.0.0
    /// - HttpHeaders
    Server(boost::asio::io_context &context, const ConfigOptions &options = {})
      : m_context(context),
        m_port(GetOption<int>(options, configuration::Port).value_or(5000)),
        m_options(options),
        m_allowPuts(IsOptionSet(options, configuration::AllowPut)),
        m_acceptor(context),
        m_sslContext(boost::asio::ssl::context::tls)
    {
      auto inter = GetOption<std::string>(options, configuration::ServerIp);
      if (!inter)
      {
        m_address = boost::asio::ip::make_address("0.0.0.0");
      }
      else
      {
        m_address = boost::asio::ip::make_address(*inter);
      }
      const auto fields = GetOption<StringList>(options, configuration::HttpHeaders);
      if (fields)
        setHttpHeaders(*fields);

      m_errorFunction = [](SessionPtr session, status st, const std::string &msg) {
        ResponsePtr response = std::make_unique<Response>(st, msg, "text/plain");
        session->writeFailureResponse(std::move(response));
        return true;
      };

      loadTlsCertificate();

      addSwaggerRoutings();
    }

    /// @brief Start the http server
    void start();

    /// @brief Shutdown the http server
    void stop()
    {
      m_run = false;
      m_acceptor.close();
    };

    /// @brief Listen for async connections
    void listen();

    /// @brief Add additional HTTP headers
    /// @param[in] fields the header fields as `<field>: <value>`
    void setHttpHeaders(const StringList &fields)
    {
      for (auto &f : fields)
      {
        auto i = f.find(':');
        if (i != std::string::npos)
        {
          m_fields.emplace_back(f.substr(0, i), f.substr(i + 1));
        }
      }
    }

    /// @brief Get the list of header fields
    /// @return header fields
    const auto &getHttpHeaders() const { return m_fields; }
    /// @brief get the bind port
    /// @return the port being bound
    auto getPort() const { return m_port; }

    /// @name PUT and POST handling
    ///@{

    /// @brief is the server listening for new connections
    /// @return `true` if it is listening
    bool isListening() const { return m_listening; }
    /// @brief is the server running
    /// @return `false` when shutting down
    bool isRunning() const { return m_run; }
    /// @brief are puts allowed?
    /// @return `true` if one can put to the server
    bool arePutsAllowed() const { return m_allowPuts; }
    /// @brief can one put from a particular IP address or host
    /// @param[in] host the host
    /// @return `true` if puts are allowed
    bool allowPutFrom(const std::string &host);
    /// @brief sets the allow puts flag
    /// @param[in] allow
    void allowPuts(bool allow = true) { m_allowPuts = allow; }
    /// @brief can one put from an ip address
    /// @param[in] addr the ip address
    /// @return `true` if puts are accepted from that address
    bool isPutAllowedFrom(boost::asio::ip::address &addr) const
    {
      return m_allowPutsFrom.find(addr) != m_allowPutsFrom.end();
    }
    ///@}

    /// @brief Entry point for all requests
    ///
    /// Search routings for a match, if a match is found, then dispatch the request, otherwise
    /// return an error.
    /// @param[in] session the client session
    /// @param[in] request the incoming request
    /// @return `true` if the request was matched and dispatched
    bool dispatch(SessionPtr session, RequestPtr request)
    {
      try
      {
        if (request->m_command)
        {
          auto route = m_commands.find(*request->m_command);
          if (route != m_commands.end())
          {
            if (route->second->matches(session, request))
              return true;
          }
          else
          {
            std::stringstream txt;
            txt << session->getRemote().address()
                << ": Cannot find handler for command: " << *request->m_command;
            session->fail(boost::beast::http::status::not_found, txt.str());
          }
        }
        else
        {
          for (auto &r : m_routings)
          {
            if (r.matches(session, request))
              return true;
          }

          std::stringstream txt;
          txt << session->getRemote().address() << ": Cannot find handler for: " << request->m_verb
              << " " << request->m_path;
          session->fail(boost::beast::http::status::not_found, txt.str());
        }
      }
      catch (RequestError &re)
      {
        LOG(error) << session->getRemote().address() << ": Error processing request: " << re.what();
        ResponsePtr resp = std::make_unique<Response>(re);
        session->writeResponse(std::move(resp));
      }
      catch (ParameterError &pe)
      {
        std::stringstream txt;
        txt << session->getRemote().address() << ": Parameter Error: " << pe.what();
        LOG(error) << txt.str();
        session->fail(boost::beast::http::status::not_found, txt.str());
      }
      catch (std::logic_error &le)
      {
        std::stringstream txt;
        txt << session->getRemote().address() << ": Logic Error: " << le.what();
        LOG(error) << txt.str();
        session->fail(boost::beast::http::status::not_found, txt.str());
      }
      catch (...)
      {
        std::stringstream txt;
        txt << session->getRemote().address() << ": Unknown Error thrown";
        LOG(error) << txt.str();
        session->fail(boost::beast::http::status::not_found, txt.str());
      }

      return false;
    }

    /// @brief accept a connection from a client
    /// @param[in] ec an error code
    /// @param[in] soc the incoming connection socket
    void accept(boost::system::error_code ec, boost::asio::ip::tcp::socket soc);
    /// @brief Method that generates an MTConnect Error document
    /// @param[in] ec an error code
    /// @param[in] what the description why the request failed
    void fail(boost::system::error_code ec, char const *what);

    /// @brief Add a routing to the server
    /// @param[in] routing the routing
    Routing &addRouting(const Routing &routing)
    {
      auto &route = m_routings.emplace_back(routing);
      if (m_parameterDocumentation)
        route.documentParameters(*m_parameterDocumentation);
      if (route.getCommand())
        m_commands.emplace(*route.getCommand(), &route);
      return route;
    }

    /// @brief Setup commands from routings
    void addCommands()
    {
      for (auto &route : m_routings)
      {
        if (route.getCommand())
          m_commands.emplace(*route.getCommand(), &route);
      }
    }

    /// @brief Add common set of documentation for all rest routings
    /// @param[in] docs Parameter documentation
    void addParameterDocumentation(const ParameterDocList &docs)
    {
      m_parameterDocumentation.emplace(docs);
    }

    /// @brief Set the error function to format the error during failure
    /// @param func the error function
    void setErrorFunction(const ErrorFunction &func) { m_errorFunction = func; }
    /// @brief get the error funciton
    /// @return the error function
    ErrorFunction getErrorFunction() const { return m_errorFunction; }

    /// @name Only for testing
    ///@{
    /// @brief Callback for testing. Allows test to grab the last session dispatched.
    std::function<void(SessionPtr)> m_lastSession;
    ///@}

  protected:
    void loadTlsCertificate();

    /// @name Swagger Support
    /// @{
    ///
    /// @brief Add swagger routings to the Agent
    void addSwaggerRoutings();
    /// @brief generate swagger API from routings
    /// @param[in] format The mime format of the response ("json" or "yaml")
    ///
    /// Caches the API document based on the response type requested. Cache cleared whenever a new
    /// routing is added.
    template <typename T>
    const void renderSwaggerResponse(T &format);
    /// @}

  protected:
    boost::asio::io_context &m_context;

    boost::asio::ip::address m_address;
    unsigned short m_port {5000};

    bool m_run {false};
    bool m_listening {false};

    ConfigOptions m_options;
    // Put handling controls
    bool m_allowPuts {false};
    std::set<boost::asio::ip::address> m_allowPutsFrom;

    std::list<Routing> m_routings;
    std::map<std::string, Routing *> m_commands;
    std::unique_ptr<FileCache> m_fileCache;
    ErrorFunction m_errorFunction;
    FieldList m_fields;

    std::optional<ParameterDocList> m_parameterDocumentation;

    boost::asio::ip::tcp::acceptor m_acceptor;
    boost::asio::ssl::context m_sslContext;
    bool m_tlsEnabled {false};
    bool m_tlsOnly {false};
  };
}  // namespace mtconnect::sink::rest_sink
