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

#include "agent_adapter.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/uuid/name_generator_sha1.hpp>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "http_session.hpp"
#include "https_session.hpp"
#include "mtconnect/configuration/config_options.hpp"
#include "mtconnect/logging.hpp"
#include "mtconnect/pipeline/deliver.hpp"
#include "session_impl.hpp"

using namespace std;
using namespace mtconnect;
using namespace mtconnect::pipeline;

namespace mtconnect::source::adapter::agent_adapter {
  void AgentAdapterPipeline::build(const ConfigOptions &options)
  {
    m_options = options;
    m_uuid = GetOption<string>(options, configuration::UUID);

    buildDeviceList();
    buildCommandAndStatusDelivery();

    TransformPtr next =
        bind(make_shared<MTConnectXmlTransform>(m_context, m_feedback, m_device, m_uuid));
    std::optional<string> obsMetrics;
    obsMetrics = m_identity + "_observation_update_rate";
    next->bind(make_shared<DeliverObservation>(m_context, obsMetrics));
    buildDeviceDelivery(next);
    buildAssetDelivery(next);

    applySplices();
  }

  AgentAdapter::AgentAdapter(boost::asio::io_context &io, pipeline::PipelineContextPtr context,
                             const ConfigOptions &options, const boost::property_tree::ptree &block)
    : Adapter("AgentAdapter", io, options),
      m_pipeline(context, Source::m_strand, m_feedback),
      m_reconnectTimer(io),
      m_pollingTimer(io),
      m_assetRetryTimer(io)
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
                         {configuration::Heartbeat, 10000ms},
                         {configuration::PollingInterval, 500ms},
                         {configuration::AutoAvailable, false},
                         {configuration::RealTime, false},
                         {configuration::ReconnectInterval, 10000ms},
                         {configuration::RelativeTime, false},
                         {configuration::UsePolling, false},
                         {configuration::EnableSourceDeviceModels, false},
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
    m_heartbeat = *GetOption<Milliseconds>(m_options, configuration::Heartbeat);
    m_usePolling = *GetOption<bool>(m_options, configuration::UsePolling);
    m_reconnectInterval = *GetOption<Milliseconds>(m_options, configuration::ReconnectInterval);
    m_pollingInterval = *GetOption<Milliseconds>(m_options, configuration::PollingInterval);
    m_probeAgent = *GetOption<bool>(m_options, configuration::EnableSourceDeviceModels);

    m_closeConnectionAfterResponse = *GetOption<bool>(m_options, "!CloseConnectionAfterResponse!");

    auto device = GetOption<string>(m_options, configuration::Device);
    if (!device)
    {
      LOG(fatal) << "Agent Adapter must target a device";
      m_failed = true;
    }
    else
    {
      m_sourceDevice = GetOption<string>(m_options, configuration::SourceDevice).value_or(*device);
    }

    m_name = m_url.getUrlText(m_sourceDevice);
    boost::uuids::detail::sha1 sha1;
    sha1.process_bytes(m_name.c_str(), m_name.length());
    boost::uuids::detail::sha1::digest_type digest;
    sha1.get_digest(digest);

    stringstream identity;
    identity << std::hex << digest[0] << digest[1] << digest[2];
    m_identity = string("_") + (identity.str()).substr(0, 10);

    m_options.insert_or_assign(configuration::AdapterIdentity, m_identity);
    m_feedbackId = "XmlTransformFeedback:" + m_identity;

    m_pipeline.m_handler = m_handler.get();
    m_pipeline.build(m_options);
  }

  AgentAdapter::~AgentAdapter() { m_reconnectTimer.cancel(); }

  bool AgentAdapter::start()
  {
    if (m_failed)
    {
      LOG(error) << "Agent adapter cannot start: fatal error";
      return false;
    }
    m_pipeline.start();

    if (m_url.m_protocol == "https")
    {
      // The SSL context is required, and holds certificates
      m_streamContext = make_unique<ssl::context>(ssl::context::tlsv12_client);
      m_streamContext->set_verify_mode(ssl::verify_none);

      m_session = make_shared<HttpsSession>(m_strand, m_url, *m_streamContext);

      m_assetContext = make_unique<ssl::context>(ssl::context::tlsv12_client);
      m_assetContext->set_verify_mode(ssl::verify_none);

      m_assetSession = make_shared<HttpsSession>(m_strand, m_url, *m_assetContext);
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
    m_assetSession->m_failed = std::bind(&AgentAdapter::assetsFailed, this, _1);
    m_session->m_failed = std::bind(&AgentAdapter::streamsFailed, this, _1);

    run();

    return true;
  }

  void AgentAdapter::clear()
  {
    m_streamRequest.reset();
    m_assetRequest.reset();

    m_assetRetryTimer.cancel();
    m_pollingTimer.cancel();
    m_reconnectTimer.cancel();

    if (m_session)
      m_session->stop();
    if (m_assetSession)
      m_assetSession->stop();

    m_feedback.m_instanceId = 0;
    m_feedback.m_next = 0;
  }

  void AgentAdapter::recoverStreams()
  {
    if (m_stopped)
      return;

    if (!canRecover())
    {
      clear();
    }

    m_reconnectTimer.expires_after(m_reconnectInterval);
    m_reconnectTimer.async_wait(asio::bind_executor(m_strand, [this](boost::system::error_code ec) {
      if (!ec)
      {
        if (canRecover() && m_streamRequest)
          m_session->makeRequest(*m_streamRequest);
        else
          run();
      }
    }));
  }

  void AgentAdapter::recoverAssetRequest()
  {
    if (m_assetRequest)
    {
      m_assetRetryTimer.expires_after(m_reconnectInterval);
      m_assetRetryTimer.async_wait(
          asio::bind_executor(m_strand, [this](boost::system::error_code ec) {
            if (!ec && m_assetRequest)
            {
              m_assetSession->makeRequest(*m_assetRequest);
            }
          }));
    }
  }

  void AgentAdapter::assetsFailed(std::error_code &ec)
  {
    if (m_stopped)
      return;

    if (ec.category() == source::TheErrorCategory())
    {
      switch (static_cast<ErrorCode>(ec.value()))
      {
        case ErrorCode::ADAPTER_FAILED:
          stop();
          if (m_handler->m_disconnected)
            m_handler->m_disconnected(m_identity);
          m_pipeline.getContext()->m_contract->sourceFailed(m_identity);
          break;

        case ErrorCode::RETRY_REQUEST:
          recoverAssetRequest();
          break;

        default:

          break;
      }
    }
  }

  void AgentAdapter::streamsFailed(std::error_code &ec)
  {
    if (m_stopped)
      return;

    if (ec.category() == source::TheErrorCategory())
    {
      switch (static_cast<ErrorCode>(ec.value()))
      {
        case ErrorCode::INSTANCE_ID_CHANGED:
        case ErrorCode::RESTART_STREAM:
        {
          if (m_handler->m_disconnected)
            m_handler->m_disconnected(m_identity);
          clear();
          recoverStreams();
          break;
        }

        case ErrorCode::RETRY_REQUEST:
          recoverStreams();
          break;

        case ErrorCode::STREAM_CLOSED:
          if (m_handler->m_disconnected)
            m_handler->m_disconnected(m_identity);
          recoverStreams();
          break;

        case ErrorCode::ADAPTER_FAILED:
          stop();
          if (m_handler->m_disconnected)
            m_handler->m_disconnected(m_identity);
          m_pipeline.getContext()->m_contract->sourceFailed(m_identity);
          break;

        case ErrorCode::MULTIPART_STREAM_FAILED:
          m_usePolling = true;
          recoverStreams();
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

      switch (static_cast<beast::http::error>(ec.value()))
      {
        case beast::http::error::end_of_stream:
          recoverStreams();
          break;

        default:
          break;
      }
    }
  }

  void AgentAdapter::run()
  {
    if (m_stopped)
      return;

    clear();

    m_reconnecting = false;
    if (m_probeAgent)
    {
      probe();
    }
    else
    {
      assets();
      current();
    }
  }

  void AgentAdapter::recover()
  {
    if (m_stopped)
      return;

    m_reconnecting = false;
    sample();
  }

  bool AgentAdapter::probe()
  {
    if (m_stopped)
      return false;

    m_streamRequest.emplace(m_sourceDevice, "probe", UrlQuery(), false, [this]() {
      m_agentVersion = m_feedback.m_agentVersion;
      assets();
      return current();
    });
    return m_session->makeRequest(*m_streamRequest);
  }

  bool AgentAdapter::current()
  {
    if (m_stopped)
      return false;

    m_streamRequest.emplace(m_sourceDevice, "current", UrlQuery(), false,
                            [this]() { return sample(); });
    return m_session->makeRequest(*m_streamRequest);
  }

  bool AgentAdapter::sample()
  {
    if (m_stopped)
      return false;

    if (m_usePolling)
    {
      using namespace boost;
      UrlQuery query({{"from", lexical_cast<string>(m_feedback.m_next)},
                      {"count", lexical_cast<string>(m_count)}});
      m_streamRequest.emplace(m_sourceDevice, "sample", query, false, [this]() {
        m_pollingTimer.expires_after(m_pollingInterval);
        m_pollingTimer.async_wait(
            asio::bind_executor(m_strand, [this](boost::system::error_code ec) {
              if (!ec && m_streamRequest)
              {
                sample();
              }
            }));
        return true;
      });
      m_session->makeRequest(*m_streamRequest);
    }
    else
    {
      using namespace boost;
      UrlQuery query({{"from", lexical_cast<string>(m_feedback.m_next)},
                      {"count", lexical_cast<string>(m_count)},
                      {"heartbeat", lexical_cast<string>(m_heartbeat.count())},
                      {"interval", lexical_cast<string>(m_pollingInterval.count())}});
      m_streamRequest.emplace(m_sourceDevice, "sample", query, true, nullptr);
      m_session->makeRequest(*m_streamRequest);
    }

    return true;
  }

  void AgentAdapter::stop()
  {
    m_stopped = true;
    clear();
    if (m_session)
      m_session->stop();
    m_session.reset();
    if (m_assetSession)
      m_assetSession->stop();
    m_assetSession.reset();
    m_pipeline.clear();
  }

  void AgentAdapter::assets()
  {
    UrlQuery query({{"count", "1048576"}});

    std::optional<std::string> source;
    if (m_agentVersion >= 200)
      source.emplace(m_sourceDevice);
    m_assetRequest.emplace(source, "assets", query, false, [this]() {
      m_assetRequest.reset();
      return true;
    });
    m_assetSession->makeRequest(*m_assetRequest);
  }

  void AgentAdapter::updateAssets()
  {
    std::vector<string> idList;
    std::transform(m_feedback.m_assetEvents.begin(), m_feedback.m_assetEvents.end(),
                   back_inserter(idList),
                   [](const EntityPtr entity) { return entity->getValue<string>(); });
    string ids = boost::join(idList, ";");

    m_assetRequest.emplace(nullopt, "assets/" + ids, UrlQuery(), false, [this]() {
      m_assetRequest.reset();
      return true;
    });
    m_assetSession->makeRequest(*m_assetRequest);
  }

}  // namespace mtconnect::source::adapter::agent_adapter
