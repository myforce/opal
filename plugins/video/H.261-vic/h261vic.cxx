/*
 * H.261 Plugin codec for OpenH323/OPAL
 *
 * Copyright (C) 2005 Post Increment, All Rights Reserved
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
 * Contributor(s): Michele Piccini (michele@piccini.com)
 *                 Derek Smithies (derek@indranet.co.nz)
 *
 * $Id$
 */

/*
  Notes
  -----

  This codec implements a H.261 encoder and decoder with RTP packaging as per 
  RFC 2032 "RTP Payload Format for H.261 Video Streams". As per this specification,
  The RTP payload code is always set to 31

  The encoder only creates I-frames

  The decoder can accept either I-frames or P-frames

  There are seperate encoder/decoder pairs which ensures that the encoder/decoder
  always knows whether it will be receiving CIF or QCIF data, and a third pair
  which will determine the size from the received data stream.
 */

//#define DEBUG_OUTPUT 1

#define _CRT_NONSTDC_NO_DEPRECATE 1
#define _CRT_SECURE_NO_WARNINGS 1

#include <codec/opalplugin.h>

#include <stdlib.h>
#ifdef _WIN32
  #include <malloc.h>
  #define STRCMPI  _strcmpi
#else
  #include <sys/time.h>
  #include <unistd.h>
  #include <semaphore.h>
  #define STRCMPI  strcasecmp
#endif
#include <string.h>

#include <stdio.h>

#ifdef _MSC_VER
#pragma warning(disable:4800)
#endif

#define BEST_ENCODER_QUALITY   1
#define WORST_ENCODER_QUALITY 31
#define DEFAULT_ENCODER_QUALITY 10

#define DEFAULT_FILL_LEVEL     5

#define RTP_RFC2032_PAYLOAD   31
#define RTP_DYNAMIC_PAYLOAD   96

#define H261_CLOCKRATE    90000
#define H261_BITRATE      621700

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;

#include "vic/p64.h"
#include "vic/p64encoder.h"

#ifdef _MSC_VER
   #include "../common/trace.h"
#else
   #include "trace.h"
#endif


#if DEBUG_OUTPUT
#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define CREAT(f) _open(f, _O_WRONLY | _O_TRUNC | _O_CREAT | _O_BINARY, 0600 )
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#define CREAT(f) ::creat(f, 0666)
#endif

static void debug_write_data(int & fd, const char * title, const char * filename, const void * buffer, size_t writeLen)
{
  if (fd == -1) {
   if ((fd = CREAT(filename)) < 0) 
    printf("cannot open %s file\n", title);
  }
  if (fd >= 0) {
    size_t len = write(fd, buffer, writeLen);
    if (len != writeLen) 
      printf("failed to write %s buffer - %i != %i\n", title, len, writeLen);
    else
      printf("wrote %i bytes to %s\n", writeLen, title);
  }
}

#endif

/////////////////////////////////////////////////////////////////
//
// define a class to implement a critical section mutex
// based on PCriticalSection from PWLib

class CriticalSection
{
  public:
    CriticalSection()
    { 
#ifdef _WIN32
      ::InitializeCriticalSection(&criticalSection); 
#else
      ::sem_init(&sem, 0, 1);
#endif
    }

    ~CriticalSection()
    { 
#ifdef _WIN32
      ::DeleteCriticalSection(&criticalSection); 
#else
      ::sem_destroy(&sem);
#endif
    }

    void Wait()
    { 
#ifdef _WIN32
      ::EnterCriticalSection(&criticalSection); 
#else
      ::sem_wait(&sem);
#endif
    }

    void Signal()
    { 
#ifdef _WIN32
      ::LeaveCriticalSection(&criticalSection); 
#else
      ::sem_post(&sem); 
#endif
    }

  private:
    CriticalSection & operator=(const CriticalSection &) { return *this; }
#ifdef _WIN32
    mutable CRITICAL_SECTION criticalSection; 
#else
    mutable sem_t sem;
#endif
};
    
class WaitAndSignal {
  public:
    inline WaitAndSignal(const CriticalSection & cs)
      : sync((CriticalSection &)cs)
    { sync.Wait(); }

    ~WaitAndSignal()
    { sync.Signal(); }

    WaitAndSignal & operator=(const WaitAndSignal &) 
    { return *this; }

  protected:
    CriticalSection & sync;
};

/////////////////////////////////////////////////////////////////////////////
//
// define some simple RTP packet routines
//

#define RTP_MIN_HEADER_SIZE 12

class RTPFrame
{
  public:
    RTPFrame(const unsigned char * _packet, int _packetLen)
      : packet((unsigned char *)_packet), packetLen(_packetLen)
    {
    }

    RTPFrame(unsigned char * _packet, int _packetLen, unsigned char payloadType)
      : packet(_packet), packetLen(_packetLen)
    { 
      if (packetLen > 0)
        packet[0] = 0x80;    // set version, no extensions, zero contrib count
      SetPayloadType(payloadType);
    }

    inline unsigned long GetLong(int offs) const
    {
      if (offs + 4 > packetLen)
        return 0;
      return (packet[offs + 0] << 24) + (packet[offs+1] << 16) + (packet[offs+2] << 8) + packet[offs+3]; 
    }

    inline void SetLong(int offs, unsigned long n)
    {
      if (offs + 4 <= packetLen) {
        packet[offs + 0] = (u_char)((n >> 24) & 0xff);
        packet[offs + 1] = (u_char)((n >> 16) & 0xff);
        packet[offs + 2] = (u_char)((n >> 8) & 0xff);
        packet[offs + 3] = (u_char)(n & 0xff);
      }
    }

    inline unsigned short GetShort(int offs) const
    { 
      if (offs + 2 > packetLen)
        return 0;
      return (packet[offs + 0] << 8) + packet[offs + 1]; 
    }

    inline void SetShort(int offs, unsigned short n) 
    { 
      if (offs + 2 <= packetLen) {
        packet[offs + 0] = (u_char)((n >> 8) & 0xff);
        packet[offs + 1] = (u_char)(n & 0xff);
      }
    }

    inline int GetPacketLen() const                    { return packetLen; }
    inline unsigned GetVersion() const                 { return (packetLen < 1) ? 0 : (packet[0]>>6)&3; }
    inline bool GetExtension() const                   { return (packetLen < 1) ? 0 : (packet[0]&0x10) != 0; }
    inline bool GetMarker()  const                     { return (packetLen < 2) ? 0 : ((packet[1]&0x80) != 0); }
    inline unsigned char GetPayloadType() const        { return (packetLen < 2) ? 0 : (packet[1] & 0x7f);  }
    inline unsigned short GetSequenceNumber() const    { return GetShort(2); }
    inline unsigned long GetTimestamp() const          { return GetLong(4); }
    inline unsigned long GetSyncSource() const         { return GetLong(8); }
    inline int GetContribSrcCount() const              { return (packetLen < 1) ? 0  : (packet[0]&0xf); }
    inline int GetExtensionSize() const                { return !GetExtension() ? 0  : GetShort(RTP_MIN_HEADER_SIZE + 4*GetContribSrcCount() + 2); }
    inline int GetExtensionType() const                { return !GetExtension() ? -1 : GetShort(RTP_MIN_HEADER_SIZE + 4*GetContribSrcCount()); }
    inline int GetPayloadSize() const                  { return packetLen - GetHeaderSize(); }
    inline unsigned char * GetPayloadPtr() const       { return packet + GetHeaderSize(); }
    inline unsigned char * GetPacketPtr() const        { return packet; }

    inline unsigned int GetHeaderSize() const    
    { 
      unsigned int sz = RTP_MIN_HEADER_SIZE + 4*GetContribSrcCount();
      if (GetExtension())
        sz += 4 + GetExtensionSize();
      return sz;
    }

    inline void SetMarker(bool m)                    { if (packetLen >= 2) packet[1] = (packet[1] & 0x7f) | (m ? 0x80 : 0x00); }
    inline void SetPayloadType(unsigned char t)      { if (packetLen >= 2) packet[1] = (packet[1] & 0x80) | (t & 0x7f); }
    inline void SetSequenceNumber(unsigned short v)  { SetShort(2, v); }
    inline void SetTimestamp(unsigned long n)        { SetLong(4, n); }
    inline void SetSyncSource(unsigned long n)       { SetLong(8, n); }
    inline void SetPayloadSize(int payloadSize)      { packetLen = GetHeaderSize() + payloadSize; }

  protected:
    unsigned char * packet;
    int packetLen;
};

/////////////////////////////////////////////////////////////////////////////

class H261EncoderContext 
{
  public:
    P64Encoder * videoEncoder;
    unsigned frameWidth;
    unsigned frameHeight;
    long waitFactor;
    bool packetDelay;
    unsigned frameLength;
    #ifdef _WIN32
      long newTime;
    #else
      timeval newTime;
    #endif

    bool forceIFrame;
    int videoQuality;
    unsigned long lastTimeStamp;
    CriticalSection mutex;
  
    H261EncoderContext()
    {
      frameWidth = frameHeight = 0;
      videoEncoder = new P64Encoder(DEFAULT_ENCODER_QUALITY, DEFAULT_FILL_LEVEL);
      forceIFrame = false;
      videoQuality = DEFAULT_ENCODER_QUALITY;
      waitFactor = 0;
      packetDelay = false;

    #ifdef _WIN32
      newTime = 0;
    #else
      newTime.tv_sec = 0;
      newTime.tv_usec = 0;
    #endif
      frameLength = 0;
    }

    ~H261EncoderContext()
    {
      delete videoEncoder;
    }

    void SetFrameSize(int w, int h)
    {
      frameWidth = w;
      frameHeight = h;
      videoEncoder->SetSize(frameWidth, frameHeight);
    }

    void SetTargetBitRate(unsigned bitrate) 
    {
      #ifdef _WIN32
        waitFactor = bitrate;
      #else
        if (bitrate==0)
          waitFactor = 0;
         else
          waitFactor = 8000000 / bitrate;  // on UNIX we deal with usecs
      #endif
    }

    int EncodeFrames(const u_char * src, unsigned & srcLen, u_char * dst, unsigned & dstLen, unsigned int & flags)
    {
      WaitAndSignal m(mutex);

      // create RTP frame from source buffer
      RTPFrame srcRTP(src, srcLen);

      // create RTP frame from destination buffer
      RTPFrame dstRTP(dst, dstLen, RTP_RFC2032_PAYLOAD);
      dstLen = 0;

      // return more pending data frames, if any
      if (videoEncoder->MoreToIncEncode()) {
        unsigned payloadLength = 0;
        videoEncoder->IncEncodeAndGetPacket((u_char *)dstRTP.GetPayloadPtr(), payloadLength); //get next packet on list
        dstLen = SetEncodedPacket(dstRTP, !videoEncoder->MoreToIncEncode(), RTP_RFC2032_PAYLOAD, lastTimeStamp, payloadLength, flags);
#if DEBUG_OUTPUT
static int encoderOutput = -1;
debug_write_data(encoderOutput, "encoder output", "encoder.output", dstRTP.GetPacketPtr(), dstLen);
#endif
        return 1;
      }

      //
      // from here on, this is encoding a new frame
      //

      // get the timestamp we will be using for the rest of the frame
      lastTimeStamp = srcRTP.GetTimestamp();

      // update the video quality
      videoEncoder->SetQualityLevel(videoQuality);

      // get and validate header
      if (srcRTP.GetPayloadSize() < sizeof(PluginCodec_Video_FrameHeader)) {
        TRACE(1,"H261\tVideo grab too small");
        return 0;
      } 

      PluginCodec_Video_FrameHeader * header = (PluginCodec_Video_FrameHeader *)srcRTP.GetPayloadPtr();
      if (header->x != 0 && header->y != 0) {
        TRACE(1,"H261\tVideo grab of partial frame unsupported");
        return 0;
      }

      // make sure the incoming frame is big enough for the specified frame size
      if (srcRTP.GetPayloadSize() < (int)(sizeof(PluginCodec_Video_FrameHeader) + frameWidth*frameHeight*12/8)) {
        TRACE(1,"H261\tPayload of grabbed frame too small for full frame");
        return 0;
      }

      // if the incoming data has changed size, tell the encoder
      if (frameWidth != header->width || frameHeight != header->height) {
        frameWidth = header->width;
        frameHeight = header->height;
        videoEncoder->SetSize(frameWidth, frameHeight);
      }

#if DEBUG_OUTPUT
static int encoderYUV = -1;
debug_write_data(encoderYUV, "encoder input", "encoder.yuv", OPAL_VIDEO_FRAME_DATA_PTR(header),
                                                             srcRTP.GetPayloadSize() - sizeof(PluginCodec_Video_FrameHeader));
#endif

      // "grab" the frame
      memcpy(videoEncoder->GetFramePtr(), OPAL_VIDEO_FRAME_DATA_PTR(header), frameWidth*frameHeight*12/8);

      // force I-frame, if requested
      if (forceIFrame || (flags & PluginCodec_CoderForceIFrame) != 0) {
        videoEncoder->FastUpdatePicture();
        forceIFrame = false;
      }

      // preprocess the data
      videoEncoder->PreProcessOneFrame();

      // get next frame from list created by preprocessor
      if (!videoEncoder->MoreToIncEncode())
        dstLen = 0;
      else {
        unsigned payloadLength = 0;
        videoEncoder->IncEncodeAndGetPacket((u_char *)dstRTP.GetPayloadPtr(), payloadLength); 
        dstLen = SetEncodedPacket(dstRTP, !videoEncoder->MoreToIncEncode(), RTP_RFC2032_PAYLOAD, lastTimeStamp, payloadLength, flags);

#if DEBUG_OUTPUT
debug_write_data(encoderOutput, "encoder output", "encoder.output", dstRTP.GetPacketPtr(), dstLen);
#endif

      }

      return 1;
    }

  protected:
    unsigned SetEncodedPacket(RTPFrame & dstRTP, bool isLast, unsigned char payloadCode, unsigned long lastTimeStamp, unsigned payloadLength, unsigned & flags);
    void adaptiveDelay(unsigned totalLength);
};



unsigned H261EncoderContext::SetEncodedPacket(RTPFrame & dstRTP, bool isLast, unsigned char payloadCode, unsigned long lastTimeStamp, unsigned payloadLength, unsigned & flags)
{
  dstRTP.SetPayloadSize(payloadLength);
  dstRTP.SetMarker(isLast);
  dstRTP.SetPayloadType(payloadCode);
  dstRTP.SetTimestamp(lastTimeStamp);

  flags = 0;
  flags |= isLast ? PluginCodec_ReturnCoderLastFrame : 0;  // marker bit on last frame of video
  flags |= PluginCodec_ReturnCoderIFrame;                       // sadly, this encoder *always* returns I-frames :(

  frameLength += dstRTP.GetPacketLen();

  if (isLast) {
    if (packetDelay) adaptiveDelay(frameLength);
    frameLength = 0;
  }

  return dstRTP.GetPacketLen();
}

void H261EncoderContext::adaptiveDelay(unsigned totalLength) {
  #ifdef _WIN32
    long waitBeforeSending =  0; 
    if (newTime!= 0) { // calculate delay and wait
      waitBeforeSending = newTime - GetTickCount();
      if (waitBeforeSending > 0)
        Sleep(waitBeforeSending);
    }
    if (waitFactor) {
      newTime = GetTickCount() +  8000 / waitFactor  * totalLength;
    }
    else {
      newTime = 0;
    }
  #else
    struct timeval currentTime;
    long waitBeforeSending;
    long waitAtNextFrame;
  
    if ((newTime.tv_sec != 0)  || (newTime.tv_usec != 0) ) { // calculate delay and wait
      gettimeofday(&currentTime, NULL); 
      waitBeforeSending = ((newTime.tv_sec - currentTime.tv_sec) * 1000000) + ((newTime.tv_usec - currentTime.tv_usec));  // in useconds
      if (waitBeforeSending > 0) 
        usleep(waitBeforeSending);
    }
    gettimeofday(&currentTime, NULL); 
    if (waitFactor) {
      waitAtNextFrame = waitFactor * totalLength ; // in us in ms
      newTime.tv_sec = currentTime.tv_sec + (int)((waitAtNextFrame  + currentTime.tv_usec) / 1000000);
      newTime.tv_usec = (int)((waitAtNextFrame + currentTime.tv_usec) % 1000000);
    }
    else {
      newTime.tv_sec = 0;
      newTime.tv_usec = 0;
    }
  #endif
}

static void * create_encoder(const struct PluginCodec_Definition * /*codec*/)
{
  return new H261EncoderContext;
}

static int encoder_set_options(const PluginCodec_Definition *, 
                               void * _context,
                               const char * , 
                               void * parm, 
                               unsigned * parmLen)
{
  H261EncoderContext * context = (H261EncoderContext *)_context;
  if (parmLen == NULL || *parmLen != sizeof(const char **))
    return 0;
  int width=0, height=0;
  if (parm != NULL) {
    const char ** options = (const char **)parm;
    int i;
    for (i = 0; options[i] != NULL; i += 2) {
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
         context->SetTargetBitRate(atoi(options[i+1]));
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_MIN_RX_FRAME_HEIGHT) == 0)
        height = atoi(options[i+1]);
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_WIDTH) == 0)
        width = atoi(options[i+1]);
      if (STRCMPI(options[i], "Adaptive Packet Delay") == 0)
        context->packetDelay = atoi(options[i+1]);
      printf ("%s = %s - %d\n", options[i], options[i+1], atoi(options[i+1]));
    }
  }
  context->SetFrameSize (width, height);

  return 1;
}

static void destroy_encoder(const struct PluginCodec_Definition * /*codec*/, void * _context)
{
  H261EncoderContext * context = (H261EncoderContext *)_context;
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
  H261EncoderContext * context = (H261EncoderContext *)_context;
  return context->EncodeFrames((const u_char *)from, *fromLen, (u_char *)to, *toLen, *flag);
}

static int encoder_get_output_data_size(const PluginCodec_Definition *, void *, const char *, void *, unsigned *)
{
  return RTP_MTU + RTP_MIN_HEADER_SIZE;
}

/////////////////////////////////////////////////////////////////////////////

class H261DecoderContext
{
  public:
    u_char * rvts;
    P64Decoder * videoDecoder;
    u_short expectedSequenceNumber;
    int ndblk, nblk;
    int now;
    bool packetReceived;
    unsigned frameWidth;
    unsigned frameHeight;

    CriticalSection mutex;

  public:
    H261DecoderContext()
    {
      rvts = NULL;

      videoDecoder = new FullP64Decoder();
      videoDecoder->marks(rvts);

      expectedSequenceNumber = 0;
      nblk = ndblk = 0;
      now = 1;
      packetReceived = false;
      frameWidth = frameHeight = 0;

      // Create the actual decoder
    }

    ~H261DecoderContext()
    {
      if (rvts)
        delete [] rvts;
      delete videoDecoder;
    }

    int DecodeFrames(const u_char * src, unsigned & srcLen, u_char * dst, unsigned & dstLen, unsigned int & flags)
    {
      WaitAndSignal m(mutex);

      // create RTP frame from source buffer
      RTPFrame srcRTP(src, srcLen);

      // create RTP frame from destination buffer
      RTPFrame dstRTP(dst, dstLen, 0);
      dstLen = 0;
      flags = 0;

      // Check for lost packets to help decoder
      bool lostPreviousPacket = false;
      if ((expectedSequenceNumber == 0) || (expectedSequenceNumber != srcRTP.GetSequenceNumber())) {
        lostPreviousPacket = true;
        TRACE(3,"H261\tDetected loss of one video packet. "
    	      << expectedSequenceNumber << " != "
              << srcRTP.GetSequenceNumber() << " Will recover.");
      }
      expectedSequenceNumber = (u_short)(srcRTP.GetSequenceNumber()+1);

#if DEBUG_OUTPUT
static int decoderInput = -1;
debug_write_data(decoderInput, "decoder input", "decoder.input", srcRTP.GetPacketPtr(), srcRTP.GetPacketLen());
#endif

      videoDecoder->mark(now);
      if (!videoDecoder->decode(srcRTP.GetPayloadPtr(), srcRTP.GetPayloadSize(), lostPreviousPacket)) {
        flags = PluginCodec_ReturnCoderRequestIFrame;
        return 1;
      }

      //Check for a resize - can change at any time!
      if (frameWidth  != (unsigned)videoDecoder->width() ||
          frameHeight != (unsigned)videoDecoder->height()) {

        frameWidth = videoDecoder->width();
        frameHeight = videoDecoder->height();

        nblk = (frameWidth * frameHeight) / 64;
        delete [] rvts;
        rvts = new u_char[nblk];
        memset(rvts, 0, nblk);
        videoDecoder->marks(rvts);
      }

      // Have not built an entire frame yet
      if (!srcRTP.GetMarker())
        return 1;

      videoDecoder->sync();
      ndblk = videoDecoder->ndblk();

      int wraptime = now ^ 0x80;
      u_char * ts = rvts;
      int k;
      for (k = nblk; --k >= 0; ++ts) {
        if (*ts == wraptime)
          *ts = (u_char)now;
      }

      now = (now + 1) & 0xff;

      int frameBytes = (frameWidth * frameHeight * 12) / 8;
      dstRTP.SetPayloadSize(sizeof(PluginCodec_Video_FrameHeader) + frameBytes);
      dstRTP.SetPayloadType(RTP_DYNAMIC_PAYLOAD);
      dstRTP.SetMarker(true);

      PluginCodec_Video_FrameHeader * frameHeader = (PluginCodec_Video_FrameHeader *)dstRTP.GetPayloadPtr();
      frameHeader->x = frameHeader->y = 0;
      frameHeader->width = frameWidth;
      frameHeader->height = frameHeight;
      memcpy(OPAL_VIDEO_FRAME_DATA_PTR(frameHeader), videoDecoder->GetFramePtr(), frameBytes);

      videoDecoder->resetndblk();

      dstLen = dstRTP.GetPacketLen();

#if DEBUG_OUTPUT
static int decoderOutput = -1;
debug_write_data(decoderOutput, "decoder output", "decoder.yuv", OPAL_VIDEO_FRAME_DATA_PTR(frameHeader),
                                                                 dstRTP.GetPayloadSize() - sizeof(PluginCodec_Video_FrameHeader));
#endif
      flags = PluginCodec_ReturnCoderLastFrame | PluginCodec_ReturnCoderIFrame;   // TODO: THIS NEEDS TO BE CHANGED TO DO CORRECT I-FRAME DETECTION
      return 1;
    }
};


/////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_information licenseInfo = {
  1143692893,                                                   // timestamp = Thu 30 Mar 2006 04:28:13 AM UTC

  "Craig Southeren, Post Increment",                            // source code author
  "1.0",                                                        // source code version
  "craigs@postincrement.com",                                   // source code email
  "http://www.postincrement.com",                               // source code URL
  "Copyright (C) 2004 by Post Increment, All Rights Reserved",  // source code copyright
  "MPL 1.0",                                                    // source code license
  PluginCodec_License_MPL,                                      // source code license
  
  "VIC H.261",                                                   // codec description
  "",                                                            // codec author
  "",                                                            // codec version
  "",                                                            // codec email
  "",                                                            // codec URL
  "Copyright (c) 1994 Regents of the University of California",  // codec copyright information
  NULL,                                                          // codec license
  PluginCodec_License_BSD                                        // codec license code
};

static const char YUV420PDesc[]  = { "YUV420P" };

static const char h261QCIFDesc[]  = { "H.261-QCIF" };
static const char h261CIFDesc[]   = { "H.261-CIF" };
static const char h261Desc[]      = { "H.261" };

static const char sdpH261[]   = { "h261" };

static const char QCIF_MPI[]  = "QCIF MPI";
static const char  CIF_MPI[]  =  "CIF MPI";


static void * create_decoder(const struct PluginCodec_Definition *)
{
  return new H261DecoderContext;
}


static int decoder_set_options(
      const struct PluginCodec_Definition *, 
      void * _context, 
      const char *, 
      void * parm, 
      unsigned * parmLen)
{
  H261DecoderContext * context = (H261DecoderContext *)_context;

  if (parmLen == NULL || *parmLen != sizeof(const char **) || parm == NULL)
    return 0;

  // get the "frame width" media format parameter to use as a hint for the encoder to start off
  for (const char * const * option = (const char * const *)parm; *option != NULL; option += 2) {
    if (STRCMPI(option[0], "Frame Width") == 0) {
      context->videoDecoder->fmt_ = (atoi(option[1]) == QCIF_WIDTH) ? IT_QCIF : IT_CIF;
      context->videoDecoder->init();
    }
  }

  return 1;
}


static void destroy_decoder(const struct PluginCodec_Definition * /*codec*/, void * _context)
{
  H261DecoderContext * context = (H261DecoderContext *)_context;
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
  H261DecoderContext * context = (H261DecoderContext *)_context;
  return context->DecodeFrames((const u_char *)from, *fromLen, (u_char *)to, *toLen, *flag);
}


static int decoder_get_output_data_size(const PluginCodec_Definition * codec, void *, const char *, void *, unsigned *)
{
  // this is really frame height * frame width;
  return sizeof(PluginCodec_Video_FrameHeader) + ((codec->parm.video.maxFrameWidth * codec->parm.video.maxFrameHeight * 3) / 2);
}


static int get_codec_options(const struct PluginCodec_Definition * codec,
                                 void *, 
                                 const char *,
                                 void * parm,
                                 unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(struct PluginCodec_Option **))
    return 0;

  *(const void **)parm = codec->userData;
  *parmLen = 0;
  return 1;
}


static char * num2str(int num)
{
  char buf[20];
  sprintf(buf, "%i", num);
  return strdup(buf);
}


static int ClampSize(int pixels, int maximum)
{
  // Set to nearest of the two possible sizes
  return pixels < maximum ? maximum/2 : maximum;
}


/* Convert the custom options for the codec to normalised options.
   For H.261 the custom options are "QCIF MPI" and "CIF MPI" which will
   restrict the min/max width/height and maximum frame rate.
 */
static int to_normalised_options(const struct PluginCodec_Definition *, void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  int qcif_mpi = 5;
  int cif_mpi = 5;
  int frameWidth = 352;
  int frameHeight = 288;
  for (const char * const * option = *(const char * const * *)parm; *option != NULL; option += 2) {
    if (STRCMPI(option[0], QCIF_MPI) == 0)
      qcif_mpi = atoi(option[1]);
    else if (STRCMPI(option[0], CIF_MPI) == 0)
      cif_mpi = atoi(option[1]);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_WIDTH) == 0)
      frameWidth = ClampSize(atoi(option[1]), 352);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_HEIGHT) == 0)
      frameHeight = ClampSize(atoi(option[1]), 288);
  }

  int minWidth = 176;
  int minHeight = 144;
  int maxWidth = 352;
  int maxHeight = 288;
  int frameTime = 3003;
  if (qcif_mpi != 5 && cif_mpi != 5)
    frameTime = 3003*(qcif_mpi > cif_mpi ? qcif_mpi : cif_mpi);
  else if (qcif_mpi != 5) {
    maxWidth = 176;
    maxHeight = 144;
    frameTime = 3003*qcif_mpi;
  }
  else if (cif_mpi != 5) {
    minWidth = 352;
    minHeight = 288;
    frameTime = 3003*cif_mpi;
  }
  else
    return 0; // Illegal!

  char ** options = (char **)calloc(15, sizeof(char *));
  *(char ***)parm = options;
  if (options == NULL)
    return 0;

  options[ 0] = strdup(PLUGINCODEC_OPTION_FRAME_WIDTH);
  options[ 1] = num2str(frameWidth);
  options[ 2] = strdup(PLUGINCODEC_OPTION_FRAME_HEIGHT);
  options[ 3] = num2str(frameHeight);
  options[ 4] = strdup(PLUGINCODEC_OPTION_MIN_RX_FRAME_WIDTH);
  options[ 5] = num2str(minWidth);
  options[ 6] = strdup(PLUGINCODEC_OPTION_MIN_RX_FRAME_HEIGHT);
  options[ 7] = num2str(minHeight);
  options[ 8] = strdup(PLUGINCODEC_OPTION_MAX_RX_FRAME_WIDTH);
  options[ 9] = num2str(maxWidth);
  options[10] = strdup(PLUGINCODEC_OPTION_MAX_RX_FRAME_HEIGHT);
  options[11] = num2str(maxHeight);
  options[12] = strdup(PLUGINCODEC_OPTION_FRAME_TIME);
  options[13] = num2str(frameTime);

  return 1;
}


/* Convert the normalised options to the codec custom options.
   For H.261 the custom options are "QCIF MPI" and "CIF MPI" which are
   set according to the min/max width/height and frame time.
 */
static int to_customised_options(const struct PluginCodec_Definition *, void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  int frameWidth = 352;
  int frameHeight = 288;
  int minWidth = 176;
  int minHeight = 144;
  int maxWidth = 352;
  int maxHeight = 288;
  int mpi = 1;
  for (const char * const * option = *(const char * const * *)parm; *option != NULL; option += 2) {
    if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_WIDTH) == 0)
      frameWidth = ClampSize(atoi(option[1]), 352);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_HEIGHT) == 0)
      frameHeight = ClampSize(atoi(option[1]), 288);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_MIN_RX_FRAME_WIDTH) == 0)
      minWidth = ClampSize(atoi(option[1]), 352);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_MIN_RX_FRAME_HEIGHT) == 0)
      minHeight = ClampSize(atoi(option[1]), 288);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_MAX_RX_FRAME_WIDTH) == 0)
      maxWidth = ClampSize(atoi(option[1]), 352);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_MAX_RX_FRAME_HEIGHT) == 0)
      maxHeight = ClampSize(atoi(option[1]), 288);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_TIME) == 0)
      mpi = atoi(option[1])/3003;
  }

  int qcif_mpi = 1;
  int cif_mpi = 1;
  if (minWidth > 176 || minHeight > 144)
    qcif_mpi = 5;
  if (maxWidth < 352 || maxHeight < 288)
    cif_mpi = 5;

  if (mpi < 1)
    mpi = 1;
  else if (mpi > 4)
    mpi = 4;
  if (qcif_mpi < 5 && mpi > qcif_mpi)
    qcif_mpi = mpi;
  if (cif_mpi < 5 && mpi > cif_mpi)
    cif_mpi = mpi;

  char ** options = (char **)calloc(17, sizeof(char *));
  *(char ***)parm = options;
  if (options == NULL)
    return 0;

  options[ 0] = strdup(PLUGINCODEC_OPTION_FRAME_WIDTH);
  options[ 1] = num2str(frameWidth);
  options[ 2] = strdup(PLUGINCODEC_OPTION_FRAME_HEIGHT);
  options[ 3] = num2str(frameHeight);
  options[ 4] = strdup(PLUGINCODEC_OPTION_MIN_RX_FRAME_WIDTH);
  options[ 5] = num2str(minWidth);
  options[ 6] = strdup(PLUGINCODEC_OPTION_MIN_RX_FRAME_HEIGHT);
  options[ 7] = num2str(minHeight);
  options[ 8] = strdup(PLUGINCODEC_OPTION_MAX_RX_FRAME_WIDTH);
  options[ 9] = num2str(maxWidth);
  options[10] = strdup(PLUGINCODEC_OPTION_MAX_RX_FRAME_HEIGHT);
  options[11] = num2str(maxHeight);
  options[12] = strdup(QCIF_MPI);
  options[13] = num2str(qcif_mpi);
  options[14] = strdup(CIF_MPI);
  options[15] = num2str(cif_mpi);

  return 1;
}
  

static int free_codec_options(const struct PluginCodec_Definition *, void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  char ** strings = (char **) parm;
  for (char ** string = strings; *string != NULL; string++)
    free(*string);
  free(strings);
  return 1;
}


static PluginCodec_ControlDefn EncoderControls[] = {
  { PLUGINCODEC_CONTROL_GET_CODEC_OPTIONS,     get_codec_options },
  { PLUGINCODEC_CONTROL_TO_NORMALISED_OPTIONS, to_normalised_options },
  { PLUGINCODEC_CONTROL_TO_CUSTOMISED_OPTIONS, to_customised_options },
  { PLUGINCODEC_CONTROL_FREE_CODEC_OPTIONS,    free_codec_options },
  { PLUGINCODEC_CONTROL_SET_CODEC_OPTIONS,     encoder_set_options },
  { PLUGINCODEC_CONTROL_GET_OUTPUT_DATA_SIZE,  encoder_get_output_data_size },
  { NULL }
};

static PluginCodec_ControlDefn DecoderControls[] = {
  { PLUGINCODEC_CONTROL_GET_CODEC_OPTIONS,     get_codec_options },
  { PLUGINCODEC_CONTROL_TO_NORMALISED_OPTIONS, to_normalised_options },
  { PLUGINCODEC_CONTROL_TO_CUSTOMISED_OPTIONS, to_customised_options },
  { PLUGINCODEC_CONTROL_FREE_CODEC_OPTIONS,    free_codec_options },
  { PLUGINCODEC_CONTROL_SET_CODEC_OPTIONS,     decoder_set_options },
  { PLUGINCODEC_CONTROL_GET_OUTPUT_DATA_SIZE,  decoder_get_output_data_size },
  { NULL }
};

static struct PluginCodec_Option const minRxFrameWidth =
  { PluginCodec_IntegerOption, PLUGINCODEC_OPTION_MIN_RX_FRAME_WIDTH,  true, PluginCodec_NoMerge, "176", NULL, NULL, 0, "176", "352" };
static struct PluginCodec_Option const minRxFrameHeight =
  { PluginCodec_IntegerOption, PLUGINCODEC_OPTION_MIN_RX_FRAME_HEIGHT, true, PluginCodec_NoMerge, "144", NULL, NULL, 0, "144", "288"  };
static struct PluginCodec_Option const maxRxFrameWidth =
  { PluginCodec_IntegerOption, PLUGINCODEC_OPTION_MAX_RX_FRAME_WIDTH,  true, PluginCodec_NoMerge, "352", NULL, NULL, 0, "176", "352"  };
static struct PluginCodec_Option const maxRxFrameHeight =
  { PluginCodec_IntegerOption, PLUGINCODEC_OPTION_MAX_RX_FRAME_HEIGHT, true, PluginCodec_NoMerge, "288", NULL, NULL, 0, "144", "288"  };

static struct PluginCodec_Option const minRxFrameWidthQCIF =
  { PluginCodec_IntegerOption, PLUGINCODEC_OPTION_MIN_RX_FRAME_WIDTH,  true, PluginCodec_NoMerge, "176", NULL, NULL, 0, "176", "176" };
static struct PluginCodec_Option const minRxFrameHeightQCIF =
  { PluginCodec_IntegerOption, PLUGINCODEC_OPTION_MIN_RX_FRAME_HEIGHT, true, PluginCodec_NoMerge, "144", NULL, NULL, 0, "144", "144"  };
static struct PluginCodec_Option const maxRxFrameWidthQCIF =
  { PluginCodec_IntegerOption, PLUGINCODEC_OPTION_MAX_RX_FRAME_WIDTH,  true, PluginCodec_NoMerge, "176", NULL, NULL, 0, "176", "176"  };
static struct PluginCodec_Option const maxRxFrameHeightQCIF =
  { PluginCodec_IntegerOption, PLUGINCODEC_OPTION_MAX_RX_FRAME_HEIGHT, true, PluginCodec_NoMerge, "144", NULL, NULL, 0, "144", "144"  };

static struct PluginCodec_Option const minRxFrameWidthCIF =
  { PluginCodec_IntegerOption, PLUGINCODEC_OPTION_MIN_RX_FRAME_WIDTH,  true, PluginCodec_NoMerge, "352", NULL, NULL, 0, "352", "352" };
static struct PluginCodec_Option const minRxFrameHeightCIF =
  { PluginCodec_IntegerOption, PLUGINCODEC_OPTION_MIN_RX_FRAME_HEIGHT, true, PluginCodec_NoMerge, "288", NULL, NULL, 0, "288", "288"  };
static struct PluginCodec_Option const maxRxFrameWidthCIF =
  { PluginCodec_IntegerOption, PLUGINCODEC_OPTION_MAX_RX_FRAME_WIDTH,  true, PluginCodec_NoMerge, "352", NULL, NULL, 0, "352", "352"  };
static struct PluginCodec_Option const maxRxFrameHeightCIF =
  { PluginCodec_IntegerOption, PLUGINCODEC_OPTION_MAX_RX_FRAME_HEIGHT, true, PluginCodec_NoMerge, "288", NULL, NULL, 0, "288", "288"  };

static struct PluginCodec_Option const qcifMPI =
  { PluginCodec_IntegerOption, QCIF_MPI, false, PluginCodec_MaxMerge, "1", "QCIF", "5", 0, "1", "5" }; // 5 is disabled

static struct PluginCodec_Option const cifMPI =
  { PluginCodec_IntegerOption,  CIF_MPI, false, PluginCodec_MaxMerge, "1", "CIF",  "5", 0, "1", "5" }; // 5 is disabled

/* The annex below is turned off and set to read/only because this
   implementation does not support them. It's presence here is so that if
   someone out there does a different implementation of the codec and copies
   this file as a template, they will get them and hopefully notice that they
   can just make it read/write and/or turned on.
 */
static struct PluginCodec_Option const annexD =
  { PluginCodec_BoolOption,    "Annex D",  true,  PluginCodec_AndMerge, "0", "D", "0" };

static struct PluginCodec_Option const * const qcifOptionTable[] = {
  &qcifMPI,
  &annexD,
  &minRxFrameWidthQCIF,
  &minRxFrameHeightQCIF,
  &maxRxFrameWidthQCIF,
  &maxRxFrameHeightQCIF,
  NULL
};

static struct PluginCodec_Option const * const cifOptionTable[] = {
  &cifMPI,
  &annexD,
  &minRxFrameWidthCIF,
  &minRxFrameHeightCIF,
  &maxRxFrameWidthCIF,
  &maxRxFrameHeightCIF,
  NULL
};

static struct PluginCodec_Option const * const xcifOptionTable[] = {
  &qcifMPI,
  &cifMPI,
  &annexD,
  &minRxFrameWidth,
  &minRxFrameHeight,
  &maxRxFrameWidth,
  &maxRxFrameHeight,
  NULL
};


static struct PluginCodec_Definition h261CodecDefn[] = {

{ 
  // CIF only encoder
  PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
  &licenseInfo,                       // license information

  PluginCodec_MediaTypeVideo |        // audio codec
  PluginCodec_RTPTypeExplicit,        // specified RTP type

  h261CIFDesc,                        // text decription
  YUV420PDesc,                        // source format
  h261CIFDesc,                        // destination format

  cifOptionTable,                     // user data 

  H261_CLOCKRATE,                     // samples per second
  H261_BITRATE,                       // raw bits per second
  20000,                              // nanoseconds per frame

  CIF_WIDTH,                          // frame width
  CIF_HEIGHT,                         // frame height
  10,                                 // recommended frame rate
  60,                                 // maximum frame rate
  RTP_RFC2032_PAYLOAD,                // IANA RTP payload code
  sdpH261,                            // RTP payload name

  create_encoder,                     // create codec function
  destroy_encoder,                    // destroy codec
  codec_encoder,                      // encode/decode
  EncoderControls,                    // codec controls

  PluginCodec_H323VideoCodec_h261,    // h323CapabilityType 
  NULL                                // h323CapabilityData
},
{ 
  // CIF only decoder
  PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
  &licenseInfo,                       // license information

  PluginCodec_MediaTypeVideo |        // audio codec
  PluginCodec_RTPTypeExplicit,        // specified RTP type

  h261CIFDesc,                        // text decription
  h261CIFDesc,                        // source format
  YUV420PDesc,                        // destination format

  cifOptionTable,                     // user data 

  H261_CLOCKRATE,                     // samples per second
  H261_BITRATE,                       // raw bits per second
  20000,                              // nanoseconds per frame

  CIF_WIDTH,                          // frame width
  CIF_HEIGHT,                         // frame height
  10,                                 // recommended frame rate
  60,                                 // maximum frame rate
  RTP_RFC2032_PAYLOAD,                // IANA RTP payload code
  sdpH261,                            // RTP payload name

  create_decoder,                     // create codec function
  destroy_decoder,                    // destroy codec
  codec_decoder,                      // encode/decode
  DecoderControls,                    // codec controls

  PluginCodec_H323VideoCodec_h261,    // h323CapabilityType 
  NULL                                // h323CapabilityData
},

{ 
  // QCIF only encoder
  PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
  &licenseInfo,                       // license information

  PluginCodec_MediaTypeVideo |        // audio codec
  PluginCodec_RTPTypeExplicit,        // specified RTP type

  h261QCIFDesc,                       // text decription
  YUV420PDesc,                        // source format
  h261QCIFDesc,                       // destination format

  qcifOptionTable,                    // user data 

  H261_CLOCKRATE,                     // samples per second
  H261_BITRATE,                       // raw bits per second
  20000,                              // nanoseconds per frame

  QCIF_WIDTH,                         // frame width
  QCIF_HEIGHT,                        // frame height
  10,                                 // recommended frame rate
  60,                                 // maximum frame rate
  RTP_RFC2032_PAYLOAD,                // IANA RTP payload code
  sdpH261,                            // RTP payload name

  create_encoder,                     // create codec function
  destroy_encoder,                    // destroy codec
  codec_encoder,                      // encode/decode
  EncoderControls,                    // codec controls

  PluginCodec_H323VideoCodec_h261,    // h323CapabilityType 
  NULL                                // h323CapabilityData
},
{ 
  // QCIF only decoder
  PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
  &licenseInfo,                       // license information

  PluginCodec_MediaTypeVideo |        // audio codec
  PluginCodec_RTPTypeExplicit,        // specified RTP type

  h261QCIFDesc,                       // text decription
  h261QCIFDesc,                       // source format
  YUV420PDesc,                        // destination format

  qcifOptionTable,                    // user data 

  H261_CLOCKRATE,                     // samples per second
  H261_BITRATE,                       // raw bits per second
  20000,                              // nanoseconds per frame

  QCIF_WIDTH,                         // frame width
  QCIF_HEIGHT,                        // frame height
  10,                                 // recommended frame rate
  60,                                 // maximum frame rate
  RTP_RFC2032_PAYLOAD,                // IANA RTP payload code
  sdpH261,                            // RTP payload name

  create_decoder,                     // create codec function
  destroy_decoder,                    // destroy codec
  codec_decoder,                      // encode/decode
  DecoderControls,                    // codec controls

  PluginCodec_H323VideoCodec_h261,    // h323CapabilityType 
  NULL                                // h323CapabilityData
},

{ 
  // Both QCIF and CIF (dynamic) encoder
  PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
  &licenseInfo,                       // license information

  PluginCodec_MediaTypeVideo |        // audio codec
  PluginCodec_RTPTypeExplicit,        // specified RTP type

  h261Desc,                           // text decription
  YUV420PDesc,                        // source format
  h261Desc,                           // destination format

  xcifOptionTable,                    // user data 

  H261_CLOCKRATE,                     // samples per second
  H261_BITRATE,                       // raw bits per second
  20000,                              // nanoseconds per frame

  CIF_WIDTH,                          // frame width
  CIF_HEIGHT,                         // frame height
  10,                                 // recommended frame rate
  60,                                 // maximum frame rate
  RTP_RFC2032_PAYLOAD,                // IANA RTP payload code
  sdpH261,                            // RTP payload name

  create_encoder,                     // create codec function
  destroy_encoder,                    // destroy codec
  codec_encoder,                      // encode/decode
  EncoderControls,                    // codec controls

  PluginCodec_H323VideoCodec_h261,    // h323CapabilityType 
  NULL                                // h323CapabilityData
},
{ 
  // Both QCIF and CIF (dynamic) decoder
  PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
  &licenseInfo,                       // license information

  PluginCodec_MediaTypeVideo |        // audio codec
  PluginCodec_RTPTypeExplicit,        // specified RTP type

  h261Desc,                           // text decription
  h261Desc,                           // source format
  YUV420PDesc,                        // destination format

  xcifOptionTable,                    // user data 

  H261_CLOCKRATE,                     // samples per second
  H261_BITRATE,                       // raw bits per second
  20000,                              // nanoseconds per frame

  CIF_WIDTH,                          // frame width
  CIF_HEIGHT,                         // frame height
  10,                                 // recommended frame rate
  60,                                 // maximum frame rate
  RTP_RFC2032_PAYLOAD,                // IANA RTP payload code
  sdpH261,                            // RTP payload name

  create_decoder,                     // create codec function
  destroy_decoder,                    // destroy codec
  codec_decoder,                      // encode/decode
  DecoderControls,                    // codec controls

  PluginCodec_H323VideoCodec_h261,    // h323CapabilityType 
  NULL                                // h323CapabilityData
},

};

extern "C" {
  PLUGIN_CODEC_IMPLEMENT(VIC_H261)

  PLUGIN_CODEC_DLL_API struct PluginCodec_Definition * PLUGIN_CODEC_GET_CODEC_FN(unsigned * count, unsigned /*version*/)
  {
    char * debug_level = getenv ("PWLIB_TRACE_CODECS");
    if (debug_level!=NULL) {
      Trace::SetLevel(atoi(debug_level));
    }
    else {
      Trace::SetLevel(0);
    }

    *count = sizeof(h261CodecDefn) / sizeof(struct PluginCodec_Definition);
    return h261CodecDefn;
  }

};

/////////////////////////////////////////////////////////////////////////////
