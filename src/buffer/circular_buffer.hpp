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

#include <boost/circular_buffer.hpp>

#include <cassert>
#include <memory>
#include <mutex>

#include "checkpoint.hpp"
#include "observation/observation.hpp"
#include "utilities.hpp"

namespace mtconnect::buffer {
  using SequenceNumber_t = uint64_t;

  class CircularBuffer
  {
  public:
    CircularBuffer(unsigned int bufferSize, int checkpointFreq)
      : m_sequence(1ull),
        m_firstSequence(m_sequence),
        m_slidingBufferSize(1 << bufferSize),
        m_slidingBuffer(m_slidingBufferSize),
        m_checkpointFreq(checkpointFreq),
        m_checkpointCount(m_slidingBufferSize / checkpointFreq),
        m_checkpoints(m_checkpointCount)
    {}

    ~CircularBuffer() { m_checkpoints.clear(); }

    observation::ObservationPtr getFromBuffer(uint64_t seq) const
    {
      auto off = seq - m_firstSequence;
      if (off < m_slidingBufferSize)
        return m_slidingBuffer[off];
      else
        return observation::ObservationPtr();
    }

    auto getIndexAt(uint64_t at) { return at - m_firstSequence; }

    SequenceNumber_t getSequence() const { return m_sequence; }

    unsigned int getBufferSize() const { return m_slidingBufferSize; }

    SequenceNumber_t getFirstSequence() const { return m_firstSequence; }

    void updateDataItems(std::unordered_map<std::string, WeakDataItemPtr> &diMap)
    {
      for (auto &o : m_slidingBuffer)
      {
        o->updateDataItem(diMap);
      }

      m_first.updateDataItems(diMap);
      m_latest.updateDataItems(diMap);

      for (auto &cp : m_checkpoints)
      {
        cp->updateDataItems(diMap);
      }
    }

    void setSequence(SequenceNumber_t seq)
    {
      m_sequence = seq;
      if (seq > m_slidingBufferSize)
        m_firstSequence = seq - m_slidingBuffer.size();
    }

    SequenceNumber_t addToBuffer(observation::ObservationPtr &observation)
    {
      if (observation->isOrphan())
        return 0;

      std::lock_guard<std::recursive_mutex> lock(m_sequenceLock);
      auto dataItem = observation->getDataItem();

      if (!dataItem->isDiscrete())
      {
        if (!observation->isUnavailable() && dataItem->isDataSet() &&
            !m_latest.dataSetDifference(observation))
        {
          return 0;
        }
      }

      auto seq = m_sequence;

      observation->setSequence(seq);
      m_slidingBuffer.push_back(observation);
      m_latest.addObservation(observation);

      // Special case for the first event in the series to prime the first checkpoint.
      if (seq == 1)
        m_first.addObservation(observation);
      else if (m_slidingBuffer.full())
      {
        observation::ObservationPtr old = m_slidingBuffer.front();
        m_first.addObservation(old);
        if (old->getSequence() > 1)
          m_firstSequence++;
        // assert(old->getSequence() == m_firstSequence);
      }

      // Checkpoint management
      if (m_checkpointCount > 0 && (seq % m_checkpointFreq) == 0)
      {
        // Copy the checkpoint from the current into the slot
        m_checkpoints.push_back(std::make_unique<Checkpoint>(m_latest));
      }

      m_sequence++;

      return seq;
    }

    // Checkpoint
    Checkpoint &getLatest() { return m_latest; }
    Checkpoint &getFirst() { return m_first; }
    auto getCheckoointFreq() { return m_checkpointFreq; }
    auto getCheckpointCount() { return m_checkpointCount; }

    std::unique_ptr<Checkpoint> getCheckpointAt(SequenceNumber_t at, const FilterSetOpt &filterSet)
    {
      std::lock_guard<std::recursive_mutex> lock(m_sequenceLock);

      // Compute the closest checkpoint. If the checkpoint is after the
      // first checkpoint and before the next incremental checkpoint,
      // use first.
      auto fi = (m_firstSequence / m_checkpointFreq);
      auto in = (at / m_checkpointFreq);
      int dt = int(in - fi) - 1;

      std::unique_ptr<Checkpoint> check;
      decltype(m_slidingBuffer.cbegin()) iter;
      decltype(m_slidingBuffer.cend()) end;

      if (dt < 0)
      {
        check = std::make_unique<Checkpoint>(m_first, filterSet);
        if (at == m_firstSequence)
          return check;

        iter = m_slidingBuffer.cbegin();
        end = iter + (at - m_firstSequence) + 1;
      }
      else
      {
        check = std::make_unique<Checkpoint>(*m_checkpoints[dt], filterSet);

        auto cps = in * m_checkpointFreq;
        if (at == cps)
          return check;

        auto ind = cps - m_firstSequence;
        iter = m_slidingBuffer.cbegin() + ind;
        end = iter + (at - cps) + 1;
      }

      // Roll forward from the checkpoint.
      while (iter != end)
      {
        check->addObservation(*iter++);
      }

      return check;
    }

    std::unique_ptr<observation::ObservationList> getObservations(
        int count, const FilterSetOpt &filterSet, const std::optional<SequenceNumber_t> start,
        const std::optional<SequenceNumber_t> to, SequenceNumber_t &end, SequenceNumber_t &firstSeq,
        bool &endOfBuffer)
    {
      auto results = std::make_unique<observation::ObservationList>();

      std::lock_guard<std::recursive_mutex> lock(m_sequenceLock);
      firstSeq = m_firstSequence;
      int limit, inc;

      SequenceNumber_t first;
      size_t max = m_slidingBuffer.size();

      // Determine where to start and direction of iteration.
      if (count >= 0)
      {
        if (to)
        {
          if (start && *start > m_firstSequence)
            firstSeq = *start;
          first = *to;
          inc = -1;
        }
        else
        {
          first = (start && *start > firstSeq) ? *start : firstSeq;
          inc = 1;
        }
        limit = count;
      }
      else
      {
        first = (start && *start < m_sequence) ? *start : m_sequence - 1;
        limit = -count;
        inc = -1;
      }

      size_t min = firstSeq - m_firstSequence;
      size_t i = first - m_firstSequence;
      for (int added = 0; added < limit && i < max && i >= min; i += inc)
      {
        // Filter out according to if it exists in the list
        auto &event = m_slidingBuffer[i];
        if (!event->isOrphan())
        {
          const std::string &dataId = event->getDataItem()->getId();
          if (!filterSet || filterSet->count(dataId) > 0)
          {
            results->push_back(event);
            added++;
          }
        }
      }

      if (to)
        end = first < m_sequence ? first + 1 : m_sequence;
      else
        end = m_firstSequence + i;

      if (count >= 0)
        endOfBuffer = i + m_firstSequence >= m_sequence;
      else
        endOfBuffer = i + m_firstSequence <= m_firstSequence;

      return results;
    }

    // For mutex locking
    auto lock() { return m_sequenceLock.lock(); }
    auto unlock() { return m_sequenceLock.unlock(); }
    auto try_lock() { return m_sequenceLock.try_lock(); }

  protected:
    // Access control to the buffer
    std::recursive_mutex m_sequenceLock;

    // Sequence number
    SequenceNumber_t m_sequence;
    SequenceNumber_t m_firstSequence;

    // The sliding/circular buffer to hold all of the events/sample data
    unsigned int m_slidingBufferSize;
    boost::circular_buffer<observation::ObservationPtr> m_slidingBuffer;

    // Checkpoints
    SequenceNumber_t m_checkpointFreq;
    SequenceNumber_t m_checkpointCount;

    Checkpoint m_latest;
    Checkpoint m_first;
    boost::circular_buffer<std::unique_ptr<Checkpoint>> m_checkpoints;
  };
}  // namespace mtconnect::buffer
