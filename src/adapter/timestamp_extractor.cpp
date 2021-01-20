//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "shdr_parser.hpp"
#include "timestamp_extractor.hpp"

#include <date/date.h>
#include <optional>

using namespace std;

namespace mtconnect
{
  namespace adapter
  {

    static dlib::logger g_logger("TimestampExtractor");

    inline optional<double> getDuration(std::string &timestamp)
    {
      optional<double> duration;

      auto pos = timestamp.find('@');
      if (pos != string::npos)
      {
        auto read = pos + 1;;
        auto dur{timestamp.substr(read)};
        duration = std::stod(dur, &read);
        if (read == pos + 1)
          duration.reset();
        else
          timestamp = timestamp.erase(pos);
      }
      
      return duration;
    }
    
    void TimestampExtractor::extractTimestamp(ShdrObservation &obs,
                                              TokenList::const_iterator &token,
                                              const TokenList::const_iterator &end,
                                              Context &context)
    {
      using namespace date;

      // Extract duration
      
      string timestamp = *token++;
      obs.m_duration = getDuration(timestamp);
      
      if (context.m_ignoreTimestamps || timestamp.empty())
      {
        Timestamp now = context.m_now ? context.m_now() : chrono::system_clock::now();
        obs.m_timestamp = now;
        return;
      }
      
      Timestamp ts;
      bool has_t{ timestamp.find('T') != string::npos };
      if (has_t)
      {
        istringstream in(timestamp);
        in >> std::setw(6) >> parse("%FT%T", ts);
        if (!in.good())
        {
          ts = context.m_now ? context.m_now() : chrono::system_clock::now();
        }
        
        if (!context.m_relativeTime)
        {
          obs.m_timestamp = ts;
          return;
        }
      }
      
      // Handle double offset
      Timestamp now = context.m_now ? context.m_now() : chrono::system_clock::now();
      double offset;
      if (!has_t)
      {
        offset = stod(timestamp);
      }
      
      if (!context.m_base)
      {
        context.m_base = now;
        if (has_t)
          context.m_offset = now - ts;
        else
          context.m_offset = Micros(int64_t(offset * 1000.0));
        obs.m_timestamp = now;
      }
      else
      {
        if (has_t)
        {
          obs.m_timestamp = ts + context.m_offset;
        }
        else
        {
          obs.m_timestamp = *context.m_base +
            Micros(int64_t(offset * 1000.0)) - context.m_offset;
        }
      }
    }    
  }
}
