/*
	ixjIdb.h

	Copyright (c) 1996-2002, Quicknet Technologies, Inc.
	All Rights Reserved.

	Internet PhoneJACK, Internet LineJACK, etc. definitions.

    -----------------------------------------------------------------

	$Header: /home/svnmigrate/clean_cvs/opal/include/lids/Attic/ixjidb.h,v 1.2002 2002/03/22 06:57:48 robertj Exp $

    $Log: ixjidb.h,v $
    Revision 1.2002  2002/03/22 06:57:48  robertj
    Updated to OpenH323 version 1.8.2

    Revision 1.2  2002/03/21 21:21:27  craigs
    Added information from ixjDefs.h

	
*/

#ifndef _IXJIDB_H
#define _IXJIDB_H

typedef enum {
  QTH_DETECT_TONE_TYPE_ADD,
  QTH_DETECT_TONE_TYPE_MOD_BEAT
} qthDetectToneType;

typedef enum {
  QTH_DETECT_TONE_REPEAT_LAST,
  QTH_DETECT_TONE_REPEAT_ALL
} qthDetectToneCadenceTerm;

typedef struct {
  UINT32 ulOnTime; // In ms
  UINT32 ulOffTime; // In ms
} qthDetectToneCadenceElement;

typedef struct {
  UINT32 ulFilter;
  UINT32 ulNumElements;
  qthDetectToneType type;
  qthDetectToneCadenceTerm term;
  UINT32 ulTolerance;
  UINT32 ulMinDetectLoops;
  qthDetectToneCadenceElement element[4]; // Array
} qthDetectToneCadence;

#endif