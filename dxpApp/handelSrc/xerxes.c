/*
 * Low-level interface for controlling XIA products.
 *
 * Xerxes is the library that Handel is currently built upon. It contains
 * lower-level primitives for accessing the hardware. Xerxes' original
 * design was to provide access to the hardware with very few bells and
 * whistles; Handel's job is to turn the low-level primitives into something
 * useful.
 */

/*
 * Copyright (c) 2002-2004 X-ray Instrumentation Associates
 * Copyright (c) 2005-2017 XIA LLC
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

/* General DXP information */
#include "md_generic.h"
#include "xerxes_structures.h"
#include "xia_xerxes_structures.h"
#include "xia_xerxes.h"
#include "xerxes_errors.h"


#include "xia_assert.h"
#include "xia_version.h"
#include "xia_common.h"
#include "xia_file.h"


/* Private routines */
XERXES_STATIC int dxp_add_btype_library(Board_Info *current);
XERXES_STATIC int dxp_init_iface_ds(void);
XERXES_STATIC int dxp_init_dsp_ds(void);
XERXES_STATIC int dxp_init_fippi_ds(void);
XERXES_STATIC int dxp_free_board(Board *board);
XERXES_STATIC int dxp_free_iface(Interface *iface);
XERXES_STATIC int dxp_free_dsp(Dsp_Info *dsp);
XERXES_STATIC int dxp_free_fippi(Fippi_Info *fippi);
XERXES_STATIC int dxp_free_params(Dsp_Params *params);
XERXES_STATIC int dxp_free_binfo(Board_Info *binfo);
XERXES_STATIC int dxp_add_fippi(char *, Board_Info *, Fippi_Info **);
XERXES_STATIC int dxp_add_dsp(char *, Board_Info *, Dsp_Info **);
XERXES_STATIC int dxp_add_iface(char *dllname, char *iolib, Interface **iface);
XERXES_STATIC int dxp_do_readout(Board *board, int *modchan,
                                 unsigned short params[],
                                 unsigned long baseline[],
                                 unsigned long spectrum[]);
XERXES_STATIC int dxp_parse_memory_str(char *name, char *type, unsigned long *base, unsigned long *offset);
XERXES_STATIC int dxp_fipconfig(void);


/* Shorthand notation telling routines to act on all channels of the DXP (-1 currently). */
static int allChan=ALLCHAN;

static int numDxpMod=0;
static char info_string[INFO_LEN];

/*
 *  Head of Linked list of Interfaces
 */
static Interface *iface_head = NULL;

/*
 *  Structure to contain the board types in the system
 */
static Board_Info *btypes_head = NULL;

/*
 *	Define the Head of the linked list of items in the system
 */
static Board *system_head = NULL;

/*
 *	Define the head of the DSP linked list
 */
static Dsp_Info *dsp_head = NULL;

/*
 *	Define the head of the FIPPI linked list
 */
static Fippi_Info *fippi_head = NULL;

/*
 * Define global static variables to hold the information
 * used to assign new board information.
 */
static Board_Info        *working_btype        = NULL;
static Board             *working_board        = NULL;
static Interface         *working_iface        = NULL;

/*
 * Routines to perform global initialization functions.  Read in configuration
 * files, download data to all modules, etc...
 */

/*
 * Routine to initial data structures and pointers for the XerXes
 * library.
 */
XERXES_EXPORT int XERXES_API dxp_init_library(void)
{
    int status=DXP_SUCCESS;


    /* First thing is to load the utils structure.  This is needed
     * to call allocation of any memory in general as well as any
     * error reporting.  It is absolutely essential that this work
     * with no errors.  */

    /* Install the utility functions of the XerXes Library */
    if((status=dxp_install_utils("INIT_LIBRARY"))!=DXP_SUCCESS) {
        sprintf(info_string,"Unable to install utilities in XerXes");
        printf("%s\n",info_string);
        return status;
    }

    /* Initialize the global system */
    if((status=dxp_init_ds())!=DXP_SUCCESS) {
        sprintf(info_string,"Unable to initialize data structures.");
        dxp_log_error("dxp_init_library",info_string,status);
        return status;
    }

    return status;
}

/*
 * Routine to install pointers to the machine dependent
 * utility routines.
 */
XERXES_EXPORT int XERXES_API dxp_install_utils(const char *utilname)
{
    Xia_Util_Functions util_funcs;
    char lstr[133];

    if (utilname != NULL) {
        strncpy(lstr, utilname, XIA_LINE_LEN);
        lstr[strlen(utilname)] = '\0';
    } else {
        strcpy(lstr, "NULL");
    }

    /* Call the MD layer init function for the utility routines */
    dxp_md_init_util(&util_funcs, lstr);

    /* Now build up some information...slowly */
    xerxes_md_alloc          = util_funcs.dxp_md_alloc;
    xerxes_md_free           = util_funcs.dxp_md_free;
    xerxes_md_error          = util_funcs.dxp_md_error;
    xerxes_md_warning        = util_funcs.dxp_md_warning;
    xerxes_md_info           = util_funcs.dxp_md_info;
    xerxes_md_debug          = util_funcs.dxp_md_debug;
    xerxes_md_output         = util_funcs.dxp_md_output;
    xerxes_md_suppress_log   = util_funcs.dxp_md_suppress_log;
    xerxes_md_enable_log	   = util_funcs.dxp_md_enable_log;
    xerxes_md_set_log_level  = util_funcs.dxp_md_set_log_level;
    xerxes_md_log            = util_funcs.dxp_md_log;
    xerxes_md_wait           = util_funcs.dxp_md_wait;
    xerxes_md_puts           = util_funcs.dxp_md_puts;
    xerxes_md_set_priority   = util_funcs.dxp_md_set_priority;
    xerxes_md_fgets          = util_funcs.dxp_md_fgets;
    xerxes_md_tmp_path       = util_funcs.dxp_md_tmp_path;
    xerxes_md_clear_tmp      = util_funcs.dxp_md_clear_tmp;
    xerxes_md_path_separator = util_funcs.dxp_md_path_separator;

    /* Initialize the global utils structure */
    if (utils != NULL) {
        xerxes_md_free(utils->dllname);
        xerxes_md_free(utils->funcs);
        xerxes_md_free(utils);
        utils = NULL;
    }

    /* Allocate and assign memory for the utils structure */
    utils = (Utils *) xerxes_md_alloc(sizeof(Utils));
    utils->dllname = (char *) xerxes_md_alloc((strlen(lstr)+1)*sizeof(char));
    strcpy(utils->dllname, lstr);

    utils->funcs = (Xia_Util_Functions *) xerxes_md_alloc(sizeof(Xia_Util_Functions));

    utils->funcs->dxp_md_alloc         	= xerxes_md_alloc;
    utils->funcs->dxp_md_free          	= xerxes_md_free;
    utils->funcs->dxp_md_error         	= xerxes_md_error;
    utils->funcs->dxp_md_warning       	= xerxes_md_warning;
    utils->funcs->dxp_md_info          	= xerxes_md_info;
    utils->funcs->dxp_md_debug         	= xerxes_md_debug;
    utils->funcs->dxp_md_output        	= xerxes_md_output;
    utils->funcs->dxp_md_suppress_log  	= xerxes_md_suppress_log;
    utils->funcs->dxp_md_enable_log	    = xerxes_md_enable_log;
    utils->funcs->dxp_md_set_log_level 	= xerxes_md_set_log_level;
    utils->funcs->dxp_md_log           	= xerxes_md_log;
    utils->funcs->dxp_md_wait          	= xerxes_md_wait;
    utils->funcs->dxp_md_puts          	= xerxes_md_puts;
    utils->funcs->dxp_md_fgets          = xerxes_md_fgets;
    utils->funcs->dxp_md_tmp_path       = xerxes_md_tmp_path;
    utils->funcs->dxp_md_clear_tmp      = xerxes_md_clear_tmp;
    utils->funcs->dxp_md_path_separator = xerxes_md_path_separator;

    return DXP_SUCCESS;
}


/*
 * Initialize the data structures to an empty state
 */
XERXES_EXPORT int XERXES_API dxp_init_ds(void)
{
    int status = DXP_SUCCESS;

    Board_Info *current = btypes_head;
    Board_Info *next = NULL;

    /* CLEAR BTYPES LINK LIST */

    /* First make sure there is not an allocated btypes_head */
    while (current != NULL) {
        next = current->next;
        /* Clear the memory first */
        dxp_free_binfo(current);
        current = next;
    }
    /* And clear out the pointer to the head of the list(the memory is freed) */
    btypes_head = NULL;

    /* NULL the current board type in the static variable, to ensure we dont point
     * free memory */
    working_btype = NULL;

    /* CLEAR BOARDS,DSP,FIPPI,DEFAULTS LINK LIST */
    if((status=dxp_init_boards_ds())!=DXP_SUCCESS) {
        sprintf(info_string,"Failed to initialize the boards structures");
        dxp_log_error("dxp_init_ds",info_string,status);
        return status;
    }

    return DXP_SUCCESS;
}

/*
 * Initialize the BOARD data structures to an empty state
 */
XERXES_EXPORT int XERXES_API dxp_init_boards_ds(void)
{
    int status=DXP_SUCCESS;

    Board *next;
    Board *current = system_head;

    /* Initialize the system_head linked list */
    if (current!=NULL) {
        /* Clear out the linked list...deallocating all memory */
        do {
            next = current->next;
            dxp_free_board(current);
            current = next;
        } while (next!=NULL);
        system_head = NULL;
    }

    /* NULL the current board in the static variable, to ensure we dont point
     * free memory */
    working_board = NULL;

    /* CLEAR INTERFACE LINK LIST */
    if((status=dxp_init_iface_ds())!=DXP_SUCCESS) {
        sprintf(info_string,"Failed to initialize the Interface structures");
        dxp_log_error("dxp_init_boards_ds",info_string,status);
        return status;
    }

    /* CLEAR DSP LINK LIST */
    if((status=dxp_init_dsp_ds())!=DXP_SUCCESS) {
        sprintf(info_string,"Failed to initialize the dsp structures");
        dxp_log_error("dxp_init_boards_ds",info_string,status);
        return status;
    }

    /* CLEAR FIPPI LINK LIST */
    if((status=dxp_init_fippi_ds())!=DXP_SUCCESS) {
        sprintf(info_string,"Failed to initialize the fippi structures");
        dxp_log_error("dxp_init_boards_ds",info_string,status);
        return status;
    }

    /* Initialize the number of modules and channels */
    numDxpMod = 0;

    return DXP_SUCCESS;
}

/*
 * Initialize the INTERFACE data structures to an empty state
 */
static int XERXES_API dxp_init_iface_ds(void)
{
    Interface *next;
    Interface *current = iface_head;

    /* Search thru the iface LL and clear them out */
    while (current != NULL) {
        next = current->next;
        /* Clear the memory first */
        dxp_free_iface(current);
        current = next;
    }
    /* And clear out the pointer to the head of the list(the memory is freed) */
    iface_head = NULL;

    /* NULL the current board type in the static variable, to ensure we dont point
     * free memory */
    working_iface = NULL;

    return DXP_SUCCESS;
}

/*
 * Initialize the DSP data structures to an empty state
 */
static int XERXES_API dxp_init_dsp_ds(void)
{
    Dsp_Info *next;
    Dsp_Info *current = dsp_head;

    /* Search thru the iface LL and clear them out */
    while (current != NULL) {
        next = current->next;
        /* Clear the memory first */
        dxp_free_dsp(current);
        current = next;
    }
    /* And clear out the pointer to the head of the list(the memory is freed) */
    dsp_head = NULL;

    return DXP_SUCCESS;
}

/*
 * Initialize the FIPPI data structures to an empty state
 */
static int XERXES_API dxp_init_fippi_ds(void)
{
    Fippi_Info *next;
    Fippi_Info *current = fippi_head;

    /* Search thru the iface LL and clear them out */
    while (current != NULL) {
        next = current->next;
        /* Clear the memory first */
        dxp_free_fippi(current);
        current = next;
    }
    /* And clear out the pointer to the head of the list(the memory is freed) */
    fippi_head = NULL;

    return DXP_SUCCESS;
}

/*
 * Add an item specified by token to the system configuration.
 */
int XERXES_API dxp_add_system_item(char *ltoken, char **values)
{
    int status=DXP_SUCCESS;
    unsigned int j;

    char *strTmp = '\0';

    strTmp = (char *)xerxes_md_alloc((strlen(ltoken) + 1) * sizeof(char));
    strcpy(strTmp, ltoken);

    /* Now Proceed to process the entry */
    sprintf(info_string, "Adding system item '%s'", ltoken);
    dxp_log_info("dxp_add_system_item", info_string);

    /* Convert the token to lower case for comparison */
    for (j = 0; j < strlen(strTmp); j++) {
        strTmp[j] = (char)tolower((int)strTmp[j]);
    }

    if ((strncmp(strTmp, "dxp",3)==0) ||
            (STREQ(strTmp, "xmap"))     	 ||
            (STREQ(strTmp, "stj"))     	 ||
            (STREQ(strTmp, "mercury"))    ||
            (strncmp(strTmp, "udxp", 4) == 0)) {
        /* Load function pointers for local use */
        if ((status=dxp_add_btype(strTmp, values[0], NULL))!=DXP_SUCCESS) {
            sprintf(info_string,"Problems adding board type %s",PRINT_NON_NULL(strTmp));
            dxp_log_error("dxp_add_system_item",info_string,status);
            xerxes_md_free((void *)strTmp);
            strTmp = NULL;
            return DXP_SUCCESS;
        }
    } else {
        status = DXP_INPUT_UNDEFINED;
        sprintf(info_string,"Unable to add token: %s", strTmp);
        dxp_log_error("dxp_add_system_item",info_string,status);
        xerxes_md_free((void *)strTmp);
        strTmp = NULL;
        return status;
    }

    xerxes_md_free((void *)strTmp);
    strTmp = NULL;

    return status;
}

/*
 * Add an item specified by token to the board configuration.
 */
int dxp_add_board_item(char *ltoken, char **values)
{
    int status           = DXP_SUCCESS;
    int param_array_size = 0;
    int total_syms       = 0;


    unsigned int j;
    unsigned int nchan;

    unsigned short used;

    int ioChan;
    int chanid;

    boolean_t found = FALSE_;

    int *detChan;

    char *board_type = NULL;

    sprintf(info_string, "Adding board item %s, value '%s'", ltoken, values[0]);
    dxp_log_debug("dxp_add_board_item", info_string);

    /*
     * Now Identify the entry type
     */
    if (STREQ(ltoken, "board_type")) {

        board_type = values[0];
        MAKE_LOWER_CASE(board_type, j);

        /* Search thru board types to match find the structure
           in the linked list */
        working_btype = btypes_head;

        while ((working_btype != NULL) && !found) {
            if (STREQ(working_btype->name, board_type)) {
                found = TRUE_;
                break;
            }

            working_btype = working_btype->next;
        }

        if (!found) {
            sprintf(info_string, "Unknown board type '%s'", board_type);
            dxp_log_error("dxp_add_board_item", info_string, DXP_UNKNOWN_BTYPE);
            return DXP_UNKNOWN_BTYPE;
        }

        sprintf(info_string, "working_btype->name = '%s'", working_btype->name);
        dxp_log_debug("dxp_add_board_item", info_string);


    } else if (STREQ(ltoken, "interface")) {
        /* Define the interface tag, this will typically be the information
         * that the DXP_MD_INITIALIZE() will use to determine what MD routines
         * are needed */

        if ((values[0] == NULL) || (values[1] == NULL)) {
            dxp_log_error("dxp_add_board_item", "Interface 'tag' and 'type' may not "
                          "be NULL.", DXP_INITIALIZE);
            return DXP_INITIALIZE;
        }

        MAKE_LOWER_CASE(values[0], j);

        status = dxp_add_iface(values[0], values[1], &working_iface);

        if(status != DXP_SUCCESS) {
            sprintf(info_string, "Unable to load '%s' interface %s",
                    PRINT_NON_NULL(values[1]),PRINT_NON_NULL(values[0]));
            dxp_log_error("dxp_add_board_item", info_string, status);
            return status;
        }

        if (working_board) {
            working_board->iface = working_iface;
        }

    } else if (STREQ(ltoken, "module")) {

        sprintf(info_string, "New module '%d': interface = '%s', # channels = %s",
                numDxpMod, values[0], values[1]);
        dxp_log_info("dxp_add_board_item", info_string);

        if(numDxpMod == MAXDXP) {
            sprintf(info_string, "Too many modules specified: only %d are allowed",
                    MAXDXP);
            dxp_log_error("dxp_add_board_item", info_string, DXP_ARRAY_TOO_SMALL);
            return DXP_ARRAY_TOO_SMALL;
        }

        if (working_btype== NULL) {
            sprintf(info_string, "No board type defined for module '%d'", numDxpMod);
            dxp_log_error("dxp_add_board_item", info_string, DXP_UNKNOWN_BTYPE);
            return DXP_UNKNOWN_BTYPE;
        }

        if (working_iface==NULL) {
            sprintf(info_string, "No interface define for module '%d'", numDxpMod);
            dxp_log_error("dxp_add_board_item", info_string, DXP_INITIALIZE);
            return DXP_INITIALIZE;
        }

        /* Set the NumDxpMod'th to have none of the channels used */
        used = 0;

        /* Pull out the information for where in the system the module sits
         * and then open the crate via the dxp_md_open() routine.  This routine is
         * expected to return a number for ioChan that identifies this module's
         * location in the array used by the machine dependent functions to
         * identify how to talk to this module. */

        status = working_iface->funcs->dxp_md_open(values[0], &ioChan);

        if(status != DXP_SUCCESS) {
            sprintf(info_string, "Error opening module '%d' with interface '%s'",
                    numDxpMod, PRINT_NON_NULL(values[0]));
            dxp_log_error("dxp_add_board_item", info_string, status);
            return status;
        }

        /* Now loop over the next MAXCHAN numbers to define a detector element number
         * associated with each DXP channel.  If positive then the channel is
         * considered instrumented and the number is stored in the Used[] array
         * for future reference.  Else if the number is negative or 0, the channel
         * is ignored */

        nchan = (unsigned int)strtol(values[1], NULL, 10);

        detChan = (int *)xerxes_md_alloc(nchan * sizeof(int));

        for(j = 0; j < nchan; j++) {
            chanid = (int)strtol(values[j + 2], NULL, 10);

            if (chanid >= 0) {
                used |= 1 << j;
                detChan[j] = chanid;
            } else {
                detChan[j] = -1;
            }
        }

        /* Find the last entry in the Linked list and add to the end */
        working_board = system_head;

        if (working_board != NULL) {

            while (working_board->next != NULL) {
                working_board = working_board->next;
            }

            working_board->next = (Board *)xerxes_md_alloc(sizeof(Board));
            working_board = working_board->next;

        } else {
            system_head = (Board *)xerxes_md_alloc(sizeof(Board));
            working_board = system_head;
        }

        working_board->ioChan  = ioChan;
        working_board->used	   = used;
        working_board->detChan = detChan;
        working_board->mod	   = numDxpMod;
        working_board->nchan   = nchan;
        working_board->btype   = working_btype;
        working_board->iface   = working_iface;
        working_board->is_full_reboot = FALSE_;

        memset(working_board->state, 0, sizeof(working_board->state));

        working_board->btype->funcs->dxp_init_driver(working_iface);
        working_board->next	= NULL;

        working_board->mmu          = NULL;
        working_board->system_dsp   = NULL;
        working_board->fippi_a      = NULL;
        working_board->system_fpga  = NULL;
        working_board->system_fippi = NULL;            
                
        /* Memory assignments */
        working_board->chanstate =
            (Chan_State *)xerxes_md_alloc(nchan * sizeof(Chan_State));

        memset(working_board->chanstate, 0, nchan * sizeof(Chan_State));

        working_board->params =
            (unsigned short **)xerxes_md_alloc(nchan * sizeof(unsigned short *));

        working_board->dsp =
            (Dsp_Info **)xerxes_md_alloc(nchan * sizeof(Dsp_Info *));
        
        working_board->fippi=
            (Fippi_Info **)xerxes_md_alloc(nchan * sizeof(Fippi_Info *));

        /* Fill in some default information */
        for (j = 0; j < working_board->nchan; j++) {
            working_board->dsp[j]        = NULL;
            working_board->params[j]     = NULL;
            working_board->fippi[j]      = NULL;
        }

        numDxpMod++;

    } else if (STREQ(ltoken, "dsp")) {

        if (working_btype == NULL) {
            dxp_log_error("dxp_add_board_item", "Unable to process 'dsp' unless a "
                          "board type has been defined", DXP_UNKNOWN_BTYPE);
            return DXP_UNKNOWN_BTYPE;
        }

        chanid = (int)strtol(values[0] , NULL, 10);

        if ((chanid < 0) || (chanid >= (int)working_board->nchan)) {
            sprintf(info_string, "Selected channel (%d) for DSP code is not valid for "
                    "the current module, which has %d channels", chanid,
                    (int)working_board->nchan);
            dxp_log_error("dxp_add_board_item", info_string, DXP_BADCHANNEL);
            return DXP_BADCHANNEL;
        }

        status = dxp_add_dsp(values[1], working_btype,
                             &(working_board->dsp[chanid]));

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Unable to add DSP code: '%s'",
                    PRINT_NON_NULL(values[1]));
            dxp_log_error("dxp_add_board_item", info_string, status);
            return status;
        }

        working_board->chanstate[chanid].dspdownloaded = 2;

        if (working_board->params[chanid]) {
            xerxes_md_free(working_board->params[chanid]);
            working_board->params[chanid] = NULL;
        }

        param_array_size =
            working_board->dsp[chanid]->params->nsymbol * sizeof(unsigned short);

        if (param_array_size > 0) {
            /* Only allocate memory if there are parameters */
            working_board->params[chanid] = (unsigned short *)
                                            xerxes_md_alloc(param_array_size);

            memset(working_board->params[chanid], 0, param_array_size);
        }

    } else if (STREQ(ltoken, "fippi")) {

        if (working_btype == NULL) {
            dxp_log_error("dxp_add_board_item", "Unable to process 'fippi' "
                          "unless a board type has been defined", DXP_UNKNOWN_BTYPE);
            return DXP_UNKNOWN_BTYPE;
        }

        chanid = (int)strtol(values[0], NULL, 10);

        if ((chanid < 0) || (chanid >= (int)working_board->nchan)) {
            sprintf(info_string, "Selected channel (%d) for FiPPI code is not valid "
                    "for the current module, which has %d channels", chanid,
                    (int)working_board->nchan);
            dxp_log_error("dxp_add_board_item", info_string, DXP_BADCHANNEL);
            return DXP_BADCHANNEL;
        }

        status = dxp_add_fippi(values[1], working_btype,
                               &(working_board->fippi[chanid]));

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Unable to add FiPPI: '%s'",
                    PRINT_NON_NULL(values[1]));
            dxp_log_error("dxp_add_board_item", info_string, status);
            return status;
        }

    } else if (STREQ(ltoken, "system_fippi")) {

        if (working_btype == NULL) {
            dxp_log_error("dxp_add_board_item", "Unable to process 'system_fippi' "
                          "unless a board type has been defined", DXP_UNKNOWN_BTYPE);
            return DXP_UNKNOWN_BTYPE;
        }

        status = dxp_add_fippi(values[0], working_btype,
                               &(working_board->system_fippi));

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Unable to add System FiPPI: '%s'",
                    PRINT_NON_NULL(values[0]));
            dxp_log_error("dxp_add_board_item", info_string, status);
            return status;
        }

    } else if (STREQ(ltoken, "system_fpga")) {

        if (working_btype == NULL) {
            dxp_log_error("dxp_add_board_item", "Unable to process 'system_fpga' "
                          "unless a board type has been defined", DXP_UNKNOWN_BTYPE);
            return DXP_UNKNOWN_BTYPE;
        }

        status = dxp_add_fippi(values[0], working_btype,
                               &(working_board->system_fpga));

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Unable to add System FPGA: '%s'",
                    PRINT_NON_NULL(values[0]));
            dxp_log_error("dxp_add_board_item", info_string, status);
            return status;
        }

    } else if (STREQ(ltoken, "fippi_a")) {

        if (working_btype == NULL) {
            dxp_log_error("dxp_add_board_item", "Unable to process 'fippi_a' "
                          "unless a board type has been defined", DXP_UNKNOWN_BTYPE);
            return DXP_UNKNOWN_BTYPE;
        }

        status = dxp_add_fippi(values[0], working_btype,
                               &(working_board->fippi_a));

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Unable to add FiPPI A: '%s'",
                    PRINT_NON_NULL(values[0]));
            dxp_log_error("dxp_add_board_item", info_string, status);
            return status;
        }

    } else if (STREQ(ltoken, "system_dsp")) {

        if (working_btype == NULL) {
            dxp_log_error("dxp_add_board_item", "Unable to process 'system_dsp' "
                          "unless a board type has been defined", DXP_UNKNOWN_BTYPE);
            return DXP_UNKNOWN_BTYPE;
        }

        status = dxp_add_dsp(values[0], working_btype, &(working_board->system_dsp));

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Unable to add System DSP: '%s'",
                    PRINT_NON_NULL(values[0]));
            dxp_log_error("dxp_add_board_item", info_string, status);
            return status;
        }

        total_syms = working_board->system_dsp->params->nsymbol +
                     working_board->system_dsp->params->n_per_chan_symbols;

        for (j = 0; j < working_board->nchan; j++) {
            param_array_size = total_syms * sizeof(unsigned short);
            working_board->params[j] =
                (unsigned short *)xerxes_md_alloc(param_array_size);
            memset(working_board->params[j], 0, param_array_size);
        }

    } else if (STREQ(ltoken, "mmu")) {

        if (working_btype == NULL) {
            dxp_log_error("dxp_add_board_item", "Unable to process 'mmu' unless a "
                          "board type has been defined", DXP_UNKNOWN_BTYPE);
            return DXP_UNKNOWN_BTYPE;
        }

        status = dxp_add_fippi(values[0], working_btype, &(working_board->mmu));

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Unable to add MMU: '%s'", PRINT_NON_NULL(values[0]));
            dxp_log_error("dxp_add_board_item", info_string, status);
            return status;
        }

    } else {
        sprintf(info_string, "Unknown board item '%s' to add",
                PRINT_NON_NULL(ltoken));
        dxp_log_error("dxp_add_board_item", info_string, DXP_NOMATCH);
        return DXP_NOMATCH;
    }

    return DXP_SUCCESS;
}


/*
 * Routine to free the memory allocated to a Board Structure
 */
static int XERXES_API dxp_free_board(Board* board)
/* Board *board;							Input: pointer to structure to free	*/
{
    int status=DXP_SUCCESS;
    unsigned short i;

    if (board==NULL) {
        status = DXP_NOMEM;
        sprintf(info_string,"Board object unallocated:  can not free");
        dxp_log_error("dxp_free_board",info_string,status);
        return status;
    }

    /* Free the detChan array */
    if (board->detChan!=NULL)
        xerxes_md_free(board->detChan);

    /* Free the params array */
    if (board->params!=NULL) {
        for (i=0; i<board->nchan; i++) {
            if (board->params[i]!=NULL) {
                xerxes_md_free(board->params[i]);
                board->params[i] = NULL;
            }
        }
        xerxes_md_free(board->params);
    }

    /* Free the Chan_State Array*/
    if (board->chanstate!=NULL)
        xerxes_md_free(board->chanstate);

    /* Free the Dsp Array*/
    if (board->dsp != NULL)
        xerxes_md_free(board->dsp);

    /* Remove the system dsp reference*/
    if (board->system_dsp != NULL)
        board->system_dsp = NULL;

    /* Free the Fippi Array*/
    if (board->fippi!=NULL)
        xerxes_md_free(board->fippi);

    /* Free the Board structure */
    xerxes_md_free(board);
    board = NULL;

    return status;
}

/*
 * Routine to free the memory allocated to an Interface Structure
 */
static int XERXES_API dxp_free_iface(Interface *iface)
/* Interface *iface;					Input: pointer to structure to free	*/
{
    int status=DXP_SUCCESS;

    if (iface ==NULL) {
        status = DXP_NOMEM;
        sprintf(info_string,"Interface object unallocated:  can not free");
        dxp_log_error("dxp_free_iface",info_string,status);
        return status;
    }

    if (iface->dllname != NULL) xerxes_md_free(iface->dllname);
    if (iface->ioname  != NULL) xerxes_md_free(iface->ioname);
    if (iface->funcs   != NULL) xerxes_md_free(iface->funcs);

    /* Free the Interface structure */
    xerxes_md_free(iface);
    iface = NULL;

    return status;
}

/*
 * Routine to free the memory allocated to an Board_Info Structure
 */
static int XERXES_API dxp_free_binfo(Board_Info *binfo)
/* Board_Info *binfo;					Input: pointer to structure to free	*/
{
    int status=DXP_SUCCESS;

    if (binfo==NULL) {
        status = DXP_NOMEM;
        sprintf(info_string,"Board_Info object unallocated:  can not free");
        dxp_log_error("dxp_free_binfo",info_string,status);
        return status;
    }

    if (binfo->pointer != NULL) xerxes_md_free(binfo->pointer);
    if (binfo->name    != NULL) xerxes_md_free(binfo->name);
    if (binfo->funcs   != NULL) xerxes_md_free(binfo->funcs);

    /* Free the Board_Info structure */
    xerxes_md_free(binfo);
    binfo = NULL;

    return status;
}

/*
 * Routine to free the memory allocated to an Dsp Structure
 */
static int XERXES_API dxp_free_dsp(Dsp_Info *dsp)
/* Dsp_Info *dsp;					Input: pointer to structure to free	*/
{
    int status = DXP_SUCCESS;

    if (dsp == NULL) {
        status = DXP_NOMEM;
        sprintf(info_string,"Dsp object unallocated:  can not free");
        dxp_log_error("dxp_free_dsp",info_string,status);
        return status;
    }

    sprintf(info_string, "Freeing dsp = %p", dsp);
    dxp_log_debug("dxp_free_dsp", info_string);

    if (dsp->filename != NULL) xerxes_md_free(dsp->filename);

    if ((dsp->maxproglen > 0) && (dsp->data != NULL)) xerxes_md_free(dsp->data);

    if (dsp->params != NULL) dxp_free_params(dsp->params);

    /* Free the Dsp_Info structure */
    xerxes_md_free(dsp);
    dsp = NULL;

    return status;
}

/*
 * Routine to free the memory allocated to an Fippi Structure
 */
static int XERXES_API dxp_free_fippi(Fippi_Info *fippi)
/* Fippi_Info *fippi;					Input: pointer to structure to free	*/
{
    int status=DXP_SUCCESS;

    if (fippi == NULL) {
        status = DXP_NOMEM;
        sprintf(info_string,"Fippi object unallocated: can not free");
        dxp_log_error("dxp_free_fippi",info_string,status);
        return status;
    }

    if (fippi->filename != NULL) xerxes_md_free(fippi->filename);

    if ((fippi->proglen > 0) && fippi->data != NULL) {
        xerxes_md_free(fippi->data);
    }

    /* Free the Fippi_Info structure */
    xerxes_md_free(fippi);
    fippi = NULL;

    return status;
}


/*
 * Routine to free the memory allocated to an Fippi Structure
 */
static int XERXES_API dxp_free_params(Dsp_Params *params)
/* Dsp_Params *params;					Input: pointer to structure to free	*/
{
    int j,status=DXP_SUCCESS;

    if (params==NULL) {
        status = DXP_NOMEM;
        sprintf(info_string,"Dsp_Params object unallocated:  can not free");
        dxp_log_error("dxp_free_params",info_string,status);
        return status;
    }

    if (params->parameters!=NULL) {
        for (j = 0; j < params->maxsym; j++) {
            if (params->parameters[j].pname!=NULL)
                xerxes_md_free(params->parameters[j].pname);
        }
        xerxes_md_free(params->parameters);
    }

    if (params->per_chan_parameters != NULL) {
        for (j = 0; j < params->maxsym; j++) {
            if (params->per_chan_parameters[j].pname != NULL) {
                xerxes_md_free(params->per_chan_parameters[j].pname);
            }
        }

        if (params->chan_offsets != NULL) {
            xerxes_md_free(params->chan_offsets);
        }

        xerxes_md_free(params->per_chan_parameters);
    }

    /* Free the Dsp_Params structure */
    xerxes_md_free(params);
    params = NULL;

    return status;
}


/*
 * Routine to Load a new board Type into the linked list.  Return pointer
 * to the member corresponding to the new board type.
 */
int XERXES_API dxp_add_btype(char* name, char* pointer, char* dllname)
/* char *name;							Input: name of the new board type	*/
/* char *pointer;						Input: extra information filename	*/
/* char *dllname;						Input: name of the board type library*/
{
    int status=DXP_SUCCESS;

    Board_Info *prev = NULL;
    Board_Info *current = btypes_head;

    UNUSED(dllname);

    sprintf(info_string, "name = '%s', pointer = '%s', dllname = '%s'",
            PRINT_NON_NULL(name), PRINT_NON_NULL(pointer), PRINT_NON_NULL(dllname));
    dxp_log_debug("dxp_add_btype", info_string);


    /* First search thru the linked list and see if this configuration
     * already exists */
    while (current!=NULL) {
        /* Does the filename match? */
        if (STREQ(current->name, name)) {
            sprintf(info_string,"Board Type: %s already exists",PRINT_NON_NULL(name));
            dxp_log_error("dxp_add_btype",info_string,status);
            return DXP_SUCCESS;
        }
        prev = current;
        current = current->next;
    }

    /* No match in the existing list, add a new entry */
    if (btypes_head==NULL) {
        /* Initialize the Header entry in the btypes linked list */
        btypes_head = (Board_Info*) xerxes_md_alloc(sizeof(Board_Info));
        btypes_head->type = 0;
        current = btypes_head;
    } else {
        /* Not First time, allocate memory for next member of linked list */
        prev->next = (Board_Info*) xerxes_md_alloc(sizeof(Board_Info));
        current = prev->next;
        current->type = prev->type+1;
    }
    /* Set funcs pointer to NULL, will be set by add_btype_library() */
    current->funcs = NULL;
    /* Set the next pointer to NULL */
    current->next = NULL;
    /* This a new board type. */
    current->name = (char *) xerxes_md_alloc((strlen(name)+1)*sizeof(char));
    /* Copy the tag into the structure.  End the string with
     * a NULL character. */
    strcpy(current->name, name);

    /* Check to make sure a pointer was defined */
    if (pointer!=NULL) {
        /* Now copy the pointer to use for this type configuration */
        current->pointer = (char *) xerxes_md_alloc((strlen(pointer)+1)*sizeof(char));
        /* Copy the filename into the structure.  End the string with
         * a NULL character. */
        strcpy(current->pointer, pointer);
    } else {
        current->pointer = NULL;
    }

    /* Load function pointers for local use */
    if ((status=dxp_add_btype_library(current))!=DXP_SUCCESS) {
        sprintf(info_string,"Problems loading functions for board type %s",PRINT_NON_NULL(current->name));
        dxp_log_error("dxp_add_btype",info_string,status);
        return DXP_SUCCESS;
    }
    /* Initialize the utilty routines in the library */
    current->funcs->dxp_init_utils(utils);

    /* All done */
    return DXP_SUCCESS;
}

/*
 * Initialize the FIPPI data structures to an empty state
 */
int XERXES_API dxp_get_board_type(int *detChan, char *name)
{
    int status;
    int modChan;
    /* Pointer to the chosen Board */
    Board *chosen=NULL;

    /* Get the ioChan and modChan number that matches the detChan */
    if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
        sprintf(info_string,"Error finding detector number %d",*detChan);
        dxp_log_error("dxp_get_board_type",info_string,status);
        return status;
    }

    strncpy(name, chosen->btype->name, MAXBOARDNAME_LEN);

    return status;
}

/*
 * Routine to Load a board library and create pointers to the
 * routines in the board library.
 */
static int XERXES_API dxp_add_btype_library(Board_Info* current)
/* Board_Info *current;			Input: pointer to structure	*/
{
    int status=DXP_SUCCESS;

    /* Allocate the memory for the function pointer structure */
    current->funcs = (Functions *) xerxes_md_alloc(sizeof(Functions));
    memset(current->funcs, 0, sizeof(Functions));

#ifndef EXCLUDE_SATURN
    if (STREQ(current->name,"dxpx10p")) {
        dxp_init_saturn(current->funcs);
    } else
#endif
#ifndef EXCLUDE_UDXPS
        if (STREQ(current->name, "udxps")) {
            dxp_init_udxps(current->funcs);
        } else
#endif
#ifndef EXCLUDE_UDXP
            if (STREQ(current->name, "udxp")) {
                dxp_init_udxp(current->funcs);
            } else
#endif
#ifndef EXCLUDE_XMAP
                if (STREQ(current->name, "xmap")) {
                    dxp_init_xmap(current->funcs);
                } else
#endif /* EXCLUDE_XMAP */
#ifndef EXCLUDE_STJ
                    if (STREQ(current->name, "stj")) {
                        dxp_init_stj(current->funcs);
                    } else
#endif /* EXCLUDE_STJ */
#ifndef EXCLUDE_MERCURY
                        if (STREQ(current->name, "mercury")) {
                            dxp_init_mercury(current->funcs);
                        } else
#endif /* EXCLUDE_MERCURY */
                        {
                            status = DXP_UNKNOWN_BTYPE;
                            sprintf(info_string, "Unknown board type %s: unable to "
                                    "load pointers", PRINT_NON_NULL(current->name));
                            dxp_log_error("dxp_add_btype_library", info_string, status);
                            return status;
                        }

    /* All done */
    return DXP_SUCCESS;
}


/*
 * Create a new DSP object and initialize it.
 *
 * This routine creates a new DSP structure representing the data in filename
 * and then assigns it to the global DSP list and the passed in pointer.
 */
XERXES_STATIC int dxp_add_dsp(char* filename, Board_Info* board,
                              Dsp_Info** passed)
{
    int status;


    unsigned int i;

    Dsp_Info *new_dsp = NULL;
    Dsp_Info *prev    = NULL;
    Dsp_Info *current = dsp_head;


    ASSERT(filename != NULL);
    ASSERT(board    != NULL);
    ASSERT(passed   != NULL);


    /* If this DSP has already been loaded then we can just point at it
     * instead of re-loading it.
     */
    while (current!=NULL) {

        if (STREQ(current->filename, filename)) {
            sprintf(info_string, "Skipping creation of new DSP entry for '%s', since "
                    "we already have a copy loaded at %p", filename, current);
            dxp_log_info("dxp_add_dsp", info_string);

            *passed = current;
            return DXP_SUCCESS;
        }

        prev = current;
        current = current->next;
    }

    new_dsp = (Dsp_Info *)xerxes_md_alloc(sizeof(Dsp_Info));

    if (!new_dsp) {
        sprintf(info_string, "Error allocating %zu bytes for 'new_dsp'",
                sizeof(Dsp_Info));
        dxp_log_error("dxp_add_dsp", info_string, DXP_NOMEM);
        return DXP_NOMEM;
    }

    new_dsp->params = (Dsp_Params *)xerxes_md_alloc(sizeof(Dsp_Params));

    if (!new_dsp->params) {
        dxp_free_dsp(new_dsp);

        sprintf(info_string, "Error allocating %zu bytes for 'new_dsp->params'",
                sizeof(Dsp_Params));
        dxp_log_error("dxp_add_dsp", info_string, DXP_NOMEM);
        return DXP_NOMEM;
    }

    new_dsp->filename = (char *)xerxes_md_alloc(strlen(filename) + 1);

    if (!new_dsp->filename) {
        dxp_free_dsp(new_dsp);

        sprintf(info_string, "Error allocating %zu bytes for 'new_dsp->filename'",
                strlen(filename) + 1);
        dxp_log_error("dxp_add_dsp", info_string, DXP_NOMEM);
        return DXP_NOMEM;
    }

    strncpy(new_dsp->filename, filename, strlen(filename) + 1);

    status = board->funcs->dxp_get_dspinfo(new_dsp);

    if (status != DXP_SUCCESS) {
        dxp_free_dsp(new_dsp);

        sprintf(info_string, "Unable to get DSP information for %s", filename);
        dxp_log_error("dxp_add_dsp", info_string, status);
        return status;
    }

    if (new_dsp->maxproglen > 0) {
        /* maxproglen == 0 indicates "ignore.dsp" as in udxp */
        new_dsp->data = (unsigned short *)xerxes_md_alloc(new_dsp->maxproglen *
                                                          sizeof(unsigned short));

        if (!new_dsp->data) {
            dxp_free_dsp(new_dsp);

            sprintf(info_string, "Error allocating %zu bytes for 'new_dsp->data'",
                    new_dsp->maxproglen * sizeof(unsigned short));
            dxp_log_error("dxp_add_dsp", info_string, DXP_NOMEM);
            return DXP_NOMEM;
        }
    }

    new_dsp->params->parameters = (Parameter *)xerxes_md_alloc(
                                      new_dsp->params->maxsym *
                                      sizeof(Parameter));

    if (!new_dsp->params->parameters) {
        dxp_free_dsp(new_dsp);

        sprintf(info_string, "Error allocating %zu bytes for 'new_dsp->params->"
                "parameter'", new_dsp->params->maxsym * sizeof(Parameter));
        dxp_log_error("dxp_add_dsp", info_string, DXP_NOMEM);
        return DXP_NOMEM;
    }

    new_dsp->params->per_chan_parameters = (Parameter *)xerxes_md_alloc(
                                               new_dsp->params->maxsym *
                                               sizeof(Parameter));

    if (!new_dsp->params->per_chan_parameters) {
        dxp_free_dsp(new_dsp);

        sprintf(info_string, "Error allocating %zu bytes for 'new_dsp->params->"
                "per_chan_parameters'", new_dsp->params->maxsym * sizeof(Parameter));
        dxp_log_error("dxp_add_dsp", info_string, DXP_NOMEM);
        return DXP_NOMEM;
    }

    for (i = 0; i < new_dsp->params->maxsym; i++) {
        new_dsp->params->parameters[i].pname  = (char *)xerxes_md_alloc(
                                                    new_dsp->params->maxsymlen);

        if (!new_dsp->params->parameters[i].pname) {
            dxp_free_dsp(new_dsp);

            sprintf(info_string, "Error allocating %hu bytes for 'new_dsp->params->"
                    "parameters[%u].pname'", new_dsp->params->maxsymlen, i);
            dxp_log_error("dxp_add_dsp", info_string, DXP_NOMEM);
        }

        new_dsp->params->parameters[i].address = 0;
        new_dsp->params->parameters[i].access  = 0;
        new_dsp->params->parameters[i].lbound  = 0;
        new_dsp->params->parameters[i].ubound  = 0;

        new_dsp->params->per_chan_parameters[i].pname = (char *)xerxes_md_alloc(
                                                            new_dsp->params->maxsymlen);

        if (!new_dsp->params->per_chan_parameters[i].pname) {
            dxp_free_dsp(new_dsp);

            sprintf(info_string, "Error allocating %hu bytes for 'new_dsp->params->"
                    "parameters[%u].pname'", new_dsp->params->maxsymlen, i);
            dxp_log_error("dxp_add_dsp", info_string, DXP_NOMEM);
            return DXP_NOMEM;
        }
    }

    /* Initialize pointers not used by all devices */
    new_dsp->params->chan_offsets = NULL;
    new_dsp->params->n_per_chan_symbols = 0;

    sprintf(info_string, "Preparing to get DSP configuration: %s",
            new_dsp->filename);
    dxp_log_debug("dxp_add_dsp", info_string);

    status = board->funcs->dxp_get_dspconfig(new_dsp);

    if (status != DXP_SUCCESS) {
        dxp_free_dsp(new_dsp);

        sprintf(info_string, "Error loading DSP file %s", filename);
        dxp_log_error("dxp_add_dsp", info_string, status);
        return status;
    }

    /* Add the new DSP code to the global list. */
    if (!dsp_head) {
        dsp_head = new_dsp;

    } else {
        prev->next = new_dsp;
    }

    new_dsp->next = NULL;

    sprintf(info_string, "Added dsp maxsym = %u, n_per_chan_symbols = %u",
            new_dsp->params->maxsym, new_dsp->params->n_per_chan_symbols);
    dxp_log_debug("dxp_add_dsp", info_string);

    *passed = new_dsp;

    return DXP_SUCCESS;
}


/*
 * Routine to Load a FIPPI configuration file into the Fippi_Info linked list
 * or find the matching member of the list and return a pointer to it
 */
static int XERXES_API dxp_add_fippi(char* filename, Board_Info* board,
                                    Fippi_Info** fippi)
/* char *filename;					Input: filename of the FIPPI program	*/
/* Board_Info *board;				Input: need the type of board		*/
/* Fippi_Info **fippi;				Output: Pointer to the structure		*/
{
    int status;

    Fippi_Info *prev = NULL;
    Fippi_Info *current = fippi_head;

    /* First search thru the linked list and see if this configuration
     * already exists */
    while (current != NULL) {
        /* Does the filename match? */
        if (STREQ(current->filename, filename)) {
            *fippi = current;
            return DXP_SUCCESS;
        }
        prev = current;
        current = current->next;
    }

    /* No match in the existing list, add a new entry */
    *fippi = xerxes_md_alloc(sizeof(Fippi_Info));

    if (*fippi == NULL) {
        sprintf(info_string, "Error allocating %zu bytes for '*fippi'",
                sizeof(Fippi_Info));
        dxp_log_error("dxp_add_fippi", info_string, DXP_NOMEM);
        return DXP_NOMEM;
    }

    (*fippi)->next = NULL;

    /* Now allocate memory and fill the program information
     * with the driver routine */
    (*fippi)->filename = xerxes_md_alloc(strlen(filename) + 1);

    if ((*fippi)->filename == NULL) {
        sprintf(info_string, "Error allocating %zu bytes for '*fippi->filename'",
                strlen(filename) + 1);
        dxp_log_error("dxp_add_fippi", info_string, DXP_NOMEM);
        return DXP_NOMEM;
    }

    strcpy((*fippi)->filename, filename);

    /* Fill in the default lengths */
    status = board->funcs->dxp_get_fipinfo(*fippi);

    if (status != DXP_SUCCESS ) {
        dxp_free_fippi(*fippi);
        dxp_log_error("dxp_add_fippi", "Unable to get FIPPI information", status);
        return status;
    }

    /* Finish allocating memory */
    if ((*fippi)->maxproglen > 0) {

        /* need to keep proglen up to date if data is allocated here */
        (*fippi)->proglen = (*fippi)->maxproglen;
        (*fippi)->data = (unsigned short *)
               xerxes_md_alloc(((*fippi)->maxproglen) * sizeof(unsigned short));

        if (!(*fippi)->data) {
            dxp_free_fippi(*fippi);
            sprintf(info_string, "Error allocating %lu bytes for '*fippi->data'",
                    (*fippi)->maxproglen * sizeof(unsigned short));
            dxp_log_error("dxp_add_fippi", info_string, DXP_NOMEM);
            return DXP_NOMEM;
        }
    }

    status = board->funcs->dxp_get_fpgaconfig(*fippi);

    if (status != DXP_SUCCESS) {
        dxp_free_fippi(*fippi);
        dxp_log_error("dxp_add_fippi", "Unable to load new FIPPI file", status);
        return status;
    }

    /* Update global linked list with the new FiPPI. */
    if (fippi_head == NULL) {
        fippi_head = *fippi;
    } else {
        prev->next = *fippi;
    }

    return DXP_SUCCESS;


}

/*
 * Routine to Load a new DLL library and return a pointer to the proper
 * Libs structure
 */
static int XERXES_API dxp_add_iface(char* dllname, char* iolib, Interface** iface)
/* char *dllname;					Input: filename of the interface DLL		*/
/* char *iolib;						Input: name of lib to talk to the board	*/
/* Interface **iface;				Output: Pointer to the structure			*/
{
    int status;
    unsigned int maxMod = MAXDXP;

    Interface *prev = NULL;
    Interface *current = iface_head;

    /* First search thru the linked list and see if this configuration
     * already exists */
    while (current!=NULL) {
        /* Does the filename match? */
        if (STREQ(current->dllname, dllname)) {
            *iface = current;
            return DXP_SUCCESS;
        }
        prev = current;
        current = current->next;
    }

    /* No match in the existing list, add a new entry */
    if (iface_head==NULL) {
        /* First time, allocate memory for head of linked list */
        iface_head = (Interface *) xerxes_md_alloc(sizeof(Interface));
        current = iface_head;
    } else {
        /* Not First time, allocate memory for next member of linked list */
        prev->next = (Interface *) xerxes_md_alloc(sizeof(Interface));
        current = prev->next;
    }
    /* Set the next pointer to NULL */
    current->next = NULL;
    /* Copy the name of the DLL into the structure */
    current->dllname = (char *) xerxes_md_alloc((strlen(dllname)+1)*sizeof(char));
    strcpy(current->dllname, dllname);
    /* Copy in the ioname variable for future reference */
    current->ioname = (char *) xerxes_md_alloc((strlen(iolib)+1)*sizeof(char));
    strcpy(current->ioname, iolib);

    /* Allocate memory for the function structure */
    current->funcs = (Xia_Io_Functions *) xerxes_md_alloc(sizeof(Xia_Io_Functions));
    /* Retrieve the function pointers from the MD appropriate MD routine */
    dxp_md_init_io(current->funcs, current->dllname);

    /* Now initialize the library */
    if ((status =
                current->funcs->dxp_md_initialize(&maxMod,
                                                  current->ioname))!=DXP_SUCCESS) {
        status = DXP_INITIALIZE;
        sprintf(info_string,"Could not initialize %s",PRINT_NON_NULL(current->dllname));
        dxp_log_error("dxp_add_iface",info_string,status);
        return status;
    }

    /* Now assign the passed variable to the proper memory */
    *iface = current;

    /* All done */
    return DXP_SUCCESS;
}


/*
 * Download all FPGAs, DSPs and known parameter sets to the hardware.
 */
XERXES_EXPORT int XERXES_API dxp_user_setup(void)
{

    int status;

    Board *current = system_head;


    while (current != NULL) {
        current->is_full_reboot = TRUE_;
        current = current->next;
    }

    dxp_log_info("dxp_user_setup", "Preparing to download FPGAs");

    status = dxp_fipconfig();

    if (status != DXP_SUCCESS) {
        dxp_log_error("dxp_user_setup", "Error downloading FPGAs", status);
        return status;
    }

    dxp_log_info("dxp_user_setup", "Preparing to download DSP code");

    status = dxp_dspconfig();

    if (status != DXP_SUCCESS) {
        dxp_log_error("dxp_user_setup", "Error downloading DSP code", status);
        return status;
    }

    current = system_head;

    /* The per-module configurations are done here. Only configurations that
     * are applicable to every module should be done here. Hardware specific
     * procedures should be farmed out to the individual device driver.
     */
    while (current != NULL) {
        current->is_full_reboot = FALSE_;
        current = current->next;
    }

    return status;
}

/*
 * Downloads all of the known FPGA configurations to the hardware.
 *
 * Past versions of this routine encoded lots of incorrect logic into
 * the procedure. This version delegates most of the download work to
 * routines in the device driver layer since some of the old assumptions
 * are no longer correct.
 *
 * Loop over each module and call the dxp_download_fpgaconfig() w/ 'all'. This
 * delegates the responsibility for determining the exact methodology to use
 * to the Device Driver layer and, as a consequence, dxp_fipconfig() can remain
 * hardware agnostic.
 */
XERXES_STATIC int dxp_fipconfig(void)
{
    int status;


    Board *current = system_head;


    while (current != NULL) {
        status = current->btype->funcs->dxp_download_fpgaconfig(&(current->ioChan),
                                                                &allChan, "all",
                                                                current);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error Broadcasting fpga configurations to module %d",
                    current->mod);
            dxp_log_error("dxp_fipconfig", info_string, status);
            return status;
        }

        current = current->next;
    }

    return DXP_SUCCESS;
}


/*
 * Replaces an FPGA configuration.
 */
int XERXES_API dxp_replace_fpgaconfig(int* detChan, char *name, char* filename)
/* int *detChan;		Input: detector channel to load     */
/* char *name;                  Input: Type of FPGA to replace      */
/* char *filename;		Input: location of the new FiPPi    */
{
    int status;
    int ioChan, modChan;

    /* Pointer to the chosen Board */
    Board *chosen=NULL;

    /* Get the ioChan and dxpChan matching the detChan */

    if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
        sprintf(info_string,"Unknown Detector Channel %d",*detChan);
        dxp_log_error("dxp_replace_fpgaconfig",info_string,status);
        return status;
    }

    ioChan = chosen->ioChan;

    /* Load the Fippi configuration into the structure */

    sprintf(info_string, "filename = %s", PRINT_NON_NULL(filename));
    dxp_log_debug("dxp_replace_fipconfig", info_string);

    /* XXX If the board has defined any of the FiPPIs used by the xMAP/STJ product,
     * then we need to pass in a different Fippi_Info structure.
     */
    if (chosen->fippi_a || chosen->system_fpga) {
        if (STRNEQ(name, "a_and_b") || STREQ(name, "a")) {
            status = dxp_add_fippi(filename, chosen->btype, &(chosen->fippi_a));
        } else if (STREQ(name, "system_fpga")) {
            status = dxp_add_fippi(filename, chosen->btype, &(chosen->system_fpga));
        }
    } else if (chosen->system_fippi != NULL) {
        status = dxp_add_fippi(filename, chosen->btype, &(chosen->system_fippi));
    } else {
        status = dxp_add_fippi(filename, chosen->btype, &(chosen->fippi[modChan]));
    }

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error loading FPGA %s", PRINT_NON_NULL(filename));
        dxp_log_error("dxp_replace_fpgaconfig", info_string, status);
        return status;
    }

    /* Download the FiPPi program to the channel */

    if((status=chosen->btype->funcs->dxp_download_fpgaconfig(&ioChan,
                                                             &modChan, name, chosen))!=DXP_SUCCESS) {
        sprintf(info_string,"Error downloading %s FPGA(s) to detector %d", PRINT_NON_NULL(name), *detChan);
        dxp_log_error("dxp_replace_fpgaconfig",info_string,status);
        return status;
    }

    /* Determine if the FiPPi was downloaded successfully */
    if((status=chosen->btype->funcs->dxp_download_fpga_done(&modChan, name, chosen))!=DXP_SUCCESS) {
        sprintf(info_string,"Failed to replace %s FPGA(s) succesfully for detector %d", PRINT_NON_NULL(name), *detChan);
        dxp_log_error("dxp_replace_fpgaconfig",info_string,status);
        return status;
    }

    return DXP_SUCCESS;
}

/*
 * Download the DSP code to all of the modules.
 */
XERXES_EXPORT int XERXES_API dxp_dspconfig(void)
{
    int status;
    int ioChan;


    Board *current = system_head;


    dxp_log_debug("dxp_dspconfig", "Preapring to call dxp_download_dspconfig");

    while (current != NULL) {
        ioChan = current->ioChan;

        status = current->btype->funcs->dxp_download_dspconfig(&ioChan, &allChan,
                                                               current);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error downloading DSP code to ioChan %d", ioChan);
            dxp_log_error("dxp_dspconfig", info_string, status);
            return status;
        }

        current = current->next;
    }

    dxp_log_debug("dxp_dspconfig", "Returning from dxp_dspconfig");

    return DXP_SUCCESS;
}


/*
 * Download DSP firmware to one specific detector channel from the given file.
 * It is currently essential that all symbol tables in all DSP programs be
 * arranged in the same way.
 */
int XERXES_API dxp_replace_dspconfig(int* detChan, char* filename)
/* int *detChan;			Input: Detector Channel to download new DSP code */
/* char *filename;			Input: Filename to obtain the new DSP config	 */
{

    int ioChan, modChan, status;
    /* Pointer to the chosen Board */
    Board *chosen=NULL;

    /* Get the ioChan and dxpChan number that matches the detChan */

    if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
        sprintf(info_string,"Error finding detector number %d",*detChan);
        dxp_log_error("dxp_replace_dspconfig",info_string,status);
        return status;
    }
    ioChan = chosen->ioChan;

    /* If the board has a system DSP, then we want to update that instead of
     * the "normal" DSP.
     */
    if (chosen->system_dsp != NULL) {
        status = dxp_add_dsp(filename, chosen->btype, &(chosen->system_dsp));
    } else {
        status = dxp_add_dsp(filename, chosen->btype, &(chosen->dsp[modChan]));
    }

    if (status != DXP_SUCCESS) {
        sprintf(info_string,"Error loading Dsp %s",PRINT_NON_NULL(filename));
        dxp_log_error("dxp_replace_dspconfig",info_string,status);
        return status;
    }

    /* Set the dspstate value to 2, indicating that the DSP needs update */
    chosen->chanstate[modChan].dspdownloaded = 2;

    /* Download the DSP code to the IO channel and DXP channel. */

    if((status=chosen->btype->funcs->dxp_download_dspconfig(&ioChan, &modChan, chosen))!=DXP_SUCCESS) {
        sprintf(info_string,"Error downloading DSP to detector number %d",*detChan);
        dxp_log_error("dxp_replace_dspconfig",info_string,status);
        return status;
    }
    /* DSP is downloaded for this channel, set its state */
    chosen->chanstate[modChan].dspdownloaded = 1;


    return DXP_SUCCESS;
}


/*
 * Upload DSP params from designated detector channel.
 */
int XERXES_API dxp_upload_dspparams(int* detChan)
/* int *detChan;					Input: detector channel to download to	*/
{
    int status;
    int ioChan, modChan;

    /* Pointer to the chosen Board */
    Board *chosen=NULL;

    /* Get the ioChan and modChan number that matches the detChan */
    if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
        sprintf(info_string,"Error finding detector number %d",*detChan);
        dxp_log_error("dxp_upload_dspparams",info_string,status);
        return status;
    }
    ioChan = chosen->ioChan;

    /* Make sure that a params array in Board structure exists */
    if (chosen->params==NULL) {
        status = DXP_NOMEM;
        sprintf(info_string,
                "Something wrong: %d detector channel has no allocated parameter memory", *detChan);
        dxp_log_error("dxp_upload_dspparams",info_string,status);
        return status;
    }

    dxp_log_info("dxp_upload_dspparams", "Read DSP parameters from board");

    /* Loop over the parameters requested, writing each value[] to addr[]. */
    status = chosen->btype->funcs->dxp_read_dspparams(&ioChan, &modChan,
                    chosen, chosen->params[modChan]);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading parameters from detector %d", *detChan);
        dxp_log_error("dxp_upload_dspparams",info_string,status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Routine to return the length of the spectrum memory.
 *
 * For 4C-2X boards, this value is stored in the DSP and dynamic.
 * For 4C boards, it is fixed.
 */
int XERXES_API dxp_nspec(int* detChan, unsigned int* nspec)
/* int *detChan;				Input: Detector channel number				*/
/* unsigned int *nspec;			Output: The number of bins in the spectrum	*/
{
    int status=DXP_SUCCESS;
    int modChan;
    /* Pointer to the chosen Board */
    Board *chosen=NULL;

    /* Get the ioChan and modChan number that matches the detChan */
    if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
        sprintf(info_string,"Error finding detector number %d",*detChan);
        dxp_log_error("dxp_nspec",info_string,status);
        return status;
    }

    status = chosen->btype->funcs->dxp_get_spectrum_length(&chosen->ioChan,
                                                           &modChan, chosen,
                                                           nspec);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading spectrum length for detChan = %d",
                *detChan);
        dxp_log_error("dxp_nspec", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}



/*
 * Routine to return the length of the base memory.
 *
 * For 4C-2X boards, this value is stored in the DSP and dynamic.
 * For 4C boards, it is fixed.
 */
int XERXES_API dxp_nbase(int* detChan, unsigned int* nbase)
/* int *detChan;				Input: Detector channel number				*/
/* unsigned int *nbase;			Output: The number of bins in the baseline */
{
    int status;
    int modChan;


    Board *chosen = NULL;

    /* Get the ioChan and modChan number that matches the detChan */
    status = dxp_det_to_elec(detChan, &chosen, &modChan);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error finding detector number %d", *detChan);
        dxp_log_error("dxp_nbase", info_string, status);
        return status;
    }

    status = chosen->btype->funcs->dxp_get_baseline_length(&modChan, chosen,
                                                           nbase);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error getting baseline length for detChan %d",
                *detChan);
        dxp_log_error("dxp_nbase", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Routines to control the taking of data and get the data from the modules.
 * These include starting and stopping runs and reading spectra from the system.
 */

/*
 * This routine initiates a data-taking run on all DXP channels.  If *gate=0,
 * the run does not actually begin until the external gate goes to a TTL high
 * state.  If *resume=0, the MCA is first cleared; if *resume=1, the spectrum
 * is not cleared.
 */
int XERXES_API dxp_start_run(unsigned short* gate, unsigned short* resume)
/* unsigned short *gate;			Input: 0 --> use extGate 1--> ignore gate*/
/* unsigned short *resume;			Input: 0 --> clear MCA first; 1--> update*/
{

    int status = DXP_SUCCESS;
    int id     = 0;

    int ioChan, detChan, runactive;
    /* Pointer to the current Board */
    Board *current = system_head;

    /* gate value, if NULL is passed, use the XerXes stored value */
    unsigned short my_gate;

    /* Check for active runs in the system */
    runactive = 0;
    if((status=dxp_isrunning_any(&detChan, &runactive))!=DXP_SUCCESS) {
        sprintf(info_string,"Error checking run active status");
        dxp_log_error("dxp_start_run",info_string,status);
        return status;
    }
    if (runactive != 0) {
        status = DXP_RUNACTIVE;
        if ((runactive&0x1)!=0) {
            sprintf(info_string,"Must stop run in detector %d before beginning a new run.", detChan);
        } else if ((runactive&0x2)!=0) {
            sprintf(info_string,"XerXes thinks there is an active run.  Please execute dxp_stop_run().");
        } else {
            sprintf(info_string,"Unknown reason why run is active.  This should never occur.");
        }
        dxp_log_error("dxp_start_run",info_string,status);
        return status;
    }


    /* Nice simple wrapper routine to loop over all modules in the system */
    while (current!=NULL) {
        ioChan = current->ioChan;
        /* Check on gate, if not defined, pick up from XerXes */
        if (gate == NULL) {
            my_gate = (unsigned short) current->state[1];
        } else {
            my_gate = *gate;
        }
        /* Start the run */
        status = DD_FUNC(current)->dxp_begin_run(&ioChan, &allChan, &my_gate, resume,
                                                 current, &id);

        if (status != DXP_SUCCESS) {
            sprintf(info_string,"error beginning run for module %d",current->mod);
            dxp_log_error("dxp_start_run",info_string,status);
            return status;
        }

        sprintf(info_string, "Started run id = %d on all channels", id);
        dxp_log_debug("dxp_start_run", info_string);

        /* Change the system status */
        current->state[0] = 1;				/* Run is active		*/
        current->state[1] = my_gate;		/* Store gate status	*/
        /* Point to the next board */
        current = current->next;
    }

    return status;
}

/*
 * This routine initiates a data-taking run on all DXP channels.  If *gate=0,
 * the run does not actually begin until the external gate goes to a TTL high
 * state.  If *resume=0, the MCA is first cleared; if *resume=1, the spectrum
 * is not cleared.
 */
int XERXES_API dxp_start_one_run(int *detChan, unsigned short* gate, unsigned short* resume)
/* int *detChan;					Input: Detector channel to write to			*/
/* unsigned short *gate;			Input: 0 --> use extGate 1--> ignore gate*/
/* unsigned short *resume;			Input: 0 --> clear MCA first; 1--> update*/
{

    int status = DXP_SUCCESS;
    int id     = 0;

    int ioChan, modChan;		/* IO channel and module channel numbers */
    /* Pointer to the Chosen Channel */
    Board *chosen=NULL;
    /* Run status */
    int active;

    /* Translate the detector channel number to IO and channel numbers for writing. */

    if ((status  = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
        sprintf(info_string, "Failed to locate detector channel %d", *detChan);
        dxp_log_error("dxp_start_one_run", info_string, status);
        return status;
    }
    ioChan = chosen->ioChan;

    /* Determine if there is already a run active */
    if ((status  = dxp_isrunning(detChan, &active))!=DXP_SUCCESS) {
        sprintf(info_string, "Failed to determine run status of detector channel %d", *detChan);
        dxp_log_error("dxp_start_one_run", info_string, status);
        return status;
    }

    if(active == 0) {
        /* Continue with starting new run */
        ioChan = chosen->ioChan;

        status = DD_FUNC(chosen)->dxp_begin_run(&ioChan, &modChan, gate, resume,
                                                chosen, &id);

        if (status != DXP_SUCCESS) {
            sprintf(info_string,"error beginning run for module %d",chosen->mod);
            dxp_log_error("dxp_start_one_run",info_string,status);
            return status;
        }

        sprintf(info_string, "Started run id = %d on ioChan = %d", id, ioChan);
        dxp_log_debug("dxp_start_one_run", info_string);

        /* Change the system status */
        chosen->state[0] = 1;				/* Run is active		*/
        chosen->state[1] = (short) *gate;	/* Store gate status	*/
    } else if ((active&0x1)!=0) {
        status = DXP_RUNACTIVE;
        sprintf(info_string,"Run already active in module %d, please end before starting new run",
                chosen->mod);
        dxp_log_error("dxp_start_one_run",info_string,status);
    } else if ((active&0x4)!=0) {
        status = DXP_RUNACTIVE;
        sprintf(info_string,"Xerxes thinks a control task is active in module %d, please end before starting new run",
                chosen->mod);
        dxp_log_error("dxp_start_one_run",info_string,status);
    } else if ((active&0x2)!=0) {
        status = DXP_RUNACTIVE;
        sprintf(info_string,"Xerxes thinks a run is active in module %d, please end before starting new run",
                chosen->mod);
        dxp_log_error("dxp_start_one_run",info_string,status);
    } else {
        status = DXP_RUNACTIVE;
        sprintf(info_string,"Unknown reason run is active in module %d.  This should never occur.",
                chosen->mod);
        dxp_log_error("dxp_start_one_run",info_string,status);
    }

    return status;
}


/*
 * Stops runs on all DXP channels.
 */
int XERXES_API dxp_stop_one_run(int *detChan)
{

    int status;
    int ioChan;
    int modChan;


    Board *chosen = NULL;


    status = dxp_det_to_elec(detChan, &chosen, &modChan);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Failed to locate detector channel %d", *detChan);
        dxp_log_error("dxp_stop_one_run", info_string, status);
        return status;
    }

    ioChan = chosen->ioChan;

    if (chosen->state[0] == 1) {
        /* Change the run active status */
        chosen->state[0] = 0;

        status = chosen->btype->funcs->dxp_end_run(&ioChan, &modChan, chosen);

        if (status != DXP_SUCCESS) {
            sprintf(info_string,"Error ending run for ioChan = %d", ioChan);
            dxp_log_error("dxp_stop_one_run", info_string, status);
            return status;
        }
    }

    sprintf(info_string, "Stopped a run on ioChan = %d", ioChan);
    dxp_log_debug("dxp_stop_one_run", info_string);

    return DXP_SUCCESS;
}


/*
 * This routine returns non-zero if the specified module has a run active.
 *
 * Return value has bit 1 set if the board thinks a run is active
 * bit 2 is set if XerXes thinks a run is active
 */
int XERXES_API dxp_isrunning(int *detChan, int *ret)
{

    int status = DXP_SUCCESS;
    int ioChan = 0;
    int modChan = 0;
    int runactive = 0x0;

    /* Pointer to the Chosen Channel */
    Board *chosen = NULL;

    /* Initialize */
    *ret = 0;

    /* Translate the detector channel number to IO and channel numbers for writing. */

    if ((status  = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
        sprintf(info_string, "Failed to locate detector channel %d", *detChan);
        dxp_log_error("dxp_isrunning", info_string, status);
        return status;
    }
    ioChan = chosen->ioChan;

    /* Does XerXes think a run is active? */
    if (chosen->state[0] == 1) *ret = 2;
    /* Does XerXes think a control task is active? */
    if (chosen->state[4] != 0) *ret = 4;

    /* Query the hardware for its status */

    if((status=chosen->btype->funcs->dxp_run_active(&ioChan, &modChan, &runactive))!=DXP_SUCCESS) {
        sprintf(info_string,"Error checking run status for module %d",chosen->mod);
        dxp_log_error("dxp_isrunning",info_string,status);
        return status;
    }

    if (runactive == 1) *ret |= 1;

    return status;
}

/*
 * This routine returns the first detector channel that registers as running.
 * If detChan=-1, then no channel was running, ret will contain if a XerXes thinks
 * any board has an active run.
 *
 * Return value has bit 1 set if the board thinks a run is active
 * bit 2 is set if XerXes thinks a run is active
 */
int XERXES_API dxp_isrunning_any(int *detChan, int *ret)
{

    int status=DXP_SUCCESS;
    int runactive, i;
    /* Pointer to the Chosen Channel */
    Board *current=system_head;

    /* Initialize */
    *detChan = -1;
    *ret = 0;

    while (current!=NULL) {
        for (i=0; i < (int) current->nchan; i++) {
            if ((current->used&(1<<i))!=0) {
                runactive = 0;
                /* Get the run status for this detector channel */
                if((status=dxp_isrunning(&(current->detChan[i]), &runactive))!=DXP_SUCCESS) {
                    sprintf(info_string,"Error determining run status for detector %d",current->detChan[i]);
                    dxp_log_error("dxp_isrunning_any",info_string,status);
                    return status;
                }
                *ret |= runactive;
                /* Does this channel think a run is active on the physical board.  Return from the routine on the first
                 * detector that has an active run */
                if ((runactive&0x1)!=0) {
                    *detChan = current->detChan[i];
                    return status;
                }
            }
        }
        current = current->next;
    }

    return status;
}

/*
 * This routine starts a control run on the specified channel.  Control runs
 * include calibrations, sleep, adc acquisition, external memory reading, etc...
 * The data in info gives the module extra information needed to perform the
 * control task.
 */
int XERXES_API dxp_start_control_task(int *detChan, short *type,
                                      unsigned int *length, int *controlTaskInfo)
/* int *detChan;					Input: Detector channel to write to			*/
/* short *type;						Input: Type of control run, defined in .h	*/
/* unsigned int *length;	Input: Length of info array					*/
/* int *controlTaskInfo;	Input: Data needed by the control task		*/
{

    int status=DXP_SUCCESS;
    int ioChan, modChan;		/* IO channel and module channel numbers */
    /* Pointer to the Chosen Channel */
    Board *chosen=NULL;
    /* Run status */
    int active;

    dxp_log_debug("dxp_start_control_task", "Entering dxp_start_control_task()");

    /* Translate the detector channel number to IO and channel numbers for writing. */

    if ((status  = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
        sprintf(info_string, "Failed to locate detector channel %d", *detChan);
        dxp_log_error("dxp_start_control_task", info_string, status);
        return status;
    }
    ioChan = chosen->ioChan;

    /* Determine if there is already a run active */
    if ((status  = dxp_isrunning(detChan, &active))!=DXP_SUCCESS) {
        sprintf(info_string, "Failed to determine run status of detector channel %d", *detChan);
        dxp_log_error("dxp_start_control_task", info_string, status);
        return status;
    }

    if((active == 0) || ((active & 0x1) != 0)) {
        /* Continue with starting new run */
        ioChan = chosen->ioChan;
        if((status=chosen->btype->funcs->dxp_begin_control_task(&ioChan, &modChan, type,
                                                                length, controlTaskInfo, chosen))!=DXP_SUCCESS) {
            sprintf(info_string,"error beginning run for module %d",chosen->mod);
            dxp_log_error("dxp_start_control_task",info_string,status);
            return status;
        }
        /* Change the system status */
        chosen->state[0] = 1;				/* Run is active				*/

        dxp_log_debug("dxp_start_control_task", "Setting chosen->state[4]");

        chosen->state[4] = *type;			/* Store the control task type	*/
    } else if ((active&0x4)!=0) {
        status = DXP_RUNACTIVE;
        sprintf(info_string,"Xerxes thinks a control task is active in module %d, please end before starting new run",
                chosen->mod);
        dxp_log_error("dxp_start_control_task",info_string,status);
    } else if ((active&0x2)!=0) {
        status = DXP_RUNACTIVE;
        sprintf(info_string,"Xerxes thinks a run is active in module %d, please end before starting new run",
                chosen->mod);
        dxp_log_error("dxp_start_control_task",info_string,status);
    } else {
        status = DXP_RUNACTIVE;
        sprintf(info_string,"Unknown reason run is active in module %d.  This should never occur.",
                chosen->mod);
        dxp_log_error("dxp_start_control_task",info_string,status);
    }

    return status;
}

/*
 * This routine starts a control run on the specified channel.  Control runs
 * include calibrations, sleep, adc acquisition, external memory reading, etc...
 */
int XERXES_API dxp_stop_control_task(int *detChan)
/* int *detChan;					Input: Detector channel to write to			*/
/* int *type;						Input: Type of control run, defined in .h	*/
{

    int status=DXP_SUCCESS;
    int ioChan, modChan;		/* IO channel and module channel numbers */
    /* Pointer to the Chosen Channel */
    Board *chosen=NULL;

    /* Translate the detector channel number to IO and channel numbers for writing. */

    if ((status  = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
        sprintf(info_string, "Failed to locate detector channel %d", *detChan);
        dxp_log_error("dxp_stop_control_task", info_string, status);
        return status;
    }
    ioChan = chosen->ioChan;

    /* Stop the control task */
    if((status=chosen->btype->funcs->dxp_end_control_task(&ioChan, &modChan, chosen))!=DXP_SUCCESS) {
        sprintf(info_string,"Error stopping control task for module %d",chosen->mod);
        dxp_log_error("dxp_stop_control_task",info_string,status);
        return status;
    }

    /* Change the system status */
    chosen->state[0] = 0;				/* Run is no longer active		*/
    chosen->state[4] = 0;				/* Clear the control task value */

    return status;
}

/*
 * This routine retrieves information about the specified control task.  It
 * returns the information in a 20 word array.  The information can vary from
 * control task type to type.  First two words are always length of data to be
 * returned and estimated execution time in milliseconds.
 */
int XERXES_API dxp_control_task_info(int *detChan, short *type, int *controlTaskInfo)
/* int *detChan;					Input: Detector channel to write to					*/
/* short *type;						Input: Type of control run, defined in .h			*/
/* int *controlTaskInfo;	Output: Array of information about the control task */
{

    int status=DXP_SUCCESS;
    int ioChan, modChan;		/* IO channel and module channel numbers */
    /* Pointer to the Chosen Channel */
    Board *chosen=NULL;

    /* Translate the detector channel number to IO and channel numbers for writing. */

    if ((status  = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
        sprintf(info_string, "Failed to locate detector channel %d", *detChan);
        dxp_log_error("dxp_control_task_info", info_string, status);
        return status;
    }
    ioChan = chosen->ioChan;

    /* Get information about this control task */
    if((status=chosen->btype->funcs->dxp_control_task_params(&ioChan, &modChan, type, chosen, controlTaskInfo))!=DXP_SUCCESS) {
        sprintf(info_string,"Error retrieving information about control task %d for detector %d",*type,*detChan);
        dxp_log_error("dxp_control_task_info",info_string,status);
        return status;
    }

    return status;
}

/*
 * This routine retrieves information about the specified control task.
 */
int XERXES_API dxp_get_control_task_data(int *detChan, short *type, void *data)
/* int *detChan;					Input: Detector channel to write to			*/
/* short *type;					Input: Type of control run, defined in .h	*/
/* void *data;						Output: Data read back from the module		*/
{

    int status=DXP_SUCCESS;
    int ioChan, modChan;		/* IO channel and module channel numbers */
    /* Pointer to the Chosen Channel */
    Board *chosen=NULL;
    /* Run status */
    int active;

    /* Translate the detector channel number to IO and channel numbers for writing. */

    if ((status  = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
        sprintf(info_string, "Failed to locate detector channel %d", *detChan);
        dxp_log_error("dxp_get_control_task_data", info_string, status);
        return status;
    }
    ioChan = chosen->ioChan;

    /* Determine if there is already a run active */
    if ((status  = dxp_isrunning(detChan, &active))!=DXP_SUCCESS) {
        sprintf(info_string, "Failed to determine run status of detector channel %d", *detChan);
        dxp_log_error("dxp_get_control_task_data", info_string, status);
        return status;
    }

    sprintf(info_string, "active = %#x", active);
    dxp_log_debug("dxp_get_control_task_data", info_string);

    if ((active == 0)||(active==4) || (active == 2)) {
        /* Continue with starting new run */
        if((status=chosen->btype->funcs->dxp_control_task_data(&ioChan, &modChan, type, chosen, data))!=DXP_SUCCESS) {
            sprintf(info_string,"error beginning run for module %d",chosen->mod);
            dxp_log_error("dxp_get_control_task_data",info_string,status);
            return status;
        }
    } else if ((active&0x1)!=0) {
        status = DXP_RUNACTIVE;
        sprintf(info_string,"Run active in module %d, run must end before reading control task data",
                chosen->mod);
        dxp_log_error("dxp_get_control_task_data",info_string,status);
        /*  } else if ((active&0x2)!=0) { */
        /*
        status = DXP_RUNACTIVE;
        sprintf(info_string,"Xerxes thinks a run is active in module %d, please end before reading data",
        		chosen->mod);
        dxp_log_error("dxp_get_control_task_data",info_string,status);
        */
    } else {
        status = DXP_RUNACTIVE;
        sprintf(info_string,"Unknown reason run is active in module %d.  This should never occur.",
                chosen->mod);
        dxp_log_error("dxp_get_control_task_data",info_string,status);
    }

    return status;
}

/*
 * Get the index of the symbol defined by name, from within the list of
 * DSP parameters.  This index can be used to reference the symbol value
 * in the parameter array returned by various routines
 */
int XERXES_API dxp_get_symbol_index(int* detChan, char* name, unsigned short* symindex)
/* int *detChan;				Input: Detector channel to write to			*/
/* char *name;					Input: user passed name of symbol			*/
/* unsigned short *symindex;	Output: Index of the symbol within parameter array	*/
{

    int status;
    int modChan;		/* IO channel and module channel numbers */
    /* Pointer to the Chosen Channel */
    Board *chosen=NULL;

    /* Translate the detector channel number to IO and channel numbers for writing. */
    if ((status  = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS)
    {
        sprintf(info_string, "Failed to locate detector channel %d", *detChan);
        dxp_log_error("dxp_get_symbol_index", info_string, status);
        return status;
    }

    /* Retrieve the index number */

    if((status=chosen->btype->funcs->dxp_loc(name, chosen->dsp[modChan], symindex))!=DXP_SUCCESS)
    {
        sprintf(info_string, "Error searching for parameter %s", name);
        dxp_log_error("dxp_get_symbol_index",info_string,status);
        return status;
    }

    return status;

}

/*
 * Get a parameter value from the DSP .  Given a symbol name, return its
 * value from a given detector number.
 */
int XERXES_API dxp_get_one_dspsymbol(int* detChan, char* name, unsigned short* value)
/* int *detChan;					Input: Detector channel to write to			*/
/* char *name;						Input: user passed name of symbol			*/
/* unsigned short *value;			Output: returned value of the parameter		*/
{

    int status;
    int ioChan, modChan;		/* IO channel and module channel numbers */
    /* Pointer to the Chosen Channel */
    Board *chosen=NULL;
    /* Temporary Storage for the double return value */
    double dtemp;

    /* Translate the detector channel number to IO and channel numbers for writing. */

    if ((status  = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS)
    {
        sprintf(info_string, "Failed to locate detector channel %d", *detChan);
        dxp_log_error("dxp_get_one_dspsymbol", info_string, status);
        return status;
    }
    ioChan = chosen->ioChan;


    /* Read the value of the symbol from DSP memory */

    if((status=chosen->btype->funcs->dxp_read_dspsymbol(&ioChan, &modChan,
                                                        name, chosen, &dtemp))!=DXP_SUCCESS)
    {
        sprintf(info_string, "Error reading parameter %s", name);
        dxp_log_error("dxp_get_one_dspsymbol",info_string,status);
        return status;
    }

    /* Convert to short */
    if (dtemp > (double) USHRT_MAX)
    {
        sprintf(info_string, "This routine is deprecated for reading parameters >16bits \nPlease use dxp_get_dspsymbol()");
        dxp_log_error("dxp_get_one_dspsymbol",info_string,status);
        return status;
    }
    *value = (unsigned short) dtemp;

    return status;

}

/*
 * Set a parameter of the DSP.  Pass the symbol name, value to set and detector
 * channel number.
 */
int XERXES_API dxp_set_one_dspsymbol(int* detChan, char* name, unsigned short* value)
/* int *detChan;				Input: Detector channel to write to			*/
/* char *name;					Input: user passed name of symbol			*/
/* unsigned short *value;		Input: Value to set the symbol to			*/
{

    int status;
    int ioChan, modChan;		/* IO channel and module channel numbers */
    /* Pointer to the Chosen Channel */
    Board *chosen=NULL;
    int runstat = 0;

    /* Translate the detector channel number to IO and channel numbers for writing. */

    if ((status  = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS)
    {
        sprintf(info_string, "Failed to locate detector channel %d", *detChan);
        dxp_log_error("dxp_set_one_dspsymbol", info_string, status);
        return status;
    }
    ioChan = chosen->ioChan;

    /* Check if there is a run in progress on this board */
    if ((status = dxp_isrunning(detChan, &runstat))!=DXP_SUCCESS)
    {
        sprintf(info_string, "Failed to determine the run status of detChan %d", *detChan);
        dxp_log_error("dxp_set_one_dspsymbol", info_string, status);
        return status;
    }

    if (runstat!=0)
    {
        status = DXP_RUNACTIVE;
        sprintf(info_string, "You must stop the run before modifying DSP parameters for detChan %d"
                ", runstat = %#x", *detChan, runstat);
        dxp_log_error("dxp_set_one_dspsymbol", info_string, status);
        return status;
    }

    /* Write the value of the symbol into DSP memory */

    if((status=chosen->btype->funcs->dxp_modify_dspsymbol(&ioChan, &modChan,
                                                          name, value, chosen))!=DXP_SUCCESS)
    {
        sprintf(info_string, "Error writing parameter %s", PRINT_NON_NULL(name));
        dxp_log_error("dxp_set_one_dspsymbol",info_string,status);
        return status;
    }

    return status;
}

/*
 * Returns the DSP parameter name located at the specified index.
 *
 * name needs to be at least MAX_DSP_PARAM_NAME_LEN characters long.
 */
int XERXES_API dxp_symbolname_by_index(int* detChan, unsigned short *lindex,
                                       char* name)
{
    int status;
    int modChan;


    Board *chosen = NULL;


    if (!detChan) {
        dxp_log_error("dxp_symbolname_by_index", "Passed in 'detChan' reference is "
                      "NULL", DXP_NULL);
        return DXP_NULL;
    }

    if (!lindex) {
        sprintf(info_string, "Passed in 'lindex' reference is NULL for detChan %d",
                *detChan);
        dxp_log_error("dxp_symbolname_by_index", info_string, DXP_NULL);
        return DXP_NULL;
    }

    if (!name) {
        sprintf(info_string, "Passed in 'name' string is NULL for detChan %d",
                *detChan);
        dxp_log_error("dxp_symbolname_by_index", info_string, DXP_NULL);
        return DXP_NULL;
    }

    status = dxp_det_to_elec(detChan, &chosen, &modChan);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Failed to locate detector channel %d", *detChan);
        dxp_log_error("dxp_symbolname_by_index", info_string, status);
        return status;
    }

    status = chosen->btype->funcs->dxp_get_symbol_by_index(modChan, *lindex, chosen,
                                                           name);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error getting name of symbol located at index %u for "
                "detChan %d", *lindex, *detChan);
        dxp_log_error("dxp_symbolname_by_index", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Routine to return a list of DSP symbol information to the user.
 *
 * The user must allocate the memory for the list of read-write codes,
 * lower and uppper bounds.  If dynamic allocation is desired, then a
 * call to dxp_num_symbols() should be made and
 * the memory allocated accordingly.
 */
int XERXES_API dxp_symbolname_limits(int* detChan, unsigned short *access,
                                     unsigned short *lbound, unsigned short *ubound)
/* int *detChan;				Input: detector channel number						*/
/* unsigned short *access;		Output: Access information for each DSP parameter	*/
/* unsigned short *lbound;		Output: lower bound for each DSP parameter			*/
/* unsigned short *ubound;		Output: upper bound for each DSP parameter			*/
{
    int status=DXP_SUCCESS;
    int modChan;
    unsigned short i;
    /* Pointer to the Chosen Channel */
    Board *chosen=NULL;

    /* Translate the detector channel number to IO and channel numbers for writing. */

    if ((status = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS)
    {
        sprintf(info_string, "Failed to locate detector channel %d", *detChan);
        dxp_log_error("dxp_symbolname_limits", info_string, status);
        return status;
    }

    /* Was Memory allocated for the access list? */
    if (access != NULL)
    {
        /* Copy the list of access information */
        for (i=0; i < chosen->dsp[modChan]->params->nsymbol ; i++)
            access[i] = chosen->dsp[modChan]->params->parameters[i].access;
    }

    /* Was Memory allocated for the lower bounds list? */
    if (lbound != NULL)
    {
        /* Copy the list of lower bounds information */
        for (i=0; i < chosen->dsp[modChan]->params->nsymbol; i++)
            lbound[i] = chosen->dsp[modChan]->params->parameters[i].lbound;
    }

    /* Was Memory allocated for the upper bounds list? */
    if (ubound != NULL)
    {
        /* Copy the list of upper bounds information */
        for (i=0; i < chosen->dsp[modChan]->params->nsymbol; i++)
            ubound[i] = chosen->dsp[modChan]->params->parameters[i].ubound;
    }

    return status;
}


/*
 * Returns the number of DSP parameters for the specificed channel.
 */
XERXES_EXPORT int XERXES_API dxp_max_symbols(int* detChan,
                                             unsigned short* nsymbols)
{
    int status;
    int modChan;


    Board *chosen = NULL;


    if (!detChan) {
        dxp_log_error("dxp_max_symbols", "'detChan' can not be NULL",
                      DXP_NULL);
        return DXP_NULL;
    }

    if (!nsymbols) {
        sprintf(info_string, "'nsymbols' can not be NULL trying to "
                "get the number of DSP parameters for detChan = %d", *detChan);
        return DXP_NULL;
    }

    status = dxp_det_to_elec(detChan, &chosen, &modChan);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Failed to locate detector channel %d", *detChan);
        dxp_log_error("dxp_max_symbols", info_string, status);
        return status;
    }

    status = chosen->btype->funcs->dxp_get_num_params(modChan, chosen, nsymbols);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error getting number of parameters for detChan = %d",
                *detChan);
        dxp_log_error("dxp_max_symbols", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Readout the DSP parameter values, baseline memory and the MCA spectrum a
 * single channel. Baseline and MCA data are returned as 32-bit words. (On
 * hardware that only supports 16-bit baseline data, the upper 16-bit word is
 * ignored.)
 */
XERXES_EXPORT int XERXES_API dxp_readout_detector_run(int* detChan,
                                                      unsigned short params[],
                                                      unsigned long baseline[],
                                                      unsigned long spectrum[])
/* int *detChan;						Input: detector channel number      */
/* unsigned short params[];				Output: parameter memory            */
/* unsigned short baseline[];			Output: baseline histogram          */
/* unsigned long spectrum[];			Output: spectrum                    */
{

    int status;

    int modChan;
    /* Pointer to the Chosen Board */
    Board *chosen=NULL;

    /* Get the ioChan and modChan number that matches the detChan */

    if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
        sprintf(info_string,"Error finding detector number %d",*detChan);
        dxp_log_error("dxp_readout_detector_run",info_string,status);
        return status;
    }
    
    /* wrapper for the real readout routine */
    status = dxp_do_readout(chosen, &modChan, params, baseline, spectrum);

    if(status != DXP_SUCCESS) {
        dxp_log_error("dxp_readout_detector_run","error reading out run data",status);
        return status;
    }

    return status;
}


/*
 * This routine reads out the paramter memory, the baseline memory and the
 * MCA spectrum for a single DXP channel.  The error words in the parameter
 * memory are first decoded and checked.  If OK, the baseline and spectrum
 * memories are read, otherwise the error is cleared and this routine is
 * exited.  Note: spectrum words are 32 bits, wheras params and baseline are
 * only 16 bits.
 */
static int dxp_do_readout(Board* board, int* modChan, unsigned short params[],
                          unsigned long baseline[], unsigned long spectrum[])
{
    int total_syms = 0;
    unsigned int i;
    int status;
    unsigned short runerror,errinfo;


    /* If a System DSP is defined, used that instead of the normal DSP. */
    if (params != NULL) {

        /* Read out the parameters from the DSP memory */
        status = board->btype->funcs->dxp_read_dspparams(&(board->ioChan),
                                                         modChan, board,
                                                         board->params[*modChan]);

        if (status != DXP_SUCCESS)
        {
            dxp_log_error("dxp_do_readout","error reading parameters",status);
            return status;
        }


        if (board->system_dsp != NULL) {
            total_syms = board->system_dsp->params->nsymbol +
                         board->system_dsp->params->n_per_chan_symbols;

        } else {
            total_syms = board->dsp[*modChan]->params->nsymbol;
        }

        for (i = 0; i < (unsigned int)total_syms; i++) {
            params[i] = board->params[*modChan][i];
        }

    }

    /* Pull out the RUNERROR and ERRINFO parameters from the DSP parameter list
     * and act on it accordingly */
    status = board->btype->funcs->dxp_decode_error(&(board->ioChan), modChan,
                                                   board->dsp[*modChan],
                                                   &runerror, &errinfo);
    if (status != DXP_SUCCESS)
    {
        dxp_log_error("dxp_do_readout","Error decoding error information",status);
        return status;
    }

    if (runerror != 0) {
        /* Continue */
        sprintf(info_string, "RUNERROR = %hu ERRINFO = %hu", runerror, errinfo);
        dxp_log_error("dxp_do_readout", info_string, DXP_DSPRUNERROR);

        status = board->btype->funcs->dxp_clear_error(&(board->ioChan), modChan,
                                                      board);
        if (status != DXP_SUCCESS) {
            dxp_log_error("dxp_do_readout","Error clearing error",status);
            return status;
        }

        return DXP_DSPRUNERROR;
    }

    /* Read out the spectrum memory now (also known as MCA memory) */

    /* Only read spectrum if there is allocated memory */
    if (spectrum!=NULL)
    {
        status = board->btype->funcs->dxp_read_spectrum(&(board->ioChan),modChan,
                                                        board, spectrum);
        if (status != DXP_SUCCESS)
        {
            dxp_log_error("dxp_do_readout", "Error reading out spectrum", status);
            return status;
        }
    }

    /* Read out the basline histogram. */

    /* Only read baseline if there is allocated memory */
    if (baseline!=NULL)
    {
        status = board->btype->funcs->dxp_read_baseline(&(board->ioChan),modChan,
                                                        board, baseline);
        if (status != DXP_SUCCESS)
        {
            dxp_log_error("dxp_do_readout","error reading out baseline",status);
            return status;
        }
    }

    return status;
}


/*
 * Returns the electronic channel numbers (e.g. module number and channel
 * within a module) for a given detector channel.
 */
int XERXES_API dxp_det_to_elec(int* detChan, Board** passed, int* dxpChan)
/* int *detChan;						Input: detector channel               */
/* Board **passed;						Output: Pointer to the correct Board   */
/* int *dxpChan;						Output: DXP channel number             */
{

    unsigned int chan;
    int status;

    /* Point the current Board to the first Board in the system */
    Board *current = system_head;

    /* Loop through the Boards to find which module we desire */

    while (current != NULL) {
        for (chan=0; chan<current->nchan; chan++) {
            if (*detChan==current->detChan[chan]) {
                *dxpChan = chan;
                *passed = current;
                return DXP_SUCCESS;
            }
        }
        current = current->next;
    }

    *passed = NULL;
    sprintf(info_string,"detector channel %d is unknown",*detChan);
    status=DXP_NODETCHAN;
    dxp_log_error("dxp_det_to_elec",info_string,status);
    return status;
}

/*
 * Returns the electronic channel numbers (e.g. module number and channel
 * within a module) for a given detector channel.
 */
int XERXES_API dxp_elec_to_det(int* ioChan, int* dxpChan, int* detChan)
/* int *ioChan;						Input: I/O channel number				*/
/* int *dxpChan;					Input: DXP channel number			*/
/* int *detChan;					Output: detector channel			*/
{

    int status;
    /* Pointer to current Board */
    Board *current=system_head;

    /* Only support up to 4 channels per module. */

    if (((unsigned int) *dxpChan) >= current->nchan) {
        sprintf(info_string,"No support for %d channels per module",*dxpChan);
        status=DXP_NOMODCHAN;
        dxp_log_error("dxp_elec_to_det",info_string,status);
        return status;
    }

    /* Now count forward through the module and channel list till we
     * find the matching detector number */

    while (current!=NULL) {
        if (*ioChan==(current->ioChan)) {
            *detChan = current->detChan[*dxpChan];
            return DXP_SUCCESS;
        }
        current = current->next;
    }

    /* Should never exit loop normally => no match */

    sprintf(info_string,"IO Channel %d unknown",*ioChan);
    status=DXP_NOIOCHAN;
    dxp_log_error("dxp_elec_to_det",info_string,status);
    return status;
}


/*
 * Reads out an arbitrary block of memory
 *
 * Parses a name of the form: '[memory type]:[offset (hex)]:length'
 * where 'memory type' is a product specific item.
 */
XERXES_EXPORT int dxp_read_memory(int *detChan, char *name, unsigned long *data)
{
    int status;
    int modChan;

    unsigned long base;
    unsigned long offset;

    char type[MAX_MEM_TYPE_LEN];

    Board *chosen = NULL;


    ASSERT(detChan != NULL);
    ASSERT(name != NULL);
    ASSERT(data != NULL);


    status = dxp_parse_memory_str(name, type, &base, &offset);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error parsing memory string '%s'", name);
        dxp_log_error("dxp_read_memory", info_string, status);
        return status;
    }

    status = dxp_det_to_elec(detChan, &chosen, &modChan);

    if (status != DXP_SUCCESS) {

        sprintf(info_string, "Failed to locate detector channel %d", *detChan);
        dxp_log_error("dxp_read_memory", info_string, status);
        return status;
    }

    status = chosen->btype->funcs->dxp_read_mem(&(chosen->ioChan), &modChan,
                                                chosen, type, &base, &offset, data);

    if (status != DXP_SUCCESS) {

        sprintf(info_string, "Error reading memory of type %s from detector channel %d",
                name, *detChan);
        dxp_log_error("dxp_read_memory", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Writes to an arbitrary block of memory
 */
XERXES_EXPORT int dxp_write_memory(int *detChan, char *name, unsigned long *data)
{
    int status;
    int modChan;

    unsigned long base;
    unsigned long offset;

    char type[MAX_MEM_TYPE_LEN];

    Board *chosen = NULL;


    ASSERT(detChan != NULL);
    ASSERT(name != NULL);
    ASSERT(data != NULL);


    status = dxp_parse_memory_str(name, type, &base, &offset);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error parsing memory string '%s'", name);
        dxp_log_error("dxp_write_memory", info_string, status);
        return status;
    }

    status = dxp_det_to_elec(detChan, &chosen, &modChan);

    if (status != DXP_SUCCESS) {

        sprintf(info_string, "Failed to locate detector channel %d", *detChan);
        dxp_log_error("dxp_write_memory", info_string, status);
        return status;
    }

    status = chosen->btype->funcs->dxp_write_mem(&(chosen->ioChan), &modChan,
                                                 chosen, type, &base, &offset, data);

    if (status != DXP_SUCCESS) {

        sprintf(info_string, "Error writing memory of type %s to detector channel %d",
                name, *detChan);
        dxp_log_error("dxp_write_memory", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * This routine allows write access to certain
 * registers.
 */
int dxp_write_register(int *detChan, char *name, unsigned long *data)
{
    int status;
    int modChan;


    Board *chosen = NULL;


    status = dxp_det_to_elec(detChan, &chosen, &modChan);

    if (status != DXP_SUCCESS) {

        sprintf(info_string, "Failed to locate detector channel %d", *detChan);
        dxp_log_error("dxp_write_register", info_string, status);
        return status;
    }

    status = chosen->btype->funcs->dxp_write_reg(&(chosen->ioChan), &modChan,
                                                 name, data);

    if (status != DXP_SUCCESS) {

        sprintf(info_string, "Error writing to register for detector channel %d",
                *detChan);
        dxp_log_error("dxp_write_register", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * This routine allows read access to certain
 * registers. The allowed registers are completely
 * defined by the individual device drivers.
 */
int dxp_read_register(int *detChan, char *name, unsigned long *data)
{
    int status;
    int modChan;


    Board *chosen = NULL;


    status = dxp_det_to_elec(detChan, &chosen, &modChan);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Failed to locate detector channel %d", *detChan);
        dxp_log_error("dxp_read_register", info_string, status);
        return status;
    }

    status = chosen->btype->funcs->dxp_read_reg(&(chosen->ioChan), &modChan,
                                                name, data);

    if (status != DXP_SUCCESS) {
        sprintf(info_string,
                "Error reading from register on detector channel %d",
                *detChan);
        dxp_log_error("dxp_read_register", info_string, status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * This routine allows access to the defined
 * command routine for a given product. Not
 * all products have a meaningful implementation
 * of the command routine.
 */
XERXES_EXPORT int XERXES_API dxp_cmd(int *detChan, byte_t *cmd, unsigned int *lenS,
                                     byte_t *send, unsigned int *lenR, byte_t *receive)
{
    int status;
    int modChan;

    Board *chosen = NULL;

    status = dxp_det_to_elec(detChan, &chosen, &modChan);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Failed to locate detector channel %d", *detChan);
        dxp_log_error("dxp_cmd", info_string, status);
        return status;
    }

    /* Only works for supported devices */
    if (chosen->btype->funcs->dxp_do_cmd) {
        status = chosen->btype->funcs->dxp_do_cmd(modChan, chosen, *cmd, *lenS,
                                                  send, *lenR, receive);

        if (status != DXP_SUCCESS) {
            dxp_log_error("dxp_cmd", "Command error", status);
            return status;
        }
    } else {
        dxp_log_debug("dxp_cmd", "This function is not implemented for current device");
    }
    
    return DXP_SUCCESS;
}


/*
 * Sets the current I/O priority of the library
 */
XERXES_EXPORT int XERXES_API dxp_set_io_priority(int *priority)
{
    int status;


    if (priority == NULL) {
        dxp_log_error("dxp_set_io_priority", "'priority' may not be NULL", DXP_NULL);
        return DXP_NULL;
    }

    status = xerxes_md_set_priority(priority);

    if (status != DXP_SUCCESS) {
        dxp_log_error("dxp_set_io_priority", "Error setting I/O priority", status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Calls the appropriate exit handler for the specified
 * board type.
 */
XERXES_EXPORT int XERXES_API dxp_exit(int *detChan)
{
    int status;
    int modChan;


    Board *chosen = NULL;

    sprintf(info_string, "Preparing to unhook detChan = %d", *detChan);
    dxp_log_debug("dxp_exit", info_string);

    status = dxp_det_to_elec(detChan, &chosen, &modChan);

    /* If the detChan is invalid, this could be symptomatic of a problem
     * in the calling library. Perhaps it's list of detChan got corrupted, etc.
     * The fact that it is trying to call dxp_exit() could mean that it is trying
     * to recover, so we need to be as permissive as possible here. That means
     * we only return a warning if we encounter a bad detChan.
     */
    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Failed to locate detector channel %d", *detChan);
        dxp_log_warning("dxp_exit", info_string);
        return DXP_SUCCESS;
    }

    status = chosen->btype->funcs->dxp_unhook(chosen);

    if (status != DXP_SUCCESS) {
        dxp_log_error("dxp_exit", "Unable to un-hook board communications", status);
        return status;
    }

    return DXP_SUCCESS;
}


/*
 * Takes a memory string and returns the parsed data
 *
 * Expects that memory has been allocated for 'type'. Expects strings
 * of the format '[memory type]:[base (hex)]:[offset]'.
 * See also: dxp_read_memory().
 */
XERXES_STATIC int XERXES_API dxp_parse_memory_str(char *name, char *type, unsigned long *base, unsigned long *offset)
{
    size_t name_len = strlen(name) + 1;

    int n = 0;

    char *mem_delim = ":";
    char *full_name = NULL;
    char *tok       = NULL;



    ASSERT(name != NULL);
    ASSERT(type != NULL);
    ASSERT(base != NULL);
    ASSERT(offset != NULL);


    full_name = (char *)xerxes_md_alloc(name_len);

    if (full_name == NULL) {
        sprintf(info_string, "Error allocating %zu bytes for 'full_name'", name_len);
        dxp_log_error("dxp_parse_memory_str", info_string, DXP_NOMEM);
        return DXP_NOMEM;
    }

    strncpy(full_name, name, name_len);

    tok = strtok(full_name, mem_delim);

    if (tok == NULL) {
        sprintf(info_string, "Memory string '%s' is improperly formatted", name);
        dxp_log_error("dxp_parse_memory_str", info_string, DXP_INVALID_STRING);
        return DXP_INVALID_STRING;
    }

    strncpy(type, tok, strlen(tok) + 1);

    tok = strtok(NULL, mem_delim);

    if (tok == NULL) {
        sprintf(info_string, "Memory string '%s' is improperly formatted", name);
        dxp_log_error("dxp_parse_memory_str", info_string, DXP_INVALID_STRING);
        return DXP_INVALID_STRING;
    }

    n = sscanf(tok, "%lx", base);

    if (n == 0) {
        sprintf(info_string, "Memory base '%s' is improperly formatted", tok);
        dxp_log_error("dxp_parse_memory_str", info_string, DXP_INVALID_STRING);
        return DXP_INVALID_STRING;
    }

    tok = strtok(NULL, mem_delim);

    if (tok == NULL) {
        sprintf(info_string, "Memory string '%s' is improperly formatted", name);
        dxp_log_error("dxp_parse_memory_str", info_string, DXP_INVALID_STRING);
        return DXP_INVALID_STRING;
    }

    n = sscanf(tok, "%lu", offset);

    if (n == 0) {
        sprintf(info_string, "Memory offset '%s' is improperly formatted", tok);
        dxp_log_error("dxp_parse_memory_str", info_string, DXP_INVALID_STRING);
        return DXP_INVALID_STRING;
    }

    xerxes_md_free(full_name);
    full_name = NULL;

    return DXP_SUCCESS;
}
