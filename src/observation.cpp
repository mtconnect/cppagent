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

#include "observation.hpp"

#include "data_item.hpp"

#include <dlib/logger.h>
#include <dlib/threads.h>

#include <mutex>
#include <regex>

#ifdef _WINDOWS
#define strcasecmp stricmp
#define strncasecmp strnicmp
#define strtof strtod
#endif

using namespace std;

namespace mtconnect
{
  static std::mutex g_attributeMutex;
  static dlib::logger g_logger("Observation");

  const string Observation::SLevels[NumLevels] = {"Normal", "Warning", "Fault", "Unavailable"};

  inline static bool splitValue(string &key, string &value)
  {
    auto found = key.find_first_of(':');

    if (found == string::npos)
    {
      return false;
    }
    else
    {
      value = key.substr(found + 1);
      key.erase(found);
      return true;
    }
  }

  Observation::Observation(DataItem &dataItem, uint64_t sequence, const string &time,
                           const string &value)
      : m_hasAttributes(false)
  {
    m_dataItem = &dataItem;
    m_isTimeSeries = m_dataItem->isTimeSeries();
    m_sequence = sequence;
    auto pos = time.find('@');

    if (pos != string::npos)
    {
      m_time = time.substr(0, pos);
      m_duration = time.substr(pos + 1);
    }
    else
      m_time = time;

    if (m_dataItem->hasResetTrigger())
    {
      string v = value, reset;
      if (splitValue(v, reset))
      {
        m_resetTriggered = reset;
        if (m_dataItem->hasInitialValue())
          v = m_dataItem->getInitialValue();
      }
      convertValue(v);
    }
    else
      convertValue(value);
  }

  Observation::Observation(const Observation &observation)
      : m_dataItem(observation.m_dataItem),
        m_sequence(observation.m_sequence),
        m_time(observation.m_time),
        m_duration(observation.m_duration),
        m_rest(observation.m_rest),
        m_value(observation.m_value),
        m_isTimeSeries(observation.m_isTimeSeries),
        m_hasAttributes(false),
        m_code(observation.m_code),
        m_resetTriggered(observation.m_resetTriggered)
  {
    if (m_isTimeSeries)
    {
      m_timeSeries = observation.m_timeSeries;
      m_sampleCount = observation.m_sampleCount;
    }
    else if (observation.isDataSet())
    {
      m_dataSet = observation.m_dataSet;
      m_sampleCount = m_dataSet.size();
    }
  }

  Observation::~Observation()
  {
  }

  const AttributeList &Observation::getAttributes()
  {
    lock_guard<std::mutex> lock(g_attributeMutex);
    if (!m_hasAttributes)
    {
      m_attributes.push_back(AttributeItem("dataItemId", m_dataItem->getId()));
      m_attributes.push_back(AttributeItem("timestamp", m_time));

      if (!m_dataItem->getName().empty())
        m_attributes.push_back(AttributeItem("name", m_dataItem->getName()));

      if (!m_dataItem->getCompositionId().empty())
        m_attributes.push_back(AttributeItem("compositionId", m_dataItem->getCompositionId()));

      m_sequenceStr = int64ToString(m_sequence);
      m_attributes.push_back(AttributeItem("sequence", m_sequenceStr));

      if (!m_dataItem->getSubType().empty())
        m_attributes.push_back(AttributeItem("subType", m_dataItem->getSubType()));

      if (!m_dataItem->getStatistic().empty())
        m_attributes.push_back(AttributeItem("statistic", m_dataItem->getStatistic()));

      if (!m_duration.empty())
        m_attributes.push_back(AttributeItem("duration", m_duration));

      if (!m_resetTriggered.empty())
        m_attributes.push_back(AttributeItem("resetTriggered", m_resetTriggered));

      if (m_dataItem->isCondition())
      {
        // Conditon data: LEVEL|NATIVE_CODE|NATIVE_SEVERITY|QUALIFIER
        istringstream toParse(m_rest);
        string token;

        getline(toParse, token, '|');

        if (!strcasecmp(token.c_str(), "normal"))
          m_level = NORMAL;
        else if (!strcasecmp(token.c_str(), "warning"))
          m_level = WARNING;
        else if (!strcasecmp(token.c_str(), "fault"))
          m_level = FAULT;
        else  // Assume unavailable
          m_level = UNAVAILABLE;

        if (!toParse.eof())
        {
          getline(toParse, token, '|');

          if (!token.empty())
          {
            m_code = token;
            m_attributes.push_back(AttributeItem("nativeCode", token));
          }
        }

        if (!toParse.eof())
        {
          getline(toParse, token, '|');

          if (!token.empty())
            m_attributes.push_back(AttributeItem("nativeSeverity", token));
        }

        if (!toParse.eof())
        {
          getline(toParse, token, '|');

          if (!token.empty())
            m_attributes.push_back(AttributeItem("qualifier", token));
        }

        m_attributes.push_back(AttributeItem("type", m_dataItem->getType()));
      }
      else if (m_dataItem->isTimeSeries())
      {
        istringstream toParse(m_rest);
        string token;

        getline(toParse, token, '|');

        if (token.empty())
          token = "0";

        m_attributes.push_back(AttributeItem("sampleCount", token));
        m_sampleCount = atoi(token.c_str());

        getline(toParse, token, '|');

        if (!token.empty())
          m_attributes.push_back(AttributeItem("sampleRate", token));
      }
      else if (m_dataItem->isMessage())
      {
        // Format to parse: NATIVECODE
        if (!m_rest.empty())
          m_attributes.push_back(AttributeItem("nativeCode", m_rest));
      }
      else if (m_dataItem->isAlarm())
      {
        // Format to parse: CODE|NATIVECODE|SEVERITY|STATE
        istringstream toParse(m_rest);
        string token;

        getline(toParse, token, '|');
        m_attributes.push_back(AttributeItem("code", token));

        getline(toParse, token, '|');
        m_attributes.push_back(AttributeItem("nativeCode", token));

        getline(toParse, token, '|');
        m_attributes.push_back(AttributeItem("severity", token));

        getline(toParse, token, '|');
        m_attributes.push_back(AttributeItem("state", token));
      }
      else if (m_dataItem->isDataSet())
      {
        m_attributes.push_back(AttributeItem("count", intToString(m_dataSet.size())));
        m_sampleCount = m_dataSet.size();
      }
      else if (m_dataItem->isAssetChanged() || m_dataItem->isAssetRemoved())
        m_attributes.push_back(AttributeItem("assetType", m_rest));

      m_hasAttributes = true;
    }

    return m_attributes;
  }

  void Observation::normal()
  {
    if (m_dataItem->isCondition())
    {
      m_attributes.clear();
      m_code.clear();
      m_hasAttributes = false;
      m_rest = "normal|||";
      getAttributes();
    }
  }

#define WS_RE "[ \t]*"
#define KEY_RE "([^ \t=]+)"
#define DQ_RE "\"([^\\\\\"]+(\\\\\")?)+\""
#define SQ_RE "'([^\\\\']+(\\\\')?)+'"
#define CB_RE "\\{([^\\}\\\\]+(\\\\\\})?)+\\}"
#define VAL_RE "[^ \t]+"

  static const char *reg = WS_RE KEY_RE "(=(" DQ_RE "|" SQ_RE "|" CB_RE "|" VAL_RE ")?)?";
  static regex tokenizer(reg);

  // Split the data set entries by space delimiters and account for the
  // use of single and double quotes as well as curly braces
  void Observation::parseDataSet(const string &s)
  {
    smatch m;
    string rest(s);

    try
    {
      // Search for key value pairs. Handle quoted text.
      while (regex_search(rest, m, tokenizer))
      {
        string key, value;
        bool removed = false;
        if (!m[3].matched)
        {
          key = m[1];
          if (!m[2].matched)
            removed = true;
          else
            value.clear();
        }
        else
        {
          key = m[1];
          string v = m[3];

          // Check for invalid termination of string
          if ((v.front() == '"' && v.back() != '"') || (v.front() == '\'' && v.back() != '\'') ||
              (v.front() == '{' && v.back() != '}'))
          {
            // consider the rest of the set invalid, issue warning.
            break;
          }

          if (v.front() == '"' || v.front() == '\'' || v.front() == '{')
            value = v.substr(1, v.size() - 2);
          else
            value = v;

          // character remove escape

          size_t pos = 0;
          do
          {
            pos = value.find('\\', pos);
            if (pos != string::npos)
            {
              value.erase(pos, 1);
              pos++;
            }
          } while (pos != string::npos && pos < value.size());
        }

        // Map the value.
        m_dataSet.emplace(key, value, removed);

        // Parse the rest of the string...
        rest = m.suffix();
      }
    }

    catch (regex_error &e)
    {
      g_logger << dlib::LWARN << "Error parsing \"" << rest << "\", \nReason: " << e.what();
    }

    // If there is leftover text, the text was invalid.
    // Warn that it is being discarded
    if (!rest.empty())
    {
      g_logger << dlib::LWARN << "Cannot parse complete string, malformed data set: '" << rest
               << "'";
    }
  }

  void Observation::convertValue(const string &value)
  {
    // Check if the type is an alarm or if it doesn't have units
    if (value == "UNAVAILABLE")
      m_value = value;
    else if (m_isTimeSeries || m_dataItem->isCondition() || m_dataItem->isAlarm() ||
             m_dataItem->isMessage() || m_dataItem->isAssetChanged() ||
             m_dataItem->isAssetRemoved())
    {
      auto lastPipe = value.rfind('|');

      // Alarm data = CODE|NATIVECODE|SEVERITY|STATE
      // Conditon data: SEVERITY|NATIVE_CODE|[SUB_TYPE]
      // Asset changed: type|id
      m_rest = value.substr(0, lastPipe);

      // sValue = DESCRIPTION
      if (m_isTimeSeries)
      {
        auto cp = value.c_str();
        cp += lastPipe + 1;

        // Check if conversion is required...
        char *np(nullptr);

        while (cp && *cp != '\0')
        {
          float v = strtof(cp, &np);

          if (cp != np)
          {
            m_timeSeries.push_back(m_dataItem->convertValue(v));
          }
          else
            np = nullptr;

          cp = np;
        }
      }
      else
        m_value = value.substr(lastPipe + 1);
    }
    else if (m_dataItem->isDataSet())
    {
      string set = value;

      // Check for reset triggered
      if (set[0] == ':')
      {
        auto found = set.find_first_of(' ');
        string trig(set);
        if (found != string::npos)
          trig.erase(found);
        trig.erase(0, 1);
        if (!trig.empty())
        {
          m_resetTriggered = trig;
          if (found != string::npos)
            set.erase(0, found + 1);
          else
            set.clear();
        }
      }

      parseDataSet(set);
    }
    else if (m_dataItem->conversionRequired())
      m_value = m_dataItem->convertValue(value);
    else
      m_value = value;
  }

  Observation *Observation::getFirst()
  {
    if (m_prev.getObject())
      return m_prev->getFirst();
    else
      return this;
  }

  void Observation::getList(std::list<ObservationPtr> &list)
  {
    if (m_prev.getObject())
      m_prev->getList(list);

    list.push_back(this);
  }

  Observation *Observation::find(const std::string &code)
  {
    if (m_code == code)
      return this;

    if (m_prev.getObject())
      return m_prev->find(code);

    return nullptr;
  }

  bool Observation::replace(Observation *oldObservation, Observation *newObservation)
  {
    auto obj = m_prev.getObject();

    if (!obj)
      return false;

    if (obj == oldObservation)
    {
      newObservation->m_prev = oldObservation->m_prev;
      m_prev = newObservation;
      return true;
    }

    return m_prev->replace(oldObservation, newObservation);
  }

  Observation *Observation::deepCopy()
  {
    auto n = new Observation(*this);

    if (m_prev.getObject())
    {
      n->m_prev = m_prev->deepCopy();
      n->m_prev->unrefer();
    }

    return n;
  }

  Observation *Observation::deepCopyAndRemove(Observation *old)
  {
    if (this == old)
    {
      if (m_prev.getObject())
        return m_prev->deepCopy();
      else
        return nullptr;
    }

    auto n = new Observation(*this);

    if (m_prev.getObject())
    {
      n->m_prev = m_prev->deepCopyAndRemove(old);

      if (n->m_prev.getObject())
        n->m_prev->unrefer();
    }

    return n;
  }
}  // namespace mtconnect
