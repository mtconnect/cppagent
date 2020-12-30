
#pragma once

#include "globals.hpp"
#include "observation.hpp"
#include "asset.hpp"

#include <list>
#include <map>
#include <string>
#include <vector>

namespace mtconnect
{
  class Device;
  class Asset;
  class CuttingTool;

  using ProtoErrorList = std::list<std::pair<std::string,std::string>>;

  class Printer
  {
   public:
    Printer(bool pretty = false) : m_pretty(pretty) {}
    virtual ~Printer() = default;

    virtual std::string printError(const unsigned int instanceId, const unsigned int bufferSize,
                                   const uint64_t nextSeq, const std::string &errorCode,
                                   const std::string &errorText) const
    {
      return printErrors(instanceId, bufferSize, nextSeq,
                        { { errorCode, errorText } });
    }
    virtual std::string printErrors(const unsigned int instanceId, const unsigned int bufferSize,
                           const uint64_t nextSeq, const ProtoErrorList &list) const = 0;

    virtual std::string printProbe(const unsigned int instanceId, const unsigned int bufferSize,
                                   const uint64_t nextSeq, const unsigned int assetBufferSize,
                                   const unsigned int assetCount,
                                   const std::vector<Device *> &devices,
                                   const std::map<std::string, int> *count = nullptr) const = 0;

    virtual std::string printSample(const unsigned int instanceId, const unsigned int bufferSize,
                                    const uint64_t nextSeq, const uint64_t firstSeq,
                                    const uint64_t lastSeq, ObservationPtrArray &results) const = 0;
    virtual std::string printAssets(const unsigned int anInstanceId, const unsigned int bufferSize,
                                    const unsigned int assetCount,
                                    AssetList const &assets) const = 0;
    virtual std::string mimeType() const = 0;

   protected:
    bool m_pretty;
  };
}  // namespace mtconnect
