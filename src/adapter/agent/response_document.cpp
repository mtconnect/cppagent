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

#include "response_document.hpp"
#include "entity/data_set.hpp"

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

using namespace std;

namespace mtconnect::adapter::agent {
  using namespace mtconnect;
  using namespace entity;
  
  inline string attributeValue(xmlNodePtr node, const char *name)
  {
    for (auto attr = node->properties; attr != nullptr; attr = attr->next)
    {
      if (attr->type == XML_ATTRIBUTE_NODE && xmlStrcmp(BAD_CAST name, attr->name) == 0)
      {
        return (const char *)(attr->children->content);
      }
    }
    
    LOG(error) << "Cannot find attribute value for resonse doc";
    
    throw runtime_error("Cannot find attribute value");
    return "";
  }
  
  inline static xmlNodePtr findChild(xmlNodePtr node, const char *name)
  {
    xmlNodePtr child;
    for (child = node->children; child != nullptr; child = child->next)
    {
      if (xmlStrcmp(BAD_CAST name, child->name) == 0)
      {
        return child;
      }
    }
    
    LOG(error) << "Cannot find " << name << " in resonse doc";

    return nullptr;
  }
  
  inline static vector<xmlNodePtr> findChildren(xmlNodePtr node, const char *name)
  {
    vector<xmlNodePtr> children;
    xmlNodePtr child;
    for (child = node->children; child != nullptr; child = child->next)
    {
      if (xmlStrcmp(BAD_CAST name, child->name) == 0)
      {
        children.push_back(child);
      }
    }
    
    return children;
  }

  
  static inline SequenceNumber_t next(xmlNodePtr root)
  {
    auto header = findChild(root, "Header");
    if (header)
    {
        return boost::lexical_cast<SequenceNumber_t>(attributeValue(header, "nextSequence"));
    }
    
    LOG(error) << "Received incorred document: " << (const char*) root->name;

    return 0;
  }
  
  const string text(xmlNodePtr node)
  {
    for (auto n = node->children; n != nullptr; n = n->next)
    {
      if (n->type == XML_TEXT_NODE)
      {
        string s((const char *)n->content);
        trim(s);
        return s;
      }
    }
    
    return "";
  }
  
  enum DSType {
    FLOAT,
    INT,
    STRING
  };
  
  inline DSType type(const string &s)
  {
    DSType ret = INT;
    for (const char c : s)
    {
      if (!isdigit(c) || c != '.')
        return STRING;
      else if (c == '.')
        ret = FLOAT;
    }
    
    return ret;
  }
  
  inline void dataSet(xmlNodePtr node, bool table, DataSet &ds)
  {
    using namespace boost;

    for (auto n = node->children; n != nullptr; n = n->next)
    {
      if (n->type == XML_ELEMENT_NODE && xmlStrcmp(BAD_CAST "Entry", n->name) == 0)
      {
        DataSetEntry entry;
        entry.m_key = attributeValue(n, "key");
        entry.m_removed = attributeValue(n, "removed") == "true";
        if (table)
        {
          entry.m_value.emplace<DataSet>();
          DataSet &row = get<DataSet>(entry.m_value);
          for (auto c = n->children; c != nullptr; c = n->next)
          {
            if (c->type == XML_ELEMENT_NODE && xmlStrcmp(BAD_CAST "Cell", c->name) == 0)
            {
              dataSet(c, false, row);
            }
          }
        }
        else
        {
          auto value = text(n);
          switch (type(value))
          {
            case STRING:
              entry.m_value.emplace<string>(value);
              break;
              
            case FLOAT:
              entry.m_value.emplace<double>(lexical_cast<double>(value));
              break;
              
            case INT:
              entry.m_value.emplace<int64_t>(lexical_cast<int64_t>(value));
              break;
          }
        }
      }
    }
  }
    
  inline static bool parseDataItems(ResponseDocument &out, xmlNodePtr node)
  {
    auto streams = findChild(node, "Streams");
    if (streams == nullptr)
      return false;
    
    auto devices = findChildren(streams, "DeviceStream");
    for (auto &dev : devices)
    {
      auto uuid = attributeValue(dev, "uuid");
      auto components = findChildren(dev, "ComponentStream");
      for (auto comp : components)
      {
        for (auto n = comp->children; n != nullptr; n = n->next)
        {
          if (n->type != XML_ELEMENT_NODE)
            continue;
          for (auto o = n->children; o != nullptr; o = o->next)
          {
            out.m_properties.emplace_back(Properties());
            auto props = out.m_properties.rbegin();

            props->insert({"deviceUuid"s, uuid});
            props->insert({"timestamp"s, attributeValue(o, "timestamp")});
            props->insert({"dataItemId"s, attributeValue(o, "dataItemId")});

            // Check for table or data set
            string name((const char*) o->name);
            auto val = text(o);
            if (val == "UNAVAILABLE")
            {
              props->insert({"VALUE"s, val});
            }
            else if (ends_with(name, "Table"))
            {
              (*props)["VALUE"] = DataSet();
              auto dsi = props->find("VALUE");
              auto &ds = get<DataSet>(dsi->second);
              dataSet(o, true, ds);
            }
            else if (ends_with(name, "DataSet"))
            {
              (*props)["VALUE"] = DataSet();
              auto dsi = props->find("VALUE");
              auto &ds = get<DataSet>(dsi->second);
              dataSet(o, false, ds);
            }
            else
            {
              props->insert({"VALUE"s, val});
            }            
          }
        }
      }
    }
    
    return true;
  }
  
  bool ResponseDocument::parse(const std::string_view &content,
                               ResponseDocument &out)
  {
    //xmlInitParser();
    //xmlXPathInit();
    unique_ptr<xmlDoc, function<void(xmlDocPtr)>> doc(
        xmlReadMemory(content.data(), content.length(), "incoming.xml", nullptr,
                      XML_PARSE_NOBLANKS),
        [](xmlDocPtr d) { xmlFreeDoc(d); });
    xmlNodePtr root = xmlDocGetRootElement(doc.get());
    if (root != nullptr)
    {
      if (xmlStrcmp(BAD_CAST "MTConnectStreams", root->name) != 0)
      {
        return false;
      }
      
      // Get the header
      out.m_next = next(root);
      if (out.m_next == 0)
      {
        return false;
      }
      
      return parseDataItems(out, root);
    }
    else
    {
      return false;
    }
  }
}

