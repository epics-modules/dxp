/* Linux platform-dependent layer */

/*
 * Copyright (c) 2003-2004 X-ray Instrumentation Associates
 * Copyright (c) 2005-2015 XIA LLC
 * All rights reserved
 *
 * Contains significant contributions from Mark Rivers, University of
 * Chicago
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



/* System include files */

#define _XOPEN_SOURCE 500
#include <ctype.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#ifndef EXCLUDE_SERIAL
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#endif

/* XIA include files */
#include "xerxes_errors.h"
#include "xerxes_structures.h"
#include "md_linux.h"
#include "md_generic.h"
#include "xia_md.h"
#include "xia_common.h"
#include "xia_assert.h"

#define MODE 4


/* total number of modules in the system */
static unsigned int numMod    = 0;

/* error string used as a place holder for calls to dxp_md_error() */
static char ERROR_STRING[132];

/* maximum number of words able to transfer in a single call to dxp_md_io() */
static unsigned int maxblk=0;


#ifndef EXCLUDE_EPP
/* EPP definitions */
/* The id variable stores an optional ID number associated with each module
 * (initially included for handling multiple EPP modules hanging off the same
 * EPP address)
 */
static unsigned int numEPP    = 0;
static int eppID[MAXMOD];
/* variables to store the IO channel information */
static char *eppName[MAXMOD];
/* Port stores the port number for each module, only used for the X10P/G200 */
static unsigned short port;

static unsigned short next_addr = 0;

/* Store the currentID used for Daisy Chain systems */
static int currentID = -1;
#endif /* EXCLUDE_EPP */


#ifndef EXCLUDE_USB
/* USB definitions */
/* The id variable stores an optional ID number associated with each module
 */
static unsigned int numUSB    = 0;
/* variables to store the IO channel information */
static char *usbName[MAXMOD];

static long usb_addr=0;
#endif /* EXCLUDE_USB IO */

#ifndef EXCLUDE_USB2
/* USB 2.0 definitions */
/* The id variable stores an optional ID number associated with each module
 */
static unsigned int numUSB2    = 0;
#include "xia_usb2.h"
#include "xia_usb2_errors.h"

/* A string holding the device number in it. */
static char *usb2Names[MAXMOD];

/* The OS handle used for communication. */
static HANDLE usb2Handles[MAXMOD];

/* The cached target address for the next operation. */
static unsigned long usb2AddrCache[MAXMOD];

#endif /* EXCLUDE_USB2 */


#ifndef EXCLUDE_SERIAL

#define HEADER_SIZE 4

/* variables to store the IO channel information */
static char *serialName[MAXMOD];
static HANDLE serialHandles[MAXMOD];
static unsigned int numSerial = 0;

/* Serial port globals */
static int dxp_md_serial_read_header(int fd, unsigned short *bytes,
                                     unsigned short *buf);
static int dxp_md_serial_read_data(int fd,
                                   unsigned long size, unsigned short *buf);

#endif /* EXCLUDE_SERIAL */


/* PLCF 4p add functions so Linux looks like Windows.
   It hurts, but it was the easy way out.		*/
typedef unsigned int	DWORD;
typedef unsigned int	BOOL;

#define NORMAL_PRIORITY_CLASS 0
#define REALTIME_PRIORITY_CLASS -20
#define HIGH_PRIORITY_CLASS -10
#define ABOVE_NORMAL_PRIORITY_CLASS -5
#define BELOW_NORMAL_PRIORITY_CLASS 5
#define IDLE_PRIORITY_CLASS 19

void Sleep(DWORD sleep_msec);
unsigned int  GetCurrentProcess(void);
BOOL SetPriorityClass(unsigned int hProcess, DWORD dwPriorityClass);

static char *TMP_PATH = "/tmp";
static char *PATH_SEP = "/";


void Sleep(DWORD sleep_msec)
{
    struct timespec ts;

    ts.tv_sec = sleep_msec / 1000;
    ts.tv_nsec = (sleep_msec - (ts.tv_sec * 1000)) * 1000000;

    nanosleep(&ts, 0);
}


unsigned int GetCurrentProcess()		/* This only works for SetPriorityClass, not a true	*/
{   /* replacement for Microsoft Windows GetCurrentProcess.	*/
    return(0);
}

BOOL SetPriorityClass(unsigned int hProcess, DWORD dwPriorityClass)
{
    int				error;

    error = setpriority(PRIO_PROCESS, hProcess, dwPriorityClass);

    if (error == 0) return(1);
    else return(0);
}


/*
 * Routine to create pointers to the MD utility routines
 */
XIA_MD_EXPORT int XIA_MD_API dxp_md_init_util(Xia_Util_Functions* funcs, char* type)
/* Xia_Util_Functions *funcs;	*/
/* char *type;					*/
{
    /* Check to see if we are intializing the library with this call in addition
     * to assigning function pointers.
     */

    if (type != NULL) {
        if (STREQ(type, "INIT_LIBRARY")) {
            numMod = 0;
        }
    }

    funcs->dxp_md_alloc         = dxp_md_alloc;
    funcs->dxp_md_free          = dxp_md_free;
    funcs->dxp_md_puts          = dxp_md_puts;
    funcs->dxp_md_wait          = dxp_md_wait;

    funcs->dxp_md_error          = dxp_md_error;
    funcs->dxp_md_warning        = dxp_md_warning;
    funcs->dxp_md_info           = dxp_md_info;
    funcs->dxp_md_debug          = dxp_md_debug;
    funcs->dxp_md_output         = dxp_md_output;
    funcs->dxp_md_suppress_log   = dxp_md_suppress_log;
    funcs->dxp_md_enable_log     = dxp_md_enable_log;
    funcs->dxp_md_set_log_level  = dxp_md_set_log_level;
    funcs->dxp_md_log            = dxp_md_log;
    funcs->dxp_md_set_priority   = dxp_md_set_priority;
    funcs->dxp_md_fgets          = dxp_md_fgets;
    funcs->dxp_md_tmp_path       = dxp_md_tmp_path;
    funcs->dxp_md_clear_tmp      = dxp_md_clear_tmp;
    funcs->dxp_md_path_separator = dxp_md_path_separator;

    if (out_stream == NULL) {
        out_stream = stdout;
    }

    return DXP_SUCCESS;
}

/*
 * Routine to create pointers to the MD utility routines
 */
XIA_MD_EXPORT int XIA_MD_API dxp_md_init_io(Xia_Io_Functions* funcs, char* type)
/* Xia_Io_Functions *funcs;		*/
/* char *type;					*/
{
    unsigned int i;

    for (i = 0; i < strlen(type); i++) {

        type[i]= (char)tolower(type[i]);
    }


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

#ifndef EXCLUDE_PLX
    /* Technically, the communications protocol is 'PXI', though the
     * driver is a PLX driver, which is why there are two different names
     * used here.
     */
    if (STREQ(type, "pxi")) {
        funcs->dxp_md_io            = dxp_md_plx_io;
        funcs->dxp_md_initialize    = dxp_md_plx_initialize;
        funcs->dxp_md_open          = dxp_md_plx_open;
        funcs->dxp_md_close         = dxp_md_plx_close;
    }
#endif /* EXCLUDE_PLX */

#ifndef EXCLUDE_USB2
    if (STREQ(type, "usb2")) {
        funcs->dxp_md_io            = dxp_md_usb2_io;
        funcs->dxp_md_initialize    = dxp_md_usb2_initialize;
        funcs->dxp_md_open          = dxp_md_usb2_open;
        funcs->dxp_md_close         = dxp_md_usb2_close;
    }
#endif /* EXCLUDE_USB2 */

    funcs->dxp_md_get_maxblk = dxp_md_get_maxblk;
    funcs->dxp_md_set_maxblk = dxp_md_set_maxblk;

    return DXP_SUCCESS;
}


#ifndef EXCLUDE_EPP
/*
 * Initialize the system.
 */
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_initialize(unsigned int* maxMod, char* dllname)
/* unsigned int *maxMod;					Input: maximum number of dxp modules allowed */
/* char *dllname;							Input: name of the DLL						*/
{
    int status = DXP_SUCCESS;
    int rstat = 0;

    /* EPP initialization */

    /* check if all the memory was allocated */
    if (*maxMod>MAXMOD) {
        status = DXP_NOMEM;
        sprintf(ERROR_STRING,"Calling routine requests %d maximum modules: only %d available.",
                *maxMod, MAXMOD);
        dxp_md_log_error("dxp_md_epp_initialize",ERROR_STRING,status);
        return status;
    }

    /* Zero out the number of modules currently in the system */
    numEPP = 0;

    /* Initialize the EPP port */
    rstat = sscanf(dllname,"%hx",&port);			/* PLCF 4pi change, %x to %hx	*/
    if (rstat!=1) {
        status = DXP_NOMATCH;
        dxp_md_log_error("dxp_md_epp_initialize",
                         "Unable to read the EPP port address",status);
        return status;
    }

    sprintf(ERROR_STRING, "EPP Port = %#x", port);
    dxp_md_log_debug("dxp_md_epp_initialize", ERROR_STRING);

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
    sprintf(ERROR_STRING,
    "Unable to initialize the EPP port: rstat=%d",rstat);
    dxp_md_log_error("dxp_md_epp_initialize", ERROR_STRING,status);
    return status;
    }*/

    /* Reset the currentID when the EPP interface is initialized */
    currentID = -1;

    return status;
}
/*
 * Routine is passed the user defined configuration string *name.  This string
 * contains all the information needed to point to the proper IO channel by
 * future calls to dxp_md_io().
 */
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_open(char* ioname, int* camChan)
/* char *ioname;							Input:  string used to specify this IO
   channel */
/* int *camChan;						Output: returns a reference number for
   this module */
{
    unsigned int i;
    int status=DXP_SUCCESS;
    int rstat = 0;

    sprintf(ERROR_STRING, "ioname = %s", ioname);
    dxp_md_log_debug("dxp_md_epp_open", ERROR_STRING);

    /* First loop over the existing names to make sure this module
     * was not already configured?  Don't want/need to perform
     * this operation 2 times. */

    for(i=0; i<numEPP; i++)
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
        dxp_md_free(eppName[numEPP]);
    }
    eppName[numEPP] = (char *) dxp_md_alloc((strlen(ioname)+1)*sizeof(char));
    strcpy(eppName[numEPP],ioname);

    /* See if this is a multi-module EPP chain, if not set its ID to -1 */
    if (ioname[0] == ':')
    {
        sscanf(ioname, ":%d", &(eppID[numEPP]));

        sprintf(ERROR_STRING, "ID = %i", eppID[numEPP]);
        dxp_md_log_debug("dxp_md_epp_open", ERROR_STRING);

        /* Initialize the port address first */
        rstat = DxpInitPortAddress((int) port);
        if (rstat != 0)
        {
            status = DXP_INITIALIZE;
            sprintf(ERROR_STRING,
                    "Unable to initialize the EPP port address: port=%d", port);
            dxp_md_log_error("dxp_md_epp_open", ERROR_STRING, status);
            return status;
        }

        /* Call setID now to setup the port for Initialization */
        DxpSetID((unsigned short) eppID[numEPP]);
        /* No return value
           if (rstat != 0)
           {
           status = DXP_INITIALIZE;
           sprintf(ERROR_STRING,
           "Unable to set the EPP Port ID: ID=%d", id[numEPP]);
           dxp_md_log_error("dxp_md_epp_open", ERROR_STRING, status);
           return status;
           }*/
    } else {
        eppID[numEPP] = -1;
    }

    /* Call the EPPLIB routine */
    rstat = DxpInitEPP((int)port);

    /* Check for Success */
    if (rstat==0)
    {
        status = DXP_SUCCESS;
    } else {
        status = DXP_INITIALIZE;
        sprintf(ERROR_STRING,
                "Unable to initialize the EPP port: rstat=%d",rstat);
        dxp_md_log_error("dxp_md_epp_open", ERROR_STRING,status);
        return status;
    }

    *camChan = numEPP++;
    numMod++;

    return status;
}


/*
 * This routine performs the IO call to read or write data.  The pointer to
 * the desired IO channel is passed as camChan.  The address to write to is
 * specified by function and address.  The data length is specified by
 * length.  And the data itself is stored in data.
 */
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_io(int* camChan, unsigned int* function,
                                           unsigned long* address, void* data,
                                           unsigned int* length)
/* int *camChan;				Input: pointer to IO channel to access	*/
/* unsigned int *function;			Input: XIA EPP function definition	*/
/* unsigned long *address;			Input: XIA EPP address definition	*/
/* void *data;		                	I/O:  data read or written		*/
/* unsigned int *length;			Input: how much data to read or write	*/
{
    int rstat = 0;
    int status;

    int i;

    unsigned short *us_data = (unsigned short *)data;
    
    int ullength = (int) *length/2;
    unsigned long *temp = NULL;

    if ((currentID != eppID[*camChan]) && (eppID[*camChan] != -1))
    {
        DxpSetID((unsigned short) eppID[*camChan]);

        /* Update the currentID */
        currentID = eppID[*camChan];

        sprintf(ERROR_STRING, "calling SetID = %i, camChan = %i", eppID[*camChan], *camChan);
        dxp_md_log_debug("dxp_md_epp_io", ERROR_STRING);
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
            temp = (unsigned long *)dxp_md_alloc(sizeof(unsigned long) * ullength);
            
            if (!temp) {
                sprintf(ERROR_STRING, "Unable to allocate %zu bytes for temp",
                        sizeof(unsigned long) * ullength);
                dxp_md_log_error("dxp_md_epp_io", ERROR_STRING, DXP_MDNOMEM);
                return DXP_MDNOMEM;
            }
            
            if (*function == MD_IO_READ) {
                rstat = DxpReadBlocklong(next_addr, temp, ullength);
                /* reverse the byte order for the EPPLIB library */
                for (i = 0; i < ullength; i++) {
                    us_data[2*i] = (unsigned short) (temp[i]&0xFFFF);
                    us_data[2*i+1] = (unsigned short) ((temp[i]>>16)&0xFFFF);
                }
            } else {
                /* reverse the byte order for the EPPLIB library */
                for (i=0; i < ullength; i++) {
                    temp[i] = ((us_data[2*i]<<16) + us_data[2*i+1]);
                }
                rstat = DxpWriteBlocklong(next_addr, temp, ullength);
            }
            /* Free the memory */
            dxp_md_free(temp);
        }
        /* Address port*/
    } else if (*address==1) {
        next_addr = *us_data;
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
        sprintf(ERROR_STRING,"Unknown EPP address=%ld",*address);
        status = DXP_MDIO;
        dxp_md_log_error("dxp_md_epp_io",ERROR_STRING,status);
        return status;
    }

    if (rstat!=0) {
        status = DXP_MDIO;
        sprintf(ERROR_STRING,"Problem Performing I/O to Function: %d, address: %#lx",*function, *address);
        dxp_md_log_error("dxp_md_epp_io",ERROR_STRING,status);
        sprintf(ERROR_STRING,"Trying to write to internal address: %d, length %d",next_addr, *length);
        dxp_md_log_error("dxp_md_epp_io",ERROR_STRING,status);
        return status;
    }

    status=DXP_SUCCESS;

    return status;
}


/*
 * "Closes" the EPP connection, which means that it does nothing.
 */
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_close(int *camChan)
{
    UNUSED(camChan);

    return DXP_SUCCESS;
}

#endif /* EXCLUDE_EPP */

#ifndef EXCLUDE_USB
/*
 * Initialize the USB system.
 */


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
        sprintf(ERROR_STRING,"Calling routine requests %d maximum modules: only %d available.",
                *maxMod, MAXMOD);
        dxp_md_log_error("dxp_md_usb_initialize",ERROR_STRING,status);
        return status;
    }

    /* Zero out the number of modules currently in the system */
    numUSB = 0;

    return status;
}
/*
 * Routine is passed the user defined configuration string *name.  This string
 * contains all the information needed to point to the proper IO channel by
 * future calls to dxp_md_io().
 */
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_open(char* ioname, int* camChan)
/* char *ioname;				 Input:  string used to specify this IO channel */
/* int *camChan;				 Output: returns a reference number for this module */
{
    int status=DXP_SUCCESS;
    unsigned int i;

    /* Temporary name so that we can get the length */
    char tempName[200];

    sprintf(ERROR_STRING, "ioname = %s", ioname);
    dxp_md_log_debug("dxp_md_usb_open", ERROR_STRING);

    /* First loop over the existing names to make sure this module
     * was not already configured?  Don't want/need to perform
     * this operation 2 times. */

    for(i=0; i<numUSB; i++)
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
        dxp_md_free(usbName[numUSB]);
    }
    usbName[numUSB] = (char *) dxp_md_alloc((strlen(tempName)+1)*sizeof(char));
    strcpy(usbName[numUSB],tempName);

    /*  dxp_md_log_info("dxp_md_usb_open", "Attempting to open usb handel");
      if (xia_usb_open(usbName[numUSB], &usbHandle[numUSB]) != 0)
      {
      dxp_md_log_info("dxp_md_usb_open", "Unable to open usb handel");
      }
      sprintf(ERROR_STRING, "device = %s, handle = %i", usbName[numUSB], usbHandle[numUSB]);
      dxp_md_log_info("dxp_md_usb_open", ERROR_STRING);
    */
    *camChan = numUSB++;
    numMod++;

    return status;
}


/*
 * This routine performs the IO call to read or write data.  The pointer to
 * the desired IO channel is passed as camChan.  The address to write to is
 * specified by function and address.  The data length is specified by
 * length.  And the data itself is stored in data.
 */
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_io(int* camChan, unsigned int* function,
                                           unsigned long* address, void* data,
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
            rstat = xia_usb_read(usb_addr, (long) *length, usbName[*camChan], us_data);
        } else {
            rstat = xia_usb_write(usb_addr, (long) *length, usbName[*camChan], us_data);
        }
    } else if (*address ==1) {
        usb_addr = (long) us_data[0];
    }

    if (rstat != 0)
    {
        status = DXP_MDIO;
        sprintf(ERROR_STRING,"Problem Performing USB I/O to Function: %d, address: %ld",*function, *address);
        dxp_md_log_error("dxp_md_usb_io",ERROR_STRING,status);
        sprintf(ERROR_STRING,"Trying to write to internal address: %#hx, length %d", (int)usb_addr, *length);
        dxp_md_log_error("dxp_md_usb_io",ERROR_STRING,status);
        return status;
    }

    status=DXP_SUCCESS;

    return status;
}


/*
 * "Closes" the USB connection
 */
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_close(int *camChan)
{
    xia_usb_close(1);

    return DXP_SUCCESS;
}

#endif


#ifndef EXCLUDE_SERIAL

/*
 * Initialized any global structures.
 *
 * Only called once per library load.
 */
XIA_MD_STATIC int dxp_md_serial_initialize(unsigned int *maxMod,
                                           char *dllname)
{
    int i;

    UNUSED(maxMod);
    UNUSED(dllname);


    numSerial = 0;

    for (i = 0; i < MAXMOD; i++) {
        serialName[i] = NULL;
    }

    return DXP_SUCCESS;
}

/* Serial port initialization from https://stackoverflow.com/a/6947758/223029 */
int dxp_md_serial_set_interface_attribs(int fd, int speed)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        printf("Error from tcgetattr: %s\n", strerror(errno));
        return -1;
    }

    cfsetospeed(&tty, (speed_t)speed);
    cfsetispeed(&tty, (speed_t)speed);

    tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         /* 8-bit characters */
    tty.c_cflag &= ~PARENB;     /* no parity bit */
    tty.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */

    /* setup for non-canonical mode */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN] = 0;   /* Don't block if no bytes ever come */
    tty.c_cc[VTIME] = 10; /* Deciseconds to wait for the first byte */

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error from tcsetattr: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

/*
 * Initializes a new COM port instance.
 *
 * This routine is called each time that a COM port is opened.
 * ioname encodes COM port and baud rate as "[port]:[baud_rate]".
 */
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_open(char* ioname, int* camChan)
{
    int status;
    int len_ioname;

    unsigned int i;

    char device_file[1024]; // TODO: max path constant

    unsigned long baud;
    int baudConst;


    sprintf(ERROR_STRING, "Open serial device %s", ioname);
    dxp_md_log_info("dxp_md_serial_open", ERROR_STRING);

    /* Check for an existing definition. */
    for(i = 0; i < numSerial; i++) {
        if(STREQ(serialName[i], ioname)) {
            *camChan = i;
            return DXP_SUCCESS;
        }
    }

    /* Parse in the device file and baud rate. */
    int loc = strcspn(ioname, ":");
    if (loc == 0) {
        sprintf(ERROR_STRING, "Unable to find : in ioname %s", ioname);
        dxp_md_log_error("dxp_md_serial_open", ERROR_STRING, DXP_BAD_PARAM);
        return DXP_BAD_PARAM;
    }

    strncpy(device_file, ioname, loc);
    device_file[loc] = '\0';

    sscanf(ioname + loc, ":%lu", &baud);

    *camChan = numSerial++;

    int fd = open(device_file, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        sprintf(ERROR_STRING, "Error opening %s, where the driver reports "
                "%s", device_file, strerror(errno));
        dxp_md_log_error("dxp_md_serial_open", ERROR_STRING, DXP_MDOPEN);
        return DXP_MDOPEN;
    }

    /* Convert baud to corresponding baud constant */
    switch (baud)
    {
    case 50: baudConst = B50; break;
    case 75: baudConst = B75; break;
    case 110: baudConst = B110; break;
    case 134: baudConst = B134; break;
    case 150: baudConst = B150; break;
    case 200: baudConst = B200; break;
    case 300: baudConst = B300; break;
    case 600: baudConst = B600; break;
    case 1200: baudConst = B1200; break;
    case 1800: baudConst = B1800; break;
    case 2400: baudConst = B2400; break;
    case 4800: baudConst = B4800; break;
    case 9600: baudConst = B9600; break;
    case 19200: baudConst = B19200; break;
    case 38400: baudConst = B38400; break;
    case 57600: baudConst = B57600; break;
    case 115200: baudConst = B115200; break;
    case 230400: baudConst = B230400; break;
    default:
        close(fd);
        sprintf(ERROR_STRING, "device %s unknown baud %lu", device_file, baud);
        dxp_md_log_error("dxp_md_serial_open", ERROR_STRING, DXP_BAD_PARAM);
        return DXP_BAD_PARAM;
    }

    status = dxp_md_serial_set_interface_attribs(fd, baudConst);
    if (status != 0) {
        close(fd);
        sprintf(ERROR_STRING, "Error configuring %s, where the driver reports "
                "%d", device_file, status);
        dxp_md_log_error("dxp_md_serial_open", ERROR_STRING, DXP_MDINITIALIZE);
        return DXP_MDINITIALIZE;
    }

    serialHandles[*camChan] = fd;

    ASSERT(serialName[*camChan] == NULL);

    len_ioname = strlen(ioname) + 1;

    serialName[*camChan] = dxp_md_alloc(len_ioname);

    if (serialName[*camChan] == NULL) {
        sprintf(ERROR_STRING, "Unable to allocate %d bytes for serialName[%d]",
                len_ioname, *camChan);
        dxp_md_log_error("dxp_md_serial_open", ERROR_STRING, DXP_MDNOMEM);
        return DXP_MDNOMEM;
    }

    strcpy(serialName[*camChan], ioname);

    numMod++;

    return DXP_SUCCESS;
}


/*
 * This routine performs the read/write operations
 * on the serial port.
 */
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_io(int *camChan,
                                              unsigned int *function,
                                              unsigned long *wait_in_ms,
                                              void *data,
                                              unsigned int *length)
{
    int status;
    unsigned int i;

    unsigned short n_bytes = 0;

    unsigned short *us_data = (unsigned short *)data;

    byte_t *buf    = NULL;

    int fd = serialHandles[*camChan];

    UNUSED(wait_in_ms);

    if (*function == MD_IO_READ) {
        status = dxp_md_serial_read_header(fd, &n_bytes, us_data);

        if (status != DXP_SUCCESS) {
            dxp_md_log_error("dxp_md_serial_io", "Error reading header", status);
            return status;
        }

        /* Check the incoming data packet length against the size of the
         * user's buffer.
         */
        if (n_bytes + HEADER_SIZE > *length) {
            tcflush(fd, TCIOFLUSH);
            sprintf(ERROR_STRING, "Header reports ndata=%hu, larger than "
                    "requested length %u-%d.", n_bytes, *length, HEADER_SIZE);
            dxp_md_log_error("dxp_md_serial_io", ERROR_STRING, DXP_MDSIZE);
            return DXP_MDSIZE;
        }

        status = dxp_md_serial_read_data(fd, n_bytes, us_data + HEADER_SIZE);

        if (status != DXP_SUCCESS) {
            dxp_md_log_error("dxp_md_serial_io", "Error reading data", status);
            return status;
        }
    } else if (*function == MD_IO_WRITE) {
        status = tcflush(fd, TCIOFLUSH);
        if (status != 0) {
            sprintf(ERROR_STRING, "Error flushing fd=%d, driver reports %s",
                    fd, strerror(errno));
            dxp_md_log_error("dxp_md_serial_open", ERROR_STRING, DXP_MDIO);
            return DXP_MDIO;
        }

        buf = (byte_t *)dxp_md_alloc(*length * sizeof(byte_t));

        if (buf == NULL) {
            status = DXP_NOMEM;
            sprintf(ERROR_STRING, "Error allocating %u bytes for buf",
                    *length);
            dxp_md_log_error("dxp_md_serial_io", ERROR_STRING, status);
            return status;
        }

        for (i = 0; i < *length; i++) {
            buf[i] = (byte_t)us_data[i];
        }

        int wlen = write(fd, buf, *length);

        if (wlen != *length) {
            dxp_md_free((void *)buf);
            buf = NULL;

            sprintf(ERROR_STRING, "Error writing %u bytes to fd %d"
                    "driver reported status %d", *length, fd, wlen);
            dxp_md_log_error("dxp_md_serial_io", ERROR_STRING, DXP_MDIO);
            return DXP_MDIO;
        }

        dxp_md_free((void *)buf);
        buf = NULL;

        status = tcdrain(fd);
        if (status != 0) {
            sprintf(ERROR_STRING, "Error draining fd=%d, driver reports %s",
                    fd, strerror(errno));
            dxp_md_log_error("dxp_md_serial_open", ERROR_STRING, DXP_MDIO);
            return DXP_MDIO;
        }

    } else if (*function == MD_IO_OPEN) {
        /* Do nothing */
    } else if (*function == MD_IO_CLOSE) {
        /* Do nothing */
    } else {
        status = DXP_MDUNKNOWN;
        sprintf(ERROR_STRING, "Unknown function: %u", *function);
        dxp_md_log_error("dxp_md_serial_io", ERROR_STRING, status);
        return status;
    }

    return DXP_SUCCESS;
}

/*
 * Reads the header of the returned packet
 *
 * Reads the header of the returned packet and puts it into the user
 * specified buffer. Also calculates the number of bytes remaining
 * in the packet including the XOR checksum.
 */
static int dxp_md_serial_read_header(int fd, unsigned short *bytes,
                                     unsigned short *buf)
{
    int i;

    byte_t lo;
    byte_t hi;
    byte_t b;

    byte_t header[HEADER_SIZE];


    ASSERT(bytes != NULL);
    ASSERT(buf != NULL);

    for (i = 0; i < HEADER_SIZE; i++) {
        int rlen = read(fd, &b, 1);

        if (rlen != 1) {
            sprintf(ERROR_STRING, "Error reading header from fd=%d: actual = %d, "
                    "expected = %d", fd, rlen, 1);
            dxp_md_log_error("dxp_md_serial_read_header", ERROR_STRING, DXP_MDIO);
            return DXP_MDIO;
        }

        header[i] = b;
    }

    lo = header[2];
    hi = header[3];

    /* Include the XOR checksum in this calculation */
    *bytes = (unsigned short)(((unsigned short)lo |
                               ((unsigned short)hi << 8)) + 1);

    if (*bytes == 1) {
        dxp_md_log_debug("dxp_md_serial_read_header",
                         "Number of data bytes = 1 in header");
        for (i = 0; i < HEADER_SIZE; i++) {
            sprintf(ERROR_STRING, "header[%d] = %#hx", i, (unsigned short)header[i]);
            dxp_md_log_debug("dxp_md_serial_read_header",
                             ERROR_STRING);
        }
    }

    /* Copy header into data buffer. Can't use memcpy() since we are
     * going from byte_t -> unsigned short.
     */
    for (i = 0; i < HEADER_SIZE; i++) {
        buf[i] = (unsigned short)header[i];
    }

    return DXP_SUCCESS;
}


/*
 * Reads the specified number of bytes from the port and copies them to
 *  the buffer.
 */
static int dxp_md_serial_read_data(int fd,
                                   unsigned long size, unsigned short *buf)
{
    byte_t *b = NULL;
    int timeToStall = 5;
    int totalRead;
    unsigned long i;

    ASSERT(buf != NULL);

    b = (byte_t *)dxp_md_alloc(size * sizeof(byte_t));

    if (b == NULL) {
        sprintf(ERROR_STRING, "Error allocating %lu bytes for 'b'", size);
        dxp_md_log_error("dxp_md_serial_read_data", ERROR_STRING, DXP_NOMEM);
        return DXP_NOMEM;
    }

    /* VMIN limits read size to 255. We just loop until it we get all the
     * requested data or stall after several failures.
     */
    for (totalRead = 0; totalRead < size; ) {
        int remaining = size - totalRead;

        int rlen = read(fd, b + totalRead, remaining);
        if (rlen <= 0) {
            if (timeToStall-- > 0)
                continue;

            dxp_md_free(b);
            sprintf(ERROR_STRING, "Error reading data from fd=%d: "
                    "requested = %d, returned = %d", fd, remaining, rlen);
            dxp_md_log_error("dxp_md_serial_read_data", ERROR_STRING, DXP_MDIO);

            /* The Windows driver closes the port and reinitializes it on failure
             * here. Do we need it?
             */

            return DXP_MDIO;
        }

        totalRead += rlen;
    }

    for (i = 0; i < size; i++) {
        buf[i] = (unsigned short)b[i];
    }

    dxp_md_free(b);

    return DXP_SUCCESS;
}


/*
 * Closes the serial port connection so that we don't
 * crash due to interrupt vectors left lying around.
 */
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_close(int *camChan)
{
    ASSERT(camChan != NULL);

    sprintf(ERROR_STRING, "Called with camChan %d", *camChan);
    dxp_md_log_debug("dxp_md_serial_close", ERROR_STRING);

    if (serialName[*camChan] != NULL) {
        int fd = serialHandles[*camChan];

        sprintf(ERROR_STRING, "Preparing to close camChan %d, device %s, fd %d",
                *camChan, serialName[*camChan], fd);
        dxp_md_log_debug("dxp_md_serial_close", ERROR_STRING);

        close(fd);

        dxp_md_free(serialName[*camChan]);
        serialName[*camChan] = NULL;

        numSerial--;
    }

    return DXP_SUCCESS;
}


#endif /* EXCLUDE_SERIAL */


#ifndef EXCLUDE_USB2

/* We need separate functions here, but we can use the same low-level calls as USB 1.0? */

/*
 * Perform any one-time initialization tasks for the USB2 driver.
 */
XIA_MD_STATIC int  dxp_md_usb2_initialize(unsigned int *maxMod,
                                          char *dllname)
{
    UNUSED(maxMod);
    UNUSED(dllname);


    numUSB2 = 0;

    return DXP_SUCCESS;
}


/*
 * Open the requested USB2 device.
 */
XIA_MD_STATIC int  dxp_md_usb2_open(char *ioname, int *camChan)
{
    int i;
    int dev;
    int status;
    int len;


    ASSERT(ioname != NULL);
    ASSERT(camChan != NULL);


    for (i = 0; i < (int)numUSB2; i++) {
        if (STREQ(usb2Names[i], ioname)) {
            *camChan = i;
            return DXP_SUCCESS;
        }
    }

    sscanf(ioname, "%d", &dev);

    *camChan = numUSB2++;

    status = xia_usb2_open(dev, &usb2Handles[*camChan]);

    if (status != XIA_USB2_SUCCESS) {
        sprintf(ERROR_STRING, "Error opening USB device '%d', where the driver "
                "reports a status of %d", dev, status);
        dxp_md_log_error("dxp_md_usb2_open", ERROR_STRING, DXP_MDOPEN);
        return DXP_MDOPEN;
    }

    ASSERT(usb2Names[*camChan] == NULL);

    len = strlen(ioname) + 1;

    usb2Names[*camChan] = dxp_md_alloc(len);

    if (usb2Names[*camChan] == NULL) {
        sprintf(ERROR_STRING, "Unable to allocate %d bytes for usb2Names[%d]",
                len, *camChan);
        dxp_md_log_error("dxp_md_usb2_open", ERROR_STRING, DXP_MDNOMEM);
        return DXP_MDNOMEM;
    }

    strcpy(usb2Names[*camChan], ioname);

    usb2AddrCache[*camChan] = MD_INVALID_ADDR;

    numMod++;

    return DXP_SUCCESS;
}


/*
 * Wraps both read/write USB2 operations.
 *
 * The I/O type (read/write) is specified in function. The allowed types
 * are the same as the USB driver so that we can use USB2 and USB interchangably
 * as communication protocols.
 */
XIA_MD_STATIC int  dxp_md_usb2_io(int *camChan, unsigned int *function,
                                  unsigned long *addr, void *data,
                                  unsigned int *len)
{
    int status;

    unsigned int i;

    unsigned long cache_addr;

    unsigned short *buf = NULL;

    byte_t *byte_buf = NULL;

    unsigned long n_bytes = 0;
    unsigned long n_bytes_read = 0;


    ASSERT(addr != NULL);
    ASSERT(function != NULL);
    ASSERT(data != NULL);
    ASSERT(camChan != NULL);
    ASSERT(len != NULL);

    buf = (unsigned short *)data;

    n_bytes = (unsigned long)(*len) * 2;

    /* Unlike some of our other communication types, we require that the
     * target address be set as a separate operation. This value will be saved
     * until the real I/O is requested. In a perfect world, we wouldn't require
     * this type of operation anymore, but we have to maintain backwards
     * compatibility for products that want to use USB2 but originally used
     * other protocols that did require the address be set separately.
     */
    switch (*addr) {

    case 0:
        if (usb2AddrCache[*camChan] == MD_INVALID_ADDR) {
            sprintf(ERROR_STRING, "No target address set for camChan %d", *camChan);
            dxp_md_log_error("dxp_md_usb2_io", ERROR_STRING, DXP_MD_TARGET_ADDR);
            return DXP_MD_TARGET_ADDR;
        }

        cache_addr = (unsigned long)usb2AddrCache[*camChan];

        switch (*function) {
        case MD_IO_READ:

            /* The data comes from the calling routine as an unsigned short, so
             * we need to convert it to a byte array for the USB2 driver.
             */
            byte_buf = dxp_md_alloc(n_bytes);

            if (byte_buf == NULL) {
                sprintf(ERROR_STRING, "Error allocating %lu bytes for 'byte_buf' for "
                        "camChan %d", n_bytes, *camChan);
                dxp_md_log_error("dxp_md_usb2_io", ERROR_STRING, DXP_MDNOMEM);
                return DXP_MDNOMEM;
            }
            status = xia_usb2_readn(usb2Handles[*camChan], cache_addr, n_bytes,
                                    byte_buf, &n_bytes_read);
            if (status != 0) {
                dxp_md_free(byte_buf);
                sprintf(ERROR_STRING, "Error reading %lu bytes from %#lx for "
                        "camChan %d, driver reports %d",
                        n_bytes, cache_addr, *camChan, status);
                dxp_md_log_error("dxp_md_usb2_io", ERROR_STRING, DXP_MDIO);
                return DXP_MDIO;
            }

            if (n_bytes_read != n_bytes) {
                sprintf(ERROR_STRING, "Reading %lu bytes from %#lx for "
                        "camChan %d, got %lu",
                        n_bytes, cache_addr, *camChan, n_bytes_read);
                dxp_md_log_error("dxp_md_usb2_io", ERROR_STRING, DXP_MDIO);
                return DXP_MDIO;
            }

            for (i = 0; i < *len; i++) {
                buf[i] = (unsigned short)(byte_buf[i * 2] |
                                          (byte_buf[(i * 2) + 1] << 8));
            }

            dxp_md_free(byte_buf);

            break;

        case MD_IO_WRITE:
            /* The data comes from the calling routine as an unsigned short, so
             * we need to convert it to a byte array for the USB2 driver.
             */
            byte_buf = dxp_md_alloc(n_bytes);

            if (byte_buf == NULL) {
                sprintf(ERROR_STRING, "Error allocating %lu bytes for 'byte_buf' for "
                        "camChan %d", n_bytes, *camChan);
                dxp_md_log_error("dxp_md_usb2_io", ERROR_STRING, DXP_MDNOMEM);
                return DXP_MDNOMEM;
            }

            for (i = 0; i < *len; i++) {
                byte_buf[i * 2]       = (byte_t)(buf[i] & 0xFF);
                byte_buf[(i * 2) + 1] = (byte_t)((buf[i] >> 8) & 0xFF);
            }

            status = xia_usb2_write(usb2Handles[*camChan], cache_addr, n_bytes,
                                    byte_buf);

            dxp_md_free(byte_buf);

            if (status != XIA_USB2_SUCCESS) {
                sprintf(ERROR_STRING, "Error writing %lu bytes to %#lx for "
                        "camChan %d", n_bytes, cache_addr, *camChan);
                dxp_md_log_error("dxp_md_usb2_io", ERROR_STRING, DXP_MDIO);
                return DXP_MDIO;
            }

            break;

        default:
            ASSERT(FALSE_);
            break;
        }

        break;

    case 1:
        /* Even though we aren't entirely thread-safe yet, it doesn't hurt to
         * store the address as a function of camChan, instead of as a global
         * variable like dxp_md_usb_io().
         */
        usb2AddrCache[*camChan] = *((unsigned long *)data);
        break;

    default:
        ASSERT(FALSE_);
        break;
    }

    return DXP_SUCCESS;
}

/*
 * Closes a device previously opened with dxp_md_usb2_open().
 */
XIA_MD_STATIC int  dxp_md_usb2_close(int *camChan)
{
    int status;

    HANDLE h;


    ASSERT(camChan != NULL);


    h = usb2Handles[*camChan];

    if (h == (HANDLE)NULL) {
        sprintf(ERROR_STRING, "Skipping previously closed camChan = %d", *camChan);
        dxp_md_log_info("dxp_md_usb2_close", ERROR_STRING);
        return DXP_SUCCESS;
    }

    status = xia_usb2_close(h);

    if (status != XIA_USB2_SUCCESS) {
        sprintf(ERROR_STRING, "Error closing camChan (%d) with HANDLE = %#lx",
                *camChan, h);
        dxp_md_log_error("dxp_md_usb2_close", ERROR_STRING, DXP_MDCLOSE);
        return DXP_MDCLOSE;
    }

    usb2Handles[*camChan] = (HANDLE)NULL;

    ASSERT(usb2Names[*camChan] != NULL);

    dxp_md_free(usb2Names[*camChan]);
    usb2Names[*camChan] = NULL;

    numUSB2--;

    return DXP_SUCCESS;
}


#endif /* EXCLUDE_USB2 */


/*
 * Routine to get the maximum number of words that can be block transfered at
 * once.  This can change from system to system and from controller to
 * controller.  A maxblk size of 0 means to write all data at once, regardless
 * of transfer size.
 */
XIA_MD_STATIC int XIA_MD_API dxp_md_get_maxblk(void)
{

    return maxblk;

}

/*
 * Routine to set the maximum number of words that can be block transfered at
 * once.  This can change from system to system and from controller to
 * controller.  A maxblk size of 0 means to write all data at once, regardless
 * of transfer size.
 */
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

/*
 * Routine to wait a specified time in seconds.  This allows the user to call
 * routines that are as precise as required for the purpose at hand.
 */
XIA_MD_STATIC int XIA_MD_API dxp_md_wait(float* time)
/* float *time;							Input: Time to wait in seconds	*/
{
    DWORD wait = (DWORD)(1000.0 * (*time));

    Sleep(wait);

    return DXP_SUCCESS;
}

/*
 * Routine to allocate memory.  The calling structure is the same as the
 * ANSI C standard routine malloc().
 */
XIA_MD_SHARED void* XIA_MD_API dxp_md_alloc(size_t length)
/* size_t length;							Input: length of the memory to allocate
   in units of size_t (defined to be a
   byte on most systems) */
{
    return malloc(length);

}

/*
 * Routine to free memory.  Same calling structure as the ANSI C routine
 * free().
 */
XIA_MD_SHARED void XIA_MD_API dxp_md_free(void* array)
/* void *array;							Input: pointer to the memory to free */
{
    free(array);
}

/*
 * Routine to print a string to the screen or log.  No direct calls to
 * printf() are performed by XIA libraries.  All output is directed either
 * through the dxp_md_error() or dxp_md_puts() routines.
 */
XIA_MD_STATIC int XIA_MD_API dxp_md_puts(char* s)
/* char *s;								Input: string to print or log	*/
{

    return printf("%s", s);

}


/*
 * Safe version of fgets() that can handle both UNIX and DOS
 * line-endings.
 *
 * If the trailing two characters are '\\r' + '\\n', they are replaced by a
 * single '\\n'.
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


/*
 * Get a safe temporary directory path.
 */
XIA_MD_STATIC char * dxp_md_tmp_path(void)
{
    return TMP_PATH;
}


/*
 * Clears the temporary path cache.
 */
XIA_MD_STATIC void dxp_md_clear_tmp(void)
{
    return;
}


/*
 * Returns the path separator
 */
XIA_MD_STATIC char * dxp_md_path_separator(void)
{
    return PATH_SEP;
}


/*
 * Sets the priority of the current process
 */
XIA_MD_STATIC int XIA_MD_API dxp_md_set_priority(int *priority)
{
    BOOL status;

    DWORD pri;

    unsigned int h;

    char pri_str[5];


    h = GetCurrentProcess();

    switch(*priority) {

    default:
        sprintf(ERROR_STRING, "Invalid priority type: %#x", *priority);
        dxp_md_log_error("dxp_md_set_priority", ERROR_STRING, DXP_MDINVALIDPRIORITY);
        return DXP_MDINVALIDPRIORITY;
        break;

    case MD_IO_PRI_NORMAL:
        pri = NORMAL_PRIORITY_CLASS;
        strncpy(pri_str, "NORM", 5);
        break;

    case MD_IO_PRI_HIGH:
        pri = REALTIME_PRIORITY_CLASS;
        strncpy(pri_str, "HIGH", 5);
        break;
    }

    status = SetPriorityClass(h, pri);

    if (!status) {
        sprintf(ERROR_STRING,
                "Error setting priority class (%s) for current process",
                pri_str);
        dxp_md_log_error("dxp_md_set_priority", ERROR_STRING, DXP_MDPRIORITY);
        return DXP_MDPRIORITY;
    }

    sprintf(ERROR_STRING, "Priority class set to '%s' for current process",
            pri_str);
    dxp_md_log_debug("dxp_md_set_priority", ERROR_STRING);

    return DXP_SUCCESS;
}
