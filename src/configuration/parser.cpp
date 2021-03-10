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

#include "parser.hpp"
#include <ostream>
#include <fstream>

//#define BOOST_SPIRIT_DEBUG 1
namespace std
{
  static ostream& operator<<(ostream &s, const boost::property_tree::ptree &t);
  static inline ostream& operator<<(ostream& s, const pair<std::string, boost::property_tree::ptree> &t)
  {
    s << "'" << t.first << "'" << t.second;
    return s;
  }

  static inline ostream& operator<<(ostream& s, const pair<std::string, std::string> &t)
  {
    s << "Pair: '" << t.first << "', '" << t.second << "'";
    return s;
  }

  
  static ostream& operator<<(ostream &s, const boost::property_tree::ptree &t)
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
}


#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_function.hpp>
#include <boost/spirit/include/qi_action.hpp>
#include <boost/spirit/include/support_line_pos_iterator.hpp>
#include <boost/lambda/lambda.hpp>

namespace qi = boost::spirit::qi;
namespace phx = boost::phoenix;
namespace ascii = boost::spirit::ascii;
namespace spirit = boost::spirit;
namespace pt = boost::property_tree;
namespace l = boost::lambda;

using namespace std;

namespace mtconnect
{
  namespace configuration
  {
    struct property {
      static void f(pair<std::string, std::string> &t,
                    const std::string &f,
                    const std::string &s)
      {
        t = make_pair(f, s);
      }
    };
    BOOST_PHOENIX_ADAPT_FUNCTION(void, property, property::f, 3);

    struct tree {
      static void f(pair<std::string, pt::ptree> &t,
                    const std::string &f,
                    const vector<pair<std::string, pt::ptree>> &s)
      {
        t.first = f;
        for (const auto &a : s)
        {
          if (!a.first.empty())
            t.second.push_back(a);
        }
      }
    };
    BOOST_PHOENIX_ADAPT_FUNCTION(void, tree, tree::f, 3);
    
    struct top {
      static void f(pt::ptree &t, const vector<pair<std::string, pt::ptree>> &s )
      {
        for (const auto &a : s)
        {
          if (!a.first.empty())
            t.push_back(a);
        }
      }
    };
    BOOST_PHOENIX_ADAPT_FUNCTION(void, start, top::f, 2);
    
    struct position {
      template<typename Iterator>
      static size_t f(Iterator it) { return it.position(); }
    };
    BOOST_PHOENIX_ADAPT_FUNCTION(size_t, pos, position::f, 1);

    template<typename It, typename Skipper = ascii::space_type>
    class ConfigParser : public qi::grammar<It, pt::ptree(), Skipper>
    {
    public:
      
      ConfigParser() : ConfigParser::base_type(m_start)
      {
        using namespace qi;
        using qi::_1;
        using qi::_2;
        using qi::on_error;
        using phx::val;
        using phx::construct;
        using qi::lexeme;
        using spirit::ascii::char_;
        
        m_name %= lexeme[+(char_ - char_(" \t=\n{}"))];
        m_value %= *blank > no_skip[+(char_ - (char_('}') | eol))];
        m_property = (m_name >> "=" > m_value > (eol | &char_('}')))[property(_val, _1, _2)];
        m_tree = (m_name >> "{" >> *m_node > "}")[tree(_val, _1, _2)];
        m_blank = eol;
        m_node = (m_property | m_tree | m_blank);
        m_start = (*m_node)[start(_val, _1)];
        
        BOOST_SPIRIT_DEBUG_NODES((m_start)(m_tree)(m_property)(m_node));
        
        m_start.name("top");
        m_tree.name("block");
        m_property.name("property");
        m_value.name("value");
        m_name.name("name");

        on_error<fail>(m_property,
                       std::cerr
                       << val("Error! Expecting ")
                       << qi::_4
                       << val(" here: \"")
                       << construct<std::string>(qi::_3, qi::_2)
                       << val("\"")
                       << val(" on line ") << pos(qi::_1)
                       << std::endl
                       );
        on_error<fail>(m_tree,
                       std::cerr
                       << val("Error! Expecting ")
                       << qi::_4
                       << val(" here: \"")
                       << construct<std::string>(qi::_3, qi::_2)
                       << val("\"")
                       << val(" on line ") << pos(qi::_1)
                       << std::endl
            );

      }
      
      
      
    protected:
      qi::rule<It, string(), Skipper> m_name;
      qi::rule<It, string(), Skipper> m_value;
      qi::rule<It, pair<std::string, std::string>(), Skipper> m_property;
      qi::rule<It, pair<std::string, pt::ptree>(), Skipper> m_tree;
      qi::rule<It, pair<std::string, pt::ptree>(), Skipper> m_node;
      qi::rule<It> m_blank;
      qi::rule<It, pt::ptree(), Skipper> m_start;
    };
    
    pt::ptree Parser::parse(const std::string &text)
    {
      
      pt::ptree tree;
      using boost::spirit::ascii::space;
      
      using Iterator = spirit::line_pos_iterator<string::const_iterator>;
      using Skipper = qi::rule<Iterator>;
      Skipper comment, skipper;
      {
        using namespace qi;
        using spirit::ascii::string;
        using spirit::ascii::char_;
        comment = lit('#') >> *(char_ - eol);
        skipper = blank | comment;
      }
      
      ConfigParser<Iterator, Skipper> parser;
      
      auto s = Iterator(text.begin());
      auto e = Iterator(text.end());
      bool r = phrase_parse(s, e, parser, skipper, tree);
      if (r && s == e)
      {
        //cout << "Success: " << tree << endl;
      }
      else
      {
        cout << "Failed" << endl;
        cout << "Stopped at line: " << s.position() << endl;
        throw ParseError("Failed to parse configuration");
      }
      
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

  }
}
