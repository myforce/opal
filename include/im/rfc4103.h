/*
 * rfc4103.h
 *
 * Implementation of RFC 4103 RTP Payload for Text Conversation
 *
 * Open Phone Abstraction Library (OPAL)
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

#ifndef OPAL_IM_RFC4103_H
#define OPAL_IM_RFC4103_H

#include <ptlib.h>
#include <opal/buildopts.h>

#include <opal/mediafmt.h>
#include <im/t140.h>
#include <rtp/rtp.h>


class RFC4103Frame : public RTP_DataFrame
{
  public:
    RFC4103Frame();
    RFC4103Frame(const T140String & t140);
    void SetPayload(const T140String & t140);
};

class RFC4103Context : public PObject
{
  public:
    RFC4103Context(const OpalMediaFormat & fmt);
    RTP_DataFrameList ConvertToFrames(const T140String & body);

    OpalMediaFormat m_mediaFormat;
    PMutex m_mutex;
    WORD   m_sequence;
    DWORD  m_baseTimeStamp;
    PTime  m_baseTime;
};

#endif // OPAL_IM_RFC4103_H
