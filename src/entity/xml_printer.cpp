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

#include "xml_printer.hpp"
#include "xml_printer_helper.hpp"
#include <libxml/xmlwriter.h>
#include <dlib/logger.h>

using namespace std;

namespace mtconnect
{
  namespace entity 
  {
    static dlib::logger g_logger("entity.xml.printer");
    
    const char *toString(const Value &value, Value &target)
    {
      const char *s{nullptr};
      if (!holds_alternative<string>(value))
      {
        target = value;
        ConvertValueToType(target, STRING);
        s = get<string>(target).c_str();
      }
      else
      {
        s = get<string>(value).c_str();
      }
      
      return s;
    }
          
    using Property = pair<string,Value>;
    void printProperty(xmlTextWriterPtr writer, const Property &p)
    {
      Value v;
      const char *s = toString(p.second, v);
      
      if (p.first == "value")
      {
        // The value is the content for a simple element
        THROW_IF_XML2_ERROR(xmlTextWriterWriteString(writer, BAD_CAST s));
      }
      else
      {
        AutoElement element(writer, p.first.c_str());
        printProperty(writer, { "value", p.second });
      }
    }
    
    void XmlPrinter::print(xmlTextWriterPtr writer, const EntityPtr entity)
    {
      AutoElement element(writer, entity->getName());

      list<Property> attributes;
      list<Property> elements;
      auto &properties = entity->getProperties();
      
      partition_copy(properties.begin(), properties.end(),
                     back_inserter(attributes), back_inserter(elements),
                     [](auto &prop) {
        auto &key = prop.first;
        return (key != "value" && key != "list" && islower(key[0]));
      });
      
      for (auto &a : attributes)
      {
        Value t;
        const char *s = toString(a.second, t);
        THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                    BAD_CAST a.first.c_str(),
                                    BAD_CAST s));

      }
      
      for (auto &e : elements)
      {
        if (holds_alternative<EntityPtr>(e.second))
        {
          print(writer, get<EntityPtr>(e.second));
        }
        else if (holds_alternative<EntityList>(e.second))
        {
          auto &list = get<EntityList>(e.second);
          for (auto &e : list)
            print(writer, e);
        }
        else
        {
          printProperty(writer, e);
        }
      }
    }
  }
}
