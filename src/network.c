#include "obbg_funcs.h"

#include <arpa/inet.h>
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

// BUG: the code doesn't convert these to network byte order,
// but they fail to consistently, so it should still be
// sending & receiving the same port number, so it shouldn't prevent
// it from working. (4077 becomes 60687, and 4127 becomes 7952)
#define CLIENT_PORT 4077
#define SERVER_PORT 4127

static UDPsocket send_socket;
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
   send_packet->address.port = is_server ? SERVER_PORT : CLIENT_PORT;
   send_packet->address.host = INADDR_BROADCAST;

   res = SDLNet_UDP_Send(receive_socket, -1, send_packet);

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

void net_init(void)
{
   int len;
   char *test = "Hello world!";
   char buffer[513];
   send_socket = SDLNet_UDP_Open(0);
   receive_socket = SDLNet_UDP_Open(is_server ? htons(SERVER_PORT) : htons(CLIENT_PORT));
   receive_packet = SDLNet_AllocPacket(MAX_SAFE_PACKET_SIZE);
   send_packet    = SDLNet_AllocPacket(MAX_SAFE_PACKET_SIZE);

   if (!net_send(test, strlen(test))) {
      ods("Whoops!");
   }
   SDL_Delay(500);
   len = net_receive(buffer, sizeof(buffer)-1);
   if (len >= 0) {
      buffer[len] = 0;
      ods("Received '%s'", buffer);
   }
}

