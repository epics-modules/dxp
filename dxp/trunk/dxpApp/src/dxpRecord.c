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

/* This is the bit position in RUNTASKS for enable baseline cut */
#define RUNTASKS_BLCUT 0x400

typedef struct {
   unsigned short runtasks;
   unsigned short gaindac;
   unsigned short blmin;
   unsigned short blmax;
   unsigned short blcut;
   unsigned short blfilter;
   unsigned short basethresh;
   unsigned short basethreshadj;
   unsigned short basebinning;
   unsigned short slowlen;
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

typedef struct  {
    long lo;
    long hi;
    long counts;
} DXP_SCA;

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

volatile int dxpRecordDebug = 0;
epicsExportAddress(int, dxpRecordDebug);


static MODULE_INFO moduleInfo[MAX_MODULE_TYPES];
static long monitor(struct dxpRecord *pdxp);
static void setDxpTasks(struct dxpRecord *pdxp);
static int setSCAs(struct dxpRecord *pdxp);
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
    if (dxpRecordDebug > 5) printf("(init_record): entry\n");

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
    status = getParamOffset(minfo, "GAINDAC", &minfo->offsets.gaindac); 
    if (status != 0) {
        printf("dxpRecord:init_record cannot find GAINDAC\n");
        goto bad;
    }
    status = getParamOffset(minfo, "BLMIN", &minfo->offsets.blmin); 
    if (status != 0) {
        printf("dxpRecord:init_record cannot find BLMIN\n");
        goto bad;
    }
    status = getParamOffset(minfo, "BLMAX", &minfo->offsets.blmax); 
    if (status != 0) {
        printf("dxpRecord:init_record cannot find BLMAX\n");
        goto bad;
    }
    status = getParamOffset(minfo, "BLCUT", &minfo->offsets.blcut); 
    if (status != 0) {
        printf("dxpRecord:init_record cannot find BLCUT\n");
        goto bad;
    }
    status = getParamOffset(minfo, "BLFILTER", &minfo->offsets.blfilter); 
    if (status != 0) {
        printf("dxpRecord:init_record cannot find BLFILTER\n");
        goto bad;
    }
    status = getParamOffset(minfo, "BASETHRESH", &minfo->offsets.basethresh); 
    if (status != 0) {
        printf("dxpRecord:init_record cannot find BASETHRESH\n");
        goto bad;
    }
    status = getParamOffset(minfo, "BASTHRADJ", &minfo->offsets.basethreshadj); 
    if (status != 0) {
        printf("dxpRecord:init_record cannot find BASTHRADJ\n");
        goto bad;
    }
    status = getParamOffset(minfo, "BASEBINNING", &minfo->offsets.basebinning); 
    if (status != 0) {
        printf("dxpRecord:init_record cannot find BASEBINNING\n");
        goto bad;
    }
    status = getParamOffset(minfo, "SLOWLEN", &minfo->offsets.slowlen); 
    if (status != 0) {
        printf("dxpRecord:init_record cannot find SLOWLEN\n");
        goto bad;
    }

    /* Allocate the space for the parameter array */
    pdxp->pptr = (unsigned short *)calloc(minfo->nparams, sizeof(short));

    /* Allocate the space for the baseline histogram array */
    pdxp->bptr = (long *)calloc(minfo->nbase_histogram, sizeof(long));

    /* Allocate the space for the baseline cut array */
    pdxp->bcptr = (long *)calloc(minfo->nbase_histogram, sizeof(long));

    /* Allocate the space for the baseline histogram X array */
    pdxp->bxptr = (float *)calloc(minfo->nbase_histogram, sizeof(float));

    /* Allocate the space for the baseline history array */
    pdxp->bhptr = (long *)calloc(minfo->nbase_history, sizeof(long));

    /* Allocate the space for the baseline history X array */
    pdxp->bhxptr = (float *)calloc(minfo->nbase_history, sizeof(float));

    /* Allocate the space for the trace array */
    pdxp->tptr = (long *)calloc(minfo->ntrace, sizeof(long));

    /* Allocate the space for the trace X array */
    pdxp->txptr = (float *)calloc(minfo->ntrace, sizeof(float));
    /* Compute the trace X array */
    for (i=0; i<minfo->ntrace; i++) {
        pdxp->txptr[i] = pdxp->trace_wait * i;
    }

    /* Allocate the space for the readback structure */
    pdxp->rbptr = (void *)calloc(1, sizeof(dxpReadbacks));

    /* Some higher-level initialization can be done here, i.e. things which don't
     * require information on the current device parameters.  Things which do
     * require information on the current device parameters must be done in process,
     * after the first time the device is read */
                                  
    /* Set default SCAs.  Must do this first, else get errors because other
     * calls will try to read SCAs, which Handel will complain it cannot find */
    setSCAs(pdxp);

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
                   pdxp->adc_rule, &pdxp->adc_rule);
        /* Must do this at the end */
        status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->emax, &pdxp->emax);
    }

    /* Initialize the tasks */
    /* setDxpTasks(pdxp); */

    if (dxpRecordDebug > 5) printf("(init_record): exit\n");
    return(0);

    bad:
    pdxp->pact = 1;
    return(-1);
}


static long cvt_dbaddr(struct dbAddr *paddr)
{
   struct dxpRecord *pdxp=(struct dxpRecord *)paddr->precord;
   MODULE_INFO *minfo = &moduleInfo[pdxp->mtyp];

   if (paddr->pfield == &(pdxp->base)) {
      paddr->pfield = (void *)(pdxp->bptr);
      paddr->no_elements = minfo->nbase_histogram;
      paddr->field_type = DBF_LONG;
      paddr->field_size = sizeof(long);
      paddr->dbr_field_type = DBF_LONG;
   } else if (paddr->pfield == &(pdxp->base_cut)) {
      paddr->pfield = (void *)(pdxp->bcptr);
      paddr->no_elements = minfo->nbase_histogram;
      paddr->field_type = DBF_LONG;
      paddr->field_size = sizeof(long);
      paddr->dbr_field_type = DBF_LONG;
   } else if (paddr->pfield == &(pdxp->base_x)) {
      paddr->pfield = (void *)(pdxp->bxptr);
      paddr->no_elements = minfo->nbase_histogram;
      paddr->field_type = DBF_FLOAT;
      paddr->field_size = sizeof(float);
      paddr->dbr_field_type = DBF_FLOAT;
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
   } else if (paddr->pfield == &(pdxp->bhist_x)) {
      paddr->pfield = (void *)(pdxp->bhxptr);
      paddr->no_elements = minfo->nbase_history;
      paddr->field_type = DBF_FLOAT;
      paddr->field_size = sizeof(float);
      paddr->dbr_field_type = DBF_FLOAT;
   } else if (paddr->pfield == &(pdxp->trace)) {
      paddr->pfield = (void *)(pdxp->tptr);
      paddr->no_elements = minfo->ntrace;
      paddr->field_type = DBF_LONG;
      paddr->field_size = sizeof(long);
      paddr->dbr_field_type = DBF_LONG;
   } else if (paddr->pfield == &(pdxp->trace_x)) {
      paddr->pfield = (void *)(pdxp->txptr);
      paddr->no_elements = minfo->ntrace;
      paddr->field_type = DBF_FLOAT;
      paddr->field_size = sizeof(float);
      paddr->dbr_field_type = DBF_FLOAT;
   } else {
      if (dxpRecordDebug > 1) printf("(cvt_dbaddr): field=unknown\n");
   }
   /* Limit size to 4000 for now because of EPICS CA limitations in clients */
   if (paddr->no_elements > 4000) paddr->no_elements = 4000;
   return(0);
}


static long get_array_info(struct dbAddr *paddr, long *no_elements, long *offset)
{
   struct dxpRecord *pdxp=(struct dxpRecord *)paddr->precord;
   MODULE_INFO *minfo = &moduleInfo[pdxp->mtyp];

   if (paddr->pfield == pdxp->bptr) {
      *no_elements = minfo->nbase_histogram;
      *offset = 0;
   } else if (paddr->pfield == pdxp->bcptr) {
      *no_elements = minfo->nbase_histogram;
      *offset = 0;
   } else if (paddr->pfield == pdxp->bxptr) {
      *no_elements = minfo->nbase_histogram;
      *offset = 0;
   } else if (paddr->pfield == pdxp->bhptr) {
      *no_elements =  minfo->nbase_history;
      *offset = 0;
   } else if (paddr->pfield == pdxp->bhxptr) {
      *no_elements =  minfo->nbase_history;
      *offset = 0;
   } else if (paddr->pfield == pdxp->pptr) {
      *no_elements =  minfo->nparams;
      *offset = 0;
   } else if (paddr->pfield == pdxp->tptr) {
      *no_elements =  minfo->ntrace;
      *offset = 0;
   } else if (paddr->pfield == pdxp->txptr) {
      *no_elements =  minfo->ntrace;
      *offset = 0;
   } else {
     if (dxpRecordDebug > 1)
         printf("(get_array_info):field=unknown,paddr->pfield=%p,pdxp->bptr=%p\n",
                paddr->pfield, pdxp->bptr);
   }
   /* Limit EPICS size to 4000 longs for now */
   if (*no_elements > 4000) *no_elements = 4000;
   return(0);
}


static long process(struct dxpRecord *pdxp)
{
    struct devDxpDset *pdset = (struct devDxpDset *)(pdxp->dset);
    int pact=pdxp->pact;
    int status;

    if (dxpRecordDebug > 5) printf("dxpRecord(process): entry\n");

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

    if (dxpRecordDebug > 5) printf("dxpRecord(process): exit\n");
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
   DXP_SCA *sca;
   MODULE_INFO *minfo = &moduleInfo[pdxp->mtyp];
   int blCutEnbl;
   int newBlCut;
   int runtasks       = pdxp->pptr[minfo->offsets.runtasks];
   int blmin          = (short)pdxp->pptr[minfo->offsets.blmin];
   int blmax          = (short)pdxp->pptr[minfo->offsets.blmax];
   int blcut          = pdxp->pptr[minfo->offsets.blcut];
   int blfilter       = pdxp->pptr[minfo->offsets.blfilter];
   int basethresh     = pdxp->pptr[minfo->offsets.basethresh];
   int basethreshadj  = pdxp->pptr[minfo->offsets.basethreshadj];
   int basebinning    = pdxp->pptr[minfo->offsets.basebinning];
   int slowlen        = pdxp->pptr[minfo->offsets.slowlen];
   double eVPerADC;
   double eVPerBin;

    if (dxpRecordDebug > 5) printf("dxpRecord(monitor): entry\n");
   /* Get the value of each parameter, post monitor if it is different
    * from current value */
   /* Address of first short parameter */
   short_param = (DXP_SHORT_PARAM *)&pdxp->a01v;
   for (i=0; i<NUM_SHORT_READ_PARAMS; i++) {
      offset = short_param[i].offset;
      if (offset < 0) continue;
      short_val = pdxp->pptr[offset];
      if(short_param[i].val != short_val) {
         if (dxpRecordDebug > 5) printf("dxpRecord: New value of short parameter %s\n",
            short_param[i].label);
         if (dxpRecordDebug > 5) printf("  old (record)=%d\n", short_param[i].val);
         if (dxpRecordDebug > 5) printf("  new (dxp)=%d\n",short_val);
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
         if (dxpRecordDebug > 5) printf("dxpRecord: New value of long parameter %s\n",
            long_param[i].label);
         if (dxpRecordDebug > 5) printf("  old (record)=%ld", long_param[i].val);
         if (dxpRecordDebug > 5) printf("  new (dxp)=%ld\n", long_val);
         long_param[i].val = long_val;
         db_post_events(pdxp,&long_param[i].val, monitor_mask);
      }
   }

   /* Address of first SCA */
   sca = (DXP_SCA *)&pdxp->sca0_lo;
   for (i=0; i<NUM_DXP_SCAS; i++) {
      if (pdxpReadbacks->sca_lo[i] != sca[i].lo) {
         sca[i].lo = pdxpReadbacks->sca_lo[i];
         db_post_events(pdxp, &sca[i].lo, monitor_mask);   
      }
      if (pdxpReadbacks->sca_hi[i] != sca[i].hi) {
         sca[i].hi = pdxpReadbacks->sca_hi[i];
         db_post_events(pdxp, &sca[i].hi, monitor_mask);   
      }
      if (pdxpReadbacks->sca_counts[i] != sca[i].counts) {
         sca[i].counts = pdxpReadbacks->sca_counts[i];
         db_post_events(pdxp, &sca[i].counts, monitor_mask);   
      }
   }

   /* Set the task menus based on the value of runtasks readback */
   task_param = (DXP_TASK_PARAM *)&pdxp->t01v;
   for (i=0; i<NUM_TASK_PARAMS; i++) {
      short_val = ((runtasks & (1 << i)) != 0);
      if (dxpRecordDebug > 5) printf("dxpRecord: short_val=%d\n", short_val);
      if (task_param[i].val != short_val) {
         if (dxpRecordDebug > 5) printf("dxpRecord: New value of task parameter %s\n",
            task_param[i].label);
         if (dxpRecordDebug > 5) printf("  old (record)=%d", task_param[i].val);
         if (dxpRecordDebug > 5) printf("  new (dxp)=%d\n", short_val);
         task_param[i].val = short_val;
         db_post_events(pdxp,&task_param[i].val, monitor_mask);
      }
   }

   /* Clear special run flags, they are done */
   if (pdxp->read_histogram) {
      pdxp->read_histogram=0;
      db_post_events(pdxp,&pdxp->read_histogram, monitor_mask);
   }
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
   if (pdxp->num_scas != pdxpReadbacks->number_scas) {
      pdxp->num_scas = pdxpReadbacks->number_scas;
      db_post_events(pdxp, &pdxp->num_scas, monitor_mask);
   }
   if (pdxp->acqg != pdxpReadbacks->acquiring) {
      pdxp->acqg = pdxpReadbacks->acquiring;
      db_post_events(pdxp, &pdxp->acqg, monitor_mask);
   }
                                  
   /* Post events on array fields if they have changed */
   if (pdxpReadbacks->newBaselineHistogram) {
      pdxpReadbacks->newBaselineHistogram = 0;
      db_post_events(pdxp,pdxp->bptr,monitor_mask);
      /* See if the x-axis has also changed */
      eVPerADC = 1000.*pdxp->emax/2. / (pdxp->adc_rule/100. * 1024);
      eVPerBin = ((1 << basebinning) / (slowlen * 4.0)) * eVPerADC;
      if (eVPerBin != pdxpReadbacks->eVPerBin) {
         pdxpReadbacks->eVPerBin = eVPerBin;
         for (i=0; i<minfo->nbase_histogram; i++) {
            pdxp->bxptr[i] = (i-512) * eVPerBin / 1000.;
         }
         db_post_events(pdxp,pdxp->bxptr,monitor_mask);
      }
   }
   if (pdxpReadbacks->newBaselineHistory) {
      pdxpReadbacks->newBaselineHistory = 0;
      db_post_events(pdxp,pdxp->bhptr,monitor_mask);
   }
   if (pdxpReadbacks->newAdcTrace) {
      pdxpReadbacks->newAdcTrace = 0;
      db_post_events(pdxp,pdxp->tptr,monitor_mask);
   }
   /* If BLMIN, BLMAX, or RUNTASKS bit for baseline cut have changed then
    * recompute the BASE_CUT array */
   blCutEnbl = (runtasks & RUNTASKS_BLCUT) != 0;
   newBlCut = 0;
   if (blCutEnbl != pdxpReadbacks->blCutEnbl) {
      newBlCut = 1;
      pdxpReadbacks->blCutEnbl = blCutEnbl;
   }
   if (blmin != pdxpReadbacks->blmin) {
      newBlCut = 1;
      pdxpReadbacks->blmin = blmin;
   }
   if (blmax != pdxpReadbacks->blmax) {
      newBlCut = 1;
      pdxpReadbacks->blmax = blmax;
   }
   if (newBlCut) {
      for (i=0; i<minfo->nbase_histogram; i++) pdxp->bcptr[i] = 1;
      if (blCutEnbl) {
         blmin = pdxpReadbacks->blmin / (1 << basebinning);
         blmin += 512;
         if (blmin < 0) blmin = 0;
         blmax = pdxpReadbacks->blmax / (1 << basebinning);
         blmax += 512;
         if (blmax > minfo->nbase_histogram) blmax = minfo->nbase_histogram;
         for (i=blmin; i<blmax; i++) {
            pdxp->bcptr[i] = 65535;
         }
      }
      db_post_events(pdxp,pdxp->bcptr,monitor_mask);
   }
   if (blcut != pdxpReadbacks->blcut) {
      pdxpReadbacks->blcut = blcut;
      pdxp->base_cut_pct = pdxpReadbacks->blcut / 32768. * 100.;
      db_post_events(pdxp,&pdxp->base_cut_pct,monitor_mask);
   }
   if (blfilter != pdxpReadbacks->blfilter) {
      pdxpReadbacks->blfilter = blfilter;
      pdxp->base_len = 32768. / blfilter;
      db_post_events(pdxp,&pdxp->base_len,monitor_mask);
   }
   if (basethresh != pdxp->base_thresh) {
      /* Next release of Handel will have a baseline_threshold parameter */
      pdxp->base_thresh = basethresh;
      db_post_events(pdxp,&pdxp->base_thresh,monitor_mask);
   }
   if (basethreshadj != pdxp->base_threshadj) {
      /* Next release of Handel will have a baseline_threshadj parameter */
      pdxp->base_threshadj = basethreshadj;
      db_post_events(pdxp,&pdxp->base_threshadj,monitor_mask);
   }

   if (dxpRecordDebug > 5) printf("dxpRecord(monitor): exit\n");
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
    unsigned short monitor_mask = DBE_VALUE | DBE_LOG;
    DXP_SHORT_PARAM *short_param;
    DXP_TASK_PARAM *task_param;
    int nparams;
    DXP_SCA *sca;
    MODULE_INFO *minfo = &moduleInfo[pdxp->mtyp];

    if (!after) return(0);  /* Don't do anything if field not yet changed */

    if (dxpRecordDebug > 5) printf("dxpRecord(special): entry\n");

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
          if (dxpRecordDebug > 5)
             printf("dxpRecord: special found new value of short parameter %s, value=%d\n",
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

    /* Check if the SCAS have changed */
    sca = (DXP_SCA *)&pdxp->sca0_lo;
    for (i=0; i<NUM_DXP_SCAS; i++) {
       if ((paddr->pfield == (void *) &sca[i].lo) ||
           (paddr->pfield == (void *) &sca[i].hi) ||
           (paddr->pfield == (void *) &pdxp->num_scas)) {
          setSCAs(pdxp);
          goto found_param;
       }
    }

    /* Special runs */
    if (paddr->pfield == (void *) &pdxp->read_histogram) {
         if (pdxp->read_histogram) 
             status = (*pdset->send_dxp_msg) (pdxp, MSG_DXP_READ_BASELINE, 
                       NULL, 0, 0., NULL);
         goto found_param;
    }
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
    if (paddr->pfield == (void *) &pdxp->strt) {
         if (pdxp->strt) 
             status = (*pdset->send_dxp_msg) (pdxp, MSG_DXP_START_RUN, 
                       NULL, 0, 0., NULL);
         goto found_param;
    }
    if (paddr->pfield == (void *) &pdxp->stop) {
         if (pdxp->stop) 
             status = (*pdset->send_dxp_msg) (pdxp, MSG_DXP_STOP_RUN, 
                       NULL, 0, 0., NULL);
         goto found_param;
    }

    /* Trace wait, recompute X axis for ADC trace */
    if (paddr->pfield == (void *) &pdxp->trace_wait) {
         /* Recompute the trace_x array, erase trace array */
         for (i=0; i<minfo->ntrace; i++) {
            pdxp->tptr[i] = 0;
            pdxp->txptr[i] = pdxp->trace_wait * i;
         }
         db_post_events(pdxp,pdxp->tptr,monitor_mask);
         db_post_events(pdxp,pdxp->txptr,monitor_mask);
         goto found_param;
    }
  

    /* This must be a DBF_DOUBLE parameter */
    status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0, 
                   *(double *)paddr->pfield, paddr->pfield);

found_param:
    if (dxpRecordDebug > 5) printf("dxpRecord(special): exit\n");
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

   if (dxpRecordDebug > 5) printf("dxpRecord(setDxpTasks): entry\n");
   task_param = (DXP_TASK_PARAM *)&pdxp->t01v;
   runtasks = 0;
   for (i=0; i<NUM_TASK_PARAMS; i++) {
      if (strcmp(task_param[i].label, "Unused") == 0) continue;
      if (task_param[i].val) runtasks |= (1 << i);
   }
   if (dxpRecordDebug > 5) printf("dxpRecord(setDxpTasks): runtasks=%d, offset=%d\n", 
      runtasks, minfo->offsets.runtasks);
   /* Copy new value to parameter array in case other routines need it
    * before record processes again */
    pdxp->pptr[minfo->offsets.runtasks] = runtasks;
    status = (*pdset->send_dxp_msg)
         (pdxp,  MSG_DXP_SET_SHORT_PARAM, minfo->names[minfo->offsets.runtasks], 
         runtasks, 0., NULL);
   if (dxpRecordDebug > 5) printf("dxpRecord(setDxpTasks): exit\n");
}


static int setSCAs(struct dxpRecord *pdxp)
{
    int i;
    DXP_SCA *sca = (DXP_SCA *)&pdxp->sca0_lo;
    dxpReadbacks *pdxpReadbacks = pdxp->rbptr;
    struct devDxpDset *pdset = (struct devDxpDset *)(pdxp->dset);
    int status;

    pdxp->num_scas = 0;
    for (i=0; i<NUM_DXP_SCAS; i++) {
        /* We would like to only program actual SCAs, but Handel has bug,
         * must set them all. */
        /* if ((sca[i].lo < 0) || (sca[i].hi < 0)) break; */
        if (sca[i].lo < 0) sca[i].lo = 0;
        if (sca[i].hi < 0) sca[i].hi = 0;
        pdxpReadbacks->sca_lo[i] = sca[i].lo;
        pdxpReadbacks->sca_hi[i] = sca[i].hi;
        pdxp->num_scas++;
    }
    status = (*pdset->send_dxp_msg)
              (pdxp,  MSG_DXP_SET_SCAS, 
              NULL, 0, (double)pdxp->num_scas, NULL);
    return(status);
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
        case dxpRecordBASE_CUT_PCT:
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
