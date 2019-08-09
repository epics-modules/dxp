/*
 * Copyright (c) 2017 XIA LLC
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


#include "xia_mddef.h"
#include "xia_assert.h"

#include "md_generic.h"

#include "xerxesdef.h"
#include "xerxes_errors.h"
#include "xerxes_structures.h"

#include "xerxes_io.h"


/* Import the necessary MD routine here */
XIA_MD_IMPORT int XIA_MD_API dxp_md_init_util(Xia_Util_Functions *funcs, char *type);


#define INFO_LEN 400
static char INFO_STRING[INFO_LEN];

#define MD_IO_READ  0
#define MD_IO_WRITE 1
#define MD_IO_OPEN  2
#define MD_IO_CLOSE 3

/*
 * This library provides functions that let device libraries do things with
 * Xerxes structures slightly higher level than accessing interface functions
 * directly.
 */


/*
 * Performs MD IO using interface functions contained in the given
 * Board. This provides a single point of control to do IO tracing for any
 * device and any MD implementation.
 */
XERXES_SHARED int dxp_md_io(Board *board, unsigned int function,
                            unsigned long addr, void *data, unsigned int len)
{

    Xia_Util_Functions funcs;

    int status;

    dxp_md_init_util(&funcs, NULL);

    status = board->iface->funcs->dxp_md_io(&board->ioChan, &function, &addr, data, &len);

#ifdef XERXES_TRACE_IO
    /* Don't bother tracing e.g. usb2 address caching. */
    if (len > 0) {
        unsigned short *buf = (unsigned short *)data;
        unsigned int i;
        char op = function == MD_IO_READ ? 'R' : 'W';
        char *pos = INFO_STRING;

        pos += sprintf(pos, "%s %c ch%d [0x%08lX..%u]",
                       board->iface->dllname, op, board->ioChan, addr, len * 2);

        for (i = 0; i < len && strlen(INFO_STRING) < sizeof(INFO_STRING) - 7; i++) {
            pos += sprintf(pos, " %02X %02X",
                           (byte_t)(buf[i] & 0xFF),
                           (byte_t)((buf[i] >> 8) & 0xFF));
        }

        funcs.dxp_md_log(MD_INFO, "dxp_md_usb2_io", INFO_STRING, 0, __FILE__, __LINE__);
    }
#endif

    return status;
}
