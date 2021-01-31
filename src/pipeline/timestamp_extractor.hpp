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

#include "shdr_tokenizer.hpp"
#include "transform.hpp"

namespace mtconnect
{
  class Agent;

  namespace pipeline
  {
    using namespace entity;
    using Micros = std::chrono::microseconds;
    using Timestamp = std::chrono::time_point<std::chrono::system_clock>;

    class Timestamped : public Tokens
    {
    public:
      using Tokens::Tokens;
      Timestamped(const Timestamped &ts) = default;
      Timestamped(const Tokens &ptr) : Tokens(ptr) {}
      Timestamped(const Timestamped &ts, TokenList list)
        : Tokens(ts, list), m_timestamp(ts.m_timestamp), m_duration(ts.m_duration)
      {
      }
      ~Timestamped() = default;
      Timestamp m_timestamp;
      std::optional<double> m_duration;
    };
    using TimestampedPtr = std::shared_ptr<Timestamped>;

    class ExtractTimestamp : public Transform
    {
    public:
      ExtractTimestamp(const ExtractTimestamp &) = default;
      ExtractTimestamp()
      {
        m_guard = TypeGuard<Tokens>();
      }
      ~ExtractTimestamp() override = default;

      using Now = std::function<Timestamp()>;
      const EntityPtr operator()(const EntityPtr ptr) override
      {
        TimestampedPtr res;
        std::optional<std::string> token;
        if (auto tokens = std::dynamic_pointer_cast<Tokens>(ptr);
            tokens && tokens->m_tokens.size() > 0)
        {
          res = std::make_shared<Timestamped>(*tokens);
          token = res->m_tokens.front();
          res->m_tokens.pop_front();
        }
        else if (ptr->hasProperty("timestamp"))
        {
          token = res->maybeGet<std::string>("timestamp");
          if (token)
            res->erase("timestamp");
        }

        if (token)
          extractTimestamp(*token, res);
        else
          res->m_timestamp = now();

        res->setProperty("timestamp", res->m_timestamp);
        return next(res);
      }

      void extractTimestamp(const std::string &token, TimestampedPtr &ts);
      inline Timestamp now() { return m_now ? m_now() : std::chrono::system_clock::now(); }
      
      Now m_now;
      bool m_relativeTime{false};

    protected:
      std::optional<Timestamp> m_base;
      Micros m_offset;
    };

    class IgnoreTimestamp : public ExtractTimestamp
    {
    public:
      using ExtractTimestamp::ExtractTimestamp;
      ~IgnoreTimestamp() override = default;

      const EntityPtr operator()(const EntityPtr ptr) override
      {
        TimestampedPtr res;
        std::optional<std::string> token;
        if (auto tokens = std::dynamic_pointer_cast<Tokens>(ptr);
            tokens && tokens->m_tokens.size() > 0)
        {
          res = std::make_shared<Timestamped>(*tokens);
          res->m_tokens.pop_front();
        }
        else if (res->hasProperty("timestamp"))
        {
          res->erase("timestamp");
        }
        res->m_timestamp = now();
        res->setProperty("timestamp", res->m_timestamp);

        return next(res);
      }      
    };
  }  // namespace source
}  // namespace mtconnect
