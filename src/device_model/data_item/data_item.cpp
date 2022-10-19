//
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

#include "data_item.hpp"

#include <boost/algorithm/string.hpp>

#include <array>
#include <map>
#include <string>

#include "device_model/device.hpp"
#include "entity/requirement.hpp"
#include "logging.hpp"

using namespace std;

namespace mtconnect {
  using namespace entity;
  namespace device_model::data_item {
    // -----------------------------

    FactoryPtr DataItem::getFactory()
    {
      static FactoryPtr dataItem;
      if (!dataItem)
      {
        auto source = Source::getFactory();
        auto filter = Filter::getFactory();
        auto relationships = Relationships::getFactory();
        auto definition = device_model::data_item::Definition::getFactory();
        auto constraints = Constraints::getFactory();
        FactoryPtr factory = make_shared<Factory>(Requirements {
            // Attributes
            {"id", true},
            {"name", false},
            {"type", true},
            {"subType", QSTRING, false},
            {"category", ControlledVocab {"EVENT", "SAMPLE", "CONDITION"}, true},
            {"discrete", BOOL, false},
            {"representation",
             ControlledVocab {"VALUE", "TIME_SERIES", "DATA_SET", "TABLE", "DISCRETE"}, false},
            {"units", false},
            {"nativeUnits", false},
            {"sampleRate", DOUBLE, false},
            {"statistic", false},
            {"nativeScale", DOUBLE, false},
            {"coordinateSystem", ControlledVocab {"MACHINE", "WORK"}, false},
            {"compositionId", false},
            {"coordinateSystemIdRef", false},
            {"significantDigits", INTEGER, false},
            // Elements
            {"Source", ENTITY, source, false},
            {"Filters", ENTITY_LIST, filter, false},
            {"Definition", ENTITY, definition, false},
            {"Constraints", ENTITY_LIST, constraints, false},
            {"Relationships", ENTITY_LIST, relationships, false},
            {"InitialValue", DOUBLE, false},
            {"ResetTrigger", false}});
        factory->setFunction([](const std::string &name, Properties &props) -> EntityPtr {
          auto ptr = make_shared<DataItem>(name, props);
          return dynamic_pointer_cast<Entity>(ptr);
        });

        factory->setOrder({"Source", "Constraints", "Filters", "InitialValue", "ResetTriger",
                           "Definition", "Relationships"});
        dataItem = make_shared<Factory>(
            Requirements {{"DataItem", ENTITY, factory, 1, Requirement::Infinite}});
      }

      return dataItem;
    }

    FactoryPtr DataItem::getRoot()
    {
      static FactoryPtr root;
      if (!root)
      {
        auto factory = DataItem::getFactory();
        root = make_shared<Factory>(Requirements {{"DataItems", ENTITY_LIST, factory, false}});
      }
      return root;
    }

    // DataItem public methods
    DataItem::DataItem(const string &name, const Properties &props) : Entity(name, props)
    {
      NAMED_SCOPE("data_item");

      static const char *samples = "Samples";
      static const char *events = "Events";
      static const char *condition = "Condition";

      m_id = get<string>("id");
      m_name = maybeGet<string>("name");
      auto type = get<string>("type");
      optional<string> pre;
      string obs = pascalize(type, pre);
      optional<string> repPre;
      if (auto rep = maybeGet<string>("representation"); rep && *rep != "VALUE")
        obs += pascalize(*rep, repPre);

      m_observationName.setQName(obs, pre);

      const static unordered_map<string, ERepresentation> reps = {{"VALUE", VALUE},
                                                                  {"TIME_SERIES", TIME_SERIES},
                                                                  {"DISCRETE", DISCRETE},
                                                                  {"DATA_SET", DATA_SET},
                                                                  {"TABLE", TABLE}};
      auto rep = maybeGet<string>("representation");
      if (rep)
        m_representation = reps.find(*rep)->second;

      auto &category = get<string>("category");

      auto units = maybeGet<string>("units");
      if (units && ends_with(*units, "3D"))
        m_specialClass = THREE_SPACE_CLS;

      if (category == "SAMPLE")
      {
        m_category = SAMPLE;
        m_categoryText = samples;
      }
      else if (category == "EVENT")
      {
        m_category = EVENT;
        m_categoryText = events;

        if (type == "ALARM")
          m_specialClass = ALARM_CLS;
        else if (type == "MESSAGE")
          m_specialClass = MESSAGE_CLS;
        else if (type == "ASSET_REMOVED")
          m_specialClass = ASSET_REMOVED_CLS;
        else if (type == "ASSET_CHANGED")
          m_specialClass = ASSET_CHANGED_CLS;
      }
      else if (category == "CONDITION")
      {
        m_category = CONDITION;
        m_categoryText = condition;
        m_specialClass = CONDITION_CLS;
      }

      std::optional<std::string> pref;
      auto source = maybeGet<entity::EntityPtr>("Source");
      if (source && (*source)->hasValue())
        pref = m_source = (*source)->getValue<string>();
      else
        pref = m_name.value_or(m_id);
      m_preferredName = *pref;

      m_discrete =
          m_representation == DISCRETE || (hasProperty("discrete") && get<bool>("discrete"));

      m_observatonProperties.insert_or_assign("dataItemId", m_id);
      if (m_name)
        m_observatonProperties.insert_or_assign("name", *m_name);
      if (hasProperty("compositionId"))
        m_observatonProperties.insert_or_assign("compositionId", get<std::string>("compositionId"));
      if (hasProperty("subType"))
        m_observatonProperties.insert_or_assign("subType", get<std::string>("subType"));
      if (hasProperty("statistic"))
        m_observatonProperties.insert_or_assign("statistic", get<std::string>("statistic"));
      if (isCondition())
        m_observatonProperties.insert_or_assign("type", get<std::string>("type"));

      if (const auto &cons = getList("Constraints"); cons && cons->size() == 1)
      {
        auto &con = cons->front();
        if (con->getName() == "Value")
          m_constantValue = con->getValue<string>();

        // Check for legacy filters
        for (auto &c : *cons)
        {
          if (c->getName() == "Filter")
          {
            if (!c->hasProperty("type"))
              c->setProperty("type", "MINIMUM_DELTA"s);

            // Legacy filter only supported minimum delta.
            m_minimumDelta = c->getValue<double>();
          }
        }
      }

      if (const auto &filters = getList("Filters"))
      {
        for (auto &filter : *filters)
        {
          const auto &type = filter->get<string>("type");
          if (type == "MINIMUM_DELTA")
            m_minimumDelta = filter->getValue<double>();
          else if (type == "PERIOD")
            m_minimumPeriod = filter->getValue<double>();
        }
      }

      if (hasProperty("nativeUnits"))
      {
        if (!hasProperty("units"))
          throw PropertyError("nativeUnits given, but no units for " + m_id);
        m_converter = UnitConversion::make(get<string>("nativeUnits"), get<string>("units"));
      }
      if (hasProperty("nativeScale"))
      {
        if (!m_converter)
          m_converter = make_unique<UnitConversion>();
        m_converter->scale(get<double>("nativeScale"));
      }

      m_key = string(m_categoryText) + ":" + m_observationName;
      if (!isDataSet())
      {
        if (m_specialClass == THREE_SPACE_CLS)
          m_key += ":3D";
        else if (m_category == EVENT && hasProperty("units"))
        {
          if (this->get<string>("units") == "COUNT")
            m_key += ":INT";
          else
            m_key += ":DOUBLE";
        }
      }
    }

    bool DataItem::hasName(const string &name) const
    {
      return m_id == name || (m_name && *m_name == name) || (m_source && *m_source == name);
    }

    // Sort by: Device, Component, Category, DataItem
    bool DataItem::operator<(const DataItem &another) const
    {
      auto component = m_component.lock();
      if (component == nullptr)
        return true;
      auto dev = component->getDevice();
      if (dev == nullptr)
        return true;

      auto otherComponent = another.getComponent();
      if (otherComponent == nullptr)
        return false;
      auto otherDev = otherComponent->getDevice();
      if (otherDev == nullptr)
        return false;

      if (dev->getId() < otherDev->getId())
        return true;
      else if (dev->getId() > otherDev->getId())
        return false;
      else if (component->getId() < otherComponent->getId())
        return true;
      else if (component->getId() > otherComponent->getId())
        return false;
      else if (m_category < another.m_category)
        return true;
      else if (m_category == another.m_category)
        return m_id < another.m_id;
      else
        return false;
    }

    void DataItem::setConstantValue(const std::string &value)
    {
      ErrorList errors;
      Properties url {{"VALUE", value}};
      auto v = Constraints::getFactory()->create("Value", url, errors);
      if (!errors.empty())
      {
        LOG(error) << "Cannot set constant value for data item " << m_id << " to " << value;
        for (auto &e : errors)
          LOG(error) << e->what();
      }
      else
      {
        addToList("Constraints", getFactory()->factoryFor("DataItem"), v, errors);
        m_constantValue = value;
      }
    }

    inline void path(std::list<string> &pth, ComponentPtr c)
    {
      auto p = c->getParent();
      if (p)
        path(pth, p);

      pth.push_back(c->getTopicName());
    }

    void DataItem::makeTopic()
    {
      std::list<string> pth;
      auto comp = m_component.lock();

      path(pth, comp);
      {
        auto cp = m_composition.lock();
        if (cp)
          pth.push_back(cp->getTopicName());
      }
      pth.push_back(m_categoryText);

      stringstream name;
      name << getObservationName();
      optional<string> opt;
      auto sub = maybeGet<string>("subType");
      if (sub)
        name << '.' << pascalize(*sub, opt);
      if (m_name)
        name << '[' << *m_name << ']';
      m_topicName = name.str();
      pth.push_back(m_topicName);

      m_topic = boost::algorithm::join(pth, "/");
    }
  }  // namespace device_model::data_item
}  // namespace mtconnect
