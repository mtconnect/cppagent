//
// Copyright 2009-2026, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "mtconnect/printer/html_printer.hpp"

#include <fstream>
#include <iterator>

namespace mtconnect::printer {
  using namespace std;

  void HtmlPrinter::loadFile(FileResolver&& resolver)
  {
    m_loaded = false;
    m_prefix.clear();
    m_suffix.clear();

    // The file cache resolves the path; the file contents are only in memory if it is cached.
    auto file = resolver(m_browserView);
    if (!file)
    {
      LOG(warning) << "HtmlPrinter: cannot find browser view: " << m_browserView;
      return;
    }

    string contents;
    ifstream stream(file->string(), ios::binary);
    if (!stream)
    {
      LOG(warning) << "HtmlPrinter: cannot open browser view: " << file->string();
      return;
    }
    contents.assign(istreambuf_iterator<char>(stream), istreambuf_iterator<char>());

    auto pos = contents.find(ContentPlaceholder);
    if (pos == string::npos)
    {
      LOG(warning) << "HtmlPrinter: browser view " << m_browserView << " does not contain "
                   << ContentPlaceholder;
      return;
    }

    m_suffix = contents.substr(pos + ContentPlaceholder.size());
    contents.resize(pos);
    m_prefix = std::move(contents);
    m_loaded = true;
  }

  inline std::string HtmlPrinter::formatResult(const std::string& content) const
  {
    if (!m_loaded)
      return content;

    // Single allocation and copy: prefix + content + suffix
    string result;
    result.reserve(m_prefix.size() + content.size() + m_suffix.size());
    result.append(m_prefix).append(content).append(m_suffix);
    return result;
  }

  std::string HtmlPrinter::printErrors(const uint64_t instanceId, const unsigned int bufferSize,
                                       const uint64_t nextSeq, const entity::EntityList& list,
                                       bool pretty,
                                       const std::optional<std::string> requestId) const
  {
    auto doc = m_delegate->printErrors(instanceId, bufferSize, nextSeq, list, pretty, requestId);
    return formatResult(doc);
  }

  std::string HtmlPrinter::printProbe(const uint64_t instanceId, const unsigned int bufferSize,
                                      const uint64_t nextSeq, const unsigned int assetBufferSize,
                                      const unsigned int assetCount,
                                      const std::list<DevicePtr>& devices,
                                      const std::map<std::string, size_t>* count,
                                      bool includeHidden, bool pretty,
                                      const std::optional<std::string> requestId) const
  {
    auto doc = m_delegate->printProbe(instanceId, bufferSize, nextSeq, assetBufferSize, assetCount,
                                      devices, count, includeHidden, pretty, requestId);

    return formatResult(doc);
  }

  std::string HtmlPrinter::printSample(const uint64_t instanceId, const unsigned int bufferSize,
                                       const uint64_t nextSeq, const uint64_t firstSeq,
                                       const uint64_t lastSeq,
                                       observation::ObservationList& results, bool pretty,
                                       const std::optional<std::string> requestId) const
  {
    auto doc = m_delegate->printSample(instanceId, bufferSize, nextSeq, firstSeq, lastSeq, results,
                                       pretty, requestId);
    return formatResult(doc);
  }

  std::string HtmlPrinter::printAssets(const uint64_t instanceId, const unsigned int bufferSize,
                                       const unsigned int assetCount, const asset::AssetList& asset,
                                       bool pretty,
                                       const std::optional<std::string> requestId) const
  {
    auto doc =
        m_delegate->printAssets(instanceId, bufferSize, assetCount, asset, pretty, requestId);
    return formatResult(doc);
  }

}  // namespace mtconnect::printer
