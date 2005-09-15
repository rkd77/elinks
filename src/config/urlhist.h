/* $Id: urlhist.h,v 1.8 2004/07/16 18:46:57 jonas Exp $ */

#ifndef EL__CONFIG_URLHIST_H
#define EL__CONFIG_URLHIST_H

#include "bfu/inphist.h"

extern struct input_history goto_url_history;

void init_url_history(void);
void done_url_history(void);

#endif
