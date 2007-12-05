/*
 * dxp4c2x.c
 *
 *   Created        25-Sep-1996  by Ed Oltman, Brad Hubbard
 *   Extensive mods 19-Dec-1996  EO
 *   Modified:      03-Feb-1996  EO:  Added "static" qualifier to variable defs
 *       add getenv() to fopen (unix compatibility); replace 
 *       #include <dxp_area/...> with #include <...> (unix compatibility)
 *       -->compiler now requires path for include files
 *       introduced LIVECLOCK_TICK_TIME for LIVETIME calculation
 *       added functions dxp_read_long and dxp_write_long
 *    Modified      05-Feb-1997 EO: introduce FNAME replacement for VAX
 *        compatability (getenv crashes if logical not defined)
 *    Modified      07-Feb-1997 EO: fix bug in dxp_read_long for LITTLE_ENDIAN
 *        machines, cast dxp_swaplong argument as (long *) in dxp_get_runstats
 *    Modified      04-Oct-97: Make function declarations compatable w/traditional
 *        C; Replace LITTLE_ENDIAN parameter with function dxp_little_endian; New
 *        function dxp_little_endian -- dynamic test; fix bug in dxp_download_dspconfig;
 *        Replace MAXBLK parameter with function call to dxp_md_get_maxblk 
 *         
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
 */


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <md_generic.h>

#include <xerxes_structures.h>
#include <xia_xerxes_structures.h>
#include <dxp4c2x.h>
#include <xerxes_errors.h>
#include <xia_dxp4c2x.h>

#include "xia_assert.h"


XERXES_STATIC int dxp_read_external_memory(int *ioChan, int *modChan,
										   Board *board, unsigned long base,
										   unsigned long offset,
										   unsigned long *data);

XERXES_STATIC int dxp_wait_for_busy(int *ioChan, int *modChan, Board *board,
									int n_timeout, double busy, float wait);

/* Define the length of the error reporting string info_string */
#define INFO_LEN 400
/* Define the length of the line string used to read in files */
#define LINE_LEN 132

/* Starting memory location and length for DSP parameter memory */
static unsigned short startp=START_PARAMS;
/* Variable to store the number of DAC counts per unit gain (dB)	*/
static double dacpergaindb;
/* Variable to store the number of DAC counts per unit gain (linear) */
static double dacpergain;
/* 
 * Store pointers to the proper DLL routines to talk to the CAMAC crate 
 */
static DXP_MD_IO dxp4c2x_md_io;
static DXP_MD_SET_MAXBLK dxp4c2x_md_set_maxblk;
static DXP_MD_GET_MAXBLK dxp4c2x_md_get_maxblk;
/* 
 * Define the utility routines used throughout this library
 */
static DXP_MD_LOG dxp4c2x_md_log;
static DXP_MD_ALLOC dxp4c2x_md_alloc;
static DXP_MD_FREE dxp4c2x_md_free;
static DXP_MD_PUTS dxp4c2x_md_puts;
static DXP_MD_WAIT dxp4c2x_md_wait;

/* All memory reads should go here */
static Mem_Op_t mem_readers[] = {
  {"external", dxp_read_external_memory},
};

#define NUM_MEM_READERS (sizeof(mem_readers) / sizeof(mem_readers[0]))


/******************************************************************************
 *
 * Routine to create pointers to all the internal routines
 * 
 ******************************************************************************/
int dxp_init_dxp4c2x(Functions* funcs)
/* Functions *funcs;		*/
{
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
  funcs->dxp_download_dsp_done = dxp_download_dsp_done;
  funcs->dxp_calibrate_channel = dxp_calibrate_channel;
  funcs->dxp_calibrate_asc = dxp_calibrate_asc;

  funcs->dxp_look_at_me = dxp_look_at_me;
  funcs->dxp_ignore_me = dxp_ignore_me;
  funcs->dxp_clear_LAM = dxp_clear_LAM;
  funcs->dxp_loc = dxp_loc;
  funcs->dxp_symbolname = dxp_symbolname;

  funcs->dxp_read_spectrum = dxp_read_spectrum;
  funcs->dxp_test_spectrum_memory = dxp_test_spectrum_memory;
  funcs->dxp_get_spectrum_length = dxp_get_spectrum_length;
  funcs->dxp_get_sca_length = dxp_get_sca_length;
  funcs->dxp_read_baseline = dxp_read_baseline;
  funcs->dxp_test_baseline_memory = dxp_test_baseline_memory;
  funcs->dxp_get_baseline_length = dxp_get_baseline_length;
  funcs->dxp_test_event_memory = dxp_test_event_memory;
  funcs->dxp_get_event_length = dxp_get_event_length;
  funcs->dxp_write_dspparams = dxp_write_dspparams;
  funcs->dxp_write_dsp_param_addr = dxp_write_dsp_param_addr;
  funcs->dxp_read_dspparams = dxp_read_dspparams;
  funcs->dxp_read_dspsymbol = dxp_read_dspsymbol;
  funcs->dxp_modify_dspsymbol = dxp_modify_dspsymbol;
  funcs->dxp_dspparam_dump = dxp_dspparam_dump;

  funcs->dxp_prep_for_readout = dxp_prep_for_readout;
  funcs->dxp_done_with_readout = dxp_done_with_readout;
  funcs->dxp_begin_run = dxp_begin_run;
  funcs->dxp_run_active = dxp_run_active;
  funcs->dxp_end_run = dxp_end_run;
  funcs->dxp_begin_control_task = dxp_begin_control_task;
  funcs->dxp_end_control_task = dxp_end_control_task;
  funcs->dxp_control_task_params = dxp_control_task_params;
  funcs->dxp_control_task_data = dxp_control_task_data;

  funcs->dxp_decode_error = dxp_decode_error;
  funcs->dxp_clear_error = dxp_clear_error;
  funcs->dxp_get_runstats = dxp_get_runstats;
  funcs->dxp_change_gains = dxp_change_gains;
  funcs->dxp_setup_asc = dxp_setup_asc;

  funcs->dxp_setup_cmd = dxp_setup_cmd;

  funcs->dxp_read_mem = dxp_read_mem;
  funcs->dxp_write_mem = dxp_write_mem;

  funcs->dxp_write_reg = dxp_write_reg;
  funcs->dxp_read_reg  = dxp_read_reg;

  funcs->dxp_do_cmd = dxp_do_cmd;

  funcs->dxp_unhook = dxp_unhook;
  
  funcs->dxp_read_sca = dxp_read_sca;
  
  return DXP_SUCCESS;
}
/******************************************************************************
 *
 * Routine to initialize the IO Driver library, get the proper pointers
 * 
 ******************************************************************************/
static int dxp_init_driver(Interface* iface) 
	 /* Interface *iface;				Input: Pointer to the IO Interface		*/
{

  /* Assign all the static vars here to point at the proper library routines */
  dxp4c2x_md_io         = iface->funcs->dxp_md_io;
  dxp4c2x_md_set_maxblk = iface->funcs->dxp_md_set_maxblk;
  dxp4c2x_md_get_maxblk = iface->funcs->dxp_md_get_maxblk;

  return DXP_SUCCESS;
}
/******************************************************************************
 *
 * Routine to initialize the Utility routines, get the proper pointers
 * 
 ******************************************************************************/
static int dxp_init_utils(Utils* utils) 
	 /* Utils *utils;					Input: Pointer to the utility functions	*/
{

  /* Assign all the static vars here to point at the proper library routines */
  dxp4c2x_md_log  = utils->funcs->dxp_md_log;

#ifdef XIA_SPECIAL_MEM
  dxp4c2x_md_alloc = utils->funcs->dxp_md_alloc;
  dxp4c2x_md_free  = utils->funcs->dxp_md_free;
#else
  dxp4c2x_md_alloc = malloc;
  dxp4c2x_md_free  = free;
#endif /* XIA_SPECIAL_MEM */

  dxp4c2x_md_wait  = utils->funcs->dxp_md_wait;
  dxp4c2x_md_puts  = utils->funcs->dxp_md_puts;

  return DXP_SUCCESS;
}

/********------******------*****------******-------******-------******------*****
 * Now begins the routines devoted to atomic operations on DXP cards.  These
 * routines involve reading or writing to a location on the DXP card.  Nothing
 * fancy is done in these routines.  Just single or Block transfers to and from 
 * the DXP.  These are the proper way to write to the CSR, etc...instead of using
 * the CAMAC F and A values throughout the code.
 *
 ********------******------*****------******-------******-------******------*****/
 

/******************************************************************************
 * Routine to write data to the TSAR (Transfer Start Address Register)
 * 
 * This register tells the DXP where to begin transferring data on the
 * data transfer which shall begin shortly.
 *
 ******************************************************************************/
static int dxp_write_tsar(int* ioChan, unsigned short* addr)
	 /* int *ioChan;						Input: I/O channel of DXP module      */
	 /* unsigned short *addr;			Input: address to write into the TSAR */
{
  unsigned int f, a, len;
  int status;

  f=DXP_TSAR_F_WRITE;
  a=DXP_TSAR_A_WRITE;
  len=1;
  status=dxp4c2x_md_io(ioChan,&f,&a,addr,&len);					/* write TSAR */
  if (status!=DXP_SUCCESS){
	status = DXP_WRITE_TSAR;
	dxp_log_error("dxp_write_tsar","Error writing TSAR",status);
  }
  return status;
}

/******************************************************************************
 * Routine to write to the CSR (Control/Status Register)
 * 
 * This register contains bits which both control the operation of the 
 * DXP and report the status of the DXP.
 *
 ******************************************************************************/
static int dxp_write_csr(int* ioChan, unsigned short* data)
	 /* int *ioChan;						Input: I/O channel of DXP module      */
	 /* unsigned short *data;			Input: address of data to write to CSR*/
{
  unsigned int f, a, len;
  int status;

  f=DXP_CSR_F_WRITE;
  a=DXP_CSR_A_WRITE;
  len=1;
  status=dxp4c2x_md_io(ioChan,&f,&a,data,&len);					/* write CSR */
  if (status!=DXP_SUCCESS){
	status = DXP_WRITE_CSR;
	dxp_log_error("dxp_write_csr","Error writing CSR",status);
  }
  return status;
}

/******************************************************************************
 * Routine to read from the CSR (Control/Status Register)
 * 
 * This register contains bits which both control the operation of the 
 * DXP and report the status of the DXP.
 *
 ******************************************************************************/
static int dxp_read_csr(int* ioChan, unsigned short* data)
	 /* int *ioChan;						Input: I/O channel of DXP module   */
	 /* unsigned short *data;			Output: where to put data from CSR */
{
  unsigned int f, a, len;
  int status;

  f=DXP_CSR_F_READ;
  a=DXP_CSR_A_READ;
  len=1;
  status=dxp4c2x_md_io(ioChan,&f,&a,data,&len);					/* write TSAR */
  if (status!=DXP_SUCCESS){
	status = DXP_READ_CSR;
	dxp_log_error("dxp_read_csr","Error reading CSR",status);
  }
  return status;
}

/******************************************************************************
 * Routine to read from the GSR (Global Status Register)
 * 
 * This register contains status bits for the DXP4C2X.
 *
 ******************************************************************************/
static int dxp_read_gsr(int* ioChan, unsigned short* data)
	 /* int *ioChan;						Input: I/O channel of DXP module   */
	 /* unsigned short *data;			Output: where to put data from CSR */
{
  unsigned int f, a, len;
  int status;

  f=DXP_GSR_F_READ;
  a=DXP_GSR_A_READ;
  len=1;
  status=dxp4c2x_md_io(ioChan,&f,&a,data,&len);					/* read GSR */
  if (status!=DXP_SUCCESS){
	status = DXP_READ_GSR;
	dxp_log_error("dxp_read_gsr","Error reading GSR",status);
  }
  return status;
}

/******************************************************************************
 * Routine to write to the Channel bits in the GCR (Global Control Register)
 * 
 * This register contains Control bits for the DXP4C2X.  This routine will only
 * modify the Channel selector bits.  Not changing any other control bits.
 *
 ******************************************************************************/
static int dxp_write_channel_gcr(int* ioChan, unsigned short* data)
	 /* int *ioChan;						Input: I/O channel of DXP module	*/
	 /* unsigned short *data;			Output: What channel to access		*/
{
  unsigned int f, a, len;
  int status;

  f=DXP_CHAN_GCR_F_WRITE;
  a=DXP_CHAN_GCR_A_WRITE;
  len=1;

  status=dxp4c2x_md_io(ioChan,&f,&a,data,&len);		/* write Channel GCR */
  if (status!=DXP_SUCCESS){
	status = DXP_WRITE_GCR;
	dxp_log_error("dxp_write_channel_gcr",
				  "Error writing to the channel GCR",status);
  }
  return status;
}

/******************************************************************************
 * Routine to read data from the DXP
 * 
 * This is the generic data transfer routine.  It can transfer data from the 
 * DSP for example based on the address previously downloaded to the TSAR
 *
 ******************************************************************************/
static int dxp_read_data(int* ioChan, unsigned short* data, unsigned int len)
	 /* int *ioChan;							Input: I/O channel of DXP module   */
	 /* unsigned short *data;				Output: where to put data read     */
	 /* unsigned int len;					Input: length of the data to read  */
{
  unsigned int f, a;
  int status;

  f=DXP_DATA_F_READ;
  a=DXP_DATA_A_READ;
  status=dxp4c2x_md_io(ioChan,&f,&a,data,&len);					/* write TSAR */
  if (status!=DXP_SUCCESS){
	status = DXP_READ_DATA;
	dxp_log_error("dxp_read_data","Error reading data",status);
  }
  return status;
}

/******************************************************************************
 * Routine to write data to the DXP
 * 
 * This is the generic data transfer routine.  It can transfer data to the 
 * DSP for example based on the address previously downloaded to the TSAR
 *
 ******************************************************************************/
static int dxp_write_data(int* ioChan, unsigned short* data, unsigned int len)
	 /* int *ioChan;							Input: I/O channel of DXP module   */
	 /* unsigned short *data;				Input: address of data to write    */
	 /* unsigned int len;					Input: length of the data to read  */
{
  unsigned int f, a;
  int status;

  f=DXP_DATA_F_WRITE;
  a=DXP_DATA_A_WRITE;
  status=dxp4c2x_md_io(ioChan,&f,&a,data,&len);					/* write TSAR */
  if (status!=DXP_SUCCESS){
	status = DXP_WRITE_DATA;
	dxp_log_error("dxp_write_data","Error writing data",status);
  }

  return status;
}

/******************************************************************************
 * Routine to write fippi data
 * 
 * This is the routine that transfers the FiPPi program to the DXP.  It 
 * assumes that the CSR is already downloaded with what channel requires
 * a FiPPi program.
 *
 ******************************************************************************/
static int dxp_write_fippi(int* ioChan, unsigned short* data, unsigned int len)
	 /* int *ioChan;							Input: I/O channel of DXP module   */
	 /* unsigned short *data;				Input: address of data to write    */
	 /* unsigned int len;					Input: length of the data to read  */
{
  unsigned int f, a;
  int status;

  f=DXP_FIPPI_F_WRITE;
  a=DXP_FIPPI_A_WRITE;
  status=dxp4c2x_md_io(ioChan,&f,&a,data,&len);					/* write TSAR */
  if (status!=DXP_SUCCESS){
	status = DXP_WRITE_FIPPI;
	dxp_log_error("dxp_write_fippi","Error writing to FiPPi reg",status);
  }
  return status;
}

/******************************************************************************
 * Routine to enable the LAMs(Look-At-Me) on the specified DXP
 *
 * Enable the LAM for a single DXP module.
 *
 ******************************************************************************/
static int dxp_look_at_me(int* ioChan, int* modChan)
	 /* int *ioChan;						Input: I/O channel of DXP module */
	 /* int *modChan;					Input: DXP channels no (-1,0,1,2,3)   */
{
  int status;
  unsigned int f,a,len;
  unsigned short dummy;
  int *itemp;

  /* Assign the unused inputs to stop compiler warnings */
  itemp = modChan;
   
  f=DXP_ENABLE_LAM_F;
  a=DXP_ENABLE_LAM_A;
  len=1;
  status=dxp4c2x_md_io(ioChan,&f,&a,&dummy,&len);                    /* enable LAM */
  if (status!=DXP_SUCCESS){
	status=DXP_ENABLE_LAM;
	dxp_log_error("dxp_enable_LAM","Error enabling LAM",status);
  }

  return status;
}

/******************************************************************************
 * Routine to enable the LAMs(Look-At-Me) on the specified DXP
 *
 * Disable the LAM for a single DXP module.
 *
 ******************************************************************************/
static int dxp_ignore_me(int* ioChan, int* modChan)
	 /* int *ioChan;						Input: I/O channel of DXP module */
	 /* int *modChan;					Input: DXP channels no (-1,0,1,2,3)   */
{
  int status;
  unsigned int f,a,len;
  unsigned short dummy;
  int *itemp;

  /* Assign the unused inputs to stop compiler warnings */
  itemp = modChan;
   
  f=DXP_DISABLE_LAM_F;
  a=DXP_DISABLE_LAM_A;
  len=1;
  status=dxp4c2x_md_io(ioChan,&f,&a,&dummy,&len);                    /* disable LAM */
  if (status!=DXP_SUCCESS){
	status=DXP_DISABLE_LAM;
	dxp_log_error("dxp_disable_LAM","Error disabling LAM",status);
  }
  return status;
}

/******************************************************************************
 * Routine to clear the LAM(Look-At-Me) on the specified DXP
 *
 * Clear the LAM for a single DXP module.
 *
 ******************************************************************************/
static int dxp_clear_LAM(int* ioChan, int* modChan)
	 /* int *ioChan;						Input: I/O channel of DXP module */
	 /* int *modChan;					Input: DXP channels no (-1,0,1,2,3)   */
{
  /*
   *     Clear the LAM for a single DXP module
   */
  int status;
  unsigned int f,a,len;
  unsigned short dummy;
  int *itemp;

  /* Assign the unused inputs to stop compiler warnings */
  itemp = modChan;
   
  f=DXP_CLEAR_LAM_F;
  a=DXP_CLEAR_LAM_A;
  len=1;
  status=dxp4c2x_md_io(ioChan,&f,&a,&dummy,&len);                      /* clear LAM */
  if (status!=DXP_SUCCESS){
	status=DXP_CLR_LAM;
	dxp_log_error("dxp_clear_LAM","Error clearing LAM",status);
  }
  return status;
}


/********------******------*****------******-------******-------******------*****
 * Now begins the code devoted to routines that look like CAMAC transfers to 
 * a preset address to the external world.  They actually write to the TSAR, 
 * then the CSR, then finally write the data to the CAMAC bus.  The user need 
 * never know
 *
 ********------******------*****------******-------******-------******------*****/

/******************************************************************************
 * Routine to read a single word of data from the DXP
 * 
 * This routine makes all the neccessary calls to the TSAR and CSR to read
 * data from the appropriate channel and address of the DXP.
 *
 ******************************************************************************/
static int dxp_read_word(int* ioChan, int* modChan, unsigned short* addr,
						 unsigned short* readdata)
	 /* int *ioChan;						Input: I/O channel of DXP module      */
	 /* int *modChan;					Input: DXP channels no (0,1,2,3)      */
	 /* unsigned short *addr;			Input: Address within DSP memory      */
	 /* unsigned short *readdata;		Output: word read from memory         */
{
  /*
   *     Read a single word from a DSP address, for a single channel of a
   *                single DXP module.
   */
  int status;
  char info_string[INFO_LEN];
  unsigned short data, saddr= *addr;
	
  if((*modChan<0)||(*modChan>3)){
	sprintf(info_string,"called with DXP channel number %d",*modChan);
	status = DXP_BAD_PARAM;
	dxp_log_error("dxp_read_word",info_string,status);
  }

  /* write status register for initiating transfer */
	
  data = (unsigned short) (*modChan<<6);
  status = dxp_write_channel_gcr(ioChan, &data);
  if (status!=DXP_SUCCESS){
	dxp_log_error("dxp_read_word","Error writing to CSR",status);
	return status;
  }

  /* write transfer start address register */
	
  status = dxp_write_tsar(ioChan, &saddr);
  if (status!=DXP_SUCCESS){
	dxp_log_error("dxp_read_word","Error writing TSAR",status);
	return status;
  }

  /* now read the data */

  status = dxp_read_data(ioChan, readdata, 1);
  if(status!=DXP_SUCCESS){
	status=DXP_READ_WORD;
	dxp_log_error("dxp_read_word","Error reading CAMDATA",status);
	return status;
  }
  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to write a single word of data to the DXP
 * 
 * This routine makes all the neccessary calls to the TSAR and CSR to write
 * data to the appropriate channel and address of the DXP.
 *
 ******************************************************************************/
static int dxp_write_word(int* ioChan, int* modChan, unsigned short* addr,
						  unsigned short* writedata)
	 /* int *ioChan;						Input: I/O channel of DXP module      */
	 /* int *modChan;					Input: DXP channels no (-1,0,1,2,3)   */
	 /* unsigned short *addr;			Input: Address within X or Y mem.     */
	 /* unsigned short *writedata;		Input: word to write to memory        */
{
  /*
   *     Write a single word to a DSP address, for a single channel or all 
   *            channels of a single DXP module
   */
  int status;
  char info_string[INFO_LEN];
  unsigned short data, saddr= *addr;

  if((*modChan<-1)||(*modChan>3)){
	sprintf(info_string,"called with DXP channel number %d",*modChan);
	status = DXP_BAD_PARAM;
	dxp_log_error("dxp_write_word",info_string,status);
  }
   
  /* write status register for initiating transfer */
	
  if (*modChan==ALLCHAN) data= MASK_ALLCHAN;
  else data = (unsigned short) (*modChan<<6);
	
  status = dxp_write_channel_gcr(ioChan, &data);
  if (status!=DXP_SUCCESS) {
	dxp_log_error("dxp_write_word","Error writing CSR",status);
	return status;
  }

  /* write transfer start address register */
	
  status = dxp_write_tsar(ioChan, &saddr);
  if (status!=DXP_SUCCESS) {
	dxp_log_error("dxp_write_word","Error writing TSAR",status);
	return status;
  }

  /* now write the data */
   
  status = dxp_write_data(ioChan, writedata, 1);
  if (status!=DXP_SUCCESS){
	status=DXP_WRITE_WORD;
	dxp_log_error("dxp_write_word","Error writing CAMDATA",status);
	return status;
  }
  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to read a block of data from the DXP
 * 
 * This routine makes all the neccessary calls to the TSAR and CSR to read
 * data from the appropriate channel and address of the DXP.
 *
 ******************************************************************************/
static int dxp_read_block(int* ioChan, int* modChan, unsigned short* addr,
						  unsigned int* length, unsigned short* readdata)
	 /* int *ioChan;						Input: I/O channel of DXP module       */
	 /* int *modChan;					Input: DXP channels no (0,1,2,3)       */
	 /* unsigned short *addr;			Input: start address within X or Y mem.*/
	 /* unsigned int *length;			Input: # of 16 bit words to transfer   */
	 /* unsigned short *readdata;		Output: words to read from memory      */
{
  /*
   *     Read a block of words from a single DSP address, or consecutive
   *          addresses for a single channel of a single DXP module
   */
  int status;
  char info_string[INFO_LEN];
  unsigned short data, saddr= *addr;
  unsigned int i,nxfers,xlen;
  unsigned int maxblk;

  if((*modChan<0)||(*modChan>3)){
	sprintf(info_string,"called with DXP channel number %d",*modChan);
	status = DXP_BAD_PARAM;
	dxp_log_error("dxp_read_block",info_string,status);
  }

  /* write status register for initiating transfer */
   
  data = (unsigned short) (*modChan<<6);
   
  status = dxp_write_channel_gcr(ioChan, &data);
  if (status!=DXP_SUCCESS) {
	dxp_log_error("dxp_read_block","Error writing CSR",status);
	return status;
  }

  /* write transfer start address register */

  status = dxp_write_tsar(ioChan, &saddr);
  if (status!=DXP_SUCCESS) {
	dxp_log_error("dxp_read_block","Error writing TSAR",status);
	return status;
  }
	
  /* Retrieve MAXBLK and check if single transfer is needed */
  maxblk=dxp4c2x_md_get_maxblk();
  if (maxblk <= 0) maxblk = *length;

  /* prepare for the first pass thru loop */
  nxfers = ((*length-1)/maxblk) + 1;
  xlen = ((maxblk>=*length) ? *length : maxblk);
  i = 0;
  do {

	/* now read the data */
        
	status = dxp_read_data(ioChan, &readdata[maxblk*i], xlen);
	if (status!=DXP_SUCCESS) {
	  status = DXP_READ_BLOCK;
	  sprintf(info_string,"Error reading %dth block transer",i);
	  dxp_log_error("dxp_read_block",info_string,status);
	  return status;
	}
	/* Next loop */
	i++;
	/* On last pass thru loop transfer the remaining bytes */
	if (i==(nxfers-1)) xlen=((*length-1)%maxblk) + 1;
  } while (i<nxfers);

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to write a single word of data to the DXP
 * 
 * This routine makes all the neccessary calls to the TSAR and CSR to write
 * data to the appropriate channel and address of the DXP.
 *
 ******************************************************************************/
static int dxp_write_block(int* ioChan, int* modChan, unsigned short* addr,
						   unsigned int* length, unsigned short* writedata)
	 /* int *ioChan;						Input: I/O channel of DXP module     */
	 /* int *modChan;					Input: DXP channels no (-1,0,1,2,3)  */
	 /* unsigned short *addr;			Input: start address within X/Y mem  */
	 /* unsigned int *length;			Input: # of 16 bit words to transfer */
	 /* unsigned short *writedata;		Input: words to write to memory      */
{
  /*
   *    Write a block of words to a single DSP address, or consecutive
   *    addresses for a single channel or all channels of a single DXP module
   */
  int status;
  char info_string[INFO_LEN];
  unsigned short data, saddr= *addr;
  unsigned int i,nxfers,xlen;
  unsigned int maxblk;

  if((*modChan<-1)||(*modChan>3)){
	sprintf(info_string,"called with DXP channel number %d",*modChan);
	status = DXP_BAD_PARAM;
	dxp_log_error("dxp_write_block",info_string,status);
  }
   
  /* write status register for initiating transfer */
	
  if (*modChan==ALLCHAN) data= MASK_ALLCHAN;
  else data = (unsigned short) (*modChan<<6);
   
  status = dxp_write_channel_gcr(ioChan, &data);
  if (status!=DXP_SUCCESS){
	dxp_log_error("dxp_write_block","Error writing CSR",status);
	return status;
  }

  /* write transfer start address register */

  status = dxp_write_tsar(ioChan, &saddr);
  if (status!=DXP_SUCCESS){
	dxp_log_error("dxp_write_block","Error writing TSAR",status);
	return status;
  }

  /* Retrieve MAXBLK and check if single transfer is needed */
  maxblk=dxp4c2x_md_get_maxblk();
  if (maxblk <= 0) maxblk = *length;

  /* prepare for the first pass thru loop */
  nxfers = ((*length-1)/maxblk) + 1;
  xlen = ((maxblk>=*length) ? *length : maxblk);
  i = 0;
  do {

	/* now read the data */
        
	status = dxp_write_data(ioChan, &writedata[maxblk*i], xlen);
	if (status!=DXP_SUCCESS) {
	  status = DXP_WRITE_BLOCK;
	  sprintf(info_string,"Error in %dth block transfer",i);
	  dxp_log_error("dxp_write_block",info_string,status);
	  return status;
	}
	/* Next loop */
	i++;
	/* On last pass thru loop transfer the remaining bytes */
	if (i==(nxfers-1)) xlen=((*length-1)%maxblk) + 1;
  } while (i<nxfers);

  return status;
}


/********------******------*****------******-------******-------******------*****
 * Now begins the section with higher level routines.  Mostly to handle reading 
 * in the DSP and FiPPi programs.  And handling starting up runs, ending runs,
 * runing calibration tasks.
 *
 ********------******------*****------******-------******-------******------*****/
/******************************************************************************
 * Routine to download the FiPPi configuration
 * 
 * This routine downloads the FiPPi program of specifice decimation(number
 * of clocks to sum data over), CAMAC channel and DXP channel.  If -1 for the 
 * DXP channel is specified then all channels are downloaded.
 *
 ******************************************************************************/
static int dxp_download_fpgaconfig(int* ioChan, int* modChan, char *name, Board* board)
     /* int *ioChan;			Input: I/O channel of DXP module	*/
     /* int *modChan;			Input: DXP channels no (-1,0,1,2,3)	*/
     /* char *name;                     Input: Type of FPGA to download         */
     /* Board *board;			Input: Board data			*/
{
  /*
   *   Download the appropriate FiPPi configuration file to a single channel
   *   or all channels of a single DXP module.
   */
  int status;
  char info_string[INFO_LEN];
  unsigned short data;
  unsigned int i,j,length,xlen,nxfers;
  float wait;
  unsigned int maxblk;
  Fippi_Info *fippi=NULL;
  int mod;

  /* Variables for the control tasks */
  short task;
  unsigned int ilen=1;
  int taskinfo[1];
  unsigned short value=7;
  float timeout;

  if (!((STREQ(name, "all")) || (STREQ(name, "fippi")))) 
    {
      sprintf(info_string, "The DXP4C2X does not have an FPGA called %s for channel number %d", name, *modChan);
      status = DXP_BAD_PARAM;
      dxp_log_error("dxp_download_fpgaconfig",info_string,status);
      return status;
    }
  
  if((*modChan<-1)||(*modChan>3)){
    sprintf(info_string,"called with DXP channel number %d",*modChan);
    status = DXP_BAD_PARAM;
    dxp_log_error("dxp_download_fpgaconfig",info_string,status);
    return status;
  }
  
  dxp_log_debug("dxp_download_fpgaconfig", "Starting fpgaconfig...");
  

  if (*modChan == ALLCHAN) {
    
    /* If allchan chosen, then select the first valid fippi */
    for (i = 0; i < board->nchan; i++) {
      
      if (((board->used) & (0x1<<i)) != 0) 
	{
	  fippi = board->fippi[i];
	  break;
	}
    }

  } else {

    fippi = board->fippi[*modChan];
  }
  
  mod = board->mod;
  
  /* make sure a valid Fippi was found */
  if (fippi==NULL) {
    sprintf(info_string,"There is no valid FiPPi defined for module %i",mod);
    status = DXP_NOFIPPI;
    dxp_log_error("dxp_download_fpgaconfig",info_string,status);
    return status;
  }
  
  /* If needed, put the DSP to sleep before downloading the FIPPI */
  if (board->chanstate==NULL) {
    sprintf(info_string,"Something wrong in initialization, no channel state information for module %i",mod);
    status = DXP_INITIALIZE;
    dxp_log_error("dxp_download_fpgaconfig",info_string,status);
    return status;
  }

  /* Quick hack to always try and stop a run
   * before doing anything else.
   * Reference: BUG ID #29
   */
  status = dxp_end_run(ioChan, modChan);
  
  if (status != DXP_SUCCESS) {
	
	sprintf(info_string, "Error stopping a run on ioChan = %d, modChan = %d",
			*ioChan, *modChan);
	dxp_log_error("dxp_download_fpgaconfig", info_string, status);
	return status;
  }

  /* check the DSP download state, if downloaded, then sleep */
  if (board->chanstate[*modChan].dspdownloaded==1) {
    
    dxp_log_debug("dxp_download_fpgaconfig", "board->chanstate[].dspdownloaded == 1");
    
    task = CT_DXP2X_SLEEP_DSP;
    ilen = 1;
    taskinfo[0] = 1;
    if ((status=dxp_begin_control_task(ioChan, modChan, &task, 
				       &ilen, taskinfo, board))!=DXP_SUCCESS) {
      sprintf(info_string,"Error putting the DSP to sleep for module %i",mod);
      status = DXP_DSPSLEEP;
      dxp_log_error("dxp_download_fpgaconfig",info_string,status);
      return status;
    }
    
    dxp_log_debug("dxp_download_fpgaconfig", "Preparing to wait for BUSY->7");
    
    /* Now wait for BUSY=7 to indicate the DSP is asleep */
    value = 7;
    timeout = 5.0;
    if ((status=dxp_download_dsp_done(ioChan, modChan, &mod, board->dsp[*modChan], 
				      &value, &timeout))!=DXP_SUCCESS) {
      sprintf(info_string,"Error waiting for BUSY=7 state for module %i",mod);
      status = DXP_DSPTIMEOUT;
      dxp_log_error("dxp_download_fpgaconfig",info_string,status);
      return status;
    }
  }
  
  dxp_log_debug("dxp_download_fpgaconfig", "Starting to download new FiPPI code");

  length = fippi->proglen;
  
  
  /* Read the GCR and preserve the RunEna
   * information so that we don't stop the
   * run where the DSP is asleep (and wake
   * it up).
   */	
  status = dxp_read_csr(ioChan, &data);
  
  if (status != DXP_SUCCESS) 
    {
      sprintf(info_string, "Error reading GCR from chan %d", *ioChan);
      dxp_log_error("dxp_download_fpgaconfig", info_string, status);
      return status;
    }
  
  /* Mask off the channel information */
  data &= ~MASK_CHANNELS;
 
  /* Set LCAReset bit to 1 */
  data |= MASK_FIPRESET;

  if (*modChan == ALLCHAN) {
    
    data |= MASK_ALLCHAN;
    
  } else {
    
    data |= (*modChan<<6);
  }
  
  status = dxp_write_csr(ioChan, &data);
  if (status!=DXP_SUCCESS){
    dxp_log_error("dxp_download_fpgaconfig","Error writing CSR",status); 
    return status;
  }

  /* wait 50ms, for LCA to be ready for next data */

  wait = 0.050f;
  status = dxp4c2x_md_wait(&wait);

	
  /* single word transfers for first 10 words */
  for (i=0;i<10;i++){
    status = dxp_write_fippi(ioChan, &(fippi->data[i]), 1);
    if (status!=DXP_SUCCESS){
      status = DXP_WRITE_WORD;
      sprintf(info_string,"Error in %dth 1-word transfer",i);
      dxp_log_error("dxp_download_fpgaconfig",info_string,status);
      return status;
    }
  }

  /* Retrieve MAXBLK and check if single transfer is needed */
  maxblk=dxp4c2x_md_get_maxblk();
  if (maxblk <= 0) maxblk = length;

  /* prepare for the first pass thru loop */
  nxfers = ((length-11)/maxblk) + 1;
  xlen = ((maxblk>=(length-10)) ? (length-10) : maxblk);
  j = 0;
  do {

    /* now read the data */
    
    status = dxp_write_fippi(ioChan, &(fippi->data[j*maxblk+10]), xlen);
    if (status!=DXP_SUCCESS){
      status = DXP_WRITE_BLOCK;
      sprintf(info_string,"Error in %dth (last) block transfer",j);
      dxp_log_error("dxp_download_fpgaconfig",info_string,status);
      return status;
    }
    /* Next loop */
    j++;
    /* On last pass thru loop transfer the remaining bytes */
    if (j==(nxfers-1)) xlen=((length-11)%maxblk) + 1;
  } while (j<nxfers);
  
  dxp_log_debug("dxp_download_fpgaconfig", "Finished downloading new FiPPI code");
  dxp_log_debug("dxp_download_fpgaconfig", "Preparing to wake the DSP up");
  
  /* After FIPPI is downloaded, end the SLEEP mode */
  if (board->chanstate[*modChan].dspdownloaded==1) {
    
    dxp_log_debug("dxp_download_fpgaconfig", "board->chanstate[].dspdownloaded == 1");
    
    if ((status=dxp_end_control_task(ioChan, modChan, board))!=DXP_SUCCESS) {
      
      sprintf(info_string,"Error waking the DSP from sleep for module %i",mod);
      status = DXP_DSPSLEEP;
      dxp_log_error("dxp_download_fpgaconfig",info_string,status);
      return status;
    }

    /* Now wait for BUSY=0 to indicate the DSP is ready */
    value = 0;
    timeout = 5.0;
    if ((status=dxp_download_dsp_done(ioChan, modChan, &mod, board->dsp[*modChan], 
				      &value, &timeout))!=DXP_SUCCESS) {
      sprintf(info_string,"Error waiting for BUSY=0 state for module %i",mod);
      status = DXP_DSPTIMEOUT;
      dxp_log_error("dxp_download_fpgaconfig",info_string,status);
      return status;
    }
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to read the FiPPi configuration file into memory
 * 
 * This routine reads in the file filename and stores the FiPPi program in 
 * the fipconfig global array at location determined by *dec_index.
 *
 ******************************************************************************/
static int dxp_get_fpgaconfig(Fippi_Info* fippi)
     /* Fippi_Info *fippi;					I/O: structure of Fippi info */
{
  int status;
  char info_string[INFO_LEN];
  char line[LINE_LEN];
  unsigned int j, nchars, len;
  FILE *fp;
	
  sprintf(info_string,"%s%s%s","Reading FiPPI file ",
		  fippi->filename,"...");
  dxp_log_info("dxp_get_fpgaconfig",info_string);

  fippi->maxproglen = MAXFIP_LEN;

  if (fippi->data==NULL){
	status = DXP_NOMEM;
	sprintf(info_string,"%s",
			"Error allocating space for configuration");
	dxp_log_error("dxp_get_fpgaconfig",info_string,status);
	return status;
  }
  /*
   *  Check to see if FiPPI configuration has already been read in: 0 words
   *  means it has not...
   */
  if((fp = dxp_find_file(fippi->filename,"r"))==NULL){
	status = DXP_OPEN_FILE;
	sprintf(info_string,"%s%s","Unable to open FiPPI configuration ",
			fippi->filename);
	dxp_log_error("dxp_get_fpgaconfig",info_string,status);
	return status;
  }
	
  /* Stuff the data into the fipconfig array */
		
  len = 0;
  while (fgets(line,132,fp)!=NULL){
	if (line[0]=='*') continue;
	nchars = strlen(line)-1;
	while ((nchars>0) && !isxdigit(line[nchars])) {
	  nchars--;
	}
	for(j=0;j<nchars;j=j+2) {
	  sscanf(&line[j],"%2hX",&(fippi->data[len]));
	  len++;
	}
  }
  fippi->proglen = len;
  fclose(fp);
  dxp_log_info("dxp_get_fpgaconfig","...DONE!");

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Routine to check that all the FiPPis downloaded successfully to
 * a single module.  If the routine returns DXP_SUCCESS, then the 
 * FiPPis are OK
 *
 ******************************************************************************/
static int dxp_download_fpga_done(int *modChan, char *name, Board *board)
/* int *modChan;			Input: Module channel number              */
/* char *name;                          Input: Type of FPGA to check the status of*/
/* board *board;			Input: Board structure for this device 	  */
{

  int status, chan;
  char info_string[INFO_LEN];
  unsigned short data;
	
  int ioChan;
  unsigned short used;

  int idummy;

  /* Assignment to satisfy the compiler */
  idummy = *modChan;

  /* Few assignements to make life easier */
  ioChan = board->ioChan;
  if (*modChan == ALLCHAN) 
    {
      used = board->used;
    } else {
      used = (unsigned short) (board->used & (1<<*modChan));
    }

  if (!((STREQ(name, "all")) || (STREQ(name, "fippi")))) 
    {
      sprintf(info_string, "The DXP4C2X does not have an FPGA called %s for module %d", name, board->mod);
      status = DXP_BAD_PARAM;
      dxp_log_error("dxp_download_fpga_done",info_string,status);
      return status;
    }
  
  /* Read back the CSR to determine if the download was successfull.  */
	
  if((status=dxp_read_gsr(&ioChan,&data))!=DXP_SUCCESS){
    sprintf(info_string," failed to read GSR for module %d", board->mod);
    dxp_log_error("dxp_download_fpga_done",info_string,status);
    return status;
  }
  for(chan=0;chan<4;chan++)
    {
      /* if not used, then we succeed */
      if((used&(1<<chan))==0) continue;

      /* Check the GSR bits */
      if((data&(0x0100<<chan))!=0){
	sprintf(info_string,
		"FiPPI download error (CSR bits) for module %d chan %d",
		board->mod,chan);
	status = DXP_FPGADOWNLOAD;
	dxp_log_error("dxp_download_fpga_done",info_string,status);
	return status;
      }
    }
  
  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to download the DSP Program
 * 
 * This routine downloads the DSP program to the specified CAMAC slot and 
 * channel on the DXP.  If -1 for the DXP channel is specified then all 
 * channels are downloaded.
 *
 ******************************************************************************/
static int dxp_download_dspconfig(int* ioChan, int* modChan, Dsp_Info* dsp)
	 /* int *ioChan;					Input: I/O channel of DXP module		*/
	 /* int *modChan;				Input: DXP channel no (-1,0,1,2,3)	*/
	 /* Dsp_Info *dsp;				Input: DSP structure					*/
{
  /*
   *   Download the DSP configuration file to a single channel or all channels 
   *                          of a single DXP module.
   *
   */

  int status;
  char info_string[INFO_LEN];
  unsigned short data;
  unsigned int length;
  unsigned short start_addr;
  unsigned int j,nxfers,xlen;
  unsigned int maxblk;

  if((*modChan<-1)||(*modChan>3)){
	sprintf(info_string,"called with DXP channel number %d",*modChan);
	status = DXP_BAD_PARAM;
	dxp_log_error("dxp_download_dspconfig",info_string,status);
  }
	
       
  status = dxp_read_csr(ioChan, &data);
  if (status!=DXP_SUCCESS){
	dxp_log_error("dxp_download_dspconfig","Error reading the CSR",status);
	return status;
  }
  /* Mask off the chanel bits */
  data &= ~MASK_CHANNELS;

  /* Write to CSR to initiate download */
	
  data |= MASK_DSPRESET;
  if (*modChan==ALLCHAN) data |= MASK_ALLCHAN;
  else data |= (*modChan<<6);
   
  status = dxp_write_csr(ioChan, &data);
  if (status!=DXP_SUCCESS){
	dxp_log_error("dxp_download_dspconfig","Error writing to the CSR",status);
	return status;
  }
  /* 
   * Transfer the DSP program code.  Skip the first word and 
   * return to it at the end.
   */
  length = dsp->proglen - 2;

  /* Retrieve MAXBLK and check if single transfer is needed */
  maxblk=dxp4c2x_md_get_maxblk();
  if (maxblk <= 0) maxblk = length;

  /* prepare for the first pass thru loop */
  nxfers = ((length-1)/maxblk) + 1;
  xlen = ((maxblk>=length) ? length : maxblk);
  j = 0;
  do {

	/* now write the data */
	start_addr = (unsigned short) (j*maxblk + 1);
	status = dxp_write_block(ioChan, modChan, &start_addr, &xlen, &(dsp->data[j*maxblk+2]));
	if (status!=DXP_SUCCESS) {
	  status = DXP_WRITE_BLOCK;
	  sprintf(info_string,"Error in  %dth block transfer",j);
	  dxp_log_error("dxp_download_dspconfig",info_string,status);
	  return status;
	}
	/* Next loop */
	j++;
	/* On last pass thru loop transfer the remaining bytes */
	if (j==(nxfers-1)) xlen=((length-1)%maxblk) + 1;
  } while (j<nxfers);
	
  /* Now write the first word of the DSP program to start the program running. */

  xlen = 2;
  start_addr = 0;
  status = dxp_write_block(ioChan, modChan, &start_addr, &xlen, &(dsp->data[0]));

  /*
   * All done, clear the LAM on this module 
   */
  if((status=dxp_clear_LAM(ioChan, modChan))!=DXP_SUCCESS){
	dxp_log_error("dxp_download_dspconfig","Unable to clear LAM",status);
  }

  return status;
}
	
/******************************************************************************
 *
 * Routine to check that the DSP is done with its initialization.  
 * If the routine returns DXP_SUCCESS, then the DSP is ready to run.
 *
 ******************************************************************************/
static int dxp_download_dsp_done(int* ioChan, int* modChan, int* mod, 
								 Dsp_Info* dsp, unsigned short* value, 
								 float* timeout)
	 /* int *ioChan;						Input: I/O channel of the module		*/
	 /* int *modChan;					Input: Module channel number			*/
	 /* int *mod;						Input: Module number, for errors		*/
	 /* Dsp_Info *dsp;					Input: Relavent DSP info				*/
	 /* unsigned short *value;			Input: Value to match for BUSY		*/
	 /* float *timeout;					Input: How long to wait, in seconds	*/
{

  int status, i;
  char info_string[INFO_LEN];
  /* The value of the BUSY parameter */
  unsigned short data;
  /* Number of times to divide up the timeout period */
  float divisions = 10.;
  /* Time to wait between checking BUSY */
  float wait;

  wait = (float)(*timeout / divisions);

  sprintf(info_string, "wait/polling time = %f", wait);
  dxp_log_debug("dxp_download_dsp_done", info_string);

  for (i = 0;i < divisions;i++) {

	/* Wait for another period and try again. */
	dxp4c2x_md_wait(&wait);

	/* readout the value of the BUSY parameter */
	if((status=dxp_read_one_dspsymbol(ioChan, modChan,
								  "BUSY", dsp, &data))!=DXP_SUCCESS){
	  sprintf(info_string,"Error reading BUSY from module %d channel %d", *mod, *modChan);
	  dxp_log_error("dxp_download_dsp_done",info_string,status);
	  return status;
	}
	
	sprintf(info_string, "BUSY = %u: Waiting for BUSY = %u", data, *value);
	dxp_log_debug("dxp_download_dsp_done", info_string);

	/* Check if the BUSY parameter matches the requested value
	 * If match then return successful. */
	if (data == (*value)) return DXP_SUCCESS;

  }

  /* If here, then timeout period reached.  Report error. */
  status = DXP_DSPTIMEOUT;
  sprintf(info_string,"Timeout waiting for DSP BUSY=%d from module %d channel %d", 
		  *value, *mod, *modChan);
  dxp_log_error("dxp_download_dsp_done",info_string,status);
  return status;
}

/******************************************************************************
 * 
 * Routine to retrieve the FIPPI program maximum sizes so that memory
 * can be allocated.
 *
 ******************************************************************************/
static int dxp_get_fipinfo(Fippi_Info* fippi)
	 /* Fippi_Info *fippi;				I/O: Structure of FIPPI program Info	*/
{

  fippi->maxproglen = MAXFIP_LEN;

  return DXP_SUCCESS;

}

/******************************************************************************
 * 
 * Routine to retrieve the DSP defaults parameters maximum sizes so that memory
 * can be allocated.
 *
 ******************************************************************************/
static int dxp_get_defaultsinfo(Dsp_Defaults* defaults)
	 /* Dsp_Defaults *defaults;				I/O: Structure of FIPPI program Info	*/
{

  defaults->params->maxsym	= MAXSYM;
  defaults->params->maxsymlen = MAXSYMBOL_LEN;

  return DXP_SUCCESS;

}

/******************************************************************************
 * 
 * Routine to retrieve the DSP program maximum sizes so that memory
 * can be allocated.
 *
 ******************************************************************************/
static int dxp_get_dspinfo(Dsp_Info* dsp)
	 /* Dsp_Info *dsp;							I/O: Structure of DSP program Info	*/
{

  dsp->params->maxsym	    = MAXSYM;
  dsp->params->maxsymlen  = MAXSYMBOL_LEN;
  dsp->maxproglen = MAXDSP_LEN;

  return DXP_SUCCESS;

}

/******************************************************************************
 * Routine to retrieve the DSP program and symbol table
 * 
 * Read the DSP configuration file  -- logical name (or symbolic link) 
 * DSP_CONFIG must be defined prior to execution of the program. At present,
 * it is NOT possible to use different configurations for different DXP 
 * channels.
 *
 ******************************************************************************/
static int dxp_get_dspconfig(Dsp_Info* dsp)
	 /* Dsp_Info *dsp;					I/O: Structure of DSP program Info	*/
{
  FILE  *fp;
  int   status;
  char info_string[INFO_LEN];
   
  sprintf(info_string, "Loading DSP program in %s", dsp->filename); 
  dxp_log_info("dxp_get_dspconfig",info_string);

  /* Now retrieve the file pointer to the DSP program */
	
  if((fp = dxp_find_file(dsp->filename,"r"))==NULL){
	status = DXP_OPEN_FILE;
	sprintf(info_string, "Unable to open %s", dsp->filename); 
	dxp_log_error("dxp_get_dspconfig",info_string,status);
	return status;
  }

  /* Fill in some general information about the DSP program		*/
  dsp->params->maxsym		= MAXSYM;
  dsp->params->maxsymlen	= MAXSYMBOL_LEN;
  dsp->maxproglen			= MAXDSP_LEN;

  /* Load the symbol table and configuration */

  if ((status = dxp_load_dspfile(fp, dsp))!=DXP_SUCCESS) {
	status = DXP_DSPLOAD;
	fclose(fp);
	dxp_log_error("dxp_get_dspconfig","Unable to Load DSP file",status);
	return status;
  }

  /* Close the file and get out */

  fclose(fp);
  dxp_log_info("dxp_get_dspconfig","...DONE!");
	
  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to retrieve the default DSP parameter values
 * 
 * Read the file pointed to by filename and read in the 
 * symbol value pairs into the array.  No writing of data
 * here.
 *
 ******************************************************************************/
static int dxp_get_dspdefaults(Dsp_Defaults* defaults)
	 /* Dsp_Defaults *defaults;					I/O: Structure of DSP defaults    */
{
  FILE  *fp;
  char info_string[INFO_LEN];
  char line[LINE_LEN];
  int   status, i, last;
  char *fstatus=" ";
  unsigned short nsymbols;
  char *token,*delim=" ,:=\r\n\t";
  char strtmp[4];
    
  sprintf(info_string,"Loading DSP defaults in %s",defaults->filename);
  dxp_log_info("dxp_get_dspdefaults",info_string);

  /* Check the filename: if NULL, then there are no parameters */

  for(i=0;i<4;i++) strtmp[i] = (char) toupper(defaults->filename[i]);
  if (strncmp(strtmp,"NULL",4)==0) {
	status = DXP_SUCCESS;
	defaults->params->nsymbol=0;
	return status;
  }

	
  /* Now retrieve the file pointer to the DSP defaults file */

  if ((fp = dxp_find_file(defaults->filename,"r"))==NULL){
	status = DXP_OPEN_FILE;
	sprintf(info_string,"Unable to open %s",defaults->filename);
	dxp_log_error("dxp_get_dspdefaults",info_string,status);
	return status;
  }

  /* Initialize the number of default values */
  nsymbols = 0;

  while(fstatus!=NULL){
	do 
	  fstatus = fgets(line,132,fp);
	while ((line[0]=='*') && (fstatus!=NULL));

	/* Check if we are downloading more parameters than allowed. */
        
	if (nsymbols==MAXSYM){
	  defaults->params->nsymbol = nsymbols;
	  status = DXP_ARRAY_TOO_SMALL;
	  sprintf(info_string,"Too many parameters in %s",defaults->filename);
	  dxp_log_error("dxp_get_dspdefaults",info_string,status);
	  return status;
	}
	if ((fstatus==NULL)) continue;					/* End of file?  or END finishes */
	if (strncmp(line,"END",3)==0) break;

	/* Parse the line for the symbol name, value pairs and store the pairs in the defaults
	 * structure for later use */
        
	token=strtok(line,delim);
	/* Got the symbol name, copy the value */
	defaults->params->parameters[nsymbols].pname = 
	  strcpy(defaults->params->parameters[nsymbols].pname, token);
	/* Now get the value and store it in the data array */
	token = strtok(NULL, delim);
	/* Check if the number is entered as a hex entry? */
	last = strlen(token)-1;
	if ((strncmp(token+last,"h",1)==0)||(strncmp(token+last,"H",1)==0)) {
	  token[last] = '\0';
	  defaults->data[nsymbols] = (unsigned short) strtoul(token,NULL,16);
	} else {
	  defaults->data[nsymbols] = (unsigned short) strtoul(token,NULL,0);
	}


	/* Increment nsymbols and go get the next pair */
	nsymbols++;
  }

  /* assign the number of symbols to the defaults structure */
  defaults->params->nsymbol = nsymbols;

  /* Close the file and get out */

  fclose(fp);
  dxp_log_info("dxp_get_dspdefaults","...DONE!");
  return DXP_SUCCESS;

}

/******************************************************************************
 * Routine to retrieve the DSP program and symbol table
 * 
 * Read the DSP configuration file  -- passed filepointer 
 *
 ******************************************************************************/
static int dxp_load_dspfile(FILE* fp, Dsp_Info* dsp)
	 /* FILE  *fp;							Input: Pointer to the opened DSP file*/
	 /* unsigned short *dspconfig;			Output: Array containing DSP program	*/
	 /* unsigned int *nwordsdsp;				Output: Size of DSP program			*/
	 /* char **dspparam;						Output: Array of DSP param names		*/
	 /* unsigned short *nsymbol;				Output: Number of defined DSP symbols*/
{
  int status;
   
  /* Load the symbol table */
	
  if ((status = dxp_load_dspsymbol_table(fp, dsp))!=DXP_SUCCESS) {
	status = DXP_DSPLOAD;
	dxp_log_error("dxp_load_dspfile","Unable to read DSP symbol table",status);
	return status;
  }
	
  /* Load the configuration */
	
  if ((status = dxp_load_dspconfig(fp, dsp))!=DXP_SUCCESS) {
	status = DXP_DSPLOAD;
	dxp_log_error("dxp_load_dspfile","Unable to read DSP configuration",status);
	return status;
  }	
	
  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to read in the DSP program
 *
 ******************************************************************************/
static int dxp_load_dspconfig(FILE* fp, Dsp_Info* dsp)
	 /* FILE *fp;							Input: File pointer from which to read the symbols	*/
	 /* unsigned short *dspconfig;			Output: Array containing DSP program				*/
	 /* unsigned int *nwordsdsp;				Output: Size of DSP program							*/
{

  int i,status;
  char line[LINE_LEN];
  int nchars;
	
  unsigned int dsp0, dsp1;

  /* Check if we have some allocated memory space	*/
  if (dsp->data==NULL){
	status = DXP_NOMEM;
	dxp_log_error("dxp_load_dspconfig",
				  "Error allocating space for configuration",status);
	return status;
  }
  /*
   *  and read the configuration
   */
  dsp->proglen = 0;
  while(fgets(line,132,fp)!=NULL) {
	nchars = strlen(line);
	while ((nchars>0) && !isxdigit(line[nchars])) {
	  nchars--;
	}
	for(i=0;i<nchars;i+=6){
	  sscanf(&line[i],"%4X%2X",&dsp0,&dsp1);
	  dsp->data[(dsp->proglen)++] = (unsigned short) dsp0;
	  dsp->data[(dsp->proglen)++] = (unsigned short) dsp1;
	}
  }

  return DXP_SUCCESS;
}	

/******************************************************************************
 * Routine to read in the DSP symbol name list
 *
 ******************************************************************************/
static int dxp_load_dspsymbol_table(FILE* fp, Dsp_Info* dsp)
	 /* FILE *fp;						Input: File pointer from which to read the symbols	*/
	 /* char **pnames;					Output: Array of DSP param names					*/
	 /* unsigned short *nsymbol;			Output: Number of defined DSP symbols				*/
{
  int status, retval;
  char info_string[INFO_LEN];
  char line[LINE_LEN];
  unsigned short i;
  /* Hold the character representing the access type *=R/W -=RO */
  char atype[2];

  /*
   *  Read comments and number of symbols
   */
  while(fgets(line,132,fp)!=NULL){
	if (line[0]=='*') continue;
	sscanf(line,"%hd",&(dsp->params->nsymbol));
	break;
  }
  if (dsp->params->nsymbol>0){
	/*
	 *  Allocate space and read symbols
	 */
	if (dsp->params->parameters==NULL){
	  status = DXP_NOMEM;
	  dxp_log_error("dxp_load_dspsymbol_table",
					"Memory not allocated for all parameter names",status);
	  return status;
	}
	for(i=0;i<(dsp->params->nsymbol);i++){
	  if (dsp->params->parameters[i].pname==NULL){
		status = DXP_NOMEM;
		dxp_log_error("dxp_load_dspsymbol_table",
					  "Memory not allocated for single parameter name",status);
		return status;
	  }
	  if (fgets(line, 132, fp)==NULL) {
		status = DXP_BAD_PARAM;
		dxp_log_error("dxp_load_dspsymbol_table",
					  "Error in SYMBOL format of DSP file",status);
		return status;
	  }
	  retval = sscanf(line, "%s %1s %hd %hd", dsp->params->parameters[i].pname, atype, 
					  &(dsp->params->parameters[i].lbound), &(dsp->params->parameters[i].ubound));
	  dsp->params->parameters[i].address = i;
	  dsp->params->parameters[i].access = 1;
	  if (retval>1) {
		if (strcmp(atype,"-")==0) dsp->params->parameters[i].access = 0;
	  }
	  if (retval==2) {
		dsp->params->parameters[i].lbound = 0;
		dsp->params->parameters[i].ubound = 0;
	  }
	  if (retval==3) {
		status = DXP_BAD_PARAM;
		sprintf(info_string, "Error in SYMBOL(%s) format of DSP file: 3 parameters found",
				dsp->params->parameters[i].pname);
		dxp_log_error("dxp_load_dspsymbol_table", info_string, status);
		return status;
	  }
	}
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to return the symbol name at location index in the symbol table
 * of the DSP 
 *
 ******************************************************************************/
static int dxp_symbolname(unsigned short* lindex, Dsp_Info* dsp, char string[])
	 /* unsigned short *lindex;				Input: address of parameter			*/
	 /* Dsp_Info *dsp;						Input: dsp structure with info		*/
	 /* char string[];						Output: parameter name				*/
{

  int status;
	
  /* There better be an array call symbolname! */
	
  if (*lindex>=(dsp->params->nsymbol)) {
	status = DXP_INDEXOOB;
	dxp_log_error("dxp_symbolname","Index greater than the number of symbols",status);
	return status;
  }
  strcpy(string,(dsp->params->parameters[*lindex].pname));

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to locate a symbol in the DSP symbol table.
 * 
 * This routine returns the address of the symbol called name in the DSP
 * symbol table.
 *
 ******************************************************************************/
static int dxp_loc(char name[], Dsp_Info* dsp, unsigned short* address)
	 /* char name[];					Input: symbol name for accessing YMEM params */
	 /* Dsp_Info *dsp;				Input: dsp structure with info				*/
	 /* unsigned short *address;		Output: address in YMEM                      */
{
  /*  
   *       Find pointer into parameter memmory using symbolic name
   *
   *  NOTE: THIS IS NOT FORTRAN CALLABLE:  FORTRAN USERS MUST CALL
   *         DXP_LOC_FORTRAN(symbol)
   */
  int status;
  char info_string[INFO_LEN];
  unsigned short i;

  if((dsp->proglen)<=0){
	status = DXP_DSPLOAD;
	sprintf(info_string, "Must Load DSP code before searching for %s", name);
	dxp_log_error("dxp_loc", info_string, status);
	return status;
  }
	
  *address = USHRT_MAX;
  for(i=0;i<(dsp->params->nsymbol);i++) {
	if (  (strlen(name)==strlen((dsp->params->parameters[i].pname))) &&
		  (strstr(name,(dsp->params->parameters[i].pname))!=NULL)  ) {
	  *address = i;
	  break;
	}
  }
	
  /* Did we find the Symbol in the table? */
	
  status = DXP_SUCCESS;
  if(*address == USHRT_MAX){
	status = DXP_NOSYMBOL;
	/*		sprintf(info_string, "Cannot find <%s> in symbol table",name);
			dxp_log_error("dxp_loc",info_string,status);
	*/	}

  return status;
}

/******************************************************************************
 * Routine to dump the memory contents of the DSP.
 * 
 * This routine prints the memory contents of the DSP in channel modChan 
 * of the DXP of ioChan.
 *
 ******************************************************************************/
static int dxp_dspparam_dump(int* ioChan, int* modChan, Dsp_Info* dsp)
	 /* int *ioChan;						Input: I/O channel of DXP module        */
	 /* int *modChan;					Input: DXP channels no (0,1,2,3)        */
	 /* Dsp_Info *dsp;					Input: dsp structure with info		*/
{
  /*
   *   Read the parameter memory from a single channel on a signle DXP module
   *              and display it along with their symbolic names
   */
  unsigned short *data;
  int status;
  int ncol, nleft;
  int i;
  char buf[128];
  int col[4]={0,0,0,0};
  unsigned int nsymbol;

  nsymbol = (unsigned int) dsp->params->nsymbol;
  /* Allocate memory to read out parameters */
  data = (unsigned short *) dxp4c2x_md_alloc(nsymbol*sizeof(unsigned short));

  if ((status=dxp_read_block(ioChan,
							 modChan,
							 &startp,
							 &nsymbol,
							 data))!=DXP_SUCCESS) {
	dxp_log_error("dxp_mem_dump"," ",status);
	dxp4c2x_md_free(data);
	return status;
  }
  dxp4c2x_md_puts("\r\nDSP Parameters Memory Dump:\r\n");
  dxp4c2x_md_puts(" Parameter  Value    Parameter  Value  ");
  dxp4c2x_md_puts("Parameter  Value    Parameter  Value\r\n");
  dxp4c2x_md_puts("_________________________________________");
  dxp4c2x_md_puts("____________________________________\r\n");
	
  ncol = (int) (((float) nsymbol) / ((float) 4.));
  nleft = nsymbol%4;
  if (nleft==1) { 
	col[0] = 0;
	col[1] = 1;
	col[2] = 1;
	col[3] = 1;
  }
  if (nleft==2) { 
	col[0] = 0;
	col[1] = 1;
	col[2] = 2;
	col[3] = 2;
  }
  if (nleft==3) { 
	col[0] = 0;
	col[1] = 1;
	col[2] = 2;
	col[3] = 3;
  }
  for (i=0;i<ncol;i++) {
	sprintf(buf, 
			"%11s x%4.4x | %11s x%4.4x | %11s x%4.4x | %11s x%4.4x\r\n",
			dsp->params->parameters[i].pname, data[i+col[0]],
			dsp->params->parameters[i+ncol+col[1]].pname, data[i+ncol+col[1]],
			dsp->params->parameters[i+2*ncol+col[2]].pname, data[i+2*ncol+col[2]],
			dsp->params->parameters[i+3*ncol+col[3]].pname, data[i+3*ncol+col[3]]);
	dxp4c2x_md_puts(buf);
  }

  /* Add any strays that are left over at the bottom! */

  if (nleft>0) {
	sprintf(buf, "%11s x%4.4x | ",
			dsp->params->parameters[ncol+col[0]].pname, data[ncol+col[0]]);
	for (i=1;i<nleft;i++) {
	  sprintf(buf, 
			  "%s%11s x%4.4x | ", buf,
			  dsp->params->parameters[i*ncol+ncol+col[i]].pname, data[i*ncol+ncol+col[i]]);
	}
	sprintf(buf, "%s \r\n", buf);
	dxp4c2x_md_puts(buf);
  }

  /* Throw in a final line skip, if there was a stray symbol or 2 on the end of the 
   * table, else it was already added by the dxp_md_puts() above */

  if (nleft!=0) dxp4c2x_md_puts("\r\n");
	
  /* Free the allocated memory */
  dxp4c2x_md_free(data);

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to test the DSP spetrum memory
 * 
 * Test the DSP memory for a single channel of a single DXP module by writing
 * a string of words to reserved spectrum memory space, reading them back and 
 * performing a compare.  The input pattern is defined using 
 *
 * see dxp_test_mem for the meaning of *pattern..
 *
 ******************************************************************************/
static int dxp_test_spectrum_memory(int* ioChan, int* modChan, int* pattern, 
									Board *board)
	 /* int *ioChan;					Input: IO channel to test			*/
	 /* int *modChan;				Input: Channel on the module to test*/
	 /* int *pattern;				Input: Pattern to use during testing*/
	 /* Board *board;				Input: Relavent Board info			*/
{
  int status;
  char info_string[INFO_LEN];
  unsigned short start, addr;
  unsigned int len;

  if((status=dxp_loc("SPECTSTART", board->dsp[*modChan], &addr))!=DXP_SUCCESS) {
	dxp_log_error("dxp_test_spectrum_memory",
				  "Unable to find SPECTSTART symbol",status);
  }
  start = (unsigned short) (board->params[*modChan][addr] + PROGRAM_BASE);
  if((status=dxp_loc("SPECTLEN", board->dsp[*modChan], &addr))!=DXP_SUCCESS){
	dxp_log_error("dxp_test_spectrum_memory",
				  "Unable to find SPECTLEN symbol",status);
  }
  len = (unsigned int) board->params[*modChan][addr];
	
  if((status = dxp_test_mem(ioChan, modChan, pattern, 
							&len, &start))!=DXP_SUCCESS) {
	sprintf(info_string,
			"Error testing spectrum memory for IO channel %d, channel %d",*ioChan, *modChan);
	dxp_log_error("dxp_test_spectrum_memory",info_string,status);
	return status;
  }
  return status;
}

/******************************************************************************
 * Routine to test the DSP baseline memory
 * 
 * Test the DSP memory for a single channel of a single DXP module by writing
 * a string of words to reserved baseline memory space, reading them back and 
 * performing a compare.  The input pattern is defined using 
 *
 * see dxp_test_mem for the meaning of *pattern..
 *
 ******************************************************************************/
static int dxp_test_baseline_memory(int* ioChan, int* modChan, int* pattern, 
									Board* board)
	 /* int *ioChan;						Input: IO channel to test				*/
	 /* int *modChan;					Input: Channel on the module to test	*/
	 /* int *pattern;					Input: Pattern to use during testing	*/
	 /* Board *board;					Input: Relavent Board info				*/
{
  int status;
  char info_string[INFO_LEN];
  unsigned short start, addr;
  unsigned int len;

  if((status=dxp_loc("BASESTART", board->dsp[*modChan], &addr))!=DXP_SUCCESS){
	dxp_log_error("dxp_test_baseline_memory",
				  "Unable to find BASESTART symbol",status);
  }
  start = (unsigned short) (board->params[*modChan][addr] + DATA_BASE);
  if((status=dxp_loc("BASELEN", board->dsp[*modChan], &addr))!=DXP_SUCCESS){
	dxp_log_error("dxp_test_baseline_memory",
				  "Unable to find BASELEN symbol",status);
  }
  len = (unsigned int) board->params[*modChan][addr];
	
  if((status = dxp_test_mem(ioChan, modChan, pattern, 
							&len, &start))!=DXP_SUCCESS) {
	sprintf(info_string,
			"Error testing baseline memory for IO channel %d, channel %d",*ioChan, *modChan);
	dxp_log_error("dxp_test_baseline_memory",info_string,status);
	return status;
  }

  return status;
}

/******************************************************************************
 * Routine to test the DSP event buffermemory
 * 
 * Test the DSP memory for a single channel of a single DXP module by writing
 * a string of words to reserved event buffer memory space, reading them back and 
 * performing a compare.  The input pattern is defined using 
 *
 * see dxp_test_mem for the meaning of *pattern..
 *
 ******************************************************************************/
static int dxp_test_event_memory(int* ioChan, int* modChan, int* pattern, 
								 Board* board)
	 /* int *ioChan;						Input: IO channel to test				*/
	 /* int *modChan;					Input: Channel on the module to test	*/
	 /* int *pattern;					Input: Pattern to use during testing	*/
	 /* Board *board;					Input: Relavent Board info				*/
{
  int status;
  char info_string[INFO_LEN];
  unsigned short start, addr;
  unsigned int len;

  /* Now read the Event Buffer base address and length and store in
   * static library variables for future use. */

  if((status=dxp_loc("EVTBSTART", board->dsp[*modChan], &addr))!=DXP_SUCCESS){
	dxp_log_error("dxp_test_event_memory",
				  "Unable to find EVTBSTART symbol",status);
  }
  start = (unsigned short) (board->params[*modChan][addr] + DATA_BASE);
  if((status=dxp_loc("EVTBLEN", board->dsp[*modChan], &addr))!=DXP_SUCCESS){
	dxp_log_error("dxp_test_event_memory",
				  "Unable to find EVTBLEN symbol",status);
  }
  len = (unsigned int) board->params[*modChan][addr];

  if((status = dxp_test_mem(ioChan, modChan, pattern,
							&len, &start))!=DXP_SUCCESS) {
	sprintf(info_string,
			"Error testing baseline memory for IO channel %d, channel %d",*ioChan, *modChan);
	dxp_log_error("dxp_test_event_memory",info_string,status);
	return status;
  }

  return status;
}

/******************************************************************************
 * Routine to test the DSP memory
 * 
 * Test the DSP memory for a single channel of a single DXP module by writing
 * a string of words to X or Y memmory, reading them back and performing a
 * compare.  The input string is defined using 
 *
 *  *pattern=0 for 0,1,2....
 *  *pattern=1 for 0xFFFF,0xFFFF,0xFFFF,...
 *  *pattern=2 for 0xAAAA,0x5555,0xAAAA,0x5555...
 *
 ******************************************************************************/
static int dxp_test_mem(int* ioChan, int* modChan, int* pattern, 
						unsigned int* length, unsigned short* addr)
	 /* int *ioChan;						Input: I/O channel of DXP module        */
	 /* int *modChan;					Input: DXP channels no (0,1,2,3)        */
	 /* int *pattern;					Input: pattern to write to memmory      */
	 /* unsigned int *length;			Input: number of 16 bit words to xfer   */
	 /* unsigned short *addr;			Input: start address for transfer       */
{
  int status, nerrors;
  char info_string[INFO_LEN];
  unsigned int j;
  unsigned short *readbuf,*writebuf;

  /* Allocate the memory for the buffers */
  if (*length>0) {
	readbuf = (unsigned short *) dxp4c2x_md_alloc(*length*sizeof(unsigned short));
	writebuf = (unsigned short *) dxp4c2x_md_alloc(*length*sizeof(unsigned short));
  } else {
	status=DXP_BAD_PARAM;
	sprintf(info_string,
			"Attempting to test %d elements in DSP memory",*length);
	dxp_log_error("dxp_test_mem",info_string,status);
	return status;
  }

  /* First 256 words of Data memory are reserved for DSP parameters. */
  if((*addr>0x4000)&&(*addr<0x4100)){
	status=DXP_BAD_PARAM;
	sprintf(info_string,
			"Attempting to overwrite Parameter values beginning at address %x",*addr);
	dxp_log_error("dxp_test_mem",info_string,status);
	dxp4c2x_md_free(readbuf);
	dxp4c2x_md_free(writebuf);
	return status;
  }
  switch (*pattern) {
  case 0 : 
	for (j=0;j<(*length)/2;j++) {
	  writebuf[2*j]   = (unsigned short) (j&0x00FFFF00);
	  writebuf[2*j+1] = (unsigned short) (j&0x000000FF);
	}
	break;
  case 1 :
	for (j=0;j<*length;j++) writebuf[j]= 0xFFFF;
	break;
  case 2 :
	for (j=0;j<*length;j++) writebuf[j] = (unsigned short) (((j&2)==0) ? 0xAAAA : 0x5555);
	break;
  default :
	status = DXP_BAD_PARAM;
	sprintf(info_string,"Pattern %d not implemented",*pattern);
	dxp_log_error("dxp_test_mem",info_string,status);
	dxp4c2x_md_free(readbuf);
	dxp4c2x_md_free(writebuf);
	return status;
  }

  /* Write the Pattern to the block of memory */

  if((status=dxp_write_block(ioChan,
							 modChan,
							 addr,
							 length,
							 writebuf))!=DXP_SUCCESS) {
	dxp_log_error("dxp_test_mem"," ",status);
	dxp4c2x_md_free(readbuf);
	dxp4c2x_md_free(writebuf);
	return status;
  }

  /* Read the memory back and compare */
	
  if((status=dxp_read_block(ioChan,
							modChan,
							addr,
							length,
							readbuf))!=DXP_SUCCESS) {
	dxp_log_error("dxp_test_mem"," ",status);
	dxp4c2x_md_free(readbuf);
	dxp4c2x_md_free(writebuf);
	return status;
  }
  for(j=0,nerrors=0; j<*length;j++) if (readbuf[j]!=writebuf[j]) {
	nerrors++;
	status = DXP_MEMERROR;
	if(nerrors<10){
	  sprintf(info_string, "Error: word %d, wrote %x, read back %x",
			  j,((unsigned int) writebuf[j]),((unsigned int) readbuf[j]));
	  dxp_log_error("dxp_test_mem",info_string,status);
	}
  }
	
  if (nerrors!=0){
	sprintf(info_string, "%d memory compare errors found",nerrors);
	dxp_log_error("dxp_test_mem",info_string,status);
	dxp4c2x_md_free(readbuf);
	dxp4c2x_md_free(writebuf);
	return status;
  }

  dxp4c2x_md_free(readbuf);
  dxp4c2x_md_free(writebuf);
  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Set a parameter of the DSP.  Pass the symbol name, value to set and module
 * pointer and channel number.
 *
 ******************************************************************************/
static int dxp_modify_dspsymbol(int* ioChan, int* modChan, char* name, 
								unsigned short* value, Dsp_Info* dsp)
	 /* int *ioChan;					Input: IO channel to write to				*/
	 /* int *modChan;				Input: Module channel number to write to	*/
	 /* char *name;					Input: user passed name of symbol			*/
	 /* unsigned short *value;		Input: Value to set the symbol to			*/
	 /* Dsp_Info *dsp;				Input: Relavent DSP info					*/
{

  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  unsigned int i;
  unsigned short addr;		/* address of the symbol in DSP memory			*/
  char uname[20]="";			/* Upper case version of the user supplied name */
	
  /* Change uname to upper case */
	
  if (strlen(name)>dsp->params->maxsymlen) {
	status = DXP_NOSYMBOL;
	sprintf(info_string, "Symbol name must be <%d characters", dsp->params->maxsymlen);
	dxp_log_error("dxp_modify_dspsymbol", info_string, status);
	return status;
  }
  for (i = 0; i < strlen(name); i++) 
	uname[i] = (char) toupper(name[i]); 

  /* Be paranoid and check if the DSP configuration is downloaded.  If not, do it */
	
  if ((dsp->proglen)<=0) {
	status = DXP_DSPLOAD;
	sprintf(info_string, "Must Load DSP code before modifying %s", name);
	dxp_log_error("dxp_modify_dspsymbol", info_string, status);
	return status;
  }
   
  /* First find the location of the symbol in DSP memory. */

  if ((status  = dxp_loc(uname, dsp, &addr))!=DXP_SUCCESS) {
	sprintf(info_string, "Failed to find symbol %s in DSP memory", uname);
	dxp_log_error("dxp_modify_dspsymbol", info_string, status);
	return status;
  }

  /* Check the access type for this parameter.  Only allow writing to r/w and wo 
   * parameters.
   */

  if (dsp->params->parameters[addr].access==0) {
	sprintf(info_string, "Parameter %s is Read-Only.  No writing allowed.", uname);
	status = DXP_DSPACCESS;
	dxp_log_error("dxp_modify_dspsymbol", info_string, status);
	return status;
  }

  /* Check the bounds, set to min or max if out of bounds. */

  /* Check if there are any bounds defined first */
  if ((dsp->params->parameters[addr].lbound!=0)||(dsp->params->parameters[addr].ubound!=0)) {
	/* Check the lower bound */
	if (*value<dsp->params->parameters[addr].lbound) {
	  sprintf(info_string, "Value is below the lower acceptable bound %i<%i. Changing to lower bound.", 
			  *value, dsp->params->parameters[addr].lbound);
	  status = DXP_DSPPARAMBOUNDS;
	  dxp_log_error("dxp_modify_dspsymbol", info_string, status);
	  /* Set to the lower bound */
	  *value = dsp->params->parameters[addr].lbound;
	}
	/* Check the upper bound */
	if (*value>dsp->params->parameters[addr].ubound) {
	  sprintf(info_string, "Value is above the upper acceptable bound %i<%i. Changing to upper bound.", 
			  *value, dsp->params->parameters[addr].ubound);
	  status = DXP_DSPPARAMBOUNDS;
	  dxp_log_error("dxp_modify_dspsymbol", info_string, status);
	  /* Set to the upper bound */
	  *value = dsp->params->parameters[addr].ubound;
	}
  }

  /* Write the value of the symbol into DSP memory */

  if((status=dxp_write_dsp_param_addr(ioChan,modChan,&(dsp->params->parameters[addr].address),
									  value))!=DXP_SUCCESS){
	sprintf(info_string, "Error writing parameter %s", uname);
	dxp_log_error("dxp_modify_dspsymbol",info_string,status);
	return status;
  }

  return status;
}

/******************************************************************************
 *
 * Write a single parameter to the DSP.  Pass the symbol address, value, module
 * pointer and channel number.
 *
 ******************************************************************************/
static int dxp_write_dsp_param_addr(int* ioChan, int* modChan, 
									unsigned int* addr, unsigned short* value)
	 /* int *ioChan;						Input: IO channel to write to				*/
	 /* int *modChan;					Input: Module channel number to write to		*/
	 /* unsigned int *addr;				Input: address to write in DSP memory		*/
	 /* unsigned short *value;			Input: Value to set the symbol to			*/
{

  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];

  unsigned short saddr;

  /* Move the address into Parameter memory.  The passed address is relative to 
   * base memory. */
	
  saddr = (unsigned short) (*addr + startp);

  if((status=dxp_write_word(ioChan,modChan,&saddr,value))!=DXP_SUCCESS){
	sprintf(info_string, "Error writing parameter at %d", *addr);
	dxp_log_error("dxp_write_dsp_param_addr",info_string,status);
	return status;
  }
	
  return status;
}

/******************************************************************************
 *
 * Read a single parameter of the DSP.  Pass the symbol name, module
 * pointer and channel number.  Returns the value read using the variable value.
 * If the symbol name has the 0/1 dropped from the end, then the 32-bit 
 * value is created from the 0/1 contents.  e.g. zigzag0 and zigzag1 exist
 * as a 32 bit number called zigzag.  if this routine is called with just
 * zigzag, then the 32 bit number is returned, else the 16 bit value
 * for zigzag0 or zigzag1 is returned depending on what was passed as *name.
 *
 ******************************************************************************/
static int dxp_read_dspsymbol(int* ioChan, int* modChan, char* name, 
							  Dsp_Info* dsp, double* value)
	 /* int *ioChan;					Input: IO channel to write to				*/
	 /* int *modChan;				Input: Module channel number to write to	*/
	 /* char *name;					Input: user passed name of symbol			*/
	 /* Dsp_Info *dsp;				Input: Reference to the DSP structure		*/
	 /* double *value;		        Output: Value to set the symbol to			*/
{

  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  unsigned int i;
  unsigned short nword = 1;     /* How many words does this symbol contain?		*/
  unsigned short addr=0;		/* address of the symbol in DSP memory			*/
  unsigned short addr1=0;		/* address of the 2nd word in DSP memory		*/
  unsigned short addr2=0;		/* address of the 3rd word (REALTIME/LIVETIME)	*/
  char uname[20]="", tempchar[20]="";	/* Upper case version of the user supplied name */
  unsigned short stemp;		    /* Temp location to store word read from DSP	*/
  double dtemp, dtemp1;         /* Long versions for the temporary variable		*/

  /* Check that the length of the name is less than maximum allowed length */

  if (strlen(name)>(dsp->params->maxsymlen)) 
	{
	  status = DXP_NOSYMBOL;
	  sprintf(info_string, "Symbol Name must be <%i characters", dsp->params->maxsymlen);
	  dxp_log_error("dxp_read_dspsymbol", info_string, status);
	  return status;
	}
  
  /* Convert the name to upper case for comparison */

  for (i = 0; i < strlen(name); i++) uname[i] = (char) toupper(name[i]); 

  /* Be paranoid and check if the DSP configuration is downloaded.  If not, warn the user */
  
  if ((dsp->proglen)<=0) 
	{
	  status = DXP_DSPLOAD;
	  sprintf(info_string, "Must Load DSP code before reading %s", name);
	  dxp_log_error("dxp_read_dspsymbol",info_string,status);
	  return status;
	}
    
  /* First find the location of the symbol in DSP memory. */

  status = dxp_loc(uname, dsp, &addr);
  if (status != DXP_SUCCESS) 
	{
	  /* Failed to find the name directly, add 0 to the name and search again */
	  strcpy(tempchar, uname);
	  sprintf(tempchar, "%s0",uname);
	  nword = 2;
	  status = dxp_loc(tempchar, dsp, &addr);
	  if (status != DXP_SUCCESS) 
		{
		  /* Failed to find the name with 0 attached, this symbol doesnt exist */
		  sprintf(info_string, "Failed to find symbol %s in DSP memory", name);
		  dxp_log_error("dxp_read_dspsymbol", info_string, status);
		  return status;
		}
	  /* Search for the 2nd entry now */
	  sprintf(tempchar, "%s1",uname);
	  status = dxp_loc(tempchar, dsp, &addr1);
	  if (status != DXP_SUCCESS)
		{
		  /* Failed to find the name with 1 attached, this symbol doesnt exist */
		  sprintf(info_string, "Failed to find symbol %s+1 in DSP memory", name);
		  dxp_log_error("dxp_read_dspsymbol", info_string, status);
		  return status;
		}
	  /* Special case of REALTIME or LIVETIME that are 48 bit numbers */
	  if ((strcmp(uname, "LIVETIME")==0)||(strcmp(uname, "REALTIME")==0))
		{
		  nword = 3;
		  /* Search for the 3rd entry now */
		  sprintf(tempchar, "%s2",uname);
		  status = dxp_loc(tempchar, dsp, &addr2);
		  if (status != DXP_SUCCESS) 
			{
			  /* Failed to find the name with 1 attached, this symbol doesnt exist */
			  sprintf(info_string, "Failed to find symbol %s+2 in DSP memory", name);
			  dxp_log_error("dxp_read_dspsymbol", info_string, status);
			  return status;
			}
		}
	}
  
  /* Check the access type for this parameter.  Only allow reading from r/w and ro 
   * parameters.
   */
  
  if (dsp->params->parameters[addr].access==2) 
	{
	  sprintf(info_string, "Parameter %s is Write-Only", name);
	  status = DXP_DSPACCESS;
	  dxp_log_error("dxp_read_dspsymbol", info_string, status);
	  return status;
	}

  /* Read the value of the symbol from DSP memory */
  addr = (unsigned short) (dsp->params->parameters[addr].address + DSP_DATA_MEM_OFFSET);
  status = dxp_read_word(ioChan, modChan, &addr, &stemp);
  if (status != DXP_SUCCESS)
	{
	  sprintf(info_string, "Error writing parameter %s", name);
	  dxp_log_error("dxp_read_dspsymbol",info_string,status);
	  return status;
	}
  dtemp = (double) stemp;

  /* If there is a second word, read it in */
  if (nword > 1) 
	{
	  addr = (unsigned short) (dsp->params->parameters[addr1].address + startp);
	  status = dxp_read_word(ioChan, modChan, &addr, &stemp);
	  if (status != DXP_SUCCESS)
		{
		  sprintf(info_string, "Error writing parameter %s+1", name);
		  dxp_log_error("dxp_read_dspsymbol",info_string,status);
		  return status;
		}
	  dtemp1 = (double) stemp;
	  /* For 2 words, create the proper 32 bit number */
	  *value = dtemp * 65536. + dtemp1;
	} else {
	  /* For single word values, assign it for the user */
	  *value = dtemp;
	}

  /* Special case of 48 Bit numbers */
  if (nword==3) 
	{
	  addr = (unsigned short) (dsp->params->parameters[addr2].address + startp);
	  status = dxp_read_word(ioChan, modChan, &addr, &stemp);
	  if (status != DXP_SUCCESS)
		{
		  sprintf(info_string, "Error writing parameter %s+2", name);
		  dxp_log_error("dxp_read_dspsymbol",info_string,status);
		  return status;
		}
	  /* For 2 words, create the proper 32 bit number */
	  *value += ((double) stemp) * 65536. * 65536.;
	}
	
  return status;

}

/******************************************************************************
 * Routine to readout the parameter memory from a single DSP.
 * 
 * This routine reads the parameter list from the DSP pointed to by ioChan and
 * modChan.  It returns the array to the caller.
 *
 ******************************************************************************/
static int dxp_read_dspparams(int* ioChan, int* modChan, Dsp_Info* dsp, 
							  unsigned short* params)
	 /* int *ioChan;						Input: I/O channel of DSP		*/
	 /* int *modChan;					Input: module channel of DSP		*/
	 /* Dsp_Info *dsp;					Input: Relavent DSP info			*/
	 /* unsigned short *params;			Output: array of DSP parameters	*/
{
  int status;
  unsigned int len;		/* Length of the DSP parameter memory */
	
  /* Read out the parameters from the DSP memory, stored in Y memory */
	
  len = dsp->params->nsymbol;
  if((status=dxp_read_block(ioChan,modChan,&startp,&len,params))!=DXP_SUCCESS){
	dxp_log_error("dxp_read_dspparams","error reading parameters",status);
	return status;
  }

  return status;
}

/******************************************************************************
 * Routine to write parameter memory to a single DSP.
 * 
 * This routine writes the parameter list to the DSP pointed to by ioChan and
 * modChan.
 *
 ******************************************************************************/
static int dxp_write_dspparams(int* ioChan, int* modChan, Dsp_Info* dsp, 
							   unsigned short* params)
	 /* int *ioChan;						Input: I/O channel of DSP		*/
	 /* int *modChan;					Input: module channel of DSP		*/
	 /* Dsp_Info *dsp;					Input: Relavent DSP info			*/
	 /* unsigned short *params;			Input: array of DSP parameters	*/
{

  int status;
  unsigned int len;		/* Length of the DSP parameter memory */
	
  /* Read out the parameters from the DSP memory, stored in Y memory */

  len = dsp->params->nsymbol;
	
  if((status=dxp_write_block(ioChan,modChan,&startp,&len,params))!=DXP_SUCCESS){
	dxp_log_error("dxp_write_dspparams","error reading parameters",status);
	return status;
  }

  return status;
}

/******************************************************************************
 * Routine to return the length of the spectrum memory.
 *
 * For 4C-2X boards, this value is stored in the DSP and dynamic.  
 * For 4C boards, it is fixed.
 * 
 ******************************************************************************/
static unsigned int dxp_get_spectrum_length(Dsp_Info* dsp, 
											unsigned short* params)
	 /* Dsp_Info *dsp;					Input: Relavent DSP info			*/
	 /* unsigned short *params;			Input: Array of DSP parameters	*/
{
  int status;
  unsigned short addr, addr1;

  if((status=dxp_loc("MCALIMHI", dsp, &addr))!=DXP_SUCCESS){
	dxp_log_error("dxp_get_spectrum_length",
				  "Unable to find SPECTLEN symbol",status);
	return 0;
  }

  if((status=dxp_loc("MCALIMLO", dsp, &addr1))!=DXP_SUCCESS){
	dxp_log_error("dxp_get_spectrum_length",
				  "Unable to find SPECTLEN symbol",status);
	return 0;
  }

  return ((unsigned int) (params[addr] - params[addr1]));

}


/** @brief Get the length of the SCA data buffer
 *
 */
XERXES_STATIC unsigned int dxp_get_sca_length(Dsp_Info *dsp, unsigned short *params)
{
  int status;

  unsigned short addr;


  ASSERT(dsp != NULL);
  ASSERT(params != NULL);


  status = dxp_loc("SCADLEN", dsp, &addr);
  
  if (status != DXP_SUCCESS) {
	dxp_log_error("dxp_get_sca_length", "Unable to find SCADLEN symbol", status);
	return 0;
  }

  /* The user is expecting to create an array of longs with one element per SCA, whereas
   * SCADLEN reports back the total number of words in the buffer,
   * which is twice the number of SCAs.
   */
  return ((unsigned int)(params[addr] / 2));
}


/******************************************************************************
 * Routine to return the length of the baseline memory.
 *
 * For 4C-2X boards, this value is stored in the DSP and dynamic.  
 * For 4C boards, it is fixed.
 *
 ******************************************************************************/
static unsigned int dxp_get_baseline_length(Dsp_Info* dsp, 
											unsigned short* params)
	 /* Dsp_Info *dsp;					Input: Relavent DSP info			*/
	 /* unsigned short *params;			Input: Array of DSP parameters	*/
{
  int status;
  unsigned short addr;

  if((status=dxp_loc("BASELEN", dsp, &addr))!=DXP_SUCCESS){
	dxp_log_error("dxp_get_baseline_length",
				  "Unable to find BASELEN symbol",status);
	return 0;
  }

  return ((unsigned int) params[addr]);

}

/******************************************************************************
 * Routine to return the length of the event memory.
 *
 * For 4C-2X boards, this value is stored in the DSP and dynamic.  
 * For 4C boards, it is fixed.
 * 
 ******************************************************************************/
static unsigned int dxp_get_event_length(Dsp_Info* dsp, unsigned short* params)
	 /* Dsp_Info *dsp;					Input: Relavent DSP info			*/
	 /* unsigned short *params;			Input: Array of DSP parameters	*/
{
  int status;
  unsigned short addr;

  if((status=dxp_loc("EVTBLEN", dsp, &addr))!=DXP_SUCCESS){
	dxp_log_error("dxp_get_event_length",
				  "Unable to find EVTBLEN symbol",status);
	return 0;
  }

  return ((unsigned int) params[addr]);

}

/******************************************************************************
 * Routine to return the length of the history buffer.
 *
 * For 4C-2X boards, this value is stored in the DSP and dynamic.  
 * For 4C boards, it is fixed.
 * 
 ******************************************************************************/
static unsigned int dxp_get_history_length(Dsp_Info* dsp, unsigned short* params)
	 /* Dsp_Info *dsp;					Input: Relavent DSP info			*/
	 /* unsigned short *params;			Input: Array of DSP parameters	*/
{
  int status;
  unsigned short addr;

  if((status=dxp_loc("HSTLEN", dsp, &addr))!=DXP_SUCCESS){
	dxp_log_error("dxp_get_history_length",
				  "Unable to find HSTLEN symbol",status);
	return 0;
  }

  return ((unsigned int) params[addr]);

}

/******************************************************************************
 * Routine to readout the spectrum memory from a single DSP.
 *
 * This routine reads the spectrum histogramfrom the DSP pointed to by ioChan and
 * modChan.  It returns the array to the caller.
 *
 ******************************************************************************/
static int dxp_read_spectrum(int* ioChan, int* modChan, Board* board, 
							 unsigned long* spectrum)
	 /* int *ioChan;					Input: I/O channel of DSP		*/
	 /* int *modChan;				Input: module channel of DSP		*/
	 /* Board *board;					Input: Relavent Board info					*/
	 /* unsigned long *spectrum;		Output: array of spectrum values	*/
{

  int status;
  unsigned int i;
  unsigned short *spec, addr, start;
  unsigned int len;					/* number of short words in spectrum	*/

  if((status=dxp_loc("SPECTSTART", board->dsp[*modChan], &addr))!=DXP_SUCCESS) {
	status = DXP_NOSYMBOL;
	dxp_log_error("dxp_read_spectrum",
				  "Unable to find SPECTSTART symbol",status);
	return status;
  }
  start = (unsigned short) (board->params[*modChan][addr] + PROGRAM_BASE);
  len = 2*dxp_get_spectrum_length(board->dsp[*modChan], board->params[*modChan]);

  /* Allocate memory for the spectrum */
  spec = (unsigned short *) dxp4c2x_md_alloc(len*sizeof(unsigned short));

  /* Read the spectrum */

  if((status=dxp_read_block(ioChan,modChan,&start,&len,spec))!=DXP_SUCCESS){
	dxp_log_error("dxp_read_spectrum","Error reading out spectrum",status);
	dxp4c2x_md_free(spec);
	return status;
  }
	
  /* Unpack the array of shorts into the long spectra */
	
  for (i=0;i<((unsigned int) len/2);i++) {
	spectrum[i] = spec[2*i] | spec[2*i+1]<<16;
  }

  /* Free memory used for the spectrum */
  dxp4c2x_md_free(spec);

  return status;
}

/******************************************************************************
 * Routine to readout the baseline histogram from a single DSP.
 * 
 * This routine reads the baselin histogram from the DSP pointed to by ioChan and
 * modChan.  It returns the array to the caller.
 *
 ******************************************************************************/
static int dxp_read_baseline(int* ioChan, int* modChan, Board* board, 
							 unsigned short* baseline)
	 /* int *ioChan;						Input: I/O channel of DSP					*/
	 /* int *modChan;					Input: module channel of DSP					*/
	 /* Board *board;					Input: Relavent Board info					*/
	 /* unsigned short *baseline;		Output: array of baseline histogram values	*/
{

  int status;
  unsigned short addr, start;
  unsigned int len;					/* number of short words in spectrum	*/

  if((status=dxp_loc("BASESTART", board->dsp[*modChan], &addr))!=DXP_SUCCESS) {
	status = DXP_NOSYMBOL;
	dxp_log_error("dxp_read_baseline",
				  "Unable to find BASESTART symbol",status);
	return status;
  }
  start = (unsigned short) (board->params[*modChan][addr] + DATA_BASE);
  len = dxp_get_baseline_length(board->dsp[*modChan], board->params[*modChan]);

	
  /* Read out the basline histogram. */
	
  if((status=dxp_read_block(ioChan,modChan,&start,&len,baseline))!=DXP_SUCCESS){
	dxp_log_error("dxp_read_baseline","Error reading out baseline",status);
	return status;
  }

  return status;
}
	
/******************************************************************************
 * Routine to readout the history buffer from a single DSP.
 * 
 * This routine reads the history buffer from the DSP pointed to by ioChan and
 * modChan.  It returns the array to the caller.
 *
 ******************************************************************************/
static int dxp_read_history(int* ioChan, int* modChan, Board* board, 
							unsigned short* history)
	 /* int *ioChan;						Input: I/O channel of DSP					*/
	 /* int *modChan;					Input: module channel of DSP				*/
	 /* Board *board;					Input: Relavent Board info					*/
	 /* unsigned short *history;			Output: array of history buffervalues		*/
{

  int status;
  unsigned short addr, start;
  unsigned int len;					/* number of short words in spectrum	*/

  if((status=dxp_loc("HSTSTART", board->dsp[*modChan], &addr))!=DXP_SUCCESS) {
	status = DXP_NOSYMBOL;
	dxp_log_error("dxp_read_history",
				  "Unable to find HSTSTART symbol",status);
	return status;
  }
  start = (unsigned short) (board->params[*modChan][addr] + DATA_BASE);
  len = dxp_get_history_length(board->dsp[*modChan], board->params[*modChan]);

	
  /* Read out the basline histogram. */
	
  if((status=dxp_read_block(ioChan,modChan,&start,&len,history))!=DXP_SUCCESS){
	dxp_log_error("dxp_read_history","Error reading out history buffer",status);
	return status;
  }

  return status;
}
	
	
/******************************************************************************
 * Routine to prepare the DXP4C2X for data readout.
 * 
 * This routine will ensure that the board is in a state that 
 * allows readout via CAMAC bus.
 *
 ******************************************************************************/
int dxp_prep_for_readout(int* ioChan, int *modChan)
	 /* int *ioChan;						Input: I/O channel of DXP module			*/
	 /* int *modChan;					Input: Module channel number				*/
{
  /*
   *   Nothing needs to be done for the DXP4C2X, the 
   * CAMAC interface does not interfere with normal data taking.
   */
  int *itemp;

  /* Assign unused input parameters to stop compiler warnings */
  itemp = ioChan;
  itemp = modChan;

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to prepare the DXP4C2X for data readout.
 * 
 * This routine will ensure that the board is in a state that 
 * allows readout via CAMAC bus.
 *
 ******************************************************************************/
int dxp_done_with_readout(int* ioChan, int *modChan, Board *board)
	 /* int *ioChan;						Input: I/O channel of DXP module	*/
	 /* int *modChan;					Input: Module channel number		*/
	 /* Board *board;					Input: Relavent Board info					*/
{
  /*
   * Nothing needs to be done for the DXP4C2X, the CAMAC interface 
   * does not interfere with normal data taking.
   */
  int *itemp;
  Board *btemp;

  /* Assign unused input parameters to stop compiler warnings */
  itemp = ioChan;
  itemp = modChan;
  btemp = board;


  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to begin a data taking run.
 * 
 * This routine starts a run on the specified CAMAC channel.  It tells the DXP
 * whether to ignore the gate signal and whether to clear the MCA.
 *
 ******************************************************************************/
static int dxp_begin_run(int* ioChan, int* modChan, unsigned short* gate, 
						 unsigned short* resume, Board *board)
	 /* int *ioChan;						Input: I/O channel of DXP module		*/
	 /* int *modChan;					Input: Module channel number			*/
	 /* unsigned short *gate;			Input: ignore (1) or use (0) ext. gate	*/
	 /* unsigned short *resume;			Input: clear MCA first(0) or update (1)	*/
	 /* Board *board;					Input: Board information				*/
{
  /* Dummy variable to remove compiler warnings */
  int *itemp;
  Board *btemp;
  /*
   *   Initiate data taking for all channels of a single DXP module. 
   */
  int status;
  unsigned short data;

  char info_string[INFO_LEN];


  itemp = modChan;
  btemp = board;

  sprintf(info_string, "Starting a run: ioChan = %d, modChan = %d, gate = %d, resume = %d", 
		  *ioChan, *modChan, *gate, *resume);
  dxp_log_debug("dxp_begin_run", info_string);

  /* read-modify-write to CSR to start data run */
  status = dxp_read_csr(ioChan, &data);                    /* read to CSR */
  if (status!=DXP_SUCCESS)
    {
      dxp_log_error("dxp_begin_run","Error reading from the CSR",status);
      return status;
    }

  data |= MASK_RUNENABLE;
  /*	data |= (*modChan==ALLCHAN ? MASK_ALLCHAN : *modChan<<6);*/
  data |= (unsigned short) MASK_ALLCHAN;
  if (*resume == CLEARMCA) 
	{
	  data |= MASK_RESETMCA;
	} else {
	  data &= ~MASK_RESETMCA;
	}
  if (*gate == IGNOREGATE) 
	{
	  data |= MASK_IGNOREGATE;
	} else {
	  data &= ~MASK_IGNOREGATE;
	}

  status = dxp_write_csr(ioChan, &data);                    /* write to CSR */
  if (status!=DXP_SUCCESS)
    {
      dxp_log_error("dxp_begin_run","Error writing to the CSR",status);
      return status;
    }

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to end a data taking run.
 * 
 * This routine ends the run on the specified CAMAC channel.
 *
 ******************************************************************************/
static int dxp_end_run(int* ioChan, int* modChan)
{
  int status;

  unsigned short data;

  /*float wait = .001f;*/

  char info_string[INFO_LEN];


  sprintf(info_string, "Stopping a run: ioChan = %d, modChan = %d",
		  *ioChan, *modChan);
  dxp_log_debug("dxp_end_run", info_string);

  status = dxp_read_csr(ioChan, &data);

  if (status != DXP_SUCCESS) {
	dxp_log_error("dxp_end_run", "Error reading CSR", status);
	return status;
  }

  data &= ~MASK_RUNENABLE;
  data |= MASK_ALLCHAN;

  status = dxp_write_csr(ioChan, &data); 

  if (status!=DXP_SUCCESS) {
	dxp_log_error("dxp_end_run","Error writing CSR",status);
	return status;
  }
	
  if((status=dxp_clear_LAM(ioChan, modChan))!=DXP_SUCCESS) {
	dxp_log_error("dxp_end_run"," ",status);
	return status;
  }

  /* Wait for the run to end */
  /*dxp4c2x_md_wait(&wait);*/
	
  return DXP_SUCCESS;    
}

/******************************************************************************
 * Routine to determine if the module thinks a run is active.
 * 
 ******************************************************************************/
XERXES_STATIC int dxp_run_active(int* ioChan, int* modChan, int* active)
{
  int status;

  unsigned short data;

  char info_string[INFO_LEN];


  status = dxp_read_gsr(ioChan, &data);
  
  if (status != DXP_SUCCESS) {
	dxp_log_error("dxp_run_active", "Error reading the GSR", status);
	return status;
  }

  sprintf(info_string, "GSR = %#x", data);
  dxp_log_debug("dxp_run_active", info_string);

  /* Check the run active bit of the GSR */
  if ((data & (0x1<<*modChan))!=0) *active = 1;
/* if ((*ioChan == 3) && (*modChan == 1)) printf("dxp4c2x::dxp_run_active, ioChan=%d, modChan=%d, GSR=0x%x, *active=%d\n",
*ioChan, *modChan, data, *active); */

  return status;    

}


/******************************************************************************
 *
 * Routine to start a control task routine.  Definitions for types are contained
 * in the xerxes_generic.h file.
 * 
 ******************************************************************************/
static int dxp_begin_control_task(int* ioChan, int* modChan, short *type, 
								  unsigned int *length, int *info, Board *board)
{
  int status = DXP_SUCCESS;
  char info_string[INFO_LEN];

  /* Variables for parameters to be read/written from/to the DSP */
  unsigned short runtasks, whichtest, tracewait;

  unsigned short zero=0;
  unsigned short EXTPAGE;
  unsigned short EXTADDRESS;
  unsigned short EXTLENGTH;


  /* Check that the length of allocated memory is greater than 0 */
  if (*length == 0) {
	status = DXP_ALLOCMEM;
	sprintf(info_string,
			"Must pass an array of at least length 1 containing LOOPCOUNT for "
			"module %d chan %d", board->mod, *modChan);
	dxp_log_error("dxp_begin_control_task", info_string, status);
	return status;
  }

  /* Read/Modify/Write the RUNTASKS parameter */
  
  status = dxp_read_one_dspsymbol(ioChan, modChan, "RUNTASKS",
								  board->dsp[*modChan], &runtasks);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error reading RUNTASKS from module %d chan %d",
			board->mod, *modChan);
	dxp_log_error("dxp_begin_control_task", info_string, status);
	return status;
  }

  sprintf(info_string, "RUNTASKS(read) = %#x", runtasks);
  dxp_log_debug("dxp_begin_control_task", info_string);

  if (*type != CT_DXP2X_BASELINE_HIST) {
	runtasks |= CONTROL_TASK;
  }

  if ((*type == CT_ADC)                 ||
	  (*type == CT_DXP2X_ADC)           ||
	  (*type == CT_DXP2X_BASELINE_HIST) ||
	  (*type == CT_DXP2X_READ_MEMORY)   ||
	  (*type == CT_DXP2X_CHECK_MEMORY))
	{
	  runtasks |= STOP_BASELINE;
	}

  sprintf(info_string, "RUNTASKS(modify/write) = %#x", runtasks);
  dxp_log_debug("dxp_begin_control_task", info_string);

  /* write */
  if((status=dxp_modify_dspsymbol(ioChan,modChan,
								  "RUNTASKS",&runtasks,board->dsp[*modChan]))!=DXP_SUCCESS){
	sprintf(info_string,
			"Error writing RUNTASKS from module %d chan %d",board->mod,*modChan);
	dxp_log_error("dxp_begin_control_task",info_string,status);
	return status;
  }

  /* First check if the control task is the ADC readout */
  if (*type==CT_DXP2X_SET_ASCDAC) {
	whichtest = WHICHTEST_SET_ASCDAC;
  } else if ((*type==CT_ADC) || (*type==CT_DXP2X_ADC)) {
	whichtest = WHICHTEST_ACQUIRE_ADC;
	/* Make sure at least the user thinks there is allocated memory */
	if (*length>1) {
	  /* write TRACEWAIT */
	  tracewait = (unsigned short) info[1];
	  if((status=dxp_modify_dspsymbol(ioChan,modChan,"TRACEWAIT",&tracewait,board->dsp[*modChan]))!=DXP_SUCCESS){
		sprintf(info_string,
				"Error writing TRACEWAIT to module %d chan %d",board->mod,*modChan);
		dxp_log_error("dxp_begin_control_task",info_string,status);
		return status;
	  }
	} else {
	  status = DXP_ALLOCMEM;
	  sprintf(info_string,
			  "This control task requires at least 2 parameters for mod %d chan %d",board->mod,*modChan);
	  dxp_log_error("dxp_begin_control_task",info_string,status);
	  return status;
	}

  } else if (*type==CT_DXP2X_TRKDAC) {
	whichtest = WHICHTEST_TRKDAC;

  } else if (*type==CT_DXP2X_SLOPE_CALIB) {
	whichtest = WHICHTEST_SLOPE_CALIB;

  } else if (*type==CT_DXP2X_SLEEP_DSP) {
	whichtest = WHICHTEST_SLEEP_DSP;

  } else if (*type==CT_DXP2X_PROGRAM_FIPPI) {
	whichtest = WHICHTEST_PROGRAM_FIPPI;

  } else if (*type==CT_DXP2X_SET_POLARITY) {
	whichtest = WHICHTEST_SET_POLARITY;

  } else if (*type==CT_DXP2X_CLOSE_INPUT_RELAY) {
	whichtest = WHICHTEST_CLOSE_INPUT_RELAY;

  } else if (*type==CT_DXP2X_OPEN_INPUT_RELAY) {
	whichtest = WHICHTEST_OPEN_INPUT_RELAY;

  } else if (*type==CT_DXP2X_RC_BASELINE) {
	whichtest = WHICHTEST_RC_BASELINE;

  } else if (*type==CT_DXP2X_RC_EVENT) {
	whichtest = WHICHTEST_RC_EVENT;

  } else if (*type==CT_DXP2X_BASELINE_HIST) {

  } else if (*type==CT_DXP2X_RESET) {
	whichtest = WHICHTEST_RESET;

  } else if (*type == CT_DXP2X_CHECK_MEMORY) {
	whichtest = WHICHTEST_CHECK_MEMORY;

  } else if (*type == CT_DXP2X_READ_MEMORY) {

	whichtest = WHICHTEST_READ_MEMORY;

	if (*length < 4) {
	  sprintf(info_string, "'info' array length (%d) is too short for "
			  "ioChan = %d, modChan = %d", *length, *ioChan, *modChan);
	  dxp_log_error("dxp_begin_control_task", info_string, DXP_ARRAY_TOO_SMALL);
	  return DXP_ARRAY_TOO_SMALL;
	}

	/* Individual 'info' components may be set to -1 if they are to be ignored */

	if (info[1] != -1) {
	  EXTPAGE = (unsigned short)info[1]; 

	  status = dxp_modify_dspsymbol(ioChan, modChan, "EXTPAGE", &EXTPAGE,
									board->dsp[*modChan]);

	  if (status != DXP_SUCCESS) {
		sprintf(info_string, "Error writing EXTPAGE for ioChan = %d, "
				"modChan = %d", *ioChan, *modChan);
		dxp_log_error("dxp_begin_control_task", info_string, status);
		return status;
	  }
	}

	if (info[2] != -1) {
	  EXTADDRESS = (unsigned short)info[2];

	  status = dxp_modify_dspsymbol(ioChan, modChan, "EXTADDRESS", &EXTADDRESS,
									board->dsp[*modChan]);

	  if (status != DXP_SUCCESS) {
		sprintf(info_string, "Error writing EXTADDRESS for ioChan = %d, "
				"modChan = %d", *ioChan, *modChan);
		dxp_log_error("dxp_begin_control_task", info_string, status);
		return status;
	  }
	}

	if (info[3] != -1) {
	  EXTLENGTH = (unsigned short)info[3];

	  status = dxp_modify_dspsymbol(ioChan, modChan, "EXTLENGTH", &EXTLENGTH,
									board->dsp[*modChan]);

	  if (status != DXP_SUCCESS) {
		sprintf(info_string, "Error writing EXTLENGTH for ioChan = %d, "
				"modChan = %d", *ioChan, *modChan);
		dxp_log_error("dxp_begin_control_task", info_string, status);
		return status;
	  }
	}

  } else {
	status=DXP_NOCONTROLTYPE;
	sprintf(info_string,
			"Unknown control type '%d' for ioChan = %d, modChan = %d",
			*type, *modChan, *ioChan);
	dxp_log_error("dxp_begin_control_task", info_string, status);
	return status;
  }

  /* For baseline history, we just want to change the RUNTASKS variable to
   * stop filling of the history, so we can readout the buffer without it
   * constantly being modified.
   */

  if (*type != CT_DXP2X_BASELINE_HIST) {

	/* write WHICHTEST */
	status = dxp_modify_dspsymbol(ioChan, modChan, "WHICHTEST", &whichtest,
								  board->dsp[*modChan]);

	if(status != DXP_SUCCESS){
	  sprintf(info_string, "Error writing WHICHTEST '%u' to module %d chan %d",
			  whichtest, board->mod, *modChan);
	  dxp_log_error("dxp_begin_control_task", info_string, status);
	  return status;
	}

	sprintf(info_string, "Setting WHICHTEST to '%u'", whichtest);
	dxp_log_debug("dxp_begin_control_task", info_string);

	/* start the control run */
	if((status=dxp_begin_run(ioChan,modChan,&zero,&zero,board))!=DXP_SUCCESS){
	  sprintf(info_string,
			  "Error starting control run on module %d chan %d",board->mod,*modChan);
	  dxp_log_error("dxp_begin_control_task",info_string,status);
	  return status;
	}

  }

  return DXP_SUCCESS;

}
/******************************************************************************
 *
 * Routine to end a control task routine.
 *
 ******************************************************************************/
static int dxp_end_control_task(int* ioChan, int* modChan, Board *board)
{
  int status = DXP_SUCCESS;
  int mod;

  char info_string[INFO_LEN];
  
  unsigned short temp;
  unsigned short runtasks;


  mod = board->mod;

  if((status=dxp_end_run(ioChan,modChan))!=DXP_SUCCESS){
	
	sprintf(info_string, "Error ending control task run for chan %d", *modChan);
	dxp_log_error("dxp_end_control_task", info_string, status);
	return status;
  }


  /* Read/Modify/Write the RUNTASKS parameter */
  /* read */
  if((status=dxp_read_one_dspsymbol(ioChan,modChan,
								"RUNTASKS",board->dsp[*modChan],&temp))!=DXP_SUCCESS){
	sprintf(info_string,
			"Error reading RUNTASKS from module %d chan %d",board->mod,*modChan);
	dxp_log_error("dxp_end_control_task",info_string,status);
	return status;
  }

  sprintf(info_string, "RUNTASKS (read) = %#x", temp);
  dxp_log_debug("dxp_end_control_task", info_string);

  /* Remove the CONTROL_TASK bit and add the WRITE_BASELINE bit */
  /* modify */
  runtasks = (unsigned short)(temp & (unsigned short) ~CONTROL_TASK);
  
  sprintf(info_string, "RUNTASKS (intermediate) = %#x", runtasks);
  dxp_log_debug("dxp_end_control_task", info_string);

  runtasks &= ~STOP_BASELINE;

  sprintf(info_string, "RUNTASKS (modify/write) = %#x", runtasks);
  dxp_log_debug("dxp_end_control_task", info_string);

  /* write */
  if((status=dxp_modify_dspsymbol(ioChan,modChan,"RUNTASKS",&runtasks,board->dsp[*modChan]))!=DXP_SUCCESS){
	sprintf(info_string,
			"Error writing RUNTASKS from module %d chan %d",board->mod,*modChan);
	dxp_log_error("dxp_end_control_task",info_string,status);
	return status;
  }

  /* Need to start a run here...apparently */
  
  /*
  status = dxp_begin_run(ioChan, modChan, &gate, &resume, board);

  if (status != DXP_SUCCESS) {

	sprintf(info_string, "Error starting run on module %d, chan %d", *modChan, *ioChan);
	dxp_log_error("dxp_end_control_task", info_string, status);
	return status;
  }
  */

  return status;
}

/******************************************************************************
 *
 * Routine to get control task parameters.
 *
 ******************************************************************************/
static int dxp_control_task_params(int* ioChan, int* modChan, short *type,
								   Board *board, int *info)
{
  int status = DXP_SUCCESS;
  int tmp;

  char info_string[INFO_LEN];

  
  /* Keep the compiler happy */
  tmp = *ioChan;

  /* Default values */
  /* nothing to readout here */
  info[0] = 0;
  /* stop when user is ready */
  info[1] = 0;
  /* stop when user is ready */
  info[2] = 0;

  /* Check the control task type */
  if (*type==CT_DXP2X_SET_ASCDAC) {
  } else if ((*type==CT_ADC) || (*type==CT_DXP2X_ADC)) {
	/* length=spectrum length*/
	info[0] = dxp_get_history_length(board->dsp[*modChan], board->params[*modChan]);
	/* Recommend waiting 4ms initially, (TRACEWAIT+4)*25ns*history buffer length */
	info[1] = 4;
	/* Recomment 1ms polling after initial wait */
	info[2] = 1;
  } else if (*type==CT_DXP2X_TRKDAC) {
	/* length=1, the value of the tracking DAC */
	info[0] = 1;
	/* Recommend waiting 10ms initially */
	info[1] = 10;
	/* Recomment 1ms polling after initial wait */
	info[2] = 1;
  } else if (*type==CT_DXP2X_SLOPE_CALIB) {
  } else if (*type==CT_DXP2X_SLEEP_DSP) {
  } else if (*type==CT_DXP2X_PROGRAM_FIPPI) {
  } else if (*type==CT_DXP2X_SET_POLARITY) {
  } else if (*type==CT_DXP2X_CLOSE_INPUT_RELAY) {
  } else if (*type==CT_DXP2X_OPEN_INPUT_RELAY) {
  } else if (*type==CT_DXP2X_RC_BASELINE) {
  } else if (*type==CT_DXP2X_RC_EVENT) {
  } else if (*type==CT_DXP2X_BASELINE_HIST) {
	info[0] = dxp_get_history_length(board->dsp[*modChan], board->params[*modChan]);
	/* Recommend waiting 0ms, baseline history is available immediately */
	info[1] = 0;
	/* Recomment 0ms polling, baseline history is available immediately */
	info[2] = 0;
  } else if (*type==CT_DXP2X_RESET) {

  } else if (*type == CT_DXP2X_READ_MEMORY) {
	
	info[0] = dxp_get_history_length(board->dsp[*modChan],
									 board->params[*modChan]);
	info[1] = (int)(.0005 * info[0]);
	info[2] = 1;

  } else {
	status=DXP_NOCONTROLTYPE;
	sprintf(info_string,
			"Unknown control type %d for this DXP4C2X module",*type);
	dxp_log_error("dxp_control_task_params",info_string,status);
	return status;
  }

  return status;

}

/******************************************************************************
 *
 * Routine to return control task data.
 *
 ******************************************************************************/
static int dxp_control_task_data(int* ioChan, int* modChan, short *type,
								 Board *board, void *data)
{
  int status = DXP_SUCCESS;
  char info_string[INFO_LEN];

  unsigned int i, lenh;
  unsigned short *stemp;
  unsigned long *ltemp;
  unsigned short hststart, circular, offset;

  double dtemp = 0.0;


  ASSERT(ioChan  != NULL);
  ASSERT(modChan != NULL);
  ASSERT(type    != NULL);
  ASSERT(board   != NULL);
  ASSERT(data    != NULL);


  if (*type == CT_DXP2X_SET_ASCDAC) {
	/* Do nothing */

  } else if ((*type == CT_ADC) || 
			 (*type == CT_DXP2X_ADC) ||
			 (*type == CT_DXP2X_BASELINE_HIST) ||
			 (*type == CT_DXP2X_READ_MEMORY))
	{

	  /* allocate memory */
	  lenh = dxp_get_history_length(board->dsp[*modChan],
									board->params[*modChan]);

	  stemp = (unsigned short *)dxp4c2x_md_alloc(lenh * sizeof(unsigned short));
	  
	  if (!stemp) {
		sprintf(info_string, "Not enough memory to allocate temporary array "
				"of length %d",lenh);
		dxp_log_error("dxp_control_task_data", info_string, DXP_NOMEM);
		return DXP_NOMEM;
	  }

	  status = dxp_read_history(ioChan, modChan, board, stemp);

	  if(status != DXP_SUCCESS){
		dxp4c2x_md_free(stemp);
		sprintf(info_string, "Error ending control task run for chan %d",
				*modChan);
		dxp_log_error("dxp_control_task_data", info_string, status);
		return status;
	  }

	  /* Assign the void *data to a local unsigned long * array for assignment 
	   * purposes.  Just easier to read than casting the void * on the left 
	   * hand side.
	   */
	  ltemp = (unsigned long *) data;

	  /* For cases where the data is stored in the baseline history buffer, but
	   * isn't actually baseline history data, we may have other actions to
	   * perform.
	   */
	  if (*type != CT_DXP2X_BASELINE_HIST) {

		/* External memory uses EXTLENGTH instead of HSTLEN to determine
		 * how much of the buffer to read.
		 */
		if (*type == CT_DXP2X_READ_MEMORY) {
		  
		  status = dxp_read_dspsymbol(ioChan, modChan, "EXTLENGTH",
									  board->dsp[*modChan], &dtemp);

		  if (status != DXP_SUCCESS) {
			dxp4c2x_md_free(stemp);
			sprintf(info_string, "Error reading EXTLENGTH for ioChan = %d, "
					"modChan = %d", *ioChan, *modChan);
			dxp_log_error("dxp_control_task_data", info_string, status);
			return status;
		  }

		  lenh = (unsigned short)dtemp;
		}

		for (i = 0; i < lenh; i++) {
		  ltemp[i] = (long) stemp[i];
		}


	  } else {
		/* For the baseline history, get the start of the circular buffer and 
		 * return the array with the start at location 0 instead of the random 
		 * location within the DSP memory.
		 */
		status = dxp_read_one_dspsymbol(ioChan, modChan, "CIRCULAR",
										board->dsp[*modChan], &circular);

		if (status != DXP_SUCCESS) {
		  dxp4c2x_md_free(stemp);
		  sprintf(info_string, "Error reading CIRCULAR from module %d chan %d",
				  board->mod, *modChan);
		  dxp_log_error("dxp_control_task_data", info_string, status);
		  return status;
		}

		status = dxp_read_one_dspsymbol(ioChan, modChan, "HSTSTART",
										board->dsp[*modChan], &hststart);

		if (status != DXP_SUCCESS) {
		  dxp4c2x_md_free(stemp);
		  sprintf(info_string, "Error reading HSTSTART from module %d chan %d",
				  board->mod, *modChan);
		  dxp_log_error("dxp_control_task_data", info_string, status);
		  return status;
		}

		offset = (unsigned short)(circular - hststart);

                /* Do a short cast of the unsigned short to force the compiler to do the
                 * sign-extension properly.  If you try to cast directly into a long,
                 * the compiler will NOT sign extend the unsigned short array */
                for (i = offset; i < lenh; i++) ltemp[i-offset] = (long) ((short) stemp[i]);
                for (i = 0; i < offset; i++) ltemp[i+(lenh-offset)] = (long) ((short) stemp[i]);
	  }

	  /* Free memory */
	  dxp4c2x_md_free(stemp);

	} else if (*type==CT_DXP2X_TRKDAC) {
	} else if (*type==CT_DXP2X_SLOPE_CALIB) {
	} else if (*type==CT_DXP2X_SLEEP_DSP) {
	} else if (*type==CT_DXP2X_PROGRAM_FIPPI) {
	} else if (*type==CT_DXP2X_SET_POLARITY) {
	} else if (*type==CT_DXP2X_CLOSE_INPUT_RELAY) {
	} else if (*type==CT_DXP2X_OPEN_INPUT_RELAY) {
	} else if (*type==CT_DXP2X_RC_BASELINE) {
	} else if (*type==CT_DXP2X_RC_EVENT) {
	} else if (*type==CT_DXP2X_RESET) {
	} else {
	  status=DXP_NOCONTROLTYPE;
	  sprintf(info_string,
			  "Unknown control type %d for this DXP4C2X module",*type);
	  dxp_log_error("dxp_control_task_data",info_string,status);
	  return status;
	}
	
  return status;    

}
/******************************************************************************
 * Routine to start a special Calibration run
 * 
 * Begin a special data run for all channels of of a single DXP module.  
 * Currently supported tasks:
 * SET_ASCDAC
 * ACQUIRE_ADC
 * TRKDAC
 * SLOPE_CALIB
 * SLEEP_DSP
 * PROGRAM_FIPPI
 * SET_POLARITY
 * CLOSE_INPUT_RELAY
 * OPEN_INPUT_RELAY
 * RC_BASELINE
 * RC_EVENT
 *				
 *
 ******************************************************************************/
static int dxp_begin_calibrate(int* ioChan, int* modChan, int* calib_task, Board* board)
	 /* int *ioChan;					Input: I/O channel of DXP module	*/
	 /* int *modChan;				Input: Module channel number		*/
	 /* int *calib_task;				Input: int.gain (1) reset (2)		*/
	 /* Board *board;				Input: Relavent Board info			*/
{
  /*
   */
  int status;
  char info_string[INFO_LEN];
  unsigned short ignore=IGNOREGATE,resume=CLEARMCA;
  unsigned short whichtest= (unsigned short) *calib_task;
  unsigned short runtasks=CONTROL_TASK;

  /* Be paranoid and check if the DSP configuration is downloaded.  If not, do it */
	
  if ((board->dsp[*modChan]->proglen) <= 0) {
	status = DXP_DSPLOAD;
	sprintf(info_string, "Must Load DSP code before calibrations");
	dxp_log_error("dxp_begin_calibrate", info_string, status);
	return status;
  }

  /* Determine if we are performing a valid task */

  if (*calib_task==99) {
	status = DXP_BAD_PARAM;
	dxp_log_error("dxp_begin_calibrate", 
				  "RESET calibration not necc with this model", status);
	return status;
  } else if ((*calib_task==4)||(*calib_task==5)||
			 ((*calib_task>6)&&(*calib_task<11))||(*calib_task>16)) {
	status = DXP_BAD_PARAM;
	sprintf(info_string,"Calibration task = %d is nonexistant",
			*calib_task);
	dxp_log_error("dxp_begin_calibrate", info_string, status);
	return status;
  }
	
	
  if((status=dxp_modify_dspsymbol(ioChan, modChan,
								  "WHICHTEST", &whichtest, board->dsp[*modChan]))!=DXP_SUCCESS) {    
	dxp_log_error("dxp_begin_calibrate","Error writing WHICHTEST",status);
	return status;
  }

  if((status=dxp_modify_dspsymbol(ioChan, modChan,
								  "RUNTASKS",&runtasks, board->dsp[*modChan]))!=DXP_SUCCESS) {    
	dxp_log_error("dxp_begin_calibrate","Error writing RUNTASKS",status);
	return status;
  }

  /* Finally startup the Calibration run. */

  if((status = dxp_begin_run(ioChan, modChan, &ignore, &resume, board))!=DXP_SUCCESS){
	dxp_log_error("dxp_begin_calibrate"," ",status);
  }
    
  return status;
}

/******************************************************************************
 * Routine to decode the error message from the DSP after a run if performed
 * 
 * Returns the RUNERROR and ERRINFO words from the DSP parameter block
 *
 ******************************************************************************/
static int dxp_decode_error(unsigned short array[], Dsp_Info* dsp, 
							unsigned short* runerror, unsigned short* errinfo)
	 /* unsigned short array[];		Input: array from parameter block read	*/
	 /* Dsp_Info *dsp;				Input: Relavent DSP info					*/
	 /* unsigned short *runerror;	Output: runerror word					*/
	 /* unsigned short *errinfo;		Output: errinfo word						*/
{

  int status;
  char info_string[INFO_LEN];
  unsigned short addr_RUNERROR,addr_ERRINFO;

  /* Be paranoid and check if the DSP configuration is downloaded.  If not, do it */
	
  if ((dsp->proglen)<=0) {
	status = DXP_DSPLOAD;
	sprintf(info_string, "Must Load DSP code before decoding errors");
	dxp_log_error("dxp_decode_error", info_string, status);
	return status;
  }

  /* Now pluck out the location of a couple of symbols */

  status  = dxp_loc("RUNERROR", dsp, &addr_RUNERROR);
  status += dxp_loc("ERRINFO", dsp, &addr_ERRINFO);
  if(status!=DXP_SUCCESS){
	status=DXP_NOSYMBOL;
	dxp_log_error("dxp_decode_error",
				  "Could not locate either RUNERROR or ERRINFO symbols",status);
	return status;
  }

  *runerror = array[addr_RUNERROR];
  *errinfo = (unsigned short) (*runerror!=0 ? array[addr_ERRINFO] : 0);
  return DXP_SUCCESS;

}

/******************************************************************************
 * Routine to clear an error in the DSP
 * 
 * Clears non-fatal DSP error in one or all channels of a single DXP module.
 * If modChan is -1 then all channels are cleared on the DXP.
 *
 ******************************************************************************/
static int dxp_clear_error(int* ioChan, int* modChan, Dsp_Info* dsp)
	 /* int *ioChan;					Input: I/O channel of DXP module		*/
	 /* int *modChan;				Input: DXP channels no (-1,0,1,2,3)	*/
	 /* Dsp_Info *dsp;				Input: Relavent DSP info				*/
{

  int status;
  char info_string[INFO_LEN];
  unsigned short zero;

  /* Be paranoid and check if the DSP configuration is downloaded.  If not, do it */
	
  if ((dsp->proglen)<=0) {
	status = DXP_DSPLOAD;
	sprintf(info_string, "Must Load DSP code before clearing errors");
	dxp_log_error("dxp_clear_error", info_string, status);
	return status;
  }

  zero=0;
  status = dxp_modify_dspsymbol(ioChan, modChan, "RUNERROR", 
								&zero, dsp);
  if (status!=DXP_SUCCESS) 
	dxp_log_error("dxp_clear_error","Unable to clear RUNERROR",status);

  return status;

}

/******************************************************************************
 * 
 * Routine that checks the status of a completed calibration run.  This 
 * typically depends on symbols in the DSP that were changed by the 
 * calibration task.
 *
 ******************************************************************************/
static int dxp_check_calibration(int* calibtest, unsigned short* params, Dsp_Info* dsp)
	 /* int *calibtest;					Input: Calibration test performed	*/
	 /* unsigned short *params;			Input: parameters read from the DSP	*/
	 /* Dsp_Info *dsp;					Input: Relavent DSP info				*/
{

  int status = DXP_SUCCESS;
  int *itemp;
  unsigned short *stemp;
  Dsp_Info *dtemp;

  /* Assing unused input parameters to stop compiler warnings */
  itemp = calibtest;
  stemp = params;
  dtemp = dsp;

  /* No checking is currently performed. */

  return status;
}

/******************************************************************************
 * Routine to get run statistics from the DXP.
 * 
 * Returns some run statistics from the parameter block array[]
 *
 ******************************************************************************/
static int dxp_get_runstats(unsigned short array[], Dsp_Info* dsp, 
							unsigned int* evts, unsigned int* under, 
							unsigned int* over, unsigned int* fast, 
							unsigned int* base, double* live,
							double* icr, double* ocr)
	 /* unsigned short array[];			Input: array from parameter block   */
	 /* Dsp_Info *dsp;					Input: Relavent DSP info					*/
	 /* unsigned int *evts;				Output: number of events in spectrum*/
	 /* unsigned int *under;			Output: number of underflows        */
	 /* unsigned int *over;				Output: number of overflows         */
	 /* unsigned int *fast;				Output: number of fast filter events*/
	 /* unsigned int *base;				Output: number of baseline events   */
	 /* double *live;					Output: livetime in seconds         */
	 /* double *icr;					Output: input count rate in kHz     */
	 /* double *ocr;					Output: output count rate in kHz    */
{

    char info_string[INFO_LEN];

  unsigned short addr[3]={USHRT_MAX, USHRT_MAX, USHRT_MAX};
  int status=DXP_SUCCESS;

  /* Temporary values from the DSP code */
  double real = 0.;
  unsigned long nEvents = 0;
  double liveclock_tick = 0;

  /* Use unsigned long temp variables since we will be bit shifting 
   * by more than an unsigned short or int in the routine */
  unsigned long temp0, temp1, temp2;

  /* Now retrieve the location of DSP parameters and fill variables */

  /* Retrieve the clock speed of this board */
  status = dxp_loc("SYSMICROSEC", dsp, &addr[0]);
  /* If we succeed in retrieving the parameter, then use it, else
   * default to 40 MHz 
   */
  if (status == DXP_SUCCESS) 
    {
      liveclock_tick = 16.e-6 / ((double) array[addr[0]]);
    } else {
      liveclock_tick = 16.e-6 / 40.;
    }

  /* Events in the run */
  status = dxp_loc("EVTSINRUN0", dsp, &addr[0]);
  status += dxp_loc("EVTSINRUN1", dsp, &addr[1]);
  temp0 = (unsigned long) array[addr[1]];
  temp1 = (unsigned long) array[addr[0]];
  *evts = (unsigned int) (temp0 + temp1*65536.);

  /* Underflows in the run */
  status += dxp_loc("UNDRFLOWS0", dsp, &addr[0]);
  status += dxp_loc("UNDRFLOWS1", dsp, &addr[1]);
  temp0 = (unsigned long) array[addr[1]];
  temp1 = (unsigned long) array[addr[0]];
  *under = (unsigned int) (temp0 + temp1*65536.);

  /* Overflows in the run */
  status += dxp_loc("OVERFLOWS0", dsp, &addr[0]);
  status += dxp_loc("OVERFLOWS1", dsp, &addr[1]);
  temp0 = (unsigned long) array[addr[1]];
  temp1 = (unsigned long) array[addr[0]];
  *over = (unsigned int) (temp0 + temp1*65536.);

  /* Fast Peaks in the run */
  status += dxp_loc("FASTPEAKS0", dsp, &addr[0]);
  status += dxp_loc("FASTPEAKS1", dsp, &addr[1]);
  temp0 = (unsigned long) array[addr[1]];
  temp1 = (unsigned long) array[addr[0]];
  *fast = (unsigned int) (temp0 + temp1*65536.);

  /* Baseline Events in the run */
  status += dxp_loc("BASEEVTS0", dsp, &addr[0]);
  status += dxp_loc("BASEEVTS1", dsp, &addr[1]);
  temp0 = (unsigned long) array[addr[1]];
  temp1 = (unsigned long) array[addr[0]];
  *base = (unsigned int) (temp0 + temp1*65536.);

  /* Livetime for the run */
  status += dxp_loc("LIVETIME0", dsp, &addr[0]);
  status += dxp_loc("LIVETIME1", dsp, &addr[1]);
  status += dxp_loc("LIVETIME2", dsp, &addr[2]);
  temp0 = (unsigned long) array[addr[1]];
  temp1 = (unsigned long) array[addr[0]];
  temp2 = (unsigned long) array[addr[2]];
  *live = ((double) (temp0 + temp1*65536. + temp2*65536.*65536.)) * liveclock_tick;
  

  /* XXX Realtime tick vs. livetime tick? */

  /* Realtime for the run */
  status += dxp_loc("REALTIME0", dsp, &addr[0]);
  status += dxp_loc("REALTIME1", dsp, &addr[1]);
  status += dxp_loc("REALTIME2", dsp, &addr[2]);
  temp0 = (unsigned long) array[addr[1]];
  temp1 = (unsigned long) array[addr[0]];
  temp2 = (unsigned long) array[addr[2]];
  real = ((double) (temp0 + temp1*pow(2,16) + temp2*pow(2,32))) * liveclock_tick;

  sprintf(info_string, "evts = %u, under = %u, over = %u, fast = %u, live = %f, real = %f",
	  *evts, *under, *over, *fast, *live, real);
  dxp_log_debug("dxp_get_runstats", info_string);
  
  /* Calculate the number of events in the run */
  
  nEvents = *evts + *under + *over;
  
  /* Calculate the event rates */
  if(*live > 0.)
	{
	  *icr = (*fast) / (*live);
	} else {
	  *icr = -999.;
	}
  if(real > 0.)
	{
	  *ocr = nEvents / real;
	} else {
	  *ocr = -999.;
	}
  
  if(status!=DXP_SUCCESS){
	status = DXP_NOSYMBOL;
	dxp_log_error("dxp_get_runstats",
				  "Unable to find 1 or more parameters",status);
	return status;
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 * 
 * This routine calculates the new value GAINDAC DAC 
 * for a gainchange of (desired gain)/(present gain)
 *
 * assumptions:  The GAINDAC has a linear response in dB
 *
 ******************************************************************************/
static int dxp_perform_gaincalc(float* gainchange, unsigned short* old_gaindac,
								short* delta_gaindac)
	 /* float *gainchange;					Input: desired gain change           */
	 /* unsigned short *old_gaindac;			Input: current gaindac				*/
	 /* short *delta_gaindac;				Input: required gaindac change       */
{

  double gain_db, gain;

  /* Convert the current GAINDAC setting back to dB from bits */

  gain_db = (((double) (*old_gaindac)) / dacpergaindb) - 10.0;

  /* Convert back to pure gain */

  gain = pow(10., gain_db/20.);

  /* Scale by gainchange and convert back to bits */

  gain_db = 20. * log10(gain * (*gainchange));
  *delta_gaindac = (short) ((*old_gaindac) - (unsigned short) ((gain_db + 10.0)*dacpergaindb));

  /* we are done.  leave error checking to the calling routine */

  return DXP_SUCCESS;
}
			
/******************************************************************************
 *
 * This routine changes all DXP channel gains:
 *
 *            initial         final
 *            gain_i -> gain_i*gainchange
 *  
 * THRESHOLD is also modified to keep the energy threshold fixed.
 * Note: DACPERADC must be modified -- this can be done via calibration
 *       run or by scaling DACPERADC --> DACPERADC/gainchange.  This routine
 *       does not do either!
 *  
 ******************************************************************************/
static int dxp_change_gains(int* ioChan, int* modChan, int* module, 
							float* gainchange, Dsp_Info* dsp)
	 /* int *ioChan;					Input: IO channel of desired channel			*/
	 /* int *modChan;				Input: channel on module						*/
	 /* int *module;					Input: module number: for error reporting	*/
	 /* float *gainchange;			Input: desired gain change					*/
	 /* Dsp_Info *dsp;				Input: Relavent DSP info					*/
{
  int status;
  char info_string[INFO_LEN];
  unsigned short gaindac, fast_threshold, slow_threshold;
  short delta_gaindac;
  unsigned long lgaindac;		/* Use a long to test bounds of new GAINDAC */
  unsigned short ltemp;

  /* Read out the GAINDAC setting from the DSP. */
            
  if((status=dxp_read_one_dspsymbol(ioChan, modChan,
								"GAINDAC", dsp, &ltemp))!=DXP_SUCCESS){
	sprintf(info_string,
			"Error reading GAINDAC from mod %d chan %d",*module,*modChan);
	dxp_log_error("dxp_change_gains",info_string,status);
	return status;
  }
  gaindac = (unsigned short) ltemp;

  /* Read out the fast peak THRESHOLD setting from the DSP. */
            
  if((status=dxp_read_one_dspsymbol(ioChan, modChan,
								"THRESHOLD", dsp, &ltemp))!=DXP_SUCCESS){
	sprintf(info_string,
			"Error reading THRESHOLD from mod %d chan %d",*module,*modChan);
	dxp_log_error("dxp_change_gains",info_string,status);
	return status;
  }
  fast_threshold = (unsigned short) ltemp;
	
  /* Read out the slow peak THRESHOLD setting from the DSP. */
            
  if((status=dxp_read_one_dspsymbol(ioChan, modChan,
								"SLOWTHRESH", dsp, &ltemp))!=DXP_SUCCESS){
	sprintf(info_string,
			"Error reading SLOWTHRESH from mod %d chan %d",*module,*modChan);
	dxp_log_error("dxp_change_gains",info_string,status);
	return status;
  }
  slow_threshold = (unsigned short) ltemp;

  /* Calculate the delta GAINDAC in bits. */
            
  if((status=dxp_perform_gaincalc(gainchange,
								  &gaindac, &delta_gaindac))!=DXP_SUCCESS){
	sprintf(info_string,
			"DXP module %d Channel %d", *module, *modChan);
	dxp_log_error("dxp_change_gains",info_string,status);
	status=DXP_SUCCESS;
  }

  /* Now do some bounds checking.  The variable gain amplifier is only capable of
   * -6 to 30 dB. Do this by subracting instead of adding to avoid bounds of 
   * short integers */

  lgaindac = ((unsigned long) gaindac) + delta_gaindac;
  if ((lgaindac<=((GAINDAC_MIN+10.)*dacpergaindb)) ||
	  (lgaindac>=((GAINDAC_MAX+10.)*dacpergaindb))) {
	sprintf(info_string,
			"Required GAINDAC setting of %x (bits) that is out of range", 
			((unsigned int) lgaindac));
	status=DXP_DETECTOR_GAIN;
	dxp_log_error("dxp_change_gains",info_string,status);
	return status;
  }

  /* All looks ok, set the new gaindac */
	
  gaindac = (unsigned short) (gaindac + delta_gaindac);

  /* Change the thresholds as well, to accomadate the new gains.  If the 
   * slow or fast thresholds are identically 0, then do not change them. */

  if (fast_threshold!=0) 
	fast_threshold = (unsigned short) ((*gainchange)*((double) fast_threshold)+0.5);
  if (slow_threshold!=0)
	slow_threshold = (unsigned short) ((*gainchange)*((double) slow_threshold)+0.5);

  /* Download the GAINDAC, THRESHOLD and SLOWTHRESH back to the DSP. */
			
  if((status=dxp_modify_dspsymbol(ioChan, modChan,
								  "GAINDAC", &gaindac, dsp))!=DXP_SUCCESS){
	sprintf(info_string,
			"Error writing GAINDAC to mod %d chan %d", *module, *modChan);
	dxp_log_error("dxp_change_gains",info_string,status);
	return status;
  }
  if((status=dxp_modify_dspsymbol(ioChan, modChan,
								  "THRESHOLD", &fast_threshold, dsp))!=DXP_SUCCESS){
	sprintf(info_string,
			"Error writing THRESHOLD to mod %d chan %d", *module, *modChan);
	dxp_log_error("dxp_change_gains",info_string,status);
	return status;
  }
  if((status=dxp_modify_dspsymbol(ioChan, modChan,
								  "SLOWTHRESH", &slow_threshold, dsp))!=DXP_SUCCESS){
	sprintf(info_string,
			"Error writing SLOWTHRESH to mod %d chan %d", *module, *modChan);
	dxp_log_error("dxp_change_gains",info_string,status);
	return status;
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Adjust the DXP channel's gain parameters to achieve the desired ADC rule.  
 * The ADC rule is the fraction of the ADC full scale that a single x-ray step 
 * of energy *energy contributes.  
 *
 * set following, detector specific parameters for all DXP channels:
 *       COARSEGAIN
 *       FINEGAIN
 *       VRYFINGAIN (default:128)
 *       POLARITY
 *       OFFDACVAL
 *
 ******************************************************************************/
static int dxp_setup_asc(int* ioChan, int* modChan, int* module, float* adcRule, 
						 float* gainmod, unsigned short* polarity, float* vmin, 
						 float* vmax, float* vstep, Dsp_Info* dsp)
	 /* int *ioChan;					Input: IO channel number					*/
	 /* int *modChan;				Input: Module channel number				*/
	 /* int *module;					Input: Module number: error reporting	*/
	 /* float *adcRule;				Input: desired ADC rule					*/
	 /* float *gainmod;				Input: desired finescale gain adjustment */
	 /* unsigned short *polarity;	Input: polarity of channel				*/
	 /* float *vmin;					Input: minimum voltage range of channel	*/
	 /* float *vmax;					Input: maximum voltage range of channel	*/
	 /* float *vstep;				Input: average step size of single pulse	*/
	 /* Dsp_Info *dsp;				Input: Relavent DSP info					*/
{
  double gain;
  char info_string[INFO_LEN];
  int status;
  unsigned short gaindac;
  double g_input = GINPUT;					/* Input attenuator Gain	*/
  double g_input_buff = GINPUT_BUFF;			/* Input buffer Gain		*/
  double g_inverting_amp = GINVERTING_AMP;	/* Inverting Amp Gain		*/
  double g_v_divider = GV_DIVIDER;			/* V Divider after Inv AMP	*/
  double g_gaindac_buff = GGAINDAC_BUFF;		/* GainDAC buffer Gain		*/
  double g_nyquist = GNYQUIST;				/* Nyquist Filter Gain		*/
  double g_adc_buff = GADC_BUFF;				/* ADC buffer Gain			*/
  double g_adc = GADC;						/* ADC Input Gain			*/
  double g_system = 0.;						/* Total gain imposed by ASC
											   not including the GAINDAC*/
  double g_desired = 0.;						/* Desired gain for the 
												   GAINDAC */

  float *ftemp;

  /* Assing unused input parameters to stop compiler warnings */
  ftemp = vmax;
  ftemp = vmin;
  ftemp = gainmod;
	

  /* First perform some initialization.  This is redundant for now, but eventually
   * these parameters will be determined by calibration runs on the board */

  /* The number of DAC bits per unit gain in dB */
  dacpergaindb = ((1<<GAINDAC_BITS)-1) / 40.;
  /* The number of DAC bits per unit gain (linear) : 40(dB)=20log10(g) => g=100.*/
  dacpergain   = ((1<<GAINDAC_BITS)-1) / 100.;
												  
  /*
   *   Following is required gain to achieve desired eV/ADC.  Note: ADC full
   *      scale = ADC_RANGE mV
   */
  gain = (*adcRule*((float) ADC_RANGE)) / *vstep;
  /*
   * Define the total system gain.  This is the gain without including the 
   * GAINDAC user controlled gain.  Any desired gain should be adjusted by
   * this system gain before determining the proper GAINDAC setting.
   */
  g_system = g_input * g_input_buff * g_inverting_amp * g_v_divider * 
	g_gaindac_buff * g_nyquist * g_adc_buff * g_adc;

  /* Now the desired GAINDAC gain is just the desired user gain 
   * divided by g_system */

  g_desired = gain / g_system;

  /* Now convert this gain to dB since the GAINDAC variable amplifier is linear
   * in dB. */

  g_desired = 20.0 * log10(g_desired);

  /* Now do some bounds checking.  The variable gain amplifier is only capable of
   * -6 to 30 dB. */

  if ((g_desired<=GAINDAC_MIN)||(g_desired>=GAINDAC_MAX)) {
	sprintf(info_string,
			"Required gain of %f (dB) that is out of range for the ASC", g_desired);
	status=DXP_DETECTOR_GAIN;
	dxp_log_error("dxp_setup_asc",info_string,status);
	return status;
  }

  /* Gain is within limits.  Determine the setting for the 16-bit DAC */
	
  gaindac = (short) ((g_desired + 10.0) * dacpergaindb);

  /* Write the GAINDAC data back to the DXP module. */
		
  if((status=dxp_modify_dspsymbol(ioChan, modChan,
								  "GAINDAC", &gaindac, dsp))!=DXP_SUCCESS){
	sprintf(info_string,
			"Error writting GAINDAC to module %d channel %d",
			*module,*modChan);
	dxp_log_error("dxp_setup_asc",info_string,status);
	return status;
  }
  /*
   *    Download POLARITY
   */
  if((status=dxp_modify_dspsymbol(ioChan, modChan,
								  "POLARITY", polarity, dsp))!=DXP_SUCCESS){
	sprintf(info_string,
			"Error writting POLARITY to module %d channel %d",
			*module,*modChan);
	dxp_log_error("dxp_setup_asc",info_string,status);
	return status;
  }

  return status;
}

/******************************************************************************
 *
 * Perform the neccessary calibration runs to get the ASC ready to take data
 *
 ******************************************************************************/
static int dxp_calibrate_asc(int* mod, int* camChan, unsigned short* used, 
							 Board *board) 
	 /* int *mod;					Input: Camac Module number to calibrate			*/
	 /* int *camChan;				Input: Camac pointer							*/
	 /* unsigned short *used;		Input: bitmask of channel numbers to calibrate	*/
	 /* Board *board;				Input: Relavent Board info						*/
{
  Board *btemp;
  int *itemp;
  unsigned short *stemp;

  /* Assing unused input parameters to stop compiler warnings */
  btemp = board;
  itemp = mod;
  itemp = camChan;
  stemp = used;

  /* No Calibration runs are performed for the DXP-4C 2X */

  return DXP_SUCCESS;
}

/******************************************************************************
 * 
 * Preform internal gain calibration or internal TRACKDAC reset point 
 * for all DXP channels:
 *    o save the current value of RUNTASKS for each channel
 *    o start run
 *    o wait
 *    o stop run
 *    o readout parameter memory
 *    o check for errors, clear errors if set
 *    o check calibration results..
 *    o restore RUNTASKS for each channel
 *
 ******************************************************************************/
static int dxp_calibrate_channel(int* mod, int* camChan, unsigned short* used, 
								 int* calibtask, Board *board)
	 /* int *mod;						Input: Camac Module number to calibrate			*/
	 /* int *camChan;					Input: Camac pointer							*/
	 /* unsigned short *used;			Input: bitmask of channel numbers to calibrate	*/
	 /* int *calibtask;					Input: which calibration function				*/
	 /* Board *board;					Input: Relavent Board info						*/
{

  int status=DXP_SUCCESS,status2,chan;
  char info_string[INFO_LEN];
  unsigned short runtasks;
  float one_second=1.0;
  unsigned short addr_RUNTASKS=USHRT_MAX;
  unsigned short runerror,errinfo;
	
  unsigned short params[MAXSYM];

  unsigned short ltemp;
  /*
   *  Loop over each channel and save the RUNTASKS word.  Then start the
   *  calibration run
   */
  for(chan=0;chan<4;chan++){
	if(((*used)&(1<<chan))==0) continue;

	/* Grab the addresses in the DSP for some symbols. */
    
	status = dxp_loc("RUNTASKS", board->dsp[chan], &addr_RUNTASKS);
	if(status!=DXP_SUCCESS){
	  status = DXP_NOSYMBOL;
	  dxp_log_error("dxp_calibrate_channel","Unable to locate RUNTASKS",status);
	  return status;
	}

	if((status2=dxp_read_one_dspsymbol(camChan, &chan, "RUNTASKS", 
								   board->dsp[chan], &ltemp))!=DXP_SUCCESS){
	  sprintf(info_string,"Error reading RUNTASKS from mod %d chan %d",
			  *mod,chan);
	  dxp_log_error("dxp_calibrate_channel",info_string,status2);
	  return status2;
	}
	runtasks = (unsigned short) ltemp;

	if((status2=dxp_begin_calibrate(camChan, &chan, calibtask, board))!=DXP_SUCCESS){
	  sprintf(info_string,"Error beginning calibration for mod %d",*mod);
	  dxp_log_error("dxp_calibrate_channel",info_string,status2);
	  return status2;
	}
	/*
	 *   wait a second, then stop the runs
	 */
	dxp4c2x_md_wait(&one_second);
	if((status2=dxp_end_run(camChan, &chan))!=DXP_SUCCESS){
	  dxp_log_error("dxp_calibrate_channel",
					"Unable to end calibration run",status2);
	  return status2;
	}
	/*
	 *  Loop over each of the channels and read out the parameter memory. Check
	 *  to see if there were errors.  Finally, restore the original value of 
	 *  RUNTASKS
	 */

	/* Read out the parameter memory into the Params array */
            
	if((status2=dxp_read_dspparams(camChan, &chan, board->dsp[chan], params))!=DXP_SUCCESS){
	  sprintf(info_string,
			  "error reading parameters for mod %d chan %d",*mod,chan);
	  dxp_log_error("dxp_calibrate_channel",info_string,status2);
	  return status2;
	}

	/* Check for errors reported by the DSP. */

	if((status2=dxp_decode_error(params, board->dsp[chan], &runerror, &errinfo))!=DXP_SUCCESS){
	  dxp_log_error("dxp_calibrate_channel","Unable to decode errors",status2);
	  return status2;
	}
	if(runerror!=0){
	  status+=DXP_DSPRUNERROR;
	  sprintf(info_string,"DSP error detected for mod %d chan %d",
			  *mod,chan);
	  dxp_log_error("dxp_calibrate_channel",info_string,status);
	  if((status2=dxp_clear_error(camChan, &chan, board->dsp[chan]))!=DXP_SUCCESS){
		sprintf(info_string,
				"Unable to clear error for mod %d chan %d",*mod,chan);
		dxp_log_error("dxp_calibrate_channel",info_string,status2);
		return status2;
	  }
	}

	/* Call the primitive routine that checks the calibration to ensure
	 * that all went well.  The results depend on the calibration performed. */

	if((status += dxp_check_calibration(calibtask, params, board->dsp[chan]))!=DXP_SUCCESS){
	  sprintf(info_string,"Calibration Error: mod %d chan %d",
			  *mod,chan);
	  dxp_log_error("dxp_calibrate_channel",info_string,status);
	}


	/* Now write back the value previously stored in RUNTASKS */            
			
	if((status2=dxp_modify_dspsymbol(camChan, &chan, "RUNTASKS", 
									 &runtasks, board->dsp[chan]))!=DXP_SUCCESS){
	  sprintf(info_string,"Error writing RUNTASKS to mod %d chan %d",
			  *mod,chan);
	  dxp_log_error("dxp_calibrate_channel",info_string,status2);
	  return status2;
	}
  }

  return status;
}

/********------******------*****------******-------******-------******------*****
 * Now begins the section with utility routines to handle some of the drudgery
 * associated with different operating systems and such.
 *
 ********------******------*****------******-------******-------******------*****/

/******************************************************************************
 *
 * Routine to open a new file.  
 * Try to open the file directly first.
 * Then try to open the file in the directory pointed to 
 *     by XIAHOME.
 * Finally try to open the file as an environment variable.
 *
 ******************************************************************************/
static FILE *dxp_find_file(const char* filename, const char* mode)
	 /* const char *filename;			Input: filename to open			*/
	 /* const char *mode;				Input: Mode to use when opening	*/
{
  FILE *fp=NULL;
  char *name=NULL, *name2=NULL;
  char *home=NULL;

  /* Try to open file directly */
  if((fp=fopen(filename,mode))!=NULL){
	return fp;
  }
  /* Try to open the file with the path XIAHOME */
  if ((home=getenv("XIAHOME"))!=NULL) {
	name = (char *) dxp4c2x_md_alloc(sizeof(char)*
									 (strlen(home)+strlen(filename)+2));
	sprintf(name, "%s/%s", home, filename);
	if((fp=fopen(name,mode))!=NULL){
	  dxp4c2x_md_free(name);
	  return fp;
	}
	dxp4c2x_md_free(name);
	name = NULL;
  }
  /* Try to open the file with the path DXPHOME */
  if ((home=getenv("DXPHOME"))!=NULL) {
	name = (char *) dxp4c2x_md_alloc(sizeof(char)*
									 (strlen(home)+strlen(filename)+2));
	sprintf(name, "%s/%s", home, filename);
	if((fp=fopen(name,mode))!=NULL){
	  dxp4c2x_md_free(name);
	  return fp;
	}
	dxp4c2x_md_free(name);
	name = NULL;
  }
  /* Try to open the file as an environment variable */
  if ((name=getenv(filename))!=NULL) {
	if((fp=fopen(name,mode))!=NULL){
	  return fp;
	}
	name = NULL;
  }
  /* Try to open the file with the path XIAHOME and pointing 
   * to a file as an environment variable */
  if ((home=getenv("XIAHOME"))!=NULL) {
	if ((name2=getenv(filename))!=NULL) {
		
	  name = (char *) dxp4c2x_md_alloc(sizeof(char)*
									   (strlen(home)+strlen(name2)+2));
	  sprintf(name, "%s/%s", home, name2);
	  if((fp=fopen(name,mode))!=NULL){
		dxp4c2x_md_free(name);
		return fp;
	  }
	  dxp4c2x_md_free(name);
	  name = NULL;
	}
  }
  /* Try to open the file with the path DXPHOME and pointing 
   * to a file as an environment variable */
  if ((home=getenv("DXPHOME"))!=NULL) {
	if ((name2=getenv(filename))!=NULL) {
		
	  name = (char *) dxp4c2x_md_alloc(sizeof(char)*
									   (strlen(home)+strlen(name2)+2));
	  sprintf(name, "%s/%s", home, name2);
	  if((fp=fopen(name,mode))!=NULL) {
		dxp4c2x_md_free(name);
		name = NULL;
		return fp;
	  }
	  dxp4c2x_md_free(name);
	  name = NULL;
	}
  }

  return NULL;
}


/**********
 * This routine does nothing for this product.
 **********/
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


/**********
 * This routine reads out a specific memory type
 * or returns an error if that type isn't supported.
 **********/
static int dxp_read_mem(int *ioChan, int *modChan, Board *board,
						char *name, unsigned long *base, unsigned long *offset,
						unsigned long *data)
{
  int status;
  int i;

  char info_string[INFO_LEN];


  ASSERT(ioChan != NULL);
  ASSERT(modChan != NULL);
  ASSERT(board != NULL);
  ASSERT(name != NULL);
  ASSERT(base != NULL);
  ASSERT(offset != NULL);
  ASSERT(data != NULL);


  for (i = 0; i < NUM_MEM_READERS; i++) {
	if (STREQ(mem_readers[i].name, name)) {
	  status = mem_readers[i].f(ioChan, modChan, board, *base, *offset, data);

	  if (status != DXP_SUCCESS) {
		sprintf(info_string, "Error reading '%s' memory: base = %lu, "
				"offset = %lu, ioChan = %d, modChan = %d",
				name, *base, *offset, *ioChan, *modChan);
		dxp_log_error("dxp_read_mem", info_string, status);
		return status;
	  }

	  return DXP_SUCCESS;
	}
  }

  /* Add a proper routine for this. See dxp_read_external_memory() as an
   * example.
   */
/*   if (STREQ(name, "internal_multisca")) { */

/* 	status = dxp_internal_multisca(ioChan, modChan, board, data); */

/*   } else { */

/* 	status = DXP_NOSUPPORT; */
/*   } */

  sprintf(info_string, "Unknown memory type '%s' for ioChan = %d, modChan = %d",
		  name, *ioChan, *modChan);
  dxp_log_error("dxp_read_mem", info_string, DXP_NOSUPPORT);
  return DXP_NOSUPPORT;
}


XERXES_STATIC int dxp_write_mem(int *ioChan, int *modChan, Board *board,
								char *name, unsigned long *base, unsigned long *offset,
								unsigned long *data)
{
  UNUSED(ioChan);
  UNUSED(modChan);
  UNUSED(board);
  UNUSED(name);
  UNUSED(base);
  UNUSED(offset);
  UNUSED(data);

  return DXP_UNIMPLEMENTED;
}


/**********
 * This routine reads out the data associated
 * with the internal multisca firmware type.
 * It attempts to verify that the currently 
 * running DSP code supports this type of firmware.
 **********/
static int dxp_internal_multisca(int *ioChan, int *modChan, Board *board,
								 unsigned long *data)
{
  int status;

  unsigned long i;
  
  parameter_t CODEVAR    = 0x0000;
  parameter_t NUMPOINTS  = 0x0000;
  parameter_t NUMSCA     = 0x0000;
  parameter_t SPECTSTART = 0x0000;

  unsigned int len;

  unsigned short *shortData = NULL;

  char info_string[INFO_LEN];


  /* Check CODEVAR first */
  status = dxp_read_one_dspsymbol(ioChan, modChan, "CODEVAR",
							  board->dsp[*modChan], &CODEVAR);

  if (status != DXP_SUCCESS) {

	sprintf(info_string, "Error reading CODEVAR for ioChan = %d, "
			"modChan = %d", *ioChan, *modChan);
	dxp_log_error("dxp_internal_multisca", info_string, status);
	return status;
  }
  
  if (CODEVAR != INTERNAL_MULTISCA) {

	status = DXP_WRONG_FIRMWARE;
	dxp_log_error("dxp_internal_multisca", "Current DSP code does not support "
				  "Internal MultiSCA Mapping mode", status);
	return status;
  }
  

  /* Check for an active run first? */
  
  
  status = dxp_read_one_dspsymbol(ioChan, modChan, "NUMPOINTS",
							  board->dsp[*modChan], &NUMPOINTS);

  if (status != DXP_SUCCESS) {

	sprintf(info_string, "Error reading NUMPOINTS for ioChan = %d, "
			"modChan = %d", *ioChan, *modChan);
	dxp_log_error("dxp_internal_multisca", info_string, status);
	return status;
  }

  status = dxp_read_one_dspsymbol(ioChan, modChan, "NUMSCA",
							  board->dsp[*modChan], &NUMSCA);

  if (status != DXP_SUCCESS) {

	sprintf(info_string, "Error reading NUMSCA for ioChan = %d, "
			"modChan = %d", *ioChan, *modChan);
	dxp_log_error("dxp_internal_multisca", info_string, status);
	return status;
  }

  status = dxp_read_one_dspsymbol(ioChan, modChan, "SPECTSTART", 
							  board->dsp[*modChan], &SPECTSTART);

  if (status != DXP_SUCCESS) {

	sprintf(info_string, "Error reading SPECTSTART for ioChan = %d, "
			"modChan = %d", *ioChan, *modChan);
	dxp_log_error("dxp_internal_multisca", info_string, status);
	return status;
  }

  /* Size in terms of 24-bit words
   * (i.e., *not* unsigned short)
   */
  len = NUMPOINTS * (NUMSCA + 4);
  
  /* Now in terms of shorts */
  len *= 2;

  sprintf(info_string, "NUMPOINTS = %u", NUMPOINTS);
  dxp_log_debug("dxp_internal_multisca", info_string);
  sprintf(info_string, "NUMSCA = %u", NUMSCA);
  dxp_log_debug("dxp_internal_multisca", info_string);
  sprintf(info_string, "len = %u", len);
  dxp_log_debug("dxp_internal_multisca", info_string);
  sprintf(info_string, "SPECTSTART = %u", SPECTSTART);
  dxp_log_debug("dxp_internal_multisca", info_string);

  /* Now, we are actually only reading
   * out unsigned shorts so we need
   * to scale things by 2 so that we
   * pass in an array of the correct size.
   */
  shortData = (unsigned short *)dxp4c2x_md_alloc(len * sizeof(unsigned short));

  if (shortData == NULL) {

	status = DXP_NOMEM;
	dxp_log_error("dxp_internal_multisca", "Out-of-memory creating shortData", status);
	return status;
  }
  
  status = dxp_read_block(ioChan, modChan, &SPECTSTART, &((unsigned int)len), shortData);

  if (status != DXP_SUCCESS) {

	dxp4c2x_md_free((void *)shortData);

	sprintf(info_string, "Error reading data block at addr = %#x, len = %u, "
			"ioChan = %d, modChan = %d", SPECTSTART, len, *ioChan, *modChan);
	dxp_log_error("dxp_internal_multisca", info_string, status);
	return status;
  }

  /* Convert the array of 16-bit words
   * into an array of 32-bit words that
   * only use 24 of their bits. (The
   * DSP Program memory is 24-bits
   * wide.)
   */
  for (i = 0; i < (len / 2); i++) {

	data[i] = (unsigned long)((unsigned long)shortData[i * 2] + ((unsigned long)(shortData[(i * 2) + 1] & 0xFF) << 16));
  }
  
  dxp4c2x_md_free((void *)shortData);

  return DXP_SUCCESS;
}



/**********
 * This routine writes to the specified
 * register, provided that it is defined
 * for this product.
 **********/
static int dxp_write_reg(int *ioChan, int *modChan, char *name, unsigned short *data)
{
  int status;

  unsigned int f   = 0;
  unsigned int a   = 0;
  unsigned int len = 0;

  char info_string[INFO_LEN];

  UNUSED(modChan);


  if (STREQ("tcr", name)) {

	f = DXP_TCR_F_WRITE;
	a = DXP_TCR_A_WRITE;
	
	len = 1;

  } else {

	status = DXP_NOMATCH;
	sprintf(info_string, "Register %s does not match known types", name);
	dxp_log_error("dxp_write_reg", info_string, status);
	return status;
  }

  status = dxp4c2x_md_io(ioChan, &f, &a, data, &len);

  if (status != DXP_SUCCESS) {

	sprintf(info_string, "Error reading %s register for ioChan = %d",
			name, *ioChan);
	dxp_log_error("dxp_write_reg", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/**********
 * This routine reads from the specified
 * register, provided that it is defined for
 * this product.
 **********/
XERXES_STATIC int XERXES_API dxp_read_reg(int *ioChan, int *modChan,
					  char *name, unsigned short *data)
{
    int status;

    unsigned int f   = 0;
    unsigned int a   = 0;
    unsigned int len = 0;

    char info_string[INFO_LEN];

    UNUSED(modChan);


    if (STREQ("tcr", name)) {
	
	f = DXP_TCR_F_READ;
	a = DXP_TCR_A_READ;

	len = 1;

    } else {

	status = DXP_NOMATCH;
	sprintf(info_string, "Register %s does not match known types", name);
	dxp_log_error("dxp_read_reg", info_string, status);
	return status;
    }

    status = dxp4c2x_md_io(ioChan, &f, &a, data, &len);

    if (status != DXP_SUCCESS) {
	sprintf(info_string, 
		"Error reading %s register for ioChan = %d",
		name,
		*ioChan);
	dxp_log_error("dxp_read_reg", info_string, status);
	return status;
    }
 
    return DXP_SUCCESS;
}


/**********
 * This routine does nothing currently.
 **********/
XERXES_STATIC int XERXES_API dxp_do_cmd(int *ioChan, byte_t cmd, unsigned int lenS,
					byte_t *send, unsigned int lenR, byte_t *receive,
					byte_t ioFlags)
{
    UNUSED(ioChan);
    UNUSED(cmd);
    UNUSED(lenS);
    UNUSED(send);
    UNUSED(lenR);
    UNUSED(receive);
    UNUSED(ioFlags);
    
    return DXP_SUCCESS;
}


/**********
 * Calls the interface close routine.
 **********/
XERXES_STATIC int XERXES_API dxp_unhook(Board *board)
{
    int status;

    
    status = board->iface->funcs->dxp_md_close(0);

    /* Ignore the status due to some issues involving
     * repetitive function calls.
     */
    
    return DXP_SUCCESS;
}


/**********
 *
 **********/
XERXES_STATIC int XERXES_API dxp_read_one_dspsymbol(int *ioChan, int *modChan, char *name, 
												   Dsp_Info *dsp, unsigned short *value)
{
  int status;

  unsigned int i;

  unsigned short addr = 0x0000;
  
  parameter_t param = 0x0000;
  
  char info_string[INFO_LEN];
  char uname[MAX_DSP_PARAM_NAME_LEN];


  /* Check that the length of the name is less than maximum allowed length */

  if (strlen(name) > dsp->params->maxsymlen) {
	status = DXP_NOSYMBOL;
	sprintf(info_string, "Symbol Name must be <%i characters", dsp->params->maxsymlen);
	dxp_log_error("dxp_read_one_dspsymbol", info_string, status);
	return status;
  }

  /* Convert the name to upper case for comparison */
  for (i = 0; i < (strlen(name) + 1); i++) { 
	uname[i] = (char) toupper(name[i]); 
  }

  /* Be paranoid and check if the DSP configuration is downloaded.  If not, warn the user */
  if (dsp->proglen <= 0) {
	status = DXP_DSPLOAD;
	sprintf(info_string, "Must Load DSP code before reading %s", name);
	dxp_log_error("dxp_read_one_dspsymbol", info_string, status);
	return status;
  }  

  /* Get address of DSP parameter in memory/array */
  status = dxp_loc(uname, dsp, &addr);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Unable to locate %s in DSP code", uname);
	dxp_log_error("dxp_read_one_dspsymbol", info_string, status);
	return status;
  }

  /* Check R/W permissions on DSP parameter: Write-Only = 2 */
  if (dsp->params->parameters[addr].access == 2) {
	status = DXP_DSPACCESS;
	sprintf(info_string, "Parameter %s is Write-Only", uname);
	dxp_log_error("dxp_read_one_dspsymbol", info_string, status);
	return status;
  }

  addr = (unsigned short)(dsp->params->parameters[addr].address + DSP_DATA_MEM_OFFSET);

  status = dxp_read_word(ioChan, modChan, &addr, &param);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error reading word from addr = %u", addr);
	dxp_log_error("dxp_read_one_dspsymbol", info_string, status);
	return status;
  }

  *value = param;
  
  return DXP_SUCCESS;
}


/** @brief Read out the SCA data buffer from the board
 *
 */
XERXES_STATIC int XERXES_API dxp_read_sca(int *ioChan, int *modChan, Board* board, unsigned long *sca)
{
  int status;

  unsigned long n_bytes = 0;

  unsigned short idx   = 0;
  unsigned short start = 0;
  
  unsigned int i;
  unsigned int len = 0;

  char info_string[INFO_LEN];

  unsigned short *sca_short = NULL;


  ASSERT(ioChan != NULL);
  ASSERT(modChan != NULL);
  ASSERT(board != NULL);
  ASSERT(sca != NULL);


  status = dxp_loc("SCADSTART", board->dsp[*modChan], &idx);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Unable to locate SCADSTART for ioChan '%d', modChan '%d'",
			*ioChan, *modChan);
	dxp_log_error("dxp_read_sca", info_string, status);
	return status;
  }

  start = (unsigned short)(board->params[*modChan][idx] + DATA_BASE);

  status = dxp_loc("SCADLEN", board->dsp[*modChan], &idx);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Unable to locate SCADLEN for ioChan '%d', modChan '%d'",
			*ioChan, *modChan);
	dxp_log_error("dxp_read_sca", info_string, status);
	return status;
  }

  len = (unsigned int)board->params[*modChan][idx];

  n_bytes = len * sizeof(unsigned short);
  sca_short = (unsigned short *)dxp4c2x_md_alloc(n_bytes);

  if (sca_short == NULL) {
	sprintf(info_string, "Error allocating %lu bytes for 'sca_short'", n_bytes);
	dxp_log_error("dxp_read_sca", info_string, DXP_NOMEM);
	return DXP_NOMEM;
  }

  status = dxp_read_block(ioChan, modChan, &start, &len, sca_short);

  if (status != DXP_SUCCESS) {
	dxp4c2x_md_free(sca_short);
	dxp_log_error("dxp_read_sca", "Error reading SCA data buffer", status);
	return status;
  }

  for (i = 0; i < (unsigned int)(len / 2); i++) {
	sca[i] = (unsigned long)(((unsigned long)sca_short[(i * 2) + 1] << 16) | sca_short[i * 2]);
  }

  dxp4c2x_md_free(sca_short);

  return DXP_SUCCESS;
}


/** @brief Read the external memory from the hardware, if it is available
 *
 * This routine is a wrapper around the raw implementation of the memory read
 * as implemented in the control tasks.
 */
XERXES_STATIC int dxp_read_external_memory(int *ioChan, int *modChan,
										   Board *board, unsigned long base,
										   unsigned long offset,
										   unsigned long *data)
{
  int status;
  int i;
  int current   = 0;
  int mem_len   = 0;
  
  /* This cast really negates the point of typing 'base' and 'offset' to be
   * unsigned long. Unfortunately, the 'info' array for the control task
   * routines is spec'd as type int.
   */
  int requested = (int)offset;

  unsigned int info_len  = 4;

  short task   = CT_DXP2X_READ_MEMORY;
  
  float wait   = .001f;
  float poll   = 0.0;

  double codevar = 0.0;
  double numpages = 0.0;

  unsigned short CODEVAR = 0;
  unsigned short NUMPAGES = 0;

  unsigned long total_mem = 0;

  int info[4];

  char info_string[INFO_LEN];

  unsigned long *buf = NULL;
  

  
  ASSERT(ioChan != NULL);
  ASSERT(modChan != NULL);
  ASSERT(board != NULL);


  /* Verify that this firmware supports external memory operations */
  status = dxp_read_dspsymbol(ioChan, modChan, "CODEVAR", board->dsp[*modChan],
							  &codevar);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error reading CODEVAR for ioChan = %d, modChan = %d",
			*ioChan, *modChan);
	dxp_log_error("dxp_read_external_memory", info_string, status);
	return status;
  }
 
  CODEVAR = (unsigned short)codevar;

  if ((CODEVAR != CV_MULTSCA_SCAN_EXT_COM) &&
	  (CODEVAR != CV_MULTSCA_SCAN_EXT_STD) &&
	  (CODEVAR != CV_MULTMCA_SCAN_EXT_COM) &&
	  (CODEVAR != CV_MULTMCA_SCAN_EXT_STD) &&
	  (CODEVAR != CV_LIST_MODE_EXT)        &&
	  (CODEVAR != CV_EXT_MEM_TEST))
	{
	  sprintf(info_string, "Current DSP Code (CODEVAR = %u) doesn't support "
			  "external memory operations for ioChan = %d, modChan = %d",
			  CODEVAR, *ioChan, *modChan);
	  dxp_log_error("dxp_read_external_memory", info_string, DXP_NOSUPPORT);
	  return DXP_NOSUPPORT;
	}

  /* Verify that the user didn't request a zero-len array */
  if (offset == 0) {
	sprintf(info_string, "Requested external memory length must be non-zero for "
			"ioChan = %d, modChan = %d", *ioChan, *modChan);
	dxp_log_error("dxp_read_external_memory", info_string, DXP_MEMORY_LENGTH);
	return DXP_MEMORY_LENGTH;
  }

  /* Verify that the memory requested is actually on the board */
  status = dxp_read_dspsymbol(ioChan, modChan, "NUMPAGES", board->dsp[*modChan],
							  &numpages);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error reading NUMPAGES for ioChan = %d, modChan = %d",
			*ioChan, *modChan);
	dxp_log_error("dxp_read_external_memory", info_string, status);
	return status;
  }

  NUMPAGES = (unsigned short)numpages;

  /* page = 16kB. total_mem is therefore in bytes */
  total_mem = (unsigned long)(NUMPAGES * 16384);

  if (base + offset > (total_mem / 2)) {
	sprintf(info_string, "Requested memory (base = %#x, length = %lu) is larger "
			"then the available memory (%lu bytes) for ioChan = %d,"
			" modChan = %d", base, offset, total_mem, *ioChan, *modChan);
	dxp_log_error("dxp_read_external_memory", info_string, DXP_MEMORY_OOR);
	return DXP_MEMORY_OOR;
  }

  status = dxp_control_task_params(ioChan, modChan, &task, board, info);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error getting 'info' for external memory control task "
			"for ioChan = %d, modChan = %d", *ioChan, *modChan);
	dxp_log_error("dxp_read_external_memory", info_string, status);
	return status;
  }

  /* x10p.c overrides the wait time in info[1] with a fixed wait of .001. I don't
   * know why this is a good idea but I will keep the same for now.
   */
  mem_len = info[0];
  poll    = (float)info[2];

  /* If the requested memory length is longer then the maximum buffer size
   * we will need to do multiple reads.
   */
  for (i = 0; requested > 0; requested -= mem_len, i++) {

	/* EXTPAGE, etc. are stored in the info[] array.
	 * See "DSP Implemenation for Reading External Memory" for more
	 * details.
	 */
	if (i == 0) {
	  info[1] = (int)((base >> 14) & 0xFF);
	  info[2] = (int)(base & 0x3FFF);
	} else {
	  info[1] = -1;
	  info[2] = -1;
	}
	
	if (requested > mem_len) {
	  current = mem_len;
	} else {
	  current = requested;
	}

	info[3] = current;

	status = dxp_begin_control_task(ioChan, modChan, &task, &info_len, info,
									board);

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Error starting external memory control task for "
			  "ioChan = %d, modChan %d", *ioChan, *modChan);
	  dxp_log_error("dxp_read_external_memory", info_string, status);
	  return status;
	}

	status = dxp_wait_for_busy(ioChan, modChan, board, 1000, BUSY_READ_EXT_MEM,
							   wait);

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Error waiting for BUSY while reading external "
			  "memory for ioChan = %d, modChan = %d", *ioChan, *modChan);
	  dxp_log_error("dxp_read_external_memory", info_string, status);
	  return status;
	}

	/* Copy the read external memory back to the user */
	buf = (unsigned long *)dxp4c2x_md_alloc(current * sizeof(unsigned long));

	if (!buf) {
	  sprintf(info_string, "Error allocating %d bytes for 'buf' for ioChan = %d "
			  "modChan = %d", current * sizeof(unsigned long), *ioChan,
			  *modChan);
	  dxp_log_error("dxp_read_external_memory", info_string, DXP_NOMEM);
	  return DXP_NOMEM;
	}

	status = dxp_control_task_data(ioChan, modChan, &task, board, (void *)buf);

	if (status != DXP_SUCCESS) {
	  dxp4c2x_md_free(buf);
	  sprintf(info_string, "Error reading external memory data for ioChan = %d, "
			  "modChan = %d", *ioChan, *modChan);
	  dxp_log_error("dxp_read_external_memory", info_string, status);
	  return status;
	}

	memcpy(data + (i * mem_len), buf, current * sizeof(unsigned long));

	dxp4c2x_md_free(buf);
	buf = NULL;

	status = dxp_end_control_task(ioChan, modChan, board);

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Error stopping external memory control task for "
			  "ioChan = %d, modChan = %d", *ioChan, *modChan);
	  dxp_log_error("dxp_read_external_memory", info_string, status);
	  return status;
	}
  }

  return DXP_SUCCESS;
}


/** @brief Waits for BUSY to reach the specified value within the given timeout
 *
 */
XERXES_STATIC int dxp_wait_for_busy(int *ioChan, int *modChan, Board *board,
									int n_timeout, double busy, float wait)
{
  int status;
  int i;

  double BUSY = 0.0;

  char info_string[INFO_LEN];


  ASSERT(ioChan != NULL);
  ASSERT(modChan != NULL);
  ASSERT(board != NULL);


  for (i = 0; i < n_timeout; i++) {
	status = dxp_read_dspsymbol(ioChan, modChan, "BUSY", board->dsp[*modChan],
								&BUSY);

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Error reading BUSY: ioChan = %d, modChan = %d",
			  *ioChan, *modChan);
	  dxp_log_error("dxp_wait_for_busy", info_string, status);
	  return status;
	}

	if (BUSY == busy) {
	  break;
	}
	
	dxp4c2x_md_wait(&wait);
  }

  if (i == n_timeout) {
	sprintf(info_string, "Timeout (%d iterations, %.3f s per iteration) wait "
			"for BUSY = %u", n_timeout, wait, (unsigned short)busy);
	dxp_log_error("dxp_wait_for_busy", info_string, DXP_TIMEOUT);
	return DXP_TIMEOUT;
  }  

  return DXP_SUCCESS;

}
