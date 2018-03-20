//
// Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the AMT nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// DISCLAIMER OF WARRANTY. ALL MTCONNECT MATERIALS AND SPECIFICATIONS PROVIDED
// BY AMT, MTCONNECT OR ANY PARTICIPANT TO YOU OR ANY PARTY ARE PROVIDED "AS IS"
// AND WITHOUT ANY WARRANTY OF ANY KIND. AMT, MTCONNECT, AND EACH OF THEIR
// RESPECTIVE MEMBERS, OFFICERS, DIRECTORS, AFFILIATES, SPONSORS, AND AGENTS
// (COLLECTIVELY, THE "AMT PARTIES") AND PARTICIPANTS MAKE NO REPRESENTATION OR
// WARRANTY OF ANY KIND WHATSOEVER RELATING TO THESE MATERIALS, INCLUDING, WITHOUT
// LIMITATION, ANY EXPRESS OR IMPLIED WARRANTY OF NONINFRINGEMENT,
// MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
//
// LIMITATION OF LIABILITY. IN NO EVENT SHALL AMT, MTCONNECT, ANY OTHER AMT
// PARTY, OR ANY PARTICIPANT BE LIABLE FOR THE COST OF PROCURING SUBSTITUTE GOODS
// OR SERVICES, LOST PROFITS, LOSS OF USE, LOSS OF DATA OR ANY INCIDENTAL,
// CONSEQUENTIAL, INDIRECT, SPECIAL OR PUNITIVE DAMAGES OR OTHER DIRECT DAMAGES,
// WHETHER UNDER CONTRACT, TORT, WARRANTY OR OTHERWISE, ARISING IN ANY WAY OUT OF
// THIS AGREEMENT, USE OR INABILITY TO USE MTCONNECT MATERIALS, WHETHER OR NOT
// SUCH PARTY HAD ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.
//
#include "agent_test.hpp"
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <cppunit/portability/Stream.h>
#include <dlib/server.h>

#if defined(WIN32) && _MSC_VER < 1500
	typedef __int64 int64_t;
#endif

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(AgentTest);

using namespace std;


void AgentTest::setUp()
{
	XmlPrinter::setSchemaVersion("1.3");
	m_agent = nullptr;
	m_agent = new Agent("../samples/test_config.xml", 8, 4, 25);
	m_agentId = intToString(getCurrentTimeInSec());
	m_adapter = nullptr;
	m_queries.clear();
}


void AgentTest::tearDown()
{
	delete m_agent; m_agent = nullptr;
	m_adapter = nullptr;
}


void AgentTest::testConstructor()
{
	CPPUNIT_ASSERT_THROW(Agent("../samples/badPath.xml", 17, 8), std::runtime_error);
	CPPUNIT_ASSERT_NO_THROW(Agent("../samples/test_config.xml", 17, 8));
}


void AgentTest::testBadPath()
{
	string pathError = getFile("../samples/test_error.xml");

	{
		m_path = "/bad_path";
		PARSE_XML_RESPONSE;
		string message = (string) "The following path is invalid: " + m_path;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "UNSUPPORTED");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
	}

	{
		m_path = "/bad/path/";
		PARSE_XML_RESPONSE;
		string message = (string) "The following path is invalid: " + m_path;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "UNSUPPORTED");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
	}

	{
		m_path = "/LinuxCNC/current/blah";
		PARSE_XML_RESPONSE;
		string message = (string) "The following path is invalid: " + m_path;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "UNSUPPORTED");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
	}
}


void AgentTest::testBadXPath()
{
	m_path = "/current";
	key_value_map query;

	{
		query["path"] = "//////Linear";
		PARSE_XML_RESPONSE_QUERY(query);
		string message = (string) "The path could not be parsed. Invalid syntax: //////Linear";
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_XPATH");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
	}

	{
		query["path"] = "//Axes?//Linear";
		PARSE_XML_RESPONSE_QUERY(query);
		string message = (string) "The path could not be parsed. Invalid syntax: //Axes?//Linear";
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_XPATH");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
	}

	{
		query["path"] = "//Devices/Device[@name=\"I_DON'T_EXIST\"";
		PARSE_XML_RESPONSE_QUERY(query);
		string message = (string)
						 "The path could not be parsed. Invalid syntax: //Devices/Device[@name=\"I_DON'T_EXIST\"";
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_XPATH");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
	}
}


void AgentTest::testBadCount()
{
	m_path = "/sample";
	key_value_map query;

	{
		query["count"] = "NON_INTEGER";
		PARSE_XML_RESPONSE_QUERY(query);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'count' must be a positive integer.");
	}

	{
		query["count"] = "-123";
		PARSE_XML_RESPONSE_QUERY(query);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'count' must be a positive integer.");
	}

	{
		query["count"] = "0";
		PARSE_XML_RESPONSE_QUERY(query);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'count' must be greater than or equal to 1.");
	}

	{
		query["count"] = "999999999999999999";
		PARSE_XML_RESPONSE_QUERY(query);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
		string value("'count' must be less than or equal to ");
		value += intToString(m_agent->getBufferSize()) + ".";
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", value.c_str());
	}

}


void AgentTest::testBadFreq()
{
	m_path = "/sample";
	key_value_map query;

	{
		query["frequency"] = "NON_INTEGER";
		PARSE_XML_RESPONSE_QUERY(query);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'frequency' must be a positive integer.");
	}

	{
		query["frequency"] = "-123";
		PARSE_XML_RESPONSE_QUERY(query);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "'frequency' must be a positive integer.");
	}

	{
		query["frequency"] = "999999999999999999";
		PARSE_XML_RESPONSE_QUERY(query);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error",
										  "'frequency' must be less than or equal to 2147483646.");
	}
}


void AgentTest::testGoodPath()
{
	{
		m_path = "/current?path=//Power";
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='Power']//m:PowerState",
										  "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='Path']//m:Condition",
										  "");
	}
}


void AgentTest::testXPath()
{
	m_path = "/current";
	key_value_map query;

	{
		query["path"] = "//Rotary[@name='C']//DataItem[@category='SAMPLE' or @category='CONDITION']";
		PARSE_XML_RESPONSE_QUERY(query);

		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='Rotary']//m:SpindleSpeed",
										  "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='Rotary']//m:Load",
										  "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='Rotary']//m:Unavailable",
										  "");
	}
}


void AgentTest::testProbe()
{
	{
		m_path = "/probe";
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Devices/m:Device@name", "LinuxCNC");
	}

	{
		m_path = "/";
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Devices/m:Device@name", "LinuxCNC");
	}

	{
		m_path = "/LinuxCNC";
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Devices/m:Device@name", "LinuxCNC");
	}
}


void AgentTest::testEmptyStream()
{
	{
		m_path = "/current";
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PowerState", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
										  "//m:ComponentStream[@name='path']/m:Condition/m:Unavailable", 0);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
										  "//m:ComponentStream[@name='path']/m:Condition/m:Unavailable@qualifier",
										  0);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:RotaryMode", "SPINDLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ToolGroup", "UNAVAILABLE");
	}

	{
		m_path = "/sample";
		char line[80] = {0};
		sprintf(line, "%d", (int) m_agent->getSequence());
		PARSE_XML_RESPONSE_QUERY_KV("from", line);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Streams", 0);
	}
}


void AgentTest::testBadDevices()
{
	{
		m_path = "/LinuxCN/probe";
		PARSE_XML_RESPONSE;
		string message = (string) "Could not find the device 'LinuxCN'";
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "NO_DEVICE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", message.c_str());
	}
}


void AgentTest::testAddAdapter()
{
	CPPUNIT_ASSERT(!m_adapter);
	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);
}


void AgentTest::testAddToBuffer()
{
	string device("LinuxCNC"), key("badKey"), value("ON");
	int seqNum;
	ComponentEvent *event1, *event2;
	DataItem *di1 = m_agent->getDataItemByName(device, key);
	seqNum = m_agent->addToBuffer(di1, value, "NOW");
	CPPUNIT_ASSERT_EQUAL(0, seqNum);

	event1 = m_agent->getFromBuffer(seqNum);
	CPPUNIT_ASSERT(!event1);

	{
		m_path = "/sample";
		PARSE_XML_RESPONSE_QUERY_KV("from", "35");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Streams", 0);
	}

	key = "power";

	DataItem *di2 = m_agent->getDataItemByName(device, key);
	seqNum = m_agent->addToBuffer(di2, value, "NOW");
	event2 = m_agent->getFromBuffer(seqNum);
	CPPUNIT_ASSERT_EQUAL(2, (int) event2->refCount());

	{
		m_path = "/current";
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PowerState", "ON");
	}

	{
		m_path = "/sample";
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PowerState[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PowerState[2]", "ON");
	}
}


void AgentTest::testAdapter()
{
	m_path = "/sample";

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
	}

	m_adapter->processData("TIME|line|204");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Alarm[1]", "UNAVAILABLE");
	}

	m_adapter->processData("TIME|alarm|code|nativeCode|severity|state|description");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Alarm[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Alarm[2]", "description");
	}
}


void AgentTest::testCurrentAt()
{
	m_path = "/current";
	string key("at"), value;

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);

	// Get the current position
	int seq = m_agent->getSequence();
	char line[80] = {0};

	// Add many events
	for (int i = 1; i <= 100; i++)
	{
		sprintf(line, "TIME|line|%d", i);
		m_adapter->processData(line);
	}

	// Check each current at all the positions.
	for (int i = 0; i < 100; i++)
	{
		value = intToString(i + seq);
		sprintf(line, "%d", i + 1);
		PARSE_XML_RESPONSE_QUERY_KV(key, value);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", line);
	}

	// Test buffer wrapping
	// Add a large many events
	for (int i = 101; i <= 301; i++)
	{
		sprintf(line, "TIME|line|%d", i);
		m_adapter->processData(line);
	}

	// Check each current at all the positions.
	for (int i = 100; i < 301; i++)
	{
		value = intToString(i + seq);
		sprintf(line, "%d", i + 1);
		PARSE_XML_RESPONSE_QUERY_KV(key, value);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", line);
	}

	// Check the first couple of items in the list
	for (int j = 0; j < 10; j ++)
	{
		int i = m_agent->getSequence() - m_agent->getBufferSize() - seq + j;
		value = intToString(i + seq);
		sprintf(line, "%d", i + 1);
		PARSE_XML_RESPONSE_QUERY_KV(key, value);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", line);
	}

	// Test out of range...
	{
		int i = m_agent->getSequence() - m_agent->getBufferSize() - seq - 1;
		value = intToString(i + seq);
		sprintf(line, "'at' must be greater than or equal to %d.", i + seq + 1);
		PARSE_XML_RESPONSE_QUERY_KV(key, value);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", line);
	}
}


void AgentTest::testCurrentAt64()
{
	m_path = "/current";
	string key("at"), value;

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);

	// Get the current position
	char line[80] = {0};

	// Initialize the sliding buffer at a very large number.
	uint64_t start = (((uint64_t) 1) << 48) + 1317;
	m_agent->setSequence(start);

	// Add many events
	for (uint64_t i = 1; i <= 500; i++)
	{
		sprintf(line, "TIME|line|%d", (int) i);
		m_adapter->processData(line);
	}

	// Check each current at all the positions.
	for (uint64_t i = start + 300; i < start + 500; i++)
	{
		value = int64ToString(i);
		sprintf(line, "%d", (int)(i - start) + 1);
		PARSE_XML_RESPONSE_QUERY_KV(key, value);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", line);
	}
}


void AgentTest::testCurrentAtOutOfRange()
{
	m_path = "/current";
	string key("at"), value;

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);

	// Get the current position
	char line[80] = {0};

	// Add many events
	for (int i = 1; i <= 200; i++)
	{
		sprintf(line, "TIME|line|%d", i);
		m_adapter->processData(line);
	}

	int seq = m_agent->getSequence();

	{
		value = intToString(seq);
		sprintf(line, "'at' must be less than or equal to %d.", seq - 1);
		PARSE_XML_RESPONSE_QUERY_KV(key, value);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", line);
	}

	seq = m_agent->getFirstSequence() - 1;

	{
		value = intToString(seq);
		sprintf(line, "'at' must be greater than or equal to %d.", seq + 1);
		PARSE_XML_RESPONSE_QUERY_KV(key, value);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "OUT_OF_RANGE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", line);
	}
}


void AgentTest::testSampleAtNextSeq()
{
	m_path = "/sample";
	string key("from"), value;

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);

	// Get the current position
	char line[80] = {0};

	// Add many events
	for (int i = 1; i <= 300; i++)
	{
		sprintf(line, "TIME|line|%d", i);
		m_adapter->processData(line);
	}

	int seq = m_agent->getSequence();

	{
		value = intToString(seq);
		PARSE_XML_RESPONSE_QUERY_KV(key, value);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Streams", 0);
	}
}


void AgentTest::testSequenceNumberRollover()
{
#ifndef WIN32
	key_value_map kvm;

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);

	// Set the sequence number near MAX_UINT32
	m_agent->setSequence(0xFFFFFFA0);
	int64_t seq = m_agent->getSequence();
	CPPUNIT_ASSERT_EQUAL((int64_t) 0xFFFFFFA0, seq);

	// Get the current position
	char line[80] = {0};

	// Add many events
	for (int i = 0; i < 128; i++)
	{
		sprintf(line, "TIME|line|%d", i);
		m_adapter->processData(line);

		{
			m_path = "/current";
			PARSE_XML_RESPONSE;
			CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line@sequence",
											  int64ToString(seq + i).c_str());
			CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence",
											  int64ToString(seq + i + 1).c_str());
		}

		{
			m_path = "/sample";
			kvm["from"] = int64ToString(seq);
			kvm["count"] = "128";

			PARSE_XML_RESPONSE_QUERY(kvm);
			CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence",
											  int64ToString(seq + i + 1).c_str());

			for (int j = 0; j <= i; j++)
			{
				sprintf(line, "//m:DeviceStream//m:Line[%d]@sequence", j + 1);
				CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, line, int64ToString(seq + j).c_str());
			}
		}

		for (int j = 0; j <= i; j++)
		{
			m_path = "/sample";
			kvm["from"] = int64ToString(seq + j);
			kvm["count"] = "1";

			PARSE_XML_RESPONSE_QUERY(kvm);
			CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line@sequence",
											  int64ToString(seq + j).c_str());
			CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence",
											  int64ToString(seq + j + 1).c_str());
		}
	}

	CPPUNIT_ASSERT_EQUAL((uint64_t) 0xFFFFFFA0 + 128, m_agent->getSequence());
#endif
}


void AgentTest::testSampleCount()
{
	key_value_map kvm;

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);

	int64_t seq = m_agent->getSequence();

	// Get the current position
	char line[80] = {0};

	// Add many events
	for (int i = 0; i < 128; i++)
	{
		sprintf(line, "TIME|line|%d|Xact|%d", i, i);
		m_adapter->processData(line);
	}

	{
		m_path = "/sample";
		kvm["path"] = "//DataItem[@name='Xact']";
		kvm["from"] = int64ToString(seq);
		kvm["count"] = "10";

		PARSE_XML_RESPONSE_QUERY(kvm);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@nextSequence",
										  int64ToString(seq + 20).c_str());

		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Position", 10);

		// Make sure we got 10 lines
		for (int j = 0; j < 10; j++)
		{
			sprintf(line, "//m:DeviceStream//m:Position[%d]@sequence", j + 1);
			CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, line, int64ToString(seq + j * 2 + 1).c_str());
		}
	}

}


void AgentTest::testAdapterCommands()
{
	m_path = "/probe";

	Device *device = m_agent->getDeviceByName("LinuxCNC");
	CPPUNIT_ASSERT(device);
	CPPUNIT_ASSERT(!device->m_preserveUuid);

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);

	m_adapter->parseBuffer("* uuid: MK-1234\n");
	m_adapter->parseBuffer("* manufacturer: Big Tool\n");
	m_adapter->parseBuffer("* serialNumber: XXXX-1234\n");
	m_adapter->parseBuffer("* station: YYYY\n");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Device@uuid", "MK-1234");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Description@manufacturer", "Big Tool");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Description@serialNumber", "XXXX-1234");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Description@station", "YYYY");
	}

	device->m_preserveUuid = true;
	m_adapter->parseBuffer("* uuid: XXXXXXX\n");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Device@uuid", "MK-1234");
	}

}


void AgentTest::testAdapterDeviceCommand()
{
	delete m_agent; m_agent = nullptr;
	m_agent = new Agent("../samples/two_devices.xml", 8, 4, 25);

	m_path = "/probe";

	Device *device1 = m_agent->getDeviceByName("Device1");
	CPPUNIT_ASSERT(device1);
	Device *device2 = m_agent->getDeviceByName("Device2");
	CPPUNIT_ASSERT(device2);

	m_adapter = m_agent->addAdapter("*", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);
	CPPUNIT_ASSERT_EQUAL(static_cast<Device *>(nullptr), m_adapter->getDevice());

	m_adapter->parseBuffer("* device: device-2\n");
	CPPUNIT_ASSERT_EQUAL(device2, m_adapter->getDevice());

	m_adapter->parseBuffer("* device: device-1\n");
	CPPUNIT_ASSERT_EQUAL(device1, m_adapter->getDevice());

	m_adapter->parseBuffer("* device: Device2\n");
	CPPUNIT_ASSERT_EQUAL(device2, m_adapter->getDevice());

	m_adapter->parseBuffer("* device: Device1\n");
	CPPUNIT_ASSERT_EQUAL(device1, m_adapter->getDevice());
}


void AgentTest::testUUIDChange()
{
	m_path = "/probe";

	Device *device = m_agent->getDeviceByName("LinuxCNC");
	CPPUNIT_ASSERT(device);
	CPPUNIT_ASSERT(!device->m_preserveUuid);

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);

	m_adapter->parseBuffer("* uuid: MK-1234\n");
	m_adapter->parseBuffer("* manufacturer: Big Tool\n");
	m_adapter->parseBuffer("* serialNumber: XXXX-1234\n");
	m_adapter->parseBuffer("* station: YYYY\n");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Device@uuid", "MK-1234");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Description@manufacturer", "Big Tool");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Description@serialNumber", "XXXX-1234");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Description@station", "YYYY");
	}

	m_path = "/current?path=//Device[@uuid=\"MK-1234\"]";
	{
		// TODO: Fix and make sure dom is updated so this xpath will parse correctly.
		//PARSE_XML_RESPONSE_QUERY_KV("path", "//Device[@uuid=\"MK-1234\"]");
		// CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream@uuid", "MK-1234");
	}
}


void AgentTest::testFileDownload()
{
	string uri("/schemas/MTConnectDevices_1.1.xsd");

	// Register a file with the agent.
	m_agent->registerFile(uri, "./MTConnectDevices_1.1.xsd");

	// Reqyest the file...
	dlib::incoming_things incoming("", "", 0, 0);
	dlib::outgoing_things outgoing;
	incoming.request_type = "GET";
	incoming.path = uri;
	incoming.queries = m_queries;
	incoming.cookies = m_cookies;
	incoming.headers = m_incomingHeaders;

	outgoing.out = &m_out;

	m_result = m_agent->on_request(incoming, outgoing);
	CPPUNIT_ASSERT(m_result.empty());
	CPPUNIT_ASSERT(m_out.bad());
	CPPUNIT_ASSERT(m_out.str().find_last_of("TEST SCHEMA FILE 1234567890\n") != string::npos);
}


void AgentTest::testFailedFileDownload()
{
	m_path = "/schemas/MTConnectDevices_1.1.xsd";
	string error = "The following path is invalid: " + m_path;

	// Register a file with the agent.
	m_agent->registerFile(m_path, "./BadFileName.xsd");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode",
										  "UNSUPPORTED");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error", error.c_str());
	}
}


void AgentTest::testDuplicateCheck()
{
	m_path = "/sample";

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);
	m_adapter->setDupCheck(true);

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
	}

	m_adapter->processData("TIME|line|204");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
	}

	m_adapter->processData("TIME|line|204");
	m_adapter->processData("TIME|line|205");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]", "205");
	}
}


void AgentTest::testDuplicateCheckAfterDisconnect()
{
	m_path = "/sample";

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);
	m_adapter->setDupCheck(true);

	m_adapter->processData("TIME|line|204");
	m_adapter->processData("TIME|line|204");
	m_adapter->processData("TIME|line|205");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]", "205");
	}

	m_adapter->disconnected();

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]", "205");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[4]", "UNAVAILABLE");
	}

	m_adapter->connected();

	m_adapter->processData("TIME|line|205");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]", "205");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[4]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[5]", "205");
	}
}


void AgentTest::testAutoAvailable()
{
	m_path = "/sample";

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);
	m_adapter->setAutoAvailable(true);
	Device *d = m_agent->getDevices()[0];
	std::vector<Device *> devices;
	devices.push_back(d);

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[1]", "UNAVAILABLE");
	}

	m_agent->connected(m_adapter, devices);

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[2]", "AVAILABLE");
	}

	m_agent->disconnected(m_adapter, devices);

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[2]", "AVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[3]", "UNAVAILABLE");
	}

	m_agent->connected(m_adapter, devices);

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[2]", "AVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[3]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Availability[4]", "AVAILABLE");
	}

}


void AgentTest::testMultipleDisconnect()
{
	m_path = "/sample";

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);
	Device *d = m_agent->getDevices()[0];
	std::vector<Device *> devices;
	devices.push_back(d);

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//*[@dataItemId='p1'][1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Unavailable[@dataItemId='cmp']", 1);
	}

	m_agent->connected(m_adapter, devices);
	m_adapter->processData("TIME|block|GTH");
	m_adapter->processData("TIME|cmp|normal||||");


	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//*[@dataItemId='p1'][1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//*[@dataItemId='p1'][2]", "GTH");
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//*[@dataItemId='p1']", 2);

		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Unavailable[@dataItemId='cmp']", 1);
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Normal[@dataItemId='cmp']", 1);
	}

	m_agent->disconnected(m_adapter, devices);

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Unavailable[@dataItemId='cmp']", 2);
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Normal[@dataItemId='cmp']", 1);

		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//*[@dataItemId='p1'][2]", "GTH");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//*[@dataItemId='p1'][3]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//*[@dataItemId='p1']", 3);
	}

	m_agent->disconnected(m_adapter, devices);

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Unavailable[@dataItemId='cmp']", 2);
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Normal[@dataItemId='cmp']", 1);

		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//*[@dataItemId='p1'][3]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//*[@dataItemId='p1']", 3);
	}

	m_agent->connected(m_adapter, devices);
	m_adapter->processData("TIME|block|GTH");
	m_adapter->processData("TIME|cmp|normal||||");

	m_agent->disconnected(m_adapter, devices);

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Unavailable[@dataItemId='cmp']", 3);
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Normal[@dataItemId='cmp']", 2);

		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//*[@dataItemId='p1']", 5);
	}
}


void AgentTest::testIgnoreTimestamps()
{
	m_path = "/sample";

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);

	m_adapter->processData("TIME|line|204");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]@timestamp", "TIME");
	}

	m_adapter->setIgnoreTimestamps(true);
	m_adapter->processData("TIME|line|205");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]@timestamp", "TIME");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]@timestamp", "!TIME");
	}
}


void AgentTest::testAssetStorage()
{
	m_agent->enablePut();
	m_path = "/asset/123";
	string body = "<Part>TEST</Part>";
	key_value_map queries;

	queries["type"] = "Part";
	queries["device"] = "LinuxCNC";

	CPPUNIT_ASSERT_EQUAL((unsigned int) 4, m_agent->getMaxAssets());
	CPPUNIT_ASSERT_EQUAL((unsigned int) 0, m_agent->getAssetCount());

	{
		PARSE_XML_RESPONSE_PUT(body, queries);
		CPPUNIT_ASSERT_EQUAL((unsigned int) 1, m_agent->getAssetCount());
	}

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetBufferSize", "4");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST");
	}

	// The device should generate an asset changed event as well.
	m_path = "/current";

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:AssetChanged", "123");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:AssetChanged@assetType", "Part");
	}
}


void AgentTest::testAssetBuffer()
{
	m_agent->enablePut();
	m_path = "/asset/1";
	string body = "<Part>TEST 1</Part>";
	key_value_map queries;

	queries["device"] = "LinuxCNC";
	queries["type"] = "Part";

	CPPUNIT_ASSERT_EQUAL((unsigned int) 4, m_agent->getMaxAssets());
	CPPUNIT_ASSERT_EQUAL((unsigned int) 0, m_agent->getAssetCount());

	{
		PARSE_XML_RESPONSE_PUT(body, queries);
		CPPUNIT_ASSERT_EQUAL((unsigned int) 1, m_agent->getAssetCount());
		CPPUNIT_ASSERT_EQUAL(1, m_agent->getAssetCount("Part"));
	}

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 1");
	}

	// Make sure replace works properly
	{
		PARSE_XML_RESPONSE_PUT(body, queries);
		CPPUNIT_ASSERT_EQUAL((unsigned int) 1, m_agent->getAssetCount());
		CPPUNIT_ASSERT_EQUAL(1, m_agent->getAssetCount("Part"));
	}

	m_path = "/asset/2";
	body = "<Part>TEST 2</Part>";

	{
		PARSE_XML_RESPONSE_PUT(body, queries);
		CPPUNIT_ASSERT_EQUAL((unsigned int) 2, m_agent->getAssetCount());
		CPPUNIT_ASSERT_EQUAL(2, m_agent->getAssetCount("Part"));
	}

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 2");
	}

	m_path = "/asset/3";
	body = "<Part>TEST 3</Part>";

	{
		PARSE_XML_RESPONSE_PUT(body, queries);
		CPPUNIT_ASSERT_EQUAL((unsigned int) 3, m_agent->getAssetCount());
		CPPUNIT_ASSERT_EQUAL(3, m_agent->getAssetCount("Part"));
	}

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 3");
	}

	m_path = "/asset/4";
	body = "<Part>TEST 4</Part>";

	{
		PARSE_XML_RESPONSE_PUT(body, queries);
		CPPUNIT_ASSERT_EQUAL((unsigned int) 4, m_agent->getAssetCount());
	}

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 4");
		CPPUNIT_ASSERT_EQUAL(4, m_agent->getAssetCount("Part"));
	}

	// Test multiple asset get
	m_path = "/assets";
	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part[4]", "TEST 1");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part[3]", "TEST 2");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part[2]", "TEST 3");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part[1]", "TEST 4");
	}

	// Test multiple asset get with filter
	m_path = "/assets";
	{
		PARSE_XML_RESPONSE_QUERY(queries);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part[4]", "TEST 1");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part[3]", "TEST 2");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part[2]", "TEST 3");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part[1]", "TEST 4");
	}

	queries["count"] = "2";
	{
		PARSE_XML_RESPONSE_QUERY(queries);
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 2);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part[1]", "TEST 4");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part[2]", "TEST 3");
	}

	queries.erase("count");

	m_path = "/asset/5";
	body = "<Part>TEST 5</Part>";

	{
		PARSE_XML_RESPONSE_PUT(body, queries);
		CPPUNIT_ASSERT_EQUAL((unsigned int) 4, m_agent->getAssetCount());
		CPPUNIT_ASSERT_EQUAL(4, m_agent->getAssetCount("Part"));
	}

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 5");
	}

	m_path = "/asset/1";

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode",
										  "ASSET_NOT_FOUND");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error",
										  "Could not find asset: 1");
	}

	m_path = "/asset/3";
	body = "<Part>TEST 6</Part>";

	{
		PARSE_XML_RESPONSE_PUT(body, queries);
		CPPUNIT_ASSERT_EQUAL((unsigned int) 4, m_agent->getAssetCount());
		CPPUNIT_ASSERT_EQUAL(4, m_agent->getAssetCount("Part"));
	}

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 6");
	}

	m_path = "/asset/2";

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 2");
	}

	m_path = "/asset/2";
	body = "<Part>TEST 7</Part>";

	{
		PARSE_XML_RESPONSE_PUT(body, queries);
		CPPUNIT_ASSERT_EQUAL((unsigned int) 4, m_agent->getAssetCount());
		CPPUNIT_ASSERT_EQUAL(4, m_agent->getAssetCount("Part"));
	}

	m_path = "/asset/6";
	body = "<Part>TEST 8</Part>";

	{
		PARSE_XML_RESPONSE_PUT(body, queries);
		CPPUNIT_ASSERT_EQUAL((unsigned int) 4, m_agent->getAssetCount());
		CPPUNIT_ASSERT_EQUAL(4, m_agent->getAssetCount("Part"));
	}

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "4");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 8");
	}

	// Now since two and three have been modified, asset 4 should be removed.
	m_path = "/asset/4";

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode",
										  "ASSET_NOT_FOUND");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error",
										  "Could not find asset: 4");
	}
}


void AgentTest::testAssetError()
{
	m_path = "/asset/123";

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error@errorCode",
										  "ASSET_NOT_FOUND");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:MTConnectError/m:Errors/m:Error",
										  "Could not find asset: 123");
	}
}


void AgentTest::testAdapterAddAsset()
{
	testAddAdapter();

	m_adapter->processData("TIME|@ASSET@|111|Part|<Part>TEST 1</Part>");
	CPPUNIT_ASSERT_EQUAL((unsigned int) 4, m_agent->getMaxAssets());
	CPPUNIT_ASSERT_EQUAL((unsigned int) 1, m_agent->getAssetCount());

	m_path = "/asset/111";

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 1");
	}

}


void AgentTest::testMultiLineAsset()
{
	testAddAdapter();

	m_adapter->parseBuffer("TIME|@ASSET@|111|Part|--multiline--AAAA\n");
	m_adapter->parseBuffer("<Part>\n"
						 "  <PartXXX>TEST 1</PartXXX>\n"
						 "  Some Text\n"
						 "  <Extra>XXX</Extra>\n");
	m_adapter->parseBuffer("</Part>\n"
						 "--multiline--AAAA\n");
	CPPUNIT_ASSERT_EQUAL((unsigned int) 4, m_agent->getMaxAssets());
	CPPUNIT_ASSERT_EQUAL((unsigned int) 1, m_agent->getAssetCount());

	m_path = "/asset/111";

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part/m:PartXXX", "TEST 1");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part/m:Extra", "XXX");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part@assetId", "111");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part@deviceUuid", "000");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part@timestamp", "TIME");
	}

	// Make sure we can still add a line and we are out of multiline mode...
	m_path = "/current";
	m_adapter->processData("TIME|line|204");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", "204");
	}

}

void AgentTest::testAssetRefCounts()
{
	testAddAdapter();
	std::list<AssetPtr *> *assets = m_agent->getAssets();

	m_adapter->parseBuffer(
	R"ASSET(2018-02-19T22:54:03.0738Z|@ASSET@|M8010N9172N:1.0|CuttingTool|--multiline--SMOOTH
	<CuttingTool toolId="0" serialNumber="0" removed="False" assetId="M8010N9172N:1.0"><CuttingToolLifeCycle><ToolLife type="MINUTES" countDirection="UP" initial="0" limit="">1</ToolLife><ToolLife type="PART_COUNT" countDirection="UP" initial="0" limit="">1</ToolLife><Location type="POT" positiveOverlap="0" negativeOverlap="0">1</Location><ProgramToolGroup>0</ProgramToolGroup><ProgramToolNumber>1.0</ProgramToolNumber><CutterStatus><Status>USED</Status><Status>ALLOCATED</Status><Status>AVAILABLE</Status></CutterStatus><Measurements><FunctionalLength code="LF" nominal="0">0</FunctionalLength><CuttingDiameterMax code="DC" nominal="0">200</CuttingDiameterMax></Measurements></CuttingToolLifeCycle></CuttingTool>
	--multiline--SMOOTH
	)ASSET");
	CPPUNIT_ASSERT_EQUAL((unsigned int) 4, m_agent->getMaxAssets());
	CPPUNIT_ASSERT_EQUAL((unsigned int) 1, m_agent->getAssetCount());

	// Asset has two secondary indexes
	AssetPtr first(*(assets->front()));
	CPPUNIT_ASSERT_EQUAL((unsigned int) 4, first.getObject()->refCount());

	m_adapter->parseBuffer(R"ASSET(2018-02-19T22:54:03.1749Z|@ASSET@|M8010N9172N:1.2|CuttingTool|--multiline--SMOOTH
	<CuttingTool toolId="0" serialNumber="1" removed="False" assetId="M8010N9172N:1.2"><CuttingToolLifeCycle><ToolLife type="MINUTES" countDirection="UP" initial="0" limit=""></ToolLife><ToolLife type="PART_COUNT" countDirection="UP" initial="0" limit=""></ToolLife><Location type="POT" positiveOverlap="0" negativeOverlap="0">1</Location><ProgramToolGroup>0</ProgramToolGroup><ProgramToolNumber>1.2</ProgramToolNumber><CutterStatus><Status>NEW</Status><Status>ALLOCATED</Status><Status>AVAILABLE</Status></CutterStatus><Measurements><FunctionalLength code="LF" nominal="649640">649640</FunctionalLength><CuttingDiameterMax code="DC" nominal="-177708">100</CuttingDiameterMax></Measurements></CuttingToolLifeCycle></CuttingTool>
	--multiline--SMOOTH
	)ASSET");

	CPPUNIT_ASSERT_EQUAL((unsigned int) 2, m_agent->getAssetCount());
	CPPUNIT_ASSERT_EQUAL((unsigned int) 2, first.getObject()->refCount());

	m_adapter->parseBuffer(R"ASSET(2018-02-19T22:54:03.2760Z|@ASSET@|M8010N9172N:1.0|CuttingTool|--multiline--SMOOTH
	<CuttingTool toolId="0" serialNumber="0" removed="False" assetId="M8010N9172N:1.0"><CuttingToolLifeCycle><ToolLife type="MINUTES" countDirection="UP" initial="0" limit=""></ToolLife><ToolLife type="PART_COUNT" countDirection="UP" initial="0" limit=""></ToolLife><Location type="POT" positiveOverlap="0" negativeOverlap="0">1</Location><ProgramToolGroup>0</ProgramToolGroup><ProgramToolNumber>1.0</ProgramToolNumber><CutterStatus><Status>NEW</Status><Status>ALLOCATED</Status><Status>AVAILABLE</Status></CutterStatus><Measurements><FunctionalLength code="LF" nominal="0">0</FunctionalLength><CuttingDiameterMax code="DC" nominal="0">0</CuttingDiameterMax></Measurements></CuttingToolLifeCycle></CuttingTool>
	--multiline--SMOOTH
	)ASSET");
	m_adapter->parseBuffer(R"ASSET(2018-02-19T22:54:03.3771Z|@ASSET@|M8010N9172N:2.5|CuttingTool|--multiline--SMOOTH
	<CuttingTool toolId="0" serialNumber="0" removed="False" assetId="M8010N9172N:2.5"><CuttingToolLifeCycle><ToolLife type="MINUTES" countDirection="UP" initial="0" limit="">11</ToolLife><ToolLife type="PART_COUNT" countDirection="UP" initial="0" limit="">4</ToolLife><Location type="POT" positiveOverlap="0" negativeOverlap="0">2</Location><ProgramToolGroup>0</ProgramToolGroup><ProgramToolNumber>2.5</ProgramToolNumber><CutterStatus><Status>USED</Status><Status>ALLOCATED</Status><Status>AVAILABLE</Status></CutterStatus><Measurements><FunctionalLength code="LF" nominal="615207">615207</FunctionalLength><CuttingDiameterMax code="DC" nominal="-174546">200</CuttingDiameterMax></Measurements></CuttingToolLifeCycle></CuttingTool>
	--multiline--SMOOTH
	)ASSET");
	m_adapter->parseBuffer(R"ASSET(2018-02-19T22:54:03.4782Z|@ASSET@|M8010N9172N:2.2|CuttingTool|--multiline--SMOOTH
	<CuttingTool toolId="0" serialNumber="0" removed="False" assetId="M8010N9172N:2.2"><CuttingToolLifeCycle><ToolLife type="MINUTES" countDirection="UP" initial="0" limit="">11</ToolLife><ToolLife type="PART_COUNT" countDirection="UP" initial="0" limit="">4</ToolLife><Location type="POT" positiveOverlap="0" negativeOverlap="0">2</Location><ProgramToolGroup>0</ProgramToolGroup><ProgramToolNumber>2.2</ProgramToolNumber><CutterStatus><Status>USED</Status><Status>ALLOCATED</Status><Status>AVAILABLE</Status></CutterStatus><Measurements><FunctionalLength code="LF" nominal="615207">615207</FunctionalLength><CuttingDiameterMax code="DC" nominal="174546">200</CuttingDiameterMax></Measurements></CuttingToolLifeCycle></CuttingTool>
	--multiline--SMOOTH
	)ASSET");

	// First asset should now be removed (we are holding the one ref)
	CPPUNIT_ASSERT_EQUAL((unsigned int) 1, first.getObject()->refCount());

	// Check next asset
	AssetPtr second(*(assets->front()));
	CPPUNIT_ASSERT_EQUAL((unsigned int) 2, second.getObject()->refCount());
	CPPUNIT_ASSERT_EQUAL(string("M8010N9172N:1.2"), second.getObject()->getAssetId());

	// Update the asset
	m_adapter->parseBuffer(R"ASSET(2018-02-19T22:54:03.1749Z|@ASSET@|M8010N9172N:1.2|CuttingTool|--multiline--SMOOTH
	<CuttingTool toolId="0" serialNumber="1" removed="False" assetId="M8010N9172N:1.2"><CuttingToolLifeCycle><ToolLife type="MINUTES" countDirection="UP" initial="0" limit=""></ToolLife><ToolLife type="PART_COUNT" countDirection="UP" initial="0" limit=""></ToolLife><Location type="POT" positiveOverlap="0" negativeOverlap="0">1</Location><ProgramToolGroup>0</ProgramToolGroup><ProgramToolNumber>1.2</ProgramToolNumber><CutterStatus><Status>NEW</Status><Status>ALLOCATED</Status><Status>AVAILABLE</Status></CutterStatus><Measurements><FunctionalLength code="LF" nominal="649640">649640</FunctionalLength><CuttingDiameterMax code="DC" nominal="-177708">100</CuttingDiameterMax></Measurements></CuttingToolLifeCycle></CuttingTool>
	--multiline--SMOOTH
	)ASSET");

	// should be deleted
	CPPUNIT_ASSERT_EQUAL((unsigned int) 1, second.getObject()->refCount());
}


void AgentTest::testBadAsset()
{
	testAddAdapter();

	m_adapter->parseBuffer("TIME|@ASSET@|111|CuttingTool|--multiline--AAAA\n");
	m_adapter->parseBuffer((getFile("asset4.xml") + "\n").c_str());
	m_adapter->parseBuffer("--multiline--AAAA\n");
	CPPUNIT_ASSERT_EQUAL((unsigned int) 0, m_agent->getAssetCount());
}


void AgentTest::testAssetProbe()
{
	m_agent->enablePut();
	m_path = "/asset/1";
	string body = "<Part>TEST 1</Part>";
	key_value_map queries;

	queries["device"] = "LinuxCNC";
	queries["type"] = "Part";

	m_path = "/asset/1";
	{
		PARSE_XML_RESPONSE_PUT(body, queries);
		CPPUNIT_ASSERT_EQUAL((unsigned int) 1, m_agent->getAssetCount());
	}
	m_path = "/asset/2";
	{
		PARSE_XML_RESPONSE_PUT(body, queries);
		CPPUNIT_ASSERT_EQUAL((unsigned int) 2, m_agent->getAssetCount());
	}

	{
		m_path = "/probe";
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header/m:AssetCounts/m:AssetCount@assetType", "Part");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header/m:AssetCounts/m:AssetCount", "2");
	}
}


void AgentTest::testAssetRemoval()
{
	m_agent->enablePut();
	m_path = "/asset/1";
	string body = "<Part>TEST 1</Part>";
	key_value_map queries;

	queries["device"] = "LinuxCNC";
	queries["type"] = "Part";

	CPPUNIT_ASSERT_EQUAL((unsigned int) 4, m_agent->getMaxAssets());
	CPPUNIT_ASSERT_EQUAL((unsigned int) 0, m_agent->getAssetCount());

	{
		PARSE_XML_RESPONSE_PUT(body, queries);
		CPPUNIT_ASSERT_EQUAL((unsigned int) 1, m_agent->getAssetCount());
		CPPUNIT_ASSERT_EQUAL(1, m_agent->getAssetCount("Part"));
	}

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 1");
	}

	// Make sure replace works properly
	{
		PARSE_XML_RESPONSE_PUT(body, queries);
		CPPUNIT_ASSERT_EQUAL((unsigned int) 1, m_agent->getAssetCount());
		CPPUNIT_ASSERT_EQUAL(1, m_agent->getAssetCount("Part"));
	}

	m_path = "/asset/2";
	body = "<Part>TEST 2</Part>";

	{
		PARSE_XML_RESPONSE_PUT(body, queries);
		CPPUNIT_ASSERT_EQUAL((unsigned int) 2, m_agent->getAssetCount());
		CPPUNIT_ASSERT_EQUAL(2, m_agent->getAssetCount("Part"));
	}

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "2");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 2");
	}

	m_path = "/asset/3";
	body = "<Part>TEST 3</Part>";

	{
		PARSE_XML_RESPONSE_PUT(body, queries);
		CPPUNIT_ASSERT_EQUAL((unsigned int) 3, m_agent->getAssetCount());
		CPPUNIT_ASSERT_EQUAL(3, m_agent->getAssetCount("Part"));
	}

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 3");
	}


	m_path = "/asset/2";
	body = "<Part removed='true'>TEST 2</Part>";

	{
		PARSE_XML_RESPONSE_PUT(body, queries);
		CPPUNIT_ASSERT_EQUAL((unsigned int) 3, m_agent->getAssetCount());
		CPPUNIT_ASSERT_EQUAL(3, m_agent->getAssetCount("Part"));
	}

	m_path = "/current";
	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "2");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
	}

	m_path = "/assets";
	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 2);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 1");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
	}

	m_queries["removed"] = "true";
	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 3);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 2");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]@removed", "true");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[3]", "TEST 1");
	}
}


void AgentTest::testAssetRemovalByAdapter()
{

	testAddAdapter();

	CPPUNIT_ASSERT_EQUAL((unsigned int) 4, m_agent->getMaxAssets());

	m_adapter->processData("TIME|@ASSET@|111|Part|<Part>TEST 1</Part>");
	CPPUNIT_ASSERT_EQUAL((unsigned int) 1, m_agent->getAssetCount());


	m_adapter->processData("TIME|@ASSET@|112|Part|<Part>TEST 2</Part>");
	CPPUNIT_ASSERT_EQUAL((unsigned int) 2, m_agent->getAssetCount());

	m_adapter->processData("TIME|@ASSET@|113|Part|<Part>TEST 3</Part>");
	CPPUNIT_ASSERT_EQUAL((unsigned int) 3, m_agent->getAssetCount());

	m_path = "/current";
	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "113");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
	}

	m_adapter->processData("TIME|@REMOVE_ASSET@|112\r");
	CPPUNIT_ASSERT_EQUAL((unsigned int) 3, m_agent->getAssetCount());

	m_path = "/current";
	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "112");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
	}

	m_path = "/assets";
	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 2);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 1");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
	}

	// TODO: When asset is removed and the content is literal, it will
	// not regenerate the attributes for the asset.
	m_queries["removed"] = "true";
	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 3);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[3]", "TEST 1");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 2");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
	}
}


void AgentTest::testAssetStorageWithoutType()
{
	m_agent->enablePut();
	m_path = "/asset/123";
	string body = "<Part>TEST</Part>";
	key_value_map queries;

	queries["device"] = "LinuxCNC";

	CPPUNIT_ASSERT_EQUAL((unsigned int) 4, m_agent->getMaxAssets());
	CPPUNIT_ASSERT_EQUAL((unsigned int) 0, m_agent->getAssetCount());

	{
		PARSE_XML_RESPONSE_PUT(body, queries);
		CPPUNIT_ASSERT_EQUAL((unsigned int) 0, m_agent->getAssetCount());
	}
}


void AgentTest::testAssetAdditionOfAssetChanged12()
{
	string version = XmlPrinter::getSchemaVersion();
	XmlPrinter::setSchemaVersion("1.2");

	delete m_agent; m_agent= nullptr;
	m_agent = new Agent("../samples/min_config.xml", 8, 4, 25);

	{
		m_path = "/probe";
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_CHANGED']", 1);
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_REMOVED']", 0);
	}

	XmlPrinter::setSchemaVersion(version);
}


void AgentTest::testAssetAdditionOfAssetRemoved13()
{
	string version = XmlPrinter::getSchemaVersion();
	XmlPrinter::setSchemaVersion("1.3");

	delete m_agent; m_agent= nullptr;
	m_agent = new Agent("../samples/min_config.xml", 8, 4, 25);

	{
		m_path = "/probe";
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_CHANGED']", 1);
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DataItem[@type='ASSET_REMOVED']", 1);
	}

	XmlPrinter::setSchemaVersion(version);
}


void AgentTest::testAssetPrependId()
{
	testAddAdapter();

	m_adapter->processData("TIME|@ASSET@|@1|Part|<Part>TEST 1</Part>");
	CPPUNIT_ASSERT_EQUAL((unsigned int) 4, m_agent->getMaxAssets());
	CPPUNIT_ASSERT_EQUAL((unsigned int) 1, m_agent->getAssetCount());

	m_path = "/asset/0001";

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part", "TEST 1");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Part@assetId", "0001");
	}
}


void AgentTest::testAssetWithSimpleCuttingItems()
{
	XmlPrinter::clearAssetsNamespaces();
	XmlPrinter::addAssetsNamespace("urn:machine.com:MachineAssets:1.3",
								   "http://www.machine.com/schemas/MachineAssets_1.3.xsd",
								   "x");

	testAddAdapter();

	m_adapter->parseBuffer("TIME|@ASSET@|XXX.200|CuttingTool|--multiline--AAAA\n");
	m_adapter->parseBuffer((getFile("asset5.xml") + "\n").c_str());
	m_adapter->parseBuffer("--multiline--AAAA\n");
	CPPUNIT_ASSERT_EQUAL((unsigned int) 1, m_agent->getAssetCount());

	m_path = "/asset/XXX.200";

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='1']/m:ItemLife", "0");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='1']/m:ItemLife@type", "PART_COUNT");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='1']/m:ItemLife@countDirection", "UP");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='1']/m:ItemLife@initial", "0");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='1']/m:ItemLife@limit", "0");

		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='1']/x:ItemCutterStatus/m:Status", "AVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='2']/x:ItemCutterStatus/m:Status", "USED");

		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='4']/m:ItemLife", "0");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='4']/m:ItemLife@type", "PART_COUNT");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='4']/m:ItemLife@countDirection", "UP");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='4']/m:ItemLife@initial", "0");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:CuttingItem[@indices='4']/m:ItemLife@limit", "0");
	}

	XmlPrinter::clearAssetsNamespaces();
}


void AgentTest::testRemoveLastAssetChanged()
{
	testAddAdapter();

	CPPUNIT_ASSERT_EQUAL((unsigned int) 4, m_agent->getMaxAssets());

	m_adapter->processData("TIME|@ASSET@|111|Part|<Part>TEST 1</Part>");
	CPPUNIT_ASSERT_EQUAL((unsigned int) 1, m_agent->getAssetCount());

	m_path = "/current";
	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "111");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
	}

	m_adapter->processData("TIME|@REMOVE_ASSET@|111");
	CPPUNIT_ASSERT_EQUAL((unsigned int) 1, m_agent->getAssetCount());

	m_path = "/current";
	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "111");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
	}
}


void AgentTest::testRemoveAllAssets()
{
	testAddAdapter();

	CPPUNIT_ASSERT_EQUAL((unsigned int) 4, m_agent->getMaxAssets());

	m_adapter->processData("TIME|@ASSET@|111|Part|<Part>TEST 1</Part>");
	CPPUNIT_ASSERT_EQUAL((unsigned int) 1, m_agent->getAssetCount());


	m_adapter->processData("TIME|@ASSET@|112|Part|<Part>TEST 2</Part>");
	CPPUNIT_ASSERT_EQUAL((unsigned int) 2, m_agent->getAssetCount());

	m_adapter->processData("TIME|@ASSET@|113|Part|<Part>TEST 3</Part>");
	CPPUNIT_ASSERT_EQUAL((unsigned int) 3, m_agent->getAssetCount());

	m_path = "/current";
	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "113");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
	}

	m_adapter->processData("TIME|@REMOVE_ALL_ASSETS@|Part");
	CPPUNIT_ASSERT_EQUAL((unsigned int) 3, m_agent->getAssetCount());

	m_path = "/current";
	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved", "111");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:AssetRemoved@assetType", "Part");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:AssetChanged@assetType", "Part");
	}

	m_path = "/assets";
	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 0);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
	}

	// TODO: When asset is removed and the content is literal, it will
	// not regenerate the attributes for the asset.
	m_queries["removed"] = "true";
	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:Assets/*", 3);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "3");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[3]", "TEST 1");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[2]", "TEST 2");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Assets/*[1]", "TEST 3");
	}
}


void AgentTest::testPut()
{
	key_value_map queries;
	string body;
	m_agent->enablePut();

	queries["time"] = "TIME";
	queries["line"] = "205";
	queries["power"] = "ON";
	m_path = "/LinuxCNC";

	{
		PARSE_XML_RESPONSE_PUT(body, queries);
	}

	m_path = "/LinuxCNC/current";

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Line@timestamp", "TIME");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Line", "205");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:PowerState", "ON");
	}
}


// Test diabling of HTTP PUT or POST
void AgentTest::testPutBlocking()
{
	key_value_map queries;
	string body;

	queries["time"] = "TIME";
	queries["line"] = "205";
	queries["power"] = "ON";
	m_path = "/LinuxCNC";

	{
		PARSE_XML_RESPONSE_PUT(body, queries);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "Only the HTTP GET request is supported");
	}
}


// Test diabling of HTTP PUT or POST
void AgentTest::testPutBlockingFrom()
{
	key_value_map queries;
	string body;
	m_agent->enablePut();

	m_incomingIp = "127.0.0.1";
	m_agent->allowPutFrom("192.168.0.1");

	queries["time"] = "TIME";
	queries["line"] = "205";
	m_path = "/LinuxCNC";

	{
		PARSE_XML_RESPONSE_PUT(body, queries);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Error", "HTTP PUT is not allowed from 127.0.0.1");
	}

	m_path = "/LinuxCNC/current";

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Line", "UNAVAILABLE");
	}

	// Retry request after adding ip address
	m_path = "/LinuxCNC";
	m_agent->allowPutFrom("127.0.0.1");

	{
		PARSE_XML_RESPONSE_PUT(body, queries);
	}

	m_path = "/LinuxCNC/current";

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Line", "205");
	}
}


void AgentTest::killThread(void *aArg)
{
	AgentTest *test = (AgentTest*) aArg;
	this_thread::sleep_for(chrono::milliseconds(test->m_delay));
	test->m_out.setstate(ios::eofbit);
}


void AgentTest::addThread(void *aArg)
{
	AgentTest *test = (AgentTest*) aArg;
	this_thread::sleep_for(chrono::milliseconds(test->m_delay));
	test->m_adapter->processData("TIME|line|204");
	test->m_out.setstate(ios::eofbit);
}


void AgentTest::testStreamData()
{
	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);

	// Start a thread...
	key_value_map query;
	query["interval"] = "50";
	query["heartbeat"] = "200";
	query["from"] = int64ToString(m_agent->getSequence());
	m_path = "/LinuxCNC/sample";

	timestamper ts;

	// Heartbeat test. Heartbeat should be sent in 100ms. Give
	// 25ms range.
	{
		uint64 start = ts.get_timestamp();

		m_delay = 25;
		dlib::create_new_thread(killThread, this);
		PARSE_XML_RESPONSE_QUERY(query);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Streams", 0);

		int64 delta = ts.get_timestamp() - start;
		CPPUNIT_ASSERT(delta < 225000);
		CPPUNIT_ASSERT(delta > 200000);
	}

	m_out.clear();
	m_out.str("");

	// Set some data and make sure we get data within 20ms.
	// Again, allow for some slop.
	{
		uint64 start = ts.get_timestamp();

		m_delay = 10;
		dlib::create_new_thread(addThread, this);
		PARSE_XML_RESPONSE_QUERY(query);

		int64 delta = ts.get_timestamp() - start;
		CPPUNIT_ASSERT(delta < 70000);
		CPPUNIT_ASSERT(delta > 40000);
	}

}


void AgentTest::testFailWithDuplicateDeviceUUID()
{
	CPPUNIT_ASSERT_THROW(new Agent("../samples/dup_uuid.xml", 8, 4, 25), std::runtime_error);
}


void AgentTest::streamThread(void *aArg)
{
	AgentTest *test = (AgentTest*) aArg;
	this_thread::sleep_for(chrono::milliseconds(test->m_delay));
	test->m_agent->setSequence(test->m_agent->getSequence() + 20);
	test->m_adapter->processData("TIME|line|204");
	this_thread::sleep_for(120ms);
	test->m_out.setstate(ios::eofbit);
}


void AgentTest::testStreamDataObserver()
{
	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);

	// Start a thread...
	key_value_map query;
	query["interval"] = "100";
	query["heartbeat"] = "1000";
	query["count"] = "10";
	query["from"] = int64ToString(m_agent->getSequence());
	m_path = "/LinuxCNC/sample";

	// Test to make sure the signal will push the sequence number forward and capture
	// the new data.
	{
		m_delay = 50;
		string seq = int64ToString(m_agent->getSequence() + 20);
		dlib::create_new_thread(streamThread, this);
		PARSE_XML_RESPONSE_QUERY(query);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:Line@sequence", seq.c_str());
	}
}


void AgentTest::testRelativeTime()
{
	{
		m_path = "/sample";

		m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
		CPPUNIT_ASSERT(m_adapter);

		m_adapter->setRelativeTime(true);
		m_adapter->setBaseOffset(1000);
		m_adapter->setBaseTime(1353414802123456LL); // 2012-11-20 12:33:22.123456 UTC

		// Add a 10.654321 seconds
		m_adapter->processData("10654|line|204");

		{
			PARSE_XML_RESPONSE;
			CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
			CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]@timestamp", "2012-11-20T12:33:32.776456Z");
		}
	}
}


void AgentTest::testRelativeParsedTime()
{
	{
		m_path = "/sample";

		m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
		CPPUNIT_ASSERT(m_adapter);

		m_adapter->setRelativeTime(true);
		m_adapter->setParseTime(true);
		m_adapter->setBaseOffset(1354165286555666); // 2012-11-29 05:01:26.555666 UTC
		m_adapter->setBaseTime(1353414802123456); // 2012-11-20 12:33:22.123456 UTC

		// Add a 10.111000 seconds
		m_adapter->processData("2012-11-29T05:01:36.666666|line|100");

		{
			PARSE_XML_RESPONSE;
			CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
			CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]@timestamp", "2012-11-20T12:33:32.234456Z");
		}
	}
}


namespace CppUnit
{
	template<>
	struct assertion_traits<dlib::uint64>
	{
		static bool equal(const dlib::uint64 &x, const dlib::uint64 &y)
		{
			return x == y;
		}

		static std::string toString(const dlib::uint64 &x)
		{
			CppUnit::OStringStream ost;
			ost << x;
			return ost.str();
		}

	};
}


void AgentTest::testRelativeParsedTimeDetection()
{
	m_path = "/sample";

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);

	m_adapter->setRelativeTime(true);

	// Add a 10.111000 seconds
	m_adapter->processData("2012-11-29T05:01:26.555666|line|100");

	CPPUNIT_ASSERT(m_adapter->isParsingTime());
	CPPUNIT_ASSERT_EQUAL((uint64_t)1354165286555666LL, m_adapter->getBaseOffset());
}


void AgentTest::testRelativeOffsetDetection()
{
	m_path = "/sample";

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);

	m_adapter->setRelativeTime(true);

	// Add a 10.111000 seconds
	m_adapter->processData("1234556|line|100");

	CPPUNIT_ASSERT(!m_adapter->isParsingTime());
	CPPUNIT_ASSERT_EQUAL((uint64_t)1234556000LL, m_adapter->getBaseOffset());
}


void AgentTest::testDynamicCalibration()
{
	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);

	// Add a 10.111000 seconds
	m_adapter->protocolCommand("* calibration:Yact|.01|200.0|Zact|0.02|300|Xts|0.01|500");
	DataItem *di = m_agent->getDataItemByName("LinuxCNC", "Yact");
	CPPUNIT_ASSERT(di);

	CPPUNIT_ASSERT(di->hasFactor());
	CPPUNIT_ASSERT_EQUAL(0.01, di->getConversionFactor());
	CPPUNIT_ASSERT_EQUAL(200.0, di->getConversionOffset());

	di = m_agent->getDataItemByName("LinuxCNC", "Zact");
	CPPUNIT_ASSERT(di);

	CPPUNIT_ASSERT(di->hasFactor());
	CPPUNIT_ASSERT_EQUAL(0.02, di->getConversionFactor());
	CPPUNIT_ASSERT_EQUAL(300.0, di->getConversionOffset());

	m_adapter->processData("TIME|Yact|200|Zact|600");
	m_adapter->processData("TIME|Xts|25|| 5118 5118 5118 5118 5118 5118 5118 5118 5118 5118 5118 5118 5119 5119 5118 5118 5117 5117 5119 5119 5118 5118 5118 5118 5118");

	m_path = "/current";

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[@dataItemId='y1']", "4");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Position[@dataItemId='z1']", "18");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PositionTimeSeries[@dataItemId='x1ts']", "56.18 56.18 56.18 56.18 56.18 56.18 56.18 56.18 56.18 56.18 56.18 56.18 56.19 56.19 56.18 56.18 56.17 56.17 56.19 56.19 56.18 56.18 56.18 56.18 56.18");
	}
}


void AgentTest::testInitialTimeSeriesValues()
{
	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);

	m_path = "/current";

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PositionTimeSeries[@dataItemId='x1ts']", "UNAVAILABLE");
	}
}


void AgentTest::testFilterValues()
{
	delete m_agent; m_agent= nullptr;
	m_agent = new Agent("../samples/filter_example.xml", 8, 4, 25);

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);

	m_path = "/sample";

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
	}

	m_adapter->processData("TIME|load|100");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[2]", "100");
	}

	m_adapter->processData("TIME|load|103");
	m_adapter->processData("TIME|load|106");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[2]", "100");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[3]", "106");
	}

	m_adapter->processData("TIME|load|106|load|108|load|112");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[2]", "100");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[3]", "106");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Load[4]", "112");
	}

	DataItem *item = m_agent->getDataItemByName((string) "LinuxCNC", "pos");
	CPPUNIT_ASSERT(item);
	CPPUNIT_ASSERT(item->hasMinimumDelta());

	CPPUNIT_ASSERT(!item->isFiltered(0.0, NAN));
	CPPUNIT_ASSERT(item->isFiltered(5.0, NAN));
	CPPUNIT_ASSERT(!item->isFiltered(20.0, NAN));
}

void AgentTest::testResetTriggered()
{
	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);
  
	m_path = "/sample";
  
	m_adapter->processData("TIME1|pcount|0");
	m_adapter->processData("TIME2|pcount|1");
	m_adapter->processData("TIME3|pcount|2");
	m_adapter->processData("TIME4|pcount|0:DAY");
	m_adapter->processData("TIME3|pcount|5");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PartCount[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PartCount[2]", "0");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PartCount[3]", "1");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PartCount[3]@resetTriggered", nullptr);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PartCount[4]", "2");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PartCount[5]", "0");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PartCount[5]@resetTriggered", "DAY");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:PartCount[6]", "5");
	}
}


void AgentTest::testReferences()
{
	delete m_agent; m_agent = nullptr;
	m_agent = new Agent("../samples/reference_example.xml", 8, 4, 25);

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);

	string id = "mf";
	DataItem *item = m_agent->getDataItemByName((string) "LinuxCNC", id);
	Component *comp = item->getComponent();

	const list<Component::Reference> refs = comp->getReferences();
	const Component::Reference &ref = refs.front();

	CPPUNIT_ASSERT_EQUAL((string) "c4", ref.m_id);
	CPPUNIT_ASSERT_EQUAL((string) "chuck", ref.m_name);

	CPPUNIT_ASSERT_MESSAGE("DataItem was not resolved", ref.m_dataItem);

	const Component::Reference &ref2 = refs.back();
	CPPUNIT_ASSERT_EQUAL((string) "d2", ref2.m_id);
	CPPUNIT_ASSERT_EQUAL((string) "door", ref2.m_name);

	CPPUNIT_ASSERT_MESSAGE("DataItem was not resolved", ref2.m_dataItem);

	m_path = "/current";
	key_value_map query;
	query["path"] = "//BarFeederInterface";


	// Additional data items should be included
	{
		PARSE_XML_RESPONSE_QUERY(query);

		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='BarFeederInterface']//m:MaterialFeed", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='Door']//m:DoorState", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:ComponentStream[@component='Rotary']//m:ChuckState", "UNAVAILABLE");
	}
}


void AgentTest::testDiscrete()
{
	delete m_agent; m_agent= nullptr;
	m_agent = new Agent("../samples/discrete_example.xml", 8, 4, 25);
	m_path = "/sample";

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	m_adapter->setDupCheck(true);
	CPPUNIT_ASSERT(m_adapter);

	DataItem *msg = m_agent->getDataItemByName("LinuxCNC", "message");
	CPPUNIT_ASSERT(msg);
	CPPUNIT_ASSERT_EQUAL(true, msg->isDiscrete());

	// Validate we are dup checking.
	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
	}

	m_adapter->processData("TIME|line|204");
	m_adapter->processData("TIME|line|204");
	m_adapter->processData("TIME|line|205");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[3]", "205");

		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:MessageDiscrete[1]", "UNAVAILABLE");
	}


	m_adapter->processData("TIME|message|Hi|Hello");
	m_adapter->processData("TIME|message|Hi|Hello");
	m_adapter->processData("TIME|message|Hi|Hello");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:MessageDiscrete[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:MessageDiscrete[2]", "Hello");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:MessageDiscrete[3]", "Hello");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:MessageDiscrete[4]", "Hello");
	}

}


void AgentTest::testUpcaseValues()
{
	m_path = "/current";
	delete m_agent; m_agent = nullptr;
	m_agent = new Agent("../samples/discrete_example.xml", 8, 4, 25);
	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	m_adapter->setDupCheck(true);
	CPPUNIT_ASSERT(m_adapter);
	CPPUNIT_ASSERT(m_adapter->upcaseValue());

	m_adapter->processData("TIME|mode|Hello");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ControllerMode", "HELLO");
	}

	m_adapter->setUpcaseValue(false);
	m_adapter->processData("TIME|mode|Hello");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ControllerMode", "Hello");
	}
}


void AgentTest::testConditionSequence()
{
	m_path = "/current";

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	m_adapter->setDupCheck(true);
	CPPUNIT_ASSERT(m_adapter);

	DataItem *logic = m_agent->getDataItemByName("LinuxCNC", "lp");
	CPPUNIT_ASSERT(logic);

	// Validate we are dup checking.
	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Unavailable[@dataItemId='lp']", 1);
	}

	m_adapter->processData("TIME|lp|NORMAL||||XXX");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Normal", "XXX");
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/*", 1);
	}

	m_adapter->processData("TIME|lp|FAULT|2218|ALARM_B|HIGH|2218-1 ALARM_B UNUSABLE G-code  A side FFFFFFFF");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/*", 1);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault", "2218-1 ALARM_B UNUSABLE G-code  A side FFFFFFFF");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault@nativeCode", "2218");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault@nativeSeverity", "ALARM_B");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault@qualifier", "HIGH");
	}

	m_adapter->processData("TIME|lp|NORMAL||||");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/*", 1);
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Normal", 1);
	}

	m_adapter->processData("TIME|lp|FAULT|4200|ALARM_D||4200 ALARM_D Power on effective parameter set");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/*", 1);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault", "4200 ALARM_D Power on effective parameter set");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault@nativeCode", "4200");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault@nativeSeverity", "ALARM_D");
	}

	m_adapter->processData("TIME|lp|FAULT|2218|ALARM_B|HIGH|2218-1 ALARM_B UNUSABLE G-code  A side FFFFFFFF");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/*", 2);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault[1]", "4200 ALARM_D Power on effective parameter set");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault[2]", "2218-1 ALARM_B UNUSABLE G-code  A side FFFFFFFF");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault[2]@nativeCode", "2218");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault[2]@nativeSeverity", "ALARM_B");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault[2]@qualifier", "HIGH");
	}

	m_adapter->processData("TIME|lp|FAULT|4200|ALARM_D||4200 ALARM_D Power on effective parameter set");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/*", 2);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault[1]", "2218-1 ALARM_B UNUSABLE G-code  A side FFFFFFFF");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault[1]@nativeCode", "2218");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault[1]@nativeSeverity", "ALARM_B");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault[1]@qualifier", "HIGH");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault[2]", "4200 ALARM_D Power on effective parameter set");
	}

	m_adapter->processData("TIME|lp|NORMAL|2218|||");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/*", 1);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault[1]@nativeCode", "4200");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Fault[1]", "4200 ALARM_D Power on effective parameter set");
	}

	m_adapter->processData("TIME|lp|NORMAL||||");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/*", 1);
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:ComponentStream[@component='Controller']/m:Condition/m:Normal", 1);
	}
}


void AgentTest::testEmptyLastItemFromAdapter()
{
	m_path = "/current";

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	m_adapter->setDupCheck(true);
	CPPUNIT_ASSERT(m_adapter);

	DataItem *program = m_agent->getDataItemByName("LinuxCNC", "program");
	CPPUNIT_ASSERT(program);

	DataItem *tool_id = m_agent->getDataItemByName("LinuxCNC", "block");
	CPPUNIT_ASSERT(tool_id);

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Program", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block", "UNAVAILABLE");
	}


	m_adapter->processData("TIME|program|A|block|B");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Program", "A");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block", "B");
	}

	m_adapter->processData("TIME|program||block|B");


	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Program", "");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block", "B");
	}

	m_adapter->processData("TIME|program||block|");


	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Program", "");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block", "");
	}

	m_adapter->processData("TIME|program|A|block|B");
	m_adapter->processData("TIME|program|A|block|");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Program", "A");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block", "");
	}

	m_adapter->processData("TIME|program|A|block|B|line|C");
	m_adapter->processData("TIME|program|D|block||line|E");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Program", "D");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block", "");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line", "E");
	}


}


xmlDocPtr AgentTest::responseHelper(CPPUNIT_NS::SourceLine sourceLine,
									key_value_map &aQueries)
{
	incoming_things incoming("", "", 0, 0);
	outgoing_things outgoing;
	incoming.request_type = "GET";
	incoming.path = m_path;
	incoming.queries = aQueries;
	incoming.cookies = m_cookies;
	incoming.headers = m_incomingHeaders;

	outgoing.out = &m_out;

	m_result = m_agent->on_request(incoming, outgoing);

	if (m_result.empty())
	{
		m_result = m_out.str();
		size_t pos = m_result.rfind("\n--");
		if (pos != string::npos)
		{
			pos = m_result.find('<', pos);
			if (pos != string::npos)
				m_result.erase(0, pos);
		}
	}

	string message = (string) "No response to request" + m_path + " with: ";

	key_value_map::iterator iter;

	for (iter = aQueries.begin(); iter != aQueries.end(); ++iter)
		message += iter->first + "=" + iter->second + ",";

	CPPUNIT_NS::Asserter::failIf(outgoing.http_return != 200, message, sourceLine);

	return xmlParseMemory(m_result.c_str(), m_result.length());
}


xmlDocPtr AgentTest::putResponseHelper(CPPUNIT_NS::SourceLine sourceLine,
									   string body, key_value_map &aQueries)
{
	incoming_things incoming("", "", 0, 0);
	outgoing_things outgoing;
	incoming.request_type = "PUT";
	incoming.path = m_path;
	incoming.queries = aQueries;
	incoming.cookies = m_cookies;
	incoming.headers = m_incomingHeaders;
	incoming.body = body;
	incoming.foreign_ip = m_incomingIp;

	outgoing.out = &m_out;

	m_result = m_agent->on_request(incoming, outgoing);

	string message = (string) "No response to request" + m_path;

	CPPUNIT_NS::Asserter::failIf(outgoing.http_return != 200, message, sourceLine);

	return xmlParseMemory(m_result.c_str(), m_result.length());
}


void AgentTest::testBadDataItem()
{
	m_path = "/sample";

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	CPPUNIT_ASSERT(m_adapter);

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
	}

	m_adapter->processData("TIME|bad|ignore|dummy|1244|line|204");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
	}
}


void AgentTest::testConstantValue()
{
	m_path = "/sample";

	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);

	DataItem *di = m_agent->getDataItemByName("LinuxCNC", "block");
	CPPUNIT_ASSERT(di);
	di->addConstrainedValue("UNAVAILABLE");

	CPPUNIT_ASSERT(m_adapter);

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block[1]", "UNAVAILABLE");
	}

	m_adapter->processData("TIME|block|G01X00|Smode|INDEX|line|204");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Block[1]", "UNAVAILABLE");
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:Block", 1);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:RotaryMode[1]", "SPINDLE");
		CPPUNITTEST_ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream//m:RotaryMode", 1);
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Line[2]", "204");
	}
}


void AgentTest::testComposition()
{
	m_path = "/current";
  
	m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
	m_adapter->setDupCheck(true);
	CPPUNIT_ASSERT(m_adapter);
  
	DataItem *motor = m_agent->getDataItemByName("LinuxCNC", "zt1");
	CPPUNIT_ASSERT(motor);

	DataItem *amp = m_agent->getDataItemByName("LinuxCNC", "zt2");
	CPPUNIT_ASSERT(amp);
  
	m_adapter->processData("TIME|zt1|100|zt2|200");

	{
		PARSE_XML_RESPONSE;
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Temperature[@dataItemId='zt1']", "100");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Temperature[@dataItemId='zt2']", "200");
	
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Temperature[@dataItemId='zt1']@compositionId", "zmotor");
		CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:DeviceStream//m:Temperature[@dataItemId='zt2']@compositionId", "zamp");
	}
}
