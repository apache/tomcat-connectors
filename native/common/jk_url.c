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
 * Description: URL manipulation subroutines. (ported from mod_proxy).     *
 ***************************************************************************/

#include "jk_global.h"
#include "jk_url.h"

static void jk_c2hex(int ch, char *x)
{
#if !CHARSET_EBCDIC
    int i;

    x[0] = '%';
    i = (ch & 0xF0) >> 4;
    if (i >= 10) {
        x[1] = ('A' - 10) + i;
    }
    else {
        x[1] = '0' + i;
    }

    i = ch & 0x0F;
    if (i >= 10) {
        x[2] = ('A' - 10) + i;
    }
    else {
        x[2] = '0' + i;
    }
#else /*CHARSET_EBCDIC*/
    static const char ntoa[] = { "0123456789ABCDEF" };
    char buf[1];

    ch &= 0xFF;

    buf[0] = ch;
    jk_xlate_to_ascii(buf, 1);

    x[0] = '%';
    x[1] = ntoa[(buf[0] >> 4) & 0x0F];
    x[2] = ntoa[buf[0] & 0x0F];
    x[3] = '\0';
#endif /*CHARSET_EBCDIC*/
}

/*
 * Convert a URL-encoded string to canonical form.
 * It encodes those which must be encoded, and does not touch
 * those which must not be touched.
 * String x must be '\0'-terminated.
 * String y must be pre-allocated with len maxlen
 * (including the terminating '\0').
 */
int jk_canonenc(const char *x, char *y, int maxlen)
{
    int i, j;
    int ch = x[0];
    char *allowed;  /* characters which should not be encoded */
    char *reserved; /* characters which much not be en/de-coded */

/*
 * N.B. in addition to :@&=, this allows ';' in an http path
 * and '?' in an ftp path -- this may be revised
 */
    allowed = "~$-_.+!*'(),;:@&=";
    reserved = "/";

    for (i = 0, j = 0; ch != '\0' && j < maxlen; i++, j++, ch=x[i]) {
/* always handle '/' first */
        if (strchr(reserved, ch)) {
            y[j] = ch;
            continue;
        }
/* recode it, if necessary */
        if (!jk_isalnum(ch) && !strchr(allowed, ch)) {
            if (j+2<maxlen) {
                jk_c2hex(ch, &y[j]);
                j += 2;
            }
            else {
                return JK_FALSE;
            }
        }
        else {
            y[j] = ch;
        }
    }
    if (j<maxlen) {
        y[j] = '\0';
        return JK_TRUE;
    }
    else {
        return JK_FALSE;
    }
}

#if USE_CHARSET_EBCDIC
static int convert_a2e[256] = {
  0x00, 0x01, 0x02, 0x03, 0x37, 0x2D, 0x2E, 0x2F, 0x16, 0x05, 0x15, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x10, 0x11, 0x12, 0x13, 0x3C, 0x3D, 0x32, 0x26, 0x18, 0x19, 0x3F, 0x27, 0x1C, 0x1D, 0x1E, 0x1F,
  0x40, 0x5A, 0x7F, 0x7B, 0x5B, 0x6C, 0x50, 0x7D, 0x4D, 0x5D, 0x5C, 0x4E, 0x6B, 0x60, 0x4B, 0x61,
  0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0x7A, 0x5E, 0x4C, 0x7E, 0x6E, 0x6F,
  0x7C, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6,
  0xD7, 0xD8, 0xD9, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xAD, 0xE0, 0xBD, 0x5F, 0x6D,
  0x79, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
  0x97, 0x98, 0x99, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xC0, 0x4F, 0xD0, 0xA1, 0x07,
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x06, 0x17, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x09, 0x0A, 0x1B,
  0x30, 0x31, 0x1A, 0x33, 0x34, 0x35, 0x36, 0x08, 0x38, 0x39, 0x3A, 0x3B, 0x04, 0x14, 0x3E, 0xFF,
  0x41, 0xAA, 0x4A, 0xB1, 0x9F, 0xB2, 0x6A, 0xB5, 0xBB, 0xB4, 0x9A, 0x8A, 0xB0, 0xCA, 0xAF, 0xBC,
  0x90, 0x8F, 0xEA, 0xFA, 0xBE, 0xA0, 0xB6, 0xB3, 0x9D, 0xDA, 0x9B, 0x8B, 0xB7, 0xB8, 0xB9, 0xAB,
  0x64, 0x65, 0x62, 0x66, 0x63, 0x67, 0x9E, 0x68, 0x74, 0x71, 0x72, 0x73, 0x78, 0x75, 0x76, 0x77,
  0xAC, 0x69, 0xED, 0xEE, 0xEB, 0xEF, 0xEC, 0xBF, 0x80, 0xFD, 0xFE, 0xFB, 0xFC, 0xBA, 0xAE, 0x59,
  0x44, 0x45, 0x42, 0x46, 0x43, 0x47, 0x9C, 0x48, 0x54, 0x51, 0x52, 0x53, 0x58, 0x55, 0x56, 0x57,
  0x8C, 0x49, 0xCD, 0xCE, 0xCB, 0xCF, 0xCC, 0xE1, 0x70, 0xDD, 0xDE, 0xDB, 0xDC, 0x8D, 0x8E, 0xDF };
#endif /*USE_CHARSET_EBCDIC*/

static char x2c(const char *what)
{
    register char digit;

#if !USE_CHARSET_EBCDIC
    digit =
            ((what[0] >= 'A') ? ((what[0] & 0xdf) - 'A') + 10 : (what[0] - '0'));
    digit *= 16;
    digit += (what[1] >= 'A' ? ((what[1] & 0xdf) - 'A') + 10 : (what[1] - '0'));
#else /*USE_CHARSET_EBCDIC*/
    char xstr[5];
    xstr[0]='0';
    xstr[1]='x';
    xstr[2]=what[0];
    xstr[3]=what[1];
    xstr[4]='\0';
    digit = convert_a2e[0xFF & strtol(xstr, NULL, 16)];
#endif /*USE_CHARSET_EBCDIC*/
    return (digit);
}

#define jk_isxdigit(c) (isxdigit(((unsigned char)(c))))

/**
 * Unescapes a URL, leaving reserved characters intact.
 * @param unescaped Optional buffer to write the encoded string, can be
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
 * @param len If set, the length of the unescaped string will be returned
 * @return JK_TRUE on success, JK_FALSE if the string is NULL,
 * if a bad escape sequence is found, or if a character on the
 * forbid list is found.
 * Implementation copied from APR 1.5.x.
 */
int jk_unescape_url(char *const unescaped,
                    const char *const url,
                    size_t slen,
                    const char *const forbid,
                    const char *const reserved,
                    const int plus,
                    size_t *len)
{
    size_t size = 1;
    int found = 0;
    const char *s = (const char *) url;
    char *d = (char *) unescaped;
    register int badesc, badpath;

    if (!url) {
        return JK_FALSE;
    }

    badesc = 0;
    badpath = 0;
    if (s) {
        if (d) {
            for (; *s && slen; ++s, d++, slen--) {
                if (plus && *s == '+') {
                    *d = ' ';
                    found = 1;
                }
                else if (*s != '%') {
                    *d = *s;
                }
                else {
                    if (!jk_isxdigit(*(s + 1)) || !jk_isxdigit(*(s + 2))) {
                        badesc = 1;
                        *d = '%';
                    }
                    else {
                        char decoded;
                        decoded = x2c(s + 1);
                        if ((decoded == '\0')
                                || (forbid && strchr(forbid, decoded))) {
                            badpath = 1;
                            *d = decoded;
                            s += 2;
                            slen -= 2;
                        }
                        else if (reserved && strchr(reserved, decoded)) {
                            *d++ = *s++;
                            *d++ = *s++;
                            *d = *s;
                            size += 2;
                        }
                        else {
                            *d = decoded;
                            s += 2;
                            slen -= 2;
                            found = 1;
                        }
                    }
                }
                size++;
            }
            *d = '\0';
        }
        else {
            for (; *s && slen; ++s, slen--) {
                if (plus && *s == '+') {
                    found = 1;
                }
                else if (*s != '%') {
                    /* character unchanged */
                }
                else {
                    if (!jk_isxdigit(*(s + 1)) || !jk_isxdigit(*(s + 2))) {
                        badesc = 1;
                    }
                    else {
                        char decoded;
                        decoded = x2c(s + 1);
                        if ((decoded == '\0')
                                || (forbid && strchr(forbid, decoded))) {
                            badpath = 1;
                            s += 2;
                            slen -= 2;
                        }
                        else if (reserved && strchr(reserved, decoded)) {
                            s += 2;
                            slen -= 2;
                            size += 2;
                        }
                        else {
                            s += 2;
                            slen -= 2;
                            found = 1;
                        }
                    }
                }
                size++;
            }
        }
    }

    if (len) {
        *len = size;
    }
    if (badesc) {
        return JK_FALSE;
    }
    else if (badpath) {
        return JK_FALSE;
    }
    else if (!found) {
        return JK_TRUE;
    }

    return JK_TRUE;
}
