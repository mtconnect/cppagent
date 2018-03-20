//
// Copyright Copyright 2012, System Insights, Inc.
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
#include "connector.hpp"
#include "dlib/logger.h"

using namespace std;

static dlib::logger sLogger("input.connector");

/* Connector public methods */
Connector::Connector(const string& server, unsigned int port, int aLegacyTimeout)
: mConnected(false), mRealTime(false), mHeartbeats(false), 
  mLegacyTimeout(aLegacyTimeout * 1000), mConnectActive(false)
{
  mServer = server;
  mPort = port;
  mCommandLock = new dlib::mutex;
  mConnectionMutex = new dlib::mutex;
  mConnectionClosed = new dlib::signaler(*mConnectionMutex);
}

Connector::~Connector()
{
  delete mCommandLock;
  delete mConnectionClosed;
  delete mConnectionMutex;
}

class AutoSignal
{
public:
  AutoSignal(dlib::mutex *aMutex, dlib::signaler *aSignal, bool *aVar)
  : m(aMutex), s(aSignal), v(aVar)
  {
    dlib::auto_mutex lock(*m);
    *v = true;
  }
  
  ~AutoSignal()
  {
    dlib::auto_mutex lock(*m);
    *v = false;
    s->signal();
  }
  
  
private:
  dlib::mutex *m;
  dlib::signaler *s;
  bool *v;
};

void Connector::connect()
{
  mConnected = false;
  const char *ping = "* PING\n";
  
  AutoSignal sig(mConnectionMutex, mConnectionClosed, &mConnectActive);

  try
  {
    // Connect to server:port, failure will throw dlib::socket_error exception
    // Using a smart pointer to ensure connection is deleted if exception thrown
    sLogger << LDEBUG << "Connecting to data source: " << mServer << " on port: " << mPort;
    mConnection.reset(dlib::connect(mServer, mPort));
  
	mLocalPort = mConnection->get_local_port();

    // Check to see if this connection supports heartbeats.
	mHeartbeatFrequency = HEARTBEAT_FREQ;
	mHeartbeats = false;
    sLogger << LDEBUG << "(Port:" << mLocalPort << ")" << "Sending initial PING";
    int status = mConnection->write(ping, strlen(ping));
    if (status < 0)
    {
      sLogger << LWARN << "(Port:" << mLocalPort << ")" << "connect: Could not write initial heartbeat: " << intToString(status);
      close();
      return;
    }

    connected();
    
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

    // Boost priority if this is a real-time adapter
    if (mRealTime)
    {
#ifdef _WINDOWS
      HANDLE hand = OpenThread(THREAD_ALL_ACCESS,FALSE,GetCurrentThreadId());
      SetThreadPriority(hand, THREAD_PRIORITY_TIME_CRITICAL);
      CloseHandle(hand);
#else
      struct sched_param param;
      param.sched_priority = 30;
      if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0)
        sLogger << LDEBUG << "Cannot set high thread priority";
#endif
    }
	sLogger << LTRACE << "(Port:" << mLocalPort << ")" << "Heartbeat : " << mHeartbeats;
	sLogger << LTRACE << "(Port:" << mLocalPort << ")" << "Heartbeat Freq: " << mHeartbeatFrequency;
    // Read from the socket, read is a blocking call
    while (mConnected)
    {
      uint64 now;
      now = stamper.get_timestamp();
      int timeout;
	  if (mHeartbeats)
	  {
		  timeout = (int)mHeartbeatFrequency - ((int)(now - mLastSent) / 1000);
		  sLogger << LTRACE << "(Port:" << mLocalPort << ")" << "Heartbeat Send Countdown: " << timeout;
	  }
	  else
	  {
		  timeout = mLegacyTimeout;
		  sLogger << LTRACE << "(Port:" << mLocalPort << ")" << "Legacy Timeout: " << timeout;
	  }

      if (timeout < 0)
        timeout = 1;
      sockBuf[0] = 0;
      
      if (mConnected)
        status = mConnection->read(sockBuf, SOCKET_BUFFER_SIZE, timeout);
      else
      {
        sLogger << LDEBUG << "(Port:" << mLocalPort << ")" << "Connection was closed, exiting adapter connect";
        break;
      }
      
      if (!mConnected) {
        sLogger << LDEBUG << "(Port:" << mLocalPort << ")" << "Connection was closed during read, exiting adapter";
        break;
      }
      
      if (status > 0)
      {
        // Give a null terminator for the end of buffer
        sockBuf[status] = '\0';
        parseBuffer(sockBuf);   
      }
      else if (status == TIMEOUT && !mHeartbeats && ((int) (stamper.get_timestamp() - now) / 1000) >= timeout) 
      {
        // We don't stop on heartbeats, but if we have a legacy timeout, then we stop.
        sLogger << LERROR << "(Port:" << mLocalPort << ")" << "connect: Did not receive data for over: " << (timeout / 1000) << " seconds";
        break;
      }
      else if (status != TIMEOUT) // Something other than timeout occurred
      {
        sLogger << LERROR << "(Port:" << mLocalPort << ")" << "connect: Socket error, disconnecting";
        break;
      }

      if (mHeartbeats)
      {
        now = stamper.get_timestamp();
        if ((now - mLastHeartbeat) > (uint64) (mHeartbeatFrequency * 2000))
        {
          sLogger << LERROR << "(Port:" << mLocalPort << ")" << "connect: Did not receive heartbeat for over: " << (mHeartbeatFrequency * 2);
          break;
        }
        else if ((now - mLastSent) >= (uint64) (mHeartbeatFrequency * 1000)) 
        {
          dlib::auto_mutex lock(*mCommandLock);
          
          sLogger << LDEBUG << "(Port:" << mLocalPort << ")" << "Sending a PING for " << mServer << " on port " << mPort;
          status = mConnection->write(ping, strlen(ping));
          if (status <= 0)
          {
            sLogger << LERROR << "(Port:" << mLocalPort << ")" << "connect: Could not write heartbeat: " << status;
            break;
          }
          mLastSent = now;
        }
      }
    }
    
    sLogger << LERROR << "(Port:" << mLocalPort << ")" << "connect: Connection exited with status: " << status;
    mConnectActive = false;
    close();
  }
  catch (dlib::socket_error &e)
  {
	  sLogger << LWARN << "(Port:" << mLocalPort << ")" << "connect: Socket exception: " << e.what();
  }
  catch (exception & e)
  {
    sLogger << LERROR << "(Port:" << mLocalPort << ")" << "connect: Exception in connect: " << e.what();
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
    string line;
    stringstream stream(mBuffer, stringstream::in);
          
    while (!stream.eof())
    {
      getline(stream, line);
      sLogger << LTRACE << "(Port:" << mLocalPort << ")" << "Received line: '" << line << '\'';
      
      if (line.empty()) continue;

      // Check for heartbeats
      if (line[0] == '*')
      {
        if (line.compare(0, 6, "* PONG") == 0)
        {
          if (sLogger.level().priority <= LDEBUG.priority) {
            sLogger << LDEBUG << "(Port:" << mLocalPort << ")" << "Received a PONG for " << mServer << " on port " << mPort;
            dlib::timestamper stamper;
            int delta = (int) ((stamper.get_timestamp() - mLastHeartbeat) / 1000ull);
            sLogger << LDEBUG << "(Port:" << mLocalPort << ")" << "    Time since last heartbeat: " << delta << "ms";
          }
          if (!mHeartbeats)
            startHeartbeats(line);
          else
          {
            dlib::timestamper stamper;
            mLastHeartbeat = stamper.get_timestamp();
          }
        }
        else
        {
          protocolCommand(line);
        }
      }
      else
      {
        processData(line);
      }
    }
          
    // Clear buffer/insert overflow data
    mBuffer = overflow;
  }
}

void Connector::sendCommand(const string &aCommand)
{
  dlib::auto_mutex lock(*mCommandLock);

  if (mConnected) {
    string command = "* " + aCommand + "\n";
    int status = mConnection->write(command.c_str(), command.length());
    if (status <= 0)
    {
      sLogger << LWARN << "(Port:" << mLocalPort << ")" << "sendCommand: Could not write command: '" << aCommand << "' - "
              << intToString(status);
    }
  }
}

void Connector::startHeartbeats(const string &aArg)
{
  size_t pos;
  if (aArg.length() > 7 && aArg[6] == ' ' && (pos = aArg.find_first_of("0123456789", 7)) != string::npos)  {
    int freq = atoi(aArg.substr(pos).c_str());
    
    // Make the maximum timeout 30 minutes.
    if (freq > 0 && freq < 30 * 60 * 1000)
    {
      sLogger << LDEBUG << "(Port:" << mLocalPort << ")" << "Received PONG, starting heartbeats every " << freq << "ms";
      mHeartbeats = true;
      mHeartbeatFrequency = freq;
    }
    else
    {
      sLogger << LERROR << "(Port:" << mLocalPort << ")" << "startHeartbeats: Bad heartbeat frequency " << aArg << ", ignoring";
    }
  }
  else
  {
    sLogger << LERROR << "(Port:" << mLocalPort << ")" << "startHeartbeats: Bad heartbeat command " << aArg << ", ignoring";
  }
}

void Connector::close()
{
  dlib::auto_mutex lock(*mConnectionMutex);

  if (mConnected && mConnection.get() != NULL)
  {
    // Shutdown the socket and close the connection.
    mConnected = false;
    mConnection->shutdown();
    
    sLogger << LWARN << "(Port:" << mLocalPort << ")" << "Waiting for connect method to exit and signal connection closed";
    if (mConnectActive)
      mConnectionClosed->wait();
    
    // Destroy the connection object.
    mConnection.reset();

    disconnected();
  }
}

