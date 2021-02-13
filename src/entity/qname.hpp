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

#pragma once

namespace mtconnect
{
  namespace entity
  {
    class QName : public std::string
    {
    public:
      QName() = default;
      QName(const std::string &name, const std::string &ns)
      {
        setQName(ns + ":" + name);
      }

      QName(const std::string &name)
      {
        setQName(name);
      }
      
      void setQName(const std::string &qname)
      {
        this->assign(qname);
        if (auto pos = find(':'); pos != npos)
        {
          m_ns = std::string_view(c_str(), pos);
          m_name = std::string_view(c_str() + pos + 1);
        }
        else
        {
          m_name = qname;
          m_ns = std::string_view();
        }
      }
      
      QName(const QName &other) = default;
      ~QName() = default;
      
      QName &operator=(const std::string &name) {
        setName(name);
        return *this;
      }
      
      void setName(const std::string &name)
      {
        if (m_ns.empty())
        {
          this->assign(name);
          m_name = *this;
        }
        else
        {
          setQName(std::string(m_ns) + ':' + name);
        }
      }

      void setNs(const std::string &ns)
      {
        setQName(ns + ':' + std::string(m_name));
      }
      
      void clear()
      {
        std::string::clear();
        m_ns = std::string_view();
        m_name = std::string_view();
      }
      
      const auto &getQName() const { return *this; }
      const auto &getName() const { return m_name; }
      const auto &getNs() const { return m_ns; }
      
    protected:
      std::string_view m_name;
      std::string_view m_ns;
    };
  }
}
