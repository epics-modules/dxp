/* This file contains the routines relating to control of the run
 * parameters, such as xiaSetAcquitionValues and xiaGainOperation.
 */

/*
 * Copyright (c) 2002-2004 X-ray Instrumentation Associates
 * Copyright (c) 2005-2016 XIA LLC
 * All rights reserved
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
 *   * Neither the name of XIA LLC
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
 */


#include <ctype.h>

#include "xia_common.h"
#include "xia_assert.h"
#include "xia_handel_structures.h"
#include "xia_handel.h"
#include "xia_system.h"

#include "handeldef.h"
#include "handel_errors.h"
#include "handel_log.h"


HANDEL_STATIC boolean_t HANDEL_API xiaIsUpperCase(char *string);


/*
 * Sets an acquisition value.
 *
 * detChan may be a single detChan or a detChan set.
 *
 * name may refer to an acquisition value or a DSP parameter.
 *
 * value must be a double *. value may be adjusted in translation. The
 * updated value is returned through the same parameter.
 */
HANDEL_EXPORT int HANDEL_API xiaSetAcquisitionValues(int detChan, char *name, void *value)
{
    int status;
    int elemType;
    int detector_chan;

    double savedValue;

    unsigned int modChan;

    char detectorType[MAXITEM_LEN];

    char *boardAlias;
    char *firmAlias;
    char *detectorAlias;

    char boardType[MAXITEM_LEN];

    boolean_t valueExists = FALSE_;

    DetChanElement *detChanElem = NULL;

    DetChanSetElem *detChanSetElem = NULL;

    XiaDefaults *defaults = NULL;

    XiaDaqEntry *entry = NULL;

    FirmwareSet *firmwareSet = NULL;

    Module *module = NULL;

    Detector *detector = NULL;

    CurrentFirmware *currentFirmware = NULL;

    PSLFuncs localFuncs;


    /* See Bug ID #66. Protect
     * against malformed name
     * strings.
     */
    if (name == NULL) {
        status = XIA_BAD_NAME;
        xiaLogError("xiaSetAcquisitionValues",
                    "Name may not be NULL",
                    status);
        return status;
    }

    elemType = xiaGetElemType((unsigned int)detChan);

    switch(elemType)
    {
    case SINGLE:
        status = xiaGetBoardType(detChan, boardType);

        if (status != XIA_SUCCESS)
        {
            sprintf(info_string, "Unable to get boardType for detChan %d", detChan);
            xiaLogError("xiaSetAcquisitionValues", info_string, status);
            return status;
        }

        status = xiaLoadPSL(boardType, &localFuncs);

        if (status != XIA_SUCCESS)
        {
            sprintf(info_string, "Unable to load PSL funcs for detChan %d", detChan);
            xiaLogError("xiaSetAcquisitionValues", info_string, status);
            return status;
        }

        /* I know that this part of the code is unbelievably weird. Ultimately,
         * the solution to this problem is to unify the dynamic config interface,
         * which we will do eventually. For now, we have to suffer though...
         */

        defaults    = xiaGetDefaultFromDetChan((unsigned int)detChan);

        boardAlias  = xiaGetAliasFromDetChan(detChan);
        module      = xiaFindModule(boardAlias);
        modChan     = xiaGetModChan((unsigned int)detChan);
        firmAlias   = module->firmware[modChan];
        firmwareSet = xiaFindFirmware(firmAlias);
        /* The preampGain is passed in directly since the detector alias also has the
         * detector channel information encoded into it.
         */
        detectorAlias 		= module->detector[modChan];
        detector_chan 		= module->detector_chan[modChan];
        detector      		= xiaFindDetector(detectorAlias);
        currentFirmware	    = &module->currentFirmware[modChan];

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
            sprintf(info_string, "No detector type specified for detChan %d", detChan);
            xiaLogError("xiaSetAcquisitionValues", info_string, status);
            return status;
            break;
        }

        /* At this stage, xiaStartSystem() has been called so we
         * know that the required defaults are already present.
         * Therefore, any acq. value beyond this point is
         * assumed to be "special" and should be added if it
         * isn't present.
         */
        entry = defaults->entry;

        while (entry != NULL) {

            if (STREQ(name, entry->name))
            {
                valueExists = TRUE_;
                break;
            }

            entry = entry->next;
        }

        if (!valueExists)
        {
            sprintf(info_string, "Adding %s to defaults %s", name, defaults->alias);
            xiaLogInfo("xiaSetAcquisitionValues", info_string);

            status = xiaAddDefaultItem(defaults->alias, name, value);

            if (status != XIA_SUCCESS) {

                sprintf(info_string, "Error adding %s to defaults %s", name, defaults->alias);
                xiaLogError("xiaSetAcquisitionValues", info_string, status);
                return status;
            }
        }

        status = localFuncs.setAcquisitionValues(detChan, name, value, defaults,
                                                 firmwareSet, currentFirmware,
                                                 detectorType, detector,
                                                 detector_chan, module, modChan);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Unable to set '%s' to %0.3f for detChan %d.",
                    name, *((double *)value), detChan);
            xiaLogError("xiaSetAcquisitionValues", info_string, status);
            return status;
        }

        break;

    case SET:
        detChanElem = xiaGetDetChanPtr((unsigned int)detChan);

        detChanSetElem = detChanElem->data.detChanSet;

        /* Save the user value, else it will be changed by the return value of the setAcquisitionValues
         * call.  Use the last return value as the actual return value */
        savedValue = *((double *) value);

        while (detChanSetElem != NULL)
        {
            *((double *) value) = savedValue;
            status = xiaSetAcquisitionValues((int)detChanSetElem->channel, name, value);

            if (status != XIA_SUCCESS)
            {
                sprintf(info_string, "Error setting acquisition values for detChan %d", detChan);
                xiaLogError("xiaSetAcquisitionValues", info_string, status);
                return status;
            }

            detChanSetElem = getListNext(detChanSetElem);
        }

        break;

    case 999:
        status = XIA_INVALID_DETCHAN;
        xiaLogError("xiaSetAcquisitionValues", "detChan number is not in the list of valid values ", status);
        return status;
        break;
    default:
        status = XIA_UNKNOWN;
        xiaLogError("xiaSetAcquisitionValues", "Should not be seeing this message", status);
        return status;
        break;
    }

    return XIA_SUCCESS;
}


/*
 * Gets an acquisition value. Unless otherwise noted, all acquisition
 * values require value to be a double *, cast as void *.
 */
HANDEL_EXPORT int HANDEL_API xiaGetAcquisitionValues(int detChan, char *name, void *value)
{
    int status;
    int elemType;

    char boardType[MAXITEM_LEN];

    XiaDefaults *defaults = NULL;

    PSLFuncs localFuncs;

    elemType = xiaGetElemType((unsigned int)detChan);

    switch(elemType)
    {
    case SET:
        status = XIA_BAD_TYPE;
        xiaLogError("xiaGetAcquisitionValues", "Unable to retrieve values for a detChan SET", status);
        return status;
        break;

    case SINGLE:
        status = xiaGetBoardType(detChan, boardType);

        if (status != XIA_SUCCESS)
        {
            sprintf(info_string, "Unable to get boardType for detChan %d", detChan);
            xiaLogError("xiaGetAcquisitionValues", info_string, status);
            return status;
        }

        status = xiaLoadPSL(boardType, &localFuncs);

        if (status != XIA_SUCCESS)
        {
            sprintf(info_string, "Unable to load PSL funcs for detChan %d", detChan);
            xiaLogError("xiaGetAcquisitionValues", info_string, status);
            return status;
        }

        defaults = xiaGetDefaultFromDetChan((unsigned int)detChan);

        status = localFuncs.getAcquisitionValues(detChan, name, value, defaults);

        if (status != XIA_SUCCESS)
        {
            sprintf(info_string, "Unable to get acquisition values for detChan %d", detChan);
            xiaLogError("xiaGetAcquisitionValues", info_string, status);
            return status;
        }

        break;

    case 999:
        status = XIA_INVALID_DETCHAN;
        xiaLogError("xiaGetAcquisitionValues", "detChan number is not in the list of valid values ", status);
        return status;
        break;
    default:
        status = XIA_UNKNOWN;
        xiaLogError("xiaGetAcquisitionValues", "Should not be seeing this message", status);
        return status;
        break;
    }

    return XIA_SUCCESS;
}


/*
 * Removes an acquisition value from the channel. There is no
 * complementary Add routine, but you can add with
 * xiaSetAcquisitionValues.
 *
 * The only known use is as a hack to reset a standard acquisition
 * value to its default.
 *
 * Calls user setup to download all acquisition values after removing
 * the specified name, so for performance and consistency reasons it
 * is critical not to call this often or during data acquisition.
 */
HANDEL_EXPORT int HANDEL_API xiaRemoveAcquisitionValues(int detChan, char *name)
{
    int status;
    int elemType;
    int modChan;

    char boardType[MAXITEM_LEN];
    char detType[MAXITEM_LEN];

    XiaDefaults *defaults = NULL;

    XiaDaqEntry *entry    = NULL;
    XiaDaqEntry *previous = NULL;

    DetChanElement *detChanElem = NULL;

    DetChanSetElem *detChanSetElem = NULL;

    boolean_t canRemove = FALSE_;

    PSLFuncs localFuncs;

    FirmwareSet *fs = NULL;

    Module *m = NULL;

    Detector *det = NULL;

    char *alias     = NULL;
    char *firmAlias = NULL;
    char *detAlias  = NULL;


    elemType = xiaGetElemType(detChan);

    switch(elemType) {

    case SINGLE:

        status = xiaGetBoardType(detChan, boardType);

        if (status != XIA_SUCCESS) {

            sprintf(info_string, "Error getting board type for detChan %d", detChan);
            xiaLogError("xiaRemoveAcquisitionValues", info_string, status);
            return status;
        }

        status = xiaLoadPSL(boardType, &localFuncs);

        if (status != XIA_SUCCESS) {

            sprintf(info_string, "Unable to load PSL functions for detChan %d", detChan);
            xiaLogError("xiaRemoveAcquisitionValues", info_string, status);
            return status;
        }

        canRemove = localFuncs.canRemoveName(name);

        if (canRemove) {

            defaults = xiaGetDefaultFromDetChan(detChan);

            entry = defaults->entry;

            while (entry != NULL) {

                if (STREQ(name, entry->name)) {

                    if (previous == NULL) {

                        defaults->entry = entry->next;

                    } else {

                        previous->next = entry->next;
                    }

                    handel_md_free((void *)entry->name);
                    handel_md_free((void *)entry);

                    break;
                }

                previous = entry;
                entry = entry->next;
            }

            /* Since we don't know what was removed, we better re-download all of
             * the acquisition values again.
             */
            alias         = xiaGetAliasFromDetChan(detChan);
            m             = xiaFindModule(alias);
            modChan       = xiaGetModChan(detChan);
            firmAlias     = m->firmware[modChan];
            fs            = xiaFindFirmware(firmAlias);
            detAlias      = m->detector[modChan];
            det           = xiaFindDetector(detAlias);
            /* Reset the defaults. */
            defaults      = xiaGetDefaultFromDetChan(detChan);

            switch (det->type) {

            case XIA_DET_RESET:
                strcpy(detType, "RESET");
                break;

            case XIA_DET_RCFEED:
                strcpy(detType, "RC");
                break;

            default:
            case XIA_DET_UNKNOWN:
                sprintf(info_string, "No detector type specified for detChan %d", detChan);
                xiaLogError("xiaSetAcquisitionValues", info_string, XIA_MISSING_TYPE);
                return XIA_MISSING_TYPE;
                break;
            }

            status = localFuncs.userSetup(detChan, defaults, fs,
                                          &(m->currentFirmware[modChan]), detType,
                                          det, m->detector_chan[modChan], m, modChan);

            if (status != XIA_SUCCESS) {
                sprintf(info_string, "Error updating acquisition values after '%s' "
                        "removed from list for detChan %d", name, detChan);
                xiaLogError("xiaRemoveAcquisitionValues", info_string, status);
                return status;
            }

        } else {

            status = XIA_NO_REMOVE;
            sprintf(info_string, "Specified acquisition value %s is a required value for detChan %d", name, detChan);
            xiaLogError("xiaRemoveAcquisitionValues", info_string, status);
            return status;
        }

        break;

    case SET:

        detChanElem = xiaGetDetChanPtr(detChan);

        detChanSetElem = detChanElem->data.detChanSet;

        while (detChanSetElem != NULL) {

            status = xiaRemoveAcquisitionValues(detChanSetElem->channel, name);

            if (status != XIA_SUCCESS) {

                sprintf(info_string, "Error removing %s from detChan %u", name,
                        detChanSetElem->channel);
                xiaLogError("xiaRemoveAcquisitionValues", info_string, status);
                return status;
            }

            detChanSetElem = detChanSetElem->next;
        }

        break;

    case 999:
        status = XIA_INVALID_DETCHAN;
        xiaLogError("xiaRemoveAcquisitionValues", "detChan number is not in the list of valid values ", status);
        return status;
        break;
    default:

        status = XIA_UNKNOWN;
        xiaLogError("xiaRemoveAcquisitionValues", "Should not be seeing this message", status);
        return status;
        break;
    }

    return XIA_SUCCESS;

}


/*
 * Downloads all user parameters, or DSP parameters set by calls to
 * xiaSetAcquisitionValues().
 */
HANDEL_EXPORT int HANDEL_API xiaUpdateUserParams(int detChan)
{
    int status;
    int elemType;

    unsigned short param = 0;

    XiaDefaults *defaults = NULL;

    XiaDaqEntry *entry = NULL;

    DetChanElement *detChanElem = NULL;

    DetChanSetElem *detChanSetElem = NULL;



    xiaLogDebug("xiaUpdateUserParams", "Searching for user params to download");

    elemType = xiaGetElemType(detChan);

    switch (elemType) {

    case SINGLE:

        defaults = xiaGetDefaultFromDetChan(detChan);

        entry = defaults->entry;

        while (entry != NULL) {

            if(xiaIsUpperCase(entry->name)) {

                param = (unsigned short)(entry->data);

                sprintf(info_string, "Setting %s to %u", entry->name, param);
                xiaLogDebug("xiaUpdateUserParams", info_string);

                status = xiaSetParameter(detChan, entry->name, param);

                if (status != XIA_SUCCESS) {

                    sprintf(info_string, "Error setting parameter %s for detChan %d", entry->name, detChan);
                    xiaLogError("xiaUpdateUserParams", info_string, status);
                    return status;
                }
            }

            entry = entry->next;
        }

        break;

    case SET:

        detChanElem = xiaGetDetChanPtr(detChan);

        detChanSetElem = detChanElem->data.detChanSet;

        while (detChanSetElem != NULL) {

            status = xiaUpdateUserParams(detChanSetElem->channel);

            if (status != XIA_SUCCESS) {
                sprintf(info_string, "Error setting user params for detChan %u",
                        detChanSetElem->channel);
                xiaLogError("xiaUpdateUserParams", info_string, status);
                return status;
            }

            detChanSetElem = detChanSetElem->next;
        }

        break;

    case 999:
        status = XIA_INVALID_DETCHAN;
        xiaLogError("xiaUpdateUserParams", "detChan number is not in the list of valid values ", status);
        return status;
        break;
    default:

        status = XIA_UNKNOWN;
        xiaLogError("xiaUpdateUserParams", "Shouldn't be seeing this message", status);
        return status;
        break;
    }

    return XIA_SUCCESS;
}

/*
 * Performs product-specific special gain operations. value is
 * typically a double*, but theoretically could vary by name.
 */
HANDEL_EXPORT int HANDEL_API xiaGainOperation(int detChan, char *name, void *value)
{
    int status;
    int elemType;

    unsigned int modChan;

    char boardType[MAXITEM_LEN];

    char *boardAlias;
    char *detectorAlias;

    DetChanElement *detChanElem = NULL;

    DetChanSetElem *detChanSetElem = NULL;

    Detector *detector = NULL;

    Module *module = NULL;

    XiaDefaults *defaults = NULL;

    PSLFuncs localFuncs;

    elemType = xiaGetElemType(detChan);

    switch(elemType)
    {
    case SINGLE:
        status = xiaGetBoardType(detChan, boardType);

        if (status != XIA_SUCCESS)
        {
            sprintf(info_string, "Unable to get boardType for detChan %d", detChan);
            xiaLogError("xiaGainOperation", info_string, status);
            return status;
        }

        status = xiaLoadPSL(boardType, &localFuncs);

        if (status != XIA_SUCCESS)
        {
            sprintf(info_string, "Unable to load PSL funcs for detChan %d", detChan);
            xiaLogError("xiaGainOperation", info_string, status);
            return status;
        }

        defaults      		= xiaGetDefaultFromDetChan((unsigned int)detChan);
        boardAlias    		= xiaGetAliasFromDetChan(detChan);
        module        		= xiaFindModule(boardAlias);
        modChan       		= xiaGetModChan((unsigned int)detChan);
        detectorAlias 		= module->detector[modChan];
        detector      		= xiaFindDetector(detectorAlias);

        status = localFuncs.gainOperation(detChan, name, value, detector,
                                          modChan, module, defaults);

        if (status != XIA_SUCCESS)
        {
            sprintf(info_string, "Error performing the gain Operation for detChan %d", detChan);
            xiaLogError("xiaGainOperation", info_string, status);
            return status;
        }

        break;

    case SET:
        detChanElem = xiaGetDetChanPtr(detChan);

        detChanSetElem = detChanElem->data.detChanSet;

        while (detChanSetElem != NULL)
        {
            status = xiaGainOperation(detChanSetElem->channel, name, value);

            if (status != XIA_SUCCESS)
            {
                sprintf(info_string, "Error changing the gain for detChan %d", detChan);
                xiaLogError("xiaGainOperation", info_string, status);
                return status;
            }

            detChanSetElem = getListNext(detChanSetElem);
        }

        break;

    case 999:
        status = XIA_INVALID_DETCHAN;
        xiaLogError("xiaGainOperation", "detChan number is not in the list of valid values ", status);
        return status;
        break;
    default:
        status = XIA_UNKNOWN;
        xiaLogError("xiaGainOperation", "Should not be seeing this message", status);
        return status;
        break;
    }

    return XIA_SUCCESS;
}


/*
 * Scales the channel's energy value by a constant factor.
 */
HANDEL_EXPORT int HANDEL_API xiaGainCalibrate(int detChan, double deltaGain)
{
    int status;
    int elemType;

    unsigned int modChan;

    char boardType[MAXITEM_LEN];

    char *detectorAlias;
    char *boardAlias;

    Detector *detector = NULL;

    XiaDefaults *defaults = NULL;

    Module *module = NULL;

    DetChanElement *detChanElem = NULL;

    DetChanSetElem *detChanSetElem = NULL;

    PSLFuncs localFuncs;

    elemType = xiaGetElemType((unsigned int)detChan);

    switch(elemType)
    {
    case SINGLE:
        status = xiaGetBoardType(detChan, boardType);

        if (status != XIA_SUCCESS)
        {
            sprintf(info_string, "Unable to get boardType for detChan %d", detChan);
            xiaLogError("xiaGainCalibrate", info_string, status);
            return status;
        }

        status = xiaLoadPSL(boardType, &localFuncs);

        if (status != XIA_SUCCESS)
        {
            sprintf(info_string, "Unable to load PSL funcs for detChan %d", detChan);
            xiaLogError("xiaGainCalibrate", info_string, status);
            return status;
        }

        defaults      = xiaGetDefaultFromDetChan((unsigned int)detChan);
        boardAlias    = xiaGetAliasFromDetChan(detChan);
        module        = xiaFindModule(boardAlias);
        modChan       = xiaGetModChan((unsigned int)detChan);
        detectorAlias = module->detector[modChan];
        detector      = xiaFindDetector(detectorAlias);

        status = localFuncs.gainCalibrate(detChan, detector, modChan, module,
                                          defaults, deltaGain);

        if (status != XIA_SUCCESS)
        {
            sprintf(info_string, "Error calibrating the gain for detChan %d", detChan);
            xiaLogError("xiaGainCalibrate", info_string, status);
            return status;
        }

        break;

    case SET:
        detChanElem = xiaGetDetChanPtr((unsigned int)detChan);

        detChanSetElem = detChanElem->data.detChanSet;

        while (detChanSetElem != NULL)
        {
            status = xiaGainCalibrate((int)detChanSetElem->channel, deltaGain);

            if (status != XIA_SUCCESS)
            {
                sprintf(info_string, "Error calibrating the gain for detChan %d", detChan);
                xiaLogError("xiaGainCalibrate", info_string, status);
                return status;
            }

            detChanSetElem = getListNext(detChanSetElem);
        }

        break;

    case 999:
        status = XIA_INVALID_DETCHAN;
        xiaLogError("xiaGainCalibrate", "detChan number is not in the list of valid values ", status);
        return status;
        break;
    default:
        status = XIA_UNKNOWN;
        xiaLogError("xiaGainCalibrate", "Should not be seeing this message", status);
        return status;
        break;
    }

    return XIA_SUCCESS;
}

/*
 * Retrieves the value of the DSP parameter name from the specified
 * detChan.
 */
HANDEL_EXPORT int HANDEL_API xiaGetParameter(int detChan, const char *name, unsigned short *value)
{
    int status;
    int elemType;

    char boardType[MAXITEM_LEN];

    PSLFuncs localFuncs;


    elemType = xiaGetElemType((unsigned int)detChan);

    /* We only support SINGLE chans... */
    switch (elemType)
    {
    case SINGLE:
        status = xiaGetBoardType(detChan, boardType);

        if (status != XIA_SUCCESS)
        {
            sprintf(info_string, "Unable to get boardType for detChan %d", detChan);
            xiaLogError("xiaGetParameter", info_string, status);
            return status;
        }

        status = xiaLoadPSL(boardType, &localFuncs);

        if (status != XIA_SUCCESS)
        {
            sprintf(info_string, "Unable to load PSL funcs for detChan %d", detChan);
            xiaLogError("xiaGetParameter", info_string, status);
            return status;
        }

        status = localFuncs.getParameter(detChan, name, value);

        if (status != XIA_SUCCESS)
        {
            sprintf(info_string, "Error getting parameter %s from detChan %d", name, detChan);
            xiaLogError("xiaGetParameter", info_string, status);
            return status;
        }

        break;

    case SET:
        xiaLogError("xiaGetParameter", "detChan SETs are not supported for this routine", XIA_BAD_TYPE);
        return XIA_BAD_TYPE;
        break;

    case 999:
        status = XIA_INVALID_DETCHAN;
        xiaLogError("xiaGetParameter", "detChan number is not in the list of valid values ", status);
        return status;
        break;
    default:
        status = XIA_UNKNOWN;
        xiaLogError("xiaGetParameter", "Should not be seeing this message", status);
        return status;
        break;
    }

    return XIA_SUCCESS;
}


/*
 * Sets the value of DSP parameter name for detChan.
 */
HANDEL_EXPORT int HANDEL_API xiaSetParameter(int detChan, const char *name, unsigned short value)
{
    int status;
    int elemType;

    char boardType[MAXITEM_LEN];

    DetChanSetElem *detChanSetElem = NULL;

    DetChanElement *detChanElem = NULL;

    PSLFuncs localFuncs;


    elemType = xiaGetElemType((unsigned int)detChan);

    switch (elemType)
    {
    case SINGLE:
        status = xiaGetBoardType(detChan, boardType);

        if (status != XIA_SUCCESS)
        {
            sprintf(info_string, "Unable to get boardType for detChan %d", detChan);
            xiaLogError("xiaSetParameter", info_string, status);
            return status;
        }

        status = xiaLoadPSL(boardType, &localFuncs);

        if (status != XIA_SUCCESS)
        {
            sprintf(info_string, "Unable to load PSL funcs for detChan %d", detChan);
            xiaLogError("xiaSetParameter", info_string, status);
            return status;
        }

        status = localFuncs.setParameter(detChan, name, value);

        if (status != XIA_SUCCESS)
        {
            sprintf(info_string, "Error setting parameter %s from detChan %d", name, detChan);
            xiaLogError("xiaSetParameter", info_string, status);
            return status;
        }

        break;

    case SET:
        detChanElem = xiaGetDetChanPtr((unsigned int)detChan);

        detChanSetElem = detChanElem->data.detChanSet;

        while (detChanSetElem != NULL)
        {
            status = xiaSetParameter((int)detChanSetElem->channel, name, value);

            if (status != XIA_SUCCESS)
            {
                sprintf(info_string, "Error setting parameter %s for detChan %d", name, detChan);
                xiaLogError("xiaSetParameter", info_string, status);
                return status;
            }

            detChanSetElem = getListNext(detChanSetElem);
        }

        break;

    case 999:
        status = XIA_INVALID_DETCHAN;
        xiaLogError("xiaSetParameter", "detChan number is not in the list of valid values ", status);
        return status;
        break;
    default:
        status = XIA_UNKNOWN;
        xiaLogError("xiaSetParameter", "Should not be seeing this message", status);
        return status;
        break;
    }

    return XIA_SUCCESS;
}


/*
 * Returns the number of DSP parameters for the channel.
 */
HANDEL_EXPORT int HANDEL_API xiaGetNumParams(int detChan, unsigned short *value)
{
    int status;
    int elemType;

    char boardType[MAXITEM_LEN];

    PSLFuncs localFuncs;


    elemType = xiaGetElemType((unsigned int)detChan);

    /* We only support SINGLE chans... */
    switch (elemType)
    {
    case SINGLE:
        status = xiaGetBoardType(detChan, boardType);

        if (status != XIA_SUCCESS)
        {
            sprintf(info_string, "Unable to get boardType for detChan %d", detChan);
            xiaLogError("xiaGetNumParams", info_string, status);
            return status;
        }

        status = xiaLoadPSL(boardType, &localFuncs);

        if (status != XIA_SUCCESS)
        {
            sprintf(info_string, "Unable to load PSL funcs for detChan %d", detChan);
            xiaLogError("xiaGetNumParams", info_string, status);
            return status;
        }

        status = localFuncs.getNumParams(detChan, value);

        if (status != XIA_SUCCESS)
        {
            sprintf(info_string, "Error getting number of DSP params from detChan %d", detChan);
            xiaLogError("xiaGetNumParams", info_string, status);
            return status;
        }

        break;

    case SET:
        xiaLogError("xiaGetNumParams", "detChan SETs are not supported for this routine", XIA_BAD_TYPE);
        return XIA_BAD_TYPE;
        break;

    case 999:
        status = XIA_INVALID_DETCHAN;
        xiaLogError("xiaGetNumParams", "detChan number is not in the list of valid values ", status);
        return status;
        break;
    default:
        status = XIA_UNKNOWN;
        xiaLogError("xiaGetNumParams", "Should not be seeing this message", status);
        return status;
        break;
    }

    return XIA_SUCCESS;
}


/*
 * See if a string is only composed of upper case
 */
HANDEL_STATIC boolean_t HANDEL_API xiaIsUpperCase(char *string)
{
    size_t len, i;

    len = strlen(string);

    for (i = 0; i < len; i++) {

        if (!isupper(CTYPE_CHAR(string[i])) &&
                !isdigit(CTYPE_CHAR(string[i]))) {

            return FALSE_;
        }
    }

    return TRUE_;
}


/*
 * Returns DSP symbol names, DSP values, etc... Assumes that the
 * proper amount of memory has been allocated based on the number of
 * parameters and the required type of value.
 */
HANDEL_EXPORT int HANDEL_API xiaGetParamData(int detChan, char *name, void *value)
{
    int status;
    int elemType;

    char boardType[MAXITEM_LEN];

    PSLFuncs localFuncs;


    elemType = xiaGetElemType((unsigned int)detChan);

    /* We only support SINGLE chans... */
    switch (elemType)
    {
    case SINGLE:
        status = xiaGetBoardType(detChan, boardType);

        if (status != XIA_SUCCESS) {

            sprintf(info_string, "Unable to get boardType for detChan %d", detChan);
            xiaLogError("xiaGetParamData", info_string, status);
            return status;
        }

        status = xiaLoadPSL(boardType, &localFuncs);

        if (status != XIA_SUCCESS) {

            sprintf(info_string, "Unable to load PSL funcs for detChan %d", detChan);
            xiaLogError("xiaGetParamData", info_string, status);
            return status;
        }

        status = localFuncs.getParamData(detChan, name, value);

        if (status != XIA_SUCCESS) {

            sprintf(info_string, "Error getting DSP param data from detChan %d", detChan);
            xiaLogError("xiaGetParamData", info_string, status);
            return status;
        }

        break;

    case SET:
        xiaLogError("xiaGetParamData", "detChan SETs are not supported for this routine", XIA_BAD_TYPE);
        return XIA_BAD_TYPE;
        break;

    case 999:
        status = XIA_INVALID_DETCHAN;
        xiaLogError("xiaGetParamData", "detChan number is not in the list of valid values ", status);
        return status;
        break;
    default:
        status = XIA_UNKNOWN;
        xiaLogError("xiaGetParamData", "Should not be seeing this message", status);
        return status;
        break;
    }

    return XIA_SUCCESS;
}


/*
 * Returns the DSP symbol name located at the specified index in the
 * symbolname list. This routine exists since VB can't pass a string array into
 * a DLL and, therefore, VB users must get a single string at a time. Assumes
 * that name has the proper amount of memory allocated.
 */
HANDEL_EXPORT int HANDEL_API xiaGetParamName(int detChan, unsigned short index, char *name)
{
    int status;
    int elemType;

    char boardType[MAXITEM_LEN];

    PSLFuncs localFuncs;


    elemType = xiaGetElemType((unsigned int)detChan);

    /* We only support SINGLE chans... */
    switch (elemType)
    {
    case SINGLE:
        status = xiaGetBoardType(detChan, boardType);

        if (status != XIA_SUCCESS) {

            sprintf(info_string, "Unable to get boardType for detChan %d", detChan);
            xiaLogError("xiaGetParamName", info_string, status);
            return status;
        }

        status = xiaLoadPSL(boardType, &localFuncs);

        if (status != XIA_SUCCESS) {

            sprintf(info_string, "Unable to load PSL funcs for detChan %d", detChan);
            xiaLogError("xiaGetParamName", info_string, status);
            return status;
        }

        status = localFuncs.getParamName(detChan, index, name);

        if (status != XIA_SUCCESS) {

            sprintf(info_string, "Error getting DSP params from detChan %d", detChan);
            xiaLogError("xiaGetParamName", info_string, status);
            return status;
        }

        break;

    case SET:
        xiaLogError("xiaGetParamName", "detChan SETs are not supported for this routine", XIA_BAD_TYPE);
        return XIA_BAD_TYPE;
        break;

    case 999:
        status = XIA_INVALID_DETCHAN;
        xiaLogError("xiaGetParamName", "detChan number is not in the list of valid values ", status);
        return status;
        break;
    default:
        status = XIA_UNKNOWN;
        xiaLogError("xiaGetParamName", "Should not be seeing this message", status);
        return status;
        break;
    }

    return XIA_SUCCESS;
}
