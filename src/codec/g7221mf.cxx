/*
 * g7221mf.cxx
 *
 * GSM-AMR Media Format descriptions
 *
 * Open Phone Abstraction Library
 * Formally known as the Open H323 project.
 *
 * Copyright (c) 2008 Vox Lucida
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
 * The Original Code is Open Phone Abstraction Library
 *
 * The Initial Developer of the Original Code is Vox Lucida
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#include <ptlib.h>

#include "g7221mf_inc.cxx"

#include <opal/mediafmt.h>
#include <codec/opalplugin.h>
#include <h323/h323caps.h>


class OpalG7221Format : public OpalAudioFormatInternal
{
  public:
    OpalG7221Format(const char * formatName, unsigned bitRate, unsigned sampleRate)
      : OpalAudioFormatInternal(formatName,
                                RTP_DataFrame::DynamicBase,
                                G7221EncodingName,
                                bitRate/400,
                                sampleRate*G7221_FRAME_MS/1000,
                                1, 1, 1,
                                sampleRate)
    {
      OpalMediaOption * option;

#if OPAL_SDP
      option = new OpalMediaOptionInteger(G7221BitRateOptionName,
                                          true,
                                          OpalMediaOption::EqualMerge,
                                          bitRate, bitRate, bitRate);
      option->SetFMTP(G7221BitRateFMTPName, "0");
      AddOption(option);
#endif

#if OPAL_H323
      OPAL_SET_MEDIA_OPTION_H245(FindOption(OpalAudioFormat::RxFramesPerPacketOption()), G7221_H241_RxFramesPerPacket);

      if (sampleRate == G7221C_24K_SAMPLE_RATE) {
        option = new OpalMediaOptionUnsigned(G7221ExtendedModesOptionName, true, OpalMediaOption::IntersectionMerge, 0x70, 0, 255);
        OPAL_SET_MEDIA_OPTION_H245(option, G7221_H241_ExtendedModes);
        AddOption(option);
      }
#endif
    }


    virtual PObject * Clone() const
    {
      return new OpalG7221Format(*this);
    }
};


#if OPAL_H323
  #define OID(type) \
    extern const char type##_Identifier[] = type##_OID;

  #define CAPABILITY(type) \
    static H323CapabilityFactory::Worker< \
      H323GenericAudioCapabilityTemplate<type##_Identifier, GetOpal##type, type##_BIT_RATE> \
    > capability(type##_FormatName, true);
#else
  #define OID(type)
  #define CAPABILITY(type)
#endif


#define FORMAT(type) \
  OID(type) \
  const OpalAudioFormat & GetOpal##type() \
  { \
    static OpalAudioFormat const plugin(type##_FormatName); if (plugin.IsValid()) return plugin; \
    static OpalAudioFormat const format(new OpalG7221Format(type##_FormatName, type##_BIT_RATE, type##_SAMPLE_RATE)); \
    CAPABILITY(type) \
    return format; \
  }


FORMAT(G7221_24K)
FORMAT(G7221_32K)
FORMAT(G7221C_24K)
FORMAT(G7221C_32K)
FORMAT(G7221C_48K)


// End of File ///////////////////////////////////////////////////////////////
