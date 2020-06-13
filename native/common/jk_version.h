/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/***************************************************************************
 * Description: JK version header file                                     *
 ***************************************************************************/

#ifndef __JK_VERSION_H
#define __JK_VERSION_H

#define JK_VERMAJOR     1
#define JK_VERMINOR     2
#define JK_VERMICRO     49

#if defined(JK_ISAPI)
#define JK_DISTNAME "isapi_redirector"
#define JK_DLL_SUFFIX "dll"
#elif defined(JK_NSAPI)
#define JK_DISTNAME "nsapi_redirector"
#define JK_DLL_SUFFIX "dll"
#else
#define JK_DISTNAME "mod_jk"
#define JK_DLL_SUFFIX "so"
#endif

/* Build JK_EXPOSED_VERSION and JK_VERSION */
#define JK_EXPOSED_VERSION JK_DISTNAME "/" JK_VERSTRING

#if !defined(JK_VERSUFIX)
#define JK_VERSUFIX ""
#endif

#define JK_FULL_EXPOSED_VERSION JK_EXPOSED_VERSION JK_VERSUFIX

#define JK_MAKEVERSION(major, minor, fix) \
            (((major) << 24) + ((minor) << 16) + ((fix) << 8))

#define JK_VERSION JK_MAKEVERSION(JK_VERMAJOR, JK_VERMINOR, JK_VERMICRO)

/** Properly quote a value as a string in the C preprocessor */
#define JK_STRINGIFY(n) JK_STRINGIFY_HELPER(n)
/** Helper macro for JK_STRINGIFY */
#define JK_STRINGIFY_HELPER(n) #n
#define JK_VERSTRING \
            JK_STRINGIFY(JK_VERMAJOR) "." \
            JK_STRINGIFY(JK_VERMINOR) "." \
            JK_STRINGIFY(JK_VERMICRO)

/* macro for Win32 .rc files using numeric csv representation */
#define JK_VERSIONCSV JK_VERMAJOR ##, \
                    ##JK_VERMINOR ##, \
                    ##JK_VERMICRO


#endif /* __JK_VERSION_H */
