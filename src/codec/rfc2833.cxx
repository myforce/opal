/*
 * rfc2833.cxx
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

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "rfc2833.h"
#endif

#include <codec/rfc2833.h>
#include <opal/connection.h>
#include <rtp/rtp.h>

static const char RFC2833Table1Events[] = "0123456789*#ABCD!";
const char * OpalDefaultNTEString = "0-15,32-49";

#if OPAL_T38FAX
const char * OpalDefaultNSEString = "192,193";
#endif

#define new PNEW


///////////////////////////////////////////////////////////////////////////////

OpalRFC2833Info::OpalRFC2833Info(char t, unsigned d, unsigned ts)
{
  tone = t;
  duration = d;
  timestamp = ts;
}


///////////////////////////////////////////////////////////////////////////////

OpalRFC2833Proto::OpalRFC2833Proto(OpalConnection & _conn, const PNotifier & rx)
  : conn(_conn), receiveNotifier(rx),
#ifdef _MSC_VER
#pragma warning(disable:4355)
#endif
    receiveHandler(PCREATE_NOTIFIER(ReceivedPacket))
#ifdef _MSC_VER
#pragma warning(default:4355)
#endif
{
  PTRACE(4, "RFC2833\tHandler created");

  payloadType = RTP_DataFrame::IllegalPayloadType;
  receiveComplete = PTrue;
  receiveTimestamp = 0;
  receiveTimer.SetNotifier(PCREATE_NOTIFIER(ReceiveTimeout));

  transmitState = TransmitIdle;

  asyncTransmitTimer.SetNotifier(PCREATE_NOTIFIER(AsyncTimeout));
  rtpSession = NULL;
}

OpalRFC2833Proto::~OpalRFC2833Proto()
{
  if (rtpSession != NULL)
    conn.ReleaseSession(1);
}

PBoolean OpalRFC2833Proto::SendToneAsync(char tone, unsigned duration)
{
  PWaitAndSignal m(mutex);

  if (rtpSession == NULL) {
    rtpSession = conn.UseSession(1);
    if (rtpSession == NULL) {
      PTRACE(4, "RFC2833\tNo RTP session suitable for RFC2833");
      return PFalse;
    }
  }

  if (tone != ' ') {
    if (!BeginTransmit(tone))
      return PFalse;
  }

  asyncDurationTimer = duration;
  asyncTransmitTimer.RunContinuous(30);
  SendAsyncFrame();

  return PTrue;
}

void OpalRFC2833Proto::SendAsyncFrame()
{
  PWaitAndSignal m(mutex);
  RTP_DataFrame frame;
  TransmitPacket(frame);
  if (rtpSession != NULL) {
    if (transmitTimestampSet)
      frame.SetTimestamp(transmitTimestamp);
    rtpSession->WriteOOBData(frame);
    if (!transmitTimestampSet)
      transmitTimestamp = frame.GetTimestamp();
  }
}

PBoolean OpalRFC2833Proto::BeginTransmit(char tone)
{
  PWaitAndSignal m(mutex);

  const char * theChar = strchr(RFC2833Table1Events, tone);
  if (theChar == NULL) {
    PTRACE(1, "RFC2833\tInvalid tone character.");
    return PFalse;
  }

  if (transmitState != TransmitIdle) {
    PTRACE(1, "RFC2833\tAttempt to send tone while currently sending.");
    return PFalse;
  }

  transmitCode  = (BYTE)(theChar-RFC2833Table1Events);
  transmitState = TransmitActive;
  transmitTimestampSet = PFalse;
  asyncStart    = 0;
  return PTrue;
}

void OpalRFC2833Proto::AsyncTimeout(PTimer &, INT)
{
  PWaitAndSignal m(mutex);
  if (asyncDurationTimer.IsRunning()) 
    SendAsyncFrame();
  else {
    EndTransmit();
    SendAsyncFrame();
    transmitTimestampSet = PFalse;
    asyncTransmitTimer.Stop();
  }
}

PBoolean OpalRFC2833Proto::EndTransmit()
{
  PWaitAndSignal m(mutex);

  if (transmitState != TransmitActive) {
    PTRACE(1, "RFC2833\tAttempt to stop send tone while not sending.");
    return PFalse;
  }

  transmitState = TransmitEnding;
  return PTrue;
}


void OpalRFC2833Proto::OnStartReceive(char tone)
{
  OpalRFC2833Info info(tone);
  receiveNotifier(info, 0);
}


void OpalRFC2833Proto::OnEndReceive(char tone, unsigned duration, unsigned timestamp)
{
  OpalRFC2833Info info(tone, duration, timestamp);
  receiveNotifier(info, 0);
}


void OpalRFC2833Proto::ReceivedPacket(RTP_DataFrame & frame, INT)
{
  if (frame.GetPayloadType() != payloadType)
    return;

  PWaitAndSignal m(mutex);

  if (frame.GetPayloadSize() < 4) {
    PTRACE(2, "RFC2833\tIgnoring packet, too small.");
    return;
  }

  const BYTE * payload = frame.GetPayloadPtr();
  if (payload[0] >= sizeof(RFC2833Table1Events)-1) {
    PTRACE(2, "RFC2833\tIgnoring packet, unsupported event.");
    return;
  }

  receivedTone = RFC2833Table1Events[payload[0]];
  receivedDuration = (payload[2]<<8) + payload[3];

  unsigned timestamp = frame.GetTimestamp();
  if (receiveTimestamp != timestamp) {
    PTRACE(3, "RFC2833\tReceived start tone=" << receivedTone);
    OnStartReceive(receivedTone);

    // Starting a new event.
    receiveTimestamp = timestamp;
    receiveComplete = PFalse;
    receiveTimer = 150;
  }
  else {
    receiveTimer = 150;
    if (receiveComplete) {
      PTRACE(3, "RFC2833\tIgnoring duplicate packet.");
      return;
    }
  }

  if ((payload[1]&0x80) == 0) {
    PTRACE(2, "RFC2833\tIgnoring packet, not end of event.");
    return;
  }

  receiveComplete = PTrue;
  receiveTimer.Stop();

  PTRACE(3, "RFC2833\tReceived end tone=" << receivedTone << " duration=" << receivedDuration);
  OnEndReceive(receivedTone, receivedDuration, receiveTimestamp);
}


void OpalRFC2833Proto::ReceiveTimeout(PTimer &, INT)
{
  PWaitAndSignal m(mutex);

  if (receiveComplete)
    return;

  receiveComplete = PTrue;
  PTRACE(3, "RFC2833\tTimeout tone=" << receivedTone << " duration=" << receivedDuration);
  OnEndReceive(receivedTone, receivedDuration, receiveTimestamp);
}


void OpalRFC2833Proto::TransmitPacket(RTP_DataFrame & frame)
{
  if (transmitState == TransmitIdle)
    return;

  PWaitAndSignal m(mutex);

  frame.SetPayloadType(payloadType);
  frame.SetPayloadSize(4);

  BYTE * payload = frame.GetPayloadPtr();
  payload[0] = transmitCode;

  payload[1] = 7;  // Volume
  if (transmitState == TransmitEnding) {
    payload[1] |= 0x80;
    transmitState = TransmitIdle;
  }

  unsigned duration;
  if (asyncStart != PTimeInterval(0)) 
    duration = (PTimer::Tick() - asyncStart).GetInterval();
  else {
    duration = 0;
	  frame.SetMarker(PTrue);
    asyncStart = PTimer::Tick();
  }

  payload[2] = (BYTE)(duration>>8);
  payload[3] = (BYTE) duration    ;

  PTRACE(4, "RFC2833\tSending packet with duration " << duration << " for code " << (int)transmitCode);
}

/////////////////////////////////////////////////////////////////////////////
