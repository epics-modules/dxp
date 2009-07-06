/*
 * dxp4c2x_psl.c
 *
 * Created 04/26/02 -- PJF
 *
 * Copyright (c) 2002,2003,2004 X-ray Instrumentation Associates
 *               2005, XIA LLC
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


#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "xia_module.h"
#include "xia_system.h"
#include "xia_psl.h"
#include "xia_assert.h"
#include "handel_errors.h"
#include "xia_handel_structures.h"

#include "xerxes.h"
#include "xerxes_errors.h"
#include "xerxes_generic.h"

#include "fdd.h"

#include "xia_common.h"

#include "dxp4c2x.h"

#include "psl_dxp4c2x.h"


#define NUM_BITS_ADC        1024.0f

/* Constants used to set the preset
 * run type.
 */
enum {
    PRESET_STD = 0,
    PRESET_RT,
    PRESET_LT,
    PRESET_OUT,
    PRESET_IN
};

#define IGNORE_MODCHAN 0


/* Prototypes */

PSL_EXPORT int PSL_API dxp4c2x_PSLInit(PSLFuncs *funcs);

ACQUISITION_VALUE(PeakingTime);
ACQUISITION_VALUE(TriggerThreshold);
ACQUISITION_VALUE(EnergyThreshold);
ACQUISITION_VALUE(NumMCAChannels);
ACQUISITION_VALUE(MCALowLimit);
ACQUISITION_VALUE(MCABinWidth);
ACQUISITION_VALUE(ADCPercentRule);
ACQUISITION_VALUE(EnableGate);
ACQUISITION_VALUE(CalibrationEnergy);
ACQUISITION_VALUE(GapTime);
ACQUISITION_VALUE(TriggerPeakingTime);
ACQUISITION_VALUE(TriggerGapTime);
ACQUISITION_VALUE(EnableBaselineCut);
ACQUISITION_VALUE(BaselineCut);
ACQUISITION_VALUE(BaselineFilterLength);
ACQUISITION_VALUE(ResetDelay);
ACQUISITION_VALUE(DecayTime);
ACQUISITION_VALUE(PreampGain);
ACQUISITION_VALUE(DetectorPolarity);

PSL_STATIC int pslSynchPreampGain(int detChan, int det_chan, Module *m,
                                  Detector *det, XiaDefaults *defs);
PSL_STATIC int pslSynchResetDelay(int detChan, int det_chan, Module *m,
                                  Detector *det, XiaDefaults *defs);
PSL_STATIC int pslSynchDecayTime(int detChan, int det_chan, Module *m,
                                 Detector *det, XiaDefaults *defs);
PSL_STATIC int pslSynchDetectorPolarity(int detChan, int det_chan, Module *m,
                                        Detector *det, XiaDefaults *defs);

PSL_STATIC int PSL_API pslDoFilter(int detChan, char *name, void *value, 
				   XiaDefaults *defaults, 
				   FirmwareSet *firmwareSet);
PSL_STATIC int PSL_API pslDoParam(int detChan, char *name, void *value, 
				  XiaDefaults *defaults);
PSL_STATIC int PSL_API pslDoPreset(int detChan, void *value, unsigned short type);
PSL_STATIC int PSL_API _pslDoNSca(int detChan, void *value, Module *m);
PSL_STATIC int PSL_API _pslDoSCA(int detChan, char *name, void *value, Module *m, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslGetMCALength(int detChan, void *value);
PSL_STATIC int PSL_API pslGetMCAData(int detChan, void *value);
PSL_STATIC int PSL_API _pslGetSCAData(int detChan, XiaDefaults *defaults, void *value);
PSL_STATIC int PSL_API pslGetLivetime(int detChan, void *value);
PSL_STATIC int PSL_API pslGetRuntime(int detChan, void *value);
PSL_STATIC int PSL_API pslGetICR(int detChan, void *value);
PSL_STATIC int PSL_API pslGetOCR(int detChan, void *value);
PSL_STATIC int PSL_API pslGetEvents(int detChan, void *value);
PSL_STATIC int PSL_API pslGetTriggers(int detChan, void *value);
PSL_STATIC int PSL_API pslGetBaselineLen(int detChan, void *value);
PSL_STATIC int PSL_API pslGetBaseline(int detChan, void *value);
PSL_STATIC int PSL_API pslGetRunActive(int detChan, void *value);
PSL_STATIC int PSL_API pslDoADCTrace(int detChan, void *info);
PSL_STATIC int PSL_API pslGetADCTraceInfo(int detChan, void *value);
PSL_STATIC int PSL_API pslGetADCTrace(int detChan, void *value);
PSL_STATIC int PSL_API pslDoBaseHistory(int detChan, void *info);
PSL_STATIC int PSL_API pslGetBaseHistoryLen(int detChan, void *value);
PSL_STATIC int PSL_API pslGetBaseHistory(int detChan, void *value);
PSL_STATIC int PSL_API _pslGetSCALen(int detChan, XiaDefaults *defaults, void *value);
PSL_STATIC int PSL_API pslDoOpenRelay(int detChan, void *info);
PSL_STATIC int PSL_API pslDoCloseRelay(int detChan, void *info);
PSL_STATIC int PSL_API pslEndSpecialRun(int detChan);

PSL_STATIC int PSL_API pslCalculateGain(double adcPercentRule, double calibEV, 
					double preampGain, double MCABinWidth, 
					parameter_t SLOWLEN, parameter_t *GAINDAC, 
					double gainScale);
PSL_STATIC double PSL_API pslCalculateSysGain(void);
PSL_STATIC boolean_t PSL_API pslIsUpperCase(char *name);
PSL_STATIC double PSL_API pslGetClockSpeed(int detChan);
PSL_STATIC int PSL_API pslUpdateFilter(int detChan, double peakingTime, 
				       XiaDefaults *defaults, 
				       FirmwareSet *firmwareSet);
PSL_STATIC int PSL_API pslUpdateTriggerFilter(int detChan, XiaDefaults *defaults); 
PSL_STATIC boolean_t PSL_API pslIsInterfaceValid(Module *module);
PSL_STATIC boolean_t PSL_API pslIsEPPAddressValid(Module *module);
PSL_STATIC boolean_t PSL_API pslIsNumChannelsValid(Module *module);
PSL_STATIC boolean_t PSL_API pslAreAllDefaultsPresent(XiaDefaults *defaults);
PSL_STATIC boolean_t PSL_API pslIsNumBinsInRange(XiaDefaults *defaults);

PSL_STATIC int pslQuickRun(int detChan, XiaDefaults *defaults, Module *m);

/* Utilities */
PSL_STATIC int pslWaitForBusy(int detChan, parameter_t BUSY, float poll,
							  float timeout);

/* Board Operations */
PSL_STATIC int pslGetPresetTick(int detChan, char *name, XiaDefaults *defs,
								void *value);
PSL_STATIC int pslCheckExternalMemory(int detChan, char *name, XiaDefaults *defs,
									  void *value);

PSL_STATIC int psl__WaitForRunStart(Module *m);

PSL_STATIC int pslDoGainSetting(int detChan, XiaDefaults *defaults, 
								double preampGain, double gainScale, Module *m);



static AcquisitionValue_t ACQ_VALUES[] = {
  { "peaking_time",           TRUE_, FALSE_, 16.0,   pslDoPeakingTime,
    NULL },

  { "trigger_threshold",      TRUE_, FALSE_, 1000.0, pslDoTriggerThreshold,
    NULL },

  { "mca_bin_width",          TRUE_, FALSE_, 20.0,   pslDoMCABinWidth,
    NULL },

  { "number_mca_channels",    TRUE_, FALSE_, 4096.0, pslDoNumMCAChannels,
    NULL },

  { "mca_low_limit",          TRUE_, FALSE_, 0.0,    pslDoMCALowLimit,
    NULL },

  { "energy_threshold",       TRUE_, FALSE_, 0.0,    pslDoEnergyThreshold,
    NULL },

  { "adc_percent_rule",       TRUE_, FALSE_, 5.0,    pslDoADCPercentRule,
    NULL },

  { "calibration_energy",     TRUE_, FALSE_, 5900.0, pslDoCalibrationEnergy,
    NULL },

  { "gap_time",               TRUE_, FALSE_, 0.150,  pslDoGapTime,
    NULL },

  { "trigger_peaking_time",   TRUE_, FALSE_, 0.200,  pslDoTriggerPeakingTime,
    NULL },

  { "trigger_gap_time",       TRUE_, FALSE_, 0.0,    pslDoTriggerGapTime,
    NULL },

  { "enable_gate",            TRUE_, FALSE_, 0.0,    pslDoEnableGate,
    NULL },

  { "enable_baseline_cut",    TRUE_, FALSE_, 1.0,    pslDoEnableBaselineCut,
    NULL },

  { "baseline_cut",           TRUE_, FALSE_, 5.0,    pslDoBaselineCut,
    NULL },

  { "baseline_filter_length", TRUE_, FALSE_, 256.0,  pslDoBaselineFilterLength,
    NULL },

  { "reset_delay",            TRUE_, TRUE_,  50.0,   pslDoResetDelay,
    pslSynchResetDelay },

  { "decay_time",             TRUE_, TRUE_,  10.0,   pslDoDecayTime,
    pslSynchDecayTime },

  { "preamp_gain",            TRUE_, TRUE_,   2.0,   pslDoPreampGain,
    pslSynchPreampGain },

  { "detector_polarity",      TRUE_, TRUE_,   1.0,   pslDoDetectorPolarity,
    pslSynchDetectorPolarity },
};


static BoardOperation_t boardOps[] = {
  { "get_preset_tick", pslGetPresetTick },
  { "check_memory",    pslCheckExternalMemory },
};

#define NUM_BOARD_OPS (sizeof(boardOps) / sizeof(boardOps[0]))

/*****************************************************************************
 *
 * This routine takes a PSLFuncs structure and points the function pointers
 * in it at the local x10p "versions" of the functions.
 *
 *****************************************************************************/
PSL_EXPORT int PSL_API dxp4c2x_PSLInit(PSLFuncs *funcs)
{
    funcs->validateDefaults     = pslValidateDefaults;
    funcs->validateModule       = pslValidateModule;
    funcs->downloadFirmware     = pslDownloadFirmware;
    funcs->setAcquisitionValues = pslSetAcquisitionValues;
    funcs->getAcquisitionValues = pslGetAcquisitionValues;
    funcs->gainOperation        = pslGainOperation;
    funcs->gainChange           = pslGainChange;
    funcs->gainCalibrate        = pslGainCalibrate;
    funcs->startRun             = pslStartRun;
    funcs->stopRun		= pslStopRun;
    funcs->getRunData		= pslGetRunData;
    funcs->setPolarity		= pslSetPolarity;
    funcs->doSpecialRun		= pslDoSpecialRun;
    funcs->getSpecialRunData	= pslGetSpecialRunData;
    funcs->setDetectorTypeValue = pslSetDetectorTypeValue;
    funcs->getDefaultAlias	= pslGetDefaultAlias;
    funcs->getParameter   	= pslGetParameter;
    funcs->setParameter		= pslSetParameter;
    funcs->userSetup            = pslUserSetup;
    funcs->canRemoveName        = pslCanRemoveName;
    funcs->getNumDefaults       = pslGetNumDefaults;
    funcs->getNumParams         = pslGetNumParams;
    funcs->getParamData         = pslGetParamData;
    funcs->getParamName         = pslGetParamName;
	funcs->boardOperation   = pslBoardOperation;
	funcs->freeSCAs             = pslDestroySCAs;
	funcs->unHook           = pslUnHook;

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine validates module information specific to the dxpx10p
 * product:
 *
 * 1) interface should be of type genericEPP or epp
 * 2) epp_address should be 0x278 or 0x378
 * 3) number_of_channels = 1
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslValidateModule(Module *module)
{
    if (!pslIsInterfaceValid(module)) {

	return XIA_MISSING_INTERFACE;
    }

    if (!pslIsEPPAddressValid(module)) {

	return XIA_MISSING_ADDRESS;
    }

    if (!pslIsNumChannelsValid(module)) {

	return XIA_INVALID_NUMCHANS;
    }
		
    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine validates defaults information specific to the dxpx10p
 * product.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslValidateDefaults(XiaDefaults *defaults)
{
    int status;

	
    if (!pslAreAllDefaultsPresent(defaults)) {

	status = XIA_INCOMPLETE_DEFAULTS;
	sprintf(info_string, "Defaults with alias %s does not contain all defaults", defaults->alias);
	pslLogError("pslValidateDefaults", info_string, status);
	return status;
    }


    if (!pslIsNumBinsInRange(defaults)) {

	status = XIA_BINS_OOR;
	sprintf(info_string, "The number of bins for defaults with alias %s is not in range", defaults->alias);
	pslLogError("pslValidateDefaults", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine verifies that the interface for this module is consistient
 * with a dxpx10p.
 *
 *****************************************************************************/
PSL_STATIC boolean_t PSL_API pslIsInterfaceValid(Module *module)
{
    if ((module->interface_info->type != JORWAY73A) &&
	(module->interface_info->type != GENERIC_SCSI)) {

	return FALSE_;
    }

    return TRUE_;
}


/*****************************************************************************
 *
 * This routine verifies the the epp_address is consistient with the
 * dxpx10p.
 *
 *****************************************************************************/
PSL_STATIC boolean_t PSL_API pslIsEPPAddressValid(Module *module)
{
    if (module->interface_info->info.jorway73a->scsi_bus > 2) {
 
	return FALSE_;
    }

    return TRUE_;
}


/*****************************************************************************
 *
 * This routine verfies that the number of channels is consistient with the 
 * dxpx10p.
 *
 *****************************************************************************/
PSL_STATIC boolean_t PSL_API pslIsNumChannelsValid(Module *module)
{
    UNUSED(module);

    /*
    if (module->number_of_channels != 4) {

	return FALSE_;
    }
    */

    return TRUE_;
}


/*****************************************************************************
 *
 * This routine checks that all of the defaults are present in the defaults
 * associated with this dxpx10p channel.
 *
 *****************************************************************************/
PSL_STATIC boolean_t PSL_API pslAreAllDefaultsPresent(XiaDefaults *defaults)
{
    boolean_t ptPresent         = FALSE_;
    boolean_t trigPresent       = FALSE_;
    boolean_t mcabinPresent     = FALSE_;
    boolean_t nummcaPresent     = FALSE_;
    boolean_t mcalowPresent     = FALSE_;
    boolean_t enerPresent       = FALSE_;
    boolean_t adcPresent	    = FALSE_;
    boolean_t energyPresent     = FALSE_;
    boolean_t gaptimePresent    = FALSE_;
    boolean_t triggerPTPresent  = FALSE_;
    boolean_t triggerGapPresent = FALSE_;

    XiaDaqEntry *current = NULL;


    current = defaults->entry;
    while (current != NULL) {

	if (STREQ(current->name, "peaking_time")) {

	    ptPresent = TRUE_;
		
	} else if (STREQ(current->name, "trigger_threshold")) {

	    trigPresent = TRUE_;

	} else if (STREQ(current->name, "mca_bin_width")) {

	    mcabinPresent = TRUE_;

	} else if (STREQ(current->name, "number_mca_channels")) {

	    nummcaPresent = TRUE_;

	} else if (STREQ(current->name, "mca_low_limit")) {

	    mcalowPresent = TRUE_;
		
	} else if (STREQ(current->name, "energy_threshold")) {

	    enerPresent = TRUE_;

	} else if (STREQ(current->name, "adc_percent_rule")) {

	    adcPresent = TRUE_;
		
	} else if (STREQ(current->name, "calibration_energy")) {
			
	    energyPresent = TRUE_;
	
	} else if (STREQ(current->name, "gap_time")) {

	    gaptimePresent = TRUE_;

	} else if (STREQ(current->name, "trigger_peaking_time")) {

	    triggerPTPresent = TRUE_;

	} else if (STREQ(current->name, "trigger_gap_time")) {

	    triggerGapPresent = TRUE_;
	}

	current = getListNext(current);
    }

    if ((ptPresent && trigPresent && mcabinPresent && nummcaPresent && 
	 mcalowPresent && enerPresent && adcPresent && energyPresent &&
	 gaptimePresent && triggerPTPresent && triggerGapPresent) != TRUE_) {

	return FALSE_;
    }

    return TRUE_;
}


/*****************************************************************************
 *
 * This routine checks that the number_mca_bins is consistient with the 
 * allowed values for a dxpx10p.
 *
 *****************************************************************************/
PSL_STATIC boolean_t PSL_API pslIsNumBinsInRange(XiaDefaults *defaults)
{
    int status;

    double numChans = 0.0;


    status = pslGetDefault("number_mca_channels", (void *)&numChans, defaults);

    if (status != XIA_SUCCESS) {

	return FALSE_;
    }

    if ((numChans > 8192.0) ||
	(numChans < 0.0)) {

	return FALSE_;
    }

    return TRUE_;
}


/** @brief Downloads the requested firmware to the hardware if it
 * has not already been downloaded.
 *
 */
PSL_STATIC int pslDownloadFirmware(int detChan, char *type, char *file,
                                   Module *m, char *rawFilename,
                                   XiaDefaults *defs)
{
  int status;
  int statusX;

  unsigned int modChan;

  CurrentFirmware *cf = NULL;

  UNUSED(defs);

	
  ASSERT(m != NULL);
  ASSERT(type != NULL);
  ASSERT(file != NULL);
  ASSERT(rawFilename != NULL);


  cf = m->currentFirmware;

  /* Immediately dismiss the types that aren't supported (currently) by the
   * 2X.
   */
  if (STREQ(type, "user_fippi") || STREQ(type, "mmu")) {
    sprintf(info_string, "Requested firmware type '%s' is not supported "
            "for the DXP-2X4C hardware", type);
    pslLogError("pslDownloadFirmware", info_string, XIA_NOSUPPORT_FIRM);
    return XIA_NOSUPPORT_FIRM;
  }

  status = pslGetModChan(detChan, m, &modChan);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error getting modChan for module '%s' and detChan %d",
            m->alias, detChan);
    pslLogError("pslDownloadFirmware", info_string, status);
    return status;
  }

  if (STREQ(type, "dsp")) {

	sprintf(info_string, "Current DSP = '%s'", cf[modChan].currentDSP);
	pslLogDebug("pslDownloadFirmware", info_string);
	sprintf(info_string, "Raw Filename = '%s'", rawFilename);
	pslLogDebug("pslDownloadFirmware", info_string);
	sprintf(info_string, "File = '%s'", file);
	pslLogDebug("pslDownloadFirmware", info_string);


	if (!STREQ(rawFilename, cf[modChan].currentDSP)) {

      statusX = dxp_replace_dspconfig(&detChan, file);

      if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error replacing current DSP config with '%s' on "
                "detChan %d", file, detChan);
        pslLogError("pslDownloadFirmware", info_string, XIA_XERXES);
		return XIA_XERXES;
      }

      strcpy(cf[modChan].currentDSP, rawFilename);
	}
		
  } else if (STREQ(type, "fippi")) {

	sprintf(info_string, "Current FiPPI = '%s'", cf[modChan].currentFiPPI);
	pslLogDebug("pslDownloadFirmware", info_string);
	sprintf(info_string, "Raw filename = '%s'", rawFilename);
	pslLogDebug("pslDownloadFirmware", info_string);
	sprintf(info_string, "File = '%s'", file);
	pslLogDebug("pslDownloadFirmware", info_string);


	if (!STREQ(rawFilename, cf[modChan].currentFiPPI)) {

      statusX = dxp_replace_fpgaconfig(&detChan, "fippi", file);

      if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error replacing current FiPPI config with '%s' on "
                "detChan %d", file, detChan);
        pslLogError("pslDownloadFirmware", info_string, XIA_XERXES);
		return XIA_XERXES;
      }

      strcpy(cf[modChan].currentFiPPI, rawFilename);
	}

  } else {
    sprintf(info_string, "Requested firmware type '%s' is unknown for "
            "module '%s'", type, m->alias);
    pslLogError("pslDownloadFirmware", info_string, XIA_UNKNOWN_FIRM);
	return XIA_UNKNOWN_FIRM;
  }

  return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine calculates the appropriate DSP parameter(s) from the name and
 * then downloads it/them to the board. 
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslSetAcquisitionValues(int detChan, char *name,
											   void *value, 
                                               XiaDefaults *defaults, 
                                               FirmwareSet *firmwareSet, 
                                               CurrentFirmware *currentFirmware, 
                                               char *detectorType,
											   double gainScale,
											   Detector *detector, 
											   int detector_chan,
											   Module *m, int modChan)
{
    int status = XIA_SUCCESS;
    int statusX;

	UNUSED(currentFirmware);
	UNUSED(modChan);


    /* All of the calculations are dispatched to the appropriate routine. This 
     * way, if the calculation ever needs to change, which it will, we don't
     * have to search in too many places to find the implementation.
     */
    if (STREQ(name, "peaking_time")) {

	status = pslDoPeakingTime(detChan, value, firmwareSet, detectorType,
							  defaults, detector->gain[detector_chan], gainScale,
                              m, detector, detector_chan);
	
    } else if (STREQ(name, "trigger_threshold")) {

      status = pslDoTriggerThreshold(detChan, value, firmwareSet, detectorType,
                                     defaults, detector->gain[detector_chan],
                                     gainScale, m, detector, detector_chan);

    } else if (STREQ(name, "energy_threshold")) {

      status = pslDoEnergyThreshold(detChan, value, firmwareSet, detectorType,
                                    defaults, detector->gain[detector_chan],
                                    gainScale, m, detector, detector_chan);

    } else if (STREQ(name, "number_mca_channels")) {

      status = pslDoNumMCAChannels(detChan, value, firmwareSet, detectorType,
                                   defaults, detector->gain[detector_chan],
                                   gainScale, m, detector, detector_chan);

    } else if (STREQ(name, "mca_low_limit")) {

      status = pslDoMCALowLimit(detChan, value, firmwareSet, detectorType,
                                defaults, detector->gain[detector_chan], gainScale,
                                m, detector, detector_chan);
	
    } else if (STREQ(name, "mca_bin_width")) {

      status = pslDoMCABinWidth(detChan, value, firmwareSet, detectorType,
                                defaults, detector->gain[detector_chan], gainScale,
                                m, detector, detector_chan);

    } else if (STREQ(name, "adc_percent_rule")) {

      status = pslDoADCPercentRule(detChan, value, firmwareSet, detectorType,
                                   defaults, detector->gain[detector_chan],
                                   gainScale, m, detector, detector_chan);

    } else if (STREQ(name, "enable_gate")) {

      status = pslDoEnableGate(detChan, value, firmwareSet, detectorType,
                               defaults, detector->gain[detector_chan],
                               gainScale, m, detector, detector_chan);

    } else if (STREQ(name, "calibration_energy")) {

      status = pslDoCalibrationEnergy(detChan, value, firmwareSet, detectorType,
                                      defaults, detector->gain[detector_chan],
                                      gainScale, m, detector, detector_chan);

    } else if (STREQ(name, "gap_time")) {

      status = pslDoGapTime(detChan, value, firmwareSet, detectorType, defaults,
                            detector->gain[detector_chan], gainScale, m, detector,
                            detector_chan);

    } else if (STREQ(name, "trigger_peaking_time")) {

      status = pslDoTriggerPeakingTime(detChan, value, firmwareSet, detectorType,
                                       defaults, detector->gain[detector_chan],
                                       gainScale, m, detector, detector_chan);

    } else if (STREQ(name, "trigger_gap_time")) {

      status = pslDoTriggerGapTime(detChan, value, firmwareSet, detectorType,
                                   defaults, detector->gain[detector_chan],
                                   gainScale, m, detector, detector_chan);

    } else if (STREQ(name, "enable_baseline_cut")) {

      status = pslDoEnableBaselineCut(detChan, value, firmwareSet, detectorType,
                                      defaults, detector->gain[detector_chan],
                                      gainScale, m, detector, detector_chan);

    } else if (STREQ(name, "baseline_cut")) {

      status = pslDoBaselineCut(detChan, value, firmwareSet, detectorType,
                                defaults, detector->gain[detector_chan],
                                gainScale, m, detector, detector_chan);

    } else if (STREQ(name, "baseline_filter_length")) {

      status = pslDoBaselineFilterLength(detChan, value, firmwareSet, detectorType,
                                         defaults, detector->gain[detector_chan],
                                         gainScale, m, detector, detector_chan);

    } else if (STREQ(name, "reset_delay")) {

      status = pslDoResetDelay(detChan, value, firmwareSet, detectorType,
                               defaults, detector->gain[detector_chan],
                               gainScale, m, detector, detector_chan);

    } else if (STREQ(name, "decay_time")) {

      status = pslDoDecayTime(detChan, value, firmwareSet, detectorType,
                              defaults, detector->gain[detector_chan],
                              gainScale, m, detector, detector_chan);

    } else if (STREQ(name, "preamp_gain")) {

      status = pslDoPreampGain(detChan, value, firmwareSet, detectorType,
                               defaults, detector->gain[detector_chan],
                               gainScale, m, detector, detector_chan);

    } else if (STREQ(name, "detector_polarity")) {

      status = pslDoDetectorPolarity(detChan, value, firmwareSet, detectorType,
                                     defaults, detector->gain[detector_chan],
                                     gainScale, m, detector, detector_chan);

    } else if (STREQ(name, "preset_standard")) {

	status = pslDoPreset(detChan, value, PRESET_STD);

    } else if (STREQ(name, "preset_runtime")) {

	status = pslDoPreset(detChan, value, PRESET_RT);

    } else if (STREQ(name, "preset_livetime")) {

	status = pslDoPreset(detChan, value, PRESET_LT);

    } else if (STREQ(name, "preset_output")) {

	status = pslDoPreset(detChan, value, PRESET_OUT);

    } else if (STREQ(name, "preset_input")) {

	status = pslDoPreset(detChan, value, PRESET_IN); 
	
    } else if ((strncmp(name, "peakint", 7) == 0) ||
	       (strncmp(name, "peaksam", 7) == 0)) {

	status = pslDoFilter(detChan, name, value, defaults, firmwareSet);

	} else if (STRNEQ(name, "number_of_scas")) {
	  
	  status = _pslDoNSca(detChan, value, m);

	} else if (STRNEQ(name, "sca")) {
	  status = _pslDoSCA(detChan, name, value, m, defaults);

    } else if (pslIsUpperCase(name)) {

	status = pslDoParam(detChan, name, value, defaults);
	
    } else {

	status = XIA_UNKNOWN_VALUE;
    }

    if (status != XIA_SUCCESS) {
      sprintf(info_string, "Error processing acquisition value '%s' for "
              "detChan %d", name, detChan);
      pslLogError("pslSetAcquisitionValues", info_string, status);
      return status;
    }

    statusX = dxp_upload_dspparams(&detChan);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	pslLogError("pslSetAcquisitionValues", "Error uploading params through Xerxes", status);
	return status;
    }

    return XIA_SUCCESS;	
}


/*****************************************************************************
 *
 * This routine does all of the steps required in modifying the peaking time
 * for a given X10P detChan:
 * 
 * 1) Change FiPPI if necessary
 * 2) Update Filter Parameters
 * 3) Return "calculated" Peaking Time
 *
 *****************************************************************************/
ACQUISITION_VALUE(PeakingTime)
{
    int statusX;
    int status;

    double peakingTime;

    char *tmpPath = NULL;

    char fippi[MAX_PATH_LEN];
    char rawFilename[MAXFILENAME_LEN];

    Firmware *firmware = NULL;

    parameter_t curDecimation = 0;
    parameter_t newDecimation = 0;
    parameter_t SLOWLEN       = 0;
    
    UNUSED(detector_chan);
    UNUSED(det);
    UNUSED(gainScale);
    UNUSED(preampGain);


    CLOCK_SPEED = pslGetClockSpeed(detChan);


	ASSERT(m != NULL);


    /* Do we still need this??? */
    statusX = dxp_get_one_dspsymbol(&detChan, "DECIMATION", &curDecimation);
	
    if (statusX != DXP_SUCCESS) {
	
	status = XIA_XERXES;
	sprintf(info_string, "Error getting DECIMATIOn from detChan %d", detChan);
	pslLogError("pslDoPeakingTime", info_string, status);
	return status;
    }
 
    peakingTime = *((double *)value);

    /* The code below is replacing an old algorithm that used to check the decimation
     * instead of the filename to determine if firmware needs to be downloaded or not.
     * 
     * All of the comparison code is going to be in pslDownloadFirmware().
     *
     */
    if (fs->filename == NULL) {

	firmware = fs->firmware;
	while (firmware != NULL) {

	    if ((peakingTime >= firmware->min_ptime) &&
		(peakingTime <= firmware->max_ptime)) {

		strcpy(fippi, firmware->fippi);
		strcpy(rawFilename, firmware->fippi);
		break;
	    }
	
	    firmware = getListNext(firmware);
	}

    } else {
	/* filename is actually defined in this case */
	sprintf(info_string, "peakingTime = %.3f", peakingTime);
	pslLogDebug("pslDoPeakingTime", info_string);

  if (fs->tmpPath) {
    tmpPath = fs->tmpPath;

  } else {
    tmpPath = utils->funcs->dxp_md_tmp_path();
  }

  status = xiaFddGetFirmware(fs->filename, tmpPath, "fippi", 
                             peakingTime,
                             (unsigned short)fs->numKeywords, 
                             fs->keywords, detectorType,
                             fippi, rawFilename);

	if (status != XIA_SUCCESS) {
	  
	    sprintf(info_string, "Error getting FiPPI file from %s at peaking time = %.3f", 
		    fs->filename, peakingTime);
	    pslLogError("pslDoPeakingTime", info_string, status);
	    return status;
	}
    }

    status = pslDownloadFirmware(detChan, "fippi", fippi, m, rawFilename, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error downloading FiPPI to detChan %d", detChan);
	pslLogError("pslDoPeakingTime", info_string, status);
	return status;
    }
	
    pslLogDebug("pslDoPeakingTime", "Preparing to call pslUpdateFilter()");

    status = pslUpdateFilter(detChan, peakingTime, defs, fs);

    if (status != XIA_SUCCESS) {
	
	sprintf(info_string, "Error updating filter information for detChan %d", detChan);
	pslLogError("pslDoPeakingTime", info_string, status);
	return status;
    }
 
    /* Need to get new SLOWLEN value from board */
    statusX = dxp_get_one_dspsymbol(&detChan, "SLOWLEN", &SLOWLEN);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting SLOWLEN for detChan %d", detChan);
	pslLogError("pslDoPeakingTime", info_string, status);
	return status;
    }

    statusX = dxp_get_one_dspsymbol(&detChan, "DECIMATION", &newDecimation);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting DECIMATION for detChan %d", detChan);
	pslLogError("pslDoPeakingTime", info_string, status);
	return status;
    }

    /* Set value equal to the new "real" peaking time... */
    peakingTime = (((double)SLOWLEN) * (1 / CLOCK_SPEED) * pow(2, (double)newDecimation));
    *((double *)value) = peakingTime;

    sprintf(info_string, "New peaking time = %.3f", peakingTime);
    pslLogDebug("pslDoPeakingTime", info_string);
  
    status = pslSetDefault("peaking_time", (void *)&peakingTime, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error setting peaking_time for detChan %d", detChan);
	pslLogError("pslDoPeakingTime", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine sets the Trigger Threshold parameter on the DSP code based on
 * calculations from values in the defaults (when required) or those that
 * have been passed in.
 *
 *****************************************************************************/
ACQUISITION_VALUE(TriggerThreshold)
{
    int statusX;
    int status;

    double eVPerADC = 0.0;
    double adcPercentRule =0.0;
    double dTHRESHOLD = 0.0;
    double thresholdEV = 0.0;
    double calibEV = 0.0;

    parameter_t FASTLEN;
    parameter_t THRESHOLD;

    UNUSED(detector_chan);
    UNUSED(det);
    UNUSED(m);
    UNUSED(gainScale);
    UNUSED(preampGain);
    UNUSED(detectorType);
    UNUSED(fs);
	

    status = pslGetDefault("adc_percent_rule", (void *)&adcPercentRule, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting adc_percent_rule from detChan %d", detChan);
	pslLogError("pslDoTriggerThreshold", info_string, status);
	return status;
    }

    status = pslGetDefault("calibration_energy", (void *)&calibEV, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting calibration_energy from detChan %d", detChan);
	pslLogError("pslDoTriggerThreshold", info_string, status);
	return status;
    }

    eVPerADC = (double)(calibEV / ((adcPercentRule / 100.0) * NUM_BITS_ADC));

    statusX = dxp_get_one_dspsymbol(&detChan, "FASTLEN", &FASTLEN);
	
    if (statusX != DXP_SUCCESS) {
	
	status = XIA_XERXES;
	sprintf(info_string, "Error getting FASTLEN from detChan %d", detChan);
	pslLogError("pslDoTriggerThreshold", info_string, status);
	return status;
    }

    thresholdEV = *((double *)value);
    dTHRESHOLD  = ((double)FASTLEN * thresholdEV) / eVPerADC;
    THRESHOLD   = (parameter_t)ROUND(dTHRESHOLD);
	
    /* The actual range to use is 0 < THRESHOLD < 255, but since THRESHOLD
     * is an unsigned short, any "negative" errors should show up as 
     * sign extension problems and will be caught by THRESHOLD > 255.
     */
    if (THRESHOLD > 255) {

	return XIA_THRESH_OOR;
    }


    statusX = dxp_set_one_dspsymbol(&detChan, "THRESHOLD", &THRESHOLD);

    if (statusX != DXP_SUCCESS) {
	
	status = XIA_XERXES;
	sprintf(info_string, "Error getting THRESHOLD from detChan %d", detChan);
	pslLogError("pslDoTriggerThreshold", info_string, status);
	return status;
    }

    /* Re-"calculate" the actual threshold. This _is_ a deterministic process since the
     * specified value of the threshold is only modified due to rounding...
     */
    thresholdEV = ((double)THRESHOLD * eVPerADC) / ((double) FASTLEN);
    *((double *)value) = thresholdEV;

    status = pslSetDefault("trigger_threshold", (void *)&thresholdEV, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error setting trigger_threshold for detChan %d", detChan);
	pslLogError("pslDoTriggerThreshold", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine translates the Energy Threshold value (in eV) into the 
 * appropriate DSP parameter.
 *
 *****************************************************************************/
ACQUISITION_VALUE(EnergyThreshold)
{
    int statusX;
    int status;

    double eVPerADC = 0.0;
    double adcPercentRule = 0.0;
    double dSLOWTHRESH = 0.0;
    double slowthreshEV = 0.0;
    double calibEV = 0.0;

    parameter_t SLOWLEN;
    parameter_t SLOWTHRESH;

    UNUSED(detector_chan);
    UNUSED(det);
    UNUSED(m);
    UNUSED(gainScale);
    UNUSED(preampGain);
    UNUSED(detectorType);
    UNUSED(fs);
	

    status = pslGetDefault("adc_percent_rule", (void *)&adcPercentRule, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting adc_percent_rule from detChan %d", detChan);
	pslLogError("pslDoEnergyThreshold", info_string, status);
	return status;
    }

    status = pslGetDefault("calibration_energy", (void *)&calibEV, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting calibration_energy from detChan %d", detChan);
	pslLogError("pslDoEnergyThreshold", info_string, status);
	return status;
    }


    eVPerADC = (double)(calibEV / ((adcPercentRule / 100.0) * NUM_BITS_ADC));

    statusX = dxp_get_one_dspsymbol(&detChan, "SLOWLEN", &SLOWLEN);
	
    if (statusX != DXP_SUCCESS) {
	
	status = XIA_XERXES;
	sprintf(info_string, "Error getting SLOWLEN from detChan %d", detChan);
	pslLogError("pslDoEnergyThreshold", info_string, status);
	return status;
    }

    slowthreshEV = *((double *)value);
    dSLOWTHRESH  = ((double)SLOWLEN * slowthreshEV) / eVPerADC;
    SLOWTHRESH   = (parameter_t)ROUND(dSLOWTHRESH);

    statusX = dxp_set_one_dspsymbol(&detChan, "SLOWTHRESH", &SLOWTHRESH);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting SLOWTHRESH from detChan %d", detChan);
	pslLogError("pslDoEnergyThreshold", info_string, status);
	return status;
    }

    /* Re-"calculate" the actual threshold. This _is_ a deterministic process since the
     * specified value of the threshold is only modified due to rounding...
     */
    slowthreshEV = ((double)SLOWTHRESH * eVPerADC) / ((double)SLOWLEN);
    *((double *)value) = slowthreshEV;

    status = pslSetDefault("energy_threshold", (void *)&slowthreshEV, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error setting energy_threshold for detChan %d", detChan);
	pslLogError("pslDoEnergyThreshold", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine essentially sets the values of MCALIMHI.
 *
 *****************************************************************************/
ACQUISITION_VALUE(NumMCAChannels)
{
    int statusX;
    int status;

    double numMCAChans;

    parameter_t MCALIMLO;
    parameter_t MCALIMHI;

    UNUSED(detector_chan);
    UNUSED(det);
    UNUSED(m);
    UNUSED(gainScale);
    UNUSED(preampGain);
    UNUSED(detectorType);
    UNUSED(fs);


    numMCAChans = *((double *)value);

    if ((numMCAChans < 0) ||
	(numMCAChans > 8192)) {

	status = XIA_BINS_OOR;
	sprintf(info_string, "Too many bins specified: %.3f", numMCAChans);
	pslLogError("pslDoNumMCAChannels", info_string, status);
	return status;
    }

    statusX = dxp_get_one_dspsymbol(&detChan, "MCALIMLO", &MCALIMLO);

    if (statusX != DXP_SUCCESS) {
	
	status = XIA_XERXES;
	sprintf(info_string, "Error getting MCALIMLO from detChan %d", detChan);
	pslLogError("pslDoNumMCAChannels", info_string, status);
	return status;
    }

    MCALIMHI = (parameter_t)ROUND(numMCAChans);

    /* Need to do another range check here. There is a little ambiguity in the
     * calculation if the user chooses to run with MCALIMLO not set to zero.
     */
    if (MCALIMHI > 8192) {
	
	status = XIA_BINS_OOR;
	sprintf(info_string, "Maximum bin # is out-of-range: %u", MCALIMHI);
	pslLogError("pslDoNumMCAChannels", info_string, status);
	return status;
    }

    statusX = dxp_set_one_dspsymbol(&detChan, "MCALIMHI", &MCALIMHI);

    if (statusX != DXP_SUCCESS) {
	
	status = XIA_XERXES;
	sprintf(info_string, "Error setting MCALIMHI for detChan %d", detChan);
	pslLogError("pslDoNumMCAChannels", info_string, status);
	return status;
    }

    status = pslSetDefault("number_mca_channels", (void *)&numMCAChans, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error setting number_mca_channels for detChan %d", detChan);
	pslLogError("pslDoNumMCAChannels", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine sets the low bin for acquisition.
 *
 *****************************************************************************/
ACQUISITION_VALUE(MCALowLimit)
{
    int statusX;
    int status;

    double dMCALIMLO;
    double eVPerBin = 0.0;
    double lowLimitEV;

    parameter_t MCALIMLO;
    parameter_t MCALIMHI;

    UNUSED(detector_chan);
    UNUSED(det);
    UNUSED(m);
    UNUSED(gainScale);
    UNUSED(preampGain);
    UNUSED(detectorType);
    UNUSED(fs);

 
    status = pslGetDefault("mca_bin_width", (void *)&eVPerBin, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting mca_bin_width for detChan %d", detChan);
	pslLogError("pslDoMCALowLimit", info_string, status);
	return status;
    }

    lowLimitEV = *((double *)value);
    dMCALIMLO  = (double)(lowLimitEV / eVPerBin);
    MCALIMLO   = (parameter_t)ROUND(dMCALIMLO);

    /* Compare MCALIMLO to MCALIMHI - 1 */
    statusX = dxp_get_one_dspsymbol(&detChan, "MCALIMHI", &MCALIMHI);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting MCALIMHI from detChan %d", detChan);
	pslLogError("pslDoMCALowLimit", info_string, status);
	return status;
    }

    if (MCALIMLO > (MCALIMHI - 1)) {
	
	status = XIA_BINS_OOR;
	sprintf(info_string, "Low MCA channel is specified too high: %u (MCALIMHI = %u)", 
		MCALIMLO, MCALIMHI);
	pslLogError("pslDoMCALowLimit", info_string, status);
	return status;
    }

    statusX = dxp_set_one_dspsymbol(&detChan, "MCALIMLO", &MCALIMLO);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error setting MCALIMLO for detChan %d", detChan);
	pslLogError("pslDoMCALowLimit", info_string, status);
	return status;
    }

    status = pslSetDefault("mca_low_limit", (void *)&dMCALIMLO, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error setting mca_low_limit for detChan %d", detChan);
	pslLogError("pslDoMCALowLimit", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine sets the bin width, through the parameter BINFACT.
 *
 *****************************************************************************/
ACQUISITION_VALUE(MCABinWidth)
{
    int statusX;
    int status;

    double eVPerADC = 0.0;
    double eVPerBin = 0.0;
    double adcPercentRule =0.0;
    double calibEV = 0.0;
    double dBINFACT1 = 0.0;

    parameter_t BINFACT1;
    parameter_t SLOWLEN;

    UNUSED(detector_chan);
    UNUSED(det);
    UNUSED(m);
    UNUSED(gainScale);
    UNUSED(preampGain);
    UNUSED(detectorType);
    UNUSED(fs);


    status = pslGetDefault("adc_percent_rule", (void *)&adcPercentRule, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting adc_percent_rule for detChan %d", detChan);
	pslLogError("pslDoMCABinWidth", info_string, status);
	return status;
    }

    status = pslGetDefault("calibration_energy", (void *)&calibEV, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting calibration_energy for detChan %d", detChan);
	pslLogError("pslDoMCABinWidth", info_string, status);
	return status;
    }

    eVPerBin = *((double *)value);

    sprintf(info_string, "eVperBin = %.3f", eVPerBin);
    pslLogDebug("pslDoMCABinWidth", info_string);

    statusX = dxp_get_one_dspsymbol(&detChan, "SLOWLEN", &SLOWLEN);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting SLOWLEN from detChan %d", detChan);
	pslLogError("pslDoMCABinWidth", info_string, status);
	return status;
    }

    /* Now we can calculate BINFACT1 from the following equation:
     * 
     * BINFACT1 = [(eVPerBin / eVPerADC) * SLOWLEN * 4]
     */
    eVPerADC  = (double)(calibEV / ((adcPercentRule / 100.0) * NUM_BITS_ADC));
    dBINFACT1 = (eVPerBin / eVPerADC) * (double)SLOWLEN * 4.0;
    BINFACT1  = (parameter_t)ROUND(dBINFACT1);

    statusX = dxp_set_one_dspsymbol(&detChan, "BINFACT1", &BINFACT1);

    if (statusX != DXP_SUCCESS) {
	
	status = XIA_XERXES;
	sprintf(info_string, "Error setting BINFACT1 for detChan %d", detChan);
	pslLogError("pslDoMCABinWidth", info_string, status);
	return status;
    }

    /* Return the value of eV/Bin that the user thinks they are setting and 
     * then compensate for the "discreteness" of BINFACT1 by scaling the 
     * gain.
     */

    status = pslSetDefault("mca_bin_width", (void *)&eVPerBin, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error setting mca_bin_width for detChan %d", detChan);
	pslLogError("pslDoMCABinWidth", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * Actually changes the value of the defaults setting AND recalculates the
 * parameters that depend on the percent rule. This is a little different 
 * then if you just modified the percent rule through the defaults, since
 * that doesn't recalculate any of the other acquisition parameters.
 *
 *****************************************************************************/
ACQUISITION_VALUE(ADCPercentRule)
{	
    int status;
    int statusX;

    double adcPercentRule = 0.0;
    double oldADCPercentRule = 0.0;
    double calibEV = 0.0;
    double mcaBinWidth = 0.0;
    double eVPerADC = 0.0;
    double threshold = 0.0;
    double slowthresh = 0.0;

    parameter_t GAINDAC;
    parameter_t SLOWLEN;
    parameter_t THRESHOLD;
    parameter_t SLOWTHRESH;
    parameter_t FASTLEN;


    adcPercentRule = *((double *)value);


    status = pslGetDefault("adc_percent_rule", (void *)&oldADCPercentRule, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting adc_percent_rule for detChan %d", detChan);
	pslLogError("pslDoADCPercentRule", info_string, status);
	return status;
    }

    status = pslSetDefault("adc_percent_rule", (void *)&adcPercentRule, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error setting adc_percent_rule for detChan %d", detChan);
	pslLogError("pslDoADCPercentRule", info_string, status);
	return status;
    }

    status = pslGetDefault("calibration_energy", (void *)&calibEV, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting calibration_energy for detChan %d", detChan);
	pslLogError("pslDoADCPercentRule", info_string, status);
	return status;
    }

    status = pslGetDefault("mca_bin_width", (void *)&mcaBinWidth, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting mca_bin_width for detChan %d", detChan);
	pslLogError("pslDoADCPercentRule", info_string, status);
	return status;
    }

    statusX = dxp_get_one_dspsymbol(&detChan, "SLOWLEN", &SLOWLEN);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting SLOWLEN from detChan %d", detChan);
	pslLogError("pslDoADCPercentRule", info_string, status);
	return status;
    }

    /* Calculate and set the gain */
    status = pslCalculateGain(adcPercentRule, calibEV, preampGain, mcaBinWidth, SLOWLEN, &GAINDAC, gainScale); 
	
    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error calculating gain for detChan %d", detChan);
	pslLogError("pslDoADCPercentRule", info_string, status);
	return status;
    }

    statusX = dxp_set_one_dspsymbol(&detChan, "GAINDAC", &GAINDAC);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error setting GAINDAC for detChan %d", detChan);
	pslLogError("pslDoADCPercentRule", info_string, status);
	return XIA_XERXES;
    }

    status = pslSetDefault("adc_percent_rule", (void *)&adcPercentRule, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error setting adc_percent_rule for detChan %d", detChan);
	pslLogError("pslDoADCPercentRule", info_string, status);
	return status;
    }

    /* Call routines that depend on the adc_percent_rule so that
     * there calculations will now be correct.
     */

    /* Use the "old" adc rule since
     * that is what the previous calculation of the trigger threshold
     * is based on.
     */
    statusX = dxp_get_one_dspsymbol(&detChan, "THRESHOLD", &THRESHOLD);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting THRESHOLD for detChan %d", detChan);
	pslLogError("pslDoADCPercentRule", info_string, status);
	return status;
    }

    statusX = dxp_get_one_dspsymbol(&detChan, "FASTLEN", &FASTLEN);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting FASTLEN for detChan %d", detChan);
	pslLogError("pslDoADCPercentRule", info_string, status);
	return status;
    }

    eVPerADC = (double)(calibEV / ((oldADCPercentRule / 100.0) * NUM_BITS_ADC));
    threshold = ((double)THRESHOLD * eVPerADC) / (double)FASTLEN;

    status = pslDoTriggerThreshold(detChan, (void *)&threshold, fs, detectorType,
                                   defs, preampGain, gainScale, m, det,
                                   detector_chan);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error updating trigger threshold for detChan %d", detChan);
	pslLogError("pslDoADCPercentRule", info_string, status);
	return status;
    }
	
    statusX = dxp_get_one_dspsymbol(&detChan, "SLOWTHRESH", &SLOWTHRESH);

    if (statusX != DXP_SUCCESS) {
	
	status = XIA_XERXES;
	sprintf(info_string, "Error getting SLOWTHRESH from detChan %d", detChan);
	pslLogError("pslDoADCPercentRule", info_string, status);
	return status;
    }

    slowthresh = ((double)SLOWTHRESH * eVPerADC) / (double)SLOWLEN;

    status = pslDoEnergyThreshold(detChan, (void *)&slowthresh, fs, detectorType,
                                  defs, preampGain, gainScale, m, det,
                                  detector_chan);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error updating energy threshold for detChan %d", detChan);
	pslLogError("pslDoADCPercentRule", info_string, status);
	return status;
    }

    status = pslDoMCABinWidth(detChan, (void *)&mcaBinWidth, fs, detectorType,
                              defs, preampGain, gainScale, m, det,
                              detector_chan);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error updating MCA bin width for detChan %d", detChan);
	pslLogError("pslDoADCPercentRule", info_string, status);
	return status;
    }

    /* Calculate and set the gain (again). It's done twice, for now, to
     * protect against the case where we change all of these other parameters
     * and then find out the gain is out-of-range and, therefore, the whole
     * system is out-of-sync.
     */
    status = pslCalculateGain(adcPercentRule, calibEV, preampGain, mcaBinWidth, SLOWLEN, &GAINDAC, gainScale); 
	
    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error calculating gain for detChan %d", detChan);
	pslLogError("pslDoADCPercentRule", info_string, status);
	return status;
    }

    statusX = dxp_set_one_dspsymbol(&detChan, "GAINDAC", &GAINDAC);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error setting GAINDAC for detChan %d", detChan);
	pslLogError("pslDoADCPercentRule", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine checks to see if the "enable gate" default is defined. If it
 * isn't then it just ignores the fact that this function is called. If this
 * routine was really cool then it would add that as a default, but it can't
 * so it won't.
 *
 *****************************************************************************/
ACQUISITION_VALUE(EnableGate)
{
  XiaDaqEntry *current = NULL;

  UNUSED(detector_chan);
  UNUSED(det);
  UNUSED(m);
  UNUSED(gainScale);
  UNUSED(preampGain);
  UNUSED(detectorType);
  UNUSED(fs);
  UNUSED(detChan);


    current = defs->entry;

    while ((current != NULL) &&
	   (!STREQ(current->name, "enable_gate"))) {

	current = getListNext(current);
    }

    if ((current == NULL) &&
	(!STREQ(current->name, "enable_gate"))) {

	return XIA_SUCCESS;
    }

    current->data = *((double *)value);

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * Like pslDoADCPercentRule(), this routine recalculates the gain from the
 * calibration energy point of view.
 *
 *****************************************************************************/
ACQUISITION_VALUE(CalibrationEnergy)
{
    int status;
    int statusX;

    double adcPercentRule = 0.0;
    double oldCalibEV = 0.0;
    double calibEV = 0.0;
    double mcaBinWidth = 0.0;
    double eVPerADC = 0.0;
    double threshold = 0.0;
    double slowthresh = 0.0;

    parameter_t GAINDAC;
    parameter_t SLOWLEN;
    parameter_t THRESHOLD;
    parameter_t SLOWTHRESH;
    parameter_t FASTLEN;


    calibEV = *((double *)value);


    status = pslGetDefault("adc_percent_rule", (void *)&adcPercentRule, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting adc_percent_rule for detChan %d", detChan);
	pslLogError("pslDoCalibrationEnergy", info_string, status);
	return status;
    }

    status = pslGetDefault("calibration_energy", (void *)&oldCalibEV, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting adc_percent_rule for detChan %d", detChan);
	pslLogError("pslDoCalibrationEnergy", info_string, status);
	return status;
    }

    status = pslSetDefault("calibration_energy", (void *)&calibEV, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error setting calibration_energy for detChan %d", detChan);
	pslLogError("pslDoCalibrationEnergy", info_string, status);
	return status;
    }

    status = pslGetDefault("mca_bin_width", (void *)&mcaBinWidth, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting mca_bin_width for detChan %d", detChan);
	pslLogError("pslDoCalibrationEnergy", info_string, status);
	return status;
    }

    statusX = dxp_get_one_dspsymbol(&detChan, "SLOWLEN", &SLOWLEN);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting SLOWLEN from detChan %d", detChan);
	pslLogError("pslDoCalibrationEnergy", info_string, status);
	return status;
    }

    /* Calculate and set the gain */
    status = pslCalculateGain(adcPercentRule, calibEV, preampGain, mcaBinWidth, SLOWLEN, &GAINDAC, gainScale); 
	
    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error calculating gain for detChan %d", detChan);
	pslLogError("pslDoCalibrationEnergy", info_string, status);
	return status;
    }

    statusX = dxp_set_one_dspsymbol(&detChan, "GAINDAC", &GAINDAC);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error setting GAINDAC for detChan %d", detChan);
	pslLogError("pslDoCalibrationEnergy", info_string, status);
	return status;
    }

    status = pslSetDefault("calibration_energy", (void *)&calibEV, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error setting calibration_energy for detChan %d", detChan);
	pslLogError("pslDoCalibrationEnergy", info_string, status);
	return status;
    }

    /* Call routines that depend on the calibration_energy so that
     * there calculations will now be correct.
     */

    /* use the "old" calibration energy since
     * that is what the previous calculation of the trigger threshold
     * is based on.
     */
    statusX = dxp_get_one_dspsymbol(&detChan, "THRESHOLD", &THRESHOLD);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting THRESHOLD from detChan %d", detChan);
	pslLogError("pslDoCalibrationEnergy", info_string, status);
	return status;
    }

    statusX = dxp_get_one_dspsymbol(&detChan, "FASTLEN", &FASTLEN);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting FASTLEN from detChan %d", detChan);
	pslLogError("pslDoCalibrationEnergy", info_string, status);
	return status;
    }

    eVPerADC = (double)(oldCalibEV / ((adcPercentRule / 100.0) * NUM_BITS_ADC));
    threshold = ((double)THRESHOLD * eVPerADC) / (double)FASTLEN;

    status = pslDoTriggerThreshold(detChan, (void *)&threshold, fs, detectorType,
                                   defs, preampGain, gainScale, m, det,
                                   detector_chan);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error updating trigger threshold for detChan %d", detChan);
	pslLogError("pslDoCalibrationEnergy", info_string, status);
	return status;
    }
	
    statusX = dxp_get_one_dspsymbol(&detChan, "SLOWTHRESH", &SLOWTHRESH);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting SLOWTHRESH from detChan %d", detChan);
	pslLogError("pslDoCalibrationEnergy", info_string, status);
	return status;
    }

    slowthresh = ((double)SLOWTHRESH * eVPerADC) / (double)SLOWLEN;

    status = pslDoEnergyThreshold(detChan, (void *)&slowthresh, fs, detectorType,
                                  defs, preampGain, gainScale, m, det,
                                  detector_chan);

    if (status != XIA_SUCCESS) {
	
	sprintf(info_string, "Error updating energy threshold for detChan %d", detChan);
	pslLogError("pslDoCalibrationEnergy", info_string, status);
	return status;
    }

    status = pslDoMCABinWidth(detChan, (void *)&mcaBinWidth, fs, detectorType,
                              defs, preampGain, gainScale, m, det,
                              detector_chan);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error updating MCA bin width for detChan %d", detChan);
	pslLogError("pslDoCalibrationEnergy", info_string, status);
	return status;
    }

    /* Calculate and set the gain (again). It's done twice, for now, to
     * protect against the case where we change all of these other parameters
     * and then find out the gain is out-of-range and, therefore, the whole
     * system is out-of-sync.
     */
    status = pslCalculateGain(adcPercentRule, calibEV, preampGain, mcaBinWidth, SLOWLEN, &GAINDAC, gainScale); 
	
    if (status != XIA_SUCCESS) {
	
	sprintf(info_string, "Error calculating gain for detChan %d", detChan);
	pslLogError("pslDoCalibrationEnergy", info_string, status);
	return status;
    }

    statusX = dxp_set_one_dspsymbol(&detChan, "GAINDAC", &GAINDAC);

    if (statusX != DXP_SUCCESS) {
	
	status = XIA_XERXES;
	sprintf(info_string, "Error setting GAINDAC for detChan %d", detChan);
	pslLogError("pslDoCalibrationEnergy", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine gets the specified acquisition value from either the defaults
 * or from on-board parameters. I anticipate that this routine will be 
 * much less complicated then setAcquisitionValues so I will do everything
 * in a single routine.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetAcquisitionValues(int detChan, char *name,
											   void *value,
											   XiaDefaults *defaults)
{
    int status;

    double tmp = 0.0;

	
	UNUSED(detChan);


    /* Try to get it with pslGetDefault() and
     * if it isn't there after that, then 
     * return an error.
     */
    status = pslGetDefault(name, (void *)&tmp, defaults);

    if (status != XIA_SUCCESS) {

	status = XIA_UNKNOWN_VALUE;
	sprintf(info_string, "Acquisition value %s is unknown", name);
	pslLogError("pslGetAcquisitionValues", info_string, status);
	return status;
    }

    *((double *)value) = tmp;


    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine computes the value of GAINDAC based on the input values. 
 * Also handles scaling due to the "discreteness" of BINFACT1. I'll try and 
 * illustrate the equations as we go along, but the best reference is 
 * probably found external to the program.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslCalculateGain(double adcPercentRule, double calibEV, 
                                        double preampGain, double MCABinWidth, 
                                        parameter_t SLOWLEN, parameter_t *GAINDAC, 
                                        double gainScale)
{
    int status;

    double gSystem;
    double gTotal;
    double gBase = 1.0;
    double gVar;
    double gDB;
    double inputRange = 1000.0;
    double gaindacDB = 40.0;
    double gGAINDAC;
    double eVPerADC;
    double dBINFACT1;
    double BINFACT1;
    double binScale;
    double gaindacBits = 16.0;

    gSystem = pslCalculateSysGain();

    sprintf(info_string, "gSystem = %.3f", gSystem);
    pslLogDebug("pslCalculateGain", info_string);

    gSystem *= gainScale;

    sprintf(info_string, "gSystem (after scale) = %.3f", gSystem);
    pslLogDebug("pslCalculateGain", info_string);  

    gTotal = ((adcPercentRule / 100.0) * inputRange) / ((calibEV / 1000.0) * preampGain);

    sprintf(info_string, "gTotal = %.3f", gTotal);
    pslLogDebug("pslCalculateGain", info_string);

    /* Scale gTotal as a BINFACT1 correction */
    eVPerADC  = (double)(calibEV / ((adcPercentRule / 100.0) * NUM_BITS_ADC));
    dBINFACT1 = (MCABinWidth / eVPerADC) * (double)SLOWLEN * 4.0;
    BINFACT1 = floor(dBINFACT1 + 0.5);

    binScale = BINFACT1 / dBINFACT1;

    gTotal *= binScale;

    sprintf(info_string, "gTotal (after binScale) = %.3f", gTotal);
    pslLogDebug("pslCalculateGain", info_string);

    gVar = gTotal / (gSystem * gBase);

    sprintf(info_string, "gVar = %.3f", gVar);
    pslLogDebug("pslCalculateGain", info_string);

    /* Now we can start converting to GAINDAC */
    gDB = (20.0 * log10(gVar));

    sprintf(info_string, "gDB = %.3f", gDB);
    pslLogDebug("pslCalculateGain", info_string);
	
    if ((gDB < -6.0) ||
	(gDB > 30.0)) {

	status = XIA_GAIN_OOR;
	sprintf(info_string, "Gain value %.3f (in dB) is out-of-range", gDB);
	pslLogError("pslCalculateGain", info_string, status);
	return status;
    }

    gDB += 10.0;

    gGAINDAC = gDB * ((double)(pow(2, gaindacBits) / gaindacDB));

    *GAINDAC = (parameter_t)ROUND(gGAINDAC);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This calculates the system gain due to the analog portion of the system.
 *
 *****************************************************************************/
PSL_STATIC double PSL_API pslCalculateSysGain(void)
{
    double gInputBuffer    = 1.0;
    double gInvertingAmp   = 3240.0 / 499.0;
    double gVoltageDivider = 124.9 / 498.9;
    double gGaindacBuffer  = 1.0;
    double gNyquist        = 422.0 / 613.0;
    double gADCBuffer      = 2.00;
    double gADC            = 250.0 / 350.0;

    double gSystem;

    gSystem = gInputBuffer * gInvertingAmp * gVoltageDivider * 
	gGaindacBuffer * gNyquist * gADCBuffer * gADC;

    return gSystem;
}

/*****************************************************************************
 *
 * This routine sets the clock speed for the X10P board. Eventually, we'd 
 * like to read this info. from the interface. For now, we will set it
 * statically.
 *
 *****************************************************************************/
PSL_STATIC double PSL_API pslGetClockSpeed(int detChan)
{
  int statusX;

  parameter_t SYSMICROSEC = 0;


  statusX = dxp_get_one_dspsymbol(&detChan, "SYSMICROSEC", &SYSMICROSEC);

  if (statusX != DXP_SUCCESS) {
      sprintf(info_string,
			  "Error getting SYSMICROSEC for detChan %d, using default speed",
			  detChan);
      pslLogWarning("pslGetClockSpeed", info_string);
      return DEFAULT_CLOCK_SPEED;
    }

  return (double) SYSMICROSEC;
}


/*****************************************************************************
 *
 * This routine adjusts the percent rule by deltaGain.
 *
 *****************************************************************************/
PSL_STATIC int pslGainChange(int detChan, double deltaGain, 
							 XiaDefaults *defaults,  
							 CurrentFirmware *currentFirmware, 
							 char *detectorType, double gainScale,
							 Detector *detector, int detector_chan, Module *m,
							 int modChan)
{
    int status;

    double oldADCPercentRule;
    double newADCPercentRule;

    FirmwareSet *nullSet = NULL;

	UNUSED(modChan);


    status = pslGetDefault("adc_percent_rule", (void *)&oldADCPercentRule, defaults);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting adc_percent_rule for detChan %d", detChan);
	pslLogError("pslGainChange", info_string, status);
	return status;
    }

    newADCPercentRule = oldADCPercentRule * deltaGain;

    status = pslSetDefault("adc_percent_rule", (void *)&newADCPercentRule, defaults);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error setting adc_percent_rule for detChan %d", detChan);
	pslLogError("pslGainChange", info_string, status);
	return status;
    }

    status = pslSetAcquisitionValues(detChan, "adc_percent_rule", 
									 (void *)&newADCPercentRule, defaults,
									 nullSet, currentFirmware, detectorType,
									 gainScale, detector, detector_chan, m,
									 IGNORE_MODCHAN);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error changing gain for detChan %d. Attempting to reset to previous value", 
		detChan);
	pslLogError("pslGainChange", info_string, status);

	/* Try to reset the gain. If it doesn't work then you aren't really any
	 * worse off then you were before. 
	 */

	/* XXX This error should still be reported! */
	pslSetAcquisitionValues(detChan, "adc_percent_rule", 
							(void *)&oldADCPercentRule, defaults, nullSet,
							currentFirmware, detectorType, gainScale, detector,
							detector_chan, m, IGNORE_MODCHAN);

	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine adjusts the gain via. the preamp gain. As the name suggests, 
 * this is mostly for situations where you are trying to calibrate the gain
 * with a fixed eV/ADC, which should cover 99% of the situations, if not the
 * full 100%.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGainCalibrate(int detChan, Detector *detector,
										int detector_chan, Module *m,
										XiaDefaults *defaults, double deltaGain,
										double gainScale)
{
    int status;
    int statusX;

    double adcPercentRule = 0.0;
    double calibEV = 0.0;
    double mcaBinWidth = 0.0;
    double preampGain = 0.0;

    parameter_t SLOWLEN;
    parameter_t GAINDAC;

	UNUSED(m);
	

    status = pslGetDefault("adc_percent_rule", (void *)&adcPercentRule, defaults);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting adc_percent_rule for detChan %d", detChan);
	pslLogError("pslGainCalibrate", info_string, status);
	return status;
    }

    status = pslGetDefault("calibration_energy", (void *)&calibEV, defaults);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting calibration_energy for detChan %d", detChan);
	pslLogError("pslGainCalibrate", info_string, status);
	return status;
    }

    status = pslGetDefault("mca_bin_width", (void *)&mcaBinWidth, defaults);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting mca_bin_width for detChan %d", detChan);
	pslLogError("pslGainCalibrate", info_string, status);
	return status;
    }

    statusX = dxp_get_one_dspsymbol(&detChan, "SLOWLEN", &SLOWLEN);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting SLOWLEN from detChan %d", detChan);
	pslLogError("pslGainCalibrate", info_string, status);
	return status;
    }

    /* Calculate scaled preamp gain keeping in mind that it is an inverse
     * relationship.
     */
    preampGain = detector->gain[detector_chan];
    preampGain *= 1.0 / deltaGain;
    detector->gain[detector_chan] = preampGain;

    status = pslCalculateGain(adcPercentRule, calibEV, preampGain, mcaBinWidth, SLOWLEN, &GAINDAC, gainScale);

    if (status != XIA_SUCCESS) {
	/* If there is a problem here, then we probably need a way to 
	 * revert back to the old gain...
	 */
	sprintf(info_string, "Error calculating gain for detChan %d", detChan);
	pslLogError("pslGainCalibrate", info_string, status);
	return status;
    }

    statusX = dxp_set_one_dspsymbol(&detChan, "GAINDAC", &GAINDAC);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error setting GAINDAC for detChan %d", detChan);
	pslLogError("pslGainCalibrate", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine is responsible for calling the XerXes version of start run.
 * There may be a problem here involving multiple calls to start a run on a 
 * module since this routine is iteratively called for each channel. May need
 * a way to start runs that takes the "state" of the module into account.
 *
 *****************************************************************************/
PSL_STATIC int pslStartRun(int detChan, unsigned short resume, XiaDefaults *defs,
						   Module *m)
{
    int statusX;
    int status;

    double tmpGate = 0.0;

    unsigned short gate;


	sprintf(info_string, "Starting a run: detChan '%d', resume '%u'", detChan, resume);
	pslLogDebug("pslStartRun", info_string);
  
    status = pslGetDefault("enable_gate", (void *)&tmpGate, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, 
		"Error getting enable_gate information for run start on detChan %d",
		detChan);
	pslLogError("pslStartRun", info_string, status);
	return status;
    }

    gate = (unsigned short)tmpGate;

    statusX = dxp_start_one_run(&detChan, &gate, &resume);

    if (statusX != DXP_SUCCESS) {
	
	status = XIA_XERXES;
	sprintf(info_string, "Error starting a run on detChan %d", detChan);
	pslLogError("pslStartRun", info_string, status);
	return status;
    }

	status = psl__WaitForRunStart(m);

	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error waiting for the run to start on the module "
			  "'%s'", m->alias);
	  pslLogError("pslStartRun", info_string, status);
	  return status;
	}

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine is responsible for calling the XerXes version of stop run.
 * With some hardware, all channels on a given module may be "stopped" 
 * together. Not sure if calling stop multiple times do to the detChan 
 * iteration procedure will cause problems or not.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslStopRun(int detChan, Module *m)
{
    int statusX;
    int status;
    int i;

    /* Since dxp_md_wait() wants a time in seconds */
    float wait = 1.0f / 1000.0f;

    parameter_t BUSY;

	UNUSED(m);


	sprintf(info_string, "Stopping a run: detChan '%d'", detChan);
	pslLogDebug("pslStopRun", info_string);

    statusX = dxp_stop_one_run(&detChan);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error stopping a run on detChan %d", detChan);
	pslLogError("pslStopRun", info_string, status);
	return status;
    }

    /* Allow time for run to end */
    statusX = utils->funcs->dxp_md_wait(&wait);

    /* If run is truly ended, then BUSY should equal 0 */
    for (i = 0; i < 4000; i++) {

	statusX = dxp_get_one_dspsymbol(&detChan, "BUSY", &BUSY);

	sprintf(info_string, "BUSY = %#x", BUSY);
	pslLogDebug("pslStopRun", info_string);

	if (BUSY == 0) {

	    return XIA_SUCCESS;
	}

	statusX = utils->funcs->dxp_md_wait(&wait);
    }

    return XIA_TIMEOUT;
}


/*****************************************************************************
 *
 * This routine retrieves the specified data from the board. In the case of
 * the X10P a run does not need to be stopped in order to retrieve the 
 * specified information.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetRunData(int detChan, char *name, void *value,
									 XiaDefaults *defaults, Module *m)
{
    int status = XIA_SUCCESS;

    UNUSED(defaults);
    UNUSED(m);


    /* This routine just calls other routines as determined by name */
    if (STREQ(name, "mca_length")) {

	status = pslGetMCALength(detChan, value);

    } else if (STREQ(name, "mca")) {

	status = pslGetMCAData(detChan, value);

    } else if (STREQ(name, "livetime")) {

	status = pslGetLivetime(detChan, value);

    } else if (STREQ(name, "runtime")) {

	status = pslGetRuntime(detChan, value);

    } else if (STREQ(name, "input_count_rate")) {

	status = pslGetICR(detChan, value);

    } else if (STREQ(name, "output_count_rate")) {

	status = pslGetOCR(detChan, value);

    } else if (STREQ(name, "events_in_run")) {

	status = pslGetEvents(detChan, value);

    } else if (STREQ(name, "triggers")) {

	status = pslGetTriggers(detChan, value);

    } else if (STREQ(name, "baseline_length")) {

	status = pslGetBaselineLen(detChan, value);

    } else if (STREQ(name, "baseline")) {

	status = pslGetBaseline(detChan, value);

    } else if (STREQ(name, "run_active")) {

	status = pslGetRunActive(detChan, value);

	} else if (STRNEQ(name, "sca_length")) {
	  
	  status = _pslGetSCALen(detChan, defaults, value);
  
	} else if (STRNEQ(name, "sca")) {
	  
	  status = _pslGetSCAData(detChan, defaults, value);

    } else {

	status = XIA_BAD_NAME;
	sprintf(info_string, "%s is either an unsupported run data type or a bad name for detChan %d",
		name, detChan);
	pslLogError("pslGetRunData", info_string, status);
	return status;
    }

    return status;
}


/*****************************************************************************
 *
 * This routine sets value to the length of the MCA spectrum.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetMCALength(int detChan, void *value)
{
    int statusX;
    int status;

    statusX = dxp_nspec(&detChan, (unsigned int *)value);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting spectrum length for detChan %d", detChan);
	pslLogError("pslGetMCALength", info_string, status);
	return status;
    }
	
    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine gets the MCA spectrum through XerXes.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetMCAData(int detChan, void *value)
{
    int statusX;
    int status;

    statusX = dxp_readout_detector_run(&detChan, NULL, NULL, (unsigned long *)value);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting MCA spectrum for detChan %d", detChan);
	pslLogError("pslGetMCAData", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine reads the livetime back from the board.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetLivetime(int detChan, void *value)
{
    int statusX;
    int status;

    double lt  = 0.0;
    double icr = 0.0;
    double ocr = 0.0;

    unsigned long nevents = 0;
    

    statusX = dxp_get_statistics(&detChan, &lt, &icr, &ocr, &nevents);

    if (statusX != DXP_SUCCESS) {
	status = XIA_XERXES;
	pslLogError("pslGetLivetime", "Error getting statistics", status);
	return status;
    }

    *((double *)value) = lt;

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine reads the runtime back from the board.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetRuntime(int detChan, void *value)
{
    int statusX;
    int status;

    /* In seconds */
    double tickTime = 400e-9;

    double dREALTIME;

    statusX = dxp_get_dspsymbol(&detChan, "REALTIME", &dREALTIME);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting REALTIME from detChan %d", detChan);
	pslLogError("pslGetRuntime", info_string, status);
	return status;
    }

    *((double *)value) = dREALTIME * tickTime;

    sprintf(info_string, "REALTIME (32-bits) = %.3f, realtime = %.3f",
	    dREALTIME, *((double *)value));
    pslLogDebug("pslGetLivetime", info_string);

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine gets the Input Count Rate (ICR)
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetICR(int detChan, void *value)
{
    int statusX;
    int status;

    double lt  = 0.0;
    double icr = 0.0;
    double ocr = 0.0;

    unsigned long nevents = 0;
    

    statusX = dxp_get_statistics(&detChan, &lt, &icr, &ocr, &nevents);

    if (statusX != DXP_SUCCESS) {
	status = XIA_XERXES;
	pslLogError("pslGetICR", "Error getting statistics", status);
	return status;
    }

	/* dxp_get_statistics() returns these values in kcps, but the user
	 * is expecting them to be in cps per the API.
	 */
    *((double *)value) = icr * 1000.0;

    return XIA_SUCCESS;
}


/*****************************************************************************
 * 
 * This routine gets the Output Count Rate (OCR)
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetOCR(int detChan, void *value)
{
    int statusX;
    int status;

    double lt  = 0.0;
    double icr = 0.0;
    double ocr = 0.0;

    unsigned long nevents = 0;
    

    statusX = dxp_get_statistics(&detChan, &lt, &icr, &ocr, &nevents);

    if (statusX != DXP_SUCCESS) {
	status = XIA_XERXES;
	pslLogError("pslGetOCR", "Error getting statistics", status);
	return status;
    }

	/* dxp_get_statistics() returns these values in kcps, but the user
	 * is expecting them to be in cps per the API.
	 */
    *((double *)value) = ocr * 1000.0;

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine gets the number of events in the run
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetEvents(int detChan, void *value)
{
    int statusX;
    int status;

    double lt  = 0.0;
    double icr = 0.0;
    double ocr = 0.0;

    unsigned long nevents = 0;
    

    statusX = dxp_get_statistics(&detChan, &lt, &icr, &ocr, &nevents);

    if (statusX != DXP_SUCCESS) {
	status = XIA_XERXES;
	pslLogError("pslGetEvents", "Error getting statistics", status);
	return status;
    }

    *((unsigned long *)value) = nevents;

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine gets the number of triggers (FASTPEAKS) in the run
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetTriggers(int detChan, void *value)
{
    int statusX;
    int status;

    double dFASTPEAKS;

    statusX = dxp_get_dspsymbol(&detChan, "FASTPEAKS", &dFASTPEAKS);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting FASTPEAKS from detChan %d", detChan);
	pslLogError("pslGetTriggers", info_string, status);
	return status;
    }

    *((unsigned long *)value) = (unsigned long) dFASTPEAKS;

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine gets the baseline length as determined by XerXes.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetBaselineLen(int detChan, void *value)
{
    int statusX;
    int status;

    unsigned int baseLen;

    statusX = dxp_nbase(&detChan, &baseLen);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting size of baseline histogram for detChan %d", detChan);
	pslLogError("pslGetBaselineLen", info_string, status);
	return status;
    }

    *((unsigned long *)value) = (unsigned long)baseLen;

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine gets the baseline data from XerXes.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetBaseline(int detChan, void *value)
{
    int statusX;

    unsigned long *baseline = (unsigned long *)value;

	
	ASSERT(value != NULL);


    statusX = dxp_readout_detector_run(&detChan, NULL, baseline, NULL);
 
    if (statusX != DXP_SUCCESS) {
	  sprintf(info_string, "Error reading baseline histogram from detChan %d",
			  detChan);
	  pslLogError("pslGetBaseline", info_string, XIA_XERXES);
	  return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/********** 
 * This routine sets the run active information as retrieved
 * from Xerxes. The raw bitmask (from Xerxes) is actually 
 * returned to the user with the appropriate constants
 * defined in handel.h.
 **********/
PSL_STATIC int PSL_API pslGetRunActive(int detChan, void *value)
{
    int status;
    int statusX;
    int runActiveX;

  
    statusX = dxp_isrunning(&detChan, &runActiveX);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting run active information for detChan %d",
		detChan);
	pslLogError("pslGetRunActive", info_string, status);
	return status;
    }

    *((unsigned long *)value) = (unsigned long)runActiveX;

    return XIA_SUCCESS;
}

  
/*****************************************************************************
 *
 * This routine dispatches calls to the requested special run routine, when
 * that special run is supported by the x10p.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoSpecialRun(int detChan, char *name, double gainScale, 
									   void *info, XiaDefaults *defaults,
									   Detector *detector, int detector_chan)
{
    int status;

    UNUSED(defaults);
    UNUSED(gainScale);
    UNUSED(detector);
    UNUSED(detector_chan);

    if (STREQ(name, "adc_trace")) {

	status = pslDoADCTrace(detChan, info);

    } else if (STREQ(name, "end_special_run")) {
	status = pslEndSpecialRun(detChan);

    } else if (STREQ(name, "baseline_history")) {

	status = pslDoBaseHistory(detChan, info);	

    } else if (STREQ(name, "open_input_relay")) {

	status = pslDoOpenRelay(detChan, info);

    } else if (STREQ(name, "close_input_relay")) {

	status = pslDoCloseRelay(detChan, info);
		
    } else {

	status = XIA_BAD_SPECIAL;
	sprintf(info_string, "%s is not a valid special run type", name);
	pslLogError("pslDoSpecialRun", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine does all of the work necessary to start and stop a special
 * run for the ADC data. A seperate call must be made to read the data out.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoADCTrace(int detChan, void *info)
{
    int statusX;
    int status;
    int infoInfo[3];
    int infoStart[2];
    int i;
    int timeout = 10000;
	
    unsigned int len = sizeof(infoStart) / sizeof(infoStart[0]);

    short task = CT_ADC;

    parameter_t TRACEWAIT;
    parameter_t BUSY;

    double *dInfo = (double *)info;
    double tracewait;
    /* In nanosec */
    double minTracewait = 75.0;
    double clockSpeed;
    double clockTick;
    float waitTime;
    float pollTime;

    statusX = dxp_control_task_info(&detChan, &task, infoInfo);
   
    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting control task info for detChan %d", detChan);
	pslLogError("pslDoADCTrace", info_string, status);
	return status;
    }

    waitTime = (float)((float)infoInfo[1] / 1000.0);
    pollTime = (float)((float)infoInfo[2] / 1000.0);

    infoStart[0] = (int)dInfo[0];
	
    /* Compute TRACEWAIT keeping in mind that the minimum value for the X10P is
     * 100 ns. 
     */
    sprintf(info_string, "dInfo[1] = %f", dInfo[1]);
    pslLogDebug("pslDoADCTrace", info_string);

    tracewait = dInfo[1];

    if (tracewait < minTracewait) {

	tracewait = minTracewait;

	sprintf(info_string, "tracewait of %.3f is less then min. tracewait."
		"Setting tracewait to %.3f.", tracewait, minTracewait);
	pslLogWarning("pslDoADCTrace", info_string);
    }

    clockSpeed = pslGetClockSpeed(detChan);
    /* Convert to sec */
    clockTick = (1.0 / (clockSpeed * 1.0e6)) * 1.0e9;

    sprintf(info_string, "tracewait = %f\nclockTick = %f\nminTracewait = %f", tracewait, clockTick,
	    minTracewait);
    pslLogDebug("pslDoADCTrace", info_string);

    TRACEWAIT = (parameter_t)((tracewait - minTracewait) / clockTick);

    sprintf(info_string, "Setting TRACEWAIT to %u", TRACEWAIT);
    pslLogDebug("pslDoADCTrace", info_string);

    infoStart[1] = (int)TRACEWAIT;

    statusX = dxp_start_control_task(&detChan, &task, &len, infoStart);
 
    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error starting control task for detChan %d", detChan);
	pslLogError("pslDoADCTrace", info_string, status);
	return status;
    }

    /* Wait for the specified amount of time before starting to poll board
     * to see if the special run is finished yet.
     */

    sprintf(info_string, "Preparing to wait: waitTime = %f", waitTime);
    pslLogDebug("pslDoADCTrace", info_string);

    utils->funcs->dxp_md_wait(&waitTime);

    pslLogDebug("pslDoADCTrace", "Finished waiting"); 
    pslLogDebug("pslDoADCTrace", "Preparing to poll board");

    for (i = 0; i < timeout; i++) {

	statusX = dxp_get_one_dspsymbol(&detChan, "BUSY", &BUSY);

	if (statusX != DXP_SUCCESS) {
		
	    status = XIA_XERXES;
	    sprintf(info_string, "Error getting BUSY from detChan %d", detChan);
	    pslLogError("pslDoADCTrace", info_string, status);
	    return status;
	}

	sprintf(info_string, "BUSY = %u", BUSY);
	pslLogDebug("pslDoADCTrce", info_string);

	if (BUSY == 99) {

	    break;
	}

	if (i == (timeout - 1)) {
		
	    status = XIA_TIMEOUT;
	    sprintf(info_string, "Timeout waiting for BUSY to go to 0 on detChan %d", detChan);
	    pslLogError("pslDoADCTrace", info_string, status);
	    return status;
	}

	utils->funcs->dxp_md_wait(&pollTime);
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine parses out the actual data gathering to other routines.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetSpecialRunData(int detChan, char *name, void *value, XiaDefaults *defaults)
{
    int status;

	UNUSED(defaults);


    if (STREQ(name, "adc_trace_length")) {

	status = pslGetADCTraceInfo(detChan, value);

    } else if (STREQ(name, "adc_trace")) {

	status = pslGetADCTrace(detChan, value);

    } else if (STREQ(name, "baseline_history_length")) {

	status = pslGetBaseHistoryLen(detChan, value);
		
    } else if (STREQ(name, "baseline_history")) {

	status = pslGetBaseHistory(detChan, value);
		
    } else {

	status = XIA_BAD_SPECIAL;
	sprintf(info_string, "%s is not a valid special run data type", name);
	pslLogError("pslGetSpecialRunData", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine get the size of the ADC trace and returns it in info.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetADCTraceInfo(int detChan, void *value)
{
    int statusX;
    int status;
    int info[3];

    short task = CT_ADC;

    statusX = dxp_control_task_info(&detChan, &task, info);

    if (statusX != DXP_SUCCESS) {
	
	status = XIA_XERXES;
	sprintf(info_string, "Error getting control task info for detChan %d", detChan);
	pslLogError("pslGetADCTraceInfo", info_string, status);
	return status;
    }

    *((unsigned long *)value) = (unsigned long)info[0];

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine gets the ADC trace. It ASSUMES that the user has allocated
 * the proper amount of memory for value, preferably by making a call to 
 * getSpecialRunData() w/ "adc_trace_info".
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetADCTrace(int detChan, void *value)
{
    int statusX;
    int status;

    short task = CT_ADC;


    statusX = dxp_get_control_task_data(&detChan, &task, value);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting control task data for detChan %d", detChan);
	pslLogError("pslGetADCTrace", info_string, status);
	return status;
    }

	statusX = dxp_stop_control_task(&detChan);

	if (statusX != DXP_SUCCESS) {
	  sprintf(info_string, "Error stopping control task '%#x' for detChan %d",
			  task, detChan);
	  pslLogError("pslGetADCTrace", info_string, XIA_XERXES);
	  return XIA_XERXES;
	}

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine "starts" a baseline history run. What it actually does is
 * disable the "updating" of the baseline history buffer.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoBaseHistory(int detChan, void *info)
{
    int statusX;
    int status;
    int infoInfo[3];

    unsigned int len = 1;

    short task = CT_DXP2X_BASELINE_HIST;

    float waitTime;


    statusX = dxp_control_task_info(&detChan, &task, infoInfo);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting control task info for detChan %d", detChan);
	pslLogError("pslDoBaseHistory", info_string, status);
	return status;
    }

    waitTime = (float)((float)infoInfo[1] / 1000.0);

    statusX = dxp_start_control_task(&detChan, &task, &len, (int *)info);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error starting control task on detChan %d", detChan);
	pslLogError("pslDoBaseHistory", info_string, status);
	return status;
    }

    /* Unlike most runs, we don't need to stop this one here since 
     * stopping it will restart the filling of the history buffer.
     * Instead, we'll just wait the specified time and then return.
     */
    utils->funcs->dxp_md_wait(&waitTime);

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine gets the length of the baseline history buffer.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetBaseHistoryLen(int detChan, void *value)
{
    int statusX;
    int status;
    int info[3];

    short task = CT_DXP2X_BASELINE_HIST;

    statusX = dxp_control_task_info(&detChan, &task, info);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting control task info for detChan %d", detChan);
	pslLogError("pslGetBaseHistoryLen", info_string, status);
	return status;
    }

    *((unsigned long *)value) = (unsigned long)info[0];

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine gets the baseline history data from the frozen baseline
 * history buffer. As usual, it assumes that the proper amount of memory has
 * been allocated for the returned data array.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetBaseHistory(int detChan, void *value)
{
    int statusX;
    int status;

    short task = CT_DXP2X_BASELINE_HIST;


    statusX = dxp_get_control_task_data(&detChan, &task, value);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting control task data for detChan %d", detChan);
	pslLogError("pslGetBaseHistory", info_string, status);
	return status;
    }

    statusX = dxp_stop_control_task(&detChan);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error stopping control task on detChan %d", detChan);
	pslLogError("pslGetBaseHistory", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine is used to open the relays on the x10p box.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoOpenRelay(int detChan, void *info)
{
    int statusX;
    int status;
    int infoInfo[3];

    float waitTime;

    unsigned int len = 1;

    short task = CT_DXP2X_OPEN_INPUT_RELAY;


    statusX = dxp_control_task_info(&detChan, &task, infoInfo);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting control task info from detChan %d", detChan);
	pslLogError("pslDoOpenRelay", info_string, status);
	return status;
    }

    waitTime = (float)((float)infoInfo[1] / 1000.0);

    statusX = dxp_start_control_task(&detChan, &task, &len, (int *)info);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error starting control task on detChan %d", detChan);
	pslLogError("pslDoOpenRelay", info_string, status);
	return status;
    }

    utils->funcs->dxp_md_wait(&waitTime);

    statusX = dxp_stop_control_task(&detChan);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error stopping control task on detChan %d", detChan);
	pslLogError("pslDoOpenRelay", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine is used to close the relays on the board.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoCloseRelay(int detChan, void *info)
{
    int statusX;
    int status;
    int infoInfo[3];

    float waitTime;

    unsigned int len = 1;

    short task = CT_DXP2X_CLOSE_INPUT_RELAY;


    statusX = dxp_control_task_info(&detChan, &task, infoInfo);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting control task info for detChan %d", detChan);
	pslLogError("pslDoCloseRelay", info_string, status);
	return status;
    }

    waitTime = (float)((float)infoInfo[1] / 1000.0);

    statusX = dxp_start_control_task(&detChan, &task, &len, (int *)info);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error starting control task on detChan %d", detChan);
	pslLogError("pslDoCloseRelay", info_string, status);
	return status;
    }

    utils->funcs->dxp_md_wait(&waitTime);

    statusX = dxp_stop_control_task(&detChan);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error stopping control task on detChan %d", detChan);
	pslLogError("pslDoCloseRelay", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine sets the proper detector type specific information (either 
 * RESETINT or TAURC) for detChan.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslSetDetectorTypeValue(int detChan, Detector *detector, 
											   int detectorChannel, XiaDefaults *defaults)
{
    int statusX;
    int status;

    parameter_t value;

    double resetint;
    double taurc;
    double clockSpeed;

    char parameter[MAXITEM_LEN];

	UNUSED(defaults);

    switch(detector->type)
    {
      case XIA_DET_RESET:
	resetint = 4.0 * detector->typeValue[detectorChannel];
	value    = (parameter_t)ROUND(resetint);
		
	strcpy(parameter, "RESETINT");
	break;

      case XIA_DET_RCFEED:
	clockSpeed = pslGetClockSpeed(detChan);
	taurc      = clockSpeed * detector->typeValue[detectorChannel];
	value      = (parameter_t)ROUND(taurc);
 	   
	strcpy(parameter, "TAURC");
	break;

      default:
      case XIA_DET_UNKNOWN:
	return XIA_UNKNOWN;
	break;
    }

    statusX = dxp_set_one_dspsymbol(&detChan, parameter, &value);
 
    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error setting %s for detChan %d", parameter, detChan);
	pslLogError("pslSetDetectorTypeValue", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}

		
	

/******************************************************************************
 *
 * This routine sets the polarity for a detChan. 
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslSetPolarity(int detChan, Detector *detector, 
									  int detectorChannel, XiaDefaults *defaults,
									  Module *m)
{
    int status;

	parameter_t POLARITY;


	ASSERT(detector != NULL);
	ASSERT(defaults != NULL);


	POLARITY = (parameter_t) detector->polarity[detectorChannel];

    status = pslSetParameter(detChan, "POLARITY", POLARITY);

    if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error setting POLARITY for detChan %d", detChan);
	  pslLogError("pslSetPolarity", info_string, status);
	  return status;
    }

	status = pslQuickRun(detChan, defaults, m);

	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error doing Quick Run on detChan %d", detChan);
	  pslLogError("pslSetPolarity", info_string, status);
	  return status;
	}

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine creates a default w/ information specific to the x10p in it.
 * 
 *****************************************************************************/
PSL_STATIC int pslGetDefaultAlias(char *alias, char **names, double *values)
{
    int i;


    ASSERT(alias != NULL);
    ASSERT(names != NULL);
    ASSERT(values != NULL);


    for (i = 0; i < N_ELEMS(ACQ_VALUES); i++) {
      if (ACQ_VALUES[i].isDefault) {
        strcpy(names[i], ACQ_VALUES[i].name);
        values[i] = ACQ_VALUES[i].def;
      }
    }

    strcpy(alias, "defaults_dxp4c2x");

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine retrieves the value of the DSP parameter name from detChan.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetParameter(int detChan, const char *name, unsigned short *value)
{
    int statusX;
    int status;

    char *noconstName = NULL;


    noconstName = (char *)utils->funcs->dxp_md_alloc((strlen(name) + 1) * sizeof(char));
  
    if (noconstName == NULL) {

	status = XIA_NOMEM;
	pslLogError("pslGetParameter", "Out-of-memory trying to create tmp. string", status);
	return status;
    }

    strcpy(noconstName, name);

    statusX = dxp_get_one_dspsymbol(&detChan, noconstName, value);

    if (statusX != DXP_SUCCESS) { 

	utils->funcs->dxp_md_free((void *)noconstName);
	noconstName = NULL;

	status = XIA_XERXES;
	sprintf(info_string, "Error trying to get DSP parameter %s from detChan %d", name, detChan);
	pslLogError("pslGetParameter", info_string, status);
	return status;
    }

    utils->funcs->dxp_md_free((void *)noconstName);
    noconstName = NULL;

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine sets the value of the DSP parameter name for detChan.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslSetParameter(int detChan, const char *name, unsigned short value)
{
    int statusX;
    int status;

    char *noconstName = NULL;


    noconstName = (char *)utils->funcs->dxp_md_alloc((strlen(name) + 1) * sizeof(char));
  
    if (noconstName == NULL) {

	status = XIA_NOMEM;
	pslLogError("pslSetParameter", "Out-of-memory trying to create tmp. string", status);
	return status;
    }

    strcpy(noconstName, name);

    statusX = dxp_set_one_dspsymbol(&detChan, noconstName, &value);

    if (statusX != DXP_SUCCESS) { 

	utils->funcs->dxp_md_free((void *)noconstName);
	noconstName = NULL;

	status = XIA_XERXES;
	sprintf(info_string, "Error trying to set DSP parameter %s for detChan %d", name, detChan);
	pslLogError("pslSetParameter", info_string, status);
	return status;
    }

    utils->funcs->dxp_md_free((void *)noconstName);
    noconstName = NULL;

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * The whole point of this routine is to make the PSL layer start things
 * off by calling pslSetAcquistionValues() for the acquisition values it 
 * thinks are appropriate for the X10P.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslUserSetup(int detChan, XiaDefaults *defaults, 
                                    FirmwareSet *firmwareSet, 
                                    CurrentFirmware *currentFirmware, 
                                    char *detectorType, double gainScale,
									Detector *detector, int detector_chan,
									Module *m, int modChan)
{
    int status;
    int i;

    double peakingTime = 0.0;

    XiaDaqEntry *daqEntry = NULL;

    
    /* Do the values that require synchronization with the detector
     * structure first.
     */
    for (i = 0; i < N_ELEMS(ACQ_VALUES); i++) {
      if (ACQ_VALUES[i].isSynch) {
        status = ACQ_VALUES[i].synchFN(detChan, detector_chan, m, detector,
                                       defaults);

        if (status != XIA_SUCCESS) {
          sprintf(info_string, "Error synchronizing '%s' for detChan %d",
                  ACQ_VALUES[i].name, detChan);
          pslLogError("pslUserSetup", info_string, status);
          return status;
        }
      }
    }


    for (i = 0; i < N_ELEMS(ACQ_VALUES); i++) {
      daqEntry = defaults->entry;

      while (daqEntry) {
        if (STREQ(daqEntry->name, ACQ_VALUES[i].name)) {
          status = pslSetAcquisitionValues(detChan, daqEntry->name,
                                           (void *)&(daqEntry->data),
                                           defaults, firmwareSet,
                                           currentFirmware, detectorType,
                                           gainScale, detector, detector_chan,
                                           m, modChan);

          if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error initializing '%s' to %0.3f for detChan %d",
                    daqEntry->name, daqEntry->data, detChan);
            pslLogError("pslUserSetup", info_string, status);
            return status;
          }
        }

        daqEntry = daqEntry->next;
      }
    }

    pslLogDebug("pslUserSetup", "Preparing to call pslUpdateFilter()");

    /* Init filter params here */
    status = pslGetDefault("peaking_time", (void *)&peakingTime, defaults);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting peaking_time from detChan %d", detChan);
	pslLogError("pslUserSetup", info_string, status);
	return status;
    }

    status = pslUpdateFilter(detChan, peakingTime, defaults, firmwareSet);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error doing initial update of filter parameters for detChan %d", detChan);
	pslLogError("pslUserSetup", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/****
 * This routine updates the filter parameters for the specified detChan using
 * information from the defaults and the firmware.
 */
PSL_STATIC int PSL_API pslUpdateFilter(int detChan, double peakingTime, XiaDefaults *defaults, FirmwareSet *firmwareSet)
{
    int status;
    int statusX;

    unsigned short numFilter;
    unsigned short i;
    unsigned short ptrr = 0;

    double dSLOWLEN = 0.0;
    double dSLOWGAP = 0.0;
    double gapTime  = 0.0;
    double tmpPIOff = 0.0;
    double tmpPSOff = 0.0;

    char piStr[100];
    char psStr[100];

    Firmware *current = NULL;

    parameter_t SLOWLEN       = 0;
    parameter_t SLOWGAP       = 0;
    parameter_t PEAKINT       = 0;
    parameter_t PEAKSAM       = 0;
    parameter_t newDecimation = 0;

    parameter_t *filterInfo = NULL;


    CLOCK_SPEED = pslGetClockSpeed(detChan);

  

    if (firmwareSet->filename != NULL) {

	status = xiaFddGetNumFilter(firmwareSet->filename, peakingTime, firmwareSet->numKeywords,
				    firmwareSet->keywords, &numFilter);
	
	if (status != XIA_SUCCESS) {
	  
	    pslLogError("pslUpdateFilter", "Error getting number of filter params", status);
	    return status;
	}

	filterInfo = (parameter_t *)utils->funcs->dxp_md_alloc(numFilter * sizeof(parameter_t));

	status = xiaFddGetFilterInfo(firmwareSet->filename, peakingTime, firmwareSet->numKeywords,
				     firmwareSet->keywords, filterInfo);

	if (status != XIA_SUCCESS) {

	    pslLogError("pslUpdateFilter", "Error getting filter information from FDD", status);
	    return status;
	}

	sprintf(info_string, "PI Offset = %u, PS Offset = %u", filterInfo[0], filterInfo[1]);
	pslLogDebug("pslUpdateFilter", info_string);

	/* Override the values loaded in from the FDD with values from the
	 * defaults. Remember that when the user is using an FDD file they
	 * don't need the _ptrr{n} specifier on their default
	 * name. These aren't required so there is no reason to check
	 * the status code.
	 */
	
	status = pslGetDefault("peakint_offset", (void *)&tmpPIOff, defaults);

	if (status == XIA_SUCCESS) { filterInfo[0] = (parameter_t)tmpPIOff; }
	
	status = pslGetDefault("peaksam_offset", (void *)&tmpPSOff, defaults);
	
	if (status == XIA_SUCCESS) { filterInfo[1] = (parameter_t)tmpPSOff; }


	sprintf(info_string, "PI Offset = %u, PS Offset = %u", filterInfo[0], filterInfo[1]);
	pslLogDebug("pslUpdateFilter", info_string);

    } else {

	/* Fill the filter information in here using the FirmwareSet */
	current = firmwareSet->firmware;
	
	while (current != NULL) {

	    if ((peakingTime >= current->min_ptime) &&
		(peakingTime <= current->max_ptime))
	    {
		filterInfo = (parameter_t *)utils->funcs->dxp_md_alloc(current->numFilter *
								     sizeof(parameter_t));
		for (i = 0; i < current->numFilter; i++) {

		    filterInfo[i] = current->filterInfo[i];
		}

		ptrr = current->ptrr;
	    }
	  
	    current = getListNext(current);

	}

	if (filterInfo == NULL) {

	    status = XIA_BAD_FILTER;
	    pslLogError("pslUpdateFilter", "Error loading filter information", status);
	    return status;
	}

	sprintf(piStr, "peakint_offset_ptrr%u", ptrr);
	sprintf(psStr, "peaksam_offset_ptrr%u", ptrr);

	/* In this case we just ignore the
	 * error values, since the fact that
	 * the acq. value is missing just means
	 * that we don't want to use it.
	 */
	status = pslGetDefault(piStr, (void *)&tmpPIOff, defaults);

	if (status == XIA_SUCCESS) { filterInfo[0] = (parameter_t)tmpPIOff; }

	status = pslGetDefault(psStr, (void *)&tmpPSOff, defaults);

	if (status == XIA_SUCCESS) { filterInfo[1] = (parameter_t)tmpPSOff; }

    }

    statusX = dxp_get_one_dspsymbol(&detChan, "DECIMATION", &newDecimation);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting DECIMATION from detChan %d", detChan);
	pslLogError("pslUpdateFilter", info_string, status);
	return status;
    }

    /* Calculate SLOWLEN from board parameters */
	sprintf(info_string, "CLOCK_SPEED = %.3f, peakingTime = %.3f, newDecimation = %u",
			CLOCK_SPEED, peakingTime, newDecimation);
	pslLogDebug("pslUpdateFilter", info_string);

    dSLOWLEN = (double)(peakingTime /((1.0 / CLOCK_SPEED) * pow(2, (double)newDecimation)));
    SLOWLEN  = (parameter_t)ROUND(dSLOWLEN);

    sprintf(info_string, "SLOWLEN = %u", SLOWLEN);
    pslLogDebug("pslUpdateFilter", info_string);

    if ((SLOWLEN > 28) ||
	(SLOWLEN < 2)) {
	
	status = XIA_SLOWLEN_OOR;
	sprintf(info_string, "Calculated value of SLOWLEN (%u) for detChan %d is out-of-range", 
		SLOWLEN, detChan);
	pslLogError("pslUpdateFilter", info_string, status);
	return status;
    }

    /* Calculate SLOWGAP from gap_time and do sanity check */
    status = pslGetDefault("gap_time", (void *)&gapTime, defaults);
  
    sprintf(info_string, "gap_time for detChan %d = %.3f", detChan, gapTime);
    pslLogDebug("pslUpdateFilter", info_string);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting gap_time from detChan %d", detChan);
	pslLogError("pslUpdateFilter", info_string, status);
	return status;
    }

    dSLOWGAP = (double)(gapTime / ((1.0 / CLOCK_SPEED) * pow(2, (double)newDecimation)));
    /* Always round SLOWGAP up...don't use std. round */
    SLOWGAP = (parameter_t)ceil(dSLOWGAP);

    sprintf(info_string, "SLOWGAP = %u", SLOWGAP);
    pslLogDebug("pslUpdateFilter", info_string);

    if (SLOWGAP > 29) {

	status = XIA_SLOWGAP_OOR;
	sprintf(info_string, "Calculated value of SLOWGAP (%u) for detChan %d is out-of-range", 
		SLOWGAP, detChan);
	pslLogError("pslUpdateFilter", info_string, status);
	return status;
    }

    if (SLOWGAP < 3) {

	/* This isn't an error...the SLOWGAP just
	 * can't be smaller then 3 which is fine
	 * at decimations > 0.
	 */
	SLOWGAP = 3;
	pslLogInfo("pslUpdateFilter", "Calculated SLOWGAP is < 3. Setting SLOWGAP = 3");
    }

    /* Check limit on total length of slow filter */
    if ((SLOWLEN + SLOWGAP) > 31) {

	status = XIA_SLOWFILTER_OOR;
	sprintf(info_string, "Calculated length of slow filter (%u) exceeds 31", 
		(unsigned short)(SLOWLEN + SLOWGAP));
	pslLogError("pslUpdateFilter", info_string, status);
	return status;
    }


    /* The X10P PSL interprets the filterInfo as follows:
     * filterInfo[0] = PEAKINT offset
     * filterInfo[1] = PEAKSAM offset
     */

    sprintf(info_string, "PI offset = %u, PS offset = %u", filterInfo[0], filterInfo[1]);
    pslLogDebug("pslUpdateFilter", info_string);

    statusX = dxp_set_one_dspsymbol(&detChan, "SLOWLEN", &SLOWLEN);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error setting SLOWLEN for detChan %d", detChan);
	pslLogError("pslUpdateFilter", info_string, status);
	return status;
    }
  
    statusX = dxp_set_one_dspsymbol(&detChan, "SLOWGAP", &SLOWGAP);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error setting SLOWGAP for detChan %d", detChan);
	pslLogError("pslUpdateFilter", info_string, status);
	return status;
    }

    PEAKINT = (parameter_t)(SLOWLEN + SLOWGAP + filterInfo[0]);

    statusX = dxp_set_one_dspsymbol(&detChan, "PEAKINT", &PEAKINT);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error setting PEAKINT for detChan %d", detChan);
	pslLogError("pslUpdateFilter", info_string, status);
	return status;
    }

    PEAKSAM = (parameter_t)(PEAKINT - filterInfo[1]);

    statusX = dxp_set_one_dspsymbol(&detChan, "PEAKSAM", &PEAKSAM);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error setting PEAKSAM for detChan %d", detChan);
	pslLogError("pslUpdateFilter", info_string, status);
	return status;
    }

    statusX = dxp_upload_dspparams(&detChan);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error uploading DSP parameters to internal memory for detChan %d", 
		detChan);
	pslLogError("pslUpdateFilter", info_string, status);
	return status;
    }
  
    return XIA_SUCCESS;
}


/**
 * This routine updates the specified filter information in the defaults
 * and then calls the update filter routine so that all of the
 * filter parameters will be brought up in sync.
 */
PSL_STATIC int PSL_API pslDoFilter(int detChan, char *name, void *value, XiaDefaults *defaults, FirmwareSet *firmwareSet)
{
    int status;

    double pt = 0.0;


    status = pslSetDefault(name, value, defaults);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error setting %s for detChan %d", name, detChan);
	pslLogError("pslDoFilter", info_string, status);
	return status;
    }

    status = pslGetDefault("peaking_time", (void *)&pt, defaults);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting peaking_time for detChan %d", detChan);
	pslLogError("pslDoFilter", info_string, status);
	return status;
    }

    pslLogDebug("pslDoFilter", "Preparing to call pslUpdateFilter()");

    status = pslUpdateFilter(detChan, pt, defaults, firmwareSet);

    if (status != XIA_SUCCESS) {

	pslLogError("pslDoFilter", "Error updating filter information", status);
	return status;
    }

    return XIA_SUCCESS;
}


/**
 * This routine checks to see if the specified string is
 * all uppercase or not.
 */
PSL_STATIC boolean_t PSL_API pslIsUpperCase(char *name)
{
    int len;
    int i;


    len = strlen(name);

    for (i = 0; i < len; i++) {

	if (!isupper(name[i]) &&
	    !isdigit(name[i])) {

	    return FALSE_;
	}
    }

    return TRUE_;
}


/**
 * This routine updates the value of the parameter_t in the 
 * defaults and then writes it to the board.
 */
PSL_STATIC int PSL_API pslDoParam(int detChan, char *name, void *value, XiaDefaults *defaults)
{
    int status;
    int statusX;
  
	unsigned short param = 0;

    double dTmp = 0.0;

	
	ASSERT(name     != NULL);
	ASSERT(value    != NULL);
	ASSERT(defaults != NULL);


    dTmp = *((double *)value);

    status = pslSetDefault(name, (void *)&dTmp, defaults);

    if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error setting %s for detChan %d", name, detChan);
	  pslLogError("pslDoParam", info_string, status);
	  return status;
    }

	param = (unsigned short)dTmp;

    statusX = dxp_set_one_dspsymbol(&detChan, name, &param);

    if (statusX != DXP_SUCCESS) {
	  sprintf(info_string, "Xerxes reported an error trying to set "
			  "%s to %u", name, param);
	  pslLogError("pslDoParam", info_string, XIA_XERXES);
	  return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/**
 * Checks to see if the specified name is on the list of 
 * required acquisition values for the Saturn.
 */
PSL_STATIC boolean_t PSL_API pslCanRemoveName(char *name)
{
    char *names[] = { 
	"peaking_time",
	"trigger_threshold",
	"mca_bin_width",
	"number_mca_channels",
	"mca_low_limit",
	"energy_threshold",
	"adc_percent_rule",
	"calibration_energy",
	"gap_time",
	"trigger_peaking_time",
	"trigger_gap_time",
	"enable_gate"
    };

    int numNames = (int)(sizeof(names) / sizeof(names[0]));
    int i;


    for (i = 0; i < numNames; i++) {

	if (STREQ(names[i], name)) {

	    return FALSE_;
	}
    }

    return TRUE_;
}


/**
 * Sets the gap time for the slow filter which,
 * in turn sets the SLOWGAP.
 */
ACQUISITION_VALUE(GapTime)
{
    int statusX;
    int status;

    double peakingTime;
 
    parameter_t newDecimation = 0;
    parameter_t SLOWGAP       = 0;

    UNUSED(detector_chan);
    UNUSED(det);
    UNUSED(m);
    UNUSED(gainScale);
    UNUSED(preampGain);
    UNUSED(detectorType);


    CLOCK_SPEED = pslGetClockSpeed(detChan);

  
    /* Get decimation from the board */
    statusX = dxp_get_one_dspsymbol(&detChan, "DECIMATION", &newDecimation);
  
    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting DECIMATION for detChan %d", detChan);
	pslLogError("pslDoGapTime", info_string, status);
	return status;
    }
  
    sprintf(info_string, "gap_time = %.3f", *((double *)value));
    pslLogDebug("pslDoGapTime", info_string);

    status = pslSetDefault("gap_time", value, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error setting gap_time for detChan %d", detChan);
	pslLogError("pslDoGapTime", info_string, status);
	return status;
    }

    status = pslGetDefault("peaking_time", (void *)&peakingTime, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting peaking_time from detChan %d", detChan);
	pslLogError("pslDoGapTime", info_string, status);
	return status;
    }

    pslLogDebug("pslDoGapTime", "Preparing to call pslUpdateFilter()");

    /* Our dirty secret is that SLOWGAP is really changed in 
     * pslUpdateFilter() since other filter params depend on
     * it as well.
     */
    status = pslUpdateFilter(detChan, peakingTime, defs, fs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error updating filter information for detChan %d", detChan);
	pslLogError("pslDoGapTime", info_string, status);
	return status;
    }

    /* Re-calculate the new gap time to report back
     * to the user.
     */
    statusX = dxp_get_one_dspsymbol(&detChan, "SLOWGAP", &SLOWGAP);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting SLOWGAP from detChan %d", detChan);
	pslLogError("pslDoGapTime", info_string, status);
	return status;
    }

    sprintf(info_string, "SLOWGAP = %u", SLOWGAP);
    pslLogDebug("pslDoGapTime", info_string);

    *((double *)value) = (double)((double)SLOWGAP * (1 / CLOCK_SPEED) * 
				  pow(2, (double)newDecimation));

    sprintf(info_string, "New (actual) gap_time for detChan %d is %.3f microseconds", 
	    detChan, *((double *)value));
    pslLogDebug("pslDoGapTime", info_string);
 

    return XIA_SUCCESS;
}


/****
 * This routine translates the fast filter peaking time
 * to FASTLEN.
 */
ACQUISITION_VALUE(TriggerPeakingTime)
{
    int status;

    UNUSED(detector_chan);
    UNUSED(det);
    UNUSED(m);
    UNUSED(gainScale);
    UNUSED(preampGain);
    UNUSED(detectorType);
    UNUSED(fs);


    status = pslSetDefault("trigger_peaking_time", value, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error setting trigger_peaking_time for detChan %d", detChan);
	pslLogError("pslDoTriggerPeakingTime", info_string, status);
	return status;
    }

    status = pslUpdateTriggerFilter(detChan, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error updating trigger filter for detChan %d", detChan);
	pslLogError("pslDoTriggerPeakingTime", info_string, status);
	return status;
    }
 
    return XIA_SUCCESS;
}


/** @brief Sets the trigger gap time in microseconds.
 *
 */
ACQUISITION_VALUE(TriggerGapTime)
{
    int status;

    UNUSED(detector_chan);
    UNUSED(det);
    UNUSED(m);
    UNUSED(gainScale);
    UNUSED(preampGain);
    UNUSED(detectorType);
    UNUSED(fs);

  
    status = pslSetDefault("trigger_gap_time", value, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error setting trigger_gap_time for detChan %d", detChan);
	pslLogError("pslDoTriggerGapTime", info_string, status);
	return status;
    }

    status = pslUpdateTriggerFilter(detChan, defs);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error updating trigger filter for detChan %d", detChan);
	pslLogError("pslDoTriggerGapTime", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


PSL_STATIC int PSL_API pslUpdateTriggerFilter(int detChan, XiaDefaults *defaults)
{
    int status;
    int statusX;

    double triggerPT  = 0.0;
    double triggerGap = 0.0;

    parameter_t FASTLEN = 0;
    parameter_t FASTGAP = 0;

    CLOCK_SPEED = pslGetClockSpeed(detChan);


    status = pslGetDefault("trigger_peaking_time", (void*)&triggerPT, defaults);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting trigger_peaking_time for detChan %d", detChan);
	pslLogError("pslUpdateTriggerFilter", info_string, status);
	return status;
    }

    status = pslGetDefault("trigger_gap_time", (void *)&triggerGap, defaults);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error getting trigger_gap_time for detChan %d", detChan);
	pslLogError("pslUpdateTriggerFilter", info_string, status);
	return status;
    }

    /* Enforce the following hardware limits for
     * optimal fast filter performance:
     * 2 < FASTLEN < 28 
     * 3 < FASTGAP < 29
     * FASTLEN + FASTGAP < 31
     */
    FASTLEN = (parameter_t)ROUND((triggerPT / (1.0 / CLOCK_SPEED)));
  
    /* This is an error condition for the X10P */
    if ((FASTLEN < 2) ||
	(FASTLEN > 28)) {

	status = XIA_FASTLEN_OOR;
	sprintf(info_string, "Calculated value of FASTLEN (%u) for detChan %d is out-of-range",
		FASTLEN, detChan);
	pslLogError("pslUpdateTriggerFilter", info_string, status);
	return status;
    }

    FASTGAP = (parameter_t)ceil((triggerGap / (1.0 / CLOCK_SPEED)));

    if (FASTGAP > 29) {

	status = XIA_FASTGAP_OOR;
	sprintf(info_string, "Calculated value of FASTGAP (%u) for detChan %d is out-of-range",
		FASTGAP, detChan);
	pslLogError("pslUpdateTriggerFilter", info_string, status);
	return status;
    }
  
    if ((FASTLEN + FASTGAP) > 31) {

	status = XIA_FASTFILTER_OOR;
	sprintf(info_string, "Calculated length of slow filter (%u) exceeds 31", 
		(parameter_t)FASTLEN + FASTGAP);
	pslLogError("pslUpdateTriggerFilter", info_string, status);
	return status;
    }

    statusX = dxp_set_one_dspsymbol(&detChan, "FASTLEN", &FASTLEN);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error setting FASTLEN for detChan %d", detChan);
	pslLogError("pslUpdateTriggerFilter", info_string, status);
	return status;
    }

    statusX = dxp_set_one_dspsymbol(&detChan, "FASTGAP", &FASTGAP);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error setting FASTGAP for detChan %d", detChan);
	pslLogError("pslUpdateTriggerFilter", info_string, status);
	return status;
    }

    statusX = dxp_upload_dspparams(&detChan);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error uploading DSP params for detChan %d", detChan);
	pslLogError("pslUpdateTriggerFilter", info_string, status);
	return status;
    }

    /* Re-calculate peaking time and gap time due to
     * the fact that FASTLEN and FASTGAP are rounded.
     */
    triggerPT  = (double)((double)FASTLEN * (1.0 / CLOCK_SPEED));
    triggerGap = (double)((double)FASTGAP * (1.0 / CLOCK_SPEED));

    status = pslSetDefault("trigger_peaking_time", (void *)&triggerPT, defaults);
  
    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error setting trigger_peaking_time for detChan %d", detChan);
	pslLogError("pslUpdateTriggerFilter", info_string, status);
	return status;
    }

    status = pslSetDefault("trigger_gap_time", (void *)&triggerGap, defaults);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error setting trigger_gap_time for detChan %d", detChan);
	pslLogError("pslUpdateTriggerFilter", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}

PSL_STATIC unsigned int PSL_API pslGetNumDefaults(void)
{
  int i;

  unsigned int n = 0;


  for (i = 0; i < N_ELEMS(ACQ_VALUES); i++) {
    if (ACQ_VALUES[i].isDefault) {
      n++;
    }
  }

  return n;
} 


/**********
 * This routine gets the number of DSP parameters
 * for the specified detChan.
 **********/
PSL_STATIC int PSL_API pslGetNumParams(int detChan, unsigned short *numParams)
{
    int status;
    int statusX;

  
    statusX = dxp_max_symbols(&detChan, numParams);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting number of DSP params for detChan %d", 
		detChan);
	pslLogError("pslGetNumParams", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/**********
 * This routine returns the parameter data
 * requested. Assumes that the proper 
 * amount of memory has been allocated for
 * value.
 **********/
PSL_STATIC int PSL_API pslGetParamData(int detChan, char *name, void *value)
{
    int status;
    int statusX;

    unsigned short numSymbols = 0;

    unsigned short *tmp1 = NULL;
    unsigned short *tmp2 = NULL;


    statusX = dxp_max_symbols(&detChan, &numSymbols);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting number of DSP symbols for detChan %d",
		detChan);
	pslLogError("pslGetParamData", info_string, status);
	return status;
    }


    /* Allocate these arrays in case we need them for
     * dxp_symbolname_limits().
     */
    tmp1 = (unsigned short *)malloc(numSymbols * sizeof(unsigned short));
    tmp2 = (unsigned short *)malloc(numSymbols * sizeof(unsigned short));

    if ((tmp1 == NULL) || (tmp2 == NULL)) {

	status = XIA_NOMEM;
	pslLogError("pslGetParaData", "Out-of-memory allocating tmp. arrays", status);
	return status;
    }

    if (STREQ(name, "names")) {

	statusX = dxp_symbolname_list(&detChan, (char **)value);

    } else if (STREQ(name, "values")) {

	statusX = dxp_readout_detector_run(&detChan, (unsigned short *)value, NULL, NULL);
	
    } else if (STREQ(name, "access")) {

	statusX = dxp_symbolname_limits(&detChan, (unsigned short *)value, tmp1, tmp2);

    } else if (STREQ(name, "lower_bounds")) {

	statusX = dxp_symbolname_limits(&detChan, tmp1, (unsigned short *)value, tmp2);

    } else if (STREQ(name, "upper_bounds")) {

	statusX = dxp_symbolname_limits(&detChan, tmp1, tmp2, (unsigned short *)value);

    } else {

	free((void *)tmp1);
	free((void *)tmp2);

	status = XIA_BAD_NAME;
	sprintf(info_string, "%s is not a valid name argument", name);
	pslLogError("pslGetParamData", info_string, status);
	return status;
    }

    free((void *)tmp1);
    free((void *)tmp2);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting DSP parameter data (%s) for detChan %d",
		name, detChan);
	pslLogError("pslGetParamData", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}
  

/**********
 * This routine is mainly a wrapper around dxp_symbolname_by_index()
 * since VB can't pass a string array into a DLL and, therefore, 
 * is unable to use pslGetParams() to retrieve the parameters list.
 **********/
PSL_STATIC int PSL_API pslGetParamName(int detChan, unsigned short index, char *name)
{
    int statusX;
    int status;


    statusX = dxp_symbolname_by_index(&detChan, &index, name);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting DSP parameter name at index %u for detChan %d",
		index, detChan);
	pslLogError("pslGetParamName", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/**********
 * This routine sets the correct type of preset run:
 * PRESET_RT, PRESET_LT, PRESET_OUT, PRESET_IN, or
 * PRESET_STD, based on the specified type and value
 * information.
 **********/
PSL_STATIC int PSL_API pslDoPreset(int detChan, void *value, unsigned short type)
{
    int status;

    unsigned long presetLen = 0L;

    double presetTime = 0.0;
	double presetTick = 16.0 / (pslGetClockSpeed(detChan) * 1.0e6);

    parameter_t PRESETLEN0;
    parameter_t PRESETLEN1;


    switch (type) {

	case PRESET_STD:
	  presetLen = 0;
	  break;

	case PRESET_RT:
	case PRESET_LT:
	  presetTime = *((double *)value);
	  presetLen = (unsigned long)(presetTime / presetTick);
	  break;

	case PRESET_OUT:
	case PRESET_IN:
	  presetLen = (unsigned long)(*((double *)value));
	  break;

	default:
	  status = XIA_UNKNOWN;
	  sprintf(info_string, "PRESET = %u. This is an impossible error", type);
	  pslLogError("pslDoPreset", info_string, status);
	  return status;
	  break;
    }

    /* PRESETLEN0 = High word
     * PRESETLEN1 = Low word
     */
    PRESETLEN0 = (parameter_t)((presetLen >> 16) & 0xFFFF);
    PRESETLEN1 = (parameter_t)(presetLen & 0xFFFF);

    sprintf(info_string, "presetLen = %lu, PRESETLEN0 = %u, PRESETLEN1 = %u",
	    presetLen, PRESETLEN0, PRESETLEN1);
    pslLogDebug("pslDoPreset", info_string);

    status = pslSetParameter(detChan, "PRESET", type);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error setting PRESET to %u for detChan %d", 
		type, detChan);
	pslLogError("pslDoPreset", info_string, status);
	return status;
    }

    status = pslSetParameter(detChan, "PRESETLEN0", PRESETLEN0);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error setting PRESETLEN0 to %#x for detChan %d",
		PRESETLEN0, detChan);
	pslLogError("pslDoPreset", info_string, status);
	return status;
    }

    status = pslSetParameter(detChan, "PRESETLEN1", PRESETLEN1);

    if (status != XIA_SUCCESS) {

	sprintf(info_string, "Error setting PRESETLEN1 to %#x for detChan %d",
		PRESETLEN1, detChan);
	pslLogError("pslDoPreset", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/**********
 * This routine is used to perform an array
 * of different gain transformations on the
 * system. Also acts as a wrapper around
 * existing gain routines.
 **********/
PSL_STATIC int PSL_API pslGainOperation(int detChan, char *name, void *value, Detector *detector, 
					int detector_chan, XiaDefaults *defaults, double gainScale, 
					CurrentFirmware *currentFirmware, char *detectorType, Module *m)
{
    UNUSED(detChan);
    UNUSED(name);
    UNUSED(value);
    UNUSED(detector);
    UNUSED(detector_chan);
    UNUSED(defaults);
    UNUSED(gainScale);
    UNUSED(currentFirmware);
    UNUSED(detectorType);
	UNUSED(m);

    return XIA_SUCCESS;
}


/** @brief Do the specified board operation
 *
 */
PSL_STATIC int pslBoardOperation(int detChan, char *name, void *value, 
								 XiaDefaults *defs) 
{
  int status;
  int i;

  
  ASSERT(name != NULL);
  ASSERT(value != NULL);

  
  for (i = 0; i < NUM_BOARD_OPS; i++) {
	if (STREQ(boardOps[i].name, name)) {
	  status = boardOps[i].fn(detChan, name, defs, value);

	  if (status != XIA_SUCCESS) {
		sprintf(info_string, "Error doing '%s' operation for detChan %d",
				name, detChan);
		pslLogError("pslBoardOperation", info_string, status);
		return status;
	  }
	  
	  return XIA_SUCCESS;
	}
  }

  sprintf(info_string, "Unknown board operation: '%s'", name);
  pslLogError("pslBoardOperation", info_string, XIA_BAD_NAME);
  return XIA_BAD_NAME;
}


/**********
 * Calls the associated Xerxes exit routine as part of 
 * the board-specific shutdown procedures.
 **********/
PSL_STATIC int PSL_API pslUnHook(int detChan)
{
    int statusX;
    int status;

    
    statusX = dxp_exit(&detChan);

    if (statusX != DXP_SUCCESS) {
	status = XIA_XERXES;
	sprintf(info_string, "Error shutting down detChan %d", detChan);
	pslLogError("pslUnHook", info_string, status);
	return status;
    } 

    return XIA_SUCCESS;
}


PSL_STATIC int PSL_API pslEndSpecialRun(int detChan)
{
    int status;
    int statusX;


    statusX = dxp_stop_control_task(&detChan);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error stopping control task on detChan %d", detChan);
	pslLogError("pslEndSpecialRun", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/** @brief This routine sets the number of SCAs in the Module.
 *
 * Sets the number of SCAs in the module. Assumes that the calling \n
 * routine has checked that the passed in pointers are valid.
 *
 * @param detChan detChan to set the number of SCAs for
 * @param value Pointer to the number of SCAs. Will be cast into a double.
 * @param m Pointer to the module where detChan is assigned.
 *
 */
PSL_STATIC int PSL_API _pslDoNSca(int detChan, void *value, Module *m)
{
  int status;

  size_t nBytes = 0;

  double nSCA = *((double *)value);

  unsigned int modChan = 0;


  ASSERT(value != NULL);
  ASSERT(m != NULL);


  status = pslGetModChan(detChan, m, &modChan);
  /* This is an assertion because the Module should
   * be derived from the detChan in Handel. If the
   * detChan isn't assigned to Module then we have
   * a serious failure.
   */
  ASSERT(status == XIA_SUCCESS);

  /* Clear existing SCAs to prevent a memory leak */
  if ((m->ch[modChan].sca_lo != NULL) ||
	  (m->ch[modChan].sca_hi != NULL))
	{
	  status = pslDestroySCAs(m, modChan);

	  if (status != XIA_SUCCESS) {
		sprintf(info_string, "Error freeing SCAs in module '%s', detChan '%d'",
				m->alias, detChan);
		pslLogError("_pslDoNSca", info_string, status);
		return status;
	  }
	}

  /* What is the limit on the number of SCAs?
   * 1) Check firmware version?
   * 2) Hardcode? 
   */
  m->ch[modChan].n_sca = (unsigned short)nSCA;

  /* Set the appropriate DSP parameter */
  status = pslSetParameter(detChan, "NUMSCA", (unsigned short)nSCA);
  
  if (status != XIA_SUCCESS) {
	m->ch[modChan].n_sca = 0;
	sprintf(info_string, "NUMSCA not available in loaded firmware for detChan %d", detChan);
	pslLogError("_pslDoNSca", info_string, XIA_MISSING_PARAM);
	return XIA_MISSING_PARAM;
  }

  /* Initialize the SCA memory */
  nBytes = m->ch[modChan].n_sca * sizeof(unsigned short);

  m->ch[modChan].sca_lo = (unsigned short *)utils->funcs->dxp_md_alloc(nBytes);
  
  if (m->ch[modChan].sca_lo == NULL) {
	/* Should n_sca be reset to 0 on failure? */
	sprintf(info_string, "Error allocating %d bytes for m->ch[modChan].sca_lo", nBytes);
	pslLogError("_psDoNSca", info_string, XIA_NOMEM);
	return XIA_NOMEM;
  }

  memset(m->ch[modChan].sca_lo, 0, nBytes); 

  m->ch[modChan].sca_hi = (unsigned short *)utils->funcs->dxp_md_alloc(nBytes);

  if (m->ch[modChan].sca_hi == NULL) {
	utils->funcs->dxp_md_free(m->ch[modChan].sca_lo);
	sprintf(info_string, "Error allocating %d bytes for m->ch[modChan].sca_hi", nBytes);
	pslLogError("_pslDoNSca", info_string, XIA_NOMEM);
	return XIA_NOMEM;
  }

  memset(m->ch[modChan].sca_hi, 0, nBytes);

  return XIA_SUCCESS;
}


/** @brief Sets the SCA value specified in name
 *
 * Expects that name is in the format: sca{n}_[lo|hi] \n
 * where 'n' is the SCA number. 
 *
 */
PSL_STATIC int PSL_API _pslDoSCA(int detChan, char *name, void *value, Module *m, XiaDefaults *defaults)
{
  int status;
  int i;

  /* These need constants */
  char bound[3];
  char scaName[MAX_DSP_PARAM_NAME_LEN];
  
  unsigned short bin = (unsigned short)(*(double *)value);
  unsigned short scaNum  = 0;

  unsigned int modChan = 0;


  ASSERT(name != NULL);
  ASSERT(value != NULL);
  ASSERT(STRNEQ(name, "sca"));

  
  sscanf(name, "sca%hu_%s", &scaNum, bound);
  
  if ((!STRNEQ(bound, "lo")) &&
	  (!STRNEQ(bound, "hi")))
  {
	sprintf(info_string, "Malformed SCA string '%s': missing bounds term 'lo' or 'hi'",
			name);
	pslLogError("_pslDoSCA", info_string, XIA_BAD_NAME);
	return XIA_BAD_NAME;
  }
 
  status = pslGetModChan(detChan, m, &modChan);
  ASSERT(status == XIA_SUCCESS);

  if (scaNum >= m->ch[modChan].n_sca) {
	sprintf(info_string, "Requested SCA number '%u' is larger then the number of SCAs '%u'",
			scaNum, m->ch[modChan].n_sca);
	pslLogError("_pslDoSCA", info_string, XIA_SCA_OOR);
	return XIA_SCA_OOR;
  }

  for (i = 0; i < 3; i++) {
	bound[i] = (char)toupper(bound[i]);
  }

  /* Primitive bounds check here: If either of the values
   * ("lo"/"hi") are 0 then we assume that they are not
   * currently set yet. If they are > 0 then we do some
   * simple bounds checking.
   */
  if (STRNEQ(bound, "LO")) {
	if ((m->ch[modChan].sca_hi[scaNum] != 0) &&
		(bin > m->ch[modChan].sca_hi[scaNum]))
	  {
		sprintf(info_string, "New %s value '%u' is greater then the existing high value '%u'",
				name, bin, m->ch[modChan].sca_hi[scaNum]);
		pslLogError("_pslDoSCA", info_string, XIA_BIN_MISMATCH);
		return XIA_BIN_MISMATCH;
	  }

  } else if (STRNEQ(bound, "HI")) {
	if ((m->ch[modChan].sca_lo[scaNum] != 0) &&
		(bin < m->ch[modChan].sca_lo[scaNum]))
	  {
		sprintf(info_string, "New %s value '%u' is less then the existing low value '%u'",
				name, bin, m->ch[modChan].sca_lo[scaNum]);
		pslLogError("_pslDoSCA", info_string, XIA_BIN_MISMATCH);
		return XIA_BIN_MISMATCH;
	  }

  } else {
	/* This is an impossible condition */
	ASSERT(FALSE_);
  }


  /* Create the proper DSP parameter to write */
  sprintf(scaName, "SCA%u%s", scaNum, bound);

  status = pslSetParameter(detChan, scaName, bin);
  
  if (status != XIA_SUCCESS) {
	sprintf(info_string, "Unable to set SCA '%s'", scaName);
	pslLogError("_pslDoSCA", info_string, status);
	return status;
  }
	  
  if (STRNEQ(bound, "LO")) {
	m->ch[modChan].sca_lo[scaNum] = bin;
  } else if (STRNEQ(bound, "HI")) {
	m->ch[modChan].sca_hi[scaNum] = bin;
  } else {
	/* An impossible condition */
	ASSERT(FALSE_);
  }

  status = pslSetDefault(name, value, defaults);

  if (status != XIA_SUCCESS) {
	sprintf(info_string, "Error setting default for '%s' to '%.3f'", name, *((double *)value));
	pslLogError("_pslDoSCA", info_string, status);
	return status;
  }

  return XIA_SUCCESS;
}


/** @brief Get the length of the SCA data buffer
 *
 */
PSL_STATIC int PSL_API _pslGetSCALen(int detChan, XiaDefaults *defaults, void *value)
{
  int status;

  double nSCAs = 0.0;

  unsigned short *scaLen = (unsigned short *)value;


  ASSERT(value != NULL);
  ASSERT(defaults != NULL);

  
  /* Don't use the value from dxp_nsca() since it
   * returns the length of the entire SCA data buffer
   * without considering the number of SCAs set by the
   * user.
   */
  status = pslGetDefault("number_of_scas", (void *)&nSCAs, defaults);

  if (status != XIA_SUCCESS) {
	sprintf(info_string, "Error finding 'number_of_scas' for detChan '%d'", detChan);
	pslLogError("_pslGetSCALen", info_string, status);
	return status;
  }

  *scaLen = (unsigned short)nSCAs;

  return XIA_SUCCESS;
}


/** @brief Gets the SCA Data buffer from Xerxes
 *
 */
PSL_STATIC int PSL_API _pslGetSCAData(int detChan, XiaDefaults *defaults, void *value)
{
  int statusX;
  int status;

  unsigned short i;
  unsigned short maxNSCA = 0;

  unsigned long nBytes = 0;

  double nSCA = 0.0;

  unsigned long *userSCA = (unsigned long *)value;
  unsigned long *sca     = NULL; 


  ASSERT(value != NULL);
  ASSERT(defaults != NULL);


  statusX = dxp_nsca(&detChan, &maxNSCA);
  
  if (statusX != DXP_SUCCESS) {
	sprintf(info_string, "Error reading number of SCAs from Xerxes for detChan '%d'",
			detChan);
	pslLogError("_pslGetSCAData", info_string, statusX);
	return statusX;
  }

  nBytes = maxNSCA * sizeof(unsigned long);
  sca = (unsigned long *)utils->funcs->dxp_md_alloc(nBytes);
  
  if (sca == NULL) {
	sprintf(info_string, "Error allocating '%lu' bytes for 'sca'", nBytes);
	pslLogError("_pslGetSCAData", info_string, XIA_NOMEM);
	return XIA_NOMEM;
  }

  statusX = dxp_readout_sca(&detChan, sca);

  if (statusX != DXP_SUCCESS) {
	utils->funcs->dxp_md_free(sca);
	sprintf(info_string, "Error reading SCA data buffer for detChan '%d'",
			detChan);
	pslLogError("_pslGetSCAData", info_string, XIA_XERXES);
	return XIA_XERXES;
  }

  /* Copy only "number_of_sca"'s worth of SCAs back to the user's array */
  status = pslGetDefault("number_of_scas", (void *)&nSCA, defaults);

  if (status != XIA_SUCCESS) {
	utils->funcs->dxp_md_free(sca);
	sprintf(info_string, "Error finding 'number_of_scas' for detChan '%d'", detChan);
	pslLogError("_pslGetSCAData", info_string, status);
	return status;
  }

  for (i = 0; i < (unsigned short)nSCA; i++) {
	userSCA[i] = sca[i];
  }

  utils->funcs->dxp_md_free(sca);

  return XIA_SUCCESS;
}


/** @brief Starts and stops a run in the shortest, safest amount of time
 *
 */
PSL_STATIC int PSL_API pslQuickRun(int detChan, XiaDefaults *defaults, Module *m)
{
  int status;


  ASSERT(defaults != NULL);


  status = pslStartRun(detChan, 0, defaults, m);
  
  if (status != XIA_SUCCESS) {
	sprintf(info_string, "Error starting quick run on detChan %d", detChan);
	pslLogError("pslQuickRun", info_string, status);
	return status;
  }

  status = pslStopRun(detChan, m);

  if (status != XIA_SUCCESS) {
	sprintf(info_string, "Error stopping quick run on detChan %d", detChan);
	pslLogError("pslQuickRun", info_string, status);
	return status;
  }

  return XIA_SUCCESS;
}


/** @brief Get the Preset tick
 *
 */
PSL_STATIC int pslGetPresetTick(int detChan, char *name, XiaDefaults *defs,
								void *value)
{
  double *presetTick = (double *)value;

  UNUSED(name);
  UNUSED(defs);


  ASSERT(value != NULL);


  *presetTick = 16.0 / (pslGetClockSpeed(detChan) * 1.0e6);

  return XIA_SUCCESS;
}


/** @brief Run the 'check memory' WHICHTEST via Xerxes
 *
 */
PSL_STATIC int pslCheckExternalMemory(int detChan, char *name, XiaDefaults *defs,
									  void *value)
{
  int statusX;
  int status;

  short ct = CT_DXP2X_CHECK_MEMORY;

  unsigned int infoLen = 3;

  float poll    = 0.1f;
  float timeout = 1.0f;

  int info[3];


  UNUSED(name);
  UNUSED(defs);
  UNUSED(value);


  info[0] = 0;
  info[1] = 1;
  info[2] = 2;

  /* The procedure is to start a control task, wait for BUSY to go to 99 and
   * then stop the control task.
   */
  statusX = dxp_start_control_task(&detChan, &ct, &infoLen, info);

  if (statusX != DXP_SUCCESS) {
	sprintf(info_string, "Error starting check memory control task (%d) for "
			"detChan %d", ct, detChan);
	pslLogError("pslCheckExternalMemory", info_string, XIA_XERXES);
	return XIA_XERXES;
  }

  status = pslWaitForBusy(detChan, 99, poll, timeout);

  if (status != XIA_SUCCESS) {
	sprintf(info_string, "Error waiting for check memory to finish on "
			"detChan %d", detChan);
	pslLogError("pslCheckExternalMemory", info_string, status);
	return status;
  }

  statusX = dxp_stop_control_task(&detChan);

  if (statusX != DXP_SUCCESS) {
	sprintf(info_string, "Error stopping check memory control task (%d) for "
			"detChan %d", ct, detChan);
	pslLogError("pslCheckExternalMemory", info_string, XIA_XERXES);
	return XIA_XERXES;
  }

  return XIA_SUCCESS;
}


/** @brief Waits for BUSY to go to the specified value on the specified channel.
 *
 * @param poll The interval to wait in seconds between successive checks of BUSY.
 *
 * @param timeout The total time to wait in seconds for the specified value of 
 * @a BUSY to be reached.
 *
 */
PSL_STATIC int pslWaitForBusy(int detChan, parameter_t busy, float poll,
							  float timeout)
{
  int status;

  unsigned int i;
  unsigned int nIters = (unsigned int)ROUND(timeout / poll);

  parameter_t BUSY = 0;


  /* We don't flag an error since the user doesn't directly call this routine,
   * which really means that a piece of XIA code is at fault if 'poll' or 
   * 'timeout' is set to 0, hence the ASSERT().
   */
  ASSERT(poll > 0.0);
  ASSERT(timeout > 0.0);


  for (i = 0; i < nIters; i++) {
	
	utils->funcs->dxp_md_wait(&poll);

	status = pslGetParameter(detChan, "BUSY", &BUSY);

	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error getting BUSY for detChan %d", detChan);
	  pslLogError("pslWaitForBusy", info_string, status);
	  return status;
	}

	if (BUSY == busy) {
	  return XIA_SUCCESS;
	}
  }

  sprintf(info_string, "Timeout waiting for BUSY to equal '%u' (poll = %.3f, "
		  "timeout = %.3f) for detChan %d", busy, poll, timeout, detChan);
  pslLogError("pslWaitForBusy", info_string, XIA_TIMEOUT);
  return XIA_TIMEOUT;
}


/** @brief Waits for all channels in the module to start a run.
 *
 */
PSL_STATIC int psl__WaitForRunStart(Module *m)
{
  int status;

  float timeout  = 0.5f;
  float pollTime = .001f;

  boolean_t isStarted = FALSE_;

  unsigned int i;
  unsigned int j;
  unsigned int n_polls = 0;

  parameter_t BUSY     = 0;

  
  ASSERT(m != NULL);


  n_polls = (unsigned int)ROUND(timeout / pollTime);

  for (i = 0; i < n_polls; i++) {
	isStarted = TRUE_;

	for (j = 0; j < m->number_of_channels; j++) {
	  /* Only test channels that are enabled. */
	  if (m->channels[j] >= 0) {
		status = pslGetParameter(m->channels[j], "BUSY", &BUSY);

		if (status != XIA_SUCCESS) {
		  sprintf(info_string, "Error getting BUSY for detChan = %d",
				  m->channels[j]);
		  pslLogError("psl__WaitForRunStart", info_string, status);
		  return status;
		}

		/* BUSY = 6 indicates that the run is active, while BUSY = 99
		 * indicates that the run was active, stopped and is waiting
		 * for the other channels to finish starting their runs. BUSY = 99
		 * mainly covers the case where a short preset run starts and finishes
		 * before we get a chance to check BUSY.
		 */
		if (BUSY == 6 || BUSY == 99) {
		  isStarted = (boolean_t)(isStarted && TRUE_);
		} else {
		  isStarted = (boolean_t)(isStarted && FALSE_);
		}
	  }
	}

	if (isStarted) {
	  sprintf(info_string, "Run started at iteration = %u", i);
	  pslLogDebug("psl__WaitForRunStart", info_string);
	  return XIA_SUCCESS;
	}

	utils->funcs->dxp_md_wait(&pollTime);
  }

  sprintf(info_string, "Timeout (BUSY = %u) waiting for the run to start on "
		  "module '%s'", BUSY, m->alias);
  pslLogError("psl__WaitForRunStart", info_string, XIA_TIMEOUT);
  return XIA_TIMEOUT;
}


/** @brief Enables the baseline cut.
 *
 */
ACQUISITION_VALUE(EnableBaselineCut)
{
  int status;
  int enabled = 0;

  parameter_t RUNTASKS;

  UNUSED(fs);
  UNUSED(detectorType);
  UNUSED(preampGain);
  UNUSED(gainScale);
  UNUSED(det);
  UNUSED(m);
  UNUSED(detector_chan);


  ASSERT(value != NULL);
  ASSERT(defs != NULL);


  status = pslGetParameter(detChan, "RUNTASKS", &RUNTASKS);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error getting RUNTASKS for detChan %d", detChan);
    pslLogError("pslDoEnableBaselineCut", info_string, status);
    return status;
  }

  enabled = (int)(*((double *)value));

  if (enabled) {
    RUNTASKS |= RT_BASELINE_CUT;
  } else {
    RUNTASKS &= ~RT_BASELINE_CUT;
  }

  status = pslSetParameter(detChan, "RUNTASKS", RUNTASKS);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error enabling baseline cut for detChan %d", detChan);
    pslLogError("pslDoEnableBaselineCut", info_string, status);
    return status;
  }

  status = pslSetDefault("enable_baseline_cut", (void *)value, defs);
  ASSERT(status == XIA_SUCCESS);

  return XIA_SUCCESS;
}


/** @brief Sets the baseline cut to the specified percentage.
 *
 */
ACQUISITION_VALUE(BaselineCut)
{
  int status;

  double blcut;

  parameter_t BLCUT = 0;

  UNUSED(detector_chan);
  UNUSED(det);
  UNUSED(m);
  UNUSED(gainScale);
  UNUSED(preampGain);
  UNUSED(detectorType);
  UNUSED(fs);


  ASSERT(value != NULL);


  blcut = *((double *)value);
  BLCUT = (parameter_t)ROUND(32768.0 * (blcut / 100.0));

  status = pslSetParameter(detChan, "BLCUT", BLCUT);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error setting baseline cut to %0.3f%% for detChan %d",
            blcut, detChan);
    pslLogError("pslDoBaselineCut", info_string, status);
    return status;
  }

  /* Re-compute the actual baseline cut do to rounding. */
  *((double *)value) = ((double)BLCUT / 32768.0) * 100.0;

  sprintf(info_string, "Updated baseline cut = %0.3f%% for detChan %d",
          *((double *)value), detChan);
  pslLogDebug("pslDoBaselineCut", info_string);

  status = pslSetDefault("baseline_cut", value, defs);
  ASSERT(status == XIA_SUCCESS);
  
  return XIA_SUCCESS;
}


/** @brief Set the number of baseline samples to average.
 *
 */
ACQUISITION_VALUE(BaselineFilterLength)
{
  int status;

  double nSamples = 0.0;

  parameter_t BLFILTER = 0;

  UNUSED(detector_chan);
  UNUSED(det);
  UNUSED(m);
  UNUSED(gainScale);
  UNUSED(preampGain);
  UNUSED(detectorType);
  UNUSED(fs);


  ASSERT(value != NULL);
  ASSERT(defs != NULL);

  
  nSamples = *((double *)value);

  if (nSamples < MIN_NUM_BASELINE_SAMPLES ||
      nSamples > MAX_NUM_BASELINE_SAMPLES)
    {
      sprintf(info_string, "Requested number of baseline samples (%0.3f) is "
              "not in the allowed range (%0.3f, %0.3f) for detChan %d",
              nSamples, MIN_NUM_BASELINE_SAMPLES, MAX_NUM_BASELINE_SAMPLES);
      pslLogError("pslDoBaselineFilterLength", info_string, XIA_BASELINE_OOR);
      return XIA_BASELINE_OOR;
    }

  BLFILTER = (parameter_t)ROUND(32768.0 / nSamples);

  status = pslSetParameter(detChan, "BLFILTER", BLFILTER);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error setting number of baseline samples to %0.3f "
            "(BLFILTER = %u) for detChan %d", nSamples, BLFILTER, detChan);
    pslLogError("pslDoBaselineFilterLength", info_string, status);
    return status;
  }

  /* Re-calculate the actual number of baseline samples since
   * BLFILTER is rounded.
   */
  *((double *)value) = 32768.0 / (double)BLFILTER;

  status = pslSetDefault("baseline_filter_length", value, defs);
  ASSERT(status == XIA_SUCCESS);

  return XIA_SUCCESS;
}


/** @brief Set the reset delay in microseconds.
 *
 * Only valid for reset-type preamplifiers.
 */
ACQUISITION_VALUE(ResetDelay)
{
  int status;

  double resetTime = 0.0;

  parameter_t RESETINT = 0;

  UNUSED(m);
  UNUSED(gainScale);
  UNUSED(preampGain);
  UNUSED(detectorType);
  UNUSED(fs);
  UNUSED(detector_chan);


  ASSERT(value != NULL);
  ASSERT(det != NULL);

  
  if (det->type != XIA_DET_RESET) {
    sprintf(info_string, "Skipping reset delay since preamplifier is not "
            "reset-type for detChan %d", detChan);
    pslLogInfo("pslDoResetDelay", info_string);
    return XIA_SUCCESS;
  }

  resetTime = *((double *)value);
  RESETINT  = (parameter_t)ROUND(4.0 * resetTime);

  status = pslSetParameter(detChan, "RESETINT", RESETINT);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error setting reset delay to %0.3f microseconds for "
            "detChan %d", resetTime, detChan);
    pslLogError("pslDoResetDelay", info_string, status);
    return status;
  }

  /* Recalculate the value due to rounding. */
  *((double *)value) = (double)RESETINT / 4.0;

  status = pslSetDefault("reset_delay", value, defs);
  ASSERT(status == XIA_SUCCESS);

  det->typeValue[detector_chan] = *((double *)value);

  return XIA_SUCCESS;
}


/** @brief Synchronize the reset delay acquisition value with the Detector
 * structure.
 *
 * If the specified detector does not have a reset-type preamplifier, this
 * routine does nothing (but does not signal an error).
 *
 */
PSL_STATIC int pslSynchResetDelay(int detChan, int det_chan, Module *m,
                                  Detector *det, XiaDefaults *defs)
{
  int status;

  double resetDelay = 0.0;

  UNUSED(m);
  UNUSED(detChan);


  ASSERT(det != NULL);
  ASSERT(defs != NULL);


  if (det->type == XIA_DET_RESET) {
	resetDelay = det->typeValue[det_chan];

	status = pslSetDefault("reset_delay", (void *)&resetDelay, defs);
	ASSERT(status == XIA_SUCCESS);
  }

  return XIA_SUCCESS;
}


/** @brief Sets the specified decay time in microseconds.
 *
 * Only works with RC-type preamplifiers.
 */
ACQUISITION_VALUE(DecayTime)
{
  int status;

  double decayTime = 0.0;

  parameter_t RCTAU     = 0;
  parameter_t RCTAUFRAC = 0;

  UNUSED(m);
  UNUSED(gainScale);
  UNUSED(preampGain);
  UNUSED(detectorType);
  UNUSED(fs);

  
  ASSERT(value != NULL);
  ASSERT(det != NULL);


  if (det->type != XIA_DET_RCFEED) {
    sprintf(info_string, "Skipping decay time since preamplifier is not "
            "RC-feedback for detChan %d", detChan);
    pslLogInfo("pslDoDecayTime", info_string);
    return XIA_SUCCESS;
  }

  decayTime = *((double *)value);
  RCTAU     = (parameter_t)floor(decayTime);
  RCTAUFRAC = (parameter_t)ROUND((decayTime - (double)RCTAU) * 65536);

  status = pslSetParameter(detChan, "RCTAU", RCTAU);
  
  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error setting non-fractional component (%u) of decay "
            "time (%0.3f) for detChan %d", RCTAU, decayTime, detChan);
    pslLogError("pslDoDecayTime", info_string, status);
    return status;
  }

  status = pslSetParameter(detChan, "RCTAUFRAC", RCTAUFRAC);

  /* In a perfect world, we would cache the previous value of RCTAU and rollback
   * the new RCTAU value if the call to set RCTAUFRAC failed. The probability
   * of such an error happening is probably quite low and, most likely, would
   * indicate a serious hardware problem, so I haven't implemented such a
   * recovery mode.
   */
  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error setting fractional component (%u) of decay "
            "time (%0.3f) for detChan %d", RCTAUFRAC, decayTime, detChan);
    pslLogError("pslDoDecayTime", info_string, status);
    return status;
  }

  /* Recalculate the actual decay time due to rounding. */
  *((double *)value) = ((double)RCTAUFRAC / 65536.0) + (double)RCTAU;

  status = pslSetDefault("decay_time", value, defs);
  ASSERT(status == XIA_SUCCESS);

  det->typeValue[detector_chan] = *((double *)value);

  return XIA_SUCCESS;
}


/** @brief Synchronize the decay time acquisition value with the detector
 * structure.
 *
 * Only works with RC-feedback preamplifiers.
 *
 */
PSL_STATIC int pslSynchDecayTime(int detChan, int det_chan, Module *m,
                                 Detector *det, XiaDefaults *defs)
{
  int status;

  double decayTime = 0.0;

  UNUSED(m);
  UNUSED(detChan);


  ASSERT(det != NULL);
  ASSERT(defs != NULL);


  if (det->type == XIA_DET_RCFEED) {
	decayTime = det->typeValue[det_chan];

	status = pslSetDefault("decay_time", (void *)&decayTime, defs);
	ASSERT(status == XIA_SUCCESS);
  }

  return XIA_SUCCESS;
}


/** @brief Sets the preamplifier gain in mV/keV.
 *
 */
ACQUISITION_VALUE(PreampGain)
{
  int status;

  double newGain = 0.0;

  UNUSED(preampGain);
  UNUSED(detectorType);
  UNUSED(fs);


  ASSERT(value != NULL);
  ASSERT(defs != NULL);
  ASSERT(det != NULL);
  ASSERT(m != NULL);

  
  newGain = *((double *)value);

  status = pslDoGainSetting(detChan, defs, newGain, gainScale, m);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error setting preamplifier gain to %0.3f mV/keV for "
            "detChan %d", newGain, detChan);
    pslLogError("pslDoPreampGain", info_string, status);
    return status;
  }
  
  det->gain[detector_chan] = newGain;
  
  status = pslSetDefault("preamp_gain", value, defs);
  ASSERT(status == XIA_SUCCESS);

  return XIA_SUCCESS;
}


/** @brief Synchronize the preamplifier gain acquisition value with the
 * Detector structure.
 *
 */
PSL_STATIC int pslSynchPreampGain(int detChan, int det_chan, Module *m,
                                  Detector *det, XiaDefaults *defs)
{
  int status;

  double gain = 0.0;

  UNUSED(m);
  UNUSED(detChan);


  ASSERT(det != NULL);
  ASSERT(defs != NULL);


  gain = det->gain[det_chan];

  status = pslSetDefault("preamp_gain", (void *)&gain, defs);
  ASSERT(status == XIA_SUCCESS);

  return XIA_SUCCESS;
}


/** @brief Master gain setting routine.
 *
 * Computes the gain based on the current system parameters and
 * updates every acquisition value that depends on the gain.
 */
PSL_STATIC int pslDoGainSetting(int detChan, XiaDefaults *defaults, 
								double preampGain, double gainScale, Module *m)
{
  int status;

  double adcPercentRule = 0.0;
  double calibEV = 0.0;
  double mcaBinWidth = 0.0;
  double threshold = 0.0;
  double slowthresh = 0.0;

  parameter_t GAINDAC;
  parameter_t SLOWLEN;

  status = pslGetDefault("adc_percent_rule", (void *)&adcPercentRule, defaults);
  ASSERT(status == XIA_SUCCESS);

  status = pslGetDefault("calibration_energy", (void *)&calibEV, defaults);
  ASSERT(status == XIA_SUCCESS);

  status = pslGetDefault("mca_bin_width", (void *)&mcaBinWidth, defaults);
  ASSERT(status == XIA_SUCCESS);

  status = pslGetParameter(detChan, "SLOWLEN", &SLOWLEN);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error reading current slow filter length for "
            "detChan %d", detChan);
    pslLogError("pslDoGainSetting", info_string, status);
    return status;
  }

  status = pslCalculateGain(adcPercentRule, calibEV, preampGain,
                            mcaBinWidth, SLOWLEN, &GAINDAC, gainScale); 
	
  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error calculating gain for detChan %d", detChan);
    pslLogError("pslDoGainSetting", info_string, status);
    return status;
  }

  status = pslSetParameter(detChan, "GAINDAC", GAINDAC);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error updating GAINDAC for detChan %d", detChan);
    pslLogError("pslDoGainSetting", info_string, status);
    return status;
  }

  /* Call routines that depend on changes in the gain so that
   * their calculations will now be correct.
   */

  /* Use the "old" settings to recalculate the trigger threshold, slow
   * threshold and MCA bin width.
   */
  status = pslGetDefault("trigger_threshold", (void *)&threshold, defaults);
  ASSERT(status == XIA_SUCCESS);

  status = pslDoTriggerThreshold(detChan, (void *)&threshold, NULL,
                                 NULL, defaults, preampGain, gainScale,
                                 m, NULL, 0);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error updating trigger threshold for detChan %d",
            detChan);
    pslLogError("pslDoGainSetting", info_string, status);
    return status;
  }
	
  status = pslGetDefault("energy_threshold", (void *)&slowthresh, defaults);
  ASSERT(status == XIA_SUCCESS);

  status = pslDoEnergyThreshold(detChan, (void *)&slowthresh, NULL, NULL,
                                defaults, preampGain, gainScale, m, NULL, 0);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error updating energy threshold for detChan %d",
            detChan);
    pslLogError("pslDoGainSetting", info_string, status);
    return status;
  }

  /* Calculate and set the gain (again). It's done twice, for now, to
   * protect against the case where we change all of these other parameters
   * and then find out the gain is out-of-range and, therefore, the whole
   * system is out-of-sync.
   */
  status = pslCalculateGain(adcPercentRule, calibEV, preampGain,
                            mcaBinWidth, SLOWLEN, &GAINDAC, gainScale); 
	
  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error calculating gain for detChan %d", detChan);
    pslLogError("pslDoGainSetting", info_string, status);
    return status;
  }

  status = pslSetParameter(detChan, "GAINDAC", GAINDAC);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error updating GAINDAC for detChan %d", detChan);
    pslLogError("pslDoGainSetting", info_string, status);
    return status;
  }

  /* Start and stop a run to "set" the GAINDAC.
   * Reference: BUG ID #83
   */
  status = pslQuickRun(detChan, defaults, m);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error performing a quick run to set GAINDAC on "
            "detChan %d", detChan);
    pslLogError("pslDoGainSetting", info_string, status);
    return status;
  }
	
  return XIA_SUCCESS;
}


/** @brief Set the detector polarity.
 *
 * Also updates the polarity in the detector strucuture.
 */
ACQUISITION_VALUE(DetectorPolarity)
{
  int statusX;
  int status;
  
  double polarity;
  
  parameter_t POLARITY;

  UNUSED(gainScale);
  UNUSED(preampGain);
  UNUSED(detectorType);
  UNUSED(fs);

  
  polarity = *((double *) value);
  POLARITY = (parameter_t) polarity;
  
  statusX = dxp_set_one_dspsymbol(&detChan, "POLARITY", &POLARITY);
  
  if (statusX != DXP_SUCCESS) {
	sprintf(info_string, "Error setting POLARITY for detChan %d", detChan);
	pslLogError("pslDoPolarity", info_string, XIA_XERXES);
	return XIA_XERXES;
  }
  
  /* Start and stop a run to "set" the polarity value.
   * Reference: BUG ID #17, #84
   */
  status = pslQuickRun(detChan, defs, m);
  
  if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error performing a quick run to set the polarity "
              "on detChan %d", detChan);
	  pslLogError("pslDoPolarity", info_string, status);
	  return status;
	}
  
  /* Set the entry in the detector structure for polarity */
  det->polarity[detector_chan] = (unsigned short) polarity;

  /* Set the polarity entry in the acquisition value list */
  status = pslSetDefault("detector_polarity", (void *) &polarity, defs);
  ASSERT(status == XIA_SUCCESS);
  
  return XIA_SUCCESS;
}


/** @brief Synchronize the polarity acquisition value with the Detector
 * structure.
 *
 */
PSL_STATIC int pslSynchDetectorPolarity(int detChan, int det_chan, Module *m,
                                        Detector *det, XiaDefaults *defs)
{
  int status;

  double pol = 0.0;

  UNUSED(m);
  UNUSED(detChan);


  ASSERT(det != NULL);
  ASSERT(defs != NULL);

  pol = (double)det->polarity[det_chan];

  status = pslSetDefault("detector_polarity", (void *)&pol, defs);
  ASSERT(status == XIA_SUCCESS);

  return XIA_SUCCESS;
}
