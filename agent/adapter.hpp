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

#include <string>
#include <sstream>
#include <stdexcept>
#include <set>

#include "dlib/sockets.h"
#include "dlib/threads.h"

#include "agent.hpp"
#include "connector.hpp"
#include "globals.hpp"
#include "data_item.hpp"

class Agent;
class Device;

using namespace dlib;

class Adapter : public Connector, public threaded_object
{
public:
	// Associate adapter with a device & connect to the server & port
	Adapter(const std::string &device,
		const std::string &server,
		const unsigned int port,
		int aLegacyTimeout = 600);

	// Virtual destructor
	virtual ~Adapter();

	// Set pointer to the agent
	void setAgent(Agent &agent);
	bool isDupChecking() { return m_dupCheck; }
	void setDupCheck(bool aFlag) { m_dupCheck = aFlag; }
	Device *getDevice() const { return m_device; }

	bool isAutoAvailable() { return m_autoAvailable; }
	void setAutoAvailable(bool aFlag) { m_autoAvailable = aFlag; }

	bool isIgnoringTimestamps() { return m_ignoreTimestamps; }
	void setIgnoreTimestamps(bool aFlag) { m_ignoreTimestamps = aFlag; }

	void setReconnectInterval(int aInterval) { m_reconnectInterval = aInterval; }
	int getReconnectInterval() const { return m_reconnectInterval; }

	void setRelativeTime(bool aFlag) { m_relativeTime = aFlag; }
	bool getrelativeTime() { return m_relativeTime; }

	void setConversionRequired(bool aFlag) { m_conversionRequired = aFlag; }
	bool conversionRequired() const { return m_conversionRequired; }

	void setUpcaseValue(bool aFlag) { m_upcaseValue = aFlag; }
	bool upcaseValue() const { return m_upcaseValue; }

	uint64_t getBaseTime() { return m_baseTime; }
	uint64_t getBaseOffset() { return m_baseOffset; }

	bool isParsingTime() { return m_parseTime; }
	void setParseTime(bool aFlag) { m_parseTime = aFlag; }

	// For testing...
	void setBaseOffset(uint64_t aOffset) { m_baseOffset = aOffset; }
	void setBaseTime(uint64_t aOffset) { m_baseTime = aOffset; }

	// Inherited method to incoming data from the server
	virtual void processData(const std::string &data);
	virtual void protocolCommand(const std::string &data);

	// Method called when connection is lost.
	virtual void disconnected();
	virtual void connected();

	bool isDuplicate(DataItem *aDataItem, const std::string &aValue, double aTimeOffset)
	{
	if (!aDataItem->isDiscrete())
	{
		if (aDataItem->hasMinimumDelta() || aDataItem->hasMinimumPeriod())
		return aDataItem->isFiltered(aDataItem->convertValue(atof(aValue.c_str())), aTimeOffset);
		else
		return  m_dupCheck && aDataItem->isDuplicate(aValue);
	}
	else
	{
		return false;
	}
	}

	// Stop
	void stop();

	// For the additional devices associated with this adapter
	void addDevice(std::string &aDevice);

protected:
	void parseCalibration(const std::string &aString);
	void processAsset(std::istringstream &toParse, const std::string &key, const std::string &value,
			  const std::string &time);
	bool processDataItem(std::istringstream &toParse, const std::string &aLine,
			 const std::string &aKey, const std::string &aValue,
			 const std::string &aTime, double anOffset, bool aFirst = false);
	std::string extractTime(const std::string &time, double &anOffset);

protected:
	// Pointer to the agent
	Agent *m_agent;
	Device *m_device;
	std::vector<Device *> m_allDevices;

	// Name of device associated with adapter
	std::string m_deviceName;

	// If the connector has been running
	bool m_running;

	// Check for dups
	bool m_dupCheck;
	bool m_autoAvailable;
	bool m_ignoreTimestamps;
	bool m_relativeTime;
	bool m_conversionRequired;
	bool m_upcaseValue;

	// For relative times
	uint64_t m_baseTime;
	uint64_t m_baseOffset;

	bool m_parseTime;

	// For multiline asset parsing...
	bool m_gatheringAsset;
	std::string m_terminator;
	std::string m_assetId;
	std::string m_assetType;
	std::string m_time;
	std::ostringstream m_body;
	Device *m_assetDevice;
	std::set<std::string> m_logOnce;

	// Timeout for reconnection attempts, given in milliseconds
	int m_reconnectInterval;

private:
	// Inherited and is run as part of the threaded_object
	void thread();
};
