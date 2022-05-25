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

#include <filesystem>
#include <list>
#include <optional>
#include <string>

#include "cached_file.hpp"

namespace boost {
  namespace asio {
    class io_context;
  }
}  // namespace boost

namespace mtconnect::sink::rest_sink {
  using XmlNamespace = std::pair<std::string, std::string>;
  using XmlNamespaceList = std::list<XmlNamespace>;
  class FileCache
  {
  public:
    using Directory = std::pair<std::string, std::pair<std::filesystem::path, std::string>>;

    FileCache(size_t max = 20 * 1024);

    XmlNamespaceList registerFiles(const std::string &uri, const std::string &path,
                                   const std::string &version);
    XmlNamespaceList registerDirectory(const std::string &uri, const std::string &path,
                                       const std::string &version);
    std::optional<XmlNamespace> registerFile(const std::string &uri, const std::string &pathName,
                                             const std::string &version)
    {
      std::filesystem::path path(pathName);
      return registerFile(uri, path, version);
    }
    std::optional<XmlNamespace> registerFile(const std::string &uri,
                                             const std::filesystem::path &path,
                                             const std::string &version);
    CachedFilePtr getFile(const std::string &name,
                          const std::optional<std::string> acceptEncoding = std::nullopt,
                          boost::asio::io_context *context = nullptr);
    bool hasFile(const std::string &name) const
    {
      return (m_fileCache.count(name) > 0) || (m_fileMap.count(name) > 0);
    }
    void addMimeType(const std::string &ext, const std::string &type)
    {
      std::string s(ext);
      if (s[0] != '.')
        s.insert(0, ".");
      m_mimeTypes[s] = type;
    }
    void addDirectory(const std::string &uri, const std::string &path, const std::string &index);

    void setMaxCachedFileSize(size_t s) { m_maxCachedFileSize = s; }
    auto getMaxCachedFileSize() const { return m_maxCachedFileSize; }

    void setMinCompressedFileSize(size_t s) { m_minCompressedFileSize = s; }
    auto getMinCompressedFileSize() const { return m_minCompressedFileSize; }

    // For testing
    void clear() { m_fileCache.clear(); }

  protected:
    CachedFilePtr findFileInDirectories(const std::string &name);
    const std::string &getMimeType(std::string ext)
    {
      static std::string octStream("application/octet-stream");
      auto mt = m_mimeTypes.find(ext);
      if (mt != m_mimeTypes.end())
        return mt->second;
      else
        return octStream;
    }

    CachedFilePtr redirect(const std::string &name, const Directory &directory);
    void compressFile(CachedFilePtr file, boost::asio::io_context *context);

  protected:
    std::map<std::string, std::pair<std::filesystem::path, std::string>> m_directories;
    std::map<std::string, std::filesystem::path> m_fileMap;
    std::map<std::string, CachedFilePtr> m_fileCache;
    std::map<std::string, std::string> m_mimeTypes;
    size_t m_maxCachedFileSize;
    size_t m_minCompressedFileSize;
  };
}  // namespace mtconnect::sink::rest_sink
