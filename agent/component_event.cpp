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

using namespace std;
static dlib::rmutex sRefMutex;

const string ComponentEvent::SLevels[NumLevels] = 
{
  "Normal",
  "Warning",
  "Fault"
};

/* ComponentEvent public methods */
ComponentEvent::ComponentEvent(
    DataItem& dataItem,
    unsigned int sequence,
    const string& time,
    const string& value
  )
{
  mDataItem = &dataItem;
  mSequence = sequence;
  mTime = time;
  mHasAttributes = false;  
  convertValue(value);
  mRefCount = 1;
}

ComponentEvent::ComponentEvent(ComponentEvent& ce)
{  
  mDataItem = ce.getDataItem();
  mTime = ce.mTime;
  mSequence = ce.mSequence;
  mAlarmData = ce.mAlarmData;
  mValue = ce.mValue;
  mHasAttributes = false;
  mRefCount = 1;
}

ComponentEvent::~ComponentEvent()
{
}

void ComponentEvent::referTo()
{
  dlib::auto_mutex lock(sRefMutex);
  mRefCount++;
}

void ComponentEvent::unrefer()
{
  dlib::auto_mutex lock(sRefMutex);
  if (--mRefCount == 0)
  {
    delete this;
  }
}

std::map<string, string> *ComponentEvent::getAttributes()
{
  if (!mHasAttributes) 
  {
    mAttributes["dataItemId"] = mDataItem->getId();
    mAttributes["timestamp"] = mTime;
    if (!mDataItem->getName().empty())
      mAttributes["name"] = mDataItem->getName();
    mAttributes["sequence"] = intToString(mSequence);
    
    if (mDataItem->isCondition())
    {
      // Conditon data: LEVEL|NATIVE_CODE|[SUB_TYPE]
      istringstream toParse(mAlarmData);
      string token;
      
      getline(toParse, token, '|');
      if (strcasecmp(token.c_str(), "normal") == 0)
        mLevel = NORMAL;
      else if (strcasecmp(token.c_str(), "warning") == 0)
        mLevel = WARNING;
      else if (strcasecmp(token.c_str(), "fault") == 0)
        mLevel = FAULT;
      else // Throw...
        mLevel = NORMAL;
      
      getline(toParse, token, '|');
      if (!token.empty())
	mAttributes["nativeCode"] = token;
      
      getline(toParse, token, '|');
      if (!token.empty())
	mAttributes["qualifier"] = token;
      
      mAttributes["type"] = mDataItem->getType();
    }
    else
    {
      if (!mDataItem->getSubType().empty())
      {
        mAttributes["subType"] = mDataItem->getSubType();
      }
      
      if (getDataItem()->getType() == "ALARM")
      {
        // Format to parse: CODE|NATIVECODE|SEVERITY|STATE
        istringstream toParse(mAlarmData);
        string token;
        
        getline(toParse, token, '|');
        mAttributes["code"] = token;
      
        getline(toParse, token, '|');
        mAttributes["nativeCode"] = token;
        
        getline(toParse, token, '|');
        mAttributes["severity"] = token;
        
        getline(toParse, token, '|');
        mAttributes["state"] = token;
      }
    }
    
    mHasAttributes = true;
  }
  return &mAttributes;
}

/* ComponentEvent protected methods */
void ComponentEvent::convertValue(const string& value)
{
  // Check if the type is an alarm or if it doesn't have units
  if (value == "UNAVAILABLE")
  {
    mValue = value;
  }
  else if (mDataItem->isCondition() || mDataItem->getType() == "ALARM")
  {
    string::size_type lastPipe = value.rfind('|');
    
    // Alarm data = CODE|NATIVECODE|SEVERITY|STATE
    // Conditon data: SEVERITY|NATIVE_CODE|[SUB_TYPE]
    mAlarmData = value.substr(0, lastPipe);

    // sValue = DESCRIPTION
    mValue = value.substr(lastPipe+1);
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



