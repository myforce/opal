# Microsoft Developer Studio Project File - Name="OPAL_lib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=OPAL_lib - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "opal_lib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "opal_lib.mak" CFG="OPAL_lib - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "OPAL_lib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "OPAL_lib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "OPAL_lib - Win32 No Trace" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 1
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "lib"
# PROP BASE Intermediate_Dir "lib\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\lib"
# PROP Intermediate_Dir "..\..\lib\Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MD /W4 /GX /Zd /O2 /Ob2 /I "$(OPENSSLDIR)/inc32" /D "NDEBUG" /D "PTRACING" /D P_SSL=0$(OPENSSLFLAG) /Yu"ptlib.h" /FD /c
# ADD BASE RSC /l 0xc09
# ADD RSC /l 0xc09
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\lib\opals.lib"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "lib"
# PROP BASE Intermediate_Dir "lib\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\lib"
# PROP Intermediate_Dir "..\..\lib\Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MDd /W4 /Gm /GX /ZI /Od /I "$(OPENSSLDIR)/inc32" /D "_DEBUG" /D "PTRACING" /D P_SSL=0$(OPENSSLFLAG) /FR /Yu"ptlib.h" /FD /c
# ADD BASE RSC /l 0xc09
# ADD RSC /l 0xc09
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\lib\opalsd.lib"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "lib"
# PROP BASE Intermediate_Dir "lib\NoTrace"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\lib"
# PROP Intermediate_Dir "..\..\lib\NoTrace"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W4 /GX /O1 /Ob2 /I "./include" /D "NDEBUG" /D "PTRACING" /Yu"ptlib.h" /FD /c
# ADD CPP /nologo /MD /W4 /GX /O1 /Ob2 /D "NDEBUG" /D "PASN_NOPRINTON" /D "PASN_LEANANDMEAN" /Yu"ptlib.h" /FD /c
# ADD BASE RSC /l 0xc09
# ADD RSC /l 0xc09
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\lib\opalsn.lib"

!ENDIF 

# Begin Target

# Name "OPAL_lib - Win32 Release"
# Name "OPAL_lib - Win32 Debug"
# Name "OPAL_lib - Win32 No Trace"
# Begin Group "Source Files"

# PROP Default_Filter ".cxx"
# Begin Group "Opal Sources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\opal\call.cxx
# End Source File
# Begin Source File

SOURCE=..\opal\connection.cxx
# End Source File
# Begin Source File

SOURCE=..\opal\endpoint.cxx
# End Source File
# Begin Source File

SOURCE=..\opal\guid.cxx
# End Source File
# Begin Source File

SOURCE=..\opal\manager.cxx
# End Source File
# Begin Source File

SOURCE=..\opal\mediafmt.cxx
# End Source File
# Begin Source File

SOURCE=..\opal\mediastrm.cxx
# End Source File
# Begin Source File

SOURCE=..\opal\patch.cxx
# End Source File
# Begin Source File

SOURCE=..\opal\pcss.cxx
# End Source File
# Begin Source File

SOURCE=..\opal\transcoders.cxx
# End Source File
# Begin Source File

SOURCE=..\opal\transports.cxx
# End Source File
# End Group
# Begin Group "H323 Sources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\h323\channels.cxx
# End Source File
# Begin Source File

SOURCE=..\h323\gkclient.cxx
# End Source File
# Begin Source File

SOURCE=..\h323\gkserver.cxx
# End Source File
# Begin Source File

SOURCE=..\h323\h225ras.cxx
# End Source File
# Begin Source File

SOURCE=..\h323\h235auth.cxx
# End Source File
# Begin Source File

SOURCE=..\h323\h235auth1.cxx
# End Source File
# Begin Source File

SOURCE=..\h323\h323.cxx
# End Source File
# Begin Source File

SOURCE=..\h323\h323caps.cxx
# End Source File
# Begin Source File

SOURCE=..\h323\h323ep.cxx
# End Source File
# Begin Source File

SOURCE=..\h323\h323neg.cxx
# End Source File
# Begin Source File

SOURCE=..\h323\h323pdu.cxx
# End Source File
# Begin Source File

SOURCE=..\h323\h323rtp.cxx
# End Source File
# Begin Source File

SOURCE=..\h323\h450pdu.cxx
# End Source File
# Begin Source File

SOURCE=..\h323\q931.cxx
# End Source File
# Begin Source File

SOURCE=..\h323\transaddr.cxx
# End Source File
# End Group
# Begin Group "Codec Sources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\codec\g711.c
# ADD CPP /W1
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\g711codec.cxx
# End Source File
# Begin Source File

SOURCE=..\codec\g726codec.cxx
# End Source File
# Begin Source File

SOURCE=..\codec\g729codec.cxx
# ADD CPP /I "$(VAG729DIR)\\" /D VOICE_AGE_G729A=0$(VAG729FLAG)
# End Source File
# Begin Source File

SOURCE=..\codec\gsmcodec.cxx
# End Source File
# Begin Source File

SOURCE=..\codec\h261codec.cxx
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10codec.cxx
# End Source File
# Begin Source File

SOURCE=..\codec\mscodecs.cxx
# End Source File
# Begin Source File

SOURCE=..\codec\opalwavfile.cxx
# End Source File
# Begin Source File

SOURCE=..\codec\rfc2833.cxx
# End Source File
# End Group
# Begin Group "LID Sources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\lids\ixjwin32.cxx
# End Source File
# Begin Source File

SOURCE=..\lids\lid.cxx
# End Source File
# Begin Source File

SOURCE=..\lids\lidep.cxx
# End Source File
# Begin Source File

SOURCE=..\lids\vblasterlid.cxx
# End Source File
# Begin Source File

SOURCE=..\lids\vpblid.cxx
# PROP Exclude_From_Build 1
# End Source File
# End Group
# Begin Group "RTP Sources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\rtp\jitter.cxx
# End Source File
# Begin Source File

SOURCE=..\rtp\rtp.cxx
# End Source File
# End Group
# Begin Group "T.120 Sources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\t120\h323t120.cxx
# End Source File
# Begin Source File

SOURCE=..\t120\t120proto.cxx
# End Source File
# Begin Source File

SOURCE=..\t120\x224.cxx
# End Source File
# End Group
# Begin Group "T.38 Sources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\t38\h323t38.cxx
# End Source File
# Begin Source File

SOURCE=..\t38\t38proto.cxx
# End Source File
# End Group
# Begin Group "SIP Sources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\sip\sdp.cxx
# End Source File
# Begin Source File

SOURCE=..\sip\sipcon.cxx
# End Source File
# Begin Source File

SOURCE=..\sip\sipep.cxx
# End Source File
# Begin Source File

SOURCE=..\sip\sippdu.cxx
# End Source File
# End Group
# Begin Source File

SOURCE=.\precompile.cxx
# ADD CPP /Yc"ptlib.h"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter ".h"
# Begin Group "Opal Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\opal\call.h
# End Source File
# Begin Source File

SOURCE=..\..\include\opal\connection.h
# End Source File
# Begin Source File

SOURCE=..\..\include\opal\endpoint.h
# End Source File
# Begin Source File

SOURCE=..\..\include\opal\guid.h
# End Source File
# Begin Source File

SOURCE=..\..\include\opal\manager.h
# End Source File
# Begin Source File

SOURCE=..\..\include\opal\mediafmt.h
# End Source File
# Begin Source File

SOURCE=..\..\include\opal\mediastrm.h
# End Source File
# Begin Source File

SOURCE=..\..\include\opal\patch.h
# End Source File
# Begin Source File

SOURCE=..\..\include\opal\pcss.h
# End Source File
# Begin Source File

SOURCE=..\..\include\opal\transcoders.h
# End Source File
# Begin Source File

SOURCE=..\..\include\opal\transports.h
# End Source File
# End Group
# Begin Group "H323 Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\h323\channels.h
# End Source File
# Begin Source File

SOURCE=..\..\include\h323\gkclient.h
# End Source File
# Begin Source File

SOURCE=..\..\include\h323\gkserver.h
# End Source File
# Begin Source File

SOURCE=..\..\include\h323\h225ras.h
# End Source File
# Begin Source File

SOURCE=..\..\include\h323\h235auth.h
# End Source File
# Begin Source File

SOURCE=..\..\include\h323\h323caps.h
# End Source File
# Begin Source File

SOURCE=..\..\include\h323\h323con.h
# End Source File
# Begin Source File

SOURCE=..\..\include\h323\h323ep.h
# End Source File
# Begin Source File

SOURCE=..\..\include\h323\h323neg.h
# End Source File
# Begin Source File

SOURCE=..\..\include\h323\h323pdu.h
# End Source File
# Begin Source File

SOURCE=..\..\include\h323\h323rtp.h
# End Source File
# Begin Source File

SOURCE=..\..\include\h323\h450pdu.h
# End Source File
# Begin Source File

SOURCE=..\..\include\h323\q931.h
# End Source File
# Begin Source File

SOURCE=..\..\include\h323\transaddr.h
# End Source File
# End Group
# Begin Group "Codec Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\codec\allcodecs.h
# End Source File
# Begin Source File

SOURCE=..\..\include\codec\g711codec.h
# End Source File
# Begin Source File

SOURCE=..\..\include\codec\g7231codec.h
# End Source File
# Begin Source File

SOURCE=..\..\include\codec\g726codec.h
# End Source File
# Begin Source File

SOURCE=..\..\include\codec\g729codec.h
# End Source File
# Begin Source File

SOURCE=..\..\include\codec\gsmcodec.h
# End Source File
# Begin Source File

SOURCE=..\..\include\codec\h261codec.h
# End Source File
# Begin Source File

SOURCE=..\..\include\codec\lpc10codec.h
# End Source File
# Begin Source File

SOURCE=..\..\include\codec\mscodecs.h
# End Source File
# Begin Source File

SOURCE=..\..\include\codec\opalwavfile.h
# End Source File
# Begin Source File

SOURCE=..\..\include\codec\rfc2833.h
# End Source File
# End Group
# Begin Group "LID Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\lids\ixjlid.h
# End Source File
# Begin Source File

SOURCE=..\..\include\lids\lid.h
# End Source File
# Begin Source File

SOURCE=..\..\include\lids\lidep.h
# End Source File
# Begin Source File

SOURCE=..\..\include\lids\vblasterlid.h
# End Source File
# Begin Source File

SOURCE=..\..\include\lids\vpblid.h
# End Source File
# End Group
# Begin Group "RTP Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\rtp\jitter.h
# End Source File
# Begin Source File

SOURCE=..\..\include\rtp\rtp.h
# End Source File
# End Group
# Begin Group "T120 Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\t120\h323t120.h
# End Source File
# Begin Source File

SOURCE=..\..\include\t120\t120proto.h
# End Source File
# Begin Source File

SOURCE=..\..\include\t120\x224.h
# End Source File
# End Group
# Begin Group "T.38 Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\t38\h323t38.h
# End Source File
# Begin Source File

SOURCE=..\..\include\t38\t38proto.h
# End Source File
# End Group
# Begin Group "SIP Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\sip\sdp.h
# End Source File
# Begin Source File

SOURCE=..\..\include\sip\sip.h
# End Source File
# Begin Source File

SOURCE=..\..\include\sip\sipcon.h
# End Source File
# Begin Source File

SOURCE=..\..\include\sip\sipep.h
# End Source File
# Begin Source File

SOURCE=..\..\include\sip\sippdu.h
# End Source File
# End Group
# End Group
# Begin Group "ASN Files"

# PROP Default_Filter ".asn"
# Begin Group "ASN Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\asn\gcc.h
# End Source File
# Begin Source File

SOURCE=..\..\include\asn\h225.h
# End Source File
# Begin Source File

SOURCE=..\..\include\asn\h235.h
# End Source File
# Begin Source File

SOURCE=..\..\include\asn\h245.h
# End Source File
# Begin Source File

SOURCE=..\..\include\asn\h4501.h
# End Source File
# Begin Source File

SOURCE=..\..\include\asn\h45010.h
# End Source File
# Begin Source File

SOURCE=..\..\include\asn\h45011.h
# End Source File
# Begin Source File

SOURCE=..\..\include\asn\h4502.h
# End Source File
# Begin Source File

SOURCE=..\..\include\asn\h4503.h
# End Source File
# Begin Source File

SOURCE=..\..\include\asn\h4504.h
# End Source File
# Begin Source File

SOURCE=..\..\include\asn\h4505.h
# End Source File
# Begin Source File

SOURCE=..\..\include\asn\h4506.h
# End Source File
# Begin Source File

SOURCE=..\..\include\asn\h4507.h
# End Source File
# Begin Source File

SOURCE=..\..\include\asn\h4508.h
# End Source File
# Begin Source File

SOURCE=..\..\include\asn\h4509.h
# End Source File
# Begin Source File

SOURCE=..\..\include\asn\ldap.h
# End Source File
# Begin Source File

SOURCE=..\..\include\asn\mcs.h
# End Source File
# Begin Source File

SOURCE=..\..\include\asn\t38.h
# End Source File
# Begin Source File

SOURCE=..\..\include\asn\x880.h
# End Source File
# End Group
# Begin Group "ASN Sources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\asn\gcc.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h225.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h235.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h245_1.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h245_2.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h4501.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h45010.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h45011.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h4502.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h4503.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h4504.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h4505.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h4506.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h4507.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h4508.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h4509.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\ldap.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\mcs.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\t38.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\x880.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /I "..\..\include\asn"

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /I "$(IntDir)" /I "..\..\include\asn"

!ENDIF 

# End Source File
# End Group
# Begin Source File

SOURCE=..\asn\gcc.asn

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling GCC ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\gcc.asn
InputName=gcc

BuildCmds= \
	asnparser -m GCC -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling GCC ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\gcc.asn
InputName=gcc

BuildCmds= \
	asnparser -m GCC -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling GCC ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\gcc.asn
InputName=gcc

BuildCmds= \
	asnparser -m GCC -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h225.asn

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# PROP Intermediate_Dir "..\..\include\asn"
USERDEP__H225_="$(IntDir)\h235.h"	
# Begin Custom Build - Compiling H225 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h225.asn
InputName=h225

BuildCmds= \
	asnparser -m H225 -r MULTIMEDIA-SYSTEM-CONTROL=H245 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# PROP Intermediate_Dir "..\..\include\asn"
USERDEP__H225_="$(IntDir)\h235.h"	
# Begin Custom Build - Compiling H225 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h225.asn
InputName=h225

BuildCmds= \
	asnparser -m H225 -r MULTIMEDIA-SYSTEM-CONTROL=H245 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# PROP Intermediate_Dir "..\..\include\asn"
USERDEP__H225_="$(IntDir)\h235.h"	
# Begin Custom Build - Compiling H225 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h225.asn
InputName=h225

BuildCmds= \
	asnparser -m H225 -r MULTIMEDIA-SYSTEM-CONTROL=H245 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h235.asn

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H235 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h235.asn
InputName=h235

BuildCmds= \
	asnparser -m H235 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H235 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h235.asn
InputName=h235

BuildCmds= \
	asnparser -m H235 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H235 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h235.asn
InputName=h235

BuildCmds= \
	asnparser -m H235 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h245.asn

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H245 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h245.asn
InputName=h245

BuildCmds= \
	asnparser -s2 -m H245 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName)_1.cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\$(InputName)_2.cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H245 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h245.asn
InputName=h245

BuildCmds= \
	asnparser -s2 -m H245 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName)_1.cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\$(InputName)_2.cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H245 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h245.asn
InputName=h245

BuildCmds= \
	asnparser -s2 -m H245 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName)_1.cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\$(InputName)_2.cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h4501.asn

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.1 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4501.asn
InputName=h4501

BuildCmds= \
	asnparser -m H4501 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.1 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4501.asn
InputName=h4501

BuildCmds= \
	asnparser -m H4501 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.1 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4501.asn
InputName=h4501

BuildCmds= \
	asnparser -m H4501 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h45010.asn

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.10 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h45010.asn
InputName=h45010

BuildCmds= \
	asnparser -m H45010 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.10 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h45010.asn
InputName=h45010

BuildCmds= \
	asnparser -m H45010 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.10 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h45010.asn
InputName=h45010

BuildCmds= \
	asnparser -m H45010 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h45011.asn

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.11 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h45011.asn
InputName=h45011

BuildCmds= \
	asnparser -m H45011 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.11 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h45011.asn
InputName=h45011

BuildCmds= \
	asnparser -m H45011 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.11 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h45011.asn
InputName=h45011

BuildCmds= \
	asnparser -m H45011 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h4502.asn

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.2 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4502.asn
InputName=h4502

BuildCmds= \
	asnparser -m H4502 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.2 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4502.asn
InputName=h4502

BuildCmds= \
	asnparser -m H4502 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.2 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4502.asn
InputName=h4502

BuildCmds= \
	asnparser -m H4502 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h4503.asn

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.3 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4503.asn
InputName=h4503

BuildCmds= \
	asnparser -m H4503 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.3 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4503.asn
InputName=h4503

BuildCmds= \
	asnparser -m H4503 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.3 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4503.asn
InputName=h4503

BuildCmds= \
	asnparser -m H4503 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h4504.asn

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.4 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4504.asn
InputName=h4504

BuildCmds= \
	asnparser -m H4504 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.4 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4504.asn
InputName=h4504

BuildCmds= \
	asnparser -m H4504 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.4 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4504.asn
InputName=h4504

BuildCmds= \
	asnparser -m H4504 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h4505.asn

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.5 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4505.asn
InputName=h4505

BuildCmds= \
	asnparser -m H4505 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.5 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4505.asn
InputName=h4505

BuildCmds= \
	asnparser -m H4505 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.5 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4505.asn
InputName=h4505

BuildCmds= \
	asnparser -m H4505 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h4506.asn

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.6 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4506.asn
InputName=h4506

BuildCmds= \
	asnparser -m H4506 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.6 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4506.asn
InputName=h4506

BuildCmds= \
	asnparser -m H4506 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.6 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4506.asn
InputName=h4506

BuildCmds= \
	asnparser -m H4506 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h4507.asn

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.7 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4507.asn
InputName=h4507

BuildCmds= \
	asnparser -m H4507 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.7 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4507.asn
InputName=h4507

BuildCmds= \
	asnparser -m H4507 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.7 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4507.asn
InputName=h4507

BuildCmds= \
	asnparser -m H4507 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h4508.asn

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.8 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4508.asn
InputName=h4508

BuildCmds= \
	asnparser -m H4508 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.8 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4508.asn
InputName=h4508

BuildCmds= \
	asnparser -m H4508 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.8 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4508.asn
InputName=h4508

BuildCmds= \
	asnparser -m H4508 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\h4509.asn

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.9 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4509.asn
InputName=h4509

BuildCmds= \
	asnparser -m H4509 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.9 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4509.asn
InputName=h4509

BuildCmds= \
	asnparser -m H4509 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling H.450.9 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\h4509.asn
InputName=h4509

BuildCmds= \
	asnparser -m H4509 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\ldap.asn

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling LDAP ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\ldap.asn
InputName=ldap

BuildCmds= \
	asnparser -m LDAP -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling LDAP ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\ldap.asn
InputName=ldap

BuildCmds= \
	asnparser -m LDAP -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling LDAP ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\ldap.asn
InputName=ldap

BuildCmds= \
	asnparser -m LDAP -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\mcs.asn

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling MCS ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\mcs.asn
InputName=mcs

BuildCmds= \
	asnparser -m MCS -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling MCS ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\mcs.asn
InputName=mcs

BuildCmds= \
	asnparser -m MCS -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling MCS ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\mcs.asn
InputName=mcs

BuildCmds= \
	asnparser -m MCS -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\t38.asn

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling T.38 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\t38.asn
InputName=t38

BuildCmds= \
	asnparser -m T38 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling T.38 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\t38.asn
InputName=t38

BuildCmds= \
	asnparser -m T38 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling T.38 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\t38.asn
InputName=t38

BuildCmds= \
	asnparser -m T38 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\asn\x880.asn

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling X.880 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\x880.asn
InputName=x880

BuildCmds= \
	asnparser -m X880 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling X.880 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\x880.asn
InputName=x880

BuildCmds= \
	asnparser -m X880 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# PROP Intermediate_Dir "..\..\include\asn"
# Begin Custom Build - Compiling X.880 ASN File
InputDir=\Work\opal\src\asn
IntDir=.\..\..\include\asn
InputPath=..\asn\x880.asn
InputName=x880

BuildCmds= \
	asnparser -m X880 -c $(InputPath) \
	move $(InputDir)\$(InputName).h $(IntDir)\$(InputName).h \
	

"$(InputDir)\$(InputName).cxx" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# End Group
# Begin Group "GSM Files"

# PROP Default_Filter ".c"
# Begin Source File

SOURCE=..\codec\gsm\src\add.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /w /W0 /O2 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /w /W0 /ZI /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD BASE CPP /W1 /O2 /I "src\gsm\inc" /D NeedFunctionPrototypes=1
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /w /W0 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /O<none> /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\codec\gsm\src\code.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /w /W0 /O2 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /w /W0 /ZI /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD BASE CPP /W1 /O2 /I "src\gsm\inc" /D NeedFunctionPrototypes=1
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /w /W0 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /O<none> /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\codec\gsm\src\decode.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /w /W0 /O2 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /w /W0 /ZI /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD BASE CPP /W1 /O2 /I "src\gsm\inc" /D NeedFunctionPrototypes=1
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /w /W0 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /O<none> /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\codec\gsm\src\gsm_create.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /w /W0 /O2 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /w /W0 /ZI /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD BASE CPP /W1 /O2 /I "src\gsm\inc" /D NeedFunctionPrototypes=1
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /w /W0 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /O<none> /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\codec\gsm\src\gsm_decode.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /w /W0 /O2 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /w /W0 /ZI /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD BASE CPP /W1 /O2 /I "src\gsm\inc" /D NeedFunctionPrototypes=1
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /w /W0 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /O<none> /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\codec\gsm\src\gsm_destroy.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /w /W0 /O2 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /w /W0 /ZI /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD BASE CPP /W1 /O2 /I "src\gsm\inc" /D NeedFunctionPrototypes=1
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /w /W0 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /O<none> /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\codec\gsm\src\gsm_encode.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /w /W0 /O2 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /w /W0 /ZI /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD BASE CPP /W1 /O2 /I "src\gsm\inc" /D NeedFunctionPrototypes=1
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /w /W0 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /O<none> /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\codec\gsm\src\gsm_option.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /w /W0 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /w /W0 /ZI /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /w /W0 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /O<none> /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\codec\gsm\src\long_term.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /w /W0 /O2 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /w /W0 /ZI /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD BASE CPP /W1 /O2 /I "src\gsm\inc" /D NeedFunctionPrototypes=1
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /w /W0 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /O<none> /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\codec\gsm\src\lpc.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /w /W0 /O2 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /w /W0 /ZI /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD BASE CPP /W1 /O2 /I "src\gsm\inc" /D NeedFunctionPrototypes=1
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /w /W0 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /O<none> /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\codec\gsm\src\preprocess.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /w /W0 /O2 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /w /W0 /ZI /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD BASE CPP /W1 /O2 /I "src\gsm\inc" /D NeedFunctionPrototypes=1
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /w /W0 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /O<none> /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\codec\gsm\src\rpe.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /w /W0 /O2 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /w /W0 /ZI /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD BASE CPP /W1 /O2 /I "src\gsm\inc" /D NeedFunctionPrototypes=1
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /w /W0 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /O<none> /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\codec\gsm\src\short_term.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /w /W0 /O2 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /w /W0 /ZI /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD BASE CPP /W1 /O2 /I "src\gsm\inc" /D NeedFunctionPrototypes=1
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /w /W0 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /O<none> /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\codec\gsm\src\table.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /w /W0 /O2 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /w /W0 /ZI /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD BASE CPP /W1 /O2 /I "src\gsm\inc" /D NeedFunctionPrototypes=1
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /w /W0 /I "..\codec\gsm\inc" /D NeedFunctionPrototypes=1 /D "WAV49"
# SUBTRACT CPP /O<none> /YX /Yc /Yu

!ENDIF 

# End Source File
# End Group
# Begin Group "VIC Files"

# PROP Default_Filter ""
# Begin Group "C Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\codec\vic\bv.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /w /W0 /D "WIN32"
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /w /W0 /D "WIN32"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /w /W0 /D "WIN32"
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\codec\vic\huffcode.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /w /W0 /D "WIN32"
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /w /W0 /D "WIN32"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /w /W0 /D "WIN32"
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# End Group
# Begin Group "CXX Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\codec\vic\dct.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /W1 /D "WIN32"
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /W1 /D "WIN32"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /W1 /D "WIN32"
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE="..\codec\vic\encoder-h261.cxx"
# ADD CPP /W1
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\vic\p64.cxx

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# ADD CPP /W1 /D "WIN32"
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /W1 /D "WIN32"
# SUBTRACT CPP /D "PTRACING" /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

# ADD CPP /W1 /D "WIN32"
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\codec\vic\p64encoder.cxx
# ADD CPP /W1
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\vic\transmitter.cxx
# ADD CPP /W1
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\vic\vid_coder.cxx
# ADD CPP /W1
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# End Group
# Begin Group "H Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=".\src\codec\vic\bsd-endian.h"
# End Source File
# Begin Source File

SOURCE=.\src\codec\vic\config.h
# End Source File
# Begin Source File

SOURCE=.\src\codec\vic\dct.h
# End Source File
# Begin Source File

SOURCE=".\src\codec\vic\encoder-h261.h"
# End Source File
# Begin Source File

SOURCE=.\src\codec\vic\grabber.h
# End Source File
# Begin Source File

SOURCE=".\src\codec\vic\p64-huff.h"
# End Source File
# Begin Source File

SOURCE=.\src\codec\vic\p64.h
# End Source File
# Begin Source File

SOURCE=.\src\codec\vic\p64encoder.h
# End Source File
# Begin Source File

SOURCE=.\src\codec\vic\transmitter.h
# End Source File
# Begin Source File

SOURCE=.\src\codec\vic\vid_coder.h
# End Source File
# End Group
# End Group
# Begin Group "LPC10 Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\codec\lpc10\src\analys.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\bsynz.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\chanwr.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\dcbias.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\decode_.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\deemp.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\difmag.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\dyptrk.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\encode_.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\energy.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\f2clib.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\ham84.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\hp100.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\invert.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\irc2pc.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\ivfilt.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\lpc10.h
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\lpcdec.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\lpcenc.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\lpcini.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\lpfilt.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\median.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\mload.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\onset.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\pitsyn.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\placea.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\placev.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\preemp.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\prepro.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\random.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\rcchk.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\synths.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\tbdm.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\voicin.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\codec\lpc10\src\vparms.c
# ADD CPP /W1 /I "../codec/lpc10"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# End Group
# Begin Group "G.726 Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\codec\g726\g726_16.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /W1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\codec\g726\g726_24.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /W1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\codec\g726\g726_32.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /W1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\codec\g726\g726_40.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /W1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\codec\g726\g72x.c

!IF  "$(CFG)" == "OPAL_lib - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 Debug"

# ADD CPP /W1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "OPAL_lib - Win32 No Trace"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\codec\g726\g72x.h
# End Source File
# Begin Source File

SOURCE=..\codec\g726\private.h
# End Source File
# End Group
# End Target
# End Project
