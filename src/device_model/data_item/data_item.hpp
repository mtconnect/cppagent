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
#include "constraints.hpp"
#include "definition.hpp"
#include "filter.hpp"
#include "relationships.hpp"
#include "source.hpp"
#include "definition.hpp"
#include "device_model/component.hpp"
#include "entity.hpp"
#include "observation/change_observer.hpp"
#include "utilities.hpp"

#include <dlib/threads.h>

#include <map>

#ifdef PASCAL
#undef PASCAL
#endif

namespace mtconnect
{
  namespace adapter
  {
    class Adapter;
  }
  namespace device_model
  {
    namespace data_item
    {
      class DataItem : public entity::Entity, public observation::ChangeSignaler
      {
      public:
        // Enumeration for data item category
        enum ECategory
        {
          SAMPLE,
          EVENT,
          CONDITION
        };
        
        enum ERepresentation
        {
          VALUE,
          TIME_SERIES,
          DISCRETE,
          DATA_SET,
          TABLE
        };
        
        enum SpecialClass
        {
          CONDITION_CLS, MESSAGE_CLS,
          ALARM_CLS, THREE_SPACE_CLS, NONE_CLS,
          ASSET_REMOVED_CLS, ASSET_CHANGED_CLS
        };
        
      public:
        // Construct a data item with appropriate attributes mapping
        DataItem(const std::string &name, const entity::Properties &props);
        static entity::FactoryPtr getFactory();
        static std::shared_ptr<DataItem> make(const entity::Properties &props,
                                              entity::ErrorList &errors)
        {
          entity::Properties ps(props);
          auto ptr = getFactory()->make("DataItem", ps, errors);
          return std::dynamic_pointer_cast<DataItem>(ptr);
        }
        
        // Destructor
        ~DataItem() override = default;
        
        // Getter methods for data item specs
        const auto &getId() const { return m_id; }
        const auto &getName() const { return m_name; }
        const auto &getSource() const { return m_source; }
        const auto &getPrefixedObservationType() const { return m_prefixedObservationType; }
        const auto &getObservationType() const { return m_observationType; }
        const auto &getObservationProperties() const { return m_observatonProperties; }
        const auto &getMinimumDelta() const { return m_minimumDelta; }
        const auto &getMinimumPeriod() const { return m_minimumPeriod; }
        bool hasName(const std::string &name) const;
        
        const auto &getType() { return get<std::string>("type"); }
        
        ECategory getCategory() const { return m_category; }
        ERepresentation getRepresentation() const { return m_representation; }
        SpecialClass getSpecialClass() const { return m_specialClass; }
        
        const auto &getConstantValue() const { return m_constantValue; }
        void setConversionFactor(double factor, double offset);
        
        // Returns true if data item is a sample
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
        
        void makeDiscrete()
        {
          setProperty("discrete", true);
          m_discrete = true;
        }
        
        // Set/get component that data item is associated with
        void setComponent(Component &component) { m_component = &component; }
        Component *getComponent() const { return m_component; }
        
        // Get the name for the adapter feed
        const std::string &getSourceOrName() const
        {
          return m_preferredName;
        }
        
        const std::optional<std::string> &getDataSource() const { return m_dataSource; }
        void setDataSource(const std::string &source) { m_dataSource = source; }
        
        bool operator<(const DataItem &another) const;
        bool operator==(const DataItem &another) const { return m_id == another.m_id; }
        
        const char *getCategoryText() const
        {
          return m_categoryText;
        }
        
      protected:
        double simpleFactor(const std::string &units);
        std::map<std::string, std::string> buildAttributes() const;
        void computeConversionFactors();
        
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

        
        // Category of data item
        ECategory m_category;
        const char *m_categoryText;
        
        // Type for observation
        std::string m_observationType;
        std::string m_prefixedObservationType;
        std::optional<std::string> m_prefix;
        entity::Properties m_observatonProperties;
        
        // Representation of data item
        ERepresentation m_representation {VALUE};
        SpecialClass m_specialClass {NONE_CLS};
        bool m_discrete;
        
        // The reset trigger;
        std::string m_resetTrigger;
        
        // Initial value
        std::string m_initialValue;
        
        // Component that data item is associated with
        Component *m_component;
        
        // The data source for this data item
        std::optional<std::string> m_dataSource;
      };
      
      using DataItemPtr = std::shared_ptr<DataItem>;
    }
  }
  using DataItemPtr = std::shared_ptr<device_model::data_item::DataItem>;

}  // namespace mtconnect
