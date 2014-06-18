/*
 * rtp_session.cxx
 *
 * RTP protocol session
 *
 * Copyright (c) 2012 Equivalence Pty. Ltd.
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

#ifdef __GNUC__
#pragma implementation "rtp_session.h"
#endif

#include <opal_config.h>

#include <rtp/rtp_session.h>

#include <opal/endpoint.h>
#include <rtp/rtpep.h>
#include <rtp/rtpconn.h>
#include <rtp/jitter.h>
#include <rtp/metrics.h>

#include <ptclib/random.h>
#include <ptclib/pnat.h>
#include <ptclib/cypher.h>
#include <ptclib/pstunsrvr.h>

#include <algorithm>

#include <h323/h323con.h>

const unsigned SecondsFrom1900to1970 = (70*365+17)*24*60*60U;

#define RTP_VIDEO_RX_BUFFER_SIZE 0x100000 // 1Mb
#define RTP_AUDIO_RX_BUFFER_SIZE 0x4000   // 16kb
#define RTP_DATA_TX_BUFFER_SIZE  0x2000   // 8kb
#define RTP_CTRL_BUFFER_SIZE     0x1000   // 4kb


#define PTraceModule() "RTP"


/////////////////////////////////////////////////////////////////////////////
/**A descendant of the OpalJitterBuffer that reads RTP_DataFrame instances
   from the OpalRTPSession
  */
class RTP_JitterBuffer : public OpalJitterBufferThread
{
    PCLASSINFO(RTP_JitterBuffer, OpalJitterBufferThread);
  public:
    RTP_JitterBuffer(OpalRTPSession & session, const Init & init)
      : OpalJitterBufferThread(init)
      , m_session(session)
    {
      PTRACE_CONTEXT_ID_FROM(session);
    }


    ~RTP_JitterBuffer()
    {
      PTRACE(4, "Jitter", "Destroying jitter buffer " << *this);

      m_running = false;
      bool reopen = m_session.Shutdown(true);

      WaitForThreadTermination();

      if (reopen)
        m_session.Restart(true);
    }


    virtual PBoolean OnReadPacket(RTP_DataFrame & frame)
    {
      if (!m_session.InternalReadData(frame))
        return false;

#if OPAL_RTCP_XR
      RTCP_XR_Metrics * metrics = m_session.GetExtendedMetrics();
      if (metrics != NULL)
        metrics->SetJitterDelay(GetCurrentJitterDelay()/GetTimeUnits());
#endif

      PTRACE(6, "Jitter", "OnReadPacket: Frame from network, timestamp " << frame.GetTimestamp());
      return true;
   }

 protected:
   /**This class extracts data from the outside world by reading from this session variable */
   OpalRTPSession & m_session;
};


#define new PNEW

/////////////////////////////////////////////////////////////////////////////

const PCaselessString & OpalRTPSession::RTP_AVP () { static const PConstCaselessString s("RTP/AVP" ); return s; }
const PCaselessString & OpalRTPSession::RTP_AVPF() { static const PConstCaselessString s("RTP/AVPF"); return s; }

PFACTORY_CREATE(OpalMediaSessionFactory, OpalRTPSession, OpalRTPSession::RTP_AVP());
static bool RegisteredAVPF = OpalMediaSessionFactory::RegisterAs(OpalRTPSession::RTP_AVPF(), OpalRTPSession::RTP_AVP());

#if P_CONFIG_FILE
static PTimeInterval GetDefaultOutOfOrderWaitTime()
{
  static PTimeInterval ooowt(PConfig(PConfig::Environment).GetInteger("OPAL_RTP_OUT_OF_ORDER_TIME", 100));
  return ooowt;
}
#else
#define GetDefaultOutOfOrderWaitTime() (100)
#endif

OpalRTPSession::OpalRTPSession(const Init & init)
  : OpalMediaSession(init)
  , m_endpoint(dynamic_cast<OpalRTPEndPoint &>(init.m_connection.GetEndPoint()))
  , m_singlePortRx(false)
  , m_isAudio(init.m_mediaType == OpalMediaType::Audio())
  , m_timeUnits(m_isAudio ? 8 : 90)
  , m_toolName(PProcess::Current().GetName())
  , m_maxNoReceiveTime(init.m_connection.GetEndPoint().GetManager().GetNoMediaTimeout())
  , m_maxNoTransmitTime(0, 10)          // Sending data for 10 seconds, ICMP says still not there
#if OPAL_RTP_FEC
  , m_redundencyPayloadType(RTP_DataFrame::IllegalPayloadType)
  , m_ulpFecPayloadType(RTP_DataFrame::IllegalPayloadType)
  , m_ulpFecSendLevel(2)
#endif
  , syncSourceOut(PRandom::Number())
  , syncSourceIn(0)
  , lastSentTimestamp(0)  // should be calculated, but we'll settle for initialising it)
  , allowAnySyncSource(true)
  , allowOneSyncSourceChange(false)
  , allowSequenceChange(false)
  , txStatisticsInterval(100)
  , rxStatisticsInterval(100)
  , lastSentSequenceNumber((WORD)PRandom::Number())
  , expectedSequenceNumber(0)
  , lastSentPacketTime(PTimer::Tick())
  , lastSRTimestamp(0)
  , lastSRReceiveTime(0)
  , lastRRSequenceNumber(0)
  , resequenceOutOfOrderPackets(true)
  , consecutiveOutOfOrderPackets(0)
  , outOfOrderWaitTime(GetDefaultOutOfOrderWaitTime())
  , m_lastFIRSequenceNumber(0)
  , m_lastTSTOSequenceNumber(0)
  , firstPacketSent(0)
  , firstPacketReceived(0)
  , senderReportsReceived(0)
#if OPAL_RTCP_XR
  , m_metrics(NULL)
#endif
  , lastReceivedPayloadType(RTP_DataFrame::IllegalPayloadType)
  , ignorePayloadTypeChanges(true)
  , m_reportTimer(0, 12)  // Seconds
  , m_closeOnBye(false)
  , m_byeSent(false)
  , m_localAddress(PIPSocket::GetInvalidAddress())
  , m_remoteAddress(PIPSocket::GetInvalidAddress())
  , m_shutdownRead(false)
  , m_shutdownWrite(false)
  , m_remoteBehindNAT(init.m_remoteBehindNAT)
  , m_localHasRestrictedNAT(false)
  , m_noTransmitErrors(0)
#if OPAL_ICE
  , m_stunServer(NULL)
  , m_stunClient(NULL)
#endif
#if PTRACING
  , m_levelTxRR(3)
  , m_levelRxSR(3)
  , m_levelRxRR(3)
  , m_levelRxSDES(3)
  , m_levelRxUnknownFEC(3)
#endif
{
  m_localAddress = PIPSocket::GetInvalidAddress();
  m_localPort[e_Data] = m_localPort[e_Control] = 0;
  m_remoteAddress = PIPSocket::GetInvalidAddress();
  m_remotePort[e_Data] = m_remotePort[e_Control] = 0;
  m_socket[e_Data] = m_socket[e_Control] = NULL;

  ClearStatistics();

  /* CNAME is no longer just a username @ host, security!
     But RFC 6222 hopelessly complicated, while not exactly the same, just
     using the right most 12 bytes of GUID is very similar. It will do. */
  PGloballyUniqueID guid;
  m_canonicalName = PBase64::Encode(&guid[PGloballyUniqueID::Size-12], 12);

  PTRACE_CONTEXT_ID_TO(m_reportTimer);
  m_reportTimer.SetNotifier(PCREATE_NOTIFIER(TimedSendReport));
}


OpalRTPSession::OpalRTPSession(const OpalRTPSession & other)
  : OpalMediaSession(Init(other.m_connection, 0, OpalMediaType(), false))
  , m_endpoint(other.m_endpoint)
{
}


OpalRTPSession::~OpalRTPSession()
{
  Close();

#if OPAL_RTCP_XR
  delete m_metrics;
#endif

#if OPAL_ICE
  delete m_stunServer;
  delete m_stunClient;
#endif

#if PTRACING
  PTime now;
  int sentDuration = (now-firstPacketSent).GetSeconds();
  if (sentDuration == 0)
    sentDuration = 1;
  int receiveDuration = (now-firstPacketReceived).GetSeconds();
  if (receiveDuration == 0)
    receiveDuration = 1;
 #endif
  PTRACE_IF(3, packetsSent != 0 || packetsReceived != 0,
      "Session " << m_sessionId << ", final statistics:\n"
      "    firstPacketSent    = " << firstPacketSent << "\n"
      "    packetsSent        = " << packetsSent << "\n"
      "    octetsSent         = " << octetsSent << "\n"
      "    bitRateSent        = " << (8*octetsSent/sentDuration) << "\n"
      "    averageSendTime    = " << averageSendTime << "\n"
      "    maximumSendTime    = " << maximumSendTime << "\n"
      "    minimumSendTime    = " << minimumSendTime << "\n"
      "    packetsLostByRemote= " << packetsLostByRemote << "\n"
      "    jitterLevelOnRemote= " << jitterLevelOnRemote << "\n"
      "    firstPacketReceived= " << firstPacketReceived << "\n"
      "    packetsReceived    = " << packetsReceived << "\n"
      "    octetsReceived     = " << octetsReceived << "\n"
      "    bitRateReceived    = " << (8*octetsReceived/receiveDuration) << "\n"
      "    packetsLost        = " << packetsLost << "\n"
      "    packetsTooLate     = " << GetPacketsTooLate() << "\n"
      "    packetOverruns     = " << GetPacketOverruns() << "\n"
      "    packetsOutOfOrder  = " << packetsOutOfOrder << "\n"
      "    averageReceiveTime = " << averageReceiveTime << "\n"
      "    maximumReceiveTime = " << maximumReceiveTime << "\n"
      "    minimumReceiveTime = " << minimumReceiveTime << "\n"
      "    averageJitter      = " << GetAvgJitterTime() << "\n"
      "    maximumJitter      = " << GetMaxJitterTime()
   );
}


void OpalRTPSession::ClearStatistics()
{
  firstPacketSent.SetTimestamp(0);
  packetsSent = 0;
  rtcpPacketsSent = 0;
  octetsSent = 0;
  firstPacketReceived.SetTimestamp(0);
  packetsReceived = 0;
  octetsReceived = 0;
  packetsLost = 0;
  packetsLostByRemote = 0;
  packetsOutOfOrder = 0;
  averageSendTime = 0;
  maximumSendTime = 0;
  minimumSendTime = 0;
  averageReceiveTime = 0;
  maximumReceiveTime = 0;
  minimumReceiveTime = 0;
  jitterLevel = 0;
  maximumJitterLevel = 0;
  jitterLevelOnRemote = 0;
  markerRecvCount = 0;
  markerSendCount = 0;

  txStatisticsCount = 0;
  rxStatisticsCount = 0;
  averageSendTimeAccum = 0;
  maximumSendTimeAccum = 0;
  minimumSendTimeAccum = 0xffffffff;
  averageReceiveTimeAccum = 0;
  maximumReceiveTimeAccum = 0;
  minimumReceiveTimeAccum = 0xffffffff;
  packetsLostSinceLastRR = 0;
  lastTransitTime = 0;
  m_lastReceivedStatisticTimestamp = 0;
}


void OpalRTPSession::AttachTransport(Transport & transport)
{
  transport.DisallowDeleteObjects();

  for (int i = 1; i >= 0; --i) {
    PObject * channel = transport.RemoveHead();
    m_socket[i] = dynamic_cast<PUDPSocket *>(channel);
    if (m_socket[i] == NULL)
      delete channel;
    else {
      PTRACE_CONTEXT_ID_TO(m_socket[i]);
      m_socket[i]->GetLocalAddress(m_localAddress, m_localPort[i]);
    }
  }

  m_endpoint.RegisterLocalRTP(this, false);
  transport.AllowDeleteObjects();
}


OpalMediaSession::Transport OpalRTPSession::DetachTransport()
{
  Transport temp;

  // Stop jitter buffer before detaching
  m_jitterBuffer.SetNULL();

  m_endpoint.RegisterLocalRTP(this, true);

  Shutdown(true);
  m_readMutex.Wait();
  m_dataMutex.Wait();

  for (int i = 1; i >= 0; --i) {
    if (m_socket[i] != NULL) {
      temp.Append(m_socket[i]);
      m_socket[i] = NULL;
    }
  }

  m_dataMutex.Signal();
  m_readMutex.Signal();

  PTRACE_IF(2, temp.IsEmpty(), "Detaching transport from closed session.");
  return temp;
}


void OpalRTPSession::SendBYE()
{
  if (!IsOpen())
    return;

  {
    PWaitAndSignal mutex(m_dataMutex);
    if (m_byeSent)
      return;

    m_byeSent = true;
  }

  RTP_ControlFrame report;
  InitialiseControlFrame(report);

  PTRACE(3, "Session " << m_sessionId << ", Sending BYE, SSRC=" << RTP_TRACE_SRC(syncSourceOut));

  static char const ReasonStr[] = "Session ended";
  static size_t ReasonLen = sizeof(ReasonStr);

  // insert BYE
  report.StartNewPacket();
  report.SetPayloadType(RTP_ControlFrame::e_Goodbye);
  report.SetPayloadSize(4+1+ReasonLen);  // length is SSRC + ReasonLen + reason

  BYTE * payload = report.GetPayloadPtr();

  // one SSRC
  report.SetCount(1);
  *(PUInt32b *)payload = syncSourceOut;

  // insert reason
  payload[4] = (BYTE)ReasonLen;
  memcpy((char *)(payload+5), ReasonStr, ReasonLen);

  report.EndPacket();
  WriteControl(report);
}

PString OpalRTPSession::GetCanonicalName() const
{
  PWaitAndSignal mutex(m_reportMutex);
  PString s = m_canonicalName;
  s.MakeUnique();
  return s;
}


void OpalRTPSession::SetCanonicalName(const PString & name)
{
  PWaitAndSignal mutex(m_reportMutex);
  m_canonicalName = name;
  m_canonicalName.MakeUnique();
}


PString OpalRTPSession::GetGroupId() const
{
  PWaitAndSignal mutex(m_reportMutex);
  PString s = m_groupId;
  s.MakeUnique();
  return s;
}


void OpalRTPSession::SetGroupId(const PString & id)
{
  PWaitAndSignal mutex(m_reportMutex);
  m_groupId = id;
  m_groupId.MakeUnique();
}


PString OpalRTPSession::GetToolName() const
{
  PWaitAndSignal mutex(m_reportMutex);
  PString s = m_toolName;
  s.MakeUnique();
  return s;
}


void OpalRTPSession::SetToolName(const PString & name)
{
  PWaitAndSignal mutex(m_reportMutex);
  m_toolName = name;
  m_toolName.MakeUnique();
}


RTPExtensionHeaders OpalRTPSession::GetExtensionHeaders() const
{
  PWaitAndSignal mutex(m_reportMutex);
  return m_extensionHeaders;
}


void OpalRTPSession::SetExtensionHeader(const RTPExtensionHeaders & ext)
{
  PWaitAndSignal mutex(m_reportMutex);
  m_extensionHeaders = ext;
}


bool OpalRTPSession::SetJitterBufferSize(const OpalJitterBuffer::Init & init)
{
  PWaitAndSignal mutex(m_dataMutex);

  if (!IsOpen())
    return false;

  if (init.m_timeUnits > 0)
    m_timeUnits = init.m_timeUnits;

  if (init.m_maxJitterDelay == 0) {
    PTRACE_IF(4, m_jitterBuffer != NULL, "Jitter", "Switching off jitter buffer " << *m_jitterBuffer);
    // This can block waiting for JB thread to end, signal mutex to avoid deadlock
    m_dataMutex.Signal();
    m_jitterBuffer.SetNULL();
    m_dataMutex.Wait();
    return false;
  }

  resequenceOutOfOrderPackets = false;

  if (m_jitterBuffer != NULL)
    m_jitterBuffer->SetDelay(init);
  else {
    m_jitterBuffer = new RTP_JitterBuffer(*this, init);
    PTRACE(4, "Jitter", "Created RTP jitter buffer " << *m_jitterBuffer);
  }

  return true;
}


unsigned OpalRTPSession::GetJitterBufferSize() const
{
  JitterBufferPtr jitter = m_jitterBuffer; // Increase reference count
  return jitter != NULL ? jitter->GetCurrentJitterDelay() : 0;
}


bool OpalRTPSession::ReadData(RTP_DataFrame & frame)
{
  if (!IsOpen() || m_shutdownRead)
    return false;

  JitterBufferPtr jitter = m_jitterBuffer; // Increase reference count
  if (jitter != NULL)
    return jitter->ReadData(frame);

  if (m_pendingPackets.empty())
    return InternalReadData(frame);

  unsigned sequenceNumber = m_pendingPackets.back().GetSequenceNumber();
  if (sequenceNumber != expectedSequenceNumber) {
    PTRACE(5, "Session " << m_sessionId << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn)
           << ", still out of order packets, next "
           << sequenceNumber << " expected " << expectedSequenceNumber);
    return InternalReadData(frame);
  }

  frame = m_pendingPackets.back();
  m_pendingPackets.pop_back();
  expectedSequenceNumber = (WORD)(sequenceNumber + 1);

  PTRACE(m_pendingPackets.empty() ? 2 : 5,
         "Session " << m_sessionId << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn) << ", resequenced "
         << (m_pendingPackets.empty() ? "last" : "next") << " out of order packet " << sequenceNumber);
  return true;
}


void OpalRTPSession::SetTxStatisticsInterval(unsigned packets)
{
  txStatisticsInterval = std::max(packets, 2U);
  txStatisticsCount = 0;
  averageSendTimeAccum = 0;
  maximumSendTimeAccum = 0;
  minimumSendTimeAccum = 0xffffffff;
}


void OpalRTPSession::SetRxStatisticsInterval(unsigned packets)
{
  rxStatisticsInterval = std::max(packets, 2U);
  rxStatisticsCount = 0;
  averageReceiveTimeAccum = 0;
  maximumReceiveTimeAccum = 0;
  minimumReceiveTimeAccum = 0xffffffff;
}


void OpalRTPSession::AddReceiverReport(RTP_ControlFrame::ReceiverReport & receiver)
{
  receiver.ssrc = syncSourceIn;
  receiver.SetLostPackets(GetPacketsLost()+GetPacketsTooLate());

  if (expectedSequenceNumber > lastRRSequenceNumber)
    receiver.fraction = (BYTE)((packetsLostSinceLastRR<<8)/(expectedSequenceNumber - lastRRSequenceNumber));
  else
    receiver.fraction = 0;
  packetsLostSinceLastRR = 0;

  receiver.last_seq = lastRRSequenceNumber;
  lastRRSequenceNumber = expectedSequenceNumber;

  receiver.jitter = jitterLevel >> JitterRoundingGuardBits; // Allow for rounding protection bits
  
  if (senderReportsReceived > 0) {
    // Calculate the last SR timestamp
    PUInt32b lsr_ntp_sec  = (DWORD)(lastSRTimestamp.GetTimeInSeconds()+SecondsFrom1900to1970); // Convert from 1970 to 1900
    PUInt32b lsr_ntp_frac = lastSRTimestamp.GetMicrosecond()*4294; // Scale microseconds to "fraction" from 0 to 2^32
    receiver.lsr = (((lsr_ntp_sec << 16) & 0xFFFF0000) | ((lsr_ntp_frac >> 16) & 0x0000FFFF));
  
    // Calculate the delay since last SR
    PTime now;
    delaySinceLastSR = now - lastSRReceiveTime;
    receiver.dlsr = (DWORD)(delaySinceLastSR.GetMilliSeconds()*65536/1000);
  }
  else {
    receiver.lsr = 0;
    receiver.dlsr = 0;
  }
  
  PTRACE(m_levelTxRR, "Session " << m_sessionId << ", Sending ReceiverReport:"
            " SSRC=" << RTP_TRACE_SRC(syncSourceIn)
         << " fraction=" << (unsigned)receiver.fraction
         << " lost=" << receiver.GetLostPackets()
         << " last_seq=" << receiver.last_seq
         << " jitter=" << receiver.jitter
         << " lsr=" << receiver.lsr
         << " dlsr=" << receiver.dlsr);
} 


OpalRTPSession::SendReceiveStatus OpalRTPSession::OnSendData(RTP_DataFrame & frame, bool rewriteHeader)
{
  PWaitAndSignal mutex(m_dataMutex);

  if (m_remotePort[e_Data] == 0) {
    PTRACE(5, "Session " << m_sessionId << ", ignoring sent data, no remote address yet.");
    return e_IgnorePacket;
  }

  if (rewriteHeader) {
    PTimeInterval tick = PTimer::Tick();  // Timestamp set now

    lastSentSequenceNumber += (WORD)(frame.GetDiscontinuity() + 1);
    PTRACE_IF(6, frame.GetDiscontinuity() > 0,
              "Have discontinuity: " << frame.GetDiscontinuity() << ", sn=" << lastSentSequenceNumber);
    frame.SetSequenceNumber(lastSentSequenceNumber);
    frame.SetSyncSource(syncSourceOut);

    // special handling for first packet
    if (!firstPacketSent.IsValid()) {
      firstPacketSent.SetCurrentTime();

      // display stuff
      PTRACE(3, "Session " << m_sessionId << ", first sent data:"
                " ver=" << frame.GetVersion()
             << " pt=" << frame.GetPayloadType()
             << " psz=" << frame.GetPayloadSize()
             << " m=" << frame.GetMarker()
             << " x=" << frame.GetExtension()
             << " seq=" << frame.GetSequenceNumber()
             << " ts=" << frame.GetTimestamp()
             << " src=" << RTP_TRACE_SRC(frame.GetSyncSource())
             << " ccnt=" << frame.GetContribSrcCount()
             << " rem=" << GetRemoteAddress()
             << " local=" << GetLocalAddress());
    }

    else {
      /* For audio we do not do statistics on start of talk burst as that
         could be a substantial time and is not useful, so we only calculate
         when the marker bit os off.

         For video we measure jitter between whole video frames which is
         indicated by the marker bit being on.
      */
      if (m_isAudio  != frame.GetMarker()) {
        DWORD diff = (tick - lastSentPacketTime).GetInterval();

        averageSendTimeAccum += diff;
        if (diff > maximumSendTimeAccum)
          maximumSendTimeAccum = diff;
        if (diff < minimumSendTimeAccum)
          minimumSendTimeAccum = diff;
        txStatisticsCount++;
      }
    }

    lastSentTimestamp = frame.GetTimestamp();
    lastSentPacketTime = tick;
  }

  octetsSent += frame.GetPayloadSize();
  packetsSent++;

  if (frame.GetMarker())
    markerSendCount++;

#if OPAL_RTP_FEC
  if (m_redundencyPayloadType != RTP_DataFrame::IllegalPayloadType) {
    SendReceiveStatus status = OnSendRedundantFrame(frame);
    if (status != e_ProcessPacket)
      return status;
  }
#endif

  if (txStatisticsCount < txStatisticsInterval)
    return e_ProcessPacket;

  txStatisticsCount = 0;

  averageSendTime = averageSendTimeAccum/txStatisticsInterval;
  maximumSendTime = maximumSendTimeAccum;
  minimumSendTime = minimumSendTimeAccum;

  averageSendTimeAccum = 0;
  maximumSendTimeAccum = 0;
  minimumSendTimeAccum = 0xffffffff;

  PTRACE(3, "Session " << m_sessionId << ", transmit statistics: "
            " packets=" << packetsSent <<
            " octets=" << octetsSent <<
            " avgTime=" << averageSendTime <<
            " maxTime=" << maximumSendTime <<
            " minTime=" << minimumSendTime);

  return e_ProcessPacket;
}

OpalRTPSession::SendReceiveStatus OpalRTPSession::OnSendControl(RTP_ControlFrame & frame)
{
  if (m_remotePort[e_Control] == 0) {
    PTRACE(5, "Session " << m_sessionId << ", ignoring sent control, no remote address yet.");
    return e_IgnorePacket;
  }

  frame.SetSize(frame.GetCompoundSize());
  return e_ProcessPacket;
}


OpalRTPSession::SendReceiveStatus OpalRTPSession::OnReceiveData(RTP_DataFrame & frame, PINDEX pduSize)
{
  // Check received PDU is big enough
  return frame.SetPacketSize(pduSize) ? OnReceiveData(frame) : e_IgnorePacket;
}


OpalRTPSession::SendReceiveStatus OpalRTPSession::OnReceiveData(RTP_DataFrame & frame)
{
  // Check that the PDU is the right version
  if (frame.GetVersion() != RTP_DataFrame::ProtocolVersion)
    return e_IgnorePacket; // Non fatal error, just ignore

  RTP_DataFrame::PayloadTypes pt = frame.GetPayloadType();
#if OPAL_RTP_FEC
  if (pt == m_redundencyPayloadType) {
    SendReceiveStatus status = OnReceiveRedundantFrame(frame);
    if (status != e_ProcessPacket)
      return status;
  }
#endif

  // Check if expected payload type
  if (lastReceivedPayloadType == RTP_DataFrame::IllegalPayloadType)
    lastReceivedPayloadType = pt;

  if (lastReceivedPayloadType != pt && !ignorePayloadTypeChanges) {
    PTRACE(4, "Session " << m_sessionId << ", got payload type "
           << pt << ", but was expecting " << lastReceivedPayloadType);
    return e_IgnorePacket;
  }

  // Check for if a control packet rather than data packet.
  if (pt > RTP_DataFrame::MaxPayloadType)
    return e_IgnorePacket; // Non fatal error, just ignore

  PTimeInterval tick = PTimer::Tick();  // Get timestamp now

  // Have not got SSRC yet, so grab it now
  if (syncSourceIn == 0)
    syncSourceIn = frame.GetSyncSource();

  // Check packet sequence numbers
  if (packetsReceived == 0) {
    firstPacketReceived.SetCurrentTime();
    PTRACE(3, "Session " << m_sessionId << ", first receive data:"
              " ver=" << frame.GetVersion()
           << " pt=" << pt
           << " psz=" << frame.GetPayloadSize()
           << " m=" << frame.GetMarker()
           << " x=" << frame.GetExtension()
           << " seq=" << frame.GetSequenceNumber()
           << " ts=" << frame.GetTimestamp()
           << " src=" << RTP_TRACE_SRC(frame.GetSyncSource())
           << " ccnt=" << frame.GetContribSrcCount());

#if OPAL_RTCP_XR
    delete m_metrics; // Should be NULL, but just in case ...
    m_metrics = RTCP_XR_Metrics::Create(frame);
    PTRACE_CONTEXT_ID_TO(m_metrics);
#endif

    if ((pt == RTP_DataFrame::T38) &&
        (frame.GetSequenceNumber() >= 0x8000) &&
         (frame.GetPayloadSize() == 0)) {
      PTRACE(4, "Session " << m_sessionId << ", ignoring left over audio packet from switch to T.38");
      return e_IgnorePacket; // Non fatal error, just ignore
    }

    expectedSequenceNumber = (WORD)(frame.GetSequenceNumber() + 1);
  }
  else {
    if (frame.GetSyncSource() != syncSourceIn) {
      if (allowAnySyncSource) {
        PTRACE(2, "Session " << m_sessionId << ", SSRC changed from "
               << RTP_TRACE_SRC(frame.GetSyncSource()) << " to " << RTP_TRACE_SRC(syncSourceIn));
        syncSourceIn = frame.GetSyncSource();
        allowSequenceChange = true;
      } 
      else if (allowOneSyncSourceChange) {
        PTRACE(2, "Session " << m_sessionId << ", allowed one SSRC change from "
                  "SSRC=" << RTP_TRACE_SRC(syncSourceIn) << " to " << RTP_TRACE_SRC(frame.GetSyncSource()));
        syncSourceIn = frame.GetSyncSource();
        allowSequenceChange = true;
        allowOneSyncSourceChange = false;
      }
      else {
        PTRACE(2, "Session " << m_sessionId << ", packet from "
                  "SSRC=" << RTP_TRACE_SRC(frame.GetSyncSource()) << " ignored, "
                  "expecting SSRC=" << RTP_TRACE_SRC(syncSourceIn));
        return e_IgnorePacket; // Non fatal error, just ignore
      }
    }

    WORD sequenceNumber = frame.GetSequenceNumber();
    if (sequenceNumber == expectedSequenceNumber) {
      expectedSequenceNumber++;
      consecutiveOutOfOrderPackets = 0;

      if (!m_pendingPackets.empty()) {
        PTRACE(5, "Session " << m_sessionId << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn)
               << ", received out of order packet " << sequenceNumber);
        outOfOrderPacketTime = tick;
        packetsOutOfOrder++;
      }

      /* For audio we do not do statistics on start of talk burst as that
         could be a substantial time and is not useful, so we only calculate
         when the marker bit os off.

         For video we measure jitter between whole video frames which is
         normally indicated by the marker bit being on, but this is unreliable,
         many endpoints not sending it correctly, so use a change in timestamp
         as most reliable method. */
      if (m_isAudio ? !frame.GetMarker() : (m_lastReceivedStatisticTimestamp != frame.GetTimestamp())) {
        m_lastReceivedStatisticTimestamp = frame.GetTimestamp();

        DWORD diff = (tick - lastReceivedPacketTime).GetInterval();

        averageReceiveTimeAccum += diff;
        if (diff > maximumReceiveTimeAccum)
          maximumReceiveTimeAccum = diff;
        if (diff < minimumReceiveTimeAccum)
          minimumReceiveTimeAccum = diff;
        rxStatisticsCount++;

        // As per RFC3550 Appendix 8
        diff *= GetJitterTimeUnits(); // Convert to timestamp units
        long variance = diff > lastTransitTime ? (diff - lastTransitTime) : (lastTransitTime - diff);
        lastTransitTime = diff;
        jitterLevel += variance - ((jitterLevel+(1<<(JitterRoundingGuardBits-1))) >> JitterRoundingGuardBits);
        if (jitterLevel > maximumJitterLevel)
          maximumJitterLevel = jitterLevel;
      }

      if (frame.GetMarker())
        markerRecvCount++;
    }
    else if (allowSequenceChange) {
      expectedSequenceNumber = (WORD) (sequenceNumber + 1);
      allowSequenceChange = false;
      m_pendingPackets.clear();
      PTRACE(2, "Session " << m_sessionId << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn)
             << ", adjusting sequence numbers to expect " << expectedSequenceNumber);
    }
    else if (sequenceNumber < expectedSequenceNumber) {
#if OPAL_RTCP_XR
      if (m_metrics != NULL) m_metrics->OnPacketDiscarded();
#endif

      // Check for Cisco bug where sequence numbers suddenly start incrementing
      // from a different base.
      if (++consecutiveOutOfOrderPackets > 10) {
        expectedSequenceNumber = (WORD)(sequenceNumber + 1);
        PTRACE(2, "Session " << m_sessionId << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn)
               << ", abnormal change of sequence numbers, adjusting to expect " << expectedSequenceNumber);
      }
      else {
        PTRACE(2, "Session " << m_sessionId << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn)
               << ", incorrect sequence, got " << sequenceNumber << " expected " << expectedSequenceNumber);

        if (resequenceOutOfOrderPackets)
          return e_IgnorePacket; // Non fatal error, just ignore

        packetsOutOfOrder++;
      }
    }
    else if (resequenceOutOfOrderPackets &&
                (m_pendingPackets.empty() || (tick - outOfOrderPacketTime) < outOfOrderWaitTime)) {
      if (m_pendingPackets.empty())
        outOfOrderPacketTime = tick;
      // Maybe packet lost, maybe out of order, save for now
      SaveOutOfOrderPacket(frame);
      return e_IgnorePacket;
    }
    else {
      if (!m_pendingPackets.empty()) {
        // Give up on the packet, probably never coming in. Save current and switch in
        // the lowest numbered packet.
        SaveOutOfOrderPacket(frame);

        for (;;) {
          if (m_pendingPackets.empty())
            return e_IgnorePacket;

          frame = m_pendingPackets.back();
          m_pendingPackets.pop_back();

          sequenceNumber = frame.GetSequenceNumber();
          if (sequenceNumber >= expectedSequenceNumber)
            break;

          PTRACE(2, "Session " << m_sessionId << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn)
                 << ", incorrect sequence after re-ordering, got "
                 << sequenceNumber << " expected " << expectedSequenceNumber);
        }
      }

      unsigned dropped = sequenceNumber - expectedSequenceNumber;
      frame.SetDiscontinuity(dropped);
      packetsLost += dropped;
      packetsLostSinceLastRR += dropped;
      PTRACE(2, "Session " << m_sessionId << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn)
             << ", " << dropped << " packet(s) missing at " << expectedSequenceNumber);
      expectedSequenceNumber = (WORD)(sequenceNumber + 1);
      consecutiveOutOfOrderPackets = 0;
#if OPAL_RTCP_XR
      if (m_metrics != NULL) m_metrics->OnPacketLost(dropped);
#endif
    }
  }

  lastReceivedPacketTime = tick;

  octetsReceived += frame.GetPayloadSize();
  packetsReceived++;

  if (m_syncRealTime.IsValid())
    frame.SetAbsoluteTime(m_syncRealTime + PTimeInterval((frame.GetTimestamp() - m_syncTimestamp)/m_timeUnits));

#if OPAL_RTCP_XR
  if (m_metrics != NULL) m_metrics->OnPacketReceived();
#endif

  if (rxStatisticsCount >= rxStatisticsInterval) {

    rxStatisticsCount = 0;

    averageReceiveTime = averageReceiveTimeAccum/rxStatisticsInterval;
    maximumReceiveTime = maximumReceiveTimeAccum;
    minimumReceiveTime = minimumReceiveTimeAccum;

    averageReceiveTimeAccum = 0;
    maximumReceiveTimeAccum = 0;
    minimumReceiveTimeAccum = 0xffffffff;

    PTRACE(4, "Session " << m_sessionId << ", receive statistics:"
              " packets=" << packetsReceived <<
              " octets=" << octetsReceived <<
              " lost=" << packetsLost <<
              " tooLate=" << GetPacketsTooLate() <<
              " order=" << packetsOutOfOrder <<
              " avgTime=" << averageReceiveTime <<
              " maxTime=" << maximumReceiveTime <<
              " minTime=" << minimumReceiveTime <<
              " jitter=" << GetAvgJitterTime() <<
              " maxJitter=" << GetMaxJitterTime());
  }

  SendReceiveStatus status = e_ProcessPacket;
  for (list<FilterNotifier>::iterator filter = m_filters.begin(); filter != m_filters.end(); ++filter) 
    (*filter)(frame, status);

  return status;
}


void OpalRTPSession::SaveOutOfOrderPacket(RTP_DataFrame & frame)
{
  WORD sequenceNumber = frame.GetSequenceNumber();

  PTRACE(m_pendingPackets.empty() ? 2 : 5,
         "Session " << m_sessionId << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn) << ", "
         << (m_pendingPackets.empty() ? "first" : "next") << " out of order packet, got "
         << sequenceNumber << " expected " << expectedSequenceNumber);

  RTP_DataFrameList::iterator it;
  for (it = m_pendingPackets.begin(); it != m_pendingPackets.end(); ++it) {
    if (sequenceNumber > it->GetSequenceNumber())
      break;
  }

  m_pendingPackets.insert(it, frame);
  frame.MakeUnique();
}


bool OpalRTPSession::InitialiseControlFrame(RTP_ControlFrame & report)
{
  report.StartNewPacket();

  // No packets sent yet, so only set RR
  if (packetsSent == 0) {

    // Send RR as we are not transmitting
    report.SetPayloadType(RTP_ControlFrame::e_ReceiverReport);

    // if no packets received, put in an empty report
    if (packetsReceived == 0) {
      report.SetPayloadSize(sizeof(PUInt32b));  // length is SSRC 
      report.SetCount(0);

      // add the SSRC to the start of the payload
      *(PUInt32b *)report.GetPayloadPtr() = syncSourceOut;
      PTRACE(m_levelTxRR, "Session " << m_sessionId << ", Sending ReceiverReport: SSRC=" << RTP_TRACE_SRC(syncSourceIn));
    }
    else {
      report.SetPayloadSize(sizeof(PUInt32b) + sizeof(RTP_ControlFrame::ReceiverReport));  // length is SSRC of packet sender plus RR
      report.SetCount(1);
      BYTE * payload = report.GetPayloadPtr();

      // add the SSRC to the start of the payload
      *(PUInt32b *)payload = syncSourceOut;

      // add the RR after the SSRC
      AddReceiverReport(*(RTP_ControlFrame::ReceiverReport *)(payload+sizeof(PUInt32b)));
    }
  }
  else {
    // send SR and RR
    report.SetPayloadType(RTP_ControlFrame::e_SenderReport);
    report.SetPayloadSize(sizeof(PUInt32b) + sizeof(RTP_ControlFrame::SenderReport));  // length is SSRC of packet sender plus SR
    report.SetCount(0);
    BYTE * payload = report.GetPayloadPtr();

    // add the SSRC to the start of the payload
    *(PUInt32b *)payload = syncSourceOut;

    // add the SR after the SSRC
    RTP_ControlFrame::SenderReport * sender = (RTP_ControlFrame::SenderReport *)(payload+sizeof(PUInt32b));
    PTime now;
    sender->ntp_sec  = (DWORD)(now.GetTimeInSeconds()+SecondsFrom1900to1970); // Convert from 1970 to 1900
    sender->ntp_frac = now.GetMicrosecond()*4294; // Scale microseconds to "fraction" from 0 to 2^32
    sender->rtp_ts   = lastSentTimestamp;
    sender->psent    = packetsSent;
    sender->osent    = octetsSent;

    PTRACE(m_levelTxRR, "Session " << m_sessionId << ", Sending SenderReport:"
              " SSRC=" << RTP_TRACE_SRC(syncSourceOut)
           << " ntp=" << sender->ntp_sec << '.' << sender->ntp_frac
           << " rtp=" << sender->rtp_ts
           << " psent=" << sender->psent
           << " osent=" << sender->osent);

    if (syncSourceIn != 0) {
      report.SetPayloadSize(sizeof(PUInt32b) + sizeof(RTP_ControlFrame::SenderReport) + sizeof(RTP_ControlFrame::ReceiverReport));
      report.SetCount(1);
      AddReceiverReport(*(RTP_ControlFrame::ReceiverReport *)(payload+sizeof(PUInt32b)+sizeof(RTP_ControlFrame::SenderReport)));
    }
  }

  report.EndPacket();

  // Add the SDES part to compound RTCP packet
  PTRACE(m_levelTxRR, "Session " << m_sessionId << ", Sending SDES: " << m_canonicalName);
  report.StartNewPacket();

  report.SetCount(0); // will be incremented automatically
  report.StartSourceDescription(syncSourceOut);
  report.AddSourceDescriptionItem(RTP_ControlFrame::e_CNAME, m_canonicalName);
  report.AddSourceDescriptionItem(RTP_ControlFrame::e_TOOL, m_toolName);
  report.EndPacket();
  
#if PTRACING
  m_levelTxRR = 4;
#endif
  return true;
}


void OpalRTPSession::TimedSendReport(PTimer&, P_INT_PTR)
{
  SendReport(false);
}


void OpalRTPSession::SendReport(bool force)
{
  PWaitAndSignal mutex(m_reportMutex);

  // Have not got anything yet, do nothing
  if (!force && (packetsSent == 0 && packetsReceived == 0))
    return;

  RTP_ControlFrame report;
  InitialiseControlFrame(report);

#if OPAL_RTCP_XR
  //Generate and send RTCP-XR packet
  if (m_metrics != NULL)
    m_metrics->InsertExtendedReportPacket(m_sessionId, syncSourceOut, m_jitterBuffer, report);
#endif

  WriteControl(report);
}


#if OPAL_STATISTICS
void OpalRTPSession::GetStatistics(OpalMediaStatistics & statistics, bool receiver) const
{
  statistics.m_startTime         = receiver ? firstPacketReceived     : firstPacketSent;
  statistics.m_totalBytes        = receiver ? GetOctetsReceived()     : GetOctetsSent();
  statistics.m_totalPackets      = receiver ? GetPacketsReceived()    : GetPacketsSent();
  statistics.m_packetsLost       = receiver ? GetPacketsLost()        : GetPacketsLostByRemote();
  statistics.m_packetsOutOfOrder = receiver ? GetPacketsOutOfOrder()  : 0;
  statistics.m_packetsTooLate    = receiver ? GetPacketsTooLate()     : 0;
  statistics.m_packetOverruns    = receiver ? GetPacketOverruns()     : 0;
  statistics.m_minimumPacketTime = receiver ? GetMinimumReceiveTime() : GetMinimumSendTime();
  statistics.m_averagePacketTime = receiver ? GetAverageReceiveTime() : GetAverageSendTime();
  statistics.m_maximumPacketTime = receiver ? GetMaximumReceiveTime() : GetMaximumSendTime();
  statistics.m_averageJitter     = receiver ? GetAvgJitterTime()      : GetJitterTimeOnRemote();
  statistics.m_maximumJitter     = receiver ? GetMaxJitterTime()      : 0;
  statistics.m_jitterBufferDelay = receiver ? GetJitterBufferDelay()  : 0;
}
#endif


OpalRTPSession::ReceiverReportArray
OpalRTPSession::BuildReceiverReportArray(const RTP_ControlFrame & frame, PINDEX offset)
{
  OpalRTPSession::ReceiverReportArray reports;

  const RTP_ControlFrame::ReceiverReport * rr = (const RTP_ControlFrame::ReceiverReport *)(frame.GetPayloadPtr()+offset);
  for (PINDEX repIdx = 0; repIdx < (PINDEX)frame.GetCount(); repIdx++) {
    OpalRTPSession::ReceiverReport * report = new OpalRTPSession::ReceiverReport;
    report->sourceIdentifier = rr->ssrc;
    report->fractionLost = rr->fraction;
    report->totalLost = rr->GetLostPackets();
    report->lastSequenceNumber = rr->last_seq;
    report->jitter = rr->jitter;
    report->lastTimestamp = (PInt64)(DWORD)rr->lsr;
    report->delay = ((PInt64)rr->dlsr << 16)/1000;
    reports.SetAt(repIdx, report);
#if OPAL_RTCP_XR
    if (m_metrics != NULL) m_metrics->OnRxSenderReport(rr->lsr, rr->dlsr);
#endif
    rr++;
  }

  return reports;
}


OpalRTPSession::SendReceiveStatus OpalRTPSession::OnReceiveControl(RTP_ControlFrame & frame)
{
  // Should never return e_ProcessPacket, as this has processed it!

  PTRACE(6, "Session " << m_sessionId << ", OnReceiveControl - "
         << frame.GetSize() << " bytes:\n" << hex << setprecision(2) << setfill('0') << frame << dec);

  m_firstControl = false;

  do {
    BYTE * payload = frame.GetPayloadPtr();
    size_t size = frame.GetPayloadSize(); 
    if ((payload == NULL) || (size == 0) || ((payload + size) > (frame.GetPointer() + frame.GetSize()))){
      /* TODO: 1.shall we test for a maximum size ? Indeed but what's the value ? *
               2. what's the correct exit status ? */
      PTRACE(2, "Session " << m_sessionId << ", OnReceiveControl invalid frame");
      break;
    }

    switch (frame.GetPayloadType()) {
      case RTP_ControlFrame::e_SenderReport :
        if (size >= sizeof(PUInt32b)+sizeof(RTP_ControlFrame::SenderReport)+frame.GetCount()*sizeof(RTP_ControlFrame::ReceiverReport)) {
          SenderReport sender;
          sender.sourceIdentifier = *(const PUInt32b *)payload;
          const RTP_ControlFrame::SenderReport & sr = *(const RTP_ControlFrame::SenderReport *)(payload+sizeof(PUInt32b));
          sender.realTimestamp = PTime(sr.ntp_sec-SecondsFrom1900to1970, sr.ntp_frac/4294);

          // Save the receive time
          lastSRTimestamp = sender.realTimestamp;
          lastSRReceiveTime.SetCurrentTime();
          senderReportsReceived++;

          sender.rtpTimestamp = sr.rtp_ts;
          sender.packetsSent = sr.psent;
          sender.octetsSent = sr.osent;
          OnRxSenderReport(sender, BuildReceiverReportArray(frame, sizeof(PUInt32b)+sizeof(RTP_ControlFrame::SenderReport)));
        }
        else {
          PTRACE(2, "Session " << m_sessionId << ", SenderReport packet truncated");
        }
        break;

      case RTP_ControlFrame::e_ReceiverReport :
        if (size >= sizeof(PUInt32b)+frame.GetCount()*sizeof(RTP_ControlFrame::ReceiverReport))
          OnRxReceiverReport(*(const PUInt32b *)payload, BuildReceiverReportArray(frame, sizeof(PUInt32b)));
        else {
          PTRACE(2, "Session " << m_sessionId << ", ReceiverReport packet truncated");
        }
        break;

      case RTP_ControlFrame::e_SourceDescription :
        if (size >= frame.GetCount()*sizeof(RTP_ControlFrame::SourceDescription)) {
          SourceDescriptionArray descriptions;
          const RTP_ControlFrame::SourceDescription * sdes = (const RTP_ControlFrame::SourceDescription *)payload;
          PINDEX srcIdx;
          for (srcIdx = 0; srcIdx < (PINDEX)frame.GetCount(); srcIdx++) {
            descriptions.SetAt(srcIdx, new SourceDescription(sdes->src));
            const RTP_ControlFrame::SourceDescription::Item * item = sdes->item;
            size_t uiSizeCurrent = 0;   /* current size of the items already parsed */
            while ((item != NULL) && (item->type != RTP_ControlFrame::e_END)) {
              descriptions[srcIdx].items.SetAt(item->type, PString(item->data, item->length));
              uiSizeCurrent += item->GetLengthTotal();
              PTRACE(4,"Session " << m_sessionId << ", SourceDescription item " << item << ", current size = " << uiSizeCurrent);
              
              /* avoid reading where GetNextItem() shall not */
              if (uiSizeCurrent >= size){
                PTRACE(4,"Session " << m_sessionId << ", SourceDescription end of items");
                item = NULL;
                break;
              }

              item = item->GetNextItem();
            }

            /* RTP_ControlFrame::e_END doesn't have a length field, so do NOT call item->GetNextItem()
               otherwise it reads over the buffer */
            if (item == NULL || 
                item->type == RTP_ControlFrame::e_END || 
                (sdes = (const RTP_ControlFrame::SourceDescription *)item->GetNextItem()) == NULL)
              break;
          }
          OnRxSourceDescription(descriptions);
        }
        else {
          PTRACE(2, "Session " << m_sessionId << ", SourceDescription packet truncated");
        }
        break;

      case RTP_ControlFrame::e_Goodbye :
        if (size >= 4) {
          PString str;
          size_t count = frame.GetCount()*4;
    
          if (size > count) {
            if (size >= payload[count] + sizeof(DWORD) /*SSRC*/ + sizeof(unsigned char) /* length */)
              str = PString((const char *)(payload+count+1), payload[count]);
            else {
              PTRACE(2, "Session " << m_sessionId << ", Goodbye packet invalid");
            }
          }

          PDWORDArray sources(count);
          for (size_t i = 0; i < count; i++)
            sources[i] = ((const PUInt32b *)payload)[i];
          OnRxGoodbye(sources, str);
        }
        else {
          PTRACE(2, "Session " << m_sessionId << ", Goodbye packet truncated");
        }

        if (m_closeOnBye) {
          PTRACE(3, "Session " << m_sessionId << ", Goodbye packet closing transport");
          return e_AbortTransport;
        }
        break;

      case RTP_ControlFrame::e_ApplDefined :
        if (size >= 8)
          OnRxApplDefined(RTP_ControlFrame::ApplDefinedInfo(frame));
        else {
          PTRACE(2, "Session " << m_sessionId << ", ApplDefined packet truncated");
        }
        break;

#if OPAL_RTCP_XR
      case RTP_ControlFrame::e_ExtendedReport :
        if (size >= sizeof(PUInt32b)+frame.GetCount()*sizeof(RTP_ControlFrame::ExtendedReport))
          OnRxExtendedReport(*(const PUInt32b *)payload, RTCP_XR_Metrics::BuildExtendedReportArray(frame, sizeof(PUInt32b)));
        else {
          PTRACE(2, "Session " << m_sessionId << ", ReceiverReport packet truncated");
        }
        break;
#endif

      case RTP_ControlFrame::e_TransportLayerFeedBack :
        switch (frame.GetFbType()) {
          case RTP_ControlFrame::e_TMMBR :
            if (size >= sizeof(RTP_ControlFrame::FbTMMB)) {
              const RTP_ControlFrame::FbTMMB * tmmb = (const RTP_ControlFrame::FbTMMB *)payload;
              PTRACE(4, "Session " << m_sessionId << ", "
                        "received TMMBR: rate=" << tmmb->GetBitRate() << ", "
                        "sender SSRC=" << RTP_TRACE_SRC(tmmb->hdr.senderSSRC) << ", "
                        "request SSRC=" << RTP_TRACE_SRC(tmmb->requestSSRC));
              m_connection.ExecuteMediaCommand(OpalMediaFlowControl(tmmb->GetBitRate()), m_sessionId);
            }
            else {
              PTRACE(2, "Session " << m_sessionId << ", TMMBR packet truncated");
            }
            break;

          case RTP_ControlFrame::e_TMMBN :
            if (size >= sizeof(RTP_ControlFrame::FbTMMB)) {
            }
            else {
              PTRACE(2, "Session " << m_sessionId << ", TMMBN packet truncated");
            }
            break;
        }
        break;

#if OPAL_VIDEO
      case RTP_ControlFrame::e_IntraFrameRequest :
        PTRACE(4, "Session " << m_sessionId << ", received RFC2032 FIR");
        m_connection.OnRxIntraFrameRequest(*this, true);
        break;

      case RTP_ControlFrame::e_PayloadSpecificFeedBack :
        switch (frame.GetFbType()) {
          case RTP_ControlFrame::e_PictureLossIndication :
            PTRACE(4, "Session " << m_sessionId << ", "
                      "received RFC4585 PLI: " << ", "
                      "sender SSRC=" << RTP_TRACE_SRC(((const RTP_ControlFrame::FbHeader *)payload)->senderSSRC) << ", "
                      "media SSRC=" << RTP_TRACE_SRC(((const RTP_ControlFrame::FbHeader *)payload)->mediaSSRC));
            m_connection.OnRxIntraFrameRequest(*this, false);
            break;

          case RTP_ControlFrame::e_FullIntraRequest :
            PTRACE(4, "Session " << m_sessionId << ", "
                      "received RFC5104 FIR: "
                      "sender SSRC=" << RTP_TRACE_SRC(((const RTP_ControlFrame::FbFIR *)payload)->hdr.senderSSRC) << ", "
                      "request SSRC=" << RTP_TRACE_SRC(((const RTP_ControlFrame::FbFIR *)payload)->requestSSRC) << ", "
                      "sn=" << (unsigned)((const RTP_ControlFrame::FbFIR *)payload)->sequenceNumber);
            m_connection.OnRxIntraFrameRequest(*this, true);
            break;

          default :
            PTRACE(2, "Session " << m_sessionId << ", Unknown Payload Specific feedback type: " << frame.GetFbType());
        }
        break;
  #endif

      default :
        PTRACE(2, "Session " << m_sessionId << ", Unknown control payload type: " << frame.GetPayloadType());
    }
  } while (frame.ReadNextPacket());

  return e_ProcessPacket;
}


void OpalRTPSession::OnRxSenderReport(const SenderReport & sender, const ReceiverReportArray & reports)
{
  m_syncTimestamp = sender.rtpTimestamp;
  m_syncRealTime = sender.realTimestamp;

#if PTRACING
  if (PTrace::CanTrace(m_levelRxSR)) {
    ostream & strm = PTrace::Begin(m_levelRxSR, __FILE__, __LINE__, this);
    strm << "Session " << m_sessionId << ", OnRxSenderReport: " << sender << '\n';
    for (PINDEX i = 0; i < reports.GetSize(); i++)
      strm << "  RR: " << reports[i] << '\n';
    strm << PTrace::End;
    m_levelRxSR = 4;
  }
#endif

  OnReceiverReports(reports);
}


void OpalRTPSession::OnRxReceiverReport(DWORD PTRACE_PARAM(src), const ReceiverReportArray & reports)
{
#if PTRACING
  if (PTrace::CanTrace(m_levelRxRR)) {
    ostream & strm = PTrace::Begin(m_levelRxRR, __FILE__, __LINE__, this);
    strm << "Session " << m_sessionId << ", OnReceiverReport: SSRC=" << RTP_TRACE_SRC(src) << '\n';
    for (PINDEX i = 0; i < reports.GetSize(); i++)
      strm << "  RR: " << reports[i] << '\n';
    strm << PTrace::End;
    m_levelRxRR = 4;
  }
#endif
  OnReceiverReports(reports);
}


void OpalRTPSession::OnReceiverReports(const ReceiverReportArray & reports)
{
  for (PINDEX i = 0; i < reports.GetSize(); i++) {
    if (reports[i].sourceIdentifier == syncSourceOut) {
      packetsLostByRemote = reports[i].totalLost;
      jitterLevelOnRemote = reports[i].jitter;
      break;
    }
  }
}


void OpalRTPSession::OnRxSourceDescription(const SourceDescriptionArray & PTRACE_PARAM(description))
{
#if PTRACING
  if (PTrace::CanTrace(m_levelRxSDES)) {
    ostream & strm = PTrace::Begin(m_levelRxSDES, __FILE__, __LINE__, this);
    strm << "Session " << m_sessionId << ", OnSourceDescription: " << description.GetSize() << " entries";
    for (PINDEX i = 0; i < description.GetSize(); i++)
      strm << "\n  " << description[i];
    strm << PTrace::End;
    m_levelRxSDES = 4;
  }
#endif
}


void OpalRTPSession::OnRxGoodbye(const PDWORDArray & PTRACE_PARAM(src), const PString & PTRACE_PARAM(reason))
{
#if PTRACING
  if (PTrace::CanTrace(3)) {
    ostream & strm = PTrace::Begin(3, __FILE__, __LINE__, this);
    strm << "Session " << m_sessionId << ", OnGoodbye: " << reason << "\" SSRC=";
    for (PINDEX i = 0; i < src.GetSize(); i++)
      strm << RTP_TRACE_SRC(src[i]) << ' ';
    strm << PTrace::End;
  }
#endif
}


void OpalRTPSession::OnRxApplDefined(const RTP_ControlFrame::ApplDefinedInfo & info)
{
  PTRACE(3, "Session " << m_sessionId << ", OnApplDefined: \""
         << info.m_type << "\"-" << info.m_subType << " " << info.m_SSRC << " [" << info.m_size << ']');
  m_applDefinedNotifiers(*this, info);
}


#if PTRACING
void OpalRTPSession::ReceiverReport::PrintOn(ostream & strm) const
{
  strm << "SSRC=" << RTP_TRACE_SRC(sourceIdentifier)
       << " fraction=" << fractionLost
       << " lost=" << totalLost
       << " last_seq=" << lastSequenceNumber
       << " jitter=" << jitter
       << " lsr=" << lastTimestamp
       << " dlsr=" << delay;
}


void OpalRTPSession::SenderReport::PrintOn(ostream & strm) const
{
  strm << "SSRC=" << RTP_TRACE_SRC(sourceIdentifier)
       << " ntp=" << realTimestamp.AsString("yyyy/M/d-h:m:s.uuuu")
       << " rtp=" << rtpTimestamp
       << " psent=" << packetsSent
       << " osent=" << octetsSent;
}


void OpalRTPSession::SourceDescription::PrintOn(ostream & strm) const
{
  static const char * const DescriptionNames[RTP_ControlFrame::NumDescriptionTypes] = {
    "END", "CNAME", "NAME", "EMAIL", "PHONE", "LOC", "TOOL", "NOTE", "PRIV"
  };

  strm << "SSRC=" << RTP_TRACE_SRC(sourceIdentifier);
  for (POrdinalToString::const_iterator it = items.begin(); it != items.end(); ++it) {
    unsigned typeNum = it->first;
    strm << "\n  item[" << typeNum << "]: type=";
    if (typeNum < PARRAYSIZE(DescriptionNames))
      strm << DescriptionNames[typeNum];
    else
      strm << typeNum;
    strm << " data=\"" << it->second << '"';
  }
}
#endif // PTRACING


DWORD OpalRTPSession::GetPacketsTooLate() const
{
  JitterBufferPtr jitter = m_jitterBuffer; // Increase reference count
  return jitter != NULL ? jitter->GetPacketsTooLate() : 0;
}


DWORD OpalRTPSession::GetPacketOverruns() const
{
  JitterBufferPtr jitter = m_jitterBuffer; // Increase reference count
  return jitter != NULL ? jitter->GetBufferOverruns() : 0;
}


void OpalRTPSession::SendFlowControl(unsigned maxBitRate, unsigned overhead, bool notify)
{
  // Create packet
  RTP_ControlFrame request;
  InitialiseControlFrame(request);

  PTRACE(3, "Session " << m_sessionId << ", Sending TMMBR (flow control) "
            "rate=" << maxBitRate << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn));

  request.StartNewPacket();

  request.SetPayloadType(RTP_ControlFrame::e_TransportLayerFeedBack);
  request.SetFbType(notify ? RTP_ControlFrame::e_TMMBN : RTP_ControlFrame::e_TMMBR, sizeof(RTP_ControlFrame::FbTMMB));

  RTP_ControlFrame::FbTMMB * tmmb = (RTP_ControlFrame::FbTMMB *)request.GetPayloadPtr();
  tmmb->hdr.senderSSRC = syncSourceOut;
  tmmb->hdr.mediaSSRC = 0;
  tmmb->requestSSRC = syncSourceIn;

  if (overhead == 0)
    overhead = m_localAddress.GetVersion() == 4 ? (20+8+12) : (40+8+12);

  unsigned exponent = 0;
  unsigned mantissa = maxBitRate;
  while (mantissa >= 0x20000) {
    mantissa >>= 1;
    ++exponent;
  }
  tmmb->bitRateAndOverhead = overhead | (mantissa << 9) | (exponent << 26);

  // Send it
  request.EndPacket();
  WriteControl(request);
}


#if OPAL_VIDEO

void OpalRTPSession::SendIntraFrameRequest(bool rfc2032, bool pictureLoss)
{
  // Create packet
  RTP_ControlFrame request;
  InitialiseControlFrame(request);

  PTRACE(3, "Session " << m_sessionId << ", Sending "
         << (rfc2032 ? "RFC2032" : (pictureLoss ? "RFC4585 PLI" : "RFC5104 FIR"))
         << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn));

  request.StartNewPacket();

  if (rfc2032) {
    // Create packet
    request.SetPayloadType(RTP_ControlFrame::e_IntraFrameRequest);
    request.SetPayloadSize(4);
    // Insert SSRC
    request.SetCount(1);
    BYTE * payload = request.GetPayloadPtr();
    *(PUInt32b *)payload = syncSourceIn;
  }
  else {
    request.SetPayloadType(RTP_ControlFrame::e_PayloadSpecificFeedBack);
    if (pictureLoss) {
      request.SetFbType(RTP_ControlFrame::e_PictureLossIndication, sizeof(RTP_ControlFrame::FbHeader));
      RTP_ControlFrame::FbHeader * hdr = (RTP_ControlFrame::FbHeader *)request.GetPayloadPtr();
      hdr->senderSSRC = syncSourceOut;
      hdr->mediaSSRC = syncSourceIn;
    }
    else {
      request.SetFbType(RTP_ControlFrame::e_FullIntraRequest, sizeof(RTP_ControlFrame::FbFIR));
      RTP_ControlFrame::FbFIR * fir = (RTP_ControlFrame::FbFIR *)request.GetPayloadPtr();
      fir->hdr.senderSSRC = syncSourceOut;
      fir->hdr.mediaSSRC = 0;
      fir->requestSSRC = syncSourceIn;
      fir->sequenceNumber = m_lastFIRSequenceNumber++;
    }
  }

  // Send it
  request.EndPacket();
  WriteControl(request);
}


void OpalRTPSession::SendTemporalSpatialTradeOff(unsigned tradeOff)
{
  RTP_ControlFrame request;
  InitialiseControlFrame(request);

  PTRACE(3, "Session " << m_sessionId << ", Sending TSTO (temporal spatial trade off) "
            "value=" << tradeOff << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn));

  request.StartNewPacket();

  request.SetPayloadType(RTP_ControlFrame::e_PayloadSpecificFeedBack);
  request.SetFbType(RTP_ControlFrame::e_TemporalSpatialTradeOffRequest, sizeof(RTP_ControlFrame::FbTSTO));
  RTP_ControlFrame::FbTSTO * tsto = (RTP_ControlFrame::FbTSTO *)request.GetPayloadPtr();
  tsto->hdr.senderSSRC = syncSourceOut;
  tsto->hdr.mediaSSRC = 0;
  tsto->requestSSRC = syncSourceIn;
  tsto->sequenceNumber = m_lastTSTOSequenceNumber++;
  tsto->tradeOff = (BYTE)tradeOff;

  // Send it
  request.EndPacket();
  WriteControl(request);
}

#endif // OPAL_VIDEO


void OpalRTPSession::AddFilter(const FilterNotifier & filter)
{
  // ensures that a filter is added only once
  if (std::find(m_filters.begin(), m_filters.end(), filter) == m_filters.end())
    m_filters.push_back(filter);
}


/////////////////////////////////////////////////////////////////////////////

#if PTRACING
#define SetMinBufferSize(sock, bufType, newSize) SetMinBufferSizeFn(sock, bufType, newSize, #bufType)
static void SetMinBufferSizeFn(PUDPSocket & sock, int bufType, int newSize, const char * bufTypeName)
#else
static void SetMinBufferSize(PUDPSocket & sock, int bufType, int newSize)
#endif
{
  int originalSize = 0;
  if (!sock.GetOption(bufType, originalSize)) {
    PTRACE(1, "GetOption(" << sock.GetHandle() << ',' << bufTypeName << ")"
              " failed: " << sock.GetErrorText());
    return;
  }

  // Already big enough
  if (originalSize >= newSize) {
    PTRACE(4, "SetOption(" << sock.GetHandle() << ',' << bufTypeName << ',' << newSize << ")"
              " unecessary, already " << originalSize);
    return;
  }

  for (; newSize >= 1024; newSize -= newSize/10) {
    // Set to new size
    if (!sock.SetOption(bufType, newSize)) {
      PTRACE(1, "SetOption(" << sock.GetHandle() << ',' << bufTypeName << ',' << newSize << ")"
                " failed: " << sock.GetErrorText());
      continue;
    }

    // As some stacks lie about setting the buffer size, we double check.
    int adjustedSize;
    if (!sock.GetOption(bufType, adjustedSize)) {
      PTRACE(1, "GetOption(" << sock.GetHandle() << ',' << bufTypeName << ")"
                " failed: " << sock.GetErrorText());
      return;
    }

    if (adjustedSize >= newSize) {
      PTRACE(4, "SetOption(" << sock.GetHandle() << ',' << bufTypeName << ',' << newSize << ")"
                " succeeded, actually " << adjustedSize);
      return;
    }

    if (adjustedSize > originalSize) {
      PTRACE(4, "SetOption(" << sock.GetHandle() << ',' << bufTypeName << ',' << newSize << ")"
                " clamped to maximum " << adjustedSize);
      return;
    }

    PTRACE(2, "SetOption(" << sock.GetHandle() << ',' << bufTypeName << ',' << newSize << ")"
              " failed, even though it said it succeeded!");
  }
}


OpalTransportAddress OpalRTPSession::GetLocalAddress(bool isMediaAddress) const
{
  if (m_localAddress.IsValid() && m_localPort[isMediaAddress] != 0)
    return OpalTransportAddress(m_localAddress, m_localPort[isMediaAddress], OpalTransportAddress::UdpPrefix());
  else
    return OpalTransportAddress();
}


OpalTransportAddress OpalRTPSession::GetRemoteAddress(bool isMediaAddress) const
{
  if (m_remoteAddress.IsValid() && m_remotePort[isMediaAddress] != 0)
    return OpalTransportAddress(m_remoteAddress, m_remotePort[isMediaAddress], OpalTransportAddress::UdpPrefix());
  else
    return OpalTransportAddress();
}


OpalMediaStream * OpalRTPSession::CreateMediaStream(const OpalMediaFormat & mediaFormat, 
                                                    unsigned sessionId, 
                                                    bool isSource)
{
  if (m_socket[e_Data] != NULL) {
    PIPSocket::QoS qos = m_connection.GetEndPoint().GetManager().GetMediaQoS(m_mediaType);
    qos.m_remote.SetAddress(m_remoteAddress, m_remotePort[e_Data]);

    unsigned maxBitRate = mediaFormat.GetMaxBandwidth();
    if (maxBitRate != 0) {
      unsigned overheadBytes = m_localAddress.GetVersion() == 4 ? (20+8+12) : (40+8+12);
      unsigned overheadBits = overheadBytes*8;

      unsigned frameSize = mediaFormat.GetFrameSize();
      if (frameSize == 0)
        frameSize = m_connection.GetEndPoint().GetManager().GetMaxRtpPayloadSize();

      unsigned packetSize = frameSize*mediaFormat.GetOptionInteger(OpalAudioFormat::RxFramesPerPacketOption(), 1);

      qos.m_receive.m_maxPacketSize = packetSize + overheadBytes;
      packetSize *= 8;
      qos.m_receive.m_maxBandwidth = maxBitRate + (maxBitRate+packetSize-1)/packetSize * overheadBits;

      maxBitRate = mediaFormat.GetOptionInteger(OpalMediaFormat::TargetBitRateOption(), maxBitRate);
      packetSize = frameSize*mediaFormat.GetOptionInteger(OpalAudioFormat::TxFramesPerPacketOption(), 1);

      qos.m_transmit.m_maxPacketSize = packetSize + overheadBytes;
      packetSize *= 8;
      qos.m_transmit.m_maxBandwidth = maxBitRate + (maxBitRate+packetSize-1)/packetSize * overheadBits;
    }

    // Audio has tighter constraints to video
    if (m_isAudio) {
      qos.m_transmit.m_maxLatency = qos.m_receive.m_maxLatency = 250000; // 250ms
      qos.m_transmit.m_maxJitter = qos.m_receive.m_maxJitter = 100000; // 100ms
    }
    else {
      qos.m_transmit.m_maxLatency = qos.m_receive.m_maxLatency = 750000; // 750ms
      qos.m_transmit.m_maxJitter = qos.m_receive.m_maxJitter = 250000; // 250ms
    }
    m_socket[e_Data]->SetQoS(qos);
  }

  if (PAssert(m_sessionId == sessionId && m_mediaType == mediaFormat.GetMediaType(), PLogicError))
    return new OpalRTPMediaStream(dynamic_cast<OpalRTPConnection &>(m_connection), mediaFormat, isSource, *this);

  return NULL;
}


bool OpalRTPSession::Open(const PString & localInterface, const OpalTransportAddress & remoteAddress, bool mediaAddress)
{
  if (IsOpen())
    return true;

  PWaitAndSignal mutex1(m_readMutex);
  PWaitAndSignal mutex2(m_dataMutex);

  if (!OpalMediaSession::Open(localInterface, remoteAddress, mediaAddress))
    return false;

  m_firstControl = true;
  m_byeSent = false;
  m_shutdownRead = false;
  m_shutdownWrite = false;

  for (int i = 0; i < 2; ++i) {
    delete m_socket[i];
    m_socket[i] = NULL;
  }

  PIPSocket::Address bindingAddress(localInterface);

  OpalManager & manager = m_connection.GetEndPoint().GetManager();

#if OPAL_PTLIB_NAT
  if (!manager.IsLocalAddress(m_remoteAddress)) {
    PNatMethod * natMethod = manager.GetNatMethods().GetMethod(bindingAddress, this);
    if (natMethod != NULL) {
      PTRACE(4, "NAT Method " << natMethod->GetMethodName() << " selected for call.");

      switch (natMethod->GetRTPSupport()) {
        case PNatMethod::RTPIfSendMedia :
          /* This NAT variant will work if we send something out through the
              NAT port to "open" it so packets can then flow inward. We set
              this flag to make that happen as soon as we get the remotes IP
              address and port to send to.
              */
          m_localHasRestrictedNAT = true;
          // Then do case for full cone support and create NAT sockets

        case PNatMethod::RTPSupported :
          PTRACE(4, "Attempting natMethod: " << natMethod->GetMethodName());
          if (m_singlePortRx) {
            if (!natMethod->CreateSocket(m_socket[e_Data], bindingAddress)) {
              delete m_socket[e_Data];
              m_socket[e_Data] = NULL;
              PTRACE(2, "Session " << m_sessionId << ", " << natMethod->GetMethodName()
                     << " could not create NAT RTP socket, using normal sockets.");
            }
          }
          else if (natMethod->CreateSocketPair(m_socket[e_Data], m_socket[e_Control], bindingAddress, this)) {
            PTRACE(4, "Session " << m_sessionId << ", " << natMethod->GetMethodName() << " created NAT RTP/RTCP socket pair.");
          }
          else {
            PTRACE(2, "Session " << m_sessionId << ", " << natMethod->GetMethodName()
                   << " could not create NAT RTP/RTCP socket pair; trying to create individual sockets.");
            if (!natMethod->CreateSocket(m_socket[e_Data], bindingAddress, 0, this) ||
                !natMethod->CreateSocket(m_socket[e_Control], bindingAddress, 0, this)) {
              for (int i = 0; i < 2; ++i) {
                delete m_socket[i];
                m_socket[i] = NULL;
              }
              PTRACE(2, "Session " << m_sessionId << ", " << natMethod->GetMethodName()
                     << " could not create NAT RTP/RTCP sockets individually either, using normal sockets.");
            }
          }
          break;

        default :
          /* We canot use NAT traversal method (e.g. STUN) to create sockets
              in the remaining modes as the NAT router will then not let us
              talk to the real RTP destination. All we can so is bind to the
              local interface the NAT is on and hope the NAT router is doing
              something sneaky like symmetric port forwarding. */
          natMethod->GetInterfaceAddress(m_localAddress);
          break;
      }
    }
  }
#endif // OPAL_PTLIB_NAT

  if (m_socket[e_Data] == NULL) {
    bool ok;

    m_socket[e_Data] = new PUDPSocket();
    if (m_singlePortRx)
      ok = manager.GetRtpIpPortRange().Listen(*m_socket[e_Data], bindingAddress);
    else {
      m_socket[e_Control] = new PUDPSocket();

      // Don't use for loop, they are in opposite order!
      PIPSocket * sockets[2];
      sockets[0] = m_socket[e_Data];
      sockets[1] = m_socket[e_Control];
      ok = manager.GetRtpIpPortRange().Listen(sockets, 2, bindingAddress);
    }

    if (!ok) {
      PTRACE(1, "RTPCon\tNo ports available for RTP session " << m_sessionId << ","
                " base=" << manager.GetRtpIpPortBase() << ","
                " max=" << manager.GetRtpIpPortMax() << ","
                " bind=" << bindingAddress << ","
                " for " << m_connection);
      return false; // Used up all the available ports!
    }
  }

  for (int i = 0; i < 2; ++i) {
    if (m_socket[i] != NULL) {
      PTRACE_CONTEXT_ID_TO(m_socket[i]);
      m_socket[i]->GetLocalAddress(m_localAddress, m_localPort[i]);
      m_socket[i]->SetReadTimeout(m_maxNoReceiveTime);
    }
  }
  manager.TranslateIPAddress(m_localAddress, m_remoteAddress);

  // Increase internal buffer size on media UDP sockets
  SetMinBufferSize(*m_socket[e_Data], SO_RCVBUF, m_isAudio ? RTP_AUDIO_RX_BUFFER_SIZE : RTP_VIDEO_RX_BUFFER_SIZE);
  SetMinBufferSize(*m_socket[e_Data], SO_SNDBUF, RTP_DATA_TX_BUFFER_SIZE);
  if (m_socket[e_Control] != NULL) {
    SetMinBufferSize(*m_socket[e_Control], SO_RCVBUF, RTP_CTRL_BUFFER_SIZE);
    SetMinBufferSize(*m_socket[e_Control], SO_SNDBUF, RTP_CTRL_BUFFER_SIZE);
  }

  m_reportTimer.RunContinuous(m_reportTimer.GetResetTime());

  PTRACE(3, "Session " << m_sessionId << " opened: "
            " local=" << m_localAddress << ':' << m_localPort[e_Data] << '-' << m_localPort[e_Control]
         << " remote=" << m_remoteAddress << " SSRC=" << RTP_TRACE_SRC(syncSourceOut));

  return true;
}


bool OpalRTPSession::IsOpen() const
{
  return m_socket[e_Data] != NULL && m_socket[e_Data]->IsOpen() &&
        (m_socket[e_Control] == NULL || m_socket[e_Control]->IsOpen());
}


bool OpalRTPSession::Close()
{
  bool ok = Shutdown(true) | Shutdown(false);

  m_reportTimer.Stop(true);

  m_endpoint.RegisterLocalRTP(this, true);

  // We need to do this to make sure that the sockets are not
  // deleted before select decides there is no more data coming
  // over them and exits the reading thread.
  SetJitterBufferSize(OpalJitterBuffer::Init());

  m_readMutex.Wait();
  m_dataMutex.Wait();

  for (int i = 0; i < 2; ++i) {
    delete m_socket[i];
    m_socket[i] = NULL;
  }

  m_localAddress = PIPSocket::GetInvalidAddress();
  m_localPort[e_Data] = m_localPort[e_Control] = 0;
  m_remoteAddress = PIPSocket::GetInvalidAddress();
  m_remotePort[e_Data] = m_remotePort[e_Control] = 0;

  m_dataMutex.Signal();
  m_readMutex.Signal();

  return ok;
}


bool OpalRTPSession::Shutdown(bool reading)
{
  if (reading) {
    {
      PWaitAndSignal mutex(m_dataMutex);

      if (m_shutdownRead) {
        PTRACE(4, "Session " << m_sessionId << ", read already shut down .");
        return false;
      }

      PTRACE(3, "Session " << m_sessionId << ", shutting down read.");

      syncSourceIn = 0;
      m_shutdownRead = true;

      if (m_socket[e_Data] != NULL) {
        PIPSocketAddressAndPort addrAndPort;
        m_socket[e_Data]->PUDPSocket::InternalGetLocalAddress(addrAndPort);
        if (!addrAndPort.IsValid())
          addrAndPort.Parse(PIPSocket::GetHostName());
        BYTE dummy = 0;
        PUDPSocket::Slice slice(&dummy, 1);
        if (!m_socket[e_Data]->PUDPSocket::InternalWriteTo(&slice, 1, addrAndPort)) {
          PTRACE(1, "Session " << m_sessionId << ", could not write to unblock read socket: "
                 << m_socket[e_Data]->GetErrorText(PChannel::LastReadError));
          m_socket[e_Data]->Close();
        }
      }
    }

    SetJitterBufferSize(OpalJitterBuffer::Init()); // Kill jitter buffer too, but outside mutex
  }
  else {
    if (m_shutdownWrite) {
      PTRACE(4, "Session " << m_sessionId << ", write already shut down .");
      return false;
    }

    PTRACE(3, "Session " << m_sessionId << ", shutting down write.");
    m_shutdownWrite = true;
  }

  // If shutting down write, no reporting any more
  if (m_shutdownWrite)
    m_reportTimer.Stop(false);

  return true;
}


void OpalRTPSession::Restart(bool reading)
{
  PWaitAndSignal mutex(m_dataMutex);

  if (reading) {
    if (!m_shutdownRead)
      return;
    m_shutdownRead = false;
  }
  else {
    if (!m_shutdownWrite)
      return;
    m_shutdownWrite = false;
    m_noTransmitErrors = 0;
    m_reportTimer.RunContinuous(m_reportTimer.GetResetTime());
  }

  PTRACE(3, "Session " << m_sessionId << " reopened for " << (reading ? "reading" : "writing"));
}


PString OpalRTPSession::GetLocalHostName()
{
  return PIPSocket::GetHostName();
}


bool OpalRTPSession::SetRemoteAddress(const OpalTransportAddress & remoteAddress, bool isMediaAddress)
{
  PWaitAndSignal m(m_dataMutex);

  if (m_remoteBehindNAT) {
    PTRACE(2, "Session " << m_sessionId << ", ignoring remote address as is behind NAT");
    return true;
  }

  PIPAddressAndPort ap;
  if (!remoteAddress.GetIpAndPort(ap))
    return false;

  return InternalSetRemoteAddress(ap, isMediaAddress PTRACE_PARAM(, "signalling"));
}


bool OpalRTPSession::InternalSetRemoteAddress(const PIPSocket::AddressAndPort & ap, bool isMediaAddress PTRACE_PARAM(, const char * source))
{
  WORD port = ap.GetPort();

  if (m_localAddress == ap.GetAddress() && m_remoteAddress == ap.GetAddress() && m_localPort[isMediaAddress] == port)
    return true;

  m_remoteAddress = ap.GetAddress();
  
  allowOneSyncSourceChange = true;
  allowSequenceChange = packetsReceived != 0;

  if (port != 0) {
    if (isMediaAddress) {
      m_remotePort[e_Data] = port;
      if ((port&1) == 0 && m_remotePort[e_Control] == 0)
        m_remotePort[e_Control] = (WORD)(port + 1);
    }
    else {
      m_remotePort[e_Control] = port;
      if ((port&1) == 1 && m_remotePort[e_Data] == 0)
        m_remotePort[e_Data] = (WORD)(port - 1);
    }
  }

  if (m_socket[isMediaAddress] != NULL)
    m_socket[isMediaAddress]->SetSendAddress(ap);
  if (m_socket[!isMediaAddress] != NULL)
    m_socket[!isMediaAddress]->SetSendAddress(m_remoteAddress, m_remotePort[!isMediaAddress]);

  if (m_localHasRestrictedNAT) {
    PTRACE(2, "Session " << m_sessionId << ", sending empty datagrams to open local restricted NAT");
    // If have Port Restricted NAT on local host then send a datagram
    // to remote to open up the port in the firewall for return data.
    static const BYTE dummy[e_Data] = { 0 };
    WriteRawPDU(dummy, sizeof(dummy), true);
    if (m_socket[e_Control] != NULL)
      WriteRawPDU(dummy, sizeof(dummy), false);
  }

#if PTRACING
  static const int Level = 3;
  if (PTrace::CanTrace(Level)) {
    ostream & trace = PTRACE_BEGIN(Level);
    trace << "Session " << m_sessionId << ", " << source << " set remote "
          << (isMediaAddress ? "data" : "control") << " address to " << ap << ", ";

    if (IsSinglePortTx())
      trace << "single port mode";
    else {
      if (m_remotePort[e_Data] != (m_remotePort[e_Control] & 0xfffe))
        trace << "disjoint ";
      trace << (isMediaAddress ? "control" : "data") << " port " << m_remotePort[!isMediaAddress];
    }

    trace << ", local=";
    if (m_localAddress.IsValid()) {
      trace << m_localAddress << ':' << m_localPort[e_Data];
      if (!IsSinglePortRx())
        trace << '-' << m_localPort[e_Control];
    }
    else
      trace << "not open";

    trace << PTrace::End;
  }
#endif

  return true;
}


bool OpalRTPSession::InternalReadData(RTP_DataFrame & frame)
{
  PWaitAndSignal mutex(m_readMutex);

  SendReceiveStatus receiveStatus = e_IgnorePacket;
  while (receiveStatus == e_IgnorePacket) {
    if (m_shutdownRead || PAssertNULL(m_socket[e_Data]) == NULL)
      return false;

    if (m_socket[e_Control] == NULL)
      receiveStatus = ReadDataPDU(frame);
    else {
      int selectStatus = PSocket::Select(*m_socket[e_Data], *m_socket[e_Control], m_maxNoReceiveTime);

      if (m_shutdownRead)
        return false;

      if (selectStatus > 0) {
        PTRACE(1, "Session " << m_sessionId << ", Select error: "
                << PChannel::GetErrorText((PChannel::Errors)selectStatus));
        return false;
      }

      if (selectStatus == 0)
        receiveStatus = OnReadTimeout(frame);

      if ((-selectStatus & 2) != 0) {
        if (ReadControlPDU() == e_AbortTransport)
          return false;
      }

      if ((-selectStatus & 1) != 0)
        receiveStatus = ReadDataPDU(frame);
    }
  }

  return receiveStatus == e_ProcessPacket;
}


#if OPAL_ICE
void OpalRTPSession::SetICE(const PString & user, const PString & pass, const PNatCandidateList & candidates)
{
  delete m_stunServer;
  m_stunServer = NULL;

  delete m_stunClient;
  m_stunClient = NULL;

  OpalMediaSession::SetICE(user, pass, candidates);
  if (user.IsEmpty() || pass.IsEmpty())
    return;

  for (PNatCandidateList::const_iterator it = candidates.begin(); it != candidates.end(); ++it) {
    if (it->m_protocol == "udp" &&
        it->m_component >= PNatMethod::eComponent_RTP &&
        it->m_component <= PNatMethod::eComponent_RTCP &&
        it->m_localTransportAddress.GetPort() == m_remotePort[it->m_component-1])
      m_candidates[it->m_component-1].push_back(it->m_localTransportAddress);
  }

  m_stunServer = new PSTUNServer;
  m_stunServer->Open(m_socket[e_Data], m_socket[e_Control]);
  m_stunServer->SetCredentials(m_localUsername + ':' + m_remoteUsername, m_localPassword, PString::Empty());

  m_stunClient = new PSTUNClient;
  m_stunClient->SetCredentials(m_remoteUsername + ':' + m_localUsername, m_remoteUsername, PString::Empty());

  m_remoteBehindNAT = true;

  PTRACE(4, "Session " << m_sessionId << ", configured for ICE with candidates: "
            "data=" << m_candidates[e_Data].size() << ", " "control=" << m_candidates[e_Control].size());
}


OpalRTPSession::SendReceiveStatus OpalRTPSession::OnReceiveICE(bool fromDataChannel,
                                                               const BYTE * framePtr,
                                                               PINDEX frameSize,
                                                               const PIPSocket::AddressAndPort & ap)
{
  if (m_stunServer == NULL)
    return e_ProcessPacket;

  PSTUNMessage message(framePtr, frameSize, ap);
  if (!message.IsValid())
    return e_ProcessPacket;

  if (message.IsRequest()) {
    if (!m_stunServer->OnReceiveMessage(message, PSTUNServer::SocketInfo(m_socket[fromDataChannel])))
      return e_IgnorePacket;

    if (!m_candidates[fromDataChannel].empty() && message.FindAttribute(PSTUNAttribute::USE_CANDIDATE) == NULL)
      return e_IgnorePacket;
  }
  else {
    if (!m_stunClient->ValidateMessageIntegrity(message))
      return e_IgnorePacket;

    for (CandidateStates::const_iterator it = m_candidates[fromDataChannel].begin(); ; ++it) {
      if (it == m_candidates[fromDataChannel].end())
        return e_IgnorePacket;
      if (it->m_remoteAP == ap)
        break;
    }
  }

  if (!m_remoteAddress.IsValid())
    InternalSetRemoteAddress(ap, fromDataChannel PTRACE_PARAM(, "ICE"));
  m_candidates[fromDataChannel].clear();
  return e_IgnorePacket;
}


OpalRTPSession::SendReceiveStatus OpalRTPSession::OnSendICE(bool toDataChannel)
{
  if (m_candidates[toDataChannel].empty())
    return e_ProcessPacket;

  for (CandidateStates::iterator it = m_candidates[toDataChannel].begin(); it != m_candidates[toDataChannel].end(); ++it) {
    PSTUNMessage request(PSTUNMessage::BindingRequest);
    m_stunClient->AppendMessageIntegrity(request);
    if (!request.Write(*m_socket[toDataChannel], it->m_remoteAP))
      return e_AbortTransport;
  }

  return e_IgnorePacket;
}


#endif // OPAL_ICE


OpalRTPSession::SendReceiveStatus OpalRTPSession::ReadRawPDU(BYTE * framePtr,
                                                             PINDEX & frameSize,
                                                             bool fromDataChannel)
{
#if PTRACING
  const char * channelName = fromDataChannel ? "Data" : "Control";
#endif
  PUDPSocket & socket = *m_socket[fromDataChannel];
  PIPSocket::AddressAndPort ap;

  if (socket.ReadFrom(framePtr, frameSize, ap)) {
    frameSize = socket.GetLastReadCount();

    // Ignore one byte packet from ourself, likely from the I/O block breaker in OpalRTPSession::Shutdown()
    if (frameSize == 1) {
      PIPSocketAddressAndPort localAP;
      socket.PUDPSocket::InternalGetLocalAddress(localAP);
      if (ap == localAP) {
        PTRACE(5, "Session " << m_sessionId << ", " << channelName << " I/O block breaker ignored.");
        return e_IgnorePacket;
      }
    }

#if OPAL_ICE
    SendReceiveStatus status = OnReceiveICE(fromDataChannel, framePtr, frameSize, ap);
    if (status != e_ProcessPacket)
      return status;
#endif // OPAL_ICE

    // If remote address never set from higher levels, then try and figure
    // it out from the first packet received.
    if (m_remotePort[fromDataChannel] == 0)
      InternalSetRemoteAddress(ap, fromDataChannel PTRACE_PARAM(, "first PDU"));

    m_noTransmitErrors = 0;

    return e_ProcessPacket;
  }

  switch (socket.GetErrorCode(PChannel::LastReadError)) {
    case PChannel::Unavailable :
      if (!HandleUnreachable(PTRACE_PARAM(channelName)))
        Shutdown(false); // Terminate transmission
      return e_IgnorePacket;

    case PChannel::BufferTooSmall :
      PTRACE(2, "Session " << m_sessionId << ", " << channelName
             << " read packet too large for buffer of " << frameSize << " bytes.");
      return e_IgnorePacket;

    case PChannel::Interrupted :
      PTRACE(4, "Session " << m_sessionId << ", " << channelName
             << " read packet interrupted.");
      // Shouldn't happen, but it does.
      return e_IgnorePacket;

    case PChannel::NoError :
      PTRACE(3, "Session " << m_sessionId << ", " << channelName
             << " received UDP packet with no payload.");
      return e_IgnorePacket;

    default:
      PTRACE(1, "Session " << m_sessionId << ", " << channelName
             << " read error (" << socket.GetErrorNumber(PChannel::LastReadError) << "): "
             << socket.GetErrorText(PChannel::LastReadError));
      m_connection.OnMediaFailed(m_sessionId, true);
      return e_AbortTransport;
  }
}


bool OpalRTPSession::HandleUnreachable(PTRACE_PARAM(const char * channelName))
{
  if (++m_noTransmitErrors == 1) {
    PTRACE(2, "Session " << m_sessionId << ", " << channelName << " port on remote not ready.");
    m_noTransmitTimer = m_maxNoTransmitTime;
    return true;
  }

  if (m_noTransmitErrors < 10 || m_noTransmitTimer.IsRunning())
    return true;

  PTRACE(2, "Session " << m_sessionId << ", " << channelName << ' '
         << m_maxNoTransmitTime << " seconds of transmit fails - informing connection");
  if (m_connection.OnMediaFailed(m_sessionId, false))
    return false;

  m_noTransmitErrors = 0;
  return true;
}


OpalRTPSession::SendReceiveStatus OpalRTPSession::ReadDataPDU(RTP_DataFrame & frame)
{
  PINDEX pduSize = frame.GetSize();
  SendReceiveStatus status = ReadRawPDU(frame.GetPointer(), pduSize, true);
  if (status != e_ProcessPacket)
    return status;

  // Check for single port operation, incoming RTCP on RTP
  RTP_ControlFrame control(frame, pduSize, false);
  unsigned type = control.GetPayloadType();
  if (type < RTP_ControlFrame::e_FirstValidPayloadType || type > RTP_ControlFrame::e_LastValidPayloadType)
    return OnReceiveData(frame, pduSize);

  status = OnReceiveControl(control);
  if (status == e_ProcessPacket)
    status = e_IgnorePacket;
  return status;
}


OpalRTPSession::SendReceiveStatus OpalRTPSession::OnReadTimeout(RTP_DataFrame & /*frame*/)
{
  if (m_connection.OnMediaFailed(m_sessionId, true))
    return e_AbortTransport;

  return e_IgnorePacket;
}


OpalRTPSession::SendReceiveStatus OpalRTPSession::ReadControlPDU()
{
  RTP_ControlFrame frame(2048);

  PINDEX pduSize = frame.GetSize();
  SendReceiveStatus status = ReadRawPDU(frame.GetPointer(), pduSize, m_socket[e_Control] == NULL);
  if (status != e_ProcessPacket)
    return status;

  if (frame.SetPacketSize(pduSize))
    return OnReceiveControl(frame);

  PTRACE_IF(2, pduSize != 1 || !m_firstControl, "Session " << m_sessionId
            << ", Received control packet too small: " << pduSize << " bytes");
  return e_IgnorePacket;
}


bool OpalRTPSession::WriteData(RTP_DataFrame & frame,
                               const PIPSocketAddressAndPort * remote,
                               bool rewriteHeader)
{
  PWaitAndSignal m(m_dataMutex);

  if (!IsOpen() || m_shutdownWrite) {
    PTRACE(3, "Session " << m_sessionId << ", write shutdown.");
    return false;
  }

  switch (OnSendData(frame, rewriteHeader)) {
    case e_ProcessPacket :
      break;
    case e_IgnorePacket :
      return true;
    case e_AbortTransport :
      return false;
  }

  return WriteRawPDU(frame, frame.GetPacketSize(), true, remote);
}


bool OpalRTPSession::WriteControl(RTP_ControlFrame & frame, const PIPSocketAddressAndPort * remote)
{
  PWaitAndSignal m(m_dataMutex);

  if (!IsOpen() || m_shutdownWrite) {
    PTRACE(3, "Session " << m_sessionId << ", write shutdown.");
    return false;
  }

  switch (OnSendControl(frame)) {
    case e_ProcessPacket :
      rtcpPacketsSent++;
      break;
    case e_IgnorePacket :
      return true;
    case e_AbortTransport :
      return false;
  }

  return WriteRawPDU(frame.GetPointer(), frame.GetSize(), m_socket[e_Control] == NULL, remote);
}


bool OpalRTPSession::WriteRawPDU(const BYTE * framePtr, PINDEX frameSize, bool toDataChannel, const PIPSocketAddressAndPort * remote)
{
#if OPAL_ICE
  switch (OnSendICE(toDataChannel)) {
    case e_ProcessPacket :
      break;
    case e_IgnorePacket :
      return true;
    case e_AbortTransport :
      return false;
  }
#endif // OPAL_ICE

  PIPSocketAddressAndPort remoteAddressAndPort;
  if (remote == NULL) {
    remoteAddressAndPort.SetAddress(m_remoteAddress, m_remotePort[toDataChannel]);

    // Trying to send a PDU before we are set up!
    if (!remoteAddressAndPort.IsValid())
      return true;

    remote = &remoteAddressAndPort;
  }

  PUDPSocket & socket = *m_socket[toDataChannel];
  do {
    if (socket.WriteTo(framePtr, frameSize, *remote))
      return true;
  } while (socket.GetErrorCode(PChannel::LastWriteError) == PChannel::Unavailable &&
           HandleUnreachable(PTRACE_PARAM(toDataChannel ? "Data" : "Control")));

  PTRACE(1, "Session " << m_sessionId
          << ", write (" << frameSize << " bytes) error on "
          << (toDataChannel ? "data" : "control") << " port ("
          << socket.GetErrorNumber(PChannel::LastWriteError) << "): "
          << socket.GetErrorText(PChannel::LastWriteError));
  m_connection.OnMediaFailed(m_sessionId, false);
  return false;
}


/////////////////////////////////////////////////////////////////////////////
