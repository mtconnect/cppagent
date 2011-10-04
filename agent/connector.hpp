/*
* Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the AMT nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* DISCLAIMER OF WARRANTY. ALL MTCONNECT MATERIALS AND SPECIFICATIONS PROVIDED
* BY AMT, MTCONNECT OR ANY PARTICIPANT TO YOU OR ANY PARTY ARE PROVIDED "AS IS"
* AND WITHOUT ANY WARRANTY OF ANY KIND. AMT, MTCONNECT, AND EACH OF THEIR
* RESPECTIVE MEMBERS, OFFICERS, DIRECTORS, AFFILIATES, SPONSORS, AND AGENTS
* (COLLECTIVELY, THE "AMT PARTIES") AND PARTICIPANTS MAKE NO REPRESENTATION OR
* WARRANTY OF ANY KIND WHATSOEVER RELATING TO THESE MATERIALS, INCLUDING, WITHOUT
* LIMITATION, ANY EXPRESS OR IMPLIED WARRANTY OF NONINFRINGEMENT,
* MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. 

* LIMITATION OF LIABILITY. IN NO EVENT SHALL AMT, MTCONNECT, ANY OTHER AMT
* PARTY, OR ANY PARTICIPANT BE LIABLE FOR THE COST OF PROCURING SUBSTITUTE GOODS
* OR SERVICES, LOST PROFITS, LOSS OF USE, LOSS OF DATA OR ANY INCIDENTAL,
* CONSEQUENTIAL, INDIRECT, SPECIAL OR PUNITIVE DAMAGES OR OTHER DIRECT DAMAGES,
* WHETHER UNDER CONTRACT, TORT, WARRANTY OR OTHERWISE, ARISING IN ANY WAY OUT OF
* THIS AGREEMENT, USE OR INABILITY TO USE MTCONNECT MATERIALS, WHETHER OR NOT
* SUCH PARTY HAD ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.
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

