/*<Thu May 23 11:38:03 2002--ALPHA_FRINK--0.0.6--Do not remove--XIA>*/

/*
 *  handel_errors.h
 *
 *  Created 3-17-00:  JW: File to contain Error codes for XIA drivers.
 *  Copied 6-25-01 JW: copied xerxes_errors.h to handel_errors.h
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


#ifndef HANDEL_ERROR_H
#define HANDEL_ERROR_H

#ifndef HANDELDEF_H
#include <handeldef.h>
#endif

/*
 *  some error codes
 */
#define XIA_SUCCESS			  0
/* I/O level error codes 1-100*/
#define XIA_OPEN_FILE			  1
#define XIA_FILEERR			  2
#define XIA_NOSECTION			  3
#define XIA_FORMAT_ERROR		  4
#define XIA_ILLEGAL_OPERATION		  5 /** Attempted to configure options in an illegal order */
#define XIA_FILE_RA			  6 /** File random access unable to find name-value pair */
/*  primitive level error codes (due to mdio failures) 101-200*/
/*  DSP/FIPPI level error codes 201-300  */
#define XIA_UNKNOWN_DECIMATION          201 /** The decimation read from the hardware does not match a known value */
#define XIA_SLOWLEN_OOR                 202 /** Calculated SLOWLEN value is out-of-range */
#define XIA_SLOWGAP_OOR                 203 /** Calculated SLOWGAP value is out-of-range */
#define XIA_SLOWFILTER_OOR              204 /** Attempt to set the Peaking or Gap time s.t. P+G>31 */
#define XIA_FASTLEN_OOR                 205 /** Calculated FASTLEN value is out-of-range */
#define XIA_FASTGAP_OOR                 206 /** Calculated FASTGAP value is out-of-range */
#define XIA_FASTFILTER_OOR              207 /** Attempt to set the Peaking or Gap time s.t. P+G>31 */

/*  configuration errors  301-400  */
#define XIA_INITIALIZE			301
#define XIA_NO_ALIAS			302
#define XIA_ALIAS_EXISTS		303
#define XIA_BAD_VALUE			304
#define XIA_INFINITE_LOOP		305
#define XIA_BAD_NAME			306 /** Specified name is not valid */
#define XIA_BAD_PTRR			307 /** Specified PTRR is not valid for this alias */
#define XIA_ALIAS_SIZE          308 /** Alias name has too many characters */
#define XIA_NO_MODULE			309 /** Must define at least one module before */
#define XIA_BAD_INTERFACE		310 /** The specified interface does not exist */
#define XIA_NO_INTERFACE		311 /** An interface must defined before more information is specified */
#define XIA_WRONG_INTERFACE		312 /** Specified information doesn't apply to this interface */
#define XIA_NO_CHANNELS			313 /** Number of channels for this module is set to 0 */
#define XIA_BAD_CHANNEL			314 /** Specified channel index is invalid or out-of-range */
#define XIA_NO_MODIFY			315 /** Specified name cannot be modified once set */
#define XIA_INVALID_DETCHAN		316 /** Specified detChan value is invalid */
#define XIA_BAD_TYPE			317 /** The DetChanElement type specified is invalid */
#define XIA_WRONG_TYPE			318 /** This routine only operates on detChans that are sets */
#define XIA_UNKNOWN_BOARD		319 /** Board type is unknown */
#define XIA_NO_DETCHANS			320 /** No detChans are currently defined */
#define XIA_NOT_FOUND           321 /** Unable to locate the Acquisition value requested */
#define XIA_PTR_CHECK           322 /** Pointer is out of synch when it should be valid */
#define XIA_LOOKING_PTRR        323 /** FirmwareSet has a FDD file defined and this only works with PTRRs */
#define XIA_NO_FILENAME         324 /** Requested filename information is set to NULL */
#define XIA_BAD_INDEX           325 /** User specified an alias index that doesn't exist */

/* Starting at 350 are codes used to represent config errors found by 
 * xiaStartSystem()
 */
#define XIA_FIRM_BOTH			350 /** A FirmwareSet may not contain both an FDD and seperate Firmware definitions */
#define XIA_PTR_OVERLAP			351 /** Peaking time ranges in the Firmware definitions may not overlap */
#define XIA_MISSING_FIRM		352 /** Either the FiPPI or DSP file is missing from a Firmware element */
#define XIA_MISSING_POL			353 /** A polarity value is missing from a Detector element */
#define XIA_MISSING_GAIN		354 /** A gain value is missing from a Detector element */
#define XIA_MISSING_INTERFACE		355 /** The interface this channel requires is missing */
#define XIA_MISSING_ADDRESS		356 /** The epp_address information is missing for this channel */
#define XIA_INVALID_NUMCHANS		357 /** The wrong number of channels are assigned to this module */
#define XIA_INCOMPLETE_DEFAULTS 	358 /** Some of the required defaults are missing */
#define XIA_BINS_OOR			359 /** There are too many or too few bins for this module type */
#define XIA_MISSING_TYPE		360 /** The type for the current detector is not specified properly */
#define XIA_NO_MMU              361 /** No MMU defined and/or required for this module */

/*  host machine errors codes:  401-500 */
#define XIA_NOMEM			401 /** Unable to allocate memory */
#define XIA_XERXES			402 /** XerXes returned an error */
#define XIA_MD				403 /** MD layer returned an error */
#define XIA_EOF				404 /** EOF encountered */
#define XIA_XERXES_NORMAL_RUN_ACTIVE    405 /** XerXes says a normal run is still active */
#define XIA_XERXES_CONTROL_RUN_ACTIVE   406 /** XerXes says a control run is still active */
#define XIA_HARDWARE_RUN_ACTIVE         406 /** The hardware says a control run is still active */

/* miscellaneous errors 501-600 */
#define XIA_UNKNOWN			501
#define XIA_LOG_LEVEL			502 /** Log level invalid */
#define XIA_NO_LIST			503 /** List size is zero */
#define XIA_NO_ELEM			504 /** No data to remove */
#define XIA_DATA_DUP			505 /** Data already in table */
#define XIA_REM_ERR			506 /** Unable to remove entry from hash table */
#define XIA_FILE_TYPE			507 /** Improper file type specified */
#define XIA_END				508 /** There are no more instances of the name specified. Pos set to end */

/* PSL error 601-700 */
#define XIA_NOSUPPORT_FIRM	601 /** The specified firmware is not supported by this board type */
#define XIA_UNKNOWN_FIRM	602 /** The specified firmware type is unknown */
#define XIA_NOSUPPORT_VALUE	603 /** The specified acquisition value is not supported */
#define XIA_UNKNOWN_VALUE       604 /** The specified acquisition value is unknown */
#define XIA_PEAKINGTIME_OOR	605 /** The specified peaking time is out-of-range for this product */
#define XIA_NODEFINE_PTRR	606 /** The specified peaking time does not have a PTRR associated with it */
#define XIA_THRESH_OOR		607 /** The specified treshold is out-of-range */
#define XIA_ERROR_CACHE		608 /** The data in the values cache is out-of-sync */
#define XIA_GAIN_OOR		609 /** The specified gain is out-of-range for this produce */
#define XIA_TIMEOUT		610 /** Timeout waiting for BUSY */
#define XIA_BAD_SPECIAL		611 /** The specified special run is not supported for this module */
#define XIA_TRACE_OOR		612 /** The specified value of tracewait (in ns) is out-of-range */
#define XIA_DEFAULTS		613 /** The PSL layer encountered an error creating a Defaults element */			
#define XIA_BAD_FILTER      614 /** Error loading filter info from either a FDD file or the Firmware configuration */
#define XIA_NO_REMOVE       615 /** Specified acquisition value is required for this product and can't be removed */
#define XIA_NO_GAIN_FOUND   616 /** Handel was unable to converge on a stable gain value */
#define XIA_UNDEFINED_RUN_TYPE      617 /** Handel does not recognize this run type */
#define XIA_INTERNAL_BUFFER_OVERRUN 618 /** Handel attempted to overrun an internal buffer boundry */ 
#define XIA_EVENT_BUFFER_OVERRUN    619 /** Handel attempted to overrun the event buffer boundry */ 
#define XIA_BAD_DATA_LENGTH 620 /** Handel was asked to set a Data length to zero for readout */
#define XIA_NO_LINEAR_FIT   621 /** Handel was unable to perform a linear fit to some data */

/* Debug support */
/*#define XIA_DEBUG		1001*/

#endif				/* Endif for HANDEL_ERRORS_H */

