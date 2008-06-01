/*
 * connection.h
 *
 * Telephony connection abstraction
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
 * Contributor(s): Post Increment
 *     Portions of this code were written with the assistance of funding from
 *     US Joint Forces Command Joint Concept Development & Experimentation (J9)
 *     http://www.jfcom.mil/about/abt_j9.htm
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#ifndef __OPAL_CONNECTION_H
#define __OPAL_CONNECTION_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include <opal/buildopts.h>

#include <opal/mediafmt.h>
#include <opal/mediastrm.h>
#include <opal/guid.h>
#include <opal/transports.h>
#include <ptclib/dtmf.h>
#include <ptlib/safecoll.h>
#include <rtp/rtp.h>

class OpalEndPoint;
class OpalCall;
class OpalSilenceDetector;
class OpalEchoCanceler;
class OpalRFC2833Proto;
class OpalRFC2833Info;

/*! \page pageOpalConnections Connection handling in the OPAL library

\section secOverview Overview 
This page of the documentation is designed to provide an understanding of the
use of the OpalConnection class in the OPAL library. From the description, you
will be able to add your own voip protocol to OPAL, and utilise the features
provided by the existing class structure. 

Key to understanding the OpalConnection class is that this class is
responsible for supervising the call signaling and media control for one call.

There will be one descendant of this OpalConnection class created for each
protocol that is supported.


\section secCallPhases Call Phases

Defined in OpalConnection, there is an enum (OpalConnection::Phases) that describes the
different stages of the call. Note that all OpalConnection instances go
through these phases as a call progresses. Even the OpalPCSSConnection, which
is the connection of PCM audio to/from the sound card, will (at some time) be
in each of the available phases.

To illustrate the meaning of the phases, we will descibe a very simple voip
protocol. This example protocol will also be used to show when the different
callbacks happen, and when the phases are changed at either endpoint of the
call. 

The two endpoints in the call are labelled L and R (left and right). L is making a call to R

\li L goes to OpalConnection::SetUpPhase, and  sends invite packet to R
\li R goes to OpalConnection::SetUpPhase, and starts ringing the phone
\li R goes to OpalConnection::AlertingPhase
\li R sends a message to L, "my phone is ringing"
\li L goes to OpalConnection::AlertingPhase
\li L sends a message to R, "I can only do G711 ALaw, RTP data to port 5400"
\li R sends s message to L, "Ok, we talk with G711 ALaw, send RTP data to me on port 5401"
\li R and L both wait for R's user to accept the ringing phone
\li R's user accepts the call by picking up the phone
\li R goes to OpalConnection::ConnectedPhase
\li R tells L that the call is accepted
\li L goes to OpalConnection::ConnectedPhase
\li R notices that media type is agreed on, the call is accepted, so goes to OpalConnection::EstablishedPhase
\li R realises that L knows everything is good for a call, so sends media to L
\li L reads the accepted message from R, goes to OpalConnection::EstablishedPhase
\li L sends media to R
\li both L and R exchange G711 ALaw on RTP port s 5400 and 5401
\li L (it could be R) sends a hangup message to R
\li L goes to OpalConnection::ReleasingPhase, and then stops all media traffic.
\li R receives the hangup message, goes to OpalConnection::ReleasingPhase, and stops all media traffic
\li L (and R) switch to OpalConnection::ReleasedPhase when all media and control streams have been terminated.

In the example above, if R had answered yes immediately, the call could not
move immediately to OpalConnection::EstablishedPhase as the media types were
not agreed on.

In the example above, we have described the decision process as to the codec
type, and the ports used. This decision process is not necessarily directly
handled by the OpalConnection descendant. For IAX2, where media negotiation is
an integral part of setting up the call, the IAX2Connection does media
negotiation. However, for SIP, there are SDPBandwidth, SDPMediaDescription,
SDPMediaFormat, SDPSessionDescription classes for handling media
negotiation. For the case of a pstn gateway using ISDN, there is no media
negotiation.  It always uses G.711 and the same "port" (the B channel).


The different phases are. 

  \li OpalConnection::UninitialisedPhase - there is no call active. An
                           OpalConnection takes this phase when instantiated.

  \li OpalConnection::SetUpPhase - the call is being created. This is around
                           the time of the first packet, which announces the
                           intent to create a call. At this point, there is no
                           agreement on the media format etc.
 
  \li OpalConnection::AlertingPhase - the OpalConnection instance is aware the phone is ringing.


  \li OpalConnection::ConnectedPhase - both endpoints have accepted the call, there may or may
                            not be agreement on media atthis time. Note that in many systems
                            this constitutes the start of charging for a call.

  \li OpalConnection::EstablishedPhase - we are "connected, there is agreement on the media
                            type, network ports, media can flow. This is the condition that
                            contitutes a call being "ready to use".

  \li OpalConnection::ReleasingPhase - Either side (L or R) has said
                        "hangup". The media streams are in the process of
                        closing down.

  \li OpalConnection::ReleasedPhase - Media and control streams have closed
                      down.  When the call is in this phase, there is no
                      possibility of additional messages between L and R.

  \li OpalConnection::NumPhases - Used for internal accounting. This indicates
                      that the phase is unknown.

\section secCallbacks OnEvent Callbacks in Opal

An OnXXX function is called when an external stimuli from the protocol occurs
and a SetXXX function may be called by OPAL to perform the protocol command. For
example OnAlerting is called when the H.323 ALERTING packet is received, and
SetAlerting transmits the ALERTING packet.

It is the OpalConnection class (or descendant) which is in charge of handling
the control packets of a particular protocol. Consequently, it is only
code in this class which sets the current call phase variable. Further, most
events are generated in the OpalConnection class, which are usually passed back to the
OpalEndPoint, then to the OpalManager, and usually on to the OpalCall class.

Consequently, if a descendant of the OpalManager has been created, the
application will have access to to notification on when the phase of a call
has changed. Applications may create descendants of the H323Connection and
SIPConnection to get protocol specific information about call progress.

Whenever the application does override a method in OpalManager (or
OpalEndPoint, or OpalConnection), the application should 
\li spend the minimum of time in the overridden method, as this is holding up
     call processing
\li call the method that was overridden, to ensure the correct operation of
    Opal


The stages in a call, as applies to the methods provided by the OpalEndPoint,
OpalConnection, OpalCall and Manager classes.  

\li The OpalConnection derivative, on deciding that a call is ready to begin
     negotiations as to media etc, sets the phase to
     OpalConnection::SetUpPhase, and invoke OpalCall::OnSetUp(). Note that it
     is only partyA which invokes OpalCall::OnSetUp(). Then, for the other
     OpalConnection instance in this voip call, the methods
     OpalConnection::SetUpConnection and OpalConnection::OnSetUpConnection are
     invoked. The OpalConnection::OnSetUpConnection then invokes
     OpalEndPoint::OnSetUpConnection(). 

\li The recipient of an incoming voip call (R from example above) will go
     straight from OpalConnection::UninitialisedPhase to
     OpalConnection::AlertingPhase, on receipt of the INVITE/Hello packet. The
     initiator of a voip call (L from example above) moves to
     OpalConnection::AlertingPhase on receiving a notice the call has not been
     immediately rejected.

\li When an OpalConnection instance moves to OpalConnection::OnAlerting
    phase, which invokes OpalConnection::OnAlerting, which invokes
    OpalEndPoint::OnAlerting, which invokes OpalManager::OnAlerting, which
    invokes OpalCall::OnAlerting. The OpalCall::OnAlerting method then invokes
    the OpalConnection::SetAlerting method of the other OpalConnection
    instance in this call (which is partyB). Consequently, when one
    OpalConnection instance in a call does OpalConnection::OnAlerting, the
    other OpalConnection instance in the call does a
    OpalConnection::SetAlerting.

\li The method for deciding on the appropriate media to use, and for deciding
    when the user at R actually picks up the remote phone to accept the call,
    is entirely protocol specific. This decision making process is handled
    by code in the descendant of the OpalConnection class. As much of the
    common behaviour is abstracted to OpalConnection, OpalMediaFormat etc
    as possible, but the details must reside in the OpalConnection.

\li When the signaling is completed, and the call is accepted at R, both L and R
     will move to the OpalConnection::ConnectedPhase, and invoke
     OpalConnection::OnConnected.  which invokes OpalEndPoint::OnConnected,
     which invokes OpalManager::OnConnected, which invokes
     OpalCall::OnConnected. The OpalCall::OnConnected method then invokes the
     OpalConnection::SetConnected method of the other OpalConnection instance
     in this call (which is partyB). Consequently, when one OpalConnection
     instance in a call does OpalConnection::OnConnected, the other
     OpalConnection instance in the call does a OpalConnection::SetConnected.
     The operation of the OpalConnection::SetConnected method is currently
     empty - it is a protocol dependant thing exactly how to respond.

\li When the media is decided on, and the call has been accepted at R, media
     can flow. At the point where the media threads are created and agreed on
     at both ends, the call is marked as OpalConnection::OnEstablished. It is
     the transition to the established phase that starts the media
     streams. The descendant of OpalConnection decides to move to established
     phase, and invokes OpalConnection::OnEstablished.  which starts the media
     streams and invokes OpalEndPoint::OnEstablished, which invokes
     OpalManager::OnEstablished., which invokes OpalCall::OnEstablished. The
     OpalCall::OnEstablished method then checks that there are two
     OpalConnection instances in the call, and starts the media streams on the
     calling connection. The OpalCall::OnEstablished method then checks that
     all OpalConnection instances in this call are marked as
     Established. Fininally, the OpalCall::OnEstablishedCall method is
     invoked, which then invokes OpalManager::OnEstablishedCall. The
     OpalManager::OnEstablishedCall method is empty, but if an application
     overrides this method, the application will get notification of when a
     call has started sending media.

\li Calls are terminated by the OpalManager::ClearCall method, which invokes
    OpalCall::Clear. For each OpalConnection instance in the OpalCall class,
    the OpalConnection::Release method is invoked. The call phase is switched
    to OpalConnection::ReleasingPhase and a thread is created to close down
    the OpalConnection instance. This thread invokes
    OpalConnection::OnRelease, which then invokes OpalConnection::OnReleased,
    closes the media streams, and then invokes OpalEndPoint::OnReleased. The
    endpoint invokes OpalManager::OnReleased, which invokes
    OpalCall::OnReleased. The OpalCall::OnReleased method will do
    OpalConnection::Release() on the other OpalConnection instance managed by
    the OpalCall (if the other OpalConnection is still there). Once all
    OpalConnection instances have gone from the OpalCall, OpalCall::OnCleared
    is invoked, which invokes OpalManager::OnClearedCall. The
    OpalManager::OnClearedCall method is empty, but can be overridden by the
    application to get notification when all components of a call (media,
    control) have fully terminated.
*/

/** Class for carying vendor/product
    information.
  */
class OpalProductInfo
{
  public:
    OpalProductInfo();

    static OpalProductInfo & Default();

    PCaselessString AsString() const;

    PString vendor;
    PString name;
    PString version;
    BYTE    t35CountryCode;
    BYTE    t35Extension;
    WORD    manufacturerCode;
};


/**This is the base class for connections to an endpoint.
   A particular protocol will have a descendant class from this to implement
   the specific semantics of that protocols connection.

   A connection is part of a call, and will be "owned" by an OpalCall object.
   It is also attached to the creator endpoint to do any protocol specific
   management of the connection. However the deletion of the connection is
   done by a special thread in the OpalManager class. A connnection should
   never be deleted directly.

   The connection is also in charge of creating media streams. It may do this
   in respose to an explicit call to OpenMediaStream or implicitly due to
   requests in the underlying protocol.

   When media streams are created they must makeinbanddtmf requests for bandwidth which
   is managed by the connection.
 */
class OpalConnection : public PSafeObject
{
    PCLASSINFO(OpalConnection, PSafeObject);
  public:
    /**Call/Connection ending reasons.
       NOTE: if anything is added to this, you also need to add the field to
       the tables in connection.cxx and h323pdu.cxx.
      */
    enum CallEndReason {
      EndedByLocalUser,         /// Local endpoint application cleared call
      EndedByNoAccept,          /// Local endpoint did not accept call OnIncomingCall()=PFalse
      EndedByAnswerDenied,      /// Local endpoint declined to answer call
      EndedByRemoteUser,        /// Remote endpoint application cleared call
      EndedByRefusal,           /// Remote endpoint refused call
      EndedByNoAnswer,          /// Remote endpoint did not answer in required time
      EndedByCallerAbort,       /// Remote endpoint stopped calling
      EndedByTransportFail,     /// Transport error cleared call
      EndedByConnectFail,       /// Transport connection failed to establish call
      EndedByGatekeeper,        /// Gatekeeper has cleared call
      EndedByNoUser,            /// Call failed as could not find user (in GK)
      EndedByNoBandwidth,       /// Call failed as could not get enough bandwidth
      EndedByCapabilityExchange,/// Could not find common capabilities
      EndedByCallForwarded,     /// Call was forwarded using FACILITY message
      EndedBySecurityDenial,    /// Call failed a security check and was ended
      EndedByLocalBusy,         /// Local endpoint busy
      EndedByLocalCongestion,   /// Local endpoint congested
      EndedByRemoteBusy,        /// Remote endpoint busy
      EndedByRemoteCongestion,  /// Remote endpoint congested
      EndedByUnreachable,       /// Could not reach the remote party
      EndedByNoEndPoint,        /// The remote party is not running an endpoint
      EndedByHostOffline,       /// The remote party host off line
      EndedByTemporaryFailure,  /// The remote failed temporarily app may retry
      EndedByQ931Cause,         /// The remote ended the call with unmapped Q.931 cause code
      EndedByDurationLimit,     /// Call cleared due to an enforced duration limit
      EndedByInvalidConferenceID, /// Call cleared due to invalid conference ID
      EndedByNoDialTone,        /// Call cleared due to missing dial tone
      EndedByNoRingBackTone,    /// Call cleared due to missing ringback tone
      EndedByOutOfService,      /// Call cleared because the line is out of service, 
      EndedByAcceptingCallWaiting, /// Call cleared because another call is answered
      NumCallEndReasons,

      EndedWithQ931Code = 0x100  /// Q931 code specified in MS byte
    };

#if PTRACING
    friend ostream & operator<<(ostream & o, CallEndReason reason);
#endif

    enum AnswerCallResponse {
      AnswerCallNow,               /// Answer the call continuing with the connection.
      AnswerCallDenied,            /// Refuse the call sending a release complete.
      AnswerCallPending,           /// Send an Alerting PDU and wait for AnsweringCall()
      AnswerCallDeferred,          /// As for AnswerCallPending but does not send Alerting PDU
      AnswerCallAlertWithMedia,    /// As for AnswerCallPending but starts media channels
      AnswerCallDeferredWithMedia, /// As for AnswerCallDeferred but starts media channels
      AnswerCallProgress,          /// Answer the call with a h323 progress, or sip 183 session in progress, or ... 
      AnswerCallNowAndReleaseCurrent, /// Answer the call and destroy the current call
      NumAnswerCallResponses
    };
#if PTRACING
    friend ostream & operator<<(ostream & o, AnswerCallResponse s);
#endif

    /** Connection options
    */
    enum Options {
      FastStartOptionDisable       = 0x0001,   // H.323 specific
      FastStartOptionEnable        = 0x0002,
      FastStartOptionMask          = 0x0003,

      H245TunnelingOptionDisable   = 0x0004,   // H.323 specific
      H245TunnelingOptionEnable    = 0x0008,
      H245TunnelingOptionMask      = 0x000c,

      H245inSetupOptionDisable     = 0x0010,   // H.323 specific
      H245inSetupOptionEnable      = 0x0020,
      H245inSetupOptionMask        = 0x0030,

      DetectInBandDTMFOptionDisable = 0x0040,  // SIP and H.323
      DetectInBandDTMFOptionEnable  = 0x0080,
      DetectInBandDTMFOptionMask    = 0x00c0,

      RTPAggregationDisable        = 0x0100,   // SIP and H.323
      RTPAggregationEnable         = 0x0200,
      RTPAggregationMask           = 0x0300,

      SendDTMFAsDefault            = 0x0000,   // SIP and H.323
      SendDTMFAsString             = 0x0400,
      SendDTMFAsTone               = 0x0800,
      SendDTMFAsRFC2833            = 0x0c00,
      SendDTMFMask                 = 0x0c00
    };

    class StringOptions : public PStringToString 
    {
    };

  /**@name Construction */
  //@{
    /**Create a new connection.
     */
    OpalConnection(
      OpalCall & call,                         ///<  Owner calll for connection
      OpalEndPoint & endpoint,                 ///<  Owner endpoint for connection
      const PString & token,                   ///<  Token to identify the connection
      unsigned options = 0,                    ///<  Connection options
      OpalConnection::StringOptions * stringOptions = NULL     ///< more complex options
    );  

    /**Destroy connection.
     */
    ~OpalConnection();
  //@}

  /**@name Overrides from PObject */
  //@{
    /**Standard stream print function.
       The PObject class has a << operator defined that invokes this function
       polymorphically.
      */
    void PrintOn(
      ostream & strm    ///<  Stream to output text representation
    ) const;
  //@}

  /**@name Basic operations */
  //@{
    /**Get indication of connection being to a "network".
       This indicates the if the connection may be regarded as a "network"
       connection. The distinction is about if there is a concept of a "remote"
       party being connected to and is best described by example: sip, h323,
       iax and pstn are all "network" connections as they connect to something
       "remote". While pc, pots and ivr are not as the entity being connected
       to is intrinsically local.

       As a rule a network connection would have different names returned by
       GetLocalPartyName() and GetRemotePartyName() functions. A non-network
       connection, for ease of use, has a unique value for GetLocalPartyName()
       but, for convenience, uses the GetRemotePartyName() for the other
       connection in the call for it's GetRemotePartyName().
      */
    virtual bool IsNetworkConnection() const = 0;

    /**Different phases of a call, which are used in all OpalConnection
       instances. These phases are fully described in the documentation page
       \ref pageOpalConnections.  */
    enum Phases {
	UninitialisedPhase,   /*!< Indicates the OpalConnection instance has just been constructed*/
	SetUpPhase,           /*!< In the process of sending/receiving the initial INVITE packet */
	AlertingPhase,        /*!< There is a phone ringing, somewhere                           */
	ConnectedPhase,       /*!< There is agreement on having a call, media type etc            */
	EstablishedPhase,     /*!< Media is flowing, control streams  are all operational         */
	ReleasingPhase,       /*!< Hangup packet has been sent/received, media and control not yet stopped  */
	ReleasedPhase,        /*!< Media and control streams have been terminated                           */
	NumPhases             /*!< Number of available phases. Can be used to indicate an unknown phase     */
    };

    /**Get the phase of the connection.
       This indicates the current phase of the connection sequence. Whether
       all phases and the transitions between phases is protocol dependent.
      */
    inline Phases GetPhase() const { return phase; }

    /**Get the call clearand reason for this connection shutting down.
       Note that this function is only generally useful in the
       H323EndPoint::OnConnectionCleared() function. This is due to the
       connection not being cleared before that, and the object not even
       exiting after that.

       If the call is still active then this will return NumCallEndReasons.
      */
    CallEndReason GetCallEndReason() const { return callEndReason; }

    /**Set the call clearance reason.
       An application should have no cause to use this function. It is present
       for the H323EndPoint::ClearCall() function to set the clearance reason.
      */
    virtual void SetCallEndReason(
      CallEndReason reason        ///<  Reason for clearance of connection.
    );

    /**Clear a current call.
       This hangs up the current call. This will release all connections
       currently in the call by calling the OpalCall::Clear() function.

       Note that this function will return quickly as the release and
       disposal of the connections is done by another thread.
      */
    void ClearCall(
      CallEndReason reason = EndedByLocalUser ///<  Reason for call clearing
    );

    /**Clear a current connection, synchronously
      */
    virtual void ClearCallSynchronous(
      PSyncPoint * sync,
      CallEndReason reason = EndedByLocalUser  ///<  Reason for call clearing
    );

    /**Get the Q.931 cause code (Q.850) that terminated this call.
       See Q931::CauseValues for common values.
     */
    unsigned GetQ931Cause() const { return q931Cause; }

    /**Set the outgoing Q.931 cause code (Q.850) that is sent for this call
       See Q931::CauseValues for common values.
     */
    void SetQ931Cause(unsigned v) { q931Cause = v; }

    /**Initiate the transfer of an existing call (connection) to a new remote 
       party.

       If remoteParty is a valid call token, then the remote party is transferred
       to that party (consultation transfer) and both calls are cleared.
     */
    virtual bool TransferConnection(
      const PString & remoteParty   ///<  Remote party to transfer the existing call to
    );
    
    /**Put the current connection on hold, suspending all media streams.
     */
    virtual bool HoldConnection();

    /**Retrieve the current connection from hold, activating all media 
     * streams.
     */
    virtual bool RetrieveConnection();

    /**Return PTrue if the current connection is on hold.
     */
    virtual PBoolean IsConnectionOnHold();

    /**Call back indicating result of last hold/retrieve operation.
       This also indicates if the local connection has been put on hold by the
       remote connection.
     */
    virtual void OnHold(
      bool fromRemote,               ///<  Indicates remote has held local connection
      bool onHold                    ///<  Indicates have just been held/retrieved.
    );
  //@}

  /**@name Call progress functions */
  //@{
    /**Call back for an incoming call.
       This function is used for an application to control the answering of
       incoming calls.

       If PTrue is returned then the connection continues. If PFalse then the
       connection is aborted.

       Note this function should not block for any length of time. If the
       decision to answer the call may take some time eg waiting for a user to
       pick up the phone, then AnswerCallPending or AnswerCallDeferred should
       be returned.

       If an application overrides this function, it should generally call the
       ancestor version to complete calls. Unless the application completely
       takes over that responsibility. Generally, an application would only
       intercept this function if it wishes to do some form of logging. For
       this you can obtain the name of the caller by using the function
       OpalConnection::GetRemotePartyName().

       The default behaviour calls the OpalManager function of the same name.

       Note that the most explicit version of this override is made pure, so as to force 
       descendant classes to implement it. This will only affect code that implements new
       descendants of OpalConnection - code that uses existing descendants will be unaffected
     */
    virtual PBoolean OnIncomingConnection(unsigned int options, OpalConnection::StringOptions * stringOptions);
    virtual PBoolean OnIncomingConnection(unsigned int options);
    virtual PBoolean OnIncomingConnection();

    /**Start an outgoing connection.
       This function will initiate the connection to the remote entity, for
       example in H.323 it sends a SETUP, in SIP it sends an INVITE etc.

       The default behaviour is pure.
      */
    virtual PBoolean SetUpConnection() = 0;

    /**Callback for outgoing connection, it is invoked after SetUpConnection
       This function allows the application to set up some parameters or to log some messages
     */
    virtual PBoolean OnSetUpConnection();

    
    /**Call back for remote party being alerted.
       This function is called after the connection is informed that the
       remote endpoint is "ringing". Generally some time after the
       SetUpConnection() function was called, this is function is called.

       If PFalse is returned the connection is aborted.

       If an application overrides this function, it should generally call the
       ancestor version for correct operation. An application would typically
       only intercept this function if it wishes to do some form of logging.
       For this you can obtain the name of the caller by using the function
       OpalConnection::GetRemotePartyName().

       The default behaviour calls the OpalEndPoint function of the same name.
     */
    virtual void OnAlerting();

    /**Indicate to remote endpoint an alert is in progress.
       If this is an incoming connection and the AnswerCallResponse is in a
       AnswerCallDeferred or AnswerCallPending state, then this function is
       used to indicate to that endpoint that an alert is in progress. This is
       usually due to another connection which is in the call (the B party)
       has received an OnAlerting() indicating that its remoteendpoint is
       "ringing".

       The default behaviour is pure.
      */
    virtual PBoolean SetAlerting(
      const PString & calleeName,   ///<  Name of endpoint being alerted.
      PBoolean withMedia                ///<  Open media with alerting
    ) = 0;

    /**Call back for answering an incoming call.
       This function is called after the connection has been acknowledged
       but before the connection is established

       This gives the application time to wait for some event before
       signalling to the endpoint that the connection is to proceed. For
       example the user pressing an "Answer call" button.

       If AnswerCallDenied is returned the connection is aborted and the
       connetion specific end call PDU is sent. If AnswerCallNow is returned 
       then the connection proceeding, Finally if AnswerCallPending is returned then the
       protocol negotiations are paused until the AnsweringCall() function is
       called.

       The default behaviour calls the endpoint function of the same name.
     */
    virtual AnswerCallResponse OnAnswerCall(
      const PString & callerName        ///<  Name of caller
    );

    /**Indicate the result of answering an incoming call.
       This should only be called if the OnAnswerCall() callback function has
       returned a AnswerCallPending or AnswerCallDeferred response.

       Note sending further AnswerCallPending responses via this function will
       have the result of notification PDUs being sent to the remote endpoint (if possible).
       In this way multiple notification PDUs may be sent.

       Sending a AnswerCallDeferred response would have no effect.
      */
    virtual void AnsweringCall(
      AnswerCallResponse response ///<  Answer response to incoming call
    );

    /**A call back function whenever a connection is "connected".
       This indicates that a connection to an endpoint was connected. That
       is the endpoint received acknowledgement via whatever protocol it uses
       that the connection may now start media streams.

       In the context of H.323 this means that the CONNECT pdu has been
       received.

       The default behaviour calls the OpalEndPoint function of the same name.
      */
    virtual void OnConnected();

    /**Indicate to remote endpoint we are connected.

       The default behaviour sets the phase to ConnectedPhase, sets the
       connection start time and then checks if there is any media channels
       opened and if so, moves on to the established phase, calling
       OnEstablished().
      */
    virtual PBoolean SetConnected();

    /**A call back function whenever a connection is established.
       This indicates that a connection to an endpoint was established. This
       usually occurs after OnConnected() and indicates that the connection
       is both connected and has media flowing.

       In the context of H.323 this means that the signalling and control
       channels are open and the TerminalCapabilitySet and MasterSlave
       negotiations are complete.

       The default behaviour calls the OpalEndPoint function of the same name.
      */
    virtual void OnEstablished();

    /**Release the current connection.
       This removes the connection from the current call. The call may
       continue if there are other connections still active on it. If this was
       the last connection for the call then the call is disposed of as well.

       Note that this function will return quickly as the release and
       disposal of the connections is done by another thread.
      */
    virtual void Release(
      CallEndReason reason = EndedByLocalUser ///<  Reason for call release
    );

    /**Clean up the termination of the connection.
       This function can do any internal cleaning up and waiting on background
       threads that may be using the connection object.

       Note that there is not a one to one relationship with the
       OnEstablishedConnection() function. This function may be called without
       that function being called. For example if SetUpConnection() was used
       but the call never completed.

       Classes that override this function should make sure they call the
       ancestor version for correct operation.

       An application will not typically call this function as it is used by
       the OpalManager during a release of the connection.

       The default behaviour calls the OpalEndPoint function of the same name.
      */
    virtual void OnReleased();
  //@}

  /**@name Additional signalling functions */
  //@{
    /**Get the destination address of an incoming connection.
       This will, for example, collect a phone number from a POTS line, or
       get the fields from the H.225 SETUP pdu in a H.323 connection.

       The default behaviour returns "*", which by convention means any
       address the endpoint/connection can get to.
      */
    virtual PString GetDestinationAddress();

    /**Forward incoming call to specified address.
       This would typically be called from within the OnIncomingCall()
       function when an application wishes to redirct an unwanted incoming
       call.

       The return value is PTrue if the call is to be forwarded, PFalse
       otherwise. Note that if the call is forwarded the current connection is
       cleared with teh ended call code of EndedByCallForwarded.
      */
    virtual PBoolean ForwardCall(
      const PString & forwardParty   ///<  Party to forward call to.
    );
  //@}

  /**@name Media Stream Management */
  //@{
    /**Get the data formats this connection is capable of operating.
       This provides a list of media data format names that a
       OpalMediaStream may be created in within this connection.

       The default behaviour is pure.
      */
    virtual OpalMediaFormatList GetMediaFormats() const = 0;

    /**Get the list of data formats used for making calls
       
       The default behaviour is to call call.GetMediaFormats();
      */
    virtual OpalMediaFormatList GetLocalMediaFormats();

    /**Adjust media formats available on a connection.
       This is called by a connection after it has called
       OpalCall::GetMediaFormats() to get all media formats that it can use so
       that an application may remove or reorder the media formats before they
       are used to open media streams.

       The default behaviour calls the OpalEndPoint function of the same name.
      */
    virtual void AdjustMediaFormats(
      OpalMediaFormatList & mediaFormats  ///<  Media formats to use
    ) const;
    
    /**Open source or sink media stream for session.
      */
    virtual OpalMediaStreamPtr OpenMediaStream(
      const OpalMediaFormat & mediaFormat, ///<  Media format to open
      unsigned sessionID,                  ///<  Session to start stream on
      bool isSource                        ///< Stream is a source/sink
    );

    /**Request close of a media stream by session.
       Note that this is usually asymchronous, the OnClosedMediaStream() function is
       called when the stream is really closed.
      */
    virtual bool CloseMediaStream(
      unsigned sessionId,  ///<  Session ID to search for.
      bool source          ///<  Indicates the direction of stream.
    );

    /**Request close of a specific media stream.
       Note that this is usually asymchronous, the OnClosedMediaStream() function is
       called when the stream is really closed.
      */
    virtual bool CloseMediaStream(
      OpalMediaStream & stream  ///< Stream to close
    );

    /**Remove the specified media stream from the list of streams for this channel.
       This will automatically delete the stream if the stream was found in the
       stream list.

      Returns true if the media stream was removed the list and deleted, else
      returns false if the media stream was unchanged
      */
    bool RemoveMediaStream(
      OpalMediaStream & strm     // media stream to remove
    );

    /**Start all media streams for connection.
      */
    virtual void StartMediaStreams();
    
    /**Request close all media streams on connection.
      */
    virtual void CloseMediaStreams();
    
    /**Pause media streams for connection.
      */
    virtual void PauseMediaStreams(PBoolean paused);

    /**Create a new media stream.
       This will create a media stream of an appropriate subclass as required
       by the underlying connection protocol. For instance H.323 would create
       an OpalRTPStream.

       The sessionID parameter may not be needed by a particular media stream
       and may be ignored. In the case of an OpalRTPStream it us used.

       Note that media streams may be created internally to the underlying
       protocol. This function is not the only way a stream can come into
       existance.
     */
    virtual OpalMediaStream * CreateMediaStream(
      const OpalMediaFormat & mediaFormat, ///<  Media format for stream
      unsigned sessionID,                  ///<  Session number for stream
      PBoolean isSource                        ///<  Is a source stream
    );

    /**Get a media stream.
       Locates a stream given a RTP session ID. Each session would usually
       have two media streams associated with it, so the source flag
       may be used to distinguish which channel to return.
      */
    OpalMediaStreamPtr GetMediaStream(
      unsigned sessionId,  ///<  Session ID to search for.
      bool source          ///<  Indicates the direction of stream.
    ) const;

    /**Get a media stream.
       Locates a stream given a media type. Each session would usually
       have two media streams associated with it, so the source flag
       may be used to distinguish which channel to return.
      */
    OpalMediaStreamPtr GetMediaStream(
      const OpalMediaType & mediaType,  ///<  Media type to search for.
      bool source          ///<  Indicates the direction of stream.
    ) const;

    /**Call back when opening a media stream.
       This function is called when a connection has created a new media
       stream according to the logic of its underlying protocol.

       The usual requirement is that media streams are created on all other
       connections participating in the call and all of the media streams are
       attached to an instance of an OpalMediaPatch object that will read from
       one of the media streams passing data to the other media streams.

       The default behaviour calls the OpalEndPoint function of the same name.
      */
    virtual PBoolean OnOpenMediaStream(
      OpalMediaStream & stream    ///<  New media stream being opened
    );

    /**Call back for closed a media stream.

       The default behaviour calls the OpalEndPoint function of the same name.
      */
    virtual void OnClosedMediaStream(
      const OpalMediaStream & stream     ///<  Media stream being closed
    );
    
    /**Call back when patching a media stream.
       This function is called when a connection has created a new media
       patch between two streams.
      */
    virtual void OnPatchMediaStream(
      PBoolean isSource,
      OpalMediaPatch & patch    ///<  New patch
    );
	
    /**Attaches the RFC 2833 handler to the media patch
       This method may be called from subclasses, e.g. within
       OnPatchMediaStream()
      */
    virtual void AttachRFC2833HandlerToPatch(PBoolean isSource, OpalMediaPatch & patch);

    /**See if the media can bypass the local host.

       The default behaviour returns PFalse indicating that media bypass is not
       possible.
     */
    virtual PBoolean IsMediaBypassPossible(
      unsigned sessionID                  ///<  Session ID for media channel
    ) const;

#if OPAL_VIDEO

    /**Add video media formats available on a connection.

       The default behaviour calls the OpalEndPoint function of the same name.
      */
    virtual void AddVideoMediaFormats(
      OpalMediaFormatList & mediaFormats  ///<  Media formats to use
    ) const;

    /**Create an PVideoInputDevice for a source media stream.
      */
    virtual PBoolean CreateVideoInputDevice(
      const OpalMediaFormat & mediaFormat,  ///<  Media format for stream
      PVideoInputDevice * & device,         ///<  Created device
      PBoolean & autoDelete                     ///<  Flag for auto delete device
    );

    /**Create an PVideoOutputDevice for a sink media stream or the preview
       display for a source media stream.
      */
    virtual PBoolean CreateVideoOutputDevice(
      const OpalMediaFormat & mediaFormat,  ///<  Media format for stream
      PBoolean preview,                         ///<  Flag indicating is a preview output
      PVideoOutputDevice * & device,        ///<  Created device
      PBoolean & autoDelete                     ///<  Flag for auto delete device
    );
#endif 

    /**Set the volume (gain) for the audio media channel to the specified percentage.
      */
    virtual PBoolean SetAudioVolume(
      PBoolean source,                  ///< true for source (microphone), false for sink (speaker)
      unsigned percentage           ///< Gain, 0=silent, 100=maximun
    );

    /**Get the average signal level (0..32767) for the audio media channel.
       A return value of UINT_MAX indicates no valid signal, eg no audio channel opened.
      */
    virtual unsigned GetAudioSignalLevel(
      PBoolean source                   ///< true for source (microphone), false for sink (speaker)
    );
  //@}

  /**@name Bandwidth Management */
  //@{
    /**Get the available bandwidth in 100's of bits/sec.
      */
    unsigned GetBandwidthAvailable() const { return bandwidthAvailable; }

    /**Set the available bandwidth in 100's of bits/sec.
       Note if the force parameter is PTrue this function will close down
       active media streams to meet the new bandwidth requirement.
      */
    virtual PBoolean SetBandwidthAvailable(
      unsigned newBandwidth,    ///<  New bandwidth limit
      PBoolean force = PFalse        ///<  Force bandwidth limit
    );

    /**Get the bandwidth currently used.
       This totals the bandwidth used by open streams and returns the total
       bandwidth used in 100's of bits/sec
      */
    virtual unsigned GetBandwidthUsed() const;

    /**Set the used bandwidth in 100's of bits/sec.
       This is an internal function used by the OpalMediaStream bandwidth
       management code.

       If there is insufficient bandwidth available, PFalse is returned. If
       sufficient bandwidth is available, then PTrue is returned and the amount
       of available bandwidth is reduced by the specified amount.
      */
    virtual PBoolean SetBandwidthUsed(
      unsigned releasedBandwidth,   ///<  Bandwidth to release
      unsigned requiredBandwidth    ///<  Bandwidth required
    );
  //@}

  /**@name User input */
  //@{
    enum SendUserInputModes {
      SendUserInputAsQ931,
      SendUserInputAsString,
      SendUserInputAsTone,
      SendUserInputAsInlineRFC2833,
      SendUserInputAsSeparateRFC2833,  // Not implemented
      SendUserInputAsProtocolDefault,
      NumSendUserInputModes
    };
#if PTRACING
    friend ostream & operator<<(ostream & o, SendUserInputModes m);
#endif

    /**Set the user input indication transmission mode.
      */
    virtual void SetSendUserInputMode(SendUserInputModes mode);

    /**Get the user input indication transmission mode.
      */
    virtual SendUserInputModes GetSendUserInputMode() const { return sendUserInputMode; }

    /**Get the real user input indication transmission mode.
       This will return the user input mode that will actually be used for
       transmissions. It will be the value of GetSendUserInputMode() provided
       the remote endpoint is capable of that mode.
      */
    virtual SendUserInputModes GetRealSendUserInputMode() const { return GetSendUserInputMode(); }

    /**Send a user input indication to the remote endpoint.
       This is for sending arbitrary strings as user indications.

       The default behaviour is to call SendUserInputTone() for each character
       in the string.
      */
    virtual PBoolean SendUserInputString(
      const PString & value                   ///<  String value of indication
    );

    /**Send a user input indication to the remote endpoint.
       This sends DTMF emulation user input. If something more sophisticated
       than the simple tones that can be sent using the SendUserInput()
       function.

       A duration of zero indicates that no duration is to be indicated.
       A non-zero logical channel indicates that the tone is to be syncronised
       with the logical channel at the rtpTimestamp value specified.

       The tone parameter must be one of "0123456789#*ABCD!" where '!'
       indicates a hook flash. If tone is a ' ' character then a
       signalUpdate PDU is sent that updates the last tone indication
       sent. See the H.245 specifcation for more details on this.

       The default behaviour sends the tone using RFC2833.
      */
    virtual PBoolean SendUserInputTone(
      char tone,        ///<  DTMF tone code
      unsigned duration = 0  ///<  Duration of tone in milliseconds
    );

    /**Call back for remote enpoint has sent user input as a string.
       This will be called irrespective of the source (H.245 string, H.245
       signal or RFC2833).

       The default behaviour calls the endpoint function of the same name.
      */
    virtual void OnUserInputString(
      const PString & value   ///<  String value of indication
    );

    /**Call back for remote enpoint has sent user input.
       If duration is zero then this indicates the beginning of the tone. If
       duration is non-zero then it indicates the end of the tone output.

       The default behaviour calls the OpalEndPoint function of the same name.
      */
    virtual void OnUserInputTone(
      char tone,
      unsigned duration
    );

    /**Send a user input indication to the remote endpoint.
       This sends a Hook Flash emulation user input.
      */
    void SendUserInputHookFlash(
      unsigned duration = 500  ///<  Duration of tone in milliseconds
    ) { SendUserInputTone('!', duration); }

    /**Get a user input indication string, waiting until one arrives.
      */
    virtual PString GetUserInput(
      unsigned timeout = 30   ///<  Timeout in seconds on input
    );

    /**Set a user indication string.
       This allows the GetUserInput() function to unblock and return this
       string.
      */
    virtual void SetUserInput(
      const PString & input     ///<  Input string
    );

    /**Read a sequence of user indications with timeouts.
      */
    virtual PString ReadUserInput(
      const char * terminators = "#\r\n", ///<  Characters that can terminte input
      unsigned lastDigitTimeout = 4,      ///<  Timeout on last digit in string
      unsigned firstDigitTimeout = 30     ///<  Timeout on receiving any digits
    );

    /**Play a prompt to the connection before rading user indication string.

       For example the LID connection would play a dial tone.

       The default behaviour does nothing.
      */
    virtual PBoolean PromptUserInput(
      PBoolean play   ///<  Flag to start or stop playing the prompt
    );
  //@}

    /** Execute garbage collection for endpoint.
        Returns PTrue if all garbage has been collected.
        Default behaviour deletes the objects in the connectionsActive list.
      */
    virtual bool GarbageCollection();
  //@}

  /**@name Member variable access */
  //@{
    /**Get the owner endpoint for this connection.
     */
    OpalEndPoint & GetEndPoint() const { return endpoint; }
    
    /**Get the owner call for this connection.
     */
    OpalCall & GetCall() const { return ownerCall; }

    /**Get the token for this connection.
     */
    const PString & GetToken() const { return callToken; }

    /**Get the call direction for this connection.
     */
    PBoolean IsOriginating() const { return originating; }

    /**Get the time at which the connection was begun
      */
    PTime GetSetupUpTime() const { return setupTime; }

    /**Get the time at which the ALERTING was received
      */
    PTime GetAlertingTime() const { return alertingTime; }

    /**Get the time at which the connection was established
      */
    PTime GetConnectionStartTime() const { return connectedTime; }

    /**Get the time at which the connection was cleared
      */
    PTime GetConnectionEndTime() const { return callEndTime; }

    /**Get the product info for all endpoints.
      */
    const OpalProductInfo & GetProductInfo() const { return productInfo; }

    /**Set the product info for all endpoints.
      */
    void SetProductInfo(
      const OpalProductInfo & info
    ) { productInfo = info; }

    /**Get the local name/alias.
      */
    const PString & GetLocalPartyName() const { return localPartyName; }

    /**Set the local name/alias.
      */
    virtual void SetLocalPartyName(const PString & name);

    /**Get the local name/alias.
      */
    virtual PString GetLocalPartyURL() const;

    /**Get the local display name.
      */
    const PString & GetDisplayName() const { return displayName; }

    /**Set the local display name.
      */
    void SetDisplayName(const PString & name) { displayName = name; }

    /**Get the caller name/alias.
      */
    const PString & GetRemotePartyName() const { return remotePartyName; }

    /**Set the caller name/alias.
      */
    void SetRemotePartyName(const PString & name) { remotePartyName = name; }

    /**Get the remote application description. This is for backward
       compatibility and has been supercedded by GeREmoteProductInfo();
      */
    PCaselessString GetRemoteApplication() const { return remoteProductInfo.AsString(); }

    /** Get the remote product info.
      */
    const OpalProductInfo & GetRemoteProductInfo() const { return remoteProductInfo; }
    
    /**Get the remote party number, if there was one one.
       If the remote party has indicated an e164 number as one of its aliases
       or as a field in the Q.931 PDU, then this function will return it.
      */
    const PString & GetRemotePartyNumber() const { return remotePartyNumber; }

    /**Get the remote party address.
      */
    const PString & GetRemotePartyAddress() const { return remotePartyAddress; }

    /**Set the remote party address.
      */
    void SetRemotePartyAddress(const PString & addr) { remotePartyAddress = addr; }

    /**Get the remote party address.
       This will return the "best guess" at an address to use in a
       to call the user again later.
      */
    virtual PString GetRemotePartyURL() const;

    // Deprecated - backward compatibility only
    const PString GetRemotePartyCallbackURL() const { return GetRemotePartyURL(); }


    /**Get the called number (for incoming calls). This is useful for gateway
       applications where the destination number may not be the same as the local number
      */
    virtual const PString & GetCalledDestinationNumber() const { return calledDestinationNumber; }

    /**Get the called name (for incoming calls). This is useful for gateway
       applications where the destination name may not be the same as the local username
      */
    virtual const PString & GetCalledDestinationName() const { return calledDestinationName; }

    /**Get the called URL (for incoming calls). This is useful for gateway
       applications where the destination number may not be the same as the local number
      */
    virtual const PString & GetCalledDestinationURL() const { return calledDestinationURL; }

    /**Get the default maximum audio jitter delay parameter.
       Defaults to 50ms
     */
    unsigned GetMinAudioJitterDelay() const { return minAudioJitterDelay; }

    /**Get the default maximum audio delay jitter parameter.
       Defaults to 250ms.
     */
    unsigned GetMaxAudioJitterDelay() const { return maxAudioJitterDelay; }

    /**Set the maximum audio delay jitter parameter.
     */
    void SetAudioJitterDelay(
      unsigned minDelay,   ///<  New minimum jitter buffer delay in milliseconds
      unsigned maxDelay    ///<  New maximum jitter buffer delay in milliseconds
    );

    /**Get the silence detector active on connection.
     */
    OpalSilenceDetector * GetSilenceDetector() const { return silenceDetector; }
    
    /**Get the echo canceler active on connection.
    */
    OpalEchoCanceler * GetEchoCanceler() const { return echoCanceler; }

    /**Get the protocol-specific unique identifier for this connection.
     */
    virtual const OpalGloballyUniqueID & GetIdentifier() const
    { return callIdentifier; }

    /**Get the maximum RTP payload size. This function allows a user to
       override the value returned on a connection by connection basis, for
       example knowing the connection is on a LAN with ethernet MTU the
       payload size could be increased.

       Defaults to the value returned by the OpalManager function of the same
       name.
      */
    virtual PINDEX GetMaxRtpPayloadSize() const;

    virtual OpalTransport & GetTransport() const
    { return *(OpalTransport *)NULL; }

    PDICTIONARY(MediaAddressesDict, POrdinalKey, OpalTransportAddress);
    MediaAddressesDict & GetMediaTransportAddresses()
    { return mediaTransportAddresses; }

#ifdef OPAL_STATISTICS
    /**Get Video Update requests statistic.
      */
    unsigned GetVideoUpdateRequestsSent() const { return m_VideoUpdateRequestsSent; }
#endif
  //@}

    const RTP_DataFrame::PayloadMapType & GetRTPPayloadMap() const
    { return rtpPayloadMap; }

    StringOptions * GetStringOptions() const
    { return stringOptions; }

    void SetStringOptions(StringOptions * options);

    /**Open the media channels associated with an incoming call
 
       This function is provided to allow an OpalConnection descendant to delay the opening of the
       media channels associated with an incoming call. By default, this function is called as soon
       as possible after an incoming connection request has been received (i.e. an INVITE or SETUP) and
       and do all of the work associated with handling SDP or fastStart eyc

       By overriding this function, the media channel open is deferred, which gives the other connections
       in the call a chance to do something before the media channels are started. This allows for features
       beyond those provided by the OnAnswer interface
      */
    virtual PBoolean OnOpenIncomingMediaChannels();

    virtual void ApplyStringOptions();

    virtual void PreviewPeerMediaFormats(const OpalMediaFormatList & fmts);

    virtual void EnableRecording();
    virtual void DisableRecording();

    virtual void OnMediaPatchStart(
      unsigned sessionId, 
          bool isSource
    );
    virtual void OnMediaPatchStop(
      unsigned sessionId, 
          bool isSource
    );

    /** Notifier function for OpalVideoUpdatePicture.
        Calls the SendIntraFrameRequest on the rtp session
      */
    PDECLARE_NOTIFIER(OpalMediaCommand, OpalConnection, OnMediaCommand);

  protected:
    void OnConnectedInternal();

#if P_DTMF
    PDECLARE_NOTIFIER(RTP_DataFrame, OpalConnection, OnUserInputInBandDTMF);
#endif
    PDECLARE_NOTIFIER(PThread, OpalConnection, OnReleaseThreadMain);
    PDECLARE_NOTIFIER(RTP_DataFrame, OpalConnection, OnRecordAudio);

  // Member variables
    OpalCall             & ownerCall;
    OpalEndPoint         & endpoint;

  private:
    PMutex               phaseMutex;
    Phases               phase;

  protected:
    PString              callToken;
    OpalGloballyUniqueID callIdentifier;
    PBoolean             originating;
    PTime                setupTime;
    PTime                alertingTime;
    PTime                connectedTime;
    PTime                callEndTime;
    OpalProductInfo      productInfo;
    PString              localPartyName;
    PString              displayName;
    PString              remotePartyName;
    OpalProductInfo      remoteProductInfo;
    PString              remotePartyNumber;
    PString              remotePartyAddress;
    CallEndReason        callEndReason;
    bool                 synchronousOnRelease;
    PString              calledDestinationNumber;
    PString              calledDestinationName;
    PString              calledDestinationURL;

    SendUserInputModes    sendUserInputMode;
    PString               userInputString;
    PSyncPoint            userInputAvailable;
    PBoolean              detectInBandDTMF;
    unsigned              dtmfScaleMultiplier;
    unsigned              dtmfScaleDivisor;
    unsigned              q931Cause;

    OpalSilenceDetector * silenceDetector;
    OpalEchoCanceler    * echoCanceler;

    MediaAddressesDict         mediaTransportAddresses;
    PSafeList<OpalMediaStream> mediaStreams;

    unsigned            minAudioJitterDelay;
    unsigned            maxAudioJitterDelay;
    unsigned            bandwidthAvailable;

    RTP_DataFrame::PayloadMapType rtpPayloadMap;

    // The In-Band DTMF detector. This is used inside an audio filter which is
    // added to the audio channel.
#if P_DTMF
    PDTMFDecoder        dtmfDecoder;
#endif

    /**Set the phase of the connection.
       @param phaseToSet the phase to set
      */
    void SetPhase(Phases phaseToSet);

#if PTRACING
    friend ostream & operator<<(ostream & o, Phases p);
#endif

    StringOptions * stringOptions;
    PString recordAudioFilename;

#ifdef OPAL_STATISTICS
    unsigned m_VideoUpdateRequestsSent;
#endif
};

#endif // __OPAL_CONNECTION_H


// End of File ///////////////////////////////////////////////////////////////
