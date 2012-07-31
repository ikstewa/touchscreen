/*
 * TUIO Daemon
 *
 * Author:
 *    Ian Stewart <ikstewa@gmail.com>
 *
 * Description:
 *    Accepts two parameters, a socket number and a device filename.
 *    This program will run as a deamon and listen for any incomming osc
 *    messages on the specified port. When a message is recieved a human
 *    readable string will be written to the specified device.
 *
 *    TUIO/OSC messages are delived in the following format:
 *       "/osc/path/info arg0 arg1 arg2 arg3"
 *   ex: "/tuio/2Dcur set 4 0.482812 0.412500 0.000000 0.000000 -7.122507"
 *
 * Usage:
 *    ./tuiod 3333 /dev/tuio
 *
 *
 * Daemon setup from Devin Watson:
 * http://www.netzmafia.de/skripten/unix/linux-daemon-howto.html
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <lo/lo.h>

//#define __VERBOSE 
#define __DAEMON

#define LOG_FILE "/var/log/tuiod.log"

// FIXME: Need to handle this somehow!?
#define BUF_LEN 1024

FILE *log_fp = 0;
FILE *dev_fp = 0;
int done = 0;
char* buf = 0;

void collect_tuio(char* sk_port);
void error(int num, const char *m, const char *path);
void sighandler(int sig);
int write_msg( char* buf, int buf_len, const char *path, const char *types,
               lo_arg **argv, int argc);

int generic_handler(const char *path, const char *types, lo_arg **argv,
                     int argc, void *data, void *user_data);

int main(int argc, char* argv[])
{
#ifdef __DAEMON
   pid_t pid, sid;
#endif
   char* dev_file;

   if(argc != 3) {
      printf("usage: %s port_num dest_device\n", argv[0]);
      exit(EXIT_FAILURE);
   }
   dev_file = argv[2];
   /* // liblo accepts port as string. Not needed now
   // Parse the port number
   sk_port = atoi(argv[1]);
   if(!sk_port || sk_port == INT_MAX || sk_port == INT_MIN) {
      printf("usage: %s port_num\n", arv[0]);
      exit(EXIT_FAILURE);
   }
   */

#ifdef __DAEMON

   /* Fork from the parent process */
   pid = fork();
   if (pid < 0)
      exit(EXIT_FAILURE);

   /* Exit parent */
   if (pid > 0) 
      exit(EXIT_SUCCESS);


   /* Change the file mode mask */
   umask(0);
#endif

   /* Open any logs here */
   if((log_fp = fopen(LOG_FILE, "w")) == NULL) {
      printf("Warning: Could not open log file [%s] for writing\n", LOG_FILE);
      //exit(EXIT_FAILURE);
   }

   /* Open the device for reading */
   if((dev_fp = fopen(dev_file, "w")) == NULL) {
      printf("ERROR: Could not open device '%s' for writing!\n", dev_file);

      if(log_fp)
         fprintf(log_fp, "ERROR: Could not open device '%s' for writing!\n", dev_file);

      exit(EXIT_FAILURE);
   }
   if(log_fp) fprintf(log_fp, "Opened device '%s' for writing\n", dev_file);

#ifdef __DAEMON
   /* Creates a new SID for the child process */
   if ((sid = setsid()) < 0) {
      if(log_fp) fprintf(log_fp, "ERROR: setsid() failed\n");
      exit(EXIT_FAILURE);
   }

   /* Change the cwd */
   if ((chdir("/")) < 0) {
      if(log_fp) fprintf(log_fp, "ERROR: chdir(\"/\") failed\n");
      exit(EXIT_FAILURE);
   }
   
   /* Close out the standard file descriptors */
   close(STDIN_FILENO);
   close(STDOUT_FILENO);
   close(STDERR_FILENO);
#endif

   /* Allocate memory for buffer */
   buf = malloc(BUF_LEN);


   collect_tuio(argv[1]);

   fclose(log_fp);
   free(buf);
   return 0;
}


/*
 * Accepts a port number as a null terminated string and begins polling the
 * specified socket for any osc data packets
 */
void collect_tuio(char* sk_port)
{
   /* Register signal handlers */
   signal(SIGABRT, &sighandler);
   signal(SIGTERM, &sighandler);
   signal(SIGINT, &sighandler);


   /* Create a new server */
   lo_server_thread st = lo_server_thread_new(sk_port, error);

   /* Add method that will match any path and args */
   lo_server_thread_add_method(st, NULL, NULL, generic_handler, NULL);

   
   /* add method that will handle the 2d objects */
   //lo_server_thread_add_method(st, "/tuio/2Dobj", NULL, obj_handler, NULL);
   /* add method that will handle the 2d cursor */
   //lo_server_thread_add_method(st, "/tuio/2Dcur", NULL, cur_handler, NULL);

   /* add method that will handle the set msg from 2dobj profile */
   //lo_server_thread_add_method(st, "/tuio/2Dobj", "siiffffffff", obj_set_handler, NULL);
   /* add method that will handle the set msg from 2dcur profile */
   //lo_server_thread_add_method(st, "/tuio/2Dcur", "sifffff", cur_set_handler, NULL);

   /* Start the server */
   lo_server_thread_start(st);

   while(!done) {
      usleep(1000);
   }

   lo_server_thread_free(st);
}

/* Generic handler for all osc messages in any format */
int generic_handler(const char *path, const char *types, lo_arg **argv,
                     int argc, void *data, void *user_data)
{
   int len;
   /*
#ifdef __VERBOSE
   int i;

   if(log_fp) fprintf(log_fp, "path: <%s>\n", path);
   for (i=0; i<argc; i++) {
      if(log_fp) fprintf(log_fp, "arg %d '%c' ", i, types[i]);
      lo_arg_pp(types[i], argv[i]);
      if(log_fp) fprintf(log_fp, "\n");
   }
   if(log_fp) fprintf(log_fp,"\n");
#endif
*/


   /* Compose the message into a one line character string */
   len = write_msg(buf, BUF_LEN-2, path, types, argv, argc);

   if( len < 0 ) {
      if(log_fp) fprintf(log_fp, "ERROR: Buffer overflow! Data not written\n");
   } else {
      // Add a null terminator
      buf[len] = '\n';
      buf[len+1] = '\0';

      // Write the data to the device
      if(!fputs(buf, dev_fp))
         if(log_fp) fprintf(log_fp, "ERROR: Could not write to device!\n");
      fflush(dev_fp);

#ifdef __VERBOSE
      if(log_fp) fprintf(log_fp, "%s\n", buf);
#endif
   }
    
   return 1;
}

/*
 * Composes the osc datatypes into a human readable string of the form:
 * "/path/here arg0 arg1 arg2 arg3\n"
 */
int write_msg( char* lbuf, int buf_len, const char *path, const char *types,
               lo_arg **argv, int argc)
{
   int i;
   char* buf_start = lbuf;

   lbuf += sprintf(lbuf, "%s", path);
   
   if(lbuf - buf_start > buf_len)
      return -1;

   for(i = 0; i < argc; i++) {

      /* check for overflow */
      if(lbuf - buf_start > buf_len)
         return -1;

      switch(types[i]) {
         /** 32 bit signed integer. */
         case 'i':
            lbuf += sprintf(lbuf, " %d", argv[i]->i);
            break;
         /** 64 bit signed integer. */
         case 'h':
            lbuf += sprintf(lbuf, " %ld", argv[i]->h);
            break;
         /** 32 bit IEEE-754 float. */
         case 'f':
            lbuf += sprintf(lbuf, " %f", argv[i]->f);
            break;
         /** 64 bit IEEE-754 double. */
         case 'd':
            lbuf += sprintf(lbuf, " %lf", argv[i]->d);
            break;
         /** Standard C, NULL terminated string. */
         case 's':
            lbuf += sprintf(lbuf, " %s", &(argv[i]->s));
            break;
         /** Standard C, NULL terminated, string. Used in systems which
           * distinguish strings and symbols. */
         case 'S':
            lbuf += sprintf(lbuf, " %s", &(argv[i]->S));
            break;
         /** Standard C, 8 bit, char. */
         case 'c':
            lbuf += sprintf(lbuf, " %c", argv[i]->c);
            break;
         /** OSC TimeTag value. */
         // TODO
         case 't':
            if(log_fp) fprintf(log_fp, "TODO: TimeTag\n");
            break;
         default:
            if(log_fp) fprintf(log_fp, "Unknown type [%c]\n", types[i]);
            break;
      }
   }

   return lbuf - buf_start;
}

/* Called when liblo recieves an error */
void error(int num, const char *m, const char *path)
{
   if(log_fp) fprintf(log_fp, "liblo server error %d in path %s: %s\n", num, path, m);
}

void sighandler(int sig)
{
   if(log_fp) fprintf(log_fp, "SIG\n");
   done = 1;
}

