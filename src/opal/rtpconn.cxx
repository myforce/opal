/*
 * rtpconn.cxx
 *
 * Connection abstraction
 *
 * Open Phone Abstraction Library (OPAL)
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
 * $Revision$
 * $Author$
 * $Date$
 */

#include <ptlib.h>

#ifdef P_USE_PRAGMA
#pragma implementation "rtpconn.h"
#endif

#include <opal/buildopts.h>

#include <opal/rtpconn.h>
#include <opal/rtpep.h>
#include <opal/manager.h>
#include <codec/rfc2833.h>
#include <t38/t38proto.h>
#include <opal/patch.h>

#if OPAL_VIDEO
#include <codec/vidcodec.h>
#endif

#ifdef OPAL_ZRTP

extern OpalZRTPConnectionInfo * OpalLibZRTPConnInfo_Create();

#endif


OpalRTPConnection::OpalRTPConnection(OpalCall & call,
                             OpalRTPEndPoint  & ep,
                                const PString & token,
                                   unsigned int options,
                                StringOptions * stringOptions)
  : OpalConnection(call, ep, token, options, stringOptions)
#ifdef _MSC_VER
#pragma warning(disable:4355)
#endif
  , m_rtpSessions(*this)
#ifdef _MSC_VER
#pragma warning(default:4355)
#endif
  , remoteIsNAT(false)
{
  rfc2833Handler  = new OpalRFC2833Proto(*this, PCREATE_NOTIFIER(OnUserInputInlineRFC2833));
  ciscoNSEHandler = new OpalRFC2833Proto(*this, PCREATE_NOTIFIER(OnUserInputInlineCiscoNSE));

#ifdef OPAL_ZRTP
  zrtpEnabled = ep.GetZRTPEnabled();
  zrtpConnInfo = NULL;
#endif
}

OpalRTPConnection::~OpalRTPConnection()
{
  delete rfc2833Handler;
  delete ciscoNSEHandler;
}


RTP_Session * OpalRTPConnection::GetSession(unsigned sessionID) const
{
  return m_rtpSessions.GetSession(sessionID);
}

OpalMediaSession * OpalRTPConnection::GetMediaSession(unsigned sessionID) const
{
  return m_rtpSessions.GetMediaSession(sessionID);
}

RTP_Session * OpalRTPConnection::UseSession(const OpalTransport & transport, unsigned sessionID, const OpalMediaType & mediaType, RTP_QOS * rtpqos)
{
  RTP_Session * rtpSession = m_rtpSessions.GetSession(sessionID);
  if (rtpSession == NULL) {
    rtpSession = CreateSession(transport, sessionID, rtpqos);
    m_rtpSessions.AddSession(rtpSession, mediaType);
  }

  return rtpSession;
}


RTP_Session * OpalRTPConnection::CreateSession(const OpalTransport & transport,
                                                            unsigned sessionID,
                                                           RTP_QOS * rtpqos)
{
  // We only support RTP over UDP at this point in time ...
  if (!transport.IsCompatibleTransport("ip$127.0.0.1")) 
    return NULL;

  PIPSocket::Address localAddress;
 
  transport.GetLocalAddress().GetIpAddress(localAddress);

  OpalManager & manager = GetEndPoint().GetManager();

  PIPSocket::Address remoteAddress;
  transport.GetRemoteAddress().GetIpAddress(remoteAddress);
  PNatMethod * natMethod = manager.GetNatMethod(remoteAddress);

  OpalMediaType mediaType = OpalMediaTypeDefinition::GetMediaTypeForSessionId(sessionID);
  OpalMediaTypeDefinition * def = mediaType.GetDefinition();
  if (def == NULL) {
    PTRACE(1, "RTPCon\tNo definition for media type " << mediaType);
    return NULL;
  }

  // create an RTP session
  RTP_UDP * rtpSession    = NULL;

#ifdef OPAL_ZRTP
  // create ZRTP channel if enabled
  {
    PWaitAndSignal m(zrtpConnInfoMutex);
    if (zrtpEnabled) {
      if (zrtpConnInfo == NULL)
        zrtpConnInfo = OpalLibZRTPConnInfo_Create();
      if (zrtpConnInfo != NULL) {
        rtpSession = zrtpConnInfo->CreateRTPSession(*this, sessionID, remoteIsNAT);
        if (rtpSession == NULL) {
          delete zrtpConnInfo;
          zrtpConnInfo = NULL;
        }
      }
    }
  }
#endif

  if (rtpSession == NULL) {
    rtpSession = def->CreateRTPSession(*this, sessionID, remoteIsNAT);  
    if (rtpSession == NULL) 
      return NULL;
  }

  WORD firstPort = manager.GetRtpIpPortPair();
  WORD nextPort = firstPort;
  while (!rtpSession->Open(localAddress, nextPort, nextPort, manager.GetRtpIpTypeofService(), natMethod, rtpqos)) {
    nextPort = manager.GetRtpIpPortPair();
    if (nextPort == firstPort) {
      PTRACE(1, "RTPCon\tNo ports available for RTP session " << sessionID << " for " << *this);
      delete rtpSession;
      return NULL;
    }
  }

  localAddress = rtpSession->GetLocalAddress();
  if (manager.TranslateIPAddress(localAddress, remoteAddress)){
    rtpSession->SetLocalAddress(localAddress);
  }
  
  return rtpSession;
}

void OpalRTPConnection::ReleaseSession(unsigned sessionID, PBoolean clearAll/* = PFalse */)
{
#ifdef HAS_LIBZRTP
  //check is security mode ZRTP
  if (0 == securityMode.Find("ZRTP")) {
    RTP_Session *session = GetSession(sessionID);
    if (NULL != session){
      OpalZrtp_UDP *zsession = (OpalZrtp_UDP*)session;
      if (NULL != zsession->zrtpStream){
        ::zrtp_stop_stream(zsession->zrtpStream);
        zsession->zrtpStream = NULL;
      }
    }
  }

  printf("release session %i\n", sessionID);
#endif

  m_rtpSessions.ReleaseSession(sessionID, clearAll);
}


PBoolean OpalRTPConnection::IsRTPNATEnabled(const PIPSocket::Address & localAddr, 
                                            const PIPSocket::Address & peerAddr,
                                            const PIPSocket::Address & sigAddr,
                                                              PBoolean incoming)
{
  return static_cast<OpalRTPEndPoint &>(endpoint).IsRTPNATEnabled(*this, localAddr, peerAddr, sigAddr, incoming);
}

#if OPAL_VIDEO
void OpalRTPConnection::OnMediaCommand(OpalMediaCommand & command, INT /*extra*/)
{
  if (PIsDescendant(&command, OpalVideoUpdatePicture)) {
    OpalMediaStreamPtr videoStream = GetMediaStream(OpalMediaType::Video(), false);
    if (videoStream != NULL) {
      RTP_Session * session = m_rtpSessions.GetSession(videoStream->GetSessionID());
      if (session != NULL) {
        session->SendIntraFrameRequest();
#if OPAL_STATISTICS
        m_VideoUpdateRequestsSent++;
      }
#endif
    }
  }
}
#else
void OpalRTPConnection::OnMediaCommand(OpalMediaCommand & /*command*/, INT /*extra*/)
{
}
#endif

void OpalRTPConnection::AttachRFC2833HandlerToPatch(PBoolean isSource, OpalMediaPatch & patch)
{
  if (isSource && (rfc2833Handler != NULL || ciscoNSEHandler != NULL)) {
    OpalRTPMediaStream * mediaStream = dynamic_cast<OpalRTPMediaStream *>(&patch.GetSource());
    if (mediaStream != NULL) {
      RTP_Session & rtpSession = mediaStream->GetRtpSession();
      if (rfc2833Handler != NULL) {
        PTRACE(3, "RTPCon\tAdding RFC2833 receive handler");
        rtpSession.AddFilter(rfc2833Handler->GetReceiveHandler());
      }
      if (ciscoNSEHandler != NULL) {
        PTRACE(3, "RTPCon\tAdding Cisco NSE receive handler");
        rtpSession.AddFilter(ciscoNSEHandler->GetReceiveHandler());
      }
    }
  }
}

PBoolean OpalRTPConnection::SendUserInputTone(char tone, unsigned duration)
{
  if (duration == 0)
    duration = 180;

  return rfc2833Handler->SendToneAsync(tone, duration);
}

PBoolean OpalRTPConnection::GetMediaInformation(unsigned sessionID,
                                         MediaInformation & info) const
{
  if (!mediaTransportAddresses.Contains(sessionID)) {
    PTRACE(2, "RTPCon\tGetMediaInformation for session " << sessionID << " - no channel.");
    return PFalse;
  }

  OpalTransportAddress & address = mediaTransportAddresses[sessionID];

  PIPSocket::Address ip;
  WORD port;
  if (address.GetIpAndPort(ip, port)) {
    info.data    = OpalTransportAddress(ip, (WORD)(port&0xfffe));
    info.control = OpalTransportAddress(ip, (WORD)(port|0x0001));
  }
  else
    info.data = info.control = address;

  info.rfc2833 = rfc2833Handler->GetPayloadType();
  PTRACE(3, "RTPCon\tGetMediaInformation for session " << sessionID
         << " data=" << info.data << " rfc2833=" << info.rfc2833);
  return PTrue;
}

PBoolean OpalRTPConnection::IsMediaBypassPossible(unsigned) const
{
  return true;
}

OpalMediaStream * OpalRTPConnection::CreateMediaStream(const OpalMediaFormat & mediaFormat, unsigned sessionID, PBoolean isSource)
{
  if (ownerCall.IsMediaBypassPossible(*this, sessionID))
    return new OpalNullMediaStream(*this, mediaFormat, sessionID, isSource);

  OpalMediaSession * mediaSession = GetMediaSession(sessionID);
  if (mediaSession == NULL) {
    PTRACE(1, "RTPCon\tCreateMediaStream could not find session " << sessionID);
    return NULL;
  }

  return mediaSession->CreateMediaStream(mediaFormat, sessionID, isSource);
}

void OpalRTPConnection::OnPatchMediaStream(PBoolean isSource, OpalMediaPatch & patch)
{
  OpalConnection::OnPatchMediaStream(isSource, patch);
  if (patch.GetSource().GetMediaFormat().GetMediaType() == OpalMediaType::Audio()) {
    AttachRFC2833HandlerToPatch(isSource, patch);
#if OPAL_PTLIB_DTMF
    if (detectInBandDTMF && isSource) {
      patch.AddFilter(PCREATE_NOTIFIER(OnUserInputInBandDTMF), OPAL_PCM16);
    }
#endif
  }

  patch.SetCommandNotifier(PCREATE_NOTIFIER(OnMediaCommand), !isSource);
}

void OpalRTPConnection::OnUserInputInlineRFC2833(OpalRFC2833Info & info, INT type)
{
  // trigger on start of tone only
  if (type == 0)
    OnUserInputTone(info.GetTone(), info.GetDuration() > 0 ? info.GetDuration()/8 : 100);
}

void OpalRTPConnection::OnUserInputInlineCiscoNSE(OpalRFC2833Info & /*info*/, INT)
{
  //if (!info.IsToneStart())
  //  OnUserInputTone(info.GetTone(), info.GetDuration()/8);
}

void OpalRTPConnection::SessionFailing(RTP_Session & session)
{
  // set this session as failed
  session.SetFailed(true);

  // check to see if all RTP session have failed
  // if so, clear the call
  if (m_rtpSessions.AllSessionsFailing()) {
    PTRACE(2, "RTPCon\tClearing call as all RTP session are failing");
    ClearCall();
  }
}

/////////////////////////////////////////////////////////////////////////////

OpalMediaSession::OpalMediaSession(OpalConnection & _conn, const OpalMediaType & _mediaType, unsigned _sessionId)
  : connection(_conn)
  , mediaType(_mediaType)
  , sessionId(_sessionId)
{
}

OpalMediaSession::OpalMediaSession(const OpalMediaSession & _obj)
  : PObject(_obj)
  , connection(_obj.connection)
  , mediaType(_obj.mediaType)
  , sessionId(_obj.sessionId)
{
}

/////////////////////////////////////////////////////////////////////////////

OpalRTPMediaSession::OpalRTPMediaSession(OpalConnection & _conn, const OpalMediaType & _mediaType, unsigned _sessionId)
  : OpalMediaSession(_conn, _mediaType, _sessionId)
  , rtpSession(NULL)
{
}


OpalRTPMediaSession::OpalRTPMediaSession(const OpalRTPMediaSession & _obj)
  : OpalMediaSession(_obj)
  , rtpSession(_obj.rtpSession)
{
}

void OpalRTPMediaSession::Close()
{
  if (rtpSession != NULL) {
    PTRACE(3, "RTP\tDeleting session " << rtpSession->GetSessionID());
    rtpSession->Close(PTrue);
    rtpSession->SetJitterBufferSize(0, 0);
    delete rtpSession;
    rtpSession = NULL;
  }
}

OpalTransportAddress OpalRTPMediaSession::GetLocalMediaAddress() const
{
  PIPSocket::Address addr = ((RTP_UDP *)rtpSession)->GetLocalAddress();
  WORD port               = ((RTP_UDP *)rtpSession)->GetLocalDataPort();
  return OpalTransportAddress(addr, port, "udp$");
}

SDPMediaDescription * OpalRTPMediaSession::CreateSDPMediaDescription(const OpalTransportAddress & sdpContactAddress)
{
  return mediaType.GetDefinition()->CreateSDPMediaDescription(sdpContactAddress);
}

OpalMediaStream * OpalRTPMediaSession::CreateMediaStream(const OpalMediaFormat & mediaFormat, 
                                                                         unsigned /*sessionID*/, 
                                                                         PBoolean isSource)
{
  return new OpalRTPMediaStream((OpalRTPConnection &)connection, mediaFormat, isSource, *rtpSession,
                                connection.GetMinAudioJitterDelay(),
                                connection.GetMaxAudioJitterDelay());
}


/////////////////////////////////////////////////////////////////////////////

OpalRTPSessionManager::OpalRTPSessionManager(OpalConnection & _conn)
  : connection(_conn)
{
  m_initialised = false;
  m_cleanupOnDelete = true;
}

OpalRTPSessionManager::~OpalRTPSessionManager()
{
  if (m_cleanupOnDelete) {
    while (sessions.GetSize() > 0) {
      sessions.GetDataAt(0).Close();
      sessions.RemoveAt(sessions.GetKeyAt(0));
    }
  }
}

void OpalRTPSessionManager::CopyFromMaster(const OpalRTPSessionManager & from)
{
  PWaitAndSignal m1(m_mutex);
  PWaitAndSignal m2(from.m_mutex);

  PAssert(sessions.GetSize() == 0, "Cannot copy from master RTP session list to non-empty list");

  //for (PINDEX i = 0; i < sessions.GetSize(); ++i) {
  //  PAssert(sessions.GetDataAt(i).rtpSession == NULL, "Cannot copy from master RTP session list after sockets have been created");
  //}

  sessions = from.sessions;
  sessions.MakeUnique();
}

void OpalRTPSessionManager::CopyToMaster(OpalRTPSessionManager & from)
{
  PWaitAndSignal m1(m_mutex);
  PWaitAndSignal m2(from.m_mutex);

  PAssert(from.sessions.GetSize() != 0, "Cannot copy from empty list to master RTP session list");

  //for (PINDEX i = 0; i < sessions.GetSize(); ++i) {
  //  if (sessions.GetDataAt(i).rtpSession != NULL) {
  //    PTRACE(2, "RTP\tCannot copy to master RTP session list after sockets have been created");
  //    return;
  //  }
  //}

  sessions = from.sessions;
  from.m_cleanupOnDelete = false;
}


void OpalRTPSessionManager::AddSession(RTP_Session * rtpSession, const OpalMediaType & mediaType)
{
  PWaitAndSignal m(m_mutex);
  
  if (rtpSession != NULL) {
    OpalMediaSession * session = sessions.GetAt(rtpSession->GetSessionID());
    OpalRTPMediaSession * s;
    if (session == NULL) {
      s = new OpalRTPMediaSession(connection, mediaType, 0);
      s->rtpSession = rtpSession;
      sessions.Insert(POrdinalKey(rtpSession->GetSessionID()), s);
      PTRACE(3, "RTP\tCreating new session " << *rtpSession);
    }
    else
    {
      s = dynamic_cast<OpalRTPMediaSession *>(session);
      PAssert(s != NULL,             "RTP session type does not match");
      PAssert(s->rtpSession == NULL, "Cannot add already existing session");
      s->rtpSession = rtpSession;
    }
  }
}

void OpalRTPSessionManager::AddMediaSession(OpalMediaSession * mediaSession, const OpalMediaType & /*mediaType*/)
{
  PWaitAndSignal m(m_mutex);
  
  PAssert(sessions.GetAt(mediaSession->sessionId) == NULL, "Cannot add already existing session");
  sessions.Insert(POrdinalKey(mediaSession->sessionId), mediaSession);
}

void OpalRTPSessionManager::ReleaseSession(unsigned PTRACE_PARAM(sessionID), PBoolean /*clearAll*/)
{
  PTRACE(3, "RTP\tReleasing session " << sessionID);
}

OpalMediaSession * OpalRTPSessionManager::GetMediaSession(unsigned sessionID) const
{
  PWaitAndSignal wait(m_mutex);

  OpalMediaSession * session;
  if (((session = sessions.GetAt(sessionID)) == NULL) || !session->IsActive()) {
    PTRACE(3, "RTP\tCannot find media session " << sessionID);
    return NULL;
  }

  PTRACE(3, "RTP\tFound existing media session " << sessionID);
  return session;
}


RTP_Session * OpalRTPSessionManager::GetSession(unsigned sessionID) const
{
  PWaitAndSignal wait(m_mutex);

  OpalMediaSession * session;
  if ( ((session = sessions.GetAt(sessionID)) == NULL) || 
       !session->IsActive() ||
       !session->IsRTP()
       ) {
    PTRACE(3, "RTP\tCannot find RTP session " << sessionID);
    return NULL;
  }

  PTRACE(3, "RTP\tFound existing RTP session " << sessionID);
  return ((OpalRTPMediaSession *)session)->rtpSession;
}

bool OpalRTPSessionManager::AllSessionsFailing()
{
  PWaitAndSignal wait(m_mutex);

  for (PINDEX i = 0; i < sessions.GetSize(); ++i) {
    OpalMediaSession & session = sessions.GetDataAt(i);
    if (session.IsActive() && !session.HasFailed())
      return false;
  }

  return true;
}

/////////////////////////////////////////////////////////////////////////////

