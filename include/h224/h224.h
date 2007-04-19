/*
 * h224.h
 *
 * H.224 PDU implementation for the OpenH323 Project.
 *
 * Copyright (c) 2006 Network for Educational Technology, ETH Zurich.
 * Written by Hannes Friederich.
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
 * Contributor(s): ______________________________________.
 *
 * $Log: h224.h,v $
 * Revision 1.3  2007/04/19 06:17:20  csoutheren
 * Fixes for precompiled headers with gcc
 *
 * Revision 1.2  2006/04/23 18:52:19  dsandras
 * Removed warnings when compiling with gcc on Linux.
 *
 * Revision 1.1  2006/04/20 16:48:17  hfriederich
 * Initial version of H.224/H.281 implementation.
 *
 */

#ifndef __OPAL_H224_H
#define __OPAL_H224_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#ifndef _PTLIB_H
#include <ptlib.h>
#endif

#include <h224/q922.h>

#define H224_HEADER_SIZE 6

#define H224_BROADCAST 0x0000

class H224_Frame : public Q922_Frame
{
  PCLASSINFO(H224_Frame, Q922_Frame);
	
public:
	
  H224_Frame(PINDEX clientDataSize = 254);
  ~H224_Frame();
	
  BOOL IsHighPriority() const { return (GetLowOrderAddressOctet() == 0x71); }
  void SetHighPriority(BOOL flag);
	
  WORD GetDestinationTerminalAddress() const;
  void SetDestinationTerminalAddress(WORD destination);
	
  WORD GetSourceTerminalAddress() const;
  void SetSourceTerminalAddress(WORD source);
	
  // Only standard client IDs are supported at the moment
  BYTE GetClientID() const;
  void SetClientID(BYTE clientID);
	
  BOOL GetBS() const;
  void SetBS(BOOL bs);
	
  BOOL GetES() const;
  void SetES(BOOL es);
	
  BOOL GetC1() const;
  void SetC1(BOOL c1);
	
  BOOL GetC0() const;
  void SetC0(BOOL c0);
	
  BYTE GetSegmentNumber() const;
  void SetSegmentNumber(BYTE segmentNumber);
	
  BYTE *GetClientDataPtr() const { return (GetInformationFieldPtr() + H224_HEADER_SIZE); }
	
  PINDEX GetClientDataSize() const { return (GetInformationFieldSize() - H224_HEADER_SIZE); }
  void SetClientDataSize(PINDEX size) { SetInformationFieldSize(size + H224_HEADER_SIZE); }
	
  BOOL Decode(const BYTE *data, PINDEX size);
};

#endif // __OPAL_H224_H

