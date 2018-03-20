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
#include "agent.hpp"
#include "dlib/logger.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <sstream>
#include <stdexcept>
#include <dlib/tokenizer.h>
#include <dlib/misc_api.h>
#include <dlib/array.h>
#include <dlib/dir_nav.h>
#include <dlib/config_reader.h>
#include <dlib/queue.h>
#include <functional>

using namespace std;

static const string g_unavailable("UNAVAILABLE");
static const string g_conditionUnavailable("UNAVAILABLE|||");

static const string g_available("AVAILABLE");
static dlib::logger g_logger("agent");

// Agent public methods
Agent::Agent(
	const string& configXmlPath,
	int bufferSize,
	int maxAssets,
	int checkpointFreq) :
	m_putEnabled(false),
	m_logStreamData(false)
{
	m_mimeTypes["xsl"] = "text/xsl";
	m_mimeTypes["xml"] = "text/xml";
	m_mimeTypes["css"] = "text/css";
	m_mimeTypes["xsd"] = "text/xml";
	m_mimeTypes["jpg"] = "image/jpeg";
	m_mimeTypes["jpeg"] = "image/jpeg";
	m_mimeTypes["png"] = "image/png";
	m_mimeTypes["ico"] = "image/x-icon";

	try
	{
		// Load the configuration for the Agent
		m_xmlParser = new XmlParser();
		m_devices = m_xmlParser->parseFile(configXmlPath);
		std::vector<Device *>::iterator device;
		std::set<std::string> uuids;
		for (device = m_devices.begin(); device != m_devices.end(); ++device)
		{
			if (uuids.count((*device)->getUuid()) > 0)
				throw runtime_error("Duplicate UUID: " + (*device)->getUuid());

			uuids.insert((*device)->getUuid());
			(*device)->resolveReferences();
		}
	}
	catch (runtime_error & e)
	{
		g_logger << LFATAL << "Error loading xml configuration: " + configXmlPath;
		g_logger << LFATAL << "Error detail: " << e.what();
		cerr << e.what() << endl;
		throw e;
	}
	catch (exception &f)
	{
		g_logger << LFATAL << "Error loading xml configuration: " + configXmlPath;
		g_logger << LFATAL << "Error detail: " << f.what();
		cerr << f.what() << endl;
		throw f;
	}

	// Grab data from configuration
	string time = getCurrentTime(GMT_UV_SEC);

	// Unique id number for agent instance
	m_instanceId = getCurrentTimeInSec();

	// Sequence number and sliding buffer for data
	m_sequence = 1;
	m_slidingBufferSize = 1 << bufferSize;
	m_slidingBuffer = new sliding_buffer_kernel_1<ComponentEventPtr>();
	m_slidingBuffer->set_size(bufferSize);
	m_checkpointFreq = checkpointFreq;
	m_checkpointCount = (m_slidingBufferSize / checkpointFreq) + 1;

	// Asset sliding buffer
	m_maxAssets = maxAssets;

	// Create the checkpoints at a regular frequency
	m_checkpoints = new Checkpoint[m_checkpointCount];

	// Mutex used for synchronized access to sliding buffer and sequence number
	m_sequenceLock = new dlib::mutex;
	m_assetLock = new dlib::mutex;

	// Add the devices to the device map and create availability and 
	// asset changed events if they don't exist
	std::vector<Device *>::iterator device;

	for (device = m_devices.begin(); device != m_devices.end(); ++device)
	{
		m_deviceMap[(*device)->getName()] = *device;

		// Make sure we have two device level data items:
		// 1. Availability
		// 2. AssetChanged
		if ((*device)->getAvailability() == nullptr)
		{
			// Create availability data item and add it to the device.
			std::map<string, string> attrs;
			attrs["type"] = "AVAILABILITY";
			attrs["id"] = (*device)->getId() + "_avail";
			attrs["category"] = "EVENT";

			DataItem *di = new DataItem(attrs);
			di->setComponent(*(*device));
			(*device)->addDataItem(*di);
			(*device)->addDeviceDataItem(*di);
			(*device)->m_availabilityAdded = true;
		}

		int major, minor;
		char c;
		stringstream ss(XmlPrinter::getSchemaVersion());
		ss >> major >> c >> minor;

		if ((*device)->getAssetChanged() == nullptr && (major > 1 || (major == 1 && minor >= 2)))
		{
			// Create asset change data item and add it to the device.
			std::map<string,string> attrs;
			attrs["type"] = "ASSET_CHANGED";
			attrs["id"] = (*device)->getId() + "_asset_chg";
			attrs["category"] = "EVENT";

			DataItem *di = new DataItem(attrs);
			di->setComponent(*(*device));
			(*device)->addDataItem(*di);
			(*device)->addDeviceDataItem(*di);
		}

		if ((*device)->getAssetRemoved() == nullptr && (major > 1 || (major == 1 && minor >= 3)))
		{
			// Create asset removed data item and add it to the device.
			std::map<string, string> attrs;
			attrs["type"] = "ASSET_REMOVED";
			attrs["id"] = (*device)->getId() + "_asset_rem";
			attrs["category"] = "EVENT";

			DataItem *di = new DataItem(attrs);
			di->setComponent(*(*device));
			(*device)->addDataItem(*di);
			(*device)->addDeviceDataItem(*di);
		}
	}

	// Reload the document for path resolution
	m_xmlParser->loadDocument(
		XmlPrinter::printProbe(
			m_instanceId,
			m_slidingBufferSize, 
			m_maxAssets,
			m_assets.size(),
			m_sequence,
			m_devices) );

	// Initialize the id mapping for the devices and set all data items to UNAVAILABLE
	for (device = m_devices.begin(); device != m_devices.end(); ++device)
	{
		const std::map<string, DataItem *> &items = (*device)->getDeviceDataItems();
		std::map<string, DataItem *>::const_iterator item;

		for (item = items.begin(); item != items.end(); ++item)
		{
			// Check for single valued constrained data items.
			DataItem *d = item->second;
			const string *value = &g_unavailable;

			if (d->isCondition())
				value = &g_conditionUnavailable;
			else if (d->hasConstantValue())
				value = &(d->getConstrainedValues()[0]);

			addToBuffer(d, *value, time);
			if (m_dataItemMap.count(d->getId()) == 0)
				m_dataItemMap[d->getId()] = d;
			else
			{
				g_logger << LFATAL << "Duplicate DataItem id " << d->getId() <<
					" for device: " << (*device)->getName() << " and data item name: " <<
					d->getName();
				std::exit(1);
			}
		}
	}
}


Device *Agent::findDeviceByUUIDorName(const std::string &aId)
{
	Device *device = nullptr;

	std::vector<Device *>::iterator it;

	for (it = m_devices.begin(); device == nullptr && it != m_devices.end(); it++)
	{
		if ((*it)->getUuid() == aId || (*it)->getName() == aId)
			device = *it;
	}

	return device;
}


Agent::~Agent()
{
	delete m_slidingBuffer;
	delete m_sequenceLock;
	delete m_assetLock;
	delete m_xmlParser;
	delete[] m_checkpoints;
}


void Agent::start()
{
	try
	{
		// Start all the adapters
		std::vector<Adapter *>::iterator iter;

		for (iter = m_adapters.begin(); iter != m_adapters.end(); iter++)
			(*iter)->start();

		// Start the server. This blocks until the server stops.
		server_http::start();
	}
	catch (dlib::socket_error &e)
	{
		g_logger << LFATAL << "Cannot start server: " << e.what();
		std::exit(1);
	}
}


void Agent::clear()
{
	// Stop all adapter threads...
	std::vector<Adapter *>::iterator iter;

	g_logger << LINFO << "Shutting down adapters";

	// Deletes adapter and waits for it to exit.
	for (iter = m_adapters.begin(); iter != m_adapters.end(); iter++)
		(*iter)->stop();

	g_logger << LINFO << "Shutting down server";
	server::http_1a::clear();
	g_logger << LINFO << "Shutting completed";

	for (iter = m_adapters.begin(); iter != m_adapters.end(); iter++)
		delete (*iter);

	m_adapters.clear();

}


// Register a file
void Agent::registerFile(const string &aUri, const string &aPath)
{
	try
	{
		directory dir(aPath);
		queue<file>::kernel_1a files;
		dir.get_files(files);
		files.reset();
		string baseUri = aUri;

		if (*baseUri.rbegin() != '/')
			baseUri.append(1, '/');

		while (files.move_next())
		{
			file &file = files.element();
			string name = file.name();
			string uri = baseUri + name;
			m_fileMap.insert(pair<string, string>(uri, file.full_name()));

			// Check if the file name maps to a standard MTConnect schema file.
			if (name.find("MTConnect") == 0 &&
				name.substr(name.length() - 4, 4) == ".xsd" &&
				XmlPrinter::getSchemaVersion() == name.substr(name.length() - 7, 3))
			{
				string version = name.substr(name.length() - 7, 3);

				if (name.substr(9, 5) == "Error")
				{
					string urn = "urn:mtconnect.org:MTConnectError:" + XmlPrinter::getSchemaVersion();
					XmlPrinter::addErrorNamespace(urn, uri, "m");
				}
				else if (name.substr(9, 7) == "Devices")
				{
					string urn = "urn:mtconnect.org:MTConnectDevices:" + XmlPrinter::getSchemaVersion();
					XmlPrinter::addDevicesNamespace(urn, uri, "m");
				}
				else if (name.substr(9, 6) == "Assets")
				{
					string urn = "urn:mtconnect.org:MTConnectAssets:" + XmlPrinter::getSchemaVersion();
					XmlPrinter::addAssetsNamespace(urn, uri, "m");
				}
				else if (name.substr(9, 7) == "Streams")
				{
					string urn = "urn:mtconnect.org:MTConnectStreams:" + XmlPrinter::getSchemaVersion();
					XmlPrinter::addStreamsNamespace(urn, uri, "m");
				}
			}
		}
	}
	catch (directory::dir_not_found e)
	{
		g_logger << LDEBUG << "registerFile: Path " << aPath << " is not a directory: "
			<< e.what() << ", trying as a file";

		try
		{
			file file(aPath);
			m_fileMap.insert(pair<string, string>(aUri, aPath));
		}
		catch (file::file_not_found e)
		{
			g_logger << LERROR << "Cannot register file: " << aPath << ": " << e.what();
		}
	}
}


// Methods for service
const string Agent::on_request(const incoming_things &incoming, outgoing_things &outgoing)
{
	string result;
	outgoing.headers["Content-Type"] = "text/xml";

	try
	{
		g_logger << LDEBUG << "Request: " << incoming.request_type << " " <<
			incoming.path << " from " << incoming.foreign_ip << ":" << incoming.foreign_port;

		if (m_putEnabled)
		{
			if ((incoming.request_type == "PUT" || incoming.request_type == "POST") &&
				!m_putAllowedHosts.empty() &&
				m_putAllowedHosts.count(incoming.foreign_ip) == 0)
			{
				return printError("UNSUPPORTED",
					"HTTP PUT is not allowed from " + incoming.foreign_ip);
			}

			if (incoming.request_type != "GET" &&
				incoming.request_type != "PUT" &&
				incoming.request_type != "POST")
			{
				return printError("UNSUPPORTED",
					"Only the HTTP GET and PUT requests are supported");
			}
		}
		else
		{
			if (incoming.request_type != "GET")
			{
				return printError("UNSUPPORTED",
					"Only the HTTP GET request is supported");
			}
		}

		// Parse the URL path looking for '/'
		string path = incoming.path;
		size_t qm = path.find_last_of('?');

		if (qm != string::npos)
			path = path.substr(0, qm);

		if (isFile(path))
			return handleFile(path, outgoing);

		string::size_type loc1 = path.find("/", 1);
		string::size_type end = (path[path.length() - 1] == '/') ?
					path.length() - 1 : string::npos;

		string first =  path.substr(1, loc1 - 1);
		string call, device;

		if (first == "assets" || first == "asset")
		{
			string list;

			if (loc1 != string::npos)
				list = path.substr(loc1 + 1);

			if (incoming.request_type == "GET")
				result = handleAssets(*outgoing.out, incoming.queries, list);
			else
				result = storeAsset(*outgoing.out, incoming.queries, list, incoming.body);
		}
		else
		{
			// If a '/' was found
			if (loc1 < end)
			{
				// Look for another '/'
				string::size_type loc2 = path.find("/", loc1 + 1);

				if (loc2 == end)
				{
					device = first;
					call = path.substr(loc1 + 1, loc2 - loc1 - 1);
				}
				else
				{
					// Path is too long
					return printError("UNSUPPORTED", "The following path is invalid: " + path);
				}
			}
			else
			{
				// Try to handle the call
				call = first;
			}

			if (incoming.request_type == "GET")
				result = handleCall(*outgoing.out, path, incoming.queries, call, device);
			else
				result = handlePut(*outgoing.out, path, incoming.queries, call, device);
		}
	}
	catch (exception &e)
	{
		printError("SERVER_EXCEPTION", (string) e.what());
	}

	return result;
}


Adapter *Agent::addAdapter(
	const string& deviceName,
	const string& host,
	const unsigned int port,
	bool start,
	int legacyTimeout )
{
	Adapter *adapter = new Adapter(deviceName, host, port, legacyTimeout);
	adapter->setAgent(*this);
	m_adapters.push_back(adapter);

	Device *dev = m_deviceMap[deviceName];

	if (dev != nullptr && dev->m_availabilityAdded)
		adapter->setAutoAvailable(true);

	if (start)
		adapter->start();

	return adapter;
}


unsigned int Agent::addToBuffer(
	DataItem *dataItem,
	const string& value,
	string time )
{
	if (dataItem == nullptr)
		return 0;

	dlib::auto_mutex lock(*m_sequenceLock);

	uint64_t seqNum = m_sequence++;
	ComponentEvent *event = new ComponentEvent(*dataItem, seqNum, time, value);

	(*m_slidingBuffer)[seqNum] = event;
	m_latest.addComponentEvent(event);
	event->unrefer();

	// Special case for the first event in the series to prime the first checkpoint.
	if (seqNum == 1)
		m_first.addComponentEvent(event);

	// Checkpoint management
	int index = m_slidingBuffer->get_element_id(seqNum);
	if (m_checkpointCount > 0 && index % m_checkpointFreq == 0)
	{
		// Copy the checkpoint from the current into the slot
		m_checkpoints[index / m_checkpointFreq].copy(m_latest);
	}


	// See if the next sequence has an event. If the event exists it
	// should be added to the first checkpoint.
	if ((*m_slidingBuffer)[m_sequence] != nullptr)
	{
		// Keep the last checkpoint up to date with the last.
		m_first.addComponentEvent((*m_slidingBuffer)[m_sequence]);
	}

	dataItem->signalObservers(seqNum);

	return seqNum;
}


bool Agent::addAsset(
	Device *device,
	const string &id,
	const string &asset,
	const string &type,
	const string &inputTime)
{
	// Check to make sure the values are present
	if (type.empty() || asset.empty() || id.empty())
	{
		g_logger << LWARN << "Asset '" << id << "' missing required type, id, or body. Asset is rejected.";
		return false;
	}

	string time;
	if (inputTime.empty())
		time = getCurrentTime(GMT_UV_SEC);
	else
		time = inputTime;

	AssetPtr ptr;

	// Lock the asset addition to protect from multithreaded collisions. Releaes
	// before we add the event so we don't cause a race condition.
	{
		dlib::auto_mutex lock(*m_assetLock);

		try
		{
			ptr = m_xmlParser->parseAsset(id, type, asset);
		}
		catch (runtime_error &e)
		{
			g_logger << LERROR << "addAsset: Error parsing asset: " << asset << "\n" << e.what();
			return false;
		}

		if (ptr.getObject() == nullptr)
		{
			g_logger << LERROR << "addAssete: Error parsing asset";
			return false;
		}

		AssetPtr *old = &m_assetMap[id];

		if (!ptr->isRemoved())
		{
			if (old->getObject() != nullptr)
				m_assets.remove(old);
			else
				m_assetCounts[type] += 1;
		}
		else if (old->getObject() == nullptr)
		{
			g_logger << LWARN << "Cannot remove non-existent asset";
			return false;
		}

		if (ptr.getObject() == nullptr)
		{
			g_logger << LWARN << "Asset could not be created";
			return false;
		}
		else
		{
			ptr->setAssetId(id);
			ptr->setTimestamp(time);
			ptr->setDeviceUuid(device->getUuid());
		}
	
		// Check for overflow
		if (m_assets.size() >= m_maxAssets)
		{
			AssetPtr oldref(*m_assets.front());
			m_assetCounts[oldref->getType()] -= 1;
			m_assets.pop_front();
			m_assetMap.erase(oldref->getAssetId());

			// Remove secondary keys
			AssetKeys &keys = oldref->getKeys();
			AssetKeys::iterator iter;
			for (iter = keys.begin(); iter != keys.end(); iter++)
			{
				AssetIndex &index = m_assetIndices[iter->first];
				index.erase(iter->second);
			}
		}

		m_assetMap[id] = ptr;

		if (!ptr->isRemoved())
		{
			AssetPtr &newPtr = m_assetMap[id];
			m_assets.push_back(&newPtr);
		}

		// Add secondary keys
		AssetKeys &keys = ptr->getKeys();
		AssetKeys::iterator iter;
		for (iter = keys.begin(); iter != keys.end(); iter++)
		{
			AssetIndex &index = m_assetIndices[iter->first];
			index[iter->second] = ptr;
		}
	}

	// Generate an asset changed event.
	if (ptr->isRemoved())
		addToBuffer(device->getAssetRemoved(), type + "|" + id, time);
	else
		addToBuffer(device->getAssetChanged(), type + "|" + id, time);

	return true;
}


bool Agent::updateAsset(
	Device *device,
	const std::string &id,
	AssetChangeList &assetChangeList,
	const string &inputTime )
{
	AssetPtr asset;
	string time;
	if (inputTime.empty())
		time = getCurrentTime(GMT_UV_SEC);
	else
		time = inputTime;

	{
		dlib::auto_mutex lock(*m_assetLock);

		asset = m_assetMap[id];
		if (asset.getObject() == nullptr)
			return false;

		if (asset->getType() != "CuttingTool" &&
			asset->getType() != "CuttingToolArchitype")
			return false;

		CuttingToolPtr tool((CuttingTool*) asset.getObject());

		try
		{
			AssetChangeList::iterator iter;

			for (iter = assetChangeList.begin(); iter != assetChangeList.end(); ++iter)
			{
				if (iter->first == "xml")
					m_xmlParser->updateAsset(asset, asset->getType(), iter->second);
				else
					tool->updateValue(iter->first, iter->second);
			}
		}
		catch (runtime_error &e)
		{
			g_logger << LERROR << "updateAsset: Error parsing asset: " << asset << "\n" << e.what();
			return false;
		}

		// Move it to the front of the queue
		m_assets.remove(&asset);
		m_assets.push_back(&asset);

		tool->setTimestamp(time);
		tool->setDeviceUuid(device->getUuid());
		tool->changed();
	}

	addToBuffer(device->getAssetChanged(), asset->getType() + "|" + id, time);

	return true;
}


bool Agent::removeAsset(
	Device *device,
	const std::string &id,
	const string &inputTime )
{
	AssetPtr asset;

	string time;
	if (inputTime.empty())
		time = getCurrentTime(GMT_UV_SEC);
	else
		time = inputTime;

	{
		dlib::auto_mutex lock(*m_assetLock);

		asset = m_assetMap[id];

		if (asset.getObject() == nullptr)
			return false;

		asset->setRemoved(true);
		asset->setTimestamp(time);

		// Check if the asset changed id is the same as this asset.
		ComponentEventPtr *ptr = m_latest.getEventPtr(device->getAssetChanged()->getId());
		if (ptr != nullptr && (*ptr)->getValue() == id)
			addToBuffer(device->getAssetChanged(), asset->getType() + "|UNAVAILABLE", time);
	}

	addToBuffer(device->getAssetRemoved(), asset->getType() + "|" + id, time);

	return true;
}


bool Agent::removeAllAssets(
	Device *device,
	const std::string &type,
	const std::string &inputTime )
{
	string time;
	if (inputTime.empty())
		time = getCurrentTime(GMT_UV_SEC);
	else
		time = inputTime;

	{
		dlib::auto_mutex lock(*m_assetLock);

		ComponentEventPtr *ptr = m_latest.getEventPtr(device->getAssetChanged()->getId());
		string changedId;
		if (ptr != nullptr)
			changedId = (*ptr)->getValue();

		list<AssetPtr*>::reverse_iterator iter;
		for (iter = m_assets.rbegin(); iter != m_assets.rend(); ++iter)
		{
			AssetPtr asset = (**iter);
			if (type == asset->getType() &&
				!asset->isRemoved() )
			{
				asset->setRemoved(true);
				asset->setTimestamp(time);

				addToBuffer(device->getAssetRemoved(), asset->getType() + "|" + asset->getAssetId(), time);

				if (changedId == asset->getAssetId())
					addToBuffer(device->getAssetChanged(), asset->getType() + "|UNAVAILABLE", time);
			}
		}
	}

	return true;
}


// Add values for related data items UNAVAILABLE
void Agent::disconnected(Adapter *adapter, std::vector<Device*> devices)
{
	string time = getCurrentTime(GMT_UV_SEC);
	g_logger << LDEBUG << "Disconnected from adapter, setting all values to UNAVAILABLE";

	std::vector<Device *>::iterator iter;

	for (iter = devices.begin(); iter != devices.end(); ++iter)
	{
		const std::map<std::string, DataItem *> &dataItems = (*iter)->getDeviceDataItems();
		std::map<std::string, DataItem *>::const_iterator dataItemAssoc;

		for (dataItemAssoc = dataItems.begin(); dataItemAssoc != dataItems.end(); ++dataItemAssoc)
		{
			DataItem *dataItem = (*dataItemAssoc).second;

			if (dataItem != nullptr &&
				(dataItem->getDataSource() == adapter || (adapter->isAutoAvailable() &&
				dataItem->getDataSource() == nullptr &&
				dataItem->getType() == "AVAILABILITY")))
			{
				ComponentEventPtr *ptr = m_latest.getEventPtr(dataItem->getId());

				if (ptr != nullptr)
				{
					const string *value = nullptr;

					if (dataItem->isCondition())
					{
						if ((*ptr)->getLevel() != ComponentEvent::UNAVAILABLE)
							value = &g_conditionUnavailable;
					}
					else if (dataItem->hasConstraints())
					{
						std::vector<std::string> &values = dataItem->getConstrainedValues();

						if (values.size() > 1 && (*ptr)->getValue() != g_unavailable)
							value = &g_unavailable;
					}
					else if ((*ptr)->getValue() != g_unavailable)
						value = &g_unavailable;

					if (value != nullptr &&
						!adapter->isDuplicate(dataItem, *value, NAN))
						addToBuffer(dataItem, *value, time);
				}
			}
			else if (dataItem == nullptr)
				g_logger << LWARN << "No data Item for " << (*dataItemAssoc).first;
		}
	}
}


void Agent::connected(Adapter *adapter, std::vector<Device *> devices)
{
	if (adapter->isAutoAvailable())
	{
		string time = getCurrentTime(GMT_UV_SEC);
		std::vector<Device *>::iterator iter;

		for (iter = devices.begin(); iter != devices.end(); ++iter)
		{
			g_logger << LDEBUG << "Connected to adapter, setting all Availability data items to AVAILABLE";

			if ((*iter)->getAvailability() != nullptr)
			{
				g_logger << LDEBUG << "Adding availabilty event for " << (*iter)->getAvailability()->getId();
				addToBuffer((*iter)->getAvailability(), g_available, time);
			}
			else
				g_logger << LDEBUG << "Cannot find availability for " << (*iter)->getName();
		}
	}
}


// Agent protected methods
string Agent::handleCall(
	ostream& out,
	const string& path,
	const key_value_map& queries,
	const string& call,
	const string& device)
{
	try
	{
		string deviceName;
		if (!device.empty())
			deviceName = device;

		if (call == "current")
		{
			const string path = queries[(string) "path"];
			string result;

			int freq = checkAndGetParam(queries, "frequency", NO_FREQ, FASTEST_FREQ, false,SLOWEST_FREQ);
			// Check for 1.2 conversion to interval
			if (freq == NO_FREQ)
				freq = checkAndGetParam(queries, "interval", NO_FREQ, FASTEST_FREQ, false, SLOWEST_FREQ);

			uint64_t at = checkAndGetParam64(queries, "at", NO_START, getFirstSequence(), true, m_sequence - 1);
			int heartbeat = checkAndGetParam(queries, "heartbeat", 10000, 10, true, 600000);

			if (freq != NO_FREQ && at != NO_START)
				return printError("INVALID_REQUEST", "You cannot specify both the at and frequency arguments to a current request");

			return handleStream(
				out,
				devicesAndPath(path, deviceName),
				true,
				freq,
				at,
				0,
				heartbeat);
		}
		else if (call == "probe" || call.empty())
			return handleProbe(deviceName);
		else if (call == "sample")
		{
			string path = queries[(string) "path"];
			string result;

			int count = checkAndGetParam(queries, "count", DEFAULT_COUNT, 1, true, m_slidingBufferSize);
			int freq = checkAndGetParam(queries, "frequency", NO_FREQ, FASTEST_FREQ, false, SLOWEST_FREQ);
			// Check for 1.2 conversion to interval
			if (freq == NO_FREQ)
				freq = checkAndGetParam(queries, "interval", NO_FREQ, FASTEST_FREQ, false, SLOWEST_FREQ);

			uint64 start = checkAndGetParam64(queries, "start", NO_START, getFirstSequence(), true, m_sequence);

			if (start == NO_START) // If there was no data in queries
				start = checkAndGetParam64(queries, "from", 1, getFirstSequence(), true, m_sequence);

			int heartbeat = checkAndGetParam(queries, "heartbeat", 10000, 10, true, 600000);

			return handleStream(
				out,
				devicesAndPath(path, deviceName),
				false,
				freq,
				start,
				count,
				heartbeat);
		}
		else if ((m_deviceMap[call] != nullptr) && device.empty())
			return handleProbe(call);
		else
			return printError("UNSUPPORTED", "The following path is invalid: " + path);
	}
	catch (ParameterError &aError)
	{
		return printError(aError.m_code, aError.m_message);
	}
}


string Agent::handlePut(
	ostream &out,
	const string &path,
	const key_value_map &queries,
	const string &adapter,
	const string &deviceName )
{
	string device = deviceName;
	if (device.empty() && adapter.empty())
		return printError("UNSUPPORTED", "Device must be specified for PUT");
	else if (device.empty())
		device = adapter;

	Device *dev = m_deviceMap[device];

	if (dev == nullptr)
	{
		string message = ((string) "Cannot find device: ") + device;
		return printError("UNSUPPORTED", message);
	}

	// First check if this is an adapter put or a data put...
	if (queries["_type"] == "command")
	{
		std::vector<Adapter *>::iterator adpt;

		for (adpt = dev->m_adapters.begin(); adpt != dev->m_adapters.end(); adpt++)
		{
			key_value_map::const_iterator kv;

			for (kv = queries.begin(); kv != queries.end(); kv++)
			{
				string command = kv->first + "=" + kv->second;
				g_logger << LDEBUG << "Sending command '" << command << "' to " << device;
				(*adpt)->sendCommand(command);
			}
		}
	}
	else
	{
		string time = queries["time"];

		if (time.empty())
			time = getCurrentTime(GMT_UV_SEC);

		key_value_map::const_iterator kv;

		for (kv = queries.begin(); kv != queries.end(); kv++)
		{
			if (kv->first != "time")
			{
				DataItem *di = dev->getDeviceDataItem(kv->first);

				if (di != nullptr)
					addToBuffer(di, kv->second, time);
				else
					g_logger << LWARN << "(" << device << ") Could not find data item: " << kv->first;
			}
		}
	}

	return "<success/>";
}


string Agent::handleProbe(const string &name)
{
	std::vector<Device *> deviceList;
 
	if (!name.empty())
	{
		Device *device = getDeviceByName(name);
		if (device == nullptr)
			return printError("NO_DEVICE", "Could not find the device '" + name + "'");
		else
			deviceList.push_back(device);
	}
	else
		deviceList = m_devices;

	return XmlPrinter::printProbe(
		m_instanceId,
		m_slidingBufferSize,
		m_sequence,
		m_maxAssets,
		m_assets.size(),
		deviceList,
		&m_assetCounts);
}


string Agent::handleStream(
	ostream &out,
	const string &path,
	bool current,
	unsigned int frequency,
	uint64_t start,
	unsigned int count,
	unsigned int heatbeat )
{
	std::set<string> filter;
	try
	{
		m_xmlParser->getDataItems(filter, path);
	}
	catch (exception& e)
	{
		return printError("INVALID_XPATH", e.what());
	}

	if (filter.empty())
		return printError("INVALID_XPATH", "The path could not be parsed. Invalid syntax: " + path);

	// Check if there is a frequency to stream data or not
	if (frequency != (unsigned)NO_FREQ )
	{
		streamData(out, filter, current, frequency, start, count, heatbeat);
		return "";
	}
	else
	{
		uint64_t end;
		bool endOfBuffer;
		if (current)
			return fetchCurrentData(filter, start);
		else
			return fetchSampleData(filter, start, count, end, endOfBuffer);
	}
}


std::string Agent::handleAssets(
	std::ostream& aOut,
	const key_value_map& queries,
	const std::string& list )
{
	using namespace dlib;
	std::vector<AssetPtr> assets;
	if (!list.empty()) 
	{
		auto_mutex lock(*m_assetLock);
		istringstream str(list);
		tokenizer_kernel_1 tok;
		tok.set_stream(str);
		tok.set_identifier_token(tok.lowercase_letters() + tok.uppercase_letters() +
									tok.numbers() + "_.@$%&^:+-_=",
									tok.lowercase_letters() + tok.uppercase_letters() +
									tok.numbers() + "_.@$%&^:+-_=");
	
	
		int type;
		string token;
		for (tok.get_token(type, token); type != tok.END_OF_FILE; tok.get_token(type, token))
		{
			if (type == tok.IDENTIFIER)
			{
				AssetPtr ptr = m_assetMap[token];
				if (ptr.getObject() == nullptr)
					return XmlPrinter::printError(m_instanceId, 0, 0, "ASSET_NOT_FOUND", (string)"Could not find asset: " + token);
				assets.push_back(ptr);
			}
		}
	}
	else
	{
		auto_mutex lock(*m_assetLock);
		// Return all asssets, first check if there is a type attribute

		string type = queries["type"];
		bool removed = (queries.count("removed") > 0 && queries["removed"] == "true");
		int count = checkAndGetParam(queries, "count", m_assets.size(), 1, false, NO_VALUE32);

		std::list<AssetPtr*>::reverse_iterator iter;
		for (iter = m_assets.rbegin(); iter != m_assets.rend() && count > 0; ++iter, --count)
		{
			if ( (type.empty() || type == (**iter)->getType()) &&
				 (removed || !(**iter)->isRemoved()) )
			{
				assets.push_back(**iter);
			}
		}
	}

	return XmlPrinter::printAssets(m_instanceId, m_maxAssets, m_assets.size(), assets);
}


// Store an asset in the map by asset # and use the circular buffer as
// an LRU. Check if we're removing an existing asset and clean up the
// map, and then store this asset.
std::string Agent::storeAsset(
	std::ostream& aOut,
	const key_value_map& queries,
	const std::string& id,
	const std::string& body )
{
	string name = queries["device"];
	string type = queries["type"];
	Device *device = nullptr;

	if (!name.empty())
		device = m_deviceMap[name];

	// If the device was not found or was not provided, use the default device.
	if (device == nullptr)
		device = m_devices[0];

	if (addAsset(device, id, body, type))
		return "<success/>";
	else
		return "<failure/>";
}


string Agent::handleFile(const string &uri, outgoing_things& outgoing)
{
	// Get the mime type for the file.
	bool unknown = true;
	size_t last = uri.find_last_of("./");
	string contentType;
	if (last != string::npos && uri[last] == '.')
	{
		string ext = uri.substr(last + 1);
		if (m_mimeTypes.count(ext) > 0)
		{
			contentType = m_mimeTypes[ext];
			unknown = false;
		}
	}

	if (unknown)
		contentType = "application/octet-stream";

	// Check if the file is cached
	RefCountedPtr<CachedFile> cachedFile;
	std::map<string, RefCountedPtr<CachedFile>>::iterator cached = m_fileCache.find(uri);
	if (cached != m_fileCache.end())
		cachedFile = cached->second;
	else
	{
		std::map<string, string>::iterator file = m_fileMap.find(uri);
	
		// Should never happen
		if (file == m_fileMap.end())
		{
			outgoing.http_return = 404;
			outgoing.http_return_status = "File not found";
			return "";
		}

		const char *path = file->second.c_str();

		struct stat fs;
		int res = stat(path, &fs);
		if (res != 0)
		{
			outgoing.http_return = 404;
			outgoing.http_return_status = "File not found";
			return "";
		}

		int fd = open(path, O_RDONLY | O_BINARY);
		if (res < 0)
		{
			outgoing.http_return = 404;
			outgoing.http_return_status = "File not found";
			return "";
		}

		cachedFile.setObject(new CachedFile(fs.st_size), true);
		int bytes = read(fd, cachedFile->m_buffer, fs.st_size);
		close(fd);

		if (bytes < fs.st_size)
		{
			outgoing.http_return = 404;
			outgoing.http_return_status = "File not found";
			return "";
		}

		// If this is a small file, cache it.
		if (bytes <= SMALL_FILE)
			m_fileCache.insert(pair<string, RefCountedPtr<CachedFile> >(uri, cachedFile));

	}

	(*outgoing.out) << "HTTP/1.1 200 OK\r\n"
	"Date: " << getCurrentTime(HUM_READ) << "\r\n"
	"Server: MTConnectAgent\r\n"
	"Connection: close\r\n"
	"Content-Length: " << cachedFile->m_size << "\r\n"
	"Expires: " << getCurrentTime(time(nullptr) + 60 * 60 * 24, 0, HUM_READ) << "\r\n"
	"Content-Type: " << contentType << "\r\n\r\n";

	outgoing.out->write(cachedFile->m_buffer, cachedFile->m_size);
	outgoing.out->setstate(ios::badbit);

	return "";
}


void Agent::streamData(
	ostream& out,
	std::set<string> &filterSet,
	bool current,
	unsigned int interval,
	uint64_t start,
	unsigned int count,
	unsigned int heartbeat )
{
	// Create header
	string boundary = md5(intToString(time(nullptr)));

	ofstream log;
	if (m_logStreamData)
	{
		string filename = "Stream_" + getCurrentTime(LOCAL) + "_" +
							int64ToString((uint64_t) dlib::get_thread_id()) + ".log";
		log.open(filename.c_str());
	}

	out << "HTTP/1.1 200 OK\r\n"
			"Date: " << getCurrentTime(HUM_READ) << "\r\n"
			"Server: MTConnectAgent\r\n"
			"Expires: -1\r\n"
			"Connection: close\r\n"
			"Cache-Control: private, max-age=0\r\n"
			"Content-Type: multipart/x-mixed-replace;boundary=" << boundary << "\r\n"
			"Transfer-Encoding: chunked\r\n\r\n";

	// This object will automatically clean up all the observer from the
	// signalers in an exception proof manor.
	ChangeObserver observer;

	// Add observers
	std::set<string>::iterator iter;
	for (iter = filterSet.begin(); iter != filterSet.end(); ++iter)
		m_dataItemMap[*iter]->addObserver(&observer);

	uint64_t interMicros = interval * 1000u;
	uint64_t firstSeq = getFirstSequence();
	if (start < firstSeq)
		start = firstSeq;

	try
	{
		// Loop until the user closes the connection
		timestamper ts;
		while (out.good())
		{
			// Remember when we started this grab...
			uint64_t last = ts.get_timestamp();

			// Fetch sample data now resets the observer while holding the sequence
			// mutex to make sure that a new event will be recorded in the observer
			// when it returns.
			string content;
			uint64_t end;
			bool endOfBuffer = true;
			if (current)
				content = fetchCurrentData(filterSet, NO_START);
			else
			{
				// Check if we're falling too far behind. If we are, generate an 
				// MTConnectError and return.
				if (start < getFirstSequence())
				{
					g_logger << LWARN << "Client fell too far behind, disconnecting";
					throw ParameterError("OUT_OF_RANGE", "Client can't keep up with event stream, disconnecting");
				}
				else
				{
					// end and endOfBuffer are set during the fetch sample data while the 
					// mutex is held. This removed the race to check if we are at the end of 
					// the bufffer and setting the next start to the last sequence number 
					// sent.
					content = fetchSampleData(filterSet, start, count, end, endOfBuffer, &observer);
				}
		
				if (m_logStreamData)
					log << content << endl;
			}

			ostringstream str;

			// Make sure we're terminated with a <cr><nl>
			content.append("\r\n");
			out.setf(ios::dec, ios::basefield);
			str << "--" << boundary << "\r\n"
					"Content-type: text/xml\r\n"
					"Content-length: " << content.length() << "\r\n\r\n"
					<< content;

			string chunk = str.str();
			out.setf(ios::hex, ios::basefield);
			out << chunk.length() << "\r\n";
			out << chunk << "\r\n";
			out.flush();

			// Wait for up to frequency ms for something to arrive... Don't wait if 
			// we are not at the end of the buffer. Just put the next set after aInterval 
			// has elapsed. Check also if in the intervening time between the last fetch
			// and now. If so, we just spin through and wait the next interval.

			// Even if we are at the end of the buffer, or within range. If we are filtering,
			// we will need to make sure we are not spinning when there are no valid events
			// to be reported. we will waste cycles spinning on the end of the buffer when 
			// we should be in a heartbeat wait as well. 
			if (!endOfBuffer)
			{
				// If we're not at the end of the buffer, move to the end of the previous set and
				// begin filtering from where we left off.
				start = end;
		
				// For replaying of events, we will stream as fast as we can with a 1ms sleep
				// to allow other threads to run.
				dlib::sleep(1);
			}
			else
			{
				uint64 delta;

				if (!current)
				{
					// Busy wait to make sure the signal was actually signaled. We have observed that
					// a signal can occur in rare conditions where there are multiple threads listening
					// on separate condition variables and this pops out too soon. This will make sure
					// observer was actually signaled and instead of throwing an error will wait again
					// for the remaining hartbeat interval.
					delta = (ts.get_timestamp() - last) / 1000;
					while ( delta < heartbeat &&
							observer.wait(heartbeat - delta) &&
							!observer.wasSignaled() )
					{
						delta = (ts.get_timestamp() - last) / 1000;
					}

					{
						dlib::auto_mutex lock(*m_sequenceLock);

						// Make sure the observer was signaled!
						if (!observer.wasSignaled())
						{
							// If nothing came out during the last wait, we may have still have advanced
							// the sequence number. We should reset the start to something closer to the
							// current sequence. If we lock the sequence lock, we can check if the observer
							// was signaled between the time the wait timed out and the mutex was locked.
							// Otherwise, nothing has arrived and we set to the next sequence number to
							// the next sequence number to be allocated and continue.
							start = m_sequence;
						}
						else
						{
							// Get the sequence # signaled in the observer when the earliest event arrived.
							// This will allow the next set of data to be pulled. Any later events will have
							// greater sequence numbers, so this should not cause a problem. Also, signaled
							// sequence numbers can only decrease, never increase.
							start = observer.getSequence();
						}
					}
				}
		
				// Now wait the remainder if we triggered before the timer was up.
				delta = ts.get_timestamp() - last;
				if (delta < interMicros)
				{
					// Sleep the remainder
					dlib::sleep((interMicros - delta) / 1000);
				}
			}
		}
	}
	catch (ParameterError &aError)
	{
		g_logger << LINFO << "Caught a parameter error.";
		if (out.good())
		{
			ostringstream str;
			string content = printError(aError.m_code, aError.m_message);
			str << "--" << boundary << "\r\n"
			"Content-type: text/xml\r\n"
			"Content-length: " << content.length() << "\r\n\r\n"
			<< content;

			string chunk = str.str();
			out.setf(ios::hex, ios::basefield);
			out << chunk.length() << "\r\n";
			out << chunk << "\r\n";
			out.flush();
		}
	}
	catch (...)
	{
		g_logger << LWARN << "Error occurred during streaming data";
		if (out.good())
		{
			ostringstream str;
			string content = printError("INTERNAL_ERROR", "Unknown error occurred during streaming");
			str << "--" << boundary << "\r\n"
			"Content-type: text/xml\r\n"
			"Content-length: " << content.length() << "\r\n\r\n"
			<< content;

			string chunk = str.str();
			out.setf(ios::hex, ios::basefield);
			out << chunk.length() << "\r\n";
			out << chunk;
			out.flush();
		}
	}

	out.setstate(ios::badbit);
	// Observer is auto removed from signalers
}


string Agent::fetchCurrentData(std::set<string> &filterSet, uint64_t at)
{
	ComponentEventPtrArray events;
	uint64_t firstSeq, seq;
	{
		dlib::auto_mutex lock(*m_sequenceLock);
		firstSeq = getFirstSequence();
		seq = m_sequence;
		if (at == NO_START)
			m_latest.getComponentEvents(events, &filterSet);
		else
		{
			long pos = (long) m_slidingBuffer->get_element_id(at);
			long first = (long) m_slidingBuffer->get_element_id(firstSeq);
			long checkIndex = pos / m_checkpointFreq;
			long closestCp = checkIndex * m_checkpointFreq;
			unsigned long index;

			Checkpoint *ref;

			// Compute the closest checkpoint. If the checkpoint is after the
			// first checkpoint and before the next incremental checkpoint,
			// use first.
			if (first > closestCp && pos >= first)
			{
				ref = &m_first;
				// The checkpoint is inclusive of the "first" event. So we add one
				// so we don't duplicate effort.
				index = first + 1;
			}
			else
			{
				index = closestCp + 1;
				ref = &m_checkpoints[checkIndex];
			}

			Checkpoint check(*ref, &filterSet);

			// Roll forward from the checkpoint.
			for (; index <= (unsigned long) pos; index++)
				check.addComponentEvent(((*m_slidingBuffer)[(unsigned long)index]).getObject());

			check.getComponentEvents(events);
		}
	}

	return XmlPrinter::printSample(
		m_instanceId,
		m_slidingBufferSize,
		seq, firstSeq,
		m_sequence - 1,
		events);
}


string Agent::fetchSampleData(
	std::set<string> &filterSet,
	uint64_t start,
	unsigned int count,
	uint64_t &end,
	bool &endOfBuffer,
	ChangeObserver *observer)
{
	ComponentEventPtrArray results;
	uint64_t firstSeq;
	{
		dlib::auto_mutex lock(*m_sequenceLock);

		firstSeq = (m_sequence > m_slidingBufferSize) ?
						m_sequence - m_slidingBufferSize : 1;

		// START SHOULD BE BETWEEN 0 AND SEQUENCE NUMBER
		start = (start <= firstSeq) ? firstSeq : start;

		uint64_t i;
		for (i = start; results.size() < count && i < m_sequence; i++)
		{
			// Filter out according to if it exists in the list
			const string &dataId = (*m_slidingBuffer)[i]->getDataItem()->getId();
			if (filterSet.count(dataId) > 0)
			{
				ComponentEventPtr event = (*m_slidingBuffer)[i];
				results.push_back(event);
			}
		}

		end = i;
		if (i >= m_sequence)
			endOfBuffer = true;
		else
			endOfBuffer = false;
	
		if (observer)
			observer->reset();
	}

	return XmlPrinter::printSample(
		m_instanceId,
		m_slidingBufferSize,
		end,
		firstSeq,
		m_sequence - 1,
		results);
}


string Agent::printError(const string &errorCode, const string &text)
{
	g_logger << LDEBUG << "Returning error " << errorCode << ": " << text;
	return XmlPrinter::printError(
		m_instanceId,
		m_slidingBufferSize,
		m_sequence,
		errorCode,
		text);
}


string Agent::devicesAndPath(const string &path, const string &device)
{
	string dataPath = "";

	if (!device.empty())
	{
		string prefix = "//Devices/Device[@name=\"" + device + "\"]";

		if (!path.empty())
		{
			istringstream toParse(path);
			string token;

			// Prefix path (i.e. "path1|path2" => "{prefix}path1|{prefix}path2")
			while (getline(toParse, token, '|'))
				dataPath += prefix + token + "|";

			dataPath.erase(dataPath.length()-1);
		}
		else
			dataPath = prefix;
	}
	else
		dataPath = (!path.empty()) ? path : "//Devices/Device";

	return dataPath;
}


int Agent::checkAndGetParam(
	const key_value_map &queries,
	const string &param,
	const int defaultValue,
	const int minValue,
	bool minError,
	const int maxValue)
{
	if (!queries.count(param))
		return defaultValue;

	if (queries[param].empty())
		throw ParameterError("QUERY_ERROR", "'" + param + "' cannot be empty.");

	if (!isNonNegativeInteger(queries[param]))
		throw ParameterError("OUT_OF_RANGE","'" + param + "' must be a positive integer.");

	long int value = strtol(queries[param].c_str(), nullptr, 10);

	if (minValue != NO_VALUE32 && value < minValue)
	{
		if (minError)
			throw ParameterError("OUT_OF_RANGE", "'" + param + "' must be greater than or equal to " + intToString(minValue) + ".");

		return minValue;
	}

	if (maxValue != NO_VALUE32 && value > maxValue)
		throw ParameterError("OUT_OF_RANGE", "'" + param + "' must be less than or equal to " + intToString(maxValue) + ".");

	return value;
}


uint64_t Agent::checkAndGetParam64(
	const key_value_map &queries,
	const string &param,
	const uint64_t defaultValue,
	const uint64_t minValue,
	bool minError,
	const uint64_t maxValue )
{
	if (!queries.count(param))
		return defaultValue;

	if (queries[param].empty())
		throw ParameterError("QUERY_ERROR", "'" + param + "' cannot be empty.");

	if (!isNonNegativeInteger(queries[param]))
	{
		throw ParameterError("OUT_OF_RANGE",
						"'" + param + "' must be a positive integer.");
	}

	uint64_t value = strtoull(queries[param].c_str(), nullptr, 10);

	if (minValue != NO_VALUE64 && value < minValue)
	{
		if (minError)
			throw ParameterError("OUT_OF_RANGE", "'" + param + "' must be greater than or equal to " + intToString(minValue) + ".");

		return minValue;
	}

	if (maxValue != NO_VALUE64 && value > maxValue)
		throw ParameterError("OUT_OF_RANGE", "'" + param + "' must be less than or equal to " + int64ToString(maxValue) + ".");

	return value;
}


DataItem *Agent::getDataItemByName(const string &device, const string &dataItemName)
{
	Device *dev = m_deviceMap[device];
	return (dev) ? dev->getDeviceDataItem(dataItemName) : nullptr;
}


void Agent::updateDom(Device *device)
{
	m_xmlParser->updateDevice(device);
}

