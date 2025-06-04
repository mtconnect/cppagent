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

#include "json_mapper.hpp"

#include <boost/algorithm/string.hpp>

#include <chrono>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/reader.h>
#include <regex>
#include <unordered_map>

#include "mtconnect/asset/asset.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/device_model/device.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/entity/xml_parser.hpp"
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/pipeline/timestamp_extractor.hpp"
#include "shdr_tokenizer.hpp"
#include "timestamp_extractor.hpp"
#include "topic_mapper.hpp"
#include "transform.hpp"

using namespace std;
namespace rj = ::rapidjson;

namespace mtconnect::pipeline {
  enum class Expectation
  {
    NONE,
    DATA_SET,
    TIMESTAMP,
    ASSET,
    VALUE_ERROR,

    VALUE,
    KEY,
    OBJECT,
    ROW,
    ARRAY,
    VECTOR
  };

  /// @brief The current context for the parser. Keeps all the interpediary state.
  struct ParserContext
  {
    ParserContext(PipelineContextPtr pipelineContext) : m_pipelineContext(pipelineContext) {}

    using Forward =
        std::function<void(entity::EntityPtr &&entity)>;  //!< Lambda to send a completed entity

    /// @brief clear the current state
    void clear()
    {
      m_device.reset();
      m_timestamp.reset();
      m_duration.reset();
    }

    /// @brief Get the data item for a device
    DataItemPtr getDataItemForDevice(const std::string_view &sv)
    {
      DataItemPtr di;
      DevicePtr device;
      auto keys = splitKey({sv.data(), sv.length()});
      if (keys.second)
        device = m_pipelineContext->m_contract->findDevice(*keys.second);
      if (!device)
        device = getDevice();
      if (!device)
        LOG(warning) << "Cannot find device for data item: " << sv;
      else
        di = device->getDeviceDataItem(keys.first);

      return di;
    }

    DevicePtr getDevice()
    {
      if (m_device)
        return m_device;
      else if (m_defaultDevice)
        return m_defaultDevice;
      else
        return nullptr;
    }

    /// @brief get a device from the agent
    DevicePtr getDevice(const std::string_view &name)
    {
      return m_pipelineContext->m_contract->findDevice({name.data(), name.length()});
    }

    /// @brief set the timestamp
    void setTimestamp(Timestamp &ts, optional<double> &duration)
    {
      m_timestamp.emplace(ts);
      m_duration = duration;
    }

    /// @brief send a complete observation when the data item and props have values.
    void send(DataItemPtr dataItem, entity::Properties &props)
    {
      if (!m_timestamp)
      {
        m_queue.emplace_back(dataItem, props);
      }
      else
      {
        if (m_duration && props.count("duration") == 0)
          props["duration"] = *m_duration;

        entity::ErrorList errors;
        auto obs = observation::Observation::make(dataItem, props, *m_timestamp, errors);
        if (!errors.empty())
        {
          for (auto &e : errors)
          {
            LOG(warning) << "Error while parsing json: " << e->what();
          }

          props.clear();
          props["VALUE"] = "UNAVAILABLE"s;
          if (m_pipelineContext->m_contract->isValidating())
            props["quality"] = "INVALID"s;

          obs = observation::Observation::make(dataItem, props, *m_timestamp, errors);
        }

        if (m_source)
          dataItem->setDataSource(*m_source);
        m_entities.push_back(obs);
        m_forward(std::move(obs));
      }
    }

    /// @brief send an asset.
    void send(asset::AssetPtr asset)
    {
      m_entities.push_back(asset);
      m_forward(std::move(asset));
    }

    void flush()
    {
      if (m_queue.empty())
        return;

      if (!m_timestamp)
        m_timestamp = DefaultNow();

      for (auto &e : m_queue)
      {
        send(e.first, e.second);
      }
      m_queue.clear();
    }

    DevicePtr m_device;
    DevicePtr m_defaultDevice;
    optional<Timestamp> m_timestamp;
    optional<double> m_duration;
    optional<string> m_source;

    EntityList m_entities;
    PipelineContextPtr m_pipelineContext;
    Forward m_forward;
    std::list<pair<DataItemPtr, entity::Properties>> m_queue;
  };

  /// @brief consume value in case of error
  struct ErrorHandler : rj::BaseReaderHandler<rj::UTF8<>, ErrorHandler>
  {
    ErrorHandler(int depth = 0) : m_depth(depth) {}

    bool Default() { return true; }
    bool Key(const Ch *str, rj::SizeType length, bool copy) { return true; }
    bool StartObject()
    {
      m_depth++;
      return true;
    }
    bool EndObject(rj::SizeType memberCount)
    {
      m_depth--;
      return true;
    }
    bool StartArray()
    {
      m_depth++;
      return true;
    }
    bool EndArray(rj::SizeType elementCount)
    {
      m_depth--;
      return true;
    }

    bool operator()(rj::Reader &reader, rj::StringStream &buff)
    {
      LOG(warning) << "Consuming value due to error";

      if (!reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this))
        return false;

      while (m_depth > 0 && !reader.IterativeParseComplete())
      {
        // Read the key
        if (!reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this))
          return false;
      }

      return true;
    }

    int m_depth {0};
  };

  /// @brief SAX Parser handler for JSON Parsing
  struct DataSetHandler : rj::BaseReaderHandler<rj::UTF8<>, DataSetHandler>
  {
    DataSetHandler(entity::DataSet &set, optional<string> key, bool table = false)
      : m_set(set), m_table(table)
    {
      if (key)
      {
        m_expectation = Expectation::VALUE;
        m_entry.m_key = *key;
      }
      else
      {
        m_expectation = Expectation::OBJECT;
      }
    }

    /// handled removed flag
    bool Null()
    {
      if (m_expectation != Expectation::VALUE)
        return false;

      m_entry.m_value.emplace<monostate>();
      m_entry.m_removed = true;
      return true;
    }

    bool Bool(bool b) { return Int64(int(b)); }
    bool Int(int i) { return Int64(i); }
    bool Uint(unsigned i) { return Int64(i); }
    bool Uint64(uint64_t i) { return Int64(i); }
    bool Int64(int64_t i)
    {
      if (m_expectation != Expectation::VALUE)
        return false;

      m_entry.m_value.emplace<int64_t>(i);
      return true;
    }
    bool Double(double d)
    {
      if (m_expectation != Expectation::VALUE)
        return false;

      m_entry.m_value.emplace<double>(d);
      return true;
    }
    bool RawNumber(const Ch *str, rj::SizeType length, bool copy)
    {
      return String(str, length, copy);
    }
    bool String(const Ch *str, rj::SizeType length, bool copy)
    {
      if (m_expectation != Expectation::VALUE)
        return false;

      m_entry.m_value.emplace<std::string>(str, length);
      return true;
    }
    bool StartObject()
    {
      if (m_table)
      {
        if (m_expectation == Expectation::VALUE)
          m_expectation = Expectation::ROW;
      }
      else
      {
        if (m_expectation != Expectation::OBJECT)
          return false;

        m_expectation = Expectation::KEY;
      }
      // Table handler
      return true;
    }
    bool Key(const Ch *str, rj::SizeType length, bool copy)
    {
      // Check for resetTriggered
      m_expectation = Expectation::VALUE;
      m_entry.m_key = string(str, length);
      return true;
    }
    bool EndObject(rj::SizeType memberCount)
    {
      m_done = true;
      return true;
    }
    bool StartArray() { return false; }
    bool EndArray(rj::SizeType elementCount) { return false; }

    bool operator()(rj::Reader &reader, rj::StringStream &buff)
    {
      // Parse initial object
      if (m_expectation == Expectation::OBJECT &&
          !reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this))
        return false;

      while (!reader.IterativeParseComplete() && !m_done)
      {
        // Read the key
        if (m_expectation == Expectation::KEY)
        {
          if (!reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this))
            return false;
          else if (m_done)
            break;
        }

        if (m_expectation == Expectation::OBJECT)
        {
          DataSetHandler handler(m_set, nullopt, m_table);
          auto success = handler(reader, buff);
          if (!success)
            return success;
        }
        else
        {
          // Read the value
          if (!reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this))
            return false;

          if (m_expectation == Expectation::ROW)
          {
            // For tables, recurse down to read the entry data set
            auto &row = m_entry.m_value.emplace<DataSetWrapper>();
            DataSetHandler handler(row, nullopt, false);
            auto success = handler(reader, buff);
            if (!success)
              return success;
            m_expectation = Expectation::VALUE;
          }

          if (m_expectation == Expectation::VALUE)
          {
            auto res = m_set.insert(m_entry);
            if (!res.second)
            {
              m_set.erase(res.first);

              // Try again
              m_set.insert(m_entry);
            }
            m_entry.m_value.emplace<monostate>();
          }
        }

        m_expectation = Expectation::KEY;
      }
      return true;
    }

    entity::DataSet &m_set;
    entity::DataSetEntry m_entry;
    optional<string> m_resetTriggered;
    bool m_table {false};
    bool m_done {false};
    Expectation m_expectation;
  };

  const static map<std::string_view, std::string_view> PropertyMap {
      {"duration", "duration"},     {"resetTriggered", "resetTriggered"},
      {"sampleRate", "sampleRate"}, {"sampleCount", "sampleCount"},
      {"value", "VALUE"},           {"message", "VALUE"}};

  const static map<std::string_view, std::string_view> ConditionMap {
      {"type", "type"},
      {"nativeCode", "nativeCode"},
      {"nativeSeverity", "nativeSeverity"},
      {"qualifier", "qualifier"},
      {"level", "level"},
      {"value", "VALUE"},
      {"message", "VALUE"}};

  /// @brief SAX JSON handler for a simple value or a property object
  /// Differentiates between an object with observation fields and data set based on data item type
  /// and field name.
  struct PropertiesHandler : rj::BaseReaderHandler<rj::UTF8<>, PropertiesHandler>
  {
    PropertiesHandler(DataItemPtr dataItem, entity::Properties &props)
      : m_props(props), m_dataItem(dataItem), m_key("VALUE"), m_expectation(Expectation::VALUE)
    {}

    template <typename T>
    void setValue(T value)
    {
      m_props.insert_or_assign(m_key, value);
    }

    bool Null()
    {
      if (m_key == "VALUE")
        setValue("UNAVAILABLE");
      else
        setValue(nullptr);
      return true;
    }

    bool Bool(bool b)
    {
      setValue(b);
      return true;
    }
    bool Int(int i) { return Int64(i); }
    bool Uint(unsigned i) { return Int64(i); }
    bool Uint64(uint64_t i) { return Int64(i); }
    bool Int64(int64_t i)
    {
      if (m_expectation == Expectation::VECTOR && m_vector != nullptr)
        m_vector->push_back(i);
      else
        setValue(i);
      return true;
    }
    bool Double(double d)
    {
      if (m_expectation == Expectation::VECTOR && m_vector != nullptr)
        m_vector->push_back(d);
      else
        setValue(d);
      return true;
    }
    bool RawNumber(const Ch *str, rj::SizeType length, bool copy)
    {
      return String(str, length, copy);
    }
    bool String(const Ch *str, rj::SizeType length, bool copy)
    {
      setValue(string(str, length));
      return true;
    }

    bool StartArray()
    {
      m_depth++;
      if (m_dataItem->isTimeSeries() || m_dataItem->isThreeSpace())
      {
        auto &value = m_props["VALUE"];
        m_vector = &value.emplace<Vector>();
        m_expectation = Expectation::VECTOR;
        return true;
      }
      else
      {
        LOG(warning) << "Unexpected vector type for data item " << m_dataItem->getId();
        m_expectation = Expectation::VALUE_ERROR;
        return true;
      }
    }
    bool EndArray(rj::SizeType elementCount)
    {
      m_depth--;
      if (m_expectation == Expectation::VECTOR)
      {
        m_vector = nullptr;
        m_expectation = Expectation::KEY;
        return true;
      }
      else
      {
        LOG(warning) << "Unexpected vector type for data item " << m_dataItem->getId();
        m_expectation = Expectation::VALUE_ERROR;
        return true;
      }
    }

    bool Key(const Ch *str, rj::SizeType length, bool copy)
    {
      std::string_view sv(str, length);
      map<std::string_view, std::string_view>::const_iterator f, e;
      if (m_dataItem->isCondition() || m_dataItem->isMessage())
      {
        f = ConditionMap.find(sv);
        e = ConditionMap.end();
        if (f == e)
        {
          LOG(warning) << "Unexpected key: " << sv << " for condition " << m_dataItem->getId();
          m_expectation = Expectation::VALUE_ERROR;
          return true;
        }
      }
      else
      {
        f = PropertyMap.find(sv);
        e = PropertyMap.end();
      }

      if (f != e)
      {
        m_key = f->second;
        m_expectation = Expectation::VALUE;
      }
      else
      {
        if (m_dataItem->isDataSet())
          m_expectation = Expectation::DATA_SET;
        else
        {
          LOG(warning) << "Unexpected key " << sv << " for data item " << m_dataItem->getId();
          m_expectation = Expectation::VALUE_ERROR;
          return true;
        }
        m_key = sv;
      }

      return true;
    }

    bool StartObject()
    {
      m_depth++;
      m_object = true;
      m_expectation = Expectation::KEY;
      return true;
    }

    bool EndObject(rj::SizeType memberCount)
    {
      m_depth--;
      m_done = true;
      return true;
    }

    bool Default()
    {
      LOG(warning) << "Expecting an object with keys and values";
      return false;
    }

    bool operator()(rj::Reader &reader, rj::StringStream &buff)
    {
      while (!reader.IterativeParseComplete() && !m_done)
      {
        if (m_expectation == Expectation::KEY)
        {
          if (!reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this))
            return false;
        }

        if (!m_done)
        {
          if (m_expectation == Expectation::VALUE_ERROR)
          {
            ErrorHandler handler(m_depth);
            if (!handler(reader, buff))
              return false;
            m_props.clear();
            return true;
          }
          else if (m_expectation == Expectation::DATA_SET)
          {
            auto &value = m_props["VALUE"];
            DataSet &set = value.emplace<DataSet>();
            DataSetHandler handler(set, m_key, m_dataItem->isTable());
            if (!handler(reader, buff))
              return false;
            m_expectation = Expectation::KEY;
          }
          else
          {
            if (!reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this))
              return false;
            else if (m_expectation != Expectation::VECTOR)
            {
              if (!m_object)
                break;
              m_expectation = Expectation::KEY;
            }
          }
        }
      }

      if (m_dataItem && m_dataItem->isCondition() && m_props.count("level") == 0)
      {
        m_props.insert_or_assign("level", "normal"s);
      }

      return true;
    }

    entity::Properties &m_props;
    DataItemPtr m_dataItem;
    Vector *m_vector {nullptr};
    bool m_done {false};
    bool m_object {false};
    std::string m_key;
    Expectation m_expectation {Expectation::NONE};
    int m_depth {0};
  };

  struct TimestampHandler : rj::BaseReaderHandler<rj::UTF8<>, TimestampHandler>
  {
    bool Default()
    {
      LOG(warning) << "Expecting a timestamp";
      return false;
    }

    bool String(const Ch *str, rj::SizeType length, bool copy)
    {
      std::string_view sv(str, length);
      std::optional<Timestamp> base;
      Microseconds off;
      auto [timestamp, duration] = ParseTimestamp(sv, false, base, off);
      m_timestamp = timestamp;
      m_duration = duration;
      return true;
    }

    bool operator()(rj::Reader &reader, rj::StringStream &buff)
    {
      auto success = (!reader.IterativeParseComplete() &&
                      reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this));
      return success;
    }

    std::optional<Timestamp> m_timestamp;
    std::optional<double> m_duration;
  };

  struct AssetHandler : rj::BaseReaderHandler<rj::UTF8<>, TimestampHandler>
  {
    AssetHandler(ParserContext &context) : m_context(context) {}
    bool Default()
    {
      LOG(warning) << "Expecting an asset";
      return false;
    }

    bool Key(const Ch *str, rj::SizeType length, bool copy)
    {
      m_assetId = string(str, length);
      m_expectation = Expectation::ASSET;
      return true;
    }

    bool String(const Ch *str, rj::SizeType length, bool copy)
    {
      using namespace mtconnect::asset;
      ErrorList errors;

      std::string_view sv(str, length);
      XmlParser parser;
      auto res = parser.parse(Asset::getRoot(), string(sv), errors);
      if (!errors.empty())
      {
        LOG(warning) << "Errors while parsing json asset: ";
        for (const auto &e : errors)
        {
          LOG(warning) << "  " << e->what();
        }
        return false;
      }

      if (auto asset = dynamic_pointer_cast<Asset>(res))
      {
        asset->setAssetId(m_assetId);
        Timestamp timestamp;
        if (m_context.m_timestamp)
          timestamp = *m_context.m_timestamp;
        else
          timestamp = DefaultNow();
        asset->setProperty("timestamp", timestamp);
        auto device = m_context.getDevice();
        if (device)
        {
          asset->setProperty("deviceUuid", *device->getUuid());
        }

        m_context.send(asset);
      }

      return true;
    }

    bool StartObject() { return true; }

    bool EndObject(rj::SizeType memberCount)
    {
      m_done = true;
      return true;
    }

    bool operator()(rj::Reader &reader, rj::StringStream &buff)
    {
      // Consume start object
      if (m_expectation == Expectation::OBJECT)
      {
        if (!reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this))
          return false;
      }

      m_expectation = Expectation::KEY;

      while (!m_done && !reader.IterativeParseComplete())
      {
        // Consume the key
        if (!reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this))
          return false;

        if (!m_done)
        {
          if (m_expectation == Expectation::ASSET)
          {
            // Consume the value
            if (!reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this))
              return false;
          }
          else
          {
            LOG(warning) << "Asset handler in wrong state, expecting asset";
            return false;
          }
        }
      }

      return true;
    }

    ParserContext &m_context;
    Expectation m_expectation {Expectation::OBJECT};
    string m_assetId;
    bool m_done {false};
  };

  struct ObjectHandler : rj::BaseReaderHandler<rj::UTF8<>, ObjectHandler>
  {
    ObjectHandler(ParserContext &context) : m_context(context) {}
    bool Default()
    {
      LOG(warning) << "Expecting a key";
      m_expectation = Expectation::VALUE_ERROR;
      return true;
    }

    bool Key(const Ch *str, rj::SizeType length, bool copy)
    {
      std::string_view sv(str, length);
      if (sv == "timestamp")
        m_expectation = Expectation::TIMESTAMP;
      else if (sv == "asset" || sv == "assets")
        m_expectation = Expectation::ASSET;
      else
      {
        auto device = m_context.getDevice(sv);
        if (device)
        {
          m_context.m_device = device;
          m_expectation = Expectation::OBJECT;
        }
        else
        {
          m_dataItem = m_context.getDataItemForDevice(sv);
          if (!m_dataItem)
          {
            LOG(warning) << "Cannot find data item for " << sv;
          }
          m_expectation = Expectation::VALUE;
        }
      }
      return true;
    }

    bool StartObject()
    {
      m_expectation = Expectation::KEY;
      return true;
    }

    bool EndObject(rj::SizeType memberCount)
    {
      m_complete = true;
      return true;
    }

    bool EndArray(rj::SizeType memberCount)
    {
      m_complete = true;
      return true;
    }

    bool operator()(rj::Reader &reader, rj::StringStream &buff)
    {
      while (!m_complete && !reader.IterativeParseComplete())
      {
        // Consume the key
        if (m_expectation == Expectation::KEY)
        {
          if (!reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this))
            return false;
        }

        if (!m_complete)
        {
          // Based on the key type, consume the value
          switch (m_expectation)
          {
            case Expectation::TIMESTAMP:
            {
              TimestampHandler handler;
              if (!handler(reader, buff))
                return false;
              m_timestamp = handler.m_timestamp;
              m_duration = handler.m_duration;
              if (m_timestamp)
                m_context.setTimestamp(*m_timestamp, m_duration);

              m_expectation = Expectation::KEY;
              break;
            }

            case Expectation::ASSET:
            {
              AssetHandler handler(m_context);
              if (!handler(reader, buff))
                return false;
              m_expectation = Expectation::KEY;
              break;
            }

            case Expectation::OBJECT:
            {
              ObjectHandler handler(m_context);
              if (m_timestamp)
                m_context.setTimestamp(*m_timestamp, m_duration);
              if (!handler(reader, buff))
                return false;
              m_context.m_device.reset();
              m_expectation = Expectation::KEY;
              break;
            }

            case Expectation::VALUE_ERROR:
            {
              ErrorHandler handler;
              if (!handler(reader, buff))
                return false;

              m_expectation = Expectation::KEY;
              break;
            }

            case Expectation::KEY:
            case Expectation::NONE:
              break;

            default:  // Data Item key with value
            {
              // If we have a data item, map the properties
              if (m_dataItem)
              {
                PropertiesHandler props(m_dataItem, m_props);
                if (!props(reader, buff))
                  return false;
                if (m_props.size() > 0)
                  m_context.send(m_dataItem, m_props);
              }
              else
              {
                LOG(warning) << "Cannot map properties without data item, consuming erronous data";
                ErrorHandler handler;
                if (!handler(reader, buff))
                  return false;
              }
              m_props.clear();
              m_dataItem.reset();
              m_expectation = Expectation::KEY;
              break;
            }
          }
        }
      }

      m_context.flush();
      m_context.clear();

      return true;
    }

    optional<Timestamp> m_timestamp;
    optional<double> m_duration;
    ParserContext &m_context;
    bool m_complete {false};
    Expectation m_expectation {Expectation::KEY};
    DataItemPtr m_dataItem;
    entity::Properties m_props;
  };

  struct ArrayHandler : rj::BaseReaderHandler<rj::UTF8<>, ArrayHandler>
  {
    ArrayHandler(ParserContext &context) : m_context(context) {}
    bool Default()
    {
      LOG(warning) << "Expecting an array of objects";
      return false;
    }
    bool StartObject() { return true; }
    bool EndObject(rj::SizeType memberCount) { return true; }
    bool EndArray(rj::SizeType memberCount)
    {
      m_complete = true;
      return true;
    }

    bool operator()(rj::Reader &reader, rj::StringStream &buff)
    {
      while (!reader.IterativeParseComplete() && !m_complete)
      {
        if (!reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this))
          return false;

        if (m_expectation == Expectation::OBJECT)
        {
          ObjectHandler handler(m_context);
          handler(reader, buff);
        }
        else
        {
          LOG(warning) << "Only objects allowed as members of top level array";
          return false;
        }
      }

      return true;
    }

    Expectation m_expectation {Expectation::OBJECT};
    ParserContext &m_context;
    bool m_complete {false};
  };

  struct TopLevelHandler : rj::BaseReaderHandler<rj::UTF8<>, TopLevelHandler>
  {
    TopLevelHandler(ParserContext &context) : m_context(context) {}
    bool Default()
    {
      LOG(warning) << "Top level can only be an object or array";
      return false;
    }

    bool StartObject()
    {
      m_expectation = Expectation::OBJECT;
      return true;
    }
    bool EndObject(rj::SizeType memberCount) { return true; }
    bool StartArray()
    {
      m_expectation = Expectation::ARRAY;
      return true;
    }
    bool EndArray(rj::SizeType elementCount) { return true; }

    bool operator()(rj::Reader &reader, rj::StringStream &buff)
    {
      while (!reader.IterativeParseComplete())
      {
        if (!reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this))
          return false;
        if (m_expectation == Expectation::OBJECT)
        {
          ObjectHandler handler(m_context);
          handler(reader, buff);
        }
        else if (m_expectation == Expectation::ARRAY)
        {
          ArrayHandler handler(m_context);
          handler(reader, buff);
        }
        else
        {
          LOG(warning) << "Invalid state not object or array";
        }
      }

      return true;
    }

    Expectation m_expectation {Expectation::NONE};
    ParserContext &m_context;
  };

  /// @brief Use rapidjson to parse the json content. If there is an error, output the text and
  /// log the error.
  EntityPtr JsonMapper::operator()(entity::EntityPtr &&entity)
  {
    static const auto GetParseError = rj::GetParseError_En;

    auto source = entity->maybeGet<string>("source");
    auto json = std::dynamic_pointer_cast<JsonMessage>(entity);
    DevicePtr device = json->m_device.lock();
    auto &body = entity->getValue<std::string>();

    rj::StringStream buff(body.c_str());
    rj::Reader reader;
    reader.IterativeParseInit();
    ParserContext context(m_context);
    context.m_forward = [this](entity::EntityPtr &&entity) { next(std::move(entity)); };
    context.m_source = source;

    TopLevelHandler handler(context);
    if (device)
      handler.m_context.m_defaultDevice = device;
    handler(reader, buff);

    EntityPtr res;
    if (reader.HasParseError())
    {
      LOG(error) << "Error parsing json: " << body;
      LOG(error) << "Error code: " << GetParseError(reader.GetParseErrorCode()) << " at "
                 << reader.GetErrorOffset();
    }
    else
    {
      res = std::make_shared<Entity>("JsonEntities");
      res->setValue(handler.m_context.m_entities);
    }
    return res;
  }

}  // namespace mtconnect::pipeline
