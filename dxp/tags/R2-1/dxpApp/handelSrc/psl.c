/*
 * psl.c
 *
 * Routines that are common
 * to all PSL modules.
 *
 * Created 04/04/02 -- PJF
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

#include "xia_psl.h"
#include "xia_handel.h"
#include "xia_assert.h"

#include "handel_errors.h"
#include "handel_generic.h"


/**
 * This routine doesn't make any assumptions about
 * the presence of an acq. value when searching
 * the list. Returns an error if the acq. value isn't
 * found. If acq. value is found then the value is stored
 * in "value".
 */
PSL_SHARED int PSL_API pslGetDefault(char *name, void *value,
									 XiaDefaults *defaults)
{
  
  XiaDaqEntry *entry = NULL;


  entry = defaults->entry;
  
  while (entry != NULL) {
	
	if (STREQ(name, entry->name)) {

	  *((double *)value) = entry->data;
	  return XIA_SUCCESS;
	}

	entry = getListNext(entry);
  }

  return XIA_NOT_FOUND;
}
	

/**
 * Sets the named acquisition value to the new value
 * specified in "value". If the value doesn't 
 * exist in the list then an error is returned.
 */
PSL_SHARED int PSL_API pslSetDefault(char *name, void *value,
									 XiaDefaults *defaults)
{

  XiaDaqEntry *entry = NULL;


  entry = defaults->entry;

  while (entry != NULL) {

	if (STREQ(name, entry->name)) {

	  entry->data = *((double *)value);
	  return XIA_SUCCESS;
	}

	entry = getListNext(entry);
  }

  return XIA_NOT_FOUND;
}


/** @brief Converts a detChan to a modChan
 *
 * Converts a detChan to the modChan for the specified module. \n
 * Returns an error if the detChan isn't assigned to a channel \n
 * in that module.
 *
 * @param detChan detChan to convert
 * @param m Module to get the module channel from
 * @param modChan Returned module channel.
 * 
 * @returns Error status indicating success or failure.
 *
 */
PSL_SHARED int PSL_API pslGetModChan(int detChan, Module *m,
									 unsigned int *modChan)
{
  unsigned int i;


  ASSERT(m != NULL);
  ASSERT(modChan != NULL);


  for (i = 0; i < m->number_of_channels; i++) {
	if (m->channels[i] == detChan) {
	  *modChan = i;
	  return XIA_SUCCESS;
	}
  }

  sprintf(info_string, "detChan '%d' is not assigned to module '%s'",
		  detChan, m->alias);
  pslLogError("pslGetModChan", info_string, XIA_INVALID_DETCHAN);
  return XIA_INVALID_DETCHAN;
}


/** @brief Frees memory associated with the SCAs
 *
 * @param m Pointer to a valid module structure containing the 
 *          SCAs to destroy.
 * @param modChan The module channel number of the SCAs to be
 *                destroyed.
 *
 * @return Integer indicating success or failure.
 *
 */
PSL_SHARED int PSL_API pslDestroySCAs(Module *m, unsigned int modChan)
{
  ASSERT(m != NULL);


  utils->funcs->dxp_md_free(m->ch[modChan].sca_lo);
  m->ch[modChan].sca_lo = NULL;
  utils->funcs->dxp_md_free(m->ch[modChan].sca_hi);
  m->ch[modChan].sca_hi = NULL;

  m->ch[modChan].n_sca = 0;

  return XIA_SUCCESS;
}


/** @brief Find the entry structure matching the supplied name
 *
 */
PSL_SHARED XiaDaqEntry *pslFindEntry(char *name, XiaDefaults *defs)
{
  XiaDaqEntry *e = NULL;


  e = defs->entry;
  
  while (e != NULL) {
	if (STREQ(e->name, name)) {
	  return e;
	}

	e = e->next;
  }
  
  return NULL;
}


/** @brief Invalidates the specified acquisition value
 *
 */
PSL_SHARED int pslInvalidate(char *name, XiaDefaults *defs)
{
  XiaDaqEntry *e = NULL;

  
  ASSERT(defs != NULL);


  e = pslFindEntry(name, defs);

  if (e == NULL) {
	sprintf(info_string, "Unknown acquisition value '%s'", name);
	pslLogError("pslInvalidate", info_string, XIA_NOT_FOUND);
	return XIA_NOT_FOUND;
  }

  /* XXX What if state is modified? */
  sprintf(info_string, "%p: e->state = %#x", e, e->state);
  pslLogDebug("pslInvalidate", info_string);

  e->state = AV_STATE_UNKNOWN;
  ASSERT(e->state == AV_STATE_UNKNOWN);

  return XIA_SUCCESS;
}


/** @brief Dump the entire defaults structure to the log file
 *
 */
PSL_SHARED void pslDumpDefaults(XiaDefaults *defs)
{
  XiaDaqEntry *e = NULL;


  ASSERT(defs != NULL);


  sprintf(info_string, "Starting dump of '%s'...", defs->alias);
  pslLogDebug("pslDumpDefaults", info_string);

  e = defs->entry;

  while (e != NULL) {
	sprintf(info_string, "%p: %s, %.3f, %.3f, %#x", e, e->name, e->data,
			e->pending, e->state);
	pslLogDebug("pslDumpDefaults", info_string);

	e = e->next;
  }

  sprintf(info_string, "...Ending dump of '%s'", defs->alias);
  pslLogDebug("pslDumpDefaults", info_string);
}
