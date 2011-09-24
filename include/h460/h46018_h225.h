/*
 * h46018_h225.h
 *
 * H.460.18 H225 NAT Traversal class.
 *
 * h323plus library
 *
 * Copyright (c) 2008 ISVO (Asia) Pte. Ltd.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the General Public License (the  "GNU License"), in which case the
 * provisions of GNU License are applicable instead of those
 * above. If you wish to allow use of your version of this file only
 * under the terms of the GNU License and not to allow others to use
 * your version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GNU License. If you do not delete
 * the provisions above, a recipient may use your version of this file
 * under either the MPL or the GNU License."
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 *
 * The Initial Developer of the Original Code is ISVO (Asia) Pte. Ltd.
 *
 * Portions of this code were written with the assisance of funding from
 * triple-IT. http://www.triple-it.nl.
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#ifndef H46018_NAT
#define H46018_NAT

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <h323/h323pdu.h>


class H46018SignalPDU  : public H323SignalPDU
{
  public:
  /**@name Construction */
  //@{
    /** Default Contructor
        This creates a H.225.0 SignalPDU to notify
        the server that the opened TCP socket is for
        the call with the callidentifier specified
    */
    H46018SignalPDU(const OpalGloballyUniqueID & callIdentifier);
  //@}
};


class H323EndPoint;
class H46018Handler;

class H46018Transport  : public OpalTransportTCP
{
    PCLASSINFO(H46018Transport, OpalTransportTCP);

  public:
    enum PDUType {
      e_raw,
    };

    /**@name Construction */
      //@{
    /**Create a new transport channel.
    */
    H46018Transport(
      H323EndPoint & endpoint        /// H323 End Point object
    );

    ~H46018Transport();
    //@}

    /**@name Functions */
    //@{
    /**Write a protocol data unit from the transport.
        This will write using the transports mechanism for PDU boundaries, for
        example UDP is a single Write() call, while for TCP there is a TPKT
        header that indicates the size of the PDU.
    */
    virtual PBoolean WritePDU(
      const PBYTEArray & pdu  /// PDU to write
    );

    /**Read a protocol data unit from the transport.
        This will read using the transports mechanism for PDU boundaries, for
        example UDP is a single Read() call, while for TCP there is a TPKT
        header that indicates the size of the PDU.
    */
    virtual PBoolean ReadPDU(
      PBYTEArray & pdu  /// PDU to Read
    );
    //@}

    /**@name NAT Functions */
    //@{
    /**HandleH46018SignallingChannelPDU
        Handle the H46018 Signalling channel
      */
    PBoolean HandleH46018SignallingChannelPDU();

    /**HandleH46018SignallingSocket
        Handle the H46018 Signalling socket
      */
    PBoolean HandleH46018SignallingSocket(H323SignalPDU & pdu);

    /**InitialPDU
        Send the initialising PDU for the call with the specified callidentifer
      */
    PBoolean InitialPDU(const OpalGloballyUniqueID & callIdentifier);

    /**isCall
       Do we have a call connected?
      */
    PBoolean isCall() { return isConnected; };

    /**ConnectionLost
       Set the connection as being lost
      */
    void ConnectionLost(PBoolean established);

    /**IsConnectionLost
       Is the connection lost?
      */
    PBoolean IsConnectionLost();
    //@}

    // Overrides
    /**Connect to the remote party.
    */
    virtual PBoolean Connect(const OpalGloballyUniqueID & callIdentifier);

    /**Close the channel.(Don't do anything)
    */
    virtual PBoolean Close();

    virtual PBoolean IsListening() const;

    virtual PBoolean IsOpen () const;

    PBoolean CloseTransport() { return closeTransport; };

  protected:
     PMutex connectionsMutex;
     PMutex WriteMutex;
     PMutex IntMutex;
     PTimeInterval ReadTimeOut;
     PSyncPoint ReadMutex;

     H46018Handler * Feature;

     PBoolean   isConnected;
     PBoolean   remoteShutDown;
     PBoolean   closeTransport;
};


class H323EndPoint;
class PNatMethod_H46019;

class H46018Handler : public PObject  
{
    PCLASSINFO(H46018Handler, PObject);

  public:
    H46018Handler(H323EndPoint * ep);
    ~H46018Handler();

    void Enable();
    PBoolean IsEnabled();

    H323EndPoint * GetEndPoint();

    PBoolean CreateH225Transport(const PASN_OctetString & information);

    void H46024ADirect(bool reply, const PString & token);
        
  protected:    
    H323EndPoint * EP;
    PNatMethod_H46019 * nat;
    PString lastCallIdentifer;

    PMutex m_h46024aMutex;
    bool   m_h46024a;

  private:
    H323TransportAddress m_address;
    OpalGloballyUniqueID m_callId;
    PThread * SocketCreateThread;
    PDECLARE_NOTIFIER(PThread, H46018Handler, SocketThread);
    PBoolean m_h46018inOperation;
};


class PNatMethod_H46019  : public PNatMethod_Null
{
    PCLASSINFO(PNatMethod_H46019,PNatMethod_Null);

  public:
    /**@name Construction */
    //@{
    /** Default Contructor
    */
    PNatMethod_H46019();

    /** Deconstructor
    */
    ~PNatMethod_H46019();
    //@}

    /**@name General Functions */
    //@{
    /**  AttachEndpoint
        Attach endpoint reference
    */
    void AttachHandler(H46018Handler * _handler);

    /**  GetExternalAddress
        Get the external address.
        This function is not used in H46019
    */
    virtual PBoolean GetExternalAddress(
      PIPSocket::Address & externalAddress, ///< External address of router
      const PTimeInterval & maxAge = 1000   ///< Maximum age for caching
    );

    /**  CreateSocketPair
        Create the UDP Socket pair (does nothing)
    */
    virtual PBoolean CreateSocketPair(
      PUDPSocket * & /*socket1*/,            ///< Created DataSocket
      PUDPSocket * & /*socket2*/,            ///< Created ControlSocket
      const PIPSocket::Address & /*binding*/
    ) {  return false; }

    /**  CreateSocketPair
        Create the UDP Socket pair
    */
    virtual PBoolean CreateSocketPair(
      PUDPSocket * & socket1,            ///< Created DataSocket
      PUDPSocket * & socket2,            ///< Created ControlSocket
      const PIPSocket::Address & binding,  
      void * userData
    );

    /**  isAvailable.
        Returns whether the Nat Method is ready and available in
        assisting in NAT Traversal. The principal is function is
        to allow the EP to detect various methods and if a method
        is detected then this method is available for NAT traversal
        The Order of adding to the PNstStrategy determines which method
        is used
    */
    virtual bool IsAvailable(const PIPSocket::Address & address);

    /* Set Available
        This will enable the natMethod to be enabled when opening the
        sockets
    */
    void SetAvailable();

    /** Activate
        Active/DeActivate the method on a call by call basis
     */
    virtual void Activate(bool act)  { active = act; }

    /**  OpenSocket
        Create a single UDP Socket 
    */
    PBoolean OpenSocket(PUDPSocket & socket, PortInfo & portInfo, const PIPSocket::Address & binding) const;

    /**  GetMethodName
        Get the NAT method name 
    */
    static PStringList GetNatMethodName() {  return PStringArray("H46019"); };

    virtual PString GetName() const
    { return GetNatMethodName()[0]; }

  protected:

    /**@name General Functions */
    //@{
    /**  SetConnectionSockets
        This function sets the socket references in the 
        H323Connection to allow the implementer to add
        keep-alive info to the sockets
    */
    void SetConnectionSockets(
      PUDPSocket * data,            ///< Data socket
      PUDPSocket * control,                        ///< control socket 
      H323Connection::SessionInformation * info    ///< session Information
    );
    
    PBoolean available;                    ///< Whether this NAT Method is available for call
    PBoolean active;                    ///< Whether the method is active for call
    H46018Handler * handler;            ///< handler
};

#ifndef _WIN32_WCE
       PPLUGIN_STATIC_LOAD(H46019,PNatMethod);
#endif

class H46019UDPSocket : public PUDPSocket
{
    PCLASSINFO(H46019UDPSocket, PUDPSocket);
  public:
    /**@name Construction/Deconstructor */
    //@{
    /** create a UDP Socket Fully Nat Supported
        ready for H323plus to Call.
    */
    H46019UDPSocket(
      H46018Handler & _handler,
      H323Connection::SessionInformation * info,
      bool _rtpSocket
    );

    /** Deconstructor to reallocate Socket and remove any exiting
        allocated NAT ports, 
    */
    ~H46019UDPSocket();
    //@}

    PBoolean GetLocalAddress(Address & addr, WORD & port);

    /**@name Functions */
    //@{

    /** Allocate (FastStart) keep-alive mechanism but don't Activate
    */
    void Allocate(
      const H323TransportAddress & keepalive, 
      unsigned _payload, 
      unsigned _ttl,
      unsigned _muxId
    );

    /** Activate (FastStart) keep-alive mechanism.
    */
    void Activate();

    /** Activate keep-alive mechanism.
    */
    void Activate(const H323TransportAddress & keepalive,    ///< KeepAlive Address
            unsigned _payload,            ///< RTP Payload type    
            unsigned _ttl,                ///< Time interval for keepalive.
            unsigned _muxId
            );

    /** Get the Ping Payload
      */
    unsigned GetPingPayload();

    /** Set the Ping Payload
      */
    void SetPingPayLoad(unsigned val);

    /** Get Ping TTL
      */
    unsigned GetTTL();

    /** Set Ping TTL
      */
    void SetTTL(unsigned val);

     /**Read a datagram from a remote computer
       @return PTrue if any bytes were sucessfully read.
       */
    virtual PBoolean ReadFrom(
      void * buf,     ///< Data to be written as URGENT TCP data.
      PINDEX len,     ///< Number of bytes pointed to by #buf#.
      Address & addr, ///< Address from which the datagram was received.
      WORD & port     ///< Port from which the datagram was received.
    );

    /**Write a datagram to a remote computer.
       @return PTrue if all the bytes were sucessfully written.
     */
    virtual PBoolean WriteTo(
      const void * buf,   ///< Data to be written as URGENT TCP data.
      PINDEX len,         ///< Number of bytes pointed to by #buf#.
      const Address & addr, ///< Address to which the datagram is sent.
      WORD port           ///< Port to which the datagram is sent.
    );

    enum  probe_state {
        e_notRequired,        ///< Polling has not started
        e_initialising,        ///< We are initialising (local set but remote not)
        e_idle,                ///< Idle (waiting for first packet from remote)
        e_probing,            ///< Probing for direct route
        e_verify_receiver,    ///< verified receive connectivity    
        e_verify_sender,    ///< verified send connectivity
        e_wait,                ///< we are waiting for direct media (to set address)
        e_direct            ///< we are going direct to detected address
    };

    struct probe_packet {
        PUInt16b    Length;        // Length
        PUInt32b    SSRC;        // Time Stamp
        BYTE        name[4];    // Name is limited to 32 (4 Bytes)
        BYTE        cui[20];    // SHA-1 is always 160 (20 Bytes)
    };

    /** Set Alternate Direct Address
      */
    virtual void SetAlternateAddresses(const H323TransportAddress & address, const PString & cui);

     /** Set Alternate Direct Address
      */
    virtual void GetAlternateAddresses(H323TransportAddress & address, PString & cui);

    /** Callback to check if the address received is a permitted alternate
      */
    virtual PBoolean IsAlternateAddress(
        const Address & address,    ///< Detected IP Address.
        WORD port                    ///< Detected Port.
        );
    /** Start sending media/control to alternate address
      */
    void H46024Adirect(bool starter);

    /** Start Probing to alternate address
      */
    void H46024Bdirect(const H323TransportAddress & address);
    //@}

  protected:

 // H.460.19 Keepalives
    void InitialiseKeepAlive();    ///< Start the keepalive
    void SendRTPPing(const PIPSocket::Address & ip, const WORD & port);
    void SendRTCPPing();
    PBoolean SendRTCPFrame(RTP_ControlFrame & report, const PIPSocket::Address & ip, WORD port);
    PMutex PingMutex;

    // H46024 Annex A support
    PBoolean ReceivedProbePacket(const RTP_ControlFrame & frame, bool & probe, bool & success);
    void BuildProbe(RTP_ControlFrame & report, bool reply);
    void StartProbe();
    void ProbeReceived(bool probe, const PIPSocket::Address & addr, WORD & port);
    void SetProbeState(probe_state newstate);
    int GetProbeState() const;
    probe_state m_state;
    PMutex probeMutex;

    /**Write a datagram to a remote computer.
       @return PTrue if all the bytes were sucessfully written.
     */
    virtual PBoolean Internal_WriteTo(
      const void * buf,   ///< Data to be written as URGENT TCP data.
      PINDEX len,         ///< Number of bytes pointed to by #buf#.
      const Address & addr, ///< Address to which the datagram is sent.
      WORD port           ///< Port to which the datagram is sent.
    );

  private:
    H46018Handler & m_Handler;
    unsigned m_Session;                        ///< Current Session ie 1-Audio 2-video
    PString m_Token;                        ///< Current Connection Token
    OpalGloballyUniqueID m_CallId;            ///< CallIdentifier
    PString m_CUI;                            ///< Local CUI (for H.460.24 Annex A)

 // H.460.19 Keepalives
    PIPSocket::Address keepip;                ///< KeepAlive Address
    WORD keepport;                            ///< KeepAlive Port
    unsigned keeppayload;                    ///< KeepAlive RTP payload
    unsigned keepTTL;                        ///< KeepAlive TTL
    unsigned muxId;
    WORD keepseqno;                            ///< KeepAlive sequence number
    PTime * keepStartTime;                    ///< KeepAlive start time for TimeStamp.

    PDECLARE_NOTIFIER(PTimer, H46019UDPSocket, Ping);    ///< Timer to notify to poll for External IP
    PTimer    Keep;                                        ///< Polling Timer

    // H46024 Annex A support
    PString m_CUIrem;                                        ///< Remote CUI
    PIPSocket::Address m_locAddr;  WORD m_locPort;            ///< local Address (address used when starting socket)
    PIPSocket::Address m_remAddr;  WORD m_remPort;            ///< Remote Address (address used when starting socket)
    PIPSocket::Address m_detAddr;  WORD m_detPort;            ///< detected remote Address (as detected from actual packets)
    PIPSocket::Address m_pendAddr;  WORD m_pendPort;        ///< detected pending RTCP Probe Address (as detected from actual packets)
    PDECLARE_NOTIFIER(PTimer, H46019UDPSocket, Probe);        ///< Thread to probe for direct connection
    PTimer m_Probe;                                            ///< Probe Timer
    PINDEX m_probes;                                        ///< Probe count
    DWORD SSRC;                                                ///< Random number
    PIPSocket::Address m_altAddr;  WORD m_altPort;            ///< supplied remote Address (as supplied in Generic Information)
    // H46024 Annex B support
    PBoolean    m_h46024b;

    bool rtpSocket;
};


#endif // H46018_NAT
