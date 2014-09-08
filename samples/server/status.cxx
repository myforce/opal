/*
 * status.cxx
 *
 * OPAL Server status pages
 *
 * Copyright (c) 2014 Vox Lucida Pty. Ltd.
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
 * Contributor(s): Vox Lucida Pty. Ltd.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#include "precompile.h"
#include "main.h"


///////////////////////////////////////////////////////////////

BaseStatusPage::BaseStatusPage(MyManager & mgr, const PHTTPAuthority & auth, const char * name)
  : PServiceHTTPString(name, PString::Empty(), "text/html; charset=UTF-8", auth)
  , m_manager(mgr)
  , m_refreshRate(30)
{
}


PString BaseStatusPage::LoadText(PHTTPRequest & request)
{
  PHTML html;
  CreateHTML(html, request.url.GetQueryVars());
  string = html;

  return PServiceHTTPString::LoadText(request);
}

void BaseStatusPage::CreateHTML(PHTML & html, const PStringToString & query)
{
  html << PHTML::Title(GetTitle());

  if (m_refreshRate > 0)
    html << "<meta http-equiv=\"Refresh\" content=\"" << m_refreshRate << "\">\n";

  html << PHTML::Body()
       << MyProcessAncestor::Current().GetPageGraphic()
       << PHTML::Paragraph() << "<center>"

       << PHTML::Form("POST");

  CreateContent(html, query);

  if (m_refreshRate > 0)
    html << PHTML::Paragraph() 
         << PHTML::TableStart()
         << PHTML::TableRow()
         << PHTML::TableData()
         << "Refresh rate"
         << PHTML::TableData()
         << PHTML::InputNumber("Page Refresh Time", 5, 3600, m_refreshRate)
         << PHTML::TableData()
         << PHTML::SubmitButton("Set", "Set Page Refresh Time")
         << PHTML::TableEnd();

  html << PHTML::Form()
       << PHTML::HRule()
       << "Last update: <!--#macro LongDateTime-->" << PHTML::Paragraph()
       << MyProcessAncestor::Current().GetCopyrightText()
       << PHTML::Body();
}


PBoolean BaseStatusPage::Post(PHTTPRequest & request,
                              const PStringToString & data,
                              PHTML & msg)
{
  PTRACE(2, "Main\tClear call POST received " << data);

  if (data("Set Page Refresh Time") == "Set") {
    m_refreshRate = data["Page Refresh Time"].AsUnsigned();
    CreateHTML(msg, request.url.GetQueryVars());
    PServiceHTML::ProcessMacros(request, msg, "", PServiceHTML::LoadFromFile);
    return true;
  }

  msg << PHTML::Title() << "Accepted Control Command" << PHTML::Body()
    << PHTML::Heading(1) << "Accepted Control Command" << PHTML::Heading(1);

  if (!OnPostControl(data, msg))
    msg << PHTML::Heading(2) << "No calls or endpoints!" << PHTML::Heading(2);

  msg << PHTML::Paragraph()
      << PHTML::HotLink(request.url.AsString()) << "Reload page" << PHTML::HotLink()
      << "&nbsp;&nbsp;&nbsp;&nbsp;"
      << PHTML::HotLink("/") << "Home page" << PHTML::HotLink();

  PServiceHTML::ProcessMacros(request, msg, "html/status.html",
                              PServiceHTML::LoadFromFile | PServiceHTML::NoSignatureForFile);
  return TRUE;
}


///////////////////////////////////////////////////////////////

RegistrationStatusPage::RegistrationStatusPage(MyManager & mgr, const PHTTPAuthority & auth)
  : BaseStatusPage(mgr, auth, "RegistrationStatus")
{
}


PString RegistrationStatusPage::LoadText(PHTTPRequest & request)
{
#if OPAL_H323
  m_h323.clear();
  const PList<H323Gatekeeper> gkList = m_manager.GetH323EndPoint().GetGatekeepers();
  for (PList<H323Gatekeeper>::const_iterator gk = gkList.begin(); gk != gkList.end(); ++gk) {
    PString addr = '@' + gk->GetTransport().GetRemoteAddress().GetHostName(true);

    PStringStream status;
    if (gk->IsRegistered())
      status << "Registered";
    else
      status << "Failed: " << gk->GetRegistrationFailReason();

    const PStringList & aliases = gk->GetAliases();
    for (PStringList::const_iterator it = aliases.begin(); it != aliases.end(); ++it)
      m_h323[*it + addr] = status;
  }
#endif

#if OPAL_SIP
  m_sip.clear();
  SIPEndPoint & sipEP = m_manager.GetSIPEndPoint();
  const PStringList & registrations = sipEP.GetRegistrations(true);
  for (PStringList::const_iterator it = registrations.begin(); it != registrations.end(); ++it)
    m_sip[*it] = sipEP.IsRegistered(*it) ? "Registered" : (sipEP.IsRegistered(*it, true) ? "Offline" : "Failed");

#endif

#if OPAL_SKINNY
  m_skinny.clear();
  OpalSkinnyEndPoint * ep = m_manager.FindEndPointAs<OpalSkinnyEndPoint>(OPAL_PREFIX_SKINNY);
  if (ep != NULL) {
    PArray<PString> names = ep->GetPhoneDeviceNames();
    for (PINDEX i = 0; i < names.GetSize(); ++i) {
      OpalSkinnyEndPoint::PhoneDevice * phoneDevice = ep->GetPhoneDevice(names[i]);
      if (phoneDevice != NULL) {
        PStringStream str;
        str << *phoneDevice;
        PString name, status;
        if (str.Split('\t', name, status))
          m_skinny[name] = status;
      }
    }
  }
#endif
  return BaseStatusPage::LoadText(request);
}


const char * RegistrationStatusPage::GetTitle() const
{
  return "OPAL Server Registration Status";
}


void RegistrationStatusPage::CreateContent(PHTML & html, const PStringToString &) const
{
  html << PHTML::TableStart(PHTML::Border1, PHTML::CellPad4)
       << PHTML::TableRow()
       << PHTML::TableHeader() << ' '
       << PHTML::TableHeader(PHTML::NoWrap) << "Address"
       << PHTML::TableHeader(PHTML::NoWrap) << "Status"
#if OPAL_H323
       << PHTML::TableRow()
       << PHTML::TableHeader(PHTML::NoWrap, "rowspan=<!--#macro H323Count-->")
       << " H.323 Gatekeeper"
       << "<!--#macrostart H323RegistrationStatus-->"
           << PHTML::TableRow()
           << PHTML::TableData(PHTML::NoWrap)
           << "<!--#status Name-->"
           << PHTML::TableData(PHTML::NoWrap)
           << "<!--#status Status-->"
       << "<!--#macroend H323RegistrationStatus-->"
#endif // OPAL_H323

#if OPAL_SIP
       << PHTML::TableRow()
       << PHTML::TableHeader(PHTML::NoWrap, "rowspan=<!--#macro SIPCount-->")
       << " SIP Registrars "
       << "<!--#macrostart SIPRegistrationStatus-->"
           << PHTML::TableRow()
           << PHTML::TableData(PHTML::NoWrap)
           << "<!--#status Name-->"
           << PHTML::TableData(PHTML::NoWrap)
           << "<!--#status Status-->"
       << "<!--#macroend SIPRegistrationStatus-->"
#endif // OPAL_SIP

#if OPAL_SKINNY
       << PHTML::TableRow()
       << PHTML::TableHeader(PHTML::NoWrap, "rowspan=<!--#macro SkinnyCount-->")
       << " SCCP Servers "
       << "<!--#macrostart SkinnyRegistrationStatus-->"
         << PHTML::TableRow()
         << PHTML::TableData(PHTML::NoWrap)
         << "<!--#status Name-->"
         << PHTML::TableData(PHTML::NoWrap)
         << "<!--#status Status-->"
       << "<!--#macroend SkinnyRegistrationStatus-->"
#endif // OPAL_SKINNY

#if OPAL_PTLIB_NAT
       << PHTML::TableRow()
       << PHTML::TableHeader(PHTML::NoWrap)
       << " STUN Server "
       << PHTML::TableData(PHTML::NoWrap)
       << "<!--#macro STUNServer-->"
       << PHTML::TableData(PHTML::NoWrap)
       << "<!--#macro STUNStatus-->"
#endif // OPAL_PTLIB_NAT

       << PHTML::TableEnd();
}


typedef const RegistrationStatusPage::StatusMap & (RegistrationStatusPage::*StatusFunc)() const;
static PINDEX GetRegistrationCount(PHTTPRequest & resource, StatusFunc func)
{
  RegistrationStatusPage * status = dynamic_cast<RegistrationStatusPage *>(resource.m_resource);
  if (PAssertNULL(status) == NULL)
    return 2;

  PINDEX count = (status->*func)().size();
  if (count == 0)
    return 2;

  return count+1;
}


static PString GetRegistrationStatus(PHTTPRequest & resource, const PString htmlBlock, StatusFunc func)
{
  PString substitution;

  RegistrationStatusPage * status = dynamic_cast<RegistrationStatusPage *>(resource.m_resource);
  if (PAssertNULL(status) != NULL) {
    const RegistrationStatusPage::StatusMap & statuses = (status->*func)();
    for (RegistrationStatusPage::StatusMap::const_iterator it = statuses.begin(); it != statuses.end(); ++it) {
      PString insert = htmlBlock;
      PServiceHTML::SpliceMacro(insert, "status Name", it->first);
      PServiceHTML::SpliceMacro(insert, "status Status", it->second);
      substitution += insert;
    }
  }

  if (substitution.IsEmpty()) {
    substitution = htmlBlock;
    PServiceHTML::SpliceMacro(substitution, "status Name", "&nbsp;");
    PServiceHTML::SpliceMacro(substitution, "status Status", "Not registered");
  }

  return substitution;
}


#if OPAL_H323
PCREATE_SERVICE_MACRO(H323Count, resource, P_EMPTY)
{
  return GetRegistrationCount(resource, &RegistrationStatusPage::GetH323);
}


PCREATE_SERVICE_MACRO_BLOCK(H323RegistrationStatus, resource, P_EMPTY, htmlBlock)
{
  return GetRegistrationStatus(resource, htmlBlock, &RegistrationStatusPage::GetH323);
}
#endif // OPAL_H323

#if OPAL_SIP
PCREATE_SERVICE_MACRO(SIPCount, resource, P_EMPTY)
{
  return GetRegistrationCount(resource, &RegistrationStatusPage::GetSIP);
}


PCREATE_SERVICE_MACRO_BLOCK(SIPRegistrationStatus, resource, P_EMPTY, htmlBlock)
{
  return GetRegistrationStatus(resource, htmlBlock, &RegistrationStatusPage::GetSIP);
}
#endif // OPAL_SIP

#if OPAL_SKINNY
PCREATE_SERVICE_MACRO(SkinnyCount, resource, P_EMPTY)
{
  return GetRegistrationCount(resource, &RegistrationStatusPage::GetSkinny);
}


PCREATE_SERVICE_MACRO_BLOCK(SkinnyRegistrationStatus, resource, P_EMPTY, htmlBlock)
{
  return GetRegistrationStatus(resource, htmlBlock, &RegistrationStatusPage::GetSkinny);
}
#endif // OPAL_SKINNY


#if OPAL_PTLIB_NAT
static bool GetSTUN(PHTTPRequest & resource, PSTUNClient * & stun)
{
  RegistrationStatusPage * status = dynamic_cast<RegistrationStatusPage *>(resource.m_resource);
  if (PAssertNULL(status) == NULL)
    return false;

  stun = dynamic_cast<PSTUNClient *>(status->m_manager.GetNatMethods().GetMethodByName(PSTUNClient::MethodName()));
  return stun != NULL;
}


PCREATE_SERVICE_MACRO(STUNServer, resource, P_EMPTY)
{
  PSTUNClient * stun;
  if (!GetSTUN(resource, stun))
    return PString::Empty();

  PHTML html(PHTML::InBody);
  html << stun->GetServer();

  PIPAddressAndPort ap;
  if (stun->GetServerAddress(ap))
    html << PHTML::BreakLine() << ap;

  return html;
}


PCREATE_SERVICE_MACRO(STUNStatus, resource, P_EMPTY)
{
  PSTUNClient * stun;
  if (!GetSTUN(resource, stun))
    return PString::Empty();

  PHTML html(PHTML::InBody);
  PNatMethod::NatTypes type = stun->GetNatType();
  html << type;

  PIPAddress ip;
  if (stun->GetExternalAddress(ip))
    html << PHTML::BreakLine() << ip;

  return html;
}
#endif // OPAL_PTLIB_NAT


///////////////////////////////////////////////////////////////

CallStatusPage::CallStatusPage(MyManager & mgr, const PHTTPAuthority & auth)
  : BaseStatusPage(mgr, auth, "CallStatus")
{
}


PString CallStatusPage::LoadText(PHTTPRequest & request)
{
  m_calls = m_manager.GetAllCalls();
  return BaseStatusPage::LoadText(request);
}


const char * CallStatusPage::GetTitle() const
{
  return "OPAL Server Call Status";
}


void CallStatusPage::CreateContent(PHTML & html, const PStringToString &) const
{
  html << "Current call count: <!--#macro CallCount-->" << PHTML::Paragraph()
       << PHTML::TableStart(PHTML::Border1, PHTML::CellPad4)
       << PHTML::TableRow()
       << PHTML::TableHeader()
       << "&nbsp;A&nbsp;Party&nbsp;"
       << PHTML::TableHeader()
       << "&nbsp;B&nbsp;Party&nbsp;"
       << PHTML::TableHeader()
       << "&nbsp;Duration&nbsp;"
       << "<!--#macrostart CallStatus-->"
         << PHTML::TableRow()
         << PHTML::TableData(PHTML::NoWrap)
         << "<!--#status A-Party-->"
         << PHTML::TableData(PHTML::NoWrap)
         << "<!--#status B-Party-->"
         << PHTML::TableData()
         << "<!--#status Duration-->"
         << PHTML::TableData()
         << PHTML::SubmitButton("Clear", "!--#status Token--")
       << "<!--#macroend CallStatus-->"
       << PHTML::TableEnd();
}


bool CallStatusPage::OnPostControl(const PStringToString & data, PHTML & msg)
{
  bool gotOne = false;

  for (PStringToString::const_iterator it = data.begin(); it != data.end(); ++it) {
    PString key = it->first;
    PString value = it->second;
    if (value == "Clear") {
      PSafePtr<OpalCall> call = m_manager.FindCallWithLock(key, PSafeReference);
      if (call != NULL) {
        msg << PHTML::Heading(2) << "Clearing call " << *call << PHTML::Heading(2);
        call->Clear();
        gotOne = true;
      }
    }
  }

  return gotOne;
}


PCREATE_SERVICE_MACRO(CallCount, resource, P_EMPTY)
{
  CallStatusPage * status = dynamic_cast<CallStatusPage *>(resource.m_resource);
  return PAssertNULL(status) == NULL ? 0 : status->GetCalls().GetSize();
}


PCREATE_SERVICE_MACRO_BLOCK(CallStatus,resource,P_EMPTY,htmlBlock)
{
  CallStatusPage * status = dynamic_cast<CallStatusPage *>(resource.m_resource);
  if (PAssertNULL(status) == NULL)
    return PString::Empty();

  PString substitution;

  const PArray<PString> & calls = status->GetCalls();
  for (PINDEX i = 0; i < calls.GetSize(); ++i) {
    PSafePtr<OpalCall> call = status->m_manager.FindCallWithLock(calls[i], PSafeReadOnly);
    if (call == NULL)
      continue;

    // make a copy of the repeating html chunk
    PString insert = htmlBlock;

    PServiceHTML::SpliceMacro(insert, "status Token",    call->GetToken());
    PServiceHTML::SpliceMacro(insert, "status A-Party",  call->GetPartyA());
    PServiceHTML::SpliceMacro(insert, "status B-Party",  call->GetPartyB());

    PStringStream duration;
    duration.precision(0);
    if (call->GetEstablishedTime().IsValid())
      duration << call->GetEstablishedTime().GetElapsed();
    else
      duration << '(' << call->GetStartTime().GetElapsed() << ')';
    PServiceHTML::SpliceMacro(insert, "status Duration", duration);

    // Then put it into the page, moving insertion point along after it.
    substitution += insert;
  }

  return substitution;
}


///////////////////////////////////////////////////////////////

#if OPAL_H323

GkStatusPage::GkStatusPage(MyManager & mgr, const PHTTPAuthority & auth)
  : BaseStatusPage(mgr, auth, "GkStatus")
  , m_gkServer(mgr.FindEndPointAs<MyH323EndPoint>("h323")->GetGatekeeperServer())
{
}


const char * GkStatusPage::GetTitle() const
{
  return "OPAL Gatekeeper Status";
}


void GkStatusPage::CreateContent(PHTML & html, const PStringToString &) const
{
  html << PHTML::TableStart(PHTML::Border1)
       << PHTML::TableRow()
       << PHTML::TableHeader()
       << "&nbsp;End&nbsp;Point&nbsp;Identifier&nbsp;"
       << PHTML::TableHeader()
       << "&nbsp;Call&nbsp;Signal&nbsp;Addresses&nbsp;"
       << PHTML::TableHeader()
       << "&nbsp;Aliases&nbsp;"
       << PHTML::TableHeader()
       << "&nbsp;Application&nbsp;"
       << PHTML::TableHeader()
       << "&nbsp;Active&nbsp;Calls&nbsp;"
       << "<!--#macrostart EndPointStatus-->"
       << PHTML::TableRow()
       << PHTML::TableData()
       << "<!--#status EndPointIdentifier-->"
       << PHTML::TableData()
       << "<!--#status CallSignalAddresses-->"
       << PHTML::TableData(PHTML::NoWrap)
       << "<!--#status EndPointAliases-->"
       << PHTML::TableData(PHTML::NoWrap)
       << "<!--#status Application-->"
       << PHTML::TableData("align=center")
       << "<!--#status ActiveCalls-->"
       << PHTML::TableData()
       << PHTML::SubmitButton("Unregister", "!--#status EndPointIdentifier--")
       << "<!--#macroend EndPointStatus-->"
       << PHTML::TableEnd();
}


PBoolean GkStatusPage::OnPostControl(const PStringToString & data, PHTML & msg)
{
  bool gotOne = false;

  for (PStringToString::const_iterator it = data.begin(); it != data.end(); ++it) {
    PString key = it->first;
    PString value = it->second;
    if (value == "Unregister") {
      PSafePtr<H323RegisteredEndPoint> ep = m_gkServer.FindEndPointByIdentifier(key);
      if (ep != NULL) {
        msg << PHTML::Heading(2) << "Unregistered endpoint " << *ep << PHTML::Heading(2);
        ep->Unregister();
        gotOne = true;
      }
    }
  }

  return gotOne;
}


#endif //OPAL_H323


// End of File ///////////////////////////////////////////////////////////////
