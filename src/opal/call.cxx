/*
 * call.cxx
 *
 * Call management
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

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "call.h"
#endif

#include <opal/call.h>

#include <opal/manager.h>
#include <opal/patch.h>
#include <opal/transcoders.h>


#define new PNEW


/////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#pragma warning(disable:4355)
#endif

OpalCall::OpalCall(OpalManager & mgr)
  : manager(mgr)
  , myToken(mgr.GetNextCallToken())
  , isEstablished(FALSE)
  , isClearing(FALSE)
  , callEndReason(OpalConnection::NumCallEndReasons)
  , endCallSyncPoint(NULL)
{
  manager.activeCalls.SetAt(myToken, this);

  connectionsActive.DisallowDeleteObjects();

  PTRACE(3, "Call\tCreated " << *this);
}

#ifdef _MSC_VER
#pragma warning(default:4355)
#endif


OpalCall::~OpalCall()
{
  PTRACE(3, "Call\t" << *this << " destroyed.");

  manager.GetRecordManager().Close(myToken);
}


void OpalCall::PrintOn(ostream & strm) const
{
  strm << "Call[" << myToken << ']';
}


void OpalCall::OnEstablishedCall()
{
  manager.OnEstablishedCall(*this);
}


void OpalCall::SetCallEndReason(OpalConnection::CallEndReason reason)
{
  // Only set reason if not already set to something
  if (callEndReason == OpalConnection::NumCallEndReasons)
    callEndReason = reason;
}


void OpalCall::Clear(OpalConnection::CallEndReason reason, PSyncPoint * sync)
{
  PTRACE(3, "Call\tClearing " << (sync != NULL ? "(sync) " : "") << *this << " reason=" << reason);

  if (!LockReadWrite())
    return;

  isClearing = TRUE;

  SetCallEndReason(reason);

  if (sync != NULL && !connectionsActive.IsEmpty()) {
    // only set the sync point if it is NULL
    if (endCallSyncPoint == NULL)
      endCallSyncPoint = sync;
    else {
      PAssertAlways("Can only have one thread doing ClearCallSynchronous");
    }
  }

  UnlockReadWrite();

  for (PSafePtr<OpalConnection> connection(connectionsActive, PSafeReadOnly); connection != NULL; ++connection)
    connection->Release(reason);
}


void OpalCall::OnCleared()
{
  manager.OnClearedCall(*this);

  if (endCallSyncPoint != NULL)
    endCallSyncPoint->Signal();
}


BOOL OpalCall::OnSetUp(OpalConnection & connection)
{
  PTRACE(3, "Call\tOnSetUp " << connection);

  BOOL ok = FALSE;

  if (isClearing || !LockReadWrite())
    return FALSE;

  partyA = connection.GetRemotePartyName();

  UnlockReadWrite();

  for (PSafePtr<OpalConnection> conn(connectionsActive, PSafeReadOnly); conn != NULL; ++conn) {
    if (conn != &connection) {
      if (conn->SetUpConnection()) {
        ok = conn->OnSetUpConnection();
      }
    }
  }

  return ok;
}


BOOL OpalCall::OnAlerting(OpalConnection & connection)
{
  PTRACE(3, "Call\tOnAlerting " << connection);

  if (isClearing || !LockReadWrite())
    return FALSE;

  partyB = connection.GetRemotePartyName();

  UnlockReadWrite();


  BOOL hasMedia = connection.GetMediaStream(OpalMediaFormat::DefaultAudioSessionID, TRUE) != NULL;

  BOOL ok = FALSE;

  for (PSafePtr<OpalConnection> conn(connectionsActive, PSafeReadOnly); conn != NULL; ++conn) {
    if (conn != &connection) {
      if (conn->SetAlerting(connection.GetRemotePartyName(), hasMedia))
        ok = TRUE;
    }
  }

  return ok;
}

OpalConnection::AnswerCallResponse OpalCall::OnAnswerCall(OpalConnection & /*connection*/,
                                                          const PString & /*caller*/)
{
  return OpalConnection::AnswerCallPending;
}


BOOL OpalCall::OnConnected(OpalConnection & connection)
{
  PTRACE(3, "Call\tOnConnected " << connection);

  if (isClearing || !LockReadOnly())
    return FALSE;

  BOOL ok = connectionsActive.GetSize() == 1 && !partyB.IsEmpty();

  UnlockReadOnly();

  if (ok) {
    if (!manager.MakeConnection(*this, partyB))
      connection.Release(OpalConnection::EndedByNoUser);
    return OnSetUp(connection);
  }

  BOOL createdOne = FALSE;

  if (!LockReadOnly())
    return FALSE;

  for (PSafePtr<OpalConnection> conn(connectionsActive, PSafeReadOnly); conn != NULL; ++conn) {
    if (conn != &connection) {
      if (conn->SetConnected())
        ok = TRUE;
    }

    OpalMediaFormatList formats = GetMediaFormats(*conn, TRUE);
    if (OpenSourceMediaStreams(*conn, formats, OpalMediaFormat::DefaultAudioSessionID))
      createdOne = TRUE;
    if (OpenSourceMediaStreams(*conn, formats, OpalMediaFormat::DefaultVideoSessionID))
      createdOne = TRUE;
    if (OpenSourceMediaStreams(*conn, formats, OpalMediaFormat::DefaultDataSessionID))
      createdOne = TRUE;
  }

  UnlockReadOnly();
  
  if (ok && createdOne) {
    for (PSafePtr<OpalConnection> conn(connectionsActive, PSafeReadOnly); conn != NULL; ++conn)
      conn->StartMediaStreams();
  }

  return ok;
}


BOOL OpalCall::OnEstablished(OpalConnection & PTRACE_PARAM(connection))
{
  PTRACE(3, "Call\tOnEstablished " << connection);

  PSafeLockReadWrite lock(*this);
  if (isClearing || !lock.IsLocked())
    return FALSE;

  if (isEstablished)
    return TRUE;

  if (connectionsActive.GetSize() < 2)
    return FALSE;

  for (PSafePtr<OpalConnection> conn(connectionsActive, PSafeReference); conn != NULL; ++conn) {
    if (conn->GetPhase() != OpalConnection::EstablishedPhase)
      return FALSE;
  }

  isEstablished = TRUE;
  OnEstablishedCall();

  return TRUE;
}


PSafePtr<OpalConnection> OpalCall::GetOtherPartyConnection(const OpalConnection & connection) const
{
  PTRACE(3, "Call\tGetOtherPartyConnection " << connection);

  for (PSafePtr<OpalConnection> conn(connectionsActive, PSafeReadOnly); conn != NULL; ++conn) {
    if (conn != &connection)
      return conn;
  }

  return NULL;
}


OpalMediaFormatList OpalCall::GetMediaFormats(const OpalConnection & connection,
                                              BOOL includeSpecifiedConnection)
{
  OpalMediaFormatList commonFormats;

  BOOL first = TRUE;

  for (PSafePtr<OpalConnection> conn(connectionsActive, PSafeReadOnly); conn != NULL; ++conn) {
    if (includeSpecifiedConnection || conn != &connection) {
      OpalMediaFormatList possibleFormats = OpalTranscoder::GetPossibleFormats(conn->GetMediaFormats());
      if (first) {
        commonFormats = possibleFormats;
        first = FALSE;
      }
      else {
        // Want intersection of the possible formats for all connections.
        for (PINDEX i = 0; i < commonFormats.GetSize(); i++) {
          if (possibleFormats.GetValuesIndex(commonFormats[i]) == P_MAX_INDEX)
            commonFormats.RemoveAt(i--);
        }
      }
    }
  }

  connection.AdjustMediaFormats(commonFormats);

  PTRACE(4, "Call\tGetMediaFormats for " << connection << '\n'
         << setfill('\n') << commonFormats << setfill(' '));

  return commonFormats;
}

BOOL OpalCall::OpenSourceMediaStreams(const OpalConnection & connection,
                                      const OpalMediaFormatList & mediaFormats,
                                      unsigned sessionID)
{
  PTRACE(3, "Call\tOpenSourceMediaStreams for session " << sessionID
         << " with media " << setfill(',') << mediaFormats << setfill(' '));

  BOOL startedOne = FALSE;

  OpalMediaFormatList adjustableMediaFormats;
  // Keep the media formats for the session ID
  for (PINDEX i = 0; i < mediaFormats.GetSize(); i++) {
    if (mediaFormats[i].GetDefaultSessionID() == sessionID)
      adjustableMediaFormats += mediaFormats[i];
  }

  if (adjustableMediaFormats.GetSize() == 0)
    return FALSE;

  // if there is already a connection with an open stream then reorder the 
  // media formats to match that connection so we get symmetric codecs
  if (connectionsActive.GetSize() > 0) {
    OpalMediaStream * strm = connection.GetMediaStream(sessionID, TRUE);
    if (strm != NULL)
      adjustableMediaFormats.Reorder(strm->GetMediaFormat().GetName());
  }
  
  for (PSafePtr<OpalConnection> conn(connectionsActive, PSafeReadOnly); conn != NULL; ++conn) {
    if (conn != &connection) {
      if (conn->OpenSourceMediaStream(adjustableMediaFormats, sessionID)) 
        startedOne = TRUE;
    }
  }

  return startedOne;
}


void OpalCall::CloseMediaStreams()
{
  for (PSafePtr<OpalConnection> conn(connectionsActive, PSafeReadOnly); conn != NULL; ++conn) 
    conn->CloseMediaStreams();
}


void OpalCall::RemoveMediaStreams()
{
  for (PSafePtr<OpalConnection> conn(connectionsActive, PSafeReadOnly); conn != NULL; ++conn) 
    conn->RemoveMediaStreams();
}


BOOL OpalCall::PatchMediaStreams(const OpalConnection & connection,
                                 OpalMediaStream & source)
{
  PTRACE(3, "Call\tPatchMediaStreams " << connection);

  PSafeLockReadOnly lock(*this);
  if (isClearing || !lock.IsLocked())
    return FALSE;

  OpalMediaPatch * patch = NULL;

  {
    // handle RTP payload translation
    RTP_DataFrame::PayloadMapType map;
    for (PSafePtr<OpalConnection> conn(connectionsActive, PSafeReadOnly); conn != NULL; ++conn) {
      if (conn != &connection) {
        map = conn->GetRTPPayloadMap();
      }
    }
    if (map.size() == 0)
      map = connection.GetRTPPayloadMap();

    for (PSafePtr<OpalConnection> conn(connectionsActive, PSafeReadOnly); conn != NULL; ++conn) {
      if (conn != &connection) {
        OpalMediaStream * sink = conn->OpenSinkMediaStream(source);
        if (sink == NULL)
          return FALSE;
        if (source.RequiresPatch()) {
          if (patch == NULL) {
            patch = manager.CreateMediaPatch(source, source.RequiresPatchThread());
            if (patch == NULL)
              return FALSE;
          }
          patch->AddSink(sink, map);
        }
      }
    }
  }

  // if a patch was created, make sure the callback is called
  if (patch) {
    for (PSafePtr<OpalConnection> conn(connectionsActive, PSafeReadOnly); conn != NULL; ++conn) 
      conn->OnPatchMediaStream(conn == &connection, *patch);
  }
  
  return TRUE;
}


void OpalCall::OnRTPStatistics(const OpalConnection & /*connection*/, const RTP_Session & /*session*/)
{
}


BOOL OpalCall::IsMediaBypassPossible(const OpalConnection & connection,
                                     unsigned sessionID) const
{
  PTRACE(3, "Call\tIsMediaBypassPossible " << connection << " session " << sessionID);

  BOOL ok = FALSE;

  for (PSafePtr<OpalConnection> conn(connectionsActive, PSafeReadOnly); conn != NULL; ++conn) {
    if (conn != &connection) {
      ok = manager.IsMediaBypassPossible(connection, *conn, sessionID);
      break;
    }
  }

  return ok;
}


void OpalCall::OnUserInputString(OpalConnection & connection, const PString & value)
{
  for (PSafePtr<OpalConnection> conn(connectionsActive, PSafeReadOnly); conn != NULL; ++conn) {
    if (conn != &connection)
      conn->SendUserInputString(value);
    else
      connection.SetUserInput(value);
  }
}


void OpalCall::OnUserInputTone(OpalConnection & connection,
                               char tone,
                               int duration)
{
  for (PSafePtr<OpalConnection> conn(connectionsActive, PSafeReadOnly); conn != NULL; ++conn) {
    if (conn != &connection)
      conn->SendUserInputTone(tone, duration);
    else {
      if (duration > 0 && tone != ' ')
        connection.OnUserInputString(tone);
    }
  }
}


void OpalCall::OnReleased(OpalConnection & connection)
{
  PTRACE(3, "Call\tOnReleased " << connection);

  SetCallEndReason(connection.GetCallEndReason());

  connectionsActive.Remove(&connection);

  // A call will evaporate when one connection left, at some point this is
  // to be changes so can have "parked" connections. 
  if(connectionsActive.GetSize() == 1)
  {
    PSafePtr<OpalConnection> last = connectionsActive.GetAt(0, PSafeReference);
    if (last != NULL)
      last->Release(connection.GetCallEndReason());
  }
  if (connectionsActive.IsEmpty()) {
    OnCleared();
    manager.activeCalls.RemoveAt(GetToken());
  }
}

BOOL OpalCall::StartRecording(const PFilePath & fn)
{
  // create the mixer entry
  if (!manager.GetRecordManager().Open(myToken, fn))
    return FALSE;

  // tell each connection to start sending data
  for (PSafePtr<OpalConnection> connection(connectionsActive, PSafeReadWrite); connection != NULL; ++connection)
    connection->EnableRecording();

  return TRUE;
}

void OpalCall::StopRecording()
{
  // tell each connection to stop sending data
  for (PSafePtr<OpalConnection> connection(connectionsActive, PSafeReadWrite); connection != NULL; ++connection)
    connection->DisableRecording();

  manager.GetRecordManager().Close(myToken);
}

void OpalCall::OnStopRecordAudio(const PString & callToken)
{
  manager.GetRecordManager().CloseStream(myToken, callToken);
}

/////////////////////////////////////////////////////////////////////////////
