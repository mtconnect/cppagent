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

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "entity/requirement.hpp"

namespace mtconnect {
  namespace device_model {
    namespace data_item {
      class UnitConversion
      {
      public:
        UnitConversion(double factor = 1.0, double offset = 0.0)
          : m_factor(factor), m_offset(offset)
        {}
        UnitConversion(const UnitConversion &) = default;
        ~UnitConversion() = default;

        double convert(double value) const { return (value + m_offset) * m_factor; }

        entity::Vector convert(const entity::Vector &value) const
        {
          entity::Vector res(value.size());
          for (size_t i = 0; i < value.size(); i++)
            res[i] = convert(value[i]);

          return res;
        }
        void convert(entity::Vector &value) const
        {
          for (size_t i = 0; i < value.size(); i++)
            value[i] = convert(value[i]);
        }
        entity::Value convertValue(const entity::Value &value)
        {
          if (const auto &v = std::get_if<double>(&value))
            return {convert(*v)};
          else if (const auto &a = std::get_if<entity::Vector>(&value))
            return {convert(*a)};
          else
            return nullptr;
        }
        void convertValue(entity::Value &value)
        {
          if (const auto &v = std::get_if<double>(&value))
            value = convert(*v);
          else if (const auto &a = std::get_if<entity::Vector>(&value))
            convert(*a);
        }
        void scale(double scale) { m_factor *= scale; }

        static std::unique_ptr<UnitConversion> make(const std::string &from, const std::string &to);

        double factor() const { return m_factor; }
        double offset() const { return m_offset; }

      protected:
        double m_factor;
        double m_offset;

        static std::unordered_map<std::string, UnitConversion> m_conversions;
        static std::unordered_set<std::string> m_mtconnectUnits;
      };
    }  // namespace data_item
  }    // namespace device_model
}  // namespace mtconnect
