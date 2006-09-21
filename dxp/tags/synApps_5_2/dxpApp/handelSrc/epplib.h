/**
 * epplib.h
 *
 * Header file for the epplib.dll source.
 *
 * Modified to be ANSI C-compatible: 05/07/02 -- PJF
 *
 *
 */

#ifndef __EPPLIB_H__
#define __EPPLIB_H__



#include "dlldefs.h"

#ifdef __cplusplus
extern "C" {
#endif

  XIA_EXPORT int XIA_API DxpInitPortAddress(int );
  XIA_EXPORT int XIA_API DxpInitEPP(int );
  XIA_EXPORT int XIA_API DxpWriteWord(unsigned short,unsigned short);
  XIA_EXPORT int XIA_API DxpWriteBlock(unsigned short,unsigned short *,int);
  XIA_EXPORT int XIA_API DxpWriteBlocklong(unsigned short,unsigned long *, int);
  XIA_EXPORT int XIA_API DxpReadWord(unsigned short,unsigned short *);
  XIA_EXPORT int XIA_API DxpReadBlock(unsigned short, unsigned short *,int);
  XIA_EXPORT int XIA_API DxpReadBlockd(unsigned short, double *,int);
  XIA_EXPORT int XIA_API DxpReadBlocklong(unsigned short,unsigned long *,int);
  XIA_EXPORT int XIA_API DxpReadBlocklongd(unsigned short, double *,int);
  XIA_EXPORT void XIA_API DxpSetID(unsigned short id);
  XIA_EXPORT int XIA_API DxpWritePort(unsigned short port, unsigned short data);
  XIA_EXPORT int XIA_API DxpReadPort(unsigned short port, unsigned short *data);
  XIA_EXPORT int XIA_API set_addr(unsigned short Input_Data);

#ifdef __cplusplus
}
#endif


#endif /* __EPPLIB_H__ */
