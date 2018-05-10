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

#pragma once
#include <mutex>

#include "dlib/sockets.h"
#include "dlib/server.h"

#include "globals.hpp"

#define HEARTBEAT_FREQ 60000

using namespace dlib;

class Connector
{
public:
	// Instantiate the server by assigning it a server and port/
	Connector(const std::string &server, unsigned int port, int legacyTimout = 600);

	// Virtual desctructor
	virtual ~Connector();

	//Blocking call to connect to the server/port
	//Put data from the socket in the string buffer
	//
	void connect();

	// Abstract method to handle what to do with each line of data from Socket
	virtual void processData(const std::string &data) = 0;
	virtual void protocolCommand(const std::string &data) = 0;

	// The connected state of this connection
	bool isConnected() const {
		return m_connected; }

	// Method called when connection is lost.
	virtual void disconnected() = 0;
	virtual void connected() = 0;

	// heartbeats
	bool heartbeats() const {
		return m_heartbeats; }
	int heartbeatFrequency() const {
		return m_heartbeatFrequency; }

	// Collect data and until it is \n terminated
	void parseBuffer(const char *buffer);

	// Send a command to the adapter
	void sendCommand(const std::string &command);

	unsigned int getPort() const {
		return m_port; }
	const std::string &getServer() const {
		return m_server; }

	int getLegacyTimeout() const {
		return m_legacyTimeout / 1000; }

	void setRealTime(bool realTime = true) {
		m_realTime = realTime; }


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
	bool m_heartbeats = false;
	int m_heartbeatFrequency = HEARTBEAT_FREQ;
	int m_legacyTimeout = 600000;
	dlib::uint64 m_lastHeartbeat;
	dlib::uint64 m_lastSent;

	std::mutex m_commandLock;

	bool m_connectActive;
	dlib::mutex *m_connectionMutex;
	dlib::signaler *m_connectionClosed;

private:
	// Size of buffer to read at a time from the socket
	static const unsigned int SOCKET_BUFFER_SIZE = 8192;
};
