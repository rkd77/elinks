
#ifndef EL__OSDEP_OS2_OS2_H
#define EL__OSDEP_OS2_OS2_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_OS_OS2

struct terminal;

void open_in_new_vio(struct terminal *term, unsigned char *exe_name,
		     unsigned char *param);
void open_in_new_fullscreen(struct terminal *term, unsigned char *exe_name,
			    unsigned char *param);

#endif

#ifdef __cplusplus
}
#endif

#endif
