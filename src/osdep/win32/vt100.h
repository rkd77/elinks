#ifndef EL__OSDEP_WIN32_VT100_H
#define EL__OSDEP_WIN32_VT100_H

#ifdef CONFIG_OS_WIN32

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

int VT100_decode(HANDLE, const void *, int);

#endif

#endif

