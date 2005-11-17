/*
 * hqsg-xmap-mapping.c
 *
 * This code accompanies the XIA Application Note
 * "Handel Quick Start Guide: xMAP". This code should be used in conjunction
 * with the "Mapping Mode" section in the Quick Start Guide.
 *
 * Created 10/18/05 -- PJF
 *
 * Copyright (c) 2005, XIA LLC
 * All rights reserved
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include <windows.h>

#include "handel.h"
#include "handel_errors.h"
#include "handel_constants.h"
#include "md_generic.h"


static void CHECK_ERROR(int status);

static int wait_for_buffer(char buf);
static int read_buffer(char buf, unsigned long *data);
static int switch_buffer(char *buf);
static int get_current_point(unsigned long *pixel);


int main(int argc, char *argv[]) 
{
    int status;
	int ignored = 0;

	char curBuffer = 'a';

	unsigned short isFull = 0;

	unsigned long curPixel  = 0;
	unsigned long bufferLen = 0;

    /* Acquisition Values */
	double nBins = 2048.0;

	/* Mapping parameters */
	double nMapPixels          = 200.0;
	double nMapPixelsPerBuffer = -1.0;
	double pixControl          = XIA_MAPPING_CTL_GATE;

	unsigned long *buffer = NULL;


    status = xiaInit("xmap_reset_map.ini");
    CHECK_ERROR(status);

    /* Setup logging here */
    xiaSetLogLevel(MD_ERROR);
    xiaSetLogOutput("errors.log");

	/* Boot hardware */
    status = xiaStartSystem();
    CHECK_ERROR(status);

	/* Set mapping parameters. */
	status = xiaSetAcquisitionValues(-1, "number_mca_channels", (void *)&nBins);
	CHECK_ERROR(status);

	status = xiaSetAcquisitionValues(-1, "num_map_pixels", (void *)&nMapPixels);
	CHECK_ERROR(status);

	status = xiaSetAcquisitionValues(-1, "num_map_pixels_per_buffer",
									 (void *)&nMapPixelsPerBuffer);
	CHECK_ERROR(status);
	
	/* Apply the mapping parameters. */
	status = xiaBoardOperation(0, "apply", (void *)&ignored);
	CHECK_ERROR(status);

	/* Prepare the buffer we will use to read back the data from the board. */
	status = xiaGetRunData(0, "buffer_len", (void *)&bufferLen);
	CHECK_ERROR(status);

	buffer = (unsigned long *)malloc(bufferLen * sizeof(unsigned long));
	
	if (!buffer) {
	  return -1;
	}

	/* Start the mapping run. */
	status = xiaStartRun(-1, 0);
	CHECK_ERROR(status);

	/* The main loop that is described in the Quick Start Guide. */
	do {
	  status = wait_for_buffer(curBuffer);
	  CHECK_ERROR(status);

	  status = read_buffer(curBuffer, buffer);
	  CHECK_ERROR(status);

	  /* This is where you would ordinarily do something with the data:
	   * write it to a file, post-process it, etc.
	   */

	  status = switch_buffer(&curBuffer);
	  CHECK_ERROR(status);

	  status = get_current_pixel(&curPixel);
	  CHECK_ERROR(status);

	} while (curPixel < (unsigned long)nMapPixels);

	free(buffer);

	/* Stop the mapping run. */
	status = xiaStopRun(-1);
	CHECK_ERROR(status);

	status = xiaExit();
	CHECK_ERROR(status);

	return 0;
}


/********** 
 * This is just an example of how to handle error values.
 * A program of any reasonable size should
 * implement a more robust error handling mechanism.
 **********/
static void CHECK_ERROR(int status)
{
    /* XIA_SUCCESS is defined in handel_errors.h */
    if (status != XIA_SUCCESS) {
	printf("Error encountered! Status = %d\n", status);
	getch();
	exit(status);
    }
}


/**********
 * Waits for the specified buffer to fill.
 **********/
static int wait_for_buffer(char buf)
{
  int status;

  char bufString[15];

  unsigned short isFull = FALSE_;


  sprintf(bufString, "buffer_full_%c", buf);

  while (!isFull) {
	status = xiaGetRunData(0, bufString, (void *)&isFull);
	
	if (status != XIA_SUCCESS) {
	  return status;
	}

	Sleep(1);
  }

  return XIA_SUCCESS;
}


/**********
 * Reads the requested buffer.
 **********/
static int read_buffer(char buf, unsigned long *data)
{
  int status;

  char bufString[9];


  sprintf(bufString, "buffer_%c", buf);
  
  status = xiaGetRunData(0, bufString, (void *)data);
  
  return status;
}


/**********
 * Clears the current buffer and switches to the next buffer.
 **********/
static int switch_buffer(char *buf)
{
  int status;


  status = xiaBoardOperation(0, "buffer_done", (void *)buf);

  switch (*buf) {
  case 'a':
	*buf = 'b';
	break;
  case 'b':
	*buf = 'a';
	break;
  }

  return status;
}


/**********
 * Get the current mapping pixel.
 **********/
static int get_current_pixel(unsigned long *pixel)
{
  int status;


  status = xiaGetRunData(0, "current_pixel", (void *)pixel);
  
  return status;
}
