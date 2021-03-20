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

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>

#include "utilities.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/array.hpp>
#include <boost/asio/detached.hpp>


#include <chrono>
#include <mutex>
#include <thread>

#define HEARTBEAT_FREQ 60000

namespace mtconnect
{
  namespace adapter
  {
    class Connector
    {
    public:
      // Instantiate the server by assigning it a server and port/
      Connector(boost::asio::io_context &context,
                std::string server, unsigned int port,
                std::chrono::seconds legacyTimout = std::chrono::seconds {600},
                std::chrono::seconds reconnectInterval = std::chrono::seconds {10});

      // Virtual desctructor
      virtual ~Connector();

      // Blocking call to connect to the server/port
      // Put data from the socket in the string buffer
      //
      virtual bool start();
      virtual bool resolve();
      virtual bool connect();

      // Abstract method to handle what to do with each line of data from Socket
      virtual void processData(const std::string &data) = 0;
      virtual void protocolCommand(const std::string &data) = 0;
      
      // Set Reconnect intervals
      void setReconnectInterval(std::chrono::milliseconds interval)
      {
        m_reconnectInterval = interval;
      }
      std::chrono::milliseconds getReconnectInterval() const { return m_reconnectInterval; }



      // The connected state of this connection
      bool isConnected() const { return m_connected; }

      // Method called when connection is lost.
      virtual void connecting() = 0;
      virtual void disconnected() = 0;
      virtual void connected() = 0;

      // heartbeats
      bool heartbeats() const { return m_heartbeats; }
      std::chrono::milliseconds heartbeatFrequency() const { return m_heartbeatFrequency; }

      // Collect data and until it is \n terminated
      void parseBuffer(const char *buffer);

      // Send a command to the adapter
      void sendCommand(const std::string &command);

      unsigned int getPort() const { return m_port; }
      const std::string &getServer() const { return m_server; }

      std::chrono::seconds getLegacyTimeout() const
      {
        return std::chrono::duration_cast<std::chrono::seconds>(m_legacyTimeout);
      }

      void setRealTime(bool realTime = true) { m_realTime = realTime; }

    protected:
      void close();
      void reconnect();
      void connected(const boost::system::error_code& error,
                     boost::asio::ip::tcp::resolver::iterator it);
      void writer(boost::system::error_code ec, std::size_t length);
      void reader(boost::system::error_code ec, std::size_t length);
      void parseSocketBuffer();
      void startHeartbeats(const std::string &buf);
      void heartbeat(boost::system::error_code ec);
      void setReceiveTimeout();

    protected:
      // Name of the server to connect to
      std::string m_server;

      // Connection
      boost::asio::io_context::strand m_strand;
      boost::asio::ip::tcp::socket m_socket;
      boost::asio::ip::tcp::endpoint m_endpoint;
      boost::asio::ip::tcp::resolver::results_type m_results;
      
      // For reentry
      boost::asio::coroutine m_coroutine;

      // The port number to connect to
      unsigned int m_port;
      unsigned int m_localPort;

      boost::asio::streambuf m_incoming;
      boost::asio::streambuf m_outgoing;

      // Some timeers
      boost::asio::steady_timer m_timer;
      boost::asio::steady_timer m_heartbeatTimer;
      boost::asio::steady_timer m_receiveTimeout;

      // The connected state of this connector
      bool m_connected;

      // Priority boost
      bool m_realTime;

      // Heartbeats
      bool m_heartbeats = false;
      std::chrono::milliseconds m_heartbeatFrequency = std::chrono::milliseconds {HEARTBEAT_FREQ};
      std::chrono::milliseconds m_legacyTimeout;
      std::chrono::milliseconds m_reconnectInterval;
      std::chrono::milliseconds m_receiveTimeLimit;
    };
  }  // namespace adapter
}  // namespace mtconnect
