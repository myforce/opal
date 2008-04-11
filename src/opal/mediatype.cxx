/*
 * mediatype.h
 *
 * Media Format Type descriptions
 *
 * Open Phone Abstraction Library (OPAL)
 *
 * Copyright (C) 2007 Post Increment and Hannes Friederich
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
 * The Original Code is OPAL
 *
 * The Initial Developer of the Original Code is Hannes Friederich and Post Increment
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision: 19424 $
 * $Author: csoutheren $
 * $Date: 2008-02-08 17:24:10 +1100 (Fri, 08 Feb 2008) $
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "mediatype.h"
#endif

#include <opal/buildopts.h>
#include <opal/mediatype.h>
#include <opal/rtpconn.h>
#include <opal/call.h>

#if OPAL_H323
#include <h323/channels.h>
#endif

namespace PWLibStupidLinkerHacks {
  int mediaTypeLoader;
}; // namespace PWLibStupidLinkerHacks

//OPAL_DEFINE_MEDIA_TYPE_NO_SDP(userinput); 

#if OPAL_AUDIO
OPAL_DECLARE_MEDIA_TYPE(audio, OpalAudioMediaType);
#endif

#if OPAL_VIDEO
OPAL_DECLARE_MEDIA_TYPE(video, OpalVideoMediaType);
#endif

///////////////////////////////////////////////////////////////////////////////

ostream & operator << (ostream & strm, const OpalMediaType & mediaType)
{ mediaType.PrintOn(strm); return strm; }

const char * OpalMediaType::Audio()     { static const char * str = "audio";     return str; }
const char * OpalMediaType::Video()     { static const char * str = "video";     return str; }
const char * OpalMediaType::Fax()       { static const char * str = "fax";       return str; };
const char * OpalMediaType::UserInput() { static const char * str = "userinput"; return str; };

///////////////////////////////////////////////////////////////////////////////

OpalMediaTypeDefinition * OpalMediaType::GetDefinition() const
{
  return OpalMediaTypeFactory::CreateInstance(*this);
}


OpalMediaTypeDefinition * OpalMediaType::GetDefinition(const OpalMediaType & key)
{
  return OpalMediaTypeFactory::CreateInstance(key);
}

#if 0 // disabled

OpalMediaType::SessionIDToMediaTypeMap_T & OpalMediaType::GetSessionIDToMediaTypeMap()
{
  static SessionIDToMediaTypeMap_T sessionIDToMediaTypeMap;
  return sessionIDToMediaTypeMap;
}

#endif  // disabled

///////////////////////////////////////////////////////////////////////////////

OpalMediaTypeDefinition::OpalMediaTypeDefinition(const char * _sdpType, unsigned /*_defaultSessionId*/)
#if OPAL_SIP
  : sdpType(_sdpType != NULL ? _sdpType : "")
#endif
  //, defaultSessionId(_defaultSessionId),
{
#if 0 // disabled
  // always deconflict the default session ID
  if (OpalMediaType::GetSessionIDToMediaTypeMap().find(defaultSessionId) != OpalMediaType::GetSessionIDToMediaTypeMap().end())
    defaultSessionId = 0;
  if (defaultSessionId == 0) {
    defaultSessionId = 100;
    while (OpalMediaType::GetSessionIDToMediaTypeMap().find(defaultSessionId) != OpalMediaType::GetSessionIDToMediaTypeMap().end())
      ++defaultSessionId;
  }
  OpalMediaType::GetSessionIDToMediaTypeMap().insert(OpalMediaType::SessionIDToMediaTypeMap_T::value_type(defaultSessionId, mediaType));
#endif // disabled
}

///////////////////////////////////////////////////////////////////////////////

OpalRTPAVPMediaType::OpalRTPAVPMediaType(const char * sdpType, unsigned sessionID)
  : OpalMediaTypeDefinition(sdpType, sessionID)
{
}

#if 0 // disabled

RTP_UDP * OpalRTPAVPMediaType::CreateRTPSession(OpalRTPConnection & conn,
#if OPAL_RTP_AGGREGATE
                                                PHandleAggregator * agg,
#endif
                                                unsigned sessionID, bool remoteIsNAT)
{
  RTP_UDP * rtpSession = NULL;

  PString securityMode = conn.GetSecurityMode();

  if (!securityMode.IsEmpty()) {
    OpalSecurityMode * parms = PFactory<OpalSecurityMode>::CreateInstance(securityMode);
    if (parms == NULL) {
      PTRACE(1, "RTPCon\tSecurity mode " << securityMode << " unknown");
      return NULL;
    }
    rtpSession = parms->CreateRTPSession(conn,
#if OPAL_RTP_AGGREGATE
                                              agg,
#endif
                                         sessionID, remoteIsNAT);
    if (rtpSession == NULL) {
      PTRACE(1, "RTPCon\tCannot create RTP session for security mode " << securityMode);
      delete parms;
      return NULL;
    }
  }
  else
  {
    rtpSession = new RTP_UDP(GetRTPEncoding(),
#if OPAL_RTP_AGGREGATE
        agg,
#endif
        sessionID, remoteIsNAT);
  }

  return rtpSession;
}

bool OpalRTPAVPMediaType::UseDirectMediaPatch() const
{
  return false;
}

#endif // disabled

///////////////////////////////////////////////////////////////////////////////

#if OPAL_AUDIO

OpalAudioMediaType::OpalAudioMediaType()
  : OpalRTPAVPMediaType("audio", 1)
{
}

#if 0 // disabled

bool OpalAudioMediaType::IsMediaAutoStart(bool) const 
{ return true; }

#endif // disabled

#endif // OPAL_AUDIO

///////////////////////////////////////////////////////////////////////////////

#if OPAL_VIDEO

OpalVideoMediaType::OpalVideoMediaType()
  : OpalRTPAVPMediaType("video", 2)
{ }

#if 0 // disabled

bool OpalVideoMediaType::IsMediaAutoStart(bool) const 
{ return true; }

#endif // disabled

#endif // OPAL_VIDEO

///////////////////////////////////////////////////////////////////////////////

#if 0

OpalMediaSessionId::OpalMediaSessionId(const OpalMediaType & _mediaType, unsigned _sessionId)
  : mediaType(_mediaType), sessionId(_sessionId)
{
  if (sessionId == 0) {
    OpalMediaTypeDefinition * def = OpalMediaTypeFactory::CreateInstance(mediaType);
    if (def != NULL)
      sessionId = def->GetPreferredSessionId();
  }
}

#endif // disabled


