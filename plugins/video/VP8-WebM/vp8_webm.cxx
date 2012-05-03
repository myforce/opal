/*
 * VP8 (WebM) video codec Plugin codec for OPAL
 *
 * Copyright (C) 2012 Vox Lucida Pty Ltd, All Rights Reserved
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
 * The Original Code is OPAL Library.
 *
 * The Initial Developer of the Original Code is Vox Lucida Pty Ltd
 *
 * Added with funding from Requestec, Inc.
 *
 * Contributor(s): Robert Jongbloed (robertj@voxlucida.com.au)
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#ifndef PLUGIN_CODEC_DLL_EXPORTS
#include "plugin-config.h"
#endif

#include <codec/opalplugin.hpp>

#include <vector>

#ifdef _MSC_VER
#pragma warning(disable:4505)
#endif

#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"


#define MY_CODEC      VP8                       // Name of codec (use C variable characters)
#define MY_CODEC_LOG "VP8"

class MY_CODEC { };

static unsigned   MyVersion = PLUGIN_CODEC_VERSION_INTERSECT;
static const char MyDescription[] = "VP8 Video Codec";      // Human readable description of codec
static const char FormatNameRFC[] = "VP8-WebM";             // OpalMediaFormat name string to generate
static const char FormatNameOM[] = "VP8-OM";                // OpalMediaFormat name string to generate
static const char MyPayloadNameRFC[] = "VP8";               // RTP payload name (IANA approved)
static const char MyPayloadNameOM[] = "X-MX-VP8";           // RTP payload name (Open Market custom)
static unsigned   MyClockRate = 90000;                      // RTP dictates 90000
static unsigned   MyMaxFrameRate = 60;                      // Maximum frame rate (per second)
static unsigned   MyMaxWidth = 2816;                        // Maximum width of frame
static unsigned   MyMaxHeight = 2304;                       // Maximum height of frame
static unsigned   MyMaxBitRate = 16000000;

static const char YUV420PFormatName[] = "YUV420P";          // Raw media format


static struct PluginCodec_information LicenseInfo = {
  1143692893,                                                   // timestamp = Thu 30 Mar 2006 04:28:13 AM UTC

  "Robert Jongbloed, Vox Lucida Pty.Ltd.",                      // source code author
  "1.0",                                                        // source code version
  "robertj@voxlucida.com.au",                                   // source code email
  "http://www.voxlucida.com.au",                                // source code URL
  "Copyright (C) 2011 by Vox Lucida Pt.Ltd., All Rights Reserved", // source code copyright
  "MPL 1.0",                                                    // source code license
  PluginCodec_License_MPL,                                      // source code license
  
  MyDescription,                                                // codec description
  "James Bankoski, John Koleszar, Lou Quillio, Janne Salonen, "
  "Paul Wilkins, Yaowu Xu, all from Google Inc.",               // codec author
  "8", 							        // codec version
  "jimbankoski@google.com, jkoleszar@google.com, "
  "louquillio@google.com, jsalonen@google.com, "
  "paulwilkins@google.com, yaowu@google.com",                   // codec email
  "http://google.com",                                          // codec URL
  "Copyright (c) 2010, 2011, Google Inc.",                      // codec copyright information
  "Copyright (c) 2010, 2011, Google Inc.",                      // codec license
  PluginCodec_License_BSD                                       // codec license code
};


PLUGINCODEC_CONTROL_LOG_FUNCTION_DEF


///////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_Option const TemporalSpatialTradeOff =
{
  PluginCodec_IntegerOption,          // Option type
  PLUGINCODEC_OPTION_TEMPORAL_SPATIAL_TRADE_OFF, // User visible name
  false,                              // User Read/Only flag
  PluginCodec_AlwaysMerge,            // Merge mode
  "31",                               // Initial value
  NULL,                               // FMTP option name
  NULL,                               // FMTP default value
  0,                                  // H.245 generic capability code and bit mask
  "1",                                // Minimum value
  "31"                                // Maximum value
};

static struct PluginCodec_Option const * OptionTable[] = {
  &TemporalSpatialTradeOff,
  NULL
};


static bool IsError(vpx_codec_err_t err, const char * fn)
{
  if (err == VPX_CODEC_OK)
    return false;

  PTRACE(1, MY_CODEC_LOG, "Error " << err << " in " << fn << " - " << vpx_codec_err_to_string(err));
  return true;
}

#define IS_ERROR(fn, args) IsError(fn args, #fn)


///////////////////////////////////////////////////////////////////////////////

class MyPluginMediaFormat : public PluginCodec_MediaFormat
{
public:
  MyPluginMediaFormat(OptionsTable options)
    : PluginCodec_MediaFormat(options)
  {
  }


  virtual bool ToNormalised(OptionMap &, OptionMap & )
  {
    return true;
  }


  virtual bool ToCustomised(OptionMap &, OptionMap &)
  {
    return true;
  }
}; 

static MyPluginMediaFormat MyMediaFormatInfo(OptionTable);


///////////////////////////////////////////////////////////////////////////////

class MyEncoder : public PluginVideoEncoder<MY_CODEC>
{
    typedef PluginVideoEncoder<MY_CODEC> BaseClass;

  protected:
    vpx_codec_enc_cfg_t        m_config;
    vpx_codec_ctx_t            m_codec;
    vpx_codec_iter_t           m_iterator;
    const vpx_codec_cx_pkt_t * m_packet;
    size_t                     m_offset;

  public:
    MyEncoder(const PluginCodec_Definition * defn)
      : BaseClass(defn)
      , m_iterator(NULL)
      , m_packet(NULL)
      , m_offset(0)
    {
      memset(&m_codec, 0, sizeof(m_codec));
    }


    ~MyEncoder()
    {
      vpx_codec_destroy(&m_codec);
    }


    virtual bool Construct()
    {
      if (IS_ERROR(vpx_codec_enc_config_default, (vpx_codec_vp8_cx(), &m_config, 0)))
        return false;

      m_maxBitRate = m_config.rc_target_bitrate*1000;

      m_config.g_w = 0; // Forces OnChangedOptions to initialise encoder
      m_config.g_h = 0;
      m_config.g_error_resilient = 1;
      m_config.g_pass = VPX_RC_ONE_PASS;
      m_config.g_lag_in_frames = 0;
      m_config.rc_end_usage = VPX_CBR;
      m_config.g_timebase.num = 1;
      m_config.g_timebase.den = MyClockRate;

      if (!OnChangedOptions())
        return false;

      PTRACE(4, MY_CODEC_LOG, "Encoder opened: " << vpx_codec_version_str());
      return true;
    }


    virtual bool OnChangedOptions()
    {
      if (m_keyFramePeriod != 0) {
        m_config.kf_mode = VPX_KF_DISABLED;
        m_config.kf_min_dist = m_config.kf_max_dist = m_keyFramePeriod;
      }
      else {
        m_config.kf_mode = VPX_KF_AUTO;
        m_config.kf_min_dist = 0;
        m_config.kf_max_dist = 10*MyClockRate/m_frameTime;  // No slower than once every 10 seconds
      }

      m_config.rc_target_bitrate = m_maxBitRate/1000;

      // Take simple temporal/spatial trade off and set multiple variables
      m_config.rc_dropframe_thresh = (31-m_tsto)*2; // m_tsto==31 is maintain frame rate, so threshold is zero
      m_config.rc_resize_allowed = m_tsto < 16;
      m_config.rc_max_quantizer = 32 + m_tsto;

      if (m_config.g_w == m_width && m_config.g_h == m_height)
        return !IS_ERROR(vpx_codec_enc_config_set, (&m_codec, &m_config));

      m_config.g_w = m_width;
      m_config.g_h = m_height;
      vpx_codec_destroy, (&m_codec);
      return !IS_ERROR(vpx_codec_enc_init, (&m_codec, vpx_codec_vp8_cx(), &m_config, 0));
    }


    bool NeedEncode()
    {
      if (m_packet != NULL)
        return false;

      while ((m_packet = vpx_codec_get_cx_data(&m_codec, &m_iterator)) != NULL) {
        if (m_packet->kind == VPX_CODEC_CX_FRAME_PKT)
          return false;
      }

      m_iterator = NULL;
      return true;
    }


    virtual bool Transcode(const void * fromPtr,
                             unsigned & fromLen,
                                 void * toPtr,
                             unsigned & toLen,
                             unsigned & flags)
    {
      while (NeedEncode()) {
        PluginCodec_RTP srcRTP(fromPtr, fromLen);
        PluginCodec_Video_FrameHeader * video = srcRTP.GetVideoHeader();

        vpx_image_t image;
        vpx_img_wrap(&image, VPX_IMG_FMT_I420, video->width, video->height, 4, srcRTP.GetVideoFrameData());

        if (IS_ERROR(vpx_codec_encode, (&m_codec, &image,
                                        srcRTP.GetTimestamp(), m_frameTime,
                                        (flags&PluginCodec_CoderForceIFrame) != 0 ? VPX_EFLAG_FORCE_KF : 0,
                                        VPX_DL_REALTIME)))
          return false;
      }

      if ((m_packet->data.frame.flags&VPX_FRAME_IS_KEY) != 0)
        flags |= PluginCodec_ReturnCoderIFrame;

      PluginCodec_RTP dstRTP(toPtr, toLen);
      Packetise(dstRTP);
      toLen = dstRTP.GetPacketSize();

      if (m_offset >= m_packet->data.frame.sz) {
        flags |= PluginCodec_ReturnCoderLastFrame;
        dstRTP.SetMarker(true);
        m_packet = NULL;
        m_offset = 0;
      }

      return true;
    }


    virtual void Packetise(PluginCodec_RTP & rtp) = 0;
};


class MyEncoderRFC : public MyEncoder
{
  public:
    MyEncoderRFC(const PluginCodec_Definition * defn)
      : MyEncoder(defn)
    {
    }


    virtual void Packetise(PluginCodec_RTP & rtp)
    {
      size_t headerSize = 2;

      rtp[0] = rtp[1] = 0;

      if (m_offset == 0)
        rtp[0] |= 0x10;

      if ((m_packet->data.frame.flags&VPX_FRAME_IS_KEY) != 0) {
        rtp[0] |= 0x20;
        rtp[1] |= 0x01;
      }

      size_t fragmentSize = GetPacketSpace(rtp, m_packet->data.frame.sz - m_offset + headerSize) - headerSize;
      rtp.CopyPayload((char *)m_packet->data.frame.buf+m_offset, fragmentSize, headerSize);
      m_offset += fragmentSize;
    }
};


class MyEncoderOM : public MyEncoder
{
  protected:
    unsigned m_currentGID;

  public:
    MyEncoderOM(const PluginCodec_Definition * defn)
      : MyEncoder(defn)
      , m_currentGID(0)
    {
    }


    virtual void Packetise(PluginCodec_RTP & rtp)
    {
      size_t headerSize;
      if (m_offset != 0)
        headerSize = 1;
      else {
        headerSize = 2;

        rtp[0] = 0x40; // Start bit
        rtp[1] = 0x40; // LANDSCAPE_UP mode

        if ((m_packet->data.frame.flags&VPX_FRAME_IS_KEY) != 0) {
          rtp[1] |= 0x80; // Indicate is golden frame
          m_currentGID = (m_currentGID+1)&0x3f;
        }
      }

      rtp[0] |= m_currentGID;

      size_t fragmentSize = GetPacketSpace(rtp, m_packet->data.frame.sz - m_offset + headerSize) - headerSize;
      rtp.CopyPayload((char *)m_packet->data.frame.buf+m_offset, fragmentSize, headerSize);
      m_offset += fragmentSize;
    }
};


///////////////////////////////////////////////////////////////////////////////

class MyDecoder : public PluginVideoDecoder<MY_CODEC>
{
    typedef PluginVideoDecoder<MY_CODEC> BaseClass;

  protected:
    vpx_codec_ctx_t      m_codec;
    vpx_codec_iter_t     m_iterator;
    std::vector<uint8_t> m_fullFrame;

  public:
    MyDecoder(const PluginCodec_Definition * defn)
      : BaseClass(defn)
      , m_iterator(NULL)
    {
      memset(&m_codec, 0, sizeof(m_codec));
      m_fullFrame.reserve(10000);
    }


    ~MyDecoder()
    {
      vpx_codec_destroy(&m_codec);
    }


    virtual bool Construct()
    {
      if (IS_ERROR(vpx_codec_dec_init, (&m_codec, vpx_codec_vp8_dx(), NULL, 0)))
        return false;

      PTRACE(4, MY_CODEC_LOG, "Decoder opened: " << vpx_codec_version_str());
      return true;
    }


    virtual bool Transcode(const void * fromPtr,
                             unsigned & fromLen,
                                 void * toPtr,
                             unsigned & toLen,
                             unsigned & flags)
    {
      vpx_image_t * image;

      if ((image = vpx_codec_get_frame(&m_codec, &m_iterator)) == NULL) {

        PluginCodec_RTP srcRTP(fromPtr, fromLen);
        if (!Unpacketise(srcRTP)) {
          flags |= PluginCodec_ReturnCoderRequestIFrame;
          return true;
        }

        if (!srcRTP.GetMarker())
          return true;

        vpx_codec_err_t err = vpx_codec_decode(&m_codec, &m_fullFrame[0], m_fullFrame.size(), NULL, 0);
        switch (err) {
          case VPX_CODEC_OK :
            break;

          // Non fatal errors
          case VPX_CODEC_UNSUP_FEATURE :
          case VPX_CODEC_CORRUPT_FRAME :
            flags |= PluginCodec_ReturnCoderRequestIFrame;
            break;

          default :
            IsError(err, "vpx_codec_decode");
            return false;
        }

#if 1
        /* Prefer to use vpx_codec_get_stream_info() here, but it doesn't
           work, it always returns key frame! The vpx_codec_peek_stream_info()
           function is also useless, it return VPX_CODEC_UNSUP_FEATURE for
           anything that isn't an I-Frame. Luckily it appears that the low bit
           of the first byte is the I-Frame indicator.
        */
        if ((m_fullFrame[0]&1) == 0)
          flags |= PluginCodec_ReturnCoderIFrame;
#else
        vpx_codec_stream_info_t info;
        info.sz = sizeof(info);
        if (IS_ERROR(vpx_codec_get_stream_info, (&m_codec, &info)))
          flags |= PluginCodec_ReturnCoderRequestIFrame;

        if (info.is_kf)
          flags |= PluginCodec_ReturnCoderIFrame;
#endif

        m_fullFrame.clear();

        m_iterator = NULL;
        if ((image = vpx_codec_get_frame(&m_codec, &m_iterator)) == NULL)
          return true;
      }

      if (image->fmt != VPX_IMG_FMT_I420) {
        PTRACE(1, MY_CODEC_LOG, "Unsupported image format from decoder.");
        return false;
      }

      PluginCodec_RTP dstRTP(toPtr, toLen);
      OutputImage(image->planes, image->stride, image->d_w, image->d_h, dstRTP, flags);
      toLen = dstRTP.GetPacketSize();
      return true;
    }


    void Accumulate(const unsigned char * fragmentPtr, size_t fragmentLen)
    {
      size_t size = m_fullFrame.size();
      m_fullFrame.reserve(size+fragmentLen*2);
      m_fullFrame.resize(size+fragmentLen);
      memcpy(&m_fullFrame[size], fragmentPtr, fragmentLen);
    }


    virtual bool Unpacketise(const PluginCodec_RTP & rtp) = 0;
};


class MyDecoderRFC : public MyDecoder
{
  public:
    MyDecoderRFC(const PluginCodec_Definition * defn)
      : MyDecoder(defn)
    {
    }


    virtual bool Unpacketise(const PluginCodec_RTP & rtp)
    {
      if (rtp.GetPayloadSize() == 0)
        return true;

      if (rtp.GetPayloadSize() < 3)
        return false;

      size_t headerSize = 1;
      if ((rtp[0]&0x80) != 0) {
        ++headerSize;
        if ((rtp[1]&0x80) != 0)
          ++headerSize;
        if ((rtp[1]&0x40) != 0)
          ++headerSize;
        if ((rtp[1]&0x20) != 0)
          ++headerSize;
        if ((rtp[1]&0x10) != 0)
          ++headerSize;
      }

      Accumulate(rtp.GetPayloadPtr()+headerSize, rtp.GetPayloadSize()-headerSize);
      return true;
    }
};


class MyDecoderOM : public MyDecoder
{
  protected:
    unsigned m_expectedGID;
    bool     m_ignoreTillFirst;

  public:
    MyDecoderOM(const PluginCodec_Definition * defn)
      : MyDecoder(defn)
      , m_expectedGID(0)
      , m_ignoreTillFirst(false)
    {
    }


    virtual bool Unpacketise(const PluginCodec_RTP & rtp)
    {
      if (rtp.GetPayloadSize() == 0)
        return true;

      if (rtp.GetPayloadSize() < 3)
        return false;

      bool first = (rtp[0]&0x40) != 0;

      size_t headerSize = first ? 2 : 1;
      if ((rtp[0]&0x80) != 0) {
        while ((rtp[headerSize]&0x80) != 0)
          ++headerSize;
      }

      if (m_ignoreTillFirst) {
        if (!first)
          return false;
        m_ignoreTillFirst =  false;
        PTRACE(3, MY_CODEC_LOG, "Found next start of frame.");
      }
      else {
        if (!first && m_fullFrame.empty()) {
          PTRACE(3, MY_CODEC_LOG, "Missing start to frame, ignoring till next.");
          m_ignoreTillFirst = true;
          return false;
        }
      }

      Accumulate(rtp.GetPayloadPtr()+headerSize, rtp.GetPayloadSize()-headerSize);

      unsigned gid = rtp[0]&0x3f;
      if (m_expectedGID == gid)
        return true;

      m_expectedGID = gid;
      return false;
    }
};


///////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_Definition MyCodecDefinition[] =
{
  {
    // Encoder
    MyVersion,                          // codec API version
    &LicenseInfo,                       // license information

    PluginCodec_MediaTypeVideo |        // audio codec
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    MyDescription,                      // text decription
    YUV420PFormatName,                  // source format
    FormatNameRFC,                      // destination format

    &MyMediaFormatInfo,                 // user data 

    MyClockRate,                        // samples per second
    MyMaxBitRate,                       // raw bits per second
    1000000/MyMaxFrameRate,             // microseconds per frame

    {{
      MyMaxWidth,                       // frame width
      MyMaxHeight,                      // frame height
      MyMaxFrameRate,                   // recommended frame rate
      MyMaxFrameRate                    // maximum frame rate
    }},
    
    0,                                  // IANA RTP payload code
    MyPayloadNameRFC,                   // IANA RTP payload name

    PluginCodec<MY_CODEC>::Create<MyEncoderRFC>,  // create codec function
    PluginCodec<MY_CODEC>::Destroy,               // destroy codec
    PluginCodec<MY_CODEC>::Transcode,             // encode/decode
    PluginCodec<MY_CODEC>::GetControls(),         // codec controls

    PluginCodec_H323Codec_NoH323,       // h323CapabilityType 
    NULL                                // h323CapabilityData
  },
  { 
    // Decoder
    MyVersion,                          // codec API version
    &LicenseInfo,                       // license information

    PluginCodec_MediaTypeVideo |        // audio codec
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    MyDescription,                      // text decription
    FormatNameRFC,                      // source format
    YUV420PFormatName,                  // destination format

    &MyMediaFormatInfo,                 // user data 

    MyClockRate,                        // samples per second
    MyMaxBitRate,                       // raw bits per second
    1000000/MyMaxFrameRate,             // microseconds per frame

    {{
      MyMaxWidth,                       // frame width
      MyMaxHeight,                      // frame height
      MyMaxFrameRate,                   // recommended frame rate
      MyMaxFrameRate                    // maximum frame rate
    }},
    
    0,                                  // IANA RTP payload code
    MyPayloadNameRFC,                   // IANA RTP payload name

    PluginCodec<MY_CODEC>::Create<MyDecoderRFC>,  // create codec function
    PluginCodec<MY_CODEC>::Destroy,               // destroy codec
    PluginCodec<MY_CODEC>::Transcode,             // encode/decode
    PluginCodec<MY_CODEC>::GetControls(),         // codec controls

    PluginCodec_H323Codec_NoH323,       // h323CapabilityType 
    NULL                                // h323CapabilityData
  },
  {
    // Encoder
    MyVersion,                          // codec API version
    &LicenseInfo,                       // license information

    PluginCodec_MediaTypeVideo |        // audio codec
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    MyDescription,                      // text decription
    YUV420PFormatName,                  // source format
    FormatNameOM,                       // destination format

    &MyMediaFormatInfo,                 // user data 

    MyClockRate,                        // samples per second
    MyMaxBitRate,                       // raw bits per second
    1000000/MyMaxFrameRate,             // microseconds per frame

    {{
      MyMaxWidth,                       // frame width
      MyMaxHeight,                      // frame height
      MyMaxFrameRate,                   // recommended frame rate
      MyMaxFrameRate                    // maximum frame rate
    }},
    
    0,                                  // IANA RTP payload code
    MyPayloadNameOM,                    // IANA RTP payload name

    PluginCodec<MY_CODEC>::Create<MyEncoderOM>,   // create codec function
    PluginCodec<MY_CODEC>::Destroy,               // destroy codec
    PluginCodec<MY_CODEC>::Transcode,             // encode/decode
    PluginCodec<MY_CODEC>::GetControls(),         // codec controls

    PluginCodec_H323Codec_NoH323,       // h323CapabilityType 
    NULL                                // h323CapabilityData
  },
  { 
    // Decoder
    MyVersion,                          // codec API version
    &LicenseInfo,                       // license information

    PluginCodec_MediaTypeVideo |        // audio codec
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    MyDescription,                      // text decription
    FormatNameOM,                       // source format
    YUV420PFormatName,                  // destination format

    &MyMediaFormatInfo,                 // user data 

    MyClockRate,                        // samples per second
    MyMaxBitRate,                       // raw bits per second
    1000000/MyMaxFrameRate,             // microseconds per frame

    {{
      MyMaxWidth,                       // frame width
      MyMaxHeight,                      // frame height
      MyMaxFrameRate,                   // recommended frame rate
      MyMaxFrameRate                    // maximum frame rate
    }},
    
    0,                                  // IANA RTP payload code
    MyPayloadNameOM,                    // IANA RTP payload name

    PluginCodec<MY_CODEC>::Create<MyDecoderOM>,   // create codec function
    PluginCodec<MY_CODEC>::Destroy,               // destroy codec
    PluginCodec<MY_CODEC>::Transcode,             // encode/decode
    PluginCodec<MY_CODEC>::GetControls(),         // codec controls

    PluginCodec_H323Codec_NoH323,       // h323CapabilityType 
    NULL                                // h323CapabilityData
  }
};

PLUGIN_CODEC_IMPLEMENT_CXX(MY_CODEC, MyCodecDefinition);


/////////////////////////////////////////////////////////////////////////////
