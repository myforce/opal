/*
 * main.h
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
 * Contributor(s): ______________________________________.
 *
 * $Log: main.h,v $
 * Revision 1.11  2004/07/04 12:53:09  rjongbloed
 * Added support for route editing.
 *
 * Revision 1.10  2004/06/05 14:37:03  rjongbloed
 * More implemntation of options dialog.
 *
 * Revision 1.9  2004/05/25 12:56:07  rjongbloed
 * Added all silence suppression modes to Options dialog.
 *
 * Revision 1.8  2004/05/24 13:44:04  rjongbloed
 * More implementation on OPAL OpenPhone.
 *
 * Revision 1.7  2004/05/15 12:18:23  rjongbloed
 * More work on wxWindows based OpenPhone
 *
 * Revision 1.6  2004/05/12 12:41:38  rjongbloed
 * More work on wxWindows based OpenPhone
 *
 * Revision 1.5  2004/05/06 13:23:43  rjongbloed
 * Work on wxWindows based OpenPhone
 *
 * Revision 1.4  2004/05/01 13:38:05  rjongbloed
 * Some early implementation of wxWIndows based OPAL GUI client.
 *
 */

#ifndef _OpenPhone_MAIN_H
#define _OpenPhone_MAIN_H

#include <wx/wx.h>

#include <ptlib.h>

#include <opal/manager.h>
#include <opal/pcss.h>


class MyFrame;

class SIPEndPoint;
class H323EndPoint;
class OpalPOTSEndPoint;
class OpalIVREndPoint;

class wxSplitterWindow;
class wxListCtrl;
class wxListEvent;


class PwxString : public wxString
{
  public:
    PwxString() { }
    PwxString(const wxString & str) : wxString(str) { }
    PwxString(const PString & str) : wxString((const char *)str) { }
    PwxString(const char * str) : wxString(str) { }

    PwxString & operator=(const wxString & str) { wxString::operator=(str); return *this; }
    PwxString & operator=(const PString & str) { wxString::operator=((const char *)str); return *this; }
    PwxString & operator=(const char * str) { wxString::operator=(str); return *this; }

    operator PString() const { return c_str(); }
    operator PIPSocket::Address() const { return PIPSocket::Address(PString(c_str())); }
    friend ostream & operator<<(ostream & stream, const PwxString & string) { return stream << string.c_str(); }
};

class MyPCSSEndPoint : public OpalPCSSEndPoint
{
  PCLASSINFO(MyPCSSEndPoint, OpalPCSSEndPoint);

  public:
    MyPCSSEndPoint(MyFrame & frame);

    bool GetAutoAnswer() const { return m_autoAnswer; }
    void SetAutoAnswer(bool answer) { m_autoAnswer = answer; }

    PString m_incomingConnectionToken;

  private:
    virtual PString OnGetDestination(const OpalPCSSConnection & connection);
    virtual void OnShowIncoming(const OpalPCSSConnection & connection);
    virtual BOOL OnShowOutgoing(const OpalPCSSConnection & connection);

    PString m_destinationAddress;
    bool    m_autoAnswer;

    MyFrame & frame;
};


class CallDialog : public wxDialog
{
  public:
    CallDialog(wxWindow *parent);

    PwxString m_Address;

  private:
    void OnAddressChange(wxCommandEvent & event);

    wxComboBox * m_AddressCtrl;
    wxButton   * m_ok;

    DECLARE_EVENT_TABLE()
};


class MyFrame;

class OptionsDialog : public wxDialog
{
  public:
    OptionsDialog(MyFrame *parent);
    virtual bool TransferDataFromWindow();

  private:
    void BrowseSoundFile(wxCommandEvent & event);
    void PlaySoundFile(wxCommandEvent & event);
    void BandwidthClass(wxCommandEvent & event);
    void SelectedLocalInterface(wxCommandEvent & event);
    void AddInterface(wxCommandEvent & event);
    void RemoveInterface(wxCommandEvent & event);
    void SelectedCodecToAdd(wxCommandEvent & event);
    void AddCodec(wxCommandEvent & event);
    void RemoveCodec(wxCommandEvent & event);
    void MoveUpCodec(wxCommandEvent & event);
    void MoveDownCodec(wxCommandEvent & event);
    void ConfigureCodec(wxCommandEvent & event);
    void SelectedCodec(wxCommandEvent & event);
    void AddAlias(wxCommandEvent & event);
    void RemoveAlias(wxCommandEvent & event);
    void AddRoute(wxCommandEvent & event);
    void RemoveRoute(wxCommandEvent & event);
    void SelectedRoute(wxListEvent & event);
    void DeselectedRoute(wxListEvent & event);
    void ChangedRouteInfo(wxCommandEvent & event);

    MyFrame & mainFrame;

    PwxString m_Username;
    PwxString m_DisplayName;
    PwxString m_RingSoundFileName;
    bool      m_AutoAnswer;
#if P_EXPAT
    PwxString m_IVRScript;
#endif

    int       m_Bandwidth;
    int       m_TCPPortBase;
    int       m_TCPPortMax;
    int       m_UDPPortBase;
    int       m_UDPPortMax;
    int       m_RTPPortBase;
    int       m_RTPPortMax;
    int       m_RTPTOS;
    PwxString m_STUNServer;
    PwxString m_NATRouter;
    PwxString m_InterfaceToAdd;

    PwxString m_SoundPlayer;
    PwxString m_SoundRecorder;
    int       m_SoundBuffers;
    PwxString m_LineInterfaceDevice;
    int       m_AEC;
    PwxString m_Country;
    int       m_MinJitter;
    int       m_MaxJitter;
    int       m_SilenceSuppression;
    int       m_SilenceThreshold;
    int       m_SignalDeadband;
    int       m_SilenceDeadband;

    PwxString m_VideoGrabber;
    int       m_VideoGrabFormat;
    int       m_VideoGrabSource;
    int       m_VideoEncodeQuality;
    int       m_VideoGrabFrameRate;
    int       m_VideoEncodeMaxBitRate;
    bool      m_VideoGrabPreview;
    bool      m_VideoFlipLocal;
    bool      m_VideoAutoTransmit;
    bool      m_VideoAutoReceive;
    bool      m_VideoFlipRemote;

    PwxString m_NewAlias;
    int       m_GatekeeperMode;
    PwxString m_GatekeeperAddress;
    PwxString m_GatekeeperIdentifier;
    int       m_GatekeeperTTL;
    PwxString m_GatekeeperLogin;
    PwxString m_GatekeeperPassword;
    int       m_DTMFSendMode;
    int       m_CallIntrusionProtectionLevel;
    bool      m_DisableFastStart;
    bool      m_DisableH245Tunneling;
    bool      m_DisableH245inSETUP;

    PwxString m_SIPProxy;
    PwxString m_SIPProxyUsername;
    PwxString m_SIPProxyPassword;
    PwxString m_RegistrarName;
    PwxString m_RegistrarUsername;
    PwxString m_RegistrarPassword;

    bool      m_EnableTracing;
    int       m_TraceLevelThreshold;
    bool      m_TraceLevelNumber;
    bool      m_TraceFileLine;
    bool      m_TraceBlocks;
    bool      m_TraceDateTime;
    bool      m_TraceTimestamp;
    bool      m_TraceThreadName;
    bool      m_TraceThreadAddress;
    PwxString m_TraceFileName;

    wxListCtrl * m_Routes;
    int          m_SelectedRoute;
    wxComboBox * m_RouteSource;
    wxTextCtrl * m_RoutePattern;
    wxTextCtrl * m_RouteDestination;
    wxButton   * m_AddRoute;
    wxButton   * m_RemoveRoute;

    DECLARE_EVENT_TABLE()
};


class MyFrame : public wxFrame, public OpalManager
{
  public:
    MyFrame();
    ~MyFrame();

    bool Initialise();

  private:
    void OnClose(wxCloseEvent& event);

    void OnAdjustMenus(wxMenuEvent& event);

    void OnMenuQuit(wxCommandEvent& event);
    void OnMenuAbout(wxCommandEvent& event);
    void OnMenuCall(wxCommandEvent& event);
    void OnMenuAnswer(wxCommandEvent& event);
    void OnMenuHangUp(wxCommandEvent& event);
    void OnViewLarge(wxCommandEvent& event);
    void OnViewSmall(wxCommandEvent& event);
    void OnViewList(wxCommandEvent& event);
    void OnViewDetails(wxCommandEvent& event);
    void OnOptions(wxCommandEvent& event);
    void OnSpeedDial(wxListEvent& event);

    void MakeCall(const PwxString & address);

    virtual void OnEstablishedCall(
      OpalCall & call   /// Call that was completed
    );
    virtual void OnClearedCall(
      OpalCall & call   /// Connection that was established
    );
    virtual BOOL OnOpenMediaStream(
      OpalConnection & connection,  /// Connection that owns the media stream
      OpalMediaStream & stream    /// New media stream being opened
    );
    virtual void OnUserInputString(
      OpalConnection & connection,  /// Connection input has come from
      const PString & value         /// String value of indication
    );

    enum { SpeedDialsID = 100 };
    enum SpeedDialViews {
      e_ViewLarge,
      e_ViewSmall,
      e_ViewList,
      e_ViewDetails,
      e_NumViews
    };
    void RecreateSpeedDials(
      SpeedDialViews view
    );

    enum { e_NameColumn, e_NumberColumn, e_AddressColumn };

    wxSplitterWindow         * m_splitter;
    wxListCtrl               * m_speedDials;
    wxTextCtrl               * m_logWindow;

    MyPCSSEndPoint   * pcssEP;
    OpalPOTSEndPoint * potsEP;
#if OPAL_H323
    H323EndPoint     * h323EP;
#endif
#if OPAL_SIP
    SIPEndPoint      * sipEP;
#endif
#if P_EXPAT
    OpalIVREndPoint  * ivrEP;
#endif

    bool     m_enableTracing;
    wxString m_traceFileName;
    PString  m_currentCallToken;

    DECLARE_EVENT_TABLE()

  friend class OptionsDialog;
};


class OpenPhoneApp : public wxApp, public PProcess
{
    PCLASSINFO(OpenPhoneApp, PProcess);

  public:
    OpenPhoneApp();

    void Main(); // Dummy function

      // FUnction from wxWindows
    virtual bool OnInit();
};


#endif


// End of File ///////////////////////////////////////////////////////////////
