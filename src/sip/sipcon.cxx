/*
 * sipcon.cxx
 *
 * Session Initiation Protocol connection.
 *
 * Open Phone Abstraction Library (OPAL)
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
 * The Original Code is Open Phone Abstraction Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Log: sipcon.cxx,v $
 * Revision 1.2195  2007/01/18 04:45:17  csoutheren
 * Messy, but simple change to add additional options argument to OpalConnection constructor
 * This allows the provision of non-trivial arguments for connections
 *
 * Revision 2.193  2007/01/15 22:13:59  dsandras
 * Make sure we do not ignore SIP reINVITEs.
 *
 * Revision 2.192  2007/01/10 09:16:55  csoutheren
 * Allow compilation with video disabled
 *
 * Revision 2.191  2007/01/02 17:28:40  dsandras
 * Do not create a new RTPSessionManager when authenticating an INVITE, because
 * it will use new UDP ports which is not needed and causes bad behaviors with
 * some broken SIP Proxies. (Ekiga report #359971)
 *
 * Revision 2.190  2006/12/18 03:18:42  csoutheren
 * Messy but simple fixes
 *   - Add access to SIP REGISTER timeout
 *   - Ensure OpalConnection options are correctly progagated
 *
 * Revision 2.189  2006/12/08 04:24:19  csoutheren
 * Applied 1604554 - SIP remote hold improvements
 * Thanks to Simon Zwahlen
 *
 * Revision 2.188  2006/11/28 20:24:24  dsandras
 * Fixed crash when calling himself due to transport deletion.
 *
 * Revision 2.187  2006/11/11 12:23:18  hfriederich
 * Code reorganisation to improve RFC2833 handling for both SIP and H.323. Thanks Simon Zwahlen for the idea
 *
 * Revision 2.186  2006/11/10 23:19:51  hfriederich
 * Don't send RFC2833 if already sent INFO
 *
 * Revision 2.185  2006/11/09 18:24:55  hfriederich
 * Ensures that responses to received INVITE get sent out on the same network interface as where the INVITE was received. Remove cout statement
 *
 * Revision 2.184  2006/11/09 18:02:07  hfriederich
 * Prevent sending OK if already releasing, thus having sent a final response. Modify call end reason if media format list empty
 *
 * Revision 2.183  2006/10/28 16:40:28  dsandras
 * Fixed SIP reinvite without breaking H.323 calls.
 *
 * Revision 2.182  2006/10/11 01:22:51  csoutheren
 * Applied 1565585 - SIP changed codec order (compatibility to Mediatrix)
 * Thanks to Simon Zwahlen
 *
 * Revision 2.181  2006/10/11 00:40:07  csoutheren
 * Fix problem introduced in v1.162 that prevent external RTP from working properly
 *
 * Revision 2.180  2006/10/06 05:33:12  hfriederich
 * Fix RFC2833 for SIP connections
 *
 * Revision 2.179  2006/10/01 17:16:32  hfriederich
 * Ensures that an ACK is sent out for every final response to INVITE
 *
 * Revision 2.178  2006/08/28 00:53:52  csoutheren
 * Applied 1546613 - Transport check in SIPConnection::SetConnected
 * Thanks to Drazen Dimoti
 *
 * Revision 2.177  2006/08/28 00:35:57  csoutheren
 * Applied 1545205 - Usage of SetPhase in SIPConnection
 * Thanks to Drazen Dimoti
 *
 * Revision 2.176  2006/08/28 00:32:16  csoutheren
 * Applied 1545191 - SIPConnection invitationsMutex fix
 * Thanks to Drazen Dimoti
 *
 * Revision 2.175  2006/08/28 00:06:10  csoutheren
 * Applied 1545107 - MediaStream - safe access to patch for adding a filter
 * Thanks to Drazen Dimoti
 *
 * Revision 2.174  2006/08/20 13:35:56  csoutheren
 * Backported changes to protect against rare error conditions
 *
 * Revision 2.173  2006/08/12 04:09:24  csoutheren
 * Applied 1538497 - Add the PING method
 * Thanks to Paul Rolland
 *
 * Revision 2.172  2006/08/12 03:23:33  csoutheren
 * Fixed sending SIP INFO application/dtmf
 *
 * Revision 2.171  2006/08/10 04:31:59  csoutheren
 * Applied 1534444 - Lock to streamsMutex in SIPConnection::InitRFC2833Handler
 * Thanks to Drazen Dimoti
 *
 * Revision 2.170  2006/08/10 04:21:40  csoutheren
 * Applied 1532444 - Small fix for SIPConnection::OnReceivedPDU
 * Thanks to Borko Jandras
 *
 * Revision 2.169  2006/08/07 19:46:24  hfriederich
 * Ensuring that no INVITE is sent if connection is already in releasing phase
 *
 * Revision 2.168  2006/07/30 11:36:09  hfriederich
 * Fixes problems with empty SDPMediaDescriptions
 *
 * Revision 2.167  2006/07/24 14:03:40  csoutheren
 * Merged in audio and video plugins from CVS branch PluginBranch
 *
 * Revision 2.166  2006/07/21 00:42:08  csoutheren
 * Applied 1525040 - More locking when changing connection's phase
 * Thanks to Borko Jandras
 *
 * Revision 2.165  2006/07/14 13:45:01  csoutheren
 * Applied 1522528 - fixes a crash on SIP call release
 * Thanks to Drazen Dimoti
 *
 * Revision 2.164  2006/07/14 04:22:43  csoutheren
 * Applied 1517397 - More Phobos stability fix
 * Thanks to Dinis Rosario
 *
 * Revision 2.163  2006/07/09 12:47:23  dsandras
 * Added clear cause for EndedByNoAnswer.
 *
 * Revision 2.162  2006/07/09 10:18:28  csoutheren
 * Applied 1517393 - Opal T.38
 * Thanks to Drazen Dimoti
 *
 * Revision 2.161  2006/07/09 10:03:29  csoutheren
 * Applied 1518681 - Refactoring boilerplate code
 * Thanks to Borko Jandras
 *
 * Revision 2.160  2006/07/05 04:29:14  csoutheren
 * Applied 1495008 - Add a callback: OnCreatingINVITE
 * Thanks to mturconi
 *
 * Revision 2.159  2006/06/30 14:55:53  dsandras
 * Reverted broken patch. Reinvites can have different branch ID's.
 *
 * Revision 2.158  2006/06/30 01:42:39  csoutheren
 * Applied 1509216 - SIPConnection::OnReceivedOK fix
 * Thanks to Boris Pavacic
 *
 * Revision 2.157  2006/06/28 11:29:08  csoutheren
 * Patch 1456858 - Add mutex to transaction dictionary and other stability patches
 * Thanks to drosario
 *
 * Revision 2.156  2006/06/27 13:50:25  csoutheren
 * Patch 1375137 - Voicetronix patches and lid enhancements
 * Thanks to Frederich Heem
 *
 * Revision 2.155  2006/06/22 00:26:04  csoutheren
 * Remove warning on gcc
 *
 * Revision 2.154  2006/06/20 05:23:58  csoutheren
 * Ignore INVITEs that arrive via different route for same call
 * Ensure CANCELs are sent for all INVITEs in all cases
 *
 * Revision 2.153  2006/06/15 17:41:34  dsandras
 * Definitely fixed recently introduced bug when forwarding calls.
 *
 * Revision 2.152  2006/06/15 00:29:58  csoutheren
 * Fixed warning on Linux
 *
 * Revision 2.151  2006/06/15 00:18:58  csoutheren
 * Fixed problem with call forwarding from last commit, thanks to Damien Sandras
 *
 * Revision 2.150  2006/06/09 04:22:24  csoutheren
 * Implemented mapping between SIP release codes and Q.931 codes as specified
 *  by RFC 3398
 *
 * Revision 2.149  2006/05/30 04:58:06  csoutheren
 * Added suport for SIP INFO message (untested as yet)
 * Fixed some issues with SIP state machine on answering calls
 * Fixed some formatting issues
 *
 * Revision 2.148  2006/05/24 00:56:14  csoutheren
 * Fixed problem with deferred answer on SIP calls
 *
 * Revision 2.147  2006/04/24 21:19:31  dsandras
 * Fixed previous commit.
 *
 * Revision 2.146  2006/04/23 20:12:52  dsandras
 * The RFC tells that the SDP answer SHOULD have the same payload type than the
 * SDP offer. Added rtpmap support to allow this. Fixes problems with Asterisk,
 * and Ekiga report #337456.
 *
 * Revision 2.145  2006/04/22 19:49:33  dsandras
 * Immediately initialize the transport with the proxy address (if any) when
 * doing the connection setup. Fixed Ekiga report #334455.
 *
 * Revision 2.144  2006/04/22 10:48:14  dsandras
 * Added support for SDP offers in the OK response, and SDP answer in the ACK
 * request.
 *
 * Revision 2.143  2006/04/10 05:18:41  csoutheren
 * Fix problem where reverse channel is opened using wrong media format list
 *
 * Revision 2.142  2006/04/06 20:39:41  dsandras
 * Keep the same From header when sending authenticated requests than in the
 * original request. Fixes Ekiga report #336762.
 *
 * Revision 2.141  2006/03/23 00:24:49  csoutheren
 * Detect if ClearCall is used within OnIncomingConnection
 *
 * Revision 2.140  2006/03/19 17:18:46  dsandras
 * Fixed SRV handling.
 *
 * Revision 2.139  2006/03/19 16:59:00  dsandras
 * Removed cout again.
 *
 * Revision 2.138  2006/03/19 16:58:19  dsandras
 * Do not release a call we are originating when receiving CANCEL.
 *
 * Revision 2.137  2006/03/19 16:05:00  dsandras
 * Improved CANCEL handling. Ignore missing transport type.
 *
 * Revision 2.136  2006/03/18 21:45:28  dsandras
 * Do an SRV lookup when creating the connection. Some domains to which
 * INVITEs are sent do not have an A record, which makes the transport creation
 * and the connection fail. Fixes Ekiga report #334994.
 *
 * Revision 2.135  2006/03/08 18:34:41  dsandras
 * Added DNS SRV lookup.
 *
 * Revision 2.134  2006/03/06 22:52:59  csoutheren
 * Reverted experimental SRV patch due to unintended side effects
 *
 * Revision 2.133  2006/03/06 12:56:02  csoutheren
 * Added experimental support for SIP SRV lookups
 *
 * Revision 2.132.2.7  2006/04/25 01:06:22  csoutheren
 * Allow SIP-only codecs
 *
 * Revision 2.132.2.6  2006/04/10 06:24:30  csoutheren
 * Backport from CVS head up to Plugin_Merge3
 *
 * Revision 2.132.2.5  2006/04/10 05:24:42  csoutheren
 * Backport from CVS head
 *
 * Revision 2.132.2.4  2006/04/06 05:34:16  csoutheren
 * Backports from CVS head up to Plugin_Merge2
 *
 * Revision 2.132.2.3  2006/04/06 05:33:09  csoutheren
 * Backports from CVS head up to Plugin_Merge2
 *
 * Revision 2.132.2.2  2006/04/06 01:21:21  csoutheren
 * More implementation of video codec plugins
 *
 * Revision 2.132.2.1  2006/03/20 02:25:27  csoutheren
 * Backports from CVS head
 *
 * Revision 2.143  2006/04/10 05:18:41  csoutheren
 * Fix problem where reverse channel is opened using wrong media format list
 *
 * Revision 2.142  2006/04/06 20:39:41  dsandras
 * Keep the same From header when sending authenticated requests than in the
 * original request. Fixes Ekiga report #336762.
 *
 * Revision 2.141  2006/03/23 00:24:49  csoutheren
 * Detect if ClearCall is used within OnIncomingConnection
 *
 * Revision 2.140  2006/03/19 17:18:46  dsandras
 * Fixed SRV handling.
 *
 * Revision 2.139  2006/03/19 16:59:00  dsandras
 * Removed cout again.
 *
 * Revision 2.138  2006/03/19 16:58:19  dsandras
 * Do not release a call we are originating when receiving CANCEL.
 *
 * Revision 2.137  2006/03/19 16:05:00  dsandras
 * Improved CANCEL handling. Ignore missing transport type.
 *
 * Revision 2.136  2006/03/18 21:45:28  dsandras
 * Do an SRV lookup when creating the connection. Some domains to which
 * INVITEs are sent do not have an A record, which makes the transport creation
 * and the connection fail. Fixes Ekiga report #334994.
 *
 * Revision 2.135  2006/03/08 18:34:41  dsandras
 * Added DNS SRV lookup.
 *
 * Revision 2.134  2006/03/06 22:52:59  csoutheren
 * Reverted experimental SRV patch due to unintended side effects
 *
 * Revision 2.133  2006/03/06 12:56:02  csoutheren
 * Added experimental support for SIP SRV lookups
 *
 * Revision 2.132  2006/02/11 21:05:27  dsandras
 * Release the call when something goes wrong.
 *
 * Revision 2.131  2006/02/11 15:47:41  dsandras
 * When receiving an invite, try using the remote prefered codec. The targetAddress
 * should be initialized to the contact field value of the incoming invite
 * when receiving a call.
 *
 * Revision 2.130  2006/02/10 23:44:04  csoutheren
 * Applied fix for SetConnection and RFC2833 startup
 *
 * Revision 2.129  2006/02/06 22:40:11  dsandras
 * Added additional check for rtpSession thanks to Guillaume Fraysse.
 *
 * Revision 2.128  2006/02/04 16:22:38  dsandras
 * Fixed recently introduced bug reported by Guillaume Fraysse. Thanks!
 *
 * Revision 2.127  2006/02/04 16:06:24  dsandras
 * Fixed problems with media formats being used when calling and when the remote
 * has a different prefered order than ours.
 *
 * Revision 2.126  2006/02/02 07:02:58  csoutheren
 * Added RTP payload map to transcoders and connections to allow remote SIP endpoints
 * to change the payload type used for outgoing RTP.
 *
 * Revision 2.125  2006/01/29 20:55:32  dsandras
 * Allow using a simple username or a fill url when registering.
 *
 * Revision 2.124  2006/01/23 22:54:57  csoutheren
 * Ensure codec remove mask is applied on outgoing SIP calls
 *
 * Revision 2.123  2006/01/23 22:11:06  dsandras
 * Only rebuild the routeSet if we have an outbound proxy and the routeSet is
 * empty.
 *
 * Revision 2.122  2006/01/22 22:18:36  dsandras
 * Added a failure case.
 *
 * Revision 2.121  2006/01/21 13:55:02  dsandras
 * Fixed default route set when an outbound proxy is being used thanks to Vincent
 * Untz <vuntz gnome org>. Thanks!
 *
 * Revision 2.120  2006/01/16 23:05:09  dsandras
 * Minor fixes. Reset the route set to the proxy (if any), when authenticating
 * invite.
 *
 * Revision 2.119  2006/01/12 20:23:44  dsandras
 * Reorganized things to prevent crashes when calling itself.
 *
 * Revision 2.118  2006/01/09 12:19:07  csoutheren
 * Added member variables to capture incoming destination addresses
 *
 * Revision 2.117  2006/01/09 11:49:46  dsandras
 * Call SetRemoteSocketInfo to update the remote address when doing calls.
 *
 * Revision 2.116  2006/01/02 14:47:28  dsandras
 * More code cleanups.
 *
 * Revision 2.115  2006/01/02 11:28:07  dsandras
 * Some documentation. Various code cleanups to prevent duplicate code.
 *
 * Revision 2.114  2005/12/26 20:53:37  dsandras
 * Small fix.
 *
 * Revision 2.113  2005/12/14 17:59:50  dsandras
 * Added ForwardConnection executed when the remote asks for a call forwarding.
 * Similar to what is done in the H.323 part with the method of the same name.
 *
 * Revision 2.112  2005/12/11 12:23:01  dsandras
 * Prevent loop of authenticating when it fails by storing the last parameters
 * before they are updated.
 *
 * Revision 2.111  2005/12/06 21:32:25  dsandras
 * Applied patch from Frederic Heem <frederic.heem _Atttt_ telsey.it> to fix
 * assert in PSyncPoint when OnReleased is called twice from different threads.
 * Thanks! (Patch #1374240)
 *
 * Revision 2.110  2005/12/04 15:02:00  dsandras
 * Fixed IP translation in the VIA field of most request PDUs.
 *
 * Revision 2.109  2005/11/25 21:02:52  dsandras
 * Release the call if the SDP can not be built.
 *
 * Revision 2.108  2005/11/20 20:55:55  dsandras
 * End the connection when the session can not be created.
 *
 * Revision 2.107  2005/10/27 20:53:36  dsandras
 * Send a BYE when receiving a NOTIFY for a REFER we just sent.
 *
 * Revision 2.106  2005/10/22 19:57:33  dsandras
 * Added SIP failure cause.
 *
 * Revision 2.105  2005/10/22 14:43:48  dsandras
 * Little cleanup.
 *
 * Revision 2.104  2005/10/22 12:16:05  dsandras
 * Moved mutex preventing media streams to be opened before they are completely closed to the SIPConnection class.
 *
 * Revision 2.103  2005/10/20 20:25:28  dsandras
 * Update the RTP Session address when the new streams are started.
 *
 * Revision 2.102  2005/10/18 20:50:13  dsandras
 * Fixed tone playing in session progress.
 *
 * Revision 2.101  2005/10/18 17:50:35  dsandras
 * Fixed REFER so that it doesn't send a BYE but sends the NOTIFY. Fixed leak on exit if a REFER transaction was in progress while the connection is destroyed.
 *
 * Revision 2.100  2005/10/13 19:33:50  dsandras
 * Added GetDirection to get the default direction for a media stream. Modified OnSendMediaDescription to call BuildSDP if no reverse streams can be opened.
 *
 * Revision 2.99  2005/10/13 18:14:45  dsandras
 * Another try to get it right.
 *
 * Revision 2.98  2005/10/12 21:39:04  dsandras
 * Small protection for the SDP.
 *
 * Revision 2.97  2005/10/12 21:32:08  dsandras
 * Cleanup.
 *
 * Revision 2.96  2005/10/12 21:10:39  dsandras
 * Removed CanAutoStartTransmitVideo and CanAutoStartReceiveVideo from the SIPConnection class.
 *
 * Revision 2.95  2005/10/12 17:55:18  dsandras
 * Fixed previous commit.
 *
 * Revision 2.94  2005/10/12 08:53:42  dsandras
 * Committed cleanup patch.
 *
 * Revision 2.93  2005/10/11 21:51:44  dsandras
 * Reverted a previous patch.
 *
 * Revision 2.92  2005/10/11 21:47:04  dsandras
 * Fixed problem when sending the 200 OK response to an INVITE for which some media stream is 'sendonly'.
 *
 * Revision 2.91  2005/10/08 19:27:26  dsandras
 * Added support for OnForwarded.
 *
 * Revision 2.90  2005/10/05 21:27:25  dsandras
 * Having a source/sink stream is not opened because of sendonly/recvonly is not
 * a stream opening failure. Fixed problems with streams direction.
 *
 * Revision 2.89  2005/10/04 16:32:25  dsandras
 * Added back support for CanAutoStartReceiveVideo.
 *
 * Revision 2.88  2005/10/04 12:57:18  rjongbloed
 * Removed CanOpenSourceMediaStream/CanOpenSinkMediaStream functions and
 *   now use overides on OpenSourceMediaStream/OpenSinkMediaStream
 *
 * Revision 2.87  2005/10/02 17:49:08  dsandras
 * Cleaned code to use the new GetContactAddress.
 *
 * Revision 2.86  2005/09/27 16:17:12  dsandras
 * - Added SendPDU method and use it everywhere for requests sent in the dialog
 * and to transmit responses to incoming requests.
 * - Fixed again re-INVITE support and 200 OK generation.
 * - Update the route set when receiving responses and requests according to the
 * RFC.
 * - Update the targetAddress to the Contact field when receiving a response.
 * - Transmit the ack for 2xx and non-2xx responses.
 * - Do not update the remote transport address when receiving requests and
 * responses. Do it only in SendPDU as the remote transport address for a response
 * and for a request in a dialog are different.
 * - Populate the route set with an initial route when an outbound proxy should
 * be used as recommended by the RFC.
 *
 * Revision 2.85  2005/09/21 21:03:04  dsandras
 * Fixed reINVITE support.
 *
 * Revision 2.84  2005/09/21 19:50:30  dsandras
 * Cleaned code. Make use of the new SIP_PDU function that returns the correct address where to send responses to incoming requests.
 *
 * Revision 2.83  2005/09/20 17:13:57  dsandras
 * Fixed warning.
 *
 * Revision 2.82  2005/09/20 17:04:35  dsandras
 * Removed redundant call to SetBufferSize.
 *
 * Revision 2.81  2005/09/20 07:57:29  csoutheren
 * Fixed bug in previous commit
 *
 * Revision 2.80  2005/09/20 07:24:05  csoutheren
 * Removed assumption of UDP transport for SIP
 *
 * Revision 2.79  2005/09/17 20:54:16  dsandras
 * Check for existing transport before using it.
 *
 * Revision 2.78  2005/09/15 17:07:47  dsandras
 * Fixed video support. Make use of BuildSDP when possible. Do not open the sink/source media streams if the remote or the local user have disabled audio/video transmission/reception by checking the direction media and session attributes.
 *
 * Revision 2.77  2005/08/25 18:49:52  dsandras
 * Added SIP Video support. Changed API of BuildSDP to allow it to be called
 * for both audio and video.
 *
 * Revision 2.76  2005/08/09 09:16:24  rjongbloed
 * Fixed compiler warning
 *
 * Revision 2.75  2005/08/04 17:15:52  dsandras
 * Fixed local port for sending requests on incoming calls.
 * Allow for codec changes on re-INVITE.
 * More blind transfer implementation.
 *
 * Revision 2.74  2005/07/15 17:22:43  dsandras
 * Use correct To tag when sending a new INVITE when authentication is required and an outbound proxy is being used.
 *
 * Revision 2.73  2005/07/14 08:51:19  csoutheren
 * Removed CreateExternalRTPAddress - it's not needed because you can override GetMediaAddress
 * to do the same thing
 * Fixed problems with logic associated with media bypass
 *
 * Revision 2.72  2005/07/11 01:52:26  csoutheren
 * Extended AnsweringCall to work for SIP as well as H.323
 * Fixed problems with external RTP connection in H.323
 * Added call to OnClosedMediaStream
 *
 * Revision 2.71  2005/06/04 12:44:36  dsandras
 * Applied patch from Ted Szoczei to fix leaks and problems on cancelling a call and to improve the Allow PDU field handling.
 *
 * Revision 2.70  2005/05/25 18:34:25  dsandras
 * Added missing AdjustMediaFormats so that only enabled common codecs are used on outgoing calls.
 *
 * Revision 2.69  2005/05/23 20:55:15  dsandras
 * Use STUN on incoming calls if required so that all the PDU fields are setup appropriately.
 *
 * Revision 2.68  2005/05/18 17:26:39  dsandras
 * Added back proxy support for INVITE.
 *
 * Revision 2.67  2005/05/16 14:40:32  dsandras
 * Make the connection fail when there is no authentication information present
 * and authentication is required.
 *
 * Revision 2.66  2005/05/12 19:47:29  dsandras
 * Fixed indentation.
 *
 * Revision 2.65  2005/05/06 07:37:06  csoutheren
 * Various changed while working with SIP carrier
 *   - remove assumption that authentication realm is a domain name.
 *   - stopped rewrite of "To" field when proxy being used
 *   - fix Contact field in REGISTER to match actual port used when Symmetric NATin use
 *   - lots of formatting changes and cleanups
 *
 * Revision 2.64  2005/05/04 17:09:40  dsandras
 * Re-Invite happens during the "EstablishedPhase". Ignore duplicate INVITEs
 * due to retransmission.
 *
 * Revision 2.63  2005/05/02 21:31:54  dsandras
 * Reinvite only if connectedPhase.
 *
 * Revision 2.62  2005/05/02 21:23:22  dsandras
 * Set default contact port to the first listener port.
 *
 * Revision 2.61  2005/05/02 20:43:03  dsandras
 * Remove the via parameters when updating to the via address in the transport.
 *
 * Revision 2.60  2005/04/28 20:22:54  dsandras
 * Applied big sanity patch for SIP thanks to Ted Szoczei <tszoczei@microtronix.ca>.
 * Thanks a lot!
 *
 * Revision 2.59  2005/04/28 07:59:37  dsandras
 * Applied patch from Ted Szoczei to fix problem when answering to PDUs containing
 * multiple Via fields in the message header. Thanks!
 *
 * Revision 2.58  2005/04/18 17:07:17  dsandras
 * Fixed cut and paste error in last commit thanks to Ted Szoczei.
 *
 * Revision 2.57  2005/04/16 18:57:36  dsandras
 * Use a TO header without tag when restarting an INVITE.
 *
 * Revision 2.56  2005/04/11 11:12:38  dsandras
 * Added Method_MESSAGE support for future use.
 *
 * Revision 2.55  2005/04/11 10:23:58  dsandras
 * 1) Added support for SIP ReINVITE without codec changes.
 * 2) Added support for GetRemotePartyCallbackURL.
 * 3) Added support for call hold (local and remote).
 * 4) Fixed missing tag problem when sending BYE requests.
 * 5) Added support for Blind Transfer (without support for being transfered).
 *
 * Revision 2.54  2005/03/11 18:12:09  dsandras
 * Added support to specify the realm when registering. That way softphones already know what authentication information to use when required. The realm/domain can also be used in the From field.
 *
 * Revision 2.53  2005/02/19 22:48:48  dsandras
 * Added the possibility to register to several registrars and be able to do authenticated calls to each of them. Added SUBSCRIBE/NOTIFY support for Message Waiting Indications.
 *
 * Revision 2.52  2005/02/19 22:26:09  dsandras
 * Ignore TO tag added by OPAL. Reported by Nick Noath.
 *
 * Revision 2.51  2005/01/16 11:28:05  csoutheren
 * Added GetIdentifier virtual function to OpalConnection, and changed H323
 * and SIP descendants to use this function. This allows an application to
 * obtain a GUID for any connection regardless of the protocol used
 *
 * Revision 2.50  2005/01/15 19:42:33  csoutheren
 * Added temporary workaround for deadlock that occurs with incoming calls
 *
 * Revision 2.49  2005/01/09 03:42:46  rjongbloed
 * Fixed warning about unused parameter
 *
 * Revision 2.48  2004/12/25 20:43:42  dsandras
 * Attach the RFC2833 handlers when we are in connected state to ensure
 * OpalMediaPatch exist. Fixes problem for DTMF sending.
 *
 * Revision 2.47  2004/12/23 20:43:27  dsandras
 * Only start the media streams if we are in the phase connectedPhase.
 *
 * Revision 2.46  2004/12/22 18:55:08  dsandras
 * Added support for Call Forwarding the "302 Moved Temporarily" SIP response.
 *
 * Revision 2.45  2004/12/18 03:54:43  rjongbloed
 * Added missing call of callback virtual for SIP 100 Trying response
 *
 * Revision 2.44  2004/12/12 13:40:45  dsandras
 * - Correctly update the remote party name, address and applications at various strategic places.
 * - If no outbound proxy authentication is provided, then use the registrar authentication parameters when proxy authentication is required.
 * - Fixed use of connectedTime.
 * - Correctly set the displayName in the FROM field for outgoing connections.
 * - Added 2 "EndedBy" cases when a connection ends.
 *
 * Revision 2.43  2004/11/29 08:18:32  csoutheren
 * Added support for setting the SIP authentication domain/realm as needed for many service
 *  providers
 *
 * Revision 2.42  2004/08/20 12:13:32  rjongbloed
 * Added correct handling of SIP 180 response
 *
 * Revision 2.41  2004/08/14 07:56:43  rjongbloed
 * Major revision to utilise the PSafeCollection classes for the connections and calls.
 *
 * Revision 2.40  2004/04/26 05:40:39  rjongbloed
 * Added RTP statistics callback to SIP
 *
 * Revision 2.39  2004/04/25 08:34:08  rjongbloed
 * Fixed various GCC 3.4 warnings
 *
 * Revision 2.38  2004/03/16 12:03:33  rjongbloed
 * Fixed poropagating port of proxy to connection target address.
 *
 * Revision 2.37  2004/03/15 12:33:48  rjongbloed
 * Fixed proxy override in URL
 *
 * Revision 2.36  2004/03/14 11:32:20  rjongbloed
 * Changes to better support SIP proxies.
 *
 * Revision 2.35  2004/03/14 10:09:54  rjongbloed
 * Moved transport on SIP top be constructed by endpoint as any transport created on
 *   an endpoint can receive data for any connection.
 *
 * Revision 2.34  2004/03/13 06:29:27  rjongbloed
 * Slight rearrangement of local party name and alias list to beter match common
 *   behaviour in ancestor.
 * Abstracted local party name for endpoint into ancestor from H.,323.
 * Changed parameter in UDP write function to void * from PObject *.
 *
 * Revision 2.33  2004/02/24 11:33:47  rjongbloed
 * Normalised RTP session management across protocols
 * Added support for NAT (via STUN)
 *
 * Revision 2.32  2004/02/15 03:12:10  rjongbloed
 * More patches from Ted Szoczei, fixed shutting down of RTP session if INVITE fails. Added correct return if no meda formats can be matched.
 *
 * Revision 2.31  2004/02/14 22:52:51  csoutheren
 * Re-ordered initialisations to remove warning on Linux
 *
 * Revision 2.30  2004/02/07 02:23:21  rjongbloed
 * Changed to allow opening of more than just audio streams.
 * Assured that symmetric codecs are used as required by "basic" SIP.
 * Made media format list subject to adjust \\ment by remoe/order lists.
 *
 * Revision 2.29  2004/01/08 22:23:23  csoutheren
 * Fixed problem with not using session ID when constructing SDP lists
 *
 * Revision 2.28  2003/12/20 12:21:18  rjongbloed
 * Applied more enhancements, thank you very much Ted Szoczei
 *
 * Revision 2.27  2003/12/15 11:56:17  rjongbloed
 * Applied numerous bug fixes, thank you very much Ted Szoczei
 *
 * Revision 2.26  2003/03/17 10:27:00  robertj
 * Added video support.
 *
 * Revision 2.25  2003/03/07 05:52:35  robertj
 * Made sure connection is locked with all function calls that are across
 *   the "call" object.
 *
 * Revision 2.24  2003/03/06 03:57:47  robertj
 * IVR support (work in progress) requiring large changes everywhere.
 *
 * Revision 2.23  2002/11/10 11:33:20  robertj
 * Updated to OpenH323 v1.10.3
 *
 * Revision 2.22  2002/09/12 06:58:34  robertj
 * Removed protocol prefix strings as static members as has problems with
 *   use in DLL environment.
 *
 * Revision 2.21  2002/07/01 04:56:33  robertj
 * Updated to OpenH323 v1.9.1
 *
 * Revision 2.20  2002/06/16 02:26:37  robertj
 * Fixed creation of RTP session for incoming calls, thanks Ted Szoczei
 *
 * Revision 2.19  2002/04/17 07:22:54  robertj
 * Added identifier for conenction in OnReleased trace message.
 *
 * Revision 2.18  2002/04/16 09:04:18  robertj
 * Fixed setting of target URI from Contact regardless of route set
 *
 * Revision 2.17  2002/04/16 07:53:15  robertj
 * Changes to support calls through proxies.
 *
 * Revision 2.16  2002/04/15 08:53:58  robertj
 * Fixed correct setting of jitter buffer size in RTP media stream.
 * Fixed starting incoming (not outgoing!) media on call progress.
 *
 * Revision 2.15  2002/04/10 06:58:31  robertj
 * Fixed incorrect return value when starting INVITE transactions.
 *
 * Revision 2.14  2002/04/10 03:14:35  robertj
 * Moved code for handling media bypass address resolution into ancestor as
 *   now done ths same way in both SIP and H.323.
 * Major changes to RTP session management when initiating an INVITE.
 * Improvements in error handling and transaction cancelling.
 *
 * Revision 2.13  2002/04/09 07:26:20  robertj
 * Added correct error return if get transport failure to remote endpoint.
 *
 * Revision 2.12  2002/04/09 01:19:41  robertj
 * Fixed typo on changing "callAnswered" to better description of "originating".
 *
 * Revision 2.11  2002/04/09 01:02:14  robertj
 * Fixed problems with restarting INVITE on  authentication required response.
 *
 * Revision 2.10  2002/04/08 02:40:13  robertj
 * Fixed issues with using double originate call, eg from simple app command line.
 *
 * Revision 2.9  2002/04/05 10:42:04  robertj
 * Major changes to support transactions (UDP timeouts and retries).
 *
 * Revision 2.8  2002/03/18 08:13:42  robertj
 * Added more logging.
 * Removed proxyusername/proxypassword parameters from URL passed on.
 * Fixed GNU warning.
 *
 * Revision 2.7  2002/03/15 10:55:28  robertj
 * Added ability to specify proxy username/password in URL.
 *
 * Revision 2.6  2002/03/08 06:28:03  craigs
 * Changed to allow Authorisation to be included in other PDUs
 *
 * Revision 2.5  2002/02/19 07:53:17  robertj
 * Restructured media bypass functions to fix problems with RFC2833.
 *
 * Revision 2.4  2002/02/13 04:55:44  craigs
 * Fixed problem with endless loop if proxy keeps failing authentication with 407
 *
 * Revision 2.3  2002/02/11 07:33:36  robertj
 * Changed SDP to use OpalTransport for hosts instead of IP addresses/ports
 * Added media bypass for streams between compatible protocols.
 *
 * Revision 2.2  2002/02/01 05:47:55  craigs
 * Removed :: from in front of isspace that confused gcc 2.96
 *
 * Revision 2.1  2002/02/01 04:53:01  robertj
 * Added (very primitive!) SIP support.
 *
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "sipcon.h"
#endif

#include <sip/sipcon.h>

#include <sip/sipep.h>
#include <codec/rfc2833.h>
#include <opal/manager.h>
#include <opal/call.h>
#include <opal/patch.h>
#include <ptclib/random.h>              // for local dialog tag
#include <ptclib/pdns.h>


typedef void (SIPConnection::* SIPMethodFunction)(SIP_PDU & pdu);

static const char ApplicationDTMFRelayKey[] = "application/dtmf-relay";
static const char ApplicationDTMFKey[]      = "application/dtmf";

#define new PNEW


////////////////////////////////////////////////////////////////////////////

SIPConnection::SIPConnection(OpalCall & call,
                             SIPEndPoint & ep,
                             const PString & token,
                             const SIPURL & destination,
                             OpalTransport * inviteTransport,
                             unsigned int options,
                             OpalConnection::StringOptions * stringOptions)
  : OpalConnection(call, ep, token, options, stringOptions),
    endpoint(ep),
    pduSemaphore(0, P_MAX_INDEX)
{
  SIPURL transportAddress = destination;
  targetAddress = destination;

  // Look for a "proxy" parameter to override default proxy
  PStringToString params = targetAddress.GetParamVars();
  SIPURL proxy;
  if (params.Contains("proxy")) {
    proxy.Parse(params("proxy"));
    targetAddress.SetParamVar("proxy", PString::Empty());
  }

  if (proxy.IsEmpty())
    proxy = endpoint.GetProxy();

  // Default routeSet if there is a proxy
  if (!proxy.IsEmpty()) 
    routeSet += "sip:" + proxy.GetHostName() + ':' + PString(proxy.GetPort()) + ";lr";
  
  // Update remote party parameters
  remotePartyAddress = targetAddress.AsQuotedString();
  remotePartyName = SIPURL (remotePartyAddress).GetDisplayName ();
  
  // Do a DNS SRV lookup
#if P_DNS
    PIPSocketAddressAndPortVector addrs;
    if (PDNS::LookupSRV(destination.GetHostName(), "_sip._udp", destination.GetPort(), addrs)) {
      transportAddress.SetHostName(addrs[0].address.AsString());
      transportAddress.SetPort(addrs [0].port);
    }
#endif

  // Create the transport
  if (inviteTransport == NULL)
    transport = NULL;
  else 
    transport = endpoint.CreateTransport(inviteTransport->GetLocalAddress(), TRUE);
  
  lastTransportAddress = transportAddress.GetHostAddress();

  originalInvite = NULL;
  pduHandler = NULL;
  lastSentCSeq = 0;
  releaseMethod = ReleaseWithNothing;

  transactions.DisallowDeleteObjects();

  referTransaction = NULL;
  local_hold = FALSE;
  remote_hold = FALSE;

  udpTransport = NULL;

  PTRACE(3, "SIP\tCreated connection.");
}


SIPConnection::~SIPConnection()
{
  delete originalInvite;
  delete transport;
  delete referTransaction;

  if (pduHandler) delete pduHandler;
  if (udpTransport) delete udpTransport;

  PTRACE(3, "SIP\tDeleted connection.");
}

//
// This table comes from RFC 3398 para 7.2.4.1
//
static struct Q931ReasonMapEntry {
  unsigned q931Cause;
  unsigned sipCode;
} Q931ReasonToSIPCode[] = {
  {   1, 404 }, // unallocated number                   404 Not Found
  {   2, 404 }, // no route to network                  404 Not found
  {   3, 404 }, // no route to destination              404 Not found
  //{ 16, -- }, // normal call clearing                 --- (*)
  {  17, 486 }, // user busy                            486 Busy here
  {  18, 408 }, // no user responding                   408 Request Timeout
  {  19, 480 }, // no answer from the user              480 Temporarily unavailable
  {  20, 480 }, // subscriber absent                    480 Temporarily unavailable
  {  21, 403 }, // call rejected                        403 Forbidden (+)
  {  22, 410 }, // number changed (w/o diagnostic)      410 Gone
  {  22, 301 }, // number changed (w/ diagnostic)       301 Moved Permanently
  {  23, 410 }, // redirection to new destination       410 Gone
  {  26, 404 }, // non-selected user clearing           404 Not Found (=)
  {  27, 502 }, // destination out of order             502 Bad Gateway
  {  28, 484 }, // address incomplete                   484 Address incomplete
  {  29, 501 }, // facility rejected                    501 Not implemented
  {  31, 480 }, // normal unspecified                   480 Temporarily unavailable
  {  34, 503 }, // no circuit available                 503 Service unavailable
  {  38, 503 }, // network out of order                 503 Service unavailable
  {  41, 503 }, // temporary failure                    503 Service unavailable
  {  42, 503 }, // switching equipment congestion       503 Service unavailable
  {  47, 503 }, // resource unavailable                 503 Service unavailable
  {  55, 403 }, // incoming calls barred within CUG     403 Forbidden
  {  57, 403 }, // bearer capability not authorized     403 Forbidden
  {  58, 503 }, // bearer capability not presently available     503 Service unavailable
  {  65, 488 }, // bearer capability not implemented    488 Not Acceptable Here
  {  70, 488 }, // only restricted digital avail        488 Not Acceptable Here
  {  79, 501 }, // service or option not implemented    501 Not implemented
  {  87, 403 }, // user not member of CUG               403 Forbidden
  {  88, 503 }, // incompatible destination             503 Service unavailable
  { 102, 504 }, // recovery of timer expiry             504 Gateway timeout
  { 111, 500 }, // protocol error                       500 Server internal error
  { 127, 500 }, // interworking unspecified             500 Server internal error
};

#define NUM_Q931_END_REASON_MAP_ENTRIES (sizeof(Q931ReasonToSIPCode) / sizeof(Q931ReasonToSIPCode[0]))

static SIP_PDU::StatusCodes RFC3398_MapQ931ToSIPCode(unsigned q931Cause)
{
  unsigned int i;
  for (i = 0; i < NUM_Q931_END_REASON_MAP_ENTRIES; ++i)
    if (Q931ReasonToSIPCode[i].q931Cause == q931Cause)
      return (SIP_PDU::StatusCodes)Q931ReasonToSIPCode[i].sipCode;

  return SIP_PDU::Failure_BadGateway;
}

static struct CallEndReasonMapEntry {
  OpalConnection::CallEndReason reason;
  SIP_PDU::StatusCodes          sipCode;
} EndReasonToSIPCode[] = {
  { OpalConnection::EndedByAnswerDenied,       SIP_PDU::GlobalFailure_Decline },        // 603
  { OpalConnection::EndedByLocalBusy,          SIP_PDU::Failure_BusyHere },             // 486
  { OpalConnection::EndedByCallerAbort,        SIP_PDU::Failure_RequestTerminated },    // 487
  { OpalConnection::EndedByCapabilityExchange, SIP_PDU::Failure_UnsupportedMediaType }, // 415
  { OpalConnection::EndedByCallForwarded,      SIP_PDU::Redirection_MovedTemporarily },  // 302
  { OpalConnection::EndedByNoAnswer,           SIP_PDU::Failure_TemporarilyUnavailable } // 480
};

#define NUM_CALL_END_REASON_MAP_ENTRIES (sizeof(EndReasonToSIPCode) / sizeof(EndReasonToSIPCode[0]))

static SIP_PDU::StatusCodes MapEndReasonToSIPCode(OpalConnection::CallEndReason callEndReason)
{
  unsigned int i;
  for (i = 0; i < NUM_CALL_END_REASON_MAP_ENTRIES; ++i)
    if (EndReasonToSIPCode[i].reason == callEndReason)
      return EndReasonToSIPCode[i].sipCode;

  return SIP_PDU::Failure_BadGateway;
}


void SIPConnection::OnReleased()
{
  PTRACE(3, "SIP\tOnReleased: " << *this << ", phase = " << phase);
  
  // OpalConnection::Release sets the phase to Releasing in the SIP Handler 
  // thread
  if (GetPhase() >= ReleasedPhase){
    PTRACE(2, "SIP\tOnReleased: already released");
    return;
  };

  SetPhase(ReleasingPhase);

  SIPTransaction * byeTransaction = NULL;

  switch (releaseMethod) {
    case ReleaseWithNothing :
      break;

    case ReleaseWithResponse :
      {
        SIP_PDU::StatusCodes sipCode;
        // if Q.931 code has been set and is not normal call clearing, then implement mapping specified 
        // Otherwise implement default mapping
        if (GetQ931Cause() != 0x100 && GetQ931Cause() != 0x16)
          sipCode = RFC3398_MapQ931ToSIPCode(q931Cause);
        else
          sipCode = MapEndReasonToSIPCode(callEndReason);

        // EndedByCallForwarded is a special case because it needs different paramaters
        if (callEndReason == (int)EndedByCallForwarded) {
          SendInviteResponse(sipCode, NULL, forwardParty);
        } else {
          SendInviteResponse(sipCode);
        }
      }
      break;

    case ReleaseWithBYE :
      // create BYE now & delete it later to prevent memory access errors
      byeTransaction = new SIPTransaction(*this, *transport, SIP_PDU::Method_BYE);
      break;

    case ReleaseWithCANCEL :
      {
        std::vector<BOOL> statuses;
        statuses.resize(invitations.GetSize());
        PINDEX i;
        {
          PWaitAndSignal m(invitationsMutex);
          for (i = 0; i < invitations.GetSize(); i++) {
            PTRACE(3, "SIP\tCancelling transaction " << i << " of " << invitations.GetSize());
            statuses[i] = invitations[i].SendCANCEL();
          }
        }
        for (i = 0; i < invitations.GetSize(); i++) {
          if (statuses[i]) {
            invitations[i].Wait();
            PTRACE(3, "SIP\tTransaction " << i << " cancelled");
          } else {
            PTRACE(3, "SIP\tCould not cancel transaction " << i);
          }
        }
      }
  }

  // Close media
  streamsMutex.Wait();
  CloseMediaStreams();
  streamsMutex.Signal();

  // Sent a BYE, wait for it to complete
  if (byeTransaction != NULL) {
    byeTransaction->Wait();
    delete byeTransaction;
  }

  SetPhase(ReleasedPhase);

  if (pduHandler != NULL) {
    pduSemaphore.Signal();
    pduHandler->WaitForTermination();
  }

  if (transport != NULL)
    transport->CloseWait();

  OpalConnection::OnReleased();
  
  // Remove all INVITEs
  {
    PWaitAndSignal m(invitationsMutex); 
    invitations.RemoveAll();
  }
}


void SIPConnection::TransferConnection(const PString & remoteParty, const PString & callIdentity)
{
  // There is still an ongoing REFER transaction 
  if (referTransaction != NULL) 
    return;
 
  referTransaction = new SIPRefer(*this, *transport, remoteParty, callIdentity);
  referTransaction->Start ();
}

BOOL SIPConnection::SetAlerting(const PString & /*calleeName*/, BOOL /*withMedia*/)
{
  if (IsOriginating()) {
    PTRACE(2, "SIP\tSetAlerting ignored on call we originated.");
    return TRUE;
  }

  PSafeLockReadWrite safeLock(*this);
  if (!safeLock.IsLocked())
    return FALSE;
  
  PTRACE(2, "SIP\tSetAlerting");

  if (phase != SetUpPhase) 
    return FALSE;

  SendInviteResponse(SIP_PDU::Information_Ringing);
  SetPhase(AlertingPhase);

  return TRUE;
}


BOOL SIPConnection::SetConnected()
{
  if (transport == NULL) {
    Release(EndedByTransportFail);
    return FALSE;
  }

  BOOL sdpFailure = TRUE;

  if (IsOriginating()) {
    PTRACE(2, "SIP\tSetConnected ignored on call we originated.");
    return TRUE;
  }

  PSafeLockReadWrite safeLock(*this);
  if (!safeLock.IsLocked())
    return FALSE;
  
  if (GetPhase() >= ConnectedPhase) {
    PTRACE(2, "SIP\tSetConnected ignored on already connected call.");
    return FALSE;
  }
  
  PTRACE(2, "SIP\tSetConnected");

  SDPSessionDescription sdpOut(GetLocalAddress());

  // get the remote media formats, if any
  if (originalInvite->HasSDP()) {
    remoteSDP = originalInvite->GetSDP();

    sdpFailure = !OnSendSDPMediaDescription(remoteSDP, SDPMediaDescription::Audio, OpalMediaFormat::DefaultAudioSessionID, sdpOut);
    sdpFailure = !OnSendSDPMediaDescription(remoteSDP, SDPMediaDescription::Video, OpalMediaFormat::DefaultVideoSessionID, sdpOut) && sdpFailure;
    sdpFailure = !OnSendSDPMediaDescription(remoteSDP, SDPMediaDescription::Image, OpalMediaFormat::DefaultDataSessionID, sdpOut) && sdpFailure;
  }

  if (sdpFailure) {
    SDPSessionDescription *sdp = (SDPSessionDescription *) &sdpOut;
    sdpFailure = !BuildSDP(sdp, rtpSessions, OpalMediaFormat::DefaultAudioSessionID);
    sdpFailure = !BuildSDP(sdp, rtpSessions, OpalMediaFormat::DefaultVideoSessionID) && sdpFailure;
    sdpFailure = !BuildSDP(sdp, rtpSessions, OpalMediaFormat::DefaultDataSessionID) && sdpFailure;

    if (sdpFailure) {
      Release(EndedByCapabilityExchange);
      return FALSE;
    }
  }
  
  // abort if already in releasing phase
  if (phase >= ReleasingPhase) {
    return FALSE;
  }
    
  // update the route set and the target address according to 12.1.1
  // requests in a dialog do not modify the route set according to 12.2
  if (phase < ConnectedPhase) {
    routeSet.RemoveAll();
    routeSet = originalInvite->GetMIME().GetRecordRoute();
    PString originalContact = originalInvite->GetMIME().GetContact();
    if (!originalContact.IsEmpty()) {
      targetAddress = originalContact;
    }
  }

  // send the 200 OK response
  SendInviteOK(sdpOut);

  // switch phase 
  SetPhase(ConnectedPhase);
  connectedTime = PTime ();
  
  return TRUE;
}


RTP_UDP *SIPConnection::OnUseRTPSession(const unsigned rtpSessionId, const OpalTransportAddress & mediaAddress, OpalTransportAddress & localAddress)
{
  RTP_UDP *rtpSession = NULL;
  RTP_DataFrame::PayloadTypes ntePayloadCode = RTP_DataFrame::IllegalPayloadType;

  {
    PSafeLockReadOnly m(ownerCall);
    PSafePtr<OpalConnection> otherParty = GetCall().GetOtherPartyConnection(*this);
    if (otherParty == NULL) {
      PTRACE(2, "H323\tCorwardly fefusing to create an RTP channel with only one connection");
      return NULL;
     }

    // if doing media bypass, we need to set the local address
    // otherwise create an RTP session
    if (ownerCall.IsMediaBypassPossible(*this, rtpSessionId)) {
      OpalConnection * otherParty = GetCall().GetOtherPartyConnection(*this);
      if (otherParty != NULL) {
        MediaInformation info;
        if (otherParty->GetMediaInformation(rtpSessionId, info)) {
          localAddress = info.data;
          ntePayloadCode = info.rfc2833;
        }
      }
      mediaTransportAddresses.SetAt(rtpSessionId, new OpalTransportAddress(mediaAddress));
    }
    else {
      // create an RTP session
      rtpSession = (RTP_UDP *)UseSession(GetTransport(), rtpSessionId, NULL);
      if (rtpSession == NULL) {
        return NULL;
      }
      
      // Set user data
      if (rtpSession->GetUserData() == NULL)
        rtpSession->SetUserData(new SIP_RTP_Session(*this));
  
      // Local Address of the session
      localAddress = GetLocalAddress(rtpSession->GetLocalDataPort());
    }
  }
  
  return rtpSession;
}


BOOL SIPConnection::OnSendSDPMediaDescription(const SDPSessionDescription & sdpIn,
                                              SDPMediaDescription::MediaType rtpMediaType,
                                              unsigned rtpSessionId,
                                              SDPSessionDescription & sdpOut)
{
  RTP_UDP * rtpSession = NULL;

  // if no matching media type, return FALSE
  SDPMediaDescription * incomingMedia = sdpIn.GetMediaDescription(rtpMediaType);
  if (incomingMedia == NULL) {
    PTRACE(2, "SIP\tCould not find matching media type for session " << rtpSessionId);
    return FALSE;
  }
  
  // Create the list of Opal format names for the remote end.
  // We will answer with the media format that will be opened.
  // When sending an answer SDP, remove media formats that we do not support.
  remoteFormatList += incomingMedia->GetMediaFormats(rtpSessionId);
  remoteFormatList.Remove(endpoint.GetManager().GetMediaFormatMask());
  if (remoteFormatList.GetSize() == 0) {
    Release(EndedByCapabilityExchange);
    return FALSE;
  }

  SDPMediaDescription * localMedia = NULL;
  OpalTransportAddress mediaAddress = incomingMedia->GetTransportAddress();

  if (rtpMediaType != SDPMediaDescription::Image) {
    // find the payload type used for telephone-event, if present
    const SDPMediaFormatList & sdpMediaList = incomingMedia->GetSDPMediaFormats();
    BOOL hasTelephoneEvent = FALSE;
    PINDEX i;
    for (i = 0; !hasTelephoneEvent && (i < sdpMediaList.GetSize()); i++) {
      if (sdpMediaList[i].GetEncodingName() == "telephone-event") {
        rfc2833Handler->SetPayloadType(sdpMediaList[i].GetPayloadType());
        hasTelephoneEvent = TRUE;
        remoteFormatList += OpalRFC2833;
      }
    }

    // Create the RTPSession if required
    OpalTransportAddress localAddress;
    rtpSession = OnUseRTPSession(rtpSessionId, mediaAddress, localAddress);
    if (rtpSession == NULL && !ownerCall.IsMediaBypassPossible(*this, rtpSessionId)) {
      Release(EndedByTransportFail);
      return FALSE;
    }

    // construct a new media session list 
    localMedia = new SDPMediaDescription(localAddress, rtpMediaType);
  
    // create map for RTP payloads
    incomingMedia->CreateRTPMap(rtpSessionId, rtpPayloadMap);

    // Open the streams and the reverse media streams
    BOOL reverseStreamsFailed = TRUE;
    reverseStreamsFailed = OnOpenSourceMediaStreams(remoteFormatList, rtpSessionId, localMedia);
  
    // Add in the RFC2833 handler, if used
    if (hasTelephoneEvent) {
      localMedia->AddSDPMediaFormat(new SDPMediaFormat("0-15", rfc2833Handler->GetPayloadType()));
    }

  // set the remote address after the stream is opened
    PIPSocket::Address ip;
    WORD port;
    mediaAddress.GetIpAndPort(ip, port);
    if (rtpSession && !rtpSession->SetRemoteSocketInfo(ip, port, TRUE)) {
      PTRACE(1, "SIP\tCannot set remote ports on RTP session");
      Release(EndedByTransportFail);
      delete localMedia;
      return FALSE;
    }
  
    // No stream opened for this session, use the default SDP
    if (reverseStreamsFailed) {
      SDPSessionDescription *sdp = (SDPSessionDescription *) &sdpOut;
      if (!BuildSDP(sdp, rtpSessions, rtpSessionId)) {
        delete localMedia;
        return FALSE;
      }
      return TRUE;
    }
  }
  else
  {
    PIPSocket::Address Ip;
    WORD Port;

    mediaAddress.GetIpAndPort(Ip, Port);
    OpalTransportAddress RemoteAddress(Ip, Port, "udp$");

    OpalTransportUDP& Transport = GetUDPTransport();
    Transport.SetRemoteAddress(RemoteAddress);
    Transport.Connect();

    localMedia = new SDPMediaDescription(Transport.GetLocalAddress(), rtpMediaType);

    OnOpenSourceMediaStreams(remoteFormatList, rtpSessionId, localMedia);
  }

  localMedia->SetDirection(GetDirection(rtpSessionId));
  sdpOut.AddMediaDescription(localMedia);

  return TRUE;
}


BOOL SIPConnection::OnOpenSourceMediaStreams(const OpalMediaFormatList & remoteFormatList, unsigned sessionId, SDPMediaDescription *localMedia)
{
  BOOL reverseStreamsFailed = TRUE;

  {
    PWaitAndSignal m(streamsMutex);
    ownerCall.OpenSourceMediaStreams(*this, remoteFormatList, sessionId);
  }

  OpalMediaFormatList otherList;
  {
    PSafePtr<OpalConnection> otherParty = GetCall().GetOtherPartyConnection(*this);
    if (otherParty == NULL) {
      PTRACE(1, "SIP\tCannot get other connection");
      return FALSE;
    }
    otherList = otherParty->GetMediaFormats();
  }

  for (PINDEX i = 0; i < mediaStreams.GetSize(); i++) {
    OpalMediaStream & mediaStream = mediaStreams[i];
    if (mediaStream.GetSessionID() == sessionId) {
      if (OpenSourceMediaStream(otherList, sessionId) && localMedia) {
        localMedia->AddMediaFormat(mediaStream.GetMediaFormat(), rtpPayloadMap);
        reverseStreamsFailed = FALSE;
      }
    }
  }

  return reverseStreamsFailed;
}


SDPMediaDescription::Direction SIPConnection::GetDirection(unsigned sessionId)
{
  if (remote_hold)
    return SDPMediaDescription::RecvOnly;
  else if (local_hold)
    return SDPMediaDescription::SendOnly;
#if OPAL_VIDEO
  else if (sessionId == OpalMediaFormat::DefaultVideoSessionID) {
    if (endpoint.GetManager().CanAutoStartTransmitVideo() && !endpoint.GetManager().CanAutoStartReceiveVideo())
      return SDPMediaDescription::SendOnly;
    else if (!endpoint.GetManager().CanAutoStartTransmitVideo() && endpoint.GetManager().CanAutoStartReceiveVideo())
      return SDPMediaDescription::RecvOnly;
  }
#endif
  
  return SDPMediaDescription::Undefined;
}


OpalTransportAddress SIPConnection::GetLocalAddress(WORD port) const
{
  PIPSocket::Address localIP;
  if (!transport->GetLocalAddress().GetIpAddress(localIP)) {
    PTRACE(1, "SIP\tNot using IP transport");
    return OpalTransportAddress();
  }

  PIPSocket::Address remoteIP;
  if (!transport->GetRemoteAddress().GetIpAddress(remoteIP)) {
    PTRACE(1, "SIP\tNot using IP transport");
    return OpalTransportAddress();
  }

  endpoint.GetManager().TranslateIPAddress(localIP, remoteIP);
  return OpalTransportAddress(localIP, port, "udp");
}


OpalMediaFormatList SIPConnection::GetMediaFormats() const
{
  return remoteFormatList;
}


BOOL SIPConnection::OpenSourceMediaStream(const OpalMediaFormatList & mediaFormats,
                                          unsigned sessionID)
{
  // The remote user is in recvonly mode or in inactive mode for that session
  switch (remoteSDP.GetDirection(sessionID)) {
    case SDPMediaDescription::Inactive :
    case SDPMediaDescription::RecvOnly :
      return FALSE;

    default :
      return OpalConnection::OpenSourceMediaStream(mediaFormats, sessionID);
  }
}


OpalMediaStream * SIPConnection::OpenSinkMediaStream(OpalMediaStream & source)
{
  // The remote user is in sendonly mode or in inactive mode for that session
  switch (remoteSDP.GetDirection(source.GetSessionID())) {
    case SDPMediaDescription::Inactive :
    case SDPMediaDescription::SendOnly :
      return NULL;

    default :
      return OpalConnection::OpenSinkMediaStream(source);
  }
}


OpalMediaStream * SIPConnection::CreateMediaStream(const OpalMediaFormat & mediaFormat,
                                                   unsigned sessionID,
                                                   BOOL isSource)
{
  if (ownerCall.IsMediaBypassPossible(*this, sessionID))
    return new OpalNullMediaStream(mediaFormat, sessionID, isSource);

  if (rtpSessions.GetSession(sessionID) == NULL)
    return NULL;

  return new OpalRTPMediaStream(mediaFormat, isSource, *rtpSessions.GetSession(sessionID),
                                endpoint.GetManager().GetMinAudioJitterDelay(),
                                endpoint.GetManager().GetMaxAudioJitterDelay());
}

void SIPConnection::OnPatchMediaStream(BOOL isSource, OpalMediaPatch & patch)
{
  OpalConnection::OnPatchMediaStream(isSource, patch);
  if(patch.GetSource().GetSessionID() == OpalMediaFormat::DefaultAudioSessionID) {
    AttachRFC2833HandlerToPatch(isSource, patch);
  }
}


void SIPConnection::OnConnected ()
{
  OpalConnection::OnConnected ();
}


BOOL SIPConnection::IsMediaBypassPossible(unsigned sessionID) const
{
  PTRACE(3, "SIP\tIsMediaBypassPossible: session " << sessionID);

  return sessionID == OpalMediaFormat::DefaultAudioSessionID ||
         sessionID == OpalMediaFormat::DefaultVideoSessionID;
}


BOOL SIPConnection::WriteINVITE(OpalTransport & transport, void * param)
{
  SIPConnection & connection = *(SIPConnection *)param;

  connection.SetLocalPartyAddress();

  SIPTransaction * invite = new SIPInvite(connection, transport);
  
  // It may happen that constructing the INVITE causes the connection
  // to be released (e.g. there are no UDP ports available for the RTP sessions)
  // Since the connection is released immediately, a INVITE must not be
  // sent out.
  if (connection.GetPhase() >= OpalConnection::ReleasingPhase) {
    PTRACE(2, "SIP\tAborting INVITE transaction since connection is in releasing phase");
    delete invite; // at this point, the INVITE is not yet added to the transactions
    return FALSE;
  }
  
  if (invite->Start()) {
    PWaitAndSignal m(connection.invitationsMutex); 
    connection.invitations.Append(invite);
    return TRUE;
  }

  PTRACE(2, "SIP\tDid not start INVITE transaction on " << transport);
  return FALSE;
}


BOOL SIPConnection::SetUpConnection()
{
  SIPURL transportAddress = targetAddress;

  PTRACE(2, "SIP\tSetUpConnection: " << remotePartyAddress);

  // Do a DNS SRV lookup
#if P_DNS
    PIPSocketAddressAndPortVector addrs;
    if (PDNS::LookupSRV(targetAddress.GetHostName(), "_sip._udp", targetAddress.GetPort(), addrs)) {
      transportAddress.SetHostName(addrs[0].address.AsString());
      transportAddress.SetPort(addrs [0].port);
    }
#endif
  PStringList routeSet = GetRouteSet();
  if (!routeSet.IsEmpty()) 
    transportAddress = routeSet[0];

  originating = TRUE;

  delete transport;
  transport = endpoint.CreateTransport(transportAddress.GetHostAddress());
  lastTransportAddress = transportAddress.GetHostAddress();
  if (transport == NULL) {
    Release(EndedByTransportFail);
    return FALSE;
  }

  if (!transport->WriteConnect(WriteINVITE, this)) {
    PTRACE(1, "SIP\tCould not write to " << targetAddress << " - " << transport->GetErrorText());
    Release(EndedByTransportFail);
    return FALSE;
  }

  releaseMethod = ReleaseWithCANCEL;
  return TRUE;
}


void SIPConnection::HoldConnection()
{
  if (local_hold)
    return;
  else
    local_hold = TRUE;

  if (transport == NULL)
    return;

  PTRACE(2, "SIP\tWill put connection on hold");

  SIPTransaction * invite = new SIPInvite(*this, *transport, rtpSessions);
  if (invite->Start()) {
    {
      PWaitAndSignal m(invitationsMutex); 
      invitations.Append(invite);
    }
    // Pause the media streams
    PauseMediaStreams(TRUE);
    
    // Signal the manager that there is a hold
    endpoint.OnHold(*this);
  }
}


void SIPConnection::RetrieveConnection()
{
  if (!local_hold)
    return;
  else
    local_hold = FALSE;

  if (transport == NULL)
    return;

  PTRACE(2, "SIP\tWill retrieve connection from hold");

  SIPTransaction * invite = new SIPInvite(*this, *transport, rtpSessions);
  if (invite->Start()) {
    {
      PWaitAndSignal m(invitationsMutex); 
      invitations.Append(invite);
    }
    
    // Un-Pause the media streams
    PauseMediaStreams(FALSE);

    // Signal the manager that there is a hold
    endpoint.OnHold(*this);
  }
}


BOOL SIPConnection::IsConnectionOnHold()
{
  return (local_hold || remote_hold);
}


BOOL SIPConnection::BuildSDP(SDPSessionDescription * & sdp, 
                             RTP_SessionManager & rtpSessions,
                             unsigned rtpSessionId)
{
  OpalTransportAddress localAddress;
  RTP_DataFrame::PayloadTypes ntePayloadCode = RTP_DataFrame::IllegalPayloadType;

#if OPAL_VIDEO
  if (rtpSessionId == OpalMediaFormat::DefaultVideoSessionID && !endpoint.GetManager().CanAutoStartReceiveVideo() && !endpoint.GetManager().CanAutoStartTransmitVideo())
    return FALSE;
#endif

  if (ownerCall.IsMediaBypassPossible(*this, rtpSessionId)) {
    OpalConnection * otherParty = GetCall().GetOtherPartyConnection(*this);
    if (otherParty != NULL) {
      MediaInformation info;
      if (otherParty->GetMediaInformation(rtpSessionId, info)) {
        localAddress = info.data;
        ntePayloadCode = info.rfc2833;
      }
    }
  }

  if (localAddress.IsEmpty() && (rtpSessionId != OpalMediaFormat::DefaultDataSessionID)) {
    /* We are not doing media bypass, so must have an RTP session.
       Due to the possibility of several INVITEs going out, all with different
       transport requirements, we actually need to use an rtpSession dictionary
       for each INVITE and not the one for the connection. Once an INVITE is
       accepted the rtpSessions for that INVITE is put into the connection. */
    RTP_Session * rtpSession = rtpSessions.UseSession(rtpSessionId);
    if (rtpSession == NULL) {

      // Not already there, so create one
      rtpSession = CreateSession(GetTransport(), rtpSessionId, NULL);
      if (rtpSession == NULL) {
        Release(OpalConnection::EndedByTransportFail);
        return FALSE;
      }

      rtpSession->SetUserData(new SIP_RTP_Session(*this));

      // add the RTP session to the RTP session manager in INVITE
      rtpSessions.AddSession(rtpSession);
    }

    localAddress = GetLocalAddress(((RTP_UDP *)rtpSession)->GetLocalDataPort());
  } 

  if (sdp == NULL)
    sdp = new SDPSessionDescription(localAddress);

  SDPMediaDescription * localMedia = NULL;
  switch (rtpSessionId) {
    case OpalMediaFormat::DefaultAudioSessionID:
      localMedia = new SDPMediaDescription(localAddress, SDPMediaDescription::Audio);
      break;

#if OPAL_VIDEO
    case OpalMediaFormat::DefaultVideoSessionID:
      localMedia = new SDPMediaDescription(localAddress, SDPMediaDescription::Video);
      break;
#endif

    case OpalMediaFormat::DefaultDataSessionID:
    default:
      localMedia = new SDPMediaDescription(localAddress, SDPMediaDescription::Image);
      break;
  }

  // add media formats first, as Mediatrix gateways barf if RFC2833 is first
  OpalMediaFormatList formats = ownerCall.GetMediaFormats(*this, FALSE);
  AdjustMediaFormats(formats);
  localMedia->AddMediaFormats(formats, rtpSessionId, rtpPayloadMap);

  // Set format if we have an RTP payload type for RFC2833
  if (rtpSessionId == OpalMediaFormat::DefaultAudioSessionID) {

    if (ntePayloadCode != RTP_DataFrame::IllegalPayloadType) {
      PTRACE(3, "SIP\tUsing bypass RTP payload " << ntePayloadCode << " for NTE");
      localMedia->AddSDPMediaFormat(new SDPMediaFormat("0-15", ntePayloadCode));
    }
    else {
      ntePayloadCode = rfc2833Handler->GetPayloadType();
      if (ntePayloadCode == RTP_DataFrame::IllegalPayloadType) {
        ntePayloadCode = OpalRFC2833.GetPayloadType();
      }

      if (ntePayloadCode != RTP_DataFrame::IllegalPayloadType) {
        PTRACE(3, "SIP\tUsing RTP payload " << ntePayloadCode << " for NTE");

        // create and add the NTE media format
        localMedia->AddSDPMediaFormat(new SDPMediaFormat("0-15", ntePayloadCode));
      }
      else {
        PTRACE(2, "SIP\tCould not allocate dynamic RTP payload for NTE");
      }
    }

    rfc2833Handler->SetPayloadType(ntePayloadCode);
  }
  
  localMedia->SetDirection(GetDirection(rtpSessionId));
  sdp->AddMediaDescription(localMedia);
  return TRUE;
}


void SIPConnection::SetLocalPartyAddress()
{
  OpalTransportAddress taddr = transport->GetLocalAddress();
  PIPSocket::Address addr; 
  WORD port;
  taddr.GetIpAndPort(addr, port);
  PString displayName = endpoint.GetDefaultDisplayName();
  PString localName = endpoint.GetRegisteredPartyName(SIPURL(remotePartyAddress).GetHostName()).GetUserName(); 
  PString domain = endpoint.GetRegisteredPartyName(SIPURL(remotePartyAddress).GetHostName()).GetHostName();

  // If no domain, use the local domain as default
  if (domain.IsEmpty()) {

    domain = addr.AsString();
    if (port != endpoint.GetDefaultSignalPort())
      domain += psprintf(":%d", port);
  }

  if (!localName.IsEmpty())
    SetLocalPartyName (localName);

  SIPURL myAddress("\"" + displayName + "\" <" + GetLocalPartyName() + "@" + domain + ">"); 

  // add displayname, <> and tag
  SetLocalPartyAddress(myAddress.AsQuotedString() + ";tag=" + GetTag());
}


const PString SIPConnection::GetRemotePartyCallbackURL() const
{
  SIPURL url = GetRemotePartyAddress();
  url.AdjustForRequestURI();
  return url.AsString();
}


void SIPConnection::OnTransactionFailed(SIPTransaction & transaction)
{
  if (transaction.GetMethod() != SIP_PDU::Method_INVITE)
    return;

  // If we are releasing then I can safely ignore failed
  // transactions - otherwise I'll deadlock.
  if (phase >= ReleasingPhase)
    return;

  {
    PWaitAndSignal m(invitationsMutex); 
    for (PINDEX i = 0; i < invitations.GetSize(); i++) {
      if (!invitations[i].IsFailed())
        return;
    }
  }

  // All invitations failed, die now
  Release(EndedByConnectFail);
}


void SIPConnection::OnReceivedPDU(SIP_PDU & pdu)
{
  PTRACE(4, "SIP\tHandling PDU " << pdu);

  switch (pdu.GetMethod()) {
    case SIP_PDU::Method_INVITE :
      OnReceivedINVITE(pdu);
      break;
    case SIP_PDU::Method_ACK :
      OnReceivedACK(pdu);
      break;
    case SIP_PDU::Method_CANCEL :
      OnReceivedCANCEL(pdu);
      break;
    case SIP_PDU::Method_BYE :
      OnReceivedBYE(pdu);
      break;
    case SIP_PDU::Method_OPTIONS :
      OnReceivedOPTIONS(pdu);
      break;
    case SIP_PDU::Method_NOTIFY :
      OnReceivedNOTIFY(pdu);
      break;
    case SIP_PDU::Method_REFER :
      OnReceivedREFER(pdu);
      break;
    case SIP_PDU::Method_INFO :
      OnReceivedINFO(pdu);
      break;
    case SIP_PDU::Method_PING :
      OnReceivedPING(pdu);
      break;
    case SIP_PDU::Method_MESSAGE :
    case SIP_PDU::Method_SUBSCRIBE :
    case SIP_PDU::Method_REGISTER :
      // Shouldn't have got this!
      break;
    case SIP_PDU::NumMethods :  // if no method, must be response
      {
        PWaitAndSignal m(transactionsMutex);
        SIPTransaction * transaction = transactions.GetAt(pdu.GetTransactionID());
        if (transaction != NULL)
          transaction->OnReceivedResponse(pdu);
      }
      break;
  }
}

//
// This table comes from RFC 3398 para 8.2.6.1
//
static struct SIPCodeToQ931ReasonMapEntry {
  unsigned  sipCode;
  unsigned  q931Cause;
} SIPCodeToQ931Reason[] = {
  { 400 /* Bad Request                    */, 41 }, // Temporary Failure
  { 401 /* Unauthorized                   */,   21 }, // Call rejected (*)
  { 402 /* Payment required               */,   21 }, // Call rejected
  { 403 /* Forbidden                      */,   21 }, // Call rejected
  { 404 /* Not found                      */,    1 }, // Unallocated number
  { 405 /* Method not allowed             */,   63 }, // Service or option unavailable
  { 406 /* Not acceptable                 */,   79 }, // Service/option not implemented (+)
  { 407 /* Proxy authentication required  */,   21 }, // Call rejected (*)
  { 408 /* Request timeout                */,  102 }, // Recovery on timer expiry
  { 410 /* Gone                           */,   22 }, // Number changed (w/o diagnostic)
  { 413 /* Request Entity too long        */,  127 }, // Interworking (+)
  { 414 /* Request-URI too long           */,  127 }, // Interworking (+)
  { 415 /* Unsupported media type         */,   79 }, // Service/option not implemented (+)
  { 416 /* Unsupported URI Scheme         */,  127 }, // Interworking (+)
  { 420 /* Bad extension                  */,  127 }, // Interworking (+)
  { 421 /* Extension Required             */,  127 }, // Interworking (+)
  { 423 /* Interval Too Brief             */,  127 }, // Interworking (+)
  { 480 /* Temporarily unavailable        */,   18 }, // No user responding
  { 481 /* Call/Transaction Does not Exist*/,   41 }, // Temporary Failure
  { 482 /* Loop Detected                  */,   25 }, // Exchange - routing error
  { 483 /* Too many hops                  */,   25 }, // Exchange - routing error
  { 484 /* Address incomplete             */,   28 }, // Invalid Number Format (+)
  { 485 /* Ambiguous                      */,    1 }, // Unallocated number
  { 486 /* Busy here                      */,   17 }, // User busy
//  { 487 /* Request Terminated             */,  --- }, // (no mapping)
  //{ 488 /* Not Acceptable here            */,  --- }, // by Warning header
  { 500 /* Server internal error          */,   41 }, // Temporary failure
  { 501 /* Not implemented                */,   79 }, // Not implemented, unspecified
  { 502 /* Bad gateway                    */,   38 }, // Network out of order
  { 503 /* Service unavailable            */,   41 }, // Temporary failure
  { 504 /* Server time-out                */,  102 }, // Recovery on timer expiry
  { 504 /* Version Not Supported          */,  127 }, // Interworking (+)
  { 513 /* Message Too Large              */,  127 }, // Interworking (+)
  { 600 /* Busy everywhere                */,   17 }, // User busy
  { 603 /* Decline                        */,   21 }, // Call rejected
  { 604 /* Does not exist anywhere        */,    1 }, // Unallocated number
  //{ 606 /* Not acceptable                 */,  --- }, // by Warning header
};

#define NUM_SIPCODE_END_REASON_MAP_ENTRIES (sizeof(SIPCodeToQ931Reason) / sizeof(SIPCodeToQ931Reason[0]))

static unsigned RFC3398_MapSIPCodeToQ931(unsigned sipCode)
{
  unsigned int i;
  for (i = 0; i < NUM_SIPCODE_END_REASON_MAP_ENTRIES; ++i)
    if (SIPCodeToQ931Reason[i].sipCode == sipCode)
      return SIPCodeToQ931Reason[i].q931Cause;

  return 41; // Temporary Failure
}

void SIPConnection::OnReceivedResponse(SIPTransaction & transaction, SIP_PDU & response)
{
  PINDEX i;

  if (transaction.GetMethod() == SIP_PDU::Method_INVITE) {
    if (phase < EstablishedPhase) {
      // Have a response to the INVITE, so CANCEL all the other invitations sent.
      PWaitAndSignal m(invitationsMutex); 
      for (i = 0; i < invitations.GetSize(); i++) {
        if (&invitations[i] != &transaction)
          invitations[i].SendCANCEL();
      }
    }

    // Save the sessions etc we are actually using
    // If we are in the EstablishedPhase, then the 
    // sessions are kept identical because the response is the
    // response to a hold/retrieve
    if (phase != EstablishedPhase)
      rtpSessions = ((SIPInvite &)transaction).GetSessionManager();
    localPartyAddress = transaction.GetMIME().GetFrom();
    remotePartyAddress = response.GetMIME().GetTo();
    SIPURL url(remotePartyAddress);
    remotePartyName = url.GetDisplayName ();
    remoteApplication = response.GetMIME().GetUserAgent ();
    remoteApplication.Replace ('/', '\t'); 

    // get the route set from the Record-Route response field (in reverse order)
    // according to 12.1.2
    // requests in a dialog do not modify the initial route set fo according 
    // to 12.2
    if (phase < ConnectedPhase) {
      PStringList recordRoute = response.GetMIME().GetRecordRoute();
      routeSet.RemoveAll();
      for (i = recordRoute.GetSize(); i > 0; i--)
        routeSet.AppendString(recordRoute[i-1]);
    }

    // If we are in a dialog or create one, then targetAddress needs to be set
    // to the contact field in the 2xx/1xx response for a target refresh 
    // request
    if (response.GetStatusCode()/100 == 2 || response.GetStatusCode()/100 == 1) {
      PString contact = response.GetMIME().GetContact();
      if (!contact.IsEmpty()) {
        targetAddress = contact;
        PTRACE(4, "SIP\tSet targetAddress to " << targetAddress);
      }
    }

    // Send the ack
    if (response.GetStatusCode()/100 != 1) {
      SendACK(transaction, response);
    }
  }

  switch (response.GetStatusCode()) {
    case SIP_PDU::Information_Trying :
      OnReceivedTrying(response);
      break;

    case SIP_PDU::Information_Ringing :
      OnReceivedRinging(response);
      break;

    case SIP_PDU::Information_Session_Progress :
      OnReceivedSessionProgress(response);
      break;

    case SIP_PDU::Failure_UnAuthorised :
    case SIP_PDU::Failure_ProxyAuthenticationRequired :
      OnReceivedAuthenticationRequired(transaction, response);
      break;

    case SIP_PDU::Redirection_MovedTemporarily :
      OnReceivedRedirection(response);
      break;

/////////////////////

    // for each of the following cases, map the SIP release code to the correct Q931 code as per RFC 3398

    case SIP_PDU::Failure_NotFound :
      Release(EndedByNoUser);
      q931Cause = RFC3398_MapSIPCodeToQ931(SIP_PDU::Failure_NotFound);
      break;

    case SIP_PDU::Failure_RequestTimeout :
      Release(EndedByTemporaryFailure);
      q931Cause = RFC3398_MapSIPCodeToQ931(SIP_PDU::Failure_RequestTimeout);
      break;

    case SIP_PDU::Failure_TemporarilyUnavailable :
      Release(EndedByTemporaryFailure);
      q931Cause = RFC3398_MapSIPCodeToQ931(SIP_PDU::Failure_TemporarilyUnavailable);
      break;
      
    case SIP_PDU::Failure_Forbidden :
      Release(EndedBySecurityDenial);
      q931Cause = RFC3398_MapSIPCodeToQ931(SIP_PDU::Failure_Forbidden);
      break;

    case SIP_PDU::Failure_BusyHere :
      Release(EndedByRemoteBusy);
      q931Cause = RFC3398_MapSIPCodeToQ931(SIP_PDU::Failure_BusyHere);
      break;

    case SIP_PDU::Failure_UnsupportedMediaType :
      Release(EndedByCapabilityExchange);
      q931Cause = RFC3398_MapSIPCodeToQ931(SIP_PDU::Failure_UnsupportedMediaType);
      break;

///////////////

    default :
      switch (response.GetStatusCode()/100) {
        case 1 :
          // old: Do nothing on 1xx
          PTRACE(2, "SIP\tReceived unknown informational response " << response.GetStatusCode());
          break;
        case 2 :
          OnReceivedOK(transaction, response);
          break;
        default :
          // All other final responses cause a call end, if it is not a
          // local hold.
          if (!local_hold) {
            Release(EndedByRefusal);
            q931Cause = RFC3398_MapSIPCodeToQ931(response.GetStatusCode());
          }
          else {
            local_hold = FALSE; // It failed
            // Un-Pause the media streams
            PauseMediaStreams(FALSE);
            // Signal the manager that there is no more hold
            endpoint.OnHold(*this);
          }
      }
  }
}


void SIPConnection::OnReceivedINVITE(SIP_PDU & request)
{
  BOOL isReinvite = FALSE;
  BOOL sdpFailure = TRUE;
 
  if (originalInvite != NULL) {

    // Ignore duplicate INVITEs
    if (originalInvite->GetMIME().GetCSeq() == request.GetMIME().GetCSeq()) {
      PTRACE(2, "SIP\tIgnoring duplicate INVITE from " << request.GetURI());
      return;
    }
  }

  // Is Re-INVITE?
  if (phase == EstablishedPhase 
      && ((!IsOriginating() && originalInvite != NULL)
         || (IsOriginating()))) {
    PTRACE(2, "SIP\tReceived re-INVITE from " << request.GetURI() << " for " << *this);
    isReinvite = TRUE;
  }
  else
  {
    // get the remote transport address
    PURL contact(originalInvite->GetMIME().GetContact());
    OpalTransportAddress sourceAddress(contact.GetHostName());

    //
    // two condition for detecting remote is behind a NAT
    //   1. the signalling address is not private, but the TCP source address is
    //   2. the signalling and TCP source address are both private, but not the same
    //
    // but don't enable NAT usage if local endpoint is configured to be behind a NAT
    //
    PIPSocket::Address srcAddr, sigAddr;
    sourceAddress.GetIpAddress(srcAddr);
    transport->GetRemoteAddress().GetIpAddress(sigAddr);
    if (
        (!sigAddr.IsRFC1918() && srcAddr.IsRFC1918()) ||                         
        ((sigAddr.IsRFC1918() && srcAddr.IsRFC1918()) && (sigAddr != srcAddr))
        )  
    {
      PIPSocket::Address localAddress;
      transport->GetLocalAddress().GetIpAddress(localAddress);

      PIPSocket::Address ourAddress = localAddress;
      endpoint.GetManager().TranslateIPAddress(localAddress, sigAddr);

      if (localAddress != ourAddress) {
        PTRACE(3, "SIP\tSource signal address " << srcAddr << " and peer address " << sigAddr << " indicate remote endpoint is behind NAT");
        remoteIsNAT = TRUE;
      }
    }
  }

  // originalInvite should contain the first received INVITE for
  // this connection
  if (originalInvite)
    delete originalInvite;

  originalInvite = new SIP_PDU(request);
  // Special case auto calling
  if (!isReinvite && IsOriginating() && invitations.GetSize() > 0 && invitations[0].GetMIME().GetCallID() == request.GetMIME().GetCallID()) {
    SendInviteResponse(SIP_PDU::Failure_InternalServerError);
    return;
  }

  if (request.HasSDP())
    remoteSDP = request.GetSDP();
  if (!isReinvite)
    releaseMethod = ReleaseWithResponse;

  SIPMIMEInfo & mime = originalInvite->GetMIME();
  remotePartyAddress = mime.GetFrom(); 
  SIPURL url(remotePartyAddress);
  remotePartyName = url.GetDisplayName ();
  remoteApplication = mime.GetUserAgent ();
  remoteApplication.Replace ('/', '\t'); 
  localPartyAddress  = mime.GetTo() + ";tag=" + GetTag(); // put a real random 
  mime.SetTo(localPartyAddress);

  // get the called destination
  calledDestinationName   = originalInvite->GetURI().GetDisplayName();
  calledDestinationNumber = originalInvite->GetURI().GetUserName();

  // update the target address
  PString contact = mime.GetContact();
  if (!contact.IsEmpty()) 
    targetAddress = contact;
  targetAddress.AdjustForRequestURI();
  PTRACE(4, "SIP\tSet targetAddress to " << targetAddress);
  
  // send trying with To: tag
  SendInviteResponse(SIP_PDU::Information_Trying);

  // We received a Re-INVITE for a current connection
  if (isReinvite) {

    remoteFormatList.RemoveAll();
    SDPSessionDescription sdpOut(GetLocalAddress());

    // get the remote media formats, if any
    if (originalInvite->HasSDP()) {

      SDPSessionDescription & sdpIn = originalInvite->GetSDP();
      // The Re-INVITE can be sent to change the RTP Session parameters,
      // the current codecs, or to put the call on hold
      if (sdpIn.GetDirection(OpalMediaFormat::DefaultAudioSessionID) == SDPMediaDescription::SendOnly && sdpIn.GetDirection(OpalMediaFormat::DefaultVideoSessionID) == SDPMediaDescription::SendOnly) {

        PTRACE(3, "SIP\tRemote hold detected");
        remote_hold = TRUE;
        PauseMediaStreams(TRUE);
        endpoint.OnHold(*this);
      }
      else {

        // If we receive a consecutive reinvite without the SendOnly
        // parameter, then we are not on hold anymore
        if (remote_hold) {
          PTRACE(3, "SIP\tRemote retrieve from hold detected");
          remote_hold = FALSE;
          PauseMediaStreams(FALSE);
          endpoint.OnHold(*this);
        }
      }
    }
    else {
      if (remote_hold) {
        PTRACE(3, "SIP\tRemote retrieve from hold without SDP detected");
        remote_hold = FALSE;
        PauseMediaStreams(FALSE);
        endpoint.OnHold(*this);
      }
    }
    
    // If it is a RE-INVITE that doesn't correspond to a HOLD, then
    // Close all media streams, they will be reopened.
    if (!IsConnectionOnHold()) {
      PWaitAndSignal m(streamsMutex);
      GetCall().RemoveMediaStreams();
      ReleaseSession(OpalMediaFormat::DefaultAudioSessionID, TRUE);
      ReleaseSession(OpalMediaFormat::DefaultVideoSessionID, TRUE);
    }
 
    if (originalInvite->HasSDP()) {
      // Try to send SDP media description for audio and video
      SDPSessionDescription & sdpIn = originalInvite->GetSDP();
      sdpFailure = !OnSendSDPMediaDescription(sdpIn, SDPMediaDescription::Audio, OpalMediaFormat::DefaultAudioSessionID, sdpOut);
      sdpFailure = !OnSendSDPMediaDescription(sdpIn, SDPMediaDescription::Video, OpalMediaFormat::DefaultVideoSessionID, sdpOut) && sdpFailure;
      sdpFailure = !OnSendSDPMediaDescription(sdpIn, SDPMediaDescription::Image, OpalMediaFormat::DefaultDataSessionID, sdpOut) && sdpFailure;
    }

    if (sdpFailure) {
      SDPSessionDescription *sdp = (SDPSessionDescription *) &sdpOut;
      sdpFailure = !BuildSDP(sdp, rtpSessions, OpalMediaFormat::DefaultAudioSessionID);
      sdpFailure = !BuildSDP(sdp, rtpSessions, OpalMediaFormat::DefaultVideoSessionID) && sdpFailure;
      sdpFailure = !BuildSDP(sdp, rtpSessions, OpalMediaFormat::DefaultDataSessionID) && sdpFailure;
      if (sdpFailure) {
        // Ignore a failed reInvite
        return;
      }
    }

  
    // send the 200 OK response
    SendInviteOK(sdpOut);

    return;
  }
  
  
  // indicate the other is to start ringing (but look out for clear calls)
  if (!OnIncomingConnection()) {
    PTRACE(2, "SIP\tOnIncomingConnection failed for INVITE from " << request.GetURI() << " for " << *this);
    Release();
    return;
  }

  PTRACE(2, "SIP\tOnIncomingConnection succeeded for INVITE from " << request.GetURI() << " for " << *this);
  SetPhase(SetUpPhase);

  ownerCall.OnSetUp(*this);

  AnsweringCall(OnAnswerCall(remotePartyAddress));
}


OpalConnection::AnswerCallResponse SIPConnection::OnAnswerCall(
      const PString & callerName      /// Name of caller
)
{
  return OpalConnection::OnAnswerCall(callerName);
}


void SIPConnection::AnsweringCall(AnswerCallResponse response)
{
  switch (phase) {
    case SetUpPhase:
    case AlertingPhase:
      switch (response) {
        case AnswerCallNow:
        case AnswerCallProgress:
          SetConnected();
          break;

        case AnswerCallDenied:
          PTRACE(1, "SIP\tApplication has declined to answer incoming call");
          Release(EndedByAnswerDenied);
          break;

        case AnswerCallPending:
          SetAlerting(localPartyName, FALSE);
          break;

        case AnswerCallAlertWithMedia:
          SetAlerting(localPartyName, TRUE);
          break;

        case AnswerCallDeferred:
        case AnswerCallDeferredWithMedia:
        case NumAnswerCallResponses:
          break;
      }
      break;

    // 
    // cannot answer call in any of the following phases
    //
    case ConnectedPhase:
    case UninitialisedPhase:
    case EstablishedPhase:
    case ReleasingPhase:
    case ReleasedPhase:
    default:
      break;
  }
}


void SIPConnection::OnReceivedACK(SIP_PDU & response)
{
  PTRACE(2, "SIP\tACK received: " << phase);
  
  OnReceivedSDP(response);

  // If we receive an ACK in established phase, perhaps it
  // is a re-INVITE
  if (phase == EstablishedPhase && !IsConnectionOnHold()) {
    OpalConnection::OnConnected ();
    StartMediaStreams();
  }
  
  // start all of the media threads for the connection
  if (phase != ConnectedPhase)  
    return;
  
  releaseMethod = ReleaseWithBYE;
  SetPhase(EstablishedPhase);
  OnEstablished();

  // HACK HACK HACK: this is a work-around for a deadlock that can occur
  // during incoming calls. What is needed is a proper resequencing that
  // avoids this problem
  StartMediaStreams();
}


void SIPConnection::OnReceivedOPTIONS(SIP_PDU & /*request*/)
{
  PTRACE(1, "SIP\tOPTIONS not yet supported");
}


void SIPConnection::OnReceivedNOTIFY(SIP_PDU & pdu)
{
  PCaselessString event, state;
  
  if (referTransaction == NULL){
    PTRACE(1, "SIP\tNOTIFY in a connection only supported for REFER requests");
    return;
  }
  
  event = pdu.GetMIME().GetEvent();
  
  // We could also compare the To and From tags
  if (pdu.GetMIME().GetCallID() != referTransaction->GetMIME().GetCallID()
      || event.Find("refer") == P_MAX_INDEX) {

    SIP_PDU response(pdu, SIP_PDU::Failure_BadEvent);
    SendPDU(response, pdu.GetViaAddress(endpoint));
    return;
  }

  state = pdu.GetMIME().GetSubscriptionState();
  
  SIP_PDU response(pdu, SIP_PDU::Successful_OK);
  SendPDU(response, pdu.GetViaAddress(endpoint));
  
  // The REFER is over
  if (state.Find("terminated") != P_MAX_INDEX) {
    referTransaction->Wait();
    delete referTransaction;
    referTransaction = NULL;

    // Release the connection
    if (phase < ReleasingPhase) 
    {
      releaseMethod = ReleaseWithBYE;
      Release(OpalConnection::EndedByCallForwarded);
    }
  }

  // The REFER is not over yet, ignore the state of the REFER for now
}


void SIPConnection::OnReceivedREFER(SIP_PDU & pdu)
{
  SIPTransaction *notifyTransaction = NULL;
  PString referto = pdu.GetMIME().GetReferTo();
  
  if (referto.IsEmpty()) {
    SIP_PDU response(pdu, SIP_PDU::Failure_BadEvent);
    SendPDU(response, pdu.GetViaAddress(endpoint));
    return;
  }    

  SIP_PDU response(pdu, SIP_PDU::Successful_Accepted);
  SendPDU(response, pdu.GetViaAddress(endpoint));
  releaseMethod = ReleaseWithNothing;

  endpoint.SetupTransfer(GetToken(),  
                         PString (), 
                         referto,  
                         NULL);
  
  // Send a Final NOTIFY,
  notifyTransaction = 
    new SIPReferNotify(*this, *transport, SIP_PDU::Successful_Accepted);
  notifyTransaction->Wait ();
  delete notifyTransaction;
}


void SIPConnection::OnReceivedBYE(SIP_PDU & request)
{
  PTRACE(2, "SIP\tBYE received for call " << request.GetMIME().GetCallID());
  SIP_PDU response(request, SIP_PDU::Successful_OK);
  SendPDU(response, request.GetViaAddress(endpoint));
  
  if (phase >= ReleasingPhase) {
    PTRACE(3, "SIP\tAlready released " << *this);
    return;
  }
  releaseMethod = ReleaseWithNothing;
  
  remotePartyAddress = request.GetMIME().GetFrom();
  SIPURL url(remotePartyAddress);
  remotePartyName = url.GetDisplayName ();
  remoteApplication = request.GetMIME ().GetUserAgent ();
  remoteApplication.Replace ('/', '\t'); 

  Release(EndedByRemoteUser);
}


void SIPConnection::OnReceivedCANCEL(SIP_PDU & request)
{
  PString origTo;
  PString origFrom;

  // Currently only handle CANCEL requests for the original INVITE that
  // created this connection, all else ignored
  // Ignore the tag added by OPAL
  if (originalInvite != NULL) {
    origTo = originalInvite->GetMIME().GetTo();
    origFrom = originalInvite->GetMIME().GetFrom();
    origTo.Replace (";tag=" + GetTag (), "");
  }
  if (originalInvite == NULL || 
      request.GetMIME().GetTo() != origTo || 
      request.GetMIME().GetFrom() != origFrom || 
      request.GetMIME().GetCSeqIndex() != originalInvite->GetMIME().GetCSeqIndex()) {
    PTRACE(1, "SIP\tUnattached " << request << " received for " << *this);
    SIP_PDU response(request, SIP_PDU::Failure_TransactionDoesNotExist);
    SendPDU(response, request.GetViaAddress(endpoint));
    return;
  }

  PTRACE(2, "SIP\tCancel received for " << *this);

  SIP_PDU response(request, SIP_PDU::Successful_OK);
  SendPDU(response, request.GetViaAddress(endpoint));
  
  if (!IsOriginating())
    Release(EndedByCallerAbort);
}


void SIPConnection::OnReceivedTrying(SIP_PDU & /*response*/)
{
  PTRACE(2, "SIP\tReceived Trying response");
}


void SIPConnection::OnReceivedRinging(SIP_PDU & /*response*/)
{
  PTRACE(2, "SIP\tReceived Ringing response");

  if (phase < AlertingPhase)
  {
    SetPhase(AlertingPhase);
    OnAlerting();
  }
}


void SIPConnection::OnReceivedSessionProgress(SIP_PDU & response)
{
  PTRACE(2, "SIP\tReceived Session Progress response");

  OnReceivedSDP(response);

  if (phase < AlertingPhase)
  {
    SetPhase(AlertingPhase);
    OnAlerting();
  }

  PTRACE(3, "SIP\tStarting receive media to annunciate remote progress tones");
  OnConnected();                        // start media streams
}


void SIPConnection::OnReceivedRedirection(SIP_PDU & response)
{
  targetAddress = response.GetMIME().GetContact();
  remotePartyAddress = targetAddress.AsQuotedString();
  PINDEX j;
  if ((j = remotePartyAddress.Find (';')) != P_MAX_INDEX)
    remotePartyAddress = remotePartyAddress.Left(j);

  endpoint.ForwardConnection (*this, remotePartyAddress);
}


void SIPConnection::OnReceivedAuthenticationRequired(SIPTransaction & transaction,
                                                     SIP_PDU & response)
{
  BOOL isProxy = response.GetStatusCode() == SIP_PDU::Failure_ProxyAuthenticationRequired;
  SIPURL proxy;
  SIPAuthentication auth;
  PString lastUsername;
  PString lastNonce;

#if PTRACING
  const char * proxyTrace = isProxy ? "Proxy " : "";
#endif
  
  if (transaction.GetMethod() != SIP_PDU::Method_INVITE) {
    PTRACE(1, "SIP\tCannot do " << proxyTrace << "Authentication Required for non INVITE");
    return;
  }

  PTRACE(2, "SIP\tReceived " << proxyTrace << "Authentication Required response");

  // Received authentication required response, try to find authentication
  // for the given realm if no proxy
  if (!auth.Parse(response.GetMIME()(isProxy 
                                     ? "Proxy-Authenticate"
                                     : "WWW-Authenticate"),
                                     isProxy)) {
    releaseMethod = ReleaseWithNothing;
    Release(EndedBySecurityDenial);
    return;
  }

  // Save the username, realm and nonce
  lastUsername = auth.GetUsername();
  lastNonce = auth.GetNonce();

  // Try to find authentication parameters for the given realm,
  // if not, use the proxy authentication parameters (if any)
  if (!endpoint.GetAuthentication(auth.GetAuthRealm(), authentication)) {
    PTRACE (3, "SIP\tCouldn't find authentication information for realm " << auth.GetAuthRealm() << ", will use SIP Outbound Proxy authentication settings, if any");
    if (!endpoint.GetProxy().IsEmpty()) {
      authentication.SetUsername(endpoint.GetProxy().GetUserName());
      authentication.SetPassword(endpoint.GetProxy().GetPassword());
    }
    else {
      releaseMethod = ReleaseWithNothing;
      Release(EndedBySecurityDenial);
      return;
    }
  }

  if (!authentication.Parse(response.GetMIME()(isProxy 
                                             ? "Proxy-Authenticate"
                                             : "WWW-Authenticate"),
                                               isProxy)) {
    releaseMethod = ReleaseWithNothing;
    Release(EndedBySecurityDenial);
    return;
  }
  
  if (!authentication.IsValid() 
      || (authentication.IsValid()
      && lastUsername == authentication.GetUsername ()
      && lastNonce    == authentication.GetNonce ())) {

    PTRACE(1, "SIP\tAlready done INVITE for " << proxyTrace << "Authentication Required");
    releaseMethod = ReleaseWithNothing;    
    Release(EndedBySecurityDenial);
    return;
  }

  // Restart the transaction with new authentication info
  // and start with a fresh To tag
  // Section 8.1.3.5 of RFC3261 tells that the authenticated
  // request SHOULD have the same value of the Call-ID, To and From.
  PINDEX j;
  if ((j = remotePartyAddress.Find (';')) != P_MAX_INDEX)
    remotePartyAddress = remotePartyAddress.Left(j);
  
  if (proxy.IsEmpty())
    proxy = endpoint.GetProxy();

  // Default routeSet if there is a proxy
  if (!proxy.IsEmpty() && routeSet.GetSize() == 0) 
    routeSet += "sip:" + proxy.GetHostName() + ':' + PString(proxy.GetPort()) + ";lr";

  SIPTransaction * invite = new SIPInvite(*this, *transport, rtpSessions);
  if (invite->Start())
  {
    PWaitAndSignal m(invitationsMutex); 
    invitations.Append(invite);
  }
  else {
    delete invite;
    PTRACE(1, "SIP\tCould not restart INVITE for " << proxyTrace << "Authentication Required");
    releaseMethod = ReleaseWithNothing;    
    Release(EndedBySecurityDenial);
  }
}


void SIPConnection::OnReceivedOK(SIPTransaction & transaction, SIP_PDU & response)
{
  if (transaction.GetMethod() != SIP_PDU::Method_INVITE) {
    PTRACE(3, "SIP\tReceived OK response for non INVITE");
    return;
  }

  PTRACE(2, "SIP\tReceived INVITE OK response");

  OnReceivedSDP(response);

  releaseMethod = ReleaseWithBYE;

  connectedTime = PTime ();
  OnConnected();                        // start media streams

  if (phase == EstablishedPhase)
    return;

  SetPhase(EstablishedPhase);
  OnEstablished();
}


void SIPConnection::OnReceivedSDP(SIP_PDU & pdu)
{
  if (!pdu.HasSDP())
    return;

  remoteSDP = pdu.GetSDP();
  OnReceivedSDPMediaDescription(remoteSDP, SDPMediaDescription::Audio, OpalMediaFormat::DefaultAudioSessionID);

  remoteFormatList += OpalRFC2833;

  OnReceivedSDPMediaDescription(remoteSDP, SDPMediaDescription::Video, OpalMediaFormat::DefaultVideoSessionID);

  OnReceivedSDPMediaDescription(remoteSDP, SDPMediaDescription::Image, OpalMediaFormat::DefaultDataSessionID);
}


BOOL SIPConnection::OnReceivedSDPMediaDescription(SDPSessionDescription & sdp,
                                                  SDPMediaDescription::MediaType mediaType,
                                                  unsigned rtpSessionId)
{
  RTP_UDP *rtpSession = NULL;
  SDPMediaDescription * mediaDescription = sdp.GetMediaDescription(mediaType);
  
  if (mediaDescription == NULL) {
    PTRACE(1, "SIP\tCould not find SDP media description for " << mediaType);
    return FALSE;
  }

  if (mediaType != SDPMediaDescription::Image) {
    // Create the RTPSession
    OpalTransportAddress localAddress;
    OpalTransportAddress address = mediaDescription->GetTransportAddress();
    rtpSession = OnUseRTPSession(rtpSessionId, address, localAddress);
    if (rtpSession == NULL && !ownerCall.IsMediaBypassPossible(*this, rtpSessionId)) {
      Release(EndedByTransportFail);
      return FALSE;
    }

    // When receiving an answer SDP, keep the remote SDP media formats order
    // but remove the media formats we do not support.
    OpalMediaFormatList mediaFormatList = mediaDescription->GetMediaFormats(rtpSessionId);
    if(mediaFormatList.GetSize() == 0) {
      PTRACE(1, "SIP\tEmpty SDP media description for " << mediaType);
      return FALSE;
    }
    remoteFormatList += mediaFormatList;
    remoteFormatList.Remove(endpoint.GetManager().GetMediaFormatMask());

    // create map for RTP payloads
    mediaDescription->CreateRTPMap(rtpSessionId, rtpPayloadMap);
    
    // Open the streams and the reverse streams
    OnOpenSourceMediaStreams(remoteFormatList, rtpSessionId, NULL);

    // set the remote address after the stream is opened
    PIPSocket::Address ip;
    WORD port;
    address.GetIpAndPort(ip, port);
    if (rtpSession && !rtpSession->SetRemoteSocketInfo(ip, port, TRUE)) {
      PTRACE(1, "SIP\tCannot set remote ports on RTP session");
      Release(EndedByTransportFail);
      return FALSE;
    }
  }
  else
  {
    // Open the streams and the reverse streams
    remoteFormatList += mediaDescription->GetMediaFormats(rtpSessionId);
    OnOpenSourceMediaStreams(remoteFormatList, rtpSessionId, NULL);

    PIPSocket::Address Ip;
    WORD Port;
    
    OpalTransportAddress MediaAddress = mediaDescription->GetTransportAddress();
    MediaAddress.GetIpAndPort(Ip, Port);

    OpalTransportAddress RemoteAddress(Ip, Port, "udp$");
 
    // set the remote address after the stream is opened
    OpalTransportUDP& Transport = GetUDPTransport();
    Transport.SetRemoteAddress(RemoteAddress);
    Transport.Connect();
  }

  return TRUE;
}


void SIPConnection::OnCreatingINVITE(SIP_PDU & /*request*/)
{
  PTRACE(2, "SIP\tCreating INVITE request");
}


void SIPConnection::QueuePDU(SIP_PDU * pdu)
{
  if (PAssertNULL(pdu) == NULL)
    return;

  if (phase >= ReleasedPhase) {
    if(pdu->GetMethod() != SIP_PDU::NumMethods)
    {
      PTRACE(4, "SIP\tIgnoring PDU: " << *pdu);
    }
    else
    {
      PTRACE(4, "SIP\tProcessing PDU: " << *pdu);
      OnReceivedPDU(*pdu);
    }
    delete pdu;
  }
  else {
    PTRACE(4, "SIP\tQueueing PDU: " << *pdu);
    pduQueue.Enqueue(pdu);
    pduSemaphore.Signal();

    if (pduHandler == NULL) {
      SafeReference();
      pduHandler = PThread::Create(PCREATE_NOTIFIER(HandlePDUsThreadMain), 0,
                                   PThread::NoAutoDeleteThread,
                                   PThread::NormalPriority,
                                   "SIP Handler:%x");
    }
  }
}


void SIPConnection::HandlePDUsThreadMain(PThread &, INT)
{
  PTRACE(2, "SIP\tPDU handler thread started.");

  while (phase != ReleasedPhase) {
    PTRACE(4, "SIP\tAwaiting next PDU.");
    pduSemaphore.Wait();

    if (!LockReadWrite())
      break;

    SIP_PDU * pdu = pduQueue.Dequeue();
    
    LockReadOnly();
    UnlockReadWrite();
    if (pdu != NULL) {
      OnReceivedPDU(*pdu);
      delete pdu;
    }

    UnlockReadOnly();
  }

  SafeDereference();

  PTRACE(2, "SIP\tPDU handler thread finished.");
}


BOOL SIPConnection::ForwardCall (const PString & fwdParty)
{
  if (fwdParty.IsEmpty ())
    return FALSE;
  
  forwardParty = fwdParty;
  PTRACE(2, "SIP\tIncoming SIP connection will be forwarded to " << forwardParty);
  Release(EndedByCallForwarded);

  return TRUE;
}


BOOL SIPConnection::SendInviteOK(const SDPSessionDescription & sdp)
{
  PString userName = endpoint.GetRegisteredPartyName(SIPURL(remotePartyAddress).GetHostName()).GetUserName();
  SIPURL contact = endpoint.GetLocalURL(*transport, userName);

  return SendInviteResponse(SIP_PDU::Successful_OK, (const char *) contact.AsQuotedString(), NULL, &sdp);
}

BOOL SIPConnection::SendACK(SIPTransaction & invite, SIP_PDU & response)
{
  if (invite.GetMethod() != SIP_PDU::Method_INVITE) { // Sanity check
    return FALSE;
  }

  SIP_PDU ack;
  // ACK Constructed following 17.1.1.3
  if (response.GetStatusCode()/100 != 2) {
    ack = SIPAck(endpoint, invite, response);
  } else { 
    ack = SIPAck(invite);
  }

  // Send the PDU using the connection transport
  if (!SendPDU(ack, ack.GetSendAddress(*this))) {
    Release(EndedByTransportFail);
    return FALSE;
  }

  return TRUE;
}

BOOL SIPConnection::SendInviteResponse(SIP_PDU::StatusCodes code, const char * contact, const char * extra, const SDPSessionDescription * sdp)
{
  if (NULL == originalInvite)
    return FALSE;

  SIP_PDU response(*originalInvite, code, contact, extra);
  if (NULL != sdp) response.SetSDP(*sdp);
  
  return SendPDU(response, originalInvite->GetViaAddress(endpoint)); 
}


BOOL SIPConnection::SendPDU(SIP_PDU & pdu, const OpalTransportAddress & address)
{
  SIPURL hosturl;

  if (transport) {
    if (lastTransportAddress != address) {
      // skip transport identifier
      PINDEX pos = address.Find('$');
      if (pos != P_MAX_INDEX)
        hosturl = address.Mid(pos+1);
      else
        hosturl = address;

      hosturl = address.Mid(pos+1);

      // Do a DNS SRV lookup
#if P_DNS
      PIPSocketAddressAndPortVector addrs;
      if (PDNS::LookupSRV(hosturl.GetHostName(), "_sip._udp", hosturl.GetPort(), addrs))  
        lastTransportAddress = OpalTransportAddress(addrs[0].address, addrs[0].port, "udp$");
      else  
#endif
        lastTransportAddress = hosturl.GetHostAddress();

      PTRACE(3, "SIP\tAdjusting transport to address " << lastTransportAddress);
      PWaitAndSignal m(transportMutex); 
      transport->SetRemoteAddress(lastTransportAddress);
    }
    
    return (pdu.Write(*transport));
  }

  return FALSE;
}

OpalTransportUDP & SIPConnection::GetUDPTransport()
{
  if (!udpTransport) {
    PIPSocket::Address localIp;
    GetTransport().GetLocalAddress().GetIpAddress(localIp);

    udpTransport = new OpalTransportUDP(GetEndPoint(), localIp);
  }

  return *udpTransport;
}

void SIPConnection::OnRTPStatistics(const RTP_Session & session) const
{
  endpoint.OnRTPStatistics(*this, session);
}

void SIPConnection::OnReceivedINFO(SIP_PDU & pdu)
{
  SIP_PDU::StatusCodes status = SIP_PDU::Failure_UnsupportedMediaType;
  SIPMIMEInfo & mimeInfo = pdu.GetMIME();
  PString contentType = mimeInfo.GetContentType();

  if (contentType *= ApplicationDTMFRelayKey) {
    PStringArray lines = pdu.GetEntityBody().Lines();
    PINDEX i;
    char tone = -1;
    int duration = -1;
    for (i = 0; i < lines.GetSize(); ++i) {
      PStringArray tokens = lines[i].Tokenise('=', FALSE);
      PString val;
      if (tokens.GetSize() > 1)
        val = tokens[1].Trim();
      if (tokens.GetSize() > 0) {
        if (tokens[0] *= "signal")
          tone = val[0];
        else if (tokens[0] *= "duration")
          duration = val.AsInteger();
      }
    }
    if (tone != -1)
      OnUserInputTone(tone, duration == 0 ? 100 : tone);
    status = SIP_PDU::Successful_OK;
  }
  else if (contentType *= ApplicationDTMFKey) {
    OnUserInputString(pdu.GetEntityBody().Trim());
    status = SIP_PDU::Successful_OK;
  }
  else 
    status = SIP_PDU::Failure_UnsupportedMediaType;

  SIP_PDU response(pdu, status);
  SendPDU(response, pdu.GetViaAddress(endpoint));
}


void SIPConnection::OnReceivedPING(SIP_PDU & pdu)
{
  PTRACE(1, "SIP\tReceived PING");
  SIP_PDU response(pdu, SIP_PDU::Successful_OK);
  SendPDU(response, pdu.GetViaAddress(endpoint));
}


OpalConnection::SendUserInputModes SIPConnection::GetRealSendUserInputMode() const
{
  switch (sendUserInputMode) {
    case SendUserInputAsString:
    case SendUserInputAsTone:
      return sendUserInputMode;
    default:
      break;
  }

  return SendUserInputAsInlineRFC2833;
}

BOOL SIPConnection::SendUserInputTone(char tone, unsigned duration)
{
  SendUserInputModes mode = GetRealSendUserInputMode();

  PTRACE(2, "SIP\tSendUserInputTime('" << tone << "', " << duration << "), using mode " << mode);

  switch (mode) {
    case SendUserInputAsTone:
    case SendUserInputAsString:
      {
        SIPTransaction * infoTransaction = new SIPTransaction(*this, *transport, SIP_PDU::Method_INFO);
        SIPMIMEInfo & mimeInfo = infoTransaction->GetMIME();
        PStringStream str;
        if (mode == SendUserInputAsTone) {
          mimeInfo.SetContentType(ApplicationDTMFRelayKey);
          str << "Signal=" << tone << "\r\n" << "Duration=" << duration << "\r\n";
        } else {
          mimeInfo.SetContentType("application/dtmf-relay");
          mimeInfo.SetContentType("application/dtmf");
          str << tone;
        }
        infoTransaction->GetEntityBody() = str;
        infoTransaction->Wait();
        BOOL success = !infoTransaction->IsFailed();
        delete infoTransaction;
        return success;
      }

    // anything else - send as RFC 2833
    case SendUserInputAsProtocolDefault:
    default:
      break;
  }

  return OpalConnection::SendUserInputTone(tone, duration);
}

/////////////////////////////////////////////////////////////////////////////

SIP_RTP_Session::SIP_RTP_Session(const SIPConnection & conn)
  : connection(conn)
{
}


void SIP_RTP_Session::OnTxStatistics(const RTP_Session & session) const
{
  connection.OnRTPStatistics(session);
}


void SIP_RTP_Session::OnRxStatistics(const RTP_Session & session) const
{
  connection.OnRTPStatistics(session);
}


// End of file ////////////////////////////////////////////////////////////////
