/*
 * h323caps.cxx
 *
 * H.323 protocol handler
 *
 * Open H323 Library
 *
 * Copyright (c) 1998-2000 Equivalence Pty. Ltd.
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
 * The Original Code is Open H323 Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Portions of this code were written with the assisance of funding from
 * Vovida Networks, Inc. http://www.vovida.com.
 *
 * Contributor(s): ______________________________________.
 *
 * $Log: h323caps.cxx,v $
 * Revision 1.2006  2002/01/14 06:35:57  robertj
 * Updated to OpenH323 v1.7.9
 *
 * Revision 2.4  2002/01/14 02:22:03  robertj
 * Fixed bug in wildcard name lookup.
 *
 * Revision 2.3  2001/10/05 00:22:14  robertj
 * Updated to PWLib 1.2.0 and OpenH323 1.7.0
 *
 * Revision 2.2  2001/08/13 05:10:39  robertj
 * Updates from OpenH323 v1.6.0 release.
 *
 * Revision 2.1  2001/08/01 05:12:59  robertj
 * Major changes to H.323 capabilities, uses OpalMediaFormat for base name.
 * Added "known" codecs capability clases.
 *
 * Revision 2.0  2001/07/27 15:48:25  robertj
 * Conversion of OpenH323 to Open Phone Abstraction Library (OPAL)
 *
 * Revision 1.42  2002/01/10 05:13:54  robertj
 * Added support for external RTP stacks, thanks NuMind Software Systems.
 *
 * Revision 1.41  2002/01/09 00:21:39  robertj
 * Changes to support outgoing H.245 RequstModeChange.
 *
 * Revision 1.40  2001/12/22 01:44:30  robertj
 * Added more support for H.245 RequestMode operation.
 *
 * Revision 1.39  2001/09/21 02:52:56  robertj
 * Added default implementation for PDU encode/decode for codecs
 *   that have simple integer as frames per packet.
 *
 * Revision 1.38  2001/09/11 10:21:42  robertj
 * Added direction field to capabilities, thanks Nick Hoath.
 *
 * Revision 1.37  2001/08/06 03:08:56  robertj
 * Fission of h323.h to h323ep.h & h323con.h, h323.h now just includes files.
 *
 * Revision 1.36  2001/07/19 09:50:30  robertj
 * Added code for default session ID on data channel being three.
 *
 * Revision 1.35  2001/07/17 04:44:31  robertj
 * Partial implementation of T.120 and T.38 logical channels.
 *
 * Revision 1.34  2001/06/29 04:58:57  robertj
 * Added wildcard character '*' to capability name string searches.
 *
 * Revision 1.33  2001/06/15 16:10:19  rogerh
 * Fix the "capabilities are the same" assertion
 *
 * Revision 1.32  2001/05/31 06:28:37  robertj
 * Made sure capability descriptors alternate capability sets are in the same
 *   order as the capability table when doing reorder. This improves compatibility
 *   with endpoints that select the first capability in that list rather than the table.
 *
 * Revision 1.31  2001/05/14 05:56:28  robertj
 * Added H323 capability registration system so can add capabilities by
 *   string name instead of having to instantiate explicit classes.
 *
 * Revision 1.30  2001/05/02 16:22:21  rogerh
 * Add IsAllow() for a single capability to check if it is in the
 * capabilities set. This fixes the bug where OpenH323 would accept
 * incoming H261 video even when told not to accept it.
 *
 * Revision 1.29  2001/04/12 03:22:44  robertj
 * Fixed fast start checking of returned OLC frame count to use minimum
 *   of user setting and remotes maximum limitation, Was always just
 *   sending whatever the remote said it could do.
 *
 * Revision 1.28  2001/03/16 23:00:22  robertj
 * Improved validation of codec selection against capability set, thanks Chris Purvis.
 *
 * Revision 1.27  2001/03/06 04:44:47  robertj
 * Fixed problem where could send capability set twice. This should not be
 *   a problem except when talking to another broken stack, eg Cisco routers.
 *
 * Revision 1.26  2001/02/09 05:13:55  craigs
 * Added pragma implementation to (hopefully) reduce the executable image size
 * under Linux
 *
 * Revision 1.25  2001/01/25 07:27:16  robertj
 * Major changes to add more flexible OpalMediaFormat class to normalise
 *   all information about media types, especially codecs.
 *
 * Revision 1.24  2001/01/16 03:14:01  craigs
 * Changed nonstanard capability Compare functions to not assert
 * if compared to other capability types
 *
 * Revision 1.23  2001/01/09 23:05:24  robertj
 * Fixed inability to have 2 non standard codecs in capability table.
 *
 * Revision 1.22  2001/01/02 07:50:46  robertj
 * Fixed inclusion of arrays (with bad size) in TCS=0 pdu, thanks Yura Aksyonov.
 *
 * Revision 1.21  2000/12/19 22:32:26  dereks
 * Removed MSVC warning about unused parameter
 *
 * Revision 1.20  2000/11/08 04:50:22  craigs
 * Changed capability reorder function to reorder all capabilities matching
 * preferred order, rather than just the first
 *
 * Revision 1.19  2000/10/16 08:50:08  robertj
 * Added single function to add all UserInput capability types.
 *
 * Revision 1.18  2000/10/13 03:43:29  robertj
 * Added clamping to avoid ever setting incorrect tx frame count.
 *
 * Revision 1.17  2000/10/13 02:20:32  robertj
 * Fixed capability clone so gets all fields including those in ancestor.
 *
 * Revision 1.16  2000/08/23 14:27:04  craigs
 * Added prototype support for Microsoft GSM codec
 *
 * Revision 1.15  2000/07/13 12:30:46  robertj
 * Fixed problems with fast start frames per packet adjustment.
 *
 * Revision 1.14  2000/07/12 10:25:37  robertj
 * Renamed all codecs so obvious whether software or hardware.
 *
 * Revision 1.13  2000/07/10 16:03:02  robertj
 * Started fixing capability set merging, still more to do.
 *
 * Revision 1.12  2000/07/04 01:16:49  robertj
 * Added check for capability allowed in "combinations" set, still needs more done yet.
 *
 * Revision 1.11  2000/07/02 14:08:43  craigs
 * Fixed problem with removing capabilities based on wildcard
 *
 * Revision 1.10  2000/06/03 03:16:39  robertj
 * Fixed using the wrong capability table (should be connections) for some operations.
 *
 * Revision 1.9  2000/05/30 06:53:48  robertj
 * Fixed bug where capability numbers in duplicate table are not identical (should be!).
 *
 * Revision 1.8  2000/05/23 11:32:37  robertj
 * Rewrite of capability table to combine 2 structures into one and move functionality into that class
 *    allowing some normalisation of usage across several applications.
 * Changed H323Connection so gets a copy of capabilities instead of using endponts, allows adjustments
 *    to be done depending on the remote client application.
 *
 * Revision 1.7  2000/05/10 04:05:34  robertj
 * Changed capabilities so has a function to get name of codec, instead of relying on PrintOn.
 *
 * Revision 1.6  2000/05/02 04:32:26  robertj
 * Fixed copyright notice comment.
 *
 * Revision 1.5  2000/04/05 19:01:12  robertj
 * Added function so can change desired transmit packet size.
 *
 * Revision 1.4  2000/03/22 01:29:43  robertj
 * Fixed default "frame" size for audio codecs, caused crash using G.711
 *
 * Revision 1.3  2000/03/21 03:06:50  robertj
 * Changes to make RTP TX of exact numbers of frames in some codecs.
 *
 * Revision 1.2  2000/02/16 03:24:27  robertj
 * Fixed bug in clamping maximum transmit packet size in G.711 capabilities.
 *
 * Revision 1.1  1999/12/23 23:02:36  robertj
 * File reorganision for separating RTP from H.323 and creation of LID for VPB support.
 *
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "h323caps.h"
#endif

#include <h323/h323caps.h>

#include <h323/h323ep.h>
#include <h323/h323con.h>
#include <h323/h323pdu.h>
#include <h323/transaddr.h>



H323_REGISTER_CAPABILITY_FUNCTION(H323_G711ALaw64Capability, OPAL_G711_ALAW_64K, H323_NO_EP_VAR)
{
  return new H323_G711Capability(H323_G711Capability::ALaw);
}

H323_REGISTER_CAPABILITY_FUNCTION(H323_G711uLaw64Capability, OPAL_G711_ULAW_64K, H323_NO_EP_VAR)
{
  return new H323_G711Capability(H323_G711Capability::muLaw);
}

H323_REGISTER_CAPABILITY(H323_G728Capability, OPAL_G728);

H323_REGISTER_CAPABILITY_FUNCTION(H323_G729Capability, OPAL_G729, H323_NO_EP_VAR)
{
  return new H323_G729Capability(H323_G729Capability::e_Normal);
}

H323_REGISTER_CAPABILITY_FUNCTION(H323_G729CapabilityA, OPAL_G729A, H323_NO_EP_VAR)
{
  return new H323_G729Capability(H323_G729Capability::e_AnnexA);
}

H323_REGISTER_CAPABILITY_FUNCTION(H323_G729CapabilityB, OPAL_G729B, H323_NO_EP_VAR)
{
  return new H323_G729Capability(H323_G729Capability::e_AnnexB);
}

H323_REGISTER_CAPABILITY_FUNCTION(H323_G729CapabilityAB, OPAL_G729AB, H323_NO_EP_VAR)
{
  return new H323_G729Capability(H323_G729Capability::e_AnnexA_AnnexB);
}

H323_REGISTER_CAPABILITY_FUNCTION(H323_G7231Capability_6k3, OPAL_G7231_6k3, H323_NO_EP_VAR)
{
  return new H323_G7231Capability();
}

H323_REGISTER_CAPABILITY_FUNCTION(H323_G7231Capability_5k3, OPAL_G7231_5k3, H323_NO_EP_VAR)
{
  return new H323_G7231Capability();
}


H323_REGISTER_CAPABILITY(H323_GSM0610Capability, OPAL_GSM0610);


static H323CapabilityRegistration * RegisteredCapabilitiesListHead;


#define new PNEW


#if PTRACING
ostream & operator<<(ostream & o , H323Capability::MainTypes t)
{
  const char * const names[] = {
    "Audio", "Video", "Data", "UserInput"
  };
  return o << names[t];
}

ostream & operator<<(ostream & o , H323Capability::CapabilityDirection d)
{
  const char * const names[] = {
    "Unknown", "Receive", "Transmit", "ReceiveAndTransmit", "NoDirection"
  };
  return o << names[d];
}
#endif


/////////////////////////////////////////////////////////////////////////////

H323Capability::H323Capability(const OpalMediaFormat & fmt)
  : mediaFormat(fmt)
{
  assignedCapabilityNumber = 0; // Unassigned
  capabilityDirection = e_Unknown;
}


PObject::Comparison H323Capability::Compare(const PObject & obj) const
{
  PAssert(obj.IsDescendant(H323Capability::Class()), PInvalidCast);
  const H323Capability & other = (const H323Capability &)obj;

  int mt = GetMainType();
  int omt = other.GetMainType();
  if (mt < omt)
    return LessThan;
  if (mt > omt)
    return GreaterThan;

  int st = GetSubType();
  int ost = other.GetSubType();
  if (st < ost)
    return LessThan;
  if (st > ost)
    return GreaterThan;

  return EqualTo;
}


void H323Capability::PrintOn(ostream & strm) const
{
  strm << GetMediaFormat();
  if (assignedCapabilityNumber != 0)
    strm << " <" << assignedCapabilityNumber << '>';
}


H323Capability * H323Capability::Create(H323EndPoint & ep, const PString & name)
{
  H323CapabilityRegistration * find = RegisteredCapabilitiesListHead;
  while (find != NULL) {
    if (*find == name)
      return find->Create(ep);
    find = find->link;
  }

  return NULL;
}


unsigned H323Capability::GetDefaultSessionID() const
{
  return 0;
}


void H323Capability::SetTxFramesInPacket(unsigned /*frames*/)
{
}


unsigned H323Capability::GetTxFramesInPacket() const
{
  return 1;
}


unsigned H323Capability::GetRxFramesInPacket() const
{
  return 1;
}


BOOL H323Capability::IsNonStandardMatch(const H245_NonStandardParameter &) const
{
  return FALSE;
}


BOOL H323Capability::OnReceivedPDU(const H245_Capability & cap)
{
  switch (cap.GetTag()) {
    case H245_Capability::e_receiveVideoCapability:
    case H245_Capability::e_receiveAudioCapability:
    case H245_Capability::e_receiveDataApplicationCapability:
    case H245_Capability::e_h233EncryptionReceiveCapability:
    case H245_Capability::e_receiveUserInputCapability:
      capabilityDirection = e_Receive;
      break;

    case H245_Capability::e_transmitVideoCapability:
    case H245_Capability::e_transmitAudioCapability:
    case H245_Capability::e_transmitDataApplicationCapability:
    case H245_Capability::e_h233EncryptionTransmitCapability:
    case H245_Capability::e_transmitUserInputCapability:
      capabilityDirection = e_Transmit;
      break;

    case H245_Capability::e_receiveAndTransmitVideoCapability:
    case H245_Capability::e_receiveAndTransmitAudioCapability:
    case H245_Capability::e_receiveAndTransmitDataApplicationCapability:
    case H245_Capability::e_receiveAndTransmitUserInputCapability:
      capabilityDirection = e_ReceiveAndTransmit;
      break;

    case H245_Capability::e_conferenceCapability:
    case H245_Capability::e_h235SecurityCapability:
    case H245_Capability::e_maxPendingReplacementFor:
      capabilityDirection = e_NoDirection;
  }

  return TRUE;
}


/////////////////////////////////////////////////////////////////////////////

H323RealTimeCapability::H323RealTimeCapability(const OpalMediaFormat & fmt)
  : H323Capability(fmt)
{
}


H323Channel * H323RealTimeCapability::CreateChannel(H323Connection & connection,
                                                    H323Channel::Directions dir,
                                                    unsigned sessionID,
                                 const H245_H2250LogicalChannelParameters * param) const
{
  return connection.CreateRealTimeLogicalChannel(*this, dir, sessionID, param);
}


/////////////////////////////////////////////////////////////////////////////

H323NonStandardCapabilityInfo::H323NonStandardCapabilityInfo(H323EndPoint & endpoint,
                                                             const BYTE * dataPtr,
                                                             PINDEX dataSize,
                                                             PINDEX _offset,
                                                             PINDEX _len)
  : nonStandardData(dataPtr, dataSize),
    comparisonOffset(_offset),
    comparisonLength(_len)
{
  H225_H221NonStandard h221;
  endpoint.SetH221NonStandardInfo(h221);
  t35CountryCode = (BYTE)(unsigned)h221.m_t35CountryCode;
  t35Extension = (BYTE)(unsigned)h221.m_t35Extension;
  manufacturerCode = (WORD)(unsigned)h221.m_manufacturerCode;
}


H323NonStandardCapabilityInfo::H323NonStandardCapabilityInfo(const PString & _oid,
                                                             const BYTE * dataPtr,
                                                             PINDEX dataSize,
                                                             PINDEX _offset,
                                                             PINDEX _len)
  : oid(_oid),
    nonStandardData(dataPtr, dataSize),
    comparisonOffset(_offset),
    comparisonLength(_len)
{
}


H323NonStandardCapabilityInfo::H323NonStandardCapabilityInfo(BYTE country,
                                                             BYTE extension,
                                                             WORD maufacturer,
                                                             const BYTE * dataPtr,
                                                             PINDEX dataSize,
                                                             PINDEX _offset,
                                                             PINDEX _len)
  : t35CountryCode(country),
    t35Extension(extension),
    manufacturerCode(maufacturer),
    nonStandardData(dataPtr, dataSize),
    comparisonOffset(_offset),
    comparisonLength(_len)
{
}


H323NonStandardCapabilityInfo::~H323NonStandardCapabilityInfo()
{
}


BOOL H323NonStandardCapabilityInfo::OnSendingPDU(PBYTEArray & data) const
{
  data = nonStandardData;
  return data.GetSize() > 0;
}


BOOL H323NonStandardCapabilityInfo::OnReceivedPDU(const PBYTEArray & data)
{
  if (CompareData(data) != PObject::EqualTo)
    return FALSE;

  nonStandardData = data;
  return TRUE;
}


BOOL H323NonStandardCapabilityInfo::OnSendingNonStandardPDU(PASN_Choice & pdu,
                                                            unsigned nonStandardTag) const
{
  PBYTEArray data;
  if (!OnSendingPDU(data))
    return FALSE;

  pdu.SetTag(nonStandardTag);
  H245_NonStandardParameter & param = (H245_NonStandardParameter &)pdu.GetObject();

  if (!oid) {
    param.m_nonStandardIdentifier.SetTag(H245_NonStandardIdentifier::e_object);
    ((PASN_ObjectId &)param) = oid;
  }
  else {
    param.m_nonStandardIdentifier.SetTag(H245_NonStandardIdentifier::e_h221NonStandard);
    H245_NonStandardIdentifier_h221NonStandard & h221 = param.m_nonStandardIdentifier;
    h221.m_t35CountryCode = (unsigned)t35CountryCode;
    h221.m_t35Extension = (unsigned)t35Extension;
    h221.m_manufacturerCode = (unsigned)manufacturerCode;
  }

  param.m_data = data;
  return data.GetSize() > 0;
}


BOOL H323NonStandardCapabilityInfo::OnReceivedNonStandardPDU(const PASN_Choice & pdu,
                                                             unsigned nonStandardTag)
{
  if (pdu.GetTag() != nonStandardTag)
    return FALSE;

  const H245_NonStandardParameter & param = (const H245_NonStandardParameter &)pdu.GetObject();

  if (CompareParam(param) != PObject::EqualTo)
    return FALSE;

  return OnReceivedPDU(param.m_data);
}


PObject::Comparison H323NonStandardCapabilityInfo::CompareParam(const H245_NonStandardParameter & param) const
{
  if (!oid) {
    if (param.m_nonStandardIdentifier.GetTag() != H245_NonStandardIdentifier::e_object)
      return PObject::LessThan;

    return oid.Compare((PASN_ObjectId &)param);
  }

  if (param.m_nonStandardIdentifier.GetTag() != H245_NonStandardIdentifier::e_h221NonStandard)
    return PObject::LessThan;

  const H245_NonStandardIdentifier_h221NonStandard & h221 = param.m_nonStandardIdentifier;

  if (h221.m_t35CountryCode < (unsigned)t35CountryCode)
    return PObject::LessThan;
  if (h221.m_t35CountryCode > (unsigned)t35CountryCode)
    return PObject::GreaterThan;

  if (h221.m_t35Extension < (unsigned)t35Extension)
    return PObject::LessThan;
  if (h221.m_t35Extension > (unsigned)t35Extension)
    return PObject::GreaterThan;

  if (h221.m_manufacturerCode < (unsigned)manufacturerCode)
    return PObject::LessThan;
  if (h221.m_manufacturerCode < (unsigned)manufacturerCode)
    return PObject::GreaterThan;


  return PObject::EqualTo;
}


PObject::Comparison H323NonStandardCapabilityInfo::CompareInfo(const H323NonStandardCapabilityInfo & other) const
{
  return CompareData(other.nonStandardData);
}


PObject::Comparison H323NonStandardCapabilityInfo::CompareData(const PBYTEArray & data) const
{
  if (comparisonOffset >= nonStandardData.GetSize())
    return PObject::LessThan;
  if (comparisonOffset >= data.GetSize())
    return PObject::GreaterThan;

  PINDEX len = comparisonLength;
  if (comparisonOffset+len > nonStandardData.GetSize())
    len = nonStandardData.GetSize() - comparisonOffset;

  if (comparisonOffset+len > data.GetSize())
    return PObject::GreaterThan;

  int cmp = memcmp((const BYTE *)nonStandardData + comparisonOffset,
                   (const BYTE *)data + comparisonOffset,
                   len);
  if (cmp < 0)
    return PObject::LessThan;
  if (cmp > 0)
    return PObject::GreaterThan;
  return PObject::EqualTo;
}


/////////////////////////////////////////////////////////////////////////////

H323AudioCapability::H323AudioCapability(const OpalMediaFormat & fmt,
                                         unsigned rx, unsigned tx)
  : H323RealTimeCapability(fmt)
{
  rxFramesInPacket = rx;
  txFramesInPacket = tx;
}


H323Capability::MainTypes H323AudioCapability::GetMainType() const
{
  return e_Audio;
}


unsigned H323AudioCapability::GetDefaultSessionID() const
{
  return OpalMediaFormat::DefaultAudioSessionID;
}


void H323AudioCapability::SetTxFramesInPacket(unsigned frames)
{
  PAssert(frames > 0, PInvalidParameter);
  if (frames > 256)
    txFramesInPacket = 256;
  else
    txFramesInPacket = frames;
}


unsigned H323AudioCapability::GetTxFramesInPacket() const
{
  return txFramesInPacket;
}


unsigned H323AudioCapability::GetRxFramesInPacket() const
{
  return rxFramesInPacket;
}


BOOL H323AudioCapability::OnSendingPDU(H245_Capability & cap) const
{
  cap.SetTag(H245_Capability::e_receiveAudioCapability);
  return OnSendingPDU((H245_AudioCapability &)cap, rxFramesInPacket);
}


BOOL H323AudioCapability::OnSendingPDU(H245_DataType & dataType) const
{
  dataType.SetTag(H245_DataType::e_audioData);
  return OnSendingPDU((H245_AudioCapability &)dataType, txFramesInPacket);
}


BOOL H323AudioCapability::OnSendingPDU(H245_ModeElement & mode) const
{
  mode.m_type.SetTag(H245_ModeElement_type::e_audioMode);
  return OnSendingPDU((H245_AudioMode &)mode);
}


BOOL H323AudioCapability::OnSendingPDU(H245_AudioCapability & pdu,
                                       unsigned packetSize) const
{
  pdu.SetTag(GetSubType());

  // Set the maximum number of frames
  PASN_Integer & value = pdu;
  value = packetSize;
  return TRUE;
}


BOOL H323AudioCapability::OnSendingPDU(H245_AudioMode & pdu) const
{
  static const H245_AudioMode::Choices AudioTable[] = {
    H245_AudioMode::e_nonStandard,
    H245_AudioMode::e_g711Alaw64k,
    H245_AudioMode::e_g711Alaw56k,
    H245_AudioMode::e_g711Ulaw64k,
    H245_AudioMode::e_g711Ulaw56k,
    H245_AudioMode::e_g722_64k,
    H245_AudioMode::e_g722_56k,
    H245_AudioMode::e_g722_48k,
    H245_AudioMode::e_g7231,
    H245_AudioMode::e_g728,
    H245_AudioMode::e_g729,
    H245_AudioMode::e_g729AnnexA,
    H245_AudioMode::e_is11172AudioMode,
    H245_AudioMode::e_is13818AudioMode,
    H245_AudioMode::e_g729wAnnexB,
    H245_AudioMode::e_g729AnnexAwAnnexB,
    H245_AudioMode::e_g7231AnnexCMode,
    H245_AudioMode::e_gsmFullRate,
    H245_AudioMode::e_gsmHalfRate,
    H245_AudioMode::e_gsmEnhancedFullRate,
    H245_AudioMode::e_genericAudioMode,
    H245_AudioMode::e_g729Extensions
  };

  unsigned subType = GetSubType();
  if (subType >= PARRAYSIZE(AudioTable))
    return FALSE;

  pdu.SetTag(AudioTable[subType]);
  return TRUE;
}


BOOL H323AudioCapability::OnReceivedPDU(const H245_Capability & cap)
{
  H323Capability::OnReceivedPDU(cap);

  if (cap.GetTag() != H245_Capability::e_receiveAudioCapability &&
    cap.GetTag() != H245_Capability::e_receiveAndTransmitAudioCapability)
    return FALSE;

  unsigned packetSize = txFramesInPacket;
  if (!OnReceivedPDU((const H245_AudioCapability &)cap, packetSize))
    return FALSE;

  // Clamp our transmit size to maximum allowed
  if (txFramesInPacket > packetSize) {
    PTRACE(4, "H323\tCapability tx frames reduced from "
           << txFramesInPacket << " to " << packetSize);
    txFramesInPacket = packetSize;
  }
  else {
    PTRACE(4, "H323\tCapability tx frames left at "
           << txFramesInPacket << " as remote allows " << packetSize);
  }

  return TRUE;
}


BOOL H323AudioCapability::OnReceivedPDU(const H245_DataType & dataType, BOOL receiver)
{
  if (dataType.GetTag() != H245_DataType::e_audioData)
    return FALSE;

  unsigned & xFramesInPacket = receiver ? rxFramesInPacket : txFramesInPacket;
  unsigned packetSize = xFramesInPacket;
  if (!OnReceivedPDU((const H245_AudioCapability &)dataType, packetSize))
    return FALSE;

  // Clamp our transmit size to maximum allowed
  if (xFramesInPacket > packetSize) {
    PTRACE(4, "H323\tCapability " << (receiver ? 'r' : 't') << "x frames reduced from "
           << xFramesInPacket << " to " << packetSize);
    xFramesInPacket = packetSize;
  }
  else {
    PTRACE(4, "H323\tCapability " << (receiver ? 'r' : 't') << "x frames left at "
           << xFramesInPacket << " as remote allows " << packetSize);
  }

  return TRUE;
}


BOOL H323AudioCapability::OnReceivedPDU(const H245_AudioCapability & pdu,
                                        unsigned & packetSize)
{
  if (pdu.GetTag() != GetSubType())
    return FALSE;

  const PASN_Integer & value = pdu;

  // Get the maximum number of frames
  packetSize = value;
  return TRUE;
}


/////////////////////////////////////////////////////////////////////////////

H323NonStandardAudioCapability::H323NonStandardAudioCapability(const OpalMediaFormat & fmt,
                                                               unsigned max,
                                                               unsigned desired,
                                                               H323EndPoint & endpoint,
                                                               const BYTE * fixedData,
                                                               PINDEX dataSize,
                                                               PINDEX offset,
                                                               PINDEX length)
  : H323AudioCapability(fmt, max, desired),
    H323NonStandardCapabilityInfo(endpoint,
                                  fixedData != NULL ? fixedData : (const BYTE *)(const char *)fmt,
                                  fixedData != NULL ? dataSize : fmt.GetLength(),
                                  offset,
                                  length)
{
}


H323NonStandardAudioCapability::H323NonStandardAudioCapability(const OpalMediaFormat & fmt,
                                                               unsigned max,
                                                               unsigned desired,
                                                               const PString & oid,
                                                               const BYTE * fixedData,
                                                               PINDEX dataSize,
                                                               PINDEX offset,
                                                               PINDEX length)
  : H323AudioCapability(fmt, max, desired),
    H323NonStandardCapabilityInfo(oid,
                                  fixedData != NULL ? fixedData : (const BYTE *)(const char *)fmt,
                                  fixedData != NULL ? dataSize : fmt.GetLength(),
                                  offset,
                                  length)
{
}


H323NonStandardAudioCapability::H323NonStandardAudioCapability(const OpalMediaFormat & fmt,
                                                               unsigned max,
                                                               unsigned desired,
                                                               BYTE country,
                                                               BYTE extension,
                                                               WORD maufacturer,
                                                               const BYTE * fixedData,
                                                               PINDEX dataSize,
                                                               PINDEX offset,
                                                               PINDEX length)
  : H323AudioCapability(fmt, max, desired),
    H323NonStandardCapabilityInfo(country,
                                  extension,
                                  maufacturer,
                                  fixedData != NULL ? fixedData : (const BYTE *)(const char *)fmt,
                                  fixedData != NULL ? dataSize : fmt.GetLength(),
                                  offset,
                                  length)
{
}


PObject::Comparison H323NonStandardAudioCapability::Compare(const PObject & obj) const
{
  if (!obj.IsDescendant(H323NonStandardAudioCapability::Class()))
    return PObject::LessThan;

  return CompareInfo((const H323NonStandardAudioCapability &)obj);
}


unsigned H323NonStandardAudioCapability::GetSubType() const
{
  return H245_AudioCapability::e_nonStandard;
}


BOOL H323NonStandardAudioCapability::OnSendingPDU(H245_AudioCapability & pdu,
                                                  unsigned) const
{
  return OnSendingNonStandardPDU(pdu, H245_AudioCapability::e_nonStandard);
}


BOOL H323NonStandardAudioCapability::OnReceivedPDU(const H245_AudioCapability & pdu,
                                                   unsigned &)
{
  return OnReceivedNonStandardPDU(pdu, H245_AudioCapability::e_nonStandard);
}


BOOL H323NonStandardAudioCapability::IsNonStandardMatch(const H245_NonStandardParameter & param) const
{
  return CompareParam(param) == EqualTo && CompareData(param.m_data) == EqualTo;
}


/////////////////////////////////////////////////////////////////////////////

H323VideoCapability::H323VideoCapability(const OpalMediaFormat & fmt)
  : H323RealTimeCapability(fmt)
{
}


H323Capability::MainTypes H323VideoCapability::GetMainType() const
{
  return e_Video;
}


BOOL H323VideoCapability::OnSendingPDU(H245_Capability & cap) const
{
  cap.SetTag(H245_Capability::e_receiveVideoCapability);
  return OnSendingPDU((H245_VideoCapability &)cap);
}


BOOL H323VideoCapability::OnSendingPDU(H245_DataType & dataType) const
{
  dataType.SetTag(H245_DataType::e_videoData);
  return OnSendingPDU((H245_VideoCapability &)dataType);
}


BOOL H323VideoCapability::OnSendingPDU(H245_ModeElement & mode) const
{
  mode.m_type.SetTag(H245_ModeElement_type::e_videoMode);
  return OnSendingPDU((H245_VideoMode &)mode.m_type);
}


BOOL H323VideoCapability::OnReceivedPDU(const H245_Capability & cap)
{
  H323Capability::OnReceivedPDU(cap);

  if (cap.GetTag() != H245_Capability::e_receiveVideoCapability &&
      cap.GetTag() != H245_Capability::e_receiveAndTransmitVideoCapability)
    return FALSE;

  return OnReceivedPDU((const H245_VideoCapability &)cap);
}


BOOL H323VideoCapability::OnReceivedPDU(const H245_DataType & dataType, BOOL)
{
  if (dataType.GetTag() != H245_DataType::e_videoData)
    return FALSE;

  return OnReceivedPDU((const H245_VideoCapability &)dataType);
}


unsigned H323VideoCapability::GetDefaultSessionID() const
{
  return OpalMediaFormat::DefaultVideoSessionID;
}


/////////////////////////////////////////////////////////////////////////////

H323NonStandardVideoCapability::H323NonStandardVideoCapability(const OpalMediaFormat & fmt,
                                                               H323EndPoint & endpoint,
                                                               const BYTE * fixedData,
                                                               PINDEX dataSize,
                                                               PINDEX offset,
                                                               PINDEX length)
  : H323VideoCapability(fmt),
    H323NonStandardCapabilityInfo(endpoint,
                                  fixedData != NULL ? fixedData : (const BYTE *)(const char *)fmt,
                                  fixedData != NULL ? dataSize : fmt.GetLength(),
                                  offset,
                                  length)
{
}


H323NonStandardVideoCapability::H323NonStandardVideoCapability(const OpalMediaFormat & fmt,
                                                               const PString & oid,
                                                               const BYTE * fixedData,
                                                               PINDEX dataSize,
                                                               PINDEX offset,
                                                               PINDEX length)
  : H323VideoCapability(fmt),
    H323NonStandardCapabilityInfo(oid,
                                  fixedData != NULL ? fixedData : (const BYTE *)(const char *)fmt,
                                  fixedData != NULL ? dataSize : fmt.GetLength(),
                                  offset,
                                  length)
{
}


H323NonStandardVideoCapability::H323NonStandardVideoCapability(const OpalMediaFormat & fmt,
                                                               BYTE country,
                                                               BYTE extension,
                                                               WORD maufacturer,
                                                               const BYTE * fixedData,
                                                               PINDEX dataSize,
                                                               PINDEX offset,
                                                               PINDEX length)
  : H323VideoCapability(fmt),
    H323NonStandardCapabilityInfo(country,
                                  extension,
                                  maufacturer,
                                  fixedData != NULL ? fixedData : (const BYTE *)(const char *)fmt,
                                  fixedData != NULL ? dataSize : fmt.GetLength(),
                                  offset,
                                  length)
{
}


PObject::Comparison H323NonStandardVideoCapability::Compare(const PObject & obj) const
{
  if (!obj.IsDescendant(H323NonStandardVideoCapability::Class()))
    return PObject::LessThan;

  return CompareInfo((const H323NonStandardVideoCapability &)obj);
}


unsigned H323NonStandardVideoCapability::GetSubType() const
{
  return H245_VideoCapability::e_nonStandard;
}


BOOL H323NonStandardVideoCapability::OnSendingPDU(H245_VideoCapability & pdu) const
{
  return OnSendingNonStandardPDU(pdu, H245_VideoCapability::e_nonStandard);
}


BOOL H323NonStandardVideoCapability::OnReceivedPDU(const H245_VideoCapability & pdu)
{
  return OnReceivedNonStandardPDU(pdu, H245_VideoCapability::e_nonStandard);
}


BOOL H323NonStandardVideoCapability::IsNonStandardMatch(const H245_NonStandardParameter & param) const
{
  return CompareParam(param) == EqualTo && CompareData(param.m_data) == EqualTo;
}


/////////////////////////////////////////////////////////////////////////////

H323Capability::MainTypes H323DataCapability::GetMainType() const
{
  return e_Data;
}


unsigned H323DataCapability::GetDefaultSessionID() const
{
  return 3;
}


BOOL H323DataCapability::OnSendingPDU(H245_Capability & cap) const
{
  cap.SetTag(H245_Capability::e_receiveAndTransmitDataApplicationCapability);
  return OnSendingPDU((H245_DataApplicationCapability &)cap);
}


BOOL H323DataCapability::OnSendingPDU(H245_DataType & dataType) const
{
  dataType.SetTag(H245_DataType::e_data);
  return OnSendingPDU((H245_DataApplicationCapability &)dataType);
}


BOOL H323DataCapability::OnSendingPDU(H245_ModeElement & mode) const
{
  mode.m_type.SetTag(H245_ModeElement_type::e_dataMode);
  return OnSendingPDU((H245_DataMode &)mode.m_type);
}


BOOL H323DataCapability::OnReceivedPDU(const H245_Capability & cap)
{
  H323Capability::OnReceivedPDU(cap);

  if (cap.GetTag() != H245_Capability::e_receiveDataApplicationCapability &&
      cap.GetTag() != H245_Capability::e_receiveAndTransmitDataApplicationCapability)
    return FALSE;

  return OnReceivedPDU((const H245_DataApplicationCapability &)cap);
}


BOOL H323DataCapability::OnReceivedPDU(const H245_DataType & dataType, BOOL)
{
  if (dataType.GetTag() != H245_DataType::e_data)
    return FALSE;

  return OnReceivedPDU((const H245_DataApplicationCapability &)dataType);
}


/////////////////////////////////////////////////////////////////////////////

H323NonStandardDataCapability::H323NonStandardDataCapability(const OpalMediaFormat & fmt,
                                                             H323EndPoint & endpoint,
                                                             const BYTE * fixedData,
                                                             PINDEX dataSize,
                                                             PINDEX offset,
                                                             PINDEX length)
  : H323DataCapability(fmt),
    H323NonStandardCapabilityInfo(endpoint,
                                  fixedData != NULL ? fixedData : (const BYTE *)(const char *)fmt,
                                  fixedData != NULL ? dataSize : fmt.GetLength(),
                                  offset,
                                  length)
{
}


H323NonStandardDataCapability::H323NonStandardDataCapability(const OpalMediaFormat & fmt,
                                                             const PString & oid,
                                                             const BYTE * fixedData,
                                                             PINDEX dataSize,
                                                             PINDEX offset,
                                                             PINDEX length)
  : H323DataCapability(fmt),
    H323NonStandardCapabilityInfo(oid,
                                  fixedData != NULL ? fixedData : (const BYTE *)(const char *)fmt,
                                  fixedData != NULL ? dataSize : fmt.GetLength(),
                                  offset,
                                  length)
{
}


H323NonStandardDataCapability::H323NonStandardDataCapability(const OpalMediaFormat & fmt,
                                                             BYTE country,
                                                             BYTE extension,
                                                             WORD maufacturer,
                                                             const BYTE * fixedData,
                                                             PINDEX dataSize,
                                                             PINDEX offset,
                                                             PINDEX length)
  : H323DataCapability(fmt),
    H323NonStandardCapabilityInfo(country,
                                  extension,
                                  maufacturer,
                                  fixedData != NULL ? fixedData : (const BYTE *)(const char *)fmt,
                                  fixedData != NULL ? dataSize : fmt.GetLength(),
                                  offset,
                                  length)
{
}


PObject::Comparison H323NonStandardDataCapability::Compare(const PObject & obj) const
{
  if (!obj.IsDescendant(H323NonStandardDataCapability::Class()))
    return PObject::LessThan;

  return CompareInfo((const H323NonStandardDataCapability &)obj);
}


unsigned H323NonStandardDataCapability::GetSubType() const
{
  return H245_DataApplicationCapability_application::e_nonStandard;
}


BOOL H323NonStandardDataCapability::OnSendingPDU(H245_DataApplicationCapability & pdu) const
{
  return OnSendingNonStandardPDU(pdu.m_application, H245_DataApplicationCapability_application::e_nonStandard);
}


BOOL H323NonStandardDataCapability::OnReceivedPDU(const H245_DataApplicationCapability & pdu)
{
  return OnReceivedNonStandardPDU(pdu.m_application, H245_DataApplicationCapability_application::e_nonStandard);
}


BOOL H323NonStandardDataCapability::IsNonStandardMatch(const H245_NonStandardParameter & param) const
{
  return CompareParam(param) == EqualTo && CompareData(param.m_data) == EqualTo;
}


/////////////////////////////////////////////////////////////////////////////

static const char * const G711Name[2][2] = {
  { OPAL_G711_ALAW_64K, "G.711-ALaw-56k" },
  { OPAL_G711_ULAW_64K, "G.711-uLaw-56k" }
};

H323_G711Capability::H323_G711Capability(Mode m, Speed s)
  : H323AudioCapability(G711Name[m][s], 240, 30) // 240ms max, 30ms desired
{
  mode = m;
  speed = s;
}


PObject * H323_G711Capability::Clone() const
{
  return new H323_G711Capability(*this);
}


unsigned H323_G711Capability::GetSubType() const
{
  static const unsigned G711SubType[2][2] = {
    { H245_AudioCapability::e_g711Alaw64k, H245_AudioCapability::e_g711Alaw56k },
    { H245_AudioCapability::e_g711Ulaw64k, H245_AudioCapability::e_g711Ulaw56k }
  };
  return G711SubType[mode][speed];
}


/////////////////////////////////////////////////////////////////////////////

H323_G728Capability::H323_G728Capability()
  : H323AudioCapability(OpalG728, 75, 15) // 300ms max, 60ms desired
{
}


PObject * H323_G728Capability::Clone() const
{
  return new H323_G728Capability(*this);
}


unsigned H323_G728Capability::GetSubType() const
{
  return H245_AudioCapability::e_g728;
}


/////////////////////////////////////////////////////////////////////////////

static const char * const G729Name[4] = {
  OPAL_G729,
  OPAL_G729A,
  OPAL_G729B,
  OPAL_G729AB
};

H323_G729Capability::H323_G729Capability(Mode m)
  : H323AudioCapability(G729Name[m], 30,  8) // 300ms max, 80ms desired
{
  mode = m;
}


PObject * H323_G729Capability::Clone() const
{
  return new H323_G729Capability(*this);
}


unsigned H323_G729Capability::GetSubType() const
{
  static const unsigned G729SubType[4] = {
    H245_AudioCapability::e_g729,
    H245_AudioCapability::e_g729AnnexA,
    H245_AudioCapability::e_g729wAnnexB,
    H245_AudioCapability::e_g729AnnexAwAnnexB
  };
  return G729SubType[mode];
}


/////////////////////////////////////////////////////////////////////////////

H323_G7231Capability::H323_G7231Capability(BOOL sid)
  : H323AudioCapability(OpalG7231_6k3, 10, 3) // 300ms max, 90ms desired
{
  allowSIDFrames = sid;
}


PObject * H323_G7231Capability::Clone() const
{
  return new H323_G7231Capability(*this);
}


unsigned H323_G7231Capability::GetSubType() const
{
  return H245_AudioCapability::e_g7231;
}


BOOL H323_G7231Capability::OnSendingPDU(H245_AudioCapability & pdu,
                                        unsigned packetSize) const
{
  pdu.SetTag(H245_AudioCapability::e_g7231);
  H245_AudioCapability_g7231 & g7231 = pdu;
  g7231.m_maxAl_sduAudioFrames = packetSize;
  g7231.m_silenceSuppression = allowSIDFrames;
  return TRUE;
}


BOOL H323_G7231Capability::OnReceivedPDU(const H245_AudioCapability & pdu,
                                         unsigned & packetSize)
{
  if (pdu.GetTag() != H245_AudioCapability::e_g7231)
    return FALSE;

  const H245_AudioCapability_g7231 & g7231 = pdu;
  packetSize = g7231.m_maxAl_sduAudioFrames;
  allowSIDFrames = g7231.m_silenceSuppression;
  return TRUE;
}


///////////////////////////////////////////////////////////////////////////////

H323_GSM0610Capability::H323_GSM0610Capability()
  : H323AudioCapability(OpalGSM0610, 7, 4) // 140ms max, 80ms desired
{
}


PObject * H323_GSM0610Capability::Clone() const
{
  return new H323_GSM0610Capability(*this);
}


unsigned H323_GSM0610Capability::GetSubType() const
{
  return H245_AudioCapability::e_gsmFullRate;
}


void H323_GSM0610Capability::SetTxFramesInPacket(unsigned frames)
{
  if (frames > 7)
    txFramesInPacket = 7;
  else
    H323AudioCapability::SetTxFramesInPacket(frames);
}


BOOL H323_GSM0610Capability::OnSendingPDU(H245_AudioCapability & cap,
                                          unsigned packetSize) const
{
  cap.SetTag(H245_AudioCapability::e_gsmFullRate);

  H245_GSMAudioCapability & gsm = cap;
  gsm.m_audioUnitSize = packetSize*33;
  return TRUE;
}


BOOL H323_GSM0610Capability::OnReceivedPDU(const H245_AudioCapability & cap,
                                           unsigned & packetSize)
{
  if (cap.GetTag() != H245_AudioCapability::e_gsmFullRate)
    return FALSE;

  const H245_GSMAudioCapability & gsm = cap;
  packetSize = gsm.m_audioUnitSize/33;
  if (packetSize == 0)
    packetSize = 1;
  return TRUE;
}


/////////////////////////////////////////////////////////////////////////////

#define USERINPUT_BASICSTRING "UserInput/basicString"
static OpalMediaFormat UserInput_basicString(USERINPUT_BASICSTRING, 0, RTP_DataFrame::MaxPayloadType, FALSE, 1);

#define USERINPUT_IA5STRING "UserInput/iA5String"
static OpalMediaFormat UserInput_iA5String(USERINPUT_IA5STRING, 0, RTP_DataFrame::MaxPayloadType, FALSE, 1);

#define USERINPUT_GENERALSTRING "UserInput/generalString"
static OpalMediaFormat UserInput_generalString(USERINPUT_GENERALSTRING, 0, RTP_DataFrame::MaxPayloadType, FALSE, 1);

#define USERINPUT_DTMF "UserInput/dtmf"
static OpalMediaFormat UserInput_dtmf(USERINPUT_DTMF, 0, RTP_DataFrame::MaxPayloadType, FALSE, 1);

#define USERINPUT_HOOKFLASH "UserInput/hookflash"
static OpalMediaFormat UserInput_hookflash(USERINPUT_HOOKFLASH, 0, RTP_DataFrame::MaxPayloadType, FALSE, 1);

static const char * const UserInputName[6] = {
  "",
  USERINPUT_BASICSTRING,
  USERINPUT_IA5STRING,
  USERINPUT_GENERALSTRING,
  USERINPUT_DTMF,
  USERINPUT_HOOKFLASH
};

H323_UserInputCapability::H323_UserInputCapability(unsigned _subType)
  : H323Capability(_subType < PARRAYSIZE(UserInputName) ? UserInputName[_subType] : "")
{
  subType = _subType;
}


PObject * H323_UserInputCapability::Clone() const
{
  return new H323_UserInputCapability(*this);
}


H323Capability::MainTypes H323_UserInputCapability::GetMainType() const
{
  return e_UserInput;
}


unsigned  H323_UserInputCapability::GetSubType()  const
{
  return subType;
}


H323Channel * H323_UserInputCapability::CreateChannel(H323Connection &,
                                                      H323Channel::Directions,
                                                      unsigned,
                                                      const H245_H2250LogicalChannelParameters *) const
{
  PTRACE(1, "Codec\tCannot create UserInputCapability channel");
  return NULL;
}


BOOL H323_UserInputCapability::OnSendingPDU(H245_Capability & pdu) const
{
  pdu.SetTag(H245_Capability::e_receiveUserInputCapability);
  H245_UserInputCapability & ui = pdu;
  ui.SetTag(subType);
  return TRUE;
}


BOOL H323_UserInputCapability::OnSendingPDU(H245_DataType &) const
{
  PTRACE(1, "Codec\tCannot have UserInputCapability in DataType");
  return FALSE;
}


BOOL H323_UserInputCapability::OnSendingPDU(H245_ModeElement &) const
{
  PTRACE(1, "Codec\tCannot have UserInputCapability in ModeElement");
  return FALSE;
}


BOOL H323_UserInputCapability::OnReceivedPDU(const H245_Capability & pdu)
{
  H323Capability::OnReceivedPDU(pdu);

  if (pdu.GetTag() != H245_Capability::e_receiveUserInputCapability)
    return FALSE;

  const H245_UserInputCapability & ui = pdu;
  subType = ui.GetTag();
  return TRUE;
}


BOOL H323_UserInputCapability::OnReceivedPDU(const H245_DataType &, BOOL)
{
  PTRACE(1, "Codec\tCannot have UserInputCapability in DataType");
  return FALSE;
}


void H323_UserInputCapability::AddAllCapabilities(H323Capabilities & capabilities,
                                                  PINDEX descriptorNum,
                                                  PINDEX simultaneous)
{
  PINDEX num = capabilities.SetCapability(descriptorNum, simultaneous, new H323_UserInputCapability(H245_UserInputCapability::e_hookflash));
  if (descriptorNum == P_MAX_INDEX) {
    descriptorNum = num;
    simultaneous = P_MAX_INDEX;
  }
  else if (simultaneous == P_MAX_INDEX)
    simultaneous = num+1;

  num = capabilities.SetCapability(descriptorNum, simultaneous, new H323_UserInputCapability(H245_UserInputCapability::e_basicString));
  if (simultaneous == P_MAX_INDEX)
    simultaneous = num;

  capabilities.SetCapability(descriptorNum, simultaneous, new H323_UserInputCapability(H245_UserInputCapability::e_dtmf));
}


/////////////////////////////////////////////////////////////////////////////

BOOL H323SimultaneousCapabilities::SetSize(PINDEX newSize)
{
  PINDEX oldSize = GetSize();

  if (!H323CapabilitiesListArray::SetSize(newSize))
    return FALSE;

  while (oldSize < newSize) {
    H323CapabilitiesList * list = new H323CapabilitiesList;
    // The lowest level list should not delete codecs on destruction
    list->DisallowDeleteObjects();
    SetAt(oldSize++, list);
  }

  return TRUE;
}


BOOL H323CapabilitiesSet::SetSize(PINDEX newSize)
{
  PINDEX oldSize = GetSize();

  if (!H323CapabilitiesSetArray::SetSize(newSize))
    return FALSE;

  while (oldSize < newSize)
    SetAt(oldSize++, new H323SimultaneousCapabilities);

  return TRUE;
}


H323Capabilities::H323Capabilities()
{
}


H323Capabilities::H323Capabilities(const H323Connection & connection,
                                   const H245_TerminalCapabilitySet & pdu)
{
  // Decode out of the PDU, the list of known codecs.
  if (pdu.HasOptionalField(H245_TerminalCapabilitySet::e_capabilityTable)) {
    for (PINDEX i = 0; i < pdu.m_capabilityTable.GetSize(); i++) {
      if (pdu.m_capabilityTable[i].HasOptionalField(H245_CapabilityTableEntry::e_capability)) {
        H323Capability * capability = connection.GetLocalCapabilities().FindCapability(pdu.m_capabilityTable[i].m_capability);
        if (capability != NULL) {
          H323Capability * copy = (H323Capability *)capability->Clone();
          copy->SetCapabilityNumber(pdu.m_capabilityTable[i].m_capabilityTableEntryNumber);
          if (copy->OnReceivedPDU(pdu.m_capabilityTable[i].m_capability))
            table.Append(copy);
          else
            delete copy;
        }
      }
    }
  }

  PINDEX outerSize = pdu.m_capabilityDescriptors.GetSize();
  set.SetSize(outerSize);
  for (PINDEX outer = 0; outer < outerSize; outer++) {
    H245_CapabilityDescriptor & desc = pdu.m_capabilityDescriptors[outer];
    if (desc.HasOptionalField(H245_CapabilityDescriptor::e_simultaneousCapabilities)) {
      PINDEX middleSize = desc.m_simultaneousCapabilities.GetSize();
      set[outer].SetSize(middleSize);
      for (PINDEX middle = 0; middle < middleSize; middle++) {
        H245_AlternativeCapabilitySet & alt = desc.m_simultaneousCapabilities[middle];
        for (PINDEX inner = 0; inner < alt.GetSize(); inner++) {
          for (PINDEX cap = 0; cap < table.GetSize(); cap++) {
            if (table[cap].GetCapabilityNumber() == alt[inner]) {
              set[outer][middle].Append(&table[cap]);
              break;
            }
          }
        }
      }
    }
  }
}


H323Capabilities::H323Capabilities(const H323Capabilities & original)
{
  operator=(original);
}


H323Capabilities & H323Capabilities::operator=(const H323Capabilities & original)
{
  RemoveAll();

  for (PINDEX i = 0; i < original.GetSize(); i++)
    Copy(original[i]);

  set.SetSize(original.set.GetSize());
  for (PINDEX outer = 0; outer < original.set.GetSize(); outer++) {
    set[outer].SetSize(original.set[outer].GetSize());
    for (PINDEX middle = 0; middle < original.set[outer].GetSize(); middle++) {
      for (PINDEX inner = 0; inner < original.set[outer][middle].GetSize(); inner++)
        set[outer][middle].Append(FindCapability(original.set[outer][middle][inner].GetCapabilityNumber()));
    }
  }

  return *this;
}


void H323Capabilities::PrintOn(ostream & strm) const
{
  int indent = strm.precision()-1;
  for (PINDEX i = 0; i < table.GetSize(); i++)
    strm << setw(indent) << " " << table[i] << '\n';
}


PINDEX H323Capabilities::SetCapability(PINDEX descriptorNum,
                                       PINDEX simultaneousNum,
                                       H323Capability * capability)
{
  // Make sure capability has been added to table.
  Add(capability);

  BOOL newDescriptor = descriptorNum == P_MAX_INDEX;
  if (newDescriptor)
    descriptorNum = set.GetSize();

  // Make sure the outer array is big enough
  set.SetMinSize(descriptorNum+1);

  if (simultaneousNum == P_MAX_INDEX)
    simultaneousNum = set[descriptorNum].GetSize();

  // Make sure the middle array is big enough
  set[descriptorNum].SetMinSize(simultaneousNum+1);

  // Now we can put the new entry in.
  set[descriptorNum][simultaneousNum].Append(capability);
  return newDescriptor ? descriptorNum : simultaneousNum;
}


static BOOL MatchWildcard(const PCaselessString & str, const PStringArray & wildcard)
{
  PINDEX last = 0;
  for (PINDEX i = 0; i < wildcard.GetSize(); i++) {
    if (wildcard[i].IsEmpty())
      last = str.GetLength();
    else {
      PINDEX next = str.Find(wildcard[i], last);
      if (next == P_MAX_INDEX)
        return FALSE;
      last = next + wildcard[i].GetLength();
    }
  }

  return TRUE;
}


PINDEX H323Capabilities::AddAllCapabilities(H323EndPoint & ep,
                                            PINDEX descriptorNum,
                                            PINDEX simultaneous,
                                            const PString & name)
{
  PINDEX reply = P_MAX_INDEX;

  PStringArray wildcard = name.Tokenise('*', FALSE);

  H323CapabilityRegistration * reg = RegisteredCapabilitiesListHead;
  while (reg != NULL) {
    if (MatchWildcard(*reg, wildcard)) {
      PINDEX num = SetCapability(descriptorNum, simultaneous, reg->Create(ep));
      if (descriptorNum == P_MAX_INDEX) {
        reply = num;
        descriptorNum = num;
        simultaneous = P_MAX_INDEX;
      }
      else if (simultaneous == P_MAX_INDEX) {
        if (reply != P_MAX_INDEX)
          reply = num;
        simultaneous = num;
      }
    }
    reg = reg->link;
  }

  return reply;
}


static unsigned MergeCapabilityNumber(const H323CapabilitiesList & table,
                                      unsigned newCapabilityNumber)
{
  // Assign a unique number to the codec, check if the user wants a specific
  // value and start with that.
  if (newCapabilityNumber == 0)
    newCapabilityNumber = 1;

  PINDEX i = 0;
  while (i < table.GetSize()) {
    if (table[i].GetCapabilityNumber() != newCapabilityNumber)
      i++;
    else {
      // If it already in use, increment it
      newCapabilityNumber++;
      i = 0;
    }
  }

  return newCapabilityNumber;
}


void H323Capabilities::Add(H323Capability * capability)
{
  // See if already added, confuses things if you add the same instance twice
  if (table.GetObjectsIndex(capability) != P_MAX_INDEX)
    return;

  capability->SetCapabilityNumber(MergeCapabilityNumber(table, 1));
  table.Append(capability);

  PTRACE(2, "H323\tAdded capability: " << *capability);
}


H323Capability * H323Capabilities::Copy(const H323Capability & capability)
{
  H323Capability * newCapability = (H323Capability *)capability.Clone();
  newCapability->SetCapabilityNumber(MergeCapabilityNumber(table, capability.GetCapabilityNumber()));
  table.Append(newCapability);

  PTRACE(2, "H323\tAdded capability: " << *newCapability);
  return newCapability;
}


void H323Capabilities::Remove(H323Capability * capability)
{
  if (capability == NULL)
    return;

  unsigned capabilityNumber = capability->GetCapabilityNumber();

  for (PINDEX outer = 0; outer < set.GetSize(); outer++) {
    for (PINDEX middle = 0; middle < set[outer].GetSize(); middle++) {
      for (PINDEX inner = 0; inner < set[outer][middle].GetSize(); inner++) {
        if (set[outer][middle][inner].GetCapabilityNumber() == capabilityNumber) {
          set[outer][middle].RemoveAt(inner);
          break;
        }
      }
      if (set[outer][middle].GetSize() == 0)
        set[outer].RemoveAt(middle);
    }
    if (set[outer].GetSize() == 0)
      set.RemoveAt(outer);
  }

  table.Remove(capability);
}


void H323Capabilities::Remove(const PString & codecName)
{
  H323Capability * cap = FindCapability(codecName);
  while (cap != NULL) {
    Remove(cap);
    cap = FindCapability(codecName);
  }
}


void H323Capabilities::Remove(const PStringArray & codecNames)
{
  for (PINDEX i = 0; i < codecNames.GetSize(); i++)
    Remove(codecNames[i]);
}


void H323Capabilities::RemoveAll()
{
  table.RemoveAll();
  set.RemoveAll();
}


H323Capability * H323Capabilities::FindCapability(unsigned capabilityNumber) const
{
  PTRACE(3, "H323\tFindCapability: " << capabilityNumber);

  for (PINDEX i = 0; i < table.GetSize(); i++) {
    if (table[i].GetCapabilityNumber() == capabilityNumber) {
      PTRACE(2, "H323\tFound capability: " << table[i]);
      return &table[i];
    }
  }

  return NULL;
}


H323Capability * H323Capabilities::FindCapability(const PString & formatName,
                              H323Capability::CapabilityDirection direction) const
{
  PTRACE(3, "H323\tFindCapability: \"" << formatName << '"');

  PStringArray wildcard = formatName.Tokenise('*', FALSE);

  for (PINDEX i = 0; i < table.GetSize(); i++) {
    PCaselessString str = table[i].GetMediaFormat();
    if (MatchWildcard(str, wildcard) &&
          (direction == H323Capability::e_Unknown ||
           table[i].GetCapabilityDirection() == direction)) {
      PTRACE(2, "H323\tFound capability: " << table[i]);
      return &table[i];
    }
  }

  return NULL;
}


H323Capability * H323Capabilities::FindCapability(
                              H323Capability::CapabilityDirection direction) const
{
  PTRACE(1, "H323\tFindCapability: \"" << direction << '"');

  for (PINDEX i = 0; i < table.GetSize(); i++) {
    if (table[i].GetCapabilityDirection() == direction) {
      PTRACE(1, "H323\tFound capability: " << table[i]);
      return &table[i];
    }
  }

  return NULL;
}


H323Capability * H323Capabilities::FindCapability(const H323Capability & capability) const
{
  PTRACE(3, "H323\tFindCapability: " << capability);

  for (PINDEX i = 0; i < table.GetSize(); i++) {
    if (table[i] == capability) {
      PTRACE(2, "H323\tFound capability: " << table[i]);
      return &table[i];
    }
  }

  return NULL;
}


H323Capability * H323Capabilities::FindCapability(const H245_Capability & cap) const
{
  PTRACE(4, "H323\tFindCapability: " << cap.GetTagName());

  switch (cap.GetTag()) {
    case H245_Capability::e_receiveAudioCapability :
    case H245_Capability::e_transmitAudioCapability :
    case H245_Capability::e_receiveAndTransmitAudioCapability :
    {
      const H245_AudioCapability & audio = cap;
      return FindCapability(H323Capability::e_Audio, audio, H245_AudioCapability::e_nonStandard);
    }

    case H245_Capability::e_receiveVideoCapability :
    case H245_Capability::e_transmitVideoCapability :
    case H245_Capability::e_receiveAndTransmitVideoCapability :
    {
      const H245_VideoCapability & video = cap;
      return FindCapability(H323Capability::e_Video, video, H245_VideoCapability::e_nonStandard);
    }

    case H245_Capability::e_receiveDataApplicationCapability :
    case H245_Capability::e_transmitDataApplicationCapability :
    case H245_Capability::e_receiveAndTransmitDataApplicationCapability :
    {
      const H245_DataApplicationCapability & data = cap;
      return FindCapability(H323Capability::e_Data, data.m_application, H245_DataApplicationCapability_application::e_nonStandard);
    }

    case H245_Capability::e_receiveUserInputCapability :
    case H245_Capability::e_transmitUserInputCapability :
    case H245_Capability::e_receiveAndTransmitUserInputCapability :
    {
      const H245_UserInputCapability & ui = cap;
      return FindCapability(H323Capability::e_UserInput, ui, H245_UserInputCapability::e_nonStandard);
    }

    default :
      break;
  }

  return NULL;
}


H323Capability * H323Capabilities::FindCapability(const H245_DataType & dataType) const
{
  PTRACE(4, "H323\tFindCapability: " << dataType.GetTagName());

  switch (dataType.GetTag()) {
    case H245_DataType::e_audioData :
      {
        const H245_AudioCapability & audio = dataType;
        return FindCapability(H323Capability::e_Audio, audio, H245_AudioCapability::e_nonStandard);
      }

    case H245_DataType::e_videoData :
      {
        const H245_VideoCapability & video = dataType;
        return FindCapability(H323Capability::e_Video, video, H245_VideoCapability::e_nonStandard);
      }

    case H245_DataType::e_data :
      {
        const H245_DataApplicationCapability & data = dataType;
        return FindCapability(H323Capability::e_Data, data.m_application, H245_DataApplicationCapability_application::e_nonStandard);
      }

    default :
      break;
  }

  return NULL;
}


H323Capability * H323Capabilities::FindCapability(const H245_ModeElement & modeElement) const
{
  PTRACE(4, "H323\tFindCapability: " << modeElement.m_type.GetTagName());

  switch (modeElement.m_type.GetTag()) {
    case H245_ModeElement_type::e_audioMode :
      {
        const H245_AudioMode & audio = modeElement.m_type;
        return FindCapability(H323Capability::e_Audio, audio, H245_AudioCapability::e_nonStandard);
      }

    case H245_ModeElement_type::e_videoMode :
      {
        const H245_VideoMode & video = modeElement.m_type;
        return FindCapability(H323Capability::e_Video, video, H245_VideoCapability::e_nonStandard);
      }

    case H245_ModeElement_type::e_dataMode :
      {
        const H245_DataMode & data = modeElement.m_type;
        return FindCapability(H323Capability::e_Data, data.m_application, H245_DataApplicationCapability_application::e_nonStandard);
      }

    default :
      break;
  }

  return NULL;
}


H323Capability * H323Capabilities::FindCapability(H323Capability::MainTypes mainType,
                                                  const PASN_Choice & subTypePDU,
                                                  unsigned nonStandardTag) const
{
  if (subTypePDU.GetTag() != nonStandardTag)
    return FindCapability(mainType, subTypePDU.GetTag());

  PTRACE(3, "H323\tFindCapability: " << mainType << " nonStandard");

  for (PINDEX i = 0; i < table.GetSize(); i++) {
    H323Capability & capability = table[i];
    if (capability.GetMainType() == mainType &&
        capability.GetSubType() == nonStandardTag &&
        capability.IsNonStandardMatch((const H245_NonStandardParameter &)subTypePDU.GetObject())) {
      PTRACE(2, "H323\tFound capability: " << capability);
      return &capability;
    }
  }

  return NULL;
}


H323Capability * H323Capabilities::FindCapability(H323Capability::MainTypes mainType,
                                                  unsigned subType) const
{
  PTRACE(3, "H323\tFindCapability: " << mainType << " subtype=" << subType);

  for (PINDEX i = 0; i < table.GetSize(); i++) {
    H323Capability & capability = table[i];
    if (capability.GetMainType() == mainType &&
                        (subType == UINT_MAX || capability.GetSubType() == subType)) {
      PTRACE(2, "H323\tFound capability: " << capability);
      return &capability;
    }
  }

  return NULL;
}


void H323Capabilities::BuildPDU(H245_TerminalCapabilitySet & pdu) const
{
  PINDEX tableSize = table.GetSize();
  PINDEX setSize = set.GetSize();
  PAssert((tableSize > 0) == (setSize > 0), PLogicError);
  if (tableSize == 0 || setSize == 0)
    return;

  // Set the table of capabilities
  pdu.IncludeOptionalField(H245_TerminalCapabilitySet::e_capabilityTable);

  pdu.m_capabilityTable.SetSize(tableSize);
  for (PINDEX i = 0; i < tableSize; i++) {
    pdu.m_capabilityTable[i].m_capabilityTableEntryNumber = table[i].GetCapabilityNumber();
    pdu.m_capabilityTable[i].IncludeOptionalField(H245_CapabilityTableEntry::e_capability);
    table[i].OnSendingPDU(pdu.m_capabilityTable[i].m_capability);
  }

  // Set the sets of compatible capabilities
  pdu.IncludeOptionalField(H245_TerminalCapabilitySet::e_capabilityDescriptors);

  pdu.m_capabilityDescriptors.SetSize(setSize);
  for (PINDEX outer = 0; outer < setSize; outer++) {
    H245_CapabilityDescriptor & desc = pdu.m_capabilityDescriptors[outer];
    desc.m_capabilityDescriptorNumber = (unsigned)(outer + 1);
    desc.IncludeOptionalField(H245_CapabilityDescriptor::e_simultaneousCapabilities);
    PINDEX middleSize = set[outer].GetSize();
    desc.m_simultaneousCapabilities.SetSize(middleSize);
    for (PINDEX middle = 0; middle < middleSize; middle++) {
      H245_AlternativeCapabilitySet & alt = desc.m_simultaneousCapabilities[middle];
      PINDEX innerSize = set[outer][middle].GetSize();
      alt.SetSize(innerSize);
      for (PINDEX inner = 0; inner < innerSize; inner++)
        alt[inner] = set[outer][middle][inner].GetCapabilityNumber();
    }
  }
}


BOOL H323Capabilities::Merge(const H323Capabilities & newCaps)
{
  // Add any new capabilities not already in set.
  PINDEX i;
  for (i = 0; i < newCaps.GetSize(); i++) {
    if (!FindCapability(newCaps[i]))
      Copy(newCaps[i]);
  }

  // This should merge instead of just replacing the set.
  set = newCaps.set;

  PTRACE(3, "H245\tReceived capability set, is "
                 << (table.IsEmpty() ? "rejected" : "accepted"));
  return !table.IsEmpty();
}


void H323Capabilities::Reorder(const PStringArray & preferenceOrder)
{
  if (preferenceOrder.IsEmpty())
    return;

  table.DisallowDeleteObjects();

  PINDEX preference = 0;
  PINDEX base = 0;

  for (preference = 0; preference < preferenceOrder.GetSize(); preference++) {
    PStringArray wildcard = preferenceOrder[preference].Tokenise('*', FALSE);
    for (PINDEX idx = base; idx < table.GetSize(); idx++) {
      PCaselessString str = table[idx].GetMediaFormat();
      if (MatchWildcard(str, wildcard)) {
        if (idx != base)
          table.InsertAt(base, table.RemoveAt(idx));
        base++;
      }
    }
  }

  for (PINDEX outer = 0; outer < set.GetSize(); outer++) {
    for (PINDEX middle = 0; middle < set[outer].GetSize(); middle++) {
      H323CapabilitiesList & list = set[outer][middle];
      for (PINDEX idx = 0; idx < table.GetSize(); idx++) {
        for (PINDEX inner = 0; inner < list.GetSize(); inner++) {
          if (&table[idx] == &list[inner]) {
            list.Append(list.RemoveAt(inner));
            break;
          }
        }
      }
    }
  }

  table.AllowDeleteObjects();
}


BOOL H323Capabilities::IsAllowed(const H323Capability & capability)
{
  return IsAllowed(capability.GetCapabilityNumber());
}


BOOL H323Capabilities::IsAllowed(const unsigned a_capno)
{
  // Check that capno is actually in the set
  PINDEX outerSize = set.GetSize();
  for (PINDEX outer = 0; outer < outerSize; outer++) {
    PINDEX middleSize = set[outer].GetSize();
    for (PINDEX middle = 0; middle < middleSize; middle++) {
      PINDEX innerSize = set[outer][middle].GetSize();
      for (PINDEX inner = 0; inner < innerSize; inner++) {
        if (a_capno == set[outer][middle][inner].GetCapabilityNumber()) {
          return TRUE;
        }
      }
    }
  }
  return FALSE;
}


BOOL H323Capabilities::IsAllowed(const H323Capability & capability1,
                                 const H323Capability & capability2)
{
  return IsAllowed(capability1.GetCapabilityNumber(),
                   capability2.GetCapabilityNumber());
}


BOOL H323Capabilities::IsAllowed(const unsigned a_capno1, const unsigned a_capno2)
{
  if (a_capno1 == a_capno2) {
    PTRACE(1, "H323\tH323Capabilities::IsAllowed() capabilities are the same.");
    return TRUE;
  }

  PINDEX outerSize = set.GetSize();
  for (PINDEX outer = 0; outer < outerSize; outer++) {
    PINDEX middleSize = set[outer].GetSize();
    for (PINDEX middle = 0; middle < middleSize; middle++) {
      PINDEX innerSize = set[outer][middle].GetSize();
      for (PINDEX inner = 0; inner < innerSize; inner++) {
        if (a_capno1 == set[outer][middle][inner].GetCapabilityNumber()) {
          /* Now go searching for the other half... */
          for (PINDEX middle2 = 0; middle2 < middleSize; ++middle2) {
            if (middle != middle2) {
              PINDEX innerSize2 = set[outer][middle2].GetSize();
              for (PINDEX inner2 = 0; inner2 < innerSize2; ++inner2) {
                if (a_capno2 == set[outer][middle2][inner2].GetCapabilityNumber()) {
                  return TRUE;
                }
              }
            }
          }
        }
      }
    }
  }
  return FALSE;
}


OpalMediaFormatList H323Capabilities::GetMediaFormats() const
{
  OpalMediaFormatList formats;

  for (PINDEX i = 0; i < table.GetSize(); i++)
    formats += table[i].GetMediaFormat();

  return formats;
}


/////////////////////////////////////////////////////////////////////////////

H323CapabilityRegistration::H323CapabilityRegistration(const char * name)
  : PCaselessString(name)
{
  H323CapabilityRegistration * test = RegisteredCapabilitiesListHead;
  while (test != NULL) {
    if (*test == *this)
      return;
    test = test->link;
  }

  link = RegisteredCapabilitiesListHead;
  RegisteredCapabilitiesListHead = this;
}


/////////////////////////////////////////////////////////////////////////////
