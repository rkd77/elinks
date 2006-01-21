#ifndef EL__UTIL_SHA1_H
#define EL__UTIL_SHA1_H

/* The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is SHA 180-1 Header File
 *
 * The Initial Developer of the Original Code is Paul Kocher of
 * Cryptography Research.  Portions created by Paul Kocher are
 * Copyright (C) 1995-9 by Cryptography Research, Inc.  All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 *     Paul Kocher
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable
 * instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL. */

/* Optionally SHA1 support can depend on external implementation when linking
 * against a SSL library that supports it. */
#if defined(CONFIG_OWN_LIBC)
#define CONFIG_SHA1 1
#elif defined(CONFIG_OPENSSL)
#include <openssl/sha.h>
#else
#define CONFIG_SHA1 1
#endif

#ifndef SHA_DIGEST_LENGTH
#define SHA_DIGEST_LENGTH 20
#endif

#define SHA_HEX_DIGEST_LENGTH (SHA_DIGEST_LENGTH * 2)

typedef unsigned char sha1_digest_bin_T[SHA_DIGEST_LENGTH];
typedef unsigned char sha1_digest_hex_T[SHA_HEX_DIGEST_LENGTH];

struct sha1_context {
	unsigned int H[5];
	unsigned int W[80];
	int lenW;
	unsigned int sizeHi,sizeLo;
};

void init_sha1(struct sha1_context *context);
void update_sha1(struct sha1_context *context, const unsigned char *data, unsigned long length);
void done_sha1(struct sha1_context *context, sha1_digest_bin_T digest);

unsigned char *
digest_sha1(const unsigned char *data, unsigned long length, sha1_digest_bin_T digest);

#ifdef CONFIG_SHA1
/* Provide compatibility with the OpenSSL interface: */

typedef struct sha1_context SHA_CTX;
#define SHA1_Init(context)		init_sha1(context)
#define SHA1_Update(context, data, len)	update_sha1(context, data, len)
#define SHA1_Final(sha1, context)	done_sha1(context, sha1)
#define SHA1(data, len, sha1)		digest_sha1(data, len, sha1)

#endif /* CONFIG_SHA1 */

#endif
