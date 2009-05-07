/*
 * main.cxx
 *
 * Copyright (C) 2009 Post Increment
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
 * The Original Code is Opal
 *
 * The Initial Developer of the Original Code is Post Increment
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision:$
 * $Author:$
 * $Date:$
 */

#include "precompile.h"
#include "main.h"
#include "custom.h"



PCREATE_PROCESS(OpalEcho);


const WORD DefaultHTTPPort = 3246;

static const char UsernameKey[] = "Username";
static const char PasswordKey[] = "Password";
static const char LogLevelKey[] = "Log Level";

static const char HttpPortKey[]       = "HTTP Port";
static const char PreferredMediaKey[] = "Preferred Media";
static const char RemovedMediaKey[]   = "Removed Media";
static const char MinJitterKey[]      = "Minimum Jitter";
static const char MaxJitterKey[]      = "Maximum Jitter";
static const char TCPPortBaseKey[]    = "TCP Port Base";
static const char TCPPortMaxKey[]     = "TCP Port Max";
static const char UDPPortBaseKey[]    = "UDP Port Base";
static const char UDPPortMaxKey[]     = "UDP Port Max";
static const char RTPPortBaseKey[]    = "RTP Port Base";
static const char RTPPortMaxKey[]     = "RTP Port Max";

///////////////////////////////////////////////////////////////////////////////

class EchoConnection : public OpalLocalConnection
{
  PCLASSINFO(EchoConnection, OpalLocalConnection);
  public:
    EchoConnection(
      OpalCall & call,         ///<  Owner calll for connection
      EchoEndPoint & endpoint, ///<  Owner endpoint for connection
      void * userData          ///<  Arbitrary data to pass to connection
    );

    ~EchoConnection();

    bool OnReadMediaFrame(
      const OpalMediaStream & mediaStream,    ///<  Media stream data is required for
      RTP_DataFrame & frame                   ///<  RTP frame for data
    );

    bool OnWriteMediaFrame(
      const OpalMediaStream & mediaStream,    ///<  Media stream data is required for
      RTP_DataFrame & frame                   ///<  RTP frame for data
    );

  protected:
    EchoEndPoint & m_endpoint;

    PMutex m_mediaMutex;

    struct MediaInfo {
      MediaInfo()
        : m_data(NULL)
      { }

      PAdaptiveDelay m_delay;
      RTP_DataFrame * m_data;
      PSyncPoint m_readSync;
      PSyncPoint m_writeSync;
    };

    typedef std::map<std::string, MediaInfo> MediaMapType;
    MediaMapType m_mediaMap;
};

///////////////////////////////////////////////////////////////////////////////

OpalEcho::OpalEcho()
  : PHTTPServiceProcess(ProductInfo)
{
}


PBoolean OpalEcho::OnStart()
{
  // change to the default directory to the one containing the executable
  PDirectory exeDir = GetFile().GetDirectory();

#if defined(_WIN32) && defined(_DEBUG)
  // Special check to aid in using DevStudio for debugging.
  if (exeDir.Find("\\Debug\\") != P_MAX_INDEX)
    exeDir = exeDir.GetParent();
#endif
  exeDir.Change();

  httpNameSpace.AddResource(new PHTTPDirectory("data", "data"));
  httpNameSpace.AddResource(new PServiceHTTPDirectory("html", "html"));

  return PHTTPServiceProcess::OnStart();
}


void OpalEcho::OnStop()
{
  PHTTPServiceProcess::OnStop();
}


void OpalEcho::OnControl()
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


void OpalEcho::OnConfigChanged()
{
}



PBoolean OpalEcho::Initialise(const char * initMsg)
{
  PConfig cfg("Parameters");

  // Sert log level as early as possible
  SetLogLevel((PSystemLog::Level)cfg.GetInteger(LogLevelKey, GetLogLevel()));
#if PTRACING
  if (GetLogLevel() >= PSystemLog::Warning)
    PTrace::SetLevel(GetLogLevel()-PSystemLog::Warning);
  else
    PTrace::SetLevel(0);
  PTrace::ClearOptions(PTrace::Timestamp);
  PTrace::SetOptions(PTrace::DateAndTime);
  PTrace::SetOptions(PTrace::FileAndLine);
#endif

  // Get the HTTP basic authentication info
  PString username = cfg.GetString(UsernameKey);
  PString password = PHTTPPasswordField::Decrypt(cfg.GetString(PasswordKey));

  PHTTPSimpleAuth authority(GetName(), username, password);

  // Create the parameters URL page, and start adding fields to it
  PConfigPage * rsrc = new PConfigPage(*this, "Parameters", "Parameters", authority);

  // HTTP authentication username/password
  rsrc->Add(new PHTTPStringField(UsernameKey, 25, username));
  rsrc->Add(new PHTTPPasswordField(PasswordKey, 25, password));

  // Log level for messages
  rsrc->Add(new PHTTPIntegerField(LogLevelKey,
                                  PSystemLog::Fatal, PSystemLog::NumLogLevels-1,
                                  GetLogLevel(),
                                  "1=Fatal only, 2=Errors, 3=Warnings, 4=Info, 5=Debug"));

  // HTTP Port number to use.
  WORD httpPort = (WORD)cfg.GetInteger(HttpPortKey, DefaultHTTPPort);
  rsrc->Add(new PHTTPIntegerField(HttpPortKey, 1, 32767, httpPort));

  // Initialise the core of the system
  if (!manager.Initialise(cfg, rsrc))
    return PFalse;

  // Finished the resource to add, generate HTML for it and add to name space
  PServiceHTML html("System Parameters");
  rsrc->BuildHTML(html);
  httpNameSpace.AddResource(rsrc, PHTTPSpace::Overwrite);


  // Create the home page
  static const char welcomeHtml[] = "welcome.html";
  if (PFile::Exists(welcomeHtml))
    httpNameSpace.AddResource(new PServiceHTTPFile(welcomeHtml, PTrue), PHTTPSpace::Overwrite);
  else {
    PHTML html;
    html << PHTML::Title("Welcome to " + GetName())
         << PHTML::Body()
         << GetPageGraphic()
         << PHTML::Paragraph() << "<center>"

         << PHTML::HotLink("Parameters") << "Parameters" << PHTML::HotLink()
         << PHTML::Paragraph();

    if (!systemLogFileName && systemLogFileName != "-")
      html << PHTML::HotLink("logfile.txt") << "Full Log File" << PHTML::HotLink()
           << PHTML::BreakLine()
           << PHTML::HotLink("tail_logfile") << "Tail Log File" << PHTML::HotLink()
           << PHTML::Paragraph();
 
    html << PHTML::HRule()
         << GetCopyrightText()
         << PHTML::Body();
    httpNameSpace.AddResource(new PServiceHTTPString("welcome.html", html), PHTTPSpace::Overwrite);
  }

  // set up the HTTP port for listening & start the first HTTP thread
  if (ListenForHTTP(httpPort))
    PSYSTEMLOG(Info, "Opened master socket for HTTP: " << httpListeningSocket->GetPort());
  else {
    PSYSTEMLOG(Fatal, "Cannot run without HTTP port: " << httpListeningSocket->GetErrorText());
    return PFalse;
  }

  PSYSTEMLOG(Info, "Service " << GetName() << ' ' << initMsg);
  return PTrue;
}


void OpalEcho::Main()
{
  Suspend();
}


///////////////////////////////////////////////////////////////

MyManager::MyManager()
{
  sipEP = NULL;
  echoEP = NULL;

#if OPAL_VIDEO
  SetAutoStartReceiveVideo(true);
  SetAutoStartTransmitVideo(true);
#endif
}


MyManager::~MyManager()
{
}


PBoolean MyManager::Initialise(PConfig & cfg, PConfigPage * rsrc)
{
  OpalMediaFormatList allMediaFormats;

  OpalMediaFormat::GetAllRegisteredMediaFormats(allMediaFormats);
  for (PINDEX i = 0; i < allMediaFormats.GetSize(); i++) {
    OpalMediaFormat mediaFormat = allMediaFormats[i];
    if (mediaFormat.GetMediaType() == OpalMediaType::Video()) {
#if 0
      PString sizeStr = "qcif";
      unsigned width, height;
      if (PVideoFrameInfo::ParseSize(sizeStr, width, height)) {
cout << "setting frame size to " << width << "x" << height << endl;
        mediaFormat.SetOptionInteger("Max Rx Frame Width", width);
        mediaFormat.SetOptionInteger("Max Rx Frame Height", height);
      }
    }
    if (args.HasOption("video-rate")) {
      unsigned rate = args.GetOptionString("video-rate").AsUnsigned();
      unsigned frameTime = 90000 / rate;
      mediaFormat.SetOptionInteger(OpalMediaFormat::FrameTimeOption(), frameTime);
    }
    if (args.HasOption("video-bitrate")) {
      unsigned rate = args.GetOptionString("video-bitrate").AsUnsigned();
      mediaFormat.SetOptionInteger(OpalMediaFormat::TargetBitRateOption(), rate);
    }
    if (!rcOption.IsEmpty())
      mediaFormat.SetOptionString(OpalVideoFormat::RateControllerOption(), rcOption);
#endif
    }
    OpalMediaFormat::SetRegisteredMediaFormat(mediaFormat);
  }

  PHTTPFieldArray * fieldArray;

  // Create all the endpoints
  if (sipEP == NULL)
    sipEP = new SIPEndPoint(*this);

  if (echoEP == NULL)
    echoEP = new EchoEndPoint(*this);

  // General parameters for all endpoint types
  fieldArray = new PHTTPFieldArray(new PHTTPStringField(PreferredMediaKey, 25), PTrue);
  PStringArray formats = fieldArray->GetStrings(cfg);
  if (formats.GetSize() > 0)
    SetMediaFormatOrder(formats);
  else
    fieldArray->SetStrings(cfg, GetMediaFormatOrder());
  rsrc->Add(fieldArray);

  fieldArray = new PHTTPFieldArray(new PHTTPStringField(RemovedMediaKey, 25), PTrue);
  SetMediaFormatMask(fieldArray->GetStrings(cfg));
  rsrc->Add(fieldArray);

  SetAudioJitterDelay(cfg.GetInteger(MinJitterKey, GetMinAudioJitterDelay()),
                      cfg.GetInteger(MaxJitterKey, GetMaxAudioJitterDelay()));
  rsrc->Add(new PHTTPIntegerField(MinJitterKey, 20, 2000, GetMinAudioJitterDelay(), "ms"));
  rsrc->Add(new PHTTPIntegerField(MaxJitterKey, 20, 2000, GetMaxAudioJitterDelay(), "ms"));

  SetTCPPorts(cfg.GetInteger(TCPPortBaseKey, GetTCPPortBase()),
              cfg.GetInteger(TCPPortMaxKey, GetTCPPortMax()));
  SetUDPPorts(cfg.GetInteger(UDPPortBaseKey, GetUDPPortBase()),
              cfg.GetInteger(UDPPortMaxKey, GetUDPPortMax()));
  SetRtpIpPorts(cfg.GetInteger(RTPPortBaseKey, GetRtpIpPortBase()),
                cfg.GetInteger(RTPPortMaxKey, GetRtpIpPortMax()));

  rsrc->Add(new PHTTPIntegerField(TCPPortBaseKey, 0, 65535, GetTCPPortBase()));
  rsrc->Add(new PHTTPIntegerField(TCPPortMaxKey,  0, 65535, GetTCPPortMax()));
  rsrc->Add(new PHTTPIntegerField(UDPPortBaseKey, 0, 65535, GetUDPPortBase()));
  rsrc->Add(new PHTTPIntegerField(UDPPortMaxKey,  0, 65535, GetUDPPortMax()));
  rsrc->Add(new PHTTPIntegerField(RTPPortBaseKey, 0, 65535, GetRtpIpPortBase()));
  rsrc->Add(new PHTTPIntegerField(RTPPortMaxKey,  0, 65535, GetRtpIpPortMax()));

  sipEP->StartListener("");

  // Routing
  PStringArray routes;
  routes.AppendString("sip:.* = echo:");

  if (!SetRouteTable(routes)) {
    PSYSTEMLOG(Error, "No legal entries in dial peers!");
  }

  return PTrue;
}

///////////////////////////////////////////////////////////////

OpalLocalConnection * EchoEndPoint::CreateConnection(
      OpalCall & call,    ///<  Owner of connection
      void * userData     ///<  Arbitrary data to pass to connection
)
{
  return new EchoConnection(call, *this, userData);
}


bool EchoEndPoint::OnReadMediaFrame(
  const OpalLocalConnection & connection, ///<  Connection for media
  const OpalMediaStream & mediaStream,    ///<  Media stream data is required for
  RTP_DataFrame & frame                   ///<  RTP frame for data
)
{
  const EchoConnection * conn = dynamic_cast<const EchoConnection *>(&connection);
  if (conn == NULL) 
    return false;
  return ((EchoConnection *)conn)->OnReadMediaFrame(mediaStream, frame);
}

bool EchoEndPoint::OnWriteMediaFrame(
  const OpalLocalConnection & connection, ///<  Connection for media
  const OpalMediaStream & mediaStream,    ///<  Media stream data is required for
  RTP_DataFrame & frame                   ///<  RTP frame for data
)
{
  const EchoConnection * conn = dynamic_cast<const EchoConnection *>(&connection);
  if (conn == NULL) 
    return false;
  return ((EchoConnection *)conn)->OnWriteMediaFrame(mediaStream, frame);
}

///////////////////////////////////////////////////////////////

EchoConnection::EchoConnection(OpalCall & call, EchoEndPoint & endpoint, void * userData)
  : OpalLocalConnection(call, endpoint, userData)
  , m_endpoint(endpoint)
{
}

EchoConnection::~EchoConnection()
{
}


bool EchoConnection::OnReadMediaFrame(
  const OpalMediaStream & mediaStream,    ///<  Media stream data is required for
  RTP_DataFrame & frame                   ///<  RTP frame for data
)
{
  if (!mediaStream.IsOpen())
    return false;

  OpalMediaFormat fmt = mediaStream.GetMediaFormat();
  OpalMediaType mediaType = fmt.GetMediaType();
  MediaMapType::iterator r;

  {
    PWaitAndSignal m(m_mediaMutex);
    r = m_mediaMap.find(mediaType);
    if (r == m_mediaMap.end()) 
      r = m_mediaMap.insert(MediaMapType::value_type(mediaType, MediaInfo())).first;
  }

  MediaInfo & info = r->second;

  info.m_data = &frame;

  info.m_readSync.Signal();

  while (info.m_data != NULL) {
   if (!mediaStream.IsOpen()) {
      PTRACE(1, "Media stream not open");
      return false;
    }
    info.m_writeSync.Wait(100);
  }

  int delay;
  if (fmt.GetMediaType() == OpalMediaType::Audio()) {
    delay = frame.GetPayloadSize() / (fmt.GetClockRate() / 500);
    if (delay == 0)
      delay = 20;
  }
  else if (fmt.GetMediaType() == OpalMediaType::Video())
    delay = 0; // fmt.GetFrameTime() / 90;
  else
    delay = 100;

  if (delay > 0)
    info.m_delay.Delay(delay);

  return true;
}

bool EchoConnection::OnWriteMediaFrame(
  const OpalMediaStream & mediaStream,    ///<  Media stream data is required for
  RTP_DataFrame & frame                   ///<  RTP frame for data
)
{
  OpalMediaFormat fmt = mediaStream.GetMediaFormat();
  OpalMediaType mediaType = fmt.GetMediaType();
  MediaMapType::iterator r;

  {
    PWaitAndSignal m(m_mediaMutex);
    r = m_mediaMap.find(mediaType);
    if (r == m_mediaMap.end()) 
      r = m_mediaMap.insert(MediaMapType::value_type(mediaType, MediaInfo())).first;
  }

  MediaInfo & info = r->second;

  while (info.m_data == NULL) {
    if (!mediaStream.IsOpen())
      return false;
    info.m_readSync.Wait(100);
  }

  *info.m_data = frame;
  info.m_data->MakeUnique();
  info.m_data = NULL;
  info.m_writeSync.Signal();

  return true;
}


// End of File ///////////////////////////////////////////////////////////////
