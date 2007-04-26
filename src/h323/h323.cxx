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
 * $Log: h323.cxx,v $
 * Revision 1.2151  2007/04/26 07:01:46  csoutheren
 * Add extra code to deal with getting media formats from connections early enough to do proper
 * gatewaying between calls. The SIP and H.323 code need to have the handing of the remote
 * and local formats standardized, but this will do for now
 *
 * Revision 2.149  2007/04/25 07:49:06  csoutheren
 * Fix problem with external RTP channels for incoming H.323 calls
 *
 * Revision 2.148  2007/04/10 05:15:54  rjongbloed
 * Fixed issue with use of static C string variables in DLL environment,
 *   must use functional interface for correct initialisation.
 *
 * Revision 2.147  2007/04/04 02:12:00  rjongbloed
 * Reviewed and adjusted PTRACE log levels
 *   Now follows 1=error,2=warn,3=info,4+=debug
 *
 * Revision 2.146  2007/04/02 05:51:33  rjongbloed
 * Tidied some trace logs to assure all have a category (bit before a tab character) set.
 *
 * Revision 2.145  2007/03/29 05:16:49  csoutheren
 * Pass OpalConnection to OpalMediaSream constructor
 * Add ID to OpalMediaStreams so that transcoders can match incoming and outgoing codecs
 *
 * Revision 2.144  2007/03/14 22:40:09  csoutheren
 * Fix H.323 problem with payload map and known payload types
 *
 * Revision 2.143  2007/03/13 02:17:46  csoutheren
 * Remove warnings/errors when compiling with various turned off
 *
 * Revision 2.142  2007/03/12 23:41:32  csoutheren
 * Undo unwanted edit
 *
 * Revision 2.141  2007/03/12 23:22:17  csoutheren
 * Add ability to remove H.450
 *
 * Revision 2.140  2007/03/07 23:47:16  csoutheren
 * Use correct dynamic payload type for H.323 calls
 *
 * Revision 2.139  2007/03/01 05:51:04  rjongbloed
 * Fixed backward compatibility of OnIncomingConnection() virtual
 *   functions on various classes. If an old override returned FALSE
 *   then it will now abort the call as it used to.
 *
 * Revision 2.138  2007/03/01 03:53:18  csoutheren
 * Use local jitter buffer values rather than getting direct from OpalManager
 * Allow OpalConnection string options to be set during incoming calls
 *
 * Revision 2.137  2007/02/21 02:34:55  csoutheren
 * Ensure codec mask is applied on local caps
 *
 * Revision 2.136  2007/02/19 04:43:42  csoutheren
 * Added OnIncomingMediaChannels so incoming calls can optionally be handled in two stages
 *
 * Revision 2.135  2007/02/05 04:23:00  csoutheren
 * Check status on starting both read and write channels
 *
 * Revision 2.133  2007/01/24 04:00:56  csoutheren
 * Arrrghh. Changing OnIncomingConnection turned out to have a lot of side-effects
 * Added some pure viritual functions to prevent old code from breaking silently
 * New OpalEndpoint and OpalConnection descendants will need to re-implement
 * OnIncomingConnection. Sorry :)
 *
 * Revision 2.132  2007/01/18 12:49:22  csoutheren
 * Add ability to disable T.38 in compile
 *
 * Revision 2.131  2007/01/18 04:45:16  csoutheren
 * Messy, but simple change to add additional options argument to OpalConnection constructor
 * This allows the provision of non-trivial arguments for connections
 *
 * Revision 2.130  2007/01/10 09:16:55  csoutheren
 * Allow compilation with video disabled
 *
 * Revision 2.129  2007/01/09 00:59:53  csoutheren
 * Fix problem with sense of NAT detection
 *
 * Revision 2.128  2006/12/20 04:40:59  csoutheren
 * Fixed typo in last checkin
 *
 * Revision 2.127  2006/12/19 07:21:49  csoutheren
 * Fix problem with not adjusting H.245 addresses for NAT
 *
 * Revision 2.126  2006/12/18 03:18:42  csoutheren
 * Messy but simple fixes
 *   - Add access to SIP REGISTER timeout
 *   - Ensure OpalConnection options are correctly progagated
 *
 * Revision 2.125  2006/12/08 04:53:50  csoutheren
 * Applied 1609966 - Small fix for H.323 hold/retrieve
 * Thanks to Simon Zwahlen
 *
 * Revision 2.124  2006/11/27 11:11:21  csoutheren
 * Apply 1588592 - Fixes for H.245 tunneling
 * Thanks to Simon Zwahlen
 *
 * Revision 2.123  2006/11/11 12:23:18  hfriederich
 * Code reorganisation to improve RFC2833 handling for both SIP and H.323. Thanks Simon Zwahlen for the idea
 *
 * Revision 2.122  2006/11/11 09:43:24  hfriederich
 * Remove tab from previous commit
 *
 * Revision 2.121  2006/11/11 09:40:14  hfriederich
 * Don't send RFC2833 if already sent User Input via other mode
 *
 * Revision 2.120  2006/11/11 08:44:42  hfriederich
 * Apply patch #1575071 fix RFC2833 handler's filter stage. Thanks to Borko Jandras
 *
 * Revision 2.119  2006/10/23 01:53:04  csoutheren
 * Fix for H.323 tunneling change
 *
 * Revision 2.118  2006/10/11 00:55:25  csoutheren
 * Only open H.245 channel is tunnelling is not specified
 *
 * Revision 2.117  2006/09/12 18:35:59  dsandras
 * Disallow forbidden media formats. Thanks Craig!
 *
 * Revision 2.116  2006/08/21 05:29:25  csoutheren
 * Messy but relatively simple change to add support for secure (SSL/TLS) TCP transport
 * and secure H.323 signalling via the sh323 URL scheme
 *
 * Revision 2.115  2006/08/20 13:35:55  csoutheren
 * Backported changes to protect against rare error conditions
 *
 * Revision 2.114  2006/08/10 05:10:30  csoutheren
 * Various H.323 stability patches merged in from DeimosPrePLuginBranch
 *
 * Revision 2.113  2006/08/03 04:57:12  csoutheren
 * Port additional NAT handling logic from OpenH323 and extend into OpalConnection class
 *
 * Revision 2.112.2.1  2006/08/09 12:49:21  csoutheren
 * Improve stablity under heavy H.323 load
 *
 * Revision 2.112  2006/06/30 01:39:58  csoutheren
 * Applied 1509222 - H323Connection-gk-deadlock
 * Thanks to Boris Pavacic
 *
 * Revision 2.111  2006/06/27 13:07:37  csoutheren
 * Patch 1374533 - add h323 Progress handling
 * Thanks to Frederich Heem
 *
 * Revision 2.110  2006/06/27 12:54:35  csoutheren
 * Patch 1374489 - h450.7 message center support
 * Thanks to Frederich Heem
 *
 * Revision 2.109  2006/06/21 04:53:15  csoutheren
 * Updated H.245 to version 13
 *
 * Revision 2.108  2006/06/20 05:21:02  csoutheren
 * Update to ASN for H.225v6
 *
 * Revision 2.107  2006/06/09 07:17:22  csoutheren
 * Fixed warning under gcc
 *
 * Revision 2.106  2006/05/30 11:33:02  hfriederich
 * Porting support for H.460 from OpenH323
 *
 * Revision 2.105  2006/05/30 04:58:06  csoutheren
 * Added suport for SIP INFO message (untested as yet)
 * Fixed some issues with SIP state machine on answering calls
 * Fixed some formatting issues
 *
 * Revision 2.104  2006/04/20 16:52:22  hfriederich
 * Adding support for H.224/H.281
 *
 * Revision 2.103  2006/04/10 05:17:47  csoutheren
 * Use locking version of GetOtherConnection rather than unlocked version
 *
 * Revision 2.102  2006/03/23 00:24:49  csoutheren
 * Detect if ClearCall is used within OnIncomingConnection
 *
 * Revision 2.101  2006/02/22 10:54:55  csoutheren
 * Appled patch #1375120 from Frederic Heem
 * Add ARQ srcCallSignalAddress only when needed
 *
 * Revision 2.100  2006/02/22 10:48:47  csoutheren
 * Applied patch #1375144 from Frederic Heem
 * Initialize detectInBandDTMF
 *
 * Revision 2.99  2006/02/22 10:40:10  csoutheren
 * Added patch #1374583 from Frederic Heem
 * Added additional H.323 virtual function
 *
 * Revision 2.98  2006/02/22 10:29:09  csoutheren
 * Applied patch #1374470 from Frederic Heem
 * Add ability to disable H.245 negotiation
 *
 * Revision 2.97  2006/01/09 12:55:13  csoutheren
 * Fixed default calledDestinationName
 *
 * Revision 2.96  2006/01/09 12:19:07  csoutheren
 * Added member variables to capture incoming destination addresses
 *
 * Revision 2.95  2006/01/02 15:52:39  dsandras
 * Added what was required to merge changes from OpenH323 Altas_devel_2 in gkclient.cxx, gkserver.cxx and channels.cxx.
 *
 * Revision 2.94  2005/12/06 21:32:24  dsandras
 * Applied patch from Frederic Heem <frederic.heem _Atttt_ telsey.it> to fix
 * assert in PSyncPoint when OnReleased is called twice from different threads.
 * Thanks! (Patch #1374240)
 *
 * Log trimmed by CRS 20 August 2006
 *
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "h323con.h"
#endif

#include <opal/buildopts.h>

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

#if OPAL_H224
#include <h224/h323h224.h>
#endif

#ifdef OPAL_H460
#include <h323/h460.h>
#include <h323/h4601.h>
#endif

const PTimeInterval MonitorCallStatusTime(0, 10); // Seconds

#define new PNEW


/////////////////////////////////////////////////////////////////////////////

#if PTRACING

const char * H323Connection::GetConnectionStatesName(ConnectionStates s)
{
  static const char * const names[NumConnectionStates] = {
    "NoConnectionActive",
    "AwaitingGatekeeperAdmission",
    "AwaitingTransportConnect",
    "AwaitingSignalConnect",
    "AwaitingLocalAnswer",
    "HasExecutedSignalConnect",
    "EstablishedConnection",
    "ShuttingDownConnection"
  };
  return s < PARRAYSIZE(names) ? names[s] : "<Unknown>";
}

const char * H323Connection::GetFastStartStateName(FastStartStates s)
{
  static const char * const names[NumFastStartStates] = {
    "FastStartDisabled",
    "FastStartInitiate",
    "FastStartResponse",
    "FastStartAcknowledged"
  };
  return s < PARRAYSIZE(names) ? names[s] : "<Unknown>";
}
#endif

#ifdef H323_H460
static void ReceiveSetupFeatureSet(const H323Connection * connection, const H225_Setup_UUIE & pdu)
{
  H225_FeatureSet fs;
  BOOL hasFeaturePDU = FALSE;
  
  if(pdu.HasOptionalField(H225_Setup_UUIE::e_neededFeatures)) {
    fs.IncludeOptionalField(H225_FeatureSet::e_neededFeatures);
    H225_ArrayOf_FeatureDescriptor & fsn = fs.m_neededFeatures;
    fsn = pdu.m_neededFeatures;
    hasFeaturePDU = TRUE;
  }
  
  if(pdu.HasOptionalField(H225_Setup_UUIE::e_desiredFeatures)) {
    fs.IncludeOptionalField(H225_FeatureSet::e_desiredFeatures);
    H225_ArrayOf_FeatureDescriptor & fsn = fs.m_desiredFeatures;
    fsn = pdu.m_desiredFeatures;
    hasFeaturePDU = TRUE;
  }
  
  if(pdu.HasOptionalField(H225_Setup_UUIE::e_supportedFeatures)) {
    fs.IncludeOptionalField(H225_FeatureSet::e_supportedFeatures);
    H225_ArrayOf_FeatureDescriptor & fsn = fs.m_supportedFeatures;
    fsn = pdu.m_supportedFeatures;
    hasFeaturePDU = TRUE;
  }
  
  if (hasFeaturePDU) {
  	connection->OnReceiveFeatureSet(H460_MessageType::e_setup, fs);
  }
}
#endif

template <typename PDUType>
static void ReceiveFeatureSet(const H323Connection * connection, unsigned code, const PDUType & pdu)
{
  if(pdu.HasOptionalField(PDUType::e_featureSet)) {
    connection->OnReceiveFeatureSet(code, pdu.m_featureSet);
  }
}

H323Connection::H323Connection(OpalCall & call,
                               H323EndPoint & ep,
                               const PString & token,
                               const PString & alias,
                               const H323TransportAddress & address,
                               unsigned options,
                               OpalConnection::StringOptions * stringOptions)
  : OpalConnection(call, ep, token, options, stringOptions),
    endpoint(ep),
    gkAccessTokenOID(ep.GetGkAccessTokenOID())
#ifdef H323_H460
    ,features(ep.GetFeatureSet())
#endif
{
  if (alias.IsEmpty())
    remotePartyName = remotePartyAddress = address;
  else {
    remotePartyName = alias;
    remotePartyAddress = alias + '@' + address;
  }

  /* Add the local alias name in the ARQ, TODO: overwrite alias name from partyB */
  localAliasNames = ep.GetAliasNames();
  
  mediaStreams.DisallowDeleteObjects();

  gatekeeperRouted = FALSE;
  distinctiveRing = 0;
  callReference = token.Mid(token.Find('/')+1).AsUnsigned();
  remoteCallWaiting = -1;

  h225version = H225_PROTOCOL_VERSION;
  h245version = H245_PROTOCOL_VERSION;
  h245versionSet = FALSE;

  signallingChannel = NULL;
  controlChannel = NULL;
  controlListener = NULL;
  holdMediaChannel = NULL;
  isConsultationTransfer = FALSE;
  isCallIntrusion = FALSE;
#if OPAL_H450
  callIntrusionProtectionLevel = endpoint.GetCallIntrusionProtectionLevel();
#endif

  switch (options&H245TunnelingOptionMask) {
    case H245TunnelingOptionDisable :
      h245Tunneling = FALSE;
      break;

    case H245TunnelingOptionEnable :
      h245Tunneling = TRUE;
      break;

    default :
      h245Tunneling = !ep.IsH245TunnelingDisabled();
      break;
  }

  h245TunnelTxPDU = NULL;
  h245TunnelRxPDU = NULL;
  setupPDU    = NULL;
  alertingPDU = NULL;
  connectPDU = NULL;

  progressPDU = NULL;
  
  connectionState = NoConnectionActive;
  q931Cause = Q931::ErrorInCauseIE;

  uuiesRequested = 0; // Empty set
  addAccessTokenToSetup = TRUE; // Automatic inclusion of ACF access token in SETUP

  mediaWaitForConnect = FALSE;
  transmitterSidePaused = FALSE;
  transmitterMediaStream = NULL;

  switch (options&FastStartOptionMask) {
    case FastStartOptionDisable :
      fastStartState = FastStartDisabled;
      break;

    case FastStartOptionEnable :
      fastStartState = FastStartInitiate;
      break;

    default :
      fastStartState = ep.IsFastStartDisabled() ? FastStartDisabled : FastStartInitiate;
      break;
  }

  mustSendDRQ = FALSE;
  earlyStart = FALSE;
#if OPAL_T120
  startT120 = TRUE;
#endif
#if OPAL_H224
  startH224 = ep.IsH224Enabled();
#endif
  lastPDUWasH245inSETUP = FALSE;
  endSessionNeeded = FALSE;

  switch (options&H245inSetupOptionMask) {
    case H245inSetupOptionDisable :
      doH245inSETUP = FALSE;
      break;

    case H245inSetupOptionEnable :
      doH245inSETUP = TRUE;
      break;

    default :
      doH245inSETUP = !ep.IsH245inSetupDisabled();
      break;
  }

  remoteMaxAudioDelayJitter = 0;

  switch (options&DetectInBandDTMFOptionMask) {
    case DetectInBandDTMFOptionDisable :
      detectInBandDTMF = FALSE;
      break;

    case DetectInBandDTMFOptionEnable :
      detectInBandDTMF = TRUE;
      break;

    default :
      detectInBandDTMF = FALSE;
      break;
  }

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

  alertDone   = FALSE;
  
#ifdef H323_H460
  features.LoadFeatureSet(H460_Feature::FeatureSignal, this);
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
  delete signallingChannel;
  delete controlChannel;
  delete setupPDU;
  delete alertingPDU;
  delete connectPDU;
  delete progressPDU;
  delete holdMediaChannel;

  PTRACE(4, "H323\tConnection " << callToken << " deleted.");
}

void H323Connection::OnReleased()
{
  OpalConnection::OnReleased();
  CleanUpOnCallEnd();
  OnCleared();
}


void H323Connection::CleanUpOnCallEnd()
{
  PTRACE(4, "H323\tConnection " << callToken << " closing: connectionState=" << connectionState);

  connectionState = ShuttingDownConnection;

  if (!callEndTime.IsValid())
    callEndTime = PTime();

  PTRACE(3, "H225\tSending release complete PDU: callRef=" << callReference);
  H323SignalPDU rcPDU;
  rcPDU.BuildReleaseComplete(*this);
#if OPAL_H450
  h450dispatcher->AttachToReleaseComplete(rcPDU);
#endif

  BOOL sendingReleaseComplete = OnSendReleaseComplete(rcPDU);

  if (endSessionNeeded) {
    if (sendingReleaseComplete)
      h245TunnelTxPDU = &rcPDU; // Piggy back H245 on this reply

    // Send an H.245 end session to the remote endpoint.
    H323ControlPDU pdu;
    pdu.BuildEndSessionCommand(H245_EndSessionCommand::e_disconnect);
    WriteControlPDU(pdu);
  }

  if (sendingReleaseComplete) {
    h245TunnelTxPDU = NULL;
    WriteSignalPDU(rcPDU);
  }

  // Check for gatekeeper and do disengage if have one
  if (mustSendDRQ) {
    H323Gatekeeper * gatekeeper = endpoint.GetGatekeeper();
    if (gatekeeper != NULL)
      gatekeeper->DisengageRequest(*this, H225_DisengageReason::e_normalDrop);
  }

  // Unblock sync points
  digitsWaitFlag.Signal();

  // Clean up any fast start "pending" channels we may have running.
  PINDEX i;
  for (i = 0; i < fastStartChannels.GetSize(); i++)
    fastStartChannels[i].Close();
  fastStartChannels.RemoveAll();

  // Dispose of all the logical channels
  logicalChannels->RemoveAll();

  if (endSessionNeeded) {
    // Calculate time since we sent the end session command so we do not actually
    // wait for returned endSession if it has already been that long
    PTimeInterval waitTime = endpoint.GetEndSessionTimeout();
    if (callEndTime.IsValid()) {
      PTime now;
      if (now > callEndTime) { // Allow for backward motion in time (DST change)
        waitTime -= now - callEndTime;
        if (waitTime < 0)
          waitTime = 0;
      }
    }

    // Wait a while for the remote to send an endSession
    PTRACE(4, "H323\tAwaiting end session from remote for " << waitTime << " seconds");
    if (!endSessionReceived.Wait(waitTime)) {
      PTRACE(2, "H323\tDid not receive an end session from remote.");
    }
  }

  SetPhase(ReleasedPhase);

  // Wait for control channel to be cleaned up (thread ended).
  if (controlChannel != NULL)
    controlChannel->CloseWait();

  // Wait for signalling channel to be cleaned up (thread ended).
  if (signallingChannel != NULL)
    signallingChannel->CloseWait();

  PTRACE(3, "H323\tConnection " << callToken << " terminated.");
}


PString H323Connection::GetDestinationAddress()
{
  if (!localDestinationAddress)
    return localDestinationAddress;

  return OpalConnection::GetDestinationAddress();
}


void H323Connection::AttachSignalChannel(const PString & token,
                                         H323Transport * channel,
                                         BOOL answeringCall)
{
  originating = !answeringCall;

  if (signallingChannel != NULL && signallingChannel->IsOpen()) {
    PAssertAlways(PLogicError);
    return;
  }

  delete signallingChannel;
  signallingChannel = channel;

  // Set our call token for identification in endpoint dictionary
  callToken = token;
}


BOOL H323Connection::WriteSignalPDU(H323SignalPDU & pdu)
{
  PAssert(signallingChannel != NULL, PLogicError);

  lastPDUWasH245inSETUP = FALSE;

  if (signallingChannel != NULL && signallingChannel->IsOpen()) {
    pdu.m_h323_uu_pdu.m_h245Tunneling = h245Tunneling;

    H323Gatekeeper * gk = endpoint.GetGatekeeper();
    if (gk != NULL)
      gk->InfoRequestResponse(*this, pdu.m_h323_uu_pdu, TRUE);

    if (pdu.Write(*signallingChannel))
      return TRUE;
  }

  Release(EndedByTransportFail);
  return FALSE;
}


void H323Connection::HandleSignallingChannel()
{
  PAssert(signallingChannel != NULL, PLogicError);

  PTRACE(3, "H225\tReading PDUs: callRef=" << callReference);

  while (signallingChannel->IsOpen()) {
    H323SignalPDU pdu;
    if (pdu.Read(*signallingChannel)) {
      if (!HandleSignalPDU(pdu)) {
        Release(EndedByTransportFail);
        break;
      }
    }
    else if (signallingChannel->GetErrorCode() != PChannel::Timeout) {
      if (controlChannel == NULL || !controlChannel->IsOpen())
        Release(EndedByTransportFail);
      signallingChannel->Close();
      break;
    }
    else {
      switch (connectionState) {
        case AwaitingSignalConnect :
          // Had time out waiting for remote to send a CONNECT
          ClearCall(EndedByNoAnswer);
          break;
        case HasExecutedSignalConnect :
          // Have had minimum MonitorCallStatusTime delay since CONNECT but
          // still no media to move it to EstablishedConnection state. Must
          // thus not have any common codecs to use!
          ClearCall(EndedByCapabilityExchange);
          break;
        default :
          break;
      }
    }

    if (controlChannel == NULL)
      MonitorCallStatus();
  }

  // If we are the only link to the far end then indicate that we have
  // received endSession even if we hadn't, because we are now never going
  // to get one so there is no point in having CleanUpOnCallEnd wait.
  if (controlChannel == NULL)
    endSessionReceived.Signal();

  PTRACE(3, "H225\tSignal channel closed.");
}


BOOL H323Connection::HandleSignalPDU(H323SignalPDU & pdu)
{
  // Process the PDU.
  const Q931 & q931 = pdu.GetQ931();

  PTRACE(3, "H225\tHandling PDU: " << q931.GetMessageTypeName()
                    << " callRef=" << q931.GetCallReference());

  PSafeLockReadWrite safeLock(*this);
  if (!safeLock.IsLocked())
    return FALSE;

  if (GetPhase() >= ReleasingPhase) {
    // Continue to look for endSession/releaseComplete pdus
    if (pdu.m_h323_uu_pdu.m_h245Tunneling) {
      for (PINDEX i = 0; i < pdu.m_h323_uu_pdu.m_h245Control.GetSize(); i++) {
        PPER_Stream strm = pdu.m_h323_uu_pdu.m_h245Control[i].GetValue();
        if (!InternalEndSessionCheck(strm))
          break;
      }
    }
    if (q931.GetMessageType() == Q931::ReleaseCompleteMsg)
      endSessionReceived.Signal();
    return FALSE;
  }

  // If remote does not do tunneling, so we don't either. Note that if it
  // gets turned off once, it stays off for good.
  if (h245Tunneling && !pdu.m_h323_uu_pdu.m_h245Tunneling && pdu.GetQ931().HasIE(Q931::UserUserIE)) {
    masterSlaveDeterminationProcedure->Stop();
    capabilityExchangeProcedure->Stop();
    h245Tunneling = FALSE;
  }

  h245TunnelRxPDU = &pdu;

  // Check for presence of supplementary services
#if OPAL_H450
  if (pdu.m_h323_uu_pdu.HasOptionalField(H225_H323_UU_PDU::e_h4501SupplementaryService)) {
    if (!h450dispatcher->HandlePDU(pdu)) { // Process H4501SupplementaryService APDU
      return FALSE;
    }
  }
#endif

  // Add special code to detect if call is from a Cisco and remoteApplication needs setting
  if (remoteApplication.IsEmpty() && pdu.m_h323_uu_pdu.HasOptionalField(H225_H323_UU_PDU::e_nonStandardControl)) {
    for (PINDEX i = 0; i < pdu.m_h323_uu_pdu.m_nonStandardControl.GetSize(); i++) {
      const H225_NonStandardIdentifier & id = pdu.m_h323_uu_pdu.m_nonStandardControl[i].m_nonStandardIdentifier;
      if (id.GetTag() == H225_NonStandardIdentifier::e_h221NonStandard) {
        const H225_H221NonStandard & h221 = id;
        if (h221.m_t35CountryCode == 181 && h221.m_t35Extension == 0 && h221.m_manufacturerCode == 18) {
          remoteApplication = "Cisco IOS\t12.x\t181/18";
          PTRACE(3, "H225\tSet remote application name: \"" << remoteApplication << '"');
          break;
        }
      }
    }
  }

  BOOL ok;
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
      ok = FALSE;
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
    gk->InfoRequestResponse(*this, pdu.m_h323_uu_pdu, FALSE);

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
    if (remoteApplication.Find("Cisco IOS") == P_MAX_INDEX) {
      // Not Cisco, so OK to tunnel multiple PDUs
      localTunnelPDU.BuildFacility(*this, TRUE);
      h245TunnelTxPDU = &localTunnelPDU;
    }
  }

  // if a response to a SETUP PDU containing TCS/MSD was ignored, then shutdown negotiations
  PINDEX i;
  if (lastPDUWasH245inSETUP && 
      (h245TunnelRxPDU->m_h323_uu_pdu.m_h245Control.GetSize() == 0) &&
      (h245TunnelRxPDU->GetQ931().GetMessageType() != Q931::CallProceedingMsg)) {
    PTRACE(4, "H225\tH.245 in SETUP ignored - resetting H.245 negotiations");
    masterSlaveDeterminationProcedure->Stop();
    lastPDUWasH245inSETUP = FALSE;
    capabilityExchangeProcedure->Stop();
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

    if (setup.HasOptionalField(H225_Setup_UUIE::e_parallelH245Control)) {
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


static BOOL BuildFastStartList(const H323Channel & channel,
                               H225_ArrayOf_PASN_OctetString & array,
                               H323Channel::Directions reverseDirection)
{
  H245_OpenLogicalChannel open;
  const H323Capability & capability = channel.GetCapability();

  if (channel.GetDirection() != reverseDirection) {
    if (!capability.OnSendingPDU(open.m_forwardLogicalChannelParameters.m_dataType))
      return FALSE;
  }
  else {
    if (!capability.OnSendingPDU(open.m_reverseLogicalChannelParameters.m_dataType))
      return FALSE;

    open.m_forwardLogicalChannelParameters.m_multiplexParameters.SetTag(
                H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters::e_none);
    open.m_forwardLogicalChannelParameters.m_dataType.SetTag(H245_DataType::e_nullData);
    open.IncludeOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters);
  }

  if (!channel.OnSendingPDU(open))
    return FALSE;

  PTRACE(4, "H225\tBuild fastStart:\n  " << setprecision(2) << open);
  PINDEX last = array.GetSize();
  array.SetSize(last+1);
  array[last].EncodeSubType(open);

  PTRACE(3, "H225\tBuilt fastStart for " << capability);
  return TRUE;
}

void H323Connection::OnEstablished()
{
  endpoint.OnConnectionEstablished(*this, callToken);
  endpoint.OnEstablished(*this);
}

void H323Connection::OnSendARQ(H225_AdmissionRequest & arq)
{
  endpoint.OnSendARQ(*this, arq);
}

void H323Connection::OnCleared()
{
  endpoint.OnConnectionCleared(*this, callToken);
}


void H323Connection::SetRemoteVersions(const H225_ProtocolIdentifier & protocolIdentifier)
{
  if (protocolIdentifier.GetSize() < 6)
    return;

  h225version = protocolIdentifier[5];

  if (h245versionSet) {
    PTRACE(3, "H225\tSet protocol version to " << h225version);
    return;
  }

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


BOOL H323Connection::OnReceivedSignalSetup(const H323SignalPDU & originalSetupPDU)
{
  if (originalSetupPDU.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_setup)
    return FALSE;

  setupPDU = new H323SignalPDU(originalSetupPDU);

  H225_Setup_UUIE & setup = setupPDU->m_h323_uu_pdu.m_h323_message_body;

  switch (setup.m_conferenceGoal.GetTag()) {
    case H225_Setup_UUIE_conferenceGoal::e_create:
    case H225_Setup_UUIE_conferenceGoal::e_join:
      break;

    case H225_Setup_UUIE_conferenceGoal::e_invite:
      return endpoint.OnConferenceInvite(*setupPDU);

    case H225_Setup_UUIE_conferenceGoal::e_callIndependentSupplementaryService:
      return endpoint.OnCallIndependentSupplementaryService(*setupPDU);

    case H225_Setup_UUIE_conferenceGoal::e_capability_negotiation:
      return endpoint.OnNegotiateConferenceCapabilities(*setupPDU);
  }

  SetRemoteVersions(setup.m_protocolIdentifier);

  // Get the ring pattern
  distinctiveRing = setupPDU->GetDistinctiveRing();

  // Save the identifiers sent by caller
  if (setup.HasOptionalField(H225_Setup_UUIE::e_callIdentifier))
    callIdentifier = setup.m_callIdentifier.m_guid;
  conferenceIdentifier = setup.m_conferenceID;
  SetRemoteApplication(setup.m_sourceInfo);

  // Determine the remote parties name/number/address as best we can
  setupPDU->GetQ931().GetCallingPartyNumber(remotePartyNumber);
  remotePartyName = setupPDU->GetSourceAliases(signallingChannel);

  // get the destination number and name, just in case we are a gateway
  if (setup.m_destinationAddress.GetSize() == 0)
    calledDestinationName = signallingChannel->GetLocalAddress();
  else 
    calledDestinationName = H323GetAliasAddressString(setup.m_destinationAddress[0]);
  setupPDU->GetQ931().GetCalledPartyNumber(calledDestinationNumber);

  // get the peer address
  remotePartyAddress = signallingChannel->GetRemoteAddress();
  if (setup.m_sourceAddress.GetSize() > 0)
    remotePartyAddress = H323GetAliasAddressString(setup.m_sourceAddress[0]) + '@' + signallingChannel->GetRemoteAddress();

  // compare the source call signalling address
  if (setup.HasOptionalField(H225_Setup_UUIE::e_sourceCallSignalAddress)) {

    H323TransportAddress sourceAddress(setup.m_sourceCallSignalAddress);

    //
    // two condition for detecting remote is behind a NAT
    //   1. the signalling address is not private, but the TCP source address is
    //   2. the signalling and TCP source address are both private, but not the same
    //
    // but don't enable NAT usage if local endpoint is configured to be behind a NAT
    //
    PIPSocket::Address srcAddr, sigAddr;
    sourceAddress.GetIpAddress(srcAddr);
    signallingChannel->GetRemoteAddress().GetIpAddress(sigAddr);
    if (
        (!sigAddr.IsRFC1918() && srcAddr.IsRFC1918()) ||                         
        ((sigAddr.IsRFC1918() && srcAddr.IsRFC1918()) && (sigAddr != srcAddr))
        )  
    {
      PIPSocket::Address localAddress;
      signallingChannel->GetLocalAddress().GetIpAddress(localAddress);

      PIPSocket::Address ourAddress = localAddress;
      endpoint.TranslateTCPAddress(localAddress, sigAddr);

      if (localAddress != ourAddress) {
        PTRACE(3, "H225\tSource signal address " << srcAddr << " and TCP peer address " << sigAddr << " indicate remote endpoint is behind NAT");
        remoteIsNAT = TRUE;
      }
    }
  }

  // Anything else we need from setup PDU
  mediaWaitForConnect = setup.m_mediaWaitForConnect;
  if (!setupPDU->GetQ931().GetCalledPartyNumber(localDestinationAddress)) {
    localDestinationAddress = setupPDU->GetDestinationAlias(TRUE);
    if (signallingChannel->GetLocalAddress().IsEquivalent(localDestinationAddress))
      localDestinationAddress = '*';
  }
  
#ifdef H323_H460
  ReceiveSetupFeatureSet(this, setup);
#endif

  // Send back a H323 Call Proceeding PDU in case OnIncomingCall() takes a while
  PTRACE(3, "H225\tSending call proceeding PDU");
  H323SignalPDU callProceedingPDU;
  H225_CallProceeding_UUIE & callProceeding = callProceedingPDU.BuildCallProceeding(*this);

  if (!isConsultationTransfer) {
    if (OnSendCallProceeding(callProceedingPDU)) {
      if (fastStartState == FastStartDisabled)
        callProceeding.IncludeOptionalField(H225_CallProceeding_UUIE::e_fastConnectRefused);

      if (!WriteSignalPDU(callProceedingPDU))
        return FALSE;
    }

    /** Here is a spot where we should wait in case of Call Intrusion
	for CIPL from other endpoints 
	if (isCallIntrusion) return TRUE;
    */

    // if the application indicates not to contine, then send a Q931 Release Complete PDU
    alertingPDU = new H323SignalPDU;
    alertingPDU->BuildAlerting(*this);

    /** If we have a case of incoming call intrusion we should not Clear the Call*/
    if (!OnIncomingCall(*setupPDU, *alertingPDU) && (!isCallIntrusion)) {
      Release(EndedByNoAccept);
      PTRACE(2, "H225\tApplication not accepting calls");
      return FALSE;
    }
    if ((phase == ReleasingPhase) || (phase == ReleasedPhase)) {
      PTRACE(1, "H225\tApplication called ClearCall during OnIncomingCall");
      return FALSE;
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
            Release(EndedByGatekeeper);
        }
        return FALSE;
      }

      if (destExtraCallInfoArray.GetSize() > 0)
        destExtraCallInfo = H323GetAliasAddressString(destExtraCallInfoArray[0]);
      mustSendDRQ = TRUE;
      gatekeeperRouted = response.gatekeeperRouted;
    }
  }

  if (!OnOpenIncomingMediaChannels())
    return FALSE;

  return connectionState != ShuttingDownConnection;
}

BOOL H323Connection::OnOpenIncomingMediaChannels()
{
  ApplyStringOptions();

  H225_Setup_UUIE & setup = setupPDU->m_h323_uu_pdu.m_h323_message_body;

  // in some circumstances, the peer OpalConnection needs to see the newly arrived media formats
  // before it knows what what formats can support. 
  if (setup.HasOptionalField(H225_Setup_UUIE::e_fastStart)) {

    OpalMediaFormatList previewFormats;

    // Extract capabilities from the fast start OpenLogicalChannel structures
    PINDEX i;
    for (i = 0; i < setup.m_fastStart.GetSize(); i++) {
      H245_OpenLogicalChannel open;
      if (setup.m_fastStart[i].DecodeSubType(open)) {
        const H245_H2250LogicalChannelParameters * param;
        const H245_DataType * dataType = NULL;
        H323Channel::Directions direction;
        if (open.HasOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters)) {
          if (open.m_reverseLogicalChannelParameters.m_multiplexParameters.GetTag() ==
                H245_OpenLogicalChannel_reverseLogicalChannelParameters_multiplexParameters::e_h2250LogicalChannelParameters) {
            PTRACE(3, "H323\tCreateLogicalChannel - reverse channel");
            dataType = &open.m_reverseLogicalChannelParameters.m_dataType;
            param = &(const H245_H2250LogicalChannelParameters &)open.m_reverseLogicalChannelParameters.m_multiplexParameters;
            direction = H323Channel::IsTransmitter;
          }
        }
        else if (open.m_forwardLogicalChannelParameters.m_multiplexParameters.GetTag() ==
              H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters
                                                      ::e_h2250LogicalChannelParameters) {
          dataType = &open.m_forwardLogicalChannelParameters.m_dataType;
          param = &(const H245_H2250LogicalChannelParameters &)
                      open.m_forwardLogicalChannelParameters.m_multiplexParameters;
          direction = H323Channel::IsReceiver;
        }
        if (dataType != NULL) {
          H323Capability * capability = endpoint.FindCapability(*dataType);
          if (capability != NULL)
            previewFormats += capability->GetMediaFormat();
        }
      }
    }

    if (previewFormats.GetSize() != 0) 
      ownerCall.GetOtherPartyConnection(*this)->PreviewPeerMediaFormats(previewFormats);
  }

  // Get the local capabilities before fast start or tunnelled TCS is handled
  OnSetLocalCapabilities();

  // Check that it has the H.245 channel connection info
  if (setup.HasOptionalField(H225_Setup_UUIE::e_h245Address) && (!setupPDU->m_h323_uu_pdu.m_h245Tunneling || endpoint.IsH245TunnelingDisabled()))
    if (!CreateOutgoingControlChannel(setup.m_h245Address))
      return FALSE;

  PINDEX i;

  // See if remote endpoint wants to start fast
  if ((fastStartState != FastStartDisabled) && setup.HasOptionalField(H225_Setup_UUIE::e_fastStart)) {
    PTRACE(3, "H225\tFast start detected");

    // If we have not received caps from remote, we are going to build a
    // fake one from the fast connect data.
    if (!capabilityExchangeProcedure->HasReceivedCapabilities())
      remoteCapabilities.RemoveAll();

    // Extract capabilities from the fast start OpenLogicalChannel structures
    for (i = 0; i < setup.m_fastStart.GetSize(); i++) {
      H245_OpenLogicalChannel open;
      if (setup.m_fastStart[i].DecodeSubType(open)) {
        PTRACE(4, "H225\tFast start open:\n  " << setprecision(2) << open);
        unsigned error;
        H323Channel * channel = CreateLogicalChannel(open, TRUE, error);
        if (channel != NULL) {
          if (channel->GetDirection() == H323Channel::IsTransmitter)
            channel->SetNumber(logicalChannels->GetNextChannelNumber());
          fastStartChannels.Append(channel);
        }
      }
      else {
        PTRACE(1, "H225\tInvalid fast start PDU decode:\n  " << open);
      }
    }

    PTRACE(3, "H225\tOpened " << fastStartChannels.GetSize() << " fast start channels");

    // If we are incapable of ANY of the fast start channels, don't do fast start
    if (!fastStartChannels.IsEmpty())
      fastStartState = FastStartResponse;
  }

  // Build the reply with the channels we are actually using
  connectPDU = new H323SignalPDU;
  connectPDU->BuildConnect(*this);
  
  progressPDU = new H323SignalPDU;
  progressPDU->BuildProgress(*this);

  // OK are now ready to send SETUP to remote protocol
  ownerCall.OnSetUp(*this);

#if OPAL_H450
  if (connectionState == NoConnectionActive) {
    /** If Call Intrusion is allowed we must answer the call*/
    if (IsCallIntrusion()) {
      AnsweringCall(AnswerCallDeferred);
    }
    else {
      if (isConsultationTransfer)
        AnsweringCall(AnswerCallNow);
      else {
        // call the application callback to determine if to answer the call or not
        connectionState = AwaitingLocalAnswer;
        SetPhase(AlertingPhase);
        AnsweringCall(OnAnswerCall(remotePartyName, *setupPDU, *connectPDU, *progressPDU));
      }
    }
  }
#endif

  return connectionState != ShuttingDownConnection;
}


void H323Connection::SetLocalPartyName(const PString & name)
{
  localPartyName = name;

  if (!name.IsEmpty()) {
    localAliasNames.RemoveAll();
    localAliasNames.AppendString(name);
  }
}


void H323Connection::SetRemotePartyInfo(const H323SignalPDU & pdu)
{
  PString newNumber;
  if (pdu.GetQ931().GetCalledPartyNumber(newNumber))
    remotePartyNumber = newNumber;

  PString remoteHostName = signallingChannel->GetRemoteAddress().GetHostName();

  PString newRemotePartyName = pdu.GetQ931().GetDisplayName();
  if (newRemotePartyName.IsEmpty() || newRemotePartyName == remoteHostName)
    remotePartyName = remoteHostName;
  else
    remotePartyName = newRemotePartyName + " [" + remoteHostName + ']';

  PTRACE(3, "H225\tSet remote party name: \"" << remotePartyName << '"');
}


void H323Connection::SetRemoteApplication(const H225_EndpointType & pdu)
{
  if (pdu.HasOptionalField(H225_EndpointType::e_vendor)) {
    remoteApplication = H323GetApplicationInfo(pdu.m_vendor);
    PTRACE(3, "H225\tSet remote application name: \"" << remoteApplication << '"');
  }
}


const PString H323Connection::GetRemotePartyCallbackURL() const
{
  PString remote;
  PINDEX j;
  
  if (IsGatekeeperRouted()) {

    remote = GetRemotePartyNumber();
  }
  
  if (remote.IsEmpty() && signallingChannel) {
  
    remote = signallingChannel->GetRemoteAddress();

    /* The address is formated the H.323 way */
    j = remote.FindLast("$");
    if (j != P_MAX_INDEX)
      remote = remote.Mid (j+1);
    j = remote.FindLast(":");
    if (j != P_MAX_INDEX)
      remote = remote.Left (j);
  }

  remote = "h323:" + remote;
  
  return remote;
}


BOOL H323Connection::OnReceivedSignalSetupAck(const H323SignalPDU & /*setupackPDU*/)
{
  OnInsufficientDigits();
  return TRUE;
}


BOOL H323Connection::OnReceivedSignalInformation(const H323SignalPDU & /*infoPDU*/)
{
  return TRUE;
}


BOOL H323Connection::OnReceivedCallProceeding(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_callProceeding)
    return FALSE;
  const H225_CallProceeding_UUIE & call = pdu.m_h323_uu_pdu.m_h323_message_body;

  SetRemoteVersions(call.m_protocolIdentifier);
  SetRemotePartyInfo(pdu);
  SetRemoteApplication(call.m_destinationInfo);
  
#ifdef H323_H460
  ReceiveFeatureSet<H225_CallProceeding_UUIE>(this, H460_MessageType::e_callProceeding, call);
#endif

  // Check for fastStart data and start fast
  if (call.HasOptionalField(H225_CallProceeding_UUIE::e_fastStart))
    HandleFastStartAcknowledge(call.m_fastStart);

  // Check that it has the H.245 channel connection info
  if (call.HasOptionalField(H225_CallProceeding_UUIE::e_h245Address) && (!pdu.m_h323_uu_pdu.m_h245Tunneling || endpoint.IsH245TunnelingDisabled()))
    CreateOutgoingControlChannel(call.m_h245Address);

  return TRUE;
}


BOOL H323Connection::OnReceivedProgress(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_progress)
    return FALSE;
  const H225_Progress_UUIE & progress = pdu.m_h323_uu_pdu.m_h323_message_body;

  SetRemoteVersions(progress.m_protocolIdentifier);
  SetRemotePartyInfo(pdu);
  SetRemoteApplication(progress.m_destinationInfo);

  // Check for fastStart data and start fast
  if (progress.HasOptionalField(H225_Progress_UUIE::e_fastStart))
    HandleFastStartAcknowledge(progress.m_fastStart);

  // Check that it has the H.245 channel connection info
  if (progress.HasOptionalField(H225_Progress_UUIE::e_h245Address) && (!pdu.m_h323_uu_pdu.m_h245Tunneling || endpoint.IsH245TunnelingDisabled()))
    return CreateOutgoingControlChannel(progress.m_h245Address);

  return TRUE;
}


BOOL H323Connection::OnReceivedAlerting(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_alerting)
    return FALSE;
  const H225_Alerting_UUIE & alert = pdu.m_h323_uu_pdu.m_h323_message_body;

  if (alertDone) 
	return TRUE;

  SetRemoteVersions(alert.m_protocolIdentifier);
  SetRemotePartyInfo(pdu);
  SetRemoteApplication(alert.m_destinationInfo);
  
#ifdef H323_H460
  ReceiveFeatureSet<H225_Alerting_UUIE>(this, H460_MessageType::e_alerting, alert);
#endif

  // Check for fastStart data and start fast
  if (alert.HasOptionalField(H225_Alerting_UUIE::e_fastStart))
    HandleFastStartAcknowledge(alert.m_fastStart);

  // Check that it has the H.245 channel connection info
  if (alert.HasOptionalField(H225_Alerting_UUIE::e_h245Address) && (!pdu.m_h323_uu_pdu.m_h245Tunneling || endpoint.IsH245TunnelingDisabled()))
    if (!CreateOutgoingControlChannel(alert.m_h245Address))
      return FALSE;

  alertDone = TRUE;
  alertingTime = PTime();

  return OnAlerting(pdu, remotePartyName);
}


BOOL H323Connection::OnReceivedSignalConnect(const H323SignalPDU & pdu)
{
  if (!alertDone) {
    alertDone = TRUE;
    alertingTime = PTime();
		if (!OnAlerting(pdu, remotePartyName))
			return FALSE;
  }

  if (connectionState == ShuttingDownConnection)
    return FALSE;
  connectionState = HasExecutedSignalConnect;
  SetPhase(ConnectedPhase);

  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_connect)
    return FALSE;
  const H225_Connect_UUIE & connect = pdu.m_h323_uu_pdu.m_h323_message_body;

  SetRemoteVersions(connect.m_protocolIdentifier);
  SetRemotePartyInfo(pdu);
  SetRemoteApplication(connect.m_destinationInfo);
  
#ifdef H323_H460
  ReceiveFeatureSet<H225_Connect_UUIE>(this, H460_MessageType::e_connect, connect);
#endif

  if (!OnOutgoingCall(pdu)) {
    Release(EndedByNoAccept);
    return FALSE;
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
  signallingChannel->SetReadTimeout(MonitorCallStatusTime);

  // Check for fastStart data and start fast
  if (connect.HasOptionalField(H225_Connect_UUIE::e_fastStart))
    HandleFastStartAcknowledge(connect.m_fastStart);

  // Check that it has the H.245 channel connection info
  if (connect.HasOptionalField(H225_Connect_UUIE::e_h245Address) && (!pdu.m_h323_uu_pdu.m_h245Tunneling || endpoint.IsH245TunnelingDisabled())) {
    if (!endpoint.IsH245Disabled() && (!CreateOutgoingControlChannel(connect.m_h245Address))) {
      if (fastStartState != FastStartAcknowledged)
        return FALSE;
    }
  }

  // If didn't get fast start channels accepted by remote then clear our
  // proposed channels
  if (fastStartState != FastStartAcknowledged) {
    fastStartState = FastStartDisabled;
    fastStartChannels.RemoveAll();
  }
  else if (mediaWaitForConnect) {
    // Otherwise start fast started channels if we were waiting for CONNECT
    for (PINDEX i = 0; i < fastStartChannels.GetSize(); i++)
      fastStartChannels[i].Start();
  }

  connectedTime = PTime();
  OnConnected();
  InternalEstablishedConnectionCheck();

  /* do not start h245 negotiation if it is disabled */
  if (endpoint.IsH245Disabled()){
    PTRACE(3, "H245\tOnReceivedSignalConnect: h245 is disabled, do not start negotiation");
    return TRUE;
  }  
  // If we have a H.245 channel available, bring it up. We either have media
  // and this is just so user indications work, or we don't have media and
  // desperately need it!
  if (h245Tunneling || controlChannel != NULL)
    return StartControlNegotiations();

  // We have no tunnelling and not separate channel, but we really want one
  // so we will start one using a facility message
  PTRACE(3, "H225\tNo H245 address provided by remote, starting control channel");

  H323SignalPDU want245PDU;
  H225_Facility_UUIE * fac = want245PDU.BuildFacility(*this, FALSE);
  fac->m_reason.SetTag(H225_FacilityReason::e_startH245);
  fac->IncludeOptionalField(H225_Facility_UUIE::e_h245Address);

  if (!CreateIncomingControlChannel(fac->m_h245Address))
    return FALSE;

  return WriteSignalPDU(want245PDU);
}


BOOL H323Connection::OnReceivedFacility(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_empty)
    return TRUE;

  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_facility)
    return FALSE;
  const H225_Facility_UUIE & fac = pdu.m_h323_uu_pdu.m_h323_message_body;
  
#ifdef H323_H460
  ReceiveFeatureSet<H225_Facility_UUIE>(this, H460_MessageType::e_facility, fac);
#endif

  SetRemoteVersions(fac.m_protocolIdentifier);

  // Check for fastStart data and start fast
  if (fac.HasOptionalField(H225_Facility_UUIE::e_fastStart))
    HandleFastStartAcknowledge(fac.m_fastStart);

  // Check that it has the H.245 channel connection info
  if (fac.HasOptionalField(H225_Facility_UUIE::e_h245Address) && (!pdu.m_h323_uu_pdu.m_h245Tunneling || endpoint.IsH245TunnelingDisabled())) {
    if (controlChannel != NULL) {
      // Fix race condition where both side want to open H.245 channel. we have
      // channel bit it is not open (ie we are listening) and the remote has
      // sent us an address to connect to. To resolve we compare the addresses.

      H323TransportAddress h323Address = controlChannel->GetLocalAddress();
      H225_TransportAddress myAddress;
      h323Address.SetPDU(myAddress);
      PPER_Stream myBuffer;
      myAddress.Encode(myBuffer);

      PPER_Stream otherBuffer;
      fac.m_h245Address.Encode(otherBuffer);

      if (myBuffer < otherBuffer) {
        PTRACE(2, "H225\tSimultaneous start of H.245 channel, connecting to remote.");
        controlChannel->CloseWait();
        delete controlChannel;
        controlChannel = NULL;
      }
      else {
        PTRACE(2, "H225\tSimultaneous start of H.245 channel, using local listener.");
      }
    }

    return CreateOutgoingControlChannel(fac.m_h245Address);
  }

  if (fac.m_reason.GetTag() != H225_FacilityReason::e_callForwarded)
    return TRUE;

  PString address;
  if (fac.HasOptionalField(H225_Facility_UUIE::e_alternativeAliasAddress) &&
      fac.m_alternativeAliasAddress.GetSize() > 0)
    address = H323GetAliasAddressString(fac.m_alternativeAliasAddress[0]);

  if (fac.HasOptionalField(H225_Facility_UUIE::e_alternativeAddress)) {
    if (!address)
      address += '@';
    address += H323TransportAddress(fac.m_alternativeAddress);
  }

  if (endpoint.OnConnectionForwarded(*this, address, pdu)) {
    Release(EndedByCallForwarded);
    return FALSE;
  }
  
  if (!endpoint.OnForwarded(*this, address)) {
    Release(EndedByCallForwarded);
    return FALSE;
  }

  if (!endpoint.CanAutoCallForward())
    return TRUE;

  if (!endpoint.ForwardConnection(*this, address, pdu))
    return TRUE;

  return FALSE;
}


BOOL H323Connection::OnReceivedSignalNotify(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_notify) {
    const H225_Notify_UUIE & notify = pdu.m_h323_uu_pdu.m_h323_message_body;
    SetRemoteVersions(notify.m_protocolIdentifier);
  }
  return TRUE;
}


BOOL H323Connection::OnReceivedSignalStatus(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_status) {
    const H225_Status_UUIE & status = pdu.m_h323_uu_pdu.m_h323_message_body;
    SetRemoteVersions(status.m_protocolIdentifier);
  }
  return TRUE;
}


BOOL H323Connection::OnReceivedStatusEnquiry(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_statusInquiry) {
    const H225_StatusInquiry_UUIE & status = pdu.m_h323_uu_pdu.m_h323_message_body;
    SetRemoteVersions(status.m_protocolIdentifier);
  }

  H323SignalPDU reply;
  reply.BuildStatus(*this);
  return reply.Write(*signallingChannel);
}


void H323Connection::OnReceivedReleaseComplete(const H323SignalPDU & pdu)
{
  if (!callEndTime.IsValid())
    callEndTime = PTime();

  endSessionReceived.Signal();

  if (q931Cause == Q931::ErrorInCauseIE)
    q931Cause = pdu.GetQ931().GetCause();
  
#ifdef H323_H460
  const H225_ReleaseComplete_UUIE & rc = pdu.m_h323_uu_pdu.m_h323_message_body;
#endif

  switch (connectionState) {
    case EstablishedConnection :
      Release(EndedByRemoteUser);
      break;

    case AwaitingLocalAnswer :
      Release(EndedByCallerAbort);
      break;

    default :
      if (callEndReason == EndedByRefusal)
        callEndReason = NumCallEndReasons;
      
      // Are we involved in a transfer with a non H.450.2 compatible transferred-to endpoint?
#if OPAL_H450
      if (h4502handler->GetState() == H4502Handler::e_ctAwaitSetupResponse &&
          h4502handler->IsctTimerRunning())
      {
        PTRACE(4, "H4502\tThe Remote Endpoint has rejected our transfer request and does not support H.450.2.");
        h4502handler->OnReceivedSetupReturnError(H4501_GeneralErrorList::e_notAvailable);
      }
#endif
		  
#ifdef H323_H460
      ReceiveFeatureSet<H225_ReleaseComplete_UUIE>(this, H460_MessageType::e_releaseComplete, rc);
#endif

      if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_releaseComplete)
        Release(EndedByRefusal);
      else {
        const H225_ReleaseComplete_UUIE & rc = pdu.m_h323_uu_pdu.m_h323_message_body;
        SetRemoteVersions(rc.m_protocolIdentifier);
        Release(H323TranslateToCallEndReason(pdu.GetQ931().GetCause(), rc.m_reason));
      }
  }
}


BOOL H323Connection::OnIncomingCall(const H323SignalPDU & setupPDU,
                                    H323SignalPDU & alertingPDU)
{
  return endpoint.OnIncomingCall(*this, setupPDU, alertingPDU);
}


BOOL H323Connection::ForwardCall(const PString & forwardParty)
{
  if (forwardParty.IsEmpty())
    return FALSE;

  PString alias;
  H323TransportAddress address;
  endpoint.ParsePartyName(forwardParty, alias, address);

  H323SignalPDU redirectPDU;
  H225_Facility_UUIE * fac = redirectPDU.BuildFacility(*this, FALSE);

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

  return TRUE;
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
  return OpalConnection::OnAnswerCall(caller);
}

void H323Connection::AnsweringCall(AnswerCallResponse response)
{
  PTRACE(3, "H323\tAnswering call: " << response);

  PSafeLockReadWrite safeLock(*this);
  if (!safeLock.IsLocked() || GetPhase() >= ReleasingPhase)
    return;

  switch (response) {
    default : // AnswerCallDeferred
      break;

    case AnswerCallProgress:
    {
      H323SignalPDU want245PDU;
      want245PDU.BuildProgress(*this);
      WriteSignalPDU(want245PDU);
      break;
    }  
    case AnswerCallDeferredWithMedia :
      if (!mediaWaitForConnect) {
        // create a new facility PDU if doing AnswerDeferredWithMedia
        H323SignalPDU want245PDU;
        H225_Progress_UUIE & prog = want245PDU.BuildProgress(*this);

        BOOL sendPDU = TRUE;

        if (SendFastStartAcknowledge(prog.m_fastStart))
          prog.IncludeOptionalField(H225_Progress_UUIE::e_fastStart);
        else {
          // See if aborted call
          if (connectionState == ShuttingDownConnection)
            break;

          // Do early H.245 start
          H225_Facility_UUIE & fac = *want245PDU.BuildFacility(*this, FALSE);
          fac.m_reason.SetTag(H225_FacilityReason::e_startH245);
          earlyStart = TRUE;
          if (!h245Tunneling && (controlChannel == NULL) && !endpoint.IsH245Disabled()) {
            if (!CreateIncomingControlChannel(fac.m_h245Address))
              break;

            fac.IncludeOptionalField(H225_Facility_UUIE::e_h245Address);
          } 
          else
            sendPDU = FALSE;
        }

        if (sendPDU) {
          HandleTunnelPDU(&want245PDU);
          WriteSignalPDU(want245PDU);
        }
      }
      break;

    case AnswerCallAlertWithMedia :
      SetAlerting(localPartyName, TRUE);
      break;

    case AnswerCallPending :
      SetAlerting(localPartyName, FALSE);
      break;

    case AnswerCallDenied :
      // If response is denied, abort the call
      PTRACE(2, "H225\tApplication has declined to answer incoming call");
      Release(EndedByAnswerDenied);
      break;

    case AnswerCallNow :
      SetConnected();
  }

  InternalEstablishedConnectionCheck();
}


BOOL H323Connection::SetUpConnection()
{
  ApplyStringOptions();

  signallingChannel->AttachThread(PThread::Create(PCREATE_NOTIFIER(StartOutgoing), 0,
                                  PThread::NoAutoDeleteThread,
                                  PThread::NormalPriority,
                                  "H225 Caller:%x"));
  return TRUE;
}


void H323Connection::StartOutgoing(PThread &, INT)
{
  PTRACE(3, "H225\tStarted call thread");

  if (!SafeReference())
    return;

  PString alias, address;
  PINDEX at = remotePartyAddress.Find('@');
  if (at == P_MAX_INDEX)
    address = remotePartyAddress;
  else {
    alias = remotePartyAddress.Left(at);
    address = remotePartyAddress.Mid(at+1);
  }

  H323TransportAddress h323addr(address, endpoint.GetDefaultSignalPort());
  CallEndReason reason = SendSignalSetup(alias, h323addr);

  // Check if had an error, clear call if so
  if (reason != NumCallEndReasons)
    Release(reason);
  else
    HandleSignallingChannel();

  SafeDereference();
}


OpalConnection::CallEndReason H323Connection::SendSignalSetup(const PString & alias,
                                                              const H323TransportAddress & address)
{
  PSafeLockReadWrite safeLock(*this);
  if (!safeLock.IsLocked())
    return EndedByCallerAbort;

  // Start the call, first state is asking gatekeeper
  connectionState = AwaitingGatekeeperAdmission;
  SetPhase(SetUpPhase);

  // Start building the setup PDU to get various ID's
  H323SignalPDU setupPDU;
  H225_Setup_UUIE & setup = setupPDU.BuildSetup(*this, address);

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
  H225_ArrayOf_AliasAddress newAliasAddresses;
  if (gatekeeper != NULL) {
    H323Gatekeeper::AdmissionResponse response;
    response.transportAddress = &gatekeeperRoute;
    response.aliasAddresses = &newAliasAddresses;
    if (!gkAccessTokenOID)
      response.accessTokenData = &gkAccessTokenData;
    for (;;) {
      safeLock.Unlock();
      BOOL ok = gatekeeper->AdmissionRequest(*this, response, alias.IsEmpty());
      if (!safeLock.Lock())
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
        if (GetPhase() >= ReleasingPhase)
          return EndedByCallerAbort;
      }
    }
    mustSendDRQ = TRUE;
    if (response.gatekeeperRouted) {
      setup.IncludeOptionalField(H225_Setup_UUIE::e_endpointIdentifier);
      setup.m_endpointIdentifier = gatekeeper->GetEndpointIdentifier();
      gatekeeperRouted = TRUE;
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
    PINDEX comma = gkAccessTokenOID.Find(',');
    if (comma == P_MAX_INDEX)
      oid1 = oid2 = gkAccessTokenOID;
    else {
      oid1 = gkAccessTokenOID.Left(comma);
      oid2 = gkAccessTokenOID.Mid(comma+1);
    }
    setup.IncludeOptionalField(H225_Setup_UUIE::e_tokens);
    PINDEX last = setup.m_tokens.GetSize();
    setup.m_tokens.SetSize(last+1);
    setup.m_tokens[last].m_tokenOID = oid1;
    setup.m_tokens[last].IncludeOptionalField(H235_ClearToken::e_nonStandard);
    setup.m_tokens[last].m_nonStandard.m_nonStandardIdentifier = oid2;
    setup.m_tokens[last].m_nonStandard.m_data = gkAccessTokenData;
  }

  if (!signallingChannel->SetRemoteAddress(gatekeeperRoute)) {
    PTRACE(1, "H225\tInvalid "
           << (gatekeeperRoute != address ? "gatekeeper" : "user")
           << " supplied address: \"" << gatekeeperRoute << '"');
    connectionState = AwaitingTransportConnect;
    return EndedByConnectFail;
  }

  // Do the transport connect
  connectionState = AwaitingTransportConnect;
  SetPhase(SetUpPhase);

  // Release the mutex as can deadlock trying to clear call during connect.
  safeLock.Unlock();

  BOOL connectFailed = !signallingChannel->Connect();

    // Lock while checking for shutting down.
  if (!safeLock.Lock())
    return EndedByCallerAbort;

  if (GetPhase() >= ReleasingPhase)
    return EndedByCallerAbort;

  // See if transport connect failed, abort if so.
  if (connectFailed) {
    connectionState = NoConnectionActive;
    SetPhase(UninitialisedPhase);
    switch (signallingChannel->GetErrorNumber()) {
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
  H323TransportAddress transportAddress = signallingChannel->GetLocalAddress();
  setup.IncludeOptionalField(H225_Setup_UUIE::e_sourceCallSignalAddress);
  transportAddress.SetPDU(setup.m_sourceCallSignalAddress);
  if (!setup.HasOptionalField(H225_Setup_UUIE::e_destCallSignalAddress)) {
    transportAddress = signallingChannel->GetRemoteAddress();
    setup.IncludeOptionalField(H225_Setup_UUIE::e_destCallSignalAddress);
    transportAddress.SetPDU(setup.m_destCallSignalAddress);
  }

  ApplyStringOptions();

  // Get the local capabilities before fast start is handled
  OnSetLocalCapabilities();

  // Ask the application what channels to open
  PTRACE(3, "H225\tCheck for Fast start by local endpoint");
  fastStartChannels.RemoveAll();
  OnSelectLogicalChannels();

  // If application called OpenLogicalChannel, put in the fastStart field
  if (!fastStartChannels.IsEmpty()) {
    PTRACE(3, "H225\tFast start begun by local endpoint");
    for (PINDEX i = 0; i < fastStartChannels.GetSize(); i++)
      BuildFastStartList(fastStartChannels[i], setup.m_fastStart, H323Channel::IsReceiver);
    if (setup.m_fastStart.GetSize() > 0)
      setup.IncludeOptionalField(H225_Setup_UUIE::e_fastStart);
  }

  // Search the capability set and see if we have video capability
  BOOL hasVideoOrData = FALSE;
  for (PINDEX i = 0; i < localCapabilities.GetSize(); i++) {
    if (!PIsDescendant(&localCapabilities[i], H323AudioCapability) &&
        !PIsDescendant(&localCapabilities[i], H323_UserInputCapability)) {
      hasVideoOrData = TRUE;
      break;
    }
  }
  if (hasVideoOrData)
    setupPDU.GetQ931().SetBearerCapabilities(Q931::TransferUnrestrictedDigital, 6);

  if (!OnSendSignalSetup(setupPDU))
    return EndedByNoAccept;

  // Do this again (was done when PDU was constructed) in case
  // OnSendSignalSetup() changed something.
  setupPDU.SetQ931Fields(*this, TRUE);
  setupPDU.GetQ931().GetCalledPartyNumber(remotePartyNumber);

  fastStartState = FastStartDisabled;
  BOOL set_lastPDUWasH245inSETUP = FALSE;

  if (h245Tunneling && doH245inSETUP && !endpoint.IsH245Disabled()) {
    h245TunnelTxPDU = &setupPDU;

    // Try and start the master/slave and capability exchange through the tunnel
    // Note: this used to be disallowed but is now allowed as of H323v4
    BOOL ok = StartControlNegotiations();

    h245TunnelTxPDU = NULL;

    if (!ok)
      return EndedByTransportFail;

    if (doH245inSETUP && (setup.m_fastStart.GetSize() > 0)) {

      // Now if fast start as well need to put this in setup specific field
      // and not the generic H.245 tunneling field
      setup.IncludeOptionalField(H225_Setup_UUIE::e_parallelH245Control);
      setup.m_parallelH245Control = setupPDU.m_h323_uu_pdu.m_h245Control;
      setupPDU.m_h323_uu_pdu.RemoveOptionalField(H225_H323_UU_PDU::e_h245Control);
      set_lastPDUWasH245inSETUP = TRUE;
    }
  }

  // Send the initial PDU
  if (!WriteSignalPDU(setupPDU))
    return EndedByTransportFail;

  // WriteSignalPDU always resets lastPDUWasH245inSETUP.
  // So set it here if required
  if (set_lastPDUWasH245inSETUP)
    lastPDUWasH245inSETUP = TRUE;

  // Set timeout for remote party to answer the call
  signallingChannel->SetReadTimeout(endpoint.GetSignallingChannelCallTimeout());

  connectionState = AwaitingSignalConnect;
  SetPhase(AlertingPhase);

  return NumCallEndReasons;
}


BOOL H323Connection::OnSendSignalSetup(H323SignalPDU & pdu)
{
  return endpoint.OnSendSignalSetup(*this, pdu);
}


BOOL H323Connection::OnSendCallProceeding(H323SignalPDU & callProceedingPDU)
{
  return endpoint.OnSendCallProceeding(*this, callProceedingPDU);
}


BOOL H323Connection::OnSendReleaseComplete(H323SignalPDU & /*releaseCompletePDU*/)
{
  return TRUE;
}


BOOL H323Connection::OnAlerting(const H323SignalPDU & alertingPDU,
                                const PString & username)
{
  return endpoint.OnAlerting(*this, alertingPDU, username);
}


BOOL H323Connection::SetAlerting(const PString & calleeName, BOOL withMedia)
{
  PTRACE(3, "H323\tSetAlerting " << *this);
  if (alertingPDU == NULL)
    return FALSE;

  if (withMedia && !mediaWaitForConnect) {
    H225_Alerting_UUIE & alerting = alertingPDU->m_h323_uu_pdu.m_h323_message_body;
    if (SendFastStartAcknowledge(alerting.m_fastStart))
      alerting.IncludeOptionalField(H225_Alerting_UUIE::e_fastStart);
    else {
      // See if aborted call
      if (connectionState == ShuttingDownConnection)
        return FALSE;

      // Do early H.245 start
      earlyStart = TRUE;
      if (!h245Tunneling && (controlChannel == NULL) && !endpoint.IsH245Disabled()) {
        if (!CreateIncomingControlChannel(alerting.m_h245Address))
          return FALSE;
        alerting.IncludeOptionalField(H225_Alerting_UUIE::e_h245Address);
      }
    }
  }

  alertingTime = PTime();

  HandleTunnelPDU(alertingPDU);

#if OPAL_H450
  h450dispatcher->AttachToAlerting(*alertingPDU);
#endif

  if (!endpoint.OnSendAlerting(*this, *alertingPDU, calleeName, withMedia)){
    /* let the application to avoid sending the alerting, mainly for testing other endpoints*/
    PTRACE(3, "H323CON\tSetAlerting Alerting not sent");
    return TRUE;
  }
  
  // send Q931 Alerting PDU
  PTRACE(3, "H323CON\tSetAlerting sending Alerting PDU");
  
  BOOL bOk = WriteSignalPDU(*alertingPDU);
  if (!endpoint.OnSentAlerting(*this)){
    /* let the application to know that the alerting has been sent */
    /* do nothing for now, at least check for the return value */
  }
  return bOk;
}


BOOL H323Connection::SetConnected()
{
  mediaWaitForConnect = FALSE;

  PTRACE(3, "H323CON\tSetConnected " << *this);
  if (connectPDU == NULL){
    PTRACE(1, "H323CON\tSetConnected connectPDU is null" << *this);
    return FALSE;
  }  

  if (!endpoint.OnSendConnect(*this, *connectPDU)){
    /* let the application to avoid sending the connect, mainly for testing other endpoints*/
    PTRACE(2, "H323CON\tSetConnected connect not sent");
    return TRUE;
  }  
  // Assure capabilities are set to other connections media list (if not already)
  OnSetLocalCapabilities();

  H225_Connect_UUIE & connect = connectPDU->m_h323_uu_pdu.m_h323_message_body;
  // Now ask the application to select which channels to start
  if (SendFastStartAcknowledge(connect.m_fastStart))
    connect.IncludeOptionalField(H225_Connect_UUIE::e_fastStart);

  // See if aborted call
  if (connectionState == ShuttingDownConnection)
    return FALSE;

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
      if (fastStartState == FastStartDisabled) {
        h245TunnelTxPDU = connectPDU; // Piggy back H245 on this reply
        BOOL ok = StartControlNegotiations();
        h245TunnelTxPDU = NULL;
        if (!ok)
          return FALSE;
      }
    }
    else if (!controlChannel) { // Start separate H.245 channel if not tunneling.
      if (!CreateIncomingControlChannel(connect.m_h245Address))
        return FALSE;
      connect.IncludeOptionalField(H225_Connect_UUIE::e_h245Address);
    }
  }

  if (!WriteSignalPDU(*connectPDU)) // Send H323 Connect PDU
    return FALSE;

  delete connectPDU;
  connectPDU = NULL;
  delete alertingPDU;
  alertingPDU = NULL;

  connectedTime = PTime();

  InternalEstablishedConnectionCheck();
  return TRUE;
}

BOOL H323Connection::SetProgressed()
{
  mediaWaitForConnect = FALSE;

  PTRACE(3, "H323\tSetProgressed " << *this);
  if (progressPDU == NULL){
    PTRACE(1, "H323\tSetProgressed progressPDU is null" << *this);
    return FALSE;
  }  

  // Assure capabilities are set to other connections media list (if not already)
  OnSetLocalCapabilities();

  H225_Progress_UUIE & progress = progressPDU->m_h323_uu_pdu.m_h323_message_body;

  // Now ask the application to select which channels to start
  if (SendFastStartAcknowledge(progress.m_fastStart))
    progress.IncludeOptionalField(H225_Connect_UUIE::e_fastStart);

  // See if aborted call
  if (connectionState == ShuttingDownConnection)
    return FALSE;

  // Set flag that we are up to CONNECT stage
  //connectionState = HasExecutedSignalConnect;
  //phase = ConnectedPhase;
  /* TODO*/
  //h450dispatcher->AttachToProgress(*progress);
  if(endpoint.IsH245Disabled() == FALSE){
    if (h245Tunneling) {
      HandleTunnelPDU(progressPDU);
  
      // If no channels selected (or never provided) do traditional H245 start
      if (fastStartState == FastStartDisabled) {
        h245TunnelTxPDU = progressPDU; // Piggy back H245 on this reply
        BOOL ok = StartControlNegotiations();
        h245TunnelTxPDU = NULL;
        if (!ok)
          return FALSE;
      }
    }
    else if (!controlChannel) { // Start separate H.245 channel if not tunneling.
      if (!CreateIncomingControlChannel(progress.m_h245Address))
        return FALSE;
      progress.IncludeOptionalField(H225_Connect_UUIE::e_h245Address);
    }
  }
  
  if (!WriteSignalPDU(*progressPDU)) // Send H323 Connect PDU
    return FALSE;

  delete progressPDU;
  progressPDU = NULL;

  delete alertingPDU;
  alertingPDU = NULL;

  connectedTime = PTime();

  InternalEstablishedConnectionCheck();
  return TRUE;
}


BOOL H323Connection::OnInsufficientDigits()
{
  return FALSE;
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


BOOL H323Connection::OnOutgoingCall(const H323SignalPDU & connectPDU)
{
  return endpoint.OnOutgoingCall(*this, connectPDU);
}


BOOL H323Connection::SendFastStartAcknowledge(H225_ArrayOf_PASN_OctetString & array)
{
  PINDEX i;

  // See if we have already added the fast start OLC's
  if (array.GetSize() > 0)
    return TRUE;

  // See if we need to select our fast start channels
  if (fastStartState == FastStartResponse)
    OnSelectLogicalChannels();

  // Remove any channels that were not started by OnSelectLogicalChannels(),
  // those that were started are put into the logical channel dictionary
  {
    PWaitAndSignal m(this->GetMediaStreamMutex());
    for (i = 0; i < fastStartChannels.GetSize(); i++) {
      if (fastStartChannels[i].IsOpen())
        logicalChannels->Add(fastStartChannels[i]);
      else
        fastStartChannels.RemoveAt(i--);
    }

    // None left, so didn't open any channels fast
    if (fastStartChannels.IsEmpty()) {
      fastStartState = FastStartDisabled;
      return FALSE;
    }

    // The channels we just transferred to the logical channels dictionary
    // should not be deleted via this structure now.
    fastStartChannels.DisallowDeleteObjects();
  }

  PTRACE(3, "H225\tAccepting fastStart for " << fastStartChannels.GetSize() << " channels");

  for (i = 0; i < fastStartChannels.GetSize(); i++)
    BuildFastStartList(fastStartChannels[i], array, H323Channel::IsTransmitter);

  // Have moved open channels to logicalChannels structure, remove all others.
  fastStartChannels.RemoveAll();

  // Set flag so internal establishment check does not require H.245
  fastStartState = FastStartAcknowledged;

  return TRUE;
}


BOOL H323Connection::HandleFastStartAcknowledge(const H225_ArrayOf_PASN_OctetString & array)
{
  if (fastStartChannels.IsEmpty()) {
    PTRACE(2, "H225\tFast start response with no channels to open");
    return FALSE;
  }

  PTRACE(3, "H225\tFast start accepted by remote endpoint");

  PINDEX i;

  // Go through provided list of structures, if can decode it and match it up
  // with a channel we requested AND it has all the information needed in the
  // m_multiplexParameters, then we can start the channel.
  for (i = 0; i < array.GetSize(); i++) {
    H245_OpenLogicalChannel open;
    if (array[i].DecodeSubType(open)) {
      PTRACE(4, "H225\tFast start open:\n  " << setprecision(2) << open);
      BOOL reverse = open.HasOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters);
      const H245_DataType & dataType = reverse ? open.m_reverseLogicalChannelParameters.m_dataType
                                               : open.m_forwardLogicalChannelParameters.m_dataType;
      H323Capability * replyCapability = localCapabilities.FindCapability(dataType);
      if (replyCapability != NULL) {
        for (PINDEX ch = 0; ch < fastStartChannels.GetSize(); ch++) {
          H323Channel & channelToStart = fastStartChannels[ch];
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
                  {
                    H323_RealTimeChannel * rtp = dynamic_cast<H323_RealTimeChannel *>(&channelToStart);
                    if (rtp != NULL) {
                      RTP_DataFrame::PayloadTypes inpt  = rtp->GetMediaStream()->GetMediaFormat().GetPayloadType();
                      RTP_DataFrame::PayloadTypes outpt = rtp->GetDynamicRTPPayloadType();
                      if ((inpt != outpt) && (outpt != RTP_DataFrame::IllegalPayloadType))
                        rtpPayloadMap.insert(RTP_DataFrame::PayloadMapType::value_type(inpt,outpt));
                    }
                  }
                  if (channelToStart.Open()) {
                    BOOL started = FALSE;
                    if (channelToStart.GetDirection() == H323Channel::IsTransmitter) {
                      transmitterMediaStream = ((H323UnidirectionalChannel &)channelToStart).GetMediaStream();
                      if (GetCall().OpenSourceMediaStreams(*this, transmitterMediaStream->GetMediaFormat(), channelToStart.GetSessionID())) {
                        if (!mediaWaitForConnect)
                          started = channelToStart.Start();
                      }
                      else {
                        transmitterMediaStream = NULL;
                        channelToStart.Close();
                      }
                    }
                    else
                      started = channelToStart.Start();
                    if (started && channelToStart.IsOpen())
                      break;
                  }
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

  // Remove any channels that were not started by above, those that were
  // started are put into the logical channel dictionary
  for (i = 0; i < fastStartChannels.GetSize(); i++) {
    if (fastStartChannels[i].IsOpen())
      logicalChannels->Add(fastStartChannels[i]);
    else
      fastStartChannels.RemoveAt(i--);
  }

  // The channels we just transferred to the logical channels dictionary
  // should not be deleted via this structure now.
  fastStartChannels.DisallowDeleteObjects();

  PTRACE(3, "H225\tFast starting " << fastStartChannels.GetSize() << " channels");
  if (fastStartChannels.IsEmpty())
    return FALSE;

  // Have moved open channels to logicalChannels structure, remove them now.
  fastStartChannels.RemoveAll();

  fastStartState = FastStartAcknowledged;

  return TRUE;
}


BOOL H323Connection::OnUnknownSignalPDU(const H323SignalPDU & PTRACE_PARAM(pdu))
{
  PTRACE(2, "H225\tUnknown signalling PDU: " << pdu);
  return TRUE;
}


BOOL H323Connection::CreateOutgoingControlChannel(const H225_TransportAddress & h245Address)
{
  PTRACE(3, "H225\tCreateOutgoingControlChannel h245Address = " << h245Address);
  if (endpoint.IsH245Disabled()){
    PTRACE(2, "H225\tCreateOutgoingControlChannel h245 is disabled, do nothing");
    /* return TRUE to act as if it was succeded*/
    return TRUE;
  }
  // Already have the H245 channel up.
  if (controlChannel != NULL)
    return TRUE;

  // Check that it is an IP address, all we support at the moment
  controlChannel = signallingChannel->GetLocalAddress().CreateTransport(
                                  endpoint, OpalTransportAddress::HostOnly);
  if (controlChannel == NULL) {
    PTRACE(1, "H225\tConnect of H245 failed: Unsupported transport");
    return FALSE;
  }

  if (!controlChannel->SetRemoteAddress(H323TransportAddress(h245Address))) {
    PTRACE(1, "H225\tCould not extract H245 address");
    delete controlChannel;
    controlChannel = NULL;
    return FALSE;
  }

  if (!controlChannel->Connect()) {
    PTRACE(1, "H225\tConnect of H245 failed: " << controlChannel->GetErrorText());
    delete controlChannel;
    controlChannel = NULL;
    return FALSE;
  }

  controlChannel->AttachThread(PThread::Create(PCREATE_NOTIFIER(NewOutgoingControlChannel), 0,
                                               PThread::NoAutoDeleteThread,
                                               PThread::NormalPriority,
                                               "H.245 Handler"));
  return TRUE;
}


void H323Connection::NewOutgoingControlChannel(PThread &, INT)
{
  if (PAssertNULL(controlChannel) == NULL)
    return;

  if (!SafeReference())
    return;

  HandleControlChannel();
  SafeDereference();
}


BOOL H323Connection::CreateIncomingControlChannel(H225_TransportAddress & h245Address)
{
  PAssert(controlChannel == NULL, PLogicError);

  if (endpoint.IsH245Disabled()){
    PTRACE(2, "H225\tCreateIncomingControlChannel: do not create channel because h245 is disabled");
    return FALSE;
  }
  
  H323TransportAddress localSignallingInterface = signallingChannel->GetLocalAddress();
  if (controlListener == NULL) {
    controlListener = localSignallingInterface.CreateListener(
                            endpoint, OpalTransportAddress::HostOnly);
    if (controlListener == NULL)
      return FALSE;

    if (!controlListener->Open(PCREATE_NOTIFIER(NewIncomingControlChannel), TRUE)) {
      delete controlListener;
      controlListener = NULL;
      return FALSE;
    }
  }

  H323TransportAddress listeningAddress = controlListener->GetLocalAddress(localSignallingInterface);

  // map H.245 listening address using NAT code
  PIPSocket::Address localAddress; WORD listenPort;
  listeningAddress.GetIpAndPort(localAddress, listenPort);

  PIPSocket::Address remoteAddress;
  signallingChannel->GetRemoteAddress().GetIpAddress(remoteAddress);

  endpoint.TranslateTCPAddress(localAddress, remoteAddress);
  listeningAddress = H323TransportAddress(localAddress, listenPort);

  // assign address into the PDU
  return listeningAddress.SetPDU(h245Address);
}


void H323Connection::NewIncomingControlChannel(PThread & listener, INT param)
{
  ((OpalListener&)listener).Close();

  if (param == 0) {
    // If H.245 channel failed to connect and have no media (no fast start)
    // then clear the call as it is useless.
    BOOL release;
    {
      PWaitAndSignal mutex(mediaStreamMutex);
      release = mediaStreams.IsEmpty();
    }
    if (release)
      Release(EndedByTransportFail);
    return;
  }

  if (!SafeReference())
    return;

  controlChannel = (H323Transport *)param;
  HandleControlChannel();
  SafeDereference();
}


BOOL H323Connection::WriteControlPDU(const H323ControlPDU & pdu)
{
  PPER_Stream strm;
  pdu.Encode(strm);
  strm.CompleteEncoding();

  H323TraceDumpPDU("H245", TRUE, strm, pdu, pdu, 0);

  if (!h245Tunneling) {
    if (controlChannel == NULL) {
      PTRACE(1, "H245\tWrite PDU fail: no control channel.");
      return FALSE;
    }

    if (controlChannel->IsOpen() && controlChannel->WritePDU(strm))
      return TRUE;

    PTRACE(1, "H245\tWrite PDU fail: " << controlChannel->GetErrorText(PChannel::LastWriteError));
    return FALSE;
  }

  // If have a pending signalling PDU, use it rather than separate write
  H323SignalPDU localTunnelPDU;
  H323SignalPDU * tunnelPDU;
  if (h245TunnelTxPDU != NULL)
    tunnelPDU = h245TunnelTxPDU;
  else {
    localTunnelPDU.BuildFacility(*this, TRUE);
    tunnelPDU = &localTunnelPDU;
  }

  tunnelPDU->m_h323_uu_pdu.IncludeOptionalField(H225_H323_UU_PDU::e_h245Control);
  PINDEX last = tunnelPDU->m_h323_uu_pdu.m_h245Control.GetSize();
  tunnelPDU->m_h323_uu_pdu.m_h245Control.SetSize(last+1);
  tunnelPDU->m_h323_uu_pdu.m_h245Control[last] = strm;

  if (h245TunnelTxPDU != NULL)
    return TRUE;

  return WriteSignalPDU(localTunnelPDU);
}


BOOL H323Connection::StartControlNegotiations()
{
  PTRACE(3, "H245\tStarted control channel");

 
  if (endpoint.IsH245Disabled()){
    PTRACE(2, "H245\tStartControlNegotiations h245 is disabled, do not start negotiation");
    return FALSE;
  }
  // Get the local capabilities before fast start is handled
  OnSetLocalCapabilities();

  // Begin the capability exchange procedure
  if (!capabilityExchangeProcedure->Start(FALSE)) {
    PTRACE(1, "H245\tStart of Capability Exchange failed");
    return FALSE;
  }

  // Begin the Master/Slave determination procedure
  if (!masterSlaveDeterminationProcedure->Start(FALSE)) {
    PTRACE(1, "H245\tStart of Master/Slave determination failed");
    return FALSE;
  }

  endSessionNeeded = TRUE;
  return TRUE;
}


void H323Connection::HandleControlChannel()
{
  // If have started separate H.245 channel then don't tunnel any more
  h245Tunneling = FALSE;

  if (LockReadWrite()) {
    // Start the TCS and MSD operations on new H.245 channel.
    if (!StartControlNegotiations()) {
      UnlockReadWrite();
      return;
    }
    UnlockReadWrite();
  }

  
  // Disable the signalling channels timeout for monitoring call status and
  // start up one in this thread instead. Then the Q.931 channel can be closed
  // without affecting the call.
  signallingChannel->SetReadTimeout(PMaxTimeInterval);
  controlChannel->SetReadTimeout(MonitorCallStatusTime);

  BOOL ok = TRUE;
  while (ok) {
    MonitorCallStatus();

    PPER_Stream strm;
    if (controlChannel->ReadPDU(strm)) {
      // Lock while checking for shutting down.
      ok = LockReadWrite();
      if (ok) {
        // Process the received PDU
        PTRACE(4, "H245\tReceived TPKT: " << strm);
        if (GetPhase() < ReleasingPhase)
          ok = HandleControlData(strm);
        else
          ok = InternalEndSessionCheck(strm);
        UnlockReadWrite(); // Unlock connection
      }
    }
    else if (controlChannel->GetErrorCode() != PChannel::Timeout) {
      PTRACE(1, "H245\tRead error: " << controlChannel->GetErrorText(PChannel::LastReadError));
      Release(EndedByTransportFail);
      ok = FALSE;
    }
  }

  // If we are the only link to the far end then indicate that we have
  // received endSession even if we hadn't, because we are now never going
  // to get one so there is no point in having CleanUpOnCallEnd wait.
  if (signallingChannel == NULL)
    endSessionReceived.Signal();

  PTRACE(3, "H245\tControl channel closed.");
}


BOOL H323Connection::InternalEndSessionCheck(PPER_Stream & strm)
{
  H323ControlPDU pdu;

  if (!pdu.Decode(strm)) {
    PTRACE(1, "H245\tInvalid PDU decode:\n  " << setprecision(2) << pdu);
    return FALSE;
  }

  PTRACE(3, "H245\tChecking for end session on PDU: " << pdu.GetTagName()
         << ' ' << ((PASN_Choice &)pdu.GetObject()).GetTagName());

  if (pdu.GetTag() != H245_MultimediaSystemControlMessage::e_command)
    return TRUE;

  H245_CommandMessage & command = pdu;
  if (command.GetTag() == H245_CommandMessage::e_endSessionCommand)
    endSessionReceived.Signal();
  return FALSE;
}


BOOL H323Connection::HandleControlData(PPER_Stream & strm)
{
  while (!strm.IsAtEnd()) {
    H323ControlPDU pdu;
    if (!pdu.Decode(strm)) {
      PTRACE(1, "H245\tInvalid PDU decode!"
                "\nRaw PDU:\n" << hex << setfill('0')
                               << setprecision(2) << strm
                               << dec << setfill(' ') <<
                "\nPartial PDU:\n  " << setprecision(2) << pdu);
      return TRUE;
    }

    H323TraceDumpPDU("H245", FALSE, strm, pdu, pdu, 0);

    if (!HandleControlPDU(pdu))
      return FALSE;

    InternalEstablishedConnectionCheck();

    strm.ByteAlign();
  }

  return TRUE;
}


BOOL H323Connection::HandleControlPDU(const H323ControlPDU & pdu)
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


BOOL H323Connection::OnUnknownControlPDU(const H323ControlPDU & pdu)
{
  PTRACE(2, "H245\tUnknown Control PDU: " << pdu);

  H323ControlPDU reply;
  reply.BuildFunctionNotUnderstood(pdu);
  return WriteControlPDU(reply);
}


BOOL H323Connection::OnH245Request(const H323ControlPDU & pdu)
{
  const H245_RequestMessage & request = pdu;

  switch (request.GetTag()) {
    case H245_RequestMessage::e_masterSlaveDetermination :
      return masterSlaveDeterminationProcedure->HandleIncoming(request);

    case H245_RequestMessage::e_terminalCapabilitySet :
    {
      const H245_TerminalCapabilitySet & tcs = request;
      if (tcs.m_protocolIdentifier.GetSize() >= 6) {
        h245version = tcs.m_protocolIdentifier[5];
        h245versionSet = TRUE;
        PTRACE(3, "H245\tSet protocol version to " << h245version);
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
  }

  return OnUnknownControlPDU(pdu);
}


BOOL H323Connection::OnH245Response(const H323ControlPDU & pdu)
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
  }

  return OnUnknownControlPDU(pdu);
}


BOOL H323Connection::OnH245Command(const H323ControlPDU & pdu)
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
      endSessionNeeded = TRUE;
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
      return FALSE;
  }

  return OnUnknownControlPDU(pdu);
}


BOOL H323Connection::OnH245Indication(const H323ControlPDU & pdu)
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
  }

  return TRUE; // Do NOT call OnUnknownControlPDU for indications
}


BOOL H323Connection::OnH245_SendTerminalCapabilitySet(
                 const H245_SendTerminalCapabilitySet & pdu)
{
  if (pdu.GetTag() == H245_SendTerminalCapabilitySet::e_genericRequest)
    return capabilityExchangeProcedure->Start(TRUE);

  PTRACE(2, "H245\tUnhandled SendTerminalCapabilitySet: " << pdu);
  return TRUE;
}


BOOL H323Connection::OnH245_FlowControlCommand(
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
      H323Channel * chan = logicalChannels->FindChannel((unsigned)(const H245_LogicalChannelNumber &)pdu.m_scope, FALSE);
      if (chan != NULL)
        OnLogicalChannelFlowControl(chan, restriction);
    }
  }

  return TRUE;
}


BOOL H323Connection::OnH245_MiscellaneousCommand(
                 const H245_MiscellaneousCommand & pdu)
{
  H323Channel * chan = logicalChannels->FindChannel((unsigned)pdu.m_logicalChannelNumber, FALSE);
  if (chan != NULL)
    chan->OnMiscellaneousCommand(pdu.m_type);
  else
    PTRACE(2, "H245\tMiscellaneousCommand: is ignored chan=" << pdu.m_logicalChannelNumber
           << ", type=" << pdu.m_type.GetTagName());

  return TRUE;
}


BOOL H323Connection::OnH245_MiscellaneousIndication(
                 const H245_MiscellaneousIndication & pdu)
{
  H323Channel * chan = logicalChannels->FindChannel((unsigned)pdu.m_logicalChannelNumber, TRUE);
  if (chan != NULL)
    chan->OnMiscellaneousIndication(pdu.m_type);
  else
    PTRACE(2, "H245\tMiscellaneousIndication is ignored. chan=" << pdu.m_logicalChannelNumber
           << ", type=" << pdu.m_type.GetTagName());

  return TRUE;
}


BOOL H323Connection::OnH245_JitterIndication(
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
      H323Channel * chan = logicalChannels->FindChannel((unsigned)(const H245_LogicalChannelNumber &)pdu.m_scope, FALSE);
      if (chan != NULL)
        OnLogicalChannelJitter(chan, jitter, skippedFrameCount, additionalBuffer);
    }
  }

  return TRUE;
}


H323Channel * H323Connection::GetLogicalChannel(unsigned number, BOOL fromRemote) const
{
  return logicalChannels->FindChannel(number, fromRemote);
}


H323Channel * H323Connection::FindChannel(unsigned rtpSessionId, BOOL fromRemote) const
{
  return logicalChannels->FindChannelBySession(rtpSessionId, fromRemote);
}

#if OPAL_H450

void H323Connection::TransferConnection(const PString & remoteParty,
					const PString & callIdentity)
{
  TransferCall(remoteParty, callIdentity);
}


void H323Connection::TransferCall(const PString & remoteParty,
                                  const PString & callIdentity)
{
  // According to H.450.4, if prior to consultation the primary call has been put on hold, the 
  // transferring endpoint shall first retrieve the call before Call Transfer is invoked.
  if (!callIdentity.IsEmpty() && IsLocalHold())
    RetrieveCall();
  h4502handler->TransferCall(remoteParty, callIdentity);
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

BOOL H323Connection::IsTransferringCall() const
{
  switch (h4502handler->GetState()) {
    case H4502Handler::e_ctAwaitIdentifyResponse :
    case H4502Handler::e_ctAwaitInitiateResponse :
    case H4502Handler::e_ctAwaitSetupResponse :
      return TRUE;

    default :
      return FALSE;
  }
}


BOOL H323Connection::IsTransferredCall() const
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

void H323Connection::HoldConnection()
{
  HoldCall(TRUE);
  
  // Signal the manager that there is a hold
  endpoint.OnHold(*this);
}


void H323Connection::RetrieveConnection()
{
  RetrieveCall();
  
  // Signal the manager that there is a retrieve 
  endpoint.OnHold(*this);
}


BOOL H323Connection::IsConnectionOnHold() 
{
  return IsCallOnHold ();
}

void H323Connection::HoldCall(BOOL localHold)
{
  h4504handler->HoldCall(localHold);
  holdMediaChannel = SwapHoldMediaChannels(holdMediaChannel);
}


void H323Connection::RetrieveCall()
{
  // Is the current call on hold?
  if (IsLocalHold()) {
    h4504handler->RetrieveCall();
    holdMediaChannel = SwapHoldMediaChannels(holdMediaChannel);
  }
  else if (IsRemoteHold()) {
    PTRACE(4, "H4504\tRemote-end Call Hold not implemented.");
  }
  else {
    PTRACE(4, "H4504\tCall is not on Hold.");
  }
}


void H323Connection::SetHoldMedia(PChannel * audioChannel)
{
  holdMediaChannel = PAssertNULL(audioChannel);
}


BOOL H323Connection::IsMediaOnHold() const
{
  return holdMediaChannel != NULL;
}


PChannel * H323Connection::SwapHoldMediaChannels(PChannel * newChannel)
{
  if (IsMediaOnHold()) {
    if (PAssertNULL(newChannel) == NULL)
      return NULL;
  }

  PChannel * existingTransmitChannel = NULL;

  PINDEX count = logicalChannels->GetSize();

  for (PINDEX i = 0; i < count; ++i) {
    H323Channel* channel = logicalChannels->GetChannelAt(i);
    if (!channel)
      return NULL;

    unsigned int session_id = channel->GetSessionID();
    if (session_id == OpalMediaFormat::DefaultAudioSessionID || session_id == OpalMediaFormat::DefaultVideoSessionID) {
      const H323ChannelNumber & channelNumber = channel->GetNumber();

      H323_RTPChannel * chan2 = reinterpret_cast<H323_RTPChannel*>(channel);
      OpalMediaStream *stream = GetMediaStream (session_id, FALSE);

      if (!channelNumber.IsFromRemote()) { // Transmit channel
        if (IsMediaOnHold()) {
//          H323Codec & codec = *channel->GetCodec();
//          existingTransmitChannel = codec.GetRawDataChannel();
	  //FIXME
        }
        else {
          // Enable/mute the transmit channel depending on whether the remote end is held
          chan2->SetPause(IsLocalHold());
	        stream->SetPaused(IsLocalHold());
        }
      }
      else {
        // Enable/mute the receive channel depending on whether the remote endis held
        chan2->SetPause(IsLocalHold());
	stream->SetPaused(IsLocalHold());
      }
    }
  }

  return existingTransmitChannel;
}


BOOL H323Connection::IsLocalHold() const
{
  return h4504handler->GetState() == H4504Handler::e_ch_NE_Held;
}


BOOL H323Connection::IsRemoteHold() const
{
  return h4504handler->GetState() == H4504Handler::e_ch_RE_Held;
}


BOOL H323Connection::IsCallOnHold() const
{
  return h4504handler->GetState() != H4504Handler::e_ch_Idle;
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


BOOL H323Connection::GetRemoteCallIntrusionProtectionLevel(const PString & intrusionCallToken,
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

BOOL H323Connection::OnControlProtocolError(ControlProtocolErrors /*errorSource*/,
                                            const void * /*errorData*/)
{
  return TRUE;
}

static void SetRFC2833PayloadType(H323Capabilities & capabilities,
                                  OpalRFC2833Proto & rfc2833handler)
{
  H323Capability * capability = capabilities.FindCapability(H323_UserInputCapability::GetSubTypeName(H323_UserInputCapability::SignalToneRFC2833));
  if (capability != NULL) {
    RTP_DataFrame::PayloadTypes pt = ((H323_UserInputCapability*)capability)->GetPayloadType();
    if (rfc2833handler.GetPayloadType() != pt) {
      PTRACE(3, "H323\tUser Input RFC2833 payload type set to " << pt);
      rfc2833handler.SetPayloadType(pt);
    }
  }
}


void H323Connection::OnSendCapabilitySet(H245_TerminalCapabilitySet & /*pdu*/)
{
  // If we originated call, then check for RFC2833 capability and set payload type
  if (!HadAnsweredCall())
    SetRFC2833PayloadType(localCapabilities, *rfc2833Handler);
}


BOOL H323Connection::OnReceivedCapabilitySet(const H323Capabilities & remoteCaps,
                                             const H245_MultiplexCapability * muxCap,
                                             H245_TerminalCapabilitySetReject & /*rejectPDU*/)
{
  if (muxCap != NULL) {
    if (muxCap->GetTag() != H245_MultiplexCapability::e_h2250Capability) {
      PTRACE(1, "H323\tCapabilitySet contains unsupported multiplex.");
      return FALSE;
    }

    const H245_H2250Capability & h225_0 = *muxCap;
    remoteMaxAudioDelayJitter = h225_0.m_maximumAudioDelayJitter;
  }

  if (remoteCaps.GetSize() == 0) {
    // Received empty TCS, so close all transmit channels
    for (PINDEX i = 0; i < logicalChannels->GetSize(); i++) {
      H245NegLogicalChannel & negChannel = logicalChannels->GetNegLogicalChannelAt(i);
      H323Channel * channel = negChannel.GetChannel();
      if (channel != NULL && !channel->GetNumber().IsFromRemote())
        negChannel.Close();
    }
    ownerCall.RemoveMediaStreams();
    transmitterSidePaused = TRUE;
  }
  else {
    /* Received non-empty TCS, if was in paused state or this is the first TCS
       received so we should kill the fake table created by fast start kill
       the remote capabilities table so Merge() becomes a simple assignment */
    if (transmitterSidePaused || !capabilityExchangeProcedure->HasReceivedCapabilities())
      remoteCapabilities.RemoveAll();

    if (!remoteCapabilities.Merge(remoteCaps))
      return FALSE;

    remoteCapabilities.Remove(GetCall().GetManager().GetMediaFormatMask());

    if (transmitterSidePaused) {
      transmitterSidePaused = FALSE;
      connectionState = HasExecutedSignalConnect;
      SetPhase(ConnectedPhase);
      capabilityExchangeProcedure->Start(TRUE);
    }
    else {
      if (localCapabilities.GetSize() > 0)
        capabilityExchangeProcedure->Start(FALSE);

      // If we terminated call, then check for RFC2833 capability and set payload type
      if (HadAnsweredCall())
        SetRFC2833PayloadType(remoteCapabilities, *rfc2833Handler);
    }
  }

  return TRUE;
}


void H323Connection::SendCapabilitySet(BOOL empty)
{
  capabilityExchangeProcedure->Start(TRUE, empty);
}

OpalMediaFormatList H323Connection::GetLocalMediaFormats()
{
  return ownerCall.GetMediaFormats(*this, FALSE);
}

void H323Connection::OnSetLocalCapabilities()
{
  if (capabilityExchangeProcedure->HasSentCapabilities())
    return;

  // create the list of media formats supported locally
  OpalMediaFormatList formats;
  if (originating)
    formats = GetLocalMediaFormats();
  else
    formats = ownerCall.GetMediaFormats(*this, FALSE);

  if (formats.IsEmpty()) {
    PTRACE(2, "H323\tSetLocalCapabilities - no existing formats in call");
    return;
  }

  // Remove those things not in the other parties media format list
  for (PINDEX c = 0; c < localCapabilities.GetSize(); c++) {
    H323Capability & capability = localCapabilities[c];
    if (formats.FindFormat(capability.GetMediaFormat()) == P_MAX_INDEX) {
      localCapabilities.Remove(&capability);
      c--;
    }
  }

  // Add those things that are in the other parties media format list
  static unsigned sessionOrder[] = {
    OpalMediaFormat::DefaultAudioSessionID,
    OpalMediaFormat::DefaultVideoSessionID,
    OpalMediaFormat::DefaultDataSessionID,
    0
  };
  PINDEX simultaneous;

  for (PINDEX s = 0; s < PARRAYSIZE(sessionOrder); s++) {
    simultaneous = P_MAX_INDEX;
    for (PINDEX i = 0; i < formats.GetSize(); i++) {
      OpalMediaFormat format = formats[i];
      if (format.GetDefaultSessionID() == sessionOrder[s] &&
          format.GetPayloadType() < RTP_DataFrame::MaxPayloadType)
        simultaneous = localCapabilities.AddAllCapabilities(endpoint, 0, simultaneous, format);
    }
  }
  
#ifdef OPAL_H224
  // If H.224 is enabled, add the corresponding capabilities
  if(GetEndPoint().IsH224Enabled()) {
    localCapabilities.SetCapability(0, P_MAX_INDEX, new H323_H224Capability());
  }
#endif

  H323_UserInputCapability::AddAllCapabilities(localCapabilities, 0, P_MAX_INDEX);

  // Special test for the RFC2833 capability to get the correct dynamic payload type
  H323Capability * capability = localCapabilities.FindCapability(OpalRFC2833);
  if (capability != NULL) {
    MediaInformation info;
    PSafePtr<OpalConnection> otherParty = GetCall().GetOtherPartyConnection(*this);
    if (otherParty != NULL &&
        otherParty->GetMediaInformation(OpalMediaFormat::DefaultAudioSessionID, info))
      capability->SetPayloadType(info.rfc2833);
    else
      localCapabilities.Remove(capability);
  }

  localCapabilities.Remove(GetCall().GetManager().GetMediaFormatMask());

  PTRACE(3, "H323\tSetLocalCapabilities:\n" << setprecision(2) << localCapabilities);
}


BOOL H323Connection::IsH245Master() const
{
  return masterSlaveDeterminationProcedure->IsMaster();
}


void H323Connection::StartRoundTripDelay()
{
  if (LockReadWrite()) {
    if (GetPhase() < ReleasingPhase &&
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
  PTRACE(3, "H323\tInternalEstablishedConnectionCheck: "
            "connectionState=" << connectionState << " "
            "fastStartState=" << fastStartState);

  BOOL h245_available = masterSlaveDeterminationProcedure->IsDetermined() &&
                        capabilityExchangeProcedure->HasSentCapabilities() &&
                        capabilityExchangeProcedure->HasReceivedCapabilities();

  if (h245_available)
    endSessionNeeded = TRUE;

  // Check for if all the 245 conditions are met so can start up logical
  // channels and complete the connection establishment.
  if (fastStartState != FastStartAcknowledged) {
    if (!h245_available)
      return;

    // If we are early starting, start channels as soon as possible instead of
    // waiting for connect PDU
    if (earlyStart && IsH245Master() && FindChannel(OpalMediaFormat::DefaultAudioSessionID, FALSE) == NULL)
      OnSelectLogicalChannels();
  }

#if OPAL_T120
  if (h245_available && startT120) {
    if (remoteCapabilities.FindCapability("T.120") != NULL) {
      H323Capability * capability = localCapabilities.FindCapability("T.120");
      if (capability != NULL)
        OpenLogicalChannel(*capability, 3, H323Channel::IsBidirectional);
    }
    startT120 = FALSE;
  }
#endif
  
  switch (phase) {
    case ConnectedPhase :
      // Check if we have already got a transmitter running, select one if not
      if (FindChannel(OpalMediaFormat::DefaultAudioSessionID, FALSE) == NULL)
        OnSelectLogicalChannels();

      connectionState = EstablishedConnection;
      SetPhase(EstablishedPhase);

      OnEstablished();
      break;

    case EstablishedPhase :
      connectionState = EstablishedConnection; // Keep in sync
      break;

    default :
      break;
  }

#if OPAL_H224
  if (h245_available && startH224) {
    if(remoteCapabilities.FindCapability(OPAL_H224_CAPABILITY_NAME) != NULL) {
      H323Capability * capability = localCapabilities.FindCapability(OPAL_H224_CAPABILITY_NAME);
      if (capability != NULL) {
	      if (logicalChannels->Open(*capability, OpalMediaFormat::DefaultH224SessionID)) {
		      H323Channel * channel = capability->CreateChannel(*this, H323Channel::IsTransmitter, OpalMediaFormat::DefaultH224SessionID, NULL);
          if  (channel != NULL) {
			      channel->SetNumber(logicalChannels->GetNextChannelNumber());
			      fastStartChannels.Append(channel);
          }
        }
      }
    }
	  startH224 = FALSE;
  }
#endif
}


OpalMediaFormatList H323Connection::GetMediaFormats() const
{
  OpalMediaFormatList list;
  
  list = remoteCapabilities.GetMediaFormats();
  AdjustMediaFormats(list);

  return list;
}


BOOL H323Connection::OpenSourceMediaStream(const OpalMediaFormatList & /*mediaFormats*/,
                                           unsigned sessionID)
{
#if OPAL_VIDEO
  if (sessionID == OpalMediaFormat::DefaultVideoSessionID && !endpoint.GetManager().CanAutoStartReceiveVideo())
    return FALSE;
#endif

  // Check if we have already got a transmitter running, select one if not
  if ((fastStartState == FastStartDisabled ||
       fastStartState == FastStartAcknowledged) &&
      FindChannel(sessionID, FALSE) != NULL)
    return FALSE;

  PTRACE(3, "H323\tOpenSourceMediaStream called: session " << sessionID);
  return TRUE;
}

OpalMediaStream * H323Connection::CreateMediaStream(const OpalMediaFormat & mediaFormat,
                                                    unsigned sessionID,
                                                    BOOL isSource)
{
  if (ownerCall.IsMediaBypassPossible(*this, sessionID))
    return new OpalNullMediaStream(*this, mediaFormat, sessionID, isSource);

  if (!isSource) {
    OpalMediaStream * stream = transmitterMediaStream;
    transmitterMediaStream = NULL;
    return stream;
  }

  RTP_Session * session = GetSession(sessionID);
  if (session == NULL) {
    PTRACE(1, "H323\tCreateMediaStream could not find session " << sessionID);
    return NULL;
  }

  return new OpalRTPMediaStream(*this, mediaFormat, isSource, *session,
                                GetMinAudioJitterDelay(),
                                GetMaxAudioJitterDelay());
}


void H323Connection::OnPatchMediaStream(BOOL isSource, OpalMediaPatch & patch)
{
  OpalConnection::OnPatchMediaStream(isSource, patch);
  if(patch.GetSource().GetSessionID() == OpalMediaFormat::DefaultAudioSessionID) {
    AttachRFC2833HandlerToPatch(isSource, patch);
    if (detectInBandDTMF && isSource) {
      patch.AddFilter(PCREATE_NOTIFIER(OnUserInputInBandDTMF), OPAL_PCM16);
    }
  }
}


BOOL H323Connection::IsMediaBypassPossible(unsigned sessionID) const
{
  //PTRACE(3, "H323\tIsMediaBypassPossible: session " << sessionID);

  return sessionID == OpalMediaFormat::DefaultAudioSessionID ||
         sessionID == OpalMediaFormat::DefaultVideoSessionID;
}


BOOL H323Connection::GetMediaInformation(unsigned sessionID,
                                         MediaInformation & info) const
{
  if (!OpalConnection::GetMediaInformation(sessionID, info))
    return FALSE;

  H323Capability * capability = remoteCapabilities.FindCapability(OpalRFC2833);
  if (capability != NULL)
    info.rfc2833 = capability->GetPayloadType();

  PTRACE(3, "H323\tGetMediaInformation for session " << sessionID
         << " data=" << info.data << " rfc2833=" << info.rfc2833);
  return TRUE;
}


void H323Connection::StartFastStartChannel(unsigned sessionID, H323Channel::Directions direction)
{
  for (PINDEX i = 0; i < fastStartChannels.GetSize(); i++) {
    H323Channel & channel = fastStartChannels[i];
    if (channel.GetSessionID() == sessionID && channel.GetDirection() == direction) {
      if (channel.Open()) {
        if (direction == H323Channel::IsTransmitter) {
          transmitterMediaStream = ((H323UnidirectionalChannel &)channel).GetMediaStream();
          if (GetCall().OpenSourceMediaStreams(*this, transmitterMediaStream->GetMediaFormat(), channel.GetSessionID())) {
            if (!mediaWaitForConnect)
              channel.Start();
          }
          else {
            transmitterMediaStream = NULL;
            channel.Close();
          }
        }
        else
          channel.Start();
        if (channel.IsOpen())
          break;
      }
    }
  }
}


void H323Connection::OnSelectLogicalChannels()
{
  PTRACE(3, "H245\tDefault OnSelectLogicalChannels, " << fastStartState);

  // Select the first codec that uses the "standard" audio session.
  switch (fastStartState) {
    default : //FastStartDisabled :
#if OPAL_AUDIO
      SelectDefaultLogicalChannel(OpalMediaFormat::DefaultAudioSessionID);
#endif
#if OPAL_VIDEO
      if (endpoint.CanAutoStartTransmitVideo())
        SelectDefaultLogicalChannel(OpalMediaFormat::DefaultVideoSessionID);
#endif
#if OPAL_T38FAX
      if (endpoint.CanAutoStartTransmitFax())
        SelectDefaultLogicalChannel(OpalMediaFormat::DefaultDataSessionID);
#endif
      break;

    case FastStartInitiate :
#if OPAL_AUDIO
      SelectFastStartChannels(OpalMediaFormat::DefaultAudioSessionID, TRUE, TRUE);
#endif
#if OPAL_VIDEO
      SelectFastStartChannels(OpalMediaFormat::DefaultVideoSessionID,
                              endpoint.CanAutoStartTransmitVideo(),
                              endpoint.CanAutoStartReceiveVideo());
#endif
#if OPAL_T38FAX
      SelectFastStartChannels(OpalMediaFormat::DefaultDataSessionID,
                              endpoint.CanAutoStartTransmitFax(),
                              endpoint.CanAutoStartReceiveFax());
#endif
      break;

    case FastStartResponse :
#if OPAL_AUDIO
      StartFastStartChannel(OpalMediaFormat::DefaultAudioSessionID, H323Channel::IsTransmitter);
      StartFastStartChannel(OpalMediaFormat::DefaultAudioSessionID, H323Channel::IsReceiver);
#endif
#if OPAL_VIDEO
      if (endpoint.CanAutoStartTransmitVideo())
        StartFastStartChannel(OpalMediaFormat::DefaultVideoSessionID, H323Channel::IsTransmitter);
      if (endpoint.CanAutoStartReceiveVideo())
        StartFastStartChannel(OpalMediaFormat::DefaultVideoSessionID, H323Channel::IsReceiver);
#endif
#if OPAL_T38FAX
      if (endpoint.CanAutoStartTransmitFax())
        StartFastStartChannel(OpalMediaFormat::DefaultDataSessionID, H323Channel::IsTransmitter);
      if (endpoint.CanAutoStartReceiveFax())
        StartFastStartChannel(OpalMediaFormat::DefaultDataSessionID, H323Channel::IsReceiver);
#endif
      break;
  }
}


void H323Connection::SelectDefaultLogicalChannel(unsigned sessionID)
{
  if (FindChannel(sessionID, FALSE))
    return; 

  for (PINDEX i = 0; i < localCapabilities.GetSize(); i++) {
    H323Capability & localCapability = localCapabilities[i];
    if (localCapability.GetDefaultSessionID() == sessionID) {
      H323Capability * remoteCapability = remoteCapabilities.FindCapability(localCapability);
      if (remoteCapability != NULL) {
        PTRACE(3, "H323\tSelecting " << *remoteCapability);
        if (OpenLogicalChannel(*remoteCapability, sessionID, H323Channel::IsTransmitter))
          break;
        PTRACE(2, "H323\tOnSelectLogicalChannels, OpenLogicalChannel failed: "
               << *remoteCapability);
      }
    }
  }
}


void H323Connection::SelectFastStartChannels(unsigned sessionID,
                                             BOOL transmitter,
                                             BOOL receiver)
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


BOOL H323Connection::OpenLogicalChannel(const H323Capability & capability,
                                        unsigned sessionID,
                                        H323Channel::Directions dir)
{
  switch (fastStartState) {
    default : // FastStartDisabled
      if (dir == H323Channel::IsReceiver)
        return FALSE;

      // Traditional H245 handshake
      if (!logicalChannels->Open(capability, sessionID))
        return FALSE;
      transmitterMediaStream = logicalChannels->FindChannelBySession(sessionID, FALSE)->GetMediaStream();
      if (GetCall().OpenSourceMediaStreams(*this, capability.GetMediaFormat(), sessionID))
        return TRUE;
      PTRACE(1, "H323\tOpenLogicalChannel, OpenSourceMediaStreams failed: " << capability);
      return FALSE;

    case FastStartResponse :
      // Do not use OpenLogicalChannel for starting these.
      return FALSE;

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
    return FALSE;

  if (dir != H323Channel::IsReceiver)
    channel->SetNumber(logicalChannels->GetNextChannelNumber());

  fastStartChannels.Append(channel);
  return TRUE;
}


BOOL H323Connection::OnOpenLogicalChannel(const H245_OpenLogicalChannel & /*openPDU*/,
                                          H245_OpenLogicalChannelAck & /*ackPDU*/,
                                          unsigned & /*errorCode*/)
{
  // If get a OLC via H.245 stop trying to do fast start
  fastStartState = FastStartDisabled;
  if (!fastStartChannels.IsEmpty()) {
    fastStartChannels.RemoveAll();
    PTRACE(3, "H245\tReceived early start OLC, aborting fast start");
  }

  //errorCode = H245_OpenLogicalChannelReject_cause::e_unspecified;
  return TRUE;
}


BOOL H323Connection::OnConflictingLogicalChannel(H323Channel & conflictingChannel)
{
  unsigned session = conflictingChannel.GetSessionID();
  PTRACE(2, "H323\tLogical channel " << conflictingChannel
         << " conflict on session " << session
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

  BOOL fromRemote = conflictingChannel.GetNumber().IsFromRemote();
  H323Channel * channel = FindChannel(session, !fromRemote);
  if (channel == NULL) {
    PTRACE(1, "H323\tCould not resolve conflict, no reverse channel.");
    return FALSE;
  }

  if (!fromRemote) {
    conflictingChannel.Close();
    H323Capability * capability = remoteCapabilities.FindCapability(channel->GetCapability());
    if (capability == NULL) {
      PTRACE(1, "H323\tCould not resolve conflict, capability not available on remote.");
      return FALSE;
    }
    OpenLogicalChannel(*capability, session, H323Channel::IsTransmitter);
    return TRUE;
  }

  // Close the conflicting channel that got in before our transmitter
  channel->Close();
  CloseLogicalChannelNumber(channel->GetNumber());

  // Get the conflisting channel number to close
  H323ChannelNumber number = channel->GetNumber();

  // Must be slave and conflict from something we are sending, so try starting a
  // new channel using the master endpoints transmitter codec.
  logicalChannels->Open(conflictingChannel.GetCapability(), session, number);

  // Now close the conflicting channel
  CloseLogicalChannelNumber(number);
  return TRUE;
}


H323Channel * H323Connection::CreateLogicalChannel(const H245_OpenLogicalChannel & open,
                                                   BOOL startingFast,
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

    capability = remoteCapabilities.FindCapability(*dataType);
    if (capability == NULL) {
      // If we actually have the remote capabilities then the remote (very oddly)
      // had a fast connect entry it could not do. If we have not yet got a remote
      // cap table then build one using all possible caps.
      if (capabilityExchangeProcedure->HasReceivedCapabilities() ||
                (capability = endpoint.FindCapability(*dataType)) == NULL) {
        errorCode = H245_OpenLogicalChannelReject_cause::e_unknownDataType;
        PTRACE(1, "H323\tCreateLogicalChannel - unknown data type");
        return NULL; // If codec not supported, return error
      }
      
      capability = remoteCapabilities.Copy(*capability);
      remoteCapabilities.SetCapability(0, capability->GetDefaultSessionID(), capability);
    }
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

    // See if datatype is supported
    capability = localCapabilities.FindCapability(*dataType);
    if (capability == NULL) {
      errorCode = H245_OpenLogicalChannelReject_cause::e_unknownDataType;
      PTRACE(1, "H323\tCreateLogicalChannel - unknown data type");
      return NULL; // If codec not supported, return error
    }
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
  else if (channel->OnReceivedPDU(open, errorCode))
    return channel;

  PTRACE(1, "H323\tOnReceivedPDU gave error " << errorCode);
  delete channel;
  return NULL;
}


H323Channel * H323Connection::CreateRealTimeLogicalChannel(const H323Capability & capability,
                                                          H323Channel::Directions dir,
                                                                         unsigned sessionID,
							                         const H245_H2250LogicalChannelParameters * param,
                                                                        RTP_QOS * rtpqos)
{
  {
    PSafeLockReadOnly m(ownerCall);
    PSafePtr<OpalConnection> otherParty = GetCall().GetOtherPartyConnection(*this);
    if (otherParty == NULL) {
      PTRACE(1, "H323\tCowardly refusing to create an RTP channel with only one connection");
      return NULL;
    }

    if (ownerCall.IsMediaBypassPossible(*this, sessionID)) {
      MediaInformation info;
      if (!otherParty->GetMediaInformation(OpalMediaFormat::DefaultAudioSessionID, info))
        return new H323_ExternalRTPChannel(*this, capability, dir, sessionID);
      else
        return new H323_ExternalRTPChannel(*this, capability, dir, sessionID, info.data, info.control);
    }
  }

  RTP_Session * session;

  if (param != NULL) {
    // We only support unicast IP at this time.
    if (param->m_mediaControlChannel.GetTag() != H245_TransportAddress::e_unicastAddress)
      return NULL;

    const H245_UnicastAddress & uaddr = param->m_mediaControlChannel;
    if (uaddr.GetTag() != H245_UnicastAddress::e_iPAddress)
      return NULL;

    sessionID = param->m_sessionID;
  }

  session = UseSession(GetControlChannel(), sessionID, rtpqos);
  if (session == NULL)
    return NULL;

  ((RTP_UDP *) session)->Reopen(dir == H323Channel::IsReceiver);
  return new H323_RTPChannel(*this, capability, dir, *session);
}


BOOL H323Connection::OnCreateLogicalChannel(const H323Capability & capability,
                                            H323Channel::Directions dir,
                                            unsigned & errorCode)
{
  if (connectionState == ShuttingDownConnection) {
    errorCode = H245_OpenLogicalChannelReject_cause::e_unspecified;
    return FALSE;
  }

  // Default error if returns FALSE
  errorCode = H245_OpenLogicalChannelReject_cause::e_dataTypeALCombinationNotSupported;

  // Check if in set at all
  if (dir != H323Channel::IsReceiver) {
    if (!remoteCapabilities.IsAllowed(capability)) {
      PTRACE(2, "H323\tOnCreateLogicalChannel - transmit capability " << capability << " not allowed.");
      return FALSE;
    }
  }
  else {
    if (!localCapabilities.IsAllowed(capability)) {
      PTRACE(2, "H323\tOnCreateLogicalChannel - receive capability " << capability << " not allowed.");
      return FALSE;
    }
  }

  // Check all running channels, and if new one can't run with it return FALSE
  for (PINDEX i = 0; i < logicalChannels->GetSize(); i++) {
    H323Channel * channel = logicalChannels->GetChannelAt(i);
    if (channel != NULL && channel->GetDirection() == dir) {
      if (dir != H323Channel::IsReceiver) {
        if (!remoteCapabilities.IsAllowed(capability, channel->GetCapability())) {
          PTRACE(2, "H323\tOnCreateLogicalChannel - transmit capability " << capability
                 << " and " << channel->GetCapability() << " incompatible.");
          return FALSE;
        }
      }
      else {
        if (!localCapabilities.IsAllowed(capability, channel->GetCapability())) {
          PTRACE(2, "H323\tOnCreateLogicalChannel - transmit capability " << capability
                 << " and " << channel->GetCapability() << " incompatible.");
          return FALSE;
        }
      }
    }
  }

  return TRUE;
}


BOOL H323Connection::OnStartLogicalChannel(H323Channel & channel)
{
  return endpoint.OnStartLogicalChannel(*this, channel);
}


void H323Connection::CloseLogicalChannel(unsigned number, BOOL fromRemote)
{
  if (connectionState != ShuttingDownConnection)
    logicalChannels->Close(number, fromRemote);
}


void H323Connection::CloseLogicalChannelNumber(const H323ChannelNumber & number)
{
  CloseLogicalChannel(number, number.IsFromRemote());
}


void H323Connection::CloseAllLogicalChannels(BOOL fromRemote)
{
  for (PINDEX i = 0; i < logicalChannels->GetSize(); i++) {
    H245NegLogicalChannel & negChannel = logicalChannels->GetNegLogicalChannelAt(i);
    H323Channel * channel = negChannel.GetChannel();
    if (channel != NULL && channel->GetNumber().IsFromRemote() == fromRemote)
      negChannel.Close();
  }
}


BOOL H323Connection::OnClosingLogicalChannel(H323Channel & /*channel*/)
{
  return TRUE;
}


void H323Connection::OnClosedLogicalChannel(const H323Channel & channel)
{
  {
    PWaitAndSignal m(mediaStreamMutex);
    mediaStreams.Remove(channel.GetMediaStream(TRUE));
  }
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


unsigned H323Connection::GetBandwidthUsed() const
{
  unsigned used = 0;

  for (PINDEX i = 0; i < logicalChannels->GetSize(); i++) {
    H323Channel * channel = logicalChannels->GetChannelAt(i);
    if (channel != NULL)
      used += channel->GetBandwidthUsed();
  }

  PTRACE(3, "H323\tBandwidth used: " << used);

  return used;
}


BOOL H323Connection::SetBandwidthAvailable(unsigned newBandwidth, BOOL force)
{
  unsigned used = GetBandwidthUsed();
  if (used > newBandwidth) {
    if (!force)
      return FALSE;

    // Go through logical channels and close down some.
    PINDEX chanIdx = logicalChannels->GetSize();
    while (used > newBandwidth && chanIdx-- > 0) {
      H323Channel * channel = logicalChannels->GetChannelAt(chanIdx);
      if (channel != NULL) {
        used -= channel->GetBandwidthUsed();
        CloseLogicalChannelNumber(channel->GetNumber());
      }
    }
  }

  bandwidthAvailable = newBandwidth - used;
  return TRUE;
}


static BOOL CheckSendUserInputMode(const H323Capabilities & caps,
                                   OpalConnection::SendUserInputModes mode)
{
  // If have remote capabilities, then verify we can send selected mode,
  // otherwise just return and accept it for future validation
  static const H323_UserInputCapability::SubTypes types[H323Connection::NumSendUserInputModes] = {
    H323_UserInputCapability::NumSubTypes,
    H323_UserInputCapability::BasicString,
    H323_UserInputCapability::SignalToneH245,
    H323_UserInputCapability::SignalToneRFC2833,
    H323_UserInputCapability::NumSubTypes,
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


BOOL H323Connection::SendUserInputString(const PString & value)
{
  SendUserInputModes mode = GetRealSendUserInputMode();

  PTRACE(3, "H323\tSendUserInput(\"" << value << "\"), using mode " << mode);

  if (mode == SendUserInputAsString || mode == SendUserInputAsProtocolDefault)
    return SendUserInputIndicationString(value);

  return OpalConnection::SendUserInputString(value);
}


BOOL H323Connection::SendUserInputTone(char tone, unsigned duration)
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

  return OpalConnection::SendUserInputTone(tone, duration);
}


BOOL H323Connection::SendUserInputIndicationQ931(const PString & value)
{
  PTRACE(3, "H323\tSendUserInputIndicationQ931(\"" << value << "\")");

  H323SignalPDU pdu;
  pdu.BuildInformation(*this);
  pdu.GetQ931().SetKeypad(value);
  if (WriteSignalPDU(pdu))
    return TRUE;

  ClearCall(EndedByTransportFail);
  return FALSE;
}


BOOL H323Connection::SendUserInputIndicationString(const PString & value)
{
  PTRACE(3, "H323\tSendUserInputIndicationString(\"" << value << "\")");

  H323ControlPDU pdu;
  PASN_GeneralString & str = pdu.BuildUserInputIndication(value);
  if (!str.GetValue())
    return WriteControlPDU(pdu);

  PTRACE(1, "H323\tInvalid characters for UserInputIndication");
  return FALSE;
}


BOOL H323Connection::SendUserInputIndicationTone(char tone,
                                                 unsigned duration,
                                                 unsigned logicalChannel,
                                                 unsigned rtpTimestamp)
{
  PTRACE(3, "H323\tSendUserInputIndicationTone("
         << tone << ','
         << duration << ','
         << logicalChannel << ','
         << rtpTimestamp << ')');

  H323ControlPDU pdu;
  pdu.BuildUserInputIndication(tone, duration, logicalChannel, rtpTimestamp);
  return WriteControlPDU(pdu);
}


BOOL H323Connection::SendUserInputIndication(const H245_UserInputIndication & indication)
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
      const H245_UserInputIndication_signal & sig = ind;
      OnUserInputTone(sig.m_signalType[0],
                      sig.HasOptionalField(H245_UserInputIndication_signal::e_duration)
                                ? (unsigned)sig.m_duration : 0);
      break;
    }
    case H245_UserInputIndication::e_signalUpdate :
    {
      const H245_UserInputIndication_signalUpdate & sig = ind;
      OnUserInputTone(' ', sig.m_duration);
      break;
    }
  }
}


RTP_Session * H323Connection::GetSession(unsigned sessionID) const
{
  return rtpSessions.GetSession(sessionID);
}


H323_RTP_Session * H323Connection::GetSessionCallbacks(unsigned sessionID) const
{
  RTP_Session * session = rtpSessions.GetSession(sessionID);
  if (session == NULL)
    return NULL;

  PTRACE(3, "RTP\tFound existing session " << sessionID);
  PObject * data = session->GetUserData();
  PAssert(PIsDescendant(data, H323_RTP_Session), PInvalidCast);
  return (H323_RTP_Session *)data;
}


RTP_Session * H323Connection::UseSession(const OpalTransport & transport,
                                         unsigned sessionID,
                                         RTP_QOS * rtpqos)
{
  RTP_UDP * udp_session = (RTP_UDP *)OpalConnection::UseSession(transport, sessionID, rtpqos);
  if (udp_session == NULL)
    return NULL;

  if (udp_session->GetUserData() == NULL)
    udp_session->SetUserData(new H323_RTP_UDP(*this, *udp_session));
  return udp_session;
}


void H323Connection::ReleaseSession(unsigned sessionID)
{
  rtpSessions.ReleaseSession(sessionID);
}


void H323Connection::OnRTPStatistics(const RTP_Session & session) const
{
  endpoint.OnRTPStatistics(*this, session);
}


static void AddSessionCodecName(PStringStream & name, H323Channel * channel)
{
  if (channel == NULL)
    return;

  OpalMediaStream * stream = ((H323UnidirectionalChannel*)channel)->GetMediaStream();
  if (stream == NULL)
    return;

  OpalMediaFormat mediaFormat = stream->GetMediaFormat();
  if (mediaFormat.IsEmpty())
    return;

  if (name.IsEmpty())
    name << mediaFormat;
  else if (name != mediaFormat)
    name << " / " << mediaFormat;
}


PString H323Connection::GetSessionCodecNames(unsigned sessionID) const
{
  PStringStream name;

  AddSessionCodecName(name, FindChannel(sessionID, FALSE));
  AddSessionCodecName(name, FindChannel(sessionID, TRUE));

  return name;
}


BOOL H323Connection::RequestModeChange(const PString & newModes)
{
  return requestModeProcedure->StartRequest(newModes);
}


BOOL H323Connection::RequestModeChange(const H245_ArrayOf_ModeDescription & newModes)
{
  return requestModeProcedure->StartRequest(newModes);
}


BOOL H323Connection::OnRequestModeChange(const H245_RequestMode & pdu,
                                         H245_RequestModeAck & /*ack*/,
                                         H245_RequestModeReject & /*reject*/,
                                         PINDEX & selectedMode)
{
  for (selectedMode = 0; selectedMode < pdu.m_requestedModes.GetSize(); selectedMode++) {
    BOOL ok = TRUE;
    for (PINDEX i = 0; i < pdu.m_requestedModes[selectedMode].GetSize(); i++) {
      if (localCapabilities.FindCapability(pdu.m_requestedModes[selectedMode][i]) == NULL) {
        ok = FALSE;
        break;
      }
    }
    if (ok)
      return TRUE;
  }

  PTRACE(2, "H245\tMode change rejected as does not have capabilities");
  return FALSE;
}


void H323Connection::OnModeChanged(const H245_ModeDescription & newMode)
{
  CloseAllLogicalChannels(FALSE);

  // Start up the new ones
  for (PINDEX i = 0; i < newMode.GetSize(); i++) {
    H323Capability * capability = localCapabilities.FindCapability(newMode[i]);
    if (PAssertNULL(capability) != NULL) { // Should not occur as OnRequestModeChange checks them
      if (!OpenLogicalChannel(*capability,
                              capability->GetDefaultSessionID(),
                              H323Channel::IsTransmitter)) {
        PTRACE(2, "H245\tCould not open channel after mode change: " << *capability);
      }
    }
  }
}


void H323Connection::OnAcceptModeChange(const H245_RequestModeAck & pdu)
{
  if (t38ModeChangeCapabilities.IsEmpty())
    return;

  PTRACE(3, "H323\tT.38 mode change accepted.");

  // Now we have conviced the other side to send us T.38 data we should do the
  // same assuming the RequestModeChangeT38() function provided a list of \n
  // separaete capability names to start. Only one will be.

  CloseAllLogicalChannels(FALSE);

  PStringArray modes = t38ModeChangeCapabilities.Lines();

  PINDEX first, last;
  if (pdu.m_response.GetTag() == H245_RequestModeAck_response::e_willTransmitMostPreferredMode) {
    first = 0;
    last = 1;
  }
  else {
    first = 1;
    last = modes.GetSize();
  }

  for (PINDEX i = first; i < last; i++) {
    H323Capability * capability = localCapabilities.FindCapability(modes[i]);
    if (capability != NULL && OpenLogicalChannel(*capability,
                                                 capability->GetDefaultSessionID(),
                                                 H323Channel::IsTransmitter)) {
      PTRACE(3, "H245\tOpened " << *capability << " after T.38 mode change");
      break;
    }

    PTRACE(2, "H245\tCould not open channel after T.38 mode change");
  }

  t38ModeChangeCapabilities = PString::Empty();
}


void H323Connection::OnRefusedModeChange(const H245_RequestModeReject * /*pdu*/)
{
  if (!t38ModeChangeCapabilities) {
    PTRACE(2, "H323\tT.38 mode change rejected.");
    t38ModeChangeCapabilities = PString::Empty();
  }
}


BOOL H323Connection::RequestModeChangeT38(const char * capabilityNames)
{
  t38ModeChangeCapabilities = capabilityNames;
  if (RequestModeChange(t38ModeChangeCapabilities))
    return TRUE;

  t38ModeChangeCapabilities = PString::Empty();
  return FALSE;
}


BOOL H323Connection::GetAdmissionRequestAuthentication(const H225_AdmissionRequest & /*arq*/,
                                                       H235Authenticators & /*authenticators*/)
{
  return FALSE;
}


const H323Transport & H323Connection::GetControlChannel() const
{
  return *(controlChannel != NULL ? controlChannel : signallingChannel);
}

OpalTransport & H323Connection::GetTransport() const
{
  return *(controlChannel != NULL ? controlChannel : signallingChannel);
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

  if (GetPhase() >= ReleasingPhase)
    return;

  if (endpoint.GetRoundTripDelayRate() > 0 && !roundTripDelayTimer.IsRunning()) {
    roundTripDelayTimer = endpoint.GetRoundTripDelayRate();
    StartRoundTripDelay();
  }

/*
  if (endpoint.GetNoMediaTimeout() > 0) {
    BOOL oneRunning = FALSE;
    BOOL allSilent = TRUE;
    for (PINDEX i = 0; i < logicalChannels->GetSize(); i++) {
      H323Channel * channel = logicalChannels->GetChannelAt(i);
      if (channel != NULL && channel->IsDescendant(H323_RTPChannel::Class())) {
        if (channel->IsRunning()) {
          oneRunning = TRUE;
          if (((H323_RTPChannel *)channel)->GetSilenceDuration() < endpoint.GetNoMediaTimeout()) {
            allSilent = FALSE;
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

BOOL H323Connection::OnSendFeatureSet(unsigned code, H225_FeatureSet & features) const
{
  return endpoint.OnSendFeatureSet(code, features);
}

void H323Connection::OnReceiveFeatureSet(unsigned code, const H225_FeatureSet & features) const
{
  endpoint.OnReceiveFeatureSet(code, features);
}

#ifdef H323_H460
H460_FeatureSet * H323Connection::GetFeatureSet()
{
	return &features;
}
#endif


/////////////////////////////////////////////////////////////////////////////
