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

#include "data_set.hpp"
#include "utilities.hpp"

#define BOOST_SPIRIT_DEBUG 1

// Parser section
#include <boost/config/warning_disable.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/phoenix_function.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_action.hpp>
#include <boost/spirit/include/support_istream_iterator.hpp>

namespace qi = boost::spirit::qi;
namespace phx = boost::phoenix;
namespace ascii = boost::spirit::ascii;
namespace spirit = boost::spirit;
namespace l = boost::lambda;


namespace std {
  static inline ostream &operator<<(ostream &s,
                                    const mtconnect::observation::DataSetValue &t)
  {
    visit(mtconnect::overloaded {[&s](const monostate &) { s << "NULL"; },
      [&s](const std::string &st) {s << "string(" << st << ")"; },
      [&s](const int64_t &i) { s << "int(" << i << ")"; },
      [&s](const double &d) { s << "double("  << d << ")"; },
      [&s](const mtconnect::observation::DataSet &arg) {
        s << "{" << arg << "}";
                      }},
          t);
    return s;
  }
  
  static inline ostream &operator<<(ostream &s,
                                    const mtconnect::observation::DataSetEntry &t)
  {
    s << t.m_key << "='" << t.m_value << "' " << t.m_removed;
    return s;
  }

}  // namespace std

using namespace std;

namespace mtconnect {
  namespace observation {
    struct data_set
    {
      inline static void f(DataSet &ds, const DataSetEntry &entry)
      {
        ds.emplace(entry);
      }
    };
    BOOST_PHOENIX_ADAPT_FUNCTION(void, add_entry, data_set::f, 2);
    
    struct quoted
    {
      inline static void f(DataSetValue &value, const vector<char> &v)
      {
        value = string(v.data(), v.size());
      }
    };
    BOOST_PHOENIX_ADAPT_FUNCTION(void, make_string, quoted::f, 2);
    
    struct braced
    {
      using BracedValue = boost::variant<vector<char>, DataSetValue>;
      inline static void f(DataSetValue &value, const vector<BracedValue> &v)
      {
        //value = string(v.data(), v.size());
      }
    };
    BOOST_PHOENIX_ADAPT_FUNCTION(void, make_braced, braced::f, 2);


    struct entry
    {
      static void f(DataSetEntry &entry, const string &key, const boost::optional<DataSetValue> &v)
      {
        entry.m_key = key;
        if (v && !holds_alternative<monostate>(*v))
          entry.m_value = *v;
        else
          entry.m_removed = true;
      }
    };
    BOOST_PHOENIX_ADAPT_FUNCTION(void, make_entry, entry::f, 3);

    
    struct position
    {
      template <typename Iterator>
      static size_t f(Iterator it)
      {
        return it.position();
      }
    };
    BOOST_PHOENIX_ADAPT_FUNCTION(size_t, pos, position::f, 1);
    
    template <typename It>
    class DataSetParser : public qi::grammar<It, DataSet()>
    {
    public:
      DataSetParser(bool table) : DataSetParser::base_type(m_start)
      {
        using namespace qi;
        using phx::construct;
        using phx::val;
        using qi::_1;
        using qi::_2;
        using qi::lexeme;
        using qi::double_;
        using qi::long_long;
        using qi::on_error;
        using spirit::ascii::char_;
        using qi::as_string;
        
        m_key %= lexeme[+(char_ - (space | char_("=|")))];
        m_value %= (m_real | long_long | m_quoted | m_simple | m_braced);
        m_simple %= lexeme[+(char_ - (char_("\"'{") | space))];

        // If table make m_value recursive for quated and braced
        
                
        m_quoted %= omit[char_("\"'")[_a = _1]] >>
                    as_string[(*(char_ - (lit(_a) | '\\') | (lit('\\') > char_)))] >>
                    lit(_a);
        
        m_braced %= lit('{') >> as_string[(*(char_ - (lit('}') | '{' | '\\') | (lit('\\') > char_)))]
          >> lit('}');
                
        m_entry = (m_key >> -("=" >> -m_value))[make_entry(_val, _1, _2)];
        m_start = *(m_entry[add_entry(_val, _1)] >> *space) >> eoi;
        
        BOOST_SPIRIT_DEBUG_NODES((m_start)(m_quoted)(m_braced)(m_key)(m_value)(m_entry)(m_simple)(m_quoted));
        
        m_start.name("top");
        m_simple.name("simple");
        m_quoted.name("quoted");
        m_entry.name("entry");
        m_value.name("value");
        m_simple.name("simple");
        m_quoted.name("quoted");
        
//        on_error<fail>(m_entry, [&]() { LOG(error) << val("Error! Expecting ") << qi::_4
//          << val(" here: \"")
//          << construct<std::string>(qi::_3, qi::_2) << val("\"")
//          << val(" on position ") << pos(qi::_1) << std::endl; });
      }
      
    protected:
      qi::rule<It, DataSet()> m_start;
      qi::real_parser<double, qi::strict_real_policies<double> > m_real;
      qi::rule<It, char> m_escaped;
      qi::rule<It, DataSetValue(), qi::locals<char>> m_quoted;
      qi::rule<It, DataSetValue()> m_braced;
      qi::rule<It, string()> m_key;
      qi::rule<It, string()> m_simple;
      qi::rule<It, DataSetValue()> m_value;
      qi::rule<It, DataSetEntry()> m_entry;
    };
    
    // Split the data set entries by space delimiters and account for the
    // use of single and double quotes as well as curly braces
    void DataSet::parse(const std::string &text, bool table)
    {
      using boost::spirit::ascii::space;

      using Iterator = std::string::const_iterator;
      DataSetParser<Iterator> parser(table);
      
      auto s = text.begin();
      auto e = text.end();

      bool r = qi::parse(s, e, parser, *this);
      if (r && s == e)
      {
        if (table)
        {
          // parse the values for each entry
        }
        // cout << "Success: " << tree << endl;
      }
      else
      {
        LOG(error) << "Failed to parse data set: " << text;
      }
    }
  }
}
