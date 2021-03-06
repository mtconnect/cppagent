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

#include "response.hpp"

#include <dlib/logger.h>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <thread>


namespace mtconnect
{
  namespace http_server
  {
    using namespace std;
    using namespace dlib;

    static dlib::logger g_logger("HttpServer");



    void Server::start()
    {
      try
      {
        run = true;
        httpProcess = new std::thread(&Server::listen, this );
        httpProcess->join();
      }
      catch (exception &e)
      {
        g_logger << LFATAL << "Cannot start server: " << e.what();
        std::exit(1);
      }
    }

    // Listen for an HTTP server connection
    void Server::listen()
    {
      bool close = false;
      beast::error_code ec;
      beast::flat_buffer buffer;
      net::io_context ioc{1};

//            if(enableSSL) {
//                // The SSL context is required, and holds certificates
//                ssl::context ctx{ssl::context::tlsv12};
//
//                // This holds the self-signed certificate used by the server
//                load_server_certificate(ctx);
//            }

      // Blocking call to listen for a connection
      tcp::acceptor acceptor{ioc, {address, mPort}};


      while (run) {
        try{
        tcp::socket socket{ioc};
        acceptor.accept(socket);
        std::thread{std::bind(&Server::session, this, std::move(socket))}.detach();
        socket.shutdown(tcp::socket::shutdown_send, ec);//        http::request<http::string_body> req;
//        http::read(socket, buffer, req, ec);
//
//        if (ec)
//          return fail(ec, "write");
//
//        Routing::Request request = getRequest(req, socket);
//        Response response(socket, m_fields);
//        if(!handleRequest(request, response))
//        {
//          throw "Server failed to handle request";
//        };
//
//        buffer.clear();
        }
        catch (exception &e)
        {
          g_logger << LERROR << "Server::listen error: "<< e.what();
        }
      }
    }

    void Server::session( tcp::socket &socket)
    {
      tcp::socket m_socket(std::move(socket));
      try{
        beast::error_code ec;
        beast::flat_buffer buffer;
        http::request<http::string_body> req;
        http::read(m_socket, buffer, req, ec);

        if (ec)
          return fail(ec, "write");

        Routing::Request request = getRequest(req, m_socket);
        Response response(m_socket, m_fields);

        if (request.m_verb == "PUT" || request.m_verb == "POST" ||
            request.m_verb == "DELETE")
        {
          if (!m_putEnabled || !isPutAllowedFrom(request.m_foreignIp))
          {
            stringstream msg;
            msg << "Error processing request from: " << request.m_foreignIp << " - "
                << "Server is read-only. Only GET verb supported";
            g_logger << LERROR << msg.str();

            if (m_errorFunction)
              m_errorFunction(request.m_accepts, response, msg.str(), FORBIDDEN);
            //out.flush();
            return;
          }
        }

        if(!handleRequest(request, response))
        {
          g_logger << LERROR << "Server::session error handling Request. ";
        };

        m_socket.shutdown(tcp::socket::shutdown_send, ec);
        buffer.clear();
      }
      catch (exception &e)
      {
        g_logger << LERROR << "Server::listen error: "<< e.what();
      }
    }


    //Parse http::request and return
    Routing::Request Server::getRequest(const http::request<http::string_body>& req, const tcp::socket& socket)
    {
      Routing::Request request;

      try
      {

        string queries;

        string path = static_cast<std::string>(req.target().data()).substr(0,req.target().length());
        while(path.find("%22")!=path.npos)
        {
          auto pt = path.find_first_of("%22");
          path.replace(pt,3,"\"");
        }

        //auto sizeString = path.size();
        //path = path.substr(0,sizeString-1);
        //path = path.substr(1,path.size()-1);
        auto qp = path.find_first_of('?');
        if (qp != string::npos){
          queries = path.substr(qp+1);
          path.erase(qp);
        }

        request.m_verb = req.method_string().data();

        request.m_path = path;
        auto pt = queries.find_first_of('=');
        if (pt != string::npos){
          request.m_query = getQueries(queries);
        }
//        request.m_query.clear();
        request.m_body = "";
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
        g_logger << LERROR << "method:" << __func__  <<" error: " <<e.what();
      }
      return request;
    }

    Routing::QueryMap Server::getQueries(const std::string& queries)
    {
      Routing::QueryMap queryMap;
      std::string tmpStr = queries;
      try
      {
        if (tmpStr.empty()) throw("queries does not contain a query.");

        while (!tmpStr.empty())
        {
          auto ptr = tmpStr.find_first_of("&");
          if (ptr != std::string().npos)
          {
            std:string string1 = tmpStr.substr(0,ptr);
            tmpStr = tmpStr.substr(ptr+1);
            ptr = string1.find_first_of('=');
            if (ptr != std::string().npos){
              queryMap.insert(std::pair<std::string,std::string >(string1.substr(0,ptr),string1.substr(ptr+1)));
            }
            else{
              throw("String does not contain a query.");
            }
          }
          else
          {
            //get the last parameter set
            ptr = tmpStr.find_first_of("=");
            if (ptr != std::string().npos){
              queryMap.insert(std::pair<std::string,std::string >(tmpStr.substr(0,ptr),tmpStr.substr(ptr+1)));
              tmpStr.clear();
            }
            else{
              throw("Error: string does not contain a query.");
            }
          }
        }
        return queryMap;
      }
      catch (exception& e)
      {
        g_logger << LERROR << __func__ << " error: " <<e.what();
        return queryMap;
      }
    }

    bool Server::handleRequest(Routing::Request &request, Response &response)
    {
      bool res{true};
      try
      {
        if (!dispatch(request, response))
        {
          stringstream msg;
          msg << "Error processing request from: " << request.m_foreignIp << " - "
              << "No matching route for: " << request.m_verb << " " << request.m_path;
          g_logger << LERROR << msg.str();

          if (m_errorFunction)
            m_errorFunction(request.m_accepts, response, msg.str(), BAD_REQUEST);
          res = false;
        }
      }
      catch (RequestError &e)
      {
        g_logger << LERROR << "Error processing request from: " << request.m_foreignIp << " - "
                 << e.what();
        response.writeResponse(e.m_body, e.m_contentType, e.m_code);
        res = false;
      }
      catch (ParameterError &e)
      {
        stringstream msg;
        msg << "Parameter Error processing request from: " << request.m_foreignIp << " - "
            << e.what();
        g_logger << LERROR << msg.str();

        if (m_errorFunction)
          m_errorFunction(request.m_accepts, response, msg.str(), BAD_REQUEST);
        res = false;
      }

      //response.flush();
      return res;
    }


//------------------------------------------------------------------------------

// Report a failure
    void Server::fail(beast::error_code ec, char const *what) {
      std::cerr << what << ": " << ec.message() << "\n";
    }

  }  // namespace http_server
}  // namespace mtconnect
