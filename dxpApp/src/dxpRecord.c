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
#include        "dxp.h"
#include        "devDxp.h"
#include        "xerxes_structures.h"
#include        "xerxes.h"


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
static long get_enum_str();
static long get_enum_strs();
static long put_enum_str();
static long get_graphic_double();
static long get_control_double();
#define get_alarm_double NULL

static long monitor(struct dxpRecord *pdxp);
static void setDxpTasks(struct dxpRecord *pdxp);
static void setPeakingTime(struct dxpRecord *pdxp);
static void setPeakingTimeStrings(struct dxpRecord *pdxp);
static void setPeakingTimeRangeStrings(struct dxpRecord *pdxp);
static void setGain(struct dxpRecord *pdxp);
static void setBinWidth(struct dxpRecord *pdxp);
static void setFippi(struct dxpRecord *pdxp);

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
#define NUM_BASELINE_PARAMS  6 /* The number of short BASELINE parameters 
                                * described above in the record */
#define NUM_READONLY_PARAMS 15 /* The number of short READ-ONLY parameters 
                                * described above in the record */
#define NUM_DXP_LONG_PARAMS 14 /* The number of long (32 bit) statistics 
                                * parameters described above in the record. 
                                * These are also read-only */
#define NUM_TASK_PARAMS     11 /* The number of task parameters described above
                                *  in the record. */
 

#define NUM_SHORT_WRITE_PARAMS \
   (NUM_ASC_PARAMS + NUM_FIPPI_PARAMS + NUM_DSP_PARAMS + NUM_BASELINE_PARAMS)
#define NUM_SHORT_READ_PARAMS (NUM_SHORT_WRITE_PARAMS +  NUM_READONLY_PARAMS)

#define MAX_PEAKING_TIMES 5
static int peakingTimes[MAX_PEAKING_TIMES] = {5,10,15,20,25};
#define MAX_DECIMATIONS 4
static int allDecimations[MAX_DECIMATIONS] = {0,2,4,6};

/* Debug support */
#if 0
#define Debug(l,FMT,V) ;
#else
#define Debug(l,FMT,V...) {if (l <= dxpRecordDebug) \
                          { printf("%s(%d):",__FILE__,__LINE__); \
                            printf(FMT,##V);}}
#endif
volatile int dxpRecordDebug = 0;
epicsExportAddress(int, dxpRecordDebug);

typedef struct {
   unsigned short runtasks;
   unsigned short decimation;
   unsigned short slowlen;
   unsigned short slowgap;
   unsigned short peaksamp;
   unsigned short peakint;
   unsigned short gaindac;
   unsigned short binfact;
   unsigned short mcalimhi;
} PARAM_OFFSETS;

typedef struct {
   unsigned short *access;
   unsigned short *lbound;
   unsigned short *ubound;
   unsigned short nsymbols;
   double clock;
   double adc_gain;
   unsigned int nbase;
   PARAM_OFFSETS offsets;
} MODULE_INFO;

static MODULE_INFO moduleInfo[MAX_MODULE_TYPES];



static long init_record(struct dxpRecord *pdxp, int pass)
{
    struct devDxpDset *pdset;
    DXP_SHORT_PARAM *short_param;
    DXP_LONG_PARAM *long_param;
    int i;
    int module;
    MODULE_INFO *minfo;
    unsigned short offset;
    int status;
    int download;
    char boardString[MAXBOARDNAME_LEN];


    if (pass != 0) return(0);
    Debug(5, "(init_record): entry\n");

    /* must have dset defined */
    if (!(pdset = (struct devDxpDset *)(pdxp->dset))) {
        recGblRecordError(S_dev_noDSET,(void *)pdxp,"dxp: init_record1");
        return(S_dev_noDSET);
    }
    /* must have read_array function defined */
    if ( (pdset->number < 7) || (pdset->read_array == NULL) ) {
        recGblRecordError(S_dev_missingSup,(void *)pdxp,"dxp: init_record2");
        printf("%ld %p\n",pdset->number, pdset->read_array);
        return(S_dev_missingSup);
    }
    if (pdset->init_record) {
        if ((status=(*pdset->init_record)(pdxp, &module))) return(status);
    }

    /* Initialize values for each type of module */
    moduleInfo[MODEL_DXP4C].clock = .050;
    moduleInfo[MODEL_DXP2X].clock = .025;
    moduleInfo[MODEL_DXPX10P].clock = .050;
    moduleInfo[MODEL_DXP4C].adc_gain = 1024./1000.;  /* ADC bits/mV */
    moduleInfo[MODEL_DXP2X].adc_gain = 1024./1000.;
    moduleInfo[MODEL_DXPX10P].adc_gain = 1024./1000.;

    /* Figure out what kind of module this is */
    dxp_get_board_type(&module, boardString);
    if (strcmp(boardString, "dxp4c") == 0) pdxp->mtyp        = MODEL_DXP4C;
    else if (strcmp(boardString, "dxp4c2x") == 0) pdxp->mtyp = MODEL_DXP2X;
    else if (strcmp(boardString, "dxpx10p") == 0) pdxp->mtyp = MODEL_DXPX10P;
    else Debug(1, "(init_record), unknown board type\n");
    minfo = &moduleInfo[pdxp->mtyp];

    /* If minfo->nsymbols=0 then this is the first DXP record of this 
     * module type, allocate global (not record instance) structures */
    if (minfo->nsymbols == 0) {
        dxp_max_symbols(&module, &minfo->nsymbols);
        Debug(5, "(init_record): allocating memory, nsymbols=%d\n", minfo->nsymbols);
        dxp_nbase(&module, &minfo->nbase);
        minfo->access =
               (unsigned short *) malloc(minfo->nsymbols * 
                                         sizeof(unsigned short));
        minfo->lbound =
               (unsigned short *) malloc(minfo->nsymbols * 
                                         sizeof(unsigned short));
        minfo->ubound =
               (unsigned short *) malloc(minfo->nsymbols * 
                                         sizeof(unsigned short));
        dxp_symbolname_limits(&module, minfo->access, minfo->lbound, 
                               minfo->ubound);
    }

    /* Download the short parameter fields if 
        * 1) pini is true and 
        * 2) d01v field is non-zero.  This is a sanity check that
        *    save-restore has restored reasonable values to the record */
    download = (pdxp->pini && pdxp->d01v);
    /* Look up the address of each parameter */
    /* Address of first short parameter */
    short_param = (DXP_SHORT_PARAM *)&pdxp->a01v;
    for (i=0; i<NUM_SHORT_READ_PARAMS; i++) {
       if (strcmp(short_param[i].label, "Unused") == 0)
          offset=-1;
       else {
          dxp_get_symbol_index(&module, short_param[i].label, &offset);
          if (download) status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_SHORT_PARAM, offset, short_param[i].val);
       }
       short_param[i].offset = offset;
    }
    /* Address of first long parameter */
    long_param = (DXP_LONG_PARAM *)&pdxp->s01v;
    for (i=0; i<NUM_DXP_LONG_PARAMS; i++) {
       if (strcmp(long_param[i].label, "Unused") == 0)
          offset=-1;
       else
          dxp_get_symbol_index(&module, long_param[i].label, &offset);
       long_param[i].offset = offset;
    }

    /* Get the offsets of specific parameters that the record will use. */
    dxp_get_symbol_index(&module, "RUNTASKS",   &minfo->offsets.runtasks);
    dxp_get_symbol_index(&module, "DECIMATION", &minfo->offsets.decimation);
    dxp_get_symbol_index(&module, "SLOWLEN",    &minfo->offsets.slowlen);
    dxp_get_symbol_index(&module, "SLOWGAP",    &minfo->offsets.slowgap);
    dxp_get_symbol_index(&module, "PEAKINT",    &minfo->offsets.peakint);
    dxp_get_symbol_index(&module, "MCALIMHI",    &minfo->offsets.mcalimhi);
    /* For some reason the DXP2X uses PEAKSAM rather than PEAKSAMP */
    switch (pdxp->mtyp) {
       case MODEL_DXP4C:
          dxp_get_symbol_index(&module, "PEAKSAMP",   &minfo->offsets.peaksamp);
          dxp_get_symbol_index(&module, "BINFACT",   &minfo->offsets.binfact);
          break;
       case MODEL_DXP2X:
       case MODEL_DXPX10P:
          dxp_get_symbol_index(&module, "PEAKSAM",   &minfo->offsets.peaksamp);
          dxp_get_symbol_index(&module, "GAINDAC",   &minfo->offsets.gaindac);
          dxp_get_symbol_index(&module, "BINFACT1",   &minfo->offsets.binfact);
          break;
    }

    /* Allocate the space for the baseline array */
    pdxp->bptr = (unsigned short *)calloc(minfo->nbase, sizeof(unsigned short));

    /* Allocate the space for the parameter array */
    pdxp->pptr = (unsigned short *)calloc(minfo->nsymbols, 
                                          sizeof(unsigned short));

    /* Allocate space for the peaking time strings */
    pdxp->pkl = (char *)calloc(DXP_STRING_SIZE * MAX_PEAKING_TIMES, 
                               sizeof(char));
    pdxp->pkrl = (char *)calloc(DXP_STRING_SIZE * MAX_DECIMATIONS, 
                               sizeof(char));

    /* Some higher-level initialization can be done here, i.e. things which don't
     * require information on the current device parameters.  Things which do
     * require information on the current device parameters must be done in process,
     * after the first time the device is read */
                                  
    /* Create the peaking time range strings */
    setPeakingTimeRangeStrings(pdxp);
   
    /* Set the gain */
    setGain(pdxp);

    /* Set the FiPPI file */
    setFippi(pdxp);
 
    /* Initialize the tasks */
    setDxpTasks(pdxp);

    Debug(5, "(init_record): exit\n");
    return(0);
}


static long cvt_dbaddr(paddr)
struct dbAddr *paddr;
{
   struct dxpRecord *pdxp=(struct dxpRecord *)paddr->precord;
   MODULE_INFO *minfo = &moduleInfo[pdxp->mtyp];

   if (paddr->pfield == &(pdxp->base)) {
      paddr->pfield = (void *)(pdxp->bptr);
      paddr->no_elements = minfo->nbase;
      paddr->field_type = DBF_USHORT;
      paddr->field_size = sizeof(short);
      paddr->dbr_field_type = DBF_USHORT;
      Debug(5, "(cvt_dbaddr): field=base\n");
   } else if (paddr->pfield == &(pdxp->parm)) {
         paddr->pfield = (void *)(pdxp->pptr);
         paddr->no_elements = minfo->nsymbols;
         paddr->field_type = DBF_USHORT;
         paddr->field_size = sizeof(short);
         paddr->dbr_field_type = DBF_USHORT;
         Debug(5, "(cvt_dbaddr): field=parm\n");
   } else {
      Debug(1, "(cvt_dbaddr): field=unknown\n");
   }
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
      *no_elements = minfo->nbase;
      *offset = 0;
      Debug(5, "(get_array_info): field=base\n");
   } else if (paddr->pfield == pdxp->pptr) {
      *no_elements =  minfo->nsymbols;
      *offset = 0;
      Debug(5, "(get_array_info): field=parm\n");
   } else {
     Debug(1, "(get_array_info):field=unknown,paddr->pfield=%p,pdxp->bptr=%p\n",
               paddr->pfield, pdxp->bptr);
   }

   return(0);
}


static long process(pdxp)
        struct dxpRecord        *pdxp;
{
    struct devDxpDset *pdset = (struct devDxpDset *)(pdxp->dset);
    int pact=pdxp->pact;
    int status;

    Debug(5, "dxpRecord(process): entry\n");

    /*** Check existence of device support ***/
    if ( (pdset==NULL) || (pdset->read_array==NULL) ) {
        pdxp->pact=TRUE;
        recGblRecordError(S_dev_missingSup,(void *)pdxp,"read_array");
        return(S_dev_missingSup);
    }

    /* Read the parameter information from the DXP */
    status = (*pdset->read_array)(pdxp);
    /* See if device support set pact=true, meaning it will call us back */
    if (!pact && pdxp->pact) return(0);
    pdxp->pact = TRUE;
    
    if (pdxp->udf==2) {
       /* If device support set udf=2 this is a flag to indicate that the device
        * has been succesfully read for the first time.  We do some additional
        * higher-level initialization that requires information about the device
        * parameters */
       
       /* Set udf to 0 */
       pdxp->udf=0;
       
       /* Create the peaking time strings */
       setPeakingTimeStrings(pdxp);

       /* Set the peaking time */
       setPeakingTime(pdxp);

       /* Set the bin width */
       setBinWidth(pdxp);
    }

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
   DXP_TASK_PARAM *task_param;
   unsigned short short_val;
   unsigned short runtasks;
   long long_val;
   unsigned short monitor_mask = recGblResetAlarms(pdxp) | DBE_VALUE | DBE_LOG;
   MODULE_INFO *minfo = &moduleInfo[pdxp->mtyp];

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

         /* Check for changes to parameters we need to handle in the record */
         if (offset == minfo->offsets.decimation) {
            /* Decimation has changed, construct peaking time strings */
            setPeakingTimeStrings(pdxp);
         }
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

   /* If rcal==1 then clear it, since the calibration is done */
   if (pdxp->rcal) {
      pdxp->rcal=0;
      db_post_events(pdxp,&pdxp->rcal, monitor_mask);
   }

   /* If GAINDAC has changed then recompute GAIN */   
   switch (pdxp->mtyp) {
      unsigned short gaindac;
      float gain;
      case MODEL_DXP2X:
      case MODEL_DXPX10P:
         /* See comments in function setGain below *
          * Gain = 0.5*10^((GAINDAC/65536)*40/20) */
         gaindac = pdxp->pptr[minfo->offsets.gaindac];
         gain = 0.5*pow(10., (gaindac/32768.));
         if (gain != pdxp->gainrbv) {
            pdxp->gainrbv = gain;
            db_post_events(pdxp,&pdxp->gainrbv, monitor_mask);
         }
         break;
      case MODEL_DXP4C:
         /* Work needed here !!! set coarse gain, fine gain?*/
         break;
   }

   /* See if EMAX has changed  */   
   switch (pdxp->mtyp) {
      unsigned short slowlen;
      unsigned short binfact1;
      float binwidth;
      float emax;
      case MODEL_DXP2X:
      case MODEL_DXPX10P:
         slowlen = pdxp->pptr[minfo->offsets.slowlen];
         binfact1 = pdxp->pptr[minfo->offsets.binfact];
         binwidth = binfact1 / (pdxp->pgain * pdxp->gain * minfo->adc_gain * 4. * 
                    slowlen);
         emax = binwidth * pdxp->pptr[minfo->offsets.mcalimhi];
         if (emax != pdxp->emaxrbv) {
            pdxp->emaxrbv = emax;
            db_post_events(pdxp,&pdxp->emaxrbv, monitor_mask);
         }
         break;
      case MODEL_DXP4C:
         /* Work needed here !!!*/
         break;
   }

   /* Post events on the baseline histogram field */
    db_post_events(pdxp,pdxp->bptr,monitor_mask);
   

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
                   (pdxp,  MSG_DXP_SET_SHORT_PARAM, offset, short_param[i].val);
          Debug(5,
         "dxpRecord: special found new value of short parameter %s, value=%d\n",
                  short_param[i].label, short_param[i].val);
          goto found_param;
       }
    }

    /* The field was not a simple parameter */
     if (paddr->pfield == (void *) &pdxp->strt) {
        if (pdxp->strt != 0) {
            status = (*pdset->send_mca_msg)
                     (pdxp, mcaStartAcquire);
            pdxp->strt=0;
        }
        goto found_param;
     }
     if (paddr->pfield == (void *) &pdxp->stop) {
        if (pdxp->stop != 0) {
            status = (*pdset->send_mca_msg)
                     (pdxp, mcaStopAcquire);
            pdxp->stop=0;
        }
        goto found_param;
     }

     if (paddr->pfield == (void *) &pdxp->eras) {
        if (pdxp->eras != 0) {
            status = (*pdset->send_mca_msg)
                     (pdxp, mcaErase);
            pdxp->eras=0;
        }
        goto found_param;
     }

     if (paddr->pfield == (void *) &pdxp->pktm) {
        setPeakingTime(pdxp);
        setBinWidth(pdxp);
        goto found_param;
     }

     if (paddr->pfield == (void *) &pdxp->gain) {
        setGain(pdxp);
        setBinWidth(pdxp);
        goto found_param;
     }

     if (paddr->pfield == (void *) &pdxp->pgain) {
        setBinWidth(pdxp);
        goto found_param;
     }

     if (paddr->pfield == (void *) &pdxp->emax) {
        setBinWidth(pdxp);
        goto found_param;
     }

     if (paddr->pfield == (void *) &pdxp->fippi) {
        setFippi(pdxp);
        goto found_param;
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

    if ((paddr->pfield == (void *) &pdxp->rcal) ||
        (paddr->pfield == (void *) &pdxp->calr)) {
          setDxpTasks(pdxp);
          goto found_param;
     }

found_param:
    /* Call "process" so changes to parameters are displayed */
    process(pdxp);

    Debug(5, "dxpRecord(special): exit\n");
    return(0);
}


static void setDxpTasks(struct dxpRecord *pdxp)
{
   struct devDxpDset *pdset = (struct devDxpDset *)(pdxp->dset);
   MODULE_INFO *minfo = &moduleInfo[pdxp->mtyp];
   DXP_TASK_PARAM *task_param;
   int cal;
   unsigned short runtasks;
   int i;
   int status;

   Debug(5, "dxpRecord(setDxpTasks): entry\n");
   if (pdxp->rcal) {
      if (pdxp->calr == dxpCAL_TRACKRST) cal=2; else cal=1;
      Debug(5, "dxpRecord(setDxpTasks): running calibration=%d\n", cal);
      status = (*pdset->send_dxp_msg) (pdxp, MSG_DXP_CALIBRATE, cal, 0);
   } else {
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
         (pdxp,  MSG_DXP_SET_SHORT_PARAM, minfo->offsets.runtasks, runtasks);
   }
   Debug(5, "dxpRecord(setDxpTasks): exit\n");
}

static void setPeakingTimeStrings(struct dxpRecord *pdxp)
{
   double peakingTime;
   MODULE_INFO *minfo = &moduleInfo[pdxp->mtyp];
   int decimation = pdxp->pptr[minfo->offsets.decimation];
   int i;
   
   for (i=0; i < MAX_PEAKING_TIMES; i++) {
      peakingTime = peakingTimes[i] * minfo->clock * (1<<decimation);
      /* Minimum peaking time on the DXP4C of .5 microsecond */
      if ((pdxp->mtyp == MODEL_DXP4C) && (peakingTime < 0.5)) peakingTime = 0.5;
      sprintf(pdxp->pkl+DXP_STRING_SIZE*i, "%.2f us", peakingTime);
   }
}

static void setPeakingTimeRangeStrings(struct dxpRecord *pdxp)
{
   double minTime, maxTime;
   MODULE_INFO *minfo = &moduleInfo[pdxp->mtyp];
   int i;
   
   for (i=0; i < MAX_DECIMATIONS; i++) {
      minTime = peakingTimes[0] * minfo->clock * 
                            (1<<allDecimations[i]);
      maxTime = peakingTimes[MAX_PEAKING_TIMES-1] * minfo->clock *
                            (1<<allDecimations[i]);
      sprintf(pdxp->pkrl+DXP_STRING_SIZE*i, "%.2f-%.2f us", minTime, maxTime);
   }
}


static void setPeakingTime(struct dxpRecord *pdxp)
{
   struct devDxpDset *pdset = (struct devDxpDset *)(pdxp->dset);
   MODULE_INFO *minfo = &moduleInfo[pdxp->mtyp];

   /* New value of peaking time.  
    * Compute new values of SLOWLEN, PEAKSAMP, etc. */
   unsigned short slowlen=0, peaksamp=0, slowgap=0, peakint=0;
   unsigned int index=pdxp->pktm;
   int decimation = pdxp->pptr[minfo->offsets.decimation];

   /* The shortest shaping time is not allowed on DXP4C */
   if ((pdxp->mtyp == MODEL_DXP4C) && (decimation == 0) && (index == 0)) index=1;
   slowlen = peakingTimes[index];
   slowgap = pdxp->pptr[minfo->offsets.slowgap];
   switch (decimation) {
      /* This algorithm is from page 38 of the DXP4C User's Manual */
      case 0:
         peaksamp = slowlen + (slowgap+1)/2 - 2;
         peakint = slowlen + slowgap;
         break;
      case 2:
         peaksamp = slowlen + (slowgap+1)/2 - 1;
         peakint = slowlen + slowgap + 1;
         break;
      case 4:
      case 6:
         peaksamp = slowlen + (slowgap+1)/2 + 1;
         peakint = slowlen + slowgap + 1;
         break;
      default:
         Debug(1, "(special) Unexpected decimation = %d\n", decimation)
   }
   /* Copy new values to parameter array in case other routines need them
    * before record processes again */
   pdxp->pptr[minfo->offsets.slowlen] = slowlen;
   pdxp->pptr[minfo->offsets.peaksamp] = peaksamp;
   pdxp->pptr[minfo->offsets.peakint] = peakint;
   (*pdset->send_dxp_msg)
            (pdxp,  MSG_DXP_SET_SHORT_PARAM, minfo->offsets.slowlen, slowlen);
   (*pdset->send_dxp_msg)
            (pdxp,  MSG_DXP_SET_SHORT_PARAM, minfo->offsets.peaksamp, peaksamp);
   (*pdset->send_dxp_msg)
            (pdxp,  MSG_DXP_SET_SHORT_PARAM, minfo->offsets.peakint, peakint);
}


static void setGain(struct dxpRecord *pdxp)
{
   struct devDxpDset *pdset = (struct devDxpDset *)(pdxp->dset);
   MODULE_INFO *minfo = &moduleInfo[pdxp->mtyp];
   unsigned short gaindac;

   /* New value of GAIN.  
    * Compute new values of GAINDAC, etc. */

   switch (pdxp->mtyp) {
      case MODEL_DXP2X:
      case MODEL_DXPX10P:
         /* This algorithm is from page 49 of the DXP2X User's Manual
          * Gain = Gin * Gvar * Gbase
          * Gin = 1, Gbase=~.5, Gvar=10^((GAINDAC/65536)*40/20)
          * Gain = 0.5*10^((GAINDAC/65536)*40/20)
          * GAINDAC=LOG10(2*GAIN)*32768 */
         if (pdxp->gain < 0.5) pdxp->gain=0.5;
         if (pdxp->gain > 50.) pdxp->gain=50.;
         gaindac = log10(2.*pdxp->gain)*32768.;
         /* Copy new value to parameter array in case other routines need it
          * before record processes again */
         pdxp->pptr[minfo->offsets.gaindac] = gaindac;
         (*pdset->send_dxp_msg)
            (pdxp,  MSG_DXP_SET_SHORT_PARAM, minfo->offsets.gaindac, gaindac);
         break;
      case MODEL_DXP4C:
         /* Work needed here !!! set coarse gain, fine gain?*/
         break;
   }
}


static void setBinWidth(struct dxpRecord *pdxp)
{
   struct devDxpDset *pdset = (struct devDxpDset *)(pdxp->dset);
   MODULE_INFO *minfo = &moduleInfo[pdxp->mtyp];
   unsigned short binfact1;
   unsigned short slowlen;
   float binwidth;

   /* New value of binfact1 to obtain the desired value of EMAX.  This must be
    * done whenever the preamp gain (PGAIN), the ASC gain (GAIN), or the shaping 
    * time is changed. */

   switch (pdxp->mtyp) {
      case MODEL_DXP2X:
      case MODEL_DXPX10P:
         binwidth = pdxp->emax / pdxp->pptr[minfo->offsets.mcalimhi];
         slowlen = pdxp->pptr[minfo->offsets.slowlen];
         binfact1 = pdxp->pgain * pdxp->gain * minfo->adc_gain * 4. * 
                    slowlen * binwidth + 0.5;
         /* Copy new value to parameter array in case other routines need it
          * before record processes again */
         pdxp->pptr[minfo->offsets.binfact] = binfact1;
         (*pdset->send_dxp_msg)
            (pdxp,  MSG_DXP_SET_SHORT_PARAM, minfo->offsets.binfact, binfact1);
         break;
      case MODEL_DXP4C:
         /* Work needed here !!! */
         break;
   }
}

static void setFippi(struct dxpRecord *pdxp)
{
   struct devDxpDset *pdset = (struct devDxpDset *)(pdxp->dset);
   (*pdset->send_dxp_msg) (pdxp, MSG_DXP_DOWNLOAD_FIPPI, pdxp->fippi, 0);
}

static long get_precision(struct dbAddr *paddr, long *precision)
{
    int fieldIndex = dbGetFieldIndex(paddr);

    if ((fieldIndex == dxpRecordGAIN)   ||
        (fieldIndex == dxpRecordGAINRBV) ||
        (fieldIndex == dxpRecordPGAIN)  ||
        (fieldIndex == dxpRecordEMAX)   ||
        (fieldIndex == dxpRecordEMAXRBV)) {
       *precision = 3;
       return(0);
    }
    recGblGetPrec(paddr,precision);
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

   if (fieldIndex == dxpRecordGAIN) {
      pgd->upper_disp_limit = 50.;
      pgd->lower_disp_limit = 0.5;
      return(0);
   }
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

   if (fieldIndex == dxpRecordGAIN) {
      pcd->upper_ctrl_limit = 50.;
      pcd->lower_ctrl_limit = 0.5;
      return(0);
   }
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


static long get_enum_str(struct dbAddr *paddr, char *pstring)
{
    struct dxpRecord   *pdxp=(struct dxpRecord *)paddr->precord;
    int index;
    unsigned short *pfield = (unsigned short *)paddr->pfield;
    unsigned short val=*pfield;

    index = dbGetFieldIndex(paddr);
    switch (index) {
    case dxpRecordPKTM:
       if (val < MAX_PEAKING_TIMES)
          strncpy(pstring, pdxp->pkl+DXP_STRING_SIZE*val, DXP_STRING_SIZE);
       else
          strcpy(pstring,"Illegal Value");
       break;
    case dxpRecordFIPPI:
       if (val < MAX_DECIMATIONS)
          strncpy(pstring, pdxp->pkrl+DXP_STRING_SIZE*val, DXP_STRING_SIZE);
       else
          strcpy(pstring,"Illegal Value");
       break;
    default:
       strcpy(pstring,"Illegal_Value");
       break;
    }
    return(0);
}

static long get_enum_strs(struct dbAddr *paddr, struct dbr_enumStrs *pes)
{
    struct dxpRecord *pdxp=(struct dxpRecord *)paddr->precord;
    int i;

    int index;

    index = dbGetFieldIndex(paddr);
    switch (index) {
    case dxpRecordPKTM:
       pes->no_str = MAX_PEAKING_TIMES;
       for (i=0; i < pes->no_str; i++) {
          strncpy(pes->strs[i], pdxp->pkl+DXP_STRING_SIZE*i, DXP_STRING_SIZE);
       }
       break;
    case dxpRecordFIPPI:
       pes->no_str = MAX_DECIMATIONS;
       for (i=0; i < pes->no_str; i++) {
          strncpy(pes->strs[i], pdxp->pkrl+DXP_STRING_SIZE*i, DXP_STRING_SIZE);
       }
       break;
    default:
       return(S_db_badChoice);
       break;
    }
    return(0);
}


static long put_enum_str(struct dbAddr *paddr, char *pstring)
{
    struct dxpRecord *pdxp=(struct dxpRecord *)paddr->precord;
    short i;
    int index;

    index = dbGetFieldIndex(paddr);
    switch (index) {
    case dxpRecordPKTM:
       for (i=0; i < MAX_PEAKING_TIMES; i++){
          if(strncmp(pdxp->pkl+DXP_STRING_SIZE*i,pstring,DXP_STRING_SIZE)==0) {
             pdxp->pktm = i;
             return(0);
          }
       }
       break;
    case dxpRecordFIPPI:
       for (i=0; i < MAX_DECIMATIONS; i++){
          if(strncmp(pdxp->pkrl+DXP_STRING_SIZE*i,pstring,DXP_STRING_SIZE)==0) {
             pdxp->fippi = i;
             return(0);
          }
       }
       break;
    default:
       return(S_db_badChoice);
       break;
    }
    return(0);
}
