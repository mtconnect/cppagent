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

#include "server.hpp"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>

#include "logging.hpp"
#include "session_impl.hpp"

#include <thread>



namespace mtconnect
{
  namespace http_server
  {
    namespace beast = boost::beast;         // from <boost/beast.hpp>
    namespace http = beast::http;           // from <boost/beast/http.hpp>
    namespace net = boost::asio;            // from <boost/asio.hpp>
    namespace asio = boost::asio;
    namespace ip = boost::asio::ip;
    using tcp = boost::asio::ip::tcp;
    namespace algo = boost::algorithm;
    
    using namespace std;
    using boost::placeholders::_1;
    using boost::placeholders::_2;
    

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
      
//            if(enableSSL) {
//                // The SSL context is required, and holds certificates
//                ssl::context ctx{ssl::context::tlsv12};
//
//                // This holds the self-signed certificate used by the server
//                load_server_certificate(ctx);
//            }

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

    void Server::accept(beast::error_code ec, tcp::socket socket)
    {
      NAMED_SCOPE("Server::accept");

      if (ec)
      {
        LOG(error) << ec.category().message(ec.value()) << ": "
        << ec.message();

        fail(ec, "Accept failed");
      }
      else
      {
        auto session = make_shared<SessionImpl>(socket, m_fields, [this](RequestPtr request){
          dispatch(request);
          return true; }, m_errorFunction);
        session->run();
        m_acceptor.async_accept(net::make_strand(m_context),
                                beast::bind_front_handler(&Server::accept, this));
      }
    }
    
#if 0

    //Parse http::request and return
    static Routing::Request getRequest(const http::request<http::string_body>& req, const tcp::socket& socket)
    {
      NAMED_SCOPE("Server::getRequest");

      Routing::Request request;

      try
      {
        string queries;
        string path = static_cast<std::string>(req.target().data()).substr(0,req.target().length());
        request.m_verb = req.method_string().data();
        if (request.m_verb == "GET")
        {
          while(path.find("%22")!=path.npos)
          {
            auto pt = path.find_first_of("%22");
            path.replace(pt,3,"\"");
          }

          auto qp = path.find_first_of('?');
          if (qp != string::npos){
            queries = path.substr(qp+1);
            path.erase(qp);
          }
          request.m_path = path;
          auto pt = queries.find_first_of('=');
          if (pt != string::npos){
            request.m_query = getQueries(queries);
          }
        }
        else if (request.m_verb == "PUT" || request.m_verb == "POST" || request.m_verb == "DELETE")
        {
          if (path.find("asset") != path.npos)
          {
            auto qp = path.find_first_of('?');
            if (qp != string::npos){
              queries = path.substr(qp+1);
              path.erase(qp);
            }
            request.m_query = parseAsset(queries, req.body());
          }
          else
          {
            request.m_query = getQueries(req.body());
          }
          //string data = req.body();
          request.m_path = path;
        }
        request.m_body = req.body();
        request.m_foreignIp = socket.remote_endpoint().address().to_string();
        request.m_foreignPort = socket.remote_endpoint().port();
        request.m_accepts = req.find(http::field::accept)->value().data();
        request.m_accepts = request.m_accepts.substr(0,request.m_accepts.size()-2);
        auto media = req.find(http::field::content_type)->value().data();
        if (media != nullptr)
          request.m_contentType = media;
      }
      catch (exception &e)
      {
        LOG(error) << "method:" << __func__  <<" error: " <<e.what();
      }
      return request;
    }
    
    void Server::session(tcp::socket &socket)
    {
      NAMED_SCOPE("Server::session");

      try{
        beast::error_code ec;
        beast::flat_buffer buffer;
        http::request<http::string_body> req;
        http::read(socket, buffer, req, ec);

        if (ec)
          return fail(ec, "write");

        Routing::Request request = getRequest(req, socket);
        ResponsePtr response = make_unique<Response>(socket, m_fields);

        if (request.m_verb == "PUT" || request.m_verb == "POST" ||
            request.m_verb == "DELETE")
        {
          if (!m_putEnabled || !isPutAllowedFrom(request.m_foreignIp))
          {
            stringstream msg;
            msg << "Error processing request from: " << request.m_foreignIp << " - "
                << "Server is read-only. Only GET verb supported";
            LOG(error) << msg.str();

            if (m_errorFunction)
              m_errorFunction(request.m_accepts, *response, msg.str(), FORBIDDEN);
            //out.flush();
            return;
          }
        }

        if(!handleRequest(request))
        {
          LOG(error) << "error handling Request. ";
        };

        buffer.clear();
      }
      catch (exception &e)
      {
        LOG(error) << "error handling Request: "<< e.what();
      }
    }


    bool Server::handleRequest(Routing::Request &request)
    {
      NAMED_SCOPE("Server::handleRequest");

      bool res {true};
      try
      {
        if (!dispatch(request, response))
        {
          stringstream msg;
          msg << "Error processing request from: " << request.m_foreignIp << " - "
              << "No matching route for: " << request.m_verb << " " << request.m_path;
          LOG(error) << msg.str();

          if (m_errorFunction)
            m_errorFunction(request.m_accepts, *response, msg.str(), BAD_REQUEST);
          res = false;
        }
      }
      catch (RequestError &e)
      {
        LOG(error) << "Error processing request from: " << request.m_foreignIp << " - "
                 << e.what();
        response->writeResponse(e.m_body, e.m_contentType, e.m_code);
        res = false;
      }
      catch (ParameterError &e)
      {
        stringstream msg;
        msg << "Parameter Error processing request from: " << request.m_foreignIp << " - "
            << e.what();
        LOG(error) << msg.str();

        if (m_errorFunction)
          m_errorFunction(request.m_accepts, *response, msg.str(), BAD_REQUEST);
        res = false;
      }

      //response.flush();
      return res;
    }

#endif


//------------------------------------------------------------------------------

// Report a failure
    void Server::fail(beast::error_code ec, char const *what) {
      LOG(error)  << " error: " << ec.message();
    }

  }  // namespace http_server
}  // namespace mtconnect
