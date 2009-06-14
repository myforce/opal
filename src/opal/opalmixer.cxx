/*
 * opalmixer.cxx
 *
 * OPAL media mixers
 *
 * Open Phone Abstraction Library (OPAL)
 * Formally known as the Open H323 project.
 *
 * Copyright (C) 2007 Post Increment
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
 * The Initial Developer of the Original Code is Post Increment
 *
 * Contributor(s): Craig Southeren (craigs@postincrement.com)
 *                 Robert Jongbloed (robertj@voxlucida.com.au)
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#include <ptlib.h>

#include <opal/buildopts.h>

#include <opal/opalmixer.h>
#include <opal/patch.h>
#include <rtp/rtp.h>
#include <rtp/jitter.h>
#include <ptlib/vconvert.h>


#define DETAIL_LOG_LEVEL 4


/////////////////////////////////////////////////////////////////////////////

OpalBaseMixer::OpalBaseMixer(bool pushThread, unsigned periodMS, unsigned periodTS)
  : m_pushThread(pushThread)
  , m_periodMS(periodMS)
  , m_periodTS(periodTS)
  , m_outputTimestamp(10000000)
  , m_pushFrame(NULL)
  , m_workerThread(NULL)
  , m_threadRunning(false)
{
}


OpalBaseMixer::~OpalBaseMixer()
{
  delete m_pushFrame;
}


void OpalBaseMixer::RemoveStream(const Key_T & key)
{
  m_mutex.Wait();

  StreamMap_T::iterator iter = m_inputStreams.find(key);
  if (iter != m_inputStreams.end()) {
    delete iter->second;
    m_inputStreams.erase(iter);
    PTRACE(4, "Mixer\tRemoved stream at key " << key);
  }

  if (m_inputStreams.empty())
    StopPushThread(false);

  m_mutex.Signal();
}


void OpalBaseMixer::RemoveAllStreams()
{
  PTRACE(4, "Mixer\tRemoved all streams");

  m_mutex.Wait();

  for (StreamMap_T::iterator iter = m_inputStreams.begin(); iter != m_inputStreams.end(); ++iter)
    delete iter->second;
  m_inputStreams.clear();

  StopPushThread(false);

  m_mutex.Signal();
}


bool OpalBaseMixer::AddStream(const Key_T & key)
{
  PWaitAndSignal mutex(m_mutex);

  StreamMap_T::iterator iter = m_inputStreams.find(key);
  if (iter != m_inputStreams.end()) 
    return false;

  m_inputStreams[key] = CreateStream();
  PTRACE(4, "Mixer\tAdded stream at key " << key);

  StartPushThread();
  return true;
}


bool OpalBaseMixer::WriteStream(const Key_T & key, const RTP_DataFrame & rtp)
{
  if (rtp.GetPayloadSize() == 0)
    return true;

  RTP_DataFrame uniqueRTP = rtp;
  uniqueRTP.MakeUnique();
  if (uniqueRTP.IsEmpty())
    return false;

  m_mutex.Wait();

  // Search for stream, note: writing a stream not yet attached is non-fatal
  StreamMap_T::iterator iter = m_inputStreams.find(key);
  if (iter != m_inputStreams.end())
    iter->second->QueuePacket(uniqueRTP);

  m_mutex.Signal();

  return true;
}


RTP_DataFrame * OpalBaseMixer::ReadMixed()
{
  // create output frame
  RTP_DataFrame * mixed = new RTP_DataFrame(0, GetOutputSize());
  mixed->SetPayloadType(RTP_DataFrame::MaxPayloadType);
  if (ReadMixed(*mixed))
    return mixed;

  delete mixed;
  return NULL;
}


bool OpalBaseMixer::ReadMixed(RTP_DataFrame & mixed)
{
  PWaitAndSignal mutex(m_mutex);

  if (!MixStreams(mixed))
    return false;

  mixed.SetTimestamp(m_outputTimestamp);
  m_outputTimestamp += m_periodTS;

  return true;
}


void OpalBaseMixer::StartPushThread()
{
  if (m_pushThread) {
    PWaitAndSignal mutex(m_mutex);
    if (m_workerThread == NULL) {
      m_threadRunning = true;
      m_workerThread = new PThreadObj<OpalBaseMixer>(*this,
                                                     &OpalBaseMixer::PushThreadMain,
                                                     false,
                                                     "OpalMixer",
                                                     PThread::HighestPriority);
    }
  }
}


void OpalBaseMixer::StopPushThread(bool lock)
{
  if (lock)
    m_mutex.Wait();

  m_threadRunning = false;
  if (m_workerThread != NULL) {
    m_mutex.Signal();

    m_workerThread->WaitForTermination();

    m_mutex.Wait();

    delete m_workerThread;
    m_workerThread = NULL;
  }

  if (lock)
    m_mutex.Signal();
}


void OpalBaseMixer::PushThreadMain()
{
  PAdaptiveDelay delay;
  while (m_threadRunning && OnPush())
    delay.Delay(m_periodMS);
}


bool OpalBaseMixer::OnPush()
{
  if (m_pushFrame == NULL) {
    m_pushFrame = new RTP_DataFrame(0, GetOutputSize());
    m_pushFrame->SetPayloadType(RTP_DataFrame::MaxPayloadType);
  }

  return ReadMixed(*m_pushFrame) && OnMixed(m_pushFrame);
}


bool OpalBaseMixer::OnMixed(RTP_DataFrame * & /*mixed*/)
{
  return false;
}


/////////////////////////////////////////////////////////////////////////////

OpalAudioMixer::OpalAudioMixer(bool stereo,
                           unsigned sampleRate,
                               bool pushThread,
                           unsigned period)
  : OpalBaseMixer(pushThread, period, period*sampleRate/1000)
  , m_stereo(stereo)
  , m_sampleRate(sampleRate)
  , m_left(NULL)
  , m_right(NULL)
{
  m_mixedAudio.resize(m_periodTS);
}


OpalBaseMixer::Stream * OpalAudioMixer::CreateStream()
{
  AudioStream * stream = new AudioStream(*this);

  if (m_stereo) {
    if (m_left == NULL)
      m_left = stream;
    else if (m_right == NULL)
      m_right = stream;
    else {
      PTRACE(2, "Mixer\tCannot have more than two streams for stereo mode!");
      delete stream;
      return NULL;
    }
  }

  return stream;
}


void OpalAudioMixer::RemoveStream(const Key_T & key)
{
  if (m_stereo) {
    PWaitAndSignal mutex(m_mutex);
    StreamMap_T::iterator iter = m_inputStreams.find(key);
    if (iter == m_inputStreams.end())
      return;
    if (m_left == iter->second)
      m_left = NULL;
    else if (m_right == iter->second)
      m_right = NULL;
  }

  OpalBaseMixer::RemoveStream(key);
}


void OpalAudioMixer::RemoveAllStreams()
{
  OpalBaseMixer::RemoveAllStreams();
  m_left = m_right = NULL;
}


bool OpalAudioMixer::SetSampleRate(unsigned rate)
{
  PWaitAndSignal mutex(m_mutex);

  if (m_inputStreams.size() > 0)
    return rate == m_sampleRate;

  m_periodTS = m_periodMS*rate/1000;
  m_sampleRate = rate;
  m_mixedAudio.resize(m_periodTS);
  for (StreamMap_T::iterator iter = m_inputStreams.begin(); iter != m_inputStreams.end(); ++iter)
    ((AudioStream *)iter->second)->m_cacheSamples.SetSize(m_periodTS);
  return true;
}


bool OpalAudioMixer::SetJitterBufferSize(const Key_T & key, unsigned minJitterDelay, unsigned maxJitterDelay)
{
  PWaitAndSignal mutex(m_mutex);

  StreamMap_T::iterator iter = m_inputStreams.find(key);
  if (iter == m_inputStreams.end())
    return false;

  OpalJitterBuffer * & jitter = ((AudioStream *)iter->second)->m_jitter;
  if (jitter != NULL) {
    if (minJitterDelay != 0 && maxJitterDelay != 0)
      jitter->SetDelay(minJitterDelay, maxJitterDelay);
    else {
      delete jitter;
      jitter = NULL;
    }
  }
  else {
    if (minJitterDelay != 0 && maxJitterDelay != 0)
      jitter = new OpalJitterBuffer(minJitterDelay, maxJitterDelay, m_sampleRate/1000);
  }

  return true;
}


void OpalAudioMixer::PreMixStreams()
{
  // Expected to already be mutexed

  size_t streamCount = m_inputStreams.size();
  const short ** buffers = (const short **)alloca(streamCount*sizeof(short *));

  size_t i = 0;
  for (StreamMap_T::iterator iter = m_inputStreams.begin(); iter != m_inputStreams.end(); ++iter, ++i)
    buffers[i] = ((AudioStream *)iter->second)->GetAudioDataPtr();

  for (unsigned samp = 0; samp < m_periodTS; ++samp) {
    m_mixedAudio[samp] = 0;
    for (size_t strm = 0; strm < streamCount; ++strm)
      m_mixedAudio[samp] += *(buffers[strm])++;
  }
}


bool OpalAudioMixer::MixStreams(RTP_DataFrame & frame)
{
  // Expected to already be mutexed

  if (m_stereo)
    MixStereo(frame);
  else {
    PreMixStreams();
    frame.SetPayloadSize(0);
    MixAdditive(frame, NULL);
  }
  return true;
}


void OpalAudioMixer::MixStereo(RTP_DataFrame & frame)
{
  // Expected to already be mutexed

  frame.SetPayloadSize(GetOutputSize());

  if (m_left != NULL) {
    const short * src = m_left->GetAudioDataPtr();
    short * dst = (short *)frame.GetPayloadPtr();
    for (unsigned i = 0; i < m_periodTS; ++i) {
      *dst++ = *src++;
      dst++;
    }
  }

  if (m_right != NULL) {
    const short * src = m_right->GetAudioDataPtr();
    short * dst = (short *)frame.GetPayloadPtr();
    for (unsigned i = 0; i < m_periodTS; ++i) {
      dst++;
      *dst++ = *src++;
    }
  }
}


void OpalAudioMixer::MixAdditive(RTP_DataFrame & frame, const short * audioToSubtract)
{
  // Expected to already be mutexed

  PINDEX size = frame.GetPayloadSize();
  frame.SetPayloadSize(size + m_periodTS*sizeof(short));

  if (size == 0)
    frame.SetTimestamp(m_outputTimestamp);

  short * dst = (short *)(frame.GetPayloadPtr()+size);
  for (unsigned i = 0; i < m_periodTS; ++i) {
    int value = m_mixedAudio[i];
    if (audioToSubtract != NULL)
      value -= *audioToSubtract++;
    if (value < -32765)
      value = -32765;
    else if (value > 32765)
      value = 32765;
    *dst++ = (short)value;
  }
}


size_t OpalAudioMixer::GetOutputSize() const
{
  return m_periodTS*(m_stereo ? (sizeof(short) * 2) : sizeof(short));
}


OpalAudioMixer::AudioStream::AudioStream(OpalAudioMixer & mixer)
  : m_mixer(mixer)
  , m_jitter(NULL)
  , m_nextTimestamp(0)
  , m_cacheSamples(mixer.GetPeriodTS())
  , m_samplesUsed(0)
{
}


OpalAudioMixer::AudioStream::~AudioStream()
{
  delete m_jitter;
}


void OpalAudioMixer::AudioStream::QueuePacket(const RTP_DataFrame & rtp)
{
  if (m_jitter == NULL)
    m_queue.push(rtp);
  else
    m_jitter->WriteData(rtp);
}


const short * OpalAudioMixer::AudioStream::GetAudioDataPtr()
{
  size_t samplesLeft = m_mixer.GetPeriodTS();
  short * cachePtr = m_cacheSamples.GetPointer(samplesLeft);

  while (samplesLeft > 0) {
    if (m_queue.empty()) {
      if (m_jitter == NULL)
        break;
      RTP_DataFrame frame;
      frame.SetTimestamp(m_nextTimestamp);
      if (!m_jitter->ReadData(frame) || frame.GetPayloadSize() == 0)
        break;
      m_nextTimestamp = frame.GetTimestamp();
      m_queue.push(frame);
    }

    size_t payloadSamples = m_queue.front().GetPayloadSize()/sizeof(short);
    size_t samplesToCopy = payloadSamples - m_samplesUsed;
    if (samplesToCopy > samplesLeft)
      samplesToCopy = samplesLeft;

    memcpy(cachePtr, ((const short *)m_queue.front().GetPayloadPtr())+m_samplesUsed, samplesToCopy*sizeof(short));

    cachePtr += samplesToCopy;
    samplesLeft -= samplesToCopy;
    m_nextTimestamp += samplesToCopy;

    m_samplesUsed += samplesToCopy;
    if (m_samplesUsed >= payloadSamples) {
      m_queue.pop();
      m_samplesUsed = 0;
    }
  }

  if (samplesLeft > 0) {
    memset(cachePtr, 0, samplesLeft*sizeof(short)); // Silence
    m_nextTimestamp += samplesLeft;
  }

  return m_cacheSamples;
}


//////////////////////////////////////////////////////////////////////////////

#if OPAL_VIDEO

OpalVideoMixer::OpalVideoMixer(Styles style, unsigned width, unsigned height, unsigned rate, bool pushThread)
  : OpalBaseMixer(pushThread, 1000/rate, OpalMediaFormat::VideoClockRate/rate)
  , m_style(style)
{
  SetFrameSize(width, height);
}


bool OpalVideoMixer::SetFrameRate(unsigned rate)
{
  if (rate == 0 || rate > 100)
    return false;

  m_mutex.Wait();
  m_periodMS = 1000/rate;
  m_periodTS = OpalMediaFormat::VideoClockRate/rate;
  m_mutex.Signal();

  return true;
}


bool OpalVideoMixer::SetFrameSize(unsigned width, unsigned height)
{
  m_mutex.Wait();

  m_width = width;
  m_height = height;
  PColourConverter::FillYUV420P(0, 0, m_width, m_height, m_width, m_height,
                                m_frameStore.GetPointer(m_width*m_height*3/2),
                                0, 0, 0);

  m_mutex.Signal();
  return true;
}


OpalBaseMixer::Stream * OpalVideoMixer::CreateStream()
{
  return new VideoStream(*this);
}


bool OpalVideoMixer::MixStreams(RTP_DataFrame & frame)
{
  // create output frame
  unsigned left, x, y, w, h;
  switch (m_style) {
    case eSideBySideLetterbox :
      x = left = 0;
      y = m_height/4;
      w = m_width/2;
      h = m_height/2;
      break;

    case eSideBySideScaled :
      x = left = 0;
      y = 0;
      w = m_width/2;
      h = m_height;
      break;

    case eStackedPillarbox :
      x = left = m_width/4;
      y = 0;
      w = m_width/2;
      h = m_height/2;
      break;

    case eStackedScaled :
      x = left = 0;
      y = 0;
      w = m_width;
      h = m_height/2;
      break;

    case eGrid :
      x = left = 0;
      y = 0;
      switch (m_inputStreams.size()) {
        case 0 :
        case 1 :
          w = m_width;
          h = m_height;
          break;
          break;

        case 2 :
        case 3 :
        case 4 :
          w = m_width/2;
          h = m_height/2;
          break;

        case 5 :
        case 6 :
        case 7 :
        case 8 :
        case 9 :
          w = m_width/3;
          h = m_height/3;
          break;

        default :
          w = m_width/4;
          h = m_height/4;
          break;
      }
      break;

    default :
      return false;
  }

  for (StreamMap_T::iterator iter = m_inputStreams.begin(); iter != m_inputStreams.end(); ++iter) {
    ((VideoStream *)iter->second)->InsertVideoFrame(x, y, w, h);

    x += w;
    if (x+w > m_width) {
      x = left;
      y += h;
      if (y+h > m_height)
        break;
    }
  }

  frame.SetPayloadSize(GetOutputSize());
  PluginCodec_Video_FrameHeader * video = (PluginCodec_Video_FrameHeader *)frame.GetPayloadPtr();
  video->width = m_width;
  video->height = m_height;
  memcpy(OPAL_VIDEO_FRAME_DATA_PTR(video), m_frameStore, m_frameStore.GetSize());

  return true;
}


size_t OpalVideoMixer::GetOutputSize() const
{
  return m_frameStore.GetSize() + sizeof(PluginCodec_Video_FrameHeader);
}


OpalVideoMixer::VideoStream::VideoStream(OpalVideoMixer & mixer)
  : m_mixer(mixer)
{
}


void OpalVideoMixer::VideoStream::QueuePacket(const RTP_DataFrame & rtp)
{
  m_queue.push(rtp);
}


void OpalVideoMixer::VideoStream::InsertVideoFrame(unsigned x, unsigned y, unsigned w, unsigned h)
{
  if (m_queue.empty())
    return;

  PluginCodec_Video_FrameHeader * header = (PluginCodec_Video_FrameHeader *)m_queue.front().GetPayloadPtr();

  PTRACE(DETAIL_LOG_LEVEL, "Mixer\tCopying video: " << header->width << 'x' << header->height
         << " -> " << x << ',' << y << '/' << w << 'x' << h);

  PColourConverter::CopyYUV420P(0, 0, header->width, header->height,
                                header->width, header->height, OPAL_VIDEO_FRAME_DATA_PTR(header),
                                x, y, w, h,
                                m_mixer.m_width, m_mixer.m_height, m_mixer.m_frameStore.GetPointer(),
                                PVideoFrameInfo::eScale);

  m_queue.pop();
}


#endif // OPAL_VIDEO


///////////////////////////////////////////////////////////////////////////////

OpalMixerEndPoint::OpalMixerEndPoint(OpalManager & manager, const char * prefix)
  : OpalLocalEndPoint(manager, prefix)
  , m_createAdHoc(true)
{
  m_nodesByName.DisallowDeleteObjects();
  PTRACE(4, "MixerEP\tConstructed");
}


OpalMixerEndPoint::~OpalMixerEndPoint()
{
  PTRACE(4, "MixerEP\tDestroyed");
}


OpalMediaFormatList OpalMixerEndPoint::GetMediaFormats() const
{
  OpalMediaFormatList formats;
  formats += OpalPCM16;
#if OPAL_VIDEO
  formats += OpalYUV420P;
#endif
  return formats;
}


PBoolean OpalMixerEndPoint::MakeConnection(OpalCall & call,
                                            const PString & party,
                                            void * userData,
                                            unsigned int options,
                                            OpalConnection::StringOptions * stringOptions)
{
  PTRACE(4, "MixerEP\tMaking connection to \"" << party << '"');

  PString name = party.Mid(party.Find(':')+1);
  if (name.IsEmpty() || name == "*")
    name = GetAdHocNodeInfo().m_name;

  PSafePtr<OpalMixerNode> node = FindNode(name);
  if (node == NULL && m_createAdHoc) {
    OpalMixerNodeInfo * info = GetAdHocNodeInfo().Clone();
    info->m_name = name;
    node = AddNode(info);
  }
  return node != NULL && AddConnection(CreateConnection(node, call, userData, options, stringOptions));
}


PBoolean OpalMixerEndPoint::GarbageCollection()
{
  // Use bitwise | not boolean || so both get executed
  return m_nodesByUID.DeleteObjectsToBeRemoved() | OpalEndPoint::GarbageCollection();
}


OpalMixerConnection * OpalMixerEndPoint::CreateConnection(PSafePtr<OpalMixerNode> node,
                                                                       OpalCall & call,
                                                                           void * userData,
                                                                         unsigned options,
                                                  OpalConnection::StringOptions * stringOptions)
{
  return new OpalMixerConnection(node, call, *this, userData, options, stringOptions);
}


PSafePtr<OpalMixerNode> OpalMixerEndPoint::AddNode(OpalMixerNodeInfo * info)
{
  PSafePtr<OpalMixerNode> node(CreateNode(info), PSafeReference);
  if (node == NULL)
    delete info;
  else {
    m_nodesByUID.SetAt(node->GetGUID(), node);
    PTRACE(3, "MixerEP\tAdded new node, id=" << node->GetGUID());
  }
  return node;
}


OpalMixerNode * OpalMixerEndPoint::CreateNode(OpalMixerNodeInfo * info)
{
  return new OpalMixerNode(*this, info);
}


PSafePtr<OpalMixerNode> OpalMixerEndPoint::FindNode(const PString & name, PSafetyMode mode)
{
  PGloballyUniqueID guid(name);
  if (!guid.IsNULL())
    return m_nodesByUID.FindWithLock(guid, mode);

  return PSafePtr<OpalMixerNode>(m_nodesByName.GetAt(name), mode);
}


void OpalMixerEndPoint::RemoveNode(OpalMixerNode & node)
{
  node.ShutDown();
  m_nodesByUID.RemoveAt(node.GetGUID());
}


///////////////////////////////////////////////////////////////////////////////

OpalMixerConnection::OpalMixerConnection(PSafePtr<OpalMixerNode> node,
                                                      OpalCall & call,
                                             OpalMixerEndPoint & ep,
                                                          void * userData,
                                                        unsigned options,
                                 OpalConnection::StringOptions * stringOptions)
  : OpalLocalConnection(call, ep, userData, options, stringOptions, 'M')
  , m_endpoint(ep)
  , m_node(node)
  , m_listenOnly(node->GetNodeInfo().m_listenOnly)
{
  m_node->AttachConnection(this);
  PTRACE(4, "MixerCon\tConstructed");
}


OpalMixerConnection::~OpalMixerConnection()
{
  PTRACE(4, "MixerCon\tDestroyed");
}


void OpalMixerConnection::OnReleased()
{
  m_node->DetachConnection(this);
  OpalLocalConnection::OnReleased();
}


OpalMediaFormatList OpalMixerConnection::GetMediaFormats() const
{
  OpalMediaFormatList list = OpalTranscoder::GetPossibleFormats(OpalPCM16);
#if OPAL_VIDEO
  if (!m_node->GetNodeInfo().m_audioOnly)
    list += OpalTranscoder::GetPossibleFormats(OpalYUV420P);
#endif
  return list;
}


OpalMediaStream * OpalMixerConnection::CreateMediaStream(const OpalMediaFormat & mediaFormat,
                                                         unsigned sessionID,
                                                         PBoolean isSource)
{
  return new OpalMixerMediaStream(*this, mediaFormat, sessionID, isSource, m_node, m_listenOnly);
}


void OpalMixerConnection::SetListenOnly(bool listenOnly)
{
  PTRACE(3, "MixerCon\tSet listen only mode to " << (listenOnly ? "ON" : "OFF"));

  m_listenOnly = listenOnly;

  for (PSafePtr<OpalMediaStream> mediaStream(mediaStreams, PSafeReference); mediaStream != NULL; ++mediaStream) {
    OpalMixerMediaStream * mixerStream = dynamic_cast<OpalMixerMediaStream *>(&*mediaStream);
    if (mixerStream != NULL && mixerStream->IsSink()) {
      mixerStream->SetPaused(listenOnly);
      if (listenOnly)
        m_node->DetachStream(mixerStream);
      else
        m_node->AttachStream(mixerStream);
    }
  }
}


///////////////////////////////////////////////////////////////////////////////

OpalMixerMediaStream::OpalMixerMediaStream(OpalMixerConnection & conn,
                                         const OpalMediaFormat & format,
                                                        unsigned sessionID,
                                                            bool isSource,
                                         PSafePtr<OpalMixerNode> node,
                                                            bool listenOnly)
  : OpalMediaStream(conn, format, sessionID, isSource)
  , m_node(node)
#if OPAL_VIDEO
  , m_video(mediaFormat.GetMediaType() == OpalMediaType::Video())
#endif
{
  paused = IsSink() && listenOnly;

  /* We are a bit sneaky here. OpalCall::OpenSourceMediaStream will have
     selected the network codec (e.g. G.723.1) anbd passed it to us, but for
     the case of incoming media to the mixer (sink), we switch it to the raw
     codec type so the OpalPatch system creates the codec for us. With the
     transmitter (source) we keep the required media format so the mixed data
     thread can cache and optimise an encoded frame across multiple remote
     connections. */
  if (IsSink()) {
#if OPAL_VIDEO
    if (m_video)
      mediaFormat = OpalYUV420P;
    else
#endif
      mediaFormat = OpalPCM16;
  }
}


OpalMixerMediaStream::~OpalMixerMediaStream()
{
  Close();
}


PBoolean OpalMixerMediaStream::Open()
{
  if (isOpen)
    return true;

  if (mediaFormat.GetMediaType() != OpalMediaType::Audio()
#if OPAL_VIDEO
   && mediaFormat.GetMediaType() != OpalMediaType::Video()
#endif
  ) {
    PTRACE(3, "MixerStrm\tCannot open media stream of type " << mediaFormat.GetMediaType());
    return false;
  }

  if (!paused && !m_node->AttachStream(this))
    return false;

  return OpalMediaStream::Open();
}


PBoolean OpalMixerMediaStream::Close()
{
  if (!isOpen)
    return false;

  m_node->DetachStream(this);

  return OpalMediaStream::Close();
}


PBoolean OpalMixerMediaStream::WritePacket(RTP_DataFrame & packet)
{
  if (paused)
    return true;

#if OPAL_VIDEO
  if (m_video)
    return m_node->WriteVideo(GetID(), packet);
#endif

  return m_node->WriteAudio(GetID(), packet);
}


PBoolean OpalMixerMediaStream::IsSynchronous() const
{
  return false;
}


PBoolean OpalMixerMediaStream::RequiresPatchThread(OpalMediaStream *) const
{
  return !isSource;
}


///////////////////////////////////////////////////////////////////////////////

OpalMixerNode::OpalMixerNode(OpalMixerEndPoint & endpoint, OpalMixerNodeInfo * info)
  : m_endpoint(endpoint)
  , m_info(info != NULL ? info : new OpalMixerNodeInfo)
  , m_audioMixer(*m_info)
#if OPAL_VIDEO
  , m_videoMixer(*m_info)
#endif
{
  m_connections.DisallowDeleteObjects();

  AddName(m_info->m_name);

  PTRACE(4, "MixerNode\tConstructed " << m_guid);
}


OpalMixerNode::~OpalMixerNode()
{
  ShutDown(); // Fail safe

  delete m_info;
  PTRACE(4, "MixerNode\tDestroyed " << m_guid);
}


void OpalMixerNode::ShutDown()
{
  for (PSafePtr<OpalMixerConnection> connection = GetFirstConnection(); connection != NULL; ++connection)
    connection->Release();

  m_audioMixer.RemoveAllStreams();
#if OPAL_VIDEO
  m_videoMixer.RemoveAllStreams();
#endif

  for (PStringList::iterator iter = m_names.begin(); iter != m_names.end(); ++iter)
    m_endpoint.m_nodesByName.RemoveAt(*iter);
}


void OpalMixerNode::PrintOn(ostream & strm) const
{
  strm << m_guid << " (" << setfill(',') << m_names << ')';
}


void OpalMixerNode::AddName(const PString & name)
{
  if (name.IsEmpty())
    return;

  if (m_names.GetValuesIndex(name) != P_MAX_INDEX) {
    PTRACE(4, "MixerNode\tName \"" << name << "\" already added to " << m_guid);
    return;
  }

  PTRACE(4, "MixerNode\tAdding name \"" << name << "\" to " << m_guid);
  m_names.AppendString(name);
  m_endpoint.m_nodesByName.SetAt(name, this);
}


void OpalMixerNode::RemoveName(const PString & name)
{
  if (name.IsEmpty())
    return;

  PINDEX index = m_names.GetValuesIndex(name);
  if (index == P_MAX_INDEX) {
    PTRACE(4, "MixerNode\tName \"" << name << "\" not present in " << m_guid);
    return;
  }

  PTRACE(4, "MixerNode\tRemoving name \"" << name << "\" from " << m_guid);
  m_names.RemoveAt(index);
  m_endpoint.m_nodesByName.RemoveAt(name);
}


void OpalMixerNode::AttachConnection(OpalMixerConnection * connection)
{
  m_connections.Append(connection);
}


void OpalMixerNode::DetachConnection(OpalMixerConnection * connection)
{
  m_connections.Remove(connection);
}


bool OpalMixerNode::AttachStream(OpalMixerMediaStream * stream)
{
  PTRACE(4, "MixerNode\tAttaching " << stream->GetMediaFormat()
         << ' ' << (stream->IsSource() ? "source" : "sink")
         << " stream with id " << stream->GetID() << " to " << m_guid);

#if OPAL_VIDEO
  if (stream->GetMediaFormat().GetMediaType() == OpalMediaType::Video()) {
    if (stream->IsSink())
      return m_videoMixer.AddStream(stream->GetID());
    m_videoMixer.m_outputStreams.Append(stream);
    return true;
  }
#endif

  if (stream->IsSink())
    return m_audioMixer.AddStream(stream->GetID());

  OpalConnection & connection = stream->GetConnection();
  m_audioMixer.SetJitterBufferSize(stream->GetID(),
                                   connection.GetMinAudioJitterDelay(),
                                   connection.GetMaxAudioJitterDelay());
  m_audioMixer.m_outputStreams.Append(stream);
  return true;
}


void OpalMixerNode::DetachStream(OpalMixerMediaStream * stream)
{
  PTRACE(4, "MixerNode\tDetaching " << stream->GetMediaFormat()
         << ' ' << (stream->IsSource() ? "source" : "sink")
         << " stream with id " << stream->GetID() << " from " << m_guid);

#if OPAL_VIDEO
  if (stream->GetMediaFormat().GetMediaType() == OpalMediaType::Video()) {
    if (stream->IsSource())
      m_videoMixer.m_outputStreams.Remove(stream);
    else
      m_videoMixer.RemoveStream(stream->GetID());
    return;
  }
#endif

  if (stream->IsSource())
    m_audioMixer.m_outputStreams.Remove(stream);
  else
    m_audioMixer.RemoveStream(stream->GetID());
}


///////////////////////////////////////////////////////////////////////////////

// This dummy transcoder is so we don't try (and fail) to create a
// transcoder for every frame coming out of the mixer.
class OpalDummyTranscoder : public OpalTranscoder
{
    PCLASSINFO(OpalDummyTranscoder, OpalTranscoder);
  public:
    OpalDummyTranscoder(
      const OpalMediaFormat & inputMediaFormat,  ///<  Input media format
      const OpalMediaFormat & outputMediaFormat  ///<  Output media format
    ) : OpalTranscoder(inputMediaFormat, outputMediaFormat) { }
    virtual PINDEX GetOptimalDataFrameSize(PBoolean) const { return 0; }
    virtual PBoolean ConvertFrames(const RTP_DataFrame &, RTP_DataFrameList &) { return false; }
    virtual PBoolean Convert(const RTP_DataFrame &, RTP_DataFrame &) { return false; }
};


OpalMixerNode::MediaMixer::MediaMixer()
{
  m_outputStreams.DisallowDeleteObjects();
}


OpalTranscoder & OpalMixerNode::MediaMixer::GetTranscoder(const OpalMediaFormat & src,
                                                          const OpalMediaFormat & dst)
{
  OpalTranscoder * transcoder = m_transcoders.GetAt(dst);
  if (transcoder == NULL) {
    transcoder = OpalTranscoder::Create(src, dst);
    if (transcoder == NULL)
      transcoder = new OpalDummyTranscoder(src, dst);
    m_transcoders.SetAt(dst, transcoder);
  }
  return *transcoder;
}


///////////////////////////////////////////////////////////////////////////////

OpalMixerNode::AudioMixer::AudioMixer(const OpalMixerNodeInfo & info)
  : OpalAudioMixer(false, info.m_sampleRate)
{
}


OpalMixerNode::AudioMixer::~AudioMixer()
{
  StopPushThread();
}


void OpalMixerNode::AudioMixer::PushOne(OpalMixerMediaStream & stream,
                                        CachedAudio & cache,
                                        const short * audioToSubtract)
{
  switch (cache.m_state) {
    case CachedAudio::Collecting :
      MixAdditive(cache.m_raw, audioToSubtract);
      cache.m_state = CachedAudio::Collected;
      m_mutex.Signal();
      break;

    case CachedAudio::Collected :
      m_mutex.Signal();
      return;

    case CachedAudio::Completed :
      m_mutex.Signal();
      stream.PushPacket(cache.m_encoded);
      return;
  }

  OpalMediaFormat mediaFormat = stream.GetMediaFormat();
  if (mediaFormat == OpalPCM16) {
    if (cache.m_raw.GetPayloadSize() < stream.GetDataSize())
      return;

    cache.m_state = CachedAudio::Completed;
    stream.PushPacket(cache.m_raw);
    return;
  }

  OpalTranscoder & transcoder = GetTranscoder(OpalPCM16, mediaFormat);
  if (cache.m_raw.GetPayloadSize() < transcoder.GetOptimalDataFrameSize(true))
    return;

  if (cache.m_encoded.SetPayloadSize(transcoder.GetOptimalDataFrameSize(false)) &&
      transcoder.Convert(cache.m_raw, cache.m_encoded)) {
    cache.m_encoded.SetPayloadType(transcoder.GetPayloadType(false));
    cache.m_encoded.SetTimestamp(cache.m_raw.GetTimestamp());
    cache.m_state = CachedAudio::Completed;
    stream.PushPacket(cache.m_encoded);
  }
  else {
    PTRACE(2, "MixerNode\tCould not convert audio to "
           << mediaFormat << " for stream id " << stream.GetID());
    stream.Close();
  }
}


bool OpalMixerNode::AudioMixer::OnPush()
{
  m_mutex.Wait();
  PreMixStreams();
  m_mutex.Signal();

  for (PSafePtr<OpalMixerMediaStream> stream = m_outputStreams; stream != NULL; ++stream) {
    OpalMediaFormat mediaFormat = stream->GetMediaFormat();

    m_mutex.Wait(); // Signal() call for this mutex is inside PushOne()

    // Check for full participant, so can subtract their signal
    StreamMap_T::iterator inputStream = m_inputStreams.find(stream->GetID());
    if (inputStream != m_inputStreams.end())
      PushOne(*stream, m_cache[stream->GetID()], ((AudioStream *)inputStream->second)->m_cacheSamples);
    else {
      // Listen only participant, can use cached encoded audio
      PString encodedFrameKey = mediaFormat;
      encodedFrameKey.sprintf(":%u", stream->GetDataSize());
      PushOne(*stream, m_cache[encodedFrameKey], NULL);
    }
  }

  for (std::map<PString, CachedAudio>::iterator iterCache = m_cache.begin(); iterCache != m_cache.end(); ++iterCache) {
    switch (iterCache->second.m_state) {
      case CachedAudio::Collected :
        iterCache->second.m_state = CachedAudio::Collecting;
        break;

      case CachedAudio::Completed :
        iterCache->second.m_raw.SetPayloadSize(0);
        iterCache->second.m_encoded.SetPayloadSize(0);
        iterCache->second.m_state = CachedAudio::Collecting;
        break;

      default :
        break;
    }
  }

  m_outputTimestamp += m_periodTS;

  return true;
}


///////////////////////////////////////////////////////////////////////////////

#if OPAL_VIDEO
OpalMixerNode::VideoMixer::VideoMixer(const OpalMixerNodeInfo & info)
  : OpalVideoMixer(info.m_style, info.m_width, info.m_height, info.m_rate)
{
}


OpalMixerNode::VideoMixer::~VideoMixer()
{
  StopPushThread();
}


bool OpalMixerNode::VideoMixer::OnMixed(RTP_DataFrame * & output)
{
  std::map<OpalMediaFormat, RTP_DataFrameList> cachedVideo;

  for (PSafePtr<OpalMixerMediaStream> stream = m_outputStreams; stream != NULL; ++stream) {
    OpalMediaFormat mediaFormat = stream->GetMediaFormat();
    if (mediaFormat == OpalYUV420P)
      stream->PushPacket(*output);
    else {
      if (cachedVideo.find(mediaFormat) == cachedVideo.end()) {
        OpalTranscoder & transcoder = GetTranscoder(OpalYUV420P, mediaFormat);
        if (!transcoder.ConvertFrames(*output, cachedVideo[mediaFormat])) {
          PTRACE(2, "MixerNode\tCould not convert video to "
                 << mediaFormat << " for stream id " << stream->GetID());
          stream->Close();
          continue;
        }
      }

      RTP_DataFrameList & list = cachedVideo[mediaFormat];
      for (RTP_DataFrameList::iterator frame = list.begin(); frame != list.end(); ++frame)
        stream->PushPacket(*frame);
    }
  }

  return true;
}
#endif


//////////////////////////////////////////////////////////////////////////////
