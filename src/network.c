// Networking uses client/server with client-side prediction.
//
// Client involves "clever" algorithm I first read about
// in description of Unreal's player physics and which you
// can find described in a million places on the net:
//
//   - Client simulates effect of moving player through world
//     in response to player input, renders player as being
//     there, but also sends input to server *and* records it
//     for later; server sends back *authorative* client position
//     and client replays user input (resimulating player physics)
//     from last authorative position to recover new ("predicted")
//     position for player.
//
// There is also clever stuff to do to handle the client player
// shooting at another player ("lag compenstation").
//
// What nobody talks much about is how the server-side is
// dealing with this, especially since there's variable
// latency from the client (e.g. even with stable latency,
// if a packet from client is dropped, then user input from
// that packet is delayed).
//
// So here are four possible approaches to server-side "physics"
// for the above scenario (doing client-side prediction and
// receiving only player-input from clients).
//
//   - 1. buffer client input for some length of time, then
//        execute a global tick for entire system at once
//
//   - 2. simulate globally-timed state; late client input rewinds
//        time, simulates that client forward (against *cached*
//        states), emits state to client as it goes forward,
//        avoids changing anything else so all other clients
//        only need updating about this client's state
//
//   - 3. simulate globally-timed state; late client input rewinds
//        time, simulates all clients forward again, may need
//        to update already-sent state with newer, more correct
//        values
//
//   - 4. simulate opportunistically as soon as you get client
//        input; all physics state is unchanging while client
//        is simulating
//
// "late client input" is just for expository purposes; effectively,
// all client input is late by a variable amount (varying depending
// on lag/dropped packets).
//
// We can describe the above in terms of a scenario where player
// A & B try to move to the same location (even though physics is
// supposed to prevent the players from being in the same location)
// and according to their relative clocks player A gets there first,
// but due to variable latency the server receives the packet for
// player B first. (Note I'm not talking in detail about what 'clocks'
// means here, I just need a characterization like that to make sense
// of the some of the above scenarios):
//
//     1. player A ends up in the place and player B doesn't (as long as the
//        buffering is sufficient to cover the late packet from A)
//
//     2. player A and player B end up in the same place
//
//     3. player A ends up in the place, and player B doesn't, but a
//        packet might go out first that claims player B got there
//
//     4. player B ends up in the place ("first come first served")
//
// Note that in case 1, if a player input packet is delayed by more
// than the client buffering time, that player input is *dropped
// entirely*, which is going to cause the player to jump around from
// their point of view. Since network dropouts can be bursty, this
// means you could easily lose a bunch of input, say a second's worth,
// which could mean a pretty big jump. However, the alternative (e.g. #4)
// would result in all the players seeing A's 1 second of activity all
// at once in a burst, so we're going to guess that it's better for A
// to suffer A's net drop, instead of everyone else, so we're going to
// implement method 1.

/*
 Stuff marked + or * must _all_ be implemented before the new
 system will even feebly work. Marked with + means it can be dummied.

   server main loop at 60hz:
 *    1. accumulate timestamped inputs from clients over network
 *    2. simulate 1 tick using corresponding input from client  (how to determine correspondence?)
 *    3. for each client:
      3a.   check accuracy of client-predicted position
      3b.      add highest-priority client state to non-reliable list
      4.    put relevent voxel changes in reliable list
               - also map downloading as player moves around
 *    5.    for each entity:
 +    6.       compute min update time (1 over max update rate)
      7.       if time-since-last is higher than min update time
      8.          compute priority based on:
                     - deviation from dead-reckoning (requires knowing last time client saw state)
                     - time-since-last
                     - distance
                     - view direction (minimal contribution since player can instantly change view direction)
                        - if player is zoomed in, need to demphasize distance if in view direction
                     - relevance (e.g. player-is-attached-to)
                     - importance (absolute player-independent)
      9.          if priority is sufficiently high, or we are on every 3rd packet,
 *   10.             add state to pending non-reliable list w/priority
     11.       add reliable-state-changes for this entity to reliable list
     12.    sort non-reliable list
 *   13.    build UDP packets containing:
     14.       as much reliable state as reliable-stream-layer requires
 *   15.       as much non-reliable data as will fit in rest of packet
     16.       packet-size determined by unused bandwidth relative to max consumed bandwidth
 *   17.    send packet(s)--may be 0 if nothing is updating
   // intent of above algorithms:
   //    - reliable data takes precedence over non-reliable data as needed
   //        - (especially as it gets resent, because it gets staler)
   //    - if no high-frequency data needs sending, packets are quantized to 20hz
   //        (60hz updating is hopefully totally unnecessary, but this allows
   //         for the possibility, which is the kind of flexibility OBBG is intended to have)

   client main loop:
 *    0. for as many 60hz ticks since last frame:
 *    1.   collect player input
 +    2.   collect server updates into 100ms buffer
 *    3.   store player input into network output buffer
 +    4.   if odd tick, send network output buffer
      5.   store player input into replay buffer
      6.   if player-state correction packet:
      7.     rewind to time of correction, replay player input from time
 *    8.   interpolate positions from buffered server states
      9.   extrapolate positions/state where dead reckoning
 *   10.   simulate player physics
     10. render


   single-player main loop:
     0. for as many 60hz ticks since last frame
     1.   collect player input
     2.   simulate 1 tick
     3. render
*/



#include "obbg_funcs.h"
#include <math.h>

#include <SDL_net.h>

static UDPsocket receive_socket;
static UDPpacket *receive_packet;
static UDPpacket *send_packet;

#define MAX_PACKET_SIZE  512


typedef struct
{
   address addr;
   objid pid;
} connection_t;

Bool net_send(void *buffer, size_t buffer_size, address *addr)
{
   int res;

   assert(buffer_size <= (size_t) send_packet->maxlen);
   if (buffer_size > (size_t) send_packet->maxlen)
      return False;

   SDLNet_Write32(addr->host, &send_packet->address.host);
   SDLNet_Write16(addr->port, &send_packet->address.port);

   memcpy(send_packet->data, buffer, buffer_size);
   send_packet->len = buffer_size;

   res = SDLNet_UDP_Send(receive_socket, -1, send_packet);

   return True;
}

int net_receive(void *buffer, size_t buffer_size, address *addr)
{
   int result = SDLNet_UDP_Recv(receive_socket, receive_packet);
   if (result == 0) {
      return -1;
   }
   if (result == -1) {
      // @TODO ??? don't know what the error case is
      return -1;
   }
   if (result == 1) {
      addr->host = SDLNet_Read32(&receive_packet->address.host);
      addr->port = SDLNet_Read16(&receive_packet->address.port);
      memcpy(buffer, receive_packet->data, stb_min(buffer_size, (size_t) receive_packet->len));
      return receive_packet->len;
   }
   return -1;
}

address server_address;

Bool net_init(Bool server, int server_port)
{
   if (SDLNet_Init() != 0)
      return False;

   receive_socket = SDLNet_UDP_Open(server ? server_port : 0);
   receive_packet = SDLNet_AllocPacket(MAX_PACKET_SIZE);
   send_packet    = SDLNet_AllocPacket(MAX_PACKET_SIZE);

   server_address.host = (127<<24) + 1;
   server_address.port = server_port;
   return True;
}

#define MAX_CONNECTIONS 32

connection_t connection[MAX_CONNECTIONS];
int connection_for_pid[PLAYER_OBJECT_MAX];

int create_connection(address *addr, objid pid)
{
   int i;
   for (i=0; i < MAX_CONNECTIONS; ++i)
      if (connection[i].addr.port == 0) {
         connection[i].pid = pid;
         connection[i].addr = *addr;
         connection_for_pid[connection[i].pid] = i;
         return i;
      }
   return -1;
}

int find_connection(address *addr)
{
   int i;
   for (i=0; i < MAX_CONNECTIONS; ++i)
      if (connection[i].addr.host == addr->host && connection[i].addr.port == addr->port)
         return i;
   return -1;
}

#define NUM_INPUTS_PER_PACKET     6  // 100 ms buffer with 30hz packets = each 60hz input in 3 packets

#define MAX_SERVER_STATE_HISTORY       8
#define MAX_CLIENT_INPUT_HISTORY      12
#define MAX_CLIENT_FRAME_NUMBER_LOG2   7 
#define MAX_CLIENT_FRAME_NUMBER       (1 << MAX_CLIENT_FRAME_NUMBER_LOG2)

#define CLIENT_FRAME_NUMBER_MASK      (MAX_CLIENT_FRAME_NUMBER-1)
#define CLIENT_FRAME_HALFWAY_POINT    (MAX_CLIENT_FRAME_NUMBER/2)

// buffer is stored so input[0] is always at server_timestamp
typedef struct
{
   player_net_controls input[MAX_CLIENT_INPUT_HISTORY];
   Bool valid[MAX_CLIENT_INPUT_HISTORY];
   uint64 client_input_offset;
   int saw_frame_past_halfway;
} player_input_history;

player_input_history phistory[PLAYER_OBJECT_MAX];

typedef struct
{
   uint8 sequence;
   uint8 frame;
   player_net_controls last_inputs[NUM_INPUTS_PER_PACKET];
} net_client_input;

typedef struct
{
   uint8 sequence;
} player_output_history;

player_output_history outhistory[PLAYER_OBJECT_MAX];

typedef struct
{
   object (*obj)[MAX_SERVER_STATE_HISTORY];
   uint64 timestamp[MAX_SERVER_STATE_HISTORY];
} versioned_object_system;

float angle_from_network(uint16 iang)
{
   return iang / 65536.0f * 360;
}

uint16 angle_to_network(float ang)
{
   int iang = (int) (ang/360 * 65536);
   if (iang >= 65536) iang = 65535;
   return iang;
}

uint64 server_timestamp = 1U<<24;

//#define NO_PLAYER_INPUT_BUFFERING

//      p_input[connection[n].pid].buttons = input.last_inputs[0].buttons;
//      obj[connection[n].pid].ang.x = angle_from_network(input.last_inputs[0].view_x) - 90;
//      obj[connection[n].pid].ang.z = angle_from_network(input.last_inputs[0].view_z);
void process_player_input(objid pid, net_client_input *input)
{
   #ifdef NO_PLAYER_INPUT_BUFFERING
   phistory[pid].input[0] = input->last_inputs[0];
   phistory[pid].valid[0] = True;
   #else
   
   int i;
   uint64 input_timestamp;

   if (input->frame < CLIENT_FRAME_HALFWAY_POINT && phistory[pid].saw_frame_past_halfway) {
      player_input_history *p = &phistory[pid];
      p->client_input_offset += MAX_CLIENT_FRAME_NUMBER;

      // if the input frame is no longer in the valid window, recompute an client offset clock;
      // @TODO: avoid doing this too frequently
      if (p->client_input_offset + input->frame < server_timestamp || p->client_input_offset + input->frame >= server_timestamp + MAX_CLIENT_INPUT_HISTORY) {
         p->client_input_offset = server_timestamp + MAX_CLIENT_INPUT_HISTORY/2 - input->frame;
      }

      phistory[pid].saw_frame_past_halfway = False;
   }

   if (input->frame > CLIENT_FRAME_HALFWAY_POINT)
      phistory[pid].saw_frame_past_halfway = True;

   input_timestamp = phistory[pid].client_input_offset + input->frame;
   for (i=0; i < NUM_INPUTS_PER_PACKET; ++i) {
      if (input_timestamp >= server_timestamp && input_timestamp < server_timestamp + MAX_CLIENT_INPUT_HISTORY) {
         phistory[pid].input[input_timestamp - server_timestamp] = input->last_inputs[i];
         phistory[pid].valid[input_timestamp - server_timestamp] = True;
      }
      --input_timestamp;
   }
   #endif
}

objid create_player(void)
{
   objid pid = allocate_player();
   if (pid == 0)
      return -1;

   obj[pid].position.x = (float) (rand() & 31);
   obj[pid].position.y = (float) (rand() & 31);
   obj[pid].position.z = 200;

   obj[pid].ang.x = 0;
   obj[pid].ang.y = 0;
   obj[pid].ang.z = 0;

   obj[pid].velocity.x = 0;
   obj[pid].velocity.y = 0;
   obj[pid].velocity.z = 0;

   return pid;
}

// 1. consumes all packets from players
// 2. consumes 1 input from each player input queue
void server_net_tick_pre_physics(void)
{
   int i;
   net_client_input input;

   address addr;
   while (net_receive(&input, sizeof(input), &addr) >= 0) {
      int n = find_connection(&addr);
      if (n < 0) {
         objid pid = create_player();
         n = create_connection(&addr, pid);
         if (n < 0)
            continue;
      }

      process_player_input(connection[n].pid, &input);
   }

   for (i=1; i < max_player_id; ++i) {
      if (obj[i].valid) {
         obj[i].ang.x = angle_from_network(phistory[i].input[0].view_x) - 90;
         obj[i].ang.z = angle_from_network(phistory[i].input[0].view_z);
         p_input[i].buttons = phistory[i].input[0].buttons;

         ods("buttons: %04x\n", p_input[i].buttons);

         #ifndef NO_PLAYER_INPUT_BUFFERING         
         memmove(&phistory[i].input[0], &phistory[i].input[1], sizeof(phistory[i].input[0]) * (MAX_CLIENT_INPUT_HISTORY-1));
         memmove(&phistory[i].valid[0], &phistory[i].valid[1], sizeof(phistory[i].valid[0]) * (MAX_CLIENT_INPUT_HISTORY-1));
         phistory[i].input[MAX_CLIENT_INPUT_HISTORY-1].buttons = 0;
         phistory[i].valid[MAX_CLIENT_INPUT_HISTORY-1] = False;
         #endif
      }
   }
}

typedef struct
{
   uint16 objid;
} net_objid;

typedef struct
{
   uint8 seq;
   uint8 timestamp;
} packet_header;

typedef struct
{
   uint8 type;
   uint8 count;
} record_header;

enum
{
   RECORD_TYPE_invalid,
   RECORD_TYPE_player_objid,
   RECORD_TYPE_object_state,
   RECORD_TYPE_connection_info
};

typedef struct
{
   uint8 buffered_packets;
} connection_info;

typedef struct
{
   float p[3],v[3],o[2];
} object_state;

void write_to_packet(char *buffer, int *off, void *data, int data_size, int limit)
{
   if (*off + data_size <= limit) {
      memcpy(buffer + *off, data, data_size);
   }
   *off += data_size;
}

int count_buffered_packets_for_player(int pid)
{
   int i, n = 0;
   for (i=0; i < MAX_CLIENT_INPUT_HISTORY; ++i)
      n += phistory[pid].valid[i];
   return n;
}

/*
 +    6.       compute min update time (1 over max update rate)
 *   10.             add state to pending non-reliable list w/priority
*/
int build_packet_for_player(objid pid, char *buffer, int buffer_size)
{
   int i;
   Bool empty = True;
   int offset=0, save_offset, object_state_count=0;
   packet_header ph;
   record_header rh;
   net_objid no;
   connection_info ci;

   ph.seq = outhistory[pid].sequence;
   ph.timestamp = (uint8) (server_timestamp & 0xff);
   write_to_packet(buffer, &offset, &ph, sizeof(ph), buffer_size);

   rh.type = RECORD_TYPE_player_objid;
   rh.count = 1;
   no.objid = pid;
   write_to_packet(buffer, &offset, &rh, sizeof(rh), buffer_size);
   write_to_packet(buffer, &offset, &no, sizeof(no), buffer_size);
   empty = False;

   rh.type = RECORD_TYPE_connection_info;
   rh.count = 1;
   ci.buffered_packets = count_buffered_packets_for_player(pid);
   write_to_packet(buffer, &offset, &rh, sizeof(rh), buffer_size);
   write_to_packet(buffer, &offset, &ci, sizeof(ci), buffer_size);

   save_offset = offset;
   rh.type = RECORD_TYPE_object_state;
   rh.count = 0;
   write_to_packet(buffer, &offset, &rh, sizeof(rh), buffer_size);

   // send player states
   for (i=1; i < max_player_id; ++i) {
      if (obj[i].valid) {
         object_state os;
         if (offset + (int) sizeof(os) + (int) sizeof(no) <= buffer_size) {
            no.objid = i;
            write_to_packet(buffer, &offset, &no, sizeof(no), buffer_size);
            os.p[0] = obj[i].position.x;
            os.p[1] = obj[i].position.y;
            os.p[2] = obj[i].position.z;
            os.v[0] = obj[i].velocity.x;
            os.v[1] = obj[i].velocity.y;
            os.v[2] = obj[i].velocity.z;
            os.o[0] = obj[i].ang.x;
            os.o[1] = obj[i].ang.z;
            write_to_packet(buffer, &offset, &os, sizeof(os), buffer_size);
            ++object_state_count;
         }
      }
   }

   // send non-player states
   for (i=PLAYER_OBJECT_MAX; i < max_obj_id; ++i) {
      if (obj[i].valid) {
         object_state os;
         if (offset + (int) sizeof(os) + (int) sizeof(no) <= buffer_size) {
            no.objid = i;
            write_to_packet(buffer, &offset, &no, sizeof(no), buffer_size);
            os.p[0] = obj[i].position.x;
            os.p[1] = obj[i].position.y;
            os.p[2] = obj[i].position.z;
            os.v[0] = obj[i].velocity.x;
            os.v[1] = obj[i].velocity.y;
            os.v[2] = obj[i].velocity.z;
            os.o[0] = obj[i].ang.x;
            os.o[1] = obj[i].ang.z;
            write_to_packet(buffer, &offset, &os, sizeof(os), buffer_size);
            ++object_state_count;
            empty = False;
         }
      }
   }

   assert(object_state_count < 256);
   rh.type = RECORD_TYPE_object_state;
   rh.count = (uint8) object_state_count;
   write_to_packet(buffer, &save_offset, &rh, sizeof(rh), buffer_size);

   if (empty) {
      return 0;
   } else {
      ++outhistory[pid].sequence;
      return offset;
   }
}

void server_net_tick_post_physics(void)
{
   int i;
   char packet_data[MAX_PACKET_SIZE];
   ++server_timestamp;
   for (i=1; i < max_player_id; ++i) {
      if (obj[i].valid) {
         int size;
         int conn = connection_for_pid[i];
         obj[0].valid = i;

         size = build_packet_for_player(i, packet_data, sizeof(packet_data));
         if (size != 0) 
            net_send(packet_data, size, &connection[conn].addr);
      }
   }
}

static net_client_input input;

/*
 *    0.  collect player input
 *    1.  send player input to server
 *    2.  receive server update packet
 *    3.  parse server update packet
 *
 *
 *    0. for as many 60hz ticks since last frame:
 *    1.   collect player input
 +    2.   collect server updates into 100ms buffer
 *    3.   store player input into network output buffer
 +    4.   if odd tick, send network output buffer
 *    8.   interpolate positions from buffered server states
 *   10.   simulate player physics
 */

player_net_controls client_build_input(objid player_id)
{
   player_net_controls pnc;

   vec ang = obj[player_id].ang;

   ang.x += 90;
   ang.x = (float) fmod(ang.x, 360);
   if (ang.x < 0) ang.x += 360;
   ang.z = (float) fmod(ang.z, 360);
   if (ang.z < 0) ang.z += 360;

   pnc.buttons = client_player_input.buttons;
   pnc.view_x = angle_to_network(ang.x); 
   pnc.view_z = angle_to_network(ang.z);

   ang.x -= 90;   
   obj[player_id].ang = ang;

   return pnc;
}

void client_net_tick(void)
{
   char packet[MAX_PACKET_SIZE];
   vec ang = { 0,0,0 };
   Bool override_ang = False;
   address receive_addr;
   int packet_size, pending_input=-1, num_object_updates=0;

   if (1 || player_id != 0) {
      ang = obj[player_id].ang;
      override_ang = True;

      input.frame += 1;
      input.frame = (uint8) (input.frame & CLIENT_FRAME_NUMBER_MASK);
      memmove(&input.last_inputs[1], &input.last_inputs[0], sizeof(input.last_inputs) - sizeof(input.last_inputs[0]));

      input.last_inputs[0] = client_build_input(player_id);

      if (input.frame & 1) {
         input.sequence += 1;
         net_send(&input, sizeof(input), &server_address);
      }
   }

   packet_size = net_receive(packet, sizeof(packet), &receive_addr);
   while (packet_size >= 0) {
      // @TODO: veryify receive_addr == server_address
      int offset=0;
      packet_header *ph = (packet_header *) (packet+offset);
      // process ph->seq
      // process ph->timestamp
      offset += sizeof(*ph);
      while(offset + (int) sizeof(record_header) < packet_size) {
         record_header *rh = (record_header *) (packet+offset);
         offset += sizeof(*rh);
         switch (rh->type) {
            case RECORD_TYPE_player_objid: {
               net_objid *no;
               if (offset + (int) sizeof(*no) > packet_size)
                  goto corrupt;
               no = (net_objid *) (packet+offset);
               offset += sizeof(*no);
               player_id = no->objid;
               break;
            }
            case RECORD_TYPE_connection_info: {
               connection_info *ci;
               if (offset + (int) sizeof(*ci) > packet_size)
                  goto corrupt;
               ci = (connection_info *) (packet+offset);
               offset += sizeof(*ci);
               pending_input = ci->buffered_packets;
               break;
            }
            case RECORD_TYPE_object_state: {
               int i;
               if (offset + (int) (sizeof(object_state)+sizeof(net_objid))*rh->count > packet_size)
                  goto corrupt;
               num_object_updates = rh->count;
               for (i=0; i < rh->count; ++i) {
                  objid o;
                  object_state *os;
                  net_objid *no = (net_objid *) (packet+offset);
                  offset += sizeof(*no);
                  os = (object_state *) (packet+offset);
                  offset += sizeof(*os);
                  o = no->objid;
                  if (o >= PLAYER_OBJECT_MAX) {
                     if (o >= max_obj_id)
                        max_obj_id = o+1;
                  } else {
                     if (o >= max_player_id)
                        max_player_id = o+1;
                  }
                  obj[o].position.x = os->p[0];
                  obj[o].position.y = os->p[1];
                  obj[o].position.z = os->p[2];
                  obj[o].velocity.x = os->v[0];
                  obj[o].velocity.y = os->v[1];
                  obj[o].velocity.z = os->v[2];
                  obj[o].ang.x = os->o[0];
                  obj[o].ang.z = os->o[1];
                  obj[o].valid = True;
               }
               break;
            }               
            default:
               goto corrupt;
         }
      }
     corrupt:
      ods("PID: %d; pending input %d; obj updates %d\n", player_id, pending_input, num_object_updates);
      packet_size = net_receive(packet, sizeof(packet), &receive_addr);
   }

   if (override_ang)
      obj[player_id].ang = ang;


}

//   1024 connections
// * 32768 objects
// * 64 bytes per object
// * 16 versions
// 2^(6+15+10+4) = 2^35  32 GB

// 64 bytes per object
//    8192 objects
//    1024 players
//       8 versions
//
// 2^(6+13+10+3) = 2^32 = 4GB


// * 1024 connections
// * 32768 objects not-lag compensated       1024 objects lag compensated
// * 64 bytes per object                       64 bytes per object
// *     1                                 *   16

//   1024 connections                       1024 connections
// * 1024 objects not-lag compensated       1024 objects lag compensated
// *   64 bytes per object                    16 bytes per object
// *    1                                      8 versions

// 2^10 * 2^10 * 2^4 * 2^3 = 2^27 => 128 MB





