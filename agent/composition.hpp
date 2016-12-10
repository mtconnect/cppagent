/*
 * Copyright Copyright 2012, System Insights, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef COMPOSITION_HPP
#define COMPOSITION_HPP

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
                const std::string &aStation) : mBody(aBody), mManufacturer(aManufacturer), mModel(aModel), mSerialNumber(aSerialNumber),
                mStation(aStation), mHasAttributes(false) {}
    Description(const Description &anObj) : mBody(anObj.mBody), mManufacturer(anObj.mManufacturer), mModel(anObj.mModel),
                mSerialNumber(anObj.mSerialNumber), mStation(anObj.mStation), mHasAttributes(false) {}
    Description(const std::string &aBody, const std::map<std::string,std::string> &aAttributes)
      : mHasAttributes(false)
    {
      if (aAttributes.count("manufacturer") > 0) mManufacturer = aAttributes.at("manufacturer");
      if (aAttributes.count("model") > 0) mModel = aAttributes.at("model");
      if (aAttributes.count("serialNumber") > 0) mSerialNumber = aAttributes.at("serialNumber");
      if (aAttributes.count("station") > 0) mStation = aAttributes.at("station");
    }

    const std::map<std::string,std::string> &getAttributes() const {
      if (!mHasAttributes)
      {
        Description *self = const_cast<Description*>(this);
        
        if (!mManufacturer.empty()) self->mAttributes["manufacturer"] = mManufacturer;
        if (!mModel.empty()) self->mAttributes["model"] = mModel;
        if (!mSerialNumber.empty()) self->mAttributes["serialNumber"] = mSerialNumber;
        if (!mStation.empty()) self->mAttributes["station"] = mStation;
        
        self->mHasAttributes = true;
      }
      
      return mAttributes;
    }
    const std::string &getBody() const { return mBody; }

  protected:
    std::map<std::string,std::string> mAttributes;

    std::string mBody;
    std::string mManufacturer;
    std::string mModel;
    std::string mSerialNumber;
    std::string mStation;
    bool mHasAttributes;
  };
  
public:
  Composition(const std::string &aId, const std::string &aType, const std::string &aName,
              const std::string &aUuid) : mId(aId), mUuid(aUuid), mName(aName), mType(aType),
              mHasAttributes(false)
  { }
  Composition(const Composition &anObj) : mId(anObj.mId), mUuid(anObj.mUuid), mName(anObj.mName),
  mType(anObj.mType), mHasAttributes(false)
  {
    if (anObj.mDescription.get() != NULL)
    {
      mDescription.reset(new Description(*anObj.mDescription.get()));
    }
  }
  Composition(const std::map<std::string,std::string> &aAttributes)
    : mHasAttributes(false)
  {
    mId = aAttributes.at("id");
    mType = aAttributes.at("type");
    if (aAttributes.count("uuid") > 0) mUuid = aAttributes.at("uuid");
    if (aAttributes.count("name") > 0) mName = aAttributes.at("name");
  }
  
  
  const std::map<std::string,std::string> &getAttributes() const {
    if (!mHasAttributes)
    {
      Composition *self = const_cast<Composition*>(this);

      self->mAttributes["id"] = mId;
      self->mAttributes["type"] = mType;
      if (!mUuid.empty()) self->mAttributes["uuid"] = mUuid;
      if (!mName.empty()) self->mAttributes["name"] = mName;
      
      self->mHasAttributes = true;
    }
    
    return mAttributes;
  }
  
  const Description *getDescription() const { return mDescription.get(); }
  void setDescription(Description *aDescription) {
    mDescription.reset(aDescription);
  }
  
protected:
  std::string mId;
  std::string mUuid;
  std::string mName;
  std::string mType;
  
  std::map<std::string,std::string> mAttributes;
  bool mHasAttributes;
  
  std::unique_ptr<Description> mDescription;
};

#endif
