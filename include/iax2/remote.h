/*
 *
 *
 * Inter Asterisk Exchange 2
 * 
 * A class to describe the node we are talking to.
 * 
 * Open Phone Abstraction Library (OPAL)
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
 *  $Log: remote.h,v $
 *  Revision 1.1  2005/07/30 07:01:32  csoutheren
 *  Added implementation of IAX2 (Inter Asterisk Exchange 2) protocol
 *  Thanks to Derek Smithies of Indranet Technologies Ltd. for
 *  writing and contributing this code
 *
 *
 *
 *
 */

#ifndef REMOTE_H
#define REMOTE_H

#include <ptlib.h>
#include <ptlib/sockets.h>

#ifdef P_USE_PRAGMA
#pragma interface
#endif

class FullFrame; 
/**A simple class which contains the different source and dest call
   numbers, and the remote address + remote port */
class Remote : public PObject
{ 
  PCLASSINFO(Remote, PObject);
  
 public:
  
  /**Constructor*/
  Remote();
  
  virtual ~Remote() { };
  
  /**Call number as used at the destination of this data frame. If we are
     receiving this packet, it refers to our call number. */
  PINDEX DestCallNumber() { return destCallNumber; }
  
  /**Call number as used at the source of this data frame. If we are
     receiving this packet, it refers to the call number at the remote
     host. */ 
  PINDEX SourceCallNumber() { return sourceCallNumber; }
 
  /**Pretty print this remote structure (address & port) to the designated stream*/
  virtual void PrintOn(ostream & strm) const; 
  
  /**Define which is used to indicate the call number is undefined */
  enum {
    callNumberUndefined = 0xffff 
  };
  
  /** Return the current value of the ip address used by the other end of this call */
  PIPSocket::Address RemoteAddress() { return remoteAddress; }
  
  /** return the current value of the port at the other end of this call */
  PINDEX   RemotePort() { return remotePort; }
  
  /**Copy data from supplied Remote structure to this class */
  void Assign(Remote &);
  
  /**Set the remote address as used by this class */
  void SetRemoteAddress(PIPSocket::Address &newVal) { remoteAddress = newVal; }
  
  /**Set the remote address as used by this class */
  void SetRemoteAddress(int newVal) { remoteAddress = newVal; }
  
  /**Set the remote port, as used bye this class */
  void SetRemotePort (PINDEX newVal) { remotePort = newVal; }
  
  /**Set the Source Call Number, as used by this class */
  void SetSourceCallNumber(PINDEX newVal) { sourceCallNumber = newVal; }
  
  /**Set the Dest Call Number, as used by this class */
  void SetDestCallNumber(PINDEX newVal) { destCallNumber = newVal; }
  
  /**Return true if remote port & address & destCallNumber & source
     call number match up.  This is used when finding a Connection
     that generated an ethernet frame which is to be transmitted*/
  BOOL operator == (Remote & other);
  
  /**Return true if remote port & address & destCallNumber==sourceCallNumber match up.
     This is used when finding a Connection to process an incoming ethernet frame */
  BOOL operator *= (Remote & other);
  
  
  /**Returns true if these are are different */
  BOOL operator != (Remote & other);
  
  
 protected:
  /**Call number at the local computer.*/
  PINDEX       sourceCallNumber;    
  
  /**Call number at the remote computer.*/
  PINDEX       destCallNumber;      
  
  /**Ip address used by the remote endpoint*/
  PIPSocket::Address remoteAddress; 
  
  /**Port number used by the remote endpoint.*/
  PINDEX               remotePort;    
  
};

////////////////////////////////////////////////////////////////////////////////
/**A class to store the timestamp and sequence number in a indexable
  fashion.  

  This class will be used as a key into the sorted list, which is
  declared below  (PacketIdList).  This class is required because 
  pwlib's dictionaries requires the key to be a descendant from a PObject

  The 32 bit timestamp is left shifted by 8 bits, and the
  result is added to the 8 bit seqno value */
class FrameIdValue : public PObject
{
  PCLASSINFO(FrameIdValue, PObject);
 public:
  /**Constructor. to timestamp<<8   +  sequenceNumber*/
  FrameIdValue (PINDEX timeStamp, PINDEX seqVal);

  /**Constructor to some value */
  FrameIdValue (PINDEX val);

  /**Retrieve this timestamp */
  PINDEX GetTimeStamp() const;

  /**Retrieve this sequence number.*/
  PINDEX GetSequenceVal() const;

  /**Retrieve the bottom 32 bits.  In this call, the data is assumed
     to be no timestamp, and sequence number could be > 255. This is
     used for the iseqno of transmitted packets */
  PINDEX GetPlainSequence() const;

  /**Pretty print this data to the designated stream*/
  virtual void PrintOn(ostream & strm) const;

  /**Declare this method so that all comparisons (as used in sorted
     lists) work. correctly*/
  virtual Comparison Compare(const PObject & obj) const;

 protected:
  /**The combination of time and sequence number is stored in this
     element, which is a pwlib construct of 64 bits */
  PUInt64 value;
};

////////////////////////////////////////////////////////////////////////////////

/**A class to store a unique identifing number, so we can keep track
of all frames we have sent and received. This will be sued to keep the
iseqno correct (for use in sent frames), and to ensure that we never
send two frames with the same iseqno and timestamp pair.*/
PDECLARE_SORTED_LIST(PacketIdList, FrameIdValue)
#ifdef DOC_PLUS_PLUS
class PacketIdList : public PSortedList
{
#endif
  
  /**Return true if a FrameIdValue object is found in the list that
   * matches the value in the supplied arguement*/
  BOOL Contains(FrameIdValue &src);
  
  /**Return the value at the beginning of the list. Use this value as
     the InSeqNo of this endpoint.*/
  PINDEX GetFirstValue();
  
  /**For the supplied frame, append to this list */
  void AppendNewFrame(FullFrame &src);
  
  /**Pretty print this listto the designated stream*/
  virtual void PrintOn(ostream & strm) const;	
  
 protected:
  /**Remove all the contiguous oldest values, which is used for
     determining the correct iseqno.  This endpoints iseqno is
     determined from::: iseqno is�always
     highest_contiguously_recieved_oseq+1�
  */
  void RemoveOldContiguousValues();
};

////////////////////////////////////////////////////////////////////////////////
/**A structure to hold incoming and outgoing sequence numbers */
class SequenceNumbers
{
 public:
  /**Constructor, which sets the in and out sequence numbers to zero*/
  SequenceNumbers() 
    { ZeroAllValues();   };
  
  /**Destructor, which is provided as this class contains virtual methods*/
  virtual ~SequenceNumbers() { }
  
  /**Initialise to Zero values */
  void ZeroAllValues();
  
  /**Read the incoming sequence number */
  PINDEX InSeqNo();
  
  /**Read the outgoing sequence number */
  PINDEX OutSeqNo();
  
  /**Report true if the frame has inSeqNo and outSeqNo of zero,
     indicating if it is a "New" packet. */
  BOOL IsSequenceNosZero();

  /**Assign new value to the in (or expected) seq number */
  void SetInSeqNo(PINDEX newVal);
  
  /**Assign a new value to the sequence number used for outgoing frames */
  void SetOutSeqNo(PINDEX newVal);
  
  /**Assign the sequence nos as appropropriate for when we are sending a
   * sequence set in a ack frame */
  void SetAckSequenceInfo(SequenceNumbers & other);
  
  /**Comparison operator - tests if sequence numbers are different */
  BOOL  operator !=(SequenceNumbers &other);
  
  /**Comparison operator - tests if sequence numbers are equal */
  BOOL operator ==(SequenceNumbers &other);
  
  /**Increment this sequences outSeqNo, and assign the results to the source arg */
  void MassageSequenceForSending(FullFrame &src /*<!src will be transmitted to the remote node */
				 );

  /**Take the incoming frame, and increment seq nos by some multiple
     of 256 to bring them into line with the current values. Use the
     wrapAound member variable to do this.*/
  void WrapAroundFrameSequence(SequenceNumbers & src);
  
  /** We have received a message from the remote node. Check sequence numbers
   * are ok. If ok reply TRUE  */
  BOOL IncomingMessageIsOk(FullFrame &src /*<!frame to be compared with current data base.*/  );
  
  /**Copy the sequence info from the source argument to this class */
  void CopyContents(SequenceNumbers &src);
  
  /**Report  the contents as a string*/
  PString AsString() const;
  
  /**Pretty print in and out sequence numbers  to the designated stream*/
  virtual void PrintOn(ostream & strm) const;

  /**Report TRUE if this sequnece info is the very first packet
     received from a remote node, where we have initiated the call */
  BOOL IsFirstReply() { return (inSeqNo == 1) && (outSeqNo == 0); }

  /**Add an offset to the inSeqNo and outSeqNo variables */
  void AddWrapAroundValue(PINDEX newOffset);

 protected:
  /** Packet number (next incoming expected)*/
  PINDEX inSeqNo;  
  
  /** Packet number (outgoing) */
  PINDEX outSeqNo; 

  /**Mutex to protect access to this structrue while doing seqno changes*/
  PMutex mutex;

  /**Last sent time stamp - ensure 3 ms gap between time stamps. */
  PINDEX lastSentTimeStamp;

  /**Dictionary of timestamp and OutSeqNo for frames received by  this iax device */
  PacketIdList receivedLog;
};

#endif

/* The comment below is magic for those who use emacs to edit this file. */
/* With the comment below, the tab key does auto indent to 4 spaces.     */

/*
 * Local Variables:
 * mode:c
 * c-file-style:linux
 * c-basic-offset:2
 * End:
 */

