
#ifndef EL__NETWORK_SSL_MATCH_HOSTNAME_H
#define EL__NETWORK_SSL_MATCH_HOSTNAME_H

#ifdef __cplusplus
extern "C" {
#endif

int match_hostname_pattern(const char *hostname,
			   size_t hostname_length,
			   const char *pattern,
			   size_t pattern_length);

#ifdef __cplusplus
}
#endif

#endif
