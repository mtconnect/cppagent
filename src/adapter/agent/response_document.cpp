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
  
  inline bool eachElement(xmlNodePtr node, std::function<bool(xmlNodePtr)> cb)
  {
    for (auto child = node->children; child != nullptr; child = child->next)
    {
      if (child->type == XML_ELEMENT_NODE)
      {
        if (!cb(child))
          return true;
      }
    }
    
    return false;
  }
  
  inline bool eachElement(xmlNodePtr node, const char *name, std::function<bool(xmlNodePtr)> cb)
  {
    for (auto child = node->children; child != nullptr; child = child->next)
    {
      if (child->type == XML_ELEMENT_NODE && xmlStrcmp(BAD_CAST name, child->name) == 0)
      {
        if (!cb(child))
          return true;
      }
    }
    
    return false;
  }

  inline bool eachAttribute(xmlNodePtr node, std::function<bool(xmlAttrPtr)> cb)
  {
    for (auto attr = node->properties; attr != nullptr; attr = attr->next)
    {
      if (attr->type == XML_ATTRIBUTE_NODE)
      {
        if (!cb(attr))
          return true;
      }
    }
    
    return false;
  }
  
  inline string attributeValue(xmlNodePtr node, const char *name)
  {
    string res;
    if (!eachAttribute(node, [&name, &res](xmlAttrPtr attr) {
      if (xmlStrcmp(BAD_CAST name, attr->name))
      {
        res = (const char *)(attr->children->content);
        return false;
      }
      return true;
    }))
    {
      LOG(error) << "Cannot find attribute " << name << " in resonse doc";
    }
    
    return res;
  }
  
  inline static xmlNodePtr findChild(xmlNodePtr node, const char *name)
  {
    xmlNodePtr child;
    if (!eachElement(node, name, [&child](xmlNodePtr node) {
      child = node;
      return false;
    }))
    {
      LOG(error) << "Cannot find " << name << " in resonse doc";
    }
    
    return nullptr;
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
  
  inline DataSetValue type(const string &s)
  {
    using namespace boost;

    for (const char c : s)
    {
      if (!isdigit(c) || c != '.')
      {
        return s;
      }
      else if (c == '.')
      {
        return lexical_cast<double>(s);
      }
    }
    
    return lexical_cast<int64_t>(s);
  }
  
  inline void dataSet(xmlNodePtr node, bool table, DataSet &ds)
  {
    eachElement(node, "Entry", [table, &ds](xmlNodePtr n) {
      DataSetEntry entry;
      entry.m_key = attributeValue(n, "key");
      entry.m_removed = attributeValue(n, "removed") == "true";
      
      if (table)
      {
        entry.m_value.emplace<DataSet>();
        DataSet &row = get<DataSet>(entry.m_value);
        
        eachElement(n, "Cell", [&row](xmlNodePtr c) {
          dataSet(c, false, row);
          return true;
        });
      }
      else
      {
        entry.m_value = type(text(n));
      }
      
      ds.insert(entry);
      return true;
    });
  }
      
  inline static bool parseDataItems(ResponseDocument &out, xmlNodePtr node)
  {
    auto streams = findChild(node, "Streams");
    if (streams == nullptr)
      return false;
    
    eachElement(streams, "DeviceStream", [&out](xmlNodePtr dev) {
      auto uuid = attributeValue(dev, "uuid");
      eachElement(dev, "ComponentStream", [&out, &uuid](xmlNodePtr comp) {
        eachElement(comp, [&out, &uuid](xmlNodePtr org){
          eachElement(org, [&out, &uuid](xmlNodePtr o){
            EntityPtr entity = make_shared<Entity>("ObservationProperties", Properties {{"deviceUuid", uuid}});
            
            eachAttribute(o, [entity](xmlAttrPtr attr){
              if (xmlStrcmp(BAD_CAST "sequence", attr->name) != 0)
              {
                string s((const char *)attr->children->content);
                entity->setProperty((const char*) attr->name, s);
              }
              
              return true;
            });
              
            // Check for table or data set
            string name((const char*) o->name);
            auto val = text(o);
            if (val == "UNAVAILABLE")
            {
              entity->setValue(val);
            }
            else if (ends_with(name, "Table"))
            {
              entity->setValue(DataSet());
              auto &ds = get<DataSet>(entity->getValue());
              dataSet(o, true, ds);
            }
            else if (ends_with(name, "DataSet"))
            {
              entity->setValue(DataSet());
              auto &ds = get<DataSet>(entity->getValue());
              dataSet(o, false, ds);
            }
            else
            {
              entity->setValue(val);
            }
            
            out.m_entities.emplace_back(entity);
            return true;
          });
          return true;
        });
        
        return true;
      });
      
      return true;
    });
        
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

