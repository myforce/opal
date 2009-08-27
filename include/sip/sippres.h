/*
 * sippres.h
 *
 * SIP Presence classes for Opal
 *
 * Open Phone Abstraction Library (OPAL)
 *
 * Copyright (c) 2009 Post Increment
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
 * The Initial Developer of the Original Code is Post Increment
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision: 22858 $
 * $Author: csoutheren $
 * $Date: 2009-06-12 22:50:19 +1000 (Fri, 12 Jun 2009) $
 */

#ifndef OPAL_SIP_SIPPRES_H
#define OPAL_SIP_SIPPRES_H

#include <ptlib.h>
#include <opal/buildopts.h>

#include <opal/pres_ent.h>

class SIP_Presentity : public OpalPresentity
{
  public:
    static const char * DefaultPresenceServerKey;
    static const char * PresenceServerKey;

    SIP_Presentity();
    ~SIP_Presentity();

    virtual bool Open(
      OpalManager * manager = NULL
    );

    virtual bool IsOpen() const { return m_endpoint != NULL; }

    virtual bool Save(
      OpalPresentityStore * store
    );

    virtual bool Restore(
      OpalPresentityStore * store
    );

    virtual bool Close();

    virtual bool SetPresence(
      State state,
      const PString & note = PString::Empty()
    ) = 0;

  protected:
    SIPURL GetSIPAOR() const { return SIPURL(GetAttribute(AddressOfRecordKey));  }
    virtual bool SetNotifySubscriptions(bool on) = 0;

    SIPEndPoint & GetEndpoint() { return *m_endpoint; }

    virtual bool InternalOpen() = 0;
    virtual bool InternalClose() = 0;

    OpalManager * m_manager;
    SIPEndPoint * m_endpoint;
};


class SIPLocal_Presentity : public SIP_Presentity
{
  public:
    ~SIPLocal_Presentity();

    virtual bool SetPresence(
      State state,
      const PString & note = PString::Empty()
    );

    virtual bool SetNotifySubscriptions(bool on);

  protected:
    virtual bool InternalOpen();
    virtual bool InternalClose();
};


class SIPXCAP_Presentity : public SIP_Presentity
{
  public:
    SIPXCAP_Presentity();
    ~SIPXCAP_Presentity();

    virtual bool SetPresence(
      State state,
      const PString & note = PString::Empty()
    );

    virtual bool SetNotifySubscriptions(bool on);

  protected:
    virtual bool InternalOpen();
    virtual bool InternalClose();

    PIPSocketAddressAndPort m_presenceServer;
};


#endif // OPAL_SIP_SIPPRES_H
