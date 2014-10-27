/*
 * sdpep.cxx
 *
 * Open Phone Abstraction Library (OPAL)
 *
 * Copyright (C) 2014 Vox Lucida Pty. Ltd.
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
#pragma implementation "sdpep.h"
#endif

#include <sdp/sdpep.h>

#if OPAL_SDP

#include <rtp/rtpconn.h>
#include <rtp/rtp_stream.h>
#include <rtp/dtls_srtp_session.h>
#include <codec/rfc2833.h>
#include <opal/transcoders.h>
#include <opal/patch.h>
#include <ptclib/random.h>


#define PTraceModule() "SDP-EP"


///////////////////////////////////////////////////////////////////////////////

const PCaselessString & OpalSDPEndPoint::ContentType() { static PConstCaselessString const s("application/sdp"); return s; }

OpalSDPEndPoint::OpalSDPEndPoint(OpalManager & manager,     ///<  Manager of all endpoints.
                       const PCaselessString & prefix,      ///<  Prefix for URL style address strings
                                  Attributes   attributes)  ///<  Bit mask of attributes endpoint has
  : OpalRTPEndPoint(manager, prefix, attributes)
  , m_holdTimeout(0, 40) // Seconds
{
}


OpalSDPEndPoint::~OpalSDPEndPoint()
{
}


SDPSessionDescription * OpalSDPEndPoint::CreateSDP(time_t sessionId, unsigned version, const OpalTransportAddress & address)
{
  return new SDPSessionDescription(sessionId, version, address);
}


///////////////////////////////////////////////////////////////////////////////

OpalSDPConnection::OpalSDPConnection(OpalCall & call,
                             OpalSDPEndPoint  & ep,
                                const PString & token,
                                   unsigned int options,
                                StringOptions * stringOptions)
  : OpalRTPConnection(call, ep, token, options, stringOptions)
  , m_endpoint(ep)
  , m_offerPending(false)
  , m_holdToRemote(eHoldOff)
  , m_holdFromRemote(false)
{
}


OpalSDPConnection::~OpalSDPConnection()
{
}


OpalMediaFormatList OpalSDPConnection::GetMediaFormats() const
{
  // Need to limit the media formats to what the other side provided in a re-INVITE
  if (!m_answerFormatList.IsEmpty()) {
    PTRACE(4, "Using offered media format list");
    return m_answerFormatList;
  }

  if (!m_remoteFormatList.IsEmpty()) {
    PTRACE(4, "Using remote media format list: " << setfill(',') << m_remoteFormatList);
    return m_remoteFormatList;
  }

  return OpalMediaFormatList();
}


bool OpalSDPConnection::HoldRemote(bool placeOnHold)
{
  switch (m_holdToRemote) {
    case eHoldOff :
    case eRetrieveInProgress :
      if (!placeOnHold) {
        PTRACE(4, "Hold off request ignored as not on hold for " << *this);
        return true;
      }
      break;

    case eHoldOn :
    case eHoldInProgress :
      if (placeOnHold) {
        PTRACE(4, "Hold on request ignored as already on hold fir " << *this);
        return true;
      }
      break;
  }

  HoldState origState = m_holdToRemote;

  switch (m_holdToRemote) {
    case eHoldOff :
      m_holdToRemote = eHoldInProgress;
      break;

    case eHoldOn :
      m_holdToRemote = eRetrieveInProgress;
      break;

    case eRetrieveInProgress :
    case eHoldInProgress :
      PTRACE(4, "Hold " << (placeOnHold ? "on" : "off") << " request deferred as in progress for " << *this);
      GetEndPoint().GetManager().QueueDecoupledEvent(new PSafeWorkArg1<OpalSDPConnection, bool>(
                                         this, placeOnHold, &OpalSDPConnection::RetryHoldRemote));
      return true;
  }

  if (OnHoldStateChanged(placeOnHold))
    return true;

  m_holdToRemote = origState;
  return false;
}


void OpalSDPConnection::RetryHoldRemote(bool placeOnHold)
{
  HoldState progressState = placeOnHold ? eRetrieveInProgress : eHoldInProgress;
  PSimpleTimer failsafe(m_endpoint.GetHoldTimeout());
  while (m_holdToRemote == progressState) {
    PThread::Sleep(100);

    if (IsReleased() || failsafe.HasExpired()) {
      PTRACE(3, "Hold " << (placeOnHold ? "on" : "off") << " request failed for " << *this);
      return;
    }

    PTRACE(5, "Hold " << (placeOnHold ? "on" : "off") << " request still in progress for " << *this);
  }

  HoldRemote(placeOnHold);
}


PBoolean OpalSDPConnection::IsOnHold(bool fromRemote) const
{
  return fromRemote ? m_holdFromRemote : (m_holdToRemote >= eHoldOn);
}


bool OpalSDPConnection::GetOfferSDP(SDPSessionDescription & offer, bool offerOpenMediaStreamsOnly)
{
  if (GetPhase() == UninitialisedPhase) {
    InternalSetAsOriginating();
    SetPhase(SetUpPhase);
    OnApplyStringOptions();
  }

  m_offerPending = true;

  return OnSendOfferSDP(offer, offerOpenMediaStreamsOnly);
}


PString OpalSDPConnection::GetOfferSDP(bool offerOpenMediaStreamsOnly)
{
  std::auto_ptr<SDPSessionDescription> sdp(CreateSDP(PString::Empty()));
  return sdp.get() != NULL && GetOfferSDP(*sdp, offerOpenMediaStreamsOnly) ? sdp->Encode() : PString::Empty();
}


bool OpalSDPConnection::AnswerOfferSDP(const SDPSessionDescription & offer, SDPSessionDescription & answer)
{
  if (m_offerPending) {
    PTRACE(2, "Outgoing offer pending, cannot handle incoming offer.");
    return false;
  }

  if (GetPhase() == UninitialisedPhase) {
    SetPhase(SetUpPhase);
    OnApplyStringOptions();
    if (!OnIncomingConnection(0, NULL))
      return false;
  }

  return OnSendAnswerSDP(offer, answer);
}


PString OpalSDPConnection::AnswerOfferSDP(const PString & offer)
{
  if (GetPhase() == UninitialisedPhase) {
    SetPhase(SetUpPhase);
    OnApplyStringOptions();
    if (!OnIncomingConnection(0, NULL))
      return PString::Empty();
  }

  std::auto_ptr<SDPSessionDescription> sdpIn(CreateSDP(offer));
  std::auto_ptr<SDPSessionDescription> sdpOut(CreateSDP(PString::Empty()));
  if (sdpIn.get() == NULL || sdpOut.get() == NULL)
    return PString::Empty();

  if (!OnSendAnswerSDP(*sdpIn, *sdpOut))
    return PString::Empty();

  SetConnected();
  return sdpOut->Encode();
}


bool OpalSDPConnection::HandleAnswerSDP(const SDPSessionDescription & answer)
{
  if (!m_offerPending) {
    PTRACE(1, "Did not send an offer before handling answer");
    return false;
  }

  m_offerPending = false;

  bool dummy;
  if (!OnReceivedAnswerSDP(answer, dummy))
    return false;

  OnConnected();
  return true;
}


bool OpalSDPConnection::HandleAnswerSDP(const PString & answer)
{
  std::auto_ptr<SDPSessionDescription> sdp(CreateSDP(answer));
  return sdp.get() != NULL && HandleAnswerSDP(*sdp);
}


SDPSessionDescription * OpalSDPConnection::CreateSDP(const PString & sdpStr)
{
  if (sdpStr.IsEmpty())
    return m_endpoint.CreateSDP(0, 0, OpalTransportAddress(GetMediaInterface(), 0, OpalTransportAddress::UdpPrefix()));

  OpalMediaFormatList formats = GetLocalMediaFormats();
  if (formats.IsEmpty())
    formats = OpalMediaFormat::GetAllRegisteredMediaFormats();

  SDPSessionDescription * sdpPtr = m_endpoint.CreateSDP(0, 0, OpalTransportAddress());
  PTRACE_CONTEXT_ID_TO(*sdpPtr);

  if (sdpPtr->Decode(sdpStr, formats))
    return sdpPtr;

  delete sdpPtr;
  return NULL;
}


OpalMediaSession * OpalSDPConnection::SetUpMediaSession(const unsigned sessionId,
                                                    const OpalMediaType & mediaType,
                                                    const SDPMediaDescription & mediaDescription,
                                                    OpalTransportAddress & localAddress,
                                                    bool & remoteChanged)
{
  if (mediaDescription.GetPort() == 0) {
    PTRACE(2, "Received disabled/missing media description for " << mediaType);

    /* Some remotes return all of the media detail (a= lines) in SDP even though
       port is zero indicating the media is not to be used. So don't return these
       bogus media formats from SDP to the "remote media format list". */
    m_remoteFormatList.Remove(PString('@')+mediaType);
    return NULL;
  }

  // Create the OpalMediaSession if required
  OpalMediaSession * session = UseMediaSession(sessionId, mediaType, mediaDescription.GetSessionType());
  if (session == NULL)
    return NULL;

  OpalRTPSession * rtpSession = dynamic_cast<OpalRTPSession *>(session);
  if (rtpSession != NULL)
    rtpSession->SetExtensionHeader(mediaDescription.GetExtensionHeaders());

  OpalTransportAddress remoteMediaAddress = mediaDescription.GetMediaAddress();
  if (remoteMediaAddress.IsEmpty()) {
    PTRACE(2, "Received media description with no address for " << mediaType);
    remoteChanged = true;
    return session;
  }

  // see if remote socket information has changed
  OpalTransportAddress oldRemoteMediaAddress = session->GetRemoteAddress();
  remoteChanged = !oldRemoteMediaAddress.IsEmpty() && oldRemoteMediaAddress != remoteMediaAddress;

  if (remoteChanged) {
    PTRACE(3, "Remote changed IP address: "<< oldRemoteMediaAddress << "!=" << remoteMediaAddress);
    ((OpalRTPEndPoint &)endpoint).CheckEndLocalRTP(*this, rtpSession);
  }

  if (mediaDescription.ToSession(session) && session->Open(GetMediaInterface(), remoteMediaAddress, true)) {
    localAddress = session->GetLocalAddress();
    return session;
  }

  ReleaseMediaSession(sessionId);
  return NULL;
}


static bool SetNxECapabilities(OpalRFC2833Proto * handler,
                      const OpalMediaFormatList & localMediaFormats,
                      const OpalMediaFormatList & remoteMediaFormats,
                          const OpalMediaFormat & baseMediaFormat,
                            SDPMediaDescription * localMedia = NULL)
{
  OpalMediaFormatList::const_iterator remFmt = remoteMediaFormats.FindFormat(baseMediaFormat);
  if (remFmt == remoteMediaFormats.end()) {
    // Not in remote list, disable transmitter
    handler->SetTxMediaFormat(OpalMediaFormat());
    return false;
  }

  OpalMediaFormatList::const_iterator localFmt = localMediaFormats.FindFormat(baseMediaFormat);
  if (localFmt == localMediaFormats.end()) {
    // Not in our local list, disable transmitter
    handler->SetTxMediaFormat(OpalMediaFormat());
    return true;
  }

  // Merge remotes format into ours.
  // Note if this is our initial offer remote is the same as local.
  OpalMediaFormat adjustedFormat = *localFmt;
  adjustedFormat.Merge(*remFmt, true);

  handler->SetTxMediaFormat(adjustedFormat);

  if (localMedia != NULL) {
    // Set the receive handler to what we are sending to remote in our SDP
    handler->SetRxMediaFormat(adjustedFormat);
    SDPMediaFormat * fmt = localMedia->CreateSDPMediaFormat();
    fmt->FromMediaFormat(adjustedFormat);
    localMedia->AddSDPMediaFormat(fmt);
  }

  return true;
}


static bool PauseOrCloseMediaStream(OpalMediaStreamPtr & stream,
                                    const OpalMediaFormatList & answerFormats,
                                    bool remoteChanged,
                                    bool paused)
{
  if (stream == NULL)
    return false;

  if (!stream->IsOpen()) {
    PTRACE(4, "Re-INVITE of closed stream " << *stream);
    stream.SetNULL();
    return false;
  }

  if (!remoteChanged) {
    OpalMediaFormatList::const_iterator fmt = answerFormats.FindFormat(stream->GetMediaFormat());
    if (fmt != answerFormats.end() && stream->UpdateMediaFormat(*fmt, true)) {
      PTRACE2(4, &*stream, "INVITE change needs to " << (paused ? "pause" : "resume") << " stream " << *stream);
      stream->InternalSetPaused(paused, false, false);
      return !paused;
    }
    PTRACE(4, "Re-INVITE (format change) needs to close stream " << *stream);
  }
  else {
    PTRACE(4, "Re-INVITE (address/port change) needs to close stream " << *stream);
  }

  OpalMediaPatchPtr patch = stream->GetPatch();
  if (patch != NULL)
    patch->GetSource().Close();
  stream.SetNULL();
  return false;
}


bool OpalSDPConnection::OnSendOfferSDP(SDPSessionDescription & sdpOut, bool offerOpenMediaStreamsOnly)
{
  bool sdpOK = false;

  if (offerOpenMediaStreamsOnly && !mediaStreams.IsEmpty()) {
    PTRACE(4, "Offering only current media streams in Re-INVITE");
    for (SessionMap::iterator it = m_sessions.begin(); it != m_sessions.end(); ++it) {
      if (OnSendOfferSDPSession(it->first, sdpOut, true))
        sdpOK = true;
      else
        sdpOut.AddMediaDescription(it->second->CreateSDPMediaDescription());
    }
  }
  else {
    PTRACE(4, "Offering all configured media:\n    " << setfill(',') << m_localMediaFormats << setfill(' '));

    if (m_remoteFormatList.IsEmpty()) {
      // Need to fake the remote formats with everything we do,
      // so parts of the offering work correctly
      m_remoteFormatList = GetLocalMediaFormats();
      m_remoteFormatList.MakeUnique();
      AdjustMediaFormats(false, NULL, m_remoteFormatList);
    }

#if OPAL_VIDEO
    SetAudioVideoGroup(); // Googlish audio and video grouping id
#endif

    // Create media sessions based on available media types and make sure audio and video are first two sessions
    vector<bool> sessions = CreateAllMediaSessions();
    for (vector<bool>::size_type session = 1; session < sessions.size(); ++session) {
      if (sessions[session]) {
        if (OnSendOfferSDPSession(session, sdpOut, false))
          sdpOK = true;
        else
          ReleaseMediaSession(session);
      }
    }
  }

  return sdpOK && !sdpOut.GetMediaDescriptions().IsEmpty();
}


#if OPAL_VIDEO
void OpalSDPConnection::SetAudioVideoGroup()
{
  if (!m_stringOptions.GetBoolean(OPAL_OPT_AV_GROUPING))
    return;

  // Googlish audio and video grouping id
  OpalRTPSession * audioSession = dynamic_cast<OpalRTPSession *>(FindSessionByMediaType(OpalMediaType::Audio()));
  if (audioSession == NULL)
    return;

  OpalRTPSession * videoSession = dynamic_cast<OpalRTPSession *>(FindSessionByMediaType(OpalMediaType::Video()));
  if (videoSession == NULL)
    return;

  PString bundle = PBase64::Encode(PRandom::Octets(24));
  audioSession->SetBundleId(bundle);
  videoSession->SetBundleId(bundle);
}
#endif // OPAL_VIDEO


bool OpalSDPConnection::OnSendOfferSDPSession(unsigned   sessionId,
                             SDPSessionDescription & sdp,
                                              bool   offerOpenMediaStreamOnly)
{
  OpalMediaSession * mediaSession = GetMediaSession(sessionId);
  if (mediaSession == NULL) {
    PTRACE(1, "Could not create RTP session " << sessionId);
    return false;
  }

  OpalMediaType mediaType = mediaSession->GetMediaType();
  if (!m_localMediaFormats.HasType(mediaType)) {
    PTRACE(2, "No formats of type " << mediaType << " for RTP session " << sessionId);
    return false;
  }

  if (m_stringOptions.GetBoolean(OPAL_OPT_RTCP_MUX)) {
    OpalRTPSession * rtpSession = dynamic_cast<OpalRTPSession *>(mediaSession);
    if (rtpSession != NULL) {
      PTRACE(3, "Setting single port mode for offerred RTP session " << sessionId << " for media type " << mediaType);
      rtpSession->SetSinglePortRx();
    }
  }

  if (!mediaSession->Open(GetMediaInterface(), GetRemoteMediaAddress(), true)) {
    PTRACE(1, "Could not open RTP session " << sessionId << " for media type " << mediaType);
    return false;
  }

  SDPMediaDescription * localMedia = mediaSession->CreateSDPMediaDescription();
  if (localMedia == NULL) {
    PTRACE(2, "Can't create SDP media description for media type " << mediaType);
    return false;
  }

  localMedia->SetOptionStrings(m_stringOptions);
  localMedia->FromSession(mediaSession, NULL);

  if (sdp.GetDefaultConnectAddress().IsEmpty())
    sdp.SetDefaultConnectAddress(mediaSession->GetLocalAddress());

  if (offerOpenMediaStreamOnly) {
    OpalMediaStreamPtr recvStream = GetMediaStream(sessionId, true);
    OpalMediaStreamPtr sendStream = GetMediaStream(sessionId, false);
    if (recvStream != NULL)
      localMedia->AddMediaFormat(*m_localMediaFormats.FindFormat(recvStream->GetMediaFormat()));
    else if (sendStream != NULL)
      localMedia->AddMediaFormat(sendStream->GetMediaFormat());
    else
      localMedia->AddMediaFormats(m_localMediaFormats, mediaType);

    bool sending = sendStream != NULL && sendStream->IsOpen() && !sendStream->IsPaused();
    if (sending && m_holdFromRemote) {
      // OK we have (possibly) asymmetric hold, check if remote supports it.
      PString regex = m_stringOptions(OPAL_OPT_SYMMETRIC_HOLD_PRODUCT);
      if (regex.IsEmpty() || remoteProductInfo.AsString().FindRegEx(regex) == P_MAX_INDEX)
        sending = false;
    }
    bool recving = m_holdToRemote < eHoldOn && recvStream != NULL && recvStream->IsOpen();

    if (sending && recving)
      localMedia->SetDirection(SDPMediaDescription::SendRecv);
    else if (recving)
      localMedia->SetDirection(SDPMediaDescription::RecvOnly);
    else if (sending)
      localMedia->SetDirection(SDPMediaDescription::SendOnly);
    else
      localMedia->SetDirection(SDPMediaDescription::Inactive);

#if PAUSE_WITH_EMPTY_ADDRESS
    if (m_holdToRemote >= eHoldOn) {
      OpalTransportAddress addr = localMedia->GetTransportAddress();
      PIPSocket::Address dummy;
      WORD port;
      addr.GetIpAndPort(dummy, port);
      OpalTransportAddress newAddr("0.0.0.0", port, addr.GetProtoPrefix());
      localMedia->SetTransportAddress(newAddr);
      localMedia->SetDirection(SDPMediaDescription::Undefined);
      sdp.SetDefaultConnectAddress(newAddr);
    }
#endif
  }
  else {
    localMedia->AddMediaFormats(m_localMediaFormats, mediaType);
    localMedia->SetDirection((SDPMediaDescription::Direction)(3&(unsigned)GetAutoStart(mediaType)));
  }

  if (mediaType == OpalMediaType::Audio()) {
    // Set format if we have an RTP payload type for RFC2833 and/or NSE
    // Must be after other codecs, as Mediatrix gateways barf if RFC2833 is first
    SetNxECapabilities(m_rfc2833Handler, m_localMediaFormats, m_remoteFormatList, OpalRFC2833, localMedia);
#if OPAL_T38_CAPABILITY
    SetNxECapabilities(m_ciscoNSEHandler, m_localMediaFormats, m_remoteFormatList, OpalCiscoNSE, localMedia);
#endif
  }

#if OPAL_SRTP
  if (GetMediaCryptoKeyExchangeModes()&OpalMediaCryptoSuite::e_SecureSignalling)
    localMedia->SetCryptoKeys(mediaSession->GetOfferedCryptoKeys());
#endif

#if OPAL_RTP_FEC
  if (GetAutoStart(OpalFEC::MediaType()) != OpalMediaType::DontOffer) {
    OpalMediaFormat redundantMediaFormat;
    for (OpalMediaFormatList::iterator it = m_localMediaFormats.begin(); it != m_localMediaFormats.end(); ++it) {
      if (it->GetMediaType() == OpalFEC::MediaType() && it->GetOptionString(OpalFEC::MediaTypeOption()) == mediaType) {
        if (it->GetName().NumCompare(OPAL_REDUNDANT_PREFIX) == EqualTo)
          redundantMediaFormat = *it;
        else
          localMedia->AddMediaFormat(*it);
      }
    }

    if (redundantMediaFormat.IsValid()) {
      // Calculate the fmtp for red
      PStringStream fmtp;
      OpalMediaFormatList formats = localMedia->GetMediaFormats();
      for (OpalMediaFormatList::iterator it = formats.begin(); it != formats.end(); ++it) {
        if (it->IsTransportable() && *it != redundantMediaFormat) {
          if (!fmtp.IsEmpty())
            fmtp << '/';
          fmtp << (unsigned)it->GetPayloadType();
        }
      }
      redundantMediaFormat.SetOptionString("FMTP", fmtp);
      localMedia->AddMediaFormat(redundantMediaFormat);
    }
  }
#endif // OPAL_RTP_FEC

  sdp.AddMediaDescription(localMedia);

  return true;
}


bool OpalSDPConnection::OnSendAnswerSDP(const SDPSessionDescription & sdpOffer, SDPSessionDescription & sdpOut)
{
  // get the remote media formats
  m_answerFormatList = sdpOffer.GetMediaFormats();

  // Remove anything we never offerred
  while (!m_answerFormatList.IsEmpty() && m_localMediaFormats.FindFormat(m_answerFormatList.front()) == m_localMediaFormats.end())
    m_answerFormatList.RemoveHead();

  AdjustMediaFormats(false, NULL, m_answerFormatList);
  if (m_answerFormatList.IsEmpty()) {
    PTRACE(3, "All media formats offered by remote have been removed.");
    return false;
  }

  size_t sessionCount = sdpOffer.GetMediaDescriptions().GetSize();
  vector<SDPMediaDescription *> sdpMediaDescriptions(sessionCount+1);
  size_t sessionId;

#if OPAL_SRTP
  for (sessionId = 1; sessionId <= sessionCount; ++sessionId) {
    SDPMediaDescription * incomingMedia = sdpOffer.GetMediaDescriptionByIndex(sessionId);
    if (PAssert(incomingMedia != NULL, PLogicError) && incomingMedia->IsSecure())
      sdpMediaDescriptions[sessionId] = OnSendAnswerSDPSession(incomingMedia, sessionId, sdpOffer.GetDirection(sessionId));
  }
  for (sessionId = 1; sessionId <= sessionCount; ++sessionId) {
    SDPMediaDescription * incomingMedia = sdpOffer.GetMediaDescriptionByIndex(sessionId);
    if (PAssert(incomingMedia != NULL, PLogicError) && !incomingMedia->IsSecure())
      sdpMediaDescriptions[sessionId] = OnSendAnswerSDPSession(incomingMedia, sessionId, sdpOffer.GetDirection(sessionId));
  }
#else
  for (sessionId = 1; sessionId <= sessionCount; ++sessionId) {
    SDPMediaDescription * incomingMedia = sdpOffer.GetMediaDescriptionByIndex(sessionId);
    if (PAssert(incomingMedia != NULL, "SDP Media description list changed"))
      sdpMediaDescriptions[sessionId] = OnSendAnswerSDPSession(incomingMedia, sessionId, sdpOffer.GetDirection(sessionId));
  }
#endif // OPAL_SRTP

#if OPAL_VIDEO
  SetAudioVideoGroup(); // Googlish audio and video grouping id
#endif

  // Fill in refusal for media sessions we didn't like
  bool gotNothing = true;
  for (sessionId = 1; sessionId <= sessionCount; ++sessionId) {
    SDPMediaDescription * incomingMedia = sdpOffer.GetMediaDescriptionByIndex(sessionId);
    if (!PAssert(incomingMedia != NULL, PLogicError))
      return false;

    SDPMediaDescription * md = sdpMediaDescriptions[sessionId];
    if (md == NULL)
      sdpOut.AddMediaDescription(new SDPDummyMediaDescription(*incomingMedia));
    else {
      md->FromSession(GetMediaSession(sessionId), incomingMedia);
      sdpOut.AddMediaDescription(md);
      gotNothing = false;
    }
  }

  if (gotNothing)
    return false;

  /* Shut down any media that is in a session not mentioned in a re-INVITE.
      While the SIP/SDP specification says this shouldn't happen, it does
      anyway so we need to deal. */
  for (OpalMediaStreamPtr stream(mediaStreams, PSafeReference); stream != NULL; ++stream) {
    unsigned session = stream->GetSessionID();
    if (session > sessionCount || sdpMediaDescriptions[session] == NULL)
      stream->Close();
  }

  // In case some new streams got created in a re-INVITE.
  if (IsEstablished())
    StartMediaStreams();

  return true;
}


SDPMediaDescription * OpalSDPConnection::OnSendAnswerSDPSession(SDPMediaDescription * incomingMedia,
                                                                       unsigned   sessionId,
                                                 SDPMediaDescription::Direction   otherSidesDir)
{
  OpalMediaType mediaType = incomingMedia->GetMediaType();

  // See if any media formats of this session id, so don't create unused RTP session
  if (!m_answerFormatList.HasType(mediaType)) {
    PTRACE(3, "No media formats of type " << mediaType << ", not adding SDP");
    return NULL;
  }

  if (!PAssert(mediaType.GetDefinition() != NULL, PString("Unusable media type \"") + mediaType + '"'))
    return NULL;

#if OPAL_SRTP
  OpalMediaCryptoKeyList keys = incomingMedia->GetCryptoKeys();
  if (!keys.IsEmpty() && !(GetMediaCryptoKeyExchangeModes()&OpalMediaCryptoSuite::e_SecureSignalling)) {
    PTRACE(2, "No secure signaling, cannot use SDES crypto for " << mediaType << " session " << sessionId);
    keys.RemoveAll();
    incomingMedia->SetCryptoKeys(keys);
  }

  // See if we already have a secure version of the media session
  for (SessionMap::const_iterator it = m_sessions.begin(); it != m_sessions.end(); ++it) {
    if (it->second->GetSessionID() != sessionId &&
        it->second->GetMediaType() == mediaType &&
        (
          it->second->GetSessionType() == OpalSRTPSession::RTP_SAVP() ||
          it->second->GetSessionType() == OpalDTLSSRTPSession::RTP_DTLS_SAVPF()
        ) &&
        it->second->IsOpen()) {
      PTRACE(3, "Not creating " << mediaType << " media session, already secure.");
      return NULL;
    }
  }
#endif // OPAL_SRTP

  // Create new media session
  OpalTransportAddress localAddress;
  bool remoteChanged = false;
  OpalMediaSession * mediaSession = SetUpMediaSession(sessionId, mediaType, *incomingMedia, localAddress, remoteChanged);
  if (mediaSession == NULL)
    return NULL;

  bool replaceSession = false;

  // For fax for example, we have to switch the media session according to mediaType
  if (mediaSession->GetMediaType() != mediaType) {
    PTRACE(3, "Replacing " << mediaSession->GetMediaType() << " session " << sessionId << " with " << mediaType);
#if OPAL_T38_CAPABILITY
    if (mediaType == OpalMediaType::Fax()) {
      if (!OnSwitchingFaxMediaStreams(true)) {
        PTRACE(2, "Switch to T.38 refused for " << *this);
        return NULL;
      }
    }
    else if (mediaSession->GetMediaType() == OpalMediaType::Fax()) {
      if (!OnSwitchingFaxMediaStreams(false)) {
        PTRACE(2, "Switch from T.38 refused for " << *this);
        return NULL;
      }
    }
#endif // OPAL_T38_CAPABILITY
    mediaSession = CreateMediaSession(sessionId, mediaType, incomingMedia->GetSDPTransportType());
    if (mediaSession == NULL) {
      PTRACE(2, "Could not create session for " << mediaType);
      return NULL;
    }

    // Set flag to force media stream close
    remoteChanged = true;
    replaceSession = true;
  }

  // construct a new media session list 
  std::auto_ptr<SDPMediaDescription> localMedia(mediaSession->CreateSDPMediaDescription());
  if (localMedia.get() == NULL) {
    if (replaceSession)
      delete mediaSession; // Still born so can delete, not used anywhere
    PTRACE(1, "Could not create SDP media description for media type " << mediaType);
    return NULL;
  }

  /* Make sure SDP transport type in preply is same as in offer. This is primarily
     a workaround for broken implementations, esecially with respect to feedback
     (AVPF) and DTLS (UDP/TLS/SAFP) */
  localMedia->SetSDPTransportType(incomingMedia->GetSDPTransportType());

  // Get SDP string options through
  localMedia->SetOptionStrings(m_stringOptions);

#if OPAL_SRTP
  if (!keys.IsEmpty()) {// SDES
    // Set rx key from the other side SDP, which it's tx key
    if (!mediaSession->ApplyCryptoKey(keys, true)) {
      PTRACE(2, "Incompatible crypto suite(s) for " << mediaType << " session " << sessionId);
      return NULL;
    }

    // Use symmetric keys, generate a cloneof the remotes tx key for out yx key
    OpalMediaCryptoKeyInfo * txKey = keys.front().CloneAs<OpalMediaCryptoKeyInfo>();
    if (PAssertNULL(txKey) == NULL)
      return NULL;

    // But with a different value
    txKey->Randomise();

    keys.RemoveAll();
    keys.Append(txKey);
    if (!mediaSession->ApplyCryptoKey(keys, false)) {
      PTRACE(2, "Unexpected error with crypto suite(s) for " << mediaType << " session " << sessionId);
      return NULL;
    }

    localMedia->SetCryptoKeys(keys);
  }
#endif // OPAL_SRTP

  if (GetPhase() < ConnectedPhase) {
    // If processing initial INVITE and video, obey the auto-start flags
    OpalMediaType::AutoStartMode autoStart = GetAutoStart(mediaType);
    if ((autoStart&OpalMediaType::Transmit) == 0)
      otherSidesDir = (otherSidesDir&SDPMediaDescription::SendOnly) != 0 ? SDPMediaDescription::SendOnly : SDPMediaDescription::Inactive;
    if ((autoStart&OpalMediaType::Receive) == 0)
      otherSidesDir = (otherSidesDir&SDPMediaDescription::RecvOnly) != 0 ? SDPMediaDescription::RecvOnly : SDPMediaDescription::Inactive;
  }

  PTRACE(4, "Answering offer for media type " << mediaType << ", directions=" << otherSidesDir);

  SDPMediaDescription::Direction newDirection = SDPMediaDescription::Inactive;

  // Check if we had a stream and the remote has either changed the codec or
  // changed the direction of the stream
  OpalMediaStreamPtr sendStream = GetMediaStream(sessionId, false);
  if (PauseOrCloseMediaStream(sendStream, m_answerFormatList, remoteChanged || replaceSession, (otherSidesDir&SDPMediaDescription::RecvOnly) == 0))
    newDirection = SDPMediaDescription::SendOnly;

  OpalMediaStreamPtr recvStream = GetMediaStream(sessionId, true);
  if (PauseOrCloseMediaStream(recvStream, m_answerFormatList, remoteChanged || replaceSession,
                              m_holdToRemote >= eHoldOn || (otherSidesDir&SDPMediaDescription::SendOnly) == 0))
    newDirection = newDirection != SDPMediaDescription::Inactive ? SDPMediaDescription::SendRecv : SDPMediaDescription::RecvOnly;

  // See if we need to do a session switcharoo, but must be after stream closing
  if (replaceSession)
    ReplaceMediaSession(sessionId, mediaSession);

  /* After (possibly) closing streams, we now open them again if necessary,
     OpenSourceMediaStreams will just return true if they are already open.
     We open tx (other party source) side first so we follow the remote
     endpoints preferences. */
  if (!incomingMedia->GetMediaAddress().IsEmpty()) {
    PSafePtr<OpalConnection> otherParty = GetOtherPartyConnection();
    if (otherParty != NULL && sendStream == NULL) {
      if ((sendStream = GetMediaStream(sessionId, false)) == NULL) {
        PTRACE(5, "Opening tx " << mediaType << " stream from offer SDP");
        if (ownerCall.OpenSourceMediaStreams(*otherParty,
                                             mediaType,
                                             sessionId,
                                             OpalMediaFormat(),
#if OPAL_VIDEO
                                             incomingMedia->GetContentRole(),
#endif
                                             false,
                                             (otherSidesDir&SDPMediaDescription::RecvOnly) == 0))
          sendStream = GetMediaStream(sessionId, false);
      }

      if ((otherSidesDir&SDPMediaDescription::RecvOnly) != 0) {
        if (sendStream == NULL) {
          PTRACE(4, "Did not open required tx " << mediaType << " stream.");
          return NULL;
        }
        newDirection = newDirection != SDPMediaDescription::Inactive ? SDPMediaDescription::SendRecv
                                                                     : SDPMediaDescription::SendOnly;
      }
    }

    if (sendStream != NULL) {
      // In case is re-INVITE and remote has tweaked the streams paramters, we need to merge them
      sendStream->UpdateMediaFormat(*m_answerFormatList.FindFormat(sendStream->GetMediaFormat()), true);
    }

    if (recvStream == NULL) {
      if ((recvStream = GetMediaStream(sessionId, true)) == NULL) {
        PTRACE(5, "Opening rx " << mediaType << " stream from offer SDP");
        if (ownerCall.OpenSourceMediaStreams(*this,
                                             mediaType,
                                             sessionId,
                                             OpalMediaFormat(),
#if OPAL_VIDEO
                                             incomingMedia->GetContentRole(),
#endif
                                             false,
                                             (otherSidesDir&SDPMediaDescription::SendOnly) == 0))
          recvStream = GetMediaStream(sessionId, true);
      }

      if ((otherSidesDir&SDPMediaDescription::SendOnly) != 0) {
        if (recvStream == NULL) {
          PTRACE(4, "Did not open required rx " << mediaType << " stream.");
          return NULL;
        }
        newDirection = newDirection != SDPMediaDescription::Inactive ? SDPMediaDescription::SendRecv
                                                                     : SDPMediaDescription::RecvOnly;
      }
    }

    if (recvStream != NULL) {
      OpalMediaFormat adjustedMediaFormat = *m_answerFormatList.FindFormat(recvStream->GetMediaFormat());

      // If we are sendrecv we will receive the same payload type as we transmit.
      if (newDirection == SDPMediaDescription::SendRecv)
        adjustedMediaFormat.SetPayloadType(sendStream->GetMediaFormat().GetPayloadType());

      recvStream->UpdateMediaFormat(adjustedMediaFormat, true);
    }
  }

  // Now we build the reply, setting "direction" as appropriate for what we opened.
  localMedia->SetDirection(newDirection);
  if (sendStream != NULL)
    localMedia->AddMediaFormat(sendStream->GetMediaFormat());
  else if (recvStream != NULL)
    localMedia->AddMediaFormat(recvStream->GetMediaFormat());
  else {
    // Add all possible formats
    bool empty = true;
    for (OpalMediaFormatList::iterator remoteFormat = m_remoteFormatList.begin(); remoteFormat != m_remoteFormatList.end(); ++remoteFormat) {
      if (remoteFormat->GetMediaType() == mediaType) {
        for (OpalMediaFormatList::iterator localFormat = m_localMediaFormats.begin(); localFormat != m_localMediaFormats.end(); ++localFormat) {
          if (localFormat->GetMediaType() == mediaType) {
            OpalMediaFormat intermediateFormat;
            if (OpalTranscoder::FindIntermediateFormat(*localFormat, *remoteFormat, intermediateFormat)) {
              localMedia->AddMediaFormat(*remoteFormat);
              empty = false;
              break;
            }
          }
        }
      }
    }

    // RFC3264 says we MUST have an entry, but it should have port zero
    if (empty) {
      localMedia->AddMediaFormat(m_answerFormatList.front());
      localMedia->FromSession(NULL, NULL);
    }
    else {
      // We can do the media type but choose not to at this time
      localMedia->SetDirection(SDPMediaDescription::Inactive);
    }
  }

  if (mediaType == OpalMediaType::Audio()) {
    // Set format if we have an RTP payload type for RFC2833 and/or NSE
    SetNxECapabilities(m_rfc2833Handler, m_localMediaFormats, m_answerFormatList, OpalRFC2833, localMedia.get());
#if OPAL_T38_CAPABILITY
    SetNxECapabilities(m_ciscoNSEHandler, m_localMediaFormats, m_answerFormatList, OpalCiscoNSE, localMedia.get());
#endif
  }

#if OPAL_T38_CAPABILITY
  ownerCall.ResetSwitchingT38();
#endif

#if OPAL_RTP_FEC
  OpalMediaFormatList fec = NegotiateFECMediaFormats(*mediaSession);
  for (OpalMediaFormatList::iterator it = fec.begin(); it != fec.end(); ++it)
    localMedia->AddMediaFormat(*it);
#endif

	PTRACE(4, "Answered offer for media type " << mediaType << ' ' << localMedia->GetMediaAddress());
  return localMedia.release();
}


bool OpalSDPConnection::OnReceivedAnswerSDP(const SDPSessionDescription & sdp, bool & multipleFormats)
{
  m_answerFormatList = sdp.GetMediaFormats();
  AdjustMediaFormats(false, NULL, m_answerFormatList);

  unsigned sessionCount = sdp.GetMediaDescriptions().GetSize();

  bool ok = false;
  for (unsigned session = 1; session <= sessionCount; ++session) {
    if (OnReceivedAnswerSDPSession(sdp, session, multipleFormats))
      ok = true;
    else {
      OpalMediaStreamPtr stream;
      if ((stream = GetMediaStream(session, false)) != NULL)
        stream->Close();
      if ((stream = GetMediaStream(session, true)) != NULL)
        stream->Close();
    }
  }

  m_answerFormatList.RemoveAll();

  /* Shut down any media that is in a session not mentioned in a re-INVITE.
     While the SIP/SDP specification says this shouldn't happen, it does
     anyway so we need to deal. */
  for (OpalMediaStreamPtr stream(mediaStreams, PSafeReference); stream != NULL; ++stream) {
    if (stream->GetSessionID() > sessionCount)
      stream->Close();
  }

  if (ok)
    StartMediaStreams();

  return ok;
}


bool OpalSDPConnection::OnReceivedAnswerSDPSession(const SDPSessionDescription & sdp, unsigned sessionId, bool & multipleFormats)
{
  SDPMediaDescription * mediaDescription = sdp.GetMediaDescriptionByIndex(sessionId);
  if (!PAssert(mediaDescription != NULL, "SDP Media description list changed"))
    return false;

  OpalMediaType mediaType = mediaDescription->GetMediaType();
  
  PTRACE(4, "Processing received SDP media description for " << mediaType);

  /* Get the media the remote has answered to our offer. Remove the media
     formats we do not support, in case the remote is insane and replied
     with something we did not actually offer. */
  if (!m_answerFormatList.HasType(mediaType)) {
    PTRACE(2, "Could not find supported media formats in SDP media description for session " << sessionId);
    return false;
  }

  // Set up the media session, e.g. RTP
  bool remoteChanged = false;
  OpalTransportAddress localAddress;
  OpalMediaSession * mediaSession = SetUpMediaSession(sessionId, mediaType, *mediaDescription, localAddress, remoteChanged);
  if (mediaSession == NULL)
    return false;

#if OPAL_SRTP
  OpalMediaCryptoKeyList keys = mediaDescription->GetCryptoKeys();
  if (!keys.IsEmpty()) {
    // Set our rx keys to remotes tx keys indicated in SDP
    if (!mediaSession->ApplyCryptoKey(keys, true)) {
      PTRACE(2, "Incompatible crypto suite(s) for " << mediaType << " session " << sessionId);
      return false;
    }

    // Now match up the tag number on our offered keys
    OpalMediaCryptoKeyList & offeredKeys = mediaSession->GetOfferedCryptoKeys();
    OpalMediaCryptoKeyList::iterator it;
    for (it = offeredKeys.begin(); it != offeredKeys.end(); ++it) {
      if (it->GetTag() == keys.front().GetTag())
        break;
    }
    if (it == offeredKeys.end()) {
      PTRACE(2, "Remote selected crypto suite(s) we did not offer for " << mediaType << " session " << sessionId);
      return false;
    }

    keys.RemoveAll();
    keys.Append(&*it);

    offeredKeys.DisallowDeleteObjects(); // Can't have in two lists and both dispose of pointer
    offeredKeys.erase(it);
    offeredKeys.AllowDeleteObjects();
    offeredKeys.RemoveAll();

    if (!mediaSession->ApplyCryptoKey(keys, false)) {
      PTRACE(2, "Incompatible crypto suite(s) for " << mediaType << " session " << sessionId);
      return false;
    }
  }
#endif // OPAL_SRTP

  SDPMediaDescription::Direction otherSidesDir = sdp.GetDirection(sessionId);

  // Check if we had a stream and the remote has either changed the codec or
  // changed the direction of the stream
  OpalMediaStreamPtr sendStream = GetMediaStream(sessionId, false);
  bool sendDisabled = (otherSidesDir&SDPMediaDescription::RecvOnly) == 0;
  PauseOrCloseMediaStream(sendStream, m_answerFormatList, remoteChanged, sendDisabled);

  OpalMediaStreamPtr recvStream = GetMediaStream(sessionId, true);
  bool recvDisabled = (otherSidesDir&SDPMediaDescription::SendOnly) == 0;
  PauseOrCloseMediaStream(recvStream, m_answerFormatList, remoteChanged, recvDisabled);

  // Then open the streams if the direction allows and if needed
  // If already open then update to new parameters/payload type

  if (recvStream == NULL) {
    PTRACE(5, "Opening rx " << mediaType << " stream from answer SDP");
    if (ownerCall.OpenSourceMediaStreams(*this,
                                         mediaType,
                                         sessionId,
                                         OpalMediaFormat(),
#if OPAL_VIDEO
                                         mediaDescription->GetContentRole(),
#endif
                                         recvDisabled))
      recvStream = GetMediaStream(sessionId, true);
    if (!recvDisabled && recvStream == NULL)
      OnMediaStreamOpenFailed(true);
  }

  if (sendStream == NULL) {
    PSafePtr<OpalConnection> otherParty = GetOtherPartyConnection();
    if (otherParty == NULL)
      return false;

    PTRACE(5, "Opening tx " << mediaType << " stream from answer SDP");
    if (ownerCall.OpenSourceMediaStreams(*otherParty,
                                          mediaType,
                                          sessionId,
                                          OpalMediaFormat(),
#if OPAL_VIDEO
                                          mediaDescription->GetContentRole(),
#endif
                                          sendDisabled))
      sendStream = GetMediaStream(sessionId, false);
    if (!sendDisabled && sendStream == NULL && !otherParty->IsOnHold(true))
      OnMediaStreamOpenFailed(false);
  }

  PINDEX maxFormats = 1;
  if (mediaType == OpalMediaType::Audio()) {
    if (SetNxECapabilities(m_rfc2833Handler, m_localMediaFormats, m_answerFormatList, OpalRFC2833))
      ++maxFormats;
#if OPAL_T38_CAPABILITY
    if (SetNxECapabilities(m_ciscoNSEHandler, m_localMediaFormats, m_answerFormatList, OpalCiscoNSE))
      ++maxFormats;
#endif
  }

  if (mediaDescription->GetSDPMediaFormats().GetSize() > maxFormats)
    multipleFormats = true;

#if OPAL_RTP_FEC
  NegotiateFECMediaFormats(*mediaSession);
#endif

	PTRACE_IF(3, otherSidesDir == SDPMediaDescription::Inactive, "No streams opened as " << mediaType << " inactive");
  return true;
}


bool OpalSDPConnection::OnHoldStateChanged(bool)
{
  return true;
}


void OpalSDPConnection::OnMediaStreamOpenFailed(bool)
{
}

#endif // OPAL_SDP


///////////////////////////////////////////////////////////////////////////////
