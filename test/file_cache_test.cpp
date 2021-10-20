// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include <boost/algorithm/string.hpp>

#include "rest_sink/file_cache.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using namespace std;
using namespace mtconnect;
using namespace mtconnect::rest_sink;

class FileCacheTest : public testing::Test
{
 protected:
  void SetUp() override
  {
    m_cache = make_unique<FileCache>();
  }

  void TearDown() override
  {
    m_cache.reset();
  }
  
  unique_ptr<FileCache> m_cache;
};

TEST_F(FileCacheTest, FindFiles)
{
  string uri("/schemas");

  // Register a file with the agent.
  auto list = m_cache->registerDirectory(uri, PROJECT_ROOT_DIR "/schemas",
                                         "1.7");
  
  ASSERT_TRUE(m_cache->hasFile("/schemas/MTConnectDevices_1.7.xsd"));
  auto file = m_cache->getFile("/schemas/MTConnectDevices_1.7.xsd");
  ASSERT_TRUE(file);
  EXPECT_EQ("text/xml", file->m_mimeType);
}

TEST_F(FileCacheTest, IconMimeType)
{
  // Register a file with the agent.
  auto list = m_cache->registerDirectory("/styles", PROJECT_ROOT_DIR "/styles",
                                         "1.7");
  
  auto file = m_cache->getFile("/styles/favicon.ico");
  ASSERT_TRUE(file);
  EXPECT_EQ("image/x-icon", file->m_mimeType);
}

TEST_F(FileCacheTest, verify_large_files_are_not_cached)
{
  // Make a cache that can only hold 1024 byte files
  m_cache = make_unique<FileCache>(1024);
  
  m_cache->addDirectory("/schemas", PROJECT_ROOT_DIR "/schemas", "none.xsd");
  m_cache->addDirectory("/styles", PROJECT_ROOT_DIR "/styles", "none.css");

  ASSERT_FALSE(m_cache->hasFile("/schemas/MTConnectDevices_1.7.xsd"));
  auto file = m_cache->getFile("/schemas/MTConnectDevices_1.7.xsd");
  ASSERT_TRUE(file);
  ASSERT_FALSE(file->m_cached);
  ASSERT_LT(0, file->m_size);
  ASSERT_TRUE(m_cache->hasFile("/schemas/MTConnectDevices_1.7.xsd"));
  
  auto css = m_cache->getFile("/styles/Streams.css");
  ASSERT_TRUE(css);
  ASSERT_TRUE(css->m_cached);
}

TEST_F(FileCacheTest, base_directory_should_redirect)
{
  m_cache->addDirectory("/schemas", PROJECT_ROOT_DIR "/schemas", "none.xsd");
  auto file = m_cache->getFile("/schemas");
  ASSERT_TRUE(file);
  ASSERT_EQ("/schemas/none.xsd", file->m_redirect);
  ASSERT_TRUE(m_cache->hasFile("/schemas"));
  ASSERT_TRUE(boost::starts_with(std::string(file->m_buffer), "<html>"));

  auto file2 = m_cache->getFile("/schemas");
  ASSERT_TRUE(file);
  ASSERT_EQ("/schemas/none.xsd", file2->m_redirect);
  ASSERT_TRUE(m_cache->hasFile("/schemas"));
  ASSERT_TRUE(boost::starts_with(std::string(file->m_buffer), "<html>"));
}

