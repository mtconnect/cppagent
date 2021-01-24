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

#include "data_set.hpp"
#include "device_model/component.hpp"
#include "device_model/data_item.hpp"
#include "entity/entity.hpp"
#include "globals.hpp"
#include "ref_counted.hpp"

#include <date/date.h>

#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace mtconnect
{
  namespace observation
  {
    class Observation;
    using ObservationPtr = std::shared_ptr<Observation>;
    using ObservationList = std::list<ObservationPtr>;
    
    class Observation : public entity::Entity
    {
    public:
      using Timestamp = std::chrono::time_point<std::chrono::system_clock>;

      using entity::Entity::Entity;
      static entity::FactoryPtr getFactory();
      ~Observation() override = default;

      static ObservationPtr make(const DataItem *dataItem,
                                 const entity::Properties &props,
                                 const Timestamp &timestamp,
                                 entity::ErrorList &errors);
      
      void setDataItem(const DataItem *dataItem)
      {
        m_dataItem = dataItem;
        setProperty("dataItemId", m_dataItem->getId());
        if (!m_dataItem->getName().empty())
          setProperty("name", m_dataItem->getName());
        if (!m_dataItem->getCompositionId().empty())
          setProperty("compositionId", m_dataItem->getCompositionId());
        if (!m_dataItem->getSubType().empty())
          setProperty("subType", m_dataItem->getSubType());
        if (!m_dataItem->getStatistic().empty())
          setProperty("statistic", m_dataItem->getStatistic());
      }
      
      const DataItem *getDataItem() const
      {
        return m_dataItem;
      }

      void setTimestamp(const Timestamp &ts)
      {
        m_timestamp = ts;
        setProperty("timestamp", date::format("%FT%TZ", m_timestamp));
      }

      void setSequence(int64_t sequence)
      {
        m_sequence = sequence;
        setProperty("sequence", sequence);
      }

      virtual void makeUnavailable()
      {
        m_unavailable = true;
        setProperty("VALUE", "UNAVAILABLE");
      }
      bool isUnavailable() const { return m_unavailable; }
      virtual void setEntityName()
      {
        Entity::setName(m_dataItem->getPrefixedElementName());
      }

      bool operator<(const Observation &another) const
      {
        if ((*m_dataItem) < (*another.m_dataItem))
          return true;
        else if (*m_dataItem == *another.m_dataItem)
          return m_sequence < another.m_sequence;
        else
          return false;
      }

      void clearResetTriggered() { m_properties.erase("resetTriggered"); }

    protected:
      Timestamp m_timestamp;
      bool m_unavailable{false};
      const DataItem *m_dataItem{nullptr};
      uint64_t m_sequence{0};
    };

    class Sample : public Observation
    {
    public:
      using Observation::Observation;
      static entity::FactoryPtr getFactory();
      ~Sample() override = default;
    };

    class ThreeSpaceSample : public Sample
    {
    public:
      using Sample::Sample;
      static entity::FactoryPtr getFactory();
      ~ThreeSpaceSample() override = default;
    };

    class Timeseries : public Observation
    {
    public:
      using Observation::Observation;
      static entity::FactoryPtr getFactory();
      ~Timeseries() override = default;
    };

    class Condition;
    using ConditionPtr = std::shared_ptr<Condition>;

    class Condition : public Observation
    {
    public:
      enum Level
      {
        NORMAL,
        WARNING,
        FAULT,
        UNAVAILABLE
      };

      using Observation::Observation;
      static entity::FactoryPtr getFactory();
      ~Condition() override = default;

      ConditionPtr getptr() { return std::dynamic_pointer_cast<Condition>(Entity::getptr()); }

      void setLevel(Level level)
      {
        m_level = level;
        setEntityName();
      }

      void setLevel(const std::string &s)
      {
        if (iequals("normal", s))
          setLevel(NORMAL);
        else if (iequals("warning", s))
          setLevel(WARNING);
        else if (iequals("fault", s))
          setLevel(FAULT);
        else if (iequals("unavailable", s))
          setLevel(UNAVAILABLE);
        else
          throw entity::PropertyError("Invalid Condition LeveL: " + s);
      }

      
      void normal()
      {
        m_level = NORMAL;
        m_code.clear();
        m_properties.erase("nativeCode");
        m_properties.erase("nativeSeverity");
        m_properties.erase("qualifier");
        m_properties.erase("statistic");
        m_properties.erase("VALUE");
        setEntityName();
      }
      
      void makeUnavailable() override
      {
        m_unavailable = true;
        m_level = UNAVAILABLE;
        setEntityName();
      }
      
      void setEntityName() override
      {
        switch (m_level)
        {
        case NORMAL:
          setName("Normal");
          break;

        case WARNING:
          setName("Warning");
          break;

        case FAULT:
          setName("Fault");
          break;

        case UNAVAILABLE:
          setName("Unavailable");
          break;
        }
      }

      ConditionPtr getFirst()
      {
        if (m_prev)
          return m_prev->getFirst();

        return getptr();
      }
      
      void getConditonList(std::list<ConditionPtr> &list)
      {
        if (m_prev)
          m_prev->getConditonList(list);

        list.emplace_back(getptr());
      }
      
      ConditionPtr find(const std::string &code)
      {
        if (m_code == code)
          return getptr();

        if (m_prev)
          return m_prev->find(code);

        return nullptr;
      }

      bool replace(ConditionPtr &old, ConditionPtr &_new);
      ConditionPtr deepCopy();
      ConditionPtr deepCopyAndRemove(ConditionPtr &old);
      
      const std::string &getCode() const { return m_code; }
      Level getLevel() const { return m_level; }
      ConditionPtr getPrev() const { return m_prev; }
      void appendTo(ConditionPtr cond) { m_prev = cond; }
      
    protected:
      std::string m_code;
      Level m_level{NORMAL};
      ConditionPtr m_prev;
    };
    
    class Event : public Observation
    {
    public:
      using Observation::Observation;
      static entity::FactoryPtr getFactory();
      ~Event() override = default;
    };

    class DataSetEvent : public Event
    {
    public:
      using Event::Event;
      static entity::FactoryPtr getFactory();
      ~DataSetEvent() override = default;
      
      const DataSet &getDataSet() const
      {
        const entity::Value &v = getProperty("VALuE");
        return std::get<DataSet>(v);
      }
      void setDataSet(const DataSet &set)
      {
        setProperty("VALUE", set);
      }
      
    };
    
    using DataSetEventPtr = std::shared_ptr<DataSetEvent>;

    class AssetEvent : public Event
    {
    public:
      using Event::Event;
      static entity::FactoryPtr getFactory();
      ~AssetEvent() override = default;
    };

    class Message : public Event
    {
    public:
      using Event::Event;
      static entity::FactoryPtr getFactory();
      ~Message() override = default;
    };

    class Alarm : public Event
    {
    public:
      using Event::Event;
      static entity::FactoryPtr getFactory();
      ~Alarm() override = default;
    };
    
    using ObservationComparer = bool (*)(ObservationPtr &, ObservationPtr &);
    inline bool ObservationCompare(ObservationPtr &aE1, ObservationPtr &aE2) { return aE1 < aE2; }
  }  // namespace observation
}  // namespace mtconnect
