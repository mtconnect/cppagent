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

#include <boost/asio/ip/address.hpp>
#include <boost/lexical_cast.hpp>

#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>

#include "mtconnect/config.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect::source::adapter::agent_adapter {
  using UrlQueryPair = std::pair<std::string, std::string>;

  /// @brief A map of URL query parameters that can format as a string
  struct AGENT_LIB_API UrlQuery : public std::map<std::string, std::string>
  {
    using std::map<std::string, std::string>::map;

    /// @brief join the parameters as `<key1>=<value1>&<key2>=<value2>&...`
    /// @return
    std::string join() const
    {
      std::stringstream ss;
      bool has_pre = false;

      for (const auto& kv : *this)
      {
        if (has_pre)
          ss << '&';

        ss << kv.first << '=' << kv.second;
        has_pre = true;
      }

      return ss.str();
    }

    /// @brief Merge twos sets over-writing existing pairs set with `query` and adding new pairs
    /// @param query query to merge
    void merge(UrlQuery query)
    {
      for (const auto& kv : query)
      {
        insert_or_assign(kv.first, kv.second);
      }
    }
  };

  /// @brief URL struct to parse and format URLs
  struct AGENT_LIB_API Url
  {
    /// @brief Variant for the Host that is either a host name or an ip address
    using Host = std::variant<std::string, boost::asio::ip::address>;

    std::string m_protocol;  ///< either `http` or `https`

    Host m_host;                            ///< the host component
    std::optional<std::string> m_username;  ///< optional username
    std::optional<std::string> m_password;  ///< optional password
    std::optional<int> m_port;              ///< The optional port number (defaults to 5000)
    std::string m_path = "/";               ///< The path component
    UrlQuery m_query;                       ///< Query parameters
    std::string m_fragment;                 ///< The component after a `#`

    /// @brief Visitor to format the Host as a string
    struct HostVisitor
    {
      std::string operator()(std::string v) const { return v; }

      std::string operator()(boost::asio::ip::address v) const { return v.to_string(); }
    };

    /// @brief Get the host as a string
    /// @return the host
    std::string getHost() const { return std::visit(HostVisitor(), m_host); }
    /// @brief Get the port as a string
    /// @return the port
    std::string getService() const { return boost::lexical_cast<std::string>(getPort()); }

    /// @brief Get the path and the query portion of the URL
    /// @return the path and query
    std::string getTarget() const
    {
      if (m_query.size())
        return m_path + '?' + m_query.join();
      else
        return m_path;
    }

    /// @brief Format a target using the existing host and port to make a request
    /// @param device an optional device
    /// @param operation the operation (probe,sample,current, or asset)
    /// @param query query parameters
    /// @return A string with the target for a GET reuest
    std::string getTarget(const std::optional<std::string>& device, const std::string& operation,
                          const UrlQuery& query) const
    {
      UrlQuery uq {m_query};
      if (!query.empty())
        uq.merge(query);

      std::stringstream path;
      path << m_path;
      if (m_path[m_path.size() - 1] != '/')
        path << '/';
      if (device)
        path << *device << '/';
      if (!operation.empty())
        path << operation;
      if (uq.size() > 0)
        path << '?' << uq.join();

      return path.str();
    }

    int getPort() const
    {
      if (m_port)
        return *m_port;
      else if (m_protocol == "https")
        return 443;
      else if (m_protocol == "http")
        return 80;
      else
        return 0;
    }

    /// @brief Format the URL as text
    /// @param device optional device to add to the URL
    /// @return formatted URL
    std::string getUrlText(const std::optional<std::string>& device)
    {
      std::stringstream url;
      url << m_protocol << "://" << getHost() << ':' << getPort() << getTarget();
      if (device)
        url << *device;
      return url.str();
    }

    /// @brief parse a string to a Url
    /// @return parsed URL
    static Url parse(const std::string_view& url);
  };

}  // namespace mtconnect::source::adapter::agent_adapter
