/*
 * Common encoded frame handling
 *
 * Copyright (C) 2014 Vox Lucida Pty. Ltd.
 * Copyright (C) 2006 Post Increment
 * Copyright (C) 2005 Salyens
 * Copyright (C) 2001 March Networks Corporation
 * Copyright (C) 1999-2000 Equivalence Pty. Ltd.
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
 * Contributor(s): Guilhem Tardy (gtardy@salyens.com)
 *                 Craig Southeren (craigs@postincrement.com)
 *                 Matthias Schneider (ma30002000@yahoo.de)
 */

#include "encframe.h"

#ifndef PLUGIN_CODEC_DLL_EXPORTS
#include "plugin_config.h"
#endif


namespace OpalMemory
{

bool AllocateAligned(void * & baseMemory, uint8_t * & alignedMemory, size_t & alignedSize, size_t requestedSize)
{
  if (requestedSize > alignedSize && baseMemory != NULL) {
    free(baseMemory);
    baseMemory = NULL;
  }

  if (baseMemory == NULL) {
#if HAVE_POSIX_MEMALIGN
    if (posix_memalign(&baseMemory, Alignment, requestedSize) != 0)
#else
    if ((baseMemory = malloc(requestedSize+Alignment)) == NULL)
#endif
    {
      PTRACE(1, "FFMPEG", "Unable to allocate " << requestedSize << " bytes for aligned buffer.");
      return false;
    }
    alignedSize = requestedSize;
    PTRACE(5, "FFMPEG", "Allocated " << requestedSize << " byte aligned buffer.");
  }

#if HAVE_POSIX_MEMALIGN
  alignedMemory = (uint8_t *)baseMemory;
#else
  alignedMemory = (uint8_t *)((Alignment-1+(intptr_t)baseMemory)&(-(intptr_t)Alignment));
#endif

  return true;
}

};


OpalPluginFrame::OpalPluginFrame()
  : m_length(0)
  , m_maxSize(0)
  , m_memory(NULL)
  , m_buffer(NULL)
  , m_maxPayloadSize(PluginCodec_RTP_MaxPayloadSize)
{
}


OpalPluginFrame::~OpalPluginFrame()
{
  if (m_memory != NULL)
    free(m_memory);
}


void OpalPluginFrame::SetMaxPayloadSize(size_t size)
{
  m_maxPayloadSize = size;
}


bool OpalPluginFrame::SetResolution(unsigned width, unsigned height)
{
  return OpalMemory::AllocateAligned(m_memory, m_buffer, m_maxSize, width*height*2);
}


bool OpalPluginFrame::Append(const uint8_t * data, size_t len)
{
  size_t newSize = m_length + len;
  if (newSize > m_maxSize) {
    if ((m_buffer = (uint8_t *)realloc(m_buffer, newSize)) == NULL) {
      PTRACE(1, "FFMPEG", "Could not (re)allocate " << newSize << " bytes of memory.");
      return false;
    }
    m_maxSize = newSize;
  }

  memcpy(m_buffer+m_length, data, len);
  m_length += len;
  return true;
}


bool OpalPluginFrame::Reset(size_t len)
{
  if (len > m_maxSize)
    return false;

  m_length = len;
  return true;
}


void OpalPluginFrame::RTPCallBack(void *, int, int)
{
}

