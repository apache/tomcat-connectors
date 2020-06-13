# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
PROJECT = pcre
# Tools
CC = cl.exe
LINK = link.exe
RC = rc.exe
MT = mt.exe

!IF !DEFINED(BUILD_CPU) || "$(BUILD_CPU)" == ""
!ERROR Must specify BUILD_CPU matching compiler x86 or x64
!ENDIF
!IF "$(BUILD_CPU)" == "z86"
CPUFLAGS = -D_X86_=1
!ELSE
CPUFLAGS = -DWIN64 -D_WIN64
!ENDIF

BUILDLIB = $(WORKDIR)\$(PROJECT).lib
WORKDIR = $(BUILD_CPU)_RELEASE
CLEANTARGET = rd /s /q $(WORKDIR)
MAKEWORKDIR = md $(WORKDIR)
NMAKE_WINVER=0x0700
CFLAGS = $(CFLAGS) -DNDEBUG -DWIN32 -D_WINNT -DWINNT -D_WIN32_WINNT=$(NMAKE_WINVER) -DWINVER=$(NMAKE_WINVER)
CFLAGS = $(CFLAGS) -DPCRE_STATIC -DHAVE_CONFIG_H $(CPUFLAGS)
CLOPTS = /c /nologo -MD -W3 -O2 -Ob2 -Zi -EHsc
PDBFLAGS = -Fo$(WORKDIR)\ -Fd$(WORKDIR)\$(PROJECT)
LFLAGS = -lib /nologo

OBJECTS = \
	$(WORKDIR)\pcre_byte_order.obj \
	$(WORKDIR)\pcre_chartables.obj \
	$(WORKDIR)\pcre_compile.obj \
	$(WORKDIR)\pcre_config.obj \
	$(WORKDIR)\pcre_dfa_exec.obj \
	$(WORKDIR)\pcre_exec.obj \
	$(WORKDIR)\pcre_fullinfo.obj \
	$(WORKDIR)\pcre_get.obj \
	$(WORKDIR)\pcre_globals.obj \
	$(WORKDIR)\pcre_jit_compile.obj \
	$(WORKDIR)\pcre_maketables.obj \
	$(WORKDIR)\pcre_newline.obj \
	$(WORKDIR)\pcre_ord2utf8.obj \
	$(WORKDIR)\pcre_refcount.obj \
	$(WORKDIR)\pcre_string_utils.obj \
	$(WORKDIR)\pcre_study.obj \
	$(WORKDIR)\pcre_tables.obj \
	$(WORKDIR)\pcre_ucd.obj \
	$(WORKDIR)\pcre_valid_utf8.obj \
	$(WORKDIR)\pcre_version.obj \
	$(WORKDIR)\pcre_xclass.obj



all : $(WORKDIR) $(BUILDLIB)

$(WORKDIR) :
	@$(MAKEWORKDIR)

.c{$(WORKDIR)}.obj:
	$(CC) $(CLOPTS) $(CFLAGS) $(PDBFLAGS) $<

$(BUILDLIB): $(WORKDIR) $(OBJECTS)
	$(LINK) $(LFLAGS) $(OBJECTS) /out:$(BUILDLIB)

clean:
	@-$(CLEANTARGET) 2>NUL
