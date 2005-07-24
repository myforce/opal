/*
 * vidcodec.h
 *
 * Uncompressed video handler
 *
 * Open Phone Abstraction Library
 *
 * Copyright (c) 2003 Equivalence Pty. Ltd.
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
 * Contributor(s): 
 *
 * $Log: vidcodec.h,v $
 * Revision 1.2007  2005/07/24 07:33:07  rjongbloed
 * Simplified "uncompressed" transcoder sp can test video media streams.
 *
 * Revision 2.5  2005/02/21 12:19:45  rjongbloed
 * Added new "options list" to the OpalMediaFormat class.
 *
 * Revision 2.4  2004/09/01 12:21:27  rjongbloed
 * Added initialisation of H323EndPoints capability table to be all codecs so can
 *   correctly build remote caps from fqast connect params. This had knock on effect
 *   with const keywords added in numerous places.
 *
 * Revision 2.3  2004/03/11 06:54:26  csoutheren
 * Added ability to disable SIP or H.323 stacks
 *
 * Revision 2.2  2004/01/18 15:35:20  rjongbloed
 * More work on video support
 *
 * Revision 2.1  2003/03/17 10:26:59  robertj
 * Added video support.
 *
 */

#ifndef __OPAL_VIDCODEC_H
#define __OPAL_VIDCODEC_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


#include <opal/transcoders.h>

#ifndef NO_H323
#include <h323/h323caps.h>
#endif


///////////////////////////////////////////////////////////////////////////////

/**This class defines a transcoder implementation class that will
   encode/decode video.

   An application may create a descendent off this class and override
   functions as required for descibing a specific transcoder.
 */
class OpalVideoTranscoder : public OpalTranscoder
{
    PCLASSINFO(OpalVideoTranscoder, OpalTranscoder);
  public:
    struct FrameHeader {
      PInt32l  x;
      PInt32l  y;
      PUInt32l width;
      PUInt32l height;
      BYTE     data[1];
    };

  /**@name Construction */
  //@{
    /** Create a new video transcoder implementation.
      */
    OpalVideoTranscoder(
      const OpalTranscoderRegistration & registration /// Registration fro transcoder
    );
  //@}

  /**@name Operations */
  //@{
    /**Convert the data from one format to another.
       This function takes the input data as a RTP_DataFrame and converts it
       to its output format, placing it into the RTP_DataFrame provided.

       This is a dummy function as nearly all video conversion will does not
       have a one to one input to output frames ratio, so the ConvertFrames()
       function is used instead.

       Returns FALSE if the conversion fails.
      */
    virtual BOOL Convert(
      const RTP_DataFrame & input,  /// Input data
      RTP_DataFrame & output        /// Output data
    );
  //@}

  protected:
    unsigned frameWidth;
    unsigned frameHeight;
    unsigned fillLevel;
    unsigned videoQuality;
};


///////////////////////////////////////////////////////////////////////////////

#ifndef NO_H323

/**This class is a uncompressed video capability.
 */
class H323_UncompVideoCapability : public H323NonStandardVideoCapability
{
  PCLASSINFO(H323_UncompVideoCapability, H323NonStandardVideoCapability)

  public:
  /**@name Construction */
  //@{
    /**Create a new uncompressed video Capability
     */
    H323_UncompVideoCapability(
      const H323EndPoint & endpoint, /// Endpoint to get t35 information
      const PString & colourFormat   /// Video colour format name
    );
  //@}

  /**@name Overrides from class PObject */
  //@{
    /**Create a copy of the object.
      */
    virtual PObject * Clone() const;
   //@}

  /**@name Identification functions */
  //@{
    /**Get the name of the media data format this class represents.
     */
    virtual PString GetFormatName() const;
  //@}

  protected:
    PString colourFormat;
};

#define OPAL_REGISTER_UNCOMPRESSED_VIDEO_H323 \
  H323_REGISTER_CAPABILITY_FUNCTION(H323_RGB24, OPAL_RGB24, ep) \
    { return new H323_UncompVideoCapability(ep, OPAL_RGB24); } \
  H323_REGISTER_CAPABILITY_FUNCTION(H323_RGB32, OPAL_RGB32, ep) \
    { return new H323_UncompVideoCapability(ep, OPAL_RGB32); }

#else // ifndef NO_H323

#define OPAL_REGISTER_UNCOMPRESSED_VIDEO_H323

#endif // ifndef NO_H323


///////////////////////////////////////////////////////////////////////////////

/**This class defines a transcoder implementation class that will
   encode/decode uncompressed video.
 */
class OpalUncompVideoTranscoder : public OpalVideoTranscoder
{
    PCLASSINFO(OpalUncompVideoTranscoder, OpalVideoTranscoder);
  public:
  /**@name Construction */
  //@{
    /** Create a new video transcoder implementation.
      */
    OpalUncompVideoTranscoder(
      const OpalTranscoderRegistration & registration /// Registration fro transcoder
    );

    /**Destroy the video transcoder cleaning up the colour converter.
      */
    ~OpalUncompVideoTranscoder();
  //@}

  /**@name Operations */
  //@{
    /**Get the optimal size for data frames to be converted.
       This function returns the size of frames that will be most efficient
       in conversion. A RTP_DataFrame will attempt to provide or use data in
       multiples of this size. Note that it may not do so, so the transcoder
       must be able to handle any sized packets.
      */
    virtual PINDEX GetOptimalDataFrameSize(
      BOOL input      /// Flag for input or output data size
    ) const;

    /**Convert the data from one format to another.
       This function takes the input data as a RTP_DataFrame and converts it
       to its output format, placing it (possibly) into multiple RTP_DataFrame
       objects.

       The default behaviour makes sure the output list has only one element
       in it and calls the Convert() function.

       Returns FALSE if the conversion fails.
      */
    virtual BOOL ConvertFrames(
      const RTP_DataFrame & input,  /// Input data
      RTP_DataFrameList & output    /// Output data
    );
  //@}
};


class Opal_RGB24_RGB24 : public OpalUncompVideoTranscoder {
  public:
    Opal_RGB24_RGB24(
      const OpalTranscoderRegistration & registration /// Registration fro transcoder
    ) : OpalUncompVideoTranscoder(registration) { }
};


class Opal_RGB32_RGB32 : public OpalUncompVideoTranscoder {
  public:
    Opal_RGB32_RGB32(
      const OpalTranscoderRegistration & registration /// Registration fro transcoder
    ) : OpalUncompVideoTranscoder(registration) { }
};


///////////////////////////////////////////////////////////////////////////////

#define OPAL_REGISTER_UNCOMPRESSED_VIDEO() \
          OPAL_REGISTER_UNCOMPRESSED_VIDEO_H323 \
          OPAL_REGISTER_TRANSCODER(Opal_RGB32_RGB32, OPAL_RGB32, OPAL_RGB32); \
          OPAL_REGISTER_TRANSCODER(Opal_RGB24_RGB24, OPAL_RGB24, OPAL_RGB24)


#define OPAL_RGB24   "RGB24"
#define OPAL_RGB32   "RGB32"
#define OPAL_YUV420P "YUV420P"


#endif // __OPAL_VIDCODEC_H


/////////////////////////////////////////////////////////////////////////////
