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
#define MAXMOD 20

/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _DXP_PROTO_						/* ANSI C prototypes */
XIA_MD_EXPORT int XIA_MD_API dxp_md_init_util(Xia_Util_Functions *funcs, char *type);
XIA_MD_EXPORT int XIA_MD_API dxp_md_init_io(Xia_Io_Functions *funcs, char *type);
XIA_MD_STATIC int XIA_MD_API dxp_md_initialize(unsigned int *, char *);
XIA_MD_STATIC int XIA_MD_API dxp_md_open(char *, int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_open_bcn(int *, int *, int *, int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_io(int *,unsigned int *,unsigned int *,unsigned short *,unsigned int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_initialize(unsigned int *, char *);
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_open(char *, int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_io(int *,unsigned int *,unsigned int *,unsigned short *,unsigned int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_wait(float *);
XIA_MD_STATIC void XIA_MD_API dxp_md_error_control(char *,int *);
XIA_MD_STATIC void XIA_MD_API dxp_md_error(char *,char *, int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_get_maxblk(void);
XIA_MD_STATIC int XIA_MD_API dxp_md_set_maxblk(unsigned int *);
XIA_MD_STATIC void XIA_MD_API *dxp_md_alloc(size_t);
XIA_MD_STATIC void XIA_MD_API dxp_md_free(void *);
XIA_MD_STATIC int XIA_MD_API dxp_md_puts(char *);

XIA_IMPORT int XIA_API DxpInitEPP(int );
XIA_IMPORT int XIA_API DxpWriteWord(unsigned short,unsigned short);
XIA_IMPORT int XIA_API DxpWriteBlock(unsigned short,unsigned short *,int);
XIA_IMPORT int XIA_API DxpWriteBlocklong(unsigned short,unsigned long *, int);
XIA_IMPORT int XIA_API DxpReadWord(unsigned short,unsigned short *);
XIA_IMPORT int XIA_API DxpReadBlock(unsigned short, unsigned short *,int);
XIA_IMPORT int XIA_API DxpReadBlockd(unsigned short, double *,int);
XIA_IMPORT int XIA_API DxpReadBlocklong(unsigned short,unsigned long *,int);
XIA_IMPORT int XIA_API DxpReadBlocklongd(unsigned short, double *,int);
XIA_IMPORT void XIA_API DxpSetID(unsigned short id);
#else									/* old style prototypes */
XIA_MD_EXPORT int XIA_MD_API dxp_md_init_util();
XIA_MD_EXPORT int XIA_MD_API dxp_md_init_io();
XIA_MD_STATIC int XIA_MD_API dxp_md_initialize();
XIA_MD_STATIC int XIA_MD_API dxp_md_open();
XIA_MD_STATIC int XIA_MD_API dxp_md_open_bcn();
XIA_MD_STATIC int XIA_MD_API dxp_md_io();
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_initialize();
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_open();
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_io();
XIA_MD_STATIC int XIA_MD_API dxp_md_wait();
XIA_MD_STATIC void XIA_MD_API dxp_md_error_control();
XIA_MD_STATIC void XIA_MD_API dxp_md_error();
XIA_MD_STATIC int XIA_MD_API dxp_md_get_maxblk();
XIA_MD_STATIC int XIA_MD_API dxp_md_set_maxblk();
XIA_MD_STATIC void XIA_MD_API *dxp_md_alloc();
XIA_MD_STATIC void XIA_MD_API dxp_md_free();
XIA_MD_STATIC int XIA_MD_API dxp_md_puts();

XIA_IMPORT int XIA_API DxpInitEPP();
XIA_IMPORT int XIA_API DxpWriteWord();
XIA_IMPORT int XIA_API DxpWriteBlock();
XIA_IMPORT int XIA_API DxpWriteBlocklong();
XIA_IMPORT int XIA_API DxpReadWord();
XIA_IMPORT int XIA_API DxpReadBlock();
XIA_IMPORT int XIA_API DxpReadBlockd();
XIA_IMPORT int XIA_API DxpReadBlocklong();
XIA_IMPORT int XIA_API DxpReadBlocklongd();
XIA_IMPORT void XIA_API DxpSetID();
#endif
#ifdef __cplusplus
}
#endif

#endif					/* End if for DXP_MD_H */