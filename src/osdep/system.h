#ifndef EL__OSDEP_SYSTEM_H
#define EL__OSDEP_SYSTEM_H

#if !defined(CONFIG_OS_BEOS) \
     && !defined(CONFIG_OS_OS2) \
     && !defined(CONFIG_OS_RISCOS) \
     && !defined(CONFIG_OS_UNIX) \
     && !defined(CONFIG_OS_WIN32)

#warning No OS platform defined, maybe config.h was not included

#endif

#include "osdep/beos/overrides.h"
#include "osdep/win32/overrides.h"

#include "osdep/beos/sysinfo.h"
#include "osdep/os2/sysinfo.h"
#include "osdep/riscos/sysinfo.h"
#include "osdep/unix/sysinfo.h"
#include "osdep/win32/sysinfo.h"

#ifndef HAVE_SA_STORAGE
#define sockaddr_storage sockaddr
#endif

#endif
