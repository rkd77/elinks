#ifndef EL__MAIN_VERSION_H
#define EL__MAIN_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

struct terminal;

char *get_dyn_full_version(struct terminal *term, int more);
void init_static_version(void);

#ifdef __cplusplus
}
#endif

#endif /* EL__MODULES_VERSION_H */
