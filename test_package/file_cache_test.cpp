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

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include "mtconnect/sink/rest_sink/file_cache.hpp"

#ifndef _WINDOWS
#include <sys/time.h>
#endif

using namespace std;
using namespace mtconnect;
using namespace mtconnect::sink::rest_sink;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class FileCacheTest : public testing::Test
{
protected:
  void SetUp() override { m_cache = make_unique<FileCache>(); }

  void TearDown() override { m_cache.reset(); }

  unique_ptr<FileCache> m_cache;
};

TEST_F(FileCacheTest, FindFiles)
{
  string uri("/schemas");

  // Register a file with the agent.
  auto list = m_cache->registerDirectory(uri, TEST_RESOURCE_DIR "/schemas", "1.7");

  ASSERT_TRUE(m_cache->hasFile("/schemas/MTConnectDevices_1.7.xsd"));
  auto file = m_cache->getFile("/schemas/MTConnectDevices_1.7.xsd");
  ASSERT_TRUE(file);
  EXPECT_EQ("text/xml", file->m_mimeType);
}

TEST_F(FileCacheTest, IconMimeType)
{
  // Register a file with the agent.
  auto list = m_cache->registerDirectory("/styles", TEST_RESOURCE_DIR "/styles", "1.7");

  auto file = m_cache->getFile("/styles/favicon.ico");
  ASSERT_TRUE(file);
  EXPECT_EQ("image/x-icon", file->m_mimeType);
}

TEST_F(FileCacheTest, verify_large_files_are_not_cached)
{
  // Make a cache that can only hold 1024 byte files
  m_cache = make_unique<FileCache>(1024);

  m_cache->addDirectory("/schemas", TEST_RESOURCE_DIR "/schemas", "none.xsd");
  m_cache->addDirectory("/styles", TEST_RESOURCE_DIR "/styles", "none.css");

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
  m_cache->addDirectory("/schemas", TEST_RESOURCE_DIR "/schemas", "none.xsd");
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

TEST_F(FileCacheTest, file_cache_should_compress_file)
{
  namespace fs = std::filesystem;

  // Cleanup
  fs::path zipped(TEST_RESOURCE_DIR);
  zipped /= "zipped_file.txt.gz";
  if (fs::exists(zipped))
  {
    fs::remove(zipped);
  }

  m_cache->addDirectory("/resources", TEST_RESOURCE_DIR, "none.txt");
  m_cache->setMinCompressedFileSize(1024);
  auto file = m_cache->getFile("/resources/zipped_file.txt");

  ASSERT_TRUE(file);
  EXPECT_EQ("text/plain", file->m_mimeType);
  EXPECT_TRUE(file->m_cached);
  EXPECT_FALSE(file->m_pathGz);

  auto gzFile = m_cache->getFile("/resources/zipped_file.txt", "gzip, deflate"s);

  ASSERT_TRUE(gzFile);
  EXPECT_EQ("text/plain", gzFile->m_mimeType);
  EXPECT_TRUE(gzFile->m_cached);
  EXPECT_TRUE(gzFile->m_pathGz);

  // Cleanup
  if (fs::exists(zipped))
  {
    fs::remove(zipped);
  }
}

TEST_F(FileCacheTest, file_cache_should_compress_file_async)
{
  namespace fs = std::filesystem;

  // Cleanup
  fs::path zipped(TEST_RESOURCE_DIR);
  zipped /= "zipped_file.txt.gz";
  if (fs::exists(zipped))
  {
    fs::remove(zipped);
  }

  m_cache->addDirectory("/resources", TEST_RESOURCE_DIR, "none.txt");
  m_cache->setMinCompressedFileSize(1024);

  boost::asio::io_context context;

  boost::asio::post(context, [&context, this]() {
    auto gzFile = m_cache->getFile("/resources/zipped_file.txt", "gzip, deflate"s, &context);

    ASSERT_TRUE(gzFile);
    EXPECT_EQ("text/plain", gzFile->m_mimeType);
    EXPECT_TRUE(gzFile->m_cached);
    EXPECT_TRUE(gzFile->m_pathGz);

    context.stop();
  });

  bool ran {false};
  context.post([&ran] { ran = true; });

  context.run();
  // EXPECT_TRUE(ran);

  // Cleanup
  if (fs::exists(zipped))
  {
    fs::remove(zipped);
  }
}

static inline void touch(const std::filesystem::path &file)
{
  namespace fs = std::filesystem;
  namespace ch = std::chrono;

  auto now = fs::file_time_type::clock::now();
  fs::last_write_time(file, now);
}

TEST_F(FileCacheTest, file_cache_should_recompress_if_gzip_older_than_file)
{
  namespace fs = std::filesystem;
  namespace ch = std::chrono;

  // Cleanup
  fs::path zipped(TEST_RESOURCE_DIR);
  zipped /= "zipped_file.txt.gz";
  if (fs::exists(zipped))
  {
    fs::remove(zipped);
  }

  m_cache->addDirectory("/resources", TEST_RESOURCE_DIR, "none.txt");
  m_cache->setMinCompressedFileSize(1024);
  auto gzFile = m_cache->getFile("/resources/zipped_file.txt", "gzip, deflate"s);

  ASSERT_TRUE(gzFile);
  EXPECT_EQ("text/plain", gzFile->m_mimeType);
  EXPECT_TRUE(gzFile->m_cached);
  EXPECT_TRUE(gzFile->m_pathGz);

  ASSERT_TRUE(fs::exists(*gzFile->m_pathGz));

  auto zipTime = fs::last_write_time(*gzFile->m_pathGz);
  auto fileTime = fs::last_write_time(gzFile->m_path);
  ASSERT_GT(zipTime, fileTime);

  std::this_thread::sleep_for(1s);
  touch(gzFile->m_path);
  std::this_thread::sleep_for(1s);
  auto gzFile2 = m_cache->getFile("/resources/zipped_file.txt", "gzip, deflate"s);
  ASSERT_TRUE(gzFile2);

  auto zipTime2 = fs::last_write_time(*gzFile2->m_pathGz);
  ASSERT_GT(zipTime2, zipTime);

  auto fileTime2 = fs::last_write_time(gzFile2->m_path);
  ASSERT_GT(zipTime2, fileTime2);

  m_cache->clear();

  std::this_thread::sleep_for(1s);
  touch(gzFile->m_path);
  std::this_thread::sleep_for(1s);
  auto gzFile3 = m_cache->getFile("/resources/zipped_file.txt", "gzip, deflate"s);
  ASSERT_TRUE(gzFile3);

  auto zipTime3 = fs::last_write_time(*gzFile3->m_pathGz);
  ASSERT_GT(zipTime3, zipTime2);

  auto fileTime3 = fs::last_write_time(gzFile3->m_path);
  ASSERT_GT(zipTime3, fileTime3);

  // Cleanup
  if (fs::exists(zipped))
  {
    fs::remove(zipped);
  }
}
