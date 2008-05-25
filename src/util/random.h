/** Random numbers.
 * @file */

#ifndef EL__UTIL_RANDOM_H
#define EL__UTIL_RANDOM_H

void seed_rand_once(void);

void random_nonce(unsigned char buf[], size_t size);

#endif
