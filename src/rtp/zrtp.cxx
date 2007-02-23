/*
 * zrtp.cxx
 *
 * ZRTP protocol handler
 *
 * OPAL Library
 *
 * Copyright (C) 2007 Post Increment
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
 *
 * $Log: zrtp.cxx,v $
 * Revision 1.2003  2007/02/23 05:24:14  csoutheren
 * Fixed problem linking with ZRTP on Windows
 *
 * Revision 2.1  2007/02/12 02:44:27  csoutheren
 * Start of support for ZRTP
 *
 * Revision 2.1  2007/02/10 07:08:41  craigs
 * Start of support for ZRTP
 *
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "zrtp.h"
#endif

#include <ptlib//pprocess.h>
#include <opal/buildopts.h>

#if defined(OPAL_ZRTP)

#include <rtp/zrtp.h>

extern "C" {
#include <ZRTP/zrtp.h>
#include <ZRTP/zrtp_log.h>
};

namespace PWLibStupidLinkerHacks {
  int libZRTPLoader;
};

#if _WIN32
#pragma comment(lib, LIBZRTP_LIBRARY)
#endif

class ZRTPSecurityMode_Base : public OpalZRTPSecurityMode
{
  PCLASSINFO(ZRTPSecurityMode_Base, OpalZRTPSecurityMode);
  public:
    ZRTPSecurityMode_Base();

    RTP_UDP * CreateRTPSession(PHandleAggregator * _aggregator,   ///< handle aggregator
                                            unsigned id,          ///<  Session ID for RTP channel
                                            BOOL remoteIsNAT      ///<  TRUE is remote is behind NAT
    );

    BOOL Open();

  protected:
    void Init();

    zrtp_zid_t zid;
    zrtp_conn_ctx_t zrtpSession;

  private:
    static PMutex initMutex;
    static zrtp_global_ctx * zrtpContext;
};

PMutex ZRTPSecurityMode_Base::initMutex;
zrtp_global_ctx_t * ZRTPSecurityMode_Base::zrtpContext = NULL;

#define DECLARE_LIBZRTP_CRYPTO_ALG(name) \
class ZRTPSecurityMode_##name : public OpalZRTPSecurityMode_Base \
{ \
  public: \
    ZRTPSecurityMode_##name() \
    { \
      Init(); \
    } \
}; \
static PFactory<OpalSecurityMode>::Worker<ZRTPSecurityMode_##name> factoryZRTPSecurityMode_##name("ZRTP|" #name); \

//DECLARE_LIBZRTP_CRYPTO_ALG(AES_CM_128_HMAC_SHA1_80);

class ZRTPSecurityMode_Test : public ZRTPSecurityMode_Base 
{ 
  public: 
    ZRTPSecurityMode_Test() 
    { 
      Init(); 
    } 
}; 
static PFactory<OpalSecurityMode>::Worker<ZRTPSecurityMode_Test> factoryZRTPSecurityMode_Test("ZRTP|Test"); \

//DECLARE_LIBZRTP_CRYPTO_ALG(AES_CM_128_HMAC_SHA1_80);
//DECLARE_LIBZRTP_CRYPTO_ALG(AES_CM_128_HMAC_SHA1_32);
//DECLARE_LIBZRTP_CRYPTO_ALG(AES_CM_128_NULL_AUTH);
//DECLARE_LIBZRTP_CRYPTO_ALG(NULL_CIPHER_HMAC_SHA1_80);

///////////////////////////////////////////////////////

extern "C" {

void zrtp_print_log(log_level_t level, const char* format, ...)
{
  va_list arg;
  va_start(arg, format);
  PTRACE(level+1, "libZRTP\t" + psprintf(format, arg));
  va_end( arg );
}

int zrtp_send_rtp(const zrtp_stream_ctx_t* /*stream_ctx*/, char* /*packet*/, unsigned int /*length*/)
{
	return zrtp_status_ok;
}

zrtp_status_t zrtp_packet_callback(zrtp_packet_event_t /*evnt*/, zrtp_stream_ctx_t * /*ctx*/, zrtp_rtp_info_t * /*packet*/)
{
  return zrtp_status_ok;
}

void zrtp_event_callback(zrtp_event_t evnt, zrtp_stream_ctx_t * /*ctx*/)
{
  //zrtp_test_stream_t* stream = (zrtp_test_stream_t*) ctx->stream_usr_data;	
    
	zrtp_print_log(LOG_DEBUG, ">>>>>>>>>>>>>>>>>>>>>>>\n");
  switch (evnt){
	  case ZRTP_EVENT_GOTHELLO:
	    zrtp_print_log(LOG_DEBUG, "libzrtp_test: ZRTP_EVENT_GOTHELLO.\n");
	    break;
    case ZRTP_EVENT_GOTHELLOACK:
	    zrtp_print_log(LOG_DEBUG, "libzrtp_test: stream ZRTP_EVENT_GOTHELLOACK.\n");
	    break;
	  case ZRTP_EVENT_IS_CLEAR:
	    zrtp_print_log(LOG_DEBUG, "libzrtp_test: stream ZRTP_EVENT_IS_CLEAR.\n");
	    break;
	  case ZRTP_EVENT_IS_INITIATINGSECURE:
	    zrtp_print_log(LOG_DEBUG, "libzrtp_test: stream ZRTP_EVENT_IS_INITIATINGSECURE.\n");
	    break;
	  case ZRTP_EVENT_IS_PENDINGSECURE:
	    zrtp_print_log(LOG_DEBUG, "libzrtp_test: stream ZRTP_EVENT_IS_PENDINGSECURE.\n");
	    break;
	  case ZRTP_EVENT_IS_INITIATINGCLEAR:
	    zrtp_print_log(LOG_DEBUG, "libzrtp_test: stream ZRTP_EVENT_IS_INITIATINGCLEAR.\n");		
	    break;
	  case ZRTP_EVENT_IS_PENDINGCLEAR:
	    zrtp_print_log(LOG_DEBUG, "libzrtp_test: stream ZRTP_EVENT_IS_PENDINGCLEAR.\n");
	    break;
	  case ZRTP_EVENT_IS_SECURE:
	    zrtp_print_log(LOG_DEBUG, "libzrtp_test: stream ZRTP_EVENT_IS_SECURE.\n");
	    //zrtp_print_log(LOG_DEBUG, "libzrtp_test: stream atributes:\n");
	    //zrtp_print_log(LOG_DEBUG, "libzrtp_test: CIPHER    =%.4s\n", ctx->_session_ctx->_blockcipher->type);
	    //zrtp_print_log(LOG_DEBUG, "libzrtp_test: ATL       =%.4s\n", ctx->_session_ctx->_authtaglength->type);
	    //zrtp_print_log(LOG_DEBUG, "libzrtp_test: SAS       =%.4s\n", ctx->_session_ctx->_sasscheme->type);
	    //zrtp_print_log(LOG_DEBUG, "libzrtp_test: HASH      =%.4s\n", ctx->_session_ctx->_hash->type);
	    //zrtp_print_log(LOG_DEBUG, "libzrtp_test: PKType    =%.4s\n", ctx->_session_ctx->_pubkeyscheme->type);
	    //zrtp_print_log(LOG_DEBUG, "libzrtp_test: \n");
	    //zrtp_print_log(LOG_DEBUG, "libzrtp_test: Staysecure=%s\n", ctx->_session_ctx->_staysecure ? "ON" : "OFF");
	    //zrtp_print_log(LOG_DEBUG, "libzrtp_test: Autosecure=%s\n", ctx->_session_ctx->_profile._autosecure ? "ON" : "OFF");
	    //zrtp_print_log(LOG_DEBUG, "libzrtp_test: \n");
	    //zrtp_print_log(LOG_DEBUG, "libzrtp_test: SAS value: <<<%.4s>>>\n", ctx->_session_ctx->sas_values.str1);		
  		break;
	  case ZRTP_EVENT_ERROR:
	    zrtp_print_log(LOG_DEBUG, "libzrtp_test: stream ZRTP_EVENT_ERROR.\n");		
		  //stream->conn->state = TEST_ERROR_STATE;
	    break;
	  default:
      break;
  }	
	zrtp_print_log(LOG_DEBUG, "<<<<<<<<<<<<<<<<<<<<<<<\n");	
}

void zrtp_play_alert(zrtp_stream_ctx_t * /*stream_ctx*/)
{
	//stream_ctx->_need_play_alert = zrtp_play_no;
}

};
///////////////////////////////////////////////////////

ZRTPSecurityMode_Base::ZRTPSecurityMode_Base()
{
}

void ZRTPSecurityMode_Base::Init()
{
  {
    PWaitAndSignal m(initMutex);
    if (zrtpContext == NULL) {
      zrtpContext = new zrtp_global_ctx_t;
      zrtp_init(zrtpContext, (const char *)PProcess::Current().GetName());
    }
  }
}

RTP_UDP * ZRTPSecurityMode_Base::CreateRTPSession(
  PHandleAggregator * _aggregator,   ///< handle aggregator
  unsigned id,                       ///<  Session ID for RTP channel
  BOOL remoteIsNAT                   ///<  TRUE is remote is behind NAT
)
{
  OpalZRTP_UDP * session = new OpalZRTP_UDP(_aggregator, id, remoteIsNAT);
  session->SetSecurityMode(this);
  return session;
}

BOOL ZRTPSecurityMode_Base::Open()
{
  return TRUE;
}

///////////////////////////////////////////////////////

OpalZRTP_UDP::OpalZRTP_UDP(PHandleAggregator * _aggregator,   ///<  RTP aggregator
                                      unsigned id,            ///<  Session ID for RTP channel
                                          BOOL remoteIsNAT)   ///<  TRUE is remote is behind NAT
  : SecureRTP_UDP(_aggregator, id, remoteIsNAT)
{
}

OpalZRTP_UDP::~OpalZRTP_UDP()
{
}

RTP_UDP::SendReceiveStatus OpalZRTP_UDP::OnSendData(RTP_DataFrame & /*frame*/)
{
  return e_IgnorePacket;
}

RTP_UDP::SendReceiveStatus OpalZRTP_UDP::OnReceiveData(RTP_DataFrame & /*frame*/)
{
  return e_IgnorePacket;
}

RTP_UDP::SendReceiveStatus OpalZRTP_UDP::OnSendControl(RTP_ControlFrame & /*frame*/, PINDEX & /* len */)
{
  return e_IgnorePacket;
}

RTP_UDP::SendReceiveStatus OpalZRTP_UDP::OnReceiveControl(RTP_ControlFrame & /*frame*/)
{
  return e_IgnorePacket;
}

#endif
