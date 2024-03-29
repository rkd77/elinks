#ifndef EL__UTIL_MD5_H
#define EL__UTIL_MD5_H

#define CONFIG_MD5 1

#ifdef __cplusplus
extern "C" {
#endif

/* GNU TLS doesn't define this */
#ifndef MD5_DIGEST_LENGTH
#define MD5_DIGEST_LENGTH 16
#endif

#define MD5_HEX_DIGEST_LENGTH (MD5_DIGEST_LENGTH * 2)

typedef unsigned char md5_digest_bin_T[MD5_DIGEST_LENGTH];
typedef unsigned char md5_digest_hex_T[MD5_HEX_DIGEST_LENGTH];

struct md5_context {
	uint32_t buf[4];
	uint32_t bits[2];
	unsigned char in[64];
};

/* The interface for digesting several chunks of data. To compute the message
 * digest of a chunk of bytes, declare an md5_context structure, pass it to
 * init_md5(), call update_md5() as needed on buffers full of bytes, and then
 * call done_md5(), which will fill a supplied 16-byte array with the digest. */
void init_md5(struct md5_context *context);
void update_md5(struct md5_context *context, const char *data, unsigned long length);
void done_md5(struct md5_context *context, md5_digest_bin_T digest);

/** Digest the passed @a data with the given length and stores the MD5
 * digest in the @a digest parameter. */
unsigned char *
digest_md5(const char *data, unsigned long length, md5_digest_bin_T digest);

#ifdef CONFIG_MD5
/** @name Provide compatibility with the OpenSSL interface:
 * @{ */

typedef struct md5_context MD5_CTX;
#define MD5_Init(context)		init_md5(context)
#define MD5_Update(context, data, len)	update_md5(context, data, len)
#define MD5_Final(md5, context)		done_md5(context, md5)
#define MD5(data, len, md5)		digest_md5(data, len, md5)

/** @} */
#endif /* CONFIG_MD5 */

#ifdef __cplusplus
}
#endif

#endif
