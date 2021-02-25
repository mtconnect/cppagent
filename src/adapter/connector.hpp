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

#include "utilities.hpp"
#include <condition_variable>

#include <dlib/server.h>
#include <dlib/sockets.h>

#include <chrono>
#include <mutex>
#include <thread>

#define HEARTBEAT_FREQ 60000

using namespace dlib;

namespace mtconnect
{
  namespace adapter
  {
    class Connector
    {
    public:
      // Instantiate the server by assigning it a server and port/
      Connector(std::string server, unsigned int port,
                std::chrono::seconds legacyTimout = std::chrono::seconds {600});

      // Virtual desctructor
      virtual ~Connector();

      // Blocking call to connect to the server/port
      // Put data from the socket in the string buffer
      //
      void connect();

      // Abstract method to handle what to do with each line of data from Socket
      virtual void processData(const std::string &data) = 0;
      virtual void protocolCommand(const std::string &data) = 0;

      // The connected state of this connection
      bool isConnected() const { return m_connected; }

      // Method called when connection is lost.
      virtual void connecting() = 0;
      virtual void disconnected() = 0;
      virtual void connected() = 0;

      // heartbeats
      bool                      heartbeats() const { return m_heartbeats; }
      std::chrono::milliseconds heartbeatFrequency() const { return m_heartbeatFrequency; }

      // Collect data and until it is \n terminated
      void parseBuffer(const char *buffer);

      // Send a command to the adapter
      void sendCommand(const std::string &command);

      unsigned int       getPort() const { return m_port; }
      const std::string &getServer() const { return m_server; }

      std::chrono::seconds getLegacyTimeout() const
      {
        return std::chrono::duration_cast<std::chrono::seconds>(m_legacyTimeout);
      }

      void setRealTime(bool realTime = true) { m_realTime = realTime; }

    protected:
      void startHeartbeats(const std::string &buf);
      void close();

    protected:
      // Name of the server to connect to
      std::string m_server;

      // Connection
      dlib::scoped_ptr<dlib::connection> m_connection;

      // The port number to connect to
      unsigned int m_port;
      unsigned int m_localPort;

      // The string buffer to hold the data from socket
      std::string m_buffer;

      // The connected state of this connector
      bool m_connected;

      // Priority boost
      bool m_realTime;

      // Heartbeats
      bool                      m_heartbeats = false;
      std::chrono::milliseconds m_heartbeatFrequency = std::chrono::milliseconds {HEARTBEAT_FREQ};
      std::chrono::milliseconds m_legacyTimeout = std::chrono::milliseconds {600000};
      std::chrono::time_point<std::chrono::system_clock> m_lastHeartbeat;
      std::chrono::time_point<std::chrono::system_clock> m_lastSent;

      std::mutex m_commandLock;
      bool       m_connectionActive;
      std::mutex m_connectionMutex;
      ;
      std::condition_variable m_connectionCondition;

    private:
      // Size of buffer to read at a time from the socket
      static const unsigned int SOCKET_BUFFER_SIZE = 8192;
    };
  }  // namespace adapter
}  // namespace mtconnect
