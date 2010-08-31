/**
 * @file xia_mercury.h
 * @brief Private header for Mercury low-level driver.
 */

/*
 * Copyright (c) 2007, XIA LLC
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
 * $Id: xia_mercury.h 11613 2009-02-24 23:19:19Z ann $
 *
 */


#ifndef __XIA_MERCURY_H__
#define __XIA_MERCURY_H__

#include <stdio.h>

#include "md_generic.h"

#include "xia_xerxes_structures.h"

#include "xerxesdef.h"
#include "xerxes_structures.h"


/* These are the routines that are required to be available since they are
 * called by Xerxes.
 */
XERXES_EXPORT int dxp_init_mercury(Functions *funcs);
XERXES_STATIC int dxp_init_driver(Interface *);
XERXES_STATIC int dxp_init_utils(Utils *);
XERXES_STATIC int dxp_look_at_me(int *ioChan, int *modChan);
XERXES_STATIC int dxp_ignore_me(int *ioChan, int *modChan);
XERXES_STATIC int dxp_clear_LAM(int *ioChan, int *modChan);
XERXES_STATIC int dxp_prep_for_readout(int *, int *);
XERXES_STATIC int dxp_done_with_readout(int *, int *, Board *board);
XERXES_STATIC int dxp_begin_run(int *ioChan, int *modChan,
                                unsigned short *gate,
                                unsigned short *resume, 
                                Board *board, int *id);
XERXES_STATIC int dxp_end_run(int *ioChan, int *modChan, Board *board);
XERXES_STATIC int dxp_run_active(int *, int *, int*);
XERXES_STATIC int dxp_begin_control_task(int* ioChan, int* modChan,
                                         short *type, 
                                         unsigned int *length, int *info,
                                         Board *board);
XERXES_STATIC int dxp_end_control_task(int* ioChan, int* modChan, Board *board);
XERXES_STATIC int dxp_control_task_params(int* ioChan, int* modChan,
                                          short *type, Board *board, int *info);
XERXES_STATIC int dxp_control_task_data(int* ioChan, int* modChan, short *type, 
                                        Board *board, void *data);
XERXES_STATIC int dxp_loc(char *, Dsp_Info *, unsigned short *);
XERXES_STATIC int dxp_get_dspinfo(Dsp_Info *);
XERXES_STATIC int dxp_get_fipinfo(void *);
XERXES_STATIC int dxp_get_defaultsinfo(Dsp_Defaults *);
XERXES_STATIC int dxp_get_fpgaconfig(void *fippi);
XERXES_STATIC int dxp_download_fpga_done(int *modChan, char *name,
                                         Board *board); 
XERXES_STATIC int dxp_download_fpgaconfig(int *ioChan, int *modChan,
                                          char *name, Board *board);
XERXES_STATIC int dxp_download_dspconfig(int *ioChan, int *modChan,
                                         Board *board);
XERXES_STATIC int dxp_get_dspconfig(Dsp_Info *);
XERXES_STATIC int dxp_get_dspdefaults(Dsp_Defaults *);
XERXES_STATIC int XERXES_API dxp_decode_error(int *, int *, Dsp_Info *, 
                unsigned short *, unsigned short *);
XERXES_STATIC int dxp_clear_error(int *, int *, Board *);
XERXES_STATIC int dxp_get_runstats(int *ioChan, int *modChan, Board *b,
                                   unsigned long *evts, unsigned long *under,
                                   unsigned long *over, unsigned long *fast,
                                   unsigned long *base, double *live,
                                   double *icr, double *ocr);
XERXES_STATIC int dxp_symbolname(unsigned short *, Dsp_Info *, char *);
XERXES_STATIC int dxp_modify_dspsymbol(int *, int *, char *,
                                       unsigned short *, Board *);
XERXES_STATIC int dxp_write_dsp_param_addr(int *, int *, unsigned int *,
                                           unsigned short *);
XERXES_STATIC int dxp_read_dspsymbol(int *, int *, char *, Board *, double *);
XERXES_STATIC int dxp_read_dspparams(int *ioChan, int *modChan, Board *b,
                                     unsigned short *params);
XERXES_STATIC int dxp_write_dspparams(int *, int *, Dsp_Info *,
                                      unsigned short *);
XERXES_STATIC int dxp_get_spectrum_length(int *ioChan, int *modChan,
                                          Board *board, unsigned int *len);
XERXES_STATIC unsigned int dxp_get_sca_length(Dsp_Info *, unsigned short *);
XERXES_STATIC int dxp_get_baseline_length(int *modChan, Board *b,
                                          unsigned int *len);
XERXES_STATIC unsigned int dxp_get_event_length(Dsp_Info *, unsigned short *);
XERXES_STATIC int dxp_read_spectrum(int *, int *, Board *, unsigned long *);
XERXES_STATIC int dxp_read_baseline(int *ioChan, int *modchan, Board *board,
                                    unsigned long *baseline);
XERXES_STATIC int dxp_read_sca(int *ioChan, int *modChan, Board *board,
                               unsigned long *sca);
XERXES_STATIC int dxp_setup_cmd(Board *board, char *name, unsigned int *lenS,
                                byte_t *send, unsigned int *lenR,
                                byte_t *receive, byte_t ioFlags);
XERXES_STATIC int dxp_read_mem(int *ioChan, int *modChan, Board *board,
                               char *name, unsigned long *base,
                               unsigned long *offset, unsigned long *data);
XERXES_STATIC int dxp_write_mem(int *ioChan, int *modChan, Board *board,
                                char *name, unsigned long *base,
                                unsigned long *offset, unsigned long *data);
XERXES_STATIC int dxp_write_reg(int *ioChan, int *modChan, char *name, 
                                unsigned long *data);
XERXES_STATIC int dxp_read_reg(int *ioChan, int *modChan, char *name,
                               unsigned long *data);
XERXES_STATIC int dxp_do_cmd(int *ioChan, byte_t cmd, unsigned int lenS,
                             byte_t *send, unsigned int lenR, byte_t *receive);
XERXES_STATIC int dxp_unhook(Board *board);
XERXES_STATIC int dxp_get_symbol_by_index(int modChan, unsigned short index,
                                          Board *board, char *name);
XERXES_STATIC int dxp_get_num_params(int modChan, Board *b,
                                     unsigned short *n_params);
static FILE *dxp_find_file(const char* filename, const char* mode);

/* Logging macro wrappers */
#define dxp_log_error(x, y, z)  \
  mercury_md_log(MD_ERROR, (x), (y), (z), __FILE__, __LINE__)
#define dxp_log_warning(x, y)	  \
  mercury_md_log(MD_WARNING, (x), (y), 0, __FILE__, __LINE__)
#define dxp_log_info(x, y)	    \
  mercury_md_log(MD_INFO, (x), (y), 0, __FILE__, __LINE__)
#define dxp_log_debug(x, y)     \
	mercury_md_log(MD_DEBUG, (x), (y), 0, __FILE__, __LINE__)

/* Memory allocation macro wrappers */
#ifdef USE_XIA_MEM_MANAGER
#include "xia_mem.h"
#define mercury_md_alloc(n)  xia_mem_malloc((n), __FILE__, __LINE__)
#define mercury_md_free(ptr) xia_mem_free(ptr)
#endif /* USE_XIA_MEM_MANAGER */

static DXP_MD_IO         mercury_md_io;
static DXP_MD_SET_MAXBLK mercury_md_set_maxblk;
static DXP_MD_GET_MAXBLK mercury_md_get_maxblk;
static DXP_MD_LOG        mercury_md_log;
static DXP_MD_ALLOC      mercury_md_alloc;
static DXP_MD_FREE       mercury_md_free;
static DXP_MD_PUTS       mercury_md_puts;
static DXP_MD_WAIT       mercury_md_wait;
static DXP_MD_FGETS      mercury_md_fgets;

/* Typedefs */
typedef int (*fpga_downloader_fp_t)(int ioChan, int modChan, Board *board);
typedef int (*do_control_task_data_fp_t)(int ioChan, int modChan,
                                         unsigned long *data, Board *b);
typedef int (*do_control_task_fp_t)(int, int, Board *);
typedef int (*do_control_task_info_fp_t)(int ioChan, int modChan,
									   unsigned int length, int *info, Board *b);

/* Structs */

/**
 * @brief Element of a function dispatch table for the possible FPGA
 * download types.
 */
typedef struct _fpga_downloader {
  
  char *type;
  fpga_downloader_fp_t f;

} fpga_downloader_t;


/**
 * @brief Element of a function dispatch table for control tasks that are
 * run via. the data readout function.
 */
typedef struct _control_task_data {

  int type;
  do_control_task_data_fp_t fn;

} control_task_data_t;


typedef struct _control_task {

  int type;
  do_control_task_info_fp_t fn_info;
  do_control_task_fp_t fn;

} control_task_t;


typedef struct _register_table {

  char *name;
  unsigned long addr;

} register_table_t;


/* Firmware constatns */

/* Maximum FiPPI length in _bytes_. */
#define MAXFIP_LEN 0x00200000
#define MAXDSP_LEN 0x10000

/* Communication constants */
#define DXP_A_IO    0
#define DXP_A_ADDR  1

#define DXP_F_IGNORE 0
#define DXP_F_WRITE  1
#define DXP_F_READ   0

/* CPLD register addresses */
#define DXP_CPLD_CFG_CTRL   0x10000001
#define DXP_CPLD_CFG_DATA   0x10000002
#define DXP_CPLD_CFG_STATUS 0x10000003

/* CPLD Control Register Bit Masks */
#define DXP_CPLD_CTRL_SYS_FPGA 0x1
#define DXP_CPLD_CTRL_SYS_FIP  0x2

/* A map of the status values for the XDONE and INIT* lines indexed by target.
 * To access this, you would use syntax like:
 *
 * system_fpga_xdone = MERCURY_CFG_STATUS[0][MERCURY_XDONE];
 *
 * etc.
 */
#define MERCURY_NUM_TARGETS 2

#define MERCURY_INIT      0
#define MERCURY_XDONE     1

static unsigned long MERCURY_CFG_STATUS[MERCURY_NUM_TARGETS][2] = {
  { 0x1,  0x2  },
  { 0x4,  0x8  },
};

static char *MERCURY_FPGA_NAMES[MERCURY_NUM_TARGETS] = {
  "system FPGA",
  "FiPPI A",
};


/* CPLD Status Register Bit Masks */
#define DXP_CPLD_STATUS_SYS_INIT 0x1
#define DXP_CPLD_STATUS_SYS_DONE 0x2
#define DXP_CPLD_STATUS_FIP_INIT 0x4
#define DXP_CPLD_STATUS_FIP_DONE 0x8

/* System FPGA Register Addresses */
#define DXP_SYS_REG_CSR 0x08000002

/* System FPGA CSR Bits */
#define DXP_CSR_RUN_ENABLE      0
#define DXP_CSR_RESET_MCA       1
#define DXP_CSR_RESET_DSP_BIT   2
#define DXP_CSR_BOOT_DSP_BIT    3
#define DXP_CSR_RUN_ACT_BIT     16
#define DXP_CSR_DSP_ACT_BIT     17

/* Base Addresses */
#define DXP_DSP_PROG_MEM_ADDR 0x00000000
#define DXP_DSP_DATA_MEM_ADDR 0x01000000
#define DXP_DSP_EXT_MEM_ADDR  0x03000000


/* RUNTYPE constants */
#define MERCURY_RUNTYPE_NORMAL  0
#define MERCURY_RUNTYPE_SPECIAL 1

/* TRACETYPE constants */
#define MERCURY_TRACETYPE_ADC           0x0
#define MERCURY_TRACETYPE_FAST_BASE_SUB 0x4
#define MERCURY_TRACETYPE_BASE_INST     0x6
#define MERCURY_TRACETYPE_BASE_HIST     0x7
#define MERCURY_TRACETYPE_BASE_SUB      0x8
#define MERCURY_TRACETYPE_SLOW_BASE_SUB 0xA
#define MERCURY_TRACETYPE_EVENTS        0xB

/* SPECIALRUN constants */
#define MERCURY_SPECIALRUN_APPLY     0
#define MERCURY_SPECIALRUN_TRACE     1
#define MERCURY_SPECIALRUN_TEST_1    2 /* Writes test pattern #1 to external memory */
#define MERCURY_SPECIALRUN_TEST_2    3 /* Writes test pattern #2 to external memory */
#define MERCURY_SPECIALRUN_DSP_SLEEP 7

/* Memory constants */
#define MERCURY_MEMORY_BLOCK_SIZE  256
#define MERCURY_MEMORY_32_MAX_ADDR 0x100000

/* relative offset for each channel in the external memory SCA block */
#define MERCURY_SCA_CHAN_OFFSET     0x40

/* These are relative offsets for each channel in the external memory
 * statistics block.
 */
static unsigned long MERCURY_STATS_CHAN_OFFSET[] = {
  0x000000,
  0x000040,
  0x000080,
  0x0000C0
};

/* Statistics offset in external memroy */
#define MERCURY_STATS_REALTIME_OFFSET   0x0
#define MERCURY_STATS_TLIVETIME_OFFSET  0x2
#define MERCURY_STATS_TRIGGERS_OFFSET   0x6
#define MERCURY_STATS_MCAEVENTS_OFFSET  0x8
#define MERCURY_STATS_UNDERFLOWS_OFFSET 0xA
#define MERCURY_STATS_OVERFLOWS_OFFSET  0xC

/* Run control constants */
#define RESUME_RUN 1

/* Useful structure access macros */
#define PARAMS(x)   (x)->system_dsp->params

#endif /* __XIA_MERCURY_H__ */
