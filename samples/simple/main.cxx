/*
 * main.cxx
 *
 * A simple H.323 "net telephone" application.
 *
 * Copyright (c) 2000 Equivalence Pty. Ltd.
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
 * The Original Code is Portable Windows Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Log: main.cxx,v $
 * Revision 1.2065  2006/04/30 14:42:00  dereksmithies
 * Fix compile Sleep statement so it compiles on unix.
 *
 * Revision 2.63  2006/04/30 14:36:54  csoutheren
 * backports from PLuginBranch
 *
 * Revision 2.62  2006/04/30 14:34:42  csoutheren
 * Backport of IVR updates from PluginBranch
 *
 * Revision 2.60.2.4  2006/04/30 14:28:25  csoutheren
 * Added disableui and srcep options
 *
 * Revision 2.60.2.3  2006/04/30 13:50:29  csoutheren
 * Add ability to set TextToSpeech algorithm
 *
 * Revision 2.61  2006/03/07 11:24:15  csoutheren
 * Add --disable-grq flag
 *
 * Revision 2.60  2006/01/23 22:56:57  csoutheren
 * Added 2 second pause before dialling outgoing SIP calls from command line args when
 *  registrar used
 *
 * Revision 2.59  2005/12/11 19:14:21  dsandras
 * Added support for setting a different user name and authentication user name
 * as required by some providers like digisip.
 *
 * Revision 2.58  2005/11/07 02:27:48  dereksmithies
 * Get the answer call Y/N mechanism to work correctly.
 *
 * Revision 2.57  2005/09/06 04:58:42  dereksmithies
 * Add console input options. This is an initial release, and some "refinement"
 * help immensely.
 *
 * Revision 2.56  2005/08/20 09:03:10  rjongbloed
 * Some cosmetic fixes in output messages
 *
 * Revision 2.55  2005/08/13 09:16:18  rjongbloed
 * Added no-Xt-video to "usage" output
 *
 * Revision 2.54  2005/08/04 08:47:38  rjongbloed
 * Some cosmetic changes, and print out the codecs that are available
 *
 * Revision 2.53  2005/07/30 07:42:15  csoutheren
 * Added IAX2 functions
 *
 * Revision 2.52  2005/07/24 07:56:35  rjongbloed
 * Removed -l parameter, as it always listens.
 *
 * Revision 2.51  2005/07/11 01:57:31  csoutheren
 * Fixed error message
 *
 * Revision 2.50  2005/06/23 06:14:02  csoutheren
 * Fixed rtp-tos argument parsing. Thanks to Paul Nader
 *
 * Revision 2.49  2005/06/09 04:51:58  dereksmithies
 * Fix formatting.
 *
 * Revision 2.48  2005/06/09 04:45:57  dereksmithies
 * Correctly close the incoming connection  if the user rejects the incoming call.
 * Thanks to Robert Jongbloed for some helpful advice.
 *
 * Revision 2.47  2005/03/11 18:12:09  dsandras
 * Added support to specify the realm when registering. That way softphones already know what authentication information to use when required. The realm/domain can also be used in the From field.
 *
 * Revision 2.46  2005/02/19 22:46:19  dsandras
 * Temporarily removed support for SetDomain.
 *
 * Revision 2.45  2004/11/29 08:20:04  csoutheren
 * Added support for setting SIP authentication domain
 *
 * Revision 2.44  2004/08/18 07:14:00  csoutheren
 * Called ancestor OnClearedCall
 *
 * Revision 2.43  2004/08/14 07:56:30  rjongbloed
 * Major revision to utilise the PSafeCollection classes for the connections and calls.
 *
 * Revision 2.42  2004/04/26 07:06:07  rjongbloed
 * Removed some ancient pieces of code and used new API's for them.
 *
 * Revision 2.41  2004/04/26 06:33:18  rjongbloed
 * Added ability to specify more than one defualt listener for an endpoint,
 *   required by SIP which listens on both UDP and TCP.
 *
 * Revision 2.40  2004/04/25 09:32:48  rjongbloed
 * Fixed correct usage of HAS_IXJ
 *
 * Revision 2.39  2004/03/29 10:53:23  rjongbloed
 * Fixed missing mutex unlock which would invariably cause a deadlock.
 *
 * Revision 2.38  2004/03/22 10:20:34  rjongbloed
 * Changed to use UseGatekeeper() function so can select by gk-id as well as host.
 *
 * Revision 2.37  2004/03/14 11:32:20  rjongbloed
 * Changes to better support SIP proxies.
 *
 * Revision 2.36  2004/03/14 08:34:09  csoutheren
 * Added ability to set User-Agent string
 *
 * Revision 2.35  2004/03/14 06:15:36  rjongbloed
 * Must set ports before stun as stun code uses those port numbers.
 *
 * Revision 2.34  2004/03/13 06:32:17  rjongbloed
 * Fixes for removal of SIP and H.323 subsystems.
 * More registration work.
 *
 * Revision 2.33  2004/03/11 06:54:27  csoutheren
 * Added ability to disable SIP or H.323 stacks
 *
 * Revision 2.32  2004/03/09 12:09:56  rjongbloed
 * More work on SIP register.
 *
 * Revision 2.31  2004/02/24 11:37:01  rjongbloed
 * More work on NAT support, manual external address translation and STUN
 *
 * Revision 2.30  2004/02/21 02:41:10  rjongbloed
 * Tidied up the translate address to utilise more of the library infrastructure.
 * Changed cmd line args port-base/port-max to portbase/portmax, same as OhPhone.
 *
 * Revision 2.29  2004/02/17 11:00:06  csoutheren
 * Added --translate, --port-base and --port-max options
 *
 * Revision 2.28  2004/02/03 12:22:28  rjongbloed
 * Added call command
 *
 * Revision 2.27  2004/01/18 15:36:07  rjongbloed
 * Added stun support
 *
 * Revision 2.26  2003/04/09 01:38:27  robertj
 * Changed default to not send/receive video and added options to turn it on.
 *
 * Revision 2.25  2003/03/26 03:51:46  robertj
 * Fixed failure to compile if not statically linked.
 *
 * Revision 2.24  2003/03/24 07:18:29  robertj
 * Added registration system for LIDs so can work with various LID types by
 *   name instead of class instance.
 *
 * Revision 2.23  2003/03/19 02:30:45  robertj
 * Added removal of IVR stuff if EXPAT is not installed on system.
 *
 * Revision 2.22  2003/03/17 08:12:08  robertj
 * Added protocol name to media stream open output.
 *
 * Revision 2.21  2003/03/07 08:18:47  robertj
 * Fixed naming convention of PC sound system in routing table.
 *
 * Revision 2.20  2003/03/07 05:56:47  robertj
 * Changed so a # from whatever source routes to IVR.
 *
 * Revision 2.19  2003/03/06 03:57:47  robertj
 * IVR support (work in progress) requiring large changes everywhere.
 *
 * Revision 2.18  2002/11/11 06:52:01  robertj
 * Added correct flag for including static global variables.
 *
 * Revision 2.17  2002/11/10 11:33:17  robertj
 * Updated to OpenH323 v1.10.3
 *
 * Revision 2.16  2002/09/06 02:46:00  robertj
 * Added routing table system to route calls by regular expressions.
 * Added ability to set gatekeeper access token OID and password.
 *
 * Revision 2.15  2002/07/01 09:05:54  robertj
 * Changed TCp/UDP port allocation to use new thread safe functions.
 *
 * Revision 2.14  2002/04/12 14:02:41  robertj
 * Separated interface option for SIP and H.323.
 *
 * Revision 2.13  2002/03/27 05:34:55  robertj
 * Removed redundent busy forward field.
 * Added ability to set tcp and udp port bases.
 *
 * Revision 2.12  2002/03/27 04:36:46  robertj
 * Changed to add all possible xJack cards to pots endpoint.
 *
 * Revision 2.11  2002/03/27 04:16:20  robertj
 * Restructured default router for sample to allow more options.
 *
 * Revision 2.10  2002/02/13 08:17:31  robertj
 * Changed routing algorithm to route call to H323/SIP if contains an '@'
 *
 * Revision 2.9  2002/02/11 09:45:35  robertj
 * Moved version file to library root directoy
 *
 * Revision 2.8  2002/02/06 13:29:03  rogerh
 * H245 tunnelling is controlled by 'T' and not by 'h'
 *
 * Revision 2.7  2002/02/01 04:53:01  robertj
 * Added (very primitive!) SIP support.
 *
 * Revision 2.6  2002/01/22 05:34:58  robertj
 * Revamp of user input API triggered by RFC2833 support
 *
 * Revision 2.5  2001/08/21 11:18:55  robertj
 * Added conditional compile for xJack code.
 *
 * Revision 2.4  2001/08/17 08:35:41  robertj
 * Changed OnEstablished() to OnEstablishedCall() to be consistent.
 * Moved call end reasons enum from OpalConnection to global.
 * Used LID management in lid EP.
 * More implementation.
 *
 * Revision 2.3  2001/08/01 06:19:00  robertj
 * Added flags for disabling H.323 or Quicknet endpoints.
 *
 * Revision 2.2  2001/08/01 05:48:30  robertj
 * Major changes to H.323 capabilities, uses OpalMediaFormat for base name.
 * Fixed loading of transcoders from static library.
 *
 * Revision 2.1  2001/07/30 07:22:25  robertj
 * Abstracted listener management from H.323 to OpalEndPoint class.
 *
 * Revision 2.0  2001/07/27 15:48:24  robertj
 * Conversion of OpenH323 to Open Phone Abstraction Library (OPAL)
 *
 * Revision 1.12  2001/07/13 08:44:16  robertj
 * Fixed incorrect inclusion of hardware codec capabilities.
 *
 * Revision 1.11  2001/05/17 07:11:29  robertj
 * Added more call end types for common transport failure modes.
 *
 * Revision 1.10  2001/05/14 05:56:26  robertj
 * Added H323 capability registration system so can add capabilities by
 *   string name instead of having to instantiate explicit classes.
 *
 * Revision 1.9  2001/03/21 04:52:40  robertj
 * Added H.235 security to gatekeepers, thanks F�rbass Franz!
 *
 * Revision 1.8  2001/03/20 23:42:55  robertj
 * Used the new PTrace::Initialise function for starting trace code.
 *
 * Revision 1.7  2000/10/16 08:49:31  robertj
 * Added single function to add all UserInput capability types.
 *
 * Revision 1.6  2000/07/31 14:08:09  robertj
 * Added fast start and H.245 tunneling flags to the H323Connection constructor so can
 *    disabled these features in easier manner to overriding virtuals.
 *
 * Revision 1.5  2000/06/20 02:38:27  robertj
 * Changed H323TransportAddress to default to IP.
 *
 * Revision 1.4  2000/06/07 05:47:55  robertj
 * Added call forwarding.
 *
 * Revision 1.3  2000/05/23 11:32:27  robertj
 * Rewrite of capability table to combine 2 structures into one and move functionality into that class
 *    allowing some normalisation of usage across several applications.
 * Changed H323Connection so gets a copy of capabilities instead of using endponts, allows adjustments
 *    to be done depending on the remote client application.
 *
 * Revision 1.2  2000/05/11 10:00:02  robertj
 * Fixed setting and resetting of current call token variable.
 *
 * Revision 1.1  2000/05/11 04:05:57  robertj
 * Simple sample program.
 *
 */

#include <ptlib.h>

#include <opal/buildopts.h>

#if OPAL_IAX2
#include <iax2/iax2.h>
#endif

#if OPAL_SIP
#include <sip/sip.h>
#endif

#if OPAL_H323
#include <h323/h323.h>
#include <h323/gkclient.h>
#endif

#include <lids/lidep.h>
#include <lids/ixjlid.h>
#include <ptclib/pstun.h>

#include "main.h"
#include "../../version.h"


#ifdef OPAL_STATIC_LINK
#define H323_STATIC_LIB
#include <codec/allcodecs.h>
#include <lids/alllids.h>
#endif


#define new PNEW


PCREATE_PROCESS(SimpleOpalProcess);

///////////////////////////////////////////////////////////////

SimpleOpalProcess::SimpleOpalProcess()
  : PProcess("Open Phone Abstraction Library", "SimpleOPAL",
             MAJOR_VERSION, MINOR_VERSION, BUILD_TYPE, BUILD_NUMBER)
{
}


void SimpleOpalProcess::Main()
{
  cout << GetName()
       << " Version " << GetVersion(TRUE)
       << " by " << GetManufacturer()
       << " on " << GetOSClass() << ' ' << GetOSName()
       << " (" << GetOSVersion() << '-' << GetOSHardware() << ")\n\n";

  // Get and parse all of the command line arguments.
  PArgList & args = GetArguments();
  args.Parse(
             "a-auto-answer."
             "b-bandwidth:"
             "D-disable:"
             "d-dial-peer:"
             "-disableui:"
             "e-silence."
             "f-fast-disable."
             "g-gatekeeper:"
             "G-gk-id:"
             "-gk-token:"
             "-disable-grq."
             "h-help."
             "H-no-h323."
             "-h323-listen:"
             "I-no-sip."
             "j-jitter:"
             "l-listen."
             "n-no-gatekeeper."
             "-no-std-dial-peer."
#if PTRACING
             "o-output:"
#endif
             "P-prefer:"
             "p-password:"
             "-portbase:"
             "-portmax:"
             "q-quicknet:"
             "Q-no-quicknet."
             "R-require-gatekeeper."
             "r-register-sip:"
             "-rtp-base:"
             "-rtp-max:"
             "-rtp-tos:"
             "s-sound:"
             "S-no-sound."
             "-sound-in:"
             "-sound-out:"
             "-srcep:"
             "-sip-listen:"
             "-sip-proxy:"
             "-sip-domain:"
             "-sip-user-agent:"
             "-stun:"
             "T-h245tunneldisable."
             "-translate:"
             "-tts:"

#if PTRACING
             "t-trace."
#endif
             "-tcp-base:"
             "-tcp-max:"
             "u-user:"
             "-udp-base:"
             "-udp-max:"
             "-use-long-mime."
             "-rx-video." "-no-rx-video."
             "-tx-video." "-no-tx-video."
             "-grabber:"
             "-display:"
#if P_EXPAT
             "V-no-ivr."
             "x-vxml:"
#endif
#if OPAL_IAX2
	     "X-no-iax2."
#endif
          , FALSE);


  if (args.HasOption('h') || (!args.HasOption('l') && args.GetCount() == 0)) {
    cout << "Usage : " << GetName() << " [options] -l\n"
            "      : " << GetName() << " [options] [alias@]hostname   (no gatekeeper)\n"
            "      : " << GetName() << " [options] alias[@hostname]   (with gatekeeper)\n"
            "General options:\n"
            "  -l --listen             : Listen for incoming calls.\n"
            "  -d --dial-peer spec     : Set dial peer for routing calls (see below)\n"
            "     --no-std-dial-peer   : Do not include the standard dial peers\n"
            "  -a --auto-answer        : Automatically answer incoming calls.\n"
            "  -u --user name          : Set local alias name(s) (defaults to login name).\n"
            "  -p --password pwd       : Set password for user (gk or SIP authorisation).\n"
            "  -D --disable media      : Disable the specified codec (may be used multiple times)\n"
            "  -P --prefer media       : Prefer the specified codec (may be used multiple times)\n"
            "  --srcep ep              : Set the source endpoint to use for making calls\n"
            "  --disableui             : disable the user interface\n"
            "\n"
            "Audio options:\n"
            "  -j --jitter [min-]max   : Set minimum (optional) and maximum jitter buffer (in milliseconds).\n"
            "  -e --silence            : Disable transmitter silence detection.\n"
            "\n"
            "Video options:\n"
            "     --rx-video           : Start receiving video immediately.\n"
            "     --tx-video           : Start transmitting video immediately.\n"
            "     --no-rx-video        : Don't start receiving video immediately.\n"
            "     --no-tx-video        : Don't start transmitting video immediately.\n"
            "     --grabber dev        : Set the video grabber device.\n"
            "     --display dev        : Set the video display device.\n"
            "\n"

#if OPAL_SIP
            "SIP options:\n"
            "  -I --no-sip             : Disable SIP protocol.\n"
            "  -r --register-sip host  : Register with SIP server.\n"
            "     --sip-proxy url      : SIP proxy information, may be just a host name\n"
            "                          : or full URL eg sip:user:pwd@host\n"
            "     --sip-listen iface   : Interface/port(s) to listen for SIP requests\n"
            "                          : '*' is all interfaces, (default udp$:*:5060)\n"
            "     --sip-user-agent name: SIP UserAgent name to use.\n"
            "     --use-long-mime      : Use long MIME headers on outgoing SIP messages\n"
            "     --sip-domain str     : set authentication domain/realm\n"
            "\n"
#endif

#if OPAL_H323
            "H.323 options:\n"
            "  -H --no-h323            : Disable H.323 protocol.\n"
            "  -g --gatekeeper host    : Specify gatekeeper host.\n"
            "  -G --gk-id name         : Specify gatekeeper identifier.\n"
            "  -n --no-gatekeeper      : Disable gatekeeper discovery.\n"
            "  -R --require-gatekeeper : Exit if gatekeeper discovery fails.\n"
            "     --gk-token str       : Set gatekeeper security token OID.\n"
            "  -b --bandwidth bps      : Limit bandwidth usage to bps bits/second.\n"
            "  -f --fast-disable       : Disable fast start.\n"
            "  -T --h245tunneldisable  : Disable H245 tunnelling.\n"
            "     --h323-listen iface  : Interface/port(s) to listen for H.323 requests\n"
            "                          : '*' is all interfaces, (default tcp$:*:1720)\n"
            " --disable-grq            : Do not send GRQ when registering with GK\n"
#endif

            "\n"
#if HAS_IXJ
            "Quicknet options:\n"
            "  -Q --no-quicknet        : Do not use Quicknet xJACK device.\n"
            "  -q --quicknet device    : Select Quicknet xJACK device (default ALL).\n"
            "\n"
#endif
            "Sound card options:\n"
            "  -S --no-sound           : Do not use sound input/output device.\n"
            "  -s --sound device       : Select sound input/output device.\n"
            "     --sound-in device    : Select sound input device.\n"
            "     --sound-out device   : Select sound output device.\n"
            "\n"
#if P_EXPAT
            "IVR options:\n"
            "  -V --no-ivr             : Disable IVR.\n"
            "  -x --vxml file          : Set vxml file to use for IVR.\n"
            "  --tts engine            : Set the text to speech engine\n"
            "\n"
#endif
            "IP options:\n"
            "     --translate ip       : Set external IP address if masqueraded\n"
            "     --portbase n         : Set TCP/UDP/RTP port base\n"
            "     --portmax n          : Set TCP/UDP/RTP port max\n"
            "     --tcp-base n         : Set TCP port base (default 0)\n"
            "     --tcp-max n          : Set TCP port max (default base+99)\n"
            "     --udp-base n         : Set UDP port base (default 6000)\n"
            "     --udp-max n          : Set UDP port max (default base+199)\n"
            "     --rtp-base n         : Set RTP port base (default 5000)\n"
            "     --rtp-max n          : Set RTP port max (default base+199)\n"
            "     --rtp-tos n          : Set RTP packet IP TOS bits to n\n"
	          "     --stun server        : Set STUN server\n"
            "\n"
            "Debug options:\n"
#if PTRACING
            "  -t --trace              : Enable trace, use multiple times for more detail.\n"
            "  -o --output             : File for trace output, default is stderr.\n"
#endif
#if OPAL_IAX2
            "  -X --no-iax2            : Remove support for iax2\n"
#endif
            "  -h --help               : This help message.\n"
            "\n"
            "\n"
            "Dial peer specification:\n"
"  General form is pattern=destination where pattern is a regular expression\n"
"  matching the incoming calls destination address and will translate it to\n"
"  the outgoing destination address for making an outgoing call. For example,\n"
"  picking up a PhoneJACK handset and dialling 2, 6 would result in an address\n"
"  of \"pots:26\" which would then be matched against, say, a spec of\n"
"  pots:26=h323:10.0.1.1, resulting in a call from the pots handset to\n"
"  10.0.1.1 using the H.323 protocol.\n"
"\n"
"  As the pattern field is a regular expression, you could have used in the\n"
"  above .*:26=h323:10.0.1.1 to achieve the same result with the addition that\n"
"  an incoming call from a SIP client would also be routed to the H.323 client.\n"
"\n"
"  Note that the pattern has an implicit ^ and $ at the beginning and end of\n"
"  the regular expression. So it must match the entire address.\n"
"\n"
"  If the specification is of the form @filename, then the file is read with\n"
"  each line consisting of a pattern=destination dial peer specification. Lines\n"
"  without and equal sign or beginning with '#' are ignored.\n"
"\n"
"  The standard dial peers that will be included are:\n"
"    If SIP is enabled but H.323 & IAX2 are disabled:\n"
"      pots:.*\\*.*\\*.* = sip:<dn2ip>\n"
"      pots:.*         = sip:<da>\n"
"      pc:.*           = sip:<da>\n"
"\n"
"    If SIP & IAX2 are not enabled and H.323 is enabled:\n"
"      pots:.*\\*.*\\*.* = h323:<dn2ip>\n"
"      pots:.*         = h323:<da>\n"
"      pc:.*           = h323:<da>\n"
"\n"
"    If POTS is enabled:\n"
"      h323:.* = pots:<da>\n"
"      sip:.*  = pots:<da>\n"
"      iax2:.* = pots:<da>\n"
"\n"
"    If POTS is not enabled and the PC sound system is enabled:\n"
"      iax2:.* = pc:<da>\n"
"      h323:.* = pc:<da>\n"
"      sip:. * = pc:<da>\n"
"\n"
#if P_EXPAT
"    If IVR is enabled then a # from any protocol will route it it, ie:\n"
"      .*:#  = ivr:\n"
"\n"
#endif
#if OPAL_IAX2
"    If IAX2 is enabled then you can make a iax2 call with a command like:\n"
"       simpleopal -IHn  iax2:guest@misery.digium.com/s\n"
#endif
            << endl;
    return;
  }

#if PTRACING
  PTrace::Initialise(args.GetOptionCount('t'),
                     args.HasOption('o') ? (const char *)args.GetOptionString('o') : NULL,
                     PTrace::Timestamp|PTrace::Thread|PTrace::FileAndLine);
#endif

  // Create the Opal Manager and initialise it
  opal = new MyManager;

  if (opal->Initialise(args))
    opal->Main(args);

  cout << "Exiting " << GetName() << endl;

  delete opal;
}


///////////////////////////////////////////////////////////////

MyManager::MyManager()
{
  potsEP = NULL;
  pcssEP = NULL;

#if OPAL_H323
  h323EP = NULL;
#endif
#if OPAL_SIP
  sipEP  = NULL;
#endif
#if OPAL_IAX2
  iax2EP = NULL;
#endif
#if P_EXPAT
  ivrEP  = NULL;
#endif

  pauseBeforeDialing = FALSE;
}


MyManager::~MyManager()
{
  // Must do this before we destroy the manager or a crash will result
  if (potsEP != NULL)
    potsEP->RemoveAllLines();
}


BOOL MyManager::Initialise(PArgList & args)
{
  // Set the various global options
  if (args.HasOption("rx-video"))
    autoStartReceiveVideo = TRUE;
  if (args.HasOption("no-rx-video"))
    autoStartReceiveVideo = FALSE;
  if (args.HasOption("tx-video"))
    autoStartTransmitVideo = TRUE;
  if (args.HasOption("no-tx-video"))
    autoStartTransmitVideo = FALSE;

  if (args.HasOption("grabber")) {
    PVideoDevice::OpenArgs video = GetVideoInputDevice();
    video.deviceName = args.GetOptionString("grabber");
    SetVideoInputDevice(video);
  }

  if (args.HasOption("display")) {
    PVideoDevice::OpenArgs video = GetVideoOutputDevice();
    video.deviceName = args.GetOptionString("display");
    SetVideoOutputDevice(video);
  }

  if (args.HasOption('j')) {
    unsigned minJitter;
    unsigned maxJitter;
    PStringArray delays = args.GetOptionString('j').Tokenise(",-");
    if (delays.GetSize() < 2) {
      maxJitter = delays[0].AsUnsigned();
      minJitter = PMIN(GetMinAudioJitterDelay(), maxJitter);
    }
    else {
      minJitter = delays[0].AsUnsigned();
      maxJitter = delays[1].AsUnsigned();
    }
    if (minJitter >= 20 && minJitter <= maxJitter && maxJitter <= 1000)
      SetAudioJitterDelay(minJitter, maxJitter);
    else {
      cerr << "Jitter should be between 20 and 1000 milliseconds." << endl;
      return FALSE;
    }
  }

  silenceDetectParams.m_mode = args.HasOption('e') ? OpalSilenceDetector::NoSilenceDetection
                                                   : OpalSilenceDetector::AdaptiveSilenceDetection;

  if (args.HasOption('D'))
    SetMediaFormatMask(args.GetOptionString('D').Lines());
  if (args.HasOption('P'))
    SetMediaFormatOrder(args.GetOptionString('P').Lines());

  cout << "Jitter buffer: "  << GetMinAudioJitterDelay() << '-' << GetMaxAudioJitterDelay() << " ms\n"
          "Codecs removed: " << setfill(',') << GetMediaFormatMask() << "\n"
          "Codec order: " << setfill(',') << GetMediaFormatOrder() << setfill(' ') << '\n';

  if (args.HasOption("translate")) {
    SetTranslationAddress(args.GetOptionString("translate"));
    cout << "External address set to " << GetTranslationAddress() << '\n';
  }

  if (args.HasOption("portbase")) {
    unsigned portbase = args.GetOptionString("portbase").AsUnsigned();
    unsigned portmax  = args.GetOptionString("portmax").AsUnsigned();
    SetTCPPorts  (portbase, portmax);
    SetUDPPorts  (portbase, portmax);
    SetRtpIpPorts(portbase, portmax);
  } else {
    if (args.HasOption("tcp-base"))
      SetTCPPorts(args.GetOptionString("tcp-base").AsUnsigned(),
                  args.GetOptionString("tcp-max").AsUnsigned());

    if (args.HasOption("udp-base"))
      SetUDPPorts(args.GetOptionString("udp-base").AsUnsigned(),
                  args.GetOptionString("udp-max").AsUnsigned());

    if (args.HasOption("rtp-base"))
      SetRtpIpPorts(args.GetOptionString("rtp-base").AsUnsigned(),
                    args.GetOptionString("rtp-max").AsUnsigned());
  }

  if (args.HasOption("rtp-tos")) {
    unsigned tos = args.GetOptionString("rtp-tos").AsUnsigned();
    if (tos > 255) {
      cerr << "IP Type Of Service bits must be 0 to 255." << endl;
      return FALSE;
    }
    SetRtpIpTypeofService(tos);
  }

  cout << "TCP ports: " << GetTCPPortBase() << '-' << GetTCPPortMax() << "\n"
          "UDP ports: " << GetUDPPortBase() << '-' << GetUDPPortMax() << "\n"
          "RTP ports: " << GetRtpIpPortBase() << '-' << GetRtpIpPortMax() << "\n"
          "RTP IP TOS: 0x" << hex << (unsigned)GetRtpIpTypeofService() << dec << "\n"
          "STUN server: " << flush;

  if (args.HasOption("stun"))
    SetSTUNServer(args.GetOptionString("stun"));

  if (stun != NULL)
    cout << stun->GetServer() << " replies " << stun->GetNatTypeName();
  else
    cout << "None";
  cout << '\n';


  ///////////////////////////////////////
  // Open the LID if parameter provided, create LID based endpoint

#if HAS_IXJ
  if (!args.HasOption('Q')) {
    PStringArray devices = args.GetOptionString('q').Lines();
    if (devices.IsEmpty() || devices[0] == "ALL")
      devices = OpalIxJDevice::GetDeviceNames();
    for (PINDEX d = 0; d < devices.GetSize(); d++) {
      OpalIxJDevice * ixj = new OpalIxJDevice;
      if (ixj->Open(devices[d])) {
        // Create LID protocol handler, automatically adds to manager
        if (potsEP == NULL)
          potsEP = new OpalPOTSEndPoint(*this);
        if (potsEP->AddDevice(ixj))
          cout << "Quicknet device \"" << devices[d] << "\" added." << endl;
      }
      else {
        cerr << "Could not open device \"" << devices[d] << '"' << endl;
        delete ixj;
      }
    }
  }
#endif


  ///////////////////////////////////////
  // Create PC Sound System handler

  if (!args.HasOption('S')) {
    pcssEP = new MyPCSSEndPoint(*this);

    pcssEP->autoAnswer = args.HasOption('a');
    cout << "Auto answer is " << (pcssEP->autoAnswer ? "on" : "off") << "\n";
          
    if (!pcssEP->SetSoundDevice(args, "sound", PSoundChannel::Recorder))
      return FALSE;
    if (!pcssEP->SetSoundDevice(args, "sound", PSoundChannel::Player))
      return FALSE;
    if (!pcssEP->SetSoundDevice(args, "sound-in", PSoundChannel::Recorder))
      return FALSE;
    if (!pcssEP->SetSoundDevice(args, "sound-out", PSoundChannel::Player))
      return FALSE;

    cout << "Sound output device: \"" << pcssEP->GetSoundChannelPlayDevice() << "\"\n"
            "Sound  input device: \"" << pcssEP->GetSoundChannelRecordDevice() << "\"\n"
            "Video output device: \"" << GetVideoOutputDevice().deviceName << "\"\n"
            "Video  input device: \"" << GetVideoInputDevice().deviceName << "\"\n"
            "Available codecs: " << setfill(',') << OpalTranscoder::GetPossibleFormats(pcssEP->GetMediaFormats()) << setfill(' ') << endl;
  }

#if OPAL_H323

  ///////////////////////////////////////
  // Create H.323 protocol handler

  if (!args.HasOption("no-h323")) {
    h323EP = new H323EndPoint(*this);

    h323EP->DisableFastStart(args.HasOption('f'));
    h323EP->DisableH245Tunneling(args.HasOption('T'));
    h323EP->SetSendGRQ(!args.HasOption("disable-grq"));

    // Get local username, multiple uses of -u indicates additional aliases
    if (args.HasOption('u')) {
      PStringArray aliases = args.GetOptionString('u').Lines();
      h323EP->SetLocalUserName(aliases[0]);
      for (PINDEX i = 1; i < aliases.GetSize(); i++)
        h323EP->AddAliasName(aliases[i]);
    }

    if (args.HasOption('b')) {
      unsigned initialBandwidth = args.GetOptionString('b').AsUnsigned()*100;
      if (initialBandwidth == 0) {
        cerr << "Illegal bandwidth specified." << endl;
        return FALSE;
      }
      h323EP->SetInitialBandwidth(initialBandwidth);
    }

    h323EP->SetGkAccessTokenOID(args.GetOptionString("gk-token"));

    cout << "H.323 Local username: " << h323EP->GetLocalUserName() << "\n"
         << "H.323 FastConnect is " << (h323EP->IsFastStartDisabled() ? "off" : "on") << "\n"
         << "H.323 H245Tunnelling is " << (h323EP->IsH245TunnelingDisabled() ? "off" : "on") << "\n"
         << "H.323 gk Token OID is " << h323EP->GetGkAccessTokenOID() << endl;


    // Start the listener thread for incoming calls.
    PStringArray listeners = args.GetOptionString("h323-listen").Lines();
    if (!h323EP->StartListeners(listeners)) {
      cerr <<  "Could not open any H.323 listener from "
           << setfill(',') << listeners << endl;
      return FALSE;
    }
    cout << "H.323 listeners: " << setfill(',') << h323EP->GetListeners() << setfill(' ') << endl;


    if (args.HasOption('p'))
      h323EP->SetGatekeeperPassword(args.GetOptionString('p'));

    // Establish link with gatekeeper if required.
    if (!args.HasOption('n')) {
      PString gkHost      = args.GetOptionString('g');
      PString gkIdentifer = args.GetOptionString('G');
      PString gkInterface = args.GetOptionString("h323-listen");
      cout << "Gatekeeper: " << flush;
      if (h323EP->UseGatekeeper(gkHost, gkIdentifer, gkInterface))
        cout << *h323EP->GetGatekeeper() << endl;
      else {
        cout << "none." << endl;
        cerr << "Could not register with gatekeeper";
        if (!gkIdentifer)
          cerr << " id \"" << gkIdentifer << '"';
        if (!gkHost)
          cerr << " at \"" << gkHost << '"';
        if (!gkInterface)
          cerr << " on interface \"" << gkInterface << '"';
        if (h323EP->GetGatekeeper() != NULL) {
          switch (h323EP->GetGatekeeper()->GetRegistrationFailReason()) {
            case H323Gatekeeper::InvalidListener :
              cerr << " - Invalid listener";
              break;
            case H323Gatekeeper::DuplicateAlias :
              cerr << " - Duplicate alias";
              break;
            case H323Gatekeeper::SecurityDenied :
              cerr << " - Security denied";
              break;
            case H323Gatekeeper::TransportError :
              cerr << " - Transport error";
              break;
            default :
              cerr << " - Error code " << h323EP->GetGatekeeper()->GetRegistrationFailReason();
          }
        }
        cerr << '.' << endl;
        if (args.HasOption("require-gatekeeper")) 
          return FALSE;
      }
    }
  }

#endif

#if OPAL_IAX2
  ///////////////////////////////////////
  // Create IAX2 protocol handler

  if (!args.HasOption("no-iax2")) {
    iax2EP = new IAX2EndPoint(*this);
    
    if (args.HasOption('p'))
      iax2EP->SetPassword(args.GetOptionString('p'));
    
    if (args.HasOption('u')) {
      PStringArray aliases = args.GetOptionString('u').Lines();
      iax2EP->SetLocalUserName(aliases[0]);
    }
  }
#endif

#if OPAL_SIP

  ///////////////////////////////////////
  // Create SIP protocol handler

  if (!args.HasOption("no-sip")) {
    sipEP = new SIPEndPoint(*this);

    if (args.HasOption("sip-user-agent"))
      sipEP->SetUserAgent(args.GetOptionString("sip-user-agent"));

    if (args.HasOption("sip-proxy"))
      sipEP->SetProxy(args.GetOptionString("sip-proxy"));

    // set MIME format
    sipEP->SetMIMEForm(args.HasOption("use-long-mime"));

    // Get local username, multiple uses of -u indicates additional aliases
    if (args.HasOption('u')) {
      PStringArray aliases = args.GetOptionString('u').Lines();
      sipEP->SetDefaultLocalPartyName(aliases[0]);
    }

    sipEP->SetRetryTimeouts(10000, 30000);

    // Start the listener thread for incoming calls.
    PStringArray listeners = args.GetOptionString("sip-listen").Lines();
    if (!sipEP->StartListeners(listeners)) {
      cerr <<  "Could not open any SIP listener from "
            << setfill(',') << listeners << endl;
      return FALSE;
    }

    if (args.HasOption('r')) {
      PString registrar = args.GetOptionString('r');
      cout << "Using SIP registrar " << registrar << " ... " << flush;
      if (sipEP->Register(registrar, args.GetOptionString('u'), args.GetOptionString('u'), args.GetOptionString('p'), args.GetOptionString("sip-domain")))
        cout << "done.";
      else
        cout << "failed!";
      cout << endl;
      pauseBeforeDialing = TRUE;
    }
  }

#endif


#if P_EXPAT
  ///////////////////////////////////////
  // Create IVR protocol handler

  if (!args.HasOption('V')) {
    ivrEP = new OpalIVREndPoint(*this);
    if (args.HasOption('x'))
      ivrEP->SetDefaultVXML(args.GetOptionString('x'));

    PString ttsEngine = args.GetOptionString("tts");
    if (ttsEngine.IsEmpty()) 
      ttsEngine = PFactory<PTextToSpeech>::GetKeyList()[0];
    if (!ttsEngine.IsEmpty()) 
      ivrEP->SetDefaultTextToSpeech(ttsEngine);
  }
#endif


  ///////////////////////////////////////
  // Set the dial peers

  if (args.HasOption('d')) {
    if (!SetRouteTable(args.GetOptionString('d').Lines())) {
      cerr <<  "No legal entries in dial peer!" << endl;
      return FALSE;
    }
  }

  if (!args.HasOption("no-std-dial-peer")) {
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

    if (potsEP != NULL) {
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
                                                                                                                                            
#if OPAL_IAX2
  if (pcssEP != NULL) {
    AddRouteEntry("iax2:.*  = pc:<da>");
    AddRouteEntry("pc:.*   = iax2:<da>");
  }
#endif

  srcEP = args.GetOptionString("srcEp", "pc:*");

  return TRUE;
}


OpalConnection::AnswerCallResponse
       MyManager::OnAnswerCall(OpalConnection & connection,
                                  const PString & caller)
{
  cout << "incoming call from " << caller << endl;
  cout << "Answer call (Y/N) " << endl;
  currentCallToken = connection.GetCall().GetToken();
  return OpalConnection::AnswerCallPending;
}

void MyManager::AnswerCall(OpalConnection::AnswerCallResponse response)
{
  PSafePtr<OpalCall> call = FindCallWithLock(currentCallToken);
  if (call == NULL) {
    cout << "Could not find call for " << currentCallToken << " to answer" << endl;
    return;
  }

  if (response != OpalConnection::AnswerCallNow) {
    cout << "Clearing call " << *call << endl;
    call->Clear();
    return;
  }

  if (pcssEP != NULL && !pcssEP->incomingConnectionToken) 
    pcssEP->AcceptIncomingConnection(pcssEP->incomingConnectionToken);
}


void MyManager::NewSpeedDial(const PString & ostr)
{
  PString str = ostr;
  PINDEX idx = str.Find(' ');
  if (str.IsEmpty() || (idx == P_MAX_INDEX)) {
    cout << "Must specify speedial number and string" << endl;
    return;
  }
 
  PString key  = str.Left(idx).Trim();
  PString data = str.Mid(idx).Trim();
 
  PConfig config("Speeddial");
  config.SetString(key, data);
 
  cout << "Speedial " << key << " set to " << data << endl;
}
 

void MyManager::Main(PArgList & args)
{
  // See if making a call or just listening.
  switch (args.GetCount()) {
    case 0 :
      cout << "Waiting for incoming calls\n";
      break;

    case 1 :
      if (pauseBeforeDialing) {
        cout << "Pausing to allow registration to occur..." << flush;
        PThread::Sleep(2000);
        cout << "done" << endl;
      }

      cout << "Initiating call to \"" << args[0] << "\"\n";
      if (!srcEP.IsEmpty())
        SetUpCall(srcEP, args[0], currentCallToken);
      else if (potsEP != NULL)
        SetUpCall("pots:*", args[0], currentCallToken);
      else
        SetUpCall("pc:*", args[0], currentCallToken);
      break;

    default :
      cout << "Initiating call from \"" << args[0] << "\"to \"" << args[1] << "\"\n";
      SetUpCall(args[0], args[1], currentCallToken);
      break;
  }

  if (args.HasOption("disableui")) {
    while (FindCallWithLock(currentCallToken) != NULL)
      PThread::Sleep(1000);
  }
  else {
    cout << "Press ? for help." << endl;

    PStringStream help;

    help << "Select:\n"
            "  0-9 : send user indication message\n"
            "  *,# : send user indication message\n"
            "  M   : send text message to remote user\n"
            "  C   : connect to remote host\n"
            "  S   : Display statistics\n"
            "  H   : Hang up phone\n"
            "  L   : List speed dials\n"
            "  D   : Create new speed dial\n"
            "  {}  : Increase/reduce record volume\n"
            "  []  : Increase/reduce playback volume\n"
      "  V   : Display current volumes\n";
     
    PConsoleChannel console(PConsoleChannel::StandardInput);
    for (;;) {
      // display the prompt
      cout << "Command ? " << flush;
       
       
      // terminate the menu loop if console finished
      char ch = (char)console.peek();
      if (console.eof()) {
        cout << "\nConsole gone - menu disabled" << endl;
        goto endSimpleOPAL;
      }
       
      console >> ch;
      PTRACE(3, "console in audio test is " << ch);
      switch (tolower(ch)) {
      case 'x' :
      case 'q' :
        goto endSimpleOPAL;
        break;
      case '?' :       
        cout << help ;
        break;
        
      case 'y' :
        AnswerCall(OpalConnection::AnswerCallNow);
        console.ignore(INT_MAX, '\n');
        break;
        
      case 'n' :
        AnswerCall(OpalConnection::AnswerCallDenied);
        console.ignore(INT_MAX, '\n');
        break;

      case 'l' :
        ListSpeedDials();
        break;
        
      case 'd' :
        {
	        PString str;
	        console >> str;
	        NewSpeedDial(str.Trim());
        }
        break;
        
      case 'h' :
        HangupCurrentCall();
        break;

      case 'c' :
        if (!currentCallToken.IsEmpty())
	        cout << "Cannot make call whilst call in progress\n";
        else {
      	  PString str;
	        console >> str;
	        StartCall(str.Trim());
        }
        break;

      case 'r':
        cout << " current call token is \"" << currentCallToken << "\" " << endl;
        break;

      case 'm' :
        if (currentCallToken.IsEmpty())
	        cout << "Cannot send a message while no call in progress\n";
        else {
	        PString str;
	        console >> str;
	        SendMessageToRemoteNode(str);
        }
        break;

      default:;
      }
       
    }
  endSimpleOPAL:
    if (!currentCallToken.IsEmpty())
      HangupCurrentCall();
  }

   cout << "Console finished " << endl;
}

void MyManager::HangupCurrentCall()
{
  PSafePtr<OpalCall> call = FindCallWithLock(currentCallToken);
  if (call != NULL) {
    cout << "Clearing call " << *call << endl;
    call->Clear();
    currentCallToken = PString();
  }
  else
    cout << "Not in a call!\n";      
}

void MyManager::SendMessageToRemoteNode(const PString & ostr)
{
  PString str = ostr.Trim();
  if (str.IsEmpty()) {
    cout << "Must supply a message to send!\n";
    return ;
  }

  for (PINDEX i = 0; i < endpoints.GetSize(); i++) {
    PStringList res = endpoints[i].GetAllConnections();
    if (res.GetSize() == 0)
      continue;

    for(PINDEX j  = 0; j < res.GetSize(); j++) {
      PSafePtr< OpalConnection >  conn = endpoints[i].GetConnectionWithLock (res[j]);
      if (conn != NULL) {
	conn->SendUserInputString(str);
	cout << "Send \"" << str << "\" to " << res[j] << endl;
      }
    }
  }
  return;
}

void MyManager::StartCall(const PString & ostr)
{
  PString str = ostr.Trim();
  if (str.IsEmpty()) {
    cout << "Must supply hostname to connect to!\n";
    return ;
  }

  // check for speed dials, and match wild cards as we go
  PString key, prefix;
  if ((str.GetLength() > 1) && (str[str.GetLength()-1] == '#')) {
 
    key = str.Left(str.GetLength()-1).Trim(); 
    str = PString();
    PConfig config("Speeddial");
    PINDEX p;
    for (p = key.GetLength(); p > 0; p--) {
 
      PString newKey = key.Left(p);
      prefix = newKey;
      PINDEX q;
 
      // look for wild cards
      str = config.GetString(newKey + '*').Trim();
      if (!str.IsEmpty())
        break;
 
      // look for digit matches
      for (q = p; q < key.GetLength(); q++)
        newKey += '?';
      str = config.GetString(newKey).Trim();
      if (!str.IsEmpty())
        break;
    }
    if (str.IsEmpty()) {
      cout << "Speed dial \"" << key << "\" not defined";
      cout << endl;
    }
  }

  if (!str.IsEmpty())
    SetUpCall(srcEP, str, currentCallToken);

  return;
}

void MyManager::ListSpeedDials()
{
  PConfig config("Speeddial");
  PStringList keys = config.GetKeys();
  if (keys.GetSize() == 0) {
    cout << "No speed dials defined" << endl;
    return;
  }
 
  PINDEX i;
  for (i = 0; i < keys.GetSize(); i++)
    cout << keys[i] << ":   " << config.GetString(keys[i]) << endl;
}

void MyManager::OnEstablishedCall(OpalCall & call)
{
  currentCallToken = call.GetToken();
  cout << "In call with " << call.GetPartyB() << " using " << call.GetPartyA() << endl;  
}

void MyManager::OnClearedCall(OpalCall & call)
{
  if (currentCallToken == call.GetToken())
    currentCallToken = PString();

  PString remoteName = '"' + call.GetPartyB() + '"';
  switch (call.GetCallEndReason()) {
    case OpalConnection::EndedByRemoteUser :
      cout << remoteName << " has cleared the call";
      break;
    case OpalConnection::EndedByCallerAbort :
      cout << remoteName << " has stopped calling";
      break;
    case OpalConnection::EndedByRefusal :
      cout << remoteName << " did not accept your call";
      break;
    case OpalConnection::EndedByNoAnswer :
      cout << remoteName << " did not answer your call";
      break;
    case OpalConnection::EndedByTransportFail :
      cout << "Call with " << remoteName << " ended abnormally";
      break;
    case OpalConnection::EndedByCapabilityExchange :
      cout << "Could not find common codec with " << remoteName;
      break;
    case OpalConnection::EndedByNoAccept :
      cout << "Did not accept incoming call from " << remoteName;
      break;
    case OpalConnection::EndedByAnswerDenied :
      cout << "Refused incoming call from " << remoteName;
      break;
    case OpalConnection::EndedByNoUser :
      cout << "Gatekeeper or registrar could not find user " << remoteName;
      break;
    case OpalConnection::EndedByNoBandwidth :
      cout << "Call to " << remoteName << " aborted, insufficient bandwidth.";
      break;
    case OpalConnection::EndedByUnreachable :
      cout << remoteName << " could not be reached.";
      break;
    case OpalConnection::EndedByNoEndPoint :
      cout << "No phone running for " << remoteName;
      break;
    case OpalConnection::EndedByHostOffline :
      cout << remoteName << " is not online.";
      break;
    case OpalConnection::EndedByConnectFail :
      cout << "Transport error calling " << remoteName;
      break;
    default :
      cout << "Call with " << remoteName << " completed";
  }
  PTime now;
  cout << ", on " << now.AsString("w h:mma") << ". Duration "
       << setprecision(0) << setw(5) << (now - call.GetStartTime())
       << "s." << endl;

  OpalManager::OnClearedCall(call);
}


BOOL MyManager::OnOpenMediaStream(OpalConnection & connection,
                                  OpalMediaStream & stream)
{
  if (!OpalManager::OnOpenMediaStream(connection, stream))
    return FALSE;

  cout << "Started ";

  PCaselessString prefix = connection.GetEndPoint().GetPrefixName();
  if (prefix == "pc" || prefix == "pots")
    cout << (stream.IsSink() ? "playing " : "grabbing ") << stream.GetMediaFormat();
  else if (prefix == "ivr")
    cout << (stream.IsSink() ? "streaming" : "recording") << stream.GetMediaFormat();
  else
    cout << (stream.IsSink() ? "sending " : "receiving ") << stream.GetMediaFormat()
          << (stream.IsSink() ? " to " : " from ")<< prefix;

  cout << endl;

  return TRUE;
}



void MyManager::OnUserInputString(OpalConnection & connection, const PString & value)
{
  cout << "User input received: \"" << value << '"' << endl;
  OpalManager::OnUserInputString(connection, value);
}


///////////////////////////////////////////////////////////////

MyPCSSEndPoint::MyPCSSEndPoint(MyManager & mgr)
  : OpalPCSSEndPoint(mgr)
{
}


PString MyPCSSEndPoint::OnGetDestination(const OpalPCSSConnection & /*connection*/)
{
  PString destination;

  if (destinationAddress.IsEmpty()) {
    cout << "Enter destination address? " << flush;
    cin >> destination;
  }
  else {
    destination = destinationAddress;
    destinationAddress = PString();
  }

  return destination;
}


void MyPCSSEndPoint::OnShowIncoming(const OpalPCSSConnection & connection)
{
  incomingConnectionToken = connection.GetToken();

  if (autoAnswer)
    AcceptIncomingConnection(incomingConnectionToken);
  else {
    PTime now;
    cout << "\nCall on " << now.AsString("w h:mma")
         << " from " << connection.GetRemotePartyName()
         << ", answer (Y/N)? " << flush;
  }
}


BOOL MyPCSSEndPoint::OnShowOutgoing(const OpalPCSSConnection & connection)
{
  PTime now;
  cout << connection.GetRemotePartyName() << " is ringing on "
       << now.AsString("w h:mma") << " ..." << endl;
  return TRUE;
}


BOOL MyPCSSEndPoint::SetSoundDevice(PArgList & args,
                                    const char * optionName,
                                    PSoundChannel::Directions dir)
{
  if (!args.HasOption(optionName))
    return TRUE;

  PString dev = args.GetOptionString(optionName);

  if (dir == PSoundChannel::Player) {
    if (SetSoundChannelPlayDevice(dev))
      return TRUE;
  }
  else {
    if (SetSoundChannelRecordDevice(dev))
      return TRUE;
  }

  cerr << "Device for " << optionName << " (\"" << dev << "\") must be one of:\n";

  PStringArray names = PSoundChannel::GetDeviceNames(dir);
  for (PINDEX i = 0; i < names.GetSize(); i++)
    cerr << "  \"" << names[i] << "\"\n";

  return FALSE;
}


// End of File ///////////////////////////////////////////////////////////////
