
#ifndef EL__OSDEP_BEOS_BEOS_H
#define EL__OSDEP_BEOS_BEOS_H

#ifdef CONFIG_OS_BEOS

struct terminal;

void open_in_new_be(struct terminal *term, unsigned char *exe_name,
		    unsigned char *param);

#endif

#endif
