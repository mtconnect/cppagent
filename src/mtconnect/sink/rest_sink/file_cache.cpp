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

#include "file_cache.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <future>
#include <thread>

#include "cached_file.hpp"
#include "mtconnect/logging.hpp"

using namespace std;

namespace mtconnect::sink::rest_sink {
  FileCache::FileCache(size_t max)
    : m_mimeTypes({{{".xsl", "text/xsl"},
                    {".xml", "text/xml"},
                    {".json", "application/json"},
                    {".js", "text/javascript"},
                    {".obj", "model/obj"},
                    {".stl", "model/stl"},
                    {".css", "text/css"},
                    {".xsd", "text/xml"},
                    {".jpg", "image/jpeg"},
                    {".jpeg", "image/jpeg"},
                    {".png", "image/png"},
                    {".txt", "text/plain"},
                    {".html", "text/html"},
                    {".ico", "image/x-icon"}}}),
      m_maxCachedFileSize(max)
  {
    NAMED_SCOPE("file_cache");
  }

  namespace fs = std::filesystem;

  // Register a file
  XmlNamespaceList FileCache::registerDirectory(const string &uri, const fs::path &pathName,
                                                const string &version)
  {
    XmlNamespaceList namespaces;

    try
    {
      fs::path path(pathName);

      if (!fs::exists(path))
      {
        LOG(warning) << "The following path " << pathName
                     << " cannot be found, full path: " << path;
      }
      else if (!fs::is_directory(path))
      {
        auto ns = registerFile(uri, path, version);
        if (ns)
          namespaces.emplace_back(*ns);
      }
      else
      {
        fs::path baseUri(uri, fs::path::format::generic_format);

        for (auto &file : fs::directory_iterator(path))
        {
          string name = (file.path().filename()).string();
          fs::path uri = baseUri / name;

          auto ns = registerFile(uri.generic_string(), (file.path()).string(), version);
          if (ns)
            namespaces.emplace_back(*ns);
        }
      }
    }
    catch (fs::filesystem_error e)
    {
      LOG(warning) << "The following path " << pathName << " cannot be accessed: " << e.what();
    }

    return namespaces;
  }

  std::optional<XmlNamespace> FileCache::registerFile(const std::string &uri, const fs::path &path,
                                                      const std::string &version)
  {
    optional<XmlNamespace> ns;

    if (!fs::exists(path))
    {
      LOG(warning) << "The following path " << path
                   << " cannot be found, full path: " << fs::absolute(path);
      return nullopt;
    }
    else if (!fs::is_regular_file(path))
    {
      LOG(warning) << "The following path " << path
                   << " is not a regular file: " << fs::absolute(path);
      return nullopt;
    }

    // Make sure the uri is using /
    string gen;
    gen.resize(uri.size());
    replace_copy(uri.begin(), uri.end(), gen.begin(), '\\', '/');

    m_fileMap.emplace(gen, fs::absolute(path));
    string name = path.filename().string();

    // Check if the file name maps to a standard MTConnect schema file.
    if (!name.find("MTConnect") && name.substr(name.length() - 4u, 4u) == ".xsd" &&
        version == name.substr(name.length() - 7u, 3u))
    {
      string version = name.substr(name.length() - 7u, 3u);

      if (name.substr(9u, 5u) == "Error")
      {
        string urn = "urn:mtconnect.org:MTConnectError:" + version;
        ns = make_optional<XmlNamespace>(urn, uri);
      }
      else if (name.substr(9u, 7u) == "Devices")
      {
        string urn = "urn:mtconnect.org:MTConnectDevices:" + version;
        ns = make_optional<XmlNamespace>(urn, uri);
      }
      else if (name.substr(9u, 6u) == "Assets")
      {
        string urn = "urn:mtconnect.org:MTConnectAssets:" + version;
        ns = make_optional<XmlNamespace>(urn, uri);
      }
      else if (name.substr(9u, 7u) == "Streams")
      {
        string urn = "urn:mtconnect.org:MTConnectStreams:" + version;
        ns = make_optional<XmlNamespace>(urn, uri);
      }
    }

    return ns;
  }

  CachedFilePtr FileCache::redirect(const std::string &name, const Directory &dir)
  {
    static const char *body = R"(<html>
<head><title>301 Moved Permanently</title></head>
<body>
<center><h1>301 Moved Permanently</h1></center>
<hr><center>MTConnect Agent</center>
</body>
</html>
)";

    auto file = make_shared<CachedFile>(body, strlen(body), "text/html"s);
    file->m_redirect = dir.first + "/" + dir.second.second;
    m_fileCache.insert_or_assign(name, file);
    return file;
  }

  void FileCache::compressFile(CachedFilePtr file, boost::asio::io_context *context)
  {
    NAMED_SCOPE("FileCache::compressFile")

    namespace fs = std::filesystem;
    namespace io = boost::iostreams;
    using namespace std::chrono_literals;

    fs::path zipped(file->m_path.string() + ".gz");

    if (!fs::exists(zipped))
    {
      promise<bool> promise;
      auto future = promise.get_future();

      thread work([file, &zipped, &promise, context] {
        try
        {
          NAMED_SCOPE("work");
          LOG(debug) << "gzipping " << file->m_path << " to " << zipped;

          ifstream input(file->m_path, ios_base::in | ios_base::binary);

          io::filtering_ostream output;
          output.push(io::gzip_compressor(io::gzip_params(io::gzip::best_compression)));
          output.push(io::file_sink(zipped.string(), ios_base::out | ios_base::binary));

          io::copy(input, output);

          promise.set_value(true);

          LOG(debug) << "done";
        }
        catch (...)
        {
          LOG(error) << "Error occurred compressing file " << file->m_path;
          promise.set_exception(std::current_exception());
        }

        if (context != nullptr)
        {
          boost::asio::post(*context, [] {});
        }
      });

      if (context)
      {
        while (future.wait_for(1ms) == std::future_status::timeout)
          context->run_one_for(1s);
      }

      try
      {
        if (future.get())
          file->m_pathGz.emplace(zipped);
      }
      catch (std::runtime_error &e)
      {
        LOG(error) << "Error occurred compressing: " << e.what();
      }
      catch (...)
      {
        LOG(error) << "Error occurred gettting future for " << file->m_path;
      }

      work.join();
    }
    else
    {
      if (fs::last_write_time(file->m_path) > fs::last_write_time(zipped))
      {
        fs::remove(zipped);
        compressFile(file, context);
      }
      else if (!file->m_pathGz)
      {
        file->m_pathGz.emplace(zipped);
      }
    }
  }

  CachedFilePtr FileCache::findFileInDirectories(const std::string &name)
  {
    namespace fs = std::filesystem;

    for (const auto &dir : m_directories)
    {
      if (boost::starts_with(name, dir.first))
      {
        auto fileName = boost::erase_first_copy(name, dir.first);
        if (fileName.empty())
        {
          return redirect(name, dir);
        }

        if (fileName[0] == '/')
          fileName.erase(0, 1);

        if (fileName.empty())
        {
          fileName = dir.second.second;
        }

        optional<string> contentEncoding;
        fs::path path {dir.second.first / fileName};

        if (fs::exists(path))
        {
          auto size = fs::file_size(path);
          auto ext = path.extension().string();

          auto file =
              make_shared<CachedFile>(path, getMimeType(ext), size <= m_maxCachedFileSize, size);

          m_fileCache.insert_or_assign(name, file);
          return file;
        }
      }
    }

    LOG(warning) << "Cannot find file: " << name;

    return nullptr;
  }

  CachedFilePtr FileCache::getFile(const std::string &name,
                                   const std::optional<std::string> acceptEncoding,
                                   boost::asio::io_context *context)
  {
    namespace fs = std::filesystem;

    try
    {
      CachedFilePtr file;

      auto cached = m_fileCache.find(name);
      if (cached != m_fileCache.end())
      {
        auto fp = cached->second;
        if (!fp->m_cached || fp->m_redirect)
        {
          file = fp;
        }
        else
        {
          // Cleanup files if they have changed since last cached
          // Also remove any gzipped content as well
          auto lastWrite = std::filesystem::last_write_time(fp->m_path);
          if (lastWrite == fp->m_lastWrite)
            file = fp;
          else if (fp->m_pathGz && fs::exists(*fp->m_pathGz))
            fs::remove(*fp->m_pathGz);
        }
      }

      if (!file)
      {
        auto path = m_fileMap.find(name);
        if (path != m_fileMap.end())
        {
          auto ext = path->second.extension().string();
          auto size = fs::file_size(path->second);
          file = make_shared<CachedFile>(path->second, getMimeType(ext),
                                         size <= m_maxCachedFileSize, size);
          m_fileCache.insert_or_assign(name, file);
        }
        else
        {
          file = findFileInDirectories(name);
        }
      }

      if (file)
      {
        if (acceptEncoding && acceptEncoding->find("gzip") != string::npos &&
            file->m_size >= m_minCompressedFileSize)
        {
          compressFile(file, context);
        }
      }
      else
      {
        LOG(warning) << "Cannot find file: " << name;
      }

      return file;
    }
    catch (fs::filesystem_error e)
    {
      LOG(warning) << "Cannot open file " << name << ": " << e.what();
    }

    return nullptr;
  }

  void FileCache::addDirectory(const std::string &uri, const std::string &pathName,
                               const std::string &index)
  {
    fs::path path(pathName);
    if (fs::exists(path))
    {
      string root(uri);
      if (boost::ends_with(root, "/"))
      {
        boost::erase_last(root, "/");
      }
      m_directories.emplace(root, make_pair(fs::canonical(path), index));
    }
    else
    {
      LOG(warning) << "Cannot find path " << pathName << " for " << uri;
    }
  }
}  // namespace mtconnect::sink::rest_sink
