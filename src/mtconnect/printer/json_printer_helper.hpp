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

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace mtconnect::printer {
  
  template <typename T>
  struct JsonHelper
  {
    JsonHelper(T &writer) : m_writer(writer) {}

    void Key(const char *s) { m_writer.Key(s); }

    void StartObject() { m_writer.StartObject(); }

    void EndObject() { m_writer.EndObject(); }

    void StartArray() { m_writer.StartArray(); }

    void EndArray() { m_writer.EndArray(); }

    void Add(double v) { m_writer.Double(v); }

    void Add(bool v) { m_writer.Bool(v); }

    void Add(int32_t i) { m_writer.Int(i); }

    void Add(uint32_t i) { m_writer.Uint(i); }

    void Add(int64_t i) { m_writer.Int64(i); }

    void Add(uint64_t i) { m_writer.Uint64(i); }

    void Add(const char *s) { m_writer.String(s); }

    void Add(const std::string &s) { m_writer.String(s.data(), rapidjson::SizeType(s.size())); }

    T &m_writer;
  };

  //----------------
  template <typename T>
  struct AutoJsonObject : JsonHelper<T>
  {
    using base = JsonHelper<T>;

    AutoJsonObject(T &writer, const char *key)
    : JsonHelper<T>(writer)
    {
      base::Key(key);
      base::StartObject();
    }

    AutoJsonObject(T &writer, bool start = true) : JsonHelper<T>(writer)
    {
      if (start)
        base::StartObject();
    }

    AutoJsonObject(T &writer, const std::string_view key)
    : JsonHelper<T>(writer)
    {
      base::Key(key.data());
      base::StartObject();
    }

    ~AutoJsonObject()
    {
      if (!m_ended)
        base::EndObject();
    }

    template <typename T1, typename T2, typename... R>
    void AddPairs(const T1 &v1, const T2 &v2, R... rest)
    {
      base::Key(v1);
      base::Add(v2);

      AddPairs(rest...);
    }

    template <typename T1, typename T2, typename... R>
    void AddPairs(const T1 &v1, const T2 &v2)
    {
      base::m_writer.Key(v1);
      base::Add(v2);
    }
    
    bool reset(const std::string_view key)
    {
      if (m_key != key)
      {
        if (!m_key.empty())
          base::EndObject();
        if (!key.empty())
        {
          base::Key(key.data());
          base::StartObject();
        }
        m_key = key;
        return true;
      }
      else
      {
        return false;
      }
    }
    
    void end()
    {
      base::EndObject();
      m_ended = true;
    }
    
    std::string_view m_key;
    bool m_ended { false };
  };

  template <typename T>
  struct AutoJsonArray : JsonHelper<T>
  {
    using base = JsonHelper<T>;
    AutoJsonArray(T &writer) : JsonHelper<T>(writer) { base::StartArray(); }
    
    AutoJsonArray(T &writer, const std::string_view key)
    : JsonHelper<T>(writer)
    {
      base::Key(key.data());
      base::StartArray();
    }

    ~AutoJsonArray()
    {
      if (!m_ended)
        base::EndArray();
    }
    
    void end()
    {
      base::EndArray();
      m_ended = true;
    }
    
    bool m_ended { false };
  };
}
