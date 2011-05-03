
#ifndef EL__NETWORK_SSL_MATCH_HOSTNAME_H
#define EL__NETWORK_SSL_MATCH_HOSTNAME_H

int match_hostname_pattern(const unsigned char *hostname,
			   size_t hostname_length,
			   const unsigned char *pattern,
			   size_t pattern_length);

#endif
