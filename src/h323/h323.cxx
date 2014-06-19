/*
 * h323.cxx
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
 * $Revision$
 * $Author$
 * $Date$
 */

#include <ptlib.h>

#include <opal_config.h>
#if OPAL_H323

#ifdef __GNUC__
#pragma implementation "h323con.h"
#endif

#include <opal_config.h>

#include <h323/h323con.h>

#include <h323/h323ep.h>
#include <h323/h323neg.h>
#include <h323/h323rtp.h>
#include <h323/gkclient.h>

#if OPAL_H450
#include <h323/h450pdu.h>
#endif

#include <h323/transaddr.h>
#include <opal/call.h>
#include <opal/patch.h>
#include <codec/rfc2833.h>

#if OPAL_VIDEO
#include <codec/vidcodec.h>
#endif

#include <h224/h224.h>
#include <h460/h460.h>
#include <h460/h4601.h>
#include <ptclib/random.h>
#include "h460/h460_std18.h"
#include "h460/h460_std19.h"


#if _DEBUG
const PTimeInterval MonitorCallStartTime(0, 0, 10);
const PTimeInterval MonitorCallStatusTime(0, 0, 10); // Minutes
#else
const PTimeInterval MonitorCallStartTime(0, 10); // Seconds
const PTimeInterval MonitorCallStatusTime(0, 0, 1); // Minutes
#endif

#if OPAL_H239
  static PConstString const H239MessageOID("0.0.8.239.2");
#endif

#if OPAL_H460_NAT
  static BYTE const EmptyTPKT[] = { 3, 0, 0, 0 };
#endif

#define new PNEW


/////////////////////////////////////////////////////////////////////////////

H323Connection::H323Connection(OpalCall & call,
                               H323EndPoint & ep,
                               const PString & token,
                               const PString & alias,
                               const H323TransportAddress & address,
                               unsigned options,
                               OpalConnection::StringOptions * stringOptions)
  : OpalRTPConnection(call, ep, token, options, stringOptions)
  , endpoint(ep)
  , m_remoteConnectAddress(address)
  , remoteCallWaiting(-1)
  , gatekeeperRouted(false)
  , distinctiveRing(0)
  , callReference(token.Mid(token.Find('/')+1).AsUnsigned())
  , m_progressIndicator(0)
  , localAliasNames(ep.GetAliasNames())
  , remoteMaxAudioDelayJitter(0)
  , uuiesRequested(0) // Empty set
  , gkAccessTokenOID(ep.GetGkAccessTokenOID())
  , addAccessTokenToSetup(true) // Automatic inclusion of ACF access token in SETUP
  , controlListener(NULL)
  , h245TunnelRxPDU(NULL)
  , h245TunnelTxPDU(NULL)
  , setupPDU(NULL)
  , alertingPDU(NULL)
  , connectPDU(NULL)
  , progressPDU(NULL)
  , connectionState(NoConnectionActive)
  , h225version(H225_PROTOCOL_VERSION)
  , h245version(H245_PROTOCOL_VERSION)
  , h245versionSet(false)
  , lastPDUWasH245inSETUP(false)
  , m_forceSymmetricTCS(ep.IsForcedSymmetricTCS())
  , mustSendDRQ(false)
  , mediaWaitForConnect(false)
  , m_holdToRemote(false)
  , earlyStart(false)
  , m_releaseCompleteNeeded(true)
  , m_endSessionNeeded(false)
  , isConsultationTransfer(false)
  , m_maintainConnection(false)
  , m_holdFromRemote(eOffHoldFromRemote)
#if OPAL_H450
  , isCallIntrusion(false)
  , callIntrusionProtectionLevel(endpoint.GetCallIntrusionProtectionLevel())
#endif
#if OPAL_H239
  , m_h239Control(ep.GetDefaultH239Control())
  , m_h239SymmetryBreaking(0)
  , m_h239TokenChannel(0)
  , m_h239TerminalLabel(0)
  , m_h239TokenOwned(false)
#endif
#if OPAL_H460
  , P_DISABLE_MSVC_WARNINGS(4355, m_features(ep.InternalCreateFeatureSet(this)))
#endif
  , m_lastUserInputIndication('\0')
{
  PTRACE_CONTEXT_ID_TO(localCapabilities);
  PTRACE_CONTEXT_ID_TO(remoteCapabilities);

  m_UserInputIndicationTimer.SetNotifier(PCREATE_NOTIFIER(UserInputIndicationTimeout));

  localAliasNames.MakeUnique();
  gkAccessTokenOID.MakeUnique();

  m_remotePartyURL = GetPrefixName() + ':';
  remotePartyName = address.GetHostName(true);
  if (alias.IsEmpty())
    m_remotePartyURL += remotePartyName;
  else {
    m_remotePartyURL += alias + '@' + remotePartyName;
    remotePartyName = alias;
  }

  if (OpalIsE164(remotePartyName))
    remotePartyNumber = remotePartyName;

  switch (options&H245TunnelingOptionMask) {
    case H245TunnelingOptionDisable :
      h245Tunneling = false;
      break;

    case H245TunnelingOptionEnable :
      h245Tunneling = true;
      break;

    default :
      h245Tunneling = !ep.IsH245TunnelingDisabled();
      break;
  }

  switch (options&FastStartOptionMask) {
    case FastStartOptionDisable :
      m_fastStartState = FastStartDisabled;
      break;

    case FastStartOptionEnable :
      m_fastStartState = FastStartInitiate;
      break;

    default :
      m_fastStartState = ep.IsFastStartDisabled() ? FastStartDisabled : FastStartInitiate;
      break;
  }

  switch (options&H245inSetupOptionMask) {
    case H245inSetupOptionDisable :
      doH245inSETUP = false;
      break;

    case H245inSetupOptionEnable :
      doH245inSETUP = true;
      break;

    default :
      doH245inSETUP = !ep.IsH245inSetupDisabled();
      break;
  }

  m_conflictingChannels.DisallowDeleteObjects();

  masterSlaveDeterminationProcedure = new H245NegMasterSlaveDetermination(endpoint, *this);
  capabilityExchangeProcedure = new H245NegTerminalCapabilitySet(endpoint, *this);
  logicalChannels = new H245NegLogicalChannels(endpoint, *this);
  requestModeProcedure = new H245NegRequestMode(endpoint, *this);
  roundTripDelayProcedure = new H245NegRoundTripDelay(endpoint, *this);

#if OPAL_H450
  h450dispatcher = new H450xDispatcher(*this);
  h4502handler = new H4502Handler(*this, *h450dispatcher);
  h4504handler = new H4504Handler(*this, *h450dispatcher);
  h4506handler = new H4506Handler(*this, *h450dispatcher);
  h4507handler = new H4507Handler(*this, *h450dispatcher);
  h45011handler = new H45011Handler(*this, *h450dispatcher);
#endif
}


H323Connection::~H323Connection()
{
  delete masterSlaveDeterminationProcedure;
  delete capabilityExchangeProcedure;
  delete logicalChannels;
  delete requestModeProcedure;
  delete roundTripDelayProcedure;
#if OPAL_H450
  delete h450dispatcher;
#endif
  delete setupPDU;
  delete alertingPDU;
  delete connectPDU;
  delete progressPDU;
#if OPAL_H460
  delete m_features;
#endif
  delete controlListener;

  PTRACE(4, "H323\tConnection " << callToken << " deleted.");
}


void H323Connection::OnApplyStringOptions()
{
  OpalConnection::OnApplyStringOptions();

  if (LockReadWrite()) {
    PString str = m_stringOptions(OPAL_OPT_CALL_IDENTIFIER);
    if (!str.IsEmpty())
      callIdentifier = PGloballyUniqueID(str);
    UnlockReadWrite();
  }
}


bool H323Connection::SendReleaseComplete()
{
  H323SignalPDU rcPDU;
  rcPDU.BuildReleaseComplete(*this);
#if OPAL_H450
  h450dispatcher->AttachToReleaseComplete(rcPDU);
#endif

  bool sendingReleaseComplete = m_releaseCompleteNeeded && OnSendReleaseComplete(rcPDU);
  PTRACE_IF(3, sendingReleaseComplete, "H225\tSending release complete PDU: callRef=" << callReference);

  if (m_endSessionNeeded) {
    if (sendingReleaseComplete)
      h245TunnelTxPDU = &rcPDU; // Piggy back H245 on this reply

    // Send an H.245 end session to the remote endpoint.
    H323ControlPDU pdu;
    pdu.BuildEndSessionCommand(H245_EndSessionCommand::e_disconnect);
    if (WriteControlPDU(pdu))
      m_endSessionNeeded = false;
    else {
      PTRACE(2, "H225\tCould not send endSession");
    }
  }

  if (sendingReleaseComplete) {
    m_releaseCompleteNeeded = false;
    h245TunnelTxPDU = NULL;
    return WriteSignalPDU(rcPDU);
  }

  return true;
}


void H323Connection::OnReleased()
{
  PTRACE(4, "H323\tOnReleased: " << callToken << ", connectionState=" << connectionState);

  connectionState = ShuttingDownConnection;

  // Unblock sync points
  digitsWaitFlag.Signal();

 bool waitForEndSession = m_endSessionNeeded; // SendReleaseComplete() will reset flag, so remember it

  if (LockReadWrite()) {
    SendReleaseComplete();

    // Clean up any fast start "pending" channels we may have running.
    for (H323LogicalChannelList::iterator channel = m_fastStartChannels.begin(); channel != m_fastStartChannels.end(); ++channel)
      channel->Close();
    m_fastStartChannels.RemoveAll();

    // Dispose of all the logical channels
    logicalChannels->RemoveAll();

    UnlockReadWrite();
  }

  // Check for gatekeeper and do disengage if have one
  if (mustSendDRQ) {
    H323Gatekeeper * gatekeeper = endpoint.GetGatekeeper();
    if (gatekeeper != NULL)
      gatekeeper->DisengageRequest(*this, H225_DisengageReason::e_normalDrop);
  }

  if (waitForEndSession) {
    // Calculate time since we sent the end session command so we do not actually
    // wait for returned endSession if it has already been that long
    PTimeInterval waitTime = endpoint.GetEndSessionTimeout();
    if (GetConnectionEndTime().IsValid()) {
      PTime now;
      if (now > GetConnectionEndTime()) { // Allow for backward motion in time (DST change)
        waitTime -= now - GetConnectionEndTime();
        if (waitTime < 0)
          waitTime = 0;
      }
    }

    // Wait a while for the remote to send an endSession
    PTRACE(4, "H323\tAwaiting end session from remote for " << waitTime << " seconds");
    if (!endSessionReceived.Wait(waitTime)) {
      PTRACE(2, "H323\tTimed out waiting for end session from remote.");
    }
  }

  // Wait for control channel to be cleaned up (thread ended).
  if (m_controlChannel != NULL)
    m_controlChannel->CloseWait();

  // Do not close m_signallingChannel as H323Endpoint can take it back for possible re-use
  if (m_signallingChannel != NULL) {
    if (m_maintainConnection) {
      PTRACE(4, "H323\tMaintaining signalling channel.");
      m_signallingChannel->AttachThread(NULL);
    }
    else {
      PTRACE(4, "H323\tClosing signalling channel.");
      m_signallingChannel->CloseWait();
      m_signallingChannel.SetNULL();
    }
  }

  OpalRTPConnection::OnReleased();
}


PString H323Connection::GetDestinationAddress()
{
  if (!localDestinationAddress)
    return localDestinationAddress;

  return OpalRTPConnection::GetDestinationAddress();
}


PString H323Connection::GetAlertingType() const
{
  return psprintf("%u", distinctiveRing);
}


bool H323Connection::SetAlertingType(const PString & info)
{
  if (!isdigit(info[0]))
    return false;

  unsigned value = info.AsUnsigned();
  if (value > 7)
    return false;

  distinctiveRing = value;
  return true;
}


void H323Connection::AttachSignalChannel(const PString & token,
                                         H323Transport * channel,
                                         PBoolean answeringCall)
{
  if (!answeringCall)
    InternalSetAsOriginating();

  if (m_signallingChannel != NULL && m_signallingChannel->IsOpen()) {
    PAssertAlways(PLogicError);
    return;
  }

  m_signallingChannel = channel;
  PTRACE_CONTEXT_ID_TO(m_signallingChannel);

  // Set our call token for identification in endpoint dictionary
  callToken = token;

#if OPAL_H460_NAT
  if (m_features != NULL && m_features->HasFeature(H460_FeatureStd18::ID()))
    channel->SetKeepAlive(endpoint.GetManager().GetNatKeepAliveTime(), PBYTEArray(EmptyTPKT, sizeof(EmptyTPKT), false));
#endif
}


PBoolean H323Connection::WriteSignalPDU(H323SignalPDU & pdu)
{
  PAssert(m_signallingChannel != NULL, PLogicError);

  lastPDUWasH245inSETUP = false;

  if (m_signallingChannel != NULL && m_signallingChannel->IsOpen()) {
    pdu.m_h323_uu_pdu.m_h245Tunneling = h245Tunneling;

    H323Gatekeeper * gk = endpoint.GetGatekeeper();
    if (gk != NULL)
      gk->InfoRequestResponse(*this, pdu.m_h323_uu_pdu, true);

    pdu.SetQ931Fields(*this); // Make sure display name is the final value

    if (pdu.Write(*m_signallingChannel))
      return true;
  }

  Release(EndedByTransportFail);
  return false;
}


void H323Connection::HandleSignallingChannel()
{
  PTRACE_CONTEXT_ID_PUSH_THREAD(this);

  PAssert(m_signallingChannel != NULL, PLogicError);

  PTRACE(3, "H225\tReading PDUs: callRef=" << callReference);

  while (m_signallingChannel->IsOpen()) {
    H323SignalPDU pdu;
    if (pdu.Read(*m_signallingChannel)) {
      if (!HandleSignalPDU(pdu)) {
        Release(EndedByTransportFail);
        break;
      }
    }
    else if (m_signallingChannel->GetErrorCode() != PChannel::Timeout) {
      if (m_controlChannel == NULL || !m_controlChannel->IsOpen())
        Release(EndedByTransportFail);
      break;
    }
    else {
      switch (connectionState) {
        case AwaitingSignalConnect :
          // Had time out waiting for remote to send a CONNECT
          ClearCall(EndedByNoAnswer);
          break;
        case HasExecutedSignalConnect :
          // Have had minimum MonitorCallStartTime delay since CONNECT but
          // still no media to move it to EstablishedConnection state. Must
          // thus not have any common codecs to use!
          PTRACE(1, "H225\tTook too long to start media");
          ClearCall(EndedByCapabilityExchange);
          break;
        default :
          break;
      }
    }

    if (m_controlChannel == NULL)
      MonitorCallStatus();
  }

  // If we are the only link to the far end then indicate that we have
  // received endSession even if we hadn't, because we are now never going
  // to get one so there is no point in having CleanUpOnCallEnd wait.
  if (m_controlChannel == NULL) {
    PTRACE(3, "H225\tChannel closed without H.245 channel, releasing H.245 endSession wait");
    endSessionReceived.Signal();
  }

  PTRACE(3, "H225\tSignal channel finished for " << *this);
}


PBoolean H323Connection::HandleSignalPDU(H323SignalPDU & pdu)
{
  // Process the PDU.
  const Q931 & q931 = pdu.GetQ931();

  PTRACE(3, "H225\tHandling PDU: " << q931.GetMessageTypeName() <<
                       " callRef=" << q931.GetCallReference() <<
                       " dn=\""    << q931.GetCalledPartyNumber() << "\""
                       " clid=\""  << q931.GetCallingPartyNumber() << "\""
                       " disp=\""  << q931.GetDisplayName() << '"');

  PSafeLockReadWrite safeLock(*this);
  if (!safeLock.IsLocked())
    return false;

  if (IsReleased()) {
    // Continue to look for endSession/releaseComplete pdus
    if (pdu.m_h323_uu_pdu.m_h245Tunneling) {
      for (PINDEX i = 0; i < pdu.m_h323_uu_pdu.m_h245Control.GetSize(); i++) {
        PPER_Stream strm = pdu.m_h323_uu_pdu.m_h245Control[i].GetValue();
        if (!InternalEndSessionCheck(strm))
          break;
      }
    }
    if (q931.GetMessageType() == Q931::ReleaseCompleteMsg) {
      PTRACE(4, "H225\tReleasing H.245 endSession wait as received Release Complete");
      endSessionReceived.Signal();
    }
    return false;
  }

  // If remote does not do tunneling, so we don't either. Note that if it
  // gets turned off once, it stays off for good.
  if (h245Tunneling && !pdu.m_h323_uu_pdu.m_h245Tunneling && pdu.GetQ931().HasIE(Q931::UserUserIE)) {
    masterSlaveDeterminationProcedure->Stop();
    capabilityExchangeProcedure->Stop();
    h245Tunneling = false;
  }

  h245TunnelRxPDU = &pdu;

  // Check for presence of supplementary services
#if OPAL_H450
  if (pdu.m_h323_uu_pdu.HasOptionalField(H225_H323_UU_PDU::e_h4501SupplementaryService)) {
    if (!h450dispatcher->HandlePDU(pdu)) { // Process H4501SupplementaryService APDU
      return false;
    }
  }
#endif

#if OPAL_H460
  if (pdu.m_h323_uu_pdu.HasOptionalField(H225_H323_UU_PDU::e_genericData)) {
    H225_FeatureSet fs;
    H460_FeatureSet::Copy(fs, pdu.m_h323_uu_pdu.m_genericData);
    OnReceiveFeatureSet(q931.GetMessageType(), fs);
  }
#endif // OPAL_H460

  // Add special code to detect if call is from a Cisco and remoteApplication needs setting
  if (remoteProductInfo.name.IsEmpty() && pdu.m_h323_uu_pdu.HasOptionalField(H225_H323_UU_PDU::e_nonStandardControl)) {
    for (PINDEX i = 0; i < pdu.m_h323_uu_pdu.m_nonStandardControl.GetSize(); i++) {
      const H225_NonStandardIdentifier & id = pdu.m_h323_uu_pdu.m_nonStandardControl[i].m_nonStandardIdentifier;
      if (id.GetTag() == H225_NonStandardIdentifier::e_h221NonStandard) {
        const H225_H221NonStandard & h221 = id;
        if (h221.m_t35CountryCode == 181 && h221.m_t35Extension == 0 && h221.m_manufacturerCode == 18) {
          remoteProductInfo.name = "Cisco IOS";
          remoteProductInfo.version = "12.x";
          remoteProductInfo.t35CountryCode = 181;
          remoteProductInfo.manufacturerCode = 18;
          PTRACE(3, "H225\tSet remote application name: \"" << GetRemoteApplication() << '"');
          break;
        }
      }
    }
  }

  pdu.GetQ931().GetProgressIndicator(m_progressIndicator);

  PBoolean ok;
  switch (q931.GetMessageType()) {
    case Q931::SetupMsg :
      ok = OnReceivedSignalSetup(pdu);
      break;

    case Q931::CallProceedingMsg :
      ok = OnReceivedCallProceeding(pdu);
      break;

    case Q931::ProgressMsg :
      ok = OnReceivedProgress(pdu);
      break;

    case Q931::AlertingMsg :
      ok = OnReceivedAlerting(pdu);
      break;

    case Q931::ConnectMsg :
      ok = OnReceivedSignalConnect(pdu);
      break;

    case Q931::FacilityMsg :
      ok = OnReceivedFacility(pdu);
      break;

    case Q931::SetupAckMsg :
      ok = OnReceivedSignalSetupAck(pdu);
      break;

    case Q931::InformationMsg :
      ok = OnReceivedSignalInformation(pdu);
      break;

    case Q931::NotifyMsg :
      ok = OnReceivedSignalNotify(pdu);
      break;

    case Q931::StatusMsg :
      ok = OnReceivedSignalStatus(pdu);
      break;

    case Q931::StatusEnquiryMsg :
      ok = OnReceivedStatusEnquiry(pdu);
      break;

    case Q931::ReleaseCompleteMsg :
      OnReceivedReleaseComplete(pdu);
      ok = false;
      break;

    default :
      ok = OnUnknownSignalPDU(pdu);
  }

  if (ok) {
    // Process tunnelled H245 PDU, if present.
    HandleTunnelPDU(NULL);

    // Check for establishment criteria met
    InternalEstablishedConnectionCheck();
  }

  h245TunnelRxPDU = NULL;

  PString digits = pdu.GetQ931().GetKeypad();
  if (!digits)
    OnUserInputString(digits);

  H323Gatekeeper * gk = endpoint.GetGatekeeper();
  if (gk != NULL)
    gk->InfoRequestResponse(*this, pdu.m_h323_uu_pdu, false);

  return ok;
}


void H323Connection::HandleTunnelPDU(H323SignalPDU * txPDU)
{
  if (h245TunnelRxPDU == NULL || !h245TunnelRxPDU->m_h323_uu_pdu.m_h245Tunneling)
    return;

  if (!h245Tunneling && h245TunnelRxPDU->m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_setup)
    return;

  H323SignalPDU localTunnelPDU;
  if (txPDU != NULL)
    h245TunnelTxPDU = txPDU;
  else {
    /* Compensate for Cisco bug. IOS cannot seem to accept multiple tunnelled
       H.245 PDUs insode the same facility message */
    if (!HasCompatibilityIssue(e_NoMultipleTunnelledH245)) {
      // Not Cisco et al, so OK to tunnel multiple PDUs
      localTunnelPDU.BuildFacility(*this, true);
      h245TunnelTxPDU = &localTunnelPDU;
    }
  }

  // if a response to a SETUP PDU containing TCS/MSD was ignored, then shutdown negotiations
  PINDEX i;
  if (lastPDUWasH245inSETUP &&
      (h245TunnelRxPDU->m_h323_uu_pdu.m_h245Control.GetSize() == 0) &&
      (h245TunnelRxPDU->GetQ931().GetMessageType() != Q931::CallProceedingMsg)) {
    PTRACE(4, "H225\tTunnelled H.245 in SETUP ignored - resetting H.245 negotiations");
    masterSlaveDeterminationProcedure->Stop();
    lastPDUWasH245inSETUP = false;
    capabilityExchangeProcedure->Stop(true);
  } else {
    for (i = 0; i < h245TunnelRxPDU->m_h323_uu_pdu.m_h245Control.GetSize(); i++) {
      PPER_Stream strm = h245TunnelRxPDU->m_h323_uu_pdu.m_h245Control[i].GetValue();
      HandleControlData(strm);
    }
  }

  // Make sure does not get repeated, clear tunnelled H.245 PDU's
  h245TunnelRxPDU->m_h323_uu_pdu.m_h245Control.SetSize(0);

  if (h245TunnelRxPDU->m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_setup) {
    H225_Setup_UUIE & setup = h245TunnelRxPDU->m_h323_uu_pdu.m_h323_message_body;
    setup.m_maintainConnection = m_maintainConnection;

    if (doH245inSETUP && setup.HasOptionalField(H225_Setup_UUIE::e_parallelH245Control)) {
      for (i = 0; i < setup.m_parallelH245Control.GetSize(); i++) {
        PPER_Stream strm = setup.m_parallelH245Control[i].GetValue();
        HandleControlData(strm);
      }

      // Make sure does not get repeated, clear tunnelled H.245 PDU's
      setup.m_parallelH245Control.SetSize(0);
    }
  }

  h245TunnelTxPDU = NULL;

  // If had replies, then send them off in their own packet
  if (txPDU == NULL && localTunnelPDU.m_h323_uu_pdu.m_h245Control.GetSize() > 0)
    WriteSignalPDU(localTunnelPDU);
}


static bool BuildFastStartList(const H323Channel & channel,
                               H225_ArrayOf_PASN_OctetString & array,
                               H323Channel::Directions reverseDirection)
{
  H245_OpenLogicalChannel open;

  if (channel.GetDirection() != reverseDirection) {
    if (!channel.OnSendingPDU(open))
      return false;
  }
  else {
    open.IncludeOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters);
    if (!channel.OnSendingPDU(open))
      return false;

    open.m_reverseLogicalChannelParameters.m_dataType = open.m_forwardLogicalChannelParameters.m_dataType;
    open.m_forwardLogicalChannelParameters.m_dataType.SetTag(H245_DataType::e_nullData);
    open.m_forwardLogicalChannelParameters.m_multiplexParameters.SetTag(
                H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters::e_none);
  }

  PTRACE(4, "H225\tBuild fastStart:\n  " << setprecision(2) << open);
  PINDEX last = array.GetSize();
  array.SetSize(last+1);
  array[last].EncodeSubType(open);

  PTRACE(3, "H225\tBuilt fastStart for " << channel << ' ' << channel.GetCapability());
  return true;
}


void H323Connection::OnEstablished()
{
  connectionState = EstablishedConnection; // Keep in sync
  endpoint.OnConnectionEstablished(*this, callToken);
  OpalRTPConnection::OnEstablished();
}


void H323Connection::OnSendARQ(H225_AdmissionRequest & arq)
{
#if OPAL_H460
  H225_FeatureSet fs;
  if (OnSendFeatureSet(H460_MessageType::e_admissionRequest, fs) && H460_FeatureSet::Copy(arq.m_genericData, fs))
    arq.IncludeOptionalField(H225_AdmissionRequest::e_genericData);
#endif // OPAL_H460

  endpoint.OnSendARQ(*this, arq);
}


void H323Connection::OnReceivedACF(const H225_AdmissionConfirm & acf)
{
#if OPAL_H460
  if (acf.HasOptionalField(H225_AdmissionConfirm::e_genericData)) {
    H225_FeatureSet fs;
    if (H460_FeatureSet::Copy(fs, acf.m_genericData))
      OnReceiveFeatureSet(H460_MessageType::e_admissionConfirm, fs);
  }
#endif // OPAL_H460
}


void H323Connection::OnReceivedARJ(const H225_AdmissionReject & arj)
{
#if OPAL_H460
  if (arj.HasOptionalField(H225_AdmissionReject::e_genericData)) {
    H225_FeatureSet fs;
    if (H460_FeatureSet::Copy(fs, arj.m_genericData))
      OnReceiveFeatureSet(H460_MessageType::e_admissionReject, fs);
  }
#endif // OPAL_H460
}


void H323Connection::OnSendIRR(H225_InfoRequestResponse & irr) const
{
#if OPAL_H460
  H225_FeatureSet fs;
  if (OnSendFeatureSet(H460_MessageType::e_inforequestresponse, fs) && H460_FeatureSet::Copy(irr.m_genericData, fs))
    irr.IncludeOptionalField(H225_InfoRequestResponse::e_genericData);
#endif // OPAL_H460
}


void H323Connection::OnSendDRQ(H225_DisengageRequest & drq) const
{
#if OPAL_H460
  H225_FeatureSet fs;
  if (OnSendFeatureSet(H460_MessageType::e_disengagerequest, fs) && H460_FeatureSet::Copy(drq.m_genericData, fs))
    drq.IncludeOptionalField(H225_DisengageRequest::e_genericData);
#endif // OPAL_H460
}


void H323Connection::SetRemoteVersions(const H225_ProtocolIdentifier & protocolIdentifier)
{
  if (protocolIdentifier.GetSize() < 6)
    return;

  h225version = std::min(protocolIdentifier[5], h225version);

  if (h245versionSet)
    return;

  // If has not been told explicitly what the H.245 version use, make an
  // assumption based on the H.225 version
  switch (h225version) {
    case 1 :
      h245version = 2;  // H.323 version 1
      break;
    case 2 :
      h245version = 3;  // H.323 version 2
      break;
    case 3 :
      h245version = 5;  // H.323 version 3
      break;
    case 4 :
      h245version = 7;  // H.323 version 4
      break;
    case 5 :
      h245version = 9;  // H.323 version 5
      break;
    default:
      h245version = 13; // H.323 version 6
      break;
  }
  PTRACE(3, "H225\tSet protocol version to " << h225version
         << " and implying H.245 version " << h245version);
}


PBoolean H323Connection::OnReceivedSignalSetup(const H323SignalPDU & originalSetupPDU)
{
  if (originalSetupPDU.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_setup)
    return false;

  SetPhase(SetUpPhase);

  setupPDU = new H323SignalPDU(originalSetupPDU);
  PTRACE_CONTEXT_ID_TO(setupPDU);

  H225_Setup_UUIE & setup = setupPDU->m_h323_uu_pdu.m_h323_message_body;

  SetRemoteVersions(setup.m_protocolIdentifier);
  SetRemotePartyInfo(*setupPDU); // Determine the remote parties name/number/address as best we can
  SetRemoteApplication(setup.m_sourceInfo);
#if OPAL_H235_6
  SetDiffieHellman(setup);
#endif
  if (HasCompatibilityIssue(e_ForceMaintainConnection))
    m_maintainConnection = true;
  else
    SetMaintainConnectionFlag(setup);

  switch (setup.m_conferenceGoal.GetTag()) {
    case H225_Setup_UUIE_conferenceGoal::e_create:
      m_conferenceGoal = e_Create;
      break;

    case H225_Setup_UUIE_conferenceGoal::e_join:
      m_conferenceGoal = e_Join;
      break;

    case H225_Setup_UUIE_conferenceGoal::e_invite:
      m_conferenceGoal= e_Invite;
      break;

    case H225_Setup_UUIE_conferenceGoal::e_callIndependentSupplementaryService:
      return endpoint.OnCallIndependentSupplementaryService(*setupPDU);

    case H225_Setup_UUIE_conferenceGoal::e_capability_negotiation:
      return endpoint.OnNegotiateConferenceCapabilities(*setupPDU);
  }

  // Get the ring pattern
  distinctiveRing = setupPDU->GetDistinctiveRing();

  // Save the identifiers sent by caller
  if (setup.HasOptionalField(H225_Setup_UUIE::e_callIdentifier))
    callIdentifier = setup.m_callIdentifier.m_guid;
  conferenceIdentifier = setup.m_conferenceID;

  setupPDU->GetQ931().GetRedirectingNumber(m_redirectingParty);

  // compare the source call signalling address
  if (setup.HasOptionalField(H225_Setup_UUIE::e_sourceCallSignalAddress))
    DetermineRTPNAT(*m_signallingChannel, H323TransportAddress(setup.m_sourceCallSignalAddress));

  // Anything else we need from setup PDU
  mediaWaitForConnect = setup.m_mediaWaitForConnect;
  if (!setupPDU->GetQ931().GetCalledPartyNumber(localDestinationAddress)) {
    localDestinationAddress = setupPDU->GetDestinationAlias(true);
    if (m_signallingChannel->GetLocalAddress().IsEquivalent(localDestinationAddress))
      localDestinationAddress = '*';
  }

  SetIncomingBearerCapabilities(*setupPDU);

#if OPAL_H460
  H225_FeatureSet fs;
  bool hasFeaturePDU = false;

  if (setup.HasOptionalField(H225_Setup_UUIE::e_neededFeatures)) {
    fs.IncludeOptionalField(H225_FeatureSet::e_neededFeatures);
    H225_ArrayOf_FeatureDescriptor & fsn = fs.m_neededFeatures;
    fsn = setup.m_neededFeatures;
    hasFeaturePDU = true;
  }

  if (setup.HasOptionalField(H225_Setup_UUIE::e_desiredFeatures)) {
    fs.IncludeOptionalField(H225_FeatureSet::e_desiredFeatures);
    H225_ArrayOf_FeatureDescriptor & fsn = fs.m_desiredFeatures;
    fsn = setup.m_desiredFeatures;
    hasFeaturePDU = true;
  }

  if (setup.HasOptionalField(H225_Setup_UUIE::e_supportedFeatures)) {
    fs.IncludeOptionalField(H225_FeatureSet::e_supportedFeatures);
    H225_ArrayOf_FeatureDescriptor & fsn = fs.m_supportedFeatures;
    fsn = setup.m_supportedFeatures;
    hasFeaturePDU = true;
  }

  if (hasFeaturePDU)
    OnReceiveFeatureSet(H460_MessageType::e_setup, fs);
#endif // OPAL_H460

  // Send back a H323 Call Proceeding PDU in case OnIncomingCall() takes a while
  PTRACE(3, "H225\tSending call proceeding PDU");
  H323SignalPDU callProceedingPDU;
  H225_CallProceeding_UUIE & callProceeding = callProceedingPDU.BuildCallProceeding(*this);

  if (!isConsultationTransfer) {
    if (OnSendCallProceeding(callProceedingPDU)) {
      if (m_fastStartState == FastStartDisabled)
        callProceeding.IncludeOptionalField(H225_CallProceeding_UUIE::e_fastConnectRefused);

      if (!WriteSignalPDU(callProceedingPDU))
        return false;

      if (GetPhase() < ProceedingPhase) {
        SetPhase(ProceedingPhase);
        OnProceeding();
      }
    }

    /** Here is a spot where we should wait in case of Call Intrusion
    for CIPL from other endpoints
    if (isCallIntrusion) return true;
    */

    // if the application indicates not to contine, then send a Q931 Release Complete PDU
    alertingPDU = new H323SignalPDU;
    PTRACE_CONTEXT_ID_TO(alertingPDU);
    alertingPDU->BuildAlerting(*this);

    /** If we have a case of incoming call intrusion we should not Clear the Call*/
    if (!OnIncomingCall(*setupPDU, *alertingPDU)
#if OPAL_H450
        && !isCallIntrusion
#endif
        ) {
      Release(EndedByNoAccept);
      PTRACE(2, "H225\tApplication not accepting calls");
      return false;
    }
    if (IsReleased()) {
      PTRACE(1, "H225\tApplication called ClearCall during OnIncomingCall");
      return false;
    }

    // send Q931 Alerting PDU
    PTRACE(3, "H225\tIncoming call accepted");

    // Check for gatekeeper and do admission check if have one
    H323Gatekeeper * gatekeeper = endpoint.GetGatekeeper();
    if (gatekeeper != NULL) {
      H225_ArrayOf_AliasAddress destExtraCallInfoArray;
      H323Gatekeeper::AdmissionResponse response;
      response.destExtraCallInfo = &destExtraCallInfoArray;
      if (!gatekeeper->AdmissionRequest(*this, response)) {
        PTRACE(2, "H225\tGatekeeper refused admission: "
               << (response.rejectReason == UINT_MAX
                    ? PString("Transport error")
                    : H225_AdmissionRejectReason(response.rejectReason).GetTagName()));
        switch (response.rejectReason) {
          case H225_AdmissionRejectReason::e_calledPartyNotRegistered :
            Release(EndedByNoUser);
            break;
          case H225_AdmissionRejectReason::e_requestDenied :
            Release(EndedByNoBandwidth);
            break;
          case H225_AdmissionRejectReason::e_invalidPermission :
          case H225_AdmissionRejectReason::e_securityDenial :
            ClearCall(EndedBySecurityDenial);
            break;
          case H225_AdmissionRejectReason::e_resourceUnavailable :
            Release(EndedByRemoteBusy);
            break;
          default :
            Release(EndedByGkAdmissionFailed);
        }
        return false;
      }

      if (destExtraCallInfoArray.GetSize() > 0)
        destExtraCallInfo = H323GetAliasAddressString(destExtraCallInfoArray[0]);
      mustSendDRQ = true;
      gatekeeperRouted = response.gatekeeperRouted;
    }
  }

  OnApplyStringOptions();

  // Get the local capabilities before fast start or tunnelled TCS is handled
  OnSetLocalCapabilities();

  // DO this handle in case a TCS is in the SETUP packet
  HandleTunnelPDU(NULL);

  if (m_fastStartState != FastStartDisabled && setup.HasOptionalField(H225_Setup_UUIE::e_fastStart)) {
    PTRACE(3, "H225\tFast start detected");

    m_fastStartState = FastStartDisabled;

    // If we have not received caps from remote, we are going to build a
    // fake one from the fast connect data.
    if (!capabilityExchangeProcedure->HasReceivedCapabilities())
      remoteCapabilities.RemoveAll();

    bool localCapsEmpty = localCapabilities.GetSize() == 0;
    if (localCapsEmpty)
      localCapabilities = endpoint.GetCapabilities();

    // Extract capabilities from the fast start OpenLogicalChannel structures
    PINDEX i;
    for (i = 0; i < setup.m_fastStart.GetSize(); i++) {
      H245_OpenLogicalChannel open;
      if (setup.m_fastStart[i].DecodeSubType(open)) {
        PTRACE(4, "H225\tFast start open:\n  " << setprecision(2) << open);
        const H245_DataType * dataType = NULL;
        if (open.HasOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters)) {
          if (open.m_reverseLogicalChannelParameters.m_multiplexParameters.GetTag() ==
                H245_OpenLogicalChannel_reverseLogicalChannelParameters_multiplexParameters::e_h2250LogicalChannelParameters)
            dataType = &open.m_reverseLogicalChannelParameters.m_dataType;
        }
        else {
          if (open.m_forwardLogicalChannelParameters.m_multiplexParameters.GetTag() ==
              H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters::e_h2250LogicalChannelParameters)
            dataType = &open.m_forwardLogicalChannelParameters.m_dataType;
        }
        if (dataType != NULL) {
          H323Capability * capability = remoteCapabilities.FindCapability(*dataType);
          if (capability == NULL && (capability = localCapabilities.FindCapability(*dataType)) != NULL) {
            // If we actually have the remote capabilities then the remote (very oddly)
            // had a fast connect entry it could not do. If we have not yet got a remote
            // cap table then build one using all possible caps.
            capability = remoteCapabilities.Copy(*capability);
            remoteCapabilities.SetCapability(0, capability->GetDefaultSessionID()-1, capability);
          }
          if (capability != NULL) {
            unsigned error;
            H323Channel * channel = CreateLogicalChannel(open, true, error);
            if (channel != NULL) {
              if (channel->GetDirection() == H323Channel::IsTransmitter)
                channel->SetNumber(logicalChannels->GetNextChannelNumber());
              m_fastStartChannels.Append(channel);
              m_fastStartState = FastStartResponse;
            }
          }
        }
      }
      else {
        PTRACE(1, "H225\tInvalid fast start PDU decode:\n  " << open);
      }
    }

    if (localCapsEmpty)
      localCapabilities.RemoveAll();

    PTRACE(3, "H225\tFound " << m_fastStartChannels.GetSize() << " fast start channels");
    PTRACE_IF(4, !capabilityExchangeProcedure->HasReceivedCapabilities(),
              "H323\tPreliminary remote capabilities generated from fast start:\n" << remoteCapabilities);
  }

  // Check that it has the H.245 channel connection info
  if (!CreateOutgoingControlChannel(setup,
                                    setup.m_h245Address, H225_Setup_UUIE::e_h245Address,
                                    setup.m_h245SecurityCapability.GetSize() > 0 ? setup.m_h245SecurityCapability[0]
                                                                                 : H225_H245Security(),
                                    H225_Setup_UUIE::e_h245SecurityCapability))
    return false;

  // Build the reply with the channels we are actually using
  connectPDU = new H323SignalPDU;
  PTRACE_CONTEXT_ID_TO(connectPDU);
  connectPDU->BuildConnect(*this);

  progressPDU = new H323SignalPDU;
  PTRACE_CONTEXT_ID_TO(progressPDU);
  progressPDU->BuildProgress(*this);

  connectionState = AwaitingLocalAnswer;

  // OK are now ready to send SETUP to remote protocol
  ownerCall.OnSetUp(*this);

  if (connectionState == ShuttingDownConnection)
    return false;

  if (connectionState != AwaitingLocalAnswer)
    return true;

#if OPAL_H450
  // If Call Intrusion is allowed we must answer the call
  if (IsCallIntrusion())
    AnsweringCall(AnswerCallDeferred);
  else if (isConsultationTransfer)
    AnsweringCall(AnswerCallNow);
  else
#endif
    // call the application callback to determine if to answer the call or not
    AnsweringCall(OnAnswerCall(remotePartyName, *setupPDU, *connectPDU, *progressPDU));

  return connectionState != ShuttingDownConnection;
}


PString H323Connection::GetIdentifier() const
{
  return callIdentifier.AsString();
}


void H323Connection::SetLocalPartyName(const PString & name)
{
  if (!name.IsEmpty()) {
    OpalRTPConnection::SetLocalPartyName(name);
    localAliasNames.RemoveAll();
    localAliasNames.AppendString(name);
  }
}


void H323Connection::SetRemotePartyInfo(const H323SignalPDU & pdu)
{
  const Q931 & q931 = pdu.GetQ931();
  PString remotePartyAddress;

  q931.GetCalledPartyNumber(m_calledPartyNumber);

  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_setup)
    remotePartyNumber = m_calledPartyName = m_calledPartyNumber;
  else {
    const H225_Setup_UUIE & setup = pdu.m_h323_uu_pdu.m_h323_message_body;

    if (m_calledPartyNumber.IsEmpty())
      m_calledPartyNumber = H323GetAliasAddressE164(setup.m_destinationAddress);

    for (PINDEX i = 0; i < setup.m_destinationAddress.GetSize(); ++i) {
      PString addr = H323GetAliasAddressString(setup.m_destinationAddress[i]);
      if (addr != m_calledPartyNumber) {
        m_calledPartyName = addr;
        break;
      }
    }

    if (!q931.GetCallingPartyNumber(remotePartyNumber))
      remotePartyNumber = H323GetAliasAddressE164(setup.m_sourceAddress);

    if (setup.m_sourceAddress.GetSize() > 0)
      remotePartyAddress = H323GetAliasAddressString(setup.m_sourceAddress[0]);
  }

  if (remotePartyAddress.IsEmpty())
    remotePartyAddress = remotePartyNumber;

  PString remoteHostName;
  H323Gatekeeper * gatekeeper = endpoint.GetGatekeeper();
  if (!gatekeeperRouted || gatekeeper == NULL)
    remoteHostName = m_signallingChannel->GetRemoteAddress().GetHostName(IsOriginating());
  else {
    PString gkId, gkHost;
    if (gatekeeper->GetName().Split('@', gkId, gkHost))
      remoteHostName = gkHost;
    else
      remoteHostName = gatekeeper->GetName();
    remoteHostName += ";type=gk";
  }

  if (!IsOriginating() || m_remotePartyURL.IsEmpty()) {
    m_remotePartyURL = GetPrefixName() + ':';
    if (remotePartyAddress.IsEmpty()) {
      remotePartyAddress = remoteHostName;
      m_remotePartyURL += remoteHostName;
    }
    else if (remotePartyAddress == remoteHostName || remotePartyAddress.Find('@') != P_MAX_INDEX)
      m_remotePartyURL += remotePartyAddress;
    else if (remotePartyNumber.IsEmpty())
      m_remotePartyURL += PURL::TranslateString(remotePartyAddress, PURL::LoginTranslation) + '@' + remoteHostName;
    else
      m_remotePartyURL += remotePartyNumber + '@' + remoteHostName;
  }

  remotePartyName = pdu.GetSourceAliases(m_signallingChannel);
  PTRACE(3, "H225\tSet remote party name: \"" << remotePartyName << '"');
}


void H323Connection::SetRemoteApplication(const H225_EndpointType & pdu)
{
  if (pdu.HasOptionalField(H225_EndpointType::e_vendor)) {
    H323GetApplicationInfo(remoteProductInfo, pdu.m_vendor);
    PTRACE(3, "H225\tSet remote application name: \"" << GetRemoteApplication() << '"');
  }
}


PBoolean H323Connection::OnReceivedSignalSetupAck(const H323SignalPDU & /*setupackPDU*/)
{
  OnInsufficientDigits();
  return true;
}


PBoolean H323Connection::OnReceivedSignalInformation(const H323SignalPDU & /*infoPDU*/)
{
  return true;
}


PBoolean H323Connection::OnReceivedCallProceeding(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_callProceeding)
    return false;
  const H225_CallProceeding_UUIE & call = pdu.m_h323_uu_pdu.m_h323_message_body;

  SetRemoteVersions(call.m_protocolIdentifier);
  SetRemotePartyInfo(pdu);
  SetRemoteApplication(call.m_destinationInfo);
#if OPAL_H235_6
  SetDiffieHellman(call);
#endif
  SetMaintainConnectionFlag(call);


#if OPAL_H460
  if (call.HasOptionalField(H225_CallProceeding_UUIE::e_featureSet))
    OnReceiveFeatureSet(H460_MessageType::e_callProceeding, call.m_featureSet);
#endif

  // Check for fastStart data and start fast
  if (call.HasOptionalField(H225_CallProceeding_UUIE::e_fastStart))
    HandleFastStartAcknowledge(call.m_fastStart);

  // Check that it has the H.245 channel connection info
  if (!CreateOutgoingControlChannel(call,
                                    call.m_h245Address, H225_CallProceeding_UUIE::e_h245Address,
                                    call.m_h245SecurityMode, H225_CallProceeding_UUIE::e_h245SecurityMode))
    return false;

  if (GetPhase() < ProceedingPhase) {
    SetPhase(ProceedingPhase);
    OnProceeding();
  }

  return true;
}


PBoolean H323Connection::OnReceivedProgress(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_progress)
    return false;
  const H225_Progress_UUIE & progress = pdu.m_h323_uu_pdu.m_h323_message_body;

  SetRemoteVersions(progress.m_protocolIdentifier);
  SetRemotePartyInfo(pdu);
  SetRemoteApplication(progress.m_destinationInfo);
#if OPAL_H235_6
  SetDiffieHellman(progress);
#endif
  SetMaintainConnectionFlag(progress);

  // Check for fastStart data and start fast
  if (progress.HasOptionalField(H225_Progress_UUIE::e_fastStart))
    HandleFastStartAcknowledge(progress.m_fastStart);

  // Check that it has the H.245 channel connection info
  return CreateOutgoingControlChannel(progress,
                                      progress.m_h245Address, H225_Progress_UUIE::e_h245Address,
                                      progress.m_h245SecurityMode, H225_Progress_UUIE::e_h245SecurityMode);
}


PBoolean H323Connection::OnReceivedAlerting(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_alerting)
    return false;

  if (GetPhase() >= AlertingPhase)
    return true;

  SetPhase(AlertingPhase);

  const H225_Alerting_UUIE & alert = pdu.m_h323_uu_pdu.m_h323_message_body;

  SetRemoteVersions(alert.m_protocolIdentifier);
  SetRemotePartyInfo(pdu);
  SetRemoteApplication(alert.m_destinationInfo);
#if OPAL_H235_6
  SetDiffieHellman(alert);
#endif
  SetMaintainConnectionFlag(alert);

#if OPAL_H460
  if (alert.HasOptionalField(H225_Alerting_UUIE::e_featureSet))
    OnReceiveFeatureSet(H460_MessageType::e_alerting, alert.m_featureSet);
#endif

  // Check for fastStart data and start fast
  if (alert.HasOptionalField(H225_Alerting_UUIE::e_fastStart))
    HandleFastStartAcknowledge(alert.m_fastStart);

  // Check that it has the H.245 channel connection info
  if (!CreateOutgoingControlChannel(alert,
                                    alert.m_h245Address, H225_Alerting_UUIE::e_h245Address,
                                    alert.m_h245SecurityMode, H225_Alerting_UUIE::e_h245SecurityMode))
    return false;

  return OnAlerting(pdu, remotePartyName);
}


PBoolean H323Connection::OnReceivedSignalConnect(const H323SignalPDU & pdu)
{
  if (GetPhase() < AlertingPhase) {
    // Never actually got the ALERTING, fake it as some things get upset
    // if the sequence is not strictly followed.
    SetPhase(AlertingPhase);
    if (!OnAlerting(pdu, remotePartyName))
      return false;
  }

  if (connectionState == ShuttingDownConnection)
    return false;
  connectionState = HasExecutedSignalConnect;

  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_connect)
    return false;
  const H225_Connect_UUIE & connect = pdu.m_h323_uu_pdu.m_h323_message_body;

  SetRemoteVersions(connect.m_protocolIdentifier);
  SetRemotePartyInfo(pdu);
  SetRemoteApplication(connect.m_destinationInfo);
#if OPAL_H235_6
  SetDiffieHellman(connect);
#endif
  SetMaintainConnectionFlag(connect);
  SetIncomingBearerCapabilities(pdu);

#if OPAL_H460
  if (connect.HasOptionalField(H225_Connect_UUIE::e_featureSet))
    OnReceiveFeatureSet(H460_MessageType::e_connect, connect.m_featureSet);
#endif

  if (!OnOutgoingCall(pdu)) {
    Release(EndedByNoAccept);
    return false;
  }

#if OPAL_H450
  // Are we involved in a transfer with a non H.450.2 compatible transferred-to endpoint?
  if (h4502handler->GetState() == H4502Handler::e_ctAwaitSetupResponse &&
      h4502handler->IsctTimerRunning())
  {
    PTRACE(4, "H4502\tRemote Endpoint does not support H.450.2.");
    h4502handler->OnReceivedSetupReturnResult();
  }
#endif

  // have answer, so set timeout to interval for monitoring calls health
  m_signallingChannel->SetReadTimeout(connectionState < EstablishedConnection ? MonitorCallStartTime : MonitorCallStatusTime);

  // NOTE h323plus checks for faststart here
  // and bails if already started.

  // Check for fastStart data and start fast
  if (connect.HasOptionalField(H225_Connect_UUIE::e_fastStart)) {
    // We have fast started channels, can connect immediately, this starts the media streams.
    if (HandleFastStartAcknowledge(connect.m_fastStart))
      OnConnectedInternal();
  }

  if (m_fastStartState != FastStartAcknowledged) {
    // If didn't get fast start channels accepted by remote then clear our
    // proposed channels
    m_fastStartState = FastStartDisabled;
    m_fastStartChannels.RemoveAll();
  }

  // Check that it has the H.245 channel connection info
  if (!CreateOutgoingControlChannel(connect,
                                    connect.m_h245Address, H225_Connect_UUIE::e_h245Address,
                                    connect.m_h245SecurityMode, H225_Connect_UUIE::e_h245SecurityMode)) {
    if (m_fastStartState != FastStartAcknowledged)
      return false;
    // Have media through fast start, so even though H.245 cactus, we battle on
  }

  /* do not start h245 negotiation if it is disabled */
  if (endpoint.IsH245Disabled()){
    PTRACE(3, "H245\tOnReceivedSignalConnect: h245 is disabled, do not start negotiation");
    return true;
  }
  // If we have a H.245 channel available, bring it up. We either have media
  // and this is just so user indications work, or we don't have media and
  // desperately need it!
  if (h245Tunneling)
    return StartControlNegotiations();

  // Already started
  if (m_controlChannel != NULL)
    return true;

  // We have no tunnelling and not separate channel, but we really want one
  // so we will start one using a facility message
  PTRACE(3, "H225\tNo H245 address provided by remote, starting control channel");

  H323SignalPDU want245PDU;
  H225_Facility_UUIE * fac = want245PDU.BuildFacility(*this, false);
  if (!CreateIncomingControlChannel(*fac,
                                    fac->m_h245Address, H225_Facility_UUIE::e_h245Address,
                                    fac->m_h245SecurityMode, H225_Facility_UUIE::e_h245SecurityMode))
    return false;

  fac->m_reason.SetTag(H225_FacilityReason::e_startH245);
  return WriteSignalPDU(want245PDU);
}


PBoolean H323Connection::OnReceivedFacility(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_empty)
    return true;

  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_facility)
    return false;
  const H225_Facility_UUIE & fac = pdu.m_h323_uu_pdu.m_h323_message_body;

  SetRemoteVersions(fac.m_protocolIdentifier);
  if (fac.HasOptionalField(H225_Facility_UUIE::e_destinationInfo))
    SetRemoteApplication(fac.m_destinationInfo);
#if OPAL_H235_6
  SetDiffieHellman(fac);
#endif
  SetMaintainConnectionFlag(fac);

#if OPAL_H460
  // Do not process H.245 Control PDU's
  if (!pdu.m_h323_uu_pdu.HasOptionalField(H225_H323_UU_PDU::e_h245Control) &&
       fac.HasOptionalField(H225_Facility_UUIE::e_featureSet))
    OnReceiveFeatureSet(H460_MessageType::e_facility, fac.m_featureSet);
#endif

  // Check for fastStart data and start fast
  if (fac.HasOptionalField(H225_Facility_UUIE::e_fastStart))
    HandleFastStartAcknowledge(fac.m_fastStart);

  // Check that it has the H.245 channel connection info
  if (fac.HasOptionalField(H225_Facility_UUIE::e_h245Address) && (!pdu.m_h323_uu_pdu.m_h245Tunneling || endpoint.IsH245TunnelingDisabled())) {
    if (m_controlChannel != NULL) {
      // Fix race condition where both side want to open H.245 channel. we have
      // channel bit it is not open (ie we are listening) and the remote has
      // sent us an address to connect to. To resolve we compare the addresses.

      H323TransportAddress h323Address = m_controlChannel->GetLocalAddress();
      H225_TransportAddress myAddress;
      h323Address.SetPDU(myAddress);
      PPER_Stream myBuffer;
      myAddress.Encode(myBuffer);

      PPER_Stream otherBuffer;
      fac.m_h245Address.Encode(otherBuffer);

      if (myBuffer < otherBuffer) {
        PTRACE(2, "H225\tSimultaneous start of H.245 channel, connecting to remote.");
        m_controlChannel->CloseWait();
        m_controlChannel.SetNULL();
      }
      else {
        PTRACE(2, "H225\tSimultaneous start of H.245 channel, using local listener.");
      }
    }

    return CreateOutgoingControlChannel(fac,
                                        fac.m_h245Address, H225_Facility_UUIE::e_h245Address,
                                        fac.m_h245SecurityMode, H225_Facility_UUIE::e_h245SecurityMode);
  }

  if (fac.m_reason.GetTag() != H225_FacilityReason::e_callForwarded &&
      fac.m_reason.GetTag() != H225_FacilityReason::e_routeCallToGatekeeper)
    return true;

  PURL addrURL = GetRemotePartyURL();
  if (fac.HasOptionalField(H225_Facility_UUIE::e_alternativeAliasAddress) &&
      fac.m_alternativeAliasAddress.GetSize() > 0)
    addrURL.SetUserName(H323GetAliasAddressString(fac.m_alternativeAliasAddress[0]));

  if (fac.HasOptionalField(H225_Facility_UUIE::e_alternativeAddress)) {
    // Handle routeCallToGatekeeper and send correct destCallSignalAddress
    // in the H.225 setup message
    if (fac.m_reason.GetTag() == H225_FacilityReason::e_routeCallToGatekeeper)
      addrURL.SetUserName(addrURL.GetUserName()+'@'+addrURL.GetHostName());

    // Set the new host/port in URL from alternative address
    H323TransportAddress alternative(fac.m_alternativeAddress);
    if (!alternative.IsEmpty()) {
      PIPSocket::Address ip;
      WORD port = endpoint.GetDefaultSignalPort();
      if (!alternative.GetIpAndPort(ip, port))
        addrURL.SetHostName(alternative.Mid(alternative.Find('$')+1));
      else {
        addrURL.SetHostName(ip.AsString(true));
        addrURL.SetPort(port);
      }
    }
  }

  PString address = addrURL.AsString();

  if (endpoint.OnConnectionForwarded(*this, address, pdu)) {
    Release(EndedByCallForwarded);
    return false;
  }

  if (!endpoint.OnForwarded(*this, address)) {
    Release(EndedByCallForwarded);
    return false;
  }

  if (!endpoint.CanAutoCallForward())
    return true;

  // If forward is successful, then return false to terminate THIS call.
  return !endpoint.ForwardConnection(*this, address, pdu);
}


PBoolean H323Connection::OnReceivedSignalNotify(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_notify) {
    const H225_Notify_UUIE & notify = pdu.m_h323_uu_pdu.m_h323_message_body;
    SetRemoteVersions(notify.m_protocolIdentifier);
#if OPAL_H235_6
    SetDiffieHellman(notify);
#endif
  }
  return true;
}


PBoolean H323Connection::OnReceivedSignalStatus(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_status) {
    const H225_Status_UUIE & status = pdu.m_h323_uu_pdu.m_h323_message_body;
    SetRemoteVersions(status.m_protocolIdentifier);
#if OPAL_H235_6
    SetDiffieHellman(status);
#endif
  }
  return true;
}


PBoolean H323Connection::OnReceivedStatusEnquiry(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_statusInquiry) {
    const H225_StatusInquiry_UUIE & status = pdu.m_h323_uu_pdu.m_h323_message_body;
    SetRemoteVersions(status.m_protocolIdentifier);
#if OPAL_H235_6
    SetDiffieHellman(status);
#endif
  }

  H323SignalPDU reply;
  reply.BuildStatus(*this);
  return reply.Write(*m_signallingChannel);
}


void H323Connection::OnReceivedReleaseComplete(const H323SignalPDU & pdu)
{
  endSessionReceived.Signal();

  CallEndReason reason(EndedByRefusal, pdu.GetQ931().GetCause());

  switch (connectionState) {
    case EstablishedConnection :
      reason.code = EndedByRemoteUser;
      break;

    case AwaitingLocalAnswer :
      reason.code = EndedByCallerAbort;
      break;

    default :
      if (callEndReason == EndedByRefusal)
        callEndReason = NumCallEndReasons;

      // Are we involved in a transfer with a non H.450.2 compatible transferred-to endpoint?
#if OPAL_H450
      if (h4502handler->GetState() == H4502Handler::e_ctAwaitSetupResponse && h4502handler->IsctTimerRunning()) {
        PTRACE(4, "H4502\tThe Remote Endpoint has rejected our transfer request and does not support H.450.2.");
        h4502handler->OnReceivedSetupReturnError(H4501_GeneralErrorList::e_notAvailable);
      }
#endif

      if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_releaseComplete) {
        const H225_ReleaseComplete_UUIE & rc = pdu.m_h323_uu_pdu.m_h323_message_body;
#if OPAL_H460
        if (rc.HasOptionalField(H225_ReleaseComplete_UUIE::e_featureSet))
          OnReceiveFeatureSet(H460_MessageType::e_releaseComplete, rc.m_featureSet);
#endif
        SetRemoteVersions(rc.m_protocolIdentifier);
        reason = H323TranslateToCallEndReason(pdu.GetQ931().GetCause(), rc.m_reason.GetTag());
      }
  }

  Release(reason);
  SendReleaseComplete();
}


PBoolean H323Connection::OnIncomingCall(const H323SignalPDU & setupPDU,
                                    H323SignalPDU & alertingPDU)
{
  return endpoint.OnIncomingCall(*this, setupPDU, alertingPDU);
}


PBoolean H323Connection::ForwardCall(const PString & forwardParty)
{
  if (forwardParty.IsEmpty())
    return false;

  PString alias;
  H323TransportAddress address;
  endpoint.ParsePartyName(forwardParty, alias, address);

  H323SignalPDU redirectPDU;
  H225_Facility_UUIE * fac = redirectPDU.BuildFacility(*this, false);

  fac->m_reason.SetTag(H225_FacilityReason::e_callForwarded);

  if (!address) {
    fac->IncludeOptionalField(H225_Facility_UUIE::e_alternativeAddress);
    address.SetPDU(fac->m_alternativeAddress, endpoint.GetDefaultSignalPort());
  }

  if (!alias) {
    fac->IncludeOptionalField(H225_Facility_UUIE::e_alternativeAliasAddress);
    fac->m_alternativeAliasAddress.SetSize(1);
    H323SetAliasAddress(alias, fac->m_alternativeAliasAddress[0]);
  }

  if (WriteSignalPDU(redirectPDU))
    Release(EndedByCallForwarded);

  return true;
}


H323Connection::AnswerCallResponse
     H323Connection::OnAnswerCall(const PString & caller,
                                  const H323SignalPDU & setupPDU,
                                  H323SignalPDU & connectPDU,
                                  H323SignalPDU & progressPDU)
{
  PTRACE(3, "H323CON\tOnAnswerCall " << *this << ", caller = " << caller);
  return endpoint.OnAnswerCall(*this, caller, setupPDU, connectPDU, progressPDU);
}

H323Connection::AnswerCallResponse
     H323Connection::OnAnswerCall(const PString & caller)
{
  return OpalRTPConnection::OnAnswerCall(caller);
}

void H323Connection::AnsweringCall(AnswerCallResponse response)
{
  PTRACE(3, "H323\tAnswering call: " << response);

  PSafeLockReadWrite safeLock(*this);
  if (!safeLock.IsLocked() || IsReleased())
    return;

  if (response == AnswerCallProgress) {
    H323SignalPDU want245PDU;
    want245PDU.BuildProgress(*this);
    WriteSignalPDU(want245PDU);
  }

  OpalConnection::AnsweringCall(response);
}


PString H323Connection::GetPrefixName() const
{
#if OPAL_PTLIB_SSL
  if (m_signallingChannel != NULL && strchr(m_signallingChannel->GetProtoPrefix(), 's') != NULL)
    return OpalConnection::GetPrefixName()+'s';
#endif
  return OpalConnection::GetPrefixName();
}


static void StartHandleSignallingChannel(PSafePtr<H323Connection> h323)
{
  h323->HandleSignallingChannel();
}


PBoolean H323Connection::SetUpConnection()
{
  InternalSetAsOriginating();

  OnApplyStringOptions();

  PString alias;
  if (remotePartyName != m_remoteConnectAddress)
    alias = remotePartyName;

  CallEndReason reason = SendSignalSetup(alias, m_remoteConnectAddress);

  // Check if had an error, clear call if so
  if (reason != NumCallEndReasons) {
    Release(reason);
    return false;
  }

  m_signallingChannel->AttachThread(new PThread1Arg< PSafePtr<H323Connection> >(this, &StartHandleSignallingChannel, false, "H225 Caller"));
  return true;
}


OpalConnection::CallEndReason H323Connection::SendSignalSetup(const PString & alias,
                                                              const H323TransportAddress & address)
{
  PSafeLockReadWrite safeLock(*this);
  if (!safeLock.IsLocked())
    return EndedByCallerAbort;

  // Start the call, first state is asking gatekeeper
  connectionState = AwaitingGatekeeperAdmission;

  // See if we are "proxied" that is the destCallSignalAddress is different
  // from the transport connection address
  H323TransportAddress destCallSignalAddress = address;
  PINDEX atInAlias = alias.Find('@');
  if (atInAlias != P_MAX_INDEX)
    destCallSignalAddress = H323TransportAddress(alias.Mid(atInAlias+1), endpoint.GetDefaultSignalPort());

  // Initial value for alias in SETUP, could be overridden by gatekeeper
  H225_ArrayOf_AliasAddress newAliasAddresses;
  if (!alias.IsEmpty() && atInAlias > 0) {
    newAliasAddresses.SetSize(1);
    H323SetAliasAddress(alias.Left(atInAlias), newAliasAddresses[0]);
  }

  // Start building the setup PDU to get various ID's
  H323SignalPDU setupPDU;
  H225_Setup_UUIE & setup = setupPDU.BuildSetup(*this, destCallSignalAddress);

#if OPAL_H450
  h450dispatcher->AttachToSetup(setupPDU);
#endif

  // Save the identifiers generated by BuildSetup
  callReference = setupPDU.GetQ931().GetCallReference();
  conferenceIdentifier = setup.m_conferenceID;
  setupPDU.GetQ931().GetCalledPartyNumber(remotePartyNumber);

  H323TransportAddress gatekeeperRoute = address;

  // Check for gatekeeper and do admission check if have one
  H323Gatekeeper * gatekeeper = endpoint.GetGatekeeper();
  if (gatekeeper != NULL) {
    H323Gatekeeper::AdmissionResponse response;
    response.transportAddress = &gatekeeperRoute;
    response.aliasAddresses = &newAliasAddresses;
    if (!gkAccessTokenOID)
      response.accessTokenData = &gkAccessTokenData;
    for (;;) {
      safeLock.Unlock();
      PBoolean ok = gatekeeper->AdmissionRequest(*this, response, alias.IsEmpty());
      if (!safeLock.Lock() ||IsReleased())
        return EndedByCallerAbort;

      if (ok)
        break;

      PTRACE(2, "H225\tGatekeeper refused admission: "
             << (response.rejectReason == UINT_MAX
                  ? PString("Transport error")
                  : H225_AdmissionRejectReason(response.rejectReason).GetTagName()));
#if OPAL_H450
      h4502handler->onReceivedAdmissionReject(H4501_GeneralErrorList::e_notAvailable);
#endif

      switch (response.rejectReason) {
        case H225_AdmissionRejectReason::e_calledPartyNotRegistered :
          return EndedByNoUser;
        case H225_AdmissionRejectReason::e_requestDenied :
          return EndedByNoBandwidth;
        case H225_AdmissionRejectReason::e_invalidPermission :
        case H225_AdmissionRejectReason::e_securityDenial :
          return EndedBySecurityDenial;
        case H225_AdmissionRejectReason::e_resourceUnavailable :
          return EndedByRemoteBusy;
        case H225_AdmissionRejectReason::e_incompleteAddress :
          if (OnInsufficientDigits())
            break;
          // Then default case
        default :
          return EndedByGatekeeper;
      }

      PString lastRemotePartyName = remotePartyName;
      while (lastRemotePartyName == remotePartyName) {
        UnlockReadWrite(); // Release the mutex as can deadlock trying to clear call during connect.
        digitsWaitFlag.Wait();
        if (!LockReadWrite()) // Lock while checking for shutting down.
          return EndedByCallerAbort;
        if (IsReleased())
          return EndedByCallerAbort;
      }
    }
    mustSendDRQ = true;
    if (response.gatekeeperRouted) {
      setup.IncludeOptionalField(H225_Setup_UUIE::e_endpointIdentifier);
      setup.m_endpointIdentifier = gatekeeper->GetEndpointIdentifier();
      gatekeeperRouted = true;
    }
  }

  // Update the field e_destinationAddress in the SETUP PDU to reflect the new
  // alias received in the ACF (m_destinationInfo).
  if (newAliasAddresses.GetSize() > 0) {
    setup.IncludeOptionalField(H225_Setup_UUIE::e_destinationAddress);
    setup.m_destinationAddress = newAliasAddresses;

    // Update the Q.931 Information Element (if is an E.164 address)
    PString e164 = H323GetAliasAddressE164(newAliasAddresses);
    if (!e164)
      remotePartyNumber = e164;
  }

  if (addAccessTokenToSetup && !gkAccessTokenOID && !gkAccessTokenData.IsEmpty()) {
    PString oid1, oid2;
    if (!gkAccessTokenOID.Split(',', oid1, oid2))
      oid1 = oid2 = gkAccessTokenOID;
    setup.IncludeOptionalField(H225_Setup_UUIE::e_tokens);
    PINDEX last = setup.m_tokens.GetSize();
    setup.m_tokens.SetSize(last+1);
    setup.m_tokens[last].m_tokenOID = oid1;
    setup.m_tokens[last].IncludeOptionalField(H235_ClearToken::e_nonStandard);
    setup.m_tokens[last].m_nonStandard.m_nonStandardIdentifier = oid2;
    setup.m_tokens[last].m_nonStandard.m_data = gkAccessTokenData;
  }

  if (!m_signallingChannel->SetRemoteAddress(gatekeeperRoute)) {
    PTRACE(1, "H225\tInvalid "
           << (gatekeeperRoute != address ? "gatekeeper" : "user")
           << " supplied address: \"" << gatekeeperRoute << '"');
    connectionState = AwaitingTransportConnect;
    return EndedByConnectFail;
  }

#if OPAL_H460
  {
    /* When sending the H460 message in the Setup PDU you have to ensure the
       ARQ is received first then add the Fields to the Setup PDU */
    H225_FeatureSet fs;
    if (OnSendFeatureSet(H460_MessageType::e_setup, fs)) {
      if (fs.HasOptionalField(H225_FeatureSet::e_neededFeatures)) {
        setup.IncludeOptionalField(H225_Setup_UUIE::e_neededFeatures);
        H225_ArrayOf_FeatureDescriptor & fsn = setup.m_neededFeatures;
        fsn = fs.m_neededFeatures;
      }
  
      if (fs.HasOptionalField(H225_FeatureSet::e_desiredFeatures)) {
        setup.IncludeOptionalField(H225_Setup_UUIE::e_desiredFeatures);
        H225_ArrayOf_FeatureDescriptor & fsn = setup.m_desiredFeatures;
        fsn = fs.m_desiredFeatures;
      }
  
      if (fs.HasOptionalField(H225_FeatureSet::e_supportedFeatures)) {
        setup.IncludeOptionalField(H225_Setup_UUIE::e_supportedFeatures);
        H225_ArrayOf_FeatureDescriptor & fsn = setup.m_supportedFeatures;
        fsn = fs.m_supportedFeatures;
      }
    }
  }
#endif

  // Do the transport connect
  connectionState = AwaitingTransportConnect;

  // Release the mutex as can deadlock trying to clear call during connect.
  safeLock.Unlock();

  bool connectFailed = !m_signallingChannel->Connect();

    // Lock while checking for shutting down.
  if (!safeLock.Lock() || IsReleased())
    return EndedByCallerAbort;

  // See if transport connect failed, abort if so.
  if (connectFailed) {
    connectionState = NoConnectionActive;
    switch (m_signallingChannel->GetErrorNumber()) {
      case ENETUNREACH :
        return EndedByUnreachable;
      case ECONNREFUSED :
        return EndedByNoEndPoint;
      case ETIMEDOUT :
        return EndedByHostOffline;
    }
    return EndedByConnectFail;
  }

  PTRACE(3, "H225\tSending Setup PDU");
  connectionState = AwaitingSignalConnect;

  // Put in all the signalling addresses for link
  H323TransportAddress transportAddress = m_signallingChannel->GetLocalAddress();
  setup.IncludeOptionalField(H225_Setup_UUIE::e_sourceCallSignalAddress);
  transportAddress.SetPDU(setup.m_sourceCallSignalAddress);
  if (!setup.HasOptionalField(H225_Setup_UUIE::e_destCallSignalAddress)) {
    transportAddress = m_signallingChannel->GetRemoteAddress();
    setup.IncludeOptionalField(H225_Setup_UUIE::e_destCallSignalAddress);
    transportAddress.SetPDU(setup.m_destCallSignalAddress);
  }

  OnApplyStringOptions();

  // Get the local capabilities before fast start is handled
  OnSetLocalCapabilities();

  if (IsReleased())
    return EndedByCallerAbort;

  // Start channels, if allowed
  m_fastStartChannels.RemoveAll();
  if (m_fastStartState == FastStartInitiate) {
    PTRACE(3, "H225\tFast connect by local endpoint");
    OnSelectLogicalChannels();
  }

  // If application called OpenLogicalChannel, put in the fastStart field
  if (!m_fastStartChannels.IsEmpty()) {
    PTRACE(3, "H225\tFast start begun by local endpoint");
    for (H323LogicalChannelList::iterator channel = m_fastStartChannels.begin(); channel != m_fastStartChannels.end(); ++channel)
      BuildFastStartList(*channel, setup.m_fastStart, H323Channel::IsReceiver);
    if (setup.m_fastStart.GetSize() > 0)
      setup.IncludeOptionalField(H225_Setup_UUIE::e_fastStart);
    else
      m_fastStartChannels.RemoveAll();
  }

  SetOutgoingBearerCapabilities(setupPDU);

#if OPAL_H235_6
  {
    OpalMediaCryptoSuite::List cryptoSuites = OpalMediaCryptoSuite::FindAll(endpoint.GetMediaCryptoSuites(), "H.235");
    for (OpalMediaCryptoSuite::List::iterator it = cryptoSuites.begin(); it != cryptoSuites.end(); ++it)
      m_dh.AddForAlgorithm(*it);
    if (m_dh.ToTokens(setup.m_tokens))
      setup.IncludeOptionalField(H225_Setup_UUIE::e_tokens);
  }
#endif

  // Do this again (was done when PDU was constructed) in case
  // OnSendSignalSetup() changed something.
  setupPDU.SetQ931Fields(*this, true);
  setupPDU.GetQ931().GetCalledPartyNumber(remotePartyNumber);

  m_fastStartState = FastStartDisabled;
  PBoolean set_lastPDUWasH245inSETUP = false;

  if (h245Tunneling && doH245inSETUP && !endpoint.IsH245Disabled()) {
    h245TunnelTxPDU = &setupPDU;

    // Try and start the master/slave and capability exchange through the tunnel
    // Note: this used to be disallowed but is now allowed as of H323v4
    PBoolean ok = StartControlNegotiations();

    h245TunnelTxPDU = NULL;

    if (!ok)
      return EndedByTransportFail;

    if (doH245inSETUP && (setup.m_fastStart.GetSize() > 0)) {

      // Now if fast start as well need to put this in setup specific field
      // and not the generic H.245 tunneling field
      setup.IncludeOptionalField(H225_Setup_UUIE::e_parallelH245Control);
      setup.m_parallelH245Control = setupPDU.m_h323_uu_pdu.m_h245Control;
      setupPDU.m_h323_uu_pdu.RemoveOptionalField(H225_H323_UU_PDU::e_h245Control);
      set_lastPDUWasH245inSETUP = true;
    }
  }

  if (!OnSendSignalSetup(setupPDU))
    return EndedByNoAccept;

  // Send the initial PDU
  if (!WriteSignalPDU(setupPDU))
    return EndedByTransportFail;

  SetPhase(SetUpPhase);

  // WriteSignalPDU always resets lastPDUWasH245inSETUP.
  // So set it here if required
  if (set_lastPDUWasH245inSETUP)
    lastPDUWasH245inSETUP = true;

  // Set timeout for remote party to answer the call
  m_signallingChannel->SetReadTimeout(endpoint.GetSignallingChannelCallTimeout());

  connectionState = AwaitingSignalConnect;

  return NumCallEndReasons;
}


void H323Connection::SetOutgoingBearerCapabilities(H323SignalPDU & pdu) const
{
  PString bearerCaps = m_stringOptions(OPAL_OPT_Q931_BEARER_CAPS);

  if (bearerCaps.IsEmpty()) {
    // Search the capability set and see if we have video capability
    for (PINDEX i = 0; i < localCapabilities.GetSize(); i++) {
      if (!PIsDescendant(&localCapabilities[i], H323AudioCapability) &&
          !PIsDescendant(&localCapabilities[i], H323_UserInputCapability)) {
        bearerCaps = "Digital"; // Unrestricted digital, video or other
        break;
      }
    }

    if (bearerCaps.IsEmpty())
      bearerCaps = "Speech";

    unsigned transferRate = std::max(GetBandwidthAvailable(OpalBandwidth::Rx),GetBandwidthAvailable(OpalBandwidth::Tx))/64000;
    if (transferRate > 127)
      transferRate = 127;
    else if (transferRate == 0)
      transferRate = 1;
    bearerCaps.sprintf(",%u", transferRate);
    PTRACE(4, "H225\tSet bandwidth in Q.931 caps: " << transferRate << " bearers");
  }

  pdu.GetQ931().SetBearerCapabilities(bearerCaps);
}


void H323Connection::SetIncomingBearerCapabilities(const H323SignalPDU & pdu)
{
  // Make sure we clamp our bandwidth to whatever they said
  Q931::InformationTransferCapability bearerCap;
  unsigned transferRate;
  if (pdu.GetQ931().GetBearerCapabilities(bearerCap, transferRate)) {
    PTRACE(4, "H225\tSet bandwidth from Q.931 caps: " << transferRate << " bearers");
    OpalBandwidth newBandwidth = transferRate*64000;
    if (GetBandwidthAvailable(OpalBandwidth::Rx) > newBandwidth)
      SetBandwidthAvailable(OpalBandwidth::Rx, newBandwidth);
    if (GetBandwidthAvailable(OpalBandwidth::Tx) > newBandwidth)
      SetBandwidthAvailable(OpalBandwidth::Tx, newBandwidth);
  }
}


PBoolean H323Connection::OnSendSignalSetup(H323SignalPDU & pdu)
{
  return endpoint.OnSendSignalSetup(*this, pdu);
}


PBoolean H323Connection::OnSendCallProceeding(H323SignalPDU & callProceedingPDU)
{
  return endpoint.OnSendCallProceeding(*this, callProceedingPDU);
}


void H323Connection::DetermineRTPNAT(const OpalTransport & transport, const OpalTransportAddress & signalAddr)
{
#if OPAL_H460_NAT
  if (m_features != NULL) {
    H460_Feature * feature = m_features->GetFeature(H460_FeatureStd19::ID());
    if (feature != NULL && feature->IsNegotiated()) {
      m_remoteBehindNAT = true;
      return;
    }
  }
#endif

  OpalRTPConnection::DetermineRTPNAT(transport, signalAddr);
}


PBoolean H323Connection::OnSendReleaseComplete(H323SignalPDU & /*releaseCompletePDU*/)
{
  return true;
}


PBoolean H323Connection::OnAlerting(const H323SignalPDU & alertingPDU,
                                const PString & username)
{
  return endpoint.OnAlerting(*this, alertingPDU, username);
}


PBoolean H323Connection::SetAlerting(const PString & calleeName, PBoolean withMedia)
{
  PSafeLockReadWrite safeLock(*this);
  if (!safeLock.IsLocked())
    return false;

  if (GetPhase() >= ConnectedPhase || alertingPDU == NULL) {
    PTRACE(3, "H323\tNo Alerting possible for " << *this);
    return false;
  }

  PTRACE(3, "H323\tSetAlerting " << (withMedia ? "with media" : "normal") << ' ' << *this);
  H225_Alerting_UUIE & alerting = alertingPDU->m_h323_uu_pdu.m_h323_message_body;
  alerting.m_maintainConnection = m_maintainConnection;

  if (withMedia && !mediaWaitForConnect) {
    if (SendFastStartAcknowledge(alerting.m_fastStart))
      alerting.IncludeOptionalField(H225_Alerting_UUIE::e_fastStart);
    earlyStart = true;
  }

  bool startH245 = !endpoint.IsH245Disabled();
  if (startH245 && localCapabilities.GetSize() == 0) {
    OnSetLocalCapabilities();
    if (localCapabilities.GetSize() == 0)
      startH245 = false; // Don't do early media if don't have any capabilities
  }

  // Do early H.245 start
  if (startH245 && !h245Tunneling && m_controlChannel == NULL) {
    if (!CreateIncomingControlChannel(alerting,
                                      alerting.m_h245Address, H225_Alerting_UUIE::e_h245Address,
                                      alerting.m_h245SecurityMode, H225_Alerting_UUIE::e_h245SecurityMode))
      return false;
  }

#if OPAL_H450
  h450dispatcher->AttachToAlerting(*alertingPDU);
#endif

  HandleTunnelPDU(alertingPDU);

  if (!endpoint.OnSendAlerting(*this, *alertingPDU, calleeName, withMedia)) {
    /* let the application to avoid sending the alerting, mainly for testing other endpoints*/
    PTRACE(3, "H323CON\tSetAlerting Alerting not sent");
    return true;
  }

  // send Q931 Alerting PDU
  PTRACE(3, "H323CON\tSetAlerting sending Alerting PDU");

  if (!WriteSignalPDU(*alertingPDU))
    return false;

  SetPhase(AlertingPhase);

  if (!endpoint.OnSentAlerting(*this))
    return false;

  // Do early H.245 start
  if (startH245 && h245Tunneling) {
    if (!StartControlNegotiations())
      return false;
  }

  InternalEstablishedConnectionCheck();
  return true;
}


PBoolean H323Connection::SetConnected()
{
  PSafeLockReadWrite safeLock(*this);
  if (!safeLock.IsLocked())
    return false;

  if (GetPhase() >= ConnectedPhase || connectPDU == NULL) {
    PTRACE(3, "H323\tNo Connect possible for " << *this);
    return false;
  }

  mediaWaitForConnect = false;

  PTRACE(3, "H323CON\tSetConnected " << *this);

  if (!endpoint.OnSendConnect(*this, *connectPDU)){
    /* let the application to avoid sending the connect, mainly for testing other endpoints*/
    PTRACE(2, "H323CON\tSetConnected connect not sent");
    return true;
  }

  // Assure capabilities are set to other connections media list (if not already)
  OnSetLocalCapabilities();

  // Must be after OnSetLocalCapabilities
  SetOutgoingBearerCapabilities(*connectPDU);

  H225_Connect_UUIE & connect = connectPDU->m_h323_uu_pdu.m_h323_message_body;
  connect.m_maintainConnection = m_maintainConnection;

  // Reply fast connect, make sure same as previously sent ALERTING or PROGRESS if present
  const H225_ArrayOf_PASN_OctetString * fastStart = NULL;

  if (alertingPDU != NULL) {
    const H225_Alerting_UUIE & alerting = alertingPDU->m_h323_uu_pdu.m_h323_message_body;
    if (alerting.m_fastStart.GetSize() > 0)
      fastStart = &alerting.m_fastStart;
  }
  if (progressPDU != NULL) {
    const H225_Progress_UUIE & progress = progressPDU->m_h323_uu_pdu.m_h323_message_body;
    if (progress.m_fastStart.GetSize() > 0)
      fastStart = &progress.m_fastStart;
  }

  if (fastStart != NULL)
    connect.m_fastStart = *fastStart;
  else {
    // Not done yey, ask the application to select which channels to start
    SendFastStartAcknowledge(connect.m_fastStart);
  }

  if (connect.m_fastStart.GetSize() > 0)
    connect.IncludeOptionalField(H225_Connect_UUIE::e_fastStart);

  // See if aborted call
  if (connectionState == ShuttingDownConnection)
    return false;

  // Set flag that we are up to CONNECT stage
  connectionState = HasExecutedSignalConnect;
  SetPhase(ConnectedPhase);

#if OPAL_H450
  h450dispatcher->AttachToConnect(*connectPDU);
#endif

  if (!endpoint.IsH245Disabled()){
    if (h245Tunneling) {
      HandleTunnelPDU(connectPDU);

      // If no channels selected (or never provided) do traditional H245 start
      if (m_fastStartState == FastStartDisabled) {
        h245TunnelTxPDU = connectPDU; // Piggy back H245 on this reply
        PBoolean ok = StartControlNegotiations();
        h245TunnelTxPDU = NULL;
        if (!ok)
          return false;
      }
    }
    else if (m_controlChannel == NULL) { // Start separate H.245 channel if not tunneling.
      if (!CreateIncomingControlChannel(connect,
                                        connect.m_h245Address, H225_Connect_UUIE::e_h245Address,
                                        connect.m_h245SecurityMode, H225_Connect_UUIE::e_h245SecurityMode))
        return false;
    }
  }

  if (!WriteSignalPDU(*connectPDU)) // Send H323 Connect PDU
    return false;

  delete connectPDU;
  connectPDU = NULL;
  delete alertingPDU;
  alertingPDU = NULL;

  InternalEstablishedConnectionCheck();
  return true;
}

PBoolean H323Connection::SetProgressed()
{
  PSafeLockReadWrite safeLock(*this);
  if (!safeLock.IsLocked())
    return false;

  if (GetPhase() >= ConnectedPhase || progressPDU == NULL) {
    PTRACE(3, "H323\tNo Progress possible for " << *this);
    return false;
  }

  PTRACE(3, "H323\tSetProgressed " << *this);

  mediaWaitForConnect = false;

  // Assure capabilities are set to other connections media list (if not already)
  OnSetLocalCapabilities();

  H225_Progress_UUIE & progress = progressPDU->m_h323_uu_pdu.m_h323_message_body;
  progress.m_maintainConnection = m_maintainConnection;

  // Now ask the application to select which channels to start
  if (SendFastStartAcknowledge(progress.m_fastStart))
    progress.IncludeOptionalField(H225_Connect_UUIE::e_fastStart);

  //h450dispatcher->AttachToProgress(*progress);
  if (!endpoint.IsH245Disabled()){
    if (h245Tunneling) {
      HandleTunnelPDU(progressPDU);

      // If no channels selected (or never provided) do traditional H245 start
      if (m_fastStartState == FastStartDisabled) {
        h245TunnelTxPDU = progressPDU; // Piggy back H245 on this reply
        PBoolean ok = StartControlNegotiations();
        h245TunnelTxPDU = NULL;
        if (!ok)
          return false;
      }
    }
    else if (!m_controlChannel) { // Start separate H.245 channel if not tunneling.
      if (!CreateIncomingControlChannel(progress,
                                        progress.m_h245Address, H225_Progress_UUIE::e_h245Address,
                                        progress.m_h245SecurityMode, H225_Progress_UUIE::e_h245SecurityMode))
        return false;
    }
  }

  if (!WriteSignalPDU(*progressPDU)) // Send H323 Progress PDU
    return false;

  InternalEstablishedConnectionCheck();
  return true;
}


PBoolean H323Connection::OnInsufficientDigits()
{
  return false;
}


void H323Connection::SendMoreDigits(const PString & digits)
{
  remotePartyNumber += digits;
  remotePartyName = remotePartyNumber;
  if (connectionState == AwaitingGatekeeperAdmission)
    digitsWaitFlag.Signal();
  else {
    H323SignalPDU infoPDU;
    infoPDU.BuildInformation(*this);
    infoPDU.GetQ931().SetCalledPartyNumber(digits);
    if (!WriteSignalPDU(infoPDU))
      Release(EndedByTransportFail);
  }
}


PBoolean H323Connection::OnOutgoingCall(const H323SignalPDU & connectPDU)
{
  return endpoint.OnOutgoingCall(*this, connectPDU);
}


PBoolean H323Connection::SendFastStartAcknowledge(H225_ArrayOf_PASN_OctetString & fastStartReply)
{
  // See if we have already added the fast start OLC's
  if (fastStartReply.GetSize() > 0) {
    PTRACE(4, "H323\tAlready have fast connect reply");
    return true;
  }

  if (m_fastStartState == FastStartDisabled) {
    PTRACE(4, "H323\tFast connect disabled, no acknowdgement");
    return false;
  }

  if (m_fastStartState == FastStartAcknowledged) {
    PTRACE(4, "H323\tFast connect already acknowdgement");
    return true;
  }

  if (m_fastStartChannels.IsEmpty()) {
    // If we are capable of ANY of the fast start channels, don't do fast start
    PTRACE(4, "H323\tNo fast connect offerred");
    m_fastStartState = FastStartDisabled;
    return false;
  }

  // Assure capabilities are set to other connections media list (if not already)
  OnSetLocalCapabilities();

  // See if we need to select our fast start channels
  if (m_fastStartState == FastStartResponse)
    OnSelectLogicalChannels();

  // Remove any channels that were not started by OnSelectLogicalChannels(),
  // those that were started are put into the logical channel dictionary
  for (H323LogicalChannelList::iterator channel = m_fastStartChannels.begin(); channel != m_fastStartChannels.end(); ) {
    if (logicalChannels->FindChannel(channel->GetNumber(), channel->GetNumber().IsFromRemote()) == &*channel)
      ++channel;
    else
      m_fastStartChannels.erase(channel++); // Do ++ in both legs so iterator works with erase
  }

  // None left, so didn't open any channels fast
  if (m_fastStartChannels.IsEmpty()) {
    PTRACE(4, "H323\tCould not use any offerred fast connect channels");
    m_fastStartState = FastStartDisabled;
    return false;
  }

  // The channels we just transferred to the logical channels dictionary
  // should not be deleted via this structure now.
  m_fastStartChannels.DisallowDeleteObjects();

  PTRACE(3, "H225\tAccepting fastStart for " << m_fastStartChannels.GetSize() << " channels");

  for (H323LogicalChannelList::iterator channel = m_fastStartChannels.begin(); channel != m_fastStartChannels.end(); ++channel)
    BuildFastStartList(*channel, fastStartReply, H323Channel::IsTransmitter);

  // Have moved open channels to logicalChannels structure, remove all others.
  m_fastStartChannels.RemoveAll();

  // Set flag so internal establishment check does not require H.245
  m_fastStartState = FastStartAcknowledged;

  return true;
}


PBoolean H323Connection::HandleFastStartAcknowledge(const H225_ArrayOf_PASN_OctetString & array)
{
  if (m_fastStartState == FastStartAcknowledged)
    return true;

  if (m_fastStartChannels.IsEmpty()) {
    PTRACE(2, "H225\tFast start response with no channels to open");
    return false;
  }

  PTRACE(3, "H225\tFast start accepted by remote endpoint");

  PINDEX i;

  H323LogicalChannelList replyFastStartChannels;

  // Go through provided list of structures, if can decode it and match it up
  // with a channel we requested AND it has all the information needed in the
  // m_multiplexParameters, then we can start the channel.
  for (i = 0; i < array.GetSize(); i++) {
    H245_OpenLogicalChannel open;
    if (array[i].DecodeSubType(open)) {
      PTRACE(4, "H225\tFast start open:\n  " << setprecision(2) << open);
      PBoolean reverse = open.HasOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters);
      const H245_DataType & dataType = reverse ? open.m_reverseLogicalChannelParameters.m_dataType
                                               : open.m_forwardLogicalChannelParameters.m_dataType;
      H323Capability * replyCapability = localCapabilities.FindCapability(dataType);
      if (replyCapability != NULL) {
        for (H323LogicalChannelList::iterator channel = m_fastStartChannels.begin(); channel != m_fastStartChannels.end(); ++channel) {
          H323Channel & channelToStart = *channel;
          H323Channel::Directions dir = channelToStart.GetDirection();
          if ((dir == H323Channel::IsReceiver) == reverse &&
               channelToStart.GetCapability() == *replyCapability) {
            unsigned error = 1000;
            if (channelToStart.OnReceivedPDU(open, error)) {
              H323Capability * channelCapability;
              if (dir == H323Channel::IsReceiver)
                channelCapability = replyCapability;
              else {
                // For transmitter, need to fake a capability into the remote table
                channelCapability = remoteCapabilities.FindCapability(channelToStart.GetCapability());
                if (channelCapability == NULL) {
                  channelCapability = remoteCapabilities.Copy(channelToStart.GetCapability());
                  remoteCapabilities.SetCapability(0, channelCapability->GetDefaultSessionID()-1, channelCapability);
                }
              }
              // Must use the actual capability instance from the
              // localCapability or remoteCapability structures.
              if (OnCreateLogicalChannel(*channelCapability, dir, error)) {
                if (channelToStart.SetInitialBandwidth()) {
                  PTRACE(4, "H225\tFast start channel opened: " << *channel);
                  replyFastStartChannels.Append(&*channel);
                  m_fastStartChannels.DisallowDeleteObjects();
                  m_fastStartChannels.erase(channel);
                  m_fastStartChannels.AllowDeleteObjects();
                  break;
                }
                else
                  PTRACE(2, "H225\tFast start channel open fail: insufficent bandwidth");
              }
              else
                PTRACE(2, "H225\tFast start channel open error: " << error);
            }
            else
              PTRACE(2, "H225\tFast start capability error: " << error);
          }
        }
      }
    }
    else {
      PTRACE(1, "H225\tInvalid fast start PDU decode:\n  " << setprecision(2) << open);
    }
  }

  // Delete all the ones we couldn't open
  m_fastStartChannels.RemoveAll();

  PTRACE(3, "H225\tFast start opening " << replyFastStartChannels.GetSize() << " channels");
  if (replyFastStartChannels.IsEmpty())
    return false;

  m_fastStartState = FastStartAcknowledged;

  /* Need to put the opened channels back into the m_fastStartChannels member
     so the OpalMediaStream opening stuff can see them */
  m_fastStartChannels = replyFastStartChannels;

  for (H323LogicalChannelList::iterator channel = m_fastStartChannels.begin(); channel != m_fastStartChannels.end(); ++channel)
    channel->Open();

  /* The channels were transferred to the logical channels dictionary and
     should not be deleted via this structure now. */
  m_fastStartChannels.DisallowDeleteObjects();
  m_fastStartChannels.RemoveAll();

  StartMediaStreams();
  return true;
}


PBoolean H323Connection::OnUnknownSignalPDU(const H323SignalPDU & PTRACE_PARAM(pdu))
{
  PTRACE(2, "H225\tUnknown signalling PDU: " << pdu);
  return true;
}


PBoolean H323Connection::CreateOutgoingControlChannel(const PASN_Sequence & enclosingPDU,
                                                      const H225_TransportAddress & h245Address,
                                                      unsigned h245AddressField,
#if OPAL_PTLIB_SSL
                                                      const H225_H245Security & h245Security,
                                                      unsigned h245SecurityField
#else
                                                      const H225_H245Security &,
                                                      unsigned
#endif
                                                      )
{
  if (endpoint.IsH245Disabled())
    return true;

  if (h245Tunneling && !endpoint.IsH245TunnelingDisabled())
    return true;

  if (!enclosingPDU.HasOptionalField(h245AddressField))
    return true;

  PTRACE(3, "H225\tCreateOutgoingControlChannel h245Address = " << h245Address);
  if (endpoint.IsH245Disabled()){
    PTRACE(2, "H225\tCreateOutgoingControlChannel h245 is disabled, do nothing");
    /* return true to act as if it was succeded*/
    return true;
  }
  // Already have the H245 channel up.
  if (m_controlChannel != NULL)
    return true;

  PIPAddress localInterface(m_signallingChannel->GetInterface());
#if OPAL_PTLIB_SSL
  if (enclosingPDU.HasOptionalField(h245SecurityField)) {
    if (h245Security.GetTag() != H225_H245Security::e_tls) {
      PTRACE(2, "H225\tUnsupported H.245 security mode");
      return false;
    }

    const H225_SecurityCapabilities & secCap = h245Security;
    if (secCap.m_encryption.GetTag() != H225_SecurityServiceMode::e_default ||
        secCap.m_authenticaton.GetTag() != H225_SecurityServiceMode::e_default ||
        secCap.m_integrity.GetTag() != H225_SecurityServiceMode::e_default) {
      PTRACE(2, "H225\tUnsupported H.245 security capabilities");
      return false;
    }

    m_controlChannel = new OpalTransportTLS(endpoint, localInterface);
  }
  else
#endif
    m_controlChannel = new OpalTransportTCP(endpoint, localInterface);

  if (m_controlChannel == NULL) {
    PTRACE(1, "H225\tConnect of H245 failed: Unsupported transport");
    return false;
  }

  PTRACE_CONTEXT_ID_TO(m_controlChannel);

  if (!m_controlChannel->SetRemoteAddress(H323TransportAddress(h245Address))) {
    PTRACE(1, "H225\tCould not extract H245 address");
    m_controlChannel.SetNULL();
    return false;
  }

  if (!m_controlChannel->Connect()) {
    PTRACE(1, "H225\tConnect of H245 failed: " << m_controlChannel->GetErrorText());
    m_controlChannel.SetNULL();
    return false;
  }

  m_controlChannel->AttachThread(PThread::Create(PCREATE_NOTIFIER(NewOutgoingControlChannel), "H.245 Handler"));
  return true;
}


void H323Connection::NewOutgoingControlChannel(PThread &, P_INT_PTR)
{
  if (PAssertNULL(m_controlChannel) == NULL)
    return;

  if (!SafeReference())
    return;

  HandleControlChannel();
  SafeDereference();
}


PBoolean H323Connection::CreateIncomingControlChannel(PASN_Sequence & enclosingPDU,
                                                      H225_TransportAddress & h245Address,
                                                      unsigned h245AddressField,
#if OPAL_PTLIB_SSL
                                                      H225_H245Security & h245Security,
                                                      unsigned h245SecurityField
#else
                                                      H225_H245Security &,
                                                      unsigned
#endif
                                                      )
{
  PAssert(m_controlChannel == NULL, PLogicError);

  if (endpoint.IsH245Disabled()){
    PTRACE(2, "H225\tCreateIncomingControlChannel: do not create channel because h245 is disabled");
    return false;
  }

  if (controlListener == NULL) {
    controlListener = m_signallingChannel->GetLocalAddress().CreateListener(endpoint, OpalTransportAddress::HostOnly);
    if (controlListener == NULL)
      return false;

    PTRACE_CONTEXT_ID_TO(controlListener);

    if (!controlListener->Open(PCREATE_NOTIFIER(NewIncomingControlChannel), OpalListener::HandOffThreadMode)) {
      delete controlListener;
      controlListener = NULL;
      return false;
    }
  }

  H323TransportAddress listeningAddress = controlListener->GetLocalAddress(m_signallingChannel->GetRemoteAddress());

  // assign address into the PDU
  if (!listeningAddress.SetPDU(h245Address))
    return false;

  enclosingPDU.IncludeOptionalField(h245AddressField);

#if OPAL_PTLIB_SSL
  if (listeningAddress.GetProtoPrefix() == OpalTransportAddress::TlsPrefix()) {
    enclosingPDU.IncludeOptionalField(h245SecurityField);
    h245Security.SetTag(H225_H245Security::e_tls);
    H225_SecurityCapabilities & secCap = h245Security;
    secCap.m_encryption.SetTag(H225_SecurityServiceMode::e_default);
    secCap.m_authenticaton.SetTag(H225_SecurityServiceMode::e_default);
    secCap.m_integrity.SetTag(H225_SecurityServiceMode::e_default);
  }
#endif
  return true;
}


void H323Connection::NewIncomingControlChannel(OpalListener & listener, const OpalTransportPtr & transport)
{
  listener.Close();

  if (transport == NULL) {
    // If H.245 channel failed to connect and have no media (no fast start)
    // then clear the call as it is useless.
    if (mediaStreams.IsEmpty())
      Release(EndedByTransportFail);
    return;
  }

  if (!SafeReference())
    return;

  m_controlChannel = transport;
  HandleControlChannel();
  SafeDereference();
}


PBoolean H323Connection::WriteControlPDU(const H323ControlPDU & pdu)
{
  PPER_Stream strm;
  pdu.Encode(strm);
  strm.CompleteEncoding();

  H323TraceDumpPDU("H245", true, strm, pdu, pdu, 0);

  if (!h245Tunneling) {
    if (m_controlChannel == NULL) {
      PTRACE(1, "H245\tWrite PDU fail: no control channel.");
      return false;
    }

    if (m_controlChannel->IsOpen() && m_controlChannel->WritePDU(strm))
      return true;

    PTRACE(1, "H245\tWrite PDU fail: " << m_controlChannel->GetErrorText(PChannel::LastWriteError));
    return false;
  }

  // If have a pending signalling PDU, use it rather than separate write
  H323SignalPDU localTunnelPDU;
  H323SignalPDU * tunnelPDU;
  if (h245TunnelTxPDU != NULL)
    tunnelPDU = h245TunnelTxPDU;
  else {
    localTunnelPDU.BuildFacility(*this, true);
    tunnelPDU = &localTunnelPDU;
  }

  tunnelPDU->m_h323_uu_pdu.IncludeOptionalField(H225_H323_UU_PDU::e_h245Control);
  PINDEX last = tunnelPDU->m_h323_uu_pdu.m_h245Control.GetSize();
  tunnelPDU->m_h323_uu_pdu.m_h245Control.SetSize(last+1);
  tunnelPDU->m_h323_uu_pdu.m_h245Control[last] = strm;

  if (h245TunnelTxPDU != NULL)
    return true;

  return WriteSignalPDU(localTunnelPDU);
}


PBoolean H323Connection::StartControlNegotiations()
{
  PTRACE(3, "H245\tStarted control channel");


  if (endpoint.IsH245Disabled()){
    PTRACE(2, "H245\tStartControlNegotiations h245 is disabled, do not start negotiation");
    return false;
  }
  // Get the local capabilities before fast start is handled
  OnSetLocalCapabilities();

  // Begin the capability exchange procedure
  if (!capabilityExchangeProcedure->Start(false)) {
    PTRACE(1, "H245\tStart of Capability Exchange failed");
    return false;
  }

  // Begin the Master/Slave determination procedure
  if (!masterSlaveDeterminationProcedure->Start(false)) {
    PTRACE(1, "H245\tStart of Master/Slave determination failed");
    return false;
  }

  m_endSessionNeeded = true;
  return true;
}

PBoolean H323Connection::OnStartHandleControlChannel()
{
  PSafeLockReadWrite mutex(*this);

  PTRACE(2, "H46018\tStarted control channel");

#if OPAL_H460_NAT
  if (m_features != NULL) {
    H460_FeatureStd18 * feature;
    if (m_features->GetFeature(feature)) {
      if (!feature->OnStartControlChannel())
        return false;

      m_controlChannel->SetKeepAlive(endpoint.GetManager().GetNatKeepAliveTime(), PBYTEArray(EmptyTPKT, sizeof(EmptyTPKT), false));
    }
  }
#endif

  return StartHandleControlChannel();
}


PBoolean H323Connection::HandleReceivedControlPDU(PBoolean readStatus, PPER_Stream & strm)
{
  if (readStatus) {
    // Lock while checking for shutting down.
    if (!LockReadWrite())
      return InternalEndSessionCheck(strm);

    // Process the received PDU
    PTRACE(4, "H245\tReceived TPKT: " << strm);
    bool ok = HandleControlData(strm);
    UnlockReadWrite(); // Unlock connection
    return ok;
  }


  if (m_controlChannel->GetErrorCode() == PChannel::Timeout) {
    PTRACE(4, "H245\tRead timeout");
    return true;
  }

  PTRACE_IF(1, m_controlChannel->GetErrorCode() != PChannel::NotOpen,
            "H245\tRead error: " << m_controlChannel->GetErrorText(PChannel::LastReadError));

  // If the connection is already shutting down then don't overwrite the
  // call end reason.  This could happen if the remote end point misbehaves
  // and simply closes the H.245 TCP connection rather than sending an
  // endSession.
  PTRACE(4, "H245\tChannel closed: endSessionNeeded=" << m_endSessionNeeded);
  if (!IsReleased())
    Release(EndedByTransportFail);

  return false;
}


PBoolean H323Connection::StartHandleControlChannel()
{
  // Start the TCS and MSD operations on new H.245 channel.
  if (!StartControlNegotiations())
    return FALSE;

  // Disable the signalling channels timeout for monitoring call status and
  // start up one in this thread instead. Then the Q.931 channel can be closed
  // without affecting the call.
  m_signallingChannel->SetReadTimeout(PMaxTimeInterval);
  m_controlChannel->SetReadTimeout(MonitorCallStatusTime);

  return TRUE;
}


void H323Connection::EndHandleControlChannel()
{
  PSafeLockReadOnly mutex(*this);

  // If we are the only link to the far end or if we have already sent our
  // endSession command then indicate that we have received endSession even
  // if we hadn't, because we are now never going to get one so there is no
  // point in having CleanUpOnCallEnd wait.
  if (m_signallingChannel == NULL) {
    PTRACE(3, "H245\tChannel closed without H.225 channel, releasing H.245 endSession wait");
    endSessionReceived.Signal();
  }
}


void H323Connection::HandleControlChannel()
{
  PTRACE_CONTEXT_ID_PUSH_THREAD(this);

  // If have started separate H.245 channel then don't tunnel any more
  h245Tunneling = FALSE;

  if (!OnStartHandleControlChannel())
    return;

  PBoolean ok = TRUE;
  while (ok) {
    MonitorCallStatus();
    PPER_Stream strm;
    bool readStatus = m_controlChannel->ReadPDU(strm);
    ok = HandleReceivedControlPDU(readStatus, strm);
  }

  EndHandleControlChannel();

  PTRACE(2, "H245\tControl channel closed.");
}


bool H323Connection::InternalEndSessionCheck(PPER_Stream & strm)
{
  H323ControlPDU pdu;

  if (!pdu.Decode(strm)) {
    PTRACE(1, "H245\tInvalid PDU decode:\n  " << setprecision(2) << pdu);
    return false;
  }

  PTRACE(3, "H245\tChecking for end session on PDU: " << pdu.GetTagName()
         << ' ' << ((PASN_Choice &)pdu.GetObject()).GetTagName());

  if (pdu.GetTag() != H245_MultimediaSystemControlMessage::e_command)
    return true;

  H245_CommandMessage & command = pdu;
  if (command.GetTag() != H245_CommandMessage::e_endSessionCommand)
    return true;

  endSessionReceived.Signal();
  return SendReleaseComplete();
}


PBoolean H323Connection::HandleControlData(PPER_Stream & strm)
{
  while (!strm.IsAtEnd()) {
    H323ControlPDU pdu;
    if (!pdu.Decode(strm)) {
      PTRACE(1, "H245\tInvalid PDU decode!"
                "\nRaw PDU:\n" << hex << setfill('0')
                               << setprecision(2) << strm
                               << dec << setfill(' ') <<
                "\nPartial PDU:\n  " << setprecision(2) << pdu);
      return true;
    }

    H323TraceDumpPDU("H245", false, strm, pdu, pdu, 0);

    if (!HandleControlPDU(pdu))
      return false;

    InternalEstablishedConnectionCheck();

    strm.ByteAlign();
  }

  return true;
}


PBoolean H323Connection::HandleControlPDU(const H323ControlPDU & pdu)
{
  switch (pdu.GetTag()) {
    case H245_MultimediaSystemControlMessage::e_request :
      return OnH245Request(pdu);

    case H245_MultimediaSystemControlMessage::e_response :
      return OnH245Response(pdu);

    case H245_MultimediaSystemControlMessage::e_command :
      return OnH245Command(pdu);

    case H245_MultimediaSystemControlMessage::e_indication :
      return OnH245Indication(pdu);
  }

  return OnUnknownControlPDU(pdu);
}


PBoolean H323Connection::OnUnknownControlPDU(const H323ControlPDU & pdu)
{
  PTRACE(2, "H245\tUnknown Control PDU: " << pdu);

  H323ControlPDU reply;
  reply.BuildFunctionNotUnderstood(pdu);
  return WriteControlPDU(reply);
}


PBoolean H323Connection::OnH245Request(const H323ControlPDU & pdu)
{
  const H245_RequestMessage & request = pdu;

  switch (request.GetTag()) {
    case H245_RequestMessage::e_masterSlaveDetermination :
      return masterSlaveDeterminationProcedure->HandleIncoming(request);

    case H245_RequestMessage::e_terminalCapabilitySet :
    {
      const H245_TerminalCapabilitySet & tcs = request;
      if (tcs.m_protocolIdentifier.GetSize() >= 6) {
        h245version = std::min(tcs.m_protocolIdentifier[5], h245version);
        PTRACE_IF(3, !h245versionSet, "H245\tSet protocol version to " << h245version);
        h245versionSet = true;
      }
      return capabilityExchangeProcedure->HandleIncoming(tcs);
    }

    case H245_RequestMessage::e_openLogicalChannel :
      return logicalChannels->HandleOpen(request);

    case H245_RequestMessage::e_closeLogicalChannel :
      return logicalChannels->HandleClose(request);

    case H245_RequestMessage::e_requestChannelClose :
      return logicalChannels->HandleRequestClose(request);

    case H245_RequestMessage::e_requestMode :
      return requestModeProcedure->HandleRequest(request);

    case H245_RequestMessage::e_roundTripDelayRequest :
      return roundTripDelayProcedure->HandleRequest(request);

#if OPAL_H239
    case H245_RequestMessage::e_genericRequest :
    {
      const H245_GenericMessage & gen = request;
      if (H323GetCapabilityIdentifier(gen.m_messageIdentifier) == H239MessageOID)
        return OnH239Message(gen.m_subMessageIdentifier, gen.m_messageContent);
    }
#endif
  }

  return OnUnknownControlPDU(pdu);
}


PBoolean H323Connection::OnH245Response(const H323ControlPDU & pdu)
{
  const H245_ResponseMessage & response = pdu;

  switch (response.GetTag()) {
    case H245_ResponseMessage::e_masterSlaveDeterminationAck :
      return masterSlaveDeterminationProcedure->HandleAck(response);

    case H245_ResponseMessage::e_masterSlaveDeterminationReject :
      return masterSlaveDeterminationProcedure->HandleReject(response);

    case H245_ResponseMessage::e_terminalCapabilitySetAck :
      return capabilityExchangeProcedure->HandleAck(response);

    case H245_ResponseMessage::e_terminalCapabilitySetReject :
      return capabilityExchangeProcedure->HandleReject(response);

    case H245_ResponseMessage::e_openLogicalChannelAck :
      return logicalChannels->HandleOpenAck(response);

    case H245_ResponseMessage::e_openLogicalChannelReject :
      return logicalChannels->HandleReject(response);

    case H245_ResponseMessage::e_closeLogicalChannelAck :
      return logicalChannels->HandleCloseAck(response);

    case H245_ResponseMessage::e_requestChannelCloseAck :
      return logicalChannels->HandleRequestCloseAck(response);

    case H245_ResponseMessage::e_requestChannelCloseReject :
      return logicalChannels->HandleRequestCloseReject(response);

    case H245_ResponseMessage::e_requestModeAck :
      return requestModeProcedure->HandleAck(response);

    case H245_ResponseMessage::e_requestModeReject :
      return requestModeProcedure->HandleReject(response);

    case H245_ResponseMessage::e_roundTripDelayResponse :
      return roundTripDelayProcedure->HandleResponse(response);

#if OPAL_H239
    case H245_ResponseMessage::e_genericResponse :
    {
      const H245_GenericMessage & gen = response;
      if (H323GetCapabilityIdentifier(gen.m_messageIdentifier) == H239MessageOID)
        return OnH239Message(gen.m_subMessageIdentifier, gen.m_messageContent);
    }
#endif
  }

  return OnUnknownControlPDU(pdu);
}


PBoolean H323Connection::OnH245Command(const H323ControlPDU & pdu)
{
  const H245_CommandMessage & command = pdu;

  switch (command.GetTag()) {
    case H245_CommandMessage::e_sendTerminalCapabilitySet :
      return OnH245_SendTerminalCapabilitySet(command);

    case H245_CommandMessage::e_flowControlCommand :
      return OnH245_FlowControlCommand(command);

    case H245_CommandMessage::e_miscellaneousCommand :
      return OnH245_MiscellaneousCommand(command);

    case H245_CommandMessage::e_endSessionCommand :
      m_endSessionNeeded = true;
      endSessionReceived.Signal();
      switch (connectionState) {
        case EstablishedConnection :
          Release(EndedByRemoteUser);
          break;
        case AwaitingLocalAnswer :
          Release(EndedByCallerAbort);
          break;
        default :
          Release(EndedByRefusal);
      }
      SendReleaseComplete();
      return false;

#if OPAL_H239
    case H245_CommandMessage::e_genericCommand :
    {
      const H245_GenericMessage & gen = command;
      if (H323GetCapabilityIdentifier(gen.m_messageIdentifier) == H239MessageOID)
        return OnH239Message(gen.m_subMessageIdentifier, gen.m_messageContent);
    }
#endif
  }

  return OnUnknownControlPDU(pdu);
}


PBoolean H323Connection::OnH245Indication(const H323ControlPDU & pdu)
{
  const H245_IndicationMessage & indication = pdu;

  switch (indication.GetTag()) {
    case H245_IndicationMessage::e_masterSlaveDeterminationRelease :
      return masterSlaveDeterminationProcedure->HandleRelease(indication);

    case H245_IndicationMessage::e_terminalCapabilitySetRelease :
      return capabilityExchangeProcedure->HandleRelease(indication);

    case H245_IndicationMessage::e_openLogicalChannelConfirm :
      return logicalChannels->HandleOpenConfirm(indication);

    case H245_IndicationMessage::e_requestChannelCloseRelease :
      return logicalChannels->HandleRequestCloseRelease(indication);

    case H245_IndicationMessage::e_requestModeRelease :
      return requestModeProcedure->HandleRelease(indication);

    case H245_IndicationMessage::e_miscellaneousIndication :
      return OnH245_MiscellaneousIndication(indication);

    case H245_IndicationMessage::e_jitterIndication :
      return OnH245_JitterIndication(indication);

    case H245_IndicationMessage::e_userInput :
      OnUserInputIndication(indication);
      break;

#if OPAL_H239
    case H245_IndicationMessage::e_genericIndication :
    {
      const H245_GenericMessage & gen = indication;
      if (H323GetCapabilityIdentifier(gen.m_messageIdentifier) == H239MessageOID)
        return OnH239Message(gen.m_subMessageIdentifier, gen.m_messageContent);
    }
#endif
  }

  return true; // Do NOT call OnUnknownControlPDU for indications
}


PBoolean H323Connection::OnH245_SendTerminalCapabilitySet(
                 const H245_SendTerminalCapabilitySet & pdu)
{
  if (pdu.GetTag() == H245_SendTerminalCapabilitySet::e_genericRequest)
    return capabilityExchangeProcedure->Start(true);

  PTRACE(2, "H245\tUnhandled SendTerminalCapabilitySet: " << pdu);
  return true;
}


PBoolean H323Connection::OnH245_FlowControlCommand(
                 const H245_FlowControlCommand & pdu)
{
  PTRACE(3, "H245\tFlowControlCommand: scope=" << pdu.m_scope.GetTagName());

  long restriction;
  if (pdu.m_restriction.GetTag() == H245_FlowControlCommand_restriction::e_maximumBitRate)
    restriction = (const PASN_Integer &)pdu.m_restriction;
  else
    restriction = -1; // H245_FlowControlCommand_restriction::e_noRestriction

  switch (pdu.m_scope.GetTag()) {
    case H245_FlowControlCommand_scope::e_wholeMultiplex :
      OnLogicalChannelFlowControl(NULL, restriction);
      break;

    case H245_FlowControlCommand_scope::e_logicalChannelNumber :
    {
      H323Channel * chan = logicalChannels->FindChannel((unsigned)(const H245_LogicalChannelNumber &)pdu.m_scope, false);
      if (chan != NULL)
        OnLogicalChannelFlowControl(chan, restriction);
    }
  }

  return true;
}


PBoolean H323Connection::OnH245_MiscellaneousCommand(
                 const H245_MiscellaneousCommand & pdu)
{
  H323Channel * chan = logicalChannels->FindChannel((unsigned)pdu.m_logicalChannelNumber, false);
  if (chan != NULL)
    chan->OnMiscellaneousCommand(pdu.m_type);
  else
    PTRACE(2, "H245\tMiscellaneousCommand: is ignored chan=" << pdu.m_logicalChannelNumber
           << ", type=" << pdu.m_type.GetTagName());

  return true;
}


PBoolean H323Connection::OnH245_MiscellaneousIndication(
                 const H245_MiscellaneousIndication & pdu)
{
  H323Channel * chan = logicalChannels->FindChannel((unsigned)pdu.m_logicalChannelNumber, true);
  if (chan != NULL)
    chan->OnMiscellaneousIndication(pdu.m_type);
  else
    PTRACE(2, "H245\tMiscellaneousIndication is ignored. chan=" << pdu.m_logicalChannelNumber
           << ", type=" << pdu.m_type.GetTagName());

  return true;
}


PBoolean H323Connection::OnH245_JitterIndication(
                 const H245_JitterIndication & pdu)
{
  PTRACE(3, "H245\tJitterIndication: scope=" << pdu.m_scope.GetTagName());

  static const DWORD mantissas[8] = { 0, 1, 10, 100, 1000, 10000, 100000, 1000000 };
  static const DWORD exponents[8] = { 10, 25, 50, 75 };
  DWORD jitter = mantissas[pdu.m_estimatedReceivedJitterMantissa]*
                 exponents[pdu.m_estimatedReceivedJitterExponent]/10;

  int skippedFrameCount = -1;
  if (pdu.HasOptionalField(H245_JitterIndication::e_skippedFrameCount))
    skippedFrameCount = pdu.m_skippedFrameCount;

  int additionalBuffer = -1;
  if (pdu.HasOptionalField(H245_JitterIndication::e_additionalDecoderBuffer))
    additionalBuffer = pdu.m_additionalDecoderBuffer;

  switch (pdu.m_scope.GetTag()) {
    case H245_JitterIndication_scope::e_wholeMultiplex :
      OnLogicalChannelJitter(NULL, jitter, skippedFrameCount, additionalBuffer);
      break;

    case H245_JitterIndication_scope::e_logicalChannelNumber :
    {
      H323Channel * chan = logicalChannels->FindChannel((unsigned)(const H245_LogicalChannelNumber &)pdu.m_scope, false);
      if (chan != NULL)
        OnLogicalChannelJitter(chan, jitter, skippedFrameCount, additionalBuffer);
    }
  }

  return true;
}


#if OPAL_H239
bool H323Connection::OnH239Message(unsigned subMessage, const H245_ArrayOf_GenericParameter & params)
{
  switch (subMessage) {
    case 1 : // flowControlReleaseRequest GenericRequest
      return OnH239FlowControlRequest(H323GetGenericParameterInteger(params, 42),
                                      H323GetGenericParameterInteger(params, 41));

    case 2 : // flowControlReleaseResponse GenericResponse
      return OnH239FlowControlResponse(H323GetGenericParameterInteger(params, 42),
                                       H323GetGenericParameterBoolean(params, 127));

    case 3 : // presentationTokenRequest GenericRequest
      return OnH239PresentationRequest(H323GetGenericParameterInteger(params, 42),
                                       H323GetGenericParameterInteger(params, 43),
                                       H323GetGenericParameterInteger(params, 44));

    case 4 : // presentationTokenResponse GenericResponse
      return OnH239PresentationResponse(H323GetGenericParameterInteger(params, 42),
                                        H323GetGenericParameterInteger(params, 44),
                                        H323GetGenericParameterBoolean(params, 127));

    case 5 : // presentationTokenRelease GenericCommand
      return OnH239PresentationRelease(H323GetGenericParameterInteger(params, 42),
                                       H323GetGenericParameterInteger(params, 44));

    case 6 : // presentationTokenIndicateOwner GenericIndication
      return OnH239PresentationIndication(H323GetGenericParameterInteger(params, 42),
                                          H323GetGenericParameterInteger(params, 44));
  }
  return true;
}


bool H323Connection::OnH239FlowControlRequest(unsigned logicalChannel, unsigned PTRACE_PARAM(bitRate))
{
  PTRACE(3, "H239\tOnH239FlowControlRequest(" << logicalChannel << ',' << bitRate << ") - sending acknowledge");

  H323ControlPDU pdu;
  H245_ArrayOf_GenericParameter & params = pdu.BuildGenericResponse(H239MessageOID, 2).m_messageContent;
  //Viji    08/05/2009 Fix the order of the generic parameters as per Table 11 of H.239 ITU spec
  H323AddGenericParameterBoolean(params, 126, true); // Acknowledge
  H323AddGenericParameterInteger(params, 42, logicalChannel, H245_ParameterValue::e_unsignedMin);
  return WriteControlPDU(pdu);
}


bool H323Connection::OnH239FlowControlResponse(unsigned PTRACE_PARAM(logicalChannel), bool PTRACE_PARAM(rejected))
{
  PTRACE(3, "H239\tOnH239FlowControlResponse(" << logicalChannel << ',' << rejected << ')');

  return true;
}


bool H323Connection::OnH239PresentationRequest(unsigned logicalChannel, unsigned symmetryBreaking, unsigned terminalLabel)
{
  PTRACE(3, "H239\tOnH239PresentationRequest(" << logicalChannel << ',' << symmetryBreaking << ',' << terminalLabel << ") - sending acknowledge");

  bool ack;
  if (m_h239SymmetryBreaking != 0) {
    // Our request is in progress, 11.2.4/H.239
    if (m_h239SymmetryBreaking > symmetryBreaking)
      ack = false; // No, we have it
    else if (m_h239SymmetryBreaking < symmetryBreaking) {
      ack = true;
      m_h239TokenOwned = false;
      m_h239SymmetryBreaking = 0;
      OnChangedPresentationRole(GetRemotePartyURL(), false);
    }
    else {
      // Try again
      m_h239SymmetryBreaking = PRandom::Number(1, 127);
      return SendH239PresentationRequest(m_h239TokenChannel, m_h239SymmetryBreaking, m_h239TerminalLabel);
    }
  }
  else if (!m_h239TokenOwned)
    ack = true; // 11.2.1/H.239 not owned
  else if (!OnChangedPresentationRole(GetRemotePartyURL(), true))
    ack = false; // 11.2.2/H.239 Is owned but not giving it up
  else {
    m_h239TokenOwned =  false;
    ack = true;
  }

  H323ControlPDU pdu;
  H245_ArrayOf_GenericParameter & params = pdu.BuildGenericResponse(H239MessageOID, 4).m_messageContent;
  //Viji    08/05/2009 Fix the order of the generic parameters as per
  //Table 13/H.239  - presentationTokenResponse syntax in the H.239 ITU spec
  H323AddGenericParameterBoolean(params, ack ? 126 : 127, true); // Acknowledge/Reject
  H323AddGenericParameterInteger(params, 44, terminalLabel, H245_ParameterValue::e_unsignedMin);
  H323AddGenericParameterInteger(params, 42, logicalChannel, H245_ParameterValue::e_unsignedMin);

  return WriteControlPDU(pdu);
}


bool H323Connection::SendH239PresentationRequest(unsigned logicalChannel, unsigned symmetryBreaking, unsigned terminalLabel)
{
  if (!GetRemoteH239Control()) {
    PTRACE(2, "H239\tCannot send presentation token request, not completed TCS or remote not capable");
    return false;
  }

  PTRACE(3, "H239\tSendH239PresentationRequest(" << logicalChannel << ',' << symmetryBreaking << ',' << terminalLabel << ')');

  H323ControlPDU pdu;
  H245_ArrayOf_GenericParameter & params = pdu.BuildGenericRequest(H239MessageOID, 3).m_messageContent;
  // Note order is important (Table 12/H.239)
  H323AddGenericParameterInteger(params, 44, terminalLabel, H245_ParameterValue::e_unsignedMin);
  H323AddGenericParameterInteger(params, 42, logicalChannel, H245_ParameterValue::e_unsignedMin);
  H323AddGenericParameterInteger(params, 43, symmetryBreaking, H245_ParameterValue::e_unsignedMin);

  return WriteControlPDU(pdu);
}


bool H323Connection::OnH239PresentationResponse(unsigned logicalChannel, unsigned terminalLabel, bool rejected)
{
  PTRACE(3, "H239\tOnH239PresentationResponse(" << logicalChannel << ',' << terminalLabel << ',' << rejected << ')');

  // Did we request it?
  if (m_h239SymmetryBreaking == 0)
    return SendH239PresentationRelease(logicalChannel, terminalLabel); // No

  m_h239SymmetryBreaking = 0;
  m_h239TokenOwned = !rejected;
  OnChangedPresentationRole(m_h239TokenOwned ? GetLocalPartyURL() : GetRemotePartyURL(), false);

  return true;
}


bool H323Connection::OnH239PresentationRelease(unsigned PTRACE_PARAM(logicalChannel), unsigned PTRACE_PARAM(terminalLabel))
{
  PTRACE(3, "H239\tOnH239PresentationRelease(" << logicalChannel << ',' << terminalLabel << ')');
  return true;
}


bool H323Connection::SendH239PresentationRelease(unsigned logicalChannel, unsigned terminalLabel)
{
  if (!GetRemoteH239Control()) {
    PTRACE(2, "H239\tCannot send presentation token release, not completed TCS or remote not capable");
    return false;
  }

  PTRACE(3, "H239\tSendH239PresentationRelease(" << logicalChannel << ',' << terminalLabel << ')');

  H323ControlPDU pdu;
  H245_ArrayOf_GenericParameter & params = pdu.BuildGenericCommand(H239MessageOID, 5).m_messageContent;
  // Note order is important (Table 12/H.239)
  H323AddGenericParameterInteger(params, 44, terminalLabel, H245_ParameterValue::e_unsignedMin);
  H323AddGenericParameterInteger(params, 42, logicalChannel, H245_ParameterValue::e_unsignedMin);

  return WriteControlPDU(pdu);
}


bool H323Connection::OnH239PresentationIndication(unsigned PTRACE_PARAM(logicalChannel), unsigned PTRACE_PARAM(terminalLabel))
{
  PTRACE(3, "H239\tOnH239PresentationIndication(" << logicalChannel << ',' << terminalLabel << ')');
  return true;
}


bool H323Connection::GetRemoteH239Control() const
{
  return remoteCapabilities.FindCapability(H323H239ControlCapability()) != NULL;
}


OpalMediaFormatList H323Connection::GetRemoteH239Formats() const
{
  OpalMediaFormatList formats;

  for (PINDEX i = 0; i < remoteCapabilities.GetSize(); ++i) {
    const H323Capability & capability = remoteCapabilities[i];
    if (capability.GetMainType() == H323Capability::e_Video &&
        capability.GetSubType() == H245_VideoCapability::e_extendedVideoCapability)
      formats += capability.GetMediaFormat();
  }

  return formats;
}


bool H323Connection::RequestPresentationRole(bool release)
{
  if (m_h239TokenOwned && release) {
    m_h239TokenOwned = false;
    SendH239PresentationRelease(m_h239TokenChannel, m_h239TerminalLabel); // 11.2.3/H.239
    OnChangedPresentationRole(PString::Empty(), false);
    return true;
  }

  if (m_h239TokenOwned || release || m_h239SymmetryBreaking != 0)
    return false;

  // 11.2.4/H.239 part 1
  m_h239SymmetryBreaking = PRandom::Number(1, 127);
  return SendH239PresentationRequest(m_h239TokenChannel, m_h239SymmetryBreaking, m_h239TerminalLabel);
}


bool H323Connection::HasPresentationRole() const
{
  return m_h239TokenOwned;
}
#endif // OPAL_H239


H323Channel * H323Connection::GetLogicalChannel(unsigned number, PBoolean fromRemote) const
{
  PSafeLockReadWrite mutex(*this);
  return logicalChannels->FindChannel(number, fromRemote);
}


H323Channel * H323Connection::FindChannel(unsigned rtpSessionId, PBoolean fromRemote) const
{
  PSafeLockReadWrite mutex(*this);
  return logicalChannels->FindChannelBySession(rtpSessionId, fromRemote);
}


bool H323Connection::HoldRemote(bool placeOnHold)
{
#if OPAL_H450
  if (placeOnHold) {
    if (h4504handler->GetState() != H4504Handler::e_ch_NE_Held && !h4504handler->HoldCall(true))
      return false;
  }
  else {
    if (h4504handler->GetState() == H4504Handler::e_ch_NE_Held && !h4504handler->RetrieveCall())
      return false;
  }
#endif

  if (!SendCapabilitySet(placeOnHold))
    return false;

  // Signal the manager that there is a hold
  OnHold(false, placeOnHold);
  return true;
}


PBoolean H323Connection::IsOnHold(bool fromRemote) const
{
#if OPAL_H450
  // Yes this looks around the wrong way, it isn't!
  return fromRemote ? (m_holdFromRemote != eOffHoldFromRemote || h4504handler->GetState() == H4504Handler::e_ch_NE_Held)
                    : (m_holdToRemote || h4504handler->GetState() == H4504Handler::e_ch_RE_Held);
#else
  return fromRemote ? m_holdFromRemote != eOffHoldFromRemote : m_holdToRemote;
#endif
}


bool H323Connection::TransferConnection(const PString & remoteParty)
{
  PTRACE(3, "H323\tTransferring " << *this << " to " << remoteParty);

  PSafePtr<OpalCall> call = endpoint.GetManager().FindCallWithLock(remoteParty, PSafeReadOnly);
  if (call == NULL) {
#if OPAL_H450
    if (IsEstablished() && TransferCall(remoteParty))
      return true;
#endif
    return ForwardCall(remoteParty);
  }

#if OPAL_H450
  PSafePtr<H323Connection> h323 = call->GetConnectionAs<H323Connection>();
  if (h323 != NULL)
    return TransferCall(h323->GetRemotePartyURL(), h323->GetToken());
#endif

  PTRACE(2, "H323\tConsultation transfer requires other party to be H.323.");
  return false;
}


#if OPAL_H450

bool H323Connection::TransferCall(const PString & remoteParty,
                                  const PString & callIdentity)
{
  // According to H.450.4, if prior to consultation the primary call has been put on hold, the
  // transferring endpoint shall first retrieve the call before Call Transfer is invoked.
  if (!callIdentity.IsEmpty() && h4504handler->GetState() == H4504Handler::e_ch_NE_Held)
    h4504handler->RetrieveCall();

  return h4502handler->TransferCall(remoteParty, callIdentity);
}


void H323Connection::ConsultationTransfer(const PString & primaryCallToken)
{
  h4502handler->ConsultationTransfer(primaryCallToken);
}


void H323Connection::HandleConsultationTransfer(const PString & callIdentity,
                                                H323Connection& incoming)
{
  h4502handler->HandleConsultationTransfer(callIdentity, incoming);
}

PBoolean H323Connection::IsTransferringCall() const
{
  switch (h4502handler->GetState()) {
    case H4502Handler::e_ctAwaitIdentifyResponse :
    case H4502Handler::e_ctAwaitInitiateResponse :
    case H4502Handler::e_ctAwaitSetupResponse :
      return true;

    default :
      return false;
  }
}


PBoolean H323Connection::IsTransferredCall() const
{
   return (h4502handler->GetInvokeId() != 0 &&
           h4502handler->GetState() == H4502Handler::e_ctIdle) ||
           h4502handler->isConsultationTransferSuccess();
}

void H323Connection::HandleTransferCall(const PString & token,
                                        const PString & identity)
{
  if (!token.IsEmpty() || !identity)
    h4502handler->AwaitSetupResponse(token, identity);
}


int H323Connection::GetCallTransferInvokeId()
{
  return h4502handler->GetInvokeId();
}


void H323Connection::HandleCallTransferFailure(const int returnError)
{
  h4502handler->HandleCallTransferFailure(returnError);
}


void H323Connection::SetAssociatedCallToken(const PString& token)
{
  h4502handler->SetAssociatedCallToken(token);
}


void H323Connection::OnConsultationTransferSuccess(H323Connection& /*secondaryCall*/)
{
   h4502handler->SetConsultationTransferSuccess();
}


void H323Connection::IntrudeCall(unsigned capabilityLevel)
{
  h45011handler->IntrudeCall(capabilityLevel);
}


void H323Connection::HandleIntrudeCall(const PString & token,
                                       const PString & identity)
{
  if (!token.IsEmpty() || !identity)
    h45011handler->AwaitSetupResponse(token, identity);
}


PBoolean H323Connection::GetRemoteCallIntrusionProtectionLevel(const PString & intrusionCallToken,
                                                           unsigned intrusionCICL)
{
  return h45011handler->GetRemoteCallIntrusionProtectionLevel(intrusionCallToken, intrusionCICL);
}


void H323Connection::SetIntrusionImpending()
{
  h45011handler->SetIntrusionImpending();
}


void H323Connection::SetForcedReleaseAccepted()
{
  h45011handler->SetForcedReleaseAccepted();
}


void H323Connection::SetIntrusionNotAuthorized()
{
  h45011handler->SetIntrusionNotAuthorized();
}


void H323Connection::SendCallWaitingIndication(const unsigned nbOfAddWaitingCalls)
{
  h4506handler->AttachToAlerting(*alertingPDU, nbOfAddWaitingCalls);
}


#endif

PBoolean H323Connection::OnControlProtocolError(ControlProtocolErrors /*errorSource*/,
                                            const void * /*errorData*/)
{
  return true;
}

void H323Connection::OnSendCapabilitySet(H245_TerminalCapabilitySet & /*pdu*/)
{
}


PBoolean H323Connection::OnReceivedCapabilitySet(const H323Capabilities & remoteCaps,
                                             const H245_MultiplexCapability * muxCap,
                                             H245_TerminalCapabilitySetReject & /*rejectPDU*/)
{
  if (muxCap != NULL) {
    if (muxCap->GetTag() != H245_MultiplexCapability::e_h2250Capability) {
      PTRACE(1, "H323\tCapabilitySet contains unsupported multiplex.");
      return false;
    }

    const H245_H2250Capability & h225_0 = *muxCap;
    remoteMaxAudioDelayJitter = h225_0.m_maximumAudioDelayJitter;
  }

  if (remoteCaps.GetSize() == 0) {
    PTRACE(3, "H323\tReceived empty CapabilitySet, shutting down transmitters.");
    if (m_holdFromRemote != eOnHoldFromRemote) {
      m_holdFromRemote = eOnHoldFromRemote;
      OnHold(true, true);
    }
    // Received empty TCS, so close all transmit channels
    for (H245LogicalChannelDict::iterator it  = logicalChannels->GetChannels().begin();
                                          it != logicalChannels->GetChannels().end(); ++it) {
      H245NegLogicalChannel & negChannel = it->second;
      H323Channel * channel = negChannel.GetChannel();
      if (channel != NULL && !channel->GetNumber().IsFromRemote())
        negChannel.Close();
    }
  }
  else {
    /* Received non-empty TCS, if was in paused state or this is the first TCS
       received so we should kill the fake table created by fast start kill
       the remote capabilities table so Merge() becomes a simple assignment */
    if (m_holdFromRemote == eOnHoldFromRemote || !capabilityExchangeProcedure->HasReceivedCapabilities())
      remoteCapabilities.RemoveAll();

    PINDEX previousCaps = remoteCapabilities.GetSize();

    if (!remoteCapabilities.Merge(remoteCaps))
      return false;

    if (m_holdFromRemote == eOnHoldFromRemote) {
      PTRACE(3, "H323\tReceived CapabilitySet while paused, re-starting transmitters.");
      m_holdFromRemote = eRetrieveFromRemote;
      if (HasCompatibilityIssue(e_NeedTCSAfterNonEmptyTCS))
        capabilityExchangeProcedure->Start(true);
      if (HasCompatibilityIssue(e_NeedMSDAfterNonEmptyTCS))
        masterSlaveDeterminationProcedure->Start(true);
      OnSelectLogicalChannels();

      /* A cheat to help in some debugging, some systems connect to one media,
         immediately put us on hold, then restore on another media source. This
         saves that time that took in the (usually) unused forwarding phase
         time entry. */
      m_phaseTime[ForwardingPhase].SetCurrentTime();
    }
    else if (connectionState > HasExecutedSignalConnect && previousCaps > 0 && remoteCapabilities.GetSize() > previousCaps) {
      PTRACE(3, "H323\tReceived CapabilitySet with more media types.");
      OnSelectLogicalChannels();
    }
    else {
      if (localCapabilities.GetSize() > 0)
        capabilityExchangeProcedure->Start(false);
    }

    // Adjust the RF2388 transitter to remotes capabilities.
    H323Capability * capability = remoteCapabilities.FindCapability(H323_UserInputCapability::GetSubTypeName(H323_UserInputCapability::SignalToneRFC2833));
    m_rfc2833Handler->SetTxMediaFormat(capability != NULL ? capability->GetMediaFormat() : OpalMediaFormat());
  }

  return true;
}

bool H323Connection::SendCapabilitySet(PBoolean empty)
{
  PSafeLockReadWrite mutex(*this);
  if (!capabilityExchangeProcedure->Start(true, empty))
    return false;

  m_holdToRemote = empty;
  return true;
}


bool H323Connection::IsSendingCapabilitySet()
{
  PSafeLockReadOnly mutex(*this);
  return capabilityExchangeProcedure->IsSendingCapabilities();
}


void H323Connection::OnSetLocalCapabilities()
{
  if (capabilityExchangeProcedure->HasSentCapabilities())
    return;

  // create the list of media formats supported locally
  OpalMediaFormatList formats = GetLocalMediaFormats();
  if (formats.IsEmpty()) {
    PTRACE(3, "H323\tSetLocalCapabilities - no existing formats in call");
    return;
  }

  PTRACE(4, "H323\tSetLocalCapabilities: " << setfill(',') << formats);

#if OPAL_H239
  H323H239ControlCapability * h329Control = NULL;
  if (m_h239Control) {
    h329Control = new H323H239ControlCapability();
    PTRACE_CONTEXT_ID_TO(h329Control);
    formats += h329Control->GetMediaFormat();
  }
#endif

  // Remove those things not in the other parties media format list
  for (PINDEX c = 0; c < localCapabilities.GetSize(); c++) {
    H323Capability & capability = localCapabilities[c];
    OpalMediaFormat format = capability.GetMediaFormat();
    if (format.GetMediaType() == OpalMediaType::UserInput() || !formats.HasFormat(format)) {
      localCapabilities.Remove(&capability);
      c--;
    }
  }

  H323Capability::CapabilityDirection symmetric;
  if (m_forceSymmetricTCS)
    symmetric = H323Capability::e_ReceiveAndTransmit;
  else {
    PSafePtr<OpalConnection> otherConnection = GetOtherPartyConnection();
    if (otherConnection != NULL && otherConnection->RequireSymmetricMediaStreams())
      symmetric = H323Capability::e_ReceiveAndTransmit;
    else
      symmetric = H323Capability::e_Receive;
  }

  // Add those things that are in the other parties media format list
  static const OpalMediaType mediaList[] = {
    OpalMediaType::Audio()
#if OPAL_T38_CAPABILITY
    , OpalMediaType::Fax()
#endif
#if OPAL_VIDEO
    , OpalMediaType::Video()
#endif
#if OPAL_HAS_H224
    , OpalH224MediaType()
#endif
  };

  OpalBandwidth availableBandwidth = GetBandwidthAvailable(OpalBandwidth::Rx);

  PINDEX simultaneous = P_MAX_INDEX;
  for (PINDEX m = 0; m < PARRAYSIZE(mediaList); m++) {
#if OPAL_T38_CAPABILITY
    if (m != 1)                   // Fax is in same simultaneous set as Audio
#endif
      simultaneous = P_MAX_INDEX;

    for (OpalMediaFormatList::iterator format = formats.begin(); format != formats.end(); ++format) {
      if (format->GetMediaType() == mediaList[m] && format->IsTransportable()) {
        if (format->GetMaxBandwidth() > availableBandwidth)
          format->SetOptionInteger(OpalMediaFormat::MaxBitRateOption(), availableBandwidth);
        simultaneous = localCapabilities.AddMediaFormat(0, simultaneous, *format, symmetric);
      }
    }
  }

#if OPAL_H239
  simultaneous = P_MAX_INDEX;
  for (OpalMediaFormatList::iterator format = formats.begin(); format != formats.end(); ++format) {
    if (localCapabilities.FindCapability(format->GetName()) != NULL &&
        format->GetOptionInteger(OpalVideoFormat::ContentRoleMaskOption()) != 0) {
      H323H239VideoCapability * newCap = new H323H239VideoCapability(*format);
      PTRACE_CONTEXT_ID_TO(newCap);
      if (localCapabilities.FindCapability(*newCap) == NULL)
        simultaneous = localCapabilities.SetCapability(0, simultaneous, newCap);
      else
        delete newCap;
    }
  }

  if (h329Control != NULL) {
    if (localCapabilities.FindCapability(*h329Control) == NULL)
      localCapabilities.SetCapability(0, P_MAX_INDEX, h329Control);
    else
      delete h329Control;
  }
#endif

#if OPAL_H235_6 || OPAL_H235_8
  // Remove secure capabilities and regenerate them
  for (PINDEX c = 0; c < localCapabilities.GetSize(); c++) {
    H323Capability * capability = &localCapabilities[c];
    if (dynamic_cast<H235SecurityCapability *>(capability) != NULL) {
      localCapabilities.Remove(capability);
      c--;
    }
  }
#endif // OPAL_H235_6 || OPAL_H235_8

#if OPAL_H235_6
  if (!GetDiffieHellman().IsEmpty())
    H235SecurityCapability::AddAllCapabilities(localCapabilities, endpoint.GetMediaCryptoSuites(), "H.235");
#endif // OPAL_H235_6

#if OPAL_H235_8
  if (GetControlChannel().GetLocalAddress().GetProtoPrefix() == OpalTransportAddress::TlsPrefix())
    H235SecurityCapability::AddAllCapabilities(localCapabilities, endpoint.GetMediaCryptoSuites(), "SRTP");
#endif // OPAL_H235_8

#if OPAL_RTP_FEC
  H323FECCapability::AddAllCapabilities(localCapabilities, formats);
#endif // OPAL_RTP_FEC

  H323Capability * rfc2833Capability = NULL;
  OpalMediaFormatList::const_iterator rfc2833Format = formats.FindFormat(OpalRFC2833);
  if (rfc2833Format != formats.end()) {
    rfc2833Capability = new H323_UserInputCapability(H323_UserInputCapability::SignalToneRFC2833);
    rfc2833Capability->SetPayloadType(rfc2833Format->GetPayloadType());  // Set the correct dynamic payload type
    m_rfc2833Handler->SetRxMediaFormat(rfc2833Capability->GetMediaFormat());  // Adjust the RF2388 transitter to local capabilities.
  }

  PSafePtr<OpalConnection> otherConnection = GetOtherPartyConnection();
  if (otherConnection == NULL || !otherConnection->IsNetworkConnection() ||
      GetEndPoint().GetManager().GetMediaTransferMode(*this, *otherConnection, OpalMediaType::Audio()) == OpalManager::MediaTransferTranscode)
    H323_UserInputCapability::AddAllCapabilities(localCapabilities, 0, P_MAX_INDEX, rfc2833Capability);
  else if (rfc2833Capability != NULL)
    localCapabilities.SetCapability(0, P_MAX_INDEX, rfc2833Capability);

  PTRACE(3, "H323\tSetLocalCapabilities:\n" << setprecision(2) << localCapabilities);
}


PBoolean H323Connection::IsH245Master() const
{
  return masterSlaveDeterminationProcedure->IsMaster();
}


void H323Connection::StartRoundTripDelay()
{
  if (LockReadWrite()) {
    if (!IsReleased() &&
        masterSlaveDeterminationProcedure->IsDetermined() &&
        capabilityExchangeProcedure->HasSentCapabilities()) {
      if (roundTripDelayProcedure->IsRemoteOffline()) {
        PTRACE(1, "H245\tRemote failed to respond to PDU.");
        if (endpoint.ShouldClearCallOnRoundTripFail())
          Release(EndedByTransportFail);
      }
      else
        roundTripDelayProcedure->StartRequest();
    }
    UnlockReadWrite();
  }
}


PTimeInterval H323Connection::GetRoundTripDelay() const
{
  return roundTripDelayProcedure->GetRoundTripDelay();
}


void H323Connection::InternalEstablishedConnectionCheck()
{
  bool h245_available = masterSlaveDeterminationProcedure->IsDetermined() &&
                        capabilityExchangeProcedure->HasSentCapabilities() &&
                        capabilityExchangeProcedure->HasReceivedCapabilities();

  PTRACE(3, "H323\tInternalEstablishedConnectionCheck: "
            "connectionState=" << connectionState << ", "
            "m_fastStartState=" << m_fastStartState << ", "
            "m_holdFromRemote=" << m_holdFromRemote << ", "
            "H.245 is " << (h245_available ? "ready" : "unavailable"));

  // Check for if all the 245 conditions are met so can start up logical
  // channels and complete the connection establishment.
  if (h245_available) {
    m_endSessionNeeded = true;

    if (m_holdFromRemote != eOnHoldFromRemote) {
      H323Channel * chan = logicalChannels->FindChannelBySession(0, false);

      // Delay handling of off hold until we finish redoing TCS, MSD & OLC.
      if (m_holdFromRemote == eRetrieveFromRemote) {
        if (chan != NULL && (chan = logicalChannels->FindChannelBySession(chan->GetSessionID(), true)) != NULL) {
          m_holdFromRemote = eOffHoldFromRemote;
          OnHold(true, false);
        }
      }
      else {
        if (chan == NULL && (connectionState >= HasExecutedSignalConnect || (earlyStart && m_fastStartState != FastStartAcknowledged)))
          OnSelectLogicalChannels();
      }
    }
  }

  switch (GetPhase()) {
    case SetUpPhase :
    case ProceedingPhase :
    case AlertingPhase :
      if (h245_available && connectionState >= HasExecutedSignalConnect) {
        bool hasEstablishedChannel = false;
        bool inProgressChannel = false;
        for (H245LogicalChannelDict::iterator it  = logicalChannels->GetChannels().begin();
                                              it != logicalChannels->GetChannels().end(); ++it) {
          if (it->second.IsEstablished())
            hasEstablishedChannel = true;
          if (it->second.IsAwaitingEstablishment())
            inProgressChannel = true;
        }
        if (hasEstablishedChannel && !inProgressChannel)
          OnConnectedInternal();
      }
      break;

    case ConnectedPhase :
      SetPhase(EstablishedPhase);
      OnEstablished();
      // Set established in next case

    case EstablishedPhase :
      connectionState = EstablishedConnection; // Keep in sync
      break;

    default :
      break;
  }
}


OpalMediaFormatList H323Connection::GetMediaFormats() const
{
  OpalMediaFormatList list = remoteCapabilities.GetMediaFormats();

  AdjustMediaFormats(false, NULL, list);

  if (  IsH245Master() &&
        (
          (localCapabilities.GetSize() > 0 &&
           localCapabilities[0].GetCapabilityDirection() == H323Capability::e_ReceiveAndTransmit)
          ||
          (remoteCapabilities.GetSize() > 0 &&
           remoteCapabilities[0].GetCapabilityDirection() == H323Capability::e_ReceiveAndTransmit)
        )) {
    /* If symmetry is required and we are the master we re-order their formats
       to OUR order. This avoids a masterSlaveConflict (assuming other end is
       working correctly) and some unecessaary open logical channel round
       trips.

       Techniically, we should be a bit more sophisticated in determining
       symmtery requirement, but 99.9% of the time of the first, if the entry
       is symmetric, they all are. */
    PStringArray order;
    OpalMediaFormatList local = localCapabilities.GetMediaFormats();
    for (OpalMediaFormatList::iterator it = local.begin(); it != local.end(); ++it)
      order.AppendString(it->GetName());
    list.Reorder(order);
    PTRACE(2, "H323\tRe-ordered media formats due to symmetry rules on " << *this);
  }

  return list;
}


PStringArray H323Connection::GetMediaCryptoSuites() const
{
  PStringArray cryptoSuites = OpalConnection::GetMediaCryptoSuites();

#if OPAL_H235_6 || OPAL_H235_8
  H235SecurityCapability * cap = dynamic_cast<H235SecurityCapability *>(remoteCapabilities.FindCapability(H323Capability::e_H235Security));
  if (cap != NULL) {
    OpalMediaCryptoSuite::List remoteCryptoSuites = cap->GetCryptoSuites();
    for (PINDEX i = 0; i < cryptoSuites.GetSize();) {
      if (remoteCryptoSuites.GetValuesIndex(cryptoSuites[i]) != P_MAX_INDEX)
        ++i;
      else
        cryptoSuites.RemoveAt(i++);
    }
  }
#endif // OPAL_H235_6 || OPAL_H235_8

  return cryptoSuites;
}


unsigned H323Connection::GetNextSessionID(const OpalMediaType & mediaType, bool isSource)
{
  unsigned sessionID;

  if (GetMediaStream(mediaType, isSource) != NULL)
    sessionID = H323Capability::MasterAllocatedBaseSessionID; // Allocate new session
  else {
    OpalMediaStreamPtr mediaStream = GetMediaStream(mediaType, !isSource);
    if (mediaStream != NULL)
      return mediaStream->GetSessionID(); // Must use same session ID as other direction.

    sessionID = mediaType->GetDefaultSessionId(); // Use default session
    if (sessionID == 0) {
#if OPAL_HAS_H224
      if (HasCompatibilityIssue(e_H224MustBeSession3) && mediaType == OpalH224MediaType())
        return H323Capability::DefaultDataSessionID;
#endif
      sessionID = H323Capability::MasterAllocatedBaseSessionID; // No default, allocate new session
    }
  }

  if (sessionID > H323Capability::DefaultDataSessionID && !IsH245Master())
    sessionID = H323Capability::DeferredSessionID;

  while (GetMediaStream(sessionID, true) != NULL || GetMediaStream(sessionID, false) != NULL)
    ++sessionID;

  return sessionID;
}


#if OPAL_T38_CAPABILITY
bool H323Connection::SwitchFaxMediaStreams(bool toT38)
{
  if (ownerCall.IsSwitchingT38()) {
    PTRACE(2, "H323\tNested call to SwitchFaxMediaStreams on " << *this);
    return false;
  }

  if (toT38 && remoteCapabilities.FindCapability(OpalT38) == NULL) {
    PTRACE(3, "H323\tRemote does not have T.38 capabilities on " << *this);
    return false;
  }

  if (GetMediaStream(toT38 ? H323Capability::DefaultDataSessionID
                           : H323Capability::DefaultAudioSessionID, true) != NULL) {
    PTRACE(3, "H323\tAlready switched media streams to " << (toT38 ? "T.38" : "audio") << " on " << *this);
    return false;
  }

  PTRACE(3, "H323\tSwitching to " << (toT38 ? "T.38" : "audio") << " on " << *this);
  ownerCall.SetSwitchingT38(toT38);
  if (RequestModeChangeT38(toT38 ? OpalT38 : OpalG711uLaw))
    return true;

  ownerCall.ResetSwitchingT38();
  return false;
}
#endif // OPAL_T38_CAPABILITY


OpalMediaStreamPtr H323Connection::OpenMediaStream(const OpalMediaFormat & mediaFormat, unsigned sessionID, bool isSource)
{
  // See if already opened
  OpalMediaStreamPtr stream = GetMediaStream(sessionID, isSource);
  if (stream != NULL && stream->IsOpen()) {
    if (stream->GetMediaFormat() == mediaFormat) {
      PTRACE(3, "H323\tOpenMediaStream (already opened) for session " << sessionID << " on " << *this);
      return stream;
    }

    if (isSource) {
      stream = CreateMediaStream(mediaFormat, sessionID, isSource);
      if (stream == NULL) {
        PTRACE(1, "H323\tCreateMediaStream returned NULL for session " << sessionID << " on " << *this);
        return NULL;
      }
      mediaStreams.Append(stream);

      // Channel from other side, do RequestModeChange
      RequestModeChange(mediaFormat);
      return stream;
    }

    // Changing the media format, needs to close and re-open the stream
    stream->Close();
    stream.SetNULL();
  }

  if ( isSource &&
      !ownerCall.IsEstablished() &&
      (GetAutoStart(mediaFormat.GetMediaType())&OpalMediaType::Receive) == 0) {
    PTRACE(3, "H323\tOpenMediaStream auto start disabled, refusing " << mediaFormat.GetMediaType() << " open");
    return NULL;
  }

  for (H323LogicalChannelList::iterator iterChan  = m_fastStartChannels.begin();
                                        iterChan != m_fastStartChannels.end(); ++iterChan) {
    if (iterChan->GetDirection() == (isSource ? H323Channel::IsReceiver : H323Channel::IsTransmitter) &&
        iterChan->GetCapability().GetMediaFormat() == mediaFormat) {
      PTRACE(4, "H323\tOpenMediaStream fast opened for session " << sessionID);
      stream = CreateMediaStream(mediaFormat, sessionID, isSource);
      if (stream != NULL && stream->Open() && OnOpenMediaStream(*stream)) {
        mediaStreams.Append(stream);
        iterChan->SetMediaStream(stream);
        logicalChannels->Add(*iterChan);
        return stream;
      }
    }
  }

  H323Channel * channel = FindChannel(sessionID, isSource);
  if (channel == NULL) {
    // Logical channel not open, if receiver that is an error
    if (isSource) {
      PTRACE(2, "H323\tNo receive logical channel for session " << sessionID);
      return NULL;
    }

    if (!masterSlaveDeterminationProcedure->IsDetermined() ||
        !capabilityExchangeProcedure->HasSentCapabilities() ||
        !capabilityExchangeProcedure->HasReceivedCapabilities()) {
      PTRACE(2, "H323\tOpenMediaStream cannot (H.245 unavailable) open logical channel for " << mediaFormat);
      return NULL;
    }

    // If transmitter, send OpenLogicalChannel using the capability associated with the media format
    PString name = mediaFormat.GetName();
#if OPAL_H239
    if (sessionID > 2 && mediaFormat.GetOptionEnum(OpalVideoFormat::ContentRoleOption(), OpalVideoFormat::eNoRole) != OpalVideoFormat::eNoRole)
      name += '+' + GetH239VideoMediaFormat().GetName();
#endif
    H323Capability * capability = remoteCapabilities.FindCapability(name);
    if (capability == NULL) {
      PTRACE(2, "H323\tOpenMediaStream could not find capability for " << name);
      return NULL;
    }

#if OPAL_H235_6 || OPAL_H235_8
    OpalMediaFormat adjustedMediaFormat = mediaFormat;
    for (PINDEX i = 0; i < remoteCapabilities.GetSize(); ++i) {
      const H235SecurityCapability * h235 = dynamic_cast<const H235SecurityCapability *>(&remoteCapabilities[i]);
      if (  h235 != NULL &&
            h235->GetMediaCapabilityNumber() == capability->GetCapabilityNumber() &&
           !h235->GetCryptoSuites().IsEmpty())
      {
        capability->SetCryptoSuite(h235->GetCryptoSuites().front());
        if (adjustedMediaFormat.GetPayloadType() < RTP_DataFrame::DynamicBase)
          adjustedMediaFormat.SetPayloadType((RTP_DataFrame::PayloadTypes)125);
        break;
      }
    }
    capability->UpdateMediaFormat(adjustedMediaFormat);
#else
    capability->UpdateMediaFormat(mediaFormat);
#endif // OPAL_H235_6 || OPAL_H235_8

    if (!OpenLogicalChannel(*capability, sessionID, H323Channel::IsTransmitter)) {
      PTRACE(2, "H323\tOpenMediaStream could not open logical channel for " << mediaFormat);
      return NULL;
    }
    channel = FindChannel(sessionID, isSource);
    if (PAssertNULL(channel) == NULL)
      return NULL;
  }

  stream = channel->GetMediaStream();
  if (stream != NULL && stream->Open()) {
    PTRACE(3, "H323\tOpenMediaStream using channel " << channel->GetNumber() << " for session " << sessionID);
    mediaStreams.Append(stream);
    return stream;
  }

  PTRACE(2, "H323\tCould not open stream for logical channel " << channel->GetNumber());
  channel->Close();
  return NULL;
}


void H323Connection::OnClosedMediaStream(const OpalMediaStream & stream)
{
  // For a channel, this gets called twice. The first time we send CLC to remote
  // The second time is after CLC Ack or a timeout occurs, then we call the ancestor
  // function to clean up the media stream.
  if (!IsReleased()) {
    H245LogicalChannelDict & channels = logicalChannels->GetChannels();
    for (H245LogicalChannelDict::iterator it = channels.begin(); it != channels.end(); ++it) {
      H323Channel * channel = it->second.GetChannel();
      if (channel != NULL && channel->GetMediaStream() == &stream) {
        const H323ChannelNumber & number = channel->GetNumber();
        logicalChannels->Close(number, number.IsFromRemote());
      }
    }
  }

  OpalRTPConnection::OnClosedMediaStream(stream);
}


bool H323Connection::OnMediaCommand(OpalMediaStream & stream, const OpalMediaCommand & command)
{
  if (stream.IsSource() != (&stream.GetConnection() == this))
    return OpalConnection::OnMediaCommand(stream, command);

  H323Channel * channel = FindChannel(stream.GetSessionID(), true);
  if (channel == NULL) {
    PTRACE(4, "H.323\tOnMediaCommand, no channel found for session " << stream.GetSessionID());
  }
  else {
    const OpalMediaFlowControl * flow = dynamic_cast<const OpalMediaFlowControl *>(&command);
    if (flow != NULL) {
      H323ControlPDU pdu;
      pdu.BuildFlowControlCommand(channel->GetNumber(), flow->GetMaxBitRate()/100);
      WriteControlPDU(pdu);
      return true;
    }

#if OPAL_VIDEO
    if (PIsDescendant(&command, OpalVideoUpdatePicture) &&
         (m_stringOptions.GetInteger(OPAL_OPT_VIDUP_METHODS, OPAL_OPT_VIDUP_METHOD_DEFAULT)&OPAL_OPT_VIDUP_METHOD_OOB) != 0) {
      if (m_h245FastUpdatePictureTimer.IsRunning()) {
        PTRACE(4, "H.323\tRecent H.245 VideoFastUpdatePicture was sent, not sending another");
        return true;
      }

      H323ControlPDU pdu;
      pdu.BuildMiscellaneousCommand(channel->GetNumber(), H245_MiscellaneousCommand_type::e_videoFastUpdatePicture);
      WriteControlPDU(pdu);

#if OPAL_STATISTICS
      m_VideoUpdateRequestsSent++;
#endif

      return true;
    }
#endif // OPAL_VIDEO
  }

  return OpalRTPConnection::OnMediaCommand(stream, command);
}


bool H323Connection::GetMediaTransportAddresses(OpalConnection & otherConnection,
                                           const OpalMediaType & mediaType,
                                     OpalTransportAddressArray & transports) const
{
  if (NoMediaBypass(otherConnection, mediaType))
    return false;

  // If have fast connect, use addresses from them as don't have sessions yet
  for (H323LogicalChannelList::const_iterator channel = m_fastStartChannels.begin(); channel != m_fastStartChannels.end(); ++channel) {
    if (channel->GetCapability().GetMediaFormat().GetMediaType() == mediaType) {
      OpalTransportAddress media, control;
      if (channel->GetMediaTransportAddress(media, control) && transports.SetAddressPair(media, control)) {
        PTRACE(3, "H323\tGetMediaTransportAddresses of " << mediaType << " found fast connect "
               << setfill(',') << transports << " for " << otherConnection << " on " << *this);
        return true;
      }
    }
  }

  return OpalRTPConnection::GetMediaTransportAddresses(otherConnection, mediaType, transports);
}


void H323Connection::OpenFastStartChannel(unsigned sessionID, H323Channel::Directions direction)
{
  for (H323LogicalChannelList::iterator channel = m_fastStartChannels.begin(); channel != m_fastStartChannels.end(); ++channel) {
    if (channel->GetSessionID() == sessionID && channel->GetDirection() == direction) {
      unsigned error;
      if (OnCreateLogicalChannel(channel->GetCapability(), direction, error)) {
        PTRACE(3, "H225\tOpening fast start channel for " << channel->GetCapability());
        if (channel->Open())
          break;
      }
    }
  }
}


void H323Connection::OnSelectLogicalChannels()
{
  PTRACE(3, "H245\tDefault OnSelectLogicalChannels, " << m_fastStartState);

#if OPAL_VIDEO
  OpalMediaType::AutoStartMode autoStartVideo = GetAutoStart(OpalMediaType::Video());
#endif
#if OPAL_T38_CAPABILITY
  OpalMediaType::AutoStartMode autoStartFax = GetAutoStart(OpalMediaType::Fax());
#endif
#if OPAL_HAS_H224
  OpalMediaType::AutoStartMode autoStartH224 = GetAutoStart(OpalH224MediaType());
#endif

  // Select the first codec that uses the "standard" audio session.
  switch (m_fastStartState) {
    default : //FastStartDisabled :
      SelectDefaultLogicalChannel(OpalMediaType::Audio(), H323Capability::DefaultAudioSessionID);
#if OPAL_T38_CAPABILITY
      if ((autoStartFax&OpalMediaType::Transmit) != 0)
        SelectDefaultLogicalChannel(OpalMediaType::Fax(), H323Capability::DefaultDataSessionID);
      else {
        PTRACE(4, "H245\tOnSelectLogicalChannels, fax not auto-started");
      }
#endif
#if OPAL_HAS_H224
      if ((autoStartH224&OpalMediaType::Transmit) != 0)
        SelectDefaultLogicalChannel(OpalH224MediaType(), 0);
      else {
        PTRACE(4, "H245\tOnSelectLogicalChannels, H.224 camera control not auto-started");
      }
#endif
#if OPAL_VIDEO
      // Start video last so gets remaining bandwidth and not steal from other channels
      if ((autoStartVideo&OpalMediaType::Transmit) != 0)
        SelectDefaultLogicalChannel(OpalMediaType::Video(), H323Capability::DefaultVideoSessionID);
      else {
        PTRACE(4, "H245\tOnSelectLogicalChannels, video not auto-started");
      }
#endif
      break;

    case FastStartInitiate :
      SelectFastStartChannels(H323Capability::DefaultAudioSessionID, true, true);
#if OPAL_T38_CAPABILITY
      if (autoStartFax != OpalMediaType::DontOffer)
        SelectFastStartChannels(H323Capability::DefaultDataSessionID,
                                (autoStartFax&OpalMediaType::Transmit) != 0,
                                (autoStartFax&OpalMediaType::Receive) != 0);
#endif
#if OPAL_HAS_H224
      if (autoStartH224 != OpalMediaType::DontOffer)
        SelectFastStartChannels(GetNextSessionID(OpalH224MediaType(), true),
                                (autoStartH224&OpalMediaType::Transmit) != 0,
                                (autoStartH224&OpalMediaType::Receive) != 0);
#endif
#if OPAL_VIDEO
      // Start video last so gets remaining bandwidth and not steal from other channels
      if (autoStartVideo != OpalMediaType::DontOffer)
        SelectFastStartChannels(H323Capability::DefaultVideoSessionID,
        (autoStartVideo&OpalMediaType::Transmit) != 0,
        (autoStartVideo&OpalMediaType::Receive) != 0);
#endif
      break;

    case FastStartResponse :
      OpenFastStartChannel(H323Capability::DefaultAudioSessionID, H323Channel::IsTransmitter);
      OpenFastStartChannel(H323Capability::DefaultAudioSessionID, H323Channel::IsReceiver);
#if OPAL_T38_CAPABILITY
      if ((autoStartFax&OpalMediaType::Transmit) != 0)
        OpenFastStartChannel(H323Capability::DefaultDataSessionID, H323Channel::IsTransmitter);
      if ((autoStartFax&OpalMediaType::Receive) != 0)
        OpenFastStartChannel(H323Capability::DefaultDataSessionID, H323Channel::IsReceiver);
#endif
#if OPAL_HAS_H224
      if ((autoStartH224&OpalMediaType::Transmit) != 0)
        OpenFastStartChannel(GetNextSessionID(OpalH224MediaType(), false), H323Channel::IsTransmitter);
      if ((autoStartH224&OpalMediaType::Receive) != 0)
        OpenFastStartChannel(GetNextSessionID(OpalH224MediaType(), true), H323Channel::IsReceiver);
#endif
#if OPAL_VIDEO
      // Start video last so gets remaining bandwidth and not steal from other channels
      if ((autoStartVideo&OpalMediaType::Transmit) != 0)
        OpenFastStartChannel(H323Capability::DefaultVideoSessionID, H323Channel::IsTransmitter);
      if ((autoStartVideo&OpalMediaType::Receive) != 0)
        OpenFastStartChannel(H323Capability::DefaultVideoSessionID, H323Channel::IsReceiver);
#endif
      break;
  }
}


void H323Connection::SelectDefaultLogicalChannel(const OpalMediaType & mediaType, unsigned sessionID)
{
  if (sessionID > 0 && FindChannel(sessionID, false))
    return;

  PSafePtr<OpalConnection> otherConnection = GetOtherPartyConnection();
  if (otherConnection == NULL) {
    PTRACE(2, "H323\tSelectLogicalChannel(" << sessionID << ") cannot start channel without second connection in call.");
    return;
  }

  if (!ownerCall.OpenSourceMediaStreams(*otherConnection, mediaType, sessionID)) {
    PTRACE(2, "H323\tSelectLogicalChannel(" << sessionID << ") could not start media stream.");
  }
}


void H323Connection::SelectFastStartChannels(unsigned sessionID,
                                             PBoolean transmitter,
                                             PBoolean receiver)
{
  // Select all of the fast start channels to offer to the remote when initiating a call.
  for (PINDEX i = 0; i < localCapabilities.GetSize(); i++) {
    H323Capability & capability = localCapabilities[i];
    if (capability.GetDefaultSessionID() == sessionID) {
      if (receiver) {
        if (!OpenLogicalChannel(capability, sessionID, H323Channel::IsReceiver)) {
          PTRACE(2, "H323\tOnSelectLogicalChannels, OpenLogicalChannel rx failed: " << capability);
        }
      }
      if (transmitter) {
        if (!OpenLogicalChannel(capability, sessionID, H323Channel::IsTransmitter)) {
          PTRACE(2, "H323\tOnSelectLogicalChannels, OpenLogicalChannel tx failed: " << capability);
        }
      }
    }
  }
}

void H323Connection::SendFlowControlCommand(unsigned channelNumber, unsigned newBitRate)
{
  H323ControlPDU pdu;
  pdu.BuildFlowControlCommand(channelNumber,newBitRate);
  WriteControlPDU(pdu);
}


PBoolean H323Connection::OpenLogicalChannel(const H323Capability & capability,
                                        unsigned sessionID,
                                        H323Channel::Directions dir)
{
  PSafeLockReadWrite mutex(*this);

  switch (m_fastStartState) {
    default : // FastStartDisabled
      if (dir == H323Channel::IsReceiver)
        return false;

      // Traditional H245 handshake
      return logicalChannels->Open(capability, sessionID);

    case FastStartResponse :
      // Do not use OpenLogicalChannel for starting these.
      return false;

    case FastStartInitiate :
      break;
  }

  /*If starting a receiver channel and are initiating the fast start call,
    indicated by the remoteCapabilities being empty, we do a "trial"
    listen on the channel. That is, for example, the UDP sockets are created
    to receive data in the RTP session, but no thread is started to read the
    packets and pass them to the codec. This is because at this point in time,
    we do not know which of the codecs is to be used, and more than one thread
    cannot read from the RTP ports at the same time.
  */
  H323Channel * channel = capability.CreateChannel(*this, dir, sessionID, NULL);
  if (channel == NULL)
    return false;

  if (dir != H323Channel::IsReceiver)
    channel->SetNumber(logicalChannels->GetNextChannelNumber());

  m_fastStartChannels.Append(channel);
  return true;
}


PBoolean H323Connection::OnOpenLogicalChannel(const H245_OpenLogicalChannel & openPDU,
                                          H245_OpenLogicalChannelAck & ackPDU,
                                          unsigned & errorCode,
                                          H323Channel & channel)
{
  unsigned sessionID = channel.GetSessionID();

  PTRACE(4,"H323\tOnOpenLogicalChannel: sessionId=" << sessionID);

  // If get a OLC via H.245 stop trying to do fast start
  m_fastStartState = FastStartDisabled;
  if (!m_fastStartChannels.IsEmpty()) {
    m_fastStartChannels.RemoveAll();
    PTRACE(3, "H245\tReceived early start OLC, aborting fast start");
  }

  if (openPDU.HasOptionalField(H245_OpenLogicalChannel::e_genericInformation)) {
    OnReceiveOLCGenericInformation(sessionID,openPDU.m_genericInformation, false);

    if (OnSendingOLCGenericInformation(sessionID, ackPDU.m_genericInformation, true))
      ackPDU.IncludeOptionalField(H245_OpenLogicalChannelAck::e_genericInformation);
  }

  // See if we got a master/slave conflict OLC reject prior to their OLC
  if (m_conflictingChannels.Contains(sessionID)) {
    OnConflictingLogicalChannel(channel);
    return true;
  }

  // Detect symmetry issues
  H323Capability * capability = remoteCapabilities.FindCapability(channel.GetCapability());
  if (capability == NULL || capability->GetCapabilityDirection() != H323Capability::e_ReceiveAndTransmit) {
    capability = localCapabilities.FindCapability(channel.GetCapability());
    if (capability == NULL || capability->GetCapabilityDirection() != H323Capability::e_ReceiveAndTransmit)
      return true; // No symmetry requested
  }

  // Yep are symmetrical, see if opening something different
  H323Channel * otherChannel = FindChannel(sessionID, false);
  if (otherChannel == NULL)
    return true; // No other channel so symmetry yet to raise it's ugly head

  if (channel.GetCapability() == otherChannel->GetCapability())
    return true; // Is symmetric, all OK!

  /* The correct protocol thing to do is reject the channel if we are the
     master. However, some systems will not then re-open a channel, so we act
     like we are a slave and close our end instead. */
  if (IsH245Master() && !HasCompatibilityIssue(e_BadMasterSlaveConflict)) {
    errorCode = H245_OpenLogicalChannelReject_cause::e_masterSlaveConflict;
    return false;
  }

  OnConflictingLogicalChannel(channel);
  return true;
}



void H323Connection::OnReceiveOLCGenericInformation(unsigned sessionID, const H245_ArrayOf_GenericInformation & infos, bool isAck) const
{
  PTRACE(4, "H245\tHandling Generic OLC Session " << sessionID);
#if OPAL_H460
  if (m_features != NULL) {
    for (PINDEX i = 0; i < infos.GetSize(); i++) {
      const H245_GenericInformation & info = infos[i];
      if (info.m_messageIdentifier.GetTag() == H245_CapabilityIdentifier::e_standard) {
        PString oid = ((const PASN_ObjectId &)info.m_messageIdentifier).AsString();
        for (H460_FeatureSet::iterator it = m_features->begin(); it != m_features->end(); ++it) {
          if (it->second->IsNegotiated() && it->first.GetOID() == oid) {
            it->second->OnReceiveOLCGenericInformation(sessionID, info.m_messageContent, isAck);
            break;
          }
        }
      }
    }
  }
#endif // OPAL_H460
}


bool H323Connection::OnSendingOLCGenericInformation(unsigned sessionID,
                                                    H245_ArrayOf_GenericInformation & info,
                                                    bool isAck) const
{
  PTRACE(4, "H245\tSet Generic " << (isAck ? "OLCack" : "OLC") << " Session " << sessionID );

#if OPAL_H460
  if (m_features != NULL) {
    for (H460_FeatureSet::iterator it = m_features->begin(); it != m_features->end(); ++it) {
      if (it->second->IsNegotiated()) {
        H245_ArrayOf_GenericParameter content;
        if (it->second->OnSendingOLCGenericInformation(sessionID, content, isAck)) {
          PINDEX lastPos = info.GetSize();
          info.SetSize(lastPos+1);

          info[lastPos].IncludeOptionalField(H245_GenericMessage::e_messageContent);
          info[lastPos].m_messageContent = content;

          H245_CapabilityIdentifier & id = info[lastPos].m_messageIdentifier;
          id.SetTag(H245_CapabilityIdentifier::e_standard);
          ((PASN_ObjectId &)id).SetValue(it->first.GetOID());
        }
      }
    }
  }
#endif // OPAL_H460

  return info.GetSize() > 0;
}


PBoolean H323Connection::OnConflictingLogicalChannel(H323Channel & conflictingChannel)
{
  unsigned sessionID = conflictingChannel.GetSessionID();
  PTRACE(2, "H323\tLogical channel " << conflictingChannel
         << " conflict on session " << sessionID
         << ", we are " << (IsH245Master() ? "master" : "slave")
         << ", codec: " << conflictingChannel.GetCapability());

  /* Matrix of conflicts:
       Local EP is master and conflicting channel from remote (OLC)
          Reject remote transmitter (function is not called)
       Local EP is master and conflicting channel to remote (OLCAck)
          Should not happen (function is not called)
       Local EP is slave and conflicting channel from remote (OLC)
          Close sessions reverse channel from remote
          Start new reverse channel using codec in conflicting channel
          Accept the OLC for masters transmitter
       Local EP is slave and conflicting channel to remote (OLCRej)
          Start transmitter channel using codec in sessions reverse channel

      Upshot is this is only called if a slave and require a restart of
      some channel. Possibly closing channels as master has precedence.
   */

  OpalMediaStreamPtr mediaStream = m_conflictingChannels.FindWithLock(sessionID, PSafeReference);
  m_conflictingChannels.RemoveAt(sessionID);

  bool fromRemote = conflictingChannel.GetNumber().IsFromRemote();
  H323Channel * otherChannel = FindChannel(sessionID, !fromRemote);
  H323Capability * capability;

  if (fromRemote) {
    if (otherChannel != NULL) {
      PTRACE_IF(1, mediaStream != NULL, "H323\tInvalid master/slave conflict resolution, already have conflicting channel info");

      mediaStream = otherChannel->GetMediaStream();
      otherChannel->SetMediaStream(NULL);
      CloseLogicalChannelNumber(otherChannel->GetNumber());
    }
    else {
      if (mediaStream == NULL) {
        // The only way to get in here is some pathological cases when releasing
        // call in the middle of the conflict negotiation
        PTRACE(1, "H323\tInvalid master/slave conflict resolution, no conflicting channel");
        return false;
      }
    }

    capability = remoteCapabilities.FindCapability(conflictingChannel.GetCapability());
  }
  else {
    // The only way for the following is if we had two OLC's running at the same
    // time and both were rejected. This should be impossible.
    PTRACE_IF(1, mediaStream != NULL, "H323\tInvalid master/slave conflict resolution, simultaneous OLC?");

    mediaStream = conflictingChannel.GetMediaStream();
    conflictingChannel.SetMediaStream(NULL);

    // From OLC reject, but don't have remote OLC yet, remember ...
    if (otherChannel == NULL) {
      PTRACE(1, "H323\tCannot resolve conflict yet, no reverse channel.");
      m_conflictingChannels.SetAt(sessionID, mediaStream);
      return true;
    }

    CloseLogicalChannelNumber(conflictingChannel.GetNumber());
    capability = remoteCapabilities.FindCapability(otherChannel->GetCapability());
  }

  if (capability == NULL) {
    PTRACE(1, "H323\tCould not resolve conflict, capability not available on remote.");
    return true;
  }

  return logicalChannels->Open(*capability, sessionID, 0, mediaStream);
}


H323Channel * H323Connection::CreateLogicalChannel(const H245_OpenLogicalChannel & open,
                                                   PBoolean startingFast,
                                                   unsigned & errorCode)
{
  const H245_H2250LogicalChannelParameters * param;
  const H245_DataType * dataType;
  H323Channel::Directions direction;
  H323Capability * capability;

  if (startingFast && open.HasOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters)) {
    if (open.m_reverseLogicalChannelParameters.m_multiplexParameters.GetTag() !=
              H245_OpenLogicalChannel_reverseLogicalChannelParameters_multiplexParameters
                                                      ::e_h2250LogicalChannelParameters) {
      errorCode = H245_OpenLogicalChannelReject_cause::e_unsuitableReverseParameters;
      PTRACE(1, "H323\tCreateLogicalChannel - reverse channel, H225.0 only supported");
      return NULL;
    }

    PTRACE(3, "H323\tCreateLogicalChannel - reverse channel");
    dataType = &open.m_reverseLogicalChannelParameters.m_dataType;
    param = &(const H245_H2250LogicalChannelParameters &)
                      open.m_reverseLogicalChannelParameters.m_multiplexParameters;
    direction = H323Channel::IsTransmitter;

    // Must have been put in earlier
    capability = remoteCapabilities.FindCapability(*dataType);
  }
  else {
    if (open.m_forwardLogicalChannelParameters.m_multiplexParameters.GetTag() !=
              H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters
                                                      ::e_h2250LogicalChannelParameters) {
      PTRACE(1, "H323\tCreateLogicalChannel - forward channel, H225.0 only supported");
      errorCode = H245_OpenLogicalChannelReject_cause::e_unspecified;
      return NULL;
    }

    PTRACE(3, "H323\tCreateLogicalChannel - forward channel");
    dataType = &open.m_forwardLogicalChannelParameters.m_dataType;
    param = &(const H245_H2250LogicalChannelParameters &)
                      open.m_forwardLogicalChannelParameters.m_multiplexParameters;
    direction = H323Channel::IsReceiver;

    PString mediaPacketization;
    if (param->HasOptionalField(H245_H2250LogicalChannelParameters::e_mediaPacketization) &&
        param->m_mediaPacketization.GetTag() == H245_H2250LogicalChannelParameters_mediaPacketization::e_rtpPayloadType)
      mediaPacketization = H323GetRTPPacketization(param->m_mediaPacketization);

    // See if datatype is supported
    capability = localCapabilities.FindCapability(*dataType, mediaPacketization);
  }

  if (capability == NULL) {
    errorCode = H245_OpenLogicalChannelReject_cause::e_unknownDataType;
    PTRACE(1, "H323\tCreateLogicalChannel - unknown data type");
    return NULL; // If codec not supported, return error
  }

  if (!capability->OnReceivedPDU(*dataType, direction == H323Channel::IsReceiver)) {
    errorCode = H245_OpenLogicalChannelReject_cause::e_dataTypeNotSupported;
    PTRACE(1, "H323\tCreateLogicalChannel - data type not supported");
    return NULL; // If codec not supported, return error
  }

  if (!OnCreateLogicalChannel(*capability, direction, errorCode))
    return NULL; // If codec combination not supported, return error

  H323Channel * channel = capability->CreateChannel(*this, direction, param->m_sessionID, param);
  if (channel == NULL) {
    errorCode = H245_OpenLogicalChannelReject_cause::e_dataTypeNotAvailable;
    PTRACE(1, "H323\tCreateLogicalChannel - data type not available");
    return NULL;
  }

  if (!channel->SetInitialBandwidth())
    errorCode = H245_OpenLogicalChannelReject_cause::e_insufficientBandwidth;
  else {
    if (startingFast)
      channel->SetBandwidthUsed(0); // Release as can prevent other fast connect OLC's from processing.
    if (channel->OnReceivedPDU(open, errorCode))
      return channel;
  }

  PTRACE(1, "H323\tOnReceivedPDU gave error " << errorCode);
  delete channel;
  return NULL;
}


H323Channel * H323Connection::CreateRealTimeLogicalChannel(const H323Capability & capability,
                                                          H323Channel::Directions dir,
                                                                         unsigned sessionID,
                                       const H245_H2250LogicalChannelParameters * param)
{
  OpalMediaType mediaType = capability.GetMediaFormat().GetMediaType();

  if (sessionID == 0)
    sessionID = GetNextSessionID(mediaType, true);

  const H323Transport & transport = GetControlChannel();

  // We only support RTP over UDP at this point in time ...
  H323TransportAddress remoteControlAddress(transport.GetRemoteAddress().GetHostName(), 0, OpalTransportAddress::UdpPrefix());
  if (param != NULL && param->HasOptionalField(H245_H2250LogicalChannelParameters::e_mediaControlChannel)) {
    remoteControlAddress = H323TransportAddress(param->m_mediaControlChannel);
    if (remoteControlAddress.IsEmpty())
      return NULL;
    // Check for IPv4 or IPv6 mismatch.
    if (!transport.GetRemoteAddress().IsCompatible(remoteControlAddress))
      return NULL;
  }

  PCaselessString sessionType = mediaType->GetMediaSessionType();

#if OPAL_H235_6 || OPAL_H235_8
  const OpalMediaCryptoSuite * cryptoSuite = capability.GetCryptoSuite();
  if (cryptoSuite != NULL)
    cryptoSuite->ChangeSessionType(sessionType);
#endif // OPAL_H235_6 || OPAL_H235_8

  OpalMediaSession * session = UseMediaSession(sessionID, mediaType, sessionType);
  if (PAssertNULL(session) == NULL)
    return NULL;

  if (session->GetMediaType() != mediaType) {
    PTRACE(1, "H323\tExisting " << session->GetMediaType() << " session " << sessionID << " does not match " << mediaType);
    return NULL;
  }

#if OPAL_T38_CAPABILITY
  if (ownerCall.IsSwitchingT38()) {
    OpalMediaSession * otherSession = GetMediaSession(sessionID == H323Capability::DefaultAudioSessionID
                          ? H323Capability::DefaultDataSessionID : H323Capability::DefaultAudioSessionID);
    if (otherSession != NULL && otherSession->IsOpen()) {
      OpalMediaSession::Transport transport = otherSession->DetachTransport();
      session->AttachTransport(transport);
    }
  }
#endif // OPAL_T38_CAPABILITY

  if (!session->Open(transport.GetInterface(), remoteControlAddress, false)) {
    ReleaseMediaSession(sessionID);
    return NULL;
  }

  OpalRTPSession * rtpSession = dynamic_cast<OpalRTPSession *>(session);
  if (rtpSession != NULL)
    rtpSession->Restart(dir == H323Channel::IsReceiver);

  return CreateRTPChannel(capability, dir, *session);
}


H323_RTPChannel * H323Connection::CreateRTPChannel(const H323Capability & capability,
                                                   H323Channel::Directions direction,
                                                   H323RTPSession & rtp)
{
  return new H323_RTPChannel(*this, capability, direction, rtp);
}


PBoolean H323Connection::OnCreateLogicalChannel(const H323Capability & capability,
                                            H323Channel::Directions dir,
                                            unsigned & errorCode)
{
  if (connectionState == ShuttingDownConnection) {
    errorCode = H245_OpenLogicalChannelReject_cause::e_unspecified;
    return false;
  }

  // Default error if returns false
  errorCode = H245_OpenLogicalChannelReject_cause::e_unspecified;

  // Check if is allowed in respective TCS
  if (dir != H323Channel::IsReceiver) {
    H323Capability * remoteCapability = remoteCapabilities.FindCapability(capability);
    if (remoteCapability == NULL || !remoteCapabilities.IsAllowed(*remoteCapability)) {
      PTRACE(2, "H323\tOnCreateLogicalChannel - transmit capability " << capability << " not allowed.");
      return false;
    }
  }
  else {
    H323Capability * localCapability = localCapabilities.FindCapability(capability);
    if (localCapability == NULL || !localCapabilities.IsAllowed(*localCapability)) {
      PTRACE(2, "H323\tOnCreateLogicalChannel - receive capability " << capability << " not allowed.");
      return false;
    }
  }

  // Check all running channels, and if new one can't run with it return false
  for (H245LogicalChannelDict::iterator it  = logicalChannels->GetChannels().begin();
                                        it != logicalChannels->GetChannels().end(); ++it) {
    H323Channel * channel = it->second.GetChannel();
    if (channel != NULL && channel->GetDirection() == dir) {
      if (dir != H323Channel::IsReceiver) {
        if (!remoteCapabilities.IsAllowed(capability, channel->GetCapability())) {
          PTRACE(2, "H323\tOnCreateLogicalChannel - transmit capability " << capability
                 << " and " << channel->GetCapability() << " incompatible.");
          return false;
        }
      }
      else {
        if (!localCapabilities.IsAllowed(capability, channel->GetCapability())) {
          PTRACE(2, "H323\tOnCreateLogicalChannel - transmit capability " << capability
                 << " and " << channel->GetCapability() << " incompatible.");
          return false;
        }
      }
    }
  }

  return true;
}


PBoolean H323Connection::OnStartLogicalChannel(H323Channel & channel)
{
#if OPAL_T38_CAPABILITY
  if (ownerCall.IsSwitchingT38()) {
    H323Channel * other = logicalChannels->FindChannelBySession(channel.GetSessionID(),
                                                   !channel.GetNumber().IsFromRemote());
    if (other != NULL && other->IsOpen()) {
      if (t38ModeChangeCapabilities.IsEmpty()) {
        PTRACE(4, "H323\tCompleted remote switch of T.38");
        ownerCall.ResetSwitchingT38();
      }
      else {
        t38ModeChangeCapabilities.Replace(channel.GetCapability().GetMediaFormat().GetName(), PString::Empty());
        if (t38ModeChangeCapabilities.FindSpan(",") == P_MAX_INDEX) {
          PTRACE(4, "H323\tCompleted local switch of T.38");
          OnSwitchedFaxMediaStreams(channel.GetSessionID() == H323Capability::DefaultDataSessionID, true);
        }
      }
    }
    else
      PTRACE(4, "H323\tWaiting for other channel in switch of T.38");
  }
#else
  t38ModeChangeCapabilities.MakeEmpty();
#endif // OPAL_T38_CAPABILITY

  return endpoint.OnStartLogicalChannel(*this, channel);
}


void H323Connection::CloseLogicalChannel(unsigned number, PBoolean fromRemote)
{
  PSafeLockReadWrite mutex(*this);
  if (connectionState != ShuttingDownConnection)
    logicalChannels->Close(number, fromRemote);
}


void H323Connection::CloseLogicalChannelNumber(const H323ChannelNumber & number)
{
  CloseLogicalChannel(number, number.IsFromRemote());
}


void H323Connection::CloseAllLogicalChannels(PBoolean fromRemote)
{
  PSafeLockReadWrite mutex(*this);
  for (H245LogicalChannelDict::iterator it  = logicalChannels->GetChannels().begin();
                                        it != logicalChannels->GetChannels().end(); ++it) {
    H245NegLogicalChannel & negChannel = it->second;
    H323Channel * channel = negChannel.GetChannel();
    if (channel != NULL && channel->GetNumber().IsFromRemote() == fromRemote)
      negChannel.Close();
  }
}


PBoolean H323Connection::OnClosingLogicalChannel(H323Channel & /*channel*/)
{
  return true;
}


void H323Connection::OnClosedLogicalChannel(const H323Channel & channel)
{
  endpoint.OnClosedLogicalChannel(*this, channel);
}


void H323Connection::OnLogicalChannelFlowControl(H323Channel * channel,
                                                 long bitRateRestriction)
{
  if (channel != NULL)
    channel->OnFlowControl(bitRateRestriction);
}


void H323Connection::OnLogicalChannelJitter(H323Channel * channel,
                                            DWORD jitter,
                                            int skippedFrameCount,
                                            int additionalBuffer)
{
  if (channel != NULL)
    channel->OnJitterIndication(jitter, skippedFrameCount, additionalBuffer);
}


OpalBandwidth H323Connection::GetBandwidthUsed(OpalBandwidth::Direction dir) const
{
  PSafeLockReadOnly mutex(*this);
  OpalBandwidth used = 0;

  for (H245LogicalChannelDict::iterator it  = logicalChannels->GetChannels().begin();
                                        it != logicalChannels->GetChannels().end(); ++it) {
    H323Channel * channel = it->second.GetChannel();
    if (channel != NULL) {
      switch (dir) {
        case OpalBandwidth::Rx :
          if (channel->GetDirection() == H323Channel::IsReceiver)
            used += channel->GetBandwidthUsed();
          break;

        case OpalBandwidth::Tx :
          if (channel->GetDirection() == H323Channel::IsTransmitter)
            used += channel->GetBandwidthUsed();
          break;

        default :
          used += channel->GetBandwidthUsed();
          break;
      }
    }
  }

  PTRACE(4, "H323\tUsing " << dir << " bandwidth of " << used << " for " << *this);

  return used;
}


static PBoolean CheckSendUserInputMode(const H323Capabilities & caps,
                                   OpalConnection::SendUserInputModes mode)
{
  // If have remote capabilities, then verify we can send selected mode,
  // otherwise just return and accept it for future validation
  static const H323_UserInputCapability::SubTypes types[H323Connection::NumSendUserInputModes] = {
    H323_UserInputCapability::NumSubTypes,        // SendUserInputAsQ931
    H323_UserInputCapability::BasicString,        // SendUserInputAsString
    H323_UserInputCapability::SignalToneH245,     // SendUserInputAsTone
    H323_UserInputCapability::SignalToneRFC2833,  // SendUserInputAsRFC2833
    H323_UserInputCapability::NumSubTypes,        // SendUserInputInBand
    H323_UserInputCapability::SignalToneH245      // SendUserInputAsProtocolDefault
  };

  if (types[mode] == H323_UserInputCapability::NumSubTypes)
    return mode == H323Connection::SendUserInputAsQ931;

  return caps.FindCapability(H323_UserInputCapability::GetSubTypeName(types[mode])) != NULL;
}


OpalConnection::SendUserInputModes H323Connection::GetRealSendUserInputMode() const
{
  // If have not yet exchanged capabilities (ie not finished setting up the
  // H.245 channel) then the only thing we can do is Q.931
  if (!capabilityExchangeProcedure->HasReceivedCapabilities())
    return SendUserInputAsQ931;

  // First try recommended mode
  if (CheckSendUserInputMode(remoteCapabilities, sendUserInputMode))
    return sendUserInputMode;

  // Then try H.245 tones
  if (CheckSendUserInputMode(remoteCapabilities, SendUserInputAsTone))
    return SendUserInputAsTone;

  // Then try H.245 strings
  if (CheckSendUserInputMode(remoteCapabilities, SendUserInputAsString))
    return SendUserInputAsString;

  // Finally if is H.245 alphanumeric or does not indicate it could do other
  // modes we use H.245 alphanumeric as per spec.
  return SendUserInputAsString;
}


PBoolean H323Connection::SendUserInputString(const PString & value)
{
  SendUserInputModes mode = GetRealSendUserInputMode();

  PTRACE(3, "H323\tSendUserInput(\"" << value << "\"), using mode " << mode);

  if (mode == SendUserInputAsString || mode == SendUserInputAsProtocolDefault)
    return SendUserInputIndicationString(value);

  return OpalRTPConnection::SendUserInputString(value);
}


PBoolean H323Connection::SendUserInputTone(char tone, unsigned duration)
{
  SendUserInputModes mode = GetRealSendUserInputMode();

  PTRACE(3, "H323\tSendUserInputTime('" << tone << "', " << duration << "), using mode " << mode);

  switch (mode) {
    case SendUserInputAsQ931 :
      return SendUserInputIndicationQ931(PString(tone));

    case SendUserInputAsString :
    case SendUserInputAsProtocolDefault:
      return SendUserInputIndicationString(PString(tone));

    case SendUserInputAsTone :
      return SendUserInputIndicationTone(tone, duration);

    default :
      ;
  }

  return OpalRTPConnection::SendUserInputTone(tone, duration);
}


PBoolean H323Connection::SendUserInputIndicationQ931(const PString & value)
{
  PTRACE(3, "H323\tSendUserInputIndicationQ931(\"" << value << "\")");

  H323SignalPDU pdu;
  pdu.BuildInformation(*this);
  pdu.GetQ931().SetKeypad(value);
  if (WriteSignalPDU(pdu))
    return true;

  ClearCall(EndedByTransportFail);
  return false;
}


PBoolean H323Connection::SendUserInputIndicationString(const PString & value)
{
  PTRACE(3, "H323\tSendUserInputIndicationString(\"" << value << "\")");

  H323ControlPDU pdu;
  PASN_GeneralString & str = pdu.BuildUserInputIndication(value);
  if (!str.GetValue())
    return WriteControlPDU(pdu);

  PTRACE(1, "H323\tInvalid characters for UserInputIndication");
  return false;
}


PBoolean H323Connection::SendUserInputIndicationTone(char tone,
                                                 unsigned duration,
                                                 unsigned logicalChannel,
                                                 unsigned rtpTimestamp)
{
  PTRACE(3, "H323\tSendUserInputIndicationTone("
         << tone << ','
         << duration << ','
         << logicalChannel << ','
         << rtpTimestamp << ')');

  if (strchr("0123456789#*ABCD!", tone) == NULL)
    return false;

  H323ControlPDU pdu;
  pdu.BuildUserInputIndication(tone, duration, logicalChannel, rtpTimestamp);
  return WriteControlPDU(pdu);
}


PBoolean H323Connection::SendUserInputIndication(const H245_UserInputIndication & indication)
{
  H323ControlPDU pdu;
  H245_UserInputIndication & ind = pdu.Build(H245_IndicationMessage::e_userInput);
  ind = indication;
  return WriteControlPDU(pdu);
}


void H323Connection::OnUserInputIndication(const H245_UserInputIndication & ind)
{
  switch (ind.GetTag()) {
    case H245_UserInputIndication::e_alphanumeric :
      OnUserInputString((const PASN_GeneralString &)ind);
      break;

    case H245_UserInputIndication::e_signal :
      {
        if (m_UserInputIndicationTimer.IsRunning())
          OnUserInputTone(m_lastUserInputIndication, (unsigned)m_lastUserInputIndicationStart.GetElapsed().GetMilliSeconds());

        const H245_UserInputIndication_signal & sig = ind;
        m_lastUserInputIndication = sig.m_signalType[0];
        m_lastUserInputIndicationStart = 0;
        m_UserInputIndicationTimer = sig.HasOptionalField(H245_UserInputIndication_signal::e_duration) ? sig.m_duration.GetValue() : 90;
        OnUserInputTone(m_lastUserInputIndication, 0);
      }
      break;

    case H245_UserInputIndication::e_signalUpdate:
      m_UserInputIndicationTimer = ((const H245_UserInputIndication_signalUpdate &)ind).m_duration;
      break;
  }
}


void H323Connection::UserInputIndicationTimeout(PTimer&, P_INT_PTR)
{
  GetEndPoint().GetManager().QueueDecoupledEvent(
            new PSafeWorkArg2<OpalConnection, char, unsigned>(this, m_lastUserInputIndication,
                                                              (unsigned)m_lastUserInputIndicationStart.GetElapsed().GetMilliSeconds(),
                                                              &OpalConnection::OnUserInputTone));
}


static void AddSessionCodecName(PStringStream & name, H323Channel * channel)
{
  if (channel == NULL)
    return;

  OpalMediaStreamPtr stream = channel->GetMediaStream();
  if (stream == NULL)
    return;

  OpalMediaFormat mediaFormat = stream->GetMediaFormat();
  if (!mediaFormat.IsValid())
    return;

  if (name.IsEmpty())
    name << mediaFormat;
  else if (name != mediaFormat)
    name << " / " << mediaFormat;
}


PString H323Connection::GetSessionCodecNames(unsigned sessionID) const
{
  PStringStream name;

  AddSessionCodecName(name, FindChannel(sessionID, false));
  AddSessionCodecName(name, FindChannel(sessionID, true));

  return name;
}


PBoolean H323Connection::RequestModeChange(const PString & newModes)
{
  PSafeLockReadWrite mutex(*this);
  return requestModeProcedure->StartRequest(newModes);
}


PBoolean H323Connection::RequestModeChange(const H245_ArrayOf_ModeDescription & newModes)
{
  PSafeLockReadWrite mutex(*this);
  return requestModeProcedure->StartRequest(newModes);
}


PBoolean H323Connection::OnRequestModeChange(const H245_RequestMode & pdu,
                                         H245_RequestModeAck & /*ack*/,
                                         H245_RequestModeReject & /*reject*/,
                                         PINDEX & selectedMode)
{
  for (selectedMode = 0; selectedMode < pdu.m_requestedModes.GetSize(); selectedMode++) {
    bool ok = true;
#if OPAL_T38_CAPABILITY
    bool hasT38 = false;
#endif
    for (PINDEX i = 0; i < pdu.m_requestedModes[selectedMode].GetSize(); i++) {
      H323Capability * capability = localCapabilities.FindCapability(pdu.m_requestedModes[selectedMode][i]);
      if (capability == NULL) {
        ok = false;
        break;
      }
#if OPAL_T38_CAPABILITY
      if (capability->GetMediaFormat() == OpalT38)
        hasT38 = true;
#endif
    }
    if (ok) {
#if OPAL_T38_CAPABILITY
      if (hasT38 != (GetMediaStream(OpalMediaType::Fax(), true) != NULL)) {
        if (!OnSwitchingFaxMediaStreams(hasT38)) {
          PTRACE(2, "H245\tMode change to " << (hasT38 ? "T.38" : "audio") << " rejected by local connection");
          return false;
        }
      }
#endif
      return true;
    }
  }

  PTRACE(2, "H245\tMode change rejected as does not have capabilities");
  return false;
}


void H323Connection::OnModeChanged(const H245_ModeDescription & newMode)
{
  if (!t38ModeChangeCapabilities.IsEmpty()) {
    PTRACE(4, "H323\tOnModeChanged ignored as T.38 Mode Change in progress");
    return;
  }

  PSafePtr<OpalConnection> otherConnection = GetOtherPartyConnection();
  if (otherConnection == NULL)
    return;

  PTRACE(4, "H323\tOnModeChanged, closing channels");

  bool closedSomething = false;

  for (H245LogicalChannelDict::iterator it  = logicalChannels->GetChannels().begin();
                                        it != logicalChannels->GetChannels().end(); ++it) {
    H245NegLogicalChannel & negChannel = it->second;
    H323Channel * channel = negChannel.GetChannel();
    OpalMediaStreamPtr mediaStream = channel->GetMediaStream();
    if (channel != NULL && mediaStream != NULL && !channel->GetNumber().IsFromRemote() &&
          (negChannel.IsAwaitingEstablishment() || negChannel.IsEstablished())) {
      bool closeOne = true;

      for (PINDEX m = 0; m < newMode.GetSize(); m++) {
        H323Capability * capability = localCapabilities.FindCapability(newMode[m]);
        if (PAssertNULL(capability) != NULL) { // Should not occur as OnRequestModeChange checks them
          if (capability->GetMediaFormat() == mediaStream->GetMediaFormat()) {
            closeOne = false;
            break;
          }
        }
      }

      if (closeOne) {
        negChannel.Close();
        closedSomething = true;
      }
      else {
        PTRACE(4, "H323\tLeaving channel " << channel->GetNumber() << " open, as mode request has not changed it.");
      }
    }
  }

  if (closedSomething) {
    PTRACE(4, "H323\tOnModeChanged, opening channels");

    // Start up the new ones
    for (PINDEX i = 0; i < newMode.GetSize(); i++) {
      H323Capability * capability = localCapabilities.FindCapability(newMode[i]);
      if (PAssertNULL(capability) != NULL) { // Should not occur as OnRequestModeChange checks them
        OpalMediaFormat mediaFormat = capability->GetMediaFormat();
        if (!ownerCall.OpenSourceMediaStreams(*otherConnection, mediaFormat.GetMediaType(), 0, mediaFormat)) {
          PTRACE(2, "H245\tCould not open channel after mode change: " << *capability);
        }
      }
    }
  }
}


void H323Connection::OnAcceptModeChange(const H245_RequestModeAck & pdu)
{
  if (t38ModeChangeCapabilities.IsEmpty())
    return;

  PTRACE(3, "H323\tT.38 mode change accepted.");

  PSafePtr<OpalConnection> otherConnection = GetOtherPartyConnection();
  if (otherConnection == NULL)
    return;

  /* This is a special case for a T.38 switch over. Normally a H.245 Mode
     Request is completely independent of what WE are transmitting. But not
     in the case of T.38, we need to switch our side to the same mode as well.
     So, now we have convinced the other side to send us T.38 data we should
     do the T.38 switch locally, the RequestModeChangeT38() function provided
     a list of \n separated capability names to start, we use that to start
     our transmitters to the same formats. After we close the existing ones!
   */
  CloseAllLogicalChannels(false);

  PStringArray modes = t38ModeChangeCapabilities.Lines();

  PStringArray formats = modes[pdu.m_response.GetTag() != H245_RequestModeAck_response::e_willTransmitMostPreferredMode
                                     && modes.GetSize() > 1 ? 1 : 0].Tokenise('\t');

#if OPAL_T38_CAPABILITY
  bool failed = false;
#endif
  for (PINDEX i = 0; i < formats.GetSize(); i++) {
    H323Capability * capability = localCapabilities.FindCapability(formats[i]);
    if (PAssertNULL(capability) != NULL) { // Should not occur!
      OpalMediaFormat mediaFormat = capability->GetMediaFormat();
      if (!ownerCall.OpenSourceMediaStreams(*otherConnection, mediaFormat.GetMediaType(), 0, mediaFormat)) {
        PTRACE(2, "H245\tCould not open channel after T.38 mode change: " << *capability);
#if OPAL_T38_CAPABILITY
        failed = true;
#endif
      }
    }
  }

#if OPAL_T38_CAPABILITY
  if (failed)
    OnSwitchedFaxMediaStreams(ownerCall.IsSwitchingToT38(), false);
#endif
}


void H323Connection::OnRefusedModeChange(const H245_RequestModeReject * /*pdu*/)
{
  if (!t38ModeChangeCapabilities.IsEmpty()) {
    t38ModeChangeCapabilities.MakeEmpty();
#if OPAL_T38_CAPABILITY
    OnSwitchedFaxMediaStreams(ownerCall.IsSwitchingToT38(), false);
#endif
  }
}


PBoolean H323Connection::RequestModeChangeT38(const char * capabilityNames)
{
  t38ModeChangeCapabilities = capabilityNames;
  if (RequestModeChange(t38ModeChangeCapabilities))
    return true;

  t38ModeChangeCapabilities = PString::Empty();
  return false;
}


PBoolean H323Connection::GetAdmissionRequestAuthentication(const H225_AdmissionRequest & /*arq*/,
                                                       H235Authenticators & /*authenticators*/)
{
  return false;
}


const H323Transport & H323Connection::GetControlChannel() const
{
  return *(m_controlChannel != NULL ? m_controlChannel : m_signallingChannel);
}

OpalTransport & H323Connection::GetTransport() const
{
  return *(m_controlChannel != NULL ? m_controlChannel : m_signallingChannel);
}

void H323Connection::SetEnforcedDurationLimit(unsigned seconds)
{
  enforcedDurationLimit.SetInterval(0, seconds);
}


void H323Connection::MonitorCallStatus()
{
  PSafeLockReadWrite safeLock(*this);
  if (!safeLock.IsLocked())
    return;

  if (IsReleased())
    return;

  if (endpoint.GetRoundTripDelayRate() > 0 && !roundTripDelayTimer.IsRunning()) {
    roundTripDelayTimer = endpoint.GetRoundTripDelayRate();
    StartRoundTripDelay();
  }

/*
  if (endpoint.GetNoMediaTimeout() > 0) {
    PBoolean oneRunning = false;
    PBoolean allSilent = true;
    for (PINDEX i = 0; i < logicalChannels->GetSize(); i++) {
      H323Channel * channel = logicalChannels->GetChannelAt(i);
      if (channel != NULL && channel->IsDescendant(H323_RTPChannel::Class())) {
        if (channel->IsRunning()) {
          oneRunning = true;
          if (((H323_RTPChannel *)channel)->GetSilenceDuration() < endpoint.GetNoMediaTimeout()) {
            allSilent = false;
            break;
          }
        }
      }
    }
    if (oneRunning && allSilent)
      ClearCall(EndedByTransportFail);
  }
*/

  if (enforcedDurationLimit.GetResetTime() > 0 && enforcedDurationLimit == 0)
    ClearCall(EndedByDurationLimit);
}


#if OPAL_H460
bool H323Connection::OnSendFeatureSet(H460_MessageType pduType, H225_FeatureSet & featureSet) const
{
  return m_features != NULL && m_features->OnSendPDU(pduType, featureSet);
}


void H323Connection::OnReceiveFeatureSet(H460_MessageType pduType, const H225_FeatureSet & featureSet) const
{
  if (m_features != NULL)
    m_features->OnReceivePDU(pduType, featureSet);
}
#endif // OPAL_H460


bool H323Connection::HasCompatibilityIssue(CompatibilityIssues issue) const
{
  return endpoint.HasCompatibilityIssue(issue, GetRemoteProductInfo());
}


#endif // OPAL_H323

/////////////////////////////////////////////////////////////////////////////
