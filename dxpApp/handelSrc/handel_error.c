/*
 * Copyright (c) 2002-2004 X-ray Instrumentation Associates
 *               2005-2020 XIA LLC
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


#include "handel_generic.h"
#include "handeldef.h"
#include "xia_handel.h"
#include "handel_errors.h"

#include "xerxes_errors.h"

const char* xiaGetXerxesErrorText(int errorcode);

/**
 * Returns the corresponding error text from given error code
 */
HANDEL_EXPORT const char* HANDEL_API xiaGetErrorText(int errorcode)
{
    if (errorcode >= XIA_XERXES_ERROR_BASE)
        return xiaGetXerxesErrorText(errorcode);

    switch(errorcode) {
        /* Success */
        case XIA_SUCCESS            : return "The routine succeeded"; break;

        /* IO level error codes 1-100 */
        case XIA_OPEN_FILE          : return "Error opening file"; break;
        case XIA_FILEERR            : return "File related error"; break;
        case XIA_NOSECTION          : return "Unable to find section in ini file"; break;
        case XIA_FORMAT_ERROR       : return "Syntax error in ini file"; break;
        case XIA_ILLEGAL_OPERATION  : return "Attempted to configure options in an illegal order"; break;
        case XIA_FILE_RA            : return "File random access unable to find name-value pair"; break;
        case XIA_SET_POS            : return "Error getting file position"; break;
        case XIA_READ_ONLY          : return "A write operation when read only"; break;
        case XIA_INVALID_VALUE      : return "The value is invalid"; break;
        case XIA_NOT_IDLE           : return "Handel is not idle so could not complete the request"; break;
        case XIA_NOT_ACTIVE         : return "Handel is idle so could not complete the request"; break;
        case XIA_THREAD_ERROR       : return "Thread related error"; break;
        case XIA_PROTOCOL_ERROR     : return "Protocol related error"; break;
        case XIA_ALREADY_OPEN       : return "Already open"; break;

        /* DSP/FIPPI level error codes 201-300  */
        case XIA_UNKNOWN_DECIMATION          : return "The decimation read from the hardware does not match a known value"; break;
        case XIA_SLOWLEN_OOR                 : return "Calculated SLOWLEN value is out-of-range"; break;
        case XIA_SLOWGAP_OOR                 : return "Calculated SLOWGAP value is out-of-range"; break;
        case XIA_SLOWFILTER_OOR              : return "Attempt to set the Peaking or Gap time s.t. P+G>31"; break;
        case XIA_FASTLEN_OOR                 : return "Calculated FASTLEN value is out-of-range"; break;
        case XIA_FASTGAP_OOR                 : return "Calculated FASTGAP value is out-of-range"; break;
        case XIA_FASTFILTER_OOR              : return "Attempt to set the Peaking or Gap time s.t. P+G>31"; break;
        case XIA_BASELINE_OOR                : return "Baseline filter length is out-of-range"; break;

        /* Configuration errors 301-400 */
        case XIA_INITIALIZE                  : return "Fatal error on initialization"; break;
        case XIA_NO_ALIAS                    : return "A module, detector, or firmware item with the given alias is not defined"; break;
        case XIA_ALIAS_EXISTS                : return "Trying to add a configuration item with existing alias"; break;
        case XIA_BAD_VALUE                   : return "Out of range or invalid input value"; break;
        case XIA_INFINITE_LOOP               : return "Infinite loop detected"; break;
        case XIA_BAD_NAME                    : return "Specified name is not valid"; break;
        case XIA_BAD_PTRR                    : return "Specified PTRR is not valid for this alias"; break;
        case XIA_ALIAS_SIZE                  : return "Alias name length exceeds #MAXALIAS_LEN"; break;
        case XIA_NO_MODULE                   : return "Must define at least one module before"; break;
        case XIA_BAD_INTERFACE               : return "The specified interface does not exist"; break;
        case XIA_NO_INTERFACE                : return "An interface must defined before more information is specified"; break;
        case XIA_WRONG_INTERFACE             : return "Specified information doesn't apply to this interface"; break;
        case XIA_NO_CHANNELS                 : return "Number of channels for this module is set to 0"; break;
        case XIA_BAD_CHANNEL                 : return "Specified channel index is invalid or out-of-range"; break;
        case XIA_NO_MODIFY                   : return "Specified name cannot be modified once set"; break;
        case XIA_INVALID_DETCHAN             : return "Specified detChan value is invalid"; break;
        case XIA_BAD_TYPE                    : return "The DetChanElement type specified is invalid"; break;
        case XIA_WRONG_TYPE                  : return "This routine only operates on detChans that are sets"; break;
        case XIA_UNKNOWN_BOARD               : return "Board type is unknown"; break;
        case XIA_NO_DETCHANS                 : return "No detChans are currently defined"; break;
        case XIA_NOT_FOUND                   : return "Unable to locate the Acquisition value requested"; break;
        case XIA_PTR_CHECK                   : return "Pointer is out of synch when it should be valid"; break;
        case XIA_LOOKING_PTRR                : return "FirmwareSet has a FDD file defined and this only works with PTRRs"; break;
        case XIA_NO_FILENAME                 : return "Requested filename information is set to NULL"; break;
        case XIA_BAD_INDEX                   : return "User specified an alias index that doesn't exist"; break;
        case XIA_NULL_ALIAS                  : return "Null alias passed into function"; break;
        case XIA_NULL_NAME                   : return "Null name passed into function"; break;
        case XIA_NULL_VALUE                  : return "Null value passed into function"; break;
        case XIA_NEEDS_BOARD_TYPE            : return "Module needs board_type"; break;
        case XIA_UNKNOWN_ITEM                : return "Unknown item"; break;
        case XIA_TYPE_REDIRECT               : return "Module type can not be redefined once set"; break;
        case XIA_NO_TMP_PATH                 : return "No FDD temporary path defined for this firmware"; break;
        case XIA_NULL_PATH                   : return "Specified path was NULL"; break;

        /* Config errors found by xiaStartSystem(). 350-400 */
        case XIA_FIRM_BOTH                   : return "A FirmwareSet may not contain both an FDD and seperate Firmware definitions"; break;
        case XIA_PTR_OVERLAP                 : return "Peaking time ranges in the Firmware definitions may not overlap"; break;
        case XIA_MISSING_FIRM                : return "Either the FiPPI or DSP file is missing from a Firmware element"; break;
        case XIA_MISSING_POL                 : return "A polarity value is missing from a Detector element"; break;
        case XIA_MISSING_GAIN                : return "A gain value is missing from a Detector element"; break;
        case XIA_MISSING_INTERFACE           : return "The interface this channel requires is missing"; break;
        case XIA_MISSING_ADDRESS             : return "The epp_address information is missing for this channel"; break;
        case XIA_INVALID_NUMCHANS            : return "The wrong number of channels are assigned to this module"; break;
        case XIA_INCOMPLETE_DEFAULTS         : return "Some of the required defaults are missing"; break;
        case XIA_BINS_OOR                    : return "There are too many or too few bins for this module type"; break;
        case XIA_MISSING_TYPE                : return "The type for the current detector is not specified properly"; break;
        case XIA_NO_MMU                      : return "No MMU defined and/or required for this module"; break;
        case XIA_NULL_FIRMWARE               : return "No firmware set defined"; break;
        case XIA_NO_FDD                      : return "No FDD defined in the firmware set"; break;

        /* Host machine error codes 401-500 */
        case XIA_NOMEM                       : return "Unable to allocate memory"; break;
        case XIA_XERXES_DEPRECATED           : return "XerXes returned an error. Check log error output"; break;
        case XIA_MD                          : return "MD layer returned an error"; break;
        case XIA_EOF                         : return "EOF encountered"; break;
        case XIA_BAD_FILE_READ               : return "File read failed"; break;
        case XIA_BAD_FILE_WRITE              : return "File write failed"; break;

        /* Miscellaneous errors 501-600 */
        case XIA_UNKNOWN                     : return "Unknown"; break;
        case XIA_LOG_LEVEL                   : return "Log level invalid"; break;
        case XIA_FILE_TYPE                   : return "Improper file type specified"; break;
        case XIA_END                         : return "There are no more instances of the name specified. Pos set to end"; break;
        case XIA_INVALID_STR                 : return "Invalid string format"; break;
        case XIA_UNIMPLEMENTED               : return "The routine is unimplemented in this version"; break;
        case XIA_PARAM_DEBUG_MISMATCH        : return "A parameter mismatch was found with XIA_PARAM_DEBUG enabled"; break;

        /* PSL errors 601-700 */
        case XIA_NOSUPPORT_FIRM              : return "The specified firmware is not supported by this board type"; break;
        case XIA_UNKNOWN_FIRM                : return "The specified firmware type is unknown"; break;
        case XIA_NOSUPPORT_VALUE             : return "The specified acquisition value is not supported"; break;
        case XIA_UNKNOWN_VALUE               : return "The specified acquisition value is unknown"; break;
        case XIA_PEAKINGTIME_OOR             : return "The specified peaking time is out-of-range for this product"; break;
        case XIA_NODEFINE_PTRR               : return "The specified peaking time does not have a PTRR associated with it"; break;
        case XIA_THRESH_OOR                  : return "The specified treshold is out-of-range"; break;
        case XIA_ERROR_CACHE                 : return "The data in the values cache is out-of-sync"; break;
        case XIA_GAIN_OOR                    : return "The specified gain is out-of-range for this produce"; break;
        case XIA_TIMEOUT                     : return "Timeout waiting for BUSY"; break;
        case XIA_BAD_SPECIAL                 : return "The specified special run is not supported for this module"; break;
        case XIA_TRACE_OOR                   : return "The specified value of tracewait (in ns) is out-of-range"; break;
        case XIA_DEFAULTS                    : return "The PSL layer encountered an error creating a Defaults element"; break;
        case XIA_BAD_FILTER                  : return "Error loading filter info from either a FDD file or the Firmware configuration"; break;
        case XIA_NO_REMOVE                   : return "Specified acquisition value is required for this product and can't be removed"; break;
        case XIA_NO_GAIN_FOUND               : return "Handel was unable to converge on a stable gain value"; break;
        case XIA_UNDEFINED_RUN_TYPE          : return "Handel does not recognize this run type"; break;
        case XIA_INTERNAL_BUFFER_OVERRUN     : return "Handel attempted to overrun an internal buffer boundry"; break;
        case XIA_EVENT_BUFFER_OVERRUN        : return "Handel attempted to overrun the event buffer boundry"; break;
        case XIA_BAD_DATA_LENGTH             : return "Handel was asked to set a Data length to zero for readout"; break;
        case XIA_NO_LINEAR_FIT               : return "Handel was unable to perform a linear fit to some data"; break;
        case XIA_MISSING_PTRR                : return "Required PTRR is missing"; break;
        case XIA_PARSE_DSP                   : return "Error parsing DSP"; break;
        case XIA_UDXPS                       : return "Udxps error"; break;
        case XIA_BIN_WIDTH                   : return "Specified bin width is out-of-range"; break;
        case XIA_NO_VGA                      : return "An attempt was made to set the gaindac on a board that doesn't have a VGA"; break;
        case XIA_TYPEVAL_OOR                 : return "Specified detector type value is out-of-range"; break;
        case XIA_LOW_LIMIT_OOR               : return "Specified low MCA limit is out-of-range"; break;
        case XIA_BPB_OOR                     : return "bytes_per_bin is out-of-range"; break;
        case XIA_FIP_OOR                     : return "Specified FiPPI is out-of-range"; break;
        case XIA_MISSING_PARAM               : return "Unable to find DSP parameter in list"; break;
        case XIA_OPEN_XW                     : return "Error opening a handle in the XW library"; break;
        case XIA_ADD_XW                      : return "Error adding to a handle in the XW library"; break;
        case XIA_WRITE_XW                    : return "Error writing out a handle in the XW library"; break;
        case XIA_VALUE_VERIFY                : return "Returned value inconsistent with sent value"; break;
        case XIA_POL_OOR                     : return "Specifed polarity is out-of-range"; break;
        case XIA_SCA_OOR                     : return "Specified SCA number is out-of-range"; break;
        case XIA_BIN_MISMATCH                : return "Specified SCA bin is either too high or too low"; break;
        case XIA_WIDTH_OOR                   : return "MCA bin width is out-of-range"; break;
        case XIA_UNKNOWN_PRESET              : return "Unknown PRESET run type specified"; break;
        case XIA_GAIN_TRIM_OOR               : return "Gain trim out-of-range"; break;
        case XIA_GENSET_MISMATCH             : return "Returned GENSET doesn't match the set GENSET"; break;
        case XIA_NUM_MCA_OOR                 : return "The specified number of MCA bins is out of range"; break;
        case XIA_PEAKINT_OOR                 : return "The specified value for PEAKINT is out of range"; break;
        case XIA_PEAKSAM_OOR                 : return "The specified value for PEAKSAM is out of range"; break;
        case XIA_MAXWIDTH_OOR                : return "The specified value for MAXWIDTH is out of range"; break;
        case XIA_NULL_TYPE                   : return "A NULL file type was specified"; break;
        case XIA_GAIN_SCALE                  : return "Gain scale factor is not valid"; break;
        case XIA_NULL_INFO                   : return "The specified info array is NULL"; break;
        case XIA_UNKNOWN_PARAM_DATA          : return "Unknown parameter data type"; break;
        case XIA_MAX_SCAS                    : return "The specified number of SCAs is more then the maximum allowed"; break;
        case XIA_UNKNOWN_BUFFER              : return "Requested buffer is unknown"; break;
        case XIA_NO_MAPPING                  : return "Mapping mode is currently not installed/enabled"; break;
        case XIA_CLRBUFFER_TIMEOUT           : return "Time out waiting for buffer to clear"; break;
        case XIA_UNKNOWN_PT_CTL              : return "Unknown mapping point control"; break;
        case XIA_CLOCK_SPEED                 : return "The hardware is reporting an invalid clock speed"; break;
        case XIA_BAD_DECIMATION              : return "Passed in decimation is invalid"; break;
        case XIA_BAD_SYNCH_RUN               : return "Specified value for synchronous run is bad"; break;
        case XIA_PRESET_VALUE_OOR            : return "Requested preset value is out-of-range"; break;
        case XIA_MEMORY_LENGTH               : return "Memory length is invalid"; break;
        case XIA_UNKNOWN_PREAMP_TYPE         : return "Preamp type is unknown"; break;
        case XIA_DAC_TARGET_OOR              : return "The specified DAC target is out of range"; break;
        case XIA_DAC_TOL_OOR                 : return "The specified DAC tolerance is out of range"; break;
        case XIA_BAD_TRIGGER                 : return "Specified trigger setting is invalid"; break;
        case XIA_EVENT_LEN_OOR               : return "The specified event length is out of range"; break;
        case XIA_PRE_BUF_LEN_OOR             : return "The specified pre-buffer length is out of range"; break;
        case XIA_HV_OOR                      : return "The specified high voltage value is out of range"; break;
        case XIA_PEAKMODE_OOR                : return "The specified peak mode is out of range"; break;
        case XIA_NOSUPPORTED_PREAMP_TYPE     : return "The specified preamp type is not supported by current firmware"; break;
        case XIA_ENERGYCOEF_OOR              : return "The calculated energy coefficient values are out of range"; break;
        case XIA_VETO_PULSE_STEP             : return "The specified step value is too large for the Alpha pulser veto pulse"; break;
        case XIA_TRIGOUTPUT_OOR              : return "The specified trigger signal output is out of range"; break;
        case XIA_LIVEOUTPUT_OOR              : return "The specified livetime signal output is out of range"; break;
        case XIA_UNKNOWN_MAPPING             : return "Unknown mapping mode value specified"; break;
        case XIA_UNKNOWN_LIST_MODE_VARIANT   : return "Illegal list mode variant"; break;
        case XIA_MALFORMED_LENGTH            : return "List mode upper length word is malformed"; break;
        case XIA_CLRBUFSIZE_LENGTH           : return "Clear Buffer Size length is too large"; break;
        case XIA_BAD_ELECTRODE_SIZE          : return "UltraLo electrode size is invalid"; break;
        case XIA_TILT_THRESHOLD_OOR          : return "Specified threshold is out-of-range"; break;
        case XIA_USB_BUSY                    : return "Direct USB command failed due to busy USB"; break;
        case XIA_MALFORMED_MM_RESPONSE       : return "UltraLo moisture meter response is malformed"; break;
        case XIA_MALFORMED_MM_STATUS         : return "UltraLo moisture meter status is invalid"; break;
        case XIA_MALFORMED_MM_VALUE          : return "UltraLo moisture meter value is invalid"; break;
        case XIA_NO_EVENTS                   : return "No events to retrieve from the event buffer"; break;
        case XIA_NOSUPPORT_RUNDATA           : return "The specified run data is not supported"; break;
        case XIA_PARAMETER_OOR               : return "The parameter passed in is out of range"; break;
        case XIA_PASSTHROUGH                 : return "UART passthrough command error"; break;
        case XIA_UNSUPPORTED                 : return "Feature unsupported by current hardware"; break;

        /* handel-sitoro errors 690-700 */
        case XIA_DAC_GAIN_OOR                : return "SiToro DAC gain is out of range"; break;
        case XIA_NO_SPECTRUM                 : return "No spectrum has been received"; break;

        /* XUP errors 701-800 */
        case XIA_XUP_VERSION                 : return "XUP version is not supported"; break;
        case XIA_CHKSUM                      : return "checksum mismatch in the XUP"; break;
        case XIA_SIZE_MISMATCH               : return "Size read from file is incorrect"; break;
        case XIA_NO_ACCESS                   : return "Specified access file isn't valid"; break;
        case XIA_N_FILTER_BAD                : return "The number of filter parameters in the FDD doesn't match the number requires for the hardware"; break;

        /* Additional PSL errors used in handel-sitoro 802-900 */
        case XIA_ACQ_OOR                     : return "Generic acquisition value conversion out of range"; break;
        case XIA_ENCODE                      : return "Base64 encoding error"; break;
        case XIA_DECODE                      : return "Base64 decoding error"; break;
        case XIA_BAD_PSL_ARGS                : return "Bad call arguments to PSL function"; break;

        default : return "Unknown error code";
    }

}

const char* xiaGetXerxesErrorText(int errorcode)
{

     switch(errorcode) {
        /* Success */
        case XIA_SUCCESS            : return "The routine succeeded"; break;

        /* I/O level error codes 4001-4100 */
        case DXP_MDOPEN             : return "Error opening port for communication"; break;
        case DXP_MDIO               : return "Device IO error"; break;
        case DXP_MDINITIALIZE       : return "Error configurting port during initialization"; break;
        case DXP_MDSIZE             : return "Incoming data packet length larger than requested"; break;
        case DXP_MDUNKNOWN          : return "Unknown IO function"; break;
        case DXP_MDCLOSE            : return "Error closing connection"; break;
        case DXP_MDINVALIDPRIORITY  : return "Invalid priority type"; break;
        case DXP_MDPRIORITY         : return "Error setting priority"; break;
        case DXP_MDINVALIDNAME      : return "Error parsing IO name"; break;
        case DXP_MD_TARGET_ADDR     : return "Invalid target address"; break;
        case DXP_OPEN_EPP           : return "Unable to open EPP port"; break;
        case DXP_BAD_IONAME         : return "Invalid io name format"; break;

        /* Saturn specific error codes */
        case DXP_WRITE_TSAR         : return "Error writing TSAR register"; break;
        case DXP_WRITE_CSR          : return "Error writing CSR register"; break;
        case DXP_WRITE_WORD         : return "Error writing single word"; break;
        case DXP_READ_WORD          : return "Error reading single word"; break;
        case DXP_WRITE_BLOCK        : return "Error writing block data"; break;
        case DXP_READ_BLOCK         : return "Error reading block data"; break;
        case DXP_READ_CSR           : return "Error reading CSR register"; break;
        case DXP_WRITE_FIPPI        : return "Error writing to FIPPI"; break;
        case DXP_WRITE_DSP          : return "Error writing to DSP"; break;
        case DXP_DSPSLEEP		    : return "Error with DSP Sleep"; break;
        case DXP_NOFIPPI			: return "No valid FiPPI defined"; break;
        case DXP_FPGADOWNLOAD       : return "Error downloading FPGA"; break;
        case DXP_DSPLOAD            : return "Error downloading DSP"; break;
        case DXP_DSPACCESS          : return "Specified DSP parameter is read-only"; break;
        case DXP_DSPPARAMBOUNDS     : return "DSP parameter value is out of range"; break;
        case DXP_NOCONTROLTYPE      : return "Unknown control task"; break;
        case DXP_CONTROL_TASK       : return "Control task parameter error"; break;

        /* microDXP errors */
        case DXP_STATUS_ERROR       : return "microDXP device status error"; break;
        case DXP_DSP_ERROR          : return "microDXP DSP status error"; break;
        case DXP_PIC_ERROR          : return "microDXP PIC status error"; break;
        case DXP_UDXP_RESPONSE      : return "microDXP response error"; break;
        case DXP_MISSING_ESC        : return "microDXP response is missing ESCAPE char"; break;

        /* DSP/FIPPI level error codes 4201-4300 */
        case DXP_DSPRUNERROR        : return "Error decoding run error"; break;
        case DXP_NOSYMBOL           : return "Unknown DSP parameter"; break;
        case DXP_TIMEOUT            : return "Time out waiting for DSP to be ready"; break;
        case DXP_DSP_RETRY          : return "DSP failed to download after multiple attempts"; break;
        case DXP_CHECKSUM           : return "Error verifing checksum"; break;
        case DXP_BAD_BIT            : return "Requested bit is out-of-range"; break;
        case DXP_RUNACTIVE		    : return "Run already active in module"; break;
        case DXP_INVALID_STRING     : return "Invalid memory string format"; break;
        case DXP_UNIMPLEMENTED      : return "Requested function is unimplemented"; break;
        case DXP_MEMORY_LENGTH      : return "Invalid memory length"; break;
        case DXP_MEMORY_BLK_SIZE    : return "Invalid memory block size"; break;
        case DXP_UNKNOWN_MEM        : return "Unknown memory type"; break;
        case DXP_UNKNOWN_FPGA       : return "Unknown FPGA type"; break;
        case DXP_FPGA_TIMEOUT       : return "Time out downloading FPGA"; break;
        case DXP_APPLY_STATUS       : return "Error applying change"; break;
        case DXP_INVALID_LENGTH     : return "Specified length is invalid"; break;
        case DXP_NO_SCA             : return "No SCAs defined for the specified channel"; break;
        case DXP_FPGA_CRC           : return "CRC error after FPGA downloaded"; break;
        case DXP_UNKNOWN_REG        : return "Unknown register"; break;
        case DXP_OPEN_FILE          : return "UNable to open firmware file"; break;
        case DXP_REWRITE_FAILURE    : return "Couldn't set parameter even after n iterations"; break;

        /* Xerxes onfiguration errors 4301-4400 */
        case DXP_BAD_SYSTEMITEM     : return "Invalid system item format"; break;
        case DXP_MAX_MODULES        : return "Too many modules specified"; break;
        case DXP_NODETCHAN          : return "Detector channel is unknown"; break;
        case DXP_NOIOCHAN           : return "IO Channel is unknown"; break;
        case DXP_NOMODCHAN          : return "Modchan is unknown"; break;
        case DXP_NEGBLOCKSIZE       : return "Negative block size unsupported"; break;
        case DXP_INITIALIZE	        : return "Initialization error"; break;
        case DXP_UNKNOWN_BTYPE      : return "Unknown board type"; break;
        case DXP_BADCHANNEL         : return "Invalid channel number"; break;
        case DXP_NULL               : return "Parameter cannot be NULL"; break;
        case DXP_MALFORMED_FILE     : return "Malformed firmware file"; break;
        case DXP_UNKNOWN_CT         : return "Unknown control task"; break;

        /* Host machine error codes 4401-4500 */
        case DXP_NOMEM             : return "Error allocating memory"; break;
        case DXP_WIN32_API         : return "Windows API error"; break;

        /* Misc error codes 4501-4600 */
        case DXP_LOG_LEVEL		  : return "Log level invalid"; break;

        default : return "Unknown XerXes error code";
    }
}

