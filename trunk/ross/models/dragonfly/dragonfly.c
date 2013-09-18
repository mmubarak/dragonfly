#include "dragonfly.h"

// Local router ID: 0 --- total_router-1
// Router LP ID 
// Terminal LP ID

////////////////////////////////////////////////// Router-Group-Terminal mapping functions ///////////////////////////////////////

FILE * dragonfly_event_log=NULL;

tw_peid mapping(tw_lpid gid)
{
  int rank = (tw_peid)gid/range_start;
  
  return rank; 
}

// Given a terminal ID, get its router's LP ID
tw_lpid getRouterLPID_Terminal(tw_lpid terminal_id)
{
  int node_id=mapping(terminal_id);
  
  int local_router_id=(int)((terminal_id - ((node_id+1)* nlp_router_per_pe))/NUM_TERMINALS);
  
  int global_router_id=local_router_id + ((int)(local_router_id/nlp_router_per_pe) * nlp_terminal_per_pe);

  if(global_router_id < 0)
   return 0;
  
  return global_router_id; 
}

// Given a terminal ID, get its router local ID 0---total_routers-1
tw_lpid getRouterIDFromTerminal(tw_lpid terminal_id)
{
  int node_id=mapping(terminal_id);
  
  int local_router_id=(int)((terminal_id - ((node_id+1)* nlp_router_per_pe))/NUM_TERMINALS);
  
  return local_router_id;
}
// Given router LP ID, get the local router ID
tw_lpid getRouterID(tw_lpid router_lpid)
{
  int node_id=mapping(router_lpid);
  
  int router_id=router_lpid-(node_id * nlp_terminal_per_pe);
  
  return router_id;
}

// Given terminal LP ID, get the local terminal ID
tw_lpid getTerminalID(tw_lpid terminal_lpid)
{
  int node_id=mapping(terminal_lpid);

  int terminal_id=terminal_lpid-((node_id+1)*nlp_router_per_pe);

  return terminal_id;
}

//Given a lp id check if it is mapped to a router id or a terminal
int check_router_lpid(tw_lpid gid)
{
   int node_id=mapping(gid);
  
   int offset=node_id * (nlp_router_per_pe + nlp_terminal_per_pe);
  
   if((gid-offset) < nlp_router_per_pe)
     return 1;
  
   return 0;
}

// Given local router id, get its LP ID
tw_lpid getRouterLPID(tw_lpid local_router_id)
{
  int global_router_id=local_router_id + ((int)(local_router_id/nlp_router_per_pe)*nlp_terminal_per_pe);
  
  return global_router_id;
}

//////////////////////////////////////// Get router in the group which has a global channel to group id gid /////////////////////////////////
tw_lpid getRouterFromGroupID(int gid, router_state * r)
{
  int group_begin=r->group_id*NUM_ROUTER;
  
  int group_end=(r->group_id*NUM_ROUTER) + NUM_ROUTER-1;
  
  int offset=(gid*NUM_ROUTER-group_begin)/NUM_ROUTER;
  
  if((gid*NUM_ROUTER)<group_begin)
    offset=(group_begin-gid*NUM_ROUTER)/NUM_ROUTER; // take absolute value
  
  int half_channel=GLOBAL_CHANNELS/2;
  
  int index=(offset-1)/(half_channel * NUM_ROUTER);
  
  offset=(offset-1)%(half_channel * NUM_ROUTER);

  // If the destination router is in the same group
  tw_lpid router_id;

  if(index%2 != 0)
    router_id=group_end - (offset/half_channel); // start from the end
  else
    router_id=group_begin + (offset/half_channel);

  return router_id;
}	

/////////////////////////////////////// Credit buffer ////////////////////////////////////////////////////////////////////////////////////////
void router_credit_send(router_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp, int output_chan)
{
  tw_event * buf_e;

  terminal_message * buf_msg;

  int dest, credit_delay;

  int ts_inc = 0;

 // Notify sender terminal about available buffer space
 
 if(msg->last_hop == TERMINAL)
  {
   dest = msg->src_terminal_id;
 
   credit_delay = TERMINAL_DELAY;
  }
   else if(msg->last_hop == GLOBAL)
   {
     dest = msg->intm_lp_id;

     credit_delay = GLOBAL_DELAY;
   }
    else if(msg->last_hop == LOCAL)
     {
	dest = msg->intm_lp_id;
	
	credit_delay = LOCAL_DELAY;
     }
    else
      printf("\n Invalid message type");

  s->next_credit_available_time[output_chan] = max(s->next_credit_available_time[output_chan], tw_now(lp));

  s->next_credit_available_time[output_chan] += 0.5;

  buf_e = tw_event_new(dest, credit_delay + s->next_credit_available_time[output_chan] - tw_now(lp), lp); 

  buf_msg = tw_event_data(buf_e);

  buf_msg->vc_index = msg->saved_vc;

  buf_msg->type=BUFFER;

  buf_msg->packet_ID=msg->packet_ID;

  if( msg->packet_ID == TRACK )
    {
       printf("(%lf) [Router %d] packet %lld sending credit to %d channel %d \n",
              tw_now(lp), (int)lp->gid, msg->packet_ID, dest, buf_msg->vc_index);
    }

  tw_event_send(buf_e);
}

/////////////////////////////////// Packet generate, receive functions ////////////////////////////////////////////
void packet_generate(terminal_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
  tw_lpid dst_lp;
  tw_stime ts;
  tw_event *e;
  terminal_message *m;

  // Before generating a packet, check if the input queue is available
   int chan=-1, i;
   for(i=0; i<NUM_VC; i++)
    {
     if(s->output_vc_state[i] == VC_IDLE)
      {
       chan=i;
       break;
      }
    }
  if(chan != -1) // If the input queue is available
  {
   // Send the packet out
   s->output_vc_state[chan] = VC_ACTIVE;

   e = tw_event_new(lp->gid, 0, lp);
  
   m = tw_event_data(e);
  
   m->type = T_SEND;

   m->saved_vc=chan;

   m->src_terminal_id=(int)lp->gid;

   // Set up random destination
   dst_lp = tw_rand_integer(lp->rng,0, total_routers+total_terminals-1);

   int isRouter=check_router_lpid(dst_lp);

   while((dst_lp==lp->gid) || (isRouter==1))
   {
    dst_lp = tw_rand_integer(lp->rng,0, total_routers+total_terminals-1);
  
    isRouter=check_router_lpid(dst_lp);
   }
  
   // record start time
   m->travel_start_time = tw_now(lp);
  
   m->my_N_hop = 0;
  
   // set up packet ID
   m->packet_ID = lp->gid + total_terminals*s->packet_counter;
  
   m->dest_terminal_id=dst_lp;
  
   tw_event_send(e);

#if DEBUG
  if(m->packet_ID == TRACK)
    printf("\n (%lf) [Terminal %d]: Packet %lld generated ", tw_now(lp), lp->gid, m->packet_ID);
#endif

  // One more packet is generating 
  s->packet_counter++;
  
  int index = floor(N_COLLECT_POINTS*(tw_now(lp)/g_tw_ts_end));

  N_generated_storage[index]++;

  // schedule next GENERATE event
  e = tw_event_new(lp->gid, MEAN_INTERVAL, lp);
  
  m = tw_event_data(e);
  
  m->type = T_GENERATE;

  tw_event_send(e);
 }
 else
  {
    //schedule a generate event after a certain delay
     e = tw_event_new(lp->gid, RESCHEDULE_DELAY, lp);

     m = tw_event_data(e);

     m->type = T_GENERATE;

     tw_event_send(e);
  }
}

void packet_send(terminal_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
#if DEBUG
 if( msg->packet_ID == TRACK )
  {
    printf("\n (%lf) [Terminal %d] Packet %lld being sent to source router %d Output VC : %d \n", tw_now(lp), (int)lp->gid, msg->packet_ID,  getRouterID(s->router_id), msg->saved_vc);
  }
#endif

  tw_stime ts;
  tw_event *e;
  terminal_message *m;

  /* Route the packet to its source router */ 

  int vc=msg->saved_vc;

  if(s->vc_occupancy[vc] > VC_BUF_SIZE)
   printf("\n (%lf) [Terminal %d] Invalid VC Occupancy ***********", tw_now(lp), lp->gid);

  s->vc_occupancy[vc]++;

#if DEBUG
 if( msg->packet_ID == TRACK )
  {
    printf("\n (%lf) [Terminal %d] VC %d buffer status %d ", tw_now(lp), (int)lp->gid, msg->packet_ID, vc, s->vc_occupancy[vc]);
  }
#endif
  ts = TERMINAL_DELAY;   

  e = tw_event_new(s->router_id, ts, lp);

  m = tw_event_data(e);

  m->type = R_ARRIVE;

    // Carry on the message info
  m->dest_terminal_id = msg->dest_terminal_id;

  m->src_terminal_id = msg->src_terminal_id;
  
  m->packet_ID = msg->packet_ID;
  
  m->travel_start_time = msg->travel_start_time;
  
  m->my_N_hop = msg->my_N_hop;

  m->saved_vc = msg->saved_vc;

  m->last_hop = TERMINAL;

  m->input_chan = -1;
  
  s->output_vc_state[vc] = VC_IDLE;
  
  if(s->vc_occupancy[vc] == VC_BUF_SIZE)
  {
    s->output_vc_state[vc] = VC_CREDIT;
  }

  tw_event_send(e);
}

void packet_arrive(terminal_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
#if DEBUG
  if( msg->packet_ID == TRACK )
    {
	printf( "(%lf) [Terminal %d] packet %lld has arrived  \n",
              tw_now(lp), (int)lp->gid, msg->packet_ID);

	printf("travel start time is %f\n",
                msg->travel_start_time);

	printf("My hop now is %d\n",msg->my_N_hop);
    }
#endif
  tw_stime ts;
  tw_event *e, *buf_e;
  terminal_message *m;
  terminal_message * buf_msg;

  // Packet arrives and accumulate # queued
  // Find a queue with an empty buffer slot
  ts = MEAN_PROCESS;

  e = tw_event_new(lp->gid, ts, lp);

  m = tw_event_data(e);

  m->type = T_PROCESS;

  m->dest_terminal_id = msg->dest_terminal_id;

  m->packet_ID = msg->packet_ID;

  m->travel_start_time = msg->travel_start_time;

  m->my_N_hop = msg->my_N_hop;

  m->src_terminal_id = msg->src_terminal_id;

  tw_event_send(e);  

  buf_e = tw_event_new(msg->intm_lp_id, LOCAL_DELAY, lp);

  buf_msg = tw_event_data(buf_e);

  buf_msg->vc_index = msg->saved_vc;

  buf_msg->type=BUFFER;

  buf_msg->packet_ID=msg->packet_ID;

  tw_event_send(buf_e);
  // Update the downstream router's buffer
  #if DEBUG
  if( msg->packet_ID == TRACK )
    {
       printf("(%lf) [Terminal %d] packet %lld sending credit to %d channel %d \n",
              tw_now(lp), (int)lp->gid, msg->packet_ID, msg->intm_lp_id, msg->saved_vc);
    }
  #endif
}

void packet_process(terminal_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
#if DEBUG
if(msg->packet_ID == TRACK)
  printf( "\n (%lf) [Terminal %d] Packet %lld processing at LP \n", tw_now(lp), (int)lp->gid, msg->packet_ID);
#endif
  
  bf->c3 = 1;
  tw_event * e;

  if(lp->gid==msg->dest_terminal_id)
    {
      // one packet arrives and dies
      bf->c3 = 0;
  
      N_finished++;
  
      int index = floor(N_COLLECT_POINTS*(tw_now(lp)/g_tw_ts_end));
      N_finished_storage[index]++;
  
      total_time += tw_now(lp) - msg->travel_start_time;
  
      if (max_latency<tw_now(lp) - msg->travel_start_time)
        max_latency=tw_now(lp) - msg->travel_start_time;
 
      total_hops += msg->my_N_hop;
     
    }
  else
    {
     printf("\n Packet %lld LP: %d Not arrived at correct destination: %d ", msg->packet_ID, (int)lp->gid, msg->dest_terminal_id);
    }
}
////////////////////////////////////////////////// Terminal related functions ///////////////////////////////////////

void terminal_setup(terminal_state * s, tw_lp * lp)
{
    int i, j;

    s->terminal_id=((int)lp->gid);  
 
    // Assign the global router ID
    s->router_id=getRouterLPID_Terminal(lp->gid);
 
    s->packet_counter = 0;

   for(i=0; i < NUM_VC; i++)
    {
      s->vc_occupancy[i]=0;

      s->output_vc_state[i]=VC_IDLE;
    }
}

void terminal_init(terminal_state * s, tw_lp * lp)
{
    tw_event *e;
   
    tw_stime ts;
    terminal_message *m;
   
    terminal_setup(s, lp);

  /** Start a GENERATE event on each LP **/
    ts = tw_rand_exponential(lp->rng, MEAN_INTERVAL);

    e = tw_event_new(lp->gid, ts, lp);
   
    m = tw_event_data(e);
   
    m->type = T_GENERATE;
    
    tw_event_send(e);
}

void terminal_buf_update(terminal_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
  // Update the buffer space associated with this router LP 
    int msg_indx = msg->vc_index;
    
    if(s->vc_occupancy[msg_indx]>VC_BUF_SIZE)
      printf("\n (%lf) [Terminal %d] ERROR: TERMINAL BUFFER OVERFLOW %d msg_indx %d msg type %d ", tw_now(lp), lp->gid, s->vc_occupancy[msg_indx], msg_indx, msg->last_hop);

    if(msg->packet_ID == TRACK)
      printf("\n (%lf) [Terminal %d] VC OCCUPANCY for channel %d is %d Packet ID %lld", tw_now(lp), lp->gid, msg_indx, s->vc_occupancy[msg_indx], msg->packet_ID);

    s->vc_occupancy[msg_indx]--;
   
    if(s->vc_occupancy < 0)
      printf("\n ERROR: TERMINAL BUFFER SIZE BELOW ZERO");
}

void terminal_event(terminal_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
  switch(msg->type)
    {
    case T_GENERATE:
      packet_generate(s,bf,msg,lp);
      break;
    
    case T_ARRIVE:
      packet_arrive(s,bf,msg,lp);
      break;
    
    case T_SEND:
      packet_send(s,bf,msg,lp);
      break;
    
    case T_PROCESS:
      packet_process(s,bf,msg,lp);
      break;

    case BUFFER:
      terminal_buf_update(s, bf, msg, lp);
     break;
  
    default:
       printf("\n LP %d Terminal message type not supported", lp->gid);
    }
}

void terminal_rc_event(terminal_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
}

void final(terminal_state * s, tw_lp * lp)
{

}

/////////////////////////////////////////// Router packet send/receive functions //////////////////////

void router_buf_update(router_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
   // Update the buffer space associated with this router LP 
    int msg_indx = msg->vc_index;

    if(s->vc_occupancy[msg_indx]>VC_BUF_SIZE)
      printf("\n ERROR ROUTER BUFFER OVERFLOW %d ", s->vc_occupancy[msg_indx]);

    if(msg->packet_ID == TRACK)
      printf("\n (%lf) [Router %d] VC OCCUPANCY for channel %d is %d Packet ID %lld", tw_now(lp), lp->gid, msg_indx, s->vc_occupancy[msg_indx], msg->packet_ID);

    s->vc_occupancy[msg_indx]--;

    if(s->vc_occupancy < 0)
      printf("\n ERROR: ROUTER BUFFER SIZE BELOW ZERO");
}

// Determine the input channel at which the message has arrived
int get_input_chan(router_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
  int input_chan = -1;
  int sender = -1;
  int i;

  if(msg->last_hop == TERMINAL)
  {
   sender = msg->src_terminal_id;

   input_chan = NUM_VC * (NUM_ROUTER + GLOBAL_CHANNELS) + getTerminalID(sender)%NUM_TERMINALS;
   }

   else if(msg->last_hop == LOCAL)
   {
     sender = msg->intm_lp_id;

     input_chan = NUM_VC * (getRouterID(sender) % NUM_ROUTER);
   }
    else if(msg->last_hop == GLOBAL)
       {
	 sender = msg->intm_lp_id;

	 for(i=0; i<GLOBAL_CHANNELS; i++)
         {
	   if(s->global_channel[i]/NUM_ROUTER == (getRouterID(sender)/NUM_ROUTER))
	     input_chan = NUM_VC * (NUM_ROUTER + i);
	 }

	if(input_chan == -1)
	  printf("\n (%lf) [Router %d] Input channel not found for packet %lld sender %d ", tw_now(lp), lp->gid, msg->packet_ID, getRouterID(sender));
       }

  return input_chan;
}
void router_reschedule_event(router_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
 // Check again after some time
  terminal_message * m;

  tw_event * e;
 
  e = tw_event_new(lp->gid, RESCHEDULE_DELAY, lp);

  m = tw_event_data(e);

  m->travel_start_time = msg->travel_start_time;

  m->dest_terminal_id = msg->dest_terminal_id;

  m->packet_ID = msg->packet_ID;

  m->type = msg->type;

  m->my_N_hop = msg->my_N_hop;

  m->intm_lp_id = msg->intm_lp_id;

  m->saved_vc = msg->saved_vc;
  
  m->src_terminal_id = msg->src_terminal_id;

  m->last_hop = msg->last_hop;

  m->input_chan = msg->input_chan;

  tw_event_send(e);

 if(msg->packet_ID == TRACK)
  printf("\n (%lf) [Router %d] rescheduled packet %lld ", tw_now(lp), lp->gid, msg->packet_ID);

}

void router_packet_send(router_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
  tw_stime ts;
  tw_event *e;
  terminal_message *m;

  int dest_router_id=getRouterLPID_Terminal(msg->dest_terminal_id);

  int dst_lp;

  int dest_group_id=getRouterIDFromTerminal(msg->dest_terminal_id)/NUM_ROUTER;

  int i, global=0, t_send=0;
  int output_chan=-1;

  int delay= LOCAL_DELAY;

  int input_chan = msg->input_chan;

  // Send to a terminal
  if(dest_router_id == lp->gid)
  {
   output_chan = (NUM_VC * NUM_ROUTER) + (NUM_VC * GLOBAL_CHANNELS) +(NUM_VC * (getTerminalID(msg->dest_terminal_id)%NUM_TERMINALS));
 
   dst_lp = msg->dest_terminal_id;

   t_send=1;
  }

  // Send to a local router
  if(!t_send && dest_group_id != s->group_id)
  {
   dst_lp=getRouterLPID(getRouterFromGroupID(dest_group_id,s));  
 
   output_chan = NUM_VC * (getRouterID(dst_lp)%NUM_ROUTER);
  }
  
 // Send over a global channel
  if(!t_send && dst_lp == lp->gid)
   {
     for(i=0; i<GLOBAL_CHANNELS; i++)
       {
         if(s->global_channel[i]/NUM_ROUTER == dest_group_id)
         {
           dst_lp=getRouterLPID(s->global_channel[i]);
           
	   output_chan = NUM_VC * (NUM_ROUTER + i);
           
	   delay = GLOBAL_DELAY;
           
	   global=1;
         }
       }
   }
  
  if(!t_send && dest_group_id == s->group_id)
   {
      dst_lp=dest_router_id;

      output_chan = NUM_VC * (getRouterID(dst_lp)%NUM_ROUTER);
   }
     
  // Allocate output Virtual Channel
#if DEBUG
  if( msg->packet_ID == TRACK && !t_send)
  {
   printf("\n (%lf) [Router %d] Packet %lld being sent to intermediate group router %d Final destination router %d Output Channel Index %d \n", 
              tw_now(lp), (int)lp->gid, msg->packet_ID, getRouterID(dst_lp), 
	      getRouterIDFromTerminal(msg->dest_terminal_id), output_chan);
  }
#endif
  // If the output virtual channel is not available, then hold the input virtual channel too
   if(s->output_vc_state[output_chan] != VC_IDLE)
    {
       // Re-schedule the event, keep holding input virtual channel
        router_reschedule_event(s, bf, msg, lp);
        
        return;
    }

 s->input_vc_state[input_chan] = VC_IDLE;

 int buf_size = s->vc_occupancy[output_chan];

 // If source router doesn't have global channel and buffer space is available, then assign to appropriate intra-group virtual channel 
  s->output_vc_state[output_chan] = VC_ACTIVE;

  ts = delay;

  s->next_output_available_time[output_chan] = max(s->next_output_available_time[output_chan], tw_now(lp));

  s->next_output_available_time[output_chan] += 0.5;

  e = tw_event_new(dst_lp, s->next_output_available_time[output_chan] + ts - tw_now(lp), lp);

  m = tw_event_data(e);

  if(global)
   {
    m->last_hop=GLOBAL;
   }
  else
   {
    m->last_hop = LOCAL;
   }

  m->saved_vc = output_chan;

  m->intm_lp_id = lp->gid;

  router_credit_send(s, bf, msg, lp, output_chan);

  // Carry on the message information
  m->dest_terminal_id = msg->dest_terminal_id;

  m->packet_ID = msg->packet_ID;

  m->travel_start_time = msg->travel_start_time;

  m->src_terminal_id = msg->src_terminal_id;

  m->my_N_hop = msg->my_N_hop;

 if(t_send)
 {
  m->type = T_ARRIVE;
 }
 else
 {
  m->type = R_ARRIVE;
 }

  s->output_vc_state[output_chan] = VC_IDLE;
   
  s->vc_occupancy[output_chan]++;

  if(s->vc_occupancy[output_chan] == VC_BUF_SIZE)
      s->output_vc_state[output_chan] = VC_CREDIT;

  tw_event_send(e);
}

// Packet arrives at the router
void router_packet_receive(router_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
  // STEP 1: Allocate an input virtual channel 
  int input_chan = get_input_chan(s, bf, msg, lp);

  if(s->input_vc_state[input_chan] != VC_IDLE)
   {
     // Re-schedule the event
     router_reschedule_event(s, bf, msg, lp);
     return;
   }

  msg->my_N_hop++;

  // STEP 2: Route Computation
  s->input_vc_state[input_chan]=VC_ALLOC;

  tw_stime ts = 0;

  tw_event *e, * buf_e;

  terminal_message *m;

  s->next_input_available_time[input_chan] = max(s->next_input_available_time[input_chan], tw_now(lp));

  s->next_input_available_time[input_chan] += 0.5;

  e = tw_event_new(lp->gid, s->next_input_available_time[input_chan] + ts - tw_now(lp), lp);

  m = tw_event_data(e);

  m->saved_vc = msg->saved_vc;

  m->intm_lp_id = msg->intm_lp_id;

 if( msg->packet_ID == TRACK )
 {
  printf(" \n (%lf) [Router %d] packet %lld arrived at intermediate router \n",
          tw_now(lp), (int)lp->gid, msg->packet_ID);
 }
 // Carry on the message information
  m->dest_terminal_id = msg->dest_terminal_id;

  m->src_terminal_id = msg->src_terminal_id;

  m->packet_ID = msg->packet_ID;

  m->travel_start_time = msg->travel_start_time;

  m->my_N_hop = msg->my_N_hop;

  m->last_hop = msg->last_hop;

  m->input_chan = input_chan;

  m->type = R_SEND;

  tw_event_send(e);  
}
/////////////////////////////////////////// Router related functions /////////////////////////////////
void router_setup(router_state * r, tw_lp * lp)
{
   r->router_id=((int)lp->gid);
   
   int local_router_id=getRouterID(lp->gid);

   r->group_id=getRouterID(lp->gid)/NUM_ROUTER;

   int i, j;
   int offset=(local_router_id%NUM_ROUTER) * (GLOBAL_CHANNELS/2) +1;
  
   for(i=0; i < RADIX; i++)
    {
	r->next_input_available_time[i]=0;

	r->next_output_available_time[i]=0;

	r->next_credit_available_time[i]=0;

       // Set credit & router occupancy
       r->vc_occupancy[i]=0;

       // Set virtual channel state to idle
       r->input_vc_state[i] = VC_IDLE;

       r->output_vc_state[i]= VC_IDLE;
    }

   //round the number of global channels to the nearest even number
   for(i=0; i<GLOBAL_CHANNELS; i++)
    {
      if(i%2!=0)
          {
             r->global_channel[i]=(local_router_id + (offset*NUM_ROUTER))%total_routers;
             offset++;
          }
          else
           {
             r->global_channel[i]=local_router_id-((offset)*NUM_ROUTER);
           }
        if(r->global_channel[i]<0)
         {
           r->global_channel[i]=total_routers+r->global_channel[i]; 
	 }
   
  #if PRINT_ROUTER_TABLE
	fprintf(dragonfly_event_log, "\n Router %d setup ", lp->gid);

	fprintf(dragonfly_event_log, "\n Router %d connected to Router %d Group %d to Group %d ", local_router_id, r->global_channel[i], r->group_id, (r->global_channel[i]/NUM_ROUTER));
   #endif
    }
}	
void router_event(router_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
  switch(msg->type)
   {
	   case R_SEND: // Router has sent a packet to an intra-group router (local channel)
 		   router_packet_send(s, bf, msg, lp);
           break;

	   case R_ARRIVE: // Router has received a packet from an intra-group router (local channel)
	          router_packet_receive(s, bf, msg, lp);
	   break;
	
	   case BUFFER:
		  router_buf_update(s, bf, msg, lp);
	   break;

	   default:
		  printf("\n (%lf) [Router %d] Router Message type not supported %d", tw_now(lp), (int)lp->gid, msg->type);
	   break;
   }	   
}

void router_rc_event(router_state* s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
}

////////////////////////////////////////////////////// LP TYPES /////////////////////////////////////////////////
tw_lptype terminals_lps[] =
{
   // Terminal handling functions
   {
    (init_f)terminal_init,
   
    (event_f) terminal_event,
   
    (revent_f) terminal_rc_event,
   
    (final_f) final,
   
    (map_f) mapping,
   
    sizeof(terminal_state)
    },
   {
     (init_f) router_setup,
   
     (event_f) router_event,
   
     (revent_f) router_rc_event,
   
     (final_f) final,
   
     (map_f) mapping,
   
     sizeof(router_state),
   },
   {0},
};

const tw_optdef app_opt [] =
{
   TWOPT_GROUP("Dragonfly Model"),
   TWOPT_UINT("memory", opt_mem, "optimistic memory"),
   TWOPT_STIME("arrive_rate", MEAN_INTERVAL, "packet arrive rate"),
   TWOPT_END()
};

////////////////////////////////////////////////////// MAIN ///////////////////////////////////////////////////////
int main(int argc, char **argv, char **env)
{
     char log[32];
     tw_opt_add(app_opt);
   
     tw_init(&argc, &argv);
	 
     MEAN_INTERVAL = 10;
   
     total_routers=NUM_ROUTER*num_groups;
   
     total_terminals=NUM_ROUTER*NUM_TERMINALS*num_groups;

     nlp_terminal_per_pe = total_terminals/tw_nnodes()/g_tw_npe;
   
     nlp_router_per_pe = total_routers/tw_nnodes()/g_tw_npe;
   
     g_tw_events_per_pe = 5 * (nlp_terminal_per_pe/g_tw_npe * g_tw_ts_end/MEAN_INTERVAL) + opt_mem;

     range_start=nlp_router_per_pe + nlp_terminal_per_pe; 

#if DEBUG
     sprintf( log, "dragonfly-log.%d", g_tw_mynode );
     dragonfly_event_log=fopen(log, "w+");

     if(dragonfly_event_log == NULL)
	tw_error(TW_LOC, "\n Failed to open dragonfly event log file \n");
#endif

     tw_define_lps(nlp_terminal_per_pe+nlp_router_per_pe, sizeof(terminal_message), 0);


#if DEBUG
          printf("\n g_tw_nlp is %lu g_tw_npe %lu g_tw_mynode: %d \n ", g_tw_nlp, g_tw_npe, g_tw_mynode);

	  printf("\n nlp_terminal_per_pe is %d, nlp_router_per_pe is %d \n ", nlp_terminal_per_pe, nlp_router_per_pe);
#endif

     int i;
     
     for(i = 0; i < nlp_router_per_pe; i++)
      tw_lp_settype(i, &terminals_lps[1]);


     for(i = nlp_router_per_pe; i < g_tw_nlp; i++)
	 tw_lp_settype(i, &terminals_lps[0]);

    tw_run();

    if(tw_ismaster())
    {
      printf("\nDragonfly Network Model Statistics:\n");
      printf("\t%-50s %11lld\n", "Number of nodes", nlp_terminal_per_pe * g_tw_npe * tw_nnodes());
    }

    unsigned long long total_finished_storage[N_COLLECT_POINTS];
 
    unsigned long long total_generated_storage[N_COLLECT_POINTS];
  
    unsigned long long N_total_finish,N_total_hop;

   tw_stime total_time_sum,g_max_latency;

   for( i=0; i<N_COLLECT_POINTS; i++ )
    {
     MPI_Reduce( &N_finished_storage[i], &total_finished_storage[i],1,
                 MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
   
     MPI_Reduce( &N_generated_storage[i], &total_generated_storage[i],1,
                  MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
     }
   	MPI_Reduce( &total_time, &total_time_sum,1,
                    MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
   
   	MPI_Reduce( &N_finished, &N_total_finish,1,
                    MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
   
   	MPI_Reduce( &total_hops, &N_total_hop,1,
                    MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
   
   	MPI_Reduce( &max_latency, &g_max_latency,1,
                    MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

      for( i=1; i<N_COLLECT_POINTS; i++ )
          {
            total_finished_storage[i]+=total_finished_storage[i-1];
            total_generated_storage[i]+=total_generated_storage[i-1];
          }
   
      if(tw_ismaster())
          {
            printf("\n ****************** \n");
    
    	    printf("\n total finish:         %lld and %lld; \n",
                   total_finished_storage[N_COLLECT_POINTS-1],N_total_finish);
    
    	    printf("\n total generate:       %lld; \n",
                   total_generated_storage[N_COLLECT_POINTS-1]);
    
    	    printf("\n total hops:           %lf; \n",
                   (double)N_total_hop/total_finished_storage[N_COLLECT_POINTS-1]);
    
    	    printf("\n average travel time:  %lf; \n\n",
                   total_time_sum/total_finished_storage[N_COLLECT_POINTS-1]);

            for( i=0; i<N_COLLECT_POINTS; i++ )
              {
                printf(" %d ",i*100/N_COLLECT_POINTS);
                printf("finish: %lld; generate: %lld; alive: %lld\n",
                       total_finished_storage[i],
                       total_generated_storage[i],
                       total_generated_storage[i]-total_finished_storage[i]);

              }

            // capture the steady state statistics
            unsigned long long steady_sum=0;
            for( i = N_COLLECT_POINTS/2; i<N_COLLECT_POINTS;i++)
              steady_sum+=total_generated_storage[i]-total_finished_storage[i];
            printf("\n Steady state, packet alive: %lld\n",
                   2*steady_sum/N_COLLECT_POINTS);

            printf("\nMax latency is %lf\n\n",g_max_latency);

          }

   return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
