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

#include "factory.hpp"
#include <dlib/logger.h>

using namespace std;

namespace mtconnect
{
  namespace entity {
    static dlib::logger g_logger("EntityFactory");
 
    void Factory::LogError(const std::string &what)
    {
      g_logger << dlib::LWARN << what;
    }
    
    void Factory::performConversions(Properties &properties, ErrorList &errors) const
    {
      for (const auto &r : m_requirements)
      {
        if (r.getType() != ENTITY && r.getType() != ENTITY_LIST)
        {
          const auto p = properties.find(r.getName());
          if (p != properties.end() && p->second.index() != r.getType())
          {
            try {
              Value &v = p->second;
              ConvertValueToType(v, r.getType());
            } catch (PropertyError &e) {
              g_logger << dlib::LWARN << "Error occurred converting " << r.getName() << ": " <<
                e.what();
              errors.emplace_back(e);
              properties.erase(p);
            }
          }
        }
      }
    }

    bool Factory::isSufficient(const Properties &properties, ErrorList &errors) const
    {
      std::set<std::string> keys;
      std::transform(properties.begin(), properties.end(), std::inserter(keys, keys.end()),
                     [](const auto &v) { return v.first; });
      
      for (const auto &r : m_requirements)
      {
        std::string key;
        if (m_isList)
          key = "list";
        else
          key = r.getName();
        const auto p = properties.find(key);
        if (p == properties.end())
        {
          if (r.isRequired())
          {
            throw MissingPropertyError("Property " + r.getName() +
                                       " is required and not provided");
          }
        }
        else
        {
          try {
            if (!r.isMetBy(p->second, m_isList))
            {
              return false;
            }
          }
          catch (PropertyError &e) {
            LogError(e.what());
            if (r.isRequired())
              throw;
            else
            {
              errors.emplace_back(e);
              LogError("Not required, skipping " + r.getName());
            }
          }
          keys.erase(r.getName());
        }
      }
      
      // Check for additional properties
      if (!m_isList && !keys.empty())
      {
        std::stringstream os;
        os << "The following keys were present and not expected: ";
        for (auto &k : keys)
          os << k << ",";
        throw ExtraPropertyError(os.str());
      }
      
      return true;
    }
  }
}

