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
 * Description: URL manipulation subroutines header file. (mod_proxy)      *
 * Version:     $Revision: 500534 $                                        *
 ***************************************************************************/
#ifndef _JK_URL_H
#define _JK_URL_H

#include "jk_global.h"
#include "jk_pool.h"
#include "jk_util.h"

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

/*
 * Do a canonical encoding of the url x.
 * String y contains the result and is pre-allocated
 * for at least maxlen bytes, including a '\0' terminator.
 */
int jk_canonenc(const char *x, char *y, int maxlen);

/**
 * Unescapes a URL, leaving reserved characters intact.
 * @param escaped Optional buffer to write the encoded string, can be
 * NULL, in which case the URL decoding does not actually take place
 * but the result length of the decoded URL will be returned.
 * @param url String to be unescaped
 * @param slen The length of the original url, or -1 to decode until
 * a terminating '\0' is seen
 * @param forbid Optional list of forbidden characters, in addition to
 * 0x00
 * @param reserved Optional list of reserved characters that will be
 * left unescaped
 * @param plus If non zero, '+' is converted to ' ' as per
 * application/x-www-form-urlencoded encoding
 * @param len If set, the length of the escaped string will be returned
 * @return JK_TRUE on success, JK_FALSE if no characters are
 * decoded or the string is NULL, if a bad escape sequence is
 * found, or if a character on the forbid list is found.
 * Implementation copied from APR 1.5.x.
 */
int jk_unescape_url(char *const escaped,
                    const char *const url,
                    size_t slen,
                    const char *const forbid,
                    const char *const reserved,
                    const int plus,
                    size_t *len);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif  /* _JK_URL_H */
