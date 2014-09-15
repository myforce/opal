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
 * $Revision$
 * $Author$
 * $Date$
 */

#include <ptlib.h>
#include <opal_config.h>

#if OPAL_SIP

#ifdef __GNUC__
#pragma implementation "sipcon.h"
#endif

#include <sip/sipcon.h>

#include <sip/sipep.h>
#include <h323/q931.h>
#include <codec/vidcodec.h>
#include <im/sipim.h>
#include <opal/patch.h>
#include <ptclib/random.h>              // for local dialog tag


#define new PNEW

//
//  uncomment this to force pause ReINVITES to have c=0.0.0.0
//
//#define PAUSE_WITH_EMPTY_ADDRESS  1

static const PConstCaselessString HeaderPrefix(SIP_HEADER_PREFIX);
static const PConstCaselessString RemotePartyID("Remote-Party-ID");
static const PConstCaselessString ApplicationDTMFRelayKey("application/dtmf-relay");
static const PConstCaselessString ApplicationDTMFKey("application/dtmf");

#if OPAL_VIDEO
static const char ApplicationMediaControlXMLKey[] = "application/media_control+xml";
#endif


static SIP_PDU::StatusCodes GetStatusCodeFromReason(OpalConnection::CallEndReason reason)
{
  static const struct {
    unsigned             q931Code;
    SIP_PDU::StatusCodes sipCode;
  }
  //
  // This table comes from RFC 3398 para 7.2.4.1
  //
  Q931ToSIPCode[] = {
    {   1, SIP_PDU::Failure_NotFound               }, // Unallocated number
    {   2, SIP_PDU::Failure_NotFound               }, // no route to network
    {   3, SIP_PDU::Failure_NotFound               }, // no route to destination
    {  17, SIP_PDU::Failure_BusyHere               }, // user busy                            
    {  18, SIP_PDU::Failure_RequestTimeout         }, // no user responding                   
    {  19, SIP_PDU::Failure_TemporarilyUnavailable }, // no answer from the user              
    {  20, SIP_PDU::Failure_TemporarilyUnavailable }, // subscriber absent                    
    {  21, SIP_PDU::Failure_Forbidden              }, // call rejected                        
    {  22, SIP_PDU::Failure_Gone                   }, // number changed (w/o diagnostic)      
    {  22, SIP_PDU::Redirection_MovedPermanently   }, // number changed (w/ diagnostic)       
    {  23, SIP_PDU::Failure_Gone                   }, // redirection to new destination       
    {  26, SIP_PDU::Failure_NotFound               }, // non-selected user clearing           
    {  27, SIP_PDU::Failure_BadGateway             }, // destination out of order             
    {  28, SIP_PDU::Failure_AddressIncomplete      }, // address incomplete                   
    {  29, SIP_PDU::Failure_NotImplemented         }, // facility rejected                    
    {  31, SIP_PDU::Failure_TemporarilyUnavailable }, // normal unspecified                   
    {  34, SIP_PDU::Failure_ServiceUnavailable     }, // no circuit available                 
    {  38, SIP_PDU::Failure_ServiceUnavailable     }, // network out of order                 
    {  41, SIP_PDU::Failure_ServiceUnavailable     }, // temporary failure                    
    {  42, SIP_PDU::Failure_ServiceUnavailable     }, // switching equipment congestion       
    {  47, SIP_PDU::Failure_ServiceUnavailable     }, // resource unavailable                 
    {  55, SIP_PDU::Failure_Forbidden              }, // incoming calls barred within CUG     
    {  57, SIP_PDU::Failure_Forbidden              }, // bearer capability not authorized     
    {  58, SIP_PDU::Failure_ServiceUnavailable     }, // bearer capability not presently available
    {  65, SIP_PDU::Failure_NotAcceptableHere      }, // bearer capability not implemented
    {  70, SIP_PDU::Failure_NotAcceptableHere      }, // only restricted digital avail    
    {  79, SIP_PDU::Failure_NotImplemented         }, // service or option not implemented
    {  87, SIP_PDU::Failure_Forbidden              }, // user not member of CUG           
    {  88, SIP_PDU::Failure_ServiceUnavailable     }, // incompatible destination         
    { 102, SIP_PDU::Failure_ServerTimeout          }, // recovery of timer expiry         
    { 111, SIP_PDU::Failure_InternalServerError    }, // protocol error                   
    { 127, SIP_PDU::Failure_InternalServerError    }, // interworking unspecified         
  };
  for (PINDEX i = 0; i < PARRAYSIZE(Q931ToSIPCode); i++) {
    if (Q931ToSIPCode[i].q931Code == reason.q931)
      return Q931ToSIPCode[i].sipCode;
  }

  static const struct {
    OpalConnection::CallEndReasonCodes reasonCode;
    SIP_PDU::StatusCodes               sipCode;
  }
  ReasonToSIPCode[] = {
    { OpalConnection::EndedByNoUser            , SIP_PDU::Failure_NotFound               }, // Unallocated number
    { OpalConnection::EndedByLocalBusy         , SIP_PDU::Failure_BusyHere               }, // user busy                            
    { OpalConnection::EndedByNoAnswer          , SIP_PDU::Failure_RequestTimeout         }, // no user responding                   
    { OpalConnection::EndedByNoUser            , SIP_PDU::Failure_TemporarilyUnavailable }, // subscriber absent                    
    { OpalConnection::EndedByCapabilityExchange, SIP_PDU::Failure_NotAcceptableHere      }, // bearer capability not implemented
    { OpalConnection::EndedByCallerAbort       , SIP_PDU::Failure_RequestTerminated      },
    { OpalConnection::EndedByCallForwarded     , SIP_PDU::Redirection_MovedTemporarily   },
    { OpalConnection::EndedByAnswerDenied      , SIP_PDU::GlobalFailure_Decline          },
    { OpalConnection::EndedByRefusal           , SIP_PDU::GlobalFailure_Decline          }, // TODO - SGW - add for call reject from H323 side.
    { OpalConnection::EndedByHostOffline       , SIP_PDU::Failure_NotFound               }, // TODO - SGW - add for no ip from H323 side.
    { OpalConnection::EndedByNoEndPoint        , SIP_PDU::Failure_NotFound               }, // TODO - SGW - add for endpoints not running on a ip from H323 side.
    { OpalConnection::EndedByUnreachable       , SIP_PDU::Failure_Forbidden              }, // TODO - SGW - add for avoid sip calls to SGW IP.
    { OpalConnection::EndedByNoBandwidth       , SIP_PDU::GlobalFailure_NotAcceptable    }, // TODO - SGW - added to reject call when no bandwidth 
    { OpalConnection::EndedByInvalidConferenceID,SIP_PDU::Failure_TransactionDoesNotExist}
  };

  for (PINDEX i = 0; i < PARRAYSIZE(ReasonToSIPCode); i++) {
    if (ReasonToSIPCode[i].reasonCode == reason.code)
      return ReasonToSIPCode[i].sipCode;
  }

  return SIP_PDU::Failure_BadGateway;
}

static OpalConnection::CallEndReason GetCallEndReasonFromResponse(SIP_PDU & response)
{
  //
  // This table comes from RFC 3398 para 8.2.6.1
  //
  static const struct {
    SIP_PDU::StatusCodes               sipCode;
    OpalConnection::CallEndReasonCodes reasonCode;
    unsigned                           q931Code;
  } SIPCodeToReason[] = {
    { SIP_PDU::Local_Timeout                      , OpalConnection::EndedByHostOffline       ,  27 }, // Destination out of order
    { SIP_PDU::Failure_RequestTerminated          , OpalConnection::EndedByNoAnswer          ,  19 }, // No answer
    { SIP_PDU::Failure_BadRequest                 , OpalConnection::EndedByQ931Cause         ,  41 }, // Temporary Failure
    { SIP_PDU::Failure_UnAuthorised               , OpalConnection::EndedBySecurityDenial    ,  21 }, // Call rejected (*)
    { SIP_PDU::Failure_PaymentRequired            , OpalConnection::EndedByRefusal           ,  21 }, // Call rejected
    { SIP_PDU::Failure_Forbidden                  , OpalConnection::EndedBySecurityDenial    ,  21 }, // Call rejected
    { SIP_PDU::Failure_NotFound                   , OpalConnection::EndedByNoUser            ,   1 }, // Unallocated number
    { SIP_PDU::Failure_MethodNotAllowed           , OpalConnection::EndedByQ931Cause         ,  63 }, // Service or option unavailable
    { SIP_PDU::Failure_NotAcceptable              , OpalConnection::EndedByQ931Cause         ,  79 }, // Service/option not implemented (+)
    { SIP_PDU::Failure_ProxyAuthenticationRequired, OpalConnection::EndedBySecurityDenial    ,  21 }, // Call rejected (*)
    { SIP_PDU::Failure_RequestTimeout             , OpalConnection::EndedByTemporaryFailure  , 102 }, // Recovery on timer expiry
    { SIP_PDU::Failure_Gone                       , OpalConnection::EndedByQ931Cause         ,  22 }, // Number changed (w/o diagnostic)
    { SIP_PDU::Failure_RequestEntityTooLarge      , OpalConnection::EndedByQ931Cause         , 127 }, // Interworking (+)
    { SIP_PDU::Failure_RequestURITooLong          , OpalConnection::EndedByQ931Cause         , 127 }, // Interworking (+)
    { SIP_PDU::Failure_UnsupportedMediaType       , OpalConnection::EndedByCapabilityExchange,  79 }, // Service/option not implemented (+)
    { SIP_PDU::Failure_NotAcceptableHere          , OpalConnection::EndedByCapabilityExchange,  79 }, // Service/option not implemented (+)
    { SIP_PDU::Failure_UnsupportedURIScheme       , OpalConnection::EndedByQ931Cause         , 127 }, // Interworking (+)
    { SIP_PDU::Failure_BadExtension               , OpalConnection::EndedByNoUser            , 127 }, // Interworking (+)
    { SIP_PDU::Failure_ExtensionRequired          , OpalConnection::EndedByNoUser            , 127 }, // Interworking (+)
    { SIP_PDU::Failure_IntervalTooBrief           , OpalConnection::EndedByQ931Cause         , 127 }, // Interworking (+)
    { SIP_PDU::Failure_UnresolvableDestination    , OpalConnection::EndedByIllegalAddress    ,  28 }, // Invalid Number Format (+)
    { SIP_PDU::Failure_TemporarilyUnavailable     , OpalConnection::EndedByTemporaryFailure  ,  18 }, // No user responding
    { SIP_PDU::Failure_TransactionDoesNotExist    , OpalConnection::EndedByQ931Cause         ,  41 }, // Temporary Failure
    { SIP_PDU::Failure_LoopDetected               , OpalConnection::EndedByQ931Cause         ,  25 }, // Exchange - routing error
    { SIP_PDU::Failure_TooManyHops                , OpalConnection::EndedByQ931Cause         ,  25 }, // Exchange - routing error
    { SIP_PDU::Failure_AddressIncomplete          , OpalConnection::EndedByQ931Cause         ,  28 }, // Invalid Number Format (+)
    { SIP_PDU::Failure_Ambiguous                  , OpalConnection::EndedByNoUser            ,   1 }, // Unallocated number
    { SIP_PDU::Failure_BusyHere                   , OpalConnection::EndedByRemoteBusy        ,  17 }, // User busy
    { SIP_PDU::Failure_InternalServerError        , OpalConnection::EndedByOutOfService      ,  41 }, // Temporary failure
    { SIP_PDU::Failure_NotImplemented             , OpalConnection::EndedByQ931Cause         ,  79 }, // Not implemented, unspecified
    { SIP_PDU::Failure_BadGateway                 , OpalConnection::EndedByOutOfService      ,  38 }, // Network out of order
    { SIP_PDU::Failure_ServiceUnavailable         , OpalConnection::EndedByOutOfService      ,  41 }, // Temporary failure
    { SIP_PDU::Failure_ServerTimeout              , OpalConnection::EndedByOutOfService      , 102 }, // Recovery on timer expiry
    { SIP_PDU::Failure_SIPVersionNotSupported     , OpalConnection::EndedByQ931Cause         , 127 }, // Interworking (+)
    { SIP_PDU::Failure_MessageTooLarge            , OpalConnection::EndedByUnreachable       , 127 }, // Interworking (+)
    { SIP_PDU::GlobalFailure_BusyEverywhere       , OpalConnection::EndedByRemoteBusy        ,  17 }, // User busy
    { SIP_PDU::GlobalFailure_Decline              , OpalConnection::EndedByRefusal           ,  21 }, // Call rejected
    { SIP_PDU::GlobalFailure_DoesNotExistAnywhere , OpalConnection::EndedByNoUser            ,   1 }, // Unallocated number
  };

  for (PINDEX i = 0; i < PARRAYSIZE(SIPCodeToReason); i++) {
    if (response.GetStatusCode() == SIPCodeToReason[i].sipCode)
      return OpalConnection::CallEndReason(SIPCodeToReason[i].reasonCode, SIPCodeToReason[i].q931Code);
  }

  // default Q.931 code is 31 Normal, unspecified
  return OpalConnection::CallEndReason(OpalConnection::EndedByQ931Cause, Q931::NormalUnspecified);
}


////////////////////////////////////////////////////////////////////////////

SIPConnection::SIPConnection(const Init & init)
  : OpalSDPConnection(init.m_call, init.m_endpoint, init.m_token, init.m_options, init.m_stringOptions)
  , P_DISABLE_MSVC_WARNINGS(4355, SIPTransactionOwner(*this, init.m_endpoint))
  , m_allowedMethods((1<<SIP_PDU::Method_INVITE)|
                     (1<<SIP_PDU::Method_ACK   )|
                     (1<<SIP_PDU::Method_CANCEL)|
                     (1<<SIP_PDU::Method_BYE   )) // Minimum set
  , m_lastReceivedINVITE(NULL)
  , m_delayedAckInviteResponse(NULL)
  , m_delayedAckTimer(init.m_endpoint.GetThreadPool(), init.m_endpoint, init.m_token, &SIPConnection::OnDelayedAckTimeout)
  , m_delayedAckTimeout1(init.m_endpoint.GetInviteTimeout() - PTimeInterval(0,2)) // A couple of seconds shorter
  , m_delayedAckTimeout2(0, 2) // second(s)
  , m_delayedAckPDU(NULL)
  , m_sdpSessionId(PTime().GetTimeInSeconds())
  , m_sdpVersion(0)
  , m_needReINVITE(false)
  , m_handlingINVITE(false)
  , m_resolveMultipleFormatReINVITE(true)
  , m_symmetricOpenStream(false)
  , m_appearanceCode(init.m_endpoint.GetDefaultAppearanceCode())
#if OPAL_VIDEO
  , m_canDoVideoFastUpdateINFO(true)
#endif
  , m_sessionTimer(init.m_endpoint.GetThreadPool(), init.m_endpoint, init.m_token, &SIPConnection::OnSessionTimeout)
  , m_prackMode((PRACKMode)m_stringOptions.GetInteger(OPAL_OPT_PRACK_MODE, init.m_endpoint.GetDefaultPRACKMode()))
  , m_prackEnabled(false)
  , m_prackSequenceNumber(0)
  , m_responseFailTimer(init.m_endpoint.GetThreadPool(), init.m_endpoint, init.m_token, &SIPConnection::OnInviteResponseTimeout)
  , m_responseRetryTimer(init.m_endpoint.GetThreadPool(), init.m_endpoint, init.m_token, &SIPConnection::OnInviteResponseRetry)
  , m_responseRetryCount(0)
  , m_inviteCollisionTimer(init.m_endpoint.GetThreadPool(), init.m_endpoint, init.m_token, &SIPConnection::OnInviteCollision)
  , m_referInProgress(false)
  , releaseMethod(ReleaseWithNothing)
  , m_receivedUserInputMethod(UserInputMethodUnknown)
{
  SIPURL adjustedDestination = init.m_address;

  // Look for a "proxy" parameter to override default proxy
  PStringToString params = adjustedDestination.GetParamVars();
  SIPURL proxy;
  if (params.Contains(OPAL_PROXY_PARAM)) {
    proxy.Parse(params[OPAL_PROXY_PARAM]);
    adjustedDestination.SetParamVar(OPAL_PROXY_PARAM, PString::Empty());
  }

  if (params.Contains("x-line-id")) {
    m_appearanceCode = params["x-line-id"].AsUnsigned();
    adjustedDestination.SetParamVar("x-line-id", PString::Empty());
  }

  if (params.Contains("appearance")) {
    m_appearanceCode = params["appearance"].AsUnsigned();
    adjustedDestination.SetParamVar("appearance", PString::Empty());
  }

  const PStringToString & query = adjustedDestination.GetQueryVars();
  for (PStringToString::const_iterator it = query.begin(); it != query.end(); ++it)
    m_stringOptions.SetAt(HeaderPrefix+it->first, it->second);
  adjustedDestination.SetQuery(PString::Empty());

  m_stringOptions.ExtractFromURL(adjustedDestination);

  m_dialog.SetRequestURI(adjustedDestination);
  m_dialog.SetRemoteURI(adjustedDestination);
  m_dialog.SetLocalTag(GetToken());

  if (init.m_invite != NULL) {
    m_remoteAddress = init.m_invite->GetTransport()->GetRemoteAddress();
    m_lastReceivedINVITE = new SIP_PDU(*init.m_invite);
    m_dialog.Update(*m_lastReceivedINVITE);
  }

  // Update remote party parameters
  UpdateRemoteAddresses();

  if (proxy.IsEmpty())
    proxy = GetEndPoint().GetProxy();

  m_dialog.SetProxy(proxy, false); // No default routeSet if there is a proxy for INVITE

  m_forkedInvitations.DisallowDeleteObjects();
  m_pendingInvitations.DisallowDeleteObjects();

  PTRACE(4, "SIP\tCreated connection.");
}


SIPConnection::~SIPConnection()
{
  PTRACE(4, "SIP\tDeleting connection.");

  delete m_lastReceivedINVITE;
  delete m_delayedAckInviteResponse;
  delete m_delayedAckPDU;
}


void SIPConnection::OnApplyStringOptions()
{
  m_prackMode = (PRACKMode)m_stringOptions.GetInteger(OPAL_OPT_PRACK_MODE, m_prackMode);
  m_dialog.SetInterface(m_stringOptions(OPAL_OPT_INTERFACE, GetInterface()));
  OpalRTPConnection::OnApplyStringOptions();
}


bool SIPConnection::GarbageCollection()
{
  return CleanPendingTransactions() & OpalRTPConnection::GarbageCollection();
}


void SIPConnection::OnReleased()
{
  PTRACE(3, "SIP\tOnReleased: " << *this);

  if (!PAssert(LockReadWrite(),PLogicError))
    return;

  if (m_referInProgress) {
    m_referInProgress = false;

    PStringToString info;
    info.SetAt("result", "blind");
    info.SetAt("party", "B");
    OnTransferNotify(info, this);
  }

  UnlockReadWrite();

  SIPDialogNotification::Events notifyDialogEvent = SIPDialogNotification::NoEvent;
  SIP_PDU::StatusCodes sipCode = SIP_PDU::IllegalStatusCode;

  PSafePtr<SIPBye> bye;

  switch (releaseMethod) {
    case ReleaseWithNothing :
      if (!m_forkedInvitations.IsEmpty())
        notifyDialogEvent = SIPDialogNotification::Timeout;
      break;

    case ReleaseWithResponse :
      // Try find best match for return code
      sipCode = GetStatusCodeFromReason(callEndReason);

      if (!PAssert(LockReadWrite(),PLogicError))
        return;

      // EndedByCallForwarded is a special case because it needs extra paramater
      if (callEndReason != EndedByCallForwarded)
        SendInviteResponse(sipCode);
      else {
        SIP_PDU response(*m_lastReceivedINVITE, sipCode);
        AdjustInviteResponse(response);
        response.GetMIME().SetContact(SIPURL(m_forwardParty).AsQuotedString());
        response.Send();
      }

      UnlockReadWrite();

      /* Wait for ACK from remote before destroying object. Note that we either
         get the ACK, or OnAckTimeout() fires and sets this flag anyway. We are
         outside of a connection mutex lock at this point, so no deadlock
         should occur. Really should be a PSyncPoint. */
      while (!m_responsePackets.empty())
        PThread::Sleep(100);

      notifyDialogEvent = SIPDialogNotification::Rejected;
      break;

    case ReleaseWithBYE :
      if (!PAssert(LockReadWrite(),PLogicError))
        return;

      // create BYE now & delete it later to prevent memory access errors
      bye = new SIPBye(*this);
      if (!bye->Start())
        bye.SetNULL();

      UnlockReadWrite();
      break;

    case ReleaseWithCANCEL :
      notifyDialogEvent = SIPDialogNotification::Cancelled;
  }

  PTRACE(3, "SIP\tCancelling " << m_forkedInvitations.GetSize() << " invitations.");
  for (PSafePtr<SIPTransaction> invitation(m_forkedInvitations, PSafeReference); invitation != NULL; ++invitation) {
    /* If we never even received a "100 Trying" from a remote, then just abort
       the transaction, do not wait, it is probably on an interface that the
       remote is not physically on, otherwise we have to CANCEL and wait. */
    if (invitation->IsInProgress())
      invitation->Cancel();
    else
      invitation->Abort();
  }

  // Abort the queued up re-INVITEs we never got a chance to send.
  for (PSafePtr<SIPTransaction> invitation(m_pendingInvitations, PSafeReference); invitation != NULL; ++invitation)
    invitation->Abort();

  // Remove all the references to the transactions so garbage can be collected
  m_pendingInvitations.RemoveAll();
  m_forkedInvitations.RemoveAll();

  // No termination event set yet, get it from the call end reason
  if (notifyDialogEvent == SIPDialogNotification::NoEvent) {
    switch (GetCallEndReason()) {
      case EndedByRemoteUser :
        notifyDialogEvent = SIPDialogNotification::RemoteBye;
        break;

      case OpalConnection::EndedByCallForwarded :
        notifyDialogEvent = SIPDialogNotification::Replaced;
        break;

      default :
        notifyDialogEvent = SIPDialogNotification::LocalBye;
    }
  }

  if (LockReadOnly()) {
    NotifyDialogState(SIPDialogNotification::Terminated, notifyDialogEvent, sipCode);
    UnlockReadOnly();
  }

  if (bye != NULL) {
    bye->WaitForCompletion();
    bye.SetNULL();
  }

  /* CANCEL any pending transactions, e.g. INFO messages. These should timeout
     and go away, but a pathological case with a broken proxy had it returning
     a 100 Trying on every request. The RFC3261/17.1.4 state machine does not
     have an out for the client in this case, it would continue forever if the
     server says so. Fail safe on call termination. */
  AbortPendingTransactions();

  // Close media and indicate call ended, even though we have a little bit more
  // to go in clean up, don't let other bits wait for it.
  OpalRTPConnection::OnReleased();

  // If forwardParty is a connection token, then must be INVITE with replaces scenario
  if (!m_forwardParty.IsEmpty()) {
    PSafePtr<OpalConnection> replacerConnection = GetEndPoint().GetConnectionWithLock(m_forwardParty);
    if (replacerConnection != NULL) {
      /* According to RFC 3891 we now send a 200 OK in both the early and confirmed
         dialog cases. OnReleased() is responsible for if the replaced connection is
         sent a BYE or a CANCEL. */
      replacerConnection->SetConnected();
    }
  }
}


bool SIPConnection::TransferConnection(const PString & remoteParty)
{
  if (IsReleased())
    return false;

  // There is still an ongoing REFER transaction 
  if (m_referInProgress) {
    PTRACE(2, "SIP\tTransfer already in progress for " << *this);
    return false;
  }

  if ((m_allowedMethods&(1<<SIP_PDU::Method_REFER)) == 0) {
    PTRACE(2, "SIP\tRemote does not allow REFER message.");
    return false;
  }

  PURL url(remoteParty, "sip");
  StringOptions extra;
  extra.ExtractFromURL(url);

  if (!(IsOriginating() || IsEstablished() || extra.GetBoolean(OPAL_OPT_FORWARD_REFER)))
    return ForwardCall(remoteParty);

  // Tell the REFER processing UA if it should suppress NOTIFYs about the REFER processing.
  // If we want to get NOTIFYs we have to clear the old connection on the progress message
  // where the connection is transfered. See OnTransferNotify().
  bool referSub = extra.GetBoolean(OPAL_OPT_REFER_SUB, m_stringOptions.GetBoolean(OPAL_OPT_REFER_SUB, true));

  // Check for valid RFC2396 scheme
  if (!PURL::ExtractScheme(remoteParty).IsEmpty()) {
    PTRACE(3, "SIP\tBlind transfer of " << *this << " to " << remoteParty);
    SIPRefer * referTransaction = new SIPRefer(*this, remoteParty, m_dialog.GetLocalURI(), referSub);
    m_referInProgress = referTransaction->Start();
    return m_referInProgress;
  }

  PSafePtr<OpalCall> call = endpoint.GetManager().FindCallWithLock(url.GetHostName(), PSafeReadOnly);
  if (call == NULL) {
    PTRACE(2, "SIP\tRemote party \"" << remoteParty << "\" must be valid call token or URI.");
    return false;
  }

  if (call == &ownerCall) {
    PTRACE(2, "SIP\tCannot transfer connection to itself: " << *this);
    return false;
  }

  PTRACE(3, "SIP\tConsultation transfer of " << *this << " to " << remoteParty);

  for (PSafePtr<OpalConnection> connection = call->GetConnection(0); connection != NULL; ++connection) {
    PSafePtr<SIPConnection> sip = PSafePtrCast<OpalConnection, SIPConnection>(connection);
    if (sip != NULL) {
      /* Note that the order of to-tag and remote-tag is counter intuitive. This is because
        the call being referred to by the call token in remoteParty is not the A party in
        the consultation transfer, but the B party. */
      PTRACE(4, "SIP\tTransferring " << *this << " to remote of " << *sip);

      /* The following is to compensate for Avaya who send a Contact without a
         username in the URL and then get upset later in th REFER when we use
         what they told us to use. They can't do the REFER without a username
         part, but they never gave us a username to give them. Give me a break!
       */
      SIPURL referTo = sip->GetRemotePartyURL();
      if (remoteProductInfo.name == "Avaya" && referTo.GetUserName().IsEmpty())
        referTo.SetUserName("anonymous");

      PStringStream id;
      id <<                 sip->GetDialog().GetCallID()
         << ";to-tag="   << sip->GetDialog().GetRemoteTag()
         << ";from-tag=" << sip->GetDialog().GetLocalTag();
      referTo.SetQueryVar("Replaces", id);

      SIPRefer * referTransaction = new SIPRefer(*this, referTo, m_dialog.GetLocalURI(), referSub);
      referTransaction->GetMIME().AddSupported("replaces");
      m_referInProgress = referTransaction->Start();
      return m_referInProgress;
    }
  }

  PTRACE(2, "SIP\tConsultation transfer requires other party to be SIP.");
  return false;
}


PBoolean SIPConnection::SetAlerting(const PString & /*calleeName*/, PBoolean withMedia)
{
  PSafeLockReadWrite safeLock(*this);
  if (!safeLock.IsLocked())
    return false;

  if (IsOriginating() || m_lastReceivedINVITE == NULL) {
    PTRACE(2, "SIP\tSetAlerting ignored on call we originated.");
    return true;
  }

  if (GetPhase() >= ConnectedPhase) {
    PTRACE(2, "SIP\tSetAlerting ignored on released or already connected call " << *this);
    return false;
  }

  PTRACE(3, "SIP\tSetAlerting " << (withMedia ? "with" : "no") << " media");

  if (!withMedia && (!m_prackEnabled || m_lastReceivedINVITE->GetSDP() != NULL))
    SendInviteResponse(SIP_PDU::Information_Ringing);
  else {
    if (!OnSendAnswer(SIP_PDU::Information_Session_Progress)) {
      Release(EndedByCapabilityExchange);
      return false;
    }
  }

  SetPhase(AlertingPhase);
  NotifyDialogState(SIPDialogNotification::Early);

  return true;
}


PBoolean SIPConnection::SetConnected()
{
  PSafeLockReadWrite safeLock(*this);
  if (!safeLock.IsLocked())
    return false;

  if (IsOriginating()) {
    PTRACE(2, "SIP\tSetConnected ignored on call we originated " << *this);
    return true;
  }

  if (GetPhase() >= ConnectedPhase) {
    PTRACE(2, "SIP\tSetConnected ignored on released or already connected call " << *this);
    return false;
  }
  
  PTRACE(3, "SIP\tSetConnected " << *this);

  GetLocalMediaFormats();

  // send the 200 OK response
  PString externalSDP = m_stringOptions(OPAL_OPT_EXTERNAL_SDP);
  if (externalSDP.IsEmpty()) {
    if (!OnSendAnswer(SIP_PDU::Successful_OK)) {
      Release(EndedByCapabilityExchange);
      return false;
    }
  }
  else {
    SIP_PDU response(*m_lastReceivedINVITE, SIP_PDU::Successful_OK);
    AdjustInviteResponse(response);
    response.SetEntityBody(externalSDP);
    if (!response.Send()) {
      Release(EndedByTransportFail);
      return false;
    }
  }

  releaseMethod = ReleaseWithBYE;
  m_sessionTimer = 10000;

  NotifyDialogState(SIPDialogNotification::Confirmed);

  // switch phase and if media was previously set up, then move to Established
  return OpalConnection::SetConnected();
}


bool SIPConnection::GetMediaTransportAddresses(OpalConnection & otherConnection,
                                                     unsigned   sessionId,
                                          const OpalMediaType & mediaType,
                                    OpalTransportAddressArray & transports) const
{
  if (!OpalSDPConnection::GetMediaTransportAddresses(otherConnection, sessionId, mediaType, transports))
    return false;

  SDPSessionDescription * sdp = NULL;
  if (m_delayedAckInviteResponse != NULL)
    sdp = m_delayedAckInviteResponse->GetSDP();
  else if (m_lastReceivedINVITE != NULL)
    sdp = m_lastReceivedINVITE->GetSDP();

  SDPMediaDescription * md = NULL;
  if (sdp != NULL) {
    md = sdp->GetMediaDescriptionByIndex(sessionId);
    if (md == NULL || md->GetMediaType() != mediaType)
      md = sdp->GetMediaDescriptionByType(mediaType);
  }

  if (md != NULL && transports.SetAddressPair(md->GetMediaAddress(), md->GetControlAddress())) {
    PTRACE(3, "SIP\tGetMediaTransportAddresses of " << mediaType << " found remote SDP "
           << setfill(',') << transports << " for " << otherConnection << " on " << *this);
  }

  return true;
}


OpalTransportAddress SIPConnection::GetDefaultSDPConnectAddress(WORD port) const
{
  PIPSocket::Address localIP(GetInterface());

  if (m_lastReceivedINVITE != NULL) {
    OpalTransportPtr transport = m_lastReceivedINVITE->GetTransport();
    if (transport != NULL)
      transport->GetLocalAddress().GetIpAddress(localIP);
  }

  if (localIP.IsValid())
    return OpalTransportAddress(localIP, port, m_dialog.GetRequestURI().GetTransportProto());

  PTRACE(1, "SIP\tNot using IP transport");
  return OpalTransportAddress();
}


OpalMediaFormatList SIPConnection::GetMediaFormats() const
{
  OpalMediaFormatList formats = OpalSDPConnection::GetMediaFormats();

  /* This executed only before or during OnIncomingConnection on receipt of an
     INVITE. In other cases the m_remoteFormatList member should be set. As
     some things, e.g. Instant Messaging, may need to route the call based on
     what was offerred by the remote, we need to return something. So,
     calculate a media list based on ANYTHING we know about. A later call
     after OnIncomingConnection() will fill m_remoteFormatList appropriately
     adjusted by AdjustMediaFormats() */
  if (formats.IsEmpty() && m_lastReceivedINVITE != NULL && m_lastReceivedINVITE->IsContentSDP()) {
    std::auto_ptr<SDPSessionDescription> sdp(GetEndPoint().CreateSDP(0, 0, OpalTransportAddress()));
    if (sdp->Decode(m_lastReceivedINVITE->GetEntityBody(), OpalMediaFormat::GetAllRegisteredMediaFormats()))
      formats = sdp->GetMediaFormats();
  }

  return formats;
}


int SIPConnection::SetRemoteMediaFormats(SIP_PDU & pdu)
{
  /* As SIP does not really do capability exchange, if we don't have an initial
     INVITE from the remote (indicated by sdp == NULL) then all we can do is
     assume the the remote can at do what we can do. We could assume it does
     everything we know about, but there is no point in assuming it can do any
     more than we can, really.
     */
  if (!pdu.DecodeSDP(GetEndPoint(), GetLocalMediaFormats()))
    return 0;

  m_remoteFormatList = pdu.GetSDP()->GetMediaFormats();
  AdjustMediaFormats(false, NULL, m_remoteFormatList);

  if (m_remoteFormatList.IsEmpty()) {
    PTRACE(2, "SIP\tAll possible media formats to offer were removed.");
    return -1;
  }

#if OPAL_T38_CAPABILITY
  /* We default to having T.38 included as most UAs do not actually
     tell you that they support it or not. For the re-INVITE mechanism
     to work correctly, the rest ofthe system has to assume that the
     UA is capable of it, even it it isn't. */
  m_remoteFormatList += OpalT38;
#endif

  PTRACE(4, "SIP\tRemote media formats set:\n    " << setfill(',') << m_remoteFormatList << setfill(' '));

  return 1;
}


bool SIPConnection::RequireSymmetricMediaStreams() const
{
  /* Technically SIP can do asymmetric media streams, G.711 one way, G,722 the
     other, but it is really difficult, we don't support it (yet) and it is
     highly likely to cause interop issues as many others don't support it
     either. So for now require symmetry. */
  return true;
}


#if OPAL_T38_CAPABILITY
bool SIPConnection::SwitchFaxMediaStreams(bool toT38)
{
  if (ownerCall.IsSwitchingT38()) {
    PTRACE(2, "SIP\tNested call to SwitchFaxMediaStreams on " << *this);
    return false;
  }

  ownerCall.SetSwitchingT38(toT38);

  PTRACE(3, "SIP\tSwitching to " << (toT38 ? "T.38" : "audio") << " on " << *this);
  OpalMediaFormat format = toT38 ? OpalT38 : OpalG711uLaw;
  if (ownerCall.OpenSourceMediaStreams(*this, format.GetMediaType(), 1, format))
    return true;

  ownerCall.ResetSwitchingT38();
  return false;
}
#endif // OPAL_T38_CAPABILITY


OpalMediaStream * SIPConnection::CreateMediaStream(const OpalMediaFormat & mediaFormat,
                                                   unsigned sessionID,
                                                   PBoolean isSource)
{
  OpalMediaType mediaType = mediaFormat.GetMediaType();

  PString sessionType;

#if  OPAL_T38_CAPABILITY
  if (!ownerCall.IsSwitchingT38())
#endif // OPAL_T38_CAPABILITY
  {
    SDPSessionDescription * sdp = NULL;
    if (m_delayedAckInviteResponse != NULL)
      sdp = m_delayedAckInviteResponse->GetSDP();
    else if (m_lastReceivedINVITE != NULL)
      sdp = m_lastReceivedINVITE->GetSDP();

    if (sdp != NULL) {
      SDPMediaDescription * mediaDescription = sdp->GetMediaDescriptionByIndex(sessionID);
      if (mediaDescription != NULL && mediaDescription->GetMediaType() == mediaType)
        sessionType = mediaDescription->GetSessionType();

      if (sessionType.IsEmpty()) {
        PTRACE(2, "SIP\tNo SDP offer of " << mediaType << " stream for session " << sessionID);
        if (m_delayedAckInviteResponse != NULL)
          return NULL;
        sessionID = sdp->GetMediaDescriptions().GetSize()+1;
      }
    }
  }

  OpalMediaSession * mediaSession = UseMediaSession(sessionID, mediaType, sessionType);
  if (mediaSession == NULL) {
    PTRACE(1, "SIP\tUnable to create " << mediaType << " stream for session " << sessionID);
    return NULL;
  }

  if (mediaSession->GetMediaType() != mediaType) {
    PTRACE(3, "SIP\tReplacing " << mediaSession->GetMediaType() << " session " << sessionID << " with " << mediaType);
    mediaSession = CreateMediaSession(sessionID, mediaType, sessionType);
    ReplaceMediaSession(sessionID, mediaSession);
  }

  return mediaSession->CreateMediaStream(mediaFormat, sessionID, isSource);
}


OpalMediaStreamPtr SIPConnection::OpenMediaStream(const OpalMediaFormat & mediaFormat, unsigned sessionID, bool isSource)
{
  if (m_holdFromRemote && !isSource && !m_handlingINVITE) {
    PTRACE(3, "SIP\tCannot start media stream as are currently in HOLD by remote.");
    return NULL;
  }

  // Make sure stream is symmetrical, if codec changed, close and re-open it
  OpalMediaStreamPtr otherStream = GetMediaStream(sessionID, !isSource);
  bool makesymmetrical = !m_symmetricOpenStream &&
                          otherStream != NULL &&
                          otherStream->IsOpen() &&
                          otherStream->GetMediaFormat() != mediaFormat;
  if (makesymmetrical) {
    PTRACE(4, "SIP\tSymmetric media stream for " << mediaFormat << " required, closing");
    m_symmetricOpenStream = true;
    // We must make sure reverse stream is closed before opening the
    // new forward one or can really confuse the RTP stack, especially
    // if switching to udptl in fax mode
    if (isSource) {
      OpalMediaPatchPtr patch = otherStream->GetPatch();
      if (patch != NULL)
        patch->GetSource().Close();
    }
    else
      otherStream->Close();
    m_symmetricOpenStream = false;
  }

  OpalMediaStreamPtr oldStream = GetMediaStream(sessionID, isSource);

  // Open forward side
  OpalMediaStreamPtr newStream = OpalRTPConnection::OpenMediaStream(mediaFormat, sessionID, isSource);
  if (newStream == NULL)
    return newStream;

  // Open other direction, if needed (must be after above open)
  if (makesymmetrical) {
    m_symmetricOpenStream = true;

    PSafePtr<OpalConnection> otherConnection = isSource ? GetCall().GetOtherPartyConnection(*this) : this;
    bool ok = false;
    if (otherConnection != NULL) {
      PTRACE(4, "SIP\tSymmetric media stream for " << mediaFormat << " required, opening");
      ok = GetCall().OpenSourceMediaStreams(*otherConnection, mediaFormat.GetMediaType(), sessionID, mediaFormat);
    }

    m_symmetricOpenStream = false;

    if (!ok) {
      newStream->Close();
      return NULL;
    }
  }

  if (!m_symmetricOpenStream && !m_handlingINVITE && GetPhase() == EstablishedPhase &&
              (newStream != oldStream || GetMediaStream(sessionID, !isSource) != otherStream))
    SendReINVITE(PTRACE_PARAM("open channel"));

  return newStream;
}


void SIPConnection::OnPatchMediaStream(PBoolean isSource, OpalMediaPatch & patch)
{
  if (!SendDelayedACK(false))
    m_delayedAckTimer = m_delayedAckTimeout2;
  OpalRTPConnection::OnPatchMediaStream(isSource, patch);
}


void SIPConnection::OnClosedMediaStream(const OpalMediaStream & stream)
{
  if (!m_symmetricOpenStream &&
      !m_handlingINVITE &&
#if OPAL_T38_CAPABILITY
      !ownerCall.IsSwitchingT38() &&
#endif // OPAL_T38_CAPABILITY
      GetPhase() == EstablishedPhase)
    SendReINVITE(PTRACE_PARAM("close channel"));

  OpalConnection::OnClosedMediaStream(stream);
}


void SIPConnection::OnPauseMediaStream(OpalMediaStream & strm, bool paused)
{
  /* If we have pasued the transmit RTP, we really need to tell the other side
     via a re-INVITE or some systems disconnect the call becuase they have not
     received any RTP for too long. */
  if (!m_symmetricOpenStream && !m_handlingINVITE && strm.IsSink())
    SendReINVITE(PTRACE_PARAM(paused ? "pausing channel" : "resume channel"));
  OpalConnection::OnPauseMediaStream(strm, paused);
}


void SIPConnection::OnInviteCollision()
{
  SendReINVITE(PTRACE_PARAM("resend after pending received"));
}


void SIPConnection::OnMediaStreamOpenFailed(bool rx)
{
  SendReINVITE(PTRACE_PARAM(rx ? "close after rx open fail" : "close after tx open fail"));
}


bool SIPConnection::SendReINVITE(PTRACE_PARAM(const char * msg))
{
  if (IsReleased())
    return false;

  bool startImmediate = !m_handlingINVITE && m_pendingInvitations.IsEmpty();

  PTRACE(3, "SIP\t" << (startImmediate ? "Start" : "Queue") << "ing re-INVITE to " << msg);

  m_needReINVITE = true;

  SIPTransaction * invite = new SIPInvite(*this);

  // To avoid overlapping INVITE transactions, we place the new transaction
  // in a queue, if queue is empty we can start immediately, otherwise
  // it waits till we get a response.
  if (startImmediate) {
    if (!invite->Start())
      return false;
    m_handlingINVITE = true;
  }

  m_pendingInvitations.Append(invite);
  return true;
}


bool SIPConnection::StartPendingReINVITE()
{
  while (!m_pendingInvitations.IsEmpty()) {
    PSafePtr<SIPTransaction> reInvite = m_pendingInvitations.GetAt(0, PSafeReadWrite);
    // reInvite can be NULL because SIPEndPoint::GarbageCollection()
    // can mark transactions as removed
    if (reInvite != NULL) {
      if (reInvite->IsInProgress())
        break;

      if (!reInvite->IsCompleted()) {
        if (reInvite->Start()) {
          m_handlingINVITE = true;
          return true;
        }
      }
    }

    m_pendingInvitations.RemoveAt(0);
  }

  return false;
}


void SIPConnection::WriteINVITE(OpalTransport & transport, bool & succeeded)
{
  m_dialog.SetForking(GetInterface().IsEmpty());

  SIPURL myAddress = m_stringOptions(OPAL_OPT_CALLING_PARTY_URL);
  if (myAddress.IsEmpty())
    myAddress = GetEndPoint().GetDefaultLocalURL(transport);

  PString transportProtocol = m_dialog.GetRequestURI().GetTransportProto();
  if (!transportProtocol.IsEmpty())
    myAddress.SetParamVar("transport", transportProtocol);

  bool changedUserName = false;
  if (IsOriginating()) {
    // only allow override of calling party number if the local party
    // name hasn't been first specified by a register handler. i.e a
    // register handler's target number is always used
    changedUserName = m_stringOptions.Contains(OPAL_OPT_CALLING_PARTY_NUMBER);
    if (changedUserName)
      myAddress.SetUserName(m_stringOptions[OPAL_OPT_CALLING_PARTY_NUMBER]);
    else {
      changedUserName = m_stringOptions.Contains(OPAL_OPT_CALLING_PARTY_NAME);
      if (changedUserName)
        myAddress.SetUserName(m_stringOptions[OPAL_OPT_CALLING_PARTY_NAME]);
    }
  }
  else {
    changedUserName = m_stringOptions.Contains(OPAL_OPT_CALLED_PARTY_NAME);
    if (changedUserName)
      myAddress.SetUserName(m_stringOptions[OPAL_OPT_CALLED_PARTY_NAME]);
  }

  bool changedDisplayName = myAddress.GetDisplayName() != GetDisplayName();
  if (changedDisplayName)
    myAddress.SetDisplayName(GetDisplayName());

  // Domain cannot be an empty string so do not set if override is empty
  {
    PString domain(m_stringOptions(OPAL_OPT_CALLING_PARTY_DOMAIN));
    if (!domain.IsEmpty())
      myAddress.SetHostName(domain);
  }

  // Tag must be set to token or the whole house of cards falls down
  myAddress.SetTag(GetToken(), true);
  m_dialog.SetLocalURI(myAddress);

  NotifyDialogState(SIPDialogNotification::Trying);

  m_needReINVITE = false;
  SIPTransaction * invite = new SIPInvite(*this, &transport);

  if (!m_stringOptions.Contains(SIP_HEADER_CONTACT) && (changedUserName || changedDisplayName)) {
    SIPMIMEInfo & mime = invite->GetMIME();
    SIPURL contact = mime.GetContact();
    if (changedUserName)
      contact.SetUserName(myAddress.GetUserName());
    if (changedDisplayName)
      contact.SetDisplayName(myAddress.GetDisplayName());
    mime.SetContact(contact.AsQuotedString());
  }

  // It may happen that constructing the INVITE causes the connection
  // to be released (e.g. there are no UDP ports available for the RTP sessions)
  // Since the connection is released immediately, a INVITE must not be
  // sent out.
  if (IsReleased()) {
    PTRACE(2, "SIP\tAborting INVITE transaction since connection is in releasing phase");
    delete invite; // Before Start() is called we are responsible for deletion
    return;
  }

  if (invite->Start()) {
    m_forkedInvitations.Append(invite);
    succeeded = true;
  }
}


void SIPConnection::OnCreatingINVITE(SIPInvite & request)
{
  PTRACE(3, "SIP\tCreating INVITE request");

  SIPMIMEInfo & mime = request.GetMIME();

  request.SetAllow(GetAllowedMethods());
  mime.SetAllowEvents(m_allowedEvents);

  switch (m_prackMode) {
    case e_prackDisabled :
      break;

    case e_prackRequired :
      mime.AddRequire("100rel");
      // Then add supported as well

    case e_prackSupported :
      mime.AddSupported("100rel");
  }

  mime.AddSupported("replaces");
  for (PStringToString::iterator it = m_stringOptions.begin(); it != m_stringOptions.end(); ++it) {
    PCaselessString key = it->first;
    if (key.NumCompare(HeaderPrefix) == EqualTo) {
      PString data = it->second;
      if (!data.IsEmpty()) {
        mime.SetAt(key.Mid(HeaderPrefix.GetLength()), data);
        if (key == SIP_HEADER_REPLACES)
          mime.AddRequire("replaces");
      }
    }
  }

  if (IsPresentationBlocked()) {
    // Should do more as per RFC3323, but this is all for now
    SIPURL from = mime.GetFrom();
    if (!from.GetDisplayName(false).IsEmpty())
      from.SetDisplayName("Anonymous");
    from.Sanitise(SIPURL::FromURI);
    mime.SetFrom(from);
  }

  SIPURL redir(m_stringOptions(OPAL_OPT_REDIRECTING_PARTY, m_redirectingParty));
  if (!redir.IsEmpty())
    mime.SetReferredBy(redir.AsQuotedString());

  mime.SetAlertInfo(m_alertInfo, m_appearanceCode);

  PString externalSDP = m_stringOptions(OPAL_OPT_EXTERNAL_SDP);
  if (!externalSDP.IsEmpty())
    request.SetEntityBody(externalSDP);
  else if ((m_needReINVITE || m_stringOptions.GetBoolean(OPAL_OPT_INITIAL_OFFER, true)) && !m_localMediaFormats.IsEmpty()) {
    if (m_needReINVITE)
      ++m_sdpVersion;

    PString oldInterface = GetInterface();

    PSafePtr<OpalTransport> transport = request.GetTransport();
    if (transport != NULL) {
      PString newInterface = transport->GetInterface();
      if (newInterface.IsEmpty())
        newInterface = transport->GetLocalAddress().GetHostName();
      m_dialog.SetInterface(newInterface);
    }

    SDPSessionDescription * sdp = GetEndPoint().CreateSDP(m_sdpSessionId, m_sdpVersion, OpalTransportAddress());
    if (OnSendOfferSDP(*sdp, m_needReINVITE)) {
      if (m_needReINVITE)
        request.m_sessions = m_sessions;
      else
        request.m_sessions.MoveFrom(m_sessions);
      request.SetSDP(sdp);
    }
    else {
      delete sdp;
      Release(EndedByCapabilityExchange);
    }
    m_dialog.SetInterface(oldInterface);
  }
}


PBoolean SIPConnection::SetUpConnection()
{
  PTRACE(3, "SIP\tSetUpConnection: " << m_dialog.GetRequestURI());

  InternalSetAsOriginating();

  OnApplyStringOptions();

  if (m_stringOptions.Contains(SIP_HEADER_PREFIX"Route")) {
    SIPMIMEInfo mime;
    mime.SetRoute(m_stringOptions[SIP_HEADER_PREFIX"Route"]);
    m_dialog.SetRouteSet(mime.GetRoute());
  }

  ++m_sdpVersion;

  m_remoteFormatList = GetLocalMediaFormats();
  m_remoteFormatList.MakeUnique();
  AdjustMediaFormats(false, NULL, m_remoteFormatList);

  PSafePtr<OpalConnection> other = GetOtherPartyConnection();
  if (other != NULL && other->GetConferenceState(NULL))
    m_allowedEvents += SIPSubscribe::EventPackage(SIPSubscribe::Conference);

  SIP_PDU::StatusCodes reason = StartTransaction(PCREATE_NOTIFIER(WriteINVITE));

  // If we user entered sip:user rather than sip:address, then try again using default registrar
  if (reason == SIP_PDU::Local_BadTransportAddress && m_dialog.GetRequestURI().GetUserName().IsEmpty()) {
    PStringList registrations = GetEndPoint().GetRegistrations();
    if (!registrations.IsEmpty()) {
      SIPURL newDestination(registrations.front());
      newDestination.SetUserName(m_dialog.GetRequestURI().GetHostName());
      m_dialog.SetRequestURI(newDestination);
      m_dialog.SetRemoteURI(newDestination);
      reason = StartTransaction(PCREATE_NOTIFIER(WriteINVITE));
    }
  }

  SetPhase(SetUpPhase);

  if (reason != SIP_PDU::Successful_OK) {
    Release(EndedByTransportFail);
    return false;
  }

  m_dialog.SetForking(false);
  releaseMethod = ReleaseWithCANCEL;
  m_handlingINVITE = true;
  return true;
}


PString SIPConnection::GetRemoteIdentity() const
{
  if (!m_remoteIdentity.IsEmpty())
    return m_remoteIdentity;

  if (!m_ciscoRemotePartyID.IsEmpty())
    return m_ciscoRemotePartyID;

  return m_remotePartyURL;
}


PString SIPConnection::GetDestinationAddress()
{
  return m_lastReceivedINVITE != NULL ? m_lastReceivedINVITE->GetURI().AsString() : OpalConnection::GetDestinationAddress();
}


PString SIPConnection::GetCalledPartyURL()
{
  if (!IsOriginating() && m_lastReceivedINVITE != NULL)
    return m_lastReceivedINVITE->GetURI().AsString();

  SIPURL calledParty = m_dialog.GetRequestURI();
  calledParty.Sanitise(SIPURL::ExternalURI);
  return calledParty.AsString();
}


PString SIPConnection::GetAlertingType() const
{
  return m_alertInfo;
}


bool SIPConnection::SetAlertingType(const PString & info)
{
  m_alertInfo = info;
  return true;
}


PString SIPConnection::GetCallInfo() const
{
  return m_lastReceivedINVITE != NULL ? m_lastReceivedINVITE->GetMIME().GetCallInfo() : PString::Empty();
}


bool SIPConnection::OnHoldStateChanged(bool PTRACE_PARAM(placeOnHold))
{
  return SendReINVITE(PTRACE_PARAM(placeOnHold ? "put connection on hold" : "retrieve connection from hold"));
}


PString SIPConnection::GetPrefixName() const
{
  return m_dialog.GetRequestURI().GetScheme();
}


PString SIPConnection::GetIdentifier() const
{
  return m_dialog.GetCallID();
}


void SIPConnection::OnTransactionFailed(SIPTransaction & transaction)
{
  SIPTransactionOwner::OnTransactionFailed(transaction);

  std::map<std::string, SIP_PDU *>::iterator it = m_responses.find(transaction.GetTransactionID());
  if (it != m_responses.end()) {
    it->second->SetStatusCode(transaction.GetStatusCode());
    m_responses.erase(it);
  }

  switch (transaction.GetMethod()) {
    case SIP_PDU::Method_INVITE :
      break;

    case SIP_PDU::Method_REFER :
      m_referInProgress = false;
      // Do next case

    default :
      return;
  }

  m_handlingINVITE = false;

  // If we are releasing then I can safely ignore failed
  // transactions - otherwise I'll deadlock.
  if (IsReleased())
    return;

  PTRACE(4, "SIP\tChecking for all forked INVITEs failing.");
  bool allFailed = true;
  {
    // The connection stays alive unless all INVITEs have failed
    PSafePtr<SIPTransaction> invitation(m_forkedInvitations, PSafeReference);
    while (invitation != NULL) {
      if (invitation == &transaction)
        m_forkedInvitations.Remove(invitation++);
      else {
        if (!invitation->IsFailed())
          allFailed = false;
        ++invitation;
      }
    }
  }

  // All invitations failed, die now, with correct code
  if (allFailed && GetPhase() < ConnectedPhase)
    Release(GetCallEndReasonFromResponse(transaction));
  else {
    switch (m_holdToRemote) {
      case eHoldInProgress :
        PTRACE(4, "SIP\tHold request failed on " << *this);
        m_holdToRemote = eHoldOff;  // Did not go into hold
        OnHold(false, false);   // Signal the manager that there is no more hold
        break;

      case eRetrieveInProgress :
        PTRACE(4, "SIP\tRetrieve request failed on " << *this);
        m_holdToRemote = eHoldOn;  // Did not go out of hold
        OnHold(false, true);   // Signal the manager that hold is still active
        break;

      default :
        break;
    }
  }
}


void SIPConnection::OnReceivedPDU(SIP_PDU & pdu)
{
  SIP_PDU::Methods method = pdu.GetMethod();

  PSafeLockReadWrite lock(*this);
  if (!lock.IsLocked())
    return;

  if (GetInterface().IsEmpty())
    m_dialog.SetInterface(pdu.GetTransport()->GetInterface());

  if (m_remoteAddress.IsEmpty())
    m_remoteAddress = pdu.GetTransport()->GetRemoteAddress();

  m_allowedMethods |= pdu.GetMIME().GetAllowBitMask();

  switch (method) {
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
    case SIP_PDU::Method_NOTIFY :
      OnReceivedNOTIFY(pdu);
      break;
    case SIP_PDU::Method_REFER :
      OnReceivedREFER(pdu);
      break;
    case SIP_PDU::Method_INFO :
      OnReceivedINFO(pdu);
      break;
    case SIP_PDU::Method_PING :
      OnReceivedPING(pdu);
      break;
    case SIP_PDU::Method_PRACK :
      OnReceivedPRACK(pdu);
      break;
    case SIP_PDU::Method_MESSAGE :
      OnReceivedMESSAGE(pdu);
      break;
    case SIP_PDU::Method_SUBSCRIBE :
      OnReceivedSUBSCRIBE(pdu);
      break;
    default :
      // Shouldn't have got this!
      PTRACE(2, "SIP\tUnhandled PDU " << pdu);
      SIP_PDU response(pdu, SIP_PDU::Failure_MethodNotAllowed);
      response.SetAllow(GetEndPoint().GetAllowedMethods()); // Required by spec
      response.Send();
      break;
  }
}


bool SIPConnection::OnReceivedResponseToINVITE(SIPTransaction & transaction, SIP_PDU & response)
{
  unsigned statusCode = response.GetStatusCode();
  if (statusCode >= 300)
    return true;

  PSafeLockReadWrite lock(*this);
  if (!lock.IsLocked())
    return true;

  // Still doing delayed ACK, don't send one yet
  if (m_delayedAckInviteResponse != NULL) {
    // But handle case where we got a 180/183 and then we get the 200
    if (m_delayedAckInviteResponse->GetStatusCode() >= 200 || response.GetStatusCode() < 200)
      return false;
    delete m_delayedAckInviteResponse;
    m_delayedAckInviteResponse = NULL;
  }

  // Have a delayed ACK (probably with media) so send this one
  if (m_delayedAckPDU != NULL && m_delayedAckPDU->GetTransactionID() == transaction.GetTransactionID()) {
    m_delayedAckPDU->Send();
    return false;
  }

  // See if duplicate response because our ACK was lost or too slow.
  if (transaction.IsCompleted())
    return true;

  // See if this is an initial INVITE or a re-INVITE
  bool reInvite = true;
  for (PSafePtr<SIPTransaction> invitation(m_forkedInvitations, PSafeReference); invitation != NULL; ++invitation) {
    if (invitation == &transaction) {
      reInvite = false;
      break;
    }
  }

  // If we are in a dialog, then m_dialog needs to be updated in the 2xx/1xx
  // response for a target refresh request
  m_dialog.Update(response);
  response.DecodeSDP(GetEndPoint(), GetLocalMediaFormats());

  const SIPMIMEInfo & responseMIME = response.GetMIME();

  m_remoteIdentity = responseMIME.GetPAssertedIdentity();

  {
    SIPURL newRemotePartyID(responseMIME, RemotePartyID);
    if (!newRemotePartyID.IsEmpty()) {
      if (m_ciscoRemotePartyID.IsEmpty() && newRemotePartyID.GetUserName() == m_dialog.GetRemoteURI().GetUserName()) {
        PTRACE(3, "SIP\tOld style Remote-Party-ID set to \"" << newRemotePartyID << '"');
        m_ciscoRemotePartyID = newRemotePartyID;
      }
      else if (m_ciscoRemotePartyID != newRemotePartyID) {
        PTRACE(3, "SIP\tOld style Remote-Party-ID used for forwarding indication to \"" << newRemotePartyID << '"');

        m_ciscoRemotePartyID = newRemotePartyID;
        newRemotePartyID.SetParameters(PString::Empty());

        PStringToString info = m_ciscoRemotePartyID.GetParamVars();
        info.SetAt("result", "forwarded");
        info.SetAt("party", "A");
        info.SetAt("code", psprintf("%u", statusCode));
        info.SetAt("Referred-By", m_dialog.GetRemoteURI().AsString());
        info.SetAt("Remote-Party", newRemotePartyID.AsString());
        OnTransferNotify(info, this);
      }
    }
  }

  // Update internal variables on remote part names/number/address
  UpdateRemoteAddresses();

  if (reInvite)
    return statusCode >= 200;

  bool collapseForks;
  if (statusCode < 200)
    collapseForks = false;
  else {
    collapseForks = true;
    // If response was 2xx we need to release with a BYE from now on.
    // If response was an error, then we need no nothing, it is already dead
    releaseMethod = statusCode < 300 ? ReleaseWithBYE : ReleaseWithNothing;
  }

  responseMIME.GetProductInfo(remoteProductInfo);

  SDPSessionDescription * sdp = response.GetSDP();
  if (sdp != NULL) {
    // Strictly getting media in a 1xx should not collapse the forking, in
    // practice, it is too hard to deal with so we abandon all the other paths
    // which will work 99.99% of scenarios.
    collapseForks = true;

    if (remoteProductInfo.vendor.IsEmpty() && remoteProductInfo.name.IsEmpty()) {
      if (sdp->GetSessionName() != "-")
        remoteProductInfo.name = sdp->GetSessionName();
      if (sdp->GetUserName() != "-")
        remoteProductInfo.vendor = sdp->GetUserName();
    }
  }
  else if (!response.GetEntityBody().IsEmpty()) {
    /* If we had SDP but no media could not be decoded from it, then we should return
       Not Acceptable Here error and not do an offer. Only offer if there was no body
       at all or there was a valid SDP with no m lines. */
    Release(EndedByCapabilityExchange);
    return true;
  }

  if (collapseForks) {
    // Save the sessions we are actually using out of all the forked INVITES sent
    SessionMap & sessionsInTransaction = ((SIPInvite &)transaction).m_sessions;
    if (m_sessions.IsEmpty() && !sessionsInTransaction.IsEmpty())
      m_sessions.MoveFrom(sessionsInTransaction);

    // Have a positive response to the INVITE, so cancel all the other invitations sent.
    for (PSafePtr<SIPTransaction> invitation(m_forkedInvitations, PSafeReference); invitation != NULL; ++invitation) {
      if (invitation != &transaction)
        invitation->Cancel();
    }

    // And end connect mode on the transport
    m_dialog.SetInterface(transaction.GetInterface());
    m_contactAddress = transaction.GetMIME().GetContact();
  }

  if (statusCode < 200) {
    // Do PRACK after all the dialog completion parts above.
    if (responseMIME.GetRequire().Contains("100rel")) {
      PString rseq = responseMIME.GetString("RSeq");
      if (rseq.IsEmpty()) {
        PTRACE(2, "SIP\tReliable (100rel) response has no RSeq field.");
      }
      else if (rseq.AsUnsigned() <= m_prackSequenceNumber) {
        PTRACE(3, "SIP\tDuplicate response " << response.GetStatusCode() << ", already PRACK'ed");
      }
      else if (transaction.IsCanceled()) {
        PTRACE(3, "SIP\tNo PRACK on cancelled transaction");
      }
      else {
        SIPTransaction * prack = new SIPPrack(*this, *transaction.GetTransport(), rseq & transaction.GetMIME().GetCSeq());
        prack->Start();
      }
    }
  }
  else {
    // If got here collapseForks was true and all the other INVITE transactions are
    // removed. This gets rid of the last one that returned the terminal response.
    m_forkedInvitations.RemoveAll();
  }

  // Check for if we did an empty INVITE offer
  if (sdp == NULL || transaction.GetSDP() != NULL)
    return statusCode >= 200; // Don't send ACK if only provisional response

  // Empty INVITE means offer in remotes response, get the media formats out of it
  if (SetRemoteMediaFormats(response) <= 0) {
    Release(EndedByCapabilityExchange);
    return true;
  }

  /* As we did an empty INVITE, we now have the remotes capabilities on their 200
     and we can flow though to OnReceivedOK, and it does an OnConnected
     causing the the OpalCall (and other connection) to do media selection and
     start media streams.
     But all that is in the future, so we need to save the 200 OK until the
     above has happened and we can actually send the ACK with SDP. */

  m_delayedAckInviteResponse = new SIP_PDU(response);

  if (SendDelayedACK(false))
    return false; // Don't send ACK ... already sent!

  PTRACE(3, "SIP\tSaving INVITE response " << response.GetStatusCode() << " for delayed ACK, timeout=" << m_delayedAckTimeout1);
  m_delayedAckTimer = m_delayedAckTimeout1;
  return false; // Don't send ACK ... yet
}


void SIPConnection::OnDelayedAckTimeout()
{
  PTRACE(4, "SIP\tDelayed ACK timeout");
  if (!SendDelayedACK(true))
    m_delayedAckTimer = m_delayedAckTimeout2; // Try again, can only be we haven't got 200 OK yet
}


bool SIPConnection::SendDelayedACK(bool force)
{
  if (m_delayedAckInviteResponse == NULL || m_delayedAckPDU != NULL)
    return true;

  if (m_delayedAckInviteResponse->GetStatusCode() < 200)
    return false;

  if (!force) {
    PSafePtr<OpalConnection> otherConnection = GetOtherPartyConnection();
    if (otherConnection == NULL)
      return false; // Not sure how we could have got here.

    OpalMediaTypeList mediaTypes = otherConnection->GetMediaFormats().GetMediaTypes();
    if (mediaTypes.empty()) {
      PTRACE(4, "SIP\tDelayed ACK does not have any channels yet");
      return false;
    }

    for (OpalMediaTypeList::iterator mediaType = mediaTypes.begin(); mediaType != mediaTypes.end(); ++mediaType) {
      for (OpalMediaType::AutoStartMode mode = OpalMediaType::Receive; mode <= OpalMediaType::Transmit; ++mode) {
        if (GetAutoStart(*mediaType) & mode) {
          OpalMediaStreamPtr stream = GetMediaStream(*mediaType, mode == OpalMediaType::Receive);
          if (stream == NULL) {
            PTRACE(4, "SIP\tDelayed ACK does not have " << *mediaType << ' ' << mode << " channel yet");
            return false;
          }

          OpalMediaSession * session = GetMediaSession(stream->GetSessionID());
          if (session == NULL) {
            PTRACE(4, "SIP\tDelayed ACK does not have " << *mediaType << " session " << stream->GetSessionID() << " yet");
            return false;
          }

          if (session->IsOpen()) {
            PTRACE(4, "SIP\tDelayed ACK does not have " << *mediaType << " session " << stream->GetSessionID() << " open yet");
            return false;
          }
        }
      }
    }
  }

  PSafePtr<SIPTransaction> transaction = GetEndPoint().GetTransaction(m_delayedAckInviteResponse->GetTransactionID(), PSafeReadOnly);
  if (transaction == NULL) {
    PTRACE(3, "SIP\tDelayed ACK failed, could not find transaction");
    Release(EndedByCapabilityExchange);
  }
  else {
    PTRACE(3, "SIP\tCreating delayed ACK via " << (force ? "timeout" : "media opened"));
    // ACK constructed following 13.2.2.4 or 17.1.1.3
    m_delayedAckPDU = new SIPAck(*transaction, *m_delayedAckInviteResponse);

    SDPSessionDescription * sdp = GetEndPoint().CreateSDP(m_sdpSessionId, ++m_sdpVersion, GetDefaultSDPConnectAddress());
    sdp->SetSessionName(m_delayedAckInviteResponse->GetMIME().GetUserAgent());
    m_delayedAckPDU->SetSDP(sdp);

    m_delayedAckInviteResponse->DecodeSDP(GetEndPoint(), GetLocalMediaFormats());
    if (OnSendAnswerSDP(*m_delayedAckInviteResponse->GetSDP(), *sdp)) {
      if (m_delayedAckPDU->Send())
        StartMediaStreams();
      else
        Release(EndedByTransportFail);
    }
    else {
      Release(EndedByCapabilityExchange);
      m_delayedAckPDU->SetSDP(NULL);
      m_delayedAckPDU->Send();
    }
  }

  delete m_delayedAckInviteResponse;
  m_delayedAckInviteResponse = NULL;
  m_handlingINVITE = false;
  return true;
}


void SIPConnection::UpdateRemoteAddresses()
{
  SIPURL remote = m_ciscoRemotePartyID;
  if (remote.IsEmpty()) {
    remote = m_dialog.GetRemoteURI();
    remote.Sanitise(SIPURL::ExternalURI);
  }
  remotePartyName = remote.GetDisplayName();

  remotePartyNumber = remote.GetUserName();
  if (!OpalIsE164(remotePartyNumber))
    remotePartyNumber.MakeEmpty();

  remotePartyName = remote.GetDisplayName();
  if (remotePartyName.IsEmpty())
    remotePartyName = remotePartyNumber.IsEmpty() ? remote.GetUserName() : remote.AsString();
  m_remotePartyURL = remote.AsString();

  // If no local name, then use what the remote thinks we are
  if (localPartyName.IsEmpty())
    localPartyName = m_dialog.GetLocalURI().GetUserName();

  ownerCall.SetPartyNames();
}


#if OPAL_SRTP
bool SIPConnection::CanDoSRTP() const
{
  return m_stringOptions.GetBoolean(OPAL_OPT_UNSECURE_SRTP) ||
         m_dialog.GetRemoteTransportAddress(m_dnsEntry).GetProtoPrefix() == OpalTransportAddress::TlsPrefix();
}
#endif

void SIPConnection::NotifyDialogState(SIPDialogNotification::States state, SIPDialogNotification::Events eventType, unsigned eventCode)
{
  if (GetPhase() == EstablishedPhase)
    return; // Don't notify for re-INVITEs

  SIPURL url = m_dialog.GetLocalURI();
  url.Sanitise(SIPURL::ExternalURI);

  SIPDialogNotification info(url.AsString());

  info.m_dialogId = m_dialogNotifyId.AsString();
  info.m_callId = m_dialog.GetCallID();

  info.m_local.m_URI = url.AsString();
  info.m_local.m_dialogTag  = m_dialog.GetLocalTag();
  info.m_local.m_identity = url.AsString();
  info.m_local.m_display = url.GetDisplayName();
  info.m_local.m_appearance = m_appearanceCode;

  url = m_dialog.GetRemoteURI();
  url.Sanitise(SIPURL::ExternalURI);

  info.m_remote.m_URI = m_dialog.GetRequestURI().AsString();
  info.m_remote.m_dialogTag = m_dialog.GetRemoteTag();
  info.m_remote.m_identity = url.AsString();
  info.m_remote.m_display = url.GetDisplayName();

  if (!info.m_remote.m_dialogTag.IsEmpty() && state == SIPDialogNotification::Proceeding)
    state = SIPDialogNotification::Early;

  info.m_initiator = IsOriginating();
  info.m_state = state;
  info.m_eventType = eventType;
  info.m_eventCode = eventCode;

  if (GetPhase() == EstablishedPhase)
    info.m_local.m_rendering = info.m_remote.m_rendering = SIPDialogNotification::NotRenderingMedia;

  for (OpalMediaStreamPtr mediaStream(mediaStreams, PSafeReference); mediaStream != NULL; ++mediaStream) {
    if (mediaStream->IsSource())
      info.m_remote.m_rendering = SIPDialogNotification::RenderingMedia;
    else
      info.m_local.m_rendering = SIPDialogNotification::RenderingMedia;
  }

  GetEndPoint().SendNotifyDialogInfo(info);
}


void SIPConnection::OnReceivedResponse(SIPTransaction & transaction, SIP_PDU & response)
{
  SIPTransactionOwner::OnReceivedResponse(transaction, response);

  // One of forks got there too
  if (response.GetStatusCode() == SIP_PDU::Failure_LoopDetected && !m_transactions.IsEmpty())
    return;

  unsigned responseClass = response.GetStatusCode()/100;

  PSafeLockReadWrite lock(*this);
  if (!lock.IsLocked())
    return;

  if (m_remoteAddress.IsEmpty()) {
    OpalTransportPtr transport = transaction.GetTransport();
    if (transport != NULL)
      m_remoteAddress = transport->GetRemoteAddress();
  }

  m_allowedMethods |= response.GetMIME().GetAllowBitMask();

  if (transaction.GetMethod() != SIP_PDU::Method_INVITE) {
    switch (response.GetStatusCode()) {
      case SIP_PDU::Failure_UnAuthorised :
      case SIP_PDU::Failure_ProxyAuthenticationRequired :
        if (OnReceivedAuthenticationRequired(transaction, response))
          return;

      default :
        switch (responseClass) {
          case 1 : // Treat all other provisional responses like a Trying.
            OnReceivedTrying(transaction, response);
            return;

          case 2 : // Successful response - there really is only 200 OK
            OnReceivedOK(transaction, response);
            break;

          default :
            switch (transaction.GetMethod()) {
              case SIP_PDU::Method_REFER :
                if (m_referInProgress) {
                  m_referInProgress = false;

                  PStringToString info;
                  info.SetAt("result", "error");
                  info.SetAt("party", "B");
                  info.SetAt("code", psprintf("%u", response.GetStatusCode()));
                  OnTransferNotify(info, this);
                }
                break;

#if OPAL_VIDEO
              case SIP_PDU::Method_INFO :
                if (transaction.GetMIME().GetContentType().NumCompare(ApplicationMediaControlXMLKey) == EqualTo) {
                  PTRACE(3, "SIP\tError response to video fast update INFO, not sending another.");
                  m_canDoVideoFastUpdateINFO = false;
                }
                break;
#endif // OPAL_VIDEO

              default :
                break;
            }
        }
    }

    std::map<std::string, SIP_PDU *>::iterator it = m_responses.find(transaction.GetTransactionID());
    if (it != m_responses.end()) {
      *it->second = response;
      m_responses.erase(it);
    }

    return;
  }

  const SIPMIMEInfo & responseMIME = response.GetMIME();

  // If they ignore our "Require" header, we have to kill the call ourselves
  if (m_prackMode == e_prackRequired &&
      responseClass == 1 &&
      response.GetStatusCode() != SIP_PDU::Information_Trying &&
      !responseMIME.GetRequire().Contains("100rel")) {
    Release(EndedBySecurityDenial);
    return;
  }

  // Don't pass on retries of reliable provisional responses.
  {
    PString snStr = responseMIME.GetString("RSeq");
    if (!snStr.IsEmpty()) {
      unsigned sn = snStr.AsUnsigned();
      if (sn <= m_prackSequenceNumber)
        return;
      m_prackSequenceNumber = sn;
    }
  }

  if (GetPhase() < EstablishedPhase) {
    PString referToken = m_stringOptions(OPAL_SIP_REFERRED_CONNECTION);
    if (!referToken.IsEmpty()) {
      PSafePtr<SIPConnection> referred = GetEndPoint().GetSIPConnectionWithLock(referToken, PSafeReadOnly);
      if (referred != NULL) {
        (new SIPReferNotify(*referred, response.GetStatusCode()))->Start();

        if (response.GetStatusCode() >= 300) {
          PTRACE(3, "SIP\tFailed to transfer " << *referred);
          referred->SetPhase(EstablishedPhase); // Go back to established

          PStringToString info;
          info.SetAt("result", "failed");
          info.SetAt("party", "A");
          info.SetAt("code", psprintf("%u", response.GetStatusCode()));
          OnTransferNotify(info, this);
        }
        else if (response.GetStatusCode() >= 200) {
          if (m_allowedEvents.Contains(SIPSubscribe::EventPackage(SIPSubscribe::Conference))) {
            PTRACE(3, "SIP\tCompleted conference invite from " << *referred);
          }
          else {
            PTRACE(3, "SIP\tCompleted transfer of " << *referred);
            referred->Release(OpalConnection::EndedByCallForwarded);
          }

          PStringToString info;
          info.SetAt("result", "success");
          info.SetAt("party", "A");
          OnTransferNotify(info, this);
        }
      }
    }
  }

  // For 180/183 may have some SDP that can be used as early remote media capbility
  if (responseClass == 1)
    SetRemoteMediaFormats(response);

  bool handled = false;

  // Break out to virtual functions for some special cases.
  switch (response.GetStatusCode()) {
    case SIP_PDU::Information_Ringing :
      OnReceivedRinging(transaction, response);
      return;

    case SIP_PDU::Information_Session_Progress :
      OnReceivedSessionProgress(transaction, response);
      return;

    case SIP_PDU::Failure_UnAuthorised :
    case SIP_PDU::Failure_ProxyAuthenticationRequired :
      if (OnReceivedAuthenticationRequired(transaction, response))
        return;
      break;

    case SIP_PDU::Failure_MessageTooLarge :
      switch (SwitchTransportProto("tcp", NULL)) {
        case SIP_PDU::Local_NotAuthenticated :
          Release(EndedByCertificateAuthority);
          return;

        case SIP_PDU::Local_BadTransportAddress :
        case SIP_PDU::Local_CannotMapScheme :
          Release(EndedByIllegalAddress);
          return;

        case SIP_PDU::Local_TransportLost :
          Release(EndedByTransportFail);
          return;

        default :
          Release(EndedByUnreachable);
          return;

        case SIP_PDU::Successful_OK :
          SIPTransaction * newTransaction = transaction.CreateDuplicate();
          if (!newTransaction->Start()) {
            PTRACE(2, "SIP\tCould not restart " << transaction << " for switch to TCP");
            break;
          }

          m_forkedInvitations.Append(newTransaction);
      }
      return;

    case SIP_PDU::Failure_RequestPending :
      m_inviteCollisionTimer = (IsOriginating() ? PRandom::Number(2100, 4000) : PRandom::Number(1, 2000));
      return;

    default :
      switch (responseClass) {
        case 1 : // Treat all other provisional responses like a Trying.
          OnReceivedTrying(transaction, response);
          return;

        case 2 : // Successful response - there really is only 200 OK
          OnReceivedOK(transaction, response);
          handled = true;
          break;

        case 3 : // Redirection response
          OnReceivedRedirection(response);
          handled = true;
          break;
      }
  }

  if (m_delayedAckInviteResponse == NULL)
    m_handlingINVITE = false;

  // To avoid overlapping INVITE transactions, wait till here before
  // starting the next one.
  if (responseClass != 1) {
    m_pendingInvitations.Remove(&transaction);
    StartPendingReINVITE();
  }

#if OPAL_T38_CAPABILITY
  if (ownerCall.IsSwitchingT38()) {
    SDPSessionDescription * sdp = response.GetSDP();
    bool isT38 = sdp != NULL && sdp->GetMediaDescriptionByType(OpalMediaType::Fax()) != NULL;
    bool toT38 = ownerCall.IsSwitchingToT38();
    OnSwitchedFaxMediaStreams(toT38, isT38 == toT38);
  }
#endif // OPAL_T38_CAPABILITY

  if (handled)
    return;

  // If we are doing a local hold, and it failed, we do not release the connection
  switch (m_holdToRemote) {
    case eHoldInProgress :
      PTRACE(4, "SIP\tHold request failed on " << *this);
      m_holdToRemote = eHoldOff;  // Did not go into hold
      OnHold(false, false);   // Signal the manager that there is no more hold
      break;

    case eRetrieveInProgress :
      PTRACE(4, "SIP\tRetrieve request failed on " << *this);
      m_holdToRemote = eHoldOn;  // Did not go out of hold
      OnHold(false, true);   // Signal the manager that hold is still active
      break;

    default :
      break;
  }

  // Is a re-INVITE if in here, so don't kill the call becuase it failed.
  if (GetPhase() == EstablishedPhase)
    return;

  // We don't always release the connection, eg not till all forked invites have completed
  // This INVITE is from a different "dialog", any errors do not cause a release

  if (GetPhase() < ConnectedPhase) {
    // Final check to see if we have forked INVITEs still running, don't
    // release connection until all of them have failed.
    for (PSafePtr<SIPTransaction> invitation(m_forkedInvitations, PSafeReference); invitation != NULL; ++invitation) {
      if (invitation->IsProceeding())
        return;
      // If we have not even got a 1xx from the remote for this forked INVITE,
      // don't keep waiting, cancel it and take the error we got
      if (invitation->IsTrying())
        invitation->Cancel();
    }
  }

  // All other responses are errors, set Q931 code if available
  releaseMethod = ReleaseWithNothing;
  Release(GetCallEndReasonFromResponse(response));
}


SIPConnection::TypeOfINVITE SIPConnection::CheckINVITE(const SIP_PDU & request) const
{
  const SIPMIMEInfo & requestMIME = request.GetMIME();
  PString requestFromTag = requestMIME.GetFieldParameter("From", "tag");
  PString requestToTag   = requestMIME.GetFieldParameter("To",   "tag");

  // Criteria for our existing dialog.
  if (!requestToTag.IsEmpty() &&
       m_dialog.GetCallID() == requestMIME.GetCallID() &&
       m_dialog.GetRemoteTag() == requestFromTag &&
       m_dialog.GetLocalTag() == requestToTag)
    return IsReINVITE;

  if (IsOriginating()) {
    /* Weird, got incoming INVITE for a call we originated, however it is not in
       the same dialog. Send back a refusal as this just isn't handled. */
    PTRACE(2, "SIP\tIgnoring INVITE from " << request.GetURI() << " when originated call.");
    return IsLoopedINVITE;
  }

  // No original INVITE, so we are in a race condition at start up of the
  // connection, assume a duplicate so it is ignored. If it does turn out
  // to be new INVITE, then it should be retried and next time the race
  // will be passed.
  if (m_lastReceivedINVITE == NULL) {
    PTRACE(3, "SIP\tIgnoring INVITE from " << request.GetURI() << " as we are originator.");
    return IsDuplicateINVITE;
  }

  /* If we have same transaction ID, it means it is a retransmission
     of the original INVITE, probably should re-transmit last sent response
     but we just ignore it. Still should work. */
  if (m_lastReceivedINVITE->GetTransactionID() == request.GetTransactionID()) {
    PTRACE(3, "SIP\tIgnoring duplicate INVITE from " << request.GetURI() << " after " << (PTime() - m_phaseTime[SetUpPhase]));
    return IsDuplicateINVITE;
  }

  // Check if is RFC3261/8.2.2.2 case relating to merged requests.
  if (!requestToTag.IsEmpty()) {
    PTRACE(3, "SIP\tIgnoring INVITE from " << request.GetURI() << " as has invalid to-tag.");
    return IsDuplicateINVITE;
  }

  // More checks for RFC3261/8.2.2.2 case relating to merged requests.
  if (m_dialog.GetRemoteTag() != requestFromTag ||
      m_dialog.GetCallID() != requestMIME.GetCallID() ||
      m_lastReceivedINVITE->GetMIME().GetCSeq() != requestMIME.GetCSeq() ||
      request.GetTransactionID().NumCompare("z9hG4bK") != EqualTo) // Or RFC2543
    return IsNewINVITE; // No it isn't

  /* This is either a merged request or a brand new "dialog" to the same call.
     Probably indicates forking request or request arriving via multiple IP
     paths. In both cases we refuse the INVITE. The first because we don't
     support multiple dialogs on one call, the second because the RFC says
     we should! */
  PTRACE(3, "SIP\tIgnoring forked INVITE from " << request.GetURI());
  return IsLoopedINVITE;
}


void SIPConnection::OnReceivedINVITE(SIP_PDU & request)
{
  bool isReinvite = IsOriginating() ||
        (m_lastReceivedINVITE != NULL && m_lastReceivedINVITE->GetTransactionID() != request.GetTransactionID());
  PTRACE_IF(4, !isReinvite, "SIP\tInitial INVITE to " << request.GetURI());

  // m_lastReceivedINVITE should contain the last received INVITE for this connection
  delete m_lastReceivedINVITE;
  m_lastReceivedINVITE = new SIP_PDU(request);

  SIPMIMEInfo & mime = m_lastReceivedINVITE->GetMIME();

  m_remoteIdentity = mime.GetPAssertedIdentity();

  // update the dialog context
  m_dialog.SetLocalTag(GetToken());
  m_dialog.Update(request);

  // We received a Re-INVITE for a current connection
  if (isReinvite) {
    m_lastReceivedINVITE->DecodeSDP(GetEndPoint(), GetLocalMediaFormats());
    OnReceivedReINVITE(request);
    return;
  }

  m_ciscoRemotePartyID = SIPURL(mime, RemotePartyID);
  PTRACE_IF(4, !m_ciscoRemotePartyID.IsEmpty(),
            "SIP\tOld style Remote-Party-ID set to \"" << m_ciscoRemotePartyID << '"');

  UpdateRemoteAddresses();

  SetPhase(SetUpPhase);

  OnApplyStringOptions();

  NotifyDialogState(SIPDialogNotification::Trying);
  mime.GetAlertInfo(m_alertInfo, m_appearanceCode);

  // Fill in all the various connection info, note our to/from is their from/to
  mime.GetProductInfo(remoteProductInfo);

  m_contactAddress = request.GetURI();

  mime.SetTo(m_dialog.GetLocalURI());

  // get the called destination number and name
  m_calledPartyName = request.GetURI().GetUserName();
  if (!m_calledPartyName.IsEmpty() && OpalIsE164(m_calledPartyName)) {
    m_calledPartyNumber = m_calledPartyName;
    m_calledPartyName = request.GetURI().GetDisplayName(false);
  }

  m_redirectingParty = mime.GetReferredBy();
  PTRACE_IF(4, !m_redirectingParty.IsEmpty(),
            "SIP\tRedirecting party (Referred-By/Diversion) set to \"" << m_redirectingParty << '"');

  {
    // get the address that remote end *thinks* it is using from the Via field
    PString via = mime.GetFirstVia();
    if (!via.IsEmpty())
      DetermineRTPNAT(*request.GetTransport(), "ip$" + via(via.Find(' ') + 1, via.Find(':') - 1));
  }

  bool prackSupported = mime.GetSupported().Contains("100rel");
  bool prackRequired = mime.GetRequire().Contains("100rel");
  switch (m_prackMode) {
    case e_prackDisabled :
      if (prackRequired) {
        SIP_PDU response(request, SIP_PDU::Failure_BadExtension);
        response.GetMIME().SetUnsupported("100rel");
        response.Send();
        return;
      }
      // Ignore, if just supported
      break;

    case e_prackSupported :
      m_prackEnabled = prackSupported || prackRequired;
      break;

    case e_prackRequired :
      m_prackEnabled = prackSupported || prackRequired;
      if (!m_prackEnabled) {
        SIP_PDU response(request, SIP_PDU::Failure_ExtensionRequired);
        response.GetMIME().SetRequire("100rel");
        response.Send();
        return;
      }
  }

  releaseMethod = ReleaseWithResponse;
  m_handlingINVITE = true;

  // See if we have a replaces header, if not is normal call
  PString replaces = mime("Replaces");
  if (replaces.IsEmpty()) {
    // indicate the other is to start ringing (but look out for clear calls)
    if (!OnIncomingConnection(0, NULL)) {
      PTRACE(1, "SIP\tOnIncomingConnection failed for INVITE from " << request.GetURI() << " for " << *this);
      Release();
      return;
    }

    if (IsReleased())
      return;

    PTRACE(3, "SIP\tOnIncomingConnection succeeded for INVITE from " << request.GetURI() << " for " << *this);

    if (SetRemoteMediaFormats(*m_lastReceivedINVITE) < 0) {
      Release(EndedByCapabilityExchange);
      return;
    }

    if (m_lastReceivedINVITE->GetSDP() != NULL)
      DetermineRTPNAT(*request.GetTransport(), m_lastReceivedINVITE->GetSDP()->GetDefaultConnectAddress());

    if (ownerCall.OnSetUp(*this)) {
      if (IsReleased())
        return;

      if (GetPhase() < ProceedingPhase) {
        SetPhase(ProceedingPhase);
        OnProceeding();
      }

      AnsweringCall(OnAnswerCall(GetRemotePartyURL()));
      return;
    }

    PTRACE(1, "SIP\tOnSetUp failed for INVITE from " << request.GetURI() << " for " << *this);
    Release();
    return;
  }

  // Replaces header string has already been validated in SIPEndPoint::OnReceivedINVITE
  PSafePtr<SIPConnection> replacedConnection = GetEndPoint().GetSIPConnectionWithLock(replaces, PSafeReadOnly);
  if (replacedConnection == NULL) {
    /* Allow for a race condition where between when SIPEndPoint::OnReceivedINVITE()
       is executed and here, the call to be replaced was released. */
    Release(EndedByInvalidConferenceID);
    return;
  }

  if (replacedConnection->GetPhase() < ConnectedPhase) {
    if (!m_stringOptions.GetBoolean(OPAL_OPT_ALLOW_EARLY_REPLACE) && !replacedConnection->IsOriginating()) {
      PTRACE(3, "SIP\tEarly connection " << *replacedConnection << " cannot be replaced by " << *this);
      Release(EndedByInvalidConferenceID);
      return;
    }
  }
  else {
    if (replaces.Find(";early-only") != P_MAX_INDEX) {
      PTRACE(3, "SIP\tReplaces has early-only on early connection " << *this);
      Release(EndedByLocalBusy);
      return;
    }
  }

  if (SetRemoteMediaFormats(*m_lastReceivedINVITE) < 0) {
    Release(EndedByCapabilityExchange);
    return;
  }

  PTRACE(3, "SIP\tEstablished connection " << *replacedConnection << " replaced by " << *this);

  // Set forward party to call token so the SetConnected() that completes the
  // operation happens on the OnReleases() thread.
  replacedConnection->m_forwardParty = GetToken();
  replacedConnection->Release(OpalConnection::EndedByCallForwarded);

  // Check if we are the target of an attended transfer, indicated by a referred-by header
  if (!m_redirectingParty.IsEmpty()) {
    /* Indicate to application we are party C in a consultation transfer.
       The calls are A->B (first call), B->C (consultation call) => A->C
       (final call after the transfer) */
    PStringToString info = PURL(m_redirectingParty).GetParamVars();
    info.SetAt("result", "incoming");
    info.SetAt("party", "C");
    info.SetAt("Referred-By", m_redirectingParty);
    info.SetAt("Remote-Party", GetRemotePartyURL());
    OnTransferNotify(info, this);
  }
}


void SIPConnection::OnReceivedReINVITE(SIP_PDU & request)
{
  if (m_handlingINVITE || GetPhase() < ConnectedPhase) {
    PTRACE(2, "SIP\tRe-INVITE from " << request.GetURI() << " received while INVITE in progress on " << *this);
    request.SendResponse(SIP_PDU::Failure_RequestPending);
    return;
  }

  PTRACE(3, "SIP\tReceived re-INVITE from " << request.GetURI() << " for " << *this);

  m_needReINVITE = true;
  m_handlingINVITE = true;

  // send the 200 OK response
  if (OnSendAnswer(SIP_PDU::Successful_OK))
    StartMediaStreams();
  else
    SendInviteResponse(SIP_PDU::Failure_NotAcceptableHere);

  SIPURL newRemotePartyID(request.GetMIME(), RemotePartyID);
  if (newRemotePartyID.IsEmpty() || m_ciscoRemotePartyID == newRemotePartyID)
    UpdateRemoteAddresses();
  else {
    PTRACE(3, "SIP\tOld style Remote-Party-ID used for transfer indication to \"" << newRemotePartyID << '"');

    m_ciscoRemotePartyID = newRemotePartyID;
    newRemotePartyID.SetParameters(PString::Empty());
    UpdateRemoteAddresses();

    PStringToString info = m_ciscoRemotePartyID.GetParamVars();
    info.SetAt("result", "incoming");
    info.SetAt("party", "C");
    info.SetAt("Referred-By", m_dialog.GetRemoteURI().AsString());
    info.SetAt("Remote-Party", newRemotePartyID.AsString());
    OnTransferNotify(info, this);
  }
}


void SIPConnection::OnReceivedACK(SIP_PDU & ack)
{
  if (m_lastReceivedINVITE == NULL) {
    PTRACE(2, "SIP\tACK from " << ack.GetURI() << " received before INVITE!");
    return;
  }

  // Forked request
  PString origFromTag = m_lastReceivedINVITE->GetMIME().GetFieldParameter("From", "tag");
  PString origToTag   = m_lastReceivedINVITE->GetMIME().GetFieldParameter("To",   "tag");
  PString fromTag     = ack.GetMIME().GetFieldParameter("From", "tag");
  PString toTag       = ack.GetMIME().GetFieldParameter("To",   "tag");
  if (fromTag != origFromTag || (!toTag.IsEmpty() && (toTag != origToTag))) {
    PTRACE(3, "SIP\tACK received for forked INVITE from " << ack.GetURI());
    return;
  }

  PTRACE(3, "SIP\tACK received: " << GetPhase());

  m_responseFailTimer.Stop(false); // Asynchronous stop to avoid deadlock
  m_responseRetryTimer.Stop(false);

  // If got ACK, any pending responses are done, even if they are not acknowledged
  while (!m_responsePackets.empty())
    m_responsePackets.pop();

  if (ack.DecodeSDP(GetEndPoint(), GetLocalMediaFormats()) && !OnReceivedAnswer(ack, NULL)) {
    Release(EndedByCapabilityExchange);
    return;
  }

  m_handlingINVITE = false;

  if (GetPhase() == ConnectedPhase) {
    SetPhase(EstablishedPhase);
    OnEstablished();
  }

  StartPendingReINVITE();
}


void SIPConnection::OnReceivedOPTIONS(SIP_PDU & request)
{
  SIPTransaction * response = new SIPResponse(GetEndPoint(), request, SIP_PDU::Failure_UnsupportedMediaType);
  if (request.GetMIME().GetAccept().Find(OpalSDPEndPoint::ContentType()) != P_MAX_INDEX) {
    response->SetStatusCode(SIP_PDU::Successful_OK);
    response->SetAllow(GetAllowedMethods());
    response->SetSDP(GetEndPoint().CreateSDP(m_sdpSessionId, m_sdpVersion, OpalTransportAddress()));
  }
  response->Send();
}


void SIPConnection::OnAllowedEventNotify(const PString & /* eventStr */)
{
}


void SIPConnection::OnReceivedNOTIFY(SIP_PDU & request)
{
  /* Transfering a Call
     We have sent a REFER to the UA in this connection.
     Now we get Progress Indication of that REFER Request by NOTIFY Requests.
     Handle the response coded of "message/sipfrag" as follows:

     Use code >= 180  &&  code < 300 to Release this dialog.
     Use Subscription State = terminated to Release this dialog.
  */

  SIPTransaction * response = new SIPResponse(GetEndPoint(), request, SIP_PDU::Successful_OK);

  const SIPMIMEInfo & mime = request.GetMIME();

  SIPSubscribe::EventPackage package(mime.GetEvent());
  if (m_allowedEvents.Contains(package)) {
    PTRACE(2, "SIP\tReceived Notify for allowed event " << package);
    response->Send();
    OnAllowedEventNotify(package);
    return;
  }

  // Do not include the id parameter in this comparison, may need to
  // do it later if we ever support multiple simultaneous REFERs
  if (package.Find("refer") == P_MAX_INDEX) {
    PTRACE(2, "SIP\tNOTIFY in a connection only supported for REFER requests");
    response->SetStatusCode(SIP_PDU::Failure_BadEvent);
    response->Send();
    return;
  }

  if (!m_referInProgress) {
    PTRACE(2, "SIP\tNOTIFY for REFER we never sent.");
    response->SetStatusCode(SIP_PDU::Failure_TransactionDoesNotExist);
    response->Send();
    return;
  }

  if (mime.GetContentType() != "message/sipfrag") {
    PTRACE(2, "SIP\tNOTIFY for REFER has incorrect Content-Type");
    response->SetStatusCode(SIP_PDU::Failure_BadRequest);
    response->Send();
    return;
  }

  PCaselessString body = request.GetEntityBody();
  unsigned code = body.Mid(body.Find(' ')).AsUnsigned();
  if (body.NumCompare("SIP/") != EqualTo || code < 100) {
    PTRACE(2, "SIP\tNOTIFY for REFER has incorrect body");
    response->SetStatusCode(SIP_PDU::Failure_BadRequest);
    response->Send();
    return;
  }

  response->Send();

  PStringToString info;
  PCaselessString state = mime.GetSubscriptionState(info);
  m_referInProgress = state != "terminated";
  info.SetAt("party", "B"); // We are B party in consultation transfer
  info.SetAt("state", state);
  info.SetAt("code", psprintf("%u", code));
  info.SetAt("result", m_referInProgress ? "progress" : (code < 300 ? "success" : "failed"));

  if (OnTransferNotify(info, this))
    return;

  // Release the connection
  if (IsReleased())
    return;

  releaseMethod = ReleaseWithBYE;
  Release(OpalConnection::EndedByCallForwarded);
}


void SIPConnection::OnReceivedREFER(SIP_PDU & request)
{
  SIPTransaction * response = new SIPResponse(GetEndPoint(), request, SIP_PDU::Successful_OK);

  if (IsReleased()) {
    PTRACE(2, "SIP\tRejecting REFER as released " << *this);
    response->SetStatusCode(SIP_PDU::Failure_TransactionDoesNotExist);
    response->Send();
    return;
  }

  const SIPMIMEInfo & requestMIME = request.GetMIME();

  SIPURL referTo = requestMIME.GetReferTo();
  if (referTo.IsEmpty()) {
    response->SetStatusCode(SIP_PDU::Failure_BadRequest);
    response->SetInfo("Missing refer-to header");
    response->Send();
    return;
  }    

  // Comply to RFC4488
  bool referSub = true;
  static PConstCaselessString const ReferSubHeader("Refer-Sub");
  if (requestMIME.Contains(ReferSubHeader)) {
    referSub = requestMIME.GetBoolean(ReferSubHeader, true);
    response->GetMIME().SetBoolean(ReferSubHeader, referSub);
  }

  if (referSub) {
    response->SetStatusCode(SIP_PDU::Successful_Accepted);
    referTo.SetParamVar(OPAL_URL_PARAM_PREFIX OPAL_SIP_REFERRED_CONNECTION, GetToken());
  }

  // RFC4579
  PSafePtr<OpalConnection> other = GetOtherPartyConnection();
  if (other != NULL) {
    OpalConferenceState state;
    if (other->GetConferenceState(&state)) {
      PCaselessString method = referTo.GetParamVars()("method", "INVITE");
      if (method == "INVITE") {
        // Don't actually transfer, get conf to do INVITE to add participant
        if (!InviteConferenceParticipant(state.m_internalURI, referTo.AsString()))
          response->SetStatusCode(SIP_PDU::Failure_NotFound);
      }
      else if (method == "BYE") {
      }
      else
        response->SetStatusCode(SIP_PDU::Failure_NotImplemented);

      response->Send();
      return;
    }
  }

  // send response before attempting the transfer
  if (!response->Send())
    return;

  m_redirectingParty = requestMIME.GetReferredBy();
  if (m_redirectingParty.IsEmpty()) {
    SIPURL from = requestMIME.GetFrom();
    from.Sanitise(SIPURL::ExternalURI);
    m_redirectingParty = from.AsString();
  }

  PStringToString info = PURL(m_redirectingParty).GetParamVars();
  info.SetAt("result", "started");
  info.SetAt("party", "A");
  info.SetAt("Referred-By", m_redirectingParty);
  OnTransferNotify(info, this);

  PString replaces = referTo.GetQueryVars()("Replaces");
  referTo.SetQuery(PString::Empty());

  // send NOTIFY if transfer failed, but only if allowed by RFC4488
  if (!GetEndPoint().SetupTransfer(GetToken(), replaces, referTo.AsQuotedString(), NULL) && referSub)
    (new SIPReferNotify(*this, SIP_PDU::GlobalFailure_Decline))->Start();
}


bool SIPConnection::InviteConferenceParticipant(const PString & conf, const PString & dest)
{
  return endpoint.GetManager().SetUpCall(conf, dest) != NULL;
}


void SIPConnection::OnReceivedBYE(SIP_PDU & request)
{
  PTRACE(3, "SIP\tBYE received for call " << request.GetMIME().GetCallID());

  SIPTransaction * response = new SIPResponse(GetEndPoint(), request, SIP_PDU::Successful_OK);
  response->Send();
  
  if (IsReleased()) {
    PTRACE(2, "SIP\tAlready released " << *this);
    return;
  }
  releaseMethod = ReleaseWithNothing;

  m_dialog.Update(request);
  UpdateRemoteAddresses();
  request.GetMIME().GetProductInfo(remoteProductInfo);

  Release(EndedByRemoteUser);
}


void SIPConnection::OnReceivedCANCEL(SIP_PDU & request)
{
  // Currently only handle CANCEL requests for the original INVITE that
  // created this connection, all else ignored

  if (m_lastReceivedINVITE == NULL || m_lastReceivedINVITE->GetTransactionID() != request.GetTransactionID()) {
    PTRACE(2, "SIP\tUnattached " << request << " received for " << *this);
    request.SendResponse(SIP_PDU::Failure_TransactionDoesNotExist);
    return;
  }

  if (IsReleased()) {
    PTRACE(2, "SIP\tCANCEL for Already released " << *this);
    return;
  }

  PTRACE(3, "SIP\tCANCEL received for " << *this);

  SIPTransaction * response = new SIPResponse(GetEndPoint(), request, SIP_PDU::Successful_OK);
  response->GetMIME().SetTo(m_dialog.GetLocalURI());
  response->Send();

  if (IsOriginating())
    return;

  CallEndReason endReason = EndedByCallerAbort;
  PString strReason = request.GetMIME().GetString("Reason");
  if (!strReason.IsEmpty()) {
    static const char cause[] = "cause=";
    PINDEX pos = strReason.Find(cause);
    if (pos != P_MAX_INDEX) {
      pos += sizeof(cause)-1;
      if (strReason(pos, strReason.Find(';', pos)-1) == "200")
        endReason = EndedByCallCompletedElsewhere;
    }
  }

  Release(endReason);
}


void SIPConnection::OnReceivedTrying(SIPTransaction & transaction, SIP_PDU & /*response*/)
{
  if (transaction.GetMethod() != SIP_PDU::Method_INVITE)
    return;

  PTRACE(3, "SIP\tReceived Trying response");
  NotifyDialogState(SIPDialogNotification::Proceeding);

  if (GetPhase() < ProceedingPhase) {
    SetPhase(ProceedingPhase);
    OnProceeding();
  }
}


PString SIPConnection::GetAuthID() const
{
  return m_dialog.GetLocalURI().GetUserName();
}


void SIPConnection::OnStartTransaction(SIPTransaction & transaction)
{
  GetEndPoint().OnStartTransaction(*this, transaction);
}


void SIPConnection::OnReceivedRinging(SIPTransaction & transaction, SIP_PDU & response)
{
  PTRACE(3, "SIP\tReceived Ringing response");
  OnReceivedAlertingResponse(transaction, response);
}


void SIPConnection::OnReceivedSessionProgress(SIPTransaction & transaction, SIP_PDU & response)
{
  PTRACE(3, "SIP\tReceived Session Progress response");
  OnReceivedAlertingResponse(transaction, response);
}


void SIPConnection::OnReceivedAlertingResponse(SIPTransaction & transaction, SIP_PDU & response)
{
  response.GetMIME().GetAlertInfo(m_alertInfo, m_appearanceCode);

  bool withMedia = OnReceivedAnswer(response, &transaction);

  if (GetPhase() < AlertingPhase) {
    SetPhase(AlertingPhase);
    OnAlerting(withMedia);
    NotifyDialogState(SIPDialogNotification::Early);
  }
}


void SIPConnection::OnReceivedRedirection(SIP_PDU & response)
{
  SIPURL whereTo = response.GetMIME().GetContact();
  for (PStringOptions::iterator it = m_stringOptions.begin(); it != m_stringOptions.end(); ++it)
    whereTo.SetParamVar(OPAL_URL_PARAM_PREFIX + it->first, it->second);
  PTRACE(3, "SIP\tReceived redirect to " << whereTo);
  GetEndPoint().ForwardConnection(*this, whereTo.AsString());
}


PBoolean SIPConnection::OnReceivedAuthenticationRequired(SIPTransaction & transaction, SIP_PDU & response)
{
  if (IsReleased()) {
    PTRACE(2, "SIP\tNo authentication retry as released " << *this);
    return false;
  }

  // Try to find authentication parameters for the given realm,
  // if not, use the proxy authentication parameters (if any)
  SIP_PDU::StatusCodes status = HandleAuthentication(response);
  if (status != SIP_PDU::Successful_OK)
    return false;

  SIPTransaction * newTransaction = transaction.CreateDuplicate();
  if (newTransaction == NULL) {
    PTRACE(1, "SIP\tCannot create duplicate transaction for " << transaction);
    return false;
  }

  if (!newTransaction->Start()) {
    PTRACE(2, "SIP\tCould not restart " << transaction);
    return false;
  }

  if (transaction.GetMethod() == SIP_PDU::Method_INVITE)
    m_forkedInvitations.Append(newTransaction);
  else {
    std::map<std::string, SIP_PDU *>::iterator it = m_responses.find(transaction.GetTransactionID());
    if (it != m_responses.end()) {
      m_responses[newTransaction->GetTransactionID()] = it->second;
      m_responses.erase(it);
    }
  }

  return true;
}


void SIPConnection::OnReceivedOK(SIPTransaction & transaction, SIP_PDU & response)
{
  switch (transaction.GetMethod()) {
    case SIP_PDU::Method_INVITE :
      if (IsReleased()) {
        PTRACE(2, "SIP\tNot processing INVITE OK as released " << *this);
        return;
      }
      break;

    case SIP_PDU::Method_MESSAGE :
#if OPAL_HAS_SIPIM
      OpalSIPIMContext::OnMESSAGECompleted(GetEndPoint(),
                                           dynamic_cast<SIPMessage &>(transaction).GetParameters(),
                                           response.GetStatusCode());
#endif
     return;

    case SIP_PDU::Method_REFER :
      if (m_referInProgress && !response.GetMIME().GetBoolean("Refer-Sub", true)) {
        // Used RFC4488 to indicate we are NOT doing NOTIFYs, release now
        PTRACE(3, "SIP\tBlind transfer accepted, without NOTIFY so ending local call.");
        m_referInProgress = false;

        PStringToString info;
        info.SetAt("result", "blind");
        info.SetAt("party", "B");
        OnTransferNotify(info, this);

        Release(OpalConnection::EndedByCallForwarded);
      }
      // Do next case

    default :
      return;
  }

  PTRACE(3, "SIP\tReceived INVITE OK response for " << transaction.GetMethod());
  m_sessionTimer = 10000;

  NotifyDialogState(SIPDialogNotification::Confirmed);

  if (GetPhase() >= ConnectedPhase)
    OnReceivedAnswer(response, &transaction);  // Re-INVITE
  else {
    // Don't use OnConnectedInternal() as need to process SDP between setting connected
    // state locally and other half of call processing SetConnected()
    SetPhase(ConnectedPhase);

    if (!OnReceivedAnswer(response, &transaction)) {
      if (mediaStreams.IsEmpty())
        Release(EndedByCapabilityExchange);
      return;
    }

    OnConnected();

    if (!mediaStreams.IsEmpty() && GetPhase() == ConnectedPhase) {
      SetPhase(EstablishedPhase);
      OnEstablished();
    }
  }

  switch (m_holdToRemote) {
    case eHoldInProgress :
      m_holdToRemote = eHoldOn;
      OnHold(false, true);   // Signal the manager that they are on hold
      break;

    case eRetrieveInProgress :
      m_holdToRemote = eHoldOff;
      OnHold(false, false);   // Signal the manager that there is no more hold
      break;

    default :
      break;
  }
}


bool SIPConnection::OnSendAnswer(SIP_PDU::StatusCodes response)
{
  if (!PAssert(m_lastReceivedINVITE != NULL, PLogicError))
    return false;

  SDPSessionDescription * sdp = m_lastReceivedINVITE->GetSDP();

  /* If we had SDP but no media could not be decoded from it, then we should return
     Not Acceptable Here error and not do an offer. Only offer if there was no body
     at all or there was a valid SDP with no m lines. */
  if (sdp == NULL && !m_lastReceivedINVITE->GetEntityBody().IsEmpty())
    return false;

  std::auto_ptr<SDPSessionDescription> sdpOut(GetEndPoint().CreateSDP(m_sdpSessionId, ++m_sdpVersion, GetDefaultSDPConnectAddress()));
  bool ok;

  if (sdp == NULL || sdp->GetMediaDescriptions().IsEmpty()) {
    // They did not offer anything, so it behooves us to do so: RFC 3261, para 14.2
    PTRACE(3, "SIP\tRemote did not offer media, so we will.");
    ok = OnSendOfferSDP(*sdpOut, false);
  }
  else {
    // The Re-INVITE can be sent to change the RTP Session parameters,
    // the current codecs, or to put the call on hold
    bool holdFromRemote = sdp->IsHold();
    if (m_holdFromRemote != holdFromRemote) {
      PTRACE(3, "SIP\tRemote " << (holdFromRemote ? "" : "retrieve from ") << "hold detected");
      m_holdFromRemote = holdFromRemote;
      OnHold(true, holdFromRemote);
    }

    ok = OnSendAnswerSDP(*sdp, *sdpOut);
  }

  m_answerFormatList.RemoveAll();
  return ok && SendInviteResponse(response, sdpOut.get());
}


bool SIPConnection::OnReceivedAnswer(SIP_PDU & response, SIPTransaction * transaction)
{
  SDPSessionDescription * sdp = response.GetSDP();
  if (sdp == NULL) {
    PTRACE(5, "SIP", "Response has no SDP");
    return false;
  }

  if (transaction != NULL && transaction->GetSDP() == NULL) {
    PTRACE(4, "SIP", "No local offer made, processing remote offer in delayed ACK");
    if (!SendDelayedACK(false))
      m_delayedAckTimer = m_delayedAckTimeout1;
    return true;
  }

  m_answerFormatList = sdp->GetMediaFormats();
  AdjustMediaFormats(false, NULL, m_answerFormatList);

  if (transaction == NULL || transaction->GetSDP()->HasActiveSend()) {
    bool holdFromRemote = sdp->IsHold();
    if (m_holdFromRemote != holdFromRemote) {
      PTRACE(3, "SIP\tRemote " << (holdFromRemote ? "" : "retrieve from ") << "hold detected");
      m_holdFromRemote = holdFromRemote;
      OnHold(true, holdFromRemote);
    }
  }

  bool multipleFormats = false;
  if (!OnReceivedAnswerSDP(*sdp,multipleFormats))
    return false;

  /* See if remote has answered our offer with multiple possible codecs.
     While this is perfectly legal, and we are supposed to wait for the first
     RTP packet to arrive before setting up codecs etc, our architecture
     cannot deal with that. So what we do is immediately, send a re-INVITE
     nailing the codec down to the first reply. */
  if (multipleFormats && m_resolveMultipleFormatReINVITE && response.GetStatusCode()/100 == 2) {
    m_resolveMultipleFormatReINVITE= false;
    SendReINVITE(PTRACE_PARAM("resolve multiple codecs in answer"));
  }

  return true;
}


PString SIPConnection::GetMediaInterface()
{
  return SIPTransactionOwner::GetInterface();
}


OpalTransportAddress SIPConnection::GetRemoteMediaAddress()
{
  return OpalTransportAddress(m_dialog.GetRemoteTransportAddress(m_dnsEntry).GetHostName(), 0, OpalTransportAddress::UdpPrefix());
}


PBoolean SIPConnection::ForwardCall (const PString & fwdParty)
{
  if (fwdParty.IsEmpty ())
    return false;


  m_forwardParty = fwdParty;
  PTRACE(2, "SIP\tIncoming SIP connection will be forwarded to " << m_forwardParty);
  Release(EndedByCallForwarded);

  return true;
}


PBoolean SIPConnection::SendInviteResponse(SIP_PDU::StatusCodes code,
                                           const SDPSessionDescription * sdp)
{
  if (m_lastReceivedINVITE == NULL)
    return true;

  SIP_PDU response(*m_lastReceivedINVITE, code, sdp);
  AdjustInviteResponse(response);

  if (sdp != NULL)
    response.GetSDP()->SetSessionName(response.GetMIME().GetUserAgent());

  return response.Send(); 
}


void SIPConnection::AdjustInviteResponse(SIP_PDU & response)
{
  SIPMIMEInfo & mime = response.GetMIME();
  mime.SetProductInfo(GetEndPoint().GetUserAgent(), GetProductInfo());
  response.SetAllow(GetAllowedMethods());

  GetEndPoint().AdjustToRegistration(response, this, m_lastReceivedINVITE->GetTransport());

  if (!m_ciscoRemotePartyID.IsEmpty()) {
    SIPURL party(mime.GetContact());
    party.GetFieldParameters().RemoveAll();
    mime.Set(RemotePartyID, party.AsQuotedString());
  }

  // this can be used to promote any incoming calls to TCP. Not quite there yet, but it *almost* works
  bool promoteToTCP = false;    // disable code for now
  if (promoteToTCP && response.GetTransport()->GetProtoPrefix() == OpalTransportAddress::TcpPrefix()) {
    // see if endpoint contains a TCP listener we can use
    OpalTransportAddress newAddr;
    if (endpoint.FindListenerForProtocol(OpalTransportAddress::TcpPrefix(), newAddr)) {
      response.GetMIME().SetContact(SIPURL(PString::Empty(), newAddr, 0).AsQuotedString());
      PTRACE(3, "SIP\tPromoting connection to TCP");
    }
  }

  // AdjustToRegistration already did a check for GetConferenceState, this is quicker ...
  if (mime.GetContact().GetParamVars().Contains("isfocus"))
    m_allowedEvents += SIPSubscribe::EventPackage(SIPSubscribe::Conference);

  if (response.GetStatusCode() > 100 && response.GetStatusCode() < 300)
    mime.SetAllowEvents(m_allowedEvents);

  if (response.GetStatusCode() == SIP_PDU::Information_Ringing)
    mime.SetAlertInfo(m_alertInfo, m_appearanceCode);

  if (response.GetStatusCode() >= 200) {
    // If sending final response, any pending unsent responses no longer need to be sent.
    // But the last sent response still needs to be acknowledged, so leave it in the queue.
    while (m_responsePackets.size() > 1)
      m_responsePackets.pop();

    m_responsePackets.push(response);
  }
  else if (m_prackEnabled) {
    mime.AddRequire("100rel");

    if (m_prackSequenceNumber == 0)
      m_prackSequenceNumber = PRandom::Number(0x40000000); // as per RFC 3262
    mime.SetAt("RSeq", PString(PString::Unsigned, ++m_prackSequenceNumber));

    m_responsePackets.push(response);
  }

  switch (m_responsePackets.size()) {
    case 0 :
      break;

    case 1 :
      m_responseRetryCount = 0;
      m_responseRetryTimer = GetEndPoint().GetRetryTimeoutMin();
      m_responseFailTimer = GetEndPoint().GetAckTimeout();
      break;
  }
}


void SIPConnection::OnInviteResponseRetry()
{
  if (m_lastReceivedINVITE == NULL || m_responsePackets.empty())
    return;

  PTRACE(3, "SIP\t" << (m_responsePackets.front().GetStatusCode() < 200 ? "PRACK" : "ACK")
         << " not received yet, retry " << m_responseRetryCount << " sending response for " << *this);

  PTimeInterval timeout = GetEndPoint().GetRetryTimeoutMin()*(1 << ++m_responseRetryCount);
  if (timeout > GetEndPoint().GetRetryTimeoutMax())
    timeout = GetEndPoint().GetRetryTimeoutMax();
  m_responseRetryTimer = timeout;

  m_responsePackets.front().Send();
}


void SIPConnection::OnInviteResponseTimeout()
{
  if (m_responsePackets.empty())
    return;

  PTRACE(1, "SIP\tFailed to receive "
         << (m_responsePackets.front().GetStatusCode() < 200 ? "PRACK" : "ACK")
         << " for " << *this);

  m_responseRetryTimer.Stop(false);

  if (IsReleased()) {
    // Clear out pending responses if we are releasing, just die now.
    while (!m_responsePackets.empty())
      m_responsePackets.pop();
  }
  else {
    if (m_responsePackets.front().GetStatusCode() < 200)
      SendInviteResponse(SIP_PDU::Failure_ServerTimeout);
    else {
      releaseMethod = ReleaseWithBYE;
      Release(EndedByTemporaryFailure);
    }
  }
}


void SIPConnection::OnReceivedPRACK(SIP_PDU & request)
{
  PStringArray rack = request.GetMIME().GetString("RAck").Tokenise(" \r\n\t", false);
  if (rack.GetSize() != 3) {
    request.SendResponse(SIP_PDU::Failure_BadRequest);
    return;
  }

  if (m_lastReceivedINVITE == NULL ||
      m_lastReceivedINVITE->GetMIME().GetCSeqIndex() != rack[1].AsUnsigned() ||
      !(rack[2] *= "INVITE") ||
      m_responsePackets.empty() ||
      m_responsePackets.front().GetMIME().GetString("RSeq").AsUnsigned() != rack[0].AsUnsigned()) {
    request.SendResponse(SIP_PDU::Failure_TransactionDoesNotExist);
    return;
  }

  m_responseFailTimer.Stop(false); // Asynchronous stop to avoid deadlock
  m_responseRetryTimer.Stop(false);

  request.SendResponse(SIP_PDU::Successful_OK);

  // Got PRACK for our response, pop it off and send next, if on
  m_responsePackets.pop();

  if (!m_responsePackets.empty()) {
    m_responseRetryCount = 0;
    m_responseRetryTimer = GetEndPoint().GetRetryTimeoutMin();
    m_responseFailTimer = GetEndPoint().GetAckTimeout();
    m_responsePackets.front().Send();
  }

  if (request.DecodeSDP(GetEndPoint(), GetLocalMediaFormats()))
    OnReceivedAnswer(request, NULL);
}


void SIPConnection::OnUserInputInlineRFC2833(OpalRFC2833Info & info, INT type)
{
  switch (m_receivedUserInputMethod) {
    case ReceivedINFO :
      PTRACE(3, "OpalCon\tUsing INFO, ignoring RFC2833 on " << *this);
      break;

    case UserInputMethodUnknown :
      m_receivedUserInputMethod = ReceivedRFC2833;
      // Do default case

    default:
      OpalRTPConnection::OnUserInputInlineRFC2833(info, type);
  }
}


void SIPConnection::OnReceivedINFO(SIP_PDU & request)
{
  SIP_PDU::StatusCodes status = SIP_PDU::Failure_UnsupportedMediaType;
  SIPMIMEInfo & mimeInfo = request.GetMIME();
  PCaselessString contentType = mimeInfo.GetContentType();

  if (contentType.NumCompare(ApplicationDTMFRelayKey) == EqualTo) {
    switch (m_receivedUserInputMethod) {
      case ReceivedRFC2833 :
        PTRACE(3, "OpalCon\tUsing RFC2833, ignoring INFO " << ApplicationDTMFRelayKey << " on " << *this);
        break;

      case UserInputMethodUnknown :
        m_receivedUserInputMethod = ReceivedINFO;
        // Do default case

      default:
        PStringArray lines = request.GetEntityBody().Lines();
        PINDEX i;
        char tone = 0;
        int duration = -1;
        for (i = 0; i < lines.GetSize(); ++i) {
          PStringArray tokens = lines[i].Tokenise('=', false);
          PString val;
          if (tokens.GetSize() > 1)
            val = tokens[1].Trim();
          if (tokens.GetSize() > 0) {
            if (tokens[0] *= "signal")
              tone = val[0];   // DTMF relay does not use RFC2833 encoding
            else if (tokens[0] *= "duration")
              duration = val.AsInteger();
          }
        }
        if (tone != 0)
          OnUserInputTone(tone, duration == 0 ? 100 : duration);
        status = SIP_PDU::Successful_OK;
        break;
    }
  }

  else if (contentType.NumCompare(ApplicationDTMFKey) == EqualTo) {
    switch (m_receivedUserInputMethod) {
      case ReceivedRFC2833 :
        PTRACE(3, "OpalCon\tUsing RFC2833, ignoring INFO " << ApplicationDTMFKey << " on " << *this);
        break;

      case UserInputMethodUnknown :
        m_receivedUserInputMethod = ReceivedINFO;
        // Do default case

      default:
        PString tones = request.GetEntityBody().Trim();
        if (tones.GetLength() == 1)
          OnUserInputTone(tones[0], 100);
        else
          OnUserInputString(tones);
        status = SIP_PDU::Successful_OK;
    }
  }

#if OPAL_VIDEO
  else if (contentType.NumCompare(ApplicationMediaControlXMLKey) == EqualTo) {
    if (OnMediaControlXML(request))
      return;
  }
#endif

  request.SendResponse(status);

  if (status == SIP_PDU::Successful_OK) {
#if OPAL_PTLIB_DTMF
    // Have INFO user input, disable the in-band tone detcetor to avoid double detection
    m_detectInBandDTMF = false;
    OpalMediaStreamPtr stream = GetMediaStream(OpalMediaType::Audio(), true);
    if (stream != NULL && stream->RemoveFilter(m_dtmfDetectNotifier, OPAL_PCM16)) {
      PTRACE(4, "OpalCon\tRemoved detect DTMF filter on connection " << *this);
    }
#endif
  }
}


void SIPConnection::OnReceivedPING(SIP_PDU & request)
{
  PTRACE(3, "SIP\tReceived PING");
  request.SendResponse(SIP_PDU::Successful_OK);
}


void SIPConnection::OnReceivedMESSAGE(SIP_PDU & request)
{
  PTRACE(3, "SIP\tReceived MESSAGE in the context of a call");

#if OPAL_HAS_SIPIM
  OpalSIPIMContext::OnReceivedMESSAGE(GetEndPoint(), this, request);
#else
  request.SendResponse(SIP_PDU::Failure_BadRequest);
#endif
}


void SIPConnection::OnReceivedSUBSCRIBE(SIP_PDU & request)
{
  SIPSubscribe::EventPackage eventPackage(request.GetMIME().GetEvent());

  PTRACE(3, "SIP\tReceived SUBSCRIBE for " << eventPackage);

  if (m_allowedEvents.Contains(eventPackage))
    GetEndPoint().OnReceivedSUBSCRIBE(request, &m_dialog);
  else {
    SIP_PDU response(request, SIP_PDU::Failure_BadEvent);
    response.GetMIME().SetAllowEvents(m_allowedEvents); // Required by spec
    response.Send();
  }
}


OpalConnection::SendUserInputModes SIPConnection::GetRealSendUserInputMode() const
{
  switch (sendUserInputMode) {
    case SendUserInputAsProtocolDefault :
    case SendUserInputAsRFC2833 :
      if (m_remoteFormatList.HasFormat(OpalRFC2833))
        return SendUserInputAsRFC2833;
      PTRACE(3, "SIP\tSendUserInputMode for RFC2833 requested, but unavailable at remote.");
      return SendUserInputAsString;

    case NumSendUserInputModes :
    case SendUserInputAsQ931 :
      return SendUserInputAsTone;

    case SendUserInputAsString :
    case SendUserInputAsTone :
    case SendUserInputInBand :
      break;
  }

  return sendUserInputMode;
}


PBoolean SIPConnection::SendUserInputString(const PString & value)
{
  if (GetRealSendUserInputMode() == SendUserInputAsString) {
    SIPInfo::Params params;
    params.m_contentType = ApplicationDTMFKey;
    params.m_body = value;
    if (SendINFO(params))
      return true;
  }

  return OpalRTPConnection::SendUserInputString(value);
}


PBoolean SIPConnection::SendUserInputTone(char tone, unsigned duration)
{
  if (m_holdFromRemote || m_holdToRemote >= eHoldOn)
    return false;

  SendUserInputModes mode = GetRealSendUserInputMode();

  PTRACE(3, "SIP\tSendUserInputTone('" << tone << "', " << duration << "), using mode " << mode);

  SIPInfo::Params params;

  switch (mode) {
    case SendUserInputAsTone :
      {
        params.m_contentType = ApplicationDTMFRelayKey;
        PStringStream strm;
        strm << "Signal= " << tone << "\r\n" << "Duration= " << duration << "\r\n";  // spaces are important. Who can guess why?
        params.m_body = strm;
      }
      break;

    case SendUserInputAsString :
      params.m_contentType = ApplicationDTMFKey;
      params.m_body = tone;
      break;

    default :
      return OpalRTPConnection::SendUserInputTone(tone, duration);
  }

  if (SendINFO(params))
    return true;

  PTRACE(2, "SIP\tCould not send tone '" << tone << "' via INFO.");
  return OpalRTPConnection::SendUserInputTone(tone, duration);
}


bool SIPConnection::SendOPTIONS(const SIPOptions::Params & params, SIP_PDU * reply)
{
  if (m_handlingINVITE) {
    PTRACE(2, "SIP\tCan't send OPTIONS message while handling INVITE.");
    return false;
  }

  if ((m_allowedMethods&(1<<SIP_PDU::Method_OPTIONS)) == 0) {
    PTRACE(2, "SIP\tRemote does not allow OPTIONS message.");
    return false;
  }

  if (IsReleased()) {
    PTRACE(2, "SIP\tCan't send OPTIONS message while releasing call.");
    return false;
  }

  PSafePtr<SIPTransaction> transaction = new SIPOptions(*this, params);
  if (reply == NULL)
    return transaction->Start();

  m_responses[transaction->GetTransactionID()] = reply;
  transaction->WaitForCompletion();
  return !transaction->IsFailed();
}


bool SIPConnection::SendINFO(const SIPInfo::Params & params, SIP_PDU * reply)
{
  if (m_handlingINVITE) {
    PTRACE(2, "SIP\tCan't send INFO message while handling INVITE.");
    return false;
  }

  if ((m_allowedMethods&(1<<SIP_PDU::Method_INFO)) == 0) {
    PTRACE(2, "SIP\tRemote does not allow INFO message.");
    return false;
  }

  if (IsReleased()) {
    PTRACE(2, "SIP\tCan't send INFO message while releasing call.");
    return false;
  }

  PSafePtr<SIPTransaction> transaction = new SIPInfo(*this, params);
  if (reply == NULL)
    return transaction->Start();

  m_responses[transaction->GetTransactionID()] = reply;
  transaction->WaitForCompletion();
  return !transaction->IsFailed();
}


bool SIPConnection::OnMediaCommand(OpalMediaStream & stream, const OpalMediaCommand & command)
{
  PTRACE(5, "SIP\tOnMediaCommand \"" << command << "\" for " << *this);

  bool done = OpalRTPConnection::OnMediaCommand(stream, command);

#if OPAL_VIDEO
  if (m_canDoVideoFastUpdateINFO && PIsDescendant(&command, OpalVideoUpdatePicture) && (stream.IsSource() == (&stream.GetConnection() == this))) {
    if ((m_stringOptions.GetInteger(OPAL_OPT_VIDUP_METHODS, OPAL_OPT_VIDUP_METHOD_DEFAULT)&OPAL_OPT_VIDUP_METHOD_OOB) == 0) {
      PTRACE(5, "RTPCon\tINFO picture_fast_update disabled in string options");
    }
    else {
      SIPInfo::Params params(ApplicationMediaControlXMLKey,
                             "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
                             "<media_control>"
                              "<vc_primitive>"
                               "<to_encoder>"
                                "<picture_fast_update>"
                                "</picture_fast_update>"
                               "</to_encoder>"
                              "</vc_primitive>"
                             "</media_control>");
      if (SendINFO(params))
        done = true;
      else
        m_canDoVideoFastUpdateINFO = false;
    }
  }
#endif

  return done;
}


#if OPAL_VIDEO

PBoolean SIPConnection::OnMediaControlXML(SIP_PDU & request)
{
  // Must always send OK, even if not OK
  request.SendResponse(SIP_PDU::Successful_OK);

  // Ignore empty body
  PCaselessString body = request.GetEntityBody();
  if (body.IsEmpty())
    return true;

  bool ok = false;

#if OPAL_PTLIB_EXPAT

  PXML xml;
  if (xml.Load(body) && xml.GetDocumentType() == "media_control") {
    PXMLElement * element;
    if ((element = xml.GetElement("general_error")) != NULL) {
      PTRACE(2, "SIP\tReceived INFO message error from remote: \"" << element->GetData() << '"');
      return true;
    }

    element =  xml.GetRootElement();
    ok = (element = element->GetElement("vc_primitive"       )) != NULL &&
         (element = element->GetElement("to_encoder"         )) != NULL &&
                    element->GetElement("picture_fast_update")  != NULL;
  }

#else // OPAL_PTLIB_EXPAT

  if (body.Find("general_error") != P_MAX_INDEX)
    return true;

  ok = body.Find("media_control"      ) != P_MAX_INDEX &&
       body.Find("vc_primitive"       ) != P_MAX_INDEX &&
       body.Find("to_encoder"         ) != P_MAX_INDEX &&
       body.Find("picture_fast_update") != P_MAX_INDEX;

#endif // OPAL_PTLIB_EXPAT

  if (ok)
    SendVideoUpdatePicture(0, 0);
  else {
    PTRACE(3, "SIP\tUnable to parse received PictureFastUpdate");
    // Error is sent in separate INFO message as per RFC5168
    SIPInfo::Params params(ApplicationMediaControlXMLKey,
                           "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
                           "<media_control>"
                             "<general_error>"
                               "Unable to parse XML request"
                             "</general_error>"
                           "</media_control>");
    SendINFO(params);
  }

  return true;
}

#endif // OPAL_VIDEO


unsigned SIPConnection::GetAllowedMethods() const
{
  unsigned methods = GetEndPoint().GetAllowedMethods();
  if (GetPRACKMode() == e_prackDisabled)
    methods &= ~(1<<SIP_PDU::Method_PRACK);
  else
    methods |= (1<<SIP_PDU::Method_PRACK);
  return methods;
}


void SIPConnection::OnSessionTimeout()
{
  //SIPTransaction * invite = new SIPInvite(*this, GetTransport(), rtpSessions);  
  //invite->Start();  
  //sessionTimer = 10000;
}


PString SIPConnection::GetLocalPartyURL() const
{
  if (m_contactAddress.IsEmpty())
    return OpalRTPConnection::GetLocalPartyURL();

  SIPURL url = m_contactAddress;
  url.Sanitise(SIPURL::ExternalURI);
  return url.AsString();
}


#endif // OPAL_SIP


// End of file ////////////////////////////////////////////////////////////////
