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

#include "xml_printer.hpp"

#include <unordered_map>

#include <libxml/xmlwriter.h>

#include "logging.hpp"
#include "printer/xml_printer_helper.hpp"

using namespace std;

namespace mtconnect {
  using namespace printer;
  namespace entity {
    inline string stripUndeclaredNamespace(const QName &qname,
                                           const unordered_set<string> &namespaces)
    {
      string name;
      if (qname.hasNs() && namespaces.count(string(qname.getNs())) == 0)
        name = qname.getName();
      else
        name = qname;

      return name;
    }

    static inline void addAttributes(xmlTextWriterPtr writer,
                                     const std::map<string, string> &attributes)
    {
      for (const auto &attr : attributes)
      {
        if (!attr.second.empty())
        {
          THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST attr.first.c_str(),
                                                          BAD_CAST attr.second.c_str()));
        }
      }
    }

    static void addSimpleElement(xmlTextWriterPtr writer, const string &element, const string &body,
                                 const map<string, string> &attributes = {}, bool raw = false)
    {
      AutoElement ele(writer, element);

      if (!attributes.empty())
        addAttributes(writer, attributes);

      if (!body.empty())
      {
        xmlChar *text = nullptr;
        if (!raw)
          text = xmlEncodeEntitiesReentrant(nullptr, BAD_CAST body.c_str());
        else
          text = BAD_CAST body.c_str();
        THROW_IF_XML2_ERROR(xmlTextWriterWriteRaw(writer, text));
        if (!raw)
          xmlFree(text);
      }
    }

    void printDataSet(xmlTextWriterPtr writer, const std::string &name, const DataSet &set)
    {
      AutoElement ele(writer);
      if (name != "VALUE")
      {
        ele.reset(name);
      }

      for (auto &e : set)
      {
        map<string, string> attrs = {{"key", e.m_key}};
        if (e.m_removed)
        {
          attrs["removed"] = "true";
        }
        visit(overloaded {[&writer, &attrs](const monostate &st) {
                            addSimpleElement(writer, "Entry", "", attrs);
                          },
                          [&writer, &attrs](const string &st) {
                            addSimpleElement(writer, "Entry", st, attrs);
                          },
                          [&writer, &attrs](const int64_t &i) {
                            addSimpleElement(writer, "Entry", to_string(i), attrs);
                          },
                          [&writer, &attrs](const double &d) {
                            addSimpleElement(writer, "Entry", format(d), attrs);
                          },
                          [&writer, &attrs](const DataSet &row) {
                            // Table
                            AutoElement ele(writer, "Entry");
                            addAttributes(writer, attrs);
                            for (auto &c : row)
                            {
                              map<string, string> attrs = {{"key", c.m_key}};
                              visit(overloaded {
                                        [&writer, &attrs](const string &s) {
                                          addSimpleElement(writer, "Cell", s, attrs);
                                        },
                                        [&writer, &attrs](const int64_t &i) {
                                          addSimpleElement(writer, "Cell", to_string(i), attrs);
                                        },
                                        [&writer, &attrs](const double &d) {
                                          addSimpleElement(writer, "Cell", format(d), attrs);
                                        },
                                        [](auto &a) {
                                          LOG(error) << "Invalid type for DataSetVariant cell";
                                        }},
                                    c.m_value);
                            }
                          }},
              e.m_value);
      }
    }

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

    void printProperty(xmlTextWriterPtr writer, const Property &p,
                       const unordered_set<string> &namespaces)
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
        QName name(p.first);
        string qname = stripUndeclaredNamespace(name, namespaces);
        AutoElement element(writer, qname);
        THROW_IF_XML2_ERROR(xmlTextWriterWriteString(writer, BAD_CAST s));
      }
    }

    void XmlPrinter::print(xmlTextWriterPtr writer, const EntityPtr entity,
                           const std::unordered_set<std::string> &namespaces)
    {
      NAMED_SCOPE("entity.xml_printer");
      const auto &properties = entity->getProperties();
      const auto order = entity->getOrder();
      const auto *localNamespaces = &namespaces;

      // If this element has a namespace and there is a xmlns delcaration, create a new set of
      // namespaces with this one added
      std::unique_ptr<std::unordered_set<std::string>> entityNamespaces;
      if (entity->getName().hasNs())
      {
        string ns(entity->getName().getNs());
        if (namespaces.count(ns) == 0)
        {
          auto attr = properties.find(string("xmlns:") + ns);
          if (namespaces.count(ns) == 0 && attr != properties.end())
          {
            entityNamespaces = make_unique<unordered_set<string>>(namespaces);
            entityNamespaces->emplace(ns);
            localNamespaces = entityNamespaces.get();
          }
        }
      }

      string qname = stripUndeclaredNamespace(entity->getName(), *localNamespaces);
      AutoElement element(writer, qname);

      list<Property> attributes;
      list<Property> elements;

      // Partition the properties
      const auto &attrs = entity->getAttributes();
      for (const auto &prop : properties)
      {
        auto &key = prop.first;
        if (islower(key.getName()[0]) || attrs.count(key) > 0)
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
        QName name(a.first);
        bool isNsDecl = name.hasNs() && name.getNs() == "xmlns";
        if (!isNsDecl || namespaces.count(string(name.getName())) == 0)
        {
          THROW_IF_XML2_ERROR(xmlTextWriterWriteAttribute(writer, BAD_CAST a.first.c_str(),
                                                          BAD_CAST toCharPtr(a.second, t)));
        }
      }

      for (auto &e : elements)
      {
        visit(overloaded {[&writer, localNamespaces, this](const EntityPtr &v) {
                            print(writer, v, *localNamespaces);
                          },
                          [&writer, localNamespaces, this](const EntityList &list) {
                            for (auto &en : list)
                              print(writer, en, *localNamespaces);
                          },
                          [&writer, &e](const DataSet &v) { printDataSet(writer, e.first, v); },
                          [&writer, &e, localNamespaces](const auto &v) {
                            printProperty(writer, e, *localNamespaces);
                          }},
              e.second);
      }
    }
  }  // namespace entity
}  // namespace mtconnect
