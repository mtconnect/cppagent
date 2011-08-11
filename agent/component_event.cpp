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

#include "component_event.hpp"
#include "data_item.hpp"
#include "dlib/threads.h"

#ifdef WIN32
#define strcasecmp stricmp
#define strncasecmp strnicmp
#define strtof strtod
#endif

using namespace std;
static dlib::rmutex sAttributeMutex;

const string ComponentEvent::SLevels[NumLevels] = 
{
  "Normal",
  "Warning",
  "Fault",
  "Unavailable"
};

/* ComponentEvent public methods */
ComponentEvent::ComponentEvent(DataItem& dataItem,
                               uint64_t sequence,
                               const string& time,
                               const string& value)
{
  mDataItem = &dataItem;
  mIsTimeSeries = mDataItem->isTimeSeries();
  mSequence = sequence;
  size_t pos = time.find('@');
  if (pos != string::npos)
  {
    mTime = time.substr(0, pos);
    mDuration = time.substr(pos + 1);
  } else {
    mTime = time;
  }
  
  mHasAttributes = false;  
  convertValue(value);
}

ComponentEvent::ComponentEvent(ComponentEvent& ce)
{  
  mDataItem = ce.getDataItem();
  mTime = ce.mTime;
  mDuration = ce.mDuration;
  mSequence = ce.mSequence;
  mRest = ce.mRest;
  mValue = ce.mValue;
  mHasAttributes = false;
  mCode = ce.mCode;
  mIsTimeSeries = ce.mIsTimeSeries;
  if (mIsTimeSeries) {
    mTimeSeries = ce.mTimeSeries;
    mSampleCount = ce.mSampleCount;
  }
}

ComponentEvent::~ComponentEvent()
{
}

AttributeList *ComponentEvent::getAttributes()
{
  if (!mHasAttributes) 
  {
    dlib::auto_mutex lock(sAttributeMutex);
    if (!mHasAttributes) 
    {
      mAttributes.push_back(AttributeItem("dataItemId", mDataItem->getId()));
      mAttributes.push_back(AttributeItem("timestamp", mTime));
      if (!mDataItem->getName().empty())
        mAttributes.push_back(AttributeItem("name", mDataItem->getName()));
      mSequenceStr = int64ToString(mSequence);
      mAttributes.push_back(AttributeItem("sequence",mSequenceStr));
      if (!mDataItem->getSubType().empty())
        mAttributes.push_back(AttributeItem("subType", mDataItem->getSubType()));
      if (!mDataItem->getStatistic().empty())
        mAttributes.push_back(AttributeItem("statistic", mDataItem->getStatistic()));
      if (!mDuration.empty())
        mAttributes.push_back(AttributeItem("duration", mDuration));
      
      if (mDataItem->isCondition())
      {
        // Conditon data: LEVEL|NATIVE_CODE|NATIVE_SEVERITY|QUALIFIER
        istringstream toParse(mRest);
        string token;
        
        getline(toParse, token, '|');
        if (strcasecmp(token.c_str(), "normal") == 0)
          mLevel = NORMAL;
        else if (strcasecmp(token.c_str(), "warning") == 0)
          mLevel = WARNING;
        else if (strcasecmp(token.c_str(), "fault") == 0)
          mLevel = FAULT;
        else // Assume unavailable
          mLevel = UNAVAILABLE;
        
        
        if (!toParse.eof()) {
          getline(toParse, token, '|');
          if (!token.empty()) {
            mCode = token;
            mAttributes.push_back(AttributeItem("nativeCode", token));
          }
        }
        
        if (!toParse.eof()) {
          getline(toParse, token, '|');
          if (!token.empty())
            mAttributes.push_back(AttributeItem("nativeSeverity", token));
        }
        
        if (!toParse.eof()) {
          getline(toParse, token, '|');
          if (!token.empty())
            mAttributes.push_back(AttributeItem("qualifier", token));
        }
        
        mAttributes.push_back(AttributeItem("type", mDataItem->getType()));
      }
      else if (mDataItem->isTimeSeries())
      {
        istringstream toParse(mRest);
        string token;
        
        getline(toParse, token, '|');
        mAttributes.push_back(AttributeItem("sampleCount", token));
        mSampleCount = atoi(token.c_str());
        
        getline(toParse, token, '|');
        if (!token.empty())
          mAttributes.push_back(AttributeItem("sampleRate", token));
      }
      else if (mDataItem->isMessage())
      {
        // Format to parse: NATIVECODE
        if (!mRest.empty())
          mAttributes.push_back(AttributeItem("nativeCode", mRest));
      }
      else if (mDataItem->isAlarm())
      {
        // Format to parse: CODE|NATIVECODE|SEVERITY|STATE
        istringstream toParse(mRest);
        string token;
        
        getline(toParse, token, '|');
        mAttributes.push_back(AttributeItem("code", token));
        
        getline(toParse, token, '|');
        mAttributes.push_back(AttributeItem("nativeCode", token));
        
        getline(toParse, token, '|');
        mAttributes.push_back(AttributeItem("severity", token));
        
        getline(toParse, token, '|');
        mAttributes.push_back(AttributeItem("state", token));
      }
      else if (mDataItem->isAssetChanged())
      {
        istringstream toParse(mRest);
        string token;
        
        getline(toParse, token, '|');
        mAttributes.push_back(AttributeItem("assetType", token));
      }
      mHasAttributes = true;
    }    
  }
  return &mAttributes;
}

void ComponentEvent::normal()
{
  if (mDataItem->isCondition())
  {
    mAttributes.clear();
    mCode.clear();
    mHasAttributes = false;
    mRest = "normal|||";
    getAttributes();
  }
}

/* ComponentEvent protected methods */
void ComponentEvent::convertValue(const string& value)
{
  // Check if the type is an alarm or if it doesn't have units
  if (value == "UNAVAILABLE")
  {
    mValue = value;
  }
  else if (mIsTimeSeries || mDataItem->isCondition() || mDataItem->isAlarm() ||
           mDataItem->isMessage() || mDataItem->isAssetChanged())
  {
    string::size_type lastPipe = value.rfind('|');
    
    // Alarm data = CODE|NATIVECODE|SEVERITY|STATE
    // Conditon data: SEVERITY|NATIVE_CODE|[SUB_TYPE]
    // Asset changed: type|id
    mRest = value.substr(0, lastPipe);
    
    // sValue = DESCRIPTION
    if (mIsTimeSeries)
    {
      const char *cp = value.c_str();
      cp += lastPipe + 1;
      
      // Check if conversion is required...
      char *np;
      while (cp != NULL && *cp != '\0') 
      {
		float v = strtof(cp, &np);
		if (cp != np)
		  mTimeSeries.push_back(v);
		else
		  np = NULL;
		cp = np;
      }
    }
    else
    {
      mValue = value.substr(lastPipe+1);
    }
  }
  else if (mDataItem->conversionRequired())
  {
    mValue = mDataItem->convertValue(value);
  }
  else
  {
    mValue = value;
  }
}

ComponentEvent *ComponentEvent::getFirst()
{
  if (mPrev.getObject() != NULL)
    return mPrev->getFirst();
  else
    return this;
}

void ComponentEvent::getList(std::list<ComponentEventPtr> &aList)
{
  if (mPrev.getObject() != NULL)
    mPrev->getList(aList);
  
  aList.push_back(this);
}

ComponentEvent *ComponentEvent::find(const std::string &aCode)
{
  if (mCode == aCode)
    return this;
  
  if (mPrev.getObject() != NULL)
    return mPrev->find(aCode);
  
  return NULL;
}

bool ComponentEvent::replace(ComponentEvent *aOld,
                             ComponentEvent *aNew)
{
  ComponentEvent *obj = mPrev.getObject();
  if (obj == NULL)
    return false;
  
  if (obj == aOld) 
  {
    aNew->mPrev = aOld->mPrev;
    mPrev = aNew;
    return true;
  }
  
  return mPrev->replace(aOld, aNew);
}

ComponentEvent *ComponentEvent::deepCopy()
{
  ComponentEvent *n = new ComponentEvent(*this);
  if (mPrev.getObject() != NULL) {
    n->mPrev = mPrev->deepCopy();
    n->mPrev->unrefer();
  }
  return n;
}

ComponentEvent *ComponentEvent::deepCopyAndRemove(ComponentEvent *aOld)
{
  if (this == aOld)
  {
    if (mPrev.getObject() != NULL)
      return mPrev->deepCopy();
    else
      return NULL;
  }
  
  ComponentEvent *n = new ComponentEvent(*this);
  if (mPrev.getObject() != NULL) {
    n->mPrev = mPrev->deepCopyAndRemove(aOld);
    if (n->mPrev.getObject() != NULL)
      n->mPrev->unrefer();
  }
  
  return n;
}


