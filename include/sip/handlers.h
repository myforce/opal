/*
 * handlers.h
 *
 * Session Initiation Protocol non-connection protocol handlers.
 *
 * Open Phone Abstraction Library (OPAL)
 *
 * Copyright (c) 2000 Equivalence Pty. Ltd.
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
 * The Initial Developer of the Original Code is Damien Sandras. 
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#ifndef OPAL_SIP_HANDLERS_H
#define OPAL_SIP_HANDLERS_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#ifndef _PTLIB_H
#include <ptlib.h>
#endif

#include <opal_config.h>

#if OPAL_SIP

#include <opal/pres_ent.h>
#include <sip/sippdu.h>


/// Separate base class to allow searching sorted list
class SIPHandlerBase : public PSafeObject 
{
    PCLASSINFO(SIPHandlerBase, PSafeObject);

  protected:
    SIPHandlerBase(const PString & callID) : m_callID(callID) { }

  public:
    const PString & GetCallID() const
      { return m_callID; }

  protected:
    const PString m_callID;

    // Keep a copy of the keys used for easy removal on destruction
    typedef std::map<PString, PSafePtr<SIPHandler> > IndexMap;
    std::pair<IndexMap::iterator, bool> m_byAorAndPackage;
    std::pair<IndexMap::iterator, bool> m_byAuthIdAndRealm;
    std::pair<IndexMap::iterator, bool> m_byAorUserAndRealm;

  friend class SIPHandlersList;
};


/* Class to handle SIP REGISTER, SUBSCRIBE, MESSAGE, and renew
 * the 'bindings' before they expire.
 */
class SIPHandler : public SIPHandlerBase, public SIPTransactionOwner
{
  PCLASSINFO(SIPHandler, SIPHandlerBase);

protected:
  SIPHandler(
    SIP_PDU::Methods method,
    SIPEndPoint & ep,
    const SIPParameters & params,
    const PString & callID = SIPTransaction::GenerateCallID()
  );

public:
  ~SIPHandler();

  virtual Comparison Compare(const PObject & other) const;

  // Overrides from SIPTransactionOwner
  virtual PString GetAuthID() const        { return m_username; }
  virtual PString GetPassword() const      { return m_password; }

  enum State {
    Subscribed,       // The registration is active
    Subscribing,      // The registration is in process
    Unavailable,      // The registration is offline and still being attempted
    Refreshing,       // The registration is being refreshed
    Restoring,        // The registration is trying to be restored after being offline
    Unsubscribing,    // The unregistration is in process
    Unsubscribed,     // The registrating is inactive
    NumStates
  };

  void SetState (SIPHandler::State s);

  inline SIPHandler::State GetState() const
  { return m_state; }

  virtual const SIPURL & GetAddressOfRecord() const
    { return m_addressOfRecord; }

  virtual PBoolean OnReceivedNOTIFY(SIP_PDU & response);

  virtual void SetExpire(int e);

  virtual int GetExpire() const
    { return m_currentExpireTime; }

  virtual void SetBody(const PString & /*body*/) { }

  virtual bool IsDuplicateCSeq(unsigned ) { return false; }

  virtual SIPTransaction * CreateTransaction(OpalTransport &) { return NULL; }

  SIP_PDU::Methods GetMethod() const { return m_method; }
  virtual SIPSubscribe::EventPackage GetEventPackage() const { return SIPSubscribe::EventPackage(); }

  virtual void OnReceivedResponse(SIPTransaction & transaction, SIP_PDU & response);
  virtual void OnReceivedIntervalTooBrief(SIPTransaction & transaction, SIP_PDU & response);
  virtual void OnReceivedTemporarilyUnavailable(SIPTransaction & transaction, SIP_PDU & response);
  virtual void OnReceivedAuthenticationRequired(SIPTransaction & transaction, SIP_PDU & response);
  virtual void OnReceivedOK(SIPTransaction & transaction, SIP_PDU & response);
  virtual void OnTransactionFailed(SIPTransaction & transaction);

//  virtual void OnFailed(const SIP_PDU & response);
  virtual void OnFailed(SIP_PDU::StatusCodes);
  virtual void SendStatus(SIP_PDU::StatusCodes code, State state);

  bool ActivateState(SIPHandler::State state, bool resetInterface = false);
  virtual bool SendNotify(const PObject * /*body*/) { return false; }

  SIP_PDU::StatusCodes GetLastResponseStatus() const { return m_lastResponseStatus; }

  const OpalProductInfo & GetProductInfo() const { return m_productInfo; }

  const PString & GetRealm() const { return m_realm; }

  virtual bool ShutDown();

protected:
  virtual PBoolean SendRequest(SIPHandler::State state);
  void RetryLater(unsigned after);
  void OnExpireTimeout();
  PDECLARE_WriteConnectCallback(SIPHandler, WriteTransaction);

  PString                     m_username;
  PString                     m_password;
  PString                     m_realm;

  const SIP_PDU::Methods      m_method;
  const SIPURL                m_addressOfRecord;
  SIPMIMEInfo                 m_mime;

  unsigned                    m_lastCseq;
  SIP_PDU::StatusCodes        m_lastResponseStatus;
  int                         m_currentExpireTime;
  int                         m_originalExpireTime;
  int                         m_offlineExpireTime;
  State                       m_state;
  std::queue<State>           m_stateQueue;
  bool                        m_receivedResponse;
  SIPPoolTimer<SIPHandler>    m_expireTimer; 
  OpalProductInfo             m_productInfo;
};

#if PTRACING
ostream & operator<<(ostream & strm, SIPHandler::State state);
#endif


class SIPRegisterHandler : public SIPHandler
{
  PCLASSINFO(SIPRegisterHandler, SIPHandler);

public:
  SIPRegisterHandler(
    SIPEndPoint & ep,
    const SIPRegister::Params & params
  );

  virtual SIPTransaction * CreateTransaction(OpalTransport &);
  virtual void OnReceivedOK(SIPTransaction & transaction, SIP_PDU & response);

  void UpdateParameters(const SIPRegister::Params & params);

  const SIPRegister::Params & GetParams() const { return m_parameters; }

  const SIPURLList & GetContacts() const { return m_contactAddresses; }
  const SIPURLList & GetServiceRoute() const { return m_serviceRoute; }

protected:
  virtual PBoolean SendRequest(SIPHandler::State state);
  virtual void SendStatus(SIP_PDU::StatusCodes code, State state);
  PString CreateRegisterContact(const OpalTransportAddress & address, int q);

  SIPRegister::Params  m_parameters;
  unsigned             m_sequenceNumber;
  SIPURLList           m_contactAddresses;
  SIPURLList           m_serviceRoute;
  OpalTransportAddress m_externalAddress;
  uint32_t             m_rfc5626_reg_id;
};


class SIPSubscribeHandler : public SIPHandler
{
  PCLASSINFO(SIPSubscribeHandler, SIPHandler);
public:
  SIPSubscribeHandler(SIPEndPoint & ep, const SIPSubscribe::Params & params);
  ~SIPSubscribeHandler();

  virtual SIPTransaction * CreateTransaction (OpalTransport &);
  virtual void OnReceivedOK(SIPTransaction & transaction, SIP_PDU & response);
  virtual PBoolean OnReceivedNOTIFY(SIP_PDU & response);
  virtual void OnFailed(SIP_PDU::StatusCodes);
  virtual SIPEventPackage GetEventPackage() const
    { return m_parameters.m_eventPackage; }

  void UpdateParameters(const SIPSubscribe::Params & params);

  const SIPSubscribe::Params & GetParams() const { return m_parameters; }

protected:
  virtual void WriteTransaction(OpalTransport & transport, bool & succeeded);
  virtual void SendStatus(SIP_PDU::StatusCodes code, State state);
  bool DispatchNOTIFY(SIP_PDU & request, SIP_PDU & response);

  SIPSubscribe::Params     m_parameters;
  bool                     m_unconfirmed;
  SIPEventPackageHandler * m_packageHandler;
};


class SIPNotifyHandler : public SIPHandler
{
  PCLASSINFO(SIPNotifyHandler, SIPHandler);
public:
  SIPNotifyHandler(
    SIPEndPoint & ep,
    const SIPEventPackage & eventPackage,
    const SIPDialogContext & dialog
  );
  ~SIPNotifyHandler();

  virtual SIPTransaction * CreateTransaction(OpalTransport &);
  virtual SIPEventPackage GetEventPackage() const
    { return m_eventPackage; }

  virtual void SetBody(const PString & body) { m_body = body; }

  virtual bool SendNotify(const PObject * body);

  enum Reasons {
    Deactivated,
    Probation,
    Rejected,
    Timeout,
    GiveUp,
    NoResource
  };

protected:
  virtual PBoolean SendRequest(SIPHandler::State state);
  virtual void WriteTransaction(OpalTransport & transport, bool & succeeded);

  SIPEventPackage          m_eventPackage;
  Reasons                  m_reason;
  SIPEventPackageHandler * m_packageHandler;
  PString                  m_body;
};


class SIPPublishHandler : public SIPHandler
{
  PCLASSINFO(SIPPublishHandler, SIPHandler);

public:
  SIPPublishHandler(SIPEndPoint & ep, 
                    const SIPSubscribe::Params & params,
                    const PString & body);

  virtual void SetBody(const PString & body) { m_body = body; }

  virtual SIPTransaction * CreateTransaction(OpalTransport &);
  virtual void OnReceivedOK(SIPTransaction & transaction, SIP_PDU & response);
  virtual SIPEventPackage GetEventPackage() const { return m_parameters.m_eventPackage; }

protected:
  SIPSubscribe::Params m_parameters;
  PString              m_body;
  PString              m_sipETag;
};


class SIPMessageHandler : public SIPHandler
{
  PCLASSINFO(SIPMessageHandler, SIPHandler);
public:
  SIPMessageHandler(SIPEndPoint & ep, const SIPMessage::Params & params);

  virtual SIPTransaction * CreateTransaction (OpalTransport &);
  virtual void OnFailed(SIP_PDU::StatusCodes);
  virtual void OnReceivedOK(SIPTransaction & transaction, SIP_PDU & response);

  void UpdateParameters(const SIPMessage::Params & params);

protected:
  SIPMessage::Params m_parameters;
  bool               m_messageSent;
};


class SIPPingHandler : public SIPHandler
{
  PCLASSINFO(SIPPingHandler, SIPHandler);
public:
  SIPPingHandler(SIPEndPoint & ep, const PURL & to);
  virtual SIPTransaction * CreateTransaction (OpalTransport &);
};


/** This dictionary is used both to contain the active and successful
 * registrations, and subscriptions. 
 */
class SIPHandlersList
{
  public:
    /** Append a new handler to the list
      */
    void Append(SIPHandler * handler);

    /** Remove a handler from the list.
        Handler is not immediately deleted but marked for deletion later by
        DeleteObjectsToBeRemoved() when all references are done with the handler.
      */
    void Remove(SIPHandler * handler);

    /** Update indexes for handler in the list
      */
    void Update(SIPHandler * handler);

    /** Clean up lists of handler.
      */
    bool DeleteObjectsToBeRemoved()
      { return m_handlersList.DeleteObjectsToBeRemoved(); }

    /**
     * Return the number of registered accounts
     */
    unsigned GetCount(SIP_PDU::Methods meth, const PString & eventPackage = PString::Empty()) const;

    /**
     * Return a list of the active address of records for each handler.
     */
    PStringList GetAddresses(bool includeOffline, SIP_PDU::Methods meth, const PString & eventPackage = PString::Empty()) const;

    /** Get the first handler in the list. Further enumeration may be done by
        the ++operator on the safe pointer.
     */
    PSafePtr<SIPHandler> GetFirstHandler(PSafetyMode mode = PSafeReference) const
      { return PSafePtr<SIPHandler>(m_handlersList, mode); }

    /** Get the first handler in the list for the specified method. Further enumeration may be done by
        the ++operator on the safe pointer.
     */
    PSafePtr<SIPHandler> FindFirstHandler(SIP_PDU::Methods meth, PSafetyMode mode = PSafeReference) const;

    /**
     * Find the SIPHandler object with the specified callID
     */
    PSafePtr<SIPHandler> FindSIPHandlerByCallID(const PString & callID, PSafetyMode m);

    /**
     * Find the SIPHandler object with the specified authRealm
     */
    PSafePtr<SIPHandler> FindSIPHandlerByAuthRealm(const PString & authRealm, PSafetyMode m);

    /**
     * Find the SIPHandler object with the specified authRealm & user
     */
    PSafePtr<SIPHandler> FindSIPHandlerByAuthRealm(const PString & authRealm, const PString & userName, PSafetyMode m);

    /**
     * Find the SIPHandler object with the specified URL. The url is
     * the registration address, for example, 6001@sip.seconix.com
     * when registering 6001 to sip.seconix.com with realm seconix.com
     * or 6001@seconix.com when registering 6001@seconix.com to
     * sip.seconix.com
     */
    PSafePtr<SIPHandler> FindSIPHandlerByUrl(const PString & url, SIP_PDU::Methods meth, PSafetyMode m);
    PSafePtr<SIPHandler> FindSIPHandlerByUrl(const PString & url, SIP_PDU::Methods meth, const PString & eventPackage, PSafetyMode m);

    /**
     * Find the SIPHandler object with the specified registration host.
     * For example, in the above case, the name parameter
     * could be "sip.seconix.com" or "seconix.com".
     */
    PSafePtr <SIPHandler> FindSIPHandlerByDomain(const PString & name, SIP_PDU::Methods meth, PSafetyMode m);

  protected:
    void RemoveIndexes(SIPHandler * handler);

    PSafeSortedList<SIPHandlerBase> m_handlersList;

    typedef SIPHandler::IndexMap IndexMap;
    PSafePtr<SIPHandler> FindBy(IndexMap & by, const PString & key, PSafetyMode m);

    IndexMap m_byAorAndPackage;
    IndexMap m_byAuthIdAndRealm;
    IndexMap m_byAorUserAndRealm;
};


#if OPAL_SIP_PRESENCE

/** Information for SIP "presence" event package notification messages.
  */
class SIPPresenceInfo : public OpalPresenceInfo
{
  PCLASSINFO_WITH_CLONE(SIPPresenceInfo, OpalPresenceInfo)

public:
  // presence agent
  PString m_presenceAgent;
  PString m_personId;

  PString AsXML() const;

  static bool ParseNotify(
    SIPSubscribe::NotifyCallbackInfo & notifyInfo,
    list<SIPPresenceInfo> & info
  );
  static bool ParseXML(
    const PXML & xml,
    list<SIPPresenceInfo> & info
  );

  void PrintOn(ostream & strm) const;
  friend ostream & operator<<(ostream & strm, const SIPPresenceInfo & info) { info.PrintOn(strm); return strm; }

  // Constructor
  SIPPresenceInfo(
    State state = Unchanged
  ) : OpalPresenceInfo(state) { }
  SIPPresenceInfo(
    SIP_PDU::StatusCodes status,
    bool subscribing
  );
  SIPPresenceInfo(
    const OpalPresenceInfo & info
  ) : OpalPresenceInfo(info) { }
};
#endif // OPAL_SIP_PRESENCE


/** Information for SIP "dialog" event package notification messages.
  */
struct SIPDialogNotification : public PObject
{
  PCLASSINFO(SIPDialogNotification, PObject);

  P_DECLARE_ENUM(States,
    Terminated,
    Trying,
    Proceeding,
    Early,
    Confirmed
  );
  static PString GetStateName(States state);
  PString GetStateName() const { return GetStateName(m_state); }

  P_DECLARE_ENUM_EX(Events, NumEvents,
    NoEvent, -1,
    Cancelled,
    Rejected,
    Replaced,
    LocalBye,
    RemoteBye,
    Error,
    Timeout
  );
  static PString GetEventName(Events state);
  PString GetEventName() const { return GetEventName(m_eventType); }

  enum Rendering {
    RenderingUnknown = -1,
    NotRenderingMedia,
    RenderingMedia
  };

  PString  m_entity;
  PString  m_dialogId;
  PString  m_callId;
  bool     m_initiator;
  States   m_state;
  Events   m_eventType;
  unsigned m_eventCode;
  struct Participant {
    Participant() : m_appearance(-1), m_byeless(false), m_rendering(RenderingUnknown) { }
    PString   m_URI;
    PString   m_dialogTag;
    PString   m_identity;
    PString   m_display;
    int       m_appearance;
    bool      m_byeless;
    Rendering m_rendering;
  } m_local, m_remote;

  SIPDialogNotification(const PString & entity = PString::Empty());

  void PrintOn(ostream & strm) const;
};


/** Information for SIP "reg" event package (RFC3680) notification messages.
  */
class SIPRegNotification : public PObject
{
  PCLASSINFO(SIPRegNotification, PObject)
public:
  P_DECLARE_ENUM(States,
    Initial,
    Active,
    Terminated
  );
  static PString GetStateName(States state);
  PString GetStateName() const { return GetStateName(m_state); }
  static States GetStateFromName(const PCaselessString & str);

  SIPRegNotification(
    const SIPURL & aor = SIPURL(),
    States state = Initial
  );

  SIPURL            m_aor;
  States            m_state;
  std::list<SIPURL> m_contacts;

  void PrintOn(ostream & strm) const;
};


#endif // OPAL_SIP

#endif // OPAL_SIP_HANDLERS_H
