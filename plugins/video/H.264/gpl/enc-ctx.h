/*
 * H.264 Plugin codec for OpenH323/OPAL
 *
 * Copyright (C) 2007 Matthias Schneider, All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef __ENC_CTX_H__
#define __ENC_CTX_H__ 1

#include <stdarg.h>
#ifdef _MSC_VER
  #include "../shared/vs-stdint.h"
#else
  #include <stdint.h>
#endif
#include "../shared/h264frame.h"

extern "C" {
#ifdef _MSC_VER
  #include "../x264/x264.h"
#else
  #include <x264.h>
#endif
};

#ifdef _WIN32
/* to keep compatibility with old build */
#define LIBX264_LINKED 1
#endif

#define CIF_WIDTH 352
#define CIF_HEIGHT 288
#define QCIF_WIDTH 176
#define QCIF_HEIGHT 144
#define IT_QCIF 0
#define IT_CIF 1

#define H264_BITRATE         768000
#define H264_PAYLOAD_SIZE      1400
#define H264_FRAME_RATE          25
#define H264_KEY_FRAME_INTERVAL 2.0

#if LIBX264_LINKED
  #define X264_ENCODER_OPEN x264_encoder_open 
  #define X264_PARAM_DEFAULT x264_param_default
  #define X264_ENCODER_ENCODE x264_encoder_encode
  #define X264_NAL_ENCODE x264_nal_encode
  #define X264_ENCODER_RECONFIG x264_encoder_reconfig
  #define X264_ENCODER_HEADERS x264_encoder_headers
  #define X264_ENCODER_CLOSE x264_encoder_close
  #define X264_PICTURE_ALLOC x264_picture_alloc
  #define X264_PICTURE_CLEAN x264_picture_clean
  #define X264_ENCODER_CLOSE x264_encoder_close
#else
  #include "x264loader_unix.h"
  #define X264_ENCODER_OPEN X264Lib.Xx264_encoder_open 
  #define X264_PARAM_DEFAULT X264Lib.Xx264_param_default
  #define X264_ENCODER_ENCODE X264Lib.Xx264_encoder_encode
  #define X264_NAL_ENCODE X264Lib.Xx264_nal_encode
  #define X264_ENCODER_RECONFIG X264Lib.Xx264_encoder_reconfig
  #define X264_ENCODER_HEADERS X264Lib.Xx264_encoder_headers
  #define X264_ENCODER_CLOSE X264Lib.Xx264_encoder_close
  #define X264_PICTURE_ALLOC X264Lib.Xx264_picture_alloc
  #define X264_PICTURE_CLEAN X264Lib.Xx264_picture_clean
  #define X264_ENCODER_CLOSE X264Lib.Xx264_encoder_close
#endif

class X264EncoderContext 
{
  public:
    X264EncoderContext ();
    ~X264EncoderContext ();

    int EncodeFrames (const unsigned char * src, unsigned & srcLen, unsigned char * dst, unsigned & dstLen, unsigned int & flags);

    void SetMaxRTPFrameSize (unsigned size);
    void SetMaxKeyFramePeriod (unsigned period);
    void SetTargetBitrate (unsigned rate);
    void SetFrameWidth (unsigned width);
    void SetFrameHeight (unsigned height);
    void SetFrameRate (unsigned rate);
    void SetTSTO (unsigned tsto);
    void SetProfileLevel (unsigned profileLevel);
    void ApplyOptions ();


  protected:

    x264_t* _codec;
    x264_param_t _context;
    x264_picture_t _inputFrame;
    H264Frame* _txH264Frame;

    uint32_t _PFramesSinceLastIFrame; // counts frames since last keyframe
    uint32_t _IFrameInterval; // confd frames between keyframes
    int _frameCounter;
} ;


#endif /* __ENC_CTX_H__ */
