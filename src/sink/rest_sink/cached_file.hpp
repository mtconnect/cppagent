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

#pragma once

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>

#include "utilities.hpp"

namespace mtconnect {
  namespace sink {
    namespace rest_sink {
      struct CachedFile;
      using CachedFilePtr = std::shared_ptr<CachedFile>;
      struct CachedFile : public std::enable_shared_from_this<CachedFile>
      {
        // Small file size
        static const int SMALL_FILE = 10 * 1024;  // 10k is considered small

        CachedFile() : m_buffer(nullptr) {}
        ~CachedFile()
        {
          if (m_buffer != nullptr)
            free(m_buffer);
        }
        CachedFilePtr getptr() { return shared_from_this(); }

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

        CachedFile(const char *buffer, size_t size, const std::string &mime)
          : m_buffer(nullptr), m_size(size), m_mimeType(mime)
        {
          allocate(m_size);
          std::memcpy(m_buffer, buffer, m_size);
        }

        CachedFile(size_t size) : m_buffer(nullptr), m_size(size) { allocate(m_size); }

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
            auto file = std::fopen(path.string().c_str(), "rb");
            m_size = std::fread(m_buffer, 1, m_size, file);
          }
          m_lastWrite = std::filesystem::last_write_time(m_path);
        }

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

        void allocate(size_t size)
        {
          if (m_buffer != nullptr)
            free(m_buffer);
          m_size = size;
          m_buffer = static_cast<char *>(malloc(m_size + 1));
          memset(m_buffer, 0, m_size + 1);
        }

        char *m_buffer;
        size_t m_size {0};
        std::string m_mimeType;
        std::filesystem::path m_path;
        std::optional<std::filesystem::path> m_pathGz;
        bool m_cached {true};
        std::filesystem::file_time_type m_lastWrite;
        std::optional<std::string> m_redirect;
      };
    }  // namespace rest_sink
  }    // namespace sink
}  // namespace mtconnect
