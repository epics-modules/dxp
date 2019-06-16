/*
 * Copyright (c) 2004 X-ray Instrumentation Associates
 *               2005-2016 XIA LLC
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


#ifndef HANDEL_XERXES_H
#define HANDEL_XERXES_H

#include "handeldef.h"
#include "xia_handel_structures.h"


static char *SYS_NULL[1]  = { "NULL" };

static char *BOARD_LIST[] = {
#ifndef EXCLUDE_SATURN
  "dxpx10p",
#endif /* EXCLUDE_SATURN */
#ifndef EXCLUDE_UDXPS
  "udxps",
#endif /* EXCLUDE_UDXPS */
#ifndef EXCLUDE_UDXP
  "udxp",
#endif /* EXCLUDE_UDXP */
#ifndef EXCLUDE_XMAP
  "xmap",
#endif /* EXCLUDE_XMAP */
#ifndef EXCLUDE_STJ
  "stj",
#endif /* EXCLUDE_STJ */
#ifndef EXCLUDE_MERCURY
  "mercury",
#endif /* EXCLUDE_MERCURY */
  };


/* These names must be kept in sync with the interface enum in
 * xia_module.h
 */
static char *INTERF_LIST[] = {
  "bad",
  "EPP",
  "EPP", /* Placeholder to match the GENERIC_EPP entry in xia_module */
  "SERIAL",
  "USB",
  "USB2",
  "PXI",
};

#define N_KNOWN_BOARDS  (sizeof(BOARD_LIST) / sizeof(BOARD_LIST[0]))

#define MAX_INTERF_LEN   24
#define MAX_MD_LEN       12
#define MAX_NUM_CHAN_LEN  4
/* As far as the Xerxes configuration goes, this allows a detChan range of
 * 0 - 9999, which should be enough for anybody.
 */
#define MAX_CHAN_LEN      4

#endif /* HANDEL_XERXES_H */
