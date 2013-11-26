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

#include <algorithm>

#include <h323/h323con.h>

const unsigned SecondsFrom1900to1970 = (70*365+17)*24*60*60U;

#define RTP_VIDEO_RX_BUFFER_SIZE 0x100000 // 1Mb
#define RTP_AUDIO_RX_BUFFER_SIZE 0x4000   // 16kb
#define RTP_DATA_TX_BUFFER_SIZE  0x2000   // 8kb
#define RTP_CTRL_BUFFER_SIZE     0x1000   // 4kb


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
      PTRACE(4, "Jitter\tDestroying jitter buffer " << *this);

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

      PTRACE(6, "Jitter\tOnReadPacket: Frame from network, timestamp " << frame.GetTimestamp());
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
  , m_singlePort(false)
  , m_isAudio(init.m_mediaType == OpalMediaType::Audio())
  , m_timeUnits(m_isAudio ? 8 : 90)
  , m_canonicalName(PProcess::Current().GetUserName())
  , m_toolName(PProcess::Current().GetName())
  , m_maxNoReceiveTime(init.m_connection.GetEndPoint().GetManager().GetNoMediaTimeout())
  , m_maxNoTransmitTime(0, 10)          // Sending data for 10 seconds, ICMP says still not there
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
  , m_localDataPort(0)
  , m_localControlPort(0)
  , m_remoteAddress(PIPSocket::GetInvalidAddress())
  , m_remoteDataPort(0)
  , m_remoteControlPort(0)
  , m_dataSocket(NULL)
  , m_controlSocket(NULL)
  , m_shutdownRead(false)
  , m_shutdownWrite(false)
  , m_remoteBehindNAT(init.m_remoteBehindNAT)
  , m_localHasRestrictedNAT(false)
  , m_noTransmitErrors(0)
{
  ClearStatistics();

  PTRACE_CONTEXT_ID_TO(m_reportTimer);
  m_reportTimer.SetNotifier(PCREATE_NOTIFIER(TimedSendReport));
}


OpalRTPSession::OpalRTPSession(const OpalRTPSession & other)
  : OpalMediaSession(Init(other.m_connection, 0, OpalMediaType(), false))
{
}


OpalRTPSession::~OpalRTPSession()
{
  Close();

#if OPAL_RTCP_XR
  delete m_metrics;
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
      "RTP\tSession " << m_sessionId << ", final statistics:\n"
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

  PObject * channel = transport.RemoveHead();
  m_dataSocket = dynamic_cast<PUDPSocket *>(channel);
  if (m_dataSocket == NULL)
    delete channel;
  else {
    PTRACE_CONTEXT_ID_TO(m_dataSocket);
    m_dataSocket->GetLocalAddress(m_localAddress, m_localDataPort);

    OpalRTPEndPoint * ep = dynamic_cast<OpalRTPEndPoint *>(&m_connection.GetEndPoint());
    if (PAssert(ep != NULL, "RTP createed by non OpalRTPEndPoint derived class"))
      ep->SetConnectionByRtpLocalPort(this, &m_connection);
  }

  channel = transport.RemoveHead();
  m_controlSocket = dynamic_cast<PUDPSocket *>(channel);
  if (m_controlSocket == NULL)
    delete channel;
  else {
    PTRACE_CONTEXT_ID_TO(m_controlSocket);
    m_controlSocket->GetLocalAddress(m_localAddress, m_localControlPort);
  }

  transport.AllowDeleteObjects();
}


OpalMediaSession::Transport OpalRTPSession::DetachTransport()
{
  Transport temp;

  // Stop jitter buffer before detaching
  m_jitterBuffer.SetNULL();

  m_readMutex.Wait();
  m_dataMutex.Wait();

  if (m_dataSocket != NULL) {
    temp.Append(m_dataSocket);
    m_dataSocket = NULL;
  }

  if (m_controlSocket != NULL) {
    temp.Append(m_controlSocket);
    m_controlSocket = NULL;
  }

  m_dataMutex.Signal();
  m_readMutex.Signal();

  PTRACE_IF(2, temp.IsEmpty(), "RTP\tDetaching transport from closed session.");
  return temp;
}


void OpalRTPSession::SendBYE()
{
  {
    PWaitAndSignal mutex(m_dataMutex);
    if (m_byeSent)
      return;

    m_byeSent = true;
  }

  RTP_ControlFrame report;
  InitialiseControlFrame(report);

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
    PTRACE_IF(4, m_jitterBuffer != NULL, "RTP\tSwitching off jitter buffer " << *m_jitterBuffer);
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
    PTRACE(4, "RTP\tCreated RTP jitter buffer " << *m_jitterBuffer);
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

  if (m_outOfOrderPackets.empty())
    return InternalReadData(frame);

  unsigned sequenceNumber = m_outOfOrderPackets.back().GetSequenceNumber();
  if (sequenceNumber != expectedSequenceNumber) {
    PTRACE(5, "RTP\tSession " << m_sessionId << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn)
           << ", still out of order packets, next "
           << sequenceNumber << " expected " << expectedSequenceNumber);
    return InternalReadData(frame);
  }

  frame = m_outOfOrderPackets.back();
  m_outOfOrderPackets.pop_back();
  expectedSequenceNumber = (WORD)(sequenceNumber + 1);

  PTRACE(m_outOfOrderPackets.empty() ? 2 : 5,
         "RTP\tSession " << m_sessionId << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn) << ", resequenced "
         << (m_outOfOrderPackets.empty() ? "last" : "next") << " out of order packet " << sequenceNumber);
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
  
  PTRACE(3, "RTP\tSession " << m_sessionId << ", SentReceiverReport:"
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

  if (rewriteHeader) {
    PTimeInterval tick = PTimer::Tick();  // Timestamp set now

    frame.SetSequenceNumber(++lastSentSequenceNumber);
    frame.SetSyncSource(syncSourceOut);

    // special handling for first packet
    if (!firstPacketSent.IsValid()) {
      firstPacketSent.SetCurrentTime();

      // display stuff
      PTRACE(3, "RTP\tSession " << m_sessionId << ", first sent data:"
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

  if (txStatisticsCount < txStatisticsInterval)
    return e_ProcessPacket;

  txStatisticsCount = 0;

  averageSendTime = averageSendTimeAccum/txStatisticsInterval;
  maximumSendTime = maximumSendTimeAccum;
  minimumSendTime = minimumSendTimeAccum;

  averageSendTimeAccum = 0;
  maximumSendTimeAccum = 0;
  minimumSendTimeAccum = 0xffffffff;

  PTRACE(3, "RTP\tSession " << m_sessionId << ", transmit statistics: "
   " packets=" << packetsSent <<
   " octets=" << octetsSent <<
   " avgTime=" << averageSendTime <<
   " maxTime=" << maximumSendTime <<
   " minTime=" << minimumSendTime
  );

  return e_ProcessPacket;
}

OpalRTPSession::SendReceiveStatus OpalRTPSession::OnSendControl(RTP_ControlFrame & frame)
{
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

  // Check if expected payload type
  if (lastReceivedPayloadType == RTP_DataFrame::IllegalPayloadType)
    lastReceivedPayloadType = frame.GetPayloadType();

  if (lastReceivedPayloadType != frame.GetPayloadType() && !ignorePayloadTypeChanges) {

    PTRACE(4, "RTP\tSession " << m_sessionId << ", got payload type "
           << frame.GetPayloadType() << ", but was expecting " << lastReceivedPayloadType);
    return e_IgnorePacket;
  }

  // Check for if a control packet rather than data packet.
  if (frame.GetPayloadType() > RTP_DataFrame::MaxPayloadType)
    return e_IgnorePacket; // Non fatal error, just ignore

  PTimeInterval tick = PTimer::Tick();  // Get timestamp now

  // Have not got SSRC yet, so grab it now
  if (syncSourceIn == 0)
    syncSourceIn = frame.GetSyncSource();

  // Check packet sequence numbers
  if (packetsReceived == 0) {
    firstPacketReceived.SetCurrentTime();
    PTRACE(3, "RTP\tSession " << m_sessionId << ", first receive data:"
              " ver=" << frame.GetVersion()
           << " pt=" << frame.GetPayloadType()
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

    if ((frame.GetPayloadType() == RTP_DataFrame::T38) &&
        (frame.GetSequenceNumber() >= 0x8000) &&
         (frame.GetPayloadSize() == 0)) {
      PTRACE(4, "RTP\tSession " << m_sessionId << ", ignoring left over audio packet from switch to T.38");
      return e_IgnorePacket; // Non fatal error, just ignore
    }

    expectedSequenceNumber = (WORD)(frame.GetSequenceNumber() + 1);
  }
  else {
    if (frame.GetSyncSource() != syncSourceIn) {
      if (allowAnySyncSource) {
        PTRACE(2, "RTP\tSession " << m_sessionId << ", SSRC changed from "
               << RTP_TRACE_SRC(frame.GetSyncSource()) << " to " << RTP_TRACE_SRC(syncSourceIn));
        syncSourceIn = frame.GetSyncSource();
        allowSequenceChange = true;
      } 
      else if (allowOneSyncSourceChange) {
        PTRACE(2, "RTP\tSession " << m_sessionId << ", allowed one SSRC change from "
                  "SSRC=" << RTP_TRACE_SRC(syncSourceIn) << " to " << RTP_TRACE_SRC(frame.GetSyncSource()));
        syncSourceIn = frame.GetSyncSource();
        allowSequenceChange = true;
        allowOneSyncSourceChange = false;
      }
      else {
        PTRACE(2, "RTP\tSession " << m_sessionId << ", packet from "
                  "SSRC=" << RTP_TRACE_SRC(frame.GetSyncSource()) << " ignored, "
                  "expecting SSRC=" << RTP_TRACE_SRC(syncSourceIn));
        return e_IgnorePacket; // Non fatal error, just ignore
      }
    }

    WORD sequenceNumber = frame.GetSequenceNumber();
    if (sequenceNumber == expectedSequenceNumber) {
      expectedSequenceNumber++;
      consecutiveOutOfOrderPackets = 0;

      if (!m_outOfOrderPackets.empty()) {
        PTRACE(5, "RTP\tSession " << m_sessionId << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn)
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
      m_outOfOrderPackets.clear();
      PTRACE(2, "RTP\tSession " << m_sessionId << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn)
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
        PTRACE(2, "RTP\tSession " << m_sessionId << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn)
               << ", abnormal change of sequence numbers, adjusting to expect " << expectedSequenceNumber);
      }
      else {
        PTRACE(2, "RTP\tSession " << m_sessionId << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn)
               << ", incorrect sequence, got " << sequenceNumber << " expected " << expectedSequenceNumber);

        if (resequenceOutOfOrderPackets)
          return e_IgnorePacket; // Non fatal error, just ignore

        packetsOutOfOrder++;
      }
    }
    else if (resequenceOutOfOrderPackets &&
                (m_outOfOrderPackets.empty() || (tick - outOfOrderPacketTime) < outOfOrderWaitTime)) {
      if (m_outOfOrderPackets.empty())
        outOfOrderPacketTime = tick;
      // Maybe packet lost, maybe out of order, save for now
      SaveOutOfOrderPacket(frame);
      return e_IgnorePacket;
    }
    else {
      if (!m_outOfOrderPackets.empty()) {
        // Give up on the packet, probably never coming in. Save current and switch in
        // the lowest numbered packet.
        SaveOutOfOrderPacket(frame);

        for (;;) {
          if (m_outOfOrderPackets.empty())
            return e_IgnorePacket;

          frame = m_outOfOrderPackets.back();
          m_outOfOrderPackets.pop_back();

          sequenceNumber = frame.GetSequenceNumber();
          if (sequenceNumber >= expectedSequenceNumber)
            break;

          PTRACE(2, "RTP\tSession " << m_sessionId << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn)
                 << ", incorrect sequence after re-ordering, got "
                 << sequenceNumber << " expected " << expectedSequenceNumber);
        }

        outOfOrderPacketTime = tick;
      }

      unsigned dropped = sequenceNumber - expectedSequenceNumber;
      frame.SetDiscontinuity(dropped);
      packetsLost += dropped;
      packetsLostSinceLastRR += dropped;
      PTRACE(2, "RTP\tSession " << m_sessionId << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn)
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

    PTRACE(4, "RTP\tSession " << m_sessionId << ", receive statistics:"
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

  PTRACE(m_outOfOrderPackets.empty() ? 2 : 5,
         "RTP\tSession " << m_sessionId << ", SSRC=" << RTP_TRACE_SRC(syncSourceIn) << ", "
         << (m_outOfOrderPackets.empty() ? "first" : "next") << " out of order packet, got "
         << sequenceNumber << " expected " << expectedSequenceNumber);

  std::list<RTP_DataFrame>::iterator it;
  for (it  = m_outOfOrderPackets.begin(); it != m_outOfOrderPackets.end(); ++it) {
    if (sequenceNumber > it->GetSequenceNumber())
      break;
  }

  m_outOfOrderPackets.insert(it, frame);
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
      PTRACE(3, "RTP\tSession " << m_sessionId << ", SentReceiverReport:"
                " SSRC=" << RTP_TRACE_SRC(syncSourceIn));
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

    PTRACE(3, "RTP\tSession " << m_sessionId << ", SentSenderReport:"
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
  PTRACE(3, "RTP\tSession " << m_sessionId << ", sending SDES: " << m_canonicalName);
  report.StartNewPacket();

  report.SetCount(0); // will be incremented automatically
  report.StartSourceDescription(syncSourceOut);
  report.AddSourceDescriptionItem(RTP_ControlFrame::e_CNAME, m_canonicalName);
  report.AddSourceDescriptionItem(RTP_ControlFrame::e_TOOL, m_toolName);
  report.EndPacket();
  
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

  PTRACE(6, "RTP\tSession " << m_sessionId << ", OnReceiveControl - "
         << frame.GetSize() << " bytes:\n" << hex << setprecision(2) << setfill('0') << frame << dec);

  m_firstControl = false;

  do {
    BYTE * payload = frame.GetPayloadPtr();
    size_t size = frame.GetPayloadSize(); 
    if ((payload == NULL) || (size == 0) || ((payload + size) > (frame.GetPointer() + frame.GetSize()))){
      /* TODO: 1.shall we test for a maximum size ? Indeed but what's the value ? *
               2. what's the correct exit status ? */
      PTRACE(2, "RTP\tSession " << m_sessionId << ", OnReceiveControl invalid frame");
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
          PTRACE(2, "RTP\tSession " << m_sessionId << ", SenderReport packet truncated");
        }
        break;

      case RTP_ControlFrame::e_ReceiverReport :
        if (size >= sizeof(PUInt32b)+frame.GetCount()*sizeof(RTP_ControlFrame::ReceiverReport))
          OnRxReceiverReport(*(const PUInt32b *)payload, BuildReceiverReportArray(frame, sizeof(PUInt32b)));
        else {
          PTRACE(2, "RTP\tSession " << m_sessionId << ", ReceiverReport packet truncated");
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
              PTRACE(4,"RTP\tSession " << m_sessionId << ", SourceDescription item " << item << ", current size = " << uiSizeCurrent);
              
              /* avoid reading where GetNextItem() shall not */
              if (uiSizeCurrent >= size){
                PTRACE(4,"RTP\tSession " << m_sessionId << ", SourceDescription end of items");
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
          PTRACE(2, "RTP\tSession " << m_sessionId << ", SourceDescription packet truncated");
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
              PTRACE(2, "RTP\tSession " << m_sessionId << ", Goodbye packet invalid");
            }
          }

          PDWORDArray sources(count);
          for (size_t i = 0; i < count; i++)
            sources[i] = ((const PUInt32b *)payload)[i];
          OnRxGoodbye(sources, str);
        }
        else {
          PTRACE(2, "RTP\tSession " << m_sessionId << ", Goodbye packet truncated");
        }

        if (m_closeOnBye) {
          PTRACE(3, "RTP\tSession " << m_sessionId << ", Goodbye packet closing transport");
          return e_AbortTransport;
        }
        break;

      case RTP_ControlFrame::e_ApplDefined :
        if (size >= 8)
          OnRxApplDefined(RTP_ControlFrame::ApplDefinedInfo(frame));
        else {
          PTRACE(2, "RTP\tSession " << m_sessionId << ", ApplDefined packet truncated");
        }
        break;

#if OPAL_RTCP_XR
      case RTP_ControlFrame::e_ExtendedReport :
        if (size >= sizeof(PUInt32b)+frame.GetCount()*sizeof(RTP_ControlFrame::ExtendedReport))
          OnRxExtendedReport(*(const PUInt32b *)payload, RTCP_XR_Metrics::BuildExtendedReportArray(frame, sizeof(PUInt32b)));
        else {
          PTRACE(2, "RTP\tSession " << m_sessionId << ", ReceiverReport packet truncated");
        }
        break;
#endif

      case RTP_ControlFrame::e_TransportLayerFeedBack :
        switch (frame.GetFbType()) {
          case RTP_ControlFrame::e_TMMBR :
            if (size >= sizeof(RTP_ControlFrame::FbTMMB)) {
              const RTP_ControlFrame::FbTMMB * tmmb = (const RTP_ControlFrame::FbTMMB *)payload;
              PTRACE(4, "RTP\tSession " << m_sessionId << ", received TMMBR " << tmmb->GetBitRate());
              m_connection.ExecuteMediaCommand(OpalMediaFlowControl(tmmb->GetBitRate()), m_sessionId);
            }
            else {
              PTRACE(2, "RTP\tSession " << m_sessionId << ", TMMBR packet truncated");
            }
            break;

          case RTP_ControlFrame::e_TMMBN :
            if (size >= sizeof(RTP_ControlFrame::FbTMMB)) {
            }
            else {
              PTRACE(2, "RTP\tSession " << m_sessionId << ", TMMBN packet truncated");
            }
            break;
        }
        break;

#if OPAL_VIDEO
      case RTP_ControlFrame::e_IntraFrameRequest :
        PTRACE(4, "RTP\tSession " << m_sessionId << ", received RFC2032 FIR");
        m_connection.OnRxIntraFrameRequest(*this, true);
        break;

      case RTP_ControlFrame::e_PayloadSpecificFeedBack :
        switch (frame.GetFbType()) {
          case RTP_ControlFrame::e_PictureLossIndication :
            PTRACE(4, "RTP\tSession " << m_sessionId << ", received RFC5104 PLI");
            m_connection.OnRxIntraFrameRequest(*this, false);
            break;

          case RTP_ControlFrame::e_FullIntraRequest :
            PTRACE(4, "RTP\tSession " << m_sessionId << ", received RFC5104 FIR");
            m_connection.OnRxIntraFrameRequest(*this, true);
            break;

          default :
            PTRACE(2, "RTP\tSession " << m_sessionId << ", Unknown Payload Specific feedback type: " << frame.GetFbType());
        }
        break;
  #endif

      default :
        PTRACE(2, "RTP\tSession " << m_sessionId << ", Unknown control payload type: " << frame.GetPayloadType());
    }
  } while (frame.ReadNextPacket());

  return e_ProcessPacket;
}


void OpalRTPSession::OnRxSenderReport(const SenderReport & sender, const ReceiverReportArray & reports)
{
  m_syncTimestamp = sender.rtpTimestamp;
  m_syncRealTime = sender.realTimestamp;

#if PTRACING
  if (PTrace::CanTrace(3)) {
    ostream & strm = PTrace::Begin(3, __FILE__, __LINE__, this);
    strm << "RTP\tSession " << m_sessionId << ", OnRxSenderReport: " << sender << '\n';
    for (PINDEX i = 0; i < reports.GetSize(); i++)
      strm << "  RR: " << reports[i] << '\n';
    strm << PTrace::End;
  }
#endif

  OnReceiverReports(reports);
}


void OpalRTPSession::OnRxReceiverReport(DWORD PTRACE_PARAM(src), const ReceiverReportArray & reports)
{
#if PTRACING
  if (PTrace::CanTrace(3)) {
    ostream & strm = PTrace::Begin(2, __FILE__, __LINE__, this);
    strm << "RTP\tSession " << m_sessionId << ", OnReceiverReport: SSRC=" << RTP_TRACE_SRC(src) << '\n';
    for (PINDEX i = 0; i < reports.GetSize(); i++)
      strm << "  RR: " << reports[i] << '\n';
    strm << PTrace::End;
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
  if (PTrace::CanTrace(3)) {
    ostream & strm = PTrace::Begin(3, __FILE__, __LINE__, this);
    strm << "RTP\tSession " << m_sessionId << ", OnSourceDescription: " << description.GetSize() << " entries";
    for (PINDEX i = 0; i < description.GetSize(); i++)
      strm << "\n  " << description[i];
    strm << PTrace::End;
  }
#endif
}


void OpalRTPSession::OnRxGoodbye(const PDWORDArray & PTRACE_PARAM(src), const PString & PTRACE_PARAM(reason))
{
  PTRACE(3, "RTP\tSession " << m_sessionId << ", OnGoodbye: \"" << reason << "\" srcs=" << src);
}


void OpalRTPSession::OnRxApplDefined(const RTP_ControlFrame::ApplDefinedInfo & info)
{
  PTRACE(3, "RTP\tSession " << m_sessionId << ", OnApplDefined: \""
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
  PTRACE(3, "RTP\tSession " << m_sessionId << ", SendFlowControl(" << maxBitRate << ") using TMMBR");
  // Create packet
  RTP_ControlFrame request;
  InitialiseControlFrame(request);

  request.StartNewPacket();

  request.SetPayloadType(RTP_ControlFrame::e_TransportLayerFeedBack);
  request.SetFbType(notify ? RTP_ControlFrame::e_TMMBN : RTP_ControlFrame::e_TMMBR, sizeof(RTP_ControlFrame::FbTMMB));

  RTP_ControlFrame::FbTMMB * tmmb = (RTP_ControlFrame::FbTMMB *)request.GetPayloadPtr();
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
  PTRACE(3, "RTP\tSession " << m_sessionId << ", SendIntraFrameRequest using "
         << (rfc2032 ? "RFC2032" : (pictureLoss ? "RFC4585 PLI" : "RFC5104 FIR")));

  // Create packet
  RTP_ControlFrame request;
  InitialiseControlFrame(request);

  request.StartNewPacket();

  if (rfc2032) {
    // Create packet
    request.SetPayloadType(RTP_ControlFrame::e_IntraFrameRequest);
    request.SetPayloadSize(4);
    // Insert SSRC
    request.SetCount(1);
    BYTE * payload = request.GetPayloadPtr();
    *(PUInt32b *)payload = syncSourceOut;
  }
  else {
    request.SetPayloadType(RTP_ControlFrame::e_PayloadSpecificFeedBack);
    if (pictureLoss)
      request.SetFbType(RTP_ControlFrame::e_PictureLossIndication, sizeof(RTP_ControlFrame::FbFCI));
    else {
      request.SetFbType(RTP_ControlFrame::e_FullIntraRequest, sizeof(RTP_ControlFrame::FbFIR));
      RTP_ControlFrame::FbFIR * fir = (RTP_ControlFrame::FbFIR *)request.GetPayloadPtr();
      fir->requestSSRC = syncSourceIn;
    }
    RTP_ControlFrame::FbFCI * fci = (RTP_ControlFrame::FbFCI *)request.GetPayloadPtr();
    fci->senderSSRC = syncSourceOut;
  }

  // Send it
  request.EndPacket();
  WriteControl(request);
}


void OpalRTPSession::SendTemporalSpatialTradeOff(unsigned tradeOff)
{
  PTRACE(3, "RTP\tSession " << m_sessionId << ", SendTemporalSpatialTradeOff " << tradeOff);

  RTP_ControlFrame request;
  InitialiseControlFrame(request);

  request.StartNewPacket();

  request.SetPayloadType(RTP_ControlFrame::e_PayloadSpecificFeedBack);
  request.SetFbType(RTP_ControlFrame::e_TemporalSpatialTradeOffRequest, sizeof(RTP_ControlFrame::FbTSTO));
  RTP_ControlFrame::FbTSTO * tsto = (RTP_ControlFrame::FbTSTO *)request.GetPayloadPtr();
  tsto->requestSSRC = syncSourceIn;
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
    PTRACE(1, "RTP_UDP\tGetOption(" << sock.GetHandle() << ',' << bufTypeName << ")"
              " failed: " << sock.GetErrorText());
    return;
  }

  // Already big enough
  if (originalSize >= newSize) {
    PTRACE(4, "RTP_UDP\tSetOption(" << sock.GetHandle() << ',' << bufTypeName << ',' << newSize << ")"
              " unecessary, already " << originalSize);
    return;
  }

  for (; newSize >= 1024; newSize -= newSize/10) {
    // Set to new size
    if (!sock.SetOption(bufType, newSize)) {
      PTRACE(1, "RTP_UDP\tSetOption(" << sock.GetHandle() << ',' << bufTypeName << ',' << newSize << ")"
                " failed: " << sock.GetErrorText());
      continue;
    }

    // As some stacks lie about setting the buffer size, we double check.
    int adjustedSize;
    if (!sock.GetOption(bufType, adjustedSize)) {
      PTRACE(1, "RTP_UDP\tGetOption(" << sock.GetHandle() << ',' << bufTypeName << ")"
                " failed: " << sock.GetErrorText());
      return;
    }

    if (adjustedSize >= newSize) {
      PTRACE(4, "RTP_UDP\tSetOption(" << sock.GetHandle() << ',' << bufTypeName << ',' << newSize << ")"
                " succeeded, actually " << adjustedSize);
      return;
    }

    if (adjustedSize > originalSize) {
      PTRACE(4, "RTP_UDP\tSetOption(" << sock.GetHandle() << ',' << bufTypeName << ',' << newSize << ")"
                " clamped to maximum " << adjustedSize);
      return;
    }

    PTRACE(2, "RTP_UDP\tSetOption(" << sock.GetHandle() << ',' << bufTypeName << ',' << newSize << ")"
              " failed, even though it said it succeeded!");
  }
}


OpalTransportAddress OpalRTPSession::GetLocalAddress(bool isMediaAddress) const
{
  WORD port = isMediaAddress ? m_localDataPort : m_localControlPort;
  if (m_localAddress.IsValid() && port != 0)
    return OpalTransportAddress(m_localAddress, port, OpalTransportAddress::UdpPrefix());
  else
    return OpalTransportAddress();
}


OpalTransportAddress OpalRTPSession::GetRemoteAddress(bool isMediaAddress) const
{
  WORD port = isMediaAddress ? m_remoteDataPort : m_remoteControlPort;
  if (m_remoteAddress.IsValid() && port != 0)
    return OpalTransportAddress(m_remoteAddress, port, OpalTransportAddress::UdpPrefix());
  else
    return OpalTransportAddress();
}


OpalMediaStream * OpalRTPSession::CreateMediaStream(const OpalMediaFormat & mediaFormat, 
                                                    unsigned sessionId, 
                                                    bool isSource)
{
  if (m_dataSocket != NULL) {
    PIPSocket::QoS qos = m_connection.GetEndPoint().GetManager().GetMediaQoS(m_mediaType);
    qos.m_remote.SetAddress(m_remoteAddress, m_remoteDataPort);

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
    m_dataSocket->SetQoS(qos);
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

  delete m_dataSocket;
  delete m_controlSocket;
  m_dataSocket = NULL;
  m_controlSocket = NULL;

  PIPSocket::Address bindingAddress(localInterface);

  OpalManager & manager = m_connection.GetEndPoint().GetManager();

#if P_NAT
  PNatMethod * natMethod = manager.GetNatMethods().GetMethod(bindingAddress, this);
  if (natMethod != NULL) {
    PTRACE(4, "RTP\tNAT Method " << natMethod->GetMethodName() << " selected for call.");

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
        PTRACE ( 4, "RTP\tAttempting natMethod: " << natMethod->GetMethodName());            
        if (m_singlePort) {
          if (natMethod->CreateSocket(m_dataSocket, bindingAddress))
            m_dataSocket->GetLocalAddress(m_localAddress, m_localDataPort);
          else {
            delete m_dataSocket;
            m_dataSocket = NULL;
            PTRACE(2, "RTP\tSession " << m_sessionId << ", " << natMethod->GetMethodName()
                    << " could not create NAT RTP socket, using normal sockets.");
          }
        }
        else if (natMethod->CreateSocketPair(m_dataSocket, m_controlSocket, bindingAddress, this)) {
          PTRACE(4, "RTP\tSession " << m_sessionId << ", " << natMethod->GetMethodName() << " created NAT RTP/RTCP socket pair.");
          m_dataSocket->GetLocalAddress(m_localAddress, m_localDataPort);
          m_controlSocket->GetLocalAddress(m_localAddress, m_localControlPort);
        }
        else {
          PTRACE(2, "RTP\tSession " << m_sessionId << ", " << natMethod->GetMethodName()
                  << " could not create NAT RTP/RTCP socket pair; trying to create individual sockets.");
          if (natMethod->CreateSocket(m_dataSocket, bindingAddress, 0, this) &&
              natMethod->CreateSocket(m_controlSocket, bindingAddress, 0, this)) {
            m_dataSocket->GetLocalAddress(m_localAddress, m_localDataPort);
            m_controlSocket->GetLocalAddress(m_localAddress, m_localControlPort);
          }
          else {
            delete m_dataSocket;
            delete m_controlSocket;
            m_dataSocket = NULL;
            m_controlSocket = NULL;
            PTRACE(2, "RTP\tSession " << m_sessionId << ", " << natMethod->GetMethodName()
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
#endif

  if (m_dataSocket == NULL) {
    m_dataSocket = new PUDPSocket();
    if (m_singlePort) {
      // If media and control address the same, then we are only using one port.
      if (manager.GetRtpIpPortRange().Listen(*m_dataSocket, bindingAddress))
        m_localControlPort = m_localDataPort = m_dataSocket->GetPort();
    }
    else {
      m_controlSocket = new PUDPSocket();

      PIPSocket * sockets[2] = { m_dataSocket, m_controlSocket };
      if (manager.GetRtpIpPortRange().Listen(sockets, 2, bindingAddress)) {
        m_localDataPort = m_dataSocket->GetPort();
        m_localControlPort = m_controlSocket->GetPort();
      }
    }

    if (!m_dataSocket->IsOpen()) {
      PTRACE(1, "RTPCon\tNo ports available for RTP session " << m_sessionId << ","
                " base=" << manager.GetRtpIpPortBase() << ","
                " max=" << manager.GetRtpIpPortMax() << ","
                " bind=" << bindingAddress << ","
                " for " << m_connection);
      return false; // Used up all the available ports!
    }

    m_localAddress = bindingAddress;
  }

  PTRACE_CONTEXT_ID_TO(m_dataSocket);
  PTRACE_CONTEXT_ID_TO(m_controlSocket);

  m_dataSocket->SetReadTimeout(m_maxNoReceiveTime);

#ifndef __BEOS__
  // Increase internal buffer size on media UDP sockets
  SetMinBufferSize(*m_dataSocket, SO_RCVBUF, m_isAudio ? RTP_AUDIO_RX_BUFFER_SIZE : RTP_VIDEO_RX_BUFFER_SIZE);
  SetMinBufferSize(*m_dataSocket, SO_SNDBUF, RTP_DATA_TX_BUFFER_SIZE);
  if (m_controlSocket != NULL) {
    SetMinBufferSize(*m_controlSocket, SO_RCVBUF, RTP_CTRL_BUFFER_SIZE);
    SetMinBufferSize(*m_controlSocket, SO_SNDBUF, RTP_CTRL_BUFFER_SIZE);
  }
#endif

  if (m_canonicalName.Find('@') == P_MAX_INDEX)
    m_canonicalName += '@' + GetLocalHostName();

  manager.TranslateIPAddress(m_localAddress, m_remoteAddress);

  PTRACE(3, "RTP_UDP\tSession " << m_sessionId << " opened: "
         << m_localAddress << ':' << m_localDataPort << '-' << m_localControlPort
         << " SSRC=" << RTP_TRACE_SRC(syncSourceOut));

  OpalRTPEndPoint * ep = dynamic_cast<OpalRTPEndPoint *>(&m_connection.GetEndPoint());
  if (PAssert(ep != NULL, "RTP createed by non OpalRTPEndPoint derived class"))
    ep->SetConnectionByRtpLocalPort(this, &m_connection);

  return true;
}


bool OpalRTPSession::IsOpen() const
{
  return m_dataSocket != NULL && m_dataSocket->IsOpen() &&
         (m_controlSocket == NULL || m_controlSocket->IsOpen());
}


bool OpalRTPSession::Close()
{
  bool ok = Shutdown(true) | Shutdown(false);

  m_reportTimer.Stop(true);

  OpalRTPEndPoint * ep = dynamic_cast<OpalRTPEndPoint *>(&m_connection.GetEndPoint());
  if (ep != NULL)
    ep->SetConnectionByRtpLocalPort(this, NULL);

  // We need to do this to make sure that the sockets are not
  // deleted before select decides there is no more data coming
  // over them and exits the reading thread.
  SetJitterBufferSize(OpalJitterBuffer::Init());

  m_readMutex.Wait();
  m_dataMutex.Wait();

  delete m_dataSocket;
  m_dataSocket = NULL;

  delete m_controlSocket;
  m_controlSocket = NULL;

  m_dataMutex.Signal();
  m_readMutex.Signal();

  m_localAddress = PIPSocket::GetInvalidAddress();
  m_localDataPort = m_localControlPort = 0;
  m_remoteAddress = PIPSocket::GetInvalidAddress();
  m_remoteDataPort = m_remoteControlPort = 0;

  return ok;
}


bool OpalRTPSession::Shutdown(bool reading)
{
  if (reading) {
    {
      PWaitAndSignal mutex(m_dataMutex);

      if (m_shutdownRead) {
        PTRACE(4, "RTP_UDP\tSession " << m_sessionId << ", read already shut down .");
        return false;
      }

      PTRACE(3, "RTP_UDP\tSession " << m_sessionId << ", shutting down read.");

      syncSourceIn = 0;
      m_shutdownRead = true;

      if (m_dataSocket != NULL) {
        PIPSocketAddressAndPort addrAndPort;
        m_dataSocket->PUDPSocket::InternalGetLocalAddress(addrAndPort);
        if (!addrAndPort.IsValid())
          addrAndPort.Parse(PIPSocket::GetHostName());
        BYTE dummy = 0;
        PUDPSocket::Slice slice(&dummy, 1);
        if (!m_dataSocket->PUDPSocket::InternalWriteTo(&slice, 1, addrAndPort)) {
          PTRACE(1, "RTP_UDP\tSession " << m_sessionId << ", could not write to unblock read socket: "
                 << m_dataSocket->GetErrorText(PChannel::LastReadError));
          m_dataSocket->Close();
        }
      }
    }

    SetJitterBufferSize(OpalJitterBuffer::Init()); // Kill jitter buffer too, but outside mutex
  }
  else {
    if (m_shutdownWrite) {
      PTRACE(4, "RTP_UDP\tSession " << m_sessionId << ", write already shut down .");
      return false;
    }

    PTRACE(3, "RTP_UDP\tSession " << m_sessionId << ", shutting down write.");
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

  PTRACE(3, "RTP_UDP\tSession " << m_sessionId << " reopened for " << (reading ? "reading" : "writing"));
}


PString OpalRTPSession::GetLocalHostName()
{
  return PIPSocket::GetHostName();
}


bool OpalRTPSession::SetRemoteAddress(const OpalTransportAddress & remoteAddress, bool isMediaAddress)
{
  PWaitAndSignal m(m_dataMutex);

  if (m_remoteBehindNAT) {
    PTRACE(2, "RTP_UDP\tSession " << m_sessionId << ", ignoring remote socket info as remote is behind NAT");
    return true;
  }

  PIPSocket::Address address;
  WORD port = isMediaAddress ? m_remoteDataPort : m_remoteControlPort;
  if (!remoteAddress.GetIpAndPort(address, port))
    return false;

  PTRACE(3, "RTP_UDP\tSession " << m_sessionId << ", Set remote "
         << (isMediaAddress ? "data" : "control") << " address, "
            "new=" << address << ':' << port << ", "
            "old=" << m_remoteAddress << ':' << m_remoteDataPort << '-' << m_remoteControlPort << ", "
            "local=" << m_localAddress << ':' << m_localDataPort << '-' << m_localControlPort);

  if (m_localAddress == address && m_remoteAddress == address && (isMediaAddress ? m_localDataPort : m_localControlPort) == port)
    return true;

  m_remoteAddress = address;
  
  allowOneSyncSourceChange = true;
  allowSequenceChange = packetsReceived != 0;

  if (port != 0) {
  if (isMediaAddress) {
      m_remoteDataPort = port;
      if ((port&1) == 0 && m_remoteControlPort == 0)
        m_remoteControlPort = (WORD)(port + 1);
  }
  else {
      m_remoteControlPort = port;
      if ((port&1) == 1 && m_remoteDataPort == 0)
        m_remoteDataPort = (WORD)(port - 1);
  }
    m_singlePort = m_remoteDataPort == m_remoteControlPort;
  PTRACE_IF(3, m_singlePort, "RTP_UDP\tSession " << m_sessionId << ", single port mode");
  }

  if (m_localHasRestrictedNAT) {
    PTRACE(2, "RTP_UDP\tSession " << m_sessionId << ", sending empty datagrams to open local restricted NAT");
    // If have Port Restricted NAT on local host then send a datagram
    // to remote to open up the port in the firewall for return data.
    static const BYTE dummy[1] = { 0 };
    WriteRawPDU(dummy, sizeof(dummy), true);
    if (m_controlSocket != NULL)
      WriteRawPDU(dummy, sizeof(dummy), false);
  }

  return true;
}


bool OpalRTPSession::InternalReadData(RTP_DataFrame & frame)
{
  PWaitAndSignal mutex(m_readMutex);

  SendReceiveStatus receiveStatus = e_IgnorePacket;
  while (receiveStatus == e_IgnorePacket) {
    if (m_shutdownRead || PAssertNULL(m_dataSocket) == NULL)
      return false;

    if (m_controlSocket == NULL)
      receiveStatus = ReadDataPDU(frame);
    else {
      int selectStatus = PSocket::Select(*m_dataSocket, *m_controlSocket, m_maxNoReceiveTime);

      if (m_shutdownRead)
        return false;

      if (selectStatus > 0) {
        PTRACE(1, "RTP_UDP\tSession " << m_sessionId << ", Select error: "
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


OpalRTPSession::SendReceiveStatus OpalRTPSession::ReadRawPDU(BYTE * framePtr,
                                                             PINDEX & frameSize,
                                                             bool fromDataChannel)
{
#if PTRACING
  const char * channelName = fromDataChannel ? "Data" : "Control";
#endif
  PUDPSocket & socket = *(fromDataChannel ? m_dataSocket : m_controlSocket);
  PIPSocket::Address addr;
  WORD port;

  if (socket.ReadFrom(framePtr, frameSize, addr, port)) {
    // Ignore one byte packet, likely from the block breaker in OpalRTPSession::Shutdown()
    if (socket.GetLastReadCount() == 1 && addr == m_localAddress)
      return e_IgnorePacket;

    // If remote address never set from higher levels, then try and figure
    // it out from the first packet received.
    if (!m_remoteAddress.IsValid()) {
      m_remoteAddress = addr;
      PTRACE(4, "RTP\tSession " << m_sessionId << ", set remote address from first "
             << channelName << " PDU from " << addr << ':' << port);
    }
    if (fromDataChannel) {
      if (m_remoteDataPort == 0)
        m_remoteDataPort = port;
    }
    else {
      if (m_remoteControlPort == 0)
        m_remoteControlPort = port;
    }

    m_noTransmitErrors = 0;
    frameSize = socket.GetLastReadCount();

    return e_ProcessPacket;
  }

  switch (socket.GetErrorCode(PChannel::LastReadError)) {
    case PChannel::Unavailable :
      if (!HandleUnreachable(PTRACE_PARAM(channelName)))
        Shutdown(false); // Terminate transmission
      return e_IgnorePacket;

    case PChannel::BufferTooSmall :
      PTRACE(2, "RTP_UDP\tSession " << m_sessionId << ", " << channelName
             << " read packet too large for buffer of " << frameSize << " bytes.");
      return e_IgnorePacket;

    case PChannel::Interrupted :
      PTRACE(4, "RTP_UDP\tSession " << m_sessionId << ", " << channelName
             << " read packet interrupted.");
      // Shouldn't happen, but it does.
      return e_IgnorePacket;

    case PChannel::NoError :
      PTRACE(3, "RTP_UDP\tSession " << m_sessionId << ", " << channelName
             << " received UDP packet with no payload.");
      return e_IgnorePacket;

    default:
      PTRACE(1, "RTP_UDP\tSession " << m_sessionId << ", " << channelName
             << " read error (" << socket.GetErrorNumber(PChannel::LastReadError) << "): "
             << socket.GetErrorText(PChannel::LastReadError));
      m_connection.OnMediaFailed(m_sessionId, true);
      return e_AbortTransport;
  }
}


bool OpalRTPSession::HandleUnreachable(PTRACE_PARAM(const char * channelName))
{
  if (++m_noTransmitErrors == 1) {
    PTRACE(2, "RTP_UDP\tSession " << m_sessionId << ", " << channelName << " port on remote not ready.");
    m_noTransmitTimer = m_maxNoTransmitTime;
    return true;
  }

  if (m_noTransmitErrors < 10 || m_noTransmitTimer.IsRunning())
    return true;

  PTRACE(2, "RTP_UDP\tSession " << m_sessionId << ", " << channelName << ' '
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
  SendReceiveStatus status = ReadRawPDU(frame.GetPointer(), pduSize, m_controlSocket == NULL);
  if (status != e_ProcessPacket)
    return status;

  if (frame.SetPacketSize(pduSize))
    return OnReceiveControl(frame);

  PTRACE_IF(2, pduSize != 1 || !m_firstControl, "RTP_UDP\tSession " << m_sessionId
            << ", Received control packet too small: " << pduSize << " bytes");
  return e_IgnorePacket;
}


bool OpalRTPSession::WriteData(RTP_DataFrame & frame,
                               const PIPSocketAddressAndPort * remote,
                               bool rewriteHeader)
{
  PWaitAndSignal m(m_dataMutex);

  if (!IsOpen() || m_shutdownWrite) {
    PTRACE(3, "RTP_UDP\tSession " << m_sessionId << ", write shutdown.");
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

  switch (OnSendControl(frame)) {
    case e_ProcessPacket :
      rtcpPacketsSent++;
      break;
    case e_IgnorePacket :
      return true;
    case e_AbortTransport :
      return false;
  }

  return WriteRawPDU(frame.GetPointer(), frame.GetSize(), m_controlSocket == NULL, remote);
}


bool OpalRTPSession::WriteRawPDU(const BYTE * framePtr, PINDEX frameSize, bool toDataChannel, const PIPSocketAddressAndPort * remote)
{
  PUDPSocket & socket = *(toDataChannel ? m_dataSocket : m_controlSocket);

  PIPSocketAddressAndPort remoteAddressAndPort;
  if (remote == NULL) {
    remoteAddressAndPort.SetAddress(m_remoteAddress, toDataChannel ? m_remoteDataPort : m_remoteControlPort);

    // Trying to send a PDU before we are set up!
    if (!remoteAddressAndPort.IsValid())
      return true;

    remote = &remoteAddressAndPort;
  }

  while (!socket.WriteTo(framePtr, frameSize, *remote)) {
    switch (socket.GetErrorCode(PChannel::LastWriteError)) {
      case PChannel::Unavailable :
        if (HandleUnreachable(PTRACE_PARAM(toDataChannel ? "Data" : "Control")))
          break;
        return false;

      default:
        PTRACE(1, "RTP_UDP\tSession " << m_sessionId
               << ", write (" << frameSize << " bytes) error on "
               << (toDataChannel ? "data" : "control") << " port ("
               << socket.GetErrorNumber(PChannel::LastWriteError) << "): "
               << socket.GetErrorText(PChannel::LastWriteError));
        m_connection.OnMediaFailed(m_sessionId, false);
        return false;
    }
  }

  return true;
}


/////////////////////////////////////////////////////////////////////////////
