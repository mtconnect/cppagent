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

#include "unit_conversion.hpp"

using namespace std;

namespace mtconnect {
  namespace device_model {
    namespace data_item {
      std::unordered_map<string, UnitConversion> UnitConversion::m_conversions(
          {{"INCH-MILLIMETER", 25.4},
           {"FOOT-MILLIMETER", 304.8},
           {"CENTIMETER-MILLIMETER", 10.0},
           {"DECIMETER-MILLIMETER", 100.0},
           {"METER-MILLIMETER", 1000.0},
           {"FAHRENHEIT-CELSIUS", {(5.0 / 9.0), -32.0}},
           {"POUND-GRAM", 453.59237},
           {"GRAM-KILOGRAM", 1 / 1000.0},
           {"RADIAN-DEGREE", 57.2957795},
           {"SECOND-MINUTE", 1.0 / 60.0},
           {"MINUTE-SECOND", 60.0},
           {"POUND/INCH^2-PASCAL", 6894.76},
           {"HOUR-SECOND", 3600.0}});

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

      static pair<double, double> scaleAndPower(string_view &unit)
      {
        double power = 1.0, scale = 1.0;

        if (unit.compare(0, 4, "KILO") == 0)
        {
          scale = 1000;
          unit.remove_prefix(4);
        }
        else if (unit.compare(0, 6, "CUBIC_") == 0)
        {
          unit.remove_prefix(6);
          power = 3.0;
        }
        else if (auto p = unit.find('^'); p != string_view::npos)
        {
          power = stod(string(unit.substr(p + 1)));
          unit.remove_suffix(unit.length() - p);
        }

        return {scale, power};
      }

      std::unique_ptr<UnitConversion> UnitConversion::make(const std::string &from,
                                                           const std::string &to)
      {
        if (from == to)
          return nullptr;

        string key(from);
        key = key.append("-").append(to);

        const auto &conversion = m_conversions.find(string(key));
        if (conversion != m_conversions.end())
          return make_unique<UnitConversion>(conversion->second);

        double factor {1.0}, offset {0.0};
        std::string_view source(from);
        std::string_view target(to);

        // Always convert back to MTConnect Units.
        auto t3D = target.rfind("_3D");
        auto s3D = source.rfind("_3D");
        if (t3D != string_view::npos && s3D != string_view::npos)
        {
          source.remove_suffix(3);
          target.remove_suffix(3);
        }
        else if (t3D != string_view::npos || s3D != string_view::npos)
          return nullptr;

        auto sslash = source.find('/');
        auto tslash = target.find('/');
        if (sslash == string_view::npos && tslash == string_view::npos)
        {
          auto [sscale, spower] = scaleAndPower(source);
          auto [tscale, tpower] = scaleAndPower(target);

          if (spower != tpower)
            return nullptr;

          factor = sscale / tscale;

          key = source;
          key = key.append("-").append(target);

          const auto &conversion = m_conversions.find(string(key));
          // Check for no support units and not power or factor scaling.
          if (conversion == m_conversions.end() && factor == 1.0)
            return nullptr;
          else if (conversion != m_conversions.end())
          {
            factor *= conversion->second.factor();
            offset = conversion->second.offset();
          }

          if (tpower != 1.0)
            factor = pow(factor, tpower);
        }
        else if (sslash == string_view::npos || tslash == string_view::npos)
        {
          return nullptr;
        }
        else
        {
          string_view snumerator(source.substr(0, sslash));
          string_view tnumerator(target.substr(0, tslash));
          string_view sdenominator(source.substr(sslash + 1));
          string_view tdenominator(target.substr(tslash + 1));

          auto num = make(string(snumerator), string(tnumerator));
          auto den = make(string(sdenominator), string(tdenominator));
          auto n = num ? num->factor() : 1.0;
          auto d = den ? den->factor() : 1.0;

          factor = n / d;
        }

        return make_unique<UnitConversion>(factor, offset);
      }
    }  // namespace data_item
  }    // namespace device_model
}  // namespace mtconnect
