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

#include "adapter/adapter.hpp"

namespace mtconnect::adapter::agent 
{
  using namespace mtconnect;
  using namespace adapter;
  
  class Client;

  class AgentAdapter : public Adapter
  {
  public:
    AgentAdapter(const std::string &name, boost::asio::io_context &io, const ConfigOptions &options)
      : Adapter(name, io, options)
    {
    }
    
    const std::string &getHost() const override
    {
      return m_host;
    }
    
    unsigned int getPort() const override
    {
      return 0;      
    }

    ~AgentAdapter() override {}

    bool start() override;
    void stop() override;    
    pipeline::Pipeline *getPipeline() override;
    
  protected:
    std::string m_host;
    std::unique_ptr<Client> m_client;
  };
  
  
}
