/*
 * rfc2833.h
 *
 * Open Phone Abstraction Library (OPAL)
 * Formally known as the Open H323 project.
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
 * The Original Code is Open Phone Abstraction Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 */

#ifndef __OPAL_RFC2833_H
#define __OPAL_RFC2833_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


#include <rtp/rtp.h>

extern const char * OpalDefaultNTEString;

#if OPAL_T38FAX
extern const char * OpalDefaultNSEString;
#endif

///////////////////////////////////////////////////////////////////////////////

class OpalRFC2833Info : public PObject {
    PCLASSINFO(OpalRFC2833Info, PObject);
  public:
    OpalRFC2833Info(
      char tone,
      unsigned duration = 0,
      unsigned timestamp = 0
    );

    char GetTone() const { return tone; }
    unsigned GetDuration() const { return duration; }
    unsigned GetTimestamp() const { return timestamp; }
    BOOL IsToneStart() const { return duration == 0; }

  protected:
    char     tone;
    unsigned duration;
    unsigned timestamp;
};

class OpalConnection;

class OpalRFC2833Proto : public PObject {
    PCLASSINFO(OpalRFC2833Proto, PObject);
  public:
    OpalRFC2833Proto(
      OpalConnection & conn,
      const PNotifier & receiveNotifier
    );
    ~OpalRFC2833Proto();

    virtual BOOL SendToneAsync(
      char tone, 
      unsigned duration
    );

    virtual BOOL BeginTransmit(
      char tone  ///<  DTMF tone code
    );
    virtual BOOL EndTransmit();

    virtual void OnStartReceive(
      char tone
    );
    virtual void OnEndReceive(
      char tone,
      unsigned duration,
      unsigned timestamp
    );

    RTP_DataFrame::PayloadTypes GetPayloadType() const { return payloadType; }

    void SetPayloadType(
      RTP_DataFrame::PayloadTypes type ///<  new payload type
    ) { payloadType = type; }

    const PNotifier & GetReceiveHandler() const { return receiveHandler; }

  protected:
    void SendAsyncFrame();
    void TransmitPacket(RTP_DataFrame & frame);

    OpalConnection & conn;

    PDECLARE_NOTIFIER(RTP_DataFrame, OpalRFC2833Proto, ReceivedPacket);
    PDECLARE_NOTIFIER(PTimer, OpalRFC2833Proto, ReceiveTimeout);
    PDECLARE_NOTIFIER(PTimer, OpalRFC2833Proto, AsyncTimeout);

    RTP_DataFrame::PayloadTypes payloadType;

    PMutex mutex;

    PNotifier receiveNotifier;
    BOOL      receiveComplete;
    BYTE      receivedTone;
    unsigned  receivedDuration;
    unsigned  receiveTimestamp;
    PTimer    receiveTimer;
    PNotifier receiveHandler;

    enum {
      TransmitIdle,
      TransmitActive,
      TransmitEnding
    }         transmitState;
    BYTE      transmitCode;

    RTP_Session * rtpSession;
    PTimer        asyncTransmitTimer;
    PTimer        asyncDurationTimer;
    DWORD         transmitTimestamp;
    BOOL          transmitTimestampSet;
    PTimeInterval asyncStart;
};


#endif // __OPAL_RFC2833_H


/////////////////////////////////////////////////////////////////////////////
