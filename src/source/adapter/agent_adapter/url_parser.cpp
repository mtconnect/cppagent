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

#include "url_parser.hpp"

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/qi.hpp>

namespace qi = boost::spirit::qi;

struct UserCred
{
  std::string m_username;
  std::string m_password;
};

BOOST_FUSION_ADAPT_STRUCT(UserCred, (std::string, m_username)(std::string, m_password))

BOOST_FUSION_ADAPT_STRUCT(mtconnect::source::adapter::agent_adapter::UrlQueryPair,
                          (std::string, first)(std::string, second))

BOOST_FUSION_ADAPT_STRUCT(
    mtconnect::source::adapter::agent_adapter::Url,
    (std::string, m_protocol)(mtconnect::source::adapter::agent_adapter::Url::Host,
                              m_host)(std::optional<std::string>, m_username)(
        std::optional<std::string>, m_password)(std::optional<int>, m_port)(std::string, m_path)(
        mtconnect::source::adapter::agent_adapter::UrlQuery, m_query)(std::string, m_fragment))

namespace mtconnect::source::adapter::agent_adapter {

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

  static boost::asio::ip::address_v6 from_v6_string(std::vector<char> str)
  {
    return boost::asio::ip::address_v6::from_string(str.data());
  }

  BOOST_PHOENIX_ADAPT_FUNCTION(boost::asio::ip::address_v4, v4_from_4number, from_four_number, 4)
  BOOST_PHOENIX_ADAPT_FUNCTION(boost::asio::ip::address_v6, v6_from_string, from_v6_string, 1)

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

      path = qi::lexeme[+(qi::char_ - '?' - '#')];

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

  Url Url::parse(const std::string_view& url)
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
}  // namespace mtconnect::source::adapter::agent_adapter
