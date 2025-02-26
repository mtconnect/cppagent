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

#include <boost/asio.hpp>
#include <boost/function.hpp>

#include "mtconnect/config.hpp"
#include "mtconnect/utilities.hpp"

namespace mtconnect {
  namespace pipeline {
    class Pipeline;
    class PipelineContext;
  }  // namespace pipeline

  /// @brief The data source namespace
  namespace source {
    class Source;
    using SourcePtr = std::shared_ptr<Source>;
    using SourceFactoryFn = boost::function<std::shared_ptr<Source>(
        const std::string &name, boost::asio::io_context &io,
        std::shared_ptr<pipeline::PipelineContext> pipelineContext, const ConfigOptions &options,
        const boost::property_tree::ptree &block)>;

    /// @brief Abstract agent data source
    class AGENT_LIB_API Source : public std::enable_shared_from_this<Source>
    {
    public:
      /// @brief Create a source with an io context
      /// @param io boost asio io context
      Source(boost::asio::io_context &io) : m_strand(io) {}
      /// @brief Create a source with a strand
      /// @param io boost asio strand
      Source(boost::asio::io_context::strand &io) : m_strand(io) {}
      /// @brief Create a named source with an io context
      /// @param name source name
      /// @param io boost asio io context
      Source(const std::string &name, boost::asio::io_context &io) : m_name(name), m_strand(io) {}
      /// @brief Create a named source with a strand
      /// @param name source name
      /// @param io boost asio strand
      Source(const std::string &name, boost::asio::io_context::strand &io)
        : m_name(name), m_strand(io)
      {}
      virtual ~Source() {}

      /// @brief get a shared pointer to the source
      /// @return shared pointer to this
      SourcePtr getptr() const { return const_cast<Source *>(this)->shared_from_this(); }

      /// @brief start the source
      /// @return `true` if it succeeded
      virtual bool start() = 0;
      /// @brief stop the source
      virtual void stop() = 0;
      /// @brief check if this is a loopback source
      /// @return `true` if it is a loopback source
      virtual bool isLoopback() { return false; }
      /// @brief get the identity of the source
      /// @return the identity
      virtual const std::string &getIdentity() const { return m_name; }

      /// @brief get the pipeline associated with the source
      /// @return pointer to the pipeline
      virtual pipeline::Pipeline *getPipeline() = 0;

      /// @brief get the name of the source
      /// @return the name
      const auto &getName() const { return m_name; }
      /// @brief get the source's strand
      /// @return the asio strand
      boost::asio::io_context::strand &getStrand();

      /// @brief changes the options in the source
      /// @param[in] options the options to update
      virtual void setOptions(const ConfigOptions &options) {}

    protected:
      std::string m_name;
      boost::asio::io_context::strand m_strand;
    };

    /// @brief A factory for creating the source
    class AGENT_LIB_API SourceFactory
    {
    public:
      /// @brief make a source using this factory
      /// @param factoryName the name of the the factory
      /// @param sourceName a the name of the source
      /// @param io the boost asio io context
      /// @param context pipeline context
      /// @param options configuration options
      /// @param block additional options
      /// @return shared pointer to the source
      SourcePtr make(const std::string &factoryName, const std::string &sourceName,
                     boost::asio::io_context &io,
                     std::shared_ptr<pipeline::PipelineContext> context,
                     const ConfigOptions &options, const boost::property_tree::ptree &block);

      /// @brief Register the factory with the factory name
      /// @param name the name of the factory
      /// @param function factory function to create a source
      void registerFactory(const std::string &name, SourceFactoryFn function)
      {
        m_factories.insert_or_assign(name, function);
      }

      /// @brief clear the factories
      void clear() { m_factories.clear(); }
      /// @brief check if a factory exists
      /// @param name the name of the factory
      /// @return `true` if the factory is registered
      bool hasFactory(const std::string &name) { return m_factories.count(name) > 0; }

    private:
      std::map<std::string, SourceFactoryFn> m_factories;
    };

    using SourceList = std::list<SourcePtr>;
  }  // namespace source
}  // namespace mtconnect
