/*
 * g729codec.h
 *
 * H.323 interface for G.729A codec
 *
 * Open H323 Library
 *
 * Copyright (c) 2001 Equivalence Pty. Ltd.
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
 * $Log: g729codec.h,v $
 * Revision 1.2004  2002/09/16 02:52:33  robertj
 * Added #define so can select if #pragma interface/implementation is used on
 *   platform basis (eg MacOS) rather than compiler, thanks Robert Monaghan.
 *
 * Revision 2.2  2002/09/04 06:01:46  robertj
 * Updated to OpenH323 v1.9.6
 *
 * Revision 2.1  2002/07/01 04:56:29  robertj
 * Updated to OpenH323 v1.9.1
 *
 * Revision 1.4  2002/09/03 06:19:36  robertj
 * Normalised the multi-include header prevention ifdef/define symbol.
 *
 * Revision 1.3  2002/08/05 10:03:47  robertj
 * Cosmetic changes to normalise the usage of pragma interface/implementation.
 *
 * Revision 1.2  2002/06/27 03:08:31  robertj
 * Added G.729 capabilitity support even though is really G.729A.
 * Added code to include G.729 codecs on static linking.
 *
 * Revision 1.1  2001/09/21 02:54:47  robertj
 * Added new codec framework with no actual implementation.
 *
 */

#ifndef __OPAL_G729CODEC_H
#define __OPAL_G729CODEC_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


#include <opal/transcoders.h>


///////////////////////////////////////////////////////////////////////////////

class Opal_G729_PCM : public OpalFramedTranscoder {
  public:
    Opal_G729_PCM(
      const OpalTranscoderRegistration & registration /// Registration for transcoder
    );
    virtual BOOL ConvertFrame(const BYTE * src, BYTE * dst);
};

typedef Opal_G729_PCM Opal_G729A_PCM;


///////////////////////////////////////////////////////////////////////////////

class Opal_PCM_G729 : public OpalFramedTranscoder {
  public:
    Opal_PCM_G729(
      const OpalTranscoderRegistration & registration /// Registration for transcoder
    );
    virtual BOOL ConvertFrame(const BYTE * src, BYTE * dst);
};

typedef Opal_PCM_G729 Opal_PCM_G729A;


///////////////////////////////////////////////////////////////////////////////

#define OPAL_REGISTER_G729() \
          OPAL_REGISTER_TRANSCODER(Opal_G729_PCM, OPAL_G729, OPAL_PCM16); \
          OPAL_REGISTER_TRANSCODER(Opal_PCM_G729, OPAL_PCM16, OPAL_G729); \
          OPAL_REGISTER_TRANSCODER(Opal_G729A_PCM, OPAL_G729A, OPAL_PCM16); \
          OPAL_REGISTER_TRANSCODER(Opal_PCM_G729A, OPAL_PCM16, OPAL_G729A)



#endif // __OPAL_G729CODEC_H


/////////////////////////////////////////////////////////////////////////////
