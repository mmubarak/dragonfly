#ifndef INC_dragonfly_h
#define INC_dragonfly_h

#include <ross.h>

// dragonfly basic configuration parameters
#define GLOBAL_CHANNELS 4
#define NUM_ROUTER 8
#define NUM_TERMINALS 4

#define PACKET_SIZE 1.0

// delay parameters
#define TERMINAL_DELAY 1.0
#define ROUTING_DELAY 5
#define LOCAL_DELAY 10.0
#define GLOBAL_DELAY 100.0
#define RESCHEDULE_DELAY 1

// time to process a packet at destination terminal
#define MEAN_PROCESS 0

#define N_COLLECT_POINTS 20

// virtual channel information
#define NUM_VC 2
#define VC_BUF_SIZE 256

// radix of each router
#define RADIX (NUM_VC * NUM_ROUTER)+ (NUM_VC*GLOBAL_CHANNELS) + (NUM_VC * NUM_TERMINALS)


// debugging parameters
#define DEBUG 1
#define TRACK 320232
#define PRINT_ROUTER_TABLE 1

// arrival rate
static double MEAN_INTERVAL;

typedef enum event_t event_t;
typedef struct terminal_state terminal_state;
typedef struct terminal_message terminal_message;
typedef struct buf_space_message buf_space_message;
typedef struct router_state router_state;

struct terminal_state
{
   unsigned long long packet_counter;

   // Dragonfly specific parameters
   unsigned int router_id;
   unsigned int terminal_id;

   // Each terminal will have an input and output channel with the router
   unsigned int vc_occupancy[NUM_VC];

   unsigned int output_vc_state[NUM_VC];

   int terminal_available_time;
};

// Terminal generate, sends and arrival T_SEND, T_ARRIVAL, T_GENERATE
// Router-Router Intra-group sends and receives RR_LSEND, RR_LARRIVE
// Router-Router Inter-group sends and receives RR_GSEND, RR_GARRIVE

enum event_t
{
  T_GENERATE,
  RESCHEDULE,
  T_ARRIVE,
  T_SEND,

  R_SEND,
  R_ARRIVE,

  T_PROCESS,
  BUFFER
};

enum vc_status
{
   VC_IDLE,
   VC_ACTIVE,
   VC_ALLOC,
   VC_CREDIT
};

enum last_hop
{
   GLOBAL,
   LOCAL,
   TERMINAL
};
struct terminal_message
{
  tw_stime travel_start_time;
  
  unsigned long long packet_ID;
  event_t  type;
  
  unsigned int dest_terminal_id;
  unsigned int src_terminal_id;
  
  int my_N_hop;

  // Intermediate LP ID from which this message is coming
  unsigned int intm_lp_id;
  int old_vc;
  int saved_vc;

  int last_hop;

  // For buffer message
   int vc_index;
   unsigned int credit_delay;

   int input_chan;
   
   tw_stime saved_available_time;
   tw_stime saved_credit_time;
};

struct router_state
{
   unsigned int router_id;
   unsigned int group_id;
  
   int global_channel[GLOBAL_CHANNELS]; 

   // 0--NUM_ROUTER-1 local router indices (router-router intragroup channels)
   // NUM_ROUTER -- NUM_ROUTER+GLOBAL_CHANNELS-1 global channel indices (router-router inter-group channels)
   // NUM_ROUTER+GLOBAL_CHANNELS -- RADIX-1 terminal indices (router-terminal channels)
   tw_stime next_output_available_time[RADIX];
   tw_stime next_input_available_time[RADIX];
   tw_stime next_credit_available_time[RADIX];

   unsigned int credit_occupancy[RADIX];   
   unsigned int vc_occupancy[RADIX];

   unsigned int input_vc_state[RADIX];
   unsigned int output_vc_state[RADIX];
};

static int       nlp_terminal_per_pe;
static int       nlp_router_per_pe;
static int opt_mem = 10000;

tw_stime         average_travel_time = 0;
tw_stime         total_time = 0;
tw_stime         max_latency = 0;

int range_start;
int terminal_rem=0, router_rem=0;
int num_terminal=0, num_router=0;
unsigned long num_groups = NUM_ROUTER*GLOBAL_CHANNELS+1;
int total_routers, total_terminals;

static unsigned long long       total_hops = 0;
static unsigned long long       N_finished = 0;
static unsigned long long       N_finished_storage[N_COLLECT_POINTS];
static unsigned long long       N_generated_storage[N_COLLECT_POINTS];

void dragonfly_mapping(void);
tw_lp * dragonfly_mapping_to_lp(tw_lpid lpid);
#endif
