/*
 * sipcon.cxx
 *
 * Session Initiation Protocol connection.
 *
 * Open Phone Abstraction Library (OPAL)
 *
 * Copyright (c) 2000 Equivalence Pty. Ltd.
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
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Log: sipcon.cxx,v $
 * Revision 1.2025  2003/03/06 03:57:47  robertj
 * IVR support (work in progress) requiring large changes everywhere.
 *
 * Revision 2.23  2002/11/10 11:33:20  robertj
 * Updated to OpenH323 v1.10.3
 *
 * Revision 2.22  2002/09/12 06:58:34  robertj
 * Removed protocol prefix strings as static members as has problems with
 *   use in DLL environment.
 *
 * Revision 2.21  2002/07/01 04:56:33  robertj
 * Updated to OpenH323 v1.9.1
 *
 * Revision 2.20  2002/06/16 02:26:37  robertj
 * Fixed creation of RTP session for incoming calls, thanks Ted Szoczei
 *
 * Revision 2.19  2002/04/17 07:22:54  robertj
 * Added identifier for conenction in OnReleased trace message.
 *
 * Revision 2.18  2002/04/16 09:04:18  robertj
 * Fixed setting of target URI from Contact regardless of route set
 *
 * Revision 2.17  2002/04/16 07:53:15  robertj
 * Changes to support calls through proxies.
 *
 * Revision 2.16  2002/04/15 08:53:58  robertj
 * Fixed correct setting of jitter buffer size in RTP media stream.
 * Fixed starting incoming (not outgoing!) media on call progress.
 *
 * Revision 2.15  2002/04/10 06:58:31  robertj
 * Fixed incorrect return value when starting INVITE transactions.
 *
 * Revision 2.14  2002/04/10 03:14:35  robertj
 * Moved code for handling media bypass address resolution into ancestor as
 *   now done ths same way in both SIP and H.323.
 * Major changes to RTP session management when initiating an INVITE.
 * Improvements in error handling and transaction cancelling.
 *
 * Revision 2.13  2002/04/09 07:26:20  robertj
 * Added correct error return if get transport failure to remote endpoint.
 *
 * Revision 2.12  2002/04/09 01:19:41  robertj
 * Fixed typo on changing "callAnswered" to better description of "originating".
 *
 * Revision 2.11  2002/04/09 01:02:14  robertj
 * Fixed problems with restarting INVITE on  authentication required response.
 *
 * Revision 2.10  2002/04/08 02:40:13  robertj
 * Fixed issues with using double originate call, eg from simple app command line.
 *
 * Revision 2.9  2002/04/05 10:42:04  robertj
 * Major changes to support transactions (UDP timeouts and retries).
 *
 * Revision 2.8  2002/03/18 08:13:42  robertj
 * Added more logging.
 * Removed proxyusername/proxypassword parameters from URL passed on.
 * Fixed GNU warning.
 *
 * Revision 2.7  2002/03/15 10:55:28  robertj
 * Added ability to specify proxy username/password in URL.
 *
 * Revision 2.6  2002/03/08 06:28:03  craigs
 * Changed to allow Authorisation to be included in other PDUs
 *
 * Revision 2.5  2002/02/19 07:53:17  robertj
 * Restructured media bypass functions to fix problems with RFC2833.
 *
 * Revision 2.4  2002/02/13 04:55:44  craigs
 * Fixed problem with endless loop if proxy keeps failing authentication with 407
 *
 * Revision 2.3  2002/02/11 07:33:36  robertj
 * Changed SDP to use OpalTransport for hosts instead of IP addresses/ports
 * Added media bypass for streams between compatible protocols.
 *
 * Revision 2.2  2002/02/01 05:47:55  craigs
 * Removed :: from in front of isspace that confused gcc 2.96
 *
 * Revision 2.1  2002/02/01 04:53:01  robertj
 * Added (very primitive!) SIP support.
 *
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "sipcon.h"
#endif

#include <sip/sipcon.h>

#include <sip/sipep.h>
#include <codec/rfc2833.h>
#include <opal/manager.h>
#include <opal/call.h>
#include <opal/patch.h>


typedef void (SIPConnection::* SIPMethodFunction)(SIP_PDU & pdu);


#define new PNEW


////////////////////////////////////////////////////////////////////////////

SIPConnection::SIPConnection(OpalCall & call,
                             SIPEndPoint & ep,
                             const PString & token,
                             const SIPURL & destination,
                             OpalTransport * inviteTransport)
  : OpalConnection(call, ep, token),
    endpoint(ep),
    authentication(endpoint.GetRegistrationName(),
                   endpoint.GetRegistrationPassword()),
    pduSemaphore(0, P_MAX_INDEX)
{
  localPartyName = endpoint.GetRegistrationName();

  targetAddress = destination;

  PStringToString params = targetAddress.GetParamVars();
  if (params.Contains("proxyusername") && params.Contains("proxypassword")) {
    localPartyName = params["proxyusername"];
    authentication.SetUsername(localPartyName);
    targetAddress.SetParamVar("proxyusername", PString::Empty());
    authentication.SetPassword(params["proxypassword"]);
    targetAddress.SetParamVar("proxypassword", PString::Empty());
    PTRACE(3, "SIP\tExtracted proxy authentication user=\"" << localPartyName << '"');
  }

  remotePartyAddress = targetAddress.AsQuotedString();

  if (inviteTransport == NULL)
    transport = NULL;
  else {
    transport = inviteTransport->GetLocalAddress().CreateTransport(
                                         endpoint, OpalTransportAddress::HostOnly);
    transport->SetBufferSize(SIP_PDU::MaxSize); // Maximum possible PDU size
  }

  originalInvite = NULL;
  pduHandler = NULL;
  lastSentCSeq = 0;
  releaseMethod = ReleaseWithNothing;

  transactions.DisallowDeleteObjects();

  PTRACE(3, "SIP\tCreated connection.");
}


SIPConnection::~SIPConnection()
{
  delete originalInvite;
  delete transport;

  PTRACE(3, "SIP\tDeleted connection.");
}


BOOL SIPConnection::OnReleased()
{
  PTRACE(3, "SIP\tOnReleased: " << *this);

  switch (releaseMethod) {
    case ReleaseWithNothing :
      break;

    case ReleaseWithResponse :
      switch (callEndReason) {
        case EndedByAnswerDenied :
          SendResponseToINVITE(SIP_PDU::Failure_Decline);
          break;
        case EndedByLocalBusy :
          SendResponseToINVITE(SIP_PDU::Failure_BusyHere);
          break;
        case EndedByCallerAbort :
          SendResponseToINVITE(SIP_PDU::Failure_RequestTerminated);
          break;
        default :
          SendResponseToINVITE(SIP_PDU::Failure_BadGateway);
      }
      break;

    case ReleaseWithBYE :
      {
        SIPTransaction bye(*this, *transport, SIP_PDU::Method_BYE);
        bye.Wait();
      }
      break;

    case ReleaseWithCANCEL :
      for (PINDEX i = 0; i < invitations.GetSize(); i++) {
        if (invitations[i].SendCANCEL())
          invitations[i].Wait();
      }
  }

  LockOnRelease();

  // send the appropriate form 
  PTRACE(2, "SIP\tReceived OnReleased in phase " << phase);

  invitations.RemoveAll();

  if (pduHandler != NULL) {
    pduSemaphore.Signal();
    pduHandler->WaitForTermination();
    delete pduHandler;
  }

  if (transport != NULL)
    transport->CloseWait();

  return OpalConnection::OnReleased();
}


BOOL SIPConnection::SetAlerting(const PString & /*calleeName*/, BOOL /*withMedia*/)
{
  if (IsOriginating()) {
    PTRACE(2, "SIP\tSetAlerting ignored on call we originated.");
    return TRUE;
  }

  PTRACE(2, "SIP\tSetAlerting");
  if (!Lock())
    return FALSE;

  if (phase != SetUpPhase) 
    return FALSE;

  SendResponseToINVITE(SIP_PDU::Information_Ringing);
  phase = AlertingPhase;

  Unlock();

  return TRUE;
}


BOOL SIPConnection::SetConnected()
{
  if (IsOriginating()) {
    PTRACE(2, "SIP\tSetConnected ignored on call we originated.");
    return TRUE;
  }

  PINDEX i;
  PTRACE(2, "SIP\tSetConnected");

  // create an RTP session number
  // for now, just hard code it
  unsigned rtpSessionId = OpalMediaFormat::DefaultAudioSessionID;

  SDPSessionDescription sdpOut(GetLocalAddress());

  // get the remote media formats
  SDPSessionDescription & sdpIn = originalInvite->GetSDP();


  // look for audio sessions only, at this stage
  SDPMediaDescription::MediaType rtpMediaType = SDPMediaDescription::Audio;


  // if no matching media type, return FALSE
  SDPMediaDescription * incomingMedia = sdpIn.GetMediaDescription(rtpMediaType);
  if (incomingMedia == NULL) {
    PTRACE(2, "SIP\tCould not find matching media type");
    return FALSE;
  }

  // create the list of Opal format names for the remote end
  remoteFormatList = incomingMedia->GetMediaFormats();

  // find the payload type used for telephone-event, if present
  const SDPMediaFormatList & sdpMediaList = incomingMedia->GetSDPMediaFormats();

  BOOL hasTelephoneEvent = FALSE;
  for (i = 0; !hasTelephoneEvent && (i < sdpMediaList.GetSize()); i++) {
    if (sdpMediaList[i].GetEncodingName() == "telephone-event") {
      rfc2833Handler->SetPayloadType(sdpMediaList[i].GetPayloadType());
      hasTelephoneEvent = TRUE;
      remoteFormatList += OpalRFC2833;
    }
  }

  // create an RTP session
  RTP_UDP * rtpSession = (RTP_UDP *)UseRTPSession(rtpSessions, rtpSessionId);

  // set the remote addresses
  PIPSocket::Address ip;
  WORD port;
  incomingMedia->GetTransportAddress().GetIpAndPort(ip, port);
  if (!rtpSession->SetRemoteSocketInfo(ip, port, TRUE)) {
    PTRACE(1, "SIP\tCannot set remote ports on RTP session");
    delete rtpSession;
    return FALSE;
  }


  // look for matching codec in peer connection
  if (!GetCall().OpenSourceMediaStreams(remoteFormatList, rtpSessionId)) {
    PTRACE(2, "SIP\tCould not find compatible codecs");
    return FALSE;
  }

  // find out what media stream was opened
  // and get the payload type that was selected
  if (mediaStreams.GetSize() < 1) {
    PTRACE(2, "SIP\tNo media streams open");
    return FALSE;
  }

  // construct a new media session list with the selected format
  SDPMediaDescription * localMedia = new SDPMediaDescription(
                            GetLocalAddress(rtpSession->GetLocalDataPort()), rtpMediaType);
  
  for (i = 0; i < mediaStreams.GetSize(); i++) {
    OpalMediaStream & mediaStream = mediaStreams[i];
    localMedia->AddMediaFormat(mediaStream.GetMediaFormat());
    if (hasTelephoneEvent) {
      OpalMediaPatch * patch = mediaStream.GetPatch();
      if (patch != NULL) {
        if (mediaStream.IsSource())
          patch->AddFilter(rfc2833Handler->GetReceiveHandler(), mediaStream.GetMediaFormat());
        else
          patch->AddFilter(rfc2833Handler->GetTransmitHandler());
      }
    }
  }

  if (hasTelephoneEvent)
    localMedia->AddSDPMediaFormat(new SDPMediaFormat("0-15", rfc2833Handler->GetPayloadType()));

  sdpOut.AddMediaDescription(localMedia);

  // send the response
  SIP_PDU response(*originalInvite, SIP_PDU::Successful_OK);
  response.SetSDP(sdpOut);
  response.Write(*transport);
  phase = ConnectedPhase;
  
  return TRUE;
}


OpalTransportAddress SIPConnection::GetLocalAddress(WORD port) const
{
  PIPSocket::Address localIP;
  if (!transport->GetLocalAddress().GetIpAddress(localIP)) {
    PTRACE(1, "SIP\tNot using IP transport");
    return OpalTransportAddress();
  }

  PIPSocket::Address remoteIP;
  if (!transport->GetRemoteAddress().GetIpAddress(remoteIP)) {
    PTRACE(1, "SIP\tNot using IP transport");
    return OpalTransportAddress();
  }

  endpoint.GetManager().TranslateIPAddress(localIP, remoteIP);
  return OpalTransportAddress(localIP, port, "udp");
}


RTP_Session * SIPConnection::UseRTPSession(RTP_SessionManager & rtpSessions,
                                           unsigned rtpSessionId)
{
  // make sure this RTP session does not already exist
  RTP_Session * oldSession = rtpSessions.UseSession(rtpSessionId);
  if (oldSession != NULL)
    return oldSession;

  // create an RTP session
  RTP_UDP * rtpSession = new RTP_UDP(rtpSessionId);

  // add the RTP session to the RTP session manager
  rtpSessions.AddSession(rtpSession);

  PIPSocket::Address ip;
  transport->GetLocalAddress().GetIpAddress(ip);

  // open an RTP session
  OpalManager & mgr = endpoint.GetManager();
  WORD firstPort = mgr.GetRtpIpPortPair();
  WORD nextPort = firstPort;
  while (!rtpSession->Open(ip, nextPort, nextPort, mgr.GetRtpIpTypeofService())) {
    nextPort = mgr.GetRtpIpPortPair();
    if (nextPort == firstPort) {
      PTRACE(1, "SIP\tCould not open RTP session");
      delete rtpSession;
      return NULL;
    }
  }
 
  return rtpSession;
}


OpalMediaFormatList SIPConnection::GetMediaFormats() const
{
  return remoteFormatList;
}


OpalMediaStream * SIPConnection::CreateMediaStream(BOOL isSource, unsigned sessionID)
{
  if (ownerCall.IsMediaBypassPossible(*this, sessionID))
    return new OpalNullMediaStream(isSource, sessionID);

  if (rtpSessions.GetSession(sessionID) == NULL)
    return NULL;

  return new OpalRTPMediaStream(isSource, *rtpSessions.GetSession(sessionID),
                                endpoint.GetManager().GetMinAudioJitterDelay(),
                                endpoint.GetManager().GetMaxAudioJitterDelay());
}


BOOL SIPConnection::IsMediaBypassPossible(unsigned sessionID) const
{
  PTRACE(3, "SIP\tIsMediaBypassPossible: session " << sessionID);

  return sessionID == OpalMediaFormat::DefaultAudioSessionID ||
         sessionID == OpalMediaFormat::DefaultVideoSessionID;
}


BOOL SIPConnection::WriteINVITE(OpalTransport & transport, PObject * param)
{
  SIPConnection & connection = *(SIPConnection *)param;

  connection.SetLocalPartyAddress();

  SIPTransaction * invite = new SIPInvite(connection, transport);
  if (invite->Start()) {
    connection.invitations.Append(invite);
    return TRUE;
  }

  PTRACE(2, "SIP\tDid not start INVITE transaction on " << transport);
  return FALSE;
}


BOOL SIPConnection::SetUpConnection()
{
  PTRACE(2, "SIP\tSetUpConnection: " << remotePartyAddress);

  originating = TRUE;

  OpalTransportAddress address = targetAddress.GetHostAddress();

  delete transport;
  transport = address.CreateTransport(endpoint, OpalTransportAddress::NoBinding);
  if (transport == NULL) {
    PTRACE(1, "SIP\tCould not create transport from " << address);
    return FALSE;
  }

  transport->SetBufferSize(SIP_PDU::MaxSize);
  if (!transport->ConnectTo(address)) {
    PTRACE(1, "SIP\tCould not connect to " << address << " - " << transport->GetErrorText());
    Release(EndedByTransportFail);
    return FALSE;
  }

  if (!transport->WriteConnect(WriteINVITE, this)) {
    PTRACE(1, "SIP\tCould not write to " << address << " - " << transport->GetErrorText());
    Release(EndedByTransportFail);
    return FALSE;
  }

  releaseMethod = ReleaseWithCANCEL;
  return TRUE;
}


SDPSessionDescription * SIPConnection::BuildSDP(RTP_SessionManager & rtpSessions,
                                                unsigned rtpSessionId)
{
  OpalTransportAddress localAddress;
  RTP_DataFrame::PayloadTypes ntePayloadCode = RTP_DataFrame::IllegalPayloadType;

  if (ownerCall.IsMediaBypassPossible(*this, rtpSessionId)) {
    OpalConnection * otherParty = GetCall().GetOtherPartyConnection(*this);
    if (otherParty != NULL) {
      MediaInformation info;
      if (otherParty->GetMediaInformation(rtpSessionId, info)) {
        localAddress = info.data;
        ntePayloadCode = info.rfc2833;
      }
    }
  }

  if (localAddress.IsEmpty()) {
    // create the SDP definition for the audio channel
    RTP_UDP * audioRTPSession = (RTP_UDP*)UseRTPSession(rtpSessions, rtpSessionId);
    localAddress = GetLocalAddress(audioRTPSession->GetLocalDataPort());
  }

  SDPSessionDescription * sdp = new SDPSessionDescription(localAddress);

  SDPMediaDescription * localMedia = new SDPMediaDescription(localAddress, SDPMediaDescription::Audio);

  // Set format if we have an RTP payload type for RFC2833
  if (ntePayloadCode != RTP_DataFrame::IllegalPayloadType) {
    PTRACE(3, "SIP\tUsing bypass RTP payload " << ntePayloadCode << " for NTE");
    localMedia->AddSDPMediaFormat(new SDPMediaFormat("0-15", ntePayloadCode));
  }
  else {
    ntePayloadCode = rfc2833Handler->GetPayloadType();
    if (ntePayloadCode == RTP_DataFrame::IllegalPayloadType) {
      ntePayloadCode = OpalRFC2833.GetPayloadType();
    }

    if (ntePayloadCode != RTP_DataFrame::IllegalPayloadType) {
      PTRACE(3, "SIP\tUsing RTP payload " << ntePayloadCode << " for NTE");

      // create and add the NTE media format
      localMedia->AddSDPMediaFormat(new SDPMediaFormat("0-15", ntePayloadCode));
    }
    else {
      PTRACE(2, "SIP\tCould not allocate dynamic RTP payload for NTE");
    }
  }

  rfc2833Handler->SetPayloadType(ntePayloadCode);


  // add the audio formats
  OpalMediaFormatList formats = ownerCall.GetMediaFormats(*this);
  localMedia->AddMediaFormats(formats);

  // add in SDP records
  sdp->AddMediaDescription(localMedia);

  return sdp;
}


void SIPConnection::SetLocalPartyAddress()
{
  SIPURL myAddress(GetLocalPartyName(), transport->GetLocalAddress(), endpoint.GetDefaultSignalPort());
  SetLocalPartyAddress(myAddress.AsString());
}


void SIPConnection::OnTransactionFailed(SIPTransaction & transaction)
{
  if (transaction.GetMethod() != SIP_PDU::Method_INVITE)
    return;

  for (PINDEX i = 0; i < invitations.GetSize(); i++) {
    if (!invitations[i].IsFailed())
      return;
  }

  // All invitations failed, die now
  Release(EndedByConnectFail);
}


void SIPConnection::OnReceivedPDU(SIP_PDU & pdu)
{
  SIPTransaction * transaction = transactions.GetAt(pdu.GetTransactionID());
  PTRACE(4, "SIP\tHandling PDU " << pdu << " (" <<
            (transaction != NULL ? "with" : "no") << " transaction)");

  switch (pdu.GetMethod()) {
    case SIP_PDU::Method_INVITE :
      OnReceivedINVITE(pdu);
      break;
    case SIP_PDU::Method_ACK :
      OnReceivedACK(pdu);
      break;
    case SIP_PDU::Method_CANCEL :
      OnReceivedCANCEL(pdu);
      break;
    case SIP_PDU::Method_BYE :
      OnReceivedBYE(pdu);
      break;
    case SIP_PDU::Method_OPTIONS :
      OnReceivedOPTIONS(pdu);
      break;
    case SIP_PDU::Method_REGISTER :
      // Shouldn't have got this!
      break;
    case SIP_PDU::NumMethods :  // if no method, must be response
      if (transaction != NULL)
        transaction->OnReceivedResponse(pdu);
      break;
  }
}


void SIPConnection::OnReceivedResponse(SIPTransaction & transaction, SIP_PDU & response)
{
  PINDEX i;

  if (transaction.GetMethod() == SIP_PDU::Method_INVITE) {
    // Have a response to the INVITE, so CANCEL al the other invitations sent.
    for (i = 0; i < invitations.GetSize(); i++) {
      if (&invitations[i] != &transaction)
        invitations[i].SendCANCEL();
    }

    // Save the sessions etc we are actually using
    rtpSessions = ((SIPInvite &)transaction).GetSessionManager();
    localPartyAddress = transaction.GetMIME().GetFrom();
    remotePartyAddress = response.GetMIME().GetTo();

    // Have a response to the INVITE, so end Connect mode on the transport
    transaction.GetTransport().EndConnect(transaction.GetLocalAddress());

    // Get teh route set from the Record-Route response field (in reverse order)
    PStringList recordRoute = response.GetMIME().GetRecordRoute();
    routeSet.RemoveAll();
    for (i = recordRoute.GetSize(); i > 0; i--)
      routeSet.AppendString(recordRoute[i-1]);
  }

  // If current SIP dialog does not have a route set (via Record-Route) and 
  // this response had a contact field then start using that address for all
  // future communication with remote SIP endpoint
  PString contact = response.GetMIME().GetContact();
  if (!contact)
    targetAddress = contact;

  switch (response.GetStatusCode()) {
    case SIP_PDU::Information_Session_Progress :
      OnReceivedSessionProgress(response);
      break;

    case SIP_PDU::Failure_UnAuthorised :
    case SIP_PDU::Failure_ProxyAuthenticationRequired :
      OnReceivedAuthenticationRequired(transaction, response);
      break;

    case SIP_PDU::Redirection_MovedTemporarily :
      OnReceivedRedirection(response);
      break;

    case SIP_PDU::Failure_BusyHere :
      Release(EndedByRemoteBusy);
      break;

    default :
      switch (response.GetStatusCode()/100) {
        case 1 :
          // Do nothing on 1xx
          break;
        case 2 :
          OnReceivedOK(transaction, response);
          break;
        default :
          // All other final responses cause a call end.
          Release(EndedByRefusal);
      }
  }
}


void SIPConnection::OnReceivedINVITE(SIP_PDU & request)
{
  // ignore duplicate INVITES
  if (originalInvite != NULL) {
    PTRACE(2, "SIP\tIgnoring duplicate INVITE from " << request.GetURI() << " for " << *this);
    return;
  }

  originalInvite = new SIP_PDU(request);
  releaseMethod = ReleaseWithResponse;

  SIPMIMEInfo & mime = originalInvite->GetMIME();
  remotePartyAddress = mime.GetFrom();
  localPartyAddress  = mime.GetTo() + ";tag=XXXXX";
  mime.SetTo(localPartyAddress);

  PString contact = mime.GetContact();
  if (contact.IsEmpty()) {
    targetAddress = mime.GetFrom();
    PString via = mime.GetVia();
    transport->SetRemoteAddress(via.Mid(via.FindLast(' ')));
  }
  else {
    targetAddress = contact;
    transport->SetRemoteAddress(targetAddress.GetHostAddress());
  }
  targetAddress.AdjustForRequestURI();

  // indicate the other is to start ringing
  if (!OnIncomingConnection()) {
    PTRACE(2, "SIP\tOnIncomingConnection failed for INVITE from " << request.GetURI() << " for " << *this);
    Release();
    return;
  }

  PTRACE(2, "SIP\tOnIncomingConnection succeeded for INVITE from " << request.GetURI() << " for " << *this);
  phase = SetUpPhase;
  ownerCall.OnSetUp(*this);
}


void SIPConnection::OnReceivedACK(SIP_PDU & /*request*/)
{
  PTRACE(2, "SIP\tACK received");

  // start all of the media threads for the connection
  StartMediaStreams();
  phase = EstablishedPhase;
  releaseMethod = ReleaseWithBYE;
}


void SIPConnection::OnReceivedOPTIONS(SIP_PDU & /*request*/)
{
  PTRACE(1, "SIP\tOPTIONS not yet supported");
}


void SIPConnection::OnReceivedBYE(SIP_PDU & request)
{
  PTRACE(2, "SIP\tBYE received for call " << request.GetMIME().GetCallID());
  SIP_PDU response(request, SIP_PDU::Successful_OK);
  response.Write(*transport);
  releaseMethod = ReleaseWithNothing;
  Release(EndedByRemoteUser);
}


void SIPConnection::OnReceivedCANCEL(SIP_PDU & request)
{
  // Currently only handle CANCEL requests for the original INVITE that
  // created this connection, all else ignored
  if (originalInvite == NULL ||
      request.GetMIME().GetTo() != originalInvite->GetMIME().GetTo() ||
      request.GetMIME().GetFrom() != originalInvite->GetMIME().GetFrom() ||
      request.GetMIME().GetCSeqIndex() != originalInvite->GetMIME().GetCSeqIndex()) {
    PTRACE(1, "SIP\tUnattached " << request << " received for " << *this);
    SIP_PDU response(request, SIP_PDU::Failure_TransactionDoesNotExist);
    response.Write(*transport);
    return;
  }

  PTRACE(2, "SIP\tCancel received for " << *this);

  SIP_PDU response(request, SIP_PDU::Successful_OK);
  response.Write(*transport);
  Release(EndedByCallerAbort);
}


void SIPConnection::OnReceivedTrying(SIP_PDU & /*response*/)
{
  PTRACE(2, "SIP\tReceived Trying response");
}


void SIPConnection::OnReceivedSessionProgress(SIP_PDU & response)
{
  PTRACE(2, "SIP\tReceived Session Progress response");

  OnReceivedSDP(response);

  OnAlerting();
  phase = AlertingPhase;

  PTRACE(3, "SIP\tStarting receive media to annunciate remote progress tones");
  for (PINDEX i = 0; i < mediaStreams.GetSize(); i++) {
    if (mediaStreams[i].IsSource())
      mediaStreams[i].Start();
  }
}


void SIPConnection::OnReceivedRedirection(SIP_PDU & /*response*/)
{
  // set the remote party address, targetAddress already set to Contact field
  remotePartyAddress = targetAddress.AsQuotedString();

  // send a new INVITE
  SIPTransaction * invite = new SIPInvite(*this, *transport);
  if (invite->Start())
    invitations.Append(invite);
  else {
    delete invite;
    PTRACE(1, "SIP\tCould not restart INVITE for Redirection");
  }
}


void SIPConnection::OnReceivedAuthenticationRequired(SIPTransaction & transaction,
                                                     SIP_PDU & response)
{
  BOOL isProxy = response.GetStatusCode() == SIP_PDU::Failure_ProxyAuthenticationRequired;
#if PTRACING
  const char * proxyTrace = isProxy ? "Proxy " : "";
#endif

  if (authentication.IsValid()) {
    PTRACE(1, "SIP\tAlready done INVITE for " << proxyTrace << "Authentication Required, aborting call");
    Release(EndedBySecurityDenial);
    return;
  }

  if (transaction.GetMethod() != SIP_PDU::Method_INVITE) {
    PTRACE(1, "SIP\tCannot do " << proxyTrace << "Authentication Required for non INVITE");
    return;
  }

  PTRACE(2, "SIP\tReceived " << proxyTrace << "Authentication Required response");

  if (!authentication.Parse(response.GetMIME()(isProxy ? "Proxy-Authenticate"
                                                       : "WWW-Authenticate"),
                                               isProxy)) {
    Release(EndedBySecurityDenial);
    return;
  }

  // make sure the To field is the same as the original
  remotePartyAddress = targetAddress.AsQuotedString();

  // Restart the transaction with new authentication info
  SIPTransaction * invite = new SIPInvite(*this, *transport);
  if (invite->Start())
    invitations.Append(invite);
  else {
    delete invite;
    PTRACE(1, "SIP\tCould not restart INVITE for " << proxyTrace << "Authentication Required");
  }
}


void SIPConnection::OnReceivedOK(SIPTransaction & transaction, SIP_PDU & response)
{
  if (transaction.GetMethod() != SIP_PDU::Method_INVITE) {
    PTRACE(3, "SIP\tReceived OK response for non INVITE");
    return;
  }

  if (phase == EstablishedPhase) {
    PTRACE(2, "SIP\tIgnoring repeated INVITE OK response");
    return;
  }

  PTRACE(2, "SIP\tReceived INVITE OK response");

  OnReceivedSDP(response);

  OnConnected();

  phase = EstablishedPhase;
  StartMediaStreams();
  releaseMethod = ReleaseWithBYE;
}


void SIPConnection::OnReceivedSDP(SIP_PDU & pdu)
{
  if (!pdu.HasSDP())
    return;

  unsigned rtpSessionId = OpalMediaFormat::DefaultAudioSessionID;

  SDPSessionDescription & sdp = pdu.GetSDP();

  SDPMediaDescription * mediaDescription = sdp.GetMediaDescription(SDPMediaDescription::Audio);
  if (mediaDescription == NULL) {
    PTRACE(1, "SIP\tCould not find SDP media description for audio");
    return;
  }

  OpalTransportAddress address = mediaDescription->GetTransportAddress();
  if (ownerCall.IsMediaBypassPossible(*this, rtpSessionId))
    mediaTransportAddresses.SetAt(rtpSessionId, new OpalTransportAddress(address));
  else {
    PIPSocket::Address ip;
    WORD port;
    address.GetIpAndPort(ip, port);

    RTP_UDP * rtpSession = (RTP_UDP *)UseSession(rtpSessionId);
    if (rtpSession == NULL) {
      PTRACE(1, "SIP\tSession in response that we never offered!");
      return;
    }

    if (!rtpSession->SetRemoteSocketInfo(ip, port, TRUE)) {
      PTRACE(1, "SIP\tCould not set RTP remote socket");
      return;
    }
  }

  remoteFormatList = mediaDescription->GetMediaFormats();
  remoteFormatList += OpalRFC2833;

  if (!ownerCall.OpenSourceMediaStreams(remoteFormatList, rtpSessionId)) {
    PTRACE(2, "SIP\tCould not open media streams for audio");
    return;
  }
}


void SIPConnection::QueuePDU(SIP_PDU * pdu)
{
  PAssertNULL(pdu);

  PTRACE(4, "SIP\tQueueing PDU: " << *pdu);
  pduQueue.Enqueue(pdu);
  pduSemaphore.Signal();

  if (pduHandler == NULL)
    pduHandler = PThread::Create(PCREATE_NOTIFIER(HandlePDUsThreadMain), 0,
                                 PThread::NoAutoDeleteThread,
                                 PThread::NormalPriority,
                                 "SIP Handler:%x");
}


void SIPConnection::HandlePDUsThreadMain(PThread &, INT)
{
  PTRACE(2, "SIP\tPDU handler thread started.");

  for(;;) {
    PTRACE(4, "SIP\tAwaiting next PDU.");
    pduSemaphore.Wait();

    if (!Lock())
      break;

    SIP_PDU * pdu = pduQueue.Dequeue();
    if (pdu != NULL) {
      OnReceivedPDU(*pdu);
      delete pdu;
    }

    Unlock();
  }

  PTRACE(2, "SIP\tPDU handler thread finished.");
}


void SIPConnection::SendResponseToINVITE(SIP_PDU::StatusCodes code, const char * extra)
{
  if (originalInvite != NULL) {
    SIP_PDU response(*originalInvite, code, extra);
    response.Write(*transport);
  }
}


// End of file ////////////////////////////////////////////////////////////////
