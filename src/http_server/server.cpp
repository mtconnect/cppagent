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

#include "server.hpp"
#include "response.hpp"

#include <dlib/logger.h>

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
        // Start the server. This blocks until the server stops.
        server_http::start();
      }
      catch (dlib::socket_error &e)
      {
        g_logger << LFATAL << "Cannot start server: " << e.what();
        std::exit(1);
      }
    }
    
    void Server::on_connect(std::istream &in, std::ostream &out,
                                const std::string &foreign_ip,
                                const std::string &local_ip,
                                unsigned short foreign_port,
                                unsigned short local_port, uint64)
    {
      Response response(out);
      std::string accepts;
      try
      {
        IncomingThings incoming(foreign_ip, local_ip, foreign_port, local_port);
        parse_http_request(in, incoming, get_max_content_length());
        auto ai = incoming.headers.find("Accept");
        if (ai != incoming.headers.end())
          accepts = ai->second;

        if (incoming.request_type == "PUT" || incoming.request_type == "POST" ||
            incoming.request_type == "DELETE")
        {
          if (!m_putEnabled || !isPutAllowedFrom(foreign_ip))
          {
            stringstream msg;
            msg << "Error processing request from: " << foreign_ip << " - " <<
                  "Server is read-only. Only GET verb supported";
            g_logger << LERROR << msg.str();
            
            if (m_errorFunction)
              m_errorFunction(accepts, response, msg.str(), FORBIDDEN);
            out.flush();
           return;
          }
        }
        
        read_body(in, incoming);
        
        Routing::Request request;
        request.m_verb = incoming.request_type;
        request.m_path = incoming.path;
        request.m_query = incoming.queries;
        request.m_body = incoming.body;
        request.m_foreignIp = foreign_ip;
        request.m_foreignPort = foreign_port;
        request.m_accepts = accepts;
        auto media = incoming.headers.find("Content-Type");
        if (media != incoming.headers.end())
          request.m_contentType = media->second;
        
        handleRequest(request, response);
      }
      catch (dlib::http_parse_error &e)
      {
        stringstream msg;
        msg << "Error processing request from: " << foreign_ip << " - " << e.what();
        g_logger << LERROR << msg.str();
                
        if (m_errorFunction)
          m_errorFunction(accepts, response, msg.str(), BAD_REQUEST);
      }
      catch (std::exception &e)
      {
        stringstream msg;
        msg << "Error processing request from: " << foreign_ip << " - " << e.what();
        g_logger << LERROR << msg.str();
        
        if (m_errorFunction)
          m_errorFunction(accepts, response, msg.str(), BAD_REQUEST);
      }
    }
  
    bool Server::handleRequest(Routing::Request &request, Response &response)
    {
      bool res { true };
      try
      {
        if (!dispatch(request, response))
        {
          stringstream msg;
          msg << "Error processing request from: " << request.m_foreignIp << " - " <<
                "No matching route for: " << request.m_verb << " " << request.m_path;
          g_logger << LERROR << msg.str();

          
          if (m_errorFunction)
            m_errorFunction(request.m_accepts, response, msg.str(), BAD_REQUEST);
          res = false;
        }
      }
      catch (RequestError &e)
      {
        g_logger << LERROR << "Error processing request from: " << request.m_foreignIp << " - " << e.what();
        response.writeResponse(e.m_body, e.m_contentType, e.m_code);
        res = false;
      }
      catch (ParameterError &e)
      {
        stringstream msg;
        msg << "Parameter Error processing request from: " << request.m_foreignIp << " - " << e.what();
        g_logger << LERROR << msg.str();
        
        if (m_errorFunction)
          m_errorFunction(request.m_accepts, response, msg.str(), BAD_REQUEST);
        res = false;
      }

      response.flush();
      return res;
    }
  }
}
