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

#include "server.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/asio.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/tokenizer.hpp>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <thread>

#include "mtconnect/logging.hpp"
#include "session_impl.hpp"

namespace mtconnect::sink::rest_sink {
  namespace beast = boost::beast;  // from <boost/beast.hpp>
  namespace http = beast::http;    // from <boost/beast/http.hpp>
  namespace net = boost::asio;     // from <boost/asio.hpp>
  namespace asio = boost::asio;
  namespace ip = boost::asio::ip;
  using tcp = boost::asio::ip::tcp;
  namespace algo = boost::algorithm;
  namespace ssl = boost::asio::ssl;

  using namespace std;
  using namespace rapidjson;
  using boost::placeholders::_1;
  using boost::placeholders::_2;

  void Server::loadTlsCertificate()
  {
    if (HasOption(m_options, configuration::TlsCertificateChain) &&
        HasOption(m_options, configuration::TlsPrivateKey) &&
        HasOption(m_options, configuration::TlsDHKey))
    {
      LOG(info) << "Initializing TLS support";
      if (HasOption(m_options, configuration::TlsCertificatePassword))
      {
        m_sslContext.set_password_callback(
            [this](size_t, boost::asio::ssl::context_base::password_purpose) -> string {
              return *GetOption<string>(m_options, configuration::TlsCertificatePassword);
            });
      }

      m_sslContext.set_options(ssl::context::default_workarounds | asio::ssl::context::no_sslv2 |
                               asio::ssl::context::single_dh_use);
      m_sslContext.use_certificate_chain_file(
          *GetOption<string>(m_options, configuration::TlsCertificateChain));
      m_sslContext.use_private_key_file(*GetOption<string>(m_options, configuration::TlsPrivateKey),
                                        asio::ssl::context::file_format::pem);
      m_sslContext.use_tmp_dh_file(*GetOption<string>(m_options, configuration::TlsDHKey));

      m_tlsEnabled = true;

      m_tlsOnly = IsOptionSet(m_options, configuration::TlsOnly);

      if (IsOptionSet(m_options, configuration::TlsVerifyClientCertificate))
      {
        LOG(info) << "Will only accept client connections with valid certificates";

        m_sslContext.set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);
        if (HasOption(m_options, configuration::TlsClientCAs))
        {
          LOG(info) << "Adding Client Certificates.";
          m_sslContext.load_verify_file(*GetOption<string>(m_options, configuration::TlsClientCAs));
        }
      }
    }
  }

  void Server::start()
  {
    try
    {
      m_run = true;
      listen();
    }
    catch (exception &e)
    {
      LOG(fatal) << "Cannot start server: " << e.what();
      std::exit(1);
    }
  }

  // Listen for an HTTP server connection
  void Server::listen()
  {
    NAMED_SCOPE("Server::listen");

    beast::error_code ec;

    // Blocking call to listen for a connection
    tcp::endpoint ep(m_address, m_port);
    m_acceptor.open(ep.protocol(), ec);
    if (ec)
    {
      fail(ec, "Cannot open server socket");
      return;
    }
    m_acceptor.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec)
    {
      fail(ec, "Cannot set reuse address");
      return;
    }
    m_acceptor.bind(ep, ec);
    if (ec)
    {
      fail(ec, "Cannot bind to server address");
      return;
    }
    if (m_port == 0)
    {
      m_port = m_acceptor.local_endpoint().port();
    }

    m_acceptor.listen(net::socket_base::max_listen_connections, ec);
    if (ec)
    {
      fail(ec, "Cannot set listen queue length");
      return;
    }

    m_listening = true;
    m_acceptor.async_accept(net::make_strand(m_context),
                            beast::bind_front_handler(&Server::accept, this));
  }

  bool Server::allowPutFrom(const std::string &host)
  {
    NAMED_SCOPE("Server::allowPutFrom");

    // Resolve the host to an ip address to verify remote addr
    beast::error_code ec;
    ip::tcp::resolver resolve(m_context);
    auto results = resolve.resolve(host, "0", ec);
    if (ec)
    {
      LOG(error) << "Cannot resolve address: " << host;
      LOG(error) << ec.category().message(ec.value()) << ": " << ec.message();
      return false;
    }

    // Add the results to the set of allowed hosts
    for (auto &addr : results)
    {
      m_allowPutsFrom.insert(addr.endpoint().address());
    }
    m_allowPuts = true;

    return true;
  }

  void Server::accept(beast::error_code ec, tcp::socket socket)
  {
    NAMED_SCOPE("Server::accept");

    if (ec)
    {
      LOG(error) << ec.category().message(ec.value()) << ": " << ec.message();

      fail(ec, "Accept failed");
    }
    else if (m_run)
    {
      auto dispatcher = [this](SessionPtr session, RequestPtr request) {
        if (!m_run)
          return false;

        if (m_lastSession)
          m_lastSession(session);
        dispatch(session, request);
        return true;
      };
      if (m_tlsEnabled)
      {
        auto dectector =
            make_shared<TlsDector>(move(socket), m_sslContext, m_tlsOnly, m_allowPuts,
                                   m_allowPutsFrom, m_fields, dispatcher, m_errorFunction);

        dectector->run();
      }
      else
      {
        boost::beast::flat_buffer buffer;
        boost::beast::tcp_stream stream(move(socket));
        auto session = make_shared<HttpSession>(move(stream), move(buffer), m_fields, dispatcher,
                                                m_errorFunction);

        if (!m_allowPutsFrom.empty())
          session->allowPutsFrom(m_allowPutsFrom);
        else if (m_allowPuts)
          session->allowPuts();

        session->run();
      }
      m_acceptor.async_accept(net::make_strand(m_context),
                              beast::bind_front_handler(&Server::accept, this));
    }
  }

  //------------------------------------------------------------------------------

  // Report a failure
  void Server::fail(beast::error_code ec, char const *what)
  {
    LOG(error) << " error: " << ec.message();
  }

  template <typename T>
  struct JsonHelper
  {
    JsonHelper(T &writer) : m_writer(writer) {}

    void Key(const char *s) { m_writer.Key(s); }

    void StartObject() { m_writer.StartObject(); }

    void EndObject() { m_writer.EndObject(); }

    void StartArray() { m_writer.StartArray(); }

    void EndArray() { m_writer.EndArray(); }

    void Add(double v) { m_writer.Double(v); }

    void Add(bool v) { m_writer.Bool(v); }

    void Add(int32_t i) { m_writer.Int(i); }

    void Add(uint32_t i) { m_writer.Uint(i); }

    void Add(int64_t i) { m_writer.Int64(i); }

    void Add(uint64_t i) { m_writer.Uint64(i); }

    void Add(const char *s) { m_writer.String(s); }

    void Add(const string &s) { m_writer.String(s.data(), SizeType(s.size())); }

    T &m_writer;
  };

  //----------------
  template <typename T>
  struct AutoJsonObject : JsonHelper<T>
  {
    using base = JsonHelper<T>;
    AutoJsonObject(T &writer) : JsonHelper<T>(writer) { base::StartObject(); }

    ~AutoJsonObject() { base::EndObject(); }

    template <typename T1, typename T2, typename... R>
    void AddPairs(const T1 &v1, const T2 &v2, R... rest)
    {
      base::Key(v1);
      base::Add(v2);

      AddPairs(rest...);
    }

    template <typename T1, typename T2, typename... R>
    void AddPairs(const T1 &v1, const T2 &v2)
    {
      base::m_writer.Key(v1);
      base::Add(v2);
    }
  };

  template <typename T>
  struct AutoJsonArray : JsonHelper<T>
  {
    using base = JsonHelper<T>;
    AutoJsonArray(T &writer) : JsonHelper<T>(writer) { base::StartArray(); }

    ~AutoJsonArray() { base::EndArray(); }
  };

  template <typename T>
  void AddParameter(T &writer, const Parameter &param)
  {
    AutoJsonObject<T> obj(writer);

    obj.AddPairs("name", param.m_name, "in", param.m_part == PATH ? "path" : "query", "required",
                 param.m_part == PATH);

    writer.Key("schema");
    {
      AutoJsonObject<T> obj(writer);
      switch (param.m_type)
      {
        case ParameterType::STRING:
          obj.AddPairs("type", "string", "format", "string");
          break;

        case ParameterType::INTEGER:
          obj.AddPairs("type", "integer", "format", "int64");
          break;

        case ParameterType::UNSIGNED_INTEGER:
          obj.AddPairs("type", "integer", "format", "uint64");
          break;

        case ParameterType::DOUBLE:
          obj.AddPairs("type", "double", "format", "double");
          break;

        case ParameterType::NONE:
          obj.AddPairs("type", "unknown", "format", "unknown");
          break;
      }
      if (param.m_default.index() != NONE)
      {
        obj.Key("default");
        visit(
            overloaded {[](const std::monostate &) {}, [&obj](const std::string &s) { obj.Add(s); },
                        [&obj](int32_t i) { obj.Add(i); }, [&obj](uint64_t i) { obj.Add(i); },
                        [&obj](double d) { obj.Add(d); }},
            param.m_default);
      }
    }
  }

  template <typename T>
  void AddRouting(T &writer, const Routing &routing)
  {
    writer.Key(routing.getPath()->c_str());
    {
      AutoJsonObject<T> obj(writer);

      string verb {to_string(routing.getVerb())};
      boost::to_lower(verb);

      writer.Key(verb.data(), SizeType(verb.length()));
      {
        AutoJsonObject<T> obj(writer);
        if (!routing.getPathParameters().empty() || !routing.getQueryParameters().empty())
        {
          writer.Key("parameters");
          {
            AutoJsonArray<T> ary(writer);
            {
              for (const auto &param : routing.getPathParameters())
              {
                AddParameter(writer, param);
              }
              for (const auto &param : routing.getQueryParameters())
              {
                AddParameter(writer, param);
              }
            }
          }
        }

        writer.Key("responses");
        {
          AutoJsonObject<T> obj(writer);
          writer.Key("200");
          {
            AutoJsonObject<T> obj(writer);
            obj.AddPairs("description", "OK");
          }
        }
      }
    }
  }

  // Swagger stuff
  template <typename T>
  const std::string &Server::renderSwaggerResponse(const std::string &format)
  {
    auto it = m_swaggerAPI.find(format);
    if (it != m_swaggerAPI.end())
      return it->second;

    StringBuffer output;
    unique_ptr<T> writer = make_unique<T>(output);
    {
      AutoJsonObject<T> obj(*writer);

      obj.AddPairs("openapi", "3.1.0");

      writer->Key("info");
      {
        AutoJsonObject<T> obj(*writer);

        obj.AddPairs("title", "MTConnect – REST API", "description", "MTConnect REST API ");

        writer->Key("contact");
        {
          AutoJsonObject<T> obj(*writer);

          obj.AddPairs("email", "will@metalogi.io");
        }

        writer->Key("license");
        {
          AutoJsonObject<T> obj(*writer);

          obj.AddPairs("name", "Apache 2.0", "url",
                       "http://www.apache.org/licenses/LICENSE-2.0.html");
        }

        obj.AddPairs("version", GetAgentVersion());
      }

      writer->Key("externalDocs");
      {
        AutoJsonObject<T> obj(*writer);

        obj.AddPairs("description", "For information related to MTConnect", "url",
                     "http://mtconnect.org");
      }

      writer->Key("Servers");
      {
        AutoJsonArray<T> ary(*writer);
        {
          AutoJsonObject<T> obj(*writer);

          stringstream str;
          if (m_tlsEnabled)
            str << "https://";
          else
            str << "http://";
          str << m_address.to_string() << ':' << m_port;

          obj.AddPairs("url", str.str());
        }
      }

      writer->Key("paths");
      {
        AutoJsonObject<T> obj(*writer);
        for (const auto &routing : m_routings)
        {
          if (!routing.isSwagger() && routing.getPath())
            AddRouting<T>(*writer, routing);
        }
      }
    }

    static string error = R"({"error": "API Generation Failed"})";
    auto [n, b] = m_swaggerAPI.insert_or_assign(format, string(output.GetString()));
    if (b)
      return n->second;
    else
      return error;
  }

  void Server::addSwaggerRoutings()
  {
    auto handler = [&](SessionPtr session, const RequestPtr request) -> bool {
      if (m_pretty)
      {
        const string &response = renderSwaggerResponse<PrettyWriter<StringBuffer>>("json");
        session->writeResponse(make_unique<Response>(status::ok, response, "application/json"));
      }
      else
      {
        //       const string &response = renderSwaggerResponse<Writer<StringBuffer>>("json");
        //      session->writeResponse(make_unique<Response>(status::ok, response,
        //      "application/json"));
      }
      return true;
    };

    addRouting({boost::beast::http::verb::get, "/swagger.json", handler, true});
    // addRouting({boost::beast::http::verb::get, "/swagger.yaml", handler, true});
  }

}  // namespace mtconnect::sink::rest_sink
