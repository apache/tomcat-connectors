# Microsoft Developer Studio Generated NMAKE File, Based on mod_jk.dsp
!IF "$(CFG)" == ""
CFG=mod_jk - Win32 Release httpd 2.4
!MESSAGE No configuration specified. Defaulting to mod_jk - Win32 Release httpd 2.4
!ENDIF 

!IF "$(CFG)" != "mod_jk - Win32 Release httpd 2.2" && "$(CFG)" != "mod_jk - Win32 Debug httpd 2.2" && "$(CFG)" != "mod_jk - Win32 Release httpd 2.4" && "$(CFG)" != "mod_jk - Win32 Debug httpd 2.4"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mod_jk.mak" CFG="mod_jk - Win32 Release httpd 2.4"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mod_jk - Win32 Release httpd 2.2" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "mod_jk - Win32 Debug httpd 2.2" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "mod_jk - Win32 Release httpd 2.4" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "mod_jk - Win32 Debug httpd 2.4" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "mod_jk - Win32 Release httpd 2.2"

OUTDIR=.\Release22
INTDIR=.\Release22
# Begin Custom Macros
OutDir=.\Release22
# End Custom Macros

ALL : "$(OUTDIR)\mod_jk.so"


CLEAN :
	-@erase "$(INTDIR)\jk_ajp12_worker.obj"
	-@erase "$(INTDIR)\jk_ajp13.obj"
	-@erase "$(INTDIR)\jk_ajp13_worker.obj"
	-@erase "$(INTDIR)\jk_ajp14.obj"
	-@erase "$(INTDIR)\jk_ajp14_worker.obj"
	-@erase "$(INTDIR)\jk_ajp_common.obj"
	-@erase "$(INTDIR)\jk_connect.obj"
	-@erase "$(INTDIR)\jk_context.obj"
	-@erase "$(INTDIR)\jk_lb_worker.obj"
	-@erase "$(INTDIR)\jk_map.obj"
	-@erase "$(INTDIR)\jk_md5.obj"
	-@erase "$(INTDIR)\jk_msg_buff.obj"
	-@erase "$(INTDIR)\jk_pool.obj"
	-@erase "$(INTDIR)\jk_shm.obj"
	-@erase "$(INTDIR)\jk_sockbuf.obj"
	-@erase "$(INTDIR)\jk_status.obj"
	-@erase "$(INTDIR)\jk_uri_worker_map.obj"
	-@erase "$(INTDIR)\jk_url.obj"
	-@erase "$(INTDIR)\jk_util.obj"
	-@erase "$(INTDIR)\jk_worker.obj"
	-@erase "$(INTDIR)\mod_jk.obj"
	-@erase "$(INTDIR)\mod_jk.res"
	-@erase "$(INTDIR)\mod_jk_src.idb"
	-@erase "$(INTDIR)\mod_jk_src.pdb"
	-@erase "$(OUTDIR)\mod_jk.exp"
	-@erase "$(OUTDIR)\mod_jk.lib"
	-@erase "$(OUTDIR)\mod_jk.pdb"
	-@erase "$(OUTDIR)\mod_jk.so"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MD /W3 /Zi /O2 /Oy- /I "..\common" /I "$(APACHE2_HOME)\include" /I "$(APACHE2_HOME)\srclib\apr\include" /I "$(APACHE2_HOME)\srclib\apr-util\include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "HAVE_APR" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\mod_jk_src" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\mod_jk.res" /i "..\common" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\mod_jk.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=libhttpd.lib libapr.lib libaprutil.lib kernel32.lib user32.lib advapi32.lib ws2_32.lib mswsock.lib /nologo /base:"0x6A6B0000" /subsystem:windows /dll /incremental:no /pdb:"$(OUTDIR)\mod_jk.pdb" /debug /machine:I386 /out:"$(OUTDIR)\mod_jk.so" /implib:"$(OUTDIR)\mod_jk.lib" /libpath:"$(APACHE2_HOME)\Release" /libpath:"$(APACHE2_HOME)\srclib\apr\Release" /libpath:"$(APACHE2_HOME)\srclib\apr-util\Release" /libpath:"$(APACHE2_HOME)\lib" /opt:ref 
LINK32_OBJS= \
	"$(INTDIR)\jk_ajp12_worker.obj" \
	"$(INTDIR)\jk_ajp13.obj" \
	"$(INTDIR)\jk_ajp13_worker.obj" \
	"$(INTDIR)\jk_ajp14.obj" \
	"$(INTDIR)\jk_ajp14_worker.obj" \
	"$(INTDIR)\jk_ajp_common.obj" \
	"$(INTDIR)\jk_connect.obj" \
	"$(INTDIR)\jk_context.obj" \
	"$(INTDIR)\jk_lb_worker.obj" \
	"$(INTDIR)\jk_map.obj" \
	"$(INTDIR)\jk_md5.obj" \
	"$(INTDIR)\jk_msg_buff.obj" \
	"$(INTDIR)\jk_pool.obj" \
	"$(INTDIR)\jk_shm.obj" \
	"$(INTDIR)\jk_sockbuf.obj" \
	"$(INTDIR)\jk_status.obj" \
	"$(INTDIR)\jk_uri_worker_map.obj" \
	"$(INTDIR)\jk_url.obj" \
	"$(INTDIR)\jk_util.obj" \
	"$(INTDIR)\jk_worker.obj" \
	"$(INTDIR)\mod_jk.obj" \
	"$(INTDIR)\mod_jk.res"

"$(OUTDIR)\mod_jk.so" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "mod_jk - Win32 Debug httpd 2.2"

OUTDIR=.\Debug22
INTDIR=.\Debug22
# Begin Custom Macros
OutDir=.\Debug22
# End Custom Macros

ALL : "$(OUTDIR)\mod_jk.so"


CLEAN :
	-@erase "$(INTDIR)\jk_ajp12_worker.obj"
	-@erase "$(INTDIR)\jk_ajp13.obj"
	-@erase "$(INTDIR)\jk_ajp13_worker.obj"
	-@erase "$(INTDIR)\jk_ajp14.obj"
	-@erase "$(INTDIR)\jk_ajp14_worker.obj"
	-@erase "$(INTDIR)\jk_ajp_common.obj"
	-@erase "$(INTDIR)\jk_connect.obj"
	-@erase "$(INTDIR)\jk_context.obj"
	-@erase "$(INTDIR)\jk_lb_worker.obj"
	-@erase "$(INTDIR)\jk_map.obj"
	-@erase "$(INTDIR)\jk_md5.obj"
	-@erase "$(INTDIR)\jk_msg_buff.obj"
	-@erase "$(INTDIR)\jk_pool.obj"
	-@erase "$(INTDIR)\jk_shm.obj"
	-@erase "$(INTDIR)\jk_sockbuf.obj"
	-@erase "$(INTDIR)\jk_status.obj"
	-@erase "$(INTDIR)\jk_uri_worker_map.obj"
	-@erase "$(INTDIR)\jk_url.obj"
	-@erase "$(INTDIR)\jk_util.obj"
	-@erase "$(INTDIR)\jk_worker.obj"
	-@erase "$(INTDIR)\mod_jk.obj"
	-@erase "$(INTDIR)\mod_jk.res"
	-@erase "$(INTDIR)\mod_jk_src.idb"
	-@erase "$(INTDIR)\mod_jk_src.pdb"
	-@erase "$(OUTDIR)\mod_jk.exp"
	-@erase "$(OUTDIR)\mod_jk.lib"
	-@erase "$(OUTDIR)\mod_jk.pdb"
	-@erase "$(OUTDIR)\mod_jk.so"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MDd /W3 /GX /Zi /Od /I "..\common" /I "$(APACHE2_HOME)\include" /I "$(APACHE2_HOME)\srclib\apr\include" /I "$(APACHE2_HOME)\srclib\apr-util\include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "HAVE_APR" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\mod_jk_src" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\mod_jk.res" /i "..\common" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\mod_jk.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=libhttpd.lib libapr.lib libaprutil.lib kernel32.lib user32.lib advapi32.lib ws2_32.lib mswsock.lib /nologo /base:"0x6A6B0000" /subsystem:windows /dll /incremental:no /pdb:"$(OUTDIR)\mod_jk.pdb" /debug /machine:I386 /out:"$(OUTDIR)\mod_jk.so" /implib:"$(OUTDIR)\mod_jk.lib" /libpath:"$(APACHE2_HOME)/Debug" /libpath:"$(APACHE2_HOME)\srclib\apr\Debug" /libpath:"$(APACHE2_HOME)\srclib\apr-util\Debug" /libpath:"$(APACHE2_HOME)\lib" 
LINK32_OBJS= \
	"$(INTDIR)\jk_ajp12_worker.obj" \
	"$(INTDIR)\jk_ajp13.obj" \
	"$(INTDIR)\jk_ajp13_worker.obj" \
	"$(INTDIR)\jk_ajp14.obj" \
	"$(INTDIR)\jk_ajp14_worker.obj" \
	"$(INTDIR)\jk_ajp_common.obj" \
	"$(INTDIR)\jk_connect.obj" \
	"$(INTDIR)\jk_context.obj" \
	"$(INTDIR)\jk_lb_worker.obj" \
	"$(INTDIR)\jk_map.obj" \
	"$(INTDIR)\jk_md5.obj" \
	"$(INTDIR)\jk_msg_buff.obj" \
	"$(INTDIR)\jk_pool.obj" \
	"$(INTDIR)\jk_shm.obj" \
	"$(INTDIR)\jk_sockbuf.obj" \
	"$(INTDIR)\jk_status.obj" \
	"$(INTDIR)\jk_uri_worker_map.obj" \
	"$(INTDIR)\jk_url.obj" \
	"$(INTDIR)\jk_util.obj" \
	"$(INTDIR)\jk_worker.obj" \
	"$(INTDIR)\mod_jk.obj" \
	"$(INTDIR)\mod_jk.res"

"$(OUTDIR)\mod_jk.so" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "mod_jk - Win32 Release httpd 2.4"

OUTDIR=.\Release24
INTDIR=.\Release24
# Begin Custom Macros
OutDir=.\Release24
# End Custom Macros

ALL : "$(OUTDIR)\mod_jk.so"


CLEAN :
	-@erase "$(INTDIR)\jk_ajp12_worker.obj"
	-@erase "$(INTDIR)\jk_ajp13.obj"
	-@erase "$(INTDIR)\jk_ajp13_worker.obj"
	-@erase "$(INTDIR)\jk_ajp14.obj"
	-@erase "$(INTDIR)\jk_ajp14_worker.obj"
	-@erase "$(INTDIR)\jk_ajp_common.obj"
	-@erase "$(INTDIR)\jk_connect.obj"
	-@erase "$(INTDIR)\jk_context.obj"
	-@erase "$(INTDIR)\jk_lb_worker.obj"
	-@erase "$(INTDIR)\jk_map.obj"
	-@erase "$(INTDIR)\jk_md5.obj"
	-@erase "$(INTDIR)\jk_msg_buff.obj"
	-@erase "$(INTDIR)\jk_pool.obj"
	-@erase "$(INTDIR)\jk_shm.obj"
	-@erase "$(INTDIR)\jk_sockbuf.obj"
	-@erase "$(INTDIR)\jk_status.obj"
	-@erase "$(INTDIR)\jk_uri_worker_map.obj"
	-@erase "$(INTDIR)\jk_url.obj"
	-@erase "$(INTDIR)\jk_util.obj"
	-@erase "$(INTDIR)\jk_worker.obj"
	-@erase "$(INTDIR)\mod_jk.obj"
	-@erase "$(INTDIR)\mod_jk.res"
	-@erase "$(INTDIR)\mod_jk_src.idb"
	-@erase "$(INTDIR)\mod_jk_src.pdb"
	-@erase "$(OUTDIR)\mod_jk.exp"
	-@erase "$(OUTDIR)\mod_jk.lib"
	-@erase "$(OUTDIR)\mod_jk.pdb"
	-@erase "$(OUTDIR)\mod_jk.so"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MD /W3 /Zi /O2 /Oy- /I "..\common" /I "$(APACHE2_HOME)\include" /I "$(APACHE2_HOME)\srclib\apr\include" /I "$(APACHE2_HOME)\srclib\apr-util\include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "HAVE_APR" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\mod_jk_src" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\mod_jk.res" /i "..\common" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\mod_jk.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=libhttpd.lib libapr-1.lib libaprutil-1.lib kernel32.lib user32.lib advapi32.lib ws2_32.lib mswsock.lib /nologo /base:"0x6A6B0000" /subsystem:windows /dll /incremental:no /pdb:"$(OUTDIR)\mod_jk.pdb" /debug /machine:I386 /out:"$(OUTDIR)\mod_jk.so" /implib:"$(OUTDIR)\mod_jk.lib" /libpath:"$(APACHE2_HOME)\Release" /libpath:"$(APACHE2_HOME)\srclib\apr\Release" /libpath:"$(APACHE2_HOME)\srclib\apr-util\Release" /libpath:"$(APACHE2_HOME)\lib" /opt:ref 
LINK32_OBJS= \
	"$(INTDIR)\jk_ajp12_worker.obj" \
	"$(INTDIR)\jk_ajp13.obj" \
	"$(INTDIR)\jk_ajp13_worker.obj" \
	"$(INTDIR)\jk_ajp14.obj" \
	"$(INTDIR)\jk_ajp14_worker.obj" \
	"$(INTDIR)\jk_ajp_common.obj" \
	"$(INTDIR)\jk_connect.obj" \
	"$(INTDIR)\jk_context.obj" \
	"$(INTDIR)\jk_lb_worker.obj" \
	"$(INTDIR)\jk_map.obj" \
	"$(INTDIR)\jk_md5.obj" \
	"$(INTDIR)\jk_msg_buff.obj" \
	"$(INTDIR)\jk_pool.obj" \
	"$(INTDIR)\jk_shm.obj" \
	"$(INTDIR)\jk_sockbuf.obj" \
	"$(INTDIR)\jk_status.obj" \
	"$(INTDIR)\jk_uri_worker_map.obj" \
	"$(INTDIR)\jk_url.obj" \
	"$(INTDIR)\jk_util.obj" \
	"$(INTDIR)\jk_worker.obj" \
	"$(INTDIR)\mod_jk.obj" \
	"$(INTDIR)\mod_jk.res"

"$(OUTDIR)\mod_jk.so" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "mod_jk - Win32 Debug httpd 2.4"

OUTDIR=.\Debug24
INTDIR=.\Debug24
# Begin Custom Macros
OutDir=.\Debug24
# End Custom Macros

ALL : "$(OUTDIR)\mod_jk.so"


CLEAN :
	-@erase "$(INTDIR)\jk_ajp12_worker.obj"
	-@erase "$(INTDIR)\jk_ajp13.obj"
	-@erase "$(INTDIR)\jk_ajp13_worker.obj"
	-@erase "$(INTDIR)\jk_ajp14.obj"
	-@erase "$(INTDIR)\jk_ajp14_worker.obj"
	-@erase "$(INTDIR)\jk_ajp_common.obj"
	-@erase "$(INTDIR)\jk_connect.obj"
	-@erase "$(INTDIR)\jk_context.obj"
	-@erase "$(INTDIR)\jk_lb_worker.obj"
	-@erase "$(INTDIR)\jk_map.obj"
	-@erase "$(INTDIR)\jk_md5.obj"
	-@erase "$(INTDIR)\jk_msg_buff.obj"
	-@erase "$(INTDIR)\jk_pool.obj"
	-@erase "$(INTDIR)\jk_shm.obj"
	-@erase "$(INTDIR)\jk_sockbuf.obj"
	-@erase "$(INTDIR)\jk_status.obj"
	-@erase "$(INTDIR)\jk_uri_worker_map.obj"
	-@erase "$(INTDIR)\jk_url.obj"
	-@erase "$(INTDIR)\jk_util.obj"
	-@erase "$(INTDIR)\jk_worker.obj"
	-@erase "$(INTDIR)\mod_jk.obj"
	-@erase "$(INTDIR)\mod_jk.res"
	-@erase "$(INTDIR)\mod_jk_src.idb"
	-@erase "$(INTDIR)\mod_jk_src.pdb"
	-@erase "$(OUTDIR)\mod_jk.exp"
	-@erase "$(OUTDIR)\mod_jk.lib"
	-@erase "$(OUTDIR)\mod_jk.pdb"
	-@erase "$(OUTDIR)\mod_jk.so"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MDd /W3 /GX /Zi /Od /I "..\common" /I "$(APACHE2_HOME)\include" /I "$(APACHE2_HOME)\srclib\apr\include" /I "$(APACHE2_HOME)\srclib\apr-util\include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "HAVE_APR" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\mod_jk_src" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\mod_jk.res" /i "..\common" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\mod_jk.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=libhttpd.lib libapr-1.lib libaprutil-1.lib kernel32.lib user32.lib advapi32.lib ws2_32.lib mswsock.lib /nologo /base:"0x6A6B0000" /subsystem:windows /dll /incremental:no /pdb:"$(OUTDIR)\mod_jk.pdb" /debug /machine:I386 /out:"$(OUTDIR)\mod_jk.so" /implib:"$(OUTDIR)\mod_jk.lib" /libpath:"$(APACHE2_HOME)/Debug" /libpath:"$(APACHE2_HOME)\srclib\apr\Debug" /libpath:"$(APACHE2_HOME)\srclib\apr-util\Debug" /libpath:"$(APACHE2_HOME)\lib" 
LINK32_OBJS= \
	"$(INTDIR)\jk_ajp12_worker.obj" \
	"$(INTDIR)\jk_ajp13.obj" \
	"$(INTDIR)\jk_ajp13_worker.obj" \
	"$(INTDIR)\jk_ajp14.obj" \
	"$(INTDIR)\jk_ajp14_worker.obj" \
	"$(INTDIR)\jk_ajp_common.obj" \
	"$(INTDIR)\jk_connect.obj" \
	"$(INTDIR)\jk_context.obj" \
	"$(INTDIR)\jk_lb_worker.obj" \
	"$(INTDIR)\jk_map.obj" \
	"$(INTDIR)\jk_md5.obj" \
	"$(INTDIR)\jk_msg_buff.obj" \
	"$(INTDIR)\jk_pool.obj" \
	"$(INTDIR)\jk_shm.obj" \
	"$(INTDIR)\jk_sockbuf.obj" \
	"$(INTDIR)\jk_status.obj" \
	"$(INTDIR)\jk_uri_worker_map.obj" \
	"$(INTDIR)\jk_url.obj" \
	"$(INTDIR)\jk_util.obj" \
	"$(INTDIR)\jk_worker.obj" \
	"$(INTDIR)\mod_jk.obj" \
	"$(INTDIR)\mod_jk.res"

"$(OUTDIR)\mod_jk.so" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("mod_jk.dep")
!INCLUDE "mod_jk.dep"
!ELSE 
!MESSAGE Warning: cannot find "mod_jk.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "mod_jk - Win32 Release httpd 2.2" || "$(CFG)" == "mod_jk - Win32 Debug httpd 2.2" || "$(CFG)" == "mod_jk - Win32 Release httpd 2.4" || "$(CFG)" == "mod_jk - Win32 Debug httpd 2.4"
SOURCE=..\common\jk_ajp12_worker.c

"$(INTDIR)\jk_ajp12_worker.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\common\jk_ajp13.c

"$(INTDIR)\jk_ajp13.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\common\jk_ajp13_worker.c

"$(INTDIR)\jk_ajp13_worker.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\common\jk_ajp14.c

"$(INTDIR)\jk_ajp14.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\common\jk_ajp14_worker.c

"$(INTDIR)\jk_ajp14_worker.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\common\jk_ajp_common.c

"$(INTDIR)\jk_ajp_common.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\common\jk_connect.c

"$(INTDIR)\jk_connect.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\common\jk_context.c

"$(INTDIR)\jk_context.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\common\jk_lb_worker.c

"$(INTDIR)\jk_lb_worker.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\common\jk_map.c

"$(INTDIR)\jk_map.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\common\jk_md5.c

"$(INTDIR)\jk_md5.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\common\jk_msg_buff.c

"$(INTDIR)\jk_msg_buff.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\common\jk_pool.c

"$(INTDIR)\jk_pool.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\common\jk_shm.c

"$(INTDIR)\jk_shm.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\common\jk_sockbuf.c

"$(INTDIR)\jk_sockbuf.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\common\jk_status.c

"$(INTDIR)\jk_status.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\common\jk_uri_worker_map.c

"$(INTDIR)\jk_uri_worker_map.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\common\jk_url.c

"$(INTDIR)\jk_url.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\common\jk_util.c

"$(INTDIR)\jk_util.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\common\jk_worker.c

"$(INTDIR)\jk_worker.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\mod_jk.c

"$(INTDIR)\mod_jk.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=..\common\jk.rc

!IF  "$(CFG)" == "mod_jk - Win32 Release httpd 2.2"


"$(INTDIR)\mod_jk.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) /l 0x409 /fo"$(INTDIR)\mod_jk.res" /i "..\common" /i "\mod_jk\jk-1.2.x\native\common" /d "NDEBUG" $(SOURCE)


!ELSEIF  "$(CFG)" == "mod_jk - Win32 Debug httpd 2.2"


"$(INTDIR)\mod_jk.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) /l 0x409 /fo"$(INTDIR)\mod_jk.res" /i "..\common" /i "\mod_jk\jk-1.2.x\native\common" /d "_DEBUG" $(SOURCE)


!ELSEIF  "$(CFG)" == "mod_jk - Win32 Release httpd 2.4"


"$(INTDIR)\mod_jk.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) /l 0x409 /fo"$(INTDIR)\mod_jk.res" /i "..\common" /i "\mod_jk\jk-1.2.x\native\common" /d "NDEBUG" $(SOURCE)


!ELSEIF  "$(CFG)" == "mod_jk - Win32 Debug httpd 2.4"


"$(INTDIR)\mod_jk.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) /l 0x409 /fo"$(INTDIR)\mod_jk.res" /i "..\common" /i "\mod_jk\jk-1.2.x\native\common" /d "_DEBUG" $(SOURCE)


!ENDIF 


!ENDIF 

