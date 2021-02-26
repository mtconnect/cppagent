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

#include "composition.hpp"
#include "configuration/configuration.hpp"
#include "utilities.hpp"

#include <list>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace mtconnect
{
  namespace device_model
  {
    namespace data_item
    {
      class DataItem;
    }
  }  // namespace device_model
  class Device;
  class Agent;
  using DataItemPtr = std::shared_ptr<device_model::data_item::DataItem>;
  using namespace device_model::configuration;
  class Component
  {
  public:
    struct Reference
    {
      enum ReferenceType
      {
        DATA_ITEM,
        COMPONENT
      };

      Reference(std::string id, std::string name, ReferenceType type)
        : m_type(type),
          m_id(std::move(id)),
          m_name(std::move(name)),
          m_dataItem(nullptr),
          m_component(nullptr)
      {
      }

      ReferenceType m_type;
      std::string m_id;
      std::string m_name;
      DataItemPtr m_dataItem;
      Component *m_component;
    };

  public:
    // Take in a class name & mapping of attributes
    Component(const std::string &className, const std::map<std::string, std::string> &attributes,
              const std::string &prefix = "");

    // Virtual destructor
    virtual ~Component();

    // Return a map of attributes of all the component specs
    const std::map<std::string, std::string> &getAttributes() { return m_attributes; }

    // Return what part of the component it is
    const std::string &getClass() const { return m_class; }
    const std::string &getPrefixedClass() const { return m_prefixedClass; }

    // Getter methods for the component ID/Name
    const std::string &getId() const { return m_id; }
    const std::string &getName() const { return m_name; }
    const std::string &getNativeName() const { return m_nativeName; }
    const std::string &getUuid() const { return m_uuid; }
    const std::string &getDescriptionBody() const { return m_descriptionBody; }
    const std::string &getPrefix() const { return m_prefix; }
    
    const auto &getConfiguration() const { return m_configuration; }
    void setConfiguration(std::unique_ptr<Configuration> &conf) { m_configuration = std::move(conf); }

    // Setter methods
    void setUuid(const std::string &uuid)
    {
      m_uuid = uuid;
      reBuildAttributes();
    }
    void setManufacturer(const std::string &manufacturer)
    {
      m_description["manufacturer"] = manufacturer;
    }
    void setSerialNumber(const std::string &serialNumber)
    {
      m_description["serialNumber"] = serialNumber;
    }
    void setStation(const std::string &station) { m_description["station"] = station; }
    void setDescription(const std::string &description) { m_descriptionBody = description; }
    void setNativeName(const std::string &nativeName)
    {
      m_nativeName = nativeName;
      reBuildAttributes();
    }

    // Add/get description specifications using an attribute map
    void addDescription(std::string body, const std::map<std::string, std::string> &attributes);
    const std::map<std::string, std::string> &getDescription() const { return m_description; }

    // Get the device that any component is associated with
    Device *getDevice();

    // Set/Get the component's parent component
    Component *getParent() const { return m_parent; }

    // Add to/get the component's std::list of children
    void addChild(Component *child)
    {
      child->setParent(this);
      m_children.emplace_back(child);
    }
    std::list<Component *> &getChildren() { return m_children; }
    
    const auto &getCompositions() const { return m_compositions; }
    void setCompositions(std::unique_ptr<Composition> &comp) { m_compositions = std::move(comp); }

    // Add to/get the component's std::list of data items
    virtual void addDataItem(DataItemPtr dataItem);
    const std::list<DataItemPtr> &getDataItems() const { return m_dataItems; }

    bool operator<(const Component &comp) const { return m_id < comp.getId(); }
    bool operator==(const Component &comp) const { return m_id == comp.getId(); }

    // References
    void addReference(Reference &reference) { m_references.emplace_back(reference); }
    const auto &getReferences() const { return m_references; }

    void resolveReferences();

  protected:
    // Return a map of attributes of all the component specs
    std::map<std::string, std::string> buildAttributes() const;
    void reBuildAttributes() { m_attributes = buildAttributes(); }
    void setParent(Component *parent);

  protected:
    // Unique ID for each component
    std::string m_id;

    // Name for itself
    std::string m_name;
    std::string m_nativeName;

    // The class
    std::string m_class;
    std::string m_prefix;
    std::string m_prefixedClass;

    // Universal unique identifier
    std::string m_uuid;

    // If receiving data, a sample rate is needed
    double m_sampleInterval;

    // Description of itself
    std::map<std::string, std::string> m_description;
    std::string m_descriptionBody;
    std::unique_ptr<Configuration> m_configuration;

    // Component relationships
    // Pointer to the parent component
    Component *m_parent;
    Device *m_device;

    // Each component keeps track of it's children in a std::list
    std::list<Component *> m_children;

    // Keep Track of all the data items associated with this component
    std::list<DataItemPtr> m_dataItems;

    // List of all the compositions
    std::unique_ptr<Composition> m_compositions;

    // The set of attribtues
    std::map<std::string, std::string> m_attributes;

    // References
    std::list<Reference> m_references;
  };

  struct ComponentComp
  {
    bool operator()(const Component *lhs, const Component *rhs) const { return *lhs < *rhs; }
  };
}  // namespace mtconnect
