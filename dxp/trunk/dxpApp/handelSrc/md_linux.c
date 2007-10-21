/*****************************************************************************
 *
 *  md_linux.c
 *
 *  Created 08/12/03 -- PJF
 *
 * Copyright (c) 2003,2004, X-ray Instrumentation Associates
 *               2005, XIA LLC
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
 * NOTE: This is a stubbed-out driver for testing the compilation ONLY!
 * It is the user's responsibility to provide the appropriate drivers for
 * their platform. XIA will gladly accept submissions of working Linux
 * drivers to incorporate into the official distribution.
 *
 */


/* System include files */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#include <sys/time.h>
#include <unistd.h>

/* XIA include files */
#include "xerxes_errors.h"
#include "xerxes_structures.h"
#include "md_linux.h"
#include "md_generic.h"
#include "xia_md.h"
#include "xia_common.h"
#include "xia_assert.h"


/* total number of modules in the system */
static unsigned int numMod    = 0;
static unsigned int numDXP    = 0;
static unsigned int numEPP    = 0;
static unsigned int numUSB    = 0;

/* are we in debug mode? */
static int print_debug=0;

/* maximum number of words able to transfer in a single call to dxp_md_io() */
static unsigned int maxblk=0;

static char error_string[132];

#ifndef EXCLUDE_CAMAC
static char *camacName[MAXMOD];
#endif /* EXCLUDE_CAMAC */


#ifndef EXCLUDE_EPP
/* EPP definitions */
/* The id variable stores an optional ID number associated with each module 
 * (initially included for handling multiple EPP modules hanging off the same
 * EPP address)
 */
static int eppID[MAXMOD];
/* variables to store the IO channel information */
static char *eppName[MAXMOD];
/* Port stores the port number for each module, only used for the X10P/G200 */
static unsigned int port;

static unsigned short next_addr = 0;

/* Store the currentID used for Daisy Chain systems */
static int currentID = -1;
#endif /* EXCLUDE_EPP */


#ifndef EXCLUDE_USB
/* USB definitions */
/* The id variable stores an optional ID number associated with each module 
 */
static int usbID[MAXMOD];
/* variables to store the IO channel information */
static char *usbName[MAXMOD];
/* variables to store the USB Device Name */
static char *usbDevice[MAXMOD];

static HANDLE usbHandle[MAXMOD];

static long usb_addr=0;
#endif /* EXCLUDE_USB IO */


#ifndef EXCLUDE_USB2

#include "xia_usb2.h"
#include "xia_usb2_errors.h"

/* A string holding the device number in it. */
static char *usb2Names[MAXMOD];

/* The OS handle used for communication. */
static HANDLE usb2Handles[MAXMOD];

/* The cached target address for the next operation. */
static int usb2AddrCache[MAXMOD];

#endif /* EXCLUDE_USB2 */



static char *TMP_PATH = "/tmp";
static char *PATH_SEP = "/";


/******************************************************************************
 *
 * Routine to create pointers to the MD utility routines
 * 
 ******************************************************************************/
XIA_MD_EXPORT int XIA_MD_API dxp_md_init_util(Xia_Util_Functions* funcs, char* type)
{
    /* Check to see if we are intializing the library with this call in addition
     * to assigning function pointers. 
     */

    if (type != NULL) 
    {
	if (STREQ(type, "INIT_LIBRARY")) { numMod = 0; }
    }

    funcs->dxp_md_error_control = dxp_md_error_control;
    funcs->dxp_md_alloc         = dxp_md_alloc;
    funcs->dxp_md_free          = dxp_md_free;
    funcs->dxp_md_puts          = dxp_md_puts;
    funcs->dxp_md_wait          = dxp_md_wait;

    funcs->dxp_md_error         = dxp_md_error;
    funcs->dxp_md_warning       = dxp_md_warning;
    funcs->dxp_md_info          = dxp_md_info;
    funcs->dxp_md_debug         = dxp_md_debug;
    funcs->dxp_md_output        = dxp_md_output;
    funcs->dxp_md_suppress_log  = dxp_md_suppress_log;
    funcs->dxp_md_enable_log    = dxp_md_enable_log;
    funcs->dxp_md_set_log_level = dxp_md_set_log_level;
    funcs->dxp_md_log	        = dxp_md_log;
    funcs->dxp_md_fgets         = dxp_md_fgets;
    funcs->dxp_md_tmp_path      = dxp_md_tmp_path;
    funcs->dxp_md_clear_tmp     = dxp_md_clear_tmp;
    funcs->dxp_md_path_separator = dxp_md_path_separator;

    if (out_stream == NULL)
    {
	out_stream = stdout;
    }

    md_md_alloc = dxp_md_alloc;
    md_md_free  = dxp_md_free;

    return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Routine to create pointers to the MD utility routines
 * 
 ******************************************************************************/
XIA_MD_EXPORT int XIA_MD_API dxp_md_init_io(Xia_Io_Functions* funcs, char* type)
{
    unsigned int i;
	
    for (i = 0; i < strlen(type); i++) {

	type[i]= (char)tolower(type[i]);
    }


#ifndef EXCLUDE_CAMAC
    if (STREQ(type, "camac")) {

	funcs->dxp_md_io         = dxp_md_io;
	funcs->dxp_md_initialize = dxp_md_initialize;
	funcs->dxp_md_open       = dxp_md_open;
	funcs->dxp_md_close      = dxp_md_close;
    } 
#endif /* EXCLUDE_CAMAC */

#ifndef EXCLUDE_EPP
    if (STREQ(type, "epp")) {

	funcs->dxp_md_io         = dxp_md_epp_io;
	funcs->dxp_md_initialize = dxp_md_epp_initialize;
	funcs->dxp_md_open       = dxp_md_epp_open;
	funcs->dxp_md_close      = dxp_md_epp_close;
    }
#endif /* EXCLUDE_EPP */

#ifndef EXCLUDE_USB
    if (STREQ(type, "usb")) 
	  {
		funcs->dxp_md_io            = dxp_md_usb_io;
		funcs->dxp_md_initialize    = dxp_md_usb_initialize;
		funcs->dxp_md_open          = dxp_md_usb_open;
		funcs->dxp_md_close         = dxp_md_usb_close;
	  }
#endif /* EXCLUDE_USB */

#ifndef EXCLUDE_SERIAL
    if (STREQ(type, "serial")) 
	  {
		funcs->dxp_md_io            = dxp_md_serial_io;
		funcs->dxp_md_initialize    = dxp_md_serial_initialize;
		funcs->dxp_md_open          = dxp_md_serial_open;
		funcs->dxp_md_close         = dxp_md_serial_close;
	  }
#endif /* EXCLUDE_SERIAL */

#ifndef EXCLUDE_ARCNET
    if (STREQ(type, "arcnet")) 
	  {
		funcs->dxp_md_io            = dxp_md_arcnet_io;
		funcs->dxp_md_initialize    = dxp_md_arcnet_initialize;
		funcs->dxp_md_open          = dxp_md_arcnet_open;
		funcs->dxp_md_close         = dxp_md_arcnet_close;
	  }
#endif /* EXCLUDE_UDXP */


    funcs->dxp_md_get_maxblk    = dxp_md_get_maxblk;
    funcs->dxp_md_set_maxblk    = dxp_md_set_maxblk;
    funcs->dxp_md_lock_resource = dxp_md_lock_resource;


    return DXP_SUCCESS;
}

#ifndef EXCLUDE_CAMAC
/*****************************************************************************
 * 
 * Initialize the system.  Alloocate the space for the library arrays, define
 * the pointer to the CAMAC library and the IO routine.
 * 
 *****************************************************************************/
XIA_MD_STATIC int dxp_md_initialize(unsigned int* maxMod, char* dllname)
{
  int status;

  UNUSED(dllname);


  if (*maxMod > MAXMOD) {
    sprintf(error_string, "%u module requested; only %u are availabled",
	    *maxMod, MAXMOD);
    dxp_md_log_error("dxp_md_initialize", error_string, DXP_NOMEM);
    return DXP_NOMEM;
  }

  numDXP = 0;

  return DXP_SUCCESS;
}


/*****************************************************************************
 * 
 * Routine is passed the user defined configuration string *name.  This string
 * contains all the information needed to point to the proper IO channel by 
 * future calls to dxp_md_io().  In the case of a simple CAMAC crate, the string 
 * should contain a branch number (representing the SCSI bus number in this case),
 * a crate number (single SCSI can control multiple crates) and a station 
 * number (better known as a slot number).
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_open(char* ioname, int* camChan)
{
  unsigned int i;

  short camadr[3];

  long status;


  for (i = 0; i < numDXP; i++) {
    if (STREQ(camacName[i], ioname)) {
      *camChan = (int)i;
      return DXP_SUCCESS;
    }
  }

  if (camacName[numDXP]) {
    md_md_free(camacName[numDXP]);
  }

  camacName[numDXP] = md_md_alloc(strlen(ioname) + 1);

  if (!camacName) {
    sprintf(error_string, "Error allocating %d bytes for 'camacName[%d]'",
	    strlen(ioname) + 1, i);
    dxp_md_log_error("dxp_md_open", error_string, DXP_NOMEM);
    return DXP_NOMEM;
  }

  strcpy(camacName[numDXP], ioname);

  /* Initialize the sjy driver. */
  sscanf(camacName[numDXP], "%1hd%1hd%2hd", &camadr[0], &camadr[1], &camadr[2]); 
  
  status = xia_caminit(camadr);

  if (status != XIA_CAMAC_SUCCESS) {
    sprintf(error_string, "Error initializing CAMAC driver for %d/%d/%d",
	    camadr[0], camadr[1], camadr[2]);
    dxp_md_log_error("dxp_md_open", error_string, DXP_MDOPEN);
    return DXP_MDOPEN;
  }

  *camChan = numDXP++;
  numMod++;

  return DXP_SUCCESS;
}

/*****************************************************************************
 * 
 * This routine performs the IO call to read or write data.  The pointer to 
 * the desired IO channel is passed as camChan.  The address to write to is
 * specified by function and address.  The data length is specified by 
 * length.  And the data itself is stored in data.
 * 
 *****************************************************************************/
XIA_MD_STATIC int dxp_md_io(int* camChan, unsigned int* function, 
			    unsigned long *address, void *data,
			    unsigned int* length)
{
  long status;
  long nbytes = 0;

  short camadr[3];


  sscanf(camacName[*camChan], "%1hd%1hd%2hd", &camadr[0], &camadr[1], &camadr[2]);

  nbytes = 2 * (*length);

  status = xia_camxfr(camadr, (short)(*function), (short)(*address), nbytes, 0,
		      (short *)data);

  if (status != XIA_CAMAC_SUCCESS) {
    sprintf(error_string, "Error doing camac transfer (f = %u, a = %lu), "
	    "status = %ld for camChan = %d", *function, *address, status,
	    *camChan);
    dxp_md_log_error("dxp_md_io", error_string, DXP_MDIO);
    return DXP_MDIO;
  }

  return DXP_SUCCESS;
}


/**********
 * This routine is used to "close" the CAMAC connection.
 * For CAMAC, nothing needs to be done...
 **********/
XIA_MD_STATIC int XIA_MD_API dxp_md_close(int *camChan)
{
    UNUSED(camChan);

    return DXP_SUCCESS;
}

#endif /* EXCLUDE_CAMAC */


#ifndef EXCLUDE_EPP
/*****************************************************************************
 * 
 * Initialize the system.  Alloocate the space for the library arrays, define
 * the pointer to the CAMAC library and the IO routine.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_initialize(unsigned int* maxMod, char* dllname)
{
  int status = DXP_SUCCESS;
  int rstat = 0;

  /* EPP initialization */

  /* check if all the memory was allocated */
  if (*maxMod>MAXMOD){
    status = DXP_NOMEM;
    sprintf(error_string,"Calling routine requests %d maximum modules: only %d available.", 
	    *maxMod, MAXMOD);
    dxp_md_log_error("dxp_md_epp_initialize",error_string,status);
    return status;
  }

  /* Zero out the number of modules currently in the system */
  numEPP = 0;

  /* Initialize the EPP port */
  rstat = sscanf(dllname,"%x",&port);
  if (rstat!=1) {
    status = DXP_NOMATCH;
    dxp_md_log_error("dxp_md_epp_initialize",
		     "Unable to read the EPP port address",status);
    return status;
  }
													 
  sprintf(error_string, "EPP Port = %#x", port);
  dxp_md_log_debug("dxp_md_epp_initialize", error_string);

  /* Move the call to InitEPP() to the open() routine, this will allow daisy chain IDs to work. 
   * NOTE: since the port number is stored in a static global, init() better not get called again
   * with a different port, before the open() call!!!!!
   */ 
  /* Call the EPPLIB.DLL routine */	
  /*  rstat = DxpInitEPP((int)port);*/


  /* Check for Success */
  /*  if (rstat==0) {
      status = DXP_SUCCESS;
      } else {
      status = DXP_INITIALIZE;
      sprintf(error_string,
      "Unable to initialize the EPP port: rstat=%d",rstat);
      dxp_md_log_error("dxp_md_epp_initialize", error_string,status);
      return status;
      }*/

  /* Reset the currentID when the EPP interface is initialized */
  currentID = -1;

  return status;
}


/*****************************************************************************
 * 
 * Routine is passed the user defined configuration string *name.  This string
 * contains all the information needed to point to the proper IO channel by 
 * future calls to dxp_md_io().  In the case of a simple CAMAC crate, the string 
 * should contain a branch number (representing the SCSI bus number in this case),
 * a crate number (single SCSI can control multiple crates) and a station 
 * number (better known as a slot number).
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_open(char* ioname, int* camChan)
{
  unsigned int i;
  int status=DXP_SUCCESS;
  int rstat = 0;

  sprintf(error_string, "ioname = %s", ioname);
  dxp_md_log_debug("dxp_md_epp_open", error_string);

  /* First loop over the existing names to make sure this module 
   * was not already configured?  Don't want/need to perform
   * this operation 2 times. */
    
  for(i=0;i<numEPP;i++)
    {
      if(STREQ(eppName[i],ioname)) 
	{
	  status=DXP_SUCCESS;
	  *camChan = i;
	  return status;
	}
    }

  /* Got a new one.  Increase the number existing and assign the global 
   * information */

  if (eppName[numEPP]!=NULL) 
    {
      md_md_free(eppName[numEPP]);
    }
  eppName[numEPP] = (char *) md_md_alloc((strlen(ioname)+1)*sizeof(char));
  strcpy(eppName[numEPP],ioname);

  /* See if this is a multi-module EPP chain, if not set its ID to -1 */
  if (ioname[0] == ':') 
    {
      sscanf(ioname, ":%d", &(eppID[numEPP]));
		
      sprintf(error_string, "ID = %i", eppID[numEPP]);
      dxp_md_log_debug("dxp_md_epp_open", error_string);
		
      /* Initialize the port address first */
      rstat = DxpInitPortAddress((int) port);
      if (rstat != 0) 
	{
	  status = DXP_INITIALIZE;
	  sprintf(error_string,
		  "Unable to initialize the EPP port address: port=%d", port);
	  dxp_md_log_error("dxp_md_epp_open", error_string, status);
	  return status;
	}
		
      /* Call setID now to setup the port for Initialization */
      DxpSetID((unsigned short) eppID[numEPP]); 
      /* No return value
	 if (rstat != 0) 
	 {
	 status = DXP_INITIALIZE;
	 sprintf(error_string,
	 "Unable to set the EPP Port ID: ID=%d", id[numEPP]);
	 dxp_md_log_error("dxp_md_epp_open", error_string, status);
	 return status;
	 }*/
    } else {
    eppID[numEPP] = -1;
  }
	
  /* Call the EPPLIB.DLL routine */	
  rstat = DxpInitEPP((int)port);
	
  /* Check for Success */
  if (rstat==0) 
    {
      status = DXP_SUCCESS;
    } else {
    status = DXP_INITIALIZE;
    sprintf(error_string,
	    "Unable to initialize the EPP port: rstat=%d",rstat);
    dxp_md_log_error("dxp_md_epp_open", error_string,status);
    return status;
  }
	
  *camChan = numEPP++;
  numMod++;

  return status;
}


/*****************************************************************************
 * 
 * This routine performs the IO call to read or write data.  The pointer to 
 * the desired IO channel is passed as camChan.  The address to write to is
 * specified by function and address.  The data length is specified by 
 * length.  And the data itself is stored in data.
 * 
 *****************************************************************************/
XIA_MD_STATIC int dxp_md_epp_io(int* camChan, unsigned int* function, 
				unsigned long *address, void *data,
				unsigned int* length)
{
  int rstat = 0; 
  int status;
  
  unsigned int i;

  unsigned short *us_data = (unsigned short *)data;

  unsigned long *temp=NULL;


  if ((currentID != eppID[*camChan]) && (eppID[*camChan] != -1))
    {
      DxpSetID((unsigned short) eppID[*camChan]);
		
      /* Update the currentID */
      currentID = eppID[*camChan];
		
      sprintf(error_string, "calling SetID = %i, camChan = %i", eppID[*camChan], *camChan);
      dxp_md_log_debug("dxp_md_epp_io", error_string);
    }
	
  /* Data*/
  if (*address==0) {
    /* Perform short reads and writes if not in program address space */
    if (next_addr>=0x4000) {
      if (*length>1) {
	if (*function == MD_IO_READ) {
	  rstat = DxpReadBlock(next_addr, us_data, (int) *length);
	} else {
	  rstat = DxpWriteBlock(next_addr, us_data, (int) *length);
	}
      } else {
	if (*function == MD_IO_READ) {
	  rstat = DxpReadWord(next_addr, us_data);
	} else {
	  rstat = DxpWriteWord(next_addr, us_data[0]);
	}
      }
    } else {
      /* Perform long reads and writes if in program address space (24-bit) */
      /* Allocate memory */
      temp = (unsigned long *) md_md_alloc(sizeof(unsigned short)*(*length));
      if (*function == MD_IO_READ) {
	rstat = DxpReadBlocklong(next_addr, temp, (int) *length/2);
	/* reverse the byte order for the EPPLIB library */
	for (i=0;i<*length/2;i++) {
	  us_data[2*i] = (unsigned short) (temp[i]&0xFFFF);
	  us_data[2*i+1] = (unsigned short) ((temp[i]>>16)&0xFFFF);
	}
      } else {
	/* reverse the byte order for the EPPLIB library */
	for (i=0;i<*length/2;i++) {
	  temp[i] = ((us_data[2*i]<<16) + us_data[2*i+1]);
	}
	rstat = DxpWriteBlocklong(next_addr, temp, (int) *length/2);
      }
      /* Free the memory */
      md_md_free(temp);
    }
    /* Address port*/
  } else if (*address==1) {
    next_addr = (unsigned short) *us_data;
    /* Control port*/
  } else if (*address==2) {
    /*		dest = cport;
     *length = 1;
     */
    /* Status port*/
  } else if (*address==3) {
    /*		dest = sport;
     *length = 1;*/
  } else {
    sprintf(error_string,"Unknown EPP address=%#lx",*address);
    status = DXP_MDIO;
    dxp_md_log_error("dxp_md_epp_io",error_string,status);
    return status;
  }

  if (rstat!=0) {
    status = DXP_MDIO;
    sprintf(error_string,"Problem Performing I/O to Function: %d, address: %#lx",*function, *address);
    dxp_md_log_error("dxp_md_epp_io",error_string,status);
    sprintf(error_string,"Trying to write to internal address: %d, length %d",next_addr, *length);
    dxp_md_log_error("dxp_md_epp_io",error_string,status);
    return status;
  }
	
  status=DXP_SUCCESS;

  return status;
}


/**********
 * "Closes" the EPP connection, which means that it does nothing.
 **********/
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_close(int *camChan)
{
    UNUSED(camChan);
    
    return DXP_SUCCESS;
}

#endif /* EXCLUDE_EPP */


#ifndef EXCLUDE_USB
/*****************************************************************************
 * 
 * Initialize the USB system.  
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_initialize(unsigned int* maxMod, char* dllname)
/* unsigned int *maxMod;					Input: maximum number of dxp modules allowed */
/* char *dllname;							Input: name of the DLL						*/
{
    int status = DXP_SUCCESS;

	UNUSED(dllname);

    /* USB initialization */

    /* check if all the memory was allocated */
    if (*maxMod>MAXMOD)
	  {
		status = DXP_NOMEM;
		sprintf(error_string,"Calling routine requests %d maximum modules: only %d available.", 
				*maxMod, MAXMOD);
		dxp_md_log_error("dxp_md_usb_initialize",error_string,status);
		return status;
	  }

    /* Zero out the number of modules currently in the system */
    numUSB = 0;

    return status;
}
/*****************************************************************************
 * 
 * Routine is passed the user defined configuration string *name.  This string
 * contains all the information needed to point to the proper IO channel by 
 * future calls to dxp_md_io().  In the case of a simple CAMAC crate, the string 
 * should contain a branch number (representing the SCSI bus number in this case),
 * a crate number (single SCSI can control multiple crates) and a station 
 * number (better known as a slot number).
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_open(char* ioname, int* camChan)
	 /* char *ioname;				 Input:  string used to specify this IO channel */
	 /* int *camChan;				 Output: returns a reference number for this module */
{
  int status=DXP_SUCCESS;
  unsigned int i;

  /* Temporary name so that we can get the length */
  char tempName[200];
  
  sprintf(error_string, "ioname = %s", ioname);
  dxp_md_log_debug("dxp_md_usb_open", error_string);
  
  /* First loop over the existing names to make sure this module 
   * was not already configured?  Don't want/need to perform
   * this operation 2 times. */
  
  for(i=0;i<numUSB;i++)
	{
	  if(STREQ(usbName[i],ioname)) 
		{
		  status=DXP_SUCCESS;
		  *camChan = i;
		  return status;
		}
	}
  
  /* Got a new one.  Increase the number existing and assign the global 
   * information */
  sprintf(tempName, "\\\\.\\ezusb-%s", ioname); 
  
  if (usbName[numUSB]!=NULL) 
	{
	  md_md_free(usbName[numUSB]);
	}
  usbName[numUSB] = (char *) md_md_alloc((strlen(tempName)+1)*sizeof(char));
  strcpy(usbName[numUSB],tempName);
  
  /*  dxp_md_log_info("dxp_md_usb_open", "Attempting to open usb handel");
	  if (usb_open(usbName[numUSB], &usbHandle[numUSB]) != 0) 
	  {
	  dxp_md_log_info("dxp_md_usb_open", "Unable to open usb handel");
	  }
	  sprintf(error_string, "device = %s, handle = %i", usbName[numUSB], usbHandle[numUSB]);
	  dxp_md_log_info("dxp_md_usb_open", error_string);
  */
    *camChan = numUSB++;
    numMod++;

    return status;
}


/*****************************************************************************
 * 
 * This routine performs the IO call to read or write data.  The pointer to 
 * the desired IO channel is passed as camChan.  The address to write to is
 * specified by function and address.  The data length is specified by 
 * length.  And the data itself is stored in data.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_io(int* camChan, unsigned int* function, 
										   unsigned long *address,
										   void *data,
										   unsigned int* length)
	 /* int *camChan;				Input: pointer to IO channel to access	*/
	 /* unsigned int *function;		Input: XIA EPP function definition	    */
	 /* unsigned int *address;		Input: XIA EPP address definition	    */
	 /* unsigned short *data;		I/O:  data read or written		        */
	 /* unsigned int *length;		Input: how much data to read or write	*/
{
  int rstat = 0; 
  int status;
  
  unsigned short *us_data = (unsigned short *)data;

  if (*address == 0) 
	{
	  if (*function == MD_IO_READ) 
		{
		  rstat = usb_read(usb_addr, (long) *length, usbName[*camChan], us_data);
		} else {
		  rstat = usb_write(usb_addr, (long) *length, usbName[*camChan], us_data);
		}
	} else if (*address ==1) {
	  usb_addr = (long) us_data[0];
	}

  if (rstat != 0) 
	{
	  status = DXP_MDIO;
	  sprintf(error_string,"Problem Performing USB I/O to Function: %d, address: %d",*function, *address);
	  dxp_md_log_error("dxp_md_usb_io",error_string,status);
	  sprintf(error_string,"Trying to write to internal address: %#hx, length %d", usb_addr, *length);
	  dxp_md_log_error("dxp_md_usb_io",error_string,status);
	  return status;
	}
  
  status=DXP_SUCCESS;
  
  return status;
}


/**********
 * "Closes" the USB connection, which means that it does nothing.
 **********/
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_close(int *camChan)
{
    UNUSED(camChan);

    return DXP_SUCCESS;
}

#endif


#ifndef EXCLUDE_SERIAL
/**********
 * This routine initializes the requested serial port
 * for operation at the specified baud. 
 **********/
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_initialize(unsigned int *maxMod, char *dllname)
{
  UNUSED(maxMod);
  UNUSED(dllname);

    return DXP_SUCCESS;
}


/**********
 * This routine assigns the specified device a chan
 * number in the global array. (I think.)
 **********/ 
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_open(char* ioname, int* camChan)
{
  UNUSED(ioname);

    *camChan = numSerial++;

    return DXP_SUCCESS;
}


/**********
 * This routine performs the read/write operations 
 * on the serial port.
 **********/
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_io(int *camChan,
					      unsigned int *function,
					      unsigned int *address,
					      unsigned short *data,
					      unsigned int *length)
{
  UNUSED(camChan);
  UNUSED(function);
  UNUSED(address);
  UNUSED(data);
  UNUSED(length);

    return DXP_SUCCESS;
} 


/**********
 * Closes the serial port connection so that we don't
 * crash due to interrupt vectors left lying around.
 **********/
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_close(int *camChan)
{
  UNUSED(camChan);

  return DXP_SUCCESS;
}


#endif /* EXCLUDE_SERIAL */  
 
 
#ifndef EXCLUDE_ARCNET
/*****************************************************************************
 * 
 * Initialize the arcnet system
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_arcnet_initialize(unsigned int* maxMod, char* dllname)
{
  UNUSED(maxMod);
  UNUSED(dllname);

    return DXP_SUCCESS;
}


/*****************************************************************************
 * 
 * Routine is passed the user defined configuration string *name.  This string
 * contains the Arcnet Node ID
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_arcnet_open(char* ioname, int* camChan)
{
  UNUSED(ioname);
	
    *camChan = numArcnet++;
	
    return DXP_SUCCESS;
}


/*****************************************************************************
 * 
 * This routine performs the IO call to read or write data to/from the Arcnet
 * connection.  The pointer to the desired IO channel is passed as camChan.  
 * The address to write to is specified by function and address.  The 
 * length is specified by length.  And the data itself is stored in data.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_arcnet_io(int* camChan, unsigned int* function, 
					      unsigned long *address, void *data,
					      unsigned int* length)
{
  UNUSED(camChan);
  UNUSED(function);
  UNUSED(address);
  UNUSED(data);
  UNUSED(length);
  
  return DXP_SUCCESS;
}


/**********
 * "Closes" the Arcnet connection, which means that it does nothing.
 **********/
XIA_MD_STATIC int XIA_MD_API dxp_md_arcnet_close(int *camChan)
{
  UNUSED(camChan);
  
  return DXP_SUCCESS;
}

#endif /* EXCLUDE_ARCNET */

/*****************************************************************************
 * 
 * Routine to control the error reporting operation.  Currently the only 
 * keyword available is "print_debug".  Then whenever dxp_md_error() is called
 * with an error_code=DXP_DEBUG, the message is printed.  
 * 
 *****************************************************************************/
XIA_MD_STATIC void XIA_MD_API dxp_md_error_control(char* keyword, int* value)
{

    /* Enable debugging */

    if (strstr(keyword,"print_debug")!=NULL){
	print_debug=(*value);
	return;
    }

    /* Else we have a problem */

    dxp_md_puts("dxp_md_error_control: keyword %s not recognized..\n");
}

/*****************************************************************************
 * 
 * Routine to get the maximum number of words that can be block transfered at 
 * once.  This can change from system to system and from controller to 
 * controller.  A maxblk size of 0 means to write all data at once, regardless 
 * of transfer size.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_get_maxblk(void)
{

    return maxblk;

}

/*****************************************************************************
 * 
 * Routine to set the maximum number of words that can be block transfered at 
 * once.  This can change from system to system and from controller to 
 * controller.  A maxblk size of 0 means to write all data at once, regardless 
 * of transfer size.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_set_maxblk(unsigned int* blksiz)
/* unsigned int *blksiz;			*/
{
    int status; 

    if (*blksiz > 0) {
	maxblk = *blksiz;
    } else {
	status = DXP_NEGBLOCKSIZE;
	dxp_md_log_error("dxp_md_set_maxblk","Block size must be positive or zero",status);
	return status;
    }
    return DXP_SUCCESS;

}

/*****************************************************************************
 * 
 * Routine to wait a specified time in seconds.  This allows the user to call
 * routines that are as precise as required for the purpose at hand.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_wait(float* time)
{
  unsigned long microsecs = 0;

  microsecs = (unsigned long)(*time * 1.0e6);
  usleep(microsecs);

  return DXP_SUCCESS;
}

/*****************************************************************************
 * 
 * Routine to allocate memory.  The calling structure is the same as the 
 * ANSI C standard routine malloc().  
 * 
 *****************************************************************************/
XIA_MD_SHARED void* XIA_MD_API dxp_md_alloc(size_t length)
{

    return malloc(length);

}

/*****************************************************************************
 * 
 * Routine to free memory.  Same calling structure as the ANSI C routine 
 * free().
 * 
 *****************************************************************************/
XIA_MD_SHARED void XIA_MD_API dxp_md_free(void* array)
{

    free(array);

}

/*****************************************************************************
 * 
 * Routine to print a string to the screen or log.  No direct calls to 
 * printf() are performed by XIA libraries.  All output is directed either
 * through the dxp_md_error() or dxp_md_puts() routines.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_puts(char* s)
{

    return printf("%s", s);

}


XIA_MD_STATIC int XIA_MD_API dxp_md_lock_resource(int *ioChan, int *modChan, short *lock)
{
  UNUSED(ioChan);
  UNUSED(modChan);
  UNUSED(lock);

  return DXP_SUCCESS;
}


/** @brief Safe version of fgets() that can handle both UNIX and DOS
 * line-endings.
 *
 * If the trailing two characters are '\r' + \n', they are replaced by a
 * single '\n'.
 */
XIA_MD_STATIC char * dxp_md_fgets(char *s, int length, FILE *stream)
{
  int buf_len = 0;

  char *buf     = NULL;
  char *cstatus = NULL;


  ASSERT(s != NULL);
  ASSERT(stream != NULL);
  ASSERT(length > 0);


  buf = dxp_md_alloc(length + 1);

  if (!buf) {
    return NULL;
  }

  cstatus = fgets(buf, (length + 1), stream);

  if (!cstatus) {
    dxp_md_free(buf);
    return NULL;
  }

  buf_len = strlen(buf);

  if ((buf[buf_len - 2] == '\r') && (buf[buf_len - 1] == '\n')) {
    buf[buf_len - 2] = '\n';
    buf[buf_len - 1] = '\0';
  }

  ASSERT(strlen(buf) < length);

  strcpy(s, buf);

  free(buf);

  return s;
}


/** @brief Get a safe temporary directory path.
 *
 */
XIA_MD_STATIC char * dxp_md_tmp_path(void)
{
  return TMP_PATH;
}


/** @brief Clears the temporary path cache.
 *
 */
XIA_MD_STATIC void dxp_md_clear_tmp(void)
{
  return;
}


/** @brief Returns the path separator for Win32 platform.
 *
 */
XIA_MD_STATIC char * dxp_md_path_separator(void)
{
  return PATH_SEP;
}
