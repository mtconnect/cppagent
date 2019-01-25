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
#include <mutex>
#include <chrono>
#include <memory>

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

struct OutgoingThings : public dlib::outgoing_things {
  OutgoingThings() : out(nullptr) {}
  std::ostream  *out;
};
typedef struct dlib::incoming_things IncomingThings;

class Agent : public server_http
{
	class ParameterError
	{
	public:
		ParameterError(const std::string &code, const std::string &message) 
		{
			m_code = code;
			m_message = message;
		}

		ParameterError(const ParameterError &anotherError)
		{
			m_code = anotherError.m_code;
			m_message = anotherError.m_message;
		}

		ParameterError &operator=(const ParameterError &anotherError)
		{
			m_code = anotherError.m_code;
			m_message = anotherError.m_message;
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
	Agent(
		const std::string &configXmlPath,
		int bufferSize,
		int maxAssets,
		std::chrono::milliseconds checkpointFreq = std::chrono::milliseconds{1000});

	// Virtual destructor
	virtual ~Agent();

	// Overridden method that is called per web request – not used
  // using httpRequest which is called from our own on_connect method.
	const std::string on_request (
		const incoming_things &incoming,
    outgoing_things &outgoing ) override {
    throw std::logic_error("Not Implemented");
    return "";
  }
  
  const std::string httpRequest(const IncomingThings &incoming,
                                OutgoingThings &outgoing);

	// Add an adapter to the agent
	Adapter *addAdapter(const std::string &device,
						const std::string &host,
						const unsigned int port,
						bool start = false,
						std::chrono::seconds legacyTimeout = std::chrono::seconds{600});

	// Get device from device map
	Device *getDeviceByName(const std::string &name);
	const Device *getDeviceByName(const std::string &name) const;
	Device *findDeviceByUUIDorName(const std::string &idOrName);
	const std::vector<Device *> &getDevices() const {
		return m_devices; }

	// Add component events to the sliding buffer
	unsigned int addToBuffer(
		DataItem *dataItem,
		const std::string &value,
		std::string time = ""
	);

	// Asset management
	bool addAsset(
		Device *device,
		const std::string &id,
		const std::string &asset,
		const std::string &type,
		const std::string &time = "");

	bool updateAsset(
		Device *device, 
		const std::string &id,
		AssetChangeList &list,
		const std::string &time);

	bool removeAsset(Device *device, const std::string &id, const std::string &time);
	bool removeAllAssets(Device *device, const std::string &type, const std::string &time);

	// Message when adapter has connected and disconnected
	void disconnected(Adapter *adapter, std::vector<Device *> devices);
	void connected(Adapter *adapter, std::vector<Device *> devices);

	DataItem *getDataItemByName(const std::string &deviceName, const std::string &dataItemName );

	ComponentEvent *getFromBuffer(uint64_t seq) const {
		return (*m_slidingBuffer)[seq]; }
	uint64_t getSequence() const {
		return m_sequence; }
	unsigned int getBufferSize() const {
		return m_slidingBufferSize; }
	unsigned int getMaxAssets() const {
		return m_maxAssets; }
	unsigned int getAssetCount() const {
		return m_assets.size(); }

	int getAssetCount(const std::string &type) const
	{ 
		const auto assetPos = m_assetCounts.find(type);
		if(assetPos != m_assetCounts.end())
			return assetPos->second;
		return 0;
	}

	uint64_t getFirstSequence() const
	{
		if (m_sequence > m_slidingBufferSize)
			return m_sequence - m_slidingBufferSize;
		else
			return 1;
	}

	// For testing...
	void setSequence(uint64_t seq) {
		m_sequence = seq; }
	std::list<AssetPtr *> *getAssets() {
		return &m_assets; }

	// Starting
	virtual void start();

	// Shutdown
	void clear();

	void registerFile(const std::string &uri, const std::string &path);
	void addMimeType(const std::string &ext, const std::string &type) {
		m_mimeTypes[ext] = type; }

	// PUT and POST handling
	void enablePut(bool flag = true) {
		m_putEnabled = flag; }
	bool isPutEnabled() const {
		return m_putEnabled; }
	void allowPutFrom(const std::string &host) {
		m_putAllowedHosts.insert(host); }
	bool isPutAllowedFrom(const std::string &host) const {
		return m_putAllowedHosts.find(host) != m_putAllowedHosts.end(); }

	// For debugging
	void setLogStreamData(bool log) {
		m_logStreamData = log; }

	// Handle probe calls
	std::string handleProbe(const std::string &device);

	// Update DOM when key changes
	void updateDom(Device *device);

protected:
  
  virtual void on_connect (
     std::istream& in,
     std::ostream& out,
     const std::string& foreign_ip,
     const std::string& local_ip,
     unsigned short foreign_port,
     unsigned short local_port,
     dlib::uint64
     ) override;
  
	// HTTP methods to handle the 3 basic calls
	std::string handleCall(
		std::ostream &out,
		const std::string &path,
		const key_value_map &queries,
		const std::string &call,
		const std::string &device);

	// HTTP methods to handle the 3 basic calls
	std::string handlePut(
		std::ostream &out,
		const std::string &path,
		const key_value_map &queries,
		const std::string &call,
		const std::string &device);

	// Handle stream calls, which includes both current and sample
	std::string handleStream(
		std::ostream &out,
		const std::string &path,
		bool current,
		unsigned int frequency,
		uint64_t start = 0,
		unsigned int count = 0,
		std::chrono::milliseconds heartbeat = std::chrono::milliseconds{10000});

	// Asset related methods
	std::string handleAssets(
		std::ostream &out,
		const key_value_map &queries,
		const std::string &list);

	std::string storeAsset(
		std::ostream &out,
		const key_value_map &queries,
		const std::string &asset,
		const std::string &body);

	// Stream the data to the user
	void streamData(
		std::ostream &out,
		std::set<std::string> &filterSet,
		bool current,
		unsigned int frequency,
		uint64_t start = 1,
		unsigned int count = 0,
		std::chrono::milliseconds heartbeat = std::chrono::milliseconds{10000});

	// Fetch the current/sample data and return the XML in a std::string
	std::string fetchCurrentData(std::set<std::string> &filterSet, uint64_t at);
	std::string fetchSampleData(
		std::set<std::string> &filterSet,
		uint64_t start,
		unsigned int count,
		uint64_t &end,
		bool &endOfBuffer,
		ChangeObserver *observer = nullptr);

	// Output an XML Error
	std::string printError(const std::string &errorCode, const std::string &text);

	// Handle the device/path parameters for the xpath search
	std::string devicesAndPath(
		const std::string &path,
		const std::string &device );

	// Get a file
	std::string handleFile(const std::string &uri, OutgoingThings &outgoing);

	bool isFile(const std::string &uri) const {
		return m_fileMap.find(uri) != m_fileMap.end(); }

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
	DataItem * getDataItemById(const std::string &id) const
	{
		auto diPos = m_dataItemMap.find(id);
		if(diPos != m_dataItemMap.end())
			return diPos->second;
		return nullptr;
	}

protected:
	// Unique id based on the time of creation
	uint64_t m_instanceId;

	// Pointer to the configuration file for node access
	XmlParser *m_xmlParser;

	// For access to the sequence number and sliding buffer, use the mutex
	std::mutex m_sequenceLock;
	std::mutex m_assetLock;

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

	int m_checkpointFreq;
	int m_checkpointCount;

	// Data containers
	std::vector<Adapter *> m_adapters;
	std::vector<Device *> m_devices;
	std::map<std::string, Device *> m_deviceNameMap;
  std::map<std::string, Device *> m_deviceUuidMap;
	std::map<std::string, DataItem *> m_dataItemMap;
	std::map<std::string, int> m_assetCounts;

	struct CachedFile : public RefCounted 
	{
		std::unique_ptr<char[]> m_buffer;
		size_t m_size;

		CachedFile() :
			m_buffer(nullptr),
			m_size(0)
		{
		}

		CachedFile(const CachedFile &file) :
			m_buffer(nullptr),
			m_size(file.m_size)
		{
			m_buffer = std::make_unique<char[]>(file.m_size);
			memcpy(m_buffer.get(), file.m_buffer.get(), file.m_size);
		}

		CachedFile(char *buffer, size_t size) :
			m_buffer(nullptr),
			m_size(size)
		{
			m_buffer = std::make_unique<char[]>(m_size);
			memcpy(m_buffer.get(), buffer, size);
		}

		CachedFile(size_t size) :
			m_buffer(nullptr),
			m_size(size)
		{
			m_buffer = std::make_unique<char[]>(m_size);
		}

		~CachedFile()
		{
			m_buffer.reset();
		}

		CachedFile &operator=(const CachedFile &file)
		{
			m_buffer.release();
			m_buffer = std::make_unique<char[]>(file.m_size);
			memcpy(m_buffer.get(), file.m_buffer.get(), file.m_size);
			m_size = file.m_size;
			return *this;
		}

		void allocate(size_t size)
		{
			m_buffer.release();
			m_buffer = std::make_unique<char[]>(size);
			m_size = size;
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

