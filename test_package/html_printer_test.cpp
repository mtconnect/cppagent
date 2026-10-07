//
// Copyright 2009-2026, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "agent_test_helper.hpp"
#include "mtconnect/agent.hpp"
#include "mtconnect/printer/html_printer.hpp"
#include "mtconnect/printer/xml_printer.hpp"
#include "test_utilities.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::sink::rest_sink;

using status = boost::beast::http::status;

int main(int argc, char* argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

namespace {
  const string BrowserViewFile {PROJECT_ROOT_DIR "/styles/viewer.html"};
  const string Placeholder {"@MTCONNECT_CONTENT@"};
  const string HtmlAccept {"text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"};
}  // namespace

class HtmlPrinterTest : public testing::Test
{
protected:
  void SetUp() override
  {
    ifstream stream(BrowserViewFile, ios::binary);
    ASSERT_TRUE(stream) << "Cannot open " << BrowserViewFile;
    string view(istreambuf_iterator<char>(stream), {});

    auto pos = view.find(Placeholder);
    ASSERT_NE(string::npos, pos) << BrowserViewFile << " does not contain " << Placeholder;
    m_prefix = view.substr(0, pos);
    m_suffix = view.substr(pos + Placeholder.size());

    createAgent(BrowserViewFile);
  }

  void TearDown() override { m_agentTestHelper.reset(); }

  void createAgent(const string& view)
  {
    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "1.7", 25, true, true,
                                   {{configuration::BrowserView, view}});
  }

  /// @brief Make a GET request the way a browser does and return the body
  const string& request(const char* path, const QueryMap& queries = {},
                        const string& accepts = HtmlAccept)
  {
    m_agentTestHelper->makeRequest(__FILE__, __LINE__, boost::beast::http::verb::get, "", queries,
                                   path, accepts.c_str());
    return m_agentTestHelper->session()->m_body;
  }

  /// @brief Check the body is the browser view with a document in place of the placeholder and
  ///        return that document.
  string embeddedDocument(const string& body)
  {
    EXPECT_EQ("text/html", m_agentTestHelper->session()->m_mimeType);
    if (body.size() < m_prefix.size() + m_suffix.size() || !body.starts_with(m_prefix) ||
        !body.ends_with(m_suffix))
    {
      ADD_FAILURE() << "Response is not the browser view:\n" << body;
      return "";
    }

    return body.substr(m_prefix.size(), body.size() - m_prefix.size() - m_suffix.size());
  }

  /// @brief Parse the document embedded in the browser view
  xmlDocPtr parseEmbedded(const string& body)
  {
    auto content = embeddedDocument(body);
    EXPECT_EQ(string::npos, content.find("</script")) << "Content would close the script element";
    return xmlParseMemory(content.c_str(), int32_t(content.size()));
  }

  static string rootName(xmlDocPtr doc)
  {
    auto root = xmlDocGetRootElement(doc);
    return root ? string((const char*)root->name) : string();
  }

  void putAsset()
  {
    string body = "<FakeAsset assetId='P1' deviceUuid='LinuxCNC'>TEST</FakeAsset>";
    QueryMap queries {{"type", "FakeAsset"}, {"device", "LinuxCNC"}};
    m_agentTestHelper->makeRequest(__FILE__, __LINE__, boost::beast::http::verb::put, body, queries,
                                   "/asset/P1", "text/xml");
    ASSERT_EQ(status::ok, m_agentTestHelper->session()->m_code);
    ASSERT_EQ(1u, m_agentTestHelper->getAgent()->getAssetStorage()->getCount());
  }

  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
  string m_prefix;
  string m_suffix;
};

#define PARSE_HTML_RESPONSE(...)                      \
  auto doc = parseEmbedded(request(__VA_ARGS__));     \
  ASSERT_TRUE(doc) << "Embedded document is not XML"; \
  XmlDocFreer cleanup(doc)

TEST_F(HtmlPrinterTest, should_register_html_printer_when_browser_view_is_configured)
{
  auto printer = m_agentTestHelper->getAgent()->getPrinter("html");
  ASSERT_NE(nullptr, printer);
  ASSERT_NE(nullptr, dynamic_cast<const printer::HtmlPrinter*>(printer));
  ASSERT_EQ("text/html", printer->mimeType());
}

TEST_F(HtmlPrinterTest, should_rest_sink_should_default_to_xml_printer)
{
  auto rest = m_agentTestHelper->getRestService();
  auto printer = rest->printerForAccepts("application/foomoo,text/glop");
  ASSERT_NE(nullptr, printer);
  ASSERT_NE(nullptr, dynamic_cast<const printer::XmlPrinter*>(printer));
  ASSERT_EQ("application/xml", printer->mimeType());
}

TEST_F(HtmlPrinterTest, should_rest_sink_should_default_to_xml_printer_when_accepts_is_blank)
{
  auto rest = m_agentTestHelper->getRestService();
  auto printer = rest->printerForAccepts("");
  ASSERT_NE(nullptr, printer);
  ASSERT_NE(nullptr, dynamic_cast<const printer::XmlPrinter*>(printer));
  ASSERT_EQ("application/xml", printer->mimeType());
}

TEST_F(HtmlPrinterTest, should_embed_probe_document_in_browser_view)
{
  PARSE_HTML_RESPONSE("/probe");
  ASSERT_EQ(status::ok, m_agentTestHelper->session()->m_code);
  ASSERT_EQ("MTConnectDevices", rootName(doc));
  ASSERT_XML_PATH_EQUAL(doc, "//m:Header@bufferSize", "256");
  ASSERT_XML_PATH_EQUAL(doc, "//m:Device@name", "LinuxCNC");
  ASSERT_XML_PATH_COUNT(doc, "//m:Device", 1);
}

TEST_F(HtmlPrinterTest, should_embed_device_probe_document_in_browser_view)
{
  PARSE_HTML_RESPONSE("/LinuxCNC/probe");
  ASSERT_EQ(status::ok, m_agentTestHelper->session()->m_code);
  ASSERT_EQ("MTConnectDevices", rootName(doc));
  ASSERT_XML_PATH_EQUAL(doc, "//m:Device@name", "LinuxCNC");
}

TEST_F(HtmlPrinterTest, should_embed_current_document_in_browser_view)
{
  auto agent = m_agentTestHelper->getAgent();
  auto di = agent->getDataItemForDevice("LinuxCNC", "p5");
  ASSERT_TRUE(di);
  m_agentTestHelper->addToBuffer(di, {{"VALUE", "ACTIVE"s}}, chrono::system_clock::now());

  PARSE_HTML_RESPONSE("/current");
  ASSERT_EQ(status::ok, m_agentTestHelper->session()->m_code);
  ASSERT_EQ("MTConnectStreams", rootName(doc));
  ASSERT_XML_PATH_COUNT(doc, "//m:DeviceStream[@name='LinuxCNC']", 1);
  ASSERT_XML_PATH_EQUAL(doc, "//m:Execution[@dataItemId='p5']", "ACTIVE");
}

TEST_F(HtmlPrinterTest, should_embed_sample_document_in_browser_view)
{
  auto agent = m_agentTestHelper->getAgent();
  auto di = agent->getDataItemForDevice("LinuxCNC", "p5");
  ASSERT_TRUE(di);
  m_agentTestHelper->addToBuffer(di, {{"VALUE", "READY"s}}, chrono::system_clock::now());
  auto seq =
      m_agentTestHelper->addToBuffer(di, {{"VALUE", "ACTIVE"s}}, chrono::system_clock::now());

  PARSE_HTML_RESPONSE("/sample", {{"from", to_string(seq - 1)}, {"count", "2"}});
  ASSERT_EQ(status::ok, m_agentTestHelper->session()->m_code);
  ASSERT_EQ("MTConnectStreams", rootName(doc));
  ASSERT_XML_PATH_COUNT(doc, "//m:Execution[@dataItemId='p5']", 2);
  ASSERT_XML_PATH_EQUAL(doc, ("//m:Execution[@sequence='" + to_string(seq - 1) + "']").c_str(),
                        "READY");
  ASSERT_XML_PATH_EQUAL(doc, ("//m:Execution[@sequence='" + to_string(seq) + "']").c_str(),
                        "ACTIVE");
}

TEST_F(HtmlPrinterTest, should_embed_assets_document_in_browser_view)
{
  putAsset();

  {
    PARSE_HTML_RESPONSE("/assets");
    ASSERT_EQ(status::ok, m_agentTestHelper->session()->m_code);
    ASSERT_EQ("MTConnectAssets", rootName(doc));
    ASSERT_XML_PATH_EQUAL(doc, "//m:Header@assetCount", "1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:FakeAsset@assetId", "P1");
    ASSERT_XML_PATH_EQUAL(doc, "//m:FakeAsset", "TEST");
  }

  {
    PARSE_HTML_RESPONSE("/asset/P1");
    ASSERT_EQ(status::ok, m_agentTestHelper->session()->m_code);
    ASSERT_EQ("MTConnectAssets", rootName(doc));
    ASSERT_XML_PATH_EQUAL(doc, "//m:FakeAsset@assetId", "P1");
  }
}

TEST_F(HtmlPrinterTest, should_use_format_and_pretty_parameters_for_asset_ids)
{
  // The agent wide Pretty option would hide whether the pretty parameter is used
  m_agentTestHelper = make_unique<AgentTestHelper>();
  m_agentTestHelper->createAgent(
      "/samples/test_config.xml", 8, 4, "1.7", 25, true, true,
      {{configuration::BrowserView, BrowserViewFile}, {configuration::Pretty, false}});
  putAsset();

  for (auto path : {"/asset/P1", "/assets/P1"})
  {
    {
      auto& body = request(path, {{"format", "json"}, {"pretty", "true"}});
      ASSERT_TRUE(m_agentTestHelper->session()->m_mimeType.ends_with("json")) << path;
      ASSERT_NE(string::npos, body.find("\n  ")) << path << " is not pretty printed:\n" << body;
      auto json = nlohmann::json::parse(body);
      ASSERT_TRUE(json.contains("MTConnectAssets")) << path;
    }

    {
      auto& body = request(path, {{"format", "json"}});
      ASSERT_TRUE(m_agentTestHelper->session()->m_mimeType.ends_with("json")) << path;
      ASSERT_EQ(string::npos, body.find('\n')) << path << " is pretty printed:\n" << body;
    }

    {
      auto& body = request(path, {{"format", "xml"}, {"pretty", "true"}});
      ASSERT_EQ("application/xml", m_agentTestHelper->session()->m_mimeType) << path;
      ASSERT_NE(string::npos, body.find(">\n  <")) << path << " is not pretty printed:\n" << body;
      auto doc = xmlParseMemory(body.c_str(), int32_t(body.size()));
      ASSERT_TRUE(doc) << path;
      XmlDocFreer cleanup(doc);
      ASSERT_XML_PATH_EQUAL(doc, "//m:FakeAsset@assetId", "P1");
    }

    {
      PARSE_HTML_RESPONSE(path);
      ASSERT_EQ("MTConnectAssets", rootName(doc)) << path;
    }
  }
}

TEST_F(HtmlPrinterTest, should_embed_error_document_for_unknown_device)
{
  PARSE_HTML_RESPONSE("/LinuxCN/probe");
  ASSERT_EQ(status::not_found, m_agentTestHelper->session()->m_code);
  ASSERT_EQ("MTConnectError", rootName(doc));
  ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "NO_DEVICE");
}

TEST_F(HtmlPrinterTest, should_embed_error_document_for_unknown_asset)
{
  PARSE_HTML_RESPONSE("/asset/NOPE");
  ASSERT_EQ(status::not_found, m_agentTestHelper->session()->m_code);
  ASSERT_EQ("MTConnectError", rootName(doc));
  ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "ASSET_NOT_FOUND");
}

TEST_F(HtmlPrinterTest, should_embed_error_document_for_invalid_request)
{
  {
    PARSE_HTML_RESPONSE("/bad_path");
    ASSERT_EQ(status::not_found, m_agentTestHelper->session()->m_code);
    ASSERT_EQ("MTConnectError", rootName(doc));
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_URI");
  }

  {
    PARSE_HTML_RESPONSE("/sample", {{"count", "xxx"}});
    ASSERT_EQ(status::bad_request, m_agentTestHelper->session()->m_code);
    ASSERT_EQ("MTConnectError", rootName(doc));
    ASSERT_XML_PATH_EQUAL(doc, "//m:Error@errorCode", "INVALID_PARAMETER_VALUE");
  }
}

TEST_F(HtmlPrinterTest, should_return_plain_xml_when_xml_is_accepted)
{
  auto& body = request("/probe", {}, "application/xml");
  ASSERT_EQ("application/xml", m_agentTestHelper->session()->m_mimeType);
  ASSERT_TRUE(body.starts_with("<?xml")) << body;
  ASSERT_EQ(string::npos, body.find(Placeholder));
  ASSERT_FALSE(body.starts_with(m_prefix));
}

TEST_F(HtmlPrinterTest, should_use_format_parameter_over_accept_header)
{
  {
    auto& body = request("/current", {{"format", "xml"}});
    ASSERT_EQ("application/xml", m_agentTestHelper->session()->m_mimeType);
    ASSERT_TRUE(body.starts_with("<?xml")) << body;
  }

  {
    auto& body = request("/current", {{"format", "json"}});
    ASSERT_TRUE(m_agentTestHelper->session()->m_mimeType.ends_with("json"));
    auto json = nlohmann::json::parse(body);
    ASSERT_TRUE(json.contains("MTConnectStreams"));
  }

  {
    PARSE_HTML_RESPONSE("/current", {{"format", "html"}}, "application/xml");
    ASSERT_EQ("MTConnectStreams", rootName(doc));
  }
}

TEST_F(HtmlPrinterTest, should_not_register_html_printer_without_browser_view)
{
  m_agentTestHelper = make_unique<AgentTestHelper>();
  m_agentTestHelper->createAgent("/samples/test_config.xml", 8, 4, "1.7", 25, true);
  ASSERT_EQ(nullptr, m_agentTestHelper->getAgent()->getPrinter("html"));

  auto& body = request("/probe");
  ASSERT_EQ("application/xml", m_agentTestHelper->session()->m_mimeType);
  ASSERT_TRUE(body.starts_with("<?xml")) << body;
}

TEST_F(HtmlPrinterTest, should_return_document_when_browser_view_cannot_be_found)
{
  createAgent("/no/such/viewer.html");

  auto& body = request("/probe");
  ASSERT_EQ(status::ok, m_agentTestHelper->session()->m_code);
  ASSERT_TRUE(body.starts_with("<?xml")) << body;
  ASSERT_EQ(string::npos, body.find(Placeholder));
}
