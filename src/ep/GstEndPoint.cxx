/*
 * GstEndPoint.cxx
 *
 * GStreamer support.
 *
 * Open Phone Abstraction Library (OPAL)
 * Formally known as the Open H323 project.
 *
 * Copyright (c) 2013 Vox Lucida Pty. Ltd. and Jonathan M. Henson
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
 * The Initial Developer of the Original Code is Jonathan M. Henson
 * jonathan.michael.henson@gmail.com, jonathan.henson@innovisit.com
 *
 * Contributor(s): Robert Jongbloed (robertj@voxlucida.com.au).
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#include <ptlib.h>

#include <ep/GstEndPoint.h>
#include <codec/vidcodec.h>
#include <rtp/rtpconn.h>


#if OPAL_GSTREAMER

#define PTraceModule() "OpalGst"

#define APP_SOURCE_SUFFIX "AppSource"
#define APP_SINK_SUFFIX   "AppSink"

// Note the start of these constanst must be same as OpalMediaType::Audio() & OpalMediaType::Video()
const PString & GstEndPoint::GetPipelineRTPName()          { static PConstString const s("rtpbin");                  return s; }
const PString & GstEndPoint::GetPipelineAudioSourceName()  { static PConstString const s("audio" APP_SOURCE_SUFFIX); return s; }
const PString & GstEndPoint::GetPipelineAudioSinkName()    { static PConstString const s("audio" APP_SINK_SUFFIX  ); return s; }
const PString & GstEndPoint::GetPipelineJitterBufferName() { static PConstString const s("jitterBuffer");            return s; }
#if OPAL_VIDEO
const PString & GstEndPoint::GetPipelineVideoSourceName()  { static PConstString const s("video" APP_SOURCE_SUFFIX); return s; }
const PString & GstEndPoint::GetPipelineVideoSinkName()    { static PConstString const s("video" APP_SINK_SUFFIX  ); return s; }
#endif // OPAL_VIDEO


static const char * const PreferredAudioSourceDevice[] = {
#if _WIN32
//  "wasapisrc", // Does not work, no idea why
  "directsoundsrc",
  "dshowaudiosrc",
#else
  "alsasrc",
  "oss4src",
  "osssrc",
  "pulsesrc",
#endif
  "autoaudiosrc" // Note this is in desperation only, if used cannot set read buffer size
};


static const char * const PreferredAudioSinkDevice[] = {
#if _WIN32
//  "wasapisink", // Does not work, no idea why
  "directsoundsink",
  "dshowaudiosink",
#else
  "alsasink",
  "oss4sink",
  "osssink",
  "pulsesink",
#endif
  "autoaudiosink"
};


#if OPAL_VIDEO
static const char * const PreferredVideoSourceDevice[] = {
#if _WIN32
  "dshowvideosrc",
#else
  "video4linux2",
#endif
  "autovideosrc"
};


static const char * const PreferredVideoSinkDevice[] = {
#if _WIN32
  "directdrawsink",
  "dshowvideosink",
#else
  "xvimagesink",
#endif
  "sdlvideosink",
  "autovideosink"
};
#endif // OPAL_VIDEO


static struct GstInitInfo {
  const char * m_mediaFormat;
  const char * m_depacketiser;
  const char * m_decoder;
  const char * m_packetiser;
  const char * m_encoder;
} const DefaultMediaFormatToGStreamer[] = {
  { OPAL_G711_ULAW_64K, "rtppcmudepay",  "mulawdec",    "rtppcmupay",         "mulawenc"   },
  { OPAL_G711_ALAW_64K, "rtppcmadepay",  "alawdec",     "rtppcmapay",         "alawenc"    },
  { OPAL_GSM0610,       "rtpgsmdepay",   "gsmdec",      "rtpgsmpay",          "gsmenc"     },
  { OPAL_GSMAMR,        "rtpamrdepay",   "amrnbdec",    "rtpamrpay",          "amrnbenc"   },
  { OPAL_G7222,         "rtpamrdepay",   "amrwbdec",    "rtpamrpay",          "amrwbenc"   },
  { OPAL_G726_40K,      "rtpg726depay",  "ffdec_g726",  "rtpg726pay",         "ffenc_g726"
                                                                              " bitrate=40000"
  },
  { OPAL_G726_32K,      "rtpg726depay",  "ffdec_g726",  "rtpg726pay",         "ffenc_g726"
                                                                              " bitrate=32000"
  },
  { OPAL_G726_24K,      "rtpg726depay",  "ffdec_g726",  "rtpg726pay",         "ffenc_g726"
                                                                              " bitrate=24000"
  },
#if 0
  { "CELT-32K",         "rtpceltdepay",  "celtdec",     "rtpceltpay",         "celtenc"    },
  { "Speex",            "rtpspeexdepay", "speexdec",    "rtpspeexpay",        "speexenc"   },
#endif
#if OPAL_VIDEO
  { OPAL_H261,          "",              "ffdec_h261",  "",                   "ffenc_h261" },
  { OPAL_H263,          "rtph263depay",  "ffdec_h263",  "rtph263pay",         "ffenc_h263"
                                                                              " rtp-payload-size={"OPAL_GST_MTU"}"
                                                                              " max-key-interval=125"
                                                                              " me-method=5"
                                                                              " max-bframes=0"
                                                                              " gop-size=125"
                                                                              " rc-buffer-size=65536000"
                                                                              " rc-min-rate=0"
                                                                              " rc-max-rate={"OPAL_GST_BIT_RATE"}"
                                                                              " rc-qsquish=0"
                                                                              " rc-eq=1"
                                                                              " max-qdiff=10"
                                                                              " qcompress=0.5"
                                                                              " i-quant-factor=-0.6"
                                                                              " i-quant-offset=0.0"
                                                                              " me-subpel-quality=8"
                                                                              " qmin=2"
  },
  { OPAL_H263plus,      "rtph263pdepay", "ffdec_h263",  "rtph263ppay",        "ffenc_h263p"
                                                                             " rtp-payload-size={"OPAL_GST_MTU"}"
                                                                             " max-key-interval=125"
                                                                             " me-method=5"
                                                                             " max-bframes=0"
                                                                             " gop-size=125"
                                                                             " rc-buffer-size=65536000"
                                                                             " rc-min-rate=0"
                                                                             " rc-max-rate={"OPAL_GST_BIT_RATE"}"
                                                                             " rc-qsquish=0"
                                                                             " rc-eq=1"
                                                                             " max-qdiff=10"
                                                                             " qcompress=0.5"
                                                                             " i-quant-factor=-0.6"
                                                                             " i-quant-offset=0.0"
                                                                             " me-subpel-quality=8"
                                                                             " qmin=2"
  },
  { OPAL_H264_MODE1,    "rtph264depay",  "ffdec_h264",  "rtph264pay"
                                                        " config-interval=5", "x264enc"
                                                                              " byte-stream=true"
                                                                              " bframes=0"
                                                                              " b-adapt=0"
                                                                              " bitrate={"OPAL_GST_BIT_RATE_K"}"
                                                                              " tune=0x4"
                                                                              " speed-preset=3"
                                                                              " sliced-threads=false"
                                                                              " profile=0"
                                                                              " key-int-max=30"
  },
#if 0
  { "theora",         "rtptheoradepay", "theoradec",   "rtptheorapay",       "theoraenc"
                                                                             " name=videoEncoder"
                                                                             " keyframe-freq=125"
                                                                             " keyframe-force=125"
                                                                             " quality=16"
                                                                             " drop-frames=false"
                                                                             " bitrate=2048"
  },
#endif
  { OPAL_MPEG4,        "rtpmp4vdepay",   "ffdec_mpeg4", "rtpmp4vpay"
                                                        " send-config=true", "ffenc_mpeg4"
                                                                             " quantizer=0.8"
                                                                             " quant-type=1"
  }
#endif // OPAL_VIDEO
};


static PString GetDefaultDevice(const char * klass, const char * const * preferred, size_t count)
{
  PStringArray available = PGstPluginFeature::Inspect(klass, false);
  for (size_t i = 0; i < count; ++i) {
    PConstString pref(preferred[i]);
    if (available.GetValuesIndex(pref) != P_MAX_INDEX) {
      PTRACE(4, "Device set as preferred \"" << pref << "\", from possible:\n"
             << setfill('\n') << PGstPluginFeature::Inspect(klass, true));
      return pref;
    }
  }

  PTRACE(4, "Device set as first available: \"" << available[0] << "\", from possible:\n"
          << setfill('\n') << PGstPluginFeature::Inspect(klass, true));
  return available[0];
}


static PString FirstWord(const PString & str)
{
  return str.Left(str.FindOneOf(" \t\r\n"));
}


static void Substitute(PString & str, const char * name, const PString & value, bool required = false)
{
  PString sub(PString::Printf, OPAL_GST_STRINTF_FMT, name);
  if (str.Find(sub) != P_MAX_INDEX)
    str.Replace(sub, value, true);
  else if (required) {
    str &= name;
    str += '=';
    str += value;
  }
}


static PString SubstituteName(const PString & original, const char * name)
{
  PString str = original;
  Substitute(str, OPAL_GST_NAME, name, true);
  return str;
}


static PString SubstituteAll(const PString & original,
                             const GstMediaStream & stream,
                             const char * nameSuffix,
                             bool packetiser = false)
{
  PString str = original;

  OpalMediaFormat mediaFormat = stream.GetMediaFormat();

  Substitute(str, OPAL_GST_NAME, mediaFormat.GetMediaType() + nameSuffix, true);
  Substitute(str, OPAL_GST_SAMPLE_RATE, mediaFormat.GetClockRate());
  Substitute(str, OPAL_GST_PT, (unsigned)mediaFormat.GetPayloadType(), packetiser);
  
  unsigned bitrate = mediaFormat.GetOptionInteger(OpalMediaFormat::TargetBitRateOption(), mediaFormat.GetMaxBandwidth());
  Substitute(str, OPAL_GST_BIT_RATE, bitrate);
  Substitute(str, OPAL_GST_BIT_RATE_K, (bitrate+1023)/1024);
  if (mediaFormat.GetMediaType() == OpalMediaType::Audio()) {
    Substitute(str, OPAL_GST_BLOCK_SIZE, std::max(mediaFormat.GetFrameTime()*2, 320U));
    Substitute(str, OPAL_GST_LATENCY, stream.GetConnection().GetMaxAudioJitterDelay());
  }
  else if (mediaFormat.GetMediaType() == OpalMediaType::Video()) {
    Substitute(str, OPAL_GST_WIDTH, mediaFormat.GetOptionInteger(OpalVideoFormat::FrameWidthOption(), 352));
    Substitute(str, OPAL_GST_HEIGHT, mediaFormat.GetOptionInteger(OpalVideoFormat::FrameHeightOption(), 288));
    Substitute(str, OPAL_GST_FRAME_RATE, psprintf("(fraction)90000/%u", mediaFormat.GetFrameTime()));
    Substitute(str, OPAL_GST_MTU, mediaFormat.GetOptionInteger(OpalMediaFormat::MaxTxPacketSizeOption(), 1400), packetiser);
  }
  
  return str;
}


//////////////////////////////////////////////////////////

GstEndPoint::GstEndPoint(OpalManager & manager, const char *prefix)
  : OpalLocalEndPoint(manager, prefix, false)
  , m_rtpbin("gstrtpbin")
  , m_audioSourceDevice(GetDefaultDevice("Source/Audio", PreferredAudioSourceDevice, PARRAYSIZE(PreferredAudioSourceDevice)))
  , m_audioSinkDevice(GetDefaultDevice("Sink/Audio", PreferredAudioSinkDevice, PARRAYSIZE(PreferredAudioSinkDevice)))
  , m_jitterBuffer("gstrtpjitterbuffer latency={"OPAL_GST_LATENCY"}")
#if OPAL_VIDEO
  , m_videoSourceDevice(GetDefaultDevice("Source/Video", PreferredVideoSourceDevice, PARRAYSIZE(PreferredVideoSourceDevice)))
  , m_videoSinkDevice(GetDefaultDevice("Sink/Video", PreferredVideoSinkDevice, PARRAYSIZE(PreferredVideoSinkDevice)))
  , m_videoSourceColourConverter("autoconvert")
  , m_videoSinkColourConverter("autoconvert")
#endif // OPAL_VIDEO
{
  OpalMediaFormat::RegisterKnownMediaFormats(); // Make sure codecs are loaded

  PStringToString gstEncoderToMediaFormat;
  for (PINDEX i = 0; i < PARRAYSIZE(DefaultMediaFormatToGStreamer); ++i) {
    OpalMediaFormat mediaFormat(DefaultMediaFormatToGStreamer[i].m_mediaFormat);
    if (PAssert(mediaFormat.IsValid(), "Unknown media format")) {
      CodecPipelines & info = m_MediaFormatToGStreamer[mediaFormat];
      info.m_encoder      = DefaultMediaFormatToGStreamer[i].m_encoder;
      info.m_packetiser   = DefaultMediaFormatToGStreamer[i].m_packetiser;
      info.m_decoder      = DefaultMediaFormatToGStreamer[i].m_decoder;
      info.m_depacketiser = DefaultMediaFormatToGStreamer[i].m_depacketiser;
      gstEncoderToMediaFormat.SetAt(FirstWord(info.m_encoder), mediaFormat);
    }
  }

  PStringList encoders = PGstPluginFeature::Inspect("Codec/Encoder/Audio", false);
  PStringList decoders = PGstPluginFeature::Inspect("Codec/Decoder/Audio", false);
  PTRACE(4, "Audio encoder elements:\n" << setfill('\n') << PGstPluginFeature::Inspect("Codec/Encoder/Audio", true));
  PTRACE(4, "Audio decoder elements:\n" << setfill('\n') << PGstPluginFeature::Inspect("Codec/Decoder/Audio", true));

#if OPAL_VIDEO
  encoders += PGstPluginFeature::Inspect("Codec/Encoder/Video", false);
  decoders += PGstPluginFeature::Inspect("Codec/Decoder/Video", false);
  PTRACE(4, "Video encoder elements:\n" << setfill('\n') << PGstPluginFeature::Inspect("Codec/Encoder/Video", true));
  PTRACE(4, "Video decoder elements:\n" << setfill('\n') << PGstPluginFeature::Inspect("Codec/Decoder/Video", true));
#endif // OPAL_VIDEO

  for (PStringList::iterator enc = encoders.begin(); enc != encoders.end(); ++enc) {
    OpalMediaFormat mediaFormat = gstEncoderToMediaFormat(*enc);
    if (mediaFormat.IsValid()) {
      CodecPipelineMap::iterator inf = m_MediaFormatToGStreamer.find(mediaFormat);
      if (inf != m_MediaFormatToGStreamer.end()) {
        for (PStringList::iterator dec = decoders.begin(); dec != decoders.end(); ++dec) {
          if (inf->second.m_decoder.NumCompare(*dec) == EqualTo) {
            m_mediaFormatsAvailable += mediaFormat;
            break;
          }
        }
      }
    }
  }

  PTRACE(3, "Constructed GStream endpoint: media formats=" << setfill(',') << m_mediaFormatsAvailable);
}


GstEndPoint::~GstEndPoint()
{
  PTRACE(3, "Destroyed GStream endpoint.");
}


OpalMediaFormatList GstEndPoint::GetMediaFormats() const
{
  return m_mediaFormatsAvailable;
}


OpalLocalConnection * GstEndPoint::CreateConnection(OpalCall & call,
                                                        void * userData,
                                                    unsigned   options,
                               OpalConnection::StringOptions * stringOptions)
{
  return new GstConnection(call, *this, userData, options, stringOptions);
}


#if OPAL_VIDEO
bool GstEndPoint::BuildPipeline(ostream & description,
                                const GstMediaStream * audioStream,
                                const GstMediaStream * videoStream)
{
  if (!PAssert(audioStream != NULL || videoStream != NULL, PInvalidParameter))
    return false;

  int rtpIndex = 0;

  if (audioStream != NULL) {
    if (!BuildRTPPipeline(description, *audioStream, 0))
      rtpIndex = -2;

    if (audioStream->IsSource()) {
      if (!BuildAudioSourcePipeline(description, *audioStream, rtpIndex))
        return false;
    }
    else {
      if (!BuildAudioSinkPipeline(description, *audioStream, rtpIndex))
        return false;
    }

    ++rtpIndex;
  }

  if (videoStream != NULL) {
    if (!BuildRTPPipeline(description, *videoStream, rtpIndex))
      rtpIndex = -1;

    if (videoStream->IsSource()) {
      if (!BuildVideoSourcePipeline(description, *videoStream, rtpIndex))
        return false;
    }
    else {
      if (!BuildVideoSinkPipeline(description, *videoStream, rtpIndex))
        return false;
    }
  }

  return true;
}
#else //OPAL_VIDEO
bool GstEndPoint::BuildPipeline(ostream & description, const GstMediaStream * audioStream)
{
  if (!PAssert(audioStream == NULL, PInvalidParameter))
    return false;

  int rtpIndex = BuildRTPElement(description, *audioStream, 0) ? 0 : -1;

  if (stream.IsSource())
    return BuildAudioSourcePipeline(description, *audioStream, rtpIndex);
  else
    return BuildAudioSinkPipeline(description, *audioStream, rtpIndex);
}
#endif //OPAL_VIDEO


static void OutputRTPCaps(ostream & description, const OpalMediaFormat & mediaFormat)
{
  description << "application/x-rtp, "
                 "media=" << mediaFormat.GetMediaType() << ", "
                 "payload=" << (unsigned)mediaFormat.GetPayloadType() << ", "
                 "clock-rate=" << mediaFormat.GetClockRate() << ", "
                 "encoding-name=" << mediaFormat.GetEncodingName();
}


bool GstEndPoint::BuildRTPPipeline(ostream & description, const GstMediaStream & stream, unsigned index)
{
  if (m_rtpbin.IsEmpty())
    return false;

  OpalRTPConnection * rtpConnection = stream.GetConnection().GetOtherPartyConnectionAs<OpalRTPConnection>();
  if (rtpConnection == NULL)
    return false;

  OpalRTPSession * session = dynamic_cast<OpalRTPSession *>(rtpConnection->GetMediaSession(stream.GetSessionID()));
  if (session == NULL)
    return false;

  PIPSocket::Address host;
  if (!session->GetRemoteAddress().GetIpAddress(host))
    return false;

  P_INT_PTR rtpHandle = session->GetDataSocket().GetHandle();
  P_INT_PTR rtcpHandle = session->GetLocalDataPort() == session->GetLocalControlPort()
                            ? session->GetDataSocket().GetHandle() : session->GetControlSocket().GetHandle();

  if (index == 0)
    description << SubstituteName(m_rtpbin, GstEndPoint::GetPipelineRTPName()) << ' ';

  OpalMediaFormat mediaFormat = stream.GetMediaFormat();
  OpalMediaType mediaType = mediaFormat.GetMediaType();

  if (stream.IsSource())
    description << GstEndPoint::GetPipelineRTPName() << ".send_rtp_src_" << index
                << " ! "
                   "udpsink name=" << mediaType << "TxRTP "
                           "sockfd=" << rtpHandle << " "
                           "closefd=false "
                           "host=" << host << " "
                           "port=" << session->GetRemoteDataPort() << ' '
                << GstEndPoint::GetPipelineRTPName() << ".send_rtcp_src_" << index
                << " ! "
                   "udpsink name=" << mediaType << "TxRTCP "
                           "sync=false "
                           "async=false "
                           "sockfd=" << rtcpHandle << " "
                           "closefd=false "
                           "host=" << host << " "
                           "port=" << session->GetRemoteControlPort() << " "
                   "udpsrc name=" << mediaType << "RxRTCP "
                           "sockfd=" << rtcpHandle << " "
                           "closefd=false"
                   " ! "
                   "rtpbin.recv_rtcp_sink_" << index;
  else {
    description << "udpsrc name=" << mediaType << "RxRTP "
                          "sockfd=" << rtpHandle << " "
                          "closefd=false "
                          "caps=\"";
    OutputRTPCaps(description, mediaFormat);
    description << "\" ! "
                << GstEndPoint::GetPipelineRTPName() << ".recv_rtp_sink_" << index << " "
                   "udpsrc name=" << mediaType << "RxRTCP "
                          "sockfd=" << rtcpHandle << " "
                          "closefd=false"
                   " ! "
                << GstEndPoint::GetPipelineRTPName() << ".recv_rtcp_sink_" << index << ' '
                << GstEndPoint::GetPipelineRTPName() << ".send_rtcp_src_" << index
                << " ! "
                   "udpsink name=" << mediaType << "TxRTCP "
                           "sync=false "
                           "async=false "
                           "sockfd=" << rtcpHandle << " "
                           "closefd=false "
                           "host=" << host << " "
                           "port=" << session->GetRemoteControlPort();
  }

  description << ' ' << GstEndPoint::GetPipelineRTPName() + ". ! ";
  return true;
}


bool GstEndPoint::BuildAppSink(ostream & description, const PString & name, int rtpIndex)
{
  description << " ! ";
  if (rtpIndex < 0)
    description << "appsink name=" << name << " sync=false max-buffers=10 drop=false";
  else
    description << GstEndPoint::GetPipelineRTPName() << ".send_rtp_sink_" << rtpIndex;
  return true;
}


static void OutputRawAudioCaps(ostream & desc, const OpalMediaFormat & mediaFormat)
{
  desc << "audio/x-raw-int, "
          "depth=16, "
          "width=16, "
          "channels=" << mediaFormat.GetOptionInteger(OpalAudioFormat::ChannelsOption(), 1) << ", "
          "rate=" << mediaFormat.GetClockRate() << ", "
          "signed=true, "
          "endianness=1234";
}


bool GstEndPoint::BuildAudioSourcePipeline(ostream & description, const GstMediaStream & stream, int rtpIndex)
{
  if (!BuildAudioSourceDevice(description, stream))
    return false;

  OpalMediaFormat mediaFormat = stream.GetMediaFormat();

  description << " ! ";
  OutputRawAudioCaps(description, mediaFormat);

  if (mediaFormat != OpalPCM16) {
    description << " ! ";
    if (!BuildEncoder(description, stream))
      return false;
  }

  return BuildAppSink(description, GetPipelineAudioSinkName(), rtpIndex);
}


bool GstEndPoint::BuildAppSource(ostream & description, const PString & name)
{
  description << "appsrc stream-type=0 is-live=false name=" << name << " ! ";
  return true;
}


bool GstEndPoint::BuildJitterBufferPipeline(ostream & description, const GstMediaStream & stream)
{
  if (!m_jitterBuffer.IsEmpty())
    description << SubstituteAll(m_jitterBuffer, stream, "JitterBuffer") << " ! ";
  return true;
}


bool GstEndPoint::BuildAudioSinkPipeline(ostream & description, const GstMediaStream & stream, int rtpIndex)
{
  if (rtpIndex < 0) {
    if (!BuildAppSource(description, GetPipelineAudioSourceName()))
      return false;

    OpalMediaFormat mediaFormat = stream.GetMediaFormat();
    if (mediaFormat == OpalPCM16) {
      OutputRawAudioCaps(description, mediaFormat);
      description << " ! ";
      return BuildAudioSinkDevice(description, stream);
    }

    OutputRTPCaps(description, mediaFormat);
    description << " ! ";
  }

  if (!BuildJitterBufferPipeline(description, stream))
    return false;

  if (!BuildDecoder(description, stream))
    return false;

  description << " ! ";
  return BuildAudioSinkDevice(description, stream);
}


bool GstEndPoint::BuildAudioSourceDevice(ostream & description, const GstMediaStream & stream)
{
  description << SubstituteAll(m_audioSourceDevice, stream, "SourceDevice");
  return true;
}


bool GstEndPoint::BuildAudioSinkDevice(ostream & description, const GstMediaStream & stream)
{
  description << SubstituteAll(m_audioSinkDevice, stream, "SinkDevice");
  return true;
}


static bool SetValidElementName(PString & device, const PString & elementName PTRACE_PARAM(, const char * which))
{
  if (!elementName.IsEmpty() && PGstPluginFeature::Inspect(FirstWord(elementName), PGstPluginFeature::ByName).IsEmpty()) {
    PTRACE(2, "Could not find gstreamer " << which << " \"" << elementName << '"');
    return false;
  }

  PTRACE(4, "Set " << which << " to \"" << elementName << '"');
  device = elementName;
  return true;
}


bool GstEndPoint::SetJitterBufferPipeline(const PString & elementName)
{
  if (!elementName.IsEmpty())
    return SetValidElementName(m_jitterBuffer, elementName PTRACE_PARAM(, "jitter buffer"));

  m_jitterBuffer.MakeEmpty();
  return true;
}


bool GstEndPoint::SetRTPPipeline(const PString & elementName)
{
  if (!elementName.IsEmpty())
    return SetValidElementName(m_rtpbin, elementName PTRACE_PARAM(, "RTP"));

  m_rtpbin.MakeEmpty();
  return true;
}


bool GstEndPoint::SetAudioSourceDevice(const PString & elementName)
{
  return PAssert(!elementName.IsEmpty(), PInvalidParameter) &&
         SetValidElementName(m_audioSourceDevice, elementName PTRACE_PARAM(, "audio source"));
}


bool GstEndPoint::SetAudioSinkDevice(const PString & elementName)
{
  return PAssert(!elementName.IsEmpty(), PInvalidParameter) &&
         SetValidElementName(m_audioSinkDevice, elementName PTRACE_PARAM(, "audio sink"));
}


#if OPAL_VIDEO

static void OutputRawVideoCaps(ostream & desc, const OpalMediaFormat & mediaFormat)
{
  desc << "video/x-raw-yuv, "
          "format=(fourcc)I420, "
          "width=" << mediaFormat.GetOptionInteger(OpalVideoFormat::FrameWidthOption()) << ", "
          "height=" << mediaFormat.GetOptionInteger(OpalVideoFormat::FrameHeightOption()) << ", "
          "framerate=(fraction)90000/" << mediaFormat.GetFrameTime();
}


bool GstEndPoint::BuildVideoSourcePipeline(ostream & description, const GstMediaStream & stream, int rtpIndex)
{
  if (!BuildVideoSourceDevice(description, stream))
    return false;

  if (!m_videoSourceColourConverter.IsEmpty())
    description << " ! " << SubstituteName(m_videoSourceColourConverter, "videoSourceConverter");

  OpalMediaFormat mediaFormat = stream.GetMediaFormat();

  description << " ! ";
  OutputRawVideoCaps(description, mediaFormat);

  if (mediaFormat != OpalYUV420P) {
    description << " ! ";
    if (!BuildEncoder(description, stream))
      return false;
  }

  return BuildAppSink(description, GetPipelineVideoSinkName(), rtpIndex);
}


bool GstEndPoint::BuildVideoSinkPipeline(ostream & description, const GstMediaStream & stream, int rtpIndex)
{
  if (rtpIndex < 0) {
    if (!BuildAppSource(description, GetPipelineVideoSourceName()))
      return false;

    OpalMediaFormat mediaFormat = stream.GetMediaFormat();
    if (mediaFormat == OpalYUV420P)
      OutputRawVideoCaps(description, mediaFormat);
    else {
      OutputRTPCaps(description, mediaFormat);

      description << " ! ";
      if (!BuildDecoder(description, stream))
        return false;
    }
  }
  else {
    if (!BuildDecoder(description, stream))
      return false;
  }

  if (!m_videoSinkColourConverter.IsEmpty())
    description << " ! " << SubstituteName(m_videoSinkColourConverter, "videoSinkConverter");

  description << " ! ";
  return BuildVideoSinkDevice(description, stream);
}


bool GstEndPoint::BuildVideoSourceDevice(ostream & description, const GstMediaStream & stream)
{
  description << SubstituteAll(m_videoSourceDevice, stream, "SourceDevice");
  return true;
}


bool GstEndPoint::BuildVideoSinkDevice(ostream & description, const GstMediaStream & stream)
{
  description << SubstituteAll(m_videoSinkDevice, stream, "SinkDevice");
  return true;
}

bool GstEndPoint::SetVideoSourceDevice(const PString & elementName)
{
  return PAssert(!elementName.IsEmpty(), PInvalidParameter) &&
         SetValidElementName(m_videoSourceDevice, elementName PTRACE_PARAM(, "video source"));
}


bool GstEndPoint::SetVideoSinkDevice(const PString & elementName)
{
  return PAssert(!elementName.IsEmpty(), PInvalidParameter) &&
         SetValidElementName(m_videoSinkDevice, elementName PTRACE_PARAM(, "video sink"));
}


bool GstEndPoint::SetVideoSourceColourConverter(const PString & elementName)
{
  return SetValidElementName(m_videoSourceColourConverter, elementName PTRACE_PARAM(, "source colour converter"));
}


bool GstEndPoint::SetVideoSinkColourConverter(const PString & elementName)
{
  return SetValidElementName(m_videoSinkColourConverter, elementName PTRACE_PARAM(, "sink colour converter"));
}


#endif // OPAL_VIDEO


bool GstEndPoint::BuildEncoder(ostream & desc, const GstMediaStream & stream)
{
  CodecPipelineMap::iterator it = m_MediaFormatToGStreamer.find(stream.GetMediaFormat());
  if (it == m_MediaFormatToGStreamer.end())
    return false;

  desc << SubstituteAll(it->second.m_encoder, stream, "Encoder");
  if (!it->second.m_packetiser.IsEmpty())
    desc << " ! " << SubstituteAll(it->second.m_packetiser, stream, "Packetiser", true);

  return true;
}


bool GstEndPoint::BuildDecoder(ostream & desc, const GstMediaStream & stream)
{
  CodecPipelineMap::iterator it = m_MediaFormatToGStreamer.find(stream.GetMediaFormat());
  if (it == m_MediaFormatToGStreamer.end())
    return false;

  if (!it->second.m_depacketiser.IsEmpty())
    desc << SubstituteAll(it->second.m_depacketiser, stream, "Depacketiser") << " ! ";
  desc << SubstituteAll(it->second.m_decoder, stream, "Decoder");

  return true;
}


bool GstEndPoint::SetMapping(const OpalMediaFormat & mediaFormat, const CodecPipelines & info)
{
  if (!PAssert(mediaFormat.IsValid(), PInvalidParameter))
    return false;

  if (PGstPluginFeature::Inspect(FirstWord(info.m_encoder), PGstPluginFeature::ByName).IsEmpty()) {
    PTRACE(2, "Could not find gstreamer encoder \"" << info.m_encoder << '"');
    return false;
  }

  if (PGstPluginFeature::Inspect(FirstWord(info.m_decoder), PGstPluginFeature::ByName).IsEmpty()) {
    PTRACE(2, "Could not find gstreamer decoder \"" << info.m_decoder << '"');
    return false;
  }

  if (PGstPluginFeature::Inspect(FirstWord(info.m_packetiser), PGstPluginFeature::ByName).IsEmpty()) {
    PTRACE(2, "Could not find gstreamer packetiser \"" << info.m_packetiser << '"');
    return false;
  }

  if (PGstPluginFeature::Inspect(FirstWord(info.m_depacketiser), PGstPluginFeature::ByName).IsEmpty()) {
    PTRACE(2, "Could not find gstreamer depacketiser \"" << info.m_depacketiser << '"');
    return false;
  }

  m_mediaFormatsAvailable += mediaFormat;
  m_MediaFormatToGStreamer[mediaFormat] = info;
  PTRACE(4, "Added mapping from " << mediaFormat         << " -> "
                      "encoder=\"" << info.m_encoder      << "\", "
                      "decoder=\"" << info.m_decoder      << "\", "
                  "packetiser=\"" << info.m_packetiser   << "\", "
                "depacketiser=\"" << info.m_depacketiser << '"');
  return true;
}


bool GstEndPoint::GetMapping(const OpalMediaFormat & mediaFormat, CodecPipelines & info) const
{
  CodecPipelineMap::const_iterator it = m_MediaFormatToGStreamer.find(mediaFormat);
  if (it == m_MediaFormatToGStreamer.end())
    return false;

  info = it->second;
  return true;
}


///////////////////////////////////////////////////////////////////////////////

GstConnection::GstConnection(OpalCall & call,
                             GstEndPoint & ep,
                             void * userData,
                             unsigned options,
                             OpalConnection::StringOptions * stringOptions,
                             char tokenPrefix)
  : OpalLocalConnection(call, ep, userData, options, stringOptions, tokenPrefix)
  , m_endpoint(ep)
{
}


void GstConnection::OnReleased()
{
  OpalLocalConnection::OnReleased();
}


OpalMediaStream* GstConnection::CreateMediaStream(const OpalMediaFormat & mediaFormat,
                                                  unsigned sessionID,
                                                  PBoolean isSource)
{
  return new GstMediaStream(*this, mediaFormat, sessionID, isSource);
}


bool GstConnection::OpenPipeline(PGstPipeline & pipeline, const GstMediaStream & stream)
{
  PStringStream description;

  bool isSource = stream.IsSource();
  OpalMediaType mediaType = stream.GetMediaFormat().GetMediaType();

#if OPAL_VIDEO
  bool isAudio = mediaType == OpalMediaType::Audio();

  if (!isAudio && mediaType != OpalMediaType::Video()) {
    PTRACE(2, "Media type \"" << mediaType << "\" is not supported.");
    return false;
  }

  const GstMediaStream * audioStream = NULL;
  const GstMediaStream * videoStream = NULL;
  OpalMediaStreamPtr otherStream ;//= GetMediaStream(isAudio ? OpalMediaType::Video() : OpalMediaType::Audio(), isSource);
  if (otherStream == NULL)
    (isAudio ? audioStream : videoStream) = &stream;
  else {
    audioStream = isAudio ? &stream : dynamic_cast<const GstMediaStream *>(&*otherStream);
    videoStream = isAudio ? dynamic_cast<const GstMediaStream *>(&*otherStream) : &stream;
  }
  if (!m_endpoint.BuildPipeline(description, audioStream, videoStream))
    return false;
#else
  if (mediaType != OpalMediaType::Audio()) {
    PTRACE(2, "Media type \"" << mediaType << "\" is not supported.");
    return false;
  }

  if (!m_endpoint.BuildPipeline(description, &stream))
    return false;
#endif

  if (pipeline.IsValid()) {
    PTRACE(4, "Reconfiguring gstreamer pipeline for " << stream);
    if (!pipeline.SetState(PGstPipeline::Null, 1000)) {
      PTRACE(2, "Could not stop gstreamer pipeline for " << stream);
    }
  }

  if (!pipeline.Parse(description)) {
    PTRACE(2, "Failed to parse gstreamer pipeline: \"" << description << '"');
    return false;
  }

  pipeline.SetName(isSource ? "SourcePipeline" : "SinkPipeline");

#if PTRACING
  if (PTrace::CanTrace(5)) {
    ostream & trace = PTRACE_BEGIN(5);
    trace << "Pipeline elements:";
    PGstIterator<PGstElement> it;
    if (pipeline.GetElements(it, true)) {
      do {
        trace << ' ' << it->GetName();
      } while (++it);
    }
    trace << PTrace::End;
  }
#endif // PTRACING

  return true;
}


///////////////////////////////////////////////////////////////////////////////

GstMediaStream::GstMediaStream(GstConnection & conn,
                                 const OpalMediaFormat & mediaFormat,
                                 unsigned sessionID,
                                 bool isSource)
  : OpalMediaStream(conn, mediaFormat, sessionID, isSource)
  , m_connection(conn)
{
}


PBoolean GstMediaStream::Open()
{
  if (IsOpen())
    return true;

  if (!m_connection.OpenPipeline(m_pipeline, *this))
    return false;

  PGstBin rtpbin;
  if (m_pipeline.GetByName(GstEndPoint::GetPipelineRTPName(), rtpbin)) {
    PGstElement::States state;
    if (!StartPlaying(state))
      return false;
  }
  else {
    // Our source, their sink, and vice versa
    PString name = mediaFormat.GetMediaType();
    if (IsSource()) {
      name += APP_SINK_SUFFIX;
      if (!m_pipeline.GetByName(name, m_pipeSink)) {
        PTRACE(2, "Could not find " << name << " in pipeline for " << *this);
        return false;
      }
    }
    else {
      name += APP_SOURCE_SUFFIX;
      if (!m_pipeline.GetByName(name, m_pipeSource)) {
        PTRACE(2, "Could not find " << name << " in pipeline for " << *this);
        return false;
      }
    }
  }

  return OpalMediaStream::Open();
}


void GstMediaStream::InternalClose()
{
  PTRACE(4, "Stopping gstreamer pipeline.");
  m_pipeline.SetState(PGstPipeline::Null);
  m_pipeline.SetNULL();
}


PBoolean GstMediaStream::SetDataSize(PINDEX dataSize, PINDEX frameTime)
{
  if (IsSink() || mediaFormat.GetMediaType() != OpalMediaType::Audio())
    return OpalMediaStream::SetDataSize(dataSize, frameTime);

  PINDEX oldSize = GetDataSize();
  if (!OpalMediaStream::SetDataSize(frameTime*2, frameTime))
    return false;

  if (GetDataSize() == oldSize)
    return true;

#if 0
  // For some reason, doing this stops the pipeline dead in it's tracks.
  PGstElement srcDev;
  if (m_connection.GetPipeline(true).GetByName("audioSourceDevice", srcDev))
    srcDev.Set("blocksize", PString(GetDataSize()));
#endif

  return true;
}


PBoolean GstMediaStream::IsSynchronous() const
{
  return IsSource();
}


PBoolean GstMediaStream::RequiresPatchThread() const
{
  return m_pipeSink.IsValid() || m_pipeSource.IsValid();
}


PBoolean GstMediaStream::ReadPacket(RTP_DataFrame & packet)
{
  if (!IsOpen() || IsSink() || !m_pipeSink.IsValid())
    return false;

  PGstElement::States state;
  if (!StartPlaying(state))
    return false;

  if (state != PGstElement::Playing) {
    PTRACE(5, "Pipeline not ready yet for pulling, state=" << state);
    packet.SetPayloadSize(0);
    PThread::Sleep(10);
    return true;
  }

  PINDEX size = m_connection.GetEndPoint().GetManager().GetMaxRtpPacketSize();
  if (m_pipeSink.Pull(packet.GetPointer(size), size)) {
    packet.SetPacketSize(size);
    PTRACE(5, "Pulled " << size << " bytes, state=" << state << ", "
              "marker=" << packet.GetMarker() << " ts=" << packet.GetTimestamp());
    return true;
  }

  PTRACE(2, "Error pulling data to pipeline for " << *this);
  return false;
}


PBoolean GstMediaStream::WritePacket(RTP_DataFrame & packet)
{
  if (!IsOpen() || IsSource() || !m_pipeSource.IsValid())
    return false;

  PGstElement::States state;
  if (!StartPlaying(state))
    return false;

  PINDEX size = packet.GetHeaderSize() + packet.GetPayloadSize();
  PTRACE(5, "Pushing " << size << " bytes, state=" << state << ", "
            "marker=" << packet.GetMarker() << " ts=" << packet.GetTimestamp());
  if (m_pipeSource.Push(packet.GetPointer(), size))
    return true;

  PTRACE(2, "Error pushing " << size <<" bytes of data to pipeline for " << *this);
  return false;
}


bool GstMediaStream::StartPlaying(PGstElement::States & state)
{
  if (m_pipeline.GetState(state) == PGstElement::Failed) {
    PTRACE(2, "Pipeline has stopped for " << *this);
    return false;
  }

  if (state != PGstElement::Null)
    return true;

  PTRACE(3, "Starting pipeline for " << *this);
  if (m_pipeline.SetState(PGstElement::Playing) == PGstElement::Failed) {
    PTRACE(2, "Pipeline could not be started on " << *this);
    return false;
  }

  m_pipeline.GetState(state);
  return true;
}

 
#else // OPAL_GSTREAMER

  #ifdef _MSC_VER
    #pragma message("GStreamer support DISABLED")
  #endif

#endif // OPAL_GSTREAMER
