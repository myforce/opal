/*
 * sdp.cxx
 *
 * Session Description Protocol support.
 *
 * Open Phone Abstraction Library (OPAL)
 *
 * Copyright (c) 2000 Equivalence Pty. Ltd.
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
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#include <ptlib.h>

#include <opal/buildopts.h>
#if OPAL_SIP

#ifdef __GNUC__
#pragma implementation "sdp.h"
#endif

#include <sip/sdp.h>

#include <ptlib/socket.h>
#include <opal/transports.h>


#define SIP_DEFAULT_SESSION_NAME    "Opal SIP Session"
#define SDP_MEDIA_TRANSPORT_RTPAVP  "RTP/AVP"

#define new PNEW

//
// uncommment this to output a=fmtp lines before a=rtpmap lines. Useful for testing
//
//#define FMTP_BEFORE_RTPMAP 1


/////////////////////////////////////////////////////////
//
//  the following functions bind the media type factory to the SDP format types
//

PString OpalMediaType::GetSDPFromFromMediaType(const OpalMediaType & type)
{
  OpalMediaTypeDefinition * defn = type.GetDefinition();
  if (defn == NULL)
    return PString();
  return defn->GetSDPType();
}

OpalMediaType OpalMediaType::GetMediaTypeFromSDP(const std::string & sdp)
{
  OpalMediaTypeFactory::KeyList_T mediaTypes = OpalMediaTypeFactory::GetKeyList();
  OpalMediaTypeFactory::KeyList_T::iterator r;
  for (r = mediaTypes.begin(); r != mediaTypes.end(); ++r) {
    if (OpalMediaType::GetDefinition(*r)->GetSDPType() == sdp)
      return OpalMediaType(*r);
  }

  return OpalMediaType();
}

SDPMediaDescription * OpalAudioMediaType::CreateSDPMediaDescription(const OpalTransportAddress & localAddress)
{
  return new SDPAudioMediaDescription(localAddress);
}

#if OPAL_VIDEO
SDPMediaDescription * OpalVideoMediaType::CreateSDPMediaDescription(const OpalTransportAddress & localAddress)
{
  return new SDPVideoMediaDescription(localAddress);
}
#endif


/////////////////////////////////////////////////////////

static OpalTransportAddress ParseConnectAddress(const PStringArray & tokens, PINDEX offset, WORD port = 0)
{
  if (tokens.GetSize() == offset+3) {
    if (tokens[offset] *= "IN") {
      if (
        (tokens[offset+1] *= "IP4")
#if P_HAS_IPV6
        || (tokens[offset+1] *= "IP6")
#endif
        ) {
        if (tokens[offset+2] == "255.255.255.255") {
          PTRACE(2, "SDP\tInvalid connection address 255.255.255.255 used, treating like HOLD request.");
        }
        else if (tokens[offset+2] == "0.0.0.0") {
          PTRACE(3, "SDP\tConnection address of 0.0.0.0 specified for HOLD request.");
        }
        else
          return OpalTransportAddress(tokens[offset+2], port, "udp");
      }
      else
      {
        PTRACE(1, "SDP\tConnect address has invalid address type \"" << tokens[offset+1] << '"');
      }
    }
    else {
      PTRACE(1, "SDP\tConnect address has invalid network \"" << tokens[offset] << '"');
    }
  }
  else {
    PTRACE(1, "SDP\tConnect address has invalid (" << tokens.GetSize() << ") elements");
  }

  return OpalTransportAddress();
}


static OpalTransportAddress ParseConnectAddress(const PString & str, WORD port = 0)
{
  PStringArray tokens = str.Tokenise(' ');
  return ParseConnectAddress(tokens, 0, port);
}


static PString GetConnectAddressString(const OpalTransportAddress & address)
{
  PStringStream str;

  PIPSocket::Address ip;
  if (!address.IsEmpty() && address.GetIpAddress(ip) && ip.IsValid())
    str << "IN IP" << ip.GetVersion() << ' ' << ip;
  else
    str << "IN IP4 0.0.0.0";

  return str;
}


/////////////////////////////////////////////////////////

SDPMediaFormat::SDPMediaFormat(RTP_DataFrame::PayloadTypes pt, const char * _name)
  : payloadType(pt)
  , clockRate(0)
  , encodingName(_name)
  , nteSet(PTrue)
  , nseSet(PTrue)
{
  if (encodingName == OpalRFC2833.GetEncodingName())
    AddNTEString("0-15,32-49");
#if OPAL_T38_CAPABILITY
  else if (encodingName == OpalCiscoNSE.GetEncodingName())
    AddNSEString("192,193");
#endif
}


SDPMediaFormat::SDPMediaFormat(const OpalMediaFormat & fmt,
                               RTP_DataFrame::PayloadTypes pt,
                               const char * nxeString)
  : mediaFormat(fmt)
  , payloadType(pt)
  , clockRate(fmt.GetClockRate())
  , encodingName(fmt.GetEncodingName())
  , nteSet(PTrue)
  , nseSet(PTrue)
{
  if (nxeString != NULL) {
    if (encodingName *= "nse")
      AddNSEString(nxeString);
    else
      AddNTEString(nxeString);
  }
  if (fmt.GetMediaType() == OpalMediaType::Audio()) 
    parameters = PString(PString::Unsigned, fmt.GetOptionInteger(OpalAudioFormat::ChannelsOption()));
}

void SDPMediaFormat::SetFMTP(const PString & str)
{
  if (str.IsEmpty())
    return;

  if (encodingName == OpalRFC2833.GetEncodingName()) {
    nteSet.RemoveAll();
    AddNTEString(str);
    return;
  }

#if OPAL_T38_CAPABILITY
  else if (encodingName == OpalCiscoNSE.GetEncodingName()) {
    nseSet.RemoveAll();
    AddNSEString(str);
    return;
  }
#endif

  fmtp = str;
  if (GetMediaFormat().IsEmpty()) // Use GetMediaFormat() to force creation of member
    return;

  mediaFormat.AddOption(new OpalMediaOptionString("RawFMTP", false, str), PTrue); // Save the 'fmtp=' line so it is available at the application level.

  // See if standard format OPT=VAL;OPT=VAL
  if (str.FindOneOf(";=") == P_MAX_INDEX) {
    // Nope, just save the whole string as is
    mediaFormat.SetOptionString("FMTP", str);
    return;
  }

  // guess at the seperator
  char sep = ';';
  if (str.Find(sep) == P_MAX_INDEX)
    sep = ' ';

  // Parse the string for option names and values OPT=VAL;OPT=VAL
  PINDEX sep1prev = 0;
  do {
    PINDEX sep1next = str.Find(sep, sep1prev);
    if (sep1next == P_MAX_INDEX)
      sep1next--; // Implicit assumption string is not a couple of gigabytes long ...

    PINDEX sep2pos = str.Find('=', sep1prev);
    if (sep2pos > sep1next)
      sep2pos = sep1next;

    PCaselessString key = str(sep1prev, sep2pos-1).Trim();
    if (key.IsEmpty()) {
      PTRACE(2, "SDP\tBadly formed FMTP parameter \"" << str << '"');
      break;
    }

    OpalMediaOption * option = mediaFormat.FindOption(key);
    if (option == NULL || key != option->GetFMTPName()) {
      for (PINDEX i = 0; i < mediaFormat.GetOptionCount(); i++) {
        if (key == mediaFormat.GetOption(i).GetFMTPName()) {
          option = const_cast<OpalMediaOption *>(&mediaFormat.GetOption(i));
          break;
        }
      }
    }
    if (option != NULL) {
      PString value = str(sep2pos+1, sep1next-1);
      if (dynamic_cast< OpalMediaOptionOctets * >(option) != NULL) {
        if (str.GetLength() % 2 != 0)
          value = value.Trim();
      } else {
        value = value.Trim();
        if (value.IsEmpty())
          value = "1"; // Assume it is a boolean
      }
      if (!option->FromString(value)) {
        PTRACE(2, "SDP\tCould not set FMTP parameter \"" << key << "\" to value \"" << value << '"');
      }
    }

    sep1prev = sep1next+1;
  } while (sep1prev != P_MAX_INDEX);
}


PString SDPMediaFormat::GetFMTP() const
{
  if (encodingName == OpalRFC2833.GetEncodingName())
    return GetNTEString();

#if OPAL_T38_CAPABILITY
  if (encodingName == OpalCiscoNSE.GetEncodingName())
    return GetNSEString();
#endif

  if (GetMediaFormat().IsEmpty()) // Use GetMediaFormat() to force creation of member
    return fmtp;

  PString str = mediaFormat.GetOptionString("FMTP");
  if (!str.IsEmpty())
    return str;

  for (PINDEX i = 0; i < mediaFormat.GetOptionCount(); i++) {
    const OpalMediaOption & option = mediaFormat.GetOption(i);
    const PString & name = option.GetFMTPName();
    if (!name.IsEmpty()) {
      PString value = option.AsString();
      if (value != option.GetFMTPDefault()) {
        if (!str.IsEmpty())
          str += ';';
        str += name + '=' + value;
      }
    }
  }

  return !str ? str : fmtp;
}


PString SDPMediaFormat::GetNTEString() const
{
  return GetNXEString(nteSet);
}

void SDPMediaFormat::AddNTEString(const PString & str)
{
  AddNXEString(nteSet, str);
}

void SDPMediaFormat::AddNTEToken(const PString & ostr)
{
  AddNXEToken(nteSet, ostr);
}

PString SDPMediaFormat::GetNSEString() const
{
  return GetNXEString(nseSet);
}

void SDPMediaFormat::AddNSEString(const PString & str)
{
  AddNXEString(nseSet, str);
}

void SDPMediaFormat::AddNSEToken(const PString & ostr)
{
  AddNXEToken(nseSet, ostr);
}

PString SDPMediaFormat::GetNXEString(POrdinalSet & nxeSet) const
{
  PString str;
  PINDEX i = 0;
  while (i < 255) {
    if (!nxeSet.Contains(POrdinalKey(i)))
      i++;
    else {
      PINDEX start = i++;
      while (nxeSet.Contains(POrdinalKey(i)))
        i++;
      if (!str.IsEmpty())
        str += ",";
      str += PString(PString::Unsigned, start);
      if (i > start+1)
        str += PString('-') + PString(PString::Unsigned, i-1);
    }
  }

  return str;
}


void SDPMediaFormat::AddNXEString(POrdinalSet & nxeSet, const PString & str)
{
  PStringArray tokens = str.Tokenise(",", PFalse);
  PINDEX i;
  for (i = 0; i < tokens.GetSize(); i++)
    AddNXEToken(nxeSet, tokens[i]);
}


void SDPMediaFormat::AddNXEToken(POrdinalSet & nxeSet, const PString & ostr)
{
  PString str = ostr.Trim();
  if (str[0] == ',')
    str = str.Mid(1);
  if (str.Right(1) == ",")
    str = str.Left(str.GetLength()-1);
  PINDEX pos = str.Find('-');
  if (pos == P_MAX_INDEX)
    nxeSet.Include(new POrdinalKey(str.AsInteger()));
  else {
    PINDEX from = str.Left(pos).AsInteger();
    PINDEX to   = str.Mid(pos+1).AsInteger();
    while (from <= to)
      nxeSet.Include(new POrdinalKey(from++));
  }
}


void SDPMediaFormat::PrintOn(ostream & strm) const
{
  PAssert(!encodingName.IsEmpty(), "SDPAudioMediaFormat encoding name is empty");
  mediaFormat.ToCustomisedOptions();

  PINDEX i;
  for (i = 0; i < 2; ++i) {
    switch (i) {
#ifdef FMTP_BEFORE_RTPMAP
      case 1:
#else
      case 0:
#endif
        strm << "a=rtpmap:" << (int)payloadType << ' ' << encodingName << '/' << clockRate;
        if (!parameters.IsEmpty())
          strm << '/' << parameters;
        strm << "\r\n";
        break;
#ifdef FMTP_BEFORE_RTPMAP
      case 0:
#else
      case 1:
#endif
        {
          PString fmtpString = GetFMTP();
          if (!fmtpString.IsEmpty())
            strm << "a=fmtp:" << (int)payloadType << ' ' << fmtpString << "\r\n";
        }
    }
  }
}


const OpalMediaFormat & SDPMediaFormat::GetMediaFormat() const
{
  if (mediaFormat.IsEmpty()) {
    mediaFormat = OpalMediaFormat(payloadType, clockRate, encodingName, "sip");
    PTRACE_IF(2, mediaFormat.IsEmpty(), "SDP\tCould not find media format for \""
              << encodingName << "\", pt=" << payloadType << ", clock=" << clockRate);

    mediaFormat.MakeUnique();
    mediaFormat.SetPayloadType(payloadType);

    if (!parameters.IsEmpty() && (mediaFormat.GetMediaType() == OpalMediaType::Audio())) 
      mediaFormat.SetOptionInteger(OpalAudioFormat::ChannelsOption(), parameters.AsUnsigned());
    else
      mediaFormat.SetOptionInteger(OpalAudioFormat::ChannelsOption(), 1);

    // Fill in the default values for (possibly) missing FMTP options
    for (PINDEX i = 0; i < mediaFormat.GetOptionCount(); i++) {
      OpalMediaOption & option = const_cast<OpalMediaOption &>(mediaFormat.GetOption(i));
      if (!option.GetFMTPName().IsEmpty() && !option.GetFMTPDefault().IsEmpty())
        option.FromString(option.GetFMTPDefault());
    }
  }

  return mediaFormat;
}


bool SDPMediaFormat::ToNormalisedOptions()
{
  if (GetMediaFormat().IsEmpty()) // Use GetMediaFormat() to force creation of member
    return false;

  // try to init encodingName from global list, to avoid PAssert when media has no rtpmap
  if (encodingName.IsEmpty())
    encodingName = mediaFormat.GetEncodingName();

  if (mediaFormat.ToNormalisedOptions())
    return true;

  PTRACE(2, "SDP\tCould not normalise format \"" << GetEncodingName() << "\", pt=" << GetPayloadType() << ", removing.");
  return false;
}


void SDPMediaFormat::SetPacketTime(const PString & optionName, unsigned ptime)
{
  if (mediaFormat.HasOption(optionName)) {
    unsigned frameTime = mediaFormat.GetFrameTime();
    unsigned newCount = (ptime*mediaFormat.GetTimeUnits()+frameTime-1)/frameTime;
    mediaFormat.SetOptionInteger(optionName, newCount);
    PTRACE(4, "SDP\tMedia format \"" << mediaFormat << "\" option \"" << optionName
           << "\" set to " << newCount << " packets from " << ptime << " milliseconds");
  }
}


PBoolean SDPMediaDescription::SetTransportAddress(const OpalTransportAddress &t)
{
  PIPSocket::Address ip;
  WORD port = 0;
  if (transportAddress.GetIpAndPort(ip, port)) {
    transportAddress = OpalTransportAddress(t, port);
    return PTrue;
  }
  return PFalse;
}


//////////////////////////////////////////////////////////////////////////////

unsigned & SDPBandwidth::operator[](const PString & type)
{
  return std::map<PString, unsigned>::operator[](type);
}


unsigned SDPBandwidth::operator[](const PString & type) const
{
  const_iterator it = find(type);
  return it != end() ? it->second : UINT_MAX;
}


ostream & operator<<(ostream & out, const SDPBandwidth & bw)
{
  for (SDPBandwidth::const_iterator iter = bw.begin(); iter != bw.end(); ++iter)
    out << "b=" << iter->first << ':' << iter->second << "\r\n";
  return out;
}


bool SDPBandwidth::Parse(const PString & param)
{
  PINDEX pos = param.FindSpan("!#$%&'*+-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ^_`abcdefghijklmnopqrstuvwxyz{|}~"); // Legal chars from RFC
  if (pos == P_MAX_INDEX || param[pos] != ':') {
    PTRACE(2, "SDP\tMalformed bandwidth attribute " << param);
    return false;
  }

  (*this)[param.Left(pos)] = param.Mid(pos+1).AsUnsigned();
  return true;
}


//////////////////////////////////////////////////////////////////////////////

SDPMediaDescription::SDPMediaDescription(const OpalTransportAddress & address)
  : transportAddress(address)
{
  direction = Undefined;
  port      = 0;
  portCount = 0;
}


bool SDPMediaDescription::Decode(const PStringArray & tokens)
{
  // parse the media type
  mediaType = OpalMediaType::GetMediaTypeFromSDP(tokens[0]);
  if (mediaType.empty()) {
    PTRACE(1, "SDP\tUnknown SDP media type " << tokens[0]);
    return false;
  }
  OpalMediaTypeDefinition * defn = mediaType.GetDefinition();
  if (defn == NULL) {
    PTRACE(1, "SDP\tNo definition for SDP media type " << tokens[0]);
    return false;
  }

  // parse the port and port count
  PString portStr  = tokens[1];
  PINDEX pos = portStr.Find('/');
  if (pos == P_MAX_INDEX) 
    portCount = 1;
  else {
    PTRACE(3, "SDP\tMedia header contains port count - " << portStr);
    portCount = (WORD)portStr.Mid(pos+1).AsUnsigned();
    portStr   = portStr.Left(pos);
  }
  port = (WORD)portStr.AsUnsigned();

  // parse the transport
  PString transport = tokens[2];
  if (transport != GetSDPTransportType()) {
    PTRACE(2, "SDP\tMedia session transport " << transport << " not compatible with " << GetSDPTransportType());
    return false;
  }

  // check everything
  switch (port) {
    case 0 :
      PTRACE(3, "SDP\tIgnoring media session " << mediaType << " with port=0");
      direction = Inactive;
      break;

    case 65535 :
      PTRACE(2, "SDP\tIllegal port=65535 in media session " << mediaType << ", trying to continue.");
      port = 65534;
      // Do next case

    default :
      PTRACE(4, "SDP\tMedia session port=" << port);

      PIPSocket::Address ip;
      if (transportAddress.GetIpAddress(ip))
        transportAddress = OpalTransportAddress(ip, (WORD)port);
  }

  // create the format list
  for (PINDEX i = 3; i < tokens.GetSize(); i++) {
    SDPMediaFormat * fmt = CreateSDPMediaFormat(tokens[i]);
    if (fmt == NULL) {
      PTRACE(2, "SDP\tCannot create SDP media format for port " << tokens[i]);
    }
    formats.Append(fmt);
  }

  return PTrue;
}


bool SDPMediaDescription::Decode(char key, const PString & value)
{
  PINDEX pos;

  switch (key) {
    case 'i' : // media title
    case 'k' : // encryption key
      break;

    case 'b' : // bandwidth information
      bandwidth.Parse(value);
      break;
      
    case 'c' : // connection information - optional if included at session-level
      SetTransportAddress(ParseConnectAddress(value, port));
      break;

    case 'a' : // zero or more media attribute lines
      pos = value.FindSpan("!#$%&'*+-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ^_`abcdefghijklmnopqrstuvwxyz{|}~"); // Legal chars from RFC
      if (pos == P_MAX_INDEX)
        SetAttribute(value, "1");
      else if (value[pos] == ':')
        SetAttribute(value.Left(pos), value.Mid(pos+1));
      else {
        PTRACE(2, "SDP\tMalformed media attribute " << value);
      }
      break;

    default:
      PTRACE(1, "SDP\tUnknown media information key " << key);
  }

  return true;
}


bool SDPMediaDescription::PostDecode()
{
  SDPMediaFormatList::iterator format = formats.begin();
  while (format != formats.end()) {
    if (format->ToNormalisedOptions())
      ++format;
    else
      formats.erase(format++);
  }

  return true;
}


void SDPMediaDescription::SetAttribute(const PString & attr, const PString & value)
{
  // get the attribute type
  if (attr *= "sendonly") {
    direction = SendOnly;
    return;
  }

  if (attr *= "recvonly") {
    direction = RecvOnly;
    return;
  }

  if (attr *= "sendrecv") {
    direction = SendRecv;
    return;
  }

  if (attr *= "inactive") {
    direction = Inactive;
    return;
  }

  // handle fmtp attributes
  if (attr *= "fmtp") {
    PString params = value;
    SDPMediaFormat * format = FindFormat(params);
    if (format != NULL)
      format->SetFMTP(params);
    return;
  }

  // unknown attriutes
  PTRACE(2, "SDP\tUnknown media attribute " << attr);
  return;
}


SDPMediaFormat * SDPMediaDescription::FindFormat(PString & params) const
{
  SDPMediaFormatList::const_iterator format;

  // extract the RTP payload type
  PINDEX pos = params.FindSpan("0123456789");
  if (pos == P_MAX_INDEX || isspace(params[pos])) {
    // find the format that matches the payload type
    RTP_DataFrame::PayloadTypes pt = (RTP_DataFrame::PayloadTypes)params.Left(pos).AsUnsigned();
    for (format = formats.begin(); format != formats.end(); ++format) {
      if (format->GetPayloadType() == pt)
        break;
    }
  }
  else {
    // Check for it a format name
    pos = params.Find(' ');
    PString fmtName = params.Left(pos);
    for (format = formats.begin(); format != formats.end(); ++format) {
      if (format->GetEncodingName() == fmtName)
        break;
    }
  }

  if (format == formats.end()) {
    PTRACE(2, "SDP\tMedia attribute found for unknown RTP type/name " << params.Left(pos));
    return NULL;
  }

  // extract the attribute argument
  if (pos != P_MAX_INDEX) {
    while (isspace(params[pos]))
      pos++;
    params.Delete(0, pos);
  }

  return const_cast<SDPMediaFormat *>(&*format);
}


void SDPMediaDescription::SetPacketTime(const PString & optionName, const PString & value)
{
  unsigned newTime = value.AsUnsigned();
  if (newTime < 10) {
    PTRACE(2, "SDP\tMalformed (max)ptime attribute value " << value);
    return;
  }

  for (SDPMediaFormatList::iterator format = formats.begin(); format != formats.end(); ++format)
   format->SetPacketTime(optionName, newTime);
}


void SDPMediaDescription::PrintOn(const OpalTransportAddress & commonAddr, ostream & str) const
{
  PString connectString;
  PIPSocket::Address commonIP, transportIP;
  if (transportAddress.GetIpAddress(transportIP) && commonAddr.GetIpAddress(commonIP) && commonIP != transportIP)
    connectString = GetConnectAddressString(transportAddress);

  PrintOn(str, connectString);
}

void SDPMediaDescription::PrintOn(ostream & str) const
{
  PrintOn(str, GetConnectAddressString(transportAddress));
}

bool SDPMediaDescription::PrintOn(ostream & str, const PString & connectString) const
{
  //
  // if no media formats, then do not output the media header
  // this avoids displaying an empty media header with no payload types
  // when (for example) video has been disabled
  //
  if (formats.GetSize() == 0)
    return false;

  PIPSocket::Address ip;
  WORD port = 0;
  transportAddress.GetIpAndPort(ip, port);

  // output media header
  str << "m=" 
      << GetSDPMediaType() << " "
      << port << " "
      << GetSDPTransportType()
      << bandwidth;

  str << GetSDPPortList() << "\r\n";

  if (!connectString.IsEmpty())
    str << "c=" << connectString << "\r\n";

  // If we have a port of zero, then shutting down SDP stream. No need for anything more
  if (port == 0)
    return false;

  // media format direction
  switch (direction) {
    case SDPMediaDescription::RecvOnly:
      str << "a=recvonly" << "\r\n";
      break;
    case SDPMediaDescription::SendOnly:
      str << "a=sendonly" << "\r\n";
      break;
    case SDPMediaDescription::SendRecv:
      str << "a=sendrecv" << "\r\n";
      break;
    case SDPMediaDescription::Inactive:
      str << "a=inactive" << "\r\n";
      break;
    default:
      break;
  }

  return true;
}

OpalMediaFormatList SDPMediaDescription::GetMediaFormats() const
{
  OpalMediaFormatList list;

  for (SDPMediaFormatList::const_iterator format = formats.begin(); format != formats.end(); ++format) {
    OpalMediaFormat opalFormat = format->GetMediaFormat();
    if (opalFormat.IsEmpty())
      PTRACE(2, "SIP\tRTP payload type " << format->GetPayloadType() 
             << ", name=" << format->GetEncodingName() << ", not matched to supported codecs");
    else {
      if (opalFormat.GetMediaType() == mediaType && 
          opalFormat.IsValidForProtocol("sip") &&
          opalFormat.GetEncodingName() != NULL) {
        PTRACE(3, "SIP\tRTP payload type " << format->GetPayloadType() << " matched to codec " << opalFormat);
        list += opalFormat;
      }
    }
  }

  return list;
}


void SDPMediaDescription::AddSDPMediaFormat(SDPMediaFormat * sdpMediaFormat)
{
  formats.Append(sdpMediaFormat);
}


void SDPMediaDescription::RemoveSDPMediaFormat(const SDPMediaFormat & sdpMediaFormat)
{
  OpalMediaFormat mediaFormat = sdpMediaFormat.GetMediaFormat();
  const char * encodingName = mediaFormat.GetEncodingName();
  unsigned clockRate = mediaFormat.GetClockRate();

  if (!mediaFormat.IsValidForProtocol("sip") || encodingName == NULL || *encodingName == '\0')
    return;

  for (SDPMediaFormatList::iterator format = formats.begin(); format != formats.end(); ) {
    if ((format->GetEncodingName() *= encodingName) && (format->GetClockRate() == clockRate)) {
      PTRACE(3, "SDP\tRemoving format=" << encodingName << ", " << format->GetPayloadType());
      formats.erase(format++);
    }
    else
      ++format;
  }
}


void SDPMediaDescription::AddMediaFormat(const OpalMediaFormat & mediaFormat)
{
  if (!mediaFormat.IsTransportable() || !mediaFormat.IsValidForProtocol("sip")) {
    PTRACE(4, "SDP\tSDP not including " << mediaFormat << " as it is not a SIP transportable format");
    return;
  }

  RTP_DataFrame::PayloadTypes payloadType = mediaFormat.GetPayloadType();
  unsigned clockRate = mediaFormat.GetClockRate();

  for (SDPMediaFormatList::iterator format = formats.begin(); format != formats.end(); ++format) {
    if (format->GetPayloadType() == payloadType ||
        ((format->GetEncodingName() *= mediaFormat.GetEncodingName()) && format->GetClockRate() == clockRate)
        ) {
      PTRACE(4, "SDP\tSDP not including " << mediaFormat << " as it is already included");
      return;
    }
  }

  SDPMediaFormat * sdpFormat = new SDPMediaFormat(mediaFormat, payloadType);

  ProcessMediaOptions(*sdpFormat, mediaFormat);

  AddSDPMediaFormat(sdpFormat);
}

void SDPMediaDescription::ProcessMediaOptions(SDPMediaFormat & /*sdpFormat*/, const OpalMediaFormat & /*mediaFormat*/)
{
}


void SDPMediaDescription::AddMediaFormats(const OpalMediaFormatList & mediaFormats, const OpalMediaType & mediaType)
{
  for (OpalMediaFormatList::const_iterator format = mediaFormats.begin(); format != mediaFormats.end(); ++format) {
    if (format->GetMediaType() == mediaType && (format->IsTransportable()))
      AddMediaFormat(*format);
  }
}

//////////////////////////////////////////////////////////////////////////////

SDPRTPAVPMediaDescription::SDPRTPAVPMediaDescription(const OpalTransportAddress & address)
  : SDPMediaDescription(address)
{
}


PCaselessString SDPRTPAVPMediaDescription::GetSDPTransportType() const
{ 
  return SDP_MEDIA_TRANSPORT_RTPAVP; 
}


SDPMediaFormat * SDPRTPAVPMediaDescription::CreateSDPMediaFormat(const PString & portString)
{
  return new SDPMediaFormat((RTP_DataFrame::PayloadTypes)portString.AsUnsigned());
}


PString SDPRTPAVPMediaDescription::GetSDPPortList() const
{
  PStringStream str;

  SDPMediaFormatList::const_iterator format;

  // output RTP payload types
  for (format = formats.begin(); format != formats.end(); ++format)
    str << ' ' << (int)format->GetPayloadType();

  return str;
}

bool SDPRTPAVPMediaDescription::PrintOn(ostream & str, const PString & connectString) const
{
  // call ancestor
  if (!SDPMediaDescription::PrintOn(str, connectString))
    return false;

  // output attributes for each payload type
  SDPMediaFormatList::const_iterator format;
  for (format = formats.begin(); format != formats.end(); ++format)
    str << *format;

  return true;
}

void SDPRTPAVPMediaDescription::SetAttribute(const PString & attr, const PString & value)
{
  // handle rtpmap attribute
  if (attr *= "rtpmap") {
    PString params = value;
    SDPMediaFormat * format = FindFormat(params);
    if (format != NULL) {
      PStringArray tokens = params.Tokenise('/');
      if (tokens.GetSize() < 2) {
        PTRACE(2, "SDP\tMalformed rtpmap attribute for " << format->GetEncodingName());
        return;
      }

      format->SetEncodingName(tokens[0]);
      format->SetClockRate(tokens[1].AsUnsigned());
      if (tokens.GetSize() > 2)
        format->SetParameters(tokens[2]);
    }
    return;
  }

  return SDPMediaDescription::SetAttribute(attr, value);
}


//////////////////////////////////////////////////////////////////////////////

SDPAudioMediaDescription::SDPAudioMediaDescription(const OpalTransportAddress & address)
  : SDPRTPAVPMediaDescription(address)
{
}


PString SDPAudioMediaDescription::GetSDPMediaType() const 
{ 
  return "audio"; 
}


bool SDPAudioMediaDescription::PrintOn(ostream & str, const PString & connectString) const
{
  // call ancestor
  if (!SDPRTPAVPMediaDescription::PrintOn(str, connectString))
    return false;

#ifdef HAVE_PTIME
  // Fill in the ptime  as maximum tx packets of all media formats
  // and maxptime as minimum rx packets of all media formats
  unsigned ptime = 0;
  unsigned maxptime = UINT_MAX;

  // output attributes for each payload type
  for (format = formats.begin(); format != formats.end(); ++format) {
    const OpalMediaFormat & mediaFormat = formats[i].GetMediaFormat();
    if (mediaFormat.HasOption(OpalAudioFormat::TxFramesPerPacketOption())) {
      unsigned ptime1 = txFrames*mediaFormat.GetFrameTime()/mediaFormat.GetTimeUnits();
      if (ptime < ptime1)
        ptime = ptime1;
    }
    if (mediaFormat.HasOption(OpalAudioFormat::RxFramesPerPacketOption())) {
      unsigned maxptime1 = mediaFormat.GetOptionInteger(OpalAudioFormat::RxFramesPerPacketOption())*mediaFormat.GetFrameTime()/mediaFormat.GetTimeUnits();
      if (maxptime > maxptime1)
        maxptime = maxptime1;
    }
  }

  // don't output ptime parameters, as some Cisco endpoints barf on it
  // and it's not very well-defined anyway
  //if (ptime > 0)
  //  str << "a=ptime:" << ptime << "\r\n";

  if (maxptime < UINT_MAX)
    str << "a=maxptime:" << maxptime << "\r\n";
#endif // HAVE_PTIME

  return true;
}

void SDPAudioMediaDescription::SetAttribute(const PString & attr, const PString & value)
{
#ifdef HAVE_PTIME
  if (attr *= "ptime") {
    SetPacketTime(OpalAudioFormat::TxFramesPerPacketOption(), value);
    return;
  }

  if (attr *= "maxptime") {
    SetPacketTime(OpalAudioFormat::RxFramesPerPacketOption(), value);
    return;
  }
#endif // HAVE_PTIME

  return SDPRTPAVPMediaDescription::SetAttribute(attr, value);
}

//////////////////////////////////////////////////////////////////////////////

SDPVideoMediaDescription::SDPVideoMediaDescription(const OpalTransportAddress & address)
  : SDPRTPAVPMediaDescription(address)
{
}

PString SDPVideoMediaDescription::GetSDPMediaType() const 
{ 
  return "video"; 
}

//////////////////////////////////////////////////////////////////////////////

SDPApplicationMediaDescription::SDPApplicationMediaDescription(const OpalTransportAddress & address)
  : SDPMediaDescription(address)
{
}

PCaselessString SDPApplicationMediaDescription::GetSDPTransportType() const
{ 
  return "rtp/avp"; 
}


PString SDPApplicationMediaDescription::GetSDPMediaType() const 
{ 
  return "application"; 
}

SDPMediaFormat * SDPApplicationMediaDescription::CreateSDPMediaFormat(const PString & portString)
{
  return new SDPMediaFormat(RTP_DataFrame::DynamicBase, portString);
}

PString SDPApplicationMediaDescription::GetSDPPortList() const
{
  PStringStream str;

  // output encoding names for non RTP
  SDPMediaFormatList::const_iterator format;
  for (format = formats.begin(); format != formats.end(); ++format)
    str << ' ' << format->GetEncodingName();

  return str;
}

//////////////////////////////////////////////////////////////////////////////

const PString & SDPSessionDescription::ConferenceTotalBandwidthType()     { static PString s = "CT"; return s; }
const PString & SDPSessionDescription::ApplicationSpecificBandwidthType() { static PString s = "AS"; return s; }

SDPSessionDescription::SDPSessionDescription(const OpalTransportAddress & address)
  : sessionName(SIP_DEFAULT_SESSION_NAME),
    ownerUsername('-'),
    ownerAddress(address),
    defaultConnectAddress(address)
{
  protocolVersion  = 0;
  ownerSessionId  = ownerVersion = (unsigned)PTime().GetTimeInSeconds();
  direction = SDPMediaDescription::Undefined;
}


void SDPSessionDescription::PrintOn(ostream & str) const
{
  OpalTransportAddress connectionAddress(defaultConnectAddress);
  PBoolean useCommonConnect = PTrue;

  // see common connect address is needed
  {
    OpalTransportAddress descrAddress;
    PINDEX matched = 0;
    PINDEX descrMatched = 0;
    PINDEX i;
    for (i = 0; i < mediaDescriptions.GetSize(); i++) {
      if (i == 0)
        descrAddress = mediaDescriptions[i].GetTransportAddress();
      if (mediaDescriptions[i].GetTransportAddress() == connectionAddress)
        ++matched;
      if (mediaDescriptions[i].GetTransportAddress() == descrAddress)
        ++descrMatched;
    }
    if (connectionAddress != descrAddress) {
      if ((descrMatched > matched))
        connectionAddress = descrAddress;
      else
        useCommonConnect = PFalse;
    }
  }

  // encode mandatory session information
  str << "v=" << protocolVersion << "\r\n"
         "o=" << ownerUsername << ' '
      << ownerSessionId << ' '
      << ownerVersion << ' '
      << GetConnectAddressString(ownerAddress)
      << "\r\n"
         "s=" << sessionName << "\r\n";

  // make sure the "c=" line (if required) is before the "t=" otherwise the proxy
  // used by FWD will reject the call with an SDP parse error. This does not seem to be 
  // required by the RFC so it is probably a bug in the proxy
  if (useCommonConnect)
    str << "c=" << GetConnectAddressString(connectionAddress) << "\r\n";
  
  str << "t=" << "0 0" << "\r\n"
      << bandwidth;

  switch (direction) {
    case SDPMediaDescription::RecvOnly:
      str << "a=recvonly" << "\r\n";
      break;
    case SDPMediaDescription::SendOnly:
      str << "a=sendonly" << "\r\n";
      break;
    case SDPMediaDescription::SendRecv:
      str << "a=sendrecv" << "\r\n";
      break;
    case SDPMediaDescription::Inactive:
      str << "a=inactive" << "\r\n";
      break;
    default:
      break;
  }

  // encode media session information
  PINDEX i;
  for (i = 0; i < mediaDescriptions.GetSize(); i++) {
    if (useCommonConnect) 
      mediaDescriptions[i].PrintOn(connectionAddress, str);
    else
      str << mediaDescriptions[i];
  }
}


PString SDPSessionDescription::Encode() const
{
  PStringStream str;
  PrintOn(str);
  return str;
}


PBoolean SDPSessionDescription::Decode(const PString & str)
{
  bool ok = true;

  // break string into lines
  PStringArray lines = str.Lines();

  // parse keyvalue pairs
  SDPMediaDescription * currentMedia = NULL;
  PINDEX i;
  for (i = 0; i < lines.GetSize(); i++) {
    PString & line = lines[i];
    PINDEX pos = line.Find('=');
    if (pos == 1) {
      PString value = line.Mid(pos+1).Trim();

      /////////////////////////////////
      //
      // Session description
      //
      /////////////////////////////////
  
      if (currentMedia != NULL && line[0] != 'm') {

        // process all of the "a=rtpmap" lines first so that the media formats are 
        // created before any media description paramaters are processed
        int y = i;
        for (int pass = 0; pass < 2; ++pass) {
          y = i;
          for (y = i; y < lines.GetSize(); ++y) {
            PString & line2 = lines[y];
            PINDEX pos = line2.Find('=');
            if (pos == 1) {
              if (line2[0] == 'm') {
                --y;
                break;
              }
              PString value = line2.Mid(pos+1).Trim();
              bool isRtpMap = value.Left(6) *= "rtpmap";
              if (pass == 0 && isRtpMap)
                currentMedia->Decode(line2[0], value);
              else if (pass == 1 && !isRtpMap)
                currentMedia->Decode(line2[0], value);
            }
          }
        }
        i = y;
      }
      else {
        switch (line[0]) {
          case 'v' : // protocol version (mandatory)
            protocolVersion = value.AsInteger();
            break;

          case 'o' : // owner/creator and session identifier (mandatory)
            ParseOwner(value);
            break;

          case 's' : // session name (mandatory)
            sessionName = value;
            break;

          case 'c' : // connection information - not required if included in all media
            defaultConnectAddress = ParseConnectAddress(value);
            break;

          case 't' : // time the session is active (mandatory)
          case 'i' : // session information
          case 'u' : // URI of description
          case 'e' : // email address
          case 'p' : // phone number
            break;
          case 'b' : // bandwidth information
            bandwidth.Parse(value);
            break;
          case 'z' : // time zone adjustments
          case 'k' : // encryption key
          case 'r' : // zero or more repeat times
            break;
          case 'a' : // zero or more session attribute lines
            if (value *= "sendonly")
              SetDirection (SDPMediaDescription::SendOnly);
            else if (value *= "recvonly")
              SetDirection (SDPMediaDescription::RecvOnly);
            else if (value *= "sendrecv")
              SetDirection (SDPMediaDescription::SendRecv);
            else if (value *= "inactive")
              SetDirection (SDPMediaDescription::Inactive);
            break;

          case 'm' : // media name and transport address (mandatory)
            {
              if (currentMedia != NULL) {
                if (!currentMedia->PostDecode())
                  ok = false;
              }

              PStringArray tokens = value.Tokenise(" ");
              if (tokens.GetSize() < 4) {
                PTRACE(1, "SDP\tMedia session has only " << tokens.GetSize() << " elements");
                currentMedia = NULL;
              }
              else
              {
                // parse the media type
                OpalMediaType mediaType = OpalMediaType::GetMediaTypeFromSDP(tokens[0]);
                if (mediaType.empty()) {
                  PTRACE(1, "SDP\tUnknown SDP media type " << tokens[0]);
                  currentMedia = NULL;
                }
                else
                {
                  OpalMediaTypeDefinition * defn = mediaType.GetDefinition();
                  if (defn == NULL) {
                    PTRACE(1, "SDP\tNo definition for SDP media type " << tokens[0]);
                    currentMedia = NULL;
                  }
                  else {
                    currentMedia = defn->CreateSDPMediaDescription(defaultConnectAddress);
                    if (currentMedia == NULL) {
                      PTRACE(1, "SDP\tCould not create SDP media description for SDP media type " << tokens[0]);
                    }
                    else if (currentMedia->Decode(tokens)) {
                      mediaDescriptions.Append(currentMedia);
                      PTRACE(3, "SDP\tAdding media session with " << currentMedia->GetSDPMediaFormats().GetSize()
                                                                  << ' ' << currentMedia->GetSDPMediaType() << " formats");
                    }
                    else {
                      delete currentMedia;
                      currentMedia = NULL;
                    }
                  }
                }
              }
            }
            break;

          default:
            PTRACE(1, "SDP\tUnknown session information key " << line[0]);
        }
      }
    }
  }

  if (currentMedia != NULL) {
    if (!currentMedia->PostDecode())
      ok = false;
  }

  return ok;
}


void SDPSessionDescription::ParseOwner(const PString & str)
{
  PStringArray tokens = str.Tokenise(" ");

  if (tokens.GetSize() != 6) {
    PTRACE(2, "SDP\tOrigin has incorrect number of elements (" << tokens.GetSize() << ')');
  }
  else {
    ownerUsername    = tokens[0];
    ownerSessionId   = tokens[1].AsUnsigned();
    ownerVersion     = tokens[2].AsUnsigned();
    ownerAddress = defaultConnectAddress = ParseConnectAddress(tokens, 3);
  }
}


SDPMediaDescription * SDPSessionDescription::GetMediaDescriptionByType(const OpalMediaType & rtpMediaType) const
{
  // look for matching media type
  PINDEX i;
  for (i = 0; i < mediaDescriptions.GetSize(); i++) {
    if (mediaDescriptions[i].GetMediaType() == rtpMediaType)
      return &mediaDescriptions[i];
  }

  return NULL;
}

SDPMediaDescription * SDPSessionDescription::GetMediaDescriptionByIndex(PINDEX index) const
{
  if (index > mediaDescriptions.GetSize())
    return NULL;

  return &mediaDescriptions[index-1];
}

SDPMediaDescription::Direction SDPSessionDescription::GetDirection(unsigned sessionID) const
{
  if (sessionID > 0 && sessionID <= (unsigned)mediaDescriptions.GetSize())
    return mediaDescriptions[sessionID-1].GetDirection();
  
  return defaultConnectAddress.IsEmpty() ? SDPMediaDescription::Inactive : direction;
}


bool SDPSessionDescription::IsHold() const
{
  if (defaultConnectAddress.IsEmpty()) // Old style "hold"
    return true;

  if (GetBandwidth(SDPSessionDescription::ApplicationSpecificBandwidthType()) == 0)
    return true;

  PINDEX i;
  for (i = 0; i < mediaDescriptions.GetSize(); i++) {
    SDPMediaDescription::Direction dir = mediaDescriptions[i].GetDirection();
    if ((dir == SDPMediaDescription::Undefined) || ((dir & SDPMediaDescription::SendOnly) != 0))
      return false;
  }

  return true;
}


void SDPSessionDescription::SetDefaultConnectAddress(const OpalTransportAddress & address)
{
   defaultConnectAddress = address;
   if (ownerAddress.IsEmpty())
     ownerAddress = address;
}

#endif // OPAL_SIP

// End of file ////////////////////////////////////////////////////////////////
