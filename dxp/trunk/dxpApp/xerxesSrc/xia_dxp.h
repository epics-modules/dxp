/*
 *  xia_dxp.h
 *
 *  Created 11/30/99 JEW: internal include file.  define here, what we
 *						don't want the user to see.
 *
 *  Copyright 1999 X-ray Instrumentation Associates
 *  All rights reserved
 *
 *
 *    Following are prototypes for dxp_primitive.c and dxp_midlevel.c routines
 */
#ifndef XIA_DXP_H
#define XIA_DXP_H

/* If we are using the DLL version of the library */
#ifndef XIA_DXP_DLL_DEFINED
#define XIA_DXP_DLL_DEFINED

#ifdef XIA_MAKE_DXP_DLL
#define XIA_PORT __declspec(dllexport)
#define XIA_API
#else

#ifdef XIA_USE_DXP_DLL
#define XIA_PORT __declspec(dllimport)
#define XIA_API
#else					/* Then we are using a static link library */
#define XIA_PORT
#define XIA_API
#endif					/* Endif for XIA_USE_DXP_DLL */

#endif					/* Endif for XIA_MAKE_DXP_DLL */

#endif					/* Endif for XIA_DXP_DLL_DEFINED */

#ifndef _DXP_SWITCH_
#define _DXP_SWITCH_

#ifdef __STDC__
#define _DXP_PROTO_
#endif                /* end of __STDC__    */

#ifdef _MSC_VER
#ifndef _DXP_PROTO_
#define _DXP_PROTO_
#endif
#endif                /* end of _MSC_VER    */

#ifdef _DXP_PROTO_
#define VOID void
#else
#define VOID
#endif               /* end of _DXP_PROTO_ */

#endif               /* end of _DXP_SWITCH_*/


/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _DXP_PROTO_							/* Begin ANSI C prototypes */
XIA_PORT int XIA_API dxp_write_tsar(int *, short *);
XIA_PORT int XIA_API dxp_write_csr(int *, short *);
XIA_PORT int XIA_API dxp_read_csr(int *, short *);
XIA_PORT int XIA_API dxp_write_data(int *, short *, int);
XIA_PORT int XIA_API dxp_read_data(int *, short *, int);
XIA_PORT int XIA_API dxp_write_dsp(int *, short *, int);
XIA_PORT int XIA_API dxp_write_fippi(int *, short *, int);
XIA_PORT int XIA_API dxp_read_word(int *,int *,int *,int *,short *);
XIA_PORT int XIA_API dxp_write_word(int *,int *,int *,int *,short *);
XIA_PORT int XIA_API dxp_read_block(int *,int *,int *,int *,int *,int *,short *);
XIA_PORT int XIA_API dxp_write_block(int *,int *,int *,int *, int *,int *,short *);
XIA_PORT int XIA_API dxp_disable_LAM(int *);
XIA_PORT int XIA_API dxp_enable_LAM(int *);
XIA_PORT int XIA_API dxp_clear_LAM(int *);
XIA_PORT int XIA_API dxp_read_CSR(int *,short *);
XIA_PORT int XIA_API dxp_download_fipconfig(int *,int *,int *);
XIA_PORT int XIA_API dxp_download_dspconfig(int *,int *);
XIA_PORT int XIA_API dxp_begin_run(int *,int *,int *);
XIA_PORT int XIA_API dxp_end_run(int *);
XIA_PORT int XIA_API dxp_loc(char *,int *);
XIA_PORT int XIA_API dxp_dspparam_dump(int *,int *);
XIA_PORT int XIA_API dxp_begin_calibrate(int *,int *);
static int XIA_API dxp_test_mem(int *,int *,int *,int *,int *,int *);
XIA_PORT int XIA_API dxp_test_spectrum_memory(int *,int *,int *);
XIA_PORT int XIA_API dxp_test_baseline_memory(int *,int *,int *);
XIA_PORT int XIA_API dxp_test_event_memory(int *,int *,int *);
XIA_PORT int XIA_API dxp_new_fipconfig(int *,int *,char *, int *);
XIA_PORT int XIA_API dxp_get_fipconfig(char *, int *);
XIA_PORT void XIA_API dxp_free_fipconfig(void);
XIA_PORT int XIA_API dxp_download_fippi_done(int *, int *, int *);
XIA_PORT int XIA_API dxp_get_dspconfig(void);
XIA_PORT int XIA_API dxp_load_dspfile(FILE *);
XIA_PORT int XIA_API dxp_new_dspconfig(char *);
XIA_PORT int XIA_API dxp_load_dspsymbol_table(FILE *);
XIA_PORT int XIA_API dxp_load_dspconfig(FILE *);
XIA_PORT void XIA_API dxp_free_dspconfig(void);
XIA_PORT int XIA_API dxp_decode_error(short [],int *,int *);
XIA_PORT int XIA_API dxp_clear_error(int *,int *);
XIA_PORT int XIA_API dxp_check_calibration(int *, short *);
XIA_PORT int XIA_API dxp_get_runstats(short [],int *,int *,int *,int *,int *,double *);
XIA_PORT void XIA_API dxp_swaplong(int *, long *);
XIA_PORT int XIA_API dxp_symbolname(int *,char *);
XIA_PORT int XIA_API dxp_nsymbols(void);
XIA_PORT int XIA_API dxp_modify_dspsymbol(int *, int *, char *, short *);
XIA_PORT int XIA_API dxp_write_dsp_param_addr(int *, int *, int *, short *);
XIA_PORT int XIA_API dxp_read_dspsymbol(int *, int *, char *, short *);
XIA_PORT int XIA_API dxp_read_dspparams(int *, int *, short *);
XIA_PORT int XIA_API dxp_write_dspparams(int *, int *, short *);
XIA_PORT int XIA_API dxp_read_spectrum(int *, int *, long *);
XIA_PORT int XIA_API dxp_read_baseline(int *, int *, short *);
XIA_PORT int XIA_API dxp_perform_gaincalc(float *,short *,short *,short *,short *);
XIA_PORT int XIA_API dxp_change_gains(int *, int *, int *, float *);
XIA_PORT int XIA_API dxp_setup_asc(int *, int *, int *, float *, float *, short *, 
								   float *, float *, float *);
XIA_PORT int XIA_API dxp_calibrate_asc();
XIA_PORT int XIA_API dxp_read_long(int *,int *,int *,int *,long *);
XIA_PORT int XIA_API dxp_write_long(int *,int *,int *,int *,long *);
XIA_PORT int XIA_API dxp_little_endian(void);
XIA_PORT int XIA_API dxp_init_fptable();
XIA_PORT int XIA_API dxp_get_filename(char *, char *);
XIA_PORT int XIA_API dxp_set_filename(char *, char *);
XIA_PORT FILE XIA_API *dxp_getfp(char *,char *);
/*
 * following are prototypes for dxp_midlevel.c routines
 */
static int XIA_API dxp_set_decimation(int *, int *);
static int XIA_API dxp_readout_run(int *,int *,short [],short [],long []);
#else									/* Begin old style C prototypes */
XIA_PORT int XIA_API dxp_write_tsar();
XIA_PORT int XIA_API dxp_write_csr();
XIA_PORT int XIA_API dxp_read_csr();
XIA_PORT int XIA_API dxp_write_data();
XIA_PORT int XIA_API dxp_read_data();
XIA_PORT int XIA_API dxp_write_dsp();
XIA_PORT int XIA_API dxp_write_fippi();
XIA_PORT int XIA_API dxp_read_word();
XIA_PORT int XIA_API dxp_write_word();
XIA_PORT int XIA_API dxp_read_block();
XIA_PORT int XIA_API dxp_write_block();
XIA_PORT int XIA_API dxp_disable_LAM();
XIA_PORT int XIA_API dxp_enable_LAM();
XIA_PORT int XIA_API dxp_clear_LAM();
XIA_PORT int XIA_API dxp_read_CSR();
XIA_PORT int XIA_API dxp_download_fipconfig();
XIA_PORT int XIA_API dxp_download_dspconfig();
XIA_PORT int XIA_API dxp_begin_run();
XIA_PORT int XIA_API dxp_end_run();
XIA_PORT int XIA_API dxp_loc();
XIA_PORT int XIA_API dxp_dspparam_dump();
XIA_PORT int XIA_API dxp_begin_calibrate();
static int XIA_API dxp_test_mem();
XIA_PORT int XIA_API dxp_test_spectrum_memory();
XIA_PORT int XIA_API dxp_test_baseline_memory();
XIA_PORT int XIA_API dxp_test_event_memory();
XIA_PORT int XIA_API dxp_new_fipconfig();
XIA_PORT int XIA_API dxp_get_fipconfig();
XIA_PORT void XIA_API dxp_free_fipconfig();
XIA_PORT int XIA_API dxp_download_fippi_done();
XIA_PORT int XIA_API dxp_get_dspconfig(;
XIA_PORT int XIA_API dxp_load_dspfile();
XIA_PORT int XIA_API dxp_new_dspconfig();
XIA_PORT int XIA_API dxp_load_dspsymbol_table();
XIA_PORT int XIA_API dxp_load_dspconfig();
XIA_PORT void XIA_API dxp_free_dspconfig();
XIA_PORT int XIA_API dxp_decode_error();
XIA_PORT int XIA_API dxp_clear_error();
XIA_PORT int XIA_API dxp_check_calibration();
XIA_PORT int XIA_API dxp_get_runstats();
XIA_PORT void XIA_API dxp_swaplong();
XIA_PORT int XIA_API dxp_symbolname();
XIA_PORT int XIA_API dxp_nsymbols();
XIA_PORT int XIA_API dxp_modify_dspsymbol();
XIA_PORT int XIA_API dxp_write_dsp_param_addr();
XIA_PORT int XIA_API dxp_read_dspsymbol();
XIA_PORT int XIA_API dxp_read_dspparams();
XIA_PORT int XIA_API dxp_write_dspparams();
XIA_PORT int XIA_API dxp_read_spectrum();
XIA_PORT int XIA_API dxp_read_baseline();
XIA_PORT int XIA_API dxp_perform_gaincalc();
XIA_PORT int XIA_API dxp_change_gains();
XIA_PORT int XIA_API dxp_setup_asc();
XIA_PORT int XIA_API dxp_calibrate_asc();
XIA_PORT int XIA_API dxp_read_long();
XIA_PORT int XIA_API dxp_write_long();
XIA_PORT int XIA_API dxp_little_endian();
XIA_PORT int XIA_API dxp_get_filename();
XIA_PORT int XIA_API dxp_set_filename();
XIA_PORT FILE XIA_API *dxp_getfp();
/*
 * following are prototypes for dxp_midlevel.c routines
 */
static int XIA_API dxp_set_decimation();
static int XIA_API dxp_readout_run();
#endif                                  /*   end if _DXP_PROTO_ */

#ifdef __cplusplus
}
#endif

#endif						/* Endif for XIA_DXP_H */