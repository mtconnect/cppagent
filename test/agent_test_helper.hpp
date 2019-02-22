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

#pragma once

#include <map>
#include <string>
#include <iosfwd>
#include <chrono>

#include "dlib/md5.h"
#include "dlib/server.h"
#include "dlib/sliding_buffer.h"


#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "adapter.hpp"
#include "agent.hpp"
#include "test_globals.hpp"

class AgentTestHelper {
public:
	AgentTestHelper()
	: m_agent(nullptr), m_incomingIp("127.0.0.1")
	{
	}

	Agent *m_agent;
	std::ostringstream m_out;

	std::string m_incomingIp;

	std::string m_path;
	dlib::key_value_map m_queries;
	std::string m_result;
	dlib::key_value_map m_cookies;
	dlib::key_value_map_ci m_incomingHeaders;

	// Helper method to test expected string, given optional query, & run tests
	xmlDocPtr responseHelper(CPPUNIT_NS::SourceLine sourceLine, key_value_map &aQueries);
	xmlDocPtr putResponseHelper(CPPUNIT_NS::SourceLine sourceLine, std::string body,
								key_value_map &aQueries);

};

#define PARSE_XML_RESPONSE \
xmlDocPtr doc = m_agentTestHelper.responseHelper(CPPUNIT_SOURCELINE(), m_agentTestHelper.m_queries); \
CPPUNIT_ASSERT(doc)

#define PARSE_XML_RESPONSE_QUERY_KV(key, value) \
m_agentTestHelper.m_queries[key] = value; \
xmlDocPtr doc = m_agentTestHelper.responseHelper(CPPUNIT_SOURCELINE(), m_agentTestHelper.m_queries); \
m_agentTestHelper.m_queries.clear(); \
CPPUNIT_ASSERT(doc)

#define PARSE_XML_RESPONSE_QUERY(queries) \
xmlDocPtr doc = m_agentTestHelper.responseHelper(CPPUNIT_SOURCELINE(), queries); \
CPPUNIT_ASSERT(doc)

#define PARSE_XML_RESPONSE_PUT(body, queries) \
xmlDocPtr doc = m_agentTestHelper.putResponseHelper(CPPUNIT_SOURCELINE(), body, queries); \
CPPUNIT_ASSERT(doc)


