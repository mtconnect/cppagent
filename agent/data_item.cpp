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

#include "data_item.hpp"
#include "device.hpp"
#include "dlib/logger.h"

using namespace std;

dlib::logger sLogger("data_item");

/* ComponentEvent public static constants */
const string DataItem::SSimpleUnits[NumSimpleUnits] =
{
  "AMPERE",
  "COUNT",
  "JOULE",
  "PASCAL",
  "PH",
  "VOLT",
  "WATT",
  "OHM",
  "SOUND_LEVEL",
  "SIEMENS",
  "DECIBEL",
  "INCH",
  "FOOT",
  "CENTIMETER",
  "DECIMETER",
  "METER",
  "FAHRENHEIT",
  "POUND",
  "GRAM",
  "RADIAN",
  "MINUTE",
  "HOUR",
  "SECOND",
  "MILLIMETER",
  "LITER",
  "DEGREE",
  "KILOGRAM",
  "NEWTON",
  "CELSIUS",
  "REVOLUTION",
  "STATUS",
  "PERCENT",
  "NEWTON_MILLIMETER",
  "HERTZ",
  "MILLIMETER_3D",
  "DEGREE_3D"
};


/* DataItem public methods */
DataItem::DataItem(std::map<string, string> attributes) 
  : mRepresentation(VALUE), mHasNativeScale(false), mHasSignificantDigits(false),  
    mHasConstraints(false), mDataSource(NULL), mConversionDetermined(false),  
    mConversionRequired(false), mHasFactor(false)
{
  mId = attributes["id"];
  mName = attributes["name"];
  mType = attributes["type"];
  mIsAlarm = (mType == "ALARM");
  mIsMessage = (mType == "MESSAGE");
  mIsAssetChanged = (mType == "ASSET_CHANGED");

  mCamelType = getCamelType(mType, mPrefix);
  if (attributes["representation"] == "TIME_SERIES")
  {
    mRepresentation = TIME_SERIES;
    mCamelType += "TimeSeries";
  }
  if (!mPrefix.empty())
    mPrefixedCamelType = mPrefix + ":" + mCamelType;
  else
    mPrefixedCamelType = mCamelType;
  mThreeD = false;
  
  if (!attributes["subType"].empty())
  {
    mSubType = attributes["subType"];
  }

  if (attributes["category"] == "SAMPLE")
    mCategory = SAMPLE;
  else if (attributes["category"] == "CONDITION")
    mCategory = CONDITION;
  else if (attributes["category"] == "EVENT")
    mCategory = EVENT;
  else
  {
    // Raise an invlaid category exception...
  }
    
  if (!attributes["nativeUnits"].empty())
  {
    mNativeUnits = attributes["nativeUnits"];
  }
  
  if (!attributes["units"].empty())
  {
    mUnits = attributes["units"];
    if (mNativeUnits.empty())
      mNativeUnits = mUnits;
  }

  if (!attributes["statistic"].empty())
  {
    mStatistic = attributes["statistic"];
  }

  if (!attributes["sampleRate"].empty())
  {
    mSampleRate = attributes["sampleRate"];
  }

  if (!attributes["nativeScale"].empty())
  {
    mNativeScale = atof(attributes["nativeScale"].c_str());
    mHasNativeScale = true;
  }

  if (!attributes["significantDigits"].empty())
  {
    mSignificantDigits = atoi(attributes["significantDigits"].c_str());
    mHasSignificantDigits = true;
  }
  
  if (!attributes["coordinateSystem"].empty())
  {
    mCoordinateSystem = attributes["coordinateSystem"];
  }

  mComponent = NULL;
  mAttributes = buildAttributes();
}

DataItem::~DataItem()
{
}

std::map<string, string> DataItem::buildAttributes() const
{
  std::map<string, string> attributes;
  
  attributes["id"] = mId;
  attributes["type"] = mType;
  
  if (!mSubType.empty())
  {
    attributes["subType"] = mSubType;
  }

  switch(mCategory)
  {
    case SAMPLE:
      attributes["category"] = "SAMPLE";
      break;
      
    case EVENT:
      attributes["category"] = "EVENT";
      break;

    case CONDITION:
      attributes["category"] = "CONDITION";
      break;
  }

  if (mRepresentation == TIME_SERIES)
  {
    attributes["representation"] = "TIME_SERIES";
  }

  if (!mStatistic.empty())
  {
    attributes["statistic"] = mStatistic;
  }

  if (!mSampleRate.empty())
  {
    attributes["sampleRate"] = mSampleRate;
  }

  if (!mName.empty())
  {
    attributes["name"] = mName;
  }
  
  if (!mNativeUnits.empty())
  {
    attributes["nativeUnits"] = mNativeUnits;
  }
  
  if (!mUnits.empty())
  {
    attributes["units"] = mUnits;
  }

  if (mHasNativeScale)
  {
    attributes["nativeScale"] = floatToString(mNativeScale);
  }

  if (mHasSignificantDigits)
  {
    attributes["significantDigits"] = intToString(mSignificantDigits);
  }
  
  if (!mCoordinateSystem.empty())
  {
    attributes["coordinateSystem"] = mCoordinateSystem;
  }
  
  return attributes;
}

bool DataItem::hasName(const string& name)
{
  return mId == name || mName == name || (!mSource.empty() && mSource == name);
}

string DataItem::getCamelType(const string& aType, string &aPrefix)
{
  if (aType.empty())
  {
    return "";
  }
  
  string camel;
  size_t colon = aType.find(':');
  if (colon != string::npos) {
    aPrefix = aType.substr(0, colon);
    camel = aType.substr(colon + 1);
  } else {
    camel = aType;
  }

  string::iterator second = camel.begin();
  second++;
  transform(second, camel.end(), second, ::tolower);

  string::iterator word = find(second, camel.end(), '_');
  while (word != camel.end())
  {
    camel.erase(word);
    camel.replace(word, word + 1, 1, ::toupper(*word));
    word = find(word, camel.end(), '_');
  }

  return camel;
}

bool DataItem::conversionRequired()
{
  if (!mConversionDetermined)
  {
    mConversionDetermined = true;
    mConversionRequired = !mNativeUnits.empty();
  }
  
  return mConversionRequired;
}

float DataItem::convertValue(float aValue)
{
  if (!mConversionDetermined)
    conversionRequired();
  
  if (!mConversionRequired) {
    return aValue;
  } else if (mHasFactor) {
    return (aValue + mConversionOffset) * mConversionFactor;
  } else {
    computeConversionFactors();
    return convertValue(aValue);
  }
}

/* ComponentEvent protected methods */
string DataItem::convertValue(const string& value)
{
  // Check if the type is an alarm or if it doesn't have units
  
  if (!mConversionRequired)
  {
    return value;
  }
  else if (mHasFactor)
  {
    if (mThreeD)
    {
      ostringstream result;
      string::size_type start = 0;
      for (int i = 0; i < 3; i++)
      {
        string::size_type pos = value.find(" ", start);
        result << floatToString((atof(value.substr(start, pos).c_str()) + mConversionOffset) * 
                                mConversionFactor);
        if (pos != string::npos)
        {
          start = value.find_first_not_of(" ", pos);
          result << " ";
        }
      }
      
      return result.str();
    }
    else
    {
      return floatToString((atof(value.c_str()) + mConversionOffset) * 
                           mConversionFactor);
    }
  }
  else
  {
    computeConversionFactors();
    return convertValue(value);
  }
}

void DataItem::computeConversionFactors()
{
  string units = mNativeUnits;
  string::size_type threeD = units.find("_3D");
  mConversionOffset = 0.0;
  string::size_type slashLoc = units.find('/');
  
  // Convert units of numerator / denominator (^ power)
  if (slashLoc == string::npos)
  {
    if (threeD != string::npos)
    {
      mThreeD = true;
      units = units.substr(0, threeD);
    }
    mConversionFactor = simpleFactor(units);
    if (mConversionFactor == 1.0)
    {
      if (mUnits == units) 
	mConversionRequired = false;
      else if (units.substr(0, 4) == "KILO" && units.substr(4) == mUnits)
	mConversionFactor = 1000.0;
      else
	mConversionRequired = false;
    }
  }
  else if (units == "REVOLUTION/MINUTE")
  {
    mConversionFactor = 1.0;
    mConversionRequired = false;
  }
  else
  {
    string numerator = units.substr(0, slashLoc);
    string denominator = units.substr(slashLoc+1);
    
    string::size_type carotLoc = denominator.find('^');
    
    if (numerator == "REVOLUTION" && denominator == "SECOND")
    {
      mConversionFactor = 60.0;
    }
    else if (carotLoc == string::npos)
    {
      mConversionFactor = simpleFactor(numerator) / simpleFactor(denominator);
    }
    else
    {
      string unit = denominator.substr(0, carotLoc);
      string power = denominator.substr(carotLoc+1);
      
      double div = pow((double) simpleFactor(unit), (double) atof(power.c_str()));
      mConversionFactor = simpleFactor(numerator) / div;
    }
  }
  
  if (mHasNativeScale)
  {
    mConversionRequired = true;
    mConversionFactor /= mNativeScale;
  }
  
  mHasFactor = true;
}

void DataItem::setConversionFactor(double aFactor, double aOffset)
{
  mHasFactor = true;
  if (aFactor == 1.0 && mConversionOffset == 0.0)
    mConversionRequired = false;
  else {
    mConversionFactor = aFactor;
    mConversionOffset = aOffset;
    mConversionRequired = true;
  }
}

double DataItem::simpleFactor(const string& units)
{
  switch(getEnumeration(units, SSimpleUnits, NumSimpleUnits))
  {
    case INCH:
      return 25.4;
    case FOOT:
      return 304.8;
    case CENTIMETER:
      return 10.0;
    case DECIMETER:
      return 100.0;
    case METER:
      return 1000.f;
    case FAHRENHEIT:
      mConversionOffset = -32.0;
      return 5.0 / 9.0;
    case POUND:
      return 0.45359237;
    case GRAM:
      return 1 / 1000.0;
    case RADIAN:
      return 57.2957795;
    case MINUTE:
      return 60.0;
    case HOUR:
      return 3600.0;
    
    case SECOND:
    case MILLIMETER:
    case LITER:
    case DEGREE:
    case KILOGRAM:
    case NEWTON:
    case CELSIUS:
    case REVOLUTION:
    case STATUS:
    case PERCENT:
    case NEWTON_MILLIMETER:
    case HERTZ:
    case AMPERE:
    case COUNT:
    case JOULE:
    case PASCAL:
    case PH:
    case VOLT:
    case WATT:
    case OHM:
    case SOUND_LEVEL:
    case SIEMENS:
    case DECIBEL:

    default:
      // Already in correct units
      return 1.0;
  }
}

// Sort by: Device, Component, Category, DataItem
bool DataItem::operator<(DataItem &aOther) 
{
  Device *dev = mComponent->getDevice();
  if (dev->getId() < aOther.getComponent()->getDevice()->getId())
    return true;
  else if (dev->getId() > aOther.getComponent()->getDevice()->getId())
    return false;
  else if (mComponent->getId() < aOther.getComponent()->getId())
    return true;
  else if (mComponent->getId() > aOther.getComponent()->getId())
    return false;
  else if (mCategory < aOther.mCategory)
    return true;
  else if (mCategory == aOther.mCategory)
    return mId < aOther.mId;
  else
    return false;
}

