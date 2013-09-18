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
tw_lpid getRouterID(tw_lpid local_router_id)
{
  int node_id=mapping(local_router_id);
  
  int router_id=local_router_id-(node_id * nlp_terminal_per_pe);
  
  return router_id;
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

#if DEBUG
   //printf("\n Router offset %d Group begin %d ", offset/half_channel);
#endif 
  return router_id;
}	

/////////////////////////////////// Packet generate, receive functions ////////////////////////////////////////////
void packet_generate(terminal_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
  tw_lpid dst_lp;
  tw_stime ts;
  tw_event *e;
  terminal_message *m;

  // Send the packet out
  e = tw_event_new(lp->gid, 0, lp);
  
  m = tw_event_data(e);
  
  m->type = SEND;
  m->transmission_time = PACKET_SIZE;

  // Set up random destination
  dst_lp = tw_rand_integer(lp->rng,0, total_routers+total_terminals-1);

  // If the destination address is of a router, select another destination LP
  // Make sure the message is not being sent to itself
  int isRouter=check_router_lpid(dst_lp);

  while((dst_lp==lp->gid) || (isRouter==1))
   {
    dst_lp = tw_rand_integer(lp->rng,0, total_routers+total_terminals-1);
  
    isRouter=check_router_lpid(dst_lp);
   }
  
  // record start time
  m->travel_start_time = tw_now(lp);
  
  m->my_N_queue = 0;
  
  m->my_N_hop = 0;
  
  m->queueing_times = 0;

  // set up packet ID
  // each packet has a unique ID
  m->packet_ID = lp->gid + total_terminals*s->packet_counter;
  
  m->dest_terminal_id=dst_lp;
  
  tw_event_send(e);

#if DEBUG
  //fprintf(dragonfly_event_log, "\n My ID is %lu", m->packet_ID);
  if(m->packet_ID == TRACK)
    fprintf(dragonfly_event_log, "\n I am generated at time %lu Terminal %d ", m->travel_start_time, lp->gid);
#endif

  // One more packet is generating 
  s->packet_counter++;
  
  int index = floor(N_COLLECT_POINTS*(tw_now(lp)/g_tw_ts_end));
  N_generated_storage[index]++;

  // schedule next GENERATE event
  ts = tw_rand_exponential(lp->rng, MEAN_INTERVAL);
  
  e = tw_event_new(lp->gid, ts, lp);
  
  m = tw_event_data(e);
  
  m->type = GENERATE;
  m->dest_terminal_id = dst_lp;

  tw_event_send(e);
}

void packet_send(terminal_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
#if DEBUG
if( msg->packet_ID == TRACK )
  {
    printf("\n I am being sent from terminal %d to terminal %d source router %d \n", (int)lp->gid, msg->dest_terminal_id, getRouterID(s->router_id));
  }
#endif

  tw_stime ts;
  tw_event *e;
  terminal_message *m;

  /* Route the packet to the source router */ 
  msg->src_terminal_id=(int)lp->gid;

  ts = ROUTER_DELAY+PACKET_SIZE;   

  msg->saved_available_time=s->next_available_time;

  s->next_available_time = max(s->next_available_time, tw_now(lp));
  s->next_available_time += ts;

  e = tw_event_new(s->router_id, s->next_available_time-tw_now(lp), lp);
  m = tw_event_data(e);
  m->type = SEND;

  // Carry on the message info
  m->dest_terminal_id = msg->dest_terminal_id;

  m->transmission_time = msg->transmission_time;
  
  m->src_terminal_id = msg->src_terminal_id;
  
  m->packet_ID = msg->packet_ID;
  
  m->travel_start_time = msg->travel_start_time;
  
  m->my_N_hop = msg->my_N_hop;
  
  m->my_N_queue = msg->my_N_queue;
  
  m->queueing_times = msg->queueing_times;

  tw_event_send(e);
}

void packet_arrive(terminal_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
#if DEBUG
  if( msg->packet_ID == TRACK )
    {
	printf( "packet %lld has arrived at %d \n",
              msg->packet_ID, lp->gid);

	printf("lp time is %f travel start time is %f\n",
             tw_now(lp),
             msg->travel_start_time);

	printf("My hop now is %d\n",msg->my_N_hop);
    }
// fprintf(dragonfly_event_log, "\n Arrived at %lf ",tw_now(lp));
#endif
  tw_stime ts;
  tw_event *e;
  terminal_message *m;

  // Packet arrives and accumulate # queued
  msg->queueing_times += s->N_wait_to_be_processed;

  msg->saved_available_time = s->next_available_time;
  s->next_available_time = max(s->next_available_time, tw_now(lp));

  ts = MEAN_PROCESS;

  s->N_wait_to_be_processed++;

  e = tw_event_new(lp->gid, s->next_available_time - tw_now(lp), lp);

  s->next_available_time += ts;

  m = tw_event_data(e);
  m->type = PROCESS;
  m->dest_terminal_id = msg->dest_terminal_id;
  
  m->transmission_time = msg->transmission_time;
  m->packet_ID = msg->packet_ID;
  m->travel_start_time = msg->travel_start_time;

  m->my_N_hop = msg->my_N_hop;

  m->my_N_queue = msg->my_N_queue;

  m->queueing_times = msg->queueing_times;

  m->src_terminal_id = msg->src_terminal_id;

  tw_event_send(e);
}

void packet_process(terminal_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
#if DEBUG
if(msg->packet_ID == TRACK)
  fprintf(dragonfly_event_log, "\n Processing at LP %d from terminal %d at %lf \n", (int)lp->gid, msg->src_terminal_id, tw_now(lp));
#endif
  
  bf->c3 = 1;
  s->N_wait_to_be_processed--;

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
      total_queue_length += msg->my_N_queue;
  
      queueing_times_sum += msg->queueing_times;
    }
  else
    {
  // if(msg->packet_ID == TRACK)
     printf("\n Packet %d LP: %d Not arrived at correct destination: %d ", msg->packet_ID, (int)lp->gid, msg->dest_terminal_id);
    }
}
////////////////////////////////////////////////// Terminal related functions ///////////////////////////////////////

void terminal_setup(terminal_state * s, tw_lp * lp)
{
    s->terminal_id=((int)lp->gid);  
 
    // Assign the global router ID
    s->router_id=getRouterLPID_Terminal(lp->gid);
 
    s->packet_counter = 0;
    
    s->N_wait_to_be_processed = 0;
    
    s->next_available_time = 0;
    s->saved_available_time=0;
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
   
    m->type = GENERATE;
    tw_event_send(e);
}

void terminal_event(terminal_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
  switch(msg->type)
    {
    case GENERATE:
      packet_generate(s,bf,msg,lp);
      break;
    
    case ARRIVAL:
      packet_arrive(s,bf,msg,lp);
      break;
    
    case SEND:
      packet_send(s,bf,msg,lp);
      break;
    
    case PROCESS:
      packet_process(s,bf,msg,lp);
      break;
    }
}

void terminal_rc_event(terminal_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
 
  int index = floor(N_COLLECT_POINTS*(tw_now(lp)/g_tw_ts_end));
  
  switch(msg->type)
    {
  
      case GENERATE:
	           N_generated_storage[index]--;
	           s->packet_counter--;
	           tw_rand_reverse_unif(lp->rng);
	           tw_rand_reverse_unif(lp->rng);
      break;

     case ARRIVAL:
                   s->next_available_time = msg->saved_available_time;
	           msg->my_N_hop--;
	           s->N_wait_to_be_processed--;
	           msg->queueing_times -= s->N_wait_to_be_processed;
     break;
    
     case SEND:
	          s->next_available_time = msg->saved_available_time;
     break;

    case PROCESS:
     
    		if ( bf->c3 == 0 )
	        {
        	  N_finished--;
	          N_finished_storage[index]--;
         
		  total_time -= tw_now(lp) - msg->travel_start_time;
	          total_hops -= msg->my_N_hop;
         
		  total_queue_length -= msg->my_N_queue;
	          queueing_times_sum -= msg->queueing_times;
	        }
	      s->N_wait_to_be_processed++;
    break;
    } 
}

void final(terminal_state * s, tw_lp * lp)
{

}

/////////////////////////////////////////// Router packet send/receive functions //////////////////////

void router_packet_send(router_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
  tw_stime ts;
  tw_event *e;
  terminal_message *m;

  int dest_router_id=getRouterLPID_Terminal(msg->dest_terminal_id);

  int dest_group_id=getRouterIDFromTerminal(msg->dest_terminal_id)/NUM_ROUTER;

  // Check if the destination router is in the same group
  if(dest_group_id != s->group_id)
     dest_router_id=getRouterLPID(getRouterFromGroupID(dest_group_id,s));

#if DEBUG
  if( msg->packet_ID == TRACK )
  {
   printf("\n I am being sent from source router %d to intermediate group router %d \n", getRouterID((int)lp->gid), getRouterFromGroupID(dest_group_id,s));
  }
#endif

  // Packet arrives and accumulate # queued
  msg->queueing_times += s->N_wait_to_be_processed;

  msg->my_N_hop++;

  msg->saved_router_available_time = s->next_router_available_time;

  s->next_router_available_time = max(s->next_router_available_time, tw_now(lp));
  ts = tw_rand_exponential(lp->rng, (double)ROUTER_DELAY/1000)+ROUTER_DELAY+PACKET_SIZE;
  s->next_router_available_time += ts;

  s->N_wait_to_be_processed++;

  e = tw_event_new(dest_router_id, s->next_router_available_time - tw_now(lp), lp);

  m = tw_event_data(e);
  m->type = ARRIVAL;

  // Carry on the message information
  m->dest_terminal_id = msg->dest_terminal_id;
  m->transmission_time = msg->transmission_time;

  m->packet_ID = msg->packet_ID;

  m->travel_start_time = msg->travel_start_time;

  m->src_terminal_id = msg->src_terminal_id;

  m->my_N_hop = msg->my_N_hop;
  m->my_N_queue = msg->my_N_queue;

  m->queueing_times = msg->queueing_times;

  tw_event_send(e);
}

void router_packet_receive(router_state * s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
  tw_stime ts;

  tw_event *e;

  terminal_message *m;

  int dest_router_id=getRouterLPID_Terminal(msg->dest_terminal_id); 

  msg->queueing_times += s->N_wait_to_be_processed;

  msg->saved_router_available_time = s->next_router_available_time;
  msg->my_N_hop++;

  s->next_router_available_time = max(s->next_router_available_time, tw_now(lp));
  ts = tw_rand_exponential(lp->rng, LINK_DELAY) + LINK_DELAY+PACKET_SIZE;;

  s->next_router_available_time += ts;
  s->N_wait_to_be_processed++;
 
  if(lp->gid == dest_router_id)
   {
#if DEBUG
  if( msg->packet_ID == TRACK )
   {
      printf("\n Packet %d At final destination: I have arrived at destination router %d %d \n", msg->packet_ID, getRouterID((int)lp->gid), getRouterID(dest_router_id));
   }
#endif
     // Packet arrives and accumulate in the queue
     e = tw_event_new(msg->dest_terminal_id, s->next_router_available_time - tw_now(lp), lp);
     s->next_router_available_time += ts;

     m = tw_event_data(e);
   }
   else
    {
     int i, isThere=0;

     int dest_group_id=getRouterID(dest_router_id)/NUM_ROUTER;

     if(dest_group_id != s->group_id)
     {
      for(i=0; i<GLOBAL_CHANNELS; i++)
       {
         if(s->global_channel[i]/NUM_ROUTER == dest_group_id)
         {
	   dest_router_id=getRouterLPID(s->global_channel[i]);
	   isThere=1;
	 }
       }
     }
      else
	   isThere=1;

#if DEBUG
     if(!isThere)
	     printf("\n NO CONNECTION TO GROUP ");
#endif

#if DEBUG
    if( msg->packet_ID == TRACK )
    {
      printf("\n Packet %d At intermediate router %d to destination router %d Final destination %d \n", msg->packet_ID, getRouterID((int)lp->gid), getRouterID(dest_router_id), getRouterLPID_Terminal(msg->dest_terminal_id));
    }
#endif
     // Packet arrives and accumulate in the queue
        e = tw_event_new(dest_router_id, s->next_router_available_time - tw_now(lp), lp);

	s->next_router_available_time += ts;

	m = tw_event_data(e);
    }
     // Carry on the message information
     m->type=ARRIVAL;
  
     m->dest_terminal_id = msg->dest_terminal_id;
     m->src_terminal_id = msg->src_terminal_id;
  
     m->transmission_time = msg->transmission_time;
  
     m->packet_ID = msg->packet_ID;
  
     m->travel_start_time = msg->travel_start_time;
  
     m->my_N_hop = msg->my_N_hop;
     m->my_N_queue = msg->my_N_queue;
     m->queueing_times = msg->queueing_times;
  
     tw_event_send(e);
}


/////////////////////////////////////////// Router related functions /////////////////////////////////
void router_setup(router_state * r, tw_lp * lp)
{
   r->router_id=((int)lp->gid);
   
   int local_router_id=getRouterID(lp->gid);

#if DEBUG
   //fprintf(dragonfly_event_log, "\n ROUTER TERMINAL INIT SEED %d ", (int)lp->gid);
#endif
   
   r->next_router_available_time=0;
   
   r->num_routed_packets=0;
   
   r->N_wait_to_be_processed=0;
   
   r->saved_router_available_time=0;
   
   r->group_id=getRouterID(lp->gid)/NUM_ROUTER;
   
   int i;
   int offset=(local_router_id%NUM_ROUTER) * (GLOBAL_CHANNELS/2) +1;
   
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
	   case SEND: // Router has received a packet from a terminal, it will send it to the destination router.
 		   router_packet_send(s, bf, msg, lp);
           break;

	   case ARRIVAL: // Router has received a packet addressed to it, it will send it to the destination terminal
	          router_packet_receive(s, bf, msg, lp);
	   break;

	   default:
		  printf("\n LP: %d Message type not supported ", (int)lp->gid);
	   break;
   }	   
}

void router_rc_event(router_state* s, tw_bf * bf, terminal_message * msg, tw_lp * lp)
{
  tw_rand_reverse_unif(lp->rng);   

  s->next_router_available_time = msg->saved_router_available_time;

  msg->my_N_hop--;

  s->N_wait_to_be_processed--;

  msg->queueing_times -= s->N_wait_to_be_processed;
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
   TWOPT_STIME("arrive_rate", ARRIVAL_RATE, "packet arrive rate"),
   TWOPT_END()
};

////////////////////////////////////////////////////// MAIN ///////////////////////////////////////////////////////
int main(int argc, char **argv, char **env)
{
     char log[32];
     tw_opt_add(app_opt);
   
     tw_init(&argc, &argv);
	 
     MEAN_INTERVAL = 100000.0;
   
     total_routers=NUM_ROUTER*num_groups;
   
     total_terminals=NUM_ROUTER*NUM_TERMINALS*num_groups;

     nlp_terminal_per_pe = total_terminals/tw_nnodes()/g_tw_npe;
   
     nlp_router_per_pe = total_routers/tw_nnodes()/g_tw_npe;
   
     g_tw_events_per_pe = nlp_terminal_per_pe/g_tw_npe * nlp_router_per_pe/g_tw_npe + opt_mem;

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
  
    unsigned long long wait_length,event_length,N_total_finish,N_total_hop;

   tw_stime total_time_sum,g_max_latency;

   for( i=0; i<N_COLLECT_POINTS; i++ )
    {
     MPI_Reduce( &N_finished_storage[i], &total_finished_storage[i],1,
                 MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
   
     MPI_Reduce( &N_generated_storage[i], &total_generated_storage[i],1,
                  MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
     }
        MPI_Reduce( &queueing_times_sum, &event_length,1,
                    MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
   
   	MPI_Reduce( &total_queue_length, &wait_length,1,
                    MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
   
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
    
    	    printf("\n total wait length:    %lf; \n",
                   (double)wait_length/total_finished_storage[N_COLLECT_POINTS-1]);
    
    	    printf("\n total queued:   %lf; \n",
                   (double)event_length/total_finished_storage[N_COLLECT_POINTS-1]);
    
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
