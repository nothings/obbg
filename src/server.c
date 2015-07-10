//
//  Server's responsibility:
//
//    - authoratative physics
//    - generate terrain for physics
//    - AI
//
//  Client's responsibility:
//
//    - predictive physics
//    - generate terrain for rendering
//    - generate terrain for physics
//    X convert generated terrain to meshes
//    - render meshes
//    - poll user input
//
//  Terrain generator's responsibility:
//
//    - maintain gen_chunk cache
//    - generate gen_chunks
//    - convert generated terrain to meshes

#include "obbg_funcs.h"

#define S_PHYSICS_CACHE_X_LOG2  2
#define S_PHYSICS_CACHE_Y_LOG2  2

#define S_PHYSICS_CACHE_X  (1 << S_PHYSICS_CACHE_X_LOG2)
#define S_PHYSICS_CACHE_Y  (1 << S_PHYSICS_CACHE_Y_LOG2)


static mesh_chunk s_phys_cache[S_PHYSICS_CACHE_Y][S_PHYSICS_CACHE_X];
static int player_x, player_y;
vec3i server_cache_feedback[64][64];

void s_init_physics_cache(void)
{
   int i,j;
   for (j=0; j < S_PHYSICS_CACHE_Y; ++j)
      for (i=0; i < S_PHYSICS_CACHE_X; ++i)
         s_phys_cache[j][i].chunk_x = i+1;
}

void update_server_cache_feedback(void)
{
   int i,j;
   for (j=0; j < S_PHYSICS_CACHE_Y; ++j) {
      for (i=0; i < S_PHYSICS_CACHE_X; ++i) {
         mesh_chunk *phys_cache_mc = &s_phys_cache[j][i];
         server_cache_feedback[j][i].x = phys_cache_mc->chunk_x;// - C_MESH_CHUNK_X_FOR_WORLD_X(player_x);
         server_cache_feedback[j][i].y = phys_cache_mc->chunk_y;// - C_MESH_CHUNK_Y_FOR_WORLD_Y(player_y);
      }
   }
}


int s_set_player_coord(requested_mesh *rm, int max_req, int px, int py)
{
   int i,j,n=0;

   int player_cx, player_cy;

   player_x = px;
   player_y = py;

   player_cx = C_MESH_CHUNK_X_FOR_WORLD_X(player_x);
   player_cy = C_MESH_CHUNK_Y_FOR_WORLD_Y(player_y);

   for (j=0; j < S_PHYSICS_CACHE_Y; ++j) {
      for (i=0; i < S_PHYSICS_CACHE_X; ++i) {
         int rx = player_cx - S_PHYSICS_CACHE_X/2 + i;
         int ry = player_cy - S_PHYSICS_CACHE_Y/2 + j;
         mesh_chunk *phys_cache_mc = &s_phys_cache[ry & (S_PHYSICS_CACHE_Y-1)][rx & (S_PHYSICS_CACHE_X-1)];
         if (phys_cache_mc->chunk_x != rx || phys_cache_mc->chunk_y != ry) {
            if (n < max_req) {
               rm[n].x = rx << MESH_CHUNK_SIZE_X_LOG2;
               rm[n].y = ry << MESH_CHUNK_SIZE_Y_LOG2;
               rm[n].state = RMS_requested;
               rm[n].needs_triangles = False;
               ++n;
            }
         }
      }
   }

   update_server_cache_feedback();

   return n;
}

void s_process_mesh_chunk(mesh_chunk *mc)
{
   int player_cx = C_MESH_CHUNK_X_FOR_WORLD_X(player_x);
   int player_cy = C_MESH_CHUNK_Y_FOR_WORLD_Y(player_y);

   // if it's within the player bounds, update it
   if (mc->chunk_x >= player_cx - S_PHYSICS_CACHE_X/2 && mc->chunk_x < player_cx + S_PHYSICS_CACHE_X/2 &&
       mc->chunk_y >= player_cy - S_PHYSICS_CACHE_Y/2 && mc->chunk_y < player_cy + S_PHYSICS_CACHE_Y/2)
   {
      int phys_cache_x = (mc->chunk_x & (S_PHYSICS_CACHE_X-1));
      int phys_cache_y = (mc->chunk_y & (S_PHYSICS_CACHE_Y-1));
      mesh_chunk *phys_cache_mc = &s_phys_cache[phys_cache_y][phys_cache_x];

      free_mesh_chunk_physics(phys_cache_mc);

      *phys_cache_mc = *mc;
   }

   update_server_cache_feedback();
}
