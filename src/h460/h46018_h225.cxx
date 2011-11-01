/*
* h460_h225.cxx
*
* H.460.18 H225 NAT Traversal class.
*
* h323plus library
*
* Copyright (c) 2008 ISVO (Asia) Pte. Ltd.
*
* The contents of this file are subject to the Mozilla Public License
* Version 1.1 (the "License"); you may not use this file except in
* compliance with the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Alternatively, the contents of this file may be used under the terms
* of the General Public License (the  "GNU License"), in which case the
* provisions of GNU License are applicable instead of those
* above. If you wish to allow use of your version of this file only
* under the terms of the GNU License and not to allow others to use
* your version of this file under the MPL, indicate your decision by
* deleting the provisions above and replace them with the notice and
* other provisions required by the GNU License. If you do not delete
* the provisions above, a recipient may use your version of this file
* under either the MPL or the GNU License."
*
* Software distributed under the License is distributed on an "AS IS"
* basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
* the License for the specific language governing rights and limitations
* under the License.
*
*
* The Initial Developer of the Original Code is ISVO (Asia) Pte. Ltd.
*
* Portions of this code were written with the assisance of funding from
* triple-IT. http://www.triple-it.nl.
*
* Contributor(s): ______________________________________.
*
* $Revision$
* $Author$
* $Date$
*/

#include "ptlib.h"
#include <opal/buildopts.h>

#ifdef OPAL_H460

#include <h323/h323ep.h>
#include <h323/h323pdu.h>
#include <h323/gkclient.h>
#include <h460/h46018_h225.h>
#include <h460/h46018.h>
#include <ptclib/random.h>
#include <ptclib/cypher.h>

PCREATE_NAT_PLUGIN(H46019);

#ifdef ANDROID
extern void android_sha1( void*, const PString &);
#endif


///////////////////////////////////////////////////////////////////////////////////

// Listening/Keep Alive Thread

class H46018TransportThread : public PThread
{
  PCLASSINFO(H46018TransportThread, PThread)

public:
  H46018TransportThread(OpalEndPoint & endpoint, H46018Transport * transport);

protected:
  void Main();

  PBoolean    isConnected;
  H46018Transport * transport;

  PTime   lastupdate;
};


/////////////////////////////////////////////////////////////////////////////

H46018TransportThread::H46018TransportThread(OpalEndPoint & ep, H46018Transport * t)
  : PThread(1000, AutoDeleteThread,
  NormalPriority,"H225 Answer:%0x"),transport(t)
{  
  isConnected = false;

  // Start the Thread
  Resume();
}


void H46018TransportThread::Main()
{
  PTRACE(3, "H46018\tStarted Listening Thread");

  PBoolean ret = true;
  while ((transport->IsOpen()) &&        // Transport is Open
    (!isConnected) &&            // Does not have a call connection 
    (ret) &&                            // is not a Failed connection
    (!transport->CloseTransport())) {    // not close due to shutdown
      ret = transport->HandleH46018SignallingChannelPDU();

      if (!ret && transport->CloseTransport()) {  // Closing down Instruction
        PTRACE(3, "H46018\tShutting down H46018 Thread");
        transport->ConnectionLost(true);
      } 
  }

  PTRACE(3, "H46018\tTransport Closed");
}


///////////////////////////////////////////////////////////////////////////////////////

H46018SignalPDU::H46018SignalPDU(const OpalGloballyUniqueID & callIdentifier)
{
  // Build facility msg
  q931pdu.BuildFacility(0, FALSE);

  // Build the UUIE
  m_h323_uu_pdu.m_h323_message_body.SetTag(H225_H323_UU_PDU_h323_message_body::e_facility);
  H225_Facility_UUIE & fac = m_h323_uu_pdu.m_h323_message_body;

  PString version = "0.0.8.2250.0." + PString(H225_PROTOCOL_VERSION);
  fac.m_protocolIdentifier.SetValue(version);
  fac.m_reason.SetTag(H225_FacilityReason::e_undefinedReason);
  fac.IncludeOptionalField(H225_Facility_UUIE::e_callIdentifier);
  fac.m_callIdentifier.m_guid = callIdentifier;

  // Put UUIE into user-user of Q931
  BuildQ931();
}


//////////////////////////////////////////////////////////////////////////////////////

H46018Transport::H46018Transport(H323EndPoint & endpoint)    
  : OpalTransportTCP(endpoint)
{
  ReadTimeOut = PMaxTimeInterval;
  isConnected = false;
  closeTransport = false;
  remoteShutDown = false;
}


H46018Transport::~H46018Transport()
{
  Close();
}


PBoolean H46018Transport::HandleH46018SignallingSocket(H323SignalPDU & pdu)
{
  for (;;) {

    if (!IsOpen())
      return false;

    H323SignalPDU rpdu;
    if (!rpdu.Read(*this)) { 
      PTRACE(3, "H46018\tSocket Read Failure");
      if (GetErrorNumber(PChannel::LastReadError) == 0) {
        PTRACE(3, "H46018\tRemote SHUT DOWN or Intermediary Shutdown!");
        remoteShutDown = true;
      }
      return false;
    } else if (rpdu.GetQ931().GetMessageType() == Q931::SetupMsg) {
      pdu = rpdu;
      return true;
    } else {
      PTRACE(3, "H46018\tUnknown PDU Received");
      return false;
    }

  }
}


PBoolean H46018Transport::HandleH46018SignallingChannelPDU()
{

  H323SignalPDU pdu;
  if (!HandleH46018SignallingSocket(pdu)) {
    if (remoteShutDown) {   // Intentional Shutdown?
      Close();
    }
    return false;
  }

  // We are connected
  isConnected = true;

  // Process the Tokens
  unsigned callReference = pdu.GetQ931().GetCallReference();
  //PSafePtr<H323EndPoint> h323endpoint = PSafePtrCast<OpalEndPoint,H323EndPoint>(&endpoint);
  H323EndPoint *h323endpoint = dynamic_cast<H323EndPoint*>(&endpoint);

  PString token = GetRemoteAddress();
  token.sprintf("/%u", callReference);

  OpalCall *call = endpoint.GetManager().InternalCreateCall();
  H323Connection *connection = h323endpoint->CreateConnection (
    *call,
    token,
    NULL,
    *this,
    PString::Empty(),
    h323endpoint->GetGatekeeper()->GetTransport().GetRemoteAddress(),
    &pdu
  );

  /* H323Connection * connection = h323endpoint->CreateConnection(callReference, NULL, this, &pdu); */
  //        if (connection == NULL) {
  if (!h323endpoint->AddConnection(connection)) {
    PTRACE(1, "H46018\tEndpoint could not create connection, " <<
      "sending release complete PDU: callRef=" << callReference);
    Q931 pdu;
    pdu.BuildReleaseComplete(callReference, true);
    PBYTEArray rawData;
    pdu.Encode(rawData);
    WritePDU(rawData);
    return true;
  } 

  PTRACE(3, "H46018\tCreated new connection: " << token);
  // NOTE I think this is the equiv of AddConnection above
  //connectionsMutex.Wait();
  //static_cast<H323EndPoint*>(&GetEndPoint())->GetConnections().SetAt(token, connection);
  //connectionsMutex.Signal();

  connection->AttachSignalChannel(token, this, true);

  PThread * thread = PThread::Current();
  AttachThread(thread);
  thread->SetNoAutoDelete();

  if (connection->HandleSignalPDU(pdu)) {
    // All subsequent PDU's should wait forever
    SetReadTimeout(PMaxTimeInterval);
    connection->HandleSignallingChannel();
  }
  else {
    connection->ClearCall(H323Connection::EndedByTransportFail);
    PTRACE(1, "H46018\tSignal channel stopped on first PDU.");
  }

  return true;
}


PBoolean H46018Transport::WritePDU( const PBYTEArray & pdu )
{
  PWaitAndSignal m(WriteMutex);
  return OpalTransportTCP::WritePDU(pdu);

}


PBoolean H46018Transport::ReadPDU(PBYTEArray & pdu)
{
  return OpalTransportTCP::ReadPDU(pdu);
}


PBoolean H46018Transport::Connect(const OpalGloballyUniqueID & callIdentifier) 
{ 
  PTRACE(4, "H46018\tConnecting to H.460.18 Server");

  if (!OpalTransportTCP::Connect())
    return false;

  return InitialPDU(callIdentifier);
}


void H46018Transport::ConnectionLost(PBoolean established)
{
  if (closeTransport)
    return;
}


PBoolean H46018Transport::IsConnectionLost()  
{ 
  return false; 
}


PBoolean H46018Transport::InitialPDU(const OpalGloballyUniqueID & callIdentifier)
{
  PWaitAndSignal mutex(IntMutex);

  if (!IsOpen())
    return false;

  H46018SignalPDU pdu(callIdentifier);

  PTRACE(6, "H46018\tCall Facility PDU: " << pdu);

  PBYTEArray rawData;
  pdu.GetQ931().Encode(rawData);

  if (!WritePDU(rawData)) {
    PTRACE(3, "H46018\tError Writing PDU.");
    return false;
  }

  PTRACE(4, "H46018\tSent PDU Call: " << callIdentifier.AsString() << " awaiting response.");

  return true;
}


PBoolean H46018Transport::Close() 
{ 
  PTRACE(4, "H46018\tClosing H46018 NAT channel.");    
  closeTransport = true;
  return OpalTransportTCP::Close(); 
}


PBoolean H46018Transport::IsOpen () const
{
  return OpalTransportTCP::IsOpen();
}


PBoolean H46018Transport::IsListening() const
{      
  if (isConnected)
    return false;

  // NOTE this is most likely incorrect...
  return IsOpen();
  //if (h245listener == NULL)
  //    return false;

  //return h245listener->IsOpen();
}


/////////////////////////////////////////////////////////////////////////////

H46018Handler::H46018Handler(H323EndPoint * ep)
  : EP(ep)
{
  PTRACE(4, "H46018\tCreating H46018 Handler.");

  nat = (PNatMethod_H46019 *)ep->GetNatMethods().LoadNatMethod("H46019");
  lastCallIdentifer = PString();
  m_h46018inOperation = false;

  if (nat != NULL) {
    nat->AttachHandler(this);
    ep->GetNatMethods().AddMethod(nat);
  }

#ifdef OPAL_H460
  m_h46024a = false;
#endif

  SocketCreateThread = NULL;
}


H46018Handler::~H46018Handler()
{
  PTRACE(4, "H46018\tClosing H46018 Handler.");
  EP->GetNatMethods().RemoveMethod("H46019");
}


PBoolean H46018Handler::CreateH225Transport(const PASN_OctetString & information)
{
  H46018_IncomingCallIndication callinfo;
  PPER_Stream raw(information);

  if (!callinfo.Decode(raw)) {
    PTRACE(2,"H46018\tUnable to decode incoming call Indication."); 
    return false;
  }

  PTRACE(4, "H46018\t" << callinfo );

  m_address = H323TransportAddress(callinfo.m_callSignallingAddress);
  m_callId = OpalGloballyUniqueID(callinfo.m_callID.m_guid);

  // Fix for Tandberg boxes that send duplicate SCI messages.
  if (m_callId.AsString() == lastCallIdentifer) {
    PTRACE(2,"H46018\tDuplicate Call Identifer " << lastCallIdentifer << " Ignoring request!"); 
    return false;
  }

  PTRACE(5, "H46018\tCreating H225 Channel");

  // We throw the socket creation onto another thread as with UMTS networks it may take several 
  // seconds to actually create the connection and we don't want to wait before signalling back
  // to the gatekeeper. This also speeds up connection time which is also nice :) - SH
  SocketCreateThread = PThread::Create(PCREATE_NOTIFIER(SocketThread), 0, PThread::AutoDeleteThread);

  return true;
}


void H46018Handler::SocketThread(PThread &, INT)
{
  if (m_callId == PString()) {
    PTRACE(3, "H46018\tTCP Connect Abort: No Call identifier");
    return;
  }

  H46018Transport * transport = new H46018Transport(*EP);
  transport->SetRemoteAddress(m_address);

  if (transport->Connect(m_callId)) {
    PTRACE(3, "H46018\tConnected to " << transport->GetRemoteAddress());
    new H46018TransportThread(transport->GetEndPoint(), transport);
    lastCallIdentifer = m_callId.AsString();
  } else {
    PTRACE(3, "H46018\tCALL ABORTED: Failed to TCP Connect to " << transport->GetRemoteAddress());
  }

  m_address = H323TransportAddress();
  m_callId = PString();
}


void H46018Handler::Enable()
{
  m_h46018inOperation = true;
  if (nat)
    nat->SetAvailable();
}


PBoolean H46018Handler::IsEnabled()
{
  return m_h46018inOperation;
}


H323EndPoint * H46018Handler::GetEndPoint() 
{ 
  return EP; 
}


void H46018Handler::H46024ADirect(bool reply, const PString & token)
{
  // NOTE 
#if 0
  PWaitAndSignal m(m_h46024aMutex);

  H323Connection * connection = EP->FindConnectionWithLock(token);
  if (connection != NULL) {
    connection->SendH46024AMessage(reply);
    connection->Unlock();
  }
#endif
}


///////////////////////////////////////////////////////////////////////////////////////////

PNatMethod_H46019::PNatMethod_H46019()
{
  handler = NULL;
  available = false;
  active = true;
}


PNatMethod_H46019::~PNatMethod_H46019()
{
}


void PNatMethod_H46019::AttachHandler(H46018Handler * _handler)
{
  handler = _handler;

  if (handler->GetEndPoint() == NULL) 
    return;

  WORD portPairBase = handler->GetEndPoint()->GetRtpIpPortBase();
  WORD portPairMax = handler->GetEndPoint()->GetRtpIpPortMax();

  // Initialise
  // ExternalAddress = 0;
  pairedPortInfo.basePort = 0;
  pairedPortInfo.maxPort = 0;
  pairedPortInfo.currentPort = 0;

  // Set the Port Pair Information
  pairedPortInfo.mutex.Wait();

  pairedPortInfo.basePort = (WORD)((portPairBase+1)&0xfffe);
  if (portPairBase == 0) {
    pairedPortInfo.basePort = 0;
    pairedPortInfo.maxPort = 0;
  }
  else if (portPairMax == 0)
    pairedPortInfo.maxPort = (WORD)(pairedPortInfo.basePort+99);
  else if (portPairMax < portPairBase)
    pairedPortInfo.maxPort = portPairBase;
  else
    pairedPortInfo.maxPort = portPairMax;

  pairedPortInfo.currentPort = pairedPortInfo.basePort;

  pairedPortInfo.mutex.Signal();

  available = FALSE;
}


PBoolean PNatMethod_H46019::GetExternalAddress(PIPSocket::Address & /*externalAddress*/, /// External address of router
                                               const PTimeInterval & /* maxAge */         /// Maximum age for caching
                                               )
{
  return FALSE;
}


PBoolean PNatMethod_H46019::CreateSocketPair(PUDPSocket * & socket1,
                                             PUDPSocket * & socket2,
                                             const PIPSocket::Address & binding,
                                             void * userData)
{

  PTRACE(2,"H46019\tCreateSocketPair");

  if (pairedPortInfo.basePort == 0 || pairedPortInfo.basePort > pairedPortInfo.maxPort)
  {
    PTRACE(1, "H46019\tInvalid local UDP port range "
      << pairedPortInfo.currentPort << '-' << pairedPortInfo.maxPort);
    return FALSE;
  }

  H323Connection::SessionInformation * info = (H323Connection::SessionInformation *)userData;

  socket1 = new H46019UDPSocket(*handler,info,true);    /// Data 
  socket2 = new H46019UDPSocket(*handler,info,false);    /// Signal

  /// Make sure we have sequential ports
  while ((!OpenSocket(*socket1, pairedPortInfo,binding)) ||
    (!OpenSocket(*socket2, pairedPortInfo,binding)) ||
    (socket2->GetPort() != socket1->GetPort() + 1 || 
    socket1->GetPort() & 1)  // socket 1 should be even
    )
  {
    PTRACE(5,"H46019\tAttempted Sockets: " << socket1->GetPort() << "," << socket2->GetPort() );
    if (socket1->GetPort() & 1) {
      // oops, somehow incremented one port but not the other....
      PWaitAndSignal mutex(pairedPortInfo.mutex);
      PTRACE(5,"H46019\tAdding one to currentPort: " << pairedPortInfo.currentPort );
      pairedPortInfo.currentPort++;
    }
    delete socket1;
    delete socket2;
    socket1 = new H46019UDPSocket(*handler,info,true);    /// Data 
    socket2 = new H46019UDPSocket(*handler,info,false);    /// Signal
  }

  PTRACE(5, "H46019\tUDP ports "
    << socket1->GetPort() << '-' << socket2->GetPort());

  if (info->GetSessionID() > 0)
    SetConnectionSockets(socket1,socket2,info);

  PTRACE(2,"H46019\tCreateSocketPair return TRUE");
  return TRUE;
}


PBoolean PNatMethod_H46019::OpenSocket(PUDPSocket & socket,
                                       PortInfo & portInfo,
                                       const PIPSocket::Address & binding) const
{
  PWaitAndSignal mutex(portInfo.mutex);

  WORD startPort = portInfo.currentPort;

  do {
    portInfo.currentPort++;
    if (portInfo.currentPort > portInfo.maxPort)
      portInfo.currentPort = portInfo.basePort;

    if (socket.Listen(binding,1, portInfo.currentPort)) {
      socket.SetReadTimeout(500);
      return true;
    }

  } while (portInfo.currentPort != startPort);

  PTRACE(2, "H46019\tFailed to bind to local UDP port in range "
    << portInfo.currentPort << '-' << portInfo.maxPort);
  return false;
}


void PNatMethod_H46019::SetConnectionSockets(PUDPSocket * data,
                                             PUDPSocket * control, 
                                             H323Connection::SessionInformation * info)
{
  PTRACE ( 3, "H46019\tSetConnectionSockets sessionId: " << info->GetSessionID() );
  if (handler->GetEndPoint() == NULL)
    return;

  PSafePtr<H323Connection> connection = handler->GetEndPoint()->FindConnectionWithLock(info->GetCallToken());
  if (connection != NULL) {
    connection->SetRTPNAT(info->GetSessionID(),data,control);
    connection->H46019Enabled();  // make sure H.460.19 is enabled
  } else {
    PTRACE ( 1, "H46019\tEEEK, null connection." );
  }

}


bool PNatMethod_H46019::IsAvailable(const PIPSocket::Address & /*address*/) 
{ 
  return handler->IsEnabled() && active;
}


void PNatMethod_H46019::SetAvailable() 
{ 
  if (!available) {
    handler->GetEndPoint()->NATMethodCallBack(GetName(),1,"Available");
    available = TRUE;
  }
}


/////////////////////////////////////////////////////////////////////////////////////////////

H46019UDPSocket::H46019UDPSocket(H46018Handler & _handler, H323Connection::SessionInformation * info, bool _rtpSocket)
  : m_Handler(_handler), m_Session(info->GetSessionID()), m_Token(info->GetCallToken()),
  m_CallId(info->GetCallIdentifer()), m_CUI(info->GetCUI()), rtpSocket(_rtpSocket)
{
  keeppayload = 0;
  keepTTL = 0;
  keepStartTime = NULL;
  muxId = 0;

  m_CUIrem = PString();
  m_locAddr = PIPSocket::GetDefaultIpAny();
  m_remAddr = PIPSocket::GetDefaultIpAny();
  m_detAddr = PIPSocket::GetDefaultIpAny();
  m_pendAddr= PIPSocket::GetDefaultIpAny();
  SetProbeState(e_notRequired);
  SSRC = PRandom::Number();

  m_h46024b = false;
}


H46019UDPSocket::~H46019UDPSocket()
{
  //PTRACE(5, "H46019\tI'm going away: " << (int)this);
  Keep.Stop();
  delete keepStartTime;

  m_Probe.Stop();
}


void H46019UDPSocket::Allocate(const H323TransportAddress & keepalive, unsigned _payload, unsigned _ttl, unsigned _muxId)
{

  PIPSocket::Address ip;  WORD port = 0;
  keepalive.GetIpAndPort(ip,port);
  if (ip.IsValid() && !ip.IsLoopback() && port > 0) {
    keepip = ip;
    keepport = port;
  }

  if (_payload > 0)
    keeppayload = _payload;

  if (_ttl > 0)
    keepTTL = _ttl;

  if (_muxId > 0)
    muxId = _muxId;

  PTRACE(4,"H46019UDP\tSetting " << keepip << ":" << keepport << " ping " << keepTTL << " secs. MuxId: " << muxId);
}


void H46019UDPSocket::Activate()
{
  InitialiseKeepAlive();
}


void H46019UDPSocket::Activate(const H323TransportAddress & keepalive, unsigned _payload, unsigned _ttl, unsigned _muxId)
{
  Allocate(keepalive,_payload,_ttl,_muxId);
  InitialiseKeepAlive();
}


void H46019UDPSocket::InitialiseKeepAlive() 
{
  PWaitAndSignal m(PingMutex);

  if (Keep.IsRunning()) {
    PTRACE(6,"H46019UDP\t" << (rtpSocket ? "RTP" : "RTCP") << " ping already running.");
    return;
  }

  if (keepTTL > 0 && keepip.IsValid() && !keepip.IsLoopback()) {
    keepseqno = 100;  // Some arbitory number
    keepStartTime = new PTime();

    PTRACE(4,"H46019UDP\tStart " << (rtpSocket ? "RTP" : "RTCP") << " pinging " 
      << keepip << ":" << keepport << " every " << keepTTL << " secs.");

    rtpSocket ? SendRTPPing(keepip,keepport) : SendRTCPPing();

    Keep.SetNotifier(PCREATE_NOTIFIER(Ping));
    Keep.RunContinuous(keepTTL * 1000); 
  } else {
    PTRACE(2,"H46019UDP\t"  << (rtpSocket ? "RTP" : "RTCP") << " PING NOT Ready " 
      << keepip << ":" << keepport << " - " << keepTTL << " secs.");

  }
}


void H46019UDPSocket::Ping(PTimer &, INT)
{ 
  rtpSocket ? SendRTPPing(keepip,keepport) : SendRTCPPing();
}


void H46019UDPSocket::SendRTPPing(const PIPSocket::Address & ip, const WORD & port) {

  RTP_DataFrame rtp;

  rtp.SetSequenceNumber(keepseqno);

  rtp.SetPayloadType((RTP_DataFrame::PayloadTypes)keeppayload);
  rtp.SetPayloadSize(0);

  // determining correct timestamp
  PTime currentTime = PTime();
  PTimeInterval timePassed = currentTime - *keepStartTime;
  rtp.SetTimestamp((DWORD)timePassed.GetMilliSeconds() * 8);

  rtp.SetMarker(TRUE);

  if (!Internal_WriteTo(rtp.GetPointer(),
    rtp.GetHeaderSize()+rtp.GetPayloadSize(),
    ip, port)) {
      switch (GetErrorNumber()) {
      case ECONNRESET :
      case ECONNREFUSED :
        PTRACE(2, "H46019UDP\t" << ip << ":" << port << " not ready.");
        break;

      default:
        PTRACE(1, "H46019UDP\t" << ip << ":" << port 
          << ", Write error on port ("
          << GetErrorNumber(PChannel::LastWriteError) << "): "
          << GetErrorText(PChannel::LastWriteError));
      }
  } else {
    PTRACE(6, "H46019UDP\tRTP KeepAlive sent: " << ip << ":" << port << " seq: " << keepseqno);    
    keepseqno++;
  }
}


void H46019UDPSocket::SendRTCPPing() 
{
  RTP_ControlFrame report;
  report.SetPayloadType(RTP_ControlFrame::e_SenderReport);
  report.SetPayloadSize(sizeof(RTP_ControlFrame::SenderReport));

  if (SendRTCPFrame(report, keepip, keepport)) {
    PTRACE(6, "H46019UDP\tRTCP KeepAlive sent: " << keepip << ":" << keepport);    
  }
}


PBoolean H46019UDPSocket::SendRTCPFrame(RTP_ControlFrame & report, const PIPSocket::Address & ip, WORD port) {

  if (!Internal_WriteTo(report.GetPointer(),report.GetSize(),
    ip, port)) {
      switch (GetErrorNumber()) {
      case ECONNRESET :
      case ECONNREFUSED :
        PTRACE(2, "H46019UDP\t" << ip << ":" << port << " not ready.");
        break;

      default:
        PTRACE(1, "H46019UDP\t" << ip << ":" << port 
          << ", Write error on port ("
          << GetErrorNumber(PChannel::LastWriteError) << "): "
          << GetErrorText(PChannel::LastWriteError));
      }
      return false;
  } 
  return true;
}


PBoolean H46019UDPSocket::GetLocalAddress(PIPSocket::Address & addr, WORD & port) 
{
  if (PUDPSocket::GetLocalAddress(addr, port)) {
    m_locAddr = addr;
    m_locPort = port;
    return true;
  }
  return false;
}


unsigned H46019UDPSocket::GetPingPayload()
{
  return keeppayload;
}


void H46019UDPSocket::SetPingPayLoad(unsigned val)
{
  keeppayload = val;
}


unsigned H46019UDPSocket::GetTTL()
{
  return keepTTL;
}


void H46019UDPSocket::SetTTL(unsigned val)
{
  keepTTL = val;
}


void H46019UDPSocket::SetProbeState(probe_state newstate)
{
  PWaitAndSignal m(probeMutex);

  m_state = newstate;
}


int H46019UDPSocket::GetProbeState() const
{
  PWaitAndSignal m(probeMutex);

  return m_state;
}


void H46019UDPSocket::SetAlternateAddresses(const H323TransportAddress & address, const PString & cui)
{
  address.GetIpAndPort(m_altAddr,m_altPort);

  PTRACE(6,"H46024A\ts: " << m_Session << (rtpSocket ? " RTP " : " RTCP ")  
    << "Remote Alt: " << m_altAddr << ":" << m_altPort << " CUI: " << cui);

  if (!rtpSocket) {
    m_CUIrem = cui;
    if (GetProbeState() < e_idle) {
      SetProbeState(e_idle);
      StartProbe();
      // We Already have a direct connection but we are waiting on the CUI for the reply
    } else if (GetProbeState() == e_verify_receiver) 
      ProbeReceived(false,m_pendAddr,m_pendPort);
  }
}


void H46019UDPSocket::GetAlternateAddresses(H323TransportAddress & address, PString & cui)
{

  address = H323TransportAddress(m_locAddr,m_locPort);

  if (!rtpSocket)
    cui = m_CUI;
  else
    cui = PString();

  if (GetProbeState() < e_idle)
    SetProbeState(e_initialising);

  PTRACE(6,"H46024A\ts:" << m_Session << (rtpSocket ? " RTP " : " RTCP ") << " Alt:" << address << " CUI " << cui);

}


PBoolean H46019UDPSocket::IsAlternateAddress(const Address & address,WORD port)
{
  return ((address == m_detAddr) && (port == m_detPort));
}


void H46019UDPSocket::StartProbe()
{

  PTRACE(4,"H46024A\ts: " << m_Session << " Starting direct connection probe.");

  SetProbeState(e_probing);
  m_probes = 0;
  m_Probe.SetNotifier(PCREATE_NOTIFIER(Probe));
  m_Probe.RunContinuous(100); 
}


void H46019UDPSocket::BuildProbe(RTP_ControlFrame & report, bool probing)
{
  report.SetPayloadType(RTP_ControlFrame::e_ApplDefined);
  report.SetCount((probing ? 0 : 1));  // SubType Probe

  report.SetPayloadSize(sizeof(probe_packet));

  probe_packet data;
  data.SSRC = SSRC;
  data.Length = sizeof(probe_packet);
  PString id = "24.1";
  PBYTEArray bytes(id,id.GetLength(), false);
  memcpy(&data.name[0], bytes, 4);

#if OPAL_PTLIB_SSL
  PMessageDigest::Result bin_digest;
  PMessageDigestSHA1::Encode(m_CallId.AsString() + m_CUIrem, bin_digest);
  memcpy(&data.cui[0], bin_digest.GetPointer(), bin_digest.GetSize());
#else
  //android_sha1(&data.cui[0],m_CallId.AsString() + m_CUIrem);
#endif

  memcpy(report.GetPayloadPtr(),&data,sizeof(probe_packet));

}


void H46019UDPSocket::Probe(PTimer &, INT)
{ 
  m_probes++;

  if (m_probes >= 5) {
    m_Probe.Stop();
    return;
  }

  if (GetProbeState() != e_probing)
    return;

  RTP_ControlFrame report;
  report.SetSize(4+sizeof(probe_packet));
  BuildProbe(report, true);

  if (!PUDPSocket::WriteTo(report.GetPointer(),report.GetSize(),
    m_altAddr, m_altPort)) {
      switch (GetErrorNumber()) {
      case ECONNRESET :
      case ECONNREFUSED :
        PTRACE(2, "H46024A\t" << m_altAddr << ":" << m_altPort << " not ready.");
        break;

      default:
        PTRACE(1, "H46024A\t" << m_altAddr << ":" << m_altPort 
          << ", Write error on port ("
          << GetErrorNumber(PChannel::LastWriteError) << "): "
          << GetErrorText(PChannel::LastWriteError));
      }
  } else {
    PTRACE(6, "H46024A\ts" << m_Session <<" RTCP Probe sent: " << m_altAddr << ":" << m_altPort);    
  }
}


void H46019UDPSocket::ProbeReceived(bool probe, const PIPSocket::Address & addr, WORD & port)
{
  if (probe) {
    m_Handler.H46024ADirect(false,m_Token);  //< Tell remote to wait for connection
  }
  else {
    RTP_ControlFrame reply;
    reply.SetSize(4+sizeof(probe_packet));
    BuildProbe(reply, false);
    if (SendRTCPFrame(reply,addr,port)) {
      PTRACE(4, "H46024A\tRTCP Reply packet sent: " << addr << ":" << port);    
    }
  }
}


void H46019UDPSocket::H46024Adirect(bool starter)
{
  if (starter) {  // We start the direct channel 
    m_detAddr = m_altAddr;  m_detPort = m_altPort;
    PTRACE(4, "H46024A\ts:" << m_Session << (rtpSocket ? " RTP " : " RTCP ")  
      << "Switching to " << m_detAddr << ":" << m_detPort);
    SetProbeState(e_direct);
  } else         // We wait for the remote to start channel
    SetProbeState(e_wait);

  Keep.Stop();  // Stop the keepAlive Packets
}


PBoolean H46019UDPSocket::ReadFrom(void * buf, PINDEX len, Address & addr, WORD & port)
{
  bool probe = false; bool success = false;
  RTP_ControlFrame frame(2048); 

  while (PUDPSocket::ReadFrom(buf, len, addr, port)) {

    /// Set the detected routed remote address (on first packet received)
    if (m_remAddr.IsAny()) {   
      m_remAddr = addr; 
      m_remPort = port;
    }
    if (m_h46024b && addr == m_altAddr && port == m_altPort) {
      PTRACE(4, "H46024B\ts:" << m_Session << (rtpSocket ? " RTP " : " RTCP ")  
        << "Switching to " << addr << ":" << port << " from " << m_remAddr << ":" << m_remPort);
      m_detAddr = addr;  m_detPort = port;
      SetProbeState(e_direct);
      Keep.Stop();  // Stop the keepAlive Packets
      m_h46024b = false;
      continue;
    }
    /// Check the probe state
    switch (GetProbeState()) {
    case e_initialising:                        // RTCP only
    case e_idle:                                // RTCP only
    case e_probing:                                // RTCP only
    case e_verify_receiver:                        // RTCP only
      frame.SetSize(len);
      memcpy(frame.GetPointer(),buf,len);
      if (ReceivedProbePacket(frame,probe,success)) {
        if (success)
          ProbeReceived(probe,addr,port);
        else {
          m_pendAddr = addr; m_pendPort = port;
        }
        continue;  // don't forward on probe packets.
      }
      break;
    case e_wait:
      if (addr == keepip) {// We got a keepalive ping...
        Keep.Stop();  // Stop the keepAlive Packets
      } else if ((addr == m_altAddr) && (port == m_altPort)) {
        PTRACE(4, "H46024A\ts:" << m_Session << (rtpSocket ? " RTP " : " RTCP ")  << "Already sending direct!");
        m_detAddr = addr;  m_detPort = port;
        SetProbeState(e_direct);
      } else if ((addr == m_pendAddr) && (port == m_pendPort)) {
        PTRACE(4, "H46024A\ts:" << m_Session << (rtpSocket ? " RTP " : " RTCP ")  
          << "Switching to Direct " << addr << ":" << port);
        m_detAddr = addr;  m_detPort = port;
        SetProbeState(e_direct);
      } else if ((addr != m_remAddr) || (port != m_remPort)) {
        PTRACE(4, "H46024A\ts:" << m_Session << (rtpSocket ? " RTP " : " RTCP ")  
          << "Switching to " << addr << ":" << port << " from " << m_remAddr << ":" << m_remPort);
        m_detAddr = addr;  m_detPort = port;
        SetProbeState(e_direct);
      } 
      break;
    case e_direct:    
    default:
      break;
    }
    return true;
  } 
  return false; 
}


PBoolean H46019UDPSocket::Internal_WriteTo(const void * bytes, PINDEX len, const Address & addr, WORD port)
{
	if (GetProbeState() == e_direct)
		return PUDPSocket::WriteTo(bytes,len, m_detAddr, m_detPort);
	else
		return PUDPSocket::WriteTo(bytes,len, addr, port);
}


PBoolean H46019UDPSocket::Internal_WriteTo(const Slice* slices, size_t nSlices, const Address & addr, WORD port)
{
  if (GetProbeState() == e_direct)
    return PUDPSocket::WriteTo(slices, nSlices, m_detAddr, m_detPort);
  else
    return PUDPSocket::WriteTo(slices, nSlices, addr, port);
}


PBoolean H46019UDPSocket::WriteTo(const void * buf, PINDEX len, const Address & addr, WORD port) {

  // If a muxId was given in traversal parameters, then we need to tac it on
  // as the 1st four bytes of rtp data.

  Slice slices[2];
  size_t nSlices=0;
  unsigned char muxBuf[4];
  if (muxId > 0) {
    memcpy(muxBuf,&muxId,4);
    slices[nSlices++] = Slice(muxBuf,4);
  }
  slices[nSlices++] = Slice(const_cast<void*>(buf),len);
  return Internal_WriteTo (slices,nSlices,addr, port );
}


PBoolean H46019UDPSocket::ReceivedProbePacket(const RTP_ControlFrame & frame, bool & probe, bool & success)
{
  success = false;

  //Inspect the probe packet
  if (frame.GetPayloadType() == RTP_ControlFrame::e_ApplDefined) {

    int cstate = GetProbeState();
    if (cstate == e_notRequired) {
      PTRACE(6, "H46024A\ts:" << m_Session <<" received RTCP probe packet. LOGIC ERROR!");
      return true;  
    }

    if (cstate > e_probing) {
      PTRACE(6, "H46024A\ts:" << m_Session <<" received RTCP probe packet. IGNORING! Already authenticated.");
      return true;  
    }

    probe = (frame.GetCount() > 0);
    PTRACE(4, "H46024A\ts:" << m_Session <<" RTCP Probe " << (probe ? "Reply" : "Request") << " received.");    

    BYTE * data = frame.GetPayloadPtr();
    PBYTEArray bytes(20);
    memcpy(bytes.GetPointer(),data+12, 20);
#if OPAL_PTLIB_SSL
    PMessageDigest::Result bin_digest;
    PMessageDigestSHA1::Encode(m_CallId.AsString() + m_CUI, bin_digest);
    PBYTEArray val(bin_digest.GetPointer(),bin_digest.GetSize());
#else
    PBYTEArray val(20);
    //android_sha1(val.GetPointer(), m_CallId.AsString() + m_CUI);
#endif

    if (bytes == val) {
      if (probe)  // We have a reply
        SetProbeState(e_verify_sender);
      else 
        SetProbeState(e_verify_receiver);

      m_Probe.Stop();
      PTRACE(4, "H46024A\ts" << m_Session <<" RTCP Probe " << (probe ? "Reply" : "Request") << " verified.");
      if (!m_CUIrem.IsEmpty())
        success = true;
      else {
        PTRACE(4, "H46024A\ts" << m_Session <<" Remote not ready.");
      }
    } else {
      PTRACE(4, "H46024A\ts" << m_Session <<" RTCP Probe " << (probe ? "Reply" : "Request") << " verify FAILURE");    
    }
    return true;
  }
  else 
    return false;
}


void H46019UDPSocket::H46024Bdirect(const H323TransportAddress & address)
{
  address.GetIpAndPort(m_altAddr,m_altPort);
  PTRACE(6,"H46024b\ts: " << m_Session << " RTP Remote Alt: " << m_altAddr << ":" << m_altPort);

  m_h46024b = true;

  // Sending an empty RTP frame to the alternate address
  // will add a mapping to the router to receive RTP from
  // the remote
  SendRTPPing(m_altAddr, m_altPort);
}


#endif  // OPAL_H460
