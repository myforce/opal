/*
 * sdp.h
 *
 * Session Description Protocol
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
 * $Revision$
 * $Author$
 * $Date$
 */

#ifndef OPAL_SIP_SDP_H
#define OPAL_SIP_SDP_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include <opal_config.h>

#if OPAL_SDP

#include <opal/transports.h>
#include <opal/mediatype.h>
#include <opal/mediafmt.h>
#include <rtp/rtp_session.h>
#include <ptclib/pssl.h>


/**OpalConnection::StringOption key to a boolean indicating the SDP ptime
   parameter should be included in audio session streams. Default false.
  */
#define OPAL_OPT_OFFER_SDP_PTIME "Offer-SDP-PTime"

/**OpalConnection::StringOption key to an integer indicating the SDP
   rtpcp-fb parameter handling. A value of zero indicates it is not to be
   offerred to the remote. A value of 1 indicates it is to be offerred
   without requiring the RTP/AVPF transport, but is included in RTP/AVP.
   A value of 2 indicates it is to be only offerred in RTP/AVPF with a
   second session in RTP/AVP mode to be also offerred. Default is 1.
  */
#define OPAL_OPT_OFFER_RTCP_FB  "Offer-RTCP-FB"

/**OpalConnection::StringOption key to a boolean indicating the RTCP
   feedback commands are to be sent irrespective of the SDP rtpcp-fb
   parameter presented by the remote. Default false.
  */
#define OPAL_OPT_FORCE_RTCP_FB  "Force-RTCP-FB"

/**Enable ICE offerred in SDP.
   Defaults to false.
*/
#define OPAL_OPT_OFFER_ICE "Offer-ICE"


/////////////////////////////////////////////////////////

class SDPBandwidth : public std::map<PCaselessString, OpalBandwidth>
{
  typedef std::map<PCaselessString, OpalBandwidth> BaseClass;
  public:
    OpalBandwidth & operator[](const PCaselessString & type);
    OpalBandwidth operator[](const PCaselessString & type) const;
    friend ostream & operator<<(ostream & out, const SDPBandwidth & bw);
    bool Parse(const PString & param);
    void SetMax(const PCaselessString & type, OpalBandwidth value);
};

/////////////////////////////////////////////////////////

class SDPMediaDescription;

class SDPMediaFormat : public PObject
{
    PCLASSINFO(SDPMediaFormat, PObject);
  protected:
    SDPMediaFormat(SDPMediaDescription & parent);

  public:
    virtual bool Initialise(
      const PString & portString
    );

    virtual void Initialise(
      const OpalMediaFormat & mediaFormat
    );

    virtual void PrintOn(ostream & str) const;
    virtual PObject * Clone() const { return new SDPMediaFormat(*this); }

    RTP_DataFrame::PayloadTypes GetPayloadType() const { return m_payloadType; }

    const PCaselessString & GetEncodingName() const { return m_encodingName; }
    void SetEncodingName(const PString & v) { m_encodingName = v; }

    void SetFMTP(const PString & _fmtp); 
    PString GetFMTP() const;

    unsigned GetClockRate(void)    { return m_clockRate ; }
    void SetClockRate(unsigned  v) { m_clockRate = v; }

    void SetParameters(const PString & v) { m_parameters = v; }

    const OpalMediaFormat & GetMediaFormat() const { return m_mediaFormat; }
    OpalMediaFormat & GetWritableMediaFormat() { return m_mediaFormat; }

    virtual bool PreEncode();
    virtual bool PostDecode(const OpalMediaFormatList & mediaFormats, unsigned bandwidth);

  protected:
    virtual void SetMediaFormatOptions(OpalMediaFormat & mediaFormat) const;

    SDPMediaDescription       & m_parent;
    OpalMediaFormat             m_mediaFormat;
    RTP_DataFrame::PayloadTypes m_payloadType;
    unsigned                    m_clockRate;
    PCaselessString             m_encodingName;
    PString                     m_parameters;
    PString                     m_fmtp;
};

typedef PList<SDPMediaFormat> SDPMediaFormatList;


/////////////////////////////////////////////////////////

class SDPCommonAttributes
{
  public:
    // The following enum is arranged so it can be used as a bit mask,
    // e.g. GetDirection()&SendOnly indicates send is available
    enum Direction {
      Undefined = -1,
      Inactive,
      RecvOnly,
      SendOnly,
      SendRecv
    };

    P_DECLARE_BITWISE_ENUM(SetupType, 3, (
      SetupNotSet,
      SetupActive,
      SetupPassive,
      SetupHoldConnection
    ));

    enum ConnectionMode
    {
      ConnectionNotSet,
      ConnectionNew,
      ConnectionExisting
    };

    SDPCommonAttributes()
      : m_direction(Undefined)
#if OPAL_SRTP // DTLS
      , m_setup(SetupNotSet)
      , m_connectionMode(ConnectionNotSet)
#endif
    { }

    virtual ~SDPCommonAttributes() { }

    virtual void SetDirection(const Direction & d) { m_direction = d; }
    virtual Direction GetDirection() const { return m_direction; }

    virtual OpalBandwidth GetBandwidth(const PString & type) const { return m_bandwidth[type]; }
    virtual void SetBandwidth(const PString & type, OpalBandwidth value) { m_bandwidth[type] = value; }

    virtual const SDPBandwidth & GetBandwidth() const { return m_bandwidth; }

    virtual const RTPExtensionHeaders & GetExtensionHeaders() const { return m_extensionHeaders; }
    virtual void SetExtensionHeader(const RTPExtensionHeaderInfo & ext) { m_extensionHeaders.insert(ext); }

    virtual void ParseAttribute(const PString & value);
    virtual void SetAttribute(const PString & attr, const PString & value);

    virtual void OutputAttributes(ostream & strm) const;

#if OPAL_SRTP
    SetupType GetSetup() const { return m_setup; }
    void SetSetup(SetupType setupType) { m_setup = setupType; }
    ConnectionMode GetConnectionMode() const { return m_connectionMode; }
    void SetConnectionMode(ConnectionMode mode) { m_connectionMode = mode; }
    const PSSLCertificateFingerprint& GetFingerprint() const { return m_fingerprint; }
    void SetFingerprint(const PSSLCertificateFingerprint& fp) { m_fingerprint = fp; }
#endif

    static const PCaselessString & ConferenceTotalBandwidthType();
    static const PCaselessString & ApplicationSpecificBandwidthType();
    static const PCaselessString & TransportIndependentBandwidthType(); // RFC3890

#if OPAL_ICE
    PString GetUsername() const { return m_username; }
    PString GetPassword() const { return m_password; }
    void SetUserPass(
      const PString & username,
      const PString & password
    ) {
      m_username = username;
      m_password = password;
    }
#endif //OPAL_ICE

  protected:
    Direction           m_direction;
    SDPBandwidth        m_bandwidth;
    RTPExtensionHeaders m_extensionHeaders;
#if OPAL_SRTP // DTLS
    SetupType      m_setup;
    ConnectionMode m_connectionMode;
    PSSLCertificateFingerprint m_fingerprint;
#endif
#if OPAL_ICE
    PStringSet          m_iceOptions;
    PString             m_username;
    PString             m_password;
#endif //OPAL_ICE
};


/////////////////////////////////////////////////////////

class SDPMediaDescription : public PObject, public SDPCommonAttributes
{
    PCLASSINFO(SDPMediaDescription, PObject);
  protected:
    SDPMediaDescription();
    SDPMediaDescription(
      const OpalTransportAddress & address,
      const OpalMediaType & mediaType
    );

  public:
    virtual bool PreEncode();
    virtual void Encode(const OpalTransportAddress & commonAddr, ostream & str) const;

    virtual bool Decode(const PStringArray & tokens);
    virtual bool Decode(char key, const PString & value);
    virtual bool PostDecode(const OpalMediaFormatList & mediaFormats);

    // return the string used within SDP to identify this media type
    virtual PString GetSDPMediaType() const;

    // return the string used within SDP to identify the transport used by this media
    virtual PCaselessString GetSDPTransportType() const;

    // return the string used in factory to create session
    virtual PCaselessString GetSessionType() const;

    virtual const SDPMediaFormatList & GetSDPMediaFormats() const;

    virtual OpalMediaFormatList GetMediaFormats() const;

    virtual void AddSDPMediaFormat(SDPMediaFormat * sdpMediaFormat);

    virtual void AddMediaFormat(const OpalMediaFormat & mediaFormat);
    virtual void AddMediaFormats(const OpalMediaFormatList & mediaFormats, const OpalMediaType & mediaType);

#if OPAL_SRTP
    virtual void SetCryptoKeys(OpalMediaCryptoKeyList & cryptoKeys);
    virtual OpalMediaCryptoKeyList GetCryptoKeys() const;
    virtual bool HasCryptoKeys() const;
#endif

    virtual void SetAttribute(const PString & attr, const PString & value);
    virtual void OutputAttributes(ostream & str) const;

    virtual Direction GetDirection() const { return m_mediaAddress.IsEmpty() ? Inactive : m_direction; }

    virtual bool SetSessionInfo(const OpalMediaSession * session, const SDPMediaDescription * offer);
    virtual PString GetMediaGroupId() const { return m_mediaGroupId; }
    virtual void SetMediaGroupId(const PString & id) { m_mediaGroupId = id; }

    const OpalTransportAddress & GetMediaAddress() const { return m_mediaAddress; }
    const OpalTransportAddress & GetControlAddress() const { return m_controlAddress; }
    bool SetAddresses(const OpalTransportAddress & media, const OpalTransportAddress & control);

    virtual WORD GetPort() const { return m_port; }

#if OPAL_ICE
    PNatCandidateList GetCandidates() const { return m_candidates; }
    bool HasICE() const;
    void SetICE(
      const PString & username,
      const PString & password,
      const PNatCandidateList & candidates
    )
    {
      m_username = username;
      m_password = password;
      m_candidates = candidates;
    }
#endif //OPAL_ICE

    virtual OpalMediaType GetMediaType() const { return m_mediaType; }

    virtual void CreateSDPMediaFormats(const PStringArray & tokens);
    virtual SDPMediaFormat * CreateSDPMediaFormat() = 0;

    virtual PString GetSDPPortList() const;

    virtual void ProcessMediaOptions(SDPMediaFormat & sdpFormat, const OpalMediaFormat & mediaFormat);

#if OPAL_VIDEO
    virtual OpalVideoFormat::ContentRole GetContentRole() const { return OpalVideoFormat::eNoRole; }
#endif

    void SetOptionStrings(const PStringOptions & options) { m_stringOptions = options; }
    const PStringOptions & GetOptionStrings() const { return m_stringOptions; }

    virtual void Copy(SDPMediaDescription & mediaDescription);

  protected:
    virtual SDPMediaFormat * FindFormat(PString & str) const;

    OpalTransportAddress m_mediaAddress;
    OpalTransportAddress m_controlAddress;
    PCaselessString      m_transportType;
    PStringOptions       m_stringOptions;
    WORD                 m_port;
    WORD                 m_portCount;
    OpalMediaType        m_mediaType;
    PString              m_mediaGroupId;
#if OPAL_ICE
    PNatCandidateList    m_candidates;
#endif //OPAL_ICE
    SDPMediaFormatList   m_formats;

  P_REMOVE_VIRTUAL(SDPMediaFormat *,CreateSDPMediaFormat(const PString &),0);
  P_REMOVE_VIRTUAL(OpalTransportAddress,GetTransportAddress(),OpalTransportAddress());
  P_REMOVE_VIRTUAL(PBoolean,SetTransportAddress(const OpalTransportAddress &),false);
};

PARRAY(SDPMediaDescriptionArray, SDPMediaDescription);


class SDPDummyMediaDescription : public SDPMediaDescription
{
    PCLASSINFO(SDPDummyMediaDescription, SDPMediaDescription);
  public:
    SDPDummyMediaDescription() { }
    SDPDummyMediaDescription(const OpalTransportAddress & address, const PStringArray & tokens);
    virtual PString GetSDPMediaType() const;
    virtual PCaselessString GetSDPTransportType() const;
    virtual PCaselessString GetSessionType() const;
    virtual SDPMediaFormat * CreateSDPMediaFormat();
    virtual PString GetSDPPortList() const;
    virtual void Copy(SDPMediaDescription & mediaDescription);

  private:
    PStringArray m_tokens;
};


#if OPAL_SRTP
class SDPCryptoSuite : public PObject
{
    PCLASSINFO(SDPCryptoSuite, PObject)
  public:
    SDPCryptoSuite(unsigned tag);

    bool SetKeyInfo(const OpalMediaCryptoKeyInfo & keyInfo);
    OpalMediaCryptoKeyInfo * GetKeyInfo() const;

    bool Decode(const PString & attrib);
    void PrintOn(ostream & strm) const;

    struct KeyParam {
      KeyParam(const PString & keySalt)
        : m_keySalt(keySalt)
        , m_lifetime(0)
        , m_mkiIndex(0)
        , m_mkiLength(0)
      { }

      PString  m_keySalt;
      PUInt64  m_lifetime;
      unsigned m_mkiIndex;
      unsigned m_mkiLength;
    };

    unsigned GetTag() const { return m_tag; }
    const PString & GetName() const { return m_suiteName; }

  protected:
    unsigned       m_tag;
    PString        m_suiteName;
    list<KeyParam> m_keyParams;
    PStringOptions m_sessionParams;
};
#endif // OPAL_SRTP


/////////////////////////////////////////////////////////
//
//  SDP media description for media classes using RTP/AVP transport (audio and video)
//

class SDPRTPAVPMediaDescription : public SDPMediaDescription
{
    PCLASSINFO(SDPRTPAVPMediaDescription, SDPMediaDescription);
  public:
    SDPRTPAVPMediaDescription(const OpalTransportAddress & address, const OpalMediaType & mediaType);
    virtual bool Decode(const PStringArray & tokens);
    virtual PCaselessString GetSDPTransportType() const;
    virtual SDPMediaFormat * CreateSDPMediaFormat();
    virtual PString GetSDPPortList() const;
    virtual void OutputAttributes(ostream & str) const;

#if OPAL_SRTP
    virtual void SetCryptoKeys(OpalMediaCryptoKeyList & cryptoKeys);
    virtual OpalMediaCryptoKeyList GetCryptoKeys() const;
    virtual bool HasCryptoKeys() const;
#endif
    virtual void SetAttribute(const PString & attr, const PString & value);
    virtual bool SetSessionInfo(const OpalMediaSession * session, const SDPMediaDescription * offer);

    void EnableFeedback() { m_enableFeedback = true; }

    typedef std::map<DWORD, PStringOptions> SsrcInfo;
    const SsrcInfo & GetSsrcInfo() const { return m_ssrcInfo; }

  protected:
    class Format : public SDPMediaFormat
    {
      public:
        Format(SDPRTPAVPMediaDescription & parent) : SDPMediaFormat(parent) { }
        bool Initialise(const PString & portString);
    };

    bool     m_enableFeedback;
    SsrcInfo m_ssrcInfo;

#if OPAL_SRTP
    PList<SDPCryptoSuite> m_cryptoSuites;
    bool m_useDTLS;
#endif
};

/////////////////////////////////////////////////////////
//
//  SDP media description for audio media
//

class SDPAudioMediaDescription : public SDPRTPAVPMediaDescription
{
    PCLASSINFO(SDPAudioMediaDescription, SDPRTPAVPMediaDescription);
  public:
    SDPAudioMediaDescription(const OpalTransportAddress & address);
    virtual void OutputAttributes(ostream & str) const;
    virtual void SetAttribute(const PString & attr, const PString & value);
    virtual bool PostDecode(const OpalMediaFormatList & mediaFormats);

  protected:
    unsigned m_PTime;
    unsigned m_maxPTime;
};


#if OPAL_VIDEO

/////////////////////////////////////////////////////////
//
//  SDP media description for video media
//

class SDPVideoMediaDescription : public SDPRTPAVPMediaDescription
{
    PCLASSINFO(SDPVideoMediaDescription, SDPRTPAVPMediaDescription);
  public:
    SDPVideoMediaDescription(const OpalTransportAddress & address);
    virtual SDPMediaFormat * CreateSDPMediaFormat();
    virtual bool PreEncode();
    virtual void OutputAttributes(ostream & str) const;
    virtual void SetAttribute(const PString & attr, const PString & value);
    virtual bool PostDecode(const OpalMediaFormatList & mediaFormats);
    virtual OpalVideoFormat::ContentRole GetContentRole() const { return m_contentRole; }

  protected:
    class Format : public SDPRTPAVPMediaDescription::Format
    {
      public:
        Format(SDPVideoMediaDescription & parent);

        virtual void PrintOn(ostream & str) const;
        virtual PObject * Clone() const { return new Format(*this); }

        virtual bool PreEncode();

        void AddRTCP_FB(const PString & str);
        void SetRTCP_FB(const OpalVideoFormat::RTCPFeedback & v) { m_rtcp_fb = v; }
        OpalVideoFormat::RTCPFeedback GetRTCP_FB() const { return m_rtcp_fb; }

        void ParseImageAttr(const PString & params);

      protected:
        virtual void SetMediaFormatOptions(OpalMediaFormat & mediaFormat) const;

        OpalVideoFormat::RTCPFeedback m_rtcp_fb; // RFC4585

        unsigned m_minRxWidth;
        unsigned m_minRxHeight;
        unsigned m_maxRxWidth;
        unsigned m_maxRxHeight;
        unsigned m_minTxWidth;
        unsigned m_minTxHeight;
        unsigned m_maxTxWidth;
        unsigned m_maxTxHeight;
    };

    OpalVideoFormat::RTCPFeedback m_rtcp_fb;
    OpalVideoFormat::ContentRole  m_contentRole;
    unsigned                      m_contentMask;
};

#endif // OPAL_VIDEO


/////////////////////////////////////////////////////////
//
//  SDP media description for application media
//

class SDPApplicationMediaDescription : public SDPMediaDescription
{
  PCLASSINFO(SDPApplicationMediaDescription, SDPMediaDescription);
  public:
    SDPApplicationMediaDescription(const OpalTransportAddress & address);
    virtual PCaselessString GetSDPTransportType() const;
    virtual SDPMediaFormat * CreateSDPMediaFormat();
    virtual PString GetSDPMediaType() const;

    static const PCaselessString & TypeName();

  protected:
    class Format : public SDPMediaFormat
    {
      public:
        Format(SDPApplicationMediaDescription & parent) : SDPMediaFormat(parent) { }
        bool Initialise(const PString & portString);
    };
};

/////////////////////////////////////////////////////////

class SDPSessionDescription : public PObject, public SDPCommonAttributes
{
    PCLASSINFO_WITH_CLONE(SDPSessionDescription, PObject);
  public:
    SDPSessionDescription(
      time_t sessionId,
      unsigned version,
      const OpalTransportAddress & address
    );

    virtual void PrintOn(ostream & strm) const;
    virtual PString Encode() const;
    virtual bool Decode(const PString & str, const OpalMediaFormatList & mediaFormats);
    virtual void SetAttribute(const PString & attr, const PString & value);

    void SetSessionName(const PString & v);
    PString GetSessionName() const { return sessionName; }

    void SetUserName(const PString & v);
    PString GetUserName() const { return ownerUsername; }

    const SDPMediaDescriptionArray & GetMediaDescriptions() const { return mediaDescriptions; }

    SDPMediaDescription * GetMediaDescriptionByType(const OpalMediaType & rtpMediaType) const;
    SDPMediaDescription * GetMediaDescriptionByIndex(PINDEX i) const;
    void AddMediaDescription(SDPMediaDescription * md) { mediaDescriptions.Append(PAssertNULL(md)); }
    
    virtual SDPMediaDescription::Direction GetDirection(unsigned) const;
    bool IsHold() const;
    bool HasActiveSend() const;

    const OpalTransportAddress & GetDefaultConnectAddress() const { return defaultConnectAddress; }
    void SetDefaultConnectAddress(
      const OpalTransportAddress & address
    );
  
    time_t GetOwnerSessionId() const { return ownerSessionId; }
    void SetOwnerSessionId(time_t value) { ownerSessionId = value; }

    unsigned GetOwnerVersion() const { return ownerVersion; }
    void SetOwnerVersion(unsigned value) { ownerVersion = value; }

    OpalTransportAddress GetOwnerAddress() const { return ownerAddress; }
    void SetOwnerAddress(OpalTransportAddress addr) { ownerAddress = addr; }

    typedef PDictionary<PString, PStringArray> GroupDict;
    GroupDict GetGroups() const { return m_groups; }

#if OPAL_ICE
    PStringSet GetICEOptions() const { return m_iceOptions; }
#endif

    OpalMediaFormatList GetMediaFormats() const;

  protected:
    void ParseOwner(const PString & str);

    SDPMediaDescriptionArray mediaDescriptions;

    PINDEX protocolVersion;
    PString sessionName;

    PString ownerUsername;
    time_t ownerSessionId;
    unsigned ownerVersion;
    OpalTransportAddress ownerAddress;
    OpalTransportAddress defaultConnectAddress;

    GroupDict  m_groups;
    PString    m_groupId;
};

/////////////////////////////////////////////////////////


#endif // OPAL_SDP

#endif // OPAL_SIP_SDP_H


// End of File ///////////////////////////////////////////////////////////////
