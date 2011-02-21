/*
 * H.264 Plugin codec for OpenH323/OPAL
 *
 * Copyright (C) Matthias Schneider, All Rights Reserved
 *
 * This code is based on the file h261codec.cxx from the OPAL project released
 * under the MPL 1.0 license which contains the following:
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
 * Contributor(s): Matthias Schneider (ma30002000@yahoo.de)
 *                 Michele Piccini (michele@piccini.com)
 *                 Derek Smithies (derek@indranet.co.nz)
 *
 *
 */

/*
  Notes
  -----

 */

#ifndef _MSC_VER
#include "plugin-config.h"
#endif

#include "h264-x264.h"

#ifdef WIN32
#include "h264pipe_win32.h"
#else
#include "h264pipe_unix.h"
#endif

#include "dyna.h"
#include "rtpframe.h"

#include <stdlib.h>
#include <string.h>

FFMPEGLibrary FFMPEGLibraryInstance(CODEC_ID_H264);
H264EncCtx H264EncCtxInstance;

static void logCallbackFFMPEG (void* v, int level, const char* fmt , va_list arg) {
  char buffer[512];
  int severity = 0;
  if (v) {
    switch (level)
    {
      case AV_LOG_QUIET: severity = 0; break;
      case AV_LOG_ERROR: severity = 1; break;
      case AV_LOG_INFO:  severity = 4; break;
      case AV_LOG_DEBUG: severity = 4; break;
      default:           severity = 4; break;
    }
    sprintf(buffer, "H264\tFFMPEG\t");
    vsprintf(buffer + strlen(buffer), fmt, arg);
    if (strlen(buffer) > 0)
      buffer[strlen(buffer)-1] = 0;
    PTRACE(severity, "H.264", buffer);
  }
}


static bool InitLibs()
{
  if (!FFMPEGLibraryInstance.IsLoaded()) {
    if (!FFMPEGLibraryInstance.Load()) {
      PTRACE(1, "H264", "Codec\tDisabled");
      return false;
    }
    FFMPEGLibraryInstance.AvLogSetLevel(AV_LOG_DEBUG);
    FFMPEGLibraryInstance.AvLogSetCallback(&logCallbackFFMPEG);
  }

  if (!H264EncCtxInstance.isLoaded()) {
    if (!H264EncCtxInstance.Load()) {
      PTRACE(1, "H264", "Codec\tDisabled");
      return false;
    }
  }

  return true;
}


static char * num2str(int num)
{
  char buf[20];
  sprintf(buf, "%i", num);
  return strdup(buf);
}

H264EncoderContext::H264EncoderContext()
{
  if (!InitLibs())
    return;

  H264EncCtxInstance.call(H264ENCODERCONTEXT_CREATE);
}

H264EncoderContext::~H264EncoderContext()
{
  WaitAndSignal m(_mutex);
  H264EncCtxInstance.call(H264ENCODERCONTEXT_DELETE);
}

void H264EncoderContext::ApplyOptions()
{
  H264EncCtxInstance.call(APPLY_OPTIONS);
}

void H264EncoderContext::SetMaxRTPFrameSize(unsigned size)
{
  H264EncCtxInstance.call(SET_MAX_FRAME_SIZE, size);
}

void H264EncoderContext::SetMaxKeyFramePeriod (unsigned period)
{
  H264EncCtxInstance.call(SET_MAX_KEY_FRAME_PERIOD, period);
}

void H264EncoderContext::SetTargetBitrate(unsigned rate)
{
  H264EncCtxInstance.call(SET_TARGET_BITRATE, rate);
}

void H264EncoderContext::SetFrameWidth(unsigned width)
{
  H264EncCtxInstance.call(SET_FRAME_WIDTH, width);
}

void H264EncoderContext::SetFrameHeight(unsigned height)
{
  H264EncCtxInstance.call(SET_FRAME_HEIGHT, height);
}

void H264EncoderContext::SetFrameRate(unsigned rate)
{
  H264EncCtxInstance.call(SET_FRAME_RATE, rate);
}

void H264EncoderContext::SetTSTO (unsigned tsto)
{
  H264EncCtxInstance.call(SET_TSTO, tsto);
}

void H264EncoderContext::SetProfileLevel (unsigned profile, unsigned constraints, unsigned level)
{
  unsigned profileLevel = (profile << 16) + (constraints << 8) + level;
  H264EncCtxInstance.call(SET_PROFILE_LEVEL, profileLevel);
}

int H264EncoderContext::EncodeFrames(const u_char * src, unsigned & srcLen, u_char * dst, unsigned & dstLen, unsigned int & flags)
{
  WaitAndSignal m(_mutex);

  int ret;
  unsigned int headerLen;

  RTPFrame dstRTP(dst, dstLen);
  headerLen = dstRTP.GetHeaderSize();

  H264EncCtxInstance.call(ENCODE_FRAMES, src, srcLen, dst, dstLen, headerLen, flags, ret);

  return ret;
}

void H264EncoderContext::Lock ()
{
  _mutex.Wait();
}

void H264EncoderContext::Unlock ()
{
  _mutex.Signal();
}

H264DecoderContext::H264DecoderContext()
{
  if (!InitLibs())
    return;

  _gotIFrame = false;
  _gotAGoodFrame = false;
  _frameCounter = 0; 
  _skippedFrameCounter = 0;
  _rxH264Frame = new H264Frame();

  if ((_codec = FFMPEGLibraryInstance.AvcodecFindDecoder(CODEC_ID_H264)) == NULL) {
    PTRACE(1, "H264", "Decoder\tCodec not found for decoder");
    return;
  }

  _context = FFMPEGLibraryInstance.AvcodecAllocContext();
  if (_context == NULL) {
    PTRACE(1, "H264", "Decoder\tFailed to allocate context for decoder");
    return;
  }

  _outputFrame = FFMPEGLibraryInstance.AvcodecAllocFrame();
  if (_outputFrame == NULL) {
    PTRACE(1, "H264", "Decoder\tFailed to allocate frame for encoder");
    return;
  }

  if (FFMPEGLibraryInstance.AvcodecOpen(_context, _codec) < 0) {
    PTRACE(1, "H264", "Decoder\tFailed to open H.264 decoder");
    return;
  }
  else
  {
    PTRACE(1, "H264", "Decoder\tDecoder successfully opened");
  }
}

H264DecoderContext::~H264DecoderContext()
{
  if (FFMPEGLibraryInstance.IsLoaded())
  {
    if (_context != NULL)
    {
      if (_context->codec != NULL)
      {
        FFMPEGLibraryInstance.AvcodecClose(_context);
        PTRACE(4, "H264", "Decoder\tClosed H.264 decoder, decoded " << _frameCounter << " Frames, skipped " << _skippedFrameCounter << " Frames" );
      }
    }

    FFMPEGLibraryInstance.AvcodecFree(_context);
    FFMPEGLibraryInstance.AvcodecFree(_outputFrame);
  }
  if (_rxH264Frame) delete _rxH264Frame;
}

int H264DecoderContext::DecodeFrames(const u_char * src, unsigned & srcLen, u_char * dst, unsigned & dstLen, unsigned int & flags)
{
  if (!FFMPEGLibraryInstance.IsLoaded()) return 0;

  // create RTP frame from source buffer
  RTPFrame srcRTP(src, srcLen);

  // create RTP frame from destination buffer
  RTPFrame dstRTP(dst, dstLen, 0);
  dstLen = 0;

  if (!_rxH264Frame->SetFromRTPFrame(srcRTP, flags)) {
    _rxH264Frame->BeginNewFrame();
    flags = (_gotAGoodFrame ? requestIFrame : 0);
    _gotAGoodFrame = false;
    return 1;
  }

  if (srcRTP.GetMarker()==0)
  {
    return 1;
  } 

  if (_rxH264Frame->GetFrameSize()==0)
  {
    _rxH264Frame->BeginNewFrame();
    PTRACE(4, "H264", "Decoder\tGot an empty frame - skipping");
    _skippedFrameCounter++;
    flags = (_gotAGoodFrame ? requestIFrame : 0);
    _gotAGoodFrame = false;
    return 1;
  }

  PTRACE(4, "H264", "Decoder\tDecoding " << _rxH264Frame->GetFrameSize()  << " bytes");

  // look and see if we have read an I frame.
  if (_gotIFrame == 0)
  {
    if (!_rxH264Frame->IsSync())
    {
      PTRACE(1, "H264", "Decoder\tWaiting for an I-Frame");
      _rxH264Frame->BeginNewFrame();
      flags = (_gotAGoodFrame ? requestIFrame : 0);
      _gotAGoodFrame = false;
      return 1;
    }
    _gotIFrame = 1;
  }

  int gotPicture = 0;
  uint32_t bytesUsed = 0;
  int bytesDecoded = FFMPEGLibraryInstance.AvcodecDecodeVideo(_context, _outputFrame, &gotPicture, _rxH264Frame->GetFramePtr() + bytesUsed, _rxH264Frame->GetFrameSize() - bytesUsed);

  _rxH264Frame->BeginNewFrame();
  if (!gotPicture) 
  {
    PTRACE(1, "H264", "Decoder\tDecoded "<< bytesDecoded << " bytes without getting a Picture..."); 
    _skippedFrameCounter++;
    flags = (_gotAGoodFrame ? requestIFrame : 0);
    _gotAGoodFrame = false;
    return 1;
  }

  PTRACE(4, "H264", "Decoder\tDecoded " << bytesDecoded << " bytes"<< ", Resolution: " << _context->width << "x" << _context->height);
  int frameBytes = (_context->width * _context->height * 3) / 2;
  PluginCodec_Video_FrameHeader * header = (PluginCodec_Video_FrameHeader *)dstRTP.GetPayloadPtr();
  header->x = header->y = 0;
  header->width = _context->width;
  header->height = _context->height;

  int size = _context->width * _context->height;
  if (_outputFrame->data[1] == _outputFrame->data[0] + size
      && _outputFrame->data[2] == _outputFrame->data[1] + (size >> 2))
  {
    memcpy(OPAL_VIDEO_FRAME_DATA_PTR(header), _outputFrame->data[0], frameBytes);
  }
  else 
  {
    unsigned char *dstData = OPAL_VIDEO_FRAME_DATA_PTR(header);
    for (int i=0; i<3; i ++)
    {
      unsigned char *srcData = _outputFrame->data[i];
      int dst_stride = i ? _context->width >> 1 : _context->width;
      int src_stride = _outputFrame->linesize[i];
      int h = i ? _context->height >> 1 : _context->height;

      if (src_stride==dst_stride)
      {
        memcpy(dstData, srcData, dst_stride*h);
        dstData += dst_stride*h;
      }
      else
      {
        while (h--)
        {
          memcpy(dstData, srcData, dst_stride);
          dstData += dst_stride;
          srcData += src_stride;
        }
      }
    }
  }

  dstRTP.SetPayloadSize(sizeof(PluginCodec_Video_FrameHeader) + frameBytes);
  dstRTP.SetTimestamp(srcRTP.GetTimestamp());
  dstRTP.SetMarker(1);
  dstLen = dstRTP.GetFrameLen();

  flags = PluginCodec_ReturnCoderLastFrame;
  _frameCounter++;
  _gotAGoodFrame = true;
  return 1;
}

/////////////////////////////////////////////////////////////////////////////

static int get_codec_options(const struct PluginCodec_Definition * codec,
                                                  void *,
                                                  const char *,
                                                  void * parm,
                                                  unsigned * parmLen)
{
  if (!InitLibs())
    return false;

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

static int int_from_string(std::string str)
{
  if (str.find_first_of("\"") != std::string::npos)
    return (atoi( str.substr(1, str.length()-2).c_str()));

  return (atoi( str.c_str()));
}

static void profile_level_from_string  (std::string profileLevelString, unsigned & profile, unsigned & constraints, unsigned & level)
{

  if (profileLevelString.find_first_of("\"") != std::string::npos)
    profileLevelString = profileLevelString.substr(1, profileLevelString.length()-2);

  unsigned profileLevelInt = strtoul(profileLevelString.c_str(), NULL, 16);

  if (profileLevelInt == 0) {
#ifdef DEFAULT_TO_FULL_CAPABILITIES
    // Baseline, Level 3
    profileLevelInt = 0x42C01E;
#else
    // Default handling according to RFC 3984
    // Baseline, Level 1
    profileLevelInt = 0x42C00A;
#endif  
  }

  profile     = (profileLevelInt & 0xFF0000) >> 16;
  constraints = (profileLevelInt & 0x00FF00) >> 8;
  level       = (profileLevelInt & 0x0000FF);
}

static int valid_for_protocol (const struct PluginCodec_Definition * codec,
                                                  void *,
                                                  const char *,
                                                  void * parm,
                                                  unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char *))
    return 0;

  if (codec->h323CapabilityType != PluginCodec_H323Codec_NoH323)
    return (STRCMPI((const char *)parm, "h.323") == 0 ||
            STRCMPI((const char *)parm, "h323") == 0) ? 1 : 0;	        
   else 
    return (STRCMPI((const char *)parm, "sip") == 0) ? 1 : 0;
}

static int adjust_bitrate_to_level (unsigned & targetBitrate, unsigned level, int idx = -1)
{
  int i = 0;
  if (idx == -1) { 
    while (h264_levels[i].level_idc) {
      if (h264_levels[i].level_idc == level)
        break;
    i++; 
    }
  
    if (!h264_levels[i].level_idc) {
      PTRACE(1, "H264", "Cap\tIllegal Level negotiated");
      return 0;
    }
  }
  else
    i = idx;

// Correct Target Bitrate
  PTRACE(4, "H264", "Cap\tBitrate: " << targetBitrate << "(" << h264_levels[i].bitrate << ")");
  if (targetBitrate > h264_levels[i].bitrate)
    targetBitrate = h264_levels[i].bitrate;

  return 1;
}

static int adjust_to_level (unsigned & width, unsigned & height, unsigned & frameTime, unsigned & targetBitrate, unsigned level)
{
  int i = 0;
  while (h264_levels[i].level_idc) {
    if (h264_levels[i].level_idc == level)
      break;
   i++; 
  }

  if (!h264_levels[i].level_idc) {
    PTRACE(1, "H264", "Cap\tIllegal Level negotiated");
    return 0;
  }

// Correct max. number of macroblocks per frame
  uint32_t nbMBsPerFrame = width * height / 256;
  unsigned j = 0;
  PTRACE(4, "H264", "Cap\tFrame Size: " << nbMBsPerFrame << "(" << h264_levels[i].frame_size << ")");
  if    ( (nbMBsPerFrame          > h264_levels[i].frame_size)
       || (width  * width  / 2048 > h264_levels[i].frame_size)
       || (height * height / 2048 > h264_levels[i].frame_size) ) {

    while (h264_resolutions[j].width) {
      if  ( (h264_resolutions[j].macroblocks                                <= h264_levels[i].frame_size) 
         && (h264_resolutions[j].width  * h264_resolutions[j].width  / 2048 <= h264_levels[i].frame_size) 
         && (h264_resolutions[j].height * h264_resolutions[j].height / 2048 <= h264_levels[i].frame_size) )
          break;
      j++; 
    }
    if (!h264_resolutions[j].width) {
      PTRACE(1, "H264", "Cap\tNo Resolution found that has number of macroblocks <=" << h264_levels[i].frame_size);
      return 0;
    }
    else {
      width  = h264_resolutions[j].width;
      height = h264_resolutions[j].height;
    }
  }

// Correct macroblocks per second
  uint32_t nbMBsPerSecond = width * height / 256 * (90000 / frameTime);
  PTRACE(4, "H264", "Cap\tMB/s: " << nbMBsPerSecond << "(" << h264_levels[i].mbps << ")");
  if (nbMBsPerSecond > h264_levels[i].mbps)
    frameTime =  (unsigned) (90000 / 256 * width  * height / h264_levels[i].mbps );

  adjust_bitrate_to_level (targetBitrate, level, i);
  return 1;
}

/////////////////////////////////////////////////////////////////////////////

static void * create_encoder(const struct PluginCodec_Definition * /*codec*/)
{
  return new H264EncoderContext;
}

static void destroy_encoder(const struct PluginCodec_Definition * /*codec*/, void * _context)
{
  H264EncoderContext * context = (H264EncoderContext *)_context;
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
  H264EncoderContext * context = (H264EncoderContext *)_context;
  return context->EncodeFrames((const u_char *)from, *fromLen, (u_char *)to, *toLen, *flag);
}

static int to_normalised_options(const struct PluginCodec_Definition *, void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  unsigned profile = 66;
  unsigned constraints = 0;
  unsigned level = 51;
  unsigned width = 352;
  unsigned height = 288;
  unsigned frameTime = 3000;
  unsigned targetBitrate = 64000;

  for (const char * const * option = *(const char * const * *)parm; *option != NULL; option += 2) {
      if (STRCMPI(option[0], "CAP RFC3894 Profile Level") == 0) {
        profile_level_from_string(option[1], profile, constraints, level);
      }
      if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_WIDTH) == 0)
        width = atoi(option[1]);
      if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_HEIGHT) == 0)
        height = atoi(option[1]);
      if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_TIME) == 0)
        frameTime = atoi(option[1]);
      if (STRCMPI(option[0], PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
        targetBitrate = atoi(option[1]);
  }

  PTRACE(4, "H264", "Cap\tProfile and Level: " << profile << ";" << constraints << ";" << level);

  // Though this is not a strict requirement we enforce 
  //it here in order to obtain optimal compression results
  width -= width % 16;
  height -= height % 16;

  if (!adjust_to_level (width, height, frameTime, targetBitrate, level))
    return 0;

  char ** options = (char **)calloc(9, sizeof(char *));
  *(char ***)parm = options;
  if (options == NULL)
    return 0;

  options[0] = strdup(PLUGINCODEC_OPTION_FRAME_WIDTH);
  options[1] = num2str(width);
  options[2] = strdup(PLUGINCODEC_OPTION_FRAME_HEIGHT);
  options[3] = num2str(height);
  options[4] = strdup(PLUGINCODEC_OPTION_FRAME_TIME);
  options[5] = num2str(frameTime);
  options[6] = strdup(PLUGINCODEC_OPTION_TARGET_BIT_RATE);
  options[7] = num2str(targetBitrate);

  return 1;
}


static int to_customised_options(const struct PluginCodec_Definition *, void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  for (const char * const * option = *(const char * const * *)parm; *option != NULL; option += 2) {
/*      if (STRCMPI(option[0], PLUGINCODEC_OPTION_MIN_RX_FRAME_WIDTH) == 0)
        h263MPIList.setMinWidth(atoi(option[1]));
      if (STRCMPI(option[0], PLUGINCODEC_OPTION_MIN_RX_FRAME_HEIGHT) == 0)
        h263MPIList.setMinHeight(atoi(option[1]));
      if (STRCMPI(option[0], PLUGINCODEC_OPTION_MAX_RX_FRAME_WIDTH) == 0)
        h263MPIList.setMaxWidth(atoi(option[1]));
      if (STRCMPI(option[0], PLUGINCODEC_OPTION_MAX_RX_FRAME_HEIGHT) == 0)
        h263MPIList.setMaxHeight(atoi(option[1]));*/
  }


  char ** options = (char **)calloc(3, sizeof(char *));
  *(char ***)parm = options;
  if (options == NULL)
    return 0;

//   options[0] = strdup(PLUGINCODEC_QCIF_MPI);
//   options[1] = num2str(qcif_mpi);

  return 1;
}

static int encoder_set_options(
      const struct PluginCodec_Definition *, 
      void * _context, 
      const char *, 
      void * parm, 
      unsigned * parmLen)
{
  H264EncoderContext * context = (H264EncoderContext *)_context;

  if (parmLen == NULL || *parmLen != sizeof(const char **)) 
    return 0;

  context->Lock();
  unsigned profile = 66;
  unsigned constraints = 0;
  unsigned level = 51;
  if (parm != NULL) {
    const char ** options = (const char **)parm;
    int i;
    unsigned targetBitrate = 64000;
    for (i = 0; options[i] != NULL; i += 2) {
      if (STRCMPI(options[i], "CAP RFC3894 Profile Level") == 0) {
        profile_level_from_string (options[i+1], profile, constraints, level);
      }
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
         targetBitrate = atoi(options[i+1]);
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_TIME) == 0)
         context->SetFrameRate((int)(H264_CLOCKRATE / atoi(options[i+1])));
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_HEIGHT) == 0)
         context->SetFrameHeight(atoi(options[i+1]));
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_WIDTH) == 0)
         context->SetFrameWidth(atoi(options[i+1]));
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_MAX_FRAME_SIZE) == 0)
         context->SetMaxRTPFrameSize(atoi(options[i+1]));
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_TX_KEY_FRAME_PERIOD) == 0)
        context->SetMaxKeyFramePeriod (atoi(options[i+1]));
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_TEMPORAL_SPATIAL_TRADE_OFF) == 0)
        context->SetTSTO (atoi(options[i+1]));

    }
    PTRACE(4, "H264", "Cap\tProfile and Level: " << profile << ";" << constraints << ";" << level);

    if (!adjust_bitrate_to_level (targetBitrate, level)) {
      context->Unlock();
      return 0;
    }

    context->SetTargetBitrate((unsigned) (targetBitrate / 1000) );
    context->SetProfileLevel(profile, constraints, level);
    context->ApplyOptions();
    context->Unlock();

  }
  return 1;
}

static int encoder_get_output_data_size(const PluginCodec_Definition *, void *, const char *, void *, unsigned *)
{
  return 2000; //FIXME
}
/////////////////////////////////////////////////////////////////////////////

static void * create_decoder(const struct PluginCodec_Definition *)
{
  return new H264DecoderContext;
}

static void destroy_decoder(const struct PluginCodec_Definition * /*codec*/, void * _context)
{
  H264DecoderContext * context = (H264DecoderContext *)_context;
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
  H264DecoderContext * context = (H264DecoderContext *)_context;
  return context->DecodeFrames((const u_char *)from, *fromLen, (u_char *)to, *toLen, *flag);
}

static int decoder_get_output_data_size(const PluginCodec_Definition * codec, void *, const char *, void *, unsigned *)
{
  return sizeof(PluginCodec_Video_FrameHeader) + ((codec->parm.video.maxFrameWidth * codec->parm.video.maxFrameHeight * 3) / 2);
}

/////////////////////////////////////////////////////////////////////////////
static int merge_profile_level_h264(char ** result, const char * dst, const char * src)
{
  // c0: obeys A.2.1 Baseline
  // c1: obeys A.2.2 Main
  // c2: obeys A.2.3, Extended
  // c3: if profile_idc profile = 66, 77, 88, and level =11 and c3: obbeys annexA for level 1b

  unsigned srcProfile, srcConstraints, srcLevel;
  unsigned dstProfile, dstConstraints, dstLevel;
  profile_level_from_string(src, srcProfile, srcConstraints, srcLevel);
  profile_level_from_string(dst, dstProfile, dstConstraints, dstLevel);

  switch (srcLevel) {
    case 10:
      srcLevel = 8;
      break;
    default:
      break;
  }

  switch (dstLevel) {
    case 10:
      dstLevel = 8;
      break;
    default:
      break;
  }

  if (dstProfile > srcProfile) {
    dstProfile = srcProfile;
  }

  dstConstraints |= srcConstraints;

  if (dstLevel > srcLevel)
    dstLevel = srcLevel;

  switch (dstLevel) {
    case 8:
      dstLevel = 10;
      break;
    default:
      break;
  }


  char buffer[10];
  sprintf(buffer, "%x", (dstProfile<<16)|(dstConstraints<<8)|(dstLevel));
  
  *result = strdup(buffer);

  PTRACE(4, "H264", "Cap\tCustom merge profile-level: " << src << " and " << dst << " to " << *result);

  return true;
}

static int merge_packetization_mode(char ** result, const char * dst, const char * src)
{
  unsigned srcInt = int_from_string (src); 
  unsigned dstInt = int_from_string (dst); 

  if (srcInt == 5) {
#ifdef DEFAULT_TO_FULL_CAPABILITIES
    srcInt = 1;
#else
    // Default handling according to RFC 3984
    srcInt = 0;
#endif
  }

  if (dstInt == 5) {
#ifdef DEFAULT_TO_FULL_CAPABILITIES
    dstInt = 1;
#else
    // Default handling according to RFC 3984
    dstInt = 0;
#endif
  }

  if (dstInt > srcInt)
    dstInt = srcInt;

  char buffer[10];
  sprintf(buffer, "%d", dstInt);
  
  *result = strdup(buffer);

  PTRACE(4, "H264", "Cap\tCustom merge packetization-mode: " << src << " and " << dst << " to " << *result);

//  if (dstInt > 0)
    return 1;

//  return 0;
}

static void free_string(char * str)
{
  free(str);
}

/////////////////////////////////////////////////////////////////////////////

extern "C" {

PLUGIN_CODEC_IMPLEMENT(H264)

PLUGIN_CODEC_DLL_API struct PluginCodec_Definition * PLUGIN_CODEC_GET_CODEC_FN(unsigned * count, unsigned version)
{
  if (version < PLUGIN_CODEC_VERSION_OPTIONS) {
    *count = 0;
    PTRACE(1, "H264", "Codec\tDisabled - plugin version mismatch");
    return NULL;
  }

  *count = sizeof(h264CodecDefn) / sizeof(struct PluginCodec_Definition);
  PTRACE(1, "H264", "Codec\tEnabled");
  return h264CodecDefn;
}
};
