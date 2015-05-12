#define STB_GLEXT_DECLARE "glext_list.h"
#include "stb_gl.h"
#include "stb_image.h"
#include "stb_glprog.h"

#include "stb.h"
#include "sdl.h"
#include "sdl_thread.h"

#include <math.h>
#include <assert.h>

#include "u_noise.h"
#include "obbg_data.h"
#include "obbg_funcs.h"

#define STBVOX_CONFIG_MODE  1

#define STBVOX_CONFIG_DISABLE_TEX2
#define STBVOX_CONFIG_OPENGL_MODELVIEW
//#define STBVOX_CONFIG_PREFER_TEXBUFFER
#define STBVOX_CONFIG_LIGHTING_SIMPLE
#define STBVOX_CONFIG_FOG_SMOOTHSTEP
//#define STBVOX_CONFIG_PREMULTIPLIED_ALPHA  // this doesn't work properly alpha test without next #define
//#define STBVOX_CONFIG_UNPREMULTIPLY  // slower, fixes alpha test makes windows & fancy leaves look better

#define STBVOX_ROTATION_IN_LIGHTING
#define STB_VOXEL_RENDER_IMPLEMENTATION
#include "stb_voxel_render.h"

extern void ods(char *fmt, ...);

unsigned char geom_map[] =
{
   STBVOX_GEOM_empty,
   STBVOX_GEOM_solid,
   STBVOX_GEOM_solid,
   STBVOX_GEOM_solid,
   STBVOX_GEOM_solid,
};

static unsigned char tex1_for_blocktype[256][6];
static unsigned char tex2_for_blocktype[256][6];
static unsigned char color_for_blocktype[256][6];
static unsigned char geom_for_blocktype[256];

// proc gen mesh
#define GEN_CHUNK_SIZE_X_LOG2     5
#define GEN_CHUNK_SIZE_Y_LOG2     5
#define GEN_CHUNK_SIZE_X         (1 << GEN_CHUNK_SIZE_X_LOG2)
#define GEN_CHUNK_SIZE_Y         (1 << GEN_CHUNK_SIZE_Y_LOG2)

#define GEN_CHUNK_CACHE_X_LOG2    5
#define GEN_CHUNK_CACHE_Y_LOG2    5
#define GEN_CHUNK_CACHE_X         (1 << GEN_CHUNK_CACHE_X_LOG2)
#define GEN_CHUNK_CACHE_Y         (1 << GEN_CHUNK_CACHE_Y_LOG2)

#define GEN_CHUNK_X_FOR_WORLD_X(x)   ((x) >> GEN_CHUNK_SIZE_X_LOG2)
#define GEN_CHUNK_Y_FOR_WORLD_Y(y)   ((y) >> GEN_CHUNK_SIZE_Y_LOG2)

#define Z_SEGMENT_SIZE_LOG2    4
#define Z_SEGMENT_SIZE         (1 << Z_SEGMENT_SIZE_LOG2)
#define NUM_Z_SEGMENTS   16

#if MAX_Z > Z_SEGMENT_SIZE * NUM_Z_SEGMENTS
#error "error, not enough z segments"
#endif

typedef struct
{
   uint8 block   [GEN_CHUNK_SIZE_Y][GEN_CHUNK_SIZE_X][Z_SEGMENT_SIZE];
   uint8 lighting[GEN_CHUNK_SIZE_Y][GEN_CHUNK_SIZE_X][Z_SEGMENT_SIZE];
   uint8 overlay [GEN_CHUNK_SIZE_Y][GEN_CHUNK_SIZE_X][Z_SEGMENT_SIZE];
} gen_chunk_partial;

struct st_gen_chunk
{
   int ref_count;
   gen_chunk_partial partial  [NUM_Z_SEGMENTS];
   unsigned char     non_empty[NUM_Z_SEGMENTS];
};

typedef struct
{
   int chunk_x, chunk_y;
   gen_chunk *chunk;
} gen_chunk_cache;

       mesh_chunk      mesh_cache[MESH_CHUNK_CACHE_Y][MESH_CHUNK_CACHE_X];
static gen_chunk_cache  gen_cache[ GEN_CHUNK_CACHE_Y][ GEN_CHUNK_CACHE_X];

mesh_chunk *get_mesh_chunk_for_coord(int x, int y)
{
   int cx = MESH_CHUNK_X_FOR_WORLD_X(x);
   int cy = MESH_CHUNK_Y_FOR_WORLD_Y(y);
   mesh_chunk *mc = &mesh_cache[cy & (MESH_CHUNK_CACHE_Y-1)][cx & (MESH_CHUNK_CACHE_X-1)];

   if (mc->chunk_x == cx && mc->chunk_y == cy)
      return mc;
   else
      return NULL;
}

void free_mesh_chunk(mesh_chunk *mc)
{
   glDeleteTextures(1, &mc->fbuf_tex);
   glDeleteBuffersARB(1, &mc->vbuf);
   glDeleteBuffersARB(1, &mc->fbuf);
}

gen_chunk_cache *get_gen_chunk_cache_for_coord(int x, int y)
{
   int cx = GEN_CHUNK_X_FOR_WORLD_X(x);
   int cy = GEN_CHUNK_Y_FOR_WORLD_Y(y);
   gen_chunk_cache *gc = &gen_cache[cy & (GEN_CHUNK_CACHE_Y-1)][cx & (GEN_CHUNK_CACHE_X-1)];

   if (gc->chunk_x == cx && gc->chunk_y == cy) {
      assert(gc->chunk != NULL);
      return gc;
   } else
      return NULL;
}

void init_mesh_building(void)
{
   int i,j;
   for (i=0; i < 256; ++i)
      for (j=0; j < 6; ++j)
         tex1_for_blocktype[i][j] = (uint8) i-1;
}

void init_chunk_caches(void)
{
   int i,j;
   for (j=0; j < MESH_CHUNK_CACHE_Y; ++j) {
      for (i=0; i < MESH_CHUNK_CACHE_X; ++i) {
         assert(mesh_cache[j][i].vbuf == 0);
         mesh_cache[j][i].vbuf = 0;
         mesh_cache[j][i].chunk_x = i+1;
      }
   }

   for (j=0; j < GEN_CHUNK_CACHE_Y; ++j) {
      for (i=0; i < GEN_CHUNK_CACHE_X; ++i) {
         assert(gen_cache[j][i].chunk == NULL);
         gen_cache[j][i].chunk = NULL;
         gen_cache[j][i].chunk_x = i+1;
      }
   }
}

void release_gen_chunk(gen_chunk *gc)
{
   assert(gc->ref_count > 0);
   --gc->ref_count;
   if (gc->ref_count == 0)
      free(gc);
}

void free_gen_chunk(gen_chunk_cache *gcc)
{
   release_gen_chunk(gcc->chunk);
   gcc->chunk = NULL;
}

#define MIN_GROUND 32
#define AVG_GROUND 64

float compute_height_field(int x, int y)
{
   float ht = AVG_GROUND;
#if 1
   float weight=0;
   int o;
   weight = (float) stb_linear_remap(stb_perlin_noise3(x/256.0f,y/256.0f,100,256,256,256), -1.5, 1.5, -4.0f, 5.0f);
   weight = stb_clamp(weight,0,1);
   for (o=0; o < 8; ++o) {
      float scale = (float) (1 << o);
      float ns = stb_perlin_noise3(x/scale, y/scale, o*2.0f, 256,256,256), heavier;
      float sign = (ns < 0 ? -1.0f : 1.0f);
      ns = (float) fabs(ns);
      heavier = ns*ns*ns*ns*4;
      ht += scale/2 * stb_lerp(weight, ns, heavier);
   }
#else

   if (x >= 10 && x <= 50 && y >= 30 && y <= 70) {
      ht += x;
   }

   if (x >= 110 && x <= 150 && y >= 30 && y <= 70) {
      ht += y;
   }

#endif
   return ht;
}

gen_chunk *generate_chunk(int x, int y)
{
   int z_seg;
   int i,j,z;
   int ground_top = 0;
   gen_chunk *gc = malloc(sizeof(*gc));
   float height_field[GEN_CHUNK_SIZE_Y+2][GEN_CHUNK_SIZE_X+2];
   int height_field_int[GEN_CHUNK_SIZE_Y+2][GEN_CHUNK_SIZE_X+2];
   int corner_tex[2][2], corner_weight[2][2];
   int block_type[GEN_CHUNK_SIZE_Y][GEN_CHUNK_SIZE_X];

   for (j=-1; j <= GEN_CHUNK_SIZE_Y; ++j)
      for (i=-1; i <= GEN_CHUNK_SIZE_X; ++i) {
         float ht = compute_height_field(x+i,y+j);
         height_field[j+1][i+1] = ht;
         height_field_int[j+1][i+1] = (int) height_field[j+1][i+1];
         ground_top = stb_max(ground_top, height_field_int[j+1][i+1]);
      }  

   memset(gc->non_empty, 0, sizeof(gc->non_empty));
   memset(gc->non_empty, 1, (ground_top+Z_SEGMENT_SIZE-1)>>Z_SEGMENT_SIZE_LOG2);

   for (j=0; j < 2; ++j) {
      for (i=0; i < 2; ++i) {
         unsigned int val = fast_noise(x+i*GEN_CHUNK_SIZE_X,y+j*GEN_CHUNK_SIZE_Y,3,777);
         val = (val*16)/65536;
         corner_tex[j][i] = val;
         corner_weight[j][i] = 32+(fast_noise(x+i*GEN_CHUNK_SIZE_X,y+j*GEN_CHUNK_SIZE_Y,3,666)&255);
      }
   }
   
   for (j=0; j < GEN_CHUNK_SIZE_Y; ++j) {
      for (i=0; i < GEN_CHUNK_SIZE_X; ++i) {
         int best=0, best_w=0, w;
         w = corner_weight[0][0]*(GEN_CHUNK_SIZE_X+i)*(GEN_CHUNK_SIZE_Y-j);
         if (w >= best_w) {
            best_w = w;
            best = corner_tex[0][0];
         }
         w = corner_weight[0][1]*(i)*(GEN_CHUNK_SIZE_Y-j);
         if (w >= best_w) {
            best_w = w;
            best = corner_tex[0][1];
         }
         w = corner_weight[1][0]*(GEN_CHUNK_SIZE_X-i)*(j);
         if (w >= best_w) {
            best_w = w;
            best = corner_tex[1][0];
         }
         w = corner_weight[1][1]*(i)*(j);
         if (w >= best_w) {
            best_w = w;
            best = corner_tex[1][1];
         }
         block_type[j][i] = (BT_solid + best);
      }
   }

   for (z_seg=0; z_seg < NUM_Z_SEGMENTS; ++z_seg) {
      int z0 = z_seg * Z_SEGMENT_SIZE;
      gen_chunk_partial *gcp = &gc->partial[z_seg];
      for (j=0; j < GEN_CHUNK_SIZE_Y; ++j) {
         for (i=0; i < GEN_CHUNK_SIZE_X; ++i) {
            int ht = height_field_int[j+1][i+1];
            int z_limit = stb_clamp(ht-z0, 0, Z_SEGMENT_SIZE);
            assert(z_limit >= 0 && Z_SEGMENT_SIZE - z_limit >= 0);
            memset(&gcp->block[j][i][   0   ], block_type[j][i], z_limit);
            memset(&gcp->block[j][i][z_limit],     BT_empty    , Z_SEGMENT_SIZE - z_limit);
         }
      }
   }

   // compute lighting for every block by weighted average of neighbors

   // loop through every partial chunk separately

   for (z_seg=0; (z_seg < NUM_Z_SEGMENTS); ++z_seg) {
      gen_chunk_partial *gcp = &gc->partial[z_seg];
      int z0 = z_seg * Z_SEGMENT_SIZE;
      int z_limit;
      z_limit = Z_SEGMENT_SIZE;
      if (z_limit + z0 > ground_top+1)
         z_limit = ground_top+1 - z0;
      if (z_limit < 0)
         break;
      for (j=0; j < GEN_CHUNK_SIZE_Y; ++j) {
         for (i=0; i < GEN_CHUNK_SIZE_X; ++i) {
            unsigned char *lt = &gcp->lighting[j][i][0];
            for (z=0; z < z_limit; ++z) {
               int light;
               if (z0+z < height_field_int[j+1][i+1]) {
                  light = 0;
               } else {
                  int m,n;
                  light = 0;
                  for (m=-1; m <= 1; ++m) {
                     for (n=-1; n <= 1; ++n) {
                        int ht = height_field_int[j+1+m][i+1+n];
                        int val = z0+z-ht+2;

                        if (val < 0) val=0;
                        if (val > 3) val = 3;
                        light += val;
                     }
                  }
                  // 27 ?
                  light += 4;
                  light = light << 3;
               }

               *lt++ = light;
            }
            for (; z < Z_SEGMENT_SIZE; ++z)
               *lt++ = 255;
         }
      }
   }

   for (; z_seg < NUM_Z_SEGMENTS; ++z_seg) {
      gen_chunk_partial *gcp = &gc->partial[z_seg];
      for (j=0; j < GEN_CHUNK_SIZE_Y; ++j)
         for (i=0; i < GEN_CHUNK_SIZE_X; ++i) {
            unsigned char *lt = &gcp->lighting[j][i][0];
            for (z=0; z < Z_SEGMENT_SIZE; ++z)
               *lt++ = 255;
         }
   }

   gc->ref_count = 0;

   return gc;
}

gen_chunk_cache * put_chunk_in_cache(int x, int y, gen_chunk *gc)
{
   int cx = GEN_CHUNK_X_FOR_WORLD_X(x);
   int cy = GEN_CHUNK_Y_FOR_WORLD_Y(y);
   int slot_x = cx & (GEN_CHUNK_CACHE_X-1);
   int slot_y = cy & (GEN_CHUNK_CACHE_Y-1);
   gen_chunk_cache *gcc = &gen_cache[slot_y][slot_x];
   assert(gcc->chunk_x != x || gcc->chunk_y != y || gcc->chunk == NULL);
   if (gcc->chunk != NULL)
      free_gen_chunk(gcc);
   gcc->chunk_x = cx;
   gcc->chunk_y = cy;
   gcc->chunk = gc;
   ++gc->ref_count;
   return gcc;
}

gen_chunk *get_gen_chunk_for_coord(int x, int y)
{
   gen_chunk_cache *gcc = get_gen_chunk_cache_for_coord(x,y);
   if (gcc == NULL) {
      gcc = put_chunk_in_cache(x, y, generate_chunk(x, y));
   }
   assert(gcc->chunk_x*GEN_CHUNK_SIZE_X == x && gcc->chunk_y*GEN_CHUNK_SIZE_Y == y);
   return gcc->chunk;
}


// mesh building
//
// To build a mesh that is 64x64x255, we need input data 66x66x257.
// If proc gen chunks are 32x32, we need a grid of 4x4 of them:
//
//                stb_voxel_render
//    x range      x coord needed   segment-array
//   of chunk       for meshing       x coord
//   -32..-1            -1                0           1 block
//     0..31           0..31            1..32        32 blocks
//    32..63          32..63           33..64        32 blocks
//    64..95            64               65           1 block

typedef struct
{
   int x,y,z;
} vec3i;

uint8 segment_blocktype[66][66][18];
uint8 segment_lighting[66][66][18];

void copy_chunk_set_to_segment(chunk_set *chunks, int z_seg)
{
   int j,i,x,y;
   for (j=0; j < 4; ++j)
      for (i=0; i < 4; ++i) {
         gen_chunk_partial *gcp;

         int x_off = (i-1) * GEN_CHUNK_SIZE_X + 1;
         int y_off = (j-1) * GEN_CHUNK_SIZE_Y + 1;

         int x0,y0,x1,y1;

         x0 = 0; x1 = GEN_CHUNK_SIZE_X;
         y0 = 0; y1 = GEN_CHUNK_SIZE_Y;

         if (x_off + x0 <  0) x0 = 0 - x_off;
         if (x_off + x1 > 66) x1 = 66 - x_off;
         if (y_off + y0 <  0) y0 = 0 - y_off;
         if (y_off + y1 > 66) y1 = 66 - y_off;

         gcp = &chunks->chunk[j][i]->partial[z_seg];
         for (y=y0; y < y1; ++y) {
            uint8 *bt = &segment_blocktype[y+y_off][x_off][0];
            uint8 *lt = &segment_lighting [y+y_off][x_off][0];
            for (x=x0; x < x1; ++x) {
               memcpy(bt + sizeof(segment_blocktype[0][0])*x, &gcp->block   [y][x][0], 16);
               memcpy(lt + sizeof(segment_lighting [0][0])*x, &gcp->lighting[y][x][0], 16);
            }
         }
      }
}

void generate_mesh_for_chunk_set(stbvox_mesh_maker *mm, mesh_chunk *mc, vec3i world_coord, chunk_set *chunks, uint8 *vertex_build_buffer, size_t build_size, uint8 *face_buffer)
{
   int a,b,z;

   stbvox_input_description *map;

   assert((world_coord.x & (MESH_CHUNK_SIZE_X-1)) == 0);
   assert((world_coord.y & (MESH_CHUNK_SIZE_Y-1)) == 0);

   mc->chunk_x = (world_coord.x >> MESH_CHUNK_SIZE_X_LOG2);
   mc->chunk_y = (world_coord.y >> MESH_CHUNK_SIZE_Y_LOG2);

   stbvox_set_input_stride(mm, 18, 66*18);

   map = stbvox_get_input_description(mm);
   map->block_tex1_face = tex1_for_blocktype;
   //map->block_tex2_face = tex2_for_blocktype;
   //static unsigned char color_for_blocktype[256][6];
   //static unsigned char geom_for_blocktype[256];
   //map->block_geometry = minecraft_geom_for_blocktype;

   //stbvox_reset_buffers(mm);
   stbvox_set_buffer(mm, 0, 0, vertex_build_buffer, build_size);
   stbvox_set_buffer(mm, 0, 1, face_buffer , build_size>>2);

   map->blocktype = &segment_blocktype[1][1][1]; // this is (0,0,0), but we need to be able to query off the edges
   map->lighting = &segment_lighting[1][1][1];

   // fill in the top two rows of the buffer
   for (b=0; b < 66; ++b) {
      for (a=0; a < 66; ++a) {
         segment_blocktype[b][a][16] = 0;
         segment_blocktype[b][a][17] = 0;
         segment_lighting [b][a][16] = 255;
         segment_lighting [b][a][17] = 255;
      }
   }

   z = 256-16;  // @TODO use MAX_Z and Z_SEGMENT_SIZE

   for (; z >= 0; z -= 16)  // @TODO use MAX_Z and Z_SEGMENT_SIZE
   {
      int z0 = z;
      int z1 = z+16;
      if (z1 == 256) z1 = 255;  // @TODO use MAX_Z and Z_SEGMENT_SIZE

      copy_chunk_set_to_segment(chunks, z >> 4);   // @TODO use MAX_Z and Z_SEGMENT_SIZE

      map->blocktype = &segment_blocktype[1][1][1-z];
      map->lighting = &segment_lighting[1][1][1-z];

      {
         stbvox_set_input_range(mm, 0,0,z0, 64,64,z1);
         stbvox_set_default_mesh(mm, 0);
         stbvox_make_mesh(mm);
      }

      // copy the bottom two rows of data up to the top
      for (b=0; b < 66; ++b) {
         for (a=0; a < 66; ++a) {
            segment_blocktype[b][a][16] = segment_blocktype[b][a][0];
            segment_blocktype[b][a][17] = segment_blocktype[b][a][1];
            segment_lighting [b][a][16] = segment_lighting [b][a][0];
            segment_lighting [b][a][17] = segment_lighting [b][a][1];
         }
      }
   }

   stbvox_set_mesh_coordinates(mm, world_coord.x, world_coord.y, world_coord.z);
   stbvox_get_transform(mm, mc->transform);

   stbvox_set_input_range(mm, 0,0,0, 64,64,255);
   stbvox_get_bounds(mm, mc->bounds);

   mc->num_quads = stbvox_get_quad_count(mm, 0);
}

static uint8 vertex_build_buffer[64*1024*1024];
static uint8 face_buffer[16*1024*1024];

mesh_chunk *build_mesh_chunk_for_coord(int x, int y)
{
   int cx = MESH_CHUNK_X_FOR_WORLD_X(x);
   int slot_x = cx & (MESH_CHUNK_CACHE_X-1);
   int cy = MESH_CHUNK_Y_FOR_WORLD_Y(y);
   int slot_y = cy & (MESH_CHUNK_CACHE_Y-1);
   mesh_chunk *mc = &mesh_cache[slot_y][slot_x];

   if (mc->vbuf) {
      assert(mc->chunk_x != cx || mc->chunk_y != cy);
      free_mesh_chunk(mc);
   }

   {
      stbvox_mesh_maker mm;
      chunk_set cs;
      int i,j;
      vec3i wc = { x,y,0 };

      for (j=0; j < 4; ++j)
         for (i=0; i < 4; ++i)
            cs.chunk[j][i] = get_gen_chunk_for_coord(x + (i-1)*GEN_CHUNK_SIZE_X, y + (j-1)*GEN_CHUNK_SIZE_Y);

      stbvox_init_mesh_maker(&mm);
      generate_mesh_for_chunk_set(&mm, mc, wc, &cs, vertex_build_buffer, sizeof(vertex_build_buffer), face_buffer);
      upload_mesh(mc, vertex_build_buffer, face_buffer);
   }

   return mc;
}


#if 0
void init_mesh_build_threads(void)
{
}
#else
/*
         SDL_SemPost(mw->request_received);
      SDL_SemWait(mw->request_received);
      SDL_LockMutex(chunk_cache_mutex);
      for (j=0; j < 4; ++j)
         for (i=0; i < 4; ++i) {
            deref_fastchunk(mw->chunks[j][i]);
            mw->chunks[j][i] = NULL;
         }
      SDL_UnlockMutex(chunk_cache_mutex);
      data->request_received = SDL_CreateSemaphore(0);
      data->chunk_server_done_processing = SDL_CreateSemaphore(0);
      SDL_CreateThread(mesh_worker_handler, "mesh worker", data);
   int num_proc = SDL_GetCPUCount();
*/

#define MAX_MESH_WORKERS 32

enum
{
   JOB_none,

   JOB_build_mesh,
   JOB_generate_terrain,
};

typedef struct
{
   int task_type;
   int world_x, world_y;

   // build_mesh:
   chunk_set cs;

   // generate_terrain:
} task;

typedef struct
{
   gen_chunk *gc;
   int world_x;
   int world_y;
} finished_gen_chunk;

#define MAX_TASKS          1024
#define MAX_GEN_CHUNKS     256

task                mesh_task_queue[MAX_TASKS];
task                gen_task_queue[MAX_TASKS];
finished_gen_chunk  gen_queue[MAX_GEN_CHUNKS];


typedef struct
{
   SDL_mutex *lock;
   SDL_cond *cond;
   int event;
} WakeableWaiter;

void waiter_wait(WakeableWaiter *ww)
{
   SDL_LockMutex(ww->lock);
   while (!ww->event)
      SDL_CondWait(ww->cond, ww->lock);
   ww->event = 0;
   SDL_UnlockMutex(ww->lock);
}
 
void waiter_wake(WakeableWaiter *ww)
{
   SDL_LockMutex(ww->lock);
   ww->event = 1;
   SDL_CondSignal(ww->cond);
   SDL_UnlockMutex(ww->lock);
}

void init_WakeableWaiter(WakeableWaiter *ww)
{
   ww->lock = SDL_CreateMutex();
   ww->cond = SDL_CreateCond();
   ww->event = 0;
}
 
void shutdown_WakeableWaiter(WakeableWaiter *ww)
{ 
   SDL_DestroyCond(ww->cond);
   SDL_DestroyMutex(ww->lock);
}

WakeableWaiter manager_monitor;
SDL_sem *pending_task_count;

typedef struct
{
   SDL_mutex *mutex;
   unsigned char padding[64];
   int  head,tail;
   int count;
   size_t itemsize;
   uint8 *data;
} threadsafe_queue;

void init_threadsafe_queue(threadsafe_queue *tq, int count, size_t size, void *data)
{
   tq->mutex = SDL_CreateMutex();
   tq->head = tq->tail = 0;
   tq->count = count;
   tq->itemsize = size;
   tq->data = data;
}


int add_to_queue(threadsafe_queue *tq, void *item)
{
   int retval = false;
   SDL_LockMutex(tq->mutex);
   if ((tq->head+1) % tq->count != tq->tail) {
      memcpy(tq->data + tq->itemsize * tq->head, item, tq->itemsize);
      tq->head = (tq->head+1) % tq->count;
      retval = true;
   }
   SDL_UnlockMutex(tq->mutex);

   return retval;
}

int get_from_queue(threadsafe_queue *tq, void *item)
{
   int retval=false;
   SDL_LockMutex(tq->mutex);
   if (tq->head != tq->tail) {
      memcpy(item, tq->data + tq->itemsize * tq->tail, tq->itemsize);
      ++tq->tail;
      if (tq->tail >= tq->count)
         tq->tail = 0;
      retval = true;
   }
   SDL_UnlockMutex(tq->mutex);
   return retval;
}

int get_from_queue_nonblocking(threadsafe_queue *tq, void *item)
{
   int retval=false;
   if (SDL_TryLockMutex(tq->mutex) == SDL_MUTEX_TIMEDOUT)
      return false;
   if (tq->head != tq->tail) {
      memcpy(item, tq->data + tq->itemsize * tq->tail, tq->itemsize);
      ++tq->tail;
      if (tq->tail >= tq->count)
         tq->tail = 0;
      retval = true;
   }
   SDL_UnlockMutex(tq->mutex);
   return retval;
}

built_mesh     built_queue              [MAX_BUILT_MESHES];
requested_mesh renderer_requested_meshes[MAX_BUILT_MESHES];

threadsafe_queue built_meshes, finished_gen;
threadsafe_queue pending_meshes, pending_gen;


int get_next_built_mesh(built_mesh *bm)
{
   return get_from_queue_nonblocking(&built_meshes, bm);
}

int add_mesh_task(task *t)
{
   int retval = add_to_queue(&pending_meshes, t);
   if (retval)
      SDL_SemPost(pending_task_count);
   return retval;
}

int add_gen_task(task *t)
{
   int retval = add_to_queue(&pending_gen, t);
   if (retval)
      SDL_SemPost(pending_task_count);
   return retval;
}

int get_pending_task(task *t)
{
   SDL_SemWait(pending_task_count);

   if (get_from_queue(&pending_meshes, t))
      return true;
   if (get_from_queue(&pending_gen, t))
      return true;
   assert(0);
   return false;
}

typedef struct
{
   int x,y;
   int in_use;
} procgen_status;

#define IN_PROGRESS_CACHE_SIZE 128

procgen_status procgen_in_progress[IN_PROGRESS_CACHE_SIZE][IN_PROGRESS_CACHE_SIZE];

int is_in_progress(int x, int y)
{
   int cx = GEN_CHUNK_X_FOR_WORLD_X(x);
   int cy = GEN_CHUNK_Y_FOR_WORLD_Y(y);
   int slot_x = cx & (IN_PROGRESS_CACHE_SIZE-1);
   int slot_y = cy & (IN_PROGRESS_CACHE_SIZE-1);
   return procgen_in_progress[slot_y][slot_x].x == x && procgen_in_progress[slot_y][slot_x].y == y && procgen_in_progress[slot_y][slot_x].in_use;
}

void start_procgen(int x, int y)
{
   int cx = GEN_CHUNK_X_FOR_WORLD_X(x);
   int cy = GEN_CHUNK_Y_FOR_WORLD_Y(y);
   int slot_x = cx & (IN_PROGRESS_CACHE_SIZE-1);
   int slot_y = cy & (IN_PROGRESS_CACHE_SIZE-1);
   procgen_in_progress[slot_y][slot_x].x = x;
   procgen_in_progress[slot_y][slot_x].y = y;
   procgen_in_progress[slot_y][slot_x].in_use = true;
}

void end_procgen(int x, int y)
{
   int cx = GEN_CHUNK_X_FOR_WORLD_X(x);
   int cy = GEN_CHUNK_Y_FOR_WORLD_Y(y);
   int slot_x = cx & (IN_PROGRESS_CACHE_SIZE-1);
   int slot_y = cy & (IN_PROGRESS_CACHE_SIZE-1);
   procgen_in_progress[slot_y][slot_x].x = x;
   procgen_in_progress[slot_y][slot_x].y = y;
   procgen_in_progress[slot_y][slot_x].in_use = false;
}

int can_start_procgen(int x, int y)
{
   int cx = GEN_CHUNK_X_FOR_WORLD_X(x);
   int cy = GEN_CHUNK_Y_FOR_WORLD_Y(y);
   int slot_x = cx & (IN_PROGRESS_CACHE_SIZE-1);
   int slot_y = cy & (IN_PROGRESS_CACHE_SIZE-1);
   return !procgen_in_progress[slot_y][slot_x].in_use;
}


int mesh_worker_handler(void *data)
{
   for(;;) {
      task t;
      if (get_pending_task(&t)) {
         switch (t.task_type) {
            case JOB_build_mesh: {
               stbvox_mesh_maker mm;
               mesh_chunk *mc = malloc(sizeof(*mc));
               built_mesh out_mesh;
               vec3i wc = { t.world_x, t.world_y, 0 };

               stbvox_init_mesh_maker(&mm);

               mc->chunk_x = t.world_x >> MESH_CHUNK_CACHE_X_LOG2;
               mc->chunk_y = t.world_y >> MESH_CHUNK_CACHE_Y_LOG2;

               generate_mesh_for_chunk_set(&mm, mc, wc, &t.cs, vertex_build_buffer, sizeof(vertex_build_buffer), face_buffer);

               out_mesh.vertex_build_buffer = malloc(mc->num_quads * 16);
               out_mesh.face_buffer  = malloc(mc->num_quads *  4);
               memcpy(out_mesh.vertex_build_buffer, vertex_build_buffer, mc->num_quads * 16);
               memcpy(out_mesh.face_buffer, face_buffer, mc->num_quads * 4);
               {
                  int i;
                  for (i=0; i < 16; ++i)
                     release_gen_chunk(t.cs.chunk[0][i]);
               }
               out_mesh.mc = mc;
               if (!add_to_queue(&built_meshes, &out_mesh)) {
                  free(out_mesh.vertex_build_buffer);
                  free(out_mesh.face_buffer);
               } else
                  waiter_wake(&manager_monitor);
               break;
            }
            case JOB_generate_terrain: {
               finished_gen_chunk fgc;
               fgc.world_x = t.world_x;
               fgc.world_y = t.world_y;
               fgc.gc = generate_chunk(t.world_x, t.world_y);
               if (!add_to_queue(&finished_gen, &fgc))
                  release_gen_chunk(fgc.gc);
               else {
                  ++fgc.gc->ref_count;
                  waiter_wake(&manager_monitor);
               }
               break;
            }
         }
      }
   }
}

enum
{
   RMS_invalid,
   
   RMS_requested,
   RMS_chunks_completed_waiting_for_meshing,
};

#define MAX_PROC_GEN 16

SDL_mutex *requested_mesh_mutex;

int worker_manager(void *data)
{
   int outstanding_proc_gen = 0;
   for(;;) {
      int i,j,k;
      waiter_wait(&manager_monitor);

      SDL_LockMutex(finished_gen.mutex);
      while (finished_gen.head != finished_gen.tail) {
         int h = finished_gen.head;
         end_procgen(gen_queue[h].world_x, gen_queue[h].world_y);
         --outstanding_proc_gen;
         put_chunk_in_cache(gen_queue[h].world_x, gen_queue[h].world_y, gen_queue[h].gc);
         --gen_queue[h].gc->ref_count;
         finished_gen.head = (h+1) % finished_gen.count;
      }
      SDL_UnlockMutex(finished_gen.mutex);

      SDL_LockMutex(requested_mesh_mutex);
      for (i=0; i < MAX_BUILT_MESHES; ++i) {
         requested_mesh *rm = &renderer_requested_meshes[i];

         if (rm->state == RMS_requested) {
            int valid_chunks=0;
            for (k=0; k < 4; ++k) {
               for (j=0; j < 4; ++j) {
                  if (!rm->chunk_set_valid[k][j]) {
                     int cx = rm->x + j * GEN_CHUNK_SIZE_X, cy = rm->y + k * GEN_CHUNK_SIZE_Y;
                     gen_chunk_cache *gcc = get_gen_chunk_cache_for_coord(cx, cy);
                     if (gcc) {
                        rm->cs.chunk[k][j] = gcc->chunk;
                        ++gcc->chunk->ref_count;
                     } else {
                        if (outstanding_proc_gen >= MAX_PROC_GEN)
                           continue;
                        if (is_in_progress(cx, cy)) {
                           // it's already in progress, so do nothing
                        } else if (can_start_procgen(cx, cy)) {
                           task t;
                           start_procgen(cx,cy);
                           t.world_x = cx;
                           t.world_y = cy;
                           t.task_type = JOB_generate_terrain;
                           if (add_gen_task(&t))
                              ++outstanding_proc_gen;
                        }
                     }
                  } else {
                     ++valid_chunks;
                  }
               }
            }
            if (valid_chunks == 16) {
               task t;
               t.cs = rm->cs;
               assert(rm->state == RMS_requested);
               rm->state = RMS_chunks_completed_waiting_for_meshing;
               t.task_type = JOB_build_mesh;
               t.world_x = rm->x;
               t.world_y = rm->y;
               add_mesh_task(&t);
            }
         }
      }
      SDL_UnlockMutex(requested_mesh_mutex);
   }
}

int num_mesh_workers;

void init_mesh_build_threads(void)
{
   int i;

   int num_proc = SDL_GetCPUCount();

   init_WakeableWaiter(&manager_monitor);
   init_threadsafe_queue(&built_meshes, MAX_BUILT_MESHES, sizeof(built_queue[0]), built_queue);
   init_threadsafe_queue(&pending_meshes, MAX_TASKS, sizeof(mesh_task_queue[0]), mesh_task_queue);
   init_threadsafe_queue(&pending_gen   , MAX_TASKS, sizeof(gen_task_queue[0]), gen_task_queue);
   init_threadsafe_queue(&finished_gen, MAX_GEN_CHUNKS, sizeof(gen_queue[0]), gen_queue);
   pending_task_count = SDL_CreateSemaphore(0);

   if (num_proc > 6)
      num_mesh_workers = num_proc/2;
   else if (num_proc > 4)
      num_mesh_workers = 4;
   else 
      num_mesh_workers = num_proc-1;

   if (num_mesh_workers > MAX_MESH_WORKERS)
      num_mesh_workers = MAX_MESH_WORKERS;

   if (num_mesh_workers < 1)
      num_mesh_workers = 1;

   for (i=0; i < num_mesh_workers; ++i)
      SDL_CreateThread(mesh_worker_handler, "mesh worker", NULL);

   SDL_CreateThread(worker_manager, "thread manager thread", NULL);
}
#endif
