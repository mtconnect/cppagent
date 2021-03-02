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


namespace mtconnect
{
  namespace http_server
  {
    using namespace std;
    using namespace dlib;

    static dlib::logger g_logger("HttpServer");


    Server::Server(unsigned short port, const std::string &inter, const ConfigOptions& options)
    {
      address = net::ip::make_address("0.0.0.0");
      mPort = 5023;
      m_errorFunction = [](const std::string &accepts, Response &response, const std::string &msg,
                           const ResponseCode code) {
        response.writeResponse(msg, code);
        return true;
      };
    }

    void Server::start()
    {
      try
      {
        run = true;
        //do_session();
        httpProcess = new std::thread(&Server::listen, this );
        httpProcess->join();
      }
      catch (dlib::socket_error &e)
      {
        g_logger << LFATAL << "Cannot start server: " << e.what();
        std::exit(1);
      }
    }

    // Handles an HTTP server connection
    void Server::listen()
    {
      bool close = false;
      beast::error_code ec;

      // This buffer is required to persist across reads
      beast::flat_buffer buffer;
      // The io_context is required for all I/O
      net::io_context ioc{1};

//            if(enableSSL) {
//                // The SSL context is required, and holds certificates
//                ssl::context ctx{ssl::context::tlsv12};
//
//                // This holds the self-signed certificate used by the server
//                load_server_certificate(ctx);
//            }

      // The acceptor receives incoming connections
      tcp::acceptor acceptor{ioc, {address, mPort}};

      tcp::socket socket{ioc};

      while (run) {
        // Block until we get a connection
        acceptor.accept(socket);

        //beast::tcp_stream temp(std::move(socket));

        // This lambda is used to send messages
        send_lambda<tcp::socket> lambda{socket, close, ec};
        //beast::tcp_stream out(std::move(socket));

        for (;;) {
          // Read a request

          http::request<http::string_body> req;
          http::read(socket, buffer, req, ec);

          if (ec == http::error::end_of_stream)
            break;

          //http::response<http::string_body, http::fields> reqRes = make_response(req);
          // Send the response
          Routing::Request request = getRequest(req, socket);
          //template<class Stream> T
          //beast::tcp_stream stream_(std::move(socket));
          std::ostream out{0};//{std::move(stream_)};
          Response response(out);
          //http_server::Response response();
          bool res{true};
          try
          {
//            if (!dispatch(request, response))
//            {
//              stringstream msg;
//              msg << "Error processing request from: " << request.m_foreignIp << " - "
//                  << "No matching route for: " << request.m_verb << " " << request.m_path;
//              g_logger << LERROR << msg.str();
//
//              if (m_errorFunction)
//                m_errorFunction(request.m_accepts, response, msg.str(), BAD_REQUEST);
//              res = false;
//            }
          }
          catch (RequestError &e)
          {
            g_logger << LERROR << "Error processing request from: " << request.m_foreignIp << " - "
                     << e.what();
            //response.writeResponse(e.m_body, e.m_contentType, e.m_code);
            res = false;
          }
          catch (ParameterError &e)
          {
            stringstream msg;
            msg << "Parameter Error processing request from: " << request.m_foreignIp << " - "
                << e.what();
            g_logger << LERROR << msg.str();

            //if (m_errorFunction)
              //m_errorFunction(request.m_accepts, response, msg.str(), BAD_REQUEST);
            res = false;
          }


          handle_request(std::move(req), lambda);
          if (ec)
            return fail(ec, "write");
          if (close) {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            break;
          }
        }

        // Send a TCP shutdown
        socket.shutdown(tcp::socket::shutdown_send, ec);
      }
      // At this point the connection is closed gracefully
    }

    void accept() {};
    void Server::send_response(
        http::status status,
        std::string const& error, tcp::socket socket)
    {
        http::response<http::string_body> string_response;
//      string_response_.emplace(
//          std::piecewise_construct,
//          std::make_tuple(),
//          std::make_tuple(alloc_));
//
//      string_response_->result(status);
//      string_response_->keep_alive(false);
//      string_response_->set(http::field::server, "Beast");
//      string_response_->set(http::field::content_type, "text/plain");
//      string_response_->body() = error;
//      string_response_->prepare_payload();
//
//      string_serializer_.emplace(*string_response_);
//
//      http::write(
//          socket,
//          *string_serializer_
//          [this](beast::error_code ec, std::size_t)
//          {
//            socket.shutdown(tcp::socket::shutdown_send, ec);
//            string_serializer_.reset();
//            string_response_.reset();
//            accept();
//          }
//          );
    }

    //Parse http::request and return
    Routing::Request Server::getRequest(const http::request<http::string_body>& req, const tcp::socket& socket)
    {
      Routing::Request request;

      try
      {
        string queries{std::string().empty()};
        auto path = static_cast<std::string>(req.target().data());
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
        request.m_body = req.target().data();
        request.m_foreignIp = socket.remote_endpoint().address().to_string();
        request.m_foreignPort = socket.remote_endpoint().port();
        request.m_accepts = req.find(http::field::accept)->value().data();
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
            std:string string1 = tmpStr.substr(0,ptr-1);
            tmpStr = tmpStr.substr(ptr+1);
            ptr = string1.find('=');
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

      response.flush();
      return res;
    }

    template<class Body, class Allocator, class Send>
    void Server::handle_request(http::request<Body, http::basic_fields<Allocator>> &&req,
    Send &&send)
    {
      // Returns a bad request response
      auto const bad_request =
          [&req](beast::string_view why) {
            http::response<http::string_body> res{http::status::bad_request, req.version()};
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            res.body() = std::string(why);
            res.prepare_payload();
            return res;
          };

      // Returns a not found response
      auto const not_found =
          [&req](beast::string_view target) {
            http::response<http::string_body> res{http::status::not_found, req.version()};
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            res.body() = "The resource '" + std::string(target) + "' was not found.";
            res.prepare_payload();
            return res;
          };

      // Returns a server error response
      auto const server_error =
          [&req](beast::string_view what) {
            http::response<http::string_body> res{http::status::internal_server_error, req.version()};
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            res.body() = "An error occurred: '" + std::string(what) + "'";
            res.prepare_payload();
            return res;
          };
//
//      // Make sure we can handle the method
//      if (req.method() != http::verb::get &&
//          req.method() != http::verb::head)
//        return send(bad_request("Unknown HTTP-method"));
//
//      // Request path must be absolute and not contain "..".
//      if (req.target().empty() ||
//          req.target()[0] != '/' ||
//          req.target().find("..") != beast::string_view::npos)
//        return send(bad_request("Illegal request-target"));
//
//      // Build the path to the requested file
//      std::string path = path_cat(doc_root, req.target());
//      if (req.target().back() == '/')
//        path.append("index.html");

      // Attempt to open the file
      beast::error_code ec;
      http::file_body::value_type body;
      body.open("device.xml", beast::file_mode::scan, ec);

      //Handle the case where the file doesn't exist
      if (ec == beast::errc::no_such_file_or_directory)
        return send(not_found(req.target()));

      // Handle an unknown error
      //if (ec)
      //  return send(server_error(ec.message()));

      // Cache the size since we need it after the move
      auto const size = body.size();

      // Respond to HEAD request
      if (req.method() == http::verb::head) {
        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/xml");
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
      }

      string queries{std::string().empty()};
      auto path = static_cast<std::string>(req.target().data());
      auto qp = path.find_first_of('?');
      if (qp != string::npos){
        queries = path.substr(qp+1);
        path.erase(qp);
      }

      if (std::strcmp(path.c_str(), "/probe") == 0)
      {
        // Respond to GET request
        http::response<http::file_body> res{
            std::piecewise_construct,
            std::make_tuple(std::move(body)),
            std::make_tuple(http::status::ok, req.version())};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/xml");
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
      }
      else
      {
        //auto const std::string().size( textSize = req.target().data());
        auto const textSize = req.target().size();
        http::response<http::string_body> res{
            std::piecewise_construct,
            std::make_tuple(std::move(req.target().data())),
            std::make_tuple(http::status::ok, req.version())};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/plain");
        res.content_length(textSize);
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
      }
    }


  //}

//------------------------------------------------------------------------------

// Report a failure
    void Server::fail(beast::error_code ec, char const *what) {
      std::cerr << what << ": " << ec.message() << "\n";
    }

  }  // namespace http_server
}  // namespace mtconnect
