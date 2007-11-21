/*
 * lid.h
 *
 * Line Interface Device
 *
 * Open Phone Abstraction Library
 *
 * Copyright (c) 1999-2001 Equivalence Pty. Ltd.
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
 * Portions of this code were written with the assisance of funding from 
 * Quicknet Technologies, Inc. http://www.quicknet.net.
 * 
 * Contributor(s): ______________________________________.
 *
 * $Id$
 */

#ifndef __OPAL_LID_H
#define __OPAL_LID_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


#include <opal/mediafmt.h>


///////////////////////////////////////////////////////////////////////////////

/** Line Interface Device abstraction.
    Note all functions in this device abstraction are assumed to be thread atomic.
 */
class OpalLineInterfaceDevice : public PObject
{
  PCLASSINFO(OpalLineInterfaceDevice, PObject);

  public:
    /**Construct a new line interface device.
      */
    OpalLineInterfaceDevice();

    /**Open the line interface device.
      */
    virtual BOOL Open(
      const PString & device      ///<  Device identifier name.
    ) = 0;

    /**Determine if the line interface device is open.
      */
    virtual BOOL IsOpen() const;

    /**Close the line interface device.
      */
    virtual BOOL Close();

    /**Get the device type identifier.
       This is as is used in the factory registration.
      */
    virtual PString GetDeviceType() const = 0;

    /**Get the device name, as used to open the device.
       Note the format of this name should be as is returned from GetAllName()
       and must be able to be used in a subsequent Open() call.
      */
    virtual PString GetDeviceName() const = 0;

    /**Get all the possible devices that can be opened.
      */
    virtual PStringArray GetAllNames() const = 0;

    /**Get the description of the line interface device.
       This is a string indication of the card type for user interface
       display purposes or device specific control. The device should be
       as detailed as possible eg "Quicknet LineJACK".
      */
    virtual PString GetDescription() const = 0;

    /**Get the total number of lines supported by this device.
      */
    virtual unsigned GetLineCount() = 0;

    /**Get the type of the line.
       A "terminal" line is one where a call may terminate. For example a POTS
       line with a standard telephone handset on it would be a terminal line.
       The alternative is a "network" line, that is one connected to switched
       network eg the standard PSTN.
      */
    virtual BOOL IsLineTerminal(
      unsigned line   ///<  Number of line
    ) = 0;


    /**Determine if a physical line is present on the logical line.
      */
    virtual BOOL IsLinePresent(
      unsigned line,      ///<  Number of line
      BOOL force = FALSE  ///<  Force test, do not optimise
    );


    /**Determine if line is currently off hook.
       This function implies that the state is debounced and that a return
       value of TRUE indicates that the phone is really off hook. That is
       hook flashes and winks are masked out.
      */
    virtual BOOL IsLineOffHook(
      unsigned line   ///<  Number of line
    ) = 0;

    /**Set the hook state of the line.
       Note that not be possible on a given line, for example a POTS line with
       a standard telephone handset. The hook state is determined by external
       hardware and cannot be changed by the software.
      */
    virtual BOOL SetLineOffHook(
      unsigned line,        ///<  Number of line
      BOOL newState = TRUE  ///<  New state to set
    ) = 0;

    /**Set the hook state of the line.
       This is the complement of SetLineOffHook().
      */
    virtual BOOL SetLineOnHook(
      unsigned line        ///<  Number of line
    ) { return SetLineOffHook(line, FALSE); }

    /**Set the hook state off then straight back on again.
       This will only operate if the line is currently off hook.
      */
    virtual BOOL HookFlash(
      unsigned line,              ///<  Number of line
      unsigned flashTime = 200    ///<  Time for hook flash in milliseconds
    );

    /**Return TRUE if a hook flash has been detected
      */
    virtual BOOL HasHookFlash(unsigned line);


    /**Determine if line is ringing.
       This function implies that the state is "debounced" and that a return
       value of TRUE indicates that the phone is still ringing and it is not
       simply a pause in the ring cadence.

       If cadence is not NULL then it is set with the bit pattern for the
       incoming ringing. Note that in this case the funtion may take a full
       sequence to return. If it is NULL it can be assumed that the function
       will return quickly.
      */
    virtual BOOL IsLineRinging(
      unsigned line,          ///<  Number of line
      DWORD * cadence = NULL  ///<  Cadence of incoming ring
    );

    /**Begin ringing local phone set with specified cadence.
       If nCadence is zero then stops ringing.

       Note that this may not be possible on a given line, for example on a
       PSTN line the ring state is determined by external hardware and cannot
       be changed by the software.

       Also note that the cadence may be ignored by particular hardware driver
       so that only the zero or non-zero values are significant.

       The ring pattern is an array of millisecond times for on and off parts
       of the cadence. Thus the Australian ring cadence would be represented
       by the array   unsigned AusRing[] = { 400, 200, 400, 2000 }

       If the nCadence in non-zero and the pattern parameter is NULL, then
       the standard ring pattern for the selected country is used.
      */
    virtual BOOL RingLine(
      unsigned line,                   ///< Number of line
      PINDEX nCadence,                 ///< Number of entries in cadence array
      const unsigned * pattern = NULL, ///< Ring pattern times
      unsigned frequency = 400         ///< Frequency of ring (if relevant)
    );


    /**Determine if line has been disconnected from a call.
       This uses the hardware (and country) dependent means for determining
       if the remote end of a PSTN connection has hung up.

       For a POTS port this is equivalent to !IsLineOffHook().
      */
    virtual BOOL IsLineDisconnected(
      unsigned line,   ///<  Number of line
      BOOL checkForWink = TRUE
    );


    /**Directly connect the two lines.
      */
    virtual BOOL SetLineToLineDirect(
      unsigned line1,   ///<  Number of first line
      unsigned line2,   ///<  Number of second line
      BOOL connect      ///<  Flag for connect/disconnect
    );

    /**Determine if the two lines are directly connected.
      */
    virtual BOOL IsLineToLineDirect(
      unsigned line1,   ///<  Number of first line
      unsigned line2    ///<  Number of second line
    );


    /**Get the media formats this device is capable of using.
      */
    virtual OpalMediaFormatList GetMediaFormats() const = 0;

    /**Set the media format (codec) for reading on the specified line.
      */
    virtual BOOL SetReadFormat(
      unsigned line,    ///<  Number of line
      const OpalMediaFormat & mediaFormat   ///<  Codec type
    ) = 0;

    /**Set the media format (codec) for writing on the specified line.
      */
    virtual BOOL SetWriteFormat(
      unsigned line,    ///<  Number of line
      const OpalMediaFormat & mediaFormat   ///<  Codec type
    ) = 0;

    /**Get the media format (codec) for reading on the specified line.
      */
    virtual OpalMediaFormat GetReadFormat(
      unsigned line    ///<  Number of line
    ) = 0;

    /**Get the media format (codec) for writing on the specified line.
      */
    virtual OpalMediaFormat GetWriteFormat(
      unsigned line    ///<  Number of line
    ) = 0;

    /**Stop the read codec.
      */
    virtual BOOL StopReading(
      unsigned line   ///<  Number of line
    );

    /**Stop the write codec.
      */
    virtual BOOL StopWriting(
      unsigned line   ///<  Number of line
    );

    /**Set the read frame size in bytes.
       Note that a LID may ignore this value so always use GetReadFrameSize()
       for I/O.
      */
    virtual BOOL SetReadFrameSize(
      unsigned line,    ///<  Number of line
      PINDEX frameSize  ///<  New frame size
    ) = 0;

    /**Set the write frame size in bytes.
       Note that a LID may ignore this value so always use GetReadFrameSize()
       for I/O.
      */
    virtual BOOL SetWriteFrameSize(
      unsigned line,    ///<  Number of line
      PINDEX frameSize  ///<  New frame size
    ) = 0;

    /**Get the read frame size in bytes.
       All calls to ReadFrame() will return this number of bytes.
      */
    virtual PINDEX GetReadFrameSize(
      unsigned line   ///<  Number of line
    ) = 0;

    /**Get the write frame size in bytes.
       All calls to WriteFrame() must be this number of bytes.
      */
    virtual PINDEX GetWriteFrameSize(
      unsigned line   ///<  Number of line
    ) = 0;

    /**Low level read of a frame from the device.
     */
    virtual BOOL ReadFrame(
      unsigned line,    ///<  Number of line
      void * buf,       ///<  Pointer to a block of memory to receive data.
      PINDEX & count    ///<  Number of bytes read, <= GetReadFrameSize()
    ) = 0;

    /**Low level write frame to the device.
     */
    virtual BOOL WriteFrame(
      unsigned line,    ///<  Number of line
      const void * buf, ///<  Pointer to a block of memory to write.
      PINDEX count,     ///<  Number of bytes to write, <= GetWriteFrameSize()
      PINDEX & written  ///<  Number of bytes written, <= GetWriteFrameSize()
    ) = 0;

    /**High level read of audio data from the device.
       This version will allow non-integral number of frames to be read.
     */
    virtual BOOL ReadBlock(
      unsigned line,    ///<  Number of line
      void * buf,   ///<  Pointer to a block of memory to receive the read bytes.
      PINDEX count  ///<  Count of bytes to read.
    );

    /**High level write audio data to the device.
     */
    virtual BOOL WriteBlock(
      unsigned line,    ///<  Number of line
      const void * buf, ///<  Pointer to a block of memory to write.
      PINDEX count      ///<  Count of bytes to write.
    );


    /**Get average signal level in last frame.
      */
    virtual unsigned GetAverageSignalLevel(
      unsigned line,  ///<  Number of line
      BOOL playback   ///<  Get average playback or record level.
    );


    /**Enable audio for the line.
      */
    virtual BOOL EnableAudio(
      unsigned line,      ///<  Number of line
      BOOL enable = TRUE
    );

    /**Disable audio for the line.
      */
    BOOL DisableAudio(
      unsigned line   ///<  Number of line
    ) { return EnableAudio(line, FALSE); }

    /**Determine if audio for the line is enabled.
      */
    virtual BOOL IsAudioEnabled(
      unsigned line      ///<  Number of line
    );


    enum {
      MaxVolume = 100
    };

    /**Set volume level for recording.
       A value of 100 is the maximum volume possible for the hardware.
       A value of 0 is the minimum volume possible for the hardware.
      */
    virtual BOOL SetRecordVolume(
      unsigned line,    ///<  Number of line
      unsigned volume   ///<  Volume level from 0 to 100%
    );

    /**Set volume level for playing.
       A value of 100 is the maximum volume possible for the hardware.
       A value of 0 is the minimum volume possible for the hardware.
      */
    virtual BOOL SetPlayVolume(
      unsigned line,    ///<  Number of line
      unsigned volume   ///<  Volume level from 0 to 100%
    );

    /**Get volume level for recording.
       A value of 100 is the maximum volume possible for the hardware.
       A value of 0 is the minimum volume possible for the hardware.
      */
    virtual BOOL GetRecordVolume(
      unsigned line,      ///<  Number of line
      unsigned & volume   ///<  Volume level from 0 to 100%
    );

    /**Set volume level for playing.
       A value of 100 is the maximum volume possible for the hardware.
       A value of 0 is the minimum volume possible for the hardware.
      */
    virtual BOOL GetPlayVolume(
      unsigned line,      ///<  Number of line
      unsigned & volume   ///<  Volume level from 0 to 100%
    );


    enum AECLevels {
      AECOff,
      AECLow,
      AECMedium,
      AECHigh,
      AECAuto,
      AECAGC,
      AECError
    };

    /**Get acoustic echo cancellation.
       Note, not all devices may support this function.
      */
    virtual AECLevels GetAEC(
      unsigned line    ///<  Number of line
    );

    /**Set acoustic echo cancellation.
       Note, not all devices may support this function.
      */
    virtual BOOL SetAEC(
      unsigned line,    ///<  Number of line
      AECLevels level   ///<  AEC level
    );

    /**Get voice activity detection.
       Note, not all devices, or selected codecs, may support this function.
      */
    virtual BOOL GetVAD(
      unsigned line    ///<  Number of line
    );

    /**Set voice activity detection.
       Note, not all devices, or selected codecs, may support this function.
      */
    virtual BOOL SetVAD(
      unsigned line,    ///<  Number of line
      BOOL enable       ///<  Flag for enabling VAD
    );


    /**Get Caller ID from the last incoming ring.
       The idString parameter is either simply the "number" field of the caller
       ID data, or if full is TRUE, all of the fields in the caller ID data.

       The full data of the caller ID string consists of the number field, the
       time/date and the name field separated by tabs ('\t').
      */
    virtual BOOL GetCallerID(
      unsigned line,      ///<  Number of line
      PString & idString, ///<  ID string returned
      BOOL full = FALSE   ///<  Get full information in idString
    );

    /**Set Caller ID for use in next RingLine() call.
       The full data of the caller ID string consists of the number field, the
       time/date and the name field separated by tabs ('\t').

       If the date field is missing (two consecutive tabs) then the current
       time and date is used. Using an empty string will clear the caller ID
       so that no caller ID is sent on the next RingLine() call.
      */
    virtual BOOL SetCallerID(
      unsigned line,            ///<  Number of line
      const PString & idString  ///<  ID string to use
    );

    /**Send a Caller ID on call waiting command
       The full data of the caller ID string consists of the number field, the
       time/date and the name field separated by tabs ('\t').

       If the date field is missing (two consecutive tabs) then the current
       time and date is used. Using an empty string will clear the caller ID
       so that no caller ID is sent on the next RingLine() call.
      */
    virtual BOOL SendCallerIDOnCallWaiting(
      unsigned line,            ///<  Number of line
      const PString & idString  ///<  ID string to use
    );

    /**Send a Visual Message Waiting Indicator
      */
    virtual BOOL SendVisualMessageWaitingIndicator(
      unsigned line,            ///<  Number of line
      BOOL on
    );


    enum {
      DefaultDTMFOnTime = 180,
      DefaultDTMFOffTime = 120
    };

    /**Play a DTMF digit.
       Any characters that are not in the set 0-9, A-D, * or # will be ignored.
      */
    virtual BOOL PlayDTMF(
      unsigned line,            ///<  Number of line
      const char * digits,      ///<  DTMF digits to be played
      DWORD onTime = DefaultDTMFOnTime,  ///<  Number of milliseconds to play each DTMF digit
      DWORD offTime = DefaultDTMFOffTime ///<  Number of milliseconds between digits
    );

    /**Read a DTMF digit detected.
       This may be characters from the set 0-9, A-D, * or #. A null ('\0')
       character indicates that there are no tones in the queue.
       Characters E through P indicate the following tones:

         E = 800   F = 1000  G = 1250  H = 950   I = 1100  J = 1400
         K = 1500  L = 1600  M = 1800  N = 2100  O = 1300  P = 2450

      */
    virtual char ReadDTMF(
      unsigned line   ///<  Number of line
    );

    /**Get DTMF removal mode.
       When set in this mode the DTMF tones detected are removed from the
       encoded data stream as returned by ReadFrame().
      */
    virtual BOOL GetRemoveDTMF(
      unsigned line   ///<  Number of line
    );

    /**Set DTMF removal mode.
       When set in this mode the DTMF tones detected are removed from the
       encoded data stream as returned by ReadFrame().
      */
    virtual BOOL SetRemoveDTMF(
      unsigned line,     ///<  Number of line
      BOOL removeTones   ///<  Flag for removing DTMF tones.
    );


    enum CallProgressTones {
      NoTone = -1, // indicates no tones
      DialTone,    // Dial tone
      RingTone,    // Ring indication tone
      BusyTone,    // Line engaged tone
      CongestionTone,// aka fast busy tone
      ClearTone,   // Call failed/disconnected tone (often same as busy tone)
      MwiTone,     // Message Waiting Tone
      CNGTone,     // Fax CNG tone
      NumTones
    };

    /**See if any tone is detected.
      */
    virtual CallProgressTones IsToneDetected(
      unsigned line   ///< Number of line
    );

    /**See if any tone is detected.
      */
    virtual CallProgressTones WaitForToneDetect(
      unsigned line,          ///< Number of line
      unsigned timeout = 3000 ///< Milliseconds to wait for
    );

    /**See if a specific tone is detected.
      */
    virtual BOOL WaitForTone(
      unsigned line,          ///<  Number of line
      CallProgressTones tone, ///<  Tone to wait for
      unsigned timeout = 3000 ///<  Milliseconds to wait for
    );

    /**Set a tones filter information.
       The description string is of the form
          frequence ':' cadence
      where frequency is either
          frequency
          low '-' high
      and cadence is
          mintime
          ontime '-' offtime
          ontime '-' offtime '-' ontime '-' offtime
      examples:
          300:0.25      300Hz for minimum 250ms
          1100:0.4-0.4  1100Hz with cadence 400ms on, 400ms off
          900-1300:1.5  900Hz to 1300Hz for minimum of 1.5 seconds
          425:0.4-0.2-0.4-2    425Hz with cadence
                                400ms on, 200ms off, 400ms on, 2 seconds off
      */
    virtual BOOL SetToneFilter(
      unsigned line,              ///<  Number of line
      CallProgressTones tone,     ///<  Tone filter to change
      const PString & description ///<  Description of filter parameters
    );

    /**Set a tones filter information.
      */
    virtual BOOL SetToneFilterParameters(
      unsigned line,            ///<  Number of line
      CallProgressTones tone,   ///<  Tone filter to change
      unsigned lowFrequency,    ///<  Low frequency
      unsigned highFrequency,   ///<  High frequency
      PINDEX numCadences,       ///<  Number of cadence times
      const unsigned * onTimes, ///<  Cadence ON times
      const unsigned * offTimes ///<  Cadence OFF times
    );

    /**Play a tone.
      */
    virtual BOOL PlayTone(
      unsigned line,          ///<  Number of line
      CallProgressTones tone  ///<  Tone to be played
    );

    /**Determine if a tone is still playing
      */
    virtual BOOL IsTonePlaying(
      unsigned line   ///<  Number of line
    );

    /**Stop playing a tone.
      */
    virtual BOOL StopTone(
      unsigned line   ///<  Number of line
    );


    enum { DIAL_TONE_TIMEOUT = 10000 };

    /**Dial a number on network line.
       The takes the line off hook, waits for dial tone, and transmits the
       specified number as DTMF tones.

       If the requireTones flag is TRUE the call is aborted of the call
       progress tones are not detected. Otherwise the call proceeds with short
       delays while it tries to detect the call progress tones.

       The return code indicates the following:
          DialTone  No dial tone detected
          RingTone  Dial was successful
          BusyTone  The remote phone was busy
          ClearTone Dial failed (usually means rang out)
          NoTone    There was an internal error making the call
      */
    virtual CallProgressTones DialOut(
      unsigned line,                ///< Number of line
      const PString & number,       ///< Number to dial
      BOOL requireTones = FALSE,    ///< Require dial/ring tone to be detected
      unsigned uiDialDelay = 0      ///< time in msec to wait between the dial tone detection and dialing the dtmf
    );


    /**Get wink detect minimum duration.
       This is the signal used by telcos to end PSTN call.
      */
    virtual unsigned GetWinkDuration(
      unsigned line    ///<  Number of line
    );

    /**Set wink detect minimum duration.
       This is the signal used by telcos to end PSTN call.
      */
    virtual BOOL SetWinkDuration(
      unsigned line,        ///<  Number of line
      unsigned winkDuration ///<  New minimum duration
    );


    enum T35CountryCodes {
      Japan, Albania, Algeria, AmericanSamoa, Germany, Anguilla, AntiguaAndBarbuda,
      Argentina, Ascension, Australia, Austria, Bahamas, Bahrain, Bangladesh,
      Barbados, Belgium, Belize, Benin, Bermudas, Bhutan, Bolivia, Botswana,
      Brazil, BritishAntarcticTerritory, BritishIndianOceanTerritory, 
      BritishVirginIslands, BruneiDarussalam, Bulgaria, Myanmar, Burundi,
      Byelorussia, Cameroon, Canada, CapeVerde, CaymanIslands,
      CentralAfricanRepublic, Chad, Chile, China, Colombia, Comoros, Congo,
      CookIslands, CostaRica, Cuba, Cyprus, Czechoslovakia, Cambodia,
      DemocraticPeoplesRepublicOfKorea, Denmark, Djibouti, DominicanRepublic,
      Dominica, Ecuador, Egypt, ElSalvador, EquatorialGuinea, Ethiopia,
      FalklandIslands, Fiji, Finland, France, FrenchPolynesia,
      FrenchSouthernAndAntarcticLands, Gabon, Gambia, Germany2, Angola, Ghana,
      Gibraltar, Greece, Grenada, Guam, Guatemala, Guernsey, Guinea, GuineaBissau,
      Guayana, Haiti, Honduras, Hongkong, Hungary, Iceland, India, Indonesia,
      Iran, Iraq, Ireland, Israel, Italy, CotedIvoire, Jamaica, Afghanistan,
      Jersey, Jordan, Kenya, Kiribati, KoreaRepublic, Kuwait, Lao, Lebanon,
      Lesotho, Liberia, Libya, Liechtenstein, Luxemborg, Macao, Madagascar,
      Malaysia, Malawi, Maldives, Mali, Malta, Mauritania, Mauritius, Mexico,
      Monaco, Mongolia, Montserrat, Morocco, Mozambique, Nauru, Nepal,
      Netherlands, NetherlandsAntilles, NewCaledonia, NewZealand, Nicaragua,
      Niger, Nigeria, Norway, Oman, Pakistan, Panama, PapuaNewGuinea, Paraguay,
      Peru, Philippines, Poland, Portugal, PuertoRico, Qatar, Romania, Rwanda,
      SaintKittsAndNevis, SaintCroix, SaintHelenaAndAscension, SaintLucia,
      SanMarino, SaintThomas, SaoTomeAndPrincipe, SaintVicentAndTheGrenadines,
      SaudiArabia, Senegal, Seychelles, SierraLeone, Singapore, SolomonIslands,
      Somalia, SouthAfrica, Spain, SriLanka, Sudan, Suriname, Swaziland, Sweden,
      Switzerland, Syria, Tanzania, Thailand, Togo, Tonga, TrinidadAndTobago,
      Tunisia, Turkey, TurksAndCaicosIslands, Tuvalu, Uganda, Ukraine,
      UnitedArabEmirates, UnitedKingdom, UnitedStates, BurkinaFaso, Uruguay,
      USSR, Vanuatu, VaticanCityState, Venezuela, VietNam, WallisAndFutuna,
      WesternSamoa, Yemen, Yemen2, Yugoslavia, Zaire, Zambia, Zimbabwe,
      NumCountryCodes,
      UnknownCountry = -1
    };

    /**Get the country code set for the device.
      */
    T35CountryCodes GetCountryCode() const { return countryCode; }

    /**Get the country code set for the device as a string.
      */
    PString GetCountryCodeName() const;

    /**Get the country code set for the device as a string.
      */
    static PString GetCountryCodeName(T35CountryCodes code);
    static T35CountryCodes GetCountryCode(const PString & name);

    /**Set the country code set for the device.
       This may change the line analogue coefficients, ring detect, call
       disconnect detect and call progress tones to fit the countries
       telephone network.
      */
    virtual BOOL SetCountryCode(
      T35CountryCodes country   ///<  COuntry code for device
    );

    /**Set the country code set for the device.
      */
    virtual BOOL SetCountryCodeName(
      const PString & countryName   ///<  COuntry code for device
    );

    /**Get the list of countries actually supported by the device
     */
    virtual PStringList GetCountryCodeNameList() const;


    /**Play a wav file
      */
    virtual BOOL PlayAudio(
      unsigned line,            ///<  Number of line
      const PString & filename  ///<  File Name
    );
    
    /**Stop playing the Wave File
      */
    virtual BOOL StopAudio(
      unsigned line   ///Number of line
    );


    /**
      * start recording audio
      */
    virtual BOOL RecordAudioStart(
      unsigned line,            /// line
      const PString & filename  /// File Name
    );
    
    /**
     * stop recording audio
     */
        
    virtual BOOL RecordAudioStop(
      unsigned line            /// line
    );
    

    /**Return number for last error.
      */
    int GetErrorNumber() const { return osError; }

    /**Return text for last error.
      */
    PString GetErrorText() const;

    virtual void PrintOn(
      ostream & strm
    ) const;

    /**Create a new device from the registration string
      */
    static OpalLineInterfaceDevice * Create(
      const PString & type,     ///<  Type of device to create
      void * parameters = NULL  ///<  Arbitrary parameters for the LID
    );

    /**Return an array of all the LID types registered.
      */
    static PStringList GetAllTypes();

    /**Return an array of all the LID types registered and all of the possible
       devices each one can open. Each string will be of the form
          "type: name"  eg "Quicknet: 3211FFFF"
      */
    static PStringList GetAllDevices();

        
  protected:
    int    getOsHandle() const {return os_handle;};
    void   setOsHandle(int os_handleToSet) {os_handle = os_handleToSet;};
    
    int    getOsError() const {return osError;};
    void   setOsError(int osErrorToSet) {osError = osErrorToSet;};
    
    const PBYTEArray& getReadDeblockingBuffer(){return m_readDeblockingBuffer;};
    const PBYTEArray& getWriteDeblockingBuffer(){return m_writeDeblockingBuffer;};
    PINDEX getReadDeblockingOffset() const {return m_readDeblockingOffset;};
    void   setReadDeblockingOffset(PINDEX readDeblockingOffset) {m_readDeblockingOffset = readDeblockingOffset;};
    
    PINDEX getWriteDeblockingOffset() const {return m_writeDeblockingOffset;};
    void   setWriteDeblockingOffset(PINDEX writeDeblockingOffset) {m_writeDeblockingOffset = writeDeblockingOffset;};
    
    unsigned int getDialToneTimeout() const {return m_uiDialToneTimeout;};
    void   setDialToneTimeout(unsigned int uiDialToneTimeout) {m_uiDialToneTimeout = uiDialToneTimeout;};
        
    int               os_handle;
    mutable int       osError;
    T35CountryCodes   countryCode;
    PBYTEArray        m_readDeblockingBuffer, m_writeDeblockingBuffer;
    PINDEX            m_readDeblockingOffset, m_writeDeblockingOffset;
    unsigned int      m_uiDialToneTimeout;
    std::vector<bool> m_LineAudioEnabled;
    PString           m_callProgressTones[NumTones];
#if PTRACING
    friend ostream & operator<<(ostream & o, CallProgressTones t);
#endif
};


PLIST(OpalLIDList, OpalLineInterfaceDevice);



/**This class describes the LID based codec capability.
 */
class OpalLine : public PObject
{
    PCLASSINFO(OpalLine, PObject);
  public:
  /**@name Construction */
  //@{
    /**Create a new telephone line.
     */
    OpalLine(
      OpalLineInterfaceDevice & device, ///<  Device to make connection with
      unsigned lineNumber,              ///<  number of line on LID
      const char * description = NULL   ///<  DEscription string for line
    );
  //@}

  /**@name Overrides from PObject */
  //@{
    /**Standard stream print function.
       The PObject class has a << operator defined that calls this function
       polymorphically.
      */
    void PrintOn(
      ostream & strm    ///<  Stream to output text representation
    ) const;
  //@}

  /**@name Basic operations */
  //@{
    /**Get the type of the line.
       A "terminal" line is one where a call may terminate. For example a POTS
       line with a standard telephone handset on it would be a terminal line.
       The alternative is a "network" line, that is one connected to switched
       network eg the standard PSTN.
      */
    virtual BOOL IsTerminal() { return device.IsLineTerminal(lineNumber); }


    /**Determine if a physical line is present on the logical line.
      */
    virtual BOOL IsPresent(
      BOOL force = FALSE  ///<  Force test, do not optimise
    ) { return device.IsLinePresent(lineNumber, force); }


    /**Determine if line is currently off hook.
       This function implies that the state is debounced and that a return
       value of TRUE indicates that the phone is really off hook. That is
       hook flashes and winks are masked out.
      */
    virtual BOOL IsOffHook() { return device.IsLineOffHook(lineNumber); }

    /**Set the hook state of the line.
       Note that not be possible on a given line, for example a POTS line with
       a standard telephone handset. The hook state is determined by external
       hardware and cannot be changed by the software.
      */
    virtual BOOL SetOffHook() { return device.SetLineOffHook(lineNumber, TRUE); }

    /**Set the hook state of the line.
       This is the complement of SetLineOffHook().
      */
    virtual BOOL SetOnHook() { return device.SetLineOffHook(lineNumber, FALSE); }

    /**Set the hook state off then straight back on again.
       This will only operate if the line is currently off hook.
      */
    virtual BOOL HookFlash(
      unsigned flashTime = 200    ///<  Time for hook flash in milliseconds
    ) { return device.HookFlash(lineNumber, flashTime); }

    /**Return TRUE if a hook flash has been detected
      */
    virtual BOOL HasHookFlash() { return device.HasHookFlash(lineNumber); }


    /**Determine if line is ringing.
       This function implies that the state is "debounced" and that a return
       value of TRUE indicates that the phone is still ringing and it is not
       simply a pause in the ring cadence.

       If cadence is not NULL then it is set with the bit pattern for the
       incoming ringing. Note that in this case the funtion may take a full
       sequence to return. If it is NULL it can be assumed that the function
       will return quickly.
      */
    virtual BOOL IsRinging(
      DWORD * cadence = NULL  ///<  Cadence of incoming ring
    ) { return device.IsLineRinging(lineNumber, cadence); }

    /**Get the number of rings.
       If the line is ringing then 
      */
    virtual unsigned GetRingCount(
      DWORD * cadence = NULL  ///<  Cadence of incoming ring
    );

    /**Begin ringing local phone set with specified cadence.
       If nCadence is zero then stops ringing.

       Note that this may not be possible on a given line, for example on a
       PSTN line the ring state is determined by external hardware and cannot
       be changed by the software.

       Also note that the cadence may be ignored by particular hardware driver
       so that only the zero or non-zero values are significant.

       The ring pattern is an array of millisecond times for on and off parts
       of the cadence. Thus the Australian ring cadence would be represented
       by the array   unsigned AusRing[] = { 400, 200, 400, 2000 }

       If the nCadence in non-zero and the pattern parameter is NULL, then
       the standard ring pattern for the selected country is used.
      */
    virtual BOOL Ring(
      PINDEX nCadence,                 ///< Number of entries in cadence array
      const unsigned * pattern = NULL, ///< Ring pattern times
      unsigned frequency = 400         ///< Frequency of ring (if relevant)
    ) { return device.RingLine(lineNumber, nCadence, pattern, frequency); }


    /**Determine if line has been disconnected from a call.
       This uses the hardware (and country) dependent means for determining
      */
    virtual BOOL IsDisconnected() { return device.IsLineDisconnected(lineNumber); }

    /**Set the media format (codec) for reading on the specified line.
      */
    virtual BOOL SetReadFormat(
      const OpalMediaFormat & mediaFormat   ///<  Codec type
    ) { return device.SetReadFormat(lineNumber, mediaFormat); }

    /**Set the media format (codec) for writing on the specified line.
      */
    virtual BOOL SetWriteFormat(
      const OpalMediaFormat & mediaFormat   ///<  Codec type
    ) { return device.SetWriteFormat(lineNumber, mediaFormat); }

    /**Get the media format (codec) for reading on the specified line.
      */
    virtual OpalMediaFormat GetReadFormat() { return device.GetReadFormat(lineNumber); }

    /**Get the media format (codec) for writing on the specified line.
      */
    virtual OpalMediaFormat GetWriteFormat() { return device.GetWriteFormat(lineNumber); }

    /**Stop the read codec.
      */
    virtual BOOL StopReading() { return device.StopReading(lineNumber); }

    /**Stop the write codec.
      */
    virtual BOOL StopWriting() { return device.StopWriting(lineNumber); }

    /**Set the read frame size in bytes.
       Note that a LID may ignore this value so always use GetReadFrameSize()
       for I/O.
      */
    virtual BOOL SetReadFrameSize(
      PINDEX frameSize  ///<  New frame size
    ) { return device.SetReadFrameSize(lineNumber, frameSize); }

    /**Set the write frame size in bytes.
       Note that a LID may ignore this value so always use GetReadFrameSize()
       for I/O.
      */
    virtual BOOL SetWriteFrameSize(
      PINDEX frameSize  ///<  New frame size
    ) { return device.SetWriteFrameSize(lineNumber, frameSize); }

    /**Get the read frame size in bytes.
       All calls to ReadFrame() will return this number of bytes.
      */
    virtual PINDEX GetReadFrameSize() { return device.GetReadFrameSize(lineNumber); }

    /**Get the write frame size in bytes.
       All calls to WriteFrame() must be this number of bytes.
      */
    virtual PINDEX GetWriteFrameSize() { return device.GetWriteFrameSize(lineNumber); }

    /**Low level read of a frame from the device.
     */
    virtual BOOL ReadFrame(
      void * buf,       ///<  Pointer to a block of memory to receive data.
      PINDEX & count    ///<  Number of bytes read, <= GetReadFrameSize()
    ) { return device.ReadFrame(lineNumber, buf, count); }

    /**Low level write frame to the device.
     */
    virtual BOOL WriteFrame(
      const void * buf, ///<  Pointer to a block of memory to write.
      PINDEX count,     ///<  Number of bytes to write, <= GetWriteFrameSize()
      PINDEX & written  ///<  Number of bytes written, <= GetWriteFrameSize()
    ) { return device.WriteFrame(lineNumber, buf, count, written); }

    /**High level read of audio data from the device.
       This version will allow non-integral number of frames to be read.
     */
    virtual BOOL ReadBlock(
      void * buf,   ///<  Pointer to a block of memory to receive the read bytes.
      PINDEX count  ///<  Count of bytes to read.
    ) { return device.ReadBlock(lineNumber, buf, count); }

    /**High level write audio data to the device.
     */
    virtual BOOL WriteBlock(
      const void * buf, ///<  Pointer to a block of memory to write.
      PINDEX count      ///<  Count of bytes to write.
    ) { return device.WriteBlock(lineNumber, buf, count); }


    /**Get average signal level in last frame.
      */
    virtual unsigned GetAverageSignalLevel(
      BOOL playback   ///<  Get average playback or record level.
    ) { return device.GetAverageSignalLevel(lineNumber, playback); }


    /**Enable audio for the line.
      */
    virtual BOOL EnableAudio(
      BOOL enable = TRUE
    ) { return device.EnableAudio(lineNumber, enable); }

    /**Disable audio for the line.
      */
    BOOL DisableAudio() { return EnableAudio(FALSE); }

    /**Determine if audio is ebabled for the line.
      */
    virtual BOOL IsAudioEnabled() { return device.IsAudioEnabled(lineNumber); }


    /**Set volume level for recording.
       A value of 100 is the maximum volume possible for the hardware.
       A value of 0 is the minimum volume possible for the hardware.
      */
    virtual BOOL SetRecordVolume(
      unsigned volume   ///<  Volume level from 0 to 100%
    ) { return device.SetRecordVolume(lineNumber, volume); }

    /**Set volume level for playing.
       A value of 100 is the maximum volume possible for the hardware.
       A value of 0 is the minimum volume possible for the hardware.
      */
    virtual BOOL SetPlayVolume(
      unsigned volume   ///<  Volume level from 0 to 100%
    ) { return device.SetPlayVolume(lineNumber, volume); }

    /**Get volume level for recording.
       A value of 100 is the maximum volume possible for the hardware.
       A value of 0 is the minimum volume possible for the hardware.
      */
    virtual BOOL GetRecordVolume(
      unsigned & volume   ///<  Volume level from 0 to 100%
    ) { return device.GetRecordVolume(lineNumber, volume); }

    /**Set volume level for playing.
       A value of 100 is the maximum volume possible for the hardware.
       A value of 0 is the minimum volume possible for the hardware.
      */
    virtual BOOL GetPlayVolume(
      unsigned & volume   ///<  Volume level from 0 to 100%
    ) { return device.GetPlayVolume(lineNumber, volume); }


    /**Get acoustic echo cancellation.
       Note, not all devices may support this function.
      */
    virtual OpalLineInterfaceDevice::AECLevels GetAEC() { return device.GetAEC(lineNumber); }

    /**Set acoustic echo cancellation.
       Note, not all devices may support this function.
      */
    virtual BOOL SetAEC(
      OpalLineInterfaceDevice::AECLevels level  ///<  AEC level
    ) { return device.SetAEC(lineNumber, level); }


    /**Get voice activity detection.
       Note, not all devices, or selected codecs, may support this function.
      */
    virtual BOOL GetVAD() { return device.GetVAD(lineNumber); }

    /**Set voice activity detection.
       Note, not all devices, or selected codecs, may support this function.
      */
    virtual BOOL SetVAD(
      BOOL enable       ///<  Flag for enabling VAD
    ) { return device.SetVAD(lineNumber, enable); }


    /**Get Caller ID from the last incoming ring.
       The idString parameter is either simply the "number" field of the caller
       ID data, or if full is TRUE, all of the fields in the caller ID data.

       The full data of the caller ID string consists of the number field, the
       time/date and the name field separated by tabs ('\t').
      */
    virtual BOOL GetCallerID(
      PString & idString, ///<  ID string returned
      BOOL full = FALSE   ///<  Get full information in idString
    ) { return device.GetCallerID(lineNumber, idString, full); }

    /**Set Caller ID for use in next RingLine() call.
       The full data of the caller ID string consists of the number field, the
       time/date and the name field separated by tabs ('\t').

       If the date field is missing (two consecutive tabs) then the current
       time and date is used. Using an empty string will clear the caller ID
       so that no caller ID is sent on the next RingLine() call.
      */
    virtual BOOL SetCallerID(
      const PString & idString  ///<  ID string to use
    ) { return device.SetCallerID(lineNumber, idString); }

    /**Send a Caller ID on call waiting command
       The full data of the caller ID string consists of the number field, the
       time/date and the name field separated by tabs ('\t').

       If the date field is missing (two consecutive tabs) then the current
       time and date is used. Using an empty string will clear the caller ID
       so that no caller ID is sent on the next RingLine() call.
      */
    virtual BOOL SendCallerIDOnCallWaiting(
      const PString & idString  ///<  ID string to use
    ) { return device.SendCallerIDOnCallWaiting(lineNumber, idString); }

    /**Send a Visual Message Waiting Indicator
      */
    virtual BOOL SendVisualMessageWaitingIndicator(
      BOOL on
    ) { return device.SendVisualMessageWaitingIndicator(lineNumber, on); }


    /**Play a DTMF digit.
       Any characters that are not in the set 0-9, A-D, * or # will be ignored.
      */
    virtual BOOL PlayDTMF(
      const char * digits,      ///<  DTMF digits to be played
      DWORD onTime = OpalLineInterfaceDevice::DefaultDTMFOnTime,  ///<  Number of milliseconds to play each DTMF digit
      DWORD offTime = OpalLineInterfaceDevice::DefaultDTMFOffTime ///<  Number of milliseconds between digits
    ) { return device.PlayDTMF(lineNumber, digits, onTime, offTime); }

    /**Read a DTMF digit detected.
       This may be characters from the set 0-9, A-D, * or #. A null ('\0')
       character indicates that there are no tones in the queue.
       Characters E through P indicate the following tones:

         E = 800   F = 1000  G = 1250  H = 950   I = 1100  J = 1400
         K = 1500  L = 1600  M = 1800  N = 2100  O = 1300  P = 2450

      */
    virtual char ReadDTMF() { return device.ReadDTMF(lineNumber); }

    /**Get DTMF removal mode.
       When set in this mode the DTMF tones detected are removed from the
       encoded data stream as returned by ReadFrame().
      */
    virtual BOOL GetRemoveDTMF() { return device.GetRemoveDTMF(lineNumber); }

    /**Set DTMF removal mode.
       When set in this mode the DTMF tones detected are removed from the
       encoded data stream as returned by ReadFrame().
      */
    virtual BOOL SetRemoveDTMF(
      BOOL removeTones   ///<  Flag for removing DTMF tones.
    ) { return device.SetRemoveDTMF(lineNumber, removeTones); }


    /**See if any tone is detected.
      */
    virtual unsigned IsToneDetected() { return device.IsToneDetected(lineNumber); }

    /**See if any tone is detected.
      */
    virtual OpalLineInterfaceDevice::CallProgressTones WaitForToneDetect(
      unsigned timeout = 3000 ///< Milliseconds to wait for
    ) { return device.WaitForToneDetect(lineNumber, timeout); }

    /**See if a specific tone is detected.
      */
    virtual BOOL WaitForTone(
      OpalLineInterfaceDevice::CallProgressTones tone, ///<  Tone to wait for
      unsigned timeout = 3000 ///<  Milliseconds to wait for
    ) { return device.WaitForTone(lineNumber, tone, timeout); }

    /**Play a tone.
      */
    virtual BOOL PlayTone(
      OpalLineInterfaceDevice::CallProgressTones tone  ///<  Tone to be played
    ) { return device.PlayTone(lineNumber, tone); }

    /**Determine if a tone is still playing
      */
    virtual BOOL IsTonePlaying() { return device.IsTonePlaying(lineNumber); }

    /**Stop playing a tone.
      */
    virtual BOOL StopTone() { return device.StopTone(lineNumber); }


    /**Dial a number on network line.
       The takes the line off hook, waits for dial tone, and transmits the
       specified number as DTMF tones.

       If the requireTones flag is TRUE the call is aborted of the call
       progress tones are not detected. Otherwise the call proceeds with short
       delays while it tries to detect the call progress tones.

       The return code indicates the following:
          DialTone  No dial tone detected
          RingTone  Dial was successful
          BusyTone  The remote phone was busy
          ClearTone Dial failed (usually means rang out)
          NoTone    There was an internal error making the call
      */
    virtual OpalLineInterfaceDevice::CallProgressTones DialOut(
      const PString & number,       ///< Number to dial
      BOOL requireTones = FALSE,    ///< Require dial/ring tone to be detected
      unsigned uiDialDelay = 0      ///< time in msec to wait between the dial tone detection and dialing the dtmf
    ) { return device.DialOut(lineNumber, number, requireTones, uiDialDelay); }
  //@}

  /**@name Member variable access */
  //@{
    /**Get the device this line is on.
      */
    OpalLineInterfaceDevice & GetDevice() const { return device; }

    /**Get the number of the line on the device.
      */
    unsigned GetLineNumber() const { return lineNumber; }

    /**Get the token to uniquely identify this line.
      */
    PString GetToken() const { return token; }

    /**Get the description of this line.
      */
    PString GetDescription() const { return description; }
  //@}

  protected:
    OpalLineInterfaceDevice & device;
    unsigned                  lineNumber;
    PString                   token;
    PString                   description;
    unsigned                  ringCount;
    PTimeInterval             ringTick;
    PTimeInterval             ringStoppedTime;
    PTimeInterval             ringInterCadenceTime;
};


PLIST(OpalLineList, OpalLine);


/**This class embodies the description of a Line Interface Device.

   An application may create a descendent off this class and override
   the Create() function to make the instance of a class implementing a
   transcoder.
 */
class OpalLIDRegistration : public PCaselessString
{
    PCLASSINFO(OpalLIDRegistration, PCaselessString);
  public:
  /**@name Construction */
  //@{
    /**Create a new LID registration.
     */
    OpalLIDRegistration(
      const char * name  ///<  Line Interface Device type name
    );

    /**Destroy and remove LID registration.
     */
    ~OpalLIDRegistration();
  //@}

  /**@name Operations */
  //@{
    /**Create an instance of the transcoder implementation.
      */
    virtual OpalLineInterfaceDevice * Create(
      void * parameters   ///<  Arbitrary parameters for the LID
    ) const = 0;
  //@}

  protected:
    OpalLIDRegistration * link;
    bool                  duplicate;

  friend class OpalLineInterfaceDevice;
};


#define OPAL_REGISTER_LID_FUNCTION(cls, type, param) \
static class cls##_Registration : public OpalLIDRegistration { \
  public: \
    cls##_Registration() : OpalLIDRegistration(type) { } \
    OpalLineInterfaceDevice * Create(void * param) const; \
} instance_##cls##_Registration; \
OpalLineInterfaceDevice * cls##_Registration::Create(void * param) const

#ifndef OPAL_NO_PARAM
#define OPAL_NO_PARAM
#endif

#define OPAL_REGISTER_LID(cls, type) \
  OPAL_REGISTER_LID_FUNCTION(cls, type, OPAL_NO_PARAM) \
  { return new cls; }

#define OPAL_REGISTER_LID_PARAM(cls, type) \
  OPAL_REGISTER_LID_FUNCTION(cls, type, parameter) \
  { return new cls(parameter); }


#endif // __OPAL_LID_H


// End of File ///////////////////////////////////////////////////////////////
