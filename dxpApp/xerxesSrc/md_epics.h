/*
 *  machine_dependant.h
 *
 *  Copyright 1996 X-ray Instrumentation Associates
 *  All rights reserved
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

#ifndef XIA_MDDEF_H
#include <xia_mddef.h>
#endif

/* Some constants */
#define MAXMOD 100

/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _DXP_PROTO_						/* ANSI C prototypes */
XIA_MD_STATIC int XIA_MD_API dxp_md_initialize(unsigned int *, char *);
XIA_MD_STATIC int XIA_MD_API dxp_md_open(char *, int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_open_bcn(int *, int *, int *, int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_io(int *,unsigned int *,unsigned int *,unsigned short *,unsigned int *);
#ifndef EXCLUDE_DXPX10P
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_initialize(unsigned int *, char *);
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_open(char *, int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_io(int *,unsigned int *,unsigned int *,unsigned short *,unsigned int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_lock_resource(int *ioChan, int *modChan, short *lock);
#endif
XIA_MD_STATIC int XIA_MD_API dxp_md_wait(float *);
XIA_MD_STATIC void XIA_MD_API dxp_md_error_control(char *,int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_get_maxblk(void);
XIA_MD_STATIC int XIA_MD_API dxp_md_set_maxblk(unsigned int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_puts(char *);

XIA_MD_IMPORT int XIA_MD_API xia_camxfr(short *camadr, short func, long nbytes, short mode, short *buf);
XIA_MD_IMPORT int XIA_MD_API xia_caminit(short *buf);

XIA_MD_IMPORT int XIA_MD_API DxpInitEPP(int );
XIA_MD_IMPORT int XIA_MD_API DxpWriteWord(unsigned short,unsigned short);
XIA_MD_IMPORT int XIA_MD_API DxpWriteBlock(unsigned short,unsigned short *,int);
XIA_MD_IMPORT int XIA_MD_API DxpWriteBlocklong(unsigned short,unsigned long *, int);
XIA_MD_IMPORT int XIA_MD_API DxpReadWord(unsigned short,unsigned short *);
XIA_MD_IMPORT int XIA_MD_API DxpReadBlock(unsigned short, unsigned short *,int);
XIA_MD_IMPORT int XIA_MD_API DxpReadBlockd(unsigned short, double *,int);
XIA_MD_IMPORT int XIA_MD_API DxpReadBlocklong(unsigned short,unsigned long *,int);
XIA_MD_IMPORT int XIA_MD_API DxpReadBlocklongd(unsigned short, double *,int);
XIA_MD_IMPORT void XIA_MD_API DxpSetID(unsigned short id);
XIA_MD_IMPORT int XIA_MD_API DxpWritePort(unsigned short port, unsigned short data);
XIA_MD_IMPORT int XIA_MD_API DxpReadPort(unsigned short port, unsigned short *data);


#else									/* old style prototypes */

XIA_MD_STATIC int XIA_MD_API dxp_md_initialize();
XIA_MD_STATIC int XIA_MD_API dxp_md_open();
XIA_MD_STATIC int XIA_MD_API dxp_md_open_bcn();
XIA_MD_STATIC int XIA_MD_API dxp_md_io();
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_initialize();
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_open();
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_io();
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
#endif
#ifdef __cplusplus
}
#endif

#endif					/* End if for DXP_MD_H */

