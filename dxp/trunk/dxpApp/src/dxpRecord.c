/* dxpRecord.c - Record Support Routines for DXP record
 *
 * This record works with both the DXP4C and the DXP2X modules.
 *
 *      Author:         Mark Rivers
 *      Date:           1/24/97
 *
 * Modification Log:
 * -----------------
 * 06/27/98     mlr    Minor fixes for compiler warnings
 * 06/01/01     mlr    Complete rewrite for Xerxes version of XIA software and
 *                     to use MPF server
 * 02/09/02     mlr    Download all parameters in init_record, now that MPF
 *                     allows sending messages at that time. Only do this if
 *                     the PINI field is YES, so that it is possible to turn
 *                     off downloading parameters at boot time, since sending
 *                     bad parameters can cause crashes.
 * 02/13/02     mlr    Update run task menus based on readback of RUNTASKS.
 *                     Moved building peaking time strings to separate function.
 *                     Added debugging statements.
 * 10-Mar-02    mlr    Added support for FIPPI, EMAXRBC, GAINRBV fields.
 * 12-Mar-02    mlr    Changed init_record to restore .GAIN, .EMAX, .FIPPI.
 * 04-Apr-02    mlr    Moved some initialization from init_record to process, since
 *                     it cannot be done until the device parameters have been read
 *                     once
 */

#include        <string.h>
#include        <stdio.h>
#include        <stdlib.h>
#include        <math.h>

#include        <alarm.h>
#include        <dbDefs.h>
#include        <dbAccess.h>
#include        <dbFldTypes.h>
#include        <dbEvent.h>
#include        <devSup.h>
#include        <drvSup.h>
#include        <errMdef.h>
#include        <recSup.h>
#include        <recGbl.h>
#include        <epicsExport.h>
#define GEN_SIZE_OFFSET
#include        "dxpRecord.h"
#undef GEN_SIZE_OFFSET
#include        "mca.h"
#include        "handel.h"
#include        "handel_generic.h"
#include        "devDxp.h"


/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long special();
#define get_value NULL
static long cvt_dbaddr();
static long get_array_info();
#define put_array_info NULL
#define get_units NULL
static long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double();
static long get_control_double();
#define get_alarm_double NULL

/* This is the maximum value of HDWRVAR - need to get from XIA, keep updated */
#define MAX_MODULE_TYPES 6
typedef struct {
   unsigned short runtasks;
   unsigned short decimation;
   unsigned short gaindac;
} PARAM_OFFSETS;

typedef struct {
   char **names;
   unsigned short *access;
   unsigned short *lbound;
   unsigned short *ubound;
   unsigned short nparams;
   unsigned int nbase_histogram;
   unsigned int nbase_history;
   unsigned int ntrace;
   PARAM_OFFSETS offsets;
} MODULE_INFO;



/* The dxp record contains many short integer parameters which control the
 * DXP analog signal processing and digital filtering.
 * The record .dbd file defines 3 fields for each of these parameters:
    short xxnV
        ; The value of the parameter. Default values for some parameters are
        ; set in the .dbd file.
    char  xxnL[DXP_STRING_SIZE]
        ; The label for the parameter, which is how we look up its address
        ; with the DXP software.  These labels are defined in dxp.db, since one
        ; cannot assign the initial value of DBF_STRING fields in the .dbd
        ; file, and the values are different for the DXP4C and DXP2X.
    short xxnO
        ; The offset of this parameter within the DXP memory.  These offsets are
        ; obtained during the record initialization phase from the labels
        ; described above.
        ; Given the above fields, much of the record processing is done using
        ; loops and a pointer to a structure which is this field
 */
/* The dxp record contains many long integer parameters which are used for
 *  reading out statistical information about the run.
 * The record .dbd file defines 3 fields for each of these parameters:
    long xxnV
        ; The value of the parameter.
    char  xxnL[DXP_STRING_SIZE]
        ; The label for the first 16 bit word of the parameter, which is how 
        ; we look up its address with the DXP software.  These labels are 
        ; defined in dxp.db, since one cannot assign the initial value of 
        ; DBF_STRING fields in the .dbd file, and the values are different 
        ; for the DXP4C and DXP2X.
    short xxnO
        ; The offset of this parameter within the DXP memory.  These offsets are
        ; obtained during the record initialization phase from the labels
        ; described above.
        ; Given the above fields, much of the record processing is done using
        ; loops and a pointer to a structure which is this field
 */
/* The dxp record contains many Yes/No menu parameters which are used for 
 * defining the "Runtasks" tasks to be perfomed.  The parameters must be 
 * defined in the order in which the bits occur in the "Runtasks" word.
 * The record .dbd file defines 2 fields for each of these parameters:
    DBF_MENU TnnV
        ; The value of the parameter. 0/1 corresponding to No/Yes.
    char  TnnL[DXP_TASK_STRING_SIZE]
        ; A descriptive string for the task.
 */
#define DXP_STRING_SIZE      16
#define DXP_TASK_STRING_SIZE 25

typedef struct  {
    unsigned short   val;
    char             label[DXP_STRING_SIZE];
    short            offset;
} DXP_SHORT_PARAM;

typedef struct  {
    long     val;
    char    label[DXP_STRING_SIZE];
    short   offset;
} DXP_LONG_PARAM;

typedef struct  {
    unsigned short   val;
    char    label[DXP_TASK_STRING_SIZE];
} DXP_TASK_PARAM;

#define NUM_ASC_PARAMS       9 /* The number of short ASC parameters described 
                                * above in the record */
#define NUM_FIPPI_PARAMS    10 /* The number of short FIPPI parameters described
                                * above in the record */
#define NUM_DSP_PARAMS      10 /* The number of short DSP parameters described
                                *  above in the record */
#define NUM_BASELINE_PARAMS  8 /* The number of short BASELINE parameters 
                                * described above in the record */
#define NUM_READONLY_PARAMS 16 /* The number of short READ-ONLY parameters 
                                * described above in the record */
#define NUM_DXP_LONG_PARAMS 14 /* The number of long (32 bit) statistics 
                                * parameters described above in the record. 
                                * These are also read-only */
#define NUM_TASK_PARAMS     16 /* The number of task parameters described above
                                *  in the record. */
 

#define NUM_SHORT_WRITE_PARAMS \
   (NUM_ASC_PARAMS + NUM_FIPPI_PARAMS + NUM_DSP_PARAMS + NUM_BASELINE_PARAMS)
#define NUM_SHORT_READ_PARAMS (NUM_SHORT_WRITE_PARAMS +  NUM_READONLY_PARAMS)

/* Debug support */
#define Debug(l,FMT,V...) {if (l <= dxpRecordDebug) \
                          { printf("%s(%d):",__FILE__,__LINE__); \
                            printf(FMT,##V);}}
volatile int dxpRecordDebug = 0;
epicsExportAddress(int, dxpRecordDebug);


static MODULE_INFO moduleInfo[MAX_MODULE_TYPES];
static long monitor(struct dxpRecord *pdxp);
static void setDxpTasks(struct dxpRecord *pdxp);
static int getParamOffset(MODULE_INFO *minfo, char *name, short *offset);

struct rset dxpRSET={
        RSETNUMBER,
        report,
        initialize,
        init_record,
        process,
        special,
        get_value,
        cvt_dbaddr,
        get_array_info,
        put_array_info,
        get_units,
        get_precision,
        get_enum_str,
        get_enum_strs,
        put_enum_str,
        get_graphic_double,
        get_control_double,
        get_alarm_double };
epicsExportAddress(rset, dxpRSET);


static long init_record(struct dxpRecord *pdxp, int pass)
{
    struct devDxpDset *pdset;
    int i;
    int detChan;
    MODULE_INFO *minfo;
    int status;
    unsigned short hdwrvar;
    DXP_SHORT_PARAM *short_param;
    DXP_LONG_PARAM *long_param;
    unsigned short offset;

    if (pass != 0) return(0);
    Debug(5, "(init_record): entry\n");

    /* must have dset defined */
    if (!(pdset = (struct devDxpDset *)(pdxp->dset))) {
        printf("dxpRecord:init_record no dset\n");
        status = S_dev_noDSET;
        goto bad;
    }
    if (pdset->init_record) {
        if ((status=(*pdset->init_record)(pdxp, &detChan))) return(status);
    }
    if (detChan < 0) {
        printf("dxpRecord:init_record hardware communication error\n");
        status = -1;
        goto bad;
    }
    /* Figure out what kind of module this is */
    xiaGetParameter(detChan, "HDWRVAR", &hdwrvar);
    if (hdwrvar > MAX_MODULE_TYPES) {
        printf("dxpRecord:init_record hdwrvar=%d > MAX_MODULE_TYPES\n", hdwrvar);
        status = -1;
        goto bad;
    }
        
    pdxp->mtyp = hdwrvar;
    minfo = &moduleInfo[pdxp->mtyp];

    /* If minfo->nparams=0 then this is the first DXP record of this 
     * module type, allocate global (not record instance) structures */
    if (minfo->nparams == 0) {
        xiaGetNumParams(detChan, &minfo->nparams);
        xiaGetRunData(detChan, "baseline_length", &minfo->nbase_histogram);
        xiaGetSpecialRunData(detChan, "adc_trace_length", &minfo->ntrace);
        xiaGetSpecialRunData(detChan, "baseline_history_length", 
                             &minfo->nbase_history);
        minfo->names = (char **)malloc(minfo->nparams * sizeof(char *));
        for (i=0; i<minfo->nparams; i++) {
           minfo->names[i] = 
               (char *) malloc(MAX_DSP_PARAM_NAME_LEN * sizeof(char *));
        }
        minfo->access =
               (unsigned short *) malloc(minfo->nparams * 
                                         sizeof(unsigned short));
        minfo->lbound =
               (unsigned short *) malloc(minfo->nparams * 
                                         sizeof(unsigned short));
        minfo->ubound =
               (unsigned short *) malloc(minfo->nparams * 
                                         sizeof(unsigned short));
        xiaGetParamData(detChan, "names", minfo->names);
        xiaGetParamData(detChan, "access", minfo->access);
        xiaGetParamData(detChan, "lower_bounds", minfo->lbound);
        xiaGetParamData(detChan, "upper_bounds", minfo->ubound);
    }

    /* Look up the address of each parameter */
    /* Address of first short parameter */
    short_param = (DXP_SHORT_PARAM *)&pdxp->a01v;
    for (i=0; i<NUM_SHORT_READ_PARAMS; i++) {
       if (strcmp(short_param[i].label, "Unused") == 0)
          offset=-1;
       else {
          getParamOffset(minfo, short_param[i].label, &offset);
       }
       short_param[i].offset = offset;
    }
    /* Address of first long parameter */
    long_param = (DXP_LONG_PARAM *)&pdxp->s01v;
    for (i=0; i<NUM_DXP_LONG_PARAMS; i++) {
       if (strcmp(long_param[i].label, "Unused") == 0)
          offset=-1;
       else
          getParamOffset(minfo, long_param[i].label, &offset);
       long_param[i].offset = offset;
    }

    /* Look up the offsets of parameters we may need to access in the record */
    status = getParamOffset(minfo, "RUNTASKS", &minfo->offsets.runtasks); 
    if (status != 0) {
        printf("dxpRecord:init_record cannot find RUNTASKS\n");
        goto bad;
    }
    status = getParamOffset(minfo, "DECIMATION", &minfo->offsets.decimation); 
    if (status != 0) {
        printf("dxpRecord:init_record cannot find DECIMATION\n");
        goto bad;
    }
    status = getParamOffset(minfo, "GAINDAC", &minfo->offsets.gaindac); 
    if (status != 0) {
        printf("dxpRecord:init_record cannot find GAINDAC\n");
        goto bad;
    }

    /* Allocate the space for the parameter array */
    pdxp->pptr = (unsigned short *)calloc(minfo->nparams, sizeof(short));

    /* Allocate the space for the baseline histogram array */
    pdxp->bptr = (long *)calloc(minfo->nbase_histogram, sizeof(long));

    /* Allocate the space for the baseline history array */
    pdxp->bhptr = (long *)calloc(minfo->nbase_history, sizeof(long));

    /* Allocate the space for the trace array */
    pdxp->tptr = (long *)calloc(minfo->ntrace, sizeof(long));

    /* Allocate the space for the readback structure */
    pdxp->rbptr = (void *)calloc(1, sizeof(dxpReadbacks));

    /* Some higher-level initialization can be done here, i.e. things which don't
     * require information on the current device parameters.  Things which do
     * require information on the current device parameters must be done in process,
     * after the first time the device is read */
                                  
    /* Download the high-level parameters if PINI is true and PKTIM is non-zero
     * which is a sanity check that save_restore worked */
    if (pdxp->pini && (pdxp->pktim != 0.)) {
        status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->slow_trig, &pdxp->slow_trig);
        status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->pktim, &pdxp->pktim);
        status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->gaptim, &pdxp->gaptim);
        status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->fast_trig, &pdxp->fast_trig);
        status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->trig_pktim, &pdxp->trig_pktim);
        status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->trig_gaptim, &pdxp->trig_gaptim);
        status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->emax, &pdxp->emax);
        status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->adc_rule, &pdxp->adc_rule);
    }

    /* Initialize the tasks */
    /* setDxpTasks(pdxp); */

    Debug(5, "(init_record): exit\n");
    return(0);

    bad:
    pdxp->pact = 1;
    return(-1);
}


static long cvt_dbaddr(paddr)
struct dbAddr *paddr;
{
   struct dxpRecord *pdxp=(struct dxpRecord *)paddr->precord;
   MODULE_INFO *minfo = &moduleInfo[pdxp->mtyp];

   if (paddr->pfield == &(pdxp->base)) {
      paddr->pfield = (void *)(pdxp->bptr);
      paddr->no_elements = minfo->nbase_histogram;
      paddr->field_type = DBF_LONG;
      paddr->field_size = sizeof(long);
      paddr->dbr_field_type = DBF_LONG;
   } else if (paddr->pfield == &(pdxp->parm)) {
         paddr->pfield = (void *)(pdxp->pptr);
         paddr->no_elements = minfo->nparams;
         paddr->field_type = DBF_USHORT;
         paddr->field_size = sizeof(short);
         paddr->dbr_field_type = DBF_USHORT;
   } else if (paddr->pfield == &(pdxp->bhist)) {
         paddr->pfield = (void *)(pdxp->bhptr);
         paddr->no_elements = minfo->nbase_history;
         paddr->field_type = DBF_LONG;
         paddr->field_size = sizeof(long);
         paddr->dbr_field_type = DBF_LONG;
   } else if (paddr->pfield == &(pdxp->trace)) {
         paddr->pfield = (void *)(pdxp->tptr);
         paddr->no_elements = minfo->ntrace;
         paddr->field_type = DBF_LONG;
         paddr->field_size = sizeof(long);
         paddr->dbr_field_type = DBF_LONG;
   } else {
      Debug(1, "(cvt_dbaddr): field=unknown\n");
   }
   /* Limit size to 4000 for now because of EPICS CA limitations in clients */
   if (paddr->no_elements > 4000) paddr->no_elements = 4000;
   return(0);
}


static long get_array_info(paddr,no_elements,offset)
struct dbAddr *paddr;
long *no_elements;
long *offset;
{
   struct dxpRecord *pdxp=(struct dxpRecord *)paddr->precord;
   MODULE_INFO *minfo = &moduleInfo[pdxp->mtyp];

   if (paddr->pfield == pdxp->bptr) {
      *no_elements = minfo->nbase_histogram;
      *offset = 0;
   } else if (paddr->pfield == pdxp->bhptr) {
      *no_elements =  minfo->nbase_history;
      *offset = 0;
   } else if (paddr->pfield == pdxp->pptr) {
      *no_elements =  minfo->nparams;
      *offset = 0;
   } else if (paddr->pfield == pdxp->tptr) {
      *no_elements =  minfo->ntrace;
      *offset = 0;
   } else {
     Debug(1, "(get_array_info):field=unknown,paddr->pfield=%p,pdxp->bptr=%p\n",
               paddr->pfield, pdxp->bptr);
   }
   /* Limit EPICS size to 4000 longs for now */
   if (*no_elements > 4000) *no_elements = 4000;
   return(0);
}


static long process(pdxp)
        struct dxpRecord        *pdxp;
{
    struct devDxpDset *pdset = (struct devDxpDset *)(pdxp->dset);
    int pact=pdxp->pact;
    int status;

    Debug(5, "dxpRecord(process): entry\n");

    /* Read the parameter information from the DXP */
    status = (*pdset->send_dxp_msg)
             (pdxp,  MSG_DXP_READ_PARAMS, NULL, 0, 0., NULL);
    /* See if device support set pact=true, meaning it will call us back */
    if (!pact && pdxp->pact) return(0);
    pdxp->pact = TRUE;
    
    recGblGetTimeStamp(pdxp);
    monitor(pdxp);
    recGblFwdLink(pdxp);
    pdxp->pact=FALSE;

    Debug(5, "dxpRecord(process): exit\n");
    return(0);
}


static long monitor(struct dxpRecord *pdxp)
{
   int i;
   int offset;
   DXP_SHORT_PARAM *short_param;
   DXP_LONG_PARAM *long_param;
   long long_val;
   unsigned short short_val;
   unsigned short monitor_mask = recGblResetAlarms(pdxp) | DBE_VALUE | DBE_LOG;
   dxpReadbacks *pdxpReadbacks = pdxp->rbptr;
   DXP_TASK_PARAM *task_param;
   MODULE_INFO *minfo = &moduleInfo[pdxp->mtyp];
   unsigned short runtasks;

    Debug(5, "dxpRecord(monitor): entry\n");
   /* Get the value of each parameter, post monitor if it is different
    * from current value */
   /* Address of first short parameter */
   short_param = (DXP_SHORT_PARAM *)&pdxp->a01v;
   for (i=0; i<NUM_SHORT_READ_PARAMS; i++) {
      offset = short_param[i].offset;
      if (offset < 0) continue;
      short_val = pdxp->pptr[offset];
      if(short_param[i].val != short_val) {
         Debug(5, "dxpRecord: New value of short parameter %s\n",
            short_param[i].label);
         Debug(5,"  old (record)=%d\n", short_param[i].val)
         Debug(5,"  new (dxp)=%d\n",short_val);
         short_param[i].val = short_val;

         db_post_events(pdxp,&short_param[i].val, monitor_mask);
      }
   }
   /* Address of first long parameter */
   long_param = (DXP_LONG_PARAM *)&pdxp->s01v;
   for (i=0; i<NUM_DXP_LONG_PARAMS; i++) {
      offset = long_param[i].offset;
      if (offset == -1) continue;
      long_val = (pdxp->pptr[offset]<<16) + pdxp->pptr[offset+1];
      if(long_param[i].val != long_val) {
         Debug(5,"dxpRecord: New value of long parameter %s\n",
            long_param[i].label);
         Debug(5,"  old (record)=%ld", long_param[i].val)
         Debug(5,"  new (dxp)=%ld\n", long_val);
         long_param[i].val = long_val;
         db_post_events(pdxp,&long_param[i].val, monitor_mask);
      }
   }

   /* Set the task menus based on the value of runtasks readback */
   task_param = (DXP_TASK_PARAM *)&pdxp->t01v;
   runtasks = pdxp->pptr[minfo->offsets.runtasks];
   for (i=0; i<NUM_TASK_PARAMS; i++) {
      short_val = ((runtasks & (1 << i)) != 0);
      Debug(5,"dxpRecord: short_val=%d\n", short_val);
      if (task_param[i].val != short_val) {
         Debug(5,"dxpRecord: New value of task parameter %s\n",
            task_param[i].label);
         Debug(5,"  old (record)=%d", task_param[i].val)
         Debug(5,"  new (dxp)=%d\n", short_val);
         task_param[i].val = short_val;
         db_post_events(pdxp,&task_param[i].val, monitor_mask);
      }
   }

   /* Clear special run flags, they are done */
   if (pdxp->read_trace) {
      pdxp->read_trace=0;
      db_post_events(pdxp,&pdxp->read_trace, monitor_mask);
   }
   if (pdxp->read_history) {
      pdxp->read_history=0;
      db_post_events(pdxp,&pdxp->read_history, monitor_mask);
   }
   if (pdxp->open_relay) {
      pdxp->open_relay=0;
      db_post_events(pdxp,&pdxp->open_relay, monitor_mask);
   }
   if (pdxp->close_relay) {
      pdxp->close_relay=0;
      db_post_events(pdxp,&pdxp->close_relay, monitor_mask);
   }

   /* See if readbacks have changed  */ 
   if (pdxp->slow_trig_rbv != pdxpReadbacks->slow_trig) {
      pdxp->slow_trig_rbv = pdxpReadbacks->slow_trig;
      db_post_events(pdxp, &pdxp->slow_trig_rbv, monitor_mask);
   }
   if (pdxp->pktim_rbv != pdxpReadbacks->pktim) {
      pdxp->pktim_rbv = pdxpReadbacks->pktim;
      db_post_events(pdxp, &pdxp->pktim_rbv, monitor_mask);
   };
   if (pdxp->gaptim_rbv != pdxpReadbacks->gaptim) {
      pdxp->gaptim_rbv = pdxpReadbacks->gaptim;
      db_post_events(pdxp, &pdxp->gaptim_rbv, monitor_mask);
   }
   if (pdxp->fast_trig_rbv != pdxpReadbacks->fast_trig) {
      pdxp->fast_trig_rbv = pdxpReadbacks->fast_trig;
      db_post_events(pdxp, &pdxp->fast_trig_rbv, monitor_mask);
   }
   if (pdxp->trig_pktim_rbv != pdxpReadbacks->trig_pktim) {
      pdxp->trig_pktim_rbv = pdxpReadbacks->trig_pktim;
      db_post_events(pdxp, &pdxp->trig_pktim_rbv, monitor_mask);
   }
   if (pdxp->trig_gaptim_rbv != pdxpReadbacks->trig_gaptim) {
      pdxp->trig_gaptim_rbv = pdxpReadbacks->trig_gaptim;
      db_post_events(pdxp, &pdxp->trig_gaptim_rbv, monitor_mask);
   }
   if (pdxp->adc_rule_rbv != pdxpReadbacks->adc_rule) {
      pdxp->adc_rule_rbv = pdxpReadbacks->adc_rule;
      db_post_events(pdxp, &pdxp->adc_rule_rbv, monitor_mask);
   }
   pdxpReadbacks->emax = pdxpReadbacks->mca_bin_width / 1000. * 
                         pdxpReadbacks->number_mca_channels;
   if (pdxp->emax_rbv != pdxpReadbacks->emax) {
      pdxp->emax_rbv = pdxpReadbacks->emax;
      db_post_events(pdxp, &pdxp->emax_rbv, monitor_mask);
   }
   if (pdxp->fast_peaks != pdxpReadbacks->fast_peaks) {
      pdxp->fast_peaks = pdxpReadbacks->fast_peaks;
      db_post_events(pdxp, &pdxp->fast_peaks, monitor_mask);
   }
   if (pdxp->slow_peaks != pdxpReadbacks->slow_peaks) {
      pdxp->slow_peaks = pdxpReadbacks->slow_peaks;
      db_post_events(pdxp, &pdxp->slow_peaks, monitor_mask);
   }
   if (pdxp->icr != pdxpReadbacks->icr) {
      pdxp->icr = pdxpReadbacks->icr;
      db_post_events(pdxp, &pdxp->icr, monitor_mask);
   }
   if (pdxp->ocr != pdxpReadbacks->ocr) {
      pdxp->ocr = pdxpReadbacks->ocr;
      db_post_events(pdxp, &pdxp->ocr, monitor_mask);
   }
                                  
   /* Post events on array fields if they have changed */
   if (pdxpReadbacks->newBaselineHistogram) {
      pdxpReadbacks->newBaselineHistogram = 0;
      db_post_events(pdxp,pdxp->bptr,monitor_mask);
   }
   if (pdxpReadbacks->newBaselineHistory) {
      pdxpReadbacks->newBaselineHistory = 0;
      db_post_events(pdxp,pdxp->bhptr,monitor_mask);
   }
   if (pdxpReadbacks->newAdcTrace) {
      pdxpReadbacks->newAdcTrace = 0;
      db_post_events(pdxp,pdxp->tptr,monitor_mask);
   }

   Debug(5, "dxpRecord(monitor): exit\n");
   return(0);
}

static long special(struct dbAddr *paddr, int after)
/* Called whenever a field is changed */
{
    struct dxpRecord *pdxp=(struct dxpRecord *)paddr->precord;
    struct devDxpDset *pdset = (struct devDxpDset *)(pdxp->dset);
    short offset;
    int i;
    int status;
    DXP_SHORT_PARAM *short_param;
    DXP_TASK_PARAM *task_param;
    int nparams;

    if (!after) return(0);  /* Don't do anything if field not yet changed */

    Debug(5, "dxpRecord(special): entry\n");

    /* Loop through seeing which field was changed.  Write the new value.*/
    /* Address of first short parameter */
    short_param = (DXP_SHORT_PARAM *)&pdxp->a01v;
    for (i=0; i<NUM_SHORT_READ_PARAMS; i++) {
       if (paddr->pfield == (void *) &short_param[i].val) {
          offset = short_param[i].offset;
          if (offset == -1) continue;
          nparams = 1;
          status = (*pdset->send_dxp_msg)
                    (pdxp,  MSG_DXP_SET_SHORT_PARAM, short_param[i].label, 
                    short_param[i].val, 0., NULL);
          Debug(5,
         "dxpRecord: special found new value of short parameter %s, value=%d\n",
                  short_param[i].label, short_param[i].val);
          goto found_param;
       }
    }

    /* Check if the tasks have changed */
    task_param = (DXP_TASK_PARAM *)&pdxp->t01v;
    for (i=0; i<NUM_TASK_PARAMS; i++) {
       if (paddr->pfield == (void *) &task_param[i].val) {
          if (strcmp(task_param[i].label, "Unused") == 0) continue;
          setDxpTasks(pdxp);
          goto found_param;
       }
    }

    /* Special runs */
    if (paddr->pfield == (void *) &pdxp->read_trace) {
         if (pdxp->read_trace) 
             status = (*pdset->send_dxp_msg) (pdxp, MSG_DXP_CONTROL_TASK, 
                       "adc_trace", 0, pdxp->trace_wait*1000., NULL);
         goto found_param;
    }
    if (paddr->pfield == (void *) &pdxp->read_history) {
         if (pdxp->read_history) 
             status = (*pdset->send_dxp_msg) (pdxp, MSG_DXP_CONTROL_TASK, 
                       "baseline_history", 0, 0., NULL);
         goto found_param;
    }
    if (paddr->pfield == (void *) &pdxp->open_relay) {
         if (pdxp->open_relay) 
             status = (*pdset->send_dxp_msg) (pdxp, MSG_DXP_CONTROL_TASK, 
                       "open_input_relay", 0, 0., NULL);
         goto found_param;
    }
    if (paddr->pfield == (void *) &pdxp->close_relay) {
         if (pdxp->close_relay) 
             status = (*pdset->send_dxp_msg) (pdxp, MSG_DXP_CONTROL_TASK, 
                       "close_input_relay", 0, 0., NULL);
         goto found_param;
    }

    /* This must be a DBF_DOUBLE parameter */
    status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0, 
                   *(double *)paddr->pfield, paddr->pfield);

found_param:
    Debug(5, "dxpRecord(special): exit\n");
    return(0);
}


static void setDxpTasks(struct dxpRecord *pdxp)
{
   struct devDxpDset *pdset = (struct devDxpDset *)(pdxp->dset);
   MODULE_INFO *minfo = &moduleInfo[pdxp->mtyp];
   DXP_TASK_PARAM *task_param;
   unsigned short runtasks;
   int i;
   int status;

   Debug(5, "dxpRecord(setDxpTasks): entry\n");
   task_param = (DXP_TASK_PARAM *)&pdxp->t01v;
   runtasks = 0;
   for (i=0; i<NUM_TASK_PARAMS; i++) {
      if (strcmp(task_param[i].label, "Unused") == 0) continue;
      if (task_param[i].val) runtasks |= (1 << i);
   }
   Debug(5, "dxpRecord(setDxpTasks): runtasks=%d, offset=%d\n", 
      runtasks, minfo->offsets.runtasks);
   /* Copy new value to parameter array in case other routines need it
    * before record processes again */
    pdxp->pptr[minfo->offsets.runtasks] = runtasks;
    status = (*pdset->send_dxp_msg)
         (pdxp,  MSG_DXP_SET_SHORT_PARAM, minfo->names[minfo->offsets.runtasks], 
         runtasks, 0., NULL);
   Debug(5, "dxpRecord(setDxpTasks): exit\n");
}

static long get_precision(struct dbAddr *paddr, long *precision)
{
    int fieldIndex = dbGetFieldIndex(paddr);

    switch (fieldIndex) {
        case dxpRecordTRACE_WAIT:
        case dxpRecordICR:
        case dxpRecordOCR:
            *precision = 1;
            break;
        case dxpRecordFAST_TRIG:
        case dxpRecordFAST_TRIG_RBV:
        case dxpRecordSLOW_TRIG:
        case dxpRecordSLOW_TRIG_RBV:
        case dxpRecordPKTIM:
        case dxpRecordPKTIM_RBV:
        case dxpRecordTRIG_PKTIM:
        case dxpRecordTRIG_PKTIM_RBV:
        case dxpRecordTRIG_GAPTIM:
        case dxpRecordTRIG_GAPTIM_RBV:
        case dxpRecordADC_RULE:
        case dxpRecordADC_RULE_RBV:
        case dxpRecordGAPTIM:
        case dxpRecordGAPTIM_RBV:
            *precision = 2;
            break;
        case dxpRecordPGAIN:
        case dxpRecordEMAX:
        case dxpRecordEMAX_RBV:
            *precision = 3;
            break;
        default:
            recGblGetPrec(paddr,precision);
            break;
    }
    return(0);
}

static long get_graphic_double(struct dbAddr *paddr, struct dbr_grDouble *pgd)
{
   struct dxpRecord   *pdxp=(struct dxpRecord *)paddr->precord;
   MODULE_INFO *minfo = &moduleInfo[pdxp->mtyp];
   int fieldIndex = dbGetFieldIndex(paddr);
   int offset, i;
   DXP_SHORT_PARAM *short_param;
   DXP_LONG_PARAM *long_param;

   if (fieldIndex == dxpRecordPGAIN) {
      pgd->upper_disp_limit = 50.;
      pgd->lower_disp_limit = 0;
      return(0);
   }
   if (fieldIndex == dxpRecordEMAX) {
      pgd->upper_disp_limit = 200.;
      pgd->lower_disp_limit = 0;
      return(0);
   }
   if ((fieldIndex < dxpRecordA01V) || (fieldIndex > dxpRecordS13V)) {
      recGblGetGraphicDouble(paddr,pgd);
      return(0);
   } else {
      short_param = (DXP_SHORT_PARAM *)&pdxp->a01v;
      for (i=0; i<NUM_SHORT_READ_PARAMS; i++) {
         if (paddr->pfield == (void *) &short_param[i].val) {
            offset = short_param[i].offset;
            if (offset < 0) continue;
            pgd->upper_disp_limit = minfo->ubound[offset];
            pgd->lower_disp_limit = minfo->lbound[offset];
            return(0);
         }
      }
      long_param = (DXP_LONG_PARAM *)&pdxp->s01v;
      for (i=0; i<NUM_DXP_LONG_PARAMS; i++) {
         if (paddr->pfield == (void *) &long_param[i].val) {
            offset = long_param[i].offset;
            if (offset < 0) continue;
            pgd->upper_disp_limit = minfo->ubound[offset];
            pgd->lower_disp_limit = minfo->lbound[offset];
            return(0);
         }
      }
   }
   return(0);
}


static long get_control_double(struct dbAddr *paddr, struct dbr_ctrlDouble *pcd)
{
   struct dxpRecord   *pdxp=(struct dxpRecord *)paddr->precord;
   int fieldIndex = dbGetFieldIndex(paddr);
   int offset, i;
   MODULE_INFO *minfo = &moduleInfo[pdxp->mtyp];
   DXP_SHORT_PARAM *short_param;
   DXP_LONG_PARAM *long_param;

   if (fieldIndex == dxpRecordPGAIN) {
      pcd->upper_ctrl_limit = 50.;
      pcd->lower_ctrl_limit = 0;
      return(0);
   }
   if (fieldIndex == dxpRecordEMAX) {
      pcd->upper_ctrl_limit = 200.;
      pcd->lower_ctrl_limit = 0;
      return(0);
   }
   if ((fieldIndex < dxpRecordA01V) || (fieldIndex > dxpRecordS13V)) {
      recGblGetControlDouble(paddr,pcd);
      return(0);
   } else {
      short_param = (DXP_SHORT_PARAM *)&pdxp->a01v;
      for (i=0; i<NUM_SHORT_READ_PARAMS; i++) {
         if (paddr->pfield == (void *) &short_param[i].val) {
            offset = short_param[i].offset;
            if (offset < 0) continue;
            pcd->upper_ctrl_limit = minfo->ubound[offset];
            pcd->lower_ctrl_limit = minfo->lbound[offset];
            return(0);
         }
      }
      long_param = (DXP_LONG_PARAM *)&pdxp->s01v;
      for (i=0; i<NUM_DXP_LONG_PARAMS; i++) {
         if (paddr->pfield == (void *) &long_param[i].val) {
            offset = long_param[i].offset;
            if (offset < 0) continue;
            pcd->upper_ctrl_limit = minfo->ubound[offset];
            pcd->lower_ctrl_limit = minfo->lbound[offset];
            return(0);
         }
      }
   }
   return(0);
}

static int getParamOffset(MODULE_INFO *minfo, char *label, short *offset)
{
    int i;

    for (i=0; i<minfo->nparams; i++) {
        if (strcmp(label, minfo->names[i]) == 0) {
            *offset = i;
            return(0);
        }
    }
    return(-1);
}
