//
// h4507.cxx
//
// Code automatically generated by asnparse.
//

#ifdef P_USE_PRAGMA
#pragma implementation "h4507.h"
#endif

#include <ptlib.h>
#include "h4507.h"

#define new PNEW


#if ! H323_DISABLE_H4507



//
// H323-MWI-Operations
//

H4507_H323_MWI_Operations::H4507_H323_MWI_Operations(unsigned tag, PASN_Object::TagClass tagClass)
  : PASN_Enumeration(tag, tagClass, 82, FALSE
#ifndef PASN_NOPRINTON
      , "mwiActivate=80 "
        "mwiDeactivate "
        "mwiInterrogate "
#endif
    )
{
}


H4507_H323_MWI_Operations & H4507_H323_MWI_Operations::operator=(unsigned v)
{
  SetValue(v);
  return *this;
}


PObject * H4507_H323_MWI_Operations::Clone() const
{
#ifndef PASN_LEANANDMEAN
  PAssert(IsClass(H4507_H323_MWI_Operations::Class()), PInvalidCast);
#endif
  return new H4507_H323_MWI_Operations(*this);
}


//
// DummyRes
//

H4507_DummyRes::H4507_DummyRes(unsigned tag, PASN_Object::TagClass tagClass)
  : PASN_Array(tag, tagClass)
{
  SetConstraints(PASN_Object::FixedConstraint, 0, 255);
}


PASN_Object * H4507_DummyRes::CreateObject() const
{
  return new H4504_MixedExtension;
}


H4504_MixedExtension & H4507_DummyRes::operator[](PINDEX i) const
{
  return (H4504_MixedExtension &)array[i];
}


PObject * H4507_DummyRes::Clone() const
{
#ifndef PASN_LEANANDMEAN
  PAssert(IsClass(H4507_DummyRes::Class()), PInvalidCast);
#endif
  return new H4507_DummyRes(*this);
}


//
// MWIInterrogateRes
//

H4507_MWIInterrogateRes::H4507_MWIInterrogateRes(unsigned tag, PASN_Object::TagClass tagClass)
  : PASN_Array(tag, tagClass)
{
  SetConstraints(PASN_Object::FixedConstraint, 1, 64);
}


PASN_Object * H4507_MWIInterrogateRes::CreateObject() const
{
  return new H4507_MWIInterrogateResElt;
}


H4507_MWIInterrogateResElt & H4507_MWIInterrogateRes::operator[](PINDEX i) const
{
  return (H4507_MWIInterrogateResElt &)array[i];
}


PObject * H4507_MWIInterrogateRes::Clone() const
{
#ifndef PASN_LEANANDMEAN
  PAssert(IsClass(H4507_MWIInterrogateRes::Class()), PInvalidCast);
#endif
  return new H4507_MWIInterrogateRes(*this);
}


//
// MsgCentreId
//

H4507_MsgCentreId::H4507_MsgCentreId(unsigned tag, PASN_Object::TagClass tagClass)
  : PASN_Choice(tag, tagClass, 3, FALSE
#ifndef PASN_NOPRINTON
      , "integer "
        "partyNumber "
        "numericString "
#endif
    )
{
}


#if defined(__GNUC__) && __GNUC__ <= 2 && __GNUC_MINOR__ < 9
H4507_MsgCentreId::operator H4501_EndpointAddress &() const
#else
H4507_MsgCentreId::operator H4501_EndpointAddress &()
{
#ifndef PASN_LEANANDMEAN
  PAssert(PIsDescendant(PAssertNULL(choice), H4501_EndpointAddress), PInvalidCast);
#endif
  return *(H4501_EndpointAddress *)choice;
}


H4507_MsgCentreId::operator const H4501_EndpointAddress &() const
#endif
{
#ifndef PASN_LEANANDMEAN
  PAssert(PIsDescendant(PAssertNULL(choice), H4501_EndpointAddress), PInvalidCast);
#endif
  return *(H4501_EndpointAddress *)choice;
}


BOOL H4507_MsgCentreId::CreateObject()
{
  switch (tag) {
    case e_integer :
      choice = new PASN_Integer();
      choice->SetConstraints(PASN_Object::FixedConstraint, 0, 65535);
      return TRUE;
    case e_partyNumber :
      choice = new H4501_EndpointAddress();
      return TRUE;
    case e_numericString :
      choice = new PASN_NumericString();
      choice->SetConstraints(PASN_Object::FixedConstraint, 1, 10);
      return TRUE;
  }

  choice = NULL;
  return FALSE;
}


PObject * H4507_MsgCentreId::Clone() const
{
#ifndef PASN_LEANANDMEAN
  PAssert(IsClass(H4507_MsgCentreId::Class()), PInvalidCast);
#endif
  return new H4507_MsgCentreId(*this);
}


//
// NbOfMessages
//

H4507_NbOfMessages::H4507_NbOfMessages(unsigned tag, PASN_Object::TagClass tagClass)
  : PASN_Integer(tag, tagClass)
{
  SetConstraints(PASN_Object::FixedConstraint, 0, 65535);
}


H4507_NbOfMessages & H4507_NbOfMessages::operator=(int v)
{
  SetValue(v);
  return *this;
}


H4507_NbOfMessages & H4507_NbOfMessages::operator=(unsigned v)
{
  SetValue(v);
  return *this;
}


PObject * H4507_NbOfMessages::Clone() const
{
#ifndef PASN_LEANANDMEAN
  PAssert(IsClass(H4507_NbOfMessages::Class()), PInvalidCast);
#endif
  return new H4507_NbOfMessages(*this);
}


//
// TimeStamp
//

H4507_TimeStamp::H4507_TimeStamp(unsigned tag, PASN_Object::TagClass tagClass)
  : PASN_GeneralisedTime(tag, tagClass)
{
  SetConstraints(PASN_Object::FixedConstraint, 12, 19);
}


PObject * H4507_TimeStamp::Clone() const
{
#ifndef PASN_LEANANDMEAN
  PAssert(IsClass(H4507_TimeStamp::Class()), PInvalidCast);
#endif
  return new H4507_TimeStamp(*this);
}


//
// MessageWaitingIndicationErrors
//

H4507_MessageWaitingIndicationErrors::H4507_MessageWaitingIndicationErrors(unsigned tag, PASN_Object::TagClass tagClass)
  : PASN_Enumeration(tag, tagClass, 2002, FALSE
#ifndef PASN_NOPRINTON
      , "notActivated=31 "
        "undefined=2002 "
        "invalidMsgCentreId=1018 "
#endif
    )
{
}


H4507_MessageWaitingIndicationErrors & H4507_MessageWaitingIndicationErrors::operator=(unsigned v)
{
  SetValue(v);
  return *this;
}


PObject * H4507_MessageWaitingIndicationErrors::Clone() const
{
#ifndef PASN_LEANANDMEAN
  PAssert(IsClass(H4507_MessageWaitingIndicationErrors::Class()), PInvalidCast);
#endif
  return new H4507_MessageWaitingIndicationErrors(*this);
}


//
// BasicService
//

H4507_BasicService::H4507_BasicService(unsigned tag, PASN_Object::TagClass tagClass)
  : PASN_Enumeration(tag, tagClass, 36, FALSE
#ifndef PASN_NOPRINTON
      , "allServices "
        "speech "
        "unrestrictedDigitalInformation "
        "audio3100Hz "
        "telephony=32 "
        "teletex "
        "telefaxGroup4Class1 "
        "videotexSyntaxBased "
        "videotelephony "
#endif
    )
{
}


H4507_BasicService & H4507_BasicService::operator=(unsigned v)
{
  SetValue(v);
  return *this;
}


PObject * H4507_BasicService::Clone() const
{
#ifndef PASN_LEANANDMEAN
  PAssert(IsClass(H4507_BasicService::Class()), PInvalidCast);
#endif
  return new H4507_BasicService(*this);
}


//
// ArrayOf_MixedExtension
//

H4507_ArrayOf_MixedExtension::H4507_ArrayOf_MixedExtension(unsigned tag, PASN_Object::TagClass tagClass)
  : PASN_Array(tag, tagClass)
{
}


PASN_Object * H4507_ArrayOf_MixedExtension::CreateObject() const
{
  return new H4504_MixedExtension;
}


H4504_MixedExtension & H4507_ArrayOf_MixedExtension::operator[](PINDEX i) const
{
  return (H4504_MixedExtension &)array[i];
}


PObject * H4507_ArrayOf_MixedExtension::Clone() const
{
#ifndef PASN_LEANANDMEAN
  PAssert(IsClass(H4507_ArrayOf_MixedExtension::Class()), PInvalidCast);
#endif
  return new H4507_ArrayOf_MixedExtension(*this);
}


//
// MWIActivateArg
//

H4507_MWIActivateArg::H4507_MWIActivateArg(unsigned tag, PASN_Object::TagClass tagClass)
  : PASN_Sequence(tag, tagClass, 6, TRUE, 0)
{
  m_priority.SetConstraints(PASN_Object::FixedConstraint, 0, 9);
  m_extensionArg.SetConstraints(PASN_Object::FixedConstraint, 0, 255);
}


#ifndef PASN_NOPRINTON
void H4507_MWIActivateArg::PrintOn(ostream & strm) const
{
  int indent = strm.precision() + 2;
  strm << "{\n";
  strm << setw(indent+15) << "servedUserNr = " << setprecision(indent) << m_servedUserNr << '\n';
  strm << setw(indent+15) << "basicService = " << setprecision(indent) << m_basicService << '\n';
  if (HasOptionalField(e_msgCentreId))
    strm << setw(indent+14) << "msgCentreId = " << setprecision(indent) << m_msgCentreId << '\n';
  if (HasOptionalField(e_nbOfMessages))
    strm << setw(indent+15) << "nbOfMessages = " << setprecision(indent) << m_nbOfMessages << '\n';
  if (HasOptionalField(e_originatingNr))
    strm << setw(indent+16) << "originatingNr = " << setprecision(indent) << m_originatingNr << '\n';
  if (HasOptionalField(e_timestamp))
    strm << setw(indent+12) << "timestamp = " << setprecision(indent) << m_timestamp << '\n';
  if (HasOptionalField(e_priority))
    strm << setw(indent+11) << "priority = " << setprecision(indent) << m_priority << '\n';
  if (HasOptionalField(e_extensionArg))
    strm << setw(indent+15) << "extensionArg = " << setprecision(indent) << m_extensionArg << '\n';
  strm << setw(indent-1) << setprecision(indent-2) << "}";
}
#endif


PObject::Comparison H4507_MWIActivateArg::Compare(const PObject & obj) const
{
#ifndef PASN_LEANANDMEAN
  PAssert(PIsDescendant(&obj, H4507_MWIActivateArg), PInvalidCast);
#endif
  const H4507_MWIActivateArg & other = (const H4507_MWIActivateArg &)obj;

  Comparison result;

  if ((result = m_servedUserNr.Compare(other.m_servedUserNr)) != EqualTo)
    return result;
  if ((result = m_basicService.Compare(other.m_basicService)) != EqualTo)
    return result;
  if ((result = m_msgCentreId.Compare(other.m_msgCentreId)) != EqualTo)
    return result;
  if ((result = m_nbOfMessages.Compare(other.m_nbOfMessages)) != EqualTo)
    return result;
  if ((result = m_originatingNr.Compare(other.m_originatingNr)) != EqualTo)
    return result;
  if ((result = m_timestamp.Compare(other.m_timestamp)) != EqualTo)
    return result;
  if ((result = m_priority.Compare(other.m_priority)) != EqualTo)
    return result;
  if ((result = m_extensionArg.Compare(other.m_extensionArg)) != EqualTo)
    return result;

  return PASN_Sequence::Compare(other);
}


PINDEX H4507_MWIActivateArg::GetDataLength() const
{
  PINDEX length = 0;
  length += m_servedUserNr.GetObjectLength();
  length += m_basicService.GetObjectLength();
  if (HasOptionalField(e_msgCentreId))
    length += m_msgCentreId.GetObjectLength();
  if (HasOptionalField(e_nbOfMessages))
    length += m_nbOfMessages.GetObjectLength();
  if (HasOptionalField(e_originatingNr))
    length += m_originatingNr.GetObjectLength();
  if (HasOptionalField(e_timestamp))
    length += m_timestamp.GetObjectLength();
  if (HasOptionalField(e_priority))
    length += m_priority.GetObjectLength();
  if (HasOptionalField(e_extensionArg))
    length += m_extensionArg.GetObjectLength();
  return length;
}


BOOL H4507_MWIActivateArg::Decode(PASN_Stream & strm)
{
  if (!PreambleDecode(strm))
    return FALSE;

  if (!m_servedUserNr.Decode(strm))
    return FALSE;
  if (!m_basicService.Decode(strm))
    return FALSE;
  if (HasOptionalField(e_msgCentreId) && !m_msgCentreId.Decode(strm))
    return FALSE;
  if (HasOptionalField(e_nbOfMessages) && !m_nbOfMessages.Decode(strm))
    return FALSE;
  if (HasOptionalField(e_originatingNr) && !m_originatingNr.Decode(strm))
    return FALSE;
  if (HasOptionalField(e_timestamp) && !m_timestamp.Decode(strm))
    return FALSE;
  if (HasOptionalField(e_priority) && !m_priority.Decode(strm))
    return FALSE;
  if (HasOptionalField(e_extensionArg) && !m_extensionArg.Decode(strm))
    return FALSE;

  return UnknownExtensionsDecode(strm);
}


void H4507_MWIActivateArg::Encode(PASN_Stream & strm) const
{
  PreambleEncode(strm);

  m_servedUserNr.Encode(strm);
  m_basicService.Encode(strm);
  if (HasOptionalField(e_msgCentreId))
    m_msgCentreId.Encode(strm);
  if (HasOptionalField(e_nbOfMessages))
    m_nbOfMessages.Encode(strm);
  if (HasOptionalField(e_originatingNr))
    m_originatingNr.Encode(strm);
  if (HasOptionalField(e_timestamp))
    m_timestamp.Encode(strm);
  if (HasOptionalField(e_priority))
    m_priority.Encode(strm);
  if (HasOptionalField(e_extensionArg))
    m_extensionArg.Encode(strm);

  UnknownExtensionsEncode(strm);
}


PObject * H4507_MWIActivateArg::Clone() const
{
#ifndef PASN_LEANANDMEAN
  PAssert(IsClass(H4507_MWIActivateArg::Class()), PInvalidCast);
#endif
  return new H4507_MWIActivateArg(*this);
}


//
// MWIDeactivateArg
//

H4507_MWIDeactivateArg::H4507_MWIDeactivateArg(unsigned tag, PASN_Object::TagClass tagClass)
  : PASN_Sequence(tag, tagClass, 3, TRUE, 0)
{
  m_extensionArg.SetConstraints(PASN_Object::FixedConstraint, 0, 255);
}


#ifndef PASN_NOPRINTON
void H4507_MWIDeactivateArg::PrintOn(ostream & strm) const
{
  int indent = strm.precision() + 2;
  strm << "{\n";
  strm << setw(indent+15) << "servedUserNr = " << setprecision(indent) << m_servedUserNr << '\n';
  strm << setw(indent+15) << "basicService = " << setprecision(indent) << m_basicService << '\n';
  if (HasOptionalField(e_msgCentreId))
    strm << setw(indent+14) << "msgCentreId = " << setprecision(indent) << m_msgCentreId << '\n';
  if (HasOptionalField(e_callbackReq))
    strm << setw(indent+14) << "callbackReq = " << setprecision(indent) << m_callbackReq << '\n';
  if (HasOptionalField(e_extensionArg))
    strm << setw(indent+15) << "extensionArg = " << setprecision(indent) << m_extensionArg << '\n';
  strm << setw(indent-1) << setprecision(indent-2) << "}";
}
#endif


PObject::Comparison H4507_MWIDeactivateArg::Compare(const PObject & obj) const
{
#ifndef PASN_LEANANDMEAN
  PAssert(PIsDescendant(&obj, H4507_MWIDeactivateArg), PInvalidCast);
#endif
  const H4507_MWIDeactivateArg & other = (const H4507_MWIDeactivateArg &)obj;

  Comparison result;

  if ((result = m_servedUserNr.Compare(other.m_servedUserNr)) != EqualTo)
    return result;
  if ((result = m_basicService.Compare(other.m_basicService)) != EqualTo)
    return result;
  if ((result = m_msgCentreId.Compare(other.m_msgCentreId)) != EqualTo)
    return result;
  if ((result = m_callbackReq.Compare(other.m_callbackReq)) != EqualTo)
    return result;
  if ((result = m_extensionArg.Compare(other.m_extensionArg)) != EqualTo)
    return result;

  return PASN_Sequence::Compare(other);
}


PINDEX H4507_MWIDeactivateArg::GetDataLength() const
{
  PINDEX length = 0;
  length += m_servedUserNr.GetObjectLength();
  length += m_basicService.GetObjectLength();
  if (HasOptionalField(e_msgCentreId))
    length += m_msgCentreId.GetObjectLength();
  if (HasOptionalField(e_callbackReq))
    length += m_callbackReq.GetObjectLength();
  if (HasOptionalField(e_extensionArg))
    length += m_extensionArg.GetObjectLength();
  return length;
}


BOOL H4507_MWIDeactivateArg::Decode(PASN_Stream & strm)
{
  if (!PreambleDecode(strm))
    return FALSE;

  if (!m_servedUserNr.Decode(strm))
    return FALSE;
  if (!m_basicService.Decode(strm))
    return FALSE;
  if (HasOptionalField(e_msgCentreId) && !m_msgCentreId.Decode(strm))
    return FALSE;
  if (HasOptionalField(e_callbackReq) && !m_callbackReq.Decode(strm))
    return FALSE;
  if (HasOptionalField(e_extensionArg) && !m_extensionArg.Decode(strm))
    return FALSE;

  return UnknownExtensionsDecode(strm);
}


void H4507_MWIDeactivateArg::Encode(PASN_Stream & strm) const
{
  PreambleEncode(strm);

  m_servedUserNr.Encode(strm);
  m_basicService.Encode(strm);
  if (HasOptionalField(e_msgCentreId))
    m_msgCentreId.Encode(strm);
  if (HasOptionalField(e_callbackReq))
    m_callbackReq.Encode(strm);
  if (HasOptionalField(e_extensionArg))
    m_extensionArg.Encode(strm);

  UnknownExtensionsEncode(strm);
}


PObject * H4507_MWIDeactivateArg::Clone() const
{
#ifndef PASN_LEANANDMEAN
  PAssert(IsClass(H4507_MWIDeactivateArg::Class()), PInvalidCast);
#endif
  return new H4507_MWIDeactivateArg(*this);
}


//
// MWIInterrogateArg
//

H4507_MWIInterrogateArg::H4507_MWIInterrogateArg(unsigned tag, PASN_Object::TagClass tagClass)
  : PASN_Sequence(tag, tagClass, 3, TRUE, 0)
{
  m_extensionArg.SetConstraints(PASN_Object::FixedConstraint, 0, 255);
}


#ifndef PASN_NOPRINTON
void H4507_MWIInterrogateArg::PrintOn(ostream & strm) const
{
  int indent = strm.precision() + 2;
  strm << "{\n";
  strm << setw(indent+15) << "servedUserNr = " << setprecision(indent) << m_servedUserNr << '\n';
  strm << setw(indent+15) << "basicService = " << setprecision(indent) << m_basicService << '\n';
  if (HasOptionalField(e_msgCentreId))
    strm << setw(indent+14) << "msgCentreId = " << setprecision(indent) << m_msgCentreId << '\n';
  if (HasOptionalField(e_callbackReq))
    strm << setw(indent+14) << "callbackReq = " << setprecision(indent) << m_callbackReq << '\n';
  if (HasOptionalField(e_extensionArg))
    strm << setw(indent+15) << "extensionArg = " << setprecision(indent) << m_extensionArg << '\n';
  strm << setw(indent-1) << setprecision(indent-2) << "}";
}
#endif


PObject::Comparison H4507_MWIInterrogateArg::Compare(const PObject & obj) const
{
#ifndef PASN_LEANANDMEAN
  PAssert(PIsDescendant(&obj, H4507_MWIInterrogateArg), PInvalidCast);
#endif
  const H4507_MWIInterrogateArg & other = (const H4507_MWIInterrogateArg &)obj;

  Comparison result;

  if ((result = m_servedUserNr.Compare(other.m_servedUserNr)) != EqualTo)
    return result;
  if ((result = m_basicService.Compare(other.m_basicService)) != EqualTo)
    return result;
  if ((result = m_msgCentreId.Compare(other.m_msgCentreId)) != EqualTo)
    return result;
  if ((result = m_callbackReq.Compare(other.m_callbackReq)) != EqualTo)
    return result;
  if ((result = m_extensionArg.Compare(other.m_extensionArg)) != EqualTo)
    return result;

  return PASN_Sequence::Compare(other);
}


PINDEX H4507_MWIInterrogateArg::GetDataLength() const
{
  PINDEX length = 0;
  length += m_servedUserNr.GetObjectLength();
  length += m_basicService.GetObjectLength();
  if (HasOptionalField(e_msgCentreId))
    length += m_msgCentreId.GetObjectLength();
  if (HasOptionalField(e_callbackReq))
    length += m_callbackReq.GetObjectLength();
  if (HasOptionalField(e_extensionArg))
    length += m_extensionArg.GetObjectLength();
  return length;
}


BOOL H4507_MWIInterrogateArg::Decode(PASN_Stream & strm)
{
  if (!PreambleDecode(strm))
    return FALSE;

  if (!m_servedUserNr.Decode(strm))
    return FALSE;
  if (!m_basicService.Decode(strm))
    return FALSE;
  if (HasOptionalField(e_msgCentreId) && !m_msgCentreId.Decode(strm))
    return FALSE;
  if (HasOptionalField(e_callbackReq) && !m_callbackReq.Decode(strm))
    return FALSE;
  if (HasOptionalField(e_extensionArg) && !m_extensionArg.Decode(strm))
    return FALSE;

  return UnknownExtensionsDecode(strm);
}


void H4507_MWIInterrogateArg::Encode(PASN_Stream & strm) const
{
  PreambleEncode(strm);

  m_servedUserNr.Encode(strm);
  m_basicService.Encode(strm);
  if (HasOptionalField(e_msgCentreId))
    m_msgCentreId.Encode(strm);
  if (HasOptionalField(e_callbackReq))
    m_callbackReq.Encode(strm);
  if (HasOptionalField(e_extensionArg))
    m_extensionArg.Encode(strm);

  UnknownExtensionsEncode(strm);
}


PObject * H4507_MWIInterrogateArg::Clone() const
{
#ifndef PASN_LEANANDMEAN
  PAssert(IsClass(H4507_MWIInterrogateArg::Class()), PInvalidCast);
#endif
  return new H4507_MWIInterrogateArg(*this);
}


//
// MWIInterrogateResElt
//

H4507_MWIInterrogateResElt::H4507_MWIInterrogateResElt(unsigned tag, PASN_Object::TagClass tagClass)
  : PASN_Sequence(tag, tagClass, 6, TRUE, 0)
{
  m_priority.SetConstraints(PASN_Object::FixedConstraint, 0, 9);
  m_extensionArg.SetConstraints(PASN_Object::FixedConstraint, 0, 255);
}


#ifndef PASN_NOPRINTON
void H4507_MWIInterrogateResElt::PrintOn(ostream & strm) const
{
  int indent = strm.precision() + 2;
  strm << "{\n";
  strm << setw(indent+15) << "basicService = " << setprecision(indent) << m_basicService << '\n';
  if (HasOptionalField(e_msgCentreId))
    strm << setw(indent+14) << "msgCentreId = " << setprecision(indent) << m_msgCentreId << '\n';
  if (HasOptionalField(e_nbOfMessages))
    strm << setw(indent+15) << "nbOfMessages = " << setprecision(indent) << m_nbOfMessages << '\n';
  if (HasOptionalField(e_originatingNr))
    strm << setw(indent+16) << "originatingNr = " << setprecision(indent) << m_originatingNr << '\n';
  if (HasOptionalField(e_timestamp))
    strm << setw(indent+12) << "timestamp = " << setprecision(indent) << m_timestamp << '\n';
  if (HasOptionalField(e_priority))
    strm << setw(indent+11) << "priority = " << setprecision(indent) << m_priority << '\n';
  if (HasOptionalField(e_extensionArg))
    strm << setw(indent+15) << "extensionArg = " << setprecision(indent) << m_extensionArg << '\n';
  strm << setw(indent-1) << setprecision(indent-2) << "}";
}
#endif


PObject::Comparison H4507_MWIInterrogateResElt::Compare(const PObject & obj) const
{
#ifndef PASN_LEANANDMEAN
  PAssert(PIsDescendant(&obj, H4507_MWIInterrogateResElt), PInvalidCast);
#endif
  const H4507_MWIInterrogateResElt & other = (const H4507_MWIInterrogateResElt &)obj;

  Comparison result;

  if ((result = m_basicService.Compare(other.m_basicService)) != EqualTo)
    return result;
  if ((result = m_msgCentreId.Compare(other.m_msgCentreId)) != EqualTo)
    return result;
  if ((result = m_nbOfMessages.Compare(other.m_nbOfMessages)) != EqualTo)
    return result;
  if ((result = m_originatingNr.Compare(other.m_originatingNr)) != EqualTo)
    return result;
  if ((result = m_timestamp.Compare(other.m_timestamp)) != EqualTo)
    return result;
  if ((result = m_priority.Compare(other.m_priority)) != EqualTo)
    return result;
  if ((result = m_extensionArg.Compare(other.m_extensionArg)) != EqualTo)
    return result;

  return PASN_Sequence::Compare(other);
}


PINDEX H4507_MWIInterrogateResElt::GetDataLength() const
{
  PINDEX length = 0;
  length += m_basicService.GetObjectLength();
  if (HasOptionalField(e_msgCentreId))
    length += m_msgCentreId.GetObjectLength();
  if (HasOptionalField(e_nbOfMessages))
    length += m_nbOfMessages.GetObjectLength();
  if (HasOptionalField(e_originatingNr))
    length += m_originatingNr.GetObjectLength();
  if (HasOptionalField(e_timestamp))
    length += m_timestamp.GetObjectLength();
  if (HasOptionalField(e_priority))
    length += m_priority.GetObjectLength();
  if (HasOptionalField(e_extensionArg))
    length += m_extensionArg.GetObjectLength();
  return length;
}


BOOL H4507_MWIInterrogateResElt::Decode(PASN_Stream & strm)
{
  if (!PreambleDecode(strm))
    return FALSE;

  if (!m_basicService.Decode(strm))
    return FALSE;
  if (HasOptionalField(e_msgCentreId) && !m_msgCentreId.Decode(strm))
    return FALSE;
  if (HasOptionalField(e_nbOfMessages) && !m_nbOfMessages.Decode(strm))
    return FALSE;
  if (HasOptionalField(e_originatingNr) && !m_originatingNr.Decode(strm))
    return FALSE;
  if (HasOptionalField(e_timestamp) && !m_timestamp.Decode(strm))
    return FALSE;
  if (HasOptionalField(e_priority) && !m_priority.Decode(strm))
    return FALSE;
  if (HasOptionalField(e_extensionArg) && !m_extensionArg.Decode(strm))
    return FALSE;

  return UnknownExtensionsDecode(strm);
}


void H4507_MWIInterrogateResElt::Encode(PASN_Stream & strm) const
{
  PreambleEncode(strm);

  m_basicService.Encode(strm);
  if (HasOptionalField(e_msgCentreId))
    m_msgCentreId.Encode(strm);
  if (HasOptionalField(e_nbOfMessages))
    m_nbOfMessages.Encode(strm);
  if (HasOptionalField(e_originatingNr))
    m_originatingNr.Encode(strm);
  if (HasOptionalField(e_timestamp))
    m_timestamp.Encode(strm);
  if (HasOptionalField(e_priority))
    m_priority.Encode(strm);
  if (HasOptionalField(e_extensionArg))
    m_extensionArg.Encode(strm);

  UnknownExtensionsEncode(strm);
}


PObject * H4507_MWIInterrogateResElt::Clone() const
{
#ifndef PASN_LEANANDMEAN
  PAssert(IsClass(H4507_MWIInterrogateResElt::Class()), PInvalidCast);
#endif
  return new H4507_MWIInterrogateResElt(*this);
}


#endif // if ! H323_DISABLE_H4507


// End of h4507.cxx
