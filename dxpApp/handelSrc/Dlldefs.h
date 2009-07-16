/*
 * Dlldefs.h
 *
 * Copyright (c) 2004 X-ray Instrumentation Associates
 *               2005 XIA LLC
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
 *
 * $Id: Dlldefs.h,v 1.5 2009-07-16 18:01:07 rivers Exp $
 */

#ifndef __DLLDEFS_H__
#define __DLLDEFS_H__

#ifdef EPICS_DLL_NO
#define XIA_STATIC_BUILD
#endif 

#define XIA_STATIC static
#define XIA_SHARED 

#ifdef _WIN32
  #ifdef XIA_STATIC_BUILD
    #define XIA_EXPORT
    #define XIA_IMPORT
  #else /* __STATIC */
    #define XIA_IMPORT __declspec( dllimport)
    #define XIA_EXPORT  __declspec( dllexport)
  #endif
#else /* _WIN32 */
  #define XIA_IMPORT 
  #define XIA_EXPORT 
#endif

#ifdef WIN32_EPPLIB_VBA
#define XIA_API _stdcall
#else

#ifdef WIN32_SERIAL_VBA
#define XIA_API _stdcall
#else

#ifdef WIN32_USBLIB_VBA
#define XIA_API _stdcall
#else

#ifdef WIN32_XIA_USB2_VBA
#define XIA_API _stdcall
#else

#define XIA_API

#endif /* WIN32_XIA_USB2_VBA */

#endif /* WIN32_USBLIB_VBA */

#endif /* WIN32_SERIAL_VBA */

#endif /* WIN32_EPPLIB_VBA */



#endif
