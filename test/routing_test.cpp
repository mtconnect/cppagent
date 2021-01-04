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
  Routing probe("GET", "/probe");
  EXPECT_EQ(0, probe.getPathParameters().size());
  EXPECT_EQ(0, probe.getQueryParameters().size());
  ASSERT_TRUE(probe.matches("GET", "/probe"));
  ASSERT_TRUE(probe.matches("GET", "/probe"));
  ASSERT_FALSE(probe.matches("PUT", "/probe"));

  Routing probeWithDevice("GET", "/{device}/probe");
  ASSERT_EQ(1, probeWithDevice.getPathParameters().size());
  EXPECT_EQ(0, probe.getQueryParameters().size());
  EXPECT_EQ("device", probeWithDevice.getPathParameters().front().m_name);
  
  auto m2 = probeWithDevice.matches("GET", "/ABC123/probe");
  ASSERT_TRUE(m2);
  ASSERT_EQ("ABC123", get<string>((*m2)["device"]));

  auto m3 = probeWithDevice.matches("GET", "/ABC123/probe");
  ASSERT_TRUE(m3);
  ASSERT_EQ("ABC123", get<string>((*m2)["device"]));
  
  ASSERT_FALSE(probe.matches("GET", "/ABC123/probe"));
}

TEST_F(RoutingTest, TestComplexPatterns)
{
  Routing r("GET", "/asset/{assets}");
  ASSERT_EQ(1, r.getPathParameters().size());
  EXPECT_EQ("assets", r.getPathParameters().front().m_name);

  auto m1 = r.matches("GET", "/asset/A1,A2,A3");
  ASSERT_TRUE(m1);
  ASSERT_EQ("A1,A2,A3", get<string>((*m1)["assets"]));
  ASSERT_FALSE(r.matches("GET", "ABC123/probe"));
}

TEST_F(RoutingTest, TestCurrentAtQueryParameter)
{
  Routing r("GET", "/{device}/current?at={unsigned_integer}");
  ASSERT_EQ(1, r.getPathParameters().size());
  ASSERT_EQ(1, r.getQueryParameters().size());
  
  auto pp = r.getPathParameters().front();
  EXPECT_EQ("device", pp.m_name);
  EXPECT_EQ(Routing::PATH, pp.m_part);

  auto &qp = *r.getQueryParameters().begin();
  EXPECT_EQ("at", qp.m_name);
  EXPECT_EQ(Routing::UNSIGNED_INTEGER, qp.m_type);
  EXPECT_EQ(Routing::QUERY, qp.m_part);
  EXPECT_TRUE(holds_alternative<monostate>(qp.m_default));
}

TEST_F(RoutingTest, TestSampleQueryParameters)
{
  Routing r("GET", "/{device}/sample?from={unsigned_integer}&"
            "interval={double}&count={integer:100}&"
            "heartbeat={double:10000}");
  ASSERT_EQ(1, r.getPathParameters().size());
  ASSERT_EQ(4, r.getQueryParameters().size());
  
  auto pp = r.getPathParameters().front();
  EXPECT_EQ("device", pp.m_name);
  EXPECT_EQ(Routing::PATH, pp.m_part);

  auto qp = r.getQueryParameters().begin();
  EXPECT_EQ("count", qp->m_name);
  EXPECT_EQ(Routing::INTEGER, qp->m_type);
  EXPECT_EQ(Routing::QUERY, qp->m_part);
  EXPECT_TRUE(holds_alternative<int32_t>(qp->m_default));
  EXPECT_EQ(100, get<int32_t>(qp->m_default));
  qp++;

  EXPECT_EQ("from", qp->m_name);
  EXPECT_EQ(Routing::UNSIGNED_INTEGER, qp->m_type);
  EXPECT_EQ(Routing::QUERY, qp->m_part);
  EXPECT_TRUE(holds_alternative<monostate>(qp->m_default));
  qp++;
  
  EXPECT_EQ("heartbeat", qp->m_name);
  EXPECT_EQ(Routing::DOUBLE, qp->m_type);
  EXPECT_EQ(Routing::QUERY, qp->m_part);
  EXPECT_TRUE(holds_alternative<double>(qp->m_default));
  EXPECT_EQ(10000.0, get<double>(qp->m_default));
  qp++;

  EXPECT_EQ("interval", qp->m_name);
  EXPECT_EQ(Routing::DOUBLE, qp->m_type);
  EXPECT_EQ(Routing::QUERY, qp->m_part);
  EXPECT_TRUE(holds_alternative<monostate>(qp->m_default));
}

TEST_F(RoutingTest, TestQueryParameterMatch)
{
  Routing r("GET", "/{device}/sample?from={unsigned_integer}&"
            "interval={double}&count={integer:100}&"
            "heartbeat={double:10000}");
  ASSERT_EQ(1, r.getPathParameters().size());
  ASSERT_EQ(4, r.getQueryParameters().size());

  auto m1 = r.matches("GET", "/ABC123/sample");
  ASSERT_TRUE(m1);
  ASSERT_EQ("ABC123", get<string>((*m1)["device"]));
  ASSERT_EQ(100, get<int32_t>((*m1)["count"]));
  ASSERT_EQ(10000.0, get<double>((*m1)["heartbeat"]));

  auto m2 = r.matches("GET", "/ABC123/sample",
                      {{ "count", "1000"}, {"from", "12345"}});
  ASSERT_TRUE(m2);
  ASSERT_EQ("ABC123", get<string>((*m2)["device"]));
  ASSERT_EQ(1000, get<int32_t>((*m2)["count"]));
  ASSERT_EQ(12345, get<uint64_t>((*m2)["from"]));
  ASSERT_EQ(10000.0, get<double>((*m2)["heartbeat"]));

  auto m3 = r.matches("GET", "/ABC123/sample",
                      {{ "count", "1000"}, {"from", "12345"}, { "dummy" , "1" }
  });
  ASSERT_TRUE(m3);
  ASSERT_EQ("ABC123", get<string>((*m3)["device"]));
  ASSERT_EQ(1000, get<int32_t>((*m3)["count"]));
  ASSERT_EQ(12345, get<uint64_t>((*m3)["from"]));
  ASSERT_EQ(10000.0, get<double>((*m3)["heartbeat"]));
  ASSERT_EQ(m3->end(), m3->find("dummy"));
}

TEST_F(RoutingTest, TestQueryParameterError)
{
  Routing r("GET", "/{device}/sample?from={unsigned_integer}&"
            "interval={double}&count={integer:100}&"
            "heartbeat={double:10000}");
  auto m1 = r.matches("GET", "/ABC123/sample",
                      {{ "count", "xxx"} });
  ASSERT_FALSE(m1);
}
