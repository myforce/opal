/*
 * pcapfile.cxx
 *
 * Ethernet capture (PCAP) file declaration
 *
 * Portable Tools Library
 *
 * Copyright (C) 2011 Vox Lucida Pty. Ltd.
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
 * The Original Code is Portable Tools Library.
 *
 * The Initial Developer of the Original Code is Vox Lucida
 *
 * All Rights Reserved.
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "pcapfile.h"
#endif

#include <rtp/pcapfile.h>
#include <codec/vidcodec.h>


void Reverse(char * ptr, size_t sz)
{
  char * top = ptr+sz-1;
  while (ptr < top) {
    char t = *ptr;
    *ptr = *top;
    *top = t;
    ptr++;
    top--;
  }
}

#define REVERSE(p) Reverse((char *)&p, sizeof(p))


///////////////////////////////////////////////////////////////////////////////

OpalPCAPFile::OpalPCAPFile()
  : m_filterSSRC(0)
{
  OpalMediaFormatList list = OpalMediaFormat::GetAllRegisteredMediaFormats();
  for (PINDEX i = 0; i < list.GetSize(); i++) {
    if (list[i].GetPayloadType() < RTP_DataFrame::DynamicBase)
      m_payloadType2mediaFormat[list[i].GetPayloadType()] = list[i];
  }
}


bool OpalPCAPFile::InternalOpen(OpenMode mode, OpenOptions opts, PFileInfo::Permissions permissions)
{
  PAssert(mode != PFile::ReadWrite, PInvalidParameter);

  if (!PFile::InternalOpen(mode, opts, permissions))
    return false;

  if (mode == PFile::WriteOnly) {
    m_fileHeader.magic_number = 0xa1b2c3d4;
    m_fileHeader.version_major = 2;
    m_fileHeader.version_minor = 4;
    m_fileHeader.thiszone = PTime().GetTimeZone();
    m_fileHeader.sigfigs = 0;
    m_fileHeader.snaplen = 65535;
    m_fileHeader.network = 1;
    if (Write(&m_fileHeader, sizeof(m_fileHeader)))
      return true;
    PTRACE(1, "PCAPFile\tCould not write header to \"" << GetFilePath() << '"');
    return false;
  }

  if (!Read(&m_fileHeader, sizeof(m_fileHeader))) {
    PTRACE(1, "PCAPFile\tCould not read header from \"" << GetFilePath() << '"');
    return false;
  }

  if (m_fileHeader.magic_number == 0xa1b2c3d4)
    m_rawPacket.m_otherEndian = false;
  else if (m_fileHeader.magic_number == 0xd4c3b2a1)
    m_rawPacket.m_otherEndian = true;
  else {
    PTRACE(1, "PCAPFile\tFile \"" << GetFilePath() << "\" is not a PCAP file, bad magic number.");
    return false;
  }

  if (m_rawPacket.m_otherEndian) {
    REVERSE(m_fileHeader.version_major);
    REVERSE(m_fileHeader.version_minor);
    REVERSE(m_fileHeader.thiszone);
    REVERSE(m_fileHeader.sigfigs);
    REVERSE(m_fileHeader.snaplen);
    REVERSE(m_fileHeader.network);
  }

  return true;
}


bool OpalPCAPFile::Restart()
{
  if (SetPosition(sizeof(m_fileHeader)))
    return true;

  PTRACE(2, "PCAPFile\tCould not seek beginning of \"" << GetFilePath() << '"');
  return false;
}


void OpalPCAPFile::PrintOn(ostream & strm) const
{
  strm << "PCAP v" << m_fileHeader.version_major << '.' << m_fileHeader.version_minor
                   << " file \"" << GetFilePath() << '"';
}


bool OpalPCAPFile::WriteFrame(const PEthSocket::Frame & frame)
{
  RecordHeader header;
  header.ts_sec  = (uint32_t)frame.GetTimestamp().GetTimeInSeconds();
  header.ts_usec = frame.GetTimestamp().GetMicrosecond();
  header.incl_len = header.orig_len = frame.GetSize();
  PWaitAndSignal mutex(m_writeMutex);
  return Write(&header, sizeof(header)) && frame.Write(*this);
}


bool OpalPCAPFile::WriteRTP(const RTP_DataFrame & rtp, WORD port)
{
  PEthSocket::Frame packet;
  memcpy(packet.CreateUDP(PIPSocketAddressAndPort(GetFilterSrcIP(), port),
                          PIPSocketAddressAndPort(GetFilterDstIP(), port),
                          rtp.GetPacketSize()),
         rtp, rtp.GetPacketSize());
  return WriteFrame(packet);
}


bool OpalPCAPFile::Frame::Read(PChannel & channel, PINDEX)
{
  PreRead();

  RecordHeader recordHeader;
  if (!channel.Read(&recordHeader, sizeof(recordHeader))) {
    PTRACE(1, "PCAPFile\tTruncated file \"" << channel.GetName() << '"');
    return false;
  }

  if (m_otherEndian) {
    REVERSE(recordHeader.ts_sec);
    REVERSE(recordHeader.ts_usec);
    REVERSE(recordHeader.incl_len);
    REVERSE(recordHeader.orig_len);
  }

  m_timestamp.SetTimestamp(recordHeader.ts_sec, recordHeader.ts_usec);

  if (!channel.Read(m_rawData.GetPointer(recordHeader.incl_len), recordHeader.incl_len)) {
    PTRACE(1, "PCAPFile\tTruncated file \"" << channel.GetName() << '"');
    return false;
  }

  m_rawSize = recordHeader.incl_len;
  return true;
}


int OpalPCAPFile::GetDataLink(PBYTEArray & payload)
{
  return m_rawPacket.Read(*this) ? m_rawPacket.GetDataLink(payload) : -1;
}


int OpalPCAPFile::GetIP(PBYTEArray & payload)
{
  if (!m_rawPacket.Read(*this))
    return -1;

  PIPSocket::Address src, dst;
  int type = m_rawPacket.GetIP(payload, src, dst);
  if (type < 0)
    return -1;

  m_packetSrc.SetAddress(src);
  m_packetDst.SetAddress(dst);

  return (m_filterSrc.GetAddress().IsValid() && m_filterSrc.GetAddress() != src) ||
         (m_filterDst.GetAddress().IsValid() || m_filterDst.GetAddress() == dst) ? -1 : type;
}


int OpalPCAPFile::GetTCP(PBYTEArray & payload)
{
  return m_rawPacket.Read(*this) &&
         m_rawPacket.GetTCP(payload, m_packetSrc, m_packetDst) &&
         m_packetSrc.MatchWildcard(m_filterSrc) &&
         m_packetDst.MatchWildcard(m_filterDst)
         ? payload.GetSize() : -1;
}


int OpalPCAPFile::GetUDP(PBYTEArray & payload)
{
  return m_rawPacket.Read(*this) &&
         m_rawPacket.GetUDP(payload, m_packetSrc, m_packetDst) &&
         m_packetSrc.MatchWildcard(m_filterSrc) &&
         m_packetDst.MatchWildcard(m_filterDst)
         ? payload.GetSize() : -1;
}


int OpalPCAPFile::GetRTP(RTP_DataFrame & rtp)
{
  int packetLength = GetUDP(rtp);
  if (packetLength < 0)
    return -1;

  if (!rtp.SetPacketSize(packetLength))
    return -1;

  if (rtp.GetVersion() != 2)
    return -1;

  RTP_DataFrame::PayloadTypes pt = rtp.GetPayloadType();
  if (pt >= RTP_DataFrame::StartConflictRTCP && pt <= RTP_DataFrame::EndConflictRTCP)
    return -1;

  RTP_SyncSourceId ssrc = rtp.GetSyncSource();
  if (ssrc == 0 || (m_filterSSRC != 0 && m_filterSSRC != ssrc))
    return -1;

  if (rtp.GetContribSrcCount() > 4) // While possible, extremely unlikely in modern usage
    return -1;

  return pt;
}


int OpalPCAPFile::GetDecodedRTP(RTP_DataFrame & decodedRTP, OpalTranscoder * & transcoder)
{
  RTP_DataFrame encodedRTP;
  if (GetRTP(encodedRTP) < 0)
    return 0;

  if (transcoder == NULL) {
    OpalMediaFormat srcFmt = GetMediaFormat(encodedRTP);
    if (!srcFmt.IsValid())
      return -1;

    OpalMediaFormat dstFmt =
#if OPAL_VIDEO
            srcFmt.GetMediaType() == OpalMediaType::Video() ? static_cast<OpalMediaFormat>(OpalYUV420P) :
#endif
            static_cast<OpalMediaFormat>(GetOpalPCM16(srcFmt.GetClockRate(), srcFmt.GetOptionInteger(OpalAudioFormat::ChannelsOption())));

    transcoder = OpalTranscoder::Create(srcFmt, dstFmt);
    if (transcoder == NULL)
      return -2;
  }

  RTP_DataFrameList output;
  if (!transcoder->ConvertFrames(encodedRTP, output))
    return -3;

  if (output.IsEmpty())
    return 0;

  decodedRTP = output.front();
  return 1;
}


OpalPCAPFile::DiscoveredRTPKey::DiscoveredRTPKey()
  : m_ssrc(0)
{
}


PObject::Comparison OpalPCAPFile::DiscoveredRTPKey::Compare(const PObject & obj) const
{
  const DiscoveredRTPKey & other = dynamic_cast<const DiscoveredRTPKey &>(obj);
  Comparison c = m_src.Compare(other.m_src);
  if (c == EqualTo)
    c = m_dst.Compare(other.m_dst);
  if (c == EqualTo)
    c = Compare2(m_ssrc, other.m_ssrc);
  return c;
}


OpalPCAPFile::DiscoveredRTPInfo::DiscoveredRTPInfo()
  : m_payloadType(RTP_DataFrame::IllegalPayloadType)
{
}


OpalPCAPFile::DiscoveredRTPInfo::DiscoveredRTPInfo(const DiscoveredRTPKey & key)
  : DiscoveredRTPKey(key)
  , m_payloadType(RTP_DataFrame::IllegalPayloadType)
{
}


void OpalPCAPFile::DiscoveredRTPInfo::PrintOn(ostream & strm) const
{
  strm << m_src << " -> " << m_dst << ", SSRC=" << RTP_TRACE_SRC(m_ssrc) << ", " << m_payloadType << ", ";
  if (m_mediaFormat.IsValid())
    strm << ", " << m_mediaFormat;
  else
    strm << "Unknown media format";
}


struct OpalPCAPFile::DiscoveryInfo
{
  unsigned           m_totalPackets;
  RTP_SequenceNumber m_expectedSequenceNumber;
  unsigned           m_matchedSequenceNumber;
  RTP_Timestamp      m_lastTimestamp;
  unsigned           m_matchedTimestamps;
  RTP_DataFrameList  m_firstFrames;
  map<RTP_DataFrame::PayloadTypes, unsigned> m_payloadTypes;

  /////

  DiscoveryInfo(const RTP_DataFrame & rtp)
    : m_totalPackets(1)
    , m_expectedSequenceNumber(rtp.GetSequenceNumber()+1)
    , m_matchedSequenceNumber(0)
    , m_lastTimestamp(rtp.GetTimestamp())
    , m_matchedTimestamps(1)
  {
    m_payloadTypes[rtp.GetPayloadType()]++;
    AddPacket(rtp);
  }

  void ProcessPacket(const RTP_DataFrame & rtp)
  {
    ++m_totalPackets;
    m_payloadTypes[rtp.GetPayloadType()]++;

    RTP_SequenceNumber sn = rtp.GetSequenceNumber();
    if (m_expectedSequenceNumber == sn)
      ++m_matchedSequenceNumber;
    m_expectedSequenceNumber = sn+1;

    RTP_Timestamp ts = rtp.GetTimestamp();
    if (ts >= m_lastTimestamp)
      ++m_matchedTimestamps;
    m_lastTimestamp = ts;

    AddPacket(rtp);
  }

  void AddPacket(const RTP_DataFrame & rtp)
  {
    if (m_firstFrames.size() > 100)
      return;

    RTP_DataFrame * newRTP = new RTP_DataFrame(rtp);
    newRTP->MakeUnique();
    m_firstFrames.Append(newRTP);
  }

  bool Finalise(DiscoveredRTPInfo & info)
  {
    if (m_totalPackets < 100) // Not worth worrying about
      return false;

    unsigned matchThreshold = m_totalPackets*4/5; // 80%

    if (m_matchedSequenceNumber < matchThreshold) // Most of them consecutive
      return false;

    if (m_matchedTimestamps < matchThreshold) // Most of them consecutive
      return false;

    unsigned maxPayloadTypeCount = 0;
    for (map<RTP_DataFrame::PayloadTypes, unsigned>::iterator it = m_payloadTypes.begin(); it != m_payloadTypes.end(); ++it) {
      if (maxPayloadTypeCount < it->second) {
        maxPayloadTypeCount = it->second;
        info.m_payloadType = it->first;
      }
    }
    if (maxPayloadTypeCount < m_totalPackets/2)
      return false;

    // look for known types
    if (info.m_payloadType <= RTP_DataFrame::LastKnownPayloadType) {
      OpalMediaFormatList formats = OpalMediaFormat::GetAllRegisteredMediaFormats();
      OpalMediaFormatList::const_iterator fmt = formats.FindFormat(info.m_payloadType);
      if (fmt != formats.end())
        info.m_mediaFormat = *fmt;
    }

#if OPAL_VIDEO
    if (!info.m_mediaFormat.IsValid()) {
      struct
      {
        OpalVideoFormat m_format;
        PBYTEArray      m_context;
      } VideoCodecs[] = {
        { OPAL_H263 },
        { OPAL_H263plus },
        { OPAL_MPEG4 },
        { OPAL_H264 },
        { OPAL_VP8 },
      };

      // try and identify media by inspection
      for (PINDEX i = 0; i < PARRAYSIZE(VideoCodecs); ++i) {
        RTP_DataFrameList::iterator rtp;
        for (rtp = m_firstFrames.begin(); rtp != m_firstFrames.end(); ++rtp) {
          if (VideoCodecs[i].m_format.GetVideoFrameType(rtp->GetPayloadPtr(),
                                                        rtp->GetPayloadSize(),
                                                        VideoCodecs[i].m_context) == OpalVideoFormat::e_IntraFrame)
            break;
        }
        if (rtp != m_firstFrames.end()) {
          info.m_mediaFormat = VideoCodecs[i].m_format;
          break;
        }
      }
    }
#endif // OPAL_VIDEO

    return true;
  }
};


bool OpalPCAPFile::DiscoverRTP(DiscoveredRTP & discoveredRTP, const ProgressNotifier & progressNotifier)
{
  if (!Restart())
    return false;

  map<DiscoveredRTPKey, DiscoveryInfo> discoveryMap;

  Progress progress(GetLength());
  while (!IsEndOfFile()) {
    ++progress.m_packets;
    progress.m_filePosition = GetPosition();

    if (!progressNotifier.IsNULL())
      progressNotifier(*this, progress);
    if (progress.m_abort)
      return false;

    RTP_DataFrame rtp;
    if (GetRTP(rtp) < 0)
      continue;

    DiscoveredRTPKey key;
    key.m_src = m_packetSrc;
    key.m_dst = m_packetDst;
    key.m_ssrc = rtp.GetSyncSource();

    map<DiscoveredRTPKey, DiscoveryInfo>::iterator it;
    if ((it = discoveryMap.find(key)) == discoveryMap.end())
      discoveryMap.insert(make_pair(key, rtp));
    else
      it->second.ProcessPacket(rtp);
  }

  for (map<DiscoveredRTPKey, DiscoveryInfo>::iterator it = discoveryMap.begin(); it != discoveryMap.end(); ++it) {
    DiscoveredRTPInfo * info = new DiscoveredRTPInfo(it->first);
    if (it->second.Finalise(*info))
      discoveredRTP.Append(info);
    else
      delete info;
  }

  return Restart();
}


bool OpalPCAPFile::SetFilters(const DiscoveredRTPInfo & info, const PString & format)
{
  if (!SetPayloadMap(info.m_payloadType, format.IsEmpty() ? info.m_mediaFormat : OpalMediaFormat(format)))
    return false;

  m_filterSrc = info.m_src;
  m_filterDst = info.m_dst;
  m_filterSSRC = info.m_ssrc;
  return true;
}


bool OpalPCAPFile::SetPayloadMap(RTP_DataFrame::PayloadTypes pt, const OpalMediaFormat & format)
{
  if (pt == RTP_DataFrame::IllegalPayloadType)
    return false;

  if (!format.IsTransportable())
    return false;

  m_payloadType2mediaFormat[pt] = format;
  m_payloadType2mediaFormat[pt].SetPayloadType(pt);
  return true;
}


OpalMediaFormat OpalPCAPFile::GetMediaFormat(const RTP_DataFrame & rtp) const
{
  std::map<RTP_DataFrame::PayloadTypes, OpalMediaFormat>::const_iterator iter =
                                          m_payloadType2mediaFormat.find(rtp.GetPayloadType());
  return iter != m_payloadType2mediaFormat.end() ? iter->second : OpalMediaFormat();
}


// End Of File ///////////////////////////////////////////////////////////////
