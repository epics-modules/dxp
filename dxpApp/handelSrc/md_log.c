/*
 * Copyright (c) 2002-2004 X-ray Instrumentation Associates
 * Copyright (c) 2005-2015 XIA LLC
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


#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>

#include "xerxes_errors.h"
#include "xerxes_structures.h"

#include "md_shim.h"
#include "md_generic.h"
#include "xia_md.h"

#include "xia_assert.h"
#include "xia_common.h"
#include "handel_errors.h"

#define INFO_LEN  400

XIA_MD_STATIC void dxp_md_local_time(struct tm **local, int *milli);

/* Current output for the logging routines. By default, this is set to stdout
 * in dxp_md_log().
 */
static boolean_t isSuppressed = FALSE_;

static int logLevel = MD_ERROR;

/**
 * This routine enables the logging output
 */
XIA_MD_SHARED int dxp_md_enable_log(void)
{
    isSuppressed = FALSE_;

    return DXP_SUCCESS;
}


/**
 * This routine disables the logging output
 */
XIA_MD_SHARED int dxp_md_suppress_log(void)
{
    isSuppressed = TRUE_;

    return DXP_SUCCESS;
}


/**
 * This routine sets the maximum level at which log messages will be
 * displayed.
 */
XIA_MD_SHARED int dxp_md_set_log_level(int level)
{

/* Validate level */
    if ((level > MD_DEBUG) || (level < MD_ERROR)) {
        /* Leave level at previous setting and return an error code */
        return DXP_LOG_LEVEL;
    }

    logLevel = level;

    return DXP_SUCCESS;
}


/**
 * This routine is the main logging routine. It shouldn't be called directly.
 * Use the macros provided in xerxes_generic.h.
 */
XIA_MD_SHARED void dxp_md_log(int level, const char *routine, const char *message,
                           int error, const char *file, int line)
{
    /* If logging is disabled or we aren't set
     * to log this message level then return gracefully, NOW!
     */
    if (isSuppressed || (level > logLevel)) {
        return;
    }

    /* Validate level */
    if ((level > MD_DEBUG) || (level < MD_ERROR)) {
        /* Don't log the message */
        return;
    }

    /* Ordinarily, we'd set this in the globals section, but on Linux 'stdout'
     * isn't a constant, so it can't be used as an initializer.
     */
    if (out_stream == NULL) {
        out_stream = stdout;
    }

    switch (level) {
        case MD_ERROR:
            dxp_md_error(routine, message, &error, file, line);
            break;
        case MD_WARNING:
            dxp_md_warning(routine, message, file, line);
            break;
        case MD_INFO:
            dxp_md_info(routine, message, file, line);
            break;
        case MD_DEBUG:
            dxp_md_debug(routine, message, file, line);
            break;
        default:
            FAIL();
            break;
    }

}

/**
 * Write a standard log format header.
 */
static void dxp_md_log_header(const char* type, const char* routine,
                                     int* error_code, const char *file, int line)
{
    struct tm *localTime;
    int milli;
    char logTimeFormat[80];

    const char* basename;
    int out;

    dxp_md_local_time(&localTime, &milli);

    strftime(logTimeFormat, sizeof(logTimeFormat), "%Y-%m-%d %H:%M:%S", localTime);

    basename = strrchr(file, '/');
    if (basename != NULL) {
        ++basename;
    } else {
        basename = strrchr(file, '\\');
        if (basename != NULL)
            ++basename;
        else
            basename = file;
    }

    out = fprintf(out_stream, "%s %s,%03d %s (%s:%d)",
                  type, logTimeFormat, milli, routine, basename, line);

    fprintf(out_stream, "%*c ", 90 - out, ':');

    if (error_code)
        fprintf(out_stream, "[%3d] ", *error_code);
}

/**
 * Routine to handle error reporting.  Allows the user to pass errors to log
 * files or report the information in whatever fashion is desired.
 */
XIA_MD_SHARED void dxp_md_error(const char* routine, const char* message,
                                int* error_code, const char *file, int line)
{
    dxp_md_log_header("[ERROR]", routine, error_code, file, line);
    fprintf(out_stream, "%s\n", message);
    fflush(out_stream);
}


/**
 * Routine to handle reporting warnings. Messages are written to the output
 * defined in out_stream.
 */
XIA_MD_SHARED void dxp_md_warning(const char *routine, const char *message,
                                  const char *file, int line)
{
    dxp_md_log_header("[WARN ]", routine, NULL, file, line);
    fprintf(out_stream, "%s\n", message);
    fflush(out_stream);
}


/**
 * Routine to handle reporting info messages. Messages are written to the
 * output defined in out_stream.
 */
XIA_MD_SHARED void dxp_md_info(const char *routine, const char *message,
                               const char *file, int line)
{
    dxp_md_log_header("[INFO ]", routine, NULL, file, line);
    fprintf(out_stream, "%s\n", message);
    fflush(out_stream);
}


/**
 * Routine to handle reporting debug messages. Messages are written to the
 * output defined in out_stream.
 */
XIA_MD_SHARED void dxp_md_debug(const char *routine, const char *message,
                                const char *file, int line)
{
    dxp_md_log_header("[DEBUG]", routine, NULL, file, line);
    fprintf(out_stream, "%s\n", message);
    fflush(out_stream);
}


/** Redirects the log output to either a file or a special descriptor
 * such as stdout or stderr. Allowed values for @a filename are: a
 * path to a file, "stdout", "stderr", "" (redirects to stdout) or
 * NULL (also redirects to stdout).
 */
XIA_MD_SHARED void dxp_md_output(const char *filename)
{
    char *strtmp = NULL;

    unsigned int i;

    char info_string[INFO_LEN];

    if (out_stream != NULL && out_stream != stdout && out_stream != stderr) {
        fclose(out_stream);
    }

    if (filename == NULL || STREQ(filename, "")) {
        out_stream = stdout;
        return;
    }

    strtmp = dxp_md_alloc(strlen(filename) + 1);
    if (!strtmp)
        abort();

    for (i = 0; i < strlen(filename); i++) {
        strtmp[i] = (char)tolower((int)filename[i]);
    }
    strtmp[strlen(filename)] = '\0';


    if (STREQ(strtmp, "stdout")) {
        out_stream = stdout;

    } else if (STREQ(strtmp, "stderr")) {
        out_stream = stderr;

    } else {
        out_stream = fopen(filename, "w");

        if (!out_stream) {
            int status = XIA_OPEN_FILE;

            /* Reset to stdout with the hope that it is redirected
             * somewhere meaningful.
             */
            out_stream = stdout;
            sprintf(info_string, "Unable to open filename '%s' for logging. "
                    "Output redirected to stdout.", filename);
            dxp_md_error("dxp_md_output", info_string, &status, __FILE__, __LINE__);
        }
    }

    dxp_md_free(strtmp);
}

#ifdef _WIN32
/**
 * Convert a Windows system time to time_t... for conversion to struct
 * tm to work with strftime. From
 * https://blogs.msdn.microsoft.com/joshpoley/2007/12/19/datetime-formats-and-conversions/
 */
XIA_MD_STATIC void dxp_md_SystemTimeToTime_t(SYSTEMTIME *systemTime, time_t *dosTime)
{
    LARGE_INTEGER jan1970FT = {0};
    LARGE_INTEGER utcFT = {0};
    unsigned __int64 utcDosTime;

    jan1970FT.QuadPart = 116444736000000000I64; // january 1st 1970
    SystemTimeToFileTime(systemTime, (FILETIME*)&utcFT);

    utcDosTime = (utcFT.QuadPart - jan1970FT.QuadPart)/10000000;
    *dosTime = (time_t)utcDosTime;
}

/**
 * Returns the current local time as a struct tm for string formatting
 * and the milliseconds on the side for extra precision.
 */
XIA_MD_STATIC void dxp_md_local_time(struct tm **local, int *milli)
{
    SYSTEMTIME tod;
    time_t current;

    GetLocalTime(&tod);

    dxp_md_SystemTimeToTime_t(&tod, &current);
    *local = gmtime(&current);
    *milli = tod.wMilliseconds;
}

#else

/**
 * Returns the current local time as a struct tm for string formatting
 * and the milliseconds on the side for extra precision.
 */
XIA_MD_STATIC void dxp_md_local_time(struct tm **local, int *milli)
{
    struct timeval tod;
    time_t current;

    gettimeofday(&tod, NULL);
    current = tod.tv_sec;
    *milli = (int) tod.tv_usec / 1000;
    *local = localtime(&current);
}
#endif
