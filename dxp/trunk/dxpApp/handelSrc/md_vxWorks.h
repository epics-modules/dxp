/*
 *  machine_dependant.h
 *
 * Copyright (c) 2002, X-ray Instrumentation Associates
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, 
 * with or without modification, are permitted provided 
 * that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer.
 *   * Redistributions in binary form must reproduce the 
 *     above copyright notice, this list of conditions and the 
 *     following disclaimer in the documentation and/or other 
 *     materials provided with the distribution.
 *   * Neither the name of X-ray Instrumentation Associates 
 *     nor the names of its contributors may be used to endorse 
 *     or promote products derived from this software without 
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF 
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *  
 *  If host computer is not big-endian (e.g. VAX) be sure to define
 *    LITTLE_ENDIAN
 *
 *
 *  Modified 10/4/97: Added compiler directives for compatibility with traditional
 *  C;  Remove LITTLE_ENDIAN: now determined dynamically. Remove MAXBLK, replace with
 *  dxp_md_get_maxblk and dxp_md_set_maxblk -- allows dynamically setting..
 *
 *  Major mods 11/30/99:  JEW:  documented, added a couple routines.
 */


#ifndef DXP_MD_H
#define DXP_MD_H


#include "xia_mddef.h"
#include "xia_common.h"


/* Remove this when the uDXP is ready to go live */
/*#define EXCLUDE_UDXP    0xDEAD*/



/* Some constants */
#define MAXMOD 100

/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _DXP_PROTO_						/* ANSI C prototypes */
#if !(defined(EXCLUDE_DXP4C) && defined(EXCLUDE_DXP4C2X))
XIA_MD_STATIC int XIA_MD_API dxp_md_initialize(unsigned int *, char *);
XIA_MD_STATIC int XIA_MD_API dxp_md_open(char *, int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_open_bcn(int *, int *, int *, int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_io(int *,unsigned int *,unsigned long *,void  *,unsigned int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_close(int *camChan);
#endif /* EXCLUDE DXP4C */

#if !(defined(EXCLUDE_DXPX10P) && defined(EXCLUDE_DGF200))
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_initialize(unsigned int *, char *);
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_open(char *, int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_io(int *,unsigned int *,unsigned long *,void *,unsigned int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_close(int *camChan);
#endif /* EXCLUDE_DXPX10P */

#ifndef EXCLUDE_USB
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_initialize(unsigned int *maxMod, char *dllName);
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_open(char *ioname, int *camChan);
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_io(int *camChan, unsigned int *function, 
					   unsigned long *address, void *data,
					   unsigned int *length);
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_close(int *camChan);
#endif /* EXCLUDE_USB */

#ifndef EXCLUDE_UDXP
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_initialize(unsigned int *maxMod, char *dllName);
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_open(char *ioname, int *camChan);
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_io(int *camChan, unsigned int *function, 
					      unsigned long *address, void *data,
					      unsigned int *length);
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_close(int *camChan);
#endif /* EXCLUDE_UDXP */

XIA_MD_STATIC int XIA_MD_API dxp_md_lock_resource(int *ioChan, int *modChan, short *lock);
XIA_MD_STATIC int XIA_MD_API dxp_md_wait(float *);
XIA_MD_STATIC void XIA_MD_API dxp_md_error_control(char *,int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_get_maxblk(void);
XIA_MD_STATIC int XIA_MD_API dxp_md_set_maxblk(unsigned int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_puts(char *);
XIA_MD_STATIC int XIA_MD_API dxp_md_set_priority(int *priority);
XIA_MD_STATIC char * dxp_md_fgets(char *s, int length, FILE *stream);
XIA_MD_STATIC char * dxp_md_tmp_path(void);
XIA_MD_STATIC void dxp_md_clear_tmp(void);
XIA_MD_STATIC char * dxp_md_path_separator(void);

#ifdef USE_SERIAL_DLPORTIO
XIA_MD_STATIC void dxp_md_poll_and_read(unsigned short port, unsigned long nBytes, unsigned char *buf);
XIA_MD_STATIC void dxp_md_serial_write(unsigned short port, unsigned long nBytes, unsigned char *buf);
XIA_MD_STATIC void dxp_md_clear_port(unsigned short port);
#endif

XIA_MD_IMPORT int XIA_MD_API xia_camxfr(short *camadr, short func, long nbytes, short mode, short *buf);
XIA_MD_IMPORT int XIA_MD_API xia_caminit(short *buf);

#if !(defined(EXCLUDE_DXPX10P) && defined(EXCLUDE_DGF200))
XIA_MD_IMPORT int XIA_MD_API DxpInitPortAddress(int );
XIA_MD_STATIC int XIA_MD_API DxpInitEPP(int );
XIA_MD_STATIC int XIA_MD_API DxpWriteWord(unsigned short,unsigned short);
XIA_MD_STATIC int XIA_MD_API DxpWriteBlock(unsigned short,unsigned short *,int);
XIA_MD_STATIC int XIA_MD_API DxpWriteBlocklong(unsigned short,unsigned long *, int);
XIA_MD_STATIC int XIA_MD_API DxpReadWord(unsigned short,unsigned short *);
XIA_MD_STATIC int XIA_MD_API DxpReadBlock(unsigned short, unsigned short *,int);
XIA_MD_IMPORT int XIA_MD_API DxpReadBlockd(unsigned short, double *,int);
XIA_MD_STATIC int XIA_MD_API DxpReadBlocklong(unsigned short,unsigned long *,int);
XIA_MD_IMPORT int XIA_MD_API DxpReadBlocklongd(unsigned short, double *,int);
XIA_MD_STATIC void XIA_MD_API DxpSetID(unsigned short id);
XIA_MD_IMPORT int XIA_MD_API DxpWritePort(unsigned short port, unsigned short data);
XIA_MD_IMPORT int XIA_MD_API DxpReadPort(unsigned short port, unsigned short *data);
#endif /* EXCLUDE_DXPX10P */

#ifndef EXCLUDE_USB
XIA_MD_IMPORT int XIA_MD_API usb_open(char *device, HANDLE *hDevice);
XIA_MD_IMPORT int XIA_MD_API usb_close(HANDLE hDevice);
XIA_MD_IMPORT int XIA_MD_API usb_read(long address, long nWords, char *device, unsigned short *data);
XIA_MD_IMPORT int XIA_MD_API usb_write(long address, long nWords, char *device, unsigned short *data);
  
#endif /* EXCLUDE_USB */

#ifndef EXCLUDE_UDXP
XIA_MD_IMPORT int XIA_MD_API InitSerialPort(unsigned short port, unsigned long baud);
XIA_MD_IMPORT int XIA_MD_API CloseSerialPort(unsigned short port);
XIA_MD_IMPORT int XIA_MD_API ReadSerialPort(unsigned short port, unsigned long size, char *data);
XIA_MD_IMPORT int XIA_MD_API WriteSerialPort(unsigned short port, unsigned long size, char *data);
XIA_MD_IMPORT int XIA_MD_API CheckAndClearTransmitBuffer(unsigned short port);
XIA_MD_IMPORT int XIA_MD_API CheckAndClearReceiveBuffer(unsigned short port);
#ifndef USE_SERIAL_DLPORTIO
XIA_MD_IMPORT int XIA_MD_API NumBytesAtSerialPort(unsigned short port, unsigned long *numBytes);
#endif /* USE_SERIAL_DLPORTIO */

#ifdef USE_SERIAL_DLPORTIO
XIA_MD_IMPORT unsigned short XIA_MD_API IsByteAtPort(unsigned short port);
XIA_MD_IMPORT unsigned short XIA_MD_API IsClearForNextByte(unsigned short port);
XIA_MD_IMPORT void XIA_MD_API DumpRegistersToConsole(unsigned short port);
#endif /* USE_SERIAL_DLPORTIO */

#endif /* EXCLUDE_UDXP */



#else									/* old style prototypes */

XIA_MD_STATIC int XIA_MD_API dxp_md_initialize();
XIA_MD_STATIC int XIA_MD_API dxp_md_open();
XIA_MD_STATIC int XIA_MD_API dxp_md_open_bcn();
XIA_MD_STATIC int XIA_MD_API dxp_md_io();
XIA_MD_STATIC int XIA_MD_API dxp_md_close();

#if !(defined(EXCLUDE_DXPX10P) && defined(EXCLUDE_DGF200))
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_initialize();
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_open();
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_io();
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_close();
#endif /* EXCLUDE_DXPX10P */

#ifndef EXCLUDE_USB
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_initialize();
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_open();
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_io();
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_close();
#endif /* EXCLUDE_USB */

#ifndef EXCLUDE_UDXP
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_initialize();
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_open();
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_io();
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_close();
#endif /* EXCLUDE_UDXP */


XIA_MD_STATIC int XIA_MD_API dxp_md_lock_resource();
XIA_MD_STATIC int XIA_MD_API dxp_md_wait();
XIA_MD_STATIC void XIA_MD_API dxp_md_error_control();
XIA_MD_STATIC int XIA_MD_API dxp_md_get_maxblk();
XIA_MD_STATIC int XIA_MD_API dxp_md_set_maxblk();
XIA_MD_STATIC void XIA_MD_API *dxp_md_alloc();
XIA_MD_STATIC void XIA_MD_API dxp_md_free();
XIA_MD_STATIC int XIA_MD_API dxp_md_puts();

XIA_MD_IMPORT int XIA_MD_API xia_camxfr();
XIA_MD_IMPORT int XIA_MD_API xia_caminit();

#if !(defined(EXCLUDE_DXPX10P) && defined(EXCLUDE_DGF200))
XIA_MD_IMPORT int XIA_MD_API DxpInitPortAddress();
XIA_MD_IMPORT int XIA_MD_API DxpInitEPP();
XIA_MD_IMPORT int XIA_MD_API DxpWriteWord();
XIA_MD_IMPORT int XIA_MD_API DxpWriteBlock();
XIA_MD_IMPORT int XIA_MD_API DxpWriteBlocklong();
XIA_MD_IMPORT int XIA_MD_API DxpReadWord();
XIA_MD_IMPORT int XIA_MD_API DxpReadBlock();
XIA_MD_IMPORT int XIA_MD_API DxpReadBlockd();
XIA_MD_IMPORT int XIA_MD_API DxpReadBlocklong();
XIA_MD_IMPORT int XIA_MD_API DxpReadBlocklongd();
XIA_MD_IMPORT void XIA_MD_API DxpSetID();
XIA_MD_IMPORT int XIA_MD_API DxpWritePort();
XIA_MD_IMPORT int XIA_MD_API DxpReadPort();
#endif /* EXCLUDE_DXPX10P */

#ifndef EXCLUDE_USB
XIA_MD_IMPORT int XIA_MD_API usb_open();
XIA_MD_IMPORT int XIA_MD_API usb_close();
XIA_MD_IMPORT int XIA_MD_API usb_read();
XIA_MD_IMPORT int XIA_MD_API usb_write();
#endif /* EXCLUDE_USB */

#ifndef EXCLUDE_UDXP
XIA_MD_IMPORT int XIA_MD_API InitSerialPort();
XIA_MD_IMPORT int XIA_MD_API CloseSerialPort();
XIA_MD_IMPORT int XIA_MD_API NumBytesAtSerialPort();
XIA_MD_IMPORT int XIA_MD_API ReadSerialPort();
XIA_MD_IMPORT int XIA_MD_API WriteSerialPort();
#endif /* EXCLUDE_UDXP */


#endif
#ifdef __cplusplus
}
#endif


#endif					/* End if for DXP_MD_H */
