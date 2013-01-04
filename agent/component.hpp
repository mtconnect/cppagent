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

#ifndef COMPONENT_HPP
#define COMPONENT_HPP

#include <sstream>
#include <string>
#include <list>
#include <map>

#include "globals.hpp"

class DataItem;
class Device;

class Component
{
public:
  /* std::string enumeration for component parts and details */
  enum EComponentSpecs
  {
    // Component parts
    DEVICE,
    // Component details
    COMPONENTS,
    DATA_ITEM,
    DATA_ITEMS,
    CONFIGURATION,
    DESCRIPTION,
    SOURCE,
    TEXT
  };
  
  static const unsigned int NumComponentSpecs = 8;
  static const std::string SComponentSpecs[];
  
public:
  /* Take in a class name & mapping of attributes */
  Component(
    const std::string& cls,
    std::map<std::string, std::string> attributes,
    const std::string &aPrefix = ""
  );
  
  /* Virtual destructor */
  virtual ~Component();
  
  /* Return a map of attributes of all the component specs */
  std::map<std::string, std::string> *getAttributes() { return &mAttributes; }
  
  /* Return what part of the component it is */
  const std::string& getClass() const { return mClass; }
  const std::string& getPrefixedClass() const { return mPrefixedClass; }
  
  /* Getter methods for the component ID/Name */
  const std::string &getId() const { return mId; }
  const std::string &getName() const { return mName; }
  const std::string &getNativeName() const { return mNativeName; }
  const std::string &getUuid() const { return mUuid; }
  const std::string &getDescriptionBody() const { return mDescriptionBody; }
  const std::string &getPrefix() const { return mPrefix; }
  const std::string &getConfiguration() const { return mConfiguration; }
  
  // Setter methods
  void setUuid(const std::string &aUuid) { mUuid = aUuid; reBuildAttributes(); }
  void setManufacturer(const std::string &aManufacturer) { mManufacturer = aManufacturer; reBuildAttributes(); }
  void setSerialNumber(const std::string &aSerialNumber) { mSerialNumber = aSerialNumber; reBuildAttributes(); }
  void setStation(const std::string &aStation) { mStation = aStation; reBuildAttributes(); }
  void setDescription(const std::string &aDescription) { mDescriptionBody = aDescription; reBuildAttributes(); }
  void setNativeName(const std::string &aNativeName) { mNativeName = aNativeName; reBuildAttributes(); }
  
  // Cached data items
  DataItem *getAvailability() const { return mAvailability; }
  DataItem *getAssetChanged() const { return mAssetChanged; }
  
  
  /* Add/get description specifications using an attribute map */
  void addDescription(std::string body, std::map<std::string, std::string> attributes);
  std::map<std::string, std::string> getDescription() const;
  
  void setConfiguration(const std::string &aConfiguration) { mConfiguration = aConfiguration; }
  
  /* Get the device that any component is associated with */
  Device * getDevice();
  
  /* Set/Get the component's parent component */
  void setParent(Component& parent) { mParent = &parent; }
  Component * getParent() const { return mParent; }
  
  /* Add to/get the component's std::list of children */
  void addChild(Component& child) { mChildren.push_back(&child); }
  std::list<Component *> &getChildren() { return mChildren; }
  
  /* Add to/get the component's std::list of data items */
  void addDataItem(DataItem& dataItem);
  const std::list<DataItem *> &getDataItems() const { return mDataItems; }
  
  bool operator<(const Component &comp) const { return mId < comp.getId(); }
  bool operator==(const Component &comp) const { return mId == comp.getId(); }

protected:
  /* Return a map of attributes of all the component specs */
  std::map<std::string, std::string> buildAttributes() const;
  void reBuildAttributes() { mAttributes = buildAttributes(); }
  
protected:
  /* Unique ID for each component */
  std::string mId;
  
  /* Name for itself */
  std::string mName;
  std::string mNativeName;

  /* The class */
  std::string mClass;
  std::string mPrefix;
  std::string mPrefixedClass;
  
  /* Universal unique identifier */
  std::string mUuid;
  
  /* If receiving data, a sample rate is needed */
  float mSampleRate;
  
  /* Description of itself */
  std::string mManufacturer;
  std::string mSerialNumber;
  std::string mStation;
  std::string mDescriptionBody;
  std::string mConfiguration;
  
  /* Component relationships */
  /* Pointer to the parent component */
  Component * mParent;
  Device *mDevice;
  DataItem *mAvailability;
  DataItem *mAssetChanged;
  
  /* Each component keeps track of it's children in a std::list */
  std::list<Component *> mChildren;
  
  /* Keep Track of all the data items associated with this component */
  std::list<DataItem *> mDataItems;
  
  /* The set of attribtues */
  std::map<std::string, std::string> mAttributes;
};

struct ComponentComp {
  bool operator()(const Component *lhs, const Component *rhs) const {
    return *lhs < *rhs;
  }
};

#endif

