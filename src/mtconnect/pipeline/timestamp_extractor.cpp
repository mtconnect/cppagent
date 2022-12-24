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

#include "timestamp_extractor.hpp"

#include <date/date.h>
#include <optional>

#include "logging.hpp"

using namespace std;

namespace mtconnect {
  namespace pipeline {
    inline optional<double> getDuration(std::string &timestamp)
    {
      optional<double> duration;

      auto pos = timestamp.find('@');
      if (pos != string::npos)
      {
        auto read = pos + 1;
        ;
        auto dur {timestamp.substr(read)};
        duration = std::stod(dur, &read);
        if (read == pos + 1)
          duration.reset();
        else
          timestamp = timestamp.erase(pos);
      }

      return duration;
    }

    void ExtractTimestamp::extractTimestamp(const std::string &token, TimestampedPtr &entity)
    {
      using namespace date;
      using namespace chrono;
      using namespace chrono_literals;
      using namespace date::literals;
      using namespace date;

      NAMED_SCOPE("TimestampExtractor");

      // Extract duration
      string timestamp = token;
      entity->m_duration = getDuration(timestamp);

      if (timestamp.empty())
      {
        entity->m_timestamp = now();
        return;
      }

      Timestamp ts;
      bool has_t {timestamp.find('T') != string::npos};
      if (has_t)
      {
        istringstream in(timestamp);
        in >> std::setw(6) >> parse("%FT%T", ts);
        if (!in.good())
        {
          ts = now();
        }

        if (!m_relativeTime)
        {
          entity->m_timestamp = ts;
          return;
        }
      }

      // Handle double offset
      Timestamp n = now();
      double offset;
      if (!has_t)
      {
        offset = stod(timestamp);
      }

      if (!m_base)
      {
        m_base = n;
        if (has_t)
        {
          auto t1 = round<Microseconds, system_clock>(ts);
          auto t2 = round<Microseconds, system_clock>(n);
          m_offset = t2 - t1;
        }
        else
          m_offset = Microseconds(int64_t(offset * 1000.0));
        entity->m_timestamp = n;
      }
      else
      {
        if (has_t)
        {
          entity->m_timestamp = ts + m_offset;
        }
        else
        {
          entity->m_timestamp = *m_base + Microseconds(int64_t(offset * 1000.0)) - m_offset;
        }
      }
    }
  }  // namespace pipeline
}  // namespace mtconnect
