//
// Copyright Copyright 2012, System Insights, Inc.
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

#include <sstream>
#include <string>
#include <list>
#include <map>
#include <memory>

#include "globals.hpp"

class Composition
{
public:
  class Description {
  public:
    Description(const std::string &aBody, const std::string &aManufacturer, const std::string &aModel, const std::string &aSerialNumber,
                const std::string &aStation) : m_body(aBody), m_manufacturer(aManufacturer), m_model(aModel), m_serialNumber(aSerialNumber),
                m_station(aStation), m_hasAttributes(false) {}
    Description(const Description &anObj) : m_body(anObj.m_body), m_manufacturer(anObj.m_manufacturer), m_model(anObj.m_model),
                m_serialNumber(anObj.m_serialNumber), m_station(anObj.m_station), m_hasAttributes(false) {}
    Description(const std::string &aBody, const std::map<std::string,std::string> &aAttributes)
      : m_body(aBody), m_hasAttributes(false)
    {
      if (aAttributes.count("manufacturer") > 0) m_manufacturer = aAttributes.at("manufacturer");
      if (aAttributes.count("model") > 0) m_model = aAttributes.at("model");
      if (aAttributes.count("serialNumber") > 0) m_serialNumber = aAttributes.at("serialNumber");
      if (aAttributes.count("station") > 0) m_station = aAttributes.at("station");
    }

    const std::map<std::string,std::string> &getAttributes() const {
      if (!m_hasAttributes)
      {
        Description *self = const_cast<Description*>(this);
        
        if (!m_manufacturer.empty()) self->m_attributes["manufacturer"] = m_manufacturer;
        if (!m_model.empty()) self->m_attributes["model"] = m_model;
        if (!m_serialNumber.empty()) self->m_attributes["serialNumber"] = m_serialNumber;
        if (!m_station.empty()) self->m_attributes["station"] = m_station;
        
        self->m_hasAttributes = true;
      }
      
      return m_attributes;
    }
    const std::string &getBody() const { return m_body; }

  protected:
    std::map<std::string,std::string> m_attributes;

    std::string m_body;
    std::string m_manufacturer;
    std::string m_model;
    std::string m_serialNumber;
    std::string m_station;
    bool m_hasAttributes;
  };
  
public:
  Composition(const std::string &aId, const std::string &aType, const std::string &aName,
              const std::string &aUuid) : m_id(aId), m_uuid(aUuid), m_name(aName), m_type(aType),
              m_hasAttributes(false)
  { }
  Composition(const Composition &anObj) : m_id(anObj.m_id), m_uuid(anObj.m_uuid), m_name(anObj.m_name),
  m_type(anObj.m_type), m_hasAttributes(false)
  {
    if (anObj.m_description.get() != NULL)
    {
      m_description.reset(new Description(*anObj.m_description.get()));
    }
  }
  Composition(const std::map<std::string,std::string> &aAttributes)
    : m_hasAttributes(false)
  {
    m_id = aAttributes.at("id");
    m_type = aAttributes.at("type");
    if (aAttributes.count("uuid") > 0) m_uuid = aAttributes.at("uuid");
    if (aAttributes.count("name") > 0) m_name = aAttributes.at("name");
  }
  
  
  const std::map<std::string,std::string> &getAttributes() const {
    if (!m_hasAttributes)
    {
      Composition *self = const_cast<Composition*>(this);

      self->m_attributes["id"] = m_id;
      self->m_attributes["type"] = m_type;
      if (!m_uuid.empty()) self->m_attributes["uuid"] = m_uuid;
      if (!m_name.empty()) self->m_attributes["name"] = m_name;
      
      self->m_hasAttributes = true;
    }
    
    return m_attributes;
  }
  
  const Description *getDescription() const { return m_description.get(); }
  void setDescription(Description *aDescription) {
    m_description.reset(aDescription);
  }
  
protected:
  std::string m_id;
  std::string m_uuid;
  std::string m_name;
  std::string m_type;
  
  std::map<std::string,std::string> m_attributes;
  bool m_hasAttributes;
  
  std::unique_ptr<Description> m_description;
};