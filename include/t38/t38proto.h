/*
 * t38proto.h
 *
 * T.38 protocol handler
 *
 * Open Phone Abstraction Library
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
 * The Original Code is Open H323 Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Log: t38proto.h,v $
 * Revision 1.2007  2002/11/10 11:33:17  robertj
 * Updated to OpenH323 v1.10.3
 *
 * Revision 2.5  2002/09/16 02:52:36  robertj
 * Added #define so can select if #pragma interface/implementation is used on
 *   platform basis (eg MacOS) rather than compiler, thanks Robert Monaghan.
 *
 * Revision 2.4  2002/09/04 06:01:47  robertj
 * Updated to OpenH323 v1.9.6
 *
 * Revision 2.3  2002/02/11 09:32:12  robertj
 * Updated to openH323 v1.8.0
 *
 * Revision 2.2  2002/01/14 06:35:57  robertj
 * Updated to OpenH323 v1.7.9
 *
 * Revision 2.1  2001/08/01 05:06:00  robertj
 * Major changes to H.323 capabilities, uses OpalMediaFormat for base name.
 *
 * Revision 2.0  2001/07/27 15:48:24  robertj
 * Conversion of OpenH323 to Open Phone Abstraction Library (OPAL)
 *
 * Revision 1.7  2002/09/16 01:14:15  robertj
 * Added #define so can select if #pragma interface/implementation is used on
 *   platform basis (eg MacOS) rather than compiler, thanks Robert Monaghan.
 *
 * Revision 1.6  2002/09/03 06:19:37  robertj
 * Normalised the multi-include header prevention ifdef/define symbol.
 *
 * Revision 1.5  2002/02/09 04:39:01  robertj
 * Changes to allow T.38 logical channels to use single transport which is
 *   now owned by the OpalT38Protocol object instead of H323Channel.
 *
 * Revision 1.4  2002/01/01 23:27:50  craigs
 * Added CleanupOnTermination functions
 * Thanks to Vyacheslav Frolov
 *
 * Revision 1.3  2001/12/22 01:57:04  robertj
 * Cleaned up code and allowed for repeated sequence numbers.
 *
 * Revision 1.2  2001/11/09 05:39:54  craigs
 * Added initial T.38 support thanks to Adam Lazur
 *
 * Revision 1.1  2001/07/17 04:44:29  robertj
 * Partial implementation of T.120 and T.38 logical channels.
 *
 */

#ifndef __OPAL_T38PROTO_H
#define __OPAL_T38PROTO_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


#include <opal/mediafmt.h>


class OpalTransport;
class T38_IFPPacket;


///////////////////////////////////////////////////////////////////////////////

/**This class handles the processing of the T.38 protocol.
  */
class OpalT38Protocol : public PObject
{
    PCLASSINFO(OpalT38Protocol, PObject);
  public:
    static OpalMediaFormat const MediaFormat;


  /**@name Construction */
  //@{
    /**Create a new protocol handler.
     */
    OpalT38Protocol();

    /**Destroy the protocol handler.
     */
    ~OpalT38Protocol();
  //@}

  /**@name Operations */
  //@{
    /**This is called to clean up any threads on connection termination.
     */
    virtual void Close();

    /**Handle the origination of a T.38 connection.
      */
    virtual BOOL Originate();

    /**Handle the origination of a T.38 connection.
      */
    virtual BOOL Answer();

    /**Prepare outgoing T.38 packet.

       If returns FALSE, then the writing loop should be terminated.
      */
    virtual BOOL PreparePacket(
      T38_IFPPacket & pdu
    );

    /**Handle incoming T.38 packet.

       If returns FALSE, then the reading loop should be terminated.
      */
    virtual BOOL HandlePacket(
      const T38_IFPPacket & pdu
    );

    /**Handle lost T.38 packets.

       If returns FALSE, then the reading loop should be terminated.
      */
    virtual BOOL HandlePacketLost(
      unsigned nLost
    );
  //@}

    OpalTransport * GetTransport() const { return transport; }
    void SetTransport(
      OpalTransport * transport,
      BOOL autoDelete = TRUE
    );

  protected:
    OpalTransport * transport;
    BOOL            autoDeleteTransport;
};


#endif // __OPAL_T38PROTO_H


/////////////////////////////////////////////////////////////////////////////
