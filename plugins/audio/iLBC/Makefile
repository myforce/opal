#
# Makefile for iLBC Codec Plugin
#
# Copyright (C) 2004 Post Increment, All Rights Reserved
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is Open H323 library.
#
# The Initial Developer of the Original Code is Post Increment
#
# Contributor(s): ______________________________________.
#
# $Revision$
# $Author$
# $Date$
#

BASENAME := iLBC

SRCDIR   := $(CURDIR)/iLBC
SOURCES  := $(CURDIR)/ilbccodec.c

PLUGIN_CPPFLAGS = $(ILBC_CFLAGS)
PLUGIN_LIBS     = $(ILBC_LIBS)

PLUGIN_SYSTEM  = $(ILBC_SYSTEM)
LOCAL_SOURCES := $(SRCDIR)/anaFilter.c \
                 $(SRCDIR)/constants.c \
                 $(SRCDIR)/createCB.c \
                 $(SRCDIR)/doCPLC.c \
                 $(SRCDIR)/enhancer.c \
                 $(SRCDIR)/filter.c \
                 $(SRCDIR)/FrameClassify.c \
                 $(SRCDIR)/gainquant.c \
                 $(SRCDIR)/getCBvec.c \
                 $(SRCDIR)/helpfun.c \
                 $(SRCDIR)/hpInput.c \
                 $(SRCDIR)/hpOutput.c \
                 $(SRCDIR)/iCBConstruct.c \
                 $(SRCDIR)/iCBSearch.c \
                 $(SRCDIR)/iLBC_decode.c \
                 $(SRCDIR)/iLBC_encode.c \
                 $(SRCDIR)/LPCdecode.c \
                 $(SRCDIR)/LPCencode.c \
                 $(SRCDIR)/lsf.c \
                 $(SRCDIR)/packing.c \
                 $(SRCDIR)/StateConstructW.c \
                 $(SRCDIR)/StateSearchW.c \
                 $(SRCDIR)/syntFilter.c

INSTALL_DIR = $(AUD_PLUGIN_DIR)
include $(dir $(lastword $(MAKEFILE_LIST)))../../plugin_inc.mak


###########################################
