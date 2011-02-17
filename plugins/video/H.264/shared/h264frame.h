/*****************************************************************************/
/* The contents of this file are subject to the Mozilla Public License       */
/* Version 1.0 (the "License"); you may not use this file except in          */
/* compliance with the License.  You may obtain a copy of the License at     */
/* http://www.mozilla.org/MPL/                                               */
/*                                                                           */
/* Software distributed under the License is distributed on an "AS IS"       */
/* basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the  */
/* License for the specific language governing rights and limitations under  */
/* the License.                                                              */
/*                                                                           */
/* The Original Code is the Open H323 Library.                               */
/*                                                                           */
/* The Initial Developer of the Original Code is Matthias Schneider          */
/* Copyright (C) 2007 Matthias Schneider, All Rights Reserved.               */
/*                                                                           */
/* Contributor(s): Matthias Schneider (ma30002000@yahoo.de)                  */
/*                                                                           */
/* Alternatively, the contents of this file may be used under the terms of   */
/* the GNU General Public License Version 2 or later (the "GPL"), in which   */
/* case the provisions of the GPL are applicable instead of those above.  If */
/* you wish to allow use of your version of this file only under the terms   */
/* of the GPL and not to allow others to use your version of this file under */
/* the MPL, indicate your decision by deleting the provisions above and      */
/* replace them with the notice and other provisions required by the GPL.    */
/* If you do not delete the provisions above, a recipient may use your       */
/* version of this file under either the MPL or the GPL.                     */
/*                                                                           */
/* The Original Code was written by Matthias Schneider <ma30002000@yahoo.de> */
/*****************************************************************************/


#ifndef __H264FRAME_H__
#define __H264FRAME_H__ 1

// GVX 3000 does not like STAP packets... So we waste 40 bytes per connection...
//#define SEND_STAP_PACKETS 1


#ifdef _MSC_VER
#include "../../common/vs-stdint.h"
#include "../../common/rtpframe.h"
#else
#include <stdint.h>
#include "rtpframe.h"
#endif

#define H264_NAL_TYPE_NON_IDR_SLICE 1
#define H264_NAL_TYPE_DP_A_SLICE 2
#define H264_NAL_TYPE_DP_B_SLICE 3
#define H264_NAL_TYPE_DP_C_SLICE 0x4
#define H264_NAL_TYPE_IDR_SLICE 0x5
#define H264_NAL_TYPE_SEI 0x6
#define H264_NAL_TYPE_SEQ_PARAM 0x7
#define H264_NAL_TYPE_PIC_PARAM 0x8
#define H264_NAL_TYPE_ACCESS_UNIT 0x9
#define H264_NAL_TYPE_END_OF_SEQ 0xa
#define H264_NAL_TYPE_END_OF_STREAM 0xb
#define H264_NAL_TYPE_FILLER_DATA 0xc
#define H264_NAL_TYPE_SEQ_EXTENSION 0xd

const static struct h264_level {
    unsigned level_idc;
    unsigned mbps;        /* max macroblock processing rate (macroblocks/sec) */
    unsigned frame_size;  /* max frame size (macroblocks) */
    unsigned dpb;         /* max decoded picture buffer (bytes) */
    long unsigned bitrate;/* max bitrate (kbit/sec) */
    unsigned cpb;         /* max vbv buffer (kbit) */
    unsigned mv_range;    /* max vertical mv component range (pixels) */
    unsigned mvs_per_2mb; /* max mvs per 2 consecutive mbs. */
    unsigned slice_rate;
    unsigned bipred8x8;   /* limit bipred to >=8x8 */
    unsigned direct8x8;   /* limit b_direct to >=8x8 */
    unsigned frame_only;  /* forbid interlacing */
} h264_levels[] = {
    { 10,   1485,    99,   152064,     64000,    175,  64, 64,  0, 0, 0, 1 },
    {  9,   1485,    99,   152064,    128000,    350,  64, 64,  0, 0, 0, 1 },
    { 11,   3000,   396,   345600,    192000,    500, 128, 64,  0, 0, 0, 1 },
    { 12,   6000,   396,   912384,    384000,   1000, 128, 64,  0, 0, 0, 1 },
    { 13,  11880,   396,   912384,    768000,   2000, 128, 64,  0, 0, 0, 1 },
    { 20,  11880,   396,   912384,   2000000,   2000, 128, 64,  0, 0, 0, 1 },
    { 21,  19800,   792,  1824768,   4000000,   4000, 256, 64,  0, 0, 0, 0 },
    { 22,  20250,  1620,  3110400,   4000000,   4000, 256, 64,  0, 0, 0, 0 },
    { 30,  40500,  1620,  3110400,  10000000,  10000, 256, 32, 22, 0, 1, 0 },
    { 31, 108000,  3600,  6912000,  14000000,  14000, 512, 16, 60, 1, 1, 0 },
    { 32, 216000,  5120,  7864320,  20000000,  20000, 512, 16, 60, 1, 1, 0 },
    { 40, 245760,  8192, 12582912,  20000000,  25000, 512, 16, 60, 1, 1, 0 },
    { 41, 245760,  8192, 12582912,  50000000,  62500, 512, 16, 24, 1, 1, 0 },
    { 42, 522240,  8704, 13369344,  50000000,  62500, 512, 16, 24, 1, 1, 1 },
    { 50, 589824, 22080, 42393600, 135000000, 135000, 512, 16, 24, 1, 1, 1 },
    { 51, 983040, 36864, 70778880, 240000000, 240000, 512, 16, 24, 1, 1, 1 },
    { 0 }
};

// This table is used in order to select a different resolution if the desired one
// consists of more macroblocks than the level limit
const static struct h264_resolution {
    unsigned width;
    unsigned height;
    unsigned macroblocks;
} h264_resolutions[] = {
    { 1920, 1088, 8160 },
    { 1600, 1200, 7500 },
    { 1408, 1152, 6336 },
    { 1280, 1024, 5120 },
    { 1280,  720, 3600 },
    { 1024,  768, 3072 },
    {  800,  600, 1900 },
    {  704,  576, 1584 },
    {  640,  480, 1200 },
    {  352,  288,  396 },
    {  320,  240,  300 },
    {  176,  144,   99 },
    {  128,   96,   48 },
    { 0 }
};

#ifndef LICENCE_MPL
extern "C"
{
  #include "x264.h"
}
#endif

enum codecInFlags {
  silenceFrame      = 1,
  forceIFrame       = 2
};
    
enum codecOutFlags {
  isLastFrame     = 1,
  isIFrame        = 2,
  requestIFrame   = 4
};
	  

typedef struct h264_nal_t
{
  uint32_t offset;
  uint32_t length;
  uint8_t  type;
} h264_nal_t;

class H264Frame
{
public:
  H264Frame();
  ~H264Frame();

  void BeginNewFrame();
#ifndef LICENCE_MPL
  void SetFromFrame (x264_nal_t *NALs, int numberOfNALs);
#endif
  void SetMaxPayloadSize (uint16_t maxPayloadSize) 
  {
    m_maxPayloadSize = maxPayloadSize;
  }
  void SetTimestamp (uint32_t timestamp) 
  {
    m_timestamp = timestamp;
  }
  bool GetRTPFrame (RTPFrame & frame, unsigned int & flags);
  bool HasRTPFrames ()
  {
    return m_currentNAL < m_numberOfNALsInFrame;
  }

  bool SetFromRTPFrame (RTPFrame & frame, unsigned int & flags);
  uint8_t* GetFramePtr ()
  {
    return m_encodedFrame;
  }
  uint32_t GetFrameSize () {
    return m_encodedFrameLen;
  }
  bool IsSync ();
  
private:
  bool EncapsulateSTAP  (RTPFrame & frame, unsigned int & flags);
  bool EncapsulateFU    (RTPFrame & frame, unsigned int & flags);

  bool DeencapsulateFU   (RTPFrame & frame, unsigned int & flags);
  bool DeencapsulateSTAP (RTPFrame & frame, unsigned int & flags);
  void AddDataToEncodedFrame (uint8_t *data, uint32_t dataLen, uint8_t header, bool addHeader);
  bool IsStartCode (const uint8_t *positionInFrame);

    // general stuff
  uint32_t m_timestamp;
  uint16_t m_maxPayloadSize;
  uint8_t* m_encodedFrame;
  uint32_t m_encodedFrameLen;

  h264_nal_t* m_NALs;
  uint32_t m_numberOfNALsInFrame;
  uint32_t m_currentNAL; 
  uint32_t m_numberOfNALsReserved;
  
  // for encapsulation
  uint32_t m_currentNALFURemainingLen;
  uint8_t* m_currentNALFURemainingDataPtr;
  uint8_t  m_currentNALFUHeader0;
  uint8_t  m_currentNALFUHeader1;

  // for deencapsulation
  uint16_t m_currentFU;
};

#endif /* __H264FRAME_H__ */
