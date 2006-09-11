/*
 * h323plugins.cxx
 *
 * OPAL codec plugins handler
 *
 * OPAL Library
 *
 * Copyright (C) 2005 Post Increment
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
 * The Initial Developer of the Original Code is Post Increment
 *
 * Contributor(s): ______________________________________.
 *
 * $Log: opalpluginmgr.cxx,v $
 * Revision 1.2010  2006/09/11 04:48:55  csoutheren
 * Fixed problem with cloning plugin media formats
 *
 * Revision 2.8  2006/09/07 09:05:44  csoutheren
 * Fix case significance in IsValidForProtocol
 *
 * Revision 2.7  2006/09/06 22:36:11  csoutheren
 * Fix problem with IsValidForProtocol on video codecs
 *
 * Revision 2.6  2006/08/22 03:08:42  csoutheren
 * Fixed compile error on gcc 4.1
 *
 * Revision 2.5  2006/08/20 03:56:57  csoutheren
 * Add OpalMediaFormat::IsValidForProtocol to allow plugin codecs to be enabled only for certain protocols
 * rather than relying on the presence of the IANA rtp encoding name field
 *
 * Revision 2.4  2006/08/15 23:52:55  csoutheren
 * Ensure codecs with same name but different clock rate get different payload types
 *
 * Revision 2.3  2006/08/11 07:52:01  csoutheren
 * Fix problem with media format factory in VC 2005
 * Fixing problems with Speex codec
 * Remove non-portable usages of PFactory code
 *
 * Revision 2.2  2006/08/10 06:05:32  csoutheren
 * Fix problem with key type in plugin factory
 *
 * Revision 2.1  2006/07/24 14:03:39  csoutheren
 * Merged in audio and video plugins from CVS branch PluginBranch
 *
 * Revision 1.1.2.15  2006/05/16 07:03:09  csoutheren
 * Fixed Linux compile system
 *
 * Revision 1.1.2.14  2006/04/26 08:03:58  csoutheren
 * H.263 encoding and decoding now working from plugin for both SIP and H.323
 *
 * Revision 1.1.2.13  2006/04/26 05:05:59  csoutheren
 * H.263 decoding working via codec plugin
 *
 * Revision 1.1.2.12  2006/04/25 01:14:54  csoutheren
 * Added H.263 capabilities
 *
 * Revision 1.1.2.11  2006/04/24 09:09:13  csoutheren
 * Added H.263 codec definitions
 *
 * Revision 1.1.2.10  2006/04/21 05:42:52  csoutheren
 * Checked in forgotten changes to fix iFrame requests
 *
 * Revision 1.1.2.9  2006/04/19 07:52:30  csoutheren
 * Add ability to have SIP-only and H.323-only codecs, and implement for H.261
 *
 * Revision 1.1.2.8  2006/04/19 04:58:56  csoutheren
 * Debugging and testing of new video plugins
 * H.261 working in both CIF and QCIF modes in H.323
 *
 * Revision 1.1.2.7  2006/04/11 08:40:22  csoutheren
 * Modified H.261 plugin handling to use new media formats
 *
 * Revision 1.1.2.6  2006/04/10 08:53:39  csoutheren
 * Fix problem with audio code parameters
 *
 * Revision 1.1.2.5  2006/04/06 05:48:08  csoutheren
 * Fixed last minute edits to allow compilation
 *
 * Revision 1.1.2.4  2006/04/06 01:21:18  csoutheren
 * More implementation of video codec plugins
 *
 * Revision 1.1.2.3  2006/03/23 07:55:18  csoutheren
 * Audio plugin H.323 capability merging completed.
 * GSM, LBC, G.711 working. Speex and LPC-10 are not
 *
 * Revision 1.1.2.2  2006/03/20 05:03:23  csoutheren
 * Changes to make audio plugins work with SIP
 *
 * Revision 1.1.2.1  2006/03/16 07:06:00  csoutheren
 * Initial support for audio plugins
 *
 * Created from OpenH323 h323pluginmgr.cxx
 * Revision 1.58  2005/08/05 17:11:03  csoutheren
 *
 */

#ifdef __GNUC__
#pragma implementation "opalpluginmgr.h"
#endif

#include <ptlib.h>

#include <opal/transcoders.h>
#include <codec/opalpluginmgr.h>
#include <codec/opalplugin.h>
#include <h323/h323caps.h>
#include <asn/h245.h>

#if OPAL_VIDEO
#include <codec/vidcodec.h>
#endif

// G.711 is *always* available
#include <codec/g711codec.h>
OPAL_REGISTER_G711();


#define H323CAP_TAG_PREFIX    "h323"
static const char GET_CODEC_OPTIONS_CONTROL[]    = "get_codec_options";
static const char FREE_CODEC_OPTIONS_CONTROL[]   = "free_codec_options";
static const char GET_OUTPUT_DATA_SIZE_CONTROL[] = "get_output_data_size";
static const char SET_CODEC_OPTIONS_CONTROL[]    = "set_codec_options";


#if OPAL_VIDEO

#define CIF_WIDTH         352
#define CIF_HEIGHT        288

#define CIF4_WIDTH        (CIF_WIDTH*2)
#define CIF4_HEIGHT       (CIF_HEIGHT*2)

#define CIF16_WIDTH       (CIF_WIDTH*4)
#define CIF16_HEIGHT      (CIF_HEIGHT*4)

#define QCIF_WIDTH        (CIF_WIDTH/2)
#define QCIF_HEIGHT       (CIF_HEIGHT/2)

#define SQCIF_WIDTH       128
#define SQCIF_HEIGHT      96

#if OPAL_H323

// H.261 only
static const char * h323_stillImageTransmission_tag            = H323CAP_TAG_PREFIX "_stillImageTransmission";

// H.261/H.263 tags
static const char * h323_qcifMPI_tag                           = H323CAP_TAG_PREFIX "_qcifMPI";
static const char * h323_cifMPI_tag                            = H323CAP_TAG_PREFIX "_cifMPI";

// H.263 only
static const char * h323_sqcifMPI_tag                          = H323CAP_TAG_PREFIX "_sqcifMPI";
static const char * h323_cif4MPI_tag                           = H323CAP_TAG_PREFIX "_cif4MPI";
static const char * h323_cif16MPI_tag                          = H323CAP_TAG_PREFIX "_cif16MPI";
//static const char * h323_slowSqcifMPI_tag                      = H323CAP_TAG_PREFIX "_slowSqcifMPI";
//static const char * h323_slowQcifMPI_tag                       = H323CAP_TAG_PREFIX "_slowQcifMPI";
//static const char * h323_slowCifMPI_tag                        = H323CAP_TAG_PREFIX "slowCifMPI";
//static const char * h323_slowCif4MPI_tag                       = H323CAP_TAG_PREFIX "_slowCif4MPI";
//static const char * h323_slowCif16MPI_tag                      = H323CAP_TAG_PREFIX "_slowCif16MPI";
static const char * h323_temporalSpatialTradeOffCapability_tag = H323CAP_TAG_PREFIX "_temporalSpatialTradeOffCapability";
static const char * h323_unrestrictedVector_tag                = H323CAP_TAG_PREFIX "_unrestrictedVector";
static const char * h323_arithmeticCoding_tag                  = H323CAP_TAG_PREFIX "_arithmeticCoding";      
static const char * h323_advancedPrediction_tag                = H323CAP_TAG_PREFIX "_advancedPrediction";
static const char * h323_pbFrames_tag                          = H323CAP_TAG_PREFIX "_pbFrames";
static const char * h323_hrdB_tag                              = H323CAP_TAG_PREFIX "_hrdB";
static const char * h323_bppMaxKb_tag                          = H323CAP_TAG_PREFIX "_bppMaxKb";
static const char * h323_errorCompensation_tag                 = H323CAP_TAG_PREFIX "_errorCompensation";

#endif // OPAL_H323
#endif // OPAL_VIDEO



//////////////////////////////////////////////////////////////////////////////
//
// Helper functions for codec control operators
//

static const char ** PStringArrayToArray(const PStringArray & list, BOOL addNull = FALSE)
{
  const char ** array = (const char **)malloc(sizeof(char *) * (list.GetSize() + (addNull ? 1 : 0)));
  PINDEX i;
  for (i = 0; i < list.GetSize(); ++i)
    array[i] = (const char *)list[i];
  if (addNull)
    array[i] = NULL;
  return array;
}

static BOOL CallCodecControl(PluginCodec_Definition * codec, 
                                               void * context,
                                         const char * name,
                                               void * parm, 
                                       unsigned int * parmLen,
                                                int & retVal)
{
  PluginCodec_ControlDefn * codecControls = codec->codecControls;
  if (codecControls == NULL)
    return FALSE;

  while (codecControls->name != NULL) {
    if (strcasecmp(codecControls->name, name) == 0) {
      retVal = (*codecControls->control)(codec, context, name, parm, parmLen);
      return TRUE;
    }
    codecControls++;
  }

  return FALSE;
}

static PluginCodec_ControlDefn * GetCodecControl(PluginCodec_Definition * codec, const char * name)
{
  PluginCodec_ControlDefn * codecControls = codec->codecControls;
  if (codecControls == NULL)
    return NULL;

  while (codecControls->name != NULL) {
    if (strcasecmp(codecControls->name, name) == 0)
      return codecControls;
    codecControls++;
  }

  return NULL;
}

/////////////////////////////////////////////////////////////////////////////

template <typename CodecClass>
class OpalFixedCodecFactory : public PFactory<OpalFactoryCodec>
{
  public:
    class Worker : public PFactory<OpalFactoryCodec>::WorkerBase 
    {
      public:
        Worker(const PString & key)
          : PFactory<OpalFactoryCodec>::WorkerBase()
        { PFactory<OpalFactoryCodec>::Register(key, this); }

      protected:
        virtual OpalFactoryCodec * Create(const PString &) const
        { return new CodecClass(); }
    };
};


static PString CreateCodecName(PluginCodec_Definition * codec, BOOL /*addSW*/)
{
  PString str;
  if (codec->destFormat != NULL)
    str = codec->destFormat;
  else
    str = PString(codec->descr);
  return str;
}

static PString CreateCodecName(const PString & baseName, BOOL /*addSW*/)
{
  return baseName;
}


static void PopulateMediaFormatOptions(PluginCodec_Definition * _encoderCodec, OpalMediaFormat & format)
{
  char ** _options = NULL;
  unsigned int optionsLen = sizeof(_options);
  int retVal;
  if (CallCodecControl(_encoderCodec, NULL, GET_CODEC_OPTIONS_CONTROL, &_options, &optionsLen, retVal) && (_options != NULL)) {
    char ** options = _options;
    while (options[0] != NULL && options[1] != NULL && options[2] != NULL) {
      const char * key = options[0];
      const char * val = options[1];
      const char * type = options[2];

      OpalMediaOption::MergeType op = OpalMediaOption::NoMerge;
      if (val != NULL && val[0] != '\0' && val[1] != '\0') {
        switch (val[0]) {
          case '<':
            op = OpalMediaOption::MinMerge;
            ++val;
            break;
          case '>':
            op = OpalMediaOption::MaxMerge;
            ++val;
            break;
          case '=':
            op = OpalMediaOption::EqualMerge;
            ++val;
            break;
          case '!':
            op = OpalMediaOption::NotEqualMerge;
            ++val;
            break;
          case '*':
            op = OpalMediaOption::AlwaysMerge;
            ++val;
            break;
          default:
            break;
        }
      }

      if (type != NULL && type[0] != '\0') {
        PStringArray tokens = PString(val+1).Tokenise(':', FALSE);
        const char ** array = PStringArrayToArray(tokens, FALSE);
        switch (toupper(type[0])) {
          case 'E':
            format.AddOption(new OpalMediaOptionEnum(key, false, array, tokens.GetSize(), op, tokens.GetStringsIndex(val)));
            break;
          case 'B':
            format.AddOption(new OpalMediaOptionBoolean(key, false, op, val != NULL && (val[0] == '1' || toupper(val[0] == 'T'))));
            break;
          case 'R':
            if (tokens.GetSize() < 2)
              format.AddOption(new OpalMediaOptionReal(key, false, op, PString(val).AsReal()));
            else
              format.AddOption(new OpalMediaOptionReal(key, false, op, PString(val).AsReal(), tokens[0].AsReal(), tokens[1].AsReal()));
            break;
          case 'I':
            if (tokens.GetSize() < 2)
              format.AddOption(new OpalMediaOptionInteger(key, false, op, PString(val).AsInteger()));
            else
              format.AddOption(new OpalMediaOptionInteger(key, false, op, PString(val).AsInteger(), tokens[0].AsInteger(), tokens[1].AsInteger()));
            break;
          case 'S':
          default:
            format.AddOption(new OpalMediaOptionString(key, false, val));
            break;
        }
        free(array);
      }
      options += 3;
    }
    CallCodecControl(_encoderCodec, NULL, FREE_CODEC_OPTIONS_CONTROL, _options, &optionsLen, retVal);
  }
}

static bool IsValidForProtocol(PluginCodec_Definition * encoderCodec, const PString & _protocol) 
{
  PString protocol(_protocol.ToLower());
  int retVal;
  unsigned int parmLen = sizeof(const char *);
  if (CallCodecControl(encoderCodec, NULL, "valid_for_protocol", (void *)(const char *)protocol, &parmLen, retVal))
    return retVal != 0;
  if (protocol == "h.323" || protocol == "h323")
    return (encoderCodec->h323CapabilityType != PluginCodec_H323Codec_undefined) &&
           (encoderCodec->h323CapabilityType != PluginCodec_H323Codec_NoH323);
  if (protocol == "sip") 
    return encoderCodec->sdpFormat != NULL;
  return FALSE;
}

#if OPAL_AUDIO

class OpalPluginAudioMediaFormat : public OpalAudioFormat
{
  public:
    friend class OpalPluginCodecManager;

    OpalPluginAudioMediaFormat(
      PluginCodec_Definition * _encoderCodec,
      const char * rtpEncodingName, /// rtp encoding name
      unsigned frameTime,           /// Time for frame in RTP units (if applicable)
      unsigned /*timeUnits*/,           /// RTP units for frameTime (if applicable)
      time_t timeStamp              /// timestamp (for versioning)
    )
    : OpalAudioFormat(
      CreateCodecName(_encoderCodec, FALSE),
      (RTP_DataFrame::PayloadTypes)(((_encoderCodec->flags & PluginCodec_RTPTypeMask) == PluginCodec_RTPTypeDynamic) ? RTP_DataFrame::DynamicBase : _encoderCodec->rtpPayload),
      rtpEncodingName,
      _encoderCodec->bytesPerFrame,
      frameTime,
      _encoderCodec->recommendedFramesPerPacket,
      _encoderCodec->recommendedFramesPerPacket,
      _encoderCodec->maxFramesPerPacket,
      _encoderCodec->sampleRate,
      timeStamp  
    )
    , encoderCodec(_encoderCodec)
    {
      PopulateMediaFormatOptions(_encoderCodec, *this);

      // manually register the new singleton type, as we do not have a concrete type
      OpalMediaFormatFactory::Register(*this, this);
    }
    ~OpalPluginAudioMediaFormat()
    {
      OpalMediaFormatFactory::Unregister(*this);
    }

    bool IsValidForProtocol(const PString & protocol) const
    {
      return ::IsValidForProtocol(encoderCodec, protocol);
    }

    PObject * Clone() const
    { return new OpalPluginAudioMediaFormat(*this); }

    PluginCodec_Definition * encoderCodec;
};

static H323Capability * CreateG7231Cap(
  PluginCodec_Definition * encoderCodec, 
  PluginCodec_Definition * decoderCodec,
  int subType
);

static H323Capability * CreateGenericAudioCap(
  PluginCodec_Definition * encoderCodec, 
  PluginCodec_Definition * decoderCodec,
  int subType
);

static H323Capability * CreateNonStandardAudioCap(
  PluginCodec_Definition * encoderCodec, 
  PluginCodec_Definition * decoderCodec,
  int subType
);

static H323Capability * CreateGSMCap(
  PluginCodec_Definition * encoderCodec, 
  PluginCodec_Definition * decoderCodec,
  int subType
);

#endif // OPAL_AUDIO

#if OPAL_VIDEO

class OpalPluginVideoMediaFormat : public OpalVideoFormat
{
  public:
    friend class OpalPluginCodecManager;

    OpalPluginVideoMediaFormat(
      PluginCodec_Definition * _encoderCodec,
      const char * rtpEncodingName, /// rtp encoding name
      time_t timeStamp              /// timestamp (for versioning)
    )
    : OpalVideoFormat(
      CreateCodecName(_encoderCodec, FALSE),
      (RTP_DataFrame::PayloadTypes)(((_encoderCodec->flags & PluginCodec_RTPTypeMask) == PluginCodec_RTPTypeDynamic) ? RTP_DataFrame::DynamicBase : _encoderCodec->rtpPayload),
      rtpEncodingName,
      _encoderCodec->samplesPerFrame,      // actually frame width
      _encoderCodec->bytesPerFrame,        // actually frame height,
      _encoderCodec->maxFramesPerPacket,   // actually max frameRate
      _encoderCodec->bitsPerSec,
      timeStamp  
    )
    , encoderCodec(_encoderCodec)
    {
      PopulateMediaFormatOptions(_encoderCodec, *this);

      // manually register the new singleton type, as we do not have a concrete type
      OpalMediaFormatFactory::Register(*this, this);
    }
    ~OpalPluginVideoMediaFormat()
    {
      OpalMediaFormatFactory::Unregister(*this);
    }

    PObject * Clone() const
    { return new OpalPluginVideoMediaFormat(*this); }

    bool IsValidForProtocol(const PString & protocol) const
    {
      return ::IsValidForProtocol(encoderCodec, protocol);
    }

    PluginCodec_Definition * encoderCodec;
};

static H323Capability * CreateNonStandardVideoCap(
  PluginCodec_Definition * encoderCodec, 
  PluginCodec_Definition * decoderCodec,
  int subType
);

static H323Capability * CreateGenericVideoCap(
  PluginCodec_Definition * encoderCodec, 
  PluginCodec_Definition * decoderCodec,
  int subType
);

static H323Capability * CreateH261Cap(
  PluginCodec_Definition * encoderCodec, 
  PluginCodec_Definition * decoderCodec,
  int subType
);

static H323Capability * CreateH263Cap(
  PluginCodec_Definition * encoderCodec, 
  PluginCodec_Definition * decoderCodec,
  int subType
);

#endif // OPAL_VIDEO

////////////////////////////////////////////////////////////////////

#if OPAL_H323

class H323CodecPluginCapabilityMapEntry {
  public:
    int pluginCapType;
    int h323SubType;
    H323Capability * (* createFunc)(PluginCodec_Definition * encoderCodec, PluginCodec_Definition * decoderCodec, int subType);
};

#if OPAL_AUDIO

static H323CodecPluginCapabilityMapEntry audioMaps[] = {
  { PluginCodec_H323Codec_nonStandard,              H245_AudioCapability::e_nonStandard,         &CreateNonStandardAudioCap },
  { PluginCodec_H323AudioCodec_gsmFullRate,	        H245_AudioCapability::e_gsmFullRate,         &CreateGSMCap },
  { PluginCodec_H323AudioCodec_gsmHalfRate,	        H245_AudioCapability::e_gsmHalfRate,         &CreateGSMCap },
  { PluginCodec_H323AudioCodec_gsmEnhancedFullRate, H245_AudioCapability::e_gsmEnhancedFullRate, &CreateGSMCap },
  { PluginCodec_H323AudioCodec_g711Alaw_64k,        H245_AudioCapability::e_g711Alaw64k },
  { PluginCodec_H323AudioCodec_g711Alaw_56k,        H245_AudioCapability::e_g711Alaw56k },
  { PluginCodec_H323AudioCodec_g711Ulaw_64k,        H245_AudioCapability::e_g711Ulaw64k },
  { PluginCodec_H323AudioCodec_g711Ulaw_56k,        H245_AudioCapability::e_g711Ulaw56k },
  { PluginCodec_H323AudioCodec_g7231,               H245_AudioCapability::e_g7231,               &CreateG7231Cap },
  { PluginCodec_H323AudioCodec_g729,                H245_AudioCapability::e_g729 },
  { PluginCodec_H323AudioCodec_g729AnnexA,          H245_AudioCapability::e_g729AnnexA },
  { PluginCodec_H323AudioCodec_g728,                H245_AudioCapability::e_g728 }, 
  { PluginCodec_H323AudioCodec_g722_64k,            H245_AudioCapability::e_g722_64k },
  { PluginCodec_H323AudioCodec_g722_56k,            H245_AudioCapability::e_g722_56k },
  { PluginCodec_H323AudioCodec_g722_48k,            H245_AudioCapability::e_g722_48k },
  { PluginCodec_H323AudioCodec_g729wAnnexB,         H245_AudioCapability::e_g729wAnnexB }, 
  { PluginCodec_H323AudioCodec_g729AnnexAwAnnexB,   H245_AudioCapability::e_g729AnnexAwAnnexB },
  { PluginCodec_H323Codec_generic,                  H245_AudioCapability::e_genericAudioCapability, &CreateGenericAudioCap },

  // not implemented
  //{ PluginCodec_H323AudioCodec_g729Extensions,      H245_AudioCapability::e_g729Extensions,   0 },
  //{ PluginCodec_H323AudioCodec_g7231AnnexC,         H245_AudioCapability::e_g7231AnnexCMode   0 },
  //{ PluginCodec_H323AudioCodec_is11172,             H245_AudioCapability::e_is11172AudioMode, 0 },
  //{ PluginCodec_H323AudioCodec_is13818Audio,        H245_AudioCapability::e_is13818AudioMode, 0 },

  { -1 }
};

#endif // OPAL_AUDIO

#if OPAL_VIDEO

static H323CodecPluginCapabilityMapEntry videoMaps[] = {
  // video codecs
  { PluginCodec_H323Codec_nonStandard,              H245_VideoCapability::e_nonStandard,            &CreateNonStandardVideoCap },
  { PluginCodec_H323VideoCodec_h261,                H245_VideoCapability::e_h261VideoCapability,    &CreateH261Cap },
  { PluginCodec_H323VideoCodec_h263,                H245_VideoCapability::e_h263VideoCapability,    &CreateH263Cap },
  { PluginCodec_H323Codec_generic,                  H245_AudioCapability::e_genericAudioCapability, &CreateGenericVideoCap },
/*
  PluginCodec_H323VideoCodec_h262,                // not yet implemented
  PluginCodec_H323VideoCodec_is11172,             // not yet implemented
*/

  { -1 }
};

#endif  // OPAL_VIDEO

#endif // OPAL_H323

//////////////////////////////////////////////////////////////////////////////

template<class TranscoderClass>
class OpalPluginTranscoderFactory : public OpalTranscoderFactory
{
  public:
    class Worker : public OpalTranscoderFactory::WorkerBase 
    {
      public:
        Worker(const OpalMediaFormatPair & key, PluginCodec_Definition * _codecDefn, BOOL _isEncoder)
          : OpalTranscoderFactory::WorkerBase(), codecDefn(_codecDefn), isEncoder(_isEncoder)
        { OpalTranscoderFactory::Register(key, this); }

      protected:
        virtual OpalTranscoder * Create(const OpalMediaFormatPair &) const
        { return new TranscoderClass(codecDefn, isEncoder); }

        PluginCodec_Definition * codecDefn;
        BOOL isEncoder;
    };
};

//////////////////////////////////////////////////////////////////////////////
//
// Plugin framed audio codec classes
//

#if OPAL_AUDIO
class OpalPluginFramedAudioTranscoder : public OpalFramedTranscoder
{
  PCLASSINFO(OpalPluginFramedAudioTranscoder, OpalFramedTranscoder);
  public:
    OpalPluginFramedAudioTranscoder(PluginCodec_Definition * _codec, BOOL _isEncoder)
      : OpalFramedTranscoder( (strcmp(_codec->sourceFormat, "L16") == 0) ? "PCM-16" : _codec->sourceFormat,
                              (strcmp(_codec->destFormat, "L16") == 0)   ? "PCM-16" : _codec->destFormat,
                             _isEncoder ? _codec->samplesPerFrame*2 : _codec->bytesPerFrame,
                             _isEncoder ? _codec->bytesPerFrame     : _codec->samplesPerFrame*2),
        codec(_codec), isEncoder(_isEncoder)
    { 
      if (codec != NULL && codec->createCodec != NULL) 
        context = (*codec->createCodec)(codec); 
      else 
        context = NULL; 
    }

    ~OpalPluginFramedAudioTranscoder()
    { 
      if (codec != NULL && codec->destroyCodec != NULL) 
        (*codec->destroyCodec)(codec, context); 
    }

    BOOL ConvertFrame(
      const BYTE * input,   ///<  Input data
      PINDEX & consumed,    ///<  number of input bytes consumed
      BYTE * output,        ///<  Output data
      PINDEX & created      ///<  number of output bytes created  
    )
    {
      if (codec == NULL || codec->codecFunction == NULL)
        return FALSE;

      unsigned int fromLen = consumed;
      unsigned int toLen   = created;
      unsigned flags = 0;

      BOOL stat = (codec->codecFunction)(codec, context, 
                                   (const unsigned char *)input, &fromLen,
                                   output, &toLen,
                                   &flags) != 0;
      consumed = fromLen;
      created  = toLen;

      return stat;
    };

    virtual BOOL ConvertSilentFrame(BYTE * buffer)
    { 
      if (codec == NULL)
        return FALSE;

      // for a decoder, this mean that we need to create a silence frame
      // which is easy - ask the decoder, or just create silence
      if (!isEncoder) {
        unsigned int length = codec->samplesPerFrame*2;
        if ((codec->flags & PluginCodec_DecodeSilence) == 0)
          memset(buffer, 0, length); 
        else {
          unsigned flags = PluginCodec_CoderSilenceFrame;
          (codec->codecFunction)(codec, context, 
                                 NULL, NULL,
                                 buffer, &length,
                                 &flags);
        }
      }

      // for an encoder, we encode silence but set the flag so it can do something special if need be
      else {
        unsigned int length = codec->bytesPerFrame;
        if ((codec->flags & PluginCodec_EncodeSilence) == 0) {
          PShortArray silence(codec->samplesPerFrame);
          memset(silence.GetPointer(), 0, codec->samplesPerFrame*sizeof(short));
          unsigned silenceLen = codec->samplesPerFrame * sizeof(short);
          unsigned flags = 0;
          (codec->codecFunction)(codec, context, 
                                 silence, &silenceLen,
                                 buffer, &length,
                                 &flags);
        }
        else {
          unsigned flags = PluginCodec_CoderSilenceFrame;
          (codec->codecFunction)(codec, context, 
                                 NULL, NULL,
                                 buffer, &length,
                                 &flags);
        }
      }

      return TRUE;
    }

  protected:
    void * context;
    PluginCodec_Definition * codec;
    BOOL isEncoder;
};

//////////////////////////////////////////////////////////////////////////////
//
// Plugin streamed audio codec classes
//

class OpalPluginStreamedAudioTranscoder : public OpalStreamedTranscoder
{
  PCLASSINFO(OpalPluginStreamedAudioTranscoder, OpalStreamedTranscoder);
  public:
    OpalPluginStreamedAudioTranscoder(PluginCodec_Definition * _codec, unsigned inputBits, unsigned outputBits, PINDEX optimalBits)
      : OpalStreamedTranscoder((strcmp(_codec->sourceFormat, "L16") == 0) ? "PCM-16" : _codec->sourceFormat,
                               (strcmp(_codec->destFormat, "L16") == 0) ? "PCM-16" : _codec->destFormat,
                               inputBits, outputBits, optimalBits), 
        codec(_codec)
    { 
      if (codec == NULL || codec->createCodec == NULL) 
        context = NULL; 
      else {
        context = (*codec->createCodec)(codec); 
      }
    }

    ~OpalPluginStreamedAudioTranscoder()
    { 
      if (codec != NULL && codec->destroyCodec != NULL) 
        (*codec->destroyCodec)(codec, context); 
    }

  protected:
    void * context;
    PluginCodec_Definition * codec;
};

class OpalPluginStreamedAudioEncoder : public OpalPluginStreamedAudioTranscoder
{
  PCLASSINFO(OpalPluginStreamedAudioEncoder, OpalPluginStreamedAudioTranscoder);
  public:
    OpalPluginStreamedAudioEncoder(PluginCodec_Definition * _codec, BOOL)
      : OpalPluginStreamedAudioTranscoder(_codec, 16, (_codec->flags & PluginCodec_BitsPerSampleMask) >> PluginCodec_BitsPerSamplePos, _codec->recommendedFramesPerPacket)
    {
    }

    int ConvertOne(int _sample) const
    {
      if (codec == NULL || codec->codecFunction == NULL)
        return 0;

      short sample = (short)_sample;
      unsigned int fromLen = sizeof(sample);
      int to;
      unsigned toLen = sizeof(to);
      unsigned flags = 0;
      (codec->codecFunction)(codec, context, 
                             (const unsigned char *)&sample, &fromLen,
                             (unsigned char *)&to, &toLen,
                             &flags);
      return to;
    }
};

class OpalPluginStreamedAudioDecoder : public OpalPluginStreamedAudioTranscoder
{
  PCLASSINFO(OpalPluginStreamedAudioDecoder, OpalPluginStreamedAudioTranscoder);
  public:
    OpalPluginStreamedAudioDecoder(PluginCodec_Definition * _codec, BOOL)
      : OpalPluginStreamedAudioTranscoder(_codec, (_codec->flags & PluginCodec_BitsPerSampleMask) >> PluginCodec_BitsPerSamplePos, 16, _codec->recommendedFramesPerPacket)
    {
    }

    int ConvertOne(int codedSample) const
    {
      if (codec == NULL || codec->codecFunction == NULL)
        return 0;

      unsigned int fromLen = sizeof(codedSample);
      short to;
      unsigned toLen = sizeof(to);
      unsigned flags = 0;
      (codec->codecFunction)(codec, context, 
                             (const unsigned char *)&codedSample, &fromLen,
                             (unsigned char *)&to, &toLen,
                             &flags);
      return to;
    }
};



#endif //  OPAL_AUDIO

#if OPAL_VIDEO

/////////////////////////////////////////////////////////////////////////////

class OpalPluginVideoTranscoder : public OpalVideoTranscoder
{
  PCLASSINFO(OpalPluginVideoTranscoder, OpalVideoTranscoder);
  public:
    OpalPluginVideoTranscoder(PluginCodec_Definition * _codec, BOOL _isEncoder);
    ~OpalPluginVideoTranscoder();
    BOOL HasCodecControl(const char * name);
    BOOL CallCodecControl(const char * name,
                                      void * parm, 
                              unsigned int * parmLen,
                                       int & retVal);
    BOOL ExecuteCommand(const OpalMediaCommand & /*command*/);
    PINDEX GetOptimalDataFrameSize(BOOL input) const;
    BOOL ConvertFrames(const RTP_DataFrame & src, RTP_DataFrameList & dstList);

  protected:
    void * context;
    PluginCodec_Definition * codec;
    BOOL isEncoder;
    RTP_DataFrame * bufferRTP;
};

OpalPluginVideoTranscoder::OpalPluginVideoTranscoder(PluginCodec_Definition * _codec, BOOL _isEncoder)
  : OpalVideoTranscoder(_codec->sourceFormat, _codec->destFormat),
    codec(_codec), isEncoder(_isEncoder)
{ 
  if (codec == NULL || codec->createCodec == NULL) 
    context = NULL;
  else {
    context = (*codec->createCodec)(codec); 
    PluginCodec_ControlDefn * ctl = GetCodecControl(_codec, SET_CODEC_OPTIONS_CONTROL);
    if (ctl != NULL) {
      PStringArray list;
      const OpalMediaFormat & fmt = GetInputFormat();
      for (PINDEX i = 0; i < fmt.GetOptionCount(); i++) {
        const OpalMediaOption & option = fmt.GetOption(i);
        list += option.GetName();
        list += option.AsString();
      }
      const char ** _options = PStringArrayToArray(list, TRUE);
      unsigned int optionsLen = sizeof(_options);
      (*ctl->control)(_codec, context, SET_CODEC_OPTIONS_CONTROL, _options, &optionsLen);
      free(_options);
    }
  }

  bufferRTP = NULL;
}

OpalPluginVideoTranscoder::~OpalPluginVideoTranscoder()
{ 
  if (bufferRTP == NULL)
    delete bufferRTP;

  if (codec != NULL && codec->destroyCodec != NULL) 
    (*codec->destroyCodec)(codec, context); 
}

BOOL OpalPluginVideoTranscoder::HasCodecControl(const char * name)
{
  PluginCodec_ControlDefn * codecControls = codec->codecControls;
  if (codecControls == NULL)
    return 0;

  while (codecControls->name != NULL) {
    if (strcmp(codecControls->name, name) == 0)
      return TRUE;
    codecControls++;
  }

  return FALSE;
}

BOOL OpalPluginVideoTranscoder::CallCodecControl(const char * name,
                                  void * parm, 
                          unsigned int * parmLen,
                                    int & retVal)
{
  PluginCodec_ControlDefn * codecControls = codec->codecControls;
  if (codecControls == NULL)
    return FALSE;

  while (codecControls->name != NULL) {
    if (strcmp(codecControls->name, name) == 0) {
      retVal = (*codecControls->control)(codec, context, name, parm, parmLen);
      return TRUE;
    }
    codecControls++;
  }

  return FALSE;
}

BOOL OpalPluginVideoTranscoder::ExecuteCommand(const OpalMediaCommand & command)
{
  if (PIsDescendant(&command, OpalVideoUpdatePicture)) {
    ++updatePictureCount;
    return TRUE;
  }

  return OpalTranscoder::ExecuteCommand(command);
}

PINDEX OpalPluginVideoTranscoder::GetOptimalDataFrameSize(BOOL /*input*/) const
{
  return (frameWidth * frameHeight * 12) / 8;
  //if (isEncoder) 
  //  return input ? ((frameWidth * frameHeight * 12) / 8) : codec->maxFramesPerPacket;
  //else
  //  return input ? codec->maxFramesPerPacket : ((frameWidth * frameHeight * 12) / 8);
}

BOOL OpalPluginVideoTranscoder::ConvertFrames(const RTP_DataFrame & src, RTP_DataFrameList & dstList)
{
  if (codec == NULL || codec->codecFunction == NULL)
    return FALSE;

  dstList.RemoveAll();

  // get the size of the output buffer
  int outputDataSize;
  if (!CallCodecControl(GET_OUTPUT_DATA_SIZE_CONTROL, NULL, NULL, outputDataSize))
    return FALSE;

  unsigned flags;

  if (isEncoder) {

    do {

      // create the output buffer
      RTP_DataFrame * dst = new RTP_DataFrame(outputDataSize);

      // call the codec function
      unsigned int fromLen = src.GetSize();
      unsigned int toLen = dst->GetSize();
      BOOL forceIFrame = updatePictureCount > 0;
      flags = forceIFrame ? PluginCodec_CoderForceIFrame : 0;

      BOOL stat = (codec->codecFunction)(codec, context, 
                                        (const BYTE *)src, &fromLen,
                                        dst->GetPointer(), &toLen,
                                        &flags) != 0;

      if (forceIFrame && ((flags & PluginCodec_ReturnCoderIFrame) != 0))
        --updatePictureCount;

      if (!stat) {
        delete dst;
        return FALSE;
      }

      if (toLen > 0) {
        dst->SetPayloadSize(toLen - dst->GetHeaderSize());
        dstList.Append(dst);
      }

    } while ((flags & PluginCodec_ReturnCoderLastFrame) == 0);
  }

  else {

    if (bufferRTP == NULL)
      bufferRTP = new RTP_DataFrame(outputDataSize);
    else
      bufferRTP->SetPayloadSize(outputDataSize);

    // call the codec function
    unsigned int fromLen = src.GetHeaderSize() + src.GetPayloadSize();
    unsigned int toLen = bufferRTP->GetSize();
    flags = 0;
    BOOL stat = (codec->codecFunction)(codec, context, 
                                        (const BYTE *)src, &fromLen,
                                        bufferRTP->GetPointer(), &toLen,
                                        &flags) != 0;
    if (!stat)
      return FALSE;

    if ((flags & PluginCodec_ReturnCoderRequestIFrame) != 0) {
      if (commandNotifier != PNotifier()) {
        //OpalVideoUpdatePicture updatePictureCommand;
        //commandNotifier(updatePictureCommand, 0); 
        PTRACE (3, "H261\t Could not decode frame, sending VideoUpdatePicture in hope of an I-Frame.");
      }
    }

    if (toLen > (unsigned)bufferRTP->GetHeaderSize() && (flags & PluginCodec_ReturnCoderLastFrame) != 0) {
      bufferRTP->SetPayloadSize(toLen - bufferRTP->GetHeaderSize());
      dstList.Append(bufferRTP);
      bufferRTP = NULL;
    }
  }

  return TRUE;
};


#endif // OPAL_VIDEO

#if OPAL_H323

//////////////////////////////////////////////////////////////////////////////
//
// Helper class for handling plugin capabilities
//

class H323PluginCapabilityInfo
{
  public:
    H323PluginCapabilityInfo(PluginCodec_Definition * _encoderCodec,
                             PluginCodec_Definition * _decoderCodec);

    H323PluginCapabilityInfo(const PString & _baseName);

    const PString & GetFormatName() const
    { return capabilityFormatName; }

  protected:
    PluginCodec_Definition * encoderCodec;
    PluginCodec_Definition * decoderCodec;
    PString                  capabilityFormatName;
};

#if OPAL_AUDIO

//////////////////////////////////////////////////////////////////////////////
//
// Class for handling most audio plugin capabilities
//

class H323AudioPluginCapability : public H323AudioCapability,
                                  public H323PluginCapabilityInfo
{
  PCLASSINFO(H323AudioPluginCapability, H323AudioCapability);
  public:
    H323AudioPluginCapability(PluginCodec_Definition * _encoderCodec,
                         PluginCodec_Definition * _decoderCodec,
                         unsigned _pluginSubType)
      : H323AudioCapability(), 
        H323PluginCapabilityInfo(_encoderCodec, _decoderCodec),
        pluginSubType(_pluginSubType)
      { 
        SetTxFramesInPacket(_decoderCodec->maxFramesPerPacket);
        // _encoderCodec->recommendedFramesPerPacket
      }

    // this constructor is only used when creating a capability without a codec
    H323AudioPluginCapability(const PString & _mediaFormat, const PString & _baseName,
                         unsigned maxFramesPerPacket, unsigned /*recommendedFramesPerPacket*/,
                         unsigned _pluginSubType)
      : H323AudioCapability(), 
        H323PluginCapabilityInfo(_baseName),
        pluginSubType(_pluginSubType)
      { 
        for (PINDEX i = 0; audioMaps[i].pluginCapType >= 0; i++) {
          if (audioMaps[i].pluginCapType == (int)_pluginSubType) { 
            h323subType = audioMaps[i].h323SubType;
            break;
          }
        }
        rtpPayloadType = OpalMediaFormat(_mediaFormat).GetPayloadType();
        SetTxFramesInPacket(maxFramesPerPacket);
        // recommendedFramesPerPacket
      }

    virtual PObject * Clone() const
    { return new H323AudioPluginCapability(*this); }

    virtual PString GetFormatName() const
    { return H323PluginCapabilityInfo::GetFormatName();}

    virtual unsigned GetSubType() const
    { return pluginSubType; }

  protected:
    unsigned pluginSubType;
    unsigned h323subType;   // only set if using capability without codec
};

//////////////////////////////////////////////////////////////////////////////
//
// Class for handling non standard audio capabilities
//

class H323CodecPluginNonStandardAudioCapability : public H323NonStandardAudioCapability,
                                                  public H323PluginCapabilityInfo
{
  PCLASSINFO(H323CodecPluginNonStandardAudioCapability, H323NonStandardAudioCapability);
  public:
    H323CodecPluginNonStandardAudioCapability(
                                   PluginCodec_Definition * _encoderCodec,
                                   PluginCodec_Definition * _decoderCodec,
                                   H323NonStandardCapabilityInfo::CompareFuncType compareFunc,
                                   const unsigned char * data, unsigned dataLen);

    H323CodecPluginNonStandardAudioCapability(
                                   PluginCodec_Definition * _encoderCodec,
                                   PluginCodec_Definition * _decoderCodec,
                                   const unsigned char * data, unsigned dataLen);

    virtual PObject * Clone() const
    { return new H323CodecPluginNonStandardAudioCapability(*this); }

    virtual PString GetFormatName() const
    { return H323PluginCapabilityInfo::GetFormatName();}
};

//////////////////////////////////////////////////////////////////////////////
//
// Class for handling generic audio capabilities
//

class H323CodecPluginGenericAudioCapability : public H323GenericAudioCapability,
					                                    public H323PluginCapabilityInfo
{
  PCLASSINFO(H323CodecPluginGenericAudioCapability, H323GenericAudioCapability);
  public:
    H323CodecPluginGenericAudioCapability(
                                   const PluginCodec_Definition * _encoderCodec,
                                   const PluginCodec_Definition * _decoderCodec,
				   const PluginCodec_H323GenericCodecData * data );

    virtual PObject * Clone() const
    { return new H323CodecPluginGenericAudioCapability(*this); }

    virtual PString GetFormatName() const
    { return H323PluginCapabilityInfo::GetFormatName();}
};

//////////////////////////////////////////////////////////////////////////////
//
// Class for handling G.723.1 codecs
//

class H323PluginG7231Capability : public H323AudioPluginCapability
{
  PCLASSINFO(H323PluginG7231Capability, H323AudioPluginCapability);
  public:
    H323PluginG7231Capability(PluginCodec_Definition * _encoderCodec,
                               PluginCodec_Definition * _decoderCodec,
                               BOOL _annexA = TRUE)
      : H323AudioPluginCapability(_encoderCodec, _decoderCodec, H245_AudioCapability::e_g7231),
        annexA(_annexA)
      { }

    Comparison Compare(const PObject & obj) const
    {
      if (!PIsDescendant(&obj, H323PluginG7231Capability))
        return LessThan;

      Comparison result = H323AudioCapability::Compare(obj);
      if (result != EqualTo)
        return result;

      PINDEX otherAnnexA = ((const H323PluginG7231Capability &)obj).annexA;
      if (annexA < otherAnnexA)
        return LessThan;
      if (annexA > otherAnnexA)
        return GreaterThan;
      return EqualTo;
    }

    virtual PObject * Clone() const
    { return new H323PluginG7231Capability(*this); }

    virtual BOOL OnSendingPDU(H245_AudioCapability & cap, unsigned packetSize) const
    {
      cap.SetTag(H245_AudioCapability::e_g7231);
      H245_AudioCapability_g7231 & g7231 = cap;
      g7231.m_maxAl_sduAudioFrames = packetSize;
      g7231.m_silenceSuppression = annexA;
      return TRUE;
    }

    virtual BOOL OnReceivedPDU(const H245_AudioCapability & cap,  unsigned & packetSize)
    {
      if (cap.GetTag() != H245_AudioCapability::e_g7231)
        return FALSE;
      const H245_AudioCapability_g7231 & g7231 = cap;
      packetSize = g7231.m_maxAl_sduAudioFrames;
      annexA = g7231.m_silenceSuppression;
      return TRUE;
    }

  protected:
    BOOL annexA;
};

//////////////////////////////////////////////////////////////////////////////
//
// Class for handling GSM plugin capabilities
//

class H323GSMPluginCapability : public H323AudioPluginCapability
{
  PCLASSINFO(H323GSMPluginCapability, H323AudioPluginCapability);
  public:
    H323GSMPluginCapability(PluginCodec_Definition * _encoderCodec,
                            PluginCodec_Definition * _decoderCodec,
                            int _pluginSubType, int _comfortNoise, int _scrambled)
      : H323AudioPluginCapability(_encoderCodec, _decoderCodec, _pluginSubType),
        comfortNoise(_comfortNoise), scrambled(_scrambled)
    { }

    Comparison Compare(const PObject & obj) const;

    virtual PObject * Clone() const
    { return new H323GSMPluginCapability(*this); }

    virtual BOOL OnSendingPDU(
      H245_AudioCapability & pdu,  /// PDU to set information on
      unsigned packetSize          /// Packet size to use in capability
    ) const;

    virtual BOOL OnReceivedPDU(
      const H245_AudioCapability & pdu,  /// PDU to get information from
      unsigned & packetSize              /// Packet size to use in capability
    );
  protected:
    int comfortNoise;
    int scrambled;
};

#endif // OPAL_H323

#if OPAL_VIDEO

//////////////////////////////////////////////////////////////////////////////
//
// Class for handling most audio plugin capabilities
//

class H323VideoPluginCapability : public H323VideoCapability,
                             public H323PluginCapabilityInfo
{
  PCLASSINFO(H323VideoPluginCapability, H323VideoCapability);
  public:
    H323VideoPluginCapability(PluginCodec_Definition * _encoderCodec,
                              PluginCodec_Definition * _decoderCodec,
                              unsigned _pluginSubType)
      : H323VideoCapability(), 
        H323PluginCapabilityInfo(_encoderCodec, _decoderCodec),
        pluginSubType(_pluginSubType)
      { 
      }

#if 0
    // this constructor is only used when creating a capability without a codec
    H323VideoPluginCapability(const PString & _mediaFormat, const PString & _baseName,
                         unsigned maxFramesPerPacket, unsigned /*recommendedFramesPerPacket*/,
                         unsigned _pluginSubType)
      : H323VideoCapability(), 
        H323PluginCapabilityInfo(_mediaFormat, _baseName),
        pluginSubType(_pluginSubType)
      { 
        for (PINDEX i = 0; audioMaps[i].pluginCapType >= 0; i++) {
          if (videoMaps[i].pluginCapType == (int)_pluginSubType) { 
            h323subType = audioMaps[i].h323SubType;
            break;
          }
        }
        rtpPayloadType = OpalMediaFormat(_mediaFormat).GetPayloadType();
      }
#endif

    virtual PString GetFormatName() const
    { return H323PluginCapabilityInfo::GetFormatName();}

    virtual unsigned GetSubType() const
    { return pluginSubType; }

    static BOOL SetCommonOptions(OpalMediaFormat & mediaFormat, int frameWidth, int frameHeight, int frameRate)
    {
      if (!mediaFormat.SetOptionInteger(OpalVideoFormat::FrameWidthOption, frameWidth))
        return FALSE;

      if (!mediaFormat.SetOptionInteger(OpalVideoFormat::FrameHeightOption, frameHeight))
        return FALSE;

      if (!mediaFormat.SetOptionInteger(OpalMediaFormat::FrameTimeOption, OpalMediaFormat::VideoClockRate * 100 * frameRate / 2997))
        return FALSE;

      return TRUE;
    }

  protected:
    unsigned pluginSubType;
    unsigned h323subType;   // only set if using capability without codec
};

//////////////////////////////////////////////////////////////////////////////
//
// Class for handling non standard video capabilities
//

class H323CodecPluginNonStandardVideoCapability : public H323NonStandardVideoCapability,
                                                  public H323PluginCapabilityInfo
{
  PCLASSINFO(H323CodecPluginNonStandardVideoCapability, H323NonStandardVideoCapability);
  public:
    H323CodecPluginNonStandardVideoCapability(
                                   PluginCodec_Definition * _encoderCodec,
                                   PluginCodec_Definition * _decoderCodec,
                                   H323NonStandardCapabilityInfo::CompareFuncType compareFunc,
                                   const unsigned char * data, unsigned dataLen);

    H323CodecPluginNonStandardVideoCapability(
                                   PluginCodec_Definition * _encoderCodec,
                                   PluginCodec_Definition * _decoderCodec,
                                   const unsigned char * data, unsigned dataLen);

    virtual PObject * Clone() const
    { return new H323CodecPluginNonStandardVideoCapability(*this); }

    virtual PString GetFormatName() const
    { return H323PluginCapabilityInfo::GetFormatName();}
};

//////////////////////////////////////////////////////////////////////////////
//
// Class for handling generic video capabilities
//

class H323CodecPluginGenericVideoCapability : public H323GenericVideoCapability,
					                                    public H323PluginCapabilityInfo
{
  PCLASSINFO(H323CodecPluginGenericVideoCapability, H323GenericVideoCapability);
  public:
    H323CodecPluginGenericVideoCapability(
                                   const PluginCodec_Definition * _encoderCodec,
                                   const PluginCodec_Definition * _decoderCodec,
				                 const PluginCodec_H323GenericCodecData * data );

    virtual PObject * Clone() const
    { return new H323CodecPluginGenericVideoCapability(*this); }

    virtual PString GetFormatName() const
    { return H323PluginCapabilityInfo::GetFormatName();}
};

//////////////////////////////////////////////////////////////////////////////
//
// Class for handling H.261 plugin capabilities
//

class H323H261PluginCapability : public H323VideoPluginCapability
{
  PCLASSINFO(H323H261PluginCapability, H323VideoPluginCapability);
  public:
    H323H261PluginCapability(PluginCodec_Definition * _encoderCodec,
                             PluginCodec_Definition * _decoderCodec);

    Comparison Compare(const PObject & obj) const;

    virtual PObject * Clone() const
    { 
      return new H323H261PluginCapability(*this); 
    }

    virtual BOOL OnSendingPDU(
      H245_VideoCapability & pdu  /// PDU to set information on
    ) const;

    virtual BOOL OnSendingPDU(
      H245_VideoMode & pdu
    ) const;

    virtual BOOL OnReceivedPDU(
      const H245_VideoCapability & pdu  /// PDU to get information from
    );
};

//////////////////////////////////////////////////////////////////////////////
//
// Class for handling H.263 plugin capabilities
//

class H323H263PluginCapability : public H323VideoPluginCapability
{
  PCLASSINFO(H323H263PluginCapability, H323VideoPluginCapability);
  public:
    H323H263PluginCapability(PluginCodec_Definition * _encoderCodec,
                             PluginCodec_Definition * _decoderCodec);

    Comparison Compare(const PObject & obj) const;

    virtual PObject * Clone() const
    { return new H323H263PluginCapability(*this); }

    virtual BOOL OnSendingPDU(
      H245_VideoCapability & pdu  /// PDU to set information on
    ) const;

    virtual BOOL OnSendingPDU(
      H245_VideoMode & pdu
    ) const;

    virtual BOOL OnReceivedPDU(
      const H245_VideoCapability & pdu  /// PDU to get information from
    );
};

/////////////////////////////////////////////////////////////////////////////

#endif //  OPAL_VIDEO

#endif // OPAL_H323


/////////////////////////////////////////////////////////////////////////////

/*
class H323StaticPluginCodec
{
  public:
    virtual ~H323StaticPluginCodec() { }
    virtual PluginCodec_GetAPIVersionFunction Get_GetAPIFn() = 0;
    virtual PluginCodec_GetCodecFunction Get_GetCodecFn() = 0;
};
*/

PMutex & OpalPluginCodecManager::GetMediaFormatMutex()
{
  static PMutex mutex;
  return mutex;
}

OpalPluginCodecManager::OpalPluginCodecManager(PPluginManager * _pluginMgr)
 : PPluginModuleManager(PLUGIN_CODEC_GET_CODEC_FN_STR, _pluginMgr)
{
  // instantiate all of the media formats
  {
    OpalMediaFormatFactory::KeyList_T keyList = OpalMediaFormatFactory::GetKeyList();
    OpalMediaFormatFactory::KeyList_T::const_iterator r;
    for (r = keyList.begin(); r != keyList.end(); ++r) {
      OpalMediaFormat * instance = OpalMediaFormatFactory::CreateInstance(*r);
      if (instance == NULL) {
        PTRACE(4, "H323PLUGIN\tCannot instantiate opal media format " << *r);
      } else {
        PTRACE(4, "H323PLUGIN\tCreating media format " << *r);
      }
    }
  }

  // instantiate all of the static codecs
  {
    H323StaticPluginCodecFactory::KeyList_T keyList = H323StaticPluginCodecFactory::GetKeyList();
    H323StaticPluginCodecFactory::KeyList_T::const_iterator r;
    for (r = keyList.begin(); r != keyList.end(); ++r) {
      H323StaticPluginCodec * instance = PFactory<H323StaticPluginCodec>::CreateInstance(*r);
      if (instance == NULL) {
        PTRACE(4, "H323PLUGIN\tCannot instantiate static codec plugin " << *r);
      } else {
        PTRACE(4, "H323PLUGIN\tLoading static codec plugin " << *r);
        RegisterStaticCodec(*r, instance->Get_GetAPIFn(), instance->Get_GetCodecFn());
      }
    }
  }

  // cause the plugin manager to load all dynamic plugins
  pluginMgr->AddNotifier(PCREATE_NOTIFIER(OnLoadModule), TRUE);
}

OpalPluginCodecManager::~OpalPluginCodecManager()
{
}

void OpalPluginCodecManager::OnShutdown()
{
  // unregister the plugin media formats
  OpalMediaFormatFactory::UnregisterAll();

#if 0 // OPAL_H323
  // unregister the plugin capabilities
  H323CapabilityFactory::UnregisterAll();
#endif
}

void OpalPluginCodecManager::OnLoadPlugin(PDynaLink & dll, INT code)
{
  PluginCodec_GetCodecFunction getCodecs;
  if (!dll.GetFunction(PString(signatureFunctionName), (PDynaLink::Function &)getCodecs)) {
    PTRACE(3, "H323PLUGIN\tPlugin Codec DLL " << dll.GetName() << " is not a plugin codec");
    return;
  }

  unsigned int count;
  PluginCodec_Definition * codecs = (*getCodecs)(&count, PLUGIN_CODEC_VERSION_VIDEO);
  if (codecs == NULL || count == 0) {
    PTRACE(3, "H323PLUGIN\tPlugin Codec DLL " << dll.GetName() << " contains no codec definitions");
    return;
  } 

  PTRACE(3, "H323PLUGIN\tLoading plugin codec " << dll.GetName());

  switch (code) {

    // plugin loaded
    case 0:
      RegisterCodecPlugins(count, codecs);
      break;

    // plugin unloaded
    case 1:
      UnregisterCodecPlugins(count, codecs);
      break;

    default:
      break;
  }
}

void OpalPluginCodecManager::RegisterStaticCodec(
      const H323StaticPluginCodecFactory::Key_T & PTRACE_PARAM(name),
      PluginCodec_GetAPIVersionFunction /*getApiVerFn*/,
      PluginCodec_GetCodecFunction getCodecFn)
{
  unsigned int count;
  PluginCodec_Definition * codecs = (*getCodecFn)(&count, PLUGIN_CODEC_VERSION);
  if (codecs == NULL || count == 0) {
    PTRACE(3, "H323PLUGIN\tStatic codec " << name << " contains no codec definitions");
    return;
  } 

  RegisterCodecPlugins(count, codecs);
}

void OpalPluginCodecManager::RegisterCodecPlugins(unsigned int count, void * _codecList)
{
  // make sure all non-timestamped codecs have the same concept of "now"
  static time_t codecNow = ::time(NULL);

  PluginCodec_Definition * codecList = (PluginCodec_Definition *)_codecList;
  unsigned i, j ;
  for (i = 0; i < count; i++) {

    PluginCodec_Definition & encoder = codecList[i];

    BOOL videoSupported = encoder.version >= PLUGIN_CODEC_VERSION_VIDEO;

    // for every encoder, we need a decoder
    BOOL found = FALSE;
    BOOL isEncoder = FALSE;
    if (encoder.h323CapabilityType != PluginCodec_H323Codec_undefined &&
         (
           ((encoder.flags & PluginCodec_MediaTypeMask) == PluginCodec_MediaTypeAudio) && 
            strcmp(encoder.sourceFormat, "L16") == 0
         ) ||
         (
           ((encoder.flags & PluginCodec_MediaTypeMask) == PluginCodec_MediaTypeAudioStreamed) && 
            strcmp(encoder.sourceFormat, "L16") == 0
         ) ||
         (
           videoSupported &&
           ((encoder.flags & PluginCodec_MediaTypeMask) == PluginCodec_MediaTypeVideo) && 
           strcmp(encoder.sourceFormat, "YUV420P") == 0
        )
       ) {
      isEncoder = TRUE;
      for (j = 0; j < count; j++) {

        PluginCodec_Definition & decoder = codecList[j];
        if (
            (decoder.h323CapabilityType == encoder.h323CapabilityType) &&
            ((decoder.flags & PluginCodec_MediaTypeMask) == (encoder.flags & PluginCodec_MediaTypeMask)) &&
            (strcmp(decoder.sourceFormat, encoder.destFormat) == 0) &&
            (strcmp(decoder.destFormat,   encoder.sourceFormat) == 0)
            )
          { 

          // deal with codec having no info, or timestamp in future
          time_t timeStamp = codecList[i].info == NULL ? codecNow : codecList[i].info->timestamp;
          if (timeStamp > codecNow)
            timeStamp = codecNow;

          // create the media format, transcoder and capability associated with this plugin
          RegisterPluginPair(&encoder, &decoder);
          found = TRUE;

          PTRACE(2, "H323PLUGIN\tPlugin codec " << encoder.descr << " defined");
          break;
        }
      }
    }
    if (!found && isEncoder) {
      PTRACE(2, "H323PLUGIN\tCannot find decoder for plugin encoder " << encoder.descr);
    }
  }
}

void OpalPluginCodecManager::UnregisterCodecPlugins(unsigned int /*count*/, void * /*codec*/)
{
}

OpalMediaFormatList & OpalPluginCodecManager::GetMediaFormatList()
{
  static OpalMediaFormatList mediaFormatList;
  return mediaFormatList;
}

OpalMediaFormatList OpalPluginCodecManager::GetMediaFormats()
{
  PWaitAndSignal m(GetMediaFormatMutex());
  return GetMediaFormatList();
}

void OpalPluginCodecManager::RegisterPluginPair(
       PluginCodec_Definition * encoderCodec,
       PluginCodec_Definition * decoderCodec
) 
{
  // make sure all non-timestamped codecs have the same concept of "now"
  static time_t mediaNow = time(NULL);

  // deal with codec having no info, or timestamp in future
  time_t timeStamp = encoderCodec->info == NULL ? mediaNow : encoderCodec->info->timestamp;
  if (timeStamp > mediaNow)
    timeStamp = mediaNow;

  unsigned defaultSessionID = 0;
  unsigned frameTime = 0;
  unsigned clockRate = 0;
  switch (encoderCodec->flags & PluginCodec_MediaTypeMask) {
#if OPAL_VIDEO
    case PluginCodec_MediaTypeVideo:
      defaultSessionID = OpalMediaFormat::DefaultVideoSessionID;
      break;
#endif
#if OPAL_AUDIO
    case PluginCodec_MediaTypeAudio:
    case PluginCodec_MediaTypeAudioStreamed:
      defaultSessionID = OpalMediaFormat::DefaultAudioSessionID;
      frameTime = (8 * encoderCodec->nsPerFrame) / 1000;
      clockRate = encoderCodec->sampleRate;
      break;
#endif
    default:
      break;
  }

  // add the media format
  if (defaultSessionID == 0) {
    PTRACE(3, "H323PLUGIN\tCodec DLL provides unknown media format " << (int)(encoderCodec->flags & PluginCodec_MediaTypeMask));
  } else {
    PString fmtName = CreateCodecName(encoderCodec, FALSE);
    OpalMediaFormat existingFormat(fmtName);
    if (existingFormat.IsValid()) {
      PTRACE(3, "H323PLUGIN\tMedia format " << fmtName << " already exists");
      AddFormat(existingFormat);
    } else {
      PTRACE(3, "H323PLUGIN\tCreating new media format" << fmtName);

      OpalMediaFormat * mediaFormat = NULL;

      // manually register the new singleton type, as we do not have a concrete type
      switch (encoderCodec->flags & PluginCodec_MediaTypeMask) {
#if OPAL_VIDEO
        case PluginCodec_MediaTypeVideo:
          mediaFormat = new OpalPluginVideoMediaFormat(
                                  encoderCodec, 
                                  encoderCodec->sdpFormat,
                                  timeStamp);
          break;
#endif
#if OPAL_AUDIO
        case PluginCodec_MediaTypeAudio:
        case PluginCodec_MediaTypeAudioStreamed:
          mediaFormat = new OpalPluginAudioMediaFormat(
                                   encoderCodec,
                                   encoderCodec->sdpFormat,
                                   frameTime,
                                   clockRate,
                                   timeStamp);
          break;
#endif
        default:
          break;
      }
      // if the codec has been flagged to use a shared RTP payload type, then find a codec with the same SDP name
      // and clock rate and use that RTP code rather than creating a new one. That prevents codecs (like Speex) from 
      // consuming dozens of dynamic RTP types
      if ((encoderCodec->flags & PluginCodec_RTPTypeShared) != 0 && (encoderCodec->sdpFormat != NULL)) {
        PWaitAndSignal m(OpalPluginCodecManager::GetMediaFormatMutex());
        OpalMediaFormatList & list = OpalPluginCodecManager::GetMediaFormatList();
        for (PINDEX i = 0; i < list.GetSize(); i++) {
          OpalMediaFormat * opalFmt = &list[i];
#if OPAL_AUDIO
          {
            OpalPluginAudioMediaFormat * fmt = dynamic_cast<OpalPluginAudioMediaFormat *>(opalFmt);
            if (
                (fmt != NULL) && 
                (encoderCodec->sampleRate == fmt->encoderCodec->sampleRate) &&
                (fmt->encoderCodec->sdpFormat != NULL) &&
                (strcasecmp(encoderCodec->sdpFormat, fmt->encoderCodec->sdpFormat) == 0)
                ) {
              mediaFormat->rtpPayloadType = fmt->GetPayloadType();
              break;
            }
          }
#endif
#if OPAL_VIDEO
          {
            OpalPluginVideoMediaFormat * fmt = dynamic_cast<OpalPluginVideoMediaFormat *>(opalFmt);
            if (
                (fmt != NULL) && 
                (encoderCodec->sampleRate == fmt->encoderCodec->sampleRate) &&
                (fmt->encoderCodec->sdpFormat != NULL) &&
                (strcasecmp(encoderCodec->sdpFormat, fmt->encoderCodec->sdpFormat) == 0)
                ) {
              mediaFormat->rtpPayloadType = fmt->GetPayloadType();
              break;
            }
          }
#endif
        }
      }

      // save the format
      AddFormat(*mediaFormat);

      // this looks like a memory leak, but it isn't
      //delete mediaFormat;
    }
  }

  // Create transcoder factories for the codecs
  switch (encoderCodec->flags & PluginCodec_MediaTypeMask) {
#if OPAL_VIDEO
    case PluginCodec_MediaTypeVideo:
      new OpalPluginTranscoderFactory<OpalPluginVideoTranscoder>::Worker(OpalMediaFormatPair(OpalYUV420P,                encoderCodec->destFormat), encoderCodec, TRUE);
      new OpalPluginTranscoderFactory<OpalPluginVideoTranscoder>::Worker(OpalMediaFormatPair(encoderCodec->destFormat, OpalYUV420P),                decoderCodec, FALSE);
      break;
#endif
#if OPAL_AUDIO
    case PluginCodec_MediaTypeAudio:
      new OpalPluginTranscoderFactory<OpalPluginFramedAudioTranscoder>::Worker(OpalMediaFormatPair("PCM-16",                 encoderCodec->destFormat), encoderCodec, TRUE);
      new OpalPluginTranscoderFactory<OpalPluginFramedAudioTranscoder>::Worker(OpalMediaFormatPair(encoderCodec->destFormat, OpalPCM16),                 decoderCodec, FALSE);
      break;
    case PluginCodec_MediaTypeAudioStreamed:
      new OpalPluginTranscoderFactory<OpalPluginStreamedAudioEncoder>::Worker(OpalMediaFormatPair("PCM-16",                 encoderCodec->destFormat), encoderCodec, TRUE);
      new OpalPluginTranscoderFactory<OpalPluginStreamedAudioDecoder>::Worker(OpalMediaFormatPair(encoderCodec->destFormat, OpalPCM16),                 decoderCodec, FALSE);
      break;
#endif
    default:
      break;
  }

  int retVal;
  unsigned int parmLen = sizeof(const char *);
  BOOL hasCodecControl = CallCodecControl(encoderCodec, NULL, "valid_for_protocol", (void *)"h323", &parmLen, retVal);
  if (encoderCodec->h323CapabilityType == PluginCodec_H323Codec_NoH323 || 
      (hasCodecControl && (retVal == 0))
       ) {
    PTRACE(3, "H323PLUGIN\tNot adding H.323 capability for plugin codec " << encoderCodec->destFormat << " as this has been specifically disabled");
    return;
  }

#if OPAL_H323

  // add the capability
  H323CodecPluginCapabilityMapEntry * map = NULL;

  switch (encoderCodec->flags & PluginCodec_MediaTypeMask) {
#if OPAL_AUDIO
    case PluginCodec_MediaTypeAudio:
    case PluginCodec_MediaTypeAudioStreamed:
      map = audioMaps;
      break;
#endif // OPAL_AUDIO

#if OPAL_VIDEO
    case PluginCodec_MediaTypeVideo:
      map = videoMaps;
      break;
#endif // OPAL_VIDEO

    default:
      break;
  }

  if (map == NULL) {
    PTRACE(3, "H323PLUGIN\tCannot create capability for unknown plugin codec media format " << (int)(encoderCodec->flags & PluginCodec_MediaTypeMask));
  } 
  else if (encoderCodec->h323CapabilityType != PluginCodec_H323Codec_undefined) {
    for (PINDEX i = 0; map[i].pluginCapType >= 0; i++) {
      if (map[i].pluginCapType == encoderCodec->h323CapabilityType) {
        H323Capability * cap = NULL;
        if (map[i].createFunc != NULL)
          cap = (*map[i].createFunc)(encoderCodec, decoderCodec, map[i].h323SubType);
        else {
          switch (encoderCodec->flags & PluginCodec_MediaTypeMask) {
#if OPAL_AUDIO
            case PluginCodec_MediaTypeAudio:
            case PluginCodec_MediaTypeAudioStreamed:
              cap = new H323AudioPluginCapability(encoderCodec, decoderCodec, map[i].h323SubType);
              break;
#endif // OPAL_AUDIO

#if OPAL_VIDEO
            case PluginCodec_MediaTypeVideo:
              // all video caps are created using the create functions
              break;
#endif // OPAL_VIDEO

            default:
              break;
          }
        }

        // manually register the new singleton type, as we do not have a concrete type
        if (cap != NULL)
          H323CapabilityFactory::Register(CreateCodecName(encoderCodec, TRUE), cap);
        break;
      }
    }
  }
#endif // OPAL_H323
}

void OpalPluginCodecManager::AddFormat(const OpalMediaFormat & fmt)
{
  PWaitAndSignal m(GetMediaFormatMutex());
  GetMediaFormatList() += fmt;
}


/////////////////////////////////////////////////////////////////////////////

#if OPAL_H323

/*
H323Capability * OpalPluginCodecManager::CreateCapability(
          const PString & _mediaFormat, 
          const PString & _baseName,
                 unsigned maxFramesPerPacket, 
                 unsigned recommendedFramesPerPacket,
                 unsigned _pluginSubType
  )
{
  return new H323PluginCapability(_mediaFormat, _baseName,
                                  maxFramesPerPacket, recommendedFramesPerPacket, _pluginSubType);
}
*/

#if OPAL_AUDIO

H323Capability * CreateNonStandardAudioCap(
  PluginCodec_Definition * encoderCodec,  
  PluginCodec_Definition * decoderCodec,
  int /*subType*/) 
{
  PluginCodec_H323NonStandardCodecData * pluginData =  (PluginCodec_H323NonStandardCodecData *)encoderCodec->h323CapabilityData;
  if (pluginData == NULL) {
    return new H323CodecPluginNonStandardAudioCapability(
                             encoderCodec, decoderCodec,
                             (const unsigned char *)encoderCodec->descr, 
                             strlen(encoderCodec->descr));
  }

  else if (pluginData->capabilityMatchFunction != NULL) 
    return new H323CodecPluginNonStandardAudioCapability(encoderCodec, decoderCodec,
                             (H323NonStandardCapabilityInfo::CompareFuncType)pluginData->capabilityMatchFunction,
                             pluginData->data, pluginData->dataLength);
  else
    return new H323CodecPluginNonStandardAudioCapability(
                             encoderCodec, decoderCodec,
                             pluginData->data, pluginData->dataLength);
}

H323Capability *CreateGenericAudioCap(
  PluginCodec_Definition * encoderCodec,  
  PluginCodec_Definition * decoderCodec,
  int /*subType*/) 
{
    PluginCodec_H323GenericCodecData * pluginData = (PluginCodec_H323GenericCodecData *)encoderCodec->h323CapabilityData;

    if(pluginData == NULL ) {
	PTRACE(1, "Generic codec information for codec '"<<encoderCodec->descr<<"' has NULL data field");
	return NULL;
    }
    return new H323CodecPluginGenericAudioCapability(encoderCodec, decoderCodec, pluginData);
}

H323Capability * CreateG7231Cap(
  PluginCodec_Definition * encoderCodec,  
  PluginCodec_Definition * decoderCodec,
  int /*subType*/) 
{
  return new H323PluginG7231Capability(encoderCodec, decoderCodec, decoderCodec->h323CapabilityData != 0);
}


H323Capability * CreateGSMCap(
  PluginCodec_Definition * encoderCodec,  
  PluginCodec_Definition * decoderCodec,
  int subType) 
{
  PluginCodec_H323AudioGSMData * pluginData =  (PluginCodec_H323AudioGSMData *)encoderCodec->h323CapabilityData;
  return new H323GSMPluginCapability(encoderCodec, decoderCodec, subType, pluginData->comfortNoise, pluginData->scrambled);
}

#endif // OPAL_AUDIO

#if OPAL_VIDEO

H323Capability * CreateNonStandardVideoCap(
  PluginCodec_Definition * encoderCodec,  
  PluginCodec_Definition * decoderCodec,
  int /*subType*/) 
{
  PluginCodec_H323NonStandardCodecData * pluginData =  (PluginCodec_H323NonStandardCodecData *)encoderCodec->h323CapabilityData;
  if (pluginData == NULL) {
    return new H323CodecPluginNonStandardVideoCapability(
                             encoderCodec, decoderCodec,
                             (const unsigned char *)encoderCodec->descr, 
                             strlen(encoderCodec->descr));
  }

  else if (pluginData->capabilityMatchFunction != NULL) 
    return new H323CodecPluginNonStandardVideoCapability(encoderCodec, decoderCodec,
                             (H323NonStandardCapabilityInfo::CompareFuncType)pluginData->capabilityMatchFunction,
                             pluginData->data, pluginData->dataLength);
  else
    return new H323CodecPluginNonStandardVideoCapability(
                             encoderCodec, decoderCodec,
                             pluginData->data, pluginData->dataLength);
}

H323Capability *CreateGenericVideoCap(
  PluginCodec_Definition * encoderCodec,  
  PluginCodec_Definition * decoderCodec,
  int /*subType*/) 
{
  PluginCodec_H323GenericCodecData * pluginData = (PluginCodec_H323GenericCodecData *)encoderCodec->h323CapabilityData;

  if (pluginData == NULL ) {
	  PTRACE(1, "Generic codec information for codec '"<<encoderCodec->descr<<"' has NULL data field");
	  return NULL;
  }
  return new H323CodecPluginGenericVideoCapability(encoderCodec, decoderCodec, pluginData);
}


H323Capability * CreateH261Cap(
  PluginCodec_Definition * encoderCodec, 
  PluginCodec_Definition * decoderCodec,
  int /*subType*/) 
{
  return new H323H261PluginCapability(encoderCodec, decoderCodec);
}

H323Capability * CreateH263Cap(
  PluginCodec_Definition * encoderCodec, 
  PluginCodec_Definition * decoderCodec,
  int /*subType*/) 
{
  return new H323H263PluginCapability(encoderCodec, decoderCodec);
}

#endif // OPAL_VIDEO

/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////

H323PluginCapabilityInfo::H323PluginCapabilityInfo(PluginCodec_Definition * _encoderCodec,
                                                   PluginCodec_Definition * _decoderCodec)
 : encoderCodec(_encoderCodec),
   decoderCodec(_decoderCodec),
   capabilityFormatName(CreateCodecName(_encoderCodec, TRUE))
{
}

H323PluginCapabilityInfo::H323PluginCapabilityInfo(const PString & _baseName)
 : encoderCodec(NULL),
   decoderCodec(NULL),
   capabilityFormatName(CreateCodecName(_baseName, TRUE))
{
}

#if OPAL_AUDIO

/////////////////////////////////////////////////////////////////////////////

H323CodecPluginNonStandardAudioCapability::H323CodecPluginNonStandardAudioCapability(
    PluginCodec_Definition * _encoderCodec,
    PluginCodec_Definition * _decoderCodec,
    H323NonStandardCapabilityInfo::CompareFuncType compareFunc,
    const unsigned char * data, unsigned dataLen)
 : H323NonStandardAudioCapability(compareFunc,data, dataLen), 
   H323PluginCapabilityInfo(_encoderCodec, _decoderCodec)
{
  SetTxFramesInPacket(_decoderCodec->maxFramesPerPacket);

  PluginCodec_H323NonStandardCodecData * nonStdData = (PluginCodec_H323NonStandardCodecData *)_encoderCodec->h323CapabilityData;
  if (nonStdData->objectId != NULL) {
    oid = PString(nonStdData->objectId);
  } else {
    t35CountryCode   = nonStdData->t35CountryCode;
    t35Extension     = nonStdData->t35Extension;
    manufacturerCode = nonStdData->manufacturerCode;
  }
}

H323CodecPluginNonStandardAudioCapability::H323CodecPluginNonStandardAudioCapability(
    PluginCodec_Definition * _encoderCodec,
    PluginCodec_Definition * _decoderCodec,
    const unsigned char * data, unsigned dataLen)
 : H323NonStandardAudioCapability(data, dataLen), 
   H323PluginCapabilityInfo(_encoderCodec, _decoderCodec)
{
  SetTxFramesInPacket(_decoderCodec->maxFramesPerPacket);
  PluginCodec_H323NonStandardCodecData * nonStdData = (PluginCodec_H323NonStandardCodecData *)_encoderCodec->h323CapabilityData;
  if (nonStdData->objectId != NULL) {
    oid = PString(nonStdData->objectId);
  } else {
    t35CountryCode   = nonStdData->t35CountryCode;
    t35Extension     = nonStdData->t35Extension;
    manufacturerCode = nonStdData->manufacturerCode;
  }
}

/////////////////////////////////////////////////////////////////////////////

H323CodecPluginGenericAudioCapability::H323CodecPluginGenericAudioCapability(
    const PluginCodec_Definition * _encoderCodec,
    const PluginCodec_Definition * _decoderCodec,
    const PluginCodec_H323GenericCodecData *data )
	: H323GenericAudioCapability(data -> standardIdentifier, data -> maxBitRate),
	  H323PluginCapabilityInfo((PluginCodec_Definition *)_encoderCodec, (PluginCodec_Definition *) _decoderCodec)
{
  SetTxFramesInPacket(_decoderCodec->maxFramesPerPacket);
  const PluginCodec_H323GenericParameterDefinition *ptr = data -> params;

  unsigned i;
  for (i = 0; i < data -> nParameters; i++) {
    switch(ptr->type) {
	    case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_ShortMin:
	    case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_ShortMax:
	    case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_LongMin:
	    case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_LongMax:
	      AddIntegerGenericParameter(ptr->collapsing,ptr->id,ptr->type, ptr->value.integer);
	      break;
	
	    case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_Logical:
	    case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_Bitfield:
	    case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_OctetString:
	    case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_GenericParameter:
	    default:
	      PTRACE(1,"Unsupported Generic parameter type "<< ptr->type << " for generic codec " << _encoderCodec->descr );
	      break;
    }
    ptr++;
  }
}

/////////////////////////////////////////////////////////////////////////////

PObject::Comparison H323GSMPluginCapability::Compare(const PObject & obj) const
{
  if (!PIsDescendant(&obj, H323GSMPluginCapability))
    return LessThan;

  Comparison result = H323AudioCapability::Compare(obj);
  if (result != EqualTo)
    return result;

  const H323GSMPluginCapability& other = (const H323GSMPluginCapability&)obj;
  if (scrambled < other.scrambled)
    return LessThan;
  if (comfortNoise < other.comfortNoise)
    return LessThan;
  return EqualTo;
}


BOOL H323GSMPluginCapability::OnSendingPDU(H245_AudioCapability & cap, unsigned packetSize) const
{
  cap.SetTag(pluginSubType);
  H245_GSMAudioCapability & gsm = cap;
  gsm.m_audioUnitSize = packetSize * encoderCodec->bytesPerFrame;
  gsm.m_comfortNoise  = comfortNoise;
  gsm.m_scrambled     = scrambled;

  return TRUE;
}


BOOL H323GSMPluginCapability::OnReceivedPDU(const H245_AudioCapability & cap, unsigned & packetSize)
{
  const H245_GSMAudioCapability & gsm = cap;
  packetSize   = gsm.m_audioUnitSize / encoderCodec->bytesPerFrame;
  if (packetSize == 0)
    packetSize = 1;

  scrambled    = gsm.m_scrambled;
  comfortNoise = gsm.m_comfortNoise;

  return TRUE;
}

/////////////////////////////////////////////////////////////////////////////

#endif   // OPAL_AUDIO

#if OPAL_VIDEO

/////////////////////////////////////////////////////////////////////////////

H323H261PluginCapability::H323H261PluginCapability(PluginCodec_Definition * _encoderCodec,
                                                   PluginCodec_Definition * _decoderCodec)
  : H323VideoPluginCapability(_encoderCodec, _decoderCodec, H245_VideoCapability::e_h261VideoCapability)
{ 
  const OpalMediaFormat & fmt = GetMediaFormat();
  if (!fmt.HasOption(h323_qcifMPI_tag) && !fmt.HasOption(h323_cifMPI_tag)) {
    OpalMediaFormat & mediaFormat = GetWritableMediaFormat();
    mediaFormat.AddOption(new OpalMediaOptionInteger(h323_cifMPI_tag,                    false, OpalMediaOption::MinMerge, 4));
    mediaFormat.AddOption(new OpalMediaOptionInteger(OpalMediaFormat::MaxBitRateOption,  false, OpalMediaOption::MinMerge, 621700));
  }
}

PObject::Comparison H323H261PluginCapability::Compare(const PObject & obj) const
{
  if (!PIsDescendant(&obj, H323H261PluginCapability))
    return LessThan;

  Comparison result = H323Capability::Compare(obj);
  if (result != EqualTo)
    return result;

  const H323H261PluginCapability & other = (const H323H261PluginCapability &)obj;

  const OpalMediaFormat & mediaFormat = GetMediaFormat();

  int qcifMPI = mediaFormat.GetOptionInteger(h323_qcifMPI_tag);
  int cifMPI = mediaFormat.GetOptionInteger(h323_cifMPI_tag);

  int other_qcifMPI = other.GetMediaFormat().GetOptionInteger(h323_qcifMPI_tag);
  int other_cifMPI = other.GetMediaFormat().GetOptionInteger(h323_cifMPI_tag);

  if (((qcifMPI > 0) && (other_qcifMPI > 0)) ||
      ((cifMPI  > 0) && (other_cifMPI > 0)))
    return EqualTo;

  if (qcifMPI > 0)
    return LessThan;

  return GreaterThan;
}


BOOL H323H261PluginCapability::OnSendingPDU(H245_VideoCapability & cap) const
{
  cap.SetTag(H245_VideoCapability::e_h261VideoCapability);

  H245_H261VideoCapability & h261 = cap;

  const OpalMediaFormat & mediaFormat = GetMediaFormat();

  /*
#if PTRACING
  ostream & traceStream = PTrace::Begin(4, __FILE__, __LINE__);
  traceStream << "H.261 extracting options from\n";
  for (PINDEX i = 0; i < mediaFormat.GetOptionCount(); i++) {
    const OpalMediaOption & option = mediaFormat.GetOption(i);
    traceStream << "         " << option.GetName() << " = " << option.AsString() << '\n';
  }
  traceStream << PTrace::End;
#endif
  */

  int qcifMPI = mediaFormat.GetOptionInteger(h323_qcifMPI_tag, 0);
  if (qcifMPI > 0) {
    h261.IncludeOptionalField(H245_H261VideoCapability::e_qcifMPI);
    h261.m_qcifMPI = qcifMPI;
  }
  int cifMPI = mediaFormat.GetOptionInteger(h323_cifMPI_tag);
  if (cifMPI > 0 || qcifMPI == 0) {
    h261.IncludeOptionalField(H245_H261VideoCapability::e_cifMPI);
    h261.m_cifMPI = cifMPI;
  }

  h261.m_temporalSpatialTradeOffCapability = mediaFormat.GetOptionBoolean(h323_temporalSpatialTradeOffCapability_tag, FALSE);
  h261.m_maxBitRate                        = (mediaFormat.GetOptionInteger(OpalMediaFormat::MaxBitRateOption, 621700)+50)/100;
  h261.m_stillImageTransmission            = mediaFormat.GetOptionBoolean(h323_stillImageTransmission_tag, FALSE);

  return TRUE;
}


BOOL H323H261PluginCapability::OnSendingPDU(H245_VideoMode & pdu) const
{
  pdu.SetTag(H245_VideoMode::e_h261VideoMode);
  H245_H261VideoMode & mode = pdu;

  const OpalMediaFormat & mediaFormat = GetMediaFormat();

  int qcifMPI = mediaFormat.GetOptionInteger(h323_qcifMPI_tag);

  mode.m_resolution.SetTag(qcifMPI > 0 ? H245_H261VideoMode_resolution::e_qcif
                                       : H245_H261VideoMode_resolution::e_cif);

  mode.m_bitRate                = (mediaFormat.GetOptionInteger(OpalMediaFormat::MaxBitRateOption, 621700) + 50) / 1000;
  mode.m_stillImageTransmission = mediaFormat.GetOptionBoolean(h323_stillImageTransmission_tag, FALSE);

  return TRUE;
}

BOOL H323H261PluginCapability::OnReceivedPDU(const H245_VideoCapability & cap)
{
  if (cap.GetTag() != H245_VideoCapability::e_h261VideoCapability)
    return FALSE;

  OpalMediaFormat & mediaFormat = GetWritableMediaFormat();

  const H245_H261VideoCapability & h261 = cap;
  if (h261.HasOptionalField(H245_H261VideoCapability::e_qcifMPI)) {

    if (!mediaFormat.SetOptionInteger(h323_qcifMPI_tag, h261.m_qcifMPI))
      return FALSE;

    if (!SetCommonOptions(mediaFormat, QCIF_WIDTH, QCIF_HEIGHT, h261.m_qcifMPI))
      return FALSE;
  }

  if (h261.HasOptionalField(H245_H261VideoCapability::e_cifMPI)) {

    if (!mediaFormat.SetOptionInteger(h323_cifMPI_tag, h261.m_cifMPI))
      return FALSE;

    if (!SetCommonOptions(mediaFormat, QCIF_WIDTH, QCIF_HEIGHT, h261.m_cifMPI))
      return FALSE;
  }

  mediaFormat.SetOptionInteger(OpalMediaFormat::MaxBitRateOption,          h261.m_maxBitRate*100);
  mediaFormat.SetOptionBoolean(h323_temporalSpatialTradeOffCapability_tag, h261.m_temporalSpatialTradeOffCapability);
  mediaFormat.SetOptionBoolean(h323_stillImageTransmission_tag,            h261.m_stillImageTransmission);

  return TRUE;
}

/////////////////////////////////////////////////////////////////////////////

H323H263PluginCapability::H323H263PluginCapability(PluginCodec_Definition * _encoderCodec,
                                                   PluginCodec_Definition * _decoderCodec)
  : H323VideoPluginCapability(_encoderCodec, _decoderCodec, H245_VideoCapability::e_h263VideoCapability)
{ 
}

PObject::Comparison H323H263PluginCapability::Compare(const PObject & obj) const
{
  if (!PIsDescendant(&obj, H323H263PluginCapability))
    return LessThan;

  Comparison result = H323Capability::Compare(obj);
  if (result != EqualTo)
    return result;

  const H323H263PluginCapability & other = (const H323H263PluginCapability &)obj;

  const OpalMediaFormat & mediaFormat = GetMediaFormat();

  int sqcifMPI = mediaFormat.GetOptionInteger(h323_sqcifMPI_tag);
  int qcifMPI  = mediaFormat.GetOptionInteger(h323_qcifMPI_tag);
  int cifMPI   = mediaFormat.GetOptionInteger(h323_cifMPI_tag);
  int cif4MPI  = mediaFormat.GetOptionInteger(h323_cif4MPI_tag);
  int cif16MPI = mediaFormat.GetOptionInteger(h323_cif16MPI_tag);

  int other_sqcifMPI = other.GetMediaFormat().GetOptionInteger(h323_sqcifMPI_tag);
  int other_qcifMPI  = other.GetMediaFormat().GetOptionInteger(h323_qcifMPI_tag);
  int other_cifMPI   = other.GetMediaFormat().GetOptionInteger(h323_cifMPI_tag);
  int other_cif4MPI  = other.GetMediaFormat().GetOptionInteger(h323_cif4MPI_tag);
  int other_cif16MPI = other.GetMediaFormat().GetOptionInteger(h323_cif16MPI_tag);

  if ((sqcifMPI && other_sqcifMPI) ||
      (qcifMPI && other_qcifMPI) ||
      (cifMPI && other_cifMPI) ||
      (cif4MPI && other_cif4MPI) ||
      (cif16MPI && other_cif16MPI))
    return EqualTo;

  if ((!cif16MPI && other_cif16MPI) ||
      (!cif4MPI && other_cif4MPI) ||
      (!cifMPI && other_cifMPI) ||
      (!qcifMPI && other_qcifMPI) ||
      (!sqcifMPI && other_sqcifMPI))
    return LessThan;

  return GreaterThan;
}

static void SetTransmittedCap(const OpalMediaFormat & mediaFormat,
                              H245_H263VideoCapability & h263,
                              const char * mpiTag,
                              int mpiEnum,
                              PASN_Integer & mpi,
                              int slowMpiEnum,
                              PASN_Integer & slowMpi)
{
  int mpiVal = mediaFormat.GetOptionInteger(mpiTag);
  if (mpiVal > 0) {
    h263.IncludeOptionalField(mpiEnum);
    mpi = mpiVal;
  }
  else if (mpiVal < 0) {
    h263.IncludeOptionalField(slowMpiEnum);
    slowMpi = -mpiVal;
  }
}


BOOL H323H263PluginCapability::OnSendingPDU(H245_VideoCapability & cap) const
{
  cap.SetTag(H245_VideoCapability::e_h263VideoCapability);
  H245_H263VideoCapability & h263 = cap;

  const OpalMediaFormat & mediaFormat = GetMediaFormat();

  SetTransmittedCap(mediaFormat, cap, h323_sqcifMPI_tag, H245_H263VideoCapability::e_sqcifMPI, h263.m_sqcifMPI, H245_H263VideoCapability::e_slowSqcifMPI, h263.m_slowSqcifMPI);
  SetTransmittedCap(mediaFormat, cap, h323_qcifMPI_tag,  H245_H263VideoCapability::e_qcifMPI,  h263.m_qcifMPI,  H245_H263VideoCapability::e_slowQcifMPI,  h263.m_slowQcifMPI);
  SetTransmittedCap(mediaFormat, cap, h323_cifMPI_tag,   H245_H263VideoCapability::e_cifMPI,   h263.m_cifMPI,   H245_H263VideoCapability::e_slowCifMPI,   h263.m_slowCifMPI);
  SetTransmittedCap(mediaFormat, cap, h323_cif4MPI_tag,  H245_H263VideoCapability::e_cif4MPI,  h263.m_cif4MPI,  H245_H263VideoCapability::e_slowCif4MPI,  h263.m_slowCif4MPI);
  SetTransmittedCap(mediaFormat, cap, h323_cif16MPI_tag, H245_H263VideoCapability::e_cif16MPI, h263.m_cif16MPI, H245_H263VideoCapability::e_slowCif16MPI, h263.m_slowCif16MPI);

  h263.m_maxBitRate                        = (mediaFormat.GetOptionInteger(OpalMediaFormat::MaxBitRateOption, 327600) + 50) / 100;
  h263.m_temporalSpatialTradeOffCapability = mediaFormat.GetOptionBoolean(h323_temporalSpatialTradeOffCapability_tag, FALSE);
  h263.m_unrestrictedVector	               = mediaFormat.GetOptionBoolean(h323_unrestrictedVector_tag, FALSE);
  h263.m_arithmeticCoding	                 = mediaFormat.GetOptionBoolean(h323_arithmeticCoding_tag, FALSE);
  h263.m_advancedPrediction	               = mediaFormat.GetOptionBoolean(h323_advancedPrediction_tag, FALSE);
  h263.m_pbFrames	                         = mediaFormat.GetOptionBoolean(h323_pbFrames_tag, FALSE);
  h263.m_errorCompensation                 = mediaFormat.GetOptionBoolean(h323_errorCompensation_tag, FALSE);

  {
    int hrdB = mediaFormat.GetOptionInteger(h323_hrdB_tag, -1);
    if (hrdB >= 0) {
      h263.IncludeOptionalField(H245_H263VideoCapability::e_hrd_B);
	    h263.m_hrd_B = hrdB;
    }
  }

  {
    int bppMaxKb = mediaFormat.GetOptionInteger(h323_bppMaxKb_tag, -1);
    if (bppMaxKb >= 0) {
      h263.IncludeOptionalField(H245_H263VideoCapability::e_bppMaxKb);
	    h263.m_bppMaxKb = bppMaxKb;
    }
  }

  return TRUE;
}


BOOL H323H263PluginCapability::OnSendingPDU(H245_VideoMode & pdu) const
{
  pdu.SetTag(H245_VideoMode::e_h263VideoMode);
  H245_H263VideoMode & mode = pdu;

  const OpalMediaFormat & mediaFormat = GetMediaFormat();

  int qcifMPI  = mediaFormat.GetOptionInteger(h323_qcifMPI_tag);
  int cifMPI   = mediaFormat.GetOptionInteger(h323_cifMPI_tag);
  int cif4MPI  = mediaFormat.GetOptionInteger(h323_cif4MPI_tag);
  int cif16MPI = mediaFormat.GetOptionInteger(h323_cif16MPI_tag);

  mode.m_resolution.SetTag(cif16MPI ? H245_H263VideoMode_resolution::e_cif16
			  :(cif4MPI ? H245_H263VideoMode_resolution::e_cif4
			   :(cifMPI ? H245_H263VideoMode_resolution::e_cif
			    :(qcifMPI ? H245_H263VideoMode_resolution::e_qcif
            : H245_H263VideoMode_resolution::e_sqcif))));

  mode.m_bitRate              = (mediaFormat.GetOptionInteger(OpalMediaFormat::MaxBitRateOption, 327600) + 50) / 100;
  mode.m_unrestrictedVector   = mediaFormat.GetOptionBoolean(h323_unrestrictedVector_tag, FALSE);
  mode.m_arithmeticCoding     = mediaFormat.GetOptionBoolean(h323_arithmeticCoding_tag, FALSE);
  mode.m_advancedPrediction   = mediaFormat.GetOptionBoolean(h323_advancedPrediction_tag, FALSE);
  mode.m_pbFrames             = mediaFormat.GetOptionBoolean(h323_pbFrames_tag, FALSE);
  mode.m_errorCompensation    = mediaFormat.GetOptionBoolean(h323_errorCompensation_tag, FALSE);

  return TRUE;
}

static BOOL SetReceivedH263Cap(OpalMediaFormat & mediaFormat, 
                               const H245_H263VideoCapability & h263, 
                               const char * mpiTag,
                               int mpiEnum,
                               const PASN_Integer & mpi,
                               int slowMpiEnum,
                               const PASN_Integer & slowMpi,
                               int frameWidth, int frameHeight,
                               BOOL & formatDefined)
{
  if (h263.HasOptionalField(mpiEnum)) {
    if (!mediaFormat.SetOptionInteger(mpiTag, mpi))
      return FALSE;
    if (!H323VideoPluginCapability::SetCommonOptions(mediaFormat, frameWidth, frameHeight, mpi))
      return FALSE;
    formatDefined = TRUE;
  } else if (h263.HasOptionalField(slowMpiEnum)) {
    if (!mediaFormat.SetOptionInteger(mpiTag, -(signed)slowMpi))
      return FALSE;
    if (!H323VideoPluginCapability::SetCommonOptions(mediaFormat, frameWidth, frameHeight, -(signed)slowMpi))
      return FALSE;
    formatDefined = TRUE;
  } 

  return TRUE;
}


BOOL H323H263PluginCapability::OnReceivedPDU(const H245_VideoCapability & cap)
{
  if (cap.GetTag() != H245_VideoCapability::e_h263VideoCapability)
    return FALSE;

  OpalMediaFormat & mediaFormat = GetWritableMediaFormat();

  BOOL formatDefined = FALSE;

  const H245_H263VideoCapability & h263 = cap;

  if (!SetReceivedH263Cap(mediaFormat, cap, h323_sqcifMPI_tag, H245_H263VideoCapability::e_sqcifMPI, h263.m_sqcifMPI, H245_H263VideoCapability::e_slowSqcifMPI, h263.m_slowSqcifMPI, SQCIF_WIDTH, SQCIF_HEIGHT, formatDefined))
    return FALSE;

  if (!SetReceivedH263Cap(mediaFormat, cap, h323_qcifMPI_tag,  H245_H263VideoCapability::e_qcifMPI,  h263.m_qcifMPI,  H245_H263VideoCapability::e_slowQcifMPI,  h263.m_slowQcifMPI,  QCIF_WIDTH,  QCIF_HEIGHT,  formatDefined))
    return FALSE;

  if (!SetReceivedH263Cap(mediaFormat, cap, h323_cifMPI_tag,   H245_H263VideoCapability::e_cifMPI,   h263.m_cifMPI,   H245_H263VideoCapability::e_slowCifMPI,   h263.m_slowCifMPI,   CIF_WIDTH,   CIF_HEIGHT,   formatDefined))
    return FALSE;

  if (!SetReceivedH263Cap(mediaFormat, cap, h323_cif4MPI_tag,  H245_H263VideoCapability::e_cif4MPI,  h263.m_cif4MPI,  H245_H263VideoCapability::e_slowCif4MPI,  h263.m_slowCif4MPI,  CIF4_WIDTH,  CIF4_HEIGHT,  formatDefined))
    return FALSE;

  if (!SetReceivedH263Cap(mediaFormat, cap, h323_cif16MPI_tag, H245_H263VideoCapability::e_cif16MPI, h263.m_cif16MPI, H245_H263VideoCapability::e_slowCif16MPI, h263.m_slowCif16MPI, CIF16_WIDTH, CIF16_HEIGHT, formatDefined))
    return FALSE;

  if (!mediaFormat.SetOptionInteger(OpalMediaFormat::MaxBitRateOption, h263.m_maxBitRate*100))
    return FALSE;

  mediaFormat.SetOptionBoolean(h323_unrestrictedVector_tag,      h263.m_unrestrictedVector);
  mediaFormat.SetOptionBoolean(h323_arithmeticCoding_tag,        h263.m_arithmeticCoding);
  mediaFormat.SetOptionBoolean(h323_advancedPrediction_tag,      h263.m_advancedPrediction);
  mediaFormat.SetOptionBoolean(h323_pbFrames_tag,                h263.m_pbFrames);
  mediaFormat.SetOptionBoolean(h323_errorCompensation_tag,       h263.m_errorCompensation);

  if (h263.HasOptionalField(H245_H263VideoCapability::e_hrd_B))
    mediaFormat.SetOptionInteger(h323_hrdB_tag, h263.m_hrd_B);

  if (h263.HasOptionalField(H245_H263VideoCapability::e_bppMaxKb))
    mediaFormat.SetOptionInteger(h323_bppMaxKb_tag, h263.m_bppMaxKb);

  return TRUE;
}

/////////////////////////////////////////////////////////////////////////////

H323CodecPluginNonStandardVideoCapability::H323CodecPluginNonStandardVideoCapability(
    PluginCodec_Definition * _encoderCodec,
    PluginCodec_Definition * _decoderCodec,
    H323NonStandardCapabilityInfo::CompareFuncType compareFunc,
    const unsigned char * data, unsigned dataLen)
 : H323NonStandardVideoCapability(compareFunc, data, dataLen), 
   H323PluginCapabilityInfo(_encoderCodec, _decoderCodec)
{
  PluginCodec_H323NonStandardCodecData * nonStdData = (PluginCodec_H323NonStandardCodecData *)_encoderCodec->h323CapabilityData;
  if (nonStdData->objectId != NULL) {
    oid = PString(nonStdData->objectId);
  } else {
    t35CountryCode   = nonStdData->t35CountryCode;
    t35Extension     = nonStdData->t35Extension;
    manufacturerCode = nonStdData->manufacturerCode;
  }
}

H323CodecPluginNonStandardVideoCapability::H323CodecPluginNonStandardVideoCapability(
    PluginCodec_Definition * _encoderCodec,
    PluginCodec_Definition * _decoderCodec,
    const unsigned char * data, unsigned dataLen)
 : H323NonStandardVideoCapability(data, dataLen), 
   H323PluginCapabilityInfo(_encoderCodec, _decoderCodec)
{
  PluginCodec_H323NonStandardCodecData * nonStdData = (PluginCodec_H323NonStandardCodecData *)_encoderCodec->h323CapabilityData;
  if (nonStdData->objectId != NULL) {
    oid = PString(nonStdData->objectId);
  } else {
    t35CountryCode   = nonStdData->t35CountryCode;
    t35Extension     = nonStdData->t35Extension;
    manufacturerCode = nonStdData->manufacturerCode;
  }
}

/////////////////////////////////////////////////////////////////////////////

H323CodecPluginGenericVideoCapability::H323CodecPluginGenericVideoCapability(
    const PluginCodec_Definition * _encoderCodec,
    const PluginCodec_Definition * _decoderCodec,
    const PluginCodec_H323GenericCodecData *data )
	: H323GenericVideoCapability(data -> standardIdentifier, data -> maxBitRate),
	  H323PluginCapabilityInfo((PluginCodec_Definition *)_encoderCodec, (PluginCodec_Definition *) _decoderCodec)
{
  const PluginCodec_H323GenericParameterDefinition *ptr = data -> params;

  unsigned i;
  for (i = 0; i < data -> nParameters; i++) {
    switch(ptr->type) {
	    case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_ShortMin:
	    case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_ShortMax:
	    case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_LongMin:
	    case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_LongMax:
	      AddIntegerGenericParameter(ptr->collapsing,ptr->id,ptr->type, ptr->value.integer);
	      break;
	
	    case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_Logical:
	    case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_Bitfield:
	    case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_OctetString:
	    case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_GenericParameter:
	    default:
	      PTRACE(1,"Unsupported Generic parameter type "<< ptr->type << " for generic codec " << _encoderCodec->descr );
	      break;
    }
    ptr++;
  }
}

/////////////////////////////////////////////////////////////////////////////

#endif  // OPAL_VIDEO

#endif // OPAL_H323

/////////////////////////////////////////////////////////////////////////////

OPALDynaLink::OPALDynaLink(const char * _baseName, const char * _reason)
  : baseName(_baseName), reason(_reason)
{
  isLoadedOK = FALSE;
}

void OPALDynaLink::Load()
{
  PStringArray dirs = PPluginManager::GetPluginDirs();
  PINDEX i;
  for (i = 0; !PDynaLink::IsLoaded() && i < dirs.GetSize(); i++)
    PLoadPluginDirectory<OPALDynaLink>(*this, dirs[i]);
  
  if (!PDynaLink::IsLoaded()) {
    cerr << "Cannot find " << baseName << " as required for " << ((reason != NULL) ? reason : " a code module") << "." << endl
         << "This function may appear to be installed, but will not operate correctly." << endl
         << "Please put the file " << baseName << PDynaLink::GetExtension() << " into one of the following directories:" << endl
         << "     " << setfill(',') << dirs << setfill(' ') << endl
         << "This list of directories can be set using the PWLIBPLUGINDIR environment variable." << endl;
    return;
  }
}

BOOL OPALDynaLink::LoadPlugin(const PString & filename)
{
  PFilePath fn = filename;
  if (fn.GetTitle() *= "libavcodec")
    return PDynaLink::Open(filename);
  return TRUE;
}

/////////////////////////////////////////////////////////////////////////////

static PAtomicInteger bootStrapCount = 0;

void OpalPluginCodecManager::Bootstrap()
{
  if (++bootStrapCount != 1)
    return;

#if defined(OPAL_AUDIO) || defined(OPAL_VIDEO)
//  OpalMediaFormatList & mediaFormatList = OpalPluginCodecManager::GetMediaFormatList();
#endif

#if OPAL_AUDIO

  //mediaFormatList += OpalMediaFormat(OpalG711uLaw);
  //mediaFormatList += OpalMediaFormat(OpalG711ALaw);

  //new OpalFixedCodecFactory<OpalG711ALaw64k_Encoder>::Worker(OpalG711ALaw64k_Encoder::GetFactoryName());
  //new OpalFixedCodecFactory<OpalG711ALaw64k_Decoder>::Worker(OpalG711ALaw64k_Decoder::GetFactoryName());

  //new OpalFixedCodecFactory<OpalG711uLaw64k_Encoder>::Worker(OpalG711uLaw64k_Encoder::GetFactoryName());
  //new OpalFixedCodecFactory<OpalG711uLaw64k_Decoder>::Worker(OpalG711uLaw64k_Decoder::GetFactoryName());

#endif // OPAL_AUDIO

#if 0 //OPAL_VIDEO
  // H.323 require an endpoint to have H.261 if it supports video
  //mediaFormatList += OpalMediaFormat("H.261");

#if RFC2190_AVCODEC
  // only have H.263 if RFC2190 is loaded
  //if (OpenH323_IsRFC2190Loaded())
  //  mediaFormatList += OpalMediaFormat("RFC2190 H.263");
#endif  // RFC2190_AVCODEC
#endif  // OPAL_VIDEO
}

/////////////////////////////////////////////////////////////////////////////

#define INCLUDE_STATIC_CODEC(name) \
extern "C" { \
extern unsigned int Opal_StaticCodec_##name##_GetAPIVersion(); \
extern struct PluginCodec_Definition * Opal_StaticCodec_##name##_GetCodecs(unsigned *,unsigned); \
}; \
class H323StaticPluginCodec_##name : public H323StaticPluginCodec \
{ \
  public: \
    PluginCodec_GetAPIVersionFunction Get_GetAPIFn() \
    { return &Opal_StaticCodec_##name##_GetAPIVersion; } \
    PluginCodec_GetCodecFunction Get_GetCodecFn() \
    { return &Opal_StaticCodec_##name##_GetCodecs; } \
}; \
static H323StaticPluginCodecFactory::Worker<H323StaticPluginCodec_##name > static##name##CodecFactory( #name ); \

#ifdef H323_EMBEDDED_GSM

INCLUDE_STATIC_CODEC(GSM_0610)

#endif

