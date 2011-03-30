/*
 * handlers.cxx
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
 * The Initial Developer of the Original Code is Damien Sandras. 
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision$
 * $Author$
 * $Date$
 *
 * This code implements all or part of the following RFCs
 *
 * RFC 4479 "A Data Model for Presence"
 * RFC 4480 "RPID: Rich Presence Extensions to the Presence Information Data Format (PIDF)"
 */

#include <ptlib.h>
#include <opal/buildopts.h>

#if OPAL_SIP

#ifdef __GNUC__
#pragma implementation "handlers.h"
#endif

#include <sip/handlers.h>

#include <ptclib/pdns.h>
#include <ptclib/enum.h>
#include <sip/sipep.h>
#include <ptclib/pxml.h>

#include <algorithm>


#define new PNEW


#if PTRACING
ostream & operator<<(ostream & strm, SIPHandler::State state)
{
  static const char * const StateNames[] = {
    "Subscribed", "Subscribing", "Unavailable", "Refreshing", "Restoring", "Unsubscribing", "Unsubscribed"
  };
  if (state < PARRAYSIZE(StateNames))
    strm << StateNames[state];
  else
    strm << (unsigned)state;
  return strm;
}
#endif


////////////////////////////////////////////////////////////////////////////

SIPHandler::SIPHandler(SIP_PDU::Methods method, SIPEndPoint & ep, const SIPParameters & params)
  : endpoint(ep)
  , authentication(NULL)
  , m_username(params.m_authID)
  , m_password(params.m_password)
  , m_realm(params.m_realm)
  , m_transport(NULL)
  , m_method(method)
  , m_addressOfRecord(params.m_addressOfRecord)
  , m_remoteAddress(params.m_remoteAddress)
  , callID(SIPTransaction::GenerateCallID())
  , expire(params.m_expire)
  , originalExpire(params.m_expire)
  , offlineExpire(params.m_restoreTime)
  , authenticationAttempts(0)
  , m_state(Unavailable)
  , m_receivedResponse(false)
  , retryTimeoutMin(params.m_minRetryTime)
  , retryTimeoutMax(params.m_maxRetryTime)
  , m_proxy(params.m_proxyAddress)
{
  m_transactions.DisallowDeleteObjects();
  expireTimer.SetNotifier(PCREATE_NOTIFIER(OnExpireTimeout));

  if (m_proxy.IsEmpty())
    m_proxy = ep.GetProxy();

  PTRACE(4, "SIP\tConstructed " << m_method << " handler for " << m_addressOfRecord);
}


SIPHandler::~SIPHandler() 
{
  expireTimer.Stop();

  if (m_transport) {
    m_transport->CloseWait();
    delete m_transport;
  }

  delete authentication;

  PTRACE(4, "SIP\tDestroyed " << m_method << " handler for " << m_addressOfRecord);
}


PObject::Comparison SIPHandler::Compare(const PObject & obj) const
{
  PAssert(PIsDescendant(&obj, SIPHandler), PInvalidCast);
  const SIPHandler * other = dynamic_cast<const SIPHandler *>(&obj);
  return other != NULL ? callID.Compare(other->callID) : GreaterThan;
}


bool SIPHandler::ShutDown()
{
  PSafeList<SIPTransaction> transactions;

  {
    PSafeLockReadWrite mutex(*this);
    if (!mutex.IsLocked())
      return true;

    while (!m_stateQueue.empty())
      m_stateQueue.pop();

    switch (GetState()) {
      case Subscribed :
      case Unavailable :
        SendRequest(Unsubscribing);
      case Unsubscribing :
        return m_transactions.IsEmpty();

      default :
        break;
    }

    transactions = m_transactions;
  }

  for (PSafePtr<SIPTransaction> transaction(transactions, PSafeReference); transaction != NULL; ++transaction)
    transaction->Abort();

  return true;
}


void SIPHandler::SetState(SIPHandler::State newState) 
{
  if (m_state == newState)
    return;

  PTRACE(4, "SIP\tChanging " << GetMethod() << " handler from " << GetState() << " to " << newState
         << ", target=" << GetAddressOfRecord() << ", id=" << GetCallID());

  m_state = newState;

  switch (m_state) {
    case Subscribing :
    case Refreshing :
    case Restoring :
    case Unsubscribing :
      return;

    default :
      break;
  }

  if (m_stateQueue.empty())
    return;

  newState = m_stateQueue.front();
  m_stateQueue.pop();
  SendRequest(newState);
}


bool SIPHandler::ActivateState(SIPHandler::State newState)
{
  // If subscribing with zero expiry time, is same as unsubscribe
  if (newState == Subscribing && expire == 0)
    newState = Unsubscribing;

  // If unsubscribing and never got a response from server, rather than trying
  // to send more unsuccessful packets, abort transactions and mark Unsubscribed
  if (newState == Unsubscribing && !m_receivedResponse) {
    ShutDown();
    return true;
  }

  static const enum {
    e_Invalid,
    e_NoChange,
    e_Execute,
    e_Queue
  } StateChangeActions[NumStates][NumStates] =
  {
    /* old-state
           V      new-state-> Subscribed  Subscribing Unavailable Refreshing  Restoring   Unsubscribing Unsubscribed */
    /* Subscribed        */ { e_NoChange, e_Execute,  e_Invalid,  e_Execute,  e_Execute,  e_Execute,    e_Invalid  },
    /* Subscribing       */ { e_Invalid,  e_Queue,    e_Invalid,  e_NoChange, e_NoChange, e_Queue,      e_Invalid  },
    /* Unavailable       */ { e_Invalid,  e_Execute,  e_NoChange, e_Execute,  e_Execute,  e_Execute,    e_Invalid  },
    /* Refreshing        */ { e_Invalid,  e_Queue,    e_Invalid,  e_NoChange, e_NoChange, e_Queue,      e_Invalid  },
    /* Restoring         */ { e_Invalid,  e_Queue,    e_Invalid,  e_NoChange, e_NoChange, e_Queue,      e_Invalid  },
    /* Unsubscribing     */ { e_Invalid,  e_Invalid,  e_Invalid,  e_Invalid,  e_NoChange, e_NoChange,   e_Invalid  },
    /* Unsubscribed      */ { e_Invalid,  e_Invalid,  e_Invalid,  e_Invalid,  e_Invalid,  e_NoChange,   e_NoChange }
  };

  PSafeLockReadWrite mutex(*this);
  if (!mutex.IsLocked())
    return true;

  switch (StateChangeActions[GetState()][newState]) {
    case e_Invalid :
      PTRACE(2, "SIP\tCannot change state to " << newState << " for " << GetMethod()
             << " handler while in " << GetState() << " state, target="
             << GetAddressOfRecord() << ", id=" << GetCallID());
      return false;

    case e_NoChange :
      PTRACE(4, "SIP\tAlready in state " << GetState() << " for " << GetMethod()
             << " handler, target=" << GetAddressOfRecord() << ", id=" << GetCallID());
      break;

    case e_Execute :
      PTRACE(4, "SIP\tExecuting state change to " << newState << " for " << GetMethod()
             << " handler, target=" << GetAddressOfRecord() << ", id=" << GetCallID());
      return SendRequest(newState);

    case e_Queue :
      PTRACE(3, "SIP\tQueueing state change to " << newState << " for " << GetMethod()
             << " handler while in " << GetState() << " state, target="
             << GetAddressOfRecord() << ", id=" << GetCallID());
      m_stateQueue.push(newState);
      break;
  }

  return true;
}


PBoolean SIPHandler::SendRequest(SIPHandler::State newState)
{
  expireTimer.Stop(false); // Stop automatic retry

  SetState(newState);

  if (GetTransport() == NULL)
    OnFailed(SIP_PDU::Local_BadTransportAddress);
  else {
    // Restoring or first time, try every interface
    if (newState == Restoring || m_transport->GetInterface().IsEmpty()) {
      PWaitAndSignal mutex(m_transport->GetWriteMutex());
      if (m_transport->WriteConnect(WriteSIPHandler, this))
        return true;
    }
    else {
      // We contacted the server on an interface last time, assume it still works!
      if (WriteSIPHandler(*m_transport, false))
        return true;
    }

    OnFailed(SIP_PDU::Local_TransportError);
  }

  if (newState == Unsubscribing) {
    // Transport level error, probably never going to get the unsubscribe through
    SetState(Unsubscribed);
    return true;
  }

  RetryLater(offlineExpire);
  return true;
}


OpalTransport * SIPHandler::GetTransport()
{
  if (m_transport != NULL) {
    if (m_transport->IsOpen())
      return m_transport;

    m_transport->CloseWait();
    delete m_transport;
    m_transport = NULL;
  }

  if (m_proxy.IsEmpty()) {
    // Look for a "proxy" parameter to override default proxy
    const PStringToString & params = m_remoteAddress.GetParamVars();
    if (params.Contains(OPAL_PROXY_PARAM)) {
      m_proxy.Parse(params(OPAL_PROXY_PARAM));
      m_remoteAddress.SetParamVar(OPAL_PROXY_PARAM, PString::Empty());
    }
  }

  SIPURL url;
  if (!m_proxy.IsEmpty())
    url = m_proxy;
  else {
    url = m_remoteAddress;
    url.AdjustToDNS();
  }

  // Must specify a network interface or get infinite recursion
  m_transport = endpoint.CreateTransport(url, "*");
  return m_transport;
}


void SIPHandler::SetExpire(int e)
{
  expire = e;
  PTRACE(3, "SIP\tExpiry time for " << GetMethod() << " set to " << expire << " seconds.");

  // Only modify the originalExpire for future requests if IntervalTooBrief gives
  // a bigger expire time. expire itself will always reflect the proxy decision
  // (bigger or lower), but originalExpire determines what is used in future 
  // requests and is only modified if interval too brief
  if (originalExpire < e)
    originalExpire = e;

  // retry before the expire time.
  // if the expire time is more than 20 mins, retry 10mins before expiry
  // if the expire time is less than 20 mins, retry after half of the expiry time
  if (expire > 0 && GetState() < Unsubscribing)
    expireTimer.SetInterval(0, (unsigned)(expire < 20*60 ? expire/2 : expire-10*60));
}


PBoolean SIPHandler::WriteSIPHandler(OpalTransport & transport, void * param)
{
  return param != NULL && ((SIPHandler *)param)->WriteSIPHandler(transport, true);
}


bool SIPHandler::WriteSIPHandler(OpalTransport & transport, bool /*forked*/)
{
  SIPTransaction * transaction = CreateTransaction(transport);

  if (transaction != NULL) {
    for (PINDEX i = 0; i < m_mime.GetSize(); ++i) 
      transaction->GetMIME().SetAt(m_mime.GetKeyAt(i), PString(m_mime.GetDataAt(i)));
    if (GetState() == Unsubscribing)
      transaction->GetMIME().SetExpires(0);
    if (authentication != NULL) {
      SIPAuthenticator auth(*transaction);
      authentication->Authorise(auth); // If already have info from last time, use it!
    }
    if (transaction->Start()) {
      m_transactions.Append(transaction);
      return true;
    }
  }

  PTRACE(2, "SIP\tDid not start transaction on " << transport);
  return false;
}


PBoolean SIPHandler::OnReceivedNOTIFY(SIP_PDU & /*response*/)
{
  return PFalse;
}


void SIPHandler::OnReceivedResponse(SIPTransaction & transaction, SIP_PDU & response)
{
  unsigned responseClass = response.GetStatusCode()/100;
  if (responseClass < 2)
    return; // Don't do anything with pending responses.

  // Received a response, so indicate that and collapse the forking on multiple interfaces.
  m_receivedResponse = true;

  m_transactions.Remove(&transaction); // Take this transaction out of list

  // And kill all the rest
  PSafePtr<SIPTransaction> transToGo;
  while ((transToGo = m_transactions.GetAt(0)) != NULL) {
    m_transactions.Remove(transToGo);
    transToGo->Abort();
  }

  // Finally end connect mode on the transport
  m_transport->SetInterface(transaction.GetInterface());

  switch (response.GetStatusCode()) {
    case SIP_PDU::Failure_UnAuthorised :
    case SIP_PDU::Failure_ProxyAuthenticationRequired :
      OnReceivedAuthenticationRequired(transaction, response);
      break;

    case SIP_PDU::Failure_IntervalTooBrief :
      OnReceivedIntervalTooBrief(transaction, response);
      break;

    case SIP_PDU::Failure_TemporarilyUnavailable:
      OnReceivedTemporarilyUnavailable(transaction, response);
      break;

    default :
      if (responseClass == 2)
        OnReceivedOK(transaction, response);
      else
        OnFailed(response);
  }
}


void SIPHandler::OnReceivedIntervalTooBrief(SIPTransaction & /*transaction*/, SIP_PDU & response)
{
  SetExpire(response.GetMIME().GetMinExpires());

  // Restart the transaction with new authentication handler
  SendRequest(GetState());
}


void SIPHandler::OnReceivedTemporarilyUnavailable(SIPTransaction & /*transaction*/, SIP_PDU & response)
{
  OnFailed(SIP_PDU::Failure_TemporarilyUnavailable);
  RetryLater(response.GetMIME().GetInteger("Retry-After", offlineExpire));
}


void SIPHandler::OnReceivedAuthenticationRequired(SIPTransaction & transaction, SIP_PDU & response)
{
  bool isProxy = response.GetStatusCode() == SIP_PDU::Failure_ProxyAuthenticationRequired;

#if PTRACING
  const char * proxyTrace = isProxy ? "Proxy " : "";
#endif
  PTRACE(3, "SIP\tReceived " << proxyTrace << "Authentication Required response");
  
  // Abort after some unsuccesful authentication attempts. This is required since
  // some implementations return "401 Unauthorized" with a different nonce at every
  // time.
  if (authenticationAttempts >= 10) {
    PTRACE(1, "SIP\tAborting after " << authenticationAttempts << " attempts to REGISTER/SUBSCRIBE");
    OnFailed(SIP_PDU::Failure_UnAuthorised);
    return;
  }
  ++authenticationAttempts;

  // authenticate 
  PString errorMsg;
  SIPAuthentication * newAuth = PHTTPClientAuthentication::ParseAuthenticationRequired(isProxy, response.GetMIME(), errorMsg);
  if (newAuth == NULL) {
    PTRACE(2, "SIP\t" << errorMsg);
    OnFailed(SIP_PDU::Failure_Forbidden);
    return;
  }

  // If either username or password blank, try and fine values from other
  // handlers which might be logged into the realm, or the proxy, if one.
  PString username = m_username;
  PString password = m_password;
  if (username.IsEmpty() || password.IsEmpty()) {
    // Try to find authentication parameters for the given realm,
    // if not, use the proxy authentication parameters (if any)
    if (endpoint.GetAuthentication(newAuth->GetAuthRealm(), username, password)) {
      PTRACE (3, "SIP\tFound auth info for realm " << newAuth->GetAuthRealm());
    }
    else if (username.IsEmpty()) {
      if (m_proxy.IsEmpty()) {
        delete newAuth;
        PTRACE(2, "SIP\tAuthentication not possible yet, no credentials available.");
        OnFailed(SIP_PDU::Failure_TemporarilyUnavailable);
        if (!transaction.IsCanceled())
          RetryLater(offlineExpire);
        return;
      }

      PTRACE (3, "SIP\tNo auth info for realm " << newAuth->GetAuthRealm() << ", using proxy auth");
      username = m_proxy.GetUserName();
      password = m_proxy.GetPassword();
    }
  }

  newAuth->SetUsername(username);
  newAuth->SetPassword(password);

  // check to see if this is a follow-on from the last authentication scheme used
  if (GetState() == Subscribing && authentication != NULL && *newAuth == *authentication) {
    delete newAuth;
    PTRACE(1, "SIP\tAuthentication already performed using current credentials, not trying again.");
    OnFailed(SIP_PDU::Failure_UnAuthorised);
    return;
  }

  PTRACE(4, "SIP\tUpdating authentication credentials.");

  // switch authentication schemes
  delete authentication;
  authentication = newAuth;
  m_username = username;
  m_password = password;

  // If we changed realm (or hadn't got one yet) update the handler database
  if (m_realm != newAuth->GetAuthRealm()) {
    m_realm = newAuth->GetAuthRealm();
    PTRACE(3, "SIP\tAuth realm set to " << m_realm);
    endpoint.UpdateHandlerIndexes(this);
  }

  // Restart the transaction with new authentication handler
  SendRequest(GetState());
}


void SIPHandler::OnReceivedOK(SIPTransaction & /*transaction*/, SIP_PDU & response)
{
  response.GetMIME().GetProductInfo(m_productInfo);

  switch (GetState()) {
    case Unsubscribing :
      SetState(Unsubscribed);
      break;

    case Subscribing :
    case Refreshing :
    case Restoring :
      if (expire == 0)
        SetState(Unsubscribed);
      else
        SetState(Subscribed);
      break;

    default :
      PTRACE(2, "SIP\tUnexpected 200 OK in handler with state " << GetState());
  }

  // reset the number of unsuccesful authentication attempts
  authenticationAttempts = 0;
}


void SIPHandler::OnTransactionFailed(SIPTransaction & transaction)
{
  if (m_transactions.Remove(&transaction)) {
    OnFailed(transaction.GetStatusCode());
    if (!transaction.IsCanceled())
      RetryLater(offlineExpire);
  }
}


void SIPHandler::RetryLater(unsigned after)
{
  if (after == 0 || expire == 0)
    return;

  PTRACE(3, "SIP\tRetrying " << GetMethod() << " after " << after << " seconds.");
  expireTimer.SetInterval(0, after); // Keep trying to get it back
}


void SIPHandler::OnFailed(const SIP_PDU & response)
{
  OnFailed(response.GetStatusCode());
}


void SIPHandler::OnFailed(SIP_PDU::StatusCodes code)
{
  switch (code) {
    case SIP_PDU::Local_TransportError :
    case SIP_PDU::Local_Timeout :
    case SIP_PDU::Failure_RequestTimeout :
    case SIP_PDU::Local_BadTransportAddress :
    case SIP_PDU::Failure_TemporarilyUnavailable:
      if (GetState() != Unsubscribing) {
        SetState(Unavailable);
        break;
      }
      // Do next case to finalise Unsubscribe even though there was an error

    default :
      PTRACE(4, "SIP\tNot retrying " << GetMethod() << " due to error response " << code);
      expire = 0; // OK, stop trying
      expireTimer.Stop(false);
      SetState(Unsubscribed);
      ShutDown();
  }
}


void SIPHandler::OnExpireTimeout(PTimer &, INT)
{
  PSafeLockReadWrite lock(*this);
  if (!lock.IsLocked())
    return;

  switch (GetState()) {
    case Subscribed :
      PTRACE(2, "SIP\tStarting " << GetMethod() << " for binding refresh");
      if (SendRequest(Refreshing))
        return;
      break;

    case Unavailable :
      PTRACE(2, "SIP\tStarting " << GetMethod() << " for offline retry");
      if (SendRequest(Restoring))
        return;
      break;

    default :
      return;
  }

  SetState(Unavailable);
}


///////////////////////////////////////////////////////////////////////////////

SIPRegisterHandler::SIPRegisterHandler(SIPEndPoint & endpoint, const SIPRegister::Params & params)
  : SIPHandler(SIP_PDU::Method_REGISTER, endpoint, params)
  , m_parameters(params)
  , m_sequenceNumber(0)
{
  // Foer some bizarre reason, even though REGISTER does not create a dialog,
  // some registrars insist that you have a from tag ...
  SIPURL local = params.m_localAddress.IsEmpty() ? params.m_addressOfRecord : params.m_localAddress;
  local.SetTag();
  m_parameters.m_localAddress = local.AsQuotedString();
  m_parameters.m_proxyAddress = m_proxy.AsString();
}


SIPTransaction * SIPRegisterHandler::CreateTransaction(OpalTransport & trans)
{
  SIPRegister::Params params = m_parameters;

  if (GetState() == Unsubscribing) {
    params.m_expire = 0;

    if (params.m_contactAddress.IsEmpty()) {
      if (m_contactAddresses.empty())
        params.m_contactAddress = "*";
      else {
        for (std::list<SIPURL>::iterator contact = m_contactAddresses.begin(); contact != m_contactAddresses.end(); ++contact) {
          contact->GetFieldParameters().Remove("expires");
          if (!params.m_contactAddress.IsEmpty())
            params.m_contactAddress += ", ";
          params.m_contactAddress += contact->AsQuotedString();
        }
      }
    }
  }
  else {
    params.m_expire = expire;

    if (params.m_contactAddress.IsEmpty()) {
      if (m_contactAddresses.empty()) {
        PString userName = SIPURL(params.m_addressOfRecord).GetUserName();
        OpalTransportAddressArray interfaces = endpoint.GetInterfaceAddresses(true, &trans);
        if (params.m_compatibility == SIPRegister::e_CannotRegisterMultipleContacts) {
          // If translated by STUN then only the external address of the interface is used.
          SIPURL contact(userName, interfaces[0]);
          contact.Sanitise(SIPURL::RegContactURI);
          m_contactAddresses.push_back(contact);
          params.m_contactAddress += contact.AsQuotedString();
        }
        else {
          OpalTransportAddress localAddress = trans.GetLocalAddress();
          unsigned qvalue = 1000;
          for (PINDEX i = 0; i < interfaces.GetSize(); ++i) {
            /* If fully compliant, put into the contact field all the bound
               interfaces. If special then we only put into the contact
               listeners that are on the same interface. If translated by STUN
               then only the external address of the interface is used. */
            if (params.m_compatibility != SIPRegister::e_CannotRegisterPrivateContacts || localAddress.IsEquivalent(interfaces[i], true)) {
              SIPURL contact(userName, interfaces[i]);
              contact.Sanitise(SIPURL::RegContactURI);
              contact.GetFieldParameters().Set("q", qvalue < 1000 ? psprintf("0.%03u", qvalue) : "1");
              m_contactAddresses.push_back(contact);

              if (!params.m_contactAddress.IsEmpty())
                params.m_contactAddress += ", ";
              params.m_contactAddress += contact.AsQuotedString();

              qvalue -= 1000/interfaces.GetSize();
            }
          }
        }
      }
      else {
        for (std::list<SIPURL>::iterator contact = m_contactAddresses.begin(); contact != m_contactAddresses.end(); ++contact) {
          if (!params.m_contactAddress.IsEmpty())
            params.m_contactAddress += ", ";
          params.m_contactAddress += contact->AsQuotedString();
        }
      }
    }
    else {
      if (!SIPMIMEInfo::ExtractURLs(params.m_contactAddress, m_contactAddresses)) {
        PTRACE(1, "SIP\tUser defined contact address has no SIP addresses in it!");
      }
    }
  }

  return new SIPRegister(endpoint, trans, GetCallID(), m_sequenceNumber, params);
}


void SIPRegisterHandler::OnReceivedOK(SIPTransaction & transaction, SIP_PDU & response)
{
  State oldState = GetState();

  SIPHandler::OnReceivedOK(transaction, response);

  if (GetState() == Unsubscribed || m_contactAddresses.empty()) {
    SendStatus(SIP_PDU::Successful_OK, oldState);
    return;
  }

  std::list<SIPURL> replyContacts;
  response.GetMIME().GetContacts(replyContacts);

  std::list<SIPURL> contacts;
  for (std::list<SIPURL>::iterator reply = replyContacts.begin(); reply != replyContacts.end(); ++reply) {
    // RFC3261 does say the registrar must send back ALL registrations, however
    // we only deal with replies from the registrar that we actually requested.
    std::list<SIPURL>::iterator request = std::find(m_contactAddresses.begin(), m_contactAddresses.end(), *reply);
    if (request != m_contactAddresses.end())
      contacts.push_back(*reply);
  }

  if (contacts.empty()) {
    if (transaction.GetMIME().GetExpires() != 0) {
      // Huh? Nothing we tried to register, registered! How is that possible?
      PTRACE(2, "SIP\tREGISTER returned no Contacts we requested, unregistering.");
      SendRequest(Unsubscribing);
      return;
    }

    // We did a partial unsubscribe but registrar doesn't understand and
    // unregisetered everthing, reregister
    std::list<SIPURL>::iterator contact = m_contactAddresses.begin();
    while (contact != m_contactAddresses.end()) {
      if (contact->GetFieldParameters().GetInteger("expires") > 0)
        ++contact;
      else
        m_contactAddresses.erase(contact++);
    }

    PTRACE(2, "SIP\tREGISTER only had some contacts to unregister but registrar to dumb, re-registering.");
    SendRequest(Subscribing);
  }

  // See if we are behind NAT and the Via header rport was present, and different
  // to our registered contact field. Some servers will refuse to work unless the
  // Contact agrees whith where the packet is physically coming from.
  OpalTransportAddress externalAddress = response.GetMIME().GetViaReceivedAddress();
  bool useExternalAddress = !externalAddress.IsEmpty();

  if (useExternalAddress) {
    for (std::list<SIPURL>::iterator contact = contacts.begin(); contact != contacts.end(); ++contact) {
      if (contact->GetHostAddress() == externalAddress) {
        useExternalAddress = false;
        break;
      }
    }
  }

  if (useExternalAddress) {
    std::list<SIPURL> newContacts;

    for (std::list<SIPURL>::iterator contact = contacts.begin(); contact != contacts.end(); ++contact) {
      if (contact->GetHostAddress().GetProto() == "udp") {
        contact->GetFieldParameters().Remove("expires");

        SIPURL newContact(contact->GetUserName(), externalAddress);
        newContact.GetFieldParameters().SetInteger("expires", originalExpire);
        newContacts.push_back(newContact);
      }
    }

    contacts.insert(contacts.end(), newContacts.begin(), newContacts.end());
    SetExpire(0);
  }
  else {
    int minExpiry = INT_MAX;
    for (std::list<SIPURL>::iterator contact = contacts.begin(); contact != contacts.end(); ++contact) {
      long expires = contact->GetFieldParameters().GetInteger("expires",
                          response.GetMIME().GetExpires(endpoint.GetRegistrarTimeToLive().GetSeconds()));
      if (minExpiry > expires)
        minExpiry = expires;
    }
    SetExpire(minExpiry);
  }

  m_contactAddresses = contacts;

  response.GetMIME().GetProductInfo(m_productInfo);

  SendStatus(SIP_PDU::Successful_OK, oldState);

  // If we have had a NAT port change to what we thought, need to initiate another REGISTER
  if (useExternalAddress) {
    PTRACE(2, "SIP\tRemote indicated change of REGISTER Contact header required due to NAT");
    SendRequest(GetState());
  }
}


void SIPRegisterHandler::OnFailed(SIP_PDU::StatusCodes r)
{
  SendStatus(r, GetState());
  SIPHandler::OnFailed(r);
}


PBoolean SIPRegisterHandler::SendRequest(SIPHandler::State s)
{
  SendStatus(SIP_PDU::Information_Trying, s);
  m_sequenceNumber = endpoint.GetNextCSeq();
  return SIPHandler::SendRequest(s);
}


void SIPRegisterHandler::SendStatus(SIP_PDU::StatusCodes code, State state)
{
  SIPEndPoint::RegistrationStatus status;
  status.m_handler = this;
  status.m_addressofRecord = GetAddressOfRecord().AsString();
  status.m_productInfo = m_productInfo;
  status.m_reason = code;
  status.m_userData = m_parameters.m_userData;

  switch (state) {
    case Subscribing :
      status.m_wasRegistering = true;
      status.m_reRegistering = false;
      break;

    case Subscribed :
    case Refreshing :
      status.m_wasRegistering = true;
      status.m_reRegistering = true;
      break;

    case Unsubscribed :
    case Unavailable :
    case Restoring :
      status.m_wasRegistering = true;
      status.m_reRegistering = code/100 != 2;
      break;

    case Unsubscribing :
      status.m_wasRegistering = false;
      status.m_reRegistering = false;
      break;

    default :
      PAssertAlways(PInvalidParameter);
  }

  endpoint.OnRegistrationStatus(status);
}


void SIPRegisterHandler::UpdateParameters(const SIPRegister::Params & params)
{
  if (!params.m_authID.IsEmpty())
    m_username = m_parameters.m_authID = params.m_authID;   // Adjust the authUser if required 
  if (!params.m_realm.IsEmpty())
    m_realm = m_parameters.m_realm = params.m_realm;   // Adjust the realm if required 
  if (!params.m_password.IsEmpty())
    m_password = m_parameters.m_password = params.m_password; // Adjust the password if required 

  if (params.m_expire > 0)
    SetExpire(m_parameters.m_expire = params.m_expire);

  m_parameters.m_contactAddress = params.m_contactAddress;
  m_contactAddresses.clear();
}


/////////////////////////////////////////////////////////////////////////

SIPSubscribeHandler::SIPSubscribeHandler(SIPEndPoint & endpoint, const SIPSubscribe::Params & params)
  : SIPHandler(SIP_PDU::Method_SUBSCRIBE, endpoint, params)
  , m_parameters(params)
  , m_unconfirmed(true)
  , m_packageHandler(SIPEventPackageFactory::CreateInstance(params.m_eventPackage))
  , m_previousResponse(NULL)
{
  callID = m_dialog.GetCallID();

  m_parameters.m_proxyAddress = m_proxy.AsString();

  if (m_parameters.m_contentType.IsEmpty() && (m_packageHandler != NULL))
    m_parameters.m_contentType = m_packageHandler->GetContentType();
}


SIPSubscribeHandler::~SIPSubscribeHandler()
{
  delete m_packageHandler;
  delete m_previousResponse;
}


SIPTransaction * SIPSubscribeHandler::CreateTransaction(OpalTransport & trans)
{ 
  // Do all the following here as must be after we have called GetTransport()
  // which sets various fields correctly for transmission
  if (!m_dialog.IsEstablished()) {
    m_dialog.SetRequestURI(GetAddressOfRecord());
    if (m_parameters.m_eventPackage.IsWatcher())
      m_parameters.m_localAddress = GetAddressOfRecord().AsString();

    m_dialog.SetRemoteURI(m_parameters.m_addressOfRecord);

    if (m_parameters.m_localAddress.IsEmpty())
      m_dialog.SetLocalURI(endpoint.GetRegisteredPartyName(m_parameters.m_addressOfRecord, *m_transport));
    else
      m_dialog.SetLocalURI(m_parameters.m_localAddress);

    m_dialog.SetProxy(m_proxy, true);
  }

  m_parameters.m_expire = GetState() != Unsubscribing ? expire : 0;
  return new SIPSubscribe(endpoint, trans, m_dialog, m_parameters);
}


void SIPSubscribeHandler::OnFailed(const SIP_PDU & response)
{
  SIP_PDU::StatusCodes responseCode = response.GetStatusCode();

  SendStatus(responseCode, GetState());

  if (GetState() != Unsubscribing && responseCode == SIP_PDU::Failure_TransactionDoesNotExist) {
    // Resubscribe as previous subscription totally lost, but dialog processing
    // may have altered the target so restore the original target address
    m_parameters.m_addressOfRecord = GetAddressOfRecord().AsString();
    PString dummy;
    endpoint.Subscribe(m_parameters, dummy);
  }

  SIPHandler::OnFailed(responseCode);
}


PBoolean SIPSubscribeHandler::SendRequest(SIPHandler::State s)
{
  SendStatus(SIP_PDU::Information_Trying, s);
  return SIPHandler::SendRequest(s);
}


bool SIPSubscribeHandler::WriteSIPHandler(OpalTransport & transport, bool forked)
{
  m_dialog.SetForking(forked);
  bool ok = SIPHandler::WriteSIPHandler(transport, forked);
  m_dialog.SetForking(false);
  return ok;
}


void SIPSubscribeHandler::SendStatus(SIP_PDU::StatusCodes code, State state)
{
  SIPEndPoint::SubscriptionStatus status;
  status.m_handler = this;
  status.m_addressofRecord = GetAddressOfRecord().AsString();
  status.m_productInfo = m_productInfo;
  status.m_reason = code;
  status.m_userData = m_parameters.m_userData;

  switch (state) {
    case Subscribing :
      status.m_wasSubscribing = true;
      status.m_reSubscribing = false;
      break;

    case Subscribed :
      if (m_unconfirmed) {
        status.m_wasSubscribing = true;
        status.m_reSubscribing = false;
        endpoint.OnSubscriptionStatus(status);
      }
      // Do next state

    case Refreshing :
      status.m_wasSubscribing = true;
      status.m_reSubscribing = true;
      break;

    case Unsubscribed :
    case Unavailable :
    case Restoring :
      status.m_wasSubscribing = true;
      status.m_reSubscribing = code/100 != 2;
      break;

    case Unsubscribing :
      status.m_wasSubscribing = false;
      status.m_reSubscribing = false;
      break;

    default :
      PAssertAlways(PInvalidParameter);
  }

  if (!m_parameters.m_onSubcribeStatus.IsNULL()) 
    m_parameters.m_onSubcribeStatus(*this, status);

  endpoint.OnSubscriptionStatus(status);
}


void SIPSubscribeHandler::UpdateParameters(const SIPSubscribe::Params & params)
{
  if (!params.m_authID.IsEmpty())
    m_username = params.m_authID;   // Adjust the authUser if required 
  if (!params.m_realm.IsEmpty())
    m_realm = params.m_realm;   // Adjust the realm if required 
  if (!params.m_password.IsEmpty())
    m_password = params.m_password; // Adjust the password if required 

  m_parameters.m_contactAddress = params.m_contactAddress;

  if (params.m_expire > 0)
    SetExpire(params.m_expire);
}


void SIPSubscribeHandler::OnReceivedOK(SIPTransaction & transaction, SIP_PDU & response)
{
  /* An "expire" parameter in the Contact header has no semantics
   * for SUBSCRIBE. RFC3265, 3.1.1.
   * An answer can only shorten the expires time.
   */
  SetExpire(response.GetMIME().GetExpires(originalExpire));
  m_dialog.Update(*m_transport, response);

  if (GetState() != Unsubscribing)
    SIPHandler::OnReceivedOK(transaction, response);
}


PBoolean SIPSubscribeHandler::OnReceivedNOTIFY(SIP_PDU & request)
{
  if (PAssertNULL(m_transport) == NULL)
    return false;

  if (m_unconfirmed) {
    SendStatus(SIP_PDU::Successful_OK, GetState());
    m_unconfirmed = false;
  }

  SIPMIMEInfo & requestMIME = request.GetMIME();

  // If we received this NOTIFY before, send the previous response
  if (m_dialog.IsDuplicateCSeq(requestMIME.GetCSeqIndex())) {

    // duplicate CSEQ but no previous response? That's unpossible!
    if (m_previousResponse == NULL)
      return request.SendResponse(*m_transport, SIP_PDU::Failure_InternalServerError, &endpoint);

    PTRACE(3, "SIP\tReceived duplicate NOTIFY");
    return request.SendResponse(*m_transport, *m_previousResponse, &endpoint);
  }

  // remove last response
  delete m_previousResponse;
  m_previousResponse = new SIP_PDU(request, SIP_PDU::Failure_BadRequest);

  PStringToString subscriptionStateInfo;
  PCaselessString subscriptionState = requestMIME.GetSubscriptionState(subscriptionStateInfo);
  if (subscriptionState.IsEmpty()) {
    PTRACE(2, "SIP\tNOTIFY received without Subscription-State");
    m_previousResponse->SetInfo("No Subscription-State field");
    return request.SendResponse(*m_transport, *m_previousResponse, &endpoint);
  }

  // Check the susbscription state
  if (subscriptionState == "terminated") {
    PTRACE(3, "SIP\tSubscription is terminated, state=" << GetState());
    m_previousResponse->SetStatusCode(SIP_PDU::Successful_OK);
    request.SendResponse(*m_transport, *m_previousResponse, &endpoint);
    if (GetState() != Unsubscribing)
      ShutDown();
    else {
      SetState(Unsubscribed);
      SendStatus(SIP_PDU::Successful_OK, Unsubscribing);
    }
    return true;
  }

  PString requestEvent = requestMIME.GetEvent();
  if (m_parameters.m_eventPackage != requestEvent) {
    PTRACE(2, "SIP\tNOTIFY received for incorrect event \"" << requestEvent << "\", requires \"" << m_parameters.m_eventPackage << '"');
    m_previousResponse->SetStatusCode(SIP_PDU::Failure_BadEvent);
    m_previousResponse->GetMIME().SetAt("Allow-Events", m_parameters.m_eventPackage);
    return request.SendResponse(*m_transport, *m_previousResponse, &endpoint);
  }

  // Check any Requires field
  PStringSet require = requestMIME.GetRequire();
  if (m_parameters.m_eventList)
    require -= "eventlist";
  if (!require.IsEmpty()) {
    PTRACE(2, "SIPPres\tNOTIFY contains unsupported Require field \"" << setfill(',') << require << '"');
    m_previousResponse->SetStatusCode(SIP_PDU::Failure_BadExtension);
    m_previousResponse->GetMIME().SetUnsupported(require);
    m_previousResponse->SetInfo("Unsupported Require");
    return request.SendResponse(*m_transport, *m_previousResponse, &endpoint);
  }

  // check the ContentType
  if (!m_parameters.m_contentType.IsEmpty()) {
    PCaselessString requestContentType = requestMIME.GetContentType();
    if (
        (m_parameters.m_contentType.Find(requestContentType) == P_MAX_INDEX) && 
        !(m_parameters.m_eventList && (requestContentType == "multipart/related")) &&
        !((m_packageHandler != NULL) && m_packageHandler->ValidateContentType(requestContentType, requestMIME))
        ) {
      PTRACE(2, "SIPPres\tNOTIFY contains unsupported Content-Type \""
             << requestContentType << "\", expecting \"" << m_parameters.m_contentType << "\" or multipart/related or other validated type");
      m_previousResponse->SetStatusCode(SIP_PDU::Failure_UnsupportedMediaType);
      m_previousResponse->GetMIME().SetAt("Accept", m_parameters.m_contentType);
      m_previousResponse->SetInfo("Unsupported Content-Type");
      return request.SendResponse(*m_transport, *m_previousResponse, &endpoint);
    }
  }

  // Check if we know how to deal with this event
  if (m_packageHandler == NULL && m_parameters.m_onNotify.IsNULL()) {
    PTRACE(2, "SIP\tNo handler for NOTIFY received for event \"" << requestEvent << '"');
    m_previousResponse->SetStatusCode(SIP_PDU::Failure_InternalServerError);
    return request.SendResponse(*m_transport, *m_previousResponse, &endpoint);
  }

  // Adjust timeouts if state is a go
  if (subscriptionState == "active" || subscriptionState == "pending") {
    PTRACE(3, "SIP\tSubscription is " << GetState());
    PString expire = SIPMIMEInfo::ExtractFieldParameter(GetState(), "expire");
    if (!expire.IsEmpty())
      SetExpire(expire.AsUnsigned());
  }

  bool sendResponse = true;

  PMultiPartList parts;
  if (!m_parameters.m_eventList || !requestMIME.DecodeMultiPartList(parts, request.GetEntityBody()))
    sendResponse = DispatchNOTIFY(request, *m_previousResponse);
  else {
    // If GetMultiParts() returns true there as at least one part and that
    // part must be the meta list, guranteed by DecodeMultiPartList()
    PMultiPartList::iterator iter = parts.begin();

    // First part is always Meta Information
    if (iter->m_mime.GetString(PMIMEInfo::ContentTypeTag) != "application/rlmi+xml") {
      PTRACE(2, "SIP\tNOTIFY received without RLMI as first multipart body");
      m_previousResponse->SetInfo("No Resource List Meta-Information");
      return request.SendResponse(*m_transport, *m_previousResponse, &endpoint);
    }

#if P_EXPAT
    PXML xml;
    if (!xml.Load(iter->m_textBody)) {
      PTRACE(2, "SIP\tNOTIFY received with illegal RLMI\n"
                "Line " << xml.GetErrorLine() << ", Column " << xml.GetErrorColumn() << ": " << xml.GetErrorString());
      m_previousResponse->SetInfo("Bad Resource List Meta-Information");
      return request.SendResponse(*m_transport, *m_previousResponse, &endpoint);
    }

    if (parts.GetSize() == 1)
      m_previousResponse->SetStatusCode(SIP_PDU::Successful_OK);
    else {
      while (++iter != parts.end()) {
        SIP_PDU pdu(request.GetMethod());
        SIPMIMEInfo & pduMIME = pdu.GetMIME();

        pduMIME.AddMIME(iter->m_mime);
        pdu.SetEntityBody(iter->m_textBody);

        PStringToString cid;
        if (iter->m_mime.GetComplex(PMIMEInfo::ContentIdTag, cid)) {
          PINDEX index = 0;
          PXMLElement * resource;
          while ((resource = xml.GetElement("resource", index++)) != NULL) {
            SIPURL uri = resource->GetAttribute("uri");
            if (!uri.IsEmpty()) {
              PXMLElement * instance = resource->GetElement("instance");
              if (instance != NULL && instance->GetAttribute("cid") == cid[PString::Empty()]) {
                pduMIME.SetSubscriptionState(instance->GetAttribute("state"));
                PXMLElement * name = resource->GetElement("name");
                if (name != NULL)
                  uri.SetDisplayName(name->GetData());
                pduMIME.SetFrom(uri.AsQuotedString());
                pduMIME.SetTo(uri.AsQuotedString());
                break;
              }
            }
          }
        }

        if (DispatchNOTIFY(pdu, *m_previousResponse))
          sendResponse = false;
      }
    }
#else
    for ( ; iter != parts.end(); ++iter) {
      SIP_PDU pdu(request.GetMethod());
      pdu.GetMIME().AddMIME(iter->m_mime);
      pdu.SetEntityBody(iter->m_textBody);

      if (DispatchNOTIFY(pdu, *m_previousResponse))
        sendResponse = false;
    }
#endif
  }

  if (sendResponse)
    request.SendResponse(*m_transport, *m_previousResponse, &endpoint);
  return true;
}


bool SIPSubscribeHandler::DispatchNOTIFY(SIP_PDU & request, SIP_PDU & response)
{
  if (!m_parameters.m_onNotify.IsNULL()) {
    PTRACE(4, "SIP\tCalling NOTIFY callback for AOR \"" << m_addressOfRecord << "\"");
    SIPSubscribe::NotifyCallbackInfo status(endpoint, *m_transport, request, response);
    m_parameters.m_onNotify(*this, status);
    return status.m_sendResponse;
  }

  if (m_packageHandler != NULL) {
    PTRACE(4, "SIP\tCalling package NOTIFY handler for AOR \"" << m_addressOfRecord << "\"");
    if (m_packageHandler->OnReceivedNOTIFY(*this, request))
      response.SetStatusCode(SIP_PDU::Successful_OK);
    return true;
  }

  PTRACE(2, "SIP\tNo NOTIFY handler for AOR \"" << m_addressOfRecord << "\"");
  return true;
}

bool SIPEventPackageHandler::ValidateContentType(const PString & type, const SIPMIMEInfo & mime) 
{ 
  return type.IsEmpty() && (mime.GetContentLength() == 0);
}

class SIPMwiEventPackageHandler : public SIPEventPackageHandler
{
  virtual PCaselessString GetContentType() const
  {
    return "application/simple-message-summary";
  }

  virtual bool OnReceivedNOTIFY(SIPHandler & handler, SIP_PDU & request)
  {
    PString body = request.GetEntityBody();
    if (body.IsEmpty ())
      return true;

    // Extract the string describing the number of new messages
    static struct {
      const char * name;
      OpalManager::MessageWaitingType type;
    } const validMessageClasses[] = {
      { "voice-message",      OpalManager::VoiceMessageWaiting      },
      { "fax-message",        OpalManager::FaxMessageWaiting        },
      { "pager-message",      OpalManager::PagerMessageWaiting      },
      { "multimedia-message", OpalManager::MultimediaMessageWaiting },
      { "text-message",       OpalManager::TextMessageWaiting       },
      { "none",               OpalManager::NoMessageWaiting         }
    };
    PString msgs;
    PStringArray bodylines = body.Lines ();
    for (PINDEX z = 0 ; z < PARRAYSIZE(validMessageClasses); z++) {

      for (int i = 0 ; i < bodylines.GetSize () ; i++) {

        PCaselessString line (bodylines [i]);
        PINDEX j = line.FindLast(validMessageClasses[z].name);
        if (j != P_MAX_INDEX) {
          line.Replace(validMessageClasses[z].name, "");
          line.Replace (":", "");
          msgs = line.Trim ();
          handler.GetEndPoint().OnMWIReceived(handler.GetAddressOfRecord().AsString(), validMessageClasses[z].type, msgs);
          return true;
        }
      }
    }

    // Received MWI, unknown messages number
    handler.GetEndPoint().OnMWIReceived(handler.GetAddressOfRecord().AsString(), OpalManager::NumMessageWaitingTypes, "1/0");

    return true;
  }
};

static SIPEventPackageFactory::Worker<SIPMwiEventPackageHandler> mwiEventPackageHandler(SIPSubscribe::MessageSummary);


///////////////////////////////////////////////////////////////////////////////

#if P_EXPAT

class SIPPresenceEventPackageHandler : public SIPEventPackageHandler
{
  virtual PCaselessString GetContentType() const
  {
    return "application/pidf+xml";
  }

  virtual bool OnReceivedNOTIFY(SIPHandler & handler, SIP_PDU & request)
  {
    PTRACE(4, "SIP\tProcessing presence NOTIFY using old API");

    // support old API 
    SIPURL from = request.GetMIME().GetFrom();
    from.Sanitise(SIPURL::ExternalURI);

    SIPURL to = request.GetMIME().GetTo();
    to.Sanitise(SIPURL::ExternalURI);

    SIPPresenceInfo info;
    info.m_entity = from.AsString();
    info.m_target = to.AsString();

    // Check for empty body, if so then is OK, just a ping ...
    if (request.GetEntityBody().IsEmpty()) {
      handler.GetEndPoint().OnPresenceInfoReceived(info);
      return true;
    }

    PXML xml;
    if (!xml.Load(request.GetEntityBody()))
      return false;

    PXMLElement * rootElement = xml.GetRootElement();
    if (rootElement == NULL || rootElement->GetName() != "presence")
      return false;

    PXMLElement * tupleElement = rootElement->GetElement("tuple");
    if (tupleElement == NULL)
      return false;

    PXMLElement * statusElement = tupleElement->GetElement("status");
    if (statusElement == NULL)
      return false;

    PXMLElement * basicElement = statusElement->GetElement("basic");
    if (basicElement != NULL) {
      PCaselessString value = basicElement->GetData();
      if (value == "open")
        info.m_state = SIPPresenceInfo::Available;
      else if (value == "closed")
        info.m_state = SIPPresenceInfo::NoPresence;
      else
        info.m_state = SIPPresenceInfo::Unchanged;
    }

    PXMLElement * noteElement = statusElement->GetElement("note");
    if (!noteElement)
      noteElement = rootElement->GetElement("note");
    if (!noteElement)
      noteElement = tupleElement->GetElement("note");
    if (noteElement)
      info.m_note = noteElement->GetData();

    PXMLElement * contactElement = tupleElement->GetElement("contact");
    if (contactElement != NULL)
      info.m_contact = contactElement->GetData();

    handler.GetEndPoint().OnPresenceInfoReceived(info);
    return true;
  }
};

static SIPEventPackageFactory::Worker<SIPPresenceEventPackageHandler> presenceEventPackageHandler(SIPSubscribe::Presence);


static void ParseParticipant(PXMLElement * participantElement, SIPDialogNotification::Participant & participant)
{
  if (participantElement == NULL)
    return;

  PXMLElement * identityElement = participantElement->GetElement("identity");
  if (identityElement != NULL) {
    participant.m_identity = identityElement->GetData();
    participant.m_display = identityElement->GetAttribute("display");
  }

  PXMLElement * targetElement = participantElement->GetElement("target");
  if (targetElement == NULL)
    return;

  participant.m_URI = targetElement->GetAttribute("uri");

  PXMLElement * paramElement;
  PINDEX i = 0;
  while ((paramElement = targetElement->GetElement("param", i++)) != NULL) {
    PCaselessString name = paramElement->GetAttribute("pname");
    PCaselessString value = paramElement->GetAttribute("pvalue");
    if (name == "appearance" || // draft-anil-sipping-bla-04 version
        name == "x-line-id")    // draft-anil-sipping-bla-03 version
      participant.m_appearance = value.AsUnsigned();
    else if (name == "sip.byeless" || name == "+sip.byeless")
      participant.m_byeless = value == "true";
    else if (name == "sip.rendering" || name == "+sip.rendering") {
      if (value == "yes")
        participant.m_rendering = SIPDialogNotification::RenderingMedia;
      else if (value == "no")
        participant.m_rendering = SIPDialogNotification::NotRenderingMedia;
      else
        participant.m_rendering = SIPDialogNotification::RenderingUnknown;
    }
  }
}


class SIPDialogEventPackageHandler : public SIPEventPackageHandler
{
public:
  SIPDialogEventPackageHandler()
    : m_dialogNotifyVersion(1)
  {
  }

  virtual PCaselessString GetContentType() const
  {
    return "application/dialog-info+xml";
  }

  virtual bool OnReceivedNOTIFY(SIPHandler & handler, SIP_PDU & request)
  {
    // Check for empty body, if so then is OK, just a ping ...
    if (request.GetEntityBody().IsEmpty())
      return true;

    PXML xml;
    if (!xml.Load(request.GetEntityBody()))
      return false;

    PXMLElement * rootElement = xml.GetRootElement();
    if (rootElement == NULL || rootElement->GetName() != "dialog-info")
      return false;

    SIPDialogNotification info(rootElement->GetAttribute("entity"));
    if (info.m_entity.IsEmpty())
      return false;

    PINDEX index = 0;
    PXMLElement * dialogElement;
    while ((dialogElement = rootElement->GetElement("dialog", index)) != NULL) {
      info.m_callId = dialogElement->GetAttribute("call-id");
      info.m_local.m_dialogTag = dialogElement->GetAttribute("local-tag");
      info.m_remote.m_dialogTag = dialogElement->GetAttribute("remote-tag");

      PXMLElement * stateElement = dialogElement->GetElement("state");
      if (stateElement == NULL)
        info.m_state = SIPDialogNotification::Terminated;
      else {
        PCaselessString str = stateElement->GetData();
        for (info.m_state = SIPDialogNotification::LastState; info.m_state > SIPDialogNotification::FirstState; --info.m_state) {
          if (str == info.GetStateName())
            break;
        }

        str = stateElement->GetAttribute("event");
        for (info.m_eventType = SIPDialogNotification::LastEvent; info.m_eventType >= SIPDialogNotification::FirstEvent; --info.m_eventType) {
          if (str == info.GetEventName())
            break;
        }

        info.m_eventCode = stateElement->GetAttribute("code").AsUnsigned();
      }

      ParseParticipant(dialogElement->GetElement("local"), info.m_local);
      ParseParticipant(dialogElement->GetElement("remote"), info.m_remote);
      handler.GetEndPoint().OnDialogInfoReceived(info);
      index++;
    }

    if (index == 0)
      handler.GetEndPoint().OnDialogInfoReceived(info);
    return true;
  }

  virtual PString OnSendNOTIFY(SIPHandler & handler, const PObject * data)
  {
    PStringStream body;
    body << "<?xml version=\"1.0\"?>\r\n"
            "<dialog-info xmlns=\"urn:ietf:params:xml:ns:dialog-info\" version=\""
         << m_dialogNotifyVersion++ << "\" state=\"partial\" entity=\""
         << handler.GetAddressOfRecord() << "\">\r\n";

    std::map<PString, SIPDialogNotification>::iterator iter;

    const SIPDialogNotification * info = dynamic_cast<const SIPDialogNotification *>(data);
    if (info != NULL) {
      if (info->m_state != SIPDialogNotification::Terminated)
        m_activeDialogs[info->m_callId] = *info;
      else {
        iter = m_activeDialogs.find(info->m_callId);
        if (iter != m_activeDialogs.end())
          m_activeDialogs.erase(iter);

        body << *info;
      }
    }

    for (iter = m_activeDialogs.begin(); iter != m_activeDialogs.end(); ++iter)
      body << iter->second;

    body << "</dialog-info>\r\n";
    return body;
  }

  unsigned m_dialogNotifyVersion;
  std::map<PString, SIPDialogNotification> m_activeDialogs;
};

static SIPEventPackageFactory::Worker<SIPDialogEventPackageHandler> dialogEventPackageHandler(SIPSubscribe::Dialog);

#endif // P_EXPAT


///////////////////////////////////////////////////////////////////////////////

SIPDialogNotification::SIPDialogNotification(const PString & entity)
  : m_entity(entity)
  , m_initiator(false)
  , m_state(Terminated)
  , m_eventType(NoEvent)
  , m_eventCode(0)
{
}


PString SIPDialogNotification::GetStateName(States state)
{
  static const char * const Names[] = {
    "terminated",
    "trying",
    "proceeding",
    "early",
    "confirmed"
  };
  if (state < PARRAYSIZE(Names) && Names[state] != NULL)
    return Names[state];

  return psprintf("<%u>", state);
}


PString SIPDialogNotification::GetEventName(Events state)
{
  static const char * const Names[] = {
    "cancelled",
    "rejected",
    "replaced",
    "local-bye",
    "remote-bye",
    "error",
    "timeout"
  };
  if (state < PARRAYSIZE(Names) && Names[state] != NULL)
    return Names[state];

  return psprintf("<%u>", state);
}


static void OutputParticipant(ostream & strm, const char * name, const SIPDialogNotification::Participant & participant)
{
  if (participant.m_URI.IsEmpty())
    return;

  strm << "    <" << name << ">\r\n";

  if (!participant.m_identity.IsEmpty()) {
    strm << "      <identity";
    if (!participant.m_display.IsEmpty())
      strm << " display=\"" << participant.m_display << '"';
    strm << '>' << participant.m_identity << "</identity>\r\n";
  }

  strm << "      <target uri=\"" << participant.m_URI << "\">\r\n";

  if (participant.m_appearance >= 0)
    strm << "        <param pname=\"appearance\" pval=\"" << participant.m_appearance << "\"/>\r\n"
            "        <param pname=\"x-line-id\" pval=\"" << participant.m_appearance << "\"/>\r\n";

  if (participant.m_byeless)
    strm << "        <param pname=\"sip.byeless\" pval=\"true\"/>\r\n";

  if (participant.m_rendering >= 0)
    strm << "        <param pname=\"sip.rendering\" pval=\"" << (participant.m_rendering > 0 ? "yes" : "no") << "\"/>\r\n";

  strm << "      </target>\r\n"
       << "    </" << name << ">\r\n";
}


void SIPDialogNotification::PrintOn(ostream & strm) const
{
  if (m_dialogId.IsEmpty())
    return;

  // Start dialog XML tag
  strm << "  <dialog id=\"" << m_dialogId << '"';
  if (!m_callId)
    strm << " call-id=\"" << m_callId << '"';
  if (!m_local.m_dialogTag)
    strm << " local-tag=\"" << m_local.m_dialogTag << '"';
  if (!m_remote.m_dialogTag)
    strm << " remote-tag=\"" << m_remote.m_dialogTag << '"';
  strm << " direction=\"" << (m_initiator ? "initiator" : "receiver") << "\">\r\n";

  // State XML tag & value
  strm << "    <state";
  if (m_eventType > SIPDialogNotification::NoEvent) {
    strm << " event=\"" << GetEventName() << '"';
    if (m_eventCode > 0)
      strm << " code=\"" << m_eventCode << '"';
  }
  strm << '>' << GetStateName() << "</state>\r\n";

  // Participant XML tags (local/remopte)
  OutputParticipant(strm, "local", m_local);
  OutputParticipant(strm, "remote", m_remote);

  // Close out dialog tag
  strm << "  </dialog>\r\n";
}

/////////////////////////////////////////////////////////////////////////

SIPNotifyHandler::SIPNotifyHandler(SIPEndPoint & endpoint,
                                   const PString & targetAddress,
                                   const SIPEventPackage & eventPackage,
                                   const SIPDialogContext & dialog)
  : SIPHandler(SIP_PDU::Method_NOTIFY, endpoint, SIPParameters(targetAddress, dialog.GetRemoteURI().AsString()))
  , m_eventPackage(eventPackage)
  , m_dialog(dialog)
  , m_reason(Deactivated)
  , m_packageHandler(SIPEventPackageFactory::CreateInstance(eventPackage))
{
  callID = m_dialog.GetCallID();
}


SIPNotifyHandler::~SIPNotifyHandler()
{
  delete m_packageHandler;
}


SIPTransaction * SIPNotifyHandler::CreateTransaction(OpalTransport & trans)
{
  PString state;
  if (expire > 0 && GetState() != Unsubscribing)
    state.sprintf("active;expires=%u", expire);
  else {
    state = "terminated;reason=";
    static const char * const ReasonNames[] = {
      "deactivated",
      "probation",
      "rejected",
      "timeout",
      "giveup",
      "noresource"
    };
    state += ReasonNames[m_reason];
  }

  return new SIPNotify(endpoint, trans, m_dialog, m_eventPackage, state, m_body);
}


PBoolean SIPNotifyHandler::SendRequest(SIPHandler::State state)
{
  // If times out, i.e. Refreshing, then this is actually a time out unsubscribe.
  if (state == Refreshing)
    m_reason = Timeout;

  return SIPHandler::SendRequest(state == Refreshing ? Unsubscribing : state);
}


bool SIPNotifyHandler::WriteSIPHandler(OpalTransport & transport, bool forked)
{
  m_dialog.SetForking(forked);
  bool ok = SIPHandler::WriteSIPHandler(transport, forked);
  m_dialog.SetForking(false);
  return ok;
}


bool SIPNotifyHandler::SendNotify(const PObject * body)
{
  if (!LockReadWrite())
    return false;

  if (m_packageHandler != NULL)
    m_body = m_packageHandler->OnSendNOTIFY(*this, body);
  else if (body == NULL)
    m_body.MakeEmpty();
  else {
    PStringStream str;
    str << *body;
    m_body = str;
  }

  UnlockReadWrite();

  return ActivateState(Subscribing);
}


/////////////////////////////////////////////////////////////////////////

SIPPublishHandler::SIPPublishHandler(SIPEndPoint & endpoint,
                                     const SIPSubscribe::Params & params,
                                     const PString & body)
  : SIPHandler(SIP_PDU::Method_PUBLISH, endpoint, params)
  , m_parameters(params)
  , m_body(body)
{
  m_parameters.m_proxyAddress = m_proxy.AsString();
}


SIPTransaction * SIPPublishHandler::CreateTransaction(OpalTransport & transport)
{
  if (GetState() == Unsubscribing)
    return NULL;

  m_parameters.m_expire = expire;
  return new SIPPublish(endpoint,
                        transport,
                        GetCallID(),
                        m_sipETag,
                        m_parameters,
                        (GetState() == Refreshing) ? PString::Empty() : m_body);
}


void SIPPublishHandler::OnReceivedOK(SIPTransaction & transaction, SIP_PDU & response)
{
  PString newETag = response.GetMIME().GetSIPETag();

  if (!newETag.IsEmpty())
    m_sipETag = newETag;

  SetExpire(response.GetMIME().GetExpires(originalExpire));

  SIPHandler::OnReceivedOK(transaction, response);
}


///////////////////////////////////////////////////////////////////////

static PAtomicInteger DefaultTupleIdentifier;

SIPPresenceInfo::SIPPresenceInfo(const PString & personId, State state)
  : OpalPresenceInfo(state)
  , m_tupleId(PString::Printf, "T%08X", ++DefaultTupleIdentifier)
  , m_personId(personId)
{
}


void SIPPresenceInfo::PrintOn(ostream & strm) const
{
  if (m_entity.IsEmpty())
    return;

  if (m_activities.GetSize() > 0)
    strm << setfill(',') << m_activities << setfill(' ');
  else {
    switch (m_state) {
      case Unchanged :
        strm << "Unchanged";
        break;

      case NoPresence :
        strm << "Closed";
        break;

      default:
        if (m_note.IsEmpty())
          strm << "Open";
        else
          strm << m_note;
    }
  }
}

// defined in RFC 4480
static const char * const ExtendedSIPActivities[] = {
      "appointment",
      "away",
      "breakfast",
      "busy",
      "dinner",
      "holiday",
      "in-transit",
      "looking-for-work",
      "lunch",
      "meal",
      "meeting",
      "on-the-phone",
      "other",
      "performance",
      "permanent-absence",
      "playing",
      "presentation",
      "shopping",
      "sleeping",
      "spectator",
      "steering",
      "travel",
      "tv",
      "vacation",
      "working",
      "worship"
};

bool SIPPresenceInfo::AsSIPActivityString(State state, PString & str)
{
  if ((state >= Appointment) && (state <= Worship)) {
    str = PString(ExtendedSIPActivities[state - Appointment]);
    return true;
  }

  return false;
}

bool SIPPresenceInfo::AsSIPActivityString(PString & str) const
{
  return AsSIPActivityString(m_state, str);
}

OpalPresenceInfo::State SIPPresenceInfo::FromSIPActivityString(const PString & str)
{
  for (size_t i = 0; i < sizeof(ExtendedSIPActivities)/sizeof(ExtendedSIPActivities[0]);++i) {
    if (str == ExtendedSIPActivities[i])
      return (State)(Appointment + i);
  }

  return NoPresence;
}

PString SIPPresenceInfo::AsXML() const
{
  if (m_entity.IsEmpty() || m_tupleId.IsEmpty()) {
    PTRACE(1, "SIP\tCannot encode Presence XML as no address or no id.");
    return PString::Empty();
  }

  PStringStream xml;

  xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
         "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" "
                  " xmlns:dm=\"urn:ietf:params:xml:ns:pidf:data-model\""
                  " xmlns:rpid=\"urn:ietf:params:xml:ns:pidf:rpid\""
                  " entity=\"" << m_entity << "\">\r\n"
         "  <tuple id=\"" << m_tupleId << "\">\r\n"
         "    <status>\r\n";
  if (m_state != Unchanged)
    xml << "      <basic>" << (m_state != NoPresence ? "open" : "closed") << "</basic>\r\n";
  xml << "    </status>\r\n"
         "    <contact priority=\"1\">";
  if (m_contact.IsEmpty())
    xml << m_entity;
  else
    xml << m_contact;
  xml << "</contact>\r\n";

  if (!m_note.IsEmpty()) {
    //xml << "    <note xml:lang=\"en\">" << PXML::EscapeSpecialChars(m_note) << "</note>\r\n";
    xml << "    <note>" << PXML::EscapeSpecialChars(m_note) << "</note>\r\n";
  }

  xml << "    <timestamp>" << PTime().AsString(PTime::RFC3339) << "</timestamp>\r\n"
         "  </tuple>\r\n";
  if (!m_personId.IsEmpty() && (((m_state >= Appointment) && (m_state <= Worship)) || (m_activities.GetSize() > 0))) {
    xml << "  <dm:person id=\"p" << m_personId << "\">\r\n"
           "    <rpid:activities>\r\n";
    bool doneState = false;
    for (PINDEX i = 0; i < m_activities.GetSize(); ++i) {
      State s = FromString(m_activities[i]);
      if (s >= Appointment) {
        if (s == m_state)
          doneState = true;
        xml << "      <rpid:" << ExtendedSIPActivities[s - Appointment] <<"/>\r\n";
      }
    }
    if (!doneState)
      xml << "      <rpid:" << ExtendedSIPActivities[m_state - Appointment] <<"/>\r\n";

    xml << "    </rpid:activities>\r\n"
           "  </dm:person>\r\n";
  }

  xml << "</presence>\r\n";

  return xml;
}


/////////////////////////////////////////////////////////////////////////

SIPMessageHandler::SIPMessageHandler(SIPEndPoint & endpoint, const SIPMessage::Params & params)
  : SIPHandler(SIP_PDU::Method_MESSAGE, endpoint, params)
  , m_parameters(params)
{
  m_parameters.m_proxyAddress = m_proxy.AsString();
  callID = params.m_id;   // make sure the transation uses the conversation ID, so we can track it
  SetState(Subscribed);
}


SIPTransaction * SIPMessageHandler::CreateTransaction(OpalTransport & transport)
{ 
  if (GetState() == Unsubscribing)
    return NULL;

  SetExpire(originalExpire);

  if (m_parameters.m_id.IsEmpty())
    m_parameters.m_id = GetCallID();

  SIPMessage * msg = new SIPMessage(endpoint, transport, m_parameters);
  if (msg != NULL)
    m_parameters.m_localAddress = msg->GetLocalAddress().AsString();

  return msg;
}


void SIPMessageHandler::OnFailed(SIP_PDU::StatusCodes reason)
{
  SIPHandler::OnFailed(reason);
  endpoint.OnMESSAGECompleted(m_parameters, reason);
}


void SIPMessageHandler::OnExpireTimeout(PTimer &, INT)
{
  PSafeLockReadWrite lock(*this);
  if (lock.IsLocked())
    SetState(Unavailable);
  endpoint.OnMESSAGECompleted(m_parameters, SIP_PDU::Failure_RequestTimeout);
}


void SIPMessageHandler::OnReceivedOK(SIPTransaction & transaction, SIP_PDU & response)
{
  SIPHandler::OnReceivedOK(transaction, response);
  endpoint.OnMESSAGECompleted(m_parameters, SIP_PDU::Successful_OK);
}

/////////////////////////////////////////////////////////////////////////

SIPOptionsHandler::SIPOptionsHandler(SIPEndPoint & endpoint, const SIPOptions::Params & params)
  : SIPHandler(SIP_PDU::Method_OPTIONS, endpoint, params)
  , m_parameters(params)
{
  m_parameters.m_proxyAddress = m_proxy.AsString();
  SetState(Subscribed);
}


SIPTransaction * SIPOptionsHandler::CreateTransaction(OpalTransport & transport)
{ 
  if (GetState() == Unsubscribing)
    return NULL;

  return new SIPOptions(endpoint, transport, callID, m_parameters);
}


void SIPOptionsHandler::OnFailed(SIP_PDU::StatusCodes reason)
{
  SIP_PDU dummy;
  dummy.SetStatusCode(reason);
  endpoint.OnOptionsCompleted(m_parameters, dummy);
  SIPHandler::OnFailed(reason);
}


void SIPOptionsHandler::OnFailed(const SIP_PDU & response)
{
  endpoint.OnOptionsCompleted(m_parameters, response);
  SIPHandler::OnFailed(response.GetStatusCode());
}


void SIPOptionsHandler::OnExpireTimeout(PTimer &, INT)
{
  PSafeLockReadWrite lock(*this);
  if (lock.IsLocked())
    SetState(Unavailable);
}


void SIPOptionsHandler::OnReceivedOK(SIPTransaction & transaction, SIP_PDU & response)
{
  endpoint.OnOptionsCompleted(m_parameters, response);
  SIPHandler::OnReceivedOK(transaction, response);
}


/////////////////////////////////////////////////////////////////////////

SIPPingHandler::SIPPingHandler(SIPEndPoint & endpoint, const PURL & to)
  : SIPHandler(SIP_PDU::Method_MESSAGE, endpoint, SIPParameters(to.AsString()))
{
}


SIPTransaction * SIPPingHandler::CreateTransaction(OpalTransport & transport)
{
  if (GetState() == Unsubscribing)
    return NULL;

  return new SIPPing(endpoint, transport, GetAddressOfRecord());
}


//////////////////////////////////////////////////////////////////

/* All of the bwlow search loops run through the list with only
   PSafeReference rather than PSafeReadOnly, even though they are
   reading fields from the handler instances. We can get away with
   this becuase the information being tested, e.g. AOR, is constant
   for the life of the handler instance, once constructed.

   We need to use PSafeReference as there are some cases where
   deadlocks can occur when locked handlers look for information
   from other handlers.
 */
unsigned SIPHandlersList::GetCount(SIP_PDU::Methods meth, const PString & eventPackage) const
{
  unsigned count = 0;
  for (PSafePtr<SIPHandler> handler(m_handlersList, PSafeReference); handler != NULL; ++handler)
    if (handler->GetState () == SIPHandler::Subscribed &&
        handler->GetMethod() == meth &&
        (eventPackage.IsEmpty() || handler->GetEventPackage() == eventPackage))
      count++;
  return count;
}


PStringList SIPHandlersList::GetAddresses(bool includeOffline, SIP_PDU::Methods meth, const PString & eventPackage) const
{
  PStringList addresses;
  for (PSafePtr<SIPHandler> handler(m_handlersList, PSafeReference); handler != NULL; ++handler)
    if ((includeOffline ? handler->GetState () != SIPHandler::Unsubscribed
                        : handler->GetState () == SIPHandler::Subscribed) &&
        handler->GetMethod() == meth &&
        (eventPackage.IsEmpty() || handler->GetEventPackage() == eventPackage))
      addresses.AppendString(handler->GetAddressOfRecord().AsString());
  return addresses;
}


static PString MakeUrlKey(const PURL & aor, SIP_PDU::Methods method, const PString & eventPackage = PString::Empty())
{
  PStringStream key;

  key << method << '\n' << aor;

  if (!eventPackage.IsEmpty())
    key << '\n' << eventPackage;

  return key;
}


/**
  * called when a handler is added
  */

void SIPHandlersList::Append(SIPHandler * newHandler)
{
  if (newHandler == NULL)
    return;

  PWaitAndSignal m(m_extraMutex);

  PSafePtr<SIPHandler> handler = m_handlersList.FindWithLock(*newHandler, PSafeReference);
  if (handler == NULL)
    handler = m_handlersList.Append(newHandler, PSafeReference);

  // add entry to call to handler map
  handler->m_byCallID = m_byCallID.insert(IndexMap::value_type(handler->GetCallID(), handler));

  // add entry to url and package map
  handler->m_byAorAndPackage = m_byAorAndPackage.insert(IndexMap::value_type(MakeUrlKey(handler->GetAddressOfRecord(), handler->GetMethod(), handler->GetEventPackage()), handler));

  // add entry to username/realm map
  PString realm = handler->GetRealm();
  if (realm.IsEmpty())
    return;

  PString username = handler->GetUsername();
  if (!username.IsEmpty()) {
    handler->m_byAuthIdAndRealm = m_byAuthIdAndRealm.insert(IndexMap::value_type(username + '\n' + realm, handler));
    PTRACE_IF(4, !handler->m_byAuthIdAndRealm.second, "Duplicate handler for authId=\"" << username << "\", realm=\"" << realm << '"');
  }

  username = handler->GetAddressOfRecord().GetUserName();
  if (!username.IsEmpty()) {
    handler->m_byAorUserAndRealm = m_byAorUserAndRealm.insert(IndexMap::value_type(username + '\n' + realm, handler));
    PTRACE_IF(4, !handler->m_byAuthIdAndRealm.second, "Duplicate handler for AOR user=\"" << username << "\", realm=\"" << realm << '"');
  }
}


/**
  * Called when a handler is removed
  */
void SIPHandlersList::Remove(SIPHandler * handler)
{
  if (handler == NULL)
    return;

  m_extraMutex.Wait();

  if (m_handlersList.Remove(handler))
    RemoveIndexes(handler);

  m_extraMutex.Signal();
}


void SIPHandlersList::Update(SIPHandler * handler)
{
  if (handler == NULL)
    return;

  m_extraMutex.Wait();

  RemoveIndexes(handler);
  Append(handler);

  m_extraMutex.Signal();
}


void SIPHandlersList::RemoveIndexes(SIPHandler * handler)
{
  if (handler->m_byAorUserAndRealm.second)
    m_byAorUserAndRealm.erase(handler->m_byAorUserAndRealm.first);

  if (handler->m_byAuthIdAndRealm.second)
    m_byAuthIdAndRealm.erase(handler->m_byAuthIdAndRealm.first);

  if (handler->m_byAorAndPackage.second)
    m_byAorAndPackage.erase(handler->m_byAorAndPackage.first);

  if (handler->m_byCallID.second)
    m_byCallID.erase(handler->m_byCallID.first);
}


PSafePtr<SIPHandler> SIPHandlersList::FindBy(IndexMap & by, const PString & key, PSafetyMode mode)
{
  PSafePtr<SIPHandler> ptr;
  {
    PWaitAndSignal m(m_extraMutex);

    IndexMap::iterator r = by.find(key);
    if (r == by.end())
      return NULL;

    ptr = r->second;
    // If above assignement ends up NULL, then entry in IndexMap was deleted
    if (ptr == NULL)
      return NULL;
  }

  if (ptr && ptr->GetState() != SIPHandler::Unsubscribed)
    return ptr.SetSafetyMode(mode) ? ptr : NULL;

  PTRACE(3, "SIP\tHandler " << *ptr << " unsubscribed, awaiting shutdown.");
  while (!ptr->ShutDown())
    PThread::Sleep(100);

  Remove(ptr);
  return NULL;
}


PSafePtr<SIPHandler> SIPHandlersList::FindSIPHandlerByCallID(const PString & callID, PSafetyMode mode)
{
  return FindBy(m_byCallID, callID, mode);
}


PSafePtr<SIPHandler> SIPHandlersList::FindSIPHandlerByUrl(const PURL & aor, SIP_PDU::Methods method, PSafetyMode mode)
{
  return FindBy(m_byAorAndPackage, MakeUrlKey(aor, method), mode);
}


PSafePtr<SIPHandler> SIPHandlersList::FindSIPHandlerByUrl(const PURL & aor, SIP_PDU::Methods method, const PString & eventPackage, PSafetyMode mode)
{
  return FindBy(m_byAorAndPackage, MakeUrlKey(aor, method, eventPackage), mode);
}


/**
 * Find the SIPHandler object with the specified authRealm
 */
PSafePtr<SIPHandler> SIPHandlersList::FindSIPHandlerByAuthRealm(const PString & authRealm, const PString & userName, PSafetyMode mode)
{
  PSafePtr<SIPHandler> ptr;

  if (!userName.IsEmpty()) {
    // look for a match to exact user name and realm
    if ((ptr = FindBy(m_byAuthIdAndRealm, userName + '\n' + authRealm, mode)) != NULL) {
      PTRACE(4, "SIP\tLocated existing credentials for ID \"" << userName << "\" at realm \"" << authRealm << '"');
      return ptr;
    }

    // look for a match to exact user name and realm
    if ((ptr = FindBy(m_byAorUserAndRealm, userName + '\n' + authRealm, mode)) != NULL) {
      PTRACE(4, "SIP\tLocated existing credentials for ID \"" << userName << "\" at realm \"" << authRealm << '"');
      return ptr;
    }
  }

  // look for a match to realm without users
  for (PSafePtr<SIPHandler> handler(m_handlersList, PSafeReference); handler != NULL; ++handler) {
    if (handler->GetRealm() == authRealm && handler.SetSafetyMode(mode)) {
      PTRACE(4, "SIP\tLocated existing credentials for realm \"" << authRealm << '"');
      return handler;
    }
  }

  return NULL;
}


/**
 * Find the SIPHandler object with the specified registration host.
 * For example, in the above case, the name parameter
 * could be "sip.seconix.com" or "seconix.com".
 */
PSafePtr<SIPHandler> SIPHandlersList::FindSIPHandlerByDomain(const PString & name, SIP_PDU::Methods meth, PSafetyMode mode)
{
  for (PSafePtr<SIPHandler> handler(m_handlersList, PSafeReference); handler != NULL; ++handler) {
    if ( handler->GetMethod() == meth &&
         handler->GetState() != SIPHandler::Unsubscribed &&
        (handler->GetAddressOfRecord().GetHostName() == name ||
         handler->GetAddressOfRecord().GetHostAddress().IsEquivalent(name)) &&
         handler.SetSafetyMode(mode))
      return handler;
  }
  return NULL;
}


#endif // OPAL_SIP
