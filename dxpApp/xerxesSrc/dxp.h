/*
 *  dxp.h
 *
 *  Modified 2-Feb-97 EO: add prototype for dxp_primitive routines
 *      dxp_read_long and dxp_write_long; added various parameters
 *
 *  Copyright 1996 X-ray Instrumentation Associates
 *  All rights reserved
 *
 *
 *    Following are prototypes for dxp_primitive.c routines
 */
#ifndef DXP_H
#define DXP_H

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
#define _DXP_SWITCH_ 1

#ifdef __STDC__
#define _DXP_PROTO_  1
#endif                /* end of __STDC__    */

#ifdef _MSC_VER
#ifndef _DXP_PROTO_
#define _DXP_PROTO_  1
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

#ifdef _DXP_PROTO_
#include <stdio.h>
/*
 * following are prototypes for dxp_midlevel.c routines
 */
XIA_PORT int XIA_API dxp_initialize(void);
XIA_PORT int XIA_API dxp_assign_channel(void);
XIA_PORT int XIA_API dxp_user_setup(void);
XIA_PORT int XIA_API dxp_start_run(int *,int *);
XIA_PORT int XIA_API dxp_stop_run(void);
XIA_PORT int XIA_API dxp_readout_detector_run(int *,short [],short [],long []);
XIA_PORT int XIA_API dxp_write_spectra(int *,int *);
XIA_PORT int XIA_API dxp_dspconfig(void);
XIA_PORT int XIA_API dxp_fipconfig(int *);
XIA_PORT int XIA_API dxp_replace_fipconfig(int *, char *, int *);
XIA_PORT int XIA_API dxp_replace_dspconfig(int *, char *);
XIA_PORT int XIA_API dxp_put_dspparams(void);
XIA_PORT int XIA_API dxp_replace_dspparams(int *);
XIA_PORT int XIA_API dxp_dspparams(char *);
XIA_PORT int XIA_API dxp_set_dspsymbol(char *, short *);
XIA_PORT int XIA_API dxp_symbolname_list(char **, int *);
XIA_PORT int XIA_API dxp_num_symbols(int *);
XIA_PORT int XIA_API dxp_set_one_dspsymbol(int *,char *, short *);
XIA_PORT int XIA_API dxp_download_params(int *,int [],short []);
XIA_PORT int XIA_API dxp_download_one_params(int *,int *,int [],short []);
XIA_PORT int XIA_API dxp_check_decimation(int *,int *);
XIA_PORT int XIA_API dxp_calibrate(int *);
XIA_PORT int XIA_API dxp_gaincalc(float *,short *,short *,short *,short *);
XIA_PORT int XIA_API dxp_findpeak(long [],int *,float *,int *,int *);
XIA_PORT int XIA_API dxp_fitgauss0(long [],int *,int *,float *,float *);
XIA_PORT int XIA_API dxp_save_config(int *);
XIA_PORT int XIA_API dxp_restore_config(int *);
XIA_PORT int XIA_API dxp_save_dspparams(int *, int *);
XIA_PORT int XIA_API dxp_open_file(int *,char *,int *);
XIA_PORT int XIA_API dxp_close_file(int *);
XIA_PORT int XIA_API dxp_get_electronics(int *,int [],int []);
XIA_PORT int XIA_API dxp_get_detectors(int *,int []);
XIA_PORT int XIA_API dxp_modify_gains(float *);
XIA_PORT int XIA_API dxp_initialize_ASC(float *,float *);
XIA_PORT int XIA_API dxp_det_to_elec(int *,int *,int *);
XIA_PORT int XIA_API dxp_elec_to_det(int *,int *,int *);
XIA_PORT int XIA_API dxp_mem_dump(int *);
XIA_PORT int XIA_API dxp_replace_filesymbol(char *, char *);
XIA_PORT int XIA_API dxp_read_filesymbol(char *, char *);
XIA_PORT void XIA_API dxp_version(void);

#else									/* Begin old style C prototypes */
/*
 * following are prototypes for dxp_midlevel.c routines
 */
XIA_PORT int XIA_API dxp_initialize();
XIA_PORT int XIA_API dxp_assign_channel();
XIA_PORT int XIA_API dxp_user_setup();
XIA_PORT int XIA_API dxp_start_run();
XIA_PORT int XIA_API dxp_stop_run();
XIA_PORT int XIA_API dxp_readout_detector_run();
XIA_PORT int XIA_API dxp_write_spectra();
XIA_PORT int XIA_API dxp_dspconfig();
XIA_PORT int XIA_API dxp_fipconfig();
XIA_PORT int XIA_API dxp_replace_fipconfig();
XIA_PORT int XIA_API dxp_replace_dspconfig();
XIA_PORT int XIA_API dxp_put_dspparams();
XIA_PORT int XIA_API dxp_replace_dspparams();
XIA_PORT int XIA_API dxp_dspparams();
XIA_PORT int XIA_API dxp_set_dspsymbol();
XIA_PORT int XIA_API dxp_set_one_dspsymbol();
XIA_PORT int XIA_API dxp_symbolname_list();
XIA_PORT int XIA_API dxp_num_symbols();
XIA_PORT int XIA_API dxp_download_params();
XIA_PORT int XIA_API dxp_download_one_params();
XIA_PORT int XIA_API dxp_check_decimation();
XIA_PORT int XIA_API dxp_calibrate();
XIA_PORT int XIA_API dxp_gaincalc();
XIA_PORT int XIA_API dxp_findpeak();
XIA_PORT int XIA_API dxp_fitgauss0();
XIA_PORT int XIA_API dxp_save_config();
XIA_PORT int XIA_API dxp_restore_config();
XIA_PORT int XIA_API dxp_save_dspparams();
XIA_PORT int XIA_API dxp_open_file();
XIA_PORT int XIA_API dxp_close_file();
XIA_PORT int XIA_API dxp_get_electronics();
XIA_PORT int XIA_API dxp_get_detectors();
XIA_PORT int XIA_API dxp_modify_gains();
XIA_PORT int XIA_API dxp_initialize_ASC();
XIA_PORT int XIA_API dxp_det_to_elec();
XIA_PORT int XIA_API dxp_elec_to_det();
XIA_PORT int XIA_API dxp_mem_dump();
XIA_PORT int XIA_API dxp_replace_filesymbol();
XIA_PORT int XIA_API dxp_read_filesymbol();
XIA_PORT void XIA_API dxp_version();
#endif                                  /*   end if _DXP_PROTO_ */

/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
}
#endif

#define CODE_VERSION			     0
#define CODE_REVISION		  	     1
#define LATEST_BOARD_TYPE	"DXP-4C\0"
#define LATEST_BOARD_VERSION	 "C\0"
/*
 *    
 */
#define MAXDET				500
#define START_PARAMS     0x0000
#define START_SPECTRUM   0x0000
#define START_BASELINE   0x0200
#define START_EVENT      0x0400
#define MAXSYM              150
#define MAXSPEC            1024
#define MAXBASE             512
#define MAXEVENT           1024
#define MAXDSP_LEN       0x2000
#define MAXFIP_LEN   0x00020000
#define MAX_FIPPI			  5
#define LIVECLOCK_TICK_TIME 8.e-7f
/*
 *    Calibration control codes
 */
#define TRKDAC                1
#define RESET                 2
/*
 * ASC parameters:
 */
#define ADC_RANGE		 2000.0
#define ADC_BITS			 10
#define OFFDAC_FACTOR      24.0
#define FINEGAIN_FACTOR  426.67
#define GAIN3_MAX          35.1
#define GAIN3_MIN          9.38
#define GAIN3_FAC          8.82
#define GAIN2_MIN          2.89
#define GAIN2_FAC          2.49
#define GAIN1_MIN         0.903
#define GAIN1_FAC         0.820
#define GAIN0_MIN         0.246
/*
 *    CAMAC status Register control codes
 */
#define XMEM                  0
#define YMEM                  1
#define ALLCHAN              -1
#define INCRADD               0
#define CONSTADD              1
#define IGNOREGATE            1
#define USEGATE               0
#define CLEARMCA              0
#define UPDATEMCA             1
/*
 *     CAMAC status Register bit positions
 */
#define MASK_YMEM         0x0001
#define MASK_WRITE        0x0002
#define MASK_CAMXFER      0x0004
#define MASK_RUNENABLE    0x0008
#define MASK_RESETMCA     0x0010
#define MASK_CONSTADD     0x0020
#define MASK_ALLCHAN      0x0100
#define MASK_DSPRESET     0x0200
#define MASK_FIPRESET     0x0400
#define MASK_IGNOREGATE   0x0800
/*
 *  some error codes
 */
#define DXP_SUCCESS            0
/* I/O level error codes 1-100*/
#define DXP_MDOPEN             1
#define DXP_MDIO               2
/*  primitive level error codes (due to mdio failures) 101-200*/
#define DXP_WRITE_TSAR       101
#define DXP_WRITE_CSR        102
#define DXP_WRITE_WORD       103
#define DXP_READ_WORD        104
#define DXP_WRITE_BLOCK      105
#define DXP_READ_BLOCK       106
#define DXP_DISABLE_LAM      107
#define DXP_CLEAR_LAM        108
#define DXP_TEST_LAM         109
#define DXP_READ_CSR         110
#define DXP_WRITE_FIPPI      111
#define DXP_WRITE_DSP        112
#define DXP_WRITE_DATA       113
#define DXP_READ_DATA        114
#define DXP_ENABLE_LAM       115
/*  DSP/FIPPI level error codes 201-300  */
#define DXP_MEMERROR         201
#define DXP_DSPRUNERROR      202
#define DXP_FIPDOWNLOAD      203
#define DXP_DSPDOWNLOAD      204
#define DXP_INTERNAL_GAIN    205
#define DXP_RESET_WARNING    206
#define DXP_DETECTOR_GAIN    207
#define DXP_NOSYMBOL         208
#define DXP_SPECTRUM         209
#define DXP_DSPLOAD          210
#define DXP_DSPPARAMS        211
/*  configuration errors  301-400  */
#define DXP_BAD_PARAM        301
#define DXP_NODECIMATION     302
#define DXP_OPEN_FILE        303
#define DXP_NORUNTASKS       304
#define DXP_OUTPUT_UNDEFINED 305
#define DXP_INPUT_UNDEFINED  306
#define DXP_ARRAY_TOO_SMALL  307
#define DXP_NOCHANNELS       308
#define DXP_NODETCHAN        309
#define DXP_NOIOCHAN         310
#define DXP_NOMODCHAN        311
#define DXP_NEGBLOCKSIZE     312
#define DXP_FILENOTFOUND     313
#define DXP_NOFILETABLE		 314
#define DXP_INITIALIZE	     315
/*  host machine errors codes:  401-500 */
#define DXP_NOMEM            401
#define DXP_CLOSE_FILE       403
#define DXP_INDEXOOB         404
/* Debug support */
#define DXP_DEBUG           1001


/* Definitions of CAMAC addresses for the DXP boards */
#define DXP_TSAR_F_WRITE		17
#define DXP_TSAR_A_WRITE		1
#define DXP_CSR_F_WRITE			17
#define DXP_CSR_A_WRITE			0
#define DXP_CSR_F_READ			1
#define DXP_CSR_A_READ			0
#define DXP_DATA_F_READ			0
#define DXP_DATA_A_READ			0
#define DXP_DATA_F_WRITE		16
#define DXP_DATA_A_WRITE		0
#define DXP_DISABLE_LAM_F		24
#define DXP_DISABLE_LAM_A		0
#define DXP_ENABLE_LAM_F		26
#define DXP_ENABLE_LAM_A		0
#define DXP_TEST_LAM_SOURCE_F	27
#define DXP_TEST_LAM_SOURCE_A	0
#define DXP_TEST_LAM_F			8
#define DXP_TEST_LAM_A			0
#define DXP_CLEAR_LAM_F			10
#define DXP_CLEAR_LAM_A			0
#define DXP_FIPPI_F_WRITE		17
#define DXP_FIPPI_A_WRITE		3
#define DXP_DSP_F_WRITE			17
#define DXP_DSP_A_WRITE			2

#endif						/* Endif for DXP_H */