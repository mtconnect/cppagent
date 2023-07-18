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

#include "data_set.hpp"

#include "mtconnect/utilities.hpp"

// #define BOOST_SPIRIT_DEBUG 1

#include <ostream>

// Parser section
#include <boost/config/warning_disable.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/phoenix.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_action.hpp>
#include <boost/spirit/include/support_istream_iterator.hpp>

namespace qi = boost::spirit::qi;
namespace phx = boost::phoenix;
namespace ascii = boost::spirit::ascii;
namespace spirit = boost::spirit;
namespace l = boost::lambda;

#ifdef BOOST_SPIRIT_DEBUG
namespace std {
  static inline ostream &operator<<(ostream &s, const mtconnect::observation::DataSetEntry &t);

  static inline ostream &operator<<(ostream &s, const mtconnect::observation::DataSetValue &t)
  {
    using namespace mtconnect::observation;
    visit(mtconnect::overloaded {[&s](const monostate &) { s << "NULL"; },
                                 [&s](const std::string &st) { s << "string(" << st << ")"; },
                                 [&s](const int64_t &i) { s << "int(" << i << ")"; },
                                 [&s](const double &d) { s << "double(" << d << ")"; },
                                 [&s](const DataSet &arg) {
                                   s << "{";
                                   for (const auto &v : arg)
                                   {
                                     s << v << ", ";
                                   }
                                   s << "}";
                                 }},
          t);
    return s;
  }

  static inline ostream &operator<<(ostream &s, const mtconnect::observation::DataSetEntry &t)
  {
    s << t.m_key << "=" << t.m_value << (t.m_removed ? ":removed" : "");
    return s;
  }

}  // namespace std
#endif

using namespace std;

namespace mtconnect::entity {
  /// @brief Functions called when parsing data sets
  namespace DataSetParserActions {
    inline static void add_entry_f(DataSet &ds, const DataSetEntry &entry) { ds.emplace(entry); }
    inline static void make_entry_f(DataSetEntry &entry, const string &key,
                                    const boost::optional<DataSetValue> &v)
    {
      entry.m_key = key;
      if (v && !holds_alternative<monostate>(*v))
        entry.m_value = *v;
      else
        entry.m_removed = true;
    }
    inline static void make_entry_f(DataSetEntry &entry, const string &key,
                                    const boost::optional<DataSet> &v)
    {
      entry.m_key = key;
      if (v)
        entry.m_value = *v;
      else
        entry.m_removed = true;
    }
  };  // namespace DataSetParserActions
  BOOST_PHOENIX_ADAPT_FUNCTION(void, add_entry, DataSetParserActions::add_entry_f, 2);
  BOOST_PHOENIX_ADAPT_FUNCTION(void, make_entry, DataSetParserActions::make_entry_f, 3);

  /// @brief Parser to turn adapter text in `key=value ...` or `key={col1=value ...}` syntax into a
  /// data set.
  /// @tparam It the type of the iterator used by the spirit grammar
  template <typename It>
  class DataSetParser : public qi::grammar<It, DataSet()>
  {
  protected:
    template <typename P, typename O, typename R>
    void logError(P &params, O &obj, R &result)
    {
      using namespace boost::fusion;

      auto &start = at_c<0>(params);
      auto &end = at_c<1>(params);
      // auto &what = at_c<2>(params);
      auto &expected = at_c<3>(params);

      std::string text(start, end);

      LOG(error) << "Error when parsing DataSet, expecting '" << expected << "' when parsing '"
                 << text << '\'';
    }

  public:
    /// @brief Create a parser
    /// @param[in] table `true` if we are parsing tables
    DataSetParser(bool table) : DataSetParser::base_type(m_start)
    {
      using namespace qi;
      using phx::construct;
      using phx::val;
      using qi::_1;
      using qi::_2;
      using qi::as_string;
      using qi::double_;
      using qi::lexeme;
      using qi::on_error;
      using spirit::ascii::char_;

      // Data set parser
      m_key %= lexeme[+(char_ - (space | char_("=|{}'\"")))];
      m_value %= (m_number | m_quoted | m_braced | m_simple);

      m_number %= (m_real | m_long) >> &(space | char_("}'\"") | eoi);

      m_simple %= lexeme[+(char_ - (char_("\"'{}") | space))];

      m_quoted %= (omit[char_("\"'")[_a = _1]] >>
                   as_string[*((lit('\\') >> (char_(_a))) | char_ - lit(_a))]) > lit(_a);

      m_braced %=
          (lit('{') >> as_string[*((lit('\\') >> char_('}')) | char_ - lit('}'))]) > lit('}');

      m_entry = (m_key >> -("=" >> -m_value))[make_entry(_val, _1, _2)];

      // Table support with quoted and braced content
      m_quotedDataSet =
          (char_("\"'")[_a = _1] >> *space >> *(m_entry[add_entry(_val, _1)] >> *space)) > lit(_a);

      m_bracedDataSet =
          (lit('{') >> *space >> *(m_entry[add_entry(_val, _1)] >> *space)) > lit('}');

      m_tableValue %= (m_quotedDataSet | m_bracedDataSet);
      m_tableEntry = (m_key >> -("=" > &(space | lit('{') | '\'' | '"') >>
                                 -m_tableValue))[make_entry(_val, _1, _2)];

      if (table)
      {
        m_start = *space >> *(m_tableEntry[add_entry(_val, _1)] >> *space) >> eoi;
      }
      else
      {
        m_start = *space >> *(m_entry[add_entry(_val, _1)] >> *space) >> eoi;
      }

      BOOST_SPIRIT_DEBUG_NODES((
          m_start)(m_quoted)(m_braced)(m_key)(m_value)(m_entry)(m_simple)(m_quoted)(m_tableValue)(m_quotedDataSet)(m_bracedDataSet)(m_tableEntry));

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

      using namespace boost::fusion;

      on_error<fail>(m_entry,
                     [&](auto &params, auto &obj, auto &result) { logError(params, obj, result); });
      on_error<fail>(m_tableEntry,
                     [&](auto &params, auto &obj, auto &result) { logError(params, obj, result); });
      on_error<fail>(m_start,
                     [&](auto &params, auto &obj, auto &result) { logError(params, obj, result); });
    }

  protected:
    qi::rule<It, DataSet()> m_start;
    qi::int_parser<int64_t, 10> m_long;
    qi::real_parser<double, qi::strict_real_policies<double>> m_real;
    qi::rule<It, DataSetValue()> m_number;
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
}  // namespace mtconnect::entity
