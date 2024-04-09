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

#include <cmath>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace mtconnect::printer {

  /// @brief Abstract helper wrapping the rapidjson writer and providing some helper methods
  /// serializing types.
  ///
  /// Use overloaded Add and Key methods to serialize data to the writer. This class can be used
  /// with the pretty or regular writer.
  ///
  /// @tparam T the type of writer used.
  template <typename T>
  class AGENT_LIB_API JsonHelper
  {
  public:
    /// @brief Create a helper using a writer
    /// @param[in] writer a reference to the writer
    JsonHelper(T &writer) : m_writer(writer) {}

    /// @name Key methods
    /// @{

    /// @brief Wrapper around the rapidjson Key writer methods
    /// @param[in] s string for the key
    void Key(const char *s) { m_writer.Key(s); }
    /// @brief Wrapper around the rapidjson Key writer methods
    /// @param[in] s string for the key
    void Key(const std::string &s) { m_writer.Key(s.data(), rapidjson::SizeType(s.size())); }
    /// @brief Wrapper around the rapidjson Key writer methods
    /// @param[in] s string for the key
    void Key(const std::string_view &s) { m_writer.Key(s.data(), rapidjson::SizeType(s.size())); }
    /// @}

    /// @name Object methods
    /// @{

    /// @brief Start a json object
    void StartObject() { m_writer.StartObject(); }
    /// @brief End a json object
    void EndObject() { m_writer.EndObject(); }
    /// @}

    /// @name Array methods
    /// @{

    /// @brief Start a json array
    void StartArray() { m_writer.StartArray(); }
    void EndArray() { m_writer.EndArray(); }
    /// @}

    /// @name Add methods
    /// @{

    /// @brief Add a double
    /// @param[in] v double value
    void Add(double v)
    {
      if (std::isfinite(v))
        m_writer.Double(v);
      else if (std::isnan(v))
        m_writer.String("NaN");
      else if (std::isinf(v))
      {
        if (std::signbit(v))
          m_writer.String("-Infinity");
        else
          m_writer.String("Infinity");
      }
    }
    /// @brief Add a bool
    /// @param[in] v bool value
    void Add(bool v) { m_writer.Bool(v); }
    /// @brief Add a signed 32 bit integer
    /// @param[in] i integer value
    void Add(int32_t i) { m_writer.Int(i); }
    /// @brief Add a unsigned 32 bit integer
    /// @param[in] i integer value
    void Add(uint32_t i) { m_writer.Uint(i); }
    /// @brief Add a signed 64 bit integer
    /// @param[in] i integer value
    void Add(int64_t i) { m_writer.Int64(i); }
    /// @brief Add a unsigned 64 bit integer
    /// @param[in] i integer value
    void Add(uint64_t i) { m_writer.Uint64(i); }
    /// @brief Add a string
    /// @param[in] s a null terminated string
    void Add(const char *s) { m_writer.String(s); }
    /// @brief Add a string view
    /// @param[in] s a string
    void Add(const std::string_view &s)
    {
      m_writer.String(s.data(), rapidjson::SizeType(s.size()));
    }
    /// @brief Add a string
    /// @param[in] s a string
    void Add(const std::string &s) { m_writer.String(s.data(), rapidjson::SizeType(s.size())); }
    /// @}

    /// @brief Add pairs of values
    ///
    /// Paris are key, value pairs that using parameter packs as follows:
    /// ```
    ///    a.AddPairs("k1", "v1", "k2", "v2");
    /// ```
    ///
    /// @param[in] key First Key
    /// @param[in] value First value
    /// @param[in] rest remaining values
    /// @tparam T1 Key type
    /// @tparam T2 Value type
    /// @tparam R parameter pack for the rest of the values
    template <typename T1, typename T2, typename... R>
    void AddPairs(const T1 &key, const T2 &value, R... rest)
    {
      Key(key);
      Add(value);

      AddPairs(rest...);
    }

    /// @brief Add one pairs of values
    ///
    /// Method to handler recursive call to add pairs with two values or a simple pair.
    /// ```
    ///    a.AddPairs("k1", "v1");
    /// ```
    ///
    /// @param[in] key The  Key
    /// @param[in] value The value
    /// @tparam T1 Key type
    /// @tparam T2 Value type
    template <typename T1, typename T2>
    void AddPairs(const T1 &v1, const T2 &v2)
    {
      Key(v1);
      Add(v2);
    }

  protected:
    /// @brief The rapidjson writer
    T &m_writer;
  };

  /// @brief Provides rapidjson automatic StartObject and EndObject when the object goes out of
  /// scope
  /// @tparam T the writer type
  template <typename T>
  class AGENT_LIB_API AutoJsonObject : public JsonHelper<T>
  {
  public:
    /// @brief Aliased type for the base helper type
    using base = JsonHelper<T>;

    /// @brief Start an object as part of another objecy where this object is the value of `key`
    /// @param[in] writer the rapidjson writer
    /// @param[in] key the parent objects key
    AutoJsonObject(T &writer, const char *key) : base(writer)
    {
      base::Key(key);
      base::StartObject();
    }

    /// @brief Create a object, but only start it if `start` is true
    /// @param[in] writer the rapidjson writer
    /// @param[in] start flag to start the array
    AutoJsonObject(T &writer, bool start = true) : base(writer)
    {
      if (start)
        base::StartObject();
      else
        m_ended = true;
    }

    /// @brief Start an object as part of another objecy where this object is the value of `key`
    /// @param[in] writer the rapidjson writer
    /// @param[in] key the parent objects key
    AutoJsonObject(T &writer, const std::string_view key) : base(writer)
    {
      base::Key(key.data());
      base::StartObject();
    }

    /// @brief descructor that ends the object if it is not already ended
    ~AutoJsonObject()
    {
      if (!m_ended)
        base::EndObject();
    }

    /// @name Key based object management
    ///
    /// These methods track a key and open a new object if the key has changed.
    /// @{

    /// @brief Check if the key has changed
    /// @param[in] key key to check
    bool check(const std::string_view &key) { return m_key != key; }

    /// @brief Check if the key has changed and end an open object and start a new one
    /// @param[in] key key to check
    /// @param[in] addKey if `true`, add an object as the value of the parent object using key as
    /// the key
    bool reset(const std::string_view &key, bool addKey = true)
    {
      if (m_key != key)
      {
        if (!m_ended)
        {
          base::EndObject();
          m_ended = true;
        }
        if (m_ended && !key.empty())
        {
          if (addKey)
            base::Key(key.data());
          base::StartObject();
          m_ended = false;
        }
        m_key = key;
        return true;
      }
      else
      {
        return false;
      }
    }
    /// @}

    /// @brief End the object if open
    void end()
    {
      if (!m_ended)
        base::EndObject();

      m_key = "";
      m_ended = true;
    }

  protected:
    std::string_view m_key;
    bool m_ended {false};
  };

  /// @brief Provides rapidjson automatic StartArray and EndArray when the object goes out of scope
  /// @tparam T the writer type
  template <typename T>
  class AGENT_LIB_API AutoJsonArray : public JsonHelper<T>
  {
  public:
    /// @brief Aliased type for the base helper type
    using base = JsonHelper<T>;

    /// @brief Create a array, but only start it if `start` is true
    /// @param[in] writer the rapidjson writer
    /// @param[in] start flag to start the array
    AutoJsonArray(T &writer, bool start = true) : base(writer)
    {
      if (start)
        base::StartArray();
      else
        m_ended = true;
    }

    /// @brief Start an array as part of another objecy where this object is the value of `key`
    /// @param[in] writer the rapidjson writer
    /// @param[in] key the parent objects key
    AutoJsonArray(T &writer, const char *key) : base(writer)
    {
      base::Key(key);
      base::StartArray();
    }

    /// @brief Start an array as part of another objecy where this object is the value of `key`
    /// @param[in] writer the rapidjson writer
    /// @param[in] key the parent objects key
    AutoJsonArray(T &writer, const std::string_view key) : base(writer)
    {
      base::Key(key.data());
      base::StartArray();
    }

    /// @brief destructor the ends the array if it is still open
    ~AutoJsonArray()
    {
      if (!m_ended)
        base::EndArray();
    }

    /// @brief start the array if it is ended
    void start()
    {
      if (m_ended)
        base::StartArray();
      m_ended = false;
    }

    /// @brief end the array if it is open
    void end()
    {
      if (!m_ended)
        base::EndArray();
      m_ended = true;
    }

  protected:
    bool m_ended {false};
  };

  /// @brief Helper function that creates a rapidjson PrettyWriter or Writer depending on the pretty
  /// flag.
  ///
  /// Calls func with the writer allowing the correct templates to be instantiated depending on
  /// pretty printing.
  ///
  /// @param[in] output the rapidjson output object, like `StringBuffer`
  /// @param[in] pretty `true` creates a `rapidjson::PrettyWriter` and `false` creates a
  /// `rapidjson::Writer`
  /// @param[in] func the lambda to callback with the writer
  /// @tparam T the type of the output buffer
  /// @tparam T2 the type of the lambda
  template <typename T, typename T2>
  inline void RenderJson(T &output, bool pretty, T2 &&func)
  {
    if (pretty)
    {
      rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(output);
      writer.SetIndent(' ', 2);
      func(writer);
    }
    else
    {
      rapidjson::Writer<rapidjson::StringBuffer> writer(output);
      func(writer);
    }
  }

  /// @brief A hierarchy of Json Objects and Arrays that are automatically managed so the opens and
  /// closes always match.
  /// @tparam W the writer type
  template <typename W>
  class AGENT_LIB_API JsonStack : public JsonHelper<W>
  {
  public:
    /// @brief alias for the base helper type
    using base = JsonHelper<W>;

    /// @brief Alias fo the writer type
    using WriterType = W;
    /// @brief alias of the AutoJsonObject type for the WriterType
    using ObjectType = AutoJsonObject<WriterType>;
    /// @brief alias of the AutoJsonArray type for the WriterType
    using ArrayType = AutoJsonArray<WriterType>;
    /// @brief alias for a smart pointer to an AutoJsonObject type for the WriterType
    using ObjectPtr = std::optional<ObjectType>;
    /// @brief alias for a smart pointer to an AutoJsonArray type for the WriterType
    using ArrayPtr = std::optional<ArrayType>;

    /// @brief Structure that holds either an object or an array
    struct StackMember
    {
      ObjectPtr m_object;
      ArrayPtr m_array;
    };

    /// @brief alias for a list of variants
    using Stack = std::list<StackMember>;

    /// @brief Create a stack for a rapidjson writer
    /// @param[in] writer the rapidjson writer
    JsonStack(W &writer) : base(writer) {}

    /// @brief Add a new object to the stack
    /// @param[in] key optional key for the parent object
    void addObject(const std::string_view key = "")
    {
      if (!key.empty())
        base::Key(key);

      auto &member = m_stack.emplace_back();
      member.m_object.emplace(base::m_writer);
    }

    /// @brief Add a new array to the stack
    /// @param[in] key optional key for the parent object
    void addArray(const std::string_view key = "")
    {
      if (!key.empty())
        base::Key(key);

      auto &member = m_stack.emplace_back();
      member.m_array.emplace(base::m_writer);
    }

    /// @brief Closes open objects and arrays, stopping when the size is `<=` to `to`
    /// @param[in] to stop when the size of the stack is equal to `to`, defaults to `0`
    void clear(size_t to = 0)
    {
      while (m_stack.size() > to)
        m_stack.pop_back();
    }

  protected:
    Stack m_stack;
  };
}  // namespace mtconnect::printer
