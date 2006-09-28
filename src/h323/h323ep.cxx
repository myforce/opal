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
 * Contributor(s): ______________________________________.
 *
 * $Log: h323ep.cxx,v $
 * Revision 1.2059  2006/09/28 07:42:17  csoutheren
 * Merge of useful SRTP implementation
 *
 * Revision 2.57  2006/09/19 01:52:17  csoutheren
 * Fixed parsing for explicit address and ports when using a GK
 *
 * Revision 2.56  2006/08/29 00:55:51  csoutheren
 * Fixed problem with parsing aliases when using GKs
 *
 * Revision 2.55  2006/08/21 05:29:25  csoutheren
 * Messy but relatively simple change to add support for secure (SSL/TLS) TCP transport
 * and secure H.323 signalling via the sh323 URL scheme
 *
 * Revision 2.54  2006/06/28 11:21:09  csoutheren
 * Removed warning on gcc
 *
 * Revision 2.53  2006/06/27 13:07:37  csoutheren
 * Patch 1374533 - add h323 Progress handling
 * Thanks to Frederich Heem
 *
 * Revision 2.52  2006/06/27 12:54:35  csoutheren
 * Patch 1374489 - h450.7 message center support
 * Thanks to Frederich Heem
 *
 * Revision 2.51  2006/05/30 11:33:02  hfriederich
 * Porting support for H.460 from OpenH323
 *
 * Revision 2.50  2006/05/30 04:58:06  csoutheren
 * Added suport for SIP INFO message (untested as yet)
 * Fixed some issues with SIP state machine on answering calls
 * Fixed some formatting issues
 *
 * Revision 2.49  2006/04/20 16:52:22  hfriederich
 * Adding support for H.224/H.281
 *
 * Revision 2.48  2006/03/12 06:36:57  rjongbloed
 * Fixed DevStudio warning
 *
 * Revision 2.47  2006/03/07 11:23:46  csoutheren
 * Add ability to disable GRQ on GK registration
 *
 * Revision 2.46  2006/02/22 10:40:10  csoutheren
 * Added patch #1374583 from Frederic Heem
 * Added additional H.323 virtual function
 *
 * Revision 2.45  2006/02/22 10:37:48  csoutheren
 * Fixed warning on Linux
 *
 * Revision 2.44  2006/02/22 10:29:09  csoutheren
 * Applied patch #1374470 from Frederic Heem
 * Add ability to disable H.245 negotiation
 *
 * Revision 2.43  2006/02/13 11:09:56  csoutheren
 * Multiple fixes for H235 authenticators
 *
 * Revision 2.42  2006/01/14 10:43:06  dsandras
 * Applied patch from Brian Lu <Brian.Lu _AT_____ sun.com> to allow compilation
 * with OpenSolaris compiler. Many thanks !!!
 *
 * Revision 2.41  2006/01/02 15:52:39  dsandras
 * Added what was required to merge changes from OpenH323 Altas_devel_2 in gkclient.cxx, gkserver.cxx and channels.cxx.
 *
 * Revision 2.40  2005/11/28 19:08:26  dsandras
 * Added E.164 support.
 *
 * History trimmed CRS 7 Sep 2006
 *
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "h323ep.h"
#endif

#include <h323/h323ep.h>

#include <ptclib/random.h>
#include <opal/call.h>
#include <h323/h323pdu.h>
#include <h323/gkclient.h>
#include <ptclib/url.h>
#include <ptclib/enum.h>
#include <ptclib/pils.h>


#define new PNEW

#if !PTRACING // Stuff to remove unised parameters warning
#define PTRACE_isEncoding
#define PTRACE_channel
#endif

BYTE H323EndPoint::defaultT35CountryCode    = 9; // Country code for Australia
BYTE H323EndPoint::defaultT35Extension      = 0;
WORD H323EndPoint::defaultManufacturerCode  = 61; // Allocated by Australian Communications Authority, Oct 2000;

/////////////////////////////////////////////////////////////////////////////

H323EndPoint::H323EndPoint(OpalManager & manager, const char * _prefix, WORD _defaultSignalPort)
  : OpalEndPoint(manager, _prefix, CanTerminateCall),
    m_bH245Disabled(FALSE),
    signallingChannelCallTimeout(0, 0, 1),  // Minutes
    controlChannelStartTimeout(0, 0, 2),    // Minutes
    endSessionTimeout(0, 10),               // Seconds
    masterSlaveDeterminationTimeout(0, 30), // Seconds
    capabilityExchangeTimeout(0, 30),       // Seconds
    logicalChannelTimeout(0, 30),           // Seconds
    requestModeTimeout(0, 30),              // Seconds
    roundTripDelayTimeout(0, 10),           // Seconds
    roundTripDelayRate(0, 0, 1),            // Minutes
    gatekeeperRequestTimeout(0, 5),         // Seconds
    rasRequestTimeout(0, 3),                // Seconds
    callTransferT1(0,10),                   // Seconds
    callTransferT2(0,10),                   // Seconds
    callTransferT3(0,10),                   // Seconds
    callTransferT4(0,10),                   // Seconds
    callIntrusionT1(0,30),                  // Seconds
    callIntrusionT2(0,30),                  // Seconds
    callIntrusionT3(0,30),                  // Seconds
    callIntrusionT4(0,30),                  // Seconds
    callIntrusionT5(0,10),                  // Seconds
    callIntrusionT6(0,10),                  // Seconds
    nextH450CallIdentity(0)
{
  // Set port in OpalEndPoint class
  defaultSignalPort = _defaultSignalPort;

  localAliasNames.AppendString(defaultLocalPartyName);

  autoStartReceiveFax = autoStartTransmitFax = FALSE;
  
  isH224Enabled = FALSE;

  m_bH245Disabled = FALSE;
  autoCallForward = TRUE;
  disableFastStart = FALSE;
  disableH245Tunneling = FALSE;
  disableH245inSetup = FALSE;
  canDisplayAmountString = FALSE;
  canEnforceDurationLimit = TRUE;
  callIntrusionProtectionLevel = 3; //H45011_CIProtectionLevel::e_fullProtection;

  terminalType = e_TerminalOnly;
  clearCallOnRoundTripFail = FALSE;

  t35CountryCode   = defaultT35CountryCode;   // Country code for Australia
  t35Extension     = defaultT35Extension;
  manufacturerCode = defaultManufacturerCode; // Allocated by Australian Communications Authority, Oct 2000

  masterSlaveDeterminationRetries = 10;
  gatekeeperRequestRetries = 2;
  rasRequestRetries = 2;
  sendGRQ = TRUE;

  gatekeeper = NULL;

  secondaryConnectionsActive.DisallowDeleteObjects();
  
#ifdef H323_H460
  features.AttachEndPoint(this);
  features.LoadFeatureSet(H460_Feature::FeatureBase);
#endif

  PTRACE(3, "H323\tCreated endpoint.");
}


H323EndPoint::~H323EndPoint()
{
  // And shut down the gatekeeper (if there was one)
  RemoveGatekeeper();

  // Shut down the listeners as soon as possible to avoid race conditions
  listeners.RemoveAll();

  PTRACE(3, "H323\tDeleted endpoint.");
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
      info.m_mc = TRUE;
  }
}


void H323EndPoint::SetVendorIdentifierInfo(H225_VendorIdentifier & info) const
{
  SetH221NonStandardInfo(info.m_vendor);

  info.IncludeOptionalField(H225_VendorIdentifier::e_productId);
  info.m_productId = PProcess::Current().GetManufacturer() & PProcess::Current().GetName();
  info.m_productId.SetSize(info.m_productId.GetSize()+2);

  info.IncludeOptionalField(H225_VendorIdentifier::e_versionId);
  info.m_versionId = PProcess::Current().GetVersion(TRUE) + " (OPAL v" + OpalGetVersion() + ')';
  info.m_versionId.SetSize(info.m_versionId.GetSize()+2);
}


void H323EndPoint::SetH221NonStandardInfo(H225_H221NonStandard & info) const
{
  info.m_t35CountryCode = t35CountryCode;
  info.m_t35Extension = t35Extension;
  info.m_manufacturerCode = manufacturerCode;
}


H323Capability * H323EndPoint::FindCapability(const H245_Capability & cap) const
{
  return GetCapabilities().FindCapability(cap);
}


H323Capability * H323EndPoint::FindCapability(const H245_DataType & dataType) const
{
  return GetCapabilities().FindCapability(dataType);
}


H323Capability * H323EndPoint::FindCapability(H323Capability::MainTypes mainType,
                                              unsigned subType) const
{
  return GetCapabilities().FindCapability(mainType, subType);
}


void H323EndPoint::AddCapability(H323Capability * capability)
{
  capabilities.Add(capability);
}


PINDEX H323EndPoint::SetCapability(PINDEX descriptorNum,
                                   PINDEX simultaneousNum,
                                   H323Capability * capability)
{
  return capabilities.SetCapability(descriptorNum, simultaneousNum, capability);
}


PINDEX H323EndPoint::AddAllCapabilities(PINDEX descriptorNum,
                                        PINDEX simultaneous,
                                        const PString & name)
{
  return capabilities.AddAllCapabilities(*this, descriptorNum, simultaneous, name);
}


void H323EndPoint::AddAllUserInputCapabilities(PINDEX descriptorNum,
                                               PINDEX simultaneous)
{
  H323_UserInputCapability::AddAllCapabilities(capabilities, descriptorNum, simultaneous);
}


void H323EndPoint::RemoveCapabilities(const PStringArray & codecNames)
{
  capabilities.Remove(codecNames);
}


void H323EndPoint::ReorderCapabilities(const PStringArray & preferenceOrder)
{
  capabilities.Reorder(preferenceOrder);
}


const H323Capabilities & H323EndPoint::GetCapabilities() const
{
  if (capabilities.GetSize() == 0) {
    capabilities.AddAllCapabilities(*this, P_MAX_INDEX, P_MAX_INDEX, "*");
    H323_UserInputCapability::AddAllCapabilities(capabilities, P_MAX_INDEX, P_MAX_INDEX);
  }

  return capabilities;
}


BOOL H323EndPoint::UseGatekeeper(const PString & address,
                                 const PString & identifier,
                                 const PString & localAddress)
{
  if (gatekeeper != NULL) {
    BOOL same = TRUE;

    if (!address)
      same = gatekeeper->GetTransport().GetRemoteAddress().IsEquivalent(address);

    if (!same && !identifier)
      same = gatekeeper->GetIdentifier() == identifier;

    if (!same && !localAddress)
      same = gatekeeper->GetTransport().GetLocalAddress().IsEquivalent(localAddress);

    if (same) {
      PTRACE(2, "H323\tUsing existing gatekeeper " << *gatekeeper);
      return TRUE;
    }
  }

  H323Transport * transport = NULL;
  if (!localAddress.IsEmpty()) {
    H323TransportAddress iface(localAddress);
    PIPSocket::Address ip;
    WORD port = H225_RAS::DefaultRasUdpPort;
    if (iface.GetIpAndPort(ip, port))
      transport = new H323TransportUDP(*this, ip, port);
  }

  if (address.IsEmpty()) {
    if (identifier.IsEmpty())
      return DiscoverGatekeeper(transport);
    else
      return LocateGatekeeper(identifier, transport);
  }
  else {
    if (identifier.IsEmpty())
      return SetGatekeeper(address, transport);
    else
      return SetGatekeeperZone(address, identifier, transport);
  }
}


BOOL H323EndPoint::SetGatekeeper(const PString & address, H323Transport * transport)
{
  H323Gatekeeper * gk = InternalCreateGatekeeper(transport);
  H323TransportAddress h323addr(address, H225_RAS::DefaultRasUdpPort, "udp");
  return InternalRegisterGatekeeper(gk, gk->DiscoverByAddress(h323addr));
}


BOOL H323EndPoint::SetGatekeeperZone(const PString & address,
                                     const PString & identifier,
                                     H323Transport * transport)
{
  H323Gatekeeper * gk = InternalCreateGatekeeper(transport);
  H323TransportAddress h323addr(address, H225_RAS::DefaultRasUdpPort, "udp");
  return InternalRegisterGatekeeper(gk, gk->DiscoverByNameAndAddress(identifier, h323addr));
}


BOOL H323EndPoint::LocateGatekeeper(const PString & identifier, H323Transport * transport)
{
  H323Gatekeeper * gk = InternalCreateGatekeeper(transport);
  return InternalRegisterGatekeeper(gk, gk->DiscoverByName(identifier));
}


BOOL H323EndPoint::DiscoverGatekeeper(H323Transport * transport)
{
  H323Gatekeeper * gk = InternalCreateGatekeeper(transport);
  return InternalRegisterGatekeeper(gk, gk->DiscoverAny());
}


H323Gatekeeper * H323EndPoint::InternalCreateGatekeeper(H323Transport * transport)
{
  RemoveGatekeeper(H225_UnregRequestReason::e_reregistrationRequired);

  if (transport == NULL)
    transport = new H323TransportUDP(*this);


  H323Gatekeeper * gk = CreateGatekeeper(transport);

  gk->SetPassword(gatekeeperPassword, gatekeeperUsername);

  return gk;
}


BOOL H323EndPoint::InternalRegisterGatekeeper(H323Gatekeeper * gk, BOOL discovered)
{
  if (discovered) {
    if (gk->RegistrationRequest()) {
      gatekeeper = gk;
      return TRUE;
    }

    // RRQ was rejected continue trying
    gatekeeper = gk;
  }
  else // Only stop listening if the GRQ was rejected
    delete gk;

  return FALSE;
}


H323Gatekeeper * H323EndPoint::CreateGatekeeper(H323Transport * transport)
{
  return new H323Gatekeeper(*this, transport);
}


BOOL H323EndPoint::IsRegisteredWithGatekeeper() const
{
  if (gatekeeper == NULL)
    return FALSE;

  return gatekeeper->IsRegistered();
}


BOOL H323EndPoint::RemoveGatekeeper(int reason)
{
  BOOL ok = TRUE;

  if (gatekeeper == NULL)
    return ok;

  ClearAllCalls();

  if (gatekeeper->IsRegistered()) // If we are registered send a URQ
    ok = gatekeeper->UnregistrationRequest(reason);

  delete gatekeeper;
  gatekeeper = NULL;

  return ok;
}


void H323EndPoint::SetGatekeeperPassword(const PString & password, const PString & username)
{
  gatekeeperUsername = username;
  gatekeeperPassword = password;

  if (gatekeeper != NULL) {
    gatekeeper->SetPassword(gatekeeperPassword, gatekeeperUsername);
    if (gatekeeper->IsRegistered()) // If we are registered send a URQ
      gatekeeper->UnregistrationRequest(H225_UnregRequestReason::e_reregistrationRequired);
    InternalRegisterGatekeeper(gatekeeper, TRUE);
  }
}

void H323EndPoint::OnGatekeeperConfirm()
{
}

void H323EndPoint::OnGatekeeperReject()
{
}

void H323EndPoint::OnRegistrationConfirm()
{
}

void H323EndPoint::OnRegistrationReject()
{
}

H235Authenticators H323EndPoint::CreateAuthenticators()
{
  H235Authenticators authenticators;

  PFactory<H235Authenticator>::KeyList_T keyList = PFactory<H235Authenticator>::GetKeyList();
  PFactory<H235Authenticator>::KeyList_T::const_iterator r;
  for (r = keyList.begin(); r != keyList.end(); ++r)
    authenticators.Append(PFactory<H235Authenticator>::CreateInstance(*r));

  PTRACE(1, "Authenticator list is size " << (int)authenticators.GetSize());

  return authenticators;
}


BOOL H323EndPoint::MakeConnection(OpalCall & call,
                                  const PString & remoteParty,
                                  void * userData)
{
  PTRACE(2, "H323\tMaking call to: " << remoteParty);
  return InternalMakeCall(call,
                          PString::Empty(),
                          PString::Empty(),
                          UINT_MAX,
                          remoteParty,
                          userData);
}


OpalMediaFormatList H323EndPoint::GetMediaFormats() const
{
  return OpalMediaFormat::GetAllRegisteredMediaFormats();
}


BOOL H323EndPoint::NewIncomingConnection(OpalTransport * transport)
{
  PTRACE(3, "H225\tAwaiting first PDU");
  transport->SetReadTimeout(15000); // Await 15 seconds after connect for first byte
  H323SignalPDU pdu;
  if (!pdu.Read(*transport)) {
    PTRACE(1, "H225\tFailed to get initial Q.931 PDU, connection not started.");
    return TRUE;
  }

  unsigned callReference = pdu.GetQ931().GetCallReference();
  PTRACE(3, "H225\tIncoming call, first PDU: callReference=" << callReference);

  // Get a new (or old) connection from the endpoint, calculate token
  PString token = transport->GetRemoteAddress();
  token.sprintf("/%u", callReference);

  PSafePtr<H323Connection> connection = FindConnectionWithLock(token);

  if (connection == NULL) {
    connection = CreateConnection(*manager.CreateCall(), token, NULL,
                                  *transport, PString::Empty(), PString::Empty(), &pdu);
    if (connection == NULL) {
      PTRACE(1, "H225\tEndpoint could not create connection, "
                "sending release complete PDU: callRef=" << callReference);

      H323SignalPDU releaseComplete;
      Q931 &q931PDU = releaseComplete.GetQ931();
      q931PDU.BuildReleaseComplete(callReference, TRUE);
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

      return TRUE;
    }

    connectionsActive.SetAt(token, connection);
  }

  PTRACE(3, "H323\tCreated new connection: " << token);
  connection->AttachSignalChannel(token, transport, TRUE);

  if (connection->HandleSignalPDU(pdu)) {
    // All subsequent PDU's should wait forever
    transport->SetReadTimeout(PMaxTimeInterval);
    connection->HandleSignallingChannel();
  }
  else {
    connection->ClearCall(H323Connection::EndedByTransportFail);
    PTRACE(1, "H225\tSignal channel stopped on first PDU.");
  }

  return FALSE;
}


H323Connection * H323EndPoint::CreateConnection(OpalCall & call,
                                                const PString & token,
                                                void * /*userData*/,
                                                OpalTransport & /*transport*/,
                                                const PString & alias,
                                                const H323TransportAddress & address,
                                                H323SignalPDU * /*setupPDU*/)
{
  H323Connection * conn = new H323Connection(call, *this, token, alias, address);
  if (conn != NULL)
    OnNewConnection(call, *conn);
  return conn;
}


BOOL H323EndPoint::SetupTransfer(const PString & oldToken,
                                 const PString & callIdentity,
                                 const PString & remoteParty,
                                 void * userData)
{
  // Make a new connection
  PSafePtr<OpalConnection> otherConnection =
    GetConnectionWithLock(oldToken, PSafeReference);
  if (otherConnection == NULL) {
    return FALSE;
  }

  OpalCall & call = otherConnection->GetCall();

  call.RemoveMediaStreams();

  PTRACE(2, "H323\tTransferring call to: " << remoteParty);
  BOOL ok = InternalMakeCall(call,
			     oldToken,
			     callIdentity,
			     UINT_MAX,
			     remoteParty,
			     userData);
  call.OnReleased(*otherConnection);
  otherConnection->Release(OpalConnection::EndedByCallForwarded);

  return ok;
}


BOOL H323EndPoint::InternalMakeCall(OpalCall & call,
                                    const PString & existingToken,
                                    const PString & callIdentity,
                                    unsigned capabilityLevel,
                                    const PString & remoteParty,
                                    void * userData)
{
  PString alias;
  H323TransportAddress address;
  if (!ParsePartyName(remoteParty, alias, address)) {
    PTRACE(2, "H323\tCould not parse \"" << remoteParty << '"');
    return FALSE;
  }

  // Restriction: the call must be made on the same transport as the one
  // that the gatekeeper is using.
  H323Transport * transport;
  if (gatekeeper != NULL)
    transport = gatekeeper->GetTransport().GetLocalAddress().CreateTransport(
                                          *this, OpalTransportAddress::Streamed);
  else
    transport = address.CreateTransport(*this, OpalTransportAddress::NoBinding);

  if (transport == NULL) {
    PTRACE(1, "H323\tInvalid transport in \"" << remoteParty << '"');
    return FALSE;
  }

  inUseFlag.Wait();

  PString newToken;
  if (existingToken.IsEmpty()) {
    do {
      newToken = psprintf("localhost/%u", Q931::GenerateCallReference());
    } while (connectionsActive.Contains(newToken));
  }

  H323Connection * connection = CreateConnection(call, newToken, userData, *transport, alias, address, NULL);
  if (connection == NULL) {
    PTRACE(1, "H225\tEndpoint could not create connection, aborting setup.");
    return FALSE;
  }

  connectionsActive.SetAt(newToken, connection);

  inUseFlag.Signal();

  connection->AttachSignalChannel(newToken, transport, FALSE);

  if (!callIdentity) {
    if (capabilityLevel == UINT_MAX)
      connection->HandleTransferCall(existingToken, callIdentity);
    else {
      connection->HandleIntrudeCall(existingToken, callIdentity);
      connection->IntrudeCall(capabilityLevel);
    }
  }

  PTRACE(3, "H323\tCreated new connection: " << newToken);

  // See if we are starting an outgoing connection as first in a call
  if (call.GetConnection(0) == (OpalConnection*)connection || !existingToken.IsEmpty())
    connection->SetUpConnection();

  return TRUE;
}


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


void H323EndPoint::HoldCall(const PString & token, BOOL localHold)
{
  PSafePtr<H323Connection> connection = FindConnectionWithLock(token);
  if (connection != NULL)
    connection->HoldCall(localHold);
}


BOOL H323EndPoint::IntrudeCall(const PString & remoteParty,
                               unsigned capabilityLevel,
                               void * userData)
{
  return InternalMakeCall(*manager.CreateCall(),
                          PString::Empty(),
                          PString::Empty(),
                          capabilityLevel,
                          remoteParty,
                          userData);
}

BOOL H323EndPoint::OnSendConnect(H323Connection & /*connection*/,
                                 H323SignalPDU & /*connectPDU*/
                                )
{
  return TRUE;
}

BOOL H323EndPoint::OnSendSignalSetup(H323Connection & /*connection*/,
                                     H323SignalPDU & /*setupPDU*/)
{
  return TRUE;
}

BOOL H323EndPoint::OnSendCallProceeding(H323Connection & /*connection*/,
                                        H323SignalPDU & /*callProceedingPDU*/)
{
  return TRUE;
}

void H323EndPoint::OnReceivedInitiateReturnError()
{
}

BOOL H323EndPoint::ParsePartyName(const PString & _remoteParty,
                                  PString & alias,
                                  H323TransportAddress & address)
{
  PString remoteParty = _remoteParty;

#if P_DNS
  // if there is no gatekeeper, and there is no '@', and there is no URL scheme, then attempt to use ENUM
  if ((remoteParty.Find('@') == P_MAX_INDEX) && (gatekeeper == NULL)) {

    // make sure the number has only digits
    PString e164 = remoteParty;
    if (e164.Left(4) *= "h323:")
      e164 = e164.Mid(4);
    PINDEX i;
    for (i = 0; i < e164.GetLength(); ++i)
      if (!isdigit(e164[i]) && (i != 0 || e164[0] != '+'))
      	break;
    if (i >= e164.GetLength()) {
      PString str;
      if (PDNS::ENUMLookup(e164, "E2U+h323", str)) {
        PTRACE(4, "H323\tENUM converted remote party " << _remoteParty << " to " << str);
        remoteParty = str;
      }
      else
        return FALSE;
    }
  }
#endif

  PURL url(remoteParty, GetPrefixName());

  // Special adjustment if 
  if (remoteParty.Find('@') == P_MAX_INDEX &&
      remoteParty.NumCompare(url.GetScheme()) != EqualTo) {
    if (gatekeeper == NULL)
      url.Parse(GetPrefixName() + ":@" + remoteParty);
    else
      url.Parse(GetPrefixName() + ":" + remoteParty);
  }

  alias = url.GetUserName();

  address = url.GetHostName();
  if (!address && url.GetPort() != 0)
    address.sprintf(":%u", url.GetPort());

  if (alias.IsEmpty() && address.IsEmpty()) {
    PTRACE(1, "H323\tAttempt to use invalid URL \"" << remoteParty << '"');
    return FALSE;
  }

  BOOL gatekeeperSpecified = FALSE;
  BOOL gatewaySpecified = FALSE;

  PCaselessString type = url.GetParamVars()("type");

  if (url.GetScheme() == "callto") {
    // Do not yet support ILS
    if (type == "directory") {
#if P_LDAP
      PString server = url.GetHostName();
      if (server.IsEmpty())
        server = GetDefaultILSServer();
      if (server.IsEmpty())
        return FALSE;

      PILSSession ils;
      if (!ils.Open(server, url.GetPort())) {
        PTRACE(1, "H323\tCould not open ILS server at \"" << server
               << "\" - " << ils.GetErrorText());
        return FALSE;
      }

      PILSSession::RTPerson person;
      if (!ils.SearchPerson(alias, person)) {
        PTRACE(1, "H323\tCould not find "
               << server << '/' << alias << ": " << ils.GetErrorText());
        return FALSE;
      }

      if (!person.sipAddress.IsValid()) {
        PTRACE(1, "H323\tILS user " << server << '/' << alias
               << " does not have a valid IP address");
        return FALSE;
      }

      // Get the IP address
      address = H323TransportAddress(person.sipAddress);

      // Get the port if provided
      for (PINDEX i = 0; i < person.sport.GetSize(); i++) {
        if (person.sport[i] != 1503) { // Dont use the T.120 port
          address = H323TransportAddress(person.sipAddress, person.sport[i]);
          break;
        }
      }

      alias = PString::Empty(); // No alias for ILS lookup, only host
      return TRUE;
#else
      return FALSE;
#endif
    }

    if (url.GetParamVars().Contains("gateway"))
      gatewaySpecified = TRUE;
  }
  else if (url.GetScheme() == "h323") {
    if (type == "gw")
      gatewaySpecified = TRUE;
    else if (type == "gk")
      gatekeeperSpecified = TRUE;
    else if (!type) {
      PTRACE(1, "H323\tUnsupported host type \"" << type << "\" in h323 URL");
      return FALSE;
    }
  }

  // User explicitly asked fo a look up to a gk
  if (gatekeeperSpecified) {
    if (alias.IsEmpty()) {
      PTRACE(1, "H323\tAttempt to use explict gatekeeper without alias!");
      return FALSE;
    }

    if (address.IsEmpty()) {
      PTRACE(1, "H323\tAttempt to use explict gatekeeper without address!");
      return FALSE;
    }

    H323TransportAddress gkAddr = address;
    PTRACE(3, "H323\tLooking for \"" << alias << "\" on gatekeeper at " << gkAddr);

    H323Gatekeeper * gk = CreateGatekeeper(new H323TransportUDP(*this));

    BOOL ok = gk->DiscoverByAddress(gkAddr);
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
  if (gatekeeper == NULL || gatewaySpecified) {
    // If URL did not have a host, but user said to use gw, or we do not have
    // a gk to do a lookup so we MUST have a host, use alias must be host
    if (address.IsEmpty()) {
      address = H323TransportAddress(alias, GetDefaultSignalPort(), GetDefaultTransport());
      alias = PString::Empty();
    }
    return TRUE;
  }

  // We have a gatekeeper and user provided a host, all done
  if (!address)
    return TRUE;

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

  return TRUE;
}


PSafePtr<H323Connection> H323EndPoint::FindConnectionWithLock(const PString & token, PSafetyMode mode)
{
  PSafePtr<H323Connection> connnection = PSafePtrCast<OpalConnection, H323Connection>(GetConnectionWithLock(token, mode));
  if (connnection != NULL)
    return connnection;

  for (connnection = PSafePtrCast<OpalConnection, H323Connection>(connectionsActive.GetAt(0)); connnection != NULL; ++connnection) {
    if (connnection->GetCallIdentifier().AsString() == token)
      return connnection;
    if (connnection->GetConferenceIdentifier().AsString() == token)
      return connnection;
  }

  return NULL;
}


BOOL H323EndPoint::OnIncomingCall(H323Connection & connection,
                                  const H323SignalPDU & /*setupPDU*/,
                                  H323SignalPDU & /*alertingPDU*/)
{
  return connection.OnIncomingConnection();
}


BOOL H323EndPoint::OnCallTransferInitiate(H323Connection & /*connection*/,
                                          const PString & /*remoteParty*/)
{
  return TRUE;
}


BOOL H323EndPoint::OnCallTransferIdentify(H323Connection & /*connection*/)
{
  return TRUE;
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

BOOL H323EndPoint::OnAlerting(H323Connection & connection,
                              const H323SignalPDU & /*alertingPDU*/,
                              const PString & /*username*/)
{
  PTRACE(1, "H225\tReceived alerting PDU.");
  ((OpalConnection&)connection).OnAlerting();
  return TRUE;
}

BOOL H323EndPoint::OnSendAlerting(H323Connection & PTRACE_PARAM(connection),
                                  H323SignalPDU & /*alerting*/,
                                  const PString & /*calleeName*/,   /// Name of endpoint being alerted.
                                  BOOL /*withMedia*/                /// Open media with alerting
                                  )
{
  PTRACE(3, "H225\tOnSendAlerting conn = " << connection);
  return TRUE;
}

BOOL H323EndPoint::OnSentAlerting(H323Connection & PTRACE_PARAM(connection))
{
  PTRACE(3, "H225\tOnSentAlerting conn = " << connection);
  return TRUE;
}

BOOL H323EndPoint::OnConnectionForwarded(H323Connection & /*connection*/,
                                         const PString & /*forwardParty*/,
                                         const H323SignalPDU & /*pdu*/)
{
  return FALSE;
}


BOOL H323EndPoint::ForwardConnection(H323Connection & connection,
                                     const PString & forwardParty,
                                     const H323SignalPDU & /*pdu*/)
{
  if (!InternalMakeCall(connection.GetCall(),
                        connection.GetCallToken(),
                        PString::Empty(),
                        UINT_MAX,
                        forwardParty,
                        NULL))
    return FALSE;

  connection.SetCallEndReason(H323Connection::EndedByCallForwarded);

  return TRUE;
}


void H323EndPoint::OnConnectionEstablished(H323Connection & /*connection*/,
                                           const PString & /*token*/)
{
}


BOOL H323EndPoint::IsConnectionEstablished(const PString & token)
{
  PSafePtr<H323Connection> connection = FindConnectionWithLock(token);
  return connection != NULL && connection->IsEstablished();
}


BOOL H323EndPoint::OnOutgoingCall(H323Connection & /*connection*/,
                                  const H323SignalPDU & /*connectPDU*/)
{
  PTRACE(1, "H225\tReceived connect PDU.");
  return TRUE;
}


void H323EndPoint::OnConnectionCleared(H323Connection & /*connection*/,
                                       const PString & /*token*/)
{
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

  PTRACE(2, "H323\t" << startstop << "ed "
                     << dir << "ing logical channel: "
                     << channel.GetCapability());
}
#endif


BOOL H323EndPoint::OnStartLogicalChannel(H323Connection & /*connection*/,
                                         H323Channel & PTRACE_channel)
{
#if PTRACING
  OnStartStopChannel("Start", PTRACE_channel);
#endif
  return TRUE;
}


void H323EndPoint::OnClosedLogicalChannel(H323Connection & /*connection*/,
                                          const H323Channel & PTRACE_channel)
{
#if PTRACING
  OnStartStopChannel("Stopp", PTRACE_channel);
#endif
}


void H323EndPoint::OnRTPStatistics(const H323Connection & /*connection*/,
                                   const RTP_Session & /*session*/) const
{
}


void H323EndPoint::OnGatekeeperNATDetect(PIPSocket::Address /* publicAddr*/,
					 PString & /*gkIdentifier*/,
					 H323TransportAddress & /*gkRouteAddress*/)
{
}


void H323EndPoint::OnHTTPServiceControl(unsigned /*opeartion*/,
                                        unsigned /*sessionId*/,
                                        const PString & /*url*/)
{
}


void H323EndPoint::OnCallCreditServiceControl(const PString & /*amount*/, BOOL /*mode*/)
{
}


void H323EndPoint::OnServiceControlSession(unsigned type,
                                           unsigned sessionId,
                                           const H323ServiceControlSession & session,
                                           H323Connection * connection)
{
  session.OnChange(type, sessionId, *this, connection);
}

BOOL H323EndPoint::OnConferenceInvite(const H323SignalPDU & /*setupPDU */)
{
  return FALSE;
}

BOOL H323EndPoint::OnCallIndependentSupplementaryService(const H323SignalPDU & /* setupPDU */)
{
  return FALSE;
}

BOOL H323EndPoint::OnNegotiateConferenceCapabilities(const H323SignalPDU & /* setupPDU */)
{
  return FALSE;
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


void H323EndPoint::SetLocalUserName(const PString & name)
{
  PAssert(!name, "Must have non-empty string in AliasAddress!");
  if (name.IsEmpty())
    return;

  localAliasNames.RemoveAll();
  localAliasNames.AppendString(name);
}


BOOL H323EndPoint::AddAliasName(const PString & name)
{
  PAssert(!name, "Must have non-empty string in AliasAddress!");

  if (localAliasNames.GetValuesIndex(name) != P_MAX_INDEX)
    return FALSE;

  localAliasNames.AppendString(name);
  return TRUE;
}


BOOL H323EndPoint::RemoveAliasName(const PString & name)
{
  PINDEX pos = localAliasNames.GetValuesIndex(name);
  if (pos == P_MAX_INDEX)
    return FALSE;

  PAssert(localAliasNames.GetSize() > 1, "Must have at least one AliasAddress!");
  if (localAliasNames.GetSize() < 2)
    return FALSE;

  localAliasNames.RemoveAt(pos);
  return TRUE;
}


BOOL H323EndPoint::IsTerminal() const
{
  switch (terminalType) {
    case e_TerminalOnly :
    case e_TerminalAndMC :
      return TRUE;

    default :
      return FALSE;
  }
}


BOOL H323EndPoint::IsGateway() const
{
  switch (terminalType) {
    case e_GatewayOnly :
    case e_GatewayAndMC :
    case e_GatewayAndMCWithDataMP :
    case e_GatewayAndMCWithAudioMP :
    case e_GatewayAndMCWithAVMP :
      return TRUE;

    default :
      return FALSE;
  }
}


BOOL H323EndPoint::IsGatekeeper() const
{
  switch (terminalType) {
    case e_GatekeeperOnly :
    case e_GatekeeperWithDataMP :
    case e_GatekeeperWithAudioMP :
    case e_GatekeeperWithAVMP :
      return TRUE;

    default :
      return FALSE;
  }
}


BOOL H323EndPoint::IsMCU() const
{
  switch (terminalType) {
    case e_MCUOnly :
    case e_MCUWithDataMP :
    case e_MCUWithAudioMP :
    case e_MCUWithAVMP :
      return TRUE;

    default :
      return FALSE;
  }
}


void H323EndPoint::TranslateTCPAddress(PIPSocket::Address & localAddr,
                                       const PIPSocket::Address & remoteAddr)
{
  manager.TranslateIPAddress(localAddr, remoteAddr);
}

BOOL H323EndPoint::OnSendFeatureSet(unsigned, H225_FeatureSet & features)
{
	return FALSE;
}

void H323EndPoint::OnReceiveFeatureSet(unsigned, const H225_FeatureSet & features)
{
}

/////////////////////////////////////////////////////////////////////////////
