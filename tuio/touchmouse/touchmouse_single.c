#include <asm/uaccess.h>
#include <asm-generic/fcntl.h>
#include <linux/completion.h>
#include <linux/err.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "state.h"

#define DRIVER_NAME "touchmouse"
#define DRIVER_DESC "TUIO Mouse Adapter"
#define DRIVER_VER "0.1"

MODULE_AUTHOR ("Ian Stewart, Pavan Trikutam, Henry Phan");
MODULE_DESCRIPTION (DRIVER_DESC);
MODULE_LICENSE ("GPL");
MODULE_VERSION (DRIVER_VER);


#define TOUCHMOUSE_TUIO_SOURCE "/dev/tuio"
#define TOUCHMOUSE_X_MIN 0
#define TOUCHMOUSE_X_MAX 999999
#define TOUCHMOUSE_Y_MIN 0
#define TOUCHMOUSE_Y_MAX 999999
#define MESSAGE_MAX_LENGTH 96
#define MESSAGE_PROFILE "/tuio/2Dcur"
#define MESSAGE_ALIVE "alive"
#define MESSAGE_SET  "set"
#define MESSAGE_FSEQ "fseq"
#define MESSAGE_TYPE_OFFSET 12

#define _VERBOSE
#define _DEBUG


static struct input_dev *touchmouse;

static struct task_struct *thread;
static struct pid *thread_pid;
static wait_queue_head_t waitq;
static DECLARE_COMPLETION (on_exit);

static struct file *tuio;

static struct screen_state pre_state;
static struct screen_state cur_state;

static int msg_status;  /* Current state of the message bundle */


/**
 * Compares two screen_state objects and returns a new state_diff
 */
static void compare_state(struct state_diff *diff, struct screen_state *s1,
      struct screen_state *s2)
{
   int i,k;
   short found_match = 0;

   /* Compare a blob in the first state with everything in the new state.
    * If a match is found possibly add to moved_blobs.
    * If not found add to dead_blobs.
    */
   for ( i = 0; i < s1->count; i++ ) {
      // compare to each blob in the second
      found_match = 0;
      for ( k = 0; k < s2->count; k++ ) {

         if( s1->alive[i].id == s2->alive[k].id ) {
            found_match = 1;
            // check for change
            if ( abs(s1->alive[i].x - s2->alive[k].x) > JITTER_THRESHOLD ||
                 abs(s1->alive[i].y - s2->alive[k].y) > JITTER_THRESHOLD ) {
               // Moved blob difference
               diff->moved_blobs[diff->move_count++] = &(s2->alive[k]);
               diff->prev_blobs[diff->move_count-1] = &(s1->alive[i]);
            }
            break;
         }

      }
      // Dead blob difference
      if (!found_match)
         diff->dead_blobs[diff->dead_count++] = &(s1->alive[i]);
   }

   /* Compare a blob from the new state with all the old blobs.
    * If a match is not found, add to the new_blobs */
   for ( k = 0; k < s2->count; k++ ) {

      found_match = 0;
      for ( i = 0; i < s1->count; i++ ) {
         if ( s2->alive[k].id == s1->alive[i].id ) {
            found_match = 1;
            break;
         }
      }

      // New blob difference
      if (!found_match)
         diff->new_blobs[diff->new_count++] = &(s2->alive[k]);
   }
}

static inline int find_id(int count, struct blob_state **blobs, int id)
{
   int i;
   for ( i = 0; i < count; i++ ) {
      if ( blobs[i]->id == id ) {
         return i;
      }
   }
   return -1;
}


/**
 * Compare the previous and current state and fires any necessary events
 */
void handle_state (void)
{
   static unsigned int mouse_state = 0;
   static unsigned int mouse_id = 0;
   struct state_diff diff;
   struct blob_state *blob;
   int i;

   memset(&diff, 0, sizeof(diff));
   compare_state(&diff, &pre_state, &cur_state);

   /*
    * Four states for a left mouse
    *
    * 0  :  Initial state: no blobs.
    * 1  :  New blob. Track ID.
    * 2  :  Currently tracked id moved.
    *
    * Transistions:
    * 0->1  touch down
    * 1->0  touch up
    * 1->2  touch move
    * 2->0  touch up
    * 2->2  touch move
    */
   switch ( mouse_state )
   {
      case 0:
         // no tracked blobs. look for new
         if ( diff.new_count > 0 ) {
            mouse_id = diff.new_blobs[0]->id;

            // move the cursor
            input_report_abs (touchmouse, ABS_X, diff.new_blobs[0]->x);
            input_report_abs (touchmouse, ABS_Y, diff.new_blobs[0]->y);
            input_sync (touchmouse);

            // transition to state 1
            mouse_state = 1;
         }
         break;
      case 1:
         // click on up
         i = find_id(diff.dead_count, diff.dead_blobs, mouse_id);
         if ( i >= 0 ) {
     
            // Fire CLICK
            input_report_abs (touchmouse, ABS_X, diff.dead_blobs[i]->x);
            input_report_abs (touchmouse, ABS_Y, diff.dead_blobs[i]->y);
            input_report_key (touchmouse, BTN_TOUCH, 1);
            input_sync (touchmouse);
            input_report_key (touchmouse, BTN_TOUCH, 0);
            input_sync (touchmouse);

            mouse_state = 0;
            break;
         }

         // start drag on move
         i = find_id(diff.move_count, diff.moved_blobs, mouse_id);
         if ( i >= 0 ) {
            // Begin Drag
            input_report_abs (touchmouse, ABS_X, diff.prev_blobs[i]->x);
            input_report_abs (touchmouse, ABS_Y, diff.prev_blobs[i]->y);
            input_report_key (touchmouse, BTN_TOUCH, 1);
            input_sync (touchmouse);
            input_report_abs (touchmouse, ABS_X, diff.moved_blobs[i]->x);
            input_report_abs (touchmouse, ABS_Y, diff.moved_blobs[i]->y);
            input_sync (touchmouse);

            mouse_state = 2;
            break;
         }
         break;
      case 2:
         // End drag on up
         i = find_id(diff.dead_count, diff.dead_blobs, mouse_id);
         if ( i >= 0 ) {
            // End drag
            input_report_abs (touchmouse, ABS_X, diff.dead_blobs[i]->x);
            input_report_abs (touchmouse, ABS_Y, diff.dead_blobs[i]->y);
            input_report_key (touchmouse, BTN_TOUCH, 0);
            input_sync (touchmouse);
            mouse_state = 0;
            break;
         }

         i = find_id(diff.move_count, diff.moved_blobs, mouse_id);
         if ( i >= 0 ) {
            // Move
            input_report_abs (touchmouse, ABS_X, diff.moved_blobs[i]->x);
            input_report_abs (touchmouse, ABS_Y, diff.moved_blobs[i]->y);
            input_sync (touchmouse);
            mouse_state = 2;
            break;
         }
         break;
      default:
         // ERROR
         break;
   }



#ifdef _DEBUG
   printk("HANDLE_STATE\n");
   for ( i = 0; i < cur_state.count; i++ ) {
      blob = &(cur_state.alive[i]);
      printk("id:%lu x:%lu y:%lu\n", blob->id, blob->x, blob->y);
   }
#endif

#ifdef _VERBOSE
   if ( diff.new_count || diff.dead_count || diff.move_count ) {
      printk("STATE_DIFF: new[%u] dead[%u] move[%u]\n",
            diff.new_count, diff.dead_count, diff.move_count);
   }
#endif
}

/**
 * Receives a new message and modifies the current state
 * Returns 0 on success; 1 when message bundle compelte (fseq received)
 * Returns -1 on error
 */
int update_state(struct screen_state *state, char *message)
{
   char **copy, *token;
   int id, new_x, new_y;
   struct blob_state *blob;

   if ( !strncmp(message, MESSAGE_ALIVE, strlen(MESSAGE_ALIVE)) ) {
      // received an alive
      printk("ALIVE\n");

      // Verify we missed no objects
      /*
      int i;
      copy = &message;
      token = strsep (copy, " ");

      while (token && strlen(token) > 0) {
         token = strsep (copy, " ");
         id = simple_strtoul (token, NULL, 10);
         // search in the state
         for ( i = 0; i < state->count; i++ ) {
            if ( state->alive[i].id == id ) 
               break;
         }
         if(i >= state->count) {
#ifdef _VERBOSE
            printk (KERN_NOTICE "%s: missed set packet\n", DRIVER_NAME);
#endif
         }
      }
      */
      return 0;
   } else if ( !strncmp(message, MESSAGE_FSEQ, strlen(MESSAGE_FSEQ)) ) {
      // received a fseq
      printk("FSEQ\n");
      return 1;
   } else if ( !strncmp(message, MESSAGE_SET, strlen(MESSAGE_SET)) ) {
      // received a set

      message += strlen(MESSAGE_SET)+1;
      copy = &message;

      token = strsep (copy, " ");
      id = simple_strtoul (token, NULL, 10);

      token = strsep (copy, ".");
      token = strsep (copy, " ");
      new_x = simple_strtoul (token, NULL, 10);

      token = strsep (copy, ".");
      token = strsep (copy, " ");
      new_y = simple_strtoul (token, NULL, 10);

      printk("SET %d %d %d\n", id, new_x, new_y);

      /* Add to the current state */
      blob = &(state->alive[state->count++]);
      blob->id = id;
      blob->x = new_x;
      blob->y = new_y;

      return 0;
   } 
   return -1;
}

void
dispatch (char *message)
{

   /* Verify message profile */
   if ( strncmp (message, MESSAGE_PROFILE, MESSAGE_TYPE_OFFSET-1) ) {
#ifdef _VERBOSE
      printk (KERN_NOTICE "%s: Unknown profile received: %s\n", DRIVER_NAME, message);
#endif
      msg_status = 1;
      return;
   }

   /* Start a new bundle */
   if (msg_status) {
      pre_state = cur_state;
      memset(&cur_state, 0, sizeof(cur_state));
   }

   /* Pass the message wtihout the profile */
   if ( (msg_status = update_state(&cur_state, message + MESSAGE_TYPE_OFFSET)) < 0 ) {
#ifdef _VERBOSE
      printk (KERN_NOTICE "%s: Bad message received: %s\n", DRIVER_NAME, message);
#endif
      msg_status = 1;
      return;
   }

   /* Only handle the state once an fseq is received. */
   if (msg_status)
      handle_state();


   /*
  if (   !strncmp (message, MESSAGE_PROFILE, 11)
      && message[MESSAGE_TYPE_OFFSET] == 's')
    handle_set (message);
    */
}

static int
touchmouse_thread (void *data)
{
  unsigned long timeout;
  int error;
  char buffer[MESSAGE_MAX_LENGTH];
  ssize_t bytes_read;
  mm_segment_t old_fs;

  daemonize ("touchmouse");
  allow_signal (SIGTERM);
  thread_pid = task_pid (current);

  tuio = filp_open (TOUCHMOUSE_TUIO_SOURCE, O_RDONLY, 0);
  if (IS_ERR(tuio))
    {
      printk (KERN_ERR "%s: unable to open %s\n", DRIVER_NAME, TOUCHMOUSE_TUIO_SOURCE);
      error = -(PTR_ERR (tuio));
      goto error_thread;
    }

  tuio->f_pos = 0;

  while (1)
    {
      old_fs = get_fs ();
      set_fs (KERNEL_DS);
      bytes_read = tuio->f_op->read (tuio, buffer, sizeof (buffer), &tuio->f_pos);
      set_fs (old_fs);

      if (bytes_read <= 0) {
         printk(KERN_ERR "%s: read error! Goodbye\n", DRIVER_NAME);
         break;
      }

      dispatch (buffer);
      memset (buffer, ' ', sizeof (buffer));

      /* Timeout no longer needed; read will block. -istewart */
      //timeout = HZ;
      //timeout = wait_event_interruptible_timeout (waitq, (timeout == 0), timeout);
      //if (timeout == -ERESTARTSYS)
      //  break;
    }

  error = 0;
  fput (tuio);
error_thread:
  thread_pid = NULL;
  complete_and_exit (&on_exit, error);
}

static int __init
touchmouse_init (void)
{
  int error = -ENOMEM;

  touchmouse = input_allocate_device ();
  if (!touchmouse)
    goto error_touchmouse;

  touchmouse->name       = "touchmouse";
  touchmouse->phys       = "A/Fake/Path";
  touchmouse->id.bustype = BUS_HOST;
  touchmouse->id.vendor  = 0x0001;
  touchmouse->id.product = 0x0001;
  touchmouse->id.version = 0x0100;

  touchmouse->absmin[ABS_X]  = TOUCHMOUSE_X_MIN;
  touchmouse->absmax[ABS_X]  = TOUCHMOUSE_X_MAX;
  touchmouse->absfuzz[ABS_X] = 0;
  touchmouse->absflat[ABS_X] = 0;

  touchmouse->absmin[ABS_Y]  = TOUCHMOUSE_Y_MIN;
  touchmouse->absmax[ABS_Y]  = TOUCHMOUSE_Y_MAX;
  touchmouse->absfuzz[ABS_Y] = 0;
  touchmouse->absflat[ABS_Y] = 0;

  set_bit (EV_ABS, touchmouse->evbit);
  set_bit (ABS_X, touchmouse->absbit);
  set_bit (ABS_Y, touchmouse->absbit);

  set_bit (EV_KEY, touchmouse->evbit);
  set_bit (BTN_TOUCH, touchmouse->keybit);

  error = input_register_device (touchmouse);
  if (error)
    {
      printk (KERN_ERR "%s: unable to register device\n", DRIVER_NAME);
      goto free_touchmouse;
    }

  init_waitqueue_head (&waitq);

  thread = kthread_run (touchmouse_thread, NULL, "touchmouse");
  if (IS_ERR (thread))
    {
      printk (KERN_ERR "%s: unable to spawn poller thread\n", DRIVER_NAME);
      error = -EIO;
      goto unregister_touchmouse;
    }

  /* init the message state */
  msg_status = -1;

  printk (KERN_NOTICE "%s: loaded\n", DRIVER_NAME);
  return 0;

unregister_touchmouse:
  input_unregister_device (touchmouse);
  goto error_touchmouse;

free_touchmouse:
  input_free_device (touchmouse);

error_touchmouse:
  return error;
}

static void __exit
touchmouse_exit (void)
{
  if (thread_pid)
    {
      kill_pid (thread_pid, SIGTERM, 1);
      wait_for_completion (&on_exit);
    }

  input_unregister_device (touchmouse);
  printk (KERN_NOTICE "%s: unloaded\n", DRIVER_NAME);
}

module_init (touchmouse_init);
module_exit (touchmouse_exit);
