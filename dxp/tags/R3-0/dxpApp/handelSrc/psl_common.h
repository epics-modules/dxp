/*
 * Copyright (c) 2009-2010 XIA LLC
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
 * $Id: psl_common.h 16573 2010-08-21 01:34:29Z patrick $
 *
 */


#ifndef __PSL_COMMON_H__
#define __PSL_COMMON_H__

#include "psldef.h"

#include "xia_handel_structures.h"

#include "xerxes.h"


/* Logging macro wrappers */
#define pslLogError(x, y, z)	utils->funcs->dxp_md_log(MD_ERROR, (x), (y), (z), __FILE__, __LINE__)
#define pslLogWarning(x, y)	utils->funcs->dxp_md_log(MD_WARNING, (x), (y), 0, __FILE__, __LINE__)
#define pslLogInfo(x, y)	utils->funcs->dxp_md_log(MD_INFO, (x), (y), 0, __FILE__, __LINE__)
#define pslLogDebug(x, y)	utils->funcs->dxp_md_log(MD_DEBUG, (x), (y), 0, __FILE__, __LINE__)

static char info_string[400];


/* Memory allocation wrappers */

/* This is obviously a major hack, but the problem that I'm not
 * dealing with here is that this code is shared across all of the
 * PSLs. Each PSL has it's own memory naming convention. In the
 * future, we probably shouldn't allow psl.c to do any memory
 * management, but that is neither here nor there.
 */
#ifdef USE_XIA_MEM_MANAGER
#include "xia_mem.h"
#define MALLOC(n) xia_mem_malloc((n), __FILE__, __LINE__)
#define FREE(ptr) xia_mem_free(ptr)
#else
#define MALLOC(n) utils->funcs->dxp_md_alloc(n)
#define FREE(ptr) utils->funcs->dxp_md_free(ptr)
#endif /* USE_XIA_MEM_MANAGER */


/* Wrappers around other MD utility routines. */
#define FGETS(s, size, stream) utils->funcs->dxp_md_fgets((s), (size), (stream))

#define GET_PARAM_IDX(detChan, name, idx, fname) do { int statusX; statusX = dxp_get_symbol_index(&detChan, name, &idx); if (statusX != DXP_SUCCESS) { sprintf(info_string, "Unable to get the index of '%s' for detChan %d. Xerxes reports status = %d.", name, detChan, statusX); pslLogError("psl__Extract" # fname, info_string, XIA_XERXES); return XIA_XERXES; } } while (FALSE_)


/* Shared routines */
PSL_SHARED int PSL_API pslGetDefault(char *name, void *value,
                                     XiaDefaults *defaults);
PSL_SHARED int PSL_API pslSetDefault(char *name, void *value,
                                     XiaDefaults *defaults);
PSL_SHARED int pslGetModChan(int detChan, Module *m,
                             unsigned int *modChan);
PSL_SHARED int PSL_API pslDestroySCAs(Module *m, unsigned int modChan);
PSL_SHARED XiaDaqEntry * pslFindEntry(char *name, XiaDefaults *defs);
PSL_SHARED int pslInvalidate(char *name, XiaDefaults *defs);
PSL_SHARED void pslDumpDefaults(XiaDefaults *defs);
PSL_SHARED double pslU64ToDouble(unsigned long *u64);
PSL_SHARED int pslRemoveDefault(char *name, XiaDefaults *defs,
                                XiaDaqEntry **removed);
PSL_SHARED boolean_t pslIsUpperCase(char *s);

#endif /* __PSL_COMMON_H__ */
