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

#include "http_server.hpp"

#include <dlib/logger.h>

namespace mtconnect
{
  namespace http_server
  {
    using namespace std;
    using namespace dlib;
    
//    m_mimeTypes({{{"xsl", "text/xsl"},
//                  {"xml", "text/xml"},
//                  {"json", "application/json"},
//                  {"css", "text/css"},
//                  {"xsd", "text/xml"},
//                  {"jpg", "image/jpeg"},
//                  {"jpeg", "image/jpeg"},
//                  {"png", "image/png"},
//                  {"ico", "image/x-icon"}}}),
    
    static dlib::logger g_logger("HttpServer");

    
    void HttpServer::on_connect(std::istream &in, std::ostream &out,
                                const std::string &foreign_ip,
                                const std::string &local_ip,
                                unsigned short foreign_port,
                                unsigned short local_port, uint64)
    {
      try
      {
        IncomingThings incoming(foreign_ip, local_ip, foreign_port, local_port);
        OutgoingThings outgoing;

        parse_http_request(in, incoming, get_max_content_length());
        read_body(in, incoming);
        outgoing.m_out = &out;
        const std::string &result = httpRequest(incoming, outgoing);
        if (out.good())
        {
          write_http_response(out, outgoing, result);
        }
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

    // Methods for service
    const string HttpServer::httpRequest(const IncomingThings &incoming,
                                           OutgoingThings &outgoing)
    {
      string result;

      try
      {
        g_logger << LDEBUG << "Request: " << incoming.request_type << " " << incoming.path << " from "
                 << incoming.foreign_ip << ":" << incoming.foreign_port;

        if (m_putEnabled && incoming.request_type != "GET")
        {
          if (incoming.request_type != "PUT" && incoming.request_type != "POST" &&
              incoming.request_type != "DELETE")
          {
            //return printError(printer, "UNSUPPORTED",
            //                  "Only the HTTP GET, PUT, POST, and DELETE requests are supported");
          }

          if (!m_putAllowedHosts.empty() && !m_putAllowedHosts.count(incoming.foreign_ip))
          {
            //return printError(
             //   printer, "UNSUPPORTED",
              //  "HTTP PUT, POST, and DELETE are not allowed from " + incoming.foreign_ip);
          }
        }
        else if (incoming.request_type != "GET")
        {
//          return printError(printer, "UNSUPPORTED", "Only the HTTP GET request is supported");
        }

        // Parse the URL path looking for '/'
        string path = incoming.path;
        auto qm = path.find_last_of('?');
        if (qm != string::npos)
          path = path.substr(0, qm);

        if (isFile(path))
          return handleFile(path, outgoing);

        string::size_type loc1 = path.find('/', 1);
        string::size_type end = (path[path.length() - 1] == '/') ? path.length() - 1 : string::npos;

        string first = path.substr(1, loc1 - 1);
        string call, device;

        if (first == "assets" || first == "asset")
        {
          string list;
          if (loc1 != string::npos)
            list = path.substr(loc1 + 1);

//          if (incoming.request_type == "GET")
//            result = handleAssets(printer, *outgoing.m_out, incoming.queries, list);
//          else
//            result = storeAsset(*outgoing.m_out, incoming.queries, incoming.request_type, list,
//                                incoming.body);
        }
        else
        {
          // If a '/' was found
          if (loc1 < end)
          {
            // Look for another '/'
            string::size_type loc2 = path.find('/', loc1 + 1);

            if (loc2 == end)
            {
              device = first;
              call = path.substr(loc1 + 1, loc2 - loc1 - 1);
            }
            else
            {
              // Path is too long
//              return printError(printer, "UNSUPPORTED", "The following path is invalid: " + path);
            }
          }
          else
          {
            // Try to handle the call
            call = first;
          }

//          if (incoming.request_type == "GET")
//            result = handleCall(printer, *outgoing.m_out, path, incoming.queries, call, device);
//          else
//            result = handlePut(printer, *outgoing.m_out, path, incoming.queries, call, device);
        }
      }
      catch (exception &e)
      {
//        printError(printer, "SERVER_EXCEPTION", (string)e.what());
      }

      return result;
    }
  }
}
