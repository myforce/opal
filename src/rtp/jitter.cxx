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


const unsigned JitterRoundingGuardBits = 4;


#if !PTRACING && !defined(NO_ANALYSER)
#define NO_ANALYSER 1
#endif

#ifdef NO_ANALYSER
  #define ANALYSE(inout, time, extra)
#else
  #define ANALYSE(inout, time, extra) m_analyser->inout(tick, time, m_frames.size(), extra)

  class RTP_JitterBufferAnalyser : public PObject
  {
      PCLASSINFO(RTP_JitterBufferAnalyser, PObject);

    protected:
      enum { TableSize = 4000 };
      struct Info {
        Info() { }
        DWORD         time;
        PTimeInterval tick;
        size_t        depth;
        const char *  extra;
      } in[TableSize], out[TableSize];
      PINDEX inPos, outPos;

    public:
      RTP_JitterBufferAnalyser()
        : inPos(0)
        , outPos(0)
      {
        in[0].time = out[0].time = in[0].depth = out[0].depth = 0;
      }

      void In(const PTimeInterval & tick, DWORD time, size_t depth, const char * extra)
      {
        if (inPos == 0)
          in[inPos++].tick = tick;

        if (inPos < PARRAYSIZE(in)) {
          in[inPos].tick = tick;
          in[inPos].time = time;
          in[inPos].depth = depth;
          in[inPos++].extra = extra;
        }
      }

      void Out(const PTimeInterval & tick, DWORD time, size_t depth, const char * extra)
      {
        if (outPos == 0)
          out[outPos++].tick = tick;

        if (outPos < PARRAYSIZE(out)) {
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
                "InDepth\tOutDep\tInTick\tInDelay\tOutTick\tOutDel\tIODelay\tTotalDelay\n";
        PINDEX ix = 1;
        PINDEX ox = 1;
        while (ix < inPos || ox < outPos) {
          while (ix < inPos && (ox >= outPos || in[ix].time < out[ox].time)) {
            strm << "In\t"
                 << in[ix].time << '\t'
                 << (in[ix].time - in[ix-1].time) << "\t"
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
                 << (out[ox].time - out[ox-1].time) << "\t"
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
                 << (in[ix].time - in[ix-1].time) << '\t'
                 << (out[ox].time - out[ox-1].time) << '\t'
                 << in[ix].extra << '\t'
                 << out[ox].extra << '\t'
                 << in[ix].depth << '\t'
                 << out[ox].depth << '\t'
                 << (in[ix].tick - in[0].tick) << '\t'
                 << (in[ix].tick - in[ix-1].tick) << '\t'
                 << (out[ox].tick - out[0].tick) << '\t'
                 << (out[ox].tick - out[ox-1].tick) << '\t'
                 << (out[ox].tick - in[ix].tick) << '\t'
                 << ((out[ox].tick - in[1].tick) - PTimeInterval((in[ix].time-in[1].time)/8))
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

OpalJitterBuffer::OpalJitterBuffer(unsigned minJitter,
                                   unsigned maxJitter,
                                   unsigned units,
                                     PINDEX packetSize)
  : m_timeUnits(units)
  , m_jitterGrowTime(10*units) // 10 milliseconds @ 8kHz
  , m_jitterShrinkPeriod(2000*units) // 2 seconds @ 8kHz
  , m_jitterShrinkTime(5*units) // 5 milliseconds @ 8kHz
  , m_silenceShrinkPeriod(5000*units) // 5 seconds @ 8kHz
  , m_slenceShrinkTime(20*units) // 20 milliseconds @ 8kHz
  , m_maxConsecutiveMarkerBits(10)
#ifdef NO_ANALYSER
  , m_analyser(NULL)
#else
  , m_analyser(new RTP_JitterBufferAnalyser)
#endif
{
  SetDelay(minJitter, maxJitter, packetSize);

  PTRACE(4, "Jitter\tBuffer created:" << *this);
}


OpalJitterBuffer::~OpalJitterBuffer()
{
#ifndef NO_ANALYSER
  PTRACE(5, "Jitter\tBuffer analysis: " << *this << '\n' << *m_analyser);
  delete m_analyser;
#endif

  PTRACE(4, "Jitter\tBuffer destroyed:" << *this);
}


void OpalJitterBuffer::PrintOn(ostream & strm) const
{
  strm << this
       << " packets=" << m_frames.size()
       << " delay=" << (m_minJitterDelay/m_timeUnits) << '-'
                    << (m_currentJitterDelay/m_timeUnits) << '-'
                    << (m_maxJitterDelay/m_timeUnits) << "ms";
}


void OpalJitterBuffer::SetDelay(unsigned minJitterDelay, unsigned maxJitterDelay, PINDEX packetSize)
{
  m_bufferMutex.Wait();

  m_minJitterDelay     = minJitterDelay;
  m_maxJitterDelay     = maxJitterDelay;
  m_currentJitterDelay = minJitterDelay;
  m_packetSize         = packetSize;

  PTRACE(3, "Jitter\tDelays set to " << *this);

  Reset();

  m_bufferMutex.Signal();
}


void OpalJitterBuffer::Reset()
{
  m_bufferMutex.Wait();

  m_packetsTooLate        = 0;
  m_bufferOverruns        = 0;
  m_consecutiveMarkerBits = 0;

  m_averageFrameTime  = 0;
  m_lastTimestamp     = UINT_MAX;
  m_bufferFilledTime  = 0;
  m_bufferEmptiedTime = 0;
  m_timestampDelta    = 0;

  m_synchronisationState = e_SynchronisationStart;

  m_frames.clear();

  m_bufferMutex.Signal();
}


PBoolean OpalJitterBuffer::WriteData(const RTP_DataFrame & frame, const PTimeInterval & tick)
{
  PWaitAndSignal mutex(m_bufferMutex);

  DWORD timestamp = frame.GetTimestamp();

  /*Check for catastrophic failure, nothing removing packets from buffer! */
  if (m_frames.size() > 100) {
    PTRACE(2, "Jitter\tNothing being removed from buffer, aborting!");
    return false;
  }


  // Add to buffer
  pair<FrameMap::iterator,bool> result = m_frames.insert(FrameMap::value_type(timestamp, frame));
  if (!result.second) {
    PTRACE(2, "Jitter\tAttempt to insert two RTP packets with same timestamp: " << timestamp);
    return false;
  }

  ANALYSE(In, timestamp, m_synchronisationState != e_SynchronisationDone ? "PreBuf" : "");


  /*Deal with naughty systems that send continuous marker bits, thus we
    cannot use it to determine start of talk burst, and the need to refill
    the jitter buffer */
  if (m_consecutiveMarkerBits < m_maxConsecutiveMarkerBits) {
    if (frame.GetMarker()) {
      PTRACE(3, "Jitter\tStart talk burst: ts=" << timestamp);
      if (m_synchronisationState == e_SynchronisationDone)
        m_synchronisationState = e_SynchronisationStart;
      m_consecutiveMarkerBits++;
    }
    else
      m_consecutiveMarkerBits = 0;
  }
  else {
    result.first->second.SetMarker(false);
    if (m_consecutiveMarkerBits == m_maxConsecutiveMarkerBits) {
      PTRACE(2, "Jitter\tEvery packet has Marker bit, ignoring them from this client!");
      m_consecutiveMarkerBits++;
    }
  }


  /*Calculate the time between packets. While not actually dictated by any
    standards, this is invariably a constant. We just need to allow for if we
    are unlucky at the start and get a missing packet, in which case we are
    twice as big (or more) as we should be. The smallest distance between
    consecutive packets will be the frame time interval. */
  if (m_lastTimestamp != UINT_MAX) {
    DWORD newFrameTime = timestamp - m_lastTimestamp;

    /* Check for naughty systems that have discontinuous timestamps, that is
       time has gone backwards or it has been 10 minutes since last packet.
       Not likely! */
    if (newFrameTime > 4800000) {
      PTRACE(3, "Jitter\tTimestamps abruptly changed from "
              << m_lastTimestamp << " to " << timestamp << ", resynching");
      Reset();
      m_frames[timestamp] = frame;
      return true;
    }

    if (m_averageFrameTime == 0 || m_averageFrameTime > newFrameTime) {
      m_averageFrameTime = newFrameTime;
      PTRACE(4, "Jitter\tAverage frame time set to " << newFrameTime);
    }
  }
  m_lastTimestamp = timestamp;

  return true;
}


PBoolean OpalJitterBuffer::ReadData(RTP_DataFrame & frame, const PTimeInterval & tick)
{
  // Default response is an empty frame, ie silence
  frame.SetPayloadSize(0);

  PWaitAndSignal mutex(m_bufferMutex);

  // Now we get the timestamp the caller wants
  DWORD playOutTimestamp = frame.GetTimestamp();
  DWORD requiredTimestamp = playOutTimestamp + m_timestampDelta - m_currentJitterDelay;

  if (m_frames.empty()) {
    /*We ran the buffer down to empty, so have no data to play, play silence.
      THis happens if packet is too late or completely missing. A too late
      packet will be picked up by later code.
     */
    PTRACE_IF(6, m_synchronisationState == e_SynchronisationDone,
              "Jitter\tBuffer is empty : ts=" << requiredTimestamp << " (" << playOutTimestamp << ')');
    ANALYSE(Out, requiredTimestamp, "Empty");

    if (m_currentJitterDelay != m_minJitterDelay &&
              (playOutTimestamp - m_bufferEmptiedTime) > m_silenceShrinkPeriod) {
      m_currentJitterDelay -= m_slenceShrinkTime;
      PTRACE(4, "Jitter\tLong silence    : ts=" << requiredTimestamp << " (" << playOutTimestamp << "),"
                " decreasing delay to " << m_currentJitterDelay);
      m_bufferEmptiedTime = playOutTimestamp;
    }

    return true;
  }
  m_bufferEmptiedTime = playOutTimestamp;

  /* Check for buffer full (or nearly so) and count them. If full for a while
     then it is time to reduce the size of the jitter buffer */
  if (m_averageFrameTime == 0 || m_frames.size() == 1 || m_frames.size() < m_currentJitterDelay/m_averageFrameTime)
    m_bufferFilledTime = playOutTimestamp;
  else if ((playOutTimestamp - m_bufferFilledTime) > m_jitterShrinkPeriod) {
    m_bufferFilledTime = playOutTimestamp;

    if (m_currentJitterDelay <= m_minJitterDelay)
      PTRACE(4, "Jitter\tPackets on time : ts=" << requiredTimestamp << " (" << playOutTimestamp << "),"
                " cannot decrease delay past " << m_currentJitterDelay);
    else {
      m_currentJitterDelay -= m_jitterShrinkTime;
      if (m_currentJitterDelay < m_minJitterDelay)
        m_currentJitterDelay = m_minJitterDelay;
      PTRACE(4, "Jitter\tPackets on time : ts=" << requiredTimestamp << " (" << playOutTimestamp << "),"
                " decreasing delay to " << m_currentJitterDelay);
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
      requiredTimestamp = playOutTimestamp + m_timestampDelta - m_currentJitterDelay;
      m_synchronisationState = e_SynchronisationFill;
      PTRACE(5, "Jitter\tSynchronising   : ts=" << requiredTimestamp << " (" << playOutTimestamp << ')');
      // Do next state

    case e_SynchronisationFill :
      /* Now see if we have buffered enough yet */
      if (requiredTimestamp < oldestFrame->first) {
        /* Nope, play out some silence */
        ANALYSE(Out, oldestFrame->first, "PreBuf");
        return true;
      }

      /* Buffer now full, return the oldest frame. From now on the caller will
         call ReadData() with the next timestamp it wants which will be the
         timstamp of this oldest packet, plus the amount of audio data that
         is in it. */
      m_synchronisationState = e_SynchronisationDone;
      PTRACE(5, "Jitter\tSynchronise done: ts=" << requiredTimestamp << " (" << playOutTimestamp << ')');
      break;

    case e_SynchronisationShrink :
      requiredTimestamp = playOutTimestamp + m_timestampDelta - m_currentJitterDelay;
      if (requiredTimestamp >= oldestFrame->first + m_averageFrameTime) {
        ANALYSE(Out, oldestFrame->first, "Shrink");
        m_frames.erase(oldestFrame);
        ++m_bufferOverruns;
        oldestFrame = m_frames.begin();
        PAssert(oldestFrame != m_frames.end(), PLogicError);
      }

      m_synchronisationState = e_SynchronisationDone;
      break;

    case e_SynchronisationDone :
      // Get rid of all the frames that are too late
      while (requiredTimestamp >= oldestFrame->first + m_averageFrameTime) {
        // Packets late, need a bigger jitter buffer
        m_currentJitterDelay += m_jitterGrowTime;
        if (m_currentJitterDelay > m_maxJitterDelay)
          m_currentJitterDelay = m_maxJitterDelay;

        PTRACE(4, "Jitter\tPacket too late : ts=" << requiredTimestamp << " (" << playOutTimestamp
                  << ") oldest=" << oldestFrame->first << ", "
                  << (m_currentJitterDelay == m_maxJitterDelay
                                           ? "cannot increase delay past " : "increasing delay to ")
                  << m_currentJitterDelay);
        ANALYSE(Out, oldestFrame->first, "Late");
        m_frames.erase(oldestFrame);
        ++m_packetsTooLate;

        if (m_frames.empty()) {
          PTRACE(5, "Jitter\tBuffer emptied  : ts=" << requiredTimestamp << " (" << playOutTimestamp << ')');
          ANALYSE(Out, requiredTimestamp, "Empty");
          return true;
        }

        requiredTimestamp = playOutTimestamp + m_timestampDelta - m_currentJitterDelay;

        oldestFrame = m_frames.begin();
        PAssert(oldestFrame != m_frames.end(), PLogicError);
      }
  }

  /* Now we see if it time for our oldest packet, or that there is a missing
     packet (not arrived yet) in buffer. Can't wait for it, return no data.
     If the packet subsequently DOES arrive, it will get picked up by the
     too late section above. */
  if (requiredTimestamp < oldestFrame->first) {
    PTRACE(4, "Jitter\tPacket not ready: ts=" << requiredTimestamp << " (" << playOutTimestamp
           << ") oldest=" << oldestFrame->first);
    ANALYSE(Out, requiredTimestamp, "Wait");
    return true;
  }

  // Finally can return the frame we have
  ANALYSE(Out, oldestFrame->first, "");
  frame = oldestFrame->second;
  m_frames.erase(oldestFrame);
  frame.SetTimestamp(playOutTimestamp);
  return true;
}


/////////////////////////////////////////////////////////////////////////////

OpalJitterBufferThread::OpalJitterBufferThread(unsigned minJitterDelay,
                                               unsigned maxJitterDelay,
                                               unsigned timeUnits,
                                                 PINDEX packetSize)
  : OpalJitterBuffer(minJitterDelay, maxJitterDelay, timeUnits, packetSize)
  , m_jitterThread(NULL)
  , m_running(true)
{
}


OpalJitterBufferThread::~OpalJitterBufferThread()
{
  WaitForThreadTermination(); // Failsafe, should be called from derived class dtor
}


void OpalJitterBufferThread::StartThread()
{
  m_bufferMutex.Wait();

  if (m_jitterThread == NULL) {
    m_jitterThread = PThread::Create(PCREATE_NOTIFIER(JitterThreadMain), "RTP Jitter");
    m_jitterThread->Resume();
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


PBoolean OpalJitterBufferThread::ReadData(RTP_DataFrame & frame)
{
  if (m_running)
    return OpalJitterBuffer::ReadData(frame);

  PTRACE(3, "Jitter\tShutting down " << *this);
  return false;
}


void OpalJitterBufferThread::JitterThreadMain(PThread &, INT)
{
  PTRACE(4, "Jitter\tReceive thread started: " << *this);

  m_bufferMutex.Wait();

  while (m_running) {
    RTP_DataFrame frame(0, m_packetSize);

    do {
      m_bufferMutex.Signal();

      // Keep reading from the RTP transport frames
      if (!OnReadPacket(frame)) {
        m_running = false; // Flag to stop the reading side thread
        goto exitThread;
      }
    } while (frame.GetSize() == 0);

    m_bufferMutex.Wait();
    WriteData(frame);
  }

  m_bufferMutex.Signal();

exitThread:
  PTRACE(4, "Jitter\tReceive thread finished: " << *this);
}


/////////////////////////////////////////////////////////////////////////////

RTP_JitterBuffer::RTP_JitterBuffer(RTP_Session & sess,
                                        unsigned minJitterDelay,
                                        unsigned maxJitterDelay,
                                        unsigned time,
                                          PINDEX packetSize)
  : OpalJitterBufferThread(minJitterDelay, maxJitterDelay, time, packetSize),
    m_session(sess)
{
  StartThread();
}


RTP_JitterBuffer::~RTP_JitterBuffer()
{
  PTRACE(4, "Jitter\tDestroying jitter buffer " << *this);

  m_running = false;
  bool reopen = m_session.Close(true);

  WaitForThreadTermination();

  if (reopen)
    m_session.Reopen(true);
}


PBoolean RTP_JitterBuffer::OnReadPacket(RTP_DataFrame & frame)
{
  bool success = m_session.ReadData(frame);
  PTRACE(8, "Jitter\tOnReadPacket: Frame from network, timestamp " << frame.GetTimestamp());
  return success;
}


/////////////////////////////////////////////////////////////////////////////
