/*
 * gkerv.cxx
 *
 * PWLib application source file for OPAL Server
 *
 * Main program entry point.
 *
 * Copyright (c) 2007 Equivalence Pty. Ltd.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#include "precompile.h"
#include "main.h"

#if OPAL_H323

#include <ptclib/random.h>

#ifdef H323_TRANSNEXUS_OSP
#include <opalosp.h>
#endif

//#define TEST_TOKEN

static const char H323AliasesKey[] = "H.323 Aliases";
static const char H323AliasPatternsKey[] = "H.323 Alias Patterns";
static const char H323TerminalTypeKey[] = "H.323 Terminal Type";
static const char DisableFastStartKey[] = "Disable H.323 Fast Start";
static const char DisableH245TunnelingKey[] = "Disable H.245 Tunneling";
static const char DisableH245inSetupKey[] = "Disable H.245 in Setup";
static const char ForceSymmetricTCSKey[] = "Force Symmetric TCS";
static const char H323BandwidthKey[] = "H.323 Bandwidth";

static const char GatekeeperEnableKey[] = "Remote Gatekeeper Enable";
static const char GatekeeperAddressKey[] = "Remote Gatekeeper Address";
static const char RemoteGatekeeperIdentifierKey[] = "Remote Gatekeeper Identifier";
static const char GatekeeperInterfaceKey[] = "Remote Gatekeeper Interface";
static const char GatekeeperPasswordKey[] = "Remote Gatekeeper Password";
static const char GatekeeperTokenOIDKey[] = "Remote Gatekeeper Token OID";
static const char GatekeeperAliasLimitKey[] = "Remote Gatekeeper Alias Limit";
static const char GatekeeperSimulatePatternKey[] = "Remote Gatekeeper Simulate Pattern";

static const char ServerGatekeeperEnableKey[] = "Gatekeeper Server Enable";
static const char ServerGatekeeperIdentifierKey[] = "Gatekeeper Server Identifier";
static const char AvailableBandwidthKey[] = "Gatekeeper Total Bandwidth";
static const char DefaultBandwidthKey[] = "Gatekeeper Default Bandwidth Allocation";
static const char MaximumBandwidthKey[] = "Gatekeeper Maximum Bandwidth Allocation";
static const char DefaultTimeToLiveKey[] = "Gatekeeper Default Time To Live";
static const char CallHeartbeatTimeKey[]    = "Gatekeeper Call Heartbeat Time";
static const char OverwriteOnSameSignalAddressKey[] = "Gatekeeper Overwrite EP On Same Signal Address";
static const char CanHaveDuplicateAliasKey[] = "Gatekeeper Can Have Duplicate Alias";
static const char CanOnlyCallRegisteredEPKey[] = "Can Only Call Registered EP";
static const char CanOnlyAnswerRegisteredEPKey[] = "Can Only Answer Gatekeeper Registered EP";
static const char AnswerCallPreGrantedARQKey[] = "Answer Call Pregranted ARQ";
static const char MakeCallPreGrantedARQKey[] = "Make Call Pregranted ARQ";
static const char IsGatekeeperRoutedKey[] = "Gatekeeper Routed";
static const char AliasCanBeHostNameKey[] = "H.323 Alias Can Be Host Name";
static const char MinAliasToAllocateKey[] = "Minimum Alias to Allocate";
static const char MaxAliasToAllocateKey[] = "Maximum Alias to Allocate";
static const char RequireH235Key[] = "Gatekeeper Requires H.235 Authentication";
static const char UsernameKey[] = "Username";
static const char PasswordKey[] = "Password";
static const char RouteAliasKey[] = "Alias";
static const char RouteHostKey[] = "Host";
static const char ClearingHouseKey[] = "H.501 Clearing House";
static const char H501InterfaceKey[] = "H.501 Interface";
static const char GkServerListenersKey[] = "Gatekeeper Server Interfaces";

#ifdef H323_TRANSNEXUS_OSP
static const char OSPRoutingURLKey[] = "OSP Routing URL";
static const char OSPPrivateKeyFileKey[] = "OSP Private Key";
static const char OSPPublicKeyFileKey[] = "OSP Public Key";
static const char OSPServerKeyFileKey[] = "OSP Server Key";
#endif

static const char AuthenticationCredentialsName[] = "Gatekeeper Authenticated Users";
static const char AuthenticationCredentialsKey[] = "Authentication\\Credentials %u\\";

static const char AliasRouteMapsName[] = "Gatekeeper Alias Route Maps";
static const char AliasRouteMapsKey[] = "Route Maps\\Mapping %u\\";

#define PTraceModule() "OpalServer"
#define new PNEW


///////////////////////////////////////////////////////////////

MyH323EndPoint::MyH323EndPoint(MyManager & mgr)
  : H323ConsoleEndPoint(mgr)
  , m_manager(mgr)
  , P_DISABLE_MSVC_WARNINGS(4355, m_gkServer(*this))
{
  terminalType = e_MCUWithAVMP;
}


bool MyH323EndPoint::Configure(PConfig & cfg, PConfigPage * rsrc)
{
  if (!m_manager.ConfigureCommon(this, "H.323", cfg, rsrc))
    return false;

  // Add H.323 parameters
  {
    PStringArray aliases = rsrc->AddStringArrayField(H323AliasesKey, false, 0, GetAliasNames(), "H.323 Alias names for local user", 1, 30);
    if (!aliases.IsEmpty()) {
      SetLocalUserName(aliases[0]);
      for (PINDEX i = 1; i < aliases.GetSize(); i++)
        AddAliasName(aliases[i]);
    }
  }

  SetAliasNamePatterns(rsrc->AddStringArrayField(H323AliasPatternsKey, false, 0, GetAliasNamePatterns(), "H.323 Alias patterns for local user", 1, 30));

  SetTerminalType((TerminalTypes)rsrc->AddIntegerField(H323TerminalTypeKey, 0, 255, GetTerminalType(), "",
                  "H.323 Terminal Type code for master/slave resolution:<BR>"
                  "50=terminal<BR>"
                  "60=gateway<BR>"
                  "120=gatekeeper<BR>"
                  "160=MCU<BR>"
                  "190=MCU with A/V"));
  DisableFastStart(rsrc->AddBooleanField(DisableFastStartKey,  IsFastStartDisabled(), "Disable H.323 Fast Connect feature"));
  DisableH245Tunneling(rsrc->AddBooleanField(DisableH245TunnelingKey,  IsH245TunnelingDisabled(), "Disable H.245 tunneled in H.225.0 signalling channel"));
  DisableH245inSetup(rsrc->AddBooleanField(DisableH245inSetupKey,  IsH245inSetupDisabled(), "Disable sending initial tunneled H.245 PDU in SETUP PDU"));
  ForceSymmetricTCS(rsrc->AddBooleanField(ForceSymmetricTCSKey, IsForcedSymmetricTCS(), "Force indication of symmetric codecs in TCS"));

  SetInitialBandwidth(OpalBandwidth::RxTx, rsrc->AddIntegerField(H323BandwidthKey, 1, OpalBandwidth::Max()/1000,
                                                                  GetInitialBandwidth(OpalBandwidth::RxTx)/1000,
                              "kb/s", "Bandwidth to request to gatekeeper on originating/answering calls")*1000);

  for (H323Connection::CompatibilityIssues issue = H323Connection::BeginCompatibilityIssues; issue < H323Connection::EndCompatibilityIssues; ++issue)
    SetCompatibility(issue, rsrc->AddStringField(H323Connection::CompatibilityIssuesToString(issue), 0, GetCompatibility(issue),
                                                 "Compatibility issue work around, product name/version regular expression", 1, 50));

  bool gkEnable = rsrc->AddBooleanField(GatekeeperEnableKey, false, "Enable registration with gatekeeper as client");

  PString gkAddress = rsrc->AddStringField(GatekeeperAddressKey, 0, PString::Empty(),
      "IP/hostname of gatekeeper to register with, if blank a broadcast is used", 1, 30);

  PString gkIdentifier = rsrc->AddStringField(RemoteGatekeeperIdentifierKey, 0, PString::Empty(),
                "Gatekeeper identifier to register with, if blank any gatekeeper is used", 1, 30);

  PString gkInterface = rsrc->AddStringField(GatekeeperInterfaceKey, 0, PString::Empty(),
            "Local network interface to use to register with gatekeeper, if blank all are used", 1, 30);

  PString gkPassword = PHTTPPasswordField::Decrypt(cfg.GetString(GatekeeperPasswordKey));
  if (!gkPassword)
    SetGatekeeperPassword(gkPassword);
  rsrc->Add(new PHTTPPasswordField(GatekeeperPasswordKey, 30, gkPassword,
            "Password for gatekeeper authentication, user is the first alias"));

  SetGkAccessTokenOID(rsrc->AddStringField(GatekeeperTokenOIDKey, 0, GetGkAccessTokenOID(),
                                   "Gatekeeper access token OID for H.235 support", 1, 30));

  SetGatekeeperAliasLimit(rsrc->AddIntegerField(GatekeeperAliasLimitKey,
            1, H323EndPoint::MaxGatekeeperAliasLimit, GetGatekeeperAliasLimit(), NULL,
            "Compatibility issue with some gatekeepers not being able to register large numbers of aliases in single RRQ."));

  SetGatekeeperSimulatePattern(rsrc->AddBooleanField(GatekeeperSimulatePatternKey, GetGatekeeperSimulatePattern(),
            "Compatibility issue with some gatekeepers not supporting alias patterns, generate separate aliases for ranges."));

  if (gkEnable) {
    if (UseGatekeeper(gkAddress, gkIdentifier, gkInterface)) {
      PSYSTEMLOG(Info, "Register with gatekeeper " << *GetGatekeeper());
    }
    else {
      PSYSTEMLOG(Error, "Could not register with gatekeeper!");
    }
  }
  else {
    PSYSTEMLOG(Info, "Not using remote gatekeeper.");
    RemoveGatekeeper();
  }

  return m_gkServer.Configure(cfg, rsrc);
}


void MyH323EndPoint::AutoRegister(const PString & alias, bool registering)
{
  if (alias.IsEmpty())
    return;

  if (alias.Find('-') == P_MAX_INDEX && alias.Find("..") != P_MAX_INDEX) {
    if (registering)
      AddAliasNamePattern(alias);
    else
      RemoveAliasNamePattern(alias);
  }
  else {
    if (registering)
      AddAliasName(alias);
    else
      RemoveAliasName(alias);
  }
}


PString MyGatekeeperServer::OnLoadEndPointStatus(const PString & htmlBlock)
{
  PINDEX i;
  PString substitution;

  for (PSafePtr<H323RegisteredEndPoint> ep = GetFirstEndPoint(PSafeReadOnly); ep != NULL; ep++) {
    // make a copy of the repeating html chunk
    PString insert = htmlBlock;

    PServiceHTML::SpliceMacro(insert, "status EndPointIdentifier", ep->GetIdentifier());

    PStringStream addresses;
    for (i = 0; i < ep->GetSignalAddressCount(); i++) {
      if (i > 0)
        addresses << "<br>";
      addresses << ep->GetSignalAddress(i);
    }
    PServiceHTML::SpliceMacro(insert, "status CallSignalAddresses", addresses);

    PStringStream aliases;
    for (i = 0; i < ep->GetAliasCount(); i++) {
      if (i > 0)
        aliases << "<br>";
      aliases << ep->GetAlias(i);
    }
    PServiceHTML::SpliceMacro(insert, "status EndPointAliases", aliases);

    PString str = "<i>Name:</i> " + ep->GetApplicationInfo();
    str.Replace("\t", "<BR><i>Version:</i> ");
    str.Replace("\t", " <i>Vendor:</i> ");
    PServiceHTML::SpliceMacro(insert, "status Application", str);

    PServiceHTML::SpliceMacro(insert, "status ActiveCalls", ep->GetCallCount());

    // Then put it into the page, moving insertion point along after it.
    substitution += insert;
  }

  return substitution;
}


PCREATE_SERVICE_MACRO_BLOCK(EndPointStatus,resource,P_EMPTY,block)
{
  GkStatusPage * status = dynamic_cast<GkStatusPage *>(resource.m_resource);
  return PAssertNULL(status)->m_gkServer.OnLoadEndPointStatus(block);
}


///////////////////////////////////////////////////////////////

MyGatekeeperServer::MyGatekeeperServer(H323EndPoint & ep)
  : H323GatekeeperServer(ep),
    endpoint(ep)
{
#ifdef H323_TRANSNEXUS_OSP
  ospProvider = NULL;
#endif
}


bool MyGatekeeperServer::Configure(PConfig & cfg, PConfigPage * rsrc)
{
  PINDEX i;

  PWaitAndSignal mutex(reconfigurationMutex);

  bool srvEnable = rsrc->AddBooleanField(ServerGatekeeperEnableKey, true, "Enable gatekeeper server");
  if (!srvEnable) {
    RemoveListener(NULL);
    return true;
  }

#ifdef H323_H501

  // set clearing house address
  PString clearingHouse = rsrc->AddStringField(ClearingHouseKey, 25));
  PString h501Interface = rsrc->AddStringField(H501InterfaceKey, 25));

  if (clearingHouse.IsEmpty())
    SetPeerElement(NULL);
  else {
    if (!h501Interface)
      CreatePeerElement(h501Interface);
    if (!OpenPeerElement(clearingHouse)) {
      PSYSTEMLOG(Error, "Main\tCould not open clearing house at: " << clearingHouse);
    }
  }
#endif

#ifdef H323_TRANSNEXUS_OSP
  PString oldOSPServer = ospRoutingURL;
  ospRoutingURL = rsrc->AddStringField(OSPRoutingURLKey, 25, ospRoutingURL);
  bool ospChanged = oldOSPServer != ospRoutingURL;

  ospPrivateKeyFileName = rsrc->AddStringField(OSPPrivateKeyFileKey, 25, ospPrivateKeyFileName);
  ospPublicKeyFileName =  rsrc->AddStringField(OSPPublicKeyFileKey, 25, ospPublicKeyFileName);
  ospServerKeyFileName = rsrc->AddStringField(OSPServerKeyFileKey, 25, ospServerKeyFileName);

  if (!ospRoutingURL.IsEmpty()) {
    if (ospProvider != NULL && ospProvider->IsOpen() && ospChanged) {
      delete ospProvider;
      ospProvider = NULL;
    }
    ospProvider = new OpalOSP::Provider();
    int status;
    if (ospPrivateKeyFileName.IsEmpty() && ospPublicKeyFileName.IsEmpty() && ospServerKeyFileName.IsEmpty())
      status = ospProvider->Open(ospRoutingURL);
    else
      status = ospProvider->Open(ospRoutingURL, ospPrivateKeyFileName, ospPublicKeyFileName, ospServerKeyFileName);
    if (status != 0) {
      delete ospProvider;
      ospProvider = NULL;
    }
  } 
  
  else if (ospProvider != NULL) {
    ospProvider->Close();
    delete ospProvider;
    ospProvider = NULL;
  }

#endif

  SetGatekeeperIdentifier(rsrc->AddStringField(ServerGatekeeperIdentifierKey, 0,
                                               MyProcess::Current().GetName() + " on " + PIPSocket::GetHostName(),
                                               "Identifier (name) for gatekeeper server", 1, 30));

  // Interfaces to listen on
  AddListeners(rsrc->AddStringArrayField(GkServerListenersKey, false, 0, PStringArray(),
                  "Local network interfaces to listen for RAS, blank means all", 1, 25));

  SetAvailableBandwidth(rsrc->AddIntegerField(AvailableBandwidthKey, 1, INT_MAX, GetAvailableBandwidth()/10,
                       "kb/s", "Total bandwidth to allocate across all calls through gatekeeper server")*10);

  defaultBandwidth = rsrc->AddIntegerField(DefaultBandwidthKey, 1, INT_MAX, defaultBandwidth/10,
               "kb/s", "Default bandwidth to allocate for a call through gatekeeper server")*10;

  maximumBandwidth = rsrc->AddIntegerField(MaximumBandwidthKey, 1, INT_MAX, maximumBandwidth/10,
                  "kb/s", "Maximum bandwidth to allow for a call through gatekeeper server")*10;

  defaultTimeToLive = rsrc->AddIntegerField(DefaultTimeToLiveKey, 10, 86400, defaultTimeToLive,
              "seconds", "Default time before assume endpoint is offline to gatekeeper server");

  defaultInfoResponseRate = rsrc->AddIntegerField(CallHeartbeatTimeKey, 0, 86400, defaultInfoResponseRate,
                    "seconds", "Time between validation requests on call controlled by gatekeeper server");

  overwriteOnSameSignalAddress = rsrc->AddBooleanField(OverwriteOnSameSignalAddressKey, overwriteOnSameSignalAddress,
               "Allow new registration to gatekeeper on a specific signal address to override previous registration");

  canHaveDuplicateAlias = rsrc->AddBooleanField(CanHaveDuplicateAliasKey, canHaveDuplicateAlias,
      "Different endpoint can register with gatekeeper the same alias name as another endpoint");

  canOnlyCallRegisteredEP = rsrc->AddBooleanField(CanOnlyCallRegisteredEPKey, canOnlyCallRegisteredEP,
                           "Gatekeeper will only allow EP to call another endpoint registered localy");

  canOnlyAnswerRegisteredEP = rsrc->AddBooleanField(CanOnlyAnswerRegisteredEPKey, canOnlyAnswerRegisteredEP,
                         "Gatekeeper will only allow endpoint to answer another endpoint registered localy");

  answerCallPreGrantedARQ = rsrc->AddBooleanField(AnswerCallPreGrantedARQKey, answerCallPreGrantedARQ,
                                               "Gatekeeper pre-grants all incoming calls to endpoint");

  makeCallPreGrantedARQ = rsrc->AddBooleanField(MakeCallPreGrantedARQKey, makeCallPreGrantedARQ,
                                       "Gatekeeper pre-grants all outgoing calls from endpoint");

  aliasCanBeHostName = rsrc->AddBooleanField(AliasCanBeHostNameKey, aliasCanBeHostName,
              "Gatekeeper allows endpoint to simply registerit's host name/IP address");

  m_minAliasToAllocate = rsrc->AddIntegerField(MinAliasToAllocateKey, 0, INT_MAX, m_minAliasToAllocate, "",
       "Minimum value for aliases gatekeeper will allocate when endpoint does not provide one, 0 disables");

  m_maxAliasToAllocate = rsrc->AddIntegerField(MaxAliasToAllocateKey, 0, INT_MAX, m_maxAliasToAllocate, "",
       "Minimum value for aliases gatekeeper will allocate when endpoint does not provide one, 0 disables");

  isGatekeeperRouted = rsrc->AddBooleanField(IsGatekeeperRoutedKey, isGatekeeperRouted,
             "All endpoionts will route sigaling for all calls through the gatekeeper");

  PHTTPCompositeField * routeFields = new PHTTPCompositeField(AliasRouteMapsKey, AliasRouteMapsName,
                                                              "Fixed mapping of alias names to hostnames for calls routed through gatekeeper");
  routeFields->Append(new PHTTPStringField(RouteAliasKey, 0, NULL, NULL, 1, 20));
  routeFields->Append(new PHTTPStringField(RouteHostKey, 0, NULL, NULL, 1, 30));
  PHTTPFieldArray * routeArray = new PHTTPFieldArray(routeFields, true);
  rsrc->Add(routeArray);

  routes.RemoveAll();
  if (!routeArray->LoadFromConfig(cfg)) {
    for (i = 0; i < routeArray->GetSize(); ++i) {
      PHTTPCompositeField & item = dynamic_cast<PHTTPCompositeField &>((*routeArray)[i]);
      RouteMap map(item[0].GetValue(), item[1].GetValue());
      if (map.IsValid())
        routes.Append(new RouteMap(map));
    }
  }

  requireH235 = rsrc->AddBooleanField(RequireH235Key, requireH235,
      "Gatekeeper requires H.235 cryptographic authentication for registrations");

  PHTTPCompositeField * security = new PHTTPCompositeField(AuthenticationCredentialsKey, AuthenticationCredentialsName,
                                                           "Table of username/password for authenticated endpoints on gatekeeper, requires H.235");
  security->Append(new PHTTPStringField(UsernameKey, 25));
  security->Append(new PHTTPPasswordField(PasswordKey, 25));
  PHTTPFieldArray * securityArray = new PHTTPFieldArray(security, false);
  rsrc->Add(securityArray);

  passwords.RemoveAll();
  if (!securityArray->LoadFromConfig(cfg)) {
    for (i = 0; i < securityArray->GetSize(); ++i) {
      PHTTPCompositeField & item = dynamic_cast<PHTTPCompositeField &>((*securityArray)[i]);
      passwords.SetAt(item[0].GetValue(), PHTTPPasswordField::Decrypt(item[1].GetValue()));
    }
  }

  PTRACE(3, "Gatekeeper server configured");
  return true;
}


H323GatekeeperCall * MyGatekeeperServer::CreateCall(const OpalGloballyUniqueID & id,
                                                    H323GatekeeperCall::Direction dir)
{
  return new MyGatekeeperCall(*this, id, dir);
}


PBoolean MyGatekeeperServer::TranslateAliasAddress(const H225_AliasAddress & alias,
                                                   H225_ArrayOf_AliasAddress & aliases,
                                                   H323TransportAddress & address,
                                                   PBoolean & isGkRouted,
                                                   H323GatekeeperCall * call)
{
#ifdef H323_TRANSNEXUS_OSP
  if (ospProvider != NULL) {
    address = "127.0.0.1";
    return true;
  }
#endif

  if (H323GatekeeperServer::TranslateAliasAddress(alias, aliases, address, isGkRouted, call))
    return true;

  PString aliasString = H323GetAliasAddressString(alias);
  PTRACE(4, "Translating \"" << aliasString << "\" through route maps");

  PWaitAndSignal mutex(reconfigurationMutex);

  for (PINDEX i = 0; i < routes.GetSize(); i++) {
    PTRACE(4, "Checking route map " << routes[i]);
    if (routes[i].IsMatch(aliasString)) {
      address = routes[i].GetHost();
      PTRACE(3, "Translated \"" << aliasString << "\" to " << address);
      break;
    }
  }

  return true;
}


MyGatekeeperServer::RouteMap::RouteMap(const PString & theAlias, const PString & theHost)
  : alias(theAlias),
    regex('^' + theAlias + '$'),
    host(theHost)
{
}


void MyGatekeeperServer::RouteMap::PrintOn(ostream & strm) const
{
  strm << '"' << alias << "\" => " << host;
}


bool MyGatekeeperServer::RouteMap::IsValid() const
{
  return !alias && regex.GetErrorCode() == PRegularExpression::NoError && !host;
}


bool MyGatekeeperServer::RouteMap::IsMatch(const PString & alias) const
{
  PINDEX start;
  return regex.Execute(alias, start);
}


///////////////////////////////////////////////////////////////

MyGatekeeperCall::MyGatekeeperCall(MyGatekeeperServer & gk,
                                   const OpalGloballyUniqueID & id,
                                   Direction dir)
  : H323GatekeeperCall(gk, id, dir)
{
#ifdef H323_TRANSNEXUS_OSP
  ospTransaction = NULL;
#endif
}

#ifdef H323_TRANSNEXUS_OSP
static bool GetE164Alias(const H225_ArrayOf_AliasAddress & aliases, H225_AliasAddress & destAlias)
{
  PINDEX i;
  for (i = 0; i < aliases.GetSize(); ++i) {
    if (aliases[i].GetTag() == H225_AliasAddress::e_dialedDigits) {
      destAlias = aliases[i];
      return true;
    }
  }
  return false;
}

PBoolean MyGatekeeperCall::AuthoriseOSPCall(H323GatekeeperARQ & info)
{
  int result;

  // if making call, authorise the call and insert the token
  if (!info.arq.m_answerCall) {

    OpalOSP::Transaction::AuthorisationInfo authInfo;

    // get the source call signalling address
    if (info.arq.HasOptionalField(H225_AdmissionRequest::e_srcCallSignalAddress))
      authInfo.ospvSource = OpalOSP::TransportAddressToOSPString(info.arq.m_srcCallSignalAddress);
    else
      authInfo.ospvSource = OpalOSP::TransportAddressToOSPString(info.endpoint->GetSignalAddress(0));

    // get the source number
    if (!GetE164Alias(info.arq.m_srcInfo, authInfo.callingNumber)) {
      PTRACE(1, "OSP\tNo E164 source address in ARQ");
      info.arj.m_rejectReason.SetTag(H225_AdmissionRejectReason::e_incompleteAddress);
      return false;
    }

    // get the dest number
    if (!info.arq.HasOptionalField(H225_AdmissionRequest::e_destinationInfo)) {
      PTRACE(1, "OSP\tNo dest aliases in ARQ");
      info.arj.m_rejectReason.SetTag(H225_AdmissionRejectReason::e_incompleteAddress);
      return false;
    }
    if (!GetE164Alias(info.arq.m_destinationInfo, authInfo.calledNumber)) {
      PTRACE(1, "OSP\tNo E164 source address in ARQ");
      info.arj.m_rejectReason.SetTag(H225_AdmissionRejectReason::e_incompleteAddress);
      return false;
    }

    // get the call ID
    authInfo.callID           = this->GetCallIdentifier();

    // authorise the call
    unsigned numberOfDestinations = 1;
    if ((result = ospTransaction->Authorise(authInfo, numberOfDestinations)) != 0) {
      PTRACE(1, "OSP\tCannot authorise ARQ - result = " << result);
      info.arj.m_rejectReason.SetTag(H225_AdmissionRejectReason::e_incompleteAddress);
      return false;
    } 
    if (numberOfDestinations == 0) {
      PTRACE(1, "OSP\tNo destinations available");
      info.arj.m_rejectReason.SetTag(H225_AdmissionRejectReason::e_noRouteToDestination);
      return false;
    }

    // get the destination
    OpalOSP::Transaction::DestinationInfo destInfo;
    if ((result = ospTransaction->GetFirstDestination(destInfo)) != 0) {
      PTRACE(1, "OSP\tCannot get first destination - result = " << result);
      info.arj.m_rejectReason.SetTag(H225_AdmissionRejectReason::e_undefinedReason);
      return false;
    } 

    // insert destination into the ACF
    if (!destInfo.Insert(info.acf)) {
      PTRACE(1, "OSP\tCannot insert information info ACF");
      info.arj.m_rejectReason.SetTag(H225_AdmissionRejectReason::e_undefinedReason);
      return false;
    }

    PTRACE(4, "OSP routed call to " << destInfo.calledNumber << "@" << destInfo.destinationAddress);
    return true;
  }

  // if answering call, validate the token
  OpalOSP::Transaction::ValidationInfo valInfo;

  // get the token
  if (!info.arq.HasOptionalField(H225_AdmissionRequest::e_tokens) || 
     !valInfo.ExtractToken(info.arq.m_tokens)) {
    PTRACE(1, "OSP\tNo tokens in in ARQ");
    info.arj.m_rejectReason.SetTag(H225_AdmissionRejectReason::e_invalidPermission);
    return false;
  }

  // get the source call signalling address
  if (info.arq.HasOptionalField(H225_AdmissionRequest::e_srcCallSignalAddress))
    valInfo.ospvSource = OpalOSP::TransportAddressToOSPString(info.arq.m_srcCallSignalAddress);
  else
    valInfo.ospvSource = OpalOSP::TransportAddressToOSPString(info.endpoint->GetSignalAddress(0));

  // get the dest call signalling address
  if (info.arq.HasOptionalField(H225_AdmissionRequest::e_destCallSignalAddress))
    valInfo.ospvDest = OpalOSP::TransportAddressToOSPString(info.arq.m_destCallSignalAddress);
  else
    valInfo.ospvDest = OpalOSP::TransportAddressToOSPString(info.endpoint->GetSignalAddress(0));

  // get the source number
  if (!GetE164Alias(info.arq.m_srcInfo, valInfo.callingNumber)) {
    PTRACE(1, "OSP\tNo E164 source address in ARQ");
    info.arj.m_rejectReason.SetTag(H225_AdmissionRejectReason::e_incompleteAddress);
    return false;
  }

  // get the dest number
  if (!info.arq.HasOptionalField(H225_AdmissionRequest::e_destinationInfo)) {
    PTRACE(1, "OSP\tNo dest aliases in ARQ");
    info.arj.m_rejectReason.SetTag(H225_AdmissionRejectReason::e_incompleteAddress);
    return false;
  }
  if (!GetE164Alias(info.arq.m_destinationInfo, valInfo.calledNumber)) {
    PTRACE(1, "OSP\tNo E164 source address in ARQ");
    info.arj.m_rejectReason.SetTag(H225_AdmissionRejectReason::e_incompleteAddress);
    return false;
  }

  // get the call ID
  valInfo.callID = this->GetCallIdentifier();

  // validate the token
  bool authorised;
  unsigned timeLimit;
  if ((result = ospTransaction->Validate(valInfo, authorised, timeLimit)) != 0) {
    PTRACE(1, "OSP\tCannot validate ARQ - result = " << result);
    info.arj.m_rejectReason.SetTag(H225_AdmissionRejectReason::e_invalidPermission);
    return false;
  }

  if (!authorised) {
    PTRACE(1, "OSP\tCall not authorised");
    info.arj.m_rejectReason.SetTag(H225_AdmissionRejectReason::e_requestDenied);
    return false;
  }

  PTRACE(4, "OSP authorised call with time limit of " << timeLimit);
  return true;
}

#endif

H323GatekeeperRequest::Response MyGatekeeperCall::OnAdmission(H323GatekeeperARQ & info)
{
#ifdef TEST_TOKEN
  info.acf.IncludeOptionalField(H225_AdmissionConfirm::e_tokens);
  info.acf.m_tokens.SetSize(1);
  info.acf.m_tokens[0].m_tokenOID = "1.2.36.76840296.1";
  info.acf.m_tokens[0].IncludeOptionalField(H235_ClearToken::e_nonStandard);
  info.acf.m_tokens[0].m_nonStandard.m_nonStandardIdentifier = "1.2.36.76840296.1.1";
  info.acf.m_tokens[0].m_nonStandard.m_data = "SnfYt0jUuZ4lVQv8umRYaH2JltXDRW6IuYcnASVU";
#endif

#ifdef H323_TRANSNEXUS_OSP
  OpalOSP::Provider * ospProvider = ((MyGatekeeperServer &)gatekeeper).GetOSPProvider();
  if (ospProvider != NULL) {
    // need to add RIP as clearing house is involved
    if (info.IsFastResponseRequired())
      return H323GatekeeperRequest::InProgress(30000);

    ospTransaction = new OpalOSP::Transaction();
    int result;
    if ((result = ospTransaction->Open(*ospProvider)) != 0) {
      delete ospTransaction;
      ospTransaction = NULL;
      PTRACE(1, "OSP\tCannot open transaction - result = " << result);
      info.arj.m_rejectReason.SetTag(H225_AdmissionRejectReason::e_exceedsCallCapacity);
      return H323GatekeeperRequest::Reject;
    }

    if (!AuthoriseOSPCall(info)) {
      delete ospTransaction;
      ospTransaction = NULL;
      return H323GatekeeperRequest::Reject;
    }

    return H323GatekeeperRequest::Confirm;
#endif

  return H323GatekeeperCall::OnAdmission(info);
}


MyGatekeeperCall::~MyGatekeeperCall()
{
#ifdef H323_TRANSNEXUS_OSP
  if (ospTransaction != NULL) {
    ospTransaction->SetEndReason(callEndReason);
    PTimeInterval duration;
    if (connectedTime.GetTimeInSeconds() != 0 && callEndTime.GetTimeInSeconds() != 0)
      duration = callEndTime - connectedTime;
    ospTransaction->CallEnd(callEndTime.GetTimeInSeconds());
    delete ospTransaction;
  }
#endif
}

#endif // OPAL_H323


// End of File ///////////////////////////////////////////////////////////////
