/*****************************************************************************
 *
 *  md_linux.c
 *
 *  Created 08/12/03 -- PJF
 *
 * Copyright (c) 2003, X-ray Instrumentation Associates
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

/* XIA include files */
#include "xerxes_errors.h"
#include "xerxes_structures.h"
#include "md_linux.h"
#include "md_generic.h"
#include "xia_md.h"
#include "xia_common.h"


/* total number of modules in the system */
static unsigned int numMod    = 0;

/* are we in debug mode? */
static int print_debug=0;

/* maximum number of words able to transfer in a single call to dxp_md_io() */
static unsigned int maxblk=0;


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

    if (out_stream == NULL)
    {
	out_stream = stdout;
    }

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
XIA_MD_STATIC int XIA_MD_API dxp_md_initialize(unsigned int* maxMod, char* dllname)
{
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
  UNUSED(ioname);

  /* Try not to abuse this too much, as I am just using this
   * code to test the compilation. See my note in the top-level
   * comment header.
   */
   *camChan = numDXP++;

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
XIA_MD_STATIC int XIA_MD_API dxp_md_io(int* camChan, unsigned int* function, 
				       unsigned int* address, unsigned short* data,
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
  UNUSED(maxMod);
  UNUSED(dllname);

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
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_open(char* ioname, int* camChan)
{
  UNUSED(ioname);
  
    *camChan = numEPP++;

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
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_io(int* camChan, unsigned int* function, 
					   unsigned int* address, unsigned short* data,
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
{
	UNUSED(dllname);
	UNUSED(maxMod);

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
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_open(char* ioname, int* camChan)
{
  UNUSED(ioname);

    *camChan = numUSB++;

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
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_io(int* camChan, unsigned int* function, 
					   unsigned int* address, unsigned short* data,
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
					      unsigned int* address, unsigned short* data,
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
  /* NOTE: Linux users need to implement this */

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
