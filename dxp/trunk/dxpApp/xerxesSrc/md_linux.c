/*****************************************************************************
 *
 *  md_linux.c
 *
 *  Created 16-Aug-2000 John Wahl
 *
 *  Copywrite 2000 X-ray Instrumentaton Associates
 *  All rights reserved
 *
 *    This file contains the interface between the Jorway 73A CAMAC SCSI
 *    interface Library and the DXP primitive calls.
 */
/* Specific include file for the Tcl call: Tcl_Sleep() */
#include <tcl.h>
/* need a couple of IEEE function prototypes */
extern unsigned int cdreg(int*,int,int,int,int);
extern unsigned int csubc(int,int,unsigned short*,int*);
extern unsigned int cssa(int,int,unsigned short*,int*);
extern unsigned int cccc(int);
extern unsigned int ccci(int,int);
extern unsigned int cdchn(int,int,int);
extern unsigned int cccz(int);
extern unsigned int ccctype(int,int,int);
/* C Include files */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "xerxes_errors.h"
#include "xerxes_structures.h"
#include "md_linux.h"
#define MODE 4

/* variables to store the IO channel information, later accessed via a pointer */
static int *branch=NULL,*crate=NULL,*station=NULL;
/* total number of modules in the system */
static unsigned int numDXP=0;
/* error string used as a place holder for calls to dxp_md_error() */
static char error_string[132];
/* are we in debug mode? */
static int print_debug=0;
/* maximum number of words able to transfer in a single call to dxp_md_io() */
static unsigned int maxblk=8192;

/******************************************************************************
 *
 * Routine to create pointers to the MD utility routines
 * 
 ******************************************************************************/
int dxp_md_init_util(Xia_Util_Functions* funcs, char* type)
/* Xia_Util_Functions *funcs;	*/
/* char *type;					*/
{
  funcs->dxp_md_error_control = dxp_md_error_control;
  funcs->dxp_md_error         = dxp_md_error;
  funcs->dxp_md_alloc         = dxp_md_alloc;
  funcs->dxp_md_free          = dxp_md_free;
  funcs->dxp_md_puts          = dxp_md_puts;
  funcs->dxp_md_wait          = dxp_md_wait;
  
  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Routine to create pointers to the MD utility routines
 * 
 ******************************************************************************/
int dxp_md_init_io(Xia_Io_Functions* funcs, char* type)
/* Xia_Io_Functions *funcs;		*/
/* char *type;					*/
{
  funcs->dxp_md_io         = dxp_md_io;
  funcs->dxp_md_initialize = dxp_md_initialize;
  funcs->dxp_md_open       = dxp_md_open;
  funcs->dxp_md_get_maxblk = dxp_md_get_maxblk;
  funcs->dxp_md_set_maxblk = dxp_md_set_maxblk;

  return DXP_SUCCESS;
}

/*****************************************************************************
 * 
 * Initialize the system.  Alloocate the space for the library arrays, define
 * the pointer to the CAMAC library and the IO routine.
 * 
 *****************************************************************************/
static int dxp_md_initialize(unsigned int* maxMod, char* dllname)
/* unsigned int *maxMod;                    Input: maximum number of dxp modules allowed */
/* char *dllname;			    Input: name of the DLL			 */
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
  if ((branch==NULL)||(crate==NULL)||(station==NULL)){
    status = DXP_NOMEM;
    dxp_md_error("dxp_md_initialize",
		 "Unable to allocate branch, crate and station memory",&status);
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
/* char *name;							Input:  string used to specify this IO 
												channel */
/* int *camChan;						Output: returns a reference number for
												this module */
{
  int branch,crate,station,status;

/* pull out the branch, crate and station number and call the internal routine */

  sscanf(name,"%2d%1d%2d",&branch,&crate,&station);
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
/* int *new_branch;					Input: branch number					*/
/* int *new_crate;					Input: crate number					*/
/* int *new_station;				Input: station number				*/
/* int *camChan;					Output: pointer to this branch and 
												crate						*/
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
/* int *camChan;					Input: pointer to IO channel to access		*/
/* unsigned int *function;				Input: function number to access (CAMAC F)	*/
/* unsigned int *address;				Input: address to access	(CAMAC A)	*/
/* unsigned short *data;				I/O:  data read or written			*/
/* unsigned int *length;				Input: how much data to read or write		*/
{

  int ext;
  int cb[4];
  short camadr[4];
  long lstatus=0;
  int status;
/* mode defines Q-stop, etc... types of transfer */
  short mode=MODE;

  short func = (short) *function;
  unsigned long  nbytes;
 
  cdchn(branch[*camChan],1,0);

/* First encode the transfer information in an array using 
 * an SJY routine. */
  cdreg(&ext, branch[*camChan], crate[*camChan], station[*camChan], (int) *address);

  cb[0] = *length;
  cb[1] = 0;
  cb[2] = 0;
  cb[3] = 0;  

/* Now perform the I/O transfer */
  lstatus = csubc(*function, ext, data, cb);

  if (*function<=8) {
    if (cb[1]!=*length){
      status = DXP_MDIO;
      sprintf(error_string,"camac transfer error: bytes read %d vs expected %d",cb[1],cb[0]);
      dxp_md_error("dxp_md_io",error_string,&status);
      return status;
    }
  } else {
    if (cb[1]!=0){
      status = DXP_MDIO;
      sprintf(error_string,"camac transfer error: status = %lx ",(long) cb[1]);
      dxp_md_error("dxp_md_io",error_string,&status);
      return status;
    }
  }

  cdchn(branch[*camChan],0,0);

/* All done, free the library and get out */
  status=DXP_SUCCESS;

  return status;
}

/*****************************************************************************
 * 
 * Routine to control the error reporting operation.  Currently the only 
 * keyword available is "print_debug".  Then whenever dxp_md_error() is called
 * with an error_code=DXP_DEBUG, the message is printed.  
 * 
 *****************************************************************************/
static void dxp_md_error_control(char* keyword, int* value)
/* char *keyword;		   Input: keyword to set for future use by dxp_md_error()	*/
/* int *value;			   Input: value to set the keyword to				*/
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
 * Routine to handle error reporting.  Allows the user to pass errors to log
 * files or report the information in whatever fashion is desired.
 * 
 *****************************************************************************/
static void dxp_md_error(char* routine, char* message, int* error_code)
/* char *routine;					Input: Name of the calling routine		*/
/* char *message;					Input: Message to report					*/
/* int *error_code;					Input: Error code denoting type of error	*/
{

/* If the error_code is larger than DXP_DEBUG, then print the error only 
 * if debugging is requested */

  if(*error_code>=DXP_DEBUG){
    if(print_debug!=0) printf("%s : %s\n",routine,message);
    return;
  }

/* Else print the error */

  printf("%s:[%d] %s\n",routine,*error_code,message);

}

/*****************************************************************************
 * 
 * Routine to get the maximum number of words that can be block transfered at 
 * once.  This can change from system to system and from controller to 
 * controller.  A maxblk size of 0 means to write all data at once, regardless 
 * of transfer size.
 * 
 *****************************************************************************/
static int dxp_md_get_maxblk(void)
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
static int dxp_md_set_maxblk(unsigned int* blksiz)
/* unsigned int *blksiz;			*/
{
  int status; 
  
  if (*blksiz > 0) {
    maxblk = *blksiz;
  } else {
    status = DXP_NEGBLOCKSIZE;
    dxp_md_error("dxp_md_set_maxblk","Block size must be positive or zero",&status);
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
static int dxp_md_wait(float* time)
/* float *time;							Input: Time to wait in seconds	*/
{
  int status=0;
  
  Tcl_Sleep((int) (1000*(*time)));
  
  return status;

}

/*****************************************************************************
 * 
 * Routine to allocate memory.  The calling structure is the same as the 
 * ANSI C standard routine malloc().  
 * 
 *****************************************************************************/
static void *dxp_md_alloc(size_t length)
/* size_t length;			Input: length of the memory to allocate
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
static void dxp_md_free(void* array)
/* void *array;				Input: pointer to the memory to free */
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
static int dxp_md_puts(char* s)
/* char *s;  		              	Input: string to print or log	*/
{

  return printf("%s", s);

}

