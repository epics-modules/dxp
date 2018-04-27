/*
 * Copyright (c) 2004 X-ray Instrumentation Associates
 * Copyright (c) 2005-2014 XIA LLC
 * All rights reserved
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
 *   * Neither the name of XIA LLC
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
 */

#include <stdlib.h>
#include <ctype.h>

#include "xia_assert.h"
#include "xia_xerxes_structures.h"
#include "xia_common.h"
#include "xia_stj.h"
#include "xia_file.h"

#include "xerxes_structures.h"
#include "xerxes_errors.h"
#include "xerxes_generic.h"

#include "stj.h"


typedef int (*FPGA_downloader_func_t)(int ioChan, int modChan, Board *board);

/* Element of a list of FPGA download routines. */
typedef struct _FPGA_downloader {
    /* Name of the FPGA type this downloader handles */
    char *name;

    /* FPGA downloader function pointer */
    FPGA_downloader_func_t f;

} FPGA_downloader_t;


/*
 * Pointer to utility functions
 */
static DXP_MD_IO         stj_md_io;
static DXP_MD_SET_MAXBLK stj_md_set_maxblk;
static DXP_MD_GET_MAXBLK stj_md_get_maxblk;

/* Define the utility routines used throughout this library */
static DXP_MD_LOG   stj_md_log;
static DXP_MD_ALLOC stj_md_alloc;
static DXP_MD_FREE  stj_md_free;
static DXP_MD_PUTS  stj_md_puts;
static DXP_MD_WAIT  stj_md_wait;
static DXP_MD_FGETS stj_md_fgets;

static char info_string[INFO_LEN];

static int dxp_is_symbol_global(char *name, Dsp_Info *dsp, boolean_t *is_global);
static int dxp_get_global_addr(char *name, Dsp_Info *dsp, unsigned long *addr);
static int dxp_get_channel_addr(char *name, int modChan, Dsp_Info *dsp,
                                unsigned long *addr);
static int dxp_set_csr_bit(int ioChan, byte_t bit);
static int dxp_clear_csr_bit(int ioChan, byte_t bit);
static int dxp_wait_for_busy(int ioChan, int modChan, parameter_t desired,
                             double timeout, Board *board);
static int dxp_get_adc_trace(int ioChan, int modChan, unsigned long *data,
                             Board *board);
static int dxp__get_mca_chan_addr(int ioChan, int modChan, Board *board,
                                  unsigned long *addr);
static int dxp__burst_read_block(int ioChan, int modChan, unsigned long addr,
                                 unsigned int len, unsigned long *data);
static int dxp__burst_read_buffer(int ioChan, int modChan, unsigned long addr,
                                  unsigned int len, unsigned long *data);
static int dxp__read_block(int ioChan, unsigned long addr, unsigned int len,
                           unsigned long *data);
static int dxp__process_trace_param(int ioChan, int modChan, unsigned int len,
                                    int *info, Board *b);
static int dxp__do_trace(int ioChan, int modChan, Board *board);
static int dxp__run_enable_active(int ioChan, int *active);

/* FPGA downloaders */
static int dxp_download_fippis(int ioChan, int modChan, Board *b);
static int dxp_download_all_fpgas(int ioChan, int modChan, Board *board);
static int dxp_download_system_fpga(int ioChan, int modChan, Board *b);
static int dxp_download_fippis_dsp_no_wake(int ioChan, int modChan, Board *b);

/* Control tasks */
static int dxp_begin_adc_trace(int ioChan, int modChan, Board *board);
static int dxp_do_apply(int ioChan, int modChan, Board *board);
static int dxp__wake_dsp_up(int ioChan, int modChan, Board *b);
static int dxp__adjust_offsets(int ioChan, int modChan, Board *b);
static int dxp__bias_scan(int ioChan, int modChan, Board *board);
static int dxp__bias_set_dac(int ioChan, int modChan, Board *board);
static int dxp__do_special_run(int ioChan, int modChan, Board *board,
                               parameter_t specialRun, double wait);
static int dxp__start_special_run(int ioChan, int modChan, Board *board,
                                  parameter_t specialRun);

/* Memory accessors */
static int dxp__write_data_memory(int ioChan, unsigned long base,
                                  unsigned long offset, unsigned long *data);
static int dxp__read_data_memory(int ioChan, unsigned long base,
                                 unsigned long offset, unsigned long *data);

/* XXX This an unbelievable hack born out of time constraints. This is not
 * an appropriate way to deal with the fact that the Stj has multiple
 * channels and that you shouldn't try and free all of them at once.
 */
static boolean_t unhooked = FALSE_;


static FPGA_downloader_t FPGA_DOWNLOADERS[] = {
    { "all",                 dxp_download_all_fpgas },
    { "system_fpga",         dxp_download_system_fpga },
    { "a_and_b",             dxp_download_fippis },
    { "a_and_b_dsp_no_wake", dxp_download_fippis_dsp_no_wake },
};


static control_task_t CONTROL_TASKS[] = {
    {STJ_CT_ADC,           dxp__process_trace_param, dxp_begin_adc_trace},
    {STJ_CT_APPLY,         NULL,                    dxp_do_apply},
    {STJ_CT_WAKE_DSP,      NULL,                    dxp__wake_dsp_up},
    {STJ_CT_ADJUST_OFFSETS, NULL,                   dxp__adjust_offsets},
    {STJ_CT_BIAS_SCAN, 			NULL,                   dxp__bias_scan},
    {STJ_CT_BIAS_SET_DAC, 	NULL,               dxp__bias_set_dac},
};


static control_task_data_t CONTROL_TASK_DATAS[] = {
    {STJ_CT_ADC,       dxp_get_adc_trace},
};


static memory_accessor_t MEMORY_WRITERS[] = {
    {"data", dxp__write_data_memory},
};


/* These are registers that are publicly exported for use in Handel and other
 * callers. Not every register needs to be included here.
 */
static register_table_t REGISTER_TABLE[] = {
    {"CVR",        0x10},
    {"SVR",        0x44},
    {"CSR",        0x48},
    {"VAR",        0x4C},
    {"MCR",        0x60},
    {"MFR",        0x64},
};


#define STJ_DSP_BOOT_ACTIVE_TIMEOUT 1.0f


/*
 * Initialize the function table for the Stj product.
 */
XERXES_EXPORT int dxp_init_stj(Functions* funcs)
{
    ASSERT(funcs != NULL);

    unhooked = FALSE_;

    funcs->dxp_init_driver = dxp_init_driver;
    funcs->dxp_init_utils  = dxp_init_utils;

    funcs->dxp_get_dspinfo = dxp_get_dspinfo;
    funcs->dxp_get_fipinfo = dxp_get_fipinfo;
    funcs->dxp_get_dspconfig = dxp_get_dspconfig;
    funcs->dxp_get_fpgaconfig = dxp_get_fpgaconfig;

    funcs->dxp_download_fpgaconfig = dxp_download_fpgaconfig;
    funcs->dxp_download_fpga_done = dxp_download_fpga_done;
    funcs->dxp_download_dspconfig = dxp_download_dspconfig;

    funcs->dxp_loc = dxp_loc;

    funcs->dxp_read_spectrum = dxp_read_spectrum;
    funcs->dxp_get_spectrum_length = dxp_get_spectrum_length;
    funcs->dxp_read_baseline = dxp_read_baseline;
    funcs->dxp_get_baseline_length = dxp_get_baseline_length;
    funcs->dxp_get_event_length = dxp_get_event_length;

    funcs->dxp_write_dspparams = dxp_write_dspparams;
    funcs->dxp_read_dspparams = dxp_read_dspparams;
    funcs->dxp_read_dspsymbol = dxp_read_dspsymbol;
    funcs->dxp_modify_dspsymbol = dxp_modify_dspsymbol;

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

    funcs->dxp_read_mem = dxp_read_mem;
    funcs->dxp_write_mem = dxp_write_mem;
    funcs->dxp_write_reg = dxp_write_reg;
    funcs->dxp_read_reg  = dxp_read_reg;
    funcs->dxp_unhook = dxp_unhook;

    funcs->dxp_get_symbol_by_index = dxp_get_symbol_by_index;
    funcs->dxp_get_num_params = dxp_get_num_params;

    return DXP_SUCCESS;
}


/*
 * Translates a DSP symbol name into an index.
 */
static int dxp_loc(char name[], Dsp_Info* dsp, unsigned short* address)
{
    UNUSED(name);
    UNUSED(dsp);
    UNUSED(address);

    return DXP_SUCCESS;
}


/*
 * Initialize the inteface function table.
 */
static int dxp_init_driver(Interface* iface)
{
    ASSERT(iface != NULL);


    stj_md_io         = iface->funcs->dxp_md_io;
    stj_md_set_maxblk = iface->funcs->dxp_md_set_maxblk;
    stj_md_get_maxblk = iface->funcs->dxp_md_get_maxblk;

    return DXP_SUCCESS;
}


/*
 * Initialize the utility function table.
 */
static int dxp_init_utils(Utils* utils)
{
    stj_md_log   = utils->funcs->dxp_md_log;
    stj_md_alloc = utils->funcs->dxp_md_alloc;
    stj_md_free  = utils->funcs->dxp_md_free;
    stj_md_wait  = utils->funcs->dxp_md_wait;
    stj_md_puts  = utils->funcs->dxp_md_puts;
    stj_md_fgets = utils->funcs->dxp_md_fgets;

    return DXP_SUCCESS;
}


/*
 * Downloads an FPGA file to the specified target on the hardware.
 *
 * Dispatches the requested FPGA to the appropriate download function.
 */
static int dxp_download_fpgaconfig(int* ioChan, int* modChan, char *name,
                                   Board* board)
{
    int i;
    int status;
    int n_dloaders = (int)(sizeof(FPGA_DOWNLOADERS) / sizeof(FPGA_DOWNLOADERS[0]));



    UNUSED(modChan);
    UNUSED(board);


    ASSERT(ioChan != NULL);
    ASSERT(name   != NULL);


    sprintf(info_string, "Preparing to download '%s' to channel %d", name,
            *ioChan);
    dxp_log_debug("dxp_download_fpgaconfig", info_string);

    for (i = 0; i < n_dloaders; i++) {
        if (STREQ(FPGA_DOWNLOADERS[i].name, name)) {

            status = FPGA_DOWNLOADERS[i].f(*ioChan, *modChan, board);

            if (status != DXP_SUCCESS) {
                sprintf(info_string, "Error downloading FPGA type '%s' for channel %d",
                        name, *ioChan);
                dxp_log_error("dxp_download_fpgaconfig", info_string, status);
                return status;
            }

            return DXP_SUCCESS;
        }
    }

    sprintf(info_string, "Unknown FPGA type '%s' for channel %d", name, *ioChan);
    dxp_log_error("dxp_download_fpgaconfig", info_string, DXP_UNKNOWN_FPGA);
    return DXP_UNKNOWN_FPGA;
}


/*
 * Parse the specified FPGA file.
 *
 * FPGA files for the Stj hardware have the following format:
 *
 * Comment lines start with a '*' and should be ignored.
 * The data lines consist of the actual FPGA bytes stored sequentially,
 * in the order that they need to be written to the hardware. The Fippi_Info
 * structure provides a full 16-bits as its data array. The two options, then,
 * are to store the data in a packed format or to only use the low-byte and
 * have the array size be twice as big. To be consistent with the other device
 * drivers, I will use the packed format, where the data will be unpacked and
 * written to the board as lo0, hi0, lo1, hi1, etc.
 */
static int dxp_get_fpgaconfig(void *fip)
{
    size_t n_chars_in_line = 0;
    unsigned int i;
    int n_data = 0;

    FILE *fp = NULL;

    Fippi_Info *fippi = NULL;

    char line[XIA_LINE_LEN];


    ASSERT(fip != NULL);

    fippi = (Fippi_Info *)fip;

    ASSERT(fippi->data != NULL);

    sprintf(info_string, "Preparing to parse the FPGA configuration '%s'",
            fippi->filename);
    dxp_log_info("dxp_get_fpgaconfig", info_string);

    fp = xia_find_file(fippi->filename, "r");

    if (!fp) {
        sprintf(info_string, "Unable to open FPGA configuration '%s'",
                fippi->filename);
        dxp_log_error("dxp_get_fpgaconfig", info_string, DXP_OPEN_FILE);
        return DXP_OPEN_FILE;
    }

    n_data = 0;

    /* This is the main loop to parse in the FPGA configuration file */
    while (stj_md_fgets(line, XIA_LINE_LEN, fp) != NULL) {

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


/*
 * Check that the specified FPGA has been downloaded correctly.
 */
static int dxp_download_fpga_done(int* modChan, char *name, Board *board)
{
    UNUSED(modChan);
    UNUSED(name);
    UNUSED(board);

    return DXP_SUCCESS;
}


/*
 * Download the specified DSP.
 *
 * Since the Stj board only has a single DSP on it, this routine ignores
 * the value of modChan.
 */
static int dxp_download_dspconfig(int* ioChan, int* modChan, Board *board)
{
    int status;
    int j;

    unsigned long i = 0;
    unsigned long data;
    unsigned long transfer_ct = 0;
    unsigned long n_transfers = 0;

    Dsp_Info *system_dsp = board->system_dsp;


    ASSERT(board  != NULL);
    ASSERT(ioChan != NULL);


    status = dxp_reset_dsp(*ioChan);

    if (status != DXP_SUCCESS) {
        dxp_log_error("dxp_download_dspconfig", "Error reseting the DSP", status);
        return status;
    }

    /* The entire program memory is stored in this DSP structure, so we
     * only need to set the base address and then write everything.
     */
    status = dxp_write_global_register(*ioChan, STJ_REG_TAR, STJ_PROGRAM_MEMORY);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error setting Transfer Address to %#lx prior to "
                "downloading DSP code", STJ_PROGRAM_MEMORY);
        dxp_log_error("dxp_download_dspconfig", info_string, status);
        return status;
    }

    /* Reassemble the 16 bit words into a single 32-bit value for transfer to
     * the hardware.
     */
    sprintf(info_string, "system_dsp->proglen = %lu", system_dsp->proglen);
    dxp_log_debug("dxp_download_dspconfig", info_string);

    /* Due to issues with the DSP Host port transfer, we verify that the
     * DSP code downloaded completely and (if necessary) re-download it.
     */
    n_transfers = (unsigned long)(system_dsp->proglen / 2);
    for (j = 0, transfer_ct = 0;
            j < 1; /* (j < MAX_NUM_DSP_RETRY)  && (transfer_ct != n_transfers); */
            j++)
    {
        sprintf(info_string, "Attempt #%d at downloading DSP code for "
                "ioChan = %d", j + 1, *ioChan);
        dxp_log_info("dxp_download_dspconfig", info_string);

        for (i = 0; i < system_dsp->proglen; i += 2) {
            /* The program memory words are only 24-bits, so we intentionally set
             * bits 24-31 to 0.
             */
            /*	ASSERT((system_dsp->data[i + 1] & 0xFF00) >> 8 == 0x00); */

            data = (unsigned long)(system_dsp->data[i + 1] << 16) |
                   system_dsp->data[i];

            status = dxp_write_global_register(*ioChan, STJ_REG_TDR, data);

            if (status != DXP_SUCCESS) {
                sprintf(info_string, "Error writing %#lx to address %#lx in DSP "
                        "Program Memory", data, (unsigned long)(i / 2));
                dxp_log_error("dxp_download_dspconfig", info_string, status);
                return status;
            }
        }

    }

    if (j ==  MAX_NUM_DSP_RETRY) {
        sprintf(info_string, "Unable to completely download the DSP code in "
                "%d tries (TCR = %lu) for ioChan = %d", MAX_NUM_DSP_RETRY,
                transfer_ct, *ioChan);
        dxp_log_error("dxp_download_dspconfig", info_string, DXP_DSP_RETRY);
        return DXP_DSP_RETRY;
    }

    sprintf(info_string, "Downloaded %lu 16-bit words to System DSP", i);
    dxp_log_debug("dxp_download_dspconfig", info_string);

    status = dxp_boot_dsp(*ioChan, *modChan, board);

    if (status != DXP_SUCCESS) {
        dxp_log_error("dxp_download_dspconfig", "Error booting DSP", status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Sets the maximum FiPPI size.
 */
static int dxp_get_fipinfo(void* fippi)
{
    ((Fippi_Info *)fippi)->maxproglen = MAXFIP_LEN;

    return DXP_SUCCESS;
}

/*
 * Get maximum sizes for the DSP program.
 */
static int dxp_get_dspinfo(Dsp_Info* dsp)
{
    dsp->params->maxsym    = MAXSYM;
    dsp->params->maxsymlen = MAX_DSP_PARAM_NAME_LEN;
    dsp->maxproglen        = MAXDSP_LEN;

    return DXP_SUCCESS;
}


/*
 * Parses the DSP code into the symbol table and the program data.
 *
 * The Stj hardware uses .dsx files as opposed to the .hex files used with
 * the 2X, Saturn and microDXP hardware. Currently, we ignore the DATA MEMORY
 * section, though we may choose to download that section at a later date.
 *
 * This routine must load the DSP parameter symbol table and the program memory
 * data.
 */
XERXES_STATIC int dxp_get_dspconfig(Dsp_Info* dsp)
{
    int status;


    ASSERT(dsp != NULL);


    dsp->params->maxsym    = MAXSYM;
    dsp->params->maxsymlen = MAX_DSP_PARAM_NAME_LEN;
    dsp->maxproglen        = MAXDSP_LEN;
    dsp->params->nsymbol   = 0;
    dsp->proglen           = 0;

    status = dxp_load_symbols_from_file(dsp->filename, dsp->params);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error loading symbols from %s", dsp->filename);
        dxp_log_error("dxp_get_dspconfig", info_string, status);
        return status;
    }

    status = dxp_load_dsp_code_from_file(dsp->filename, dsp);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error loading DSP code from %s", dsp->filename);
        dxp_log_error("dxp_get_dspconfig", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Reads the actual DSP code from a .dsx file
 *
 * Ignores all of the .dsx header information and just parses in the
 * PROGRAM MEMORY section of the .dsx file. In the future, this program
 * may also parse in the DATA MEMORY section as well.
 */
XERXES_STATIC int dxp_load_dsp_code_from_file(char *file, Dsp_Info *dsp)
{
    size_t i, n_chars = 0;

    unsigned long n_data_words = 0;

    unsigned short hi = 0x0000;
    unsigned short lo = 0x0000;

    char line[82];

    FILE *fp = NULL;


    ASSERT(file       != NULL);
    ASSERT(dsp        != NULL);
    ASSERT(dsp-> data != NULL);


    fp = xia_find_file(file, "r");

    if (!fp) {
        sprintf(info_string, "Error opening %s while trying to load DSP code",
                file);
        dxp_log_error("dxp_load_dsp_code_from_file", info_string, DXP_OPEN_FILE);
        return DXP_OPEN_FILE;
    }

    while (stj_md_fgets(line, sizeof(line), fp) != NULL) {

        /* Skip comments */
        if (line[0] == '*') {
            continue;
        }

        if (STRNEQ(line, "@PROGRAM MEMORY@")) {
            while (stj_md_fgets(line, sizeof(line), fp)) {

                n_chars = strlen(line);

                for (i = 0; (i < n_chars) && isxdigit((int)line[i]); i += 8) {
                    sscanf(&line[i], "%4hx%4hx", &hi, &lo);

                    dsp->data[n_data_words++] = lo;
                    dsp->data[n_data_words++] = hi;
                }
            }

            dsp->proglen = n_data_words;

            sprintf(info_string, "DSP Code length = %lu", dsp->proglen);
            dxp_log_debug("dxp_load_dsp_code_from_file", info_string);

            xia_file_close(fp);

            return DXP_SUCCESS;
        }
    }

    xia_file_close(fp);

    sprintf(info_string, "Malformed DSX file '%s' is missing '@PROGRAM MEMORY@' "
            "section", file);
    dxp_log_error("dxp_load_dsp_code_from_file", info_string, DXP_MALFORMED_FILE);
    return DXP_MALFORMED_FILE;
}


/*
 * Reads the symbols from a .dsx file.
 *
 * Parses in the sections of the .dsx file that contains the DSP parameter
 * information, including the offsets and the per-channel parameters.
 */
XERXES_STATIC int dxp_load_symbols_from_file(char *file, Dsp_Params *params)
{
    int i;

    unsigned short n_globals  = 0;
    unsigned short n_per_chan = 0;

    unsigned long global_offset = 0;
    unsigned long offset        = 0;

    char line[82];

    FILE *fp = NULL;


    ASSERT(file   != NULL);
    ASSERT(params != NULL);


    fp = xia_find_file(file, "r");

    if (!fp) {
        sprintf(info_string, "Error opening '%s' while trying to load DSP "
                "parameters", file);
        dxp_log_error("dxp_load_symbols_from_file", info_string, DXP_OPEN_FILE);
        return DXP_OPEN_FILE;
    }

    while (stj_md_fgets(line, sizeof(line), fp) != NULL) {

        /* Skip comment lines */
        if (line[0] == '*') {
            continue;
        }

        /* Ignore lines that don't contain a section header */
        if (line[0] != '@') {
            continue;
        }

        if (STRNEQ(line, "@CONSTANTS@")) {
            sscanf(stj_md_fgets(line, sizeof(line), fp), "%hu", &n_globals);
            sscanf(stj_md_fgets(line, sizeof(line), fp), "%hu", &n_per_chan);

            params->nsymbol            = n_globals;
            params->n_per_chan_symbols = n_per_chan;

            sprintf(info_string, "n_globals = %hu, n_per_chan = %hu", n_globals,
                    n_per_chan);
            dxp_log_debug("dxp_load_symbols_from_file", info_string);

        } else if (STRNEQ(line, "@OFFSETS@")) {
            /* Offsets in the Dsp_Params structure need to be initialized */
            params->chan_offsets = (unsigned long *)stj_md_alloc(32 *
                                                                 sizeof(unsigned long));

            if (!params->chan_offsets) {
                sprintf(info_string, "Error allocating %d bytes for 'params->"
                        "chan_offsets'", 32 * sizeof(unsigned long));
                dxp_log_error("dxp_load_symbols_from_file", info_string, DXP_NOMEM);
                return DXP_NOMEM;
            }

            /* Global DSP parameters are stored by their absolute address. Per
             * channel DSP parameters, since they have 32 unique addresses, are
             * stored as offsets relative to the appropriate channel offset.
             */
            sscanf(stj_md_fgets(line, sizeof(line), fp), "%lx", &global_offset);

            sprintf(info_string, "global_offset = %#lx", global_offset);
            dxp_log_debug("dxp_load_symbols_from_file", info_string);

            for (i = 0; i < 32; i++) {
                sscanf(stj_md_fgets(line, sizeof(line), fp), "%lx",
                       &(params->chan_offsets[i]));

                sprintf(info_string, "chan%d_offset = %#lx", i,
                        params->chan_offsets[i]);
                dxp_log_debug("dxp_load_symbols_from_file", info_string);
            }

        } else if (STRNEQ(line, "@GLOBAL@")) {
            for (i = 0; i < n_globals; i++) {
                sscanf(stj_md_fgets(line, sizeof(line), fp), "%s : %lu",
                       params->parameters[i].pname, &offset);
                params->parameters[i].address = offset + global_offset;

                sprintf(info_string, "Global DSP Parameter: %s, addr = %#x",
                        params->parameters[i].pname, params->parameters[i].address);
                dxp_log_debug("dxp_load_symbols_from_file", info_string);
            }

        } else if (STRNEQ(line, "@CHANNEL@")) {
            for (i = 0; i < n_per_chan; i++) {
                sscanf(stj_md_fgets(line, sizeof(line), fp), "%s : %lu",
                       params->per_chan_parameters[i].pname, &offset);
                params->per_chan_parameters[i].address = offset;

                sprintf(info_string, "Per Channel DSP Parameter: %s, addr = %x",
                        params->per_chan_parameters[i].pname,
                        params->per_chan_parameters[i].address);
                dxp_log_debug("dxp_load_symbols_from_file", info_string);
            }
        }
    }

    xia_file_close(fp);

    return DXP_SUCCESS;
}


/*
 * Get the value of a DSP parameter.
 */
static int dxp_modify_dspsymbol(int* ioChan, int* modChan, char* name,
                                unsigned short* value, Board *board)
{
    int status;

    unsigned long sym_addr = 0;
    unsigned long val      = 0;

    boolean_t is_global;

    Dsp_Info *dsp = board->system_dsp;

    UNUSED(modChan);


    ASSERT(ioChan != NULL);
    ASSERT(name != NULL);
    ASSERT(board != NULL);


    status = dxp_is_symbol_global(name, dsp, &is_global);

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
        status = dxp_get_global_addr(name, dsp, &sym_addr);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Unable to get address for global DSP parameter %s",
                    name);
            dxp_log_error("dxp_modify_dspsymbol", info_string, status);
            return status;
        }

    } else {

        status = dxp_get_channel_addr(name, *modChan, dsp, &sym_addr);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Unable to get address for per-channnel DSP "
                    "parameter %s", name);
            dxp_log_error("dxp_modify_dspsymbol", info_string, status);
            return status;
        }
    }

    val = (unsigned long)(*value);

    status = dxp__write_data_memory(*ioChan, sym_addr, 1, &val);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error writing '%s' to ioChan = %d", name, *ioChan);
        dxp_log_error("dxp_modify_dspsymbol", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Read a single parameter of the DSP.
 *
 * modChan is only used if the parameter is found to be a channel-specific
 * parameter.
 *
 * Pass the symbol name, module
 * pointer and channel number.  Returns the value read using the variable value.
 * If the symbol name has the 0/1 dropped from the end, then the 32-bit
 * value is created from the 0/1 contents.  e.g. zigzag0 and zigzag1 exist
 * as a 32 bit number called zigzag.  if this routine is called with just
 * zigzag, then the 32 bit number is returned, else the 16 bit value
 * for zigzag0 or zigzag1 is returned depending on what was passed as *name.
 */
static int dxp_read_dspsymbol(int *ioChan, int *modChan, char *name,
                              Board *board, double *value)
{
    int status;

    unsigned long sym_addr = 0;
    unsigned long val      = 0;

    boolean_t is_global;

    Dsp_Info *dsp = board->system_dsp;


    ASSERT(ioChan != NULL);
    ASSERT(name != NULL);
    ASSERT(board != NULL);


    status = dxp_is_symbol_global(name, dsp, &is_global);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error determining if %s is a global parameter or not",
                name);
        dxp_log_error("dxp_read_dspsymbol", info_string, status);
        return status;
    }

    /* The address is calculated differently depending if the parameter is a
     * global parameter or a per-channel parameter.
     */
    if (is_global) {

        status = dxp_get_global_addr(name, dsp, &sym_addr);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Unable to get address for global DSP parameter %s",
                    name);
            dxp_log_error("dxp_read_dspsymbol", info_string, status);
            return status;
        }

    } else {

        status = dxp_get_channel_addr(name, *modChan, dsp, &sym_addr);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Unable to get address for per-channnel DSP "
                    "parameter %s", name);
            dxp_log_error("dxp_read_dspsymbol", info_string, status);
            return status;
        }
    }

    sym_addr += STJ_DATA_MEMORY;

    status = dxp_write_global_register(*ioChan, STJ_REG_TAR, sym_addr);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error setting Transfer Address Register to Data "
                "Memory (%#lx)", STJ_DATA_MEMORY);
        dxp_log_error("dxp_read_dspsymbol", info_string, status);
        return status;
    }

    status = dxp_read_global_register(*ioChan, STJ_REG_TDR, &val);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading %s from ioChan %d", name, *ioChan);
        dxp_log_error("dxp_read_dspsymbol", info_string, status);
        return status;
    }

    *value = (double)val;

    return DXP_SUCCESS;

}


/*
 * Get per-channel symbol address.
 *
 * Calculates the absolute address of the per-channel which, unlike global
 * parameters, requires the addition of the relative offset and the channel
 * base address.
 */
static int dxp_get_channel_addr(char *name, int modChan, Dsp_Info *dsp,
                                unsigned long *addr)
{
    unsigned short i;


    ASSERT(name != NULL);
    ASSERT(dsp != NULL);
    ASSERT(addr != NULL);
    ASSERT(modChan >=0 && modChan < 32);


    for (i = 0; i < dsp->params->n_per_chan_symbols; i++) {
        if (STREQ(name, dsp->params->per_chan_parameters[i].pname)) {

            *addr = dsp->params->per_chan_parameters[i].address +
                    dsp->params->chan_offsets[modChan];

            return DXP_SUCCESS;
        }
    }

    sprintf(info_string, "Unknown symbol '%s' in per-channel DSP parameter list",
            name);
    dxp_log_error("dxp_get_channel_addr", info_string, DXP_NOSYMBOL);
    return DXP_NOSYMBOL;
}


/*
 * Get global symbol address.
 *
 * Returns an error if the symbol isn't global (or a symbol at all). The
 * address is a global address and can be used directly with the I/O routines.
 */
static int dxp_get_global_addr(char *name, Dsp_Info *dsp, unsigned long *addr)
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

    sprintf(info_string, "Unknown symbol '%s' in global DSP parameter list",
            name);
    dxp_log_error("dxp_get_global_addr", info_string, DXP_NOSYMBOL);
    return DXP_NOSYMBOL;
}


/*
 * Determines if the symbol is a global DSP symbol or a per-channel
 * symbol.
 *
 * This routine does not indicate definitively that the parameter is an
 * actual parameter, just that it is or isn't a global parameter.
 */
static int dxp_is_symbol_global(char *name, Dsp_Info *dsp, boolean_t *is_global)
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


/*
 * Readout the parameter memory for a single channel.
 *
 * This routine reads the parameter list from the DSP pointed to by ioChan and
 * modChan.  It returns the array to the caller.
 */
static int dxp_read_dspparams(int* ioChan, int* modChan, Board *b,
                              unsigned short* params)
{
    int status;

    unsigned int i;
    unsigned int offset = 0;

    unsigned long p    = 0;
    unsigned long addr = 0x0;


    ASSERT(ioChan != NULL);
    ASSERT(b != NULL);
    ASSERT(params != NULL);


    /* Read two separate blocks: the global block and the per-channel block. */

    for (i = 0; i < PARAMS(b)->nsymbol; i++) {
        addr = (unsigned long)PARAMS(b)->parameters[i].address |
               (unsigned long)STJ_DATA_MEMORY;

        status = dxp_write_global_register(*ioChan, STJ_REG_TAR, addr);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error writing Transfer Address Register prior "
                    "to reading a DSP parameter for ioChan = %d", *ioChan);
            dxp_log_error("dxp_read_dspparams", info_string, status);
            return status;
        }

        status = dxp_read_global_register(*ioChan, STJ_REG_TDR, &p);

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
               (unsigned long)STJ_DATA_MEMORY;

        status = dxp_write_global_register(*ioChan, STJ_REG_TAR, addr);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error writing Transfer Address Register prior "
                    "to reading a DSP parameter for ioChan = %d", *ioChan);
            dxp_log_error("dxp_read_dspparams", info_string, status);
            return status;
        }

        status = dxp_read_global_register(*ioChan, STJ_REG_TDR, &p);

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


/*
 * Write an array of DSP parameters to the specified DSP.
 *
 * The meaning of 'the specified DSP' will need to be clarified for the
 * Stj hardware since it only has a single DSP chip.
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


/*
 * Gets the length of the spectrum memory buffer.
 */
XERXES_STATIC int dxp_get_spectrum_length(int *ioChan, int *modChan,
                                          Board *board, unsigned int *len)
{
    int status;

    double MCALIMLO = 0.0;
    double MCALIMHI = 0.0;


    ASSERT(ioChan != NULL);
    ASSERT(modChan != NULL);
    ASSERT(board != NULL);
    ASSERT(len != NULL);


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


/*
 * Gets the length of the baseline memory buffer.
 *
 * The baseline length is defined as the DSP parameter BASELEN.
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


/*
 * Gets the length of the event memory buffer.
 */
static unsigned int dxp_get_event_length(Dsp_Info* dsp, unsigned short* params)
{
    UNUSED(dsp);
    UNUSED(params);

    return 0;
}


/*
 * Reads the spectrum memory for a single channel.
 *
 * Returns the MCA spectrum for a single channel using brute force to
 * read only the memory for the requested channel. This method is inefficient
 * by design. When we add other ways to read out the spectrum I will add
 * references to them here.
 */
static int dxp_read_spectrum(int* ioChan, int* modChan, Board* board,
                             unsigned long* spectrum)
{
    int status;

    unsigned long addr;

    unsigned int spectrum_len;


    ASSERT(ioChan != NULL);
    ASSERT(modChan != NULL);
    ASSERT(board != NULL);


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

    status = dxp__burst_read_block(*ioChan, *modChan, addr, spectrum_len,
                                   spectrum);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading spectrum for ioChan = %d", *ioChan);
        dxp_log_error("dxp_read_spectrum", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Returns the offset in the external memory of the selected module
 * channel.
 *
 * This routine verifies that the addr falls on a block boundary as specified
 * in the external memory documentation.
 */
static int dxp__get_mca_chan_addr(int ioChan, int modChan, Board *board,
                                  unsigned long *addr)
{
    int i;
    int status;

    unsigned int mca_len = 0;
    unsigned long total_len = 0;

    ASSERT(addr != NULL);
    ASSERT(board != NULL);

    /* Calculate the external memory address for the specified module channel by
     * summing the lengths of the channels that precede it.
     */
    for (i = 0, total_len = 0; i < modChan; i++) {
        status = dxp_get_spectrum_length(&ioChan, &i, board, &mca_len);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error reading MCA spectrum length for ioChan = %d",
                    ioChan);
            dxp_log_error("dxp__get_mca_chan_addr", info_string, status);
            return status;
        }

        total_len += mca_len;
    }

    if ((total_len % STJ_MEMORY_BLOCK_SIZE) != 0) {
        sprintf(info_string, "Total MCA length (%lu) of channels prior to module "
                "channel %d is not a multiple of the memory block size (%u)",
                total_len, modChan, STJ_MEMORY_BLOCK_SIZE);
        dxp_log_error("dxp__get_mca_chan_addr", info_string, DXP_MEMORY_BLK_SIZE);
        return DXP_MEMORY_BLK_SIZE;
    }

    /* Add one block for the statistics */
    *addr = total_len + STJ_MCA_DATA_OFFSET;

    return DXP_SUCCESS;
}


/*
 * Reads the baseline memory for a single channel.
 */
static int dxp_read_baseline(int *ioChan, int *modChan, Board *board,
                             unsigned long *baseline)
{
    int status;

    double BASESTART = 0.0;
    double BASELEN   = 0.0;

    unsigned long buffer_addr = 0;


    ASSERT(board != NULL);


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

    buffer_addr = (unsigned long)BASESTART + STJ_DATA_MEMORY;

    status = dxp__read_block(*ioChan, buffer_addr, (unsigned int)BASELEN,
                             baseline);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading baseline histogram from %#lx for "
                "ioChan = %d", buffer_addr, *ioChan);
        dxp_log_error("dxp_read_baseline", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Starts data acquisition.
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
        status = dxp_clear_csr_bit(*ioChan, STJ_CSR_RESET_MCA);

    } else {
        status = dxp_set_csr_bit(*ioChan, STJ_CSR_RESET_MCA);
    }

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error setting the Reset MCA bit while trying to "
                "start a run on ioChan = %d", *ioChan);
        dxp_log_error("dxp_begin_run", info_string, status);
        return status;
    }

    sprintf(info_string, "Starting a run on ioChan = %d", *ioChan);
    dxp_log_debug("dxp_begin_run", info_string);

    status = dxp_set_csr_bit(*ioChan, STJ_CSR_RUN_ENA);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error setting the Run Enable bit while trying to "
                "start a run on ioChan = %d", *ioChan);
        dxp_log_error("dxp_begin_run", info_string, status);
        return status;
    }

    *id = gid++;

    return DXP_SUCCESS;
}


/*
 * Stops data acquisition.
 */
static int dxp_end_run(int* ioChan, int* modChan, Board *board)
{
    int status;

    UNUSED(modChan);


    ASSERT(ioChan != NULL);
    ASSERT(board != NULL);


    sprintf(info_string, "Ending a run on ioChan = %d", *ioChan);
    dxp_log_debug("dxp_end_run", info_string);

    status = dxp_clear_csr_bit(*ioChan, STJ_CSR_RUN_ENA);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error clearing Run Enable bit while trying to stop a "
                "run on ioChan = %d", *ioChan);
        dxp_log_error("dxp_end_run", info_string, status);
        return status;
    }

    status = dxp_wait_for_busy(*ioChan, *modChan, 0, 10.0, board);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error waiting for the run to end on ioChan = %d",
                *ioChan);
        dxp_log_error("dxp_end_run", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Determines if a run is active on the specified channel.
 *
 * active == 1 -> Run active
 */
static int dxp_run_active(int* ioChan, int* modChan, int* active)
{
    int status;

    unsigned long CSR = 0x0;

    UNUSED(modChan);
    UNUSED(active);


    status = dxp_read_global_register(*ioChan, STJ_REG_CSR, &CSR);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading Control Status Register for ioChan = %d",
                *ioChan);
        dxp_log_error("dxp_run_active", info_string, status);
        return status;
    }

    if (CSR & (0x1 << STJ_CSR_RUN_ACT_BIT)) {
        *active = 1;

    } else {
        *active = 0;
    }

    return DXP_SUCCESS;
}


/*
 * Starts a control task.
 *
 * Starts a control task of the specified type.
 */
XERXES_STATIC int dxp_begin_control_task(int* ioChan, int* modChan, short *type,
                                         unsigned int *length, int *info, Board *board)
{
    int status;
    int i;

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
                sprintf(info_string, "Error doing control task type %hd for ioChan = "
                        "%d", *type, *ioChan);
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


/*
 * End a control task.
 */
static int dxp_end_control_task(int* ioChan, int* modChan, Board *board)
{
    int status;

    parameter_t RUNTYPE = STJ_RUNTYPE_NORMAL;


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
        sprintf(info_string, "Error setting RUNTYPE back to normal (%#hx) for "
                "ioChan = %d", RUNTYPE, *ioChan);
        dxp_log_error("dxp_end_control_task", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Retrieve the current control task parameters.
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


/*
 * Get the control task data.
 */
static int dxp_control_task_data(int* ioChan, int* modChan, short *type,
                                 Board *board, void *data)
{
    int status;
    int i;


    for (i = 0; i < N_ELEMS(CONTROL_TASK_DATAS); i++) {
        if (*type == CONTROL_TASK_DATAS[i].type) {
            status = CONTROL_TASK_DATAS[i].fn(*ioChan, *modChan,
                                              (unsigned long *)data, board);

            if (status != DXP_SUCCESS) {
                sprintf(info_string, "Error getting data for control task %hd for "
                        "ioChan = %d", *type, *ioChan);
                dxp_log_error("dxp_control_task_data", info_string, status);
                return status;
            }

            return DXP_SUCCESS;
        }
    }

    sprintf(info_string, "Unknown control task data type: %hd", *type);
    dxp_log_error("dxp_control_task_data", info_string, DXP_UNKNOWN_CT);
    return DXP_UNKNOWN_CT;
}


/*
 * Decode DSP error values.
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


/*
 * Unused for the Stj.
 */
static int dxp_clear_error(int* ioChan, int* modChan, Board *board)
{
    UNUSED(ioChan);
    UNUSED(modChan);
    UNUSED(board);

    return DXP_SUCCESS;
}


/*
 * XERXES layer unused for the Stj.
 */
static int dxp_get_runstats(int *ioChan, int *modChan, Board *b,
                            unsigned long *evts, unsigned long *under,
                            unsigned long *over, unsigned long *fast,
                            unsigned long *base, double *live,
                            double *icr, double *ocr)
{
    UNUSED(ioChan);
    UNUSED(modChan);
    UNUSED(b);
    UNUSED(under);
    UNUSED(over);
    UNUSED(fast);
    UNUSED(base);
    UNUSED(live);
    UNUSED(icr);
    UNUSED(ocr);
    UNUSED(evts);

    return DXP_SUCCESS;
}


/*
 * Read the specified memory from the requested location.
 *
 * The only memory type currently allowed is 'burst'.
 */
XERXES_STATIC int dxp_read_mem(int *ioChan, int *modChan, Board *board,
                               char *name, unsigned long *base,
                               unsigned long *offset, unsigned long *data)
{
    int status;

    UNUSED(board);


    ASSERT(ioChan != NULL);
    ASSERT(name != NULL);

    if (STREQ(name, "burst_map")) {
        status = dxp__burst_read_buffer(*ioChan, *modChan, *base,
                                        (unsigned int)(*offset), data);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error reading mapping buffer for ioChan = %d",
                    *ioChan);
            dxp_log_error("dxp_read_mem", info_string, status);
            return status;
        }

    } else if (STREQ(name, "data")) {

        status = dxp__read_data_memory(*ioChan, *base, *offset, data);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error reading data memory for ioChan = %d", *ioChan);
            dxp_log_error("dxp_read_mem", info_string, status);
            return status;
        }

    } else if (STREQ(name, "burst")) {

        status = dxp__burst_read_block(*ioChan, *modChan, *base,
                                       (unsigned int)(*offset), data);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error burst reading memory block for ioChan = %d",
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


/*
 * Writes the specified memory to the requested address.
 */
XERXES_STATIC int dxp_write_mem(int *ioChan, int *modChan, Board *board,
                                char *name, unsigned long *base,
                                unsigned long *offset, unsigned long *data)
{
    int i;
    int status;

    UNUSED(modChan);
    UNUSED(board);


    ASSERT(ioChan != NULL);
    ASSERT(name   != NULL);
    ASSERT(base   != NULL);
    ASSERT(offset != NULL);
    ASSERT(data   != NULL);


    for (i = 0; i < N_ELEMS(MEMORY_WRITERS); i++) {
        if (STREQ(MEMORY_WRITERS[i].name, name)) {

            status = MEMORY_WRITERS[i].fn(*ioChan, *base, *offset, data);

            if (status != DXP_SUCCESS) {
                sprintf(info_string, "Error writing memory '%s' to ioChan = %d",
                        name, *ioChan);
                dxp_log_error("dxp_write_mem", info_string, status);
                return status;
            }

            return DXP_SUCCESS;
        }
    }

    sprintf(info_string, "Unknown memory type '%s' for ioChan = %d",
            name, *ioChan);
    dxp_log_error("dxp_write_mem", info_string, DXP_UNKNOWN_MEM);
    return DXP_UNKNOWN_MEM;
}


/*
 * Currently unimplemented.
 *
 * Meant to target user-space registers, not hardware developer ones.
 */
static int dxp_write_reg(int *ioChan, int *modChan, char *name,
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

            status = dxp_write_global_register(*ioChan, REGISTER_TABLE[i].addr, *data);

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


/*
 * Reads the specified register.
 *
 * Meant to target user-space registers, not hardware developer ones.
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

            status = dxp_read_global_register(*ioChan, REGISTER_TABLE[i].addr, data);

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


/*
 * Cleans up the communication interface and releases any resources
 * that may have been acquired when the connection was opened.
 */
XERXES_STATIC int XERXES_API dxp_unhook(Board *board)
{
    int status;


    ASSERT(board != NULL);


    sprintf(info_string, "Attempting to unhook ioChan = %d, unhooked = %u",
            board->ioChan, (unsigned int)unhooked);
    dxp_log_debug("dxp_unhook", info_string);

    status = board->iface->funcs->dxp_md_close(&(board->ioChan));

    return DXP_SUCCESS;
}

/*
 * Downloads an FPGA to the selected target on the hardware.
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
 * The Stj has several targetable FPGAs. The selected FPGA is selected by
 * setting the appropriate bits in the target parameter. The constants
 * for allowed targets are defined in xia_stj.h. This routine is smart enough
 * to account for a target that is an OR'd combination of multiple targets.
 */
XERXES_STATIC int dxp_download_fpga(int ioChan, unsigned long target,
                                    Fippi_Info *fpga)
{
    int status;

    unsigned int i;

    unsigned short data;

    unsigned long cfg_status;
    unsigned long j;



    float init_timeout  = .003f;
    float xdone_timeout = .001f;


    ASSERT(fpga != NULL);


#ifdef XIA_INTERNAL_DEBUGGING
    dxp_dump_fpga(fpga);
#endif /* XIA_INTERNAL_DEBUGGING */

    status = dxp_write_global_register(ioChan, STJ_REG_CFG_CONTROL, target);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error writing target '%#lx' to Control Register for "
                "ioChan = %d", target, ioChan);
        dxp_log_error("dxp_download_fpga", info_string, status);
        return status;
    }

    /* The simplest thing to do is to just wait the allowed timeout time,
     * especially if it is short and then see if the value is correct. Future
     * optimizations can focus on writing a special timeout loop.
     */
    stj_md_wait(&init_timeout);

    status = dxp_read_global_register(ioChan, STJ_REG_CFG_STATUS, &cfg_status);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading Status Register for channel %d", ioChan);
        dxp_log_error("dxp_download_fpga", info_string, status);
        return status;
    }

    /* Since the target could potentially be more then one FPGA, we must check
     * the INIT* line for each FPGA that is targeted.
     */
    for (j = 0; j < STJ_NUM_TARGETS; j++) {
        if (target & (1 << j)) {
            if (! (cfg_status & STJ_CFG_STATUS[j][STJ_INIT])) {
                sprintf(info_string, "INIT* line never asserted for Target %lu after "
                        "waiting %f seconds, status register %#lx", j, (double)init_timeout,
                        cfg_status);
                dxp_log_error("dxp_download_fpga", info_string, DXP_FPGA_TIMEOUT);
                return DXP_FPGA_TIMEOUT;
            }
        }
    }

    sprintf(info_string, "Downloaded FPGA target %lu proglen = %u",
            target, fpga->proglen);
    dxp_log_debug("dxp_download_fpga", info_string);

    /* The FPGA data bytes are packed two to a word, so we need to write
     * two of them each time we increment the prog counter.
     */
    for (i = 0; i < fpga->proglen; i++) {
        data = fpga->data[i];

        /*	stj_md_wait(&init_timeout); */

        status = dxp_write_global_register(ioChan, STJ_REG_CFG_DATA, data & 0xFF);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error writing low-byte of FPGA word #%u to channel "
                    "%d", i, ioChan);
            dxp_log_error("dxp_download_fpga", info_string, status);
            return status;
        }

        status = dxp_write_global_register(ioChan, STJ_REG_CFG_DATA,
                                           (data >> 8) & 0xFF);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error writing high-byte of FPGA word#%u to channel "
                    "%d", i, ioChan);
            dxp_log_error("dxp_download_fpga", info_string, status);
            return status;
        }
    }

    stj_md_wait(&xdone_timeout);

    status = dxp_read_global_register(ioChan, STJ_REG_CFG_STATUS, &cfg_status);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading Status Register for channel %d", ioChan);
        dxp_log_error("dxp_download_fpga", info_string, status);
        return status;
    }

    /* See the comment above (where INIT* is checked). */
    for (j = 0; j < STJ_NUM_TARGETS; j++) {
        if (target & (1 << j)) {
            if (! (cfg_status & STJ_CFG_STATUS[j][STJ_XDONE])) {
                sprintf(info_string, "XDONE line never asserted for Target %lu after "
                        "waiting %f seconds, status register %#lx", j, (double)xdone_timeout,
                        cfg_status);
                dxp_log_error("dxp_download_fpga", info_string, DXP_FPGA_TIMEOUT);
                return DXP_FPGA_TIMEOUT;
            }

            /* Init can de-assert if there is a CRC error after the last FPGA bit has
             * been clocked-in.
             */
            if (! (cfg_status & STJ_CFG_STATUS[j][STJ_INIT])) {
                sprintf(info_string, "CRC error after downloading '%s'",
                        STJ_FPGA_NAMES[j]);
                dxp_log_error("dxp_download_fpga", info_string, DXP_FPGA_CRC);
                return DXP_FPGA_CRC;
            }
        }
    }

    return DXP_SUCCESS;
}


/*
 * Downloads all of the FPGAs on the module
 */
XERXES_STATIC int dxp_download_all_fpgas(int ioChan, int modChan, Board *board)
{
    int status;


    ASSERT(board != NULL);


    dxp_log_debug("dxp_download_all_fpgas", "Preparing to download all FPGAs to "
                  "the hardware");

    status = dxp_download_fpga(ioChan, STJ_CONTROL_SYS_FPGA, board->system_fpga);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error downloading System FPGA while downloading "
                "all of the FPGAs to channel %d", ioChan);
        dxp_log_error("dxp_download_all_fpgas", info_string, status);
        return status;
    }

    /* This assumes that FiPPI A and FiPPI B are using the same FiPPI. In the
     * future, we may loosen this constraint.
     */
    status = dxp_download_fippis(ioChan, modChan, board);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error downloading FiPPI A and FiPPI B while "
                "downloading all of the FPGAs to channel %d", ioChan);
        dxp_log_error("dxp_download_all_fpgas", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Write the specified value to the specified register on the hardware.
 *
 * This routine is basically a wrapper around the low-level I/O routine.
 */
XERXES_STATIC int dxp_write_global_register(int ioChan, unsigned long reg,
                                            unsigned long val)
{
    int status;

    unsigned int io_len  = 1;
    unsigned int io_type = STJ_IO_SINGLE_WRITE;




    status = stj_md_io(&ioChan, &io_type, &reg, (void *)&val, &io_len);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error writing %#lx to address %#lx for channel %d",
                val, reg, ioChan);
        dxp_log_error("dxp_write_global_register", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Read 32-bits from the specified register.
 */
XERXES_STATIC int dxp_read_global_register(int ioChan, unsigned long reg,
                                           unsigned long *val)
{
    int status;

    unsigned int io_len  = 1;
    unsigned int io_type = STJ_IO_SINGLE_READ;




    ASSERT(val != NULL);


    status = stj_md_io(&ioChan, &io_type, &reg, (void *)val, &io_len);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading a long word from address %#lx for "
                "channel %d", reg, ioChan);
        dxp_log_error("dxp_read_global_register", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Issues the proper command to the hardware to reset the DSP.
 */
XERXES_STATIC int dxp_reset_dsp(int ioChan)
{
    int status;

    float wait = .001f;


    sprintf(info_string, "Performing DSP reset for ioChan = %d", ioChan);
    dxp_log_debug("dxp_reset_dsp", info_string);

    status = dxp_set_csr_bit(ioChan, STJ_DSP_RESET_BIT);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error setting bit %d in the CSR to reset the DSP",
                STJ_DSP_RESET_BIT);
        dxp_log_error("dxp_reset_dsp", info_string, status);
        return status;
    }

    /* The hardware needs a millisecond to finish configuring */
    stj_md_wait(&wait);

    return DXP_SUCCESS;
}


/*
 * Instructs the hardware to boot the DSP code
 *
 * This routine should be called after the DSP code words have been
 * downloaded to the hardware. first_time should be set to true if
 * this is the first "cold" boot of the DSP.
 */
XERXES_STATIC int dxp_boot_dsp(int ioChan, int modChan, Board *b)
{
    int status;

    unsigned long dsp_active;

    float wait = 0.001f;
    float elapsed = 0.0;


    ASSERT(b != NULL);


    sprintf(info_string, "Performing DSP boot for ioChan = %d", ioChan);
    dxp_log_debug("dxp_boot_dsp", info_string);

    status = dxp_set_csr_bit(ioChan, STJ_DSP_BOOT_BIT);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error setting bit %d in the CSR to boot the DSP",
                STJ_DSP_BOOT_BIT);
        dxp_log_error("dxp_boot_dsp", info_string, status);
        return status;
    }

    do {
        unsigned long csr;

        stj_md_wait(&wait);
        elapsed += wait;

        status = dxp_read_global_register(ioChan, STJ_REG_CSR, &csr);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error waiting for DSP to signal that it is "
                    "active for ioChan = %d.", ioChan);
            dxp_log_error("dxp_boot_dsp", info_string, status);
            return status;
        }

        dsp_active = csr & (1 << STJ_CSR_DSP_ACT_BIT);

    } while (!dsp_active && elapsed < STJ_DSP_BOOT_ACTIVE_TIMEOUT);

    if (elapsed >= STJ_DSP_BOOT_ACTIVE_TIMEOUT) {
        sprintf(info_string, "Timeout (%0.1f s) waiting for DSP Active bit in "
                "the CSR to be set for ioChan = %d.", elapsed, ioChan);
        dxp_log_error("dxp_boot_dsp", info_string, DXP_TIMEOUT);
        return DXP_TIMEOUT;
    }

    /* Once we have established that the DSP is active, it is safe to
     * poll waiting for busy. Extra long wait for the STJ time out
     */
    status = dxp_wait_for_busy(ioChan, modChan, 0, 10.0, b);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error waiting for DSP to boot for ioChan = %d",
                ioChan);
        dxp_log_error("dxp_boot_dsp", info_string, status);
        return status;
    }

    /* If this is a full system reboot (all firmware updated), then we
     * need to initialize the data memory.
     */
    if (b->is_full_reboot) {
        parameter_t INITIALIZE = 1;

        status = dxp_modify_dspsymbol(&ioChan, &modChan, "INITIALIZE",
                                      &INITIALIZE, b);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error initializing data memory during "
                    "the first boot of the DSP for ioChan = %d.", ioChan);
            dxp_log_error("dxp_boot_dsp", info_string, status);
            return status;
        }

        status = dxp_do_apply(ioChan, modChan, b);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error doing apply run to initialize the data "
                    "memory for the DSP, ioChan = %d.", ioChan);
            dxp_log_error("dxp_boot_dsp", info_string, status);
            return status;
        }
    }


    return DXP_SUCCESS;
}


/*
 * Set the specified bit in the Control Status Register.
 *
 * Uses the read/modify/write idiom to set the selected bit in the Control
 * Status Register.
 */
XERXES_STATIC int dxp_set_csr_bit(int ioChan, byte_t bit)
{
    int status;

    unsigned long val;


    if (bit > 31) {
        sprintf(info_string, "Specified bit '%u' is larger then the maximum bit "
                "'31' in the CSR", (unsigned int)bit);
        dxp_log_error("dxp_set_csr_bit", info_string, DXP_BAD_BIT);
        return DXP_BAD_BIT;
    }

    status = dxp_read_global_register(ioChan, STJ_REG_CSR, &val);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading CSR for ioChan = %d", ioChan);
        dxp_log_error("dxp_set_csr_bit", info_string, status);
        return status;
    }

    val |= (0x1 << bit);

    sprintf(info_string, "Setting CSR to %#lx", val);
    dxp_log_debug("dxp_set_csr_bit", info_string);

    status = dxp_write_global_register(ioChan, STJ_REG_CSR, val);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error writing %#lx to the CSR for ioChan = %d", val,
                ioChan);
        dxp_log_error("dxp_set_csr_bit", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Clear the specified bit in the Control Status Register.
 *
 * Uses the read/modify/write idiom to clear the selected bit in the Control
 * Status Register.
 */
XERXES_STATIC int dxp_clear_csr_bit(int ioChan, byte_t bit)
{
    int status;

    unsigned long val;


    if (bit > 31) {
        sprintf(info_string, "Specified bit '%u' is larger then the maximum bit "
                "'31' in the CSR", (unsigned int)bit);
        dxp_log_error("dxp_clear_csr_bit", info_string, DXP_BAD_BIT);
        return DXP_BAD_BIT;
    }

    status = dxp_read_global_register(ioChan, STJ_REG_CSR, &val);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading CSR for ioChan = %d", ioChan);
        dxp_log_error("dxp_clear_csr_bit", info_string, DXP_BAD_BIT);
        return DXP_BAD_BIT;
    }

    val &= ~(0x1 << bit);

    sprintf(info_string, "Setting CSR to %#lx", val);
    dxp_log_debug("dxp_clear_csr_bit", info_string);

    status = dxp_write_global_register(ioChan, STJ_REG_CSR, val);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error writing %#lx to the CSR for ioChan = %d", val,
                ioChan);
        dxp_log_error("dxp_clear_csr_bit", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Start the ADC trace special run.
 */
static int dxp_begin_adc_trace(int ioChan, int modChan, Board *board)
{
    int status;

    ASSERT(board != NULL);

    status = dxp__do_trace(ioChan, modChan, board);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error starting ADC trace for ioChan = %d", ioChan);
        dxp_log_error("dxp_begin_adc_trace", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Wait for BUSY to go to the desired value.
 *
 * If BUSY does not go to desired within timeout seconds then an error is
 * returned.
 */
static int dxp_wait_for_busy(int ioChan, int modChan, parameter_t desired,
                             double timeout, Board *board)
{
    int status;
    int n_polls = 0;
    int i;

    float wait = .01f;

    double BUSY = 0;


    ASSERT(board != NULL);


    n_polls = (int)ROUND(timeout / wait);

    sprintf(info_string, "Max. polls for waiting for BUSY = %d", n_polls);
    dxp_log_debug("dxp_wait_for_busy", info_string);

    for (i = 0; i < n_polls; i++) {

        status = dxp_read_dspsymbol(&ioChan, &modChan, "BUSY", board, &BUSY);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error reading BUSY during poll iteration %d while "
                    "waiting for BUSY to go to %hu on ioChan = %d", i, desired,
                    ioChan);
            dxp_log_error("dxp_wait_for_busy", info_string, status);
            return status;
        }

        if (BUSY == (double)desired) {
            return DXP_SUCCESS;
        }

        stj_md_wait(&wait);
    }

    sprintf(info_string, "Timeout waiting for BUSY to go to %hu (current = "
            "%0.0f) on ioChan = %d", desired, BUSY, ioChan);
    dxp_log_error("dxp_wait_for_busy", info_string, DXP_TIMEOUT);
    return DXP_TIMEOUT;
}


/*
 * Read the ADC trace from the hardware
 */
static int dxp_get_adc_trace(int ioChan, int modChan, unsigned long *data,
                             Board *board)
{
    int status;

    unsigned long buffer_addr = 0;

    double TRACELEN   = 0.0;

    ASSERT(board != NULL);

    status = dxp_read_dspsymbol(&ioChan, &modChan, "TRACELEN", board, &TRACELEN);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading TRACELEN from ioChan = %d", ioChan);
        dxp_log_error("dxp_get_adc_trace", info_string, status);
        return status;
    }

    buffer_addr = (unsigned long)STJ_TRACE_OFFSET +
                  STJ_TRACE_CHAN_OFFSET * modChan;

    sprintf(info_string, "TRACELEN = %f, buffer address = %#lx", TRACELEN,
            buffer_addr);
    dxp_log_debug("dxp_get_adc_trace", info_string);

    status = dxp__burst_read_block(ioChan, modChan, buffer_addr,
                                   (unsigned int)TRACELEN, data);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading ADC trace from %#lx for ioChan = %d",
                buffer_addr, ioChan);
        dxp_log_error("dxp_get_adc_trace", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Triggers a special run in the DSP to apply settings.
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
 * Apply is a global operation so it needs to only be done once per module.
 */
static int dxp_do_apply(int ioChan, int modChan, Board *board)
{
    int status;
    int active;
    int n_polls;
    int i;

    double timeout    = 1.0;

    float poll_interval = 0.1f;

    ASSERT(board != NULL);

    status = dxp__start_special_run(ioChan, modChan, board, STJ_SPECIALRUN_APPLY);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error starting apply special run on ioChan = %d",
                ioChan);
        dxp_log_error("dxp_do_apply", info_string, status);
        return status;
    }

    /* We have an approximate timeout here since we don't include the
     * time it takes to call dxp__run_enable_active() in the calculation.
     */
    n_polls = (int)ROUND(timeout / (double)poll_interval);

    sprintf(info_string, "n_polls = %d, timeout = %0.1f, poll_interval = %0.1f",
            n_polls, (double)timeout, (double)poll_interval);
    dxp_log_debug("dxp_do_apply", info_string);

    for (i = 0; i < n_polls; i++) {
        status = dxp__run_enable_active(ioChan, &active);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error reading run enable for ioChan = %d", ioChan);
            dxp_log_error("dxp_do_apply", info_string, status);
            return status;
        }

        if (!active) {
            /* The apply is complete. */
            break;
        }

        stj_md_wait(&poll_interval);
    }

    if (i == n_polls) {
        status = dxp_end_run(&ioChan, &modChan, board);
        sprintf(info_string, "Timeout waiting %0.1f second(s) for the apply run "
                "to complete on ioChan = %d", (double)timeout, ioChan);
        dxp_log_error("dxp_do_apply", info_string, DXP_TIMEOUT);
        return DXP_TIMEOUT;
    }

    return DXP_SUCCESS;
}


/*
 * Do a special run to try to set all 32 channels Offset DACs
 * such that their ADC average = ADC Offset value
 */
static int dxp__adjust_offsets(int ioChan, int modChan, Board *board)
{
    int status;

    status = dxp__do_special_run(ioChan, modChan, board,
                                 STJ_SPECIALRUN_ADJUST_OFFSETS, 10.0);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error doing special run on ioChan = %d", ioChan);
        dxp_log_error("dxp__adjust_offsets", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Do a special run to set bias dac for all channels
 */
static int dxp__bias_set_dac(int ioChan, int modChan, Board *board)
{
    int status;

    status = dxp__do_special_run(ioChan, modChan, board,
                                 STJ_SPECIALRUN_BIAS_SET_DAC, 10.0);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error doing special run on ioChan = %d", ioChan);
        dxp_log_error("dxp__bias_set_dac", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Do a special run steps the STJ Bias DACs for all
 * and acquires the average preamp output voltage
 */
static int dxp__bias_scan(int ioChan, int modChan, Board *board)
{
    int status;

    ASSERT(board != NULL);

    status = dxp__start_special_run(ioChan, modChan, board,
                                    STJ_SPECIALRUN_BIAS_SCAN);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error starting bias scan special run on ioChan = %d",
                ioChan);
        dxp_log_error("dxp__bias_scan", info_string, status);
        return status;
    }

    /* Don't wait for BUSY so that we can keep reading out data */

    return DXP_SUCCESS;
}


static int dxp__start_special_run(int ioChan, int modChan, Board *board,
                                  parameter_t specialRun)
{
    int status;
    int id = 0;

    unsigned short ignored = 0;

    parameter_t RUNTYPE    = STJ_RUNTYPE_SPECIAL;

    ASSERT(board != NULL);

    status = dxp_modify_dspsymbol(&ioChan, &modChan, "RUNTYPE", &RUNTYPE, board);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error setting RUNTYPE to special run for ioChan = %d",
                ioChan);
        dxp_log_error("dxp__start_special_run", info_string, status);
        return status;
    }

    status = dxp_modify_dspsymbol(&ioChan, &modChan, "SPECIALRUN", &specialRun,
                                  board);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error setting SPECIALRUN to adjust offset "
                "ioChan = %d", ioChan);
        dxp_log_error("dxp__start_special_run", info_string, status);
        return status;
    }

    status = dxp_begin_run(&ioChan, &modChan, &ignored, &ignored, board, &id);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error starting special run for ioChan = %d", ioChan);
        dxp_log_error("dxp__start_special_run", info_string, status);
        return status;
    }

    sprintf(info_string, "Started special run %hu run id = %d on ioChan = %d",
            specialRun, id, ioChan);
    dxp_log_debug("dxp__start_special_run", info_string);

    return DXP_SUCCESS;
}


static int dxp__do_special_run(int ioChan, int modChan, Board *board,
                               parameter_t specialRun, double wait)
{
    int status;

    ASSERT(board != NULL);

    status = dxp__start_special_run(ioChan, modChan, board, specialRun);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error starting special run on ioChan = %d", ioChan);
        dxp_log_error("dxp__do_special_run", info_string, status);
        return status;
    }

    status = dxp_wait_for_busy(ioChan, modChan, 0, wait, board);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error waiting for special run to finish "
                "on ioChan = %d", ioChan);
        dxp_log_error("dxp__do_special_run", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}



/*
 * 'Burst' read a block of data from the 32-bit external memory.
 *
 * Reads a block of data from the external memory in 32-bit mode. The address
 * must be in the range of the external memory or an error will be returned.
 * This routine does not support burst reads in any other memory modes.
 */
static int dxp__burst_read_block(int ioChan, int modChan, unsigned long addr,
                                 unsigned int len, unsigned long *data)
{
    int status;

    unsigned int io_type = STJ_IO_BURST_READ;

    unsigned long ext_mem_addr = 0;

    UNUSED(modChan);

    ASSERT(data != NULL);

    if (addr > STJ_MEMORY_32_MAX_ADDR) {
        sprintf(info_string, "Specified external memory address (%#lx) is greater "
                "then the maximum allowed address (%#lx) for ioChan = %d", addr,
                STJ_MEMORY_32_MAX_ADDR, ioChan);
        dxp_log_error("dxp__burst_read_block", info_string, DXP_MEMORY_LENGTH);
        return DXP_MEMORY_LENGTH;
    }

    /* The TAR is written as part of the overall burst read command so there is
     * no need to set it explicitly here.
     */
    ext_mem_addr = STJ_32_EXT_MEMORY | addr;

    status = stj_md_io(&ioChan, &io_type, &ext_mem_addr, (void *)data, &len);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error doing 'burst' read (addr = %#lx, len = %u) for "
                "ioChan = %d", ext_mem_addr, len, ioChan);
        dxp_log_error("dxp__burst_read_block", info_string, status);
        return status;
    }

    status = dxp_write_global_register(ioChan, STJ_REG_ARB,  STJ_CLEAR_ARB);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error clearing external memory bit from arbitration "
                "register after 'burst' reading ioChan = %d", ioChan);
        dxp_log_error("dxp__burst_read_block", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Returns the name of the symbol located at the specified index.
 *
 * The Stj hardware does not have a separate DSP chip for each channel, which
 * means that a single chip must manage the DSP parameters for all 32 channels.
 * Some portion of the DSP parameters are designated "global" and are shared
 * amongst all of the channels. The rest of the DSP parameters are
 * "per-channel", meaning that the DSP keeps 32 different copies of the parameter:
 * -- one for each channel.
 *
 * To look up a symbol by its index, we must treat the DSP parameter name
 * space as flat with the following layout:
 *
 * [global params][chan0 params][chan1 params][chan2 params][chan3 params]
 *
 * Assumes that name is of size MAX_DSP_PARAM_NAME_LEN.
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


/*
 * Calculates the total number of DSP parameters for a single channel
 *
 * This routine does *not* calculate the total number of DSP parameters, which
 * is something like n_global + (32 * n_per_channel). This only calculates
 * one channel's worth of DSP parameters.
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


/*
 * Read a block of data
 *
 * Non-burst read a block of data from the specified address. This routine
 * repeatedly reads data from the DATA register. Assumes that data has
 * already been allocated by the caller.
 */
static int dxp__read_block(int ioChan, unsigned long addr, unsigned int len,
                           unsigned long *data)
{
    int status;

    unsigned int i;


    ASSERT(len > 0);
    ASSERT(data != NULL);


    /* Set the starting address for the block read */
    status = dxp_write_global_register(ioChan, STJ_REG_TAR, addr);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error writing block base address (%#lx) to TAR for "
                "ioChan = %d", addr, ioChan);
        dxp_log_error("dxp__read_block", info_string, status);
        return status;
    }

    /* Read the data register until we've received all of the values */
    for (i = 0; i < len; i++) {
        status = dxp_read_global_register(ioChan, STJ_REG_TDR, &(data[i]));

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error getting block value at index %u for "
                    "ioChan = %d", i, ioChan);
            dxp_log_error("dxp__read_block", info_string, status);
            return status;
        }
    }

    return DXP_SUCCESS;
}


/*
 * Sets the trace parameter based on the value in info.
 *
 * Interprets info[1] as TRACEWAIT, info[2] as TRACETYPE.
 */
static int dxp__process_trace_param(int ioChan, int modChan, unsigned int len,
                                    int *info, Board *b)
{
    int status;

    parameter_t TRACEWAIT = 0;
    parameter_t TRACETYPE = 0;

    ASSERT(info != NULL);
    ASSERT(b != NULL);

    if (len < 3) {
        sprintf(info_string, "Specified length '%u' for 'info' array is invalid for "
                "ioChan = %d", len, ioChan);
        dxp_log_error("dxp__process_trace_param", info_string, DXP_INVALID_LENGTH);
        return DXP_INVALID_LENGTH;
    }

    TRACEWAIT = (parameter_t)info[1];
    status = dxp_modify_dspsymbol(&ioChan, &modChan, "TRACEWAIT", &TRACEWAIT, b);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error setting TRACEWAIT for ioChan = %d", ioChan);
        dxp_log_error("dxp__process_trace_param", info_string, status);
        return status;
    }

    sprintf(info_string, "Setting TRACETYPE to %hu for ioChan = %d",
            TRACETYPE, ioChan);
    dxp_log_info("dxp__process_trace_param", info_string);

    TRACETYPE = (parameter_t)info[2];
    status = dxp_modify_dspsymbol(&ioChan, &modChan, "TRACETYPE", &TRACETYPE, b);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error setting TRACETYPE for ioChan = %d", ioChan);
        dxp_log_error("dxp__process_trace_param", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Do a trace special run.
 *
 * Other, higher-level routines are meant to wrap this: dxp_begin_adc_trace(), etc.
 */
static int dxp__do_trace(int ioChan, int modChan, Board *board)
{
    int status;

    status = dxp__do_special_run(ioChan, modChan, board,
                                 STJ_SPECIALRUN_TRACE, 10.0);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error doing special run on ioChan = %d", ioChan);
        dxp_log_error("dxp__do_trace", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}

/*
 * Reads from the base to base + offset in the DSP data memory.
 */
static int dxp__read_data_memory(int ioChan, unsigned long base,
                                 unsigned long offset, unsigned long *data)
{
    int status;

    unsigned long addr = 0;
    unsigned long i    = 0;
    unsigned long val  = 0;


    ASSERT(data != NULL);


    if (offset == 0) {
        sprintf(info_string, "Requested data memory length must be non-zero for "
                "ioChan = %d", ioChan);
        dxp_log_error("dxp__read_data_memory", info_string, DXP_MEMORY_LENGTH);
        return DXP_MEMORY_LENGTH;
    }

    addr = (unsigned long)(base + STJ_DATA_MEMORY);

    status = dxp_write_global_register(ioChan, STJ_REG_TAR, addr);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error setting Transfer Address Register to the base "
                "memory address (%#lx) for ioChan = %d", addr, ioChan);
        dxp_log_error("dxp__read_data_memory", info_string, status);
        return status;
    }

    for (i = 0; i < offset; i++) {
        status = dxp_read_global_register(ioChan, STJ_REG_TDR, &val);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error reading data memory word (%#lx) for "
                    "ioChan = %d", i, ioChan);
            dxp_log_error("dxp__read_data_memory", info_string, status);
            return status;
        }

        data[i] = val;
    }

    return DXP_SUCCESS;
}


/*
 * Write the specified data from base to base + offset in
 * the DSP data memory.
 *
 * base should be referenced from the DSP data memory base since the
 * DSP data memory offset will be added to the base value.
 */
static int dxp__write_data_memory(int ioChan, unsigned long base,
                                  unsigned long offset, unsigned long *data)
{
    int status;

    unsigned long i;
    unsigned long addr = 0;

#ifdef DXP_DEBUG_PERSISTENT_WRITE
    int j;
    unsigned long val;
    boolean_t data_matches;
#endif /* DXP_DEBUG_PERSISTENT_WRITE */


    if (offset == 0) {
        sprintf(info_string, "Requested data memory length must be non-zero for "
                "ioChan = %d", ioChan);
        dxp_log_error("dxp__write_data_memory", info_string, DXP_MEMORY_LENGTH);
        return DXP_MEMORY_LENGTH;
    }

    addr = (unsigned long)(base + STJ_DATA_MEMORY);

    status = dxp_write_global_register(ioChan, STJ_REG_TAR, addr);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error setting Transfer Address Register to the base "
                "memory address (%#lx) for ioChan = %d", addr, ioChan);
        dxp_log_error("dxp__write_data_memory", info_string, status);
        return status;
    }

    for (i = 0; i < offset; i++) {

        status = dxp_write_global_register(ioChan, STJ_REG_TDR, data[i]);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error writing data memory word for ioChan = %d",
                    ioChan);
            dxp_log_error("dxp__write_data_memory", info_string, status);
            return status;
        }
    }

#ifdef DXP_DEBUG_PERSISTENT_WRITE
    for (j = 0; j < MAX_NUM_REWRITES; j++) {

        data_matches = TRUE_;

        status = dxp_write_global_register(ioChan, STJ_REG_TAR, addr);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error setting Transfer Address Register to the base "
                    "memory address (%#lx) for ioChan = %d", addr, ioChan);
            dxp_log_error("dxp__write_data_memory", info_string, status);
            return status;
        }

        for (i = 0; i < offset; i++) {

            status = dxp_read_global_register(ioChan, STJ_REG_TDR, &val);

            if (status != DXP_SUCCESS) {
                sprintf(info_string, "Error reading data memory word for ioChan = %d",
                        ioChan);
                dxp_log_error("dxp__write_data_memory", info_string, status);
                return status;
            }

            data_matches = (boolean_t)(data_matches && (val == data[i]));
        }

        if (data_matches) {
            break;
        }

        /* If it didn't match, then rewrite it. */
        sprintf(info_string, "Rewriting %lu words to %#lx (i = %d) for ioChan = %d",
                offset, addr, j, ioChan);
        dxp_log_debug("dxp__write_data_memory", info_string);

        status = dxp_write_global_register(ioChan, STJ_REG_TAR, addr);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error setting Transfer Address Register to the base "
                    "memory address (%#lx) for ioChan = %d", addr, ioChan);
            dxp_log_error("dxp__write_data_memory", info_string, status);
            return status;
        }

        for (i = 0; i < offset; i++) {

            status = dxp_write_global_register(ioChan, STJ_REG_TDR, data[i]);

            if (status != DXP_SUCCESS) {
                sprintf(info_string, "Error writing data memory word for ioChan = %d",
                        ioChan);
                dxp_log_error("dxp__write_data_memory", info_string, status);
                return status;
            }
        }
    }

    if (j == MAX_NUM_REWRITES) {
        sprintf(info_string, "Error writing %lu words to %#lx for ioChan = %d",
                offset, base, ioChan);
        dxp_log_error("dxp__write_data_memory", info_string, DXP_REWRITE_FAILURE);
        return DXP_REWRITE_FAILURE;
    }
#endif /* DXP_DEBUG_PERSISTENT_WRITE */

    return DXP_SUCCESS;
}


/*
 * Awakens the DSP from its slumber.
 */
static int dxp__wake_dsp_up(int ioChan, int modChan, Board *b)
{
    int status;
    int ignored_status;

    parameter_t RUNTYPE = STJ_RUNTYPE_NORMAL;

    double RUNERROR = 0;

    unsigned long csr = 0;


    ASSERT(b != NULL);


    status = dxp_read_global_register(ioChan, STJ_REG_CSR, &csr);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading CSR for ioChan = %d", ioChan);
        dxp_log_error("dxp__wake_dsp_up", info_string, status);
        return status;
    }

    /* If no DSP is active, then we don't need to bother waking it up. */
    if (!(csr & (0x1 << STJ_CSR_DSP_ACT_BIT))) {
        sprintf(info_string, "Skipping DSP wake-up since no DSP is active for "
                "ioChan = %d", ioChan);
        dxp_log_info("dxp__wake_dsp_up", info_string);
        return DXP_SUCCESS;
    }

    sprintf(info_string, "Preparing to wake up DSP for ioChan = %d", ioChan);
    dxp_log_debug("dxp__wake_dsp_up", info_string);

    status = dxp_end_run(&ioChan, &modChan, b);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error stopping special DSP run for ioChan = %d", ioChan);
        dxp_log_error("dxp__wake_dsp_up", info_string, status);
        return status;
    }

    status = dxp_wait_for_busy(ioChan, modChan, 0, 1.0, b);

    if (status != DXP_SUCCESS) {
        ignored_status = dxp_read_dspsymbol(&ioChan, &modChan, "RUNERROR", b,
                                            &RUNERROR);
        sprintf(info_string, "Error waiting for DSP to wake up (RUNERROR = %#hx) "
                "for ioChan = %d", (parameter_t)RUNERROR, ioChan);
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


/*
 * Download the A + B FiPPIs to the hardware.
 */
static int dxp_download_fippis(int ioChan, int modChan, Board *b)
{
    int status;
    int i;


    unsigned long both_fippis = STJ_CONTROL_FIP_A | STJ_CONTROL_FIP_B;


    ASSERT(b != NULL);

    for (i = 0; i < MAX_NUM_FPGA_ATTEMPTS; i++) {

        sprintf(info_string, "Attempt #%d to download FPGAs for ioChan = %d",
                i, ioChan);

        status = dxp_download_fpga(ioChan, both_fippis, b->fippi_a);

        if (status == DXP_SUCCESS) {
            break;
        }

        if (status == DXP_FPGA_CRC) {
            sprintf(info_string, "CRC error detected while trying to download "
                    "FiPPI A & B, trying again for ioChan = %d", ioChan);
            dxp_log_warning("dxp_download_fippis", info_string);
            continue;
        }

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error downloading FiPPI A and FiPPI B for ioChan = %d",
                    ioChan);
            dxp_log_error("dxp_download_fippis", info_string, status);
            return status;
        }
    }

    if (!b->is_full_reboot) {
        status = dxp__wake_dsp_up(ioChan, modChan, b);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error waking DSP up for ioChan = %d", ioChan);
            dxp_log_error("dxp_download_fippis", info_string, status);
            return status;
        }
    }

    return DXP_SUCCESS;
}


/*
 * Burst read a mapping buffer.
 */
static int dxp__burst_read_buffer(int ioChan, int modChan, unsigned long addr,
                                  unsigned int len, unsigned long *data)
{
    int status;

    unsigned int io_type = STJ_IO_BURST_READ;

    UNUSED(modChan);


    ASSERT(data != NULL);


    sprintf(info_string, "addr = %#lx, len = %u", addr, len);
    dxp_log_debug("dxp__burst_read_buffer", info_string);

    /* The TAR is written as part of the overall burst read command so there is
     * no need to set it explicitly here.
     */
    status = stj_md_io(&ioChan, &io_type, &addr, (void *)data, &len);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error doing 'burst' read (addr = %#lx, len = %u) for "
                "ioChan = %d", addr, len, ioChan);
        dxp_log_error("dxp__burst_read_buffer", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Download a system fpga to the hardware.
 */
static int dxp_download_system_fpga(int ioChan, int modChan, Board *b)
{
    int status;

    parameter_t BUSY = 0x23;


    ASSERT(b != NULL);
    ASSERT(b->system_fpga != NULL);


    status = dxp_download_fpga(ioChan, STJ_CONTROL_SYS_FPGA, b->system_fpga);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error downloading System FPGA for ioChan = %d",
                ioChan);
        dxp_log_error("dxp_download_system_fpga", info_string, status);
        return status;
    }

    status = dxp_reset_dsp(ioChan);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error clearing DSP reset for ioChan = %d", ioChan);
        dxp_log_error("dxp_download_system_fpga", info_string, status);
        return status;
    }

    status = dxp_modify_dspsymbol(&ioChan, &modChan, "BUSY", &BUSY, b);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error setting BUSY for ioChan = %d",
                ioChan);
        dxp_log_error("dxp_download_system_fpga", info_string, status);
        return status;
    }

    status = dxp_boot_dsp(ioChan, modChan, b);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error booting DSP  for ioChan = %d", ioChan);
        dxp_log_error("dxp_download_system_fpga", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Check if the run enable bit is set active.
 */
static int dxp__run_enable_active(int ioChan, int *active)
{
    int status;

    unsigned long CSR = 0x0;


    ASSERT(active != NULL);


    status = dxp_read_global_register(ioChan, STJ_REG_CSR, &CSR);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading Control Status Register for ioChan = %d",
                ioChan);
        dxp_log_error("dxp_run_enable_active", info_string, status);
        return status;
    }

    if (CSR & (0x1 << STJ_CSR_RUN_ENA)) {
        *active = 1;

    } else {
        *active = 0;
    }

    return DXP_SUCCESS;
}


/*
 * Downloads FiPPIs A & B, similar to dxp_download_fippis(), except
 * that it doesn't wake the DSP up at the end of the download process. This
 * feature is needed when switching between RC and Reset firmware.
 */
static int dxp_download_fippis_dsp_no_wake(int ioChan, int modChan, Board *b)
{
    int status;
    int i;


    unsigned long both_fippis = STJ_CONTROL_FIP_A | STJ_CONTROL_FIP_B;

    UNUSED(modChan);
    ASSERT(b != NULL);

    for (i = 0; i < MAX_NUM_FPGA_ATTEMPTS; i++) {

        sprintf(info_string, "Attempt #%d to download FPGAs for ioChan = %d",
                i, ioChan);

        status = dxp_download_fpga(ioChan, both_fippis, b->fippi_a);

        if (status == DXP_SUCCESS) {
            break;
        }

        if (status == DXP_FPGA_CRC) {
            sprintf(info_string, "CRC error detected while trying to download "
                    "FiPPI A & B, trying again for ioChan = %d", ioChan);
            dxp_log_warning("dxp_download_fippis", info_string);
            continue;
        }

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error downloading FiPPI A and FiPPI B for "
                    "ioChan = %d", ioChan);
            dxp_log_error("dxp_download_fippis", info_string, status);
            return status;
        }
    }

    return DXP_SUCCESS;
}
