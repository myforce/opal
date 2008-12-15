/*
 * sipep.h
 *
 * Session Initiation Protocol endpoint.
 *
 * Open Phone Abstraction Library (OPAL)
 * Formally known as the Open H323 project.
 *
 * Copyright (c) 2001 Equivalence Pty. Ltd.
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
 * Contributor(s): ______________________________________.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#ifndef OPAL_SIP_SIPEP_H
#define OPAL_SIP_SIPEP_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#ifndef _PTLIB_H
#include <ptlib.h>
#endif

#include <opal/buildopts.h>

#if OPAL_SIP

#include <ptclib/sockagg.h>
#include <opal/rtpep.h>
#include <sip/sipcon.h>
#include <sip/sippdu.h>
#include <sip/handlers.h> 

#if OPAL_HAS_SIPIM
#include <im/sipim.h>
#endif

class SIPRegisterHandler;


//
//  provide flag so applications know if presence is available
//
#define OPAL_HAS_SIP_PRESENCE   1

/////////////////////////////////////////////////////////////////////////

/** Class to hold authentication information 
  */

class SIPAuthInfo : public PObject
{
  public:
    SIPAuthInfo()
    { }

    SIPAuthInfo(const PString & u, const PString & p)
    { username = u; password = p; }
    PString username;
    PString password;
};

/////////////////////////////////////////////////////////////////////////

/**Session Initiation Protocol endpoint.
 */
class SIPEndPoint : public OpalRTPEndPoint
{
  PCLASSINFO(SIPEndPoint, OpalRTPEndPoint);

  public:
  /**@name Construction */
  //@{
    /**Create a new endpoint.
     */
    SIPEndPoint(
      OpalManager & manager
    );

    /**Destroy endpoint.
     */
    ~SIPEndPoint();
  //@}

  /**@name Overrides from OpalEndPoint */
  //@{
    /**Shut down the endpoint, this is called by the OpalManager just before
       destroying the object and can be handy to make sure some things are
       stopped before the vtable gets clobbered.
      */
    virtual void ShutDown();

    /**Get the default transports for the endpoint type.
       Overrides the default behaviour to return udp and tcp.
      */
    virtual PString GetDefaultTransport() const;

    /**Handle new incoming connection from listener.

       The default behaviour does nothing.
      */
    virtual PBoolean NewIncomingConnection(
      OpalTransport * transport  ///<  Transport connection came in on
    );

    /**Set up a connection to a remote party.
       This is called from the OpalManager::MakeConnection() function once
       it has determined that this is the endpoint for the protocol.

       The general form for this party parameter is:

            [proto:][alias@][transport$]address[:port]

       where the various fields will have meanings specific to the endpoint
       type. For example, with H.323 it could be "h323:Fred@site.com" which
       indicates a user Fred at gatekeeper size.com. Whereas for the PSTN
       endpoint it could be "pstn:5551234" which is to call 5551234 on the
       first available PSTN line.

       The proto field is optional when passed to a specific endpoint. If it
       is present, however, it must agree with the endpoints protocol name or
       PFalse is returned.

       This function usually returns almost immediately with the connection
       continuing to occur in a new background thread.

       If PFalse is returned then the connection could not be established. For
       example if a PSTN endpoint is used and the assiciated line is engaged
       then it may return immediately. Returning a non-NULL value does not
       mean that the connection will succeed, only that an attempt is being
       made.

       The default behaviour is pure.
     */
    virtual PBoolean MakeConnection(
      OpalCall & call,                         ///<  Owner of connection
      const PString & party,                   ///<  Remote party to call
      void * userData,                         ///<  Arbitrary data to pass to connection
      unsigned int options,                    ///<  options to pass to conneciton
      OpalConnection::StringOptions * stringOptions  ///<  complex string options
    );

    /** Execute garbage collection for endpoint.
        Returns PTrue if all garbage has been collected.
        Default behaviour deletes the objects in the connectionsActive list.
      */
    virtual PBoolean GarbageCollection();
  //@}

  /**@name Customisation call backs */
  //@{
    /**Create a connection for the SIP endpoint.
       The default implementation is to create a OpalSIPConnection.
      */
    virtual SIPConnection * CreateConnection(
      OpalCall & call,                         ///<  Owner of connection
      const PString & token,                   ///<  token used to identify connection
      void * userData,                         ///<  User data for connection
      const SIPURL & destination,              ///<  Destination for outgoing call
      OpalTransport * transport,               ///<  Transport INVITE has been received on
      SIP_PDU * invite,                        ///<  Original INVITE pdu
      unsigned int options = 0,                ///<  connection options
      OpalConnection::StringOptions * stringOptions = NULL ///<  complex string options

    );
    
    /**Setup a connection transfer a connection for the SIP endpoint.
      */
    virtual PBoolean SetupTransfer(
      const PString & token,        ///<  Existing connection to be transferred
      const PString & callIdentity, ///<  Call identity of the secondary call (if it exists)
      const PString & remoteParty,  ///<  Remote party to transfer the existing call to
      void * userData = NULL        ///<  user data to pass to CreateConnection
    );
    
    /**Forward the connection using the same token as the specified connection.
       Return PTrue if the connection is being redirected.
      */
    virtual PBoolean ForwardConnection(
      SIPConnection & connection,     ///<  Connection to be forwarded
      const PString & forwardParty    ///<  Remote party to forward to
    );

  //@}
  
  /**@name Protocol handling routines */
  //@{

    /**Creates an OpalTransport instance, based on the
       address is interpreted as the remote address to which the transport should connect
      */
    OpalTransport * CreateTransport(
      const SIPURL & remoteURL,
      const PString & localInterface = PString::Empty()
    );

    virtual void HandlePDU(
      OpalTransport & transport
    );

    /**Handle an incoming SIP PDU that has been full decoded
      */
    virtual PBoolean OnReceivedPDU(
      OpalTransport & transport,
      SIP_PDU * pdu
    );

    /**Handle an incoming SIP PDU not assigned to any connection
      */
    virtual bool OnReceivedConnectionlessPDU(
      OpalTransport & transport, 
      SIP_PDU * pdu
    );

    /**Handle an incoming response PDU.
      */
    virtual void OnReceivedResponse(
      SIPTransaction & transaction,
      SIP_PDU & response
    );

    /**Handle an incoming INVITE request.
      */
    virtual PBoolean OnReceivedINVITE(
      OpalTransport & transport,
      SIP_PDU * pdu
    );

    /**Handle an incoming IntervalTooBrief response PDU
      */
    virtual void OnReceivedIntervalTooBrief(
      SIPTransaction & transaction, 
      SIP_PDU & response)
    ;
  
    /**Handle an incoming Proxy Authentication Required response PDU
      */
    virtual void OnReceivedAuthenticationRequired(
      SIPTransaction & transaction,
      SIP_PDU & response
    );

    /**Handle an incoming OK response PDU.
       This actually gets any PDU of the class 2xx not just 200.
      */
    virtual void OnReceivedOK(
      SIPTransaction & transaction,
      SIP_PDU & response
    );
    
    /**Handle an incoming NOTIFY PDU.
      */
    virtual PBoolean OnReceivedNOTIFY(
      OpalTransport & transport,
      SIP_PDU & response
    );

    /**Handle an incoming REGISTER PDU.
      */
    virtual PBoolean OnReceivedREGISTER(
      OpalTransport & transport, 
      SIP_PDU & pdu
    );

    /**Handle an incoming SUBSCRIBE PDU.
      */
    virtual PBoolean OnReceivedSUBSCRIBE(
      OpalTransport & transport, 
      SIP_PDU & pdu
    );

    /**Handle an incoming MESSAGE PDU.
      */
    virtual bool OnReceivedMESSAGE(
      OpalTransport & transport,
      SIP_PDU & response
    );

    /**Handle an incoming OPTIONS PDU.
      */
    virtual bool OnReceivedOPTIONS(
      OpalTransport & transport,
      SIP_PDU & response
    );
    
    /**Handle a SIP packet transaction failure
      */
    virtual void OnTransactionFailed(
      SIPTransaction & transaction
    );
    
    /**Callback from the RTP session for statistics monitoring.
       This is called every so many packets on the transmitter and receiver
       threads of the RTP session indicating that the statistics have been
       updated.

       The default behaviour does nothing.
      */
    virtual void OnRTPStatistics(
      const SIPConnection & connection,  ///<  Connection for the channel
      const RTP_Session & session         ///<  Session with statistics
    ) const;
  //@}
 

    /**Find a connection that uses the specified token.
       This searches the endpoint for the connection that contains the token
       as provided by functions such as MakeConnection().
      */
    PSafePtr<SIPConnection> GetSIPConnectionWithLock(
      const PString & token,     ///<  Token to identify connection
      PSafetyMode mode = PSafeReadWrite
    ) { return PSafePtrCast<OpalConnection, SIPConnection>(GetConnectionWithLock(token, mode)); }

    virtual PBoolean IsAcceptedAddress(const SIPURL & toAddr);


    /**Callback for SIP message received
     */
    virtual void OnMessageReceived (const SIPURL & from, const PString & body);
    virtual void OnMessageReceived (const SIPURL & from, const SIP_PDU & pdu);

    /**Register to a registrar. This function is asynchronous to permit
       several registrations to occur at the same time. It can be
       called several times for different hosts and users.
       
       The username can be of the form user@domain. In that case,
       the address-of-record field will be set to that value. If not then
       the address-of-record is constructed from the user and host fields.
       
       The realm can be specified when registering, this will
       allow to find the correct authentication information when being
       requested. If no realm is specified, authentication will
       occur with the "best guess" of authentication parameters.
     */
    bool Register(
      const SIPRegister::Params & params, ///< Registration paarameters
      PString & aor                       ///< Resultant address-of-record for unregister
    );

    /// Registration function for backward compatibility.
    bool Register(
      const PString & host,
      const PString & user = PString::Empty(),
      const PString & autName = PString::Empty(),
      const PString & password = PString::Empty(),
      const PString & authRealm = PString::Empty(),
      unsigned expire = 0,
      const PTimeInterval & minRetryTime = PMaxTimeInterval, 
      const PTimeInterval & maxRetryTime = PMaxTimeInterval
    );

    /**Unregister from a registrar.
       This will unregister the specified address-of-record. If an empty
       string is provided then ALL registrations are removed.
     */
    bool Unregister(const PString & aor);

    /**Unregister all current registrations.
      */
    bool UnregisterAll();

    /**Returns PTrue if the given URL has been registered 
     * (e.g.: 6001@seconix.com). The includeOffline parameter indicates
       if the caller is interested in if we are, to the best of our knowlegde,
       currently registered (have had recent confirmation) or we are not sure
       if we are registered or not, but are continually re-trying.
     */
    PBoolean IsRegistered(
      const PString & aor,          ///< Address of record to check
      bool includeOffline = false   ///< Include offline registrations
    );

    /** Returns the number of registered accounts.
     */
    unsigned GetRegistrationsCount() const { return activeSIPHandlers.GetCount(SIP_PDU::Method_REGISTER); }

    /** Returns a list of registered accounts.
     */
    PStringList GetRegistrations(
      bool includeOffline = false   ///< Include offline registrations
    ) const { return activeSIPHandlers.GetAddresses(includeOffline, SIP_PDU::Method_REGISTER); }

    /** Information provided on the registration status. */
    struct RegistrationStatus {
      PString              m_addressofRecord;   ///< Address of record for registration
      bool                 m_wasRegistering;    ///< Was registering or unregistering
      bool                 m_reRegistering;     ///< Was a registration refresh
      SIP_PDU::StatusCodes m_reason;            ///< Reason for status change
      OpalProductInfo      m_productInfo;       ///< Server product info from registrar if available.
    };

    /**Callback called when a registration to a SIP registrar status.
     */
    virtual void OnRegistrationStatus(
      const RegistrationStatus & status   ///< Status of registration request
    );

    // For backward compatibility
    virtual void OnRegistrationStatus(
      const PString & aor,
      PBoolean wasRegistering,
      PBoolean reRegistering,
      SIP_PDU::StatusCodes reason
    );

    /**Callback called when a registration to a SIP registrars fails.
       Deprecated, maintained for backward compatibility, use OnRegistrationStatus().
     */
    virtual void OnRegistrationFailed(
      const PString & aor,
      SIP_PDU::StatusCodes reason,
      PBoolean wasRegistering
    );

    /**Callback called when a registration or an unregistration is successful.
       Deprecated, maintained for backward compatibility, use OnRegistrationStatus().
     */
    virtual void OnRegistered(
      const PString & aor,
      PBoolean wasRegistering
    );


    /**Subscribe to a notifier. This function is asynchronous to permit
     * several subscriptions to occur at the same time.
     */
    bool Subscribe(
      SIPSubscribe::PredefinedPackages eventPackage, ///< Event package being unsubscribed
      unsigned expire,                               ///< Expiry time in seconds
      const PString & aor                            ///< Address-of-record for subscription
    );
    bool Subscribe(
      const SIPSubscribe::Params & params, ///< Subscription paarameters
      PString & aor                        ///< Resultant address-of-record for unsubscribe
    );


    bool Unsubscribe(
      SIPSubscribe::PredefinedPackages eventPackage,  ///< Event package being unsubscribed
      const PString & aor                             ///< Address-of-record for subscription
    );
    bool Unsubscribe(
      const PString & eventPackage,  ///< Event package being unsubscribed
      const PString & aor            ///< Address-of-record for subscription
    );

    /**Unsubscribe all current subscriptions.
      */
    bool UnsubcribeAll(
      SIPSubscribe::PredefinedPackages eventPackage  ///< Event package being unsubscribed
    );
    bool UnsubcribeAll(
      const PString & eventPackage  ///< Event package being unsubscribed
    );

    /**Returns true if the endpoint is subscribed to some
       event for the given to address. The includeOffline parameter indicates
       if the caller is interested in if we are, to the best of our knowlegde,
       currently subscribed (have had recent confirmation) or we are not sure
       if we are subscribed or not, but are continually re-trying.
     */
    PBoolean IsSubscribed(
      const PString & eventPackage,  ///< Event package being unsubscribed
      const PString & aor,           ///< Address-of-record for subscription
      bool includeOffline = false    ///< Include offline subscriptions
    );

    /** Returns the number of registered accounts.
     */
    unsigned GetSubscriptionCount(
      const SIPSubscribe::EventPackage & eventPackage  ///< Event package of subscription
    ) { return activeSIPHandlers.GetCount(SIP_PDU::Method_SUBSCRIBE, eventPackage); }

    /** Returns a list of subscribed accounts for package.
     */
    PStringList GetSubscriptions(
      const SIPSubscribe::EventPackage & eventPackage, ///< Event package of subscription
      bool includeOffline = false   ///< Include offline subscriptions
    ) const { return activeSIPHandlers.GetAddresses(includeOffline, SIP_PDU::Method_REGISTER, eventPackage); }

    /**Callback called when a subscription to a SIP UA status changes.
     */
    virtual void OnSubscriptionStatus(
      const PString & eventPackage, ///< Event package subscribed to
      const SIPURL & uri,           ///< Target URI for the subscription.
      bool wasSubscribing,          ///< Indication the subscribing or unsubscribing
      bool reSubscribing,           ///< If subscribing then indication was refeshing subscription
      SIP_PDU::StatusCodes reason   ///< Status of subscription
    );

    /** Indicate notifications for the specified event package are supported.
      */
    virtual bool CanNotify(
      const PString & eventPackage ///< Event package we support
    );

    /** Send notification to all remotes that are subcribed to the event package.
      */
    bool Notify(
      const SIPURL & targetAddress, ///< Address that was subscribed
      const PString & eventPackage, ///< Event package for notification
      const PString & body          ///< Body of notification
    );


    /**Callback called when dialog NOTIFY message is received
     */
    virtual void OnDialogInfoReceived(
      const SIPDialogNotification & info  ///< Information on dialog state change
    );

    void SendNotifyDialogInfo(
      const SIPDialogNotification & info  ///< Information on dialog state change
    );


    /**Send a message to the given URL.
     */
    PBoolean Message(
      const PString & to, 
      const PString & body
    );
    PBoolean Message(
      const PString & to, 
      const PString & body,
      const PString & remoteContact, 
      const PString & callID
    );
    
    /**Callback called when a message sent by the endpoint didn't reach
     * its destination or when the proxy or remote endpoint returns
     * an error code.
     */
    virtual void OnMessageFailed(
      const SIPURL & messageUrl,
      SIP_PDU::StatusCodes reason
    );


    /**Publish new state information.
       A expire time of zero will cease to automatically update the publish.
     */
    bool Publish(
      const SIPSubscribe::Params & params,
      const PString & body,
      PString & aor
    );
    bool Publish(
      const PString & to,   ///< Address to send PUBLISH
      const PString & body, ///< Body of PUBLISH
      unsigned expire = 300 ///< Time between automatic updates in seconds.
    );

    /** Returns a list of published entities.
     */
    PStringList GetPublications(
      const SIPSubscribe::EventPackage & eventPackage, ///< Event package for publication
      bool includeOffline = false   ///< Include offline publications
    ) const { return activeSIPHandlers.GetAddresses(includeOffline, SIP_PDU::Method_PUBLISH, eventPackage); }


    /**Publish new state information.
     * Only the basic & note fields of the PIDF+xml are supported for now.
     */
    bool PublishPresence(
      const SIPPresenceInfo & info,  ///< Presence info to publish
      unsigned expire = 300          ///< Refresh time
    );
    
    /**Callback called when presence is received
     */
    virtual void OnPresenceInfoReceived (
      const SIPPresenceInfo & info  ///< Presence info publicised
    );
    virtual void OnPresenceInfoReceived (
      const PString & identity,
      const PString & basic,
      const PString & note
    );


    /**Send a SIP PING to the remote host
      */
    PBoolean Ping(
      const PString & to
    );


    void SetMIMEForm(PBoolean v) { mimeForm = v; }
    PBoolean GetMIMEForm() const { return mimeForm; }

    void SetMaxRetries(unsigned r) { maxRetries = r; }
    unsigned GetMaxRetries() const { return maxRetries; }

    void SetRetryTimeouts(
      const PTimeInterval & t1,
      const PTimeInterval & t2
    ) { retryTimeoutMin = t1; retryTimeoutMax = t2; }
    const PTimeInterval & GetRetryTimeoutMin() const { return retryTimeoutMin; }
    const PTimeInterval & GetRetryTimeoutMax() const { return retryTimeoutMax; }

    void SetNonInviteTimeout(
      const PTimeInterval & t
    ) { nonInviteTimeout = t; }
    const PTimeInterval & GetNonInviteTimeout() const { return nonInviteTimeout; }

    void SetPduCleanUpTimeout(
      const PTimeInterval & t
    ) { pduCleanUpTimeout = t; }
    const PTimeInterval & GetPduCleanUpTimeout() const { return pduCleanUpTimeout; }

    void SetInviteTimeout(
      const PTimeInterval & t
    ) { inviteTimeout = t; }
    const PTimeInterval & GetInviteTimeout() const { return inviteTimeout; }

    void SetAckTimeout(
      const PTimeInterval & t
    ) { ackTimeout = t; }
    const PTimeInterval & GetAckTimeout() const { return ackTimeout; }

    void SetRegistrarTimeToLive(
      const PTimeInterval & t
    ) { registrarTimeToLive = t; }
    const PTimeInterval & GetRegistrarTimeToLive() const { return registrarTimeToLive; }
    
    void SetNotifierTimeToLive(
      const PTimeInterval & t
    ) { notifierTimeToLive = t; }
    const PTimeInterval & GetNotifierTimeToLive() const { return notifierTimeToLive; }
    
    void SetNATBindingTimeout(
      const PTimeInterval & t
    ) { natBindingTimeout = t; natBindingTimer.RunContinuous (natBindingTimeout); }
    const PTimeInterval & GetNATBindingTimeout() const { return natBindingTimeout; }

    void AddTransaction(
      SIPTransaction * transaction
    ) { transactions.SetAt(transaction->GetTransactionID(), transaction); }

    PSafePtr<SIPTransaction> GetTransaction(const PString & transactionID, PSafetyMode mode = PSafeReadWrite)
    { return transactions.FindWithLock(transactionID, mode); }
    
    /**Return the next CSEQ for the next transaction.
     */
    unsigned GetNextCSeq() { return ++lastSentCSeq; }

    /**Return the SIPAuthentication for a specific realm.
     */
    PBoolean GetAuthentication(const PString & authRealm, PString & realm, PString & user, PString & password); 
    
    /**Return the registered party name URL for the given host.
     *
     * That URL can be used in the FORM field of the PDU's. 
     * The host part can be different from the registration domain.
     */
    virtual SIPURL GetRegisteredPartyName(const SIPURL & remoteURL, const OpalTransport & transport);


    /**Return the default registered party name URL.
     */
    virtual SIPURL GetDefaultRegisteredPartyName(const OpalTransport & transport);
    

    /**Return the contact URL for the given host and user name
     * based on the listening port of the registration to that host.
     * 
     * That URL can be used as as contact field in outgoing
     * requests.
     *
     * The URL is translated if required.
     *
     * If no active registration is used, return the result of GetLocalURL
     * on the given transport.
     */
    SIPURL GetContactURL(const OpalTransport &transport, const PString & userName, const PString & host);


    /**Return the local URL for the given transport and user name.
     * That URL can be used as via address, and as contact field in outgoing
     * requests.
     *
     * The URL is translated if required.
     *
     * If the transport is not running, the first listener transport
     * will be used, if any.
     */
    virtual SIPURL GetLocalURL(
       const OpalTransport & transport,             ///< Transport on which we can receive new requests
       const PString & userName = PString::Empty()  ///< The user name part of the contact field
    );
    
    
    /**Return the outbound proxy URL, if any.
     */
    const SIPURL & GetProxy() const { return proxy; }

    
    /**Set the outbound proxy URL.
     */
    void SetProxy(const SIPURL & url);
    
    
    /**Set the outbound proxy URL.
     */
    void SetProxy(
      const PString & hostname,
      const PString & username,
      const PString & password
    );

    
    /**Get the default line appearance code for new connections.
      */
    int GetDefaultAppearanceCode() const { return m_defaultAppearanceCode; }

    /**Set the default line appearance code for new connections.
      */
    void SetDefaultAppearanceCode(int code) { m_defaultAppearanceCode = code; }

    /**Get the User Agent for this endpoint.
       Default behaviour returns an empty string so the SIPConnection builds
       a valid string from the productInfo data.

       These semantics are for backward compatibility.
     */
    virtual PString GetUserAgent() const;
        
    /**Set the User Agent for the endpoint.
     */
    void SetUserAgent(const PString & str) { userAgentString = str; }


    /** Return a bit mask of the allowed SIP methods.
      */
    virtual unsigned GetAllowedMethods() const;


    /**NAT Binding Refresh Method
     */
    enum NATBindingRefreshMethod{
      None,
      Options,
      EmptyRequest,
      NumMethods
    };


    /**Set the NAT Binding Refresh Method
     */
    void SetNATBindingRefreshMethod(const NATBindingRefreshMethod m) { natMethod = m; }

    virtual SIPRegisterHandler * CreateRegisterHandler(const SIPRegister::Params & params);

    virtual void OnStartTransaction(SIPConnection & conn, SIPTransaction & transaction);

  protected:
    PDECLARE_NOTIFIER(PThread, SIPEndPoint, TransportThreadMain);
    PDECLARE_NOTIFIER(PTimer, SIPEndPoint, NATBindingRefresh);

    SIPURL        proxy;
    PString       userAgentString;

    bool          mimeForm;
    unsigned      maxRetries;
    PTimeInterval retryTimeoutMin;   // T1
    PTimeInterval retryTimeoutMax;   // T2
    PTimeInterval nonInviteTimeout;  // T3
    PTimeInterval pduCleanUpTimeout; // T4
    PTimeInterval inviteTimeout;
    PTimeInterval ackTimeout;
    PTimeInterval registrarTimeToLive;
    PTimeInterval notifierTimeToLive;
    PTimeInterval natBindingTimeout;

    bool              m_shuttingDown;
    SIPHandlersList   activeSIPHandlers;

    PSafeDictionary<PString, SIPTransaction> transactions;

    PTimer                  natBindingTimer;
    NATBindingRefreshMethod natMethod;
    PAtomicInteger          lastSentCSeq;
    unsigned                m_dialogNotifyVersion;
    int                     m_defaultAppearanceCode;

    struct SIP_PDU_Work
    {
      SIP_PDU_Work()
      { ep = NULL; pdu = NULL; }

      SIPEndPoint * ep;
      SIP_PDU * pdu;
      PString callID;
    };

    typedef std::queue<SIP_PDU_Work *> SIP_PDUWorkQueue;

    class SIP_PDU_Thread : public PThreadPoolWorkerBase
    {
      public:
        SIP_PDU_Thread(PThreadPoolBase & _pool);
        unsigned GetWorkSize() const;
        void OnAddWork(SIP_PDU_Work * work);
        void OnRemoveWork(SIP_PDU_Work *);
        void Shutdown();
        void Main();

      protected:
        PMutex mutex;
        PSyncPoint sync;
        SIP_PDUWorkQueue pduQueue;
    };

    typedef PThreadPool<SIP_PDU_Work, SIP_PDU_Thread> SIPMainThreadPool;
    SIPMainThreadPool threadPool;

    enum {
      HighPriority = 80,
      LowPriority  = 30,
    };
    class InterfaceMonitor : public PInterfaceMonitorClient
    {
        PCLASSINFO(InterfaceMonitor, PInterfaceMonitorClient);
      public:
        InterfaceMonitor(SIPEndPoint & manager, PINDEX priority);
        
        virtual void OnAddInterface(const PIPSocket::InterfaceEntry & entry);
        virtual void OnRemoveInterface(const PIPSocket::InterfaceEntry & entry);

    protected:
        SIPEndPoint & m_endpoint;
    };
    InterfaceMonitor m_highPriorityMonitor;
    InterfaceMonitor m_lowPriorityMonitor;

    friend void InterfaceMonitor::OnAddInterface(const PIPSocket::InterfaceEntry & entry);
    friend void InterfaceMonitor::OnRemoveInterface(const PIPSocket::InterfaceEntry & entry);

#if OPAL_HAS_SIPIM
    OpalSIPIMManager m_sipIMManager;
#endif
};


#endif // OPAL_SIP

#endif // OPAL_SIP_SIPEP_H


// End of File ///////////////////////////////////////////////////////////////
