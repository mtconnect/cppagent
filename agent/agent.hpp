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

#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <list>

#include "dlib/md5.h"
#include "dlib/server.h"
#include "dlib/sliding_buffer.h"

#include "adapter.hpp"
#include "globals.hpp"
#include "xml_parser.hpp"
#include "xml_printer.hpp"
#include "checkpoint.hpp"
#include "service.hpp"
#include "asset.hpp"

class Adapter;
class ComponentEvent;
class DataItem;
class Device;

using namespace dlib;

typedef std::vector<std::pair<std::string, std::string>> AssetChangeList;

class Agent : public server_http
{
	class ParameterError
	{
	public:
	ParameterError(const std::string &aCode, const std::string &aMessage)
	{
		m_code = aCode;
		m_message = aMessage;
	}
	ParameterError(const ParameterError &aError)
	{
		m_code = aError.m_code;
		m_message = aError.m_message;
	}
	ParameterError &operator=(const ParameterError &aError)
	{
		m_code = aError.m_code;
		m_message = aError.m_message;
		return *this;
	}
	std::string m_code;
	std::string m_message;
	};

public:
	// Slowest frequency allowed
	static const int SLOWEST_FREQ = 2147483646;

	// Fastest frequency allowed
	static const int FASTEST_FREQ = 0;

	// Default count for sample query
	static const unsigned int DEFAULT_COUNT = 100;

	// Code to return when a parameter has no value
	static const int NO_VALUE32 = -1;
	static const uint64_t NO_VALUE64 = UINT64_MAX;

	// Code to return for no frequency specified
	static const int NO_FREQ = -2;

	// Code to return for no heartbeat specified
	static const int NO_HB = 0;

	// Code for no start value specified
	static const uint64_t NO_START = NO_VALUE64;

	// Small file size
	static const int SMALL_FILE = 10 * 1024; // 10k is considered small

public:
	// Load agent with the xml configuration 
	Agent(const std::string &configXmlPath, int aBufferSize, int aMaxAssets,
	  int aCheckpointFreq = 1000);

	// Virtual destructor
	virtual ~Agent();

	// Overridden method that is called per web request
	virtual const std::string on_request(
	const incoming_things &incoming,
	outgoing_things &outgoing
	);

	// Add an adapter to the agent
	Adapter *addAdapter(const std::string &device,
			const std::string &host,
			const unsigned int port,
			bool start = false,
			int aLegacyTimeout = 600);

	// Get device from device map
	Device *getDeviceByName(const std::string &name) { return m_deviceMap[name]; }
	Device *findDeviceByUUIDorName(const std::string &aId);
	const std::vector<Device *> &getDevices() { return m_devices; }

	// Add component events to the sliding buffer
	unsigned int addToBuffer(
	DataItem *dataItem,
	const std::string &value,
	std::string time = ""
	);

	// Asset management
	bool addAsset(Device *aDevice, const std::string &aId, const std::string &aAsset,
		  const std::string &aType,
		  const std::string &aTime = "");

	bool updateAsset(Device *aDevice, const std::string &aId, AssetChangeList &aList,
			 const std::string &aTime);

	bool removeAsset(Device *aDevice, const std::string &aId, const std::string &aTime);
	bool removeAllAssets(Device *aDevice, const std::string &aType, const std::string &aTime);

	// Message when adapter has connected and disconnected
	void disconnected(Adapter *anAdapter, std::vector<Device *> aDevices);
	void connected(Adapter *anAdapter, std::vector<Device *> aDevices);

	DataItem *getDataItemByName(
	const std::string &device,
	const std::string &name
	);

	ComponentEvent *getFromBuffer(uint64_t aSeq) const { return (*m_slidingBuffer)[aSeq]; }
	uint64_t getSequence() const { return m_sequence; }
	unsigned int getBufferSize() const { return m_slidingBufferSize; }
	unsigned int getMaxAssets() const { return m_maxAssets; }
	unsigned int getAssetCount() const { return m_assets.size(); }
	int getAssetCount(const std::string &aType) const
	{
	return const_cast<std::map<std::string, int>& >(m_assetCounts)[aType];
	}
	uint64_t getFirstSequence() const
	{
	if (m_sequence > m_slidingBufferSize)
		return m_sequence - m_slidingBufferSize;
	else
		return 1;
	}

	// For testing...
	void setSequence(uint64_t aSeq) { m_sequence = aSeq; }
	std::list<AssetPtr *> *getAssets() { return &m_assets; }

	// Starting
	virtual void start();

	// Shutdown
	void clear();

	void registerFile(const std::string &aUri, const std::string &aPath);
	void addMimeType(const std::string &aExt, const std::string &aType) { m_mimeTypes[aExt] = aType; }

	// PUT and POST handling
	void enablePut(bool aFlag = true) { m_putEnabled = aFlag; }
	bool isPutEnabled() { return m_putEnabled; }
	void allowPutFrom(const std::string &aHost) { m_putAllowedHosts.insert(aHost); }
	bool isPutAllowedFrom(const std::string &aHost) { return m_putAllowedHosts.count(aHost) > 0; }

	// For debugging
	void setLogStreamData(bool aLog) { m_logStreamData = aLog; }

	// Handle probe calls
	std::string handleProbe(const std::string &device);

	// Update DOM when key changes
	void updateDom(Device *aDevice);

protected:
	// HTTP methods to handle the 3 basic calls
	std::string handleCall(std::ostream &out,
			   const std::string &path,
			   const key_value_map &queries,
			   const std::string &call,
			   const std::string &device);

	// HTTP methods to handle the 3 basic calls
	std::string handlePut(std::ostream &out,
			  const std::string &path,
			  const key_value_map &queries,
			  const std::string &call,
			  const std::string &device);

	// Handle stream calls, which includes both current and sample
	std::string handleStream(std::ostream &out,
				 const std::string &path,
				 bool current,
				 unsigned int frequency,
				 uint64_t start = 0,
				 unsigned int count = 0,
				 unsigned int aHb = 10000);

	// Asset related methods
	std::string handleAssets(std::ostream &aOut,
				 const key_value_map &aQueries,
				 const std::string &aList);

	std::string storeAsset(std::ostream &aOut,
			   const key_value_map &aQueries,
			   const std::string &aAsset,
			   const std::string &aBody);

	// Stream the data to the user
	void streamData(std::ostream &out,
			std::set<std::string> &aFilterSet,
			bool current,
			unsigned int frequency,
			uint64_t start = 1,
			unsigned int count = 0,
			unsigned int aHb = 10000);

	// Fetch the current/sample data and return the XML in a std::string
	std::string fetchCurrentData(std::set<std::string> &aFilter, uint64_t at);
	std::string fetchSampleData(std::set<std::string> &aFilterSet,
				uint64_t start, unsigned int count, uint64_t &end,
				bool &endOfBuffer, ChangeObserver *aObserver = NULL);

	// Output an XML Error
	std::string printError(const std::string &errorCode, const std::string &text);

	// Handle the device/path parameters for the xpath search
	std::string devicesAndPath(
	const std::string &path,
	const std::string &device
	);

	// Get a file
	std::string handleFile(const std::string &aUri, outgoing_things &aOutgoing);

	bool isFile(const std::string &aUri) { return m_fileMap.count(aUri) > 0; }

	// Perform a check on parameter and return a value or a code
	int checkAndGetParam(
	const key_value_map &queries,
	const std::string &param,
	const int defaultValue,
	const int minValue = NO_VALUE32,
	bool minError = false,
	const int maxValue = NO_VALUE32
	);

	// Perform a check on parameter and return a value or a code
	uint64_t checkAndGetParam64(
	const key_value_map &queries,
	const std::string &param,
	const uint64_t defaultValue,
	const uint64_t minValue = NO_VALUE64,
	bool minError = false,
	const uint64_t maxValue = NO_VALUE64
	);

	// Find data items by name/id
	DataItem *getDataItemById(const std::string &id) { return m_dataItemMap[id]; }

protected:
	// Unique id based on the time of creation
	unsigned int m_instanceId;

	// Pointer to the configuration file for node access
	XmlParser *m_xmlParser;

	// For access to the sequence number and sliding buffer, use the mutex
	dlib::mutex *m_sequenceLock;
	dlib::mutex *m_assetLock;

	// Sequence number
	uint64_t m_sequence;

	// The sliding/circular buffer to hold all of the events/sample data
	dlib::sliding_buffer_kernel_1<ComponentEventPtr> *m_slidingBuffer;
	unsigned int m_slidingBufferSize;

	// Asset storage, circ buffer stores ids
	std::list<AssetPtr *> m_assets;
	AssetIndex m_assetMap;

	// Natural key indices for assets
	std::map<std::string, AssetIndex> m_assetIndices;
	unsigned int m_maxAssets;

	// Checkpoints
	Checkpoint m_latest;
	Checkpoint m_first;
	Checkpoint *m_checkpoints;

	int mCheckpointFreq, m_checkpointCount;

	// Data containers
	std::vector<Adapter *> m_adapters;
	std::vector<Device *> m_devices;
	std::map<std::string, Device *> m_deviceMap;
	std::map<std::string, DataItem *> m_dataItemMap;
	std::map<std::string, int> m_assetCounts;

	struct CachedFile : public RefCounted
	{
	char    *m_buffer;
	size_t   m_size;

	CachedFile() : m_buffer(NULL), m_size(0) { }

	CachedFile(const CachedFile &aFile)
		: m_size(aFile.m_size)
	{
		m_buffer = (char *)malloc(aFile.m_size);
		memcpy(m_buffer, aFile.m_buffer, aFile.m_size);
	}


	CachedFile(char *aBuffer, size_t aSize)
		: m_size(aSize)
	{
		m_buffer = (char *)malloc(aSize);
		memcpy(m_buffer, aBuffer, aSize);
	}

	CachedFile(size_t aSize)
		: m_size(aSize)
	{
		m_buffer = (char *)malloc(aSize);
	}


	~CachedFile()
	{
		free(m_buffer);
	}

	CachedFile &operator=(const CachedFile &aFile)
	{
		if (m_buffer != NULL) free(m_buffer);

		m_buffer = (char *)malloc(aFile.m_size);
		memcpy(m_buffer, aFile.m_buffer, aFile.m_size);
		m_size = aFile.m_size;
		return *this;
	}

	void allocate(size_t aSize)
	{
		m_buffer = (char *)malloc(aSize);
		m_size = aSize;
	}
	};

	// For file handling, small files will be cached
	std::map<std::string, std::string> m_fileMap;
	std::map<std::string, RefCountedPtr<CachedFile>> m_fileCache;
	std::map<std::string, std::string> m_mimeTypes;

	// Put handling controls
	bool m_putEnabled;
	std::set<std::string> m_putAllowedHosts;

	// For debugging
	bool m_logStreamData;
};

