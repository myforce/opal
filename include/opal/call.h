/*
 * call.h
 *
 * Telephone call management
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
 * $Log: call.h,v $
 * Revision 1.2010  2002/09/16 02:52:35  robertj
 * Added #define so can select if #pragma interface/implementation is used on
 *   platform basis (eg MacOS) rather than compiler, thanks Robert Monaghan.
 *
 * Revision 2.8  2002/04/08 02:40:13  robertj
 * Fixed issues with using double originate call, eg from simple app command line.
 *
 * Revision 2.7  2002/02/19 07:42:25  robertj
 * Restructured media bypass functions to fix problems with RFC2833.
 *
 * Revision 2.6  2002/02/11 07:38:01  robertj
 * Added media bypass for streams between compatible protocols.
 *
 * Revision 2.5  2002/01/22 05:03:47  robertj
 * Revamp of user input API triggered by RFC2833 support
 *
 * Revision 2.4  2001/11/15 07:02:12  robertj
 * Changed OpalCall::OpenSourceMediaStreams so the connection to not open
 *   a media stream on is optional.
 *
 * Revision 2.3  2001/08/22 10:20:09  robertj
 * Changed connection locking to use double mutex to guarantee that
 *   no threads can ever deadlock or access deleted connection.
 *
 * Revision 2.2  2001/08/17 08:24:33  robertj
 * Added call end reason for whole call, not just connection.
 *
 * Revision 2.1  2001/08/01 05:28:59  robertj
 * Added function to get all media formats possible in a call.
 *
 * Revision 2.0  2001/07/27 15:48:24  robertj
 * Conversion of OpenH323 to Open Phone Abstraction Library (OPAL)
 *
 */

#ifndef __OPAL_CALL_H
#define __OPAL_CALL_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


#include <opal/connection.h>
#include <opal/guid.h>


class OpalManager;


/**This class manages a call.
   A call consists of one or more OpalConnection instances. While these
   connections may be created elsewhere this class is responsible for their
   disposal. The GarbageCollection() function is called from a thread managed
   by the OpalManager instance. This function disposes of connections. The
   OpalManager then disposes of the call itself if it has not more connections
   attached to it.
 */
class OpalCall : public PObject
{
    PCLASSINFO(OpalCall, PObject);
  public:
  /**@name Construction */
  //@{
    /**Create a new call.
     */
    OpalCall(
      OpalManager & manager   /// Manager for the opal system
    );

    /**Destroy call.
     */
    ~OpalCall();
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

  /**@name Basic operations */
  //@{
    /**Lock connection.
       When the OpalEndPoint::FindCallWithLock() function is used to gain
       access to a connection object, this is called to prevent it from being
       closed and deleted by the background threads.
     */
    void Lock() { inUseFlag.Wait(); }

    /**Unlock connection.
       If the OpalEndPoint::FindCallWithLock() function is used to gain
       access to a connection object, this MUST be called to allow it to
       subsequently be closed and disposed of.
     */
    void Unlock() { inUseFlag.Signal(); }

    /**Check that all connections in call are connected and media is going.
       If all criteria are met, this calls OnEstablished() on every connection
       and then OnEstablishedCall() on the manager.
      */
    virtual void CheckEstablished();

    /**Call back to indicate that the call has been established.
       At this point in time there are no connections left in the call.

       The default behaviour is to call OpalManager::OnEstablishedCall().
      */
    virtual void OnEstablished();

    /**Get the call clearand reason for this connection shutting down.
       Note that this function is only generally useful in the
       H323EndPoint::OnConnectionCleared() function. This is due to the
       connection not being cleared before that, and the object not even
       exiting after that.

       If the call is still active then this will return NumOpalCallEndReasons.
      */
    OpalCallEndReason GetCallEndReason() const { return callEndReason; }

    /**Set the call clearance reason.
       An application should have no cause to use this function. It is present
       for the H323EndPoint::ClearCall() function to set the clearance reason.
      */
    void SetCallEndReason(
      OpalCallEndReason reason   /// Reason for clearance of connection.
    );

    /**Clear call.
       This releases all connections currently attached to the call. Note that
       this function will return quickly as the release and disposal of the
       connections is done by another thread.
     */
    void Clear(
      OpalCallEndReason reason = EndedByLocalUser, /// Reason for call clearing
      PSyncPoint * sync = NULL
    );

    /**Call back to indicate that the call has been cleared.
       At this point in time there are no connections left in the call.

       The default behaviour is to call OpalManager::OnClearedCall().
      */
    virtual void OnCleared();

    /**Clean up calls and connections.
       This function is called from a background thread created by the
       OpalManager and checks for released connections to clean up.

       Returns TRUE if there are no connections left.

       This would not normally be called by an application.
      */
    BOOL GarbageCollection();
  //@}

  /**@name Connection management */
  //@{
    /**Add a connection.
       This is an internal called by the OpalConnection constructor to add
       the newly created instance to the calls internal structures.
      */
    void AddConnection(
      OpalConnection * connection  /// Connection to add.
    );

    /**Call back for alerting.

       The default behaviour is to call SetAlerting() on all the other
       connections in the call.
      */
    virtual void OnAlerting(
      OpalConnection & connection   /// Connection that indicates it is alerting
    );

    /**A call back function whenever a connection is "connected".
       This indicates that a connection to an endpoint was connected. That
       is the endpoint received acknowledgement via whatever protocol it uses
       that the connection may now start media streams.

       In the context of H.323 this means that the CONNECT pdu has been
       received.

       The default behaviour is to call SetConnected() on all other
       connections in the call.
      */
    virtual void OnConnected(
      OpalConnection & connection   /// Connection that indicates it is alerting
    );

    /**A call back function whenever a connection is released.

       The default behaviour releases the remaining connection if there is
       only one left.
      */
    virtual void OnReleased(
      OpalConnection & connection   /// Connection that was established
    );

    /**Release a connection.
       This is an internal function called by the OpalConnection::Release
       function to remove it from the calls internal structures. Note that the
       connection is not destroyed immediately, as this is done in another
       thread.
      */
    void Release(
      OpalConnection * connection  /// Connection to add.
    );

    /**Get the other party's connection object.
       This will return the other party in the call. It will return NULL if
       there is no other party yet, or there are more than two parties in the
       call. Usefull during certain stages during initial call set up.
      */
    OpalConnection * GetOtherPartyConnection(
      const OpalConnection & connection  /// Source requesting formats
    ) const;

    /**Get the number of active connections in call.
      */
    PINDEX GetConnectionCount() const { return activeConnections.GetSize(); }

    /**Get the specified active connection in call.
      */
    OpalConnection & GetConnection(PINDEX idx) const { return activeConnections[idx]; }
  //@}

  /**@name Media management */
  //@{
    /**Get the media formats of the other connections in call.
       This returns the intersection of all the media formats that all
       connections in the call, except the one provided as a parameter,
       are capable of.

       This will also add to the list all media formats for which there are
       transcoders registered.
      */
    OpalMediaFormatList GetMediaFormats(
      const OpalConnection & connection  /// Source requesting formats
    );

    /**Open transmitter media streams for each connection.
       If the connection parameter is not NULL then it is exempt from the
       opening of media streams.
      */
    virtual BOOL OpenSourceMediaStreams(
      const OpalMediaFormatList & mediaFormats, /// Optional media format to open
      unsigned sessionID,                       /// Session to start stream on
      const OpalConnection * connection = NULL  /// Exempt connection
    );

    /**Connect up the media streams on the connections.
     */
    virtual BOOL PatchMediaStreams(
      const OpalConnection & connection, /// Source connection
      OpalMediaStream & source           /// Source media stream to patch
    );

    /**See if the media can bypass the local host.
     */
    virtual BOOL IsMediaBypassPossible(
      const OpalConnection & connection,  /// Source connection
      unsigned sessionID                  /// Session ID for media channel
    ) const;
  //@}

  /**@name User indications */
  //@{
    /**Call back for remote endpoint has sent user input as a string.

       The default behaviour call OpalConnection::SetUserInput() which
       saves the value so the GetUserInput() function can return it.
      */
    virtual void OnUserInputString(
      OpalConnection & connection,  /// Connection input has come from
      const PString & value         /// String value of indication
    );

    /**Call back for remote enpoint has sent user input as tones.
       If duration is zero then this indicates the beginning of the tone. If
       duration is non-zero then it indicates the end of the tone output.

       The default behaviour calls connection.OnUserInputString(tone) if there
       are no other connections in the call, otherwise it calls
       SendUserInputTone() for each of the other connections in the call.
      */
    virtual void OnUserInputTone(
      OpalConnection & connection,  /// Connection input has come from
      char tone,                    /// Tone received
      int duration                  /// Duration of tone in milliseconds
    );
  //@}

  /**@name Member variable access */
  //@{
    /**Get the manager for this endpoint.
     */
    OpalManager & GetManager() const { return manager; }

    /**Get the internal identifier token for this connection.
     */
    const PString & GetToken() const { return myToken; }

    /**Get the A party for the call.
       Note this will be available even after the A party connection has been
       released from the call.
     */
    const PString & GetPartyA() const { return partyA; }

    /**Get the B party for the call.
       Note this will be available even after the B party connection has been
       released from the call. Also this will only be the first B party if the
       object represents a conference call with more that 2 parties.
     */
    const PString & GetPartyB() const { return partyB; }

    /**Set the B party for a call.
       This is used when we wish to make two outgoing calls and bridge them.
       When the OnConnected() call back occurs for the first outgoing call
       (the A-Party) then this variable ised to make teh second outgoing call.
      */
    void SetPartyB(
      const PString & b
    ) { partyB = b; }

    /**Get the time the call started.
     */
    const PTime & GetStartTime() const { return startTime; }
  //@}


  protected:
    void InternalReleaseConnection(PINDEX activeIndex, OpalCallEndReason reason);

    OpalManager & manager;

    PString myToken;

    PString partyA;
    PString partyB;
    PTime   startTime;

    OpalCallEndReason callEndReason;

    OpalConnectionList activeConnections;
    OpalConnectionList garbageConnections;
    PMutex             inUseFlag;
};


PLIST(OpalCallList, OpalCall);
PDICTIONARY(OpalCallDict, OpalGloballyUniqueID, OpalCall);


#endif // __OPAL_CALL_H


// End of File ///////////////////////////////////////////////////////////////
