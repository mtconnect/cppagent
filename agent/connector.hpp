/*
 * Copyright Copyright 2012, System Insights, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef CONNECTOR_HPP
#define CONNECTOR_HPP

#include "dlib/sockets.h"
#include "dlib/server.h"

#include "globals.hpp"

using namespace dlib;

class Connector
{
public:
  /* Instantiate the server by assigning it a server and port */
  Connector(const std::string& server, unsigned int port, int aLegacyTimout = 600);
  
  /* Virtual desctructor */
  virtual ~Connector();
  
  /**
   *  Blocking call to connect to the server/port
   *  Put data from the socket in the string buffer
   */
  void connect();
  
  /* Abstract method to handle what to do with each line of data from Socket */
  virtual void processData(const std::string& data) = 0;
  virtual void protocolCommand(const std::string& data) = 0;
  
  /* The connected state of this connection */
  bool isConnected() { return mConnected; }
  
  /* Method called when connection is lost. */
  virtual void disconnected() = 0;
  virtual void connected() = 0;

  /* heartbeats */
  bool heartbeats() const { return mHeartbeats; }
  int heartbeatFrequency() const { return mHeartbeatFrequency; }

  // Collect data and until it is \n terminated
  void parseBuffer(const char *aBuffer);
  
  // Send a command to the adapter
  void sendCommand(const std::string &aCommand);

  unsigned int getPort() const { return mPort; }
  const std::string &getServer() const { return mServer; }
  
  int getLegacyTimeout() { return mLegacyTimeout / 1000; }

  void setRealTime(bool aV = true) { mRealTime = aV; }


protected:
  void startHeartbeats(const std::string &buf);
  void close();

protected:
  /* Name of the server to connect to */
  std::string mServer;

  // Connection
  dlib::scoped_ptr<dlib::connection> mConnection;  
  
  /* The port number to connect to */
  unsigned int mPort;
  
  /* The string buffer to hold the data from socket */
  std::string mBuffer;
  
  /* The connected state of this connector */
  bool mConnected;

  /* Priority boost */
  bool mRealTime;
  
  // Heartbeats
  bool mHeartbeats;
  int mHeartbeatFrequency;
  int mLegacyTimeout;
  dlib::uint64 mLastHeartbeat;
  dlib::uint64 mLastSent;
  
  dlib::mutex *mCommandLock;
  
private:
  /* Size of buffer to read at a time from the socket */  
  static const unsigned int SOCKET_BUFFER_SIZE = 8192;
};

#endif

