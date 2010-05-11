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
  try
  {
    // Connect to server:port, failure will throw dlib::socket_error exception
    // Using a smart pointer to ensure connection is deleted if exception thrown
    mConnection = dlib::connect(mServer, mPort);

    // Check to see if this connection supports heartbeats.
    
    // Make sure connection buffer is clear
    mBuffer.clear();
    
    // Socket buffer to put the extracted data into
    char sockBuf[SOCKET_BUFFER_SIZE + 1];
    
    // Keep track of the status return, else status = character bytes read
    // Assuming it always enters the while loop, it should never be 1
    long status = 1;
    mConnected = true;
    
    // Read from the socket, read is a blocking call
    while ((status = mConnection->read(sockBuf, SOCKET_BUFFER_SIZE)) > 0)
    { 
      // Give a null terminator for the end of buffer
      sockBuf[status] = '\0';
      
      // Append the temporary buffer to the socket buffer
      mBuffer += sockBuf;
      
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
        char buf[LINE_BUFFER_SIZE];
        stringstream stream (mBuffer);
        
        while (stream.getline(buf, LINE_BUFFER_SIZE))
        {
	  // Check for heartbeats
	  if (buf[0] == '*')
	  {
	    if (strncmp(buf.c_str(), "* PONG", 6) == 0)
	    {
	      if (!mHeartbeats)
		startHeartbeats(buf);
	    }
	    else
	    {
	      protocolCommand(buf);
	    }
	  }
	  else
	    processData(buf);
        }
        
        // Clear buffer/insert overflow data
        mBuffer = overflow;
      }
    }
    
    // Code should never get here, if it does, notify the exit status
    logEvent("Connector::connect", "Connection exited with status " + status);

    close();
  }
  catch (exception & e)
  {
    logEvent("Connector::connect", e.what());
  }
  
}

void Connector::startHeartbeats(const string aArg)
{
  size_t pos = aArg.find_last_of(' ');
  if (pos != npos)
  {
    string freq = atoi(aArg.substr(pos + 1));
    if (freq > 0 && freq < 20000)
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
    close_gracefully(mConnection);
    mConnection.reset();

    // Call the disconnected callback.
    mConnected = false;
    disconnected();
  }
}

