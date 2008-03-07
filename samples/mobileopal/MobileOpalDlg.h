/*
 * MobileOpalDlg.h
 *
 * Sample Windows Mobile application.
 *
 * Open Phone Abstraction Library
 *
 * Copyright (c) 2008 Vox Lucida
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
 * The Initial Developer of the Original Code is Vox Lucida (Robert Jongbloed)
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#pragma once
#include "afxwin.h"

// CMobileOpalDlg dialog
class CMobileOpalDlg : public CDialog
{
  // Construction
public:
  CMobileOpalDlg(CWnd* pParent = NULL);	// standard constructor
  ~CMobileOpalDlg();

  // Dialog Data
  enum { IDD = IDD_MOBILEOPAL_DIALOG };

protected:
  virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

  // Implementation
protected:
  HICON m_hIcon;
  CCommandBar m_dlgCommandBar;
  OpalHandle m_opal;
  CStatic m_ctrlStatus;
  CEdit m_ctrlAddress;
  CString m_callAddress;
  CStringA m_incomingCallToken;
  CStringA m_currentCallToken;

  // Generated message map functions
  virtual BOOL OnInitDialog();
#if defined(_DEVICE_RESOLUTION_AWARE) && !defined(WIN32_PLATFORM_WFSP)
  afx_msg void OnSize(UINT /*nType*/, int /*cx*/, int /*cy*/);
#endif
  DECLARE_MESSAGE_MAP()

  void InitialiseOPAL();
  void ErrorBox(UINT strId);
  void SetStatusText(UINT ids);
  void SetCallButton(bool enabled, UINT strId = 0);

public:
  afx_msg void OnTimer(UINT_PTR nIDEvent);
  afx_msg void OnOK();
  afx_msg void OnCancel();
  afx_msg void OnCallAnswer();
  afx_msg void OnChangedAddress();
  afx_msg void OnMenuOptionsGeneral();
  afx_msg void OnMenuOptionsH323();
  afx_msg void OnMenuOptionsSIP();
  afx_msg void OnExit();
};
