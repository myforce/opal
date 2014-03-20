/*
 * h323ep.cxx
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
 * Contributor(s): Many thanks to Simon Horne.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#include <ptlib.h>

#include <opal_config.h>
#if OPAL_H323

#pragma message("H.323 support enabled")

#ifdef __GNUC__
#pragma implementation "h323ep.h"
#endif

#include <h323/h323ep.h>

#include <ptclib/random.h>
#include <opal/call.h>
#include <h323/h323pdu.h>
#include <h323/gkclient.h>
#include <h323/h323rtp.h>
#include <ptclib/url.h>
#include <ptclib/enum.h>
#include <ptclib/pils.h>


#define new PNEW


/////////////////////////////////////////////////////////////////////////////

H323EndPoint::H323EndPoint(OpalManager & manager)
  : OpalRTPEndPoint(manager, "h323", IsNetworkEndPoint | SupportsE164)
  , autoCallForward(true)
  , disableFastStart(false)
  , disableH245Tunneling(false)
  , disableH245inSetup(false)
  , m_forceSymmetricTCS(false)
  , m_bH245Disabled(false)
  , canDisplayAmountString(false)
  , canEnforceDurationLimit(true)
#if OPAL_H450
  , callIntrusionProtectionLevel(3) //H45011_CIProtectionLevel::e_fullProtection;
#endif
  , terminalType(e_TerminalOnly)
#if OPAL_H239
  , m_defaultH239Control(false)
#endif
  , clearCallOnRoundTripFail(false)
  , signallingChannelCallTimeout(0, 0, 1)  // Minutes
  , firstSignalPduTimeout(0, 2)            // Seconds
  , endSessionTimeout(0, 10)               // Seconds
  , masterSlaveDeterminationTimeout(0, 30) // Seconds
  , masterSlaveDeterminationRetries(10)
  , capabilityExchangeTimeout(0, 30)       // Seconds
  , logicalChannelTimeout(0, 30)           // Seconds
  , requestModeTimeout(0, 30)              // Seconds
  , roundTripDelayTimeout(0, 10)           // Seconds
  , roundTripDelayRate(0, 0, 1)            // Minutes
  , gatekeeperRequestTimeout(0, 5)         // Seconds
  , gatekeeperRequestRetries(2)
  , rasRequestTimeout(0, 3)                // Seconds
  , rasRequestRetries(2)
  , registrationTimeToLive(0, 0, 10)       // Minutes
  , m_sendGRQ(true)
  , m_oneSignalAddressInRRQ(true)
  , callTransferT1(0,10)                   // Seconds
  , callTransferT2(0,10)                   // Seconds
  , callTransferT3(0,10)                   // Seconds
  , callTransferT4(0,10)                   // Seconds
  , callIntrusionT1(0,30)                  // Seconds
  , callIntrusionT2(0,30)                  // Seconds
  , callIntrusionT3(0,30)                  // Seconds
  , callIntrusionT4(0,30)                  // Seconds
  , callIntrusionT5(0,10)                  // Seconds
  , callIntrusionT6(0,10)                  // Seconds
  , m_gatekeeperAliasLimit(MaxGatekeeperAliasLimit)
  , m_gatekeeperSimulatePattern(false)
#if OPAL_H460
  , m_features(NULL)
#endif
#if OPAL_H460_NAT
  , m_H46019Server(NULL)
#endif
{
  localAliasNames.AppendString(defaultLocalPartyName);

  m_connectionsByCallId.DisallowDeleteObjects();
#if OPAL_H450
  m_secondaryConnectionsActive.DisallowDeleteObjects();
#endif

  manager.AttachEndPoint(this, "h323s");

  SetCompatibility(H323Connection::e_NoMultipleTunnelledH245, "Cisco IOS");
  SetCompatibility(H323Connection::e_BadMasterSlaveConflict,  "NetMeeting|HDX");
  SetCompatibility(H323Connection::e_NoUserInputCapability,   "AltiServ-ITG");
  SetCompatibility(H323Connection::e_H224MustBeSession3,      "HDX|Tandberg");
  SetCompatibility(H323Connection::e_NeedMSDAfterNonEmptyTCS, "Avaya|Radvision");
  SetCompatibility(H323Connection::e_ForceMaintainConnection, "Multivantage");

  m_capabilities.AddAllCapabilities(0, 0, "*");
  H323_UserInputCapability::AddAllCapabilities(m_capabilities, P_MAX_INDEX, P_MAX_INDEX);
#if OPAL_H239
  m_capabilities.Add(new H323H239VideoCapability(OpalMediaFormat()));
  m_capabilities.Add(new H323H239ControlCapability());
#endif
#if OPAL_H235_6
  m_capabilities.Add(new H235SecurityAlgorithmCapability(OpalMediaFormat(), 1)); // Just a template for cloning
#endif
#if OPAL_H235_8
  m_capabilities.Add(new H235SecurityGenericCapability(OpalMediaFormat(), 1)); // Just a template for cloning
#endif

  PTRACE(4, "H323\tCreated endpoint.");
}


H323EndPoint::~H323EndPoint()
{
#if OPAL_H460
  delete m_features;
#endif
}


void H323EndPoint::ShutDown()
{
  PTRACE(4, "H323\tShutting down: " << m_reusableTransports.size() << " maintained transports");
  for (set<OpalTransportPtr>::iterator it = m_reusableTransports.begin(); it != m_reusableTransports.end(); ++it)
    (*it)->CloseWait();
  m_reusableTransports.clear();

  /* Unregister request needs/depends OpalEndpoint listeners object, so shut
     down the gatekeeper (if there was one) before cleaning up the OpalEndpoint
     object which kills the listeners. */
  RemoveGatekeeper();

  OpalEndPoint::ShutDown();
}


PBoolean H323EndPoint::GarbageCollection()
{
  for (set<OpalTransportPtr>::iterator it = m_reusableTransports.begin(); it != m_reusableTransports.end(); ) {
    if ((*it)->IsOpen())
      ++it;
    else {
      PTRACE(4, "H323\tRemoving maintained transport " << **it);
      (*it)->CloseWait();
      m_reusableTransports.erase(it++);
    }
  }

  return OpalRTPEndPoint::GarbageCollection();
}


PString H323EndPoint::GetDefaultTransport() const
{
  return OpalTransportAddress::TcpPrefix()
#if OPAL_PTLIB_SSL
            + ',' + OpalTransportAddress::TlsPrefix() + ":1300"
#endif
    ;
}


WORD H323EndPoint::GetDefaultSignalPort() const
{
  return DefaultTcpSignalPort;
}


void H323EndPoint::SetEndpointTypeInfo(H225_EndpointType & info) const
{
  info.IncludeOptionalField(H225_EndpointType::e_vendor);
  SetVendorIdentifierInfo(info.m_vendor);

  switch (terminalType) {
    case e_TerminalOnly :
    case e_TerminalAndMC :
      info.IncludeOptionalField(H225_EndpointType::e_terminal);
      break;

    case e_GatewayOnly :
    case e_GatewayAndMC :
    case e_GatewayAndMCWithDataMP :
    case e_GatewayAndMCWithAudioMP :
    case e_GatewayAndMCWithAVMP :
      info.IncludeOptionalField(H225_EndpointType::e_gateway);
      if (SetGatewaySupportedProtocol(info.m_gateway.m_protocol))
        info.m_gateway.IncludeOptionalField(H225_GatewayInfo::e_protocol);
      break;

    case e_GatekeeperOnly :
    case e_GatekeeperWithDataMP :
    case e_GatekeeperWithAudioMP :
    case e_GatekeeperWithAVMP :
      info.IncludeOptionalField(H225_EndpointType::e_gatekeeper);
      break;

    case e_MCUOnly :
    case e_MCUWithDataMP :
    case e_MCUWithAudioMP :
    case e_MCUWithAVMP :
      info.IncludeOptionalField(H225_EndpointType::e_mcu);
      info.m_mc = true;
      if (SetGatewaySupportedProtocol(info.m_mcu.m_protocol))
        info.m_mcu.IncludeOptionalField(H225_McuInfo::e_protocol);
  }
}


void H323EndPoint::SetVendorIdentifierInfo(H225_VendorIdentifier & info) const
{
  SetH221NonStandardInfo(info.m_vendor);

  info.IncludeOptionalField(H225_VendorIdentifier::e_productId);
  info.m_productId = productInfo.vendor & productInfo.name;
  info.m_productId.SetSize(info.m_productId.GetSize()+2);

  info.IncludeOptionalField(H225_VendorIdentifier::e_versionId);
  info.m_versionId = productInfo.version + " (OPAL v" + OpalGetVersion() + ')';
  info.m_versionId.SetSize(info.m_versionId.GetSize()+2);
}


void H323EndPoint::SetH221NonStandardInfo(H225_H221NonStandard & info) const
{
  info.m_t35CountryCode = productInfo.t35CountryCode;
  info.m_t35Extension = productInfo.t35Extension;
  info.m_manufacturerCode = productInfo.manufacturerCode;
}


bool H323EndPoint::SetGatewaySupportedProtocol(H225_ArrayOf_SupportedProtocols & protocols) const
{
  PStringList prefixes;

  if (!OnSetGatewayPrefixes(prefixes))
    return false;

  PINDEX count = protocols.GetSize();
  protocols.SetSize(count+1);

  protocols[count].SetTag(H225_SupportedProtocols::e_h323);
  H225_H323Caps & caps = protocols[count];

  caps.IncludeOptionalField(H225_H323Caps::e_supportedPrefixes);
  H225_ArrayOf_SupportedPrefix & supportedPrefixes = caps.m_supportedPrefixes;  
  supportedPrefixes.SetSize(prefixes.GetSize());

  for (PINDEX i = 0; i < prefixes.GetSize(); i++)
    H323SetAliasAddress(prefixes[i], supportedPrefixes[i].m_prefix);

  return true;
}


bool H323EndPoint::OnSetGatewayPrefixes(PStringList & /*prefixes*/) const
{
  return false;
}


bool H323EndPoint::UseGatekeeper(const PString & address,
                                 const PString & identifier,
                                 const PString & localAddress)
{
  H323Gatekeeper * gatekeeper = GetGatekeeper();
  if (gatekeeper != NULL) {
    bool same = true;

    if (!address && address != "*")
      same = gatekeeper->GetTransport().GetRemoteAddress().IsEquivalent(address);

    if (!same && !identifier)
      same = gatekeeper->GetIdentifier() == identifier;

    if (!same && !localAddress)
      same = gatekeeper->GetTransport().GetLocalAddress().IsEquivalent(localAddress);

    if (same) {
      PTRACE(3, "H323\tUsing existing gatekeeper " << *gatekeeper);
      return true;
    }
  }

  if (address.IsEmpty() || address == "*") {
    if (identifier.IsEmpty())
      return DiscoverGatekeeper(localAddress);
    else
      return LocateGatekeeper(identifier, localAddress);
  }
  else {
    if (identifier.IsEmpty())
      return SetGatekeeper(address, localAddress);
    else
      return SetGatekeeperZone(address, identifier, localAddress);
  }
}


static H323TransportAddress GetGatekeeperAddress(const PString & address)
{
#if OPAL_PTLIB_DNS_RESOLVER
  PIPSocketAddressAndPortVector addresses;
  if (PDNS::LookupSRV(address, "_h323rs._udp", H225_RAS::DefaultRasUdpPort, addresses)) {
    PTRACE(4, "H323\tUsing DNS SRV record for gatekeeper at " << addresses[0]);
    return OpalTransportAddress(addresses[0], OpalTransportAddress::UdpPrefix());
  }
#endif //OPAL_PTLIB_DNS_RESOLVER

  return H323TransportAddress(address, H225_RAS::DefaultRasUdpPort, OpalTransportAddress::UdpPrefix());
}


bool H323EndPoint::SetGatekeeper(const PString & remoteAddress, const PString & localAddress)
{
  H323TransportAddress h323addr = GetGatekeeperAddress(remoteAddress);
  if (!InternalCreateGatekeeper(h323addr, localAddress))
    return false;

  for (PList<H323Gatekeeper>::iterator it = m_gatekeepers.begin(); it != m_gatekeepers.end(); ++it) {
    if (!it->DiscoverByAddress(h323addr))
      return false;
  }

  return true;
}


bool H323EndPoint::SetGatekeeperZone(const PString & remoteAddress,
                                     const PString & identifier,
                                     const PString & localAddress)
{
  H323TransportAddress h323addr = GetGatekeeperAddress(remoteAddress);
  if (!InternalCreateGatekeeper(h323addr, localAddress))
    return false;

  for (PList<H323Gatekeeper>::iterator it = m_gatekeepers.begin(); it != m_gatekeepers.end(); ++it) {
    if (!it->DiscoverByNameAndAddress(identifier, h323addr))
      return false;
  }

  return true;
}


PBoolean H323EndPoint::LocateGatekeeper(const PString & identifier, const PString & localAddress)
{
  if (!InternalCreateGatekeeper(H323TransportAddress(), localAddress))
    return false;

  for (PList<H323Gatekeeper>::iterator it = m_gatekeepers.begin(); it != m_gatekeepers.end(); ++it) {
    if (!it->DiscoverByName(identifier))
      return false;
  }

  return true;
}


PBoolean H323EndPoint::DiscoverGatekeeper(const PString & localAddress)
{
  if (!InternalCreateGatekeeper(H323TransportAddress(), localAddress))
    return false;

  for (PList<H323Gatekeeper>::iterator it = m_gatekeepers.begin(); it != m_gatekeepers.end(); ++it) {
    if (!it->DiscoverAny())
      return false;
  }

  return true;
}


bool H323EndPoint::InternalCreateGatekeeper(const H323TransportAddress & remoteAddress, const PString & localAddress)
{
  RemoveGatekeeper(H225_UnregRequestReason::e_reregistrationRequired);

  PIPSocket::Address interfaceIP(PIPSocket::GetInvalidAddress());

  if (localAddress.IsEmpty() || !OpalTransportAddress(localAddress).GetIpAddress(interfaceIP)) {
    // See if the system can tell us which interface would be used
    {
      PIPSocket::Address remoteIP;
      if (remoteAddress.GetIpAddress(remoteIP) && !remoteIP.IsAny()) {
        interfaceIP = PIPSocket::GetRouteInterfaceAddress(remoteIP);
        PTRACE_IF(4, interfaceIP.IsValid(), "H323\tUsing interface " << interfaceIP << " routed from " << remoteIP);
      }
    }

    if (!interfaceIP.IsValid()) {
      OpalTransportAddressArray interfaces = GetInterfaceAddresses();
      for (PINDEX i = 0; i < interfaces.GetSize(); ++i) {
        if (interfaces[i].IsCompatible(remoteAddress)) {
          PTRACE(4, "H323\tInterface " << interfaces[i] << " compatible with " << remoteAddress);
          if (interfaceIP.IsValid())
            interfaces[i].GetIpAddress(interfaceIP);
          else {
            // More than one so do gk on all interfaces
            interfaceIP = PIPSocket::GetDefaultIpAny();
            break;
          }
        }
      }
      if (!interfaceIP.IsValid()) {
        PTRACE(2, "H323\tCannot find a compatible listener for \"" << remoteAddress << '"');
        return false;
      }
    }
  }

  PStringList allAliases = localAliasNames;
  if (GetGatekeeperSimulatePattern() && !localAliasPatterns.IsEmpty()) {
    allAliases.MakeUnique();
    for (PStringList::iterator it = localAliasPatterns.begin(); it != localAliasPatterns.end(); ++it) {
      PString start, end;
      PINDEX fixedDigits = ParseAliasPatternRange(*it, start, end);
      while (start != end) {
        allAliases += start;
        for (PINDEX i = start.GetLength() - 1; i >= fixedDigits; --i) {
          if (start[i] < '9') {
            ++start[i];
            break;
          }
          start[i] = '0';
        }
      }
      allAliases += end;
    }
  }

  PStringList aliasSubset;
  for (PStringList::iterator it = allAliases.begin(); it != allAliases.end(); ++it) {
    aliasSubset += *it;
    if (aliasSubset.GetSize() >= m_gatekeeperAliasLimit || &*it == &allAliases.back()) {
      H323Gatekeeper * gatekeeper = CreateGatekeeper(new OpalTransportUDP(*this, interfaceIP));
      if (gatekeeper == NULL)
        return false;

      PTRACE(3, "H323\tAdded gatekeeper for aliases: " << setfill(',') << aliasSubset);
      gatekeeper->SetAliases(aliasSubset);
      gatekeeper->SetPassword(GetGatekeeperPassword(), GetGatekeeperUsername());
      m_gatekeepers.Append(gatekeeper);

      aliasSubset = PStringList(); /// Don't use RemoveAll() which does it for all references
    }
  }

  return true;
}


H323Gatekeeper * H323EndPoint::CreateGatekeeper(H323Transport * transport)
{
  return new H323Gatekeeper(*this, transport);
}


PBoolean H323EndPoint::IsRegisteredWithGatekeeper() const
{
  if (m_gatekeepers.IsEmpty())
    return false;

  return m_gatekeepers.front().IsRegistered();
}


PBoolean H323EndPoint::RemoveGatekeeper(int reason)
{
  PTRACE(3, "H323\tRemoving gatekeeper");

  bool ok = true;

  for (PList<H323Gatekeeper>::iterator it = m_gatekeepers.begin(); it != m_gatekeepers.end(); ++it) {
    if (it->IsRegistered()) { // If we are registered send a URQ
      ClearAllCalls();
      if (!it->UnregistrationRequest(reason))
        ok = false;
    }
  }

  m_gatekeepers.RemoveAll();
  return ok;
}


void H323EndPoint::SetGatekeeperPassword(const PString & password, const PString & username)
{
  m_gatekeeperUsername = username;
  m_gatekeeperPassword = password;

  for (PList<H323Gatekeeper>::iterator it = m_gatekeepers.begin(); it != m_gatekeepers.end(); ++it) {
    it->SetPassword(GetGatekeeperPassword(), GetGatekeeperUsername());
    if (it->IsRegistered()) { // If we are registered send a URQ
      it->UnregistrationRequest(H225_UnregRequestReason::e_reregistrationRequired);
      it->RegistrationRequest();
    }
  }
}


void H323EndPoint::SetGatekeeperAliasLimit(PINDEX limit)
{
  if (m_gatekeeperAliasLimit == limit)
    return;

  m_gatekeeperAliasLimit = limit;

  if (m_gatekeepers.IsEmpty())
    return;

  OpalTransportAddress remoteAddress = m_gatekeepers.front().GetTransport().GetRemoteAddress();
  PString localAddress = m_gatekeepers.front().GetTransport().GetInterface();

  RemoveGatekeeper();
  SetGatekeeper(remoteAddress, localAddress);
}


void H323EndPoint::OnGatekeeperStatus(H323Gatekeeper::RegistrationFailReasons)
{
}



H235Authenticators H323EndPoint::CreateAuthenticators()
{
  H235Authenticators authenticators;

  PFactory<H235Authenticator>::KeyList_T keyList = PFactory<H235Authenticator>::GetKeyList();
  for (PFactory<H235Authenticator>::KeyList_T::const_iterator it = keyList.begin(); it != keyList.end(); ++it) {
    H235Authenticator * auth = PFactory<H235Authenticator>::CreateInstance(*it);
    if (auth->GetApplication() == H235Authenticator::GKAdmission || auth->GetApplication() == H235Authenticator::AnyApplication)
      authenticators.Append(auth);
    else
      delete auth;
  }

  return authenticators;
}


PSafePtr<OpalConnection> H323EndPoint::MakeConnection(OpalCall & call,
                                                 const PString & remoteParty,
                                                          void * userData,
                                                    unsigned int options,
                                 OpalConnection::StringOptions * stringOptions)
{
  if (listeners.IsEmpty())
    return NULL;

  PTRACE(3, "H323\tMaking call to: " << remoteParty);
  return InternalMakeCall(call,
                          PString::Empty(),
                          PString::Empty(),
                          UINT_MAX,
                          remoteParty,
                          userData,
                          options,
                          stringOptions);
}


PStringList H323EndPoint::GetAvailableStringOptions() const
{
  static char const * const StringOpts[] = {
    OPAL_OPT_Q931_BEARER_CAPS
  };

  PStringList list = OpalEndPoint::GetAvailableStringOptions();
  list += PStringList(PARRAYSIZE(StringOpts), StringOpts, true);
  return list;
}


void H323EndPoint::OnReleased(OpalConnection & connection)
{
  m_connectionsByCallId.RemoveAt(connection.GetIdentifier());

  OpalTransportPtr signallingChannel = dynamic_cast<H323Connection &>(connection).GetSignallingChannel();
  if (signallingChannel != NULL && signallingChannel->IsOpen()) {
    PTRACE(3, "H323", "Maintaining TCP connection: " << *signallingChannel);
    m_reusableTransports.insert(signallingChannel);
    signallingChannel->AttachThread(new PThreadObj2Arg<H323EndPoint, OpalTransportPtr, bool>(*this,
                signallingChannel, true, &H323EndPoint::InternalNewIncomingConnection, false, "H225 Answer"));
  }

  OpalRTPEndPoint::OnReleased(connection);
}


void H323EndPoint::NewIncomingConnection(OpalListener &, const OpalTransportPtr & transport)
{
  if (transport != NULL)
    InternalNewIncomingConnection(transport);
}


void H323EndPoint::InternalNewIncomingConnection(OpalTransportPtr transport, bool reused)
{
  if (transport == NULL)
    return;

  PTRACE(4, "H225\tAwaiting first PDU on " << (reused ? "reused" : "initial") << " connection " << *transport);
  transport->SetReadTimeout(GetFirstSignalPduTimeout());

  H323SignalPDU pdu;
  do {
    if (!pdu.Read(*transport)) {
      if (reused) {
        PTRACE(4, "H225\tReusable TCP connection not reused.");
        transport->Close();
        return;
      }

      PTRACE(2, "H225\tFailed to get initial Q.931 PDU, connection not started.");
      return;
    }
  } while (pdu.GetQ931().GetMessageType() != Q931::SetupMsg);

  unsigned callReference = pdu.GetQ931().GetCallReference();
  PTRACE(3, "H225\tIncoming call, first PDU: callReference=" << callReference
         << " on " << (reused ? "reused" : "initial") << " connection " << *transport);

  // Get a new (or old) connection from the endpoint, calculate token
  PString token = transport->GetRemoteAddress();
  token.sprintf("/%u", callReference);

  PSafePtr<H323Connection> connection = PSafePtrCast<OpalConnection, H323Connection>(GetConnectionWithLock(token, PSafeReadWrite));
  if (connection == NULL) {
    // Get new instance of a call, abort if none created
    OpalCall * call = manager.InternalCreateCall();
    if (call != NULL) {
      PTRACE_CONTEXT_ID_SET(*PThread::Current(), call);
      connection = CreateConnection(*call, token, NULL, *transport, PString::Empty(), PString::Empty(), &pdu);
      PTRACE(3, "H323\tCreated new connection: " << token);
    }
  }
  else {
    PTRACE_CONTEXT_ID_SET(*PThread::Current(), connection);
  }

  // Make sure transport is attached before AddConnection()
  if (connection != NULL) {
    m_reusableTransports.erase(transport);
    connection->AttachSignalChannel(token, transport, true);
  }

  if (AddConnection(connection) != NULL && connection->HandleSignalPDU(pdu)) {
    m_connectionsByCallId.SetAt(connection->GetIdentifier(), connection);
    // All subsequent PDU's should wait forever
    transport->SetReadTimeout(PMaxTimeInterval);
    connection->HandleSignallingChannel();
    return;
  }

  PTRACE(1, "H225\tEndpoint could not create connection, "
            "sending release complete PDU: callRef=" << callReference);

  H323SignalPDU releaseComplete;
  Q931 &q931PDU = releaseComplete.GetQ931();
  q931PDU.BuildReleaseComplete(callReference, true);
  releaseComplete.m_h323_uu_pdu.m_h323_message_body.SetTag(H225_H323_UU_PDU_h323_message_body::e_releaseComplete);

  H225_ReleaseComplete_UUIE &release = releaseComplete.m_h323_uu_pdu.m_h323_message_body;
  release.m_protocolIdentifier.SetValue(psprintf("0.0.8.2250.0.%u", H225_PROTOCOL_VERSION));

  H225_Setup_UUIE &setup = pdu.m_h323_uu_pdu.m_h323_message_body;
  if (setup.HasOptionalField(H225_Setup_UUIE::e_callIdentifier)) {
    release.IncludeOptionalField(H225_Setup_UUIE::e_callIdentifier);
    release.m_callIdentifier = setup.m_callIdentifier;
  }

  // Set the cause value
  q931PDU.SetCause(Q931::TemporaryFailure);

  // Send the PDU
  releaseComplete.Write(*transport);
}


H323Connection * H323EndPoint::CreateConnection(OpalCall & call,
                                                const PString & token,
                                                void * /*userData*/,
                                                OpalTransport & /*transport*/,
                                                const PString & alias,
                                                const H323TransportAddress & address,
                                                H323SignalPDU * /*setupPDU*/,
                                                unsigned options,
                                                OpalConnection::StringOptions * stringOptions)
{
  return new H323Connection(call, *this, token, alias, address, options, stringOptions);
}


PBoolean H323EndPoint::SetupTransfer(const PString & oldToken,
                                 const PString & callIdentity,
                                 const PString & remoteParty,
                                 void * userData)
{
  // Make a new connection
  PSafePtr<OpalConnection> otherConnection = GetConnectionWithLock(oldToken, PSafeReference);
  if (otherConnection == NULL)
    return false;

  OpalCall & call = otherConnection->GetCall();

  call.CloseMediaStreams();

  PTRACE(3, "H323\tTransferring call to: " << remoteParty);
  PBoolean ok = InternalMakeCall(call, oldToken, callIdentity, UINT_MAX, remoteParty, userData) != NULL;
  call.OnReleased(*otherConnection);
  otherConnection->Release(OpalConnection::EndedByCallForwarded);

  return ok;
}


H323Connection * H323EndPoint::InternalMakeCall(OpalCall & call,
                                           const PString & existingToken,
#if OPAL_H450
                                           const PString & callIdentity,
                                                  unsigned capabilityLevel,
#else
                                           const PString & ,
                                                  unsigned ,
#endif
                                           const PString & remoteParty,
                                                    void * userData,
                                              unsigned int options,
                           OpalConnection::StringOptions * stringOptions)
{
  OpalConnection::StringOptions localStringOptions;
  if (stringOptions == NULL)
    stringOptions = &localStringOptions;

  PString alias;
  H323TransportAddress address;
  if (!ParsePartyName(remoteParty, alias, address, stringOptions)) {
    PTRACE(2, "H323\tCould not parse \"" << remoteParty << '"');
    return NULL;
  }

  // Restriction: the call must be made on the same local interface as the one
  // that the gatekeeper is using.
  H323Transport * transport;
  H323Gatekeeper * gatekeeper = GetGatekeeper();
  if (gatekeeper == NULL && !stringOptions->Contains(OPAL_OPT_INTERFACE))
    transport = address.CreateTransport(*this, OpalTransportAddress::NoBinding);
  else {
    OpalTransportAddress localInterface = stringOptions->GetString(OPAL_OPT_INTERFACE, gatekeeper->GetTransport().GetInterface());
    transport = localInterface.CreateTransport(*this, OpalTransportAddress::HostOnly);
  }

  if (transport == NULL) {
    PTRACE(1, "H323\tInvalid transport in \"" << remoteParty << '"');
    return NULL;
  }

  PString newToken(PString::Printf, "localhost/%u", Q931::GenerateCallReference());

  H323Connection * connection = CreateConnection(call, newToken, userData, *transport, alias, address, NULL, options, stringOptions);
  if (!AddConnection(connection)) {
    PTRACE(1, "H225\tEndpoint could not create connection, aborting setup.");
    return NULL;
  }

  connection->AttachSignalChannel(newToken, transport, false);

#if OPAL_H450
  if (!callIdentity) {
    if (capabilityLevel == UINT_MAX)
      connection->HandleTransferCall(existingToken, callIdentity);
    else {
      connection->HandleIntrudeCall(existingToken, callIdentity);
      connection->IntrudeCall(capabilityLevel);
    }
  }
#endif

  PTRACE(3, "H323\tCreated new connection: " << newToken);

  // See if we are starting an outgoing connection as first in a call
  if (call.GetConnection(0) == (OpalConnection*)connection || !existingToken.IsEmpty())
    connection->SetUpConnection();
  m_connectionsByCallId.SetAt(connection->GetIdentifier(), connection);

  return connection;
}


#if OPAL_H450

void H323EndPoint::TransferCall(const PString & token, 
                                const PString & remoteParty,
                                const PString & callIdentity)
{
  PSafePtr<H323Connection> connection = FindConnectionWithLock(token);
  if (connection != NULL)
    connection->TransferCall(remoteParty, callIdentity);
}


void H323EndPoint::ConsultationTransfer(const PString & primaryCallToken,   
                                        const PString & secondaryCallToken)
{
  PSafePtr<H323Connection> secondaryCall = FindConnectionWithLock(secondaryCallToken);
  if (secondaryCall != NULL)
    secondaryCall->ConsultationTransfer(primaryCallToken);
}


PBoolean H323EndPoint::IntrudeCall(const PString & remoteParty,
                               unsigned capabilityLevel,
                               void * userData)
{
  // Get new instance of a call, abort if none created
  OpalCall * call = manager.InternalCreateCall();
  if (call == NULL)
    return false;

  return InternalMakeCall(*call, PString::Empty(), PString::Empty(), capabilityLevel, remoteParty, userData) != NULL;
}

#endif

PBoolean H323EndPoint::OnSendConnect(H323Connection & /*connection*/,
                                 H323SignalPDU & /*connectPDU*/
                                )
{
  return true;
}

PBoolean H323EndPoint::OnSendSignalSetup(H323Connection & /*connection*/,
                                     H323SignalPDU & /*setupPDU*/)
{
  return true;
}

PBoolean H323EndPoint::OnSendCallProceeding(H323Connection & /*connection*/,
                                        H323SignalPDU & /*callProceedingPDU*/)
{
  return true;
}

void H323EndPoint::OnReceivedInitiateReturnError()
{
}

PBoolean H323EndPoint::ParsePartyName(const PString & remoteParty,
                                            PString & alias,
                               H323TransportAddress & address,
                       OpalConnection::StringOptions * stringOptions)
{
  PURL url(remoteParty, GetPrefixName()); // Parses as per RFC3508

  if (stringOptions != NULL)
    stringOptions->ExtractFromURL(url);

#if OPAL_PTLIB_DNS_RESOLVER

  // if there is no gatekeeper, try altarnate address lookup methods
  if (m_gatekeepers.IsEmpty()) {
    if (url.GetHostName().IsEmpty()) {
      // No host, so lets try ENUM on the username part, and only has digits and +
      PString username = url.GetUserName();
      if (OpalIsE164(username)) {
        PString newName;
        if (PDNS::ENUMLookup(username, "E2U+h323", newName)) {
          PTRACE(4, "H323\tENUM converted remote party " << username << " to " << newName);
          url.Parse(newName, GetPrefixName());
        }
      }
    }

    /* URL default for H.323 is if no @ then is username, but that does
       not make sense when no GK, so we switch to being a host only */
    if (url.GetHostName().IsEmpty()) {
      if (url.GetScheme() == "tel") {
        PTRACE(2, "H323\tCannot use tel URI without phone-context or active gatekeeper");
        return false;
      }
      url.SetHostName(url.GetUserName());
      url.SetUserName(PString::Empty());
    }

    // If it is a valid IP address then can't be a domain so do not try SRV record lookup
    PIPSocketAddressAndPortVector addresses;
    if (PDNS::LookupSRV(url.GetHostName(), "_h323cs._tcp", url.GetPort(), addresses) && !addresses.empty()) {
      // Only use first entry
      url.SetHostName(addresses[0].GetAddress().AsString());
      url.SetPort(addresses[0].GetPort());
    }
  }

#endif // OPAL_PTLIB_DNS_RESOLVER

  address = OpalTransportAddress(url.GetHostName(), url.GetPort(),
#if OPAL_PTLIB_SSL
                                 url.GetScheme() == "h323s" ? OpalTransportAddress::TlsPrefix() :
#endif
                                 OpalTransportAddress::TcpPrefix());
  alias = url.GetUserName();
  if (alias.IsEmpty() && address.IsEmpty()) {
    PTRACE(1, "H323\tAttempt to use invalid URL \"" << remoteParty << '"');
    return false;
  }

  PBoolean gatekeeperSpecified = false;
  PBoolean gatewaySpecified = false;

  PCaselessString type = url.GetParamVars()("type");

  if (url.GetScheme() == "callto") {
    // Do not yet support ILS
    if (type == "directory") {
#if OPAL_PTLIB_LDAP
      PString server = url.GetHostName();
      if (server.IsEmpty())
        server = GetDefaultILSServer();
      if (server.IsEmpty())
        return false;

      PILSSession ils;
      if (!ils.Open(server, url.GetPort())) {
        PTRACE(1, "H323\tCould not open ILS server at \"" << server
               << "\" - " << ils.GetErrorText());
        return false;
      }

      PILSSession::RTPerson person;
      if (!ils.SearchPerson(alias, person)) {
        PTRACE(1, "H323\tCould not find "
               << server << '/' << alias << ": " << ils.GetErrorText());
        return false;
      }

      if (!person.sipAddress.IsValid()) {
        PTRACE(1, "H323\tILS user " << server << '/' << alias
               << " does not have a valid IP address");
        return false;
      }

      // Get the IP address
      address = OpalTransportAddress(person.sipAddress);

      // Get the port if provided
      for (PINDEX i = 0; i < person.sport.GetSize(); i++) {
        if (person.sport[i] != 1503) { // Dont use the T.120 port
          address = H323TransportAddress(person.sipAddress, person.sport[i]);
          break;
        }
      }

      alias = PString::Empty(); // No alias for ILS lookup, only host
      return true;
#else
      return false;
#endif
    }

    if (url.GetParamVars().Contains("gateway"))
      gatewaySpecified = true;
  }
  else if (url.GetScheme() == "h323") {
    if (type == "gw")
      gatewaySpecified = true;
    else if (type == "gk")
      gatekeeperSpecified = true;
    else if (!type) {
      PTRACE(1, "H323\tUnsupported host type \"" << type << "\" in h323 URL");
      return false;
    }
  }

  // User explicitly asked fo a look up to a gk
  if (gatekeeperSpecified) {
    if (alias.IsEmpty()) {
      PTRACE(1, "H323\tAttempt to use explict gatekeeper without alias!");
      return false;
    }

    if (address.IsEmpty()) {
      PTRACE(1, "H323\tAttempt to use explict gatekeeper without address!");
      return false;
    }

    H323TransportAddress gkAddr(url.GetHostName(), url.GetPort(), OpalTransportAddress::UdpPrefix());
    PTRACE(3, "H323\tLooking for \"" << alias << "\" on gatekeeper at " << gkAddr);

    H323Gatekeeper * gk = CreateGatekeeper(new H323TransportUDP(*this));

    PBoolean ok = gk->DiscoverByAddress(gkAddr);
    if (ok) {
      ok = gk->LocationRequest(alias, address);
      if (ok) {
        PTRACE(3, "H323\tLocation Request of \"" << alias << "\" on gk " << gkAddr << " found " << address);
      }
      else {
        PTRACE(1, "H323\tLocation Request failed for \"" << alias << "\" on gk " << gkAddr);
      }
    }
    else {
      PTRACE(1, "H323\tLocation Request discovery failed for gk " << gkAddr);
    }

    delete gk;

    return ok;
  }

  // User explicitly said to use a gw, or we do not have a gk to do look up
  if (m_gatekeepers.IsEmpty() || gatewaySpecified) {
    /* If URL did not have an '@' as per RFC3508 we get an alias but no
       address, but user said to use gw, or we do not have a gk to do a lookup
       so we MUST have a host. So, we swap the alias into the address field
       and blank out the alias. */
    if (address.IsEmpty()) {
      PStringArray transports = GetDefaultTransport().Tokenise(',');
      address = H323TransportAddress(alias, GetDefaultSignalPort(), transports[0]);
      alias = PString::Empty();
    }
    return true;
  }

  // We have a gatekeeper and user provided a host, all done
  if (!address)
    return true;

  // We have a gk and user did not explicitly supply a host, so lets
  // do a check to see if it is an IP address 
  if (alias.FindRegEx("^[0-9][0-9]*\\.[0-9][0-9]*\\.[0-9][0-9]*\\.[0-9][0-9]*") != P_MAX_INDEX) {
    PINDEX colon = alias.Find(':');
    WORD port = GetDefaultSignalPort();
    if (colon != P_MAX_INDEX) {
      port = (WORD)alias.Mid(colon+1).AsInteger();
      alias = alias.Left(colon);
    }
    PIPSocket::Address ip(alias);
    if (ip.IsValid()) {
      alias = PString::Empty();
      address = H323TransportAddress(ip, port);
    }
  }

  return true;
}


PSafePtr<H323Connection> H323EndPoint::FindConnectionWithLock(const PString & token, PSafetyMode mode)
{
  PSafePtr<H323Connection> connection = PSafePtrCast<OpalConnection, H323Connection>(GetConnectionWithLock(token, mode));
  if (connection == NULL)
    connection = m_connectionsByCallId.FindWithLock(token, mode);

  return connection;
}


PBoolean H323EndPoint::OnIncomingCall(H323Connection & connection,
                                  const H323SignalPDU & /*setupPDU*/,
                                  H323SignalPDU & /*alertingPDU*/)
{
  return connection.OnIncomingConnection(0, NULL);
}


PBoolean H323EndPoint::OnCallTransferInitiate(H323Connection & /*connection*/,
                                          const PString & /*remoteParty*/)
{
  return true;
}


PBoolean H323EndPoint::OnCallTransferIdentify(H323Connection & /*connection*/)
{
  return true;
}


void H323EndPoint::OnSendARQ(H323Connection & /*conn*/, H225_AdmissionRequest & /*arq*/)
{
}


OpalConnection::AnswerCallResponse
       H323EndPoint::OnAnswerCall(H323Connection & connection,
                                  const PString & caller,
                                  const H323SignalPDU & /*setupPDU*/,
                                  H323SignalPDU & /*connectPDU*/,
                                  H323SignalPDU & /*progressPDU*/)
{
  return connection.OnAnswerCall(caller);
}

OpalConnection::AnswerCallResponse
       H323EndPoint::OnAnswerCall(OpalConnection & connection,
       const PString & caller
)
{
  return OpalEndPoint::OnAnswerCall(connection, caller);
}

PBoolean H323EndPoint::OnAlerting(H323Connection & connection,
                              const H323SignalPDU & /*alertingPDU*/,
                              const PString & /*username*/)
{
  PTRACE(3, "H225\tReceived alerting PDU.");
  ((OpalConnection&)connection).OnAlerting();
  return true;
}

PBoolean H323EndPoint::OnSendAlerting(H323Connection & PTRACE_PARAM(connection),
                                  H323SignalPDU & /*alerting*/,
                                  const PString & /*calleeName*/,   /// Name of endpoint being alerted.
                                  PBoolean /*withMedia*/                /// Open media with alerting
                                  )
{
  PTRACE(3, "H225\tOnSendAlerting conn = " << connection);
  return true;
}

PBoolean H323EndPoint::OnSentAlerting(H323Connection & PTRACE_PARAM(connection))
{
  PTRACE(3, "H225\tOnSentAlerting conn = " << connection);
  return true;
}

PBoolean H323EndPoint::OnConnectionForwarded(H323Connection & /*connection*/,
                                         const PString & /*forwardParty*/,
                                         const H323SignalPDU & /*pdu*/)
{
  return false;
}


PBoolean H323EndPoint::ForwardConnection(H323Connection & connection,
                                     const PString & forwardParty,
                                     const H323SignalPDU & /*pdu*/)
{
  if (InternalMakeCall(connection.GetCall(),
                       connection.GetCallToken(),
                       PString::Empty(),
                       UINT_MAX,
                       forwardParty,
                       NULL) == NULL)
    return false;

  connection.SetCallEndReason(H323Connection::EndedByCallForwarded);

  return true;
}


void H323EndPoint::OnConnectionEstablished(H323Connection & /*connection*/,
                                           const PString & /*token*/)
{
}


PBoolean H323EndPoint::IsConnectionEstablished(const PString & token)
{
  PSafePtr<H323Connection> connection = FindConnectionWithLock(token);
  return connection != NULL && connection->IsEstablished();
}


PBoolean H323EndPoint::OnOutgoingCall(H323Connection & /*connection*/,
                                  const H323SignalPDU & /*connectPDU*/)
{
  PTRACE(3, "H225\tReceived connect PDU.");
  return true;
}


#if PTRACING
static void OnStartStopChannel(const char * startstop, const H323Channel & channel)
{
  const char * dir;
  switch (channel.GetDirection()) {
    case H323Channel::IsTransmitter :
      dir = "send";
      break;

    case H323Channel::IsReceiver :
      dir = "receiv";
      break;

    default :
      dir = "us";
      break;
  }

  PTRACE(3, "H323\t" << startstop << "ed "
                     << dir << "ing logical channel: "
                     << channel.GetCapability());
}
#endif


PBoolean H323EndPoint::OnStartLogicalChannel(H323Connection & /*connection*/,
                                         H323Channel & PTRACE_PARAM(channel))
{
#if PTRACING
  OnStartStopChannel("Start", channel);
#endif
  return true;
}


void H323EndPoint::OnClosedLogicalChannel(H323Connection & /*connection*/,
                                          const H323Channel & PTRACE_PARAM(channel))
{
#if PTRACING
  OnStartStopChannel("Stopp", channel);
#endif
}


void H323EndPoint::OnGatekeeperNATDetect(const PIPSocket::Address & /* publicAddr*/,
                                         H323TransportAddress & /*gkRouteAddress*/)
{
}


void H323EndPoint::OnHTTPServiceControl(unsigned /*opeartion*/,
                                        unsigned /*sessionId*/,
                                        const PString & /*url*/)
{
}


void H323EndPoint::OnCallCreditServiceControl(const PString & /*amount*/, PBoolean /*mode*/)
{
}


void H323EndPoint::OnServiceControlSession(unsigned type,
                                           unsigned sessionId,
                                           const H323ServiceControlSession & session,
                                           H323Connection * connection)
{
  session.OnChange(type, sessionId, *this, connection);
}

PBoolean H323EndPoint::OnCallIndependentSupplementaryService(const H323SignalPDU & /* setupPDU */)
{
  return false;
}

PBoolean H323EndPoint::OnNegotiateConferenceCapabilities(const H323SignalPDU & /* setupPDU */)
{
  return false;
}

H323ServiceControlSession * H323EndPoint::CreateServiceControlSession(const H225_ServiceControlDescriptor & contents)
{
  switch (contents.GetTag()) {
    case H225_ServiceControlDescriptor::e_url :
      return new H323HTTPServiceControl(contents);

    case H225_ServiceControlDescriptor::e_callCreditServiceControl :
      return new H323CallCreditServiceControl(contents);
  }

  return NULL;
}


void H323EndPoint::SetDefaultLocalPartyName(const PString & name)
{
  SetLocalUserName(name);
  OpalEndPoint::SetDefaultLocalPartyName(name);
}


void H323EndPoint::SetLocalUserName(const PString & name)
{
  PAssert(!name, "Must have non-empty string in AliasAddress!");
  if (name.IsEmpty())
    return;

  localAliasNames.RemoveAll();
  localAliasNames.AppendString(name);
}


bool H323EndPoint::AddAliasName(const PString & name)
{
  PAssert(!name, "Must have non-empty string in alias name!");

  if (localAliasNames.find(name) != localAliasNames.end()) {
    PTRACE(3, "H323\tAlias already present");
    return false;
  }

  localAliasNames.AppendString(name);
  return true;
}


bool H323EndPoint::RemoveAliasName(const PString & name)
{
  PAssert(!name, "Must have non-empty string in alias name!");

  PStringList::iterator pos = localAliasNames.find(name);
  if (pos == localAliasNames.end()) {
    PTRACE(3, "H323\tAlias already removed");
    return false;
  }

  if (localAliasNames.GetSize() == 1) {
    PTRACE(3, "H323\tLast alias cannopt be removed");
    return false;
  }

  localAliasNames.erase(pos);
  return true;
}


int H323EndPoint::ParseAliasPatternRange(const PString & pattern, PString & start, PString & end)
{
  if (!pattern.Split('-', start, end))
    return 0;

  if (start.GetLength() != end.GetLength())
    return 0;
  if (!OpalIsE164(start))
    return 0;
  if (!OpalIsE164(end))
    return 0;

  PINDEX commonDigits = 0;
  while (start[commonDigits] == end[commonDigits]) {
    if (++commonDigits >= start.GetLength())
      return 0;
  }

  if (start.Mid(commonDigits) >= end.Mid(commonDigits))
    return 0;

  return commonDigits;
}


bool H323EndPoint::AddAliasNamePattern(const PString & pattern)
{
  if (pattern.IsEmpty()) {
    PTRACE(2, "H323\tMust have non-empty string in alias pattern !");
    return false;
  }

  if (!OpalIsE164(pattern)) {
    PString start, end;
    if (ParseAliasPatternRange(pattern, start, end) == 0) {
      PTRACE(2, "H323\tIllegal range pattern  \"" << pattern << '"');
      return false;
    }
  }

  if (localAliasPatterns.find(pattern) != localAliasPatterns.end()) {
    PTRACE(3, "H323\tAlias pattern already present");
    return false;
  }

  localAliasPatterns.AppendString(pattern);
  return true;
}


bool H323EndPoint::RemoveAliasNamePattern(const PString & pattern)
{
  PAssert(!pattern, "Must have non-empty string in alias pattern !");

  PStringList::iterator pos = localAliasPatterns.find(pattern);
  if (pos == localAliasPatterns.end()) {
    PTRACE(3, "H323\tAlias pattern already removed");
    return false;
  }

  localAliasPatterns.erase(pos);
  return true;
}


PBoolean H323EndPoint::IsTerminal() const
{
  switch (terminalType) {
    case e_TerminalOnly :
    case e_TerminalAndMC :
      return true;

    default :
      return false;
  }
}


PBoolean H323EndPoint::IsGateway() const
{
  switch (terminalType) {
    case e_GatewayOnly :
    case e_GatewayAndMC :
    case e_GatewayAndMCWithDataMP :
    case e_GatewayAndMCWithAudioMP :
    case e_GatewayAndMCWithAVMP :
      return true;

    default :
      return false;
  }
}


PBoolean H323EndPoint::IsGatekeeper() const
{
  switch (terminalType) {
    case e_GatekeeperOnly :
    case e_GatekeeperWithDataMP :
    case e_GatekeeperWithAudioMP :
    case e_GatekeeperWithAVMP :
      return true;

    default :
      return false;
  }
}


PBoolean H323EndPoint::IsMCU() const
{
  switch (terminalType) {
    case e_MCUOnly :
    case e_MCUWithDataMP :
    case e_MCUWithAudioMP :
    case e_MCUWithAVMP :
      return true;

    default :
      return false;
  }
}


void H323EndPoint::TranslateTCPAddress(PIPSocket::Address & localAddr,
                                       const PIPSocket::Address & remoteAddr)
{
  manager.TranslateIPAddress(localAddr, remoteAddr);
}


#if OPAL_H460
H460_FeatureSet * H323EndPoint::CreateFeatureSet(H323Connection *)
{
  return new H460_FeatureSet(*this);
}


H460_FeatureSet * H323EndPoint::InternalCreateFeatureSet(H323Connection * connection)
{
  if (m_features == NULL) {
    m_features = CreateFeatureSet(NULL);
    if (m_features != NULL)
      m_features->LoadFeatureSet();
  }

  H460_FeatureSet * features = CreateFeatureSet(connection);
  if (features != NULL)
    features->LoadFeatureSet(connection);
  return features;
}


PBoolean H323EndPoint::OnSendFeatureSet(H460_MessageType pduType, H225_FeatureSet & featureSet)
{
  return m_features != NULL && m_features->OnSendPDU(pduType, featureSet);
}


void H323EndPoint::OnReceiveFeatureSet(H460_MessageType pduType, const H225_FeatureSet & featureSet)
{
  if (m_features != NULL)
    m_features->OnReceivePDU(pduType, featureSet);
}


bool H323EndPoint::OnLoadFeature(H460_Feature & /*feature*/)
{
  return true;
}

#endif // OPAL_H460


PString H323EndPoint::GetCompatibility(H323Connection::CompatibilityIssues issue) const
{
  CompatibilityEndpoints::const_iterator it = m_compatibility.find(issue);
  return it != m_compatibility.end() ? it->second.GetPattern() : PString::Empty();
}


bool H323EndPoint::SetCompatibility(H323Connection::CompatibilityIssues issue, const PString & regex)
{
  if (regex.IsEmpty()) {
    m_compatibility.erase(issue);
    return true;
  }

  PRegularExpression re;
  if (!re.Compile(regex, PRegularExpression::Extended|PRegularExpression::IgnoreCase))
    return false;

  m_compatibility[issue] = re;
  return true;
}


bool H323EndPoint::AddCompatibility(H323Connection::CompatibilityIssues issue, const PString & regex)
{
  if (!PAssert(!regex, PInvalidParameter))
    return false;

  PString pattern;

  CompatibilityEndpoints::iterator it = m_compatibility.find(issue);
  if (it != m_compatibility.end())
    pattern = it->second.GetPattern() + '|';

  pattern += regex;

  return SetCompatibility(issue, pattern);
}


bool H323EndPoint::HasCompatibilityIssue(H323Connection::CompatibilityIssues issue, const OpalProductInfo & productInfo) const
{
  bool found = false;
  CompatibilityEndpoints::const_iterator it = m_compatibility.find(issue);
  if (it != m_compatibility.end())
    found = productInfo.AsString().FindRegEx(it->second) != P_MAX_INDEX;

  PTRACE(found ? 3 : 4, "H.323\t" << (found ? "Found" : "Checked")
         << " compatibility issue " << issue << ", "
            "product=\"" << productInfo << "\", "
            "regex=\"" << (it != m_compatibility.end() ? it->second.GetPattern() : PString::Empty()) << '"');
  return found;
}


#else
  #pragma message("H.323 support DISABLED")
#endif // OPAL_H323

/////////////////////////////////////////////////////////////////////////////
