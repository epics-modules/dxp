/* 
 *    This is a machine dependant section of code: EPICS/vxWorks
 *  Created by Mark Rivers, 1996
 *  Modifications:
 *   06/27/98  mlr    Minor changes to fix compiler warnings
 *   07/21/98  mlr    Workaround for problem that 2917 cannot seem to do block
 *                    mode write operations with a long cable
 */
#include "stdlib.h"
#include "stdio.h"
#include "sysLib.h"
#include "taskLib.h"
#include "machine_dependent.h"
#include "dxp.h"
#include "vxWorks.h"
#include "camacLib.h"
#include <string.h>

static struct
   {  int b;
      int c;
      int n;
   } camac_addr[MAXDXP];
static int numdxp=0;
static int print_debug;
/*****************************************************************************/
int mdopen_bcn(int *b, int *c, int *n, int *cam_chan){
    int i;
    /* See if we already have a channel open to this module */
    *cam_chan = -1;
    for (i=0; i<numdxp; i++) {
       if (camac_addr[i].b == *b &&
           camac_addr[i].c == *c &&
           camac_addr[i].n == *n) *cam_chan = i;
    }
    if (*cam_chan == -1) {
      numdxp +=1;
      i = numdxp-1;
      if (numdxp >= MAXDXP) return DXP_MDOPEN;
      camac_addr[i].b = *b;
      camac_addr[i].c = *c;
      camac_addr[i].n = *n;
      *cam_chan = i;
    }
    return DXP_SUCCESS;
}
/*****************************************************************************/
int mdopen(char *string, int *cam_chan){
   int b=0, c=0, n;

   n = atoi(string);
   return mdopen_bcn(&b, &c, &n, cam_chan);
}
   
/*****************************************************************************/
int mdio(int *cam_chan,int *function,int *subaddress,short *data,int *length){
    int status;
    int ext;
    int q;
    int i;
    int cb[4]={0,0,0,0};
    char error_string[132];

    cdreg(&ext,
          camac_addr[*cam_chan].b, 
          camac_addr[*cam_chan].c, 
          camac_addr[*cam_chan].n, 
          *subaddress);
    if (*length == 1) {
       cssa(*function, ext, data, &q);
       if (q==0) {
          status = DXP_MDIO;
          mderror("mdio","(cssa): Q=0",&status);
          return status;
       }
    } else {
       /* If this is a write operation then do single-cycle transfers, due
          to an apparent bug in the 2917/3922 when using long cables.  This
          problem should be tracked down and fixed!  MLR 7/21/98 */
       if ((*function >= 16) && (*function < 24)) {
           for (i=0; i<*length; i++) {
       		cssa(*function, ext, data, &q);
       		if (q==0) {
          	   status = DXP_MDIO;
          	   mderror("mdio","(cssa): Q=0",&status);
          	   return status;
                }
           }
       }
       else {
       	  /* This code is only being used for read operations now, it should
       	     be used for both reads and writes once above problem is fixed.
       	     MLR 7/21/98 */
          cb[0] = *length;
          csubc(*function, ext, data, cb);
          if (cb[1] != cb[0]) {
       	     status=DXP_MDIO;
             sprintf(error_string,
                "(csubc): desired output words=%d, actual=%d",
                cb[0], cb[1]);
             mderror("mdio",error_string,&status);
             return status;
          }
       }
    }
    return DXP_SUCCESS;
}
/*****************************************************************************/
int mdwait(float *time){
     int ticks;
     ticks = sysClkRateGet() * (*time);
     if (ticks<1) ticks=1;  /* Wait at least one clock tick */
     taskDelay(ticks);
     return(0);
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
