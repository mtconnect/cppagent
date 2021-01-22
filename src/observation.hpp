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
  class Observation2;
  using Observation2Ptr = std::shared_ptr<Observation2>;
  class Observation2 : public entity::Entity
  {
  public:
    using Timestamp = std::chrono::time_point<std::chrono::system_clock>;

    using entity::Entity::Entity;
    static entity::FactoryPtr getFactory();
    ~Observation2() override = default;

    static Observation2Ptr makeObservation(const DataItem *dataItem, entity::Properties &props,
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

    void makeUnavailable()
    {
      m_unavailable = true;
      setProperty("VALUE", "UNAVAILABLE");
    }

    bool operator<(const Observation2 &another) const
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

  class Sample : public Observation2
  {
  public:
    using Observation2::Observation2;
    static entity::FactoryPtr getFactory();
    ~Sample() override = default;
  };

  class Timeseries : public Observation2
  {
  public:
    using Observation2::Observation2;
    static entity::FactoryPtr getFactory();
    ~Timeseries() override = default;
  };

  class Condition : public Observation2
  {
  public:
    using ConditionPtr = std::shared_ptr<Condition>;
    enum Level
    {
      NORMAL,
      WARNING,
      FAULT,
      UNAVAILABLE
    };

    using Observation2::Observation2;
    static entity::FactoryPtr getFactory();
    ~Condition() override = default;

    ConditionPtr getptr() { return std::dynamic_pointer_cast<Condition>(Entity::getptr()); }

    void getConditonList(std::list<ConditionPtr> &list)
    {
      if (m_prev)
        m_prev->getConditonList(list);

      list.emplace_back(getptr());
    }

    void setLevel() {}

    void normal()
    {
      m_level = NORMAL;
      m_properties.erase("nativeCode");
      m_properties.erase("nativeSeverity");
      m_properties.erase("qualifier");
      m_properties.erase("statistic");
      m_properties.erase("VALUE");
    }

    ConditionPtr getFirst()
    {
      if (m_prev)
        return m_prev->getFirst();

      return getptr();
    }

    Level m_level{NORMAL};
    ConditionPtr m_prev;
  };

  class Event : public Observation2
  {
  public:
    using Observation2::Observation2;
    static entity::FactoryPtr getFactory();
    ~Event() override = default;
  };

  class DataSetEvent : public Event
  {
  public:
    using Event::Event;
    static entity::FactoryPtr getFactory();
    ~DataSetEvent() override = default;
  };

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

  // --------------------------------------------------------------------------

  struct AttributeItem : public std::pair<const char *, std::string>
  {
    AttributeItem(const char *f, const std::string &s, bool force = false)
      : std::pair<const char *, std::string>(f, s), m_force(force)
    {
    }

    bool m_force;
  };

  using AttributeList = std::vector<AttributeItem>;

  class Observation;
  using ObservationPtr = RefCountedPtr<Observation>;
  using ObservationPtrArray = dlib::array<ObservationPtr>;

  class Observation : public RefCounted
  {
  public:
    enum ELevel
    {
      NORMAL,
      WARNING,
      FAULT,
      UNAVAILABLE
    };

    static const unsigned int NumLevels = 4;
    static const std::string SLevels[];

  public:
    // Initialize with the data item reference, sequence number, time and value
    Observation(DataItem &dataItem, const std::string &time, const std::string &value,
                uint64_t sequence = 0);

    // Copy constructor
    Observation(const Observation &observation);

    Observation *deepCopy();
    Observation *deepCopyAndRemove(Observation *old);

    // Extract the component event data into a map
    const AttributeList &getAttributes();

    // Get the data item associated with this event
    DataItem *getDataItem() const { return m_dataItem; }

    // Get the value
    const std::string &getValue() const { return m_value; }
    ELevel getLevel();
    const std::string &getLevelString() { return SLevels[getLevel()]; }
    const std::string &getCode()
    {
      getAttributes();
      return m_code;
    }
    void normal();

    // Time series info...
    const std::vector<float> &getTimeSeries() const { return m_timeSeries; }
    bool isTimeSeries() const { return m_isTimeSeries; }
    int getSampleCount() const { return m_sampleCount; }
    const DataSet &getDataSet() const { return m_dataSet; }
    const std::string &getResetTriggered() const { return m_resetTriggered; }
    bool isDataSet() const { return m_dataItem->isDataSet(); }
    bool isUnavailable() const { return m_value == "UNAVAILABLE"; }

    uint64_t getSequence() const { return m_sequence; }

    void copySequence(const Observation *other) { m_sequence = other->m_sequence; }
    void setSequence(uint64_t sequence) { m_sequence = sequence; }

    const std::string &getDuration() const { return m_duration; }

    Observation *getFirst();
    Observation *getPrev() { return m_prev; }
    void getList(std::list<ObservationPtr> &list);
    void appendTo(Observation *event);
    Observation *find(const std::string &nativeCode);
    bool replace(Observation *oldObservation, Observation *newObservation);

    bool operator<(Observation &another) const
    {
      if ((*m_dataItem) < (*another.m_dataItem))
        return true;
      else if (*m_dataItem == *another.m_dataItem)
        return m_sequence < another.m_sequence;
      else
        return false;
    }

    void clearResetTriggered()
    {
      if (!m_resetTriggered.empty())
      {
        m_hasAttributes = false;
        m_attributes.clear();
        m_resetTriggered.clear();
      }
    }

    void setDataSet(DataSet &aSet)
    {
      m_dataSet = aSet;
      m_hasAttributes = false;
      m_attributes.clear();
    }

  protected:
    // Virtual destructor
    ~Observation() override = default;

  protected:
    // Holds the data item from the device
    DataItem *m_dataItem;

    // Sequence number of the event
    uint64_t m_sequence;
    std::string m_sequenceStr;

    // Timestamp of the event's occurence
    std::string m_time;
    std::string m_duration;

    // Hold the alarm data:  CODE|NATIVECODE|SEVERITY|STATE
    // or the Conditon data: LEVEL|NATIVE_CODE|NATIVE_SEVERITY|QUALIFIER
    // or the message data:  NATIVE_CODE
    // or the time series data
    std::string m_rest;
    ELevel m_level;

    // The value of the event, either as a float or a string
    std::string m_value;
    bool m_isFloat;
    bool m_isTimeSeries;
    std::vector<float> m_timeSeries;
    int m_sampleCount;

    // The attributes, created on demand
    bool m_hasAttributes;
    AttributeList m_attributes;

    // For condition tracking
    std::string m_code;

    // For reset triggered.
    std::string m_resetTriggered;

    // For back linking of condition
    ObservationPtr m_prev;

    // For data sets
    DataSet m_dataSet;

  protected:
    // Convert the value to the agent unit standards
    void convertValue(const std::string &value);
  };

  inline Observation::ELevel Observation::getLevel()
  {
    if (!m_hasAttributes)
      getAttributes();

    return m_level;
  }

  inline void Observation::appendTo(Observation *event) { m_prev = event; }

  using ObservationComparer = bool (*)(ObservationPtr &, ObservationPtr &);
  inline bool ObservationCompare(ObservationPtr &aE1, ObservationPtr &aE2) { return aE1 < aE2; }

}  // namespace mtconnect
