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
        assign(ns + ":" + name);
        m_nsLen = ns.length();
      }

      QName(const std::string &qname) { setQName(qname); }

      void setQName(const std::string &qname)
      {
        assign(qname);
        if (auto pos = find(':'); pos != npos)
        {
          m_nsLen = pos;
        }
        else
        {
          m_nsLen = 0;
        }
      }

      QName(const QName &other) = default;
      ~QName() = default;

      QName &operator=(const std::string &name)
      {
        setQName(name);
        return *this;
      }

      void setName(const std::string &name)
      {
        if (m_nsLen == 0)
        {
          assign(name);
        }
        else
        {
          std::string ns(getNs());
          assign(ns + ':' + name);
        }
      }

      bool hasNs() const { return m_nsLen > 0; }

      void setNs(const std::string &ns)
      {
        std::string name(getName());
        m_nsLen = ns.length();
        if (m_nsLen > 0)
        {
          assign(ns + ':' + name);
        }
        else
        {
          assign(name);
        }
      }

      void clear()
      {
        std::string::clear();
        m_nsLen = 0;
      }

      const auto &getQName() const { return *this; }
      const std::string_view getName() const
      {
        if (m_nsLen == 0)
          return std::string_view(*this);
        else
          return std::string_view(c_str() + m_nsLen + 1);
      }
      const std::string_view getNs() const
      {
        if (m_nsLen == 0)
          return std::string_view();
        else
          return std::string_view(c_str(), m_nsLen);
      }

    protected:
      size_t m_nsLen;
    };
  }  // namespace entity
}  // namespace mtconnect
