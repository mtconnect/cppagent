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

#include <ostream>

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
        s << "{";
        for (const auto &v : arg) {
          s << v.m_key << "='" << v.m_value << "' " << v.m_removed << ", ";
        }
        s << "}";
      }}, t);
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
      inline static void add_entry_f(DataSet &ds, const DataSetEntry &entry)
      {
        ds.emplace(entry);
      }
      inline static void add_value_entry_f(DataSetValue &ds, const DataSetEntry &entry)
      {
        if (holds_alternative<monostate>(ds))
        {
          ds = DataSet();
        }
        if (holds_alternative<DataSet>(ds))
        {
          get<DataSet>(ds).emplace(entry);
        }
        else
        {
          LOG(error) << "Incorrect type for data set entry of table";
        }
      }
      static void make_entry_f(DataSetEntry &entry, const string &key, const boost::optional<DataSetValue> &v)
      {
        entry.m_key = key;
        if (v && !holds_alternative<monostate>(*v))
          entry.m_value = *v;
        else
          entry.m_removed = true;
      }
      static void make_entry_f(DataSetEntry &entry, const string &key, const boost::optional<DataSet> &v)
      {
        entry.m_key = key;
        if (v)
          entry.m_value = *v;
        else
          entry.m_removed = true;
      }
      template <typename Iterator>
      static size_t pos_f(Iterator it)
      {
        return it.position();
      }
    };
    BOOST_PHOENIX_ADAPT_FUNCTION(void, add_entry, data_set::add_entry_f, 2);
    BOOST_PHOENIX_ADAPT_FUNCTION(void, add_value_entry, data_set::add_value_entry_f, 2);
    BOOST_PHOENIX_ADAPT_FUNCTION(void, make_entry, data_set::make_entry_f, 3);
    BOOST_PHOENIX_ADAPT_FUNCTION(size_t, pos, data_set::pos_f, 1);
    
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
        
        // Data set parser
        m_key %= lexeme[+(char_ - (space | char_("=|{}'\"")))];
        m_value %= (m_real | long_long | m_quoted | m_simple | m_braced);
        m_simple %= lexeme[+(char_ - (char_("\"'{") | space))];

        m_quoted %= omit[char_("\"'")[_a = _1]] >>
                    as_string[(*(char_ - (lit(_a) | '\\') | (lit('\\') > char_)))] >>
                    lit(_a);
        
        m_braced %= lit('{') >> as_string[(*(char_ - (lit('}') | '{' | '\\') | (lit('\\') > char_)))]
          >> lit('}');
                        
        m_entry = (m_key >> -("=" >> -m_value))[make_entry(_val, _1, _2)];

        // Table support with quoted and braced content
        m_quotedDataSet = char_("\"'")[_a = _1] >> *space >> *(m_entry[add_entry(_val, _1)] >> *space) >>
        lit(_a);
        m_bracedDataSet = lit('{') >> *space >> *(m_entry[add_entry(_val, _1)] >> *space) >> lit('}');
        m_tableValue %= (m_quotedDataSet | m_bracedDataSet);
        m_tableEntry = (m_key >> -("=" >> -m_tableValue))[make_entry(_val, _1, _2)];
        
        if (table)
        {
          m_start = *space >> *(m_tableEntry[add_entry(_val, _1)] >> *space) >> eoi;
        }
        else
        {
          m_start = *space >> *(m_entry[add_entry(_val, _1)] >> *space) >> eoi;
        }
        
        BOOST_SPIRIT_DEBUG_NODES((m_start)(m_quoted)(m_braced)(m_key)(m_value)(m_entry)(m_simple)
                                 (m_quoted)(m_tableValue)(m_quotedDataSet)(m_bracedDataSet)(m_tableEntry));
        
        m_start.name("top");
        m_simple.name("simple");
        m_quoted.name("quoted");
        m_entry.name("entry");
        m_value.name("value");
        m_simple.name("simple");
        m_quoted.name("quoted");

        m_tableValue.name("table value");
        m_quotedDataSet.name("quoted data set");
        m_bracedDataSet.name("braced data set");
        m_tableEntry.name("table entry");


        on_error<fail>(m_braced, [&](const auto &it, const auto &what, const auto &params) {
          LOG(error) << "Error occurred when parsing data set at " << pos(it);
        });
        on_error<fail>(m_entry, [&](const auto &it, const auto &what, const auto &params) {
          LOG(error) << "Error occurred when parsing data set at " << pos(it);
        });
        on_error<fail>(m_tableEntry, [&](const auto &it, const auto &what, const auto &params) {
          LOG(error) << "Error occurred when parsing data set at " << pos(it);
        });
        on_error<fail>(m_start, [&](const auto &it, const auto &what, const auto &params) {
          LOG(error) << "Error occurred when parsing data set at " << pos(it);
        });
      }
      
    protected:
      qi::rule<It, DataSet()> m_start;
      qi::real_parser<double, qi::strict_real_policies<double> > m_real;
      qi::rule<It, DataSetValue(), qi::locals<char>> m_quoted;
      qi::rule<It, DataSetValue()> m_braced;
      qi::rule<It, string()> m_key;
      qi::rule<It, string()> m_simple;
      qi::rule<It, DataSetValue()> m_value;
      qi::rule<It, DataSetEntry()> m_entry;
      
      
      qi::rule<It, DataSet(), qi::locals<char>> m_quotedDataSet;
      qi::rule<It, DataSet()> m_bracedDataSet;
      qi::rule<It, DataSet()> m_tableValue;
      qi::rule<It, DataSetEntry()> m_tableEntry;
    };
    
    // Split the data set entries by space delimiters and account for the
    // use of single and double quotes as well as curly braces
    bool DataSet::parse(const std::string &text, bool table)
    {
      using boost::spirit::ascii::space;

      using Iterator = std::string::const_iterator;
      DataSetParser<Iterator> parser(table);
      
      auto s = text.begin();
      auto e = text.end();

      bool r = qi::parse(s, e, parser, *this);
      if (r && s == e)
      {
        // cout << "Success: " << tree << endl;
        return true;
      }
      else
      {
        LOG(error) << "Failed to parse data set: " << text;
        return false;
      }
    }
  }
}
