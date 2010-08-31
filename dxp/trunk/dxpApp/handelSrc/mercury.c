/*
 * Copyright (c) 2007-2009, XIA LLC
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
 * $Id: mercury.c 15339 2010-04-13 19:02:10Z ann $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "mercury.h"

#include "xia_assert.h"
#include "xia_common.h"
#include "xia_file.h"
#include "xia_mercury.h"

#include "xerxes_errors.h"
#include "xerxes_generic.h"

static char info_string[INFO_LEN];


/* Basic I/O */
static int dxp__write_word(int *ioChan, unsigned long addr,
                           unsigned long val);
static int dxp__read_word(int *ioChan, unsigned long addr,
                          unsigned long *val);
static int dxp__write_block(int *ioChan, unsigned long addr, unsigned long n,
                            unsigned long *data);
static int dxp__read_block(int *ioChan, unsigned long addr, unsigned long n,
                           unsigned long *data);
static int dxp__get_mca_chan_addr(int ioChan, int modChan, Board *board,
								  unsigned long *addr);

/* Register manipulation */
static int dxp__set_csr_bit(int ioChan, byte_t bit);
static int dxp__clear_csr_bit(int ioChan, byte_t bit);
static int dxp__read_global_register(int ioChan, unsigned long reg,
										   unsigned long *val);
static int dxp__write_global_register(int ioChan, unsigned long reg,
											unsigned long val);          
                      
/* FPGA downloaders */
static int dxp__download_system_fpga(int ioChan, int modChan, Board *b);
static int dxp__download_all_fpgas(int ioChan, int modChan, Board *b);
static int dxp__download_fippi(int ioChan, int modChan, Board *b);
static int dxp__download_fippi_dsp_no_wake(int ioChan, int modChan, Board *b);
static int dxp__download_fpga(int ioChan, unsigned long target, Fippi_Info *fpga);

/* Firmware parsers */
static int dxp__load_symbols_from_file(char *file, Dsp_Params *params);
static int dxp__load_dsp_code_from_file(char *file, Dsp_Info *dsp);

/* Misc. firmware helpers */
static int dxp__reset_dsp(int ioChan);
static int dxp__boot_dsp(int ioChan, int modChan, Board *b);

/* DSP parameter helpers */
static int dxp__get_global_addr(char *name, Dsp_Info *dsp, unsigned long *addr);
static int dxp__get_channel_addr(char *name, int modChan, Dsp_Info *dsp,
                                 unsigned long *addr);
static int dxp__is_symbol_global(char *name, Dsp_Info *dsp,
                                 boolean_t *is_global);

/* Misc. */
static int dxp__wait_for_busy(int ioChan, int modChan, parameter_t desired,
                              double timeout, Board *board);
static int dxp__wait_for_active(int ioChan, int modChan, double timeout, 
                                Board *board);
static int dxp__run_enable_active(int ioChan, int *active);
static double dxp__unsigned64_to_double(unsigned long *u64);
static double dxp__get_clock_tick(void);
static int dxp__put_dsp_to_sleep(int ioChan, int modChan, Board *b);
static int dxp__wake_dsp_up(int ioChan, int modChan, Board *b);
static int dxp__get_sca_addr(int ioChan, int modChan, Board *b,
							 unsigned long *addr);

/* Control task operations */
static int dxp__do_apply(int ioChan, int modChan, Board *board);
static int dxp__get_adc_trace(int ioChan, int modChan, unsigned long *data,
                              Board *board);
static int dxp__begin_adc_trace(int ioChan, int modChan, Board *board);
static int dxp__process_trace_wait(int ioChan, int modChan, unsigned int len,
                                    int *info, Board *b);
static int dxp__do_trace(int ioChan, int modChan, parameter_t type, Board *board);
static int dxp__begin_base_hist(int ioChan, int modChan, Board *board);
static int dxp__begin_fast_base_sub_trace(int ioChan, int modChan, Board *board);
static int dxp__begin_base_inst_trace(int ioChan, int modChan, Board *board);
static int dxp__begin_base_sub_trace(int ioChan, int modChan, Board *board);
static int dxp__begin_slow_base_sub_trace(int ioChan, int modChan, Board *board);
static int dxp__begin_events_trace(int ioChan, int modChan, Board *board);
                                    
                                    
static fpga_downloader_t FPGA_DOWNLOADERS[] = {
  { "system_fpga", dxp__download_system_fpga },
  { "all",         dxp__download_all_fpgas   },
  { "a",           dxp__download_fippi       },
  { "a_dsp_no_wake", dxp__download_fippi_dsp_no_wake },   
};

static control_task_t CONTROL_TASKS[] = {
  { MERCURY_CT_ADC,  dxp__process_trace_wait,   dxp__begin_adc_trace },
  { MERCURY_CT_APPLY,         NULL,             dxp__do_apply},  
  { MERCURY_CT_WAKE_DSP,      NULL,             dxp__wake_dsp_up},
  { MERCURY_CT_BASE_HIST,     dxp__process_trace_wait, dxp__begin_base_hist },
  { MERCURY_CT_FAST_BASE_SUB, dxp__process_trace_wait, dxp__begin_fast_base_sub_trace},
  { MERCURY_CT_BASE_INST,     dxp__process_trace_wait, dxp__begin_base_inst_trace},
  { MERCURY_CT_BASE_SUB,      dxp__process_trace_wait, dxp__begin_base_sub_trace},
  { MERCURY_CT_SLOW_BASE_SUB, dxp__process_trace_wait, dxp__begin_slow_base_sub_trace},
  { MERCURY_CT_EVENTS,        dxp__process_trace_wait, dxp__begin_events_trace},  
};

static control_task_data_t CONTROL_TASK_DATA[] = {
  { MERCURY_CT_ADC,         dxp__get_adc_trace },
};

/* These are registers that are publicly exported for use in Handel and other
 * callers. Not every register needs to be included here.
 */
static register_table_t REGISTER_TABLE[] = {
  {"CVR",     0x10000004},
  {"SVR",     0x08000001},
  {"CSR",     0x08000002},
  {"VAR",     0x08000003},
  {"MCR",     0x08000006},
  {"MFR",     0x08000007},
  {"SYNCCNT", 0x08000009},
};


/** @brief Initialize the Mercury functions that Xerxes needs.
 *
 */
XERXES_EXPORT int dxp_init_mercury(Functions* funcs)
{
  ASSERT(funcs != NULL);

  funcs->dxp_init_driver = dxp_init_driver;
  funcs->dxp_init_utils  = dxp_init_utils;
  funcs->dxp_get_dspinfo = dxp_get_dspinfo;
  funcs->dxp_get_fipinfo = dxp_get_fipinfo;
  funcs->dxp_get_defaultsinfo = dxp_get_defaultsinfo;
  funcs->dxp_get_dspconfig = dxp_get_dspconfig;
  funcs->dxp_get_fpgaconfig = dxp_get_fpgaconfig;
  funcs->dxp_get_dspdefaults = dxp_get_dspdefaults;
  funcs->dxp_download_fpgaconfig = dxp_download_fpgaconfig;
  funcs->dxp_download_fpga_done = dxp_download_fpga_done;
  funcs->dxp_download_dspconfig = dxp_download_dspconfig;
  funcs->dxp_look_at_me = dxp_look_at_me;
  funcs->dxp_ignore_me = dxp_ignore_me;
  funcs->dxp_clear_LAM = dxp_clear_LAM;
  funcs->dxp_loc = dxp_loc;
  funcs->dxp_symbolname = dxp_symbolname;
  funcs->dxp_read_spectrum = dxp_read_spectrum;
  funcs->dxp_get_spectrum_length = dxp_get_spectrum_length;
  funcs->dxp_read_baseline = dxp_read_baseline;
  funcs->dxp_get_baseline_length = dxp_get_baseline_length;
  funcs->dxp_get_event_length = dxp_get_event_length;
  funcs->dxp_write_dspparams = dxp_write_dspparams;
  funcs->dxp_write_dsp_param_addr = dxp_write_dsp_param_addr;
  funcs->dxp_read_dspparams = dxp_read_dspparams;
  funcs->dxp_read_dspsymbol = dxp_read_dspsymbol;
  funcs->dxp_modify_dspsymbol = dxp_modify_dspsymbol;
  funcs->dxp_prep_for_readout = dxp_prep_for_readout;
  funcs->dxp_done_with_readout = dxp_done_with_readout;
  funcs->dxp_begin_run = dxp_begin_run;
  funcs->dxp_end_run = dxp_end_run;
  funcs->dxp_run_active = dxp_run_active;
  funcs->dxp_begin_control_task = dxp_begin_control_task;
  funcs->dxp_end_control_task = dxp_end_control_task;
  funcs->dxp_control_task_params = dxp_control_task_params;
  funcs->dxp_control_task_data = dxp_control_task_data;
  funcs->dxp_decode_error = dxp_decode_error;
  funcs->dxp_clear_error = dxp_clear_error;
  funcs->dxp_get_runstats = dxp_get_runstats;
  funcs->dxp_setup_cmd = dxp_setup_cmd;
  funcs->dxp_read_mem = dxp_read_mem;
  funcs->dxp_write_mem = dxp_write_mem;
  funcs->dxp_write_reg = dxp_write_reg;
  funcs->dxp_read_reg  = dxp_read_reg;
  funcs->dxp_do_cmd = dxp_do_cmd;
  funcs->dxp_unhook = dxp_unhook;
  funcs->dxp_get_sca_length = dxp_get_sca_length;
  funcs->dxp_read_sca = dxp_read_sca;
  funcs->dxp_get_symbol_by_index = dxp_get_symbol_by_index;
  funcs->dxp_get_num_params = dxp_get_num_params;

  return DXP_SUCCESS;
}


/** @brief Initialize the inteface function table.
 *
 */
static int dxp_init_driver(Interface* iface) 
{
  ASSERT(iface != NULL);


  mercury_md_io         = iface->funcs->dxp_md_io;
  mercury_md_set_maxblk = iface->funcs->dxp_md_set_maxblk;
  mercury_md_get_maxblk = iface->funcs->dxp_md_get_maxblk;

  return DXP_SUCCESS;
}


/** @brief Initialize the utility function table.
 *
 */
static int dxp_init_utils(Utils* utils) 
{
  ASSERT(utils != NULL);


  mercury_md_log = utils->funcs->dxp_md_log;

#ifdef XIA_SPECIAL_MEM
  mercury_md_alloc = utils->funcs->dxp_md_alloc;
  mercury_md_free  = utils->funcs->dxp_md_free;
#else
  mercury_md_alloc = malloc;
  mercury_md_free  = free;
#endif /* XIA_SPECIAL_MEM */

  mercury_md_wait  = utils->funcs->dxp_md_wait;
  mercury_md_puts  = utils->funcs->dxp_md_puts;
  mercury_md_fgets = utils->funcs->dxp_md_fgets;

  return DXP_SUCCESS;
}
 

/** @brief Unused for the Mercury hardware.
 *
 */
static int dxp_look_at_me(int* ioChan, int* modChan)
{
  UNUSED(ioChan);
  UNUSED(modChan);


  return DXP_SUCCESS;
}

/** @brief Unused for the Mercury hardware.
 *
 */
static int dxp_ignore_me(int* ioChan, int* modChan)
{
  UNUSED(ioChan);
  UNUSED(modChan);

 
  return DXP_SUCCESS;
}

/** @brief Unused for the Mercury hardware.
 *
 */
static int dxp_clear_LAM(int* ioChan, int* modChan)
{
  UNUSED(ioChan);
  UNUSED(modChan);


  return DXP_SUCCESS;
}


/** @brief Downloads code to the Mercury FPGA.
 */
static int dxp_download_fpgaconfig(int* ioChan, int* modChan, char *name,
                                   Board* b)
{
  int i;
  int status;


  ASSERT(ioChan != NULL);
  ASSERT(modChan != NULL);
  ASSERT(name != NULL);
  ASSERT(b != NULL);

  sprintf(info_string, "Preparing to download '%s'", name);
  dxp_log_debug("dxp_download_fpgaconfig", info_string);

  for (i = 0; i < N_ELEMS(FPGA_DOWNLOADERS); i++) {
    if (STREQ(FPGA_DOWNLOADERS[i].type, name)) {
      status = FPGA_DOWNLOADERS[i].f(*ioChan, *modChan, b);

      if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error downloading '%s' to ioChan = %d",
                name, *ioChan);
        dxp_log_error("dxp_download_fpgaconfig", info_string, status);
        return status;
      }

      return DXP_SUCCESS;
    }
  }

  sprintf(info_string, "Unknown FPGA, '%s', requested for download to "
          "ioChan = %d", name, *ioChan);
  dxp_log_error("dxp_download_fpgaconfig", info_string, DXP_UNKNOWN_FPGA);
  return DXP_UNKNOWN_FPGA;
}


/** @brief Parse the specified FPGA file.
 */
static int dxp_get_fpgaconfig(void *fip)
{
  int n_chars_in_line = 0;
  int i;
  int n_data = 0;

  FILE *fp = NULL;

  Fippi_Info *fippi = (Fippi_Info *)fip;
  
  char line[XIA_LINE_LEN];

  
  ASSERT(fippi       != NULL);
  ASSERT(fippi->data != NULL);


  sprintf(info_string, "Preparing to parse the FPGA configuration '%s'",
          fippi->filename);
  dxp_log_info("dxp_get_fpgaconfig", info_string);
  
  fp = dxp_find_file(fippi->filename, "r");

  if (!fp) {
    sprintf(info_string, "Unable to open FPGA configuration '%s'",
            fippi->filename);
    dxp_log_error("dxp_get_fpgaconfig", info_string, DXP_OPEN_FILE);
    return DXP_OPEN_FILE;
  }

  n_data = 0;

  /* This is the main loop to parse in the FPGA configuration file */
  while (mercury_md_fgets(line, XIA_LINE_LEN, fp) != NULL) {

    /* Ignore comments */
    if (line[0] == '*') {
      continue;
    }

    /* Ignore the trailing newline character. Note: does this only work
     * with MS-style EOLs or with all? Does C treat the EOL character as
     * a single \n? My experience on Win32 systems is that this works.
     */
    n_chars_in_line = strlen(line) - 1;

    for (i = 0; i < n_chars_in_line; i += 4, n_data++) {
      unsigned short first;
      unsigned short second;

      sscanf(&line[i], "%2hx%2hx", &first, &second);

      /* When the data is unpacked to be written to the hardware,
       * we will do it as lo-byte, followed by hi-byte, which is why
       * the second byte to be written is stored as the hi-byte.
       */
      fippi->data[n_data] = (unsigned short)((second << 8) | first);
    }
  }

  fippi->proglen = n_data;

  xia_file_close(fp);

  return DXP_SUCCESS;
}


/** @brief Check that the specified FPGA has been downloaded correctly.
 *
 */
static int dxp_download_fpga_done(int* modChan, char *name, Board *board)
{
  UNUSED(modChan);
  UNUSED(name);
  UNUSED(board);
  

  return DXP_SUCCESS;
}


/** @brief Download the current System DSP.
 */
static int dxp_download_dspconfig(int* ioChan, int* modChan, Board *board)
{
  int status;
  int i;

  Dsp_Info *sys_dsp = NULL;

  unsigned long *dsp_data = NULL;


  ASSERT(board != NULL);
  ASSERT(board->system_dsp != NULL);
  ASSERT(ioChan != NULL);
  ASSERT(modChan != NULL);


  sys_dsp = board->system_dsp;

  status = dxp__reset_dsp(*ioChan);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error resetting the DSP for ioChan = %d", *ioChan);
    dxp_log_error("dxp_download_dspconfig", info_string, status);
    return status;
  }

  ASSERT(sys_dsp->proglen % 2 == 0);

  dsp_data = mercury_md_alloc((sys_dsp->proglen / 2) * sizeof(unsigned long));

  if (dsp_data == NULL) {
    sprintf(info_string, "Unable to allocate %d bytes for 'dsp_data'",
            (sys_dsp->proglen / 2) * sizeof(unsigned long));
    dxp_log_error("dxp_download_dspconfig", info_string, DXP_NOMEM);
    return DXP_NOMEM;
  }
  
  for (i = 0; i < (int) (sys_dsp->proglen / 2); i++) {
    dsp_data[i] = (unsigned long)(sys_dsp->data[(i * 2) + 1] << 16) |
      sys_dsp->data[i * 2];
  }

  status = dxp__write_block(ioChan, DXP_DSP_PROG_MEM_ADDR, 
                                  sys_dsp->proglen / 2, dsp_data);

  mercury_md_free(dsp_data);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error writing DSP data block for ioChan = %d",
            *ioChan);
    dxp_log_error("dxp_download_dspconfig", info_string, status);
    return status;
  }
  
  status = dxp__boot_dsp(*ioChan, *modChan, board);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error booting DSP for ioChan = %d", *ioChan);
    dxp_log_error("dxp_download_dspconfig", info_string, status);
    return status;
  }

  return DXP_SUCCESS;
}


/** @brief Sets the maximum FiPPI size.
 *
 */
static int dxp_get_fipinfo(void *fippi)
{
  Fippi_Info *fip = (Fippi_Info *)fippi;
  

  ASSERT(fip != NULL);


  fip->maxproglen = MAXFIP_LEN;

  return DXP_SUCCESS;
}


/** @brief Unused for the Mercury.
 *n
 */
static int dxp_get_defaultsinfo(Dsp_Defaults* defaults)
{
  ASSERT(defaults != NULL);
  

  defaults->params->maxsym    = MAXSYM;
  defaults->params->maxsymlen = MAX_DSP_PARAM_NAME_LEN;

  return DXP_SUCCESS;
}


/** @brief Unused for the Mercury.
 *
 */
static int dxp_get_dspinfo(Dsp_Info* dsp)
{
  ASSERT(dsp != NULL);


  dsp->params->maxsym    = MAXSYM;
  dsp->params->maxsymlen = MAX_DSP_PARAM_NAME_LEN;
  dsp->maxproglen        = MAXDSP_LEN;

  return DXP_SUCCESS;
}


/** @brief Parses a Mercury DSP code file into program data and a symbol
 * table.
 *
 * The Mercury uses .dsx files.
 */
XERXES_STATIC int dxp_get_dspconfig(Dsp_Info* dsp)
{
  int status;


  ASSERT(dsp != NULL);
  ASSERT(dsp->params != NULL);
  ASSERT(dsp->filename != NULL);


  dsp->params->maxsym    = MAXSYM;
  dsp->params->maxsymlen = MAX_DSP_PARAM_NAME_LEN;
  dsp->maxproglen        = MAXDSP_LEN;
  dsp->params->nsymbol   = 0;
  dsp->proglen           = 0;

  status = dxp__load_symbols_from_file(dsp->filename, dsp->params);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error loading symbols from %s", dsp->filename);
	dxp_log_error("dxp_get_dspconfig", info_string, status);
	return status;
  }

  status = dxp__load_dsp_code_from_file(dsp->filename, dsp);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error loading DSP code from %s", dsp->filename);
	dxp_log_error("dxp_get_dspconfig", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/** @brief Reads the actual DSP code from a .dsx file
 *
 * Ignores all of the .dsx header information and just parses in the
 * PROGRAM MEMORY section of the .dsx file. In the future, this program
 * may also parse in the DATA MEMORY section as well.
 *
 */
XERXES_STATIC int dxp__load_dsp_code_from_file(char *file, Dsp_Info *dsp)
{
  int i;
  int n_chars = 0;

  unsigned long n_data_words = 0;

  unsigned short hi = 0x0000;
  unsigned short lo = 0x0000;

  char line[82];

  FILE *fp = NULL;
  

  ASSERT(file       != NULL);
  ASSERT(dsp        != NULL);
  ASSERT(dsp-> data != NULL);


  fp = dxp_find_file(file, "r");

  if (!fp) {
    sprintf(info_string, "Error opening %s while trying to load DSP code",
            file);
    dxp_log_error("dxp__load_dsp_code_from_file", info_string, DXP_OPEN_FILE);
    return DXP_OPEN_FILE;
  }

  while (mercury_md_fgets(line, sizeof(line), fp) != NULL) {
	
    /* Skip comments */
    if (line[0] == '*') {
      continue;
    }

    if (STRNEQ(line, "@PROGRAM MEMORY@")) {
      while (mercury_md_fgets(line, sizeof(line), fp)) {

        n_chars = strlen(line);

        for (i = 0; (i < n_chars) && isxdigit(line[i]); i += 8) {
          sscanf(&line[i], "%4hx%4hx", &hi, &lo);

          dsp->data[n_data_words++] = lo;
          dsp->data[n_data_words++] = hi;
        }
      }
	  
      dsp->proglen = n_data_words;

      sprintf(info_string, "DSP Code length = %lu", dsp->proglen);
      dxp_log_debug("dxp__load_dsp_code_from_file", info_string);

      xia_file_close(fp);

      return DXP_SUCCESS;
    }
  }

  xia_file_close(fp);

  sprintf(info_string, "Malformed DSX file '%s' is missing '@PROGRAM MEMORY@' "
          "section", file);
  dxp_log_error("dxp__load_dsp_code_from_file", info_string,
                DXP_MALFORMED_FILE);
  return DXP_MALFORMED_FILE;
}


/** @brief Reads the symbols from a .dsx file.
 *
 * Parses in the sections of the .dsx file that contains the DSP parameter
 * information, including the offsets and the per-channel parameters.
 *
 */
static int dxp__load_symbols_from_file(char *file, Dsp_Params *params)
{
  int i;

  unsigned short n_globals  = 0;
  unsigned short n_per_chan = 0;

  unsigned long global_offset = 0;
  unsigned long offset        = 0;

  char line[82];

  static int call_count = 0;

  FILE *fp = NULL;


  ASSERT(file   != NULL);
  ASSERT(params != NULL);


  fp = dxp_find_file(file, "r");

  if (!fp) {
	sprintf(info_string, "Error opening '%s' while trying to load DSP "
			"parameters", file);
	dxp_log_error("dxp__load_symbols_from_file", info_string, DXP_OPEN_FILE);
	return DXP_OPEN_FILE;
  }

  while (mercury_md_fgets(line, sizeof(line), fp) != NULL) {

	/* Skip comment lines */
	if (line[0] == '*') {
	  continue;
	}
	
	/* Ignore lines that don't contain a section header */
	if (line[0] != '@') {
	  continue;
	}

	if (STRNEQ(line, "@CONSTANTS@")) {
	  sscanf(mercury_md_fgets(line, sizeof(line), fp), "%hu", &n_globals);
	  sscanf(mercury_md_fgets(line, sizeof(line), fp), "%hu", &n_per_chan);

	  params->nsymbol            = n_globals;
	  params->n_per_chan_symbols = n_per_chan;

	  sprintf(info_string, "n_globals = %hu, n_per_chan = %hu", n_globals,
            n_per_chan);
	  dxp_log_debug("dxp__load_symbols_from_file", info_string);
	
	} else if (STRNEQ(line, "@OFFSETS@")) {
	  /* Offsets in the Dsp_Params structure need to be initialized */
	  params->chan_offsets = (unsigned long *)mercury_md_alloc(4 *
													  sizeof(unsigned long));
	  
	  if (!params->chan_offsets) {
		sprintf(info_string, "Error allocating %d bytes for 'params->"
				"chan_offsets'", 4 * sizeof(unsigned long));
		dxp_log_error("dxp__load_symbols_from_file", info_string, DXP_NOMEM);
		return DXP_NOMEM;
	  }
															
	  /* Global DSP parameters are stored by their absolute address. Per
	   * channel DSP parameters, since they have 4 unique addresses, are
	   * stored as offsets relative to the appropriate channel offset.
	   */
	  sscanf(mercury_md_fgets(line, sizeof(line), fp), "%lx", &global_offset);

	  sprintf(info_string, "global_offset = %#lx", global_offset);
	  dxp_log_debug("dxp__load_symbols_from_file", info_string);

	  for (i = 0; i < 4; i++) {
		sscanf(mercury_md_fgets(line, sizeof(line), fp), "%lx",
           &(params->chan_offsets[i]));
		
		sprintf(info_string, "chan%d_offset = %#lx", i, params->chan_offsets[i]);
		dxp_log_debug("dxp__load_symbols_from_file", info_string);
	  }
	
	} else if (STRNEQ(line, "@GLOBAL@")) {
	  for (i = 0; i < n_globals; i++) {
		sscanf(mercury_md_fgets(line, sizeof(line), fp), "%s : %lu",
			   params->parameters[i].pname, &offset);
		params->parameters[i].address = offset + global_offset;

		sprintf(info_string, "Global DSP Parameter: %s, addr = %#x",
				params->parameters[i].pname, params->parameters[i].address);
		dxp_log_debug("dxp__load_symbols_from_file", info_string);
	  }
	
	} else if (STRNEQ(line, "@CHANNEL@")) {
	  for (i = 0; i < n_per_chan; i++) {
		sscanf(mercury_md_fgets(line, sizeof(line), fp), "%s : %lu",
			   params->per_chan_parameters[i].pname, &offset);
		params->per_chan_parameters[i].address = offset;

		sprintf(info_string, "Per Channel DSP Parameter: %s, addr = %x",
				params->per_chan_parameters[i].pname,
				params->per_chan_parameters[i].address);
		dxp_log_debug("dxp__load_symbols_from_file", info_string);
	  }
	}
  }

  xia_file_close(fp);
  
  return DXP_SUCCESS;
}


/** @brief Unused for the Mercury.
 */
static int dxp_get_dspdefaults(Dsp_Defaults* defaults)
{
  ASSERT(defaults != NULL);


  defaults->params->nsymbol = 0;

  return DXP_SUCCESS;
}


/** @brief Translate a parameter index into a DSP symbol name.
 *
 */
static int dxp_symbolname(unsigned short* lindex, Dsp_Info* dsp, char string[])
{
  UNUSED(lindex);
  UNUSED(dsp);
  UNUSED(string);


  return DXP_SUCCESS;
}


/** @brief Translates a DSP symbol name into an index.
 *
 */
static int dxp_loc(char name[], Dsp_Info* dsp, unsigned short* address)
{
  UNUSED(name);
  UNUSED(dsp);
  UNUSED(address);


  return DXP_SUCCESS;
}


/** @brief Get the value of what would be called a DSP symbol
 * on other products, but is really a register in the FPGA.
 *
 */
static int dxp_modify_dspsymbol(int* ioChan, int* modChan, char* name, 
                                unsigned short* value, Board *board)
{
  int status;

  unsigned long sym_addr = 0;
  unsigned long val      = 0;

  boolean_t is_global;

  Dsp_Info *dsp = board->system_dsp;


  ASSERT(ioChan != NULL);
  ASSERT(name != NULL);
  ASSERT(board != NULL);


  status = dxp__is_symbol_global(name, dsp, &is_global);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error determining if %s is a global parameter or not",
			name);
	dxp_log_error("dxp_modify_dspsymbol", info_string, status);
	return status;
  }

  /* The address is calculated differently depending if the parameter is a 
   * global parameter or a per-channel parameter.
   */
  if (is_global) {
	status = dxp__get_global_addr(name, dsp, &sym_addr);

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Unable to get address for global DSP parameter %s",
			  name);
	  dxp_log_error("dxp_modify_dspsymbol", info_string, status);
	  return status;
	}

  } else {

	status = dxp__get_channel_addr(name, *modChan, dsp, &sym_addr);

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Unable to get address for per-channnel DSP "
			  "parameter %s", name);
	  dxp_log_error("dxp_modify_dspsymbol", info_string, status);
	  return status;
	}
  }

  val = (unsigned long)(*value);
  sym_addr += DXP_DSP_DATA_MEM_ADDR;

  status = dxp__write_word(ioChan, sym_addr, val);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error writing '%s' to ioChan = %d", name, *ioChan);
    dxp_log_error("dxp_modify_dspsymbol", info_string, status);
    return status;
  }

  
  return DXP_SUCCESS;
}


/** @brief Write to a specific DSP parameter address.
 *
 */
static int dxp_write_dsp_param_addr(int* ioChan, int* modChan, 
                                    unsigned int* addr, unsigned short* value)
{
  UNUSED(ioChan);
  UNUSED(modChan);
  UNUSED(addr);
  UNUSED(value);
	

  return DXP_SUCCESS;
}


/** @brief Read a single parameter from the hardware.
 *
 * This routine handles all of the details of fetching global vs. per-chan
 * DSP parameters. 
 *
 * @a modChan is only used if the parameter is found to be a channel-specific
 * parameter.
 */
static int dxp_read_dspsymbol(int *ioChan, int *modChan, char *name, 
                              Board *b, double *value)
{
  int status;

  unsigned long sym_addr = 0;
  unsigned long val      = 0;

  boolean_t is_global;

  Dsp_Info *dsp = b->system_dsp;


  ASSERT(ioChan != NULL);
  ASSERT(name != NULL);
  ASSERT(b != NULL);
  ASSERT(modChan != NULL);
  ASSERT(value != NULL);


  status = dxp__is_symbol_global(name, dsp, &is_global);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error determining if '%s' is a global parameter or not",
			name);
	dxp_log_error("dxp_read_dspsymbol", info_string, status);
	return status;
  }

  /* The address is calculated differently depending if the parameter is a 
   * global parameter or a per-channel parameter.
   */
  if (is_global) {

    status = dxp__get_global_addr(name, dsp, &sym_addr);

    if (status != DXP_SUCCESS) {
      sprintf(info_string, "Unable to get address for global DSP parameter "
              "'%s'", name);
      dxp_log_error("dxp_read_dspsymbol", info_string, status);
      return status;
    }

  } else {

    status = dxp__get_channel_addr(name, *modChan, dsp, &sym_addr);

    if (status != DXP_SUCCESS) {
      sprintf(info_string, "Unable to get address for per-channnel DSP "
              "parameter '%s'", name);
      dxp_log_error("dxp_read_dspsymbol", info_string, status);
      return status;
    }
  }

  sym_addr += DXP_DSP_DATA_MEM_ADDR;

  status = dxp__read_word(ioChan, sym_addr, &val);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading '%s' at %#lx for ioChan = %d",
            name, sym_addr, *ioChan);
    dxp_log_error("dxp_read_dspsymbol", info_string, status);
    return status;
  }

  *value = (double)val;

  return DXP_SUCCESS;
}


/** @ brief Readout the parameter memory for a single channel.
 * 
 * This routine reads the parameter list from the DSP pointed to by ioChan and
 * modChan.  It returns the array to the caller.
 *
 */
static int dxp_read_dspparams(int* ioChan, int* modChan, Board *b, 
							  unsigned short* params)
{
  int status;

  unsigned int i;
  unsigned int offset = 0;

  unsigned long p    = 0;
  unsigned long addr = 0x0;

  
  ASSERT(modChan != NULL);
  ASSERT(ioChan != NULL);
  ASSERT(b != NULL);
  ASSERT(params != NULL);
  

  /* Read two separate blocks: the global block and the per-channel block. */
  
  for (i = 0; i < PARAMS(b)->nsymbol; i++) {
	addr = (unsigned long)PARAMS(b)->parameters[i].address |
	  (unsigned long)DXP_DSP_DATA_MEM_ADDR;

    status = dxp__read_word(ioChan, addr, &p);  

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Error reading DSP parameter located at %#lx for "
			  "ioChan = %d", addr, *ioChan);
	  dxp_log_error("dxp_read_dspparams", info_string, status);
	  return status;
	}

	params[i] = (unsigned short)p;
  }


  offset = PARAMS(b)->nsymbol;

  for (i = 0; i < PARAMS(b)->n_per_chan_symbols; i++) {
	addr = (unsigned long)PARAMS(b)->per_chan_parameters[i].address |
	  (unsigned long)PARAMS(b)->chan_offsets[*modChan] |
	  (unsigned long)DXP_DSP_DATA_MEM_ADDR;

    status = dxp__read_word(ioChan, addr, &p);  
	
	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Error reading DSP parameters located at %#lx for "
			  "ioChan = %d", addr, *ioChan);
	  dxp_log_error("dxp_read_dspparams", info_string, status);
	  return status;
	}

	params[i + offset] = (unsigned short)p;
  }

  return DXP_SUCCESS;
}


/** @brief Write an array of DSP parameters to the specified DSP.
 */
static int dxp_write_dspparams(int* ioChan, int* modChan, Dsp_Info* dsp, 
							   unsigned short* params)
{
  UNUSED(ioChan);
  UNUSED(modChan);
  UNUSED(dsp);
  UNUSED(params);


  return DXP_SUCCESS;
}


/** @brief Gets the length of the spectrum memory buffer.
 */
XERXES_STATIC int dxp_get_spectrum_length(int *ioChan, int *modChan,
                                          Board *board, unsigned int *len)
{
  int status;

  double MCALIMLO = 0.0;
  double MCALIMHI = 0.0;


  ASSERT(ioChan != NULL);
  ASSERT(board != NULL);
  ASSERT(len != NULL);
  ASSERT(modChan != NULL);

  status = dxp_read_dspsymbol(ioChan, modChan, "MCALIMLO", board, &MCALIMLO);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error reading lower MCA bin limit for ioChan = %d",
			*ioChan);
	dxp_log_error("dxp_get_spectrum_length", info_string, status);
	return status;
  }

  status = dxp_read_dspsymbol(ioChan, modChan, "MCALIMHI", board, &MCALIMHI);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error reading upper MCA bin limit for ioChan = %d",
			*ioChan);
	dxp_log_error("dxp_get_spectrum_length", info_string, status);
	return status;
  }

  *len = (unsigned int)(MCALIMHI - MCALIMLO);

  return DXP_SUCCESS;
}


/** @brief Gets the length of the baseline memory buffer.
 */
static int dxp_get_baseline_length(int *modChan, Board *b, unsigned int *len)
{
  int status;

  double BASELEN;

  
  ASSERT(modChan != NULL);
  ASSERT(b != NULL);
  ASSERT(len != NULL);

  
  status = dxp_read_dspsymbol(&(b->ioChan), modChan, "BASELEN", b, &BASELEN);
  
  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error getting BASELEN for baseline length "
			"calculation for ioChan = %d", b->ioChan);
	dxp_log_error("dxp_get_baseline_length", info_string, status);
	return status;
  }

  *len = (unsigned int)BASELEN;

  return DXP_SUCCESS;
}


/** @brief Unused on the Mercury.
 *
 */
static unsigned int dxp_get_event_length(Dsp_Info* dsp, unsigned short* params)
{
  UNUSED(dsp);
  UNUSED(params);


  return DXP_SUCCESS;
}


/** @brief Reads the spectrum memory for a single channel.
 */
static int dxp_read_spectrum(int* ioChan, int* modChan, Board* board, 
                             unsigned long* spectrum)
{
  int status;
  
  unsigned long addr;

  unsigned int spectrum_len;

  UNUSED(spectrum);

  ASSERT(ioChan != NULL);
  ASSERT(board != NULL);
  ASSERT(modChan != NULL);

  status = dxp__get_mca_chan_addr(*ioChan, *modChan, board, &addr);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error getting address of MCA spectrum for ioChan = %d",
			*ioChan);
	dxp_log_error("dxp_read_spectrum", info_string, status);
	return status;
  }

  sprintf(info_string, "ioChan = %d, modChan = %d: MCA addr = %#lx", *ioChan,
		  *modChan, addr);
  dxp_log_debug("dxp_read_spectrum", info_string);

  status = dxp_get_spectrum_length(ioChan, modChan, board, &spectrum_len);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error getting spectrum length for ioChan = %d",
			*ioChan);
	dxp_log_error("dxp_read_spectrum", info_string, status);
	return status;
  }

  if (spectrum_len == 0) {
    sprintf(info_string, "Returned spectrum length is 0 for ioChan = %d",
            *ioChan);
    dxp_log_error("dxp_read_spectrum", info_string, DXP_INVALID_LENGTH);
    return DXP_INVALID_LENGTH;
  }

  status = dxp__read_block(ioChan, addr, spectrum_len, spectrum);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error reading spectrum for ioChan = %d", *ioChan);
	dxp_log_error("dxp_read_spectrum", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/** @brief Returns the offset in the external memory of the selected module
 * channel.
 *
 * This routine verifies that the addr falls on a block boundary as specified
 * in the external memory documentation.
 */
static int dxp__get_mca_chan_addr(int ioChan, int modChan, Board *board,
								  unsigned long *addr)
{
  int status;
  int i;

  unsigned int mca_len = 0;

  unsigned long total_len = 0;


  ASSERT(addr != NULL);
  ASSERT(board != NULL);


  /* Calculate the external memory address for the specified module channel by
   * summing the lengths of the channels that precede it.
   */
  for (i = 0, total_len = 0; i < modChan; i++) {
	status = dxp_get_spectrum_length(&ioChan, &i, board, &mca_len);

	sprintf(info_string, "MCA length = %u for modChan = %d", mca_len, i);
	dxp_log_debug("dxp__get_mca_chan_addr", info_string);

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Error reading MCA spectrum length for ioChan = %d",
			  ioChan);
	  dxp_log_error("dxp__get_mca_chan_addr", info_string, status);
	  return status;
	}

	total_len += mca_len;
  }

  sprintf(info_string, "total_len = %lu", total_len);
  dxp_log_debug("dxp__get_mca_chan_addr", info_string);

  if ((total_len % MERCURY_MEMORY_BLOCK_SIZE) != 0) {
    sprintf(info_string, "Total MCA length (%lu) of channels prior to module "
        "channel %d is not a multiple of the memory block size (%d)",
        total_len, modChan, MERCURY_MEMORY_BLOCK_SIZE);
    dxp_log_error("dxp__get_mca_chan_addr", info_string, DXP_MEMORY_BLK_SIZE);
    return DXP_MEMORY_BLK_SIZE;
  }

  /* The spectrum starts at 0x100 in the external memory */
  *addr = total_len + DXP_DSP_EXT_MEM_ADDR + MERCURY_MEMORY_BLOCK_SIZE;
  
  return DXP_SUCCESS;
}


/** @brief Reads the baseline memory for a single channel.
 *
 */
static int dxp_read_baseline(int *ioChan, int *modChan, Board *board, 
                             unsigned long *baseline)
{
  int status;

  double BASESTART = 0.0;
  double BASELEN   = 0.0;

  unsigned long buffer_addr = 0;


  ASSERT(board != NULL);
  ASSERT(modChan != NULL);

  status = dxp_read_dspsymbol(ioChan, modChan, "BASESTART", board, &BASESTART);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error reading BASESTART for ioChan = %d", *ioChan);
	dxp_log_error("dxp_read_baseline", info_string, status);
	return status;
  }
  
  status = dxp_read_dspsymbol(ioChan, modChan, "BASELEN", board, &BASELEN);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error reading BASELEN for ioChan = %d", *ioChan);
	dxp_log_error("dxp_read_baseline", info_string, status);
	return status;
  }

  ASSERT(BASELEN != 0);

  buffer_addr = (unsigned long)BASESTART + DXP_DSP_DATA_MEM_ADDR;
   
  status = dxp__read_block(ioChan, buffer_addr, (unsigned int)BASELEN,
						   baseline);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error reading baseline histogram from %#lx for "
			"ioChan = %d", buffer_addr, *ioChan);
	dxp_log_error("dxp_read_baseline", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}
	

/** @brief Unused on the Mercury.
 */
int dxp_prep_for_readout(int* ioChan, int *modChan)
{
  UNUSED(ioChan);
  UNUSED(modChan);


  return DXP_SUCCESS;
}


/** @brief Unused on the Mercury.
 */
int dxp_done_with_readout(int* ioChan, int *modChan, Board *board)
{
  UNUSED(ioChan);
  UNUSED(modChan);
  UNUSED(board);


  return DXP_SUCCESS;
}


/** @brief Starts data acquisition.
 *
 */
static int dxp_begin_run(int* ioChan, int* modChan, unsigned short* gate, 
                         unsigned short* resume, Board *board, int *id)
{
  int status;

  static int gid = 0;

  UNUSED(gate);
  UNUSED(resume);
  UNUSED(board);
  UNUSED(modChan);

  
  ASSERT(ioChan != NULL);

  
  if (*resume == RESUME_RUN) {
	status = dxp__clear_csr_bit(*ioChan, DXP_CSR_RESET_MCA);
  
  } else {
	status = dxp__set_csr_bit(*ioChan, DXP_CSR_RESET_MCA);
  }

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error setting the Reset MCA bit while trying to "
			"start a run on ioChan = %d", *ioChan);
	dxp_log_error("dxp_begin_run", info_string, status);
	return status;
  }

  sprintf(info_string, "Starting a run on ioChan = %d", *ioChan);
  dxp_log_debug("dxp_begin_run", info_string);

  status = dxp__set_csr_bit(*ioChan, DXP_CSR_RUN_ENABLE);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error setting the Run Enable bit while trying to "
			"start a run on ioChan = %d", *ioChan);
	dxp_log_error("dxp_begin_run", info_string, status);
	return status;
  }

/*   status = dxp_wait_for_busy(*ioChan, *modChan, 6, 10.0, board); */

/*   if (status != DXP_SUCCESS) { */
/*     sprintf(info_string, "Error waiting for run to start on ioChan = %d", *ioChan); */
/*     dxp_log_error("dxp_begin_run", info_string, status); */
/*     return status; */
/*   } */

  *id = gid++;

  return DXP_SUCCESS;

}


/** @brief Stops data acquisition.
 *
 */
static int dxp_end_run(int* ioChan, int* modChan, Board *board)
{
  int status;

  ASSERT(modChan != NULL);
  ASSERT(ioChan != NULL);
  ASSERT(board != NULL);


  sprintf(info_string, "Ending a run on ioChan = %d", *ioChan);
  dxp_log_debug("dxp_end_run", info_string);

  status = dxp__clear_csr_bit(*ioChan, DXP_CSR_RUN_ENABLE);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error clearing Run Enable bit while trying to stop a "
			"run on ioChan = %d", *ioChan);
	dxp_log_error("dxp_end_run", info_string, status);
	return status;
  }

  status = dxp__wait_for_busy(*ioChan, *modChan, 0, 10.0, board);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error waiting for the run to end on ioChan = %d",
            *ioChan);
    dxp_log_error("dxp_end_run", info_string, status);
    return status;
  }

  return DXP_SUCCESS;    

}


/** @brief Determines if a run is active on the specified channel.
 *
 * @a active == 1 -> Run active
 *
 */
static int dxp_run_active(int* ioChan, int* modChan, int* active)
{
  int status;

  unsigned long CSR = 0x0;

  UNUSED(modChan);
  UNUSED(active);
  

  status = dxp__read_global_register(*ioChan, DXP_SYS_REG_CSR, &CSR);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error reading Control Status Register for ioChan = %d",
			*ioChan);
	dxp_log_error("dxp_run_active", info_string, status);
	return status;
  }

  if (CSR & (0x1 << DXP_CSR_RUN_ACT_BIT)) {
	*active = 1;
  
  } else {
	*active = 0;
  }


  return DXP_SUCCESS; 
}


/** @brief Starts a control task.
 *
 * Starts a control task of the specified @a type.
 *
 */
XERXES_STATIC int dxp_begin_control_task(int* ioChan, int* modChan,
                                         short *type, unsigned int *length,
                                         int *info, Board *board)
{

  int status;
  int i;

  UNUSED(length);
  UNUSED(info);


  for (i = 0; i < N_ELEMS(CONTROL_TASKS); i++) {
	if (CONTROL_TASKS[i].type == (int)*type) {

	  /* Each control task my specify an optional routine that parses
	   * the 'info' values.
	   */
	  if (CONTROL_TASKS[i].fn_info) {
		status = CONTROL_TASKS[i].fn_info(*ioChan, *modChan, *length, info, board);

		if (status != DXP_SUCCESS) {
		  sprintf(info_string, "Error processing 'info' for ioChan = %d", *ioChan);
		  dxp_log_error("dxp_begin_control_task", info_string, status);
		  return status;
		}
	  }

	  status = CONTROL_TASKS[i].fn(*ioChan, *modChan, board);

	  if (status != DXP_SUCCESS) {
		sprintf(info_string, "Error doing control task type %hd for ioChan = %d",
				*type, *ioChan);
		dxp_log_error("dxp_begin_control_task", info_string, status);
		return status;
	  }

	  return DXP_SUCCESS;
	}
  }

  sprintf(info_string, "Unknown control type %hd for ioChan = %d",
		  *type, *ioChan);
  dxp_log_error("dxp_begin_control_task", info_string, DXP_UNKNOWN_CT);
  
  return DXP_UNKNOWN_CT;

}


/** @brief End a control task.
 *
 */
static int dxp_end_control_task(int* ioChan, int* modChan, Board *board)
{
  int status;

  parameter_t RUNTYPE = MERCURY_RUNTYPE_NORMAL;

  
  ASSERT(ioChan != NULL);
  ASSERT(modChan != NULL);


  status = dxp_end_run(ioChan, modChan, board);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error stopping run for ioChan = %d", *ioChan);
	dxp_log_error("dxp_end_control_task", info_string, status);
	return status;
  }

  /* Reset the run-type to normal data acquisition. */
  status  = dxp_modify_dspsymbol(ioChan, modChan, "RUNTYPE", &RUNTYPE, board);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error setting RUNTYPE back to normal (%hu) for "
			"ioChan = %d", RUNTYPE, *ioChan);
	dxp_log_error("dxp_end_control_task", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/** @brief Unused for the Mercury.
 *
 */
static int dxp_control_task_params(int* ioChan, int* modChan, short *type, 
								   Board *board, int *info)
{
  UNUSED(ioChan);
  UNUSED(modChan);
  UNUSED(type);
  UNUSED(board);
  UNUSED(info);
	

  return DXP_SUCCESS;
}


/** @brief Get the control task data.
 *
 */
static int dxp_control_task_data(int* ioChan, int* modChan, short *type, 
                                 Board *board, void *data)
{
  int i;
  int status;

  
  ASSERT(ioChan != NULL);
  ASSERT(modChan != NULL);
  ASSERT(type != NULL);
  ASSERT(board != NULL);
  ASSERT(data != NULL);


  for (i = 0; i < N_ELEMS(CONTROL_TASK_DATA); i++) {
    if (*type == CONTROL_TASK_DATA[i].type) {
      status = CONTROL_TASK_DATA[i].fn(*ioChan, *modChan, data, board);

      if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error running data control task %hd for "
                "ioChan = %d", *type, *ioChan);
        dxp_log_error("dxp_control_task_data", info_string, status);
        return status;
      }

      return DXP_SUCCESS;
    }
  }

  sprintf(info_string, "Unknown data control task type %hd for ioChan = %d",
          *type, *ioChan);
  dxp_log_error("dxp_control_task_data", info_string, DXP_UNKNOWN_CT);
  return DXP_UNKNOWN_CT;
}


/** @brief Unused for the Mercury.
 *
 */
static int dxp_decode_error(int* ioChan, int* modChan, Dsp_Info* dsp, 
							unsigned short* runerror, unsigned short* errinfo)
{
  UNUSED(ioChan);
  UNUSED(modChan);
  UNUSED(dsp);

  *runerror = 0;
  *errinfo  = 0;

  return DXP_SUCCESS;
}


/** @brief Unused for the Mercury.
 *
 */
static int dxp_clear_error(int* ioChan, int* modChan, Board *board)
{
  UNUSED(ioChan);
  UNUSED(modChan);
  UNUSED(board);


  return DXP_SUCCESS;
}


/** @brief Get the statistics for the specified channel.
 *
 * Returns the 32-bit unsigned statistics values to the caller. To get
 * the full 64-bits worth of values, see dxp_get_runstats_ex().
 *
 * If the value is > 0xFFFFFFFF then 0xFFFFFFFF is returned.
 *
 * Always returns 0 for the number of baseline events.
 */
static int dxp_get_runstats(int *ioChan, int *modChan, Board *b, 
							unsigned long *evts, unsigned long *under, 
							unsigned long *over, unsigned long *fast, 
							unsigned long *base, double *live,
							double *icr, double *ocr)
{
  int status;

  double mca_evts_ex = 0.0;
  double under_ex    = 0.0;
  double over_ex     = 0.0;
  double fast_ex     = 0.0;
  double real        = 0.0;
  double total_evts  = 0.0;
  double tick        = dxp__get_clock_tick();

  unsigned long addr          = DXP_DSP_EXT_MEM_ADDR;
  unsigned long mca_evts_addr = 0;
  unsigned long under_addr    = 0;
  unsigned long over_addr     = 0;
  unsigned long live_addr     = 0;
  unsigned long triggers_addr = 0;
  unsigned long real_addr     = 0;

  unsigned long* buf = NULL;
  unsigned long buffer_size   = MERCURY_MEMORY_BLOCK_SIZE;

  UNUSED(ocr);
  UNUSED(b);


  ASSERT(modChan != NULL);

  /* The MD layer expects an array of 16-bit words. */
  buf = mercury_md_alloc(buffer_size * sizeof(unsigned long));
  
  status = dxp__read_block(ioChan, addr, buffer_size , buf);

  if (status != DXP_SUCCESS) {
  mercury_md_free(buf);  
	sprintf(info_string, "Error reading statistics block for ioChan = %d",
			*ioChan);
	dxp_log_error("dxp_get_runstats", info_string, status);
	return status;
  }

  *base = 0;

  mca_evts_addr = MERCURY_STATS_CHAN_OFFSET[*modChan] + MERCURY_STATS_MCAEVENTS_OFFSET;
  mca_evts_ex = dxp__unsigned64_to_double(buf + mca_evts_addr);

  if (mca_evts_ex >= 4294967296.0) {
	mca_evts_ex = 4294967295.0;
  }

  *evts = (unsigned long)mca_evts_ex;

  under_addr = MERCURY_STATS_CHAN_OFFSET[*modChan] + MERCURY_STATS_UNDERFLOWS_OFFSET;
  under_ex = dxp__unsigned64_to_double(buf + under_addr);

  if (under_ex >= 4294967296.0) {
	under_ex = 4294967295.0;
  }

  *under = (unsigned long)under_ex;

  over_addr = MERCURY_STATS_CHAN_OFFSET[*modChan] + MERCURY_STATS_OVERFLOWS_OFFSET;
  over_ex = dxp__unsigned64_to_double(buf + over_addr);

  if (over_ex >= 4294967296.0) {
	over_ex = 4294967295.0;
  }

  *over = (unsigned long)over_ex;

  live_addr = MERCURY_STATS_CHAN_OFFSET[*modChan] + MERCURY_STATS_TLIVETIME_OFFSET;
  *live = dxp__unsigned64_to_double(buf + live_addr) * tick * 16.0;

  triggers_addr = MERCURY_STATS_CHAN_OFFSET[*modChan] + MERCURY_STATS_TRIGGERS_OFFSET;
  fast_ex = dxp__unsigned64_to_double(buf + triggers_addr);

  if (*live > 0.0) {
	*icr = fast_ex / *live;
  } else {
	*icr = 0.0;
  }

  if (fast_ex >= 4294967296.0) {
	fast_ex = 4294967295.0;
  }

  *fast = (unsigned long)fast_ex;

  real_addr = MERCURY_STATS_CHAN_OFFSET[*modChan] + MERCURY_STATS_REALTIME_OFFSET;
  real = dxp__unsigned64_to_double(buf + real_addr) * tick * 16.0;

  total_evts = mca_evts_ex + under_ex + over_ex;

  if (real > 0.0) {
	*ocr = total_evts / real;
  } else {
	*ocr = 0.0;
  }

  mercury_md_free(buf);
  return DXP_SUCCESS;
}


/** @brief Unused for the Mercury.
 *
 */
static int dxp_setup_cmd(Board *board, char *name, unsigned int *lenS,
                         byte_t *send, unsigned int *lenR, byte_t *receive,
                         byte_t ioFlags)
{
  UNUSED(board);
  UNUSED(name);
  UNUSED(lenS);
  UNUSED(send);
  UNUSED(lenR);
  UNUSED(receive);
  UNUSED(ioFlags);


  return DXP_SUCCESS;
}


/** @brief Read the specified memory from the requested location.
 *  burst mode in USB is just the same as read_block.
 */
XERXES_STATIC int dxp_read_mem(int *ioChan, int *modChan, Board *board,
                               char *name, unsigned long *base,
                               unsigned long *offset, unsigned long *data)
{
  int status;
  unsigned long addr = *base;

  UNUSED(board);
  UNUSED(name);
  UNUSED(modChan);

  
  /* XXX: Convert this routine to use a memory_accessor_t table. */
  if (STREQ(name, "burst")) {

    addr += DXP_DSP_EXT_MEM_ADDR;
    status = dxp__read_block(ioChan, addr, *offset, data);

    if (status != DXP_SUCCESS) {
      sprintf(info_string, "Error reading external memory block for ioChan = %d",
              *ioChan);
      dxp_log_error("dxp_read_mem", info_string, status);
      return status;
    }
    
  } else if (STREQ(name, "burst_map")) {
  
    status = dxp__read_block(ioChan, addr, *offset, data);

    if (status != DXP_SUCCESS) {
      sprintf(info_string, "Error reading memory buffer for ioChan = %d",
              *ioChan);
      dxp_log_error("dxp_read_mem", info_string, status);
      return status;
    }  
  
  } else if (STREQ(name, "data")) {

    addr += DXP_DSP_DATA_MEM_ADDR;
    status = dxp__read_block(ioChan, addr, *offset, data);

    if (status != DXP_SUCCESS) {
      sprintf(info_string, "Error reading data memory block for ioChan = %d",
              *ioChan);
      dxp_log_error("dxp_read_mem", info_string, status);
      return status;
    }
    
  } else if (STREQ(name, "eeprom")) {

    /* no offset: used in reading out serial */
    status = dxp__read_block(ioChan, addr, *offset, data);

    if (status != DXP_SUCCESS) {
      sprintf(info_string, "Error reading data memory block for ioChan = %d",
              *ioChan);
      dxp_log_error("dxp_read_mem", info_string, status);
      return status;
    }
  
  } else {

    sprintf(info_string, "The requested memory type '%s' is not implemented for "
            "ioChan = %d", name, *ioChan);
    dxp_log_error("dxp_read_mem", info_string, DXP_UNIMPLEMENTED);
    return DXP_UNIMPLEMENTED;
    
  }

 
  return DXP_SUCCESS;
}


/** @brief Writes the specified memory to the requested address.
 *
 */
XERXES_STATIC int dxp_write_mem(int *ioChan, int *modChan, Board *board,
                                char *name, unsigned long *base,
                                unsigned long *offset, unsigned long *data)
{
  int status;
  unsigned long addr = *base;

  UNUSED(board);
  UNUSED(name);
  UNUSED(modChan);

  ASSERT(data   != NULL);
  ASSERT(name   != NULL);
  ASSERT(base   != NULL);
  ASSERT(offset != NULL);
  
  
  /* XXX: Convert this routine to use a memory_accessor_t table. */
  if (STREQ(name, "burst")) {

    addr += DXP_DSP_EXT_MEM_ADDR;
    status = dxp__write_block(ioChan, addr, *offset, data);

    if (status != DXP_SUCCESS) {
      sprintf(info_string, "Error writing external memory block for ioChan = %d",
              *ioChan);
      dxp_log_error("dxp_write_mem", info_string, status);
      return status;
    }
  
  } else if (STREQ(name, "data")) {

    addr += DXP_DSP_DATA_MEM_ADDR;
    status = dxp__write_block(ioChan, addr, *offset, data);

    if (status != DXP_SUCCESS) {
      sprintf(info_string, "Error writing data memory block for ioChan = %d",
              *ioChan);
      dxp_log_error("dxp_write_mem", info_string, status);
      return status;
    }
  } else if (STREQ(name, "eeprom")) {

    /* no offset for writing serial number */
    status = dxp__write_block(ioChan, addr, *offset, data);

    if (status != DXP_SUCCESS) {
      sprintf(info_string, "Error writing data memory block for ioChan = %d",
              *ioChan);
      dxp_log_error("dxp_write_mem", info_string, status);
      return status;
    }
    
  } else {

    sprintf(info_string, "The requested memory type '%s' is not implemented for "
            "ioChan = %d", name, *ioChan);
    dxp_log_error("dxp_write_mem", info_string, DXP_UNIMPLEMENTED);
    return DXP_UNIMPLEMENTED;
  }

 
  return DXP_SUCCESS;
}


/** @brief Writes the specified data to the specified register on the
 * hardware.
 */
static int dxp_write_reg(int *ioChan, int *modChan, char *name,
                         unsigned long *data)
{
  int i;
  int status;

  UNUSED(ioChan);
  UNUSED(modChan);
  UNUSED(name);
  UNUSED(data);
  
  for (i = 0; i < N_ELEMS(REGISTER_TABLE); i++) {
    if (STREQ(name, REGISTER_TABLE[i].name)) {

      status = dxp__write_global_register(*ioChan, REGISTER_TABLE[i].addr, 
                                          *data);

      if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error writing '%s' register for ioChan = %d",
                name, *ioChan);
        dxp_log_error("dxp_write_reg", info_string, status);
        return status;
      }

      return DXP_SUCCESS;
    }
  }

  sprintf(info_string, "Unknown register '%s' for ioChan = %d", name, *ioChan);
  dxp_log_error("dxp_write_reg", info_string, DXP_UNKNOWN_REG);
  return DXP_UNKNOWN_REG;
}


/** @brief Write the specified value to the specified register on the hardware.
 *
 * This routine is basically a wrapper around the low-level I/O routine.
 */
static int dxp__write_global_register(int ioChan, unsigned long reg,
											unsigned long val)
{
  int status;
 
  status = dxp__write_word(&ioChan, reg, val);
  
  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error writing %#lx to address %#lx for channel %d",
			val, reg, ioChan);
	dxp_log_error("dxp__write_global_register", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/** @brief Reads the specified register.
 *
 */
XERXES_STATIC int dxp_read_reg(int *ioChan, int *modChan, char *name,
                               unsigned long *data)
{
  int i;
  int status;

  UNUSED(modChan);


  ASSERT(ioChan != NULL);
  ASSERT(name   != NULL);
  ASSERT(data   != NULL);

  
  for (i = 0; i < N_ELEMS(REGISTER_TABLE); i++) {
  	if (STREQ(name, REGISTER_TABLE[i].name)) {

  	  status = dxp__read_global_register(*ioChan, REGISTER_TABLE[i].addr, data);

  	  if (status != DXP_SUCCESS) {
  		sprintf(info_string, "Error reading '%s' register for ioChan = %d",
  				name, *ioChan);
  		dxp_log_error("dxp_read_reg", info_string, status);
  		return status;
  	  }

  	  return DXP_SUCCESS;
  	}
  }

  sprintf(info_string, "Unknown register '%s' for ioChan = %d", name, *ioChan);
  dxp_log_error("dxp_read_reg", info_string, DXP_UNKNOWN_REG);
  return DXP_UNKNOWN_REG;

}


/** @brief Unused for the Mercury.
 *
 */
XERXES_STATIC int dxp_do_cmd(int *ioChan, byte_t cmd, unsigned int lenS,
							 byte_t *send, unsigned int lenR, byte_t *receive)
{
  UNUSED(ioChan);
  UNUSED(cmd);
  UNUSED(lenS);
  UNUSED(send);
  UNUSED(lenR);
  UNUSED(receive);
    

  return DXP_SUCCESS;
}


/** @brief Cleans up the communication interface and releases any resources
 * that may have been acquired when the connection was opened.
 *
 */
XERXES_STATIC int dxp_unhook(Board *b)
{
  int status;

  
  ASSERT(b != NULL);
    

  status = b->iface->funcs->dxp_md_close(&(b->ioChan));

  /* Ignore status? */

  return DXP_SUCCESS;
}


/** @brief Get the length of the SCA data buffer
 *
 */
XERXES_STATIC unsigned int dxp_get_sca_length(Dsp_Info *dsp,
                                              unsigned short *params)
{
  UNUSED(dsp);
  UNUSED(params);


  return DXP_SUCCESS;
}


/** @brief Read out the SCA data buffer from the board
 *
 */
XERXES_STATIC int dxp_read_sca(int *ioChan, int *modChan, Board* board,
							   unsigned long *sca)
{
  int status;

  double NUMSCA = 0;

  unsigned long sca_addr = 0;


  ASSERT(board != NULL);
  ASSERT(ioChan != NULL);
  ASSERT(modChan != NULL);


  status = dxp__get_sca_addr(*ioChan, *modChan, board, &sca_addr);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error getting SCA data array address for ioChan = %d",
			*ioChan);
	dxp_log_error("dxp_read_sca", info_string, status);
	return status;
  }

  sprintf(info_string, "SCA data array addr = %#lx", sca_addr);
  dxp_log_debug("dxp_read_sca", info_string);

  status = dxp_read_dspsymbol(ioChan, modChan, "NUMSCA", board, &NUMSCA);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error getting number of SCAs for ioChan = %d", *ioChan);
	dxp_log_error("dxp_read_sca", info_string, status);
	return status;
  }

  if (NUMSCA == 0.0) {
	sprintf(info_string, "No SCAs defined for ioChan = %d", *ioChan);
	dxp_log_error("dxp_read_sca", info_string, DXP_NO_SCA);
	return DXP_NO_SCA;
  }

  status = dxp__read_block(ioChan, sca_addr, (unsigned long)NUMSCA * 2, sca);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error reading SCA data array for ioChan = %d", *ioChan);
	dxp_log_error("dxp_read_sca", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/** @brief Returns the name of the symbol located at the specified index.
 */
static int dxp_get_symbol_by_index(int modChan, unsigned short index, Board *b,
                                   char *name)
{
  UNUSED(modChan);


  ASSERT(name  != NULL);
  ASSERT(b != NULL);
  ASSERT(index < (PARAMS(b)->nsymbol +
				  PARAMS(b)->n_per_chan_symbols));

  /* Determine if the index represents a global or per-channel parameter. */
  if (index < PARAMS(b)->nsymbol) {
	strncpy(name, PARAMS(b)->parameters[index].pname, MAX_DSP_PARAM_NAME_LEN);
  
  } else {
	strncpy(name,
			PARAMS(b)->per_chan_parameters[index - PARAMS(b)->nsymbol].pname,
			MAX_DSP_PARAM_NAME_LEN);
  }

  return DXP_SUCCESS;

}


/** @brief Calculates the total number of parameters.
 */
static int dxp_get_num_params(int modChan, Board *b, unsigned short *n_params)
{
  UNUSED(modChan);

  ASSERT(b != NULL);
  ASSERT(n_params != NULL);


  *n_params = (unsigned short)(PARAMS(b)->nsymbol +
							   PARAMS(b)->n_per_chan_symbols);

  return DXP_SUCCESS;
}


/**
 * @brief Legacy routine to open a file checking several locations.
 */
static FILE *dxp_find_file(const char* filename, const char* mode)
{
	FILE *fp=NULL;
	char *name=NULL, *name2=NULL;
	char *home=NULL;

/* Try to open file directly */
	if((fp=xia_file_open(filename,mode))!=NULL){
		return fp;
	}
/* Try to open the file with the path XIAHOME */
	if ((home=getenv("XIAHOME"))!=NULL) {
		name = (char *) mercury_md_alloc(sizeof(char)*
							(strlen(home)+strlen(filename)+2));
		sprintf(name, "%s/%s", home, filename);
		if((fp=xia_file_open(name,mode))!=NULL){
			mercury_md_free(name);
			return fp;
		}
		mercury_md_free(name);
		name = NULL;
	}
/* Try to open the file with the path DXPHOME */
	if ((home=getenv("DXPHOME"))!=NULL) {
		name = (char *) mercury_md_alloc(sizeof(char)*
							(strlen(home)+strlen(filename)+2));
		sprintf(name, "%s/%s", home, filename);
		if((fp=xia_file_open(name,mode))!=NULL){
			mercury_md_free(name);
			return fp;
		}
		mercury_md_free(name);
		name = NULL;
	}
/* Try to open the file as an environment variable */
	if ((name=getenv(filename))!=NULL) {
		if((fp=xia_file_open(name,mode))!=NULL){
			return fp;
		}
		name = NULL;
	}
/* Try to open the file with the path XIAHOME and pointing 
 * to a file as an environment variable */
	if ((home=getenv("XIAHOME"))!=NULL) {
		if ((name2=getenv(filename))!=NULL) {
		
			name = (char *) mercury_md_alloc(sizeof(char)*
								(strlen(home)+strlen(name2)+2));
			sprintf(name, "%s/%s", home, name2);
			if((fp=xia_file_open(name,mode))!=NULL){
				mercury_md_free(name);
				return fp;
			}
			mercury_md_free(name);
			name = NULL;
		}
	}
/* Try to open the file with the path DXPHOME and pointing 
 * to a file as an environment variable */
	if ((home=getenv("DXPHOME"))!=NULL) {
		if ((name2=getenv(filename))!=NULL) {
		
			name = (char *) mercury_md_alloc(sizeof(char)*
								(strlen(home)+strlen(name2)+2));
			sprintf(name, "%s/%s", home, name2);
			if((fp=xia_file_open(name,mode))!=NULL) {
				mercury_md_free(name);
				name = NULL;
				return fp;
			}
			mercury_md_free(name);
			name = NULL;
		}
	}

	return NULL;
}


/**
 * @brief Writes a single 32-bit value to the device.
 */
static int dxp__write_word(int *ioChan, unsigned long addr, unsigned long val)
{
  int status;

  unsigned int f;
  unsigned int len;

  unsigned long a;

  /* The 32-bit transfer needs to be split into 2 16-bit words for the
   * Mercury.
   */
  unsigned short buf[2];


  /* Write address to cache. */
  a   = DXP_A_ADDR;
  f   = DXP_F_IGNORE;
  len = 0;
  status = mercury_md_io(ioChan, &f, &a, &addr, &len);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error setting write address to %#lx for ioChan = %d",
            addr, *ioChan);
    dxp_log_error("dxp__write_word", info_string, status);
    return status;
  }

  a   = DXP_A_IO;
  f   = DXP_F_WRITE;
  len = 2;
  buf[0] = (unsigned short)(val & 0xFFFF);
  buf[1] = (unsigned short)((val >> 16) & 0xFFFF);

  status = mercury_md_io(ioChan, &f, &a, buf, &len);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error writing %#lx to %#lx for ioChan = %d", val,
            addr, *ioChan);
    dxp_log_error("dxp__write_word", info_string, status);
    return status;
  }

  /* DEBUG Print out all USB read and write addresses when in doubt
  sprintf(info_string, "Wrote word %#X to address %#X", val, addr);
  dxp_log_debug("dxp__write_word", info_string); */

  return DXP_SUCCESS;
}


/**
 * @brief Write the specified block of data to the requested address.
 */
static int dxp__write_block(int *ioChan, unsigned long addr, unsigned long n,
                            unsigned long *data)
{
  int status;

  unsigned int f;
  unsigned int len;

  unsigned long a;
  unsigned long i;

  unsigned short *buf = NULL;


  /* Write address to cache. */
  a   = DXP_A_ADDR;
  f   = DXP_F_IGNORE;
  len = 0;
  status = mercury_md_io(ioChan, &f, &a, &addr, &len);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error setting write address to %#lx for ioChan = %d",
            addr, *ioChan);
    dxp_log_error("dxp__write_block", info_string, status);
    return status;
  }

  a   = DXP_A_IO;
  f   = DXP_F_WRITE;
  len = (unsigned int)(n * 2);
  
  /* The MD layer expects an array of unsigned shorts. */
  buf = mercury_md_alloc(len * sizeof(unsigned short));

  if (buf == NULL) {
    sprintf(info_string, "Unable to allocate %d bytes for 'buf'",
            len * sizeof(unsigned short));
    dxp_log_error("dxp__write_block", info_string, status);
    return status;
  }
  
  for (i = 0; i < n; i++) {
    buf[i * 2]       = (unsigned short)LO_WORD(data[i]);
    buf[(i * 2) + 1] = (unsigned short)HI_WORD(data[i]);
  }

  status = mercury_md_io(ioChan, &f, &a, buf, &len);

  mercury_md_free(buf);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error writing %u words to %#lx for ioChan = %d", len,
            addr, *ioChan);
    dxp_log_error("dxp__write_block", info_string, status);
    return status;
  }
  
  /* DEBUG Print out all USB read and write addresses when in doubt
  sprintf(info_string, "Wrote block of %u words from address %#X", n, addr);
  dxp_log_debug("dxp__write_block", info_string); */

  return DXP_SUCCESS;
}


/**
 * @brief Read a single 32-bit word from the specified address.
 */
static int dxp__read_word(int *ioChan, unsigned long addr,
                          unsigned long *val)
{
  int status;

  unsigned int f;
  unsigned int len;

  unsigned long a;

  unsigned short buf[2];

  /* Write address to cache. */
  a   = DXP_A_ADDR;
  f   = DXP_F_IGNORE;
  len = 0;
  status = mercury_md_io(ioChan, &f, &a, &addr, &len);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error setting write address to %#lx for ioChan = %d",
            addr, *ioChan);
    dxp_log_error("dxp__read_word", info_string, status);
    return status;
  }

  a   = DXP_A_IO;
  f   = DXP_F_READ;
  len = 2;
  status = mercury_md_io(ioChan, &f, &a, buf, &len);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading from  %#lx for ioChan = %d", addr,
            *ioChan);
    dxp_log_error("dxp__read_word", info_string, status);
    return status;
  }

  *val = WORD_TO_LONG(buf[0], buf[1]);

  /* DEBUG Print out all USB read and write addresses when in doubt
  sprintf(info_string, "Read word %#X from address %#X", *val, addr);
  dxp_log_debug("dxp__read_word", info_string); */

  return DXP_SUCCESS;
}

 
/**
 * @brief Download a System FPGA to the board.
 *
 */
static int dxp__download_system_fpga(int ioChan, int modChan, Board *b)
{
  int status;

  parameter_t BUSY     = 0x23;
  parameter_t RUNERROR = 0xFFFF;
  
  ASSERT(b != NULL);
  ASSERT(b->system_fpga != NULL);


  status = dxp__reset_dsp(ioChan);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error resetting the DSP for ioChan = %d", ioChan);
    dxp_log_error("dxp__download_dspconfig", info_string, status);
    return status;
  }

  status = dxp__download_fpga(ioChan, DXP_CPLD_CTRL_SYS_FPGA, b->system_fpga);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error downloading System FPGA for ioChan = %d", ioChan);
    dxp_log_error("dxp__download_system_fpga", info_string, status);
    return status;
  }

  /* This reset call stops the LEDs on the hardware from flashing due to 
   * the System FPGA download.
   */

   status = dxp__reset_dsp(ioChan);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error clearing LEDs for ioChan = %d", ioChan);
    dxp_log_error("dxp__download_system_fpga", info_string, status);
    return status;
  }

  /* Setting RUNERROR to -1 tells the DSP to leave the exisiting parameters
   * alone. If we didn't do this, it would set all of the parameters to the
   * default values upon boot.
   */
  
  status = dxp_modify_dspsymbol(&ioChan, &modChan, "RUNERROR", &RUNERROR, b);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error disabling parameter update for ioChan = %d",
            ioChan);
    dxp_log_error("dxp__download_system_fpga", info_string, status);
    return status;
  }

  status = dxp_modify_dspsymbol(&ioChan, &modChan, "BUSY", &BUSY, b);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error setting BUSY for ioChan = %d",
            ioChan);
    dxp_log_error("dxp__download_system_fpga", info_string, status);
    return status;
  }
 

  status = dxp__boot_dsp(ioChan, modChan, b);

  /*if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error booting DSP, re-trying", ioChan);
    dxp_log_debug("dxp__download_system_fpga", info_string);
    status = dxp__boot_dsp(ioChan, modChan, b);
  }*/
  
  if (status != DXP_SUCCESS) {
    dxp__reset_dsp(ioChan);
    sprintf(info_string, "Error booting DSP  for ioChan = %d", ioChan);
    dxp_log_error("dxp__download_system_fpga", info_string, status);
    return status;
  }

  return DXP_SUCCESS;
  
}


/** @brief Downloads an FPGA to the selected target on the hardware.
 *
 * All FPGAs are downloaded to the hardware using the same procedure
 *
 * 1) Write the Control Register with the target FPGA encoded.
 *
 * 2) Read Status Register until the appropriate *INIT line is asserted. This
 * operation can timeout after 1 millisecond.
 *
 * 3) Write FPGA configuration data to the Data Register.
 *
 * 4) Read Status Register until the appropriate XDONE line is asserted. This
 * operation cam timeout after 1 second.
 *
 * The mercury has several targetable FPGAs. The selected FPGA is selected by 
 * setting the appropriate bits in the @a target parameter. The constants
 * for allowed targets are defined in xia_xmap.h. This routine is smart enough
 * to account for a target that is an OR'd combination of multiple targets.
 *
 */
static int dxp__download_fpga(int ioChan, unsigned long target,
                              Fippi_Info *fpga)
{
  int status;

  unsigned int i;
  unsigned long j;

  unsigned long *cfg_data = NULL;

  float cpld_ctrl_wait = 0.001f;
  float sys_done_wait  = 1.0f;

  unsigned long cpld_status = 0;

  ASSERT(fpga != NULL);

  status = dxp__write_global_register(ioChan, DXP_CPLD_CFG_CTRL, target);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error writing target '%#lx' to Control Register for "
			"ioChan = %d", target, ioChan);
	dxp_log_error("dxp__download_fpga", info_string, status);
	return status;
  }

  /* Since the sleep granularity is typically 1 millisecond, there is no need
   * to try and create a timeout loop. We will simply wait 1 millisecond and
   * check the result.
   */
  mercury_md_wait(&cpld_ctrl_wait);

  status = dxp__read_global_register(ioChan, DXP_CPLD_CFG_STATUS, &cpld_status);

  if (status != DXP_SUCCESS) {
  	sprintf(info_string, "Error reading Status Register for channel %d", ioChan);
  	dxp_log_error("dxp__download_fpga", info_string, status);
  	return status;
  }

  /* Since the target could potentially be more then one FPGA, we must check
   * the INIT* line for each FPGA that is targeted.
   */
  for (j = 0; j < MERCURY_NUM_TARGETS; j++) {
	if (target & (1 << j)) {
	  if (! (cpld_status & MERCURY_CFG_STATUS[j][MERCURY_INIT])) {
		sprintf(info_string, "INIT* line never asserted for Target %lu after "
				"waiting %f seconds", j, cpld_ctrl_wait);
		dxp_log_error("dxp__download_fpga", info_string, DXP_FPGA_TIMEOUT);
		return DXP_FPGA_TIMEOUT;
	  }
	}
  }

  sprintf(info_string, "FPGA proglen = %u", fpga->proglen);
  dxp_log_debug("dxp__download_fpga", info_string);

  
  /* The FPGA configuration data is packed into a 16-bit wide
   * array in the Fippi_Info structure. We need to unpack it into a
   * 32-bit wide array that only uses a single data byte. In other words,
   * each byte in the 16-bit array will be unpacked into its own 32-bit
   * array entry.
   */  
  cfg_data = mercury_md_alloc(fpga->proglen * 2 * sizeof(unsigned long));

  if (cfg_data == NULL) {
    sprintf(info_string, "Unable to allocate %d bytes for 'cfg_data'",
            fpga->proglen * 2 * sizeof(unsigned long));
    dxp_log_error("dxp__download_fpga", info_string, DXP_NOMEM);
    return DXP_NOMEM;
  }

  for (i = 0; i < fpga->proglen; i++) {
    cfg_data[i * 2]       = (unsigned long)LO_BYTE(fpga->data[i]);
    cfg_data[(i * 2) + 1] = (unsigned long)HI_BYTE(fpga->data[i]);
  }

  status = dxp__write_block(&ioChan, DXP_CPLD_CFG_DATA, fpga->proglen * 2,
                            cfg_data);

  mercury_md_free(cfg_data);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error writing System FPGA data for ioChan = %d",
            ioChan);
    dxp_log_error("dxp__download_fpga", info_string, status);
    return status;
  }

  mercury_md_wait(&sys_done_wait);

  status = dxp__read_global_register(ioChan, DXP_CPLD_CFG_STATUS, &cpld_status);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error reading Status Register for channel %d", ioChan);
	dxp_log_error("dxp_download_fpga", info_string, status);
	return status;
  }

  /* See the comment above (where INIT* is checked). */
  for (j = 0; j < MERCURY_NUM_TARGETS; j++) {
  	if (target & (1 << j)) {
  	  if (! (cpld_status & MERCURY_CFG_STATUS[j][MERCURY_XDONE])) {
  		sprintf(info_string, "XDONE line never asserted for Target %lu after "
  				"waiting %f seconds", j, sys_done_wait);
  		dxp_log_error("dxp__download_fpga", info_string, DXP_FPGA_TIMEOUT);
  		return DXP_FPGA_TIMEOUT;
  	  }
  	}
  }

  return DXP_SUCCESS;
}


/**
 * @brief Downloads the System FPGA and FiPPI A to the hardware.
 */
static int dxp__download_all_fpgas(int ioChan, int modChan, Board *b)
{
  int status;


  ASSERT(b != NULL);

  status = dxp__download_fpga(ioChan, DXP_CPLD_CTRL_SYS_FPGA, b->system_fpga);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error downloading System FPGA for ioChan = %d",
            ioChan);
    dxp_log_error("dxp__download_all_fpgas", info_string, status);
    return status;
  }

  status = dxp__download_fippi(ioChan, modChan, b);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error downloading FiPPI for ioChan = %d",
            ioChan);
    dxp_log_error("dxp__download_all_fpgas", info_string, status);
    return status;
  }

  return DXP_SUCCESS;
}


/**
 * @brief Download the FiPPI to the board.
 *
 * The FiPPI is downloaded the same as the System FPGA. See
 * dxp__download_system_fpga() for more details.
 */
static int dxp__download_fippi(int ioChan, int modChan, Board *b)
{
  int status;

  ASSERT(b != NULL);
  ASSERT(b->fippi_a != NULL);

  status = dxp__download_fippi_dsp_no_wake(ioChan, modChan, b);
  
  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error downloading FiPPI for ioChan = %d",
            ioChan);
    dxp_log_error("dxp__download_fippi", info_string, status);
    return status;
  }

  status = dxp__wake_dsp_up(ioChan, modChan, b);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error waking DSP up for ioChan = %d", ioChan);
	dxp_log_error("dxp__download_fippi", info_string, status);
	return status;
  }  
  
  return DXP_SUCCESS;
}


/**
 * @brief Download the FiPPI to the board.
 *
 * doesn't wake the DSP up at the end of the download process. This
 * feature is needed when switching between RC and Reset firmware.
 */
static int dxp__download_fippi_dsp_no_wake(int ioChan, int modChan, Board *b)
{
  int status;

  ASSERT(b != NULL);
  ASSERT(b->fippi_a != NULL);

  status = dxp__put_dsp_to_sleep(ioChan, modChan, b);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error putting DSP to sleep for ioChan = %d", ioChan);
	dxp_log_error("dxp_download_fippis", info_string, status);
	return status;
  }

  status = dxp__download_fpga(ioChan, DXP_CPLD_CTRL_SYS_FIP, b->fippi_a);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error downloading FiPPI A and FiPPI B for "
            "ioChan = %d", ioChan);
    dxp_log_error("dxp_download_fippis", info_string, status);
    return status;
  }

  return DXP_SUCCESS;
}


/**
 * @brief Put the DSP into a reset state, usually so we can download
 * new firmware to it.
 */
static int dxp__reset_dsp(int ioChan)
{
  int status;

  float wait = .001f;

  
  sprintf(info_string, "Performing DSP reset for ioChan = %d", ioChan);
  dxp_log_debug("dxp__reset_dsp", info_string);

  status = dxp__set_csr_bit(ioChan, DXP_CSR_RESET_DSP_BIT);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error setting bit %d in the CSR to reset the DSP for "
            "ioChan = %d", DXP_CSR_RESET_DSP_BIT, ioChan);
    dxp_log_error("dxp__reset_dsp", info_string, status);
    return status;
  }

  /* The hardware needs a millisecond to finish configuring */
  mercury_md_wait(&wait);

  return DXP_SUCCESS;
}


/** @brief Read 32-bits from the specified register.
 *
 */
static int dxp__read_global_register(int ioChan, unsigned long reg,
										   unsigned long *val)
{
  int status;

  unsigned long csr;
 
  ASSERT(val != NULL);

  status = dxp__read_word(&ioChan, reg, &csr);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading register from the System FPGA ioChan = %d",
            ioChan);
    dxp_log_error("dxp__read_global_register", info_string, status);
    return status;
  }

  *val = csr;
  return DXP_SUCCESS;
}


/**
 * @brief Sets a bit in the CSR using the read/modify/write style.
 */
static int dxp__set_csr_bit(int ioChan, byte_t bit)
{
  int status;

  unsigned long csr;


  ASSERT(bit < 32);


  status = dxp__read_word(&ioChan, DXP_SYS_REG_CSR, &csr);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading CSR from the System FPGA ioChan = %d",
            ioChan);
    dxp_log_error("dxp__set_csr_bit", info_string, status);
    return status;
  }

  csr |= (1 << bit);

  status = dxp__write_word(&ioChan, DXP_SYS_REG_CSR, csr);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error writing modified CSR (%#lx), bit = %#hx, to "
            "the System FPGA for ioChan = %d", csr, (unsigned short)bit,
            ioChan);
    dxp_log_error("dxp__set_csr_bit", info_string, status);
    return status;
  }

  return DXP_SUCCESS;
}



/** @brief Clear the specified bit in the Control Status Register.
 *
 * Uses the read/modify/write idiom to clear the selected bit in the Control
 * Status Register.
 */
static int dxp__clear_csr_bit(int ioChan, byte_t bit)
{
  int status;

  unsigned long val;


  if (bit > 31) {
	sprintf(info_string, "Specified bit '%hu' is larger then the maximum bit "
          "'31' in the CSR", (unsigned short)bit);
	dxp_log_error("dxp__clear_csr_bit", info_string, DXP_BAD_BIT);
	return DXP_BAD_BIT;
  }

  status = dxp__read_word(&ioChan, DXP_SYS_REG_CSR, &val);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error reading CSR for ioChan = %d", ioChan);
	dxp_log_error("dxp_clear_csr_bit", info_string, status);
	return status;
  }

  val &= ~(0x1 << bit);

  sprintf(info_string, "Setting CSR to %#lx", val);
  dxp_log_debug("dxp__clear_csr_bit", info_string);

  status = dxp__write_word(&ioChan, DXP_SYS_REG_CSR, val);
  
  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error writing %#lx to the CSR for ioChan = %d", val,
			ioChan);
	dxp_log_error("dxp__clear_csr_bit", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/**
 * @brief Instructs the hardware to boot the DSP code
 *
 * This routine should be called after the DSP code words have been downloaded
 * to the hardware.
 *
 */
static int dxp__boot_dsp(int ioChan, int modChan, Board *b)
{
  int status;


  ASSERT(b != NULL);


  sprintf(info_string, "Performing DSP boot for ioChan = %d", ioChan);
  dxp_log_debug("dxp__boot_dsp", info_string);

  status = dxp__set_csr_bit(ioChan, DXP_CSR_BOOT_DSP_BIT);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error setting bit %d in the CSR to boot the DSP",
            DXP_CSR_BOOT_DSP_BIT);
    dxp_log_error("dxp__boot_dsp", info_string, status);
    return status;
  }

  /* Check DSP_Active before waiting for busy
   * to avoid errors in loading the dsp code which prevents
   * both ACTIVE and BUSY to be set */
  status = dxp__wait_for_active(ioChan, modChan, 1.0, b);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error waiting for DSP Active to be set ioChan = %d",
            ioChan);
    dxp_log_error("dxp__boot_dsp", info_string, status);
    return status;
  }
  
  status = dxp__wait_for_busy(ioChan, modChan, 0, 1.0, b);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error waiting for DSP to boot for ioChan = %d",
            ioChan);
    dxp_log_error("dxp__boot_dsp", info_string, status);
    return status;
  }

  return DXP_SUCCESS;
}


/** @brief Wait for the dxp active line to be set.
 *
 *  If the DSP startup completes successfully
 *  the DXP_CSR_DSP_ACT_BIT bit is set just before BUSY is set to zero.
 *  It's a more reliable way to assert that DSP startup is succesful
 */
static int dxp__wait_for_active(int ioChan, int modChan, double timeout,
                                Board *board)
{

  int status;
  int n_polls = 0;
  int i;

  float wait = .01f;
  unsigned long csr = 0;

  UNUSED(modChan);
  UNUSED(board);

  
  n_polls = (int)ROUND(timeout / wait);

  for (i = 0; i < n_polls; i++) {

    status = dxp__read_global_register(ioChan, DXP_SYS_REG_CSR, &csr);

    if (status != DXP_SUCCESS) {
      sprintf(info_string, "Error reading CSR for ioChan = %d", ioChan);
      dxp_log_error("dxp_wait_for_active", info_string, status);
      return status;
    }

    /* Stop waiting if DSP is active. */
    if ((csr & (0x1 << DXP_CSR_DSP_ACT_BIT))) {
      sprintf(info_string, "Polls for waiting for DSP Active = %d", i);
      dxp_log_info("dxp_wait_for_active", info_string);
      return DXP_SUCCESS;
    }

    mercury_md_wait(&wait);
  }

  sprintf(info_string, "Timeout waiting for DSP Active to be set "
		  "after %d pools", n_polls);
  dxp_log_error("dxp_wait_for_active", info_string, DXP_TIMEOUT);
  return DXP_TIMEOUT;
}


/**
 * @brief Wait for BUSY to go to the desired value.
 *
 * If BUSY does not go to @a BUSY within @a timeout seconds then an error is
 * returned.
 *
 */
static int dxp__wait_for_busy(int ioChan, int modChan, parameter_t desired,
                              double timeout, Board *board)
{
  int status;
  int n_polls = 0;
  int i;

  float wait = .01f;

  double BUSY = 0;
  double RUNERROR = 0;

  ASSERT(board != NULL);

  
  n_polls = (int)ROUND(timeout / wait);

  for (i = 0; i < n_polls; i++) {

	status = dxp_read_dspsymbol(&ioChan, &modChan, "BUSY", board, &BUSY);

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Error reading BUSY during poll iteration %d while "
			  "waiting for BUSY to go to %hu on ioChan = %d", i, desired, ioChan);
	  dxp_log_error("dxp_wait_for_busy", info_string, status);
	  return status;
	}

	if (BUSY == (double)desired) {
    sprintf(info_string, "Polls for waiting for DSP BUSY = %d", i);
    dxp_log_info("dxp__wait_for_busy", info_string);
	  return DXP_SUCCESS;
	}

	mercury_md_wait(&wait);
  }

  sprintf(info_string, "Timeout waiting for BUSY to go to %hu (current = "
          "%0.0f) on ioChan = %d", desired, BUSY, ioChan);
  dxp_log_error("dxp_wait_for_busy", info_string, DXP_TIMEOUT);

  /* DEBUG read out RUNERROR and print it for debugging purpose */
  status = dxp_read_dspsymbol(&ioChan, &modChan, "RUNERROR", board, &RUNERROR);
  sprintf(info_string, "RUNERROR after time out waiting for busy is %0.0f) "
		  "on ioChan = %d", RUNERROR, ioChan);
  dxp_log_error("dxp_wait_for_busy", info_string, DXP_TIMEOUT);
  

  return DXP_TIMEOUT;
}


/**
 * @brief Gets the relative address in the DSP data memory of a global
 * DSP parameter. This routine does not verify that the parameter is global
 * and will return an error if it cannot be found in the global list. The
 * calling routine is expected to add this relative address to the base address
 * of the DSP data memory.
 */
static int dxp__get_global_addr(char *name, Dsp_Info *dsp, unsigned long *addr)
{
  unsigned short i;


  ASSERT(name != NULL);
  ASSERT(dsp != NULL);
  ASSERT(addr != NULL);


  for (i = 0; i < dsp->params->nsymbol; i++) {
    if (STREQ(name, dsp->params->parameters[i].pname)) {
      *addr = dsp->params->parameters[i].address;
      return DXP_SUCCESS;
    }
  }

  sprintf(info_string, "Unable to find '%s' in global DSP parameter list",
          name);
  dxp_log_error("dxp__get_global_addr", info_string, DXP_NOSYMBOL);
  return DXP_NOSYMBOL;
}


/**
 * @brief Gets the relative address in the DSP data memory of a per-channel
 * DSP parameter. This routine does not verify that the parameter is 
 * per-channel and will return an error if it cannot be found in the per-channel
 * list. The calling routine is expected to add this relative address to the
 * base address of the DSP data memory.
 */
static int dxp__get_channel_addr(char *name, int modChan, Dsp_Info *dsp,
                                 unsigned long *addr)
{
  unsigned short i;


  ASSERT(name != NULL);
  ASSERT(dsp != NULL);
  ASSERT(addr != NULL);
  ASSERT(modChan >=0 && modChan < 4);
  
  for (i = 0; i < dsp->params->n_per_chan_symbols; i++) {
    if (STREQ(name, dsp->params->per_chan_parameters[i].pname)) {

      *addr = dsp->params->per_chan_parameters[i].address +
        dsp->params->chan_offsets[modChan];

      return DXP_SUCCESS;
    }
  }

  sprintf(info_string, "Unable to find '%s' in per-channel DSP parameter list",
		  name);
  dxp_log_error("dxp__get_channel_addr", info_string, DXP_NOSYMBOL);
  return DXP_NOSYMBOL;
}


/**
 * @brief Determines if the symbol is a global DSP symbol or a per-channel
 * symbol.
 *
 * This routine does not indicate definitively that the parameter is an
 * actual parameter, just that it is or isn't a global parameter.
 */
static int dxp__is_symbol_global(char *name, Dsp_Info *dsp,
                                 boolean_t *is_global)
{
  unsigned short i;


  ASSERT(name != NULL);
  ASSERT(dsp != NULL);
  ASSERT(is_global != NULL);


  for (i = 0; i < dsp->params->nsymbol; i++) {
    if (STREQ(name, dsp->params->parameters[i].pname)) {
      *is_global = TRUE_;
      return DXP_SUCCESS;
    }
  }

  *is_global = FALSE_;
  return DXP_SUCCESS;
}


/**
 * @brief Reads an ADC trace from the board. Assumes that the appropriate
 * parameters (TRACEWAIT, etc.) have already been set.
 */
static int dxp__get_adc_trace(int ioChan, int modChan, unsigned long *data,
                              Board *board)
{
  int status;

  unsigned long buffer_addr = 0;

  double TRACELEN   = 0.0;
  double TRACESTART = 0.0;


  ASSERT(board != NULL);


  status = dxp_read_dspsymbol(&ioChan, &modChan, "TRACELEN", board, &TRACELEN);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading TRACELEN from ioChan = %d", ioChan);
    dxp_log_error("dxp_get_adc_trace", info_string, status);
    return status;
  }
  
  ASSERT(TRACELEN != 0);

  status = dxp_read_dspsymbol(&ioChan, &modChan, "TRACESTART", board,
                              &TRACESTART);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading TRACESTART from ioChan = %d", ioChan);
    dxp_log_error("dxp_get_adc_trace", info_string, status);
    return status;
  }

  sprintf(info_string, "TRACESTART = %0.3f, TRACELEN = %0.3f", TRACESTART, 
    TRACELEN);
  dxp_log_debug("dxp_get_adc_trace", info_string);

  buffer_addr = (unsigned long)TRACESTART + DXP_DSP_DATA_MEM_ADDR;

  status = dxp__read_block(&ioChan, buffer_addr, (unsigned int)TRACELEN,
                           data);
  
  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading ADC trace from %#lx for ioChan = %d",
            buffer_addr, ioChan);
    dxp_log_error("dxp_get_adc_trace", info_string, status);
    return status;
  }

  return DXP_SUCCESS;
}


/**
 * @brief Read @a n 32-bit words from the requested address @a addr.
 *
 * Expects @a data to already be allocated.
 */
static int dxp__read_block(int *ioChan, unsigned long addr, unsigned long n,
                           unsigned long *data)
{
  int status;

  unsigned int f;
  unsigned int len;

  unsigned long a;
  unsigned long i;

  unsigned short *buf = NULL;


  ASSERT(ioChan != NULL);
  ASSERT(data != NULL);


  /* Write the address to the cache. */
  a   = DXP_A_ADDR;
  f   = DXP_F_IGNORE;
  len = 0;

  status = mercury_md_io(ioChan, &f, &a, &addr, &len);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error setting read address to %#lx for ioChan = %d",
            addr, *ioChan);
    dxp_log_error("dxp__read_block", info_string, status);
    return status;
  }

  a   = DXP_A_IO;
  f   = DXP_F_READ;
  len = (unsigned int)(n * 2);

  /* The MD layer expects an array of 16-bit words. */
  buf = mercury_md_alloc(len * sizeof(unsigned short));

  if (buf == NULL) {
    sprintf(info_string, "Unable to allocate %d bytes for 'buf'",
            len * sizeof(unsigned short));
    dxp_log_error("dxp__read_block", info_string, status);
    return status;
  }

  status = mercury_md_io(ioChan, &f, &a, buf, &len);
  
  if (status != DXP_SUCCESS) {
    mercury_md_free(buf);
    sprintf(info_string, "Error reading %u words of block data from address "
            "%#lx for ioChan = %d", len, addr, *ioChan);            
    dxp_log_error("dxp__read_block", info_string, status);
    return status;
  }

  /* DEBUG Print out all USB read and write addresses when in doubt
  sprintf(info_string, "Read block of %u words from address %#X", n, addr);
  dxp_log_debug("dxp__read_block", info_string); */
  
  for (i = 0; i < n; i++) {
    data[i] = ((unsigned long)buf[(i * 2) + 1] << 16) | buf[i * 2];
  }

  mercury_md_free(buf);
  return DXP_SUCCESS;
}


/** @brief Start the ADC trace special run.
 *
 */
static int dxp__begin_adc_trace(int ioChan, int modChan, Board *board)
{
  int status;


  ASSERT(board != NULL);


  status = dxp__do_trace(ioChan, modChan, MERCURY_TRACETYPE_ADC, board);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error starting ADC trace for ioChan = %d", ioChan);
	dxp_log_error("dxp_begin_adc_trace", info_string, status);
	return status;
  }


  return DXP_SUCCESS;
}

/** @brief Do a trace special run.
 *
 * Other, higher-level routines are meant to wrap this: dxp_begin_adc_trace(), etc.
 *
 */
static int dxp__do_trace(int ioChan, int modChan, parameter_t type, Board *board)
{
  int status;
  int id = 0;

  parameter_t TRACECHAN  = (parameter_t)modChan;
  parameter_t RUNTYPE    = MERCURY_RUNTYPE_SPECIAL;
  parameter_t TRACETYPE  = type;
  parameter_t SPECIALRUN = MERCURY_SPECIALRUN_TRACE;

  unsigned short ignored_gate   = 0;
  unsigned short ignored_resume = 0;

  
  ASSERT(board != NULL);

  status = dxp_modify_dspsymbol(&ioChan, &modChan, "TRACECHAN", &TRACECHAN,
								board);
  
  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error setting TRACECHAN to %hu for ioChan = %d",
			TRACECHAN, ioChan);
	dxp_log_error("dxp__do_trace", info_string, status);
	return status;
  }

  status = dxp_modify_dspsymbol(&ioChan, &modChan, "RUNTYPE", &RUNTYPE, board);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error setting RUNTYPE to %hu for ioChan = %d",
			RUNTYPE, ioChan);
	dxp_log_error("dxp__do_trace", info_string, status);
	return status;
  }  

  status = dxp_modify_dspsymbol(&ioChan, &modChan, "TRACETYPE", &TRACETYPE,
								board);
  
  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error setting TRACETYPE to %hu for ioChan = %d",
			TRACETYPE, ioChan);
	dxp_log_error("dxp__do_trace", info_string, status);
	return status;
  }

  status = dxp_modify_dspsymbol(&ioChan, &modChan, "SPECIALRUN", &SPECIALRUN,
								board);
  
  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error setting SPECIALRUN to %hu for ioChan = %d",
			SPECIALRUN, ioChan);
	dxp_log_error("dxp__do_trace", info_string, status);
	return status;
  }  

  status = dxp_begin_run(&ioChan, &modChan, &ignored_gate, &ignored_resume,
						 board, &id);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error starting ADC trace special run for ioChan = %d",
			ioChan);
	dxp_log_error("dxp__do_trace", info_string, status);
	return status;
  }

  sprintf(info_string, "Started run id = %d on ioChan = %d", id, ioChan);
  dxp_log_debug("dxp__do_trace", info_string);

  /* XXX Calculate timeout based on TRACEWAIT here */
  
  
  status = dxp__wait_for_busy(ioChan, modChan, 0, 10.0, board);

  if (status != DXP_SUCCESS) {
	/* End the run so that RUNTYPE is reset properly. */
	status = dxp_end_control_task(&ioChan, &modChan, board);

	sprintf(info_string, "Timeout waiting for BUSY to go to 0 on ioChan = %d",
			ioChan);
	dxp_log_error("dxp__do_trace", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/** @brief Sets the trace interval based on the value in @a info.
 *
 * Interprets @a info[1] as TRACEWAIT.
 */
static int dxp__process_trace_wait(int ioChan, int modChan, unsigned int len,
								   int *info, Board *b)
{
  int status;

  parameter_t TRACEWAIT = 0;


  ASSERT(info != NULL);
  ASSERT(b != NULL);


  if (len < 2) {
	sprintf(info_string, "Specified length '%u' for 'info' array is invalid for "
			"ioChan = %d", len, ioChan);
	dxp_log_error("dxp__process_trace_wait", info_string, DXP_INVALID_LENGTH);
	return DXP_INVALID_LENGTH;
  }

  TRACEWAIT = (parameter_t)info[1];

  status = dxp_modify_dspsymbol(&ioChan, &modChan, "TRACEWAIT", &TRACEWAIT, b);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error setting TRACEWAIT for ioChan = %d", ioChan);
	dxp_log_error("dxp__process_trace_wait", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/** @brief Triggers a special run in the DSP to apply settings.
 *
 * In previous versions of our hardware, the apply functionality was performed
 * in the DSP. This feature has now been pushed into the software, which offers
 * a higher granularity of control then the hardware does when it comes to
 * knowing if any parameters change or not.
 *
 * Any time a parameter on the board changes, an apply run should be started
 * and stopped. The purpose of this run is to allow the DSP to apply any new
 * changes and it should take no longer then 10 seconds.
 *
 * It is important to reset the apply status (APPLYSTAT) back to 1 so that the
 * DSP can reset it to 0 when the apply has been completed (in addition to
 * setting BUSY to 0 as usual to indicate that the run is complete).
 *
 * Apply is a global operation so it needs to only be done once per module.
 */
static int dxp__do_apply(int ioChan, int modChan, Board *board)
{
  int status;
  int active;
  int id = 0;
  int n_polls;
  int i;

  unsigned short ignored = 0;

  parameter_t RUNTYPE    = MERCURY_RUNTYPE_SPECIAL;
  parameter_t SPECIALRUN = MERCURY_SPECIALRUN_APPLY;
  parameter_t APPLYSTAT  = 0xCDCD;
  parameter_t ERRINFO    = 0xCDCD;

  double applystat  = 0.0;
  double runtype    = 0.0;
  double specialrun = 0.0;
  double errinfo    = 0.0;
  double timeout    = 1.0;

  float poll_interval = 0.1f;


  ASSERT(board != NULL);


  status = dxp_modify_dspsymbol(&ioChan, &modChan, "RUNTYPE", &RUNTYPE, board);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error setting RUNTYPE for ioChan = %d", ioChan);
	dxp_log_error("dxp__do_apply", info_string, status);
	return status;
  }

  status = dxp_modify_dspsymbol(&ioChan, &modChan, "SPECIALRUN", &SPECIALRUN,
								board);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error setting SPECIALRUN for ioChan = %d", ioChan);
	dxp_log_error("dxp__do_apply", info_string, status);
	return status;
  }

  /* Set APPLYSTAT so that the DSP can clear it upon successful completion of
   * of the apply operation.
   */
  status = dxp_modify_dspsymbol(&ioChan, &modChan, "APPLYSTAT", &APPLYSTAT,
								board);
  
  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error setting APPLYSTAT for ioChan = %d", ioChan);
	dxp_log_error("dxp__do_apply", info_string, status);
	return status;
  }

  status = dxp_modify_dspsymbol(&ioChan, &modChan, "ERRINFO", &ERRINFO, board);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error setting ERRINFO for ioChan = %d", ioChan);
    dxp_log_error("dxp__do_apply", info_string, status);
    return status;
  }

  /* Extra debug check for #527. */
  status = dxp_read_dspsymbol(&ioChan, &modChan, "RUNTYPE", board, &runtype);
  ASSERT(status == DXP_SUCCESS);
  status = dxp_read_dspsymbol(&ioChan, &modChan, "SPECIALRUN", board, &specialrun);
  ASSERT(status == DXP_SUCCESS);
  status = dxp_read_dspsymbol(&ioChan, &modChan, "APPLYSTAT", board, &applystat);
  ASSERT(status == DXP_SUCCESS);
  
  sprintf(info_string, "Right before begin run: RUNTYPE = %#hx, "
          "SPECIALRUN = %#hx, APPLYSTAT = %#hx", (parameter_t)runtype,
          (parameter_t)specialrun, (parameter_t)applystat);
  dxp_log_debug("dxp__do_apply", info_string);

  status = dxp_begin_run(&ioChan, &modChan, &ignored, &ignored, board, &id);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error starting special run for ioChan = %d", ioChan);
	dxp_log_error("dxp__do_apply", info_string, status);
	return status;
  }

  sprintf(info_string, "Started run id = %d on ioChan = %d", id, ioChan);
  dxp_log_debug("dxp__do_apply", info_string);

  /* We have an approximate timeout here since we don't include the
   * time it takes to call dxp__run_enable_active() in the calculation.
   */
  n_polls = (int)ROUND(timeout / (double)poll_interval);

  sprintf(info_string, "n_polls = %d, timeout = %0.1f, poll_interval = %0.1f",
          n_polls, timeout, poll_interval);
  dxp_log_debug("dxp__do_apply", info_string);

  for (i = 0; i < n_polls; i++) {
    status = dxp__run_enable_active(ioChan, &active);

    if (status != DXP_SUCCESS) {
      sprintf(info_string, "Error reading run enable for ioChan = %d", ioChan);
      dxp_log_error("dxp__do_apply", info_string, status);
      return status;
    }

    if (!active) {
      /* The apply is complete. */
      break;
    }

    mercury_md_wait(&poll_interval);
  }

  if (i == n_polls) {
    status = dxp_end_run(&ioChan, &modChan, board);
    sprintf(info_string, "Timeout waiting %0.1f second(s) for the apply run "
            "to complete on ioChan = %d", timeout, ioChan);
    dxp_log_error("dxp__do_apply", info_string, DXP_TIMEOUT);
    return DXP_TIMEOUT;
  }

  status = dxp_read_dspsymbol(&ioChan, &modChan, "ERRINFO", board, &errinfo);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading the error information for "
            "ioChan = %d", ioChan);
    dxp_log_error("dxp__do_apply", info_string, status);
    return status;
  }

  sprintf(info_string, "ERRINFO after apply = %#hx", (parameter_t)errinfo);
  dxp_log_debug("dxp_do_apply", info_string);

  status = dxp_read_dspsymbol(&ioChan, &modChan, "APPLYSTAT", board, &applystat);
  
  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error reading the status of the apply operation for "
			"ioChan = %d", ioChan);
	dxp_log_error("dxp__do_apply", info_string, status);
	return status;
  }

  if ((unsigned short)applystat != 0) {
	sprintf(info_string, "Apply operation (status = %#hx) did not complete for "
			"ioChan = %d", (unsigned short)applystat, ioChan);
	dxp_log_error("dxp__do_apply", info_string, DXP_APPLY_STATUS);
	return DXP_APPLY_STATUS;
  }

  return DXP_SUCCESS;
}

/** @brief Check if the run enable bit is set active.
 *
 */
static int dxp__run_enable_active(int ioChan, int *active)
{
  int status;

  unsigned long CSR = 0x0;


  ASSERT(active != NULL);


  status = dxp__read_global_register(ioChan, DXP_SYS_REG_CSR, &CSR);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading Control Status Register for ioChan = %d",
            ioChan);
    dxp_log_error("dxp_run_enable_active", info_string, status);
    return status;
  }

  if (CSR & (0x1 << DXP_CSR_RUN_ENABLE)) {
    *active = 1;
  
  } else {
    *active = 0;
  }

  return DXP_SUCCESS; 
}


/** @brief Converts an array of 2 32-bit unsigned longs to a double.
 *
 * It is an unchecked exception to pass a NULL array into this routine.
 */
static double dxp__unsigned64_to_double(unsigned long *u64)
{
  double d = 0.0;


  ASSERT(u64 != NULL);


  d = (double)u64[0] + ((double)u64[1] * pow(2.0, 32.0));

  return d;
}


/** @brief Returns on the clock tick in seconds.
 *
 */
static double dxp__get_clock_tick(void)
{
  return 20.0e-9;
}


/** @brief Puts the DSP to sleep in preparation for doing things like
 * downloading a new FiPPI.
 *
 * Only tries to put the DSP to sleep if it is running.
 */
static int dxp__put_dsp_to_sleep(int ioChan, int modChan, Board *b)
{
  int status;
  int id = 0;

  parameter_t SPECIALRUN = MERCURY_SPECIALRUN_DSP_SLEEP;
  parameter_t RUNTYPE    = MERCURY_RUNTYPE_SPECIAL;

  unsigned short ignored_gate   = 0;
  unsigned short ignored_resume = 0;

  unsigned long csr = 0;


  ASSERT(b != NULL);


  status = dxp__read_global_register(ioChan, DXP_SYS_REG_CSR, &csr);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading CSR for ioChan = %d", ioChan);
    dxp_log_error("dxp__put_dsp_to_sleep", info_string, status);
    return status;
  }

  /* If no DSP is active, then we don't need to bother putting it to sleep. */
  if (!(csr & (0x1 << DXP_CSR_DSP_ACT_BIT))) {
    sprintf(info_string, "Skipping DSP sleep since no DSP is active for "
            "ioChan = %d", ioChan);
    dxp_log_info("dxp__put_dsp_to_sleep", info_string);
    return DXP_SUCCESS;
  }

  sprintf(info_string, "Preparing to put DSP to sleep for ioChan = %d", ioChan);
  dxp_log_debug("dxp__put_dsp_to_sleep", info_string);

  status = dxp_modify_dspsymbol(&ioChan, &modChan, "SPECIALRUN", &SPECIALRUN, b);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error setting SPECIALRUN to put DSP to sleep for "
			"ioChan = %d", ioChan);
	dxp_log_error("dxp__put_dsp_to_sleep", info_string, status);
	return status;
  }

  status = dxp_modify_dspsymbol(&ioChan, &modChan, "RUNTYPE", &RUNTYPE, b);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error setting RUNTYPE to put DSP to sleep for "
			"ioChan = %d", ioChan);
	dxp_log_error("dxp__put_dsp_to_sleep", info_string, status);
	return status;
  }

  status = dxp_begin_run(&ioChan, &modChan, &ignored_gate, &ignored_resume,
						 b, &id);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error starting run to put DSP to sleep for ioChan = %d",
			ioChan);
	dxp_log_error("dxp__put_dsp_to_sleep", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/** @brief Awakens the DSP from its slumber.
 *
 */
static int dxp__wake_dsp_up(int ioChan, int modChan, Board *b)
{
  int status;
  int ignored_status;

  parameter_t RUNTYPE = MERCURY_RUNTYPE_NORMAL;

  double RUNERROR = 0;

  unsigned long csr = 0;

  ASSERT(b != NULL);


  status = dxp__read_global_register(ioChan, DXP_SYS_REG_CSR, &csr);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading CSR for ioChan = %d", ioChan);
    dxp_log_error("dxp__wake_dsp_up", info_string, status);
    return status;
  }

  /* If no DSP is active, then we don't need to bother waking it up. */
  if (!(csr & (0x1 << DXP_CSR_DSP_ACT_BIT))) {
    sprintf(info_string, "Skipping DSP wake-up since no DSP is active for "
            "ioChan = %d", ioChan);
    dxp_log_info("dxp__wake_dsp_up", info_string);
    return DXP_SUCCESS;
  }

  sprintf(info_string, "Preparing to wake up DSP (CSR = %#lx) for ioChan = %d", 
        csr, ioChan);
  dxp_log_debug("dxp__wake_dsp_up", info_string);

  status = dxp_end_run(&ioChan, &modChan, b);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error stopping special DSP run for ioChan = %d", ioChan);
	dxp_log_error("dxp__wake_dsp_up", info_string, status);
	return status;
  }

  status = dxp__wait_for_busy(ioChan, modChan, 0, 1.0, b);
  
  if (status != DXP_SUCCESS) {
	ignored_status = dxp_read_dspsymbol(&ioChan, &modChan, "RUNERROR", b,
										&RUNERROR);
	sprintf(info_string, "Error waiting for DSP to wake up (RUNERROR = %#hx) for "
			"ioChan = %d", (parameter_t)RUNERROR, ioChan);
	dxp_log_error("dxp__wake_dsp_up", info_string, status);
	return status;
  }

  status = dxp_modify_dspsymbol(&ioChan, &modChan, "RUNTYPE", &RUNTYPE, b);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error setting run type to normal after waking up "
			"the DSP for ioChan = %d", ioChan);
	dxp_log_error("dxp__wake_dsp_up", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/** @brief Start the baseline history trace.
 *
 */
static int dxp__begin_base_hist(int ioChan, int modChan, Board *board)
{
  int status;


  ASSERT(board != NULL);
  

  status = dxp__do_trace(ioChan, modChan, MERCURY_TRACETYPE_BASE_HIST, board);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error starting baseline history trace for ioChan = %d",
			ioChan);
	dxp_log_error("dxp__begin_base_hist", info_string, status);
	return status;
  }
  
  return DXP_SUCCESS;
}


/** @brief Starts a trace for the fast baseline-subtracted output
 *
 */
static int dxp__begin_fast_base_sub_trace(int ioChan, int modChan, Board *board)
{
  int status;


  ASSERT(board != NULL);


  status = dxp__do_trace(ioChan, modChan, MERCURY_TRACETYPE_FAST_BASE_SUB, board);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error starting trigger filter trace for ioChan = %d",
			ioChan);
	dxp_log_error("dxp__begin_fast_base_sub_trace", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/** @brief Starts a trace for the baseline instantaneous samples
 *
 */
static int dxp__begin_base_inst_trace(int ioChan, int modChan, Board *board)
{
  int status;


  ASSERT(board != NULL);


  status = dxp__do_trace(ioChan, modChan, MERCURY_TRACETYPE_BASE_INST, board);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error starting baseline samples trace for ioChan = %d",
			ioChan);
	dxp_log_error("dxp__begin_base_inst_trace", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/** @brief Starts a trace for the baseline-subtracted output
 *
 */
static int dxp__begin_base_sub_trace(int ioChan, int modChan, Board *board)
{
  int status;


  ASSERT(board != NULL);


  status = dxp__do_trace(ioChan, modChan, MERCURY_TRACETYPE_BASE_SUB, board);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error starting baseline filter trace for ioChan = %d",
			ioChan);
	dxp_log_error("dxp__begin_base_sub_trace", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/** @brief Starts a trace for the slow filter baseline-subtracted output
 *
 */
static int dxp__begin_slow_base_sub_trace(int ioChan, int modChan, Board *board)
{
  int status;


  ASSERT(board != NULL);


  status = dxp__do_trace(ioChan, modChan, MERCURY_TRACETYPE_SLOW_BASE_SUB, board);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error starting energy filter trace for ioChan = %d",
			ioChan);
	dxp_log_error("dxp__begin_slow_base_sub_trace", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/** @brief Starts a trace for the event samples
 *
 */
static int dxp__begin_events_trace(int ioChan, int modChan, Board *board)
{
  int status;


  ASSERT(board != NULL);


  status = dxp__do_trace(ioChan, modChan, MERCURY_TRACETYPE_EVENTS, board);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error starting energy samples trace for ioChan = %d",
			ioChan);
	dxp_log_error("dxp__begin_events_trace", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/** @brief Get the start of the SCA data in the external memory.
 *
 */
XERXES_STATIC int dxp__get_sca_addr(int ioChan, int modChan, Board *b,
									unsigned long *addr)
{
  int status;

  double SCAMEMBASE = 0.0;


  ASSERT(b != NULL);
  ASSERT(addr != NULL);

  status = dxp_read_dspsymbol(&ioChan, &modChan, "SCAMEMBASE", b, &SCAMEMBASE);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error getting base of SCA memory block for ioChan = %d",
			ioChan);
	dxp_log_error("dxp__get_sca_addr", info_string, status);
	return status;
  }

  *addr = (unsigned long)SCAMEMBASE + DXP_DSP_EXT_MEM_ADDR + 
          (modChan * MERCURY_SCA_CHAN_OFFSET);
  
  return DXP_SUCCESS;
}
