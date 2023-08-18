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

#include "mtconnect/utilities.hpp"

#include <boost/asio.hpp>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <date/tz.h>
#include <date/date.h>
#include <list>
#include <map>
#include <mutex>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "logging.hpp"

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
  AGENT_LIB_API void mt_localtime(const time_t *time, struct tm *buf) { localtime_r(time, buf); }

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

  AGENT_LIB_API string addNamespace(const string aPath, const string aPrefix)
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

  std::string GetBestHostAddress(boost::asio::io_context &context, bool onlyV4)
  {
    using namespace boost;
    using namespace asio;
    using res = ip::udp::resolver;

    string address;

    boost::system::error_code ec;
    res resolver(context);
    auto iter = resolver.resolve(ip::host_name(), "5000", res::flags::address_configured, ec);
    if (ec)
    {
      LOG(warning) << "Cannot find IP address: " << ec.message();
      address = "127.0.0.1";
    }
    else
    {
      res::iterator end;
      while (iter != end)
      {
        const auto &ep = iter->endpoint();
        const auto &ad = ep.address();
        if (!ad.is_unspecified() && !ad.is_loopback() && (!onlyV4 || !ad.is_v6()))
        {
          auto ads {ad.to_string()};
          if (ads.length() > address.length() ||
              (ads.length() == address.length() && ads > address))
          {
            address = ads;
          }
        }
        iter++;
      }
    }

    return address;
  }
}  // namespace mtconnect
