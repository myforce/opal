/*
 * sipep.cxx
 *
 * Session Initiation Protocol endpoint.
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
 * $Log: sipep.cxx,v $
 * Revision 1.2033  2004/12/22 18:56:39  dsandras
 * Ignore ACK's received outside the context of a connection (e.g. when sending a non-2XX answer to an INVITE).
 *
 * Revision 2.31  2004/12/17 12:06:52  dsandras
 * Added error code to OnRegistrationFailed. Made Register/Unregister wait until the transaction is over. Fixed Unregister so that the SIPRegister is used as a pointer or the object is deleted at the end of the function and make Opal crash when transactions are cleaned. Reverted part of the patch that was sending authentication again when it had already been done on a Register.
 *
 * Revision 2.30  2004/12/12 13:42:31  dsandras
 * - Send back authentication when required when doing a REGISTER as it might be consecutive to an unregister.
 * - Update the registered variable when unregistering from a SIP registrar.
 * - Added OnRegistrationFailed () function to indicate when a registration failed. Call that function at various strategic places.
 *
 * Revision 2.29  2004/10/02 04:30:11  rjongbloed
 * Added unregister function for SIP registrar
 *
 * Revision 2.28  2004/08/22 12:27:46  rjongbloed
 * More work on SIP registration, time to live refresh and deregistration on exit.
 *
 * Revision 2.27  2004/08/20 12:54:48  rjongbloed
 * Fixed crash caused by double delete of registration transactions
 *
 * Revision 2.26  2004/08/18 13:04:56  rjongbloed
 * Fixed trying to start regitration if have empty string for domain/user
 *
 * Revision 2.25  2004/08/14 07:56:43  rjongbloed
 * Major revision to utilise the PSafeCollection classes for the connections and calls.
 *
 * Revision 2.24  2004/07/11 12:42:13  rjongbloed
 * Added function on endpoints to get the list of all media formats any
 *   connection the endpoint may create can support.
 *
 * Revision 2.23  2004/06/05 14:36:32  rjongbloed
 * Added functions to get registration URL.
 * Added ability to set proxy bu host/user/password strings.
 *
 * Revision 2.22  2004/04/26 06:30:35  rjongbloed
 * Added ability to specify more than one defualt listener for an endpoint,
 *   required by SIP which listens on both UDP and TCP.
 *
 * Revision 2.21  2004/04/26 05:40:39  rjongbloed
 * Added RTP statistics callback to SIP
 *
 * Revision 2.20  2004/04/25 08:46:08  rjongbloed
 * Fixed GNU compatibility
 *
 * Revision 2.19  2004/03/29 10:56:31  rjongbloed
 * Made sure SIP UDP socket is "promiscuous", ie the host/port being sent to may not
 *   be where packets come from.
 *
 * Revision 2.18  2004/03/14 11:32:20  rjongbloed
 * Changes to better support SIP proxies.
 *
 * Revision 2.17  2004/03/14 10:13:04  rjongbloed
 * Moved transport on SIP top be constructed by endpoint as any transport created on
 *   an endpoint can receive data for any connection.
 * Changes to REGISTER to support authentication
 *
 * Revision 2.16  2004/03/14 08:34:10  csoutheren
 * Added ability to set User-Agent string
 *
 * Revision 2.15  2004/03/13 06:51:32  rjongbloed
 * Alllowed for empty "username" in registration
 *
 * Revision 2.14  2004/03/13 06:32:18  rjongbloed
 * Fixes for removal of SIP and H.323 subsystems.
 * More registration work.
 *
 * Revision 2.13  2004/03/09 12:09:56  rjongbloed
 * More work on SIP register.
 *
 * Revision 2.12  2004/02/17 12:41:54  csoutheren
 * Use correct Contact field when behind NAT
 *
 * Revision 2.11  2003/12/20 12:21:18  rjongbloed
 * Applied more enhancements, thank you very much Ted Szoczei
 *
 * Revision 2.10  2003/03/24 04:32:58  robertj
 * SIP register must have a server address
 *
 * Revision 2.9  2003/03/06 03:57:47  robertj
 * IVR support (work in progress) requiring large changes everywhere.
 *
 * Revision 2.8  2002/10/09 04:27:44  robertj
 * Fixed memory leak on error reading PDU, thanks Ted Szoczei
 *
 * Revision 2.7  2002/09/12 06:58:34  robertj
 * Removed protocol prefix strings as static members as has problems with
 *   use in DLL environment.
 *
 * Revision 2.6  2002/07/04 07:41:47  robertj
 * Fixed memory/thread leak of transports.
 *
 * Revision 2.5  2002/04/10 03:15:17  robertj
 * Fixed using incorrect field as default reply address on receiving a respons PDU.
 *
 * Revision 2.4  2002/04/09 08:04:01  robertj
 * Fixed typo in previous patch!
 *
 * Revision 2.3  2002/04/09 07:18:33  robertj
 * Fixed the default return address for PDU requests not attached to connection.
 *
 * Revision 2.2  2002/04/05 10:42:04  robertj
 * Major changes to support transactions (UDP timeouts and retries).
 *
 * Revision 2.1  2002/02/01 04:53:01  robertj
 * Added (very primitive!) SIP support.
 *
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "sipep.h"
#endif

#include <sip/sipep.h>

#include <sip/sippdu.h>
#include <sip/sipcon.h>

#include <opal/manager.h>
#include <opal/call.h>


#define new PNEW


////////////////////////////////////////////////////////////////////////////

SIPEndPoint::SIPEndPoint(OpalManager & mgr)
  : OpalEndPoint(mgr, "sip", CanTerminateCall),
    retryTimeoutMin(500),     // 0.5 seconds
    retryTimeoutMax(0, 4),    // 4 seconds
    nonInviteTimeout(0, 16),  // 16 seconds
    pduCleanUpTimeout(0, 5),  // 5 seconds
    inviteTimeout(0, 32),     // 32 seconds
    ackTimeout(0, 32),        // 32 seconds
    registrarTimeToLive(0, 0, 0, 1) // 1 hour
{
  defaultSignalPort = 5060;
  mimeForm = FALSE;
  maxRetries = 10;
  lastSentCSeq = 0;
  userAgentString = "OPAL/2.0";

  transactions.DisallowDeleteObjects();

  registrarTransport = NULL;
  registrations.DisallowDeleteObjects();
  registered = false;
  registrationTimer.SetNotifier(PCREATE_NOTIFIER(RegistrationRefresh));

  PTRACE(3, "SIP\tCreated endpoint.");
}


SIPEndPoint::~SIPEndPoint()
{
  Unregister();

  delete registrarTransport;

  listeners.RemoveAll();

  PTRACE(3, "SIP\tDeleted endpoint.");
}


PStringArray SIPEndPoint::GetDefaultListeners() const
{
  PStringArray listenerAddresses = OpalEndPoint::GetDefaultListeners();
  if (!listenerAddresses.IsEmpty())
    listenerAddresses.AppendString(psprintf("udp$*:%u", defaultSignalPort));
  return listenerAddresses;
}


BOOL SIPEndPoint::NewIncomingConnection(OpalTransport * transport)
{
  PTRACE_IF(2, transport->IsReliable(), "SIP\tListening thread started.");

  transport->SetBufferSize(SIP_PDU::MaxSize);

  do {
    HandlePDU(*transport);
  } while (transport->IsOpen() && transport->IsReliable());

  PTRACE_IF(2, transport->IsReliable(), "SIP\tListening thread finished.");

  return TRUE;
}


void SIPEndPoint::TransportThreadMain(PThread &, INT param)
{
  PTRACE(2, "SIP\tRead thread started.");

  OpalTransport * transport = (OpalTransport *)param;

  do {
    HandlePDU(*transport);
  } while (transport->IsOpen());

  PTRACE(2, "SIP\tRead thread finished.");
}


OpalTransport * SIPEndPoint::CreateTransport(const OpalTransportAddress & address)
{
  OpalTransport * transport = address.CreateTransport(*this, OpalTransportAddress::NoBinding);
  if (transport == NULL) {
    PTRACE(1, "SIP\tCould not create transport from \"" << address << '"');
    return NULL;
  }

  transport->SetBufferSize(SIP_PDU::MaxSize);
  if (!transport->ConnectTo(address)) {
    PTRACE(1, "SIP\tCould not connect to " << address << " - " << transport->GetErrorText());
    delete transport;
    return NULL;
  }

  transport->SetPromiscuous(OpalTransport::AcceptFromAny);

  if (!transport->IsReliable())
    transport->AttachThread(PThread::Create(PCREATE_NOTIFIER(TransportThreadMain),
                                            (INT)transport,
                                            PThread::NoAutoDeleteThread,
                                            PThread::NormalPriority,
                                            "SIP Transport:%x"));

  return transport;
}


void SIPEndPoint::HandlePDU(OpalTransport & transport)
{
  // create a SIP_PDU structure, then get it to read and process PDU
  SIP_PDU * pdu = new SIP_PDU;

  PTRACE(4, "SIP\tWaiting for PDU on " << transport);
  if (pdu->Read(transport)) {
    if (!transport.IsReliable()) {
      // Calculate default return address
      if (pdu->GetMethod() != SIP_PDU::NumMethods) {
        PString via = pdu->GetMIME().GetVia();
        transport.SetRemoteAddress(via.Mid(via.FindLast(' ')));
      }
    }
    if (OnReceivedPDU(transport, pdu))
      return;
  }
  else if (transport.good()) {
    PTRACE(1, "SIP\tMalformed request received on " << transport);
    SIP_PDU response(*pdu, SIP_PDU::Failure_BadRequest);
    response.Write(transport);
  }

  delete pdu;
}


BOOL SIPEndPoint::MakeConnection(OpalCall & call,
                                 const PString & remoteParty,
                                 void * userData)
{
  if (remoteParty.Find("sip:") != 0)
    return FALSE;

  PStringStream callID;
  OpalGloballyUniqueID id;
  callID << id << '@' << PIPSocket::GetHostName();
  SIPConnection * connection = CreateConnection(call, callID, userData, remoteParty, NULL, NULL);
  if (connection == NULL)
    return FALSE;

  connectionsActive.SetAt(connection->GetToken(), connection);

  // If we are the A-party then need to initiate a call now in this thread. If
  // we are the B-Party then SetUpConnection() gets called in the context of
  // the A-party thread.
  if (call.GetConnection(0) == connection)
    connection->SetUpConnection();

  return TRUE;
}


OpalMediaFormatList SIPEndPoint::GetMediaFormats() const
{
  return OpalMediaFormat::GetAllRegisteredMediaFormats();
}


BOOL SIPEndPoint::IsAcceptedAddress(const SIPURL & /*toAddr*/)
{
  return TRUE;
}


SIPConnection * SIPEndPoint::CreateConnection(OpalCall & call,
                                              const PString & token,
                                              void * /*userData*/,
                                              const SIPURL & destination,
                                              OpalTransport * transport,
                                              SIP_PDU * /*invite*/)
{
  return new SIPConnection(call, *this, token, destination, transport);
}


BOOL SIPEndPoint::OnReceivedPDU(OpalTransport & transport, SIP_PDU * pdu)
{
  PSafePtr<SIPConnection> connection = GetSIPConnectionWithLock(pdu->GetMIME().GetCallID());
  if (connection != NULL) {
    connection->QueuePDU(pdu);
    return TRUE;
  }

  // PDU's outside of connection context
  switch (pdu->GetMethod()) {
    case SIP_PDU::NumMethods :
      {
        SIPTransaction * transaction = transactions.GetAt(pdu->GetTransactionID());
        if (transaction != NULL)
          transaction->OnReceivedResponse(*pdu);
      }
      break;

    case SIP_PDU::Method_INVITE :
      return OnReceivedINVITE(transport, pdu);

    case SIP_PDU::Method_REGISTER :
      {
        SIP_PDU response(*pdu, SIP_PDU::Failure_MethodNotAllowed);
        response.GetMIME().SetAt("Allow", "INVITE");
        response.Write(transport);
      }
      break;

    case SIP_PDU::Method_ACK :
      {
	// If we receive an ACK outside of the context of a connection,
	// ignore it.
      }
      break;

    default :
      {
        SIP_PDU response(*pdu, SIP_PDU::Failure_TransactionDoesNotExist);
        response.Write(transport);
      }
  }

  return FALSE;
}


void SIPEndPoint::OnReceivedResponse(SIPTransaction & transaction, SIP_PDU & response)
{
  RegistrationFailReasons reason;
  
  if (transaction.GetMethod() == SIP_PDU::Method_REGISTER) {
    // Have a response to the REGISTER, so CANCEL all the other registrations sent.
    for (PINDEX i = 0; i < registrations.GetSize(); i++) {
      if (&registrations[i] != &transaction)
        registrations[i].SendCANCEL();
    }

    // Have a response to the INVITE, so end Connect mode on the transport
    transaction.GetTransport().EndConnect(transaction.GetLocalAddress());

    // Failure, in the 4XX class, handle some cases to give feedback.
    if (response.GetStatusCode()/100 == 4
	&& response.GetStatusCode() != SIP_PDU::Failure_UnAuthorised
	&& response.GetStatusCode() != SIP_PDU::Failure_ProxyAuthenticationRequired) {

      switch (response.GetStatusCode()) {

      case SIP_PDU::Failure_BadRequest:
	reason = BadRequest;
	break;
      case SIP_PDU::Failure_UnAuthorised:
      case SIP_PDU::Failure_Forbidden:
	reason = Forbidden;
	break;
      case SIP_PDU::Failure_PaymentRequired:
	reason = PaymentRequired;
	break;
      case SIP_PDU::Failure_RequestTimeout:
	reason = Timeout;
	break;
      case SIP_PDU::Failure_Conflict:
	reason = Conflict;
	break;
      case SIP_PDU::Failure_TemporarilyUnavailable:
	reason = TemporarilyUnavailable;
	break;

      default:
	reason = RegistrationFailed;
      }

      OnRegistrationFailed (reason, TRUE);
    }
  }

  switch (response.GetStatusCode()) {
    case SIP_PDU::Failure_UnAuthorised :
    case SIP_PDU::Failure_ProxyAuthenticationRequired :
      OnReceivedAuthenticationRequired(transaction, response);
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
          ;
      }
  }
}


BOOL SIPEndPoint::OnReceivedINVITE(OpalTransport & transport, SIP_PDU * request)
{
  SIPMIMEInfo & mime = request->GetMIME();

  // parse the incoming To field, and check if we accept incoming calls for this address
  SIPURL toAddr(mime.GetTo());
  if (!IsAcceptedAddress(toAddr)) {
    PTRACE(1, "SIP\tIncoming INVITE from " << request->GetURI() << " for unknown address " << toAddr);
    SIP_PDU response(*request, SIP_PDU::Failure_NotFound);
    response.Write(transport);
    return FALSE;
  }

  // ask the endpoint for a connection
  SIPConnection * connection = CreateConnection(*GetManager().CreateCall(), mime.GetCallID(),
                                                NULL, request->GetURI(), &transport, request);
  if (connection == NULL) {
    PTRACE(2, "SIP\tFailed to create SIPConnection for INVITE from " << request->GetURI() << " for " << toAddr);
    SIP_PDU response(*request, SIP_PDU::Failure_NotFound);
    response.Write(transport);
    return FALSE;
  }

  // add the connection to the endpoint list
  connectionsActive.SetAt(connection->GetToken(), connection);

  // Get the connection to handle the rest of the INVITE
  connection->QueuePDU(request);
  return TRUE;
}


void SIPEndPoint::OnReceivedAuthenticationRequired(SIPTransaction & transaction, SIP_PDU & response)
{
  BOOL isProxy = response.GetStatusCode() == SIP_PDU::Failure_ProxyAuthenticationRequired;
#if PTRACING
  const char * proxyTrace = isProxy ? "Proxy " : "";
#endif

  if (authentication.IsValid()) {
    PTRACE(1, "SIP\tAlready done INVITE for " << proxyTrace << "Authentication Required");
    return;
  }

  if (transaction.GetMethod() != SIP_PDU::Method_REGISTER) {
    PTRACE(1, "SIP\tCannot do " << proxyTrace << "Authentication Required for non REGISTER");
    return;
  }

  PTRACE(2, "SIP\tReceived " << proxyTrace << "Authentication Required response");

  if (!authentication.Parse(response.GetMIME()(isProxy ? "Proxy-Authenticate"
                                                       : "WWW-Authenticate"),
                                               isProxy)) {
    return;
  }

  // Restart the transaction with new authentication info
  SIPTransaction * request = new SIPRegister(*this,
                                             transaction.GetTransport(),
                                             registrationAddress,
                                             registrationID,
                                             registrarTimeToLive.GetSeconds());
  if (request->Start())
    registrations.Append(request);
  else {
    delete request;
    PTRACE(1, "SIP\tCould not restart REGISTER for Authentication Required");
  }
}


void SIPEndPoint::OnReceivedOK(SIPTransaction & transaction, SIP_PDU & response)
{
  if (transaction.GetMethod() != SIP_PDU::Method_REGISTER) {
    PTRACE(3, "SIP\tReceived OK response for non REGISTER");
    return;
  }

  PTRACE(2, "SIP\tReceived REGISTER OK response");

  /* We were registered */
  if (registered) {
    
    registered = FALSE;
    OnRegistered(FALSE);
    return;
  }
  
  /* We were not registered */
  registered = true;

  SIPURL contact = response.GetMIME().GetContact();
  registrationTimer.SetInterval(0, contact.GetParamVars()("expires", "3600").AsUnsigned()*9/10);

  OnRegistered(TRUE);
}


void SIPEndPoint::OnRegistered(BOOL wasRegistering)
{
  PTRACE(2, "SIP\tREGISTER success");
}


void SIPEndPoint::OnRegistrationFailed(RegistrationFailReasons reason, BOOL wasRegistering)
{
  PTRACE(2, "SIP\tREGISTER failed");
}


void SIPEndPoint::RegistrationRefresh(PTimer &, INT)
{
  PTRACE(2, "SIP\tStarting REGISTER for binding refresh");

  // Restart the transaction with new authentication info
  SIPTransaction * request = new SIPRegister(*this,
                                             *registrarTransport,
                                             registrationAddress,
                                             registrationID,
                                             registrarTimeToLive.GetSeconds());
  if (request->Start())
    registrations.Append(request);
  else {
    delete request;
    PTRACE(1, "SIP\tCould not start REGISTER for binding refresh");
  }
}


BOOL SIPEndPoint::WriteREGISTER(OpalTransport & transport, void * param)
{
  SIPEndPoint & endpoint = *(SIPEndPoint *)param;
  SIPTransaction * request = new SIPRegister(endpoint,
                                             transport,
                                             endpoint.registrationAddress,
                                             endpoint.registrationID,
                                             endpoint.registrarTimeToLive.GetSeconds());

  if (request->Start()) {
    endpoint.registrations.Append(request);
    request->Wait ();
    return TRUE;
  }

  PTRACE(2, "SIP\tDid not start REGISTER transaction on " << transport);
  return FALSE;
}


BOOL SIPEndPoint::Register(const PString & domain,
                           const PString & username,
                           const PString & password)
{
  if (listeners.IsEmpty() || domain.IsEmpty() || username.IsEmpty())
    return FALSE;

  registered = false;

  PString adjustedUsername = username;
  if (adjustedUsername.IsEmpty())
    adjustedUsername = GetDefaultLocalPartyName();

  if (adjustedUsername.Find('@') == P_MAX_INDEX)
    adjustedUsername += '@' + domain;

  registrationAddress.Parse(adjustedUsername);

  PString hostname;
  WORD port;

  if (proxy.IsEmpty()) {
    // Should do DNS SRV record lookup to get registrar address
    hostname = domain;
    port = defaultSignalPort;
  }
  else {
    hostname = proxy.GetHostName();
    port = proxy.GetPort();
    if (port == 0)
      port = defaultSignalPort;
  }

  OpalTransportAddress registrarAddress(hostname, port, "udp");

  delete registrarTransport;
  registrarTransport = CreateTransport(registrarAddress);
  if (registrarTransport == NULL)
    return FALSE;

  authentication.SetUsername(registrationAddress.GetUserName());
  authentication.SetPassword(password);

  registrationID = OpalGloballyUniqueID().AsString();

  if (!registrarTransport->WriteConnect(WriteREGISTER, this)) {
    PTRACE(1, "SIP\tCould not write to " << registrarAddress << " - " << registrarTransport->GetErrorText());
    return FALSE;
  }

  return TRUE;
}


BOOL SIPEndPoint::Unregister()
{
  if (!registered || registrarTransport == NULL)
    return FALSE;

  SIPRegister *unregister =
    new SIPRegister (*this, *registrarTransport, registrationAddress, registrationID, 0);

  if (!unregister->Start()) {

    PTRACE(1, "SIP\tCould not start UNREGISTER");
    delete (unregister);
    unregister = NULL;
    
    return FALSE;
  }

  registrations.Append(unregister);
  unregister->Wait();


  return TRUE;
}


void SIPEndPoint::SetProxy(const PString & hostname,
                           const PString & username,
                           const PString & password)
{
  PStringStream str;
  if (!hostname) {
    str << "sip:";
    if (!username) {
      str << username;
      if (!password)
        str << ':' << password;
      str << '@';
    }
    str << hostname;
  }
  proxy = str;
}


PString SIPEndPoint::GetUserAgent() const 
{ 
  return userAgentString;
}


void SIPEndPoint::OnRTPStatistics(const SIPConnection & /*connection*/,
                                  const RTP_Session & /*session*/) const
{
}


// End of file ////////////////////////////////////////////////////////////////
