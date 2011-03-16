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
 * $Revision$
 * $Author$
 * $Date$
 */

#ifndef OPAL_T38_T38PROTO_H
#define OPAL_T38_T38PROTO_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include <opal/buildopts.h>


#if OPAL_FAX

#include <ptlib/pipechan.h>

#include <opal/mediafmt.h>
#include <opal/mediastrm.h>
#include <opal/localep.h>


class OpalTransport;
class T38_IFPPacket;
class PASN_OctetString;
class OpalFaxConnection;


#define OPAL_OPT_STATION_ID  "Station-Id"      ///< String option for fax station ID string
#define OPAL_NO_G111_FAX     "No-G711-Fax"     ///< Suppress G.711 fall back
#define OPAL_SUPPRESS_CED    "Suppress-CED"    ///< Suppress transmission of CED tone
#define OPAL_IGNORE_CED      "Ignore-CED"      ///< Ignore receipt of CED tone
#define OPAL_T38_SWITCH_TIME "T38-Switch-Time" ///< Seconds for fail safe switch to T.38 mode

#define OPAL_FAX_TIFF_FILE "TIFF-File"


///////////////////////////////////////////////////////////////////////////////

class OpalFaxConnection;

/** Fax Endpoint.
    This class represents connection that can take a standard group 3 fax
    TIFF file and produce either T.38 packets or actual tones represented
    by a stream of PCM. For T.38 it is expected the second connection in the
    call supports T.38 e.g. SIP or H.323. If PCM is being used then the second
    connection may be anything that supports PCM, such as SIP or H.323 using
    G.711 codec or OpalLineEndpoint which could the send the TIFF file to a
    physical fax machine.

    Relies on the presence of the spandsp plug in to do the hard work.
 */
class OpalFaxEndPoint : public OpalLocalEndPoint
{
  PCLASSINFO(OpalFaxEndPoint, OpalLocalEndPoint);
  public:
  /**@name Construction */
  //@{
    /**Create a new endpoint.
     */
    OpalFaxEndPoint(
      OpalManager & manager,        ///<  Manager of all endpoints.
      const char * g711Prefix = "fax", ///<  Prefix for URL style address strings
      const char * t38Prefix = "t38"  ///<  Prefix for URL style address strings
    );

    /**Destroy endpoint.
     */
    ~OpalFaxEndPoint();
  //@}

  /**@name Overrides from OpalEndPoint */
  //@{
    virtual PSafePtr<OpalConnection> MakeConnection(
      OpalCall & call,          ///<  Owner of connection
      const PString & party,    ///<  Remote party to call
      void * userData = NULL,          ///<  Arbitrary data to pass to connection
      unsigned int options = 0,     ///<  options to pass to conneciton
      OpalConnection::StringOptions * stringOptions = NULL  ///< Options to pass to connection
    );

    /**Get the data formats this endpoint is capable of operating.
       This provides a list of media data format names that may be used by an
       OpalMediaStream may be created by a connection from this endpoint.

       Note that a specific connection may not actually support all of the
       media formats returned here, but should return no more.
      */
    virtual OpalMediaFormatList GetMediaFormats() const;
  //@}

  /**@name Fax specific operations */
  //@{
    /**Determine if the fax plug in is available, that is fax system can be used.
      */
    virtual bool IsAvailable() const;

    /**Create a connection for the fax endpoint.
      */
    virtual OpalFaxConnection * CreateConnection(
      OpalCall & call,          ///< Owner of connection
      void * userData,          ///<  Arbitrary data to pass to connection
      OpalConnection::StringOptions * stringOptions, ///< Options to pass to connection
      const PString & filename, ///< filename to send/receive
      bool receiving,           ///< Flag for receiving/sending fax
      bool disableT38           ///< Flag to disable use of T.38
    );

    /**Fax transmission/receipt completed.
       Default behaviour releases the connection.
      */
    virtual void OnFaxCompleted(
      OpalFaxConnection & connection, ///< Connection that completed.
      bool failed   ///< Fax ended with failure
    );
  //@}

  /**@name Member variable access */
    /**Get the default directory for received faxes.
      */
    const PString & GetDefaultDirectory() const { return m_defaultDirectory; }

    /**Set the default directory for received faxes.
      */
    void SetDefaultDirectory(
      const PString & dir    /// New directory for fax reception
    ) { m_defaultDirectory = dir; }

    const PString & GetT38Prefix() const { return m_t38Prefix; }
  //@}

  protected:
    PString    m_t38Prefix;
    PDirectory m_defaultDirectory;
};


///////////////////////////////////////////////////////////////////////////////

/** Fax Connection.
    There are six modes of operation:
        Mode            receiving     disableT38    filename
        TIFF -> T.38      false         false       "something.tif"
        T.38 -> TIFF      true          false       "something.tif"
        TIFF -> G.711     false         true        "something.tif"
        G.711 ->TIFF      true          true        "something.tif"
        T.38  -> G.711    false       don't care    PString::Empty()
        G.711 -> T.38     true        don't care    PString::Empty()

    If T.38 is involved then there is generally two stages to the setup, as
    indicated by the m_switchedToT38 flag. When false then we are in audio
    mode looking for CNG/CED tones. When true, then we are switching, or have
    switched, to T.38 operation. If the switch fails, then the m_disableT38
    is set and we proceed in fall back mode.
 */
class OpalFaxConnection : public OpalLocalConnection
{
  PCLASSINFO(OpalFaxConnection, OpalLocalConnection);
  public:
  /**@name Construction */
  //@{
    /**Create a new endpoint.
     */
    OpalFaxConnection(
      OpalCall & call,                 ///< Owner calll for connection
      OpalFaxEndPoint & endpoint,      ///< Owner endpoint for connection
      const PString & filename,        ///< TIFF file name to send/receive
      bool receiving,                  ///< True if receiving a fax
      bool disableT38,                 ///< True if want to force G.711
      OpalConnection::StringOptions * stringOptions = NULL  ///< Options to pass to connection
    );

    /**Destroy endpoint.
     */
    ~OpalFaxConnection();
  //@}

  /**@name Overrides from OpalLocalConnection */
  //@{
    virtual PString GetPrefixName() const;

    virtual void OnApplyStringOptions();
    virtual OpalMediaFormatList GetMediaFormats() const;
    virtual void AdjustMediaFormats(bool local, const OpalConnection * otherConnection, OpalMediaFormatList & mediaFormats) const;
    virtual void AcceptIncoming();
    virtual void OnEstablished();
    virtual void OnReleased();
    virtual OpalMediaStream * CreateMediaStream(const OpalMediaFormat & mediaFormat, unsigned sessionID, PBoolean isSource);
    virtual void OnStartMediaPatch(OpalMediaPatch & patch);
    virtual void OnStopMediaPatch(OpalMediaPatch & patch);
    virtual PBoolean SendUserInputTone(char tone, unsigned duration);
    virtual void OnUserInputTone(char tone, unsigned duration);
    virtual bool SwitchFaxMediaStreams(bool enableFax);
    virtual void OnSwitchedFaxMediaStreams(bool enabledFax);
  //@}

  /**@name New operations */
  //@{
    /**Fax transmission/receipt completed.
       Default behaviour calls equivalent function on OpalFaxEndPoint.
      */
    virtual void OnFaxCompleted(
      bool failed   ///< Fax ended with failure
    );

#if OPAL_STATISTICS
    /**Get fax transmission/receipt statistics.
      */
    virtual void GetStatistics(
      OpalMediaStatistics & statistics  ///< Statistics for call
    ) const;
#endif

    /**Get the file to send/receive
      */
    const PString & GetFileName() const { return m_filename; }

    /**Get receive fax flag.
      */
    bool IsReceive() const { return m_receiving; }
  //@}

  protected:
    PDECLARE_NOTIFIER(PTimer,  OpalFaxConnection, OnSendCNGCED);
    PDECLARE_NOTIFIER(PThread, OpalFaxConnection, OpenFaxStreams);


    OpalFaxEndPoint & m_endpoint;
    PString           m_filename;
    bool              m_receiving;
    PString           m_stationId;
    bool              m_disableT38;
    PTimeInterval     m_releaseTimeout;
    PTimeInterval     m_switchTimeout;
    OpalMediaFormat   m_tiffFileFormat;
#if OPAL_STATISTICS
    void InternalGetStatistics(OpalMediaStatistics & statistics, bool terminate) const;
    OpalMediaStatistics m_finalStatistics;
#endif

    enum {
      e_AwaitingSwitchToT38,
      e_SwitchingToT38,
      e_CompletedSwitch
    } m_state;
    PTimer   m_faxTimer;

  friend class OpalFaxMediaStream;
};


typedef OpalFaxConnection OpalT38Connection; // For backward compatibility


#endif // OPAL_FAX

#endif // OPAL_T38_T38PROTO_H
