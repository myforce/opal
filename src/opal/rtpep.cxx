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

#include <opal/buildopts.h>

#include <opal/rtpep.h>

OpalRTPEndPoint::OpalRTPEndPoint(OpalManager & manager,     ///<  Manager of all endpoints.
                       const PCaselessString & prefix,      ///<  Prefix for URL style address strings
                                      unsigned attributes)  ///<  Bit mask of attributes endpoint has
  : OpalEndPoint(manager, prefix, attributes)
#ifdef OPAL_ZRTP
    , zrtpEnabled(manager.GetZRTPEnabled())
#endif
{
}

OpalRTPEndPoint::~OpalRTPEndPoint()
{
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
  return manager.GetCommonMediaFormats(true, false);
}


#ifdef OPAL_ZRTP

bool OpalRTPEndPoint::GetZRTPEnabled() const
{ 
  return zrtpEnabled; 
}

#endif


static RTP_UDP * GetRTPFromStream(const OpalMediaStream & stream)
{
  const OpalRTPMediaStream * rtpStream = dynamic_cast<const OpalRTPMediaStream *>(&stream);
  if (rtpStream == NULL)
    return NULL;

  return dynamic_cast<RTP_UDP *>(&rtpStream->GetRtpSession());
}


void OpalRTPEndPoint::OnClosedMediaStream(const OpalMediaStream & stream)
{
  RTP_UDP * rtp = GetRTPFromStream(stream);
  if (rtp != NULL) {
    RtpPortMap::iterator it = m_connectionsByRtpLocalPort.find(rtp->GetRemoteDataPort());
    if (it != m_connectionsByRtpLocalPort.end())
      OnLocalRTP(stream.GetConnection(), *it->second, rtp->GetSessionID(), false);

    it = m_connectionsByRtpLocalPort.find(rtp->GetLocalDataPort());
    if (it != m_connectionsByRtpLocalPort.end())
      m_connectionsByRtpLocalPort.erase(it);
  }

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
  RTP_UDP * rtp = GetRTPFromStream(stream);
  if (rtp == NULL)
    return false;

  m_connectionsByRtpLocalPort[rtp->GetLocalDataPort()] = &stream.GetConnection();
  RtpPortMap::iterator it = m_connectionsByRtpLocalPort.find(rtp->GetRemoteDataPort());
  if (it == m_connectionsByRtpLocalPort.end())
    return false;

  return OnLocalRTP(stream.GetConnection(), *it->second, rtp->GetSessionID(), true);
}
