/* Signals handling. */

#ifndef EL__OSDEP_SIGNALS_H
#define EL__OSDEP_SIGNALS_H

#ifdef __cplusplus
extern "C" {
#endif

struct terminal;

#define NUM_SIGNALS	32

void install_signal_handler(int, void (*)(void *), void *, int);
void set_sigcld(void);
void sig_ctrl_c(struct terminal *term);
void clear_signal_mask_and_handlers(void);
void handle_basic_signals(struct terminal *term);
void unhandle_terminal_signals(struct terminal *term);
int check_signals(void);

#ifdef __cplusplus
}
#endif

#endif /* EL__LOWLEVEL_SIGNALS_H */
