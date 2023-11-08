#ifndef EL__OSDEP_WIN32_VT100_H
#define EL__OSDEP_WIN32_VT100_H

#ifdef CONFIG_OS_WIN32

#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

int VT100_decode(HANDLE, const void *, int);

#ifdef __cplusplus
}
#endif

#endif

#endif

