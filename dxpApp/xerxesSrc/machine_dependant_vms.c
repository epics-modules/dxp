/*  machine_dependant_vms.c
 *
 *  created  30-Sep-1996 Ed Oltman
 *  modified 19-Dec-1996 E.O.
 *  modified 03-Feb-1996 EO : remove path name from includes
 *  modified 07-Feb-1996 EO : use static declaration for globals
 *
 *  Copyright 1996 X-ray Instrumentation Associates
 *  All rights reserved
 *
 *    This file contains the interface between the DSP Technologies LSI-11
 *    CAMAC interface drivers and the DXP primitive calls.
 *
 */
#include <camvms.h>
#include <ssdef.h>
#include <stdio.h>
#include <string.h>
#include <machine_dependant.h>
#include <dxp.h>
static char error_string[132];
static int print_debug;
/*****************************************************************************/
int mdopen(char *camac_device_string,int *cam_chan){
/*
 *  This is the c version of above...
 */
    struct fortstring camac_device;
    int status;
    unsigned short chan;

    camac_device.len = strlen(camac_device_string);
    camac_device.thestring=camac_device_string;
    status= sys$assign(&camac_device,&chan,0,NULL,0);
    if (status != SS$_NORMAL){
        sprintf(error_string,
           "mdopen: sys$assign error: return status = %d",status);
        status=DXP_MDOPEN;
        mderror("mdopen",error_string,&status);
    } else status = DXP_SUCCESS;
    *cam_chan = (int)chan;
    return status;
}
/******************************************************************************/
int mdio(int *cam_chan,  /* input: I/O channel for DXP module                 */
         int *function,  /* input: CAMAC F-code                               */
         int *subaddress,/* input: CAMAC A-code                               */
         short *data,    /* input/output: first word of data transfer         */
         int *length){   /* input: number of words to transfer                */
/*
 *  primary I/O transfer routine. Use for both reads and writes.  Note: this
 *  version does not look for Q- or X-response
 */
    CAMSBLOCK iostat;
    int status;
    int FAword;
    FAword = FA$M_16BIT | (*function<<FA$V_FUNCTION) | *subaddress;
    status = sys$qiow(
                 0,
                 *cam_chan,
                 IO$_COMMAND,
                 &iostat,
                 NULL,
                 0,
                 data,
                 *length,
                 FAword,
                 NULL,
                 NULL,
                 NULL);
    if (status != SS$_NORMAL){
       sprintf(error_string,
            " sys$qiow error: return status = %d",status);
       status=DXP_MDIO;
       mderror("mdio",error_string,&status);
       sprintf(error_string,
            " ...I/O status block: return_code: %x %x return data %x",
            iostat.return_code[0],iostat.return_code[1],iostat.return_data);
       mderror("mdio",error_string,&status);
    } else status = DXP_SUCCESS;
    return status;
}
/*****************************************************************************/
int mdwait(float *time){
/*
 *        wait time in seconds
 */
   return LIB$WAIT(time);
}
/*****************************************************************************/
void mderror(char *routine,char *message,int *error_code){
    if (*error_code>=DXP_DEBUG){
        if (print_debug!=0) printf("%s : %s\n",routine,message);
        return;
    }
    printf("%s:[%d] %s\n",routine,*error_code,message);
}
/*****************************************************************************/
void mderror_control(char *keyword, int *value){
    if (strstr(keyword,"print_debug")!=NULL) {
        print_debug = (*value);
        return;
    }
    printf("mderror_control: keyword %s not recognized..no action taken\n",
        keyword);
}
