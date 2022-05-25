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

#include "factory.hpp"

#include <unordered_set>

#include "logging.hpp"

using namespace std;

namespace mtconnect {
  namespace entity {
    void Factory::_dupFactory(FactoryPtr &factory, FactoryMap &factories)
    {
      auto old = factories.find(factory);
      if (old != factories.end())
      {
        factory = old->second;
      }
      else
      {
        auto ptr = make_shared<Factory>(*factory);
        factories.emplace(factory, ptr);
        ptr->_deepCopy(factories);
        factory = ptr;
      }
    }

    void Factory::_deepCopy(FactoryMap &factories)
    {
      for (auto &r : m_requirements)
      {
        auto factory = r.getFactory();
        if (factory)
        {
          _dupFactory(factory, factories);
          r.setFactory(factory);
        }
      }

      for (auto &f : m_matchFactory)
      {
        _dupFactory(f.second, factories);
      }

      for (auto &f : m_stringFactory)
      {
        _dupFactory(f.second, factories);
      }
    }

    FactoryPtr Factory::deepCopy() const
    {
      auto copy = make_shared<Factory>(*this);
      map<FactoryPtr, FactoryPtr> factories;
      copy->_deepCopy(factories);

      return copy;
    }

    void Factory::LogError(const std::string &what) { LOG(warning) << what; }

    void Factory::performConversions(Properties &properties, ErrorList &errors) const
    {
      for (const auto &r : m_requirements)
      {
        if (r.getType() != ENTITY && r.getType() != ENTITY_LIST)
        {
          const auto p = properties.find(r.getName());
          if (p != properties.end() && p->second.index() != r.getType())
          {
            try
            {
              Value &v = p->second;
              ConvertValueToType(v, r.getType());
            }
            catch (PropertyError &e)
            {
              LOG(warning) << "Error occurred converting " << r.getName() << ": " << e.what();
              e.setProperty(r.getName());
              errors.emplace_back(e.dup());
              properties.erase(p);
            }
          }
        }
      }
    }

    bool Factory::isSufficient(Properties &properties, ErrorList &errors) const
    {
      NAMED_SCOPE("EntityFactory");
      bool success {true};
      for (auto &p : properties)
        p.first.clearMark();

      if (m_isList && m_minListSize > 0)
      {
        const auto p = properties.find("LIST");
        if (p == properties.end())
        {
          errors.emplace_back(new PropertyError("A list is required for this entity"));
          success = false;
        }
        else
        {
          p->first.setMark();
          auto &list = get<EntityList>(p->second);
          if (list.size() < m_minListSize)
          {
            errors.emplace_back(new PropertyError("The list must have at least " +
                                                  to_string(m_minListSize) + " entries"));
            success = false;
          }
        }
      }

      for (const auto &r : m_requirements)
      {
        Properties::const_iterator p;
        if (m_isList && r.getType() == ENTITY)
          p = properties.find("LIST");
        else
          p = properties.find(r.getName());
        if (p == properties.end())
        {
          if (r.isRequired())
          {
            errors.emplace_back(new PropertyError(
                "Property " + r.getName() + " is required and not provided", r.getName()));
            success = false;
          }
        }
        else
        {
          p->first.setMark();
          try
          {
            if (!r.isMetBy(p->second, m_isList))
            {
              success = false;
            }
          }
          catch (PropertyError &e)
          {
            LogError(e.what());
            if (r.isRequired())
            {
              success = false;
            }
            else
            {
              LogError("Not required, skipping " + r.getName());
              properties.erase(p->first);
            }
            e.setProperty(r.getName());
            errors.emplace_back(e.dup());
          }
        }
      }

      if (!m_any && !m_isList)
      {
        std::list<string> extra;
        for (auto &p : properties)
        {
          // Check that all properties are expected and if they are not, allow
          // xml attributes through if the namespace portion starts with xml.
          if (!p.first.m_mark && (!p.first.hasNs() || p.first.getNs().find_first_of("xml") != 0))
          {
            extra.emplace_back(p.first);
          }
        }

        // Check for additional properties
        if (!extra.empty())
        {
          std::stringstream os;
          os << "The following keys were present and not expected: ";
          for (auto &k : extra)
            os << k << ",";
          errors.emplace_back(new PropertyError(os.str()));
          success = false;
        }
      }

      return success;
    }
  }  // namespace entity
}  // namespace mtconnect
