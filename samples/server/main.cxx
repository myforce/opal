/*
 * main.cxx
 *
 * OPAL Server main program
 *
 * Copyright (c) 2003 Equivalence Pty. Ltd.
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
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): Vox Lucida Pty. Ltd.
 *                 BCS Global, Inc.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#include "precompile.h"
#include "main.h"
#include "custom.h"


PCREATE_PROCESS(MyProcess);

static const char ParametersSection[] = "Parameters";

const WORD DefaultHTTPPort = 1719;
const WORD DefaultTelnetPort = 1718;
static const char UsernameKey[] = "Username";
static const char PasswordKey[] = "Password";
static const char LogLevelKey[] = "Log Level";
static const char LogFileKey[] = "Log File";
static const char LogRotateDirKey[]  = "Log Rotate Directory";
static const char LogRotateSizeKey[] = "Log Rotate File Size";
static const char LogRotateCountKey[] = "Log Rotate File Count";
static const char LogRotateAgeKey[] = "Log Rotate File Age";
static const char DefaultAddressFamilyKey[] = "AddressFamily";
#if OPAL_PTLIB_SSL
static const char HTTPCertificateFileKey[]  = "HTTP Certificate";
#endif
static const char HttpPortKey[] = "HTTP Port";
static const char TelnetPortKey[] = "Console Port";
static const char DisplayNameKey[] = "Display Name";
static const char MediaTransferModeKey[] = "Media Transfer Mode";
static const char AutoStartKeyPrefix[] = "Auto Start ";
static const char PreferredMediaKey[] = "Preferred Media";
static const char RemovedMediaKey[] = "Removed Media";
static const char MinJitterKey[] = "Minimum Jitter";
static const char MaxJitterKey[] = "Maximum Jitter";
static const char TCPPortBaseKey[] = "TCP Port Base";
static const char TCPPortMaxKey[] = "TCP Port Max";
static const char UDPPortBaseKey[] = "UDP Port Base";
static const char UDPPortMaxKey[] = "UDP Port Max";
static const char RTPPortBaseKey[] = "RTP Port Base";
static const char RTPPortMaxKey[] = "RTP Port Max";
static const char RTPTOSKey[] = "RTP Type of Service";
#if P_NAT
static const char NATMethodKey[] = "NAT Method";
static const char NATServerKey[] = "NAT Server";
static const char STUNServerKey[] = "STUN Server";
#endif

static const char OverrideProductInfoKey[] = "Override Product Info";
static const char VendorNameKey[] = "Product Vendor";
static const char ProductNameKey[] = "Product Name";
static const char ProductVersionKey[] = "Product Version";

#if OPAL_SKINNY
static const char SkinnyServerKey[] = "SCCP Server";
static const char SkinnyTypeKey[] = "SCCP Device Type";
static const char SkinnyNamesKey[] = "SCCP Device Names";
#endif

#if OPAL_LID
static const char LIDKey[] = "Line Interface Devices";
#endif

#if OPAL_CAPI
static const char EnableCAPIKey[] = "CAPI ISDN";
#endif

#if OPAL_IVR
static const char VXMLKey[] = "VXML Script";
static const char IVRCacheKey[] = "TTS Cache Directory";
static const char IVRRecordDirKey[] = "Record Message Directory";
#endif

#if OPAL_HAS_MIXER
static const char ConfAudioOnlyKey[] = "Conference Audio Only";
static const char ConfMediaPassThruKey[] = "Conference Media Pass Through";
static const char ConfVideoResolutionKey[] = "Conference Video Resolution";
#endif

#if OPAL_SCRIPT
static const char ScriptLanguageKey[] = "Language";
static const char ScriptTextKey[] = "Script";
#endif

#define ROUTES_SECTION "Routes"
#define ROUTES_KEY "Route"

static const char RouteAPartyKey[] = "A Party";
static const char RouteBPartyKey[] = "B Party";
static const char RouteDestKey[]   = "Destination";

#define CONFERENCE_NAME "conference"

#define ROUTE_USERNAME(name, to) ".*:.*\t" name "|.*:" name "@.* = " to

static const char * const DefaultRoutes[] = {
  ROUTE_USERNAME("loopback", "loopback:"),
#if OPAL_IVR
  ROUTE_USERNAME("#", "ivr:"),
#endif
#if OPAL_HAS_MIXER
  ROUTE_USERNAME(CONFERENCE_NAME, OPAL_PREFIX_MIXER":<du>"),
#endif
#if OPAL_LID
  "pots:\\+*[0-9]+ = tel:<dn>",
  "pstn:\\+*[0-9]+ = tel:<dn>",
#endif
#if OPAL_CAPI
  "capi:\\+*[0-9]+ = tel:<dn>",
#endif
#if OPAL_H323
  "h323:\\+*[0-9]+ = tel:<dn>",
 #if OPAL_SIP
  "h323:.+@.+ = sip:<da>",
  "h323:.* = sip:<db>@",
  "sip:.* = h323:<du>",
 #else
  "tel:[0-9]+\\*[0-9]+\\*[0-9]+\\*[0-9]+ = h323:<dn2ip>",
  "tel:.*=h323:<dn>"
 #endif
#endif
#if OPAL_SIP
  "sip:\\+*[0-9]+@.* = tel:<dn>",
  "tel:[0-9]+\\*[0-9]+\\*[0-9]+\\*[0-9]+ = sip:<dn2ip>",
  "tel:.*=sip:<dn>"
#endif
};

static char LoopbackPrefix[] = "loopback";


///////////////////////////////////////////////////////////////////////////////

MyProcess::MyProcess()
  : MyProcessAncestor(ProductInfo)
{
}


PBoolean MyProcess::OnStart()
{
  // Set log level as early as possible
  {
    PConfig cfg(ParametersSection);
    PString file = cfg.GetString(LogFileKey);
    if (!file.IsEmpty()) {
      PSystemLogToFile * logFile = new PSystemLogToFile(file);
      PSystemLogToFile::RotateInfo info = logFile->GetRotateInfo();
      info.m_directory = cfg.GetString(LogRotateDirKey, info.m_directory);
      info.m_maxSize = cfg.GetInteger(LogRotateSizeKey, info.m_maxSize / 1000) * 1000;
      info.m_maxFileCount = cfg.GetInteger(LogRotateCountKey, info.m_maxFileCount);
      info.m_maxFileAge.SetInterval(0, 0, 0, 0, cfg.GetInteger(LogRotateAgeKey, info.m_maxFileAge.GetDays()));
      logFile->SetRotateInfo(info, true);
      PSystemLog::SetTarget(logFile);
    }
    SetLogLevel(cfg.GetEnum(LogLevelKey, GetLogLevel()));
  }

  return m_manager.PreInitialise(GetArguments(), false) &&
         PHTTPServiceProcess::OnStart() &&
         m_manager.Initialise(GetArguments(), false);
}


void MyProcess::OnStop()
{
  m_manager.ShutDownEndpoints();
  PHTTPServiceProcess::OnStop();
}


void MyProcess::OnControl()
{
  // This function get called when the Control menu item is selected in the
  // tray icon mode of the service.
  PStringStream url;
  url << "http://";

  PString host = PIPSocket::GetHostName();
  PIPSocket::Address addr;
  if (PIPSocket::GetHostAddress(host, addr))
    url << host;
  else
    url << "localhost";

  url << ':' << DefaultHTTPPort;

  PURL::OpenBrowser(url);
}


void MyProcess::OnConfigChanged()
{
}


PBoolean MyProcess::Initialise(const char * initMsg)
{
  PConfig cfg(ParametersSection);

  // Get the HTTP basic authentication info
  PString username = cfg.GetString(UsernameKey);
  PString password = PHTTPPasswordField::Decrypt(cfg.GetString(PasswordKey));

  PString addressFamily = cfg.GetString(DefaultAddressFamilyKey, "IPV4");
#if P_HAS_IPV6
  if(addressFamily *= "IPV6")
	 PIPSocket::SetDefaultIpAddressFamilyV6();
#endif

  PHTTPSimpleAuth authority(GetName(), username, password);

  // Create the parameters URL page, and start adding fields to it
  PConfigPage * rsrc = new PConfigPage(*this, "Parameters", ParametersSection, authority);

  // HTTP authentication username/password
  rsrc->AddStringField(UsernameKey, 25, username, "User name to access HTTP user interface for server.");
  rsrc->Add(new PHTTPPasswordField(PasswordKey, 20, password));

  // Log level for messages
  rsrc->AddIntegerField(LogLevelKey,
                        PSystemLog::Fatal, PSystemLog::NumLogLevels-1,
                        GetLogLevel(),
                        "1=Fatal only, 2=Errors, 3=Warnings, 4=Info, 5=Debug");
  SetLogLevel(cfg.GetEnum(LogLevelKey, GetLogLevel()));

  PSystemLogToFile * logFile = dynamic_cast<PSystemLogToFile *>(&PSystemLog::GetTarget());

  PString logFileName = rsrc->AddStringField(LogFileKey, 0, logFile != NULL ? logFile->GetFilePath() : PString::Empty(),
                                             "File for logging output, empty string disables logging", 1, 80);
  if (logFileName.IsEmpty()) {
    if (logFile != NULL)
      PSystemLog::SetTarget(new PSystemLogToNowhere);
  }
  else {
    if (logFile == NULL || logFile->GetFilePath() != logFileName)
      PSystemLog::SetTarget(new PSystemLogToFile(logFileName));
  }

  if ((logFile = dynamic_cast<PSystemLogToFile *>(&PSystemLog::GetTarget())) != NULL) {
    PSystemLogToFile::RotateInfo info = logFile->GetRotateInfo();
    info.m_directory = rsrc->AddStringField(LogRotateDirKey, 0, info.m_directory,
                                           "Directory path for log file rotation", 1, 80);
    info.m_maxSize = rsrc->AddIntegerField(LogRotateSizeKey, 0, INT_MAX, info.m_maxSize / 1000,
                                           "kb", "Size of log file to trigger rotation, zero disables")*1000;
    info.m_maxFileCount = rsrc->AddIntegerField(LogRotateCountKey, 0, 10000, info.m_maxFileCount,
                                                "", "Number of rotated log file to keep, zero is infinite");
    info.m_maxFileAge.SetInterval(0, 0, 0, 0, rsrc->AddIntegerField(LogRotateAgeKey,
                                              0, 10000, info.m_maxFileAge.GetDays(),
                                              "days", "Number of days to keep rotated log files, zero is infinite"));
    logFile->SetRotateInfo(info);
  }

#if OPAL_PTLIB_SSL
  // SSL certificate file.
  PString certificateFile = cfg.GetString(HTTPCertificateFileKey);
  rsrc->AddStringField(HTTPCertificateFileKey, 250, certificateFile,
      "Certificate for HTTPS user interface, if empty HTTP is used.", 1, 50);
  if (certificateFile.IsEmpty())
    DisableSSL();
  else if (!SetServerCertificate(certificateFile, true)) {
    PSYSTEMLOG(Fatal, "MyProcess\tCould not load certificate \"" << certificateFile << '"');
    return false;
  }
#endif

  // HTTP Port number to use.
  WORD httpPort = (WORD)rsrc->AddIntegerField(HttpPortKey, 1, 65535, DefaultHTTPPort,
                                      "", "Port for HTTP user interface for server.");

  // Configure the core of the system
  if (!m_manager.Configure(cfg, rsrc))
    return false;

  // Finished the resource to add, generate HTML for it and add to name space
  PServiceHTML html("System Parameters");
  rsrc->BuildHTML(html);
  httpNameSpace.AddResource(rsrc, PHTTPSpace::Overwrite);

#if OPAL_H323 | OPAL_SIP
  RegistrationStatusPage * registrationStatusPage = new RegistrationStatusPage(m_manager, authority);
  httpNameSpace.AddResource(registrationStatusPage, PHTTPSpace::Overwrite);
#endif

  CallStatusPage * callStatusPage = new CallStatusPage(m_manager, authority);
  httpNameSpace.AddResource(callStatusPage, PHTTPSpace::Overwrite);

#if OPAL_H323
  GkStatusPage * gkStatusPage = new GkStatusPage(m_manager, authority);
  httpNameSpace.AddResource(gkStatusPage, PHTTPSpace::Overwrite);
#endif // OPAL_H323

  CDRListPage * cdrListPage = new CDRListPage(m_manager, authority);
  httpNameSpace.AddResource(cdrListPage, PHTTPSpace::Overwrite);
  httpNameSpace.AddResource(new CDRPage(m_manager, authority), PHTTPSpace::Overwrite);

  // Log file resource
  PHTTPFile * fullLog = NULL;
  ClearLogPage * clearLog = NULL;
  PHTTPTailFile * tailLog = NULL;

  if (logFile != NULL) {
    fullLog = new PHTTPFile("FullLog", logFile->GetFilePath(), PMIMEInfo::TextPlain(), authority);
    httpNameSpace.AddResource(fullLog, PHTTPSpace::Overwrite);

    clearLog = new ClearLogPage(m_manager, authority);
    httpNameSpace.AddResource(clearLog, PHTTPSpace::Overwrite);

    tailLog = new PHTTPTailFile("TailLog", logFile->GetFilePath(), PMIMEInfo::TextPlain(), authority);
    httpNameSpace.AddResource(tailLog, PHTTPSpace::Overwrite);
  }

  // Create the home page
  static const char welcomeHtml[] = "welcome.html";
  if (PFile::Exists(welcomeHtml))
    httpNameSpace.AddResource(new PServiceHTTPFile(welcomeHtml, true), PHTTPSpace::Overwrite);
  else {
    PHTML html;
    html << PHTML::Title("Welcome to " + GetName())
         << PHTML::Body()
         << GetPageGraphic()
         << PHTML::Paragraph() << "<center>"

         << PHTML::HotLink(rsrc->GetHotLink()) << "System Parameters" << PHTML::HotLink()
         << PHTML::Paragraph()
         << PHTML::HotLink(callStatusPage->GetHotLink()) << "Call Status" << PHTML::HotLink()
#if OPAL_H323
         << PHTML::BreakLine()
         << PHTML::HotLink(gkStatusPage->GetHotLink()) << "Gatekeeper Status" << PHTML::HotLink()
#endif // OPAL_H323
#if OPAL_H323 | OPAL_SIP
         << PHTML::BreakLine()
         << PHTML::HotLink(registrationStatusPage->GetHotLink()) << "Registration Status" << PHTML::HotLink()
#endif
         << PHTML::Paragraph()
         << PHTML::HotLink(cdrListPage->GetHotLink()) << "Call Detail Records" << PHTML::HotLink()
         << PHTML::Paragraph();

    if (logFile != NULL)
      html << PHTML::HotLink(fullLog->GetHotLink()) << "Full Log File" << PHTML::HotLink()
           << PHTML::BreakLine()
           << PHTML::HotLink(clearLog->GetHotLink()) << "Clear Log File" << PHTML::HotLink()
           << PHTML::BreakLine()
           << PHTML::HotLink(tailLog->GetHotLink()) << "Tail Log File" << PHTML::HotLink()
           << PHTML::Paragraph();


    html << PHTML::HRule()
         << GetCopyrightText()
         << PHTML::Body();
    httpNameSpace.AddResource(new PServiceHTTPString(welcomeHtml, html), PHTTPSpace::Overwrite);
  }

  // set up the HTTP port for listening & start the first HTTP thread
  if (ListenForHTTP(httpPort))
    PSYSTEMLOG(Info, "Opened master socket(s) for HTTP: " << m_httpListeningSockets.front().GetPort());
  else {
    PSYSTEMLOG(Fatal, "Cannot run without HTTP");
    return false;
  }

  PSYSTEMLOG(Info, "Service " << GetName() << ' ' << initMsg);
  return true;
}


#if OPAL_PTLIB_SSL
void MyManager::ConfigureSecurity(OpalEndPoint * ep,
                                  const PString & signalingKey,
                                  const PString & suitesKey,
                                  PConfig & cfg,
                                  PConfigPage * rsrc)
{
  PString normalPrefix = ep->GetPrefixName();
  PString securePrefix = normalPrefix + 's';

  PINDEX security = 0;
  if (FindEndPoint(normalPrefix) != NULL)
    security |= 1;
  if (FindEndPoint(securePrefix) != NULL)
    security |= 2;

  security = cfg.GetInteger(signalingKey, security);

  switch (security) {
    case 1:
      AttachEndPoint(ep, normalPrefix);
      DetachEndPoint(securePrefix);
      break;

    case 2:
      AttachEndPoint(ep, securePrefix);
      DetachEndPoint(normalPrefix);
      break;

    default:
      AttachEndPoint(ep, normalPrefix);
      AttachEndPoint(ep, securePrefix);
  }

  PStringArray valueArray, titleArray;
  valueArray[0] = '1'; titleArray[0] = normalPrefix & "only";
  valueArray[1] = '2'; titleArray[1] = securePrefix & "only";
  valueArray[2] = '3'; titleArray[2] = normalPrefix & '&' & securePrefix;
  rsrc->Add(new PHTTPRadioField(signalingKey, valueArray, titleArray, security - 1, "Signaling security methods available."));

  ep->SetMediaCryptoSuites(rsrc->AddSelectArrayField(suitesKey, true, ep->GetMediaCryptoSuites(),
                                            ep->GetAllMediaCryptoSuites(), "Media security methods available."));
}
#endif // OPAL_PTLIB_SSL


///////////////////////////////////////////////////////////////

MyManager::MyManager()
  : MyManagerParent(OPAL_CONSOLE_PREFIXES OPAL_PREFIX_PCSS" "OPAL_PREFIX_IVR" "OPAL_PREFIX_MIXER)
  , m_mediaTransferMode(MediaTransferForward)
#if OPAL_CAPI
  , m_enableCAPI(true)
#endif
  , m_cdrListMax(100)
{
  new OpalLocalEndPoint(*this, LoopbackPrefix);
  OpalMediaFormat::RegisterKnownMediaFormats(); // Make sure codecs are loaded
}


MyManager::~MyManager()
{
}


void MyManager::EndRun(bool)
{
  PServiceProcess::Current().OnStop();
}


#if OPAL_H323
H323ConsoleEndPoint * MyManager::CreateH323EndPoint()
{
  return new MyH323EndPoint(*this);
}
#endif


#if OPAL_SIP
SIPConsoleEndPoint * MyManager::CreateSIPEndPoint()
{
  return new MySIPEndPoint(*this);
}
#endif


PBoolean MyManager::Configure(PConfig & cfg, PConfigPage * rsrc)
{
  PINDEX arraySize;

  // Make sure all endpoints created
  for (PINDEX i = 0; i < m_endpointPrefixes.GetSize(); ++i)
    GetConsoleEndPoint(m_endpointPrefixes[i]);

  PString defaultSection = cfg.GetDefaultSection();

#if P_CLI && P_TELNET
  // Telnet Port number to use.
  {
    WORD telnetPort = (WORD)rsrc->AddIntegerField(TelnetPortKey, 0, 65535, DefaultTelnetPort,
                                   "", "Port for console user interface (telnet) of server (zero disables).");
    PCLISocket * cliSocket = dynamic_cast<PCLISocket *>(m_cli);
    if (cliSocket != NULL && telnetPort != 0)
      cliSocket->Listen(telnetPort);
    else {
      delete m_cli;
      if (telnetPort == 0)
        m_cli = NULL;
      else {
        m_cli = new PCLITelnet(telnetPort);
        m_cli->Start();
      }
    }
  }
#endif //P_CLI && P_TELNET

  // General parameters for all endpoint types
  SetDefaultDisplayName(rsrc->AddStringField(DisplayNameKey, 25, GetDefaultDisplayName(), "Display name used in various protocols"));

  m_mediaTransferMode = cfg.GetEnum(MediaTransferModeKey, m_mediaTransferMode);
  static const char * const MediaTransferModeValues[] = { "0", "1", "2" };
  static const char * const MediaTransferModeTitles[] = { "Bypass", "Forward", "Transcode" };
  rsrc->Add(new PHTTPRadioField(MediaTransferModeKey,
                    PARRAYSIZE(MediaTransferModeValues), MediaTransferModeValues, MediaTransferModeTitles,
                    m_mediaTransferMode, "How media is to be routed between the endpoints."));

  {
    OpalMediaTypeList mediaTypes = OpalMediaType::GetList();
    for (OpalMediaTypeList::iterator it = mediaTypes.begin(); it != mediaTypes.end(); ++it) {
      PString key = AutoStartKeyPrefix;
      key &= it->c_str();

      (*it)->SetAutoStart(cfg.GetEnum<OpalMediaType::AutoStartMode::Enumeration>(key, (*it)->GetAutoStart()));

      static const char * const AutoStartValues[] = { "Offer inactive", "Receive only", "Send only", "Send & Receive", "Don't offer" };
      rsrc->Add(new PHTTPEnumField<OpalMediaType::AutoStartMode::Enumeration>(key,
                    PARRAYSIZE(AutoStartValues), AutoStartValues, (*it)->GetAutoStart(),
                    "Initial start up mode for media type."));
    }
  }

  {
    OpalMediaFormatList allFormats;
    PList<OpalEndPoint> endpoints = GetEndPoints();
    for (PList<OpalEndPoint>::iterator it = endpoints.begin(); it != endpoints.end(); ++it)
      allFormats += it->GetMediaFormats();
    OpalMediaFormatList transportableFormats;
    for (OpalMediaFormatList::iterator it = allFormats.begin(); it != allFormats.end(); ++it) {
      if (it->IsTransportable())
        transportableFormats += *it;
    }
    PStringStream help;
    help << "Preference order for codecs to be offered to remotes.<p>"
      "Note, these are not regular expressions, just simple "
      "wildcards where '*' matches any number of characters.<p>"
      "Known media formats are:<br>";
    for (OpalMediaFormatList::iterator it = transportableFormats.begin(); it != transportableFormats.end(); ++it) {
      if (it != transportableFormats.begin())
        help << ", ";
      help << *it;
    }
    SetMediaFormatOrder(rsrc->AddStringArrayField(PreferredMediaKey, true, 25, GetMediaFormatOrder(), help));
  }

  SetMediaFormatMask(rsrc->AddStringArrayField(RemovedMediaKey, true, 25, GetMediaFormatMask(),
                     "Codecs to be prevented from being used.<p>"
                     "These are wildcards as in the above Preferred Media, with "
                     "the addition of preceding the expression with a '!' which "
                     "removes all formats <i>except</i> the indicated wildcard. "
                     "Also, the '@' character may also be used to indicate a "
                     "media type, e.g. <code>@video</code> removes all video "
                     "codecs."));

  SetAudioJitterDelay(rsrc->AddIntegerField(MinJitterKey, 20, 2000, GetMinAudioJitterDelay(), "ms", "Minimum jitter buffer size"),
                      rsrc->AddIntegerField(MaxJitterKey, 20, 2000, GetMaxAudioJitterDelay(), "ms", "Maximum jitter buffer size"));

  SetTCPPorts(rsrc->AddIntegerField(TCPPortBaseKey, 0, 65535, GetTCPPortBase(), "", "Base of port range for allocating TCP streams, e.g. H.323 signalling channel"),
              rsrc->AddIntegerField(TCPPortMaxKey, 0, 65535, GetTCPPortMax(), "", "Maximum of port range for allocating TCP streams"));
  SetUDPPorts(rsrc->AddIntegerField(UDPPortBaseKey, 0, 65535, GetUDPPortBase(), "", "Base of port range for allocating UDP streams, e.g. SIP signalling channel"),
              rsrc->AddIntegerField(UDPPortMaxKey, 0, 65535, GetUDPPortMax(), "", "Maximum of port range for allocating UDP streams"));
  SetRtpIpPorts(rsrc->AddIntegerField(RTPPortBaseKey, 0, 65535, GetRtpIpPortBase(), "", "Base of port range for allocating RTP/UDP streams"),
                rsrc->AddIntegerField(RTPPortMaxKey, 0, 65535, GetRtpIpPortMax(), "", "Maximum of port range for allocating RTP/UDP streams"));

  SetMediaTypeOfService(rsrc->AddIntegerField(RTPTOSKey, 0, 255, GetMediaTypeOfService(), "", "Value for DIFSERV Quality of Service"));

#if P_STUN
  SetNATServer(rsrc->AddStringField(NATMethodKey, 25, PSTUNClient::MethodName(), "Method for NAT traversal"),
               rsrc->AddStringField(NATServerKey, 100, cfg.GetString(STUNServerKey, GetNATServer()), "Server IP/hostname for NAT traversal", 1, 30));
#endif // P_NAT

  bool overrideProductInfo = rsrc->AddBooleanField(OverrideProductInfoKey, false, "Override the default product information");
  OpalProductInfo productInfo = GetProductInfo();
  productInfo.vendor = rsrc->AddStringField(VendorNameKey, 20, productInfo.vendor);
  productInfo.name = rsrc->AddStringField(ProductNameKey, 20, productInfo.name);
  productInfo.version = rsrc->AddStringField(ProductVersionKey, 20, productInfo.version);
  if (overrideProductInfo)
    SetProductInfo(productInfo);

#if OPAL_H323
  if (!GetH323EndPoint().Configure(cfg, rsrc))
    return false;
#endif

#if OPAL_SIP
  if (!GetSIPEndPoint().Configure(cfg, rsrc))
    return false;
#endif

#if OPAL_SKINNY
  {
    OpalSkinnyEndPoint * ep = FindEndPointAs<OpalSkinnyEndPoint>(OPAL_PREFIX_SKINNY);
    PString server = rsrc->AddStringField(SkinnyServerKey, 20, PString::Empty(), "Server for Skinny Client Control Protocol");
    unsigned deviceType = rsrc->AddIntegerField(SkinnyTypeKey, 1, 32767, OpalSkinnyEndPoint::DefaultDeviceType,
                          "", "Device type for Skinny Client Control Protocol. Default 30016 = Cisco IP Communicator.");
    PStringArray names = ep->GetPhoneDeviceNames();
    names = rsrc->AddStringArrayField(SkinnyNamesKey, false, 30, names, "Device names for Skinny Client Control Protocol");
    if (!server.IsEmpty()) {
      for (PINDEX i = 0; i < names.GetSize(); ++i) {
        PString name = names[i];

        static PRegularExpression const Wildcards("\\[([0-9]+)-([0-9]+)\\]", PRegularExpression::Extended);
        PIntArray starts(3), ends(3);
        if (Wildcards.Execute(name, starts, ends)) {
          unsigned number = name(starts[1], ends[1]-1).AsUnsigned();
          unsigned lastNumber = name(starts[2], ends[2]-1).AsUnsigned();
          unsigned digits = ends[2] - starts[2];
          while (number <= lastNumber) {
            PString calculatedName = name.Left(starts[0]) + psprintf("%0*u", digits, number++) + name.Mid(ends[0]);
            if (!ep->Register(server, calculatedName)) {
              PSYSTEMLOG(Error, "Could not register " << calculatedName << " with skinny server \"" << server << '"');
            }
          }
        }
        else {
          if (!ep->Register(server, name, deviceType)) {
            PSYSTEMLOG(Error, "Could not register " << name << " with skinny server \"" << server << '"');
          }
        }
      }
    }
  }
#endif

#if OPAL_LID
  // Add POTS and PSTN endpoints
  if (!FindEndPointAs<OpalLineEndPoint>(OPAL_PREFIX_POTS)->AddDeviceNames(rsrc->AddSelectArrayField(LIDKey, false,
    OpalLineInterfaceDevice::GetAllDevices(), PStringArray(), "Line Interface Devices (PSTN, ISDN etc) to monitor, if any"))) {
    PSYSTEMLOG(Error, "No LID devices!");
  }
#endif // OPAL_LID


#if OPAL_CAPI
  m_enableCAPI = rsrc->AddBooleanField(EnableCAPIKey, m_enableCAPI, "Enable CAPI ISDN controller(s), if available");
  if (m_enableCAPI && FindEndPointAs<OpalCapiEndPoint>(OPAL_PREFIX_CAPI)->OpenControllers() == 0) {
    PSYSTEMLOG(Error, "No CAPI controllers!");
  }
#endif


#if OPAL_IVR
  {
    OpalIVREndPoint * ivrEP = FindEndPointAs<OpalIVREndPoint>(OPAL_PREFIX_IVR);
    // Set IVR protocol handler
    ivrEP->SetDefaultVXML(rsrc->AddStringField(VXMLKey, 0, PString::Empty(),
                          "Interactive Voice Response VXML script, may be a URL or the actual VXML", 10, 80));
    ivrEP->SetCacheDir(rsrc->AddStringField(IVRCacheKey, 0, ivrEP->GetCacheDir(),
                       "Interactive Voice Response directory to cache Text To Speech phrases", 1, 50));
    ivrEP->SetRecordDirectory(rsrc->AddStringField(IVRRecordDirKey, 0, ivrEP->GetRecordDirectory(),
                              "Interactive Voice Response directory to save recorded messages", 1, 50));
  }
#endif

#if OPAL_HAS_MIXER
  {
    OpalMixerEndPoint * mcuEP = FindEndPointAs<OpalMixerEndPoint>(OPAL_PREFIX_MIXER);
    OpalMixerNodeInfo adHoc;
    if (mcuEP->GetAdHocNodeInfo() != NULL)
      adHoc = *mcuEP->GetAdHocNodeInfo();
    adHoc.m_mediaPassThru = rsrc->AddBooleanField(ConfMediaPassThruKey, adHoc.m_mediaPassThru, "Conference media pass though optimisation");

#if OPAL_VIDEO
    adHoc.m_audioOnly = rsrc->AddBooleanField(ConfAudioOnlyKey, adHoc.m_audioOnly, "Conference is audio only");

    PVideoFrameInfo::ParseSize(rsrc->AddStringField(ConfVideoResolutionKey, 10,
                                                    PVideoFrameInfo::AsString(adHoc.m_width, adHoc.m_height),
                                                    "Conference video frame resolution"),
                               adHoc.m_width, adHoc.m_height);
#endif

    adHoc.m_closeOnEmpty = true;
    mcuEP->SetAdHocNodeInfo(adHoc);
  }
#endif

#if OPAL_SCRIPT
  PFactory<PScriptLanguage>::KeyList_T keys = PFactory<PScriptLanguage>::GetKeyList();
  if (!keys.empty()) {
    PStringArray languages;
    for (PFactory<PScriptLanguage>::KeyList_T::iterator it = keys.begin(); it != keys.end(); ++it)
      languages.AppendString(*it);
    PString language = cfg.GetString(ScriptLanguageKey, languages[0]);
    rsrc->Add(new PHTTPRadioField(ScriptLanguageKey, languages,
              languages.GetValuesIndex(language),"Interpreter script language."));
  
    PString script = rsrc->AddStringField(ScriptTextKey, 0, PString::Empty(),
        "Interpreter script, may be a filename or the actual script text", 10, 80);
    if (m_scriptLanguage != language || m_scriptText != script) {
      m_scriptLanguage = language;
      m_scriptText = script;
      RunScript(script, language);
    }
  }
#endif //OPAL_SCRIPT

  // Routing
  RouteTable routes;
  arraySize = cfg.GetInteger(ROUTES_SECTION, ROUTES_KEY" Array Size");
  for (PINDEX i = 0; i < arraySize; i++) {
    cfg.SetDefaultSection(psprintf(ROUTES_SECTION"\\"ROUTES_KEY" %u", i+1));
    RouteEntry * entry = new RouteEntry(cfg.GetString(RouteAPartyKey),
                                        cfg.GetString(RouteBPartyKey),
                                        cfg.GetString(RouteDestKey));
    if (entry->IsValid())
      routes.Append(entry);
    else
      delete entry;
  }

  if (routes.IsEmpty()) {
    arraySize = 0;
    for (PINDEX i = 0; i < PARRAYSIZE(DefaultRoutes); ++i) {
      RouteEntry * entry = new RouteEntry(DefaultRoutes[i]);
      PAssert(entry->IsValid(), PLogicError);
      routes.Append(entry);

      cfg.SetDefaultSection(psprintf(ROUTES_SECTION"\\"ROUTES_KEY" %u", ++arraySize));
      cfg.SetString(RouteAPartyKey, entry->GetPartyA());
      cfg.SetString(RouteBPartyKey, entry->GetPartyB());
      cfg.SetString(RouteDestKey,   entry->GetDestination());
    }
    cfg.SetInteger(ROUTES_SECTION, ROUTES_KEY" Array Size", arraySize);
  }
  cfg.SetDefaultSection(defaultSection);

  PHTTPCompositeField * routeFields = new PHTTPCompositeField(
           ROUTES_SECTION"\\"ROUTES_KEY" %u\\", ROUTES_SECTION,
           "Internal routing of calls to varous sub-systems.<p>"
           "The A Party and B Party columns are regular expressions for the call "
           "originator and receiver respectively. The Destination string determines "
           "the endpoint used for the outbound leg of the route. This can be be "
           "constructed using various meta-strings that correspond to parts of the "
           "B Party address.<p>"
           "A Destination starting with the string 'label:' causes the router "
           "to restart searching from the beginning of the route table using "
           "the new string as the A Party<p>"
           "The available meta-strings are:<p>"
           "<dl>"
           "<dt><code>&lt;da&gt;</code></dt><dd>"
                 "Replaced by the B Party string. For example A Party=\"pc:.*\" "
                 "B Party=\".*\" Destination=\"sip:&lt;da&gt;\" directs calls "
                 "to the SIP protocol. In this case there is a special condition "
                 "where if the original destination had a valid protocol, eg "
                 "h323:fred.com, then the entire string is replaced not just "
                 "the &lt;da&gt; part.</dd>"
           "<dt><code>&lt;db&gt;</code></dt><dd>"
                 "Same as &lt;da&gt;, but without the special condtion.</dd>"
           "<dt><code>&lt;du&gt;</code></dt><dd>"
                 "Copy the \"user\" part of the B Party string. This is "
                 "essentially the component after the : and before the '@', or "
                 "the whole B Party string if these are not present.</dd>"
           "<dt><code>&lt;!du&gt;</code></dt><dd>"
                 "The rest of the B Party string after the &lt;du&gt; section. The "
                 "protocol is still omitted. This is usually the '@' and onward. "
                 "Note, if there is already an '@' in the destination before the "
                 "&lt;!du&gt; and what is about to replace it also has an '@' then "
                 "everything between the @ and the &lt;!du&gt; (inclusive) is deleted, "
                 "then the substitution is made so a legal URL can be produced.</dd>"
           "<dt><code>&lt;dn&gt;</code></dt><dd>"
                 "Copy all valid consecutive E.164 digits from the B Party so "
                 "pots:0061298765@vpb:1/2 becomes sip:0061298765@carrier.com</dd>"
           "<dt><code>&lt;dnX&gt;</code></dt><dd>"
                 "As above but skip X digits, eg &lt;dn2&gt; skips 2 digits, so "
                 "pots:00612198765 becomes sip:61298765@carrier.com</dd>"
           "<dt><code>&lt;!dn&gt;</code></dt><dd>"
                 "The rest of the B Party after the &lt;dn&gt; or &lt;dnX&gt; sections.</dd>"
           "<dt><code>&lt;dn2ip&gt;</code></dt><dd>"
                 "Translate digits separated by '*' characters to an IP "
                 "address. e.g. 10*0*1*1 becomes 10.0.1.1, also "
                 "1234*10*0*1*1 becomes 1234@10.0.1.1 and "
                 "1234*10*0*1*1*1722 becomes 1234@10.0.1.1:1722.</dd>"
           "</dl>"
           );
  routeFields->Append(new PHTTPStringField(RouteAPartyKey, 0, NULL, NULL, 1, 20));
  routeFields->Append(new PHTTPStringField(RouteBPartyKey, 0, NULL, NULL, 1, 20));
  routeFields->Append(new PHTTPStringField(RouteDestKey, 0, NULL, NULL, 1, 30));
  rsrc->Add(new PHTTPFieldArray(routeFields, true));

  SetRouteTable(routes);

  return ConfigureCDR(cfg, rsrc);
}


OpalCall * MyManager::CreateCall(void *)
{
  return new MyCall(*this);
}


MyManager::MediaTransferMode MyManager::GetMediaTransferMode(const OpalConnection &, const OpalConnection &, const OpalMediaType &) const
{
  return m_mediaTransferMode;
}


void MyManager::OnStartMediaPatch(OpalConnection & connection, OpalMediaPatch & patch)
{
  PSafePtr<OpalConnection> other = connection.GetOtherPartyConnection();
  if (other != NULL) {
    // Detect if we have loopback endpoint, and start pas thro on the other connection
    if (connection.GetPrefixName() == LoopbackPrefix)
      SetMediaPassThrough(*other, *other, true, patch.GetSource().GetSessionID());
    else if (other->GetPrefixName() == LoopbackPrefix)
      SetMediaPassThrough(connection, connection, true, patch.GetSource().GetSessionID());
  }

  dynamic_cast<MyCall &>(connection.GetCall()).OnStartMediaPatch(connection, patch);
  OpalManager::OnStartMediaPatch(connection, patch);
}


void MyManager::OnStopMediaPatch(OpalConnection & connection, OpalMediaPatch & patch)
{
  dynamic_cast<MyCall &>(connection.GetCall()).OnStopMediaPatch(patch);
  OpalManager::OnStopMediaPatch(connection, patch);
}


// End of File ///////////////////////////////////////////////////////////////
