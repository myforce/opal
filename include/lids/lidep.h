/*
 * lidep.h
 *
 * Line Interface Device EndPoint
 *
 * Open Phone Abstraction Library
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
 * The Original Code is Open H323 Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Portions of this code were written with the assisance of funding from 
 * Quicknet Technologies, Inc. http://www.quicknet.net.
 * 
 * Contributor(s): ______________________________________.
 *
 * $Log: lidep.h,v $
 * Revision 1.2014  2003/06/02 02:56:17  rjongbloed
 * Moved LID specific media stream class to LID source file.
 *
 * Revision 2.12  2003/03/24 07:18:29  robertj
 * Added registration system for LIDs so can work with various LID types by
 *   name instead of class instance.
 *
 * Revision 2.11  2003/03/17 10:26:59  robertj
 * Added video support.
 *
 * Revision 2.10  2003/03/06 03:57:46  robertj
 * IVR support (work in progress) requiring large changes everywhere.
 *
 * Revision 2.9  2002/09/16 02:52:34  robertj
 * Added #define so can select if #pragma interface/implementation is used on
 *   platform basis (eg MacOS) rather than compiler, thanks Robert Monaghan.
 *
 * Revision 2.8  2002/09/04 05:27:55  robertj
 * Added ability to set default line name to be used when the destination
 *   does not match any lines configured.
 *
 * Revision 2.7  2002/01/22 05:00:54  robertj
 * Revamp of user input API triggered by RFC2833 support
 *
 * Revision 2.6  2001/11/13 06:25:56  robertj
 * Changed SetUpConnection() so returns BOOL as returning
 *   pointer to connection is not useful.
 *
 * Revision 2.5  2001/10/15 04:29:35  robertj
 * Removed answerCall signal and replaced with state based functions.
 *
 * Revision 2.4  2001/10/03 05:56:15  robertj
 * Changes abndwidth management API.
 *
 * Revision 2.3  2001/08/17 01:11:52  robertj
 * Added ability to add whole LID's to LID endpoint.
 * Added ability to change the prefix on POTS and PSTN endpoints.
 *
 * Revision 2.2  2001/08/01 06:23:55  robertj
 * Changed to use separate mutex for LIDs structure to avoid Unix nested mutex problem.
 *
 * Revision 2.1  2001/08/01 05:18:51  robertj
 * Made OpalMediaFormatList class global to help with documentation.
 *
 * Revision 2.0  2001/07/27 15:48:24  robertj
 * Conversion of OpenH323 to Open Phone Abstraction Library (OPAL)
 *
 */

#ifndef __LIDS_LIDEP_H
#define __LIDS_LIDEP_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


#include <opal/endpoint.h>
#include <lids/lid.h>


class OpalLineConnection;


/**This class describes and endpoint that handles LID lines.
   This is the ancestor class to the LID endpoints that handle PSTN lines
   and POTS lines respectively.
 */
class OpalLIDEndPoint : public OpalEndPoint
{
  PCLASSINFO(OpalLIDEndPoint, OpalEndPoint);

  public:
  /**@name Construction */
  //@{
    /**Create a new endpoint.
     */
    OpalLIDEndPoint(
      OpalManager & manager,  /// Manager of all endpoints.
      const PString & prefix, /// Prefix for URL style address strings
      unsigned attributes     /// Bit mask of attributes endpoint has
    );

    /// Make sure thread has stopped before exiting.
    ~OpalLIDEndPoint();
  //@}

  /**@name Overrides from OpalEndPoint */
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
      void * userData = NULL  /// Arbitrary data to pass to connection
    );
  //@}

  /**@name Connection management */
  //@{
    OpalLineConnection * CreateConnection(
      OpalCall & call,        /// Call that owns the connection
      OpalLine & line,        /// Line connection is to use
      void * userData,        /// Arbitrary user data from MakeConnection
      const PString & number  /// Number to dial in outgoing call
    );
  //@}

  /**@name LID management */
  //@{
    /**Add a line to the endpoint.
       Note that once the line is added it is "owned" by the endpoint and
       should not be deleted directly. Use the RemoveLine() function to
       delete the line.

       Returns TRUE if the lines device was open and the line was added.
      */
    BOOL AddLine(
      OpalLine * line
    );

    /**Remove a line from the endpoint.
       The line is removed from the endpoints processing and deleted.
      */
    void RemoveLine(
      OpalLine * line
    );

    /**Remove a line from the endpoint.
       The line is removed from the endpoints processing and deleted.
      */
    void RemoveLine(
      const PString & token
    );

    /**Remove all lines from the endpoint.
       The line is removed from the endpoints processing and deleted. All
       devices are also deleted from the endpoint
      */
    void RemoveAllLines();

    /**Add all lines on a device to the endpoint.
       Note that once the line is added it is "owned" by the endpoint and
       should not be deleted directly. Use the RemoveLine() function to
       delete the line.

       Note the device should already be open or no lines are added.

       Returns TRUE if at least one line was added.
      */
    BOOL AddLinesFromDevice(
      OpalLineInterfaceDevice & device  /// Device to add lines
    );

    /**Remove all lines on a device from the endpoint.
       The lines are removed from the endpoints processing and deleted.
      */
    void RemoveLinesFromDevice(
      OpalLineInterfaceDevice & device  /// Device to remove lines
    );

    /**Add a line interface devices to the endpoint by name.
       This will add the registered OpalLineInterfaceDevice descendent and
       all of the lines that it has to the endpoint. It uses a strng to create
       and open the device as would be returned from the 

       Returns TRUE if at least one line was added.
      */
    BOOL AddDeviceNames(
      const PStringArray & descriptors  /// Device descritptions to add
    );

    /**Add a line interface device to the endpoint by name.
       This will add the registered OpalLineInterfaceDevice descendent and
       all of the lines that it has to the endpoint. It uses a strng to create
       and open the device as would be returned from the 

       Returns TRUE if at least one line was added.
      */
    BOOL AddDeviceName(
      const PString & descriptor  /// Device descritption to add
    );

    /**Add a line interface device to the endpoint.
       This will add the OpalLineInterfaceDevice descendent and all of the
       lines that it has to the endpoint.

       The lid is then "owned" by the endpoint and will be deleted
       automatically when the endpoint is destroyed.

       Note the device should already be open or no lines are added.

       Returns TRUE if at least one line was added.
      */
    BOOL AddDevice(
      OpalLineInterfaceDevice * device    /// Device to add
    );

    /**Remove the device and all its lines from the endpoint.
       The device will be automatically deleted.
      */
    void RemoveDevice(
      OpalLineInterfaceDevice * device  /// Device to remove
    );

    /**Get the line by name.
       The lineName parameter may be "*" to matche the first line.

       If the enableAudio flag is TRUE then the EnableAudio() function is
       called on the line and it is returns only if successful. This
       effectively locks the line for exclusive use of the caller.
      */
    OpalLine * GetLine(
      const PString & lineName,  /// Name of line to get.
      BOOL enableAudio = FALSE   /// Flag to enable audio on the line.
    ) const;

    /**Set the default line to be used on call.
       If the lineName is "*" then the first available line is used.
      */
    void SetDefaultLine(
      const PString & lineName  /// Name of line to set to default.
    );
  //@}

  protected:
    PDECLARE_NOTIFIER(PThread, OpalLIDEndPoint, MonitorLines);
    virtual void MonitorLine(OpalLine & line);

    OpalLIDList  devices;
    OpalLineList lines;
    PString      defaultLine;
    PMutex       linesMutex;
    PThread    * monitorThread;
    PSyncPoint   exitFlag;
};


/**This class describes an endpoint that handles PSTN lines.
   This is the ancestor class to the LID endpoints that handle PSTN lines
   and POTS lines respectively.
 */
class OpalPSTNEndPoint : public OpalLIDEndPoint
{
  PCLASSINFO(OpalLIDEndPoint, OpalLIDEndPoint);

  public:
  /**@name Construction */
  //@{
    /**Create a new endpoint.
     */
    OpalPSTNEndPoint(
      OpalManager & manager,  /// Manager of all endpoints.
      const char * prefix = "pstn" /// Prefix for URL style address strings
    ) : OpalLIDEndPoint(manager, prefix, HasLineInterface) { }
  //@}
};


/**This class describes an endpoint that handles PSTN lines.
   This is the ancestor class to the LID endpoints that handle PSTN lines
   and POTS lines respectively.
 */
class OpalPOTSEndPoint : public OpalLIDEndPoint
{
  PCLASSINFO(OpalPOTSEndPoint, OpalLIDEndPoint);

  public:
  /**@name Construction */
  //@{
    /**Create a new endpoint.
     */
    OpalPOTSEndPoint(
      OpalManager & manager,  /// Manager of all endpoints.
      const char * prefix = "pots" /// Prefix for URL style address strings
    ) : OpalLIDEndPoint(manager, prefix, CanTerminateCall) { }
  //@}
};


/**This class describes the LID based codec capability.
 */
class OpalLineConnection : public OpalConnection
{
  PCLASSINFO(OpalLineConnection, OpalConnection);

  public:
  /**@name Construction */
  //@{
    /**Create a new connection.
     */
    OpalLineConnection(
      OpalCall & call,              /// Owner calll for connection
      OpalLIDEndPoint & endpoint,   /// Endpoint for LID connection
      OpalLine & line,              /// Line to make connection on
      const PString & number        /// Number to call on line
    );
  //@}

  /**@name Overrides from OpalConnection */
  //@{
    /**Start an outgoing connection.
       This function will initiate the connection to the remote entity, for
       example in H.323 it sends a SETUP, in SIP it sends an INVITE etc.

       The default behaviour does.
      */
    virtual BOOL SetUpConnection();

    /**Indicate to remote endpoint an alert is in progress.
       If this is an incoming connection and the AnswerCallResponse is in a
       AnswerCallDeferred or AnswerCallPending state, then this function is
       used to indicate to that endpoint that an alert is in progress. This is
       usually due to another connection which is in the call (the B party)
       has received an OnAlerting() indicating that its remoteendpoint is
       "ringing".

       The default behaviour starts the ring back tone.
      */
    virtual BOOL SetAlerting(
      const PString & calleeName,   /// Name of endpoint being alerted.
      BOOL withMedia                /// Open media with alerting
    );

    /**Indicate to remote endpoint we are connected.

       The default behaviour stops the ring back tone.
      */
    virtual BOOL SetConnected();

    /**Clean up the termination of the connection.
       This function can do any internal cleaning up and waiting on background
       threads that may be using the connection object.

       Note that there is not a one to one relationship with the
       OnEstablishedConnection() function. This function may be called without
       that function being called. For example if SetUpConnection() was used
       but the call never completed.

       The return value indicates if the connection object is to be deleted. A
       value of FALSE can be returned and it then someone elses responsibility
       to free the memory used.

       Classes that override this function should make sure they call the
       ancestor version for correct operation.

       An application will not typically call this function as it is used by
       the OpalManager during a release of the connection.

       The default behaviour calls starts playing the busy tone and calls the
       ancestor function.
      */
    virtual BOOL OnReleased();

    /**Get the destination address of an incoming connection.
       The default behaviour collects a DTMF number terminated with a '#' or
       if no digits were entered for a time (default 3 seconds). If no digits
       are entered within a longer time time (default 30 seconds), then an
       empty string is returned.
      */
    virtual PString GetDestinationAddress();

    /**Get the data formats this connection is capable of operating.
       This provides a list of media data format names that a
       OpalMediaStream may be created in within this connection.

       The default behaviour returns the capabilities of the LID line.
      */
    virtual OpalMediaFormatList GetMediaFormats() const;

    /**Open a new media stream.
       This will create a media stream of an appropriate subclass as required
       by the underlying connection protocol. For instance H.323 would create
       an OpalRTPStream.

       The sessionID parameter may not be needed by a particular media stream
       and may be ignored. In the case of an OpalRTPStream it us used.

       Note that media streams may be created internally to the underlying
       protocol. This function is not the only way a stream can come into
       existance.

       The default behaviour is pure.
     */
    virtual OpalMediaStream * CreateMediaStream(
      const OpalMediaFormat & mediaFormat, /// Media format for stream
      unsigned sessionID,                  /// Session number for stream
      BOOL isSource                        /// Is a source stream
    );

    /**Send a user input indication to the remote endpoint.
       This sends an arbitrary string as a user indication. If DTMF tones in
       particular are required to be sent then the SendIndicationTone()
       function should be used.

       The default behaviour plays the DTMF tones on the line.
      */
    virtual BOOL SendUserInputString(
      const PString & value                   /// String value of indication
    );

    /**Send a user input indication to the remote endpoint.
       This sends DTMF emulation user input. If something other than the
       standard tones need be sent use the SendUserInputString() function.

       The default behaviour plays the DTMF tone on the line.
      */
    virtual BOOL SendUserInputTone(
      char tone,    /// DTMF tone code
      int duration  /// Duration of tone in milliseconds
    );

    /**Play a prompt to the connection before rading user indication string.

       For example the LID connection would play a dial tone.

       The default behaviour does nothing.
      */
    virtual BOOL PromptUserInput(
      BOOL play   /// Flag to start or stop playing the prompt
    );
  //@}

  /**@name Call handling functions */
  //@{
    /**Handle a new connection from the LID line.
      */
    void StartIncoming();

    /**Check for line hook state, DTMF tone for user indication etc.
      */
    void Monitor(
      BOOL offHook
    );
  //@}


  protected:
    OpalLIDEndPoint & endpoint;
    OpalLine        & line;
    BOOL              wasOffHook;
    unsigned          answerRingCount;
    BOOL              requireTonesForDial;

    PDECLARE_NOTIFIER(PThread, OpalLineConnection, HandleIncoming);
    PThread         * handlerThread;
};


/**This class describes a media stream that transfers data to/from a Line
   Interface Device.
  */
class OpalLineMediaStream : public OpalMediaStream
{
    PCLASSINFO(OpalLineMediaStream, OpalMediaStream);
  public:
  /**@name Construction */
  //@{
    /**Construct a new media stream for Line Interface Devices.
      */
    OpalLineMediaStream(
      const OpalMediaFormat & mediaFormat, /// Media format for stream
      unsigned sessionID,                  /// Session number for stream
      BOOL isSource,                       /// Is a source stream
      OpalLine & line                      /// LID line to stream to/from
    );
  //@}

  /**@name Overrides of OpalMediaStream class */
  //@{
    /**Open the media stream.

       The default behaviour sets the OpalLineInterfaceDevice format and
       calls Resume() on the associated OpalMediaPatch thread.
      */
    virtual BOOL Open();

    /**Close the media stream.

       The default does nothing.
      */
    virtual BOOL Close();

    /**Read raw media data from the source media stream.
       The default behaviour reads from the OpalLine object.
      */
    virtual BOOL ReadData(
      BYTE * data,      /// Data buffer to read to
      PINDEX size,      /// Size of buffer
      PINDEX & length   /// Length of data actually read
    );

    /**Write raw media data to the sink media stream.
       The default behaviour writes to the OpalLine object.
      */
    virtual BOOL WriteData(
      const BYTE * data,   /// Data to write
      PINDEX length,       /// Length of data to read.
      PINDEX & written     /// Length of data actually written
    );

    /**Indicate if the media stream is synchronous.
       Returns TRUE for LID streams.
      */
    virtual BOOL IsSynchronous() const;
  //@}

  protected:
    OpalLine & line;
    BOOL       useDeblocking;
    unsigned   missedCount;
    BYTE       lastSID[4];
    BOOL       lastFrameWasSignal;
};



#endif // __LIDS_LIDEP_H


// End of File ///////////////////////////////////////////////////////////////
