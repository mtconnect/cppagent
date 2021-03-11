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

#include "checkpoint.hpp"
#include "utilities.hpp"

#include <boost/circular_buffer.hpp>

#include <memory>
#include <mutex>

namespace mtconnect
{
  namespace observation
  {
    using SequenceNumber_t = uint64_t;

    class CircularBuffer
    {
    public:
      CircularBuffer(unsigned int bufferSize, int checkpointFreq)
        : m_sequence(1ull), m_firstSequence(m_sequence), m_slidingBufferSize(1 << bufferSize),
          m_slidingBuffer(m_slidingBufferSize),
          m_checkpointFreq(checkpointFreq),
          m_checkpointCount(m_slidingBufferSize / checkpointFreq),
          m_checkpoints(m_checkpointCount)
      {
      }

      ~CircularBuffer()
      {
        m_checkpoints.clear();
      }

      ObservationPtr getFromBuffer(uint64_t seq) const
      {
        auto off = seq - m_firstSequence;
        if (off < m_slidingBufferSize)
          return m_slidingBuffer[off];
        else
          return ObservationPtr();
      }
      auto getIndexAt(uint64_t at)
      {
        return at - m_firstSequence;
      }

      SequenceNumber_t getSequence() const { return m_sequence; }
      unsigned int getBufferSize() const { return m_slidingBufferSize; }

      SequenceNumber_t getFirstSequence() const
      {
        return m_firstSequence;
      }

      void setSequence(SequenceNumber_t seq)
      {
        m_sequence = seq;
        if (seq > m_slidingBufferSize)
          m_firstSequence = seq - m_slidingBuffer.size();
      }

      SequenceNumber_t addToBuffer(ObservationPtr &event)
      {
        std::lock_guard<std::recursive_mutex> lock(m_sequenceLock);

        auto seq = m_sequence;

        if (m_slidingBuffer.full())
        {
          ObservationPtr old = m_slidingBuffer.front();
          m_first.addObservation(old);
          m_firstSequence++;
        }
        
        event->setSequence(seq);
        m_slidingBuffer.push_back(event);
        m_latest.addObservation(event);

        // Special case for the first event in the series to prime the first checkpoint.
        if (seq == 1)
          m_first.addObservation(event);

        // Checkpoint management
        if (m_checkpointCount > 0 && (seq % m_checkpointFreq) == 0)
        {
          // Copy the checkpoint from the current into the slot
          m_checkpoints.push_back(std::make_unique<Checkpoint>(m_latest));
        }

        // See if the next sequence has an event. If the event exists it
        // should be added to the first checkpoint.
        m_sequence++;
          
        return seq;
      }

      // Checkpoint
      Checkpoint &getLatest() { return m_latest; }
      Checkpoint &getFirst() { return m_first; }
      auto getCheckoointFreq() { return m_checkpointFreq; }
      auto getCheckpointCount() { return m_checkpointCount; }

      std::unique_ptr<Checkpoint> getCheckpointAt(SequenceNumber_t at,
                                                  const FilterSetOpt &filterSet)
      {
        std::lock_guard<std::recursive_mutex> lock(m_sequenceLock);

        // Compute the closest checkpoint. If the checkpoint is after the
        // first checkpoint and before the next incremental checkpoint,
        // use first.
        auto fi = (m_firstSequence / m_checkpointFreq);
        auto in = (at / m_checkpointFreq);
        auto cp = in * m_checkpointFreq;
        int dt = int(in - fi) - 1;
        
        std::unique_ptr<Checkpoint> check;
        SequenceNumber_t ind;

        if (dt < 0)
        {
          ind = 0;
          check = std::make_unique<Checkpoint>(m_first, filterSet);
        }
        else
        {
          ind = cp - m_firstSequence + 1;
          check = std::make_unique<Checkpoint>(*m_checkpoints[dt], filterSet);
        }
                
        // Roll forward from the checkpoint.
        auto limit = at - m_firstSequence;
        for (; ind <= limit; ind++)
        {
          ObservationPtr o = m_slidingBuffer[ind];
          check->addObservation(o);
        }

        return check;
      }

      std::unique_ptr<ObservationList> getObservations(int count, const FilterSetOpt &filterSet,
                                                       const std::optional<SequenceNumber_t> start,
                                                       const std::optional<SequenceNumber_t> to,
                                                       SequenceNumber_t &end,
                                                       SequenceNumber_t &firstSeq,
                                                       bool &endOfBuffer)
      {
        auto results = std::make_unique<ObservationList>();

        std::lock_guard<std::recursive_mutex> lock(m_sequenceLock);
        firstSeq = m_firstSequence;
        int limit, inc;

        SequenceNumber_t first;

        // START SHOULD BE BETWEEN 0 AND SEQUENCE NUMBER
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
        
        SequenceNumber_t sin = first - m_firstSequence;
        SequenceNumber_t ein = firstSeq - m_firstSequence;

        SequenceNumber_t i;
        for (i = sin; int(results->size()) < limit && i < m_slidingBuffer.size() && i >= ein; i += inc)
        {
          // Filter out according to if it exists in the list
          auto &event = m_slidingBuffer[i];
          const std::string &dataId = event->getDataItem()->getId();
          if (!filterSet || filterSet->count(dataId) > 0)
          {
            results->push_back(event);
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
      boost::circular_buffer<ObservationPtr> m_slidingBuffer;

      // Checkpoints
      SequenceNumber_t m_checkpointFreq;
      SequenceNumber_t m_checkpointCount;

      Checkpoint m_latest;
      Checkpoint m_first;
      boost::circular_buffer<std::unique_ptr<Checkpoint>> m_checkpoints;

    };
  }  // namespace observation
}  // namespace mtconnect
