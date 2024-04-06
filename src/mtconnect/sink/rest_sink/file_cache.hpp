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

#include <filesystem>
#include <list>
#include <optional>
#include <string>

#include "cached_file.hpp"
#include "mtconnect/config.hpp"

namespace boost {
  namespace asio {
    class io_context;
  }
}  // namespace boost

namespace mtconnect::sink::rest_sink {
  using XmlNamespace = std::pair<std::string, std::string>;
  using XmlNamespaceList = std::list<XmlNamespace>;
  /// @brief Class to manage file caching for the REST service
  class AGENT_LIB_API FileCache
  {
  public:
    /// @brief Directory mapping from the server path to the file system
    using Directory = std::pair<std::string, std::pair<std::filesystem::path, std::string>>;

    /// @brief Create a file cache
    /// @param max optional maxumimum size of the cache, defaults to 20k.
    FileCache(size_t max = 20 * 1024);

    /// @brief register files to be served by the agent.
    /// @note Cover method for `registerDirectory()`.
    /// @param uri the uri to the file
    /// @param path the path on the file system
    /// @param version schema version when registering MTConnect files
    /// @return A namespace list associated with the files
    XmlNamespaceList registerFiles(const std::string &uri, const std::filesystem::path &path,
                                   const std::string &version)
    {
      return registerDirectory(uri, path, version);
    }
    /// @brief register all files in a directory to be served by the agent
    /// @param uri the uri to the file
    /// @param path the path on the file system
    /// @param version schema version when registering MTConnect files
    /// @return A namespace list associated with the files
    XmlNamespaceList registerDirectory(const std::string &uri, const std::filesystem::path &path,
                                       const std::string &version);
    /// @brief Register a single file
    /// @param uri the uri for the file
    /// @param pathName the std filesystem path of file
    /// @param version the schema version when registering MTConnect files
    /// @return an optional XmlNamespace if successful
    std::optional<XmlNamespace> registerFile(const std::string &uri,
                                             const std::filesystem::path &path,
                                             const std::string &version);
    /// @brief get a cached file given a filename and optional encoding
    /// @param name the name of the file from the server
    /// @param acceptEncoding optional accepted encodings
    /// @param context optional context to perform async io
    /// @return shared pointer to the cached file
    CachedFilePtr getFile(const std::string &name,
                          const std::optional<std::string> acceptEncoding = std::nullopt,
                          boost::asio::io_context *context = nullptr);
    /// @brief check if the file is cached
    /// @param name the name of the file from the server
    /// @return `true` if the file is cached
    bool hasFile(const std::string &name) const
    {
      return (m_fileCache.count(name) > 0) || (m_fileMap.count(name) > 0);
    }
    /// @brief Register an file name extension with a mime type
    /// @param ext the extension (will insert a leading dot if one is not provided)
    /// @param type the mime type
    void addMimeType(const std::string &ext, const std::string &type)
    {
      std::string s(ext);
      if (s[0] != '.')
        s.insert(0, ".");
      m_mimeTypes[s] = type;
    }

    /// @brief Add a directory mapping to the local files system
    /// @param uri the server uri to the directory
    /// @param path the path on the files system
    /// @param index the default file to return for the directory if one is not given. For example,
    /// `index.html`
    void addDirectory(const std::string &uri, const std::string &path, const std::string &index);

    /// @brief Set the maximum size of the cache
    /// @param s the maximum size
    void setMaxCachedFileSize(size_t s) { m_maxCachedFileSize = s; }
    /// @brief Get the maximum size of the file cache
    /// @return the maxumum size
    auto getMaxCachedFileSize() const { return m_maxCachedFileSize; }

    /// @brief Set the file size where they are returned compressed
    ///
    /// Any file larger than the size will be returned gzipped if the user agent supports
    /// compression.
    /// @param s the mimum size
    void setMinCompressedFileSize(size_t s) { m_minCompressedFileSize = s; }
    /// @brief Get the minimum file size for compression
    /// @return the size
    auto getMinCompressedFileSize() const { return m_minCompressedFileSize; }

    /// @name Only used for testing
    ///@{
    /// @brief clean the file cache
    void clear() { m_fileCache.clear(); }
    ///@}
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

    // Access control to the buffer
    mutable std::recursive_mutex m_cacheLock;
  };
}  // namespace mtconnect::sink::rest_sink
