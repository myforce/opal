#
# toplevel.mak
#
# Make file for Open Phone Abstraction library
#
# Copyright (c) 2001 Equivalence Pty. Ltd.
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
# The Original Code is Open Phone Abstraction Library.
#
# The Initial Developer of the Original Code is Equivalence Pty. Ltd.
#
# Contributor(s): ______________________________________.
#
# $Revision$
# $Author$
# $Date$
#

OPAL_TOP_LEVEL_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))..)
ifneq ($(CURDIR),$(OPAL_TOP_LEVEL_DIR))
  $(info Doing out-of-source OPAL build in $(CURDIR))
endif

export OPALDIR := $(OPAL_TOP_LEVEL_DIR)
export OPAL_PLATFORM_DIR := $(CURDIR)
OPAL_BUILDING_ITSELF := yes
include $(OPAL_TOP_LEVEL_DIR)/make/opal.mak


###############################################################################

SUBDIRS := 

ifeq ($(OPAL_PLUGINS),yes)
  SUBDIRS += $(OPAL_TOP_LEVEL_DIR)/plugins
endif

ifeq ($(OPAL_SAMPLES),yes)

  SUBDIRS += $(OPAL_TOP_LEVEL_DIR)/samples/c_api

  ifeq ($(OPAL_PTLIB_HTTPSVC),yes)
    SUBDIRS += $(OPAL_TOP_LEVEL_DIR)/samples/server
  endif

  ifeq ($(OPAL_HAS_PCSS),yes)
    SUBDIRS += $(OPAL_TOP_LEVEL_DIR)/samples/console
    ifneq (,$(shell which wx-config))
      #SUBDIRS += $(OPAL_TOP_LEVEL_DIR)/samples/openphone
    endif
  endif
  ifeq ($(OPAL_VIDEO),yes)
    SUBDIRS += $(OPAL_TOP_LEVEL_DIR)/samples/codectest
  endif
  ifeq ($(OPAL_HAS_PCSS)$(OPAL_IVR)$(OPAL_PTLIB_CLI),yesyesyes)
    SUBDIRS += $(OPAL_TOP_LEVEL_DIR)/samples/callgen
  endif
  ifeq ($(OPAL_SIP)$(OPAL_HAS_PRESENCE), yesyes)
    SUBDIRS += $(OPAL_TOP_LEVEL_DIR)/samples/test/presence
  endif
  ifeq ($(OPAL_PTLIB_CLI)$(OPAL_FAX), yesyes)
    SUBDIRS += $(OPAL_TOP_LEVEL_DIR)/samples/fax
  endif
  ifeq ($(OPAL_PTLIB_CLI)$(OPAL_IVR),yesyes)
    SUBDIRS += $(OPAL_TOP_LEVEL_DIR)/samples/ivr
  endif
  ifeq ($(OPAL_PTLIB_CLI)$(OPAL_IVR)$(OPAL_HAS_MIXER),yesyesyes)
    SUBDIRS += $(OPAL_TOP_LEVEL_DIR)/samples/mcu
  endif
  ifeq ($(OPAL_GSTREAMER),yes)
    SUBDIRS += $(OPAL_TOP_LEVEL_DIR)/samples/gstreamer
  endif

endif # OPAL_SAMPLES


################################################################################

OBJDIR	= $(OPAL_OBJDIR)

VERSION_FILE = $(OPAL_TOP_LEVEL_DIR)/version.h
REVISION_FILE = $(OPAL_TOP_LEVEL_DIR)/revision.h

DOXYGEN_CFG := $(OPAL_TOP_LEVEL_DIR)/opal_cfg.dxy

ifneq ($(STATIC_BUILD),yes)
  SHARED_LIB_LINK = $(OPAL_SHARED_LIB_LINK)
  SHARED_LIB_FILE = $(OPAL_SHARED_LIB_FILE)
endif
STATIC_LIB_FILE = $(OPAL_STATIC_LIB_FILE)


OPAL_SRCDIR = $(OPAL_TOP_LEVEL_DIR)/src
OPAL_INCDIR = $(OPAL_TOP_LEVEL_DIR)/include
ASN_SRCDIR := $(OPAL_SRCDIR)/asn
ASN_INCDIR := $(OPAL_INCDIR)/asn

VPATH_CXX := $(OPAL_SRCDIR)/opal \
             $(OPAL_SRCDIR)/ep \
             $(OPAL_SRCDIR)/rtp \
             $(OPAL_SRCDIR)/lids \
             $(OPAL_SRCDIR)/codec \
             $(OPAL_SRCDIR)/t38 \
             $(OPAL_SRCDIR)/im

ifeq ($(OPAL_PTLIB_ASN), yes)
  VPATH_CXX += $(ASN_SRCDIR)
endif

VPATH_C := $(OPAL_SRCDIR)/codec


###############################################################################

SOURCES += $(OPAL_SRCDIR)/opal/manager.cxx \
           $(OPAL_SRCDIR)/opal/endpoint.cxx \
           $(OPAL_SRCDIR)/opal/connection.cxx \
           $(OPAL_SRCDIR)/opal/call.cxx \
           $(OPAL_SRCDIR)/opal/mediafmt.cxx \
           $(OPAL_SRCDIR)/opal/mediatype.cxx \
           $(OPAL_SRCDIR)/opal/mediasession.cxx \
           $(OPAL_SRCDIR)/opal/mediastrm.cxx \
           $(OPAL_SRCDIR)/opal/patch.cxx \
           $(OPAL_SRCDIR)/opal/transcoders.cxx \
           $(OPAL_SRCDIR)/opal/transports.cxx \
           $(OPAL_SRCDIR)/opal/guid.cxx \
           $(OPAL_SRCDIR)/rtp/rtp.cxx \
           $(OPAL_SRCDIR)/rtp/rtp_session.cxx \
           $(OPAL_SRCDIR)/rtp/rtp_stream.cxx \
           $(OPAL_SRCDIR)/rtp/rtp_fec.cxx \
           $(OPAL_SRCDIR)/rtp/jitter.cxx \
           $(OPAL_SRCDIR)/rtp/metrics.cxx \
           $(OPAL_SRCDIR)/rtp/pcapfile.cxx \
           $(OPAL_SRCDIR)/rtp/rtpep.cxx \
           $(OPAL_SRCDIR)/rtp/rtpconn.cxx \
           $(OPAL_SRCDIR)/ep/localep.cxx \
           $(OPAL_SRCDIR)/opal/opal_c.cxx \
           $(OPAL_SRCDIR)/opal/pres_ent.cxx

ifeq ($(OPAL_PTLIB_CLI), yes)
  SOURCES += $(OPAL_SRCDIR)/opal/console_mgr.cxx 
endif

ifeq ($(OPAL_IVR), yes)
  SOURCES += $(OPAL_SRCDIR)/ep/ivr.cxx \
             $(OPAL_SRCDIR)/ep/opalvxml.cxx 
endif


ifeq ($(OPAL_HAS_MIXER), yes)
  SOURCES += $(OPAL_SRCDIR)/opal/recording.cxx \
             $(OPAL_SRCDIR)/ep/opalmixer.cxx
  ifeq ($(target_os),mingw)
    LIBS := -lvfw32 $(LIBS)
  endif
endif

ifeq ($(OPAL_HAS_PCSS), yes)
  SOURCES += $(OPAL_SRCDIR)/ep/pcss.cxx
endif

ifeq ($(OPAL_GSTREAMER),yes)
  SOURCES += $(OPAL_SRCDIR)/ep/GstEndPoint.cxx
endif


########################################

ifeq ($(OPAL_H323),yes)
  VPATH_CXX += $(OPAL_SRCDIR)/h323 \
               $(OPAL_SRCDIR)/t120

  SIMPLE_ASN_FILES := x880 mcs gcc

  ifeq ($(OPAL_H450),yes)
    SIMPLE_ASN_FILES += h4501 h4502 h4503 h4504 h4505 h4506 h4507 h4508 h4509 h45010 h45011
    SOURCES          += $(OPAL_SRCDIR)/h323/h450pdu.cxx 
  endif

  ASN_H_FILES   := $(addprefix $(ASN_INCDIR)/,$(addsuffix .h,  $(SIMPLE_ASN_FILES)))
  ASN_CXX_FILES := $(addprefix $(ASN_SRCDIR)/,$(addsuffix .cxx,$(SIMPLE_ASN_FILES)))

  ASN_H_FILES   += $(ASN_INCDIR)/h225.h
  ASN_CXX_FILES += $(ASN_SRCDIR)/h225_1.cxx $(ASN_SRCDIR)/h225_2.cxx

  ASN_H_FILES   += $(ASN_INCDIR)/h235.h $(ASN_SRCDIR)/h235_t.cxx
  ASN_CXX_FILES += $(ASN_SRCDIR)/h235.cxx

  ifeq ($(OPAL_SRTP)$(OPAL_H235_8), yesyes)
    ASN_CXX_FILES += $(ASN_SRCDIR)/h235_srtp.cxx
  endif

  ASN_H_FILES   += $(ASN_INCDIR)/h245.h
  ASN_CXX_FILES += $(ASN_SRCDIR)/h245_1.cxx $(ASN_SRCDIR)/h245_2.cxx $(ASN_SRCDIR)/h245_3.cxx

  ASN_H_FILES   += $(ASN_INCDIR)/h248.h
  ASN_CXX_FILES += $(ASN_SRCDIR)/h248.cxx

  ifeq ($(OPAL_H501),yes)
    ASN_H_FILES   += $(ASN_INCDIR)/h501.h
    ASN_CXX_FILES += $(ASN_SRCDIR)/h501.cxx
  endif

  .PRECIOUS: $(ASN_CXX_FILES) $(ASN_H_FILES)

  SOURCES += $(ASN_CXX_FILES) \
             $(OPAL_SRCDIR)/h323/h323ep.cxx \
             $(OPAL_SRCDIR)/h323/h323.cxx \
             $(OPAL_SRCDIR)/h323/h323caps.cxx \
             $(OPAL_SRCDIR)/h323/h323neg.cxx \
             $(OPAL_SRCDIR)/h323/h323pdu.cxx \
             $(OPAL_SRCDIR)/h323/h323rtp.cxx \
             $(OPAL_SRCDIR)/h323/channels.cxx \
             $(OPAL_SRCDIR)/h323/svcctrl.cxx \
             $(OPAL_SRCDIR)/h323/q931.cxx \
             $(OPAL_SRCDIR)/h323/transaddr.cxx \
             $(OPAL_SRCDIR)/h323/gkclient.cxx \
             $(OPAL_SRCDIR)/h323/gkserver.cxx \
             $(OPAL_SRCDIR)/h323/h225ras.cxx \
             $(OPAL_SRCDIR)/h323/h323trans.cxx \
             $(OPAL_SRCDIR)/h323/h235auth.cxx \

  ifeq ($(OPAL_H501),yes)
    SOURCES += $(OPAL_SRCDIR)/h323/h501pdu.cxx \
               $(OPAL_SRCDIR)/h323/h323annexg.cxx \
               $(OPAL_SRCDIR)/h323/peclient.cxx 
  endif

  ifeq ($(OPAL_H460), yes)
    VPATH_CXX += $(OPAL_SRCDIR)/h460
    SOURCES += $(OPAL_SRCDIR)/h460/h4601.cxx \
               $(OPAL_SRCDIR)/h460/h46018.cxx \
               $(OPAL_SRCDIR)/h460/h46019.cxx \
               $(OPAL_SRCDIR)/h460/h4609.cxx \
               $(OPAL_SRCDIR)/h460/h460p.cxx \
               $(OPAL_SRCDIR)/h460/h460pres.cxx \
               $(OPAL_SRCDIR)/h460/h460tm.cxx \
               $(OPAL_SRCDIR)/h460/h46024b.cxx \
               $(OPAL_SRCDIR)/h460/h460_std18.cxx \
               $(OPAL_SRCDIR)/h460/h460_std19.cxx \
               $(OPAL_SRCDIR)/h460/h460_std23.cxx \
               $(OPAL_SRCDIR)/h460/h460_std24.cxx
  endif

  ifeq ($(OPAL_PTLIB_SSL), yes)
    SOURCES += $(OPAL_SRCDIR)/h235auth1.cxx
    lIBS := -lcrypto $(LIBS)
  endif

  ifeq ($(OPAL_T120DATA), yes)
    SOURCES += $(OPAL_SRCDIR)/t120/t120proto.cxx \
               $(OPAL_SRCDIR)/t120/h323t120.cxx \
               $(OPAL_SRCDIR)/t120/x224.cxx 
  endif

  ifeq ($(OPAL_T38_CAP), yes)
    SOURCES += $(OPAL_SRCDIR)/t38/h323t38.cxx
  endif

  ifeq ($(OPAL_HAS_H224), yes)
    SOURCES += $(OPAL_SRCDIR)/h224/h323h224.cxx
  endif

  ifeq ($(OPAL_H235_6), yes)
    SOURCES += $(OPAL_SRCDIR)/h323/h235dh.cxx \
               $(OPAL_SRCDIR)/h323/h235_session.cxx 
  endif

endif # OPAL_H323


########################################

ifeq ($(OPAL_IAX2), yes)
  VPATH_CXX += $(OPAL_SRCDIR)/iax2 
  SOURCES += $(OPAL_SRCDIR)/iax2/callprocessor.cxx \
             $(OPAL_SRCDIR)/iax2/frame.cxx \
             $(OPAL_SRCDIR)/iax2/iax2con.cxx \
             $(OPAL_SRCDIR)/iax2/iax2ep.cxx \
             $(OPAL_SRCDIR)/iax2/iax2medstrm.cxx \
             $(OPAL_SRCDIR)/iax2/iedata.cxx \
             $(OPAL_SRCDIR)/iax2/ies.cxx \
             $(OPAL_SRCDIR)/iax2/processor.cxx \
             $(OPAL_SRCDIR)/iax2/receiver.cxx \
             $(OPAL_SRCDIR)/iax2/regprocessor.cxx\
             $(OPAL_SRCDIR)/iax2/remote.cxx \
             $(OPAL_SRCDIR)/iax2/safestrings.cxx \
             $(OPAL_SRCDIR)/iax2/sound.cxx \
             $(OPAL_SRCDIR)/iax2/specialprocessor.cxx \
             $(OPAL_SRCDIR)/iax2/transmit.cxx 
endif # OPAL_IAX2


########################################

ifeq ($(OPAL_SDP),yes)
  VPATH_CXX += $(OPAL_SRCDIR)/sdp
  SOURCES += $(OPAL_SRCDIR)/sdp/sdpep.cxx \
             $(OPAL_SRCDIR)/sdp/sdphttpep.cxx \
             $(OPAL_SRCDIR)/sip/sdp.cxx \
             $(OPAL_SRCDIR)/sip/ice.cxx
endif # OPAL_SDP


########################################

ifeq ($(OPAL_SIP),yes)
  VPATH_CXX += $(OPAL_SRCDIR)/sip
  SOURCES += $(OPAL_SRCDIR)/sip/sipep.cxx \
             $(OPAL_SRCDIR)/sip/sipcon.cxx \
             $(OPAL_SRCDIR)/sip/sippdu.cxx \
             $(OPAL_SRCDIR)/sip/handlers.cxx \
             $(OPAL_SRCDIR)/sip/sippres.cxx

  ifeq ($(OPAL_T38_CAP), yes)
    SOURCES += $(OPAL_SRCDIR)/t38/sipt38.cxx
  endif
endif


########################################

ifeq ($(OPAL_SKINNY),yes)
  SOURCES += $(OPAL_SRCDIR)/ep/skinnyep.cxx
endif


########################################

ifeq ($(OPAL_LID), yes)
  SOURCES += $(OPAL_SRCDIR)/lids/lid.cxx \
             $(OPAL_SRCDIR)/lids/lidep.cxx \
             $(OPAL_SRCDIR)/lids/lidpluginmgr.cxx
endif

ifeq ($(OPAL_CAPI), yes)
  SOURCES += $(OPAL_SRCDIR)/lids/capi_ep.cxx
endif

ifeq ($(OPAL_DAHDI), yes)
  SOURCES += $(OPAL_SRCDIR)/lids/dahdi_ep.cxx
endif


####################################################

ifeq ($(OPAL_SRTP), yes)
  SOURCES += $(OPAL_SRCDIR)/rtp/srtp_session.cxx \
             $(OPAL_SRCDIR)/rtp/dtls_srtp_session.cxx

  ifneq ($(SRTP_SYSTEM),yes)
    SRTP_DIR := $(OPAL_TOP_LEVEL_DIR)/src/rtp/libsrtp
    CPPFLAGS += -I$(SRTP_DIR)/include -I$(SRTP_DIR)/crypto/include
    LIBS     := -L$(SRTP_DIR) -lsrtp $(LIBS)
    SRTP_LIB := $(SRTP_DIR)/libsrtp.a
  endif
endif

ifeq ($(OPAL_ZRTP),yes)
  SOURCES += $(OPAL_SRCDIR)/rtp/opalzrtp.cxx \
             $(OPAL_SRCDIR)/rtp/zrtpudp.cxx
endif


####################################################

ifneq (,$(SWIG))
  ifeq ($(OPAL_JAVA), yes)
    JAVA_SRCDIR      = $(OPAL_SRCDIR)/java
    JAVA_WRAPPER     = $(JAVA_SRCDIR)/java_swig_wrapper.c
    VPATH_C         += $(JAVA_SRCDIR)
    SOURCES         += $(JAVA_WRAPPER)
  endif

  ifeq ($(OPAL_RUBY), yes)
    RUBY_SRCDIR      = $(OPAL_SRCDIR)/ruby
    RUBY_WRAPPER     = $(JAVA_SRCDIR)/ruby_swig_wrapper.c
    VPATH_C         += $(RUBY_SRCDIR)
    SOURCES         += $(RUBY_WRAPPER)
  endif
endif # SWIG


####################################################
# IM/MSRP

SOURCES += $(OPAL_SRCDIR)/im/t140.cxx \
           $(OPAL_SRCDIR)/im/im_mf.cxx \
           $(OPAL_SRCDIR)/im/rfc4103.cxx 

ifeq ($(OPAL_HAS_MSRP), yes)
  SOURCES += $(OPAL_SRCDIR)/im/msrp.cxx 
endif
ifeq ($(OPAL_HAS_SIPIM), yes)
  SOURCES += $(OPAL_SRCDIR)/im/sipim.cxx 
endif


####################################################
# Software codecs

SOURCES += $(OPAL_SRCDIR)/codec/g711codec.cxx \
           $(OPAL_SRCDIR)/codec/g711.c \
           $(OPAL_SRCDIR)/codec/g722mf.cxx \
           $(OPAL_SRCDIR)/codec/g7221mf.cxx \
           $(OPAL_SRCDIR)/codec/g7222mf.cxx \
           $(OPAL_SRCDIR)/codec/g7231mf.cxx \
           $(OPAL_SRCDIR)/codec/g726mf.cxx \
           $(OPAL_SRCDIR)/codec/g728mf.cxx \
           $(OPAL_SRCDIR)/codec/g729mf.cxx \
           $(OPAL_SRCDIR)/codec/gsm0610mf.cxx \
           $(OPAL_SRCDIR)/codec/gsmamrmf.cxx \
           $(OPAL_SRCDIR)/codec/iLBCmf.cxx \
           $(OPAL_SRCDIR)/codec/speexmf.cxx \
           $(OPAL_SRCDIR)/codec/t38mf.cxx \
           $(OPAL_SRCDIR)/codec/rfc2833.cxx \
           $(OPAL_SRCDIR)/codec/opalwavfile.cxx \
           $(OPAL_SRCDIR)/codec/silencedetect.cxx \
           $(OPAL_SRCDIR)/codec/opalpluginmgr.cxx \
           $(OPAL_SRCDIR)/codec/ratectl.cxx 

ifeq ($(OPAL_VIDEO), yes)
  SOURCES += $(OPAL_SRCDIR)/codec/vidcodec.cxx \
             $(OPAL_SRCDIR)/codec/h261mf.cxx \
             $(OPAL_SRCDIR)/codec/h263mf.cxx \
             $(OPAL_SRCDIR)/codec/h264mf.cxx \
             $(OPAL_SRCDIR)/codec/mpeg4mf.cxx
endif

ifeq ($(OPAL_RFC4175), yes)
  SOURCES += $(OPAL_SRCDIR)/codec/rfc4175.cxx
endif

ifeq ($(OPAL_RFC2435), yes)
  SOURCES += $(OPAL_SRCDIR)/codec/rfc2435.cxx
endif

ifeq ($(OPAL_G711PLC), yes)
  SOURCES += $(OPAL_SRCDIR)/codec/g711a1_plc.cxx
endif

ifeq ($(OPAL_AEC), yes)
  SOURCES += $(OPAL_SRCDIR)/codec/echocancel.cxx
  ifeq ($(SPEEXDSP_SYSTEM), no)
    SPEEXDSP_SRCDIR    = $(OPAL_SRCDIR)/codec/speex/libspeex
    VPATH_C         += $(SPEEXDSP_SRCDIR)
    VPATH_CXX       += $(SPEEXDSP_SRCDIR)
    SOURCES         += $(SPEEXDSP_SRCDIR)/speex_preprocess.c \
                       $(SPEEXDSP_SRCDIR)/smallft.c \
                       $(SPEEXDSP_SRCDIR)/misc.c \
                       $(SPEEXDSP_SRCDIR)/mdf.c \
                       $(SPEEXDSP_SRCDIR)/math_approx.c \
                       $(SPEEXDSP_SRCDIR)/kiss_fftr.c \
                       $(SPEEXDSP_SRCDIR)/kiss_fft.c \
                       $(SPEEXDSP_SRCDIR)/fftwrap.c
  endif # SPEEXDSP_SYSTEM
endif # OPAL_AEC


# Far end camera control
ifeq ($(OPAL_HAS_H224), yes)
  VPATH_CXX += $(OPAL_SRCDIR)/h224
  SOURCES += $(OPAL_SRCDIR)/h224/q922.cxx \
             $(OPAL_SRCDIR)/h224/h224.cxx \
             $(OPAL_SRCDIR)/h224/h281.cxx
endif


ifeq ($(OPAL_FAX), yes)
  SOURCES += $(OPAL_SRCDIR)/t38/t38proto.cxx \
             $(ASN_SRCDIR)/t38.cxx
endif


###############################################################################

CPPFLAGS += $(SHARED_CPPFLAGS) -DOPAL_PLUGIN_DIR="\"$(OPAL_PLUGIN_DIR)\""

include $(PTLIB_MAKE_DIR)/pre.mak
include $(PTLIB_MAKE_DIR)/post.mak


ifdef SRTP_LIB
  $(STATIC_LIB_FILE) : $(SRTP_LIB)

  $(SRTP_LIB) :
	$(MAKE) -C $(SRTP_DIR)
endif


###############################################################################

$(OPAL_SRCDIR)/opal/manager.cxx: $(REVISION_FILE)


ifneq (,$(SWIG))

  ifeq ($(OPAL_JAVA), yes)
    $(JAVA_WRAPPER): $(JAVA_SRCDIR)/opal.i $(OPAL_INCDIR)/opal.h
	$(SWIG) -java -package org.opalvoip -w451 -I$(OPAL_INCDIR) -o $@ $<
  endif

  ifeq ($(OPAL_RUBY), yes)
    $(RUBY_WRAPPER): $(RUBY_SRCDIR)/opal.i $(OPAL_INCDIR)/opal.h
	$(SWIG) -ruby -w451 -I$(OPAL_INCDIR) -o $@ $<
  endif

endif


################################################################################

ifeq ($(prefix),$(OPALDIR))

install uninstall:
	@echo install/uninstall not available as prefix=OPALDIR
	@false

else # OPALDIR

INCSUBDIRS:=asn codec ep h323 h460 h224 iax2 im lids opal rtp sdp sip t120 t38

install:
	for dir in $(DESTDIR)$(libdir) \
	           $(DESTDIR)$(libdir)/pkgconfig \
		   $(DESTDIR)$(includedir)/opal \
                   $(DESTDIR)$(datarootdir)/opal/make ; \
	do \
	    $(MKDIR_P) $$dir ; \
	    chmod 755 $$dir ; \
	done
	for lib in $(OPAL_OPT_SHARED_FILE) \
	           $(OPAL_DEBUG_SHARED_FILE) \
	           $(OPAL_OPT_STATIC_FILE) \
	           $(OPAL_DEBUG_STATIC_FILE) ; \
	do \
	   if test -e $$lib ; then \
	      $(INSTALL) -m 644 $$lib $(DESTDIR)$(libdir); \
	   fi \
	done
	cd $(DESTDIR)$(libdir) ; \
	$(LN_S) -f $(notdir $(OPAL_OPT_SHARED_FILE)) $(notdir $(OPAL_OPT_SHARED_LINK))
	if test -e $(OPAL_DEBUG_SHARED_FILE); then \
	   cd $(DESTDIR)$(libdir) ; \
	   $(LN_S) -f $(notdir $(OPAL_DEBUG_SHARED_FILE)) $(notdir $(OPAL_DEBUG_SHARED_LINK)) ; \
	fi
	$(INSTALL) -m 644 make/*.mak $(DESTDIR)$(datarootdir)/opal/make
	$(INSTALL) -m 644 include/*.h $(DESTDIR)$(includedir)/opal
	for dir in $(INCSUBDIRS); \
	do \
	   $(MKDIR_P) $(DESTDIR)$(includedir)/opal/$$dir ; \
	   chmod 755 $(DESTDIR)$(includedir)/opal/$$dir ; \
	   $(INSTALL) -m 644 include/$$dir/*.h $(DESTDIR)$(includedir)/opal/$$dir ; \
	done
	$(INSTALL) -m 644 opal.pc $(DESTDIR)$(libdir)/pkgconfig
ifeq ($(OPAL_PLUGINS),yes)
	$(Q_MAKE) -C plugins install
endif

uninstall:
	rm -rf $(foreach dir,$(INCSUBDIRS),$(DESTDIR)$(includedir)/$(dir)) \
               $(foreach file,$(wildcard include/*.h),$(DESTDIR)$(includedir)/$(notdir $(file))) \
               $(DESTDIR)$(datarootdir)/opal \
	       $(DESTDIR)$(libdir)/$(notdir $(OPAL_OPT_SHARED_LINK)) \
               $(DESTDIR)$(libdir)/$(notdir $(OPAL_OPT_SHARED_FILE)) \
               $(DESTDIR)$(libdir)/$(notdir $(OPAL_OPT_STATIC_FILE)) \
               $(DESTDIR)$(libdir)/$(notdir $(OPAL_DEBUG_SHARED_LINK)) \
               $(DESTDIR)$(libdir)/$(notdir $(OPAL_DEBUG_SHARED_FILE)) \
               $(DESTDIR)$(libdir)/$(notdir $(OPAL_DEBUG_STATIC_FILE)) \
	       $(DESTDIR)$(libdir)/pkgconfig/opal.pc
ifeq ($(OPAL_PLUGINS),yes)
	$(Q_MAKE) -C plugins uninstall
endif


endif # OPALDIR


# End of file #################################################################
