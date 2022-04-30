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

#include <nlohmann/json.hpp>

constexpr inline nlohmann::json::size_type operator"" _S(unsigned long long v)
{
  return static_cast<nlohmann::json::size_type>(v);
}

inline std::string operator"" _S(const char *v, std::size_t) { return std::string(v); }

static inline nlohmann::json find(nlohmann::json &array, const char *path, const char *value)
{
  nlohmann::json::json_pointer pointer(path);
  nlohmann::json res;
  for (auto &item : array)
  {
    if (item.at(pointer).get<std::string>() == value)
    {
      res = item;
      break;
    }
  }
  return res;
}
