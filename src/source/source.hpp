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

#include <boost/asio.hpp>
#include <boost/function.hpp>

#include "utilities.hpp"

namespace mtconnect {
  namespace pipeline {
    class Pipeline;
    class PipelineContext;
  }  // namespace pipeline

  namespace source {
    class Source;
    using SourcePtr = std::shared_ptr<Source>;
    using SourceFactoryFn = boost::function<std::shared_ptr<Source>(
        const std::string &name, boost::asio::io_context &io,
        std::shared_ptr<pipeline::PipelineContext> pipelineContext, const ConfigOptions &options,
        const boost::property_tree::ptree &block)>;

    class Source : public std::enable_shared_from_this<Source>
    {
    public:
      Source(boost::asio::io_context &io) : m_strand(io) {}
      Source(boost::asio::io_context::strand &io) : m_strand(io) {}
      Source(const std::string &name, boost::asio::io_context &io) : m_name(name), m_strand(io) {}
      Source(const std::string &name, boost::asio::io_context::strand &io)
        : m_name(name), m_strand(io)
      {}
      virtual ~Source() {}

      SourcePtr getptr() const { return const_cast<Source *>(this)->shared_from_this(); }

      virtual bool start() = 0;
      virtual void stop() = 0;
      virtual bool isLoopback() { return false; }
      virtual const std::string &getIdentity() const { return m_name; }

      virtual pipeline::Pipeline *getPipeline() = 0;

      const auto &getName() const { return m_name; }
      boost::asio::io_context::strand &getStrand();

    protected:
      std::string m_name;
      boost::asio::io_context::strand m_strand;
    };

    class SourceFactory
    {
    public:
      SourcePtr make(const std::string &factoryName, const std::string &sinkName,
                     boost::asio::io_context &io,
                     std::shared_ptr<pipeline::PipelineContext> context,
                     const ConfigOptions &options, const boost::property_tree::ptree &block);

      void registerFactory(const std::string &name, SourceFactoryFn function)
      {
        m_factories.insert_or_assign(name, function);
      }

      void clear() { m_factories.clear(); }

      bool hasFactory(const std::string &name) { return m_factories.count(name) > 0; }

    private:
      std::map<std::string, SourceFactoryFn> m_factories;
    };

    using SourceList = std::list<SourcePtr>;
  }  // namespace source
}  // namespace mtconnect
