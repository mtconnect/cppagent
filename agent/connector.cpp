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
Connector::Connector(const string &server, unsigned int port, int aLegacyTimeout)
	: m_connected(false), m_realTime(false), m_heartbeats(false),
	  m_legacyTimeout(aLegacyTimeout * 1000), m_connectActive(false)
{
	m_server = server;
	m_port = port;
	m_commandLock = new dlib::mutex;
	m_connectionMutex = new dlib::mutex;
	m_connectionClosed = new dlib::signaler(*m_connectionMutex);
}

Connector::~Connector()
{
	delete m_commandLock;
	delete m_connectionClosed;
	delete m_connectionMutex;
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
	m_connected = false;
	const char *ping = "* PING\n";

	AutoSignal sig(m_connectionMutex, m_connectionClosed, &m_connectActive);

	try
	{
	// Connect to server:port, failure will throw dlib::socket_error exception
	// Using a smart pointer to ensure connection is deleted if exception thrown
	sLogger << LDEBUG << "Connecting to data source: " << m_server << " on port: " << m_port;
	m_connection.reset(dlib::connect(m_server, m_port));

	m_localPort = m_connection->get_local_port();

	// Check to see if this connection supports heartbeats.
	m_heartbeatFrequency = HEARTBEAT_FREQ;
	m_heartbeats = false;
	sLogger << LDEBUG << "(Port:" << m_localPort << ")" << "Sending initial PING";
	int status = m_connection->write(ping, strlen(ping));

	if (status < 0)
	{
		sLogger << LWARN << "(Port:" << m_localPort << ")" <<
			"connect: Could not write initial heartbeat: " << intToString(status);
		close();
		return;
	}

	connected();

	// If we have heartbeats, make sure we receive something every
	// freq milliseconds.
	dlib::timestamper stamper;
	m_lastSent = m_lastHeartbeat = stamper.get_timestamp();

	// Make sure connection buffer is clear
	m_buffer.clear();

	// Socket buffer to put the extracted data into
	char sockBuf[SOCKET_BUFFER_SIZE + 1];

	// Keep track of the status return, else status = character bytes read
	// Assuming it always enters the while loop, it should never be 1
	m_connected = true;

	// Boost priority if this is a real-time adapter
	if (m_realTime)
	{
#ifdef _WINDOWS
		HANDLE hand = OpenThread(THREAD_ALL_ACCESS, FALSE, GetCurrentThreadId());
		SetThreadPriority(hand, THREAD_PRIORITY_TIME_CRITICAL);
		CloseHandle(hand);
#else
		struct sched_param param;
		param.sched_priority = 30;

		if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0)
		sLogger << LDEBUG << "Cannot set high thread priority";

#endif
	}

	sLogger << LTRACE << "(Port:" << m_localPort << ")" << "Heartbeat : " << m_heartbeats;
	sLogger << LTRACE << "(Port:" << m_localPort << ")" << "Heartbeat Freq: " << m_heartbeatFrequency;

	// Read from the socket, read is a blocking call
	while (m_connected)
	{
		uint64 now;
		now = stamper.get_timestamp();
		int timeout;

		if (m_heartbeats)
		{
		timeout = (int)m_heartbeatFrequency - ((int)(now - m_lastSent) / 1000);
		sLogger << LTRACE << "(Port:" << m_localPort << ")" << "Heartbeat Send Countdown: " << timeout;
		}
		else
		{
		timeout = m_legacyTimeout;
		sLogger << LTRACE << "(Port:" << m_localPort << ")" << "Legacy Timeout: " << timeout;
		}

		if (timeout < 0)
		timeout = 1;

		sockBuf[0] = 0;

		if (m_connected)
		status = m_connection->read(sockBuf, SOCKET_BUFFER_SIZE, timeout);
		else
		{
		sLogger << LDEBUG << "(Port:" << m_localPort << ")" <<
			"Connection was closed, exiting adapter connect";
		break;
		}

		if (!m_connected)
		{
		sLogger << LDEBUG << "(Port:" << m_localPort << ")" <<
			"Connection was closed during read, exiting adapter";
		break;
		}

		if (status > 0)
		{
		// Give a null terminator for the end of buffer
		sockBuf[status] = '\0';
		parseBuffer(sockBuf);
		}
		else if (status == TIMEOUT && !m_heartbeats &&
			 ((int)(stamper.get_timestamp() - now) / 1000) >= timeout)
		{
		// We don't stop on heartbeats, but if we have a legacy timeout, then we stop.
		sLogger << LERROR << "(Port:" << m_localPort << ")" << "connect: Did not receive data for over: "
			<< (timeout / 1000) << " seconds";
		break;
		}
		else if (status != TIMEOUT) // Something other than timeout occurred
		{
		sLogger << LERROR << "(Port:" << m_localPort << ")" << "connect: Socket error, disconnecting";
		break;
		}

		if (m_heartbeats)
		{
		now = stamper.get_timestamp();

		if ((now - m_lastHeartbeat) > (uint64)(m_heartbeatFrequency * 2000))
		{
			sLogger << LERROR << "(Port:" << m_localPort << ")" <<
				"connect: Did not receive heartbeat for over: " << (m_heartbeatFrequency * 2);
			break;
		}
		else if ((now - m_lastSent) >= (uint64)(m_heartbeatFrequency * 1000))
		{
			dlib::auto_mutex lock(*m_commandLock);

			sLogger << LDEBUG << "(Port:" << m_localPort << ")" << "Sending a PING for " << m_server <<
				" on port " << m_port;
			status = m_connection->write(ping, strlen(ping));

			if (status <= 0)
			{
			sLogger << LERROR << "(Port:" << m_localPort << ")" << "connect: Could not write heartbeat: " <<
				status;
			break;
			}

			m_lastSent = now;
		}
		}
	}

	sLogger << LERROR << "(Port:" << m_localPort << ")" << "connect: Connection exited with status: "
		<< status;
	m_connectActive = false;
	close();
	}
	catch (dlib::socket_error &e)
	{
	sLogger << LWARN << "(Port:" << m_localPort << ")" << "connect: Socket exception: " << e.what();
	}
	catch (exception &e)
	{
	sLogger << LERROR << "(Port:" << m_localPort << ")" << "connect: Exception in connect: " <<
		e.what();
	}
}

void Connector::parseBuffer(const char *aBuffer)
{
	// Append the temporary buffer to the socket buffer
	m_buffer.append(aBuffer);

	size_t newLine = m_buffer.find_last_of('\n');

	// Check to see if there is even a '\n' in buffer
	if (newLine != string::npos)
	{
	// If the '\n' is not at the end of the buffer, then save the overflow
	string overflow = "";

	if (newLine != m_buffer.length() - 1)
	{
		overflow = m_buffer.substr(newLine + 1);
		m_buffer = m_buffer.substr(0, newLine);
	}

	// Search the buffer for new complete lines
	string line;
	stringstream stream(m_buffer, stringstream::in);

	while (!stream.eof())
	{
		getline(stream, line);
		sLogger << LTRACE << "(Port:" << m_localPort << ")" << "Received line: '" << line << '\'';

		if (line.empty()) continue;

		// Check for heartbeats
		if (line[0] == '*')
		{
		if (line.compare(0, 6, "* PONG") == 0)
		{
			if (sLogger.level().priority <= LDEBUG.priority)
			{
			sLogger << LDEBUG << "(Port:" << m_localPort << ")" << "Received a PONG for " << m_server <<
				" on port " << m_port;
			dlib::timestamper stamper;
			int delta = (int)((stamper.get_timestamp() - m_lastHeartbeat) / 1000ull);
			sLogger << LDEBUG << "(Port:" << m_localPort << ")" << "    Time since last heartbeat: " << delta
				<< "ms";
			}

			if (!m_heartbeats)
			startHeartbeats(line);
			else
			{
			dlib::timestamper stamper;
			m_lastHeartbeat = stamper.get_timestamp();
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
	m_buffer = overflow;
	}
}

void Connector::sendCommand(const string &aCommand)
{
	dlib::auto_mutex lock(*m_commandLock);

	if (m_connected)
	{
	string command = "* " + aCommand + "\n";
	int status = m_connection->write(command.c_str(), command.length());

	if (status <= 0)
	{
		sLogger << LWARN << "(Port:" << m_localPort << ")" << "sendCommand: Could not write command: '" <<
			aCommand << "' - "
			<< intToString(status);
	}
	}
}

void Connector::startHeartbeats(const string &aArg)
{
	size_t pos;

	if (aArg.length() > 7 && aArg[6] == ' ' &&
	(pos = aArg.find_first_of("0123456789", 7)) != string::npos)
	{
	int freq = atoi(aArg.substr(pos).c_str());

	// Make the maximum timeout 30 minutes.
	if (freq > 0 && freq < 30 * 60 * 1000)
	{
		sLogger << LDEBUG << "(Port:" << m_localPort << ")" << "Received PONG, starting heartbeats every "
			<< freq << "ms";
		m_heartbeats = true;
		m_heartbeatFrequency = freq;
	}
	else
	{
		sLogger << LERROR << "(Port:" << m_localPort << ")" << "startHeartbeats: Bad heartbeat frequency "
			<< aArg << ", ignoring";
	}
	}
	else
	{
	sLogger << LERROR << "(Port:" << m_localPort << ")" << "startHeartbeats: Bad heartbeat command " <<
		aArg << ", ignoring";
	}
}

void Connector::close()
{
	dlib::auto_mutex lock(*m_connectionMutex);

	if (m_connected && m_connection.get() != NULL)
	{
	// Shutdown the socket and close the connection.
	m_connected = false;
	m_connection->shutdown();

	sLogger << LWARN << "(Port:" << m_localPort << ")" <<
		"Waiting for connect method to exit and signal connection closed";

	if (m_connectActive)
		m_connectionClosed->wait();

	// Destroy the connection object.
	m_connection.reset();

	disconnected();
	}
}

