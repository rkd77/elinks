/* declare corestrings */
#define CORESTRING_LWC_VALUE(NAME,VALUE)		\
	extern lwc_string *corestring_lwc_##NAME
#define CORESTRING_DOM_VALUE(NAME,VALUE)		\
	extern struct dom_string *corestring_dom_##NAME
//#define CORESTRING_NSURL(NAME,VALUE) \
//	extern struct nsurl *corestring_nsurl_##NAME
#include "document/libdom/corestringlist.h"
#undef CORESTRING_LWC_VALUE
#undef CORESTRING_DOM_VALUE
//#undef CORESTRING_NSURL

int corestrings_init(void);
int corestrings_fini(void);
