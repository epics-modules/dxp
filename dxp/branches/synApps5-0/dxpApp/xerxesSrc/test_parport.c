#include <linux/parport.h>
#include <linux/ppdev.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFF_LEN 10
int main ()
{
   int fd;
   char *name="/dev/parport0";
   unsigned char buffer[BUFF_LEN];
   int mode=IEEE1284_MODE_EPP, status;

   fd = open (name, O_RDWR);
   if (fd == -1) {
      perror ("Error opening /dev/parport0:");
   }
   if (ioctl (fd, PPCLAIM)) {
      perror ("Error calling ioctl(PPCLAIM):");
   }
   status = ioctl (fd, PPNEGOT, &mode);
   if (status != 0) {
      perror("Error calling ioctl(PPNEGOT):");
   }
   status = ioctl (fd, PPSETMODE, &mode);
   if (status != 0) {
      perror("Error calling ioctl(PPSETMODE):");
   }
   status = write (fd, &buffer, BUFF_LEN);
   printf("%s wrote %d bytes\n", status==BUFF_LEN?"OK":"Error", status);
   status = read(fd, &buffer, BUFF_LEN);
   printf("%s read %d bytes\n", status==BUFF_LEN?"OK":"Error", status);
   return(0);
}
