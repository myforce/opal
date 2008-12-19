/*
 * main.cxx
 *
 * OPAL application source file for sending/receiving faxes via T.38
 *
 * Copyright (c) 2008 Vox Lucida Pty. Ltd.
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

#include "precompile.h"
#include "main.h"


PCREATE_PROCESS(FaxOPAL);


FaxOPAL::FaxOPAL()
  : PProcess("OPAL T.38 Fax", "FaxOPAL", OPAL_MAJOR, OPAL_MINOR, ReleaseCode, OPAL_BUILD)
  , m_manager(NULL)
{
}


FaxOPAL::~FaxOPAL()
{
  delete m_manager;
}


void FaxOPAL::Main()
{
  PArgList & args = GetArguments();

  args.Parse("a-audio."
             "d-directory:"
             "g-gk-host:"
             "G-gk-id:"
             "h-help."
             "H-h323:"
             "m-mode:"
             "N-stun:"
             "p-password:"
             "P-proxy:"
             "r-register:"
             "s-spandsp:"
             "S-sip:"
             "u-user:"
#if PTRACING
             "o-output:"             "-no-output."
             "t-trace."              "-no-trace."
#endif
             , FALSE);

#if PTRACING
  PTrace::Initialise(args.GetOptionCount('t'),
                     args.HasOption('o') ? (const char *)args.GetOptionString('o') : NULL,
         PTrace::Blocks | PTrace::Timestamp | PTrace::Thread | PTrace::FileAndLine);
#endif

  if (args.HasOption('h') || args.GetCount() == 0) {
    cerr << "usage: " << GetFile().GetTitle() << " [ options ] filename [ url ]\n"
            "\n"
            "Available options are:\n"
            "  --help                  : print this help message.\n"
            "  -d or --directory dir   : Set default directory for fax receive\n"
            "  -s or --spandsp exe     : Set location of spandsp_util.exe\n"
            "  -a or --audio           : Send fax as G.711 audio\n"
            "  -m or --mode m          : T.38 synchronisation mode:\n"
            "                            \"Wait\" wait for remote to switch,\n"
            "                            \"Timeout\" switch after a short time\n"
            "                            \"UserInput\" send/detect CNG/CED tones\n"
            "                            \"InBand\" send/detect CNG/CED tones in band\n"
            "  -u or --user name       : Set local username, defaults to OS username.\n"
            "  -p or --password pwd    : Set password for authentication.\n"
#if OPAL_SIP
            "  -S or --sip interface   : SIP listens on interface, defaults to udp$*:5060, 'x' disables.\n"
            "  -r or --register server : SIP registration to server.\n"
            "  -P or --proxy url       : SIP outbound proxy.\n"
#endif
#if OPAL_H323
            "  -H or --h323 interface  : H.323 listens on interface, defaults to tcp$*:1720, 'x' disables.\n"
            "  -g or --gk-host host    : H.323 gatekeeper host.\n"
            "  -G or --gk-id id        : H.323 gatekeeper identifier.\n"
#endif
            "  -N or --stun server     : Set NAT traversal STUN server.\n"
#if PTRACING
            "  -o or --output file     : file name for output of log messages\n"       
            "  -t or --trace           : degree of verbosity in error log (more times for more detail)\n"     
#endif
            "\n"
            "e.g. " << GetFile().GetTitle() << " send_fax.tif sip:fred@bloggs.com\n"
            "\n"
            "     " << GetFile().GetTitle() << " received_fax.tif\n\n";
    return;
  }

  m_manager = new MyManager();

  if (args.HasOption('N')) {
    PSTUNClient::NatTypes nat = m_manager->SetSTUNServer(args.GetOptionString('N'));
    cout << "STUN server \"" << m_manager->GetSTUNClient()->GetServer() << "\" replies " << nat;
    PIPSocket::Address externalAddress;
    if (nat != PSTUNClient::BlockedNat && m_manager->GetSTUNClient()->GetExternalAddress(externalAddress))
      cout << " with address " << externalAddress;
    cout << endl;
  }

  if (args.HasOption('u'))
    m_manager->SetDefaultUserName(args.GetOptionString('u'));

  PCaselessString interfaces;

#if OPAL_SIP
  // Set up SIP
  interfaces = args.GetOptionString('S');
  if (interfaces != "x") {
    SIPEndPoint * sip  = new SIPEndPoint(*m_manager);
    if (!sip->StartListeners(interfaces.Lines())) {
      cerr << "Could not start SIP listeners." << endl;
      return;
    }

    if (args.HasOption('r')) {
      SIPRegister::Params params;
      params.m_addressOfRecord = args.GetOptionString('r');
      params.m_password = args.GetOptionString('p');
      params.m_expire = 300;

      PString aor;
      sip->Register(params, aor);
    }

    if (args.HasOption('P'))
      sip->SetProxy(args.GetOptionString('P'), args.GetOptionString('u'), args.GetOptionString('p'));
  }
#endif // OPAL_SIP


#if OPAL_H323
  // Set up H.323
  interfaces = args.GetOptionString('H');
  if (interfaces != "x") {
    H323EndPoint * h323 = new H323EndPoint(*m_manager);
    if (!h323->StartListeners(interfaces.Lines())) {
      cerr << "Could not start H.323 listeners." << endl;
      return;
    }

    if (args.HasOption('g') || args.HasOption('G'))
      h323->UseGatekeeper(args.GetOptionString('g'), args.GetOptionString('G'));
  }
#endif // OPAL_H323

  // Create audio or T.38 fax endpoint.
  OpalFaxEndPoint * fax  = args.HasOption('a') ? new OpalFaxEndPoint(*m_manager)
                                               : new OpalT38EndPoint(*m_manager);
  if (args.HasOption('d'))
    fax->SetDefaultDirectory(args.GetOptionString('d'));
  if (args.HasOption('s'))
    fax->SetSpanDSP(args.GetOptionString('s'));
  if (args.HasOption('m'))
    fax->SetDefaultStringOption("Fax-Sync-Mode", args.GetOptionString('m'));

  // Route SIP/H.323 calls to the Fax endpoint
#if OPAL_SIP
  m_manager->AddRouteEntry("sip.*:.* = " + fax->GetPrefixName() + ":" + args[0] + ";receive");
#endif
#if OPAL_H323
  m_manager->AddRouteEntry("h323.*:.* = " + fax->GetPrefixName() + ":" + args[0] + ";receive");
#endif

  // If no explicit protocol on URI, then send to SIP.
  m_manager->AddRouteEntry(fax->GetPrefixName()+":.* = sip:<da>");

  if (args.GetCount() == 1)
    cout << "Awaiting incoming fax, saving as " << args[0] << " ..." << flush;
  else {
    PString token;
    if (!m_manager->SetUpCall(fax->GetPrefixName() + ":" + args[0], args[1], token)) {
      cerr << "Could not start call to \"" << args[1] << '"' << endl;
      return;
    }
    cout << "Sending " << args[0] << " to " << args[1] << " ..." << flush;
  }

  // Wait for call to come in and finish
  m_manager->m_completed.Wait();
  cout << " completed.";
}


void MyManager::OnClearedCall(OpalCall & /*call*/)
{
  m_completed.Signal();
}


// End of File ///////////////////////////////////////////////////////////////
