/*
 * mediafmt.cxx
 *
 * Media Format descriptions
 *
 * Open H323 Library
 *
 * Copyright (c) 1999-2000 Equivalence Pty. Ltd.
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
 * $Log: mediafmt.cxx,v $
 * Revision 1.2017  2003/01/07 04:39:53  robertj
 * Updated to OpenH323 v1.11.2
 *
 * Revision 2.15  2002/11/10 11:33:19  robertj
 * Updated to OpenH323 v1.10.3
 *
 * Revision 2.14  2002/09/04 06:01:49  robertj
 * Updated to OpenH323 v1.9.6
 *
 * Revision 2.13  2002/07/01 04:56:33  robertj
 * Updated to OpenH323 v1.9.1
 *
 * Revision 2.12  2002/03/27 05:36:44  robertj
 * Set RFC2833 payload type to be 101 for Cisco compatibility
 *
 * Revision 2.11  2002/02/19 07:36:51  robertj
 * Added OpalRFC2833 as a OpalMediaFormat variable.
 *
 * Revision 2.10  2002/02/11 09:32:13  robertj
 * Updated to openH323 v1.8.0
 *
 * Revision 2.9  2002/01/22 05:14:38  robertj
 * Added RTP encoding name string to media format database.
 * Changed time units to clock rate in Hz.
 *
 * Revision 2.8  2002/01/14 06:35:58  robertj
 * Updated to OpenH323 v1.7.9
 *
 * Revision 2.7  2001/11/15 06:55:26  robertj
 * Fixed Reorder() function so reorders EVERY format that matches wildcard.
 *
 * Revision 2.6  2001/10/05 00:22:14  robertj
 * Updated to PWLib 1.2.0 and OpenH323 1.7.0
 *
 * Revision 2.5  2001/10/04 00:43:57  robertj
 * Added function to remove wildcard from list.
 * Added constructor to make a list with one format in it.
 * Fixed wildcard matching so trailing * works.
 * Optimised reorder so does not reorder if already in order.
 *
 * Revision 2.4  2001/08/23 05:51:17  robertj
 * Completed implementation of codec reordering.
 *
 * Revision 2.3  2001/08/22 03:51:44  robertj
 * Added functions to look up media format by payload type.
 *
 * Revision 2.2  2001/08/01 06:22:07  robertj
 * Fixed GNU warning.
 *
 * Revision 2.1  2001/08/01 05:45:34  robertj
 * Made OpalMediaFormatList class global to help with documentation.
 *
 * Revision 2.0  2001/07/27 15:48:25  robertj
 * Conversion of OpenH323 to Open Phone Abstraction Library (OPAL)
 *
 * Revision 1.11  2002/12/03 09:20:01  craigs
 * Fixed problem with RFC2833 and a dynamic RTP type using the same RTP payload number
 *
 * Revision 1.10  2002/12/02 03:06:26  robertj
 * Fixed over zealous removal of code when NO_AUDIO_CODECS set.
 *
 * Revision 1.9  2002/10/30 05:54:17  craigs
 * Fixed compatibilty problems with G.723.1 6k3 and 5k3
 *
 * Revision 1.8  2002/08/05 10:03:48  robertj
 * Cosmetic changes to normalise the usage of pragma interface/implementation.
 *
 * Revision 1.7  2002/06/25 08:30:13  robertj
 * Changes to differentiate between stright G.723.1 and G.723.1 Annex A using
 *   the OLC dataType silenceSuppression field so does not send SID frames
 *   to receiver codecs that do not understand them.
 *
 * Revision 1.6  2002/01/22 07:08:26  robertj
 * Added IllegalPayloadType enum as need marker for none set
 *   and MaxPayloadType is a legal value.
 *
 * Revision 1.5  2001/12/11 04:27:28  craigs
 * Added support for 5.3kbps G723.1
 *
 * Revision 1.4  2001/09/21 02:51:45  robertj
 * Implemented static object for all "known" media formats.
 * Added default session ID to media format description.
 *
 * Revision 1.3  2001/05/11 04:43:43  robertj
 * Added variable names for standard PCM-16 media format name.
 *
 * Revision 1.2  2001/02/09 05:13:56  craigs
 * Added pragma implementation to (hopefully) reduce the executable image size
 * under Linux
 *
 * Revision 1.1  2001/01/25 07:27:16  robertj
 * Major changes to add more flexible OpalMediaFormat class to normalise
 *   all information about media types, especially codecs.
 *
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "mediafmt.h"
#endif

#include <opal/mediafmt.h>


#define new PNEW


/////////////////////////////////////////////////////////////////////////////

OpalMediaFormat const OpalPCM16(
  OPAL_PCM16,
  OpalMediaFormat::DefaultAudioSessionID,
  RTP_DataFrame::L16_Mono,
  "L16",
  TRUE,   // Needs jitter
  128000, // bits/sec
  16, // bytes/frame
  8, // 1 millisecond
  OpalMediaFormat::AudioClockRate
);

OpalMediaFormat const OpalG711uLaw(
  OPAL_G711_ULAW_64K,
  OpalMediaFormat::DefaultAudioSessionID,
  RTP_DataFrame::PCMU,
  "PCMU",
  TRUE,   // Needs jitter
  64000, // bits/sec
  8, // bytes/frame
  8, // 1 millisecond/frame
  OpalMediaFormat::AudioClockRate
);

OpalMediaFormat const OpalG711ALaw(
  OPAL_G711_ALAW_64K,
  OpalMediaFormat::DefaultAudioSessionID,
  RTP_DataFrame::PCMA,
  "PCMA",
  TRUE,   // Needs jitter
  64000, // bits/sec
  8, // bytes/frame
  8, // 1 millisecond/frame
  OpalMediaFormat::AudioClockRate
);

OpalMediaFormat const OpalG728(  
  OPAL_G728,
  OpalMediaFormat::DefaultAudioSessionID,
  RTP_DataFrame::G728,
  "G728",
  TRUE, // Needs jitter
  16000,// bits/sec
  5,    // bytes
  20,   // 2.5 milliseconds
  OpalMediaFormat::AudioClockRate
);

OpalMediaFormat const OpalG729( 
  OPAL_G729,
  OpalMediaFormat::DefaultAudioSessionID,
  RTP_DataFrame::G729,
  "G729",
  TRUE, // Needs jitter
  8000, // bits/sec
  10,   // bytes
  80,   // 10 milliseconds
  OpalMediaFormat::AudioClockRate
);

OpalMediaFormat const OpalG729A( 
  OPAL_G729A,
  OpalMediaFormat::DefaultAudioSessionID,
  RTP_DataFrame::G729,
  "G729",
  TRUE, // Needs jitter
  8000, // bits/sec
  10,   // bytes
  80,   // 10 milliseconds
  OpalMediaFormat::AudioClockRate
);

OpalMediaFormat const OpalG729B(
  OPAL_G729B,
  OpalMediaFormat::DefaultAudioSessionID,
  RTP_DataFrame::G729,
  "G729",
  TRUE, // Needs jitter
  8000, // bits/sec
  10,   // bytes
  80,   // 10 milliseconds
  OpalMediaFormat::AudioClockRate
);

OpalMediaFormat const OpalG729AB(
  OPAL_G729AB,
  OpalMediaFormat::DefaultAudioSessionID,
  RTP_DataFrame::G729,
  "G729",
  TRUE, // Needs jitter
  8000, // bits/sec
  10,   // bytes
  80,   // 10 milliseconds
  OpalMediaFormat::AudioClockRate
);

OpalMediaFormat const OpalG7231_6k3(
  OPAL_G7231_6k3,
  OpalMediaFormat::DefaultAudioSessionID,
  RTP_DataFrame::G7231,
  "G723",
  TRUE, // Needs jitter
  6400, // bits/sec
  24,   // bytes
  240,  // 30 milliseconds
  OpalMediaFormat::AudioClockRate
);

OpalMediaFormat const OpalG7231_5k3(
  OPAL_G7231_5k3,
  OpalMediaFormat::DefaultAudioSessionID,
  RTP_DataFrame::G7231,
  "G723",
  TRUE, // Needs jitter
  5300, // bits/sec
  24,   // bytes
  240,  // 30 milliseconds
  OpalMediaFormat::AudioClockRate
);

OpalMediaFormat const OpalG7231A_6k3(
  OPAL_G7231A_6k3,
  OpalMediaFormat::DefaultAudioSessionID,
  RTP_DataFrame::G7231,
  "G723",
  TRUE, // Needs jitter
  6400, // bits/sec
  24,   // bytes
  240,  // 30 milliseconds
  OpalMediaFormat::AudioClockRate
);

OpalMediaFormat const OpalG7231A_5k3(
  OPAL_G7231A_5k3,
  OpalMediaFormat::DefaultAudioSessionID,
  RTP_DataFrame::G7231,
  "G723",
  TRUE, // Needs jitter
  5300, // bits/sec
  24,   // bytes
  240,  // 30 milliseconds
  OpalMediaFormat::AudioClockRate
);

OpalMediaFormat const OpalGSM0610(
  OPAL_GSM0610,
  OpalMediaFormat::DefaultAudioSessionID,
  RTP_DataFrame::GSM,
  "GSM",
  TRUE,  // Needs jitter
  13200, // bits/sec
  33,    // bytes
  160,   // 20 milliseconds
  OpalMediaFormat::AudioClockRate
);

OpalMediaFormat const OpalRFC2833(
  OPAL_RFC2833,
  0,
  (RTP_DataFrame::PayloadTypes)101,  // Set to this for Cisco compatibility
  "telephone-event",
  TRUE,   // Needs jitter
  32*(1000/50), // bits/sec  (32 bits every 50ms)
  4,      // bytes/frame
  150*8,  // 150 millisecond
  OpalMediaFormat::AudioClockRate
);


/////////////////////////////////////////////////////////////////////////////

OpalMediaFormat::OpalMediaFormat()
{
  rtpPayloadType = RTP_DataFrame::IllegalPayloadType;

  needsJitter = FALSE;
  bandwidth = 0;
  frameSize = 0;
  frameTime = 0;
  clockRate = 0;
}


OpalMediaFormat::OpalMediaFormat(RTP_DataFrame::PayloadTypes pt)
{
  operator=(pt);
}


OpalMediaFormat::OpalMediaFormat(const char * wildcard)
{
  operator=(PString(wildcard));
}


OpalMediaFormat::OpalMediaFormat(const PString & wildcard)
{
  operator=(wildcard);
}


OpalMediaFormat::OpalMediaFormat(const char * fullName,
                                 unsigned dsid,
                                 RTP_DataFrame::PayloadTypes pt,
                                 const char * en,
                                 BOOL     nj,
                                 unsigned bw,
                                 PINDEX   fs,
                                 unsigned ft,
                                 unsigned cr)
  : PCaselessString(fullName)
{
  rtpPayloadType = pt;
  rtpEncodingName = en;
  defaultSessionID = dsid;
  needsJitter = nj;
  bandwidth = bw;
  frameSize = fs;
  frameTime = ft;
  clockRate = cr;

  PINDEX i;
  OpalMediaFormatList & registeredFormats = GetMediaFormatsList();

  if ((i = registeredFormats.GetValuesIndex(*this)) != P_MAX_INDEX) {
    *this = registeredFormats[i]; // Already registered, use previous values
    return;
  }

  // assume non-dynamic payload types are correct and do not need deconflicting
  if (rtpPayloadType < RTP_DataFrame::DynamicBase) {
    registeredFormats.OpalMediaFormatBaseList::Append(this);
    return;
  }

  // find the next unused dynamic number, and find anything with the new 
  // rtp payload type if it is explicitly required
  OpalMediaFormat * match = NULL;
  RTP_DataFrame::PayloadTypes nextUnused = RTP_DataFrame::DynamicBase;
  do {
    for (i = 0; i < registeredFormats.GetSize(); i++) {
      if (registeredFormats[i].GetPayloadType() == nextUnused) {
        nextUnused = (RTP_DataFrame::PayloadTypes)(nextUnused + 1);
        break;
      }
      if ((rtpPayloadType >= RTP_DataFrame::DynamicBase) && 
          (registeredFormats[i].GetPayloadType() == rtpPayloadType))
        match = &registeredFormats[i];
    }
  } while (i < registeredFormats.GetSize());

  // if new format requires a specific payload type in the dynamic range, 
  // then move the old format to the next unused format
  if (match != NULL)
    match->rtpPayloadType = nextUnused;
  else
    rtpPayloadType = nextUnused;

  registeredFormats.OpalMediaFormatBaseList::Append(this);
}


OpalMediaFormat & OpalMediaFormat::operator=(RTP_DataFrame::PayloadTypes pt)
{
  const OpalMediaFormatList & registeredFormats = GetRegisteredMediaFormats();
  PINDEX idx = registeredFormats.FindFormat(pt);
  if (idx != P_MAX_INDEX)
    *this = registeredFormats[idx];
  else
    *this = OpalMediaFormat();

  return *this;
}


OpalMediaFormat & OpalMediaFormat::operator=(const char * wildcard)
{
  return operator=(PString(wildcard));
}

OpalMediaFormat & OpalMediaFormat::operator=(const PString & wildcard)
{
  const OpalMediaFormatList & registeredFormats = GetRegisteredMediaFormats();
  PINDEX idx = registeredFormats.FindFormat(wildcard);
  if (idx != P_MAX_INDEX)
    *this = registeredFormats[idx];
  else
    *this = OpalMediaFormat();

  return *this;
}


OpalMediaFormatList & OpalMediaFormat::GetMediaFormatsList()
{
  static OpalMediaFormatList registeredFormats;
  return registeredFormats;
}


///////////////////////////////////////////////////////////////////////////////

OpalMediaFormatList::OpalMediaFormatList()
{
  DisallowDeleteObjects();
}


OpalMediaFormatList::OpalMediaFormatList(const OpalMediaFormat & format)
{
  DisallowDeleteObjects();
  *this += format;
}


OpalMediaFormatList & OpalMediaFormatList::operator+=(const OpalMediaFormat & format)
{
  if (!format) {
    if (!HasFormat(format)) {
      const OpalMediaFormatList & registeredFormats = OpalMediaFormat::GetRegisteredMediaFormats();
      PINDEX idx = registeredFormats.FindFormat(format);
      if (idx != P_MAX_INDEX)
        OpalMediaFormatBaseList::Append(&registeredFormats[idx]);
    }
  }
  return *this;
}


OpalMediaFormatList & OpalMediaFormatList::operator-=(const OpalMediaFormat & format)
{
  PINDEX idx = FindFormat(format);
  if (idx != P_MAX_INDEX)
    RemoveAt(idx);

  return *this;
}


void OpalMediaFormatList::Remove(const PStringArray & mask)
{
  PINDEX i;
  for (i = 0; i < mask.GetSize(); i++) {
    PINDEX idx;
    while ((idx = FindFormat(mask[i])) != P_MAX_INDEX)
      RemoveAt(idx);
  }
}


PINDEX OpalMediaFormatList::FindFormat(RTP_DataFrame::PayloadTypes pt) const
{
  for (PINDEX idx = 0; idx < GetSize(); idx++) {
    if ((*this)[idx].GetPayloadType() == pt)
      return idx;
  }

  return P_MAX_INDEX;
}


static BOOL WildcardMatch(const PCaselessString & str, const PStringArray & wildcards)
{
  if (wildcards.GetSize() == 1)
    return str == wildcards[0];

  PINDEX i;
  PINDEX last = 0;
  for (i = 0; i < wildcards.GetSize(); i++) {
    PString wildcard = wildcards[i];

    PINDEX next;
    if (wildcard.IsEmpty())
      next = last;
    else {
      next = str.Find(wildcard, last);
      if (next == P_MAX_INDEX)
        return FALSE;
    }

    // Check for having * at beginning of search string
    if (i == 0 && next != 0 && !wildcard)
      return FALSE;

    last = next + wildcard.GetLength();

    // Check for having * at end of search string
    if (i == wildcards.GetSize()-1 && !wildcard && last != str.GetLength())
      return FALSE;
  }

  return TRUE;
}


PINDEX OpalMediaFormatList::FindFormat(const PString & search) const
{
  PStringArray wildcards = search.Tokenise('*', TRUE);
  for (PINDEX idx = 0; idx < GetSize(); idx++) {
    if (WildcardMatch((*this)[idx], wildcards))
      return idx;
  }

  return P_MAX_INDEX;
}


void OpalMediaFormatList::Reorder(const PStringArray & order)
{
  PINDEX nextPos = 0;
  for (PINDEX i = 0; i < order.GetSize(); i++) {
    PStringArray wildcards = order[i].Tokenise('*', TRUE);

    PINDEX findPos = 0;
    while (findPos < GetSize()) {
      if (WildcardMatch((*this)[findPos], wildcards)) {
        if (findPos > nextPos)
          OpalMediaFormatBaseList::InsertAt(nextPos, RemoveAt(findPos));
        nextPos++;
      }
      findPos++;
    }
  }
}


// End of File ///////////////////////////////////////////////////////////////
