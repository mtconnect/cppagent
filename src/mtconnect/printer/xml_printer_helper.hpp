//
// Copyright Copyright 2009-2024, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <optional>
#include <string>

#include "mtconnect/config.hpp"
#include "mtconnect/printer/xml_helper.hpp"

namespace mtconnect::printer {
  // The write* helpers below are intentionally header-inline (no AGENT_LIB_API):
  // each translation unit that uses them gets its own copy, which avoids the
  // ODR/MSVC-export complications of mixing `inline` with dllexport.

  /// @brief Scan for XML-1.0-illegal C0 control bytes and, if any are found,
  /// return a sanitized copy with each replaced by a space.
  ///
  /// XML 1.0 permits only 0x09 (TAB), 0x0A (LF), and 0x0D (CR) below 0x20;
  /// any other byte below 0x20 is rejected by strict parsers (e.g. Python's
  /// lxml: "PCDATA invalid Char value N"). libxml2's writer escapes &<>"'
  /// but does not validate against the XML Char production, so the caller
  /// must do it.
  ///
  /// Replacement (rather than deletion) preserves token boundaries, e.g.
  /// `"1.25 \x16 3"` becomes `"1.25   3"` — not `"1.253"`. Multi-byte UTF-8
  /// continuation bytes are >=0x80 and pass through untouched.
  ///
  /// Returns std::nullopt when the input is already clean — the caller can
  /// then use the original buffer and skip the allocation.
  ///
  /// Note: iteration stops at the first NUL byte because callers pass NUL-
  /// terminated C strings to libxml2 via xmlTextWriterWriteString, which
  /// itself truncates at NUL. Embedded NULs would be lost regardless.
  inline std::optional<std::string> sanitizeXmlChars(const char *s)
  {
    auto isInvalid = [](unsigned char c) {
      return c < 0x20 && c != 0x09 && c != 0x0A && c != 0x0D;
    };

    if (s == nullptr)
      return std::nullopt;

    const char *p = s;
    while (*p != '\0' && !isInvalid(static_cast<unsigned char>(*p)))
      ++p;
    if (*p == '\0')
      return std::nullopt;

    std::string out;
    out.append(s, p);
    for (; *p != '\0'; ++p)
    {
      const unsigned char c = static_cast<unsigned char>(*p);
      out.push_back(isInvalid(c) ? ' ' : *p);
    }
    return out;
  }

  /// @brief Write element text, replacing XML-illegal control bytes first.
  inline int writeXmlString(xmlTextWriterPtr writer, const char *s)
  {
    if (auto clean = sanitizeXmlChars(s))
      return xmlTextWriterWriteString(writer, BAD_CAST clean->c_str());
    return xmlTextWriterWriteString(writer, BAD_CAST s);
  }

  /// @brief Write an attribute, replacing XML-illegal control bytes first.
  inline int writeXmlAttribute(xmlTextWriterPtr writer, const char *name, const char *value)
  {
    if (auto clean = sanitizeXmlChars(value))
      return xmlTextWriterWriteAttribute(writer, BAD_CAST name, BAD_CAST clean->c_str());
    return xmlTextWriterWriteAttribute(writer, BAD_CAST name, BAD_CAST value);
  }

  /// @brief Write a raw (already entity-encoded) XML fragment, replacing
  /// XML-illegal control bytes first. C0 controls remain illegal regardless
  /// of how entities are escaped, so RAW outputs must be sanitized too.
  inline int writeXmlRaw(xmlTextWriterPtr writer, const char *s)
  {
    if (auto clean = sanitizeXmlChars(s))
      return xmlTextWriterWriteRaw(writer, BAD_CAST clean->c_str());
    return xmlTextWriterWriteRaw(writer, BAD_CAST s);
  }

  /// @brief Helper class for XML document generation. Wraps some common libxml2 functions
  class AGENT_LIB_API XmlWriter
  {
  public:
    /// @brief Construct an XmlWriter creating setting up the buffer for writing.
    /// @param pretty `true` if output is formatted with indentation
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

    /// @brief cast this object as a xmlTextWriterPtr
    /// @return the xmlTextWriterPtr
    operator xmlTextWriterPtr() { return m_writer; }

    /// @brief Get the content of the buffer as a string. Free the writer if it is allocated.
    /// @return content as a string
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

  /// @brief Wrapper to create an XML open element
  /// @param writer the writer
  /// @param name the name of the element
  static inline void openElement(xmlTextWriterPtr writer, const char *name)
  {
    THROW_IF_XML2_ERROR(xmlTextWriterStartElement(writer, BAD_CAST name));
  }

  /// @brief Close the last open element
  /// @param writer the writer
  static inline void closeElement(xmlTextWriterPtr writer)
  {
    THROW_IF_XML2_ERROR(xmlTextWriterEndElement(writer));
  }

  /// @brief Helper class to automatically close an element when the object goes out of scope
  class AGENT_LIB_API AutoElement
  {
  public:
    /// @brief Constructor where the element name will be filled in later
    /// @param writer the writer
    AutoElement(xmlTextWriterPtr writer) : m_writer(writer) {}
    /// @brief Constor where the element is opened
    /// @param writer the writer
    /// @param name name of the element
    /// @param key optional key if the for closing an element and reopening another element
    AutoElement(xmlTextWriterPtr writer, const char *name, std::string key = "")
      : m_writer(writer), m_name(name), m_key(std::move(key))
    {
      openElement(writer, name);
    }
    /// @brief Constor where the element is opened
    /// @param writer the writer
    /// @param name name of the element
    /// @param key optional key if the for closing an element and reopening another element
    AutoElement(xmlTextWriterPtr writer, const std::string &name, std::string key = "")
      : m_writer(writer), m_name(name), m_key(std::move(key))
    {
      openElement(writer, name.c_str());
    }
    /// @brief close the currently open element if the name or the key don't match
    /// @param name of the element
    /// @param key optional key if the for closing an element and reopening another element
    /// @return `true` if the element was closed and reopened
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
    /// @brief Destructor closes the element if it is open
    ~AutoElement()
    {
      if (!m_name.empty())
        xmlTextWriterEndElement(m_writer);
    }

    /// @brief get the key
    /// @return the key
    const std::string &key() const { return m_key; }
    /// @brief return the name
    /// @return the name
    const std::string &name() const { return m_name; }

  protected:
    xmlTextWriterPtr m_writer;
    std::string m_name;
    std::string m_key;
  };
}  // namespace mtconnect::printer
