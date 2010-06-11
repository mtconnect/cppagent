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

#include "connector.hpp"

using namespace std;

/* Connector public methods */
Connector::Connector(const string& server, unsigned int port)
: mConnected(false), mHeartbeats(false)
{
  mServer = server;
  mPort = port;
}

Connector::~Connector()
{
}

void Connector::connect()
{
  mConnected = false;
  const char *ping = "* PING\n";

  try
  {
    // Connect to server:port, failure will throw dlib::socket_error exception
    // Using a smart pointer to ensure connection is deleted if exception thrown
    mConnection.reset(dlib::connect(mServer, mPort));

    // Check to see if this connection supports heartbeats.
    int status = mConnection->write(ping, strlen(ping));
    if (status < 0)
    {
      logEvent("Connector::connect", "Could not write heartbeat  " + intToString(status));
      close();
      return;
    }
    
    // If we have heartbeats, make sure we receive something every
    // freq milliseconds.
    dlib::timestamper stamper;
    mLastSent = mLastHeartbeat = stamper.get_timestamp();
    
    // Make sure connection buffer is clear
    mBuffer.clear();
    
    // Socket buffer to put the extracted data into
    char sockBuf[SOCKET_BUFFER_SIZE + 1];
    
    // Keep track of the status return, else status = character bytes read
    // Assuming it always enters the while loop, it should never be 1
    mConnected = true;

    
    // Read from the socket, read is a blocking call
    while (true)
    {
      uint64 now;
      if (mHeartbeats)
      {
        now = stamper.get_timestamp();
        int timeout = (int) mHeartbeatFrequency - ((int) (now - mLastSent) / 1000);
        if (timeout < 0)
          timeout = 1;
        status = mConnection->read(sockBuf, SOCKET_BUFFER_SIZE, timeout);
      }
      else
      {
        status = mConnection->read(sockBuf, SOCKET_BUFFER_SIZE);
      }

      if (status > 0)
      {
        // Give a null terminator for the end of buffer
        sockBuf[status] = '\0';
        parseBuffer(sockBuf);   
      }
      else if (status != TIMEOUT) // We don't stop on heartbeats
      {
        break;
      }

      if (mHeartbeats)
      {
        now = stamper.get_timestamp();
        if ((now - mLastHeartbeat) > (uint64) (mHeartbeatFrequency * 2000))
        {
          logEvent("Connector::connect", "Did not receive heartbeat for over " + intToString(mHeartbeatFrequency * 2));
          break;
        }
        else if ((now - mLastSent) >= (uint64) (mHeartbeatFrequency * 1000)) 
        {
          status = mConnection->write(ping, strlen(ping));
          if (status <= 0)
          {
            logEvent("Connector::connect", "Could not write heartbeat  " + intToString(status));
            break;
          }
          mLastSent = now;
        }
      }
    }
    
    // Code should never get here, if it does, notify the exit status
    logEvent("Connector::connect", "Connection exited with status " + intToString(status));

    close();
  }
  catch (exception & e)
  {
    logEvent("Connector::connect", e.what());
  }
}

void Connector::parseBuffer(const char *aBuffer)
{
  // Append the temporary buffer to the socket buffer
  mBuffer.append(aBuffer);
        
  size_t newLine = mBuffer.find_last_of('\n');
        
  // Check to see if there is even a '\n' in buffer
  if (newLine != string::npos)
  {
    // If the '\n' is not at the end of the buffer, then save the overflow
    string overflow = "";
          
    if (newLine != mBuffer.length() - 1)
    {
      overflow = mBuffer.substr(newLine + 1);
      mBuffer = mBuffer.substr(0, newLine);
    }
          
    // Search the buffer for new complete lines
    stringbuf line;
    stringstream stream(mBuffer, stringstream::in);
          
    while (stream.get(line))
    {
      string buf(line.str());
      // Check for heartbeats
      if (buf[0] == '*')
      {
        if (buf.compare(0, 6, "* PONG") == 0)
        {
          if (!mHeartbeats)
            startHeartbeats(buf);
          else
          {
            dlib::timestamper stamper;
            mLastHeartbeat = stamper.get_timestamp();
          }
        }
        else
        {
          protocolCommand(buf);
        }
      }
      else
      {
        processData(buf);
      }
    }
          
    // Clear buffer/insert overflow data
    mBuffer = overflow;
  }
}

void Connector::startHeartbeats(const string &aArg)
{
  size_t pos = aArg.find_last_of(' ');
  if (pos != string::npos)
  {
    int freq = atoi(aArg.substr(pos + 1).c_str());
    if (freq > 0 && freq < 120000)
    {
      mHeartbeats = true;
      mHeartbeatFrequency = freq;
    }
    else
    {
      logEvent("Connector::startHeartbeats", "Bad heartbeat command " + aArg);
      close();
    }
  }
}

void Connector::close()
{
  if (mConnected && mConnection.get() != NULL)
  {
    // Close the connection gracefully
    mConnection.reset();

    // Call the disconnected callback.
    mConnected = false;
    disconnected();
  }
}

