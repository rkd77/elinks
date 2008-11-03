/** Random numbers.
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "elinks.h"

#include "util/random.h"

void
seed_rand_once(void)
{
	static int seeded = 0;

	if (!seeded) {
		srand(time(NULL));
		seeded = 1;
	}
}

#ifndef CONFIG_SSL

static void
pseudorandom_nonce(unsigned char buf[], size_t size)
{
	static int initialized = 0;
	static int accept_bits;
	static int accept_mask;
	unsigned int got_mask;
	unsigned int got_random;
	size_t index;
	
	if (!initialized) {
		unsigned int shift;

		seed_rand_once();

		/* 32767 <= RAND_MAX <= INT_MAX.  Find the largest
		 * accept_mask such that accept_mask <= RAND_MAX and
		 * accept_mask + 1 is a power of two.  */
		shift = RAND_MAX;
		accept_bits = 0U;
		accept_mask = 0U;
		while (shift != 0U) {
			shift >>= 1;
			accept_bits++;
			accept_mask = (accept_mask << 1) + 1U;
		}
		if (accept_mask > (unsigned int) RAND_MAX) {
			accept_bits--;
			accept_mask >>= 1;
		}

		initialized = 1;
	}
	
	got_mask = got_random = 0U;
	for (index = 0; index < size; ) {
		if (got_mask >= UCHAR_MAX) {
			buf[index++] = (unsigned char) got_random;
			got_mask >>= CHAR_BIT;
			got_random >>= CHAR_BIT;
		} else {
			unsigned int candidate;

			do {
				candidate = rand();
			} while (candidate > accept_mask);

			/* These shifts can discard some bits.  */
			got_mask = (got_mask << accept_bits) | accept_mask;
			got_random = (got_random << accept_bits) | candidate;
		}
	}
}

/** Fill a buffer with random bytes.  The bytes are not
 * cryptographically random enough to be used in a key, but they
 * should be good enough for a nonce or boundary string that may
 * be sent in cleartext.
 *
 * If CONFIG_SSL is defined, then this function is instead defined in
 * src/network/ssl/ssl.c, and it gets random numbers directly from the
 * selected SSL library.  */
void
random_nonce(unsigned char buf[], size_t size)
{
	size_t i = 0;
	FILE *f = fopen("/dev/urandom", "rb");

	if (!f) f = fopen("/dev/prandom", "rb"); /* OpenBSD */
	if (f) {
		i = fread(buf, 1, size, f);
		fclose(f);
	}

	/* If the random device did not exist or could not provide
	 * enough data, then fill the buffer with rand().  The
	 * resulting numbers may be predictable but they provide
	 * ELinks with at least some way to generate boundary strings
	 * for multipart uploads.  A more secure algorithm and entropy
	 * collection could be implemented, but there doesn't seem to
	 * be much point as SSL libraries already provide this
	 * facility.  */
	if (i < size)
		pseudorandom_nonce(buf + i, size - i);
}

#endif	/* ndef CONFIG_SSL */
