/*
 *
 * Inter Asterisk Exchange 2
 * 
 * Open Phone Abstraction Library (OPAL)
 *
 * Describes the IAX2 extension of the OpalConnection class.
 *
 * Copyright (c) 2005 Indranet Technologies Ltd.
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
 * The Initial Developer of the Original Code is Indranet Technologies Ltd.
 *
 * The author of this code is Derek J Smithies
 *
 *  $Log: iax2con.h,v $
 *  Revision 1.8  2006/08/09 03:46:39  dereksmithies
 *  Add ability to register to a remote Asterisk box. The iaxProcessor class is split
 *  into a callProcessor and a regProcessor class.
 *  Big thanks to Stephen Cook, (sitiveni@gmail.com) for this work.
 *
 *  Revision 1.7  2006/06/16 01:47:08  dereksmithies
 *  Get the OnHold features of IAX2 to work correctly.
 *  Thanks to Stephen Cook, (sitiveni@gmail.com) for this work.
 *
 *  Revision 1.6  2005/09/05 01:19:43  dereksmithies
 *  add patches from Adrian Sietsma to avoid multiple hangup packets at call end,
 *  and stop the sending of ping/lagrq packets at call end. Many thanks.
 *
 *  Revision 1.5  2005/08/26 03:07:38  dereksmithies
 *  Change naming convention, so all class names contain the string "IAX2"
 *
 *  Revision 1.4  2005/08/25 03:26:06  dereksmithies
 *  Add patch from Adrian Sietsma to correctly set the packet timestamps under windows.
 *  Many thanks.
 *
 *  Revision 1.3  2005/08/24 04:56:25  dereksmithies
 *  Add code from Adrian Sietsma to send FullFrameTexts and FullFrameDtmfs to
 *  the remote end.  Many Thanks.
 *
 *  Revision 1.2  2005/08/04 08:14:17  rjongbloed
 *  Fixed Windows build under DevStudio 2003 of IAX2 code
 *
 *  Revision 1.1  2005/07/30 07:01:32  csoutheren
 *  Added implementation of IAX2 (Inter Asterisk Exchange 2) protocol
 *  Thanks to Derek Smithies of Indranet Technologies Ltd. for
 *  writing and contributing this code
 *
 *
 *
 *
 *
 */

#ifndef IAX_CONNECTION_H
#define IAX_CONNECTION_H

#include <ptlib.h>
#include <opal/connection.h>

#include <iax2/frame.h>
#include <iax2/iedata.h>
#include <iax2/processor.h>
#include <iax2/callprocessor.h>
#include <iax2/safestrings.h>
#include <iax2/sound.h>

class IAX2EndPoint;


////////////////////////////////////////////////////////////////////////////////
/**This class handles all data associated with a call to one remote computer.
   It runs a separate thread, which wakes in response to an incoming sound 
   block from a sound device, or inresponse to an incoming ethernet packet, or in
   response from the UI for action (like sending dtmf)
   
   It is a thread, and runs when activated by outside events.*/
class IAX2Connection : public OpalConnection
{ 
  PCLASSINFO(IAX2Connection, OpalConnection);
  
 public:  
  /**@name Construction/Destruction functions*/
  //@{
  
  /**Construct a connection given the endpoint. 
   */
  IAX2Connection(
    OpalCall & call,             ///Owner call for connection
    IAX2EndPoint & endpoint,     ///Owner iax endpoint for connection
    const PString & token,       ///Token to identify the connection
    void *userData,              ///Specific user data for this call
    const PString & remoteParty, ///Url we are calling or getting called by
    const PString & remotePartyName = PString::Empty() ///The name of the remote party
  );
  
  /**Destroy this connection, but do it nicely and let attached sound objects
     close first.
  */
  ~IAX2Connection();
  //@}

  /**@name General worker methods*/
  //@{
  
  /**Handle a received IAX frame. This may be a mini frame or full frame */
  //void IncomingEthernetFrame (IAX2Frame *frame);
  
  /**Test to see if it is a status query type iax frame (eg lagrq) and handle it. If the frame
     is a status query, and it is handled, return TRUE */
  //static BOOL IsStatusQueryEthernetFrame(IAX2Frame *frame);
    
  /**Return reference to the endpoint class */
  IAX2EndPoint & GetEndPoint() { return endpoint; }
  
  /**Invoked by the User interface, which causes the statistics (count of in/out packets)
     to be printed*/
  void ReportStatistics();
  
  /**Release the current connection.
     This removes the connection from the current call. The call may
     continue if there are other connections still active on it. If this was
     the last connection for the call then the call is disposed of as well.
     
     Note that this function will return quickly as the release and
     disposal of the connections is done by another thread.
     
     The ConnectionRun that manages packets going into/out of the
     IAX2Connection will continue to run, and will send the appropriate
     IAX@ hangup messages. Death of the Connection thread will happen when the 
     OnReleased thread of the OpalConnection runs */
  void Release( CallEndReason reason = EndedByLocalUser /// Reason for call release
		);
  
    /**Clear a current call.
       This sends a hangup frame to the remote endpoint, and then calls
       OpalConnection::ClearCall();
      */    
  void ClearCall(
      CallEndReason reason = EndedByLocalUser /// Reason for call clearing
    );

  /**Cause the call to end now, but do not send any iax hangup frames etc */
  void EndCallNow(
      CallEndReason reason = EndedByLocalUser /// Reason for call clearing
      );

  /**Provided as a link between the iax endpoint and the iax processor */
  void SendDtmf(PString dtmf);

  /** sending text fullframes **/
  virtual BOOL SendUserInputString(const PString & value );
  
  /** sending dtmf  - which is 1 char per IAX2FullFrameDtmf on the frame.**/
  virtual BOOL SendUserInputTone(char tone, unsigned duration );


  /**Send appropriate packets to the remote node to indicate we will accept this call.
     Note that this method is called from the endpoint thread, (not this IAX2Connection's thread*/
  void AcceptIncomingCall();
  
  /**Report if this Connection is still active */
  BOOL IsCallTerminating() { return iax2Processor->IsCallTerminating(); }
  
  /**Indicate to remote endpoint an alert is in progress.  If this is
     an incoming connection and the AnswerCallResponse is in a
     AnswerCallDeferred or AnswerCallPending state, then this function
     is used to indicate to that endpoint that an alert is in
     progress. This is usually due to another connection which is in
     the call (the B party) has received an OnAlerting() indicating
     that its remoteendpoint is "ringing".

     The default behaviour is pure.
  */
  BOOL SetAlerting(
			   const PString & calleeName,   /// Name of endpoint being alerted.
			   BOOL withMedia                /// Open media with alerting
			   ); 
  
  /**Indicate to remote endpoint we are connected.
     
  The default behaviour is pure.
  */
  BOOL SetConnected();       
  
  /**Get the data formats this connection is capable of operating.
     This provides a list of media data format names that a
     OpalMediaStream may be created in within this connection.
     
     The default behaviour is pure.
  */
  OpalMediaFormatList GetMediaFormats() const { return remoteMediaFormats; }
  
  
  /**Open a new media stream.  This will create a media stream of 
     subclass OpalIAXMediaStream.
     
     The sessionID parameter is (in this case) not needed by a
     particular media stream and is ignored.
     
     
     Note that media streams may be created internally to the
     underlying protocol. This function is not the only way a stream
     can come into existance.
  */
  OpalMediaStream * CreateMediaStream(
				      const OpalMediaFormat & mediaFormat, /// Media format for stream
				      unsigned sessionID,                  /// Session number for stream
				      BOOL isSource                        /// Is a source stream
				      );

  /**Give the call token a value. The call token is the ipaddress of
     the remote node concatented with the remote nodes src
     number. This is guaranteed to be unique.  Sadly, if this
     connection is setting up the cal, the callToken is not known
     until receipt of the first packet from the remote node.

     However, if this connection is created in response to a call,
     this connection can determine the callToken on examination of
     that incoming first packet */

  void SetCallToken(PString newToken);

  /**Return the string that identifies this IAX2Connection instance */
  PString GetCallToken() { return iax2Processor->GetCallToken(); }

  /**Transmit IAX2Frame to remote endpoint,
    It is only called by the the IAXProcessor class. */
  void TransmitFrameToRemoteEndpoint(IAX2Frame *src);
 
  /**Handle a sound packet received from the sound device. 
     
  Now onsend this to the remote endpoint. */
  void PutSoundPacketToNetwork(PBYTEArray *sund);

  /**Handle a sound packet received from the network.
     Return the media frame (IAX2MiniFrame or IAX2FullFrame) to the audio play stream */
  IAX2Frame *GetSoundPacketFromNetwork(DWORD minBuffer, DWORD maxBuffer) { return iax2Processor->GetSoundPacketFromNetwork(minBuffer, maxBuffer); }

  /**Get information on Remote class (remote node address & port + source & dest call number.) */
  IAX2Remote & GetRemoteInfo() { return iax2Processor->GetRemoteInfo(); }

  /**Get the sequence number info (inSeqNo and outSeqNo) */
  IAX2SequenceNumbers & GetSequenceInfo() { return iax2Processor->GetSequenceInfo(); }
  
  /**Get the call start time */
  const PTimeInterval & GetCallStartTick() { return iax2Processor->GetCallStartTick(); } 

  /**Call back for an incoming call.
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
     
     This method is called right at the very beginning of the call sequence 
  */
  virtual BOOL OnIncomingConnection();
  
    /**We have received a packet from the remote iax endpoint, requeting a call.
       Now, we use this method to invoke the opal components to do their bit.

    This method is called after OnIncomingConnection().*/
    virtual void OnSetUp();

    /**Call back for remote party being alerted.
       This function is called after the connection is informed that the
       remote endpoint is "ringing". Generally some time after the
       SetUpConnection() function was called, this is function is called.

       If FALSE is returned the connection is aborted.

       If an application overrides this function, it should generally call the
       ancestor version for correct operation. An application would typically
       only intercept this function if it wishes to do some form of logging.
       For this you can obtain the name of the caller by using the function
       OpalConnection::GetRemotePartyName().

       The default behaviour calls the OpalEndPoint function of the same name.
     */
  virtual void OnAlerting();


  /**A call back function whenever a connection is "connected".  This
     indicates that an iax link to an endpoint has been made &
     verified. Now, the endpoint has to receive information on
     this. Further, media has to start to flow.
     
     In the context of IAX, this means that the accept and
     verification packets have been procesed, and or that we have
     received the first media from the remote end.
  */
  virtual void OnConnected();
  
  /**A call back function whenever a connection is established.  This
       indicates that a connection to an endpoint was
       established. This differs from OnConnected() in that the media
       streams are started.

       In the context of IAX2 this means we have received the first
       full frame of media from the remote endpoint

       The default behaviour calls the OpalEndPoint function of the
       same name.
      */
  virtual void OnEstablished();



  /**Clean up the termination of the connection.  This function can
     do any internal cleaning up and waiting on background threads
     that may be using the connection object.
     
     Note that there is not a one to one relationship with the
     OnEstablishedConnection() function. This function may be called
     without that function being called. For example if
     SetUpConnection() was used but the call never completed.
     
     Classes that override this function should make sure they call
     the ancestor version for correct operation.
     
     An application will not typically call this function as it is
     used by the OpalManager during a release of the connection.
     
     The default behaviour calls the OpalEndPoint function of the
     same name.
  */
  void OnReleased();



  /**Start an outgoing connection.
     This function will initiate the connection to the remote entity, for
     example in H.323 it sends a SETUP, in SIP it sends an INVITE etc.
     
     The behaviour at the opal level is pure. Here, the method is defined.
  */
  BOOL SetUpConnection();


  /**Return the bitmask which specifies the possible codecs we
     support.  The supported codecs are a bitmask of values defined by
     FullFrameVoice::AudioSc */
  PINDEX GetSupportedCodecs();
  
  /**Return the bitmask which specifies the preferred codec. The
     selected codec is in the binary value defined by
     FullFrameVoice::AudioSc */
  PINDEX GetPreferredCodec();

  /**Fill the OpalMediaFormatList which describes the remote nodes
     capabilities */
  void BuildRemoteCapabilityTable(unsigned int remoteCapability, unsigned int format);

     
  /** The local capabilites and remote capabilites are in
      OpalMediaFormatList classes, stored in this class.  The local
      capbilities have already been ordered by the users
      preferences. The first entry in the remote capabilities is the
      remote endpoints preferred codec. Now, we have to select a codec
      to use for this connection. The selected codec is in the binary
      value defined by FullFrameVoice::AudioSc */
  unsigned int ChooseCodec();
  
  /**Return TRUE if the current connection is on hold.*/
  virtual BOOL IsConnectionOnHold();
  
  /**Take the current connection off hold*/
  virtual void RetrieveConnection();
  
  /**Put the current connection on hold, suspending all media streams.*/
  virtual void HoldConnection();
  
  /**Signal that the remote side has put the connection on hold*/
  void RemoteHoldConnection();
  
  /**Signal that the remote side has retrieved the connection*/
  void RemoteRetrieveConnection();
  
  IAX2CallProcessor * GetCallProcessor() { return iax2Processor; }
  
  /**Set the username for when we connect to a remote node
     we use it as authentication.  Note this must only be
     used before SetUpConnection is ran.  This is optional
     because some servers do not required authentication, also
     if it is not set then the default iax2Ep username 
     will be used instead.*/
  void SetUserName(PString & inUserName) { userName = inUserName; };
  
  /**Get the username*/
  PString GetUserName() const { return userName; };
  
  /**Set the password for when we connect to a remote node
     we use it as authentication.  Note this must only be
     used before SetUpConnection is ran.  This is optional
     because some servers do not required authentication, also
     if it is not set then the default iax2Ep password 
     will be used instead.*/
  void SetPassword(PString & inPassword) { password = inPassword; };
  
  /**Get the password*/
  PString GetPassword() const { return password; };

 protected:
  
  /**Username for the iax2CallProcessor*/
  PString userName;
  
  /**Password for the iax2CallProcessor*/
  PString password;
  
  /**@name Internal, protected methods, which are invoked only by this
     thread*/
  //@{

  /**Global variable which specifies IAX2 protocol specific issues */
  IAX2EndPoint    &endpoint;

  /**The list of media formats (codecs) the remote enpoint can use.
     This list is defined on receiving a particular Inforation Element */
  OpalMediaFormatList remoteMediaFormats;

  /**The list of media formats (codecs) this end wants to use.
     This list is defined on constructing this IAX2Connection class */
  OpalMediaFormatList localMediaFormats;
    
  /**The thread that processes the list of pending frames on this class */
  IAX2CallProcessor * iax2Processor;
  
  /**Whether the connection is on hold locally */
  BOOL            local_hold;
  
  /**Whether the connection is on hold remotely */
  BOOL            remote_hold;

  //@}
};


////////////////////////////////////////////////////////////////////////////////



#endif // IAX_CONNECTION_H
/* The comment below is magic for those who use emacs to edit this file. */
/* With the comment below, the tab key does auto indent to 2 spaces.     */

/*
 * Local Variables:
 * mode:c
 * c-file-style:linux
 * c-basic-offset:2
 * End:
 */



