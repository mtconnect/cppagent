//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
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

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "component_event.hpp"
#include "data_item.hpp"
#include "device.hpp"
#include "json_printer.hpp"
#include "globals.hpp"
#include "checkpoint.hpp"

#include "test_globals.hpp"

namespace mtconnect {
  namespace test {
    class JsonPrinterProbeTest : public CppUnit::TestFixture
    {
      CPPUNIT_TEST_SUITE(JsonPrinterProbeTest);
      CPPUNIT_TEST(testPrintDevice);
      CPPUNIT_TEST_SUITE_END();
      
    public:
      void testPrintDevice();
      
      void setUp();
      void tearDown();

    protected:
      JsonPrinter *m_printer;
      std::vector<Device *> m_devices;
      
    };

    CPPUNIT_TEST_SUITE_REGISTRATION(JsonPrinterProbeTest);

    void JsonPrinterProbeTest::setUp()
    {
    }

    void JsonPrinterProbeTest::tearDown()
    {
    }
    
    void JsonPrinterProbeTest::testPrintDevice()
    {
    }
  }
}

      
