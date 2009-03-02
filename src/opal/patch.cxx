/*
 * patch.cxx
 *
 * Media stream patch thread.
 *
 * Open Phone Abstraction Library (OPAL)
 * Formally known as the Open H323 project.
 *
 * Copyright (c) 2001 Equivalence Pty. Ltd.
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
 * The Original Code is Open Phone Abstraction Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "patch.h"
#endif


#include <opal/buildopts.h>

#include <opal/patch.h>
#include <opal/mediastrm.h>
#include <opal/transcoders.h>

#if OPAL_VIDEO
#include <codec/vidcodec.h>
#endif

#define new PNEW


/////////////////////////////////////////////////////////////////////////////

OpalMediaPatch::OpalMediaPatch(OpalMediaStream & src)
: source(src)
{
  src.SetPatch(this);
  patchThread = NULL;
  PTRACE(5, "Patch\tCreated media patch " << this);
}


OpalMediaPatch::~OpalMediaPatch()
{
  PWaitAndSignal m(patchThreadMutex);
  inUse.StartWrite();
  if (patchThread != NULL) {
    PAssert(patchThread->WaitForTermination(10000), "Media patch thread not terminated.");
    delete patchThread;
    patchThread = NULL;
  }
  PTRACE(5, "Patch\tDestroyed media patch " << this);
}


void OpalMediaPatch::PrintOn(ostream & strm) const
{
  strm << "Patch " << source;

  inUse.StartRead();

  if (sinks.GetSize() > 0) {
    strm << " -> ";
    if (sinks.GetSize() == 1)
      strm << *sinks.front().stream;
    else {
      PINDEX i = 0;
      for (PList<Sink>::const_iterator s = sinks.begin(); s != sinks.end(); ++s,++i) {
        if (i > 0)
          strm << ", ";
        strm << "sink[" << i << "]=" << *s->stream;
      }
    }
  }

  inUse.EndRead();
}

void OpalMediaPatch::Start()
{
  PWaitAndSignal m(patchThreadMutex);
	
  if(patchThread != NULL) 
    return;
	
  patchThread = new Thread(*this);
  patchThread->Resume();
  PThread::Yield();
  PTRACE(4, "Media\tStarting thread " << patchThread->GetThreadName());
}


void OpalMediaPatch::Close()
{
  PTRACE(3, "Patch\tClosing media patch " << *this);

  inUse.StartWrite();
  filters.RemoveAll();
  source.Close();

  while (sinks.GetSize() > 0) {
    OpalMediaStreamPtr stream = sinks.front().stream;
    inUse.EndWrite();
    if (!stream->Close()) {
      // The only way we can get here is if the sink is in the proccess of being closed
      // but is blocked on the inUse mutex waiting to remove the sink from this patch.
      // Se we unlock it, and wait for it to do it in the other thread.
      PThread::Sleep(10);
    }
    inUse.StartWrite();
  }

  PTRACE(4, "Patch\tWaiting for media patch thread to stop " << *this);
  {
    PWaitAndSignal m(patchThreadMutex);
    if (patchThread != NULL && !patchThread->IsSuspended()) {
      inUse.EndWrite();
      PAssert(patchThread->WaitForTermination(10000), "Media patch thread not terminated.");
      return;
    }
  }
  
  inUse.EndWrite();
}


PBoolean OpalMediaPatch::AddSink(const OpalMediaStreamPtr & sinkStream)
{
  PWriteWaitAndSignal mutex(inUse);

  if (PAssertNULL(sinkStream) == NULL)
    return false;

  PAssert(sinkStream->IsSink(), "Attempt to set source stream as sink!");

  if (!sinkStream->SetPatch(this)) {
    PTRACE(2, "Patch\tCould not set patch in stream " << *sinkStream);
    return false;
  }

  Sink * sink = new Sink(*this, sinkStream);
  sinks.Append(sink);

  // Find the media formats than can be used to get from source to sink
  OpalMediaFormat sourceFormat = source.GetMediaFormat();
  OpalMediaFormat destinationFormat = sinkStream->GetMediaFormat();

  PTRACE(5, "Patch\tAddSink\n"
            "Source format:\n" << setw(-1) << sourceFormat << "\n"
            "Destination format:\n" << setw(-1) << destinationFormat);

  if (sourceFormat == destinationFormat) {
    PTRACE(3, "Patch\tAdded direct media stream sink " << *sinkStream);
    return true;
  }

  PString id = sinkStream->GetID();
  sink->primaryCodec = OpalTranscoder::Create(sourceFormat, destinationFormat, (const BYTE *)id, id.GetLength());
  if (sink->primaryCodec != NULL) {
    PTRACE(4, "Patch\tCreated primary codec " << sourceFormat << "->" << destinationFormat << " with ID " << id);

    if (!sinkStream->SetDataSize(sink->primaryCodec->GetOptimalDataFrameSize(false))) {
      PTRACE(1, "Patch\tSink stream " << *sinkStream << " cannot support data size "
              << sink->primaryCodec->GetOptimalDataFrameSize(PFalse));
      return false;
    }
    sink->primaryCodec->SetMaxOutputSize(sinkStream->GetDataSize());

    PTRACE(3, "Patch\tAdded media stream sink " << *sinkStream
           << " using transcoder " << *sink->primaryCodec << ", data size=" << sinkStream->GetDataSize());
  }
  else {
    OpalMediaFormat intermediateFormat;
    if (!OpalTranscoder::FindIntermediateFormat(sourceFormat, destinationFormat,
                                                intermediateFormat)) {
      PTRACE(1, "Patch\tCould find compatible media format for " << *sinkStream);
      return false;
    }

    sink->primaryCodec = OpalTranscoder::Create(sourceFormat, intermediateFormat, (const BYTE *)id, id.GetLength());
    sink->secondaryCodec = OpalTranscoder::Create(intermediateFormat, destinationFormat, (const BYTE *)id, id.GetLength());

    PTRACE(4, "Patch\tCreated two stage codec " << sourceFormat << "/" << intermediateFormat << "/" << destinationFormat << " with ID " << id);

    if (!sinkStream->SetDataSize(sink->secondaryCodec->GetOptimalDataFrameSize(PFalse))) {
      PTRACE(1, "Patch\tSink stream " << *sinkStream << " cannot support data size "
              << sink->secondaryCodec->GetOptimalDataFrameSize(PFalse));
      return false;
    }
    sink->secondaryCodec->SetMaxOutputSize(sinkStream->GetDataSize());

    PTRACE(3, "Patch\tAdded media stream sink " << *sinkStream
           << " using transcoders " << *sink->primaryCodec
           << " and " << *sink->secondaryCodec << ", data size=" << sinkStream->GetDataSize());
  }

  source.SetDataSize(sink->primaryCodec->GetOptimalDataFrameSize(true));
  return true;
}


void OpalMediaPatch::RemoveSink(const OpalMediaStreamPtr & stream)
{
  if (PAssertNULL(stream) == NULL)
    return;

  PTRACE(3, "Patch\tRemoving media stream sink " << *stream);

  inUse.StartWrite();

  for (PList<Sink>::iterator s = sinks.begin(); s != sinks.end(); ++s) {
    if (s->stream == stream) {
      sinks.erase(s);
      PTRACE(5, "Patch\tRemoved media stream sink " << *stream);
      break;
    }
  }

  inUse.EndWrite();
}


OpalMediaStreamPtr OpalMediaPatch::GetSink(PINDEX i) const
{
  PReadWaitAndSignal mutex(inUse);
  return i < sinks.GetSize() ? sinks[i].stream : OpalMediaStreamPtr();
}


OpalMediaFormat OpalMediaPatch::GetSinkFormat(PINDEX i) const
{
  OpalMediaFormat fmt;

  OpalTranscoder * xcoder = GetAndLockSinkTranscoder(i);
  if (xcoder != NULL) {
    fmt = xcoder->GetOutputFormat();
    UnLockSinkTranscoder();
  }

  return fmt;
}


OpalTranscoder * OpalMediaPatch::GetAndLockSinkTranscoder(PINDEX i) const
{
  inUse.StartRead();

  if (i >= sinks.GetSize()) {
    UnLockSinkTranscoder();
    return NULL;
  }

  Sink & sink = sinks[i];
  if (sink.secondaryCodec != NULL) 
    return sink.secondaryCodec;

  if (sink.primaryCodec != NULL)
    return sink.primaryCodec;

  UnLockSinkTranscoder();

  return NULL;
}


void OpalMediaPatch::UnLockSinkTranscoder() const
{
  inUse.EndRead();
}


#if OPAL_STATISTICS
void OpalMediaPatch::GetStatistics(OpalMediaStatistics & statistics) const
{
  inUse.StartRead();

  if (!sinks.IsEmpty())
    sinks.front().GetStatistics(statistics);

  inUse.EndRead();
}


void OpalMediaPatch::Sink::GetStatistics(OpalMediaStatistics & statistics) const
{
  if (primaryCodec != NULL)
    primaryCodec->GetStatistics(statistics);
  if (secondaryCodec != NULL)
    secondaryCodec->GetStatistics(statistics);
}
#endif


OpalMediaPatch::Sink::Sink(OpalMediaPatch & p, const OpalMediaStreamPtr & s)
  : patch(p)
  , stream(s)
  , primaryCodec(NULL)
  , secondaryCodec(NULL)
  , writeSuccessful(true)
#if OPAL_VIDEO
  , rateController(NULL)
#endif
{
#if OPAL_VIDEO
  SetRateControlParameters(stream->GetMediaFormat());
#endif

  PTRACE(3, "Patch\tCreated Sink: format=" << stream->GetMediaFormat());
}


OpalMediaPatch::Sink::~Sink()
{
  delete primaryCodec;
  delete secondaryCodec;
#if OPAL_VIDEO
  delete rateController;
#endif
}


void OpalMediaPatch::AddFilter(const PNotifier & filter, const OpalMediaFormat & stage)
{
  PWriteWaitAndSignal mutex(inUse);
  
  // ensures that a filter is added only once
  for (PList<Filter>::iterator f = filters.begin(); f != filters.end(); ++f) {
    if (f->notifier == filter && f->stage == stage)
      return;
  }
  filters.Append(new Filter(filter, stage));
}


PBoolean OpalMediaPatch::RemoveFilter(const PNotifier & filter, const OpalMediaFormat & stage)
{
  PWriteWaitAndSignal mutex(inUse);

  for (PList<Filter>::iterator f = filters.begin(); f != filters.end(); ++f) {
    if (f->notifier == filter && f->stage == stage) {
      filters.erase(f);
      return PTrue;
    }
  }

  return PFalse;
}


void OpalMediaPatch::FilterFrame(RTP_DataFrame & frame,
                                 const OpalMediaFormat & mediaFormat)
{
  inUse.StartRead();

  for (PList<Filter>::iterator f = filters.begin(); f != filters.end(); ++f) {
    if (f->stage.IsEmpty() || f->stage == mediaFormat)
      f->notifier(frame, (INT)this);
  }

  inUse.EndRead();
}


bool OpalMediaPatch::UpdateMediaFormat(const OpalMediaFormat & mediaFormat)
{
  PReadWaitAndSignal mutex(inUse);

  bool atLeastOne = source.UpdateMediaFormat(mediaFormat, true);

  for (PList<Sink>::iterator s = sinks.begin(); s != sinks.end(); ++s)
    atLeastOne = s->UpdateMediaFormat(mediaFormat) || atLeastOne;

  PTRACE_IF(2, !atLeastOne, "Patch\tCould not update media format for any stream/transcoder in " << *this);
  return atLeastOne;
}


PBoolean OpalMediaPatch::ExecuteCommand(const OpalMediaCommand & command, PBoolean fromSink)
{
  PReadWaitAndSignal mutex(inUse);

  if (fromSink)
    return source.ExecuteCommand(command);

  PBoolean atLeastOne = PFalse;
  for (PList<Sink>::iterator s = sinks.begin(); s != sinks.end(); ++s)
    atLeastOne = s->ExecuteCommand(command) || atLeastOne;

  return atLeastOne;
}


void OpalMediaPatch::SetCommandNotifier(const PNotifier & notifier, PBoolean fromSink)
{
  PReadWaitAndSignal mutex(inUse);

  if (fromSink)
    source.SetCommandNotifier(notifier);
  else {
    for (PList<Sink>::iterator s = sinks.begin(); s != sinks.end(); ++s)
      s->SetCommandNotifier(notifier);
  }
}


bool OpalMediaPatch::OnPatchStart()
{
  source.OnPatchStart();

  if (source.IsSynchronous())
    return false;

  bool isAudio = source.GetMediaFormat().GetMediaType() == OpalMediaType::Audio();

  PReadWaitAndSignal mutex(inUse);

  for (PList<Sink>::iterator s = sinks.begin(); s != sinks.end(); ++s) {
    if (s->stream->IsSynchronous()) {
      if (isAudio) 
        source.EnableJitterBuffer();
      return false;
    }
  }
	
  return true;
}


void OpalMediaPatch::Main()
{
  PTRACE(4, "Patch\tThread started for " << *this);
	
  bool asynchronous = OnPatchStart();

  RTP_DataFrame sourceFrame(source.GetDataSize());
  sourceFrame.SetPayloadType(source.GetMediaFormat().GetPayloadType());

  while (source.IsOpen()) {
    sourceFrame.SetPayloadSize(0); 
    if (!source.ReadPacket(sourceFrame)) {
      PTRACE(4, "Patch\tThread ended because source read failed");
      break;
    }
 
    inUse.StartRead();
    bool written = DispatchFrame(sourceFrame);
    inUse.EndRead();

    if (!written) {
      PTRACE(4, "Patch\tThread ended because all sink writes failed failed");
      break;
    }
 
    // Don't starve the CPU if we have idle frames and the no source or destination is synchronous.
    // Note that performing a Yield is not good enough, as the media patch threads are
    // high priority and will consume all available CPU if allowed.
    if (asynchronous)
      PThread::Sleep(5);
  }

  source.OnPatchStop();

  PTRACE(4, "Patch\tThread ended for " << *this);
}


bool OpalMediaPatch::DispatchFrame(RTP_DataFrame & frame)
{
  FilterFrame(frame, source.GetMediaFormat());    

  bool written = false;
  for (PList<Sink>::iterator s = sinks.begin(); s != sinks.end(); ++s) {
    if (s->WriteFrame(frame))
      written = true;
    else {
      PTRACE(2, "Patch\tWriteFrame failed");
    }
  }

  return written;
}


bool OpalMediaPatch::Sink::UpdateMediaFormat(const OpalMediaFormat & mediaFormat)
{
  bool ok;

  if (primaryCodec == NULL)
    ok = stream->UpdateMediaFormat(mediaFormat, true);
  else if (secondaryCodec != NULL && secondaryCodec->GetOutputFormat() == mediaFormat)
    ok = secondaryCodec->UpdateMediaFormats(OpalMediaFormat(), mediaFormat) &&
         stream->UpdateMediaFormat(secondaryCodec->GetOutputFormat(), true);
  else if (primaryCodec->GetOutputFormat() == mediaFormat)
    ok = primaryCodec->UpdateMediaFormats(OpalMediaFormat(), mediaFormat) &&
         stream->UpdateMediaFormat(primaryCodec->GetOutputFormat(), true);
  else
    ok = primaryCodec->UpdateMediaFormats(mediaFormat, OpalMediaFormat()) &&
         stream->UpdateMediaFormat(primaryCodec->GetInputFormat(), true);

#if OPAL_VIDEO
    SetRateControlParameters(stream->GetMediaFormat());
#endif

  PTRACE(3, "Patch\tUpdated Sink: format=" << mediaFormat << " ok=" << ok);
  return ok;
}


bool OpalMediaPatch::Sink::ExecuteCommand(const OpalMediaCommand & command)
{
  PBoolean atLeastOne = PFalse;

  if (secondaryCodec != NULL)
    atLeastOne = secondaryCodec->ExecuteCommand(command) || atLeastOne;

  if (primaryCodec != NULL)
    atLeastOne = primaryCodec->ExecuteCommand(command) || atLeastOne;

  return atLeastOne;
}


void OpalMediaPatch::Sink::SetCommandNotifier(const PNotifier & notifier)
{
  if (secondaryCodec != NULL)
    secondaryCodec->SetCommandNotifier(notifier);

  if (primaryCodec != NULL)
    primaryCodec->SetCommandNotifier(notifier);
}


static bool CannotTranscodeFrame(const OpalTranscoder & codec, RTP_DataFrame & frame)
{
  RTP_DataFrame::PayloadTypes pt = frame.GetPayloadType();

  if (!codec.AcceptComfortNoise()) {
    if (pt == RTP_DataFrame::CN || pt == RTP_DataFrame::Cisco_CN) {
      PTRACE(4, "Patch\tRemoving comfort noise frame with payload type " << pt);
      frame.SetPayloadSize(0);   // remove the payload because the transcoder has indicated it won't understand it
      frame.SetPayloadType(codec.GetPayloadType(true));
      return true;
    }
  }

  if ((pt != codec.GetPayloadType(true)) && !codec.AcceptOtherPayloads()) {
    PTRACE(4, "Patch\tRemoving frame with mismatched payload type " << pt << " - should be " << codec.GetPayloadType(true));
    frame.SetPayloadSize(0);   // remove the payload because the transcoder has indicated it won't understand it
    frame.SetPayloadType(codec.GetPayloadType(true)); // Reset pt so if get silence frames from jitter buffer, they don't cause errors
    return true;
  }

  if (!codec.AcceptEmptyPayload() && frame.GetPayloadSize() == 0) 
    return true;

  return false;
}


#if OPAL_VIDEO
void OpalMediaPatch::Sink::SetRateControlParameters(const OpalMediaFormat & mediaFormat)
{
  if ((mediaFormat.GetMediaType() == OpalMediaType::Video()) && mediaFormat != OpalYUV420P) {
    PString rc = mediaFormat.GetOptionString(OpalVideoFormat::RateControllerOption());
    if (rc.IsEmpty() && mediaFormat.GetOptionBoolean(OpalVideoFormat::RateControlEnableOption()))
      rc = "Standard";
    rateController = PFactory<OpalVideoRateController>::CreateInstance(rc);
  }

  if (rateController != NULL) 
    rateController->Open(mediaFormat);
}


bool OpalMediaPatch::Sink::RateControlExceeded(bool & forceIFrame)
{
  if (!rateController != NULL || !rateController->SkipFrame(forceIFrame)) 
    return false;

  PTRACE(4, "Patch\tRate controller skipping frame.");
  return true;
}

#endif


bool OpalMediaPatch::Sink::WriteFrame(RTP_DataFrame & sourceFrame)
{
  if (!writeSuccessful)
    return false;
  
#if OPAL_VIDEO
  if (rateController != NULL) {
    bool forceIFrame = false;
    if (RateControlExceeded(forceIFrame)) {
      if (forceIFrame)
        stream->ExecuteCommand(OpalVideoUpdatePicture());
      return true;
    }
  }
#endif

  if (primaryCodec == NULL)
    return (writeSuccessful = stream->WritePacket(sourceFrame));

  if (CannotTranscodeFrame(*primaryCodec, sourceFrame))
    return (writeSuccessful = stream->WritePacket(sourceFrame));

  if (!primaryCodec->ConvertFrames(sourceFrame, intermediateFrames)) {
    PTRACE(1, "Patch\tMedia conversion (primary) failed");
    return false;
  }

#if OPAL_VIDEO
  if (secondaryCodec == NULL && rateController != NULL) {
    rateController->Push(intermediateFrames, ((OpalVideoTranscoder *)primaryCodec)->WasLastFrameIFrame());
    bool wasIFrame = false;
    if (rateController->Pop(intermediateFrames, wasIFrame, false)) {
      for (RTP_DataFrameList::iterator interFrame = intermediateFrames.begin(); interFrame != intermediateFrames.end(); ++interFrame) {
        patch.FilterFrame(*interFrame, primaryCodec->GetOutputFormat());
        if (!stream->WritePacket(*interFrame))
          return (writeSuccessful = false);
        sourceFrame.SetTimestamp(interFrame->GetTimestamp());
        continue;
      }
    }
  }
  else 
#endif
  for (RTP_DataFrameList::iterator interFrame = intermediateFrames.begin(); interFrame != intermediateFrames.end(); ++interFrame) {
    patch.FilterFrame(*interFrame, primaryCodec->GetOutputFormat());

    if (secondaryCodec == NULL) {
      if (!stream->WritePacket(*interFrame))
        return (writeSuccessful = false);
      sourceFrame.SetTimestamp(interFrame->GetTimestamp());
      continue;
    }

    if (CannotTranscodeFrame(*secondaryCodec, *interFrame)) {
      if (!stream->WritePacket(*interFrame))
        return (writeSuccessful = false);
      continue;
    }

    if (!secondaryCodec->ConvertFrames(*interFrame, finalFrames)) {
      PTRACE(1, "Patch\tMedia conversion (secondary) failed");
      return false;
    }

    for (RTP_DataFrameList::iterator finalFrame = finalFrames.begin(); finalFrame != finalFrames.end(); ++finalFrame) {
      patch.FilterFrame(*finalFrame, secondaryCodec->GetOutputFormat());
      if (!stream->WritePacket(*finalFrame))
        return (writeSuccessful = false);
      sourceFrame.SetTimestamp(finalFrame->GetTimestamp());
    }
  }

#if OPAL_VIDEO
  //if (rcEnabled)
  //  rateController.AddFrame(totalPayloadSize, frameCount);
#endif

  return true;
}


OpalMediaPatch::Thread::Thread(OpalMediaPatch & p)
  : PThread(65536,  //16*4kpage size
            NoAutoDeleteThread,
            HighPriority,
            "Media Patch"),
    patch(p)
{
}


/////////////////////////////////////////////////////////////////////////////


OpalPassiveMediaPatch::OpalPassiveMediaPatch(OpalMediaStream & source)
: OpalMediaPatch(source)
{
}


void OpalPassiveMediaPatch::Start()
{
  source.OnPatchStart();
}


PBoolean OpalPassiveMediaPatch::PushFrame(RTP_DataFrame & frame)
{
  DispatchFrame(frame);
  return PTrue;
}


