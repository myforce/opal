/*
 * Common ffmpeg code for OPAL
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
 * Copyright (C) 2011 Vox Lucida Pty. Ltd.
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
 * Contributor(s): Guilhem Tardy (gtardy@salyens.com)
 *                 Craig Southeren (craigs@postincrement.com)
 *                 Matthias Schneider (ma30002000@yahoo.de)
 *                 Robert Jongbloed (robertj@voxlucida.com.au)
 */

#ifndef __FFMPEG_H__
#define __FFMPEG_H__ 1

#include "platform.h"
#include <codec/opalplugin.hpp>

extern "C" {
  #include "libavcodec/avcodec.h"
  #include "libavutil/mem.h"
};

#ifndef LIBAVCODEC_VERSION_INT
  #error Libavcodec include is not correct
#endif

// Compile time version checking
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(51,11,0)
  #error Libavcodec LIBAVCODEC_VERSION_INT too old.
#endif

// AVPacket was declared in avformat.h before April 2009
#if LIBAVCODEC_VERSION_INT <= AV_VERSION_INT(52, 25, 0)
  #include "libavformat/avformat.h"
#endif

#if LIBAVCODEC_VERSION_INT <= AV_VERSION_INT(53, 35, 0)
  #define AVCodecID          CodecID
  #define AV_CODEC_ID_H263   CODEC_ID_H263
  #define AV_CODEC_ID_H263P  CODEC_ID_H263P
  #define AV_CODEC_ID_H264   CODEC_ID_H264
  #define AV_CODEC_ID_MPEG4  CODEC_ID_MPEG4
  #define PICTURE_WIDTH      m_context->width
  #define PICTURE_HEIGHT     m_context->height
#else
  #define PICTURE_WIDTH      m_picture->width
  #define PICTURE_HEIGHT     m_picture->height
#endif


/////////////////////////////////////////////////////////////////

class FFMPEGCodec
{
  public:
    class EncodedFrame
    {
      protected:
        size_t    m_length;
        size_t    m_maxSize;
        void    * m_memory;
        uint8_t * m_buffer;
        size_t    m_maxPayloadSize;

      public:
        EncodedFrame();
        virtual ~EncodedFrame();

        virtual const char * GetName() const { return ""; }

        uint8_t * GetBuffer() const { return m_buffer; }
        size_t GetMaxSize() const { return m_maxSize; }
        size_t GetLength() const { return m_length; }
        size_t GetMaxPayloadSize() const { return m_maxPayloadSize; }

        virtual void SetMaxPayloadSize(size_t size);
        virtual bool SetResolution(unsigned width, unsigned height);
        virtual bool Reset(size_t len = 0);

        virtual bool GetPacket(PluginCodec_RTP & rtp, unsigned & flags) = 0;
        virtual bool AddPacket(const PluginCodec_RTP & rtp, unsigned & flags) = 0;

        virtual bool IsIntraFrame() const = 0;

        virtual void RTPCallBack(void * data, int size, int mbCount);

      protected:
        virtual bool Append(const uint8_t * data, size_t len);
    };

  protected:
    const char     * m_prefix;
    AVCodec        * m_codec;
    AVCodecContext * m_context;
    AVFrame        * m_picture;
    AVPacket         m_packet;
    void           * m_alignedInputYUV;
    size_t           m_alignedInputSize;
    EncodedFrame   * m_fullFrame;
    bool             m_open;
    int              m_errorCount;
    bool             m_hadMissingPacket;

  public:
    FFMPEGCodec(const char * prefix, EncodedFrame * fullFrame);
    ~FFMPEGCodec();

    virtual bool InitEncoder(AVCodecID codecId);
    virtual bool InitDecoder(AVCodecID codecId);

    bool SetResolution(unsigned width, unsigned height);
    void SetEncoderOptions(
      unsigned frameTime,
      unsigned maxBitRate,
      unsigned maxRTPSize,
      unsigned tsto,
      unsigned keyFramePeriod
    );

    virtual bool OpenCodec();
    virtual void CloseCodec();

    virtual bool EncodeVideoPacket(const PluginCodec_RTP & in, PluginCodec_RTP & out, unsigned & flags);
    virtual bool DecodeVideoPacket(const PluginCodec_RTP & in, unsigned & flags);

    virtual int EncodeVideoFrame(uint8_t * frame, size_t length, unsigned & flags);
    virtual bool DecodeVideoFrame(const uint8_t * frame, size_t length, unsigned & flags);

    EncodedFrame * GetEncodedFrame() const { return m_fullFrame; }

    virtual void ErrorCallback(unsigned level, const char * msg);

  protected:
    bool InitContext();
};


#endif // __FFMPEG_H__
