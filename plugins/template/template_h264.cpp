/*
 * Template H.264 Plugin codec for OPAL
 *
 * This the starting point for creating new H.264 plug in codecs for
 * OPAL in C++. The requirements of H.264 over H.323 and SIP are quite
 * complex. The mapping of width/height/frame rate/bit rate to the
 * H.264 profile/level with possible overrides (e.g. MaxMBS for maximum
 * macro blocks per second) which are not universally supported by all
 * endpoints make precise control of the video difficult and in some
 * cases impossible.
 *
 *
 * Copyright (C) 2010 Vox Lucida Pty Ltd, All Rights Reserved
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
 * Contributor(s): ______________________________________.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#include <codec/opalplugin.hpp>

#define MY_CODEC   MyH264                                   // Name of codec (use C variable characters)

static unsigned   MyVersion = PLUGIN_CODEC_VERSION_OPTIONS; // Minimum version for codec (should never change)
static const char MyDescription[] = "My H.264 Video Codec"; // Human readable description of codec
static const char FormatNameH323[] = "H.264";               // OpalMediaFormat name string to generate
static const char FormatNameSIP0[] = "H.264-0";             // OpalMediaFormat name string to generate
static const char FormatNameSIP1[] = "H.264-1";             // OpalMediaFormat name string to generate
static const char MyPayloadName[] = "h264";                 // RTP payload name (IANA approved)
static unsigned   MyClockRate = 90000;                      // RTP dictates 90000
static unsigned   MyMaxFrameRate = 60;                      // Maximum frame rate (per second)
static unsigned   MyMaxWidth = 2816;                        // Maximum width of frame
static unsigned   MyMaxHeight = 2304;                       // Maximum height of frame

static const char YUV420PFormatName[] = "YUV420P";          // Raw media format


static struct PluginCodec_information LicenseInfo = {
  1143692893,                                                   // timestamp = Thu 30 Mar 2006 04:28:13 AM UTC

  "Robert Jongbloed, Vox Lucida Pty.Ltd.",                      // source code author
  "1.0",                                                        // source code version
  "robertj@voxlucida.com.au",                                   // source code email
  "http://www.voxlucida.com.au",                                // source code URL
  "Copyright (C) 2010 by Vox Lucida Pt.Ltd., All Rights Reserved", // source code copyright
  "MPL 1.0",                                                    // source code license
  PluginCodec_License_MPL,                                      // source code license
  
  MyDescription,                                                // codec description
  "Professor Dumbledore",                                       // codec author
  "1.0",                                                        // codec version
  "headmaster@hogwarts.ac.uk",                                  // codec email
  "http://hogwarts.ac.uk",                                      // codec URL
  "Copyright, Ministry of Magic",                               // codec copyright information
  NULL,                                                         // codec license
  PluginCodec_License_BSD                                       // codec license code
};


///////////////////////////////////////////////////////////////////////////////

// Level 3 allows 4CIF at 25fps
#define DefaultProfileStr          H264_PROFILE_STR_BASELINE
#define DefaultProfileInt          66
#define DefaultProfileH241         64
#define DefaultLevelStr            H264_LEVEL_STR_3
#define DefaultLevelInt            30
#define DefaultLevelH241           64
#define DefaultSDPProfileAndLevel  "42801e"

#define DefaultSendAccessUnitDelimiters 0  // Many endpoints don't seem to like these, initially false

#define H264_H241_MAX_NALU_SIZE   1400   // From H.241/8.3.2.10

#define H264_PROFILE_STR_BASELINE  "Baseline"
#define H264_PROFILE_STR_MAIN      "Main"
#define H264_PROFILE_STR_EXTENDED  "Extended"

#define H264_LEVEL_STR_1    "1"
#define H264_LEVEL_STR_1_b  "1.b"
#define H264_LEVEL_STR_1_1  "1.1"
#define H264_LEVEL_STR_1_2  "1.2"
#define H264_LEVEL_STR_1_3  "1.3"
#define H264_LEVEL_STR_2    "2"
#define H264_LEVEL_STR_2_1  "2.1"
#define H264_LEVEL_STR_2_2  "2.2"
#define H264_LEVEL_STR_3    "3"
#define H264_LEVEL_STR_3_1  "3.1"
#define H264_LEVEL_STR_3_2  "3.2"
#define H264_LEVEL_STR_4    "4"
#define H264_LEVEL_STR_4_1  "4.1"
#define H264_LEVEL_STR_4_2  "4.2"
#define H264_LEVEL_STR_5    "5"
#define H264_LEVEL_STR_5_1  "5.1"


enum
{
    H241_PROFILES                      = 41 | PluginCodec_H245_Collapsing | PluginCodec_H245_TCS | PluginCodec_H245_OLC | PluginCodec_H245_ReqMode | PluginCodec_H245_BooleanArray,
    H241_LEVEL                         = 42 | PluginCodec_H245_Collapsing | PluginCodec_H245_TCS | PluginCodec_H245_OLC | PluginCodec_H245_ReqMode,
    H241_CustomMaxMBPS                 =  3 | PluginCodec_H245_Collapsing | PluginCodec_H245_TCS | PluginCodec_H245_OLC | PluginCodec_H245_ReqMode,
    H241_CustomMaxFS                   =  4 | PluginCodec_H245_Collapsing | PluginCodec_H245_TCS | PluginCodec_H245_OLC | PluginCodec_H245_ReqMode,
    H241_CustomMaxDPB                  =  5 | PluginCodec_H245_Collapsing | PluginCodec_H245_TCS | PluginCodec_H245_OLC | PluginCodec_H245_ReqMode,
    H241_CustomMaxBRandCPB             =  6 | PluginCodec_H245_Collapsing | PluginCodec_H245_TCS | PluginCodec_H245_OLC | PluginCodec_H245_ReqMode,
    H241_MaxStaticMBPS                 =  7 | PluginCodec_H245_Collapsing | PluginCodec_H245_TCS | PluginCodec_H245_OLC | PluginCodec_H245_ReqMode,
    H241_Max_RCMD_NALU_size            =  8 | PluginCodec_H245_Collapsing | PluginCodec_H245_TCS | PluginCodec_H245_OLC | PluginCodec_H245_ReqMode,
    H241_Max_NAL_unit_size             =  9 | PluginCodec_H245_Collapsing | PluginCodec_H245_TCS | PluginCodec_H245_OLC | PluginCodec_H245_ReqMode,
    H241_SampleAspectRatiosSupported   = 10 | PluginCodec_H245_Collapsing | PluginCodec_H245_TCS | PluginCodec_H245_OLC | PluginCodec_H245_ReqMode,
    H241_AdditionalModesSupported      = 11 | PluginCodec_H245_Collapsing | PluginCodec_H245_TCS | PluginCodec_H245_OLC | PluginCodec_H245_ReqMode | PluginCodec_H245_BooleanArray,
    H241_AdditionalDisplayCapabilities = 12 | PluginCodec_H245_Collapsing | PluginCodec_H245_TCS | PluginCodec_H245_OLC | PluginCodec_H245_ReqMode | PluginCodec_H245_BooleanArray,
};

static struct PluginCodec_Option const Profile =
{
  PluginCodec_EnumOption,             // Option type
  PLUGINCODEC_OPTION_PROFILE,         // User visible name
  false,                              // User Read/Only flag
  PluginCodec_MinMerge,               // Merge mode
  DefaultProfileStr,                  // Initial value
  NULL,                               // FMTP option name
  NULL,                               // FMTP default value
  0,                                  // H.245 generic capability code and bit mask
  // Enum values, single string of value separated by colons
  H264_PROFILE_STR_BASELINE ":"
  H264_PROFILE_STR_MAIN     ":"
  H264_PROFILE_STR_EXTENDED
};                                  

static struct PluginCodec_Option const Level =
{
  PluginCodec_EnumOption,             // Option type
  PLUGINCODEC_OPTION_LEVEL,           // User visible name
  false,                              // User Read/Only flag
  PluginCodec_MinMerge,               // Merge mode
  DefaultLevelStr,                    // Initial value
  NULL,                               // FMTP option name
  NULL,                               // FMTP default value
  0,                                  // H.245 generic capability code and bit mask
  // Enum values, single string of value separated by colons
  H264_LEVEL_STR_1   ":"
  H264_LEVEL_STR_1_b ":" 
  H264_LEVEL_STR_1_1 ":"
  H264_LEVEL_STR_1_2 ":"
  H264_LEVEL_STR_1_3 ":"
  H264_LEVEL_STR_2   ":"
  H264_LEVEL_STR_2_1 ":"
  H264_LEVEL_STR_2_2 ":"
  H264_LEVEL_STR_3   ":"
  H264_LEVEL_STR_3_1 ":"
  H264_LEVEL_STR_3_2 ":"
  H264_LEVEL_STR_4   ":"
  H264_LEVEL_STR_4_1 ":"
  H264_LEVEL_STR_4_2 ":"
  H264_LEVEL_STR_5   ":"
  H264_LEVEL_STR_5_1
};

static const char NameH241Profile[] = "H.241 Profile Mask";
static struct PluginCodec_Option const H241Profiles =
{
  PluginCodec_IntegerOption,          // Option type
  NameH241Profile,                    // User visible name
  true,                               // User Read/Only flag
  PluginCodec_MinMerge,               // Merge mode
  STRINGIZE(DefaultProfileH241),      // Initial value
  NULL,                               // FMTP option name
  NULL,                               // FMTP default value
  H241_PROFILES,                      // H.245 generic capability code and bit mask
  "1",                                // Minimum value
  "127"                               // Maximum value
};

static const char NameH241Level[] = "H.241 Level";
static struct PluginCodec_Option const H241Level =
{
  PluginCodec_IntegerOption,          // Option type
  NameH241Level,                      // User visible name
  true,                               // User Read/Only flag
  PluginCodec_MinMerge,               // Merge mode
  STRINGIZE(DefaultLevelH241),        // Initial value
  NULL,                               // FMTP option name
  NULL,                               // FMTP default value
  H241_LEVEL,                         // H.245 generic capability code and bit mask
  "15",                               // Minimum value
  "113"                               // Maximum value
};

static const char NameSDPProfileAndLevel[] = "SIP/SDP Profile & Level";
static struct PluginCodec_Option const SDPProfileAndLevel =
{
  PluginCodec_OctetsOption,           // Option type
  NameSDPProfileAndLevel,             // User visible name
  true,                               // User Read/Only flag
  PluginCodec_NoMerge,                // Merge mode
  DefaultSDPProfileAndLevel,          // Initial value
  "profile-level-id",                 // FMTP option name
  "42800A"                            // FMTP default value (as per RFC)
};

static const char NameMaxMBPS[] = "Max MBPS";
static struct PluginCodec_Option const MaxMBPS =
{
  PluginCodec_IntegerOption,          // Option type
  NameMaxMBPS,                        // User visible name
  false,                              // User Read/Only flag
  PluginCodec_MinMerge,               // Merge mode
  "0",                                // Initial value
  "max-mbps",                         // FMTP option name
  "0",                                // FMTP default value
  H241_CustomMaxMBPS,                 // H.245 generic capability code and bit mask
  "0",                                // Minimum value
  "983040"                            // Maximum value
};

static const char NameMaxFS[] = "Max FS";
static struct PluginCodec_Option const MaxFS =
{
  PluginCodec_IntegerOption,          // Option type
  NameMaxFS,                          // User visible name
  false,                              // User Read/Only flag
  PluginCodec_MinMerge,               // Merge mode
  "0",                                // Initial value
  "max-fs",                           // FMTP option name
  "0",                                // FMTP default value
  H241_CustomMaxFS,                   // H.245 generic capability code and bit mask
  "0",                                // Minimum value
  "36864"                             // Maximum value
};

static const char NameMaxBR[] = "Max BR";
static struct PluginCodec_Option const MaxBR =
{
  PluginCodec_IntegerOption,          // Option type
  NameMaxBR,                          // User visible name
  true,                               // User Read/Only flag
  PluginCodec_MinMerge,               // Merge mode
  "0",                                // Initial value
  "max-br",                           // FMTP option name
  "0",                                // FMTP default value
  0,                                  // H.245 generic capability code and bit mask
  "0",                                // Minimum value
  "240000"                            // Maximum value
};

static const char NameMaxNaluSize[] = "Max NALU Size";
static struct PluginCodec_Option const MaxNaluSize =
{
  PluginCodec_IntegerOption,          // Option type
  NameMaxNaluSize,                    // User visible name
  false,                              // User Read/Only flag
  PluginCodec_MinMerge,               // Merge mode
  STRINGIZE(H241_MAX_NALU_SIZE),      // Initial value
  "max-rcmd-nalu-size",               // FMTP option name
  STRINGIZE(H241_MAX_NALU_SIZE),      // FMTP default value
  H241_Max_NAL_unit_size,             // H.245 generic capability code and bit mask
  STRINGIZE(UNCOMPRESSED_MB_SIZE),    // Minimum value
  "65535"                             // Maximum value
};

#define OpalPluginCodec_Identifer_H264_Truncated "0.0.8.241.0.0.0" // Some stupid endpoints (e.g. Polycom) use this, one zero short!

static struct PluginCodec_Option const MediaPacketizations =
{
  PluginCodec_StringOption,           // Option type
  PLUGINCODEC_MEDIA_PACKETIZATIONS,   // User visible name
  true,                               // User Read/Only flag
  PluginCodec_NoMerge,                // Merge mode
  OpalPluginCodec_Identifer_H264_Aligned "," OpalPluginCodec_Identifer_H264_NonInterleaved // Initial value
  "," OpalPluginCodec_Identifer_H264_Truncated
};

static const char NamePacketizationMode[] = PLUGINCODEC_OPTION_H264_PACKET_MODE;
static struct PluginCodec_Option const PacketizationMode_0 =
{
  PluginCodec_IntegerOption,          // Option type
  NamePacketizationMode,              // User visible name
  true,                               // User Read/Only flag
  PluginCodec_EqualMerge,             // Merge mode
  "0",                                // Initial value
  "packetization-mode",               // FMTP option name
  "0",                                // FMTP default value
  0,                                  // H.245 generic capability code and bit mask
  "0",                                // Minimum value
  "2"                                 // Maximum value
};

static struct PluginCodec_Option const PacketizationMode_1 =
{
  PluginCodec_IntegerOption,          // Option type
  NamePacketizationMode,              // User visible name
  true,                               // User Read/Only flag
  PluginCodec_EqualMerge,             // Merge mode
  "1",                                // Initial value
  "packetization-mode",               // FMTP option name
  "0",                                // FMTP default value
  0,                                  // H.245 generic capability code and bit mask
  "0",                                // Minimum value
  "2"                                 // Maximum value
};

static const char NameSendAccessUnitDelimiters[] = "Send Access Unit Delimiters";
static struct PluginCodec_Option const SendAccessUnitDelimiters =
{
  PluginCodec_BoolOption,                         // Option type
  NameSendAccessUnitDelimiters,                   // User visible name
  false,                                          // User Read/Only flag
  PluginCodec_AndMerge,                           // Merge mode
  STRINGIZE(DefaultSendAccessUnitDelimiters)      // Initial value
};

static struct PluginCodec_Option const * const OptionTable[] = {
  &Profile,
  &Level,
  &H241Profiles,
  &H241Level,
  &SDPProfileAndLevel,
  &MaxNaluSize,
  &MaxMBPS,
  &MaxFS,
  &MaxBR,
  &MediaPacketizations,
  &SendAccessUnitDelimiters,
  NULL
};

static struct PluginCodec_Option const * const OptionTable_0[] = {
  &Profile,
  &Level,
  &SDPProfileAndLevel,
  &MaxNaluSize,
  &MaxMBPS,
  &MaxFS,
  &MaxBR,
  &PacketizationMode_0,
  &SendAccessUnitDelimiters,
  NULL
};

static struct PluginCodec_Option const * const OptionTable_1[] = {
  &Profile,
  &Level,
  &SDPProfileAndLevel,
  &MaxNaluSize,
  &MaxMBPS,
  &MaxFS,
  &MaxBR,
  &PacketizationMode_1,
  &SendAccessUnitDelimiters,
  NULL
};


static struct PluginCodec_H323GenericCodecData H323GenericData = {
  OpalPluginCodec_Identifer_H264_Generic
};


static struct {
  char     m_Name[9];
  unsigned m_H264;
  unsigned m_H241;
  unsigned m_Constraints;
} const ProfileInfo[] = {
  { H264_PROFILE_STR_BASELINE, 66, 64, 0x80 },
  { H264_PROFILE_STR_MAIN,     77, 32, 0x40 },
  { H264_PROFILE_STR_EXTENDED, 88, 16, 0x20 }
};

static struct LevelInfoStruct {
  char     m_Name[4];
  unsigned m_H264;
  unsigned m_constraints;
  unsigned m_H241;
  unsigned m_MaxFrameSize;   // In macroblocks
  unsigned m_MaxWidthHeight; // sqrt(m_MaxFrameSize*8)*16
  unsigned m_MaxMBPS;        // In macroblocks/second
  unsigned m_MaxBitRate;
} const LevelInfo[] = {
  // Table A-1 from H.264 specification
  { H264_LEVEL_STR_1,    10, 0x00,  15,    99,  448,   1485,     64000 },
  { H264_LEVEL_STR_1_b,  11, 0x10,  19,    99,  448,   1485,    128000 },
  { H264_LEVEL_STR_1_1,  11, 0x00,  22,   396,  896,   3000,    192000 },
  { H264_LEVEL_STR_1_2,  12, 0x00,  29,   396,  896,   6000,    384000 },
  { H264_LEVEL_STR_1_3,  13, 0x00,  36,   396,  896,  11880,    768000 },
  { H264_LEVEL_STR_2,    20, 0x00,  43,   396,  896,  11880,   2000000 },
  { H264_LEVEL_STR_2_1,  21, 0x00,  50,   792, 1264,  19800,   4000000 },
  { H264_LEVEL_STR_2_2,  22, 0x00,  57,  1620, 1808,  20250,   4000000 },
  { H264_LEVEL_STR_3,    30, 0x00,  64,  1620, 1808,  40500,  10000000 },
  { H264_LEVEL_STR_3_1,  31, 0x00,  71,  3600, 2704, 108000,  14000000 },
  { H264_LEVEL_STR_3_2,  32, 0x00,  78,  5120, 3232, 216000,  20000000 },
  { H264_LEVEL_STR_4,    40, 0x00,  85,  8192, 4096, 245760,  25000000 },
  { H264_LEVEL_STR_4_1,  41, 0x00,  92,  8292, 4112, 245760,  62500000 },
  { H264_LEVEL_STR_4_2,  42, 0x00,  99,  8704, 4208, 522340,  62500000 },
  { H264_LEVEL_STR_5,    50, 0x00, 106, 22080, 6720, 589824, 135000000 },
  { H264_LEVEL_STR_5_1,  51, 0x00, 113, 36864, 8320, 983040, 240000000 }
};

static unsigned MyMaxBitRate = LevelInfo[sizeof(LevelInfo)/sizeof(LevelInfo[0])-1].m_MaxBitRate;


static struct {
  unsigned m_width;
  unsigned m_height;
  unsigned m_macroblocks;
} MaxVideoResolutions[] = {
#define VIDEO_RESOLUTION(width, height) { width, height, ((width+15)/16) * ((height+15)/16) }
  VIDEO_RESOLUTION(2816, 2304),
  VIDEO_RESOLUTION(1920, 1080),
  VIDEO_RESOLUTION(1408, 1152),
  VIDEO_RESOLUTION(1280,  720),
  VIDEO_RESOLUTION( 704,  576),
  VIDEO_RESOLUTION( 640,  480),
  VIDEO_RESOLUTION( 352,  288),
  VIDEO_RESOLUTION( 320,  240),
  VIDEO_RESOLUTION( 176,  144),
  VIDEO_RESOLUTION( 128,   96)
};
static size_t LastMaxVideoResolutions = sizeof(MaxVideoResolutions)/sizeof(MaxVideoResolutions[0]) - 1;


static unsigned GetMacroBlocks(unsigned width, unsigned height)
{
  return ((width+15)/16) * ((height+15)/16);
}


static int hexdigit(char ch)
{
  if (ch < '0')
    return 0;
  if (ch <= '9')
    return ch -'0';
  ch = (char)tolower(ch);
  if (ch < 'a')
    return 0;
  if (ch <= 'f')
    return ch - 'a' + 10;

  return 0;
}


static unsigned hexbyte(const char * hex)
{
  return ((hexdigit(hex[0]) << 4) | hexdigit(hex[1]));
}


///////////////////////////////////////////////////////////////////////////////

class MyPluginMediaFormat : public PluginCodec_MediaFormat
{
    bool m_sipOnly;

  public:
    MyPluginMediaFormat(OptionsTable options, bool sipOnly)
      : PluginCodec_MediaFormat(options)
      , m_sipOnly(sipOnly)
    {
    }


  static void ClampOptions(const LevelInfoStruct & info,
                           unsigned maxWidth,
                           unsigned maxHeight,
                           unsigned & maxFrameSize,
                           OptionMap & original,
                           OptionMap & changed)
  {
    unsigned macroBlocks = GetMacroBlocks(maxWidth, maxHeight);
    if (macroBlocks > maxFrameSize || maxWidth > info.m_MaxWidthHeight || maxHeight > info.m_MaxWidthHeight) {
      size_t i = 0;
      while (i < LastMaxVideoResolutions &&
             (MaxVideoResolutions[i].m_macroblocks > maxFrameSize ||
              MaxVideoResolutions[i].m_width > info.m_MaxWidthHeight ||
              MaxVideoResolutions[i].m_height > info.m_MaxWidthHeight))
        ++i;
      maxWidth = MaxVideoResolutions[i].m_width;
      maxHeight = MaxVideoResolutions[i].m_height;
      PTRACE(5, "H.264", "Reduced max resolution to " << maxWidth << 'x' << maxHeight
                 << " (" << macroBlocks << '>' << maxFrameSize << ')');
      macroBlocks = MaxVideoResolutions[i].m_macroblocks;
    }

    maxFrameSize = macroBlocks;

    ClampMax(maxWidth,  original, changed, PLUGINCODEC_OPTION_MIN_RX_FRAME_WIDTH);
    ClampMax(maxHeight, original, changed, PLUGINCODEC_OPTION_MIN_RX_FRAME_HEIGHT);
    ClampMax(maxWidth,  original, changed, PLUGINCODEC_OPTION_MAX_RX_FRAME_WIDTH);
    ClampMax(maxHeight, original, changed, PLUGINCODEC_OPTION_MAX_RX_FRAME_HEIGHT);
    ClampMax(maxWidth,  original, changed, PLUGINCODEC_OPTION_FRAME_WIDTH);
    ClampMax(maxHeight, original, changed, PLUGINCODEC_OPTION_FRAME_HEIGHT);

    // Frame rate
    unsigned maxMBPS = std::max(info.m_MaxMBPS, String2Unsigned(original[NameMaxMBPS]));
    ClampMin(90000*macroBlocks/maxMBPS, original, changed, PLUGINCODEC_OPTION_FRAME_TIME);

    // Bit rate
    unsigned maxBitRate = std::max(info.m_MaxBitRate, String2Unsigned(original[NameMaxBR])*1000);
    ClampMax(maxBitRate, original, changed, PLUGINCODEC_OPTION_MAX_BIT_RATE);
    ClampMax(maxBitRate, original, changed, PLUGINCODEC_OPTION_TARGET_BIT_RATE);
  }


  virtual bool ToNormalised(OptionMap & original, OptionMap & changed)
  {
    size_t levelIndex = 0;
    size_t profileIndex = sizeof(ProfileInfo)/sizeof(ProfileInfo[0]);

    if (original["Protocol"] == "H.323") {
      unsigned h241profiles = String2Unsigned(original[NameH241Profile]);
      while (--profileIndex > 0) {
        if ((h241profiles&ProfileInfo[profileIndex].m_H241) != 0)
          break;
      }

      unsigned h241level = String2Unsigned(original[NameH241Level]);
      for (; levelIndex < sizeof(LevelInfo)/sizeof(LevelInfo[0])-1; ++levelIndex) {
        if (h241level <= LevelInfo[levelIndex].m_H241)
          break;
      }
    }
    else {
      std::string sdpProfLevel = original[NameSDPProfileAndLevel];
      if (sdpProfLevel.length() < 6) {
        PTRACE(1, "H.264", "SDP profile-level-id field illegal.");
        return false;
      }

      unsigned sdpProfile = hexbyte(&sdpProfLevel[0]);
      while (--profileIndex > 0) {
        if (sdpProfile == ProfileInfo[profileIndex].m_H264)
          break;
      }

      unsigned sdpConstraints = hexbyte(&sdpProfLevel[2]);
      unsigned sdpLevel = hexbyte(&sdpProfLevel[4]);

      // convert sdpLevel to an index into LevelInfo
      for (; levelIndex < sizeof(LevelInfo)/sizeof(LevelInfo[0])-1; ++levelIndex) {

        // if not reached SDP level yet, keep looking
        if (LevelInfo[levelIndex].m_H264 < sdpLevel)
          continue;

        // if gone past the level, stop
        if (LevelInfo[levelIndex].m_H264 > sdpLevel)
          break;

        // if reached the level, stop if constraints are the same
        if ((sdpConstraints & 0x10) == LevelInfo[levelIndex].m_constraints)
          break;
      }
    }

    Change(ProfileInfo[profileIndex].m_Name, original, changed, PLUGINCODEC_OPTION_PROFILE);
    Change(LevelInfo[levelIndex].m_Name, original, changed, PLUGINCODEC_OPTION_LEVEL);

    unsigned maxFrameSizeInMB = std::max(LevelInfo[levelIndex].m_MaxFrameSize, String2Unsigned(original[NameMaxFS]));
    ClampOptions(LevelInfo[levelIndex],
                 String2Unsigned(original[PLUGINCODEC_OPTION_MAX_RX_FRAME_WIDTH]),
                 String2Unsigned(original[PLUGINCODEC_OPTION_MAX_RX_FRAME_HEIGHT]),
                 maxFrameSizeInMB,
                 original, changed);

    return true;
  }


  virtual bool ToCustomised(OptionMap & original, OptionMap & changed)
  {
    // Determine the profile
    std::string str = original[PLUGINCODEC_OPTION_PROFILE];
    if (str.empty())
      str = H264_PROFILE_STR_BASELINE;

    size_t profileIndex = sizeof(ProfileInfo)/sizeof(ProfileInfo[0]);
    while (--profileIndex > 0) {
      if (str == ProfileInfo[profileIndex].m_Name)
        break;
    }

    Change(ProfileInfo[profileIndex].m_H241, original, changed, NameH241Profile);

    // get the current level 
    str = original[PLUGINCODEC_OPTION_LEVEL];
    if (str.empty())
      str = H264_LEVEL_STR_1_3;

    size_t levelIndex = sizeof(LevelInfo)/sizeof(LevelInfo[0]);
    while (--levelIndex > 0) {
      if (str == LevelInfo[levelIndex].m_Name)
        break;
    }
    PTRACE(5, "H.264", "Level \"" << str << "\" selected index " << levelIndex);

    /* While we have selected the desired level, we may need to adjust it
       further due to resolution restrictions. This is due to the fact that
       we have no other mechnism to prevent the remote from sending, say
       CIF when we only support QCIF.
     */
    unsigned maxWidth = String2Unsigned(original[PLUGINCODEC_OPTION_MAX_RX_FRAME_WIDTH]);
    unsigned maxHeight = String2Unsigned(original[PLUGINCODEC_OPTION_MAX_RX_FRAME_HEIGHT]);
    unsigned maxFrameSizeInMB = GetMacroBlocks(maxWidth, maxHeight);
    if (maxFrameSizeInMB > 0) {
      while (levelIndex > 0 && maxFrameSizeInMB < LevelInfo[levelIndex].m_MaxFrameSize)
        --levelIndex;
    }
    PTRACE(5, "H.264", "Max resolution " << maxWidth << 'x' << maxHeight << " selected index " << levelIndex);

    unsigned bitRate = String2Unsigned(original[PLUGINCODEC_OPTION_MAX_BIT_RATE]);
    if (bitRate > LevelInfo[levelIndex].m_MaxBitRate)
      Change(bitRate/1000, original, changed, NameMaxBR);

    // set the new level
    Change(LevelInfo[levelIndex].m_H241, original, changed, NameH241Level);

    // Calculate SDP parameters from the adjusted profile/level
    char sdpProfLevel[7];
    sprintf(sdpProfLevel, "%02x%02x%02x",
            ProfileInfo[profileIndex].m_H264,
            ProfileInfo[profileIndex].m_Constraints | LevelInfo[levelIndex].m_constraints,
            LevelInfo[levelIndex].m_H264);
    Change(sdpProfLevel, original, changed, NameSDPProfileAndLevel);

    // Clamp other variables (width/height etc) according to the adjusted profile/level
    ClampOptions(LevelInfo[levelIndex], maxWidth, maxHeight, maxFrameSizeInMB, original, changed);

    // Do this afer the clamping, maxFrameSizeInMB may change
    if (maxFrameSizeInMB > LevelInfo[levelIndex].m_MaxFrameSize)
      Change(maxFrameSizeInMB, original, changed, NameMaxFS);

    return true;
  }


  virtual bool IsValidForProtocol(const char * protocol)
  {
    return (strcasecmp(protocol, "SIP") == 0) == m_sipOnly;
  }
}; 

/* SIP requires two completely independent media formats for packetisation
   modes zero and one. */
static MyPluginMediaFormat MyMediaFormatInfo  (OptionTable  , false);
static MyPluginMediaFormat MyMediaFormatInfo_0(OptionTable_0, true );
static MyPluginMediaFormat MyMediaFormatInfo_1(OptionTable_1, true );


///////////////////////////////////////////////////////////////////////////////

class MyEncoder : public PluginCodec
{
  protected:
    unsigned m_width;
    unsigned m_height;
    unsigned m_frameRate;
    unsigned m_bitRate;
    unsigned m_profile;
    unsigned m_level;
    unsigned m_packetisationMode;
    bool     m_sendAccessUnitDelimiters;

  public:
    MyEncoder(const PluginCodec_Definition * defn)
      : PluginCodec(defn)
      , m_width(352)
      , m_height(288)
      , m_frameRate(15)
      , m_bitRate(512000)
      , m_profile(DefaultProfileInt)
      , m_level(DefaultLevelInt)
      , m_packetisationMode(1)
      , m_sendAccessUnitDelimiters(false)
    {
    }


    virtual bool Construct()
    {
      /* Complete construction of object after it has been created. This
         allows you to fail the create operation (return false), which cannot
         be done in the normal C++ constructor. */
      return true;
    }


    virtual bool SetOption(const char * optionName, const char * optionValue)
    {
      if (strcasecmp(optionName, PLUGINCODEC_OPTION_FRAME_WIDTH) == 0)
        return SetOptionUnsigned(m_width, optionValue, 16, MyMaxWidth);

      if (strcasecmp(optionName, PLUGINCODEC_OPTION_FRAME_HEIGHT) == 0)
        return SetOptionUnsigned(m_height, optionValue, 16, MyMaxHeight);

      if (strcasecmp(optionName, PLUGINCODEC_OPTION_FRAME_TIME) == 0) {
        unsigned frameTime = MyClockRate/m_frameRate;
        if (!SetOptionUnsigned(frameTime, optionValue, MyClockRate/MyMaxFrameRate, MyClockRate))
          return false;
        m_frameRate = MyClockRate/frameTime;
        return true;
      }

      if (strcasecmp(optionName, PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
        return SetOptionUnsigned(m_bitRate, optionValue, 1000);

      if (strcasecmp(optionName, NameH241Level) == 0) {
        size_t i;
        for (i = 0; i < sizeof(LevelInfo)/sizeof(LevelInfo[0])-1; i++) {
          if ((unsigned)m_level <= LevelInfo[i].m_H264)
            break;
        }
        unsigned h241level = LevelInfo[i].m_H241;

        if (!SetOptionUnsigned(h241level, optionValue, 15, 133))
          return false;

        for (i = 0; i < sizeof(LevelInfo)/sizeof(LevelInfo[0])-1; i++) {
          if (h241level <= LevelInfo[i].m_H241)
            break;
        }
        m_level = LevelInfo[i].m_H264;
        return true;
      }

      if (strcasecmp(optionName, NameSendAccessUnitDelimiters) == 0)
        return SetOptionBoolean(m_sendAccessUnitDelimiters, optionValue);

      if (strcasecmp(optionName, PLUGINCODEC_MEDIA_PACKETIZATIONS) == 0 ||
          strcasecmp(optionName, PLUGINCODEC_MEDIA_PACKETIZATION) == 0) {
        if (strstr(optionValue, OpalPluginCodec_Identifer_H264_Interleaved) != NULL)
          return SetPacketisationMode(2);
        if (strstr(optionValue, OpalPluginCodec_Identifer_H264_NonInterleaved) != NULL)
          return SetPacketisationMode(1);
        if (strstr(optionValue, OpalPluginCodec_Identifer_H264_Aligned) != NULL ||
            strstr(optionValue, OpalPluginCodec_Identifer_H264_Truncated) != NULL)
          return SetPacketisationMode(0);
        PTRACE(2, "H.264", "Unknown packetisation mode: \"" << optionValue << '"');
        return false; // Unknown/unsupported media packetization
      }

      if (strcasecmp(optionName, NamePacketizationMode) == 0)
        return SetPacketisationMode(atoi(optionValue));

      // Base class sets bit rate and frame time
      return PluginCodec::SetOption(optionName, optionValue);
    }


    bool SetPacketisationMode(unsigned mode)
    {
      PTRACE(4, "H.264", "Setting NALU " << (mode == 0 ? "aligned" : "fragmentation") << " mode.");

      if (mode > 2) // Or 3 if support interleaved mode
        return false; // Unknown/unsupported packetization mode

      if (m_packetisationMode != mode) {
        m_packetisationMode = mode;
        m_optionsSame = false;
      }

      return true;
    }


    virtual bool OnChangedOptions()
    {
      /* After all the options are set via SetOptions() this is called if any
         of them changed. This would do whatever is needed to set parmaeters
         for the actual codec. */
      return true;
    }


    virtual bool Transcode(const void * fromPtr,
                             unsigned & fromLen,
                                 void * toPtr,
                             unsigned & toLen,
                             unsigned & flags)
    {
      // At this point fromPtr and toPtr are validated as non NULL

      const PluginCodec_Video_FrameHeader * videoHeader =
                (const PluginCodec_Video_FrameHeader *)PluginCodec_RTP_GetPayloadPtr(fromPtr);
      const unsigned char * videoBuffer = OPAL_VIDEO_FRAME_DATA_PTR(videoHeader);

      /* Check for catastrophic failures, if return false, aborts the codec
        conversion completely, and will be closed and re-opened. */
      if (fromLen < videoHeader->width*videoHeader->height*3/2 + 
                    sizeof(PluginCodec_Video_FrameHeader) +
                    PluginCodec_RTP_GetHeaderLength(fromPtr))
        return false;

      /* Do conversion of source YUV420 video frame to RTP packets, do not exceed
         the size indicated in toLen for any of the output packets. If there are
         no packets generated, set the toLen to zero and no packets will be sent
         over the wire. This allows for various bit rate control algorithms. */

      // Set output packet size, including RTP header
      toLen = 1400;

      // If we determine this is an Intra (reference) frame, set flag
      if (true)
        flags |= PluginCodec_ReturnCoderIFrame;

      // If we determine this is the last RTP packet of video frame, set flag
      if (true)
        flags |= PluginCodec_ReturnCoderLastFrame;

      return true;
    }
};


///////////////////////////////////////////////////////////////////////////////

class MyDecoder : public PluginCodec
{
  public:
    MyDecoder(const PluginCodec_Definition * defn)
      : PluginCodec(defn)
    {
    }


    virtual bool Construct()
    {
      /* Complete construction of object after it has been created. This
         allows you to fail the create operation (return false), which cannot
         be done in the normal C++ constructor. */
      return true;
    }


    virtual size_t GetOutputDataSize()
    {
      /* Example just uses maximums. If they are very large, e.g. 4096x4096
         then a VERY large buffer will be created by OPAL. It is more sensible
         to use a "typical" size here and allow it to get bigger via the
         PluginCodec_ReturnCoderBufferTooSmall mechanism if needed. */

      return MyMaxWidth*MyMaxHeight*3/2 + 
             sizeof(PluginCodec_Video_FrameHeader) +
             PluginCodec_RTP_MinHeaderSize;
    }


    virtual bool Transcode(const void * fromPtr,
                             unsigned & fromLen,
                                 void * toPtr,
                             unsigned & toLen,
                             unsigned & flags)
    {
      // At this point fromPtr and toPtr are validated as non NULL

      PluginCodec_Video_FrameHeader * videoHeader =
                (PluginCodec_Video_FrameHeader *)PluginCodec_RTP_GetPayloadPtr(toPtr);
      unsigned char * videoBuffer = OPAL_VIDEO_FRAME_DATA_PTR(videoHeader);

      /* Do conversion of RTP packet. Note that typically many are received
         before an output frame is generated. If no output fram is available
         yet, simply return true and more will be sent. */

      // Test if we MUST output an Intra frame;
      if ((flags&PluginCodec_CoderForceIFrame) != 0)
        /*Force reference frame */;

      /* If we determine there is some non-fatal decoding error that will
         cause the output to be corrupted, set flag to get new Intra frame */
      if (true)
        flags |= PluginCodec_ReturnCoderRequestIFrame;

      /* If we determine via toLen the output is not big enough, set flag.
         Make sure GetOutputDataSize() then returns correct larger size. */
      if (true)
        flags |= PluginCodec_ReturnCoderBufferTooSmall;

      // If we determine this is an Intra (reference) frame, set flag
      if (true)
        flags |= PluginCodec_ReturnCoderIFrame;

      // If we determine we have enough input to generate output, set flag
      if (true)
        flags |= PluginCodec_ReturnCoderLastFrame;

      return true;
    }
};


///////////////////////////////////////////////////////////////////////////////

PLUGINCODEC_DEFINE_CONTROL_TABLE(MyControls);

static struct PluginCodec_Definition MyCodecDefinition[] =
{
  {
    // Encoder H.323
    MyVersion,                          // codec API version
    &LicenseInfo,                       // license information

    PluginCodec_MediaTypeVideo |        // audio codec
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    MyDescription,                      // text decription
    YUV420PFormatName,                  // source format
    FormatNameH323,                     // destination format

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
    MyPayloadName,                      // IANA RTP payload name

    PluginCodec::Create<MyEncoder>,     // create codec function
    PluginCodec::Destroy,               // destroy codec
    PluginCodec::Transcode,             // encode/decode
    MyControls,                         // codec controls

    PluginCodec_H323Codec_generic,      // h323CapabilityType 
    &H323GenericData                    // h323CapabilityData
  },
  { 
    // Decoder H.323
    MyVersion,                          // codec API version
    &LicenseInfo,                       // license information

    PluginCodec_MediaTypeVideo |        // audio codec
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    MyDescription,                      // text decription
    FormatNameH323,                     // source format
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
    MyPayloadName,                      // IANA RTP payload name

    PluginCodec::Create<MyDecoder>,     // create codec function
    PluginCodec::Destroy,               // destroy codec
    PluginCodec::Transcode,             // encode/decode
    MyControls,                         // codec controls

    PluginCodec_H323Codec_generic,      // h323CapabilityType 
    &H323GenericData                    // h323CapabilityData
  },
  {
    // Encoder SIP mode 1
    MyVersion,                          // codec API version
    &LicenseInfo,                       // license information

    PluginCodec_MediaTypeVideo |        // audio codec
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    MyDescription,                      // text decription
    YUV420PFormatName,                  // source format
    FormatNameSIP1,                     // destination format

    &MyMediaFormatInfo_1,               // user data 

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
    MyPayloadName,                      // IANA RTP payload name

    PluginCodec::Create<MyEncoder>,     // create codec function
    PluginCodec::Destroy,               // destroy codec
    PluginCodec::Transcode,             // encode/decode
    MyControls,                         // codec controls

    PluginCodec_H323Codec_NoH323,       // h323CapabilityType 
    NULL                                // h323CapabilityData
  },
  { 
    // Decoder SIP mode 1
    MyVersion,                          // codec API version
    &LicenseInfo,                       // license information

    PluginCodec_MediaTypeVideo |        // audio codec
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    MyDescription,                      // text decription
    FormatNameSIP1,                     // source format
    YUV420PFormatName,                  // destination format

    &MyMediaFormatInfo_1,               // user data 

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
    MyPayloadName,                      // IANA RTP payload name

    PluginCodec::Create<MyDecoder>,     // create codec function
    PluginCodec::Destroy,               // destroy codec
    PluginCodec::Transcode,             // encode/decode
    MyControls,                         // codec controls

    PluginCodec_H323Codec_NoH323,       // h323CapabilityType 
    NULL                                // h323CapabilityData
  },
  {
    // Encoder SIP mode 0
    MyVersion,                          // codec API version
    &LicenseInfo,                       // license information

    PluginCodec_MediaTypeVideo |        // audio codec
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    MyDescription,                      // text decription
    YUV420PFormatName,                  // source format
    FormatNameSIP0,                     // destination format

    &MyMediaFormatInfo_0,               // user data 

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
    MyPayloadName,                      // IANA RTP payload name

    PluginCodec::Create<MyEncoder>,     // create codec function
    PluginCodec::Destroy,               // destroy codec
    PluginCodec::Transcode,             // encode/decode
    MyControls,                         // codec controls

    PluginCodec_H323Codec_NoH323,       // h323CapabilityType 
    NULL                                // h323CapabilityData
  },
  { 
    // Decoder SIP mode 0
    MyVersion,                          // codec API version
    &LicenseInfo,                       // license information

    PluginCodec_MediaTypeVideo |        // audio codec
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    MyDescription,                      // text decription
    FormatNameSIP0,                     // source format
    YUV420PFormatName,                  // destination format

    &MyMediaFormatInfo_0,               // user data 

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
    MyPayloadName,                      // IANA RTP payload name

    PluginCodec::Create<MyDecoder>,     // create codec function
    PluginCodec::Destroy,               // destroy codec
    PluginCodec::Transcode,             // encode/decode
    MyControls,                         // codec controls

    PluginCodec_H323Codec_NoH323,       // h323CapabilityType 
    NULL                                // h323CapabilityData
  }
};

extern "C"
{
  PLUGIN_CODEC_IMPLEMENT_ALL(MY_CODEC, MyCodecDefinition, MyVersion)
};

/////////////////////////////////////////////////////////////////////////////
