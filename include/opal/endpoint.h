/*
 * endpoint.h
 *
 * Telephony endpoint abstraction
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
 * $Log: endpoint.h,v $
 * Revision 1.2016  2003/03/17 10:26:59  robertj
 * Added video support.
 *
 * Revision 2.14  2003/03/06 03:57:47  robertj
 * IVR support (work in progress) requiring large changes everywhere.
 *
 * Revision 2.13  2002/11/10 11:33:17  robertj
 * Updated to OpenH323 v1.10.3
 *
 * Revision 2.12  2002/09/16 02:52:35  robertj
 * Added #define so can select if #pragma interface/implementation is used on
 *   platform basis (eg MacOS) rather than compiler, thanks Robert Monaghan.
 *
 * Revision 2.11  2002/07/04 07:41:47  robertj
 * Fixed memory/thread leak of transports.
 *
 * Revision 2.10  2002/07/01 04:56:30  robertj
 * Updated to OpenH323 v1.9.1
 *
 * Revision 2.9  2002/04/05 10:36:53  robertj
 * Rearranged OnRelease to remove the connection from endpoints connection
 *   list at the end of the release phase rather than the beginning.
 *
 * Revision 2.8  2002/01/22 05:04:40  robertj
 * Revamp of user input API triggered by RFC2833 support
 *
 * Revision 2.7  2001/11/14 01:31:55  robertj
 * Corrected placement of adjusting media format list.
 *
 * Revision 2.6  2001/11/13 06:25:56  robertj
 * Changed SetUpConnection() so returns BOOL as returning
 *   pointer to connection is not useful.
 *
 * Revision 2.5  2001/08/23 05:51:17  robertj
 * Completed implementation of codec reordering.
 *
 * Revision 2.4  2001/08/22 10:20:09  robertj
 * Changed connection locking to use double mutex to guarantee that
 *   no threads can ever deadlock or access deleted connection.
 *
 * Revision 2.3  2001/08/17 08:22:23  robertj
 * Moved call end reasons enum from OpalConnection to global.
 *
 * Revision 2.2  2001/08/01 05:26:35  robertj
 * Moved media formats list from endpoint to connection.
 *
 * Revision 2.1  2001/07/30 07:22:25  robertj
 * Abstracted listener management from H.323 to OpalEndPoint class.
 *
 * Revision 2.0  2001/07/27 15:48:24  robertj
 * Conversion of OpenH323 to Open Phone Abstraction Library (OPAL)
 *
 */

#ifndef __OPAL_ENDPOINT_H
#define __OPAL_ENDPOINT_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


#include <opal/connection.h>
#include <opal/mediafmt.h>
#include <opal/transports.h>


class OpalManager;
class OpalCall;
class OpalMediaStream;


/**This class describes an endpoint base class.
   Each protocol (or psuedo-protocol) would create a descendant off this
   class to manage its particular subsystem. Typically this would involve
   listening for incoming connections and being able to set up outgoing
   connections. Depending on exact semantics it may need to spawn many threads
   to achieve this.

   An endpoint will also have a default set of media data formats that it can
   support. Connections created by it would initially have the same set, but
   according to the semantics of the underlying protocol may end up using a
   different set.

   Various call backs are provided for points in the connection management. As
   a rule these are passed straight on to the OpalManager for processing. An
   application may create descendants off this class' subclasses, eg
   H323EndPoint, to modify or monitor its behaviour but it does not have to do
   so as all basic operations are passed to the OpalManager so only that class
   need be subclassed.
 */
class OpalEndPoint : public PObject
{
    PCLASSINFO(OpalEndPoint, PObject);
  public:
    enum Attributes {
      CanTerminateCall = 1,
      HasLineInterface = 2
    };

  /**@name Construction */
  //@{
    /**Create a new endpoint.
     */
    OpalEndPoint(
      OpalManager & manager,          /// Manager of all endpoints.
      const PCaselessString & prefix, /// Prefix for URL style address strings
      unsigned attributes             /// Bit mask of attributes endpoint has
    );

    /**Destroy the endpoint.
     */
    ~OpalEndPoint();
  //@}

  /**@name Overrides from PObject */
  //@{
    /**Standard stream print function.
       The PObject class has a << operator defined that calls this function
       polymorphically.
      */
    void PrintOn(
      ostream & strm    /// Stream to output text representation
    ) const;
  //@}

  /**@name Listeners management */
  //@{
    /**Add a listener to the endoint.
       This allows for the automatic creating of incoming call connections. An
       application should use OnConnectionEstablished() to monitor when calls
       have arrived and been successfully negotiated.
      */
    BOOL StartListeners(
      const PStringArray & interfaces /// Address of interface to listen on.
    );

    /**Add a listener to the endoint.
       This allows for the automatic creating of incoming call connections. An
       application should use OnConnectionEstablished() to monitor when calls
       have arrived and been successfully negotiated.
      */
    BOOL StartListener(
      const OpalTransportAddress & iface /// Address of interface to listen on.
    );

    /**Add a listener to the endoint.
       This allows for the automatic creating of incoming call connections. An
       application should use OnConnectionEstablished() to monitor when calls
       have arrived and been successfully negotiated.
      */
    BOOL StartListener(
      OpalListener * listener /// Transport dependent listener.
    );

    /**Remove a listener from the endoint.
       If the listener parameter is NULL then all listeners are removed.
      */
    BOOL RemoveListener(
      OpalListener * listener /// Transport dependent listener.
    );

    /**Handle new incoming connection.
       This will either create a new connection object or utilise a previously
       created connection on the same transport address and reference number.
      */
    PDECLARE_NOTIFIER(PThread, OpalEndPoint, ListenerCallback);

    /**Handle new incoming connection from listener.

       A return value of TRUE indicates that the transport object should be
       deleted by the caller. FALSE indicates that something else (eg the
       connection) has taken over responsibility for deleting the transport.

       The default behaviour just returns TRUE.
      */
    virtual BOOL NewIncomingConnection(
      OpalTransport * transport  /// Transport connection came in on
    );
  //@}

  /**@name Connection management */
  //@{
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
       FALSE is returned.

       This function usually returns almost immediately with the connection
       continuing to occur in a new background thread.

       If FALSE is returned then the connection could not be established. For
       example if a PSTN endpoint is used and the assiciated line is engaged
       then it may return immediately. Returning a non-NULL value does not
       mean that the connection will succeed, only that an attempt is being
       made.

       The default behaviour is pure.
     */
    virtual BOOL MakeConnection(
      OpalCall & call,        /// Owner of connection
      const PString & party,  /// Remote party to call
      void * userData         /// Arbitrary data to pass to connection
    ) = 0;

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

       The default behaviour calls the OpalManager function of the same name.
     */
    virtual BOOL OnIncomingConnection(
      OpalConnection & connection   /// Connection that is calling
    );

    /**Call back for remote party being alerted.
       This function is called after the connection is informed that the
       remote endpoint is "ringing". Generally some time after the
       MakeConnection() function was called, this is function is called.

       If FALSE is returned the connection is aborted.

       If an application overrides this function, it should generally call the
       ancestor version for correct operation. An application would typically
       only intercept this function if it wishes to do some form of logging.
       For this you can obtain the name of the caller by using the function
       OpalConnection::GetRemotePartyName().

       The default behaviour calls the OpalManager function of the same name.
     */
    virtual void OnAlerting(
      OpalConnection & connection   /// Connection that was established
    );

    /**A call back function whenever a connection is "connected".
       This indicates that a connection to an endpoint was connected. That
       is the endpoint received acknowledgement via whatever protocol it uses
       that the connection may now start media streams.

       In the context of H.323 this means that the CONNECT pdu has been
       received.

       The default behaviour calls the OpalManager function of the same name.
      */
    virtual void OnConnected(
      OpalConnection & connection   /// Connection that was established
    );

    /**A call back function whenever a connection is established.
       This indicates that a connection to an endpoint was established. That
       is the endpoint received acknowledgement via whatever protocol it uses
       that the connection may now start media streams.

       In the context of H.323 this means that the signalling and control
       channels are open and the TerminalCapabilitySet and MasterSlave
       negotiations are complete.

       The default behaviour does nothing.
      */
    virtual void OnEstablished(
      OpalConnection & connection   /// Connection that was established
    );

    /**A call back function whenever a connection is broken.
       This function can do any internal cleaning up and waiting on background
       threads that may be using the connection object.

       Note that there is not a one to one relationship with the
       OnEstablishedConnection() function. This function may be called without
       that function being called. For example if MakeConnection() was used
       but the call never completed.

       The return value indicates if the connection object is to be deleted. A
       value of FALSE can be returned and it then someone elses responsibility
       to free the memory used.

       Classes that override this function should make sure they call the
       ancestor version for correct operation.

       An application will not typically call this function as it is used by
       the OpalManager during a release of the connection.

       The default behaviour removes the connection from the internal database
       and calls the OpalManager function of the same name.
      */
    virtual BOOL OnReleased(
      OpalConnection & connection   /// Connection that was established
    );

    /**Clear a call.
       This finds the call by using the token then calls the OpalCall::Clear()
       function on it. All connections are released, and the conenctions and
       call disposed of. Note that this function returns quickly and the
       disposal happens at some later time by a background thread. This it is
       safe to call this function from anywhere.
      */
    virtual BOOL ClearCall(
      const PString & token,    /// Token for identifying connection
      OpalConnection::CallEndReason reason = OpalConnection::EndedByLocalUser, /// Reason for call clearing
      PSyncPoint * sync = NULL  /// Sync point to wait on.
    );

    /**Clear a current connection.
       This hangs up the connection to a remote endpoint. Note that these functions
       are synchronous
      */
    virtual BOOL ClearCallSynchronous(
      const PString & token,    /// Token for identifying connection
      OpalConnection::CallEndReason reason = OpalConnection::EndedByLocalUser, /// Reason for call clearing
      PSyncPoint * sync = NULL  /// Sync point to wait on.
    );

    /**Clear all current connections.
       This hangs up all the connections to remote endpoints. The wait
       parameter is used to wait for all the calls to be cleared and their
       memory usage cleaned up before returning. This is typically used in
       the destructor for your descendant of H323EndPoint.
      */
    virtual void ClearAllCalls(
      OpalConnection::CallEndReason reason = OpalConnection::EndedByLocalUser, /// Reason for call clearing
      BOOL wait = TRUE   /// Flag for wait for calls to e cleared.
    );

    /**Find a connection that uses the specified token.
       This searches the endpoint for the connection that contains the token
       as provided by functions such as MakeConnection().

       Note the caller of this function MUSt call the OpalConnection::Unlock()
       function if this function returns a non-NULL pointer. If it does not
       then a deadlock can occur.
      */
    virtual OpalConnection * GetConnectionWithLock(
      const PString & token     /// Token to identify connection
    );

    /**Get all calls current on the endpoint.
      */
    PStringList GetAllConnections();

    /**Determine if a connection is active.
      */
    virtual BOOL HasConnection(
      const PString & token   /// Token for identifying connection
    );
  //@}

  /**@name Media Streams management */
  //@{
    /**Adjust media formats available on a connection.
       This is called by a connection after it has called
       OpalCall::GetMediaFormats() to get all media formats that it can use so
       that an application may remove or reorder the media formats before they
       are used to open media streams.

       The default behaviour calls the OpalManager function of the same name.
      */
    virtual void AdjustMediaFormats(
      const OpalConnection & connection,  /// Connection that is about to use formats
      OpalMediaFormatList & mediaFormats  /// Media formats to use
    ) const;

    /**Call back when opening a media stream.
       This function is called when a connection has created a new media
       stream according to the logic of its underlying protocol.

       The usual requirement is that media streams are created on all other
       connections participating in the call and all of the media streams are
       attached to an instance of an OpalMediaPatch object that will read from
       one of the media streams passing data to the other media streams.

       The default behaviour calls the OpalManager function of the same name.
      */
    virtual BOOL OnOpenMediaStream(
      OpalConnection & connection,  /// Connection that owns the media stream
      OpalMediaStream & stream      /// New media stream being opened
    );

    /**Call back for closed a media stream.

       The default behaviour calls the OpalManager function of the same name.
      */
    virtual void OnClosedMediaStream(
      const OpalMediaStream & stream     /// Media stream being closed
    );

    /**Add video media formats available on a connection.

       The default behaviour calls the OpalEndPoint function of the same name.
      */
    virtual void AddVideoMediaFormats(
      const OpalConnection & connection,  /// Connection that is about to use formats
      OpalMediaFormatList & mediaFormats  /// Media formats to use
    ) const;

    /**Create an PVideoInputDevice for a source media stream.
      */
    virtual PVideoInputDevice * CreateVideoInputDevice(
      const OpalConnection & connection /// Connection needing created video device
    );

    /**Create an PVideoOutputDevice for a sink media stream.
      */
    virtual PVideoOutputDevice * CreateVideoOutputDevice(
      const OpalConnection & connection /// Connection needing created video device
    );
  //@}

  /**@name User indications */
  //@{
    /**Call back for remote enpoint has sent user input as a string.

       The default behaviour calls the OpalManager function of the same name.
      */
    virtual void OnUserInputString(
      OpalConnection & connection,  /// Connection input has come from
      const PString & value   /// String value of indication
    );

    /**Call back for remote enpoint has sent user input.
       If duration is zero then this indicates the beginning of the tone. If
       duration is non-zero then it indicates the end of the tone output.

       The default behaviour calls the OpalManager function of the same name.
      */
    virtual void OnUserInputTone(
      OpalConnection & connection,  /// Connection input has come from
      char tone,                    /// Tone received
      int duration                  /// Duration of tone
    );
  //@}

  /**@name Other services */
  //@{
    /**Create an instance of the T.120 protocol handler.
       This is called when the OpenLogicalChannel subsystem requires that
       a T.120 channel be established.

       Note that if the application overrides this it should return a pointer to a
       heap variable (using new) as it will be automatically deleted when the
       H323Connection is deleted.

       The default behavour calls the OpalManager function of the same name.
      */
    virtual OpalT120Protocol * CreateT120ProtocolHandler(
      const OpalConnection & connection  /// Connection for which T.120 handler created
    ) const;

    /**Create an instance of the T.38 protocol handler.
       This is called when the OpenLogicalChannel subsystem requires that
       a T.38 fax channel be established.

       Note that if the application overrides this it should return a pointer to a
       heap variable (using new) as it will be automatically deleted when the
       H323Connection is deleted.

       The default behavour calls the OpalManager function of the same name.
      */
    virtual OpalT38Protocol * CreateT38ProtocolHandler(
      const OpalConnection & connection  /// Connection for which T.38 handler created
    ) const;
  //@}

  /**@name Member variable access */
  //@{
    /**Get the manager for this endpoint.
     */
    OpalManager & GetManager() const { return manager; }

    /**Get the protocol prefix name for the endpoint.
      */
    const PString & GetPrefixName() const { return prefixName; }

    /**Get an indication of if this endpoint has particular option.
      */
    BOOL HasAttribute(Attributes opt) const { return (attributeBits&opt) != 0; }

    /**Get the initial bandwidth parameter.
     */
    WORD GetDefaultSignalPort() const { return defaultSignalPort; }

    /**Get the initial bandwidth parameter.
     */
    unsigned GetInitialBandwidth() const { return initialBandwidth; }

    /**Get the initial bandwidth parameter.
     */
    void SetInitialBandwidth(unsigned bandwidth) { initialBandwidth = bandwidth; }

    /**Get the set of listeners (incoming call transports) for this endpoint.
     */
    const OpalListenerList & GetListeners() const { return listeners; }
  //@}


  protected:
    OpalManager   & manager;
    PCaselessString prefixName;
    unsigned        attributeBits;
    WORD            defaultSignalPort;

    unsigned initialBandwidth;  // in 100s of bits/sev

    OpalListenerList   listeners;
    OpalConnectionDict connectionsActive;
    PSyncPoint         allConnectionsCleared;

    PMutex inUseFlag;
};


PLIST(OpalEndPointList, OpalEndPoint);


#endif // __OPAL_ENDPOINT_H


// End of File ///////////////////////////////////////////////////////////////
