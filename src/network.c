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

#if 0
typedef struct
{
   uint8 seq;
} packet_header;

typedef struct
{
   uint8 type;
   uint8 count;
} record_header;
#endif

static UDPsocket receive_socket;
static UDPpacket *receive_packet;
static UDPpacket *send_packet;

#define MAX_SAFE_PACKET_SIZE  512

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
   receive_packet = SDLNet_AllocPacket(MAX_SAFE_PACKET_SIZE);
   send_packet    = SDLNet_AllocPacket(MAX_SAFE_PACKET_SIZE);

   server_address.host = (127<<24) + 1;
   server_address.port = server_port;
   return True;
}

#define MAX_CONNECTIONS 32

connection_t connection[MAX_CONNECTIONS];
int connection_for_pid[PLAYER_OBJECT_MAX];

int create_connection(address *addr)
{
   int i;
   for (i=0; i < MAX_CONNECTIONS; ++i)
      if (connection[i].addr.port == 0) {
         objid pid;
         connection[i].pid = pid = allocate_player();
         if (pid == 0)
            return -1;
         connection[i].addr = *addr;
         connection_for_pid[connection[i].pid] = i;
         obj[pid].position.x = (float) (rand() & 31);
         obj[pid].position.y = (float) (rand() & 31);
         obj[pid].position.z = 100;
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
#define INPUT_PACKETS_PER_SECOND  30

typedef struct
{
   uint8 type;
   uint8 sequence;
   player_net_controls last_inputs[NUM_INPUTS_PER_PACKET];
} net_client_input;

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

void server_net_tick(void)
{
   int i;
   net_client_input input;

   address addr;
   while (net_receive(&input, sizeof(input), &addr) >= 0) {
      int n = find_connection(&addr);
      if (n < 0) {
         n = create_connection(&addr);
         if (n < 0)
            continue;
      }
      p_input[connection[n].pid].buttons = input.last_inputs[0].buttons;
      obj[connection[n].pid].ang.x = angle_from_network(input.last_inputs[0].view_x) - 90;
      obj[connection[n].pid].ang.z = angle_from_network(input.last_inputs[0].view_z);
   }

   for (i=1; i < max_player_id; ++i) {
      if (obj[i].valid) {
         int conn = connection_for_pid[i];
         obj[0].valid = i;
         net_send(obj, sizeof(obj[0])*8, &connection[conn].addr);
      }
   }
}

static net_client_input input;
void client_net_tick(void)
{
   int i;
   vec ang;
   address receive_addr;

   ang = obj[player_id].ang;

   ang.x += 90;
   ang.x = (float) fmod(ang.x, 360);
   if (ang.x < 0) ang.x += 360;
   ang.z = (float) fmod(ang.z, 360);
   if (ang.z < 0) ang.z += 360;

   input.sequence += 1;
   input.type = 0;
   memmove(&input.last_inputs[1], &input.last_inputs[0], sizeof(input.last_inputs) - sizeof(input.last_inputs[0]));

   input.last_inputs[0].buttons = client_player_input.buttons;
   input.last_inputs[0].view_x = angle_to_network(ang.x);
   input.last_inputs[0].view_z = angle_to_network(ang.z);

   ang.x -= 90;   
   obj[player_id].ang = ang;

   net_send(&input, sizeof(input), &server_address);

   while (net_receive(obj, sizeof(obj[0])*8, &receive_addr) >= 0) {
      // @TODO: veryify receive_addr == server_address
      obj[player_id].ang = ang;
      player_id = obj[0].valid;
   }
   for (i=1; i < 8; ++i)
      if (obj[i].valid && i >= max_player_id)
         max_player_id = i+1;
}
