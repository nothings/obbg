#include "obbg_funcs.h"

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

#define CLIENT_PORT 4077
#define SERVER_PORT 4127

static UDPsocket receive_socket;
static UDPpacket *receive_packet;
static UDPpacket *send_packet;

#define MAX_SAFE_PACKET_SIZE  512

Bool net_send(void *buffer, size_t buffer_size)
{
   int res;

   assert(buffer_size <= (size_t) send_packet->maxlen);
   if (buffer_size > (size_t) send_packet->maxlen)
      return False;

   memcpy(send_packet->data, buffer, buffer_size);
   send_packet->len = buffer_size;

   // loopback to same machine & socket for quick testing
   //send_packet->address.port = is_server ? SERVER_PORT : CLIENT_PORT;
   SDLNet_Write16(is_server ? CLIENT_PORT : SERVER_PORT, &send_packet->address.port);

   // one of these must be right!

   send_packet->address.host = (127<<  0)+(1 << 24);  // 127.0.0.1
   res = SDLNet_UDP_Send(receive_socket, -1, send_packet);
   // currently this returns 1

   return True;
}

int net_receive(void *buffer, size_t buffer_size)
{
   int result = SDLNet_UDP_Recv(receive_socket, receive_packet);
   if (result == 0) {
      return -1;
   }
   if (result == -1) {
      // ??? don't know what the error case is
   }
   if (result == 1) {
      memcpy(buffer, receive_packet->data, stb_min(buffer_size, (size_t) receive_packet->len));
      return receive_packet->len;
   }
   return -1;
}

Bool net_init(void)
{
   int len;
   char *test = "Hello world!";
   char buffer[513];

   if (SDLNet_Init() != 0)
      return False;

   receive_socket = SDLNet_UDP_Open(is_server ? SERVER_PORT : CLIENT_PORT);
   receive_packet = SDLNet_AllocPacket(MAX_SAFE_PACKET_SIZE);
   send_packet    = SDLNet_AllocPacket(MAX_SAFE_PACKET_SIZE);

   if (!net_send(test, strlen(test))) {
      ods("Whoops!");
   }
   len = net_receive(buffer, sizeof(buffer)-1);
   if (len >= 0) {
      buffer[len] = 0;
      ods("Received '%s'", buffer);
   }

   return True;
}

