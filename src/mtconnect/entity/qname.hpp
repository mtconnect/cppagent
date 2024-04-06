//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <optional>
#include <string>
#include <string_view>

#include "mtconnect/config.hpp"

namespace mtconnect {
  namespace entity {
    /// @brief Qualified name
    ///
    /// The qname uses the underlying string for storage and keeps a position
    /// to the namespace position
    class AGENT_LIB_API QName : public std::string
    {
    public:
      QName() = default;
      /// @brief Create a qualified name from name and ns
      /// @param name the name
      /// @param ns the namespace prefix
      QName(const std::string &name, const std::string &ns)
      {
        assign(ns + ":" + name);
        m_nsLen = ns.length();
      }

      /// @brief Create a qualified name from a string
      /// @param qname the name
      QName(const std::string &qname) { setQName(qname); }

      /// @brief Set the qualified name. Parses the qname and looks for a colon and splits the
      ///        name into the namespace prefix and the name
      /// @param qname
      /// @param ns
      void setQName(const std::string &qname, const std::optional<std::string> &ns = std::nullopt)
      {
        if (ns)
        {
          assign(*ns + ":" + qname);
          m_nsLen = ns->length();
        }
        else
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
      }

      /// @brief copy constructor
      /// @param other the source
      QName(const QName &other) = default;
      ~QName() = default;

      /// @brief operator =
      /// @param name the source
      /// @return this qname
      QName &operator=(const std::string &name)
      {
        setQName(name);
        return *this;
      }

      /// @brief set the name portion
      /// @param name
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

      /// @brief is there a namespace
      /// @return `true` if there is a namespace
      bool hasNs() const { return m_nsLen > 0; }

      /// @brief set the namespace portion
      /// @param ns the namespace
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

      /// @brief clear the string and the namespace
      void clear()
      {
        std::string::clear();
        m_nsLen = 0;
      }

      /// @brief get this qname
      /// @return this
      const auto &getQName() const { return *this; }

      /// @brief get a string view to the name portion of the qname
      /// @return string view of the name
      const std::string_view getName() const
      {
        if (m_nsLen == 0)
          return std::string_view(*this);
        else
          return std::string_view(c_str() + m_nsLen + 1);
      }
      /// @brief get a stringview to the namespace portion
      /// @return string view of the namespace or an empty string view
      const std::string_view getNs() const
      {
        if (m_nsLen == 0)
          return std::string_view();
        else
          return std::string_view(c_str(), m_nsLen);
      }
      /// @brief get a pair of strings with the namespace and the name
      /// @return namespace and the name
      const std::pair<std::string, std::string> getPair() const
      {
        if (m_nsLen > 0)
        {
          return {std::string(getNs()), std::string(getName())};
        }
        else
        {
          return {std::string(), *this};
        }
      }

      /// @brief get the qname as a string
      /// @return this
      std::string &str() { return *this; }
      /// @brief const get this as a string
      /// @return this
      const std::string &str() const { return *this; }

    protected:
      size_t m_nsLen;
    };
  }  // namespace entity
}  // namespace mtconnect
