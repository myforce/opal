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
 * $Revision$
 * $Author$
 * $Date$
 */

#include <ptlib.h>
#include <opal/buildopts.h>

#if OPAL_SIP

#ifdef __GNUC__
#pragma implementation "sipep.h"
#endif

#include <sip/sipep.h>

#include <ptclib/enum.h>
#include <sip/sippdu.h>
#include <sip/sipcon.h>
#include <opal/manager.h>
#include <opal/call.h>
#include <sip/handlers.h>


#define new PNEW


////////////////////////////////////////////////////////////////////////////

SIPEndPoint::SIPEndPoint(OpalManager & mgr)
  : OpalRTPEndPoint(mgr, "sip", CanTerminateCall)
  , retryTimeoutMin(500)             // 0.5 seconds
  , retryTimeoutMax(0, 4)            // 4 seconds
  , nonInviteTimeout(0, 16)          // 16 seconds
  , pduCleanUpTimeout(0, 5)          // 5 seconds
  , inviteTimeout(0, 32)             // 32 seconds
  , ackTimeout(0, 32)                // 32 seconds
  , registrarTimeToLive(0, 0, 0, 1)  // 1 hour
  , notifierTimeToLive(0, 0, 0, 1)   // 1 hour
  , natBindingTimeout(0, 0, 1)       // 1 minute
  , m_shuttingDown(false)
  , m_defaultAppearanceCode(-1)

#ifdef _MSC_VER
#pragma warning(disable:4355)
#endif

  , m_highPriorityMonitor(*this, HighPriority)
  , m_lowPriorityMonitor(*this, LowPriority)

#if OPAL_HAS_SIPIM
  , m_sipIMManager(*this)
#endif

#ifdef _MSC_VER
#pragma warning(default:4355)
#endif
{
  defaultSignalPort = 5060;
  mimeForm = PFalse;
  maxRetries = 10;

  natBindingTimer.SetNotifier(PCREATE_NOTIFIER(NATBindingRefresh));
  natBindingTimer.RunContinuous(natBindingTimeout);

  natMethod = None;

  // Make sure these have been contructed now to avoid
  // payload type disambiguation problems.
  GetOpalRFC2833();

#if OPAL_T38_CAPABILITY
  GetOpalCiscoNSE();
#endif

#if OPAL_PTLIB_SSL
  manager.AttachEndPoint(this, "sips");
#endif

  PTRACE(4, "SIP\tCreated endpoint.");
}


SIPEndPoint::~SIPEndPoint()
{
}


void SIPEndPoint::ShutDown()
{
  PTRACE(4, "SIP\tShutting down.");
  m_shuttingDown = true;

  // Stop timers before compiler destroys member objects
  natBindingTimer.Stop(false);

  // Clean up the handlers, wait for them to finish before destruction.
  bool shuttingDown = true;
  while (shuttingDown) {
    shuttingDown = false;
    PSafePtr<SIPHandler> handler(activeSIPHandlers, PSafeReference);
    while (handler != NULL) {
      if (handler->ShutDown())
        activeSIPHandlers.Remove(handler++);
      else {
        shuttingDown = true;
        ++handler;
      }
    }
    PThread::Sleep(100);
  }

  // Clean up transactions still in progress, waiting for them to complete.
  PSafePtr<SIPTransaction> transaction;
  while ((transaction = transactions.GetAt(0, PSafeReference)) != NULL) {
    transaction->WaitForCompletion();
    transactions.RemoveAt(transaction->GetTransactionID());
  }

  // Now shut down listeners and aggregators
  OpalEndPoint::ShutDown();
}


PString SIPEndPoint::GetDefaultTransport() const 
{  
  return "udp$,tcp$"
#if OPAL_PTLIB_SSL
         ",tcps$:5061"
#endif
    ; 
}

PBoolean SIPEndPoint::NewIncomingConnection(OpalTransport * transport)
{
  PTRACE_IF(2, transport->IsReliable(), "SIP\tListening thread started.");

  transport->SetBufferSize(SIP_PDU::MaxSize);

  do {
    HandlePDU(*transport);
  } while (transport->IsOpen() && transport->IsReliable() && !transport->bad() && !transport->eof());

  PTRACE_IF(2, transport->IsReliable(), "SIP\tListening thread finished.");

  return PTrue;
}


void SIPEndPoint::TransportThreadMain(PThread &, INT param)
{
  PTRACE(4, "SIP\tRead thread started.");
  OpalTransport * transport = (OpalTransport *)param;

  do {
    HandlePDU(*transport);
  } while (transport->IsOpen() && !transport->bad() && !transport->eof());
  
  PTRACE(4, "SIP\tRead thread finished.");
}


void SIPEndPoint::NATBindingRefresh(PTimer &, INT)
{
  if (m_shuttingDown)
    return;

  if (natMethod != None) {
    PTRACE(5, "SIP\tNAT Binding refresh started.");
    for (PSafePtr<SIPHandler> handler(activeSIPHandlers, PSafeReadOnly); handler != NULL; ++handler) {

      OpalTransport * transport = NULL;
      if (handler->GetState () != SIPHandler::Subscribed ||
           (transport = handler->GetTransport()) == NULL ||
           transport->IsReliable() ||
           GetManager().GetNatMethod(transport->GetRemoteAddress().GetHostName()) == NULL)
        continue;

      switch (natMethod) {

        case Options: 
          {
            SIPOptions options(*this, *transport, SIPURL(transport->GetRemoteAddress()).GetHostName());
            options.Write(*transport);
          }
          break;

        case EmptyRequest:
          transport->Write("\r\n", 2);
          break;

        default:
          break;
      }
    }

    PTRACE(5, "SIP\tNAT Binding refresh finished.");
  }
}


OpalTransport * SIPEndPoint::CreateTransport(const SIPURL & remoteURL, const PString & localInterface)
{
  OpalTransportAddress remoteAddress = remoteURL.GetHostAddress();

  OpalTransportAddress localAddress;
  if (!localInterface.IsEmpty()) {
    if (localInterface != "*") // Nasty kludge to get around infinite recursion in REGISTER
      localAddress = OpalTransportAddress(localInterface, 0, remoteAddress.GetProto());
  }
  else {
    PSafePtr<SIPHandler> handler = activeSIPHandlers.FindSIPHandlerByDomain(remoteURL.GetHostName(), SIP_PDU::Method_REGISTER, PSafeReadOnly);
    if (handler != NULL) {
      OpalTransport * transport = handler->GetTransport();
      if (transport != NULL) {
        localAddress = transport->GetInterface();
        PTRACE(4, "SIP\tFound registrar on domain " << remoteURL.GetHostName() << ", using interface " << transport->GetInterface());
      }
    }
  }

  OpalTransport * transport = NULL;

  for (OpalListenerList::iterator listener = listeners.begin(); listener != listeners.end(); ++listener) {
    if ((transport = listener->CreateTransport(localAddress, remoteAddress)) != NULL)
      break;
  }

  if (transport == NULL) {
    // No compatible listeners, can't use their binding
    transport = remoteAddress.CreateTransport(*this, OpalTransportAddress::NoBinding);
    if (transport == NULL) {
      PTRACE(1, "SIP\tCould not create transport for " << remoteAddress);
      return NULL;
    }
  }

  if (!transport->SetRemoteAddress(remoteAddress)) {
    PTRACE(1, "SIP\tCould not find " << remoteAddress);
    delete transport;
    return NULL;
  }

  PTRACE(4, "SIP\tCreated transport " << *transport);

  transport->SetBufferSize(SIP_PDU::MaxSize);
  if (!transport->Connect()) {
    PTRACE(1, "SIP\tCould not connect to " << remoteAddress << " - " << transport->GetErrorText());
    transport->CloseWait();
    delete transport;
    return NULL;
  }

  transport->SetPromiscuous(OpalTransport::AcceptFromAny);

  if (transport->IsReliable())
    transport->AttachThread(PThread::Create(PCREATE_NOTIFIER(TransportThreadMain),
                                            (INT)transport,
                                            PThread::NoAutoDeleteThread,
                                            PThread::HighestPriority,
                                            "SIP Transport"));
  return transport;
}


void SIPEndPoint::HandlePDU(OpalTransport & transport)
{
  // create a SIP_PDU structure, then get it to read and process PDU
  SIP_PDU * pdu = new SIP_PDU;

  PTRACE(4, "SIP\tWaiting for PDU on " << transport);
  if (pdu->Read(transport)) {
    if (OnReceivedPDU(transport, pdu)) 
      return;
  }
  else {
    PTRACE_IF(1, transport.GetErrorCode(PChannel::LastReadError) != PChannel::NoError,
              "SIP\tPDU Read failed: " << transport.GetErrorText(PChannel::LastReadError));
    if (transport.good()) {
      PTRACE(2, "SIP\tMalformed request received on " << transport);
      pdu->SendResponse(transport, SIP_PDU::Failure_BadRequest, this);
    }
  }

  delete pdu;
}


static PString TranslateENUM(const PString & remoteParty)
{
#if OPAL_PTLIB_DNS
  // if there is no '@', and then attempt to use ENUM
  if (remoteParty.Find('@') == P_MAX_INDEX) {

    // make sure the number has only digits
    PString e164 = remoteParty;
    PINDEX pos = e164.Find(':');
    if (pos != P_MAX_INDEX)
      e164.Delete(0, pos+1);
    pos = e164.FindSpan("0123456789*#", e164[0] != '+' ? 0 : 1);
    e164.Delete(pos, P_MAX_INDEX);

    if (!e164.IsEmpty()) {
      PString str;
      if (PDNS::ENUMLookup(e164, "E2U+SIP", str)) {
        PTRACE(4, "SIP\tENUM converted remote party " << remoteParty << " to " << str);
        return str;
      }
    }
  }
#endif

  return remoteParty;
}


PBoolean SIPEndPoint::MakeConnection(OpalCall & call,
                                 const PString & remoteParty,
                                 void * userData,
                                 unsigned int options,
                                 OpalConnection::StringOptions * stringOptions)
{
  if ((remoteParty.NumCompare("sip:") != EqualTo) && (remoteParty.NumCompare("sips:") != EqualTo))
    return false;

  if (listeners.IsEmpty())
    return false;

  return AddConnection(CreateConnection(call, SIPURL::GenerateTag(), userData, TranslateENUM(remoteParty), NULL, NULL, options, stringOptions));
}


void SIPEndPoint::OnReleased(OpalConnection & connection)
{
  m_receivedConnectionTokens.RemoveAt(connection.GetIdentifier());
  OpalEndPoint::OnReleased(connection);
}


PBoolean SIPEndPoint::GarbageCollection()
{
  PSafePtr<SIPTransaction> transaction(transactions, PSafeReadOnly);
  while (transaction != NULL) {
    if (transaction->IsTerminated()) {
      PString id = transaction->GetTransactionID();
      ++transaction;
      transactions.RemoveAt(id);
    }
    else
      ++transaction;
  }

  PSafePtr<SIPHandler> handler(activeSIPHandlers, PSafeReference);
  while (handler != NULL) {
    if (handler->GetState() == SIPHandler::Unsubscribed || handler->ShutDown())
      activeSIPHandlers.Remove(handler++);
    else
      ++handler;
  }

  // Note use & rather than && so not McCarthy boolean, want all three functions executed
  return transactions.DeleteObjectsToBeRemoved() &
         activeSIPHandlers.DeleteObjectsToBeRemoved() &
         OpalEndPoint::GarbageCollection();
}


PBoolean SIPEndPoint::IsAcceptedAddress(const SIPURL & /*toAddr*/)
{
  return PTrue;
}


SIPConnection * SIPEndPoint::CreateConnection(OpalCall & call,
                                              const PString & token,
                                              void * /*userData*/,
                                              const SIPURL & destination,
                                              OpalTransport * transport,
                                              SIP_PDU * /*invite*/,
                                              unsigned int options,
                                              OpalConnection::StringOptions * stringOptions)
{
  return new SIPConnection(call, *this, token, destination, transport, options, stringOptions);
}


PBoolean SIPEndPoint::SetupTransfer(const PString & token,
                                    const PString & callId,
                                    const PString & remoteParty,
                                    void * userData)
{
  // Make a new connection
  PSafePtr<OpalConnection> otherConnection = GetConnectionWithLock(token, PSafeReference);
  if (otherConnection == NULL)
    return false;

  OpalCall & call = otherConnection->GetCall();

  PTRACE(3, "SIP\tTransferring " << *otherConnection << " to " << remoteParty << " in call " << call);

  OpalConnection::StringOptions options;
  if (!callId.IsEmpty())
    options.SetAt(SIP_HEADER_REPLACES, callId);

  SIPConnection * connection = CreateConnection(call, SIPURL::GenerateTag(), userData, TranslateENUM(remoteParty), NULL, NULL, 0, &options);
  if (!AddConnection(connection))
    return false;

  otherConnection->Release(OpalConnection::EndedByCallForwarded);
  otherConnection->CloseMediaStreams();

  return connection->SetUpConnection();
}


PBoolean SIPEndPoint::ForwardConnection(SIPConnection & connection, const PString & forwardParty)
{
  OpalCall & call = connection.GetCall();
  
  SIPConnection * conn = CreateConnection(call, SIPURL::GenerateTag(), NULL, forwardParty, NULL, NULL);
  if (!AddConnection(conn))
    return PFalse;

  call.OnReleased(connection);
  
  conn->SetUpConnection();
  connection.Release(OpalConnection::EndedByCallForwarded);

  return PTrue;
}

PBoolean SIPEndPoint::OnReceivedPDU(OpalTransport & transport, SIP_PDU * pdu)
{
  if (PAssertNULL(pdu) == NULL)
    return PFalse;

  const SIPMIMEInfo & mime = pdu->GetMIME();

  /* Get tokens to determine the connection to operate on, not as easy as it
     sounds due to allowing for talking to ones self, always thought madness
     generally lies that way ... */

  PString fromToken = mime.GetFieldParameter("from", "tag");
  PString toToken = mime.GetFieldParameter("to", "tag");
  bool hasFromConnection = HasConnection(fromToken);
  bool hasToConnection = HasConnection(toToken);

  PString token;

  // Adjust the Via list and send a trying in case it takes us a while to process request
  switch (pdu->GetMethod()) {
    case SIP_PDU::Method_CANCEL :
      token = m_receivedConnectionTokens(mime.GetCallID());
      if (!token.IsEmpty()) {
        threadPool.AddWork(new SIP_PDU_Work(*this, token, pdu));
        return true;
      }
      break;

    case SIP_PDU::Method_INVITE :
      if (toToken.IsEmpty()) {
        token = m_receivedConnectionTokens(mime.GetCallID());
        if (!token.IsEmpty()) {
          threadPool.AddWork(new SIP_PDU_Work(*this, token, pdu));
          return true;
        }

        pdu->SendResponse(transport, SIP_PDU::Information_Trying, this);
        return OnReceivedConnectionlessPDU(transport, pdu);
      }

      if (!hasToConnection) {
        // Has to tag but doesn't correspond to anything, odd.
        pdu->SendResponse(transport, SIP_PDU::Failure_TransactionDoesNotExist);
        return false;
      }
      // Do next case

    default :
      pdu->SendResponse(transport, SIP_PDU::Information_Trying, this);
      // Do next case

    case SIP_PDU::Method_ACK :
      pdu->AdjustVia(transport);
      // Do next case

    case SIP_PDU::NumMethods :
      break;
  }

  if (hasFromConnection && hasToConnection)
    token = pdu->GetMethod() != SIP_PDU::NumMethods ? fromToken : toToken;
  else if (hasFromConnection)
    token = fromToken;
  else if (hasToConnection)
    token = toToken;
  else
    return OnReceivedConnectionlessPDU(transport, pdu);

  threadPool.AddWork(new SIP_PDU_Work(*this, token, pdu));
  return true;
}


bool SIPEndPoint::OnReceivedConnectionlessPDU(OpalTransport & transport, SIP_PDU * pdu)
{
  if (pdu->GetMethod() == SIP_PDU::NumMethods || pdu->GetMethod() == SIP_PDU::Method_CANCEL) {
    PSafePtr<SIPTransaction> transaction = GetTransaction(pdu->GetTransactionID(), PSafeReference);
    if (transaction != NULL)
      transaction->OnReceivedResponse(*pdu);
    return false;
  }

  // Prevent any new INVITE/SUBSCRIBE etc etc while we are on the way out.
  if (m_shuttingDown) {
    pdu->SendResponse(transport, SIP_PDU::Failure_ServiceUnavailable);
    return false;
  }

  switch (pdu->GetMethod()) {
    case SIP_PDU::Method_INVITE :
      return OnReceivedINVITE(transport, pdu);

    case SIP_PDU::Method_REGISTER :
      if (OnReceivedREGISTER(transport, *pdu))
        return false;
      break;

    case SIP_PDU::Method_SUBSCRIBE :
      if (OnReceivedSUBSCRIBE(transport, *pdu))
        return false;
      break;

    case SIP_PDU::Method_NOTIFY :
       if (OnReceivedNOTIFY(transport, *pdu))
         return false;
       break;

    case SIP_PDU::Method_MESSAGE :
      if (OnReceivedMESSAGE(transport, *pdu))
        return false;
      break;
   
    case SIP_PDU::Method_OPTIONS :
      if (OnReceivedOPTIONS(transport, *pdu))
        return false;
      break;

    case SIP_PDU::Method_ACK :
    case SIP_PDU::Method_BYE :
      // If we receive an ACK or BYE outside of the context of a connection, ignore it.
      pdu->SendResponse(transport, SIP_PDU::Failure_TransactionDoesNotExist, this);
      return false;

    default :
      break;
  }

  SIP_PDU response(*pdu, SIP_PDU::Failure_MethodNotAllowed);
  response.SetAllow(GetAllowedMethods()); // Required by spec
  pdu->SendResponse(transport, response, this);
  return false;
}

PBoolean SIPEndPoint::OnReceivedREGISTER(OpalTransport & /*transport*/, SIP_PDU & /*pdu*/)
{
  return false;
}


PBoolean SIPEndPoint::OnReceivedSUBSCRIBE(OpalTransport & transport, SIP_PDU & pdu)
{
  SIPMIMEInfo & mime = pdu.GetMIME();

  SIPSubscribe::EventPackage eventPackage = mime.GetEvent();
  if (!CanNotify(eventPackage))
    return false;

  // See if already subscribed. Now this is not perfect as we only check the call-id and strictly
  // speaking we should check the from-tag and to-tags as well due to it being a dialog.
  PSafePtr<SIPHandler> handler = activeSIPHandlers.FindSIPHandlerByCallID(mime.GetCallID(), PSafeReadWrite);
  if (handler == NULL) {
    SIPDialogContext dialog;
    dialog.SetRequestURI(mime.GetContact());
    dialog.SetLocalURI(mime.GetTo());
    dialog.SetRemoteURI(mime.GetFrom());
    dialog.SetCallID(mime.GetCallID());
    dialog.SetRouteSet(mime.GetRecordRoute(true));

    handler = new SIPNotifyHandler(*this, dialog.GetRemoteURI().AsString(), eventPackage, dialog);
    activeSIPHandlers.Append(handler);

    handler->GetTransport()->SetInterface(transport.GetInterface());

    mime.SetTo(dialog.GetLocalURI().AsQuotedString());
  }

  // Update expiry time
  unsigned expires = mime.GetExpires();
  if (expires > 0)
    handler->SetExpire(expires);

  SIP_PDU response(pdu, SIP_PDU::Successful_OK);
  response.GetMIME().SetEvent(eventPackage); // Required by spec
  response.GetMIME().SetExpires(handler->GetExpire()); // Required by spec
  pdu.SendResponse(transport, response, this);

  if (handler->IsDuplicateCSeq(mime.GetCSeqIndex()))
    return true;

  // Send initial NOTIFY as per spec 3.1.6.2/RFC3265
  if (expires > 0)
    handler->SendNotify(NULL);
  else
    handler->SendRequest(SIPHandler::Unsubscribing);

  return true;
}


void SIPEndPoint::OnReceivedResponse(SIPTransaction & transaction, SIP_PDU & response)
{
  PSafePtr<SIPHandler> handler = NULL;
 
  if (transaction.GetMethod() == SIP_PDU::Method_REGISTER
      || transaction.GetMethod() == SIP_PDU::Method_SUBSCRIBE
      || transaction.GetMethod() == SIP_PDU::Method_PUBLISH
      || transaction.GetMethod() == SIP_PDU::Method_MESSAGE) {
    // Have a response to various non-INVITE messages
    handler = activeSIPHandlers.FindSIPHandlerByCallID(transaction.GetMIME().GetCallID(), PSafeReadWrite);
    if (handler == NULL) 
      return;
  }

  switch (response.GetStatusCode()) {
    case SIP_PDU::Failure_IntervalTooBrief :
      OnReceivedIntervalTooBrief(transaction, response);
      break;

    case SIP_PDU::Failure_UnAuthorised :
    case SIP_PDU::Failure_ProxyAuthenticationRequired :
      OnReceivedAuthenticationRequired(transaction, response);
      break;

    case SIP_PDU::Failure_RequestTimeout :
      if (handler != NULL)
        handler->OnTransactionFailed(transaction);
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
          // Failure for a SUBSCRIBE/REGISTER/PUBLISH/MESSAGE 
          if (handler != NULL)
            handler->OnFailed(response.GetStatusCode());
          break;
      }
  }
}


PBoolean SIPEndPoint::OnReceivedINVITE(OpalTransport & transport, SIP_PDU * request)
{
  SIPMIMEInfo & mime = request->GetMIME();

  // parse the incoming To field, and check if we accept incoming calls for this address
  SIPURL toAddr(mime.GetTo());
  if (!IsAcceptedAddress(toAddr)) {
    PTRACE(2, "SIP\tIncoming INVITE from " << request->GetURI() << " for unacceptable address " << toAddr);
    request->SendResponse(transport, SIP_PDU::Failure_NotFound, this);
    return PFalse;
  }

  // See if we are replacing an existing call.
  OpalCall * call = NULL;
  if (mime.Contains("Replaces")) {
    PString replaces = mime("Replaces");

    PString to;
    static const char toTag[] = ";to-tag=";
    PINDEX pos = replaces.Find(toTag);
    if (pos != P_MAX_INDEX) {
      pos += sizeof(toTag)-1;
      to = replaces(pos, replaces.Find(';', pos)-1).Trim();
    }

    PString from;
    static const char fromTag[] = ";from-tag=";
    pos = replaces.Find(fromTag);
    if (pos != P_MAX_INDEX) {
      pos += sizeof(fromTag)-1;
      from = replaces(pos, replaces.Find(';', pos)-1).Trim();
    }

    PString callid = replaces.Left(replaces.Find(';')).Trim();
    if (callid.IsEmpty() || to.IsEmpty() || from.IsEmpty()) {
      PTRACE(2, "SIP\tBad Replaces header in INVITE from " << request->GetURI());
      request->SendResponse(transport, SIP_PDU::Failure_BadRequest, this);
      return false;
    }

    PSafePtr<SIPConnection> replacedConnection = GetSIPConnectionWithLock(callid, PSafeReadOnly);
    if (replacedConnection == NULL ||
        replacedConnection->GetDialog().GetLocalTag() != to ||
        replacedConnection->GetDialog().GetRemoteTag() != from) {
      PTRACE(2, "SIP\tUnknown " << (replacedConnection != NULL ? "to/from" : "call-id")
             << " in Replaces header of INVITE from " << request->GetURI());
      request->SendResponse(transport, SIP_PDU::Failure_TransactionDoesNotExist, this);
      return false;
    }

    // Use the existing call instance when replacing the SIP side of it.
    call = &replacedConnection->GetCall();
    PTRACE(3, "SIP\tIncoming INVITE replaces connection " << *replacedConnection);
  }

  // create and check transport
  OpalTransport * newTransport;
  if (transport.IsReliable())
    newTransport = &transport;
  else {
    newTransport = CreateTransport(SIPURL("", transport.GetRemoteAddress(), 0), transport.GetInterface());
    if (newTransport == NULL) {
      PTRACE(1, "SIP\tFailed to create transport for SIPConnection for INVITE from " << request->GetURI() << " for " << toAddr);
      request->SendResponse(transport, SIP_PDU::Failure_NotFound, this);
      return PFalse;
    }
  }

  if (call == NULL) {
    // Get new instance of a call, abort if none created
    call = manager.InternalCreateCall();
    if (call == NULL) {
      request->SendResponse(transport, SIP_PDU::Failure_TemporarilyUnavailable, this);
      return false;
    }
  }

  // ask the endpoint for a connection
  SIPConnection *connection = CreateConnection(*call,
                                               SIPURL::GenerateTag(),
                                               NULL,
                                               request->GetURI(),
                                               newTransport,
                                               request);
  if (!AddConnection(connection)) {
    PTRACE(1, "SIP\tFailed to create SIPConnection for INVITE from " << request->GetURI() << " for " << toAddr);
    request->SendResponse(transport, SIP_PDU::Failure_NotFound, this);
    return PFalse;
  }

  m_receivedConnectionTokens.SetAt(mime.GetCallID(), connection->GetToken());

  // Get the connection to handle the rest of the INVITE in the thread pool
  threadPool.AddWork(new SIP_PDU_Work(*this, connection->GetToken(), request));

  return PTrue;
}

void SIPEndPoint::OnReceivedIntervalTooBrief(SIPTransaction & transaction, SIP_PDU & response)
{
  const SIPMIMEInfo & responseMIME = response.GetMIME();
  PSafePtr<SIPHandler> handler = activeSIPHandlers.FindSIPHandlerByCallID(responseMIME.GetCallID(), PSafeReadOnly);
  if (handler == NULL)
    return;

  SIPTransaction *newTransaction = handler->CreateTransaction(transaction.GetTransport());
  if (newTransaction) {
    handler->SetExpire(responseMIME.GetMinExpires());
    newTransaction->GetMIME().SetExpires(responseMIME.GetMinExpires());
    newTransaction->GetMIME().SetCallID(handler->GetCallID());
    if (newTransaction->Start())
      return;
  }

  PTRACE(1, "SIP\t Could not restart REGISTER after IntervalTooBrief error!");
  handler->OnFailed(SIP_PDU::Failure_IntervalTooBrief);
  return;
}

void SIPEndPoint::OnReceivedAuthenticationRequired(SIPTransaction & transaction, SIP_PDU & response)
{
  // Try to find authentication information for the given call ID
  PSafePtr<SIPHandler> handler = activeSIPHandlers.FindSIPHandlerByCallID(response.GetMIME().GetCallID(), PSafeReadOnly);
  if (handler == NULL)
    return;
  
  handler->OnReceivedAuthenticationRequired(transaction, response);
}


void SIPEndPoint::OnReceivedOK(SIPTransaction & transaction, SIP_PDU & response)
{
  PSafePtr<SIPHandler> handler = activeSIPHandlers.FindSIPHandlerByCallID(response.GetMIME().GetCallID(), PSafeReadOnly);
  if (handler == NULL) 
    return;
  
  handler->OnReceivedOK(transaction, response);
}
    

void SIPEndPoint::OnTransactionFailed(SIPTransaction & transaction)
{
  PSafePtr<SIPHandler> handler = activeSIPHandlers.FindSIPHandlerByCallID(transaction.GetMIME().GetCallID(), PSafeReadOnly);
  if (handler == NULL) 
    return;
  
  handler->OnTransactionFailed(transaction);
}


PBoolean SIPEndPoint::OnReceivedNOTIFY(OpalTransport & transport, SIP_PDU & pdu)
{
  SIPEventPackage eventPackage = pdu.GetMIME().GetEvent();

  PTRACE(3, "SIP\tReceived NOTIFY " << eventPackage);
  
  // A NOTIFY will have the same CallID than the SUBSCRIBE request it corresponds to
  // Technically should check for whole dialog, but call-id will do.
  PSafePtr<SIPHandler> handler = activeSIPHandlers.FindSIPHandlerByCallID(pdu.GetMIME().GetCallID(), PSafeReadOnly);

  if (handler == NULL && eventPackage == SIPSubscribe::MessageSummary) {
    PTRACE(4, "SIP\tWork around Asterisk bug in message-summary event package.");
    SIPURL url_from (pdu.GetMIME().GetFrom());
    SIPURL url_to (pdu.GetMIME().GetTo());
    PString to = url_to.GetUserName() + "@" + url_from.GetHostName();
    handler = activeSIPHandlers.FindSIPHandlerByUrl(to, SIP_PDU::Method_SUBSCRIBE, eventPackage, PSafeReadOnly);
  }

  if (handler == NULL) {
    PTRACE(3, "SIP\tCould not find a SUBSCRIBE corresponding to the NOTIFY " << eventPackage);
    pdu.SendResponse(transport, SIP_PDU::Failure_TransactionDoesNotExist, this);
    return true;
  }

  PTRACE(3, "SIP\tFound a SUBSCRIBE corresponding to the NOTIFY " << eventPackage);
  return handler->OnReceivedNOTIFY(pdu);
}


bool SIPEndPoint::OnReceivedMESSAGE(OpalTransport & transport, SIP_PDU & pdu)
{
  PString from = pdu.GetMIME().GetFrom();
  PINDEX j = from.Find (';');
  if (j != P_MAX_INDEX)
    from = from.Left(j); // Remove all parameters
  j = from.Find ('<');
  if (j != P_MAX_INDEX && from.Find ('>') == P_MAX_INDEX)
    from += '>';

  OnMessageReceived(from, pdu);
  pdu.SendResponse(transport, SIP_PDU::Successful_OK, this);
  return true;
}

bool SIPEndPoint::OnReceivedOPTIONS(OpalTransport & transport, SIP_PDU & pdu)
{
  pdu.SendResponse(transport, SIP_PDU::Successful_OK, this);
  return true;
}


void SIPEndPoint::OnRegistrationStatus(const RegistrationStatus & status)
{
  OnRegistrationStatus(status.m_addressofRecord, status.m_wasRegistering, status.m_reRegistering, status.m_reason);
}


void SIPEndPoint::OnRegistrationStatus(const PString & aor,
                                       PBoolean wasRegistering,
                                       PBoolean /*reRegistering*/,
                                       SIP_PDU::StatusCodes reason)
{
  if (reason == SIP_PDU::Information_Trying)
    return;

  if (reason == SIP_PDU::Successful_OK)
    OnRegistered(aor, wasRegistering);
  else
    OnRegistrationFailed(aor, reason, wasRegistering);
}


void SIPEndPoint::OnRegistrationFailed(const PString & /*aor*/, 
               SIP_PDU::StatusCodes /*reason*/, 
               PBoolean /*wasRegistering*/)
{
}
    

void SIPEndPoint::OnRegistered(const PString & /*aor*/, 
             PBoolean /*wasRegistering*/)
{
}


PBoolean SIPEndPoint::IsRegistered(const PString & url, bool includeOffline) 
{
  PSafePtr<SIPHandler> handler = activeSIPHandlers.FindSIPHandlerByUrl(url, SIP_PDU::Method_REGISTER, PSafeReadOnly);
  if (handler == NULL)
    return PFalse;

  return includeOffline ? (handler->GetState() != SIPHandler::Unsubscribed)
                        : (handler->GetState() == SIPHandler::Subscribed);
}


PBoolean SIPEndPoint::IsSubscribed(const PString & eventPackage, const PString & to, bool includeOffline) 
{
  PSafePtr<SIPHandler> handler = activeSIPHandlers.FindSIPHandlerByUrl(to, SIP_PDU::Method_SUBSCRIBE, eventPackage, PSafeReadOnly);
  if (handler == NULL)
    return PFalse;

  return includeOffline ? (handler->GetState() != SIPHandler::Unsubscribed)
                        : (handler->GetState() == SIPHandler::Subscribed);
}


void SIPEndPoint::OnSubscriptionStatus(const PString & /*eventPackage*/,
                                       const SIPURL & /*aor*/,
                                       bool /*wasSubscribing*/,
                                       bool /*reSubscribing*/,
                                       SIP_PDU::StatusCodes /*reason*/)
{
}


bool SIPEndPoint::Register(const PString & host,
                           const PString & user,
                           const PString & authName,
                           const PString & password,
                           const PString & realm,
                           unsigned expire,
                           const PTimeInterval & minRetryTime,
                           const PTimeInterval & maxRetryTime)
{
  SIPRegister::Params params;

  if (user.Find('@') == P_MAX_INDEX) {
    if (user.IsEmpty())
      params.m_addressOfRecord = GetDefaultLocalPartyName() + '@' + host;
    else
      params.m_addressOfRecord = user + '@' + host;
  }
  else {
    params.m_addressOfRecord = user;
    if (!host.IsEmpty())
      params.m_addressOfRecord += ";proxy=" + host;
  }

  params.m_authID = authName;
  params.m_password = password;
  params.m_realm = realm;
  params.m_expire = expire != 0 ? expire : GetRegistrarTimeToLive().GetSeconds();
  params.m_minRetryTime = minRetryTime;
  params.m_maxRetryTime = maxRetryTime;

  PString dummy;
  return Register(params, dummy);
}


bool SIPEndPoint::Register(const SIPRegister::Params & params, PString & aor)
{
  if (params.m_expire == 0) {
    aor = params.m_addressOfRecord;
    return Unregister(params.m_addressOfRecord);
  }

  PTRACE(4, "SIP\tStart REGISTER\n"
            "        aor=" << params.m_addressOfRecord << "\n"
            "  registrar=" << params.m_registrarAddress << "\n"
            "    contact=" << params.m_contactAddress << "\n"
            "     authID=" << params.m_authID << "\n"
            "      realm=" << params.m_realm << "\n"
            "     expire=" << params.m_expire << "\n"
            "    restore=" << params.m_restoreTime << "\n"
            "   minRetry=" << params.m_minRetryTime << "\n"
            "   maxRetry=" << params.m_maxRetryTime);
  PSafePtr<SIPRegisterHandler> handler = PSafePtrCast<SIPHandler, SIPRegisterHandler>(
          activeSIPHandlers.FindSIPHandlerByUrl(params.m_addressOfRecord, SIP_PDU::Method_REGISTER, PSafeReadWrite));

  // If there is already a request with this URL and method, 
  // then update it with the new information
  if (handler != NULL)
    handler->UpdateParameters(params);
  else {
    // Otherwise create a new request with this method type
    handler = CreateRegisterHandler(params);
    handler.SetSafetyMode(PSafeReadWrite);
    activeSIPHandlers.Append(handler);
  }

  aor = handler->GetAddressOfRecord().AsString();

  if (handler->SendRequest(SIPHandler::Subscribing))
    return true;

  activeSIPHandlers.Remove(handler);
  return false;
}


SIPRegisterHandler * SIPEndPoint::CreateRegisterHandler(const SIPRegister::Params & params)
{
  return new SIPRegisterHandler(*this, params);
}


PBoolean SIPEndPoint::Unregister(const PString & addressOfRecord)
{
  PSafePtr<SIPHandler> handler = activeSIPHandlers.FindSIPHandlerByUrl(addressOfRecord, SIP_PDU::Method_REGISTER, PSafeReadOnly);
  if (handler == NULL) {
    PTRACE(1, "SIP\tCould not find active REGISTER for " << addressOfRecord);
    return false;
  }

  return handler->SendRequest(SIPHandler::Unsubscribing);
}


bool SIPEndPoint::UnregisterAll()
{
  bool ok = true;

  for (PSafePtr<SIPHandler> handler(activeSIPHandlers, PSafeReadOnly); handler != NULL; ++handler) {
    if (handler->GetMethod() == SIP_PDU::Method_REGISTER) {
      if (!handler->SendRequest(SIPHandler::Unsubscribing))
        ok = false;
    }
  }

  return ok;
}


bool SIPEndPoint::Subscribe(SIPSubscribe::PredefinedPackages eventPackage, unsigned expire, const PString & to)
{
  SIPSubscribe::Params params(eventPackage);
  params.m_addressOfRecord = to;
  params.m_expire = expire;

  PString dummy;
  return Subscribe(params, dummy);
}


bool SIPEndPoint::Subscribe(const SIPSubscribe::Params & params, PString & aor)
{
  // Zero is special case of unsubscribe
  if (params.m_expire == 0) {
    aor = params.m_addressOfRecord;
    return Unsubscribe(params.m_eventPackage, params.m_addressOfRecord);
  }

  // Create the SIPHandler structure
  PSafePtr<SIPSubscribeHandler> handler = PSafePtrCast<SIPHandler, SIPSubscribeHandler>(
          activeSIPHandlers.FindSIPHandlerByUrl(params.m_addressOfRecord, SIP_PDU::Method_SUBSCRIBE, params.m_eventPackage, PSafeReadOnly));
  
  // If there is already a request with this URL and method, 
  // then update it with the new information
  if (handler != NULL && handler->GetState() != SIPHandler::Unsubscribed)
    handler->UpdateParameters(params);
  else {
    // Otherwise create a new request with this method type
    handler = new SIPSubscribeHandler(*this, params);
    activeSIPHandlers.Append(handler);
  }

  aor = handler->GetAddressOfRecord().AsString();

  return handler->SendRequest(SIPHandler::Subscribing);
}


bool SIPEndPoint::Unsubscribe(SIPSubscribe::PredefinedPackages eventPackage, const PString & to)
{
  return Unsubscribe(SIPEventPackage(eventPackage), to);
}


bool SIPEndPoint::Unsubscribe(const PString & eventPackage, const PString & to)
{
  // Create the SIPHandler structure
  PSafePtr<SIPHandler> handler = activeSIPHandlers.FindSIPHandlerByUrl(to, SIP_PDU::Method_SUBSCRIBE, eventPackage, PSafeReadOnly);
  if (handler == NULL) {
    PTRACE(1, "SIP\tCould not find active SUBSCRIBE for " << to);
    return PFalse;
  }

  return handler->SendRequest(SIPHandler::Unsubscribing);
}


bool SIPEndPoint::UnsubcribeAll(SIPSubscribe::PredefinedPackages eventPackage)
{
  return UnsubcribeAll(SIPEventPackage(eventPackage));
}


bool SIPEndPoint::UnsubcribeAll(const PString & eventPackage)
{
  bool ok = true;

  for (PSafePtr<SIPHandler> handler(activeSIPHandlers, PSafeReadOnly); handler != NULL; ++handler) {
    if (handler->GetMethod() == SIP_PDU::Method_SUBSCRIBE && handler->GetEventPackage() == eventPackage) {
      if (!handler->SendRequest(SIPHandler::Unsubscribing))
        ok = false;
    }
  }

  return ok;
}


bool SIPEndPoint::CanNotify(const PString & eventPackage)
{
  return SIPEventPackage(SIPSubscribe::Dialog) == eventPackage;
}


bool SIPEndPoint::Notify(const SIPURL & aor, const PString & eventPackage, const PObject & body)
{
  bool atLeastOne = false;

  for (PSafePtr<SIPHandler> handler(activeSIPHandlers, PSafeReadWrite); handler != NULL; ++handler) {
    if (handler->GetMethod() == SIP_PDU::Method_NOTIFY &&
        handler->GetAddressOfRecord() == aor &&
        handler->GetEventPackage() == eventPackage &&
        handler->SendNotify(&body))
      atLeastOne = true;
  }

  return atLeastOne;
}

PBoolean SIPEndPoint::Message(const PString & to, const PString & body)
{
  // Create the SIPHandler structure
  PSafePtr<SIPHandler> handler = activeSIPHandlers.FindSIPHandlerByUrl(to, SIP_PDU::Method_MESSAGE, PSafeReadOnly);

  // Otherwise create a new request with this method type
  if (handler != NULL)
    handler->SetBody(body);
  else {
    handler = new SIPMessageHandler(*this, to, body, "", SIPTransaction::GenerateCallID());
    activeSIPHandlers.Append(handler);
  }

  return handler->SendRequest(SIPHandler::Subscribing);
}

PBoolean SIPEndPoint::Message(const PString & to, const PString & body, const PString & remoteContact, const PString & callId)
{
  // Create the SIPHandler structure
  PSafePtr<SIPHandler> handler = activeSIPHandlers.FindSIPHandlerByCallID(callId, PSafeReadOnly);
  
  // Otherwise create a new request with this method type
  if (handler != NULL)
    handler->SetBody(body);
  else {
    handler = new SIPMessageHandler(*this, to, body, remoteContact, callId);
    activeSIPHandlers.Append(handler);
  }

  return handler->SendRequest(SIPHandler::Subscribing);
}


void SIPEndPoint::OnMessageReceived(const SIPURL & from, const SIP_PDU & pdu)
{
  OnMessageReceived(from, pdu.GetEntityBody());

#if OPAL_HAS_SIPIM
  m_sipIMManager.OnReceivedMessage(pdu);
#endif
}


void SIPEndPoint::OnMessageReceived (const SIPURL & /*from*/, const PString & /*body*/)
{
}


PBoolean SIPEndPoint::Ping(const PString & to)
{
  PSafePtr<SIPHandler> handler = activeSIPHandlers.FindSIPHandlerByUrl(to, SIP_PDU::Method_PING, PSafeReadOnly);
  if (handler == NULL)
    handler = new SIPPingHandler(*this, to);

  return handler->SendRequest(SIPHandler::Subscribing);
}


bool SIPEndPoint::Publish(const SIPSubscribe::Params & params, const PString & body, PString & aor)
{
  PSafePtr<SIPHandler> handler = activeSIPHandlers.FindSIPHandlerByUrl(params.m_addressOfRecord, SIP_PDU::Method_PUBLISH, PSafeReadWrite);
  if (handler != NULL)
    handler->SetBody(body);
  else {
    handler = new SIPPublishHandler(*this, params, body);
    activeSIPHandlers.Append(handler);
  }

  aor = handler->GetAddressOfRecord().AsString();

  return handler->SendRequest(params.m_expire != 0 ? SIPHandler::Subscribing : SIPHandler::Unsubscribing);
}


bool SIPEndPoint::Publish(const PString & to, const PString & body, unsigned expire)
{
  SIPSubscribe::Params params(SIPSubscribe::Presence);
  params.m_addressOfRecord = to;
  params.m_expire = expire;

  PString aor;
  return Publish(params, body, aor);
}


bool SIPEndPoint::PublishPresence(const SIPPresenceInfo & info, unsigned expire)
{
  return Publish(info.m_address, info.AsString(), info.m_basic != SIPPresenceInfo::Closed ? expire : 0);
}


void SIPEndPoint::OnPresenceInfoReceived(const SIPPresenceInfo & info)
{
  // For backward compatibility
  switch (info.m_basic) {
    case SIPPresenceInfo::Open :
      OnPresenceInfoReceived(info.m_address, "open", info.m_note);
      break;
    case SIPPresenceInfo::Closed :
      OnPresenceInfoReceived(info.m_address, "closed", info.m_note);
      break;
    default :
      OnPresenceInfoReceived(info.m_address, PString::Empty(), info.m_note);
  }
}


void SIPEndPoint::OnPresenceInfoReceived(const PString & /*user*/,
                                         const PString & /*basic*/,
                                         const PString & /*note*/)
{
}


void SIPEndPoint::OnDialogInfoReceived(const SIPDialogNotification & PTRACE_PARAM(info))
{
  PTRACE(3, "SIP\tReceived dialog info for \"" << info.m_entity << "\" id=\"" << info.m_callId << '"');
}


void SIPEndPoint::SendNotifyDialogInfo(const SIPDialogNotification & info)
{
  Notify(info.m_entity, SIPEventPackage(SIPSubscribe::Dialog), info);
}


void SIPEndPoint::OnMessageFailed(const SIPURL & /* messageUrl */,
          SIP_PDU::StatusCodes /* reason */)
{
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


void SIPEndPoint::SetProxy(const SIPURL & url) 
{ 
  proxy = url; 
}


PString SIPEndPoint::GetUserAgent() const 
{
  return userAgentString;
}

void SIPEndPoint::OnStartTransaction(SIPConnection & /*conn*/, SIPTransaction & /*transaction*/)
{
}

unsigned SIPEndPoint::GetAllowedMethods() const
{
  return (1<<SIP_PDU::Method_INVITE   )|
         (1<<SIP_PDU::Method_ACK      )|
         (1<<SIP_PDU::Method_CANCEL   )|
         (1<<SIP_PDU::Method_BYE      )|
         (1<<SIP_PDU::Method_OPTIONS  )|
         (1<<SIP_PDU::Method_NOTIFY   )|
         (1<<SIP_PDU::Method_REFER    )|
         (1<<SIP_PDU::Method_MESSAGE  )|
         (1<<SIP_PDU::Method_INFO     )|
         (1<<SIP_PDU::Method_PING     )|
         (1<<SIP_PDU::Method_SUBSCRIBE);
}


PBoolean SIPEndPoint::GetAuthentication(const PString & authRealm, PString & realm, PString & user, PString & password) 
{
  PSafePtr<SIPHandler> handler = activeSIPHandlers.FindSIPHandlerByAuthRealm(authRealm, PString::Empty(), PSafeReadOnly);
  if (handler == NULL)
    return PFalse;

  realm    = handler->GetRealm();
  user     = handler->GetUsername();
  password = handler->GetPassword();

  return PTrue;
}

SIPURL SIPEndPoint::GetRegisteredPartyName(const SIPURL & url, const OpalTransport & transport)
{
  PSafePtr<SIPHandler> handler = activeSIPHandlers.FindSIPHandlerByDomain(url.GetHostName(), SIP_PDU::Method_REGISTER, PSafeReadOnly);
  if (handler == NULL) 
    return GetDefaultRegisteredPartyName(transport);

  return handler->GetAddressOfRecord();
}


SIPURL SIPEndPoint::GetDefaultRegisteredPartyName(const OpalTransport & transport)
{
  PIPSocket::Address myAddress(0);
  WORD myPort = GetDefaultSignalPort();
  OpalTransportAddressArray interfaces = GetInterfaceAddresses();

  PIPSocket::Address transportAddress;
  if (transport.GetLocalAddress().GetIpAddress(transportAddress)) {
    for (PINDEX i = 0; i < interfaces.GetSize(); ++i) {
      PIPSocket::Address interfaceAddress;
      WORD interfacePort = myPort;
      if (interfaces[i].GetIpAddress(interfaceAddress) && interfaceAddress == transportAddress) {
        myAddress = interfaceAddress;
        myPort = interfacePort;
        break;
      }
    }
  }

  if (!myAddress.IsValid() && !interfaces.IsEmpty())
    interfaces[0].GetIpAndPort(myAddress, myPort);

  if (!myAddress.IsValid())
    PIPSocket::GetHostAddress(myAddress);

  if (transport.GetRemoteAddress().GetIpAddress(transportAddress))
    GetManager().TranslateIPAddress(myAddress, transportAddress);

  OpalTransportAddress addr(myAddress, myPort, transport.GetLocalAddress().GetProto());
  PString defPartyName(GetDefaultLocalPartyName());
  SIPURL rpn;
  PINDEX pos;
  if ((pos = defPartyName.Find('@')) == P_MAX_INDEX) 
    rpn = SIPURL(defPartyName, addr, myPort);
  else {
    rpn = SIPURL(defPartyName.Left(pos), addr, myPort);   // set transport from address
    rpn.SetHostName(defPartyName.Right(pos+1));
  }

  rpn.SetDisplayName(GetDefaultDisplayName());
  return rpn;
}


SIPURL SIPEndPoint::GetContactURL(const OpalTransport &transport, const PString & userName, const PString & host)
{
  PSafePtr<SIPHandler> handler = activeSIPHandlers.FindSIPHandlerByDomain(host, SIP_PDU::Method_REGISTER, PSafeReadOnly);
  if (handler != NULL) {
    OpalTransport * handlerTransport = handler->GetTransport();
    if (handlerTransport != NULL)
      return GetLocalURL(*handlerTransport, handler->GetAddressOfRecord().GetUserName());
  }

  return GetLocalURL(transport, userName);
}


SIPURL SIPEndPoint::GetLocalURL(const OpalTransport &transport, const PString & userName)
{
  PIPSocket::Address ip(PIPSocket::GetDefaultIpAny());
  OpalTransportAddress contactAddress = transport.GetLocalAddress();
  WORD contactPort = GetDefaultSignalPort();
  if (transport.IsOpen())
    transport.GetLocalAddress().GetIpAndPort(ip, contactPort);
  else {
    for (OpalListenerList::iterator listener = listeners.begin(); listener != listeners.end(); ++listener) {
      OpalTransportAddress binding = listener->GetLocalAddress();
      if (transport.IsCompatibleTransport(binding)) {
        binding.GetIpAndPort(ip, contactPort);
        break;
      }
    }
  }

  PIPSocket::Address localIP;
  WORD localPort;
  
  if (contactAddress.GetIpAndPort(localIP, localPort)) {
    PIPSocket::Address remoteIP;
    if (transport.GetRemoteAddress().GetIpAddress(remoteIP)) {
      GetManager().TranslateIPAddress(localIP, remoteIP);    
      contactPort = localPort;
      PString proto = transport.GetProtoPrefix();
      contactAddress = OpalTransportAddress(localIP, contactPort, proto.Left(proto.GetLength()-1)); //xxxxxx
    }
  }

  SIPURL contact(userName, contactAddress, contactPort);

  return contact;
}


void SIPEndPoint::OnRTPStatistics(const SIPConnection & connection,
                                  const RTP_Session & session) const
{
  manager.OnRTPStatistics(connection, session);
}

SIPEndPoint::SIP_PDU_Thread::SIP_PDU_Thread(PThreadPoolBase & _pool)
  : PThreadPoolWorkerBase(_pool)
{
  SetPriority(HighestPriority);
}

unsigned SIPEndPoint::SIP_PDU_Thread::GetWorkSize() const { return pduQueue.size(); }

void SIPEndPoint::SIP_PDU_Thread::OnAddWork(SIP_PDU_Work * work)
{
  PWaitAndSignal m(mutex);
  pduQueue.push(work);
  if (pduQueue.size() == 1)
    sync.Signal();
}

void SIPEndPoint::SIP_PDU_Thread::OnRemoveWork(SIP_PDU_Work *) 
{ }

void SIPEndPoint::SIP_PDU_Thread::Shutdown()
{
  shutdown = true;
  sync.Signal();
}


void SIPEndPoint::SIP_PDU_Thread::Main()
{
  while (!shutdown) {
    mutex.Wait();
    if (pduQueue.size() == 0) {
      mutex.Signal();
      sync.Wait();
      continue;
    }

    SIP_PDU_Work * work = pduQueue.front();
    pduQueue.pop();
    mutex.Signal();

    work->OnReceivedPDU();
    delete work;
  }
}


SIPEndPoint::SIP_PDU_Work::SIP_PDU_Work(SIPEndPoint & ep, const PString & token, SIP_PDU * pdu)
  : m_endpoint(ep)
  , m_token(token)
  , m_pdu(pdu)
{
}


SIPEndPoint::SIP_PDU_Work::~SIP_PDU_Work()
{
  delete m_pdu;
}


void SIPEndPoint::SIP_PDU_Work::OnReceivedPDU()
{
  if (PAssertNULL(m_pdu) == NULL)
    return;

  if (m_pdu->GetMethod() == SIP_PDU::NumMethods) {
    PSafePtr<SIPTransaction> transaction = m_endpoint.GetTransaction(m_pdu->GetTransactionID(), PSafeReference);
    if (transaction != NULL)
      transaction->OnReceivedResponse(*m_pdu);
    else {
      PTRACE(3, "SIP\tCannot find transaction for response");
    }
    return;
  }

  if (PAssert(!m_token.IsEmpty(), PInvalidParameter)) {
    PSafePtr<SIPConnection> connection = m_endpoint.GetSIPConnectionWithLock(m_token, PSafeReference);
    if (connection != NULL) 
      connection->OnReceivedPDU(*m_pdu);
  }
}


SIPEndPoint::InterfaceMonitor::InterfaceMonitor(SIPEndPoint & ep, PINDEX priority)
  : PInterfaceMonitorClient(priority) 
  , m_endpoint(ep)
{
}

        
void SIPEndPoint::InterfaceMonitor::OnAddInterface(const PIPSocket::InterfaceEntry &)
{
  if (priority == SIPEndPoint::LowPriority) {
    for (PSafePtr<SIPHandler> handler(m_endpoint.activeSIPHandlers, PSafeReadOnly); handler != NULL; ++handler) {
      if (handler->GetState() == SIPHandler::Unavailable)
        handler->SendRequest(SIPHandler::Restoring);
    }
  } else {
    // special case if interface filtering is used: A new interface may 'hide' the old interface.
    // If this is the case, remove the transport interface. 
    //
    // There is a race condition: If the transport interface binding is cleared AFTER
    // PMonitoredSockets::ReadFromSocket() is interrupted and starts listening again,
    // the transport will still listen on the old interface only. Therefore, clear the
    // socket binding BEFORE the monitored sockets update their interfaces.
    if (PInterfaceMonitor::GetInstance().HasInterfaceFilter()) {
      for (PSafePtr<SIPHandler> handler(m_endpoint.activeSIPHandlers, PSafeReadOnly); handler != NULL; ++handler) {
        OpalTransport *transport = handler->GetTransport();
        if (transport != NULL) {
          PString iface = transport->GetInterface();
          if (iface.IsEmpty()) // not connected
            continue;
          
          PIPSocket::Address addr;
          if (!transport->GetRemoteAddress().GetIpAddress(addr))
            continue;
          
          PStringArray ifaces = GetInterfaces(PFalse, addr);
          
          if (ifaces.GetStringsIndex(iface) == P_MAX_INDEX) { // original interface no longer available
            transport->SetInterface(PString::Empty());
            handler->SetState(SIPHandler::Unavailable);
          }
        }
      }
    }
  }
}


void SIPEndPoint::InterfaceMonitor::OnRemoveInterface(const PIPSocket::InterfaceEntry & entry)
{
  if (priority == SIPEndPoint::LowPriority) {
    for (PSafePtr<SIPHandler> handler(m_endpoint.activeSIPHandlers, PSafeReadOnly); handler != NULL; ++handler) {
      if (handler->GetState() == SIPHandler::Subscribed &&
          handler->GetTransport() != NULL &&
          handler->GetTransport()->GetInterface().Find(entry.GetName()) != P_MAX_INDEX) {
        handler->GetTransport()->SetInterface(PString::Empty());
        handler->SendRequest(SIPHandler::Refreshing);
      }
    }
  }
}


#endif // OPAL_SIP

// End of file ////////////////////////////////////////////////////////////////
