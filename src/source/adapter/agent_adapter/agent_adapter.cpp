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

#include "agent_adapter.hpp"

#include <boost/algorithm/string.hpp>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "configuration/config_options.hpp"
#include "http_session.hpp"
#include "https_session.hpp"
#include "logging.hpp"
#include "pipeline/deliver.hpp"
#include "session_impl.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::pipeline;

namespace mtconnect::source::adapter::agent_adapter {
  void AgentAdapterPipeline::build(const ConfigOptions &options)
  {
    buildDeviceList();
    buildCommandAndStatusDelivery();

    TransformPtr next = bind(make_shared<MTConnectXmlTransform>(m_context, m_device));
    std::optional<string> obsMetrics;
    obsMetrics = m_identity + "_observation_update_rate";
    next->bind(make_shared<DeliverObservation>(m_context, obsMetrics));

    buildAssetDelivery(next);
  }

  AgentAdapter::AgentAdapter(boost::asio::io_context &io, pipeline::PipelineContextPtr context,
                             const ConfigOptions &options, const boost::property_tree::ptree &block)
    : Adapter("AgentAdapter", io, options),
      m_pipeline(context, Source::m_strand),
      m_reconnectTimer(io)
  {
    GetOptions(block, m_options, options);
    AddOptions(block, m_options,
               {{configuration::UUID, string()},
                {configuration::Manufacturer, string()},
                {configuration::Station, string()},
                {configuration::SourceDevice, string()},
                {configuration::Url, string()}});

    AddDefaultedOptions(block, m_options,
                        {{configuration::Host, "localhost"s},
                         {configuration::Port, 5000},
                         {configuration::Count, 1000},
                         {configuration::Heartbeat, 10000},
                         {configuration::AutoAvailable, false},
                         {configuration::RealTime, false},
                         {configuration::ReconnectInterval, 10},
                         {configuration::RelativeTime, false},
                         {"!CloseConnectionAfterResponse!", false}});

    m_handler = m_pipeline.makeHandler();

    auto urlOpt = GetOption<std::string>(m_options, configuration::Url);
    if (urlOpt)
    {
      m_url = Url::parse(*urlOpt);
    }
    else
    {
      m_url.m_protocol = "http";
      m_url.m_host = *GetOption<string>(m_options, configuration::Host);
      m_url.m_port = GetOption<int>(m_options, configuration::Port);
      m_url.m_path = GetOption<string>(m_options, configuration::Device).value_or("/");
    }

    if (m_url.m_path[m_url.m_path.size() - 1] != '/')
      m_url.m_path.append("/");

    m_count = *GetOption<int>(m_options, configuration::Count);
    m_heartbeat = *GetOption<int>(m_options, configuration::Heartbeat);
    m_sourceDevice = GetOption<std::string>(m_options, configuration::SourceDevice);
    m_reconnectInterval =
        std::chrono::seconds(*GetOption<int>(m_options, configuration::ReconnectInterval));
    m_closeConnectionAfterResponse = *GetOption<bool>(m_options, "!CloseConnectionAfterResponse!");

    m_pipeline.m_handler = m_handler.get();
    m_pipeline.build(m_options);
  }

  AgentAdapter::~AgentAdapter() { m_reconnectTimer.cancel(); }

  bool AgentAdapter::start()
  {
    m_pipeline.start();

    if (m_url.m_protocol == "https")
    {
      // The SSL context is required, and holds certificates
      ssl::context ctx {ssl::context::tlsv12_client};
      ctx.set_verify_mode(ssl::verify_peer);
      m_session = make_shared<HttpsSession>(m_strand, m_url, ctx);
      m_assetSession = make_shared<HttpsSession>(m_strand, m_url, ctx);
    }
    else if (m_url.m_protocol == "http")
    {
      m_session = make_shared<HttpSession>(m_strand, m_url);
      m_assetSession = make_shared<HttpSession>(m_strand, m_url);
    }
    else
    {
      LOG(error) << "Unknown protocol: " << m_url.m_protocol;
      return false;
    }

    m_session->m_handler = m_handler.get();
    m_session->m_identity = m_identity;
    m_session->m_closeConnectionAfterResponse = m_closeConnectionAfterResponse;
    m_session->m_updateAssets = [this]() { updateAssets(); };

    m_assetSession->m_handler = m_handler.get();
    m_assetSession->m_identity = m_identity;
    m_assetSession->m_closeConnectionAfterResponse = m_closeConnectionAfterResponse;

    using namespace std::placeholders;
    m_session->m_failed = std::bind(&AgentAdapter::sessionFailed, this, _1);
    m_assetSession->m_failed = std::bind(&AgentAdapter::sessionFailed, this, _1);

    run();

    return true;
  }
  
  void AgentAdapter::recoverStreams()
  {
    if (!m_reconnecting)
    {
      m_reconnecting = true;
      m_session->stop();
      m_assetSession->stop();
      
      m_reconnectTimer.expires_after(m_reconnectInterval);
      m_reconnectTimer.async_wait(
          asio::bind_executor(m_strand, [this](boost::system::error_code ec) {
            if (!ec)
            {
              if (instanceId() != 0)
                recover();
              else
                run();
            }
          }));
    }
  }
  
  void AgentAdapter::sessionFailed(std::error_code &ec)
  {
    if (ec.category() == source::TheErrorCategory())
    {
      switch (static_cast<ErrorCode>(ec.value()))
      {
        case ErrorCode::INSTANCE_ID_CHANGED:
        case ErrorCode::RESTART_STREAM:
          run();
          break;
          
        case ErrorCode::STREAM_CLOSED:
          if (m_handler->m_disconnected)
            m_handler->m_disconnected(m_identity);
          recoverStreams();
          break;

        case ErrorCode::RECOVER_STREAM:
          recoverStreams();
          break;
          
        case ErrorCode::ADAPTER_FAILED:
          if (m_handler->m_disconnected)
            m_handler->m_disconnected(m_identity);
          m_pipeline.getContext()->m_contract->sourceFailed(m_identity);
          break;
          
        default:
          LOG(error) << "Unknown error: " << ec.message();
          break;
      }
    }
    else
    {
      if (m_handler->m_disconnected)
        m_handler->m_disconnected(m_identity);
    }
  }

  void AgentAdapter::run()
  {
    const auto &feedback =
        m_pipeline.getContext()->getSharedState<XmlTransformFeedback>("XmlTransformFeedback");
    feedback->m_instanceId = 0;
    feedback->m_next = 0;

    m_reconnecting = false;
    assets();
    current();
  }
  
  void AgentAdapter::recover()
  {
    m_reconnecting = false;
    sample();
  }

  void AgentAdapter::current()
  {
    m_session->makeRequest("current", UrlQuery(), false, [this]() { return sample(); });
  }

  bool AgentAdapter::sample()
  {
    const auto &feedback =
        m_pipeline.getContext()->getSharedState<XmlTransformFeedback>("XmlTransformFeedback");

    using namespace boost;
    UrlQuery query({{"from", lexical_cast<string>(feedback->m_next)},
                    {"count", lexical_cast<string>(m_count)},
                    {"heartbeat", lexical_cast<string>(m_heartbeat)},
                    {"interval", "500"}});
    m_session->makeRequest("sample", query, true, nullptr);

    return true;
  }

  void AgentAdapter::stop()
  {
    m_reconnectTimer.cancel();
    m_session->stop();
    m_session.reset();
  }

  void AgentAdapter::assets()
  {
    UrlQuery query({{"count", "1048576"}});
    m_assetSession->makeRequest("assets", query, false, nullptr);
  }

  void AgentAdapter::updateAssets()
  {
    const auto &feedback =
        m_pipeline.getContext()->getSharedState<XmlTransformFeedback>("XmlTransformFeedback");

    std::vector<string> idList;
    std::transform(feedback->m_assetEvents.begin(), feedback->m_assetEvents.end(), back_inserter(idList),
                   [](const EntityPtr entity) { return entity->getValue<string>(); });
    string ids = boost::join(idList, ";");

    m_assetSession->makeRequest("/assets/" + ids, UrlQuery(), false, nullptr);
  }

}  // namespace mtconnect::source::adapter::agent_adapter
