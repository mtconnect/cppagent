//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "pipeline/pipeline.hpp"
#include "pipeline/transform.hpp"

namespace mtconnect
{
  class Agent;
  class Asset;
  using AssetPtr = std::shared_ptr<Asset>;

  namespace adapter
  {
    class AdapterPipeline : public pipeline::Pipeline
    {
    public:
      AdapterPipeline(pipeline::PipelineContextPtr context) : Pipeline(context) {}

      void build(const ConfigOptions &options) override;
      std::unique_ptr<adapter::Handler> makeHandler();

    protected:
      ConfigOptions m_options;
    };
  }  // namespace adapter
}  // namespace mtconnect
