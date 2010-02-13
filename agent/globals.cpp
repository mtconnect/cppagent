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

/* Dlib library */
#include "../lib/dlib/all/source.cpp"

#include "globals.hpp"

using namespace std;

string intToString(int i)
{
  ostringstream stm;
  stm << i;
  return stm.str();
}

string floatToString(double f)
{
  char s[32];
  sprintf(s, "%.7g", f);
  return (string) s;
}

string toUpperCase(string& text)
{
  for (unsigned int i = 0; i < text.length(); i++)
  {
    text[i] = toupper(text[i]);
  }
  
  return text;
}

bool isNonNegativeInteger(const string& s)
{
  for (unsigned int i = 0; i < s.length(); i++)
  {
    if (!isdigit(s[i]))
    {
      return false;
    }
  }
  
  return true;
}

string getCurrentTime(TimeFormat format)
{
#ifdef WIN32
  SYSTEMTIME st;
  char timestamp[64];
  GetSystemTime(&st);
  sprintf(timestamp, "%4d-%02d-%02dT%02d:%02d:%02d", st.wYear, st.wMonth,
    st.wDay, st.wHour, st.wMinute, st.wSecond);
  
  if (format == GMT_UV_SEC)
  {
    sprintf(timestamp + strlen(timestamp), ".%04d", st.wMilliseconds);
  }
  
  return timestamp;
#else
  char timeBuffer[50];
  time_t rawtime;
  struct tm * timeinfo;

  time (&rawtime);
  
  timeinfo = (format == LOCAL) ? localtime(&rawtime) : gmtime(&rawtime);
  
  switch (format)
  {
    case HUM_READ:
      strftime(timeBuffer, 50, "%a, %d %b %Y %H:%M:%S %Z", timeinfo);
      break;
    case GMT:
      strftime(timeBuffer, 50, "%Y-%m-%dT%H:%M:%S+00:00", timeinfo);
      break;
    case GMT_UV_SEC:
    case LOCAL:
      strftime(timeBuffer, 50, "%Y-%m-%dT%H:%M:%S", timeinfo);
      break;
  }
  
  string toReturn(timeBuffer);
  
  if (format == GMT_UV_SEC)
  {
    timeval timEval;
    gettimeofday(&timEval, NULL);
    
    ostringstream ostm;
    ostm << timEval.tv_usec;
    string toAppend = ostm.str();
    
    // Precision is 6 digits
    toReturn += ".";
    for (unsigned int i = toAppend.length(); i < 6; i++)
    {
      toReturn += "0";
    }
    toReturn += toAppend;
  }
  
  return toReturn;
#endif
}

unsigned int getCurrentTimeInSec()
{
  return time(NULL);
}

void replaceIllegalCharacters(string& data)
{
  for (unsigned int i=0; i<data.length(); i++)
  {
    char c = data[i];
    switch (c) {
      case '&': data.replace(i, 1, "&amp;"); break;
      case '<': data.replace(i, 1, "&lt;"); break;
      case '>': data.replace(i, 1, "&gt;"); break;
    }
  }
}

const char *gLogFile;

void logEvent(const string& source, const string& message)
{
  ofstream logFile;
  logFile.open(gLogFile, ios::app);
  if (logFile.is_open())
  {
    logFile << "[" << getCurrentTime(LOCAL) << "] ";
    logFile << source << ": " << message << endl; 
  }
  logFile.close();
}

int getEnumeration(const string& name, const string *array, unsigned int size)
{
  for (unsigned int i = 0; i < size; i++)
  {
    if (name == array[i])
    {
       return i;
    }
  }
  
  return ENUM_MISS;
}

static string::size_type insertPrefix(string &aPath, string::size_type &aPos,
				      const string aPrefix)
{
  aPath.insert(aPos, aPrefix);
  aPos += aPrefix.length();
  aPath.insert(aPos++, ":");

  return aPos;
}

string addNamespace(const string aPath, const string aPrefix)
{
  if (aPrefix.empty())
    return aPath;

  string newPath = aPath;
  string::size_type pos = 0;
  // Special case for relative pathing
  if (newPath[pos] != '/')
  {
    insertPrefix(newPath, pos, aPrefix);
  }
  
  
  while ((pos = newPath.find('/', pos)) != string::npos)
  {
    pos ++;
    if (newPath[pos] == '/')
      pos++;
    if (newPath[pos] != '*')
      insertPrefix(newPath, pos, aPrefix);
  }

  pos = 0;
  while ((pos = newPath.find('|', pos)) != string::npos)
  {
    pos ++;
    if (newPath[pos] != '/')
      insertPrefix(newPath, pos, aPrefix);
  }
  
  return newPath;
}

