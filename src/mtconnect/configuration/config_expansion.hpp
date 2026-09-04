//
// Copyright 2009-2026, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//

#pragma once

#include <boost/property_tree/ptree.hpp>

#include <cstdlib>
#include <map>
#include <regex>
#include <sstream>

namespace mtconnect::configuration {
  namespace detail {
    inline std::string expandConfigurationValue(const std::map<std::string, std::string>& values,
                                                const std::string& value)
    {
      static const std::regex pattern("\\$(([A-Za-z0-9_]+)|\\{([^}]+)\\})");
      std::stringstream output;
      std::sregex_iterator match(value.begin(), value.end(), pattern), end;
      std::sregex_iterator::value_type::value_type suffix;
      if (match == end)
        return value;

      while (match != end)
      {
        output << match->prefix().str();
        const auto symbol = (*match)[3].matched ? (*match)[3].str() : (*match)[2].str();
        if (const auto found = values.find(symbol); found != values.end())
          output << found->second;
        else if (const auto environment = std::getenv(symbol.c_str()))
          output << environment;
        else
          output << match->str();
        suffix = match->suffix();
        ++match;
      }

      output << suffix.str();
      return output.str();
    }

    inline void expandConfigurationValues(std::map<std::string, std::string> values,
                                          boost::property_tree::ptree& node)
    {
      if (const auto value = node.get_value_optional<std::string>();
          value && value->find('$') != std::string::npos)
        node.put_value(expandConfigurationValue(values, *value));

      for (auto& block : node)
      {
        expandConfigurationValues(values, block.second);
        if (const auto value = block.second.get_value_optional<std::string>(); value && !value->empty())
          values[block.first] = *value;
      }
    }
  }  // namespace detail

  /// Expands configuration references from previously visited sibling values and the environment.
  inline void expandConfigurationVariables(boost::property_tree::ptree& config)
  {
    detail::expandConfigurationValues({}, config);
  }
}
