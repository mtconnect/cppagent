// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "http_server/routing.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using namespace std;
using namespace mtconnect;
using namespace mtconnect::http_server;

class RoutingTest : public testing::Test
{
 protected:
  void SetUp() override
  {  // Create an agent with only 16 slots and 8 data items.
  }

  void TearDown() override
  {
  }

};

TEST_F(RoutingTest, TestSimplePattern)
{
  Routing probe("GET", "probe");
  EXPECT_EQ(0, probe.getParameters().size());
  ASSERT_TRUE(probe.matches("GET", "probe"));
  ASSERT_TRUE(probe.matches("GET", "/probe"));
  ASSERT_FALSE(probe.matches("PUT", "/probe"));

  Routing probeWithDevice("GET", "{device}/probe");
  ASSERT_EQ(1, probeWithDevice.getParameters().size());
  EXPECT_EQ("device", probeWithDevice.getParameters().front());
  
  auto m2 = probeWithDevice.matches("GET", "ABC123/probe");
  ASSERT_TRUE(m2);
  ASSERT_EQ("ABC123", (*m2)["device"]);

  auto m3 = probeWithDevice.matches("GET", "/ABC123/probe");
  ASSERT_TRUE(m3);
  ASSERT_EQ("ABC123", (*m2)["device"]);
  
  ASSERT_FALSE(probe.matches("GET", "ABC123/probe"));
}

TEST_F(RoutingTest, TestComplexPatterns)
{
  Routing r("GET", "asset/{assets}");
  ASSERT_EQ(1, r.getParameters().size());
  EXPECT_EQ("assets", r.getParameters().front());

  auto m1 = r.matches("GET", "asset/A1,A2,A3");
  ASSERT_TRUE(m1);
  ASSERT_EQ("A1,A2,A3", (*m1)["assets"]);
  ASSERT_FALSE(r.matches("GET", "ABC123/probe"));
}

TEST_F(RoutingTest, TestCallback)
{
  bool called { false };
  Routing r("GET", "{device}/probe",
            [&](const Routing::ParameterMap &p){
    called = true;
    auto it = p.find("device");
    ASSERT_NE(it, p.end());
    ASSERT_EQ("ABC123", it->second);
  });
  ASSERT_EQ(1, r.getParameters().size());
  EXPECT_EQ("device", r.getParameters().front());
  
  ASSERT_FALSE(called);
  auto m2 = r.matches("GET", "ABC123/probe");
  ASSERT_TRUE(called);
}
