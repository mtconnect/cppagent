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

// Parser section
#include <boost/config/warning_disable.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/phoenix_function.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
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
  namespace observation {
    template <typename It, typename Skipper = ascii::space_type>
    class DataSetParser : public qi::grammar<It, pt::ptree(), Skipper>
    {
    public:
      DataSetParser() : DataSetParser::base_type(m_start)
      {
        using namespace qi;
        using phx::construct;
        using phx::val;
        using qi::_1;
        using qi::_2;
        using qi::lexeme;
        using qi::on_error;
        using spirit::ascii::char_;
        
        m_key %= lexeme[+(char_ - (space | char_("=|")))];
        m_value %= (m_simple | m_quoted | m_braced);
        m_simple %= +(char_ - space);
        m_quoted %= omit[char_("\"'")[_a = _1]] > no_skip[*((char_ - char_(_a)) | (char_('\\') >> char_))] > lit(_a);
        
        m_start.name("top");
      }
      
    protected:
      qi::rule<It, DataSet(), Skipper> m_start;
      qi::rule<It, string(), Skipper> m_key;
      qi::rule<It, DataSetValue(), Skipper> m_value;
      qi::rule<It, DataSetValue(), Skipper> m_simple;
      qi::rule<It, DataSetValue(), Skipper> m_quoted;
      qi::rule<It, DataSetValue(), Skipper> m_braced;
      qi::rule<It, pair<string, DataSetValue>(), Skipper> m_entry;
    };
  }
}
