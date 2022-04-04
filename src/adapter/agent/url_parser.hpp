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

#pragma once

#include <boost/asio/ip/address.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace mtconnect::adapter::agent {
  using UrlQueryPair = std::pair<std::string, std::string>;

  struct UrlQuery : public std::vector<UrlQueryPair>
  {
    using std::vector<UrlQueryPair>::vector;
    std::string join() const;
  };

  struct Url
  {
    using Host = std::variant<std::string, boost::asio::ip::address>;

    std::string m_protocol;  // http/https

    Host m_host;
    std::optional<std::string> m_username;
    std::optional<std::string> m_password;
    std::optional<int> m_port;
    std::string m_path = "/";
    UrlQuery m_query;
    std::string m_fragment;

    std::string getHost() const;

    // the path with query and without fragment
    std::string getTarget() const;

    static Url parse(const std::string_view& url);
  };

}  // namespace mtconnect::adapter::agent
