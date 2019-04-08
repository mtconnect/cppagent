//
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

#include "data_item.hpp"
#include "device.hpp"
#include "adapter.hpp"
#include <map>
#include <string>

using namespace std;

namespace mtconnect {
  // Observation public static constants
  std::map<string, double> sUnitsConversion {
    { "INCH", 25.4 },
    { "FOOT", 304.8 },
    { "CENTIMETER", 10.0 },
    { "DECIMETER", 100.0 },
    { "METER", 1000.0 },
    { "FAHRENHEIT", (5.0 / 9.0) },
    { "POUND", 0.45359237 },
    { "GRAM", 1 / 1000.0 },
    { "RADIAN", 57.2957795 },
    { "MINUTE", 60.0 },
    { "HOUR", 3600.0 }
  };

  // DataItem public methods
  DataItem::DataItem(std::map<string, string> const &attributes) : 
  m_representation(VALUE),
  m_hasNativeScale(false),
  m_isDiscrete(false),
  m_hasSignificantDigits(false),
  m_hasConstraints(false),
  m_filterValue(0.0),
  m_hasMinimumDelta(false),
  m_hasMinimumPeriod(false),
  m_lastSampleValue(NAN),
  m_lastTimeOffset(NAN),
  m_dataSource(nullptr),
  m_conversionDetermined(false),
  m_conversionRequired(false),
  m_hasFactor(false)
  {
    const auto idPos = attributes.find("id");
    if(idPos != attributes.end())
      m_id = idPos->second;
    
    const auto namePos = attributes.find("name");
    if(namePos != attributes.end())
      m_name = namePos->second;
    
    const auto typePos = attributes.find("type");
    if(typePos != attributes.end())
      m_type = typePos->second;
    
    const auto allowDups = attributes.find("discrete");
    if(allowDups != attributes.end())
      m_isDiscrete = allowDups->second == "true";
    
    
    m_isAlarm = (m_type == "ALARM");
    m_isMessage = (m_type == "MESSAGE");
    m_isAssetChanged = (m_type == "ASSET_CHANGED");
    m_isAssetRemoved = (m_type == "ASSET_REMOVED");
    
    
    m_camelType = getCamelType(m_type, m_prefix);
    
    const auto repPos = attributes.find("representation");
    if(repPos != attributes.end())
    {
      if (repPos->second == "TIME_SERIES")
      {
        m_representation = TIME_SERIES;
        m_camelType += "TimeSeries";
      }
      else if (repPos->second == "DISCRETE")
      {
        m_representation = DISCRETE;
        m_camelType += "Discrete";
      }
      else if (repPos->second == "DATA_SET")
      {
        m_representation = DATA_SET;
        m_camelType += "DataSet";
      }
    }
    
    if (!m_prefix.empty())
      m_prefixedCamelType = m_prefix + ":" + m_camelType;
    else
      m_prefixedCamelType = m_camelType;
    
    m_threeD = false;
    
    const auto subTypePos = attributes.find("subType");
    if(subTypePos != attributes.end())
      m_subType = subTypePos->second;
    
    const auto catPos = attributes.find("category");
    if(catPos != attributes.end())
    {
      if (catPos->second == "SAMPLE")
        m_category = SAMPLE;
      else if (catPos->second == "CONDITION")
        m_category = CONDITION;
      else if (catPos->second == "EVENT")
        m_category = EVENT;
      else
      {
        // Raise an invlaid category exception...
      }
    }
    
    const auto nativeUnitsPos = attributes.find("nativeUnits");
    if(nativeUnitsPos != attributes.end())
      m_nativeUnits = nativeUnitsPos->second;
    
    const auto unitsPos = attributes.find("units");
    if(unitsPos != attributes.end())
    {
      m_units = unitsPos->second;
      if (m_nativeUnits.empty())
        m_nativeUnits = m_units;
    }
    
    const auto statisticPos = attributes.find("statistic");
    if(statisticPos != attributes.end())
      m_statistic = statisticPos->second;
    
    const auto sampleRatePos = attributes.find("sampleRate");
    if(sampleRatePos != attributes.end())
      m_sampleRate = sampleRatePos->second;
    
    const auto nativeScalePos = attributes.find("nativeScale");
    if(nativeScalePos != attributes.end())
    {
      m_nativeScale = atof(nativeScalePos->second.c_str());
      m_hasNativeScale = true;
    }
    
    const auto significantDigitsPos = attributes.find("significantDigits");
    if(significantDigitsPos != attributes.end())
    {
      m_significantDigits = atoi(significantDigitsPos->second.c_str());
      m_hasSignificantDigits = true;
    }
    
    const auto coordinateSystemPos = attributes.find("coordinateSystem");
    if(coordinateSystemPos != attributes.end())
      m_coordinateSystem = coordinateSystemPos->second;
    
    const auto compositionIdPos = attributes.find("compositionId");
    if(compositionIdPos != attributes.end())
      m_compositionId = compositionIdPos->second;
    
    m_component = nullptr;
    m_attributes = buildAttributes();
  }
  
  
  DataItem::~DataItem()
  {
  }
  
  
  void DataItem::setDataSource(Adapter *source)
  {
    if (m_dataSource != source)
      m_dataSource = source;
    
    if (!m_dataSource->conversionRequired())
    {
      m_conversionRequired = false;
      m_conversionDetermined = true;
    }
  }
  
  
  std::map<string, string> DataItem::buildAttributes() const
  {
    std::map<string, string> attributes;
    
    attributes["id"] = m_id;
    attributes["type"] = m_type;
    
    if (!m_subType.empty())
      attributes["subType"] = m_subType;
    
    switch (m_category)
    {
      case SAMPLE:
        attributes["category"] = "SAMPLE";
        break;
        
      case EVENT:
        attributes["category"] = "EVENT";
        break;
        
      case CONDITION:
        attributes["category"] = "CONDITION";
        break;
    }
    
    if (m_representation == TIME_SERIES)
      attributes["representation"] = "TIME_SERIES";
    
    if (m_representation == DISCRETE)
      attributes["representation"] = "DISCRETE";
    
    if (m_representation == DATA_SET)
      attributes["representation"] = "DATA_SET";
    
    if (!m_statistic.empty())
      attributes["statistic"] = m_statistic;
    
    if (!m_sampleRate.empty())
      attributes["sampleRate"] = m_sampleRate;
    
    if (!m_name.empty())
      attributes["name"] = m_name;
    
    if (!m_nativeUnits.empty())
      attributes["nativeUnits"] = m_nativeUnits;
    
    if (!m_units.empty())
      attributes["units"] = m_units;
    
    if (m_hasNativeScale)
      attributes["nativeScale"] = floatToString(m_nativeScale);
    
    if (m_hasSignificantDigits)
      attributes["significantDigits"] = intToString(m_significantDigits);
    
    if (!m_coordinateSystem.empty())
      attributes["coordinateSystem"] = m_coordinateSystem;
    
    if (!m_compositionId.empty())
      attributes["compositionId"] = m_compositionId;
    
    if (m_isDiscrete)
      attributes["discrete"] = "true";
    
    return attributes;
  }
  
  
  bool DataItem::hasName(const string &name) const
  {
    return m_id == name || m_name == name || (!m_source.empty() && m_source == name);
  }
  
  
  string DataItem::getCamelType(const string &type, string &prefix)
  {
    if (type.empty())
      return "";
    else if (type == "PH") // Exception to the rule.
      return "PH";
    
    string camel;
    auto colon = type.find(':');
    
    if (colon != string::npos)
    {
      prefix = type.substr(0ul, colon);
      camel = type.substr(colon + 1ul);
    }
    else
      camel = type;
    
    auto second = camel.begin();
    second++;
    transform(second, camel.end(), second, ::tolower);
    
    auto word = find(second, camel.end(), '_');
    
    while (word != camel.end())
    {
      camel.erase(word);
      camel.replace(word, word + 1ul, 1ul, ::toupper(*word));
      word = find(word, camel.end(), '_');
    }
    
    return camel;
  }
  
  
  bool DataItem::conversionRequired()
  {
    if (!m_conversionDetermined)
    {
      m_conversionDetermined = true;
      m_conversionRequired = !m_nativeUnits.empty();
    }
    
    return m_conversionRequired;
  }
  
  
  float DataItem::convertValue(float value)
  {
    if (!m_conversionDetermined)
      conversionRequired();
    
    if (!m_conversionRequired)
    {
      return value;
    }
    else if (m_hasFactor)
    {
      return (value + m_conversionOffset) * m_conversionFactor;
    }
    else
    {
      computeConversionFactors();
      return convertValue(value);
    }
  }
  
  
  string DataItem::convertValue(const string &value)
  {
    // Check if the type is an alarm or if it doesn't have units
    
    if (!m_conversionRequired)
    {
      return value;
    }
    else if (m_hasFactor)
    {
      if (m_threeD)
      {
        ostringstream result;
        string::size_type start = 0ul;
        
        for (int i = 0; i < 3; i++)
        {
          auto pos = value.find(" ", start);
          result << floatToString((atof(value.substr(start, pos).c_str()) + m_conversionOffset) * m_conversionFactor);
          
          if (pos != string::npos)
          {
            start = value.find_first_not_of(" ", pos);
            result << " ";
          }
        }
        
        return result.str();
      }
      else
      {
        return floatToString((atof(value.c_str()) + m_conversionOffset) * m_conversionFactor);
      }
    }
    else
    {
      computeConversionFactors();
      return convertValue(value);
    }
  }
  
  
  void DataItem::computeConversionFactors()
  {
    string units = m_nativeUnits;
    auto threeD = units.find("_3D");
    m_conversionOffset = 0.0;
    auto slashLoc = units.find('/');
    
    // Convert units of numerator / denominator (^ power)
    if (slashLoc == string::npos)
    {
      if (threeD != string::npos)
      {
        m_threeD = true;
        units = units.substr(0, threeD);
      }
      
      m_conversionFactor = simpleFactor(units);
      
      if (m_conversionFactor == 1.0)
      {
        if (m_units == units)
          m_conversionRequired = false;
        else if (units.substr(0ul, 4ul) == "KILO" && units.substr(4) == m_units)
          m_conversionFactor = 1000.0;
        else
          m_conversionRequired = false;
      }
    }
    else if (units == "REVOLUTION/MINUTE")
    {
      m_conversionFactor = 1.0;
      m_conversionRequired = false;
    }
    else
    {
      auto numerator = units.substr(0, slashLoc);
      auto denominator = units.substr(slashLoc + 1);
      
      auto carotLoc = denominator.find('^');
      
      if (numerator == "REVOLUTION" && denominator == "SECOND")
        m_conversionFactor = 60.0;
      else if (carotLoc == string::npos)
        m_conversionFactor = simpleFactor(numerator) / simpleFactor(denominator);
      else
      {
        auto unit = denominator.substr(0ul, carotLoc);
        auto power = denominator.substr(carotLoc + 1);
        
        double div = pow((double) simpleFactor(unit), (double) atof(power.c_str()));
        m_conversionFactor = simpleFactor(numerator) / div;
      }
    }
    
    if (m_hasNativeScale)
    {
      m_conversionRequired = true;
      m_conversionFactor /= m_nativeScale;
    }
    
    m_hasFactor = true;
  }
  
  
  void DataItem::setConversionFactor(double factor, double offset)
  {
    m_hasFactor = true;
    m_conversionDetermined = true;
    
    if (factor == 1.0 && offset == 0.0)
      m_conversionRequired = false;
    else
    {
      m_conversionFactor = factor;
      m_conversionOffset = offset;
      m_conversionRequired = true;
    }
  }
  
  
  double DataItem::simpleFactor(const string &units)
  {
    auto conv = sUnitsConversion.find(units);
    if (conv == sUnitsConversion.end())
    {
      return 1.0;
    } else if (conv->first == "FAHRENHEIT") {
      m_conversionOffset = -32.0;
    }
    return conv->second;
  }
  
  
  // Sort by: Device, Component, Category, DataItem
  bool DataItem::operator<(const DataItem &another) const
  {
    auto dev = m_component->getDevice();
    
    if (dev->getId() < another.getComponent()->getDevice()->getId())
      return true;
    else if (dev->getId() > another.getComponent()->getDevice()->getId())
      return false;
    else if (m_component->getId() < another.getComponent()->getId())
      return true;
    else if (m_component->getId() > another.getComponent()->getId())
      return false;
    else if (m_category < another.m_category)
      return true;
    else if (m_category == another.m_category)
      return m_id < another.m_id;
    else
      return false;
  }
}
