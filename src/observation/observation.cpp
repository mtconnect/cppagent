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

#include "observation.hpp"

#include <mutex>
#include <regex>

#include "device_model/data_item/data_item.hpp"
#include "entity/factory.hpp"
#include "logging.hpp"

#ifdef _WINDOWS
#define strcasecmp stricmp
#define strncasecmp strnicmp
#define strtof strtod
#endif

using namespace std;

namespace mtconnect {
  using namespace entity;

  namespace observation {
    FactoryPtr Observation::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        factory = make_shared<Factory>(Requirements({{"dataItemId", true},
                                                     {"timestamp", TIMESTAMP, true},
                                                     {"sequence", false},
                                                     {"subType", false},
                                                     {"name", false},
                                                     {"compositionId", false}}),
                                       [](const std::string &name, Properties &props) -> EntityPtr {
                                         return make_shared<Observation>(name, props);
                                       });

        factory->registerFactory("Events:Message", Message::getFactory());
        factory->registerFactory("Events:MessageDiscrete", Message::getFactory());
        factory->registerFactory("Events:AssetChanged", AssetEvent::getFactory());
        factory->registerFactory("Events:AssetRemoved", AssetEvent::getFactory());
        factory->registerFactory("Events:Alarm", Alarm::getFactory());

        // regex(".+TimeSeries$")
        factory->registerFactory(
            [](const std::string &name) { return ends_with(name, "TimeSeries"); },
            Timeseries::getFactory());
        factory->registerFactory([](const std::string &name) { return ends_with(name, "DataSet"); },
                                 DataSetEvent::getFactory());
        factory->registerFactory([](const std::string &name) { return ends_with(name, "Table"); },
                                 TableEvent::getFactory());
        factory->registerFactory(
            [](const std::string &name) { return starts_with(name, "Condition:"); },
            Condition::getFactory());
        factory->registerFactory(
            [](const std::string &name) {
              return starts_with(name, "Samples:") && ends_with(name, ":3D");
            },
            ThreeSpaceSample::getFactory());
        factory->registerFactory(
            [](const std::string &name) {
              return starts_with(name, "Events:") && ends_with(name, ":3D");
            },
            ThreeSpaceSample::getFactory());
        factory->registerFactory(
            [](const std::string &name) { return starts_with(name, "Samples:"); },
            Sample::getFactory());
        factory->registerFactory(
            [](const std::string &name) {
              return starts_with(name, "Events:") && ends_with(name, ":DOUBLE");
            },
            DoubleEvent::getFactory());
        factory->registerFactory(
            [](const std::string &name) {
              return starts_with(name, "Events:") && ends_with(name, ":INT");
            },
            IntEvent::getFactory());
        factory->registerFactory(
            [](const std::string &name) { return starts_with(name, "Events:"); },
            Event::getFactory());
      }
      return factory;
    }

    ObservationPtr Observation::make(const DataItemPtr dataItem, const Properties &incompingProps,
                                     const Timestamp &timestamp, entity::ErrorList &errors)
    {
      NAMED_SCOPE("Observation");

      auto props = entity::Properties(incompingProps);
      setProperties(dataItem, props);
      props.insert_or_assign("timestamp", timestamp);

      bool unavailable {false};
      string level;
      if (dataItem->isCondition())
      {
        auto l = props.find("level");
        if (l != props.end())
        {
          level = std::get<string>(l->second);
          if (iequals(level, "unavailable"))
            unavailable = true;
          props.erase(l);
        }
        else if (l == props.end())
        {
          unavailable = true;
        }
      }
      else
      {
        // Check for unavailable
        auto v = props.find("VALUE");
        if (v != props.end() && holds_alternative<string>(v->second))
        {
          if (iequals(std::get<string>(v->second), "unavailable"))
          {
            unavailable = true;
            props.erase(v);
          }
        }
        else if (v == props.end())
        {
          unavailable = true;
        }
      }

      auto ent = getFactory()->create(dataItem->getKey(), props, errors);
      if (!ent)
      {
        LOG(warning) << "Could not parse properties for data item: " << dataItem->getId();
        for (auto &e : errors)
        {
          LOG(warning) << "   Error: " << e->what();
        }
        throw EntityError("Invalid properties for data item");
      }

      auto obs = dynamic_pointer_cast<Observation>(ent);
      obs->m_timestamp = timestamp;
      obs->m_dataItem = dataItem;

      if (unavailable)
        obs->makeUnavailable();

      if (!dataItem->isCondition())
        obs->setEntityName();
      else if (!unavailable)
        dynamic_pointer_cast<Condition>(obs)->setLevel(level);

      return obs;
    }

    FactoryPtr Event::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        factory = make_shared<Factory>(*Observation::getFactory());
        factory->setFunction([](const std::string &name, Properties &props) -> EntityPtr {
          return make_shared<Event>(name, props);
        });
        factory->addRequirements(
            Requirements {{"VALUE", false}, {"resetTriggered", USTRING, false}});
      }

      return factory;
    }

    FactoryPtr DataSetEvent::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        factory = make_shared<Factory>(*Observation::getFactory());
        factory->setFunction([](const std::string &name, Properties &props) -> EntityPtr {
          auto ent = make_shared<DataSetEvent>(name, props);
          auto v = ent->m_properties.find("VALUE");
          if (v != ent->m_properties.end())
          {
            auto &ds = std::get<DataSet>(v->second);
            ent->m_properties.insert_or_assign("count", int64_t(ds.size()));
          }
          return ent;
        });
        factory->addRequirements(Requirements {{"count", INTEGER, false},
                                               {"VALUE", DATA_SET, false},
                                               {"resetTriggered", USTRING, false}});
      }

      return factory;
    }

    FactoryPtr TableEvent::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        factory = make_shared<Factory>(*DataSetEvent::getFactory());
        factory->setFunction([](const std::string &name, Properties &props) -> EntityPtr {
          auto ent = make_shared<TableEvent>(name, props);
          auto v = ent->m_properties.find("VALUE");
          if (v != ent->m_properties.end())
          {
            auto &ds = std::get<DataSet>(v->second);
            ent->m_properties.insert_or_assign("count", int64_t(ds.size()));
          }
          return ent;
        });

        factory->addRequirements(Requirements {{"VALUE", TABLE, false}});
      }

      return factory;
    }

    FactoryPtr DoubleEvent::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        factory = make_shared<Factory>(*Observation::getFactory());
        factory->setFunction([](const std::string &name, Properties &props) -> EntityPtr {
          return make_shared<DoubleEvent>(name, props);
        });
        factory->addRequirements(Requirements({{"resetTriggered", USTRING, false},
                                               {"statistic", USTRING, false},
                                               {"duration", DOUBLE, false},
                                               {"VALUE", DOUBLE, false}}));
      }
      return factory;
    }

    FactoryPtr IntEvent::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        factory = make_shared<Factory>(*Observation::getFactory());
        factory->setFunction([](const std::string &name, Properties &props) -> EntityPtr {
          return make_shared<IntEvent>(name, props);
        });
        factory->addRequirements(Requirements({{"resetTriggered", USTRING, false},
                                               {"statistic", USTRING, false},
                                               {"duration", DOUBLE, false},
                                               {"VALUE", INTEGER, false}}));
      }
      return factory;
    }

    FactoryPtr Sample::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        factory = make_shared<Factory>(*Observation::getFactory());
        factory->setFunction([](const std::string &name, Properties &props) -> EntityPtr {
          return make_shared<Sample>(name, props);
        });
        factory->addRequirements(Requirements({{"sampleRate", DOUBLE, false},
                                               {"resetTriggered", USTRING, false},
                                               {"statistic", USTRING, false},
                                               {"duration", DOUBLE, false},
                                               {"VALUE", DOUBLE, false}}));
      }
      return factory;
    }

    FactoryPtr ThreeSpaceSample::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        factory = make_shared<Factory>(*Sample::getFactory());
        factory->setFunction([](const std::string &name, Properties &props) -> EntityPtr {
          return make_shared<ThreeSpaceSample>(name, props);
        });
        factory->addRequirements(Requirements({{"VALUE", VECTOR, 3, false}}));
      }
      return factory;
    }

    FactoryPtr Timeseries::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        factory = make_shared<Factory>(*Sample::getFactory());
        factory->setFunction([](const std::string &name, Properties &props) -> EntityPtr {
          auto ent = make_shared<Timeseries>(name, props);
          auto v = ent->m_properties.find("VALUE");
          if (v != ent->m_properties.end())
          {
            auto &ts = std::get<entity::Vector>(v->second);
            ent->m_properties.insert_or_assign("sampleCount", int64_t(ts.size()));
          }
          return ent;
        });
        factory->addRequirements(
            Requirements({{"sampleCount", INTEGER, false},
                          {"VALUE", VECTOR, 0, entity::Requirement::Infinite}}));
      }
      return factory;
    }

    FactoryPtr Condition::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        factory = make_shared<Factory>(*Observation::getFactory());
        factory->setFunction([](const std::string &name, Properties &props) -> EntityPtr {
          auto cond = make_shared<Condition>(name, props);
          if (cond)
          {
            auto code = cond->m_properties.find("nativeCode");
            if (code != cond->m_properties.end())
              cond->m_code = std::get<string>(code->second);
          }
          return cond;
        });
        factory->addRequirements(Requirements {{"type", USTRING, true},
                                               {"nativeCode", false},
                                               {"nativeSeverity", false},
                                               {"qualifier", USTRING, false},
                                               {"statistic", USTRING, false},
                                               {"VALUE", false}});
      }

      return factory;
    }

    FactoryPtr AssetEvent::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        factory = make_shared<Factory>(*Event::getFactory());
        factory->setFunction([](const std::string &name, Properties &props) -> EntityPtr {
          auto ent = make_shared<AssetEvent>(name, props);
          if (!ent->hasProperty("assetType") && !ent->hasValue())
          {
            ent->setProperty("assetType", "UNAVAILABLE"s);
          }
          return ent;
        });
        factory->addRequirements(Requirements({{"assetType", false}}));
      }
      return factory;
    }

    FactoryPtr Message::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        factory = make_shared<Factory>(*Event::getFactory());
        factory->setFunction([](const std::string &name, Properties &props) -> EntityPtr {
          return make_shared<Message>(name, props);
        });
        factory->addRequirements(Requirements({{"nativeCode", false}}));
      }
      return factory;
    }

    FactoryPtr Alarm::getFactory()
    {
      static FactoryPtr factory;
      if (!factory)
      {
        factory = make_shared<Factory>(*Event::getFactory());
        factory->setFunction([](const std::string &name, Properties &props) -> EntityPtr {
          return make_shared<Alarm>(name, props);
        });
        factory->addRequirements(Requirements({{"code", false},
                                               {"nativeCode", false},
                                               {"state", USTRING, false},
                                               {"severity", false}}));
      }
      return factory;
    }

    bool Condition::replace(ConditionPtr &old, ConditionPtr &_new)
    {
      if (!m_prev)
        return false;

      if (m_prev == old)
      {
        _new->m_prev = old->m_prev;
        m_prev = _new;
        return true;
      }

      return m_prev->replace(old, _new);
    }

    ConditionPtr Condition::deepCopy()
    {
      auto n = make_shared<Condition>(*this);

      if (m_prev)
      {
        n->m_prev = m_prev->deepCopy();
      }

      return n;
    }

    ConditionPtr Condition::deepCopyAndRemove(ConditionPtr &old)
    {
      if (this->getptr() == old)
      {
        if (m_prev)
          return m_prev->deepCopy();
        else
          return nullptr;
      }

      auto n = make_shared<Condition>(*this);

      if (m_prev)
      {
        n->m_prev = m_prev->deepCopyAndRemove(old);
      }

      return n;
    }
  }  // namespace observation
}  // namespace mtconnect
