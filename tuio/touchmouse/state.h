#ifndef __STATE_H__
#define __STATE_H__


#define MOVE_DELAY_US 1000 // 1ms
#define DRAG_DELAY_US 250000 // 250 ms
#define JITTER_THRESHOLD 0
#define MAX_ALIVE_BLOBS 20

/**
 * Current state of a blob
 * Only using x and y
 */
struct blob_state {
   unsigned long id;
   unsigned long x;
   unsigned long y;
};

/**
 * Current state of the screen.
 * Contains blob states
 */
struct screen_state {
   struct blob_state alive[MAX_ALIVE_BLOBS];
   unsigned int count;
};

/**
 * Difference between two screen_state structs
 */
struct state_diff {
   /* New blobs */
   unsigned int new_count;
   struct blob_state *new_blobs[MAX_ALIVE_BLOBS];

   /* removed blobs */
   unsigned int dead_count;
   struct blob_state *dead_blobs[MAX_ALIVE_BLOBS];

   /* moved blobs */
   unsigned int move_count;
   struct blob_state *moved_blobs[MAX_ALIVE_BLOBS];
   /* Previous states of the moved blobs */
   struct blob_state *prev_blobs[MAX_ALIVE_BLOBS];
};


static int compare_state(struct state_diff *diff, struct screen_state *s1,
      struct screen_state *s2);

#endif
