/*
 * Copyright (c) 2009-2012 XIA LLC
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

#ifndef __HANDEL_LOG_H__
#define __HANDEL_LOG_H__

#include "xia_common.h"
#include "xerxes_structures.h"

DXP_MD_LOG           handel_md_log;

/* Logging macro wrappers */
#define xiaLogError(x, y, z) xiaLog(XIA_LOG_ERROR, (z), (x), "%s", (y))
#define xiaLogWarning(x, y)	 xiaLog(XIA_LOG_WARNING, (x), "%s", (y))
#define xiaLogInfo(x, y)	 xiaLog(XIA_LOG_INFO, (x), "%s", (y))
#define xiaLogDebug(x, y)	 xiaLog(XIA_LOG_DEBUG, (x), "%s", (y))

#define XIA_LOG_ERROR   MD_ERROR,  XIA_FILE, __LINE__
#define XIA_LOG_WARNING MD_WARNING, XIA_FILE, __LINE__, 0
#define XIA_LOG_INFO    MD_INFO, XIA_FILE, __LINE__, 0
#define XIA_LOG_DEBUG   MD_DEBUG, XIA_FILE, __LINE__, 0

static char info_string[400];

#endif /* __HANDEL_LOG_H__ */
