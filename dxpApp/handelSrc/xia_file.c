/*
 * Copyright (c) 2006-2014 XIA LLC
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

#include "Dlldefs.h"

#include "xia_file_private.h"
#include "xia_common.h"
#include "xia_assert.h"


/* Private functions */
static void xia__add_handle(FILE *fp, char *file, int line);
static void xia__remove_handle(FILE *fp);
static FILE *xia__open_home(const char* filename, const char* mode, const char* env);


/* Global variables */

/* If this library wants to become thread-safe, this variable
 * needs to be handled carefully. (Currently, it is not handled carefully
 * at all.)
 */
static xia_file_handle_t *FILE_HANDLES = NULL;


/*
 * Opens a file stream.
 *
 * Opens a file stream with the requested mode. The allowed modes are
 * the same as those of the standard C library function fopen(). This function
 * is the same as fopen(), except that it tracks when the file was opened
 * via. the file and line parameters. This function is typically wrapped
 * in a macro like so:
 *
 * @code
 * #define FOPEN(name, mode) xia_fopen(name, mode, __FILE__, __LINE__)
 * @endcode
 *
 * This routine returns NULL if fopen() fails or if any of the passed
 * in arguments are invalid.
 */
XIA_SHARED FILE *xia_fopen(const char *name, const char *mode, char *file,
                           int line)
{
    FILE *fp = NULL;


    if (name == NULL || mode == NULL || file == NULL) {
        return NULL;
    }

    fp = fopen(name, mode);

    if (fp) {
        xia__add_handle(fp, file, line);
    }

    return fp;
}


/*
 * Returns the number of open file handles
 */
XIA_SHARED int xia_num_open_handles(void)
{
    xia_file_handle_t *fh = NULL;

    int n_handles = 0;


    for (fh = FILE_HANDLES; fh != NULL; fh = fh->next, n_handles++) {
        /* Do nothing. */
    }

    return n_handles;
}


/*
 * Prints a list of the open file handles to the
 * specified stream.
 *
 * It is an unchecked exception to pass a NULL stream to this function.
 */
XIA_SHARED void xia_print_open_handles(FILE *stream)
{
    xia_file_handle_t *fh = NULL;


    ASSERT(stream != NULL);


    for (fh = FILE_HANDLES; fh != NULL; fh = fh->next) {
        fprintf(stream, "<%p> %s, line %d\n", fh->fp, fh->file, fh->line);
    }
}


/*
 * Prints a list of the open file handles to stdout
 */
XIA_SHARED void xia_print_open_handles_stdout(void)
{
    xia_print_open_handles(stdout);
}


/*
 * Close an open file handle.
 *
 * Removes the specified fp from the handles list. It is an unchecked
 * exception to pass a NULL file pointer into this routine.
 */
XIA_SHARED int xia_fclose(FILE *fp)
{
    ASSERT(fp != NULL);

    xia__remove_handle(fp);
    return fclose(fp);
}


/*
 * Adds a FILE pointer to the global list.
 *
 * Any memory allocation failures that occur during this process are treated
 * as unchecked exceptions.
 */
static void xia__add_handle(FILE *fp, char *file, int line)
{
    xia_file_handle_t *new_handle = NULL;


    ASSERT(fp != NULL);
    ASSERT(file != NULL);


    new_handle = malloc(sizeof(xia_file_handle_t));
    ASSERT(new_handle != NULL);

    new_handle->fp   = fp;
    new_handle->line = line;
    strncpy(new_handle->file, file, MAX_FILE_SIZE);

    new_handle->next = FILE_HANDLES;
    FILE_HANDLES = new_handle;
}


/*
 * Remove the handle reference containing fp from the list.
 */
static void xia__remove_handle(FILE *fp)
{
    xia_file_handle_t *fh   = NULL;
    xia_file_handle_t *prev = NULL;


    if (!fp) {
        return;
    }

    for (fh = FILE_HANDLES, prev = NULL; fh != NULL; fh = fh->next) {

        if (fh->fp == fp) {

            if (prev == NULL) {
                FILE_HANDLES = fh->next;

            } else {
                prev->next = fh->next;
            }

            free(fh);
            fh = NULL;
            return;
        }

        prev = fh;
    }

    /* This means that we couldn't find the pointer in our list of handles. */
    FAIL();
}


/*
 * Find and open given file, returns a FILE handle
 * Try to open the file directly first.
 * Then try to open the file in the directory pointed to by XIAHOME or DXPHOME
 
 * const char *filename;    Input: filename to open  
 * const char *mode;        Input: Mode to use when opening 
 *
 */
XIA_SHARED FILE *xia_find_file(const char* filename, const char* mode)
{
    FILE *fp = NULL;
    ASSERT(filename != NULL);

    /* Try to open file directly */
    if((fp = xia_file_open(filename, mode)) != NULL) {
        return fp;
    }

    /* Try to open the file with the path XIAHOME */
    if((fp = xia__open_home(filename, mode, "XIAHOME")) != NULL) {
        return fp;
    }

    /* Try to open the file with the path DXPHOME */
    if((fp = xia__open_home(filename, mode, "DXPHOME")) != NULL) {
        return fp;
    }
   
    return NULL;
}

/*
 * Try to open the file with the path specified in env
 */
static FILE *xia__open_home(const char* filename, const char* mode, const char* env)
{
    FILE *fp = NULL;
    size_t filenameLen = 0;

    char *home = getenv(env);
    char *name = NULL;
    
    if (home != NULL) {
        filenameLen = strlen(home) + strlen(filename) + 2;
        name = (char *) malloc(sizeof(char) * filenameLen);                                       

        if (!name) return NULL;

        sprintf(name, "%s/%s", home, filename);
        fp = xia_file_open(name, mode);

        free(name);
        return fp;
    }
    
    return NULL;
}