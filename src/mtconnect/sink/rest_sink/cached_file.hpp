//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

#pragma once

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>

#include "mtconnect/config.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect::sink::rest_sink {
  struct CachedFile;
  using CachedFilePtr = std::shared_ptr<CachedFile>;
  /// @brief A wrapper around a singe cached file that (< 10k) that is served from memory. Also
  /// supports files dynamically read from the operating system.
  struct CachedFile : public std::enable_shared_from_this<CachedFile>
  {
    // Small file size
    static const int SMALL_FILE = 10 * 1024;  // 10k is considered small

    /// @brief Create a cached file with no buffer
    CachedFile() : m_buffer(nullptr) {}
    ~CachedFile()
    {
      if (m_buffer != nullptr)
        free(m_buffer);
    }
    /// @brief get the shared pointer to this file
    /// @return shared pointer to this
    CachedFilePtr getptr() { return shared_from_this(); }

    /// @brief Create a cached file from another file specifying the mime type
    /// @param file the file to copy
    /// @param mime the new mime type
    CachedFile(const CachedFile &file, const std::string &mime)
      : m_size(file.m_size),
        m_mimeType(mime),
        m_path(file.m_path),
        m_cached(file.m_cached),
        m_lastWrite(file.m_lastWrite)
    {
      if (m_cached)
      {
        allocate(file.m_size);
        std::memcpy(m_buffer, file.m_buffer, file.m_size);
      }
    }

    /// @brief Create a cached file from a buffer and a mime type
    /// @param buffer a pointer to the buffer
    /// @param size the buffer size
    /// @param mime the mime type
    CachedFile(const char *buffer, size_t size, const std::string &mime)
      : m_buffer(nullptr), m_size(size), m_mimeType(mime)
    {
      allocate(m_size);
      std::memcpy(m_buffer, buffer, m_size);
    }

    /// @brief Create a cached file with an fixed size
    /// @param size the buffer size
    CachedFile(size_t size) : m_buffer(nullptr), m_size(size) { allocate(m_size); }

    /// @brief Create a cahed file by loading the file from the file system
    /// @param path the path to the file
    /// @param mime the mime type of the file
    /// @param cached `true` if the buffer should be allocated
    /// @param size optional size; if 0, size of will be determined from the operating system
    CachedFile(const std::filesystem::path &path, const std::string &mime, bool cached = true,
               size_t size = 0)
      : m_buffer(nullptr), m_mimeType(mime), m_path(path), m_cached(cached)
    {
      if (size == 0)
        m_size = std::filesystem::file_size(path);
      else
        m_size = size;
      if (cached)
      {
        allocate(m_size);
        std::filebuf file;
        if (file.open(m_path, std::ios::binary | std::ios::in) != nullptr)
          m_size = file.sgetn(m_buffer, m_size);
        else
        {
          LOG(warning) << "Cannot open cached file: " << path;
          m_cached = false;
        }
      }
      m_lastWrite = std::filesystem::last_write_time(m_path);
    }

    /// @brief Clone a CachedFile
    /// @param file the file
    /// @return this
    CachedFile &operator=(const CachedFile &file)
    {
      m_cached = file.m_cached;
      m_path = file.m_path;
      if (m_cached)
      {
        allocate(file.m_size);
        std::memcpy(m_buffer, file.m_buffer, m_size);
      }
      return *this;
    }

    /// @brief Allocate a buffer. If a buffer already exists, free it. Zeros the allocated memory.
    /// @param size The size to allocate.
    void allocate(size_t size)
    {
      if (m_buffer != nullptr)
        free(m_buffer);
      m_size = size;
      m_buffer = static_cast<char *>(malloc(m_size + 1));
      memset(m_buffer, 0, m_size + 1);
    }

    char *m_buffer {nullptr};
    size_t m_size {0};
    std::string m_mimeType;
    std::filesystem::path m_path;
    std::optional<std::filesystem::path> m_pathGz;
    bool m_cached {true};
    std::filesystem::file_time_type m_lastWrite;
    std::optional<std::string> m_redirect;
  };
}  // namespace mtconnect::sink::rest_sink
