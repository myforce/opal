/*
 * srtp_session.h
 *
 * SRTP protocol session handler
 *
 * OPAL Library
 *
 * Copyright (C) 2012 Vox Lucida Pty. Ltd.
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
 * The Initial Developer of the Original Code is Vox Lucida
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#ifndef OPAL_RTP_SRTP_SESSION_H
#define OPAL_RTP_SRTP_SESSION_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#ifndef _PTLIB_H
#include <ptlib.h>
#endif

#include <opal_config.h>

#include <rtp/rtp.h>
#include <rtp/rtpconn.h>

#if OPAL_SRTP

class OpalSRTPCryptoSuite;
struct srtp_ctx_t;


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

class OpalSRTPKeyInfo : public OpalMediaCryptoKeyInfo
{
    PCLASSINFO(OpalSRTPKeyInfo, OpalMediaCryptoKeyInfo);
  public:
    OpalSRTPKeyInfo(const OpalSRTPCryptoSuite & cryptoSuite);

    PObject * Clone() const;

    virtual bool IsValid() const;
    virtual void Randomise();
    virtual bool FromString(const PString & str);
    virtual PString ToString() const;
    virtual bool SetCipherKey(const PBYTEArray & key);
    virtual bool SetAuthSalt(const PBYTEArray & key);
    virtual PBYTEArray GetCipherKey() const;
    virtual PBYTEArray GetAuthSalt() const;

    const OpalSRTPCryptoSuite & GetCryptoSuite() const { return m_cryptoSuite; }

  protected:
    const OpalSRTPCryptoSuite & m_cryptoSuite;
    PBYTEArray m_key;
    PBYTEArray m_salt;
    BYTE       m_key_salt[32]; // libsrtp internal

  friend class OpalSRTPSession;
};


class OpalSRTPCryptoSuite : public OpalMediaCryptoSuite
{
    PCLASSINFO(OpalSRTPCryptoSuite, OpalMediaCryptoSuite);
  protected:
    OpalSRTPCryptoSuite() { }

  public:
#if OPAL_H235_8
    virtual H235SecurityCapability * CreateCapability(const H323Capability & mediaCapability) const;
#endif
    virtual bool Supports(const PCaselessString & proto) const;
    virtual bool ChangeSessionType(PCaselessString & mediaSession, KeyExchangeModes modes) const;

    virtual OpalMediaCryptoKeyInfo * CreateKeyInfo() const;

    virtual void SetCryptoPolicy(struct crypto_policy_t & policy) const = 0;
};


/** This class implements SRTP using libSRTP
  */
class OpalSRTPSession : public OpalRTPSession
{
  PCLASSINFO(OpalSRTPSession, OpalRTPSession);
  public:
    static const PCaselessString & RTP_SAVP();
    static const PCaselessString & RTP_SAVPF();

    OpalSRTPSession(const Init & init);
    ~OpalSRTPSession();

    virtual const PCaselessString & GetSessionType() const { return RTP_SAVP(); }
    virtual OpalMediaCryptoKeyList & GetOfferedCryptoKeys();
    virtual bool ApplyCryptoKey(OpalMediaCryptoKeyList & keys, Direction dir);
    virtual bool IsCryptoSecured(Direction dir) const;

    virtual RTP_SyncSourceId AddSyncSource(RTP_SyncSourceId id, Direction dir, const char * cname = NULL);

    virtual SendReceiveStatus OnSendData(RTP_DataFrame & frame, RewriteMode rewrite);
    virtual SendReceiveStatus OnSendControl(RTP_ControlFrame & frame);
    virtual SendReceiveStatus OnReceiveData(RTP_DataFrame & frame);
    virtual SendReceiveStatus OnReceiveControl(RTP_ControlFrame & frame);

  protected:
    virtual bool ResequenceOutOfOrderPackets(SyncSource & ssrc) const;
    virtual bool ApplyKeyToSRTP(OpalSRTPKeyInfo & keyInfo, Direction dir);
    virtual bool AddStreamToSRTP(RTP_SyncSourceId ssrc, Direction dir);

    struct srtp_ctx_t * m_context;
    OpalSRTPKeyInfo   * m_keyInfo[2]; // rx & tx
    unsigned            m_consecutiveErrors;

#if PTRACING
    map< uint64_t, PTrace::Throttle<3> > m_throttle;
    __inline PTrace::Throttle<3> & GetThrottle(Direction dir, Channel chan, RTP_SyncSourceId ssrc) { return m_throttle[dir|(chan<<2)|((uint64_t)ssrc<<4)]; }
#endif
};


#endif // OPAL_SRTP

#endif // OPAL_RTP_SRTP_SESSION_H
