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
#include <date/date.h>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "mtconnect/config.hpp"
#include "mtconnect/device_model/component.hpp"
#include "mtconnect/device_model/data_item/data_item.hpp"
#include "mtconnect/entity/entity.hpp"
#include "mtconnect/utilities.hpp"

/// @brief Observation namespace
namespace mtconnect::observation {
  class Observation;
  using ObservationPtr = std::shared_ptr<Observation>;
  using ConstObservationPtr = std::shared_ptr<const Observation>;
  using ObservationList = std::list<ObservationPtr>;

  /// @brief Abstract observation
  class AGENT_LIB_API Observation : public entity::Entity
  {
  public:
    using super = entity::Entity;
    using entity::Entity::Entity;

    static entity::FactoryPtr getFactory();
    ~Observation() override = default;
    virtual ObservationPtr copy() const { return std::make_shared<Observation>(); }

    /// @brief Method to create an observation for a data item
    ///
    /// This method should always be used instead of the constructor.
    ///
    /// @param[in] dataItem related data item
    /// @param[in] props properties
    /// @param[in] timestamp the timestamp
    /// @param[in,out] errors any errors that occurred when creating the observation
    /// @return shared pointer to the observations
    static ObservationPtr make(const DataItemPtr dataItem, const entity::Properties &props,
                               const Timestamp &timestamp, entity::ErrorList &errors);

    /// @brief utility method to copy the properties from a data item to a set of properties
    /// @param[in] dataItem the data item
    /// @param[out] props properties to recieve data item properties
    static void setProperties(const DataItemPtr dataItem, entity::Properties &props)
    {
      for (auto &prop : dataItem->getObservationProperties())
        props.emplace(prop);
    }

    /// @brief set the associated data item and its properties
    /// @param[in] dataItem the data item
    void setDataItem(const DataItemPtr dataItem)
    {
      m_dataItem = dataItem;
      setProperties(dataItem, m_properties);
    }

    /// @brief get the associated data item
    /// @return shared pointer to the data item
    const auto getDataItem() const { return m_dataItem.lock(); }
    /// @brief get the sequence number of the observation
    /// @return the sequence number
    auto getSequence() const { return m_sequence; }

    /// @brief update related data item when the device is updated
    /// @param[in] diMap a map of data item ids to data items
    void updateDataItem(std::unordered_map<std::string, WeakDataItemPtr> &diMap)
    {
      auto old = m_dataItem.lock();
      auto ndi = diMap.find(old->getId());
      if (ndi != diMap.end())
        m_dataItem = ndi->second;
      else
        LOG(trace) << "Observation cannot find data item: " << old->getId();
    }

    /// @brief set the timestamp
    /// @param[in] ts the timestamp
    void setTimestamp(const Timestamp &ts)
    {
      m_timestamp = ts;
      setProperty("timestamp", m_timestamp);
    }
    /// @brief get the timestamp
    /// @return the timestamp
    auto getTimestamp() const { return m_timestamp; }

    /// @brief set the sequence number
    /// @param[in] sequence the sequence number
    void setSequence(int64_t sequence)
    {
      m_sequence = sequence;
      setProperty("sequence", sequence);
    }
    /// @brief make the observation unavailable
    virtual void makeUnavailable()
    {
      using namespace std::literals;
      m_unavailable = true;
      setProperty("VALUE", "UNAVAILABLE"s);
    }
    /// @brief get the unavailable state
    /// @return `true` if unavailable
    bool isUnavailable() const { return m_unavailable; }
    /// @brief set the entity name (QName) from the data item observation name
    virtual void setEntityName()
    {
      auto di = m_dataItem.lock();
      if (di)
        Entity::setQName(di->getObservationName());
    }
    /// @brief Compare observations
    ///
    /// compare by the data item and then by sequence number
    /// @param[in] another the other observation
    /// @return `true` if this observation is less than `another`
    bool operator<(const Observation &another) const
    {
      auto di = m_dataItem.lock();
      if (!di)
        return false;
      auto odi = another.m_dataItem.lock();
      if (!odi)
        return true;

      if ((*di) < (*odi))
        return true;
      else if (*di == *odi)
        return m_sequence < another.m_sequence;
      else
        return false;
    }

    /// @brief check if the data item has been removed when device is updated
    /// @return `true` if the observation is no longer viable
    bool isOrphan() const
    {
#ifdef NDEBUG
      return m_dataItem.expired();
#else
      if (m_dataItem.expired())
        return true;
      if (m_dataItem.lock()->isOrphan())
      {
        auto di = m_dataItem.lock();
        LOG(trace) << "!!! DataItem " << di->getTopicName() << " orphaned";
        return true;
      }
      return false;
#endif
    }

    /// @brief Clear the reset triggered state
    void clearResetTriggered() { m_properties.erase("resetTriggered"); }

  protected:
    Timestamp m_timestamp;
    bool m_unavailable {false};
    std::weak_ptr<device_model::data_item::DataItem> m_dataItem;
    uint64_t m_sequence {0};
  };

  /// @brief A MTConnect Sample with a double value
  class AGENT_LIB_API Sample : public Observation
  {
  public:
    using super = Observation;

    using Observation::Observation;
    static entity::FactoryPtr getFactory();
    ~Sample() override = default;

    ObservationPtr copy() const override { return std::make_shared<Sample>(*this); }
  };

  /// @brief An MTConnect Sample with a Vector with three values for X, Y and Z, or A, B, and C.
  class AGENT_LIB_API ThreeSpaceSample : public Sample
  {
  public:
    using super = Sample;

    using Sample::Sample;
    static entity::FactoryPtr getFactory();
    ~ThreeSpaceSample() override = default;
  };

  /// @brief A vector of timeseries values with a count and duration
  class AGENT_LIB_API Timeseries : public Sample
  {
  public:
    using super = Sample;

    using Sample::Sample;
    static entity::FactoryPtr getFactory();
    ~Timeseries() override = default;

    ObservationPtr copy() const override { return std::make_shared<Timeseries>(*this); }
  };

  class Condition;
  using ConditionPtr = std::shared_ptr<Condition>;
  using ConditionList = std::list<ConditionPtr>;

  /// @brief An MTConnect Condition
  ///
  /// Conditions are linked together to keep track of all the conditions that are
  /// active at one time. When the normal condition arrives, the list is cleared.
  class AGENT_LIB_API Condition : public Observation
  {
  public:
    using super = Observation;

    /// @brief The Condition level
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
    ObservationPtr copy() const override { return std::make_shared<Condition>(*this); }

    ConditionPtr getptr() { return std::dynamic_pointer_cast<Condition>(Entity::getptr()); }

    /// @brief Set the conditions level
    /// @param[in] level the level
    void setLevel(Level level)
    {
      m_level = level;
      setEntityName();
    }

    /// @brief set the level as a string
    /// @param[in] s the level
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
        throw entity::PropertyError("Invalid Condition Level: " + s);
    }

    /// @brief Make this condition normal
    void normal()
    {
      m_level = NORMAL;
      m_code.clear();
      m_properties.erase("nativeCode");
      m_properties.erase("conditionId");
      m_properties.erase("nativeSeverity");
      m_properties.erase("qualifier");
      m_properties.erase("statistic");
      m_properties.erase("VALUE");
      setEntityName();
    }
    /// @brief Make this condition unavailable
    void makeUnavailable() override
    {
      m_unavailable = true;
      m_level = UNAVAILABLE;
      setEntityName();
    }
    /// @brief Using the level, set the QName of this Observation
    void setEntityName() override
    {
      switch (m_level)
      {
        case NORMAL:
          setQName("Normal");
          break;

        case WARNING:
          setQName("Warning");
          break;

        case FAULT:
          setQName("Fault");
          break;

        case UNAVAILABLE:
          setQName("Unavailable");
          break;
      }
    }

    /// @brief get the first condition in the chain
    ///
    /// Conditions are chained together to allow for mutiple conditions active at the same time
    /// @return shared pointer the first condition in the chain
    ConditionPtr getFirst()
    {
      if (m_prev)
        return m_prev->getFirst();

      return getptr();
    }

    /// @brief Get a list of all active conditions
    /// @param[out] list the list condtions
    void getConditionList(ConditionList &list)
    {
      if (m_prev)
        m_prev->getConditionList(list);

      list.emplace_back(getptr());
    }

    /// @brief find a condition by code in the condition list
    /// @param[in] code te code
    /// @return shared pointer to the condition if found
    ConditionPtr find(const std::string &code)
    {
      if (m_code == code)
        return getptr();

      if (m_prev)
        return m_prev->find(code);

      return nullptr;
    }

    /// @brief const find a condition by code in the condition list
    /// @param[in] code te code
    /// @return shared pointer to the condition if found
    const ConditionPtr find(const std::string &code) const
    {
      if (m_code == code)
        return std::dynamic_pointer_cast<Condition>(Entity::getptr());

      if (m_prev)
        return m_prev->find(code);

      return nullptr;
    }

    /// @brief replace a condition with another in the condition list
    /// @param[in] old the condition to be placed
    /// @param[in] _new the replacement condition
    /// @return `true` if the old condition was found
    bool replace(ConditionPtr &old, ConditionPtr &_new);
    /// @brief copy the condition and all conditions in the list
    /// @return a new shared condition pointer
    ConditionPtr deepCopy();
    /// @brief copy the condition and all conditions in the list removing one condition
    /// @param[in] old the condition to skip
    /// @return the new condition pointer
    ConditionPtr deepCopyAndRemove(ConditionPtr &old);

    /// @brief Get the code for the condition
    /// @return the code
    const std::string &getCode() const { return m_code; }
    /// @brief get the condition level
    /// @return the level
    Level getLevel() const { return m_level; }
    /// @brief get the previous condition in the list
    /// @return the previous condition if it exists
    ConditionPtr getPrev() const { return m_prev; }
    /// @brief make a condition as the previous condition linking it to the list
    /// @param[in] cond the previous condition
    void appendTo(ConditionPtr cond) { m_prev = cond; }

  protected:
    std::string m_code;
    Level m_level {NORMAL};
    ConditionPtr m_prev;
  };

  /// @brief an MTConnect Event with a string value or controlled vocabulary
  class AGENT_LIB_API Event : public Observation
  {
  public:
    using super = Observation;

    using Observation::Observation;
    static entity::FactoryPtr getFactory();
    ~Event() override = default;
    ObservationPtr copy() const override { return std::make_shared<Event>(*this); }
  };

  /// @brief An `Event` that has a double value
  class AGENT_LIB_API DoubleEvent : public Observation
  {
  public:
    using super = Observation;

    using Observation::Observation;
    static entity::FactoryPtr getFactory();
    ~DoubleEvent() override = default;
    ObservationPtr copy() const override { return std::make_shared<DoubleEvent>(*this); }
  };

  /// @brief An `Event` that has a integer value
  class AGENT_LIB_API IntEvent : public Observation
  {
  public:
    using super = Observation;

    using Observation::Observation;
    static entity::FactoryPtr getFactory();
    ~IntEvent() override = default;
    ObservationPtr copy() const override { return std::make_shared<IntEvent>(*this); }
  };

  /// @brief An `Event` that has a data set representation
  class AGENT_LIB_API DataSetEvent : public Event
  {
  public:
    using super = Event;

    using Event::Event;
    static entity::FactoryPtr getFactory();
    ~DataSetEvent() override = default;
    ObservationPtr copy() const override { return std::make_shared<DataSetEvent>(*this); }

    /// @brief makes the data set unavailable and sets the count to 0
    void makeUnavailable() override
    {
      super::makeUnavailable();
      setProperty("count", int64_t(0));
    }
    /// @brief get the data set value
    /// @return the value
    const entity::DataSet &getDataSet() const
    {
      const entity::Value &v = getValue();
      return std::get<entity::DataSet>(v);
    }
    /// @brief set the data set value and the count
    /// @param[in] set the data set
    void setDataSet(const entity::DataSet &set)
    {
      setValue(set);
      setProperty("count", int64_t(set.size()));
    }
  };

  using DataSetEventPtr = std::shared_ptr<DataSetEvent>;

  /// @brief An `Event` that has a table representation
  class AGENT_LIB_API TableEvent : public DataSetEvent
  {
  public:
    using DataSetEvent::DataSetEvent;
    static entity::FactoryPtr getFactory();
    ObservationPtr copy() const override { return std::make_shared<TableEvent>(*this); }
  };

  /// @brief An asset changed or removed Event
  class AGENT_LIB_API AssetEvent : public Event
  {
  public:
    using Event::Event;
    static entity::FactoryPtr getFactory();
    ~AssetEvent() override = default;
    ObservationPtr copy() const override { return std::make_shared<AssetEvent>(*this); }

  protected:
  };

  /// @brief Agent Device events for added, changed, and removed
  class AGENT_LIB_API DeviceEvent : public Event
  {
  public:
    using Event::Event;
    static entity::FactoryPtr getFactory();
    ~DeviceEvent() override = default;
    ObservationPtr copy() const override { return std::make_shared<DeviceEvent>(*this); }

  protected:
  };

  /// @brief A Message Event
  class AGENT_LIB_API Message : public Event
  {
  public:
    using super = Event;

    using Event::Event;
    static entity::FactoryPtr getFactory();
    ~Message() override = default;
    ObservationPtr copy() const override { return std::make_shared<Message>(*this); }
  };

  /// @brief A deprecated Alarm type.
  ///
  /// Alarms should not be used in modern MTConnect
  class AGENT_LIB_API Alarm : public Event
  {
  public:
    using super = Event;

    using Event::Event;
    static entity::FactoryPtr getFactory();
    ~Alarm() override = default;
    ObservationPtr copy() const override { return std::make_shared<Alarm>(*this); }
  };

  using ObservationComparer = bool (*)(ObservationPtr &, ObservationPtr &);
  inline bool ObservationCompare(ObservationPtr &aE1, ObservationPtr &aE2) { return *aE1 < *aE2; }
}  // namespace mtconnect::observation
