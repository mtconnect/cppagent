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

#include "file_cache.hpp"
#include "cached_file.hpp"

#include <dlib/logger.h>

using namespace std;

namespace mtconnect
{
  namespace http_server
  {
    static dlib::logger g_logger("file_cache");
    namespace fs = std::filesystem;

    // Register a file
    XmlNamespaceList FileCache::registerFile(const string &uri,
                                             const string &pathName,
                                             const string &version)
    {
      XmlNamespaceList namespaces;

      try
      {
        fs::path path(pathName);
        
        if (!fs::exists(path))
        {
          g_logger << dlib::LWARN << "The following path " <<
              pathName << " cannot be found, full path: " <<
              path;
        }
        else if (!fs::is_directory(path))
        {
          m_fileMap.emplace(uri, fs::absolute(path));
        }
        else
        {
          fs::path baseUri(uri, fs::path::format::generic_format);

          for (auto &file : fs::directory_iterator(path))
          {
            string name = file.path().filename();
            fs::path uri = baseUri / name;

            // Check if the file name maps to a standard MTConnect schema file.
            if (!name.find("MTConnect") && name.substr(name.length() - 4u, 4u) == ".xsd" &&
                version == name.substr(name.length() - 7u, 3u))
            {
              string version = name.substr(name.length() - 7u, 3u);

              if (name.substr(9u, 5u) == "Error")
              {
                string urn = "urn:mtconnect.org:MTConnectError:" + version;
                namespaces.emplace_back(urn, uri);
              }
              else if (name.substr(9u, 7u) == "Devices")
              {
                string urn = "urn:mtconnect.org:MTConnectDevices:" + version;
                namespaces.emplace_back(urn, uri);
              }
              else if (name.substr(9u, 6u) == "Assets")
              {
                string urn = "urn:mtconnect.org:MTConnectAssets:" + version;
                namespaces.emplace_back(urn, uri);
              }
              else if (name.substr(9u, 7u) == "Streams")
              {
                string urn = "urn:mtconnect.org:MTConnectStreams:" + version;
                namespaces.emplace_back(urn, uri);
              }
            }
          }
        }
      }
      catch (fs::filesystem_error e)
      {
        g_logger << dlib::LWARN << "The following path " <<
            pathName << " cannot be accessed: " <<
            e.what();
      }
      
      return namespaces;
    }
    
    CachedFilePtr FileCache::getFile(const std::string &name)
    {
      try {
        auto file = m_fileCache.find(name);
        if (file != m_fileCache.end())
        {
          return file->second;
        }
        else
        {
          auto path = m_fileMap.find(name);
          if (path != m_fileMap.end())
          {
            auto file = make_shared<CachedFile>(path->second);
            m_fileCache.emplace(name, file);
            return file;
          }
        }
      }
      catch (fs::filesystem_error e)
      {
        g_logger << dlib::LWARN << "Cannot open file " <<
                  name << ": " << e.what();
      }

      return nullptr;
    }
  }
}
