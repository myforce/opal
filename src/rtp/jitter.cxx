/*
 * jitter.cxx
 *
 * Jitter buffer support
 *
 * Open H323 Library
 *
 * Copyright (c) 1998-2000 Equivalence Pty. Ltd.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Open H323 Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Portions of this code were written with the assisance of funding from
 * Vovida Networks, Inc. http://www.vovida.com.
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "jitter.h"
#endif

#include <opal/buildopts.h>

#include <rtp/jitter.h>

#include <rtp/metrics.h>


const unsigned AverageFrameTimePackets = 4;
const unsigned MaxConsecutiveOverflows = 10;

#define EVERY_PACKET_TRACE_LEVEL 5
#define ANALYSER_TRACE_LEVEL     5


#if !PTRACING && !defined(NO_ANALYSER)
#define NO_ANALYSER 1
#endif

#ifdef NO_ANALYSER
  #define ANALYSE(inout, time, extra)
#else

  #define ANALYSE(inout, time, extra) \
    if (PTrace::CanTrace(ANALYSER_TRACE_LEVEL)) \
      m_analyser->inout(tick, time, m_frames.size(), extra)

  class RTP_JitterBufferAnalyser : public PObject
  {
      PCLASSINFO(RTP_JitterBufferAnalyser, PObject);

    protected:
      struct Info {
        Info() : time(0), depth(0), extra("") { }
        DWORD         time;
        PTimeInterval tick;
        size_t        depth;
        const char *  extra;
      };
      std::vector<Info> in, out;
      std::vector<Info>::size_type inPos, outPos;

    public:
      RTP_JitterBufferAnalyser()
        : inPos(0)
        , outPos(0)
      {
        std::vector<Info>::size_type size;
#if P_CONFIG_FILE
        PConfig env(PConfig::Environment);
        size = env.GetInteger("OPAL_JITTER_ANALYSER_SIZE", 1000);
#else
        size = 1000;
#endif
        if (size > 100000)
          size = 100000;
        in.resize(size);
        out.resize(size);
      }

      void In(PTimeInterval tick, DWORD time, size_t depth, const char * extra)
      {
        if (tick == 0)
          tick = PTimer::Tick();

        if (inPos == 0)
          in[inPos++].tick = tick;

        if (inPos < in.size()) {
          in[inPos].tick = tick;
          in[inPos].time = time;
          in[inPos].depth = depth;
          in[inPos++].extra = extra;
        }
      }

      void Out(PTimeInterval tick, DWORD time, size_t depth, const char * extra)
      {
        if (tick == 0)
          tick = PTimer::Tick();

        if (outPos == 0)
          out[outPos++].tick = tick;

        if (outPos < out.size()) {
          out[outPos].tick = tick;
          if (time == 0 && outPos > 0)
            out[outPos].time = out[outPos-1].time;
          else
            out[outPos].time = time;
          out[outPos].depth = depth;
          out[outPos++].extra = extra;
        }
      }

      void PrintOn(ostream & strm) const
      {
        strm << "Input samples: " << inPos << " Output samples: " << outPos << "\n"
                "Dir\tRTPTime\tInDiff\tOutDiff\tInMode\tOutMode\t"
                "InDepth\tOuDepth\tInTick\tInDelta\tOutTick\tODelta\tIODelay\n";
        std::vector<Info>::size_type ix = 1;
        std::vector<Info>::size_type ox = 1;
        while (ix < inPos || ox < outPos) {
          while (ix < inPos && (ox >= outPos || in[ix].time < out[ox].time)) {
            strm << "In\t"
                 << in[ix].time << '\t'
                 << (int)(in[ix].time - in[ix-1].time) << "\t"
                    "\t"
                 << in[ix].extra << "\t"
                    "\t"
                 << in[ix].depth << "\t"
                    "\t"
                 << (in[ix].tick - in[0].tick) << '\t'
                 << (in[ix].tick - in[ix-1].tick) << "\t"
                    "\t"
                    "\t"
                    "\t"
                    "\n";
            ix++;
          }

          while (ox < outPos && (ix >= inPos || out[ox].time < in[ix].time)) {
            strm << "Out\t"
                 << out[ox].time << "\t"
                    "\t"
                 << (int)(out[ox].time - out[ox-1].time) << "\t"
                    "\t"
                 << out[ox].extra << "\t"
                    "\t"
                 << out[ox].depth << "\t"
                    "\t"
                    "\t"
                 << (out[ox].tick - out[0].tick) << '\t'
                 << (out[ox].tick - out[ox-1].tick) << "\t"
                    "\t"
                    "\n";
            ox++;
          }

          while (ix < inPos && ox < outPos && in[ix].time == out[ox].time) {
            strm << "I/O\t"
                 << in[ix].time << '\t'
                 << (int)(in[ix].time - in[ix-1].time) << '\t'
                 << (int)(out[ox].time - out[ox-1].time) << '\t'
                 << in[ix].extra << '\t'
                 << out[ox].extra << '\t'
                 << in[ix].depth << '\t'
                 << out[ox].depth << '\t'
                 << (in[ix].tick - in[0].tick) << '\t'
                 << (in[ix].tick - in[ix-1].tick) << '\t'
                 << (out[ox].tick - out[0].tick) << '\t'
                 << (out[ox].tick - out[ox-1].tick) << '\t'
                 << (out[ox].tick - in[ix].tick)
                 << '\n';
            ox++;
            ix++;
          }
        }
      }
  };

#endif

#define new PNEW


/////////////////////////////////////////////////////////////////////////////

OpalJitterBuffer::OpalJitterBuffer(const Init & init)
  : m_timeUnits(init.m_timeUnits)
  , m_packetSize(init.m_packetSize)
  , m_minJitterDelay(init.m_minJitterDelay)
  , m_maxJitterDelay(init.m_maxJitterDelay)
  , m_jitterGrowTime(10*init.m_timeUnits) // 10 milliseconds @ 8kHz
  , m_jitterShrinkPeriod(2000*init.m_timeUnits) // 2 seconds @ 8kHz
  , m_jitterShrinkTime(-5*init.m_timeUnits) // 5 milliseconds @ 8kHz
  , m_silenceShrinkPeriod(5000*init.m_timeUnits) // 5 seconds @ 8kHz
  , m_silenceShrinkTime(-20*init.m_timeUnits) // 20 milliseconds @ 8kHz
  , m_jitterDriftPeriod(500*init.m_timeUnits) // 0.5 second @ 8kHz
  , m_currentJitterDelay(init.m_minJitterDelay)
  , m_packetsTooLate(0)
  , m_bufferOverruns(0)
  , m_consecutiveMarkerBits(0)
  , m_maxConsecutiveMarkerBits(10)
  , m_consecutiveLatePackets(0)
  , m_consecutiveOverflows(0)
  , m_lastSyncSource(0)
#ifdef NO_ANALYSER
  , m_analyser(NULL)
#else
  , m_analyser(new RTP_JitterBufferAnalyser)
#endif
{
  Reset();
  PTRACE(4, "Jitter\tBuffer created:" << *this);
}


OpalJitterBuffer::~OpalJitterBuffer()
{
#ifndef NO_ANALYSER
  PTRACE(ANALYSER_TRACE_LEVEL, "Jitter\tBuffer analysis: " << *this << '\n' << *m_analyser);
  delete m_analyser;
#endif

  PTRACE(4, "Jitter\tBuffer destroyed:" << *this);
}


void OpalJitterBuffer::PrintOn(ostream & strm) const
{
  strm << "this=" << (void *)this
       << " packets=" << m_frames.size()
       <<   " rate=" << m_timeUnits << "kHz"
       << " delay=" << (m_minJitterDelay/m_timeUnits) << '-'
                    << (m_currentJitterDelay/m_timeUnits) << '-'
                    << (m_maxJitterDelay/m_timeUnits) << "ms";
}


void OpalJitterBuffer::SetDelay(const Init & init)
{
  m_bufferMutex.Wait();

  m_minJitterDelay     = init.m_minJitterDelay;
  m_maxJitterDelay     = init.m_maxJitterDelay;
  m_currentJitterDelay = init.m_minJitterDelay;
  m_packetSize         = init.m_packetSize;

  PTRACE(3, "Jitter\tDelays set to " << *this);

  Reset();

  m_bufferMutex.Signal();
}


void OpalJitterBuffer::Reset()
{
  m_bufferMutex.Wait();

  m_frameTimeCount    = 0;
  m_frameTimeSum      = 0;
  m_incomingFrameTime = 0;
  m_lastSequenceNum   = UINT_MAX;
  m_lastTimestamp     = UINT_MAX;
  m_bufferFilledTime  = 0;
  m_bufferLowTime     = 0;
  m_bufferEmptiedTime = 0;

  m_consecutiveLatePackets = 0;
  m_consecutiveOverflows   = 0;

  m_synchronisationState = e_SynchronisationStart;

  m_frames.clear();

  m_bufferMutex.Signal();
}


PBoolean OpalJitterBuffer::WriteData(const RTP_DataFrame & frame, const PTimeInterval & PTRACE_PARAM(tick))
{
  if (frame.GetSize() < RTP_DataFrame::MinHeaderSize) {
    PTRACE(2, "Jitter\tWriting invalid RTP data frame.");
    return true; // Don't abort, but ignore
  }

  PWaitAndSignal mutex(m_bufferMutex);

  DWORD timestamp = frame.GetTimestamp();
  unsigned currentSequenceNum = frame.GetSequenceNumber();
  DWORD newSyncSource = frame.GetSyncSource();

  // Check for remote switching media senders, they shouldn't do this but do anyway
  if (newSyncSource != m_lastSyncSource) {
    PTRACE_IF(4, m_lastSyncSource != 0, "Jitter\tBuffer reset due to SSRC change from 0x"
              << hex << m_lastSyncSource << " to 0x" << newSyncSource << " at sn=" << dec << currentSequenceNum);
    Reset();
    m_lastSyncSource = newSyncSource;
  }


  /*Deal with naughty systems that send continuous marker bits, thus we
    cannot use it to determine start of talk burst, and the need to refill
    the jitter buffer */
  if (m_consecutiveMarkerBits < m_maxConsecutiveMarkerBits) {
    if (frame.GetMarker()) {
      m_consecutiveMarkerBits++;
      Reset();

      // Have been told there is explicit silence by marker, take opportunity
      // to reduce the current jitter delay.
      AdjustCurrentJitterDelay(m_silenceShrinkTime);
      PTRACE(3, "Jitter\tStart talk burst: ts=" << timestamp << ", decreasing delay="
             << m_currentJitterDelay << " (" << (m_currentJitterDelay/m_timeUnits) << "ms)");
    }
    else
      m_consecutiveMarkerBits = 0;
  }
  else {
    if (m_consecutiveMarkerBits == m_maxConsecutiveMarkerBits) {
      PTRACE(2, "Jitter\tEvery packet has Marker bit, ignoring them from this client!");
      m_consecutiveMarkerBits++;
    }
  }


  /*Calculate the time between packets. While not actually dictated by any
    standards, this is invariably a constant. We just need to allow for if we
    are unlucky at the start and get a missing packet, in which case we are
    twice as big (or more) as we should be. Se we make sure we do not have
    a missing packet by inspecting sequence numbers. */
  if (m_lastSequenceNum != UINT_MAX) {
    if (timestamp < m_lastTimestamp) {
      PTRACE(3, "Jitter\tTimestamps abruptly changed from "
              << m_lastTimestamp << " to " << timestamp << ", resynching");
      Reset();
    }
    else if (m_lastSequenceNum+1 == currentSequenceNum) {
      DWORD delta = timestamp - m_lastTimestamp;
      PTRACE_IF(5, m_incomingFrameTime == 0,
                "Jitter\tWait frame time : ts=" << timestamp << ","
                " delta=" << delta << " (" << (delta/m_timeUnits) << "ms),"
                  " sn=" << currentSequenceNum);

      /* Average the deltas. Most systems are "normalised" and the timestamp
          goes up by a constant on consecutive packets. Not not all. */
      m_frameTimeSum += delta;
      if (++m_frameTimeCount > AverageFrameTimePackets) {
        int newFrameTime = (int)(m_frameTimeSum/m_frameTimeCount);
        m_frameTimeSum = 0;
        m_frameTimeCount = 0;

        // If new average changed by more than millisecond, start using it.
        if (std::abs(newFrameTime - (int)m_incomingFrameTime) > (int)m_timeUnits) {
          m_incomingFrameTime = newFrameTime;
          AdjustCurrentJitterDelay(0);
          PTRACE(4, "Jitter\tFrame time set  : ts=" << timestamp << ", size=" << m_frames.size() << ","
                    " time=" << newFrameTime << " (" << (newFrameTime/m_timeUnits) << "ms),"
                    " delay=" << m_currentJitterDelay << " (" << (m_currentJitterDelay/m_timeUnits) << "ms)");
        }
      }
    }
    else {
      PTRACE(5, "Jitter\tLost packet(s), resetting frame time average, sn=" << currentSequenceNum);
      m_frameTimeSum = 0;
      m_frameTimeCount = 0;
    }
  }
  m_lastSequenceNum = currentSequenceNum;
  m_lastTimestamp = timestamp;


  /* Fail safe for infinite queueing, for example, if other thread is not
     taking stuff out.  Also checks for abrupt changes in timestamp values, can
     happen when remote is swapping media sources */
  FrameMap::iterator oldestFrame = m_frames.begin();
  if (oldestFrame != m_frames.end()) {
    DWORD delta = timestamp - oldestFrame->second.GetTimestamp();
    if (delta < m_maxJitterDelay*2)
      m_consecutiveOverflows = 0;
    else {
      ANALYSE(In, timestamp, "Overflow");
      PTRACE(4, "Jitter\tBuffer overflow : ts=" << timestamp << ", delta=" << delta << ", size=" << m_frames.size());
      if (++m_consecutiveOverflows > (m_incomingFrameTime == 0 ? AverageFrameTimePackets : MaxConsecutiveOverflows)) {
        PTRACE(3, "Jitter\tConsecutive overlow packets, resynching");
        Reset();
      }
      return true;
    }
  }


  // Add to buffer
  pair<FrameMap::iterator,bool> result = m_frames.insert(FrameMap::value_type(timestamp, frame));
  if (result.second) {
    ANALYSE(In, timestamp, m_synchronisationState != e_SynchronisationDone ? "PreBuf" : "");
    PTRACE(EVERY_PACKET_TRACE_LEVEL, "Jitter\tInserted packet : ts=" << timestamp << " sz=" << frame.GetPayloadSize());
  }
  else {
    PTRACE(2, "Jitter\tAttempt to insert two RTP packets with same timestamp: " << timestamp);
  }

  return true;
}


DWORD OpalJitterBuffer::CalculateRequiredTimestamp(DWORD playOutTimestamp) const
{
  DWORD timestamp = playOutTimestamp + m_timestampDelta;
  return timestamp > (DWORD)m_currentJitterDelay ? (timestamp - m_currentJitterDelay) : 0;
}


bool OpalJitterBuffer::AdjustCurrentJitterDelay(int delta)
{
  int minJitterDelay = max(m_minJitterDelay, 2*m_incomingFrameTime);
  int maxJitterDelay = max(m_minJitterDelay, m_maxJitterDelay);

  if (delta < 0 && m_currentJitterDelay <= minJitterDelay)
    return false;
  if (delta > 0 && m_currentJitterDelay >= maxJitterDelay)
    return false;

  m_currentJitterDelay += delta;
  if (m_currentJitterDelay < minJitterDelay)
    m_currentJitterDelay = minJitterDelay;
  else if (m_currentJitterDelay > maxJitterDelay)
    m_currentJitterDelay = maxJitterDelay;

  return true;
}


#define COMMON_TRACE_INFO ": ts=" << requiredTimestamp << " (" << playOutTimestamp << "), size=" << m_frames.size()

PBoolean OpalJitterBuffer::ReadData(RTP_DataFrame & frame, const PTimeInterval & PTRACE_PARAM(tick))
{
  // Default response is an empty frame, ie silence
  frame.SetPayloadSize(0);

  PWaitAndSignal mutex(m_bufferMutex);

  // Make sure the input side to jitter buffer has been started
  Start();

  // Now we get the timestamp the caller wants
  DWORD playOutTimestamp = frame.GetTimestamp();
  DWORD requiredTimestamp = CalculateRequiredTimestamp(playOutTimestamp);

  if (m_frames.empty()) {
    /*We ran the buffer down to empty, so have no data to play, play silence.
      This happens if packet is too late or completely missing. A too late
      packet will be picked up by later code.
     */
    ANALYSE(Out, requiredTimestamp, "Empty");

    if ((playOutTimestamp - m_bufferEmptiedTime) > m_silenceShrinkPeriod &&
         AdjustCurrentJitterDelay(m_silenceShrinkTime)) {
      PTRACE(4, "Jitter\tLong silence    " COMMON_TRACE_INFO << ", decreasing delay="
             << m_currentJitterDelay << " (" << (m_currentJitterDelay/m_timeUnits) << "ms)");
      m_bufferEmptiedTime = playOutTimestamp;
    }

    PTRACE(m_synchronisationState == e_SynchronisationDone ? 4 : EVERY_PACKET_TRACE_LEVEL,
           "Jitter\tBuffer is empty " COMMON_TRACE_INFO);
    return true;
  }
  m_bufferEmptiedTime = playOutTimestamp;

  size_t framesInBuffer = m_frames.size(); // Disable Clock overrun check if no m_incomingFrameTime yet
  if (m_incomingFrameTime == 0)
    m_synchronisationState = e_SynchronisationStart; // Can't start until we have an average packet time
  else {
    framesInBuffer = m_currentJitterDelay/m_incomingFrameTime;
    if (framesInBuffer < 2)
      framesInBuffer = 2;

    /* Check for buffer low (one packet) for prologed period, then that generally
       means we have a sample clock drift problem, that is we have a clock of 8.01kHz and
       the remote has 7.99kHz so gradually the buffer drains as we take things out faster
       than they arrive. */
    if (m_bufferLowTime == 0 || m_frames.size() > 1)
      m_bufferLowTime = playOutTimestamp;
    else if ((playOutTimestamp - m_bufferLowTime) > m_jitterDriftPeriod) {
      m_bufferLowTime = playOutTimestamp;
      PTRACE(4, "Jitter\tClock underrun  " COMMON_TRACE_INFO << " <= " << framesInBuffer << "/2");
      m_timestampDelta -= m_incomingFrameTime;
      ANALYSE(Out, requiredTimestamp, "Drift");
      return true;
    }

    /* Check for buffer full (or nearly so) and count them. If full for a while
       then it is time to reduce the size of the jitter buffer */
    if (m_bufferFilledTime == 0 || m_frames.size() < framesInBuffer-1)
      m_bufferFilledTime = playOutTimestamp;
    else if ((playOutTimestamp - m_bufferFilledTime) > m_jitterShrinkPeriod) {
      m_bufferFilledTime = playOutTimestamp;

      bool adjusted = AdjustCurrentJitterDelay(m_jitterShrinkTime);
      PTRACE(4, "Jitter\tPackets on time " COMMON_TRACE_INFO << ", "
             << (adjusted ? "decreasing" : "cannot decrease") << " delay="
             << m_currentJitterDelay << " (" << (m_currentJitterDelay/m_timeUnits) << "ms)");
      if (adjusted && m_frames.size() > 1)
        m_synchronisationState = e_SynchronisationShrink;
    }
  }

  // Get the oldest packet
  FrameMap::iterator oldestFrame = m_frames.begin();
  PAssert(oldestFrame != m_frames.end(), PLogicError);

  // Check current buffer state and act accordingly
  switch (m_synchronisationState) {
    case e_SynchronisationStart :
      /* First packet of talk burst, re-calculate the timestamp delta */
      m_timestampDelta = oldestFrame->first - playOutTimestamp;
      requiredTimestamp = CalculateRequiredTimestamp(playOutTimestamp);
      m_synchronisationState = e_SynchronisationFill;
      PTRACE(5, "Jitter\tSynchronising   " COMMON_TRACE_INFO << ", oldest=" << oldestFrame->first);
      ANALYSE(Out, oldestFrame->first, "PreBuf");
      return true;

    case e_SynchronisationFill :
      /* Now see if we have buffered enough yet */
      if (requiredTimestamp < oldestFrame->first) {
        PTRACE(EVERY_PACKET_TRACE_LEVEL, "Jitter\tPre-buffering   " COMMON_TRACE_INFO << ", oldest=" << oldestFrame->first);
        /* Nope, play out some silence */
        ANALYSE(Out, oldestFrame->first, "PreBuf");
        return true;
      }

      /* Buffer now full, return the oldest frame. From now on the caller will
         call ReadData() with the next timestamp it wants which will be the
         timstamp of this oldest packet, plus the amount of audio data that
         is in it. */
      m_synchronisationState = e_SynchronisationDone;
      PTRACE(5, "Jitter\tSynchronise done" COMMON_TRACE_INFO << ", delay="
             << m_currentJitterDelay << " (" << (m_currentJitterDelay/m_timeUnits) << "ms)");
      break;

    case e_SynchronisationDone :
      // Get rid of all the frames that are too late
      while (requiredTimestamp >= oldestFrame->first + m_incomingFrameTime) {
        if (++m_consecutiveLatePackets > 10) {
          PTRACE(3, "Jitter\tToo many late   " COMMON_TRACE_INFO);
          Reset();
          return true;
        }

        // Packets late, need a bigger jitter buffer
        PTRACE_PARAM(bool adjusted =) AdjustCurrentJitterDelay(m_jitterGrowTime);
        PTRACE(4, "Jitter\tPacket too late " COMMON_TRACE_INFO
                  << ", oldest=" << oldestFrame->first << ", "
                  << (adjusted ? "increasing" : "cannot increase") << " delay="
                  << m_currentJitterDelay << " (" << (m_currentJitterDelay/m_timeUnits) << "ms)");
        ANALYSE(Out, oldestFrame->first, "Late");
        m_frames.erase(oldestFrame);
        ++m_packetsTooLate;

        if (m_frames.empty()) {
          PTRACE(5, "Jitter\tBuffer emptied  " COMMON_TRACE_INFO);
          ANALYSE(Out, requiredTimestamp, "Emptied");
          return true;
        }

        requiredTimestamp = CalculateRequiredTimestamp(playOutTimestamp);

        oldestFrame = m_frames.begin();
        PAssert(oldestFrame != m_frames.end(), PLogicError);
      }

      /* Check for buffer overfull due to clock mismatch. It is possible for the remote
         to have a clock of 8.01kHz and the receiver 7.99kHz so gradually the remote
         sends more data than we take out over time, gradually building up in the
         jitter buffer. So, drop a frame every now and then. */
      if (m_frames.size() <= framesInBuffer*2)
        break;

      PTRACE(4, "Jitter\tClock overrun   " COMMON_TRACE_INFO << " greater than " << framesInBuffer << "*2");
      m_timestampDelta += m_incomingFrameTime;
      // Do next case

    case e_SynchronisationShrink :
      requiredTimestamp = CalculateRequiredTimestamp(playOutTimestamp);
      if (requiredTimestamp >= oldestFrame->first + m_incomingFrameTime) {
        ANALYSE(Out, oldestFrame->first, "Shrink");
        m_frames.erase(oldestFrame);
        ++m_bufferOverruns;
        oldestFrame = m_frames.begin();
        PAssert(oldestFrame != m_frames.end(), PLogicError);
      }

      m_synchronisationState = e_SynchronisationDone;
      break;
  }

  /* Now we see if it time for our oldest packet, or that there is a missing
     packet (not arrived yet) in buffer. Can't wait for it, return no data.
     If the packet subsequently DOES arrive, it will get picked up by the
     too late section above. */
  if (requiredTimestamp < oldestFrame->first) {
    if (oldestFrame->first - requiredTimestamp > m_timeUnits*1000) {
      PTRACE(3, "Jitter\tToo far in ahead" COMMON_TRACE_INFO);
      Reset();
    }
    else {
      PTRACE(5, "Jitter\tPacket not ready" COMMON_TRACE_INFO << ", oldest=" << oldestFrame->first);
      ANALYSE(Out, requiredTimestamp, "Wait");
    }
    return true;
  }

  // Finally can return the frame we have
  ANALYSE(Out, oldestFrame->first, "");
  frame = oldestFrame->second;
  PTRACE(EVERY_PACKET_TRACE_LEVEL, "Jitter\tDelivered packet" COMMON_TRACE_INFO << ", payload=" << frame.GetPayloadSize());
  m_frames.erase(oldestFrame);
  frame.SetTimestamp(playOutTimestamp);
  m_consecutiveLatePackets = 0;
  return true;
}


/////////////////////////////////////////////////////////////////////////////

OpalJitterBufferThread::OpalJitterBufferThread(const Init & init)
  : OpalJitterBuffer(init)
  , m_jitterThread(NULL)
  , m_running(true)
{
}


OpalJitterBufferThread::~OpalJitterBufferThread()
{
  WaitForThreadTermination(); // Failsafe, should be called from derived class dtor
}


void OpalJitterBufferThread::Start()
{
  m_bufferMutex.Wait();

  if (m_jitterThread == NULL) {
    m_jitterThread = PThread::Create(PCREATE_NOTIFIER(JitterThreadMain), "RTP Jitter");
    PTRACE_CONTEXT_ID_TO(m_jitterThread);
  }

  m_bufferMutex.Signal();
}


void OpalJitterBufferThread::WaitForThreadTermination()
{
  m_running = false;

  m_bufferMutex.Wait();
  PThread * jitterThread = m_jitterThread;
  m_jitterThread = NULL;
  m_bufferMutex.Signal();

  if (jitterThread != NULL) {
    PTRACE(3, "Jitter\tWaiting for thread " << jitterThread->GetThreadName() << " on jitter buffer " << *this);
    PAssert(jitterThread->WaitForTermination(10000), "Jitter buffer thread did not terminate");
    delete jitterThread;
  } 
}


PBoolean OpalJitterBufferThread::ReadData(RTP_DataFrame & frame, const PTimeInterval & tick)
{
  if (m_running)
    return OpalJitterBuffer::ReadData(frame, tick);

  PTRACE(3, "Jitter\tShutting down " << *this);
  return false;
}


void OpalJitterBufferThread::JitterThreadMain(PThread &, P_INT_PTR)
{
  PTRACE(4, "Jitter\tReceive thread started: " << *this);

  while (m_running) {
    RTP_DataFrame frame(m_packetSize);

    // Keep reading from the RTP transport frames
    if (!OnReadPacket(frame) || !WriteData(frame))
      m_running = false;
  }

  PTRACE(4, "Jitter\tReceive thread finished: " << *this);
}


/////////////////////////////////////////////////////////////////////////////
