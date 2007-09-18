/*
 * manager.h
 *
 * OPAL system manager.
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
 * $Log: manager.h,v $
 * Revision 1.2066  2007/09/18 09:37:52  rjongbloed
 * Propagated call backs for RTP statistics through OpalManager and OpalCall.
 *
 * Revision 2.64  2007/07/20 05:49:57  rjongbloed
 * Added member variable for stun server name in OpalManager, so can be remembered
 *   in original form so change in IP address or temporary STUN server failures do not
 *   lose the server being selected by the user.
 *
 * Revision 2.63  2007/06/29 06:59:56  rjongbloed
 * Major improvement to the "product info", normalising H.221 and User-Agent mechanisms.
 *
 * Revision 2.62  2007/05/15 07:26:38  csoutheren
 * Remove deprecated  interface to STUN server in H323Endpoint
 * Change UseNATForIncomingCall to IsRTPNATEnabled
 * Various cleanups of messy and unused code
 *
 * Revision 2.61  2007/05/15 05:24:50  csoutheren
 * Add UseNATForIncomingCall override so applications can optionally implement their own NAT activation strategy
 *
 * Revision 2.60  2007/05/07 14:13:51  csoutheren
 * Add call record capability
 *
 * Revision 2.59  2007/04/18 00:00:44  csoutheren
 * Add hooks for recording call audio
 *
 * Revision 2.58  2007/03/13 00:32:16  csoutheren
 * Simple but messy changes to allow compile time removal of protocol
 * options such as H.450 and H.460
 * Fix MakeConnection overrides
 *
 * Revision 2.57  2007/03/01 03:37:37  csoutheren
 * Fix typo
 *
 * Revision 2.56  2007/01/25 11:48:10  hfriederich
 * OpalMediaPatch code refactorization.
 * Split into OpalMediaPatch (using a thread) and OpalPassiveMediaPatch
 * (not using a thread). Also adds the possibility for source streams
 * to push frames down to the sink streams instead of having a patch
 * thread around.
 *
 * Revision 2.55  2007/01/18 04:45:16  csoutheren
 * Messy, but simple change to add additional options argument to OpalConnection constructor
 * This allows the provision of non-trivial arguments for connections
 *
 * Revision 2.54  2006/12/18 03:18:41  csoutheren
 * Messy but simple fixes
 *   - Add access to SIP REGISTER timeout
 *   - Ensure OpalConnection options are correctly progagated
 *
 * Revision 2.53  2006/12/08 04:22:06  csoutheren
 * Applied 1589261 - new release cause for fxo endpoints
 * Thanks to Frederic Heem
 *
 * Revision 2.52  2006/11/20 03:37:12  csoutheren
 * Allow optional inclusion of RTP aggregation
 *
 * Revision 2.51  2006/11/19 06:02:58  rjongbloed
 * Moved function that reads User Input into a destination address to
 *   OpalManager so can be easily overidden in applications.
 *
 * Revision 2.50  2006/10/10 07:18:18  csoutheren
 * Allow compilation with and without various options
 *
 * Revision 2.49  2006/09/28 07:42:17  csoutheren
 * Merge of useful SRTP implementation
 *
 * Revision 2.48  2006/06/21 23:43:27  csoutheren
 * Fixed comment on OpalManager::SetTranslationAddress
 *
 * Revision 2.47  2006/06/21 04:54:15  csoutheren
 * Fixed build with latest PWLib
 *
 * Revision 2.46  2006/04/20 16:52:22  hfriederich
 * Adding support for H.224/H.281
 *
 * Revision 2.45  2005/11/30 13:35:26  csoutheren
 * Changed tags for Doxygen
 *
 * Revision 2.44  2005/11/24 20:31:54  dsandras
 * Added support for echo cancelation using Speex.
 * Added possibility to add a filter to an OpalMediaPatch for all patches of a connection.
 *
 * Revision 2.43  2005/10/08 19:26:38  dsandras
 * Added OnForwarded callback in case of call forwarding.
 *
 * Revision 2.42  2005/09/20 07:25:43  csoutheren
 * Allow userdata to be passed from SetupCall through to Call and Connection constructors
 *
 * Revision 2.41  2005/08/24 10:43:51  rjongbloed
 * Changed create video functions yet again so can return pointers that are not to be deleted.
 *
 * Revision 2.40  2005/08/23 12:45:09  rjongbloed
 * Fixed creation of video preview window and setting video grab/display initial frame size.
 *
 * Revision 2.39  2005/07/14 08:51:18  csoutheren
 * Removed CreateExternalRTPAddress - it's not needed because you can override GetMediaAddress
 * to do the same thing
 * Fixed problems with logic associated with media bypass
 *
 * Revision 2.38  2005/07/11 06:52:16  csoutheren
 * Added support for outgoing calls using external RTP
 *
 * Revision 2.37  2005/07/11 01:52:24  csoutheren
 * Extended AnsweringCall to work for SIP as well as H.323
 * Fixed problems with external RTP connection in H.323
 * Added call to OnClosedMediaStream
 *
 * Revision 2.36  2005/07/09 06:57:20  rjongbloed
 * Changed SetSTUNServer so returns the determined NAT type.
 *
 * Revision 2.35  2005/06/13 23:13:53  csoutheren
 * Fixed various typos thanks to Julien PUYDT
 *
 * Revision 2.34  2005/05/25 17:04:08  dsandras
 * Fixed ClearAllCalls when being executed synchronously. Fixes a crash on exit when a call is in progress. Thanks Robert!
 *
 * Revision 2.33  2005/04/10 20:46:56  dsandras
 * Added callback that is called when a connection is put on hold (locally or remotely).
 *
 * Revision 2.32  2005/01/31 07:37:09  csoutheren
 * Fixed problem with compiling under gcc 3.4
 * Thanks to Peter Robinson
 *
 * Revision 2.31  2004/12/21 08:24:31  dsandras
 * The dictionnary key is a PString, not an OpalGloballyUniqueID. Fixes problem when doing several calls at the same time.
 *
 * Revision 2.30  2004/08/18 13:02:48  rjongbloed
 * Changed to make calling OPalManager::OnClearedCall() in override optional.
 *
 * Revision 2.29  2004/08/14 07:56:29  rjongbloed
 * Major revision to utilise the PSafeCollection classes for the connections and calls.
 *
 * Revision 2.28  2004/07/14 13:26:14  rjongbloed
 * Fixed issues with the propagation of the "established" phase of a call. Now
 *   calling an OnEstablished() chain like OnAlerting() and OnConnected() to
 *   finally arrive at OnEstablishedCall() on OpalManager
 *
 * Revision 2.27  2004/07/11 12:42:10  rjongbloed
 * Added function on endpoints to get the list of all media formats any
 *   connection the endpoint may create can support.
 *
 * Revision 2.26  2004/07/04 12:50:50  rjongbloed
 * Added some access functions to manager lists
 *
 * Revision 2.25  2004/06/05 14:34:44  rjongbloed
 * Added functions to set the auto start video tx/rx flags.
 *
 * Revision 2.24  2004/05/24 13:40:12  rjongbloed
 * Added default values for silence suppression parameters.
 *
 * Revision 2.23  2004/05/15 12:53:03  rjongbloed
 * Added default username and display name to manager, for all endpoints.
 *
 * Revision 2.22  2004/04/18 07:09:12  rjongbloed
 * Added a couple more API functions to bring OPAL into line with similar functions in OpenH323.
 *
 * Revision 2.21  2004/03/11 06:54:27  csoutheren
 * Added ability to disable SIP or H.323 stacks
 *
 * Revision 2.20  2004/02/24 11:37:01  rjongbloed
 * More work on NAT support, manual external address translation and STUN
 *
 * Revision 2.19  2004/02/19 10:47:01  rjongbloed
 * Merged OpenH323 version 1.13.1 changes.
 *
 * Revision 2.18  2004/01/18 15:36:07  rjongbloed
 * Added stun support
 *
 * Revision 2.17  2003/03/17 10:26:59  robertj
 * Added video support.
 *
 * Revision 2.16  2003/03/06 03:57:47  robertj
 * IVR support (work in progress) requiring large changes everywhere.
 *
 * Revision 2.15  2002/11/10 11:33:17  robertj
 * Updated to OpenH323 v1.10.3
 *
 * Revision 2.14  2002/09/16 02:52:35  robertj
 * Added #define so can select if #pragma interface/implementation is used on
 *   platform basis (eg MacOS) rather than compiler, thanks Robert Monaghan.
 *
 * Revision 2.13  2002/09/06 02:44:09  robertj
 * Added routing table system to route calls by regular expressions.
 *
 * Revision 2.12  2002/09/04 06:01:47  robertj
 * Updated to OpenH323 v1.9.6
 *
 * Revision 2.11  2002/07/01 04:56:31  robertj
 * Updated to OpenH323 v1.9.1
 *
 * Revision 2.10  2002/04/10 08:15:31  robertj
 * Added functions to set ports.
 *
 * Revision 2.9  2002/02/19 07:42:50  robertj
 * Restructured media bypass functions to fix problems with RFC2833.
 *
 * Revision 2.8  2002/02/11 07:38:55  robertj
 * Added media bypass for streams between compatible protocols.
 *
 * Revision 2.7  2002/01/22 05:04:58  robertj
 * Revamp of user input API triggered by RFC2833 support
 *
 * Revision 2.6  2001/12/07 08:56:19  robertj
 * Renamed RTP to be more general UDP port base, and TCP to be IP.
 *
 * Revision 2.5  2001/11/14 01:31:55  robertj
 * Corrected placement of adjusting media format list.
 *
 * Revision 2.4  2001/11/13 06:25:56  robertj
 * Changed SetUpConnection() so returns BOOL as returning
 *   pointer to connection is not useful.
 *
 * Revision 2.3  2001/08/23 05:51:17  robertj
 * Completed implementation of codec reordering.
 *
 * Revision 2.2  2001/08/17 08:23:59  robertj
 * Changed OnEstablished() to OnEstablishedCall() to be consistent.
 * Moved call end reasons enum from OpalConnection to global.
 *
 * Revision 2.1  2001/08/01 05:52:55  robertj
 * Moved media formats list from endpoint to connection.
 * Added function to adjust calls media formats list.
 *
 * Revision 2.0  2001/07/27 15:48:24  robertj
 * Conversion of OpenH323 to Open Phone Abstraction Library (OPAL)
 *
 */

#ifndef __OPAL_MANAGER_H
#define __OPAL_MANAGER_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include <opal/buildopts.h>

#include <opal/call.h>
#include <opal/connection.h> //OpalConnection::AnswerCallResponse
#include <opal/guid.h>
#include <opal/audiorecord.h>
#include <codec/silencedetect.h>
#include <codec/echocancel.h>
#include <ptclib/pstun.h>

#if OPAL_VIDEO
#include <ptlib/videoio.h>
#endif

class OpalEndPoint;
class OpalMediaPatch;
class OpalH224Handler;
class OpalH281Handler;


/**This class is the central manager for OPAL.
   The OpalManager embodies the root of the tree of objects that constitute an
   OPAL system. It contains all of the endpoints that make up the system. Other
   entities such as media streams etc are in turn contained in these objects.
   It is expected that an application would only ever have one instance of
   this class, and also descend from it to override call back functions.

   The manager is the eventual destination for call back indications from
   various other objects. It is possible, for instance, to get an indication
   of a completed call by creating a descendant of the OpalCall and overriding
   the OnClearedCall() virtual. However, this could quite unwieldy for all of
   the possible classes, so the default behaviour is to call the equivalent
   function on the OpalManager. This allows most applications to only have to
   create a descendant of the OpalManager and override virtual functions there
   to get all the indications it needs.
 */
class OpalManager : public PObject
{
    PCLASSINFO(OpalManager, PObject);
  public:
  /**@name Construction */
  //@{
    /**Create a new manager.
     */
    OpalManager();

    /**Destroy the manager.
       This will clear all calls, then delete all endpoints still attached to
       the manager.
     */
    ~OpalManager();
  //@}

  /**@name Endpoint management */
  //@{
    /**Attach a new endpoint to the manager.
       This is an internal function called by the OpalEndPoint constructor.

       Note that any endpoint is automatically "owned" by the manager. They
       should not be deleted directly. The RemoveEndPoint() command should be
       used to do this.
      */
    void AttachEndPoint(
      OpalEndPoint * endpoint
    );

    /**Remove an endpoint from the manager.
       This will delete the endpoint object.
      */
    void DetachEndPoint(
      OpalEndPoint * endpoint
    );

    /**Find an endpoint instance that is using the specified prefix.
      */
    OpalEndPoint * FindEndPoint(
      const PString & prefix
    );

    /**Get the endpoints attached to this manager.
      */
    const PList<OpalEndPoint> & GetEndPoints() const { return endpoints; }
  //@}

  /**@name Call management */
  //@{
    /**Set up a call between two parties.
       This is used to initiate a call. Incoming calls are "answered" using a
       different mechanism.

       The A party and B party strings indicate the protocol and address of
       the party to call in the style of a URL. The A party is the initiator
       of the call and the B party is the remote system being called. See the
       MakeConnection() function for more details on the format of these
       strings.

       The token returned is a unique identifier for the call that allows an
       application to gain access to the call at later time. This is necesary
       as any pointer being returned could become invalid (due to being
       deleted) at any time due to the multithreaded nature of the OPAL
       system. While this function does return a pointer it is only safe to
       use immediately after the function returns. At any other time the
       FindCallWithLock() function should be used to gain a locked pointer
       to the call.
     */
    virtual BOOL SetUpCall(
      const PString & partyA,       ///<  The A party of call
      const PString & partyB,       ///<  The B party of call
      PString & token,              ///<  Token for call
      void * userData = NULL,       ///<  user data passed to Call and Connection
      unsigned options = 0,         ///<  options passed to connection
      OpalConnection::StringOptions * stringOptions = NULL   ///<  complex string options passed to call
    );

    /**A call back function whenever a call is completed.
       In telephony terminology a completed call is one where there is an
       established link between two parties.

       This called from the OpalCall::OnEstablished() function.

       The default behaviour does nothing.
      */
    virtual void OnEstablishedCall(
      OpalCall & call   ///<  Call that was completed
    );

    /**Determine if a call is active.
       Return TRUE if there is an active call with the specified token. Note
       that the call could clear any time (even milliseconds) after this
       function returns TRUE.
      */
    virtual BOOL HasCall(
      const PString & token  ///<  Token for identifying call
    ) { return activeCalls.FindWithLock(token, PSafeReference) != NULL; }

    /**Determine if a call is established.
       Return TRUE if there is an active call with the specified token and
       that call has at least two parties with media flowing between them.
       Note that the call could clear any time (even milliseconds) after this
       function returns TRUE.
      */
    virtual BOOL IsCallEstablished(
      const PString & token  ///<  Token for identifying call
    );

    /**Find a call with the specified token.
       This searches the manager database for the call that contains the token
       as provided by functions such as SetUpCall().

       Note the caller of this function MUST call the OpalCall::Unlock()
       function if this function returns a non-NULL pointer. If it does not
       then a deadlock can occur.
      */
    PSafePtr<OpalCall> FindCallWithLock(
      const PString & token,  ///<  Token to identify connection
      PSafetyMode mode = PSafeReadWrite
    ) { return activeCalls.FindWithLock(token, mode); }

    /**Clear a call.
       This finds the call by using the token then calls the OpalCall::Clear()
       function on it. All connections are released, and the connections and
       call are disposed of. Note that this function returns quickly and the
       disposal happens at some later time in a background thread. It is safe
       to call this function from anywhere.
      */
    virtual BOOL ClearCall(
      const PString & token,    ///<  Token for identifying connection
      OpalConnection::CallEndReason reason = OpalConnection::EndedByLocalUser, ///<  Reason for call clearing
      PSyncPoint * sync = NULL  ///<  Sync point to wait on.
    );

    /**Clear a call.
       This finds the call by using the token then calls the OpalCall::Clear()
       function on it. All connections are released, and the connections and
       caller disposed of. Note that this function waits until the call has
       been cleared and all responses timeouts etc completed. Care must be
       used as to when it is called as deadlocks may result.
      */
    virtual BOOL ClearCallSynchronous(
      const PString & token,    ///<  Token for identifying connection
      OpalConnection::CallEndReason reason = OpalConnection::EndedByLocalUser ///<  Reason for call clearing
    );

    /**Clear all current calls.
       This effectively executes OpalCall::Clear() on every call that the
       manager has active.
       This function can not be called from several threads at the same time.
      */
    virtual void ClearAllCalls(
      OpalConnection::CallEndReason reason = OpalConnection::EndedByLocalUser, ///<  Reason for call clearing
      BOOL wait = TRUE   ///<  Flag to wait for calls to e cleared.
    );

    /**A call back function whenever a call is cleared.
       A call is cleared whenever there is no longer any connections attached
       to it. This function is called just before the call is deleted.
       However, it may be used to display information on the call after
       completion, eg the call parties and duration.

       Note that there is not a one to one relationship with the
       OnEstablishedCall() function. This function may be called without that
       function being called. For example if MakeConnection() was used but
       the call never completed.

       The default behaviour removes the call from the activeCalls dictionary.
      */
    virtual void OnClearedCall(
      OpalCall & call   ///<  Connection that was established
    );

    /**Create a call object.
       This function allows an application to have the system create
       desccendants of the OpalCall class instead of instances of that class
       directly. The application can thus override call backs or add extra
       information that it wishes to maintain on a call by call basis.

       The default behavious returns an instance of OpalCall.
      */
    virtual OpalCall * CreateCall();
    virtual OpalCall * CreateCall(
      void * userData            ///<  user data passed to SetUpCall
    );

    /**Destroy a call object.
       This gets called from background thread that garbage collects all calls
       and connections. If an application has object lifetime issues with the
       threading, it can override this function and take responsibility for
       deleting the object at some later time.

       The default behaviour simply calls "delete call".
      */
    virtual void DestroyCall(
      OpalCall * call
    );

    /**Get next unique token ID for calls.
       This is an internal function called by the OpalCall constructor.
      */
    PString GetNextCallToken();
  //@}

  /**@name Connection management */
  //@{
    /**Set up a connection to a remote party.
       An appropriate protocol (endpoint) is determined from the party
       parameter. That endpoint is then called to create a connection and that
       connection is attached to the call provided.
       
       If the endpoint is already occupied in a call then the endpoints list
       is further searched for additional endpoints that support the protocol.
       For example multiple pstn endpoints may be present for multiple LID's.
       
       The general form for this party parameter is:

            [proto:][alias@][transport$]address[:port]

       where the various fields will have meanings specific to the endpoint
       type. For example, with H.323 it could be "h323:Fred@site.com" which
       indicates a user Fred at gatekeeper size.com. Whereas for the PSTN
       endpoint it could be "pstn:5551234" which is to call 5551234 on the
       first available PSTN line.

       The default for the proto is the name of the protocol for the first
       endpoint attached to the manager. Other fields default to values on an
       endpoint basis.

       This function usually returns almost immediately with the connection
       continuing to occur in a new background thread.

       If FALSE is returned then the connection could not be established. For
       example if a PSTN endpoint is used and the associated line is engaged
       then it may return immediately. Returning a non-NULL value does not
       mean that the connection will succeed, only that an attempt is being
       made.

       The default behaviour is pure.
     */
    virtual BOOL MakeConnection(
      OpalCall & call,                   ///<  Owner of connection
      const PString & party,             ///<  Party to call
      void * userData = NULL,            ///<  user data to pass to connections
      unsigned int options = 0,          ///<  options to pass to conneciton
      OpalConnection::StringOptions * stringOptions = NULL
    );

    /**Call back for answering an incoming call.
       This function is used for an application to control the answering of
       incoming calls.

       If TRUE is returned then the connection continues. If FALSE then the
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

       The default behaviour is to call OnRouteConnection to determine a
       B party for the connection.

       If the call associated with the incoming call already had two parties
       and this connection is a third party for a conference call then
       AnswerCallNow is returned as a B party is not required.
     */
    virtual BOOL OnIncomingConnection(
      OpalConnection & connection,   ///<  Connection that is calling
      unsigned options,              ///<  options for new connection (can't use default as overrides will fail)
      OpalConnection::StringOptions * stringOptions
    );
    virtual BOOL OnIncomingConnection(
      OpalConnection & connection,   ///<  Connection that is calling
      unsigned options               ///<  options for new connection (can't use default as overrides will fail)
    );
    virtual BOOL OnIncomingConnection(
      OpalConnection & connection   ///<  Connection that is calling
    );

    /**Route a connection to another connection from an endpoint.

       The default behaviour gets the destination address from the connection
       and translates it into an address by using the routeTable member
       variable.
      */
    virtual PString OnRouteConnection(
      OpalConnection & connection  ///<  Connection being routed
    );

    /**Call back for remote party being alerted on outgoing call.
       This function is called after the connection is informed that the
       remote endpoint is "ringing". This function is generally called
       some time after the MakeConnection() function was called.

       If FALSE is returned the connection is aborted.

       If an application overrides this function, it should generally call the
       ancestor version for correct operation. An application would typically
       only intercept this function if it wishes to do some form of logging.
       For this you can obtain the name of the caller by using the function
       OpalConnection::GetRemotePartyName().

       The default behaviour calls the OnAlerting() on the connection's
       associated OpalCall object.
     */
    virtual void OnAlerting(
      OpalConnection & connection   ///<  Connection that was established
    );

    virtual OpalConnection::AnswerCallResponse
       OnAnswerCall(OpalConnection & connection,
                     const PString & caller
    );

    /**A call back function whenever a connection is "connected".
       This indicates that a connection to an endpoint was connected. That
       is the endpoint received acknowledgement via whatever protocol it uses
       that the connection may now start media streams.

       In the context of H.323 this means that the CONNECT pdu has been
       received.

       The default behaviour calls the OnConnected() on the connections
       associated OpalCall object.
      */
    virtual void OnConnected(
      OpalConnection & connection   ///<  Connection that was established
    );

    /**A call back function whenever a connection is "established".
       This indicates that a connection to an endpoint was connected and that
       media streams are opened. 

       In the context of H.323 this means that the CONNECT pdu has been
       received and either fast start was in operation or the subsequent Open
       Logical Channels have occurred. For SIP it indicates the INVITE/OK/ACK
       sequence is complete.

       The default behaviour calls the OnEstablished() on the connection's
       associated OpalCall object.
      */
    virtual void OnEstablished(
      OpalConnection & connection   ///<  Connection that was established
    );

    /**A call back function whenever a connection is released.
       This function can do any internal cleaning up and waiting on background
       threads that may be using the connection object.

       Classes that override this function should make sure they call the
       ancestor version for correct operation.

       An application will not typically call this function as it is used by
       the OpalManager during a release of the connection.

       The default behaviour calls OnReleased() on the connection's
       associated OpalCall object. This indicates to the call that the
       connection has been released so it can release the last remaining
       connection and then returns TRUE.
      */
    virtual void OnReleased(
      OpalConnection & connection   ///<  Connection that was established
    );
    
    /**A call back function whenever a connection is "held" or "retrieved".
       This indicates that a connection to an endpoint was held, or
       retrieved, either locally or by the remote endpoint.

       The default behaviour does nothing.
      */
    virtual void OnHold(
      OpalConnection & connection   ///<  Connection that was held
    );

    /**A call back function whenever a connection is forwarded.

       The default behaviour does nothing.
      */
    virtual BOOL OnForwarded(
      OpalConnection & connection,  ///<  Connection that was held
      const PString & remoteParty         ///<  The new remote party
    );
  //@}

  /**@name Media Streams management */
  //@{
    /**Adjust media formats available on a connection.
       This is called by a connection after it has called
       OpalCall::GetMediaFormats() to get all media formats that it can use so
       that an application may remove or reorder the media formats before they
       are used to open media streams.

       The default behaviour uses the mediaFormatOrder and mediaFormatMask
       member variables to adjust the mediaFormats list.
      */
    virtual void AdjustMediaFormats(
      const OpalConnection & connection,  ///<  Connection that is about to use formats
      OpalMediaFormatList & mediaFormats  ///<  Media formats to use
    ) const;

    /**See if the media can bypass the local host.
     */
    virtual BOOL IsMediaBypassPossible(
      const OpalConnection & source,      ///<  Source connection
      const OpalConnection & destination, ///<  Destination connection
      unsigned sessionID                  ///<  Session ID for media channel
    ) const;

    /**Call back when opening a media stream.
       This function is called when a connection has created a new media
       stream according to the logic of its underlying protocol.

       The usual requirement is that media streams are created on all other
       connections participating in the call and all of the media streams are
       attached to an instance of an OpalMediaPatch object that will read from
       one of the media streams passing data to the other media streams.

       The default behaviour achieves the above using the FindMatchingCodecs()
       to determine what (if any) software codecs are required, the
       OpalConnection::CreateMediaStream() function to open streams and the
       CreateMediaPatch() function to create a patch for all of the streams
       and codecs just produced.
      */
    virtual BOOL OnOpenMediaStream(
      OpalConnection & connection,  ///<  Connection that owns the media stream
      OpalMediaStream & stream    ///<  New media stream being opened
    );

    /**Callback from the RTP session for statistics monitoring.
       This is called every so many packets on the transmitter and receiver
       threads of the RTP session indicating that the statistics have been
       updated.

       The default behaviour does nothing.
      */
    virtual void OnRTPStatistics(
      const OpalConnection & connection,  ///<  Connection for the channel
      const RTP_Session & session         ///<  Session with statistics
    );

    /**Call back for closed a media stream.

       The default behaviour does nothing.
      */
    virtual void OnClosedMediaStream(
      const OpalMediaStream & stream     ///<  Stream being closed
    );

#if OPAL_VIDEO

    /**Add video media formats available on a connection.

       The default behaviour calls the OpalEndPoint function of the same name.
      */
    virtual void AddVideoMediaFormats(
      OpalMediaFormatList & mediaFormats, ///<  Media formats to use
      const OpalConnection * connection = NULL  ///<  Optional connection that is using formats
    ) const;

    /**Create a PVideoInputDevice for a source media stream.
      */
    virtual BOOL CreateVideoInputDevice(
      const OpalConnection & connection,    ///<  Connection needing created video device
      const OpalMediaFormat & mediaFormat,  ///<  Media format for stream
      PVideoInputDevice * & device,         ///<  Created device
      BOOL & autoDelete                     ///<  Flag for auto delete device
    );

    /**Create an PVideoOutputDevice for a sink media stream or the preview
       display for a source media stream.
      */
    virtual BOOL CreateVideoOutputDevice(
      const OpalConnection & connection,    ///<  Connection needing created video device
      const OpalMediaFormat & mediaFormat,  ///<  Media format for stream
      BOOL preview,                         ///<  Flag indicating is a preview output
      PVideoOutputDevice * & device,        ///<  Created device
      BOOL & autoDelete                     ///<  Flag for auto delete device
    );
#endif

    /**Create a OpalMediaPatch instance.
       This function allows an application to have the system create descendant
       class versions of the OpalMediPatch class. The application could use
       this to modify the default behaviour of a patch.

       The default behaviour returns an instance of OpalMediaPatch.
      */
    virtual OpalMediaPatch * CreateMediaPatch(
      OpalMediaStream & source,         ///<  Source media stream
      BOOL requiresPatchThread = TRUE
    );

    /**Destroy a OpalMediaPatch instance.

       The default behaviour simply calls delete patch.
      */
    virtual void DestroyMediaPatch(
      OpalMediaPatch * patch
    );

    /**Call back for a media patch thread starting.
       This function is called within the context of the thread associated
       with the media patch. It may be used to do any last checks on if the
       patch should proceed.

       The default behaviour simply returns TRUE.
      */
    virtual BOOL OnStartMediaPatch(
      const OpalMediaPatch & patch     ///<  Media patch being started
    );
  //@}

  /**@name User indications */
  //@{
    /**Call back for remote endpoint has sent user input as a string.

       The default behaviour call OpalConnection::SetUserInput() which
       saves the value so the GetUserInput() function can return it.
      */
    virtual void OnUserInputString(
      OpalConnection & connection,  ///<  Connection input has come from
      const PString & value         ///<  String value of indication
    );

    /**Call back for remote enpoint has sent user input as tones.
       If duration is zero then this indicates the beginning of the tone. If
       duration is non-zero then it indicates the end of the tone output.

       The default behaviour calls the OpalCall function of the same name.
      */
    virtual void OnUserInputTone(
      OpalConnection & connection,  ///<  Connection input has come from
      char tone,                    ///<  Tone received
      int duration                  ///<  Duration of tone
    );

    /**Read a sequence of user indications from connection with timeouts.
      */
    virtual PString ReadUserInput(
      OpalConnection & connection,        ///<  Connection to read input from
      const char * terminators = "#\r\n", ///<  Characters that can terminte input
      unsigned lastDigitTimeout = 4,      ///<  Timeout on last digit in string
      unsigned firstDigitTimeout = 30     ///<  Timeout on receiving any digits
    );
  //@}

  /**@name Other services */
  //@{
#if OPAL_T120DATA
    /**Create an instance of the T.120 protocol handler.
       This is called when the OpenLogicalChannel subsystem requires that
       a T.120 channel be established.

       Note that if the application overrides this it should return a pointer
       to a heap variable (using new) as it will be automatically deleted when
       the H323Connection is deleted.

       The default behavour returns NULL.
      */
    virtual OpalT120Protocol * CreateT120ProtocolHandler(
      const OpalConnection & connection  ///<  Connection for which T.120 handler created
    ) const;
#endif

#if OPAL_T38FAX
    /**Create an instance of the T.38 protocol handler.
       This is called when the OpenLogicalChannel subsystem requires that
       a T.38 fax channel be established.

       Note that if the application overrides this it should return a pointer
       to a heap variable (using new) as it will be automatically deleted when
       the H323Connection is deleted.

       The default behavour returns NULL.
      */
    virtual OpalT38Protocol * CreateT38ProtocolHandler(
      const OpalConnection & connection  ///<  Connection for which T.38 handler created
    ) const;
	
#endif

#if OPAL_H224
    /** Create an instance of the H.224 protocol handler.
        This is called when the call subsystem requires that a H.224 channel be established.
		
        Note that if the application overrides this it should return a pointer
        to a heap variable (using new) as it will be automatically deleted when
        the OpalConnection is deleted.
		
        The default behaviour creates a standard OpalH224Handler instance
      */
	virtual OpalH224Handler * CreateH224ProtocolHandler(
      OpalConnection & connection, unsigned sessionID
    ) const;
	
    /** Create an instance of the H.281 protocol handler.
        This is called when a H.224 channel is established.
		
        Note that if the application overrides this it should return a pointer
        to a heap variable (using new) as it will be automatically deleted when
        the OpalConnection is deleted.
		
        The default behaviour creates a standard OpalH281Handler instance
      */
	virtual OpalH281Handler * CreateH281ProtocolHandler(
      OpalH224Handler & h224Handler
    ) const;
#endif

    class RouteEntry : public PObject
    {
        PCLASSINFO(RouteEntry, PObject);
      public:
        RouteEntry(const PString & pat, const PString & dest);
        void PrintOn(ostream & strm) const;
        PString            pattern;
        PString            destination;
        PRegularExpression regex;
    };
    PLIST(RouteTable, RouteEntry);

    /**Parse a route table specification list for the manager.
       Add a route entry to the route table.

       The specification string is of the form pattern=destination where
       pattern is a regular expression matching the incoming calls
       destination address and will translate it to the outgoing destination
       address for making an outgoing call. For example, picking up a PhoneJACK
       handset and dialing 2, 6 would result in an address of "pots:26"
       which would then be matched against, say, a specification of
       pots:26=h323:10.0.1.1, resulting in a call from the pots handset to
       10.0.1.1 using H.323.

       As the pattern field is a regular expression, you could have used in
       the above .*:26=h323:10.0.1.1 to achieve the same result with the
       addition that an incoming call from a SIP client would also be routed
       to the H.323 client.

       Note that the pattern has an implicit ^ and $ at the beginning and end
       of the regular expression. So it must match the entire address.

       If the specification is of the form @filename, then the file is read
       with each line consisting of a pattern=destination route specification.
       Lines without an equal sign or beginning with '#' are ignored.

       Returns TRUE if an entry was added.
      */
    virtual BOOL AddRouteEntry(
      const PString & spec  ///<  Specification string to add
    );

    /**Parse a route table specification list for the manager.
       This removes the current routeTable and calls AddRouteEntry for every
       string in the array.

       Returns TRUE if at least one entry was added.
      */
    BOOL SetRouteTable(
      const PStringArray & specs  ///<  Array of specification strings.
    );

    /**Set a route table for the manager.
       Note that this will make a copy of the table and not maintain a
       reference.
      */
    void SetRouteTable(
      const RouteTable & table  ///<  New table to set for routing
    );

    /**Get the active route table for the manager.
      */
    const RouteTable & GetRouteTable() const { return routeTable; }

    /**Route the source address to a destination using the route table.
      */
    virtual PString ApplyRouteTable(
      const PString & proto,
      const PString & addr
    );
  //@}

  /**@name Member variable access */
  //@{
    /**Get the product info for all endpoints.
      */
    const OpalProductInfo & GetProductInfo() const { return productInfo; }

    /**Set the product info for all endpoints.
      */
    void SetProductInfo(
      const OpalProductInfo & info
    ) { productInfo = info; }

    /**Get the default username for all endpoints.
      */
    const PString & GetDefaultUserName() const { return defaultUserName; }

    /**Set the default username for all endpoints.
      */
    void SetDefaultUserName(
      const PString & name
    ) { defaultUserName = name; }

    /**Get the default display name for all endpoints.
      */
    const PString & GetDefaultDisplayName() const { return defaultDisplayName; }

    /**Set the default display name for all endpoints.
      */
    void SetDefaultDisplayName(
      const PString & name
    ) { defaultDisplayName = name; }

#if OPAL_VIDEO

    /**See if should auto-start receive video channels on connection.
     */
    BOOL CanAutoStartReceiveVideo() const { return autoStartReceiveVideo; }

    /**Set if should auto-start receive video channels on connection.
     */
    void SetAutoStartReceiveVideo(BOOL can) { autoStartReceiveVideo = can; }

    /**See if should auto-start transmit video channels on connection.
     */
    BOOL CanAutoStartTransmitVideo() const { return autoStartTransmitVideo; }

    /**Set if should auto-start transmit video channels on connection.
     */
    void SetAutoStartTransmitVideo(BOOL can) { autoStartTransmitVideo = can; }

#endif

    /**Determine if the address is "local", ie does not need any address
       translation (fixed or via STUN) to access.

       The default behaviour checks if remoteAddress is a RFC1918 private
       IP address: 10.x.x.x, 172.16.x.x or 192.168.x.x.
     */
    virtual BOOL IsLocalAddress(
      const PIPSocket::Address & remoteAddress
    ) const;

    /**Provide address translation hook.
       This will check to see that remoteAddress is NOT a local address by
       using IsLocalAddress() and if not, set localAddress to the
       translationAddress (if valid) which would normally be the router
       address of a NAT system.
     */
    virtual BOOL TranslateIPAddress(
      PIPSocket::Address & localAddress,
      const PIPSocket::Address & remoteAddress
    );

    /**Get the translation address to use for TranslateIPAddress().
      */
    const PIPSocket::Address & GetTranslationAddress() const { return translationAddress; }

    /**Set the translation address to use for TranslateIPAddress().
      */
    void SetTranslationAddress(
      const PIPSocket::Address & address
    ) { translationAddress = address; }

    /**Return the STUN server to use.
       Returns NULL if address is a local address as per IsLocalAddress().
       Always returns the STUN server if address is zero.
       Note, the pointer is NOT to be deleted by the user.
      */
    PSTUNClient * GetSTUN(
      const PIPSocket::Address & address = 0
    ) const;

    /**Set the STUN server address, is of the form host[:port]
       Note that if the STUN server is found then the translationAddress
       is automatically set to the router address as determined by STUN.
      */
    PSTUNClient::NatTypes SetSTUNServer(
      const PString & server
    );

    /**Get the current host name and optional port for the STUN server.
      */
    const PString & GetSTUNServer() const { return stunServer; }

    /**Get the TCP port number base for H.245 channels
     */
    WORD GetTCPPortBase() const { return tcpPorts.base; }

    /**Get the TCP port number base for H.245 channels.
     */
    WORD GetTCPPortMax() const { return tcpPorts.max; }

    /**Set the TCP port number base and max for H.245 channels.
     */
    void SetTCPPorts(unsigned tcpBase, unsigned tcpMax);

    /**Get the next TCP port number for H.245 channels
     */
    WORD GetNextTCPPort();

    /**Get the UDP port number base for RAS channels
     */
    WORD GetUDPPortBase() const { return udpPorts.base; }

    /**Get the UDP port number base for RAS channels.
     */
    WORD GetUDPPortMax() const { return udpPorts.max; }

    /**Set the TCP port number base and max for H.245 channels.
     */
    void SetUDPPorts(unsigned udpBase, unsigned udpMax);

    /**Get the next UDP port number for RAS channels
     */
    WORD GetNextUDPPort();

    /**Get the UDP port number base for RTP channels.
     */
    WORD GetRtpIpPortBase() const { return rtpIpPorts.base; }

    /**Get the max UDP port number for RTP channels.
     */
    WORD GetRtpIpPortMax() const { return rtpIpPorts.max; }

    /**Set the UDP port number base and max for RTP channels.
     */
    void SetRtpIpPorts(unsigned udpBase, unsigned udpMax);

    /**Get the UDP port number pair for RTP channels.
     */
    WORD GetRtpIpPortPair();

    /**Get the IP Type Of Service byte for media (eg RTP) channels.
     */
    BYTE GetRtpIpTypeofService() const { return rtpIpTypeofService; }

    /**Set the IP Type Of Service byte for media (eg RTP) channels.
     */
    void SetRtpIpTypeofService(unsigned tos) { rtpIpTypeofService = (BYTE)tos; }

    /**Get the default maximum audio jitter delay parameter.
       Defaults to 50ms
     */
    unsigned GetMinAudioJitterDelay() const { return minAudioJitterDelay; }

    /**Get the default maximum audio jitter delay parameter.
       Defaults to 250ms.
     */
    unsigned GetMaxAudioJitterDelay() const { return maxAudioJitterDelay; }

    /**Set the maximum audio jitter delay parameter.
     */
    void SetAudioJitterDelay(
      unsigned minDelay,   ///<  New minimum jitter buffer delay in milliseconds
      unsigned maxDelay    ///<  New maximum jitter buffer delay in milliseconds
    );

    /**Get the default media format order.
     */
    const PStringArray & GetMediaFormatOrder() const { return mediaFormatOrder; }

    /**Set the default media format order.
     */
    void SetMediaFormatOrder(const PStringArray & order) { mediaFormatOrder = order; }

    /**Get the default media format mask.
     */
    const PStringArray & GetMediaFormatMask() const { return mediaFormatMask; }

    /**Set the default media format mask.
     */
    void SetMediaFormatMask(const PStringArray & mask) { mediaFormatMask = mask; }

    /**Set the default parameters for the silence detector.
     */
    virtual void SetSilenceDetectParams(
      const OpalSilenceDetector::Params & params
    ) { silenceDetectParams = params; }

    /**Get the default parameters for the silence detector.
     */
    const OpalSilenceDetector::Params & GetSilenceDetectParams() const { return silenceDetectParams; }
    
    /**Set the default parameters for the echo cancelation.
     */
    virtual void SetEchoCancelParams(
      const OpalEchoCanceler::Params & params
    ) { echoCancelParams = params; }

    /**Get the default parameters for the silence detector.
     */
    const OpalEchoCanceler::Params & GetEchoCancelParams() const { return echoCancelParams; }

#if OPAL_VIDEO

    /**Set the parameters for the video device to be used for input.
       If the name is not suitable for use with the PVideoInputDevice class
       then the function will return FALSE and not change the device.

       This defaults to the value of the PVideoInputDevice::GetInputDeviceNames()
       function.
     */
    virtual BOOL SetVideoInputDevice(
      const PVideoDevice::OpenArgs & deviceArgs ///<  Full description of device
    );

    /**Get the parameters for the video device to be used for input.
       This defaults to the value of the PSoundChannel::GetInputDeviceNames()[0].
     */
    const PVideoDevice::OpenArgs & GetVideoInputDevice() const { return videoInputDevice; }

    /**Set the parameters for the video device to be used to preview input.
       If the name is not suitable for use with the PVideoOutputDevice class
       then the function will return FALSE and not change the device.

       This defaults to the value of the PVideoInputDevice::GetOutputDeviceNames()
       function.
     */
    virtual BOOL SetVideoPreviewDevice(
      const PVideoDevice::OpenArgs & deviceArgs ///<  Full description of device
    );

    /**Get the parameters for the video device to be used for input.
       This defaults to the value of the PSoundChannel::GetInputDeviceNames()[0].
     */
    const PVideoDevice::OpenArgs & GetVideoPreviewDevice() const { return videoPreviewDevice; }

    /**Set the parameters for the video device to be used for output.
       If the name is not suitable for use with the PVideoOutputDevice class
       then the function will return FALSE and not change the device.

       This defaults to the value of the PVideoInputDevice::GetOutputDeviceNames()
       function.
     */
    virtual BOOL SetVideoOutputDevice(
      const PVideoDevice::OpenArgs & deviceArgs ///<  Full description of device
    );

    /**Get the parameters for the video device to be used for input.
       This defaults to the value of the PSoundChannel::GetOutputDeviceNames()[0].
     */
    const PVideoDevice::OpenArgs & GetVideoOutputDevice() const { return videoOutputDevice; }

#endif

    BOOL DetectInBandDTMFDisabled() const
      { return disableDetectInBandDTMF; }

    /**Set the default H.245 tunneling mode.
      */
    void DisableDetectInBandDTMF(
      BOOL mode ///<  New default mode
    ) { disableDetectInBandDTMF = mode; } 

    /**Get the amount of time with no media that should cause a call to clear
     */
    const PTimeInterval & GetNoMediaTimeout() const { return noMediaTimeout; }

    /**Set the amount of time with no media that should cause a call to clear
     */
    BOOL SetNoMediaTimeout(
      const PTimeInterval & newInterval  ///<  New timeout for media
    );

    /**Get the default ILS server to use for user lookup.
      */
    const PString & GetDefaultILSServer() const { return ilsServer; }

    /**Set the default ILS server to use for user lookup.
      */
    void SetDefaultILSServer(
      const PString & server
    ) { ilsServer = server; }
  //@}

    // needs to be public for gcc 3.4
    void GarbageCollection();

    virtual void OnNewConnection(OpalConnection & conn);

    virtual void SetDefaultSecurityMode(const PString & v)
    { defaultSecurityMode = v; }

    virtual PString GetDefaultSecurityMode() const 
    { return defaultSecurityMode; }

    virtual BOOL UseRTPAggregation() const;

    OpalRecordManager & GetRecordManager()
    { return recordManager; }

    virtual BOOL StartRecording(const PString & callToken, const PFilePath & fn);
    virtual void StopRecording(const PString & callToken);

    virtual BOOL IsRTPNATEnabled(
      OpalConnection & conn, 
      const PIPSocket::Address & localAddr, 
      const PIPSocket::Address & peerAddr,
      const PIPSocket::Address & sigAddr,
      BOOL incoming
    );

  protected:
    // Configuration variables
    OpalProductInfo productInfo;

    PString       defaultUserName;
    PString       defaultDisplayName;

#if OPAL_VIDEO
    BOOL          autoStartReceiveVideo;
    BOOL          autoStartTransmitVideo;
#endif

    BYTE          rtpIpTypeofService;
    unsigned      minAudioJitterDelay;
    unsigned      maxAudioJitterDelay;
    PStringArray  mediaFormatOrder;
    PStringArray  mediaFormatMask;
    BOOL          disableDetectInBandDTMF;
    PTimeInterval noMediaTimeout;
    PString       ilsServer;

    OpalSilenceDetector::Params silenceDetectParams;
    OpalEchoCanceler::Params echoCancelParams;

#if OPAL_VIDEO
    PVideoDevice::OpenArgs videoInputDevice;
    PVideoDevice::OpenArgs videoPreviewDevice;
    PVideoDevice::OpenArgs videoOutputDevice;
#endif

    struct PortInfo {
      void Set(
        unsigned base,
        unsigned max,
        unsigned range,
        unsigned dflt
      );
      WORD GetNext(
        unsigned increment
      );

      PMutex mutex;
      WORD   base;
      WORD   max;
      WORD   current;
    } tcpPorts, udpPorts, rtpIpPorts;

    PIPSocket::Address translationAddress;
    PString            stunServer;
    PSTUNClient      * stun;

    RouteTable routeTable;
    PMutex     routeTableMutex;

    // Dynamic variables
    PMutex inUseFlag;

    PList<OpalEndPoint> endpoints;

    unsigned     lastCallTokenID;

    class CallDict : public PSafeDictionary<PString, OpalCall>
    {
      public:
        CallDict(OpalManager & mgr) : manager(mgr) { }
        virtual void DeleteObject(PObject * object) const;
        OpalManager & manager;
    } activeCalls;

    BOOL	 clearingAllCalls;
    PSyncPoint   allCallsCleared;
    PThread    * garbageCollector;
    PSyncPoint   garbageCollectExit;
    PDECLARE_NOTIFIER(PThread, OpalManager, GarbageMain);

    PString defaultSecurityMode;

#if OPAL_RTP_AGGREGATE
    BOOL useRTPAggregation; 
#endif

    OpalRecordManager recordManager;

    friend OpalCall::OpalCall(OpalManager & mgr);
    friend void OpalCall::OnReleased(OpalConnection & connection);
};


PString  OpalGetVersion();
unsigned OpalGetMajorVersion();
unsigned OpalGetMinorVersion();
unsigned OpalGetBuildNumber();


#endif // __OPAL_MANAGER_H


// End of File ///////////////////////////////////////////////////////////////
