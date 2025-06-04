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

#include <mruby-bigint/core/bigint.h>
#include <mruby-time/include/mruby/time.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/value.h>

#include "mtconnect/config.hpp"
#include "mtconnect/device_model/data_item/data_item.hpp"
#include "mtconnect/device_model/device.hpp"
#include "mtconnect/entity/data_set.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/pipeline/shdr_tokenizer.hpp"
#include "mtconnect/pipeline/timestamp_extractor.hpp"
#include "ruby_smart_ptr.hpp"
#include "ruby_type.hpp"

namespace mtconnect::ruby {
  using namespace mtconnect;
  using namespace device_model;
  using namespace data_item;
  using namespace entity;
  using namespace std;

  /// @brief Convert a table cell value to a ruby hash element. Recursive in the case of tables.
  /// @param[in] mrb the mruby state
  /// @param[in] value the data set value
  /// @returns an mruby value
  inline mrb_value toRuby(mrb_state *mrb, const TableCellValue &value)
  {
    mrb_value rv;

    rv = visit(overloaded {[](const std::monostate &v) -> mrb_value { return mrb_nil_value(); },
                           [mrb](const std::string &v) -> mrb_value {
                             return mrb_str_new_cstr(mrb, v.c_str());
                           },
                           [mrb](const int64_t v) -> mrb_value { return mrb_int_value(mrb, v); },
                           [mrb](const double v) -> mrb_value { return mrb_float_value(mrb, v); }},
               value);

    return rv;
  }

  /// @brief Convert data set to ruby hash
  ///
  /// Recurses for tables.
  ///
  /// @param[in] mrb the mruby state
  /// @param[in] value the data set
  /// @returns an mruby value
  inline mrb_value toRuby(mrb_state *mrb, const TableRow &set)
  {
    mrb_value hash = mrb_hash_new(mrb);
    for (const auto &entry : set)
    {
      const auto &value = (entry.m_value);

      mrb_sym k = mrb_intern_cstr(mrb, entry.m_key.c_str());
      mrb_value v = toRuby(mrb, value);

      mrb_hash_set(mrb, hash, mrb_symbol_value(k), v);
    }

    return hash;
  }

  /// @brief Convert a data set value to a ruby hash element. Recursive in the case of tables.
  /// @param[in] mrb the mruby state
  /// @param[in] value the data set value
  /// @returns an mruby value
  inline mrb_value toRuby(mrb_state *mrb, const DataSetValue &value)
  {
    mrb_value rv;

    rv = visit(overloaded {[](const std::monostate &v) -> mrb_value { return mrb_nil_value(); },
                           [mrb](const std::string &v) -> mrb_value {
                             return mrb_str_new_cstr(mrb, v.c_str());
                           },
                           [mrb](const entity::TableRow &v) -> mrb_value { return toRuby(mrb, v); },
                           [mrb](const int64_t v) -> mrb_value { return mrb_int_value(mrb, v); },
                           [mrb](const double v) -> mrb_value { return mrb_float_value(mrb, v); }},
               value);

    return rv;
  }

  /// @brief Convert data set to ruby hash
  ///
  /// Recurses for tables.
  ///
  /// @param[in] mrb the mruby state
  /// @param[in] value the data set
  /// @returns an mruby value
  inline mrb_value toRuby(mrb_state *mrb, const DataSet &set)
  {
    mrb_value hash = mrb_hash_new(mrb);
    for (const auto &entry : set)
    {
      const auto &value = (entry.m_value);

      mrb_sym k = mrb_intern_cstr(mrb, entry.m_key.c_str());
      mrb_value v = toRuby(mrb, value);

      mrb_hash_set(mrb, hash, mrb_symbol_value(k), v);
    }

    return hash;
  }

  /// @brief Convert a Ruby hash value to an MTConect DataSet
  /// @param[in] mrb mruby state
  /// @param[in] value the hash value to convert
  /// @returns true if succesful
  inline bool tableRowCellValueFromRuby(mrb_state *mrb, mrb_value value, TableCellValue &tcv)
  {
    bool res = true;
    switch (mrb_type(value))
    {
      case MRB_TT_SYMBOL:
      case MRB_TT_STRING:
        tcv.emplace<string>(stringFromRuby(mrb, value));
        break;

      case MRB_TT_FIXNUM:
        tcv.emplace<int64_t>(mrb_as_int(mrb, value));
        break;

      case MRB_TT_FLOAT:
        tcv.emplace<double>(mrb_as_float(mrb, value));
        break;

      default:
      {
        LOG(warning) << "DataSet cannot conver type: "
                     << stringFromRuby(mrb, mrb_inspect(mrb, value));
        res = false;
        break;
      }
    }
    return res;
  }

  /// @brief convert a ruby hash table to a table row
  inline void tableRowFromRuby(mrb_state *mrb, mrb_value value, TableRow &row)
  {
    auto hash = mrb_hash_ptr(value);
    mrb_hash_foreach(
        mrb, hash,
        [](mrb_state *mrb, mrb_value key, mrb_value val, void *data) {
          TableRow *row = static_cast<TableRow *>(data);
          string k = stringFromRuby(mrb, key);
          TableCellValue tcv;
          if (tableRowCellValueFromRuby(mrb, val, tcv))
            row->emplace(k, tcv);

          return 0;
        },
        &row);
  }

  /// @brief Convert a Ruby hash value to an MTConect DataSet
  /// @param[in] mrb mruby state
  /// @param[in] value the hash value to convert
  /// @returns true if succesful
  inline bool dataSetValueFromRuby(mrb_state *mrb, mrb_value value, DataSetValue &dsv)
  {
    bool res = true;
    switch (mrb_type(value))
    {
      case MRB_TT_SYMBOL:
      case MRB_TT_STRING:
        dsv.emplace<string>(stringFromRuby(mrb, value));
        break;

      case MRB_TT_FIXNUM:
        dsv.emplace<int64_t>(mrb_as_int(mrb, value));
        break;

      case MRB_TT_FLOAT:
        dsv.emplace<double>(mrb_as_float(mrb, value));
        break;

      case MRB_TT_HASH:
      {
        TableRow inner;
        tableRowFromRuby(mrb, value, inner);
        dsv.emplace<entity::TableRow>(inner);
        break;
      }

      default:
      {
        LOG(warning) << "DataSet cannot conver type: "
                     << stringFromRuby(mrb, mrb_inspect(mrb, value));
        res = false;
        break;
      }
    }
    return res;
  }

  /// @brief Convert a Ruby hash  to an MTConnect DataSet
  /// @param[in] mrb mruby state
  /// @param[in] value the hash value to convert
  /// @param[out] dataSet the data set to populate
  inline void dataSetFromRuby(mrb_state *mrb, mrb_value value, DataSet &dataSet)
  {
    auto hash = mrb_hash_ptr(value);
    mrb_hash_foreach(
        mrb, hash,
        [](mrb_state *mrb, mrb_value key, mrb_value val, void *data) {
          DataSet *dataSet = static_cast<DataSet *>(data);
          string k = stringFromRuby(mrb, key);
          DataSetValue dsv;
          if (dataSetValueFromRuby(mrb, val, dsv))
            dataSet->emplace(k, dsv);

          return 0;
        },
        &dataSet);
  }

  /// @brief Translate an mruby type to a Entity property
  /// @param[in] mrb mruby state
  /// @param[in] value  the mruby typed value
  /// @returns an Entity Value
  inline Value valueFromRuby(mrb_state *mrb, mrb_value value)
  {
    Value res;

    if (mrb_nil_p(value))
    {
      res.emplace<std::nullptr_t>();
      return res;
    }

    switch (mrb_type(value))
    {
      case MRB_TT_UNDEF:
        res.emplace<std::monostate>();
        break;

      case MRB_TT_STRING:
        res.emplace<string>(mrb_str_to_cstr(mrb, value));
        break;

      case MRB_TT_BIGINT:
      case MRB_TT_FIXNUM:
        res.emplace<int64_t>(mrb_as_int(mrb, value));
        break;

      case MRB_TT_FLOAT:
        res.emplace<double>(mrb_as_float(mrb, value));
        break;

      case MRB_TT_TRUE:
        res.emplace<bool>(true);
        break;

      case MRB_TT_FALSE:
        res.emplace<bool>(false);
        break;

      case MRB_TT_HASH:
      {
        DataSet ds;
        dataSetFromRuby(mrb, value, ds);
        res.emplace<DataSet>(ds);
        break;
      }

      case MRB_TT_ARRAY:
      {
        auto ary = mrb_ary_ptr(value);
        auto size = ARY_LEN(ary);
        auto values = ARY_PTR(ary);

        if (mrb_type(values[0]) == MRB_TT_FIXNUM || mrb_type(values[0]) == MRB_TT_FLOAT)
        {
          res.emplace<Vector>();
          Vector &out = get<Vector>(res);

          for (int i = 0; i < size; i++)
          {
            mrb_value &v = values[i];
            auto t = mrb_type(v);
            if (t == MRB_TT_FIXNUM)
              out.emplace_back((double)mrb_integer(v));
            else if (t == MRB_TT_FLOAT)
              out.emplace_back(mrb_float(v));
            else
            {
              auto in = mrb_inspect(mrb, value);
              LOG(warning) << "Invalid type for array: " << mrb_str_to_cstr(mrb, in);
            }
          }
        }
        else
        {
          auto mod = mrb_module_get(mrb, "MTConnect");
          auto klass = mrb_class_get_under(mrb, mod, "Entity");

          res.emplace<EntityList>();
          EntityList &list = get<EntityList>(res);
          for (int i = 0; i < size; i++)
          {
            mrb_value &v = values[i];
            if (mrb_type(v) == MRB_TT_DATA)
            {
              if (mrb_obj_is_kind_of(mrb, value, klass))
              {
                auto ent = MRubySharedPtr<Entity>::unwrap(mrb, value);
                list.emplace_back(ent);
              }
            }
          }
        }
        break;
      }

      case MRB_TT_DATA:
      case MRB_TT_OBJECT:
      {
        string kn(mrb_obj_classname(mrb, value));
        // Convert time
        if (kn == "Time")
        {
          res.emplace<Timestamp>(timestampFromRuby(mrb, value));
        }
        else
        {
          auto mod = mrb_module_get(mrb, "MTConnect");
          auto klass = mrb_class_get_under(mrb, mod, "Entity");
          if (mrb_obj_is_kind_of(mrb, value, klass))
          {
            auto ent = MRubySharedPtr<Entity>::unwrap(mrb, value);
            res.emplace<EntityPtr>(ent);
          }
        }
        break;
      }

      default:
      {
        auto in = mrb_inspect(mrb, value);
        LOG(warning) << "Unhandled type for Value: " << mrb_str_to_cstr(mrb, in);
        res.emplace<std::monostate>();
        break;
      }
    }

    return res;
  }

  /// @brief Convert property value to ruby
  /// @param[in] mrb MRuby state
  /// @param[in] value Value to convert
  /// @return MRuby value
  inline mrb_value toRuby(mrb_state *mrb, const Value &value)
  {
    mrb_value res = visit(
        overloaded {
            [](const std::monostate &) -> mrb_value { return mrb_nil_value(); },
            [](const std::nullptr_t &) -> mrb_value { return mrb_nil_value(); },

            // Not handled yet
            [mrb](const EntityPtr &entity) -> mrb_value {
              return MRubySharedPtr<Entity>::wrap(mrb, "Entity", entity);
            },
            [mrb](const EntityList &list) -> mrb_value {
              mrb_value ary = mrb_ary_new_capa(mrb, list.size());

              for (auto &e : list)
                mrb_ary_push(mrb, ary, MRubySharedPtr<Entity>::wrap(mrb, "Entity", e));

              return ary;
            },
            [mrb](const entity::DataSet &v) -> mrb_value { return toRuby(mrb, v); },

            // Handled types
            [mrb](const entity::Vector &v) -> mrb_value {
              mrb_value ary = mrb_ary_new_capa(mrb, v.size());
              for (auto &f : v)
                mrb_ary_push(mrb, ary, mrb_float_value(mrb, f));
              return ary;
            },
            [mrb](const Timestamp &v) -> mrb_value { return toRuby(mrb, v); },
            [mrb](const string &arg) -> mrb_value { return mrb_str_new_cstr(mrb, arg.c_str()); },
            [](const bool arg) -> mrb_value { return mrb_bool_value(static_cast<mrb_bool>(arg)); },
            [mrb](const double arg) -> mrb_value { return mrb_float_value(mrb, arg); },
            [mrb](const int64_t arg) -> mrb_value { return mrb_int_value(mrb, arg); }},
        value);

    return res;
  }

  /// @brief Convert  ruby Hash or Value to properties
  /// @param[in] mrb MRuby state
  /// @param[in] value Ruby value to convert
  ///   If Hash, then convert to MTConnect properties, otherwise set the Properties VALUE
  /// @param[out] props converted properties
  /// @return `true` if successful
  inline bool fromRuby(mrb_state *mrb, mrb_value value, Properties &props)
  {
    if (mrb_type(value) != MRB_TT_HASH)
    {
      Value v = valueFromRuby(mrb, value);
      props.emplace("VALUE", v);
    }
    else
    {
      auto hash = mrb_hash_ptr(value);
      mrb_hash_foreach(
          mrb, hash,
          [](mrb_state *mrb, mrb_value key, mrb_value val, void *data) {
            Properties *props = static_cast<Properties *>(data);
            string k = stringFromRuby(mrb, key);
            auto v = valueFromRuby(mrb, val);

            props->emplace(k, v);

            return 0;
          },
          &props);
    }

    return true;
  }

  /// @brief Convert properties to ruby Hash
  /// @param[in] mrb MRuby state
  /// @param[in] props  properties
  /// @return mruby Hash representing the properties
  inline mrb_value toRuby(mrb_state *mrb, const Properties &props)
  {
    mrb_value hash = mrb_hash_new(mrb);
    for (auto &[key, value] : props)
    {
      mrb_sym k = mrb_intern_cstr(mrb, key.c_str());
      mrb_value v = toRuby(mrb, value);

      mrb_hash_set(mrb, hash, mrb_symbol_value(k), v);
    }

    return hash;
  }

  /// @brief Ruby Entity wrapper
  struct RubyEntity
  {
    /// @brief Create Ruby Entity class and method wrappers
    static void initialize(mrb_state *mrb, RClass *module)
    {
      auto entityClass = mrb_define_class_under(mrb, module, "Entity", mrb->object_class);
      MRB_SET_INSTANCE_TT(entityClass, MRB_TT_DATA);

      mrb_define_method(
          mrb, entityClass, "initialize",
          [](mrb_state *mrb, mrb_value self) {
            const char *name;
            mrb_value properties;
            mrb_get_args(mrb, "zo", &name, &properties);

            Properties props;
            fromRuby(mrb, properties, props);

            auto entity = make_shared<Entity>(name, props);
            MRubySharedPtr<Entity>::replace(mrb, self, entity);

            return self;
          },
          MRB_ARGS_REQ(2));

      mrb_define_method(
          mrb, entityClass, "name",
          [](mrb_state *mrb, mrb_value self) {
            auto entity = MRubySharedPtr<Entity>::unwrap(self);
            return mrb_str_new_cstr(mrb, entity->getName().c_str());
          },
          MRB_ARGS_NONE());
      mrb_define_method(
          mrb, entityClass, "hash",
          [](mrb_state *mrb, mrb_value self) {
            auto entity = MRubySharedPtr<Entity>::unwrap(self);
            return mrb_str_new_cstr(mrb, entity->hash().c_str());
          },
          MRB_ARGS_NONE());
      mrb_define_method(
          mrb, entityClass, "value",
          [](mrb_state *mrb, mrb_value self) {
            auto entity = MRubySharedPtr<Entity>::unwrap(self);
            return toRuby(mrb, entity->getValue());
          },
          MRB_ARGS_NONE());
      mrb_define_method(
          mrb, entityClass, "value=",
          [](mrb_state *mrb, mrb_value self) {
            auto entity = MRubySharedPtr<Entity>::unwrap(self);
            mrb_value value;
            mrb_get_args(mrb, "o", &value);
            entity->setValue(valueFromRuby(mrb, value));
            return value;
          },
          MRB_ARGS_REQ(1));

      mrb_define_method(
          mrb, entityClass, "properties",
          [](mrb_state *mrb, mrb_value self) {
            auto entity = MRubySharedPtr<Entity>::unwrap(self);
            auto props = entity->getProperties();

            return toRuby(mrb, props);
          },
          MRB_ARGS_NONE());
      mrb_define_method(
          mrb, entityClass, "[]",
          [](mrb_state *mrb, mrb_value self) {
            auto entity = MRubySharedPtr<Entity>::unwrap(self);
            const char *key;

            mrb_get_args(mrb, "z", &key);

            auto props = entity->getProperties();
            auto it = props.find(key);
            if (it != props.end())
              return toRuby(mrb, it->second);
            else
              return mrb_nil_value();
          },
          MRB_ARGS_REQ(1));
      mrb_define_method(
          mrb, entityClass, "[]=",
          [](mrb_state *mrb, mrb_value self) {
            auto entity = MRubySharedPtr<Entity>::unwrap(self);
            const char *key;
            mrb_value value;
            mrb_get_args(mrb, "zo", &key, &value);

            entity->setProperty(key, valueFromRuby(mrb, value));

            return value;
          },
          MRB_ARGS_REQ(1));

      auto componentClass = mrb_define_class_under(mrb, module, "Component", entityClass);
      MRB_SET_INSTANCE_TT(componentClass, MRB_TT_DATA);

      mrb_define_method(
          mrb, componentClass, "children",
          [](mrb_state *mrb, mrb_value self) {
            auto comp = MRubySharedPtr<Entity>::unwrap<Component>(mrb, self);
            mrb_value ary = mrb_ary_new(mrb);
            const auto &list = comp->getChildren();
            if (list)
            {
              auto mod = mrb_module_get(mrb, "MTConnect");
              auto klass = mrb_class_get_under(mrb, mod, "Component");

              for (const auto &c : *list)
              {
                ComponentPtr cmp = dynamic_pointer_cast<Component>(c);
                if (cmp)
                  mrb_ary_push(mrb, ary, MRubySharedPtr<Entity>::wrap(mrb, klass, cmp));
              }
            }

            return ary;
          },
          MRB_ARGS_NONE());
      mrb_define_method(
          mrb, componentClass, "data_items",
          [](mrb_state *mrb, mrb_value self) {
            auto comp = MRubySharedPtr<Entity>::unwrap<Component>(mrb, self);
            mrb_value ary = mrb_ary_new(mrb);
            const auto &list = comp->getDataItems();
            if (list)
            {
              auto mod = mrb_module_get(mrb, "MTConnect");
              auto klass = mrb_class_get_under(mrb, mod, "DataItem");

              for (const auto &c : *list)
              {
                DataItemPtr di = dynamic_pointer_cast<DataItem>(c);
                if (di)
                  mrb_ary_push(mrb, ary, MRubySharedPtr<Entity>::wrap(mrb, klass, di));
              }
            }

            return ary;
          },
          MRB_ARGS_NONE());

      mrb_define_method(
          mrb, componentClass, "uuid",
          [](mrb_state *mrb, mrb_value self) {
            auto comp = MRubySharedPtr<Entity>::unwrap<Component>(mrb, self);
            auto &uuid = comp->getUuid();
            if (uuid)
              return mrb_str_new_cstr(mrb, uuid->c_str());
            else
              return mrb_nil_value();
          },
          MRB_ARGS_NONE());

      auto deviceClass = mrb_define_class_under(mrb, module, "Device", componentClass);
      MRB_SET_INSTANCE_TT(deviceClass, MRB_TT_DATA);

      mrb_define_method(
          mrb, deviceClass, "data_item",
          [](mrb_state *mrb, mrb_value self) {
            auto dev = MRubySharedPtr<Entity>::unwrap<Device>(mrb, self);
            const char *name;
            mrb_get_args(mrb, "z", &name);

            auto di = dev->getDeviceDataItem(name);
            if (di)
              return MRubySharedPtr<Entity>::wrap(mrb, "DataItem", di);
            else
              return mrb_nil_value();
          },
          MRB_ARGS_REQ(1));

      auto dataItemClass = mrb_define_class_under(mrb, module, "DataItem", entityClass);
      MRB_SET_INSTANCE_TT(dataItemClass, MRB_TT_DATA);

      mrb_define_method(
          mrb, dataItemClass, "name",
          [](mrb_state *mrb, mrb_value self) {
            auto di = MRubySharedPtr<Entity>::unwrap<DataItem>(mrb, self);
            if (di->getName())
              return mrb_str_new_cstr(mrb, (*di->getName()).c_str());
            else
              return mrb_nil_value();
          },
          MRB_ARGS_NONE());
      mrb_define_method(
          mrb, dataItemClass, "observation_name",
          [](mrb_state *mrb, mrb_value self) {
            auto di = MRubySharedPtr<Entity>::unwrap<DataItem>(mrb, self);
            return mrb_str_new_cstr(mrb, di->getObservationName().c_str());
          },
          MRB_ARGS_NONE());
      mrb_define_method(
          mrb, dataItemClass, "id",
          [](mrb_state *mrb, mrb_value self) {
            auto di = MRubySharedPtr<Entity>::unwrap<DataItem>(mrb, self);
            return mrb_str_new_cstr(mrb, di->getId().c_str());
          },
          MRB_ARGS_NONE());
      mrb_define_method(
          mrb, dataItemClass, "type",
          [](mrb_state *mrb, mrb_value self) {
            auto di = MRubySharedPtr<Entity>::unwrap<DataItem>(mrb, self);
            return mrb_str_new_cstr(mrb, di->getType().c_str());
          },
          MRB_ARGS_NONE());
      mrb_define_method(
          mrb, dataItemClass, "sub_type",
          [](mrb_state *mrb, mrb_value self) {
            auto di = MRubySharedPtr<Entity>::unwrap<DataItem>(mrb, self);
            return mrb_str_new_cstr(mrb, di->getSubType().c_str());
          },
          MRB_ARGS_NONE());
      mrb_define_method(
          mrb, dataItemClass, "topic",
          [](mrb_state *mrb, mrb_value self) {
            auto di = MRubySharedPtr<Entity>::unwrap<DataItem>(mrb, self);
            return mrb_str_new_cstr(mrb, di->getTopic().c_str());
          },
          MRB_ARGS_NONE());
      mrb_define_method(
          mrb, dataItemClass, "topic=",
          [](mrb_state *mrb, mrb_value self) {
            auto di = MRubySharedPtr<Entity>::unwrap<DataItem>(mrb, self);
            char *val;
            mrb_get_args(mrb, "z", &val);

            di->setTopic(val);

            return mrb_str_new_cstr(mrb, val);
          },
          MRB_ARGS_REQ(1));

      auto tokensClass = mrb_define_class_under(mrb, module, "Tokens", entityClass);
      MRB_SET_INSTANCE_TT(tokensClass, MRB_TT_DATA);
      mrb_define_method(
          mrb, tokensClass, "initialize",
          [](mrb_state *mrb, mrb_value self) {
            const char *name;
            mrb_value properties;
            mrb_get_args(mrb, "zo", &name, &properties);

            Properties props;
            fromRuby(mrb, properties, props);

            auto entity = make_shared<pipeline::Tokens>(name, props);
            MRubySharedPtr<Entity>::replace(mrb, self, entity);

            return self;
          },
          MRB_ARGS_REQ(2));

      mrb_define_method(
          mrb, tokensClass, "tokens",
          [](mrb_state *mrb, mrb_value self) {
            auto tokens = MRubySharedPtr<Entity>::unwrap<pipeline::Tokens>(mrb, self);

            mrb_value ary = mrb_ary_new(mrb);
            for (auto &token : tokens->m_tokens)
            {
              mrb_ary_push(mrb, ary, mrb_str_new_cstr(mrb, token.c_str()));
            }
            return ary;
          },
          MRB_ARGS_NONE());

      mrb_define_method(
          mrb, tokensClass, "tokens=",
          [](mrb_state *mrb, mrb_value self) {
            auto tokens = MRubySharedPtr<Entity>::unwrap<pipeline::Tokens>(mrb, self);
            mrb_value ary;
            mrb_get_args(mrb, "A", &ary);
            if (mrb_array_p(ary))
            {
              tokens->m_tokens.clear();
              auto aryp = mrb_ary_ptr(ary);
              for (int i = 0; i < ARY_LEN(aryp); i++)
              {
                auto item = ARY_PTR(aryp)[i];
                tokens->m_tokens.push_back(stringFromRuby(mrb, item));
              }
            }
            return ary;
          },
          MRB_ARGS_REQ(1));

      auto timestampedClass = mrb_define_class_under(mrb, module, "Timestamped", tokensClass);
      MRB_SET_INSTANCE_TT(timestampedClass, MRB_TT_DATA);
      mrb_define_method(
          mrb, timestampedClass, "initialize",
          [](mrb_state *mrb, mrb_value self) {
            const char *name;
            mrb_value properties;
            mrb_get_args(mrb, "zo", &name, &properties);

            Properties props;
            fromRuby(mrb, properties, props);

            auto entity = make_shared<pipeline::Timestamped>(name, props);
            MRubySharedPtr<Entity>::replace(mrb, self, entity);

            return self;
          },
          MRB_ARGS_REQ(2));

      mrb_define_method(
          mrb, tokensClass, "timestamp",
          [](mrb_state *mrb, mrb_value self) {
            auto ts = MRubySharedPtr<Entity>::unwrap<pipeline::Timestamped>(mrb, self);

            return toRuby(mrb, ts->m_timestamp);
          },
          MRB_ARGS_NONE());

      mrb_define_method(
          mrb, tokensClass, "timestamp=",
          [](mrb_state *mrb, mrb_value self) {
            auto ts = MRubySharedPtr<Entity>::unwrap<pipeline::Timestamped>(mrb, self);
            mrb_value val;
            mrb_get_args(mrb, "o", &val);

            ts->m_timestamp = timestampFromRuby(mrb, val);

            return val;
          },
          MRB_ARGS_REQ(1));

      mrb_define_method(
          mrb, tokensClass, "duration",
          [](mrb_state *mrb, mrb_value self) {
            auto ts = MRubySharedPtr<Entity>::unwrap<pipeline::Timestamped>(mrb, self);
            if (ts->m_duration)
              return mrb_float_value(mrb, *(ts->m_duration));
            else
              return mrb_nil_value();
          },
          MRB_ARGS_NONE());

      mrb_define_method(
          mrb, tokensClass, "duration=",
          [](mrb_state *mrb, mrb_value self) {
            auto ts = MRubySharedPtr<Entity>::unwrap<pipeline::Timestamped>(mrb, self);
            mrb_float val;
            mrb_get_args(mrb, "f", &val);

            ts->m_duration = val;

            return mrb_float_value(mrb, val);
          },
          MRB_ARGS_REQ(1));
    }
  };

  /// @struct RubyEntity
  /// @remark Ruby Entity wrapper
  /// @code
  /// class Entity -> mtconnect::entity::Entity
  ///   def initialize(name, properties) -> mtconnect::entity::Entity::Entity(name, properties)
  ///   def name -> mtconnect::entity::Entity::getName()
  ///   def hash -> mtconnect::entity::Entity::hash()
  ///   def value -> mtconnect::entity::Entity::getValue()
  ///   def value=(v) -> mtconnect::entity::Entity::setValue(v)
  ///   def properties -> mtconnect::entity::Entity::getProperties()
  ///   def [](n) -> mtconnect::entity::Entity::getProperty(n)
  ///   def []=(n, v) -> mtconnect::entity::Entity::setProperty(n, v)
  /// end
  /// @endcode
  ///
  /// @remark Ruby Component wrapper
  /// @code
  /// class Component < Entity -> mtconnect::device_model::Component
  ///   def id -> mtconnect::device_model::Component::getId()
  ///   def uuid -> mtconnect::device_model::Component::getUuid()
  ///   def children -> mtconnect::device_model::Component::getChildren()
  ///   def data_items -> mtconnect::device_model::Component::getDataItems()
  /// end
  /// @endcode
  ///
  /// @remark Ruby Device wrapper
  /// @code
  /// class Device < Component -> mtconnect::device_model::Device
  ///   def data_item(name) -> mtconnect::device_model::Component::getDeviceDataItem(name)
  /// end
  /// @endcode
  ///
  /// @remark Ruby DataItem wrapper
  /// @code
  /// class DataItem < Event -> mtconnect::device_model::data_item::DataItem
  ///   def observation_name -> mtconnect::device_model::data_item::DataItem::getObservationName()
  ///   def id -> mtconnect::device_model::data_item::DataItem::getId()
  ///   def name -> mtconnect::device_model::data_item::DataItem::getName()
  ///   def type -> mtconnect::device_model::data_item::DataItem::getType()
  ///   def sub_type -> mtconnect::device_model::data_item::DataItem::getSubType()
  ///   def topic -> mtconnect::device_model::data_item::DataItem::getTopic()
  ///   def topic=(v) -> mtconnect::device_model::data_item::DataItem::setTopic()
  /// end
  /// @endcode

}  // namespace mtconnect::ruby
