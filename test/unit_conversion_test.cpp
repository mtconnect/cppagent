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

// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "mtconnect/device_model/data_item/unit_conversion.hpp"

using namespace std;
using namespace mtconnect::device_model::data_item;
using namespace mtconnect::entity;

// main
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

TEST(UnitConversionTest, check_inch_3d)
{
  auto conv = UnitConversion::make("INCH_3D", "MILLIMETER_3D");
  Value value {Vector {1.0, 2.0, 3.0}};
  conv->convertValue(value);
  auto vec = get<Vector>(value);
  EXPECT_NEAR(25.4, vec[0], 0.0001);
  EXPECT_NEAR(50.8, vec[1], 0.0001);
  EXPECT_NEAR(76.2, vec[2], 0.0001);
}

TEST(UnitConversionTest, check_radian_3d)
{
  auto conv = UnitConversion::make("RADIAN_3D", "DEGREE_3D");
  Value value {Vector {1.0, 2.0, 3.0}};
  conv->convertValue(value);
  auto vec = get<Vector>(value);
  EXPECT_NEAR(57.29578, vec[0], 0.0001);
  EXPECT_NEAR(114.5916, vec[1], 0.0001);
  EXPECT_NEAR(171.8873, vec[2], 0.0001);
}

TEST(UnitConversionTest, check_kilo_prefix)
{
  auto conv = UnitConversion::make("KILOAMPERE", "AMPERE");
  EXPECT_NEAR(130.0, conv->convert(0.13), 0.0001);
}

// Test cubic inch to millimeter
TEST(UnitConversionTest, check_cubic_conversion)
{
  auto conv = UnitConversion::make("CUBIC_INCH", "CUBIC_MILLIMETER");
  EXPECT_NEAR(114709.44799, conv->convert(7.0), 0.0001);
}

// Test temperature
TEST(UnitConversionTest, check_temperature_conversions_with_offset)
{
  auto conv = UnitConversion::make("FAHRENHEIT", "CELSIUS");
  EXPECT_NEAR(-12.22222, conv->convert(10.0), 0.0001);
}

// Check ratios
TEST(UnitConversionTest, check_simple_ratio_conversion)
{
  auto conv = UnitConversion::make("FOOT/MINUTE", "MILLIMETER/SECOND");
  EXPECT_NEAR(35.56, conv->convert(7.0), 0.0001);
}

TEST(UnitConversionTest, check_acceleration)
{
  auto conv = UnitConversion::make("FOOT/MINUTE^2", "MILLIMETER/SECOND^2");
  EXPECT_NEAR(0.592666667, conv->convert(7.0), 0.0001);
}

TEST(UnitConversionTest, check_special_pound_inch_squared)
{
  auto conv = UnitConversion::make("POUND/INCH^2", "PASCAL");
  EXPECT_NEAR(48263.32, conv->convert(7.0), 0.0001);
}

TEST(UnitConversionTest, check_revolution_per_second)
{
  auto conv = UnitConversion::make("REVOLUTION/SECOND", "REVOLUTION/MINUTE");
  EXPECT_NEAR(420.0, conv->convert(7.0), 0.0001);
}

TEST(UnitConversionTest, check_cubic_feet_per_minute)
{
  auto conv = UnitConversion::make("CUBIC_FOOT/MINUTE", "CUBIC_MILLIMETER/SECOND");
  EXPECT_NEAR(3303632.15, conv->convert(7.0), 0.1);
}
