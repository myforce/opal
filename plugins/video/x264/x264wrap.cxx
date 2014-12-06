/*****************************************************************************/
/* The contents of this file are subject to the Mozilla Public License       */
/* Version 1.0 (the "License"); you may not use this file except in          */
/* compliance with the License.  You may obtain a copy of the License at     */
/* http://www.mozilla.org/MPL/                                               */
/*                                                                           */
/* Software distributed under the License is distributed on an "AS IS"       */
/* basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the  */
/* License for the specific language governing rights and limitations under  */
/* the License.                                                              */
/*                                                                           */
/* The Original Code is the Open H323 Library.                               */
/*                                                                           */
/* The Initial Developer of the Original Code is Matthias Schneider          */
/* Copyright (C) 2007 Matthias Schneider, All Rights Reserved.               */
/*                                                                           */
/* Contributor(s): Matthias Schneider (ma30002000@yahoo.de)                  */
/*                                                                           */
/* Alternatively, the contents of this file may be used under the terms of   */
/* the GNU General Public License Version 2 or later (the "GPL"), in which   */
/* case the provisions of the GPL are applicable instead of those above.  If */
/* you wish to allow use of your version of this file only under the terms   */
/* of the GPL and not to allow others to use your version of this file under */
/* the MPL, indicate your decision by deleting the provisions above and      */
/* replace them with the notice and other provisions required by the GPL.    */
/* If you do not delete the provisions above, a recipient may use your       */
/* version of this file under either the MPL or the GPL.                     */
/*                                                                           */
/* The Original Code was written by Matthias Schneider <ma30002000@yahoo.de> */
/*****************************************************************************/

#include "x264wrap.h"

#include <codec/opalplugin.hpp>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#if defined(X264_LICENSED) || defined(GPL_HELPER_APP)

#if PLUGINCODEC_TRACING
  static char const HelperTraceName[] = "x264-help";

  static void logCallbackX264(void * /*priv*/, int level, const char *fmt, va_list arg) {
    unsigned severity = 4;
    switch (level) {
      case X264_LOG_NONE:    severity = 1; break;
      case X264_LOG_ERROR:   severity = 2; break;
      case X264_LOG_WARNING: severity = 3; break;
      case X264_LOG_INFO:    severity = 4; break;
      case X264_LOG_DEBUG:   severity = 5; break;
    }

    char buffer[512];
    int len = vsnprintf(buffer, sizeof(buffer), fmt, arg);
    if (len <= 0)
      return;

    while (buffer[--len] == '\n')
      buffer[len] = '\0';

    PTRACE(severity, "x264-lib", buffer);
  }
#endif


H264Encoder::H264Encoder()
  : m_codec(NULL)
{
  // Default
  x264_param_default_preset(&m_context, "veryfast", "fastdecode,zerolatency");

  SetProfileLevel(100, 30, 0xc0);

  // default size, will likely get reset later
  m_context.i_width = 352;
  m_context.i_height = 288;

  // No aspect ratio correction
  m_context.vui.i_sar_width = 0;
  m_context.vui.i_sar_height = 0;

  // default frame rate, will likely get reset later
  m_context.i_fps_num = 15000;
  m_context.i_fps_den = 1000;

  // frames between key frames
  m_context.i_keyint_max = 132;

  // Auto detect number of CPUs
  m_context.i_threads = 0;

  // Do not do markers, we are RTP so don't need them
  m_context.b_annexb = false;

  /* As Jason said it keeps us from one frame encoding latency,
     allowing to achieve zero frame latency on single core encoding. */
  m_context.b_vfr_input = 0;

  /* Compatibility with some Polycom and Tandberg codecs since those
     two can't decode frames with long vectors going beyound this limit */
  m_context.analyse.i_mv_range = 128;

  // At least Polycom HDX 4000 is incompatible unless have this
  m_context.i_frame_reference = 1;

  /* Judging by Jason proud posts it's a kickass uber feature for
     Telepresence, allow you to stay away from big periodic I-Frames.
     Encoder always keep a wave of I-type macro blocks which refresh
     picture and keeps more stable frame sizes on output. And if
     picture is broken full I-frame introduced thanks to
     videoFastUpdatePicture PDU. Probably */
  m_context.b_intra_refresh = 1;

  // Always good to put some noise reduction and this filter must be very fast.
  m_context.analyse.i_noise_reduction = 1000;

  // ABR with bit rate tolerance = 1 is CBR...
  m_context.rc.i_rc_method = X264_RC_ABR;
  m_context.rc.f_rate_tolerance = 1;
  // 768kbps and 1 second rate control period
  m_context.rc.i_vbv_buffer_size = m_context.rc.i_vbv_max_bitrate = m_context.rc.i_bitrate = 768;

#if PLUGINCODEC_TRACING
  // Enable logging
  m_context.pf_log = logCallbackX264;
  m_context.i_log_level = X264_LOG_DEBUG;
  m_context.p_log_private = NULL;
#endif
}


H264Encoder::~H264Encoder()
{
  if (m_codec != NULL) {
    x264_encoder_close(m_codec);
    PTRACE(5, HelperTraceName, "Closed H.264 encoder");
  }
}


bool H264Encoder::Load(void *)
{
  m_codec = x264_encoder_open(&m_context);
  if (m_codec == NULL) {
    PTRACE(1, HelperTraceName, "Couldn't open encoder");
    return false;
  } 

  PTRACE(4, HelperTraceName, "Encoder successfully opened");
  return true;
}


bool H264Encoder::SetProfileLevel(unsigned profile, unsigned level, unsigned /*constraints*/)
{
  int profileIndex = 0;
  switch (profile) {
    case 66 : // Baseline
      break;
    case 77 : // Main
      profileIndex = 1;
      break;
    case 88 : // Extended
      profileIndex = 2;
      break;

    case 100 : // High
      profileIndex = 3;
      break;
  }
  x264_param_apply_profile(&m_context, x264_profile_names[profileIndex]);

  m_context.i_level_idc = level;
  return true;
}


bool H264Encoder::SetFrameWidth(unsigned width)
{
  m_context.i_width = width;
  return true;
}


bool H264Encoder::SetFrameHeight(unsigned height)
{
  m_context.i_height = height;
  return true;
}


unsigned H264Encoder::GetWidth() const
{
  return m_context.i_width;
}


unsigned H264Encoder::GetHeight() const
{
  return m_context.i_height;
}


bool H264Encoder::SetFrameRate(unsigned rate)
{
  m_context.i_fps_num = rate*1000;
  return true;
}


bool H264Encoder::SetTargetBitrate(unsigned rate)
{
  if (rate < 32) // Anything below 32kbps is stupid
    return false;

  unsigned period = m_context.rc.i_vbv_buffer_size*1000/m_context.rc.i_bitrate;

  /* That's how CBR should looks like. Much more stable bitrate output
     but quality degrade on dynamic scenes.  Other case it's ABR. */
  m_context.rc.i_vbv_max_bitrate = m_context.rc.i_bitrate = rate;

  return SetRateControlPeriod(period);
}


bool H264Encoder::SetRateControlPeriod(unsigned period)
{
  // This averages the instantaneous bandwidth over a period, this effectively
  // controls the "tightness" of rate control.
  m_context.rc.i_vbv_buffer_size = m_context.rc.i_bitrate*period/1000;

  return true;
}


bool H264Encoder::SetMaxRTPPayloadSize(unsigned size)
{
  m_encapsulation.SetMaxPayloadSize(size);
  return true;
}


bool H264Encoder::SetMaxNALUSize(unsigned size)
{
  m_context.i_slice_max_size = size;
  return true;
}


bool H264Encoder::SetMaxKeyFramePeriod(unsigned period)
{
  m_context.i_keyint_max = period;
  return true;
}


bool H264Encoder::SetTSTO(unsigned tsto)
{
  m_context.rc.i_qp_max = m_context.rc.i_qp_min + ((51 - m_context.rc.i_qp_min)*tsto)/32;
  return true;
}


bool H264Encoder::ApplyOptions()
{
  if (m_codec != NULL)
    x264_encoder_close(m_codec);

  m_codec = x264_encoder_open(&m_context);
  if (m_codec == NULL) {
    PTRACE(1, HelperTraceName, "Couldn't re-open encoder");
    return false;
  }

  PTRACE(4, HelperTraceName, "Encoder successfully re-opened:"
         << " level " << m_context.i_level_idc
         << ' ' << m_context.i_width << 'x' << m_context.i_height << ","
            " " << (m_context.i_fps_num/m_context.i_fps_den) << "fps,"
            " " << m_context.rc.i_vbv_max_bitrate << "kbps,"
            " max-rtp=" << m_context.i_slice_max_size << ","
            " key-rate=" << m_context.i_keyint_max << ","
            " qp-max=" << m_context.rc.i_qp_max);
  return true;
}


bool H264Encoder::EncodeFrames(const unsigned char * src, unsigned & srcLen,
                               unsigned char * dst, unsigned & dstLen,
                               unsigned /*headerLen*/, unsigned int & flags)
{
  if (m_codec == NULL) {
    PTRACE(1, HelperTraceName, "Encoder not open");
    return 0;
  }

  // if there are NALU's encoded, return them
  if (!m_encapsulation.HasRTPFrames()) {
    // create RTP frame from source buffer
    PluginCodec_RTP srcRTP(src, srcLen);

    // do a validation of size
    size_t payloadSize = srcRTP.GetPayloadSize();
    if (payloadSize < sizeof(PluginCodec_Video_FrameHeader)) {
      PTRACE(1, HelperTraceName, "Video grab far too small, Close down video transmission thread");
      return 0;
    }

    PluginCodec_Video_FrameHeader * header = (PluginCodec_Video_FrameHeader *)srcRTP.GetPayloadPtr();
    if (header->x != 0 || header->y != 0) {
      PTRACE(1, HelperTraceName, "Video grab of partial frame unsupported, Close down video transmission thread");
      return 0;
    }

    if (payloadSize < sizeof(PluginCodec_Video_FrameHeader)+header->width*header->height*3/2) {
      PTRACE(1, HelperTraceName, "Video grab far too small, Close down video transmission thread");
      return 0;
    }

    // if the incoming data has changed size, tell the encoder
    if ((unsigned)m_context.i_width != header->width || (unsigned)m_context.i_height != header->height) {
      PTRACE(4, HelperTraceName, "Detected resolution change " << m_context.i_width << 'x' << m_context.i_height
                                                    << " to " << header->width << 'x' << header->height);
      x264_encoder_close(m_codec);
      m_context.i_width = header->width;
      m_context.i_height = header->height;
      m_codec = x264_encoder_open(&m_context);
      if (m_codec == NULL) {
        PTRACE(1, HelperTraceName, "Couldn't re-open encoder");
        return 0;
      }
      PTRACE(4, HelperTraceName, "Encoder successfully re-opened");
    } 

    // Prepare the frame to be encoded
    x264_picture_t inputPicture;
    x264_picture_init(&inputPicture);
    inputPicture.i_qpplus1 = 0;
    inputPicture.img.i_csp = X264_CSP_I420;
    inputPicture.img.i_stride[0] = header->width;
    inputPicture.img.i_stride[1] = inputPicture.img.i_stride[2] = header->width/2;
    inputPicture.img.plane[0] = (uint8_t *)(((unsigned char *)header) + sizeof(PluginCodec_Video_FrameHeader));
    inputPicture.img.plane[1] = inputPicture.img.plane[0] + header->width*header->height;
    inputPicture.img.plane[2] = inputPicture.img.plane[1] + header->width*header->height/4;
    inputPicture.i_type = flags != 0 ? X264_TYPE_IDR : X264_TYPE_AUTO;

    x264_nal_t *NALUs = NULL;
    int numberOfNALUs = 0;
    while (numberOfNALUs == 0) { // workaround for first 2 packets being 0
      x264_picture_t outputPicture;
      if (x264_encoder_encode(m_codec, &NALUs, &numberOfNALUs, &inputPicture, &outputPicture) < 0) {
        PTRACE(1, HelperTraceName, "x264_encoder_encode failed");
        return 0;
      }
    }

    m_encapsulation.Reset();
    m_encapsulation.Allocate(numberOfNALUs);
    m_encapsulation.SetTimestamp(srcRTP.GetTimestamp());
    for (int i = 0; i < numberOfNALUs; i++)
      m_encapsulation.AddNALU(NALUs[i].i_type, NALUs[i].i_payload-4, NALUs[i].p_payload+4);
  }

  // create RTP frame from destination buffer
  PluginCodec_RTP dstRTP(dst, dstLen);
  m_encapsulation.GetPacket(dstRTP, flags);
  dstLen = (unsigned)dstRTP.GetPacketSize();
  return 1;
}


#else // X264_LICENSED || GPL_HELPER_APP

#if PLUGINCODEC_TRACING
  static char const PipeTraceName[] = "x264-pipe";
#endif

#if WIN32

#ifdef __MINGW32__
static const char DefaultPluginDirs[] = "plugins";
#else
static const char DefaultPluginDirs[] = "." DIR_TOKENISER "C:\\PTLib_Plugins";
#endif

#include <io.h>


class Overlapped : public OVERLAPPED
{
  public:
    Overlapped(HANDLE hEvent)
    {
      memset(this, 0, sizeof(OVERLAPPED));
      this->hEvent = hEvent;
      ::ResetEvent(hEvent);
    }
};


static bool IsExecutable(const char * path)
{
  return _access(path, 4) == 0;
}


H264Encoder::H264Encoder()
  : m_loaded(false)
  , m_hStandardError(NULL)
  , m_hNamedPipe(NULL)
  , m_hEvent(NULL)
  , m_startNewFrame(true)
{
}


H264Encoder::~H264Encoder()
{
  if (m_hStandardError != NULL && !CloseHandle(m_hStandardError))
    PTRACE(1, PipeTraceName, "Failure on closing stderr handle (" << GetLastError() << ')');

  if (m_hNamedPipe != NULL) {
    if (!DisconnectNamedPipe(m_hNamedPipe))
      PTRACE(1, PipeTraceName, "Failure on disconnecting pipe (" << GetLastError() << ')');
    if (!CloseHandle(m_hNamedPipe))
      PTRACE(1, PipeTraceName, "Failure on closing pipe handle (" << GetLastError() << ')');
  }

  if (m_hEvent != NULL && !CloseHandle(m_hEvent))
    PTRACE(1, PipeTraceName, "Failure on closing event handle (" << GetLastError() << ')');
}


bool H264Encoder::OpenPipeAndExecute(void * instance, const char * executablePath)
{
  if ((m_hEvent = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL) {
    PTRACE(1, PipeTraceName, "Failure on creating event (" << GetLastError() << ')');
    return false;
  }

  char pipeName[_MAX_PATH];
  _snprintf(pipeName, sizeof(pipeName), "\\\\.\\pipe\\x264-%d-%p", GetCurrentProcessId(), instance);
  if ((m_hNamedPipe = CreateNamedPipe(pipeName,
#ifdef _MSC_VER
                                      FILE_FLAG_FIRST_PIPE_INSTANCE // (not supported by minGW lib)
#endif
                                      |PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED,
                                      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, // deny via network ACE
                                      1,    // Max instances
                                      4096, // Output buffer
                                      4096, // Input buffer
                                      5000, // Timeout
                                      NULL)) == INVALID_HANDLE_VALUE) {
    PTRACE(1, PipeTraceName, "Failure on creating Pipe (" << GetLastError() << ')');
    return false;
  }

  char command[1024];
  _snprintf(command, sizeof(command), "%s %s", executablePath, pipeName);

  SECURITY_ATTRIBUTES security;
  security.nLength = sizeof(security);
  security.lpSecurityDescriptor = NULL;
  security.bInheritHandle = TRUE;

  STARTUPINFO si;
  memset(&si, 0, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = INVALID_HANDLE_VALUE;
  si.hStdOutput = INVALID_HANDLE_VALUE;
  CreatePipe(&m_hStandardError, &si.hStdError, &security, 1);
  SetHandleInformation(m_hStandardError, HANDLE_FLAG_INHERIT, 0);
  si.hStdOutput = si.hStdError;

  PROCESS_INFORMATION pi;
  memset(&pi, 0, sizeof(pi));

  // Start the child process. 
  if (!CreateProcess(NULL,        // No module name (use command line)
                     command,     // Command line
                     NULL,        // Process handle not inheritable
                     NULL,        // Thread handle not inheritable
                     TRUE,        // Set handle inheritance to FALSE
                     CREATE_NO_WINDOW, // Creation flags
                     NULL,        // Use parent's environment block
                     NULL,        // Use parent's starting directory 
                     &si,         // Pointer to STARTUPINFO structure
                     &pi))        // Pointer to PROCESS_INFORMATION structure
  {
      PTRACE(1, PipeTraceName, "Couldn't create child process (" << GetLastError() << ')');
      return false;
  }

  CloseHandle(si.hStdError);

  PTRACE(4, PipeTraceName, "Successfully created child process " << pi.dwProcessId << " using " << command);

  CheckStandardError();

  Overlapped overlapped(m_hEvent);
  if (ConnectNamedPipe(m_hNamedPipe, &overlapped) ||
      GetLastError() == ERROR_PIPE_CONNECTED ||
      CheckCompleted(overlapped))
    return true;

  DWORD tick = GetTickCount();
  while ((GetTickCount() -  tick) < 2000)
    CheckStandardError();

  PTRACE(1, PipeTraceName, "Could not establish communication with child process (" << GetLastError() << ')');
  return false;
}


void H264Encoder::CheckStandardError()
{
  DWORD available;
  while (PeekNamedPipe(m_hStandardError, NULL, 0, NULL, &available, NULL) && available > 0) {
    char * str = (char *)alloca(available+1);
    if (!ReadFile(m_hStandardError, str, available, &available, NULL))
      return;

    str[available] = '\0';
    m_errorOutput += str;
    std::string::size_type pos;
    while ((pos = m_errorOutput.find("\r\n")) != std::string::npos) {
      PTRACE(1, "x264-help", m_errorOutput.substr(0, pos));
      m_errorOutput.erase(0, pos+2);
    }
  }
}


bool H264Encoder::CheckCompleted(OVERLAPPED & overlapped, LPDWORD bytes)
{
  DWORD dummy;

  if (GetLastError() != ERROR_IO_PENDING)
    return false;

  for (int retry = 0; retry < 50; ++retry) {
    if (WaitForSingleObject(overlapped.hEvent, 100) == WAIT_OBJECT_0)
      return GetOverlappedResult(m_hNamedPipe, &overlapped, bytes != NULL ? bytes : &dummy, FALSE) != FALSE;
    CheckStandardError();
  }

  CancelIo(m_hNamedPipe);
  SetLastError(ERROR_TIMEOUT);
  return false;
}


bool H264Encoder::ReadPipe(void * ptr, size_t len)
{
  Overlapped overlapped(m_hEvent);
  DWORD bytesRead;
  if ((ReadFile(m_hNamedPipe, ptr, (DWORD)len, &bytesRead, &overlapped) ||
       CheckCompleted(overlapped, &bytesRead)) && bytesRead == len)
    return true;

  PTRACE(1, PipeTraceName, "Failure on read (" << GetLastError() << ')');
  return false;
}


bool H264Encoder::WritePipe(const void * ptr, size_t len)
{
  Overlapped overlapped(m_hEvent);
  DWORD bytesWritten;
  if ((WriteFile(m_hNamedPipe, ptr, (DWORD)len, &bytesWritten, &overlapped) ||
       CheckCompleted(overlapped, &bytesWritten)) && bytesWritten == len)
    return true;

  PTRACE(1, PipeTraceName, "Failure on write (" << GetLastError() << ')');
  return false;
}


#else // WIN32

#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>


static const char DefaultPluginDirs[] = "." DIR_TOKENISER
                                        VID_PLUGIN_DIR DIR_TOKENISER
                                        "/usr/lib" DIR_TOKENISER "/usr/local/lib";


static bool IsExecutable(const char * path)
{
  return access(path, R_OK|X_OK) == 0;
}


H264Encoder::H264Encoder()
  : m_loaded(false)
  , m_pipeToProcess(-1)
  , m_pipeFromProcess(-1)
  , m_startNewFrame(true)
{
}


H264Encoder::~H264Encoder()
{
  if (m_pipeToProcess >= 0) {
    close(m_pipeToProcess);
    m_pipeToProcess = -1;
  }

  if (m_pipeFromProcess >= 0) {
    close(m_pipeFromProcess);
    m_pipeFromProcess = -1;
  }

  if (remove(m_ulName) == -1)
    PTRACE(1, PipeTraceName, "Error when trying to remove UL named pipe - " << strerror(errno));
  if (remove(m_dlName) == -1)
    PTRACE(1, PipeTraceName, "Error when trying to remove DL named pipe - " << strerror(errno));
}


bool H264Encoder::OpenPipeAndExecute(void * instance, const char * executablePath)
{
  snprintf(m_dlName, sizeof(m_dlName), "/tmp/x264-%d-%p-dl", getpid(), instance);
  snprintf(m_ulName, sizeof(m_ulName), "/tmp/x264-%d-%p-ul", getpid(), instance);

  umask(0);

#ifdef HAVE_MKFIFO
  if (mkfifo(m_dlName, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) {
    PTRACE(1, PipeTraceName, "Error when trying to create DL named pipe");
    return false;
  }
  if (mkfifo(m_ulName, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) {
    PTRACE(1, PipeTraceName, "Error when trying to create UL named pipe");
    return false;
  }
#else
  if (mknod(m_dlName, S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, 0)) {
    PTRACE(1, PipeTraceName, "Error when trying to create named pipe");
    return false;
  }
  if (mknod(m_ulName, S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, 0)) {
    PTRACE(1, PipeTraceName, "Error when trying to create named pipe");
    return false;
  }
#endif /* HAVE_MKFIFO */

  m_pid = fork();
  if (m_pid < 0) {
    PTRACE(1, PipeTraceName, "Error when trying to fork");
    return false;
  }

  if (m_pid == 0) {
    // If succeeds, does not return
    execl(executablePath, executablePath, m_dlName, m_ulName, NULL);
    PTRACE(1, PipeTraceName, "Error when trying to execute GPL process  " << executablePath << " - " << strerror(errno));
    return false;
  }

  m_pipeToProcess = open(m_dlName, O_WRONLY);
  if (m_pipeToProcess < 0) { 
    PTRACE(1, PipeTraceName, "Error when opening DL named pipe - " << strerror(errno));
    return false;
  }

  m_pipeFromProcess = open(m_ulName, O_RDONLY);
  if (m_pipeFromProcess < 0) { 
    PTRACE(1, PipeTraceName, "Error when opening UL named pipe - " << strerror(errno));
    return false;
  }

  PTRACE(4, PipeTraceName, "Started GPL process id " << m_pid << " using " << executablePath);
  return true;
}


bool H264Encoder::ReadPipe(void * ptr, size_t len)
{
  int result = read(m_pipeFromProcess, ptr, len);
  if (result == len)
    return true;

  PTRACE(1, PipeTraceName, "Error reading pipe (" << result << ") - " << strerror(errno));
  if (kill(m_pid, 0) < 0)
    PTRACE(1, PipeTraceName, "Sub-process no longer running!");
  return false;
}


bool H264Encoder::WritePipe(const void * ptr, size_t len)
{
  int result = write(m_pipeToProcess, ptr, len);
  if (result == len)
    return true;

  PTRACE(1, PipeTraceName, "Error writing pipe (" << result << ") - " << strerror(errno));
  if (kill(m_pid, 0) < 0)
    PTRACE(1, PipeTraceName, "Sub-process no longer running!");
  return false;
}


#endif // WIN32


bool H264Encoder::Load(void * instance)
{
  if (m_loaded)
    return true;

  const char * pluginDirs = ::getenv("PTLIBPLUGINDIR");
  if (pluginDirs == NULL) {
    pluginDirs = ::getenv("PWLIBPLUGINDIR");
    if (pluginDirs == NULL)
      pluginDirs = DefaultPluginDirs;
  }

  static const char ExecutableName[] = EXECUTABLE_NAME;

  char executablePath[500];
  char * tempDirs = strdup(pluginDirs);
  const char * token = strtok(tempDirs, DIR_TOKENISER);
  while (token != NULL) {
    snprintf(executablePath, sizeof(executablePath), "%s/%s", token, ExecutableName);
    if (IsExecutable(executablePath))
      break;

    token = strtok(NULL, DIR_TOKENISER);
  }
  free(tempDirs);

  if (token == NULL) {
    PTRACE(1, PipeTraceName, "Could not find GPL process executable " << ExecutableName << " in " << pluginDirs);
    return false;
  }

  if (!OpenPipeAndExecute(instance, executablePath))
    return false;

  unsigned msg = H264ENCODERCONTEXT_CREATE;
  if (!WritePipe(&msg, sizeof(msg)) || !ReadPipe(&msg, sizeof(msg))) {
    PTRACE(1, PipeTraceName, "GPL process did not initialise.");
    return false;
  }

  PTRACE(4, PipeTraceName, "Successfully established communication with GPL process version " << msg);
  m_loaded = true;
  return true;
}


bool H264Encoder::WriteValue(unsigned msg, unsigned value)
{
  unsigned reply;
  return WritePipe(&msg, sizeof(msg)) &&
         WritePipe(&value, sizeof(value)) &&
         ReadPipe(&reply, sizeof(reply)) && reply == msg;
}


bool H264Encoder::SetProfileLevel(unsigned profile, unsigned level, unsigned constraints)
{
  return WriteValue(SET_PROFILE_LEVEL, (profile << 16) | (constraints << 8) | level);
}


bool H264Encoder::SetFrameWidth(unsigned width)
{
  return WriteValue(SET_FRAME_WIDTH, width);
}


bool H264Encoder::SetFrameHeight(unsigned height)
{
  return WriteValue(SET_FRAME_HEIGHT, height);
}


unsigned H264Encoder::GetWidth() const
{
  return 0; /// dummy
}


unsigned H264Encoder::GetHeight() const
{
  return 0; /// dummy
}


bool H264Encoder::SetFrameRate(unsigned rate)
{
  return WriteValue(SET_FRAME_RATE, rate);
}


bool H264Encoder::SetTargetBitrate(unsigned rate)
{
  return WriteValue(SET_TARGET_BITRATE, rate);
}


bool H264Encoder::SetRateControlPeriod(unsigned period)
{
  return WriteValue(SET_RATE_CONTROL_PERIOD, period);
}


bool H264Encoder::SetMaxRTPPayloadSize(unsigned size)
{
  return WriteValue(SET_MAX_PAYLOAD_SIZE, size);
}


bool H264Encoder::SetMaxNALUSize(unsigned size)
{
  return WriteValue(SET_MAX_NALU_SIZE, size);
}


bool H264Encoder::SetTSTO(unsigned tsto)
{
  return WriteValue(SET_TSTO, tsto);
}


bool H264Encoder::SetMaxKeyFramePeriod(unsigned period)
{
  return WriteValue(SET_MAX_KEY_FRAME_PERIOD, period);
}


bool H264Encoder::ApplyOptions()
{
  unsigned msg = APPLY_OPTIONS;
  return WritePipe(&msg, sizeof(msg)) && ReadPipe(&msg, sizeof(msg)) && msg == APPLY_OPTIONS;
}


bool H264Encoder::EncodeFrames(const unsigned char * src, unsigned & srcLen,
                               unsigned char * dst, unsigned & dstLen,
                               unsigned headerLen, unsigned int & flags)
{
  unsigned msg;
  if (m_startNewFrame) {
    msg = ENCODE_FRAMES;
    if (!WritePipe(&msg, sizeof(msg)) ||
        !WritePipe(&srcLen, sizeof(srcLen)) ||
        !WritePipe(src, srcLen) ||
        !WritePipe(&headerLen, sizeof(headerLen)) ||
        !WritePipe(dst, headerLen) ||
        !WritePipe(&flags, sizeof(flags)))
      return false;
  }
  else {
    msg = ENCODE_FRAMES_BUFFERED;
    if (!WritePipe(&msg, sizeof(msg)))
      return false;
  }

  unsigned returnValue = 0;
  if (!ReadPipe(&msg, sizeof(msg)) ||
      !ReadPipe(&dstLen, sizeof(dstLen)) ||
      !ReadPipe(dst, dstLen) ||
      !ReadPipe(&flags, sizeof(flags)) ||
      !ReadPipe(&returnValue, sizeof(returnValue)))
    return false;

  m_startNewFrame = (flags & 1) != 0;
  return returnValue != 0;
}


#endif // X264_LICENSED || GPL_HELPER_APP

///////////////////////////////////////////////////////////////////////////////
