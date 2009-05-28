/*
 * handlers.h
 *
 * Session Initiation Protocol endpoint.
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

#include <opal/buildopts.h>

#if OPAL_SIP

#include <ptlib/safecoll.h>

#include <opal/connection.h>
#include <sip/sippdu.h>


/* Class to handle SIP REGISTER, SUBSCRIBE, MESSAGE, and renew
 * the 'bindings' before they expire.
 */
class SIPHandler : public PSafeObject 
{
  PCLASSINFO(SIPHandler, PSafeObject);

protected:
  SIPHandler(
    SIPEndPoint & ep, 
    const PString & target,
    const PString & remote,
    int expireTime = 0,
    int offlineExpire = 30,
    const PTimeInterval & retryMin = PMaxTimeInterval,
    const PTimeInterval & retryMax = PMaxTimeInterval
  );

public:
  ~SIPHandler();

  virtual bool ShutDown();

  enum State {
    Subscribed,       // The registration is active
    Subscribing,      // The registration is in process
    Unavailable,      // The registration is offline and still being attempted
    Refreshing,       // The registration is being refreshed
    Restoring,        // The registration is trying to be restored after being offline
    Unsubscribing,    // The unregistration is in process
    Unsubscribed      // The registrating is inactive
  };

  void SetState (SIPHandler::State s);

  inline SIPHandler::State GetState () 
  { return state; }

  virtual OpalTransport * GetTransport();

  virtual SIPAuthentication * GetAuthentication()
  { return authentication; }

  virtual const SIPURL & GetAddressOfRecord()
    { return m_addressOfRecord; }

  virtual PBoolean OnReceivedNOTIFY(SIP_PDU & response);

  virtual void SetExpire(int e);

  virtual int GetExpire()
    { return expire; }

  virtual PString GetCallID()
    { return callID; }

  virtual void SetBody(const PString & b)
    { body = b;}

  virtual bool IsDuplicateCSeq(unsigned ) { return false; }

  virtual SIPTransaction * CreateTransaction(OpalTransport & t) = 0;

  virtual SIP_PDU::Methods GetMethod() = 0;
  virtual SIPSubscribe::EventPackage GetEventPackage() const
  { return PString::Empty(); }

  virtual void OnReceivedAuthenticationRequired(SIPTransaction & transaction, SIP_PDU & response);
  virtual void OnReceivedOK(SIPTransaction & transaction, SIP_PDU & response);
  virtual void OnTransactionFailed(SIPTransaction & transaction);
  virtual void OnFailed(SIP_PDU::StatusCodes);

  virtual PBoolean SendRequest(SIPHandler::State state);
  virtual bool SendNotify(const PObject * /*body*/) { return false; }

  SIPEndPoint & GetEndPoint() const { return endpoint; }

  const OpalProductInfo & GetProductInfo() const { return m_productInfo; }

  const PString & GetUsername() const     { return m_username; }
  const PString & GetPassword() const     { return m_password; }
  const PString & GetRealm() const        { return m_realm; }
  const SIPURL & GetRemoteAddress() const { return m_remoteAddress; }

protected:
  void CollapseFork(SIPTransaction & transaction);
  PDECLARE_NOTIFIER(PTimer, SIPHandler, OnExpireTimeout);
  static PBoolean WriteSIPHandler(OpalTransport & transport, void * info);
  bool WriteSIPHandler(OpalTransport & transport);

  SIPEndPoint               & endpoint;

  SIPAuthentication         * authentication;
  PString                     m_username;
  PString                     m_password;
  PString                     m_realm;

  PSafeList<SIPTransaction>   transactions;
  OpalTransport             * m_transport;
  SIPURL                      m_addressOfRecord;
  SIPURL                      m_remoteAddress;
  PString                     callID;
  int                         expire;
  int                         originalExpire;
  int                         offlineExpire;
  PString                     body;
  unsigned                    authenticationAttempts;
  State                       state;
  PTimer                      expireTimer; 
  PTimeInterval               retryTimeoutMin; 
  PTimeInterval               retryTimeoutMax; 
  SIPURL                      m_proxy;
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

  ~SIPRegisterHandler();

  virtual SIPTransaction * CreateTransaction(OpalTransport &);
  virtual void OnReceivedOK(SIPTransaction & transaction, SIP_PDU & response);
  virtual SIP_PDU::Methods GetMethod()
    { return SIP_PDU::Method_REGISTER; }

  virtual void OnFailed(SIP_PDU::StatusCodes r);
  virtual PBoolean SendRequest(SIPHandler::State state);

  void UpdateParameters(const SIPRegister::Params & params);

private:
  void SendStatus(SIP_PDU::StatusCodes code, State state);

  SIPRegister::Params m_parameters;
  unsigned            m_sequenceNumber;
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
  virtual void OnFailed(SIP_PDU::StatusCodes r);
  virtual PBoolean SendRequest(SIPHandler::State state);
  virtual SIP_PDU::Methods GetMethod ()
    { return SIP_PDU::Method_SUBSCRIBE; }
  virtual SIPEventPackage GetEventPackage() const
    { return m_parameters.m_eventPackage; }

  void UpdateParameters(const SIPSubscribe::Params & params);

  virtual bool IsDuplicateCSeq(unsigned sequenceNumber) { return m_dialog.IsDuplicateCSeq(sequenceNumber); }

private:
  void SendStatus(SIP_PDU::StatusCodes code, State state);

  SIPSubscribe::Params     m_parameters;
  SIPDialogContext         m_dialog;
  bool                     m_unconfirmed;
  SIPEventPackageHandler * m_packageHandler;
};


class SIPNotifyHandler : public SIPHandler
{
  PCLASSINFO(SIPNotifyHandler, SIPHandler);
public:
  SIPNotifyHandler(
    SIPEndPoint & ep,
    const PString & targetAddress,
    const SIPEventPackage & eventPackage,
    const SIPDialogContext & dialog
  );
  ~SIPNotifyHandler();

  virtual SIPTransaction * CreateTransaction(OpalTransport &);
  virtual PBoolean SendRequest(SIPHandler::State state);
  virtual SIP_PDU::Methods GetMethod ()
    { return SIP_PDU::Method_NOTIFY; }
  virtual SIPEventPackage GetEventPackage() const
    { return m_eventPackage; }

  virtual bool IsDuplicateCSeq(unsigned sequenceNumber) { return m_dialog.IsDuplicateCSeq(sequenceNumber); }
  virtual bool SendNotify(const PObject * body);

  enum Reasons {
    Deactivated,
    Probation,
    Rejected,
    Timeout,
    GiveUp,
    NoResource
  };

private:
  SIPEventPackage          m_eventPackage;
  SIPDialogContext         m_dialog;
  Reasons                  m_reason;
  SIPEventPackageHandler * m_packageHandler;
};


class SIPPublishHandler : public SIPHandler
{
  PCLASSINFO(SIPPublishHandler, SIPHandler);

public:
  SIPPublishHandler(SIPEndPoint & ep, 
                    const SIPSubscribe::Params & params,
                    const PString & body);
  ~SIPPublishHandler();

  virtual SIPTransaction * CreateTransaction(OpalTransport &);
  virtual void OnReceivedOK(SIPTransaction & transaction, SIP_PDU & response);
  virtual SIP_PDU::Methods GetMethod()
    { return SIP_PDU::Method_PUBLISH; }
  virtual SIPEventPackage GetEventPackage() const
    { return m_parameters.m_eventPackage; }
  virtual void SetBody(const PString & body);

private:
  SIPSubscribe::Params m_parameters;
  PString              m_sipETag;
  bool                 m_stateChanged;
};


class SIPMessageHandler : public SIPHandler
{
  PCLASSINFO(SIPMessageHandler, SIPHandler);
public:
  SIPMessageHandler(SIPEndPoint & ep, 
                    const PString & to,
                    const PString & body,
                    const PString & remoteContact,
                    const PString & callId);
  ~SIPMessageHandler();

  virtual SIPTransaction * CreateTransaction (OpalTransport &);
  virtual SIP_PDU::Methods GetMethod ()
    { return SIP_PDU::Method_MESSAGE; }
  virtual void OnFailed (SIP_PDU::StatusCodes);
  virtual void SetBody(const PString & b);

private:
  virtual void OnExpireTimeout(PTimer &, INT);
};


class SIPPingHandler : public SIPHandler
{
  PCLASSINFO(SIPPingHandler, SIPHandler);
public:
  SIPPingHandler(SIPEndPoint & ep, 
                 const PString & to);
  virtual SIPTransaction * CreateTransaction (OpalTransport &);
  virtual SIP_PDU::Methods GetMethod ()
    { return SIP_PDU::Method_MESSAGE; }
};


/** This dictionary is used both to contain the active and successful
 * registrations, and subscriptions. 
 */
class SIPHandlersList : public PSafeList<SIPHandler>
{
  public:
    /**
     * Return the number of registered accounts
     */
    unsigned GetCount(SIP_PDU::Methods meth, const PString & eventPackage = PString::Empty()) const;

    /**
     * Return a list of the active address of records for each handler.
     */
    PStringList GetAddresses(bool includeOffline, SIP_PDU::Methods meth, const PString & eventPackage = PString::Empty()) const;

    /**
     * Find the SIPHandler object with the specified callID
     */
    PSafePtr<SIPHandler> FindSIPHandlerByCallID(const PString & callID, PSafetyMode m);

    /**
     * Find the SIPHandler object with the specified authRealm
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
};


/** Information for SIP "presence" event package notification messages.
  */
struct SIPPresenceInfo
{
  enum BasicStates {
    Unknown,
    Open,
    Closed
  };

  enum ExtendedStates {
    UnknownExtended,
    Appointment,
    Away,
    Breakfast,
    Busy,
    Dinner,
    Holiday,
    InTransit,
    LookingForWork,
    Lunch,
    Meal,
    Meeting,
    OnThePhone,
    Other,
    Performance,
    PermanentAbsence,
    Playing,
    Presentation,
    Shopping,
    Sleeping,
    Spectator,
    Steering,
    Travel,
    TV,
    Vacation,
    Working,
    Worship
  };

  SIPPresenceInfo() : m_basic(Unknown) { }

  // basic presence defined by RFC 3863
  PString     m_address;
  PString     m_entity;
  BasicStates m_basic;
  PString     m_note;
  PString     m_contact;
  PString     m_to;

  // presence extensions defined by RFC 4480
  PStringArray m_activities;  // list of activities, seperated by newline
  ExtendedStates m_activity;  // primary activity

  PString AsXML() const;

  void PrintOn(ostream & strm) const;
  friend ostream & operator<<(ostream & strm, const SIPPresenceInfo & info) { info.PrintOn(strm); return strm; }
};


/** Information for SIP "dialog" event package notification messages.
  */
struct SIPDialogNotification : public PObject
{
  PCLASSINFO(SIPDialogNotification, PObject);

  enum States {
    Terminated,
    Trying,
    Proceeding,
    Early,
    Confirmed,

    FirstState = Terminated,
    LastState = Confirmed
  };
  friend States operator++(States & state) { return (state = (States)(state+1)); }
  friend States operator--(States & state) { return (state = (States)(state-1)); }
  static PString GetStateName(States state);
  PString GetStateName() const { return GetStateName(m_state); }

  enum Events {
    NoEvent = -1,
    Cancelled,
    Rejected,
    Replaced,
    LocalBye,
    RemoteBye,
    Error,
    Timeout,

    FirstEvent = Cancelled,
    LastEvent = Timeout
  };
  friend Events operator++(Events & evt) { return (evt = (Events)(evt+1)); }
  friend Events operator--(Events & evt) { return (evt = (Events)(evt-1)); }
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


#endif // OPAL_SIP

#endif // OPAL_SIP_HANDLERS_H
