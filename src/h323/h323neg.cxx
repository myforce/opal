/*
 * h323neg.cxx
 *
 * H.323 PDU definitions
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
 * Portions of this code were written with the assisance of funding from
 * Vovida Networks, Inc. http://www.vovida.com.
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
#pragma implementation "h323neg.h"
#endif

#include <h323/h323neg.h>

#include <ptclib/random.h>
#include <h323/h323ep.h>


#define new PNEW


///////////////////////////////////////////////////////////////////////////////

H245Negotiator::H245Negotiator(H323EndPoint & end, H323Connection & conn)
  : endpoint(end),
    connection(conn)
{
  PTRACE_CONTEXT_ID_FROM(conn);
  PTRACE_CONTEXT_ID_TO(replyTimer);
  replyTimer.SetNotifier(PCREATE_NOTIFIER(HandleTimeoutUnlocked));
}


void H245Negotiator::HandleTimeoutUnlocked(PTimer &, P_INT_PTR)
{
  if (connection.LockReadWrite()) {
    HandleTimeout();
    connection.UnlockReadWrite();
  }
}


void H245Negotiator::HandleTimeout()
{
}


/////////////////////////////////////////////////////////////////////////////

H245NegMasterSlaveDetermination::H245NegMasterSlaveDetermination(H323EndPoint & end,
                                                                 H323Connection & conn)
  : H245Negotiator(end, conn)
  , m_state(e_Idle)
  , m_determinationNumber(0)
  , m_retryCount(1)
  , m_status(e_Indeterminate)
{
}


PBoolean H245NegMasterSlaveDetermination::Start(PBoolean renegotiate)
{
  if (m_state != e_Idle) {
    PTRACE(3, "H245\tMasterSlaveDetermination already in progress");
    return true;
  }

  if (!renegotiate && IsDetermined())
    return true;

  m_retryCount = 1;
  return Restart();
}


PBoolean H245NegMasterSlaveDetermination::Restart()
{
  PTRACE(3, "H245\tSending MasterSlaveDetermination");

  // Begin the Master/Slave determination procedure
  m_determinationNumber = PRandom::Number() % 16777216;
  replyTimer = endpoint.GetMasterSlaveDeterminationTimeout();
  m_state = e_Outgoing;

  H323ControlPDU pdu;
  pdu.BuildMasterSlaveDetermination(endpoint.GetTerminalType(), m_determinationNumber);
  return connection.WriteControlPDU(pdu);
}


void H245NegMasterSlaveDetermination::Stop()
{
  PTRACE(3, "H245\tStopping MasterSlaveDetermination: state=" << m_state);

  if (m_state == e_Idle)
    return;

  replyTimer.Stop(false);
  m_state = e_Idle;
}


PBoolean H245NegMasterSlaveDetermination::HandleIncoming(const H245_MasterSlaveDetermination & pdu)
{
  PTRACE(3, "H245\tReceived MasterSlaveDetermination: state=" << m_state);

  if (m_state == e_Incoming) {
    replyTimer.Stop(false);
    m_state = e_Idle;
    return connection.OnControlProtocolError(H323Connection::e_MasterSlaveDetermination,
                                             "Duplicate MasterSlaveDetermination");
  }

  replyTimer = endpoint.GetMasterSlaveDeterminationTimeout();

  // Determine the master and slave
  MasterSlaveStatus newStatus;
  if (pdu.m_terminalType < (unsigned)endpoint.GetTerminalType())
    newStatus = e_DeterminedMaster;
  else if (pdu.m_terminalType > (unsigned)endpoint.GetTerminalType())
    newStatus = e_DeterminedSlave;
  else {
    DWORD moduloDiff = (pdu.m_statusDeterminationNumber - m_determinationNumber) & 0xffffff;
    if (moduloDiff == 0 || moduloDiff == 0x800000)
      newStatus = e_Indeterminate;
    else if (moduloDiff < 0x800000)
      newStatus = e_DeterminedMaster;
    else
      newStatus = e_DeterminedSlave;
  }

  H323ControlPDU reply;

  if (newStatus != e_Indeterminate) {
    PTRACE(3, "H245\tMasterSlaveDetermination: local is "
                  << (newStatus == e_DeterminedMaster ? "master" : "slave"));
    reply.BuildMasterSlaveDeterminationAck(newStatus == e_DeterminedMaster);
    m_state = m_state == e_Outgoing ? e_Incoming : e_Idle;
    m_status = newStatus;
  }
  else if (m_state == e_Outgoing) {
    if (++m_retryCount < endpoint.GetMasterSlaveDeterminationRetries())
      return Restart(); // Try again

    replyTimer.Stop(false);
    m_state = e_Idle;
    return connection.OnControlProtocolError(H323Connection::e_MasterSlaveDetermination,
                                             "Retries exceeded");
  }
  else {
    reply.BuildMasterSlaveDeterminationReject(H245_MasterSlaveDeterminationReject_cause::e_identicalNumbers);
  }

  return connection.WriteControlPDU(reply);
}


PBoolean H245NegMasterSlaveDetermination::HandleAck(const H245_MasterSlaveDeterminationAck & pdu)
{
  PTRACE(3, "H245\tReceived MasterSlaveDeterminationAck: state=" << m_state);

  if (m_state == e_Idle)
    return true;

  replyTimer = endpoint.GetMasterSlaveDeterminationTimeout();

  MasterSlaveStatus newStatus;
  if (pdu.m_decision.GetTag() == H245_MasterSlaveDeterminationAck_decision::e_master)
    newStatus = e_DeterminedMaster;
  else
    newStatus = e_DeterminedSlave;

  H323ControlPDU reply;
  if (m_state == e_Outgoing) {
    m_status = newStatus;
    PTRACE(3, "H245\tMasterSlaveDetermination: remote is "
                  << (newStatus == e_DeterminedSlave ? "master" : "slave"));
    reply.BuildMasterSlaveDeterminationAck(newStatus == e_DeterminedMaster);
    if (!connection.WriteControlPDU(reply))
      return false;
  }

  replyTimer.Stop(false);
  m_state = e_Idle;

  if (m_status != newStatus)
    return connection.OnControlProtocolError(H323Connection::e_MasterSlaveDetermination,
                                             "Master/Slave mismatch");

  return true;
}


PBoolean H245NegMasterSlaveDetermination::HandleReject(const H245_MasterSlaveDeterminationReject & pdu)
{
  PTRACE(3, "H245\tReceived MasterSlaveDeterminationReject: state=" << m_state);

  switch (m_state) {
    case e_Idle :
      return true;

    case e_Outgoing :
      if (pdu.m_cause.GetTag() == H245_MasterSlaveDeterminationReject_cause::e_identicalNumbers) {
        if (++m_retryCount < endpoint.GetMasterSlaveDeterminationRetries())
          return Restart();
      }

    default :
      break;
  }

  replyTimer.Stop(false);
  m_state = e_Idle;

  return connection.OnControlProtocolError(H323Connection::e_MasterSlaveDetermination,
                                           "Retries exceeded");
}


PBoolean H245NegMasterSlaveDetermination::HandleRelease(const H245_MasterSlaveDeterminationRelease & /*pdu*/)
{
  PTRACE(3, "H245\tReceived MasterSlaveDeterminationRelease: state=" << m_state);

  if (m_state == e_Idle)
    return true;

  replyTimer.Stop(false);
  m_state = e_Idle;

  return connection.OnControlProtocolError(H323Connection::e_MasterSlaveDetermination,
                                           "Aborted");
}


void H245NegMasterSlaveDetermination::HandleTimeout()
{
  if (m_state == e_Idle)
    return;

  PTRACE(3, "H245\tTimeout on MasterSlaveDetermination: state=" << m_state);

  if (m_state == e_Outgoing) {
    H323ControlPDU reply;
    reply.Build(H245_IndicationMessage::e_masterSlaveDeterminationRelease);
    connection.WriteControlPDU(reply);
  }

  m_state = e_Idle;

  connection.OnControlProtocolError(H323Connection::e_MasterSlaveDetermination,
                                    "Timeout");
}


/////////////////////////////////////////////////////////////////////////////

H245NegTerminalCapabilitySet::H245NegTerminalCapabilitySet(H323EndPoint & end,
                                                           H323Connection & conn)
  : H245Negotiator(end, conn)
{
  inSequenceNumber = UINT_MAX;
  outSequenceNumber = 0;
  state = e_Idle;
  receivedCapabilites = false;
}


PBoolean H245NegTerminalCapabilitySet::Start(PBoolean renegotiate, PBoolean empty)
{
  if (state == e_InProgress) {
    PTRACE(2, "H245\tTerminalCapabilitySet already in progress: outSeq=" << outSequenceNumber);
    return true;
  }

  if (!renegotiate && state == e_Confirmed) {
    PTRACE(2, "H245\tTerminalCapabilitySet already sent.");
    return true;
  }

  // Begin the capability exchange procedure
  outSequenceNumber = (outSequenceNumber+1)%256;
  replyTimer = endpoint.GetCapabilityExchangeTimeout();
  state = e_InProgress;

  PTRACE(3, "H245\tSending TerminalCapabilitySet: outSeq=" << outSequenceNumber);

  H323ControlPDU pdu;
  connection.OnSendCapabilitySet(pdu.BuildTerminalCapabilitySet(connection, outSequenceNumber, empty));
  return connection.WriteControlPDU(pdu);
}


void H245NegTerminalCapabilitySet::Stop(PBoolean dec)
{
  PTRACE(3, "H245\tStopping TerminalCapabilitySet: state=" << state);

  if (state == e_Idle)
    return;

  replyTimer.Stop(false);
  state = e_Idle;
  receivedCapabilites = false;

  if (dec) {
    if (outSequenceNumber == 0)
      outSequenceNumber = 255;
    else
      --outSequenceNumber;
  }
}


PBoolean H245NegTerminalCapabilitySet::HandleIncoming(const H245_TerminalCapabilitySet & pdu)
{
  PTRACE(3, "H245\tReceived TerminalCapabilitySet:"
                     " state=" << state <<
                     " pduSeq=" << pdu.m_sequenceNumber <<
                     " inSeq=" << inSequenceNumber);

  if (pdu.m_sequenceNumber == inSequenceNumber) {
    PTRACE(2, "H245\tIgnoring TerminalCapabilitySet, already received sequence number");
    return true;  // Already had this one
  }

  inSequenceNumber = pdu.m_sequenceNumber;

  H323Capabilities remoteCapabilities(connection, pdu);

  const H245_MultiplexCapability * muxCap = NULL;
  if (pdu.HasOptionalField(H245_TerminalCapabilitySet::e_multiplexCapability))
    muxCap = &pdu.m_multiplexCapability;

  H323ControlPDU reject;
  if (connection.OnReceivedCapabilitySet(remoteCapabilities, muxCap,
                    reject.BuildTerminalCapabilitySetReject(inSequenceNumber,
                            H245_TerminalCapabilitySetReject_cause::e_unspecified))) {
    receivedCapabilites = true;
    H323ControlPDU ack;
    ack.BuildTerminalCapabilitySetAck(inSequenceNumber);
    return connection.WriteControlPDU(ack);
  }

  connection.WriteControlPDU(reject);
  connection.ClearCall(H323Connection::EndedByCapabilityExchange);
  return true;
}


PBoolean H245NegTerminalCapabilitySet::HandleAck(const H245_TerminalCapabilitySetAck & pdu)
{
  PTRACE(3, "H245\tReceived TerminalCapabilitySetAck:"
                     " state=" << state <<
                     " pduSeq=" << pdu.m_sequenceNumber <<
                     " outSeq=" << (unsigned)outSequenceNumber);

  if (state != e_InProgress)
    return true;

  if (pdu.m_sequenceNumber != outSequenceNumber)
    return true;

  replyTimer.Stop(false);
  state = e_Confirmed;
  PTRACE(3, "H245\tTerminalCapabilitySet Sent.");
  return true;
}


PBoolean H245NegTerminalCapabilitySet::HandleReject(const H245_TerminalCapabilitySetReject & pdu)
{
  PTRACE(3, "H245\tReceived TerminalCapabilitySetReject:"
                     " state=" << state <<
                     " pduSeq=" << pdu.m_sequenceNumber <<
                     " outSeq=" << (unsigned)outSequenceNumber);

  if (state != e_InProgress)
    return true;

  if (pdu.m_sequenceNumber != outSequenceNumber)
    return true;

  state = e_Idle;
  replyTimer.Stop(false);
  return connection.OnControlProtocolError(H323Connection::e_CapabilityExchange,
                                           "Rejected");
}


PBoolean H245NegTerminalCapabilitySet::HandleRelease(const H245_TerminalCapabilitySetRelease & /*pdu*/)
{
  PTRACE(3, "H245\tReceived TerminalCapabilityRelease: state=" << state);

  receivedCapabilites = false;
  return connection.OnControlProtocolError(H323Connection::e_CapabilityExchange,
                                           "Aborted");
}


void H245NegTerminalCapabilitySet::HandleTimeout()
{
  if (state == e_Idle)
    return;

  PTRACE(3, "H245\tTimeout on TerminalCapabilitySet: state=" << state);

  H323ControlPDU reply;
  reply.Build(H245_IndicationMessage::e_terminalCapabilitySetRelease);
  connection.WriteControlPDU(reply);

  connection.OnControlProtocolError(H323Connection::e_CapabilityExchange, "Timeout");
}


/////////////////////////////////////////////////////////////////////////////

H245NegLogicalChannel::H245NegLogicalChannel(H323EndPoint & end,
                                             H323Connection & conn,
                                             const H323ChannelNumber & chanNum)
  : H245Negotiator(end, conn),
    channelNumber(chanNum)
{
  channel = NULL;
  state = e_Released;
}


H245NegLogicalChannel::H245NegLogicalChannel(H323EndPoint & end,
                                             H323Connection & conn,
                                             H323Channel & chan)
  : H245Negotiator(end, conn),
    channelNumber(chan.GetNumber())
{
  channel = &chan;
  state = e_Established;
}


H245NegLogicalChannel::~H245NegLogicalChannel()
{
  replyTimer.Stop();
  PThread::Yield(); // Do this to avoid possible race condition with timer

  delete channel;
}


PBoolean H245NegLogicalChannel::Open(const H323Capability & capability,
                                 unsigned sessionID,
                                 unsigned replacementFor,
                                 OpalMediaStreamPtr mediaStream)
{
  if (state != e_Released && state != e_AwaitingRelease) {
    PTRACE(2, "H245\tOpen of channel currently in negotiations: " << channelNumber);
    return false;
  }

  state = e_AwaitingEstablishment;

  PTRACE(3, "H245\tOpening channel: " << channelNumber);

  if (channel != NULL) {
    channel->Close();
    delete channel;
  }

  channel = capability.CreateChannel(connection, H323Channel::IsTransmitter, sessionID, NULL);
  if (channel == NULL) {
    PTRACE(1, "H245\tOpen channel: " << channelNumber << ", capability.CreateChannel() failed");
    return false;
  }

  channel->SetNumber(channelNumber);
  channel->SetMediaStream(mediaStream);

  if (!channel->PreOpen()) {
    PTRACE(2, "H245\tOpen channel: " << channelNumber << ", pre-open failed");
    return false;
  }

  if (!channel->SetInitialBandwidth()) {
    PTRACE(2, "H245\tOpen channel: " << channelNumber << ", Insufficient bandwidth");
    return false;
  }

  H323ControlPDU pdu;
  H245_OpenLogicalChannel & open = pdu.BuildOpenLogicalChannel(channelNumber);
  if (!channel->OnSendingPDU(open)) {
    PTRACE(1, "H245\tOpen channel: " << channelNumber << ", channel->OnSendingPDU() failed");
    return false;
  }

  if (replacementFor > 0) {
    if (open.HasOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters)) {
      open.m_reverseLogicalChannelParameters.IncludeOptionalField(H245_OpenLogicalChannel_reverseLogicalChannelParameters::e_replacementFor);
      open.m_reverseLogicalChannelParameters.m_replacementFor = replacementFor;
    }
    else {
      open.m_forwardLogicalChannelParameters.IncludeOptionalField(H245_OpenLogicalChannel_forwardLogicalChannelParameters::e_replacementFor);
      open.m_forwardLogicalChannelParameters.m_replacementFor = replacementFor;
    }
  }

  replyTimer = endpoint.GetLogicalChannelTimeout();

  return connection.WriteControlPDU(pdu);
}


PBoolean H245NegLogicalChannel::Close()
{
  switch (state) {
    case e_Establishing :
    case e_AwaitingRelease :
    case e_Released :
      return true;

    default :
      break;
  }

  PTRACE(3, "H245\tClosing channel: " << channelNumber << ", state=" << state);

  replyTimer = endpoint.GetLogicalChannelTimeout();

  H323ControlPDU reply;

  if (channelNumber.IsFromRemote()) {
    reply.BuildRequestChannelClose(channelNumber, H245_RequestChannelClose_reason::e_normal);
    state = e_AwaitingResponse;
  }
  else {
    reply.BuildCloseLogicalChannel(channelNumber);
    state = e_AwaitingRelease;
    if (channel != NULL)
      channel->Close();
  }

  return connection.WriteControlPDU(reply);
}


PBoolean H245NegLogicalChannel::HandleOpen(const H245_OpenLogicalChannel & pdu)
{
  H323ControlPDU reply;
  H245_OpenLogicalChannelAck & ack = reply.BuildOpenLogicalChannelAck(channelNumber);

  if (channel != NULL) {
    if (state == e_Established) {
      PTRACE(3, "H245\tReceived duplicate open channel: " << channelNumber << ", state=" << state);
      // Really should check that the codec has not changed, later maybe
      channel->OnSendOpenAck(pdu, ack);
      return connection.WriteControlPDU(reply);
    }

    PTRACE(3, "H245\tClosing channel due to received open channel: " << channelNumber << ", state=" << state);
    channel->Close();
    delete channel;
    channel = NULL;
  }

  PTRACE(3, "H245\tReceived open channel: " << channelNumber << ", state=" << state);

  state = e_Establishing;

  PBoolean ok = false;

  unsigned cause = H245_OpenLogicalChannelReject_cause::e_unspecified;
  channel = connection.CreateLogicalChannel(pdu, FALSE, cause);

  if (channel != NULL) {
    channel->SetNumber(channelNumber);
    channel->OnSendOpenAck(pdu, ack);

    if (connection.OnOpenLogicalChannel(pdu, ack, cause, *channel)) {
      if (channel->GetDirection() == H323Channel::IsBidirectional) {
        state = e_AwaitingConfirmation;
        replyTimer = endpoint.GetLogicalChannelTimeout(); // T103
        ok = true;
      }
      else {
        ok = channel->Open();
        if (ok)
          state = e_Established;
      }
    }
    else {
      channel->Close();
      delete channel;
      channel = NULL;
      ok = false;
    }
  }

  if (ok) {
    OpalMediaStreamPtr mediaStream = channel->GetMediaStream();
    if (mediaStream != NULL && connection.OnOpenMediaStream(*mediaStream))
      mediaStream->Start();
    else
      ok = false;
  }

  if (!ok) {
    reply.BuildOpenLogicalChannelReject(channelNumber, cause);
    Release();
  }

  return connection.WriteControlPDU(reply);
}


PBoolean H245NegLogicalChannel::HandleOpenAck(const H245_OpenLogicalChannelAck & pdu)
{
  PTRACE(3, "H245\tReceived open channel ack: " << channelNumber << ", state=" << state);

  switch (state) {
    case e_Released :
      return connection.OnControlProtocolError(H323Connection::e_LogicalChannel,
                                               "Ack unknown channel");
    case e_AwaitingEstablishment :
      state = e_Established;
      replyTimer.Stop(false);

      if (!channel->OnReceivedAckPDU(pdu)) {
        if (connection.GetRemoteProductInfo().name != "Cisco IOS")
          return Close();
        PTRACE(4, "H245\tWorkaround for Cisco bug, cannot close channel on illegal ack or it hangs up on you.");
        return true;
      }

      if (channel->GetDirection() == H323Channel::IsBidirectional) {
        H323ControlPDU reply;
        reply.BuildOpenLogicalChannelConfirm(channelNumber);
        if (!connection.WriteControlPDU(reply))
          return false;
      }

      // Channel was already opened when OLC sent, if have error here it is
      // somthing other than an asymmetric codec conflict, so close it down.
      if (channel->Open()) {
        OpalMediaStreamPtr mediaStream = channel->GetMediaStream();
        if (mediaStream != NULL) {
          if (connection.OnOpenMediaStream(*mediaStream))
            mediaStream->Start();
          else
            return Close();
        }
      }
      else
        return Close();

    default :
      break;
  }

  return true;
}


PBoolean H245NegLogicalChannel::HandleOpenConfirm(const H245_OpenLogicalChannelConfirm & /*pdu*/)
{
  PTRACE(3, "H245\tReceived open channel confirm: " << channelNumber << ", state=" << state);

  switch (state) {
    case e_Released :
      return connection.OnControlProtocolError(H323Connection::e_LogicalChannel,
                                               "Confirm unknown channel");
    case e_AwaitingEstablishment :
      return connection.OnControlProtocolError(H323Connection::e_LogicalChannel,
                                               "Confirm established channel");
    case e_AwaitingConfirmation :
      replyTimer.Stop(false);
      state = e_Established;
      // Channel was already opened when OLC sent, if have error here it is
      // somthing other than an asymmetric codec conflict, so close it down.
      if (!channel->Open())
        return Close();

    default :
      break;
  }

  return true;
}


PBoolean H245NegLogicalChannel::HandleReject(const H245_OpenLogicalChannelReject & pdu)
{
  PTRACE(3, "H245\tReceived open channel reject: " << channelNumber
         << ", cause=" << pdu.m_cause.GetTagName() << ", state=" << state);

  switch (state) {
    case e_Released :
      return connection.OnControlProtocolError(H323Connection::e_LogicalChannel,
                                               "Reject unknown channel");
    case e_Established :
      Release();
      return connection.OnControlProtocolError(H323Connection::e_LogicalChannel,
                                               "Reject established channel");
    case e_AwaitingEstablishment :
      // Master rejected our attempt to open, so try something else.
      if (pdu.m_cause.GetTag() == H245_OpenLogicalChannelReject_cause::e_masterSlaveConflict) {
        if (!connection.OnConflictingLogicalChannel(*channel))
          return false;
      }
      // Do next case

    case e_AwaitingRelease :
      Release();
      break;

    default :
      break;
  }

  return true;
}


PBoolean H245NegLogicalChannel::HandleClose(const H245_CloseLogicalChannel & /*pdu*/)
{
  PTRACE(3, "H245\tReceived close channel: " << channelNumber << ", state=" << state);
  Release();
  return true;
}


PBoolean H245NegLogicalChannel::HandleCloseAck(const H245_CloseLogicalChannelAck & /*pdu*/)
{
  PTRACE(3, "H245\tReceived close channel ack: " << channelNumber << ", state=" << state);

  switch (state) {
    case e_Established :
      Release();
      return connection.OnControlProtocolError(H323Connection::e_LogicalChannel,
                                               "Close ack open channel");
    case e_AwaitingRelease :
      Release();
      break;

    default :
      break;
  }

  return true;
}


PBoolean H245NegLogicalChannel::HandleRequestClose(const H245_RequestChannelClose & pdu)
{
  PTRACE(3, "H245\tReceived request close channel: " << channelNumber << ", state=" << state);

  if (state != e_Established)
    return true;    // Already closed

  H323ControlPDU reply;
  if (connection.OnClosingLogicalChannel(*channel)) {
    reply.BuildRequestChannelCloseAck(channelNumber);
    if (!connection.WriteControlPDU(reply))
      return false;

    // Do normal Close procedure
    replyTimer = endpoint.GetLogicalChannelTimeout();
    reply.BuildCloseLogicalChannel(channelNumber);
    state = e_AwaitingRelease;

    if (pdu.m_reason.GetTag() == H245_RequestChannelClose_reason::e_reopen) {
      PTRACE(2, "H245\tReopening channel: " << channelNumber);
      connection.OpenLogicalChannel(channel->GetCapability(),
                                    channel->GetSessionID(),
                                    channel->GetDirection());
    }
  }
  else
    reply.BuildRequestChannelCloseReject(channelNumber);

  return connection.WriteControlPDU(reply);
}


PBoolean H245NegLogicalChannel::HandleRequestCloseAck(const H245_RequestChannelCloseAck & /*pdu*/)
{
  PTRACE(3, "H245\tReceived request close ack channel: " << channelNumber << ", state=" << state);

  if (state == e_AwaitingResponse)
    Release();  // Other end says close OK, so do so.

  return true;
}


PBoolean H245NegLogicalChannel::HandleRequestCloseReject(const H245_RequestChannelCloseReject & /*pdu*/)
{
  PTRACE(3, "H245\tReceived request close reject channel: " << channelNumber << ", state=" << state);

  // Other end refused close, so go back to still having channel open
  if (state == e_AwaitingResponse)
    state = e_Established;

  return true;
}


PBoolean H245NegLogicalChannel::HandleRequestCloseRelease(const H245_RequestChannelCloseRelease & /*pdu*/)
{
  PTRACE(3, "H245\tReceived request close release channel: " << channelNumber << ", state=" << state);

  // Other end refused close, so go back to still having channel open
  state = e_Established;

  return true;
}


void H245NegLogicalChannel::HandleTimeout()
{
  PTRACE(3, "H245\tTimeout on open channel: " << channelNumber << ", state=" << state);

  H323ControlPDU reply;
  switch (state) {
    case e_AwaitingEstablishment :
      reply.BuildCloseLogicalChannel(channelNumber);
      connection.WriteControlPDU(reply);
      break;

    case e_AwaitingResponse :
      reply.BuildRequestChannelCloseRelease(channelNumber);
      connection.WriteControlPDU(reply);
      break;

    case e_Released :
      return;

    default :
      break;
  }

  Release();
  connection.OnControlProtocolError(H323Connection::e_LogicalChannel, "Timeout");
}


void H245NegLogicalChannel::Release()
{
  state = e_Released;
  H323Channel * chan = channel;
  channel = NULL;

  replyTimer.Stop(false);

  PTRACE(4, "H245\tOLC Release: chan=" << chan);
  
  if (chan != NULL) {
    chan->Close();
    delete chan;
  }
}


H323Channel * H245NegLogicalChannel::GetChannel() const
{
  return channel;
}


/////////////////////////////////////////////////////////////////////////////

H245NegLogicalChannels::H245NegLogicalChannels(H323EndPoint & end,
                                               H323Connection & conn)
  : H245Negotiator(end, conn),
    lastChannelNumber(100, false)
{
}


void H245NegLogicalChannels::Add(H323Channel & channel)
{
  channels.SetAt(channel.GetNumber(), new H245NegLogicalChannel(endpoint, connection, channel));
}


PBoolean H245NegLogicalChannels::Open(const H323Capability & capability,
                                  unsigned sessionID,
                                  unsigned replacementFor,
                                  OpalMediaStreamPtr mediaStream)
{
  lastChannelNumber++;

  H245NegLogicalChannel * negChan = new H245NegLogicalChannel(endpoint, connection, lastChannelNumber);
  channels.SetAt(lastChannelNumber, negChan);

  return negChan->Open(capability, sessionID, replacementFor, mediaStream);
}


PBoolean H245NegLogicalChannels::Close(unsigned channelNumber, PBoolean fromRemote)
{
  H245NegLogicalChannel * chan = FindNegLogicalChannel(channelNumber, fromRemote);
  if (chan != NULL)
    return chan->Close();

  return connection.OnControlProtocolError(H323Connection::e_LogicalChannel,
                                           "Close unknown");
}


PBoolean H245NegLogicalChannels::HandleOpen(const H245_OpenLogicalChannel & pdu)
{
  H323ChannelNumber chanNum(pdu.m_forwardLogicalChannelNumber, true);
  H245NegLogicalChannel * chan;

  if (channels.Contains(chanNum))
    chan = &channels[chanNum];
  else {
    chan = new H245NegLogicalChannel(endpoint, connection, chanNum);
    channels.SetAt(chanNum, chan);
  }

  return chan->HandleOpen(pdu);
}


PBoolean H245NegLogicalChannels::HandleOpenAck(const H245_OpenLogicalChannelAck & pdu)
{
  H245NegLogicalChannel * chan = FindNegLogicalChannel(pdu.m_forwardLogicalChannelNumber, false);
  if (chan != NULL)
    return chan->HandleOpenAck(pdu);

  return connection.OnControlProtocolError(H323Connection::e_LogicalChannel,
                                           "Ack unknown");
}


PBoolean H245NegLogicalChannels::HandleOpenConfirm(const H245_OpenLogicalChannelConfirm & pdu)
{
  H245NegLogicalChannel * chan = FindNegLogicalChannel(pdu.m_forwardLogicalChannelNumber, true);
  if (chan != NULL)
    return chan->HandleOpenConfirm(pdu);

  return connection.OnControlProtocolError(H323Connection::e_LogicalChannel,
                                           "Confirm unknown");
}


PBoolean H245NegLogicalChannels::HandleReject(const H245_OpenLogicalChannelReject & pdu)
{
  H245NegLogicalChannel * chan = FindNegLogicalChannel(pdu.m_forwardLogicalChannelNumber, false);
  if (chan != NULL)
    return chan->HandleReject(pdu);

  return connection.OnControlProtocolError(H323Connection::e_LogicalChannel,
                                           "Reject unknown");
}


PBoolean H245NegLogicalChannels::HandleClose(const H245_CloseLogicalChannel & pdu)
{
  H245NegLogicalChannel * chan = FindNegLogicalChannel(pdu.m_forwardLogicalChannelNumber, true);
  if (chan != NULL) {
    if (!chan->HandleClose(pdu))
      return false;
  }
  else {
    if (!connection.OnControlProtocolError(H323Connection::e_LogicalChannel, "Close unknown"))
      return false;
  }

  H323ControlPDU reply;
  reply.BuildCloseLogicalChannelAck(pdu.m_forwardLogicalChannelNumber);
  return connection.WriteControlPDU(reply);
}


PBoolean H245NegLogicalChannels::HandleCloseAck(const H245_CloseLogicalChannelAck & pdu)
{
  H245NegLogicalChannel * chan = FindNegLogicalChannel(pdu.m_forwardLogicalChannelNumber, false);
  if (chan != NULL)
    return chan->HandleCloseAck(pdu);

  return connection.OnControlProtocolError(H323Connection::e_LogicalChannel,
                                           "Close Ack unknown");
}


PBoolean H245NegLogicalChannels::HandleRequestClose(const H245_RequestChannelClose & pdu)
{
  H245NegLogicalChannel * chan = FindNegLogicalChannel(pdu.m_forwardLogicalChannelNumber, false);
  if (chan != NULL)
    return chan->HandleRequestClose(pdu);

  if (!connection.OnControlProtocolError(H323Connection::e_LogicalChannel, "Request Close unknown"))
    return false;

  H323ControlPDU reply;
  reply.BuildRequestChannelCloseReject(pdu.m_forwardLogicalChannelNumber);
  return connection.WriteControlPDU(reply);
}


PBoolean H245NegLogicalChannels::HandleRequestCloseAck(const H245_RequestChannelCloseAck & pdu)
{
  H245NegLogicalChannel * chan = FindNegLogicalChannel(pdu.m_forwardLogicalChannelNumber, true);
  if (chan != NULL)
    return chan->HandleRequestCloseAck(pdu);

  return connection.OnControlProtocolError(H323Connection::e_LogicalChannel,
                                           "Request Close Ack unknown");
}


PBoolean H245NegLogicalChannels::HandleRequestCloseReject(const H245_RequestChannelCloseReject & pdu)
{
  H245NegLogicalChannel * chan = FindNegLogicalChannel(pdu.m_forwardLogicalChannelNumber, true);
  if (chan != NULL)
    return chan->HandleRequestCloseReject(pdu);

  return connection.OnControlProtocolError(H323Connection::e_LogicalChannel,
                                           "Request Close Reject unknown");
}


PBoolean H245NegLogicalChannels::HandleRequestCloseRelease(const H245_RequestChannelCloseRelease & pdu)
{
  H245NegLogicalChannel * chan = FindNegLogicalChannel(pdu.m_forwardLogicalChannelNumber, false);
  if (chan != NULL)
    return chan->HandleRequestCloseRelease(pdu);

  return connection.OnControlProtocolError(H323Connection::e_LogicalChannel,
                                           "Request Close Release unknown");
}


H323ChannelNumber H245NegLogicalChannels::GetNextChannelNumber()
{
  lastChannelNumber++;
  return lastChannelNumber;
}


H323Channel * H245NegLogicalChannels::FindChannel(unsigned channelNumber,
                                                  PBoolean fromRemote)
{
  H323ChannelNumber chanNum(channelNumber, fromRemote);

  if (channels.Contains(chanNum))
    return channels[chanNum].GetChannel();

  return NULL;
}


H245NegLogicalChannel * H245NegLogicalChannels::FindNegLogicalChannel(unsigned channelNumber,
                                                                      PBoolean fromRemote)
{
  H323ChannelNumber chanNum(channelNumber, fromRemote);
  return channels.GetAt(chanNum);
}


H323Channel * H245NegLogicalChannels::FindChannelBySession(unsigned rtpSessionId,
                                                           PBoolean fromRemote)
{
  H323Channel::Directions desiredDirection = fromRemote ? H323Channel::IsReceiver : H323Channel::IsTransmitter;
  for (H245LogicalChannelDict::iterator it = channels.begin(); it != channels.end(); ++it) {
    H245NegLogicalChannel & logChan = it->second;
    if (logChan.IsAwaitingEstablishment() || logChan.IsEstablished()) {
      H323Channel * channel = logChan.GetChannel();
      if (channel != NULL && (rtpSessionId == 0 || channel->GetSessionID() == rtpSessionId) &&
                                                   channel->GetDirection() == desiredDirection)
        return channel;
    }
  }

  return NULL;
}


void H245NegLogicalChannels::RemoveAll()
{
  for (H245LogicalChannelDict::iterator it = channels.begin(); it != channels.end(); ++it) {
    H245NegLogicalChannel & neg = it->second;
    H323Channel * channel = neg.GetChannel();
    if (channel != NULL)
      channel->Close();
  }

  channels.RemoveAll();
}


/////////////////////////////////////////////////////////////////////////////

H245NegRequestMode::H245NegRequestMode(H323EndPoint & end, H323Connection & conn)
  : H245Negotiator(end, conn)
{
  awaitingResponse = false;
  inSequenceNumber = UINT_MAX;
  outSequenceNumber = 0;
}


PBoolean H245NegRequestMode::StartRequest(const PString & newModes)
{
  PStringArray modes = newModes.Lines();
  if (modes.IsEmpty()) {
    PTRACE(2, "H245\tNo new mode to request");
    return false;
  }

  H245_ArrayOf_ModeDescription descriptions;
  PINDEX modeCount = 0;

  const H323Capabilities & localCapabilities = connection.GetLocalCapabilities();

  for (PINDEX i = 0; i < modes.GetSize(); i++) {
    H245_ModeDescription description;
    PINDEX count = 0;

    PStringArray caps = modes[i].Tokenise('\t');
    for (PINDEX j = 0; j < caps.GetSize(); j++) {
      H323Capability * capability = localCapabilities.FindCapability(caps[j]);
      if (capability != NULL) {
        description.SetSize(count+1);
        capability->OnSendingPDU(description[count]);
        count++;
      }
    }

    if (count > 0) {
      descriptions.SetSize(modeCount+1);
      descriptions[modeCount] = description;
      modeCount++;
    }
  }

  if (modeCount == 0) {
    PTRACE(2, "H245\tUnsupported new mode to request");
    return false;
  }

  return StartRequest(descriptions);
}


PBoolean H245NegRequestMode::StartRequest(const H245_ArrayOf_ModeDescription & newModes)
{
  PTRACE(3, "H245\tStarted request mode: outSeq=" << outSequenceNumber
         << (awaitingResponse ? " awaitingResponse" : " idle"));

  if (awaitingResponse) {
    PTRACE(2, "H245\tAwaiting response to previous mode request");
    return false;
  }

  // Initiate a mode request
  outSequenceNumber = (outSequenceNumber+1)%256;
  replyTimer = endpoint.GetRequestModeTimeout();
  awaitingResponse = true;

  H323ControlPDU pdu;
  H245_RequestMode & requestMode = pdu.BuildRequestMode(outSequenceNumber);
  requestMode.m_requestedModes = newModes;
  requestMode.m_requestedModes.SetConstraints(PASN_Object::FixedConstraint, 1, 256);

  return connection.WriteControlPDU(pdu);
}


PBoolean H245NegRequestMode::HandleRequest(const H245_RequestMode & pdu)
{
  inSequenceNumber = pdu.m_sequenceNumber;

  PTRACE(3, "H245\tReceived request mode: inSeq=" << inSequenceNumber);

  H323ControlPDU reply_ack;
  H245_RequestModeAck & ack = reply_ack.BuildRequestModeAck(inSequenceNumber,
                  H245_RequestModeAck_response::e_willTransmitMostPreferredMode);

  H323ControlPDU reply_reject;
  H245_RequestModeReject & reject = reply_reject.BuildRequestModeReject(inSequenceNumber,
                                        H245_RequestModeReject_cause::e_modeUnavailable);

  PINDEX selectedMode = 0;
  if (!connection.OnRequestModeChange(pdu, ack, reject, selectedMode))
    return connection.WriteControlPDU(reply_reject);

  if (selectedMode != 0)
    ack.m_response.SetTag(H245_RequestModeAck_response::e_willTransmitLessPreferredMode);

  if (!connection.WriteControlPDU(reply_ack))
    return false;

  connection.OnModeChanged(pdu.m_requestedModes[selectedMode]);
  return true;
}


PBoolean H245NegRequestMode::HandleAck(const H245_RequestModeAck & pdu)
{
  PTRACE(3, "H245\tReceived ack on request mode: outSeq=" << outSequenceNumber
         << (awaitingResponse ? " awaitingResponse" : " idle"));

  if (awaitingResponse && pdu.m_sequenceNumber == outSequenceNumber) {
    awaitingResponse = false;
    replyTimer.Stop(false);
    connection.OnAcceptModeChange(pdu);
  }

  return true;
}

PBoolean H245NegRequestMode::HandleReject(const H245_RequestModeReject & pdu)
{
  PTRACE(3, "H245\tReceived reject on request mode: outSeq=" << outSequenceNumber
         << (awaitingResponse ? " awaitingResponse" : " idle"));

  if (awaitingResponse && pdu.m_sequenceNumber == outSequenceNumber) {
    awaitingResponse = false;
    replyTimer.Stop(false);
    connection.OnRefusedModeChange(&pdu);
  }

  return true;
}


PBoolean H245NegRequestMode::HandleRelease(const H245_RequestModeRelease & /*pdu*/)
{
  PTRACE(3, "H245\tReceived release on request mode: inSeq=" << inSequenceNumber);
  return true;
}


void H245NegRequestMode::HandleTimeout()
{
  PTRACE(3, "H245\tTimeout on request mode: outSeq=" << outSequenceNumber
         << (awaitingResponse ? " awaitingResponse" : " idle"));

  if (awaitingResponse) {
    awaitingResponse = false;
    H323ControlPDU pdu;
    pdu.Build(H245_IndicationMessage::e_requestModeRelease);
    connection.WriteControlPDU(pdu);
    connection.OnRefusedModeChange(NULL);
    connection.OnControlProtocolError(H323Connection::e_ModeRequest, "Timeout");
  }
}


/////////////////////////////////////////////////////////////////////////////

H245NegRoundTripDelay::H245NegRoundTripDelay(H323EndPoint & end, H323Connection & conn)
  : H245Negotiator(end, conn)
{
  awaitingResponse = false;
  sequenceNumber = 0;

  // Temporary (ie quick) fix for strange Cisco behaviour. If keep trying to
  // do this it stops sending RTP audio data!!
  retryCount = 1;
}


PBoolean H245NegRoundTripDelay::StartRequest()
{
  replyTimer = endpoint.GetRoundTripDelayTimeout();
  sequenceNumber = (sequenceNumber + 1)%256;
  awaitingResponse = true;

  PTRACE(3, "H245\tStarted round trip delay: seq=" << sequenceNumber
         << (awaitingResponse ? " awaitingResponse" : " idle"));

  H323ControlPDU pdu;
  pdu.BuildRoundTripDelayRequest(sequenceNumber);
  if (!connection.WriteControlPDU(pdu))
    return false;

  tripStartTime = PTimer::Tick();
  return true;
}


PBoolean H245NegRoundTripDelay::HandleRequest(const H245_RoundTripDelayRequest & pdu)
{
  PTRACE(3, "H245\tStarted round trip delay: seq=" << sequenceNumber
         << (awaitingResponse ? " awaitingResponse" : " idle"));

  H323ControlPDU reply;
  reply.BuildRoundTripDelayResponse(pdu.m_sequenceNumber);
  return connection.WriteControlPDU(reply);
}


PBoolean H245NegRoundTripDelay::HandleResponse(const H245_RoundTripDelayResponse & pdu)
{
  PTimeInterval tripEndTime = PTimer::Tick();

  PTRACE(3, "H245\tHandling round trip delay: seq=" << sequenceNumber
         << (awaitingResponse ? " awaitingResponse" : " idle"));

  if (awaitingResponse && pdu.m_sequenceNumber == sequenceNumber) {
    replyTimer.Stop(false);
    awaitingResponse = false;
    roundTripTime = tripEndTime - tripStartTime;
    retryCount = 3;
  }

  return true;
}


void H245NegRoundTripDelay::HandleTimeout()
{
  PTRACE(3, "H245\tTimeout on round trip delay: seq=" << sequenceNumber
         << (awaitingResponse ? " awaitingResponse" : " idle"));

  if (awaitingResponse && retryCount > 0)
    retryCount--;
  awaitingResponse = false;

  connection.OnControlProtocolError(H323Connection::e_RoundTripDelay, "Timeout");
}


#endif // OPAL_H323

/////////////////////////////////////////////////////////////////////////////
