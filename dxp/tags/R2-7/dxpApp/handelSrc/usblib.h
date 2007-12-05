/************/
/*  usblib.h  */
/************/

#define CTRL_SIZE 5
#ifdef LINUX
#define IN2 2
#define OUT1 1
#define OUT2 2
#define OUT4 4
#else
#define IN2 8
#define OUT1 0
#define OUT2 1
#define OUT4 3
#endif

/* Few definitions shamelessly copied from ezusbsys.h provided by Cypress */
#include "Dlldefs.h"
#ifdef WIN32
  #include <windows.h>
#endif
#ifdef LINUX
  #include "xia_linux.h"
#endif

#define Ezusb_IOCTL_INDEX  0x0800

typedef struct _BULK_TRANSFER_CONTROL
{
  ULONG pipeNum;
} BULK_TRANSFER_CONTROL, *PBULK_TRANSFER_CONTROL;

/*
 * Perform an IN transfer over the specified bulk or interrupt pipe.
 *
 *lpInBuffer: BULK_TRANSFER_CONTROL stucture specifying the pipe number to read from
 *nInBufferSize: sizeof(BULK_TRANSFER_CONTROL)
 *lpOutBuffer: Buffer to hold data read from the device.  
 *nOutputBufferSize: size of lpOutBuffer.  This parameter determines
 *   the size of the USB transfer.
 *lpBytesReturned: actual number of bytes read
 */ 
#define IOCTL_EZUSB_BULK_READ             CTL_CODE(FILE_DEVICE_UNKNOWN,  \
                                                   Ezusb_IOCTL_INDEX+19,\
                                                   METHOD_OUT_DIRECT,  \
                                                   FILE_ANY_ACCESS)

/*
 * Perform an OUT transfer over the specified bulk or interrupt pipe.
 *
 * lpInBuffer: BULK_TRANSFER_CONTROL stucture specifying the pipe number to write to
 * nInBufferSize: sizeof(BULK_TRANSFER_CONTROL)
 * lpOutBuffer: Buffer of data to write to the device
 * nOutputBufferSize: size of lpOutBuffer.  This parameter determines
 *    the size of the USB transfer.
 * lpBytesReturned: actual number of bytes written
 */ 
#define IOCTL_EZUSB_BULK_WRITE            CTL_CODE(FILE_DEVICE_UNKNOWN,  \
                                                   Ezusb_IOCTL_INDEX+20,\
                                                   METHOD_IN_DIRECT,  \
                                                   FILE_ANY_ACCESS)


/* Function Prototypes */

/*XIA_EXPORT int XIA_API xia_usb_read(long address, long nWords, long ModNum, uint16* buffer);
XIA_EXPORT int XIA_API xia_usb_write(long address, long nWords, long ModNum, uint16* buffer);
*/
XIA_EXPORT int XIA_API xia_usb_close(HANDLE hDevice);
XIA_EXPORT int XIA_API xia_usb_open(char *device, HANDLE *hDevice);
XIA_EXPORT int XIA_API xia_usb_read(long address, long nWords, char *device, unsigned short *buffer);
XIA_EXPORT int XIA_API xia_usb_write(long address, long nWords, char *device, unsigned short *buffer);
