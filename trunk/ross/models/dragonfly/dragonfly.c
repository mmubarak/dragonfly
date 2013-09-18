#include "dragonfly.h"

// Local router ID: 0 --- total_router-1
// Router LP ID 
// Terminal LP ID

////////////////////////////////////////////////// Router-Group-Terminal mapping functions ///////////////////////////////////////

FILE * dragonfly_event_log=NULL;

int get_terminal_rem()
{
//  if(terminal_rem > 0 && gid/(num_terminal+1) > terminal_rem)
//    return terminal_rem;

   if(terminal_rem > 0 && terminal_rem <= g_tw_mynode)
    return terminal_rem;

   return 0;
}

int get_router_rem()
{
  if(router_rem > 0 && router_rem <= g_tw_mynode)
   return router_rem;

  return 0;
}

int getTerminalID(tw_lpid lpid)
{
  return lpid - total_routers;
}

int getRouterID(tw_lpid terminal_id)
{
  int tid = getTerminalID(terminal_id);
  return tid/NUM_TERMINALS; 
}

tw_peid mapping( tw_lpid gid)
{
   int rank;
   int offset;
   int rem = 0;
   int nlp_per_pe;
   int N_nodes = tw_nnodes();

   if(gid < total_routers)
    {
     rank = gid / nlp_router_per_pe;

     rem = router_rem;

     if(nlp_router_per_pe == (total_routers/N_nodes))
            offset = (nlp_router_per_pe + 1) * router_rem;
     else
	    offset = nlp_router_per_pe * router_rem;

     nlp_per_pe = nlp_router_per_pe;
    }
    else
     {
     rank = getTerminalID(gid)/nlp_terminal_per_pe;	

     rem = terminal_rem;

     if(nlp_terminal_per_pe == (total_terminals/N_nodes))
            offset = total_routers + (nlp_terminal_per_pe + 1) * terminal_rem;
     else
  	    offset = total_routers + nlp_terminal_per_pe * terminal_rem;     

     nlp_per_pe = nlp_terminal_per_pe;
     }

   if(rem > 0)
    {
     if(g_tw_mynode >= rem)
        {
	  if(gid < offset) 
	    rank = gid / (nlp_per_pe + 1);
	  else
	    rank = rem + ((gid - offset)/nlp_per_pe);
	}
     else
 	 {
	   if(gid >= offset)
	     rank = rem + ((gid - offset)/(nlp_per_pe - 1)); 
	 }
    }
/*   if(gid <= 369 && gid >= 264 && rank != 0)
     printf("\n ERROR INCORRECT RANK %d LPID %d", rank, gid);
*/
   return rank;
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

  msg->saved_credit_time = s->next_credit_available_time[output_chan];

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

  bf->c3 = 1;
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

  int terminal_grp_id = (int)lp->gid / (NUM_TERMINALS * NUM_ROUTER);

  int next_group_begin = (terminal_grp_id + 1) * NUM_TERMINALS * NUM_ROUTER;

   int next_group_end = (terminal_grp_id + 2) * NUM_TERMINALS * NUM_ROUTER - 1;

  if(chan != -1) // If the input queue is available
  {
   // Send the packet out
   s->output_vc_state[chan] = VC_ACTIVE;

   ts = 0.1 + tw_rand_exponential(lp->rng, (double)MEAN_INTERVAL/1000);

   e = tw_event_new(lp->gid, ts, lp);
  
   m = tw_event_data(e);
  
   m->type = T_SEND;

   m->saved_vc=chan;

   msg->saved_vc = chan;

   m->src_terminal_id=(int)lp->gid;

   // Set up random destination
   //dst_lp = tw_rand_integer(lp->rng, total_routers + next_group_begin, total_routers+next_group_end);
   dst_lp = tw_rand_integer(lp->rng, total_routers, total_routers+total_terminals-1);

   // record start time
   m->travel_start_time = tw_now(lp);
  
   m->my_N_hop = 0;
  
   // set up packet ID
   m->packet_ID = lp->gid + total_terminals*s->packet_counter;
  
   m->dest_terminal_id=dst_lp;
  
   tw_event_send(e);

#if DEBUG
  if(m->packet_ID == TRACK)
    printf("\n (%lf) [Terminal %d]: Packet %lld generated ", tw_now(lp), (int)lp->gid, m->packet_ID);
#endif

  // One more packet is generating 
  s->packet_counter++;
  
  int index = floor(N_COLLECT_POINTS*(tw_now(lp)/g_tw_ts_end));

  N_generated_storage[index]++;

  // schedule next GENERATE event
  ts = tw_rand_exponential(lp->rng, (double)MEAN_INTERVAL);

  e = tw_event_new(lp->gid, ts, lp);
  
  m = tw_event_data(e);
  
  m->type = T_GENERATE;

  tw_event_send(e);
 }
 else
  {
    //schedule a generate event after a certain delay
     bf->c3 = 0;

     ts = RESCHEDULE_DELAY + tw_rand_exponential(lp->rng, (double)RESCHEDULE_DELAY/10);

     //printf("\n (%lf) [Terminal %d]: Generate event rescheduled ", tw_now(lp), (int)lp->gid);

     e = tw_event_new(lp->gid, ts, lp);

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
    printf("\n (%lf) [Terminal %d] {Node %d} Packet %lld being sent to source router %d Output VC : %d \n", tw_now(lp), (int)lp->gid, (int)g_tw_mynode, msg->packet_ID,  (int)s->router_id, (int)msg->saved_vc);
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

  msg->saved_available_time = s->terminal_available_time;

  s->terminal_available_time = max(s->terminal_available_time, tw_now(lp));

  ts = TERMINAL_DELAY + tw_rand_exponential(lp->rng, (double)TERMINAL_DELAY/1000);   

  s->terminal_available_time += 1;

  e = tw_event_new(s->router_id, s->terminal_available_time + ts - tw_now(lp), lp);

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

  ts = LOCAL_DELAY + tw_rand_exponential(lp->rng, (double)LOCAL_DELAY/100);

  buf_e = tw_event_new(msg->intm_lp_id, ts, lp);

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
    s->router_id=getRouterID(lp->gid);

    //printf("\n (%lf) [Terminal %d] assigned router id %d ", tw_now(lp), (int)lp->gid, (int)s->router_id);
 
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
    ts = tw_rand_exponential(lp->rng, (double)MEAN_INTERVAL);

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

     input_chan = NUM_VC * (sender % NUM_ROUTER);
   }
    else if(msg->last_hop == GLOBAL)
       {
	 sender = msg->intm_lp_id;

	 for(i=0; i<GLOBAL_CHANNELS; i++)
         {
	   if(s->global_channel[i]/NUM_ROUTER == (sender/NUM_ROUTER))
	     input_chan = NUM_VC * (NUM_ROUTER + i);
	 }

	if(input_chan == -1)
	  printf("\n (%lf) [Router %d] Input channel not found for packet %lld sender %d ", tw_now(lp), lp->gid, msg->packet_ID, sender);
       }

  return input_chan;
}
void router_reschedule_event(router_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
 // Check again after some time
  terminal_message * m;

  tw_event * e;

  tw_stime ts = RESCHEDULE_DELAY + tw_rand_exponential(lp->rng, (double)RESCHEDULE_DELAY/10);

  e = tw_event_new(lp->gid, ts, lp);

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

/* if(msg->packet_ID == TRACK)
  printf("\n (%lf) [Router %d] rescheduled packet %lld ", tw_now(lp), lp->gid, msg->packet_ID);*/

}

void router_packet_send(router_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
  tw_stime ts;
  tw_event *e;
  terminal_message *m;

  int dest_router_id=getRouterID(msg->dest_terminal_id);

  int dst_lp=-1;

  int dest_group_id=getRouterID(msg->dest_terminal_id)/NUM_ROUTER;

  int i, global=0, t_send=0;
  int output_chan=-1;

  int delay= LOCAL_DELAY;

  int input_chan = msg->input_chan;

  bf->c3 = 1;

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
   dst_lp=getRouterFromGroupID(dest_group_id,s);  
 
   output_chan = NUM_VC * (dst_lp%NUM_ROUTER);
  }
  
 // Send over a global channel
  if(!t_send && dst_lp == lp->gid)
   {
     for(i=0; i<GLOBAL_CHANNELS; i++)
       {
         if(s->global_channel[i]/NUM_ROUTER == dest_group_id)
         {
           dst_lp=s->global_channel[i];
           
	   output_chan = NUM_VC * (NUM_ROUTER + i);
           
	   delay = GLOBAL_DELAY;
           
	   global=1;
         }
       }
   }
  
  if(!t_send && dest_group_id == s->group_id)
   {
      dst_lp=dest_router_id;

      output_chan = NUM_VC * (dst_lp%NUM_ROUTER);
   }
     
  // Allocate output Virtual Channel
#if DEBUG
 if( msg->packet_ID == TRACK && !t_send)
  {
   printf("\n (%lf) [Router %d] Packet %lld being sent to intermediate group router %d Final destination router %d Output Channel Index %d \n", 
              tw_now(lp), (int)lp->gid, msg->packet_ID, dst_lp, 
	      getRouterID(msg->dest_terminal_id), output_chan);
  }
#endif
  // If the output virtual channel is not available, then hold the input virtual channel too
   if(s->output_vc_state[output_chan] != VC_IDLE)
    {
       // Re-schedule the event, keep holding input virtual channel
        bf->c3 = 0;
  
  	router_reschedule_event(s, bf, msg, lp);
  
        return;
    }

 s->input_vc_state[input_chan] = VC_IDLE;

 // If source router doesn't have global channel and buffer space is available, then assign to appropriate intra-group virtual channel 
  msg->saved_available_time = s->next_output_available_time[output_chan];

  s->output_vc_state[output_chan] = VC_ACTIVE;

  ts = delay + tw_rand_exponential(lp->rng, (double)delay/1000);

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

  msg->old_vc = output_chan;

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
  
  bf->c3 = 1;

  if(s->input_vc_state[input_chan] != VC_IDLE)
   {
     // Re-schedule the event
     bf->c3 = 0;

     router_reschedule_event(s, bf, msg, lp);

     return;
   }

  msg->my_N_hop++;

  // STEP 2: Route Computation
  s->input_vc_state[input_chan]=VC_ALLOC;

  //tw_stime ts = 0.1 + tw_rand_exponential(lp->rng, (double)0.1);

  tw_event *e, * buf_e;

  terminal_message *m;

  msg->saved_available_time = s->next_input_available_time[input_chan];

  s->next_input_available_time[input_chan] = max(s->next_input_available_time[input_chan], tw_now(lp));

  s->next_input_available_time[input_chan] += 0.5;

  e = tw_event_new(lp->gid, s->next_input_available_time[input_chan] - tw_now(lp), lp);

  m = tw_event_data(e);

  msg->input_chan = input_chan;

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
   
   r->group_id=lp->gid/NUM_ROUTER;

   int i, j;
   int offset=(lp->gid%NUM_ROUTER) * (GLOBAL_CHANNELS/2) +1;
  
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
             r->global_channel[i]=(lp->gid + (offset*NUM_ROUTER))%total_routers;
             offset++;
          }
          else
           {
             r->global_channel[i]=lp->gid-((offset)*NUM_ROUTER);
           }
        if(r->global_channel[i]<0)
         {
           r->global_channel[i]=total_routers+r->global_channel[i]; 
	 }
   
  #if PRINT_ROUTER_TABLE
	//fprintf(dragonfly_event_log, "\n Router %d setup ", lp->gid);


	//fprintf(dragonfly_event_log, "\n Router %d connected to Router %d Group %d to Group %d ", local_router_id, r->global_channel[i], r->group_id, (r->global_channel[i]/NUM_ROUTER));
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

/////////////////////////////////////////// REVERSE EVENT HANDLERS ///////////////////////////////////
//
void terminal_rc_event_handler(terminal_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
   int index = floor(N_COLLECT_POINTS*(tw_now(lp)/g_tw_ts_end));

   switch(msg->type)
   {
	   case T_GENERATE:
		 {
		  if(bf->c3 == 1)
		  {
		     int vc = msg->saved_vc;

		     N_generated_storage[index]--;
  	         
		     s->packet_counter--;
		 
		     s->output_vc_state[vc] = VC_IDLE;

		     tw_rand_reverse_unif(lp->rng);

		     tw_rand_reverse_unif(lp->rng);
		  }
		    tw_rand_reverse_unif(lp->rng);

		 }
	   break;
	   
	   case T_SEND:
	         {
		   int vc = msg->saved_vc;
		   
		   s->vc_occupancy[vc]--;
		   
		   tw_rand_reverse_unif(lp->rng);
		   
		   s->terminal_available_time = msg->saved_available_time;

		   s->output_vc_state[vc] = VC_ACTIVE;
		 }
	   break;

	   case T_ARRIVE:
	   	 {
		    tw_rand_reverse_unif(lp->rng);	 
		 }
           break;

	   case T_PROCESS:
	        if ( bf->c3 == 0 )
		   {
		      N_finished--;
		      
		      N_finished_storage[index]--;
		      
		      total_time -= tw_now(lp) - msg->travel_start_time;

		      total_hops -= msg->my_N_hop;
	         }
	   break;

	   case BUFFER:
	        {
		   int msg_indx = msg->vc_index;

		   s->vc_occupancy[msg_indx]++;

		   if(s->vc_occupancy[msg_indx] == VC_BUF_SIZE)
         	     {
			s->output_vc_state[msg_indx] = VC_CREDIT;
	             }

		}
   }
}

void router_rc_event_handler(router_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
  switch(msg->type)
    {
            case R_SEND:
		    {
		       tw_rand_reverse_unif(lp->rng);

			if(bf->c3 == 1)
			 {    
			   int input_chan = msg->input_chan;

			   int output_chan = msg->old_vc;

			   s->input_vc_state[input_chan] = VC_ALLOC;		    

			   s->output_vc_state[output_chan] = VC_IDLE;

			   s->next_output_available_time[output_chan] = msg->saved_available_time;

			   s->vc_occupancy[output_chan]--;

			   s->next_credit_available_time[output_chan] = msg->saved_credit_time;
			 }
		    }
	    break;

	    case R_ARRIVE:
	    	    {
		      if(bf->c3 == 1)
		       {
		        int input_chan = msg->input_chan;
		      
		        s->next_input_available_time[input_chan] = msg->saved_available_time;

		        s->input_vc_state[input_chan] = VC_IDLE;

			msg->my_N_hop--;
		       }
		      else
		       {
		        tw_rand_reverse_unif(lp->rng);
		       }
		    }
	    break;

	    case BUFFER:
	    	   {
		      int msg_indx = msg->vc_index;

		      s->vc_occupancy[msg_indx]++;

		      if(s->vc_occupancy[msg_indx] == VC_BUF_SIZE)
                        {
                          s->output_vc_state[msg_indx] = VC_CREDIT;
                        }
		   
		   }
	    break;
    }
}
////////////////////////////////////////////////////// MAIN ///////////////////////////////////////////////////////
////////////////////////////////////////////////////// LP TYPES /////////////////////////////////////////////////
tw_lptype dragonfly_lps[] =
{
   // Terminal handling functions
   {
    (init_f)terminal_init,
   
    (event_f) terminal_event,
   
    (revent_f) terminal_rc_event_handler,
   
    (final_f) final,
   
    (map_f) mapping,
   
    sizeof(terminal_state)
    },
   {
     (init_f) router_setup,
   
     (event_f) router_event,
   
     (revent_f) router_rc_event_handler,
   
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

tw_lp * dragonfly_mapping_to_lp(tw_lpid lpid)
{
  int index;

  if(lpid < total_routers)
   index = lpid - g_tw_mynode * nlp_router_per_pe - get_router_rem();
  else
   index = nlp_router_per_pe + (lpid - g_tw_mynode * nlp_terminal_per_pe - get_terminal_rem() - total_routers);

 // printf("\n Inverse mapping index %d for LP %d g_tw_mynode %d ", index, (int)lpid, g_tw_mynode); 
  return g_tw_lp[index];
}

void dragonfly_mapping(void)
{
  tw_lpid kpid;

  tw_pe * pe;

  int nkp_per_pe=16;

  for(kpid = 0; kpid < nkp_per_pe; kpid++)
    tw_kp_onpe(kpid, g_tw_pe[0]);

  int i;

  //printf("\n Node %d router start %d Terminal start %d ", g_tw_mynode, g_tw_mynode * nlp_router_per_pe + get_router_rem(), total_routers + g_tw_mynode * nlp_terminal_per_pe + get_terminal_rem());
  for(i = 0; i < nlp_router_per_pe; i++)
   {
     kpid = i % g_tw_nkp;

     pe = tw_getpe(kpid % g_tw_npe);

     tw_lp_onpe(i, pe, g_tw_mynode * nlp_router_per_pe + i + get_router_rem());

     tw_lp_onkp(g_tw_lp[i], g_tw_kp[kpid]);

    tw_lp_settype(i, &dragonfly_lps[1]);

#ifdef DEBUG
//    printf("\n [Node %d] Local Router ID %d Global Router ID %d ", g_tw_mynode, i, g_tw_mynode * nlp_router_per_pe + i + get_router_rem());
#endif
   } 

  for(i = 0; i < nlp_terminal_per_pe; i++)
   {
      kpid = i % g_tw_nkp;

      pe = tw_getpe(kpid % g_tw_npe);

      tw_lp_onpe(nlp_router_per_pe + i, pe, total_routers + g_tw_mynode * nlp_terminal_per_pe + i + get_terminal_rem());

      tw_lp_onkp(g_tw_lp[nlp_router_per_pe + i], g_tw_kp[kpid]);

      tw_lp_settype(nlp_router_per_pe + i, &dragonfly_lps[0]);

#ifdef DEBUG
//    printf("\n [Node %d] Local Terminal ID %d Global Terminal ID %d ", g_tw_mynode, nlp_router_per_pe + i, total_routers + g_tw_mynode * nlp_terminal_per_pe + i + get_terminal_rem());
#endif
    }
}

/////////////////////////////////////////////////////// REVERSE COMPUTATION ///////////////////////////////////////
int main(int argc, char **argv)
{
     //char log[32];
     tw_opt_add(app_opt);
   
     tw_init(&argc, &argv);

     MEAN_INTERVAL = 10;

     total_routers=NUM_ROUTER*num_groups;

     total_terminals=NUM_ROUTER*NUM_TERMINALS*num_groups;

     nlp_terminal_per_pe = total_terminals/tw_nnodes()/g_tw_npe;

     terminal_rem = total_terminals % (tw_nnodes()/g_tw_npe);

     if(g_tw_mynode < terminal_rem)
       nlp_terminal_per_pe++;

     nlp_router_per_pe = total_routers/tw_nnodes()/g_tw_npe;

      router_rem = total_routers % (tw_nnodes()/g_tw_npe);

      if(g_tw_mynode < router_rem)
        nlp_router_per_pe++;

      range_start=nlp_router_per_pe + nlp_terminal_per_pe;

    g_tw_mapping=CUSTOM;
    
    g_tw_custom_initial_mapping=&dragonfly_mapping;
    
    g_tw_custom_lp_global_to_local_map=&dragonfly_mapping_to_lp;
  
     g_tw_events_per_pe = (nlp_terminal_per_pe/g_tw_npe) * (g_tw_ts_end/MEAN_INTERVAL);

     tw_define_lps(range_start, sizeof(terminal_message), 0);

   
#if DEBUG
     //sprintf( log, "dragonfly-log.%d", g_tw_mynode );
     //dragonfly_event_log=fopen(log, "w+");

     //if(dragonfly_event_log == NULL)
	//tw_error(TW_LOC, "\n Failed to open dragonfly event log file \n");
#endif


#if DEBUG
     if(tw_ismaster())
	{
          printf("\n total_routers %d total_terminals %d g_tw_nlp is %d g_tw_npe %d g_tw_mynode: %d \n ", total_routers, total_terminals, (int)g_tw_nlp, (int)g_tw_npe, (int)g_tw_mynode);

	  printf("\n Arrival rate %f g_tw_mynode %d total %d nlp_terminal_per_pe is %d, nlp_router_per_pe is %d \n ", MEAN_INTERVAL, (int)g_tw_mynode, range_start, nlp_terminal_per_pe, nlp_router_per_pe);
	}
#endif
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

   int i;

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
   tw_end();

   return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
