/*
 * main.cpp
 *
 * An OPAL GUI phone application.
 *
 * Open Phone Abstraction Library (OPAL)
 * Formally known as the Open H323 project.
 *    
 * Copyright (c) 2004 Post Increment
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
 * The Original Code is Open Phone client.
 *
 * The Initial Developer of the Original Code is Post Increment
 *
 * Contributor(s): 
 *
 * $Log: main.cpp,v $
 * Revision 1.24  2004/09/29 12:47:39  rjongbloed
 * Added gatekeeper support
 *
 * Revision 1.23  2004/09/29 12:02:40  rjongbloed
 * Added popup menu to edit Speed DIals
 *
 * Revision 1.22  2004/09/28 23:00:17  rjongbloed
 * Added ability to add and edit Speed DIals
 *
 * Revision 1.21  2004/08/22 12:27:45  rjongbloed
 * More work on SIP registration, time to live refresh and deregistration on exit.
 *
 * Revision 1.20  2004/08/14 07:56:30  rjongbloed
 * Major revision to utilise the PSafeCollection classes for the connections and calls.
 *
 * Revision 1.19  2004/07/17 08:21:24  rjongbloed
 * Added ability to manipulate codec lists
 *
 * Revision 1.18  2004/07/14 13:17:42  rjongbloed
 * Added saving of the width of columns in the speed dial list.
 * Fixed router display in options dialog so is empty if IP address invalid.
 *
 * Revision 1.17  2004/07/11 12:42:11  rjongbloed
 * Added function on endpoints to get the list of all media formats any
 *   connection the endpoint may create can support.
 *
 * Revision 1.16  2004/07/04 12:53:09  rjongbloed
 * Added support for route editing.
 *
 * Revision 1.15  2004/06/05 14:37:03  rjongbloed
 * More implemntation of options dialog.
 *
 * Revision 1.14  2004/05/25 12:55:52  rjongbloed
 * Added all silence suppression modes to Options dialog.
 *
 * Revision 1.13  2004/05/24 13:44:03  rjongbloed
 * More implementation on OPAL OpenPhone.
 *
 * Revision 1.12  2004/05/15 12:18:23  rjongbloed
 * More work on wxWindows based OpenPhone
 *
 * Revision 1.11  2004/05/12 12:41:38  rjongbloed
 * More work on wxWindows based OpenPhone
 *
 * Revision 1.10  2004/05/09 13:24:25  rjongbloed
 * More work on wxWindows based OpenPhone
 *
 * Revision 1.9  2004/05/06 13:23:43  rjongbloed
 * Work on wxWindows based OpenPhone
 *
 * Revision 1.8  2004/05/01 13:38:05  rjongbloed
 * Some early implementation of wxWIndows based OPAL GUI client.
 *
 */

#ifdef __GNUG__
#pragma implementation
#pragma interface
#endif

#include "main.h"
#include "version.h"

#include <wx/config.h>
#include <wx/splitter.h>
#include <wx/listctrl.h>
#include <wx/image.h>
#include <wx/valgen.h>

#undef LoadMenu // Bizarre but necessary before the xml code
#include <wx/xrc/xmlres.h>

#include <opal/ivr.h>
#include <lids/lidep.h>
#include <ptclib/pstun.h>

#ifdef OPAL_STATIC_LINK
#define H323_STATIC_LIB
#include <codec/allcodecs.h>
#include <lids/alllids.h>
#endif


#define DEF_FIELD(name) static const char name##Key[] = #name;

static const char AppearanceGroup[] = "/Appearance";
DEF_FIELD(MainFrameX);
DEF_FIELD(MainFrameY);
DEF_FIELD(MainFrameWidth);
DEF_FIELD(MainFrameHeight);
DEF_FIELD(SashPosition);
DEF_FIELD(ActiveView);

static const char GeneralGroup[] = "/General";
DEF_FIELD(Username);
DEF_FIELD(DisplayName);
DEF_FIELD(RingSoundFileName);
DEF_FIELD(AutoAnswer);
DEF_FIELD(IVRScript);

static const char NetworkingGroup[] = "/Networking";
DEF_FIELD(Bandwidth);
DEF_FIELD(TCPPortBase);
DEF_FIELD(TCPPortMax);
DEF_FIELD(UDPPortBase);
DEF_FIELD(UDPPortMax);
DEF_FIELD(RTPPortBase);
DEF_FIELD(RTPPortMax);
DEF_FIELD(RTPTOS);
DEF_FIELD(STUNServer);
DEF_FIELD(NATRouter);

static const char AudioGroup[] = "/Audio";
DEF_FIELD(SoundPlayer);
DEF_FIELD(SoundRecorder);
DEF_FIELD(SoundBuffers);
DEF_FIELD(LineInterfaceDevice);
DEF_FIELD(AEC);
DEF_FIELD(Country);
DEF_FIELD(MinJitter);
DEF_FIELD(MaxJitter);
DEF_FIELD(SilenceSuppression);
DEF_FIELD(SilenceThreshold);
DEF_FIELD(SignalDeadband);
DEF_FIELD(SilenceDeadband);

static const char VideoGroup[] = "/Video";
DEF_FIELD(VideoGrabber);
DEF_FIELD(VideoGrabFormat);
DEF_FIELD(VideoGrabSource);
DEF_FIELD(VideoEncodeQuality);
DEF_FIELD(VideoGrabFrameRate);
DEF_FIELD(VideoEncodeMaxBitRate);
DEF_FIELD(VideoGrabPreview);
DEF_FIELD(VideoFlipLocal);
DEF_FIELD(VideoAutoTransmit);
DEF_FIELD(VideoAutoReceive);
DEF_FIELD(VideoFlipRemote);

static const char CodecsGroup[] = "/Codecs";
static const char CodecNameKey[] = "Name";

static const char H323Group[] = "/H.323";
DEF_FIELD(GatekeeperMode);
DEF_FIELD(GatekeeperAddress);
DEF_FIELD(GatekeeperIdentifier);
DEF_FIELD(GatekeeperTTL);
DEF_FIELD(GatekeeperLogin);
DEF_FIELD(GatekeeperPassword);
DEF_FIELD(DTMFSendMode);
DEF_FIELD(CallIntrusionProtectionLevel);
DEF_FIELD(DisableFastStart);
DEF_FIELD(DisableH245Tunneling);
DEF_FIELD(DisableH245inSETUP);

static const char SIPGroup[] = "/SIP";
DEF_FIELD(SIPProxy);
DEF_FIELD(SIPProxyUsername);
DEF_FIELD(SIPProxyPassword);
DEF_FIELD(RegistrarName);
DEF_FIELD(RegistrarUsername);
DEF_FIELD(RegistrarPassword);
DEF_FIELD(RegistrarTimeToLive);

static const char RoutingGroup[] = "/Routes";

#if PTRACING
static const char TracingGroup[] = "/Tracing";
DEF_FIELD(EnableTracing);
DEF_FIELD(TraceLevelThreshold);
DEF_FIELD(TraceLevelNumber);
DEF_FIELD(TraceFileLine);
DEF_FIELD(TraceBlocks);
DEF_FIELD(TraceDateTime);
DEF_FIELD(TraceTimestamp);
DEF_FIELD(TraceThreadName);
DEF_FIELD(TraceThreadAddress);
DEF_FIELD(TraceFileName);
DEF_FIELD(TraceOptions);
#endif

static const char SpeedDialsGroup[] = "/Speed Dials";
static const char SpeedDialAddressKey[] = "Address";
static const char SpeedDialNumberKey[] = "Number";
static const char SpeedDialDescriptionKey[] = "Description";


///////////////////////////////////////////////////////////////////////////////

class TextCtrlChannel : public PChannel
{
    PCLASSINFO(TextCtrlChannel, PChannel)
  public:
    TextCtrlChannel(wxTextCtrl * textCtrl = NULL)
      : m_textCtrl(textCtrl)
      { }

    virtual BOOL Write(
      const void * buf, /// Pointer to a block of memory to write.
      PINDEX len        /// Number of bytes to write.
    ) {
      if (m_textCtrl == NULL)
        return FALSE;
      m_textCtrl->WriteText(wxString((const wxChar *)buf, (size_t)len));
      return TRUE;
    }

    void SetTextCtrl(
      wxTextCtrl * textCtrl
    ) { m_textCtrl = textCtrl; }

  protected:
    wxTextCtrl * m_textCtrl;
} LogWindow;



///////////////////////////////////////////////////////////////////////////////

IMPLEMENT_APP(OpenPhoneApp)

OpenPhoneApp::OpenPhoneApp()
  : PProcess("Equivalence", "OpenPhone",
              MAJOR_VERSION, MINOR_VERSION, BUILD_TYPE, BUILD_NUMBER)
{
  wxConfig::Set(new wxConfig((const char *)GetName(), (const char *)GetManufacturer()));
}


void OpenPhoneApp::Main()
{
  // Dummy function
}


bool OpenPhoneApp::OnInit()
{
  wxImage::AddHandler(new wxGIFHandler);
  wxXmlResource::Get()->InitAllHandlers();
  wxXmlResource::Get()->Load("openphone.xrc");

  // Create the main frame window
  MyFrame * frame = new MyFrame();
  SetTopWindow(frame);
  return frame->Initialise();
}


///////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(MyFrame, wxFrame)
  EVT_CLOSE(MyFrame::OnClose)

  EVT_MENU_OPEN(MyFrame::OnAdjustMenus)

  EVT_MENU(XRCID("MenuQuit"),     MyFrame::OnMenuQuit)
  EVT_MENU(XRCID("MenuAbout"),    MyFrame::OnMenuAbout)
  EVT_MENU(XRCID("MenuCall"),     MyFrame::OnMenuCall)
  EVT_MENU(XRCID("MenuAnswer"),   MyFrame::OnMenuAnswer)
  EVT_MENU(XRCID("MenuHangUp"),   MyFrame::OnMenuHangUp)
  EVT_MENU(XRCID("NewSpeedDial"), MyFrame::OnNewSpeedDial)
  EVT_MENU(XRCID("ViewLarge"),    MyFrame::OnViewLarge)
  EVT_MENU(XRCID("ViewSmall"),    MyFrame::OnViewSmall)
  EVT_MENU(XRCID("ViewList"),     MyFrame::OnViewList)
  EVT_MENU(XRCID("ViewDetails"),  MyFrame::OnViewDetails)
  EVT_MENU(XRCID("EditSpeedDial"),MyFrame::OnEditSpeedDial)
  EVT_MENU(XRCID("MenuOptions"),  MyFrame::OnOptions)

  EVT_SPLITTER_SASH_POS_CHANGED(SplitterID, MyFrame::OnSashPositioned)
  EVT_LIST_ITEM_ACTIVATED(SpeedDialsID, MyFrame::OnSpeedDialActivated)
  EVT_LIST_COL_END_DRAG(SpeedDialsID, MyFrame::OnSpeedDialColumnResize)
  EVT_LIST_ITEM_RIGHT_CLICK(SpeedDialsID, MyFrame::OnRightClick) 
END_EVENT_TABLE()

MyFrame::MyFrame()
  : wxFrame(NULL, -1, wxT("OpenPhone"), wxDefaultPosition, wxSize(640, 480)),
    m_speedDials(NULL),
    pcssEP(NULL),
    potsEP(NULL),
#if OPAL_H323
    h323EP(NULL),
#endif
#if OPAL_SIP
    sipEP(NULL),
#endif
#if P_EXPAT
    ivrEP(NULL)
#endif
{
  wxConfigBase * config = wxConfig::Get();
  config->SetPath(AppearanceGroup);

  // Give it an icon
  SetIcon( wxICON(OpenOphone) );

  // Make a menubar
  wxMenuBar * menubar = wxXmlResource::Get()->LoadMenuBar("MenuBar");
  SetMenuBar(menubar);

  int x, y = 0;
  if (config->Read(MainFrameXKey, &x) && config->Read(MainFrameYKey, &y))
    Move(x, y);

  int w, h = 0;
  if (config->Read(MainFrameWidthKey, &w) && config->Read(MainFrameHeightKey, &h))
    SetSize(w, h);

  // Make the content of the main window, speed dial and log panes
  m_splitter = new wxSplitterWindow(this, SplitterID, wxDefaultPosition, wxDefaultSize, wxSP_3D);

  m_logWindow = new wxTextCtrl(m_splitter, -1, wxEmptyString,
                               wxDefaultPosition, wxDefaultSize,
                               wxTE_MULTILINE | wxSUNKEN_BORDER);
  LogWindow.SetTextCtrl(m_logWindow);
  m_logWindow->SetForegroundColour(wxColour(0,255,0)); // Green
  m_logWindow->SetBackgroundColour(wxColour(0,0,0)); // Black

  int i;
  if (!config->Read(ActiveViewKey, &i) || i < 0 || i >= e_NumViews)
  {
    i = e_ViewList;
  }
  static const char * const ViewMenuNames[e_NumViews] = {
    "ViewLarge", "ViewSmall", "ViewList", "ViewDetails"
  };
  menubar->Check(XRCID(ViewMenuNames[i]), true);
  RecreateSpeedDials((SpeedDialViews)i);

  // Set up sizer to automatically resize the splitter to size of window
  wxBoxSizer * sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(m_splitter, 1, wxGROW, 0);
  SetSizer(sizer);

  // Show the frame
  Show(TRUE);
}


MyFrame::~MyFrame()
{
  ClearAllCalls();

  // Must do this before we destroy the manager or a crash will result
  if (potsEP != NULL)
    potsEP->RemoveAllLines();

  LogWindow.SetTextCtrl(NULL);
}


void MyFrame::RecreateSpeedDials(SpeedDialViews view)
{
  wxConfigBase * config = wxConfig::Get();
  config->SetPath(AppearanceGroup);

  config->Write(ActiveViewKey, view);

  wxListCtrl * oldSpeedDials = m_speedDials;

  static DWORD const ListCtrlStyle[e_NumViews] = {
    wxLC_ICON, wxLC_SMALL_ICON, wxLC_LIST, wxLC_REPORT
  };

  m_speedDials = new wxListCtrl(m_splitter, SpeedDialsID,
                               wxDefaultPosition, wxDefaultSize,
                               ListCtrlStyle[view] | wxLC_EDIT_LABELS | wxSUNKEN_BORDER);

  int width;
  static const char * const titles[e_NumColumns] = { "Name", "Number", "Address", "Description" };

  for (int i = 0; i < e_NumColumns; i++) {
    m_speedDials->InsertColumn(i, titles[i]);
    wxString key;
    key.sprintf("ColumnWidth%u", i);
    if (config->Read(key, &width))
      m_speedDials->SetColumnWidth(i, width);
  }

  // Now either replace the top half of the splitter or set it for the first time
  if (oldSpeedDials == NULL)
    m_splitter->SplitHorizontally(m_speedDials, m_logWindow);
  else {
    m_splitter->ReplaceWindow(oldSpeedDials, m_speedDials);
    delete oldSpeedDials;
  }

  if (config->Read(SashPositionKey, &width))
    m_splitter->SetSashPosition(width);

  // Read the speed dials from the configuration
  config->SetPath(SpeedDialsGroup);
  wxString groupName;
  long groupIndex;
  if (config->GetFirstGroup(groupName, groupIndex)) {
    do {
      config->SetPath(groupName);
      wxString number, address, description;
      if (config->Read(SpeedDialAddressKey, &address) && !address.empty()) {
        int pos = m_speedDials->InsertItem(INT_MAX, groupName);
        m_speedDials->SetItem(pos, e_NumberColumn, config->Read(SpeedDialNumberKey, ""));
        m_speedDials->SetItem(pos, e_AddressColumn, address);
        m_speedDials->SetItem(pos, e_DescriptionColumn, config->Read(SpeedDialDescriptionKey, ""));
      }
      config->SetPath("..");
    } while (config->GetNextGroup(groupName, groupIndex));
  }
}


void MyFrame::OnClose(wxCloseEvent& event)
{
  wxConfigBase * config = wxConfig::Get();
  config->SetPath(AppearanceGroup);

  int x, y;
  GetPosition(&x, &y);
  config->Write(MainFrameXKey, x);
  config->Write(MainFrameYKey, y);

  int w, h;
  GetSize(&w, &h);
  config->Write(MainFrameWidthKey, w);
  config->Write(MainFrameHeightKey, h);

  Destroy();
}


void MyFrame::OnAdjustMenus(wxMenuEvent& WXUNUSED(event))
{
  wxMenuBar * menubar = GetMenuBar();
  bool inCall = !m_currentCallToken.IsEmpty();
  menubar->Enable(XRCID("MenuCall"),   !inCall);
  menubar->Enable(XRCID("MenuAnswer"), !pcssEP->m_incomingConnectionToken);
  menubar->Enable(XRCID("MenuHangUp"),  inCall);
}


void MyFrame::OnMenuQuit(wxCommandEvent& WXUNUSED(event))
{
    Close(TRUE);
}


void MyFrame::OnMenuAbout(wxCommandEvent& WXUNUSED(event))
{
  wxMessageDialog dialog(this,
                         _T("OpenPhone\nPost Increment (c) 2004"),
                         _T("About OpenPhone"),
                         wxOK);
  dialog.ShowModal();
}


void MyFrame::OnMenuCall(wxCommandEvent& WXUNUSED(event))
{
  CallDialog dlg(this);
  if (dlg.ShowModal() == wxID_OK)
    MakeCall(dlg.m_Address);
}


void MyFrame::OnMenuAnswer(wxCommandEvent& WXUNUSED(event))
{
  m_currentCallToken = pcssEP->m_incomingConnectionToken;
  pcssEP->m_incomingConnectionToken.MakeEmpty();
  pcssEP->AcceptIncomingConnection(m_currentCallToken);
}


void MyFrame::OnMenuHangUp(wxCommandEvent& WXUNUSED(event))
{
  if (m_currentCallToken.IsEmpty())
    return;

  LogWindow << "Hanging up \"" << m_currentCallToken << '"' << endl;
  ClearCall(m_currentCallToken);
}


void MyFrame::OnNewSpeedDial(wxCommandEvent& WXUNUSED(event))
{
  wxString groupName = "New Speed Dial";
  unsigned tieBreaker = 0;
  int count = m_speedDials->GetItemCount();
  int i = 0;
  while (i < count) {
    if (m_speedDials->GetItemText(i) != groupName)
      i++;
    else {
      groupName.sprintf("New Speed Dial (%u)", ++tieBreaker);
      i = 0;
    }
  }

  int pos = m_speedDials->InsertItem(INT_MAX, groupName);
  m_speedDials->SetItem(pos, e_NumberColumn, "");
  m_speedDials->SetItem(pos, e_AddressColumn, "");
  m_speedDials->SetItem(pos, e_DescriptionColumn, "");
  m_speedDials->EditLabel(pos);
}


void MyFrame::OnViewLarge(wxCommandEvent& event)
{
  GetMenuBar()->Check(event.GetId(), true);
  RecreateSpeedDials(e_ViewLarge);
}


void MyFrame::OnViewSmall(wxCommandEvent& event)
{
  GetMenuBar()->Check(event.GetId(), true);
  RecreateSpeedDials(e_ViewSmall);
}


void MyFrame::OnViewList(wxCommandEvent& event)
{
  GetMenuBar()->Check(event.GetId(), true);
  RecreateSpeedDials(e_ViewList);
}


void MyFrame::OnViewDetails(wxCommandEvent& event)
{
  GetMenuBar()->Check(event.GetId(), true);
  RecreateSpeedDials(e_ViewDetails);
}


void MyFrame::OnEditSpeedDial(wxCommandEvent& WXUNUSED(event))
{
  if (m_speedDials->GetSelectedItemCount() == 1)
    EditSpeedDial(m_speedDials->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED));
}


void MyFrame::OnSashPositioned(wxSplitterEvent& event)
{
  wxConfigBase * config = wxConfig::Get();
  config->SetPath(AppearanceGroup);
  config->Write(SashPositionKey, event.GetSashPosition());
}


void MyFrame::OnSpeedDialActivated(wxListEvent& event)
{
  wxListItem item;
  item.m_itemId = event.GetIndex();
  if (item.m_itemId < 0)
    return;

  item.m_col = e_AddressColumn;
  item.m_mask = wxLIST_MASK_TEXT;
  if (m_speedDials->GetItem(item))
    MakeCall(item.m_text);
}


void MyFrame::OnSpeedDialColumnResize(wxListEvent& event)
{
  wxConfigBase * config = wxConfig::Get();
  config->SetPath(AppearanceGroup);
  wxString key;
  key.sprintf("ColumnWidth%u", event.GetColumn());
  config->Write(key, m_speedDials->GetColumnWidth(event.GetColumn()));
}


void MyFrame::OnRightClick(wxListEvent& event)
{
  wxMenuBar * menuBar = wxXmlResource::Get()->LoadMenuBar("PopUpMenu");
  PopupMenu(menuBar->GetMenu(0), event.GetPoint());
  delete menuBar;
}


void MyFrame::EditSpeedDial(int index)
{
  if (index < 0)
    return;

  wxListItem item;
  item.m_itemId = index;
  item.m_mask = wxLIST_MASK_TEXT;

  item.m_col = e_NameColumn;
  if (!m_speedDials->GetItem(item))
    return;

  // Should display a menu, but initially just allow editing
  SpeedDialDialog dlg(this);

  wxString originalName = dlg.m_Name = item.m_text;

  item.m_col = e_NumberColumn;
  if (m_speedDials->GetItem(item))
    dlg.m_Number = item.m_text;

  item.m_col = e_AddressColumn;
  if (m_speedDials->GetItem(item))
    dlg.m_Address = item.m_text;

  item.m_col = e_DescriptionColumn;
  if (m_speedDials->GetItem(item))
    dlg.m_Description = item.m_text;

  if (dlg.ShowModal() == wxID_OK) {
    item.m_col = e_NameColumn;
    item.m_text = dlg.m_Name;
    m_speedDials->SetItem(item);

    item.m_col = e_NumberColumn;
    item.m_text = dlg.m_Number;
    m_speedDials->SetItem(item);

    item.m_col = e_AddressColumn;
    item.m_text = dlg.m_Address;
    m_speedDials->GetItem(item);

    item.m_col = e_DescriptionColumn;
    item.m_text = dlg.m_Description;
    m_speedDials->SetItem(item);

    wxConfigBase * config = wxConfig::Get();
    config->SetPath(SpeedDialsGroup);
    config->DeleteGroup(originalName);
    config->SetPath(dlg.m_Name);
    config->Write(SpeedDialNumberKey, dlg.m_Number);
    config->Write(SpeedDialAddressKey, dlg.m_Address);
    config->Write(SpeedDialDescriptionKey, dlg.m_Description);
  }
}


bool MyFrame::HasSpeedDialName(const wxString & name) const
{
  return m_speedDials->FindItem(-1, name) >= 0;
}


bool MyFrame::HasSpeedDialNumber(const char * number, const char * ignore) const
{
  int count = m_speedDials->GetItemCount();
  wxListItem item;
  item.m_mask = wxLIST_MASK_TEXT;
  item.m_col = e_NumberColumn;
  for (item.m_itemId = 0; item.m_itemId < count; item.m_itemId++) {
    if (m_speedDials->GetItem(item)) {
      int len = item.m_text.Length();
      if (len > 0 && (ignore == NULL || strcmp(ignore, item.m_text) != 0) && strncmp(number, item.m_text, len) == 0)
        return true;
    }
  }
  return false;
}


void MyFrame::MakeCall(const PwxString & address)
{
  if (address.IsEmpty())
    return;

  LogWindow << "Calling \"" << address << '"' << endl;

  if (potsEP != NULL && potsEP->GetLine("*") != NULL)
    SetUpCall("pots:*", address, m_currentCallToken);
  else
    SetUpCall("pc:*", address, m_currentCallToken);
}


void MyFrame::OnEstablishedCall(OpalCall & call)
{
  m_currentCallToken = call.GetToken();
  LogWindow << "In call with " << call.GetPartyB() << " using " << call.GetPartyA() << endl;
}


void MyFrame::OnClearedCall(OpalCall & call)
{
  if (m_currentCallToken == call.GetToken())
    m_currentCallToken.MakeEmpty();

  PString remoteName = '"' + call.GetPartyB() + '"';
  switch (call.GetCallEndReason()) {
    case OpalConnection::EndedByRemoteUser :
      LogWindow << remoteName << " has cleared the call";
      break;
    case OpalConnection::EndedByCallerAbort :
      LogWindow << remoteName << " has stopped calling";
      break;
    case OpalConnection::EndedByRefusal :
      LogWindow << remoteName << " did not accept your call";
      break;
    case OpalConnection::EndedByNoAnswer :
      LogWindow << remoteName << " did not answer your call";
      break;
    case OpalConnection::EndedByTransportFail :
      LogWindow << "Call with " << remoteName << " ended abnormally";
      break;
    case OpalConnection::EndedByCapabilityExchange :
      LogWindow << "Could not find common codec with " << remoteName;
      break;
    case OpalConnection::EndedByNoAccept :
      LogWindow << "Did not accept incoming call from " << remoteName;
      break;
    case OpalConnection::EndedByAnswerDenied :
      LogWindow << "Refused incoming call from " << remoteName;
      break;
    case OpalConnection::EndedByNoUser :
      LogWindow << "Gatekeeper could find user " << remoteName;
      break;
    case OpalConnection::EndedByNoBandwidth :
      LogWindow << "Call to " << remoteName << " aborted, insufficient bandwidth.";
      break;
    case OpalConnection::EndedByUnreachable :
      LogWindow << remoteName << " could not be reached.";
      break;
    case OpalConnection::EndedByNoEndPoint :
      LogWindow << "No phone running for " << remoteName;
      break;
    case OpalConnection::EndedByHostOffline :
      LogWindow << remoteName << " is not online.";
      break;
    case OpalConnection::EndedByConnectFail :
      LogWindow << "Transport error calling " << remoteName;
      break;
    default :
      LogWindow << "Call with " << remoteName << " completed";
  }
  PTime now;
  LogWindow << ", on " << now.AsString("w h:mma") << ". Duration "
            << setprecision(0) << setw(5) << (now - call.GetStartTime())
            << "s." << endl;
}


BOOL MyFrame::OnOpenMediaStream(OpalConnection & connection, OpalMediaStream & stream)
{
  if (!OpalManager::OnOpenMediaStream(connection, stream))
    return FALSE;

  PString prefix = connection.GetEndPoint().GetPrefixName();
  if (prefix == pcssEP->GetPrefixName())
    return TRUE;

  LogWindow << "Started ";

  if (stream.IsSource())
    LogWindow << "receiving ";
  else
    LogWindow << "sending ";

  LogWindow << stream.GetMediaFormat();

  if (stream.IsSource())
    LogWindow << " from ";
  else
    LogWindow << " to ";

  LogWindow << connection.GetEndPoint().GetPrefixName() << " endpoint" << endl;

  return TRUE;
}


void MyFrame::OnUserInputString(OpalConnection & connection, const PString & value)
{
  LogWindow << "User input received: \"" << value << '"' << endl;
  OpalManager::OnUserInputString(connection, value);
}


bool MyFrame::Initialise()
{
  wxConfigBase * config = wxConfig::Get();

#if PTRACING
  ////////////////////////////////////////
  // Tracing fields
  config->SetPath(TracingGroup);
  if (config->Read(EnableTracingKey, &m_enableTracing, false) && m_enableTracing &&
      config->Read(TraceFileNameKey, &m_traceFileName) && !m_traceFileName.empty()) {
    int traceLevelThreshold = 3;
    config->Read(TraceLevelThresholdKey, &traceLevelThreshold);
    int traceOptions = PTrace::DateAndTime|PTrace::Thread|PTrace::FileAndLine;
    config->Read(TraceOptionsKey, &traceOptions);
    PTrace::Initialise(traceLevelThreshold, m_traceFileName, traceOptions);
  }
#endif

  ////////////////////////////////////////
  // Creating the endpoints
#if OPAL_H323
  h323EP = new MyH323EndPoint(*this);

  if (h323EP->StartListeners(PStringArray()))
    LogWindow << "H.323 listening on " << setfill(',') << h323EP->GetListeners() << setfill(' ') << endl;
#endif

#if OPAL_SIP
  sipEP = new MySIPEndPoint(*this);

  if (sipEP->StartListeners(PStringArray()))
    LogWindow << "SIP listening on " << setfill(',') << sipEP->GetListeners() << setfill(' ') << endl;
#endif

#if P_EXPAT
  ivrEP = new OpalIVREndPoint(*this);
#endif

  potsEP = new OpalPOTSEndPoint(*this);
  pcssEP = new MyPCSSEndPoint(*this);

  PwxString str;
  bool on;
  int value1, value2;

#define LOAD_FIELD_STR(name, set) \
  if (config->Read(name##Key, &str)) set(str)
#define LOAD_FIELD_BOOL(name, set) \
  if (config->Read(name##Key, &on)) set(on)
#define LOAD_FIELD_INT(name, set) \
  if (config->Read(name##Key, &value1)) set(value1)
#define LOAD_FIELD_INT2(name1, name2, set) \
  if (config->Read(name1##Key, &value1)) { \
    config->Read(name2##Key, &value2, 0); \
    set(value1, value2); \
  }
#define LOAD_FIELD_ENUM(name, set, type) \
  if (config->Read(name##Key, &value1)) set((type)value1)

  ////////////////////////////////////////
  // General fields
  config->SetPath(GeneralGroup);
  LOAD_FIELD_STR(Username, SetDefaultUserName);
  LOAD_FIELD_STR(DisplayName, SetDefaultDisplayName);
  LOAD_FIELD_BOOL(AutoAnswer, pcssEP->SetAutoAnswer);
#if P_EXPAT
  LOAD_FIELD_STR(IVRScript, ivrEP->SetDefaultVXML);
#endif

  ////////////////////////////////////////
  // Networking fields
  config->SetPath(NetworkingGroup);
  LOAD_FIELD_INT2(TCPPortBase, TCPPortBase, SetTCPPorts);
  LOAD_FIELD_INT2(UDPPortBase, UDPPortBase, SetUDPPorts);
  LOAD_FIELD_INT2(RTPPortBase, RTPPortBase, SetRtpIpPorts);
  LOAD_FIELD_INT(RTPTOS, SetRtpIpTypeofService);
  LOAD_FIELD_STR(NATRouter, SetTranslationAddress);
  LOAD_FIELD_STR(STUNServer, SetSTUNServer);

  ////////////////////////////////////////
  // Sound fields
  config->SetPath(AudioGroup);
  LOAD_FIELD_STR(SoundPlayer, pcssEP->SetSoundChannelPlayDevice);
  LOAD_FIELD_STR(SoundRecorder, pcssEP->SetSoundChannelRecordDevice);
  LOAD_FIELD_INT(SoundBuffers, pcssEP->SetSoundChannelBufferDepth);

  OpalLine * line = potsEP->GetLine("*");
  if (line != NULL) {
    LOAD_FIELD_STR(LineInterfaceDevice, line->GetDevice().Open);
    LOAD_FIELD_ENUM(AEC, line->SetAEC, OpalLineInterfaceDevice::AECLevels);
    LOAD_FIELD_STR(Country, line->GetDevice().SetCountryCodeName);
  }

  LOAD_FIELD_INT2(MinJitter, MaxJitter, SetAudioJitterDelay);

  OpalSilenceDetector::Params silenceParams = GetSilenceDetectParams();
  LOAD_FIELD_INT(SilenceSuppression, silenceParams.m_mode = (OpalSilenceDetector::Mode));
  LOAD_FIELD_INT(SilenceThreshold, silenceParams.m_threshold = );
  LOAD_FIELD_INT(SignalDeadband, silenceParams.m_signalDeadband = 8*);
  LOAD_FIELD_INT(SilenceDeadband, silenceParams.m_silenceDeadband = 8*);
  SetSilenceDetectParams(silenceParams);

  ////////////////////////////////////////
  // Video fields
  config->SetPath(VideoGroup);
  PVideoDevice::OpenArgs videoArgs = GetVideoInputDevice();
  LOAD_FIELD_STR(VideoGrabber, videoArgs.deviceName = (PString));
  LOAD_FIELD_INT(VideoGrabFormat, videoArgs.videoFormat = (PVideoDevice::VideoFormat));
  LOAD_FIELD_INT(VideoGrabSource, videoArgs.channelNumber = );
  LOAD_FIELD_INT(VideoGrabFrameRate, videoArgs.rate = );
  LOAD_FIELD_BOOL(VideoFlipLocal, videoArgs.flip = );
  SetVideoInputDevice(videoArgs);

//  LOAD_FIELD_INT(VideoEncodeQuality, );
//  LOAD_FIELD_INT(VideoEncodeMaxBitRate, );
//  LOAD_FIELD_BOOL(VideoGrabPreview, );
  LOAD_FIELD_BOOL(VideoAutoTransmit, SetAutoStartTransmitVideo);
  LOAD_FIELD_BOOL(VideoAutoReceive, SetAutoStartReceiveVideo);

  videoArgs = GetVideoOutputDevice();
  LOAD_FIELD_BOOL(VideoFlipRemote, videoArgs.flip = );
  SetVideoOutputDevice(videoArgs);

  ////////////////////////////////////////
  // Codec fields
  InitMediaInfo(pcssEP->GetPrefixName(), pcssEP->GetMediaFormats());
  InitMediaInfo(potsEP->GetPrefixName(), potsEP->GetMediaFormats());
#if P_EXPAT
  InitMediaInfo(ivrEP->GetPrefixName(), ivrEP->GetMediaFormats());
#endif

  OpalMediaFormatList mediaFormats;
  mediaFormats += pcssEP->GetMediaFormats();
  mediaFormats += potsEP->GetMediaFormats();
#if P_EXPAT
  mediaFormats += ivrEP->GetMediaFormats();
#endif
  InitMediaInfo("sw", OpalTranscoder::GetPossibleFormats(mediaFormats));

  config->SetPath(CodecsGroup);
  int codecIndex = 0;
  for (;;) {
    wxString groupName;
    groupName.sprintf("%04u", codecIndex);
    if (!config->HasGroup(groupName))
      break;

    config->SetPath(groupName);
    PwxString codecName;
    if (config->Read(CodecNameKey, &codecName) && !codecName.empty()) {
      for (MyMediaList::iterator mm = m_mediaInfo.begin(); mm != m_mediaInfo.end(); ++mm) {
        if (codecName == mm->mediaFormat)
          mm->preferenceOrder = codecIndex;
      }
    }
    config->SetPath("..");
    codecIndex++;
  }

  if (codecIndex > 0)
    ApplyMediaInfo();
  else {
    PStringArray mediaFormatOrder = GetMediaFormatOrder();
    for (PINDEX i = 0; i < mediaFormatOrder.GetSize(); i++) {
      for (MyMediaList::iterator mm = m_mediaInfo.begin(); mm != m_mediaInfo.end(); ++mm) {
        if (mm->mediaFormat == mediaFormatOrder[i])
          mm->preferenceOrder = codecIndex++;
      }
    }
    for (MyMediaList::iterator mm = m_mediaInfo.begin(); mm != m_mediaInfo.end(); ++mm) {
      if (mm->preferenceOrder < 0)
        mm->preferenceOrder = codecIndex++;
    }
    m_mediaInfo.sort();
  }

  ////////////////////////////////////////
  // H.323 fields
  config->SetPath(H323Group);
  LOAD_FIELD_ENUM(DTMFSendMode, h323EP->SetSendUserInputMode, H323Connection::SendUserInputModes);
  LOAD_FIELD_INT(CallIntrusionProtectionLevel, h323EP->SetCallIntrusionProtectionLevel);
  LOAD_FIELD_BOOL(DisableFastStart, h323EP->DisableFastStart);
  LOAD_FIELD_BOOL(DisableH245Tunneling, h323EP->DisableH245Tunneling);
  LOAD_FIELD_BOOL(DisableH245inSETUP, h323EP->DisableH245inSetup);

  PwxString username, password;
  config->Read(GatekeeperModeKey, &m_gatekeeperMode, 0);
  if (m_gatekeeperMode > 0) {
    if (config->Read(GatekeeperTTLKey, &value1))
      h323EP->SetGatekeeperTimeToLive(PTimeInterval(0, value1));

    config->Read(GatekeeperLoginKey, &username, "");
    config->Read(GatekeeperPasswordKey, &password, "");
    h323EP->SetGatekeeperPassword(password, username);

    config->Read(GatekeeperAddressKey, &m_gatekeeperAddress, "");
    config->Read(GatekeeperIdentifierKey, &m_gatekeeperIdentifier, "");
    if (!StartGatekeeper())
      return false;
  }

  ////////////////////////////////////////
  // SIP fields
  config->SetPath(SIPGroup);
  const SIPURL & proxy = sipEP->GetProxy();
  PwxString hostname;
  config->Read(SIPProxyKey, &hostname, PwxString(proxy.GetHostName()));
  config->Read(SIPProxyUsernameKey, &username, PwxString(proxy.GetUserName()));
  config->Read(SIPProxyPasswordKey, &password, PwxString(proxy.GetPassword()));
  sipEP->SetProxy(hostname, username, password);

  if (config->Read(RegistrarTimeToLiveKey, &value1))
    sipEP->SetRegistrarTimeToLive(PTimeInterval(0, value1));

  if (config->Read(RegistrarNameKey, &m_registrarName) &&
      config->Read(RegistrarUsernameKey, &m_registrarUser) &&
      config->Read(RegistrarPasswordKey, &m_registrarPassword))
    StartRegistrar();

  ////////////////////////////////////////
  // Routing fields
  {
#if OPAL_SIP
    if (sipEP != NULL) {
      AddRouteEntry("pots:.*\\*.*\\*.* = sip:<dn2ip>");
      AddRouteEntry("pots:.*           = sip:<da>");
      AddRouteEntry("pc:.*             = sip:<da>");
    }
#if OPAL_H323
    else
#endif
#endif

#if OPAL_H323
    if (h323EP != NULL) {
      AddRouteEntry("pots:.*\\*.*\\*.* = h323:<dn2ip>");
      AddRouteEntry("pots:.*           = h323:<da>");
      AddRouteEntry("pc:.*             = h323:<da>");
    }
#endif

#if P_EXPAT
    if (ivrEP != NULL)
      AddRouteEntry(".*:#  = ivr:"); // A hash from anywhere goes to IVR
#endif

    if (potsEP != NULL && potsEP->GetLine("*") != NULL) {
#if OPAL_H323
      AddRouteEntry("h323:.* = pots:<da>");
#endif
#if OPAL_SIP
      AddRouteEntry("sip:.*  = pots:<da>");
#endif
    }
    else if (pcssEP != NULL) {
#if OPAL_H323
      AddRouteEntry("h323:.* = pc:<da>");
#endif
#if OPAL_SIP
      AddRouteEntry("sip:.*  = pc:<da>");
#endif
    }
  }

  return true;
}


bool MyFrame::StartGatekeeper()
{
  LogWindow << "H.323 registration started for " << m_gatekeeperIdentifier;
  if (!m_gatekeeperIdentifier.IsEmpty() || !m_gatekeeperAddress.IsEmpty())
    LogWindow << '@';
  LogWindow << m_gatekeeperAddress << endl;

  if (h323EP->UseGatekeeper(m_gatekeeperAddress, m_gatekeeperIdentifier)) {
    LogWindow << "H.323 registration successful to " << *h323EP->GetGatekeeper() << endl;
    return true;
  }

  return m_gatekeeperMode < 2;
}


bool MyFrame::StartRegistrar()
{
  if (!sipEP->Register(m_registrarName, m_registrarUser, m_registrarPassword))
    return false;

  LogWindow << "SIP registration started for " << m_registrarUser << '@' << m_registrarName << endl;
  return true;
}


void MyFrame::InitMediaInfo(const char * source, const OpalMediaFormatList & mediaFormats)
{
  for (PINDEX i = 0; i < mediaFormats.GetSize(); i++) {
    const OpalMediaFormat & mediaFormat = mediaFormats[i];
    if (mediaFormat.GetPayloadType() != RTP_DataFrame::MaxPayloadType)
      m_mediaInfo.push_back(MyMedia(source, mediaFormat));
  }
}


void MyFrame::ApplyMediaInfo()
{
  PStringList mediaFormatOrder, mediaFormatMask;

  m_mediaInfo.sort();

  for (MyMediaList::iterator mm = m_mediaInfo.begin(); mm != m_mediaInfo.end(); ++mm) {
    if (mm->preferenceOrder < 0)
      mediaFormatMask.AppendString(mm->mediaFormat);
    else
      mediaFormatOrder.AppendString(mm->mediaFormat);
  }

  if (!mediaFormatOrder.IsEmpty()) {
    SetMediaFormatOrder(mediaFormatOrder);
    SetMediaFormatMask(mediaFormatMask);
  }
}


///////////////////////////////////////////////////////////////////////////////

void MyFrame::OnOptions(wxCommandEvent& event)
{
  OptionsDialog dlg(this);
  dlg.ShowModal();
}

BEGIN_EVENT_TABLE(OptionsDialog, wxDialog)
  ////////////////////////////////////////
  // Codec fields
  EVT_BUTTON(XRCID("AddCodec"), OptionsDialog::AddCodec)
  EVT_BUTTON(XRCID("RemoveCodec"), OptionsDialog::RemoveCodec)
  EVT_BUTTON(XRCID("MoveUpCodec"), OptionsDialog::MoveUpCodec)
  EVT_BUTTON(XRCID("MoveDownCodec"), OptionsDialog::MoveDownCodec)
  EVT_BUTTON(XRCID("ConfigureCodec"), OptionsDialog::ConfigureCodec)
  EVT_LISTBOX(XRCID("AllCodecs"), OptionsDialog::SelectedCodecToAdd)
  EVT_LISTBOX(XRCID("SelectedCodecs"), OptionsDialog::SelectedCodec)

  ////////////////////////////////////////
  // Routing fields
  EVT_BUTTON(XRCID("AddRoute"), OptionsDialog::AddRoute)
  EVT_BUTTON(XRCID("RemoveRoute"), OptionsDialog::RemoveRoute)
  EVT_LIST_ITEM_SELECTED(XRCID("Routes"), OptionsDialog::SelectedRoute)
  EVT_LIST_ITEM_DESELECTED(XRCID("Routes"), OptionsDialog::DeselectedRoute)
  EVT_TEXT(XRCID("RoutePattern"), OptionsDialog::ChangedRouteInfo)
  EVT_TEXT(XRCID("RouteDestination"), OptionsDialog::ChangedRouteInfo)
END_EVENT_TABLE()


#define INIT_FIELD(name, value) \
  m_##name = value; \
  FindWindowByName(name##Key)->SetValidator(wxGenericValidator(&m_##name))

OptionsDialog::OptionsDialog(MyFrame *parent)
  : m_frame(*parent)
{
  PINDEX i;

  SetExtraStyle(GetExtraStyle() | wxWS_EX_VALIDATE_RECURSIVELY);
  wxXmlResource::Get()->LoadDialog(this, parent, "OptionsDialog");

  ////////////////////////////////////////
  // General fields
  INIT_FIELD(Username, m_frame.GetDefaultUserName());
  INIT_FIELD(DisplayName, m_frame.GetDefaultDisplayName());
  INIT_FIELD(RingSoundFileName, "");
  INIT_FIELD(AutoAnswer, m_frame.pcssEP->GetAutoAnswer());
#if P_EXPAT
  INIT_FIELD(IVRScript, m_frame.ivrEP->GetDefaultVXML());
#endif

  ////////////////////////////////////////
  // Networking fields
  INIT_FIELD(Bandwidth, m_frame.h323EP->GetInitialBandwidth());
  INIT_FIELD(TCPPortBase, m_frame.GetTCPPortBase());
  INIT_FIELD(TCPPortMax, m_frame.GetTCPPortMax());
  INIT_FIELD(UDPPortBase, m_frame.GetUDPPortBase());
  INIT_FIELD(UDPPortMax, m_frame.GetUDPPortMax());
  INIT_FIELD(RTPPortBase, m_frame.GetRtpIpPortBase());
  INIT_FIELD(RTPPortMax, m_frame.GetRtpIpPortMax());
  INIT_FIELD(RTPTOS, m_frame.GetRtpIpTypeofService());
  INIT_FIELD(STUNServer, m_frame.GetSTUN() != NULL ? m_frame.GetSTUN()->GetServer() : PString());
  PwxString natRouter;
  if (m_frame.GetTranslationAddress().IsValid())
    natRouter = m_frame.GetTranslationAddress().AsString();
  INIT_FIELD(NATRouter, natRouter);

  ////////////////////////////////////////
  // Sound fields
  INIT_FIELD(SoundBuffers, m_frame.pcssEP->GetSoundChannelBufferDepth());
  INIT_FIELD(MinJitter, m_frame.GetMinAudioJitterDelay());
  INIT_FIELD(MaxJitter, m_frame.GetMaxAudioJitterDelay());
  INIT_FIELD(SilenceSuppression, m_frame.GetSilenceDetectParams().m_mode);
  INIT_FIELD(SilenceThreshold, m_frame.GetSilenceDetectParams().m_threshold);
  INIT_FIELD(SignalDeadband, m_frame.GetSilenceDetectParams().m_signalDeadband/8);
  INIT_FIELD(SilenceDeadband, m_frame.GetSilenceDetectParams().m_silenceDeadband/8);

  // Fill sound player combo box with available devices and set selection
  wxComboBox * combo = (wxComboBox *)FindWindowByName(SoundPlayerKey);
  combo->SetValidator(wxGenericValidator(&m_SoundPlayer));
  PStringList devices = PSoundChannel::GetDeviceNames(PSoundChannel::Player);
  for (i = 0; i < devices.GetSize(); i++)
    combo->Append((const char *)devices[i]);
  m_SoundPlayer = m_frame.pcssEP->GetSoundChannelPlayDevice();

  // Fill sound recorder combo box with available devices and set selection
  combo = (wxComboBox *)FindWindowByName(SoundRecorderKey);
  combo->SetValidator(wxGenericValidator(&m_SoundRecorder));
  devices = PSoundChannel::GetDeviceNames(PSoundChannel::Recorder);
  for (i = 0; i < devices.GetSize(); i++)
    combo->Append((const char *)devices[i]);
  m_SoundRecorder = m_frame.pcssEP->GetSoundChannelRecordDevice();

  // Fill line interface combo box with available devices and set selection
  combo = (wxComboBox *)FindWindowByName(LineInterfaceDeviceKey);
  combo->SetValidator(wxGenericValidator(&m_LineInterfaceDevice));
  OpalLine * line = m_frame.potsEP->GetLine("*");
  if (line != NULL) {
    devices = line->GetDevice().GetAllNames();
    for (i = 0; i < devices.GetSize(); i++)
      combo->Append((const char *)devices[i]);
    m_LineInterfaceDevice = line->GetDevice().GetName();
    INIT_FIELD(AEC, line->GetAEC());
    INIT_FIELD(Country, line->GetDevice().GetCountryCodeName());
  }
  else {
    m_LineInterfaceDevice = "<< None available >>";
    combo->Append(m_LineInterfaceDevice);
    FindWindowByName(AECKey)->Disable();
    FindWindowByName(CountryKey)->Disable();
  }

  ////////////////////////////////////////
  // Video fields
  INIT_FIELD(VideoGrabber, m_frame.GetVideoInputDevice().deviceName);
  INIT_FIELD(VideoGrabFormat, m_frame.GetVideoInputDevice().videoFormat);
  INIT_FIELD(VideoGrabSource, m_frame.GetVideoInputDevice().channelNumber);
  INIT_FIELD(VideoGrabFrameRate, m_frame.GetVideoInputDevice().rate);
  INIT_FIELD(VideoFlipLocal, m_frame.GetVideoInputDevice().flip != FALSE);
//  INIT_FIELD(VideoEncodeQuality, m_frame);
//  INIT_FIELD(VideoEncodeMaxBitRate, m_frame);
//  INIT_FIELD(VideoGrabPreview, m_frame);
  INIT_FIELD(VideoAutoTransmit, m_frame.CanAutoStartTransmitVideo() != FALSE);
  INIT_FIELD(VideoAutoReceive, m_frame.CanAutoStartReceiveVideo() != FALSE);
  INIT_FIELD(VideoFlipRemote, m_frame.GetVideoOutputDevice().flip != FALSE);

  ////////////////////////////////////////
  // Codec fields
  m_AddCodec = (wxButton *)FindWindowByName("AddCodec");
  m_AddCodec->Disable();
  m_RemoveCodec = (wxButton *)FindWindowByName("RemoveCodec");
  m_RemoveCodec->Disable();
  m_MoveUpCodec = (wxButton *)FindWindowByName("MoveUpCodec");
  m_MoveUpCodec->Disable();
  m_MoveDownCodec = (wxButton *)FindWindowByName("MoveDownCodec");
  m_MoveDownCodec ->Disable();
  m_ConfigureCodec = (wxButton *)FindWindowByName("ConfigureCodec");
  m_ConfigureCodec->Disable();

  m_allCodecs = (wxListBox *)FindWindowByName("AllCodecs");
  m_selectedCodecs = (wxListBox *)FindWindowByName("SelectedCodecs");
  for (MyMediaList::iterator mm = m_frame.m_mediaInfo.begin(); mm != m_frame.m_mediaInfo.end(); ++mm) {
    wxString str = mm->sourceProtocol;
    str += ": ";
    str += (const char *)mm->mediaFormat;
    m_allCodecs->Append(str);

    str = (const char *)mm->mediaFormat;
    if (mm->preferenceOrder >= 0 && m_selectedCodecs->FindString(str) < 0)
      m_selectedCodecs->Append(str);
  }

  ////////////////////////////////////////
  // H.323 fields
  INIT_FIELD(DTMFSendMode, m_frame.h323EP->GetSendUserInputMode());
  INIT_FIELD(CallIntrusionProtectionLevel, m_frame.h323EP->GetCallIntrusionProtectionLevel());
  INIT_FIELD(DisableFastStart, m_frame.h323EP->IsFastStartDisabled() != FALSE);
  INIT_FIELD(DisableH245Tunneling, m_frame.h323EP->IsH245TunnelingDisabled() != FALSE);
  INIT_FIELD(DisableH245inSETUP, m_frame.h323EP->IsH245inSetupDisabled() != FALSE);
  INIT_FIELD(GatekeeperMode, m_frame.m_gatekeeperMode);
  INIT_FIELD(GatekeeperAddress, m_frame.m_gatekeeperAddress);
  INIT_FIELD(GatekeeperIdentifier, m_frame.m_gatekeeperIdentifier);
  INIT_FIELD(GatekeeperTTL, m_frame.h323EP->GetGatekeeperTimeToLive().GetSeconds());
  INIT_FIELD(GatekeeperLogin, m_frame.h323EP->GetGatekeeperUsername());
  INIT_FIELD(GatekeeperPassword, m_frame.h323EP->GetGatekeeperPassword());

  ////////////////////////////////////////
  // SIP fields
  INIT_FIELD(SIPProxy, m_frame.sipEP->GetProxy().GetHostName());
  INIT_FIELD(SIPProxyUsername, m_frame.sipEP->GetProxy().GetUserName());
  INIT_FIELD(SIPProxyPassword, m_frame.sipEP->GetProxy().GetPassword());
  INIT_FIELD(RegistrarName, m_frame.m_registrarName);
  INIT_FIELD(RegistrarUsername, m_frame.m_registrarUser);
  INIT_FIELD(RegistrarPassword, m_frame.m_registrarPassword);
  INIT_FIELD(RegistrarTimeToLive, m_frame.sipEP->GetRegistrarTimeToLive().GetSeconds());

  ////////////////////////////////////////
  // Routing fields
  m_SelectedRoute = INT_MAX;

  m_RoutePattern = (wxTextCtrl *)FindWindowByName("RoutePattern");
  m_RouteDestination = (wxTextCtrl *)FindWindowByName("RouteDestination");

  m_AddRoute = (wxButton *)FindWindowByName("AddRoute");
  m_AddRoute->Disable();

  m_RemoveRoute = (wxButton *)FindWindowByName("RemoveRoute");
  m_RemoveRoute->Disable();

  // Fill list box with active routes
  static char const AllSources[] = "<ALL>";
  m_Routes = (wxListCtrl *)FindWindowByName("Routes");
  m_Routes->InsertColumn(0, _T("Source"));
  m_Routes->InsertColumn(1, _T("Pattern"));
  m_Routes->InsertColumn(2, _T("Destination"));
  const OpalManager::RouteTable & routeTable = m_frame.GetRouteTable();
  for (i = 0; i < routeTable.GetSize(); i++) {
    PString pattern = routeTable[i].pattern;
    PINDEX colon = pattern.Find(':');
    wxString source;
    if (colon == P_MAX_INDEX) {
      source = AllSources;
      colon = 0;
    }
    else {
      source = (const char *)pattern.Left(colon);
      if (source == ".*")
        source = AllSources;
      ++colon;
    }
    int pos = m_Routes->InsertItem(INT_MAX, source);
    m_Routes->SetItem(pos, 1, (const char *)pattern.Mid(colon));
    m_Routes->SetItem(pos, 2, (const char *)routeTable[i].destination);
  }

  // Fill combo box with possible protocols
  m_RouteSource = (wxComboBox *)FindWindowByName("RouteSource");
  m_RouteSource->Append(AllSources);
  const PList<OpalEndPoint> & endponts = m_frame.GetEndPoints();
  for (i = 0; i < endponts.GetSize(); i++)
    m_RouteSource->Append((const char *)endponts[i].GetPrefixName());
  m_RouteSource->SetSelection(0);


#if PTRACING
  ////////////////////////////////////////
  // Tracing fields
  INIT_FIELD(EnableTracing, m_frame.m_enableTracing);
  INIT_FIELD(TraceLevelThreshold, PTrace::GetLevel());
  INIT_FIELD(TraceLevelNumber, (PTrace::GetOptions()&PTrace::TraceLevel) != 0);
  INIT_FIELD(TraceFileLine, (PTrace::GetOptions()&PTrace::FileAndLine) != 0);
  INIT_FIELD(TraceBlocks, (PTrace::GetOptions()&PTrace::Blocks) != 0);
  INIT_FIELD(TraceDateTime, (PTrace::GetOptions()&PTrace::DateAndTime) != 0);
  INIT_FIELD(TraceTimestamp, (PTrace::GetOptions()&PTrace::Timestamp) != 0);
  INIT_FIELD(TraceThreadName, (PTrace::GetOptions()&PTrace::Thread) != 0);
  INIT_FIELD(TraceThreadAddress, (PTrace::GetOptions()&PTrace::ThreadAddress) != 0);
  INIT_FIELD(TraceFileName, m_frame.m_traceFileName);
#endif // PTRACING
}


#define SAVE_FIELD(name, set) \
  set(m_##name); \
  config->Write(name##Key, m_##name)

#define SAVE_FIELD2(name1, name2, set) \
  set(m_##name1, m_##name2); \
  config->Write(name1##Key, m_##name1); \
  config->Write(name2##Key, m_##name2)

bool OptionsDialog::TransferDataFromWindow()
{
  if (!wxDialog::TransferDataFromWindow())
    return false;

  wxConfigBase * config = wxConfig::Get();

  ////////////////////////////////////////
  // General fields
  config->SetPath(GeneralGroup);
  SAVE_FIELD(Username, m_frame.SetDefaultUserName);
  SAVE_FIELD(DisplayName, m_frame.SetDefaultDisplayName);
  SAVE_FIELD(AutoAnswer, m_frame.pcssEP->SetAutoAnswer);
#if P_EXPAT
  SAVE_FIELD(IVRScript, m_frame.ivrEP->SetDefaultVXML);
#endif

  ////////////////////////////////////////
  // Networking fields
  config->SetPath(NetworkingGroup);
  SAVE_FIELD(Bandwidth, m_frame.h323EP->SetInitialBandwidth);
  SAVE_FIELD2(TCPPortBase, TCPPortMax, m_frame.SetTCPPorts);
  SAVE_FIELD2(UDPPortBase, UDPPortMax, m_frame.SetUDPPorts);
  SAVE_FIELD2(RTPPortBase, RTPPortMax, m_frame.SetRtpIpPorts);
  SAVE_FIELD(RTPTOS, m_frame.SetRtpIpTypeofService);
  SAVE_FIELD(STUNServer, m_frame.SetSTUNServer);
  SAVE_FIELD(NATRouter, m_frame.SetTranslationAddress);

  ////////////////////////////////////////
  // Sound fields
  config->SetPath(AudioGroup);
  SAVE_FIELD(SoundPlayer, m_frame.pcssEP->SetSoundChannelPlayDevice);
  SAVE_FIELD(SoundRecorder, m_frame.pcssEP->SetSoundChannelRecordDevice);
  SAVE_FIELD(SoundBuffers, m_frame.pcssEP->SetSoundChannelBufferDepth);
//  SAVE_FIELD(LineInterfaceDevice, m_frame.potsEP->);
//  SAVE_FIELD(AEC, m_frame.potsEP->);
//  SAVE_FIELD(Country, m_frame.potsEP->);
  SAVE_FIELD2(MinJitter, MaxJitter, m_frame.SetAudioJitterDelay);

  OpalSilenceDetector::Params silenceParams;
  SAVE_FIELD(SilenceSuppression, silenceParams.m_mode=(OpalSilenceDetector::Mode));
  SAVE_FIELD(SilenceThreshold, silenceParams.m_threshold=);
  SAVE_FIELD(SignalDeadband, silenceParams.m_signalDeadband=8*);
  SAVE_FIELD(SilenceDeadband, silenceParams.m_silenceDeadband=8*);
  m_frame.SetSilenceDetectParams(silenceParams);

  ////////////////////////////////////////
  // Video fields
  config->SetPath(VideoGroup);
  PVideoDevice::OpenArgs grabber = m_frame.GetVideoInputDevice();
  SAVE_FIELD(VideoGrabber, grabber.deviceName = (PString));
  SAVE_FIELD(VideoGrabFormat, grabber.videoFormat = (PVideoDevice::VideoFormat));
  SAVE_FIELD(VideoGrabSource, grabber.channelNumber = );
  SAVE_FIELD(VideoGrabFrameRate, grabber.rate = );
  SAVE_FIELD(VideoFlipLocal, grabber.flip = );
  m_frame.SetVideoInputDevice(grabber);
//  SAVE_FIELD(VideoEncodeQuality, );
//  SAVE_FIELD(VideoEncodeMaxBitRate, );
//  SAVE_FIELD(VideoGrabPreview, );
  SAVE_FIELD(VideoAutoTransmit, m_frame.SetAutoStartTransmitVideo);
  SAVE_FIELD(VideoAutoReceive, m_frame.SetAutoStartReceiveVideo);
//  SAVE_FIELD(VideoFlipRemote, );

  ////////////////////////////////////////
  // Codec fields
  MyMediaList::iterator mm;
  for (mm = m_frame.m_mediaInfo.begin(); mm != m_frame.m_mediaInfo.end(); ++mm)
    mm->preferenceOrder = -1;

  int codecIndex;
  for (codecIndex = 0; codecIndex < m_selectedCodecs->GetCount(); codecIndex++) {
    PwxString selectedFormat = m_selectedCodecs->GetString(codecIndex);
    for (mm = m_frame.m_mediaInfo.begin(); mm != m_frame.m_mediaInfo.end(); ++mm) {
      if (selectedFormat == mm->mediaFormat) {
        mm->preferenceOrder = codecIndex;
        break;
      }
    }
  }

  m_frame.ApplyMediaInfo();

  config->DeleteGroup(CodecsGroup);
  for (mm = m_frame.m_mediaInfo.begin(); mm != m_frame.m_mediaInfo.end(); ++mm) {
    if (mm->preferenceOrder >= 0) {
      wxString groupName;
      groupName.sprintf("%s/%04u", CodecsGroup, mm->preferenceOrder);
      config->SetPath(groupName);
      config->Write(CodecNameKey, mm->mediaFormat);
    }
  }


  ////////////////////////////////////////
  // H.323 fields
  config->SetPath(H323Group);
  m_frame.h323EP->SetSendUserInputMode((H323Connection::SendUserInputModes)m_DTMFSendMode);
  config->Write(DTMFSendModeKey, m_DTMFSendMode);
  SAVE_FIELD(CallIntrusionProtectionLevel, m_frame.h323EP->SetCallIntrusionProtectionLevel);
  SAVE_FIELD(DisableFastStart, m_frame.h323EP->DisableFastStart);
  SAVE_FIELD(DisableH245Tunneling, m_frame.h323EP->DisableH245Tunneling);
  SAVE_FIELD(DisableH245inSETUP, m_frame.h323EP->DisableH245inSetup);

  config->Write(GatekeeperTTLKey, m_GatekeeperTTL);
  m_frame.h323EP->SetGatekeeperTimeToLive(PTimeInterval(0, m_GatekeeperTTL));

  if (m_frame.m_gatekeeperMode != m_GatekeeperMode ||
      m_frame.m_gatekeeperAddress != m_GatekeeperAddress ||
      m_frame.m_gatekeeperIdentifier != m_GatekeeperIdentifier ||
      m_frame.h323EP->GetGatekeeperUsername() != m_GatekeeperLogin.c_str() ||
      m_frame.h323EP->GetGatekeeperPassword() != m_GatekeeperPassword.c_str()) {
    SAVE_FIELD(GatekeeperMode, m_frame.m_gatekeeperMode = );
    SAVE_FIELD(GatekeeperAddress, m_frame.m_gatekeeperAddress = );
    SAVE_FIELD(GatekeeperIdentifier, m_frame.m_gatekeeperIdentifier = );
    SAVE_FIELD2(GatekeeperPassword, GatekeeperLogin, m_frame.h323EP->SetGatekeeperPassword);

    if (!m_frame.StartGatekeeper()) {
      // exit app
    }
  }

  ////////////////////////////////////////
  // SIP fields
  config->SetPath(SIPGroup);
  m_frame.sipEP->SetProxy(m_SIPProxy, m_SIPProxyUsername, m_SIPProxyPassword);
  config->Write(SIPProxyKey, m_SIPProxy);
  config->Write(SIPProxyUsernameKey, m_SIPProxyUsername);
  config->Write(SIPProxyPasswordKey, m_SIPProxyPassword);

  config->Write(RegistrarTimeToLiveKey, m_RegistrarTimeToLive);
  m_frame.sipEP->SetRegistrarTimeToLive(PTimeInterval(0, m_RegistrarTimeToLive));

  if (m_frame.m_registrarName != m_RegistrarName ||
      m_frame.m_registrarUser != m_RegistrarUsername ||
      m_frame.m_registrarPassword != m_RegistrarPassword) {
    SAVE_FIELD(RegistrarName, m_frame.m_registrarName =);
    SAVE_FIELD(RegistrarUsername, m_frame.m_registrarUser =);
    SAVE_FIELD(RegistrarPassword, m_frame.m_registrarPassword =);
    m_frame.StartRegistrar();
  }

  ////////////////////////////////////////
  // Routing fields


#if PTRACING
  ////////////////////////////////////////
  // Tracing fields
  config->SetPath(TracingGroup);
  int traceOptions = 0;
  if (m_TraceLevelNumber)
    traceOptions |= PTrace::TraceLevel;
  if (m_TraceFileLine)
    traceOptions |= PTrace::FileAndLine;
  if (m_TraceBlocks)
    traceOptions |= PTrace::Blocks;
  if (m_TraceDateTime)
    traceOptions |= PTrace::DateAndTime;
  if (m_TraceTimestamp)
    traceOptions |= PTrace::Timestamp;
  if (m_TraceThreadName)
    traceOptions |= PTrace::Thread;
  if (m_TraceThreadAddress)
    traceOptions |= PTrace::ThreadAddress;

  config->Write(EnableTracingKey, m_EnableTracing);
  config->Write(TraceLevelThresholdKey, m_TraceLevelThreshold);
  config->Write(TraceFileNameKey, m_TraceFileName);
  config->Write(TraceOptionsKey, traceOptions);

  // Check for stopping tracing
  if (m_frame.m_enableTracing && (!m_EnableTracing || m_TraceFileName.empty()))
    PTrace::SetStream(NULL);
  else if (m_EnableTracing && (!m_frame.m_enableTracing || m_frame.m_traceFileName != m_TraceFileName))
    PTrace::Initialise(m_TraceLevelThreshold, m_TraceFileName, traceOptions);
  else {
    PTrace::SetLevel(m_TraceLevelThreshold);
    PTrace::SetOptions(traceOptions);
    PTrace::ClearOptions(~traceOptions);
  }

  m_frame.m_enableTracing = m_EnableTracing;
  m_frame.m_traceFileName = m_TraceFileName;
#endif // PTRACING

  return true;
}


////////////////////////////////////////
// General fields

void OptionsDialog::BrowseSoundFile(wxCommandEvent & event)
{
}


void OptionsDialog::PlaySoundFile(wxCommandEvent & event)
{
}


////////////////////////////////////////
// Networking fields

void OptionsDialog::BandwidthClass(wxCommandEvent & event)
{
}


void OptionsDialog::SelectedLocalInterface(wxCommandEvent & event)
{
}


void OptionsDialog::AddInterface(wxCommandEvent & event)
{
}


void OptionsDialog::RemoveInterface(wxCommandEvent & event)
{
}


////////////////////////////////////////
// Codec fields

void OptionsDialog::AddCodec(wxCommandEvent & event)
{
  int insertionPoint = -1;
  wxArrayInt destinationSelections;
  if (m_selectedCodecs->GetSelections(destinationSelections) > 0)
    insertionPoint = destinationSelections[0];

  wxArrayInt sourceSelections;
  m_allCodecs->GetSelections(sourceSelections);
  for (size_t i = 0; i < sourceSelections.GetCount(); i++) {
    wxString value = m_allCodecs->GetString(sourceSelections[i]);
    value.Remove(0, value.Find(':')+2);
    if (m_selectedCodecs->FindString(value) < 0) {
      if (insertionPoint < 0)
        m_selectedCodecs->Append(value);
      else
        m_selectedCodecs->InsertItems(1, &value, insertionPoint);
    }
    m_allCodecs->Deselect(sourceSelections[i]);
  }

  m_AddCodec->Enable(false);
}


void OptionsDialog::RemoveCodec(wxCommandEvent & event)
{
  wxArrayInt selections;
  m_selectedCodecs->GetSelections(selections);
  for (int i = selections.GetCount()-1; i >= 0; i--)
    m_selectedCodecs->Delete(selections[i]);
  m_RemoveCodec->Enable(false);
  m_MoveUpCodec->Enable(false);
  m_MoveDownCodec->Enable(false);
}


void OptionsDialog::MoveUpCodec(wxCommandEvent & event)
{
  wxArrayInt selections;
  m_selectedCodecs->GetSelections(selections);
  wxString value = m_selectedCodecs->GetString(selections[0]);
  m_selectedCodecs->Delete(selections[0]);
  m_selectedCodecs->InsertItems(1, &value, selections[0]-1);
  m_selectedCodecs->SetSelection(selections[0]-1);
  m_MoveUpCodec->Enable(selections[0] > 1);
  m_MoveDownCodec->Enable(true);
}


void OptionsDialog::MoveDownCodec(wxCommandEvent & event)
{
  wxArrayInt selections;
  m_selectedCodecs->GetSelections(selections);
  wxString value = m_selectedCodecs->GetString(selections[0]);
  m_selectedCodecs->Delete(selections[0]);
  m_selectedCodecs->InsertItems(1, &value, selections[0]+1);
  m_selectedCodecs->SetSelection(selections[0]+1);
  m_MoveUpCodec->Enable(true);
  m_MoveDownCodec->Enable(selections[0] < m_selectedCodecs->GetCount()-2);
}


void OptionsDialog::ConfigureCodec(wxCommandEvent & event)
{
}


void OptionsDialog::SelectedCodecToAdd(wxCommandEvent & /*event*/)
{
  wxArrayInt selections;
  m_AddCodec->Enable(m_allCodecs->GetSelections(selections) > 0);
}


void OptionsDialog::SelectedCodec(wxCommandEvent & /*event*/)
{
  wxArrayInt selections;
  int count = m_selectedCodecs->GetSelections(selections);
  m_RemoveCodec->Enable(count > 0);
  m_MoveUpCodec->Enable(count == 1 && selections[0] > 0);
  m_MoveDownCodec->Enable(count == 1 && selections[0] < m_selectedCodecs->GetCount()-1);
  m_ConfigureCodec->Enable(count == 1);
}


////////////////////////////////////////
// H.323 fields

void OptionsDialog::AddAlias(wxCommandEvent & event)
{
}


void OptionsDialog::RemoveAlias(wxCommandEvent & event)
{
}


////////////////////////////////////////
// Routing fields

void OptionsDialog::AddRoute(wxCommandEvent & )
{
  int pos = m_Routes->InsertItem(m_SelectedRoute, m_RouteSource->GetValue());
  m_Routes->SetItem(pos, 1, m_RoutePattern->GetValue());
  m_Routes->SetItem(pos, 2, m_RouteDestination->GetValue());
}


void OptionsDialog::RemoveRoute(wxCommandEvent & event)
{
  m_Routes->DeleteItem(m_SelectedRoute);
}


void OptionsDialog::SelectedRoute(wxListEvent & event)
{
  m_SelectedRoute = event.GetIndex();
  m_RemoveRoute->Enable(true);
}


void OptionsDialog::DeselectedRoute(wxListEvent & event)
{
  m_SelectedRoute = INT_MAX;
  m_RemoveRoute->Enable(false);
}


void OptionsDialog::ChangedRouteInfo(wxCommandEvent & event)
{
  m_AddRoute->Enable(!m_RoutePattern->GetValue().IsEmpty() && !m_RouteDestination->GetValue().IsEmpty());
}


///////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(CallDialog, wxDialog)
  EVT_TEXT(XRCID("Address"), CallDialog::OnAddressChange)
END_EVENT_TABLE()

CallDialog::CallDialog(wxWindow *parent)
{
  wxXmlResource::Get()->LoadDialog(this, parent, "CallDialog");

  m_ok = (wxButton *)FindWindowByName("wxID_OK", this);
  m_ok->Disable();

  m_AddressCtrl = (wxComboBox *)FindWindowByName("Address");
  m_AddressCtrl->SetValidator(wxGenericValidator(&m_Address));

  // Temprary, must get from config
  m_AddressCtrl->Append("h323:h323.voxgratia.org");
  m_AddressCtrl->Append("sip:gw.voxgratia.org");
}


void CallDialog::OnAddressChange(wxCommandEvent & WXUNUSED(event))
{
  m_ok->Enable(!m_AddressCtrl->GetValue().IsEmpty());
}


///////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(SpeedDialDialog, wxDialog)
  EVT_TEXT(XRCID("SpeedDialName"), SpeedDialDialog::OnChange)
  EVT_TEXT(XRCID("SpeedDialNumber"), SpeedDialDialog::OnChange)
END_EVENT_TABLE()

SpeedDialDialog::SpeedDialDialog(MyFrame * parent)
  : m_frame(*parent)
{
  wxXmlResource::Get()->LoadDialog(this, parent, "SpeedDialDialog");

  m_ok = (wxButton *)FindWindowByName("wxID_OK", this);

  m_nameCtrl = (wxTextCtrl *)FindWindowByName("SpeedDialName");
  m_nameCtrl->SetValidator(wxGenericValidator(&m_Name));

  m_numberCtrl = (wxTextCtrl *)FindWindowByName("SpeedDialNumber");
  m_numberCtrl->SetValidator(wxGenericValidator(&m_Number));

  FindWindowByName("SpeedDialAddress")->SetValidator(wxGenericValidator(&m_Address));
  FindWindowByName("SpeedDialDescription")->SetValidator(wxGenericValidator(&m_Description));

  m_inUse = (wxStaticText *)FindWindowByName("SpeedDialInUse");
  m_ambiguous = (wxStaticText *)FindWindowByName("SpeedDialAmbiguous");
}


void SpeedDialDialog::OnChange(wxCommandEvent & WXUNUSED(event))
{
  wxString newName = m_nameCtrl->GetValue();
  bool inUse = newName != m_Name && m_frame.HasSpeedDialName(newName);
  m_inUse->Show(inUse);

  m_ok->Enable(!newName.IsEmpty() && !inUse);

  m_ambiguous->Show(m_frame.HasSpeedDialNumber(m_numberCtrl->GetValue(), m_Number));
}


///////////////////////////////////////////////////////////////////////////////

MyPCSSEndPoint::MyPCSSEndPoint(MyFrame & ep)
  : OpalPCSSEndPoint(ep),
    frame(ep)
{
  m_autoAnswer = false;
}


PString MyPCSSEndPoint::OnGetDestination(const OpalPCSSConnection & /*connection*/)
{
  // Dialog to prompt for address
  return PString();
}


void MyPCSSEndPoint::OnShowIncoming(const OpalPCSSConnection & connection)
{
  m_incomingConnectionToken = connection.GetToken();

  if (m_autoAnswer)
    AcceptIncomingConnection(m_incomingConnectionToken);
  else {
    PTime now;
    LogWindow << "\nCall on " << now.AsString("w h:mma")
              << " from " << connection.GetRemotePartyName()
              << ", answer (Y/N)? " << endl;
  }
}


BOOL MyPCSSEndPoint::OnShowOutgoing(const OpalPCSSConnection & connection)
{
  PTime now;
  LogWindow << connection.GetRemotePartyName() << " is ringing on "
            << now.AsString("w h:mma") << " ..." << endl;
  return TRUE;
}


///////////////////////////////////////////////////////////////////////////////
#if OPAL_H323

MyH323EndPoint::MyH323EndPoint(MyFrame & f)
  : H323EndPoint(f),
    frame(f)
{
}


void MyH323EndPoint::OnRegistrationConfirm()
{
//  LogWindow << "H.323 registration successful." << endl;
}

#endif


///////////////////////////////////////////////////////////////////////////////
#if OPAL_SIP

MySIPEndPoint::MySIPEndPoint(MyFrame & f)
  : SIPEndPoint(f),
    frame(f)
{
}


void MySIPEndPoint::OnRegistered()
{
  LogWindow << "SIP registration successful." << endl;
}

#endif


// End of File ///////////////////////////////////////////////////////////////
