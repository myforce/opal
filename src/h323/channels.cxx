/*
 * channels.cxx
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
#pragma implementation "channels.h"
#endif

#include <h323/channels.h>

#include <opal/transports.h>

#if OPAL_VIDEO
#include <codec/vidcodec.h>
#endif

#include <h323/h323ep.h>
#include <h323/h323con.h>
#include <h323/h323rtp.h>
#include <h323/transaddr.h>
#include <h323/h323pdu.h>


#define new PNEW


#define MAX_PAYLOAD_TYPE_MISMATCHES 8
#define RTP_TRACE_DISPLAY_RATE 16000 // 2 seconds


#if PTRACING

ostream & operator<<(ostream & out, H323Channel::Directions dir)
{
  static const char * const DirNames[H323Channel::NumDirections] = {
    "IsBidirectional", "IsTransmitter", "IsReceiver"
  };

  if (dir < H323Channel::NumDirections && DirNames[dir] != NULL)
    out << DirNames[dir];
  else
    out << "Direction<" << (unsigned)dir << '>';

  return out;
}

#endif


/////////////////////////////////////////////////////////////////////////////

H323ChannelNumber::H323ChannelNumber(unsigned num, PBoolean fromRem)
{
  PAssert(num < 0x10000, PInvalidParameter);
  number = num;
  fromRemote = fromRem;
}


PObject * H323ChannelNumber::Clone() const
{
  return new H323ChannelNumber(number, fromRemote);
}


PINDEX H323ChannelNumber::HashFunction() const
{
  PINDEX hash = (number%17) << 1;
  if (fromRemote)
    hash++;
  return hash;
}


void H323ChannelNumber::PrintOn(ostream & strm) const
{
  strm << (fromRemote ? 'R' : 'T') << '-' << number;
}


PObject::Comparison H323ChannelNumber::Compare(const PObject & obj) const
{
#ifndef PASN_LEANANDMEAN
  PAssert(PIsDescendant(&obj, H323ChannelNumber), PInvalidCast);
#endif
  const H323ChannelNumber & other = (const H323ChannelNumber &)obj;
  if (number < other.number)
    return LessThan;
  if (number > other.number)
    return GreaterThan;
  if (fromRemote && !other.fromRemote)
    return LessThan;
  if (!fromRemote && other.fromRemote)
    return GreaterThan;
  return EqualTo;
}


H323ChannelNumber & H323ChannelNumber::operator++(int)
{
  number++;
  return *this;
}


/////////////////////////////////////////////////////////////////////////////

H323Channel::H323Channel(H323Connection & conn, const H323Capability & cap)
  : endpoint(conn.GetEndPoint())
  , connection(conn)
  , capability((H323Capability *)cap.Clone())
  , opened(false)
  , m_bandwidthUsed(0)
{
  PTRACE_CONTEXT_ID_FROM(conn);
  PTRACE_CONTEXT_ID_TO(capability);
}


H323Channel::~H323Channel()
{
  delete capability;
}


void H323Channel::PrintOn(ostream & strm) const
{
  strm << number;
}


unsigned H323Channel::GetSessionID() const
{
  return 0;
}


bool H323Channel::SetSessionID(unsigned /*sessionID*/)
{
  return true;
}


PBoolean H323Channel::GetMediaTransportAddress(OpalTransportAddress & /*data*/,
                                               OpalTransportAddress & /*control*/) const
{
  return false;
}


void H323Channel::Close()
{
  if (m_terminating++ == 0)
    InternalClose();
}


void H323Channel::InternalClose()
{
  if (opened) {
    // Signal to the connection that this channel is on the way out
    connection.OnClosedLogicalChannel(*this);

    connection.SetBandwidthUsed(GetDirection() == IsReceiver ? OpalBandwidth::Rx : OpalBandwidth::Tx, m_bandwidthUsed, 0);
  }
  
  PTRACE(4, "LogChan\tCleaned up " << number);
}


OpalMediaStreamPtr H323Channel::GetMediaStream() const
{
  return NULL;
}


void H323Channel::SetMediaStream(OpalMediaStreamPtr)
{
}


PBoolean H323Channel::OnReceivedPDU(const H245_OpenLogicalChannel & /*pdu*/,
                                unsigned & /*errorCode*/)
{
  return true;
}


PBoolean H323Channel::OnReceivedAckPDU(const H245_OpenLogicalChannelAck & /*pdu*/)
{
  return true;
}


void H323Channel::OnSendOpenAck(const H245_OpenLogicalChannel & /*pdu*/,
                                H245_OpenLogicalChannelAck & /* pdu*/) const
{
}


void H323Channel::OnFlowControl(long bitRateRestriction)
{
  PTRACE(3, "LogChan\tOnFlowControl: " << bitRateRestriction);

  OpalMediaStreamPtr stream = GetMediaStream();
  if (stream != NULL)
    stream->ExecuteCommand(OpalMediaFlowControl(bitRateRestriction*100));
}


void H323Channel::OnMiscellaneousCommand(const H245_MiscellaneousCommand_type & type)
{
  PTRACE(3, "LogChan\tOnMiscellaneousCommand: chan=" << number << ", type=" << type.GetTagName());
  OpalMediaStreamPtr mediaStream = GetMediaStream();
  if (mediaStream == NULL)
    return;

#if OPAL_VIDEO
  switch (type.GetTag())
  {
    case H245_MiscellaneousCommand_type::e_videoFreezePicture :
      mediaStream->ExecuteCommand(OpalVideoFreezePicture());
      break;

    case H245_MiscellaneousCommand_type::e_videoFastUpdatePicture:
      mediaStream->ExecuteCommand(OpalVideoUpdatePicture());
      break;

    case H245_MiscellaneousCommand_type::e_videoFastUpdateGOB :
    case H245_MiscellaneousCommand_type::e_videoFastUpdateMB :
      mediaStream->ExecuteCommand(OpalVideoPictureLoss());
      break;

    case H245_MiscellaneousCommand_type::e_videoTemporalSpatialTradeOff :
      mediaStream->ExecuteCommand(OpalTemporalSpatialTradeOff((const PASN_Integer &)type));
      break;

    default:
      break;
  }
#endif
}


void H323Channel::OnMiscellaneousIndication(const H245_MiscellaneousIndication_type & PTRACE_PARAM(type))
{
  PTRACE(3, "LogChan\tOnMiscellaneousIndication: chan=" << number
         << ", type=" << type.GetTagName());
}


void H323Channel::OnJitterIndication(DWORD PTRACE_PARAM(jitter),
                                     int   PTRACE_PARAM(skippedFrameCount),
                                     int   PTRACE_PARAM(additionalBuffer))
{
  PTRACE(3, "LogChan\tOnJitterIndication:"
            " jitter=" << jitter <<
            " skippedFrameCount=" << skippedFrameCount <<
            " additionalBuffer=" << additionalBuffer);
}


PBoolean H323Channel::SetBandwidthUsed(OpalBandwidth bandwidth)
{
  if (!connection.SetBandwidthUsed(GetDirection() == IsReceiver ? OpalBandwidth::Rx : OpalBandwidth::Tx, m_bandwidthUsed, bandwidth)) {
    m_bandwidthUsed = 0;
    PTRACE(2, "LogChan\tFailed bandwidth request=" << bandwidth << ", used=" << m_bandwidthUsed);
    return false;
  }

  PTRACE(3, "LogChan\tBandwidth requested=" << bandwidth << ", used=" << m_bandwidthUsed);
  m_bandwidthUsed = bandwidth;
  return true;
}


bool H323Channel::PreOpen()
{
  return Open();
}


PBoolean H323Channel::Open()
{
  if (opened)
    return true;

  // Give the connection (or endpoint) a chance to do something with
  // the opening of the codec.
  if (!connection.OnStartLogicalChannel(*this)) {
    PTRACE(1, "LogChan\t" << (GetDirection() == IsReceiver ? "Receive" : "Transmit")
           << " open failed (OnStartLogicalChannel fail)");
    return false;
  }

  opened = true;
  return true;
}


/////////////////////////////////////////////////////////////////////////////

H323UnidirectionalChannel::H323UnidirectionalChannel(H323Connection & conn,
                                                     const H323Capability & cap,
                                                     Directions direction)
  : H323Channel(conn, cap)
  , receiver(direction == IsReceiver)
  , m_mediaFormat(capability->GetMediaFormat())
{
}

H323UnidirectionalChannel::~H323UnidirectionalChannel()
{
}


H323Channel::Directions H323UnidirectionalChannel::GetDirection() const
{
  return receiver ? IsReceiver : IsTransmitter;
}


PBoolean H323UnidirectionalChannel::SetInitialBandwidth()
{
  OpalBandwidth bandwidth;
  if (GetDirection() == IsTransmitter) {
    bandwidth = capability->GetMediaFormat().GetOptionInteger(OpalMediaFormat::TargetBitRateOption());
    OpalBandwidth available = connection.GetBandwidthAvailable(OpalBandwidth::Tx);
    if (bandwidth > available)
      bandwidth = available;
  }
  if (bandwidth == 0)
    bandwidth = capability->GetMediaFormat().GetOptionInteger(OpalMediaFormat::MaxBitRateOption());
  return SetBandwidthUsed(bandwidth);
}


bool H323UnidirectionalChannel::PreOpen()
{
  if (m_mediaStream != NULL)
    return true;

  m_mediaStream = connection.CreateMediaStream(m_mediaFormat, GetSessionID(), receiver);

  OpalCall & call = connection.GetCall();
  OpalMediaType mediaType = m_mediaFormat.GetMediaType();

  if (GetDirection() == IsReceiver) {
    if (!call.OpenSourceMediaStreams(connection, mediaType, GetSessionID(), m_mediaFormat)) {
      PTRACE(1, "LogChan\tReceive OpenSourceMediaStreams failed");
      return false;
    }
  }
  else {
    PSafePtr<OpalConnection> otherConnection = call.GetOtherPartyConnection(connection);
    if (otherConnection == NULL) {
      PTRACE(1, "LogChan\tTransmit failed, no other connection");
      return false;
    }
    if (!call.OpenSourceMediaStreams(*otherConnection, mediaType, GetSessionID(), m_mediaFormat)) {
      PTRACE(1, "LogChan\tTransmit OpenSourceMediaStreams failed");
      return false;
    }
  }

  m_mediaFormat = m_mediaStream->GetMediaFormat();
  capability->UpdateMediaFormat(m_mediaFormat);
  return true;
}


PBoolean H323UnidirectionalChannel::Open()
{
  return opened || (PreOpen() && H323Channel::Open());
}


void H323UnidirectionalChannel::InternalClose()
{
  PTRACE(4, "H323RTP\tCleaning up media stream on " << number);

  // If we have source media stream close it
  if (m_mediaStream != NULL) {
    if (!m_mediaStream->Close())
      connection.RemoveMediaStream(*m_mediaStream);
    m_mediaStream.SetNULL();
  }

  H323Channel::InternalClose();
}


OpalMediaStreamPtr H323UnidirectionalChannel::GetMediaStream() const
{
  return m_mediaStream;
}


void H323UnidirectionalChannel::SetMediaStream(OpalMediaStreamPtr mediaStream)
{
  m_mediaStream = mediaStream;
  m_mediaFormat = m_mediaStream->GetMediaFormat();
}


/////////////////////////////////////////////////////////////////////////////

H323BidirectionalChannel::H323BidirectionalChannel(H323Connection & conn,
                                                   const H323Capability & cap)
  : H323Channel(conn, cap)
{
}


H323Channel::Directions H323BidirectionalChannel::GetDirection() const
{
  return IsBidirectional;
}


/////////////////////////////////////////////////////////////////////////////

H323_RealTimeChannel::H323_RealTimeChannel(H323Connection & connection,
                                           const H323Capability & capability,
                                           Directions direction)
  : H323UnidirectionalChannel(connection, capability, direction)
{
}


PBoolean H323_RealTimeChannel::OnSendingPDU(H245_OpenLogicalChannel & open) const
{
  PTRACE(3, "H323RTP\tOnSendingPDU");

  open.m_forwardLogicalChannelNumber = (unsigned)number;

  if (open.HasOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters)) {
    open.m_reverseLogicalChannelParameters.IncludeOptionalField(
            H245_OpenLogicalChannel_reverseLogicalChannelParameters::e_multiplexParameters);
    // Set the communications information for unicast IPv4
    open.m_reverseLogicalChannelParameters.m_multiplexParameters.SetTag(
                H245_OpenLogicalChannel_reverseLogicalChannelParameters_multiplexParameters
                    ::e_h2250LogicalChannelParameters);

    // NOTE Two callbacks?  This may be supposed to fit somewhere else
    connection.OnSendH245_OpenLogicalChannel(open, false);
    return OnSendingPDU(open.m_reverseLogicalChannelParameters.m_multiplexParameters);
  }
  else {
    // Set the communications information for unicast IPv4
    open.m_forwardLogicalChannelParameters.m_multiplexParameters.SetTag(
                H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters
                    ::e_h2250LogicalChannelParameters);

    if (OnSendingAltPDU(open.m_genericInformation))
        open.IncludeOptionalField(H245_OpenLogicalChannel::e_genericInformation);

    connection.OnSendH245_OpenLogicalChannel(open, true);
    return OnSendingPDU(open.m_forwardLogicalChannelParameters.m_multiplexParameters);
  }
}


PBoolean H323_RealTimeChannel::OnSendingPDU(H245_H2250LogicalChannelParameters & param) const
{
  // If we are master (or a standard ID) then we can set the session ID, otherwise zero
  unsigned sessionID = GetSessionID();
  if (connection.IsH245Master() || sessionID <= H323Capability::DefaultDataSessionID)
    param.m_sessionID = sessionID;

  return true;
}


void H323_RealTimeChannel::OnSendOpenAck(H245_H2250LogicalChannelAckParameters & /*param*/) const
{
}


void H323_RealTimeChannel::OnSendOpenAck(const H245_OpenLogicalChannel & open,
                                         H245_OpenLogicalChannelAck & ack) const
{
  PTRACE(3, "H323RTP\tOnSendOpenAck");

  // set forwardMultiplexAckParameters option
  ack.IncludeOptionalField(H245_OpenLogicalChannelAck::e_forwardMultiplexAckParameters);

  // select H225 choice
  ack.m_forwardMultiplexAckParameters.SetTag(
      H245_OpenLogicalChannelAck_forwardMultiplexAckParameters::e_h2250LogicalChannelAckParameters);

  // get H225 parms
  H245_H2250LogicalChannelAckParameters & param = ack.m_forwardMultiplexAckParameters;

  // set session ID
  param.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_sessionID);
  const H245_H2250LogicalChannelParameters & openparam =
                          open.m_forwardLogicalChannelParameters.m_multiplexParameters;
  unsigned sessionID = openparam.m_sessionID;
  param.m_sessionID = sessionID;

  OnSendOpenAck(param);

  PTRACE(3, "H323RTP\tSending open logical channel ACK: sessionID=" << sessionID);
}


PBoolean H323_RealTimeChannel::OnReceivedPDU(const H245_OpenLogicalChannel & open,
                                         unsigned & errorCode)
{
  if (receiver)
    number = H323ChannelNumber(open.m_forwardLogicalChannelNumber, true);

  PTRACE(3, "H323RTP\tOnReceivedPDU for channel: " << number);

  PBoolean reverse = open.HasOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters);
  const H245_DataType & dataType = reverse ? open.m_reverseLogicalChannelParameters.m_dataType
                                           : open.m_forwardLogicalChannelParameters.m_dataType;

  if (!capability->OnReceivedPDU(dataType, receiver)) {
    PTRACE(1, "H323RTP\tData type not supported");
    errorCode = H245_OpenLogicalChannelReject_cause::e_dataTypeNotSupported;
    return false;
  }

  if (reverse) {
    if (open.m_reverseLogicalChannelParameters.m_multiplexParameters.GetTag() ==
             H245_OpenLogicalChannel_reverseLogicalChannelParameters_multiplexParameters::e_h2250LogicalChannelParameters)
      return OnReceivedPDU(open.m_reverseLogicalChannelParameters.m_multiplexParameters, errorCode);
  }
  else {
    if (open.m_forwardLogicalChannelParameters.m_multiplexParameters.GetTag() ==
             H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters::e_h2250LogicalChannelParameters)
      return OnReceivedPDU(open.m_forwardLogicalChannelParameters.m_multiplexParameters, errorCode);
  }

  PTRACE(1, "H323RTP\tOnly H.225.0 multiplex supported");
  errorCode = H245_OpenLogicalChannelReject_cause::e_unsuitableReverseParameters;
  return false;
}


PBoolean H323_RealTimeChannel::OnReceivedPDU(const H245_H2250LogicalChannelParameters & param,
                                          unsigned & errorCode)
{
  unsigned sessionID = param.m_sessionID;

  if (connection.IsH245Master()) {
    if (sessionID == 0)
      return true;
  }
  else {
    if (sessionID != 0)
      SetSessionID(sessionID);
  }

  if (sessionID != GetSessionID()) {
    PTRACE(1, "H323RTP\tOpen of " << *this << " with invalid session: " << param.m_sessionID);
    errorCode = H245_OpenLogicalChannelReject_cause::e_invalidSessionID;
    return false;
  }

  return true;
}


PBoolean H323_RealTimeChannel::OnReceivedAckPDU(const H245_OpenLogicalChannelAck & ack)
{
  PTRACE(3, "H323RTP\tOnReceiveOpenAck");

  if (!ack.HasOptionalField(H245_OpenLogicalChannelAck::e_forwardMultiplexAckParameters)) {
    PTRACE(1, "H323RTP\tNo forwardMultiplexAckParameters");
    return false;
  }

  if (ack.m_forwardMultiplexAckParameters.GetTag() !=
            H245_OpenLogicalChannelAck_forwardMultiplexAckParameters::e_h2250LogicalChannelAckParameters) {
    PTRACE(1, "H323RTP\tOnly H.225.0 multiplex supported");
    return false;
  }

  if (ack.HasOptionalField(H245_OpenLogicalChannel::e_genericInformation))
    OnReceivedAckAltPDU(ack.m_genericInformation);

  return OnReceivedAckPDU(ack.m_forwardMultiplexAckParameters);
}


PBoolean H323_RealTimeChannel::OnReceivedAckPDU(const H245_H2250LogicalChannelAckParameters & param)
{
  unsigned sessionID = 0;
  if (param.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_sessionID))
    sessionID = param.m_sessionID;

  if (connection.IsH245Master() || sessionID <= H323Capability::DefaultDataSessionID) {
    PTRACE_IF(2, sessionID != 0 && sessionID != GetSessionID(),
              "LogChan\tAck contains invalid session ID " << param.m_sessionID << ", ignoring");
    return true;
  }

  if (sessionID == 0) {
    PTRACE(2, "LogChan\tNo session ID in ACK from master!");
    return false;
  }

  if (param.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_dynamicRTPPayloadType)) {
    m_mediaFormat.SetPayloadType((RTP_DataFrame::PayloadTypes)param.m_dynamicRTPPayloadType.GetValue());
    if (m_mediaStream != NULL)
      m_mediaStream->UpdateMediaFormat(m_mediaFormat);
  }

  return SetSessionID(sessionID);
}


RTP_DataFrame::PayloadTypes H323_RealTimeChannel::GetDynamicRTPPayloadType() const
{
  OpalMediaFormat mediaFormat = m_mediaStream != NULL ? m_mediaStream->GetMediaFormat()
                                                      : capability->GetMediaFormat();
  return mediaFormat.GetPayloadType();
}


/////////////////////////////////////////////////////////////////////////////

H323DataChannel::H323DataChannel(H323Connection & conn,
                                 const H323Capability & cap,
                                 Directions dir,
                                 unsigned id)
  : H323UnidirectionalChannel(conn, cap, dir)
{
  sessionID = id;
  listener = NULL;
  autoDeleteListener = true;
  transport = NULL;
  autoDeleteTransport = true;
  separateReverseChannel = false;
}


H323DataChannel::~H323DataChannel()
{
  if (autoDeleteListener)
    delete listener;
  if (autoDeleteTransport)
    delete transport;
}


void H323DataChannel::InternalClose()
{
  PTRACE(4, "LogChan\tCleaning up data channel " << number);

  // Break any I/O blocks and wait for the thread that uses this object to
  // terminate before we allow it to be deleted.
  if (listener != NULL)
    listener->Close();
  if (transport != NULL)
    transport->Close();

  H323UnidirectionalChannel::InternalClose();
}


unsigned H323DataChannel::GetSessionID() const
{
  return sessionID;
}


PBoolean H323DataChannel::OnSendingPDU(H245_OpenLogicalChannel & open) const
{
  PTRACE(3, "LogChan\tOnSendingPDU for channel: " << number);

  open.m_forwardLogicalChannelNumber = (unsigned)number;

  open.m_forwardLogicalChannelParameters.m_multiplexParameters.SetTag(
              H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters
                  ::e_h2250LogicalChannelParameters);
  H245_H2250LogicalChannelParameters & fparam = open.m_forwardLogicalChannelParameters.m_multiplexParameters;
  fparam.m_sessionID = GetSessionID();

  if (separateReverseChannel)
    return true;

  open.IncludeOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters);
  open.m_reverseLogicalChannelParameters.IncludeOptionalField(
              H245_OpenLogicalChannel_reverseLogicalChannelParameters::e_multiplexParameters);
  open.m_reverseLogicalChannelParameters.m_multiplexParameters.SetTag(
              H245_OpenLogicalChannel_reverseLogicalChannelParameters_multiplexParameters
                  ::e_h2250LogicalChannelParameters);
  H245_H2250LogicalChannelParameters & rparam = open.m_reverseLogicalChannelParameters.m_multiplexParameters;
  rparam.m_sessionID = GetSessionID();

  return capability->OnSendingPDU(open.m_reverseLogicalChannelParameters.m_dataType);
}


void H323DataChannel::OnSendOpenAck(const H245_OpenLogicalChannel & /*open*/,
                                    H245_OpenLogicalChannelAck & ack) const
{
  if (listener == NULL && transport == NULL) {
    PTRACE(2, "LogChan\tOnSendOpenAck without a listener or transport");
    return;
  }

  PTRACE(3, "LogChan\tOnSendOpenAck for channel: " << number);
  
  H245_H2250LogicalChannelAckParameters * param;

  if (separateReverseChannel) {
    ack.IncludeOptionalField(H245_OpenLogicalChannelAck::e_forwardMultiplexAckParameters);
    ack.m_forwardMultiplexAckParameters.SetTag(
              H245_OpenLogicalChannelAck_forwardMultiplexAckParameters::e_h2250LogicalChannelAckParameters);
    param = (H245_H2250LogicalChannelAckParameters*)&ack.m_forwardMultiplexAckParameters.GetObject();
  }
  else {
    ack.IncludeOptionalField(H245_OpenLogicalChannelAck::e_reverseLogicalChannelParameters);
    ack.m_reverseLogicalChannelParameters.m_multiplexParameters.SetTag(
              H245_OpenLogicalChannelAck_reverseLogicalChannelParameters_multiplexParameters
                  ::e_h2250LogicalChannelParameters);
    param = (H245_H2250LogicalChannelAckParameters*)
                &ack.m_reverseLogicalChannelParameters.m_multiplexParameters.GetObject();
  }

  H323TransportAddress address;
  param->IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaChannel);
  if (listener != NULL)
    address = listener->GetLocalAddress(connection.GetControlChannel().GetRemoteAddress());
  else
    address = transport->GetLocalAddress();

  address.SetPDU(param->m_mediaChannel);
}


PBoolean H323DataChannel::OnReceivedPDU(const H245_OpenLogicalChannel & open,
                                    unsigned & errorCode)
{
  number = H323ChannelNumber(open.m_forwardLogicalChannelNumber, true);

  PTRACE(3, "LogChan\tOnReceivedPDU for data channel: " << number);

  if (!CreateListener()) {
    PTRACE(1, "LogChan\tCould not create listener");
    errorCode = H245_OpenLogicalChannelReject_cause::e_unspecified;
    return false;
  }

  if (separateReverseChannel &&
      open.HasOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters)) {
    errorCode = H245_OpenLogicalChannelReject_cause::e_unsuitableReverseParameters;
    PTRACE(1, "LogChan\tOnReceivedPDU has unexpected reverse parameters");
    return false;
  }

  if (!capability->OnReceivedPDU(open.m_forwardLogicalChannelParameters.m_dataType, receiver)) {
    PTRACE(1, "H323RTP\tData type not supported");
    errorCode = H245_OpenLogicalChannelReject_cause::e_dataTypeNotSupported;
    return false;
  }

  return true;
}


PBoolean H323DataChannel::OnReceivedAckPDU(const H245_OpenLogicalChannelAck & ack)
{
  PTRACE(3, "LogChan\tOnReceivedAckPDU");

  const H245_TransportAddress * address;

  if (separateReverseChannel) {
      PTRACE(3, "LogChan\tseparateReverseChannels");
    if (!ack.HasOptionalField(H245_OpenLogicalChannelAck::e_forwardMultiplexAckParameters)) {
      PTRACE(1, "LogChan\tNo forwardMultiplexAckParameters");
      return false;
    }

    if (ack.m_forwardMultiplexAckParameters.GetTag() !=
              H245_OpenLogicalChannelAck_forwardMultiplexAckParameters::e_h2250LogicalChannelAckParameters) {
      PTRACE(1, "LogChan\tOnly H.225.0 multiplex supported");
      return false;
    }

    const H245_H2250LogicalChannelAckParameters & param = ack.m_forwardMultiplexAckParameters;

    if (!param.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaChannel)) {
      PTRACE(1, "LogChan\tNo media channel address provided");
      return false;
    }

    address = &param.m_mediaChannel;

    if (ack.HasOptionalField(H245_OpenLogicalChannelAck::e_reverseLogicalChannelParameters)) {
      PTRACE(3, "LogChan\treverseLogicalChannelParameters set");
      reverseChannel = H323ChannelNumber(ack.m_reverseLogicalChannelParameters.m_reverseLogicalChannelNumber, true);
    }
  }
  else {
    if (!ack.HasOptionalField(H245_OpenLogicalChannelAck::e_reverseLogicalChannelParameters)) {
      PTRACE(1, "LogChan\tNo reverseLogicalChannelParameters");
      return false;
    }

    if (ack.m_reverseLogicalChannelParameters.m_multiplexParameters.GetTag() !=
              H245_OpenLogicalChannelAck_reverseLogicalChannelParameters_multiplexParameters
                              ::e_h2250LogicalChannelParameters) {
      PTRACE(1, "LogChan\tOnly H.225.0 multiplex supported");
      return false;
    }

    const H245_H2250LogicalChannelParameters & param = ack.m_reverseLogicalChannelParameters.m_multiplexParameters;

    if (!param.HasOptionalField(H245_H2250LogicalChannelParameters::e_mediaChannel)) {
      PTRACE(1, "LogChan\tNo media channel address provided");
      return false;
    }

    address = &param.m_mediaChannel;
  }

  if (!CreateTransport()) {
    PTRACE(1, "LogChan\tCould not create transport");
    return false;
  }

  if (!transport->ConnectTo(H323TransportAddress(*address))) {
    PTRACE(1, "LogChan\tCould not connect to remote transport address: " << *address);
    return false;
  }

  return true;
}


PBoolean H323DataChannel::CreateListener()
{
  if (listener == NULL) {
    listener = connection.GetControlChannel().GetLocalAddress().CreateListener(
                          connection.GetEndPoint(), OpalTransportAddress::HostOnly);
    if (listener == NULL)
      return false;

    PTRACE(3, "LogChan\tCreated listener for data channel: " << *listener);
  }

  return listener->Open(NULL);
}


PBoolean H323DataChannel::CreateTransport()
{
  if (transport == NULL) {
    transport = connection.GetControlChannel().GetLocalAddress().CreateTransport(
                          connection.GetEndPoint(), OpalTransportAddress::HostOnly);
    if (transport == NULL)
      return false;

    PTRACE(3, "LogChan\tCreated transport for data channel: " << *transport);
  }

  return transport != NULL;
}


#endif // OPAL_H323

/////////////////////////////////////////////////////////////////////////////
