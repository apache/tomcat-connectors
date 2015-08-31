# Microsoft Developer Studio Project File - Name="pcre" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=pcre - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pcre.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pcre.mak" CFG="pcre - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pcre - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "pcre - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pcre - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "LibR"
# PROP Intermediate_Dir "LibR"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /O2 /D "_WIN32" /D "NDEBUG" /D "_WINDOWS" /D "HAVE_CONFIG_H" /FD /c
# ADD CPP /nologo /MD /W3 /Zi /O2 /D "_WIN32" /D "NDEBUG" /D "_WINDOWS" /D "HAVE_CONFIG_H" /Fd"LibR/pcre_src" /FD /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "pcre - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "LibD"
# PROP Intermediate_Dir "LibD"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /EHsc /Zi /Od /D "_WIN32" /D "_DEBUG" /D "_WINDOWS" /D "HAVE_CONFIG_H" /FD /c
# ADD CPP /nologo /MDd /W3 /EHsc /Zi /Od /D "_WIN32" /D "_DEBUG" /D "_WINDOWS" /D "HAVE_CONFIG_H" /Fd"LibD/pcre_src" /FD /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "pcre - Win32 Release"
# Name "pcre - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "*.c"

# Begin Source File

SOURCE=.\pcre_byte_order.c
# End Source File

# Begin Source File

SOURCE=.\pcre_compile.c
# End Source File

# Begin Source File

SOURCE=.\pcre_config.c
# End Source File

# Begin Source File

SOURCE=.\pcre_dfa_exec.c
# End Source File

# Begin Source File

SOURCE=.\pcre_exec.c
# End Source File

# Begin Source File

SOURCE=.\pcre_fullinfo.c
# End Source File

# Begin Source File

SOURCE=.\pcre_get.c
# End Source File

# Begin Source File

SOURCE=.\pcre_globals.c
# End Source File

# Begin Source File

SOURCE=.\pcre_jit_compile.c
# End Source File

# Begin Source File

SOURCE=.\pcre_maketables.c
# End Source File

# Begin Source File

SOURCE=.\pcre_newline.c
# End Source File

# Begin Source File

SOURCE=.\pcre_ord2utf8.c
# End Source File

# Begin Source File

SOURCE=.\pcre_refcount.c
# End Source File

# Begin Source File

SOURCE=.\pcre_string_utils.c
# End Source File

# Begin Source File

SOURCE=.\pcre_study.c
# End Source File

# Begin Source File

SOURCE=.\pcre_tables.c
# End Source File

# Begin Source File

SOURCE=.\pcre_ucd.c
# End Source File

# Begin Source File

SOURCE=.\pcre_valid_utf8.c
# End Source File

# Begin Source File

SOURCE=.\pcre_version.c
# End Source File

# Begin Source File

SOURCE=.\pcre_xclass.c
# End Source File

# Begin Source File

SOURCE=.\pcre_chartables.c.dist

!IF  "$(CFG)" == "pcre - Win32 Release"

# Begin Custom Build - Creating pcre_chartables.c from pcre_chartables.c.dist
InputPath=.\pcre_chartables.c.dist

".\pcre_chartables.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\pcre_chartables.c.dist > .\pcre_chartables.c
	
# End Custom Build

!ELSEIF  "$(CFG)" == "pcre - Win32 Debug"

# Begin Custom Build - Creating pcre_chartables.c from pcre_chartables.c.dist
InputPath=.\pcre_chartables.c.dist

".\pcre_chartables.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\pcre_chartables.c.dist > .\pcre_chartables.c
	
# End Custom Build

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "*.h"
# Begin Source File

SOURCE=.\config.hw

!IF  "$(CFG)" == "pcre - Win32 Release"

# Begin Custom Build - Creating pcre config.h from config.hw 
InputPath=.\config.hw

".\config.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\config.hw > .\config.h
	
# End Custom Build

!ELSEIF  "$(CFG)" == "pcre - Win32 Debug"

# Begin Custom Build - Creating pcre config.h from config.hw 
InputPath=.\config.hw

".\config.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\config.hw > .\config.h
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\pcre_internal.h
# End Source File
# Begin Source File

SOURCE=.\pcre.h.generic

!IF  "$(CFG)" == "pcre - Win32 Release"

# Begin Custom Build - Creating pcre.h from pcre.h.generic 
InputPath=.\pcre.h.generic

".\pcre.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\pcre.h.generic > .\pcre.h
	
# End Custom Build

!ELSEIF  "$(CFG)" == "pcre - Win32 Debug"

# Begin Custom Build - Creating pcre.h from pcre.h.generic 
InputPath=.\pcre.h.generic

".\pcre.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\pcre.h.generic > .\pcre.h
	
# End Custom Build

!ENDIF 

# End Source File
# End Group
# End Target
# End Project
