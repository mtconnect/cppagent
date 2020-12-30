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

#include <unordered_map>

using namespace std;

namespace mtconnect
{
  namespace entity 
  {
    static dlib::logger g_logger("entity.xml.printer");
    
    const char *toCharPtr(const Value &value, string &temp)
    {
      const string *s;
      if (!holds_alternative<string>(value))
      {
        Value conv = value;
        ConvertValueToType(conv, STRING);
        temp = get<string>(conv);
        s = &temp;
      }
      else
      {
        s = &get<string>(value);
      }
      
      return s->c_str();
    }
          
    using Property = pair<string,Value>;
    void printProperty(xmlTextWriterPtr writer, const Property &p)
    {
      string t;
      const char *s = toCharPtr(p.second, t);
      if (p.first == "VALUE")
      {
        // The value is the content for a simple element
        THROW_IF_XML2_ERROR(xmlTextWriterWriteString(writer, BAD_CAST s));
      }
      else if (p.first == "RAW")
      {
        THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, BAD_CAST s));
      }
      else
      {
        AutoElement element(writer, p.first);
        THROW_IF_XML2_ERROR(xmlTextWriterWriteString(writer, BAD_CAST s));
      }
    }
    
    void XmlPrinter::print(xmlTextWriterPtr writer, const EntityPtr entity)
    {
      AutoElement element(writer, entity->getName());

      list<Property> attributes;
      list<Property> elements;
      auto &properties = entity->getProperties();
      const auto order = entity->getOrder();
      
      // Partition the properties
      for (const auto &prop : properties)
      {
        auto &key = prop.first;
        if (key != "VALUE" && key != "LIST" && islower(key[0]))
          attributes.emplace_back(prop);
        else
          elements.emplace_back(prop);
      }
      
      // Reorder elements if they need to be specially ordered.
      if (order)
      {
        // Sort all ordered elements first based on the order in the
        // ordering list
        elements.sort([&order](auto &e1, auto &e2) -> bool {
          auto it1 = order->find(e1.first);
          if (it1 == order->end())
            return false;
          auto it2 = order->find(e2.first);
          if (it2 == order->end())
            return true;
          return it1->second < it2->second;
        });
      }
      
      for (auto &a : attributes)
      {
        string t;
        THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer,
                                    BAD_CAST a.first.c_str(),
                                    BAD_CAST toCharPtr(a.second, t)));

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
