#ifndef EL__PROTOCOL_GEMINI_DIALOG_H
#define EL__PROTOCOL_GEMINI_DIALOG_H

#ifdef __cplusplus
extern "C" {
#endif

struct session;
void do_gemini_query_dialog(struct session *ses, void *data);

#ifdef __cplusplus
}
#endif

#endif
