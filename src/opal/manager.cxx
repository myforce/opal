/*
 * manager.cxx
 *
 * Media channels abstraction
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
 * $Log: manager.cxx,v $
 * Revision 1.2023  2003/03/07 08:13:42  robertj
 * Fixed validation of "protocol:" part of address in routing system.
 *
 * Revision 2.21  2003/03/06 03:57:47  robertj
 * IVR support (work in progress) requiring large changes everywhere.
 *
 * Revision 2.20  2003/01/07 04:39:53  robertj
 * Updated to OpenH323 v1.11.2
 *
 * Revision 2.19  2002/11/10 11:33:19  robertj
 * Updated to OpenH323 v1.10.3
 *
 * Revision 2.18  2002/09/06 02:44:30  robertj
 * Added routing table system to route calls by regular expressions.
 *
 * Revision 2.17  2002/09/04 06:01:49  robertj
 * Updated to OpenH323 v1.9.6
 *
 * Revision 2.16  2002/07/01 04:56:33  robertj
 * Updated to OpenH323 v1.9.1
 *
 * Revision 2.15  2002/06/16 02:25:11  robertj
 * Fixed memory leak of garbage collector, thanks Ted Szoczei
 *
 * Revision 2.14  2002/04/17 07:19:38  robertj
 * Fixed incorrect trace message for OnReleased
 *
 * Revision 2.13  2002/04/10 08:15:31  robertj
 * Added functions to set ports.
 *
 * Revision 2.12  2002/04/08 02:40:13  robertj
 * Fixed issues with using double originate call, eg from simple app command line.
 *
 * Revision 2.11  2002/02/19 07:49:47  robertj
 * Restructured media bypass functions to fix problems with RFC2833.
 *
 * Revision 2.10  2002/02/11 07:42:17  robertj
 * Added media bypass for streams between compatible protocols.
 *
 * Revision 2.9  2002/01/22 05:12:51  robertj
 * Revamp of user input API triggered by RFC2833 support
 *
 * Revision 2.8  2002/01/14 06:35:58  robertj
 * Updated to OpenH323 v1.7.9
 *
 * Revision 2.7  2001/12/07 08:56:43  robertj
 * Renamed RTP to be more general UDP port base, and TCP to be IP.
 *
 * Revision 2.6  2001/11/14 01:31:55  robertj
 * Corrected placement of adjusting media format list.
 *
 * Revision 2.5  2001/11/13 06:25:56  robertj
 * Changed SetUpConnection() so returns BOOL as returning
 *   pointer to connection is not useful.
 *
 * Revision 2.4  2001/10/04 00:44:26  robertj
 * Added function to remove wildcard from list.
 *
 * Revision 2.3  2001/08/23 05:51:17  robertj
 * Completed implementation of codec reordering.
 *
 * Revision 2.2  2001/08/17 08:25:41  robertj
 * Added call end reason for whole call, not just connection.
 * Moved call end reasons enum from OpalConnection to global.
 *
 * Revision 2.1  2001/08/01 05:44:41  robertj
 * Added function to get all media formats possible in a call.
 *
 * Revision 2.0  2001/07/27 15:48:25  robertj
 * Conversion of OpenH323 to Open Phone Abstraction Library (OPAL)
 *
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "manager.h"
#endif

#include <opal/manager.h>

#include <opal/patch.h>
#include <opal/mediastrm.h>
#include "../../version.h"


#ifndef IPTOS_PREC_CRITIC_ECP
#define IPTOS_PREC_CRITIC_ECP (5 << 5)
#endif

#ifndef IPTOS_LOWDELAY
#define IPTOS_LOWDELAY 0x10
#endif


static const char * const DefaultMediaFormatOrder[] = {
  OPAL_G7231_6k3,
  OPAL_G729B,
  OPAL_G729AB,
  OPAL_G729,
  OPAL_G729A,
  OPAL_GSM0610,
  OPAL_G728,
  OPAL_G711_ULAW_64K,
  OPAL_G711_ALAW_64K
};

#define new PNEW


/////////////////////////////////////////////////////////////////////////////

PString OpalGetVersion()
{
#define AlphaCode   "alpha"
#define BetaCode    "beta"
#define ReleaseCode "."

  return psprintf("%u.%u%s%u", MAJOR_VERSION, MINOR_VERSION, BUILD_TYPE, BUILD_NUMBER);
}


unsigned OpalGetMajorVersion()
{
  return MAJOR_VERSION;
}

unsigned OpalGetMinorVersion()
{
  return MINOR_VERSION;
}

unsigned OpalGetBuildNumber()
{
  return BUILD_NUMBER;
}



/////////////////////////////////////////////////////////////////////////////

OpalManager::OpalManager()
  : mediaFormatOrder(PARRAYSIZE(DefaultMediaFormatOrder), DefaultMediaFormatOrder)
{
  autoStartReceiveVideo = autoStartTransmitVideo = TRUE;

  rtpIpPorts.current = rtpIpPorts.base = 5000;
  rtpIpPorts.max = 5199;

  // use dynamic port allocation by default
  tcpPorts.current = tcpPorts.base = tcpPorts.max = 0;
  udpPorts.current = udpPorts.base = udpPorts.max = 0;

#ifdef _WIN32
  rtpIpTypeofService = IPTOS_PREC_CRITIC_ECP|IPTOS_LOWDELAY;
#else
  // Don't use IPTOS_PREC_CRITIC_ECP on Unix platforms as then need to be root
  rtpIpTypeofService = IPTOS_LOWDELAY;
#endif

  minAudioJitterDelay = 50;  // milliseconds
  maxAudioJitterDelay = 250; // milliseconds

  lastCallTokenID = 1;

  callsActive.DisallowDeleteObjects();
  collectingGarbage = TRUE;

  garbageCollector = PThread::Create(PCREATE_NOTIFIER(GarbageMain), 0,
                                     PThread::NoAutoDeleteThread,
                                     PThread::LowPriority,
                                     "OpalGarbage");

  PTRACE(3, "OpalMan\tCreated manager.");
}


OpalManager::~OpalManager()
{
  // Clear any pending calls on this endpoint
  ClearAllCalls();

  // Shut down the cleaner thread
  collectingGarbage = FALSE;
  garbageCollectFlag.Signal();
  garbageCollector->WaitForTermination();

  // Clean up any calls that the cleaner thread missed
  GarbageCollection();

  delete garbageCollector;

  // Kill off the endpoints, could wait till compiler generated destructor but
  // prefer to keep the PTRACE's in sequence.
  endpoints.RemoveAll();

  PTRACE(3, "OpalMan\tDeleted manager.");
}


void OpalManager::AttachEndPoint(OpalEndPoint * endpoint)
{
  PAssertNULL(endpoint);

  inUseFlag.Wait();

  if (endpoints.GetObjectsIndex(endpoint) == P_MAX_INDEX)
    endpoints.Append(endpoint);

  inUseFlag.Signal();
}


void OpalManager::RemoveEndPoint(OpalEndPoint * endpoint)
{
  inUseFlag.Wait();
  endpoints.Remove(endpoint);
  inUseFlag.Signal();
}


OpalEndPoint * OpalManager::FindEndPoint(const PString & prefix)
{
  PWaitAndSignal mutex(inUseFlag);

  for (PINDEX i = 0; i < endpoints.GetSize(); i++) {
    if (endpoints[i].GetPrefixName() *= prefix)
      return &endpoints[i];
  }

  return NULL;
}


BOOL OpalManager::SetUpCall(const PString & partyA,
                            const PString & partyB,
                            PString & token)
{
  PTRACE(3, "OpalMan\tSet up call from " << partyA << " to " << partyB);

  OpalCall * call = CreateCall();
  token = call->GetToken();

  call->SetPartyB(partyB);

  if (MakeConnection(*call, partyA))
    return TRUE;

  call->Clear();
  return FALSE;
}


void OpalManager::OnEstablishedCall(OpalCall & /*call*/)
{
}


BOOL OpalManager::HasCall(const PString & token)
{
  PWaitAndSignal wait(callsMutex);

  return FindCallWithoutLocks(token) != NULL;
}


OpalCall * OpalManager::FindCallWithLock(const PString & token)
{
  PWaitAndSignal wait(callsMutex);

  OpalCall * call = FindCallWithoutLocks(token);
  if (call == NULL)
    return NULL;

  call->Lock();
  return call;
}


OpalCall * OpalManager::FindCallWithoutLocks(const PString & token)
{
  if (token.IsEmpty())
    return NULL;

  OpalCall * conn_ptr = callsActive.GetAt(token);
  if (conn_ptr != NULL)
    return conn_ptr;

  return NULL;
}


BOOL OpalManager::ClearCall(const PString & token,
                            OpalConnection::CallEndReason reason,
                            PSyncPoint * sync)
{
  /*The hugely multi-threaded nature of the OpalCall objects means that
    to avoid many forms of race condition, a call is cleared by moving it from
    the "active" call dictionary to a list of calls to be cleared that will be
    processed by a background thread specifically for the purpose of cleaning
    up cleared calls. So that is all that this function actually does.
    The real work is done in the OpalGarbageCollector thread.
   */

  PWaitAndSignal wait(callsMutex);

  // Find the call by token, callid or conferenceid
  OpalCall * call = FindCallWithoutLocks(token);
  if (call == NULL)
    return FALSE;

  call->Clear(reason);

  if (sync != NULL)
    sync->Wait();

  return TRUE;
}


void OpalManager::ClearAllCalls(OpalConnection::CallEndReason reason, BOOL wait)
{
  /*The hugely multi-threaded nature of the OpalCall objects means that
    to avoid many forms of race condition, a call is cleared by moving it from
    the "active" call dictionary to a list of calls to be cleared that will be
    processed by a background thread specifically for the purpose of cleaning
    up cleared calls. So that is all that this function actually does.
    The real work is done in the OpalGarbageCollector thread.
   */

  callsMutex.Wait();

  // Remove all calls from the active list first
  PINDEX i;
  for (i = 0; i < callsActive.GetSize(); i++)
    callsActive.GetDataAt(i).Clear(reason);

  callsMutex.Signal();

  SignalGarbageCollector();

  if (wait)
    allCallsCleared.Wait();
}


void OpalManager::OnClearedCall(OpalCall & /*call*/)
{
}


OpalCall * OpalManager::CreateCall()
{
  return new OpalCall(*this);
}


void OpalManager::DestroyCall(OpalCall * call)
{
  delete call;
}


PString OpalManager::GetNextCallToken()
{
  PString token;
  callsMutex.Wait();
  token.sprintf("%u", lastCallTokenID++);
  callsMutex.Signal();
  return token;
}


void OpalManager::AttachCall(OpalCall * call)
{
  callsMutex.Wait();
  callsActive.SetAt(call->GetToken(), call);
  callsMutex.Signal();
}


BOOL OpalManager::MakeConnection(OpalCall & call, const PString & remoteParty)
{
  PTRACE(3, "OpalMan\tSet up connection to \"" << remoteParty << '"');

  if (endpoints.IsEmpty())
    return FALSE;

  PCaselessString epname = remoteParty.Left(remoteParty.Find(':'));
  if (epname.IsEmpty())
    epname = endpoints[0].GetPrefixName();

  for (PINDEX i = 0; i < endpoints.GetSize(); i++) {
    if (epname == endpoints[i].GetPrefixName()) {
      if (endpoints[i].MakeConnection(call, remoteParty, NULL))
        return TRUE;
    }
  }

  return FALSE;
}


BOOL OpalManager::OnIncomingConnection(OpalConnection & connection)
{
  PTRACE(3, "OpalMan\tOn incoming connection " << connection);

  OpalCall & call = connection.GetCall();
  if (call.GetConnectionCount() > 1)
    return TRUE;

  PString destinationAddress = OnRouteConnection(connection);
  if (destinationAddress.IsEmpty())
    return FALSE;

  return MakeConnection(call, destinationAddress);
}


PString OpalManager::OnRouteConnection(OpalConnection & connection)
{
  PString addr = connection.GetDestinationAddress();

  // No address, fail call
  if (addr.IsEmpty())
    return PString::Empty();

  // Have explicit protocol defined, so use that
  PINDEX colon = addr.Find(':');
  if (colon != P_MAX_INDEX && FindEndPoint(addr.Left(colon)) != NULL)
    return addr;

  // No routes specified, just return what we've got so far
  if (routeTable.IsEmpty())
    return addr;

  return ApplyRouteTable(connection.GetEndPoint().GetPrefixName(), addr);
}


void OpalManager::OnAlerting(OpalConnection & connection)
{
  PTRACE(3, "OpalMan\tOnAlerting " << connection);

  connection.GetCall().OnAlerting(connection);
}


void OpalManager::OnConnected(OpalConnection & connection)
{
  PTRACE(3, "OpalMan\tOnConnected " << connection);

  connection.GetCall().OnConnected(connection);
}


BOOL OpalManager::OnReleased(OpalConnection & connection)
{
  PTRACE(3, "OpalMan\tOnReleased " << connection);

  connection.GetCall().OnReleased(connection);
  return TRUE;
}


void OpalManager::AdjustMediaFormats(const OpalConnection & /*connection*/,
                                     OpalMediaFormatList & mediaFormats) const
{
  mediaFormats.Remove(mediaFormatMask);
  mediaFormats.Reorder(mediaFormatOrder);
}


BOOL OpalManager::IsMediaBypassPossible(const OpalConnection & source,
                                        const OpalConnection & destination,
                                        unsigned sessionID) const
{
  PTRACE(3, "OpalMan\tIsMediaBypassPossible: session " << sessionID);

  return source.IsMediaBypassPossible(sessionID) &&
         destination.IsMediaBypassPossible(sessionID);
}


BOOL OpalManager::OnOpenMediaStream(OpalConnection & connection,
                                    OpalMediaStream & stream)
{
  PTRACE(3, "OpalMan\tOnOpenMediaStream " << connection << ',' << stream);

  if (stream.IsSource() && stream.RequiresPatchThread())
    return connection.GetCall().PatchMediaStreams(connection, stream);

  return TRUE;
}


void OpalManager::OnClosedMediaStream(const OpalMediaStream & /*channel*/)
{
}


OpalMediaPatch * OpalManager::CreateMediaPatch(OpalMediaStream & source)
{
  return new OpalMediaPatch(source);
}


void OpalManager::DestroyMediaPatch(OpalMediaPatch * patch)
{
  delete patch;
}


BOOL OpalManager::OnStartMediaPatch(const OpalMediaPatch & /*patch*/)
{
  return TRUE;
}


void OpalManager::OnUserInputString(OpalConnection & connection,
                                    const PString & value)
{
  connection.GetCall().OnUserInputString(connection, value);
}


void OpalManager::OnUserInputTone(OpalConnection & connection,
                                  char tone,
                                  int duration)
{
  connection.GetCall().OnUserInputTone(connection, tone, duration);
}


OpalT120Protocol * OpalManager::CreateT120ProtocolHandler(const OpalConnection & ) const
{
  return NULL;
}


OpalT38Protocol * OpalManager::CreateT38ProtocolHandler(const OpalConnection & ) const
{
  return NULL;
}


OpalManager::RouteEntry::RouteEntry(const PString & pat, const PString dest)
  : pattern(pat),
    destination(dest),
    regex('^'+pat+'$')
{
}


void OpalManager::RouteEntry::PrintOn(ostream & strm) const
{
  strm << pattern << '=' << destination;
}


BOOL OpalManager::AddRouteEntry(const PString & spec)
{
  if (spec[0] == '#') // Comment
    return FALSE;

  if (spec[0] == '@') { // Load from file
    PTextFile file;
    if (!file.Open(spec.Mid(1), PFile::ReadOnly)) {
      PTRACE(1, "OpalMan\tCould not open route file \"" << file.GetFilePath() << '"');
      return FALSE;
    }
    PTRACE(4, "OpalMan\tAdding routes from file \"" << file.GetFilePath() << '"');
    BOOL ok = FALSE;
    PString line;
    while (file.good()) {
      file >> line;
      if (AddRouteEntry(line))
        ok = TRUE;
    }
    return ok;
  }

  PINDEX equal = spec.Find('=');
  if (equal == P_MAX_INDEX) {
    PTRACE(2, "OpalMan\tInvalid route table entry: \"" << spec << '"');
    return FALSE;
  }

  RouteEntry * entry = new RouteEntry(spec.Left(equal).Trim(), spec.Mid(equal+1).Trim());
  if (entry->regex.GetErrorCode() != PRegularExpression::NoError) {
    PTRACE(1, "OpalMan\tIllegal regular expression in route table entry: \"" << spec << '"');
    delete entry;
    return FALSE;
  }

  PTRACE(4, "OpalMan\tAdded route \"" << *entry << '"');
  routeTableMutex.Wait();
  routeTable.Append(entry);
  routeTableMutex.Signal();
  return TRUE;
}


BOOL OpalManager::SetRouteTable(const PStringArray & specs)
{
  BOOL ok = FALSE;

  routeTableMutex.Wait();
  routeTable.RemoveAll();

  for (PINDEX i = 0; i < specs.GetSize(); i++) {
    if (AddRouteEntry(specs[i].Trim()))
      ok = TRUE;
  }

  routeTableMutex.Signal();

  return ok;
}


void OpalManager::SetRouteTable(const RouteTable & table)
{
  routeTableMutex.Wait();
  routeTable = table;
  routeTable.MakeUnique();
  routeTableMutex.Signal();
}


PString OpalManager::ApplyRouteTable(const PString & proto, const PString & addr)
{
  PWaitAndSignal mutex(routeTableMutex);

  PString destination;
  PString search = proto + ':' + addr;
  PTRACE(4, "OpalMan\tSearching for route \"" << search << '"');
  for (PINDEX i = 0; i < routeTable.GetSize(); i++) {
    PINDEX pos;
    if (routeTable[i].regex.Execute(search, pos)) {
      destination = routeTable[i].destination;
      break;
    }
  }

  if (destination.IsEmpty())
    return PString::Empty();

  destination.Replace("<da>", addr);

  PINDEX pos;
  if ((pos = destination.Find("<dn>")) != P_MAX_INDEX)
    destination.Splice(addr.Left(::strspn(addr, "0123456789*#")), pos, 4);

  if ((pos = destination.Find("<!dn>")) != P_MAX_INDEX)
    destination.Splice(addr.Mid(::strspn(addr, "0123456789*#")), pos, 5);

  // Do meta character substitutions
  if ((pos = destination.Find("<dn2ip>")) != P_MAX_INDEX) {
    PStringStream route;
    PStringArray stars = addr.Tokenise('*');
    switch (stars.GetSize()) {
      case 0 :
      case 1 :
      case 2 :
      case 3 :
        route << addr;
        break;

      case 4 :
        route << stars[0] << '.' << stars[1] << '.'<< stars[2] << '.'<< stars[3];
        break;

      case 5 :
        route << stars[0] << '@'
              << stars[1] << '.' << stars[2] << '.'<< stars[3] << '.'<< stars[4];
        break;

      default :
        route << stars[0] << '@'
              << stars[1] << '.' << stars[2] << '.'<< stars[3] << '.'<< stars[4]
              << ':' << stars[5];
        break;
    }
    destination.Splice(route, pos, 7);
  }

  return destination;
}


void OpalManager::SetAudioJitterDelay(unsigned minDelay, unsigned maxDelay)
{
  PAssert(minDelay <= 10000 && maxDelay <= 10000, PInvalidParameter);

  if (minDelay < 10)
    minDelay = 10;
  minAudioJitterDelay = minDelay;

  if (maxDelay < minDelay)
    maxDelay = minDelay;
  maxAudioJitterDelay = maxDelay;
}


BOOL OpalManager::TranslateIPAddress(PIPSocket::Address & /*localAddr*/,
                                     const PIPSocket::Address & /*remoteAddr */)
{
  return FALSE;
}


void OpalManager::PortInfo::Set(unsigned newBase,
                                unsigned newMax,
                                unsigned range,
                                unsigned dflt)
{
  if (newBase == 0) {
    newBase = dflt;
    newMax = dflt;
    if (dflt > 0)
      newMax += range;
  }
  else {
    if (newBase < 1024)
      newBase = 1024;
    else if (newBase > 65500)
      newBase = 65500;

    if (newMax <= newBase)
      newMax = newBase + range;
    if (newMax > 65535)
      newMax = 65535;
  }

  mutex.Wait();

  current = base = (WORD)newBase;
  max = (WORD)newMax;

  mutex.Signal();
}


WORD OpalManager::PortInfo::GetNext(unsigned increment)
{
  PWaitAndSignal m(mutex);

  if (current < base || current >= (max-increment))
    current = base;

  if (current == 0)
    return 0;

  WORD p = current;
  current = (WORD)(current + increment);
  return p;
}


void OpalManager::SetTCPPorts(unsigned tcpBase, unsigned tcpMax)
{
  tcpPorts.Set(tcpBase, tcpMax, 49, 0);
}


WORD OpalManager::GetNextTCPPort()
{
  return tcpPorts.GetNext(1);
}


void OpalManager::SetUDPPorts(unsigned udpBase, unsigned udpMax)
{
  udpPorts.Set(udpBase, udpMax, 99, 0);
}


WORD OpalManager::GetNextUDPPort()
{
  return udpPorts.GetNext(1);
}


void OpalManager::SetRtpIpPorts(unsigned rtpIpBase, unsigned rtpIpMax)
{
  rtpIpPorts.Set((rtpIpBase+1)&0xfffe, rtpIpMax&0xfffe, 199, 5000);
}


WORD OpalManager::GetRtpIpPortPair()
{
  return rtpIpPorts.GetNext(2);
}


void OpalManager::GarbageCollection()
{
  OpalCallList callsToRemove;
  callsToRemove.DisallowDeleteObjects();

  callsMutex.Wait();

  PINDEX i;
  for (i = 0; i < callsActive.GetSize(); i++) {
    OpalCall & call = callsActive.GetDataAt(i);
    if (call.GarbageCollection())
      callsToRemove.Append(&call);
  }

  for (i = 0; i < callsToRemove.GetSize(); i++)
    callsActive.SetAt(callsToRemove[i].GetToken(), NULL);

  callsMutex.Signal();

  for (i = 0; i < callsToRemove.GetSize(); i++)
    callsToRemove[i].OnCleared();

  for (i = 0; i < callsToRemove.GetSize(); i++)
    DestroyCall(&callsToRemove[i]);

  allCallsCleared.Signal();
}


void OpalManager::SignalGarbageCollector()
{
  // Signal the background threads that there is some stuff to process.
  garbageCollectFlag.Signal();
}


void OpalManager::GarbageMain(PThread &, INT)
{
  while (collectingGarbage) {
    garbageCollectFlag.Wait();
    GarbageCollection();
  }
}


/////////////////////////////////////////////////////////////////////////////
