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

#pragma once

#include <map>

#include "constraints.hpp"
#include "definition.hpp"
#include "filter.hpp"
#include "mtconnect/config.hpp"
#include "mtconnect/device_model/component.hpp"
#include "mtconnect/observation/change_observer.hpp"
#include "mtconnect/utilities.hpp"
#include "relationships.hpp"
#include "source.hpp"
#include "unit_conversion.hpp"

#ifdef PASCAL
#undef PASCAL
#endif

namespace mtconnect {
  namespace source::adapter {
    class Adapter;
  }
  namespace device_model {
    class Composition;

    /// @brief DataItem related entities
    namespace data_item {
      /// @brief Data Item entity
      class AGENT_LIB_API DataItem : public entity::Entity, public observation::ChangeSignaler
      {
      public:
        /// @brief MTConnect DataItem Enumeration for category
        enum Category
        {
          SAMPLE,
          EVENT,
          CONDITION
        };

        /// @brief MTConnect DataItem Enumeration for representation
        enum Representation
        {
          VALUE,
          TIME_SERIES,
          DISCRETE,
          DATA_SET,
          TABLE
        };

        /// @brief MTConnect DataItem classifications
        enum SpecialClass
        {
          CONDITION_CLS,
          MESSAGE_CLS,
          ALARM_CLS,
          THREE_SPACE_CLS,
          NONE_CLS,
          ASSET_REMOVED_CLS,
          ASSET_CHANGED_CLS
        };

      public:
        /// @brief constructor for a data item. name is always `DataItem`.
        ///
        /// @note Do not use this method directly. Use the `make()` method.
        DataItem(const std::string &name, const entity::Properties &props);
        static entity::FactoryPtr getFactory();
        static entity::FactoryPtr getRoot();

        /// @brief make method to create
        /// @param[in] props data item properties
        /// @param[in,out] errors list of errors creating the data item
        /// @return shared pointer to DataItem
        static std::shared_ptr<DataItem> make(const entity::Properties &props,
                                              entity::ErrorList &errors)
        {
          entity::Properties ps(props);
          auto ptr = getFactory()->create("DataItem", ps, errors);
          return std::dynamic_pointer_cast<DataItem>(ptr);
        }

        // Destructor
        ~DataItem() override = default;

        /// @name Cached transformed and derived property access methods
        ///@{
        const auto &getId() const { return m_id; }
        const auto &getName() const { return m_name; }
        const auto &getSource() const { return get<entity::EntityPtr>("Source"); }

        /// @brief get the name or the id of the data item
        /// @return the preferred name
        const auto &getPreferredName() const { return m_preferredName; }
        const auto &getMinimumDelta() const { return m_minimumDelta; }
        const auto &getMinimumPeriod() const { return m_minimumPeriod; }
        /// @brief get a key related to the data item for creating observations
        /// @return a key
        const auto &getKey() const { return m_key; }
        const auto &getType() { return get<std::string>("type"); }
        const auto &getSubType() { return get<std::string>("subType"); }

        /// @brief get the pascalized name for the data item when represented as a observation
        /// @return observation name
        const auto &getObservationName() const { return m_observationName; }
        /// @brief get the properties to build an observation
        /// @return observation properties
        const auto &getObservationProperties() const { return m_observatonProperties; }

        /// @brief get the topic with the path
        /// @return data item topic
        const auto &getTopic() const { return m_topic; }
        /// @brief get the topic name leaf node for this data item
        /// @return the topic name
        const auto &getTopicName() const { return m_topicName; }

        Category getCategory() const { return m_category; }
        Representation getRepresentation() const { return m_representation; }
        SpecialClass getSpecialClass() const { return m_specialClass; }

        const auto &getConstantValue() const { return m_constantValue; }
        ///@}

        /// @brief make this data item a constant
        /// @param[in] value constant value
        void setConstantValue(const std::string &value);

        /// @name boolean methods to interigate the data item
        ///@{

        bool hasName(const std::string &name) const;
        bool isSample() const { return m_category == SAMPLE; }
        bool isEvent() const { return m_category == EVENT; }
        bool isCondition() const { return m_category == CONDITION; }
        bool isAlarm() const { return m_specialClass == ALARM_CLS; }
        bool isMessage() const { return m_specialClass == MESSAGE_CLS; }
        bool isAssetChanged() const { return m_specialClass == ASSET_CHANGED_CLS; }
        bool isAssetRemoved() const { return m_specialClass == ASSET_REMOVED_CLS; }
        bool isTimeSeries() const { return m_representation == TIME_SERIES; }
        bool isDiscreteRep() const { return m_representation == DISCRETE; }
        bool isTable() const { return m_representation == TABLE; }
        bool isDataSet() const { return m_representation == DATA_SET || isTable(); }
        bool isDiscrete() const { return m_discrete; }
        bool isThreeSpace() const { return m_specialClass == THREE_SPACE_CLS; }
        bool isOrphan() const { return m_component.expired(); }
        ///@}

        void makeDiscrete()
        {
          setProperty("discrete", true);
          m_discrete = true;
        }

        /// @brief Build the topic
        void makeTopic();

        // Value converter
        const auto &getConverter() const { return m_converter; }
        void setConverter(const UnitConversion &conv)
        {
          m_converter = std::make_unique<UnitConversion>(conv);
        }

        /// @brief Set the associated component
        /// @param[in] the component
        void setComponent(ComponentPtr component)
        {
          m_component = component;
          auto cid = maybeGet<std::string>("compositionId");
          if (cid)
          {
            m_composition = component->getComposition(*cid);
          }
        }
        /// @brief get the associated component
        /// @return shared pointer to the component
        ComponentPtr getComponent() const { return m_component.lock(); }
        /// @brief get the associated composition
        /// @return shared pointer to the composition
        CompositionPtr getComposition() const { return m_composition.lock(); }

        /// @brief get the preferred name
        /// @return the preferred name
        const std::string &getSourceOrName() const { return m_preferredName; }

        /// @brief get the source
        /// @return source if available
        const std::optional<std::string> &getDataSource() const { return m_dataSource; }
        /// @brief set the data source
        /// @param[in] source the source
        void setDataSource(const std::string &source) { m_dataSource = source; }
        /// @brief set the topic for the data item
        /// @param[in] topic the topic
        void setTopic(const std::string &topic) { m_topic = topic; }

        bool operator<(const DataItem &another) const;
        bool operator==(const DataItem &another) const { return m_id == another.m_id; }

        const char *getCategoryText() const { return m_categoryText; }

      protected:
        double simpleFactor(const std::string &units);
        std::map<std::string, std::string> buildAttributes() const;

      protected:
        // Unique ID for each component
        std::string m_id;

        // Name for itself
        std::optional<std::string> m_name;
        std::optional<std::string> m_source;
        std::string m_preferredName;
        std::optional<std::string> m_constantValue;
        std::optional<double> m_minimumDelta;
        std::optional<double> m_minimumPeriod;
        std::string m_key;
        std::string m_topic;
        std::string m_topicName;

        // Category of data item
        Category m_category;
        const char *m_categoryText;

        // Type for observation
        entity::QName m_observationName;
        entity::Properties m_observatonProperties;

        // Representation of data item
        Representation m_representation {VALUE};
        SpecialClass m_specialClass {NONE_CLS};
        bool m_discrete;

        // The reset trigger;
        std::string m_resetTrigger;

        // Initial value
        std::string m_initialValue;

        // Component that data item is associated with
        std::weak_ptr<Component> m_component;
        std::weak_ptr<Composition> m_composition;

        // The data source for this data item
        std::optional<std::string> m_dataSource;

        // Conversions
        std::unique_ptr<UnitConversion> m_converter;
      };

      using DataItemPtr = std::shared_ptr<DataItem>;
    }  // namespace data_item
  }    // namespace device_model
  using DataItemPtr = std::shared_ptr<device_model::data_item::DataItem>;
  using WeakDataItemPtr = std::weak_ptr<device_model::data_item::DataItem>;

}  // namespace mtconnect
