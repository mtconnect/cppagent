//
// Copyright Copyright 2009-2025, AMT – The Association For Manufacturing Technology (“AMT”)
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

/// @file
/// Tests for unit conversions module

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

/// @test check 3D conversion with inch to millimeter
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

/// @test check 3D unit conversions with radians to degrees
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

/// @test test kilo prefix with kilo amps to amps
TEST(UnitConversionTest, check_kilo_prefix)
{
  auto conv = UnitConversion::make("KILOAMPERE", "AMPERE");
  EXPECT_NEAR(130.0, conv->convert(0.13), 0.0001);
}

/// @test Test cubic inch to millimeter
TEST(UnitConversionTest, check_cubic_conversion)
{
  auto conv = UnitConversion::make("CUBIC_INCH", "CUBIC_MILLIMETER");
  EXPECT_NEAR(114709.44799, conv->convert(7.0), 0.0001);
}

/// @test Test temperature conversions of F to C
TEST(UnitConversionTest, check_temperature_conversions_with_offset)
{
  auto conv = UnitConversion::make("FAHRENHEIT", "CELSIUS");
  EXPECT_NEAR(-12.22222, conv->convert(10.0), 0.0001);
}

/// @test Check ratios conversion of ft/min to mm/s
TEST(UnitConversionTest, check_simple_ratio_conversion)
{
  auto conv = UnitConversion::make("FOOT/MINUTE", "MILLIMETER/SECOND");
  EXPECT_NEAR(35.56, conv->convert(7.0), 0.0001);
}

/// @test check foot/minute^2 to mm/s^s
TEST(UnitConversionTest, check_acceleration)
{
  auto conv = UnitConversion::make("FOOT/MINUTE^2", "MILLIMETER/SECOND^2");
  EXPECT_NEAR(0.592666667, conv->convert(7.0), 0.0001);
}

/// @test check lbs/in^2 to pascals
TEST(UnitConversionTest, check_special_pound_inch_squared)
{
  auto conv = UnitConversion::make("POUND/INCH^2", "PASCAL");
  EXPECT_NEAR(48263.32, conv->convert(7.0), 0.0001);
}

/// @test check conversion of rev/second to RPM
TEST(UnitConversionTest, check_revolution_per_second)
{
  auto conv = UnitConversion::make("REVOLUTION/SECOND", "REVOLUTION/MINUTE");
  EXPECT_NEAR(420.0, conv->convert(7.0), 0.0001);
}

/// @test check cubit feet/minute to cubic millimeter/second
TEST(UnitConversionTest, check_cubic_feet_per_minute)
{
  auto conv = UnitConversion::make("CUBIC_FOOT/MINUTE", "CUBIC_MILLIMETER/SECOND");
  EXPECT_NEAR(3303632.15, conv->convert(7.0), 0.1);
}

/// @test Check square feet to square millimeters conversion
TEST(UnitConversionTest, check_square_feet_to_square_millimeter)
{
  auto conv = UnitConversion::make("SQUARE_FOOT", "SQUARE_MILLIMETER");
  EXPECT_NEAR(650321.3, conv->convert(7.0), 0.1);
}

/// @test Tests volume conversion to liters and liters pre second
TEST(UnitConversionTest, test_volume_and_volume_per_time)
{
  ///      - Check gallon to liter conversion
  auto conv = UnitConversion::make("GALLON", "LITER");
  EXPECT_NEAR(64.35, conv->convert(17.0), 0.1);
  ///      - Check pint to liters
  conv = UnitConversion::make("PINT", "LITER");
  EXPECT_NEAR(8.04, conv->convert(17.0), 0.1);

  ///       - Check gallon/minute to liter/second
  conv = UnitConversion::make("GALLON/MINUTE", "LITER/SECOND");
  EXPECT_NEAR(1.0725, conv->convert(17.0), 0.001);
}

TEST(UnitConversionTest, check_conversion_from_kw_h_to_watt_second)
{
  auto conv = UnitConversion::make("KILOWATT/HOUR", "WATT/SECOND");
  EXPECT_NEAR(0.16666, conv->convert(0.6), 0.001);
  EXPECT_NEAR(0.25556, conv->convert(0.92), 0.001);

  conv = UnitConversion::make("KILOWATT_HOUR", "WATT_SECOND");
  EXPECT_NEAR(2160000.0, conv->convert(0.6), 0.001);
  EXPECT_NEAR(3312000.0, conv->convert(0.92), 0.001);
}
