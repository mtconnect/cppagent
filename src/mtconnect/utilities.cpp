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

#include "mtconnect/utilities.hpp"

#include <boost/asio.hpp>
#include <boost/config/warning_disable.hpp>
#include <boost/phoenix.hpp>
#include <boost/spirit/include/qi.hpp>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <date/date.h>
#include <date/tz.h>
#include <list>
#include <map>
#include <mutex>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "logging.hpp"

// Don't include WinSock.h when processing <windows.h>
#ifdef _WINDOWS
#define _WINSOCKAPI_
#include <windows.h>
#include <winsock2.h>
#define localtime_r(t, tm) localtime_s(tm, t)
#define gmtime_r(t, tm) gmtime_s(tm, t)

#define DELTA_EPOCH_IN_MICROSECS 11644473600000000ull
#endif

using namespace std;
using namespace std::chrono;

namespace qi = boost::spirit::qi;

/// @brief User credentials struct for intermediate parsing
struct UserCred
{
  std::string m_username;
  std::string m_password;
};

/// @brief make the struct available to spirit
BOOST_FUSION_ADAPT_STRUCT(UserCred, (std::string, m_username)(std::string, m_password))

/// @brief make the struct available to spirit
BOOST_FUSION_ADAPT_STRUCT(mtconnect::url::UrlQueryPair, (std::string, first)(std::string, second))

/// @brief make the struct available to spirit
BOOST_FUSION_ADAPT_STRUCT(mtconnect::url::Url,
                          (std::string, m_protocol)(mtconnect::url::Url::Host,
                                                    m_host)(std::optional<std::string>, m_username)(
                              std::optional<std::string>, m_password)(std::optional<int>, m_port)(
                              std::string, m_path)(mtconnect::url::UrlQuery, m_query)(std::string,
                                                                                      m_fragment))

namespace mtconnect {
  AGENT_LIB_API void mt_localtime(const time_t *time, struct tm *buf) { localtime_r(time, buf); }

  inline string::size_type insertPrefix(string &aPath, string::size_type &aPos,
                                        const string aPrefix)
  {
    aPath.insert(aPos, aPrefix);
    aPos += aPrefix.length();
    aPath.insert(aPos++, ":");

    return aPos;
  }

  inline bool hasNamespace(const string &aPath, string::size_type aStart)
  {
    string::size_type len = aPath.length(), pos = aStart;

    while (pos < len)
    {
      if (aPath[pos] == ':')
        return true;
      else if (!isalpha(aPath[pos]))
        return false;

      pos++;
    }

    return false;
  }

  AGENT_LIB_API string addNamespace(const string aPath, const string aPrefix)
  {
    if (aPrefix.empty())
      return aPath;

    string newPath = aPath;
    string::size_type pos = 0u;

    // Special case for relative pathing
    if (newPath[pos] != '/')
    {
      if (!hasNamespace(newPath, pos))
        insertPrefix(newPath, pos, aPrefix);
    }

    while ((pos = newPath.find('/', pos)) != string::npos && pos < newPath.length() - 1)
    {
      pos++;

      if (newPath[pos] == '/')
        pos++;

      // Make sure there are no existing prefixes
      if (newPath[pos] != '*' && newPath[pos] != '\0' && !hasNamespace(newPath, pos))
        insertPrefix(newPath, pos, aPrefix);
    }

    pos = 0u;

    while ((pos = newPath.find('|', pos)) != string::npos)
    {
      pos++;

      if (newPath[pos] != '/' && !hasNamespace(newPath, pos))
        insertPrefix(newPath, pos, aPrefix);
    }

    return newPath;
  }

  std::string GetBestHostAddress(boost::asio::io_context &context, bool onlyV4)
  {
    using namespace boost;
    using namespace asio;
    using res = ip::tcp::resolver;

    string address;

    boost::system::error_code ec;
    res resolver(context);
    auto results = resolver.resolve(ip::host_name(), "5000", res::flags::address_configured, ec);
    if (ec)
    {
      LOG(warning) << "Cannot find IP address: " << ec.message();
      address = "127.0.0.1";
    }
    else
    {
      for (auto &res : results)
      {
        const auto &ad = res.endpoint().address();
        if (!ad.is_unspecified() && !ad.is_loopback() && (!onlyV4 || !ad.is_v6()))
        {
          auto ads {ad.to_string()};
          if (ads.length() > address.length() ||
              (ads.length() == address.length() && ads > address))
          {
            address = ads;
          }
        }
      }
    }

    return address;
  }

  namespace url {
    /// @brief Convert from four uns8s to an ipv4 address
    /// @param n1 first octet
    /// @param n2 second octet
    /// @param n3 third octet
    /// @param n4 fourth octet
    /// @return a boost ipv4 address
    static boost::asio::ip::address_v4 from_four_number(unsigned char n1, unsigned char n2,
                                                        unsigned char n3, unsigned char n4)
    {
      boost::asio::ip::address_v4::bytes_type bt;

      bt[0] = n1;
      bt[1] = n2;
      bt[2] = n3;
      bt[3] = n4;

      return boost::asio::ip::address_v4(bt);
    }

    /// @brief convert a string to an ipv6 address
    /// @param str the string (as a char vector)
    /// @return a boost ipv6 address
    static boost::asio::ip::address_v6 from_v6_string(std::vector<char> str)
    {
      return boost::asio::ip::make_address_v6(str.data());
    }

    BOOST_PHOENIX_ADAPT_FUNCTION(boost::asio::ip::address_v4, v4_from_4number, from_four_number, 4)
    BOOST_PHOENIX_ADAPT_FUNCTION(boost::asio::ip::address_v6, v6_from_string, from_v6_string, 1)

    /// @brief The Uri parser spirit qi grammar
    /// @tparam Iterator
    template <typename Iterator>
    struct UriGrammar : qi::grammar<Iterator, Url()>
    {
      UriGrammar() : UriGrammar::base_type(url)
      {
        using namespace boost::phoenix;
        using boost::phoenix::ref;

        url = schema[at_c<0>(qi::_val) = qi::_1] >> "://" >>
              -(username[at_c<2>(qi::_val) = qi::_1] >>
                -(':' >> password[at_c<3>(qi::_val) = qi::_1]) >>
                qi::lit('@')[boost::phoenix::ref(has_user_name) = true]) >>
              host[at_c<1>(qi::_val) = qi::_1] >>
              -(qi::lit(':') >> qi::int_[at_c<4>(qi::_val) = qi::_1]) >>
              -(path[at_c<5>(qi::_val) = qi::_1] >> -('?' >> query[at_c<6>(qi::_val) = qi::_1])) >>
              -('#' >> fragment[at_c<7>(qi::_val) = qi::_1]);

        host = ip_host | domain_host;

        domain_host = qi::lexeme[+(qi::char_("a-zA-Z0-9.\\-"))];
        ip_host = ('[' >> ipv6_host >> ']') | ipv4_host;

        ipv6_host = (+qi::char_("0123456789abcdefABCDEF:."))[qi::_val = v6_from_string(qi::_1)];

        ipv4_host = (qi::int_ >> '.' >> qi::int_ >> '.' >> qi::int_ >> '.' >>
                     qi::int_)[qi::_val = v4_from_4number(qi::_1, qi::_2, qi::_3, qi::_4)];

        username = qi::lexeme[+(qi::char_ - ':' - '@' - '/')];
        password = qi::lexeme[+(qi::char_ - '@')];

        schema = qi::lexeme[+(qi::char_ - ':' - '/')];

        path = qi::lexeme[+(qi::char_ - '?')];

        query = pair >> *((qi::lit(';') | '&') >> pair);
        pair = key >> -('=' >> value);
        key = qi::lexeme[+(qi::char_ - '=' - '#')];
        value = qi::lexeme[*(qi::char_ - '&' - '#')];

        fragment = qi::lexeme[+(qi::char_)];
      };

      qi::rule<Iterator, Url()> url;
      qi::rule<Iterator, std::string()> schema, path;

      qi::rule<Iterator, std::variant<std::string, boost::asio::ip::address>()> host;

      qi::rule<Iterator, std::string()> domain_host;
      qi::rule<Iterator, boost::asio::ip::address()> ip_host;

      qi::rule<Iterator, boost::asio::ip::address_v4()> ipv4_host;
      qi::rule<Iterator, boost::asio::ip::address_v6()> ipv6_host;
      ;

      qi::rule<Iterator, std::string()> username, password;

      qi::rule<Iterator, UrlQuery()> query;
      qi::rule<Iterator, UrlQueryPair()> pair;
      qi::rule<Iterator, std::string()> key, value;

      qi::rule<Iterator, std::string()> fragment;

      bool has_user_name = false;
    };

    Url Url::parse(const std::string_view &url)
    {
      Url ast;
      UriGrammar<std::string_view::const_iterator> grammar;

      auto first = url.begin();

      [[maybe_unused]] bool r = boost::spirit::qi::parse(first, url.end(), grammar, ast);
      if (!grammar.has_user_name)
      {
        ast.m_username.reset();
        ast.m_password.reset();
      }
      return ast;
    }
  }  // namespace url

}  // namespace mtconnect
