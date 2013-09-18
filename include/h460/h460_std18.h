/* H460_std18.h
 *
 * h323plus library
 *
 * Copyright (c) 2008 ISVO (Asia) Pte. Ltd.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the General Public License (the  "GNU License"), in which case the
 * provisions of GNU License are applicable instead of those
 * above. If you wish to allow use of your version of this file only
 * under the terms of the GNU License and not to allow others to use
 * your version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GNU License. If you do not delete
 * the provisions above, a recipient may use your version of this file
 * under either the MPL or the GNU License."
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 *
 * The Initial Developer of the Original Code is ISVO (Asia) Pte. Ltd.
 *
 * Portions of this code were written with the assisance of funding from
 * triple-IT. http://www.triple-it.nl.
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#ifndef OPAL_H460_STD18_H
#define OPAL_H460_STD18_H

#include <h460/h4601.h>

#if OPAL_H460

#if _MSC_VER
#pragma once
#endif 

/////////////////////////////////////////////////////////////////

class MyH323EndPoint;
class MyH323Connection;
class H46018Handler;

class H460_FeatureStd18 : public H460_FeatureStd 
{
    PCLASSINFO(H460_FeatureStd18, H460_FeatureStd);

  public:
    H460_FeatureStd18();
    virtual ~H460_FeatureStd18();

    // Universal Declarations Every H460 Feature should have the following
    virtual void AttachEndPoint(H323EndPoint * _ep);

    static PStringArray GetFeatureName()         { return PStringArray("Std18"); };
    static PStringArray GetFeatureFriendlyName() { return PStringArray("NatTraversal-H.460.18"); };
    static int GetPurpose()                      { return FeatureRas; };
    static PStringArray GetIdentifier()          { return PStringArray("18"); };

    virtual PBoolean CommonFeature() { return isEnabled; }

    /////////////////////
    // H.460.18 Messages
    virtual PBoolean OnSendGatekeeperRequest(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveGatekeeperConfirm(const H225_FeatureDescriptor & pdu);

    virtual PBoolean OnSendRegistrationRequest(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveRegistrationConfirm(const H225_FeatureDescriptor & pdu);

    virtual void OnReceiveServiceControlIndication(const H225_FeatureDescriptor & pdu);

  private:
    H323EndPoint * EP;

    H46018Handler * handler;
    PBoolean isEnabled;
};


// Need to declare for Factory Loader
#ifndef _WIN32_WCE
  PPLUGIN_STATIC_LOAD(Std18, H460_Feature);
#endif


/////////////////////////////////////////////////////////////////

class MyH323EndPoint;
class MyH323Connection;

class H460_FeatureStd19 : public H460_FeatureStd 
{
    PCLASSINFO(H460_FeatureStd19,H460_FeatureStd);

  public:
    H460_FeatureStd19();
    virtual ~H460_FeatureStd19();

    // Universal Declarations Every H460 Feature should have the following
    virtual void AttachEndPoint(H323EndPoint * _ep);
    virtual void AttachConnection(H323Connection * _con);

    static PStringArray GetFeatureName()         { return PStringArray("Std19"); };
    static PStringArray GetFeatureFriendlyName() { return PStringArray("NatTraversal-H.460.19"); };
    static int GetPurpose()                      { return FeatureSignal; };
    static PStringArray GetIdentifier()          { return PStringArray("19"); };

    virtual PBoolean CommonFeature() { return remoteSupport; }

    /////////////////////
    // H.460.19 Messages
    virtual PBoolean OnSendSetup_UUIE(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveSetup_UUIE(const H225_FeatureDescriptor & pdu);

    virtual PBoolean OnSendCallProceeding_UUIE(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveCallProceeding_UUIE(const H225_FeatureDescriptor & pdu);

    virtual PBoolean OnSendFacility_UUIE(H225_FeatureDescriptor & pdu);
    //virtual void OnReceiveFacility_UUIE(const H225_FeatureDescriptor & pdu) {};

    virtual PBoolean OnSendAlerting_UUIE(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveAlerting_UUIE(const H225_FeatureDescriptor & pdu);

    virtual PBoolean OnSendCallConnect_UUIE(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveCallConnect_UUIE(const H225_FeatureDescriptor & pdu);

    ////////////////////
    // H.460.24 Override
    void SetAvailable(bool avail);

  private:
    H323EndPoint * EP;
    H323Connection * CON;

    PBoolean isEnabled;
    PBoolean isAvailable;
    PBoolean remoteSupport;
};
 

// Need to declare for Factory Loader
#ifndef _WIN32_WCE
  PPLUGIN_STATIC_LOAD(Std19, H460_Feature);
#endif


#endif // OPAL_H460

#endif // OPAL_H460_STD18_H
