/*****************************************************************************
 *
 *  md_epics.c
 *
 *  Created July 2003 Mark Rivers
 *
 */


/* System include files */
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <string.h>

/* EPICS include files */
#include <epicsThread.h>

/* XIA include files */
#include "xerxes_errors.h"
#include "xerxes_structures.h"
#include "md_epics.h"
#include "md_generic.h"
#include "xia_md.h"
/* #include "seriallib.h" */
#include "xia_common.h"

#define MODE 4


#define SERIAL_WAIT_TIMEOUT  ((unsigned long)500000)

/* total number of modules in the system */
static unsigned int numMod    = 0;

/* error string used as a place holder for calls to dxp_md_error() */
static char error_string[132];

/* are we in debug mode? */
static int print_debug=0;

/* maximum number of words able to transfer in a single call to dxp_md_io() */
static unsigned int maxblk=0;


#if !(defined(EXCLUDE_DXP4C) && defined(EXCLUDE_DXP4C2X))
#include <camacLib.h>
static unsigned int numDXP    = 0;
/* variables to store the IO channel information, later accessed via a pointer*/
static int *branch=NULL,*crate=NULL,*station=NULL;
#endif

#if !(defined(EXCLUDE_DXPX10P) && defined(EXCLUDE_DGF200))
/* EPP definitions */
/* The id variable stores an optional ID number associated with each module 
 * (initially included for handling multiple EPP modules hanging off the same
 * EPP address)
 */
/* #include <asm/io.h> */
#include <sys/io.h>
#define IO_inb inb
#define IO_outb outb
static unsigned int numEPP    = 0;
static int eppID[MAXMOD];
/* variables to store the IO channel information */
static char *eppName[MAXMOD];
/* eppPort stores the port number for each module, only used for the X10P/G200 */
static unsigned short eppPort;

static unsigned short next_addr=0;
#endif /* EXCLUDE_EPP Boards */

#ifndef EXCLUDE_USB
/* USB definitions */
/* The id variable stores an optional ID number associated with each module 
 */
static unsigned int numUSB    = 0;
static int usbID[MAXMOD];
/* variables to store the IO channel information */
static char *usbName[MAXMOD];
/* variables to store the USB Device Name */
static char *usbDevice[MAXMOD];

static HANDLE usbHandle[MAXMOD];

static long usb_addr=0;
#endif /* EXCLUDE_USB IO */

#if !(defined(EXCLUDE_UDXP) && defined(EXCLUDE_UDXPS))
/* variables to store the IO channel information */
static unsigned int numSerial = 0;
static char *serialName[MAXMOD];
/* Serial port globals */
static unsigned short comPort = 0;
#endif /* EXCLUDE_UDXP */

#ifdef USE_SERIAL_DLPORTIO
static void dxp_md_poll_and_read(unsigned short port, unsigned long nBytes, unsigned char *buf);
static void dxp_md_serial_write(unsigned short port, unsigned long nBytes, unsigned char *buf);
static void dxp_md_clear_port(unsigned short port);
#endif /* USE_SERIAL_DLPORTIO */


/******************************************************************************
 *
 * Routine to create pointers to the MD utility routines
 * 
 ******************************************************************************/
XIA_MD_EXPORT int XIA_MD_API dxp_md_init_util(Xia_Util_Functions* funcs, char* type)
/* Xia_Util_Functions *funcs;	*/
/* char *type;					*/
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
/* Xia_Io_Functions *funcs;		*/
/* char *type;					*/
{
    unsigned int i;
	
    for (i = 0; i < strlen(type); i++) {

	type[i]= (char)tolower(type[i]);
    }


#if !(defined(EXCLUDE_DXP4C) && defined(EXCLUDE_DXP4C2X))
    if (STREQ(type, "camac")) {

	funcs->dxp_md_io         = dxp_md_io;
	funcs->dxp_md_initialize = dxp_md_initialize;
	funcs->dxp_md_open       = dxp_md_open;
	funcs->dxp_md_close      = dxp_md_close;
    } 
#endif

#if !(defined(EXCLUDE_DXPX10P) && defined(EXCLUDE_DGF200))
    if (STREQ(type, "epp")) {

	funcs->dxp_md_io         = dxp_md_epp_io;
	funcs->dxp_md_initialize = dxp_md_epp_initialize;
	funcs->dxp_md_open       = dxp_md_epp_open;
	funcs->dxp_md_close      = dxp_md_epp_close;
    }
#endif

#ifndef EXCLUDE_USB
    if (STREQ(type, "usb")) 
	  {
		funcs->dxp_md_io            = dxp_md_usb_io;
		funcs->dxp_md_initialize    = dxp_md_usb_initialize;
		funcs->dxp_md_open          = dxp_md_usb_open;
		funcs->dxp_md_close         = dxp_md_usb_close;
	  }
#endif /* EXCLUDE_USB */

#ifndef EXCLUDE_UDXP
    if (STREQ(type, "serial")) 
	  {
		funcs->dxp_md_io            = dxp_md_serial_io;
		funcs->dxp_md_initialize    = dxp_md_serial_initialize;
		funcs->dxp_md_open          = dxp_md_serial_open;
		funcs->dxp_md_close         = dxp_md_serial_close;
	  }
#endif /* EXCLUDE_UDXP */


    funcs->dxp_md_get_maxblk = dxp_md_get_maxblk;
    funcs->dxp_md_set_maxblk = dxp_md_set_maxblk;
    funcs->dxp_md_lock_resource = NULL;


    return DXP_SUCCESS;
}

#if !(defined(EXCLUDE_DXP4C) && defined(EXCLUDE_DXP4C2X))
/*****************************************************************************
 * 
 * Initialize the system.  Alloocate the space for the library arrays, define
 * the pointer to the CAMAC library and the IO routine.
 * 
 *****************************************************************************/
static int dxp_md_initialize(unsigned int* maxMod, char* dllname)
/* unsigned int *maxMod;        Input: maximum number of dxp modules allowed */
/* char *dllname;               Input: name of the DLL                    */
{
  int status = DXP_SUCCESS;

/* Ensure that the library arrays do not exist */
  if (branch!=NULL) {
    dxp_md_free(branch);
  }
  if (crate!=NULL) {
    dxp_md_free(crate);
  }
  if (station!=NULL) {
    dxp_md_free(station);
  }

/* Allocate memory for the library arrays */
  branch  = (int *) dxp_md_alloc(*maxMod * sizeof(int));
  crate   = (int *) dxp_md_alloc(*maxMod * sizeof(int));
  station = (int *) dxp_md_alloc(*maxMod * sizeof(int));

/* check if all the memory was allocated */
  if ((branch==NULL)||(crate==NULL)||(station==NULL)) {
    status = DXP_NOMEM;
    dxp_md_log_error("dxp_md_initialize",
                 "Unable to allocate branch, crate and station memory", status);
    return status;
  }

/* Zero out the number of modules currently in the system */
  numDXP = 0;

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
static int dxp_md_open(char* name, int* camChan)
/* char *name;                        Input:  string used to specify this IO
                                                                     channel */
/* int *camChan;                      Output: returns a reference number for
                                                                  this module */
{
  int branch,crate,station,status;

/* pull out the branch, crate and station number and call the internal routine*/

  sscanf(name,"%1d%1d%2d",&branch,&crate,&station);
  status = dxp_md_open_bcn(&branch,&crate,&station,camChan);

  return status;
}

/*****************************************************************************
 *
 * Internal routine to assign a new pointer to this branch, crate and station
 * combination.  Information is stored in global static arrays.
 *
 *****************************************************************************/
static int dxp_md_open_bcn(int* new_branch, int* new_crate, int* new_station,
                                                   int* camChan)
/* int *new_branch;         Input: branch number                 */
/* int *new_crate;          Input: crate number                  */
/* int *new_station;        Input: station number                */
/* int *camChan;            Output: pointer to this bra and ch and crate */

{
  unsigned int i;
  int status;

/* First loop over the existing branch, crate and station numbers to make
 * sure this module was not already configured?  Don't want/need to perform
 * this operation 2 times. */

  for(i=0;i<numDXP;i++){
    if( (*new_branch==branch[i])&&
        (*new_crate==crate[i])&&
        (*new_station==station[i])){
/*                 sprintf(error_string,
                   " branch = %d  crate = %d   station = %d already defined",
                   *new_branch,*new_crate,*new_station);
                   dxp_md_puts("dxp_md_open",error_string,&status);*/
      status=DXP_SUCCESS;
      *camChan = i;
      return status;
    }
  }

/* Got a new one.  Increase the number existing and assign the global
 * information */

  branch[numDXP]=*new_branch;
  crate[numDXP]=*new_crate;
  station[numDXP]=*new_station;

  *camChan = numDXP++;


  status=DXP_SUCCESS;

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
static int dxp_md_io(int* camChan, unsigned int* function,
                                   unsigned int* address, unsigned short* data,
                                   unsigned int* length)
/* int *camChan;             Input: pointer to IO channel to access          */
/* unsigned int *function;   Input: function number to access (CAMAC F)      */
/* unsigned int *address;    Input: address to access        (CAMAC A)       */
/* unsigned short *data;     I/O:  data read or written                      */
/* unsigned int *length;     Input: how much data to read or write           */
{

  int ext;
  int cb[4];
  int status;
  int q;

/* First encode the transfer information in an array using
 * an SJY routine. */
  cdreg(&ext, branch[*camChan], crate[*camChan],
        station[*camChan], (int) *address);

  if (*length == 1) {
    cssa(*function, ext, (short *) data, &q);
    if (q==0) {
      status = DXP_MDIO;
      dxp_md_log_error("dxp_md_io","cssa transfer error: Q=0", status);
      return status;
    }
  } else {
    cb[0] = *length;
    cb[1] = 0;
    cb[2] = 0;
    cb[3] = 0;

    csubc(*function, ext, (short *) data, cb);

    if (cb[1]!=*length){
      status = DXP_MDIO;
      sprintf(error_string,
      "camac transfer error: bytes transferred %d vs expected %d",cb[1],cb[0]);
      dxp_md_log_error("dxp_md_io",error_string,status);
      return status;
    }
  }

  status=DXP_SUCCESS;
  return status;
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

#endif


#if !(defined(EXCLUDE_DXPX10P) && defined(EXCLUDE_DGF200))
/*****************************************************************************
 * 
 * Initialize the system.  Alloocate the space for the library arrays, define
 * the pointer to the CAMAC library and the IO routine.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_initialize(unsigned int* maxMod, char* dllname)
/* unsigned int *maxMod;					Input: maximum number of dxp modules allowed */
/* char *dllname;							Input: name of the DLL						*/
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
    rstat = sscanf(dllname,"%hx",&eppPort);
    if (rstat!=1) {
	status = DXP_NOMATCH;
	dxp_md_log_error("dxp_md_epp_initialize",
			 "Unable to read the EPP port address",status);
	return status;
    }
													 
    sprintf(error_string, "EPP Port = %#x", eppPort);
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
/* char *ioname;							Input:  string used to specify this IO 
   channel */
/* int *camChan;						Output: returns a reference number for
   this module */
{
    unsigned int i;
    int status=DXP_SUCCESS;
    int rstat = 0;

	
    sprintf(error_string, "ioname = %s", ioname);
    dxp_md_log_debug("dxp_md_epp_open", error_string);

    /* First loop over the existing names to make sure this module 
     * was not already configured?  Don't want/need to perform
     * this operation 2 times. */
    
	for(i=0;i<numEPP;i++){
	if(STREQ(eppName[i],ioname)) {
	    status=DXP_SUCCESS;
	    *camChan = i;
	    return status;
	}
    }

    /* Got a new one.  Increase the number existing and assign the global 
     * information */

    if (eppName[numEPP]!=NULL) {
	dxp_md_free(eppName[numEPP]);
    }
    eppName[numEPP] = (char *) dxp_md_alloc((strlen(ioname)+1)*sizeof(char));
    strcpy(eppName[numEPP],ioname);

    /* See if this is a multi-module EPP chain, if not set its ID to -1 */
    if (ioname[0] == ':') 
    {
	sscanf(ioname, ":%d", &(eppID[numEPP]));

	sprintf(error_string, "ID = %i", eppID[numEPP]);
	dxp_md_log_debug("dxp_md_epp_open", error_string);

	/* Initialize the port address first */
        /* NOT NEEDED ON EPICS/LINUX FOR NOW */
	/* rstat = DxpInitPortAddress((int) eppPort);
	if (rstat != 0) 
	{
	    status = DXP_INITIALIZE;
	    sprintf(error_string,
		    "Unable to initialize the EPP port address: port=%d", eppPort);
	    dxp_md_log_error("dxp_md_epp_open", error_string, status);
	    return status;
	} */

	/* Call setID now to setup the port for Initialization */
        /* NOT NEEDED ON EPICS/LINUX FOR NOW */
	/* DxpSetID((unsigned short) eppID[numEPP]); */
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
    rstat = DxpInitEPP((int)eppPort);

    /* Check for Success */
    if (rstat==0) {
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
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_io(int* camChan, unsigned int* function, 
					   unsigned int* address, unsigned short* data,
					   unsigned int* length)
/* int *camChan;				Input: pointer to IO channel to access	*/
/* unsigned int *function;			Input: XIA EPP function definition	*/
/* unsigned int *address;			Input: XIA EPP address definition	*/
/* unsigned short *data;			I/O:  data read or written		*/
/* unsigned int *length;			Input: how much data to read or write	*/
{
    int rstat = 0; 
    int status;
  
    static int currentID = -1;

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
		if (*function==0) {
		    rstat = DxpReadBlock(next_addr, data, (int) *length);
		} else {
		    rstat = DxpWriteBlock(next_addr, data, (int) *length);
		}
	    } else {
		if (*function==0) {
		    rstat = DxpReadWord(next_addr, data);
		} else {
		    rstat = DxpWriteWord(next_addr, data[0]);
		}
	    }
	} else {
	    /* Perform long reads and writes if in program address space (24-bit) */
	    /* Allocate memory */
	    if (*function==0) {
		rstat = DxpReadBlocklong(next_addr, (unsigned long *)data, (int) *length/2);
	    } else {
		rstat = DxpWriteBlocklong(next_addr, (unsigned long *)data, (int) *length/2);
	    }
	}
	/* Address port*/
    } else if (*address==1) {
	next_addr = (unsigned short) *data;
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
	sprintf(error_string,"Unknown EPP address=%d",*address);
	status = DXP_MDIO;
	dxp_md_log_error("dxp_md_epp_io",error_string,status);
	return status;
    }

    if (rstat!=0) {
	status = DXP_MDIO;
	sprintf(error_string,"Problem Performing I/O to Function: %d, address: %#hx",*function, *address);
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

static int DxpInitEPP(int base)
{
   int status;
   unsigned char d;
   /* status = ioperm(base, 4, 0xffffffff); */
   status = iopl(3);
   sprintf(error_string, "DxpInitEPP port=%x, status=%d", base, status);
   dxp_md_log_debug("DxpInitEPP",error_string);
   if (status != 0) return(status);

   /* The following initialization code is from Gerry Swislow */
   d = IO_inb(base + 0x402);
   sprintf(error_string, "DxpInitEPP input from base+0x402 = %#X", d);
   dxp_md_log_debug("DxpInitEPP",error_string);
   d = (d&0x1F) + 0x80;
   IO_outb(d, base + 0x402);

   IO_outb(0x4, base + 2);
   IO_outb(0x1, base + 1);
   IO_outb(0x0, base + 1);

   /* check status (not time out, nByte=1) */
   d = IO_inb(base + 1);
   if ((d&0x01) == 1) {
      sprintf(error_string, "Timeout: status=(%#X)", d);
      dxp_md_log_error("DxpInitEPP",error_string,1);
      return(-1);
   }

   if(((d>>5)&0x01) == 1) {
      /* interface is off by one byte, attempt to fix */
      IO_outb(0, base + 3);
      d = IO_inb(base + 1);
      if (((d>>5)&0x01) == 1) {
         sprintf(error_string, "Error fixing interface offset");
         dxp_md_log_error("DxpInitEPP",error_string,1);
         return(-1);
      }
   }
   return(0);
}

static void DxpSetID(unsigned short id)
{
   /* This does nothing for now */
}

static int chk_stat(int p, int how) {
        int     s;

        /* check status (not a time out, nBytes = 1) */
        s = IO_inb(p + 1);          /* status port */
        if (s&0x01)
                return(-1);
        if (!how && !(s&0x20))
                return(-1);
        if (how && (s&0x20))
                return(-1);
        return(0);
}

static int set_addr(int base, unsigned short d) {
        unsigned char data;

        data = d&0xff;
        IO_outb(data, base + 3); 
        if (chk_stat(base, 0))
                return(-1);
        data = (d>>8)&0xff;
        IO_outb(data, base + 3);
        if (chk_stat(base, 1))
                return(-1);
        return(0);
}

static int DxpWriteWord(unsigned short addr, unsigned short data) 
{
        unsigned char   c0;
        int base = eppPort;
        int     ret = 0;

        if (addr < 0x4000)
                return(-1);

        if (set_addr(base, addr))
                return(-1);

        c0 = data&0xFF;
        IO_outb(c0, base + 4);
        ret = chk_stat(base, 0);

        c0 = (data>>8)&0xFF;
        IO_outb(c0, base + 4);
        ret += chk_stat(base, 1);

        return(ret);
}

static int DxpReadWord(unsigned short addr, unsigned short *data)
{
        int     ret = 0;
        unsigned  c0, c1;
        int base = eppPort;

        if (addr < 0x4000)
                return(-1);

        if (set_addr(base, addr))
                return(-1);

        c0 = IO_inb(base + 4);
        ret = chk_stat(base, 0);

        c1 = IO_inb(base + 4);
        ret += chk_stat(base, 1);

        *data = c0|(c1<<8);
        return(ret);
}

static int DxpWriteBlock(unsigned short addr, unsigned short *data, int len)
{
        int     i, ret = 0;
        unsigned char   c0;
        int base = eppPort;

        if (addr < 0x4000)
                return(-1);
        if (set_addr(base, addr))
                return(-1);
        for (i = 0; i < len; i++) {
                c0 = data[i]&0xFF;
                IO_outb(c0, base + 4);

                ret = chk_stat(base, 0);

                c0 = (data[i]>>8)&0xFF;
                IO_outb(c0, base +4);

                ret += chk_stat(base, 1);
                if (ret)
                        return(i + 1);
        }
        return(0);
}

static int DxpWriteBlocklong(unsigned short addr, unsigned long *data, int len)
{
        int     i, ret = 0;
        unsigned char   c0;
        int base = eppPort;

        if (addr >= 0x4000)
                return(-1);
        if (set_addr(base, addr))
                return(-1);
        for (i = 0; i < len; i++) {
                c0 = (data[i]    )&0xFF;
                IO_outb(c0, base + 4);
                ret = chk_stat(base, 0);

                c0 = (data[i]>> 8)&0xFF;
                IO_outb(c0, base + 4);
                ret += chk_stat(base, 1);

                c0 = (data[i]>>16)&0xFF;
                IO_outb(c0, base + 4);
                ret = chk_stat(base, 0);

                c0 = (data[i]>>24)&0xFF;
                IO_outb(c0, base + 4);
                ret += chk_stat(base, 1);

                if (ret)
                        return(i+1);
        }
        return(0);
}

static int DxpReadBlock(unsigned short addr, unsigned short *data, int len)
{
        int     i, ret = 0;
        unsigned short  c0, c1;
        int base = eppPort;

        if (addr < 0x4000)
                return(-1);
        if (set_addr(base, addr))
                return(-1);
        for (i = 0; i < len; i++) {
                c0 = IO_inb(base + 4);
                ret = chk_stat(base, 0);

                c1 = IO_inb(base + 4);
                ret += chk_stat(base, 1);

                data[i] = c0|(c1<<8);

                if (ret)
                        return(i+1);
        }
        return(0);
}

static int DxpReadBlocklong(unsigned short addr, unsigned long *data, int len)
{
        int     i, ret = 0;
        unsigned long   c0, c1, c2, c3;
        int base = eppPort;

        if (addr >= 0x4000)
                return(-1);
        if (set_addr(base, addr))
                return(-1);
        for (i = 0; i < len; i++) {
                c0 = IO_inb(base + 4);
                ret = chk_stat(base, 0);

                c1 = IO_inb(base + 4);
                ret += chk_stat(base, 1);

                c2 = IO_inb(base + 4);
                ret = chk_stat(base, 0);

                c3 = IO_inb(base + 4);
                ret += chk_stat(base, 1);

                data[i] = c0|(c1<< 8)|(c2<<16);

                if (ret)
                        return(i+1);
        }
        return(0);
}

#endif

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
	  dxp_md_free(usbName[numUSB]);
	}
  usbName[numUSB] = (char *) dxp_md_alloc((strlen(tempName)+1)*sizeof(char));
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
										   unsigned int* address, unsigned short* data,
										   unsigned int* length)
	 /* int *camChan;				Input: pointer to IO channel to access	*/
	 /* unsigned int *function;		Input: XIA EPP function definition	    */
	 /* unsigned int *address;		Input: XIA EPP address definition	    */
	 /* unsigned short *data;		I/O:  data read or written		        */
	 /* unsigned int *length;		Input: how much data to read or write	*/
{
  int rstat = 0; 
  int status;
  
  if (*address == 0) 
	{
	  if (*function==0) 
		{
		  rstat = usb_read(usb_addr, (long) *length, usbName[*camChan], data);
		} else {
		  rstat = usb_write(usb_addr, (long) *length, usbName[*camChan], data);
		}
	} else if (*address ==1) {
	  usb_addr = (long) data[0];
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


#if !(defined(EXCLUDE_UDXP) && defined(EXCLUDE_UDXPS))
/**********
 * This routine initializes the requested serial port
 * for operation at the specified baud. 
 **********/
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_initialize(unsigned int *maxMod, char *dllname)
{
    int status;
    int statusSerial;
  

    if (*maxMod > MAXMOD) 
    {
	status = DXP_NOMEM;
	sprintf(error_string, "Calling routine requests %d maximum modules: only"
		"%d available.", *maxMod, MAXMOD);
	dxp_md_log_error("dxp_md_serial_initialize", error_string, status);
	return status;
    }
  
    sprintf(error_string, "dllname = %s", dllname);
    dxp_md_log_debug("dxp_md_serial_initialize", error_string);
  
    /* Reset # of currently defined 
     * serial mods in the system.
     */
    numSerial = 0;

    sscanf(dllname, "COM%u", &comPort);

    sprintf(error_string, "COM Port = %u", comPort);
    dxp_md_log_debug("dxp_md_serial_initialize", error_string);
  
    statusSerial = InitSerialPort(comPort, 115200);

    if (statusSerial != SERIAL_SUCCESS) 
    {
	status = DXP_INITIALIZE;
	sprintf(error_string, "Unable to initialize the serial port: status = %d",
		statusSerial);
	dxp_md_log_error("dxp_md_serial_initialize", error_string, status);
	return status;
    }

    /* Close the port */

    /*
    statusSerial = CloseSerialPort(comPort);

    if (statusSerial != SERIAL_SUCCESS) 
    {
	status = DXP_INITIALIZE;
	sprintf(error_string, "Error closing serial port: status = %d", statusSerial);
	dxp_md_log_error("dxp_md_serial_initialize", error_string, status);
	return status;
    }
    */

    return DXP_SUCCESS;
}


/**********
 * This routine assigns the specified device a chan
 * number in the global array. (I think.)
 **********/ 
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_open(char* ioname, int* camChan)
{
    unsigned int i;


    sprintf(error_string, "ioname = %s", ioname);
    dxp_md_log_debug("dxp_md_serial_open", error_string);

    /* First loop over the existing names to make sure this module 
     * was not already configured?  Don't want/need to perform
     * this operation 2 times. */
    
    for(i = 0; i < numSerial; i++)
    {
	if(STREQ(serialName[i], ioname)) 
	{
	    *camChan = i;
	    return DXP_SUCCESS;
	}
    }
  
    /* Got a new one.  Increase the number existing and assign the global 
     * information */
    if (serialName[numSerial] != NULL) { dxp_md_free(serialName[numSerial]); }

    serialName[numSerial] = (char *)dxp_md_alloc((strlen(ioname) + 1) * sizeof(char));
    strcpy(serialName[numSerial], ioname);

    *camChan = numSerial++;
    numMod++;

    return DXP_SUCCESS;
}


#ifndef USE_SERIAL_DLPORTIO
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
    LARGE_INTEGER freq;
    LARGE_INTEGER before, after;

    int status;
    int statusSerial;

    unsigned int i;
 
    unsigned long timeout;
    unsigned long numBytes        = 0;
    unsigned long totalBytes      = 0;
    unsigned long bytesLeftToRead = 0;
    unsigned long loopCnt         = 0;

    char lowByte;
    char highByte;

    char sizeBuf[4];

    char *dataBuf    = NULL;
    char *bufPtr     = NULL;

    float wait = .001f;

    UNUSED(address); 


    /* Dump some debug information */
    /*
      sprintf(error_string, "camChan = %d, func = %u",
      *camChan, *function);
      dxp_md_log_debug("dxp_md_serial_io", error_string);
    */

    /* Get the proper comPort information 
     * Use this when we need to support multiple
     * serial ports.
     */
    sscanf(serialName[*camChan], "%u", &comPort);
	   
    QueryPerformanceFrequency(&freq);


    /* Check to see if this is a read or a write:
     * function = 0 : Read
     * function = 1 : Write
     * function = 2 : Open Port
     * function = 3 : Close Port
     */
    if (*function == 0) {

	/*Procedure to read:
	 * 1) Wait for 4 bytes
	 * 2) Read first 4 bytes from buffer
	 * 3) Get total buffer information from bytes read (BytesLeftToRead)
	 * 4) Poll buffer for # of bytes n
	 * 5) If n < BytesLeftToRead, read n bytes and subtract from BytesLeftToRead
	 * 6) if n >= BytesLeftToRead, read BytesLeftToRead and return
	 * 7) Repeat (4)-(6)
	 */
	timeout = 0;

	QueryPerformanceCounter(&before);

	do {

	    dxp_md_wait(&wait);

	    statusSerial = NumBytesAtSerialPort(comPort, &numBytes);
	  
	    if (numBytes > 0) {
		timeout = 0;
	    } else {
		timeout++;
	    }

	    if (timeout == SERIAL_WAIT_TIMEOUT) {
		break;
	    }

	    /*dxp_md_wait(&wait);*/

	} while (numBytes < 4);
	
	QueryPerformanceCounter(&after);

	/*
	sprintf(error_string, "READ Loop = %lf (secs)", 
		((double)after.QuadPart - (double)before.QuadPart) / (double)freq.QuadPart);
	dxp_md_log_debug("dxp_md_serial_io", error_string);
	*/

	if (timeout == SERIAL_WAIT_TIMEOUT) {

	    status = DXP_MDTIMEOUT;
	    sprintf(error_string, "Timeout waiting for first 4 bytes at COM%u",
		    comPort);
	    dxp_md_log_error("dxp_md_serial_io", error_string, status);
	    return status;
	}

	statusSerial = ReadSerialPort(comPort, 4, sizeBuf);

	if (statusSerial != SERIAL_SUCCESS) {

	    status = DXP_MDIO;
	    sprintf(error_string, "Error reading size from COM%u", comPort);
	    dxp_md_log_error("dxp_md_serial_io", error_string, status);
	    return status;
	}

	lowByte  = sizeBuf[2];
	highByte = sizeBuf[3];

	/*
	  sprintf(error_string, "lowByte = %#x, highByte = %#x", 
	  lowByte, highByte);
	  dxp_md_log_debug("dxp_md_serial_io", error_string);
	*/	

	totalBytes |= (unsigned long)lowByte & 0xFF;
	totalBytes |= (((unsigned long)highByte & 0xFF) << 8);

	/* Create buffer here */
	dataBuf = (char *)dxp_md_alloc((totalBytes + 1) * sizeof(char));

	if (dataBuf == NULL) {

	    status = DXP_NOMEM;
	    dxp_md_log_error("dxp_md_serial_io", "Out-of-memory allocating memory for read buffer", 
			     status);
	    return status;
	}

	bytesLeftToRead = totalBytes + 1;

	/* Reset the timeout again. In principle,
	 * any time that we are querying for 
	 * number of bytes in a loop we should be
	 * waiting for a timeout.
	 */
	timeout = 0;

	do {

	    /*
	      dxp_md_wait(&wait);
	    */

	    statusSerial = NumBytesAtSerialPort(comPort, &numBytes);

	    if (statusSerial != SERIAL_SUCCESS) {
		
		free((void *)dataBuf);
		dataBuf = NULL;

		status = DXP_MDIO;
		sprintf(error_string, "Error waiting for number of bytes at COM%u", 
			comPort);
		dxp_md_log_error("dxp_md_serial_io", error_string, status);
		return status;
	    }

	    if (numBytes > bytesLeftToRead) {

		dxp_md_free((void *)dataBuf);
		dataBuf = NULL;
		bufPtr  = NULL;

		status = DXP_MDOVERFLOW;
		sprintf(error_string, "Number of bytes (%#x bytes) at COM%u is greater "
			"then the number of bytes left to read (%#x bytes)",
			numBytes, comPort, bytesLeftToRead);
		dxp_md_log_error("dxp_md_serial_io", error_string, status);
		return status;
	    }

	    /* We only want to fill info in dataBuf
	     * from where we left off last time.
	     */
	    bufPtr = dataBuf + ((totalBytes + 1) - bytesLeftToRead);

	    statusSerial = ReadSerialPort(comPort, numBytes, bufPtr);
	  
	    if (statusSerial != SERIAL_SUCCESS) {

		dxp_md_free((void *)dataBuf);
		dataBuf = NULL;
		bufPtr  = NULL;

		status = DXP_MDIO;
		sprintf(error_string, "Error reading %u bytes from COM%u", 
			numBytes, comPort);
		dxp_md_log_error("dxp_md_serial_io", error_string, status);
		return status;
	    }

	    bytesLeftToRead -= numBytes;
      
	    if (timeout == SERIAL_WAIT_TIMEOUT) {
		status = DXP_MDTIMEOUT;
		sprintf(error_string,
			"Timeout at COM%u waiting for %u bytes",
			comPort, bytesLeftToRead);
		dxp_md_log_error("dxp_md_serial_io", error_string, status);
		return status;
	    }
      
	    /* Reset the timeout if we read
	     * any bytes at all.
	     */
	    if (numBytes > 0) {
		/*
		sprintf(error_string, "%u: Timeout reset", sizeBuf[1]);
		dxp_md_log_debug("dxp_md_serial_io", error_string);
		*/
		timeout = 0;
	    } else {
		/*
		sprintf(error_string, "%u: In Timeout mode", sizeBuf[1]);
		dxp_md_log_debug("dxp_md_serial_io", error_string);
		*/
		timeout++;
	    }


	} while (bytesLeftToRead != 0);

	/* Transfer data from the local array to 
	 * the user's array.
	 */

	/* totalBytes = # of data 
	 * +1         = for checksum
	 * +4         = for sizeBuf bytes
	 */
	if ((totalBytes + 5) > *length) {

	    /* For debugging purposes,
	     * let's see where things went wrong.
	     */
	    

	    dxp_md_free((void *)dataBuf);
	    dataBuf = NULL;
	    bufPtr  = NULL;

	    status = DXP_MDSIZE;
	    sprintf(error_string, "Local data buffer (%u bytes) is larger then "
		    "user data buffer (%u bytes)", totalBytes + 5, *length);
	    dxp_md_log_error("dxp_md_serial_io", error_string, status);
	    return status;
	}

	/* Do sizeBuf copy first */
	for (i = 0; i < 4; i++) {

	    data[i] = (unsigned short)sizeBuf[i];
	}

	/* Now the rest of the data */
	for (i = 0; i < (totalBytes + 1); i++) {

	    data[i + 4] = (unsigned short)dataBuf[i];
	}
  
	dxp_md_free((void *)dataBuf);
	dataBuf = NULL;
	bufPtr  = NULL;

    } else if (*function == 1) {

	/* Write to the serial port */

	statusSerial = CheckAndClearTransmitBuffer(comPort);

	if (statusSerial != SERIAL_SUCCESS) {
	    status = DXP_MDIO;
	    dxp_md_log_error("dxp_md_serial_io", "Error clearing transmit buffer", status);
	    return status;
	}

	statusSerial = CheckAndClearReceiveBuffer(comPort);

	if (statusSerial != SERIAL_SUCCESS) {
	    status = DXP_MDIO;
	    dxp_md_log_error("dxp_md_serial_io", "Error clearing receive buffer", status);
	    return status;
	}

	dataBuf = (char *)dxp_md_alloc(*length * sizeof(char));

	if (dataBuf == NULL) {

	    status = DXP_NOMEM;
	    sprintf(error_string, "Error allocating %u bytes for dataBuf",
		    *length);
	    dxp_md_log_error("dxp_md_serial_io", error_string, status);
	    return status;
	}

	for (i = 0; i < *length; i++) {

	    dataBuf[i] = (char)data[i];
	}

	statusSerial = WriteSerialPort(comPort, *length, dataBuf);

	if (statusSerial != SERIAL_SUCCESS) {

	    dxp_md_free((void *)dataBuf);
	    dataBuf = NULL;

	    status = DXP_MDIO;
	    sprintf(error_string, "Error writing %u bytes to COM%u",
		    *length, comPort);
	    dxp_md_log_error("dxp_md_serial_io", error_string, status);
	    return status;
	}

	dxp_md_free((void *)dataBuf);
	dataBuf = NULL;

    } else if (*function == 2) {

	/* Open the serial port */

	/*
	statusSerial = InitSerialPort(comPort, 115200);

	if (statusSerial != SERIAL_SUCCESS) {

	    status = DXP_INITIALIZE;
	    sprintf(error_string, "Error opening serial port COM%u", comPort);
	    dxp_md_log_error("dxp_md_serial_io", error_string, status);
	    return status;
	}
	*/

    } else if (*function == 3) {

	/* Close the serial port */

	/*
	statusSerial = CloseSerialPort(comPort);

	if (statusSerial != SERIAL_SUCCESS) {

	    status = DXP_INITIALIZE;
	    sprintf(error_string, "Error closing serial port: status = %u",
		    statusSerial);
	    dxp_md_log_error("dxp_md_serial_io", error_string, status);
	    return status;
	}
	*/
    } else {

	status = DXP_MDUNKNOWN;
	sprintf(error_string, "Unknown function: %u", *function);
	dxp_md_log_error("dxp_md_serial_io", error_string, status);
	return status;
    }

    return DXP_SUCCESS;
} 

#else

XIA_MD_STATIC int XIA_MD_API dxp_md_serial_io(int *camChan,
					      unsigned int *function,
					      unsigned int *address,
					      unsigned short *data,
					      unsigned int *length)
{
    int status;
    
    unsigned int i;

    unsigned char lo = 0x00;
    unsigned char hi = 0x00;

    unsigned short total = 0x0000;

    unsigned char sizeBuf[4];

    unsigned char *buf = NULL;

    UNUSED(address);


    sscanf(serialName[*camChan], "%u", &comPort);

    /* function = 0: Read
     * function = 1: Write
     * function = 2: Open Port
     * function = 3: Close Port
     */
    if (*function == 0) {
	dxp_md_poll_and_read(comPort, 4, sizeBuf);

	lo = sizeBuf[2];
	hi = sizeBuf[3];

	total = (unsigned short)(((unsigned short)hi << 8) | lo);

	buf = (unsigned char *)dxp_md_alloc((total + 1) * sizeof(unsigned char));

	if (buf == NULL) {
	    status = DXP_NOMEM;
	    dxp_md_log_error("dxp_md_serial_io", 
			     "Out-of-memory allocating memory for received data buffer",
			     status);
	    return status;
	}

	dxp_md_poll_and_read(comPort, (unsigned long)(total + 1), buf);

	if ((unsigned int)(total + 5) > *length) {
	    dxp_md_free(buf);
	    status = DXP_MDSIZE;
	    sprintf(error_string,
		    "Local data buffer (%u bytes) is larger then user data buffer (%u bytes)",
		    total + 5, *length);
	    dxp_md_log_error("dxp_md_serial_io", error_string, status);
	    return status;
	}

	/* Copy the size info */
	for (i = 0; i < 4; i++) {
	    data[i] = (unsigned short)sizeBuf[i];
	}

	for (i = 0; i < (unsigned int)(total + 1); i++) {
	    data[i + 4] = (unsigned short)buf[i];
	}
	
	dxp_md_free(buf);
	buf = NULL;
    
    } else if (*function == 1) {
	
	/* Clear buffer of any detritus */
	dxp_md_clear_port(comPort);

	/* Copy "unsigned short" data to an 
	 * unsigned char * buffer
	 */
	buf = (unsigned char *)dxp_md_alloc(*length * sizeof(unsigned char));

	if (buf == NULL) {
	    status = DXP_NOMEM;
	    dxp_md_log_error("dxp_md_serial_io",
		    "Out-of-memory allocating memory for data buffer",
		    status);
	    return status;
	}

	for (i = 0; i < *length; i++) {
	    buf[i] = (unsigned char)(data[i] & 0xFF);
	}

	dxp_md_serial_write(comPort, *length, buf);

	dxp_md_free(buf);
	buf = NULL;
    
    } else if (*function == 2) {
	/* Don't need to do a thing! */

    } else if (*function == 3) {
	/* Don't need to do a thing! */
    
    } else {
	status = DXP_MDUNKNOWN;
	sprintf(error_string, "Unknown function: %u", *function);
	dxp_md_log_error("dxp_md_serial_io", error_string, status);
	return status;
    }

    return DXP_SUCCESS;
}

#endif /* USE_SERIAL_DLPORTIO */


/**********
 * Closes the serial port connection so that we don't
 * crash due to interrupt vectors left lying around.
 **********/
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_close(int *camChan)
{
    int status;
    
    UNUSED(camChan);


    status = CloseSerialPort(0);

    if (status != 0) {
	return DXP_MDCLOSE;
    }

    return DXP_SUCCESS;
}


#endif /* EXCLUDE_UDXP && EXCLUDE_UDXPS */  
 
 
/*****************************************************************************
 * 
 * Routine to control the error reporting operation.  Currently the only 
 * keyword available is "print_debug".  Then whenever dxp_md_error() is called
 * with an error_code=DXP_DEBUG, the message is printed.  
 * 
 *****************************************************************************/
XIA_MD_STATIC void XIA_MD_API dxp_md_error_control(char* keyword, int* value)
/* char *keyword;						Input: keyword to set for future use by dxp_md_error()	*/
/* int *value;							Input: value to set the keyword to					*/
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
/* float *time;							Input: Time to wait in seconds	*/
{
    double dtime = *time;

#ifdef vxWorks
    /* On vxWorks there is no guarantee that epicsThreadSleep will sleep for at least the
     * requested time, there is only the guarantee that it will sleep until the next clock tick
     * To guarantee we add 2 clock ticks to the requested time */
    /* dtime += 2.*epicsThreadSleepQuantum(); */
    /* This seems to fix a problem with initializing the DXP-2X.  Need to track down! */
    if (dtime < 0.1) dtime = 0.1;
#endif

    epicsThreadSleep(dtime);

    return DXP_SUCCESS;
}

/*****************************************************************************
 * 
 * Routine to allocate memory.  The calling structure is the same as the 
 * ANSI C standard routine malloc().  
 * 
 *****************************************************************************/
XIA_MD_SHARED void* XIA_MD_API dxp_md_alloc(size_t length)
/* size_t length;							Input: length of the memory to allocate
   in units of size_t (defined to be a 
   byte on most systems) */
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
/* void *array;							Input: pointer to the memory to free */
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
/* char *s;								Input: string to print or log	*/
{

    return printf("%s", s);

}


#ifdef USE_SERIAL_DLPORTIO
/**********
 * This routine polls the LSR for the data received bit and
 * then puts the byte into a buffer. 
 **********/
static void dxp_md_poll_and_read(unsigned short port, unsigned long nBytes, unsigned char *buf)
{
    unsigned long cnt = 0;


    sprintf(error_string, "port = %u, nBytes = %u", port, nBytes);
    dxp_md_log_debug("dxp_md_poll_and_read", error_string);

    while (cnt < nBytes) {
	
	if (IsByteAtPort(port)) {
	    ReadSerialPort(port, 1, (char *)&buf[cnt]);
	    cnt++;

	    sprintf(error_string, "Found a byte (%u total) at port = %u" ,cnt, port);
	    dxp_md_log_debug("dxp_md_poll_and_read", error_string);
	}
	
	/* Do the timeout here */

    }
}

static void dxp_md_serial_write(unsigned short port, unsigned long nBytes, unsigned char *buf)
{
    unsigned long i = 0;

    float myWait = .001f;


    DumpRegistersToConsole(port);

    sprintf(error_string, "port = %u, nBytes = %u", port, nBytes);
    dxp_md_log_debug("dxp_md_serial_write", error_string);

    for (i = 0; i < nBytes; i++) {

	while (!IsClearForNextByte) {
	    dxp_md_wait(&myWait);
	}

	WriteSerialPort(port, 1, (char *)&buf[i]);
	
	dxp_md_log_debug("dxp_md_serial_write", "Wrote a byte");
    }
}

static void dxp_md_clear_port(unsigned short port)
{
    char byte = 0x00;

    while (IsByteAtPort(port)) {
	ReadSerialPort(port, 1, (char *)&byte);
    }
}
#endif /* USE_SERIAL_DLPORTIO */





