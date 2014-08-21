/*
 * main.h
 *
 * OPAL Server main header
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

#ifndef _OPALSRV_MAIN_H
#define _OPALSRV_MAIN_H

#include "precompile.h" // Really only here as Visual Studio editor syntax checking gets confused

class MyManager;


///////////////////////////////////////

class CallDetailRecord
{
public:
  CallDetailRecord();


  P_DECLARE_ENUM(FieldCodes,
    CallId,
    StartTime,
    ConnectTime,
    EndTime,
    CallState,
    CallResult, // EndedByXXX text
    OriginatorID,
    OriginatorURI,
    OriginatorSignalAddress,   // ip4:port or [ipv6]:port form
    DialedNumber,
    DestinationID,
    DestinationURI,
    DestinationSignalAddress,   // ip4:port or [ipv6]:port form
    AudioCodec,
    AudioOriginatorMediaAddress,   // ip4:port or [ipv6]:port form
    AudioDestinationMediaAddress,   // ip4:port or [ipv6]:port form
    VideoCodec,
    VideoOriginatorMediaAddress,   // ip4:port or [ipv6]:port form
    VideoDestinationMediaAddress,   // ip4:port or [ipv6]:port form
    Bandwidth
  );

  P_DECLARE_ENUM(ActiveCallStates,
    CallCompleted,
    CallSetUp,
    CallProceeding,
    CallAlerting,
    CallConnected,
    CallEstablished
  );

  struct Media
  {
    Media() : m_closed(false) { }

    OpalMediaFormat         m_Codec;
    PIPSocketAddressAndPort m_OriginatorAddress;
    PIPSocketAddressAndPort m_DestinationAddress;
    bool                    m_closed;
  };
  typedef std::map<PString, Media> MediaMap;


  Media GetAudio() const { return GetMedia(OpalMediaType::Audio()); }
#if OPAL_VIDEO
  Media GetVideo() const { return GetMedia(OpalMediaType::Video()); }
#endif
  Media GetMedia(const OpalMediaType & mediaType) const;
  PString ListMedia() const;

  PString GetGUID() const { return m_GUID.AsString(); }
  PString GetCallState() const;

  void OutputText(ostream & strm, const PString & format) const;
#if P_ODBC
  void OutputSQL(PODBC::Row & row, PString const mapping[NumFieldCodes]) const;
#endif
  void OutputSummaryHTML(PHTML & html) const;
  void OutputDetailedHTML(PHTML & html) const;

protected:
  PGloballyUniqueID             m_GUID;
  PTime                         m_StartTime;
  PTime                         m_ConnectTime;
  PTime                         m_EndTime;
  ActiveCallStates              m_CallState;
  OpalConnection::CallEndReason m_CallResult;
  PString                       m_OriginatorID;
  PString                       m_OriginatorURI;
  PIPSocketAddressAndPort       m_OriginatorSignalAddress;
  PString                       m_DialedNumber;
  PString                       m_DestinationID;
  PString                       m_DestinationURI;
  PIPSocketAddressAndPort       m_DestinationSignalAddress;
  uint64_t                      m_Bandwidth;
  MediaMap                      m_media;
};


///////////////////////////////////////

class MyCall : public OpalCall, public CallDetailRecord
{
  PCLASSINFO(MyCall, OpalCall);
public:
  MyCall(MyManager & manager);

  // Callbacks from OPAL
  virtual PBoolean OnSetUp(OpalConnection & connection);
  virtual void OnProceeding(OpalConnection & connection);
  virtual PBoolean OnAlerting(OpalConnection & connection);
  virtual PBoolean OnConnected(OpalConnection & connection);
  virtual void OnEstablishedCall();
  virtual void OnCleared();

  // Called by MyManager
  void OnStartMediaPatch(OpalConnection & connection, OpalMediaPatch & patch);
  void OnStopMediaPatch(OpalMediaPatch & patch);

protected:
  MyManager & m_manager;
};


///////////////////////////////////////

#if OPAL_H323

class MyGatekeeperServer;

class MyGatekeeperCall : public H323GatekeeperCall
{
    PCLASSINFO(MyGatekeeperCall, H323GatekeeperCall);
  public:
    MyGatekeeperCall(
      MyGatekeeperServer & server,
      const OpalGloballyUniqueID & callIdentifier, /// Unique call identifier
      Direction direction
    );
    ~MyGatekeeperCall();

    virtual H323GatekeeperRequest::Response OnAdmission(
      H323GatekeeperARQ & request
    );

#ifdef H323_TRANSNEXUS_OSP
    PBoolean AuthoriseOSPCall(H323GatekeeperARQ & info);
    OpalOSP::Transaction * ospTransaction;
#endif
};


///////////////////////////////////////

class MyGatekeeperServer : public H323GatekeeperServer
{
    PCLASSINFO(MyGatekeeperServer, H323GatekeeperServer);
  public:
    MyGatekeeperServer(H323EndPoint & ep);

    // Overrides
    virtual H323GatekeeperCall * CreateCall(
      const OpalGloballyUniqueID & callIdentifier,
      H323GatekeeperCall::Direction direction
    );
    virtual PBoolean TranslateAliasAddress(
      const H225_AliasAddress & alias,
      H225_ArrayOf_AliasAddress & aliases,
      H323TransportAddress & address,
      PBoolean & isGkRouted,
      H323GatekeeperCall * call
    );

    // new functions
    bool Configure(PConfig & cfg, PConfigPage * rsrc);
    PString OnLoadEndPointStatus(const PString & htmlBlock);

#ifdef H323_TRANSNEXUS_OSP
    OpalOSP::Provider * GetOSPProvider() const
    { return ospProvider; }
#endif

  private:
    H323EndPoint & endpoint;

    class RouteMap : public PObject {
        PCLASSINFO(RouteMap, PObject);
      public:
        RouteMap(
          const PString & alias,
          const PString & host
        );
        RouteMap(
          const RouteMap & map
        ) : PObject(map), alias(map.alias), regex(map.alias), host(map.host) { }

        void PrintOn(
          ostream & strm
        ) const;

        bool IsValid() const;

        bool IsMatch(
          const PString & alias
        ) const;

        const H323TransportAddress & GetHost() const { return host; }

      private:
        PString              alias;
        PRegularExpression   regex;
        H323TransportAddress host;
    };
    PList<RouteMap> routes;

    PMutex reconfigurationMutex;

#ifdef H323_TRANSNEXUS_OSP
    OpalOSP::Provider * ospProvider;
    PString ospRoutingURL;
    PString ospPrivateKeyFileName;
    PString ospPublicKeyFileName;
    PString ospServerKeyFileName;
#endif
};


///////////////////////////////////////

class MyH323EndPoint : public H323ConsoleEndPoint
{
    PCLASSINFO(MyH323EndPoint, H323ConsoleEndPoint);
  public:
    MyH323EndPoint(MyManager & mgr);

    bool Configure(PConfig & cfg, PConfigPage * rsrc);

    void AutoRegister(const PString & alias, bool registering);

    const MyGatekeeperServer & GetGatekeeperServer() const { return m_gkServer; }
          MyGatekeeperServer & GetGatekeeperServer()       { return m_gkServer; }

  protected:
    MyManager        & m_manager;
    MyGatekeeperServer m_gkServer;
};

#endif // OPAL_H323


///////////////////////////////////////

#if OPAL_SIP

class MySIPEndPoint : public SIPConsoleEndPoint
{
  PCLASSINFO(MySIPEndPoint, SIPConsoleEndPoint);
public:
  MySIPEndPoint(MyManager & mgr);

  bool Configure(PConfig & cfg, PConfigPage * rsrc);

#if OPAL_H323 || OPAL_SKINNY
  virtual void OnChangedRegistrarAoR(RegistrarAoR & ua);
#endif

#if OPAL_H323
  bool m_autoRegisterH323;
#endif
#if OPAL_SKINNY
  bool m_autoRegisterSkinny;
#endif

protected:
  MyManager & m_manager;
};

#endif // OPAL_SIP


///////////////////////////////////////

#if OPAL_SKINNY

class MySkinnyEndPoint : public OpalConsoleSkinnyEndPoint
{
  PCLASSINFO(MySkinnyEndPoint, OpalConsoleSkinnyEndPoint);
public:
  MySkinnyEndPoint(MyManager & mgr);

  bool Configure(PConfig & cfg, PConfigPage * rsrc);

  void AutoRegister(const PString & server, const PString & name, bool registering);
  void ExpandWildcards(const PStringArray & input, PStringArray & names, PStringArray & servers);

protected:
  MyManager  & m_manager;
  PString      m_defaultServer;
  unsigned     m_deviceType;
  PStringArray m_deviceNames;
};

#endif // OPAL_SKINNY


///////////////////////////////////////

class BaseStatusPage : public PServiceHTTPString
{
    PCLASSINFO(BaseStatusPage, PServiceHTTPString);
  public:
    BaseStatusPage(MyManager & mgr, const PHTTPAuthority & auth, const char * name);

    virtual PString LoadText(
      PHTTPRequest & request    // Information on this request.
      );

    virtual PBoolean Post(
      PHTTPRequest & request,
      const PStringToString &,
      PHTML & msg
      );

    MyManager & m_manager;

  protected:
    virtual const char * GetTitle() const = 0;
    virtual void CreateHTML(PHTML & html, const PStringToString & query);
    virtual void CreateContent(PHTML & html, const PStringToString & query) const = 0;
    virtual bool OnPostControl(const PStringToString & /*data*/, PHTML & /*msg*/) { return false; }

    unsigned m_refreshRate;
};


///////////////////////////////////////

class ClearLogPage : public BaseStatusPage
{
  PCLASSINFO(ClearLogPage, BaseStatusPage);
public:
  ClearLogPage(MyManager & mgr, const PHTTPAuthority & auth);

protected:
  virtual const char * GetTitle() const;
  virtual void CreateContent(PHTML & html, const PStringToString & query) const;
  virtual bool OnPostControl(const PStringToString & data, PHTML & msg);
};


///////////////////////////////////////

class RegistrationStatusPage : public BaseStatusPage
{
    PCLASSINFO(RegistrationStatusPage, BaseStatusPage);
  public:
    RegistrationStatusPage(MyManager & mgr, const PHTTPAuthority & auth);

    typedef map<PString, PString> StatusMap;
#if OPAL_H323
    const StatusMap & GetH323() const { return m_h323; }
#endif
#if OPAL_SIP
    const StatusMap & GetSIP() const { return m_sip; }
#endif
#if OPAL_SKINNY
    const StatusMap & GetSkinny() const { return m_skinny; }
#endif

  protected:
    virtual PString LoadText(PHTTPRequest & request);
    virtual const char * GetTitle() const;
    virtual void CreateContent(PHTML & html, const PStringToString & query) const;

#if OPAL_H323
    StatusMap m_h323;
#endif
#if OPAL_SIP
    StatusMap m_sip;
#endif
#if OPAL_SKINNY
    StatusMap m_skinny;
#endif
};


///////////////////////////////////////

class CallStatusPage : public BaseStatusPage
{
    PCLASSINFO(CallStatusPage, BaseStatusPage);
  public:
    CallStatusPage(MyManager & mgr, const PHTTPAuthority & auth);

    const PArray<PString> & GetCalls() const { return m_calls; }

  protected:
    virtual PString LoadText(PHTTPRequest & request);
    virtual const char * GetTitle() const;
    virtual void CreateContent(PHTML & html, const PStringToString & query) const;
    virtual bool OnPostControl(const PStringToString & data, PHTML & msg);

    PArray<PString> m_calls;
};


///////////////////////////////////////

#if OPAL_H323

class GkStatusPage : public BaseStatusPage
{
    PCLASSINFO(GkStatusPage, BaseStatusPage);
  public:
    GkStatusPage(MyManager & mgr, const PHTTPAuthority & auth);

  protected:
    virtual const char * GetTitle() const;
    virtual void CreateContent(PHTML & html, const PStringToString & query) const;
    virtual bool OnPostControl(const PStringToString & data, PHTML & msg);

    MyGatekeeperServer & m_gkServer;

    friend class PServiceMacro_EndPointStatus;
};

#endif // OPAL_H323


///////////////////////////////////////

class CDRListPage : public BaseStatusPage
{
  PCLASSINFO(CDRListPage, BaseStatusPage);
public:
  CDRListPage(MyManager & mgr, const PHTTPAuthority & auth);
protected:
  virtual const char * GetTitle() const;
  virtual void CreateContent(PHTML & html, const PStringToString & query) const;
};


///////////////////////////////////////

class CDRPage : public BaseStatusPage
{
    PCLASSINFO(CDRPage, BaseStatusPage);
  public:
    CDRPage(MyManager & mgr, const PHTTPAuthority & auth);
  protected:
    virtual const char * GetTitle() const;
    virtual void CreateContent(PHTML & html, const PStringToString & query) const;
};


///////////////////////////////////////

#if P_CLI
  typedef OpalManagerCLI MyManagerParent;
#else
  typedef OpalConsoleManager MyManagerParent;
#endif

class MyManager : public MyManagerParent
{
    PCLASSINFO(MyManager, MyManagerParent);
  public:
    MyManager();
    ~MyManager();

    virtual void EndRun(bool interrupt);

    bool Configure(PConfig & cfg, PConfigPage * rsrc);
    bool ConfigureCDR(PConfig & cfg, PConfigPage * rsrc);

    bool ConfigureCommon(
      OpalEndPoint * ep,
      const PString & cfgPrefix,
      PConfig & cfg,
      PConfigPage * rsrc
    );

    virtual OpalCall * CreateCall(void *);

    virtual MediaTransferMode GetMediaTransferMode(
      const OpalConnection & source,      ///<  Source connection
      const OpalConnection & destination, ///<  Destination connection
      const OpalMediaType & mediaType     ///<  Media type for session
    ) const;
    virtual void OnStartMediaPatch(
      OpalConnection & connection,  ///< Connection patch is in
      OpalMediaPatch & patch        ///< Media patch being started
    );
    virtual void OnStopMediaPatch(
      OpalConnection & connection,
      OpalMediaPatch & patch
    );


    PString OnLoadCallStatus(const PString & htmlBlock);

#if OPAL_SIP && (OPAL_H323 || OPAL_SKINNY)
    void OnChangedRegistrarAoR(const PURL & aor, bool registering);
#endif

#if OPAL_H323
    virtual H323ConsoleEndPoint * CreateH323EndPoint();
    MyH323EndPoint & GetH323EndPoint() const { return *FindEndPointAs<MyH323EndPoint>(OPAL_PREFIX_H323); }
#endif
#if OPAL_SIP
    virtual SIPConsoleEndPoint * CreateSIPEndPoint();
    MySIPEndPoint & GetSIPEndPoint() const { return *FindEndPointAs<MySIPEndPoint>(OPAL_PREFIX_SIP); }
#endif
#if OPAL_SKINNY
    virtual OpalConsoleSkinnyEndPoint * CreateSkinnyEndPoint();
    MySkinnyEndPoint & GetSkinnyEndPoint() const { return *FindEndPointAs<MySkinnyEndPoint>(OPAL_PREFIX_SKINNY); }
#endif

    void DropCDR(const MyCall & call, bool final);

    typedef std::list<CallDetailRecord> CDRList;
    CDRList::const_iterator BeginCDR();
    bool NotEndCDR(const CDRList::const_iterator & it);
    bool FindCDR(const PString & guid, CallDetailRecord & cdr);

  protected:
    PSystemLog m_systemLog;

    MediaTransferMode m_mediaTransferMode;

#if OPAL_CAPI
    bool m_enableCAPI;
#endif

#if OPAL_SCRIPT
    PString m_scriptLanguage;
    PString m_scriptText;
#endif

    CDRList   m_cdrList;
    size_t    m_cdrListMax;
    PTextFile m_cdrTextFile;
    PString   m_cdrFormat;
    PString   m_cdrTable;
    PString   m_cdrFieldNames[MyCall::NumFieldCodes];
#if P_ODBC
    PODBC     m_odbc;
#endif
    PMutex    m_cdrMutex;
};


///////////////////////////////////////

class MyProcess : public MyProcessAncestor
{
    PCLASSINFO(MyProcess, MyProcessAncestor)
  public:
    MyProcess();
    ~MyProcess();
    virtual PBoolean OnStart();
    virtual void OnStop();
    virtual void OnControl();
    virtual void OnConfigChanged();
    virtual PBoolean Initialise(const char * initMsg);

  protected:
    MyManager * m_manager;
};


#endif  // _OPALSRV_MAIN_H


// End of File ///////////////////////////////////////////////////////////////
