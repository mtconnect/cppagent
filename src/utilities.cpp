//
// Copyright Copyright 2009-2022, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
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

#include "utilities.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <date/tz.h>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// Don't include WinSock.h when processing <windows.h>
#ifdef _WINDOWS
#define _WINSOCKAPI_
#include <windows.h>
#include <winsock2.h>
#define localtime_r(t, tm) localtime_s(tm, t)
#define gmtime_r(t, tm) gmtime_s(tm, t)

#define DELTA_EPOCH_IN_MICROSECS 11644473600000000ull
#endif

using namespace std;
using namespace std::chrono;

namespace mtconnect {
  void mt_localtime(const time_t *time, struct tm *buf) { localtime_r(time, buf); }

  uint64_t parseTimeMicro(const std::string &aTime)
  {
    struct tm timeinfo;
    memset(&timeinfo, 0, sizeof(timeinfo));
    char ms[16] = {0};

    int c = sscanf(aTime.c_str(), "%d-%d-%dT%d:%d:%d%15s", &timeinfo.tm_year, &timeinfo.tm_mon,
                   &timeinfo.tm_mday, &timeinfo.tm_hour, &timeinfo.tm_min, &timeinfo.tm_sec,
                   (char *)&ms);

    if (c < 7)
      return 0;

    ms[15] = '\0';

    timeinfo.tm_mon -= 1;
    timeinfo.tm_year -= 1900;

#ifndef _WINDOWS
    auto existingTz = getenv("TZ");
    setenv("TZ", "UTC", 1);
#endif

    uint64_t time = (mktime(&timeinfo) - timezone) * 1000000ull;

#ifndef _WINDOWS
    if (existingTz)
      setenv("TZ", existingTz, 1);
#endif

    uint64_t ms_v = 0;
    uint64_t len = strlen(ms);

    if (len > 0u)
    {
      ms_v = strtol(ms + 1, nullptr, 10);

      if (len < 8u)
        for (auto pf = 7 - len; pf > 0; pf--)
          ms_v *= 10;
    }

    return time + ms_v;
  }

  inline string::size_type insertPrefix(string &aPath, string::size_type &aPos,
                                        const string aPrefix)
  {
    aPath.insert(aPos, aPrefix);
    aPos += aPrefix.length();
    aPath.insert(aPos++, ":");

    return aPos;
  }

  inline bool hasNamespace(const string &aPath, string::size_type aStart)
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
    string::size_type pos = 0u;

    // Special case for relative pathing
    if (newPath[pos] != '/')
    {
      if (!hasNamespace(newPath, pos))
        insertPrefix(newPath, pos, aPrefix);
    }

    while ((pos = newPath.find('/', pos)) != string::npos && pos < newPath.length() - 1)
    {
      pos++;

      if (newPath[pos] == '/')
        pos++;

      // Make sure there are no existing prefixes
      if (newPath[pos] != '*' && newPath[pos] != '\0' && !hasNamespace(newPath, pos))
        insertPrefix(newPath, pos, aPrefix);
    }

    pos = 0u;

    while ((pos = newPath.find('|', pos)) != string::npos)
    {
      pos++;

      if (newPath[pos] != '/' && !hasNamespace(newPath, pos))
        insertPrefix(newPath, pos, aPrefix);
    }

    return newPath;
  }
}  // namespace mtconnect
