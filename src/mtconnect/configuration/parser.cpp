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

#include "parser.hpp"

#include <fstream>
#include <ostream>
#include <regex>
#include <iterator>

#include "mtconnect/utilities.hpp"

// #define BOOST_SPIRIT_DEBUG 1
#ifdef BOOST_SPIRIT_DEBUG
namespace std {
  static ostream &operator<<(ostream &s, const boost::property_tree::ptree &t);
  static inline ostream &operator<<(ostream &s,
                                    const pair<std::string, boost::property_tree::ptree> &t)
  {
    s << "'" << t.first << "'" << t.second;
    return s;
  }

  static inline ostream &operator<<(ostream &s, const pair<std::string, std::string> &t)
  {
    s << "Pair: '" << t.first << "', '" << t.second << "'";
    return s;
  }

  static ostream &operator<<(ostream &s, const boost::property_tree::ptree &t)
  {
    if (!t.data().empty())
    {
      s << " = '" << t.data() << "'";
    }
    if (!t.empty())
    {
      s << " [Tree: ";
      for (const auto &c : t)
        s << c << ", ";
      s << "]";
    }
    return s;
  }
}  // namespace std
#endif

#include <boost/config/warning_disable.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/phoenix.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_action.hpp>
#include <boost/spirit/include/support_line_pos_iterator.hpp>

namespace qi = boost::spirit::qi;
namespace phx = boost::phoenix;
namespace ascii = boost::spirit::ascii;
namespace spirit = boost::spirit;
namespace pt = boost::property_tree;
namespace l = boost::lambda;

using namespace std;

namespace mtconnect {
  namespace configuration {
    /// @brief Actions for the configuration parser in reductions
    namespace ConfigurationParserActions {
      inline static void property(pair<std::string, std::string> &t, const std::string &f,
                                  const std::string &s)
      {
        t = make_pair(f, trim(s));
      }

      inline static void tree(pair<std::string, pt::ptree> &t, const std::string &f,
                              const vector<pair<std::string, pt::ptree>> &s)
      {
        t.first = f;
        for (const auto &a : s)
        {
          if (!a.first.empty())
            t.second.push_back(a);
        }
      }

      inline static void start(pt::ptree &t, const vector<pair<std::string, pt::ptree>> &s)
      {
        for (const auto &a : s)
        {
          if (!a.first.empty())
            t.push_back(a);
        }
      }

      template <typename Iterator>
      inline static size_t pos(Iterator it)
      {
        return it.position();
      }
    }  // namespace ConfigurationParserActions
    BOOST_PHOENIX_ADAPT_FUNCTION(void, property, ConfigurationParserActions::property, 3);
    BOOST_PHOENIX_ADAPT_FUNCTION(void, tree, ConfigurationParserActions::tree, 3);
    BOOST_PHOENIX_ADAPT_FUNCTION(void, start, ConfigurationParserActions::start, 2);
    BOOST_PHOENIX_ADAPT_FUNCTION(size_t, pos, ConfigurationParserActions::pos, 1);

    /// @brief boost Spirit parser
    /// @tparam It iterator type for consuming text
    /// @tparam Skipper skip whitespace by default
    template <typename It, typename Skipper = ascii::space_type>
    class ConfigParser : public qi::grammar<It, pt::ptree(), Skipper>
    {
    public:
      ConfigParser() : ConfigParser::base_type(m_start)
      {
        using namespace qi;
        using phx::construct;
        using phx::val;
        using qi::_1;
        using qi::_2;
        using qi::lexeme;
        using qi::on_error;
        using spirit::ascii::char_;

        m_name %= lexeme[+(char_ - (space | char_("=\\{}") | eol))];
        m_line %= no_skip[+(char_ - (char_("}#") | eol))];
        m_string %= omit[char_("\"'")[_a = _1]] >> no_skip[*(char_ - char_(_a))] >> lit(_a);
        m_value %= *blank > (m_string | m_line);
        m_property = (m_name >> "=" > m_value > (eol | &char_("}#")))[property(_val, _1, _2)];
        m_tree = (m_name >> *eol >> "{" >> *m_node > "}")[tree(_val, _1, _2)];
        m_blank = eol;
        m_node = (m_property | m_tree | m_blank);
        m_start = (*m_node)[start(_val, _1)];

        BOOST_SPIRIT_DEBUG_NODES((m_start)(m_tree)(m_property)(m_node));

        m_start.name("top");
        m_tree.name("block");
        m_property.name("property");
        m_value.name("value");
        m_name.name("name");

        on_error<fail>(m_property, std::cerr << val("Error! Expecting ") << qi::_4
                                             << val(" here: \"")
                                             << construct<std::string>(qi::_3, qi::_2) << val("\"")
                                             << val(" on line ") << pos(qi::_1) << std::endl);
        on_error<fail>(m_tree, std::cerr << val("Error! Expecting ") << qi::_4 << val(" here: \"")
                                         << construct<std::string>(qi::_3, qi::_2) << val("\"")
                                         << val(" on line ") << pos(qi::_1) << std::endl);
      }

    protected:
      qi::rule<It, string(), Skipper> m_name;
      qi::rule<It, string(), Skipper> m_line;
      qi::rule<It, string(), Skipper, qi::locals<char>> m_string;
      qi::rule<It, string(), Skipper> m_value;
      qi::rule<It, pair<std::string, std::string>(), Skipper> m_property;
      qi::rule<It, pair<std::string, pt::ptree>(), Skipper> m_tree;
      qi::rule<It, pair<std::string, pt::ptree>(), Skipper> m_node;
      qi::rule<It> m_blank;
      qi::rule<It, pt::ptree(), Skipper> m_start;
    };

    static std::string ExpandValue(const std::map<std::string, std::string> &values,
                                   const std::string &s)
    {
      static std::regex pat("\\$(([A-Za-z0-9_]+)|\\{([^}]+)\\})");
      stringstream out;
      std::sregex_iterator iter(s.begin(), s.end(), pat);
      std::sregex_iterator end;
      std::sregex_iterator::value_type::value_type suf;

      if (iter == end)
      {
        return s;
      }

      while (iter != end)
      {
        out << iter->prefix().str();
        string sym;
        if ((*iter)[3].matched)
          sym = (*iter)[3].str();
        else if ((*iter)[2].matched)
          sym = (*iter)[2].str();

        // Resolve match text
        auto opt = values.find(sym);
        if (opt != values.end())
        {
          out << opt->second;
        }
        else if (auto env = getenv(sym.c_str()))
        {
          out << env;
        }
        else
        {
          out << iter->str();
        }

        suf = iter->suffix();
        iter++;
      }

      out << suf.str();

      return out.str();
    }

    static void ExpandValues(std::map<std::string, std::string> values,
                             boost::property_tree::ptree &node)
    {
      if (auto value = node.get_value_optional<std::string>(); value->find('$') != std::string::npos)
      {
        auto expanded = ExpandValue(values, *value);
        node.put_value(expanded);
      }

      for (auto &block : node)
      {
        ExpandValues(values, block.second);
        const auto &value = block.second.get_value_optional<std::string>();
        if (value && !value->empty())
          values[block.first] = *value;
      }
    }

    static void ExpandVariables(boost::property_tree::ptree &config)
    {
      std::map<std::string, std::string> values;
      ExpandValues(values, config);
    }
    
    pt::ptree Parser::parse(const std::string &text)
    {
      pt::ptree tree;
      using boost::spirit::ascii::space;

      using Iterator = spirit::line_pos_iterator<string::const_iterator>;
      using Skipper = qi::rule<Iterator>;
      Skipper comment, skipper;
      {
        using namespace qi;
        using spirit::ascii::char_;
        using spirit::ascii::string;
        comment = lit('#') >> *(char_ - eol);
        skipper = blank | comment;
      }

      ConfigParser<Iterator, Skipper> parser;

      auto s = Iterator(text.begin());
      auto e = Iterator(text.end());
      bool r = phrase_parse(s, e, parser, skipper, tree);
      if (r && s == e)
      {
        // cout << "Success: " << tree << endl;
      }
      else
      {
        cout << "Failed" << endl;
        cout << "Stopped at line: " << s.position() << endl;
        throw ParseError("Failed to parse configuration");
      }
      
      ExpandVariables(tree);

      return tree;
    }
    
    pt::ptree Parser::parse(const std::filesystem::path &path)
    {
      std::ifstream t(path);
      std::stringstream buffer;
      buffer << t.rdbuf();

      cout << "Parsing file: " << path << endl;
      return parse(buffer.str());
    }

  }  // namespace configuration
}  // namespace mtconnect
