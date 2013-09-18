#ifndef INC_dragonfly_h
#define INC_dragonfly_h

#include <ross.h>
#define GLOBAL_CHANNELS 16
#define NUM_ROUTER 32
#define NUM_TERMINALS 16
#define PACKET_SIZE 256.0
#define LINK_DELAY 100.0
#define ROUTER_DELAY 100.0
#define MEAN_PROCESS 200.0
#define N_COLLECT_POINTS 20
#define DEBUG 1
#define TRACK 26419
#define PRINT_ROUTER_TABLE 1

static double ARRIVAL_RATE = 0.0000001;
static double MEAN_INTERVAL;

typedef enum terminal_event_t terminal_event_t;
typedef struct terminal_state terminal_state;
typedef struct terminal_message terminal_message;
typedef struct router_state router_state;
typedef enum router_event_t router_event_t;

struct terminal_state
{
   unsigned long long packet_counter;
   int N_wait_to_be_processed;
   unsigned int group_id;
   unsigned int router_id;
   unsigned int terminal_id;
   tw_stime next_available_time;
   tw_stime saved_available_time;  
};

enum terminal_event_t
{
  GENERATE,
  ARRIVAL,
  SEND,
  PROCESS
};

struct terminal_message
{
  tw_stime transmission_time;
  tw_stime travel_start_time;
  tw_stime saved_router_available_time;
  tw_stime saved_available_time;
  unsigned long long packet_ID;
  terminal_event_t  type;
  unsigned int dest_terminal_id;
  unsigned int saved_src_terminal_id;
  unsigned int src_terminal_id;
  int my_N_queue;
  int my_N_hop;
  int queueing_times;
  int next_stop;
};

struct router_state
{
   unsigned int router_id;
   unsigned int group_id;
   int global_channel[GLOBAL_CHANNELS];
   unsigned long long num_routed_packets;
   int N_wait_to_be_processed;
   tw_stime next_router_available_time;
   tw_stime saved_router_available_time;
};

static int       nlp_terminal_per_pe;
static int       nlp_router_per_pe;
static int opt_mem = 30000;

tw_stime         average_travel_time = 0;
tw_stime         total_time = 0;
tw_stime         max_latency = 0;

int range_start;
unsigned long num_groups = NUM_ROUTER*GLOBAL_CHANNELS+1;
int total_routers, total_terminals;

static unsigned long long       total_hops = 0;
static unsigned long long       total_queue_length = 0;
static unsigned long long       queueing_times_sum = 0;

static unsigned long long       N_finished = 0;
static unsigned long long       N_finished_storage[N_COLLECT_POINTS];
static unsigned long long       N_generated_storage[N_COLLECT_POINTS];
#endif
