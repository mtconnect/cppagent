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

namespace mtconnect
{
  struct CachedFile : public RefCounted
  {
    std::unique_ptr<char[]> m_buffer;
    size_t m_size = 0;

    CachedFile() : m_buffer(nullptr) {}

    CachedFile(const CachedFile &file) : RefCounted(file), m_buffer(nullptr), m_size(file.m_size)
    {
      m_buffer = std::make_unique<char[]>(file.m_size);
      memcpy(m_buffer.get(), file.m_buffer.get(), file.m_size);
    }

    CachedFile(char *buffer, size_t size) : m_buffer(nullptr), m_size(size)
    {
      m_buffer = std::make_unique<char[]>(m_size);
      memcpy(m_buffer.get(), buffer, size);
    }

    CachedFile(size_t size) : m_buffer(nullptr), m_size(size)
    {
      m_buffer = std::make_unique<char[]>(m_size);
    }

    ~CachedFile() override { m_buffer.reset(); }

    CachedFile &operator=(const CachedFile &file)
    {
      m_buffer.reset();
      m_buffer = std::make_unique<char[]>(file.m_size);
      memcpy(m_buffer.get(), file.m_buffer.get(), file.m_size);
      m_size = file.m_size;
      return *this;
    }

    void allocate(size_t size)
    {
      m_buffer.reset();
      m_buffer = std::make_unique<char[]>(size);
      m_size = size;
    }
  };
  
}
