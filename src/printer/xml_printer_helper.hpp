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

#pragma once

#include <libxml/xmlwriter.h>

#include "printer/xml_helper.hpp"

namespace mtconnect::printer {
  class XmlWriter
  {
  public:
    XmlWriter(bool pretty) : m_writer(nullptr), m_buf(nullptr)
    {
      THROW_IF_XML2_NULL(m_buf = xmlBufferCreate());
      THROW_IF_XML2_NULL(m_writer = xmlNewTextWriterMemory(m_buf, 0));
      if (pretty)
      {
        THROW_IF_XML2_ERROR(xmlTextWriterSetIndent(m_writer, 1));
        THROW_IF_XML2_ERROR(xmlTextWriterSetIndentString(m_writer, BAD_CAST "  "));
      }
    }

    ~XmlWriter()
    {
      if (m_writer != nullptr)
      {
        xmlFreeTextWriter(m_writer);
        m_writer = nullptr;
      }
      if (m_buf != nullptr)
      {
        xmlBufferFree(m_buf);
        m_buf = nullptr;
      }
    }

    operator xmlTextWriterPtr() { return m_writer; }

    std::string getContent()
    {
      if (m_writer != nullptr)
      {
        THROW_IF_XML2_ERROR(xmlTextWriterEndDocument(m_writer));
        xmlFreeTextWriter(m_writer);
        m_writer = nullptr;
      }
      return std::string((char *)m_buf->content, m_buf->use);
    }

  protected:
    xmlTextWriterPtr m_writer;
    xmlBufferPtr m_buf;
  };

  static inline void openElement(xmlTextWriterPtr writer, const char *name)
  {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST name));
  }

  static inline void closeElement(xmlTextWriterPtr writer)
  {
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
  }

  class AutoElement
  {
  public:
    AutoElement(xmlTextWriterPtr writer) : m_writer(writer) {}
    AutoElement(xmlTextWriterPtr writer, const char *name, std::string key = "")
      : m_writer(writer), m_name(name), m_key(std::move(key))
    {
      openElement(writer, name);
    }
    AutoElement(xmlTextWriterPtr writer, const std::string &name, std::string key = "")
      : m_writer(writer), m_name(name), m_key(std::move(key))
    {
      openElement(writer, name.c_str());
    }
    bool reset(const std::string &name, const std::string &key = "")
    {
      if (name != m_name || m_key != key)
      {
        if (!m_name.empty())
          closeElement(m_writer);
        if (!name.empty())
          openElement(m_writer, name.c_str());
        m_name = name;
        m_key = key;
        return true;
      }
      else
      {
        return false;
      }
    }
    ~AutoElement()
    {
      if (!m_name.empty())
        xmlTextWriterEndElement(m_writer);
    }

    const std::string &key() const { return m_key; }
    const std::string &name() const { return m_name; }

  protected:
    xmlTextWriterPtr m_writer;
    std::string m_name;
    std::string m_key;
  };
}  // namespace mtconnect::printer
