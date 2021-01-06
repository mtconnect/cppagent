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
      try
      {
        IncomingThings incoming(foreign_ip, local_ip, foreign_port, local_port);
        parse_http_request(in, incoming, get_max_content_length());
        
        if (incoming.request_type == "PUT" || incoming.request_type == "POST" ||
            incoming.request_type == "DELETE")
        {
          if (!m_putEnabled || !isPutAllowedFrom(foreign_ip))
          {
            g_logger << LERROR << "Error processing request from: " << foreign_ip << " - " <<
                "Server is read-only. Only GET verb supported";
            response.writeResponse("Server is read-only. Only GET verb supported",
                                   "text/plain", FORBIDDEN);
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
        auto accepts = incoming.headers.find("Accept");
        if (accepts != incoming.headers.end())
          request.m_accepts = accepts->second;
        auto media = incoming.headers.find("Content-Type");
        if (media != incoming.headers.end())
          request.m_contentType = media->second;
        
        handleRequest(request, response);
      }
      catch (dlib::http_parse_error &e)
      {
        g_logger << LERROR << "Error processing request from: " << foreign_ip << " - " << e.what();
        write_http_response(out, e);
      }
      catch (std::exception &e)
      {
        g_logger << LERROR << "Error processing request from: " << foreign_ip << " - " << e.what();
        write_http_response(out, e);
      }
    }
  
    bool Server::handleRequest(Routing::Request &request, Response &response)
    {
      bool res { true };
      try
      {
        if (!dispatch(request, response))
        {
          g_logger << LERROR << "Error processing request from: " << request.m_foreignIp << " - " <<
              "No matching route for: " << request.m_verb << " " << request.m_path;
          response.writeResponse("No routing matches: " + request.m_verb + " "
                                 + request.m_path, "text/plain", BAD_REQUEST);
          res = false;
        }
      }
      catch (RequestError &e)
      {
        g_logger << LERROR << "Error processing request from: " << request.m_foreignIp << " - " << e.what();
        response.writeResponse(e.m_body, e.m_contentType, e.m_code);
        res = false;
      }

      response.flush();
      return res;
    }
  }
}
