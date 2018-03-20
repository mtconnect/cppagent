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

#include <map>

#include "dlib/threads.h"

#include "component.hpp"
#include "globals.hpp"
#include "change_observer.hpp"

#ifdef PASCAL
#undef PASCAL
#endif

class Adapter;

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
    TIME_SERIES,
    DISCRETE
  };
  
  enum EFilterType
  {
    FILTER_MINIMUM_DELTA,
    FILTER_PERIOD,
    FILTER_NONE
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
  const std::string &getCompositionId() const { return mCompositionId; }
  
  float getNativeScale() const { return mNativeScale; }
  double getConversionFactor() const { return mConversionFactor; }
  double getConversionOffset() const { return mConversionOffset; }
  bool hasFactor() const { return mHasFactor; }
  ECategory getCategory() const { return mCategory; }
  ERepresentation getRepresentation() const { return mRepresentation; }
  
  void setConversionFactor(double aFactor, double aOffset);
  
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
  bool isAssetRemoved() const { return mIsAssetRemoved; }
  bool isTimeSeries() const { return mRepresentation == TIME_SERIES; }
  bool isDiscrete() const { return mRepresentation == DISCRETE; }
  
  bool hasResetTrigger() const { return !mResetTrigger.empty(); }
  const std::string &getResetTrigger() const { return mResetTrigger; }
  void setResetTrigger(const std::string &aTrigger) { mResetTrigger = aTrigger; }
  
  bool hasInitialValue() const { return !mInitialValue.empty(); }
  const std::string &getInitialValue() const { return mInitialValue; }
  void setInitialValue(const std::string &aValue) { mInitialValue = aValue; }

  
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
    // Do not dupe check for time series.
    if (mRepresentation != VALUE)
      return false;
    else if (aValue == mLastValue)
      return true;
    
    mLastValue = aValue;
    return false;
  }
  
  /* Filter checking */
  bool isFiltered(const double aValue, const double aTimeOffset) {
    if (mHasMinimumDelta && mCategory == SAMPLE)
    {
      if (!ISNAN(mLastSampleValue))
      {
        if (aValue > (mLastSampleValue - mFilterValue) && aValue < (mLastSampleValue + mFilterValue)) {
          // Filter value
          return true;      
        }
      }
      
      mLastSampleValue = aValue;
    }
    
    if (mHasMinimumPeriod)
    {
      if (!ISNAN(mLastTimeOffset) && !ISNAN(aTimeOffset))
      {
        if (aTimeOffset < (mLastTimeOffset + mFilterPeriod)) {
          // Filter value
          return true;
        }
      }
      
      mLastTimeOffset = aTimeOffset;
    }
    return false;
  }
  
  /* Constrainsts */
  bool hasConstraints() { return mHasConstraints; }
  std::string getMaximum() const { return mMaximum; }
  std::string getMinimum() const { return mMinimum; }
  std::vector<std::string> &getConstrainedValues() { return mValues; }
  bool hasConstantValue() { return mValues.size() == 1; }
  
  bool hasMinimumDelta() const { return mHasMinimumDelta; }
  bool hasMinimumPeriod() const { return mHasMinimumPeriod; }
  double getFilterValue() const { return mFilterValue; }
  double getFilterPeriod() const { return mFilterPeriod; }
  
  void setMaximum(std::string aMax) { mMaximum = aMax; mHasConstraints = true; }
  void setMinimum(std::string aMin) { mMinimum = aMin; mHasConstraints = true; }
  void addConstrainedValue(std::string aValue) { mValues.push_back(aValue); mHasConstraints = true; }
  
  void setMinmumDelta(double aValue) { mFilterValue = aValue; mHasMinimumDelta = true; }
  void setMinmumPeriod(double aValue) { mFilterPeriod = aValue; mHasMinimumPeriod = true; }
  
  bool conversionRequired();
  std::string convertValue(const std::string& value);
  float convertValue(float aValue);
  
  Adapter *getDataSource() const { return mDataSource;  }
  void setDataSource(Adapter *aSource);
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
  
  std::string mCompositionId;
  
  /* Native scale of data item */
  float mNativeScale;
  bool mHasNativeScale;
  bool mThreeD;
  bool mIsMessage, mIsAlarm, mIsAssetChanged, mIsAssetRemoved;
  
  /* Sig figs of data item */
  unsigned int mSignificantDigits;
  bool mHasSignificantDigits;
  
  /* Coordinate system of data item */
  std::string mCoordinateSystem;
  
  /* Extra source information of data item */
  std::string mSource;
  
  // The reset trigger;
  std::string mResetTrigger;
  
  // Initial value
  std::string mInitialValue;
  
  /* Constraints for this data item */
  std::string mMaximum, mMinimum;
  std::vector<std::string> mValues;
  bool mHasConstraints;
  
  double mFilterValue;
  double mFilterPeriod;
  bool mHasMinimumDelta;
  bool mHasMinimumPeriod;
  
  /* Component that data item is associated with */  
  Component * mComponent;

  /* Duplicate and filter checking */
  std::string mLastValue;
  double mLastSampleValue;
  double mLastTimeOffset;
  
  /* Attrubutes */
  std::map<std::string, std::string> mAttributes;
  
  /* The data source for this data item */
  Adapter *mDataSource;

  /* Conversion factor */
  double mConversionFactor;
  double mConversionOffset;
  bool mConversionDetermined, mConversionRequired, mHasFactor;
};
