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
 * $Log: jitter.cxx,v $
 * Revision 1.2005  2002/09/04 06:01:49  robertj
 * Updated to OpenH323 v1.9.6
 *
 * Revision 2.3  2002/04/15 08:52:20  robertj
 * Added buffer reset on excess buffer overruns.
 *
 * Revision 2.2  2002/01/22 05:21:10  robertj
 * Update from OpenH323, rev 1.27
 *
 * Revision 2.1  2001/10/05 00:22:14  robertj
 * Updated to PWLib 1.2.0 and OpenH323 1.7.0
 *
 * Revision 2.0  2001/07/27 15:48:25  robertj
 * Conversion of OpenH323 to Open Phone Abstraction Library (OPAL)
 *
 * Revision 1.29  2002/09/03 07:31:35  robertj
 * Added buffer reset on excess buffer overruns.
 *
 * Revision 1.28  2002/08/05 10:03:47  robertj
 * Cosmetic changes to normalise the usage of pragma interface/implementation.
 *
 * Revision 1.27  2002/01/17 07:01:28  robertj
 * Fixed jitter buffer failing to deliver a talk burst shorted than the size of
 *    the buffer, this is particularly noticable with RFC2833.
 *
 * Revision 1.26  2001/09/11 00:21:23  robertj
 * Fixed missing stack sizes in endpoint for cleaner thread and jitter thread.
 *
 * Revision 1.25  2001/09/10 08:18:11  robertj
 * Added define to remove jitter buffer analyser, thanks Nick Hoath.
 *
 * Revision 1.24  2001/07/05 05:55:17  robertj
 * Added thread name.
 *
 * Revision 1.23  2001/04/04 03:13:31  robertj
 * Added PTRACE for when have packets too late.
 *
 * Revision 1.22  2001/02/09 05:13:56  craigs
 * Added pragma implementation to (hopefully) reduce the executable image size
 * under Linux
 *
 * Revision 1.21  2000/12/17 22:45:36  robertj
 * Set media stream threads to highest unprivileged priority.
 *
 * Revision 1.20  2000/09/14 23:03:45  robertj
 * Increased timeout on asserting because of driver lockup
 *
 * Revision 1.19  2000/09/05 22:21:02  robertj
 * Fixed bug in jitter buffer list changes if get out of order packet within jitter time.
 *
 * Revision 1.18  2000/08/25 01:10:28  robertj
 * Added assert if various thrads ever fail to terminate.
 *
 * Revision 1.17  2000/05/30 06:53:04  robertj
 * Fixed bug where jitter buffer needs to be restarted, eg Cisco double use of session.
 *
 * Revision 1.16  2000/05/25 02:26:12  robertj
 * Added ignore of marker bits on broken clients that sets it on every RTP packet.
 *
 * Revision 1.15  2000/05/25 00:35:37  robertj
 * Fixed rare crashing bug in jitter buffer caused by our of order packet.
 *
 * Revision 1.14  2000/05/16 07:37:41  robertj
 * Initialised preBuffering flag just in case sender does not set RTP marker bit.
 *
 * Revision 1.13  2000/05/08 14:05:58  robertj
 * Increased log level of big jitter debug output to level 5.
 *
 * Revision 1.12  2000/05/05 02:54:15  robertj
 * Fixed memory leak just introduced in jitter buffer.
 *
 * Revision 1.11  2000/05/04 11:52:35  robertj
 * Added Packets Too Late statistics, requiring major rearrangement of jitter
 *    buffer code, not also changes semantics of codec Write() function slightly.
 *
 * Revision 1.10  2000/05/02 04:32:27  robertj
 * Fixed copyright notice comment.
 *
 * Revision 1.9  2000/05/01 09:11:04  robertj
 * Fixed removal of analysis code on No Trace version.
 *
 * Revision 1.8  2000/05/01 06:04:33  robertj
 * More jitter buffer debugging.
 *
 * Revision 1.7  2000/04/10 18:55:46  robertj
 * Changed RTP data receive tp be more forgiving, will process packet even if payload type is wrong.
 *
 * Revision 1.6  2000/03/31 20:10:43  robertj
 * Fixed problem with insufficient jitter buffer frames being allocated.
 * Fixed "center in frame" change in previous version.
 *
 * Revision 1.5  2000/03/23 03:08:52  robertj
 * Changed jitter buffer so asumes output double buffering and centers output in time frames.
 *
 * Revision 1.4  2000/03/21 03:06:50  robertj
 * Changes to make RTP TX of exact numbers of frames in some codecs.
 *
 * Revision 1.3  2000/03/20 20:51:47  robertj
 * Fixed possible buffer overrun problem in RTP_DataFrames
 *
 * Revision 1.2  1999/12/29 01:18:07  craigs
 * Fixed problem with codecs other than G.711 not working after reorganisation
 *
 * Revision 1.1  1999/12/23 23:02:36  robertj
 * File reorganision for separating RTP from H.323 and creation of LID for VPB support.
 *
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "jitter.h"
#endif

#include <rtp/jitter.h>


#define MAX_BUFFER_OVERRUNS 20


#if PTRACING && !defined(NO_ANALYSER)

class RTP_JitterBufferAnalyser : public PObject
{
    PCLASSINFO(RTP_JitterBufferAnalyser, PObject);
  public:
    RTP_JitterBufferAnalyser();
    void In(DWORD time, unsigned depth, const char * extra);
    void Out(DWORD time, unsigned depth, const char * extra);
    void PrintOn(ostream & strm) const;

    struct Info {
      Info() { }
      DWORD         time;
      PTimeInterval tick;
      int           depth;
      const char *  extra;
    } in[1000], out[1000];
    PINDEX inPos, outPos;
};

#endif


#define new PNEW


/////////////////////////////////////////////////////////////////////////////

RTP_JitterBuffer::RTP_JitterBuffer(RTP_Session & sess,
                                   unsigned jitterDelay,
                                   PINDEX stackSize)
  : PThread(stackSize, NoAutoDeleteThread, HighestPriority, "RTP Jitter:%x"),
    session(sess)
{
  // Jitter buffer is a queue of frames waiting for playback, a list of
  // free frames, and a couple of place holders for the frame that is
  // currently beeing read from the RTP transport or written to the codec.

  oldestFrame = newestFrame = currentWriteFrame = NULL;

  // Calculate the maximum amount of timestamp units for the jitter buffer
  maxJitterTime = jitterDelay;

  // Calculate number of frames to allocate, we make the assumption that the
  // smallest packet we can possibly get is 5ms long (assuming audio 8kHz unit).
  bufferSize = jitterDelay/40+1;

  // Nothing in the buffer so far
  currentDepth = 0;
  packetsTooLate = 0;
  bufferOverruns = 0;
  consecutiveBufferOverruns = 0;
  maxConsecutiveMarkerBits = 10;
  consecutiveMarkerBits = 0;
  shuttingDown = FALSE;
  preBuffering = TRUE;

  // Allocate the frames and put them all into the free list
  freeFrames = new Entry;
  freeFrames->next = freeFrames->prev = NULL;

  for (PINDEX i = 0; i < bufferSize; i++) {
    Entry * frame = new Entry;
    frame->prev = NULL;
    frame->next = freeFrames;
    freeFrames->prev = frame;
    freeFrames = frame;
  }

  PTRACE(2, "RTP\tJitter buffer created:"
            " size=" << bufferSize <<
            " delay=" << maxJitterTime);

#if PTRACING && !defined(NO_ANALYSER)
  analyser = new RTP_JitterBufferAnalyser;
#else
  analyser = NULL;
#endif

  // Start reading data from RTP session
  Resume();
}


RTP_JitterBuffer::~RTP_JitterBuffer()
{
  PTRACE(3, "RTP\tRemoving jitter buffer");

  shuttingDown = TRUE;
  PAssert(WaitForTermination(10000), "Jitter buffer thread did not terminate");

  bufferMutex.Wait();

  // Free up all the memory allocated
  while (oldestFrame != NULL) {
    Entry * frame = oldestFrame;
    oldestFrame = oldestFrame->next;
    delete frame;
  }

  while (freeFrames != NULL) {
    Entry * frame = freeFrames;
    freeFrames = freeFrames->next;
    delete frame;
  }

  delete currentWriteFrame;

  bufferMutex.Signal();

#if PTRACING && !defined(NO_ANALYSER)
  PTRACE(5, "Jitter buffer analysis: size=" << bufferSize
         << " time=" << maxJitterTime << '\n' << *analyser);
  delete analyser;
#endif
}


void RTP_JitterBuffer::SetDelay( unsigned delay)
{
  bufferMutex.Wait();

  if (shuttingDown)
    PAssert(WaitForTermination(10000), "Jitter buffer thread did not terminate");

  maxJitterTime = delay;

  PINDEX newBufferSize = maxJitterTime/40+1;
  while (bufferSize < newBufferSize) {
    Entry * frame = new Entry;
    frame->prev = NULL;
    frame->next = freeFrames;
    freeFrames->prev = frame;
    freeFrames = frame;
    bufferSize++;
  }

  if (IsTerminated()) {
    packetsTooLate = 0;
    bufferOverruns = 0;
    consecutiveBufferOverruns = 0;
    consecutiveMarkerBits = 0;
    shuttingDown = FALSE;
    preBuffering = TRUE;

    PTRACE(2, "RTP\tJitter buffer restarted:"
              " size=" << bufferSize <<
              " delay=" << maxJitterTime);
    Restart();
  }

  bufferMutex.Signal();
}


void RTP_JitterBuffer::Main()
{
  PTRACE(3, "RTP\tJitter RTP receive thread started");

  bufferMutex.Wait();

  for (;;) {

    // Get the next free frame available for use for reading from the RTP
    // transport. Place it into a parking spot.
    Entry * currentReadFrame;
    if (freeFrames != NULL) {
      // Take the next free frame and make it the current for reading
      currentReadFrame = freeFrames;
      freeFrames = freeFrames->next;
      if (freeFrames != NULL)
        freeFrames->prev = NULL;
      PTRACE_IF(2, consecutiveBufferOverruns > 1,
                "RTP\tJitter buffer full, threw away "
                << consecutiveBufferOverruns << " oldest frames");
      consecutiveBufferOverruns = 0;
    }
    else {
      // We have a full jitter buffer, need a new frame so take the oldest one
      currentReadFrame = oldestFrame;
      oldestFrame = oldestFrame->next;
      if (oldestFrame != NULL)
        oldestFrame->prev = NULL;
      currentDepth--;
      bufferOverruns++;
      consecutiveBufferOverruns++;
      if (consecutiveBufferOverruns > MAX_BUFFER_OVERRUNS) {
        PTRACE(2, "RTP\tJitter buffer continuously full, throwing away entire buffer.");
        freeFrames = oldestFrame;
        oldestFrame = newestFrame = NULL;
        preBuffering = TRUE;
      }
      else {
        PTRACE_IF(2, consecutiveBufferOverruns == 1,
                  "RTP\tJitter buffer full, throwing away oldest frame ("
                  << currentReadFrame->GetTimestamp() << ')');
      }
    }

    currentReadFrame->next = NULL;

    bufferMutex.Signal();

    // Keep reading from the RTP transport frames
    if (!session.ReadData(*currentReadFrame)) {
      delete currentReadFrame;  // Destructor won't delete this one, so do it here.
      shuttingDown = TRUE; // Flag to stop the reading side thread
      PTRACE(3, "RTP\tJitter RTP receive thread ended");
      return;
    }

    if (currentReadFrame->GetMarker()) {
      // See if remote appears to be setting marker bit on EVERY packet.
      consecutiveMarkerBits++;
      if (consecutiveMarkerBits < maxConsecutiveMarkerBits) {
        PTRACE(3, "RTP\tReceived start of talk burst: " << currentReadFrame->GetTimestamp());
        preBuffering = TRUE;
      }
      if (consecutiveMarkerBits == maxConsecutiveMarkerBits) {
        PTRACE(3, "RTP\tEvery packet has Marker bit, ignoring them from this client!");
      }
    }
    else
      consecutiveMarkerBits = 0;

#if PTRACING && !defined(NO_ANALYSER)
    analyser->In(currentReadFrame->GetTimestamp(), currentDepth, preBuffering ? "PreBuf" : "");
#endif

    // Queue the frame for playing by the thread at other end of jitter buffer
    bufferMutex.Wait();

    // Have been reading a frame, put it into the queue now, at correct position
    if (newestFrame == NULL)
      oldestFrame = newestFrame = currentReadFrame; // Was empty
    else {
      DWORD time = currentReadFrame->GetTimestamp();

      if (time > newestFrame->GetTimestamp()) {
        // Is newer than newst, put at that end of queue
        currentReadFrame->prev = newestFrame;
        newestFrame->next = currentReadFrame;
        newestFrame = currentReadFrame;
      }
      else if (time <= oldestFrame->GetTimestamp()) {
        // Is older than the oldest, put at that end of queue
        currentReadFrame->next = oldestFrame;
        oldestFrame->prev = currentReadFrame;
        oldestFrame = currentReadFrame;
      }
      else {
        // Somewhere in between, locate its position
        Entry * frame = newestFrame->prev;
        while (time < frame->GetTimestamp())
          frame = frame->prev;

        currentReadFrame->prev = frame;
        currentReadFrame->next = frame->next;
        frame->next->prev = currentReadFrame;
        frame->next = currentReadFrame;
      }
    }

    currentDepth++;
  }
}


BOOL RTP_JitterBuffer::ReadData(DWORD timestamp, RTP_DataFrame & frame)
{
  if (shuttingDown)
    return FALSE;

  /*Free the frame just written to codec, putting it back into
    the free list and clearing the parking spot for it.
   */
  if (currentWriteFrame != NULL) {
    bufferMutex.Wait();

    // Move frame from current to free list
    currentWriteFrame->next = freeFrames;
    if (freeFrames != NULL)
      freeFrames->prev = currentWriteFrame;
    freeFrames = currentWriteFrame;

    currentWriteFrame = NULL;

    bufferMutex.Signal();
  }

  // Default response is an empty frame, ie silence
  frame.SetPayloadSize(0);

  /*Get the next frame to write to the codec. Takes it from the oldest
    position in the queue, if it is time to do so, and parks it in the
    special member so can unlock the mutex while the writer thread has its
    way with the buffer.
   */
  if (oldestFrame == NULL) {
    /*No data to play! We ran the buffer down to empty, restart buffer by
      setting flag that will fill it again before returning any data.
     */
    preBuffering = TRUE;

#if PTRACING && !defined(NO_ANALYSER)
    analyser->Out(0, currentDepth, "Empty");
#endif
    return TRUE;
  }

  PWaitAndSignal mutex(bufferMutex);

  /* See if time for this packet, if our oldest frame is older than the
     required age, then use it. If it is not time yet, make sure that the
     writer thread isn't falling behind (not enough MIPS). If the time
     between the oldest and the newest entries in the jitter buffer is
     greater than the size specified for the buffer, then return the oldest
     entry regardless, making the writer thread catch up.
  */

  DWORD oldestTimestamp = oldestFrame->GetTimestamp();
  DWORD newestTimestamp = newestFrame->GetTimestamp();

  if (preBuffering) {
    // Check for requesting something that already exceeds the maximum time,
    // or have filled the jitter buffer, not filling if this is so
    if ((timestamp - oldestTimestamp) < maxJitterTime &&
        (newestTimestamp - oldestTimestamp) < maxJitterTime/2) {
      // Are filling the buffer, don't return anything yet
#if PTRACING && !defined(NO_ANALYSER)
      analyser->Out(oldestTimestamp, currentDepth, "PreBuf");
#endif
      return TRUE;
    }

    preBuffering = FALSE;
  }

  if (timestamp < oldestTimestamp && timestamp > (newestTimestamp - maxJitterTime)) {
    // It is not yet time for something in the buffer
#if PTRACING && !defined(NO_ANALYSER)
    analyser->Out(oldestTimestamp, currentDepth, "Wait");
#endif
    return TRUE;
  }

  // Detatch oldest packet from the list, put into parking space
  currentDepth--;
#if PTRACING && !defined(NO_ANALYSER)
  analyser->Out(oldestTimestamp, currentDepth, timestamp >= oldestTimestamp ? "" : "Late");
#endif
  currentWriteFrame = oldestFrame;
  oldestFrame = currentWriteFrame->next;
  currentWriteFrame->next = NULL;

  if (oldestFrame == NULL)
    newestFrame = NULL;
  else {
    oldestFrame->prev = NULL;

    // See if exceeded maximum jitter buffer time delay, waste them if so
    while ((newestTimestamp - oldestFrame->GetTimestamp()) > maxJitterTime) {
      PTRACE(4, "RTP\tJitter buffer oldest packet ("
             << oldestFrame->GetTimestamp() << " < "
             << (newestTimestamp - maxJitterTime)
             << ") too late, throwing away");

      // Throw away the oldest entry
      Entry * wastedFrame = oldestFrame;
      oldestFrame = oldestFrame->next;
      oldestFrame->prev = NULL;
      currentDepth--;

      // Put thrown away frame on free list
      wastedFrame->next = freeFrames;
      if (freeFrames != NULL)
        freeFrames->prev = wastedFrame;
      freeFrames = wastedFrame;

      packetsTooLate++;
    }
  }

  frame = *currentWriteFrame;
  return TRUE;
}


#if PTRACING && !defined(NO_ANALYSER)

RTP_JitterBufferAnalyser::RTP_JitterBufferAnalyser()
{
  inPos = outPos = 1;
  in[0].time = out[0].time = 0;
  in[0].tick = out[0].tick = PTimer::Tick();
  in[0].depth = out[0].depth = 0;
}


void RTP_JitterBufferAnalyser::In(DWORD time, unsigned depth, const char * extra)
{
  if (inPos < PARRAYSIZE(in)) {
    in[inPos].tick = PTimer::Tick();
    in[inPos].time = time;
    in[inPos].depth = depth;
    in[inPos++].extra = extra;
  }
}


void RTP_JitterBufferAnalyser::Out(DWORD time, unsigned depth, const char * extra)
{
  if (outPos < PARRAYSIZE(out)) {
    out[outPos].tick = PTimer::Tick();
    if (time == 0 && outPos > 0)
      out[outPos].time = out[outPos-1].time;
    else
      out[outPos].time = time;
    out[outPos].depth = depth;
    out[outPos++].extra = extra;
  }
}


void RTP_JitterBufferAnalyser::PrintOn(ostream & strm) const
{
  strm << "Input samples: " << inPos << " Output samples: " << outPos << "\n"
          "Dir\tRTPTime\tInDiff\tOutDiff\tInMode\tOutMode\t"
          "InDepth\tOutDep\tInTick\tInDelay\tOutTick\tOutDel\tIODelay\n";
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
           << (out[ox].tick - in[ix].tick)
           << '\n';
      ox++;
      ix++;
    }
  }
}


#endif


/////////////////////////////////////////////////////////////////////////////
