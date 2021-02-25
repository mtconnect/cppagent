//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "unit_conversion.hpp"

using namespace std;

namespace mtconnect
{
  namespace device_model
  {
    namespace data_item
    {
      std::unordered_map<string, UnitConversion> UnitConversion::m_conversions(
          {{"INCH", 25.4},
           {"FOOT", 304.8},
           {"MILLIMETER", 1.0},
           {"CENTIMETER", 10.0},
           {"DECIMETER", 100.0},
           {"METER", 1000.0},
           {"FAHRENHEIT", {(5.0 / 9.0), -32.0}},
           {"POUND", 0.45359237},
           {"GRAM", 1 / 1000.0},
           {"RADIAN", 57.2957795},
           {"SECOND", 1.0},
           {"MINUTE", 60.0},
           {"POUND/INCH^2", 6894.76},
           {"HOUR", 3600.0}});

      std::unordered_set<std::string> UnitConversion::m_mtconnectUnits(
          {"AMPERE",
           "CELSIUS",
           "COUNT",
           "DECIBEL",
           "DEGREE",
           "DEGREE_3D",
           "DEGREE/SECOND",
           "DEGREE/SECOND^2",
           "HERTZ",
           "JOULE",
           "KILOGRAM",
           "LITER",
           "LITER/SECOND",
           "MICRO_RADIAN",
           "MILLIMETER",
           "MILLIMETER_3D",
           "MILLIMETER/REVOLUTION",
           "MILLIMETER/SECOND",
           "MILLIMETER/SECOND^2",
           "NEWTON",
           "NEWTON_METER",
           "OHM",
           "PASCAL",
           "PASCAL_SECOND",
           "PERCENT",
           "PH",
           "REVOLUTION/MINUTE",
           "SECOND",
           "SIEMENS/METER",
           "VOLT",
           "VOLT_AMPERE",
           "VOLT_AMPERE_REACTIVE",
           "WATT",
           "WATT_SECOND",
           "REVOLUTION/SECOND",
           "REVOLUTION/SECOND^2",
           "GRAM/CUBIC_METER",
           "CUBIC_MILLIMETER",
           "CUBIC_MILLIMETER/SECOND",
           "CUBIC_MILLIMETER/SECOND^2",
           "MILLIGRAM",
           "MILLIGRAM/CUBIC_MILLIMETER",
           "MILLILITER",
           "COUNT/SECOND",
           "PASCAL/SECOND",
           "UNIT_VECTOR_3D"});

      std::unique_ptr<UnitConversion> UnitConversion::make(const std::string &units)
      {
        double factor{1.0}, offset{0.0};
        std::string_view target(units);

        if (m_mtconnectUnits.count(units) > 0)
          return nullptr;

        // Always convert back to MTConnect Units.
        auto threeD = target.rfind("_3D");
        if (threeD != string_view::npos)
        {
          target.remove_suffix(3);
        }

        const auto &special = m_conversions.find(string(target));
        if (special != m_conversions.end())
          return make_unique<UnitConversion>(special->second);

        auto slash = target.find('/');
        if (slash == string_view::npos)
        {
          double power{1.0};

          if (target.compare(0, 4, "KILO") == 0)
          {
            factor *= 1000;
            target.remove_prefix(4);
          }
          else if (target.compare(0, 6, "CUBIC_") == 0)
          {
            target.remove_prefix(6);
            power = 3.0;
          }
          else if (auto p = target.find('^'); p != string_view::npos)
          {
            power = stod(string(target.substr(p + 1)));
            target.remove_suffix(target.length() - p);
          }

          const auto &conversion = m_conversions.find(string(target));
          // Check for no support units and not power or factor scaling.
          if (conversion == m_conversions.end() && factor == 1.0)
            return nullptr;
          else if (conversion != m_conversions.end())
          {
            factor *= conversion->second.factor();
            offset = conversion->second.offset();
          }

          if (power != 1.0)
            factor = pow(factor, power);
        }
        else
        {
          string_view numerator(target.substr(0, slash));
          string_view denominator(target.substr(slash + 1));

          auto num = make(string(numerator));
          auto den = make(string(denominator));
          auto n = num->factor();

          // Revolutions are in minutes not seconds
          if (numerator == "REVOLUTION")
            n *= 60.0;

          factor = n / den->factor();
        }

        return make_unique<UnitConversion>(factor, offset);
      }
    }  // namespace data_item
  }    // namespace device_model
}  // namespace mtconnect
