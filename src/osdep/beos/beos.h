
#ifndef EL__OSDEP_BEOS_BEOS_H
#define EL__OSDEP_BEOS_BEOS_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_OS_BEOS

struct terminal;

void open_in_new_be(struct terminal *term, char *exe_name,
		    char *param);

#endif

#ifdef __cplusplus
}
#endif

#endif
