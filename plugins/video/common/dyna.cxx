/*
 * Common Plugin code for OpenH323/OPAL
 *
 * This code is based on the following files from the OPAL project which
 * have been removed from the current build and distributions but are still
 * available in the CVS "attic"
 * 
 *    src/codecs/h263codec.cxx 
 *    include/codecs/h263codec.h 

 * The original files, and this version of the original code, are released under the same 
 * MPL 1.0 license. Substantial portions of the original code were contributed
 * by Salyens and March Networks and their right to be identified as copyright holders
 * of the original code portions and any parts now included in this new copy is asserted through 
 * their inclusion in the copyright notices below.
 *
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
#include "dyna.h"

bool DynaLink::Open(const char *name)
{
  // At first we try without a path
  if (InternalOpen("", name))
    return true;

  // Try the current directory
  if (InternalOpen(".", name))
    return true;

  // try directories specified in PTLIBPLUGINDIR
  char ptlibPath[1024];
  char * env = ::getenv("PTLIBPLUGINDIR");
  if (env != NULL) 
    strcpy(ptlibPath, env);
  else
#ifdef P_DEFAULT_PLUGIN_DIR
    strcpy(ptlibPath, P_DEFAULT_PLUGIN_DIR);
#elif _WIN32
    strcpy(ptlibPath, "C:\\PTLib_Plugins");
#else
    strcpy(ptlibPath, "/usr/local/lib");
#endif
  char * p = ::strtok(ptlibPath, DIR_TOKENISER);
  while (p != NULL) {
    if (InternalOpen(p, name))
      return true;
    p = ::strtok(NULL, DIR_TOKENISER);
  }

  return false;
}

bool DynaLink::InternalOpen(const char * dir, const char *name)
{
  char path[1024];
  memset(path, 0, sizeof(path));

  // Copy the directory to "path" and add a separator if necessary
  if (strlen(dir) > 0) {
    strcpy(path, dir);
    if (path[strlen(path)-1] != DIR_SEPARATOR[0]) 
      strcat(path, DIR_SEPARATOR);
  }
  strcat(path, name);

  if (strlen(path) == 0) {
    PTRACE(1, m_codecString, "DynaLink: dir '" << (dir != NULL ? dir : "(NULL)")
           << "', name '" << (name != NULL ? name : "(NULL)") << "' resulted in empty path");
    return false;
  }

  // Load the Libary
#ifdef _WIN32
# ifdef UNICODE
  WITH_ALIGNED_STACK({  // must be called before using avcodec lib
     USES_CONVERSION;
    m_hDLL = LoadLibraryEx(A2T(path), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
  });
# else
  WITH_ALIGNED_STACK({  // must be called before using avcodec lib
    m_hDLL = LoadLibraryEx(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
  });
# endif /* UNICODE */
#else
  WITH_ALIGNED_STACK({  // must be called before using avcodec lib
    m_hDLL = dlopen((const char *)path, RTLD_NOW);
  });
#endif /* _WIN32 */

  // Check for errors
  if (m_hDLL == NULL) {
#ifndef _WIN32
    const char * err = dlerror();
    if (err != NULL) {
      PTRACE(1, m_codecString, "Error loading " << path << " - " << err);
    }  
    else {
      PTRACE(1, m_codecString, "Error loading " << path);
    }
#else /* _WIN32 */
    PTRACE(1, m_codecString, "Error loading " << path);
#endif /* _WIN32 */
    return false;
  } 

#ifdef _WIN32
  GetModuleFileName(m_hDLL, path, sizeof(path));
#endif

  PTRACE(1, m_codecString, "Successfully loaded '" << path << "'");
  return true;
}

void DynaLink::Close()
{
  if (m_hDLL != NULL) {
#ifdef _WIN32
    FreeLibrary(m_hDLL);
#else
    dlclose(m_hDLL);
#endif /* _WIN32 */
    m_hDLL = NULL;
  }
}

bool DynaLink::GetFunction(const char * name, Function & func)
{
  if (m_hDLL == NULL)
    return false;

#ifdef _WIN32

  #ifdef UNICODE
    USES_CONVERSION;
    FARPROC p = GetProcAddress(m_hDLL, A2T(name));
  #else
    FARPROC p = GetProcAddress(m_hDLL, name);
  #endif /* UNICODE */
  if (p == NULL) {
    PTRACE(1, m_codecString, "Error linking function " << name << ", error=" << GetLastError());
    return false;
  }

  func = (Function)p;

#else // _WIN32

  void * p = dlsym(m_hDLL, (const char *)name);
  if (p == NULL) {
    PTRACE(1, m_codecString, "Error linking function " << name << ", error=" << dlerror());
    return false;
  }
  func = (Function &)p;

#endif // _WIN32

  return true;
}


#if PLUGINCODEC_TRACING
static void logCallbackFFMPEG(void* v, int severity, const char* fmt , va_list arg)
{
  if (v == NULL)
    return;

  int level;
  if (severity <= AV_LOG_FATAL)
    level = 0;
  else if (severity <= AV_LOG_ERROR)
    level = 1;
  else if (severity <= AV_LOG_WARNING)
    level = 2;
  else if (severity <= AV_LOG_INFO)
    level = 3;
  else if (severity <= AV_LOG_VERBOSE)
    level = 4;
  else
    level = 5;

  if (PTRACE_CHECK(level)) {
    char buffer[512];
    int len = vsnprintf(buffer, sizeof(buffer), fmt, arg);
    if (len > 0) {
      // Drop extra trailing line feed
      --len;
      if (buffer[len] == '\n')
        buffer[len] = '\0';
      // Check for bogus error from H.263, everything works so what does this mean?
      if (strncmp(buffer, "Too many slices", 15) != 0)
        PluginCodec_LogFunctionInstance(level, __FILE__, __LINE__, "FFMPEG", buffer);
    }
  }
}
#endif


FFMPEGLibrary::FFMPEGLibrary(CodecID codec)
{
  m_codec = codec;
  if (m_codec==CODEC_ID_H264)
      snprintf( m_codecString, sizeof(m_codecString), "H264");
  if (m_codec==CODEC_ID_H263P)
      snprintf( m_codecString, sizeof(m_codecString), "H263+");
  if (m_codec==CODEC_ID_MPEG4)
      snprintf( m_codecString, sizeof(m_codecString), "MPEG4");
  m_isLoadedOK = false;
}

FFMPEGLibrary::~FFMPEGLibrary()
{
  m_libAvcodec.Close();
  m_libAvutil.Close();
}

#define CHECK_AVUTIL(name, func) \
      (seperateLibAvutil ? \
        m_libAvutil.GetFunction(name,  (DynaLink::Function &)func) : \
        m_libAvcodec.GetFunction(name, (DynaLink::Function &)func) \
       ) \


bool FFMPEGLibrary::Load()
{
  WaitAndSignal m(processLock);      
  if (IsLoaded())
    return true;

  bool seperateLibAvutil = false;

#ifdef LIBAVCODEC_LIB_NAME
  if (m_libAvcodec.Open(LIBAVCODEC_LIB_NAME))
    seperateLibAvutil = true;
  else
#endif
  if (m_libAvcodec.Open("libavcodec"))
    seperateLibAvutil = false;
  else if (m_libAvcodec.Open("avcodec-52"))
    seperateLibAvutil = true;
  else {
    PTRACE(1, m_codecString, "Failed to load FFMPEG libavcodec library");
    return false;
  }

  if (seperateLibAvutil && !(
#ifdef LIBAVUTIL_LIB_NAME
    m_libAvutil.Open(LIBAVUTIL_LIB_NAME) ||
#endif
    m_libAvutil.Open("avutil-50")) ) {
    PTRACE(1, m_codecString, "Failed to load FFMPEG libavutil library");
    return false;
  }

  strcpy(m_libAvcodec.m_codecString, m_codecString);
  strcpy(m_libAvutil.m_codecString,  m_codecString);

  if (!m_libAvcodec.GetFunction("avcodec_init", (DynaLink::Function &)Favcodec_init))
    return false;

  if (m_codec==CODEC_ID_H264) {
    if (!m_libAvcodec.GetFunction("h264_decoder", (DynaLink::Function &)Favcodec_h264_decoder))
      return false;
  }
  
  if (m_codec==CODEC_ID_H263P) {
    if (!m_libAvcodec.GetFunction("h263_encoder", (DynaLink::Function &)Favcodec_h263_encoder))
      return false;
  
    if (!m_libAvcodec.GetFunction("h263p_encoder", (DynaLink::Function &)Favcodec_h263p_encoder))
      return false;

    if (!m_libAvcodec.GetFunction("h263_decoder", (DynaLink::Function &)Favcodec_h263_decoder))
      return false;
  }

  if (m_codec==CODEC_ID_MPEG4) {
    if (!m_libAvcodec.GetFunction("mpeg4_encoder", (DynaLink::Function &)mpeg4_encoder))
      return false;

    if (!m_libAvcodec.GetFunction("mpeg4_decoder", (DynaLink::Function &)mpeg4_decoder))
      return false;
  }

  if (!m_libAvcodec.GetFunction("register_avcodec", (DynaLink::Function &)Favcodec_register))
    return false;
  
  if (!m_libAvcodec.GetFunction("avcodec_find_encoder", (DynaLink::Function &)Favcodec_find_encoder))
    return false;

  if (!m_libAvcodec.GetFunction("avcodec_find_decoder", (DynaLink::Function &)Favcodec_find_decoder))
    return false;

  if (!m_libAvcodec.GetFunction("avcodec_alloc_context", (DynaLink::Function &)Favcodec_alloc_context))
    return false;

  if (!m_libAvcodec.GetFunction("avcodec_alloc_frame", (DynaLink::Function &)Favcodec_alloc_frame))
    return false;

  if (!m_libAvcodec.GetFunction("avcodec_open", (DynaLink::Function &)Favcodec_open))
    return false;

  if (!m_libAvcodec.GetFunction("avcodec_close", (DynaLink::Function &)Favcodec_close))
    return false;

  if (!m_libAvcodec.GetFunction("avcodec_encode_video", (DynaLink::Function &)Favcodec_encode_video))
    return false;

  if (!m_libAvcodec.GetFunction("avcodec_decode_video", (DynaLink::Function &)Favcodec_decode_video))
    return false;

  if (!m_libAvcodec.GetFunction("avcodec_set_dimensions", (DynaLink::Function &)Favcodec_set_dimensions))
    return false;

  if (!CHECK_AVUTIL("av_malloc", Favcodec_malloc))
    return false;

  if (!CHECK_AVUTIL("av_free", Favcodec_free))
    return false;

  if (!m_libAvcodec.GetFunction("ff_check_alignment", (DynaLink::Function &) Fff_check_alignment)) {
    PTRACE(1, m_codecString, "Failed to load ff_check_alignment - alignment checks will be skipped");
    Fff_check_alignment = NULL;
  }

  if(!m_libAvcodec.GetFunction("avcodec_version", (DynaLink::Function &)Favcodec_version))
    return false;
  
  if (!CHECK_AVUTIL("av_log_set_level", FAv_log_set_level))
    return false;

  if (!CHECK_AVUTIL("av_log_set_callback", FAv_log_set_callback))
    return false;

  WITH_ALIGNED_STACK({  // must be called before using avcodec lib

    unsigned libVer = Favcodec_version();
    if (libVer != LIBAVCODEC_VERSION_INT ) {
      PTRACE(2, m_codecString, "Warning: compiled against libavcodec headers from version "
             << (LIBAVCODEC_VERSION_INT >> 16) << ((LIBAVCODEC_VERSION_INT>>8) & 0xff) << (LIBAVCODEC_VERSION_INT & 0xff)
             << ", loaded " 
             << (libVer >> 16) << ((libVer>>8) & 0xff) << (libVer & 0xff));
    }

    Favcodec_init();

    // register only the codecs needed (to have smaller code)
    if (m_codec==CODEC_ID_H264) 
      Favcodec_register(Favcodec_h264_decoder);

    if (m_codec==CODEC_ID_H263P) {
      Favcodec_register(Favcodec_h263_encoder);
      Favcodec_register(Favcodec_h263p_encoder);
      Favcodec_register(Favcodec_h263_decoder);
    }

    if (m_codec==CODEC_ID_MPEG4) {
      Favcodec_register(mpeg4_encoder);
      Favcodec_register(mpeg4_decoder);
    }

    if (FFCheckAlignment() != 0) {
      PTRACE(1, m_codecString, "ff_check_alignment() reports failure - stack alignment is not correct");
    }	    
  });

#if PLUGINCODEC_TRACING
  AvLogSetLevel(AV_LOG_DEBUG);
  AvLogSetCallback(&logCallbackFFMPEG);
#endif

  m_isLoadedOK = true;
  PTRACE(4, m_codecString, "Successfully loaded libavcodec library and verified functions");

  return true;
}

AVCodec *FFMPEGLibrary::AvcodecFindEncoder(enum CodecID id)
{
  char dummy[16];

  WITH_ALIGNED_STACK({
    AVCodec *res = Favcodec_find_encoder(id);
    return res;
  });
}

AVCodec *FFMPEGLibrary::AvcodecFindDecoder(enum CodecID id)
{
  char dummy[16];

  WaitAndSignal m(processLock);

  WITH_ALIGNED_STACK({
    AVCodec *res = Favcodec_find_decoder(id);
    return res;
  });
}

AVCodecContext *FFMPEGLibrary::AvcodecAllocContext(void)
{
  char dummy[16];

  WaitAndSignal m(processLock);

  WITH_ALIGNED_STACK({
    AVCodecContext *res = Favcodec_alloc_context();
    return res;
  });
}

AVFrame *FFMPEGLibrary::AvcodecAllocFrame(void)
{
  char dummy[16];

  WaitAndSignal m(processLock);

  WITH_ALIGNED_STACK({
    AVFrame *res = Favcodec_alloc_frame();
    return res;
  });
}

int FFMPEGLibrary::AvcodecOpen(AVCodecContext *ctx, AVCodec *codec)
{
  char dummy[16];

  WaitAndSignal m(processLock);

  WITH_ALIGNED_STACK({
    return Favcodec_open(ctx, codec);
  });
}

int FFMPEGLibrary::AvcodecClose(AVCodecContext *ctx)
{
  char dummy[16];

  WaitAndSignal m(processLock);

  WITH_ALIGNED_STACK({
    return Favcodec_close(ctx);
  });
}

int FFMPEGLibrary::AvcodecEncodeVideo(AVCodecContext *ctx, BYTE *buf, int buf_size, const AVFrame *pict)
{
  char dummy[16];

  WITH_ALIGNED_STACK({
    int res = Favcodec_encode_video(ctx, buf, buf_size, pict);

    PTRACE(4, m_codecString, "DYNA\tEncoded " << buf_size << " bytes of YUV420P data into " << res << " bytes");
    return res;
  });
}

int FFMPEGLibrary::AvcodecDecodeVideo(AVCodecContext *ctx, AVFrame *pict, int *got_picture_ptr, BYTE *buf, int buf_size)
{
  char dummy[16];

  WITH_ALIGNED_STACK({
    return Favcodec_decode_video(ctx, pict, got_picture_ptr, buf, buf_size);
  });
}

void FFMPEGLibrary::AvcodecFree(void * ptr)
{
  char dummy[16];

  WaitAndSignal m(processLock);

  WITH_ALIGNED_STACK({
    Favcodec_free(ptr);
  });
}

void FFMPEGLibrary::AvSetDimensions(AVCodecContext *s, int width, int height)
{
  char dummy[16];

  WaitAndSignal m(processLock);

  WITH_ALIGNED_STACK({
    Favcodec_set_dimensions(s, width, height);
  });
}
  

void FFMPEGLibrary::AvLogSetLevel(int level)
{
  char dummy[16];

  WITH_ALIGNED_STACK({
    FAv_log_set_level(level);
  });
}

void FFMPEGLibrary::AvLogSetCallback(void (*callback)(void*, int, const char*, va_list))
{
  char dummy[16];

  WITH_ALIGNED_STACK({
    FAv_log_set_callback(callback);
  });
}

int FFMPEGLibrary::FFCheckAlignment(void)
{
  char dummy[16];

  if (Fff_check_alignment == NULL) {
    PTRACE(1, m_codecString, "ff_check_alignment is not supported by libavcodec.so - skipping check");
    return 0;
  }
  else {
    return Fff_check_alignment();
  }
}

bool FFMPEGLibrary::IsLoaded()
{
  return m_isLoadedOK;
}

