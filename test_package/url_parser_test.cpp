//
// Copyright Copyright 2009-2022, AMT – The Association For Manufacturing Technology (“AMT”)
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

// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "mtconnect/source/adapter/agent_adapter/url_parser.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::source::adapter;
using namespace std::literals;

using namespace mtconnect::source::adapter::agent_adapter;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

TEST(UrlParserTest, should_parse_url_with_port)
{
  Url url = Url::parse("http://127.0.0.1:5000/Device");

  EXPECT_EQ("http", url.m_protocol);
  EXPECT_TRUE(holds_alternative<boost::asio::ip::address>(url.m_host));
  EXPECT_TRUE(url.m_port);
  EXPECT_EQ(5000, url.m_port);
  EXPECT_EQ("/Device", url.m_path);
  EXPECT_EQ(0, url.m_query.size());

  EXPECT_FALSE(url.m_username);
  EXPECT_FALSE(url.m_password);

  auto host = get<boost::asio::ip::address>(url.m_host);
  EXPECT_TRUE(host.is_v4());
  auto ipv4 = host.to_v4();
  EXPECT_EQ(0x7F000001, ipv4.to_uint());
}

TEST(UrlParserTest, should_parse_url_without_port)
{
  Url url = Url::parse("http://127.0.0.1/Device");

  EXPECT_EQ("http", url.m_protocol);
  EXPECT_TRUE(holds_alternative<boost::asio::ip::address>(url.m_host));
  EXPECT_FALSE(url.m_port);
  EXPECT_EQ("/Device", url.m_path);
  EXPECT_EQ(0, url.m_query.size());

  EXPECT_FALSE(url.m_username);
  EXPECT_FALSE(url.m_password);

  auto host = get<boost::asio::ip::address>(url.m_host);
  EXPECT_TRUE(host.is_v4());
  auto ipv4 = host.to_v4();
  EXPECT_EQ(0x7F000001, ipv4.to_uint());
}

TEST(UrlParserTest, should_parse_url_with_device_name)
{
  Url url = Url::parse("http://dev.example.com/Device");

  EXPECT_EQ("http", url.m_protocol);
  EXPECT_TRUE(holds_alternative<string>(url.m_host));
  EXPECT_FALSE(url.m_port);
  EXPECT_EQ("/Device", url.m_path);
  EXPECT_EQ(0, url.m_query.size());

  EXPECT_FALSE(url.m_username);
  EXPECT_FALSE(url.m_password);

  EXPECT_EQ("dev.example.com", get<string>(url.m_host));
}

TEST(UrlParserTest, should_parse_url_with_device_name_and_port)
{
  Url url = Url::parse("http://dev.example.com:5000/Device");

  EXPECT_EQ("http", url.m_protocol);
  EXPECT_TRUE(holds_alternative<string>(url.m_host));
  EXPECT_TRUE(url.m_port);
  EXPECT_EQ(5000, url.m_port);
  EXPECT_EQ("/Device", url.m_path);
  EXPECT_EQ(0, url.m_query.size());

  EXPECT_FALSE(url.m_username);
  EXPECT_FALSE(url.m_password);

  EXPECT_EQ("dev.example.com", get<string>(url.m_host));
}

TEST(UrlParserTest, should_parse_url_with_no_path)
{
  Url url = Url::parse("http://dev.example.com:5000");

  EXPECT_EQ("http", url.m_protocol);
  EXPECT_TRUE(holds_alternative<string>(url.m_host));
  EXPECT_TRUE(url.m_port);
  EXPECT_EQ(5000, url.m_port);
  EXPECT_EQ("/", url.m_path);
  EXPECT_EQ(0, url.m_query.size());

  EXPECT_FALSE(url.m_username);
  EXPECT_FALSE(url.m_password);

  EXPECT_EQ("dev.example.com", get<string>(url.m_host));
}

TEST(UrlParserTest, should_parse_url_with_query)
{
  Url url = Url::parse("http://dev.example.com:5000/Device?one=1&two=2");

  EXPECT_EQ("http", url.m_protocol);
  EXPECT_TRUE(holds_alternative<string>(url.m_host));
  EXPECT_TRUE(url.m_port);
  EXPECT_EQ(5000, url.m_port);
  EXPECT_EQ("/Device", url.m_path);

  EXPECT_FALSE(url.m_username);
  EXPECT_FALSE(url.m_password);

  EXPECT_EQ("dev.example.com", get<string>(url.m_host));

  EXPECT_EQ(2, url.m_query.size());
  EXPECT_EQ("1"s, url.m_query["one"s]);
  EXPECT_EQ("2"s, url.m_query["two"s]);
}

TEST(UrlParserTest, should_get_query_string)
{
  Url url = Url::parse("http://dev.example.com:5000/Device?one=1&two=2");

  EXPECT_EQ("one=1&two=2", url.m_query.join());
}

TEST(UrlParserTest, should_get_ip_addr_as_string)
{
  Url url = Url::parse("http://127.0.0.1:5000/Device");
  EXPECT_EQ("127.0.0.1", url.getHost());
}

TEST(UrlParserTest, should_get_host_name_as_string)
{
  Url url = Url::parse("http://dev.example.com:5000/Device");
  EXPECT_EQ("dev.example.com", url.getHost());
}

TEST(UrlParserTest, should_get_target_without_query)
{
  Url url = Url::parse("http://dev.example.com:5000/Device");

  EXPECT_EQ("/Device", url.getTarget());
}

TEST(UrlParserTest, should_get_target_with_query)
{
  Url url = Url::parse("http://dev.example.com:5000/Device?one=1&two=2");

  EXPECT_EQ("/Device?one=1&two=2", url.getTarget());
}
