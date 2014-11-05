/*
 * mediasession.h
 *
 * Media session abstraction
 *
 * Open Phone Abstraction Library (OPAL)
 *
 * Copyright (C) 2010 Vox Lucida Pty. Ltd.
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
 * The Initial Developer of the Original Code is Vox Lucida Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#ifndef OPAL_OPAL_MEDIASESSION_H
#define OPAL_OPAL_MEDIASESSION_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include <opal_config.h>

#include <opal/transports.h>
#include <opal/mediatype.h>


class OpalConnection;
class OpalMediaStream;
class OpalMediaFormat;
class OpalMediaFormatList;
class OpalMediaCryptoSuite;
class H235SecurityCapability;
class H323Capability;


#if OPAL_STATISTICS

/**This class carries statistics on the media stream.
  */
class OpalMediaStatistics : public PObject
{
    PCLASSINFO(OpalMediaStatistics, PObject);
  public:
    OpalMediaStatistics();

    OpalMediaStatistics & Update(const OpalMediaStream & stream);
    virtual void PrintOn(ostream & strm) const;

    OpalMediaType m_mediaType;
    PString       m_mediaFormat;

    PTimeInterval m_updateInterval;
    PTimeInterval m_lastUpdateTime;

    // General info (typicallly from RTP)
    PTime    m_startTime;
    PUInt64  m_totalBytes;
    PUInt64  m_deltaBytes;
    unsigned m_totalPackets;
    unsigned m_deltaPackets;
    unsigned m_NACKs;
    unsigned m_packetsLost;
    unsigned m_packetsOutOfOrder;
    unsigned m_packetsTooLate;
    unsigned m_packetOverruns;
    unsigned m_minimumPacketTime; // Milliseconds
    unsigned m_averagePacketTime; // Milliseconds
    unsigned m_maximumPacketTime; // Milliseconds
    unsigned m_roundTripTime;     // Milliseconds

    // Audio
    unsigned m_averageJitter;     // Milliseconds
    unsigned m_maximumJitter;     // Milliseconds
    unsigned m_jitterBufferDelay; // Milliseconds

#if OPAL_VIDEO
    // Video
    unsigned m_totalFrames;
    unsigned m_deltaFrames;
    unsigned m_keyFrames;
    unsigned m_updateRequests;
    int      m_quality; // -1 is none, 0 is very good > 0 is progressively worse
#endif

    // Fax
#if OPAL_FAX
    enum {
      FaxNotStarted = -2,
      FaxInProgress = -1,
      FaxSuccessful = 0,
      FaxErrorBase  = 1
    };
    enum FaxCompression {
      FaxCompressionUnknown,
      FaxCompressionT4_1d,
      FaxCompressionT4_2d,
      FaxCompressionT6,
    };
    friend ostream & operator<<(ostream & strm, FaxCompression compression);
    struct Fax {
      Fax();

      int  m_result;      // -2=not started, -1=progress, 0=success, >0=ended with error
      char m_phase;       // 'A', 'B', 'D'
      int  m_bitRate;     // e.g. 14400, 9600
      FaxCompression m_compression; // 0=N/A, 1=T.4 1d, 2=T.4 2d, 3=T.6
      bool m_errorCorrection;
      int  m_txPages;
      int  m_rxPages;
      int  m_totalPages;
      int  m_imageSize;   // In bytes
      int  m_resolutionX; // Pixels per inch
      int  m_resolutionY; // Pixels per inch
      int  m_pageWidth;
      int  m_pageHeight;
      int  m_badRows;     // Total number of bad rows
      int  m_mostBadRows; // Longest run of bad rows
      int  m_errorCorrectionRetries;

      PString m_stationId; // Remote station identifier
      PString m_errorText;
    } m_fax;
#endif // OPAL_FAX
};

#endif


/** Class for contianing the cryptographic keys for use by OpalMediaCryptoSuite.
  */
class OpalMediaCryptoKeyInfo : public PObject
{
    PCLASSINFO(OpalMediaCryptoKeyInfo, PObject);
  protected:
    OpalMediaCryptoKeyInfo(const OpalMediaCryptoSuite & cryptoSuite) : m_cryptoSuite(cryptoSuite) { }

  public:
    virtual ~OpalMediaCryptoKeyInfo() { }

    virtual bool IsValid() const = 0;
    virtual void Randomise() = 0;
    virtual bool FromString(const PString & str) = 0;
    virtual PString ToString() const = 0;

    virtual bool SetCipherKey(const PBYTEArray & key) = 0;
    virtual bool SetAuthSalt(const PBYTEArray & key) = 0;

    virtual PBYTEArray GetCipherKey() const = 0;
    virtual PBYTEArray GetAuthSalt() const = 0;

    const OpalMediaCryptoSuite & GetCryptoSuite() const { return m_cryptoSuite; }

    void SetTag(const PString & tag) { m_tag = tag; }
    const PString & GetTag() const { return m_tag; }

  protected:
    const OpalMediaCryptoSuite & m_cryptoSuite;
    PString m_tag;
};

struct OpalMediaCryptoKeyList : PList<OpalMediaCryptoKeyInfo>
{
  void Select(iterator & it);
};


/** Class for desribing the crytpographic mechanism used by an OpalMediaSession.
    These are singletons that descibe the cypto suite in use
  */
class OpalMediaCryptoSuite : public PObject
{
    PCLASSINFO(OpalMediaCryptoSuite, PObject);
  protected:
    OpalMediaCryptoSuite() { }

  public:
    static const PCaselessString & ClearText();

    virtual const PCaselessString & GetFactoryName() const = 0;
    virtual bool Supports(const PCaselessString & proto) const = 0;
    P_DECLARE_BITWISE_ENUM(KeyExchangeModes, 3, (e_NoMode, e_AllowClear, e_SecureSignalling, e_InBandKeyEchange));
    virtual bool ChangeSessionType(PCaselessString & mediaSession, KeyExchangeModes modes) const = 0;

    virtual const char * GetDescription() const = 0;
#if OPAL_H235_6 || OPAL_H235_8
    virtual H235SecurityCapability * CreateCapability(const H323Capability & mediaCapability) const;
    virtual const char * GetOID() const = 0;
    static OpalMediaCryptoSuite * FindByOID(const PString & oid);
#endif

    PINDEX GetCipherKeyBytes() const { return (GetCipherKeyBits()+7)/8; }
    virtual PINDEX GetCipherKeyBits() const = 0;
    virtual PINDEX GetAuthSaltBits() const = 0;

    virtual OpalMediaCryptoKeyInfo * CreateKeyInfo() const = 0;

    struct List : PList<OpalMediaCryptoSuite>
    {
      List() { DisallowDeleteObjects(); }
    };
    static List FindAll(
      const PStringArray & cryptoSuiteNames,
      const char * prefix = NULL
    );

  private:
    P_REMOVE_VIRTUAL(H235SecurityCapability *,CreateCapability(const OpalMediaFormat &, unsigned) const,0);
};

typedef PFactory<OpalMediaCryptoSuite, PCaselessString> OpalMediaCryptoSuiteFactory;


/** Class for carrying media session information
  */
class OpalMediaSession : public PSafeObject
{
    PCLASSINFO(OpalMediaSession, PSafeObject);
  public:
    /// Initialisation information for constructing a session
    struct Init {
      Init(
        OpalConnection & connection,     ///< Connection that owns the sesion
        unsigned sessionId,              ///< Unique (in connection) session ID for session
        const OpalMediaType & mediaType, ///< Media type for session
        bool remoteBehindNAT
      ) : m_connection(connection)
        , m_sessionId(sessionId)
        , m_mediaType(mediaType)
        , m_remoteBehindNAT(remoteBehindNAT)
      { }


      OpalConnection & m_connection;
      unsigned         m_sessionId;
      OpalMediaType    m_mediaType;
      bool             m_remoteBehindNAT;
    };

  protected:
    OpalMediaSession(const Init & init);

  public:
    virtual void PrintOn(ostream & strm) const;

    virtual const PCaselessString & GetSessionType() const = 0;
    virtual bool Open(const PString & localInterface, const OpalTransportAddress & remoteAddress, bool isMediaAddress);
    virtual bool IsOpen() const;
    virtual bool Close();
    virtual OpalTransportAddress GetLocalAddress(bool isMediaAddress = true) const;
    virtual OpalTransportAddress GetRemoteAddress(bool isMediaAddress = true) const;
    virtual bool SetRemoteAddress(const OpalTransportAddress & remoteAddress, bool isMediaAddress = true);

    typedef PList<PChannel> Transport;
    virtual void AttachTransport(Transport & transport);
    virtual Transport DetachTransport();

    virtual bool UpdateMediaFormat(
      const OpalMediaFormat & mediaFormat
    );

#if OPAL_SDP
    virtual SDPMediaDescription * CreateSDPMediaDescription();
#endif

    virtual OpalMediaStream * CreateMediaStream(
      const OpalMediaFormat & mediaFormat,
      unsigned sessionID, 
      bool isSource
    ) = 0;

#if OPAL_STATISTICS
    virtual void GetStatistics(OpalMediaStatistics & statistics, bool receiver) const;
#endif

    void OfferCryptoSuite(const PString & cryptoSuite);
    virtual OpalMediaCryptoKeyList & GetOfferedCryptoKeys();

    virtual bool ApplyCryptoKey(
      OpalMediaCryptoKeyList & keys,
      bool rx
    );

    virtual bool IsCryptoSecured(bool rx) const;

    OpalConnection & GetConnection() const { return m_connection; }

    unsigned GetSessionID() const { return m_sessionId; }
    const OpalMediaType & GetMediaType() const { return m_mediaType; }

#if OPAL_ICE
    virtual void SetICE(
      const PString & user,
      const PString & pass,
      const PNatCandidateList & candidates
    );
    virtual bool GetICE(
      PString & user,
      PString & pass,
      PNatCandidateList & candidates
    );
#endif

  protected:
    OpalConnection & m_connection;
    unsigned         m_sessionId;  // unique session ID
    OpalMediaType    m_mediaType;  // media type for session
#if OPAL_ICE
    PString          m_localUsername;    // ICE username sent to remote
    PString          m_localPassword;    // ICE password sent to remote
    PString          m_remoteUsername;   // ICE username expected from remote
    PString          m_remotePassword;   // ICE password expected from remote
#endif

    OpalMediaCryptoKeyList m_offeredCryptokeys;

  private:
    OpalMediaSession(const OpalMediaSession & other) : PSafeObject(other), m_connection(other.m_connection) { }
    void operator=(const OpalMediaSession &) { }

    P_REMOVE_VIRTUAL(bool, Open(const PString &), false);
    P_REMOVE_VIRTUAL(OpalTransportAddress, GetLocalMediaAddress() const, 0);
    P_REMOVE_VIRTUAL(OpalTransportAddress, GetRemoteMediaAddress() const, 0);
    P_REMOVE_VIRTUAL(bool, SetRemoteMediaAddress(const OpalTransportAddress &), false);
    P_REMOVE_VIRTUAL(OpalTransportAddress, GetRemoteControlAddress() const, 0);
    P_REMOVE_VIRTUAL(bool, SetRemoteControlAddress(const OpalTransportAddress &), false);
    P_REMOVE_VIRTUAL_VOID(SetRemoteUserPass(const PString &, const PString &));
};


/**Dummy session.
   This is a place holder for the local and remote address in use for a
   session, but there is no actual implementation that does anything. It
   is used for cases such as unknown media types in SDP or external "bypassed"
   media sessions where data is sent driectly between two remote endpoints and
   not throught the local machine at all.
  */
class OpalDummySession : public OpalMediaSession
{
    PCLASSINFO(OpalDummySession, OpalMediaSession)
  public:
    OpalDummySession(const Init & init, const OpalTransportAddressArray & transports = OpalTransportAddressArray());
    static const PCaselessString & SessionType();
    virtual const PCaselessString & GetSessionType() const;
    virtual bool Open(const PString & localInterface, const OpalTransportAddress & remoteAddress, bool isMediaAddress);
    virtual bool IsOpen() const;
    virtual OpalTransportAddress GetLocalAddress(bool isMediaAddress = true) const;
    virtual OpalTransportAddress GetRemoteAddress(bool isMediaAddress = true) const;
    virtual bool SetRemoteAddress(const OpalTransportAddress & remoteAddress, bool isMediaAddress = true);
#if OPAL_SDP
    virtual SDPMediaDescription * CreateSDPMediaDescription();
#endif
    virtual OpalMediaStream * CreateMediaStream(const OpalMediaFormat & mediaFormat, unsigned sessionID, bool isSource);

  private:
    OpalTransportAddress m_localTransportAddress[2];
    OpalTransportAddress m_remoteTransportAddress[2];
};


typedef PParamFactory<OpalMediaSession, const OpalMediaSession::Init &, PCaselessString> OpalMediaSessionFactory;

#if OPAL_SRTP
  PFACTORY_LOAD(OpalSRTPSession);
#endif

#if OPAL_H235_6
  PFACTORY_LOAD(H2356_Session);
#endif

#endif // OPAL_OPAL_MEDIASESSION_H
