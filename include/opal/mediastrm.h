/*
 * mediastrm.h
 *
 * Media Stream classes
 *
 * Open Phone Abstraction Library (OPAL)
 * Formally known as the Open H323 project.
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
 * The Original Code is Open Phone Abstraction Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Log: mediastrm.h,v $
 * Revision 1.2021  2004/10/02 11:50:54  rjongbloed
 * Fixed RTP media stream so assures RTP session is open before starting.
 *
 * Revision 2.19  2004/08/14 07:56:29  rjongbloed
 * Major revision to utilise the PSafeCollection classes for the connections and calls.
 *
 * Revision 2.18  2004/05/17 13:24:18  rjongbloed
 * Added silence suppression.
 *
 * Revision 2.17  2004/03/11 06:54:27  csoutheren
 * Added ability to disable SIP or H.323 stacks
 *
 * Revision 2.16  2003/06/02 02:57:10  rjongbloed
 * Moved LID specific media stream class to LID source file.
 *
 * Revision 2.15  2003/04/16 02:30:21  robertj
 * Fixed comments on ReadData() and WriteData() functions.
 *
 * Revision 2.14  2003/03/17 10:26:59  robertj
 * Added video support.
 *
 * Revision 2.13  2002/11/10 11:33:17  robertj
 * Updated to OpenH323 v1.10.3
 *
 * Revision 2.12  2002/09/16 02:52:35  robertj
 * Added #define so can select if #pragma interface/implementation is used on
 *   platform basis (eg MacOS) rather than compiler, thanks Robert Monaghan.
 *
 * Revision 2.11  2002/04/15 08:47:42  robertj
 * Fixed problem with mismatched payload type being propagated.
 * Fixed correct setting of jitter buffer size in RTP media stream.
 *
 * Revision 2.10  2002/02/13 02:33:29  robertj
 * Added ability for media patch (and transcoders) to handle multiple RTP frames.
 * Removed media stream being descended from PChannel, not really useful.
 *
 * Revision 2.9  2002/02/11 07:39:15  robertj
 * Added media bypass for streams between compatible protocols.
 *
 * Revision 2.8  2002/01/22 05:10:58  robertj
 * Removed redundant code
 *
 * Revision 2.7  2002/01/22 05:09:00  robertj
 * Removed payload mismatch detection from RTP media stream.
 * Added function to get media patch from media stream.
 *
 * Revision 2.6  2002/01/14 02:27:32  robertj
 * Added ability to turn jitter buffer off in media stream to allow for patches
 *   that do not require it.
 *
 * Revision 2.5  2001/10/15 04:28:35  robertj
 * Added delayed start of media patch threads.
 *
 * Revision 2.4  2001/10/04 05:44:00  craigs
 * Changed to start media patch threads in Paused state
 *
 * Revision 2.3  2001/10/04 00:41:20  robertj
 * Removed GetMediaFormats() function as is not useful.
 *
 * Revision 2.2  2001/08/21 01:10:35  robertj
 * Fixed propagation of sound channel buffers through media stream.
 *
 * Revision 2.1  2001/08/01 05:51:47  robertj
 * Made OpalMediaFormatList class global to help with documentation.
 *
 * Revision 2.0  2001/07/27 15:48:24  robertj
 * Conversion of OpenH323 to Open Phone Abstraction Library (OPAL)
 *
 */

#ifndef __OPAL_MEDIASTRM_H
#define __OPAL_MEDIASTRM_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


#include <opal/buildopts.h>
#include <opal/mediafmt.h>


class RTP_Session;
class OpalMediaPatch;
class OpalLine;


/**This class describes a media stream as used in the OPAL system. A media
   stream is the channel through which media data is trasferred between OPAL
   entities. For example, data being sent to an RTP session over a network
   would be through a media stream.
  */
class OpalMediaStream : public PObject
{
    PCLASSINFO(OpalMediaStream, PObject);
  protected:
  /**@name Construction */
  //@{
    /**Construct a new media stream.
      */
    OpalMediaStream(
      const OpalMediaFormat & mediaFormat, /// Media format for stream
      unsigned sessionID,                  /// Session number for stream
      BOOL isSource                        /// Is a source stream
    );

  public:
    /**Destroy the media stream.
       Make sure the patch, if present, has been stopped and deleted.
      */
    ~OpalMediaStream();
  //@}

  public:
  /**@name Overrides from PObject */
  //@{
    /**Standard stream print function.
       The PObject class has a << operator defined that calls this function
       polymorphically.
      */
    void PrintOn(
      ostream & strm    /// Stream to output text representation
    ) const;
  //@}

  /**@name Operations */
  //@{
    /**Get the currently selected media format.
       The media data format is a string representation of the format being
       transferred by the media channel. It is typically a value as provided
       by the RTP_PayloadType class.

       The default behaviour simply returns the member variable "mediaFormat".
      */
    virtual OpalMediaFormat GetMediaFormat() const;

    /**Open the media stream using the media format.

       The default behaviour simply sets the isOpen variable to TRUE.
      */
    virtual BOOL Open();

    /**Start the media stream.

       The default behaviour calls Resume() on the associated OpalMediaPatch
       thread if it was suspended.
      */
    virtual BOOL Start();

    /**Close the media stream.

       The default does nothing.
      */
    virtual BOOL Close();

    /**Write a list of RTP frames of data to the sink media stream.
       The default behaviour simply calls WritePacket() on each of the
       elements in the list.
      */
    virtual BOOL WritePackets(
      RTP_DataFrameList & packets
    );

    /**Read an RTP frame of data from the source media stream.
       The default behaviour simply calls ReadData() on the data portion of the
       RTP_DataFrame and sets the frames timestamp and marker from the internal
       member variables of the media stream class.
      */
    virtual BOOL ReadPacket(
      RTP_DataFrame & packet
    );

    /**Write an RTP frame of data to the sink media stream.
       The default behaviour simply calls WriteData() on the data portion of the
       RTP_DataFrame and and sets the internal timestamp and marker from the
       member variables of the media stream class.
      */
    virtual BOOL WritePacket(
      RTP_DataFrame & packet
    );

    /**Read raw media data from the source media stream.
       The default behaviour simply calls ReadPacket() on the data portion of the
       RTP_DataFrame and sets the frames timestamp and marker from the internal
       member variables of the media stream class.
      */
    virtual BOOL ReadData(
      BYTE * data,      /// Data buffer to read to
      PINDEX size,      /// Size of buffer
      PINDEX & length   /// Length of data actually read
    );

    /**Write raw media data to the sink media stream.
       The default behaviour calls WritePacket() on the data portion of the
       RTP_DataFrame and and sets the internal timestamp and marker from the
       member variables of the media stream class.
      */
    virtual BOOL WriteData(
      const BYTE * data,   /// Data to write
      PINDEX length,       /// Length of data to read.
      PINDEX & written     /// Length of data actually written
    );

    /**Set the data size in bytes that is expected to be used. Some media
       streams can make use of this information to perform optimisations.

       The default behaviour does nothing.
      */
    virtual BOOL SetDataSize(
      PINDEX dataSize  /// New data size
    );

    /**Get the data size in bytes that is expected to be used. Some media
       streams can make use of this information to perform optimisations.
      */
    PINDEX GetDataSize() const { return defaultDataSize; }

    /**Indicate if the media stream is synchronous.
       If this returns TRUE then the media stream will block of the amount of
       time it takes to annunciate the data. For example if the media stream
       is over a sound card, and 480 bytes of data are to be written it will
       take 30 milliseconds to complete.
      */
    virtual BOOL IsSynchronous() const = 0;

    /**Indicate if the media stream requires a OpalMediaPatch thread.
       The default behaviour returns TRUE.
      */
    virtual BOOL RequiresPatchThread() const;

    /**Enable jitter buffer for the media stream.

       The default behaviour does nothing.
      */
    virtual void EnableJitterBuffer() const;
  //@}

  /**@name Member variable access */
  //@{
    /**Determine of media stream is a source or a sink.
      */
    BOOL IsSource() const { return isSource; }

    /**Determine of media stream is a source or a sink.
      */
    BOOL IsSink() const { return !isSource; }

    /**Get the session number of the stream.
     */
    unsigned GetSessionID() const { return sessionID; }

    /**Get the timestamp of last read.
      */
    unsigned GetTimestamp() const { return timestamp; }

    /**Set timestamp for next write.
      */
    void SetTimestamp(unsigned ts) { timestamp = ts; }

    /**Get the marker bit of last read.
      */
    BOOL GetMarker() const { return marker; }

    /**Set marker bit for next write.
      */
    void SetMarker(BOOL m) { marker = m; }

    /**Set the patch thread that is using this stream.
      */
    void SetPatch(
      OpalMediaPatch * patch  /// Media patch thread
    );

    /**Get the patch thread that is using the stream.
      */
    OpalMediaPatch * GetPatch() const { return patchThread; }
  //@}

  protected:
    OpalMediaFormat mediaFormat;
    unsigned        sessionID;
    BOOL            isSource;
    BOOL            isOpen;
    PINDEX          defaultDataSize;
    unsigned        timestamp;
    BOOL            marker;
    unsigned        mismatchedPayloadTypes;

    OpalMediaPatch * patchThread;
    PMutex           patchMutex;
};

PLIST(OpalMediaStreamList, OpalMediaStream);


/**This class describes a media stream that is used for media bypass.
  */
class OpalNullMediaStream : public OpalMediaStream
{
    PCLASSINFO(OpalNullMediaStream, OpalMediaStream);
  public:
  /**@name Construction */
  //@{
    /**Construct a new media stream for RTP sessions.
      */
    OpalNullMediaStream(
      const OpalMediaFormat & mediaFormat, /// Media format for stream
      unsigned sessionID,                  /// Session number for stream
      BOOL isSource                        /// Is a source stream
    );
  //@}

  /**@name Overrides of OpalMediaStream class */
  //@{
    /**Read raw media data from the source media stream.
       The default behaviour does nothing and returns FALSE.
      */
    virtual BOOL ReadData(
      BYTE * data,      /// Data buffer to read to
      PINDEX size,      /// Size of buffer
      PINDEX & length   /// Length of data actually read
    );

    /**Write raw media data to the sink media stream.
       The default behaviour does nothing and returns FALSE.
      */
    virtual BOOL WriteData(
      const BYTE * data,   /// Data to write
      PINDEX length,       /// Length of data to read.
      PINDEX & written     /// Length of data actually written
    );

    /**Indicate if the media stream requires a OpalMediaPatch thread.
       The default behaviour returns FALSE.
      */
    virtual BOOL RequiresPatchThread() const;

    /**Indicate if the media stream is synchronous.
       Returns FALSE.
      */
    virtual BOOL IsSynchronous() const;
  //@}
};


/**This class describes a media stream that transfers data to/from a RTP
   session.
  */
class OpalRTPMediaStream : public OpalMediaStream
{
    PCLASSINFO(OpalRTPMediaStream, OpalMediaStream);
  public:
  /**@name Construction */
  //@{
    /**Construct a new media stream for RTP sessions.
      */
    OpalRTPMediaStream(
      const OpalMediaFormat & mediaFormat, /// Media format for stream
      BOOL isSource,                       /// Is a source stream
      RTP_Session & rtpSession,    /// RTP session to stream to/from
      unsigned minAudioJitterDelay,/// Minimum jitter buffer size (if applicable)
      unsigned maxAudioJitterDelay /// Maximum jitter buffer size (if applicable)
    );
  //@}

  /**@name Overrides of OpalMediaStream class */
  //@{
    /**Open the media stream using the media format.

       The default behaviour simply sets the isOpen variable to TRUE.
      */
    virtual BOOL Open();

    /**Close the media stream.

       The default does nothing.
      */
    virtual BOOL Close();

    /**Read an RTP frame of data from the source media stream.
       The new behaviour simply calls RTP_Session::ReadData().
      */
    virtual BOOL ReadPacket(
      RTP_DataFrame & packet
    );

    /**Write an RTP frame of data to the sink media stream.
       The new behaviour simply calls RTP_Session::WriteData().
      */
    virtual BOOL WritePacket(
      RTP_DataFrame & packet
    );

    /**Indicate if the media stream is synchronous.
       Returns FALSE for RTP streams.
      */
    virtual BOOL IsSynchronous() const;

    /**Enable jitter buffer for the media stream.

       The default behaviour does nothing.
      */
    virtual void EnableJitterBuffer() const;
  //@}

  protected:
    RTP_Session & rtpSession;
    unsigned      minAudioJitterDelay;
    unsigned      maxAudioJitterDelay;
};



/**This class describes a media stream that transfers data to/from a PChannel.
  */
class OpalRawMediaStream : public OpalMediaStream
{
    PCLASSINFO(OpalRawMediaStream, OpalMediaStream);
  protected:
  /**@name Construction */
  //@{
    /**Construct a new media stream for channel.
      */
    OpalRawMediaStream(
      const OpalMediaFormat & mediaFormat, /// Media format for stream
      unsigned sessionID,                  /// Session number for stream
      BOOL isSource,                       /// Is a source stream
      PChannel * channel,                  /// I/O channel to stream to/from
      BOOL autoDelete                      /// Automatically delete channel
    );

    /**Delete attached channel if autoDelete enabled.
      */
    ~OpalRawMediaStream();
  //@}

  public:
  /**@name Overrides of OpalMediaStream class */
  //@{
    /**Read raw media data from the source media stream.
       The default behaviour reads from the PChannel object.
      */
    virtual BOOL ReadData(
      BYTE * data,      /// Data buffer to read to
      PINDEX size,      /// Size of buffer
      PINDEX & length   /// Length of data actually read
    );

    /**Write raw media data to the sink media stream.
       The default behaviour writes to the PChannel object.
      */
    virtual BOOL WriteData(
      const BYTE * data,   /// Data to write
      PINDEX length,       /// Length of data to read.
      PINDEX & written     /// Length of data actually written
    );

    /**Close the media stream.

       Closes the associated PChannel.
      */
    virtual BOOL Close();
  //@}

  protected:
    PChannel * channel;
    BOOL       autoDelete;
};



/**This class describes a media stream that transfers data to/from a file.
  */
class OpalFileMediaStream : public OpalRawMediaStream
{
    PCLASSINFO(OpalFileMediaStream, OpalRawMediaStream);
  public:
  /**@name Construction */
  //@{
    /**Construct a new media stream for files.
      */
    OpalFileMediaStream(
      const OpalMediaFormat & mediaFormat, /// Media format for stream
      unsigned sessionID,                  /// Session number for stream
      BOOL isSource,                       /// Is a source stream
      PFile * file,                        /// File to stream to/from
      BOOL autoDelete = TRUE               /// Automatically delete file
    );

    /**Construct a new media stream for files.
      */
    OpalFileMediaStream(
      const OpalMediaFormat & mediaFormat, /// Media format for stream
      unsigned sessionID,                  /// Session number for stream
      BOOL isSource,                       /// Is a source stream
      const PFilePath & path               /// File path to stream to/from
    );
  //@}

  /**@name Overrides of OpalMediaStream class */
  //@{
    /**Indicate if the media stream is synchronous.
       Returns TRUE for LID streams.
      */
    virtual BOOL IsSynchronous() const;
  //@}

  protected:
    PFile file;
};


/**This class describes a media stream that transfers data to/from a audio
   PSoundChannel.
  */
class OpalAudioMediaStream : public OpalRawMediaStream
{
    PCLASSINFO(OpalAudioMediaStream, OpalRawMediaStream);
  public:
  /**@name Construction */
  //@{
    /**Construct a new media stream for audio.
      */
    OpalAudioMediaStream(
      const OpalMediaFormat & mediaFormat, /// Media format for stream
      unsigned sessionID,                  /// Session number for stream
      BOOL isSource,                       /// Is a source stream
      PINDEX buffers,                      /// Number of buffers on sound channel
      PSoundChannel * channel,             /// Audio device to stream to/from
      BOOL autoDelete = TRUE               /// Automatically delete PSoundChannel
    );

    /**Construct a new media stream for audio.
      */
    OpalAudioMediaStream(
      const OpalMediaFormat & mediaFormat, /// Media format for stream
      unsigned sessionID,                  /// Session number for stream
      BOOL isSource,                       /// Is a source stream
      PINDEX buffers,                      /// Number of buffers on sound channel
      const PString & deviceName           /// Name of audio device to stream to/from
    );
  //@}

  /**@name Overrides of OpalMediaStream class */
  //@{
    /**Set the data size in bytes that is expected to be used. Some media
       streams can make use of this information to perform optimisations.

       The defafault simply sets teh member variable defaultDataSize.
      */
    virtual BOOL SetDataSize(
      PINDEX dataSize  /// New data size
    );

    /**Indicate if the media stream is synchronous.
       Returns TRUE for LID streams.
      */
    virtual BOOL IsSynchronous() const;
  //@}

  protected:
    PINDEX soundChannelBuffers;
};


/**This class describes a media stream that transfers data to/from a
   PVideoDevice.
  */
class OpalVideoMediaStream : public OpalMediaStream
{
    PCLASSINFO(OpalVideoMediaStream, OpalMediaStream);
  public:
  /**@name Construction */
  //@{
    /**Construct a new media stream for channel.
      */
    OpalVideoMediaStream(
      const OpalMediaFormat & mediaFormat, /// Media format for stream
      unsigned sessionID,                  /// Session number for stream
      PVideoInputDevice * inputDevice,     /// Device to use for video grabbing
      PVideoOutputDevice * outputDevice,   /// Device to use for video display
      BOOL autoDelete = TRUE               /// Automatically delete PVideoDevices
    );

    /**Delete attached channel if autoDelete enabled.
      */
    ~OpalVideoMediaStream();
  //@}

  /**@name Overrides of PChannel class */
  //@{
    /**Open the media stream.

       The default behaviour sets the OpalLineInterfaceDevice format and
       calls Resume() on the associated OpalMediaPatch thread.
      */
    virtual BOOL Open();

    /**Read raw media data from the source media stream.
       The default behaviour simply calls ReadPacket() on the data portion of the
       RTP_DataFrame and sets the frames timestamp and marker from the internal
       member variables of the media stream class.
      */
    virtual BOOL ReadData(
      BYTE * data,      /// Data buffer to read to
      PINDEX size,      /// Size of buffer
      PINDEX & length   /// Length of data actually read
    );

    /**Write raw media data to the sink media stream.
       The default behaviour calls WritePacket() on the data portion of the
       RTP_DataFrame and and sets the internal timestamp and marker from the
       member variables of the media stream class.
      */
    virtual BOOL WriteData(
      const BYTE * data,   /// Data to write
      PINDEX length,       /// Length of data to read.
      PINDEX & written     /// Length of data actually written
    );

    /**Indicate if the media stream is synchronous.
       Returns TRUE for LID streams.
      */
    virtual BOOL IsSynchronous() const;
  //@}

  protected:
    PVideoInputDevice  * inputDevice;
    PVideoOutputDevice * outputDevice;
    BOOL                 autoDelete;
};



#endif //__OPAL_MEDIASTRM_H


// End of File ///////////////////////////////////////////////////////////////
