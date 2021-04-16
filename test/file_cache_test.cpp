// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "http_server/file_cache.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using namespace std;
using namespace mtconnect;
using namespace mtconnect::http_server;

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
