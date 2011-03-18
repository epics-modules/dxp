/*
 * Copyright (c) 2004 X-ray Instrumentation Associates
 *               2005-2010 XIA LLC
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
 *
 * $Id$
 *
 */


#ifndef XIA_HANDEL_STRUCTURES_H
#define XIA_HANDEL_STRUCTURES_H

#include "handel_generic.h"
#include "xia_common.h"

/** Constants **/

/* These are for the DetChanElements */
#define SINGLE		0
#define SET			1


/** Structs **/

/**********
 * Represents an individual channel in a module.
 * Eventually, most of the module items will
 * be moved to this structure.
 **********/
typedef struct _Channel {
  unsigned short n_sca;
  unsigned short *sca_lo;
  unsigned short *sca_hi;

} Channel_t;


/*
 * Linked-list that defines either a single detChan or
 * a detChan set. The Tag field is used to detect
 * cycles in the list since a detChan set can point
 * to other points, etc...
 */
struct DetChanElement {
    /* Type (either SINGLE or SET) used to determine which
     * element of the union to access.
     */
    int type;

    /*
     * The detChan # itself, believe it or not
     */
    int detChan;

    /* A union that contains either the alias of the
     * module this element "refers" to (in the case
     * of a SINGLE) or a pointer to a list of
     * other detChan elements.
     */
    union {

	char *modAlias;
	struct DetChanSetElem *detChanSet;

    } data;
    /* A flag to tell, when iterating, if we have a 
     * cycle in the LL
     */
    boolean_t isTagged;

    struct DetChanElement *next;
};
typedef struct DetChanElement DetChanElement;


/*
 * Linked-list for the individual elements of a DetChanSet list
 */
struct DetChanSetElem {
	
    unsigned int channel;

    struct DetChanSetElem *next;
};
typedef struct DetChanSetElem DetChanSetElem;


/*
 * Linked list containing the default DAQ entries. 
 * name-value pairs only.
 */
typedef struct _XiaDaqEntry {
  /* String containing the DAQ entry settting name */
  char *name;

  /* DAQ entry value */
  double data;

  /* For products using the 'apply' model, this is new value */
  double pending;

  /* Value state information. Not used by all products. */
  flag_t state;
  
  /* Pointer to the next entry */
  struct _XiaDaqEntry *next;

} XiaDaqEntry; 


/*
 * Linked list containing the default DAQ settings
 * retained for the HanDeL library.  name-value pairs only.
 */
struct XiaDefaults {
    /* Filename identifying this entry */
    char *alias;
    /* Linked list of DAQ entries */
    struct _XiaDaqEntry *entry;
    /* Pointer to the next entry */
    struct XiaDefaults *next;
};
typedef struct XiaDefaults XiaDefaults;


/* 
 * Define the linked list used to track peaking time ranges for firmware
 * definitions.  Peaking times are specified in nanoseconds. 
 */
struct Firmware {
    /* Peaking Time Range Reference */
    unsigned short ptrr;
    /* Min Peaking time */
    double min_ptime;
    /* Max Peaking time */
    double max_ptime;
    /* Point to the fippi for this definition */
    char *fippi;
    /* Point to the user_fippi for this definition */
    char *user_fippi;
    /* Point to the Dsp_Info for this definition */
    char *dsp;
    /* Number of filter parameters */
    unsigned short numFilter;
    /* Array of filter parameters */
    parameter_t *filterInfo;

    struct Firmware *next;
    struct Firmware *prev;
};
typedef struct Firmware Firmware;


/* 
 * Define a linked list of firmware sets.  These are 'sets' of peaking
 * time definitions that can be referenced within the board structure
 * to allow boards to have arbitrary firmware definitions for arbitrary 
 * board combinations.
 */
struct FirmwareSet {
    /* Give this set a name as a reference */
    char *alias;

    /* This is the filename of the FDF file provided by XIA */
    char *filename;

    /* This is the string array of keywords associated with the FDD file */
    char **keywords;

    /* This is the number of keywords. Mainly used as an aid in freeing the 
     * structure
     */
    unsigned int numKeywords;

  /* Temporary directory where the expanded firmware files are stored.
   * Can be NULL.
   */
  char *tmpPath;

    /* Point to the single MMU possible for each processor */
    char *mmu;

    /* Point to an array of Firmware structures */
    struct Firmware *firmware;

    /* Point to the next element of the linked list */
    struct FirmwareSet *next;
};
typedef struct FirmwareSet FirmwareSet;


/* 
 * Define a linked list of Detectors.
 */
struct Detector {
    /* Give this set a name as a reference */
    char *alias;

    /* Number of channels for this detector */
    unsigned short nchan;

    /* Array of polarities for all channels
     * 1 = positive
     * 0 = negative 
     */
    unsigned short *polarity;

    /* Array of preamp gains for the channels in 
     * mv/KeV.
     */
    double *gain;

    /* The type (Reset, RC Feedback, etc...) */
    unsigned short type;

    /* The type specific value such as RESETINT 
     * associated with the type.
     */
    double *typeValue;

    struct Detector *next;
};
typedef struct Detector Detector;


struct CurrentFirmware
{
  /* The current fippi that is being used. This exists so that
   * we can check to see if we are running with the same fippi
   * or not before downloading it again when the user does things
   * like change peaking time.
   */
  char currentFiPPI[MAXFILENAME_LEN];

  /* The current "user fippi" that is being used. */
  char currentUserFiPPI[MAXFILENAME_LEN];
 
  /* The current dsp that is being used. */
  char currentDSP[MAXFILENAME_LEN];

  /* The current "user dsp" that is being used. */
  char currentUserDSP[MAXFILENAME_LEN];

  /* The current "mmu" that is being used. */
  char currentMMU[MAXFILENAME_LEN];

  /* Not all products support a system FPGA */
  char currentSysFPGA[MAXFILENAME_LEN];

  /* Not all products support a system FiPPI */
  char currentSysFiPPI[MAXFILENAME_LEN];
};
typedef struct CurrentFirmware CurrentFirmware;


/**********
 * This structure holds information about the status
 * of multichannel operations.
 **********/
typedef struct _MultiChannelState {
    boolean_t *runActive;

    /* Add more here as necessary */

} MultiChannelState;


/*
 * Define a struct of linked-lists for the module information
 */
struct Module {
    /* Logical, human-readable, name for this module. Referenced/Set in
     * the .ini file.
     */
    char *alias;

    /* Hardware type for this module. */
    char *type;

    /* The communication interface */
    struct HDLInterface *interface_info;

    /* Number of channels for this module, determined by the hardware
     * type. Also used to distinguish between hardware types that only
     * differ by the number of channels they have.
     */
    unsigned int number_of_channels;

    /* Eventually, these "channels" will hold most of the information
     * that is currently in the structure.
     */
    Channel_t *ch;

    /* Array of detChan values for this module. The indices of this
     * array map the logical module channel to its detChan. Set an
     * element to -1 to disable the channel.
     */
    int *channels;

    /* An array of strings mapping logical module channels to detector
     * alias + physical detector channel. The format is defined as
     * {detector alias}:{detector channel}.
     */
    char **detector;

    /* Mapping of physical detector channel to logical module channel.
     */
    int *detector_chan;

    /* Array of strings mapping logical module channels to firmware
     * alias.
     */
    char **firmware;

    /* Array of strings mapping logical module channels to default
     * (acquisition value) alias.
     */
    char **defaults;

    /* An array of CurrentFirmware elements mapping logical module
     * channels to their running firmware.
     */
    CurrentFirmware *currentFirmware;

    /* Multi-channel modules could be validated more then once, which
     * is a waste of resources. This flag is set once the module is
     * validated and unset when xiaStartSystem() is called.
     */
    boolean_t isValidated;

    /* Used to differentiate between single- and non-single-channel
     * modules in Handel, which does not know enough about the
     * different products to make the decision based on the number of
     * channels.
     */
    boolean_t isMultiChannel;

    MultiChannelState *state;

    /* Indicates if the user setup operations have been applied to this 
     * module or not.
     */
    boolean_t isSetup;

    struct Module *next;
}; 
typedef struct Module Module;



/*
 * If this were an object-program, then Interface would be the base
 * class, but...in this case I use a union to make Interface as 
 * polymorphic as possible, but the programmer still needs to keep
 * track of which pointer to use via the "type".
 */
struct HDLInterface {
  /* Type of interface being defined here...j73a, epp, etc... */
  unsigned int type;

  /* Pointers to the various interface structures. Only ONE pointer should
   * be valid per Interface structure. For instance, don't try and allocate
   * memory for both the EPP and JY73A pointers! Use type (declared above)
   * to determine which pointer to play with.
   */
  union {
	  struct Interface_Jy73a  *jorway73a;
	  struct Interface_Epp    *epp;
	  struct Interface_Serial *serial;
	  struct Interface_Usb    *usb;
    struct Interface_Usb2   *usb2;
	  struct Interface_Plx    *plx;
	  
	  /* Add other specific interfaces here */
  } info;
};
typedef struct HDLInterface HDLInterface;


/*****************************************************************************
 * SPECIFIC INTERFACES:
 * May move these to a seperate file. Stay tuned.
 *****************************************************************************/

struct Interface_Jy73a {
    /* SCSI bus # */
    unsigned int scsi_bus;

    /* The 73a crate #, set with the dial on the front */
    unsigned int crate_number;

    /* CAMAC crate slot that this module is plugged into */
    unsigned int slot;
};
typedef struct Interface_Jy73a Interface_Jy73a;


struct Interface_Epp {
    /* The address of the EPP port. Typically 0x378 or 0x278. */
    unsigned int epp_address;

    /* The daisy chain id of the module, IF applicable */
    unsigned int daisy_chain_id;
};
typedef struct Interface_Epp Interface_Epp;


struct Interface_Usb {
    /* The device name of the USB port. */
    unsigned int device_number;
};
typedef struct Interface_Usb Interface_Usb;

struct Interface_Usb2 {
    /* The device name of the USB2 port. */
    unsigned int device_number;
};
typedef struct Interface_Usb2 Interface_Usb2;

struct Interface_Serial {

    /* The COM port number */
    unsigned int com_port;

    /* The Baud rate of the port */
    unsigned int baud_rate;
};
typedef struct Interface_Serial Interface_Serial;

typedef struct Interface_Plx {
  /* The PCI bus that the slot is on */
  byte_t bus;
  
  /* The PCI slot that the module is plugged into */
  byte_t slot;

} Interface_Plx;

#endif /* XIA_HANDEL_STRUCTURES_H */
