/*
 * skinnyep.cxx
 *
 * Cisco SCCP "skinny" protocol support.
 *
 * Open Phone Abstraction Library (OPAL)
 * Formally known as the Open H323 project.
 *
 * Copyright (c) 2014 Vox Lucida Pty. Ltd.
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
 * The Initial Developer of the Original Code is Vox Lucida Pty. Ltd.
 *
 * Contributor(s): Robert Jongbloed (robertj@voxlucida.com.au).
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#include <ptlib.h>
#include <ep/skinnyep.h>

#if OPAL_SKINNY

#include <ptclib/url.h>
#include <opal/patch.h>
#include <codec/opalwavfile.h>


#define PTraceModule() "Skinny"


PURL_LEGACY_SCHEME(sccp,
                   true,  /* URL scheme has a username */
                   false, /* URL scheme has a password */
                   true,  /* URL scheme has a host:port */
                   true,  /* URL scheme is username if no @, otherwise host:port */
                   false, /* URL scheme defaults to PIPSocket::GetHostName() if not present */
                   false, /* URL scheme has a query section */
                   true,  /* URL scheme has a parameter section */
                   false, /* URL scheme has a fragment section */
                   false, /* URL scheme has a path */
                   false, /* URL scheme has relative path (no //) then scheme: is not output */
                   2000);


static PString CreateToken(const OpalSkinnyEndPoint::PhoneDevice & phone, unsigned callIdentifier)
{
  PString token = "sccp-" + phone.GetName();
  if (callIdentifier > 0)
    token.sprintf("-%u", callIdentifier);

  return token;
}

static POrdinalToString::Initialiser const CodecCodes[] = {
  {   2, OPAL_G711_ALAW_64K  },
  {   4, OPAL_G711_ULAW_64K  },
  {   6, OPAL_G722           },
  {   9, OPAL_G7231          },
  {  10, OPAL_G728           },
  {  11, OPAL_G729           },
  {  12, OPAL_G729A          },
  {  15, OPAL_G729B          },
  {  16, OPAL_G729AB         },
  {  40, OPAL_G7221_32K      },
  {  41, OPAL_G7221_24K      },
  {  80, OPAL_GSM0610        },
  {  82, OPAL_G726_32K       },
  {  83, OPAL_G726_24K       },
  {  84, OPAL_G726_16K       },
#if OPAL_VIDEO
  { 100, OPAL_H261 },
  { 101, OPAL_H263           },
  { 103, OPAL_H264           },
  { 106, OPAL_FECC_RTP       },
#endif
  { 257, OPAL_RFC2833        }
};
static PStringToOrdinal const MediaFormatToCodecCode(PARRAYSIZE(CodecCodes), CodecCodes);
static POrdinalToString const CodecCodeToMediaFormat(PARRAYSIZE(CodecCodes), CodecCodes);

static PConstString const RegisteredStatusText("Registered");


///////////////////////////////////////////////////////////////////////////////

OpalSkinnyEndPoint::SkinnyMsg::SkinnyMsg(uint32_t id, PINDEX sizeofClass, PINDEX extraSpace)
  : m_extraSpace(extraSpace)
  , m_length(0)
  , m_headerVersion(0)
{
  m_length = sizeofClass - ((char *)&m_messageId - (char *)this);
  memset(&m_messageId, 0, m_length);
  m_messageId = id;
}


void OpalSkinnyEndPoint::SkinnyMsg::Construct(const PBYTEArray & pdu)
{
  PINDEX len = m_length + sizeof(m_headerVersion);
  memcpy(&m_headerVersion, pdu, std::min(len, pdu.GetSize()));
  PTRACE_IF(2, pdu.GetSize() < len - m_extraSpace, &pdu, PTraceModule(), "Received message size error: "
            "id=0x" << hex << GetID() << dec << ", expected = " << len << ", received = " << pdu.GetSize());
}


///////////////////////////////////////////////////////////////////////////////

OpalSkinnyEndPoint::OpalSkinnyEndPoint(OpalManager & manager, const char *prefix)
  : OpalRTPEndPoint(manager, prefix, IsNetworkEndPoint | SupportsE164)
  , m_secondaryAudioAlwaysSimulated(true)
{
}


OpalSkinnyEndPoint::~OpalSkinnyEndPoint()
{
}


void OpalSkinnyEndPoint::ShutDown()
{
  PTRACE(3, "Endpoint shutting down.");

  m_phoneDevicesMutex.Wait();

  for (PhoneDeviceDict::iterator it = m_phoneDevices.begin(); it != m_phoneDevices.end(); ++it)
    it->second.Stop();

  // Wait a bit for ack replies.
  for (PINDEX wait = 0; wait < 10; ++wait) {
    PhoneDeviceDict::iterator it;
    for (it = m_phoneDevices.begin(); it != m_phoneDevices.end(); ++it) {
      if (it->second.m_status == RegisteredStatusText)
        break;
    }
    if (it == m_phoneDevices.end())
      break;

    m_phoneDevicesMutex.Signal();
    PThread::Sleep(200);
    m_phoneDevicesMutex.Wait();
  }

  for (PhoneDeviceDict::iterator it = m_phoneDevices.begin(); it != m_phoneDevices.end(); ++it)
    it->second.Close();

  m_phoneDevicesMutex.Signal();

  PTRACE(4, "Endpoint shut down.");
}


PString OpalSkinnyEndPoint::GetDefaultTransport() const
{
  return OpalTransportAddress::TcpPrefix();
}


WORD OpalSkinnyEndPoint::GetDefaultSignalPort() const
{
  return 2000;
}


OpalMediaFormatList OpalSkinnyEndPoint::GetMediaFormats() const
{
  OpalMediaFormatList formats;
  for (PINDEX i = 0; i < PARRAYSIZE(CodecCodes); ++i)
    formats += CodecCodes[i].value;
  return formats;
}


void OpalSkinnyEndPoint::NewIncomingConnection(OpalListener & /*listener*/, const OpalTransportPtr & /*transport*/)
{
}


PSafePtr<OpalConnection> OpalSkinnyEndPoint::MakeConnection(OpalCall & call,
                                                            const PString & party,
                                                            void * userData,
                                                            unsigned int options,
                                                            OpalConnection::StringOptions * stringOptions)
{
  PString numberName;
  if (party.NumCompare(GetPrefixName() + ':') == EqualTo)
    numberName = party.Mid(GetPrefixName().GetLength() + 1);
  else
    numberName = party;

  PString number, name;
  numberName.Split('@', number, name, PString::SplitDefaultToBefore|PString::SplitTrim);

  if (!OpalIsE164(number)) {
    PTRACE(2, "Remote party \"" << number << "\" is not an E.164 number.");
    return NULL;
  }

  PWaitAndSignal mutex(m_phoneDevicesMutex);

  PhoneDevice * phone;

  if (name.IsEmpty()) {
    PhoneDeviceDict::iterator dev = m_phoneDevices.begin();
    if (dev == m_phoneDevices.end()) {
      PTRACE(2, "Cannot call, no phone devices registered.");
      return NULL;
    }

    while (dev->second.m_activeConnection != NULL) {
      if (++dev == m_phoneDevices.end()) {
        PTRACE(2, "Cannot call, all phone devices are in use.");
        return NULL;
      }
    }

    phone = &dev->second;
  }
  else {
    phone = m_phoneDevices.GetAt(name);

    if (phone == NULL) {
      PTRACE(2, "Cannot call, phone device \"" << name << "\" does not exist.");
      return NULL;
    }

    if (phone->m_activeConnection != NULL) {
      PTRACE(2, "Cannot call, phone device \"" << name << "\" is in use.");
      return NULL;
    }
  }

  return AddConnection(CreateConnection(call, *phone, 0, number, userData, options, stringOptions));
}


PBoolean OpalSkinnyEndPoint::GarbageCollection()
{
  return OpalEndPoint::GarbageCollection();
}


OpalSkinnyConnection * OpalSkinnyEndPoint::CreateConnection(OpalCall & call,
                                                            OpalSkinnyEndPoint::PhoneDevice & phone,
                                                            unsigned callIdentifier,
                                                            const PString & dialNumber,
                                                            void * userData,
                                                            unsigned int options,
                                                            OpalConnection::StringOptions * stringOptions)
{
  return new OpalSkinnyConnection(call, *this, phone, callIdentifier, dialNumber, userData, options, stringOptions);
}


bool OpalSkinnyEndPoint::Register(const PString & server, const PString & name, unsigned deviceType, const PString & localInterface)
{
  if (name.IsEmpty() || name.GetLength() > RegisterMsg::MaxNameSize) {
    PTRACE(2, "Illegal phone device name \"" << name << '"');
    return false;
  }

  PWaitAndSignal mutex(m_phoneDevicesMutex);

  PhoneDevice * phoneDevice = m_phoneDevices.GetAt(name);
  if (phoneDevice != NULL) {
    if (phoneDevice->m_deviceType == deviceType) {
      PTRACE(3, "Phone device \"" << name << "\" already registered");
      return false;
    }

    m_phoneDevices.RemoveAt(name);
  }

  phoneDevice = CreatePhoneDevice(name, deviceType, localInterface);
  if (phoneDevice == NULL) {
    PTRACE(3, "Could not create phone device for \"" << name << '"');
    return false;
  }

  if (phoneDevice->Start(server)) {
    m_phoneDevices.SetAt(name, phoneDevice);
    return true;
  }

  delete phoneDevice;
  return false;
}


bool OpalSkinnyEndPoint::Unregister(const PString & name)
{
  PWaitAndSignal mutex(m_phoneDevicesMutex);

  PhoneDeviceDict::iterator it = m_phoneDevices.find(name);
  if (it != m_phoneDevices.end()) {
    m_phoneDevices.erase(it);
    return true;
  }

  PTRACE(3, "Phone device \"" << name << "\" not registered");
  return false;
}


OpalSkinnyEndPoint::PhoneDevice * OpalSkinnyEndPoint::CreatePhoneDevice(const PString & name, unsigned deviceType, const PString & localInterface)
{
  return new PhoneDevice(*this, name, deviceType, localInterface);
}


OpalSkinnyEndPoint::PhoneDevice::PhoneDevice(OpalSkinnyEndPoint & ep, const PString & name, unsigned deviceType, const PString & localInterface)
  : m_endpoint(ep)
  , m_name(name)
  , m_deviceType(deviceType)
  , m_transport(ep, PIPAddress(localInterface))
{
  m_transport.SetPDULengthFormat(-4, 4);
}


void OpalSkinnyEndPoint::PhoneDevice::PrintOn(ostream & strm) const
{
  strm << m_name << '@' << m_transport.GetRemoteAddress().GetHostName() << '\t' << m_status;
}


bool OpalSkinnyEndPoint::PhoneDevice::Start(const PString & server)
{
  OpalTransportAddress addr(server, m_endpoint.GetDefaultSignalPort(), m_endpoint.GetDefaultTransport());
  if (!m_transport.SetRemoteAddress(addr)) {
    m_status = "Transport error: " + m_transport.GetErrorText();
    return false;
  }

  // Set start up delay dependent on how many connections are pensing.
  for (PhoneDeviceDict::iterator it = m_endpoint.m_phoneDevices.begin(); it != m_endpoint.m_phoneDevices.end(); ++it) {
    if (it->second.m_status == "Registering")
      m_delay += 50;
  }

  m_status = "Registering";
  m_transport.AttachThread(new PThreadObj<PhoneDevice>(*this, &PhoneDevice::HandleTransport, false, "Skinny", PThread::HighPriority));
  return true;
}


bool OpalSkinnyEndPoint::PhoneDevice::SendRegisterMsg()
{
  RegisterMsg msg;

  PIPSocket::Address ip;
  m_transport.GetLocalAddress().GetIpAddress(ip);


  strncpy(msg.m_deviceName, m_name, sizeof(msg.m_deviceName) - 1);
  strncpy(msg.m_macAddress, PIPSocket::GetInterfaceMACAddress().ToUpper(), sizeof(msg.m_macAddress));
  msg.m_ip = ip;
  msg.m_maxStreams = 5;
  msg.m_deviceType = m_deviceType;
  msg.m_protocol.m_version = 15;

  if (!SendSkinnyMsg(msg))
    return false;
  
  PTRACE(4, "Sent register message for " << m_name << ", type=" << m_deviceType << ", " << msg.m_protocol);
  return true;
}


void OpalSkinnyEndPoint::PhoneDevice::Stop()
{
  if (m_status == RegisteredStatusText) {
    PTRACE(4, "Unregistering " << m_name);
    SendSkinnyMsg(UnregisterMsg());
  }
}


void OpalSkinnyEndPoint::PhoneDevice::Close()
{
  PTRACE(4, "Closing " << m_name);

  Stop();

  // Wait a bit for ack reply.
  if (m_status == RegisteredStatusText) {
    for (PINDEX wait = 0; wait < 10; ++wait) {
      if (!m_transport.IsOpen())
        break;
      PThread::Sleep(200);
    }
  }

  m_exit.Signal();
  m_transport.CloseWait();
}


bool OpalSkinnyEndPoint::PhoneDevice::SendSkinnyMsg(const SkinnyMsg & msg)
{
  PTRACE(3, "Sending " << msg);
  if (m_transport.Write(msg.GetPacketPtr(), msg.GetPacketLen()))
    return true;

  PTRACE(2, "Error writing message for " << m_name << " to " << m_transport.GetRemoteAddress().GetHostName());
  return false;
}


void OpalSkinnyEndPoint::PhoneDevice::HandleTransport()
{
  PTRACE(4, "Started phone device handler thread: " << m_name << ", delay=" << m_delay);

  bool running = true;

  if (m_exit.Wait(m_delay)) {
    PTRACE(4, "Exiting thread for " << m_name);
    running = false;
  }

  while (running) {
    PBYTEArray pdu;
    if (m_transport.ReadPDU(pdu)) {
      unsigned msgId = pdu.GetAs<PUInt32l>(4);
      switch (msgId) {
#define ON_RECEIVE_MSG(cls) case cls::ID: running = OnReceiveMsg(cls(pdu)); break
        ON_RECEIVE_MSG(KeepAliveMsg);
        ON_RECEIVE_MSG(RegisterMsg);
        ON_RECEIVE_MSG(RegisterAckMsg);
        ON_RECEIVE_MSG(RegisterRejectMsg);
        ON_RECEIVE_MSG(UnregisterMsg);
        ON_RECEIVE_MSG(UnregisterAckMsg);
        ON_RECEIVE_MSG(PortMsg);
        ON_RECEIVE_MSG(CapabilityRequestMsg);
        ON_RECEIVE_MSG(CapabilityResponseMsg);
        ON_RECEIVE_MSG(CallStateMsg);
        ON_RECEIVE_MSG(CallInfoMsg);
        ON_RECEIVE_MSG(CallInfo5Msg);
        ON_RECEIVE_MSG(SetRingerMsg);
        ON_RECEIVE_MSG(OffHookMsg);
        ON_RECEIVE_MSG(OnHookMsg);
        ON_RECEIVE_MSG(StartToneMsg);
        ON_RECEIVE_MSG(StopToneMsg);
        ON_RECEIVE_MSG(KeyPadButtonMsg);
        ON_RECEIVE_MSG(SoftKeyEventMsg);
        ON_RECEIVE_MSG(OpenReceiveChannelMsg);
        ON_RECEIVE_MSG(OpenReceiveChannelAckMsg);
        ON_RECEIVE_MSG(CloseReceiveChannelMsg);
        ON_RECEIVE_MSG(StartMediaTransmissionMsg);
        ON_RECEIVE_MSG(StopMediaTransmissionMsg);

        // Don't use above macro as PTRACE() generate a lot of noise
        case KeepAliveAckMsg::ID :
          if (!m_endpoint.OnReceiveMsg(*this, KeepAliveAckMsg(pdu)))
            running = false;
          break;

        default:
          PTRACE(4, "Received unhandled message id=0x" << hex << msgId << dec);
          break;
      }
    }
    else {
      switch (m_transport.GetErrorCode(PChannel::LastReadError)) {
        case PChannel::Timeout :
        case PChannel::Interrupted :
          continue;

        case PChannel::NotOpen :
          break;

        case PChannel::NoError:
          // Remote close of TCP
          PTRACE(3, "Lost transport to " << m_transport << " for " << m_name);
          m_status = "Lost transport";
          break;

        default :
          m_status = "Transport error: " + m_transport.GetErrorText(PChannel::LastReadError);
          PTRACE(2, m_status);
          break;
      }

      m_transport.Close();
      m_delay.SetInterval(0, 10);
      while (!m_transport.Connect() || !SendRegisterMsg()) {
        PTRACE(2, "Server for " << m_name << " transport reconnect error: " << m_transport.GetErrorText());
        if (m_exit.Wait(m_delay)) {
          PTRACE(4, "Exiting thread for " << m_name);
          running = false;
          break;
        }

        if (m_delay < 60)
          m_delay += 10000;
      }
    }
  }

  m_transport.Close();
  PTRACE(4, "Ended phone device handler thread: " << m_name);
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice & phone, const KeepAliveMsg &)
{
  return phone.SendSkinnyMsg(KeepAliveAckMsg());
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice &, const KeepAliveAckMsg &)
{
  return true;
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice &, const RegisterMsg &)
{
  return true;
}


void OpalSkinnyEndPoint::RegisterMsg::PrintOn(ostream & strm) const
{
  strm << GetClass() << " device=" << m_deviceName
        << " type=" << m_deviceType
        << " streams=" << m_maxStreams
        << " ip=" << PIPAddress(m_ip);
}


ostream & operator<<(ostream&strm, const OpalSkinnyEndPoint::Proto & protocol)
{
  return strm << "protocol=" << (unsigned)protocol.m_version << ", "
                  "flags=0x" << hex << (unsigned)protocol.m_flags << dec << ", "
                  "features=0x" << hex << (unsigned)protocol.m_features << dec;
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice & phone, const RegisterAckMsg & ack)
{
  phone.m_status = RegisteredStatusText;

  PTRACE(2, "Registered: Server " << ack.m_protocol);


  PIPSocket::AddressAndPort ap;
  if (phone.m_transport.GetLocalAddress().GetIpAndPort(ap)) {
    PortMsg msg;
    msg.m_port = ap.GetPort();
    phone.SendSkinnyMsg(msg);
  }

  KeepAliveMsg msg;
  phone.m_transport.SetKeepAlive(PTimeInterval(0, ack.m_keepAlive-1), PBYTEArray(msg.GetPacketPtr(), msg.GetPacketLen()));
  return true;
}


void OpalSkinnyEndPoint::RegisterAckMsg::PrintOn(ostream & strm) const
{
  strm << GetClass() << " keepAlive=" << m_keepAlive;
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice & phone, const RegisterRejectMsg & msg)
{
  PTRACE(2, "Server rejected registration for " << phone.m_name << ": " << msg.m_errorText);
  phone.m_status = PConstString("Rejected: ") + msg.m_errorText;
  return false;
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice &, const UnregisterMsg &)
{
  return true;
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice & phone, const UnregisterAckMsg &)
{
  PTRACE(2, "Unregistered " << phone.m_name << " from server");
  phone.m_status = "Unregistered";
  return false;
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice &, const PortMsg &)
{
  return true;
}


void OpalSkinnyEndPoint::PortMsg::PrintOn(ostream & strm) const
{
  strm << GetClass() << ' ' << m_port;
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice & phone, const CapabilityRequestMsg &)
{
  CapabilityResponseMsg msg;

  PINDEX count = 0;
  OpalMediaFormatList formats = GetMediaFormats();
  formats.Remove(manager.GetMediaFormatMask());
  formats.Reorder(manager.GetMediaFormatOrder());
  for (OpalMediaFormatList::iterator it = formats.begin(); it != formats.end(); ++it) {
    if (MediaFormatToCodecCode.Contains(it->GetName())) {
      msg.m_capability[count].m_codec = MediaFormatToCodecCode[it->GetName()];
      msg.m_capability[count].m_maxFramesPerPacket = (uint16_t)it->GetOptionInteger(OpalAudioFormat::RxFramesPerPacketOption(), 1);
      ++count;
      if (count >= PARRAYSIZE(msg.m_capability))
        break;
    }
  }

  PTRACE_IF(2, count == 0, "Must have a valid codec!");
  msg.SetCount(count);

  return phone.SendSkinnyMsg(msg);
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice &, const CapabilityResponseMsg &)
{
  return true;
}


void OpalSkinnyEndPoint::CapabilityResponseMsg::PrintOn(ostream & strm) const
{
  strm << GetClass() << ' ' << m_count << " codecs";
}


void OpalSkinnyEndPoint::CapabilityResponseMsg::SetCount(PINDEX count)
{
  m_count = count;
  m_length = m_length - (PARRAYSIZE(m_capability) - count) * sizeof(Info);
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice & phone, const CallStateMsg & msg)
{
  {
    PSafePtr<OpalSkinnyConnection> connection = phone.m_activeConnection;
    if (connection != NULL) {
      PTRACE_CONTEXT_ID_PUSH_THREAD(connection);
      return connection->OnReceiveMsg(msg);
    }
  }

  if (msg.GetState() != eStateRingIn) {
    PTRACE(4, "Unhandled state: " << msg.GetState());
    return true;
  }

  // Incoming call
  OpalCall * call = manager.InternalCreateCall();
  if (call == NULL) {
    PTRACE(2, "Internal failure to create call");
    return true;
  }

  OpalSkinnyConnection * connection = CreateConnection(*call, phone, msg.m_callIdentifier, PString::Empty(), NULL, 0, NULL);
  if (AddConnection(connection) == NULL)
    return true;

  PTRACE_CONTEXT_ID_PUSH_THREAD(connection);
  return connection->OnReceiveMsg(msg);
}


void OpalSkinnyEndPoint::CallStateMsg::PrintOn(ostream & strm) const
{
  strm << GetClass() << ' ' << GetState() << " line=" << m_lineInstance << " call=" << m_callIdentifier;
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice & phone, const CallInfoMsg & msg)
{
  return DelegateMsg(phone, msg);
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice & phone, const CallInfo5Msg & msg)
{
  return DelegateMsg(phone, msg);
}


void OpalSkinnyEndPoint::CallInfoCommon::PrintOn(ostream & strm) const
{
  strm << GetClass() << ' ' << GetType()
       << " line=" << GetLineInstance()
       << " call=" << GetCallIdentifier()
       << " called=" << GetCalledPartyNumber()
       << " calling=" << GetCallingPartyNumber();
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice & phone, const SetRingerMsg & msg)
{
  return DelegateMsg(phone, msg);
}


void OpalSkinnyEndPoint::SetRingerMsg::PrintOn(ostream & strm) const
{
  strm << GetClass() << ' ' << GetType()
       << " mode=" << m_ringMode
       << " line=" << m_lineInstance
       << " call=" << m_callIdentifier;
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice &, const OffHookMsg &)
{
  return true;
}


void OpalSkinnyEndPoint::OffHookMsg::PrintOn(ostream & strm) const
{
  strm << GetClass() << " line=" << m_lineInstance << " call=" << m_callIdentifier;
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice &, const OnHookMsg &)
{
  return true;
}


void OpalSkinnyEndPoint::OnHookMsg::PrintOn(ostream & strm) const
{
  strm << GetClass() << " line=" << m_lineInstance << " call=" << m_callIdentifier;
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice &, const StartToneMsg &)
{
  return true;
}


void OpalSkinnyEndPoint::StartToneMsg::PrintOn(ostream & strm) const
{
  strm << GetClass() << ' ' << GetType() << " line=" << m_lineInstance << " call=" << m_callIdentifier;
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice &, const StopToneMsg &)
{
  return true;
}


void OpalSkinnyEndPoint::StopToneMsg::PrintOn(ostream & strm) const
{
  strm << GetClass() << " line=" << m_lineInstance << " call=" << m_callIdentifier;
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice &, const KeyPadButtonMsg &)
{
  return true;
}


void OpalSkinnyEndPoint::KeyPadButtonMsg::PrintOn(ostream & strm) const
{
  strm << GetClass() << " '" << m_button<< "' line=" << m_lineInstance << " call=" << m_callIdentifier;
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice &, const SoftKeyEventMsg &)
{
  return true;
}


void OpalSkinnyEndPoint::SoftKeyEventMsg::PrintOn(ostream & strm) const
{
  strm << GetClass() << ' ' << GetEvent() << " line=" << m_lineInstance << " call=" << m_callIdentifier;
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice & phone, const OpenReceiveChannelMsg & msg)
{
  return DelegateMsg(phone, msg);
}


void OpalSkinnyEndPoint::OpenReceiveChannelMsg::PrintOn(ostream & strm) const
{
  strm << GetClass()
       << " call=" << m_callIdentifier
       << " id=" << m_passThruPartyId
       << " payload=" << m_payloadCapability;
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice &, const OpenReceiveChannelAckMsg &)
{
  return true;
}


void OpalSkinnyEndPoint::OpenReceiveChannelAckMsg::PrintOn(ostream & strm) const
{
  strm << GetClass()
       << " id=" << m_passThruPartyId
       << ' ' << PIPAddress(m_ip) << ':' << m_port
       << " status=" << m_status;
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice & phone, const CloseReceiveChannelMsg & msg)
{
  return DelegateMsg(phone, msg);
}


void OpalSkinnyEndPoint::CloseReceiveChannelMsg::PrintOn(ostream & strm) const
{
  strm << GetClass() << " call=" << m_callIdentifier << " id=" << m_passThruPartyId;
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice & phone, const StartMediaTransmissionMsg & msg)
{
  return DelegateMsg(phone, msg);
}


void OpalSkinnyEndPoint::StartMediaTransmissionMsg::PrintOn(ostream & strm) const
{
  strm << GetClass()
       << " call=" << m_callIdentifier
       << " id=" << m_passThruPartyId
       << ' ' << PIPAddress(m_ip) << ':' << m_port
       << " payload=" << m_payloadCapability;
}


bool OpalSkinnyEndPoint::OnReceiveMsg(PhoneDevice & phone, const StopMediaTransmissionMsg & msg)
{
  return DelegateMsg(phone, msg);
}


void OpalSkinnyEndPoint::StopMediaTransmissionMsg::PrintOn(ostream & strm) const
{
  strm << GetClass() << " call=" << m_callIdentifier << " id=" << m_passThruPartyId;
}


///////////////////////////////////////////////////////////////////////////////

OpalSkinnyConnection::OpalSkinnyConnection(OpalCall & call,
                                           OpalSkinnyEndPoint & ep,
                                           OpalSkinnyEndPoint::PhoneDevice & phone,
                                           unsigned callIdentifier,
                                           const PString & dialNumber,
                                           void * /*userData*/,
                                           unsigned options,
                                           OpalConnection::StringOptions * stringOptions)
  : OpalRTPConnection(call, ep, CreateToken(phone, callIdentifier), options, stringOptions)
  , m_endpoint(ep)
  , m_phoneDevice(phone)
  , m_lineInstance(0)
  , m_callIdentifier(callIdentifier)
  , m_needSoftKeyEndcall(true)
{
  m_calledPartyNumber = dialNumber;
  m_phoneDevice.m_activeConnection = this;
}


PBoolean OpalSkinnyConnection::SetUpConnection()
{
  InternalSetAsOriginating();
  SetPhase(SetUpPhase);
  OnApplyStringOptions();

  // At this point we just go off hook, wait for CallStateMsg which starts dialing
  m_phoneDevice.SendSkinnyMsg(OpalSkinnyEndPoint::OffHookMsg());
  return true;
}


void OpalSkinnyConnection::OnReleased()
{
  if (m_needSoftKeyEndcall) {
    OpalSkinnyEndPoint::SoftKeyEventMsg msg;
    msg.m_event = OpalSkinnyEndPoint::eSoftKeyEndcall;
    msg.m_callIdentifier = m_callIdentifier;
    msg.m_lineInstance = m_lineInstance;
    m_phoneDevice.SendSkinnyMsg(msg);
  }

  m_phoneDevice.m_activeConnection.SetNULL();
  OpalRTPConnection::OnReleased();
}


OpalMediaFormatList OpalSkinnyConnection::GetMediaFormats() const
{
  return m_remoteMediaFormats;
}


PBoolean OpalSkinnyConnection::SetAlerting(const PString & calleeName, PBoolean withMedia)
{
  if (withMedia) {
    // In case we have already received them, try starting them now
    for (set<MediaInfo>::iterator it = m_passThruMedia.begin(); it != m_passThruMedia.end(); ++it)
      OpenMediaChannel(*it);
  }

  return OpalRTPConnection::SetAlerting(calleeName, withMedia);
}


PBoolean OpalSkinnyConnection::SetConnected()
{
  if (GetPhase() >= ConnectedPhase)
    return true;

  SetPhase(ConnectedPhase);

  // In case we have already received them, start them now
  for (set<MediaInfo>::iterator it = m_passThruMedia.begin(); it != m_passThruMedia.end(); ++it)
    OpenMediaChannel(*it);

  OpalSkinnyEndPoint::SoftKeyEventMsg msg;
  msg.m_event = OpalSkinnyEndPoint::eSoftKeyAnswer;
  msg.m_callIdentifier = m_callIdentifier;
  msg.m_lineInstance = m_lineInstance;
  return m_phoneDevice.SendSkinnyMsg(msg);
}


OpalMediaType::AutoStartMode OpalSkinnyConnection::GetAutoStart(const OpalMediaType &) const
{
  // Driven from server, so we don't try and open media streams directly.
  return OpalMediaType::DontOffer;
}


PString OpalSkinnyConnection::GetAlertingType() const
{
  return m_alertingType;
}


OpalTransportAddress OpalSkinnyConnection::GetRemoteAddress() const
{
  return m_phoneDevice.GetRemoteAddress();
}


bool OpalSkinnyConnection::OnReceiveMsg(const OpalSkinnyEndPoint::CallStateMsg & msg)
{
  if (m_callIdentifier != msg.m_callIdentifier) {
    m_callIdentifier = msg.m_callIdentifier;
    PTRACE(3, "Call identifier set to " << m_callIdentifier);
    PString newToken = CreateToken(m_phoneDevice, m_callIdentifier);
    if (callToken != newToken) {
      callToken = newToken;
      PTRACE(3, "Set incoming calls new token to \"" << callToken << '"');
    }
  }

  if (m_lineInstance != msg.m_lineInstance) {
    m_lineInstance = msg.m_lineInstance;
    PTRACE(4, "Line instance set to " << m_lineInstance);
  }

  switch (msg.GetState()) {
    case OpalSkinnyEndPoint::eStateRingIn :
      break;

    case OpalSkinnyEndPoint::eStateOffHook:
      if (IsOriginating()) {
        OnProceeding();
        for (PINDEX i = 0; i < m_calledPartyNumber.GetLength(); ++i) {
          OpalSkinnyEndPoint::KeyPadButtonMsg keypad;
          keypad.m_button = m_calledPartyNumber[i];
          keypad.m_callIdentifier = m_callIdentifier;
          keypad.m_lineInstance = m_lineInstance;
          if (!m_phoneDevice.SendSkinnyMsg(keypad))
            return false;
        }
      }
      break;

    case OpalSkinnyEndPoint::eStateOnHook:
      if (IsEstablished()) {
        m_needSoftKeyEndcall = false;
        Release(EndedByRemoteUser);
      }
      break;

    case OpalSkinnyEndPoint::eStateConnected :
      if (IsOriginating())
        OnConnectedInternal();
      else if (GetPhase() == ConnectedPhase) {
        SetPhase(EstablishedPhase);
        OnEstablished();
      }
      break;

    default :
      PTRACE(4, "Unhandled state: " << msg.GetState());
  }
  return true;
}


bool OpalSkinnyConnection::OnReceiveMsg(const OpalSkinnyEndPoint::CallInfoMsg & msg)
{
  return OnReceiveCallInfo(msg);
}


bool OpalSkinnyConnection::OnReceiveMsg(const OpalSkinnyEndPoint::CallInfo5Msg & msg)
{
  return OnReceiveCallInfo(msg);
}


bool OpalSkinnyConnection::OnReceiveCallInfo(const OpalSkinnyEndPoint::CallInfoCommon & msg)
{
  if (msg.GetType() == OpalSkinnyEndPoint::eTypeOutboundCall) {
    remotePartyName = msg.GetCalledPartyName();
    remotePartyNumber = msg.GetCalledPartyNumber();
    displayName = msg.GetCallingPartyName();
    localPartyName = msg.GetCallingPartyNumber();
  }
  else {
    remotePartyName = msg.GetCallingPartyName();
    remotePartyNumber = msg.GetCallingPartyNumber();
    m_calledPartyName = displayName = msg.GetCalledPartyName();
    m_calledPartyNumber = localPartyName = msg.GetCalledPartyNumber();
    m_redirectingParty = GetPrefixName() + ':' + msg.GetRedirectingPartyNumber();
  }

  if (remotePartyNumber.IsEmpty() && OpalIsE164(remotePartyName))
    remotePartyNumber = remotePartyName;
  if (m_calledPartyNumber.IsEmpty() && OpalIsE164(m_calledPartyName))
    m_calledPartyNumber = m_calledPartyName;

  PTRACE(3, "Local party: number=\"" << localPartyName << "\", name=\"" << displayName << "\" - "
            "Remote party: number=\"" << remotePartyNumber << "\", name=\"" << remotePartyName << "\" "
            "for " << *this);

  if (GetPhase() == UninitialisedPhase) {
    SetPhase(SetUpPhase);
    OnApplyStringOptions();
    if (OnIncomingConnection(0, NULL))
      ownerCall.OnSetUp(*this);
  }
  return true;
}


const char * OpalSkinnyEndPoint::CallInfo5Msg::GetStringByIndex(PINDEX idx) const
{
  const char * str = m_strings;
  while (idx-- > 0)
    str += strlen(str)+1;
  return str;
}

bool OpalSkinnyConnection::OnReceiveMsg(const OpalSkinnyEndPoint::SetRingerMsg & msg)
{
  switch (msg.GetType()) {
    case OpalSkinnyEndPoint::eRingOff :
      return true;

    case OpalSkinnyEndPoint::eRingInside :
      m_alertingType = "Inside";
      break;

    case OpalSkinnyEndPoint::eRingOutside :
      m_alertingType = "Outside";
      break;

    default:
      break;
  }

  return true;
}


void OpalSkinnyConnection::OpenMediaChannel(const MediaInfo & info)
{
  if (info.m_passThruPartyId == 0)
    return;

  OpalMediaFormat mediaFormat = CodecCodeToMediaFormat(info.m_payloadCapability);
  if (mediaFormat.IsEmpty()) {
    PTRACE(2, "Could not find media format for capability=" << info.m_payloadCapability);
    Release(EndedByCapabilityExchange);
    return;
  }

  m_remoteMediaFormats += mediaFormat;

  OpalMediaType mediaType = mediaFormat.GetMediaType();

  if (info.m_sessionId == 0)
    info.m_sessionId = GetNextSessionID(mediaType, info.m_receiver);

  OpalMediaSession * mediaSession = UseMediaSession(info.m_sessionId, mediaType);
  if (mediaSession == NULL) {
    PTRACE(2, "Could not create session " << info.m_sessionId << " for " << mediaFormat);
    Release(EndedByCapabilityExchange);
    return;
  }

  if (!mediaSession->Open(m_phoneDevice.m_transport.GetInterface(), info.m_receiver ? m_phoneDevice.m_transport.GetRemoteAddress() : info.m_mediaAddress, true)) {
    PTRACE(2, "Could not open session " << info.m_sessionId << " for " << mediaFormat);
    return;
  }

  PSafePtr<OpalConnection> con = info.m_receiver ? this : GetOtherPartyConnection();
  if (con == NULL)
    return;

  bool canSimulate = !info.m_receiver && !m_endpoint.GetSimulatedAudioFile().IsEmpty() && mediaType == OpalMediaType::Audio();

  if (canSimulate && info.m_sessionId > 1 && m_endpoint.IsSecondaryAudioAlwaysSimulated()) {
    OpenSimulatedMediaChannel(info.m_sessionId, mediaFormat);
    return;
  }

  if (!ownerCall.OpenSourceMediaStreams(*con, mediaType, info.m_sessionId, mediaFormat)) {
    PTRACE(2, "Could not open " << (info.m_receiver ? 'r' : 't') << "x " << mediaType << " stream, session=" << info.m_sessionId);
    if (canSimulate)
      OpenSimulatedMediaChannel(info.m_sessionId, mediaFormat);
    return;
  }

  PTRACE(3, "Opened " << (info.m_receiver ? 'r' : 't') << "x " << mediaType << " stream, session=" << info.m_sessionId);

  if (info.m_receiver) {
    PIPSocket::AddressAndPort ap;
    mediaSession->GetLocalAddress().GetIpAndPort(ap);

    OpalSkinnyEndPoint::OpenReceiveChannelAckMsg ack;
    ack.m_passThruPartyId = info.m_passThruPartyId;
    ack.m_ip = ap.GetAddress();
    ack.m_port = ap.GetPort();
    m_phoneDevice.SendSkinnyMsg(ack);
  }

  StartMediaStreams();
}


void OpalSkinnyConnection::OpenSimulatedMediaChannel(unsigned sessionId, const OpalMediaFormat & mediaFormat)
{
  m_simulatedTransmitters.insert(sessionId);

  OpalMediaSession * mediaSession = GetMediaSession(sessionId);
  if (mediaSession == NULL) {
    PTRACE(2, "No session " << sessionId << " for " << mediaFormat << " to simulate");
    return;
  }

  if (dynamic_cast<OpalRTPSession *>(mediaSession) == NULL) {
    OpalTransportAddress mediaAddress = mediaSession->GetRemoteAddress();
    mediaSession = new OpalRTPSession(OpalMediaSession::Init(*this, sessionId, mediaFormat.GetMediaType(), false));
    if (!mediaSession->Open(m_phoneDevice.m_transport.GetInterface(), mediaAddress, true)) {
      PTRACE(2, "Could not open RTP session " << sessionId << " for " << mediaFormat << " using " << mediaAddress);
      delete mediaSession;
      return;
    }
    m_sessions.SetAt(sessionId, mediaSession);
  }

  OpalMediaStreamPtr sinkStream = OpenMediaStream(mediaFormat, sessionId, false);
  if (sinkStream == NULL)
    return;

  OpalMediaStreamPtr sourceStream;
  OpalWAVFile * wavFile = new OpalWAVFile(m_endpoint.GetSimulatedAudioFile(), PFile::ReadOnly, PFile::ModeDefault, PWAVFile::fmt_PCM, false);
  if (!wavFile->IsOpen()) {
    PTRACE(3, "Could not simulate transmit " << mediaFormat << " stream, session=" << sessionId
            << ", file=" << m_endpoint.GetSimulatedAudioFile() << ": " << wavFile->GetErrorText());
    return;
  }
  if (wavFile->GetFormatString() == mediaFormat)
    sourceStream = new OpalFileMediaStream(*this, mediaFormat, sessionId, true, wavFile);
  else {
    if (!wavFile->SetAutoconvert()) {
      PTRACE(3, "Could not simulate transmit " << mediaFormat << " stream, session=" << sessionId
              << ", file=" << m_endpoint.GetSimulatedAudioFile() << ": unsupported codec");
      return;
    }
    sourceStream = new OpalFileMediaStream(*this, OpalPCM16, sessionId, true, wavFile);
  }

  if (!sourceStream->Open()) {
    PTRACE(2, "Could not open stream for simulated transmit " << mediaFormat << " stream, session=" << sessionId);
    return;
  }

  mediaStreams.Append(sourceStream);
  OpalMediaPatchPtr patch = m_endpoint.GetManager().CreateMediaPatch(*sourceStream, true);
  if (patch == NULL || !patch->AddSink(sinkStream)) {
    PTRACE(2, "Could not create patch for simulated transmit " << mediaFormat << " stream, session=" << sessionId);
    return;
  }

  PTRACE(3, "Simulating transmit " << mediaFormat << " stream, session=" << sessionId << ", file=" << m_endpoint.GetSimulatedAudioFile());
  StartMediaStreams();
}


bool OpalSkinnyConnection::OnReceiveMsg(const OpalSkinnyEndPoint::OpenReceiveChannelMsg & msg)
{
  std::pair<std::set<MediaInfo>::iterator, bool> result = m_passThruMedia.insert(msg);
  if (result.second)
    OpenMediaChannel(*result.first);
  return true;
}


bool OpalSkinnyConnection::OnReceiveMsg(const OpalSkinnyEndPoint::CloseReceiveChannelMsg & msg)
{
  std::set<MediaInfo>::iterator it = m_passThruMedia.find(msg);
  if (it == m_passThruMedia.end())
    return true;

  OpalMediaStreamPtr mediaStream = GetMediaStream(it->m_sessionId, true);
  if (mediaStream != NULL)
    m_endpoint.GetManager().QueueDecoupledEvent(
                     new PSafeWorkArg1<OpalSkinnyConnection, OpalMediaStreamPtr>(this,
                            mediaStream, &OpalSkinnyConnection::DelayCloseMediaStream));
  return true;
}


bool OpalSkinnyConnection::OnReceiveMsg(const OpalSkinnyEndPoint::StartMediaTransmissionMsg & msg)
{
  std::pair<std::set<MediaInfo>::iterator, bool> result = m_passThruMedia.insert(msg);
  if (result.second)
    OpenMediaChannel(*result.first);
  return true;
}


bool OpalSkinnyConnection::OnReceiveMsg(const OpalSkinnyEndPoint::StopMediaTransmissionMsg & msg)
{
  std::set<MediaInfo>::iterator it = m_passThruMedia.find(msg);
  if (it == m_passThruMedia.end())
    return true;

  OpalMediaStreamPtr mediaStream = GetMediaStream(it->m_sessionId, false);
  if (mediaStream != NULL)
    m_endpoint.GetManager().QueueDecoupledEvent(
                     new PSafeWorkArg1<OpalSkinnyConnection, OpalMediaStreamPtr>(this,
                            mediaStream, &OpalSkinnyConnection::DelayCloseMediaStream));
  return true;
}


void OpalSkinnyConnection::OnClosedMediaStream(const OpalMediaStream & stream)
{
  OpalRTPConnection::OnClosedMediaStream(stream);

  if (IsReleased())
    return;

  unsigned sessionId =stream.GetSessionID();
  if (m_simulatedTransmitters.find(sessionId) == m_simulatedTransmitters.end())
    OpenSimulatedMediaChannel(sessionId, stream.GetMediaFormat());
  else {
    PTRACE(4, "Session " << sessionId << " had already tried simulation, not trying again.");
  }
}


void OpalSkinnyConnection::DelayCloseMediaStream(OpalMediaStreamPtr mediaStream)
{
  /* We delay closing the media stream slightly as Skinny server closes them
    before sending the "on hook" message for ending the call. This means that
    phantom re-INVITE or CLC gets sent when in gateway mode */
  for (PINDEX delay = 0; delay < 10; ++delay) {
    if (IsReleased())
      return;
    PThread::Sleep(50);
  }

  mediaStream->Close();
}


OpalSkinnyConnection::MediaInfo::MediaInfo(const OpalSkinnyEndPoint::OpenReceiveChannelMsg & msg)
  : m_receiver(true)
  , m_passThruPartyId(msg.m_passThruPartyId)
  , m_payloadCapability(msg.m_payloadCapability)
  , m_sessionId(0)
{
}


OpalSkinnyConnection::MediaInfo::MediaInfo(const OpalSkinnyEndPoint::CloseReceiveChannelMsg & msg)
  : m_receiver(true)
  , m_passThruPartyId(msg.m_passThruPartyId)
  , m_payloadCapability(0)
  , m_sessionId(0)
{
}


OpalSkinnyConnection::MediaInfo::MediaInfo(const OpalSkinnyEndPoint::StartMediaTransmissionMsg & msg)
  : m_receiver(false)
  , m_passThruPartyId(msg.m_passThruPartyId)
  , m_payloadCapability(msg.m_payloadCapability)
  , m_mediaAddress(msg.m_ip, msg.m_port, OpalTransportAddress::UdpPrefix())
  , m_sessionId(0)
{
}


OpalSkinnyConnection::MediaInfo::MediaInfo(const OpalSkinnyEndPoint::StopMediaTransmissionMsg & msg)
  : m_receiver(false)
  , m_passThruPartyId(msg.m_passThruPartyId)
  , m_payloadCapability(0)
  , m_sessionId(0)
{
}


bool OpalSkinnyConnection::MediaInfo::operator<(const MediaInfo & other) const
{
  if (!m_receiver && other.m_receiver)
    return true;
  if (m_receiver && !other.m_receiver)
    return false;
  return m_passThruPartyId < other.m_passThruPartyId;
}


#endif // OPAL_SKINNY

///////////////////////////////////////////////////////////////////////////////
