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

#ifndef COMPONENT_EVENT_HPP
#define COMPONENT_EVENT_HPP

#include <string>
#include <vector>
#include <cmath>

#include <dlib/array.h>

#include "component.hpp"
#include "globals.hpp"
#include "data_item.hpp"
#include "ref_counted.hpp"



typedef std::pair<const char*, std::string> AttributeItem;
typedef std::vector<AttributeItem> AttributeList;

class ComponentEvent;
typedef RefCountedPtr<ComponentEvent> ComponentEventPtr;
typedef dlib::array_kernel_2<ComponentEventPtr> ComponentEventPtrArrayBase;
typedef dlib::array_expand_1<ComponentEventPtrArrayBase> ComponentEventPtrArray;

/* Component Event */
class ComponentEvent : public RefCounted
{
  
public:
  enum ELevel {
    NORMAL,
    WARNING,
    FAULT,
    UNAVAILABLE
  };
    
  static const unsigned int NumLevels = 4;
  static const std::string SLevels[];
  
public:
  /* Initialize with the data item reference, sequence number, time and value */
  ComponentEvent(
    DataItem& dataItem,
    uint64_t sequence,
    const std::string& time,
    const std::string& value
  );
  
  /* Copy constructor */
  ComponentEvent(ComponentEvent& ce);

  ComponentEvent *deepCopy();
  ComponentEvent *deepCopyAndRemove(ComponentEvent *aOld);
  
  /* Extract the component event data into a map */
  AttributeList *getAttributes();
  
  /* Get the data item associated with this event */
  DataItem * getDataItem() const { return mDataItem; }
  
  /* Get the value */
  const std::string &getValue() const { return mValue; }
  ELevel getLevel();
  const std::string &getLevelString() { return SLevels[getLevel()]; }
  const std::string &getCode() { getAttributes(); return mCode; }
  void normal();

  // Time series info...
  const std::vector<float> &getTimeSeries() { return mTimeSeries; }
  bool isTimeSeries() const { return mIsTimeSeries; }
  int getSampleCount() const { return mSampleCount; }
  
  uint64_t getSequence() const { return mSequence; }
  
  ComponentEvent *getFirst();
  ComponentEvent *getPrev() { return mPrev; }
  void getList(std::list<ComponentEventPtr> &aList);
  void appendTo(ComponentEvent *aEvent);
  ComponentEvent *find(const std::string &aNativeCode);
  bool replace(ComponentEvent *aOld,
               ComponentEvent *aNew); 

  bool operator<(ComponentEvent &aOther) {
    if ((*mDataItem) < (*aOther.mDataItem))
      return true;
    else if (*mDataItem == *aOther.mDataItem)
      return mSequence < aOther.mSequence;
    else
      return false;
  }
  
protected:
  /* Virtual destructor */
  virtual ~ComponentEvent();
  
protected:
  /* Holds the data item from the device */
  DataItem * mDataItem;
  
  /* Sequence number of the event */
  uint64_t mSequence;
  std::string mSequenceStr;
  
  /* Timestamp of the event's occurence */
  std::string mTime;
  std::string mDuration;
  
  /* Hold the alarm data:  CODE|NATIVECODE|SEVERITY|STATE */
  /* or the Conditon data: LEVEL|NATIVE_CODE|NATIVE_SEVERITY|QUALIFIER */
  /* or the message data:  NATIVE_CODE */
  /* or the time series data */
  std::string mRest;
  ELevel mLevel;
  
  /* The value of the event, either as a float or a string */
  std::string mValue;
  bool mIsFloat;
  bool mIsTimeSeries;
  std::vector<float> mTimeSeries;
  int mSampleCount;
  
  /* The attributes, created on demand */
  bool mHasAttributes;
  AttributeList mAttributes;

  // For condition tracking
  std::string mCode;
  
  // For back linking of condition
  ComponentEventPtr mPrev;

protected:
  /* Convert the value to the agent unit standards */
  void convertValue(const std::string& value);
};

inline ComponentEvent::ELevel ComponentEvent::getLevel()
{
  if (!mHasAttributes) getAttributes();
  return mLevel;
}

inline void ComponentEvent::appendTo(ComponentEvent *aEvent) 
{ 
  mPrev = aEvent; 
}
#endif

