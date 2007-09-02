/*
 * H.264 Plugin codec for OpenH323/OPAL
 *
 * Copyright (C) Matthias Schneider, All Rights Reserved
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
 * Contributor(s): Matthias Schneider (ma30002000@yahoo.de)
 *
 */

/*
  Notes
  -----

 */

#ifndef __H264PIPE_H__
#define __H264PIPE_H__ 1
#include "shared/pipes.h"
#include <fstream>

typedef unsigned char u_char;

class H264EncCtx
{
  public:
     H264EncCtx();
     ~H264EncCtx();
     bool Load();
     bool isLoaded() { return loaded; };
     void call(unsigned msg);
     void call(unsigned msg, int);
     void call(unsigned msg , const u_char * src, unsigned & srcLen, u_char * dst, unsigned & dstLen, unsigned & headerLen, unsigned int & flags, int & ret);

  protected:
     bool createPipes();
     void closeAndRemovePipes();
     void writeStream (const char* data, unsigned bytes);
     void readStream (char* data, unsigned bytes);
     void flushStream ();
     bool findGplProcess();
     bool checkGplProcessExists (const char * dir);
     void execGplProcess();
     void cpCloseAndExit();

     char dlName [512];
     char ulName [512];
     char gplProcess [512];
     std::ofstream dlStream;
     std::ifstream ulStream;
     int width;
     int height;
     int size;
     bool startNewFrame;
     bool loaded;
     bool pipesCreated;
     bool pipesOpened;
     
     // only for signaling failed execution of helper process
     std::ifstream cpDLStream;
     std::ofstream cpULStream;
};
#endif /* __PIPE_H__ */
