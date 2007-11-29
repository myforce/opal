/*
 *
 * Inter Asterisk Exchange 2
 * 
 * Open Phone Abstraction Library (OPAL)
 *
 * Extension of the Opal Media stream, where the media from the IAX2 side is
 * linked to the OPAL 
 *
 * Copyright (c) 2005 Indranet Technologies Ltd.
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
 * The Initial Developer of the Original Code is Indranet Technologies Ltd.
 *
 * The author of this code is Derek J Smithies
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#ifndef __OPAL_IAX2_MEDIASTRM_H
#define __OPAL_IAX2_MEDIASTRM_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


#include <opal/buildopts.h>
#include <opal/mediafmt.h>
#include <iax2/iax2con.h>
#include <iax2/iax2jitter.h>

class RTP_Session;
class OpalMediaPatch;
class OpalLine;


/**This class describes a media stream, which is an interface to the opal classes for 
   generating encoded media data 
*/
class OpalIAX2MediaStream : public OpalMediaStream
{
  PCLASSINFO(OpalIAX2MediaStream, OpalMediaStream);
  /**@name Construction */
  //@{
    /**Construct a new media stream for connecting to the media 
      */
    OpalIAX2MediaStream(
		   IAX2Connection &con,                 /*!< IAX connection to read/send incoming packets */
		   const OpalMediaFormat & mediaFormat, /*!< Media format for stream */
		   unsigned sessionID,                  /*!< Session number for stream */
		   PBoolean isSource                        /*!< Is a source stream */
		   );
  //@}
 
 public:
  /**@name Overrides of OpalMediaStream class */
  //@{
    /**Open the media stream.
 
 
      */
    virtual PBoolean Open();
 
    /**Start the media stream.
       
    The default behaviour calls Resume() on the associated
    OpalMediaPatch thread if it was suspended.
    */
    virtual PBoolean Start();

    /**Close the media stream.
 
       The default does nothing.
      */
    virtual PBoolean Close();
 
    /**
       Goes to the IAX2Connection class, and removes a packet from the connection. The connection class turned the media 
       packet into a RTP_DataFrame class, and jitter buffered it.

    @return PTrue on successful read of a packet, PFalse on faulty read.*/
    virtual PBoolean ReadPacket(
      RTP_DataFrame & packet ///< Data buffer to read to
    );

   /**Write raw media data to the sink media stream.
       The default behaviour writes to the OpalLine object.
      */
    virtual PBoolean WriteData(
      const BYTE * data,   ///< Data to write
      PINDEX length,       ///< Length of data to write.
      PINDEX & written     ///<Length of data actually written
    );

    /**Indicate if the media stream is synchronous.
       A synchronous stream is one that is regular, such as the sound frames
       from a sound card.
      */
    virtual PBoolean IsSynchronous() const;
  //@}

  protected:
    /**The connection is the source/sink of our data packets */
    IAX2Connection & connection;

    /**There was unused data from an incoming ethernet frame. The
       unused data is stored here. 
    */
    PBYTEArray pendingData;
};

#endif  //__OPAL_IAX2_MEDIASTRM_H

/* The comment below is magic for those who use emacs to edit this file. */
/* With the comment below, the tab key does auto indent to 2 spaces.     */

/*
 * Local Variables:
 * mode:c
 * c-file-style:linux
 * c-basic-offset:2
 * End:
 */

