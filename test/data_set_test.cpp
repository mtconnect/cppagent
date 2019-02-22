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

#include <stdio.h>
#include "data_set_test.hpp"

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION(DataSetTest);

using namespace std;

void DataSetTest::setUp()
{
	// Create an agent with only 16 slots and 8 data items.
	m_checkpoint = nullptr;
	m_agent = make_unique<Agent>("../samples/data_set.xml", 4, 4);
	m_agentId = int64ToString(getCurrentTimeInSec());
	m_adapter = nullptr;
	m_checkpoint = make_unique<Checkpoint>();
	m_agentTestHelper.m_agent = m_agent.get();

	std::map<string, string> attributes;
	auto device = m_agent->getDeviceByName("LinuxCNC");
	m_dataItem1 = device->getDeviceDataItem("v1");
}


void DataSetTest::tearDown()
{
	m_agent.reset();
	m_checkpoint.reset();
	if(m_dataItem1)
	{
		delete m_dataItem1;
		m_dataItem1 = nullptr;
	}
}

void DataSetTest::testDataItem()
{
	CPPUNIT_ASSERT(m_dataItem1->isDataSet());
	auto &attrs = m_dataItem1->getAttributes();

	CPPUNIT_ASSERT_EQUAL((string) "DATA_SET", attrs.at("representation"));
	CPPUNIT_ASSERT_EQUAL((string) "VariableDataSet", m_dataItem1->getElementName());
}

void DataSetTest::testInitialSet()
{
  string value("a=1 b=2 c=3 d=4");
  auto ce = new ComponentEvent(*m_dataItem1, 2, "time", value);
  
  CPPUNIT_ASSERT_EQUAL((size_t) 4, ce->getDataSet().size());
  auto &al = ce->getAttributes();
  std::map<string, string> attrs;
  
  for (const auto &attr : al)
    attrs[attr.first] = attr.second;

  CPPUNIT_ASSERT_EQUAL((string) "4", attrs.at("sampleCount"));
  
  auto map1 = ce->getDataSet();
  CPPUNIT_ASSERT_EQUAL((string) "1", map1.at("a"));
  CPPUNIT_ASSERT_EQUAL((string) "2", map1.at("b"));
  CPPUNIT_ASSERT_EQUAL((string) "3", map1.at("c"));
  CPPUNIT_ASSERT_EQUAL((string) "4", map1.at("d"));

  m_checkpoint->addComponentEvent(ce);
  auto c2 = *m_checkpoint->getEventPtr("v1");
  auto al2 = c2->getAttributes();
  
  attrs.clear();
  for (const auto &attr : al2)
    attrs[attr.first] = attr.second;

  CPPUNIT_ASSERT_EQUAL((string) "4", attrs.at("sampleCount"));
  
  auto map2 = c2->getDataSet();
  CPPUNIT_ASSERT_EQUAL((string) "1", map2.at("a"));
  CPPUNIT_ASSERT_EQUAL((string) "2", map2.at("b"));
  CPPUNIT_ASSERT_EQUAL((string) "3", map2.at("c"));
  CPPUNIT_ASSERT_EQUAL((string) "4", map2.at("d"));
  
  ce->unrefer();
}

void DataSetTest::testUpdateOneElement()
{
  string value("a=1 b=2 c=3 d=4");
  ComponentEventPtr ce(new ComponentEvent(*m_dataItem1, 2, "time", value));
  m_checkpoint->addComponentEvent(ce);

  auto cecp = *m_checkpoint->getEventPtr("v1");
  CPPUNIT_ASSERT_EQUAL((size_t) 4, cecp->getDataSet().size());

  string value2("c=5");
  ComponentEventPtr ce2(new ComponentEvent(*m_dataItem1, 2, "time", value2));
  m_checkpoint->addComponentEvent(ce2);

  auto ce3 = *m_checkpoint->getEventPtr("v1");
  CPPUNIT_ASSERT_EQUAL((size_t) 4, ce3->getDataSet().size());

  auto map1 = ce3->getDataSet();
  CPPUNIT_ASSERT_EQUAL((string) "1", map1.at("a"));
  CPPUNIT_ASSERT_EQUAL((string) "2", map1.at("b"));
  CPPUNIT_ASSERT_EQUAL((string) "5", map1.at("c"));
  CPPUNIT_ASSERT_EQUAL((string) "4", map1.at("d"));
  
  string value3("e=6");
  ComponentEventPtr ce4(new ComponentEvent(*m_dataItem1, 2, "time", value3));
  m_checkpoint->addComponentEvent(ce4);
  
  auto ce5 = *m_checkpoint->getEventPtr("v1");
  CPPUNIT_ASSERT_EQUAL((size_t) 5, ce5->getDataSet().size());
  
  auto map2 = ce5->getDataSet();
  CPPUNIT_ASSERT_EQUAL((string) "1", map2.at("a"));
  CPPUNIT_ASSERT_EQUAL((string) "2", map2.at("b"));
  CPPUNIT_ASSERT_EQUAL((string) "5", map2.at("c"));
  CPPUNIT_ASSERT_EQUAL((string) "4", map2.at("d"));
  CPPUNIT_ASSERT_EQUAL((string) "6", map2.at("e"));
}

void DataSetTest::testUpdateMany()
{
  string value("a=1 b=2 c=3 d=4");
  ComponentEventPtr ce(new ComponentEvent(*m_dataItem1, 2, "time", value));
  m_checkpoint->addComponentEvent(ce);
  
  auto cecp = *m_checkpoint->getEventPtr("v1");
  CPPUNIT_ASSERT_EQUAL((size_t) 4, cecp->getDataSet().size());
  
  string value2("c=5 e=6");
  ComponentEventPtr ce2(new ComponentEvent(*m_dataItem1, 2, "time", value2));
  m_checkpoint->addComponentEvent(ce2);
  
  auto ce3 = *m_checkpoint->getEventPtr("v1");
  CPPUNIT_ASSERT_EQUAL((size_t) 5, ce3->getDataSet().size());
  
  auto map1 = ce3->getDataSet();
  CPPUNIT_ASSERT_EQUAL((string) "1", map1.at("a"));
  CPPUNIT_ASSERT_EQUAL((string) "2", map1.at("b"));
  CPPUNIT_ASSERT_EQUAL((string) "5", map1.at("c"));
  CPPUNIT_ASSERT_EQUAL((string) "4", map1.at("d"));
  CPPUNIT_ASSERT_EQUAL((string) "6", map1.at("e"));

  string value3("e=7 a=8 f=9");
  ComponentEventPtr ce4(new ComponentEvent(*m_dataItem1, 2, "time", value3));
  m_checkpoint->addComponentEvent(ce4);
  
  auto ce5 = *m_checkpoint->getEventPtr("v1");
  CPPUNIT_ASSERT_EQUAL((size_t) 6, ce5->getDataSet().size());
  
  auto map2 = ce5->getDataSet();
  CPPUNIT_ASSERT_EQUAL((string) "8", map2.at("a"));
  CPPUNIT_ASSERT_EQUAL((string) "2", map2.at("b"));
  CPPUNIT_ASSERT_EQUAL((string) "5", map2.at("c"));
  CPPUNIT_ASSERT_EQUAL((string) "4", map2.at("d"));
  CPPUNIT_ASSERT_EQUAL((string) "7", map2.at("e"));
  CPPUNIT_ASSERT_EQUAL((string) "9", map2.at("f"));
}

void DataSetTest::testReset()
{
  string value("a=1 b=2 c=3 d=4");
  ComponentEventPtr ce(new ComponentEvent(*m_dataItem1, 2, "time", value));
  m_checkpoint->addComponentEvent(ce);
  
  auto cecp = *m_checkpoint->getEventPtr("v1");
  CPPUNIT_ASSERT_EQUAL((size_t) 4, cecp->getDataSet().size());
  
  string value2("RESET|c=5 e=6");
  ComponentEventPtr ce2(new ComponentEvent(*m_dataItem1, 2, "time", value2));
  m_checkpoint->addComponentEvent(ce2);
  
  auto ce3 = *m_checkpoint->getEventPtr("v1");
  CPPUNIT_ASSERT_EQUAL((size_t) 2, ce3->getDataSet().size());
  
  auto map1 = ce3->getDataSet();
  CPPUNIT_ASSERT_EQUAL((string) "5", map1.at("c"));
  CPPUNIT_ASSERT_EQUAL((string) "6", map1.at("e"));
  
  string value3("x=pop y=hop");
  ComponentEventPtr ce4(new ComponentEvent(*m_dataItem1, 2, "time", value3));
  m_checkpoint->addComponentEvent(ce4);
  
  auto ce5 = *m_checkpoint->getEventPtr("v1");
  CPPUNIT_ASSERT_EQUAL((size_t) 4, ce5->getDataSet().size());
  
  auto map2 = ce5->getDataSet();
  CPPUNIT_ASSERT_EQUAL((string) "pop", map2.at("x"));
  CPPUNIT_ASSERT_EQUAL((string) "hop", map2.at("y"));
}

void DataSetTest::testBadData()
{
  string value("12356");
  auto ce = new ComponentEvent(*m_dataItem1, 2, "time", value);
  
  CPPUNIT_ASSERT_EQUAL((size_t) 1, ce->getDataSet().size());
  
  string value1("  a=2      b3=xxx");
  auto ce2 = new ComponentEvent(*m_dataItem1, 2, "time", value1);
  
  CPPUNIT_ASSERT_EQUAL((size_t) 2, ce2->getDataSet().size());
  
  auto map1 = ce2->getDataSet();
  CPPUNIT_ASSERT_EQUAL((string) "2", map1.at("a"));
  CPPUNIT_ASSERT_EQUAL((string) "xxx", map1.at("b3"));
}

void DataSetTest::testCurrent()
{
  m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
  CPPUNIT_ASSERT(m_adapter);
  
  m_agentTestHelper.m_path = "/current";
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                      "//m:DeviceStream//m:VariableDataSet[@dataItemId='v1']",
                                      "UNAVAILABLE");
  }
  
  m_adapter->processData("TIME|vars|a=1 b=2 c=3");

  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                      "//m:DeviceStream//m:VariableDataSet[@dataItemId='v1']",
                                      "a=1 b=2 c=3");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,"//m:DeviceStream//m:VariableDataSet[@dataItemId='v1']@sampleCount", "3");
}

  m_adapter->processData("TIME|vars|c=6");
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                      "//m:DeviceStream//m:VariableDataSet[@dataItemId='v1']",
                                      "a=1 b=2 c=6");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,"//m:DeviceStream//m:VariableDataSet[@dataItemId='v1']@sampleCount", "3");
  }

  m_adapter->processData("TIME|vars|RESET|d=10");
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                      "//m:DeviceStream//m:VariableDataSet[@dataItemId='v1']",
                                      "d=10");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,"//m:DeviceStream//m:VariableDataSet[@dataItemId='v1']@sampleCount", "1");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                      "//m:DeviceStream//m:VariableDataSet[@dataItemId='v1']@resetTriggered",
                                      "RESET");

  }
  
  m_adapter->processData("TIME|vars|c=6");
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,
                                      "//m:DeviceStream//m:VariableDataSet[@dataItemId='v1']",
                                      "c=6 d=10");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc,"//m:DeviceStream//m:VariableDataSet[@dataItemId='v1']@sampleCount", "2");
  }
}

void DataSetTest::testSample()
{
  m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
  CPPUNIT_ASSERT(m_adapter);
  
  m_adapter->processData("TIME|vars|a=1 b=2 c=3");
  m_adapter->processData("TIME|vars|c=5");
  m_adapter->processData("TIME|vars|c=8");

  m_agentTestHelper.m_path = "/sample";
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[2]", "a=1 b=2 c=3");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[2]@sampleCount", "3");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[3]", "c=5");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[3]@sampleCount", "1");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[4]", "c=8");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[4]@sampleCount", "1");
  }

  m_agentTestHelper.m_path = "/current";
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "a=1 b=2 c=8");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@sampleCount", "3");
  }
  
  m_agentTestHelper.m_path = "/sample";
  m_adapter->processData("TIME|vars|c b=5");

  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[2]", "a=1 b=2 c=3");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[2]@sampleCount", "3");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[3]", "c=5");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[3]@sampleCount", "1");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[4]", "c=8");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[4]@sampleCount", "1");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[5]", "b=5 c=");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[5]@sampleCount", "2");
  }

  m_agentTestHelper.m_path = "/current";
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "a=1 b=5");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@sampleCount", "2");
  }
  
}

void DataSetTest::testCurrentAt()
{
  m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
  CPPUNIT_ASSERT(m_adapter);
  
  auto seq = m_agent->getSequence();
  
  m_adapter->processData("TIME|vars|a=1 b=2 c=3");
  m_adapter->processData("TIME|vars| c=5 ");
  m_adapter->processData("TIME|vars|c=8");
  m_adapter->processData("TIME|vars|b=10   a=xxx");
  m_adapter->processData("TIME|vars|RESET|q=hello_there");
  m_adapter->processData("TIME|vars|r=good_bye");

  m_agentTestHelper.m_path = "/current";

  {
    PARSE_XML_RESPONSE_QUERY_KV("at", int64ToString(seq - 1));
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "UNAVAILABLE");
  }

  {
    PARSE_XML_RESPONSE_QUERY_KV("at", int64ToString(seq));
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "a=1 b=2 c=3");
  }
  
  {
    PARSE_XML_RESPONSE_QUERY_KV("at", int64ToString(seq + 1));
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "a=1 b=2 c=5");
  }

  {
    PARSE_XML_RESPONSE_QUERY_KV("at", int64ToString(seq + 2));
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "a=1 b=2 c=8");
  }

  {
    PARSE_XML_RESPONSE_QUERY_KV("at", int64ToString(seq + 3));
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "a=xxx b=10 c=8");
  }

  {
    PARSE_XML_RESPONSE_QUERY_KV("at", int64ToString(seq + 4));
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "q=hello_there");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "///m:VariableDataSet@resetTriggered",
                                      "RESET");
  }
  
  {
    PARSE_XML_RESPONSE_QUERY_KV("at", int64ToString(seq + 5));
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "q=hello_there r=good_bye");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@resetTriggered", 0);
  }
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "q=hello_there r=good_bye");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@resetTriggered", 0);
  }
}

void DataSetTest::testDeleteKey()
{
  string value("a=1 b=2 c=3 d=4");
  ComponentEventPtr ce(new ComponentEvent(*m_dataItem1, 2, "time", value));
  m_checkpoint->addComponentEvent(ce);
  
  auto cecp = *m_checkpoint->getEventPtr("v1");
  CPPUNIT_ASSERT_EQUAL((size_t) 4, cecp->getDataSet().size());
  
  string value2("c e=6 a=");
  ComponentEventPtr ce2(new ComponentEvent(*m_dataItem1, 4, "time", value2));
  m_checkpoint->addComponentEvent(ce2);
  
  auto ce3 = *m_checkpoint->getEventPtr("v1");
  CPPUNIT_ASSERT_EQUAL((size_t) 3, ce3->getDataSet().size());
  
  auto map1 = ce3->getDataSet();
  CPPUNIT_ASSERT_EQUAL((string) "2", map1.at("b"));
  CPPUNIT_ASSERT_EQUAL((string) "4", map1.at("d"));
  CPPUNIT_ASSERT_EQUAL((string) "6", map1.at("e"));
  CPPUNIT_ASSERT(map1.find("c") == map1.end());
  CPPUNIT_ASSERT(map1.find("a") == map1.end());
}

void DataSetTest::testResetWithNoItems()
{
  m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
  CPPUNIT_ASSERT(m_adapter);
  
  m_adapter->processData("TIME|vars|a=1 b=2 c=3");
  m_adapter->processData("TIME|vars| c=5 ");
  m_adapter->processData("TIME|vars|c=8");
  m_adapter->processData("TIME|vars|b=10   a=xxx");
  m_adapter->processData("TIME|vars|RESET|");
  m_adapter->processData("TIME|vars|r=good_bye");
  
  m_agentTestHelper.m_path = "/sample";
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[2]", "a=1 b=2 c=3");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[2]@sampleCount", "3");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[3]", "c=5");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[3]@sampleCount", "1");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[4]", "c=8");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[4]@sampleCount", "1");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[5]", "a=xxx b=10");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[5]@sampleCount", "2");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[6]", "");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[6]@sampleCount", "0");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[7]", "r=good_bye");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[7]@sampleCount", "1");
  }

}

void DataSetTest::testDuplicateCompression()
{
  m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
  CPPUNIT_ASSERT(m_adapter);
  
  m_adapter->processData("TIME|vars|a=1 b=2 c=3");
  m_adapter->processData("TIME|vars|b=2");
  m_adapter->processData("TIME|vars|b=2 d=4");
  m_adapter->processData("TIME|vars|b=2 d=4 c=3");
  m_adapter->processData("TIME|vars|b=2 d=4 c=3");
  m_adapter->processData("TIME|vars|b=3 e=4");

  m_agentTestHelper.m_path = "/sample";

  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "UNAVAILABLE");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[2]", "a=1 b=2 c=3");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[2]@sampleCount", "3");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[3]", "d=4");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[3]@sampleCount", "1");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[4]", "b=3 e=4");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[4]@sampleCount", "2");
  }
  
  m_agentTestHelper.m_path = "/current";

  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "a=1 b=3 c=3 d=4 e=4");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@sampleCount", "5");
  }

  m_adapter->processData("TIME|vars|RESET|a=1 b=3 c=3 d=4 e=4");
  
  m_agentTestHelper.m_path = "/sample";

  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[5]", "a=1 b=3 c=3 d=4 e=4");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[5]@sampleCount", "5");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[5]@resetTriggered", "RESET");

  }
  
  m_agentTestHelper.m_path = "/current";
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "a=1 b=3 c=3 d=4 e=4");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@sampleCount", "5");
  }
}

void DataSetTest::testQuoteDelimeter()
{
  m_adapter = m_agent->addAdapter("LinuxCNC", "server", 7878, false);
  CPPUNIT_ASSERT(m_adapter);
  
  m_adapter->processData("TIME|vars|a='1 2 3' b=\"x y z\" c={cats and dogs}");

  m_agentTestHelper.m_path = "/current";
  
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "a='1 2 3' b=\"x y z\" c={cats and dogs}");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@sampleCount", "3");
  }
  
  m_adapter->processData("TIME|vars|b='u v w' c={chickens and horses");
  {
    PARSE_XML_RESPONSE;
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]", "a='1 2 3' b='u v w' c={cats and dogs}");
    CPPUNITTEST_ASSERT_XML_PATH_EQUAL(doc, "//m:VariableDataSet[1]@sampleCount", "3");
  }
}
