#ifndef __DLLDEFS_H__
#define __DLLDEFS_H__

#define XIA_STATIC static

#define XIA_IMPORT __declspec( dllimport)
#define XIA_EXPORT  __declspec( dllexport)

#ifdef __STATIC__
#define XIA_EXPORT
#endif /* __STATIC__ */

#ifdef WIN32_EPPLIB_VBA
#define XIA_API _stdcall
#else

#ifdef WIN32_SERIAL_VBA
#define XIA_API _stdcall
#else

#ifdef WIN32_USBLIB_VBA
#define XIA_API _stdcall
#else
#define XIA_API
#endif /* WIN32_USBLIB_VBA */

#endif /* WIN32_SERIAL_VBA */

#endif /* WIN32_EPPLIB_VBA */



#endif
