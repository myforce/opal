/*
 * rfc4175.h
 *
 * RFC4175 transport for uncompressed video
 *
 * Open Phone Abstraction Library
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
 * Contributor(s): ______________________________________.
 *
 * $Log: rfc4175.h,v $
 * Revision 1.5  2007/09/11 13:41:19  csoutheren
 * Fully implemented RFC 4175 codec with YCrCb420 encoding
 *
 * Revision 1.4  2007/08/29 00:45:57  csoutheren
 * Change base class for RFC4175 transcoder
 *
 * Revision 1.3  2007/06/30 14:00:05  dsandras
 * Fixed previous commit so that things at least compile. Untested.
 *
 * Revision 1.2  2007/06/29 23:24:19  csoutheren
 * More RFC4175 implementation
 *
 * Revision 1.1  2007/05/31 14:11:16  csoutheren
 * Add initial support for RFC 4175 uncompressed video encoding
 *
 */

#ifndef __OPAL_RFC4175_H
#define __OPAL_RFC4175_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include <ptlib.h>

#include <ptclib/random.h>

#include <opal/transcoders.h>
#include <codec/opalplugin.h>
#include <codec/vidcodec.h>

namespace PWLibStupidLinkerHacks {
  extern int rfc4175Loader;
};

#define OPAL_RFC4175_YCbCr420  "RFC4175_YCbCr-4:2:0"
extern const OpalVideoFormat & GetOpalRFC4175_YCbCr420();
#define OpalRFC4175YCbCr420    GetOpalRFC4175_YCbCr420()

/////////////////////////////////////////////////////////////////////////////

class OpalRFC4175Transcoder : public OpalUncompVideoTranscoder
{
  PCLASSINFO(OpalRFC4175Transcoder, OpalUncompVideoTranscoder);
  public:
    OpalRFC4175Transcoder(      
      const OpalMediaFormat & inputMediaFormat,  ///<  Input media format
      const OpalMediaFormat & outputMediaFormat  ///<  Output media format
    );
    virtual PINDEX PixelsToBytes(PINDEX pixels) const = 0;
    PINDEX RFC4175HeaderSize(PINDEX lines);

    struct ScanLineHeader {
      PUInt16b length;
      PUInt16b y;       // has field flag in top bit
      PUInt16b offset;  // has last line flag in top bit
    };
};

/////////////////////////////////////////////////////////////////////////////

class OpalRFC4175Encoder : public OpalRFC4175Transcoder
{
  PCLASSINFO(OpalRFC4175Encoder, OpalRFC4175Transcoder);
  public:
    OpalRFC4175Encoder(      
      const OpalMediaFormat & inputMediaFormat,  ///<  Input media format
      const OpalMediaFormat & outputMediaFormat  ///<  Output media format
    );

    BOOL ConvertFrames(const RTP_DataFrame & input, RTP_DataFrameList & output);

  protected:
    void EncodeFullFrame();
    void EncodeScanLineSegment(PINDEX y, PINDEX offs, PINDEX width);
    void AddNewDstFrame();
    void EncodeFrames();
    void FinishOutputFrame();

    DWORD extendedSequenceNumber;
    PINDEX maximumPacketSize;
    unsigned frameHeight;
    unsigned frameWidth;

    DWORD srcTimestamp;
    BYTE * srcYPlane;
    BYTE * srcCbPlane;
    BYTE * srcCrPlane;

    RTP_DataFrameList * dstFrames;
    std::vector<PINDEX> dstScanlineCounts;
    PINDEX dstScanLineCount;
    PINDEX dstPacketSize;
    ScanLineHeader * dstScanLineTable;
};

/////////////////////////////////////////////////////////////////////////////

class OpalRFC4175Decoder : public OpalRFC4175Transcoder
{
  PCLASSINFO(OpalRFC4175Decoder, OpalRFC4175Transcoder);
  public:
    OpalRFC4175Decoder(      
      const OpalMediaFormat & inputMediaFormat,  ///<  Input media format
      const OpalMediaFormat & outputMediaFormat  ///<  Output media format
    );
    ~OpalRFC4175Decoder();

    virtual PINDEX PixelsToBytes(PINDEX pixels) const = 0;
    virtual PINDEX BytesToPixels(PINDEX pixels) const = 0;
    BOOL ConvertFrames(const RTP_DataFrame & input, RTP_DataFrameList & output);

  protected:
    BOOL Initialise();
    BOOL DecodeStoredFrames(RTP_DataFrameList & output);

    RTP_DataFrameList inputFrames;
    std::vector<PINDEX> scanlineCounts;
    PINDEX frameWidth, frameHeight;

    BOOL  first;
    DWORD lastSequenceNumber;
    DWORD lastTimeStamp;
};

/////////////////////////////////////////////////////////////////////////////

/**This class defines a transcoder implementation class that converts RFC4175 to YUV420P
 */
class Opal_RFC4175YCbCr420_to_YUV420P : public OpalRFC4175Decoder
{
  PCLASSINFO(Opal_RFC4175YCbCr420_to_YUV420P, OpalRFC4175Decoder);
  public:
    Opal_RFC4175YCbCr420_to_YUV420P() : OpalRFC4175Decoder(OpalYUV420P, OpalRFC4175YCbCr420) { }
    PINDEX PixelsToBytes(PINDEX pixels) const                                     { return pixels*12/8; }
    PINDEX BytesToPixels(PINDEX bytes) const                                      { return bytes*8/12; }
};

class Opal_YUV420P_to_RFC4175YCbCr420 : public OpalRFC4175Encoder
{
  PCLASSINFO(Opal_YUV420P_to_RFC4175YCbCr420, OpalRFC4175Encoder);
  public:
    Opal_YUV420P_to_RFC4175YCbCr420() : OpalRFC4175Encoder(OpalRFC4175YCbCr420, OpalYUV420P) { }
    PINDEX PixelsToBytes(PINDEX pixels) const                                     { return pixels*12/8; }
    PINDEX BytesToPixels(PINDEX bytes) const                                      { return bytes*8/12; }
};

#define OPAL_REGISTER_RFC4175_VIDEO(oformat, rformat) \
  OPAL_REGISTER_TRANSCODER(Opal_RFC4175##rformat##_to_##oformat, OpalRFC4175##rformat, Opal##oformat); \
  OPAL_REGISTER_TRANSCODER(Opal_##oformat##_to_RFC4175##rformat, Opal##oformat, OpalRFC4175##rformat);

/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////

#if 0

#define OPAL_RFC4175_RGB24   "RFC4175_RGB24"
extern const OpalVideoFormat & GetOpalRFC4175_RGB24();
#define OpalRFC4175_RGB24   GetOpalRFC4175_RGB24()

/**This class defines a transcoder implementation class that converts RFC4175 to RGB24
 */
class Opal_RFC4175_RGB24 : public OpalRFC4175Decoder
{
  PCLASSINFO(Opal_RFC4175_RGB24, OpalRFC4175Decoder);
  public:
    Opal_RFC4175_RGB24() : OpalRFC4175Decoder(OpalRGB24, OpalRFC4175_RGB24) { }
    PINDEX PixelsToBytes(PINDEX pixels) const                               { return pixels*3; }
    PINDEX BytesToPixels(PINDEX bytes) const                                { return bytes/3; }
};

class Opal_RGB24_RFC4175 : public OpalRFC4175Encoder
{
  PCLASSINFO(Opal_RGB24_RFC4175, OpalRFC4175Encoder);
  public:
    Opal_RGB24_RFC4175() : OpalRFC4175Encoder(OpalRFC4175_RGB24, OpalRGB24) { }
    PINDEX PixelsToBytes(PINDEX pixels) const                               { return pixels*3; }
    PINDEX BytesToPixels(PINDEX bytes) const                                { return bytes/3; }
};

#endif

/////////////////////////////////////////////////////////////////////////////

#endif
