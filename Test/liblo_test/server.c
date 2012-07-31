#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "lo/lo.h"


#define PORT 3333
#define PORT_STR "3333"

int done = 0;




void error(int num, const char *m, const char *path);
int generic_handler(const char *path, const char *types, lo_arg **argv,
                     int argc, void *data, void *user_data);
 
int foo_handler(const char *path, const char *types, lo_arg **argv, int argc,
                 void *data, void *user_data);

int quit_handler(const char *path, const char *types, lo_arg **argv, int argc,
                 void *data, void *user_data);

int idle_handler(const char *path, const char *types, lo_arg **argv, int argc,
                 void *data, void *user_data);
int obj_handler(const char *path, const char *types, lo_arg **argv, int argc,
                 void *data, void *user_data);
int cur_handler(const char *path, const char *types, lo_arg **argv, int argc,
                 void *data, void *user_data);
int cur_set_handler(const char *path, const char *types, lo_arg **argv, int argc,
                 void *data, void *user_data);
int obj_set_handler(const char *path, const char *types, lo_arg **argv, int argc,
                 void *data, void *user_data);

int main()
{
   /** start a new server */
   lo_server_thread st = lo_server_thread_new(PORT_STR, error);

   /* add method taht will match any path and args */
   //lo_server_thread_add_method(st, NULL, NULL, generic_handler, NULL);

   /** add a method taht will match the path /foo/bar, with two numbers, coerced
    * to float and int */
   //lo_server_thread_add_method(st, "/foo/bar", "fi", foo_handler, NULL);

   /* add method that will match the path /quit with no args */
   //lo_server_thread_add_method(st, "/quit", "", quit_handler, NULL);

   /* add method that will handle the 2d objects */
   //lo_server_thread_add_method(st, "/tuio/2Dobj", "s", idle_handler, NULL);

   /* add method that will handle the 2d objects */
   lo_server_thread_add_method(st, "/tuio/2Dobj", NULL, obj_handler, NULL);
   /* add method that will handle the 2d cursor */
   lo_server_thread_add_method(st, "/tuio/2Dcur", NULL, cur_handler, NULL);

   /* add method that will handle the set msg from 2dobj profile */
   lo_server_thread_add_method(st, "/tuio/2Dobj", "siiffffffff", obj_set_handler, NULL);
   /* add method that will handle the set msg from 2dcur profile */
   lo_server_thread_add_method(st, "/tuio/2Dcur", "sifffff", cur_set_handler, NULL);


   lo_server_thread_start(st);

   while(!done) {
      usleep(1000);
   }

   lo_server_thread_free(st);
   return 0;
}

void error(int num, const char *m, const char *path)
{
   printf("liblo server error %d in path %s: %s\n", num, path, m);
   fflush(stdout);
}

int generic_handler(const char *path, const char *types, lo_arg **argv,
                     int argc, void *data, void *user_data)
{
   int i;

   printf("path: <%s>\n", path);
   for (i=0; i<argc; i++) {
      printf("arg %d '%c' ", i, types[i]);
      lo_arg_pp(types[i], argv[i]);
      printf("\n");
   }
   printf("\n");
   fflush(stdout);

   return 1;
}
 
int foo_handler(const char *path, const char *types, lo_arg **argv, int argc,
                 void *data, void *user_data)
{
   printf("%s <- f:%f, i:%d\n\n", path, argv[0]->f, argv[1]->i);
   fflush(stdout);

   return 0;
}


int quit_handler(const char *path, const char *types, lo_arg **argv, int argc,
                 void *data, void *user_data)
{
   done = 1;
   printf("quiting\n\n");
   fflush(stdout);

   return 0;
}

int obj_handler(const char *path, const char *types, lo_arg **argv, int argc,
                 void *data, void *user_data)
{
   int seq_num, i;

   if(argc == 1)
   {
      //printf("No live objects\n");
   }
   else if(argc == 2 && types[0] == 's' && strcmp(argv[0], "fseq")==0)
   {
      // found an FSEQ
      seq_num = (argv[1]->i);
      if(seq_num < 0)
      {
         // idle sequence number
      }
      else
      {
         printf("FSEQ %d\n", seq_num);
      }

   }
   else
   {
      if(types[0] == 's' && strcmp(argv[0], "alive")==0)
      {
         printf("%s %s\n", (char*)path, (char*)argv[0]);
         for(i = 1; i < argc; i++)
         {
            printf("\tsessionID:%d\n", argv[i]->i);
         }
      }
   }
}

int cur_handler(const char *path, const char *types, lo_arg **argv, int argc,
                 void *data, void *user_data)
{
   int seq_num, i;

   if(argc == 1)
   {
      //printf("No live cursors!\n");
   }
   else if(argc == 2 && types[0] == 's' && strcmp(argv[0], "fseq")==0)
   {
      // fseq
      seq_num = argv[1]->i;
      if(seq_num < 0)
      {
         // idle sequence number
      }
      else
      {
         printf("FSEQ %d\n", seq_num);
      }
   }
   else
   {
      if(types[0] == 's' && strcmp(argv[0], "alive")==0)
      {
         printf("%s %s\n", (char*)path, (char*)argv[0]);
         for(i = 1; i < argc; i++)
         {
            printf("\tsessionID:%d\n", argv[i]->i);
         }
      }
   }
}


int cur_set_handler(const char *path, const char *types, lo_arg **argv, int argc,
                 void *data, void *user_data)
{
   printf("ACTIVE CURSOR!\n");
   printf("%s %s\n", (char*)path, (char*)argv[0]);
   printf("\tsessionID: %d\n", argv[1]->i);
   printf("\tx: %f\n", argv[2]->f);
   printf("\ty: %f\n", argv[3]->f);
   printf("\tvelX: %f\n", argv[4]->f);
   printf("\tvelY: %f\n", argv[5]->f);
   printf("\taccel: %f\n", argv[6]->f);

}
int obj_set_handler(const char *path, const char *types, lo_arg **argv, int argc,
                 void *data, void *user_data)
{
   printf("%s %s\n", path, (char*)argv[0]);
   printf("\tsessionID: %d\n", argv[1]->i);
   printf("\tclassID: %d\n", argv[2]->i);
   printf("\tx: %f\n", argv[3]->f);
   printf("\ty: %f\n", argv[4]->f);
   printf("\tangle: %f\n", argv[5]->f);
   printf("\tvelX: %f\n", argv[6]->f);
   printf("\tvelY: %f\n", argv[7]->f);
   printf("\tvelRot: %f\n", argv[8]->f);
   printf("\taccel: %f\n", argv[9]->f);
   printf("\taccelRot: %f\n", argv[10]->f);
}

