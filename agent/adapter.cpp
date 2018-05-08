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
#define __STDC_LIMIT_MACROS 1
#include "adapter.hpp"
#include "device.hpp"
#include "globals.hpp"
#include "dlib/logger.h"
#include <algorithm>

using namespace std;

static dlib::logger g_logger("input.adapter");

// Adapter public methods
Adapter::Adapter(const string &device,
				 const string &server,
				 const unsigned int port,
				 int legacyTimeout) :
	Connector(server, port, legacyTimeout),
	m_agent(nullptr),
	m_device(nullptr),
	m_deviceName(device),
	m_running(true),
	m_dupCheck(false),
	m_autoAvailable(false),
	m_ignoreTimestamps(false),
	m_relativeTime(false),
	m_conversionRequired(true),
	m_upcaseValue(true),
	m_baseTime(0),
	m_baseOffset(0),
	m_parseTime(false),
	m_gatheringAsset(false),
	m_assetDevice(nullptr),
	m_reconnectInterval(10 * 1000)
{
}


Adapter::~Adapter()
{
	if (m_running)
		stop();
}


void Adapter::stop()
{
	// Will stop threaded object gracefully Adapter::thread()
	m_running = false;
	close();
	wait();
}


void Adapter::setAgent(Agent &agent)
{
	m_agent = &agent;
	m_device = m_agent->getDeviceByName(m_deviceName);
	if (m_device != nullptr)
	{
		m_device->addAdapter(this);
		m_allDevices.push_back(m_device);
	}
}


void Adapter::addDevice(string &device)
{
	Device *dev = m_agent->getDeviceByName(device);
	if (dev != nullptr)
	{
		m_allDevices.push_back(dev);
		dev->addAdapter(this);
	}
}


inline static bool splitKey(string &key, string &dev)
{
	size_t found = key.find_first_of(':');
	if (found == string::npos)
		return false;
	else
	{
		dev = key.substr(0, found);
		key.erase(0, found + 1);
		return true;
	}
}


inline static void trim(std::string &str)
{
	size_t index = str.find_first_not_of(" \r\t");

	if (index != string::npos && index > 0)
		str.erase(0, index);
	index = str.find_last_not_of(" \r\t");
	if (index != string::npos)
		str.erase(index + 1);
}


inline string Adapter::extractTime(const string &time, double &anOffset)
{
	// Check how to handle time. If the time is relative, then we need to compute the first
	// offsets, otherwise, if this function is being used as an API, add the current time.
	string result;
	if (m_relativeTime)
	{
		uint64_t offset;

		if (m_baseTime == 0)
		{
			m_baseTime = getCurrentTimeInMicros();

			if (time.find('T') != string::npos)
			{
				m_parseTime = true;
				m_baseOffset = parseTimeMicro(time);
			}
			else
				m_baseOffset = (uint64_t)(atof(time.c_str()) * 1000.0);

			offset = 0;
		}
		else if (m_parseTime)
			offset = parseTimeMicro(time) - m_baseOffset;
		else
			offset = ((uint64_t)(atof(time.c_str()) * 1000.0)) - m_baseOffset;

		// convert microseconds to seconds
		anOffset = offset / 1000000;
		result = getRelativeTimeString(m_baseTime + offset);
	}
	else if (m_ignoreTimestamps || time.empty())
	{
		anOffset = getCurrentTimeInSec();
		result = getCurrentTime(GMT_UV_SEC);
	}
	else
	{
    	anOffset = parseTimeMicro(time) / 1000000;
		result = time;
	}

	return result;
}


//
// Expected data to parse in SDHR format:
//   Time|Alarm|Code|NativeCode|Severity|State|Description
//   Time|Item|Value
//   Time|Item1|Value1|Item2|Value2...
//
// Support for assets:
//   Time|@ASSET@|id|type|<...>...</...>
//
void Adapter::processData(const string &data)
{
	if (m_gatheringAsset)
	{
		if (data == m_terminator)
		{
			m_agent->addAsset(m_assetDevice, m_assetId, m_body.str(), m_assetType, m_time);
			m_gatheringAsset = false;
		}
		else
			m_body << data << endl;

		return;
	}

	istringstream toParse(data);
	string key, value;

	getline(toParse, key, '|');
	double offset = NAN;
	string time = extractTime(key, offset);
	getline(toParse, key, '|');
	getline(toParse, value, '|');

	// Data item name has a @, it is an asset special prefix.
	if (key.find('@') != string::npos)
	{
		trim(value);
		processAsset(toParse, key, value, time);
	}
	else
	{
		if (processDataItem(toParse, data, key, value, time, offset, true))
		{
			// Look for more key->value pairings in the rest of the data
			while (getline(toParse, key, '|'))
			{
				value.clear();
				getline(toParse, value, '|');
				processDataItem(toParse, data, key, value, time, offset);
			}
		}
	}
}


bool Adapter::processDataItem(
	istringstream &toParse,
	const string &line,
	const string &inputKey,
	const string &inputValue,
	const string &time,
	double anOffset,
	bool first)
{
	string dev, key = inputKey;
	Device *device;
	DataItem *dataItem;
	bool more = true;

	if (splitKey(key, dev))
		device = m_agent->getDeviceByName(dev);
	else
	{
		dev = m_deviceName;
		device = m_device;
	}

	if (device != nullptr)
	{
		dataItem = device->getDeviceDataItem(key);

		if (dataItem == nullptr)
		{
			if (m_logOnce.count(key) > 0)
				g_logger << LTRACE <<  "(" << device->getName() << ") Could not find data item: " << key;
			else
			{
				g_logger << LWARN << "(" << device->getName() << ") Could not find data item: " << key <<
						" from line '" << line << "'";
				m_logOnce.insert(key);
			}
		}
		else if (dataItem->hasConstantValue())
		{
			if (m_logOnce.count(key) == 0)
			{
				g_logger << LDEBUG << "(" << device->getName() << ") Ignoring value for: " << key << ", constant value";
				m_logOnce.insert(key);
			}
		}
		else
		{
			string rest, value;
			if (first &&
				(dataItem->isCondition() || dataItem->isAlarm() || dataItem->isMessage() || dataItem->isTimeSeries() ) )
			{
				getline(toParse, rest);
				value = inputValue + "|" + rest;
				more = false;
			}
			else
			{
				if (m_upcaseValue)
				{
					value.resize(inputValue.length());
					transform(inputValue.begin(), inputValue.end(), value.begin(), ::toupper);
				}
				else
					value = inputValue;
			}

			dataItem->setDataSource(this);

			trim(value);
			string check = value;

			if (dataItem->hasResetTrigger())
			{
				size_t found = value.find_first_of(':');
				if (found != string::npos)
					check.erase(found);
			}

			if (!isDuplicate(dataItem, check, anOffset))
				m_agent->addToBuffer(dataItem, value, time);
			else if (m_dupCheck)
				g_logger << LTRACE << "Dropping duplicate value for " << key << " of " << value;
		}
	}
	else
	{
		g_logger << LDEBUG << "Could not find device: " << dev;
		// Continue on processing the rest of the fields. Assume key/value pairs...
	}

	return more;
}


void Adapter::processAsset(
	istringstream &toParse,
	const string &inputKey,
	const string &value,
	const string &time)
{
	Device *device(nullptr);
	string key = inputKey, dev;
	if (splitKey(key, dev))
		device = m_agent->getDeviceByName(dev);
	else
		device = m_device;

	string assetId;
	if (value[0] == '@')
		assetId = device->getUuid() + value.substr(1);
	else
		assetId = value;

	if (key == "@ASSET@")
	{
		string type, rest;
		getline(toParse, type, '|');
		getline(toParse, rest);
	
		// Chck for an update and parse key value pairs. If only a type
		// is presented, then assume the remainder is a complete doc.

		// if the rest of the line begins with --multiline--... then
		// set multiline and accumulate until a completed document is found
		if (rest.find("--multiline--") != rest.npos)
		{
			m_assetDevice = device;
			m_gatheringAsset = true;
			m_terminator = rest;
			m_time = time;
			m_assetType = type;
			m_assetId = assetId;
			m_body.str("");
			m_body.clear();
		}
		else
			m_agent->addAsset(device, assetId, rest, type, time);
	}
	else if (key == "@UPDATE_ASSET@")
	{
		string assetKey, assetValue;
		AssetChangeList list;
		getline(toParse, assetKey, '|');
		if (assetKey[0] == '<')
		{
			do
			{
				pair<string,string> kv("xml", assetKey);
				list.push_back(kv);
			} while (getline(toParse, assetKey, '|'));
		}
		else
		{
			while (getline(toParse, assetValue, '|'))
			{
				pair<string,string> kv(assetKey, assetValue);
				list.push_back(kv);
		
				if (!getline(toParse, assetKey, '|'))
					break;
			}
		}

		m_agent->updateAsset(device, assetId, list, time);
	}
	else if (key == "@REMOVE_ASSET@")
		m_agent->removeAsset(device, assetId, time);
	else if (key == "@REMOVE_ALL_ASSETS@")
		m_agent->removeAllAssets(device, value, time);

}


static inline bool is_true(const string &aValue)
{
	return(aValue == "yes" || aValue == "true" || aValue == "1");
}


void Adapter::protocolCommand(const std::string &data)
{
	// Handle initial push of settings for uuid, serial number and manufacturer. 
	// This will override the settings in the device from the xml
	if (data == "* PROBE")
	{
		string response = m_agent->handleProbe(m_deviceName);
		string probe = "* PROBE LENGTH=";
		probe.append(intToString(response.length()));
		probe.append("\n");
		probe.append(response);
		probe.append("\n");
		m_connection->write(probe.c_str(), probe.length());
	}
	else
	{
		size_t index = data.find(':', 2);
		if (index != string::npos)
		{
			// Slice from the second character to the :, without the colon
			string key = data.substr(2, index - 2);
			trim(key);
			string value = data.substr(index + 1);
			trim(value);
	
			bool updateDom = true;
			if (key == "uuid" && !m_device->m_preserveUuid)
				m_device->setUuid(value);
			else if (key == "manufacturer")
				m_device->setManufacturer(value);
			else if (key == "station")
				m_device->setStation(value);
			else if (key == "serialNumber")
				m_device->setSerialNumber(value);
			else if (key == "description")
				m_device->setDescription(value);
			else if (key == "nativeName")
				m_device->setNativeName(value);
			else if (key == "calibration")
				parseCalibration(value);
			else if (key == "conversionRequired")
				m_conversionRequired = is_true(value);
			else if (key == "relativeTime")
				m_relativeTime = is_true(value);
			else if (key == "realTime")
				m_realTime = is_true(value);
			else if (key == "device")
			{
				Device *device = m_agent->findDeviceByUUIDorName(value);

				if (device != nullptr)
				{
					m_device = device;
					g_logger << LINFO << "Device name given by the adapter " << value
					<< ", has been assigned to cfg " << m_deviceName;
					m_deviceName = value;
				}
				else
				{
					g_logger << LERROR << "Cannot find device for device command: " << value;
					throw std::invalid_argument(string("Cannot find device for device name or uuid: ") + value);
				}
			}
			else
			{
				g_logger << LWARN << "Unknown command '" << data << "' for device '" << m_deviceName;
				updateDom = false;
			}

			if (updateDom)
				m_agent->updateDom(m_device);
		}
	}
}


void Adapter::parseCalibration(const std::string &aLine)
{
	istringstream toParse(aLine);

	// Look for name|factor|offset triples
	string name, factor, offset;
	while (getline(toParse, name, '|') &&
			getline(toParse, factor, '|') &&
			getline(toParse, offset, '|'))
	{
		// Convert to a floating point number
		DataItem *di = m_device->getDeviceDataItem(name);

		if (di == nullptr)
			g_logger << LWARN << "Cannot find data item to calibrate for " << name;
		else
		{
			double fact_value = strtod(factor.c_str(), nullptr);
			double off_value = strtod(offset.c_str(), nullptr);
			di->setConversionFactor(fact_value, off_value);
		}
	}
}


void Adapter::disconnected()
{
	m_baseTime = 0;
	m_agent->disconnected(this, m_allDevices);
}


void Adapter::connected()
{
	m_agent->connected(this, m_allDevices);
}


// Adapter private methods
void Adapter::thread()
{
	while (m_running)
	{
		try
		{
			// Start the connection to the socket
			connect();

			// make sure we're closed...
			close();
		}
		catch (std::invalid_argument &err)
		{
			g_logger << LERROR << "Adapter for " << m_deviceName << "'s thread threw an argument error, stopping adapter: "
			<< err.what();
			stop();
		}
		catch (std::exception &err)
		{
			g_logger << LERROR << "Adapter for " << m_deviceName << "'s thread threw an exceotion, stopping adapter: "
			<< err.what();
			stop();
		}
		catch (...)
		{
			g_logger << LERROR << "Thread for adapter " << m_deviceName << "'s thread threw an unhandled exception, stopping adapter";
			stop();
		}

		if (!m_running)
			break;

		// Try to reconnect every 10 seconds
		g_logger << LINFO << "Will try to reconnect in " << m_reconnectInterval << " milliseconds";
		dlib::sleep(m_reconnectInterval);
	}
	g_logger << LINFO << "Adapter thread stopped";
}

