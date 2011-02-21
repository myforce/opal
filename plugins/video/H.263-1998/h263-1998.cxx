/*
 * H.263 Plugin codec for OpenH323/OPAL
 *
 * This code is based on the following files from the OPAL project which
 * have been removed from the current build and distributions but are still
 * available in the CVS "attic"
 * 
 *    src/codecs/h263codec.cxx 
 *    include/codecs/h263codec.h 

 * The original files, and this version of the original code, are released under the same 
 * MPL 1.0 license. Substantial portions of the original code were contributed
 * by Salyens and March Networks and their right to be identified as copyright holders
 * of the original code portions and any parts now included in this new copy is asserted through 
 * their inclusion in the copyright notices below.
 *
 * Copyright (C) 2007 Matthias Schneider
 * Copyright (C) 2006 Post Increment
 * Copyright (C) 2005 Salyens
 * Copyright (C) 2001 March Networks Corporation
 * Copyright (C) 1999-2000 Equivalence Pty. Ltd.
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
 * Contributor(s): Matthias Schneider (ma30002000@yahoo.de)
 *                 Guilhem Tardy (gtardy@salyens.com)
 *                 Craig Southeren (craigs@postincrement.com)
 *
 */

/*
  Notes
  -----

 */

#ifndef PLUGIN_CODEC_DLL_EXPORTS
#include "plugin-config.h"
#endif

#define _CRT_SECURE_NO_DEPRECATE

#define __STDC_CONSTANT_MACROS

#include "h263-1998.h"
#include <limits>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "../common/mpi.h"
#include "../common/ffmpeg.h"
#include "../common/dyna.h"

#include "tracer.h"

DECLARE_TRACER

extern "C" {
#include LIBAVCODEC_HEADER
};

static const char * h263_Prefix = "H.263";
static const char * h263P_Prefix = "H.263+";

static const char YUV420PDesc[]  = { "YUV420P" };
static const char h263PDesc[]    = { "H.263P" };
static const char sdpH263P[]     = { "h263-1998" };

static const char h263Desc[]     = { "H.263" };
static const char sdpH263[]      = { "h263" };

static struct StdSizes {
  enum { 
    SQCIF, 
    QCIF, 
    CIF, 
    CIF4, 
    CIF16, 
    NumStdSizes,
    UnknownStdSize = NumStdSizes
  };

  int width;
  int height;
  const char * optionName;
} StandardVideoSizes[StdSizes::NumStdSizes] = {
  { SQCIF_WIDTH, SQCIF_HEIGHT, PLUGINCODEC_SQCIF_MPI },
  {  QCIF_WIDTH,  QCIF_HEIGHT, PLUGINCODEC_QCIF_MPI  },
  {   CIF_WIDTH,   CIF_HEIGHT, PLUGINCODEC_CIF_MPI   },
  {  CIF4_WIDTH,  CIF4_HEIGHT, PLUGINCODEC_CIF4_MPI  },
  { CIF16_WIDTH, CIF16_HEIGHT, PLUGINCODEC_CIF16_MPI },
};

static FFMPEGLibrary FFMPEGLibraryInstance(CODEC_ID_H263P);


/////////////////////////////////////////////////////////////////////////////

static char * num2str(int num)
{
  char buf[20];
  sprintf(buf, "%i", num);
  return strdup(buf);
}

#if TRACE_FILE

static void DumpRTPPayload(Tracer & tracer, const RTPFrame & rtp, int max)
{
  if (max > rtp.GetPayloadSize())
    max = rtp.GetPayloadSize();
  unsigned char * ptr = rtp.GetPayloadPtr();
  tracer.GetStream() << hex << setfill('0') << setprecision(2);
  while (max-- > 0) 
    tracer.GetStream() << (int) *ptr++ << ' ';
  tracer.GetStream() << setfill(' ') << dec;
}

static ostream & RTPDump(Tracer & tracer, const RTPFrame & rtp)
{
  tracer.GetStream() << "seq=" << rtp.GetSequenceNumber()
       << ",ts=" << rtp.GetTimestamp()
       << ",mkr=" << rtp.GetMarker()
       << ",pt=" << (int)rtp.GetPayloadType()
       << ",ps=" << rtp.GetPayloadSize();
  return tracer.GetStream();
}

static ostream & RFC2190Dump(Tracer & tracer, const RTPFrame & rtp)
{
  RTPDump(tracer, rtp);
  if (rtp.GetPayloadSize() > 2) {
    bool iFrame = false;
    char mode;
    unsigned char * payload = rtp.GetPayloadPtr();
    if ((payload[0] & 0x80) == 0) {
      mode = 'A';
      iFrame = (payload[1] & 0x10) == 0;
    }
    else if ((payload[0] & 0x40) == 0) {
      mode = 'B';
      iFrame = (payload[4] & 0x80) == 0;
    }
    else {
      mode = 'C';
      iFrame = (payload[4] & 0x80) == 0;
    }
    tracer.GetStream() << "mode=" << mode << ",I=" << (iFrame ? "yes" : "no");
  }
  tracer.GetStream() << ",data=";
  DumpRTPPayload(tracer, rtp, 10);
  return tracer.GetStream();
}

static ostream & RFC2429Dump(Tracer & tracer, const RTPFrame & rtp)
{
  RTPDump(tracer, rtp);
  tracer.GetStream() << ",data=";
  DumpRTPPayload(tracer, rtp, 10);
  return tracer.GetStream();
}

#define CODEC_TRACER_RTP(tracer, text, rtp, func) \
tracer.Start(); tracer.GetStream() << text; func(tracer, rtp); tracer.End()

#else

#define CODEC_TRACER_RTP(tracer, text, rtp, func) 

#endif
      
/////////////////////////////////////////////////////////////////////////////
      
H263_Base_EncoderContext::H263_Base_EncoderContext(const char * _prefix)
  : prefix(_prefix)
#if TRACE_FILE
  , tracer(_prefix, true)
#endif
  , _context(NULL)
{ 
  _inputFrameBuffer = NULL;
  FFMPEGLibraryInstance.Load();
}

H263_Base_EncoderContext::~H263_Base_EncoderContext()
{
  if (_inputFrameBuffer != NULL)
    free(_inputFrameBuffer);
}

bool H263_Base_EncoderContext::Open(CodecID codecId)
{
  TRACE_AND_LOG(tracer, 1, "Opening encoder");

  if (!FFMPEGLibraryInstance.IsLoaded())
    return false;

  _codec = FFMPEGLibraryInstance.AvcodecFindEncoder(codecId);
  if (_codec == NULL) {
    TRACE_AND_LOG(tracer, 1, "Codec not found for encoder");
    return false;
  }

  _context = FFMPEGLibraryInstance.AvcodecAllocContext();
  if (_context == NULL) {
    TRACE_AND_LOG(tracer, 1, "Failed to allocate context for encoder");
    return false;
  }

  _inputFrame = FFMPEGLibraryInstance.AvcodecAllocFrame();
  if (_inputFrame == NULL) {
    TRACE_AND_LOG(tracer, 1, "Failed to allocate frame for encoder");
    return false;
  }

  if (!InitContext())
    return false;

  _context->opaque = this;

  _context->codec = NULL;
  _context->mb_decision = FF_MB_DECISION_SIMPLE; // choose only one MB type at a time
  _context->me_method = ME_EPZS;

  _context->max_b_frames = 0;
  _context->pix_fmt = PIX_FMT_YUV420P;

  // X-Lite does not like Custom Picture frequency clocks...
  _context->time_base.num = 100; 
  _context->time_base.den = 2997;
  _context->gop_size      = 125;

  // avoid copying input/output
  _context->flags |= CODEC_FLAG_INPUT_PRESERVED; // we guarantee to preserve input for max_b_frames+1 frames
  _context->flags |= CODEC_FLAG_EMU_EDGE;        // don't draw edges
  _context->flags |= CODEC_FLAG_PASS1;

  _context->error_concealment = 3;
  _context->error_recognition = 5;

  // debugging flags
#if PLUGINCODEC_TRACING
  if (PTRACE_CHECK(4)) {
    _context->debug |= FF_DEBUG_RC;
    _context->debug |= FF_DEBUG_PICT_INFO;
    _context->debug |= FF_DEBUG_MV;
    _context->debug |= FF_DEBUG_QP;
  }
#endif

  _height = CIF_WIDTH; _width = CIF_HEIGHT;
  SetFrameWidth(_height);
  SetFrameHeight(_width);
  SetTargetBitrate(256000);
  SetTSTO(0);
  DisableAnnex(D);
  DisableAnnex(F);
  DisableAnnex(I);
  DisableAnnex(K);
  DisableAnnex(J);
  DisableAnnex(S);

  _frameCount = 0;

  TRACE_AND_LOG(tracer, 3, "encoder created");

  return true;
}

void H263_Base_EncoderContext::SetMaxKeyFramePeriod (unsigned period)
{
  _context->gop_size = period;
}

void H263_Base_EncoderContext::SetTargetBitrate (unsigned rate)
{
  m_targetBitRate = rate;
  CODEC_TRACER(tracer, "target bit rate set to " << m_targetBitRate);

  _context->bit_rate = (m_targetBitRate * 3) >> 2;        // average bit rate
  _context->bit_rate_tolerance = m_targetBitRate >> 1;
  _context->rc_min_rate = 0;                   // minimum bitrate
  _context->rc_max_rate = m_targetBitRate;                // maximum bitrate
  _context->rc_buffer_size = 224;

  /* ratecontrol qmin qmax limiting method
     0-> clipping, 1-> use a nice continous function to limit qscale wthin qmin/qmax.
  */
  
  _context->rc_qsquish = 0;            // limit q by clipping 
  _context->rc_eq = (char*) "1";       // rate control equation
  _context->rc_buffer_size = rate * 64;
}

void H263_Base_EncoderContext::SetFrameWidth (unsigned width)
{
  _width = width;
  FFMPEGLibraryInstance.AvSetDimensions(_context, _width, _height);

  _inputFrame->linesize[0] = width;
  _inputFrame->linesize[1] = width / 2;
  _inputFrame->linesize[2] = width / 2;

  CODEC_TRACER(tracer, "frame width set to width");
}

void H263_Base_EncoderContext::SetFrameHeight (unsigned height)
{
  _height = height;
  FFMPEGLibraryInstance.AvSetDimensions(_context, _width, _height);
  CODEC_TRACER(tracer, "frame height set to " << height);
}

void H263_Base_EncoderContext::SetTSTO (unsigned tsto)
{
  _inputFrame->quality = H263P_MIN_QUANT;

  _context->max_qdiff = 10;  // was 3      // max q difference between frames
  _context->qcompress = 0.5;               // qscale factor between easy & hard scenes (0.0-1.0)
  _context->i_quant_factor = (float)-0.6;  // qscale factor between p and i frames
  _context->i_quant_offset = (float)0.0;   // qscale offset between p and i frames
  _context->me_subpel_quality = 8;

  _context->qmin = H263P_MIN_QUANT;
  _context->qmax = round ( (31.0 - H263P_MIN_QUANT) / 31.0 * tsto + H263P_MIN_QUANT);
  _context->qmax = std::min( _context->qmax, 31);

  _context->mb_qmin = _context->qmin;
  _context->mb_qmax = _context->qmax;

  // Lagrange multipliers - this is how the context defaults do it:
  _context->lmin = _context->qmin * FF_QP2LAMBDA;
  _context->lmax = _context->qmax * FF_QP2LAMBDA; 

  CODEC_TRACER(tracer, "TSTO set to " << tsto);
}

void H263_Base_EncoderContext::EnableAnnex (Annex annex)
{
  switch (annex) {
    case D:
      // Annex D: Unrestructed Motion Vectors
      // Level 2+ 
      // works with eyeBeam, signaled via  non-standard "D"
      _context->flags |= CODEC_FLAG_H263P_UMV; 
      break;
    case F:
      // Annex F: Advanced Prediction Mode
      // does not work with eyeBeam
      // DO NOT ENABLE THIS FLAG. FFMPEG IS NOT THREAD_SAFE WHEN THIS FLAG IS SET
      //_context->flags |= CODEC_FLAG_OBMC; 
      break;
    case I:
      // Annex I: Advanced Intra Coding
      // Level 3+
      // works with eyeBeam
      _context->flags |= CODEC_FLAG_AC_PRED; 
      break;
    case K:
      // Annex K: 
      // does not work with eyeBeam
      //_context->flags |= CODEC_FLAG_H263P_SLICE_STRUCT;  
      break;
    case J:
      // Annex J: Deblocking Filter
      // works with eyeBeam
      _context->flags |= CODEC_FLAG_LOOP_FILTER;
      break;
    case T:
      break;
    case S:
      // Annex S: Alternative INTER VLC mode
      // does not work with eyeBeam
      //_context->flags |= CODEC_FLAG_H263P_AIV;
      break;
    case N:
    case P:
    default:
      break;
  }
}

void H263_Base_EncoderContext::DisableAnnex (Annex annex)
{
  switch (annex) {
    case D:
      // Annex D: Unrestructed Motion Vectors
      // Level 2+ 
      // works with eyeBeam, signaled via  non-standard "D"
      _context->flags &= ~CODEC_FLAG_H263P_UMV; 
      break;
    case F:
      // Annex F: Advanced Prediction Mode
      // does not work with eyeBeam
      _context->flags &= ~CODEC_FLAG_OBMC; 
      break;
    case I:
      // Annex I: Advanced Intra Coding
      // Level 3+
      // works with eyeBeam
      _context->flags &= ~CODEC_FLAG_AC_PRED; 
      break;
    case K:
      // Annex K: 
      // does not work with eyeBeam
      _context->flags &= ~CODEC_FLAG_H263P_SLICE_STRUCT;  
      break;
    case J:
      // Annex J: Deblocking Filter
      // works with eyeBeam
      _context->flags &= ~CODEC_FLAG_LOOP_FILTER;  
      break;
    case T:
      break;
    case S:
      // Annex S: Alternative INTER VLC mode
      // does not work with eyeBeam
      _context->flags &= ~CODEC_FLAG_H263P_AIV;
      break;
    case N:
    case P:
    default:
      break;
  }
}

#define CODEC_TRACER_FLAG(tracer, flag) \
CODEC_TRACER(tracer, #flag " is " << ((_context->flags & flag) ? "enabled" : "disabled"));

bool H263_Base_EncoderContext::OpenCodec()
{
  if (_codec == NULL) {
    TRACE_AND_LOG(tracer, 1, "Codec not initialized");
    return false;
  }

  CODEC_TRACER(tracer, "Size is " << _width << "x" << _height);
  CODEC_TRACER(tracer, "rc_max_rate is " <<  _context->rc_max_rate);
  CODEC_TRACER(tracer, "GOP is " << _context->gop_size);
  CODEC_TRACER(tracer, "qmin set to " << _context->qmin);
  CODEC_TRACER(tracer, "qmax set to " << _context->qmax);
  CODEC_TRACER(tracer, "mb_qmin set to " << _context->mb_qmin);
  CODEC_TRACER(tracer, "mb_qmax set to " << _context->mb_qmax);
  CODEC_TRACER(tracer, "bit_rate set to " << _context->bit_rate);
  CODEC_TRACER(tracer, "bit_rate_tolerance set to " <<_context->bit_rate_tolerance);
  CODEC_TRACER(tracer, "rc_min_rate set to " << _context->rc_min_rate);
  CODEC_TRACER_FLAG(tracer, CODEC_FLAG_H263P_UMV);
  CODEC_TRACER_FLAG(tracer, CODEC_FLAG_OBMC);
  CODEC_TRACER_FLAG(tracer, CODEC_FLAG_AC_PRED);
  CODEC_TRACER_FLAG(tracer, CODEC_FLAG_H263P_SLICE_STRUCT)
  CODEC_TRACER_FLAG(tracer, CODEC_FLAG_LOOP_FILTER);
  CODEC_TRACER_FLAG(tracer, CODEC_FLAG_H263P_AIV);

  return FFMPEGLibraryInstance.AvcodecOpen(_context, _codec) == 0;
}

void H263_Base_EncoderContext::CloseCodec()
{
  if (_context != NULL) {
    if (_context->codec != NULL) {
      FFMPEGLibraryInstance.AvcodecClose(_context);
    }
  }
}

void H263_Base_EncoderContext::Lock()
{
  _mutex.Wait();
}

void H263_Base_EncoderContext::Unlock()
{
  _mutex.Signal();
}

/////////////////////////////////////////////////////////////////////////////

H263_RFC2190_EncoderContext::H263_RFC2190_EncoderContext()
  : H263_Base_EncoderContext("RFC2190")
{
}

H263_RFC2190_EncoderContext::~H263_RFC2190_EncoderContext()
{
  WaitAndSignal m(_mutex);

  CloseCodec();

  if (_context != NULL)
    FFMPEGLibraryInstance.AvcodecFree(_context);
  if (_inputFrame != NULL)
    FFMPEGLibraryInstance.AvcodecFree(_inputFrame);

  TRACE_AND_LOG(tracer, 3, "encoder closed");
}

//s->avctx->rtp_callback(s->avctx, s->ptr_lastgob, current_packet_size, number_mb)
static void rtp_callback(struct AVCodecContext *avctx, void * _data, int size, int mb_nb)
{
  void * opaque = avctx->opaque;
  H263_RFC2190_EncoderContext * context = (H263_RFC2190_EncoderContext *)opaque;
  context->RTPCallBack(avctx, _data, size, mb_nb);
}

void H263_RFC2190_EncoderContext::RTPCallBack(struct AVCodecContext * /*avctx*/, void * _data, int size, int mbCount)
{
  // sometimes, FFmpeg encodes the same frame multiple times
  // we need to detect this in order to avoid duplicating the encoded data
  if ((_data == packetizer.m_buffer) && (packetizer.fragments.size() != 0)) {
    packetizer.fragments.resize(0);
    currentMb = 0;
    currentBytes = 0;
  }

  // add the fragment to the list
  RFC2190Packetizer::fragment frag;
  frag.length = size;
  frag.mbNum  = currentMb;
  packetizer.fragments.push_back(frag);
  currentMb = currentMb + mbCount;
  currentBytes += size;
}

bool H263_RFC2190_EncoderContext::Open()
{
  if (!H263_Base_EncoderContext::Open(CODEC_ID_H263))
    return false;

#if LIBAVCODEC_RTP_MODE
  _context->rtp_mode = 1;
#endif

  _context->rtp_payload_size = 200;
  _context->rtp_callback = &rtp_callback;
  _context->opaque = (H263_RFC2190_EncoderContext *)this; // used to separate out packets from different encode threads

  _context->flags &= ~CODEC_FLAG_H263P_UMV;
  _context->flags &= ~CODEC_FLAG_4MV;
#if LIBAVCODEC_RTP_MODE
  _context->flags &= ~CODEC_FLAG_H263P_AIC;
#endif
  _context->flags &= ~CODEC_FLAG_H263P_AIV;
  _context->flags &= ~CODEC_FLAG_H263P_SLICE_STRUCT;
  
  SetMaxKeyFramePeriod(H263_KEY_FRAME_INTERVAL);
  SetMaxRTPFrameSize(H263_PAYLOAD_SIZE);
  
  return true;
}

bool H263_RFC2190_EncoderContext::InitContext()
{
  return true;
}

void H263_RFC2190_EncoderContext::SetMaxRTPFrameSize (unsigned /*size*/)
{
  //_context->rtp_payload_size = (size * 6 / 7);
  
  //if ((size * 6 / 7) > 0)
  // _context->rtp_payload_size = (size * 6 / 7);
  // else  
  // _context->rtp_payload_size = size;
  //_txH263PFrame->SetMaxPayloadSize(size);
}

int H263_RFC2190_EncoderContext::EncodeFrames(const BYTE * src, unsigned & srcLen, BYTE * dst, unsigned & dstLen, unsigned int & flags)
{
  WaitAndSignal m(_mutex);

  if (_codec == NULL) {
    TRACE_AND_LOG(tracer, 1, "Encoder\tCodec not initialized");
    return 0;
  }

  // create RTP frame from source buffer
  RTPFrame srcRTP(src, srcLen);

  // create RTP frame from destination buffer
  RTPFrame dstRTP(dst, dstLen);
  dstLen = 0;

  // if still running out packets from previous frame, then return it
  if (packetizer.GetPacket(dstRTP, flags) != 0) {
    CODEC_TRACER_RTP(tracer, "Tx frame:", dstRTP, RFC2190Dump);
    dstLen = dstRTP.GetHeaderSize() + dstRTP.GetPayloadSize();
    return 1;
  }

  // zero payload means do nothing
  if (srcRTP.GetPayloadSize() == 0) {
    TRACE_AND_LOG(tracer, 1, "Zero payload passed");
    dstLen = dstRTP.GetHeaderSize();
    dstRTP.SetPayloadSize(0);
    dstRTP.SetMarker(true);
    flags |= 1;
    return 1;
  }

  // make sure the source frame is legal
  if (srcRTP.GetPayloadSize() < sizeof(PluginCodec_Video_FrameHeader)) {
    TRACE_AND_LOG(tracer, 1, "Video grab too small, closing down video transmission thread.");
    return 0;
  }
  PluginCodec_Video_FrameHeader * header = (PluginCodec_Video_FrameHeader *)srcRTP.GetPayloadPtr();
  if (header->x != 0 || header->y != 0) {
    TRACE_AND_LOG(tracer, 1, "Video grab of partial frame unsupported, closing down video transmission thread.");
    return 0;
  }

  // if this is the first frame, or the frame size has changed, deal wth it
  if ((_frameCount == 0) || 
      ((unsigned) _width !=  header->width) || 
      ((unsigned) _height != header->height)) {

    TRACE_AND_LOG(tracer, 4, "First frame received or resolution has changed - reopening codec");
    CloseCodec();
    SetFrameWidth(header->width);
    SetFrameHeight(header->height);
    if (!OpenCodec()) {
      TRACE_AND_LOG(tracer, 1, "Reopening codec failed");
      return 0;
    }

    if (_inputFrameBuffer != NULL)
      free(_inputFrameBuffer);
#if HAVE_POSIX_MEMALIGN
    if (posix_memalign((void **)&_inputFrameBuffer, 64, header->width*header->height*3/2 + (FF_INPUT_BUFFER_PADDING_SIZE*2)) != 0) 
#else
    if ((_inputFrameBuffer = (BYTE *)malloc(header->width*header->height*3/2 + (FF_INPUT_BUFFER_PADDING_SIZE*2))) == NULL) 
#endif
    {
      TRACE_AND_LOG(tracer, 1, "Unable to allocate memory for frame buffer");
      return 0;
    }
  }

  CODEC_TRACER(tracer, "Input:seq=" << _frameCount
                       << ",size=" << header->width << "x" << header->height
                       << ",I=" << ((flags && forceIFrame) ? "yes" : "no"));

  ++_frameCount;

  int size = header->width * header->height;
  int frameSize = (size * 3) >> 1;
 
  // we need FF_INPUT_BUFFER_PADDING_SIZE allocated bytes after the YVU420P image for the encoder
  memcpy (_inputFrameBuffer, OPAL_VIDEO_FRAME_DATA_PTR(header), frameSize);
  memset (_inputFrameBuffer + frameSize, 0 , FF_INPUT_BUFFER_PADDING_SIZE);
  _inputFrame->data[0] = _inputFrameBuffer;

  _inputFrame->data[1] = _inputFrame->data[0] + size;
  _inputFrame->data[2] = _inputFrame->data[1] + (size / 4);
  _inputFrame->pict_type = (flags && forceIFrame) ? FF_I_TYPE : 0;

  currentMb = 0;
  currentBytes = 0;

  packetizer.fragments.resize(0);

  size_t newOutputSize = 100000;

  if (packetizer.m_buffer != NULL) {
    if (packetizer.m_bufferSize < newOutputSize) {
      free(packetizer.m_buffer);
      packetizer.m_buffer = NULL;
    }
  }
  if (packetizer.m_buffer == NULL) {
    packetizer.m_bufferSize = newOutputSize;
#if HAVE_POSIX_MEMALIGN
    if (posix_memalign((void **)&packetizer.m_buffer, 64, packetizer.m_bufferSize) != 0) 
#else
    if ((packetizer.m_buffer = (BYTE *)malloc(packetizer.m_bufferSize)) == NULL) 
#endif
    {
      TRACE_AND_LOG(tracer, 1, "Unable to allocate memory for packet buffer");
      return 0;
    }
  }

  //CODEC_TRACER(tracer, "Encoder called with " << frameSize << " bytes and frame type " << _inputFrame->pict_type << " at " << header->width << "x" << header->height);

  int encodedLen = FFMPEGLibraryInstance.AvcodecEncodeVideo(_context, packetizer.m_buffer, packetizer.m_bufferSize, _inputFrame);  

  if (encodedLen < 0) {
    TRACE_AND_LOG(tracer, 1, "Encoder failed");
    return 0;
  }
  if (encodedLen == 0) {
    TRACE_AND_LOG(tracer, 1, "Encoder returned empty frame");
    dstRTP.SetPayloadSize(0);
    dstLen = dstRTP.GetHeaderSize();
    flags |= 1;
    return 1;
  }

  packetizer.m_bufferLen = encodedLen;

  // push the encoded frame through the packetizer
#if TRACE_FILE
  {
    const unsigned char * p =  packetizer.m_buffer;
    CODEC_TRACER(tracer, "Raw data: " << hex << setfill('0') << setprecision(2)
                         << (int)p[0] << ' ' << (int)p[1] << ' ' << (int)p[2] << ' ' << (int)p[3] << ' ' << (int)p[4]
                         << setfill(' ') << dec);
  }
#endif

  if (packetizer.Open(srcRTP.GetTimestamp(), encodedLen) < 0) {
    TRACE_AND_LOG(tracer, 1,  "Packetizer failed");
    flags = 1;
    return 0;
  }

  CODEC_TRACER(tracer, "Encoder returned " << encodedLen << " bytes as " << packetizer.fragments.size() << " frames");

  // return the first encoded block of data
  if (packetizer.GetPacket(dstRTP, flags)) {
    CODEC_TRACER_RTP(tracer, "Tx frame:", dstRTP, RFC2190Dump);
    dstLen = dstRTP.GetHeaderSize() + dstRTP.GetPayloadSize();
  }

  return 1;
}

/////////////////////////////////////////////////////////////////////////////

H263_RFC2429_EncoderContext::H263_RFC2429_EncoderContext()
 : H263_Base_EncoderContext("RFC2429")
{
  _txH263PFrame = NULL;
}

H263_RFC2429_EncoderContext::~H263_RFC2429_EncoderContext()
{
  WaitAndSignal m(_mutex);

  CloseCodec();

  if (_txH263PFrame)
    delete _txH263PFrame;

  if (_context != NULL)
    FFMPEGLibraryInstance.AvcodecFree(_context);
  if (_inputFrame != NULL)
    FFMPEGLibraryInstance.AvcodecFree(_inputFrame);

  TRACE_AND_LOG(tracer, 3, "encoder closed");
}


bool H263_RFC2429_EncoderContext::Open()
{
  if (!H263_Base_EncoderContext::Open(CODEC_ID_H263P))
    return false;

  SetMaxKeyFramePeriod(H263P_KEY_FRAME_INTERVAL);
  SetMaxRTPFrameSize(H263P_PAYLOAD_SIZE);

  return true;
}

bool H263_RFC2429_EncoderContext::InitContext()
{
  _txH263PFrame = new H263PFrame(MAX_YUV420P_FRAME_SIZE);
  return _txH263PFrame != NULL;
}

void H263_RFC2429_EncoderContext::SetMaxRTPFrameSize (unsigned size)
{
   if ((size * 6 / 7) > 0)
    _context->rtp_payload_size = (size * 6 / 7);
    else  
    _context->rtp_payload_size = size;

  _txH263PFrame->SetMaxPayloadSize((uint16_t)size);
}


int H263_RFC2429_EncoderContext::EncodeFrames(const BYTE * src, unsigned & srcLen, BYTE * dst, unsigned & dstLen, unsigned int & flags)
{
  WaitAndSignal m(_mutex);

  if (_codec == NULL) {
    TRACE_AND_LOG(tracer, 1, "Codec not initialized");
    return 0;
  }

  // create RTP frame from source buffer
  RTPFrame srcRTP(src, srcLen);

  // create RTP frame from destination buffer
  RTPFrame dstRTP(dst, dstLen);
  dstLen = 0;

  // if there are RTP packets to return, return them
  if  (_txH263PFrame->HasRTPFrames())
  {
    _txH263PFrame->GetRTPFrame(dstRTP, flags);
    dstLen = dstRTP.GetFrameLen();
    CODEC_TRACER_RTP(tracer, "Tx frame:", dstRTP, RFC2429Dump);
    return 1;
  }

  if (srcRTP.GetPayloadSize() < sizeof(PluginCodec_Video_FrameHeader)) {
    TRACE_AND_LOG(tracer, 1, "Video grab too small, closing down video transmission thread.");
    return 0;
  }

  PluginCodec_Video_FrameHeader * header = (PluginCodec_Video_FrameHeader *)srcRTP.GetPayloadPtr();
  if (header->x != 0 || header->y != 0) {
    TRACE_AND_LOG(tracer, 1, "Video grab of partial frame unsupported, closing down video transmission thread.");
    return 0;
  }

  // if this is the first frame, or the frame size has changed, deal wth it
  if ((_frameCount == 0) || 
      ((unsigned) _width !=  header->width) || 
      ((unsigned) _height != header->height)) {

    TRACE_AND_LOG(tracer, 4, "First frame received or resolution has changed - reopening codec");
    CloseCodec();
    SetFrameWidth(header->width);
    SetFrameHeight(header->height);
    if (!OpenCodec()) {
      TRACE_AND_LOG(tracer, 1, "Reopening codec failed");
      return 0;
    }
    if (_inputFrameBuffer != NULL)
      free(_inputFrameBuffer);
#if HAVE_POSIX_MEMALIGN
    if (posix_memalign((void **)&_inputFrameBuffer, 64, header->width*header->height*3/2 + (FF_INPUT_BUFFER_PADDING_SIZE*2)) != 0) {
#else
    if ((_inputFrameBuffer = (BYTE *)malloc(header->width*header->height*3/2 + (FF_INPUT_BUFFER_PADDING_SIZE*2))) != NULL) {
#endif
      TRACE_AND_LOG(tracer, 1, "Unable to allocate memory for frame buffer");
      return 0;
    }
  }

  CODEC_TRACER(tracer, "Input:seq=" << _frameCount
                       << ",size=" << header->width << "x" << header->height
                       << ",I=" << ((flags && forceIFrame) ? "yes" : "no"));

  int size = header->width * header->height;
  int frameSize = (size * 3) >> 1;
 
  // we need FF_INPUT_BUFFER_PADDING_SIZE allocated bytes after the YVU420P image for the encoder
  memset (_inputFrameBuffer, 0 , FF_INPUT_BUFFER_PADDING_SIZE);
  memcpy (_inputFrameBuffer + FF_INPUT_BUFFER_PADDING_SIZE, OPAL_VIDEO_FRAME_DATA_PTR(header), frameSize);
  memset (_inputFrameBuffer + FF_INPUT_BUFFER_PADDING_SIZE + frameSize, 0 , FF_INPUT_BUFFER_PADDING_SIZE);

  _inputFrame->data[0] = _inputFrameBuffer + FF_INPUT_BUFFER_PADDING_SIZE;
  _inputFrame->data[1] = _inputFrame->data[0] + size;
  _inputFrame->data[2] = _inputFrame->data[1] + (size / 4);
  _inputFrame->pict_type = (flags && forceIFrame) ? FF_I_TYPE : 0;
 
  _txH263PFrame->BeginNewFrame();
  _txH263PFrame->SetTimestamp(srcRTP.GetTimestamp());
  _txH263PFrame->SetFrameSize (FFMPEGLibraryInstance.AvcodecEncodeVideo(_context, _txH263PFrame->GetFramePtr(), frameSize, _inputFrame));  
  _frameCount++; 

  if (_txH263PFrame->GetFrameSize() == 0) {
    TRACE_AND_LOG(tracer, 1, "Encoder internal error - there should be outstanding packets at this point");
    return 1;
  }

  CODEC_TRACER(tracer, "Encoder created " << _txH263PFrame->GetFrameSize() << " output frames");

  if (_txH263PFrame->HasRTPFrames())
  {
    _txH263PFrame->GetRTPFrame(dstRTP, flags);
    dstLen = dstRTP.GetFrameLen();
    CODEC_TRACER_RTP(tracer, "Tx frame:", dstRTP, RFC2429Dump);
    return 1;
  }
  return 1;
}

/////////////////////////////////////////////////////////////////////////////

H263_Base_DecoderContext::H263_Base_DecoderContext(const char * _prefix)
  : prefix(_prefix)
#if TRACE_FILE
  , tracer(_prefix, false)
#endif
{
  if (!FFMPEGLibraryInstance.Load())
    return;

  if ((_codec = FFMPEGLibraryInstance.AvcodecFindDecoder(CODEC_ID_H263)) == NULL) {
    TRACE_AND_LOG(tracer, 1, "Codec not found for decoder");
    return;
  }

  _context = FFMPEGLibraryInstance.AvcodecAllocContext();
  if (_context == NULL) {
    TRACE_AND_LOG(tracer, 1, "Failed to allocate context for decoder");
    return;
  }

  _outputFrame = FFMPEGLibraryInstance.AvcodecAllocFrame();
  if (_outputFrame == NULL) {
    TRACE_AND_LOG(tracer, 1, "Failed to allocate frame for decoder");
    return;
  }

  if (!OpenCodec()) { // decoder will re-initialise context with correct frame size
    TRACE_AND_LOG(tracer, 1, "Failed to open codec for decoder");
    return;
  }

  _frameCount = 0;

  // debugging flags
#if PLUGINCODEC_TRACING
  if (PTRACE_CHECK(4)) {
    _context->debug |= FF_DEBUG_RC;
    _context->debug |= FF_DEBUG_PICT_INFO;
    _context->debug |= FF_DEBUG_MV;
  }
#endif

  TRACE_AND_LOG(tracer, 4, "Decoder created");
}

H263_Base_DecoderContext::~H263_Base_DecoderContext()
{
  CloseCodec();

  if (_context != NULL)
    FFMPEGLibraryInstance.AvcodecFree(_context);
  if (_outputFrame != NULL)
    FFMPEGLibraryInstance.AvcodecFree(_outputFrame);
}

bool H263_Base_DecoderContext::OpenCodec()
{
  if (_codec == NULL) {
    TRACE_AND_LOG(tracer, 1, "Codec not initialized");
    return 0;
  }

  if (FFMPEGLibraryInstance.AvcodecOpen(_context, _codec) < 0) {
    TRACE_AND_LOG(tracer, 1, "Failed to open H.263 decoder");
    return false;
  }

  TRACE_AND_LOG(tracer, 4, "Codec opened");

  return true;
}

void H263_Base_DecoderContext::CloseCodec()
{
  if (_context != NULL) {
    if (_context->codec != NULL) {
      FFMPEGLibraryInstance.AvcodecClose(_context);
      TRACE_AND_LOG(tracer, 4, "Closed H.263 decoder" );
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////

H263_RFC2429_DecoderContext::H263_RFC2429_DecoderContext()
  : H263_Base_DecoderContext("RFC2429")
{
  _rxH263PFrame = new H263PFrame(MAX_YUV420P_FRAME_SIZE);
  _skippedFrameCounter = 0;
  _gotIFrame = false;
  _gotAGoodFrame = true;
}

H263_RFC2429_DecoderContext::~H263_RFC2429_DecoderContext()
{
  if (_rxH263PFrame)
    delete _rxH263PFrame;
}

bool H263_RFC2429_DecoderContext::DecodeFrames(const BYTE * src, unsigned & srcLen, BYTE * dst, unsigned & dstLen, unsigned int & flags)
{
  TRACE_AND_LOG(tracer, 4, "Codec opened");

  // create RTP frame from source buffer
  RTPFrame srcRTP(src, srcLen);

  CODEC_TRACER_RTP(tracer, "Tx frame:", srcRTP, RFC2429Dump);

  // create RTP frame from destination buffer
  RTPFrame dstRTP(dst, dstLen, 0);
  dstLen = 0;

  if (!_rxH263PFrame->SetFromRTPFrame(srcRTP, flags)) {
    _rxH263PFrame->BeginNewFrame();
    flags = (_gotAGoodFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);
    _gotAGoodFrame = false;
    return true;
  }
  
  if (srcRTP.GetMarker()==0)
  {
     return 1;
  } 

  if (_rxH263PFrame->GetFrameSize()==0)
  {
    _rxH263PFrame->BeginNewFrame();
    TRACE_AND_LOG(tracer, 4, "Got an empty frame - skipping");
    _skippedFrameCounter++;
    return 1;
  }

  if (!_rxH263PFrame->hasPicHeader()) {
    TRACE_AND_LOG(tracer, 1, "Received frame has no picture header - dropping");
    _rxH263PFrame->BeginNewFrame();
    flags = (_gotAGoodFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);
    _gotAGoodFrame = false;
    return 1;
  }

  // look and see if we have read an I frame.
  if (!_gotIFrame)
  {
    if (!_rxH263PFrame->IsIFrame())
    {
      TRACE_AND_LOG(tracer, 1, "Waiting for an I-Frame");
      _rxH263PFrame->BeginNewFrame();
      flags = (_gotAGoodFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);
      _gotAGoodFrame = false;
      return 1;
    }
    _gotIFrame = true;
  }

  int gotPicture = 0;

  TRACE_AND_LOG(tracer, 4, "Decoding " << _rxH263PFrame->GetFrameSize()  << " bytes");
  int bytesDecoded = FFMPEGLibraryInstance.AvcodecDecodeVideo(_context, _outputFrame, &gotPicture, _rxH263PFrame->GetFramePtr(), _rxH263PFrame->GetFrameSize());

  _rxH263PFrame->BeginNewFrame();

  if (!gotPicture) 
  {
    TRACE_AND_LOG(tracer, 1, "Decoded "<< bytesDecoded << " bytes without getting a Picture"); 
    _skippedFrameCounter++;
    flags = (_gotAGoodFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);
    _gotAGoodFrame = false;
    return 1;
  }

  TRACE_AND_LOG(tracer, 4, "Decoded " << bytesDecoded << " bytes"<< ", Resolution: " << _context->width << "x" << _context->height);

  // if error occurred, tell the other end to send another I-frame and hopefully we can resync
  if (bytesDecoded < 0) {
    TRACE_AND_LOG(tracer, 1, "Decoded 0 bytes");
    flags = (_gotAGoodFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);
    _gotAGoodFrame = false;
    return 1;
  }

  // if decoded frame size is not legal, request an I-Frame
  if (_context->width == 0 || _context->height == 0) {
    TRACE_AND_LOG(tracer, 1, "Received frame with invalid size");
    flags = (_gotAGoodFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);
    _gotAGoodFrame = false;
    return 1;
  }
  _gotAGoodFrame = true;

  int frameBytes = (_context->width * _context->height * 12) / 8;
  PluginCodec_Video_FrameHeader * header = (PluginCodec_Video_FrameHeader *)dstRTP.GetPayloadPtr();
  header->x = header->y = 0;
  header->width = _context->width;
  header->height = _context->height;
  int size = _context->width * _context->height;
  if (_outputFrame->data[1] == _outputFrame->data[0] + size
      && _outputFrame->data[2] == _outputFrame->data[1] + (size >> 2)) {
    memcpy(OPAL_VIDEO_FRAME_DATA_PTR(header), _outputFrame->data[0], frameBytes);
  } else {
    unsigned char *dst = OPAL_VIDEO_FRAME_DATA_PTR(header);
    for (int i=0; i<3; i ++) {
      unsigned char *src = _outputFrame->data[i];
      int dst_stride = i ? _context->width >> 1 : _context->width;
      int src_stride = _outputFrame->linesize[i];
      int h = i ? _context->height >> 1 : _context->height;

      if (src_stride==dst_stride) {
        memcpy(dst, src, dst_stride*h);
        dst += dst_stride*h;
      } else {
        while (h--) {
          memcpy(dst, src, dst_stride);
          dst += dst_stride;
          src += src_stride;
        }
      }
    }
  }

  dstRTP.SetPayloadSize(sizeof(PluginCodec_Video_FrameHeader) + frameBytes);
  dstRTP.SetTimestamp(srcRTP.GetTimestamp());
  dstRTP.SetMarker(true);

  dstLen = dstRTP.GetFrameLen();

  flags = PluginCodec_ReturnCoderLastFrame ;

  _frameCount++;

  return 1;
}


H263_RFC2190_DecoderContext::H263_RFC2190_DecoderContext()
  : H263_Base_DecoderContext("RFC2190")
{
}

H263_RFC2190_DecoderContext::~H263_RFC2190_DecoderContext()
{
}

static bool ReturnEmptyFrame(RTPFrame & dstRTP, unsigned & dstLen, unsigned int & flags)
{
  flags |= PluginCodec_ReturnCoderLastFrame;
  dstRTP.SetPayloadSize(0);
  dstLen = 0;
  return true;
}

bool H263_RFC2190_DecoderContext::DecodeFrames(const BYTE * src, unsigned & srcLen, BYTE * dst, unsigned & dstLen, unsigned int & flags)
{
  // create RTP frame from source buffer
  RTPFrame srcRTP(src, srcLen);

  // create RTP frame from destination
  RTPFrame dstRTP(dst, dstLen, 0);
  dstRTP.SetTimestamp(srcRTP.GetTimestamp());

  if (dstLen < (12 + sizeof(PluginCodec_Video_FrameHeader))) {
    flags = 0;
    TRACE_AND_LOG(tracer, 1, "Destination buffer " << dstLen << " insufficient for video header");
    ReturnEmptyFrame(dstRTP, dstLen, flags);
  }

  dstLen = 0;

  CODEC_TRACER_RTP(tracer, "Rx frame:", srcRTP, RFC2190Dump);

  // push new frame through the depacketiser
  bool requestIFrame, isIFrame;
  int code = depacketizer.SetPacket(srcRTP, requestIFrame, isIFrame);
  if (code <= 0) {
    flags = requestIFrame ? PluginCodec_ReturnCoderRequestIFrame : 0;
    return ReturnEmptyFrame(dstRTP, dstLen, flags);
  }

  if ((depacketizer.frame.size() < 3)  ||
      (depacketizer.frame[0] != 0x00) ||
      (depacketizer.frame[1] != 0x00) ||
      (depacketizer.frame[2] & 0x80) != 0x80) {
    TRACE_AND_LOG(tracer, 1, "Frame does not start with correct code");
    flags = PluginCodec_ReturnCoderRequestIFrame;
    return ReturnEmptyFrame(dstRTP, dstLen, flags);
  }

  TRACE_AND_LOG(tracer, 4, "Decoder called with " << depacketizer.frame.size()  << " bytes");

#if FFMPEG_HAS_DECODE_ERROR_COUNT
  unsigned error_before = _context->decode_error_count;
#endif

  int gotPicture = 0;
  int bytesDecoded = FFMPEGLibraryInstance.AvcodecDecodeVideo(_context, _outputFrame, &gotPicture, &depacketizer.frame[0], depacketizer.frame.size());

  depacketizer.NewFrame();

  if (!gotPicture) {
    flags = PluginCodec_ReturnCoderRequestIFrame;
    TRACE_AND_LOG(tracer, 1, "Decoded "<< bytesDecoded << " bytes without getting a Picture"); 
    return ReturnEmptyFrame(dstRTP, dstLen, flags);
  }

  TRACE_AND_LOG(tracer, 4, "Decoder processed " << bytesDecoded << " bytes, creating frame at " << _context->width << "x" << _context->height);

  // if error occurred, tell the other end to send another I-frame and hopefully we can resync
  if (bytesDecoded < 0
#if FFMPEG_HAS_DECODE_ERROR_COUNT
      || error_before != _context->decode_error_count
#endif
  ) {
    flags = PluginCodec_ReturnCoderRequestIFrame;
    return ReturnEmptyFrame(dstRTP, dstLen, flags);
  }

  if (bytesDecoded == 0) {
    return ReturnEmptyFrame(dstRTP, dstLen, flags);
  }

  // if decoded frame size is not legal, request an I-Frame
  if ((_context->width <= 0) ||
      (_context->height <= 0) ||
      (_context->width > CIF4_WIDTH) ||
      (_context->height > CIF4_HEIGHT) ||
      (_context->height * _context->width > (CIF4_WIDTH * CIF4_HEIGHT))) {
    TRACE_AND_LOG(tracer, 1, "Received frame with invalid size");
    flags = PluginCodec_ReturnCoderRequestIFrame;
    return ReturnEmptyFrame(dstRTP, dstLen, flags);
  }

  // create RTP frame from destination buffer
  unsigned frameBytes = (_context->width * _context->height * 12) / 8;
  if (dstRTP.GetPayloadSize() - sizeof(PluginCodec_Video_FrameHeader) < frameBytes) {
    TRACE_AND_LOG(tracer, 1, "Destination buffer size " << dstRTP.GetPayloadSize() << " too small for frame of size " << _context->width  << "x" <<  _context->height);
    flags = PluginCodec_ReturnCoderRequestIFrame;
    return ReturnEmptyFrame(dstRTP, dstLen, flags);
  }

  PluginCodec_Video_FrameHeader * header = (PluginCodec_Video_FrameHeader *)dstRTP.GetPayloadPtr();
  header->x      = header->y = 0;
  header->width  = _context->width;
  header->height = _context->height;
  int size = _context->width * _context->height;

  if ((unsigned)dstRTP.GetFrameLen() < (frameBytes + sizeof(PluginCodec_Video_FrameHeader))) {
    flags = PluginCodec_ReturnCoderRequestIFrame;
    TRACE_AND_LOG(tracer, 1, "Destination buffer " << dstLen << " insufficient for decoded data size " << header->width << "x" << header->height);
    return ReturnEmptyFrame(dstRTP, dstLen, flags);
  }

  dstRTP.SetPayloadSize(sizeof(PluginCodec_Video_FrameHeader) + frameBytes);
  dstLen = dstRTP.GetHeaderSize() + dstRTP.GetPayloadSize();

  if (
       (_outputFrame->data[1] == (_outputFrame->data[0] + size)) &&
       (_outputFrame->data[2] == (_outputFrame->data[1] + (size >> 2)))
     ) {
    memcpy(OPAL_VIDEO_FRAME_DATA_PTR(header), _outputFrame->data[0], frameBytes);
  } else {
    unsigned char *dst = OPAL_VIDEO_FRAME_DATA_PTR(header);
    for (int i=0; i<3; i ++) {
      unsigned char *src = _outputFrame->data[i];
      int dst_stride = i ? _context->width >> 1 : _context->width;
      int src_stride = _outputFrame->linesize[i];
      int h = i ? _context->height >> 1 : _context->height;

      if (src_stride==dst_stride) {
        memcpy(dst, src, dst_stride*h);
        dst += dst_stride*h;
      } else {
        while (h-- > 0) {
          memcpy(dst, src, dst_stride);
          dst += dst_stride;
          src += src_stride;
        }
      }
    }
  }

  dstRTP.SetTimestamp(srcRTP.GetTimestamp());
  dstRTP.SetMarker(true);

  flags = PluginCodec_ReturnCoderLastFrame |
          (isIFrame ? PluginCodec_ReturnCoderIFrame : 0) |
          (requestIFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);

  _frameCount++;

  return 1;
}

/////////////////////////////////////////////////////////////////////////////

static int get_codec_options(const struct PluginCodec_Definition * codec,
                                                  void *,
                                                  const char *,
                                                  void * parm,
                                                  unsigned * parmLen)
{
    if (parmLen == NULL || parm == NULL || *parmLen != sizeof(struct PluginCodec_Option **))
        return 0;

    *(const void **)parm = codec->userData;
    *parmLen = 0; //FIXME
    return 1;
}

static int free_codec_options ( const struct PluginCodec_Definition *, void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  char ** strings = (char **) parm;
  for (char ** string = strings; *string != NULL; string++)
    free(*string);
  free(strings);
  return 1;
}

static int valid_for_protocol ( const struct PluginCodec_Definition *, void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char *))
    return 0;

  return (STRCMPI((const char *)parm, "sip") == 0) ? 1 : 0;
}

/////////////////////////////////////////////////////////////////////////////

static void * create_encoder(const struct PluginCodec_Definition * codec)
{
  H263_Base_EncoderContext * context;

  if (strcmp(codec->destFormat, h263Desc) == 0)
    context = new H263_RFC2190_EncoderContext();
  else
    context = new H263_RFC2429_EncoderContext();

  if (context->Open()) 
    return context;

  delete context;
  return NULL;
}

static void destroy_encoder(const struct PluginCodec_Definition * /*codec*/, void * _context)
{
  H263_Base_EncoderContext * context = (H263_Base_EncoderContext *)_context;
  delete context;
}

static int codec_encoder(const struct PluginCodec_Definition * , 
                                           void * _context,
                                     const void * from, 
                                       unsigned * fromLen,
                                           void * to,
                                       unsigned * toLen,
                                   unsigned int * flag)
{
  H263_Base_EncoderContext * context = (H263_Base_EncoderContext *)_context;
  return context->EncodeFrames((const BYTE *)from, *fromLen, (BYTE *)to, *toLen, *flag);
}

#define PMAX(a,b) ((a)>=(b)?(a):(b))
#define PMIN(a,b) ((a)<=(b)?(a):(b))

static void FindBoundingBox(const char * const * * parm, 
                                             int * mpi,
                                             int & minWidth,
                                             int & minHeight,
                                             int & maxWidth,
                                             int & maxHeight,
                                             int & frameTime,
                                             int & targetBitRate,
                                             int & maxBitRate)
{
  // initialise the MPI values to disabled
  int i;
  for (i = 0; i < 5; i++)
    mpi[i] = PLUGINCODEC_MPI_DISABLED;

  // following values will be set while scanning for options
  minWidth      = INT_MAX;
  minHeight     = INT_MAX;
  maxWidth      = 0;
  maxHeight     = 0;
  int rxMinWidth    = QCIF_WIDTH;
  int rxMinHeight   = QCIF_HEIGHT;
  int rxMaxWidth    = QCIF_WIDTH;
  int rxMaxHeight   = QCIF_HEIGHT;
  int frameRate     = 10;      // 10 fps
  int origFrameTime = 900;     // 10 fps in video RTP timestamps
  int maxBR = 0;
  maxBitRate = 0;
  targetBitRate = 0;

  // extract the MPI values set in the custom options, and find the min/max of them
  frameTime = 0;

  for (const char * const * option = *parm; *option != NULL; option += 2) {
    if (STRCMPI(option[0], "MaxBR") == 0)
      maxBR = atoi(option[1]);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_MAX_BIT_RATE) == 0)
      maxBitRate = atoi(option[1]);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
      targetBitRate = atoi(option[1]);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_MIN_RX_FRAME_WIDTH) == 0)
      rxMinWidth  = atoi(option[1]);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_MIN_RX_FRAME_HEIGHT) == 0)
      rxMinHeight = atoi(option[1]);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_MAX_RX_FRAME_WIDTH) == 0)
      rxMaxWidth  = atoi(option[1]);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_MAX_RX_FRAME_HEIGHT) == 0)
      rxMaxHeight = atoi(option[1]);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_TIME) == 0)
      origFrameTime = atoi(option[1]);
    else {
      for (i = 0; i < 5; i++) {
        if (STRCMPI(option[0], StandardVideoSizes[i].optionName) == 0) {
          mpi[i] = atoi(option[1]);
          if (mpi[i] != PLUGINCODEC_MPI_DISABLED) {
            int thisTime = 3003*mpi[i];
            if (minWidth > StandardVideoSizes[i].width)
              minWidth = StandardVideoSizes[i].width;
            if (minHeight > StandardVideoSizes[i].height)
              minHeight = StandardVideoSizes[i].height;
            if (maxWidth < StandardVideoSizes[i].width)
              maxWidth = StandardVideoSizes[i].width;
            if (maxHeight < StandardVideoSizes[i].height)
              maxHeight = StandardVideoSizes[i].height;
            if (thisTime > frameTime)
              frameTime = thisTime;
          }
        }
      }
    }
  }

  // if no MPIs specified, then the spec says to use QCIF
  if (frameTime == 0) {
    int ft;
    if (frameRate != 0) 
      ft = 90000 / frameRate;
    else 
      ft = origFrameTime;
    mpi[1] = (ft + 1502) / 3003;

#ifdef DEFAULT_TO_FULL_CAPABILITIES
    minWidth  = QCIF_WIDTH;
    maxWidth  = CIF16_WIDTH;
    minHeight = QCIF_HEIGHT;
    maxHeight = CIF16_HEIGHT;
#else
    minWidth  = maxWidth  = QCIF_WIDTH;
    minHeight = maxHeight = QCIF_HEIGHT;
#endif
  }

  // find the smallest MPI size that is larger than the min frame size
  for (i = 0; i < 5; i++) {
    if (StandardVideoSizes[i].width >= rxMinWidth && StandardVideoSizes[i].height >= rxMinHeight) {
      rxMinWidth = StandardVideoSizes[i].width;
      rxMinHeight = StandardVideoSizes[i].height;
      break;
    }
  }

  // find the largest MPI size that is smaller than the max frame size
  for (i = 4; i >= 0; i--) {
    if (StandardVideoSizes[i].width <= rxMaxWidth && StandardVideoSizes[i].height <= rxMaxHeight) {
      rxMaxWidth  = StandardVideoSizes[i].width;
      rxMaxHeight = StandardVideoSizes[i].height;
      break;
    }
  }

  // the final min/max is the smallest bounding box that will enclose both the MPI information and the min/max information
  minWidth  = PMAX(rxMinWidth, minWidth);
  maxWidth  = PMIN(rxMaxWidth, maxWidth);
  minHeight = PMAX(rxMinHeight, minHeight);
  maxHeight = PMIN(rxMaxHeight, maxHeight);

  // turn off any MPI that are outside the final bounding box
  for (i = 0; i < 5; i++) {
    if (StandardVideoSizes[i].width < minWidth || 
        StandardVideoSizes[i].width > maxWidth ||
        StandardVideoSizes[i].height < minHeight || 
        StandardVideoSizes[i].height > maxHeight)
     mpi[i] = PLUGINCODEC_MPI_DISABLED;
  }

  // find an appropriate max bit rate
  if (maxBitRate == 0) {
    if (maxBR != 0)
      maxBitRate = maxBR * 100;
    else if (targetBitRate != 0)
      maxBitRate = targetBitRate;
    else
      maxBitRate = 327000;
  }
  else if (maxBR > 0)
    maxBitRate = PMIN(maxBR * 100, maxBitRate);

  if (targetBitRate == 0)
    targetBitRate = 327000;
}

static int to_normalised_options(const struct PluginCodec_Definition *, void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  // find bounding box enclosing all MPI values
  int mpi[5];
  int minWidth, minHeight, maxHeight, maxWidth, frameTime, targetBitRate, maxBitRate;
  FindBoundingBox((const char * const * *)parm, mpi, minWidth, minHeight, maxWidth, maxHeight, frameTime, targetBitRate, maxBitRate);

  char ** options = (char **)calloc(16+(5*2)+2, sizeof(char *));
  *(char ***)parm = options;
  if (options == NULL)
    return 0;

  options[ 0] = strdup(PLUGINCODEC_OPTION_MIN_RX_FRAME_WIDTH);
  options[ 1] = num2str(minWidth);
  options[ 2] = strdup(PLUGINCODEC_OPTION_MIN_RX_FRAME_HEIGHT);
  options[ 3] = num2str(minHeight);
  options[ 4] = strdup(PLUGINCODEC_OPTION_MAX_RX_FRAME_WIDTH);
  options[ 5] = num2str(maxWidth);
  options[ 6] = strdup(PLUGINCODEC_OPTION_MAX_RX_FRAME_HEIGHT);
  options[ 7] = num2str(maxHeight);
  options[ 8] = strdup(PLUGINCODEC_OPTION_FRAME_TIME);
  options[ 9] = num2str(frameTime);
  options[10] = strdup(PLUGINCODEC_OPTION_MAX_BIT_RATE);
  options[11] = num2str(maxBitRate);
  options[12] = strdup(PLUGINCODEC_OPTION_TARGET_BIT_RATE);
  options[13] = num2str(targetBitRate);
  options[14] = strdup("MaxBR");
  options[15] = num2str((maxBitRate+50)/100);
  for (int i = 0; i < 5; i++) {
    options[16+i*2] = strdup(StandardVideoSizes[i].optionName);
    options[16+i*2+1] = num2str(mpi[i]);
  }

  return 1;
}

static int to_customised_options(const struct PluginCodec_Definition *, void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  // find bounding box enclosing all MPI values
  int mpi[5];
  int minWidth, minHeight, maxHeight, maxWidth, frameTime, targetBitRate, maxBitRate;
  FindBoundingBox((const char * const * *)parm, mpi, minWidth, minHeight, maxWidth, maxHeight, frameTime, targetBitRate, maxBitRate);

  char ** options = (char **)calloc(14+5*2+2, sizeof(char *));
  *(char ***)parm = options;
  if (options == NULL)
    return 0;

  options[ 0] = strdup(PLUGINCODEC_OPTION_MIN_RX_FRAME_WIDTH);
  options[ 1] = num2str(minWidth);
  options[ 2] = strdup(PLUGINCODEC_OPTION_MIN_RX_FRAME_HEIGHT);
  options[ 3] = num2str(minHeight);
  options[ 4] = strdup(PLUGINCODEC_OPTION_MAX_RX_FRAME_WIDTH);
  options[ 5] = num2str(maxWidth);
  options[ 6] = strdup(PLUGINCODEC_OPTION_MAX_RX_FRAME_HEIGHT);
  options[ 7] = num2str(maxHeight);
  options[ 8] = strdup(PLUGINCODEC_OPTION_MAX_BIT_RATE);
  options[ 9] = num2str(maxBitRate);
  options[10] = strdup(PLUGINCODEC_OPTION_TARGET_BIT_RATE);
  options[11] = num2str(targetBitRate);
  options[12] = strdup("MaxBR");
  options[13] = num2str((maxBitRate+50)/100);
  for (int i = 0; i < 5; i++) {
    options[14+i*2] = strdup(StandardVideoSizes[i].optionName);
    options[14+i*2+1] = num2str(mpi[i]);
  }

  return 1;
}

static int encoder_set_options(const PluginCodec_Definition *, 
                               void * _context,
                               const char * , 
                               void * parm, 
                               unsigned * parmLen)
{
  H263_Base_EncoderContext * context = (H263_Base_EncoderContext *)_context;
  if (parmLen == NULL || *parmLen != sizeof(const char **) || parm == NULL)
    return 0;

  context->Lock();
  context->CloseCodec();

  // get the "frame width" media format parameter to use as a hint for the encoder to start off
  for (const char * const * option = (const char * const *)parm; *option != NULL; option += 2) {
    if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_WIDTH) == 0)
      context->SetFrameWidth (atoi(option[1]));
    if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_HEIGHT) == 0)
      context->SetFrameHeight (atoi(option[1]));
    if (STRCMPI(option[0], PLUGINCODEC_OPTION_MAX_FRAME_SIZE) == 0)
      context->SetMaxRTPFrameSize (atoi(option[1]));
    if (STRCMPI(option[0], PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
       context->SetTargetBitrate(atoi(option[1]));
    if (STRCMPI(option[0], PLUGINCODEC_OPTION_TX_KEY_FRAME_PERIOD) == 0)
      context->SetMaxKeyFramePeriod (atoi(option[1]));
    if (STRCMPI(option[0], PLUGINCODEC_OPTION_TEMPORAL_SPATIAL_TRADE_OFF) == 0)
       context->SetTSTO (atoi(option[1]));

    if (STRCMPI(option[0], "Annex D") == 0)
      if (atoi(option[1]) == 1)
        context->EnableAnnex (D);
       else
        context->DisableAnnex (D);
    if (STRCMPI(option[0], "Annex F") == 0)
      if (atoi(option[1]) == 1)
        context->EnableAnnex (F);
       else
        context->DisableAnnex (F);
    if (STRCMPI(option[0], "Annex I") == 0)
      if (atoi(option[1]) == 1)
        context->EnableAnnex (I);
       else
        context->DisableAnnex (I);
    if (STRCMPI(option[0], "Annex K") == 0)
      if (atoi(option[1]) == 1)
        context->EnableAnnex (K);
       else
        context->DisableAnnex (K);
    if (STRCMPI(option[0], "Annex J") == 0)
      if (atoi(option[1]) == 1)
        context->EnableAnnex (J);
       else
        context->DisableAnnex (J);
    if (STRCMPI(option[0], "Annex S") == 0)
      if (atoi(option[1]) == 1)
        context->EnableAnnex (S);
       else
        context->DisableAnnex (S);
  }

  context->OpenCodec();
  context->Unlock();
  return 1;
}

static int encoder_get_output_data_size(const PluginCodec_Definition *, void *, const char *, void *, unsigned *)
{
  return 2000; //FIXME
}

/////////////////////////////////////////////////////////////////////////////

static void * create_decoder(const struct PluginCodec_Definition * codec)
{
  if (strcmp(codec->sourceFormat, h263Desc) == 0)
    return new H263_RFC2190_DecoderContext();
  else
    return new H263_RFC2429_DecoderContext();
}

static void destroy_decoder(const struct PluginCodec_Definition * /*codec*/, void * _context)
{
  H263_Base_DecoderContext * context = (H263_Base_DecoderContext *)_context;
  delete context;
}

static int codec_decoder(const struct PluginCodec_Definition *, 
                                           void * _context,
                                     const void * from, 
                                       unsigned * fromLen,
                                           void * to, 
                                       unsigned * toLen,
                                   unsigned int * flag)
{
  H263_Base_DecoderContext * context = (H263_Base_DecoderContext *)_context;
  return context->DecodeFrames((const BYTE *)from, *fromLen, (BYTE *)to, *toLen, *flag) ? 1 : 0;
}

static int decoder_get_output_data_size(const PluginCodec_Definition * codec, void *, const char *, void *, unsigned *)
{
  return sizeof(PluginCodec_Video_FrameHeader) + ((codec->parm.video.maxFrameWidth * codec->parm.video.maxFrameHeight * 3) / 2);
}

PLUGINCODEC_CONTROL_LOG_FUNCTION_DEF

/////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_information licenseInfo = {
  1145863600,                                                   // timestamp =  Mon 24 Apr 2006 07:26:40 AM UTC

  "Matthias Schneider, Craig Southeren"                         // source code author
  "Guilhem Tardy, Derek Smithies",
  "1.0",                                                        // source code version
  "openh323@openh323.org",                                      // source code email
  "http://sourceforge.net/projects/openh323",                   // source code URL
  "Copyright (C) 2007 Matthias Schneider"                       // source code copyright
  ", Copyright (C) 2006 by Post Increment"
  ", Copyright (C) 2005 Salyens"
  ", Copyright (C) 2001 March Networks Corporation"
  ", Copyright (C) 1999-2000 Equivalence Pty. Ltd.",
  "MPL 1.0",                                                    // source code license
  PluginCodec_License_MPL,                                      // source code license
  
  "FFMPEG",                                                     // codec description
  "Michael Niedermayer, Fabrice Bellard",                       // codec author
  "",                                                           // codec version
  "ffmpeg-devel-request@mplayerhq.hu",                          // codec email
  "http://ffmpeg.mplayerhq.hu",                                 // codec URL
  "Copyright (c) 2000-2001 Fabrice Bellard"                     // codec copyright information
  ", Copyright (c) 2002-2003 Michael Niedermayer",
  "GNU LESSER GENERAL PUBLIC LICENSE, Version 2.1, February 1999", // codec license
  PluginCodec_License_LGPL                                         // codec license code
};

static const char SQCIF_MPI[]  = PLUGINCODEC_SQCIF_MPI;
static const char QCIF_MPI[]   = PLUGINCODEC_QCIF_MPI;
static const char CIF_MPI[]    = PLUGINCODEC_CIF_MPI;
static const char CIF4_MPI[]   = PLUGINCODEC_CIF4_MPI;
static const char CIF16_MPI[]  = PLUGINCODEC_CIF16_MPI;

static PluginCodec_ControlDefn EncoderControls[] = {
  { PLUGINCODEC_CONTROL_VALID_FOR_PROTOCOL,    valid_for_protocol },
  { PLUGINCODEC_CONTROL_GET_CODEC_OPTIONS,     get_codec_options },
  { PLUGINCODEC_CONTROL_FREE_CODEC_OPTIONS,    free_codec_options },
  { PLUGINCODEC_CONTROL_TO_NORMALISED_OPTIONS, to_normalised_options },
  { PLUGINCODEC_CONTROL_TO_CUSTOMISED_OPTIONS, to_customised_options },
  { PLUGINCODEC_CONTROL_SET_CODEC_OPTIONS,     encoder_set_options },
  { PLUGINCODEC_CONTROL_GET_OUTPUT_DATA_SIZE,  encoder_get_output_data_size },
  PLUGINCODEC_CONTROL_LOG_FUNCTION_INC
  { NULL }
};

static PluginCodec_ControlDefn DecoderControls[] = {
  { PLUGINCODEC_CONTROL_GET_CODEC_OPTIONS,     get_codec_options },
  { PLUGINCODEC_CONTROL_GET_OUTPUT_DATA_SIZE,  decoder_get_output_data_size },
  { NULL }
};

static struct PluginCodec_Option const sqcifMPI =
{
  PluginCodec_IntegerOption,            // Option type
  SQCIF_MPI,                            // User visible name
  false,                                // User Read/Only flag
  PluginCodec_MaxMerge,                 // Merge mode
  "1",                                  // Initial value
  "SQCIF",                              // FMTP option name
  STRINGIZE(PLUGINCODEC_MPI_DISABLED),  // FMTP default value
  0,                                    // H.245 generic capability code and bit mask
  "1",                                  // Minimum value
  STRINGIZE(PLUGINCODEC_MPI_DISABLED)   // Maximum value
};

static struct PluginCodec_Option const qcifMPI =
{
  PluginCodec_IntegerOption,            // Option type
  QCIF_MPI,                             // User visible name
  false,                                // User Read/Only flag
  PluginCodec_MaxMerge,                 // Merge mode
  "1",                                  // Initial value
  "QCIF",                               // FMTP option name
  STRINGIZE(PLUGINCODEC_MPI_DISABLED),  // FMTP default value
  0,                                    // H.245 generic capability code and bit mask
  "1",                                  // Minimum value
  STRINGIZE(PLUGINCODEC_MPI_DISABLED)   // Maximum value
};

static struct PluginCodec_Option const cifMPI =
{
  PluginCodec_IntegerOption,            // Option type
  CIF_MPI,                              // User visible name
  false,                                // User Read/Only flag
  PluginCodec_MaxMerge,                 // Merge mode
  "1",                                  // Initial value
  "CIF",                                // FMTP option name
  STRINGIZE(PLUGINCODEC_MPI_DISABLED),  // FMTP default value
  0,                                    // H.245 generic capability code and bit mask
  "1",                                  // Minimum value
  STRINGIZE(PLUGINCODEC_MPI_DISABLED)   // Maximum value
};

static struct PluginCodec_Option const cif4MPI =
{
  PluginCodec_IntegerOption,            // Option type
  CIF4_MPI,                             // User visible name
  false,                                // User Read/Only flag
  PluginCodec_MaxMerge,                 // Merge mode
  "1",                                  // Initial value
  "CIF4",                               // FMTP option name
  STRINGIZE(PLUGINCODEC_MPI_DISABLED),  // FMTP default value
  0,                                    // H.245 generic capability code and bit mask
  "1",                                  // Minimum value
  STRINGIZE(PLUGINCODEC_MPI_DISABLED)   // Maximum value
};

static struct PluginCodec_Option const cif16MPI =
{
  PluginCodec_IntegerOption,            // Option type
  CIF16_MPI,                            // User visible name
  false,                                // User Read/Only flag
  PluginCodec_MaxMerge,                 // Merge mode
  "1",                                  // Initial value
  "CIF16",                              // FMTP option name
  STRINGIZE(PLUGINCODEC_MPI_DISABLED),  // FMTP default value
  0,                                    // H.245 generic capability code and bit mask
  "1",                                  // Minimum value
  STRINGIZE(PLUGINCODEC_MPI_DISABLED)   // Maximum value
};

static struct PluginCodec_Option const maxBR =
{
  PluginCodec_IntegerOption,          // Option type
  "MaxBR",                            // User visible name
  false,                              // User Read/Only flag
  PluginCodec_MinMerge,               // Merge mode
  "0",                                // Initial value
  "maxbr",                            // FMTP option name
  "0",                                // FMTP default value
  0,                                  // H.245 generic capability code and bit mask
  "0",                                // Minimum value
  "32767"                             // Maximum value
};

static struct PluginCodec_Option const mediaPacketization =
{
  PluginCodec_StringOption,           // Option type
  PLUGINCODEC_MEDIA_PACKETIZATION,    // User visible name
  true,                               // User Read/Only flag
  PluginCodec_EqualMerge,             // Merge mode
  "RFC2190"                           // Initial value
};

static struct PluginCodec_Option const sifMPI =
  { PluginCodec_StringOption, "SIF MPI", false, PluginCodec_EqualMerge, "320,240,1", "CUSTOM"};

static struct PluginCodec_Option const sif4MPI =
  { PluginCodec_StringOption, "SIF4 MPI", false, PluginCodec_EqualMerge, "640,480,1", "CUSTOM"};

static struct PluginCodec_Option const annexF =
  { PluginCodec_BoolOption,    "Annex F",   false,  PluginCodec_MinMerge, "1", "F", "0" };

static struct PluginCodec_Option const annexI =
  { PluginCodec_BoolOption,    "Annex I",   false,  PluginCodec_MinMerge, "1", "I", "0" };

static struct PluginCodec_Option const annexJ =
  { PluginCodec_BoolOption,    "Annex J",   true,  PluginCodec_MinMerge, "1", "J", "0" };

static struct PluginCodec_Option const annexK =
  { PluginCodec_IntegerOption, "Annex K",   true,  PluginCodec_EqualMerge, "0", "K", "0", 0, "0", "4" };

static struct PluginCodec_Option const annexN =
  { PluginCodec_BoolOption,    "Annex N",   true,  PluginCodec_AndMerge, "0", "N", "0" };

static struct PluginCodec_Option const annexP =
  { PluginCodec_BoolOption,    "Annex P",   true,  PluginCodec_AndMerge, "0", "P", "0" };

static struct PluginCodec_Option const annexT =
  { PluginCodec_BoolOption,    "Annex T",   true,  PluginCodec_AndMerge, "0", "T", "0" };

static struct PluginCodec_Option const annexD =
  { PluginCodec_BoolOption,    "Annex D",   true,  PluginCodec_MinMerge, "1", "D", "0" };

static struct PluginCodec_Option const * const h263POptionTable[] = {
  &qcifMPI,
  &cifMPI,
  &sqcifMPI,
  &cif4MPI,
  &cif16MPI,
  &sifMPI,
  &sif4MPI,
  &annexF,
  &annexI,
  &annexJ,
  &annexK,
  &annexN,
  &annexP,
  &annexT,
  &annexD,
  NULL
};


static struct PluginCodec_Option const * const h263OptionTable[] = {
  &mediaPacketization,
  &maxBR,
  //&videoQuality,
  //&minVideoQuality,
  //&maxVideoQuality,
  &qcifMPI,
  &cifMPI,
  &sqcifMPI,
  &cif4MPI,
  &cif16MPI,
  &annexF,
  NULL
};

/////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_Definition h263CodecDefn[] = {
{ 
  // All frame sizes (dynamic) encoder
  PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
  &licenseInfo,                       // license information

  PluginCodec_MediaTypeVideo |        // audio codec
  PluginCodec_RTPTypeShared |         // specified RTP type
  PluginCodec_RTPTypeDynamic,         // specified RTP type

  h263PDesc,                          // text decription
  YUV420PDesc,                        // source format
  h263PDesc,                          // destination format

  h263POptionTable,                   // user data 

  H263P_CLOCKRATE,                    // samples per second
  H263P_BITRATE,                      // raw bits per second
  20000,                              // nanoseconds per frame

  CIF4_WIDTH,                         // frame width
  CIF4_HEIGHT,                        // frame height
  10,                                 // recommended frame rate
  60,                                 // maximum frame rate
  0,                                  // IANA RTP payload code
  sdpH263P,                           // RTP payload name

  create_encoder,                     // create codec function
  destroy_encoder,                    // destroy codec
  codec_encoder,                      // encode/decode
  EncoderControls,                    // codec controls

  PluginCodec_H323Codec_NoH323,    // h323CapabilityType 
  NULL                                // h323CapabilityData
},
{ 
  // All frame sizes (dynamic) decoder
  PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
  &licenseInfo,                       // license information

  PluginCodec_MediaTypeVideo |        // audio codec
  PluginCodec_RTPTypeShared |         // specified RTP type
  PluginCodec_RTPTypeDynamic,         // specified RTP type

  h263PDesc,                          // text decription
  h263PDesc,                          // source format
  YUV420PDesc,                        // destination format

  h263POptionTable,                   // user data 

  H263P_CLOCKRATE,                    // samples per second
  H263P_BITRATE,                      // raw bits per second
  20000,                              // nanoseconds per frame

  CIF4_WIDTH,                         // frame width
  CIF4_HEIGHT,                        // frame height
  10,                                 // recommended frame rate
  60,                                 // maximum frame rate
  0,                                  // IANA RTP payload code
  sdpH263P,                           // RTP payload name

  create_decoder,                     // create codec function
  destroy_decoder,                    // destroy codec
  codec_decoder,                      // encode/decode
  DecoderControls,                    // codec controls

  PluginCodec_H323Codec_NoH323,    // h323CapabilityType 
  NULL                                // h323CapabilityData
},

{ 
  // H.263 (RFC 2190) encoder
  PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
  &licenseInfo,                       // license information

  PluginCodec_MediaTypeVideo |        // video codec
  PluginCodec_MediaTypeExtVideo |     // Extended video codec
  PluginCodec_RTPTypeExplicit,        // specified RTP type

  h263Desc,                           // text decription
  YUV420PDesc,                        // source format
  h263Desc,                           // destination format

  h263OptionTable,                    // user data 

  H263_CLOCKRATE,                     // samples per second
  H263_BITRATE,                       // raw bits per second
  20000,                              // nanoseconds per frame

  {{
    CIF16_WIDTH,                        // frame width
    CIF16_HEIGHT,                       // frame height
    10,                                 // recommended frame rate
    60,                                 // maximum frame rate
  }},

  RTP_RFC2190_PAYLOAD,                // IANA RTP payload code
  sdpH263,                            // RTP payload name

  create_encoder,                     // create codec function
  destroy_encoder,                    // destroy codec
  codec_encoder,                      // encode/decode
  EncoderControls,                    // codec controls

  PluginCodec_H323VideoCodec_h263,    // h323CapabilityType 
  NULL                                // h323CapabilityData
},
{ 
  // H.263 (RFC 2190) decoder
  PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
  &licenseInfo,                       // license information

  PluginCodec_MediaTypeVideo |        // video codec
  PluginCodec_MediaTypeExtVideo |     // Extended video codec
  PluginCodec_RTPTypeExplicit,        // specified RTP type

  h263Desc,                           // text decription
  h263Desc,                           // source format
  YUV420PDesc,                        // destination format

  h263OptionTable,                    // user data 

  H263_CLOCKRATE,                     // samples per second
  H263_BITRATE,                       // raw bits per second
  20000,                              // nanoseconds per frame

  {{
    CIF16_WIDTH,                        // frame width
    CIF16_HEIGHT,                       // frame height
    10,                                 // recommended frame rate
    60,                                 // maximum frame rate
  }},
  RTP_RFC2190_PAYLOAD,                // IANA RTP payload code
  sdpH263,                            // RTP payload name

  create_decoder,                     // create codec function
  destroy_decoder,                    // destroy codec
  codec_decoder,                      // encode/decode
  DecoderControls,                    // codec controls

  PluginCodec_H323VideoCodec_h263,    // h323CapabilityType 
  NULL                                // h323CapabilityData
}

};

/////////////////////////////////////////////////////////////////////////////

extern "C" {
  PLUGIN_CODEC_IMPLEMENT(FFMPEG_H263P)

  PLUGIN_CODEC_DLL_API struct PluginCodec_Definition * PLUGIN_CODEC_GET_CODEC_FN(unsigned * count, unsigned version)
  {
    if (version < PLUGIN_CODEC_VERSION_OPTIONS) {
      *count = 0;
      return NULL;
    }

    *count = sizeof(h263CodecDefn) / sizeof(struct PluginCodec_Definition);
    return h263CodecDefn;
  }

};
