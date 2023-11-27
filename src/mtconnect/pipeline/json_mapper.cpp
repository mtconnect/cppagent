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
#include "mtconnect/observation/observation.hpp"
#include "mtconnect/pipeline/timestamp_extractor.hpp"
#include "shdr_tokenizer.hpp"
#include "timestamp_extractor.hpp"
#include "topic_mapper.hpp"
#include "transform.hpp"

using namespace std;
namespace rj = ::rapidjson;

namespace mtconnect::pipeline {
  enum class KeyToken
  {
    NONE,
    DATA_ITEM,
    TIMESTAMP,
    DEVICE,
    DURATION,
    RESET_TRIGGER,
    VALUE,
    DATA_ITEM_VALUE,
    SAMPLE_RATE,
    ASSET
  };

  enum class ParserState
  {
    NONE,
    ARRAY,
    OBJECT,
  };
  
  using Expectation = bitset<16>;

  /// @brief The current context for the parser. Keeps all the interpediary state.
  struct ParserContext
  {
    ParserContext(PipelineContextPtr pipelineContext) : m_pipelineContext(pipelineContext) {}

    using Forward =
        std::function<void(entity::EntityPtr &&entity)>;  //!< Lambda to send a completed entity

    /// @brief clear the current state
    void clear()
    {
      m_props.clear();
      m_dataItem.reset();
      m_device.reset();
      m_defaultDevice.reset();
      m_asset.reset();
      m_entities.clear();
      m_token = KeyToken::NONE;
      m_state = ParserState::NONE;

      m_device = m_defaultDevice;
    }

    /// @brief Get the data item for a device
    void getDataItemForDevice(const std::string_view &sv)
    {
      if (!m_device)
      {
        LOG(warning) << "JsonMapper: Cannot find data item without Device";
      }
      else
      {
        m_dataItem = m_device->getDeviceDataItem({sv.data(), sv.length()});
      }
    }

    /// @brief send a complete observation when the data item and props have values.
    void send()
    {
      if (m_dataItem && !holds_alternative<monostate>(m_value))
      {
        if (!m_timestamp)
          m_timestamp = DefaultNow();
        if (m_duration)
          m_props["duration"] = *m_duration;

        m_props["VALUE"] = m_value;

        entity::ErrorList errors;
        auto obs = observation::Observation::make(m_dataItem, m_props, *m_timestamp, errors);
        if (!errors.empty())
        {
          for (auto &e : errors)
          {
            LOG(warning) << "Error while parsing json: " << e->what();
          }
        }
        else
        {
          m_entities.push_back(obs);
          m_forward(std::move(obs));
        }
      }
      else
      {
        LOG(warning) << "Incomplete observation";
      }

      m_props.clear();
      m_dataItem.reset();
    }

    entity::Properties m_props;
    ParserState m_state {ParserState::NONE};
    KeyToken m_token {KeyToken::NONE};
    std::optional<Timestamp> m_timestamp;
    std::optional<double> m_duration;
    DataItemPtr m_dataItem;
    DevicePtr m_device;
    DevicePtr m_defaultDevice;
    asset::AssetPtr m_asset;

    entity::Value m_value;

    EntityList m_entities;
    PipelineContextPtr m_pipelineContext;
    Forward m_forward;
  };

  struct VectorHandler : rj::BaseReaderHandler<rj::UTF8<>, VectorHandler>
  {
    VectorHandler(entity::Vector &vector) : m_value(vector) {}

    bool Null() { return true; }

    bool Int(int i) { return Double(i); }
    bool Uint(unsigned i) { return Double(i); }
    bool Int64(int64_t i) { return Double(i); }
    bool Uint64(uint64_t i) { return Double(i); }

    bool Double(double d)
    {
      m_value.push_back(d);
      return true;
    }

    bool Default()
    {
      LOG(warning) << "Invalid vector value, expected only doubles";
      return false;
    }

    bool EndArray(rj::SizeType count)
    {
      m_done = true;
      if (m_value.size() != count)
      {
        LOG(warning) << "Invalid array of values, size: " << m_value.size()
                     << ", expected: " << count;
        return false;
      }
      return true;
    }

    bool operator()(rj::Reader &reader, rj::StringStream &buff)
    {
      while (!reader.IterativeParseComplete() && !m_done)
      {
        if (!reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this))
        {
          LOG(warning) << "Cannot parse double vector";
          m_value.clear();
          return false;
        }
      }

      return true;
    }

    entity::Vector &m_value;
    bool m_done {false};
  };
  
  /// @brief SAX Parser handler for JSON Parsing
  struct DataSetHandler : rj::BaseReaderHandler<rj::UTF8<>, DataSetHandler>
  {
    DataSetHandler(entity::DataSet &set, bool table = false)
    : m_set(set), m_table(table)
    {
    }
    
    /// handled removed flag
    bool Null()
    {
      if (m_token != KeyToken::NONE)
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
      if (m_token != KeyToken::NONE)
        return false;

      m_entry.m_value.emplace<int64_t>(i);
      return true;
    }
    bool Double(double d)
    {
      if (m_token != KeyToken::NONE)
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
      if (m_token == KeyToken::VALUE)
        return false;
      
      m_entry.m_value.emplace<std::string>(str, length);
      return true;
    }
    bool StartObject()
    {
      if (m_token == KeyToken::RESET_TRIGGER)
        return false;

      // Table handler
      return true;
    }
    bool Key(const Ch *str, rj::SizeType length, bool copy) 
    {
      // Check for resetTriggered
      string_view key(str, length);
      if (key == "resetTriggered" )
        m_token = KeyToken::RESET_TRIGGER;
      else if (key == "value")
        m_token = KeyToken::VALUE;
      else
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
      while (!reader.IterativeParseComplete() && !m_done)
      {
        if (!reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this))
          return false;
        else if (m_done)
          break;
        
        if (m_token == KeyToken::RESET_TRIGGER && !holds_alternative<monostate>(m_entry.m_value))
        {
          m_resetTriggered.emplace(get<string>(m_entry.m_value));
        }
        else if (m_token == KeyToken::VALUE)
        {
          DataSetHandler handler(m_set, m_table);
          auto success = handler(reader, buff);
          if (!success)
            return success;
        }
        else if (m_token == KeyToken::NONE && !holds_alternative<monostate>(m_entry.m_value))
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
      return true;
    }

    entity::DataSet &m_set;
    entity::DataSetEntry m_entry;
    optional<string> m_resetTriggered;
    bool m_table { false };
    bool m_done { false };
    bool m_hasValue { false };
    KeyToken m_token { KeyToken::NONE };
  };

  /// @brief SAX Parser handler for JSON Parsing
  struct ValueHandler : rj::BaseReaderHandler<rj::UTF8<>, ValueHandler>
  {
    bool Null()
    {
      m_value.emplace<nullptr_t>();
      return true;
    }

    bool Bool(bool b)
    {
      m_value.emplace<bool>(b);
      return true;
    }
    bool Int(int i) { return Int64(i); }
    bool Uint(unsigned i) { return Int64(i); }
    bool Uint64(uint64_t i) { return Int64(i); }
    bool Int64(int64_t i)
    {
      m_value.emplace<int64_t>(i);
      return true;
    }
    bool Double(double d)
    {
      m_value.emplace<double>(d);
      return true;
    }
    bool RawNumber(const Ch *str, rj::SizeType length, bool copy)
    {
      return String(str, length, copy);
    }
    bool String(const Ch *str, rj::SizeType length, bool copy)
    {
      m_value.emplace<std::string>(str, length);
      return true;
    }
    bool StartObject()
    {
      // We use an entity pointer to signal we have a property set.
      // The PropertiesHandler will map each of the properties in the object
      // to the properties.
      m_value.emplace<EntityPtr>();
      return true;
    }
    bool Key(const Ch *str, rj::SizeType length, bool copy) { return false; }
    bool EndObject(rj::SizeType memberCount) { return true; }
    bool StartArray()
    {
      m_value.emplace<Vector>();
      return true;
    }
    bool EndArray(rj::SizeType elementCount) { return false; }

    entity::Value operator()(rj::Reader &reader, rj::StringStream &buff)
    {
      auto success = (!reader.IterativeParseComplete() &&
                      reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this));
      if (!success)
      {
        LOG(warning) << "Cannot get value for json mapper";
        m_value = monostate();
      }
      else if (holds_alternative<Vector>(m_value))
      {
        VectorHandler handler(get<Vector>(m_value));
        handler(reader, buff);
      }
      return m_value;
    }

    entity::Value m_value {std::monostate()};
    bool m_done {true};
  };

  const static map<std::string_view, std::string_view> PropertyMap {{"value", "VALUE"},
                                                                    {"message", "VALUE"}};

  struct PropertiesHandler : rj::BaseReaderHandler<rj::UTF8<>, PropertiesHandler>
  {
    PropertiesHandler(entity::Properties &props) : m_props(props) {}

    bool Default()
    {
      LOG(warning) << "Expecting an object with keys and values";
      return false;
    }

    bool Key(const Ch *str, rj::SizeType length, bool copy)
    {
      std::string_view sv(str, length);
      auto f = PropertyMap.find(sv);
      if (f != PropertyMap.end())
      {
        m_key = f->second;
      }
      else
      {
        m_key = sv;
      }

      return true;
    }

    bool StartObject()
    {
      LOG(warning) << "Expecting not expecting sub-objects";
      return false;
    }

    bool EndObject(rj::SizeType memberCount)
    {
      m_done = true;
      return true;
    }

    bool operator()(rj::Reader &reader, rj::StringStream &buff)
    {
      while (!reader.IterativeParseComplete() && !m_done)
      {
        if (!reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this))
          return false;
        else if (m_done)
          break;

        ValueHandler handler;
        auto value = handler(reader, buff);
        if (!std::holds_alternative<std::monostate>(value) && !m_key.empty())
        {
          m_props.insert_or_assign(m_key, value);
        }
        else
        {
          LOG(warning) << "Could not map " << m_key;
        }
      }

      return true;
    }

    entity::Properties &m_props;
    bool m_done {false};
    std::string m_key;
  };

  struct DataItemHandler : rj::BaseReaderHandler<rj::UTF8<>, DataItemHandler>
  {
    DataItemHandler(ParserContext &context) : m_context(context) {}

    bool Default()
    {
      LOG(warning) << "Expecting a data item string value";
      return false;
    }

    bool String(const Ch *str, rj::SizeType length, bool copy)
    {
      if (m_context.m_device)
      {
        auto di = m_context.m_device->getDeviceDataItem({str, length});
        if (!di)
        {
          LOG(warning) << "JsonMapper: cannot find data item for " << str;
          return false;
        }
        return true;
      }
      else
      {
        LOG(warning) << "JsonMapper: value without data item";
        return false;
      }
    }

    bool operator()(rj::Reader &reader, rj::StringStream &buff)
    {
      auto success = (!reader.IterativeParseComplete() &&
                      reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this));
      return success;
    }

    ParserContext &m_context;
  };

  struct DeviceHandler : rj::BaseReaderHandler<rj::UTF8<>, DeviceHandler>
  {
    DeviceHandler(ParserContext &context) : m_context(context) {}

    bool Default()
    {
      LOG(warning) << "Expecting a device string value";
      return false;
    }

    bool String(const Ch *str, rj::SizeType length, bool copy)
    {
      m_context.m_device = m_context.m_pipelineContext->m_contract->findDevice({str, length});
      if (!m_context.m_device)
      {
        LOG(warning) << "JsonMapper: Cannot find device '" << str;
        return false;
      }
      else
      {
        return true;
      }
    }

    bool operator()(rj::Reader &reader, rj::StringStream &buff)
    {
      auto success = (!reader.IterativeParseComplete() &&
                      reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this));
      return success;
    }

    ParserContext &m_context;
  };

  struct TimestampHandler : rj::BaseReaderHandler<rj::UTF8<>, TimestampHandler>
  {
    TimestampHandler(ParserContext &context) : m_context(context) {}

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
      m_context.m_timestamp = timestamp;
      m_context.m_duration = duration;
      return true;
    }

    bool operator()(rj::Reader &reader, rj::StringStream &buff)
    {
      auto success = (!reader.IterativeParseComplete() &&
                      reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this));
      return success;
    }

    ParserContext &m_context;
  };

  const static map<std::string_view, KeyToken> FieldMap {{"timestamp", KeyToken::TIMESTAMP},
                                                         {"dataItem", KeyToken::DATA_ITEM},
                                                         {"device", KeyToken::DEVICE},
                                                         {"duration", KeyToken::DURATION},
                                                         {"resetTrigger", KeyToken::RESET_TRIGGER},
                                                         {"sampleRate", KeyToken::SAMPLE_RATE},
                                                         {"value", KeyToken::VALUE},
                                                         {"asset", KeyToken::ASSET}};

  struct ObjectHandler : rj::BaseReaderHandler<rj::UTF8<>, ObjectHandler>
  {
    ObjectHandler(ParserContext &context) : m_context(context) {}
    bool Default()
    {
      LOG(warning) << "Expecting a key";
      return false;
    }

    bool Key(const Ch *str, rj::SizeType length, bool copy)
    {
      std::string_view sv(str, length);
      auto f = FieldMap.find(sv);
      if (f != FieldMap.end())
      {
        m_context.m_token = f->second;
      }
      else
      {
        m_context.m_token = KeyToken::DATA_ITEM_VALUE;
        m_context.getDataItemForDevice(sv);
      }
      return true;
    }

    bool EndObject(rj::SizeType memberCount)
    {
      m_complete = true;
      m_context.m_token = KeyToken::NONE;
      return true;
    }

    bool operator()(rj::Reader &reader, rj::StringStream &buff)
    {
      while (!m_complete && !reader.IterativeParseComplete())
      {
        if (!reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this))
          return false;

        switch (m_context.m_token)
        {
          case KeyToken::TIMESTAMP:
          {
            TimestampHandler handler(m_context);
            if (!handler(reader, buff))
              return false;
            break;
          }
          case KeyToken::DEVICE:
          {
            DeviceHandler handler(m_context);
            if (!handler(reader, buff))
              return false;
            break;
          }
          case KeyToken::DATA_ITEM:
          {
            DataItemHandler handler(m_context);
            if (!handler(reader, buff))
              return false;
            break;
          }
          case KeyToken::NONE:
            break;

          default: // Data Item key with value
          {
            if (m_context.m_dataItem->isDataSet())
            {
              auto &res = m_context.m_value.emplace<DataSet>();
              
              DataSetHandler handler(res, m_context.m_dataItem->isTable());
              if (!handler(reader, buff))
                return false;
            }
            else
            {
              ValueHandler handler;
              auto value = handler(reader, buff);
              if (std::holds_alternative<std::monostate>(value))
                return false;
              else if (std::holds_alternative<EntityPtr>(value))
              {
                PropertiesHandler props(m_context.m_props);
                if (!props(reader, buff))
                  return false;
                auto value = m_context.m_props.find("VALUE");
                if (value == m_context.m_props.end() && holds_alternative<monostate>(value->second))
                {
                  LOG(warning) << "value was not given for data item "
                  << m_context.m_dataItem->getId();
                  return false;
                }
                m_context.m_value = value->second;
                m_context.m_props.erase(value);
              }
              else
              {
                m_context.m_value = value;
              }
              m_context.send();
            }
            break;
         }
        }
      }

      return true;
    }

    ParserContext &m_context;
    bool m_complete {false};
  };

  struct ArrayHandler : rj::BaseReaderHandler<rj::UTF8<>, ArrayHandler>
  {
    ArrayHandler(ParserContext &context) : m_context(context) {}
    bool Default()
    {
      LOG(warning) << "Expecting an array of objects";
      return false;
    }
    bool StartObject()
    {
      m_context.m_state = ParserState::OBJECT;
      return true;
    }
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

        if (m_context.m_state == ParserState::OBJECT)
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
      m_state = ParserState::OBJECT;
      return true;
    }
    bool EndObject(rj::SizeType memberCount) { return true; }
    bool StartArray()
    {
      m_state = ParserState::ARRAY;
      return true;
    }
    bool EndArray(rj::SizeType elementCount) { return true; }

    bool operator()(rj::Reader &reader, rj::StringStream &buff)
    {
      while (!reader.IterativeParseComplete())
      {
        if (!reader.IterativeParseNext<rj::kParseNanAndInfFlag>(buff, *this))
          return false;
        if (m_state == ParserState::OBJECT)
        {
          ObjectHandler handler(m_context);
          handler(reader, buff);
        }
        else if (m_state == ParserState::ARRAY)
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

    ParserState m_state {ParserState::NONE};
    ParserContext &m_context;
  };

  /// @brief Use rapidjson to parse the json content. If there is an error, output the text and
  /// log the error.
  EntityPtr JsonMapper::operator()(entity::EntityPtr &&entity)
  {
    static const auto GetParseError = rj::GetParseError_En;

    auto json = std::dynamic_pointer_cast<JsonMessage>(entity);
    DevicePtr device = json->m_device.lock();
    auto &body = entity->getValue<std::string>();

    rj::StringStream buff(body.c_str());
    rj::Reader reader;
    reader.IterativeParseInit();
    ParserContext context(m_context);
    context.m_forward = [this](entity::EntityPtr &&entity) { next(std::move(entity)); };

    TopLevelHandler handler(context);
    if (device)
      handler.m_context.m_device = device;
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
