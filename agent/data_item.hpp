/*
* Copyright (c) 2008, AMT – The Association For Manufacturing Technology (“AMT”)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the AMT nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* DISCLAIMER OF WARRANTY. ALL MTCONNECT MATERIALS AND SPECIFICATIONS PROVIDED
* BY AMT, MTCONNECT OR ANY PARTICIPANT TO YOU OR ANY PARTY ARE PROVIDED "AS IS"
* AND WITHOUT ANY WARRANTY OF ANY KIND. AMT, MTCONNECT, AND EACH OF THEIR
* RESPECTIVE MEMBERS, OFFICERS, DIRECTORS, AFFILIATES, SPONSORS, AND AGENTS
* (COLLECTIVELY, THE "AMT PARTIES") AND PARTICIPANTS MAKE NO REPRESENTATION OR
* WARRANTY OF ANY KIND WHATSOEVER RELATING TO THESE MATERIALS, INCLUDING, WITHOUT
* LIMITATION, ANY EXPRESS OR IMPLIED WARRANTY OF NONINFRINGEMENT,
* MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. 

* LIMITATION OF LIABILITY. IN NO EVENT SHALL AMT, MTCONNECT, ANY OTHER AMT
* PARTY, OR ANY PARTICIPANT BE LIABLE FOR THE COST OF PROCURING SUBSTITUTE GOODS
* OR SERVICES, LOST PROFITS, LOSS OF USE, LOSS OF DATA OR ANY INCIDENTAL,
* CONSEQUENTIAL, INDIRECT, SPECIAL OR PUNITIVE DAMAGES OR OTHER DIRECT DAMAGES,
* WHETHER UNDER CONTRACT, TORT, WARRANTY OR OTHERWISE, ARISING IN ANY WAY OUT OF
* THIS AGREEMENT, USE OR INABILITY TO USE MTCONNECT MATERIALS, WHETHER OR NOT
* SUCH PARTY HAD ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.
*/

#ifndef DATA_ITEM_HPP
#define DATA_ITEM_HPP

#include <map>

#include "dlib/threads.h"

#include "component.hpp"
#include "globals.hpp"
#include "change_observer.hpp"

class Adapter;

#ifdef PASCAL
#undef PASCAL
#endif

class DataItem : public ChangeSignaler
{
public:
  /* Enumeration for data item category */
  enum ECategory
  {
    SAMPLE,
    EVENT,
    CONDITION
  };

  enum ERepresentation
  {
    VALUE,
    TIME_SERIES
  };
    
  
  /* Enumeration for the simple units for simple conversions */
  enum ESimpleUnits
  {
    AMPERE,
    COUNT,
    JOULE,
    PASCAL,
    PH,
    VOLT,
    WATT,
    OHM,
    SOUND_LEVEL,
    SIEMENS,
    DECIBEL,
    INCH,
    FOOT,
    CENTIMETER,
    DECIMETER,
    METER,
    FAHRENHEIT,
    POUND,
    GRAM,
    RADIAN,
    MINUTE,
    HOUR,
    SECOND, 
    MILLIMETER, 
    LITER,
    DEGREE,
    KILOGRAM,
    NEWTON,
    CELSIUS,
    REVOLUTION,
    STATUS,
    PERCENT,
    NEWTON_MILLIMETER,
    HERTZ,
    MILLIMETER_3D,
    DEGREE_3D
  };
  
  static const unsigned int NumSimpleUnits = 36;
  static const std::string SSimpleUnits[];
  
public:
  /* Construct a data item with appropriate attributes mapping */
  DataItem(std::map<std::string, std::string> attributes);
  
  /* Destructor */
  ~DataItem();
  
  /* Get a map of all the attributes of this data item */
  std::map<std::string, std::string> *getAttributes() { return &mAttributes; }
  
  /* Getter methods for data item specs */
  const std::string &getId() const { return mId; }
  const std::string &getName() const { return mName; }
  const std::string &getSource() const { return mSource; }
  const std::string &getType() const { return mType; }
  const std::string &getElementName() const { return mCamelType; }
  const std::string &getPrefixedElementName() const { return mPrefixedCamelType; }
  const std::string &getSubType() const { return mSubType; }
  const std::string &getNativeUnits() const { return mNativeUnits; }
  const std::string &getUnits() const { return mUnits; }
  const std::string &getPrefix() const { return mPrefix; }
  const std::string &getStatistic() const { return mStatistic; }
  const std::string &getSampleRate() const { return mSampleRate; }
  
  float getNativeScale() const { return mNativeScale; }
  double getConversionFactor() const;
  ECategory getCategory() const { return mCategory; }
  ERepresentation getRepresentation() const { return mRepresentation; }
  
  /* Returns if data item has this attribute */
  bool hasName(const std::string& name);
  bool hasNativeScale() const { return mHasNativeScale; }
  
  /* Add a source (extra information) to data item */
  void addSource(const std::string& source) { mSource = source; }
  
  /* Returns true if data item is a sample */
  bool isSample() const { return mCategory == SAMPLE; }
  bool isEvent() const { return mCategory == EVENT; }
  bool isCondition() const { return mCategory == CONDITION; }
  bool isAlarm() const { return mIsAlarm; }
  bool isMessage() const { return mIsMessage; }
  bool isAssetChanged() const { return mIsAssetChanged; }
  bool isTimeSeries() const { return mRepresentation == TIME_SERIES; }
    
  /* Set/get component that data item is associated with */
  void setComponent(Component& component) { mComponent = &component; }
  Component * getComponent() const { return mComponent; }
  
  /* Get the name for the adapter feed */
  std::string getSourceOrName() { return mSource.empty() ? (mName.empty() ? mId : mName) : mSource; }
  
  /* Transform a name to camel casing */
  static std::string getCamelType(const std::string& aType, 
                                  std::string &aPrefix);

  /* Duplicate Checking */
  bool isDuplicate(const std::string &aValue) {
    if (aValue == mLastValue) return true;
    mLastValue = aValue;
    return false;
  }
  
  /* Constrainsts */
  bool hasConstraints() { return mHasConstraints; }
  std::string getMaximum() const { return mMaximum; }
  std::string getMinimum() const { return mMinimum; }
  std::vector<std::string> &getConstrainedValues() { return mValues; }
  
  void setMaximum(std::string aMax) { mMaximum = aMax; mHasConstraints = true; }
  void setMinimum(std::string aMin) { mMinimum = aMin; mHasConstraints = true; }
  void addConstrainedValue(std::string aValue) { mValues.push_back(aValue); mHasConstraints = true; }

  bool conversionRequired();
  std::string convertValue(const std::string& value);
  
  Adapter *getDataSource() const { return mDataSource;  }
  void setDataSource(Adapter *aSource) { if (mDataSource != aSource) mDataSource = aSource; }
  bool operator<(DataItem &aOther);

  bool operator==(DataItem &aOther) {
    return mId == aOther.mId;
  }
  
protected:
  double simpleFactor(const std::string& units);
  std::map<std::string, std::string> buildAttributes() const;
  void computeConversionFactors();
  
protected:
  /* Unique ID for each component */
  std::string mId;
  
  /* Name for itself */
  std::string mName;
  
  /* Type of data item */
  std::string mType;
  std::string mCamelType;
  std::string mPrefixedCamelType;
  std::string mPrefix;

  /* Subtype of data item */
  std::string mSubType;
  
  /* Category of data item */
  ECategory mCategory;
  
  /* Native units of data item */
  std::string mNativeUnits;
  
  /* Units of data item */
  std::string mUnits;
  
  /* The statistical process used on this data item */
  std::string mStatistic;

  /* Representation of data item */
  ERepresentation mRepresentation;
  std::string mSampleRate;
  
  /* Native scale of data item */
  float mNativeScale;
  bool mHasNativeScale;
  bool mThreeD;
  bool mIsMessage, mIsAlarm, mIsAssetChanged;
  
  /* Sig figs of data item */
  unsigned int mSignificantDigits;
  bool mHasSignificantDigits;
  
  /* Coordinate system of data item */
  std::string mCoordinateSystem;
  
  /* Extra source information of data item */
  std::string mSource;
  
  /* Constraints for this data item */
  std::string mMaximum, mMinimum;
  std::vector<std::string> mValues;
  bool mHasConstraints;
  
  /* Component that data item is associated with */  
  Component * mComponent;

  /* Duplicate checking */
  std::string mLastValue;
  
  /* Attrubutes */
  std::map<std::string, std::string> mAttributes;
  
  /* The data source for this data item */
  Adapter *mDataSource;

  /* Conversion factor */
  double mConversionFactor;
  double mConversionOffset;
  bool mConversionDetermined, mConversionRequired, mHasFactor;
};

#endif

