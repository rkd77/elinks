/** Random numbers.
 * @file */

#ifndef EL__UTIL_RANDOM_H
#define EL__UTIL_RANDOM_H

#ifdef __cplusplus
extern "C" {
#endif

void seed_rand_once(void);

void random_nonce(unsigned char *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif
