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

#pragma once

#include "globals.hpp"
#include "ref_counted.hpp"

#include <memory>
#include <filesystem>
#include <cstdio>
#include <cstring>

namespace mtconnect
{
  namespace http_server
  {
    class CachedFile;
    using CachedFilePtr = std::shared_ptr<CachedFile>;
    struct CachedFile : public  std::enable_shared_from_this<CachedFile>
    {
      CachedFile() : m_buffer(nullptr) {}
      CachedFilePtr getptr() {
        return shared_from_this();
      }

      CachedFile(const CachedFile &file, const std::string &mime)
      : m_size(file.m_size), m_mimeType(mime)
      {
        m_buffer = std::make_unique<char[]>(file.m_size);
        std::memcpy(m_buffer.get(), file.m_buffer.get(), file.m_size);
      }

      CachedFile(char *buffer, size_t size) : m_buffer(nullptr), m_size(size)
      {
        m_buffer = std::make_unique<char[]>(m_size);
        std::memcpy(m_buffer.get(), buffer, size);
      }

      CachedFile(size_t size) : m_buffer(nullptr), m_size(size)
      {
        m_buffer = std::make_unique<char[]>(m_size);
      }
      
      CachedFile(const std::filesystem::path &path, const std::string &mime)
        : m_buffer(nullptr), m_mimeType(mime)
      {
        m_size = std::filesystem::file_size(path);
        m_buffer = std::make_unique<char[]>(m_size);
        auto file = std::fopen(path.c_str(), "r");
        std::fread(m_buffer.get(), 1, m_size, file);
      }


      ~CachedFile() { m_buffer.reset(); }

      CachedFile &operator=(const CachedFile &file)
      {
        m_buffer.reset();
        m_buffer = std::make_unique<char[]>(file.m_size);
        std::memcpy(m_buffer.get(), file.m_buffer.get(), file.m_size);
        m_size = file.m_size;
        return *this;
      }

      void allocate(size_t size)
      {
        m_buffer.reset();
        m_buffer = std::make_unique<char[]>(size);
        m_size = size;
      }
      
      std::unique_ptr<char[]> m_buffer;
      size_t m_size = 0;
      std::string m_mimeType;
    };
  }
}
