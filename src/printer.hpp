
#pragma once

#include "globals.hpp"
#include "observation.hpp"

#include <list>
#include <map>
#include <string>
#include <vector>

namespace mtconnect
{
  class Device;
  class Asset;
  class CuttingTool;

  class Printer
  {
   public:
    Printer(bool pretty = false) : m_pretty(pretty) {}
    virtual ~Printer() = default;

    virtual std::string printError(const unsigned int instanceId, const unsigned int bufferSize,
                                   const uint64_t nextSeq, const std::string &errorCode,
                                   const std::string &errorText) const = 0;

    virtual std::string printProbe(const unsigned int instanceId, const unsigned int bufferSize,
                                   const uint64_t nextSeq, const unsigned int assetBufferSize,
                                   const unsigned int assetCount,
                                   const std::vector<Device *> &devices,
                                   const std::map<std::string, int> *count = nullptr) const = 0;

    virtual std::string printSample(const unsigned int instanceId, const unsigned int bufferSize,
                                    const uint64_t nextSeq, const uint64_t firstSeq,
                                    const uint64_t lastSeq, ObservationPtrArray &results) const = 0;
#if 0
    virtual std::string printAssets(const unsigned int anInstanceId, const unsigned int bufferSize,
                                    const unsigned int assetCount,
                                    std::vector<AssetPtr> const &assets) const = 0;

    virtual std::string printCuttingTool(CuttingToolPtr const tool) const = 0;
#endif
    virtual std::string mimeType() const = 0;

   protected:
    bool m_pretty;
  };
}  // namespace mtconnect
