#include <sys/io.h>
#include <errno.h>
#include <stdio.h>

#define MEM_ADDR 0x4038

int main()
{
   int base=0x378;
   int status=0;
   unsigned char d, d1, inbuff[2];
   int i;
   
   status = iopl(3);
   printf("Port=%x, iopl status=%d\n", base, status);
   if (status != 0) return(status);

   /* The following initialization code is from Gerry Swislow */
   d = inb(base + 0x402);
   printf("Base+0x402 = %#x\n", d);
   d1 = (d&0x1F) + 0x80;
   outb(d1, base + 0x402);
   d = inb(base + 0x402);
   printf("Wrote %#x to Base+0x402, read back %#x\n", d1, d);

   outb(0x4, base + 2);
   outb(0x1, base + 1);
   outb(0x0, base + 1);

   /* check status (not time out, nByte=1) */
   d = inb(base + 1);
   if ((d&0x01) == 1) {
      printf("Timeout: status=(%#x)\n", d);
   }

   if(((d>>5)&0x01) == 1) {
      /* interface is off by one byte, attempt to fix */
      printf("Interface off by 1 byte, fixing\n");
      outb(0, base + 3);
      d = inb(base + 1);
      if (((d>>5)&0x01) == 1) {
         printf("Error fixing interface offset\n");
      }
   }
   /* Set address=MEM_ADDR */
   outb((MEM_ADDR & 0xff), base+3);
   outb(((MEM_ADDR >> 8) & 0xff), base+3);
   /* Write 0n0n to locations 0-9 */
   for (i=0; i<9; i++) {
      outb(i, base+4);
      outb(i+1, base+4);
   }
   /* Set address=MEM_ADDR */
   outb((MEM_ADDR & 0xff), base+3);
   outb(((MEM_ADDR >> 8) & 0xff), base+3);
   /* Read back values from locations 0-9 */
   for (i=0; i<9; i++) {
      inbuff[0]=inb(base+4);
      inbuff[1]=inb(base+4);
      printf("Location %d = %#x\n", i, *(short *)inbuff);
   }
   return(0);
}
