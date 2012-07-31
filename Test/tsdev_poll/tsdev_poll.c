
/**
 * Simple tester class to poll the tuio character device and print
 * to stdout.
 *
 * @author Ian Stewart 05/26/09
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#define DEV_FILE "/dev/tuio"
#define BUF_LEN 1024


int main(int argc, char** argv)
{
   char buf[BUF_LEN];
   size_t read_len;
   int filep;


   if ((filep = open(DEV_FILE, O_RDONLY)) < 0) {
      printf("ERROR: Could not open device '%s' for reading!\n", DEV_FILE);
      exit(-1);
   }

   while(1) {

      read_len = read(filep, buf, BUF_LEN-1);
      if(read_len <= 0) {
         printf("ERROR: Reading device failed! %d\n", read_len);
      } else {
         buf[read_len] = '\0';
         printf("[%d]%s", read_len, buf);
         fflush(stdout);
      }

   }
   

}
