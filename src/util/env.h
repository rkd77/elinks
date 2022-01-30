#ifndef EL__UTIL_ENV_H
#define EL__UTIL_ENV_H

#ifdef __cplusplus
extern "C" {
#endif

int env_set(const char *name, char *value, int len);

#ifdef __cplusplus
}
#endif

#endif
