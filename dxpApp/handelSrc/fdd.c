/*
 * Copyright (c) 2002-2004 X-ray Instrumentation Associates
 * Copyright (c) 2005-2014 XIA LLC
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


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>

#include "handel_errors.h"
#include "xia_fdd.h"

#include "xia_common.h"
#include "xia_assert.h"
#include "xia_file.h"

FDD_STATIC void fdd__StringChomp(char *str);

static char info_string[INFO_LEN],line[XIA_LINE_LEN],*token,*delim=" ,=\t\r\n";

static char *section = "$$$NEW SECTION$$$\n";


/*
 * Global initialization routine.  Should be called before performing get and/or
 * put routines to the database.
 */
FDD_EXPORT int FDD_API xiaFddInitialize(void)
{
    int status=XIA_SUCCESS;

    /* First thing is to load the utils structure.  This is needed
     * to call allocation of any memory in general as well as any
     * error reporting.  It is absolutely essential that this work
     * with no errors.
     */
    status=xiaFddInitLibrary();

    /* Initialize the data structures and pointers of the library */
    if (status != XIA_SUCCESS) {
        sprintf(info_string,"Unable to initialize FDD");
        printf("%s\n",info_string);
        return status;
    }

    return status;
}

/*
 * Global initialization routine.  Should be called before performing get and/or
 * put routines to the database.
 */
FDD_EXPORT int FDD_API xiaFddInitLibrary(void)
{
    int status=XIA_SUCCESS;

    Xia_Util_Functions util_funcs;

    /* Call the MD layer init function for the utility routines */
    dxp_md_init_util(&util_funcs, NULL);
    /* Now build up some information...slowly */
    fdd_md_alloc = util_funcs.dxp_md_alloc;
    fdd_md_free  = util_funcs.dxp_md_free;
    fdd_md_error = util_funcs.dxp_md_error;
    fdd_md_warning = util_funcs.dxp_md_warning;
    fdd_md_info  = util_funcs.dxp_md_info;
    fdd_md_debug = util_funcs.dxp_md_debug;
    fdd_md_output = util_funcs.dxp_md_output;
    fdd_md_log  = util_funcs.dxp_md_log;
    fdd_md_wait  = util_funcs.dxp_md_wait;
    fdd_md_puts  = util_funcs.dxp_md_puts;
    fdd_md_fgets  = util_funcs.dxp_md_fgets;
    fdd_md_path_separator = util_funcs.dxp_md_path_separator;
    fdd_md_tmp_path = util_funcs.dxp_md_tmp_path;

    return status;
}

/*
 * Routine to create a file(s) of firmware based on criteria specified by the
 * calling routine.
 *
 * currently recognized firmware types:
 *    fippi
 *    dsp
 *    system
 */
FDD_EXPORT int FDD_API xiaFddGetFirmware(const char *filename, char *path,
                                         const char *ftype,
                                         double pt, unsigned int nother,
                                         const char **others,
                                         const char *detectorType,
                                         char newfilename[],
                                         char rawFilename[])
/* const char *filename;     Input: name of the file that is the fdd        */
/* const char *ftype;      Input: firmware type to retrieve           */
/* unsigned short nother;     Input: number of elements in the array of other specifiers */
/* const char **others;      Input: array of stings containing firmware options    */
/* const char *detectorType    Input: detector type to be added to the keywords list       */
/* char newfilename[MAXFILENAME_LEN] Output: filename of the temporary firmware file     */
/* char rawFilename[MACFILENAME_LEN]   Output: filename for the raw filename. For ID purposes in PSL */
{
    int status=XIA_SUCCESS;
    size_t len;
    size_t completePathLen = 0;

    unsigned int i, j;

    unsigned short numFilter;

    /* Store the file pointer of the FDD file and the new temporary file */
    FILE *fp=NULL, *ofp=NULL;

    boolean_t exact   = FALSE_;
    boolean_t isFound = FALSE_;

    char relativeName[MAXFILENAME_LEN];
    char postTok[MAXFILENAME_LEN];

    char *cstatus      = NULL;
    char *start        = NULL;
    char *pathSep      = fdd_md_path_separator();
    char *completePath = NULL;

    char **keywords = NULL;


    if (path == NULL) {
        sprintf(info_string, "Temporary path may not be NULL for '%s'",
                filename);
        xiaFddLogError("xiaFddGetFirmware", info_string, XIA_NULL_PATH);
        return XIA_NULL_PATH;
    }

    /* Add the detector type to the keywords list here */
    keywords = (char **)fdd_md_alloc((nother + 1) * sizeof(char *));

    if (!keywords) {
        sprintf(info_string, "Unable to allocate %lu bytes for the keywords "
                "array.", (nother + 1) * sizeof(char *));
        xiaFddLogError("xiaFddGetFirmware", info_string, XIA_NOMEM);
        return XIA_NOMEM;
    }

    for (i = 0; i < nother; i++) {
        len = strlen(others[i]);
        keywords[i] = (char *)fdd_md_alloc((len + 1) * sizeof(char));

        if (!keywords[i]) {
            for (j = 0; j < i; j++) {
                fdd_md_free(keywords[j]);
            }

            fdd_md_free(keywords);

            sprintf(info_string, "Unable to allocate %zu bytes for the keywords[%u] "
                    "array.", len + 1, i);
            xiaFddLogError("xiaFddGetFirmware", info_string, XIA_NOMEM);
            return XIA_NOMEM;
        }

        strcpy(keywords[i], others[i]);
    }

    len = strlen(detectorType);
    keywords[nother] = (char *)fdd_md_alloc((len + 1) * sizeof(char));

    if (!keywords[nother]) {
        for (i = 0; i < nother; i++) {
            fdd_md_free(keywords[i]);
        }

        fdd_md_free(keywords);

        sprintf(info_string, "Unable to allocate %zu bytes for keywords[%u] "
                "array.", len + 1, nother);
        xiaFddLogError("xiaFddGetFirmware", info_string, XIA_NOMEM);
        return XIA_NOMEM;
    }

    strcpy(keywords[nother], detectorType);


    /* First find and open the FDD file */
    isFound = xiaFddFindFirmware(filename, ftype, pt, -1.0,
                                 (unsigned short)(nother + 1), keywords, "r",
                                 &fp, &exact, rawFilename);

    for (i = 0; i < (nother + 1); i++) {
        fdd_md_free(keywords[i]);
    }

    fdd_md_free(keywords);

    if (!isFound)  {
        sprintf(info_string,"Cannot find '%s' in '%s': pt = %f, det = '%s'",
                ftype, filename, pt, detectorType);
        xiaFddLogDebug("xiaFddGetFirmware", info_string);

        if (fp != NULL) {
            xia_file_close(fp);
        }

        return XIA_FILEERR;
    }

    /* Manipulate the rawFilename and just rip off the last name...
     * DANGER! DANGER! This only works for Windows. Must add some
     * sort of global to deal with OS-dependent separators...
     */
    start = strrchr(rawFilename, '\\');
    start++;
    strcpy(postTok, start);
    start = strtok(postTok, " \n");
    strcpy(relativeName, start);

    sprintf(info_string, "relativeName = %s", relativeName);
    xiaFddLogDebug("xiaFddGetFirmware", info_string);


    /* Located the location in the FDD file, now write out the temporary file */
    /* Open the new file */
    sprintf(newfilename, "xia%s", relativeName);

    sprintf(info_string, "newfilename = %s", newfilename);
    xiaFddLogDebug("xiaFddGetFirmware", info_string);

    /* Build a path to the temporary file and directory */
    completePathLen = strlen(path) + strlen(newfilename) + 1;

    if (path[strlen(path) - 1] != *pathSep) {
        completePathLen++;
    }

    /* This is a bit of a leaky abstraction. We want to encourage callers to
     * make newfilename as big as MAX_PATH_LEN.
     */
    ASSERT(completePathLen < MAX_PATH_LEN);

    completePath = fdd_md_alloc(completePathLen);

    if (!completePath) {
        sprintf(info_string, "Error allocating %zu bytes for 'completePath'",
                completePathLen);
        xiaFddLogError("xiaFddGetFirmware", info_string, XIA_NOMEM);
        xia_file_close(fp);
        return XIA_NOMEM;
    }

    strcpy(completePath, path);

    if (path[strlen(path) - 1] != *pathSep) {
        completePath = strcat(completePath, pathSep);
    }

    completePath = strcat(completePath, newfilename);

    strcpy(newfilename, completePath);

    ofp = xia_file_open(completePath, "w");

    if (ofp == NULL) {
        sprintf(info_string,"Error opening the temporary file: %s", completePath);
        xiaFddLogError("xiaFddGetFirmware", info_string, XIA_OPEN_FILE);
        xia_file_close(fp);
        fdd_md_free(completePath);
        return XIA_OPEN_FILE;
    }

    fdd_md_free(completePath);

    /* Need to skip past filter info */
    cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);
    numFilter = (unsigned short)strtol(line, NULL, 10);

    sprintf(info_string, "numFilter = %u", numFilter);
    xiaFddLogDebug("xiaFddGetFirmware", info_string);

    for (j = 0; j < numFilter; j++) {
        cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);
    }

    cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);
    while ((!STREQ(line, section)) && (cstatus!=NULL)) {
        fprintf(ofp, "%s", line);
        cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);
    }

    xia_file_close(ofp);
    xia_file_close(fp);

    return status;
}


/*
 * Find the requested firmware in the specified FDD file.
 */
FDD_STATIC boolean_t xiaFddFindFirmware(const char *filename, const char *ftype,
                                        double ptmin, double ptmax,
                                        unsigned int nother, char **others,
                                        const char *mode, FILE **fp,
                                        boolean_t *exact,
                                        char rawFilename[MAXFILENAME_LEN])
/* Returns TRUE_ if the file was found FALSE_ otherwise                */
/* const char *filename;     Input: name of the file that is the fdd        */
/* const char *ftype;      Input: firmware type to retrieve           */
/* double ptmin;        Input: min peaking time for this firmware       */
/* double ptmax;        Input: max peaking time for this firmware       */
/* unsigned short nother;     Input: number of elements in the array of other specifiers */
/* const char **others;      Input: array of stings contianing firmware options    */
/* const char *mode;       Input: what mode to open the FDD file        */
/* FILE **fp;         Output: pointer to the file, if found        */
/* boolean_t *exact;       Output: was this an exact match to the types?     */
{

    char *cstatus = NULL;

    boolean_t found = FALSE_;

    int status;

    unsigned int i;
    unsigned int j;
    unsigned int nmatch;

    /* local variable used to decrement the number of type matches */
    unsigned short nkey;

    /* Min and max ptime read from fdd */
    double fddmin;
    double fddmax;


    ASSERT(filename != NULL);

    *fp = xia_find_file(filename, mode);

    if (!*fp) {
        sprintf(info_string,"Error finding the FDD file: %s", filename);
        xiaFddLogError("xiaFddFindFirmware", info_string, XIA_OPEN_FILE);
        return FALSE_;
    }

    rewind(*fp);

    /* Skip past anything that doesn't equal the first section line. */
    do {
        cstatus = fdd_md_fgets(line, XIA_LINE_LEN, *fp);
    } while (!STRNEQ(line, section));

    /* Skip over the original filename entry */
    cstatus = fdd_md_fgets(rawFilename, XIA_LINE_LEN, *fp);

    fdd__StringChomp(rawFilename);

    sprintf(info_string, "rawFilename = %s", rawFilename);
    xiaFddLogDebug("xiaFddFindFirmware", info_string);

    /* Located the FDD file, now start the search */
    cstatus = fdd_md_fgets(line, XIA_LINE_LEN, *fp);

    while ((!found)&&(cstatus!=NULL)) {
        /* Second line contains the firmware type, strip off delimiters */
        token = strtok(line, delim);

        if (STREQ(token, ftype)) {
            sprintf(info_string, "Matched token: '%s'", token);
            xiaFddLogDebug("xiaFddFindFirmware", info_string);

            /* Read in the number of keywords */
            cstatus = fdd_md_fgets(line, XIA_LINE_LEN, *fp);
            nkey = (unsigned short) strtol(line, NULL, 10);

            /* Reset the number of matches */
            nmatch = nother;
            for (i=0; i < nkey; i++) {
                /* Read in the keywords and check for matches */
                cstatus = fdd_md_fgets(line, XIA_LINE_LEN, *fp);
                /* Strip off spaces and endline characters */
                token = strtok(line, delim);
                for (j = 0; j < nother; j++) {
                    if (STREQ(token,others[j])) {
                        nmatch--;
                        break;
                    }
                }
                if (nmatch==0) {
                    found = TRUE_;
                    if (nkey == nother) *exact = TRUE_;
                    break;
                }
            }
            /* case of no other keywords */
            if (nkey == 0) {
                found = TRUE_;
                *exact = TRUE_;
            }
        } else {
            /* Read in the number of keywords */
            cstatus = fdd_md_fgets(line, XIA_LINE_LEN, *fp);
            nkey = (unsigned short) strtol(line, NULL, 10);
            /* Step past the other keywords line */
            for (i=0; i < nkey; i++) {
                cstatus = fdd_md_fgets(line, XIA_LINE_LEN, *fp);
            }
        }
        /* If we found a keyword match, proceed to check the peaking times */
        if (found) {
            /* Reset found to false for the next comparison */
            found = FALSE_;
            /* Found the type, now match the peaking time, read min and max from fdd */
            cstatus = fdd_md_fgets(line, XIA_LINE_LEN, *fp);
            fddmin = strtod(line, NULL);
            cstatus = fdd_md_fgets(line, XIA_LINE_LEN, *fp);
            fddmax = strtod(line, NULL);
            if (ptmax < 0.0) {
                /* Case where we are locating a firmware file for download, only need to match the range */
                if ((ptmin > fddmin) && (ptmin <= fddmax)) {
                    found = TRUE_;
                }
            } else {
                /* check for overlap of the peaking time ranges */
                if (!((ptmin >= fddmax) || (ptmax <= fddmin))) {
                    /* Overlap with the current entry */
                    found = TRUE_;
                    status = XIA_UNKNOWN;
                    sprintf(info_string,"Peaking time and keyword overlap with member of FDD");
                    xiaFddLogError("xiaFddFindFirmware",info_string,status);
                    xia_file_close(*fp);
                    return found;
                }
            }
        }
        /* If no match, jump to the next section */
        while ((!found)&&(cstatus!=NULL)) {
            cstatus = fdd_md_fgets(line, XIA_LINE_LEN, *fp);
            if (STREQ(line,section)) {
                cstatus = fdd_md_fgets(rawFilename, XIA_LINE_LEN, *fp);

                fdd__StringChomp(rawFilename);

                sprintf(info_string, "rawFilename = %s", rawFilename);
                xiaFddLogDebug("xiaFddFindFirmware", info_string);

                cstatus = fdd_md_fgets(line, XIA_LINE_LEN, *fp);
                break;
            }
        }
    }

    return found;
}


/*
 * Returns the number of filter parameters that are present for a given
 * peaking time and keywords.
 */
FDD_EXPORT int FDD_API xiaFddGetNumFilter(const char *filename,
                                          double peakingTime,
                                          unsigned int nKey,
                                          const char **keywords,
                                          unsigned short *numFilter)
{
    int status;

    unsigned int numToMatch;

    unsigned short numKeys;
    unsigned short i;
    unsigned short j;

    double ptMin;
    double ptMax;

    char *cstatus = NULL;

    boolean_t isFound = FALSE_;

    FILE *fp = NULL;


    if (filename == NULL) {
        status = XIA_FILEERR;
        xiaFddLogError("xiaFddGetNumFilter", "Must specify a non-NULL FDD filename", status);
        return status;
    }

    /* Open the file */
    fp = xia_find_file(filename, "r");
    if (fp == NULL) {
        status = XIA_OPEN_FILE;
        sprintf(info_string, "Error finding the FDD file: %s", filename);
        xiaFddLogError("xiaFddGetNumFilter", info_string, status);
        return status;
    }


    /* Start the hard-parsing here. Wish that we could make this into a
     * little more robust of an engine so that multiple FDD routines
     * could share it...
     */

    /* 1) Read in $$$NEW SECTION$$$ line */
    cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);

    /* 2) Read in raw filename */
    cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);

    /* 3) Read in type */
    cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);

    /* If this file is not of type "fippi", then we can fo ahead and skip ahead to
     * the next $$$NEW SECTION$$$. Do this until we match the proper peaking time
     * and keywords.
     */
    while ((!isFound) &&
            (cstatus != NULL)) {
        token = strtok(line, delim);

        if (STREQ(token, "fippi") || STREQ(token, "fippi_a")) {
            sprintf(info_string, "token = %s", token);
            xiaFddLogDebug("xiaFddGetNumFilter", info_string);

            cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);

            numKeys = (unsigned short)strtol(line, NULL, 10);

            sprintf(info_string, "numKeys = %u", numKeys);
            xiaFddLogDebug("xiaFddGetNumFilter", info_string);

            numToMatch = nKey;
            for (i = 0; i < numKeys; i++) {
                cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);

                token = strtok(line, delim);
                for (j = 0; j < nKey; j++) {
                    if (STREQ(token, keywords[j])) {
                        xiaFddLogDebug("xiaFddGetNumFilter", "Matched a keyword");

                        numToMatch--;
                        break;
                    }
                }

                if (numToMatch == 0) {
                    xiaFddLogDebug("xiaFddGetNumFilter", "Matched all keywords");

                    isFound = TRUE_;
                    break;
                }
            }

            /* In case there were no keywords in the first place...Is this possible? */
            if (nKey == 0) {
                isFound = TRUE_;
            }

        } else {

            /* Skip over keywords here since this isn't a "fippi" and we don't really
             * care about it. Remember that the line position (in this branch only)
             * is still sitting right after the type variable...
             */
            cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);

            numKeys = (unsigned short)strtol(line, NULL, 10);
            for (i = 0; i < numKeys; i++) {
                cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);
            }
        }

        if (isFound) {
            isFound = FALSE_;

            cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);
            ptMin = strtod(line, NULL);
            cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);
            ptMax = strtod(line, NULL);

            sprintf(info_string, "ptMin = %.3f, ptMax = %.3f", ptMin, ptMax);
            xiaFddLogDebug("xiaFddGetNumFilter", info_string);

            if ((peakingTime > ptMin) &&
                    (peakingTime <= ptMax)) {
                isFound = TRUE_;

                cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);
                *numFilter = (unsigned short)strtol(line, NULL, 10);

                sprintf(info_string, "numFilter = %u\n", *numFilter);
                xiaFddLogDebug("xiaFddGetNumFilter", info_string);

            }
        }

        while ((!isFound) &&
                (cstatus != NULL)) {
            cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);
            if (STREQ(line, section)) {
                /* This should be the raw filename */
                cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);

                /* This should be the type...so start all over again */
                cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);
                break;
            }
        }
    }

    xia_file_close(fp);

    return XIA_SUCCESS;
}


/*
 * This routine returns a list of values for the filters in filterInfo.
 * It is the responsibility of the calling routine to allocate the
 * right amount of memory for filterInfo, preferably using the size
 * returned from xiaFddGetNumFilter().
 */
FDD_EXPORT int FDD_API xiaFddGetFilterInfo(const char *filename, double peakingTime, unsigned int nKey,
                                           const char **keywords, double *ptMin, double *ptMax, parameter_t *filterInfo)
{
    int status;

    unsigned int numToMatch;

    unsigned short numKeys;
    unsigned short i;
    unsigned short j;
    unsigned short k;
    unsigned short numFilter;

    char *cstatus = NULL;

    boolean_t isFound = FALSE_;

    FILE *fp = NULL;


    if (filename == NULL) {

        status = XIA_FILEERR;
        xiaFddLogError("xiaFddGetFilterInfo", "Must specify a non-NULL FDD filename", status);
        return status;
    }

    fp = xia_find_file(filename, "r");

    if (fp == NULL) {
        status = XIA_OPEN_FILE;
        sprintf(info_string, "Error finding the FDD file: %s", filename);
        xiaFddLogError("xiaFddGetFilterInfo", info_string, status);
        return status;
    }

    /* Start the hard-parsing here. Wish that we could make this into a
     * little more robust of an engine so that multiple FDD routines
     * could share it...
     */

    /* 1) Read in $$$NEW SECTION$$$ line */
    cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);

    /* 2) Read in raw filename */
    cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);

    /* 3) Read in type */
    cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);

    /* If this file is not of type "fippi", then we can fo ahead and skip ahead to
     * the next $$$NEW SECTION$$$. Do this until we match the proper peaking time
     * and keywords.
     */
    while ((!isFound) &&
            (cstatus != NULL)) {
        token = strtok(line, delim);

        if (STREQ(token, "fippi") || STREQ(token, "fippi_a")) {
            sprintf(info_string, "token = %s", token);
            xiaFddLogDebug("xiaFddGetFilterInfo", info_string);

            cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);

            numKeys = (unsigned short)strtol(line, NULL, 10);

            sprintf(info_string, "numKeys = %u", numKeys);
            xiaFddLogDebug("xiaFddGetFilterInfo", info_string);

            numToMatch = nKey;
            for (i = 0; i < numKeys; i++) {
                cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);

                token = strtok(line, delim);
                for (j = 0; j < nKey; j++) {
                    if (STREQ(token, keywords[j])) {
                        xiaFddLogDebug("xiaFddGetFilterInfo", "Matched a keyword");

                        numToMatch--;
                        break;
                    }
                }

                if (numToMatch == 0) {
                    xiaFddLogDebug("xiaFddGetFilterInfo", "Matched all keywords");

                    isFound = TRUE_;
                    break;
                }
            }

            /* In case there were no keywords in the first place...Is this possible? */
            if (nKey == 0) {
                isFound = TRUE_;
            }

        } else {

            /* Skip over keywords here since this isn't a "fippi" and we don't really
             * care about it. Remember that the line position (in this branch only)
             * is still sitting right after the type variable...
             */
            cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);

            numKeys = (unsigned short)strtol(line, NULL, 10);
            for (i = 0; i < numKeys; i++) {
                cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);
            }
        }

        if (isFound) {
            isFound = FALSE_;

            cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);
            *ptMin = strtod(line, NULL);
            cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);
            *ptMax = strtod(line, NULL);

            sprintf(info_string, "ptMin = %.3f, ptMax = %.3f", *ptMin, *ptMax);
            xiaFddLogDebug("xiaFddGetFilterInfo", info_string);

            if ((peakingTime > *ptMin) &&
                    (peakingTime <= *ptMax)) {
                isFound = TRUE_;

                cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);
                numFilter = (unsigned short)strtol(line, NULL, 10);

                sprintf(info_string, "numFilter = %u\n", numFilter);
                xiaFddLogDebug("xiaFddGetFilterInfo", info_string);

                for (k = 0; k < numFilter; k++) {
                    cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);
                    filterInfo[k] = (parameter_t)strtol(line, NULL, 10);
                }

            }
        }

        while ((!isFound) &&
                (cstatus != NULL)) {
            cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);
            if (STREQ(line, section)) {
                /* This should be the raw filename */
                cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);

                /* This should be the type...so start all over again */
                cstatus = fdd_md_fgets(line, XIA_LINE_LEN, fp);
                break;
            }
        }
    }

    xia_file_close(fp);

    return XIA_SUCCESS;
}


/*
 * Attempts to remove any of the standard combinations of EOL
 * characters in existence.
 *
 * Specifically, this routine will remove \\n, \\r\\n and \\r.
 *
 * Modifies str in place since the chomped length is guaranteed to be
 * less then or equal to the original length.
 */
FDD_STATIC void fdd__StringChomp(char *str)
{
    size_t len = 0;

    char *chomped = NULL;


    ASSERT(str != NULL);


    len = strlen(str);
    chomped = fdd_md_alloc(len + 1);
    ASSERT(chomped != NULL);

    strcpy(chomped, str);

    if (chomped[len - 1] == '\n') {
        chomped[len - 1] = '\0';

        if (chomped[len - 2] == '\r') {
            chomped[len - 2] = '\0';
        }

    } else if (chomped[len - 1] == '\r') {
        chomped[len - 1] = '\0';
    }

    strcpy(str, chomped);
    fdd_md_free(chomped);
}


/*
 * Get the required firmware by calling xiaFddGetFirmware
 * Update FirmwareSet structure with the returned firmware information.
 */
FDD_EXPORT int FDD_API xiaFddGetAndCacheFirmware(FirmwareSet *fs,
                                                 const char *ftype, double pt, char *detType,
                                                 char *file, char *rawFile)
{
    int status;
    size_t len;

    unsigned short numFilter;

    char *tmpPath = NULL;
    parameter_t *filterInfo = NULL;

    Firmware *f = NULL;
    Firmware *current = NULL;

    double ptMin, ptMax;

    ASSERT(fs != NULL);
    ASSERT(detType != NULL);
    ASSERT(file != NULL);
    ASSERT(ftype != NULL);

    if (fs->filename == NULL) {
        xiaFddLogInfo("xiaFddGetAndCacheFirmware",
                      "No FDD defined in the firmware set.");
        return XIA_NO_FDD;
    }

    sprintf(info_string, "Getting firmware type %s from '%s'", ftype,
            fs->filename);
    xiaFddLogDebug("xiaFddGetAndCacheFirmware", info_string);

    if (fs->tmpPath) {
        tmpPath = fs->tmpPath;
    } else {
        tmpPath = fdd_md_tmp_path();
    }

    current = fs->firmware;

    while (current != NULL) {
        if ((pt >= current->min_ptime) && (pt <= current->max_ptime)) {
            break;
        }
        current = current->next;
    }

    if (current != NULL) {
        memset(file, '\0', sizeof(*file));
        sprintf(info_string, "Found firmware with ptMin = %0.2f "
                "ptMax = %0.2f for peaking time %0.2f", current->min_ptime,
                current->max_ptime, pt);
        xiaFddLogDebug("xiaFddGetAndCacheFirmware", info_string);
        if (STREQ(ftype, "system_fippi") && current->fippi != NULL) {
            strncpy(file, current->fippi, strlen(current->fippi) + 1);
        } else if (STREQ(ftype, "system_fpga") && current->system_fpga != NULL) {
            strncpy(file, current->system_fpga, strlen(current->system_fpga) + 1);
        } else if ((STREQ(ftype, "fippi") || STREQ(ftype, "fippi_a"))
                   && current->user_fippi != NULL) {
            strncpy(file, current->user_fippi, strlen(current->user_fippi) + 1);
        } else if (STREQ(ftype, "dsp") && current->user_dsp != NULL) {
            strncpy(file, current->user_dsp, strlen(current->user_dsp) + 1);
        } else if (STREQ(ftype, "system_dsp") && current->dsp != NULL) {
            strncpy(file, current->dsp, strlen(current->dsp) + 1);
        }

        /* Actual filename is offset from raw path with "\xia", then prefixed with "\" */
        if (strlen(file) > 0) {
            strcpy(rawFile, file + strlen(tmpPath) + 2);
            rawFile[0] = '\\';
            sprintf(info_string, "Current %s is: %s, rawFile: %s", ftype, file, rawFile);
            xiaFddLogDebug("xiaFddGetAndCacheFirmware", info_string);
            return XIA_SUCCESS;
        }

        /* Firmware information hasn't been filled yet */
        sprintf(info_string, "Firmware type %s not defined for found firmware.",
                ftype);
        xiaFddLogDebug("xiaFddGetAndCacheFirmware", info_string);
    }

    status = xiaFddGetFirmware(fs->filename, tmpPath, ftype, pt,
                               (unsigned int)fs->numKeywords,
                               (const char **)fs->keywords, detType, file, rawFile);

    /* Do not log an error if firmware file is not found */
    if (status == XIA_FILEERR) {
        sprintf(info_string, "Unable to locate firmware type %s from '%s'",
                ftype, fs->filename);
        xiaFddLogInfo("xiaFddGetAndCacheFirmware", info_string);
        return status;
    } else if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting firmware type %s from '%s'",
                ftype, fs->filename);
        xiaFddLogError("xiaFddGetAndCacheFirmware", info_string, status);
        return status;
    }

    /* Add this firmware to FirmwareSet if no match */
    if (current == NULL) {
        xiaFddLogDebug("xiaFddGetAndCacheFirmware", "Adding current firmware to FirmwareSet.");
        current = (Firmware *) fdd_md_alloc(sizeof(Firmware));

        current->user_dsp     = NULL;
        current->dsp          = NULL;
        current->fippi        = NULL;
        current->user_fippi   = NULL;
        current->system_fpga  = NULL;
        current->next          = NULL;

        if (fs->firmware == NULL) {
            fs->firmware = current;
            current->prev = NULL;
        } else {
            f = fs->firmware;
            while (f->next != NULL) {
                f = f->next;
            }
            f->next = current;
            current->prev = f;
        }

        status = xiaFddGetNumFilter(fs->filename, pt, (unsigned int) fs->numKeywords,
                                    (const char **)fs->keywords, &numFilter);

        if (status != XIA_SUCCESS) {
            xiaFddLogError("xiaFddGetAndCacheFirmware", "Error getting number of filter params", status);
            return status;
        }

        ASSERT(numFilter > 0);
        filterInfo = (parameter_t *)fdd_md_alloc(numFilter * sizeof(parameter_t));

        status = xiaFddGetFilterInfo(fs->filename, pt, fs->numKeywords,
                                     (const char **)fs->keywords, &ptMin, &ptMax, filterInfo);

        if (status != XIA_SUCCESS) {
            fdd_md_free(filterInfo);
            xiaFddLogError("xiaFddGetAndCacheFirmware", "Error getting filter information from FDD", status);
            return status;
        }

        current->min_ptime = ptMin;
        current->max_ptime = ptMax;
        current->numFilter = numFilter;
        current->filterInfo = filterInfo;
    }

    sprintf(info_string, "Setting file name %s for firmware type %s",
            file, ftype);
    xiaFddLogDebug("xiaFddGetAndCacheFirmware", info_string);

    len = strlen(file) + 1;

    if (STREQ(ftype, "system_fippi")) {
        current->fippi = (char *)fdd_md_alloc(len * sizeof(char));
        strncpy(current->fippi, file, len);
    } else if (STREQ(ftype, "system_fpga")) {
        current->system_fpga = (char *)fdd_md_alloc(len * sizeof(char));
        strncpy(current->system_fpga, file, len);
    }  else if ((STREQ(ftype, "fippi") || STREQ(ftype, "fippi_a"))) {
        current->user_fippi = (char *)fdd_md_alloc(len * sizeof(char));
        strncpy(current->user_fippi, file, len);
    } else if (STREQ(ftype, "dsp")) {
        current->user_dsp = (char *)fdd_md_alloc(len * sizeof(char));
        strncpy(current->user_dsp, file, len);
    } else if (STREQ(ftype, "system_dsp")) {
        current->dsp = (char *)fdd_md_alloc(len * sizeof(char));
        strncpy(current->dsp, file, len);
    } else {
        xiaFddLogError("xiaFddGetAndCacheFirmware",
                       "The required firmware type is not supported.", XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    return XIA_SUCCESS;
}
