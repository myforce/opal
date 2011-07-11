/*
 * t38proto.cxx
 *
 * T.38 protocol handler
 *
 * Open Phone Abstraction Library
 *
 * Copyright (c) 1998-2002 Equivalence Pty. Ltd.
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
 * Contributor(s): Vyacheslav Frolov.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

/*

There are two methods for signalling T.38 within a SIP session.
See T.38 Annex D for more information

UDPTL encapsulation (see T.38 section 9.1) over UDP is signalled as described in RFC 3362 as follows:

  m=image 5000 udptl t38
  a=T38FaxVersion=2
  a=T38FaxRateManagement=transferredTCF

RTP encapsulation (see T.38 section 9.2) is signalled as described in RFC 4612 as follows:

  m=audio 5000 RTP/AVP 96
  a=rtpmap:96 t38/8000
  a=fmtp:98 T38FaxVersion=2;T38FaxRateManagement=transferredTCF

Either, or both, can be used in a call

*/

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "sipt38.h"
#endif

#include <opal/buildopts.h>

#include <t38/sipt38.h>

#if OPAL_SIP
#if OPAL_T38_CAPABILITY

/////////////////////////////////////////////////////////////////////////////

SDPMediaDescription * OpalFaxMediaType::CreateSDPMediaDescription(const OpalTransportAddress & localAddress,
                                                                  OpalMediaSession * /*session*/) const
{
  return new SDPFaxMediaDescription(localAddress);
}

/////////////////////////////////////////////////////////////////////////////

SDPFaxMediaDescription::SDPFaxMediaDescription(const OpalTransportAddress & address)
  : SDPMediaDescription(address, OpalMediaType::Fax())
{
  t38Attributes.SetAt("T38FaxRateManagement", "transferredTCF");
  t38Attributes.SetAt("T38FaxVersion",        "0");
  //t38Attributes.SetAt("T38MaxBitRate",        "9600");
  //t38Attributes.SetAt("T38FaxMaxBuffer",      "200");
}

PCaselessString SDPFaxMediaDescription::GetSDPTransportType() const
{ 
  return "udptl";
}

PString SDPFaxMediaDescription::GetSDPMediaType() const 
{ 
  return "image"; 
}

SDPMediaFormat * SDPFaxMediaDescription::CreateSDPMediaFormat(const PString & portString)
{
  const OpalMediaFormat mediaFormat(RTP_DataFrame::DynamicBase, 0, portString, "sip");
  if (mediaFormat.IsEmpty()) {
    PTRACE(2, "SDPFax\tCould not find media format for " << portString);
    return NULL;
  }

  PTRACE(3, "SDPFax\tUsing RTP payload " << mediaFormat.GetPayloadType() << " for " << portString);

  return new SDPMediaFormat(*this, mediaFormat);
}


PString SDPFaxMediaDescription::GetSDPPortList() const
{
  PStringStream str;

  // output encoding names for non RTP
  SDPMediaFormatList::const_iterator format;
  for (format = formats.begin(); format != formats.end(); ++format)
    str << ' ' << format->GetEncodingName();

  return str;
}

bool SDPFaxMediaDescription::PrintOn(ostream & str, const PString & connectString) const
{
  // call ancestor
  if (!SDPMediaDescription::PrintOn(str, connectString))
    return false;

  // output options
  for (PStringToString::const_iterator it = t38Attributes.begin(); it != t38Attributes.end(); ++it)
    str << "a=" << it->first << ":" << it->second << "\r\n";

  return true;
}

void SDPFaxMediaDescription::SetAttribute(const PString & attr, const PString & value)
{
  if (attr.Left(3) *= "t38") {
    t38Attributes.SetAt(attr, value);
    return;
  }

  return SDPMediaDescription::SetAttribute(attr, value);
}

void SDPFaxMediaDescription::ProcessMediaOptions(SDPMediaFormat & /*sdpFormat*/, const OpalMediaFormat & mediaFormat)
{
  if (mediaFormat.GetMediaType() == OpalMediaType::Fax()) {
    for (PINDEX i = 0; i < mediaFormat.GetOptionCount(); ++i) {
      const OpalMediaOption & option = mediaFormat.GetOption(i);
      if (option.GetName().Left(3) *= "t38") {
        PString value = option.AsString();
        if (value != option.GetFMTPDefault())
          t38Attributes.SetAt(option.GetName(), option.AsString());
      }
    }
  }
}


bool SDPFaxMediaDescription::PostDecode(const OpalMediaFormatList & mediaFormats)
{
  if (!SDPMediaDescription::PostDecode(mediaFormats))
    return false;

  for (SDPMediaFormatList::iterator format = formats.begin(); format != formats.end(); ++format) {
    OpalMediaFormat & mediaFormat = format->GetWritableMediaFormat();
    if (mediaFormat.GetMediaType() == OpalMediaType::Fax()) {
      for (PINDEX i = 0; i < t38Attributes.GetSize(); ++i) {
        PString key = t38Attributes.GetKeyAt(i);
        PString data = t38Attributes.GetDataAt(i);
        if (!mediaFormat.SetOptionValue(key, data)) {
          PTRACE(2, "T38\tCould not set option \"" << key << "\" to \"" << data << '"');
        }
      }
      PTRACE(5, "T38\tMedia format set from SDP:\n" << setw(-1) << mediaFormat);
    }
  }

  return true;
}


#endif // OPAL_T38_CAPABILITY
#endif // OPAL_SIP

