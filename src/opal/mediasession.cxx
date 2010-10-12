/*
 * mediasession.cxx
 *
 * Media session abstraction
 *
 * Open Phone Abstraction Library (OPAL)
 *
 * Copyright (C) 2010 Vox Lucida Pty. Ltd.
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
 * The Initial Developer of the Original Code is Vox Lucida Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#include <ptlib.h>

#ifdef P_USE_PRAGMA
#pragma implementation "mediasession.h"
#endif

#include <opal/buildopts.h>

#include <opal/mediasession.h>


#define new PNEW


///////////////////////////////////////////////////////////////////////////////

#if OPAL_STATISTICS

OpalMediaStatistics::OpalMediaStatistics()
  : m_totalBytes(0)
  , m_totalPackets(0)
  , m_packetsLost(0)
  , m_packetsOutOfOrder(0)
  , m_packetsTooLate(0)
  , m_packetOverruns(0)
  , m_minimumPacketTime(0)
  , m_averagePacketTime(0)
  , m_maximumPacketTime(0)

    // Audio
  , m_averageJitter(0)
  , m_maximumJitter(0)

    // Video
  , m_totalFrames(0)
  , m_keyFrames(0)
{
}

#if OPAL_FAX
OpalMediaStatistics::Fax::Fax()
  : m_result(OpalMediaStatistics::FaxNotStarted)
  , m_bitRate(9600)
  , m_compression(1)
  , m_errorCorrection(false)
  , m_txPages(-1)
  , m_rxPages(-1)
  , m_totalPages(0)
  , m_imageSize(0)
  , m_resolutionX(0)
  , m_resolutionY(0)
  , m_pageWidth(0)
  , m_pageHeight(0)
  , m_badRows(0)
  , m_mostBadRows(0)
  , m_errorCorrectionRetries(0)
{
}
#endif

#endif

/////////////////////////////////////////////////////////////////////////////

OpalMediaSession::OpalMediaSession(OpalConnection & connection, unsigned sessionId, const OpalMediaType & mediaType)
  : m_connection(connection)
  , m_sessionId(sessionId)
  , m_mediaType(mediaType)
  , m_referenceCount(1)
{
}

bool OpalMediaSession::Open(const OpalTransportAddress &)
{
  return true;
}


bool OpalMediaSession::Close(bool reading)
{
  return true;
}


void OpalMediaSession::SetRemoteMediaAddress(const OpalTransportAddress &)
{
}


void OpalMediaSession::AttachTransport(Transport &)
{
}


OpalMediaSession::Transport OpalMediaSession::DetachTransport()
{
  return Transport();
}


#if OPAL_STATISTICS
void OpalMediaSession::GetStatistics(OpalMediaStatistics &, bool) const
{
}
#endif

/////////////////////////////////////////////////////////////////////////////

