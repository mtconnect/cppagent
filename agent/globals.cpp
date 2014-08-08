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

/* Dlib library */
#include "../lib/dlib/all/source.cpp"

#include "globals.hpp"
#include <time.h>
#include <stdio.h>
#if 0
#ifdef _WINDOWS
#include <psapi.h>
#endif
#endif

#ifdef _WINDOWS
#define localtime_r(t, tm) localtime_s(tm, t)
#define gmtime_r(t, tm) gmtime_s(tm, t)

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

#endif

using namespace std;

string int64ToString(uint64_t i)
{
  ostringstream stm;
  stm << i;
  return stm.str();
}

string intToString(unsigned int i)
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
  time_t sec;
  long usec;

  if (format == GMT_UV_SEC)
  {
#ifdef _WINDOWS
    uint64_t now = getCurrentTimeInMicros();
    sec = (long)(now / 1000000UL);
    usec = (long)(now % 1000000UL);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    sec = tv.tv_sec;
    usec = tv.tv_usec;
#endif
  }
  else
  {
    sec = time(NULL);
    usec = 0;
  }
  
  return getCurrentTime(sec, usec, format);
}

string getCurrentTime(time_t aSec, int aUsec, TimeFormat format)
{
  struct tm timeinfo;
  char timestamp[64];
  
  if (format == LOCAL)
    localtime_r(&aSec, &timeinfo);
  else
    gmtime_r(&aSec, &timeinfo);

  switch (format)
  {
    case HUM_READ:
      std::strftime(timestamp, 50, "%a, %d %b %Y %H:%M:%S GMT", &timeinfo);
      break;
    case GMT:
      strftime(timestamp, 50, "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
      break;
    case GMT_UV_SEC:
      strftime(timestamp, 50, "%Y-%m-%dT%H:%M:%S", &timeinfo);
      break;
    case LOCAL:
      strftime(timestamp, 50, "%Y-%m-%dT%H:%M:%S%z", &timeinfo);
      break;
  }
  
  if (format == GMT_UV_SEC)
  {
    sprintf(timestamp + strlen(timestamp), ".%06dZ", (int) aUsec);
  }
  
  return string(timestamp);
}

uint64_t getCurrentTimeInMicros()
{
  uint64_t now;
#ifdef _WINDOWS
  FILETIME ft;
  static int tzflag;
  
  GetSystemTimeAsFileTime(&ft);
  
  now = ft.dwHighDateTime;
  now <<= 32;
  now |= ft.dwLowDateTime;
  
  now /= 10;  /*convert into microseconds*/
  now -= DELTA_EPOCH_IN_MICROSECS;
#else
  struct timeval tv;
  struct timezone tz;
  
  gettimeofday(&tv, &tz);
  now = tv.tv_sec * 1000000 + tv.tv_usec;
#endif
  return now;
}

string getRelativeTimeString(uint64_t aTime)
{
  char timeBuffer[50];
  time_t seconds;
  int micros;
  struct tm * timeinfo;
  
  seconds = aTime / 1000000;
  micros = aTime % 1000000;
  
  timeinfo = gmtime(&seconds);
  strftime(timeBuffer, 50, "%Y-%m-%dT%H:%M:%S", timeinfo);
  sprintf(timeBuffer + strlen(timeBuffer), ".%06dZ", micros);
  
  return string(timeBuffer);
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

uint64_t parseTimeMicro(const std::string &aTime)
{
  struct tm timeinfo;
  memset(&timeinfo, 0, sizeof(timeinfo));
  char ms[16];
  
  int c = sscanf(aTime.c_str(), "%d-%d-%dT%d:%d:%d%15s", &timeinfo.tm_year, &timeinfo.tm_mon, &timeinfo.tm_mday,
                 &timeinfo.tm_hour, &timeinfo.tm_min, &timeinfo.tm_sec, (char*) &ms);
  if (c < 7)
    return 0;
  
  ms[15] = '\0';
  
  timeinfo.tm_mon -= 1;
  timeinfo.tm_year -= 1900;
  

  uint64_t time = (mktime(&timeinfo) - timezone) * 1000000;
  
  int ms_v = 0;
  int len = strlen(ms);
  if (len > 0) {
    ms_v = strtol(ms + 1, 0, 10);
    for (int pf = 7 - len; pf > 0; pf--)
      ms_v *= 10;
  }
  
  return time + ms_v;
}


static string::size_type insertPrefix(string &aPath, string::size_type &aPos,
				      const string aPrefix)
{
  aPath.insert(aPos, aPrefix);
  aPos += aPrefix.length();
  aPath.insert(aPos++, ":");

  return aPos;
}

static bool hasNamespace(const string &aPath, string::size_type aStart)
{
  string::size_type len = aPath.length(), pos = aStart;
  while (pos < len) 
  {
    if (aPath[pos] == ':')
      return true;
    else if (!isalpha(aPath[pos]))
      return false;
    pos++;
  }

  return false;
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
    if (!hasNamespace(newPath, pos))
      insertPrefix(newPath, pos, aPrefix);
  }
  
  
  while ((pos = newPath.find('/', pos)) != string::npos && pos < newPath.length() - 1)
  {
    pos ++;
    if (newPath[pos] == '/')
      pos++;

    // Make sure there are no existing prefixes
    if (newPath[pos] != '*' && newPath[pos] != '\0' && !hasNamespace(newPath, pos))
      insertPrefix(newPath, pos, aPrefix);
  }

  pos = 0;
  while ((pos = newPath.find('|', pos)) != string::npos)
  {
    pos ++;
    if (newPath[pos] != '/' && !hasNamespace(newPath, pos))
      insertPrefix(newPath, pos, aPrefix);
  }
  
  return newPath;
}

bool isMTConnectUrn(const char *aUrn)
{
  return strncmp(aUrn, "urn:mtconnect.org:MTConnect", 27) == 0;
}

#if 0
long getMemorySize()
{
  long size = 0;
  
#ifdef _WINDOWS
  HANDLE myself = GetCurrentProcess();
  PROCESS_MEMORY_COUNTERS memory;
  if (GetProcessMemoryInfo(myself, &memory, sizeof(memory))) {
    size = (long) (memory.PeakWorkingSetSize / 1024);
  }
#else
  struct rusage memory;
  if (getrusage(RUSAGE_SELF, &memory) == 0) {
    size = memory.ru_maxrss;
  }
#endif
  
  return size;
}
#endif
