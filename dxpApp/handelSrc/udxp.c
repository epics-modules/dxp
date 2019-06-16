/*
 * Copyright (c) 2002-2004 X-ray Instrumentation Associates
 * Copyright (c) 2005-2013 XIA LLC
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>

#include "md_generic.h"
#include "xerxes_generic.h"
#include "xerxes_structures.h"
#include "xia_xerxes_structures.h"
#include "xerxes_io.h"
#include "udxp.h"
#include "xerxes_errors.h"
#include "xia_udxp.h"
#include "udxp_common.h"
#include "xia_assert.h"

#define RECV_BASE 5

#define CMD_START_RUN           0x00
#define CMD_STOP_RUN            0x01
#define CMD_READ_DSP_PARAMS     0x42
#define CMD_WRITE_DSP_DATA_MEM  0x45
#define CMD_READ_DSP_DATA_MEM   0x45
#define CMD_READ_SERIAL_NUM     0x48
#define CMD_FIPPI_CONFIG        0x81

/* For microDXP the parameter data is stored in Board->dsp in the first modChan */
#define PARAMS(x)   (x)->dsp[0]->params

/* Define the length of the error reporting string info_string */
#define INFO_LEN 400

/*
 * Pointer to utility functions
 */
static DXP_MD_SET_MAXBLK  udxp_md_set_maxblk;
static DXP_MD_GET_MAXBLK  udxp_md_get_maxblk;
static DXP_MD_LOG         udxp_md_log;
static DXP_MD_ALLOC       udxp_md_alloc;
static DXP_MD_FREE        udxp_md_free;
static DXP_MD_PUTS        udxp_md_puts;
static DXP_MD_WAIT        udxp_md_wait;

static int dxp_init_dspparams(int ioChan, int modChan, Board *board);

char info_string[INFO_LEN];

/*
 * Routine to create pointers to all the internal routines
 */
XERXES_EXPORT int dxp_init_udxp(Functions* funcs)
/* Functions *funcs;    */
{
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

    funcs->dxp_do_cmd = dxp_do_cmd;
    
    dxp_init_pic_version_cache();

    return DXP_SUCCESS;
}
/*
 * Routine to initialize the IO Driver library, get the proper pointers
 */
static int dxp_init_driver(Interface* iface)
/* Interface *iface;    Input: Pointer to the IO Interface  */
{    
    /* Assign all the static vars here to point at the proper library routines */
    udxp_md_set_maxblk = iface->funcs->dxp_md_set_maxblk;
    udxp_md_get_maxblk = iface->funcs->dxp_md_get_maxblk;
    
    return DXP_SUCCESS;
}
/*
 * Routine to initialize the Utility routines, get the proper pointers
 */
static int dxp_init_utils(Utils* utils)
/* Utils *utils;     Input: Pointer to the utility functions */
{

    /* Assign all the static vars here to point at the proper library routines */
    udxp_md_log = utils->funcs->dxp_md_log;

    udxp_md_alloc = utils->funcs->dxp_md_alloc;
    udxp_md_free  = utils->funcs->dxp_md_free;

    udxp_md_wait = utils->funcs->dxp_md_wait;
    udxp_md_puts = utils->funcs->dxp_md_puts;

    return DXP_SUCCESS;
}


/*
 * Now begins the section with higher level routines.  Mostly to handle reading
 * in the DSP and FiPPi programs.  And handling starting up runs, ending runs,
 * runing calibration tasks.
 */

/*
 * Routine to download the FiPPi configuration
 *
 * Specify as either "fippi0", "fippi1" or "fippi2".
 */
static int dxp_download_fpgaconfig(int* ioChan, int* modChan, char *name, Board* board)
{
    int status;

    byte_t send[2];
    byte_t receive[8];

    unsigned int lenR      = 8;
    unsigned int lenS      = 2;

    unsigned short fippiNum = 0;

    UNUSED(ioChan);
    UNUSED(modChan);

    if (STREQ(name, "all")) {
        return DXP_SUCCESS;
    }

    sscanf(name, "fippi%hu", &fippiNum);

    if (fippiNum > 2) {
        status = DXP_UDXP;
        dxp_log_error("dxp_download_fpgaconfig",
                      "Specified FiPPI configuration is out-of-range",
                      status);
        return status;
    }

    send[0] = (byte_t)0x00;
    send[1] = (byte_t)fippiNum;

    status = dxp_command(*modChan, board, CMD_FIPPI_CONFIG, lenS,
                         send, lenR, receive);

    if (status != DXP_SUCCESS) {
        status = DXP_UDXP;
        dxp_log_error("dxp_download_fpgaconfig",
                      "Error executing command",
                      status);
        return status;
    }

    if (receive[4] != 0) {
        status = DXP_UDXP;
        dxp_log_error("dxp_download_fpgaconfig",
                      "Board reported an error condition",
                      status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Routine to read the FiPPi configuration file into memory
 *
 * A moot point for the uDXP.
 */
static int dxp_get_fpgaconfig(void* fippi)
{
    ((Fippi_Info *)fippi)->maxproglen = MAXFIP_LEN;
    ((Fippi_Info *)fippi)->proglen    = 0;

    return DXP_SUCCESS;
}


/*
 * Routine to check that all the FiPPis downloaded successfully to
 * a single module.  If the routine returns DXP_SUCCESS, then the
 * FiPPis are OK
 */
static int dxp_download_fpga_done(int* modChan, char *name, Board *board)
{
    UNUSED(modChan);
    UNUSED(name);
    UNUSED(board);

    /* Return to this if it turns out that
     * checking "something" on the board is
     * required for the uDXP.
     */

    return DXP_SUCCESS;
}


/*
 * Routine to download the DSP Program
 *
 * DSP code doesn't need to be downloaded to the board for the microDXP.
 *
 * We use this hook to read and initialize the microDXP's DSP parameter
 * data structure
 */
static int dxp_download_dspconfig(int* ioChan, int* modChan, Board *board)
{
    int status;
    int udxpModChan = 0;

    UNUSED(modChan);

    /* Xerxes only calls this hook once per module. */
    ASSERT(*modChan == ALLCHAN);

    status = dxp_init_dspparams(*ioChan, udxpModChan, board);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error initializing DSP parameters for "
                "ioChan %d", *ioChan);
        dxp_log_error("dxp_download_dspconfig", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Routine to retrieve the FIPPI program maximum sizes so that memory
 * can be allocated.
 */
static int dxp_get_fipinfo(void* fippi)
{
    ((Fippi_Info *)fippi)->maxproglen = MAXFIP_LEN;

    return DXP_SUCCESS;
}


/*
 * Routine to retrieve the DSP program maximum sizes so that memory
 * can be allocated.
 */
static int dxp_get_dspinfo(Dsp_Info* dsp)
{
    dsp->params->maxsym    = MAXSYM;
    dsp->params->maxsymlen = MAX_DSP_PARAM_NAME_LEN;
    dsp->params->nsymbol   = 0;
    dsp->maxproglen        = MAXDSP_LEN;

    return DXP_SUCCESS;

}


/*
 * Routine to retrieve the DSP program and symbol table
 *
 * Read the DSP configuration file  -- logical name (or symbolic link)
 * DSP_CONFIG must be defined prior to execution of the program. At present,
 * it is NOT possible to use different configurations for different DXP
 * channels.
 */
static int dxp_get_dspconfig(Dsp_Info* dsp)
{
    dsp->params->maxsym    = MAXSYM;
    dsp->params->maxsymlen = MAX_DSP_PARAM_NAME_LEN;
    dsp->params->nsymbol   = 0;
    dsp->maxproglen        = MAXDSP_LEN;
    dsp->proglen           = 0;

    return DXP_SUCCESS;
}

/*
 * Routine to locate a symbol in the DSP symbol table.
 *
 * This routine returns the address of the symbol called name in the DSP
 * symbol table.
 */
static int dxp_loc(char name[], Dsp_Info* dsp, unsigned short* address)
{
    unsigned short i = 0;
    ASSERT(dsp->params->nsymbol > 0);

    for (i = 0; i < dsp->params->nsymbol; i++) {
        if (STREQ(name, dsp->params->parameters[i].pname)) {
            *address = (unsigned short)dsp->params->parameters[i].address;
            return DXP_SUCCESS;
        }
    }

    sprintf(info_string, "Unknown symbol '%s' in DSP parameter list", name);
    dxp_log_error("dxp_loc", info_string, DXP_NOSYMBOL);

    return DXP_NOSYMBOL;
}

/*
 * Read the list of DSP parameter names from board and populate the dsp
 * data structure.
 */
static int dxp_init_dspparams(int ioChan, int modChan, Board *board)
{
    int status;

    unsigned int param_array_size;
    unsigned int nsymbol;

    unsigned int lenS   = 1;
    unsigned int lenR   = 10;
    unsigned int strLen = 0;
    unsigned int i;

    byte_t cmd = CMD_READ_DSP_PARAMS;

    byte_t send[1];
    byte_t receive[10];

    byte_t *names = NULL;

    char *ptr = NULL;

    /* When called from xerxes the modChan is set to allchan which we should
     * ignore, instead always use modChan = 0 for per-board parameter storage
     */
    ASSERT(PARAMS(board) != NULL);
    ASSERT(PARAMS(board)->parameters != NULL);
    ASSERT(PARAMS(board)->maxsymlen > 0);
    ASSERT(PARAMS(board)->maxsym > 0);

    send[0] = (byte_t)1;
    status = dxp_command(modChan, board, cmd, lenS, send, lenR, receive);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading number of DSP parameters for "
                "ioChan %d", ioChan);
        dxp_log_error("dxp_init_dspparams", info_string, status);
        return status;
    }

    /* Extract the length of the string here */
    strLen = (unsigned int)(receive[7] | ((unsigned int)receive[8] << 8));

    names = (byte_t *)udxp_md_alloc((strLen + 10) * sizeof(byte_t));

    if (names == NULL) {
        dxp_log_error("dxp_init_dspparams", "Out-of-memory trying to create DSP list",
                    DXP_NOMEM);
        return DXP_NOMEM;
    }

    lenR = strLen + 5 + RECV_BASE;
    send[0] = (byte_t)0;

    status = dxp_command(modChan, board, cmd, lenS, send, lenR, names);

    if (status != DXP_SUCCESS) {
        udxp_md_free((void *)names);
        sprintf(info_string, "Error reading DSP parameter names for ioChan %d",
                ioChan);
        dxp_log_error("dxp_init_dspparams", info_string, status);
        return status;
    }

    nsymbol = (unsigned int)(receive[5] | (unsigned int)receive[6] << 8);

    sprintf(info_string, "Initialized %d DSP parameters for channel %d, dsp %s",
                    nsymbol, ioChan, board->dsp[0]->filename);
    dxp_log_debug("dxp_init_dspparams", info_string);

    /* Parse in names list */
    ptr = (char *)(names + 9);
    for (i = 0; i < nsymbol; i++) {
        strcpy(PARAMS(board)->parameters[i].pname, ptr);
        PARAMS(board)->parameters[i].address = i;
        ptr = strchr(ptr, '\0');
        ptr++;
    }

    udxp_md_free((void *)names);

    /* For microDXP the delayed parsing of DSP parameter information means
     * the global param data will not be initialized until now
     */
    for (i = 0; i < board->nchan; i++) {
        if (board->params[i] != NULL) {
            udxp_md_free(board->params[i]);
            board->params[i] = NULL;
        }

        board->dsp[i]->params->nsymbol = (unsigned short)nsymbol;

        param_array_size = nsymbol * sizeof(unsigned short);
        board->params[i] = (unsigned short *)udxp_md_alloc(param_array_size);
        memset(board->params[i], 0, param_array_size);
    }

   return DXP_SUCCESS;

}

/*
 * Set a parameter of the DSP.  Pass the symbol name, value to set and module
 * pointer and channel number.
 */
static int dxp_modify_dspsymbol(int* ioChan, int* modChan, char* name,
                                unsigned short* value, Board *board)
{
    int status;

    unsigned int lenS = 6;
    unsigned int lenR = 1 + RECV_BASE;

    unsigned short addr = 0x0000;

    byte_t cmd = CMD_WRITE_DSP_DATA_MEM;

    byte_t send[6];
    byte_t receive[1 + RECV_BASE];

    UNUSED(ioChan);

    /* Get index of symbol */
    status = dxp_loc(name, board->dsp[0], &addr);

    if (status != DXP_SUCCESS) {
        dxp_log_error("dxp_modify_dspsymbol", "Error finding DSP parameter",
                      status);

        return status;
    }

    send[0] = 0x01;
    send[1] = 0x01;
    send[2] = LO_BYTE(addr);
    send[3] = HI_BYTE(addr);
    send[4] = LO_BYTE(*value);
    send[5] = HI_BYTE(*value);

    /* Write new value to the board */
    status = dxp_command(*modChan, board, cmd, lenS, send, lenR, receive);

    if (status != DXP_SUCCESS) {
        dxp_log_error("dxp_modify_dspsymbol", "Error writing to DSP memory",
                      status);
        return status;
    }

    if (receive[4] != 0) {
        dxp_log_error("dxp_modify_dspsymbol",
                        "Board reported an error condition", status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Read a single parameter of the DSP.  Pass the symbol name, module
 * pointer and channel number.  Returns the value read using the variable value.
 */
static int dxp_read_dspsymbol(int* ioChan, int* modChan, char* name,
                              Board *board, double* value)
{
    int status;

    unsigned int lenS = 4;
    unsigned int lenR = 3 + RECV_BASE;

    byte_t cmd = CMD_READ_DSP_DATA_MEM;

    byte_t send[4];
    byte_t receive[3 + RECV_BASE];

    unsigned short addr = 0x0000;

    UNUSED(ioChan);

    status = dxp_loc(name, board->dsp[0], &addr);

    if (status != DXP_SUCCESS) {
        dxp_log_error("dxp_read_dspsymbol", "Error finding DSP parameter",
                      status);
        return status;
    }

    send[0] = 0x00;
    send[1] = 0x01;
    send[2] = LO_BYTE(addr);
    send[3] = HI_BYTE(addr);

    status = dxp_command(*modChan, board, cmd, lenS, send, lenR, receive);

    if (status != DXP_SUCCESS) {
        dxp_log_error("dxp_read_dspsymbol", "Error executing command",
                      status);
        return status;
    }

    if (receive[4] != 0) {
        status = DXP_UDXP;
        dxp_log_error("dxp_read_dspsymbol", "Board reported an error condition",
                      status);
        return status;
    }

    *value = (double)((double) receive[5] +
                     ((double) receive[6] * 256.));

    sprintf(info_string, "ioChan %d, %s = %0.0f @ %hu", *ioChan, name, *value, addr);
    dxp_log_debug("dxp_read_dspsymbol", info_string);

    return DXP_SUCCESS;
}


/*
 * Routine to readout the parameter memory from a single DSP.
 *
 * This routine reads the parameter list from the DSP pointed to by ioChan and
 * modChan.  It returns the array to the caller.
 */
static int dxp_read_dspparams(int* ioChan, int* modChan, Board *board,
                              unsigned short* params)
{
    int status;

    unsigned int lenS = 4;
    unsigned int lenR;
    unsigned int maxLenR = 65 + RECV_BASE;
    unsigned int maxWordsPerTransfer = 32;

    unsigned int addr;
    unsigned int nParams;
    unsigned int nFullReads;

    byte_t cmd = CMD_READ_DSP_DATA_MEM;

    byte_t send[4];
    byte_t maxR[65 + RECV_BASE];

    byte_t *finalR = NULL;

    unsigned short *data = (unsigned short *)params;

    UNUSED(ioChan);

    ASSERT(PARAMS(board) != NULL);
    ASSERT(PARAMS(board)->nsymbol > 0);

    nParams = PARAMS(board)->nsymbol;

    nFullReads = (unsigned int)floor((double)(nParams / maxWordsPerTransfer));

    /* Do the full reads first */
    for (addr = 0x0000; addr < (nFullReads * maxWordsPerTransfer);
            addr += maxWordsPerTransfer) {
        send[0] = (byte_t)0x00;
        send[1] = (byte_t)maxWordsPerTransfer;
        send[2] = LO_BYTE(addr);
        send[3] = HI_BYTE(addr);

        status = dxp_command(*modChan, board, cmd, lenS, send, maxLenR, maxR);
        
        if (status != DXP_SUCCESS) {
            dxp_log_error("dxp_read_dspparams", "Error reading DSP data memory", status);
            return status;
        }

        memcpy(data + addr, maxR + 5, maxWordsPerTransfer * sizeof(unsigned short));
    }

    /* Transfer the remaining words. Should be less then 32 */
    send[1] = (byte_t)(nParams % 32);
    send[2] = (byte_t)LO_BYTE(addr);
    send[3] = (byte_t)HI_BYTE(addr);

    lenR = ((nParams % 32) * 2) + 1 + RECV_BASE;
    finalR = (byte_t *)udxp_md_alloc(lenR * sizeof(byte_t));

    if (finalR == NULL) {
        status = DXP_NOMEM;
        sprintf(info_string, "Out-of-memory allocating %zu bytes", lenR * sizeof(byte_t));
        dxp_log_error("dxp_read_dspparams", info_string, status);
        return status;
    }

    status = dxp_command(*modChan, board, cmd, lenS, send, lenR, finalR);

    if (status != DXP_SUCCESS) {
        udxp_md_free(finalR);
        dxp_log_error("dxp_read_dspparams", "Error reading DSP data memory", status);
        return status;
    }

    memcpy(data + addr, finalR + 5, (nParams % 32) * sizeof(unsigned short));
    udxp_md_free(finalR);

    return DXP_SUCCESS;
}


/*
 * Routine to write parameter memory to a single DSP.
 *
 * This routine writes the parameter list to the DSP pointed to by ioChan and
 * modChan.
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
 * Routine to return the length of the spectrum memory.
 *
 * For 4C-2X boards, this value is stored in the DSP and dynamic.
 * For 4C boards, it is fixed.
 */
static int dxp_get_spectrum_length(int *ioChan, int *modChan,
                                   Board *board, unsigned int *len)
{
    UNUSED(ioChan);
    UNUSED(modChan);
    UNUSED(board);
    UNUSED(len);

    return DXP_SUCCESS;
}


/*
 * Routine to return the length of the baseline memory.
 *
 * For 4C-2X boards, this value is stored in the DSP and dynamic.
 * For 4C boards, it is fixed.
 */
static int dxp_get_baseline_length(int *modChan, Board *b, unsigned int *len)
{
    UNUSED(modChan);
    UNUSED(b);
    UNUSED(len);


    return DXP_SUCCESS;
}


/*
 * Routine to return the length of the event memory.
 *
 * For 4C-2X boards, this value is stored in the DSP and dynamic.
 * For 4C boards, it is fixed.
 */
static unsigned int dxp_get_event_length(Dsp_Info* dsp, unsigned short* params)
{
    UNUSED(dsp);
    UNUSED(params);

    return 0;
}


/*
 * Routine to readout the spectrum memory from a single DSP.
 *
 * This routine reads the spectrum histogramfrom the DSP pointed to by ioChan and
 * modChan.  It returns the array to the caller.
 */
static int dxp_read_spectrum(int* ioChan, int* modChan, Board* board,
                             unsigned long* spectrum)
{
    UNUSED(ioChan);
    UNUSED(modChan);
    UNUSED(board);
    UNUSED(spectrum);




    return DXP_SUCCESS;
}


/*
 * Routine to readout the baseline histogram from a single DSP.
 *
 * This routine reads the baselin histogram from the DSP pointed to by ioChan and
 * modChan.  It returns the array to the caller.
 */
static int dxp_read_baseline(int* ioChan, int* modChan, Board* board,
                             unsigned long* baseline)
{
    UNUSED(ioChan);
    UNUSED(modChan);
    UNUSED(board);
    UNUSED(baseline);

    return DXP_SUCCESS;
}
/*
 * Routine to begin a data taking run.
 *
 * This routine starts a run on the specified channel.  It tells the DXP
 * whether to ignore the gate signal and whether to clear the MCA.
 */
static int dxp_begin_run(int* ioChan, int* modChan, unsigned short* gate,
                         unsigned short* resume, Board *board, int *id)
{
    int status;

    unsigned int lenS = 1;
    unsigned int lenR = 8;

    byte_t send[1];
    byte_t receive[8];

    UNUSED(ioChan);
    UNUSED(gate);
    UNUSED(board);
    UNUSED(id);


    /* Xerxes and the uDXP have different ideas
     * of what resume should be set to. For instance,
     * Xerxes thinks that resume = 1 means that the run
     * should be resumed w/o clearing the MCA. The uDXP
     * prefers that resume = 0.
     */
    send[0] = (byte_t)(0x01 ^ *resume);


    status = dxp_command(*modChan, board, CMD_START_RUN, lenS, send, 
                        lenR, receive);
   
    if (status != DXP_SUCCESS) {
        dxp_log_error("dxp_begin_run",
                      "Error executing command",
                      status);
        return status;
    }

    if (receive[4] != 0) {
        dxp_log_error("dxp_begin_run",
                      "Board reported an error condition",
                      status);
        return status;
    }

    /* Probably need to return the run number
     * (receive[5] and receive[6]) back to
     * the user but there is no mechanism in
     * Xerxes for that kind of information.
     */

    return DXP_SUCCESS;
}


/*
 * Routine to end a data taking run.
 *
 * This routine ends the run on the specified channel.
 */
static int dxp_end_run(int* ioChan, int* modChan, Board *board)
{
    int status;

    unsigned int lenS = 0;
    unsigned int lenR = 6;

    byte_t receive[6];

    UNUSED(ioChan);

    status = dxp_command(*modChan, board, CMD_STOP_RUN, lenS, NULL, 
                        lenR, receive);

    if (status != DXP_SUCCESS) {
        dxp_log_error("dxp_end_run",
                      "Error executing command",
                      status);
        return status;
    }

    if (receive[4] != 0) {
        dxp_log_error("dxp_end_run",
                      "Board reported an error condition",
                      status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Routine to determine if the module thinks a run is active.
 */
static int dxp_run_active(int* ioChan, int* modChan, int* active)
{
    UNUSED(ioChan);
    UNUSED(modChan);
    UNUSED(active);

    return DXP_SUCCESS;
}


/*
 * Routine to start a control task routine.  Definitions for types are contained
 * in the xerxes_generic.h file.
 */
static int dxp_begin_control_task(int* ioChan, int* modChan, short *type,
                                  unsigned int *length, int *info, Board *board)
{
    UNUSED(ioChan);
    UNUSED(modChan);
    UNUSED(type);
    UNUSED(length);
    UNUSED(info);
    UNUSED(board);

    return DXP_SUCCESS;
}


/*
 * Routine to end a control task routine.
 */
static int dxp_end_control_task(int* ioChan, int* modChan, Board *board)
{
    UNUSED(ioChan);
    UNUSED(modChan);
    UNUSED(board);

    return DXP_SUCCESS;
}


/*
 * Routine to get control task parameters.
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
 * Routine to return control task data.
 */
static int dxp_control_task_data(int* ioChan, int* modChan, short *type,
                                 Board *board, void *data)
{
    UNUSED(ioChan);
    UNUSED(modChan);
    UNUSED(type);
    UNUSED(board);
    UNUSED(data);

    return DXP_SUCCESS;
}


/*
 * Routine to decode the error message from the DSP after a run if performed
 *
 * Returns the RUNERROR and ERRINFO words from the DSP parameter block
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
 * Routine to clear an error in the DSP
 *
 * Clears non-fatal DSP error in one or all channels of a single DXP module.
 * If modChan is -1 then all channels are cleared on the DXP.
 */
static int dxp_clear_error(int* ioChan, int* modChan, Board *board)
{
    UNUSED(ioChan);
    UNUSED(modChan);
    UNUSED(board);

    return DXP_SUCCESS;
}


/*
 * Routine to get run statistics from the DXP.
 *
 * Returns some run statistics from the parameter block array[]
 */
static int dxp_get_runstats(int *ioChan, int *modChan, Board *b,
                            unsigned long* evts, unsigned long* under,
                            unsigned long* over, unsigned long* fast,
                            unsigned long* base, double* live,
                            double* icr, double* ocr)
{
    UNUSED(ioChan);
    UNUSED(modChan);
    UNUSED(b);
    UNUSED(evts);
    UNUSED(under);
    UNUSED(over);
    UNUSED(fast);
    UNUSED(base);
    UNUSED(live);
    UNUSED(icr);
    UNUSED(ocr);

    return DXP_SUCCESS;
}


/*
 * Directly reads offset count entries from the memory starting at base.
 *
 * The data array is unsigned long, but the result of the
 * underlying read might not be. The underlying values will be copied
 * into data but not packed into it, which means you might have
 * some 0 padding in data. It is the job of the calling routine to transfer
 * from data to the correctly typed array.
 */
XERXES_STATIC int dxp_read_mem(int *ioChan, int *modChan, Board *board,
                               char *name, unsigned long *base,
                               unsigned long *offset,
                               unsigned long *data)
{
    int status;

    unsigned long i;

    unsigned short *us_data = NULL;

    UNUSED(board);

    ASSERT(name != NULL);
    ASSERT(offset != NULL);
    ASSERT(board != NULL);
    ASSERT(ioChan != NULL);
    ASSERT(modChan != NULL);
    ASSERT(base != NULL);
    ASSERT(data != NULL);


    if (!STREQ(name, "direct")) {
        sprintf(info_string, "A memory read of type '%s' is not currently "
                "supported for ioChan = %d, modChan = %d", name, *ioChan,
                *modChan);
        dxp_log_error("dxp_read_mem", info_string, DXP_UNIMPLEMENTED);
        return DXP_UNIMPLEMENTED;
    }

    us_data = udxp_md_alloc((*offset) * sizeof(unsigned short));

    if (us_data == NULL) {
        sprintf(info_string, "Unable to allocate %d bytes for 'us_data' for "
                "ioChan = %d, modChan = %d",
                (int)((*offset) * sizeof(unsigned short)), *ioChan, *modChan);
        dxp_log_error("dxp_read_mem", info_string, DXP_NOMEM);
        return DXP_NOMEM;
    }

    status = dxp_usb_read_block(*modChan, board, *base, *offset, us_data);


    if (status != DXP_SUCCESS) {
        udxp_md_free(us_data);
        sprintf(info_string, "Error reading data memory block (addr = %#lx, "
                "n = %lu) for ioChan = %d", *base, *offset, *ioChan);
        dxp_log_error("dxp_read_mem", info_string, status);
        return status;
    }

    for (i = 0; i < *offset; i++) {
        data[i] = (unsigned long)us_data[i];
    }

    udxp_md_free(us_data);

    return DXP_SUCCESS;
}


XERXES_STATIC int dxp_write_mem(int *ioChan, int *modChan, Board *board,
                                char *name, unsigned long *base,
                                unsigned long *offset, unsigned long *data)
{
    int status;

    unsigned short *us_data = NULL;

    unsigned long i;

    UNUSED(board);


    ASSERT(name);
    ASSERT(ioChan);
    ASSERT(modChan);
    ASSERT(offset);
    ASSERT((*offset) > 0);
    ASSERT(base);
    ASSERT(data);


    if (!STREQ(name, "direct")) {
        sprintf(info_string, "A memory write of type '%s' is not currently "
                "supported for ioChan = %d, modChan = %d.", name, *ioChan,
                *modChan);
        dxp_log_error("dxp_write_mem", info_string, DXP_UNIMPLEMENTED);
        return DXP_UNIMPLEMENTED;
    }

    us_data = udxp_md_alloc((*offset) * sizeof(unsigned short));

    if (!us_data) {
        sprintf(info_string, "Unable to allocate %zu bytes for 'us_data' for "
                "ioChan = %d, modChan = %d.",
                (*offset) * sizeof(unsigned short), *ioChan, *modChan);
        dxp_log_error("dxp_write_mem", info_string, DXP_NOMEM);
        return DXP_NOMEM;
    }

    for (i = 0; i < *offset; i++) {
        /* Yes this clips the top 16-bits off your data. */
        us_data[i] = (unsigned short)(data[i] & 0xFFFF);
    }

    status = dxp_usb_write_block(*modChan, board, *base, *offset, us_data);

    udxp_md_free(us_data);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error writing %lu words to %#lx for ioChan = %d, "
                "modChan = %d", *offset, *base, *ioChan, *modChan);
        dxp_log_error("dxp_write_mem", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * This routine does nothing currently.
 */
static int dxp_write_reg(int *ioChan, int *modChan, char *name,
                         unsigned long *data)
{
    UNUSED(ioChan);
    UNUSED(modChan);
    UNUSED(name);
    UNUSED(data);

    return DXP_SUCCESS;
}


/*
 * This routine currently does nothing.
 */
XERXES_STATIC int dxp_read_reg(int *ioChan, int *modChan, char *name,
                               unsigned long *data)
{
    UNUSED(ioChan);
    UNUSED(modChan);
    UNUSED(name);
    UNUSED(data);

    return DXP_SUCCESS;
}


/*
 * This routine simply calls the dxp_command() routine.
 */
XERXES_STATIC int XERXES_API dxp_do_cmd(int modChan, Board *board, byte_t cmd, unsigned int lenS,
                                        byte_t *send, unsigned int lenR, byte_t *receive)
{
    int status;
    status = dxp_command(modChan, board, cmd, lenS, send, lenR, receive);
    
    if (status != DXP_SUCCESS) {
        dxp_log_error("dxp_do_cmd", "Command error", status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Calls the interface close routine.
 */
XERXES_STATIC int XERXES_API dxp_unhook(Board *board)
{
    sprintf(info_string, "Attempting to unhook ioChan = %d", board->ioChan);
    dxp_log_debug("dxp_unhook", info_string);

    board->iface->funcs->dxp_md_close(&(board->ioChan));

    /* Ignore the status due to some issues involving
     * repetitive function calls.
     */

    return DXP_SUCCESS;
}

/*
 * Returns the name of the symbol located at the specified index.
 */
XERXES_STATIC int dxp_get_symbol_by_index(int modChan, unsigned short index,
                                          Board *board, char *name)
{
    UNUSED(modChan);

    ASSERT(name  != NULL);
    ASSERT(board != NULL);
    ASSERT(index < PARAMS(board)->nsymbol);

    strncpy(name, PARAMS(board)->parameters[index].pname, MAX_DSP_PARAM_NAME_LEN);

    return DXP_SUCCESS;
}


XERXES_STATIC int dxp_get_num_params(int modChan, Board *board,
                                     unsigned short *n_params)
{
    UNUSED(modChan);

    ASSERT(board != NULL);
    ASSERT(n_params != NULL);

    *n_params = (unsigned short)(PARAMS(board)->nsymbol);

    return DXP_SUCCESS;
}
