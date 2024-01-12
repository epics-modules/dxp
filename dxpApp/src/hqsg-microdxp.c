/*
 * This code accompanies the XIA Application Note
 * "Handel Quick Start Guide: microDXP".
 *
 * Copyright (c) 2004-2015 XIA LLC
 * All rights reserved
 */

#include <stdio.h>
#include <stdlib.h>


#ifdef _WIN32
#pragma warning(disable : 4115)

/* For Sleep() */
#include <windows.h>
#else
#include <time.h>
#endif

#include "handel.h"
#include "handel_errors.h"
#include "md_generic.h"

static void CHECK_ERROR(int status);
static void CHECK_MEM(void *mem);
static void SLEEP(double time_seconds);
static void print_usage(void);


int main(int argc, char *argv[])
{
  int status;
  int i;
  unsigned short ignored = 0;

  /* Acquisition Values */
  double nMCA     = 4096.0;
  double thresh   = 48.0;
  double polarity = 0.0;
  double gain     = 4.077;
  double currentGENSET;
  double currentPARSET;

  unsigned short GENSET;
  unsigned short PARSET;

  unsigned short numberPeakingTimes = 0;
  unsigned short numberFippis = 0;

  unsigned long mcaLen = 0;

  unsigned long *mca = NULL;

  double *peakingTimes = NULL;

  if (argc < 2) {
      print_usage();
      exit(1);
  }

  /* Setup logging here */
  printf("Configuring the Handel log file.\n");
  xiaSetLogLevel(MD_DEBUG);
  xiaSetLogOutput("handel.log");

  printf("Loading the .ini file.\n");
  status = xiaInit(argv[1]);
  CHECK_ERROR(status);

  xiaSetIOPriority(MD_IO_PRI_HIGH);

  status = xiaStartSystem();
  CHECK_ERROR(status);

  /* Modify some acquisition values */
  status = xiaSetAcquisitionValues(0, "number_mca_channels", &nMCA);
  CHECK_ERROR(status);

  status = xiaSetAcquisitionValues(0, "trigger_threshold", &thresh);
  CHECK_ERROR(status);

  status = xiaSetAcquisitionValues(0, "polarity", &polarity);
  CHECK_ERROR(status);

  status = xiaSetAcquisitionValues(0, "gain", &gain);
  CHECK_ERROR(status);

  /* Apply changes to parameters */
  status = xiaBoardOperation(0, "apply", (void *)&ignored);

  /* Save the settings to the current GENSET and PARSET */
  status = xiaGetAcquisitionValues(0, "genset", &currentGENSET);
  CHECK_ERROR(status);

  status = xiaGetAcquisitionValues(0, "parset", &currentPARSET);
  CHECK_ERROR(status);

  GENSET = (unsigned short)currentGENSET;
  PARSET = (unsigned short)currentPARSET;

  status = xiaBoardOperation(0, "save_genset", &GENSET);
  CHECK_ERROR(status);

  status = xiaBoardOperation(0, "save_parset", &PARSET);
  CHECK_ERROR(status);

  /* Read out number of peaking times to pre-allocate peaking time array */
  status = xiaBoardOperation(0, "get_number_pt_per_fippi", &numberPeakingTimes);
  CHECK_ERROR(status);

  peakingTimes = (double *)malloc(numberPeakingTimes * sizeof(double));
  CHECK_MEM(peakingTimes);

  status = xiaBoardOperation(0, "get_current_peaking_times", peakingTimes);
  CHECK_ERROR(status);

  /* Print out the current peaking times */
  for (i = 0; i < numberPeakingTimes; i++) {
    printf("peaking time %d = %lf\n", i, peakingTimes[i]);
  }

  free(peakingTimes);

  /* Read out number of fippis to pre-allocate peaking time array */
  status = xiaBoardOperation(0, "get_number_of_fippis", &numberFippis);
  CHECK_ERROR(status);

  peakingTimes = (double *)malloc(numberPeakingTimes * numberFippis * sizeof(double));
  CHECK_MEM(peakingTimes);

  status = xiaBoardOperation(0, "get_peaking_times", peakingTimes);
  CHECK_ERROR(status);

  /* Print out the current peaking times */
  for (i = 0; i < numberPeakingTimes * numberFippis; i++) {
    printf("peaking time %d = %lf\n", i, peakingTimes[i]);
  }

  free(peakingTimes);

  /* Start a run w/ the MCA cleared */
  status = xiaStartRun(0, 0);
  CHECK_ERROR(status);

  printf("Started run. Sleeping...\n");
  SLEEP(1);

  status = xiaStopRun(0);
  CHECK_ERROR(status);

  /* Prepare to read out MCA spectrum */
  status = xiaGetRunData(0, "mca_length", &mcaLen);
  CHECK_ERROR(status);

  if (mcaLen > 0)
  {
    printf("Got run data\n");
  }
  /* If you don't want to dynamically allocate memory here,
   * then be sure to declare mca as an array of length 8192,
   * since that is the maximum length of the spectrum.
   */
  mca = (unsigned long *)malloc(mcaLen * sizeof(unsigned long));
  CHECK_MEM(mca);

  status = xiaGetRunData(0, "mca", (void *)mca);
  CHECK_ERROR(status);

  /* Display the spectrum, write it to a file, etc... */

  xiaSetIOPriority(MD_IO_PRI_NORMAL);

  free(mca);

  status = xiaExit();
  CHECK_ERROR(status);
  xiaCloseLog();

  return 0;
}


/**
 * This is an example of how to handle error values. In your program
 * it is likely that you will want to do something more robust than
 * just exit the program.
 */
static void CHECK_ERROR(int status)
{
  /* XIA_SUCCESS is defined in handel_errors.h */
  if (status != XIA_SUCCESS) {
    fprintf(stderr, "Error encountered! Status = %d\n", status);
    exit(status);
  }
}

static void CHECK_MEM(void *mem)
{
  if (mem == NULL) {
    printf("Memory allocation failed\n");
    exit(1);
  }
}

static void SLEEP(double time_seconds)
{
#if _WIN32
    DWORD wait = (DWORD)(1000.0 * time_seconds);
    Sleep(wait);
#else
    unsigned long secs = (unsigned long)time_seconds;
    struct timespec req = {
        .tv_sec = secs,
        .tv_nsec = ((time_seconds - secs) * 1000000000.0)
    };
    struct timespec rem = {
        .tv_sec = 0,
        .tv_nsec = 0
    };
    while (TRUE_) {
        if (nanosleep(&req, &rem) == 0)
        break;
        req = rem;
    }
#endif
}

static void print_usage(void)
{
    fprintf(stdout, "Arguments: [.ini file]]\n");
    return;
}
