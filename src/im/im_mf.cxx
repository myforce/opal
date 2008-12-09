/*
 * im_mf.cxx
 *
 * Instant Messaging Media Format descriptions
 *
 * Open Phone Abstraction Library
 *
 * Copyright (c) 2008 Post Increment
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
 * The Original Code is Open Phone Abstraction Library
 *
 * The Initial Developer of the Original Code is Post Increment
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision: 21406 $
 * $Author: csoutheren $
 * $Date: 2008-10-22 22:59:46 +1100 (Wed, 22 Oct 2008) $
 */

#include <ptlib.h>
#include <opal/buildopts.h>

#include <opal/mediafmt.h>
#include <opal/connection.h>
#include <im/im.h>
#include <im/msrp.h>
#include <rtp/rtp.h>

#define new PNEW

/////////////////////////////////////////////////////////////////////////////

#if OPAL_MSRP_CAPABILITY

OPAL_INSTANTIATE_MEDIATYPE(msrp, OpalMSRPMediaType);

OpalMSRPMediaType::OpalMSRPMediaType()
  : OpalIMMediaType("msrp", "message|tcp/msrp", 5, true)
{
}

/////////////////////////////////////////////////////////////////////////////


#define DECLARE_MSRP_ENCODING(title, encoding) \
class IM##title##OpalMSRPEncoding : public OpalMSRPEncoding \
{ \
}; \
static PFactory<OpalMSRPEncoding>::Worker<IM##title##OpalMSRPEncoding> worker_##IM##title##OpalMSRPEncoding(encoding, true); \

/////////////////////////////////////////////////////////////////////////////

DECLARE_MSRP_ENCODING(Text, "text/plain");
DECLARE_MSRP_ENCODING(CPIM, "message/cpim");
DECLARE_MSRP_ENCODING(HTML, "message/html");

const OpalMediaFormat & GetOpalMSRP() 
{ 
  static class IMMSRPMediaFormat : public OpalMediaFormat { 
    public: 
      IMMSRPMediaFormat() 
        : OpalMediaFormat(OPAL_MSRP, 
                          "msrp", 
                          RTP_DataFrame::MaxPayloadType, 
                          "+", 
                          false,  
                          1440, 
                          512, 
                          0, 
                          0) 
      { 
        PFactory<OpalMSRPEncoding>::KeyList_T types = PFactory<OpalMSRPEncoding>::GetKeyList();
        PFactory<OpalMSRPEncoding>::KeyList_T::iterator r;

        PString acceptTypes;
        for (r = types.begin(); r != types.end(); ++r) {
          if (!acceptTypes.IsEmpty())
            acceptTypes += " ";
          acceptTypes += *r;
        }
        
        OpalMediaOption * acceptOption = new OpalMediaOptionString("Accept Types", false, acceptTypes);
        acceptOption->SetMerge(OpalMediaOption::NoMerge);
        AddOption(acceptOption);
      } 
  } const f; 
  return f; 
} 

#endif // OPAL_MSRP_CAPABILITY 


//////////////////////////////////////////////////////////////////////////////////////////

#if OPAL_SIPIM_CAPABILITY

OPAL_INSTANTIATE_MEDIATYPE2(sipim, "sip-im", OpalSIPIMMediaType);

/////////////////////////////////////////////////////////////////////////////

OpalSIPIMMediaType::OpalSIPIMMediaType()
  : OpalIMMediaType("sip-im", "message|sip", 6, true)
{
}

/////////////////////////////////////////////////////////////////////////////

const OpalMediaFormat & GetOpalSIPIM() 
{ 
  static class IMSIPMediaFormat : public OpalMediaFormat { 
    public: 
      IMSIPMediaFormat() 
        : OpalMediaFormat(OPAL_SIPIM, 
                          "sip-im", 
                          RTP_DataFrame::MaxPayloadType, 
                          "+", 
                          false,  
                          1440, 
                          512, 
                          0, 
                          0) 
      { 
      } 
  } const f; 
  return f; 
} 

#endif // OPAL_SIPIM_CAPABILITY

//////////////////////////////////////////////////////////////////////////////////////////

OpalIMMediaStream::OpalIMMediaStream(
      OpalConnection & conn,
      const OpalMediaFormat & mediaFormat, ///<  Media format for stream
      unsigned sessionID,                  ///<  Session number for stream
      bool isSource                       ///<  Is a source stream
    )
  : OpalMediaStream(conn, mediaFormat, sessionID, isSource)
{
}

