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
 * $Revision$
 * $Author$
 * $Date$
 */

#ifndef OPAL_CODEC_RFC2833_H
#define OPAL_CODEC_RFC2833_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include <opal/buildopts.h>

#include <rtp/rtp.h>

extern const char * OpalDefaultNTEString;

#if OPAL_T38_CAPABILITY
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
    PBoolean IsToneStart() const { return duration == 0; }

  protected:
    char     tone;
    unsigned duration;
    unsigned timestamp;
};

class OpalRTPConnection;

class OpalRFC2833Proto : public PObject {
    PCLASSINFO(OpalRFC2833Proto, PObject);
  public:
    OpalRFC2833Proto(
      OpalRTPConnection & conn,
      const PNotifier & receiveNotifier
    );
    ~OpalRFC2833Proto();

    virtual PBoolean SendToneAsync(
      char tone, 
      unsigned duration
    );

    virtual PBoolean BeginTransmit(
      char tone  ///<  DTMF tone code
    );

    virtual void OnStartReceive(
      char tone,
      unsigned timestamp
    );
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

    static PINDEX ASCIIToRFC2833(char tone);
    static char RFC2833ToASCII(PINDEX rfc2833);

  protected:
    void SendAsyncFrame();

    OpalRTPConnection & conn;

    PDECLARE_NOTIFIER(RTP_DataFrame, OpalRFC2833Proto, ReceivedPacket);
    PDECLARE_NOTIFIER(PTimer, OpalRFC2833Proto, ReceiveTimeout);
    PDECLARE_NOTIFIER(PTimer, OpalRFC2833Proto, AsyncTimeout);

    RTP_DataFrame::PayloadTypes payloadType;

    PMutex mutex;

    enum {
      ReceiveIdle,
      ReceiveActive,
      ReceiveEnding
    } receiveState;

    BYTE      receivedTone;

    //PBoolean  receiveComplete;
    //unsigned  receivedDuration;
    //unsigned  receiveTimestamp;

    PNotifier receiveNotifier;
    PTimer    receiveTimer;
    PNotifier receiveHandler;

    enum {
      TransmitIdle,
      TransmitActive,
      TransmitEnding1,
      TransmitEnding2,
      TransmitEnding3,
    } transmitState;
    BYTE      transmitCode;

    RTP_Session * rtpSession;
    PTimer        asyncTransmitTimer;
    PTimer        asyncDurationTimer;
    DWORD         transmitTimestamp;
    PBoolean      rewriteTransmitTimestamp;
    PTimeInterval asyncStart;
    unsigned      transmitDuration;
    unsigned      tonesReceived;
    DWORD         previousReceivedTimestamp;
};


#endif // OPAL_CODEC_RFC2833_H


/////////////////////////////////////////////////////////////////////////////
