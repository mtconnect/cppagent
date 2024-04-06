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

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "mtconnect/config.hpp"
#include "mtconnect/entity/requirement.hpp"

namespace mtconnect::device_model::data_item {
  /// @brief Utility class to convert units from native to MTConnect
  class AGENT_LIB_API UnitConversion
  {
  public:
    /// @brief Create a unit conversion with a scalar factor and an offset
    /// @param[in] factor
    /// @param[in] offset
    UnitConversion(double factor = 1.0, double offset = 0.0) : m_factor(factor), m_offset(offset) {}
    UnitConversion(const UnitConversion &) = default;
    ~UnitConversion() = default;

    /// @brief convert a value
    /// @param[in] value the value
    /// @return converted value
    double convert(double value) const { return (value + m_offset) * m_factor; }

    /// @brief convert a vector of values
    /// @param[in] value the vector of double
    /// @return converted vector of doubles
    entity::Vector convert(const entity::Vector &value) const
    {
      entity::Vector res(value.size());
      for (size_t i = 0; i < value.size(); i++)
        res[i] = convert(value[i]);

      return res;
    }

    /// @brief convert a vector of values in place
    /// @param[in,out] value the vector of double
    void convert(entity::Vector &value) const
    {
      for (size_t i = 0; i < value.size(); i++)
        value[i] = convert(value[i]);
    }
    /// @brief Convert a entity variant Value if it holds a double or a vector of doubles
    /// @param[in] value a Value variant
    /// @return the converted value
    entity::Value convertValue(const entity::Value &value)
    {
      if (const auto &v = std::get_if<double>(&value))
        return {convert(*v)};
      else if (const auto &a = std::get_if<entity::Vector>(&value))
        return {convert(*a)};
      else
        return nullptr;
    }

    /// @brief Convert a entity variant Value if it holds a double or a vector of doubles in place
    /// @param[in,out] value a Value variant
    void convertValue(entity::Value &value)
    {
      if (const auto &v = std::get_if<double>(&value))
        value = convert(*v);
      else if (const auto &a = std::get_if<entity::Vector>(&value))
        convert(*a);
    }
    /// @brief add a scaling factor to the conversion
    void scale(double scale) { m_factor *= scale; }

    /// @brief Create a unit conversion
    /// @param[in] from units from
    /// @param[in] to units to
    /// @return A units conversion object
    static std::unique_ptr<UnitConversion> make(const std::string &from, const std::string &to);

    /// @brief get the factor
    /// @return the scaling factor
    double factor() const { return m_factor; }
    /// @brief get the offset
    /// @return the offset
    double offset() const { return m_offset; }

  protected:
    double m_factor;
    double m_offset;

    static std::unordered_map<std::string, UnitConversion> m_conversions;
    static std::unordered_set<std::string> m_mtconnectUnits;
  };
}  // namespace mtconnect::device_model::data_item
