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

#pragma once

#include "cached_file.hpp"

#include <filesystem>
#include <list>
#include <optional>
#include <string>

namespace mtconnect
{
  namespace http_server
  {
    using XmlNamespace = std::pair<std::string, std::string>;
    using XmlNamespaceList = std::list<XmlNamespace>;
    class FileCache
    {
     public:
      FileCache();

      XmlNamespaceList registerFiles(const std::string &uri, const std::string &path,
                                     const std::string &version);
      XmlNamespaceList registerDirectory(const std::string &uri, const std::string &path,
                                         const std::string &version);
      std::optional<XmlNamespace> registerFile(const std::string &uri, const std::string &path,
                                               const std::string &version);
      CachedFilePtr getFile(const std::string &name);
      bool hasFile(const std::string &name) const
      {
        return (m_fileMap.find(name) != m_fileMap.end());
      }
      void addMimeType(const std::string &ext, const std::string &type)
      {
        std::string s(ext);
        if (s[0] != '.')
          s.insert(0, ".");
        m_mimeTypes[s] = type;
      }

     protected:
      std::map<std::string, std::filesystem::path> m_fileMap;
      std::map<std::string, CachedFilePtr> m_fileCache;
      std::map<std::string, std::string> m_mimeTypes;
    };
  }  // namespace http_server
}  // namespace mtconnect
