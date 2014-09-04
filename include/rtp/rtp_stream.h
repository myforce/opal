/*
 * rtp_stream.h
 *
 * Media Stream classes
 *
 * Open Phone Abstraction Library (OPAL)
 *
 * Copyright (C) 2014 Vox Lucida Pty. Ltd.
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

#ifndef OPAL_RTP_RTP_STREAM_H
#define OPAL_RTP_RTP_STREAM_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include <opal_config.h>

#include <opal/mediastrm.h>


class OpalRTPSession;
class OpalRTPConnection;
class OpalMediaStatistics;


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
       This will add a reference to the rtpSession passed in.
      */
    OpalRTPMediaStream(
      OpalRTPConnection & conn,            ///<  Connection that owns the stream
      const OpalMediaFormat & mediaFormat, ///<  Media format for stream
      bool isSource,                       ///<  Is a source stream
      OpalRTPSession & rtpSession          ///<  RTP session to stream to/from
    );

    /**Destroy the media stream for RTP sessions.
       This will release the reference to the rtpSession passed into the constructor.
      */
    ~OpalRTPMediaStream();
  //@}

  /**@name Overrides of OpalMediaStream class */
  //@{
    /**Open the media stream using the media format.

       The default behaviour simply sets the isOpen variable to true.
      */
    virtual PBoolean Open();

    /**Returns true if the media stream is open.
      */
    virtual bool IsOpen() const;

    /**Callback that is called on the source stream once the media patch has started.
       The default behaviour calls OpalConnection::OnMediaPatchStart()
      */
    virtual void OnStartMediaPatch();

    /**Read an RTP frame of data from the source media stream.
       The new behaviour simply calls OpalRTPSession::ReadData().
      */
    virtual PBoolean ReadPacket(
      RTP_DataFrame & packet
    );

    /**Write an RTP frame of data to the sink media stream.
       The new behaviour simply calls OpalRTPSession::WriteData().
      */
    virtual PBoolean WritePacket(
      RTP_DataFrame & packet
    );

    /**Set the data size in bytes that is expected to be used.
      */
    virtual PBoolean SetDataSize(
      PINDEX dataSize,  ///< New data size (in total)
      PINDEX frameTime  ///< Individual frame time (if applicable)
    );

    /**Indicate if the media stream is synchronous.
       Returns false for RTP streams.
      */
    virtual PBoolean IsSynchronous() const;

    /**Indicate if the media stream requires a OpalMediaPatch thread (active patch).
       The default behaviour dermines if the media will be flowing between two
       RTP sessions within the same process. If so the
       OpalRTPConnection::OnLocalRTP() is called, and if it returns true
       indicating local handling then this function returns faklse to disable
       the patch thread.
      */
    virtual PBoolean RequiresPatchThread() const;

    /**Set the patch thread that is using this stream.
      */
    virtual PBoolean SetPatch(
      OpalMediaPatch * patch  ///<  Media patch thread
    );

    /** Return current RTP session
      */
    virtual OpalRTPSession & GetRtpSession() const
    { return m_rtpSession; }

#if OPAL_STATISTICS
    virtual void GetStatistics(OpalMediaStatistics & statistics, bool fromPatch = false) const;
#endif
  //@}

    virtual void PrintDetail(
      ostream & strm,
      const char * prefix = NULL,
      Details details = Details::All()
    ) const;

    void SetRewriteHeaders(bool v) { m_rewriteHeaders = v; }

protected:
    virtual void InternalClose();
    virtual bool InternalSetJitterBuffer(const OpalJitterBuffer::Init & init);
    virtual bool InternalUpdateMediaFormat(const OpalMediaFormat & mediaFormat);
    virtual bool InternalSetPaused(bool pause, bool fromUser, bool fromPatch);

    OpalRTPSession   & m_rtpSession;
    bool               m_rewriteHeaders;
    RTP_SyncSourceId   m_syncSource;
    OpalJitterBuffer * m_jitterBuffer;
    PReadWriteMutex    m_jitterMutex;
#if OPAL_VIDEO
    bool             m_forceIntraFrameFlag;
    PSimpleTimer     m_forceIntraFrameTimer;
#endif

    PDECLARE_RTPDataNotifier(OpalRTPMediaStream, OnReceivedPacket);
    OpalRTPSession::DataNotifier  m_receiveNotifier;
};



#endif //OPAL_RTP_RTP_STREAM_H


// End of File ///////////////////////////////////////////////////////////////
