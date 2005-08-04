/*
 *
 * Inter Asterisk Exchange 2
 * 
 * Open Phone Abstraction Library (OPAL)
 *
 * Define the classes that carry information over the ethernet.
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
 *  $Log: frame.h,v $
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
 */

#ifndef FRAME_H
#define FRAME_H


class Frame;
class FrameList;
class FullFrame;
class FullFrameCng;
class FullFrameDtmf;
class FullFrameHtml;
class FullFrameImage;
class FullFrameNull;
class FullFrameProtocol;
class FullFrameSessionControl;
class FullFrameText;
class FullFrameVideo;
class FullFrameVoice;
class IAX2EndPoint;
class IAX2Processor;
class IeList;
class MiniFrame;
class Transmitter;

#include <iax2/ies.h>
#include <iax2/remote.h>
#include <ptlib.h>
#include <ptlib/sockets.h>

#ifdef P_USE_PRAGMA
#pragma interface
#endif



/**Base class for holding data to be sent to a remote endpoint*/
class Frame :  public PObject
{
  PCLASSINFO(Frame, PObject);
 public:
  /**construction */
  Frame(IAX2EndPoint &_endpoint);
  
  /**Destructor - which is empty */
  virtual ~Frame() {}
  
  /**Wait on the designated socket for an incoming UDP packet. This
     method is only called by the receiver. This method does NO interpretation*/
  BOOL ReadNetworkPacket(PUDPSocket &sock);
  
  /**Interpret the data from the read process*/
  virtual BOOL ProcessNetworkPacket();
  
  /**True if this is a full frame */
  virtual BOOL IsFullFrame();
  
  /**True if this is encrypted */
  BOOL IsEncrypted();
  
  /**True if it is a video frame */
  BOOL IsVideo() const;
  
  /**True if is is an audio frame */
  BOOL IsAudio() const;
  
  /**Pointer to the beginning of the media (after the header) in this packet.
     The low level frame has no idea on headers, so just return pointer to beginning
     of data. */
  virtual BYTE *GetMediaDataPointer() { return data.GetPointer(); }
  
  /**Number of bytes in the media section of this packet. The low
     level frame has no idea on headers, so just return the number of
     bytes in the packet.*/
  virtual PINDEX GetMediaDataSize() { return DataSize();}
  
  /**Reporting function, to describe the number of bytes in this packet */
  PINDEX DataSize() { return data.GetSize(); }
  
  /**Get the value of the Remote structure */
  Remote & GetRemoteInfo() { return remote; }
  
  /**Obtain a pointer to the current position in the incoming data array */
  const BYTE * GetDataPointer() { return data + currentReadIndex; }
  
  /** How many bytes are unread in the incoming data array */
  PINDEX   GetUnReadBytes() { return data.GetSize() - currentReadIndex; }
  
  /**Cause the header bytes for this particular frame type to be written to the internal array */
  virtual BOOL WriteHeader() { return FALSE; }
  
  /**Send this packet on the specified socket to the remote host. This method is only
     called by the transmiter.*/
  virtual BOOL TransmitPacket(PUDPSocket &sock);
  
  /**Pretty print this frame data to the designated stream*/
  virtual void PrintOn(ostream & strm) const;
  
  /**Calculate the timestamp value, given the call start time*/
  static DWORD CalcTimeStamp(const PTime & callStartTime);

  /**Write the timestamp value, in preparation for writing the
     header. When sending a packet, the timestamp is written at packet
     construction.*/
  virtual void BuildTimeStamp(const PTime & callStartTime);
  
  /**Return a reference to the endpoint structure */
  IAX2EndPoint & GetEndpoint() { return endpoint; }
  
  /**Globally unique ID string for this frame, to help track frames */
  PString IdString() const { return PString("FR-ID#") + PString(frameIndex); }
  
  
  /**Get the timestamp as used by this class*/
  DWORD  GetTimeStamp() { return timeStamp; }
  
  /** Report flag stating that this call must be active when this frame is transmitted*/
  virtual BOOL CallMustBeActive() { return TRUE; }     
  
  /**Specify the type of this frame. */
  enum IaxFrameType {
    undefType       = 0,     /*!< full frame type is  Undefined                     */
    dtmfType        = 1,     /*!< full frame type is  DTMF                          */
    voiceType       = 2,     /*!< full frame type is  Audio                         */
    videoType       = 3,     /*!< full frame type is  Video                         */
    controlType     = 4,     /*!< full frame type is  Session Control               */
    nullType        = 5,     /*!< full frame type is  NULL - frame ignored.         */
    iaxProtocolType = 6,     /*!< full frame type is  IAX protocol specific         */
    textType        = 7,     /*!< full frame type is  text message                  */
    imageType       = 8,     /*!< full frame type is  image                         */
    htmlType        = 9,     /*!< full frame type is  HTML                          */
    cngType         = 10,    /*!< full frame type is  CNG (comfort noise generation */
    numFrameTypes   = 11     /*!< the number of defined IAX2 frame types            */
  };
  
  /**Access the current value of the variable controlling frame type,
     which is used when reading data from the network socket. */
  IaxFrameType GetFrameType() { return frameType; }
  
  
  /**Method supplied here to provide basis for descendant classes.

   Whenever a frame is transmitted, this method will be called.*/
  virtual void InitialiseHeader(IAX2Processor * /*processor*/) { }
  
  /**Return true if this frame should be retransmitted. Acks are never retransmitted. cmdNew are retransmitted.*/
  BOOL CanRetransmitFrame() const {return canRetransmitFrame; } 
  
  /**Get the string which uniquely identifies the IAXConnection that sent this frame */
  PString GetConnectionToken() const { return connectionToken; }

  /**Set the string which uniquely identifies the IAXConnection that is responsible for
     this frame */
  void SetConnectionToken(PString newToken) { connectionToken = newToken; } 

  /**Create the connection token id, which uniquely identifies the
     connection to process this call */
  void BuildConnectionTokenId();

  

 protected:
  
  /**Specification of the location (address, call number etc) of the far endpoint */
  Remote  remote;
  
  /**Examine frame type, which is used in deciding how to read a full frame from the network */
  BOOL LookAheadFullFrameType();
  
  /**Variable specifying the IAX type of frame that this is. Used only in reading from the network */
  IaxFrameType frameType;
  
  /** Read 1 byte from the internal area, (Internal area is filled when reading the packet in). Big Endian.*/
  BOOL          Read1Byte(BYTE & res);
  
  /** Read 2 bytes from the internal area, (Internal area is filled when reading the packet in)  Big Endian.*/
  BOOL          Read2Bytes(PINDEX & res);
  
  /** Read 2 bytes from the internal area, (Internal area is filled when reading the packet in)  Big Endian.*/
  BOOL          Read2Bytes(WORD & res);
  
  /** Read 4 bytes from the internal area, (Internal area is filled when reading the packet in)  Big Endian.*/
  BOOL          Read4Bytes(DWORD & res);
  
  /** Write 1 byte to the internal area, as part of writing the header info */
  void          Write1Byte(BYTE newVal);
  
  /** Write 1 byte to the internal area, as part of writing the header info. Send only the lowest 8 bits of source*/
  void          Write1Byte(PINDEX newVal);
  
  /** Write 2 bytes to the internal area, as part of writing the header info  Big Endian.*/
  void          Write2Bytes(PINDEX newVal);
  
  /** Write 4 bytes to the internal area, as part of writing the header info  Big Endian.*/
  void          Write4Bytes(unsigned int newVal);
  
  /** Initialise all internal storage in this structrue */
  void          ZeroAllValues();
  
  /**Reference to the global variable of this program */
  IAX2EndPoint      & endpoint;
  
  /**Internal storage array, ready for sending to remote node, or ready for receiving from remote node*/
  PBYTEArray         data;
  
  /**Flag to indicate if this data is encrypted or not */
  BOOL               encrypted;
  
  /**Flag to indicate if this is a MiniFrame or FullFrame */
  BOOL               isFullFrame;
  
  /**Flag to indicate if this is a MiniFrame with video */
  BOOL               isVideo;
  
  /**Flag to indicate if this is a MiniFrame with audio */
  BOOL               isAudio;
  
  /**Index of where we are reading from the internal data area */
  PINDEX               currentReadIndex;  
  
  /**Index of where we are writing to  the internal data area */  
  PINDEX               currentWriteIndex;  
  
  /**unsigned 32 bit representaiton of the time of this day */
  DWORD                timeStamp;  
  
  /** Internal variable that uniquely identifies this frame */
  int                frameIndex;
  
  /**Indicate if this frame can be retransmitted*/
  BOOL               canRetransmitFrame;

  /**Connection Token, which uniquely identifies the IAXConnection
     that sent this frame. The token will (except for the first setup
     packet) uniquely identify the IAXConnection that is to receive
     this incoming frame.  */
  PString            connectionToken;

  /**The time stamp to use, for those cases when the user demands a particular
   * timestamp on construction. */
  PINDEX presetTimeStamp;
};

/////////////////////////////////////////////////////////////////////////////    
/**Class to manage a mini frame, which is sent unreliable to the remote endpoint*/
class MiniFrame : public Frame
{
  PCLASSINFO(MiniFrame, Frame);
 public:
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet*/
  MiniFrame(Frame & srcFrame);
  
  /** Construction from an endpoint, to create an empty frame. */
  MiniFrame(IAX2EndPoint & _endpoint); 
  
  /**Construction from an encoded audio array (stored in a
     PBYTEArray), in preparation to sending to remote node.  The
     constructor will not delete the supplied PBYTEArray structure.

     TimeStamp will be calculated from time since call started, if the users timestamp is 0.
     If the users timeStamp is non zero, the frames timestamp will be this.
  */
  MiniFrame(IAX2Processor * con, PBYTEArray &sound, BOOL isAudio, PINDEX usersTimeStamp = 0);

  /**Destructor*/
  virtual ~MiniFrame();
  
  /** Process the incoming frame some more, but process it as this frame type demands*/
  virtual BOOL ProcessNetworkPacket();
  
  /**Write the header to the internal data area */
  virtual BOOL WriteHeader();
  
  /**Pretty print this frame data to the designated stream*/
  virtual void PrintOn(ostream & strm) const;
  
  /**Pointer to the beginning of the media (after the header) in this packet */
  virtual BYTE *GetMediaDataPointer();
  
  /**Number of bytes in the media section of this packet. */
  virtual PINDEX GetMediaDataSize();
  
  /**Fix the timestamp in this class, after being shrunk in the MiniFrames */
  void AlterTimeStamp(PINDEX newValue);
  
  /**Given the supplied Connection class, write the first 12 bytes of the frame.
     This method is called by the frame constructors, in preparation for transmission.
     This method is never called when processing a received frame.

     Whenever a frame is transmitted, this method will be called.*/ 
  virtual void InitialiseHeader(IAX2Processor *processor);
  
 protected:
  /**Initialise valus in this class to some preset value */
  void ZeroAllValues();
};

/////////////////////////////////////////////////////////////////////////////    
/**Class to handle a full frame, which is sent reliably to the remote endpoint */
class FullFrame : public Frame
{
  PCLASSINFO(FullFrame, Frame);
 public:
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet*/
  FullFrame(Frame & srcFrame);
  
  /** Construction from an endpoint, to create an empty frame. 
      In this case, the class is filled with the various methods*/
  FullFrame(IAX2EndPoint  &_endpoint);
  
  /**Delete this frame now, but first we have to delete every timer on it.
   */
  virtual ~FullFrame();
  
  /**Return True if this an ack frame */
  BOOL IsAckFrame() { return isAckFrame; }
  
  /**Return True if this is a PING frame */
  BOOL IsPingFrame();

  /**Return True if this is a NEW frame */
  BOOL IsNewFrame();

  /**Return True if this is a LAGRQ frame */
  BOOL IsLagRqFrame();

  /**Return True if this is a LAGRP frame */
  BOOL IsLagRpFrame();

  /**Return True if this is a PONG frame */
  BOOL IsPongFrame();

  /**Return True if this is a AuthReq frame */
  BOOL IsAuthReqFrame();

  /**Return True if this is a VNAK frame */
  BOOL IsVNakFrame();

  /**Return True if this FullFrame is of a type that increments the InSeqNo */
  BOOL FrameIncrementsInSeqNo();

  /**True if this is a full frame - always returns true as this is a full frame. */
  virtual BOOL IsFullFrame() { return TRUE; }  
  
  /** Initialise to zero all the members of this particular class */
  void ZeroAllValues();
  
  /** Process the incoming frame some more, but process it as a full frame */
  virtual BOOL ProcessNetworkPacket();
  
  /**Send this packet on the specified socket to the remote host. This method is only
     called by the transmiter.*/
  virtual BOOL TransmitPacket(PUDPSocket &sock);
  
  /**Get text descrption of this frame type*/
  PString GetFullFrameName() const;
  
  /**Get text description of the subclass contents*/
  virtual PString GetSubClassName() const
    { return PString(" subclass=") + PString(subClass); }
  
  /**Stop the timer, so this packet is not retransmitted. Mark packet as dead. This happens
     when a packet has been received that matches one of the previously sent packets. */
  void MarkDeleteNow();
  
  /**Pointer to the beginning of the media (after the header) in this packet */
  virtual BYTE *GetMediaDataPointer();
  
  /**Number of bytes in the media section of this packet. */
  virtual PINDEX GetMediaDataSize();
  
  /**Deteremine the current value of the subClass variable */
  PINDEX GetSubClass() const { return subClass; }
  
  /**Dry the current value of the subClass variable */
  void SetSubClass(int newValue) { subClass = newValue;}
  
  /**Write the header for this class to the internal data array. 12
     bytes of data are writen.  The application developer must write the
     remaining bytes, before transmiting this frame. */
  virtual BOOL WriteHeader();
  
  /**Alter the two bytes for in and out sequence values. (in the header)*/
  void ModifyFrameHeaderSequenceNumbers(PINDEX inNo, PINDEX outNo);

  /**Alter the four bytes for this frames timestamp. It is required,
     when transmitting full frames, that there is a 3ms interval to
     last full frame in the timestamps. This is required by
     limitations in the handline of time in asterisk. */
  void ModifyFrameTimeStamp(PINDEX newTimeStamp);

  /**Mark this frame as having (or not having) information elements*/
  virtual BOOL InformationElementsPresent() { return FALSE; }  
  
  /**Get flag to see if this frame is ready to be sent (or resent). In other words, has the timer expired?*/
  BOOL  SendFrameNow() { return sendFrameNow; }
  
  /**Get flag to see if this frame is ready for deletion. In other words. Has it been sent too many times? */
  BOOL  DeleteFrameNow() { return deleteFrameNow; }
  
  /**Get the sequence number info (inSeqNo and outSeqNo) */
  SequenceNumbers & GetSequenceInfo() { return sequence; }
  
  /**Pretty print this frame data to the designated stream*/
  virtual void PrintOn(ostream & strm) const;
  
  /**Mark this frame as having been resent (set bit 7 of data[2])*/
  void MarkAsResent();
  
  /**Compare this FullFrame with another full frame, which is used when determining if we are
     dealing with a frame we have already processed */
  BOOL operator *= (FullFrame & other);
  
  /**enum to define if the call must be active when sending this
     frame*/
  enum ConnectionRequired {
    callActive,      /*!< the call must be active when sending frame*/
    callIrrelevant   /*!< the call may, or may not be, active when sending frame*/
  };
  
  /**Return the FullFrame type represented here (voice, protocol, session etc*/
  virtual BYTE GetFullFrameType() { return 0; }
  
 protected:
  /** Report flag stating that this call must be active when this frame is transmitted*/
  virtual BOOL CallMustBeActive() { return callMustBeActive; }
  
  /**Turn the 8 bit subClass value into a 16 bit representation */
  void UnCompressSubClass(BYTE a);
  
  /**Turn the 16 bit subClass value into a 8 bit representation */
  int  CompressSubClass();
  
  /**Set flag in internal data area to indicate this frame is resent */
  void MarkDataResent();
  
  /**Mark this frame as not to be sent, and not to be deleted */
  void ClearListFlags();
  
  /**Given the supplied Connection class, write the first 12 bytes of the frame.
     This method is called by the frame construcors, in preparation for transmission.
     This method is never called when processing a received frame.

     Whenever a frame is transmitted, this method will be called.*/
  virtual void InitialiseHeader(IAX2Processor *processor);
  
#ifdef DOC_PLUS_PLUS
  /** pwlib constructs to cope with timeout, when transmitting a full frame.  This
      happens when a full frame has not been acknowledged in the
      required time period. This frame will be resent.*/
  void OnTransmissionTimeout(PTimer &, INT);
#else
  PDECLARE_NOTIFIER(PTimer, FullFrame, OnTransmissionTimeout);
#endif
  /** The timer which is used to test for no reply to this frame (on transmission) */
  PTimer transmissionTimer;
  
  /** integer variable specifying the uncompressed subClass value for this particular frame */
  PINDEX subClass;
  
  /**Time to wait between retries */
  PTimeInterval retryDelta;     
  
  /**Time delta between call start and sending (or receiving)*/
  PTimeInterval timeOffset;     
  
  /**Number of retries this frame has undergone*/
  PINDEX       retries;        
  
  /** Internal variables specifying the retry time periods (in milliseconds) */
  enum RetryTime {
    minRetryTime = 500,    /*!< 500 milliseconds     */
    maxRetryTime = 010000, /*!< 10 seconds           */
    maxRetries   = 10,     /*!< number of retries    */
  };
  
  /**Class holding the sequence numbers, which is used by all classes which have a FullFrame ancestor. */
  SequenceNumbers sequence;
  
  /**List flag, indicating if this frame ready for sending*/
  BOOL         sendFrameNow;   
  
  /**List flag, this frame is ready for deletion (too many retries)*/
  BOOL         deleteFrameNow; 
  
  /**A tracking flag to indicate this fame has been resent*/
  BOOL         packetResent;   
  
  /** Flag stating that this call must be active when this frame is transmitted  */
  BOOL callMustBeActive;
  
  /** flag to indicate if this is an ack frame */
  BOOL isAckFrame;  
};

/////////////////////////////////////////////////////////////////////////////    

/**Used for transmitting dtmf characters in a reliable fashion. One
   frame per dtmf character.  No data is carried in the data
   section */

class FullFrameDtmf : public FullFrame
{
  PCLASSINFO(FullFrameDtmf, FullFrame);
 public:
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet*/
  FullFrameDtmf(Frame & srcFrame);
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet*/
  FullFrameDtmf(FullFrame & srcFrame);
  
  /**Construction from a Connection class. 
     Classes generated from this are then on sent to a remote endpoint. */
  FullFrameDtmf(IAX2Processor *processor,       /*!< Iax Processor from which this frame originates      */ 
		PString  subClassValue/*!< IAX protocol command for remote end to process   */
		);
  
  
  /**Get text description of the subclass contents*/
  virtual PString GetSubClassName() const; 
  
  /**enum comtaining the possible subclass value for these dtmf frames */
  enum DtmfSc {
    dtmf0 = 48,     /*!< DTMF character 0     */
    dtmf1 = 49,     /*!< DTMF character 1     */
    dtmf2 = 50,     /*!< DTMF character 2     */
    dtmf3 = 51,     /*!< DTMF character 3     */
    dtmf4 = 52,     /*!< DTMF character 4     */
    dtmf5 = 53,     /*!< DTMF character 5     */
    dtmf6 = 54,     /*!< DTMF character 6     */
    dtmf7 = 55,     /*!< DTMF character 7     */
    dtmf8 = 56,     /*!< DTMF character 8     */
    dtmf9 = 57,     /*!< DTMF character 9     */
    dtmfA = 65,     /*!< DTMF character A     */
    dtmfB = 66,     /*!< DTMF character B     */
    dtmfC = 67,     /*!< DTMF character C     */
    dtmfD = 68,     /*!< DTMF character D     */
    dtmfStar = 42,  /*!< DTMF character *     */
    dtmfHash = 35   /*!< DTMF character #     */
  };
  
  /**Return the FullFrame type represented here (voice, protocol, session etc*/
  virtual BYTE GetFullFrameType() { return dtmfType; }
  
 protected:
};

/////////////////////////////////////////////////////////////////////////////    
/**Used for transmitting voice packets in a relaible
   fashion. Typically, one is sent at the start of the call, and then
   at regular intervals in the call to keep the timestamps in sync.
   
   This class has the ability to build audio codecs, and report on available formats.
   
   The contents the data section is the compressed audio frame. */
class FullFrameVoice : public FullFrame
{
  PCLASSINFO(FullFrameVoice, FullFrame);
 public:
  
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet*/
  FullFrameVoice(Frame & srcFrame);
  
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet*/
  FullFrameVoice(FullFrame & srcFrame);
  
  /**Construction from an encoded audio array (stored in a
     PBYTEArray), in preparation to sending to remote node.  The
     constructor will not delete the supplied PBYTEArray structure.

     The full frame will use the specified timeStamp, if it is > 0.  If the
     specified timeStamp == 0, the timeStamp will be calculated from when the
     call started.
  */
  FullFrameVoice(IAX2Processor *processor, PBYTEArray &sound, 
		 PINDEX usersTimeStamp = 0);
  
  /**Declare an empty destructor */
  virtual ~FullFrameVoice();

  /**Get text description of the subclass contents*/
  virtual PString GetSubClassName() const;
  
  /**Get text description of the subclass contents, given the supplied
     argument*/
  static PString GetSubClassName(unsigned short testValue)
     { return GetSubClassName((unsigned int) testValue); }

  /**Get text description of the subclass contents, given the supplied
     argument*/
  static PString GetSubClassName(unsigned int testValue);
  
  /**Get text description of the subclass contents, given the supplied
   argument.  The name returned is that recoginised by the OPAL
   library. */
  static PString GetOpalNameOfCodec(PINDEX testValue);
  
  /**Get text description of the subclass contents, given the supplied argument*/
  static PString GetSubClassName(int testValue) 
    { return GetSubClassName((unsigned short) testValue); }
  
  /**Turn the OPAL string (which describes the codec) into a AudioSc value. If there is no conversion
     found, return 0. */
  static unsigned short OpalNameToIax2Value(const PString opalName);

  /**enum comtaining the possible (uncompressed) subclass value for these voice frames */
  enum AudioSc {
    g7231    = 1,         /*!< G.723.1 audio format in this frame        */
    gsm      = 2,         /*!< GSM audio format in this frame            */
    g711ulaw = 4,         /*!< G.711 uLaw audio format in this frame     */
    g711alaw = 8,         /*!< G.711 ALaw audio format in this frame     */
    mp3      = 0x10,      /*!< MPeg 3 audio format in this frame         */
    adpcm    = 0x20,      /*!< ADPCM audio format in this frame          */
    pcm      = 0x40,      /*!< PCM audio format in this frame            */
    lpc10    = 0x80,      /*!< LPC10 audio format in this frame          */
    g729     = 0x100,     /*!< G.729 audio format in this frame          */
    speex    = 0x200,     /*!< Speex audio format in this frame          */
    ilbc     = 0x400,     /*!< ILBC audio format in this frame           */
    supportedCodecs = 11  /*!< The number of codecs defined by this enum */
  };
  
  /**Return the FullFrame type represented here (voice, protocol, session etc*/
  virtual BYTE GetFullFrameType() { return voiceType; }
};
/////////////////////////////////////////////////////////////////////////////    
/**Used for transmitting video packets in a relaible
   fashion. Typically, one is sent at the start of the call, and then
   at regular intervals in the call to keep the timestamps in sync.
   
   The contents the data section is compressed video. */
class FullFrameVideo : public FullFrame
{
  PCLASSINFO(FullFrameVideo, FullFrame);
 public:
  
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet*/
  FullFrameVideo(Frame & srcFrame);
  
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet*/
  FullFrameVideo(FullFrame & srcFrame);
  
  /**Get text description of the subclass contents*/
  virtual PString GetSubClassName() const;
  
  /**enum comtaining the possible (uncompressed) subclass value for these video frames */
  enum VideoSc {
    jpeg  = 0x10000,   /*!< Jpeg video format in this frame     */
    png   = 0x20000,   /*!< PNG video format in this frame      */
    h261  = 0x40000,   /*!< H.261 video format in this frame    */
    h263  = 0x80000    /*!< H.263 video fromat in this frame    */
  };
  
  /**Return the FullFrame type represented here (voice, protocol, session etc*/
  virtual BYTE GetFullFrameType() { return videoType; }
 protected:
};

/////////////////////////////////////////////////////////////////////////////    
/**Used for sending  Control Frames. These are used to manipulate the session. 
   
Asterisk calls these AST_FRAME_CONTROLs

No data is carried in the data section */
class FullFrameSessionControl : public FullFrame
{
  PCLASSINFO(FullFrameSessionControl, FullFrame);
 public:
  
  /**enum comtaining the possible subclass value for these Session Control frames */
  enum SessionSc {
    hangup          = 1,     /*!< Other end has hungup    */
    ring            = 2,     /*!< Local ring    */
    ringing         = 3,     /*!< Remote end is ringing    */
    answer          = 4,     /*!< Remote end has answered    */
    busy            = 5,     /*!< Remote end is busy    */
    tkoffhk         = 6,     /*!< Make it go off hook    */
    offhook         = 7,     /*!< Line is off hook    */
    congestion      = 8,     /*!< Congestion (circuits busy)    */
    flashhook       = 9,     /*!< Flash hook    */
    wink            = 10,    /*!< Wink    */
    option          = 11,    /*!< Set a low-level option    */
    keyRadio        = 12,    /*!< Key Radio    */
    unkeyRadio      = 13,    /*!< Un-Key Radio    */
    callProgress    = 14,    /*!< Indicate PROGRESS    */
    callProceeding  = 15,    /*!< Indicate CALL PROCEEDING    */
    callOnHold      = 16,    /*!< Call has been placed on hold    */
    callHoldRelease = 17,    /*!< Call is no longer on hold    */
    stopSounds      = 255    /*!< Indicates the transition from ringback to bidirectional audio */
  };
  
  
  
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet*/
  FullFrameSessionControl(Frame & srcFrame);
  
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet*/
  FullFrameSessionControl(FullFrame & srcFrame);
  
  /**Construction from a Connection class. 
     Classes generated from this are then on sent to a remote endpoint. */
  FullFrameSessionControl(IAX2Processor *processor, /*!<Iax Processor from which this frame originates    */ 
			  PINDEX subClassValue/*!<IAX protocol command for remote end to process   */
			  );
  
  /**Construction from a Connection class. 
     Classes generated from this are then on sent to a remote endpoint. */
  FullFrameSessionControl(IAX2Processor *processor,     /*!< Iax Processor from which this frame originates */
			  SessionSc subClassValue /*!< IAX protocol command for remote end to process*/
			  );
  
  /**Declare an empty destructor*/
  virtual ~FullFrameSessionControl() { }

  /**Get text description of the subclass contents*/
  virtual PString GetSubClassName() const;
  
  /**Return the FullFrame type represented here (voice, protocol, session etc*/
  virtual BYTE GetFullFrameType() { return controlType; }
  
 protected:
};

/////////////////////////////////////////////////////////////////////////////    
/**Class for transfering Nothing. It is essentially a NO Op 
   It is used internally to indicate that the sound card should play a silence frame.
   
   These frames are sent reliably.
   There is no data in the subclass section.
   There is no data in the data section */
class FullFrameNull : public FullFrame
{
  PCLASSINFO(FullFrameNull, FullFrame);
 public:
  /**Construct an empty Full Frame.
     Classes generated like this are used to handle transmitted information */
  FullFrameNull(IAX2EndPoint & endpoint) : FullFrame(endpoint)   { }
  
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet.
     Classes generated like this are used to handle received data. */
  FullFrameNull(Frame & srcFrame);
  
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet.
     Classes generated like this are used to handle received data. */
  FullFrameNull(FullFrame & srcFrame);
  
  /**Get text description of the subclass contents*/
  virtual PString GetSubClassName() const { return  PString(""); }
  
  /**Return the FullFrame type represented here (voice, protocol, session etc*/
  virtual BYTE GetFullFrameType() { return nullType; }
  
 protected:
};

/////////////////////////////////////////////////////////////////////////////    
/**Handle IAX specific protocol issues. Used for initiating a call,
   closing a call, registration, reject a call etc.. These are used to
   manipulate the session.
   
   The data section contains information elements, or type Ie classes. */

class FullFrameProtocol : public FullFrame
{
  PCLASSINFO(FullFrameProtocol, FullFrame);
 public:
  
  /**enum comtaining the possible subclass value for these IAX protocol control frames */
  enum ProtocolSc {
    cmdNew       =  1,       /*!< Create a new call    */
    cmdPing      =  2,       /*!< Ping request, which is done on an open call. It is "Are you Alive    */
    cmdPong      =  3,       /*!< reply to a Ping    */
    cmdAck       =  4,       /*!< Acknowledge a Reliably sent full frame    */
    cmdHangup    =  5,       /*!< Request to terminate this call    */
    cmdReject    =  6,       /*!< Refuse to accept this call. May happen if authentication faile    */
    cmdAccept    =  7,       /*!< Allow this call to procee    */
    cmdAuthReq   =  8,       /*!< Ask remote end to supply authenticatio    */
    cmdAuthRep   =  9,       /*!< A reply, that contains authenticatio    */
    cmdInval     =  10,      /*!< Destroy this call immediatly    */
    cmdLagRq     =  11,      /*!< Initial message, used to measure the round trip time    */
    cmdLagRp     =  12,      /*!< Reply to cmdLagrq, which tells us the round trip time    */
    cmdRegReq    =  13,      /*!< Request fore Registration    */
    cmdRegAuth   =  14,      /*!< Registration is required for  authentication    */
    cmdRegAck    =  15,      /*!< Registration has been accepted    */
    cmdRegRej    =  16,      /*!< Registration has been rejected    */
    cmdRegRel    =  17,      /*!< Force the release of the current registration    */
    cmdVnak      =  18,      /*!< This indicates out of order frames, and can be read as voice not acknowledged */
    cmdDpReq     =  19,      /*!< Request the status of an entry for dialplan     */
    cmdDpRep     =  20,      /*!< Request status of an entry for  dialplan     */
    cmdDial      =  21,      /*!< Request that there is a dial (TBD) on a channel    */
    cmdTxreq     =  22,      /*!< Request a Transfer    */
    cmdTxcnt     =  23,      /*!< Connect up a  Transfer     */
    cmdTxacc     =  24,      /*!< Transfer has been accepted    */
    cmdTxready   =  25,      /*!< Transfer is ready to happen   */
    cmdTxrel     =  26,      /*!< Release a Transfer    */
    cmdTxrej     =  27,      /*!< Reject a Transfer   */
    cmdQuelch    =  28,      /*!< Stop media transmission    */
    cmdUnquelch  =  29,      /*!< Resume media transmission    */
    cmdPoke      =  30,      /*!< Query the remote endpoint (there is no open connection) */
    cmdPage      =  31,      /*!< Do a Page */
    cmdMwi       =  32,      /*!< Indicate : message waiting */
    cmdUnsupport =  33,      /*!< We have received an unsupported message */
    cmdTransfer  =  34,      /*!< Initiate the remote end to do a transfer */
    cmdProvision =  35,      /*!< Provision the remote end    */
    cmdFwDownl   =  36,      /*!< The remote end must download some firmware    */
    cmdFwData    =  37       /*!< This message contains firmware.    */
  };
  
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet.
     Classes generated like this are used to handle received data. */
  FullFrameProtocol(Frame & srcFrame);
  
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet.
     Classes generated like this are used to handle received data. */
  FullFrameProtocol(FullFrame & srcFrame);
  
  /**Construction from a Connection class. 
     Classes generated from this are then on sent to a remote endpoint. */
  FullFrameProtocol(IAX2Processor *processor,         /*!< Iax Processor from which this frame originates            */ 
		    PINDEX subClassValue,            /*!< IAX protocol command for remote end to process         */
		    ConnectionRequired needCon = FullFrame::callActive
		                                     /*!< this frame is only sent if the Connection class exists */
		    );
  
  /**Construction from an Connection class. 
     Classes generated from this are then on sent to a remote endpoint. */
  FullFrameProtocol(IAX2Processor *processor,         /*!< Iax Processor from which this frame originates            */ 
		    ProtocolSc  subClassValue,       /*!< IAX protocol command for remote end to process         */
		    ConnectionRequired needCon=FullFrame::callActive 
		                                     /*!< this frame is only sent if the Connection class exists */
		    );
  
  /**Construction from a Connection class. 
     Classes generated from this are then on sent to a remote endpoint.
     
     We have received a FullFrameProtocol, and this constructor is used to create a reply.
     Use the iseqno and time stamp from the supplied frame to construct the reply */
  FullFrameProtocol(IAX2Processor *processor,         /*!< Iax Processor from which this frame originates            */ 
		    ProtocolSc  subClassValue,       /*!< IAX protocol command for remote end to process         */
		    FullFrame *inReplyTo,            /*!< this message was sent in reply to this frame           */
		    ConnectionRequired needCon = FullFrame::callActive
		                                     /*!< this frame is only sent if the Connection class exists */
		    );
  
  /**Destructor, which deletes all current Information Elements */
  virtual ~FullFrameProtocol();
  
  /**Set internal variable to say that this frame does not need to be retransmitted*/
  void SetRetransmissionRequired();
  
  /**Mark this frame as having (or not having) information elements*/
  virtual BOOL InformationElementsPresent() { return !ieElements.IsEmpty(); }
  
  /**Get text description of the subclass contents*/
  virtual PString GetSubClassName() const; 
  
  /**Return a pointer to the n'th Ie in the internal list. If it is not
     there, a NULL is returned */
  Ie *GetIeAt(PINDEX i) {      return ieElements.GetIeAt(i); }
  
  /**Add a new Information Element (Ie) to the intenral list */
  void AppendIe(Ie *newElement) { ieElements.AppendIe(newElement); }
  
  /**Write the contents of the Ie internal list to the frame data array.
     This is usually done in preparation to transmitting this frame */
  void WriteIeAsBinaryData();
  
  /**Transfer the data (stored in the IeList) and place it in into
     the IeData class.  This is done when precessing a frame we
     have received from an external node, which has to be stored in the IeData class*/
  void CopyDataFromIeListTo(IeData &res);
  
  /**Look through the list of IEs, and look for remote capabability
     and preferred codec */
  void GetRemoteCapability(unsigned int & capability, unsigned int & preferred);

  /**Return the FullFrame type represented here (voice, protocol, session etc*/
  virtual BYTE GetFullFrameType() { return iaxProtocolType; }
  
 protected:
  
  /**Read the information elements from the incoming data array 
     to generate a list of information element classes*/
  BOOL ReadInformationElements();
  
  /**A list of the IEs read from/(or  written to) the data section of this frame,*/
  IeList ieElements;
};

/////////////////////////////////////////////////////////////////////////////    
/**Class for transfering text. These frames are sent reliably.
   There is no data in the subclass section.
   
   The text is carried in the data section.
*/
class FullFrameText : public FullFrame
{
  PCLASSINFO(FullFrameText, FullFrame);
 public:
  
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet*/
  FullFrameText(Frame & srcFrame);
  
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet*/
  FullFrameText(FullFrame & srcFrame);
  
  /**Get text description of the subclass contents*/
  virtual PString GetSubClassName() const;
  
  /**Return the FullFrame type represented here (voice, protocol, session etc*/
  virtual BYTE GetFullFrameType() { return textType; }
 protected:
};
/////////////////////////////////////////////////////////////////////////////    

/**Class for transfering images. These frames are sent reliably.
   The subclass describes the image compression format.
   
   The contents of the data section is the raw image data */
class FullFrameImage : public FullFrame
{
  PCLASSINFO(FullFrameImage, FullFrame);
 public:
  
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet*/
  FullFrameImage(Frame & srcFrame);
  
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet*/
  FullFrameImage(FullFrame & srcFrame);
  
  /**Get text description of the subclass contents*/
  virtual PString GetSubClassName() const;
  
  /**Return the FullFrame type represented here (voice, protocol, session etc*/
  virtual BYTE GetFullFrameType() { return imageType; }
 protected:
};

/////////////////////////////////////////////////////////////////////////////    

/**Class for transfering html. These frames are sent reliably.
   The subclass describes the html frame type.
   
   The contents of the data section is message specific */
class FullFrameHtml : public FullFrame
{
  PCLASSINFO(FullFrameHtml, FullFrame);
 public:
  
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet*/
  FullFrameHtml(Frame & srcFrame);
  
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet*/
  FullFrameHtml(FullFrame & srcFrame);
  
  /**Get text description of the subclass contents*/
  virtual PString GetSubClassName() const;
  
  /**Return the FullFrame type represented here (voice, protocol, session etc*/
  virtual BYTE GetFullFrameType() { return htmlType; }
 protected:
};

/////////////////////////////////////////////////////////////////////////////    
/**Class for transfering Cng (comfort noise generation). These frames are sent reliably.
   

The contents of the data section is message specific */
class FullFrameCng : public FullFrame
{
  PCLASSINFO(FullFrameCng, FullFrame);
 public:
  
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet*/
  FullFrameCng(Frame & srcFrame);
  
  /**Construction from a supplied dataframe.
     In this case, this class is filled from an incoming data packet*/
  FullFrameCng(FullFrame & srcFrame);
  
  /**Get text description of the subclass contents*/
  virtual PString GetSubClassName() const;
  
  /**Return the FullFrame type represented here (voice, protocol, session etc*/
  virtual BYTE GetFullFrameType() { return cngType; }
 protected:
};

/////////////////////////////////////////////////////////////////////////////    

PDECLARE_LIST (FrameList, Frame *)
#ifdef DOC_PLUS_PLUS     //This makes emacs bracket matching code happy.
/** A list of all frames waiting for processing
	 
    Note please, this class is thread safe.
     
   You do not need to protect acces to this class.
*/     
class FrameList : public Frame *  
{
#endif
 public:
  ~FrameList();
  
  /**Report the frames queued in this list*/
  void ReportList();
  
  /**Get pointer to last frame in the list. Remove this frame from the list */
  Frame *GetLastFrame();
  
  /**Removing item from list will not automatically delete it */
  void Initialise() {  DisallowDeleteObjects(); }
    
  /**True if this frame list is empty*/
  BOOL Empty() { return GetSize() == 0; }
  
  /**Copy to this frame the contents of the frameList pointed to by src*/
  void GrabContents(FrameList &src);
  
  /**Delete the frame that has been sent, which is waiting for this
   * reply. The reply is the argument. */
  void DeleteMatchingSendFrame(FullFrame *reply);
  
  /**Add the frame (supplied as an argument) to the end of this list*/
  void AddNewFrame(Frame *src);
  
  /**Get a list of frames to send, and delete the timed out frames */
  void GetResendFramesDeleteOldFrames(FrameList & framesToSend);
  
  /**Thread safe read of the number of elements on this list. */
  virtual PINDEX GetSize() { PWaitAndSignal m(mutex); return PAbstractList::GetSize(); }
  
  /**Mark every frame on this list as having been resent*/
  void MarkAllAsResent();
  
 protected:
  /**NON Thread safe read of the number of elements on this list. */
  virtual PINDEX GetEntries() { return PAbstractList::GetSize(); }
  
  /**Local variable which protects access. */
  PMutex mutex;
};


/////////////////////////////////////////////////////////////////////////////    


#endif // FRAME_H
/* The comment below is magic for those who use emacs to edit this file. */
/* With the comment below, the tab key does auto indent to 4 spaces.     */

/*
 * Local Variables:
 * mode:c
 * c-file-style:linux
 * c-basic-offset:2
 * End:
 */



