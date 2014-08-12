/*
 * srtp_session.cxx
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

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "srtp_session.h"
#endif

#include <opal_config.h>

#if OPAL_SRTP

#include <rtp/srtp_session.h>
#include <h323/h323caps.h>
#include <ptclib/cypher.h>


#define PTraceModule() "SRTP"


/////////////////////////////////////////////////////////////////////////////////////
//
//  SRTP implementation using Cisco libSRTP
//  See http://srtp.sourceforge.net/srtp.html
//
// Will use OS installed version for Linux, for DevStudio uses built in version
//

#if OPAL_SRTP==2
  #define uint32_t uint32_t
  #pragma warning(disable:4505)
  #include <srtp.h>
  #pragma comment(lib, "ws2_32.lib") // As libsrtp uses htonl etc
#elif HAS_SRTP_SRTP_H
  #include <srtp/srtp.h>
#else
  #include <srtp.h>
#endif


///////////////////////////////////////////////////////

#if PTRACING
static bool CheckError(err_status_t err, const char * fn, const char * file, int line, RTP_SyncSourceId ssrc = 0, RTP_SequenceNumber sn = 0)
{
  if (err == err_status_ok)
    return true;

  ostream & trace = PTrace::Begin(2, file, line, NULL, PTraceModule());
  trace << "Library error " << err << " from " << fn << "() - ";
  switch (err) {
    case err_status_fail :
      trace << "unspecified failure";
      break;
    case err_status_bad_param :
      trace << "unsupported parameter";
      break;
    case err_status_alloc_fail :
      trace << "couldn't allocate memory";
      break;
    case err_status_dealloc_fail :
      trace << "couldn't deallocate properly";
      break;
    case err_status_init_fail :
      trace << "couldn't initialize";
      break;
    case err_status_terminus :
      trace << "can't process as much data as requested";
      break;
    case err_status_auth_fail :
      trace << "authentication failure";
      break;
    case err_status_cipher_fail :
      trace << "cipher failure";
      break;
    case err_status_replay_fail :
      trace << "replay check failed (bad index)";
      break;
    case err_status_replay_old :
      trace << "replay check failed (index too old)";
      break;
    case err_status_algo_fail :
      trace << "algorithm failed test routine";
      break;
    case err_status_no_such_op :
      trace << "unsupported operation";
      break;
    case err_status_no_ctx :
      trace << "no appropriate context found";
      break;
    case err_status_cant_check :
      trace << "unable to perform desired validation";
      break;
    case err_status_key_expired :
      trace << "can't use key any more";
      break;
    case err_status_socket_err :
      trace << "error in use of socket";
      break;
    case err_status_signal_err :
      trace << "error in use POSIX signals";
      break;
    case err_status_nonce_bad :
      trace << "nonce check failed";
      break;
    case err_status_read_fail :
      trace << "couldn't read data";
      break;
    case err_status_write_fail :
      trace << "couldn't write data";
      break;
    case err_status_parse_err :
      trace << "error pasring data";
      break;
    case err_status_encode_err :
      trace << "error encoding data";
      break;
    case err_status_semaphore_err :
      trace << "error while using semaphores";
      break;
    case err_status_pfkey_err :
      trace << "error while using pfkey";
      break;
    default :
      trace << "unknown error (" << err << ')';
  }
  if (ssrc != 0)
    trace << " - SSRC=" << RTP_TRACE_SRC(ssrc);
  if (sn != 0)
    trace << " SN=" << sn;
  trace << PTrace::End;
  return false;
}

#define CHECK_ERROR(fn, param, ...) CheckError(fn param, #fn, __FILE__, __LINE__, ##__VA_ARGS__)
#else //PTRACING
#define CHECK_ERROR(fn, param, ...) ((fn param) == err_status_ok)
#endif //PTRACING

extern "C" {
  err_reporting_level_t err_level = err_level_none;

  err_status_t err_reporting_init(char *)
  {
    return err_status_ok;
  }

  void err_report(int PTRACE_PARAM(priority), char * PTRACE_PARAM(format), ...)
  {
#if PTRACING
    va_list args;
    va_start(args, format);

    unsigned level;
    switch (priority) {
      case err_level_emergency:
      case err_level_alert:
      case err_level_critical:
         level = 0;
         break;
      case err_level_error:
         level = 1;
         break;
      case err_level_warning:
         level = 2;
         break;
      case err_level_notice:
         level = 3;
         break;
      case err_level_info:
         level = 4;
         break;
      case err_level_debug:
      case err_level_none:
      default:
         level = 5;
         break;
    }

    PTRACE(level, "libsrtp: " << pvsprintf(format, args));
#endif //PTRACING
  }
};

///////////////////////////////////////////////////////

class PSRTPInitialiser : public PProcessStartup
{
  PCLASSINFO(PSRTPInitialiser, PProcessStartup)
  public:
    virtual void OnStartup()
    {
      CHECK_ERROR(srtp_init,());
    }
};

PFACTORY_CREATE_SINGLETON(PProcessStartupFactory, PSRTPInitialiser);


///////////////////////////////////////////////////////

const PCaselessString & OpalSRTPSession::RTP_SAVP () { static const PConstCaselessString s("RTP/SAVP" ); return s; }
const PCaselessString & OpalSRTPSession::RTP_SAVPF() { static const PConstCaselessString s("RTP/SAVPF"); return s; }

PFACTORY_CREATE(OpalMediaSessionFactory, OpalSRTPSession, OpalSRTPSession::RTP_SAVP());
PFACTORY_SYNONYM(OpalMediaSessionFactory, OpalSRTPSession, SAVPF, OpalSRTPSession::RTP_SAVPF());

static PConstCaselessString AES_CM_128_HMAC_SHA1_80("AES_CM_128_HMAC_SHA1_80");

class OpalSRTPCryptoSuite_AES_CM_128_HMAC_SHA1_80 : public OpalSRTPCryptoSuite
{
    PCLASSINFO(OpalSRTPCryptoSuite_AES_CM_128_HMAC_SHA1_80, OpalSRTPCryptoSuite);
  public:
    virtual const PCaselessString & GetFactoryName() const { return AES_CM_128_HMAC_SHA1_80; }
    virtual const char * GetDescription() const { return "SRTP: AES-128 & SHA1-80"; }
#if OPAL_H235_6 || OPAL_H235_8
    virtual const char * GetOID() const { return "0.0.8.235.0.4.91"; }
#endif
    virtual PINDEX GetCipherKeyBits() const { return 128; }
    virtual PINDEX GetAuthSaltBits() const { return 112; }

    virtual void SetCryptoPolicy(struct crypto_policy_t & policy) const { crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy); }
};

PFACTORY_CREATE(OpalMediaCryptoSuiteFactory, OpalSRTPCryptoSuite_AES_CM_128_HMAC_SHA1_80, AES_CM_128_HMAC_SHA1_80, true);


static PConstCaselessString AES_CM_128_HMAC_SHA1_32("AES_CM_128_HMAC_SHA1_32");

class OpalSRTPCryptoSuite_AES_CM_128_HMAC_SHA1_32 : public OpalSRTPCryptoSuite
{
    PCLASSINFO(OpalSRTPCryptoSuite_AES_CM_128_HMAC_SHA1_32, OpalSRTPCryptoSuite);
  public:
    virtual const PCaselessString & GetFactoryName() const { return AES_CM_128_HMAC_SHA1_32; }
    virtual const char * GetDescription() const { return "SRTP: AES-128 & SHA1-32"; }
#if OPAL_H235_6 || OPAL_H235_8
    virtual const char * GetOID() const { return "0.0.8.235.0.4.92"; }
#endif
    virtual PINDEX GetCipherKeyBits() const { return 128; }
    virtual PINDEX GetAuthSaltBits() const { return 32; }

    virtual void SetCryptoPolicy(struct crypto_policy_t & policy) const { crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy); }
};

PFACTORY_CREATE(OpalMediaCryptoSuiteFactory, OpalSRTPCryptoSuite_AES_CM_128_HMAC_SHA1_32, AES_CM_128_HMAC_SHA1_32, true);



///////////////////////////////////////////////////////

#if OPAL_H235_8
H235SecurityCapability * OpalSRTPCryptoSuite::CreateCapability(const H323Capability & mediaCapability) const
{
  return new H235SecurityGenericCapability(mediaCapability);
}
#endif

bool OpalSRTPCryptoSuite::Supports(const PCaselessString & proto) const
{
  return proto == "sip" || proto == "h323";
}


bool OpalSRTPCryptoSuite::ChangeSessionType(PCaselessString & mediaSession) const
{
  if (mediaSession == OpalRTPSession::RTP_AVP()) {
    mediaSession = OpalSRTPSession::RTP_SAVP();
    return true;
  }

  if (mediaSession == OpalRTPSession::RTP_AVPF()) {
    mediaSession = OpalSRTPSession::RTP_SAVPF();
    return true;
  }

  return false;
}


OpalMediaCryptoKeyInfo * OpalSRTPCryptoSuite::CreateKeyInfo() const
{
  return new OpalSRTPKeyInfo(*this);
}


///////////////////////////////////////////////////////////////////////

OpalSRTPKeyInfo::OpalSRTPKeyInfo(const OpalSRTPCryptoSuite & cryptoSuite)
  : OpalMediaCryptoKeyInfo(cryptoSuite)
  , m_cryptoSuite(cryptoSuite)
{
}


PObject * OpalSRTPKeyInfo::Clone() const
{
  OpalSRTPKeyInfo * clone = new OpalSRTPKeyInfo(*this);
  clone->m_key.MakeUnique();
  clone->m_salt.MakeUnique();
  return clone;
}


bool OpalSRTPKeyInfo::IsValid() const
{
  return m_key.GetSize() == (m_cryptoSuite.GetCipherKeyBits()+7)/8 &&
         m_salt.GetSize() == (m_cryptoSuite.GetAuthSaltBits()+7)/8;
}


bool OpalSRTPKeyInfo::FromString(const PString & str)
{
  PBYTEArray key_salt;
  if (!PBase64::Decode(str, key_salt)) {
    PTRACE2(2, &m_cryptoSuite, "Illegal key/salt \"" << str << '"');
    return false;
  }

  PINDEX keyBytes = (m_cryptoSuite.GetCipherKeyBits()+7)/8;
  PINDEX saltBytes = (m_cryptoSuite.GetAuthSaltBits()+7)/8;

  if (key_salt.GetSize() < keyBytes+saltBytes) {
    PTRACE2(2, &m_cryptoSuite, "Incorrect combined key/salt size (" << key_salt.GetSize()
            << " bytes) for " << m_cryptoSuite.GetDescription());
    return false;
  }

  SetCipherKey(PBYTEArray(key_salt, keyBytes));
  SetAuthSalt(PBYTEArray(key_salt+keyBytes, key_salt.GetSize()-keyBytes));

  return true;
}


PString OpalSRTPKeyInfo::ToString() const
{
  PBYTEArray key_salt(m_key.GetSize()+m_salt.GetSize());
  memcpy(key_salt.GetPointer(), m_key, m_key.GetSize());
  memcpy(key_salt.GetPointer()+m_key.GetSize(), m_salt, m_salt.GetSize());
  return PBase64::Encode(key_salt, "");
}


void OpalSRTPKeyInfo::Randomise()
{
  m_key.SetSize((m_cryptoSuite.GetCipherKeyBits()+7)/8);
  rand_source_get_octet_string(m_key.GetPointer(), m_key.GetSize());

  m_salt.SetSize((m_cryptoSuite.GetAuthSaltBits()+7)/8);
  rand_source_get_octet_string(m_salt.GetPointer(), m_salt.GetSize());
}


bool OpalSRTPKeyInfo::SetCipherKey(const PBYTEArray & key)
{
  if (key.GetSize() < (m_cryptoSuite.GetCipherKeyBits()+7)/8) {
    PTRACE2(2, &m_cryptoSuite, "Incorrect key size (" << key.GetSize() << " bytes) for " << m_cryptoSuite.GetDescription());
    return false;
  }

  m_key = key;
  return true;
}


bool OpalSRTPKeyInfo::SetAuthSalt(const PBYTEArray & salt)
{
  if (salt.GetSize() < (m_cryptoSuite.GetAuthSaltBits()+7)/8) {
    PTRACE2(2, &m_cryptoSuite, "Incorrect salt size (" << salt.GetSize() << " bytes) for " << m_cryptoSuite.GetDescription());
    return false;
  }

  m_salt = salt;
  return true;
}


PBYTEArray OpalSRTPKeyInfo::GetCipherKey() const
{
  return m_key;
}


PBYTEArray OpalSRTPKeyInfo::GetAuthSalt() const
{
  return m_salt;
}


///////////////////////////////////////////////////////////////////////////////

OpalSRTPSession::OpalSRTPSession(const Init & init)
  : OpalRTPSession(init)
{
  CHECK_ERROR(srtp_create, (&m_context, NULL));

  for (int i = 0; i < 2; ++i) {
    m_keyInfo[i] = NULL;
#if PTRACING
    for (int j = 0; j < 2; ++j) {
      m_traceLevel[i][j] = 3;
      m_traceUnsecuredCount[i][j] = 0;
    }
#endif
  }
}


OpalSRTPSession::~OpalSRTPSession()
{
  Close();

  if (m_context != NULL)
    CHECK_ERROR(srtp_dealloc,(m_context));
}


bool OpalSRTPSession::Close()
{
  for (int i = 0; i < 2; ++i) {
    delete m_keyInfo[i];
    m_keyInfo[i] = NULL;
  }

  return OpalRTPSession::Close();
}


OpalMediaCryptoKeyList & OpalSRTPSession::GetOfferedCryptoKeys()
{
  if (m_offeredCryptokeys.IsEmpty() && m_keyInfo[e_Sender] != NULL)
    m_offeredCryptokeys.Append(m_keyInfo[e_Sender]->CloneAs<OpalMediaCryptoKeyInfo>());

  return OpalRTPSession::GetOfferedCryptoKeys();
}


bool OpalSRTPSession::ApplyCryptoKey(OpalMediaCryptoKeyList & keys, Direction dir)
{
  for (OpalMediaCryptoKeyList::iterator it = keys.begin(); it != keys.end(); ++it) {
    OpalSRTPKeyInfo * keyInfo = dynamic_cast<OpalSRTPKeyInfo*>(&*it);
    if (keyInfo == NULL)
      PTRACE(2, *this << "unsuitable crypto suite " << it->GetCryptoSuite().GetDescription());
    else if (ApplyKeyToSRTP(*keyInfo, dir)) {
      keys.Select(it);
      return true;
    }
  }
  return false;
}


bool OpalSRTPSession::ApplyKeyToSRTP(OpalSRTPKeyInfo & keyInfo, Direction dir)
{
  // This is all a bit vague in docs for libSRTP. Had to look into source to figure it out.
  BYTE tmp_key_salt[32];
  memcpy(tmp_key_salt, keyInfo.GetCipherKey(), std::min((PINDEX)16, keyInfo.GetCipherKey().GetSize()));
  memcpy(&tmp_key_salt[16], keyInfo.GetAuthSalt(), std::min((PINDEX)14, keyInfo.GetAuthSalt().GetSize()));

  if (m_keyInfo[dir] != NULL) {
    if (memcmp(tmp_key_salt, m_keyInfo[dir]->m_key_salt, 32) == 0) {
      PTRACE(3, *this << "crypto key for " << dir << " already set.");
      return true;
    }
    PTRACE(3, *this << "changing crypto keys for " << dir);
    delete m_keyInfo[dir];
  }
  else {
    PTRACE(3, *this << "setting crypto keys for " << dir);
  }

  m_keyInfo[dir] = new OpalSRTPKeyInfo(keyInfo);
  memcpy(m_keyInfo[dir]->m_key_salt, tmp_key_salt, 32);

  for (SyncSourceMap::iterator it = m_SSRC.begin(); it != m_SSRC.end(); ++it) {
    if (it->second->m_direction == dir && !AddStreamToSRTP(it->first, dir))
      return false;
  }

  return true;
}


bool OpalSRTPSession::IsCryptoSecured(Direction dir) const
{
  return m_keyInfo[dir] != NULL;
}


RTP_SyncSourceId OpalSRTPSession::AddSyncSource(RTP_SyncSourceId ssrc, Direction dir, const char * cname)
{
  if ((ssrc = OpalRTPSession::AddSyncSource(ssrc, dir, cname)) == 0)
    return 0;

  if (m_keyInfo[dir] == NULL) {
    PTRACE(2, *this << "could not add stream for SSRC=" << RTP_TRACE_SRC(ssrc) << ", no " << dir << " key (yet)");
    return ssrc;
  }

  if (AddStreamToSRTP(ssrc, dir))
    return ssrc;

  m_SSRC.erase(m_SSRC.find(ssrc));
  return 0;
}


bool OpalSRTPSession::AddStreamToSRTP(RTP_SyncSourceId ssrc, Direction dir)
{
  // Get policy, create blank one if needed
  srtp_policy_t policy;
  memset(&policy, 0, sizeof(policy));

  policy.ssrc.type = ssrc_specific;
  policy.ssrc.value = ssrc;

  const OpalSRTPCryptoSuite & cryptoSuite = m_keyInfo[dir]->GetCryptoSuite();
  cryptoSuite.SetCryptoPolicy(policy.rtp);
  cryptoSuite.SetCryptoPolicy(policy.rtcp);

  policy.key = m_keyInfo[dir]->m_key_salt;

  if (!CHECK_ERROR(srtp_add_stream, (m_context, &policy), ssrc))
    return false;

  PTRACE(4, *this << "added " << dir << " stream for SSRC=" << RTP_TRACE_SRC(ssrc));
  return true;
}


OpalRTPSession::SendReceiveStatus OpalSRTPSession::OnSendData(RTP_DataFrame & frame, RewriteMode rewrite)
{
  SendReceiveStatus status = OpalRTPSession::OnSendData(frame, rewrite);
  if (status != e_ProcessPacket)
    return status;

  if (rewrite == e_RewriteNothing)
    return e_ProcessPacket;

  if (!IsCryptoSecured(e_Sender)) {
    PTRACE_IF(3, (m_traceUnsecuredCount[e_Data][e_Sender]++ % 100) == 0,
              *this << "keys not set, cannot protect data: " << m_traceUnsecuredCount[e_Data][e_Sender]);
    return e_IgnorePacket;
  }

  frame.MakeUnique();

  int len = frame.GetPacketSize();
  frame.SetMinSize(len + SRTP_MAX_TRAILER_LEN);
  if (!CHECK_ERROR(srtp_protect,(m_context, frame.GetPointer(), &len), frame.GetSyncSource(), frame.GetSequenceNumber()))
    return e_AbortTransport;

  PTRACE(m_traceLevel[e_Data][e_Sender], *this << "protected RTP packet: "
         << frame.GetPacketSize() << "->" << len << " SSRC=" << frame.GetSyncSource());
#if PTRACING
  m_traceLevel[e_Data][e_Sender] = 6;
#endif

  frame.SetPayloadSize(len - frame.GetHeaderSize());
  return e_ProcessPacket;
}


OpalRTPSession::SendReceiveStatus OpalSRTPSession::OnSendControl(RTP_ControlFrame & frame)
{
  SendReceiveStatus status = OpalRTPSession::OnSendControl(frame);
  if (status != e_ProcessPacket)
    return status;

  if (!IsCryptoSecured(e_Sender)) {
    PTRACE_IF(3, (m_traceUnsecuredCount[e_Control][e_Sender]++ % 100) == 0,
              *this << "keys not set, cannot protect control: " << m_traceUnsecuredCount[e_Control][e_Sender]);
    return OpalRTPSession::e_IgnorePacket;
  }

  frame.MakeUnique();

  int len = frame.GetPacketSize();
  frame.SetMinSize(len + SRTP_MAX_TRAILER_LEN);

  if (!CHECK_ERROR(srtp_protect_rtcp,(m_context, frame.GetPointer(), &len), frame.GetSenderSyncSource()))
    return OpalRTPSession::e_AbortTransport;

  PTRACE(m_traceLevel[e_Control][e_Sender], *this << "protected RTCP packet: "
         << frame.GetPacketSize() << "->" << len << " SSRC=" << frame.GetSenderSyncSource());
#if PTRACING
  m_traceLevel[e_Control][e_Sender] = 6;
#endif

  frame.SetPacketSize(len);
  return OpalRTPSession::e_ProcessPacket;
}


OpalRTPSession::SendReceiveStatus OpalSRTPSession::OnReceiveData(RTP_DataFrame & frame)
{
  if (!IsCryptoSecured(e_Receiver)) {
    PTRACE_IF(3, (m_traceUnsecuredCount[e_Data][e_Receiver]++ % 100) == 0,
              *this << "keys not set, cannot protect control: " << m_traceUnsecuredCount[e_Data][e_Receiver]);
    return OpalRTPSession::e_IgnorePacket;
  }

  frame.MakeUnique();

  int len = frame.GetPacketSize();
  if (!CHECK_ERROR(srtp_unprotect, (m_context, frame.GetPointer(), &len), frame.GetSyncSource(), frame.GetSequenceNumber()))
    return OpalRTPSession::e_AbortTransport;

  PTRACE(m_traceLevel[e_Data][e_Receiver], *this << "unprotected RTP packet: "
         << frame.GetPacketSize() << "->" << len << " SSRC=" << frame.GetSyncSource());
#if PTRACING
  m_traceLevel[e_Data][e_Receiver] = 6;
#endif

  frame.SetPayloadSize(len - frame.GetHeaderSize());
  return OpalRTPSession::OnReceiveData(frame);
}


OpalRTPSession::SendReceiveStatus OpalSRTPSession::OnReceiveControl(RTP_ControlFrame & frame)
{
  if (!IsCryptoSecured(e_Receiver)) {
    PTRACE_IF(3, (m_traceUnsecuredCount[e_Control][e_Receiver]++ % 100) == 0,
              *this << "keys not set, cannot protect control: " << m_traceUnsecuredCount[e_Control][e_Receiver]);
    return OpalRTPSession::e_IgnorePacket;
  }

  frame.MakeUnique();

  int len = frame.GetSize();
  if (!CHECK_ERROR(srtp_unprotect_rtcp,(m_context, frame.GetPointer(), &len), frame.GetSenderSyncSource()))
    return OpalRTPSession::e_AbortTransport;

  PTRACE(m_traceLevel[e_Control][e_Receiver], *this << "unprotected RTCP packet: "
         << frame.GetPacketSize() << "->" << len << " SSRC=" << frame.GetSenderSyncSource());
#if PTRACING
  m_traceLevel[e_Control][e_Receiver] = 6;
#endif

  frame.SetPacketSize(len);

  return OpalRTPSession::OnReceiveControl(frame);
}
#endif // OPAL_SRTP
