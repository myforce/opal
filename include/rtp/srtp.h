/*
 * srtp.h
 *
 * SRTP protocol handler
 *
 * OPAL Library
 *
 * Copyright (C) 2006 Post Increment
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
 * The Original Code is OPAL Library.
 *
 * The Initial Developer of the Original Code is Post Increment
 *     Portions of this code were written with the assistance of funding from
 *     US Joint Forces Command Joint Concept Development & Experimentation (J9)
 *     http://www.jfcom.mil/about/abt_j9.htm
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#ifndef __OPAL_SRTP_H
#define __OPAL_SRTP_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#ifndef _PTLIB_H
#include <ptlib.h>
#endif

#include <opal/buildopts.h>
#include <rtp/rtp.h>
#include <opal/rtpconn.h>

#if OPAL_SRTP

namespace PWLibStupidLinkerHacks {
  extern int libSRTPLoader;
};

////////////////////////////////////////////////////////////////////
//
//  this class holds the parameters required for an SRTP session
//
//  Crypto modes are identified by key strings that are contained in PFactory<OpalSRTPParms>
//  The following strings should be implemented:
//
//     AES_CM_128_HMAC_SHA1_80,
//     AES_CM_128_HMAC_SHA1_32,
//     AES_CM_128_NULL_AUTH,   
//     NULL_CIPHER_HMAC_SHA1_80
//     STRONGHOLD
//

class OpalSRTPSecurityMode : public OpalSecurityMode
{
  PCLASSINFO(OpalSRTPSecurityMode, OpalSecurityMode);
  public:
    struct KeySalt {
      KeySalt()                                                       { }
      KeySalt(const PBYTEArray & data)           : key(data)          { }
      KeySalt(const BYTE * data, PINDEX dataLen) : key(data, dataLen) { }
      PBYTEArray key;
      PBYTEArray salt;
    };
    virtual PBoolean SetOutgoingKey(const KeySalt & key) = 0;
    virtual PBoolean GetOutgoingKey(KeySalt & key) const = 0;
    virtual PBoolean SetOutgoingSSRC(DWORD ssrc) = 0;
    virtual PBoolean GetOutgoingSSRC(DWORD & ssrc) const = 0;

    virtual PBoolean SetIncomingKey(const KeySalt & key) = 0;
    virtual PBoolean GetIncomingKey(KeySalt & key) const = 0;
    virtual PBoolean SetIncomingSSRC(DWORD ssrc) = 0;
    virtual PBoolean GetIncomingSSRC(DWORD & ssrc) const = 0;
};

////////////////////////////////////////////////////////////////////
//
//  this class implements SRTP over UDP
//

class OpalSRTP_UDP : public SecureRTP_UDP
{
  PCLASSINFO(OpalSRTP_UDP, SecureRTP_UDP);
  public:
    OpalSRTP_UDP(
      const PString & encoding,          ///<  identifies initial RTP encoding (RTP/AVP, UDPTL etc)
#if OPAL_RTP_AGGREGATE
      PHandleAggregator * _aggregator,   ///< handle aggregator
#endif
      unsigned id,                       ///<  Session ID for RTP channel
      PBoolean remoteIsNAT               ///<  PTrue is remote is behind NAT
    );

    virtual SendReceiveStatus OnSendData   (RTP_DataFrame & frame) = 0;
    virtual SendReceiveStatus OnReceiveData(RTP_DataFrame & frame) = 0;
    virtual SendReceiveStatus OnSendControl(RTP_ControlFrame & frame, PINDEX & len) = 0;
    virtual SendReceiveStatus OnReceiveControl(RTP_ControlFrame & frame) = 0;
};


////////////////////////////////////////////////////////////////////
//
//  this class implements SRTP using libSRTP
//

class LibSRTP_UDP : public OpalSRTP_UDP
{
  PCLASSINFO(LibSRTP_UDP, OpalSRTP_UDP);
  public:
    LibSRTP_UDP(
      const PString & encoding,          ///<  identifies initial RTP encoding (RTP/AVP, UDPTL etc)
#if OPAL_RTP_AGGREGATE
      PHandleAggregator * _aggregator,   ///< handle aggregator
#endif
      unsigned int id,                   ///<  Session ID for RTP channel
      PBoolean remoteIsNAT               ///<  PTrue is remote is behind NAT
    );

    ~LibSRTP_UDP();

    PBoolean Open(
      PIPSocket::Address localAddress,  ///<  Local interface to bind to
      WORD portBase,                    ///<  Base of ports to search
      WORD portMax,                     ///<  end of ports to search (inclusive)
      BYTE ipTypeOfService,             ///<  Type of Service byte
      PNatMethod * natMethod = NULL,    ///<  NAT traversal method to use createing sockets
      RTP_QOS * rtpqos = NULL           ///<  QOS spec (or NULL if no QoS)
    );

    virtual SendReceiveStatus OnSendData   (RTP_DataFrame & frame);
    virtual SendReceiveStatus OnReceiveData(RTP_DataFrame & frame);
    virtual SendReceiveStatus OnSendControl(RTP_ControlFrame & frame, PINDEX & len);
    virtual SendReceiveStatus OnReceiveControl(RTP_ControlFrame & frame);
};


#endif // OPAL_SRTP

#endif // __OPAL_SRTP_H
