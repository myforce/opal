/*
 * g726codec.cxx
 *
 * H.323 protocol handler
 *
 * Open H323 Library
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
 * Contributor(s): ______________________________________.
 *
 * $Log: g726codec.cxx,v $
 * Revision 1.2005  2004/02/19 10:47:02  rjongbloed
 * Merged OpenH323 version 1.13.1 changes.
 *
 * Revision 2.3  2002/11/10 11:33:17  robertj
 * Updated to OpenH323 v1.10.3
 *
 * Revision 2.2  2002/09/04 06:01:47  robertj
 * Updated to OpenH323 v1.9.6
 * Revision 1.6  2003/11/08 03:10:48  rjongbloed
 * Fixed incorrect size passed in for non-standard G.726 capability, thanks  Victor Ivashin.
 *
 * Revision 1.5  2002/11/09 07:07:10  robertj
 * Made public the media format names.
 * Other cosmetic changes.
 *
 * Revision 1.4  2002/09/03 07:28:42  robertj
 * Cosmetic change to formatting.
 *
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "g726codec.h"
#endif

#include <codec/g726codec.h>

extern "C" {
#include "g726/g72x.h"
};


#define new PNEW


OpalMediaFormat const OpalG726_40(OPAL_G726_40,
                                  OpalMediaFormat::DefaultAudioSessionID,
                                  RTP_DataFrame::G721,
                                  "G721",
                                  TRUE,  // Needs jitter
                                  40000, // bits/sec
                                  5,  // 4 bytes per "frame"
                                  8,  // 1 millisecond
                                  OpalMediaFormat::AudioClockRate);

OpalMediaFormat const OpalG726_32(OPAL_G726_32,
                                  OpalMediaFormat::DefaultAudioSessionID,
                                  RTP_DataFrame::G721,
                                  "G721",
                                  TRUE,  // Needs jitter
                                  32000, // bits/sec
                                  4,  // 4 bytes per "frame"
                                  8,  // 1 millisecond
                                  OpalMediaFormat::AudioClockRate);

OpalMediaFormat const OpalG726_24(OPAL_G726_24,
                                  OpalMediaFormat::DefaultAudioSessionID,
                                  RTP_DataFrame::G721,
                                  "G721",
                                  TRUE,  // Needs jitter
                                  24000, // bits/sec
                                  3,  // 4 bytes per "frame"
                                  8,  // 1 millisecond
                                  OpalMediaFormat::AudioClockRate);

OpalMediaFormat const OpalG726_16(OPAL_G726_16,
                                  OpalMediaFormat::DefaultAudioSessionID,
                                  RTP_DataFrame::G721,
                                  "G721",
                                  TRUE,  // Needs jitter
                                  16000, // bits/sec
                                  2,  // 4 bytes per "frame"
                                  8,  // 1 millisecond
                                  OpalMediaFormat::AudioClockRate);


struct G726_NonStandardInfo {
  char name[10]; /// G.726-xxk
  BYTE count;
};


static G726_NonStandardInfo const G726_NonStandard[H323_G726_Capability::NumSpeeds] = {
  { OPAL_G726_40 },
  { OPAL_G726_32 },
  { OPAL_G726_24 },
  { OPAL_G726_16 }
};


/////////////////////////////////////////////////////////////////////////////

#ifndef NO_H323

H323_G726_Capability::H323_G726_Capability(H323EndPoint & endpoint, Speeds s)
    : H323NonStandardAudioCapability(240, 10, endpoint,
                                     (const BYTE *)&G726_NonStandard[s],
                                     sizeof(G726_NonStandardInfo),
                                     0, sizeof(G726_NonStandard[s].name))
{
  speed = s;
}


PObject * H323_G726_Capability::Clone() const
{
  return new H323_G726_Capability(*this);
}


PString H323_G726_Capability::GetFormatName() const
{
  return G726_NonStandard[speed].name;
}


BOOL H323_G726_Capability::OnSendingPDU(H245_AudioCapability & pdu,
                                        unsigned packetSize) const
{
  G726_NonStandardInfo * info = (G726_NonStandardInfo *)(const BYTE *)nonStandardData;
  info->count = (BYTE)packetSize;
  return H323NonStandardAudioCapability::OnSendingPDU(pdu, packetSize);
}


BOOL H323_G726_Capability::OnReceivedPDU(const H245_AudioCapability & pdu,
                                         unsigned & packetSize)
{
  if (!H323NonStandardAudioCapability::OnReceivedPDU(pdu, packetSize))
    return FALSE;

  const G726_NonStandardInfo * info = (const G726_NonStandardInfo *)(const BYTE *)nonStandardData;
  packetSize = info->count;
  return TRUE;
}


#endif


///////////////////////////////////////////////////////////////////////////////

Opal_G726_Transcoder::Opal_G726_Transcoder(const OpalTranscoderRegistration & registration,
                                           unsigned bits)
  : OpalStreamedTranscoder(registration, bits, 16, 160)
{
  g726 = (struct g726_state_s *)malloc((unsigned)sizeof(struct g726_state_s));
  PTRACE(3, "Codec\tG.726 transcoder created");
}


Opal_G726_Transcoder::~Opal_G726_Transcoder()
{
  if (g726 != NULL)
    free(g726);
}
 

Opal_G726_40_PCM::Opal_G726_40_PCM(const OpalTranscoderRegistration & registration)
  : Opal_G726_Transcoder(registration, 5)
{
}


int Opal_G726_40_PCM::ConvertOne(int sample) const
{
  return g726_40_encoder(sample, AUDIO_ENCODING_LINEAR, g726);
}


Opal_PCM_G726_40::Opal_PCM_G726_40(const OpalTranscoderRegistration & registration)
  : Opal_G726_Transcoder(registration, 5)
{
}


int Opal_PCM_G726_40::ConvertOne(int sample) const
{
  return g726_40_decoder(sample, AUDIO_ENCODING_LINEAR, g726);
}


Opal_G726_32_PCM::Opal_G726_32_PCM(const OpalTranscoderRegistration & registration)
  : Opal_G726_Transcoder(registration, 4)
{
}


int Opal_G726_32_PCM::ConvertOne(int sample) const
{
  return g726_32_encoder(sample, AUDIO_ENCODING_LINEAR, g726);
}


Opal_PCM_G726_32::Opal_PCM_G726_32(const OpalTranscoderRegistration & registration)
  : Opal_G726_Transcoder(registration, 4)
{
}


int Opal_PCM_G726_32::ConvertOne(int sample) const
{
  return g726_32_decoder(sample, AUDIO_ENCODING_LINEAR, g726);
}


Opal_G726_24_PCM::Opal_G726_24_PCM(const OpalTranscoderRegistration & registration)
  : Opal_G726_Transcoder(registration, 3)
{
}


int Opal_G726_24_PCM::ConvertOne(int sample) const
{
  return g726_24_encoder(sample, AUDIO_ENCODING_LINEAR, g726);
}


Opal_PCM_G726_24::Opal_PCM_G726_24(const OpalTranscoderRegistration & registration)
  : Opal_G726_Transcoder(registration, 3)
{
}


int Opal_PCM_G726_24::ConvertOne(int sample) const
{
  return g726_24_decoder(sample, AUDIO_ENCODING_LINEAR, g726);
}


Opal_G726_16_PCM::Opal_G726_16_PCM(const OpalTranscoderRegistration & registration)
  : Opal_G726_Transcoder(registration, 2)
{
}


int Opal_G726_16_PCM::ConvertOne(int sample) const
{
  return g726_16_encoder(sample, AUDIO_ENCODING_LINEAR, g726);
}


Opal_PCM_G726_16::Opal_PCM_G726_16(const OpalTranscoderRegistration & registration)
  : Opal_G726_Transcoder(registration, 2)
{
}


int Opal_PCM_G726_16::ConvertOne(int sample) const
{
  return g726_16_decoder(sample, AUDIO_ENCODING_LINEAR, g726);
}


/////////////////////////////////////////////////////////////////////////////
