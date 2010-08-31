/**
 * Copyright (c) 2002-2004, X-ray Instrumentation Associates
 *               2005-2009, XIA LLC
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
 * $Id: xia_usb.c 14816 2010-03-03 20:37:08Z patrick $
 */

#include <windows.h>

/* We don't want _WIN32 from windows.h on Cygwin. */
#ifdef CYGWIN32
#undef _WIN32
#endif /* CYGWIN32 */

#ifdef _WIN32
#pragma warning( disable : 4201 )
#endif /* _WIN32 */

#include <winioctl.h>

#include <stdio.h>
#include <stdlib.h>

#include "Dlldefs.h"

#include "xia_common.h"

#include "usblib.h"


#define MAX_BUFFER_LEN 262144


static byte_t inBuffer[MAX_BUFFER_LEN];
static byte_t outBuffer[MAX_BUFFER_LEN];


XIA_EXPORT int XIA_API xia_usb_open(char *device, HANDLE *hDevice)
{
  /* Get handle to USB device */
  *hDevice = CreateFile(device,
					   GENERIC_WRITE,
					   FILE_SHARE_WRITE,
					   NULL,
					   OPEN_EXISTING,
					   0,
					   NULL);
  
  if(hDevice == INVALID_HANDLE_VALUE)
	{
	  return 1;
	}

  return 0;
}


XIA_EXPORT int XIA_API xia_usb_close(HANDLE hDevice)
{
  CloseHandle(hDevice);  

  return 0;
}


XIA_EXPORT int XIA_API xia_usb_read(long address, long nWords, char *device,
                                    unsigned short *buffer)
{	
  unsigned char* pData = (unsigned char*)buffer;
  long byte_count; 
  UCHAR ctrlBuffer[CTRL_SIZE];
  UCHAR lo_address, hi_address, lo_count, hi_count;
  int i = 0;
  unsigned long nBytes = 0;
  BOOL bResult = FALSE;
  HANDLE hDevice = NULL;
  long inPacketSize, outPacketSize;
  BULK_TRANSFER_CONTROL bulkControl;

  int ret;

  byte_count = (nWords * 2);
  
  hi_address = (unsigned char)(address >> 8);
  lo_address = (unsigned char)(address & 0x00ff);
  hi_count = (unsigned char)(byte_count >> 8);
  lo_count = (unsigned char)(byte_count & 0x00ff);
  
  ctrlBuffer[0] = lo_address;
  ctrlBuffer[1] = hi_address;
  ctrlBuffer[2] = lo_count;
  ctrlBuffer[3] = hi_count;
  ctrlBuffer[4] = (unsigned char)0x01;
  
  /******************************************************/
	
  /* Get handle to USB device */
  ret = xia_usb_open(device, &hDevice);

  if (ret != 0) 
	{
	  return 1;
	}
  /******************************************************/
  
  /* Write Address and Byte Count */	
  bulkControl.pipeNum = OUT1;
  outPacketSize = CTRL_SIZE;
  
  bResult = DeviceIoControl(hDevice,
							IOCTL_EZUSB_BULK_WRITE,
							&bulkControl,
							sizeof(BULK_TRANSFER_CONTROL),
							&ctrlBuffer[0],
							outPacketSize,
							&nBytes,
							NULL);
  
  if(bResult != TRUE)
	{
        xia_usb_close(hDevice);
        return 14;
	}
  
  /******************************************************/
		
  /* Read Data */
  bulkControl.pipeNum = IN2;
  inPacketSize = byte_count;
  
  bResult = DeviceIoControl(hDevice,
							IOCTL_EZUSB_BULK_READ,
							&bulkControl,
							sizeof(BULK_TRANSFER_CONTROL),
							&inBuffer[0],
							inPacketSize,
							&nBytes,
							NULL);
  
  if(bResult != TRUE)
	{
	  xia_usb_close(hDevice);
	  return 2;
	}
  
  for(i=0;i<byte_count;i++)
	{
	  *pData++ = inBuffer[i];
	}
  
  /*****************************************************/

  /* Close Handle to USB device */
  xia_usb_close(hDevice);
  
  return 0;
}


XIA_EXPORT int XIA_API xia_usb_write(long address, long nWords, char *device,
                                     unsigned short *buffer)
{
  unsigned char* pData = (unsigned char*)buffer;
  long byte_count; 
  UCHAR ctrlBuffer[CTRL_SIZE];
  UCHAR hi_address, lo_address, hi_count, lo_count;
  int i = 0;
  unsigned long nBytes = 0;
  BOOL bResult = FALSE;
  HANDLE hDevice = NULL;
  long outPacketSize;
  BULK_TRANSFER_CONTROL bulkControl;

  int ret;

  byte_count = (nWords * 2);
  
  hi_address = (unsigned char)(address >> 8);
  lo_address = (unsigned char)(address & 0x00ff);
  hi_count = (unsigned char)(byte_count >> 8);
  lo_count = (unsigned char)(byte_count & 0x00ff);
  
  ctrlBuffer[0] = lo_address;
  ctrlBuffer[1] = hi_address;
  ctrlBuffer[2] = lo_count;
  ctrlBuffer[3] = hi_count;
  ctrlBuffer[4] = (unsigned char)0x00;
  
  /******************************************************/

  /* Get handle to USB device */
  ret = xia_usb_open(device, &hDevice);
  
  if (ret != 0)
	{
	  return 1;
	}
  /******************************************************/

  /* Write Address and Byte Count	*/
  bulkControl.pipeNum = OUT1;
  outPacketSize = CTRL_SIZE;
  
  bResult = DeviceIoControl(hDevice,
							IOCTL_EZUSB_BULK_WRITE,
							&bulkControl,
							sizeof(BULK_TRANSFER_CONTROL),
							&ctrlBuffer[0],
							outPacketSize,
							&nBytes,
							NULL);
  
  if(bResult != TRUE)
	{
        xia_usb_close(hDevice);
	  return 14;
	}
  /******************************************************/
	
  /* Write Data */
  for(i=0;i<byte_count;i++)
	{
	  outBuffer[i] = *pData++;
	  Sleep(0);
	}
  
  bulkControl.pipeNum = OUT2;
  outPacketSize = byte_count;
  
  bResult = DeviceIoControl(hDevice,
							IOCTL_EZUSB_BULK_WRITE,
							&bulkControl,
							sizeof(BULK_TRANSFER_CONTROL),
							&outBuffer[0],
							outPacketSize,
							&nBytes,
							NULL);
  
  if(bResult != TRUE)
	{
        xia_usb_close(hDevice);
	  return 15;
	}
  
  /******************************************************/
  
  /* Close Handle to USB device */
  xia_usb_close(hDevice);
  
  return 0;
}
