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

#include "component_configuration.hpp"
#include "globals.hpp"

#include <utility>
#include <vector>

namespace mtconnect
{
  struct Relationship
  {
    Relationship() = default;
    Relationship(const Relationship &r) = default;
    virtual ~Relationship() = default;

    std::string m_id;
    std::string m_type;
    std::string m_name;
    std::string m_criticality;
  };
  
  struct ComponentRelationship : public Relationship
  {
    ComponentRelationship() = default;
    ComponentRelationship(const ComponentRelationship &r) = default;
    virtual ~ComponentRelationship() = default;

    std::string m_idRef;
  };
  
  struct DeviceRelationship  : public Relationship
  {
    DeviceRelationship() = default;
    DeviceRelationship(const DeviceRelationship &r) = default;
    virtual ~DeviceRelationship() = default;
    
    std::string m_deviceUuidRef;
    std::string m_role;
    std::string m_href;
  };

  class Relationships : public ComponentConfiguration
  {
   public:
    Relationships() = default;
    virtual ~Relationships() = default;
    
    void addRelationship(Relationship *r)
    {
      m_relationships.emplace_back(std::move(r));
    }
    void addRelationship(std::unique_ptr<Relationship> &r)
    {
      m_relationships.emplace_back(std::move(r));
    }

    const std::list<std::unique_ptr<Relationship>> &getRelationships() const
    {
      return m_relationships;
    }
    
   protected:
    std::list<std::unique_ptr<Relationship>> m_relationships;
  };
}  // namespace mtconnect
