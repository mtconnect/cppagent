//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
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
#include "globals.hpp"

#include <dlib/sliding_buffer.h>

#include <memory>
#include <mutex>

namespace mtconnect
{
  using SequenceNumber_t = uint64_t;

  class CircularBuffer
  {
  public:
    CircularBuffer(unsigned int bufferSize, int checkpointFreq)
      : m_sequence(1ull), m_checkpointFreq(checkpointFreq)
    {
      // Sequence number and sliding buffer for data
      m_slidingBufferSize = 1 << bufferSize;
      m_slidingBuffer = std::make_unique<dlib::sliding_buffer_kernel_1<ObservationPtr>>();
      m_slidingBuffer->set_size(bufferSize);
      m_checkpointCount = (m_slidingBufferSize / m_checkpointFreq) + 1;

      // Create the checkpoints at a regular frequency
      m_checkpoints.reserve(m_checkpointCount);
      for (auto i = 0; i < m_checkpointCount; i++)
        m_checkpoints.emplace_back();
    }

    ~CircularBuffer()
    {
      m_slidingBuffer.reset();
      m_checkpoints.clear();
    }

    Observation *getFromBuffer(uint64_t seq) const { return (*m_slidingBuffer)[seq]; }
    auto getIndexAt(uint64_t at) { return m_slidingBuffer->get_element_id(at); }

    SequenceNumber_t getSequence() const { return m_sequence; }
    unsigned int getBufferSize() const { return m_slidingBufferSize; }

    SequenceNumber_t getFirstSequence() const
    {
      if (m_sequence > m_slidingBufferSize)
        return m_sequence - m_slidingBufferSize;
      else
        return 1ull;
    }

    void setSequence(SequenceNumber_t seq) { m_sequence = seq; }

    SequenceNumber_t addToBuffer(Observation *event)
    {
      std::lock_guard<std::recursive_mutex> lock(m_sequenceLock);

      auto seq = m_sequence;

      event->setSequence(seq);

      (*m_slidingBuffer)[seq] = event;
      m_latest.addObservation(event);

      // Special case for the first event in the series to prime the first checkpoint.
      if (seq == 1)
        m_first.addObservation(event);

      // Checkpoint management
      const auto index = m_slidingBuffer->get_element_id(seq);
      if (m_checkpointCount > 0 && !(index % m_checkpointFreq))
      {
        // Copy the checkpoint from the current into the slot
        m_checkpoints[index / m_checkpointFreq].copy(m_latest);
      }

      // See if the next sequence has an event. If the event exists it
      // should be added to the first checkpoint.
      m_sequence++;
      if ((*m_slidingBuffer)[m_sequence])
      {
        // Keep the last checkpoint up to date with the last.
        m_first.addObservation((*m_slidingBuffer)[m_sequence]);
      }

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

      auto firstSeq = getFirstSequence();
      auto pos = m_slidingBuffer->get_element_id(at);
      auto first = m_slidingBuffer->get_element_id(firstSeq);
      auto checkIndex = pos / m_checkpointFreq;
      auto closestCp = checkIndex * m_checkpointFreq;
      unsigned long index;

      Checkpoint *ref(nullptr);

      // Compute the closest checkpoint. If the checkpoint is after the
      // first checkpoint and before the next incremental checkpoint,
      // use first.
      if (first > closestCp && pos >= first)
      {
        ref = &m_first;
        // The checkpoint is inclusive of the "first" event. So we add one
        // so we don't duplicate effort.
        index = first + 1;
      }
      else
      {
        index = closestCp + 1;
        ref = &m_checkpoints[checkIndex];
      }

      auto check = std::make_unique<Checkpoint>(*ref, filterSet);

      // Roll forward from the checkpoint.
      for (; index <= pos; index++)
      {
        Observation *o = (*m_slidingBuffer)[index];
        check->addObservation(o);
      }

      return check;
    }

    std::unique_ptr<ObservationPtrArray> getObservations(
        int count, const FilterSetOpt &filterSet, const std::optional<SequenceNumber_t> start,
        SequenceNumber_t &end, SequenceNumber_t &firstSeq, bool &endOfBuffer)
    {
      auto results = std::make_unique<ObservationPtrArray>();

      std::lock_guard<std::recursive_mutex> lock(m_sequenceLock);
      firstSeq = (m_sequence > m_slidingBufferSize) ? m_sequence - m_slidingBufferSize : 1;
      int limit, inc;

      SequenceNumber_t first;

      // START SHOULD BE BETWEEN 0 AND SEQUENCE NUMBER
      if (count >= 0)
      {
        first = (!start || *start <= firstSeq) ? firstSeq : *start;
        limit = count;
        inc = 1;
      }
      else
      {
        first = (!start || *start >= m_sequence) ? m_sequence - 1 : *start;
        limit = -count;
        inc = -1;
      }

      SequenceNumber_t i;
      for (i = first; int(results->size()) < limit && i < m_sequence && i >= firstSeq; i += inc)
      {
        // Filter out according to if it exists in the list
        const std::string &dataId = (*m_slidingBuffer)[i]->getDataItem()->getId();
        if (!filterSet || filterSet->count(dataId) > 0)
        {
          ObservationPtr event = (*m_slidingBuffer)[i];
          results->push_back(event);
        }
      }

      end = i;

      if (count >= 0)
        endOfBuffer = i >= m_sequence;
      else
        endOfBuffer = i <= firstSeq;

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

    // The sliding/circular buffer to hold all of the events/sample data
    std::unique_ptr<dlib::sliding_buffer_kernel_1<ObservationPtr>> m_slidingBuffer;
    unsigned int m_slidingBufferSize;

    // Checkpoints
    Checkpoint m_latest;
    Checkpoint m_first;
    std::vector<Checkpoint> m_checkpoints;

    int m_checkpointFreq;
    long long m_checkpointCount;
  };
}  // namespace mtconnect
