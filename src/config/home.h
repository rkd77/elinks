#ifndef EL__CONFIG_HOME_H
#define EL__CONFIG_HOME_H

#ifdef __cplusplus
extern "C" {
#endif

extern char *elinks_home;
extern int first_use;

void init_home(void);
void done_home(void);

#ifdef __cplusplus
}
#endif

#endif
