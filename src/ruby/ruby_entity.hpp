//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "entity/entity.hpp"
#include "device_model/device.hpp"
#include "device_model/data_item/data_item.hpp"
#include "ruby_type.hpp"
#include "ruby_smart_ptr.hpp"
#include <rice/rice.hpp>
#include <rice/stl.hpp>
#include <ruby/thread.h>

namespace Rice::detail {
  template<>
  struct Type<entity::DataSet>
  {
    static bool verify() { return true; }
  };

  template <>
  class From_Ruby<entity::DataSet>
  {
  public:
    entity::DataSet convert(VALUE value);
  };
  
  template <>
  class To_Ruby<entity::DataSet>
  {
  public:
    VALUE convert(const entity::DataSet &value);
  };

  template <>
  class To_Ruby<entity::DataSet&>
  {
  public:
    VALUE convert(const entity::DataSet &value);
  };
  
  template<>
  struct Type<entity::DataSetValue>
  {
    static bool verify() { return true; }
  };
  
  template<>
  struct To_Ruby<entity::DataSetValue>
  {
    VALUE convert(const entity::DataSetValue &value)
    {
      VALUE rv;
      
      rv = visit(overloaded {
        [](const std::monostate &v) -> VALUE { return Qnil; },
        [](const std::string &v) -> VALUE { return To_Ruby<string>().convert(v); },
        [](const entity::DataSet &v) -> VALUE {
          return To_Ruby<entity::DataSet>().convert(v);
        },
        [](const int64_t v) -> VALUE { return To_Ruby<int64_t>().convert(v); },
        [](const double v) -> VALUE { return To_Ruby<double>().convert(v); }
      }, value);
      
      return rv;
    }
  };
  
  template<>
  struct From_Ruby<entity::DataSetValue>
  {
    entity::DataSetValue convert(VALUE value)
    {
      entity::DataSetValue res;
      
      switch (TYPE(value))
      {
        case RUBY_T_STRING:
          res.emplace<string>(From_Ruby<string>().convert(value));
          break;
          
        case RUBY_T_BIGNUM:
        case RUBY_T_FIXNUM:
          res.emplace<int64_t>(From_Ruby<int64_t>().convert(value));
          break;

        case RUBY_T_FLOAT:
          res.emplace<double>(From_Ruby<double>().convert(value));
          break;
          
        case RUBY_T_HASH:
          res.emplace<entity::DataSet>(From_Ruby<entity::DataSet>().convert(value));
          break;
          
        default:
          break;
      }
      return res;
    }
  };

  entity::DataSet From_Ruby<entity::DataSet>::convert(VALUE value)
  {
    using namespace mtconnect::entity;
    DataSet set;
    if (TYPE(value) == RUBY_T_HASH)
    {
      Rice::Hash h(value);
      for (const auto &e : h)
      {
        DataSetEntry entry(From_Ruby<std::string>().convert(e.first),
                           From_Ruby<DataSetValue>().convert(e.value.value()));
        set.insert(entry);
      }
    }
    return set;
  }

  VALUE To_Ruby<entity::DataSet>::convert(const entity::DataSet &set)
  {
    Rice::Hash h;
    for (const auto &entry : set)
    {
      auto value = To_Ruby<DataSetValue>().convert(entry.m_value);
      h[entry.m_key] = value;
    }
    return h.value();
  }

  VALUE To_Ruby<entity::DataSet&>::convert(const entity::DataSet &set)
  {
    Rice::Hash h;
    for (const auto &entry : set)
    {
      h[entry.m_key] = entry.m_value;
    }
    return h.value();
  }
  
  template<>
  struct Type<EntityList>
  {
    static bool verify() { return true; }
  };
  
  template<>
  struct To_Ruby<EntityList>
  {
    VALUE convert(const EntityList &list)
    {
      Rice::Array ary;
      for (const auto &ent : list)
      {
        ary.push(ent.get());
      }
      
      return ary;
    }
  };

  template<>
  struct From_Ruby<EntityList>
  {
    EntityList convert(VALUE list)
    {
      EntityList ary;
      Rice::Array vary(list);
      for (const auto &ent : vary)
      {
        if (Rice::protect(rb_obj_is_kind_of, ent.value(), mtconnect::ruby::c_Entity))
        {
          Data_Object<Entity> entity(ent.value());
          ary.emplace_back(entity->getptr());
        }
      }
      
      return ary;
    }
  };
    
  template<>
  struct Type<Entity>
  {
    static bool verify() { return true; }
  };
  
  template<>
  struct To_Ruby<Entity>
  {
    VALUE convert(const EntityPtr &e)
    {
      Data_Object<Entity> entity(e.get());
      return entity.value();
    }
  };
  
  template<>
  struct From_Ruby<Entity>
  {
    EntityPtr convert(VALUE e)
    {
      Data_Object<Entity> entity(e);
      return entity->getptr();
    }
  };

  template<>
  struct Type<Value>
  {
    static bool verify() { return true; }
  };

  template<>
  struct Type<Value&>
  {
    static bool verify() { return true; }
  };
  
  inline VALUE ConvertValueToRuby(const Value &value)
  {
    VALUE rv;
    
    rv = visit(overloaded {
      [](const std::monostate &) -> VALUE { return Qnil; },
      [](const std::nullptr_t &) -> VALUE { return Qnil; },

      // Not handled yet
      [](const EntityPtr &entity) -> VALUE {
        return To_Ruby<Entity>().convert(entity);
      },
      [](const EntityList &list) -> VALUE {
        Rice::Array ary;
        for (const auto &ent : list)
        {
          ary.push(ent.get());
        }
        
        return ary;
      },
      [](const entity::DataSet &v) -> VALUE {
        return To_Ruby<entity::DataSet>().convert(v);
      },
              
      // Handled types
      [](const Vector &v) -> VALUE {
        Rice::Array array(v.begin(), v.end());
        return array;
      },
      [](const Timestamp &v) -> VALUE {
        return To_Ruby<Timestamp>().convert(v);
      },
      [](const string &arg) -> VALUE { return To_Ruby<string>().convert(arg); },
      [](const bool arg) -> VALUE { return To_Ruby<bool>().convert(arg); },
      [](const double arg) -> VALUE { return To_Ruby<double>().convert(arg); },
      [](const int64_t arg) -> VALUE { return To_Ruby<int64_t>().convert(arg); }
    }, value);
    
    return rv;
  }

  template<>
  struct To_Ruby<Value>
  {
    VALUE convert(Value &value)
    {
      return ConvertValueToRuby(value);
    }

    VALUE convert(const Value &value)
    {
      return ConvertValueToRuby(value);
    }
  };

  template<>
  struct To_Ruby<Value&>
  {
    VALUE convert(Value &value)
    {
      return ConvertValueToRuby(value);
    }

    VALUE convert(const Value &value)
    {
      return ConvertValueToRuby(value);
    }
  };

  inline Value ConvertRubyToValue(VALUE value)
  {
    using namespace mtconnect::ruby;
    using namespace mtconnect::observation;

    Value res;
    
    switch (TYPE(value))
    {
      case RUBY_T_NIL:
      case RUBY_T_UNDEF:
        res.emplace<std::nullptr_t>();
        break;
        
      case RUBY_T_STRING:
        res.emplace<string>(From_Ruby<string>().convert(value));
        break;
        
      case RUBY_T_BIGNUM:
      case RUBY_T_FIXNUM:
        res.emplace<int64_t>(From_Ruby<int64_t>().convert(value));
        break;

      case RUBY_T_FLOAT:
        res.emplace<double>(From_Ruby<double>().convert(value));
        break;
        
      case RUBY_T_TRUE:
        res.emplace<bool>(true);
        break;

      case RUBY_T_FALSE:
        res.emplace<bool>(false);
        break;
        
      case RUBY_T_HASH:
        res.emplace<DataSet>(From_Ruby<DataSet>().convert(value));
        break;
        
      case RUBY_T_ARRAY:
      {
        res.emplace<Vector>();
        Vector &out = get<Vector>(res);
        const Rice::Array ary(value);
        for (const auto &v : ary) {
          auto t = rb_type(v);
          if (t == RUBY_T_FIXNUM || t == RUBY_T_BIGNUM || t == RUBY_T_FLOAT)
          {
            out.emplace_back(From_Ruby<double>().convert(v));
          }
        }
        break;
      }
        
      case RUBY_T_OBJECT:
        if (protect(rb_obj_is_kind_of, value, rb_cTime))
        {
          res = From_Ruby<Timestamp>().convert(value);
        }
        else if (protect(rb_obj_is_kind_of, value, c_Entity))
        {
          res = From_Ruby<Entity>().convert(value);
        }
        {
          res.emplace<std::monostate>();
        }
        break;
        
      default:
        res.emplace<std::monostate>();
        break;
    }
          
    return res;
  }

  template<>
  struct From_Ruby<Value>
  {
    Value convert(VALUE value)
    {
      return ConvertRubyToValue(value);
    }
  };

  template<>
  struct From_Ruby<Value&>
  {
    Value convert(VALUE value)
    {
      return ConvertRubyToValue(value);
    }
  };

}

namespace mtconnect::ruby {
  using namespace mtconnect;
  using namespace device_model;
  using namespace data_item;
  using namespace entity;
  using namespace std;
  using namespace Rice;
  
  struct RubyEntity {
    void create(Rice::Module &module)
    {
      m_properties = define_class_under<Properties>(module, "Properties");
      m_propertyKey = define_class_under<PropertyKey>(module, "PropertyKey");
      m_entity = define_class_under<Entity>(module, "Entity");
      c_Entity = m_entity.value();
      m_component = define_class_under<Component, Entity>(module, "Component");
      m_device = define_class_under<device_model::Device, device_model::Component>(module, "Device");
      m_dataItem = define_class_under<data_item::DataItem, entity::Entity>(module, "DataItem");
    }
    
    void methods()
    {
      m_entity.define_constructor(smart_ptr::Constructor<Entity, const std::string , const Properties &>(), Arg("name"),
                                  Arg("properties")).
        define_method("value", [](Entity *e) { return e->getValue(); }).
        define_method("value=", [](Entity *e, const Value value) { return e->setValue(value); }, Arg("value")).
        define_method("name", [](Entity *e) { return e->getName().str(); }).
        define_method("property", &Entity::getProperty, Arg("name"));
      
      m_dataItem.define_method("name", [](data_item::DataItem *di) { return di->getName(); }).
        define_method("observation_name", [](data_item::DataItem *di) { return di->getObservationName().str(); }).
        define_method("id", [](data_item::DataItem *di) { return di->getId(); }).
        define_method("type", [](data_item::DataItem *di) { return di->getType(); }).
        define_method("sub_type", [](data_item::DataItem *di) { return di->getSubType(); });
      
      m_component.define_method("component_name", [](Component *c) { return c->getComponentName(); }).
        define_method("uuid", &Component::getUuid).
        define_method("id", &Component::getId).
        define_method("children", [](Component *c) {
          Rice::Array ary;
          const auto &list = c->getChildren();
          if (list)
          {
            for (auto const &s : *list)
            {
              auto cp = dynamic_cast<Component*>(s.get());
              ary.push(cp);
            }
          }
          return ary;
        }).
      define_method("data_items", [](Component *c) {
          Rice::Array ary;
          const auto &list = c->getDataItems();
          if (list)
          {
            for (auto const &s : *list)
            {
              auto di = dynamic_cast<data_item::DataItem*>(s.get());
              ary.push(di);
            }
          }
          return ary;
        });
      
      m_propertyKey.define_constructor(Constructor<PropertyKey, const string>(), Arg("key")).
        define_method("to_s", [](PropertyKey *key) { return key->str(); }).
        define_method("ns?", [](PropertyKey *key) { return key->hasNs(); });
      
      
      m_device.define_method("device_data_items", [](DevicePtr device) {
        Rice::Array ary;
        for (auto const &[k, wdi] : device->getDeviceDataItems())
        {
          const auto &di = wdi.lock();
          ary.push(di.get());
        }
        return ary;
      });
    }
    
    Data_Type<Properties> m_properties;
    Data_Type<PropertyKey> m_propertyKey;
    Data_Type<Entity> m_entity;
    Data_Type<Component> m_component;
    Data_Type<Device> m_device;
    Data_Type<DataItem> m_dataItem;
  };
}

namespace Rice::detail {
  template<>
  struct Type<Properties>
  {
    static bool verify()
    {
      return true;
    }
  };
  
  template<>
  struct From_Ruby<Properties>
  {
    From_Ruby() = default;

    explicit From_Ruby(Arg * arg) : m_arg(arg)
    {
    }

    auto &convert(VALUE value)
    {
      if (TYPE(value) == RUBY_T_HASH)
      {
        Rice::Hash hash(value);
        for (const auto &p : hash)
        {
          PropertyKey key;
          auto pt = TYPE(p.first.value());
          if (pt == RUBY_T_SYMBOL)
            key = Symbol(p.first.value()).str();
          else if (pt == RUBY_T_STRING)
            key = From_Ruby<string>().convert(p.first);
          else
            key = From_Ruby<string>().convert(p.first.call(Identifier("to_s")));
          Value val { From_Ruby<Value>().convert(p.second.value()) };
          
          m_converted.emplace(key, val);
        }
      }
      else
      {
        Value val { From_Ruby<Value>().convert(value) };
        m_converted.emplace("VALUE", val);
      }
      return m_converted;
    }
    
    Arg* m_arg = nullptr;
    Properties m_converted;
  };

  template<>
  struct To_Ruby<Properties>
  {
    VALUE convert(const Properties &props)
    {
      Rice::Hash hash;
      for (const auto &[key, value] : props)
      {
        hash[key.str()] = To_Ruby<Value>().convert(value);
      }
      
      return hash;
    }
  };

  template <>
  class From_Ruby<EntityPtr>
  {
  public:
    EntityPtr convert(VALUE value)
    {
      Wrapper* wrapper = detail::getWrapper(value, Data_Type<Entity>::rb_type());

      using Wrapper_T = WrapperSmartPointer<std::shared_ptr, Entity>;
      Wrapper_T* smartWrapper = static_cast<Wrapper_T*>(wrapper);
      std::shared_ptr<Entity> ptr = dynamic_pointer_cast<Entity>(smartWrapper->data());
      
      if (!ptr)
      {
        std::string message = "Invalid smart pointer wrapper";
          throw std::runtime_error(message.c_str());
      }
      return ptr;
    }
  };

}
