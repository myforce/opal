/*
 * rtpep.cxx
 *
 * Open Phone Abstraction Library (OPAL)
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
 * The Original Code is Open Phone Abstraction Library.
 *
 * The Initial Developer of the Original Code is Post Increment
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#include <ptlib.h>

#ifdef P_USE_PRAGMA
#pragma implementation "rtpep.h"
#endif

#include <opal_config.h>

#include <rtp/rtpep.h>
#include <rtp/rtpconn.h>
#include <rtp/rtp_stream.h>


#if OPAL_RTP_FEC

namespace OpalFEC
{
  class MediaDefinition : public OpalMediaTypeDefinition
  {
  public:
    static const char * Name()
    {
      return "RTP-FEC";
    }

    MediaDefinition()
      : OpalMediaTypeDefinition(Name(), NULL)
    {
    }
  };

  OPAL_INSTANTIATE_MEDIATYPE2(MediaDefinition, MediaDefinition::Name());

  const OpalMediaType & MediaType()
  {
    static OpalMediaType mt(MediaDefinition::Name());
    return mt;
  }


  const PString & MediaTypeOption()
  {
    static const PConstCaselessString s("Media-Type");
    return s;
  }


  class BaseMediaFormat : public OpalMediaFormat
  {
    PCLASSINFO(BaseMediaFormat, OpalMediaFormat)
  public:
    BaseMediaFormat(const char * name, const OpalMediaType & mediaType, unsigned clockRate, const char * encodingName, const char * desc)
      : OpalMediaFormat(name, MediaType(), RTP_DataFrame::DynamicBase, encodingName, false, 0, 0, 0, clockRate)
    {
      SetOptionString(OpalMediaFormat::DescriptionOption(), desc + mediaType);
      AddOption(new OpalMediaOptionString(MediaTypeOption(), true, mediaType));
    }
  };


  class RedundantMediaFormat : public BaseMediaFormat
  {
    PCLASSINFO(RedundantMediaFormat, BaseMediaFormat)
  public:
    RedundantMediaFormat(const char * name, const OpalMediaType & mediaType, unsigned clockRate)
      : BaseMediaFormat(name, mediaType, clockRate, "red", "RFC 2198 Redundant RTP for ")
    {
      AddOption(new OpalMediaOptionString("FMTP", true));
    }
  };


  const OpalMediaFormat & RedundantAudio()
  {
    static RedundantMediaFormat const fmt(OPAL_REDUNDANT_AUDIO, OpalMediaType::Audio(), OpalMediaFormat::AudioClockRate);
    return fmt;
  }


  class UlpFecMediaFormat : public BaseMediaFormat
  {
    PCLASSINFO(UlpFecMediaFormat, BaseMediaFormat)
  public:
    UlpFecMediaFormat(const char * name, const OpalMediaType & mediaType, unsigned clockRate)
      : BaseMediaFormat(name, mediaType, clockRate, "ulpfec", "RFC 5109 ULP Forward Error Correction for ")
    {
    }
  };


  const OpalMediaFormat & UlpFecAudio()
  {
    static UlpFecMediaFormat const fmt(OPAL_ULP_FEC_AUDIO, OpalMediaType::Audio(), OpalMediaFormat::AudioClockRate);
    return fmt;
  }


#if OPAL_VIDEO
  const OpalMediaFormat & RedundantVideo()
  {
    static RedundantMediaFormat const fmt(OPAL_REDUNDANT_VIDEO, OpalMediaType::Video(), OpalMediaFormat::VideoClockRate);
    return fmt;
  }


  const OpalMediaFormat & UlpFecVideo()
  {
    static UlpFecMediaFormat const fmt(OPAL_ULP_FEC_VIDEO, OpalMediaType::Video(), OpalMediaFormat::VideoClockRate);
    return fmt;
  }
#endif //OPAL_VIDEO

};

#endif // OPAL_RTP_FEC


///////////////////////////////////////////////////////////////////////////////

OpalRTPEndPoint::OpalRTPEndPoint(OpalManager & manager,     ///<  Manager of all endpoints.
                       const PCaselessString & prefix,      ///<  Prefix for URL style address strings
                                  Attributes   attributes)  ///<  Bit mask of attributes endpoint has
  : OpalEndPoint(manager, prefix, attributes)
{
}


OpalRTPEndPoint::~OpalRTPEndPoint()
{
}


void OpalRTPEndPoint::OnReleased(OpalConnection & connection)
{
  OpalEndPoint::OnReleased(connection);

  m_connectionsByRtpMutex.Wait();
  for (LocalRtpInfoMap::iterator it = m_connectionsByRtpLocalAddr.begin(); it != m_connectionsByRtpLocalAddr.end(); ) {
    if (&it->second.m_connection == &connection)
      m_connectionsByRtpLocalAddr.erase(it++);
    else
      ++it;
  }
  m_connectionsByRtpMutex.Signal();
}


PBoolean OpalRTPEndPoint::IsRTPNATEnabled(OpalConnection & conn, 
                                const PIPSocket::Address & localAddr, 
                                const PIPSocket::Address & peerAddr,
                                const PIPSocket::Address & sigAddr,
                                                PBoolean   incoming)
{
  return GetManager().IsRTPNATEnabled(conn, localAddr, peerAddr, sigAddr, incoming);
}


OpalMediaFormatList OpalRTPEndPoint::GetMediaFormats() const
{
  OpalMediaFormatList list = manager.GetCommonMediaFormats(true, false);
#if OPAL_RTP_FEC
  if (!list.IsEmpty()) {
    list += OpalRedundantAudio;
    list += OpalUlpFecAudio;
 #if OPAL_VIDEO
    list += OpalRedundantVideo;
    list += OpalUlpFecVideo;
 #endif
  }
#endif
  return list;
}


static OpalRTPSession * GetRTPFromStream(const OpalMediaStream & stream)
{
  const OpalRTPMediaStream * rtpStream = dynamic_cast<const OpalRTPMediaStream *>(&stream);
  return rtpStream == NULL ? NULL : &rtpStream->GetRtpSession();
}


void OpalRTPEndPoint::OnClosedMediaStream(const OpalMediaStream & stream)
{
  CheckEndLocalRTP(stream.GetConnection(), GetRTPFromStream(stream));

  OpalEndPoint::OnClosedMediaStream(stream);
}


bool OpalRTPEndPoint::OnLocalRTP(OpalConnection & connection1,
                                 OpalConnection & connection2,
                                 unsigned         sessionID,
                                 bool             opened) const
{
  return manager.OnLocalRTP(connection1, connection2, sessionID, opened);
}


bool OpalRTPEndPoint::CheckForLocalRTP(const OpalRTPMediaStream & stream)
{
  OpalRTPSession * rtp = GetRTPFromStream(stream);
  if (rtp == NULL)
    return false;

  OpalConnection & connection = stream.GetConnection();

  OpalTransportAddress remoteAddr = rtp->GetRemoteAddress();
  PIPSocket::Address remoteIP;
  if (!remoteAddr.GetIpAddress(remoteIP))
    return false;

  if (!PIPSocket::IsLocalHost(remoteIP)) {
    PTRACE(5, "RTPEp\tSession " << stream.GetSessionID() << ", "
              "remote RTP address " << rtp->GetRemoteAddress() << " not local (different host).");
    CheckEndLocalRTP(connection, rtp);
    return false;
  }

  PWaitAndSignal mutex(m_connectionsByRtpMutex);

  OpalTransportAddress localAddr = rtp->GetLocalAddress();
  LocalRtpInfoMap::iterator itLocal = m_connectionsByRtpLocalAddr.find(localAddr);
  if (itLocal == m_connectionsByRtpLocalAddr.end())
    return false;

  LocalRtpInfoMap::iterator itRemote = m_connectionsByRtpLocalAddr.find(remoteAddr);
  if (itRemote == m_connectionsByRtpLocalAddr.end()) {
    PTRACE(4, "RTPEp\tSession " << stream.GetSessionID() << ", "
              "remote RTP address " << remoteAddr << " not local (different process).");
    return false;
  }

  bool result;
  bool cached = itRemote->second.m_previousResult >= 0;
  if (cached)
    result = itRemote->second.m_previousResult != 0;
  else {
    result = OnLocalRTP(connection, itRemote->second.m_connection, rtp->GetSessionID(), true);
    itLocal->second.m_previousResult = result;
    itRemote->second.m_previousResult = result;
  }

  PTRACE(3, "RTPEp\tSession " << stream.GetSessionID() << ", "
            "RTP at " << localAddr << " and " << remoteAddr
         << ' ' << (cached ? "cached" : "flagged") << " as "
         << (result ? "bypassed" : "normal")
         << " on connection " << connection);

  return result;
}


void OpalRTPEndPoint::CheckEndLocalRTP(OpalConnection & connection, OpalRTPSession * rtp)
{
  if (rtp == NULL)
    return;

  PWaitAndSignal mutex(m_connectionsByRtpMutex);

  LocalRtpInfoMap::iterator it = m_connectionsByRtpLocalAddr.find(rtp->GetLocalAddress());
  if (it == m_connectionsByRtpLocalAddr.end() || it->second.m_previousResult < 0)
    return;

  PTRACE(5, "RTPEp\tSession " << rtp->GetSessionID() << ", "
            "local RTP port " << it->first << " cache cleared "
            "on connection " << it->second.m_connection);
  it->second.m_previousResult = -1;

  it = m_connectionsByRtpLocalAddr.find(rtp->GetRemoteAddress());
  if (it == m_connectionsByRtpLocalAddr.end() || it->second.m_previousResult < 0)
    return;
  it->second.m_previousResult = -1;
  OnLocalRTP(connection, it->second.m_connection, rtp->GetSessionID(), false);

  PTRACE(5, "RTPEp\tSession " << rtp->GetSessionID() << ", "
            "remote RTP port " << it->first << " is local, ended bypass "
            "on connection " << it->second.m_connection);
}


void OpalRTPEndPoint::RegisterLocalRTP(OpalRTPSession * rtp, bool removed)
{
  if (rtp == NULL)
    return;

  OpalTransportAddress localAddr = rtp->GetLocalAddress();
  m_connectionsByRtpMutex.Wait();
  if (removed) {
    LocalRtpInfoMap::iterator it = m_connectionsByRtpLocalAddr.find(localAddr);
    if (it != m_connectionsByRtpLocalAddr.end()) {
      PTRACE(4, "RTPEp\tSession " << rtp->GetSessionID() << ", "
                "forgetting local RTP at " << localAddr << " on connection " << it->second.m_connection);
      m_connectionsByRtpLocalAddr.erase(it);
    }
  }
  else {
    std::pair<LocalRtpInfoMap::iterator, bool> insertResult =
              m_connectionsByRtpLocalAddr.insert(LocalRtpInfoMap::value_type(localAddr, rtp->GetConnection()));
    PTRACE_IF(4, insertResult.second, "RTPEp", *rtp << "remembering local RTP at " << localAddr << " on connection " << rtp->GetConnection());
    PTRACE_IF(1, &insertResult.first->second.m_connection != &rtp->GetConnection(), "RTPEp", *rtp <<
              "did not remove local RTP at " << localAddr << " on connection " << insertResult.first->second.m_connection);
  }
  m_connectionsByRtpMutex.Signal();
}

