/*
 * handel_xerxes.c
 *
 * Created 10/25/01 -- PJF
 *
 *
 * This serves as an interface to all calls to XerXes routines.
 *
 * Copyright (c) 2002, X-ray Instrumentation Associates
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
 */


#include <stdio.h>
#include <limits.h>

#include "xia_handel.h"
#include "xia_system.h"
#include "handel_errors.h"
#include "xia_handel_structures.h"
#include "handel_xerxes.h"
#include "xerxes.h"
#include "xerxes_errors.h"

#include "fdd.h"


HANDEL_STATIC int HANDEL_API xiaGetMMUName(Module *module, int channel,
										   char *mmuName,
										   char *rawFilename);

/*****************************************************************************
 *
 * This routine calls the XerXes fit routine. See XerXes for more details.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaFitGauss(long data[], int lower, int upper, float *pos, float *fwhm)
{
  int statusX;

  statusX = dxp_fitgauss0(data, &lower, &upper, pos, fwhm);

  if (statusX != XIA_SUCCESS)
	{
	  return XIA_XERXES;
	}

  return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine calls the XerXes find peak routine. See XerXes for more 
 * details.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaFindPeak(long *data, int numBins, float thresh, int *lower, int *upper)
{
  int statusX;

  statusX = dxp_findpeak(data, &numBins, &thresh, lower, upper);

  if (statusX != XIA_SUCCESS)
	{
	  return XIA_XERXES;
	}

  return XIA_SUCCESS;
}



/*****************************************************************************
 *
 * This routine calls XerXes routines in order to build a proper XerXes
 * configuration based on the HanDeL information.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaBuildXerxesConfig(void)
{
  int statusX = DXP_SUCCESS;
  int statusH = XIA_SUCCESS;
  int i;
  int detChan;

  unsigned int numModStrElems = 0;
  unsigned int modChan;

  double peakingTime = 0;

  char *boardName[1];
  char *interfStr[2];
  char *dspStr[2];
  char *fippiStr[2];
  char *mmuStr[1];
	
  char *detAlias = NULL;

  char **modString;

  /* This is the maximum size for the MD string passed to XerXes */
  char mdString[6];
  char detChStr[2];
  /* This should handle a 1000-element array */
  char numChanStr[4];
  char dspName[MAXFILENAME_LEN];
  char fippiName[MAXFILENAME_LEN];
  char mmuName[MAXFILENAME_LEN];
  char detectorType[MAXITEM_LEN];
  char rawFilename[MAXFILENAME_LEN];

  Detector *detector = NULL;

  Module *current = NULL;

  statusX = dxp_init_ds();

  if (statusX != DXP_SUCCESS)
	{
	  statusH = XIA_XERXES;
	  sprintf(info_string, "Unable to init XerXes DSs. dxp_init_ds() returned %d", statusX);
	  xiaLogError("xiaBuildXerxesConfig", info_string, statusH);
	  return statusH;
	}

  /* Define board types for XerXes */
  for (i = 0; i < KNOWN_BOARDS; i++)
	{
	  statusX += dxp_add_system_item(boardList[i], (char **)null);
	}

  statusX += dxp_add_system_item("preamp", (char **)null);

  if (statusX != DXP_SUCCESS)
	{
	  statusH = XIA_XERXES;
	  sprintf(info_string, "Unable to add system items to XerXes");
	  xiaLogError("xiaBuildXerxesConfig", info_string, statusH);
	  return statusH;
	}

  /* Walk through the module list and make the proper calls to XerXes */
  current = xiaGetModuleHead();

  while (current != NULL)
	{
	  /* BOARD_TYPE */
	  boardName[0] = (char *)handel_md_alloc((strlen(current->type) + 1) * sizeof(char));
	  strcpy(boardName[0], current->type);

	  statusX = dxp_add_board_item("board_type", (char **)boardName);
		
	  handel_md_free((void *)boardName[0]);
	  boardName[0] = NULL;

	  if (statusX != DXP_SUCCESS)
		{
		  statusH = XIA_XERXES;
		  sprintf(info_string, "Error adding board_type to XerXes for alias %s", current->alias);
		  xiaLogError("xiaBuildXerxesConfig", info_string, statusH);
		  return statusH;
		}


	  sprintf(info_string, "board item-board type = %s configuration finished", current->type);
	  xiaLogDebug("xiaBuildXerxesConfig", info_string);

	  /* INTERFACE */
	  xiaBuildInterfString(current, interfStr);

	  statusX = dxp_add_board_item("interface", (char **)interfStr);

	  handel_md_free((void *)interfStr[0]);
	  handel_md_free((void *)interfStr[1]);
	  interfStr[0] = NULL;
	  interfStr[1] = NULL;

	  if (statusX != DXP_SUCCESS)
		{
		  statusH = XIA_XERXES;
		  sprintf(info_string, "Error adding interface to XerXes for alias %s", current->alias);
		  xiaLogError("xiaBuildXerxesConfig", info_string, statusH);
		  return statusH;
		}

	  sprintf(info_string, "board item-interface configuration finished");
	  xiaLogDebug("xiaBuildXerxesConfig", info_string);

	  /* MODULE */
	  numModStrElems = current->number_of_channels + 2;
	  modString = (char **)handel_md_alloc(numModStrElems * sizeof(char *));

	  xiaBuildMDString(current, mdString);

	  sprintf(numChanStr, "%u", current->number_of_channels);

	  modString[0] = (char *)handel_md_alloc((strlen(mdString) + 1) * sizeof(char));
	  strcpy(modString[0], mdString);

	  modString[1] = (char *)handel_md_alloc((strlen(numChanStr) + 1) * sizeof(char));
	  strcpy(modString[1], numChanStr);

	  for (i = 0; i < (int)current->number_of_channels; i++)
		{
		  sprintf(detChStr, "%d", current->channels[i]);
		  modString[i + 2] = (char *)handel_md_alloc((strlen(detChStr) + 1) * sizeof(char));
		  strcpy(modString[i + 2], detChStr);
		}

	  statusX = dxp_add_board_item("module", modString);

	  for (i = 0; i < (int)numModStrElems; i++)
		{
		  handel_md_free((void *)modString[i]);
		}
		
	  handel_md_free((void *)modString);
	  modString = NULL;

	  if (statusX != DXP_SUCCESS)
		{
		  statusH = XIA_XERXES;
		  sprintf(info_string, "Error adding module to XerXes for alias %s", current->alias);
		  xiaLogError("xiaBuildXerxesConfig", info_string, statusH);
		  return statusH;
		}		

	  sprintf(info_string, "board item-module configuration finished");
	  xiaLogDebug("xiaBuildXerxesConfig", info_string);
	  
	  /* Deal with the MMU first.  Only one per module, so do before channel loop */
	  statusH = xiaGetMMUName(current, 0, mmuName, rawFilename);
	  
	  if (statusH == XIA_NO_MMU) {
	    
	    xiaLogDebug("xiaBuildXerxesConfig", "No MMU present");
	    
	    /* Not all products have a MMU */
	    
	  } else {

	    if (statusH != XIA_SUCCESS) {
	      /* This is an actual problem */
	      sprintf(info_string, "Unable to get a MMU name for module %s", current->alias);
	      xiaLogError("xiaBuildXerxesConfig", info_string, statusH);
	      return statusH;
	    }
	    
	    strcpy(current->currentFirmware[0].currentMMU, rawFilename);
	    
	    mmuStr[0] = (char *)handel_md_alloc((strlen(mmuName) + 1) * sizeof(char));
	    
	    strcpy(mmuStr[0], mmuName);
	    
	    statusX = dxp_add_board_item("mmu", (char **)mmuStr);
	    
	    handel_md_free((void *)mmuStr[0]);
	    
	    if (statusX != DXP_SUCCESS) {
	      
	      statusH = XIA_XERXES;
	      sprintf(info_string, "Error adding board item MMU for module %s", current->alias);
	      xiaLogError("xiaBuildXerxesConfig", info_string, statusH);
	      return statusH;
	    }
	  }

	  /* DSP */
	  /* (and) MMU */
	  /* This part of the code bypasses the whole "dsp" vs. "default_dsp"
	   * issue by looping over the channels and making calls to "dsp".
	   * This may prove to be too inefficient for certain systems and 
	   * should be made to be more clever in the future.
	   */
	  for (i = 0; i < (int)current->number_of_channels; i++)
		{
		  /* All products validate that peaking time exists in the defaults */
		  detChan  = current->channels[i];
		  modChan  = xiaGetModChan((unsigned int)detChan);
		  detAlias = current->detector[modChan];
		  detector = xiaFindDetector(detAlias);

		  /* Ultra-paranoia check here for NULL pointer. */
		  if (detector == NULL) {

			statusH = XIA_NO_ALIAS;
			sprintf(info_string,
					"Specified detector alias %s does not exist in the system", detAlias);
			xiaLogError("xiaBuildXerxesConfig", info_string, statusH);
			return statusH;
		  }

		  switch (detector->type)
			{
			case XIA_DET_RESET:
			  strcpy(detectorType, "RESET");
			  break;

			case XIA_DET_RCFEED:
			  strcpy(detectorType, "RC");
			  break;

			default:
			case XIA_DET_UNKNOWN:
			  statusH = XIA_MISSING_TYPE;
			  sprintf(info_string, "No detector type specified for detChan %d", detChan);
			  xiaLogError("xiaBuildXerxesConfiguration", info_string, statusH);
			  return statusH;
			  break;
			}
			
		  peakingTime = xiaGetValueFromDefaults("peaking_time", current->defaults[i]);

		  statusH = xiaGetDSPName(current, i, peakingTime, dspName, detectorType, rawFilename);
			
		  if (statusH != XIA_SUCCESS)
			{
			  sprintf(info_string, "Error getting DSP name for alias %s", current->alias);
			  xiaLogError("xiaBuildXerxesConfig", info_string, statusH);
			  return statusH;
			}


		  /* Update the currentFirmware info here. Technically this doesn't have
		   * to do with the XerXes config, but that's okay for now. We'll have to
		   * find somewhere else to set the "user" fields since they aren't really
		   * dealt with here.
		   */
		  strcpy(current->currentFirmware[i].currentDSP, rawFilename);

		  dspStr[0] = (char *)handel_md_alloc(3 * sizeof(char));
		  dspStr[1] = (char *)handel_md_alloc((strlen(dspName) + 1) * sizeof(char));

		  sprintf(dspStr[0], "%d", i);
		  strcpy(dspStr[1], dspName);

		  statusX += dxp_add_board_item("dsp", (char **)dspStr);

		  handel_md_free((void *)dspStr[0]);
		  handel_md_free((void *)dspStr[1]);
		}

	  if (statusX != DXP_SUCCESS)
		{
		  statusH = XIA_XERXES;
		  sprintf(info_string, "Error adding DSP info to XerXes for alias %s", current->alias);
		  xiaLogError("xiaBuildXerxesConfig", info_string, statusH);
		  return statusH;
		}


	  /* FIPPI */
	  for (i = 0; i < (int)current->number_of_channels; i++)
		{
		  /* Can use peakingTime from DSP section */
		  detChan  = current->channels[i];
		  modChan  = xiaGetModChan((unsigned int)detChan);
		  detAlias = current->detector[modChan];
		  detector = xiaFindDetector(detAlias);

		  switch (detector->type)
			{
			case XIA_DET_RESET:
			  strcpy(detectorType, "RESET");
			  break;

			case XIA_DET_RCFEED:
			  strcpy(detectorType, "RC");
			  break;

			default:
			case XIA_DET_UNKNOWN:
			  statusH = XIA_MISSING_TYPE;
			  sprintf(info_string, "No detector type specified for detChan %d", detChan);
			  xiaLogError("xiaBuildXerxesConfiguration", info_string, statusH);
			  return statusH;
			  break;
			}

		  statusH = xiaGetFippiName(current, i, peakingTime, fippiName, detectorType, rawFilename);

		  if (statusH != XIA_SUCCESS)
			{
			  sprintf(info_string, "Error getting FiPPI for alias %s", current->alias);
			  xiaLogError("xiaBuildXerxesConfig", info_string, statusH);
			  return statusH;
			}

		  strcpy(current->currentFirmware[i].currentFiPPI, rawFilename);

		  fippiStr[0] = (char *)handel_md_alloc(3 * sizeof(char));
		  fippiStr[1] = (char *)handel_md_alloc((strlen(fippiName) + 1) * sizeof(char));

		  sprintf(fippiStr[0], "%d", i);
		  strcpy(fippiStr[1], fippiName);

		  statusX += dxp_add_board_item("fippi", (char **)fippiStr);

		  handel_md_free((void *)fippiStr[0]);
		  handel_md_free((void *)fippiStr[1]);
		}

	  if (statusX != DXP_SUCCESS)
		{
		  statusH = XIA_XERXES;
		  sprintf(info_string, "Error adding FiPPI info to XerXes for alias %s", current->alias);
		  xiaLogError("xiaBuildXerxesConfig", info_string, statusH);
		  return statusH;
		}


	  /* PARAM */
	  /* Let's pass in the a string that says "NULL" and then setAcqValues will
	   * be responsible for modifying the relevant parameters
	   */
	  statusX = dxp_add_board_item("default_param", (char **)null);

	  if (statusX != DXP_SUCCESS)
		{
		  statusH = XIA_XERXES;
		  sprintf(info_string, "Error adding default_params to XerXes for alias %s", current->alias);
		  xiaLogError("xiaBuildXerxesConfig", info_string, statusH);
		  return statusH;
		}


	  current = getListNext(current);

	}
	

  return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * Essentially a wrapper around dxp_user_setup().
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaUserSetup(void)
{
  int statusX;
  int status;
  int detector_chan;

  unsigned int modChan;

  unsigned short polarity;
  unsigned short type;

  double typeValue;
  double gainScale;

  char boardType[MAXITEM_LEN];
  char detectorType[MAXITEM_LEN];

  char *alias;
  char *detAlias;
  char *firmAlias;

  DetChanElement *current = NULL;

  FirmwareSet *firmwareSet = NULL;

  CurrentFirmware *currentFirmware = NULL;

  Module *module = NULL;

  Detector *detector = NULL;

  XiaDefaults *defaults = NULL;

  PSLFuncs localFuncs;

  statusX = dxp_user_setup();

  if (statusX != DXP_SUCCESS)
	{
	  status = XIA_XERXES;
	  xiaLogError("xiaUserSetup", "Error downloading firmware", status);
	  return status;
	}

  /* Add calls to xiaSetAcquisitionValues() here using values from the defaults
   * list.
   */


  /* Set polarity and reset time from info in detector struct */
  current = xiaGetDetChanHead();
 	
  while (current != NULL)
	{
	  switch (xiaGetElemType(current->detChan))
		{
		case SET:
		  /* Skip SETs since all SETs are composed of SINGLES */
		  break;
		  
		case SINGLE:
		  status = xiaGetBoardType(current->detChan, boardType);

		  if (status != XIA_SUCCESS)
			{
			  sprintf(info_string, "Unable to get boardType for detChan %u", current->detChan);
			  xiaLogError("xiaUserSetup", info_string, status);
			  return status;
			}

		  alias         		= xiaGetAliasFromDetChan(current->detChan);
		  module        		= xiaFindModule(alias);
		  modChan       		= xiaGetModChan((unsigned int)current->detChan);
		  firmAlias     		= module->firmware[modChan];
		  firmwareSet   		= xiaFindFirmware(firmAlias);
		  detAlias      		= module->detector[modChan];
		  detector_chan 		= module->detector_chan[modChan];
		  detector      		= xiaFindDetector(detAlias);
		  polarity      		= detector->polarity[detector_chan];
		  type			  		= detector->type;
		  typeValue     		= detector->typeValue[detector_chan];
		  gainScale             = module->gain[modChan];
		  currentFirmware	    = &module->currentFirmware[modChan];
		  defaults      		= xiaGetDefaultFromDetChan((unsigned int)current->detChan);

		  switch (detector->type)
			{
			case XIA_DET_RESET:
			  strcpy(detectorType, "RESET");
			  break;
		
			case XIA_DET_RCFEED:
			  strcpy(detectorType, "RC");
			  break;
		
			default:
			case XIA_DET_UNKNOWN:
			  status = XIA_MISSING_TYPE;
			  sprintf(info_string, "No detector type specified for detChan %d", current->detChan);
			  xiaLogError("xiaSetAcquisitionValues", info_string, status);
			  return status;
			  break;
			}

		  status = xiaLoadPSL(boardType, &localFuncs);

		  if (status != XIA_SUCCESS)
			{
			  sprintf(info_string, "Unable to load PSL funcs for detChan %d", current->detChan);
			  xiaLogError("xiaUserSetup", info_string, status);
			  return status;
			}

/* printf("handel_xerxes:: Begin setting polarity for detector %d\n", current->detChan); */
		  status = localFuncs.setPolarity((int)current->detChan, detector, detector_chan, defaults);

		  if (status != XIA_SUCCESS)
			{
			  sprintf(info_string, "Unable to set polarity for detChan %d", current->detChan);
			  xiaLogError("xiaUserSetup", info_string, status);
			  return status;
			}

		  status = localFuncs.setDetectorTypeValue((int)current->detChan, detector, detector_chan, defaults);

		  if (status != XIA_SUCCESS)
			{
			  sprintf(info_string, "Unable to set detector typeValue for detChan %d", current->detChan);
			  xiaLogError("xiaUserSetup", info_string, status);
			  return status;
			}

		  /* Now we can do the defaults */
		  status = localFuncs.userSetup((int)current->detChan, defaults, firmwareSet,
										currentFirmware, detectorType, gainScale,
										detector, detector_chan, module);
   
		  if (status != XIA_SUCCESS)
			{
			  sprintf(info_string, "Unable to complete user setup for detChan %d",
					  current->detChan);
			  xiaLogError("xiaUserSetup", info_string, status);
			  return status;
			}

		  /* Do any DSP parameters that are in the list */
		  status = xiaUpdateUserParams(current->detChan);

		  if (status != XIA_SUCCESS) {

			sprintf(info_string, "Unable to update user parameters for detChan %d", current->detChan);
			xiaLogError("xiaUserSetup", info_string, status);
			return status;
		  }

		  break;

		case 999:
		  status = XIA_INVALID_DETCHAN;
		  xiaLogError("xiaUserSetup", "detChan number is not in the list of valid values ", status);
		  return status;
		  break;
		default:
		  status = XIA_UNKNOWN;
		  xiaLogError("xiaUserSetup", "Should not be seeing this message", status);
		  return status;
		  break;
		}

	  current = getListNext(current);
	}

  return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * Builds a char** to pass to XerXes for the "interface" item.
 *
 *****************************************************************************/
HANDEL_STATIC void HANDEL_API xiaBuildInterfString(Module *module, char *interfStr[2])
{
  interfStr[0] = (char *)handel_md_alloc((strlen(interfList[module->interface_info->type]) + 1) * sizeof(char));
  strcpy(interfStr[0], interfList[module->interface_info->type]);

  switch (module->interface_info->type)
	{
	case NO_INTERFACE:
	default:
	  interfStr[1] = NULL;
	  break;

#ifndef EXCLUDE_CAMAC
	case JORWAY73A:
	case GENERIC_SCSI:
	  interfStr[1] = (char *)handel_md_alloc((strlen("camacdll.dll") + 1) * sizeof(char));
	  strcpy(interfStr[1], "camacdll.dll");
	  break;
#endif /* EXCLUDE_CAMAC */

#ifndef EXCLUDE_EPP
	case EPP:
	case GENERIC_EPP:
	  interfStr[1] = (char *)handel_md_alloc(6 * sizeof(char));
	  sprintf(interfStr[1], "%#x", module->interface_info->info.epp->epp_address);
	  break;
#endif /* EXCLUDE_EPP */

#ifndef EXCLUDE_USB
	case USB:
	  interfStr[1] = (char *)handel_md_alloc(6 * sizeof(char));
	  sprintf(interfStr[1], "%i", module->interface_info->info.usb->device_number);
	  break;
#endif /* EXCLUDE_USB */

#ifndef EXCLUDE_SERIAL
	case SERIAL:
	  interfStr[1] = (char *)handel_md_alloc(6 * sizeof(char));
	  sprintf(interfStr[1], "COM%u", module->interface_info->info.serial->com_port);
	  break;
#endif /* EXCLUDE_SERIAL */

	  /* XXX ARCNET stuff goes here... */

	}

}


/*****************************************************************************
 *
 * Builds a char* corresponding to the MD String that XerXes likes.
 *
 *****************************************************************************/
HANDEL_STATIC void HANDEL_API xiaBuildMDString(Module *module, char *mdString)
{
  switch (module->interface_info->type)
	{
	case NO_INTERFACE:
	default:
	  mdString = NULL;
	  break;

#ifndef EXCLUDE_CAMAC
	case JORWAY73A:
	case GENERIC_SCSI:
	  sprintf(mdString, "%u%u%02u", module->interface_info->info.jorway73a->scsi_bus, 
			  module->interface_info->info.jorway73a->crate_number,
			  module->interface_info->info.jorway73a->slot);
	  break;
#endif /* EXCLUDE_CAMAC */

#ifndef EXCLUDE_EPP
	case EPP:
	case GENERIC_EPP:
	  /* If default then dont change anything, else tack on a : in front 
	   * of the string (tells XerXes to 
	   * treat this as a multi-module EPP chain 
	   */
	  if (module->interface_info->info.epp->daisy_chain_id == UINT_MAX) 
	    {
	      sprintf(mdString, "0");
	    } else {
	      sprintf(mdString, ":%u", module->interface_info->info.epp->daisy_chain_id);
	    }
	  break;
#endif /* EXCLUDE_EPP */

#ifndef EXCLUDE_USB
	case USB:
	  sprintf(mdString, "%i", module->interface_info->info.usb->device_number);
	  break;
#endif /* EXCLUDE_USB */

#ifndef EXCLUDE_SERIAL
	case SERIAL:
	  sprintf(mdString, "%u", module->interface_info->info.serial->com_port);
	  break;
#endif /* EXCLUDE_ARCNET */

	  /* XXX ARCNET stuff goes here... */

	}
}


/*****************************************************************************
 *
 * This routine sets dspName equal to the dsp code associated with channel
 * and module.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetDSPName(Module *module, int channel, double peakingTime, char *dspName, char *detectorType, char *rawFilename)
{
  char *firmAlias;

  int status;

  FirmwareSet *firmwareSet = NULL;

  firmAlias = module->firmware[channel];

  firmwareSet = xiaFindFirmware(firmAlias);

  if (firmwareSet->filename == NULL)
	{
	  status = xiaGetDSPNameFromFirmware(firmAlias, peakingTime, dspName);

	  if (status != XIA_SUCCESS)
		{
		  sprintf(info_string, "Error getting DSP code for firmware %s @ peaking time = %f", firmAlias, peakingTime);
		  xiaLogError("xiaGetDSPName", info_string, status);
		  return status;
		}
	
	} else {

	  status = xiaFddGetFirmware(firmwareSet->filename, "dsp", peakingTime, 
								 (unsigned short)firmwareSet->numKeywords, firmwareSet->keywords,
								 detectorType, dspName, rawFilename);

	  if (status != XIA_SUCCESS)
		{
		  sprintf(info_string, "Error getting DSP code from FDD file %s @ peaking time = %f", firmwareSet->filename, peakingTime);
		  xiaLogError("xiaGetDSPName", info_string, status);
		  return status;
		}

	  /* !!DEBUG!! sequence...used to test FDD stuff */
	  xiaLogDebug("xiaGetDSPName", "***** Dump of data sent to FDD *****");
	  sprintf(info_string, "firmwareSet->filename = %s", firmwareSet->filename);
	  xiaLogDebug("xiaGetDSPName", info_string);
	  sprintf(info_string, "peakingTime = %f", peakingTime);
	  xiaLogDebug("xiaGetDSPName", info_string);
	  sprintf(info_string, "detectorType = %s", detectorType);
	  xiaLogDebug("xiaGetDSPName", info_string);
	  sprintf(info_string, "Returned DSPName = %s", dspName);
	  xiaLogDebug("xiaGetDSPName", info_string);
	}

  return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine sets fippiName equal to the fippi code associated with
 * channel and module.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetFippiName(Module *module, int channel, double peakingTime, char *fippiName, char *detectorType, char *rawFilename)
{
  char *firmAlias;

  int status;

  FirmwareSet *firmwareSet = NULL;

  firmAlias = module->firmware[channel];

  firmwareSet = xiaFindFirmware(firmAlias);

  if (firmwareSet->filename == NULL)
	{
	  status = xiaGetFippiNameFromFirmware(firmAlias, peakingTime, fippiName);
	
	  if (status != XIA_SUCCESS)
		{
		  sprintf(info_string, "Error getting FiPPI code for firmware %s @ peaking time = %f", firmAlias, peakingTime);
		  xiaLogError("xiaGetFippiName", info_string, status);
		  return status;
		}

	  /* I think that we need this! */
	  strcpy(rawFilename, fippiName);

	} else {

	  status = xiaFddGetFirmware(firmwareSet->filename, "fippi", peakingTime,
								 (unsigned short)firmwareSet->numKeywords, firmwareSet->keywords,
								 detectorType, fippiName, rawFilename);


	  /* !!DEBUG!! sequence...used to test FDD stuff */
	  xiaLogDebug("xiaGetFiPPIName", "***** Dump of data sent to FDD *****");
	  sprintf(info_string, "firmwareSet->filename = %s", firmwareSet->filename);
	  xiaLogDebug("xiaGetFiPPIName", info_string);
	  sprintf(info_string, "peakingTime = %f", peakingTime);
	  xiaLogDebug("xiaGetFiPPIName", info_string);
	  sprintf(info_string, "detectorType = %s", detectorType);
	  xiaLogDebug("xiaGetFiPPIName", info_string);
	  sprintf(info_string, "Returned fippiName = %s", fippiName);
	  xiaLogDebug("xiaGetFiPPIName", info_string);


	  if (status != XIA_SUCCESS)
		{
		  sprintf(info_string, "Error getting FiPPI code from FDD file %s @ peaking time = %f", firmwareSet->filename, peakingTime);
		  xiaLogError("xiaGetFiPPIName", info_string, status);
		  return status;
		}
	}

  return XIA_SUCCESS;
}


HANDEL_STATIC int HANDEL_API xiaGetMMUName(Module *module, int channel,
										   char *mmuName,
										   char *rawFilename)
{
  int status;

  char *firmAlias = NULL;

  FirmwareSet *firmware = NULL;


  /* Ultra-paranoid check for a NULL pointer here.
   * Reference: BUG ID #6
   */
  if (module->firmware == NULL) {

	status = XIA_PTR_CHECK;
	xiaLogError("xiaGetMMUName", "module->firmware is NULL when it shouldn't be", status);
	return status;
  }

  /* Ibid. */
  if (module->firmware[channel] == NULL) {

	status = XIA_PTR_CHECK;
	sprintf(info_string, "module->firmware[%d] is NULL when it shouldn't be", channel);
	xiaLogError("xiaGetMMUName", info_string, status);
	return status;
  }

  firmAlias = module->firmware[channel];


  firmware = xiaFindFirmware(firmAlias);

  sprintf(info_string, "Firmware Addr. = %p", firmware);
  xiaLogDebug("xiaGetMMUName", info_string);

  if (firmware->filename == NULL) {

	if (firmware->mmu == NULL) {

	  return XIA_NO_MMU;
	}

	strcpy(mmuName, firmware->mmu);

	strcpy(rawFilename, firmware->mmu); 

  } else {

	/* Do something with the FDD here */
	return XIA_NO_MMU;

  }

  return XIA_SUCCESS;
}

	
