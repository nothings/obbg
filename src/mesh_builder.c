//  Mesh builder "process"
//
//    Both server & client are customers of this process

#define TRACK_ALLOCATED_GEN_CHUNKS_IN_GLOBAL_TABLE

#include "obbg_data.h"

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
#include "obbg_funcs.h"

#define STBVOX_CONFIG_MODE  1

#define STBVOX_CONFIG_DISABLE_TEX2
#define STBVOX_CONFIG_OPENGL_MODELVIEW
//#define STBVOX_CONFIG_PREFER_TEXBUFFER
#define STBVOX_CONFIG_LIGHTING_SIMPLE
#define STBVOX_CONFIG_FOG_SMOOTHSTEP
//#define STBVOX_CONFIG_PREMULTIPLIED_ALPHA  // this doesn't work properly alpha test without next #define
//#define STBVOX_CONFIG_UNPREMULTIPLY  // slower, fixes alpha test makes windows & fancy leaves look better
#define STBVOX_CONFIG_TEXTURE_TRANSLATION

#define STBVOX_CONFIG_ROTATION_IN_LIGHTING
#define STB_VOXEL_RENDER_IMPLEMENTATION
#include "stb_voxel_render.h"

SDL_mutex *ref_count_mutex;
SDL_mutex *swap_renderer_request_mutex, *manager_mutex;

static unsigned char geom_for_blocktype[256] =
{
   STBVOX_MAKE_GEOMETRY(STBVOX_GEOM_empty, 0, 0),
};

static unsigned char vheight_for_blocktype[256];

static unsigned char tex1_for_blocktype[256][6];
static unsigned char tex2_for_blocktype[256][6];
static unsigned char color_for_blocktype[256][6];

void set_blocktype_texture(int bt, int tex)
{
   int i;
   for (i=0; i < 6; ++i)
      tex1_for_blocktype[bt][i] = tex;
}

float texture_scales[256];

//   -2,0   <---  thread 1
//    6,0   <---  thread 2
//   -2,0   <---  thread 3


void init_mesh_building(void)
{
   int i;

   for (i=1; i < 256; ++i)
      geom_for_blocktype[i] = STBVOX_MAKE_GEOMETRY(STBVOX_GEOM_solid,0,0);
      
   //geom_for_blocktype[BT_conveyor] = STBVOX_GEOM_slab_upper;

   geom_for_blocktype[BT_conveyor_ramp_up_low    ] = STBVOX_MAKE_GEOMETRY(STBVOX_GEOM_floor_vheight_03, 0, 0),
   geom_for_blocktype[BT_conveyor_ramp_up_high   ] = STBVOX_MAKE_GEOMETRY(STBVOX_GEOM_floor_vheight_03, 0, 0),
   geom_for_blocktype[BT_conveyor_ramp_down_high ] = STBVOX_MAKE_GEOMETRY(STBVOX_GEOM_floor_vheight_03, 0, 0),
   geom_for_blocktype[BT_conveyor_ramp_down_low  ] = STBVOX_MAKE_GEOMETRY(STBVOX_GEOM_floor_vheight_03, 0, 0),

   vheight_for_blocktype[BT_conveyor_ramp_up_low    ] = STBVOX_MAKE_VHEIGHT(STBVOX_VERTEX_HEIGHT_0, STBVOX_VERTEX_HEIGHT_half, STBVOX_VERTEX_HEIGHT_0, STBVOX_VERTEX_HEIGHT_half),
   vheight_for_blocktype[BT_conveyor_ramp_up_high   ] = STBVOX_MAKE_VHEIGHT(STBVOX_VERTEX_HEIGHT_half, STBVOX_VERTEX_HEIGHT_1, STBVOX_VERTEX_HEIGHT_half, STBVOX_VERTEX_HEIGHT_1),
   vheight_for_blocktype[BT_conveyor_ramp_down_high ] = STBVOX_MAKE_VHEIGHT(STBVOX_VERTEX_HEIGHT_1, STBVOX_VERTEX_HEIGHT_half, STBVOX_VERTEX_HEIGHT_1, STBVOX_VERTEX_HEIGHT_half),
   vheight_for_blocktype[BT_conveyor_ramp_down_low  ] = STBVOX_MAKE_VHEIGHT(STBVOX_VERTEX_HEIGHT_half, STBVOX_VERTEX_HEIGHT_0, STBVOX_VERTEX_HEIGHT_half, STBVOX_VERTEX_HEIGHT_0),

   set_blocktype_texture(BT_sand, 25);
   set_blocktype_texture(BT_grass, 5);
   set_blocktype_texture(BT_gravel, 2);
   set_blocktype_texture(BT_asphalt, 9);
   set_blocktype_texture(BT_wood, 15);
   set_blocktype_texture(BT_marble, 17);
   set_blocktype_texture(BT_stone, 20);
   set_blocktype_texture(BT_leaves, 1);
   set_blocktype_texture(BT_conveyor, 21);
   set_blocktype_texture(BT_conveyor_ramp_up_low, 21);
   set_blocktype_texture(BT_conveyor_ramp_up_high, 21);
   set_blocktype_texture(BT_conveyor_ramp_down_low, 21);
   set_blocktype_texture(BT_conveyor_ramp_down_high, 21);
   set_blocktype_texture(BT_conveyor_90_left, 21);
   set_blocktype_texture(BT_conveyor_90_right, 21);

   tex1_for_blocktype[BT_conveyor         ][FACE_up] = 22;
   tex1_for_blocktype[BT_conveyor_90_right][FACE_up] = 0;
   tex1_for_blocktype[BT_conveyor_90_left ][FACE_up] = 64;
   for (i=0; i < 4; ++i)
      tex1_for_blocktype[BT_conveyor_ramp_up_low+i][FACE_up] = 22;

   set_blocktype_texture(BT_ore_eater, 17); tex1_for_blocktype[BT_ore_eater][FACE_east] = 23;
   set_blocktype_texture(BT_ore_drill, 17); tex1_for_blocktype[BT_ore_drill][FACE_east] = 24;

   geom_for_blocktype[BT_ore_drill] = STBVOX_MAKE_GEOMETRY(STBVOX_GEOM_solid, 0, 0);
   geom_for_blocktype[BT_ore_eater] = STBVOX_MAKE_GEOMETRY(STBVOX_GEOM_solid, 0, 0);
   geom_for_blocktype[BT_picker   ] = 0;

   for (i=0; i < 256; ++i)
      texture_scales[i] = 1.0f/4;// textures[i].scale;
   texture_scales[21] = 1.0f;
   texture_scales[0] = texture_scales[16] = texture_scales[32] = texture_scales[48] = 1.0f;
}



// proc gen mesh
#define GEN_CHUNK_CACHE_X_LOG2    4
#define GEN_CHUNK_CACHE_Y_LOG2    4
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
   uint8 rotate  [GEN_CHUNK_SIZE_Y][GEN_CHUNK_SIZE_X][Z_SEGMENT_SIZE];
} gen_chunk_partial;

enum
{
   REF_queue,
   REF_cache,
   REF_creation,
   REF_mesh_chunk_status,

   REF__count
};

struct st_gen_chunk
{
   int ref_count;
   gen_chunk_partial partial  [NUM_Z_SEGMENTS];
   unsigned char     non_empty[NUM_Z_SEGMENTS];

   int augmented_ref_count[REF__count];
};

typedef struct
{
   int chunk_x, chunk_y;
   gen_chunk *chunk;
} gen_chunk_cache;

enum estatus
{
   CHUNK_STATUS_invalid,
   CHUNK_STATUS_empty_chunk_set,
   CHUNK_STATUS_nonempty_chunk_set,
   CHUNK_STATUS_processing
};

typedef struct
{
   int chunk_x, chunk_y;
   int in_new_list;
   enum estatus status;
   Bool rebuild_chunks;
   chunk_set cs;
   int chunk_set_valid[4][4];
} mesh_chunk_status;

#define MESH_STATUS_X   64
#define MESH_STATUS_Y   64

       mesh_chunk      *c_mesh_cache[C_MESH_CHUNK_CACHE_Y][C_MESH_CHUNK_CACHE_X];

static mesh_chunk_status mesh_status[MESH_STATUS_Y][MESH_STATUS_X][2];
static gen_chunk_cache    gen_cache [   GEN_CHUNK_CACHE_Y][   GEN_CHUNK_CACHE_X];

static mesh_chunk_status *get_chunk_status(int x, int y, Bool needs_triangles)
{
   int cx = C_MESH_CHUNK_X_FOR_WORLD_X(x);
   int cy = C_MESH_CHUNK_Y_FOR_WORLD_Y(y);
   mesh_chunk_status *mcs = &mesh_status[cy & (MESH_STATUS_Y-1)][cx & (MESH_STATUS_X-1)][needs_triangles];
   if (mcs->chunk_x == cx && mcs->chunk_y == cy)
      return mcs;
   return NULL;
}

void release_gen_chunk(gen_chunk *gc, int type);

static void abandon_mesh_chunk_status(mesh_chunk_status *mcs)
{
   int i,j;
   //ods("Abandon %d,%d", mcs->chunk_x, mcs->chunk_y);
   for (j=0; j < 4; ++j)
      for (i=0; i < 4; ++i)
         if (mcs->chunk_set_valid[j][i])
            release_gen_chunk(mcs->cs.chunk[j][i], REF_mesh_chunk_status);
         else
            assert(mcs->cs.chunk[j][i] == 0);
   mcs->status = CHUNK_STATUS_invalid;
   memset(mcs->chunk_set_valid, 0, sizeof(mcs->chunk_set_valid));
   memset(&mcs->cs, 0, sizeof(mcs->cs));
}

void finished_caching_mesh_chunk(int x, int y, Bool needs_triangles)
{
   // render knows about it now, so we don't need to cache it anymore
   mesh_chunk_status *mcs;
   SDL_LockMutex(manager_mutex);
   mcs = get_chunk_status(x, y, needs_triangles);
   if (mcs != NULL) {
      int i,j;
      assert(mcs->status == CHUNK_STATUS_processing);
      for (j=0; j < 4; ++j)
         for (i=0; i < 4; ++i)
            assert(mcs->chunk_set_valid[j][i] == 0);
      //ods("Invalidating %d,%d", mcs->chunk_x, mcs->chunk_y);
      mcs->status = CHUNK_STATUS_invalid;
   }
   SDL_UnlockMutex(manager_mutex);
}

static mesh_chunk_status *get_chunk_status_alloc(int x, int y, Bool needs_triangles, Bool rebuild_chunks)
{
   int cx = C_MESH_CHUNK_X_FOR_WORLD_X(x);
   int cy = C_MESH_CHUNK_Y_FOR_WORLD_Y(y);
   mesh_chunk_status *mcs = &mesh_status[cy & (MESH_STATUS_Y-1)][cx & (MESH_STATUS_X-1)][needs_triangles];
   if (mcs->chunk_x == cx && mcs->chunk_y == cy && mcs->status != CHUNK_STATUS_invalid)
      return mcs;

   if (mcs->status == CHUNK_STATUS_nonempty_chunk_set)
      abandon_mesh_chunk_status(mcs);

   memset(mcs, 0, sizeof(*mcs));
   mcs->status = CHUNK_STATUS_empty_chunk_set;
   mcs->chunk_x = cx;
   mcs->chunk_y = cy;
   mcs->rebuild_chunks = rebuild_chunks;

   if (0 && !needs_triangles) {
      int i;
      for (i=0; i < 4; ++i) {
         mcs->chunk_set_valid[0][i] = mcs->chunk_set_valid[3][i] = True;
         mcs->chunk_set_valid[i][0] = mcs->chunk_set_valid[i][3] = True;
      }
   }

   return mcs;
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
   procgen_in_progress[slot_y][slot_x].in_use = True;
}

void end_procgen(int x, int y)
{
   int cx = GEN_CHUNK_X_FOR_WORLD_X(x);
   int cy = GEN_CHUNK_Y_FOR_WORLD_Y(y);
   int slot_x = cx & (IN_PROGRESS_CACHE_SIZE-1);
   int slot_y = cy & (IN_PROGRESS_CACHE_SIZE-1);
   procgen_in_progress[slot_y][slot_x].x = x;
   procgen_in_progress[slot_y][slot_x].y = y;
   procgen_in_progress[slot_y][slot_x].in_use = False;
}

int can_start_procgen(int x, int y)
{
   int cx = GEN_CHUNK_X_FOR_WORLD_X(x);
   int cy = GEN_CHUNK_Y_FOR_WORLD_Y(y);
   int slot_x = cx & (IN_PROGRESS_CACHE_SIZE-1);
   int slot_y = cy & (IN_PROGRESS_CACHE_SIZE-1);
   return !procgen_in_progress[slot_y][slot_x].in_use;
}

mesh_chunk *get_mesh_chunk_for_coord(int x, int y)
{
   int cx = C_MESH_CHUNK_X_FOR_WORLD_X(x);
   int cy = C_MESH_CHUNK_Y_FOR_WORLD_Y(y);
   mesh_chunk *mc = c_mesh_cache[cy & (C_MESH_CHUNK_CACHE_Y-1)][cx & (C_MESH_CHUNK_CACHE_X-1)];

   if (mc && mc->chunk_x == cx && mc->chunk_y == cy)
      return mc;
   else
      return NULL;
}

void free_mesh_chunk_physics(mesh_chunk *mc)
{
   if (mc->allocs) {
      int i;
      for (i=0; i < stb_arr_len(mc->allocs); ++i)
         free(mc->allocs[i]);
      stb_arr_free(mc->allocs);
   }
}

void free_mesh_chunk(mesh_chunk *mc)
{
   glDeleteTextures(1, &mc->fbuf_tex);
   glDeleteBuffersARB(1, &mc->vbuf);
   glDeleteBuffersARB(1, &mc->fbuf);

   free_mesh_chunk_physics(mc);
   free(mc);
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

void init_chunk_caches(void)
{
   int i,j;
   for (j=0; j < C_MESH_CHUNK_CACHE_Y; ++j) {
      for (i=0; i < C_MESH_CHUNK_CACHE_X; ++i) {
         c_mesh_cache[j][i] = 0;
      }
   }

   for (j=0; j < GEN_CHUNK_CACHE_Y; ++j) {
      for (i=0; i < GEN_CHUNK_CACHE_X; ++i) {
         assert(gen_cache[j][i].chunk == NULL);
         gen_cache[j][i].chunk = NULL;
         gen_cache[j][i].chunk_x = i+1;
      }
   }

   free_physics_cache();
}

#ifdef TRACK_ALLOCATED_GEN_CHUNKS_IN_GLOBAL_TABLE
#define MAX_GENCHUNK   16000

gen_chunk *gen_chunk_table[MAX_GENCHUNK];
int num_gen_chunk;

static void monitor_create_gen_chunk(gen_chunk *gc)
{
   int i;
   SDL_LockMutex(ref_count_mutex);
   assert(num_gen_chunk < MAX_GENCHUNK);
   for (i=0; i < num_gen_chunk; ++i)
      assert(gen_chunk_table[i] != gc);
   gen_chunk_table[num_gen_chunk++] = gc;
   SDL_UnlockMutex(ref_count_mutex);
}

static void monitor_delete_gen_chunk(gen_chunk *gc)
{
   int i;
   SDL_LockMutex(ref_count_mutex);
   for (i=0; i < num_gen_chunk; ++i)
      if (gen_chunk_table[i] == gc)
         break;
   assert(i < num_gen_chunk);
   gen_chunk_table[i] = gen_chunk_table[--num_gen_chunk]; // delete by swapping down last
   SDL_UnlockMutex(ref_count_mutex);
}

static void monitor_refcount_gen_chunk(gen_chunk *gc)
{
   int i;
   SDL_LockMutex(ref_count_mutex);
   for (i=0; i < num_gen_chunk; ++i)
      if (gen_chunk_table[i] == gc)
         break;
   assert(i < num_gen_chunk);
   SDL_UnlockMutex(ref_count_mutex);
}
#else
#define monitor_delete_gen_chunk(gc)
#define monitor_create_gen_chunk(gc)
#define monitor_refcount_gen_chunk(gc)
#endif


int num_gen_chunk_alloc;

void release_gen_chunk(gen_chunk *gc, int type)
{
   assert(gc->ref_count > 0);
   monitor_refcount_gen_chunk(gc);
   SDL_LockMutex(ref_count_mutex);
   --gc->ref_count;
   --gc->augmented_ref_count[type];
   if (gc->ref_count == 0) {
      SDL_UnlockMutex(ref_count_mutex);
      monitor_delete_gen_chunk(gc);
      free(gc);
      --num_gen_chunk_alloc;
   } else
      SDL_UnlockMutex(ref_count_mutex);
}

void add_ref_count(gen_chunk *gc, int type)
{
   monitor_refcount_gen_chunk(gc);
   SDL_LockMutex(ref_count_mutex);
   ++gc->ref_count;
   ++gc->augmented_ref_count[type];
   SDL_UnlockMutex(ref_count_mutex);
}

void free_gen_chunk(gen_chunk_cache *gcc, int type)
{
   release_gen_chunk(gcc->chunk, type);
   gcc->chunk = NULL;
}

void free_chunk_caches(void)
{
   int i,j;
   for (j=0; j < C_MESH_CHUNK_CACHE_Y; ++j) {
      for (i=0; i < C_MESH_CHUNK_CACHE_X; ++i) {
         if (c_mesh_cache[j][i] != NULL)
            free_mesh_chunk(c_mesh_cache[j][i]);
      }
   }

   for (j=0; j < GEN_CHUNK_CACHE_Y; ++j) {
      for (i=0; i < GEN_CHUNK_CACHE_X; ++i) {
         if (gen_cache[j][i].chunk != NULL)
            release_gen_chunk(gen_cache[j][i].chunk, REF_cache);
      }
   }

   free_physics_cache();
}


#define MIN_GROUND 32
#define AVG_GROUND 72

float octave_multiplier[8] =
{
   1.01f,
   1.03f,
   1.052f,
   1.021f,
   1.0057f,
   1.111f,
   1.089f,
   1.157f,
};

float compute_height_field(int x, int y, float weight)
{
   float ht = AVG_GROUND;
#if 1
   int o;
   for (o=0; o < 8; ++o) {
      float scale = (float) (1 << o) * octave_multiplier[o];
      float ns = stb_perlin_noise3(x/scale, y/scale, o*2.0f, 256,256,256), heavier;
      float sign = (ns < 0 ? -1.0f : 1.0f);
      ns = (float) fabs(ns);
      heavier = ns*ns*ns*ns*4*sign;
      ht += scale/2 * stb_lerp(weight, ns, heavier) / 2;
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

void build_column(gen_chunk *gc, int x, int y, int z0, int z1, int bt)
{
   int z;
   if (x >= 0 && x < GEN_CHUNK_SIZE_X && y >= 0 && y < GEN_CHUNK_SIZE_Y)
      for (z=z0; z < z1; ++z)
         gc->partial[z >> Z_SEGMENT_SIZE_LOG2].block[y][x][z & 15] = bt;
}

void build_disk(gen_chunk *gc, float x, float y, int z, float radius, int bt)
{
   int ix,iy;
   int x0,y0,x1,y1;
   x0 = (int) floor(x - radius);
   y0 = (int) floor(y - radius);
   x1 = (int) ceil(x + radius);
   y1 = (int) ceil(y + radius);
   for (ix=x0; ix <= x1; ++ix)
      for (iy=y0; iy <= y1; ++iy)
         if (ix >= 0 && ix < GEN_CHUNK_SIZE_X && iy >= 0 && iy < GEN_CHUNK_SIZE_Y)
            if ((ix-x)*(ix-x)+(iy-y)*(iy-y) <= radius*radius)
               gc->partial[z >> Z_SEGMENT_SIZE_LOG2].block[iy][ix][z & 15] = bt;
}

//  m = minimum spacing between trees
//  e = edge-generation "radius"
//  c = corner-generation "radius"
//
//  2e >= m
//
//  distance between (c,e) and (e,c) must be >= m
//
//  (e-c)^2 + (c-e)^2 >= m^2
//  2(c-e)^2 >= m^2
//  sqrt(2)*(c-e) >= m
//  abs(c-e) >= m / sqrt(2) > m / 1.5 = m*2/3

//  m = 6
//
//  (c-e) >= 4
//  c=8, e=4, m=6
//
//  c=7,e=3,m=6

typedef struct
{
   int x,y;
   int type;
} tree_location;

#define MAX_TREES_PER_AREA   6
typedef struct
{
   int num_trees;
   tree_location trees[MAX_TREES_PER_AREA];
} tree_area_data;

static int random_5[16] = { -2,-1,0,1,2, -2,1,0,1,2, -2,1,0,1,2, 0 };

#define TREE_CORNER_SIZE   7
#define TREE_MIN_SPACING   6
#define TREE_EDGE_SIZE     3

static void add_tree(tree_area_data *tad, int x, int y, int type)
{
   tree_location tl = { x,y, type };
   tad->trees[tad->num_trees++] = tl;
}

Bool collides_raw(tree_location *trees, int n, int x, int y)
{
   int i;
   for (i=0; i < n; ++i) {
      int sx = x - trees[i].x;
      int sy = y - trees[i].y;
      if (abs(sx) < TREE_MIN_SPACING && abs(sy) < TREE_MIN_SPACING)
         if (sx*sx + sy*sy < TREE_MIN_SPACING * TREE_MIN_SPACING)
            return True;
   }
   return False;
}

Bool collides(tree_area_data *tad, int x, int y)
{
   return collides_raw(tad->trees, tad->num_trees, x, y);
}

tree_area_data generate_trees_for_corner(int x, int y)
{
   int i,j;
   tree_area_data tad;
   uint32 n = flat_noise32_weak(x,y,8938);
   int bx = x + (n & 7) - 6;
   int by = y + ((n >> 3) & 7) - 6;
   tad.num_trees = 0;

   n = flat_noise32_weak(x,y, 319491);
   for (j=0; j < 2; ++j) {
      for (i=0; i < 2; ++i) {
         int tx = bx + i*8;
         int ty = by + j*8;
         tx += random_5[n & 15];
         n >>= 4;
         ty += random_5[n & 15];
         n >>= 4;
         if (tx >= bx - TREE_CORNER_SIZE && tx < bx + TREE_CORNER_SIZE &&
             ty >= by - TREE_CORNER_SIZE && ty < by + TREE_CORNER_SIZE)
            if (!collides(&tad, tx, ty))
               add_tree(&tad, tx,ty, BT_marble);
      }
   }
   return tad;
}

tree_area_data generate_trees_for_horizontal_edge_raw(int ex, int ey, tree_area_data *c1, tree_area_data *c2, uint32 seed)
{
   uint32 n = flat_noise32_weak(ex,ey,seed);
   tree_area_data tad;
   // TREE_CORNER_SIZE <= x < GEN_CHUNK_SIZE_X - TREE_CORNER_SIZE
   int x_left, x_right, y_left, y_right, x_mid, y_mid;

   x_left = ex + TREE_CORNER_SIZE + (n & 3); n >>= 2;
   y_left = ey - 2 + (n & 3); n >>= 2;
   while (collides(c1, x_left, y_left))
      x_left += 2;
   assert(x_left <= ex + TREE_CORNER_SIZE + TREE_MIN_SPACING + 2);
   //     x_left <= ex + 14

   x_right = ex + GEN_CHUNK_SIZE_X - TREE_CORNER_SIZE + (n & 3) - 4; n >>= 2;
   y_right = ey - 2 + (n & 3); n >>= 2;
   while (collides(c2, x_right, y_right))
      x_right -= 2;
   assert(x_right >= ex + GEN_CHUNK_SIZE_X - TREE_CORNER_SIZE - TREE_MIN_SPACING - 2);

   if (x_right > x_left + TREE_MIN_SPACING) {
      tad.num_trees = 0;
      x_mid = (int) stb_linear_remap((n&255),0,255, x_left, x_right); n >>= 8;
      y_mid = ey - 2 + (n & 3); n >>= 2;
      add_tree(&tad, x_mid, y_mid, BT_gravel);
   } else {
      x_mid = (x_left + x_right) + ((n & 7) - 4); n >>= 3;
      y_mid = ey - 2 + (n & 3); n >>= 2;

      if (x_mid < x_left + TREE_MIN_SPACING)
         x_mid = x_left + TREE_MIN_SPACING;
      if (x_mid > x_right - TREE_MIN_SPACING)
         x_mid = x_right - TREE_MIN_SPACING;

      tad.num_trees = 0;
      add_tree(&tad, x_left, y_left, BT_gravel);
      if (x_mid >= x_left + TREE_MIN_SPACING)
         add_tree(&tad, x_mid, y_mid, BT_gravel);
      add_tree(&tad, x_right, y_right, BT_gravel);
   }
   return tad;
}

tree_area_data generate_trees_for_horizontal_edge(int ex, int ey, tree_area_data *c1, tree_area_data *c2)
{
   return generate_trees_for_horizontal_edge_raw(ex,ey, c1,c2, 8938);
}

void swap_tree_coords(tree_area_data *tad)
{
   int i;
   for (i=0; i < tad->num_trees; ++i) {
      int x = tad->trees[i].x;
      tad->trees[i].x = tad->trees[i].y;
      tad->trees[i].y = x;
   }
}

tree_area_data generate_trees_for_vertical_edge(int ex, int ey, tree_area_data *c1, tree_area_data *c2)
{
   tree_area_data tad;
   swap_tree_coords(c1);
   swap_tree_coords(c2);
   tad = generate_trees_for_horizontal_edge_raw(ey,ex, c1,c2, 9387);
   swap_tree_coords(c2);
   swap_tree_coords(c1);

   swap_tree_coords(&tad);
   return tad;
}

static int add_trees(tree_location trees[], int num, tree_area_data *tad)
{
   int i;
   for (i=0; i < tad->num_trees; ++i)
      trees[num++] = tad->trees[i];
   return num;
}

// returns number of trees
int generate_trees_for_chunk(tree_location trees[], int x, int y, int limit)
{
   int i;
   int num=0;
   tree_area_data c00,c01,c10,c11;
   tree_area_data ee,en,ew,es;

   c00 = generate_trees_for_corner(x                   , y                   );
   c10 = generate_trees_for_corner(x + GEN_CHUNK_SIZE_X, y                   );
   c01 = generate_trees_for_corner(x                   , y + GEN_CHUNK_SIZE_Y);
   c11 = generate_trees_for_corner(x + GEN_CHUNK_SIZE_X, y + GEN_CHUNK_SIZE_Y);

   en = generate_trees_for_horizontal_edge(x, y                   , &c00, &c10);
   es = generate_trees_for_horizontal_edge(x, y + GEN_CHUNK_SIZE_Y, &c01, &c11);
   ew = generate_trees_for_vertical_edge  (x,                    y, &c00, &c01);
   ee = generate_trees_for_vertical_edge  (x + GEN_CHUNK_SIZE_X, y, &c10, &c11);

   num = add_trees(trees, num, &c00);
   num = add_trees(trees, num, &c10);
   num = add_trees(trees, num, &c01);
   num = add_trees(trees, num, &c11);
   num = add_trees(trees, num, &ee);
   num = add_trees(trees, num, &en);
   num = add_trees(trees, num, &ew);
   num = add_trees(trees, num, &es);

   for (i=0; i < 40; ++i) {
      int r = flat_noise32_weak(x, y, 838383+i);
      int tx = r & (GEN_CHUNK_SIZE_X-1);
      int ty = (r>>16) & (GEN_CHUNK_SIZE_Y-1);

      if (   tx >= TREE_EDGE_SIZE && tx < GEN_CHUNK_SIZE_X - TREE_EDGE_SIZE
          && ty >= TREE_EDGE_SIZE && ty < GEN_CHUNK_SIZE_Y - TREE_EDGE_SIZE) {
         tx += x;
         ty += y;
         if (!collides_raw(trees, num, tx, ty)) {
            trees[num].x = tx;
            trees[num].y = ty;
            trees[num].type = BT_wood;
            ++num;
            if (num >= limit)
               break;
         }
      }
   }

   return num;
}

#define MAX_TREES_PER_CHUNK  100

static float tree_shape_function(float pos)
{
   if (pos < 0.25) {
      pos *= 4;
      return 1.0f-(float)sqrt(1.0f-pos*pos);
   } else {
      pos = stb_linear_remap(pos, 0.25f, 1.0f, 0.0f, 1.0f);
      return 1.0f-pos;
   }
}


// 4K x 4K x 256 = > 2^12 * 2^12 * 2^8  => 2^32  /  2^10 => 2^22 4M => 4MB

// 32x32x4

#define EDIT_CHUNK_Z_COUNT_LOG2   2
#define EDIT_CHUNK_Z_COUNT        (1 << EDIT_CHUNK_Z_COUNT_LOG2)
#define NUM_EDIT_CHUNK_Z_SEG      (MAX_Z_POW2CEIL / EDIT_CHUNK_Z_COUNT)

typedef struct
{
   uint8 type;
   uint8 rotate:2;
   uint8 unused:6;
} block_change;

typedef struct
{
   int x,y,z;
   block_change blocks[EDIT_CHUNK_Z_COUNT][GEN_CHUNK_SIZE_Y][GEN_CHUNK_SIZE_X];
} edit_chunk;

edit_chunk **edit_chunks;

void save_edits(void)
{
   FILE *f = fopen("savegame.dat", "wb");
   int i;
   for (i=0; i < stb_arr_len(edit_chunks); ++i) {
      fwrite(edit_chunks[i], sizeof(edit_chunk), 1, f);
   }
   fclose(f);
}

void load_edits(void)
{
   FILE *f = fopen("savegame.dat", "rb");
   if (f) {
      while (!feof(f)) {
         edit_chunk ec;
         if (fread(&ec, sizeof(edit_chunk), 1, f)) {
            int i,j,k;
            for (k=0; k < EDIT_CHUNK_Z_COUNT; ++k) {
               for (j=0; j < GEN_CHUNK_SIZE_Y; ++j) {
                  for (i=0; i < GEN_CHUNK_SIZE_X; ++i) {
                     if (ec.blocks[k][j][i].type != BT_no_change) {
                        change_block(ec.x*GEN_CHUNK_SIZE_X+i, ec.y*GEN_CHUNK_SIZE_Y+j, ec.z*EDIT_CHUNK_Z_COUNT+k, ec.blocks[k][j][i].type, ec.blocks[k][j][i].rotate);
                     }
                  }
               }
            }
         
         }
      }
      fclose(f);
   }
}

static edit_chunk *get_edit_chunk_for_coord_raw(int x, int y, int z, Bool alloc)
{
   int cx = x >> GEN_CHUNK_SIZE_X_LOG2;
   int cy = y >> GEN_CHUNK_SIZE_Y_LOG2;
   int cz = z >> EDIT_CHUNK_Z_COUNT_LOG2;
   int i;
   for (i=0; i < stb_arr_len(edit_chunks); ++i) {
      edit_chunk *e = edit_chunks[i];
      if (e->x == cx && e->y == cy && e->z == cz)
         return e;
   }
   if (alloc) {
      edit_chunk *e = malloc(sizeof(*e));
      e->x = cx;
      e->y = cy;
      e->z = cz;
      memset(e->blocks, BT_no_change, sizeof(e->blocks));
      stb_arr_push(edit_chunks, e);
      return e;
   } else
      return NULL;
}

static edit_chunk *get_edit_chunk      (int x, int y, int z) { return get_edit_chunk_for_coord_raw(x,y,z,False); }
static edit_chunk *get_edit_chunk_alloc(int x, int y, int z) { return get_edit_chunk_for_coord_raw(x,y,z,True ); }

int get_block(int x, int y, int z)
{
   edit_chunk *e = get_edit_chunk(x,y,z);   
   if (e == NULL)
      return 0;
   x &= (GEN_CHUNK_SIZE_X-1);
   y &= (GEN_CHUNK_SIZE_Y-1);
   z &= (EDIT_CHUNK_Z_COUNT-1);
   return e->blocks[z][y][x].type;
}

int get_block_rot(int x, int y, int z)
{
   edit_chunk *e = get_edit_chunk(x,y,z);   
   if (e == NULL)
      return 0;
   x &= (GEN_CHUNK_SIZE_X-1);
   y &= (GEN_CHUNK_SIZE_Y-1);
   z &= (EDIT_CHUNK_Z_COUNT-1);
   return e->blocks[z][y][x].rotate;
}

void change_block(int x, int y, int z, int type, int rot)
{
   edit_chunk *e = get_edit_chunk_alloc(x,y,z);
   int sx = x & (GEN_CHUNK_SIZE_X-1);
   int sy = y & (GEN_CHUNK_SIZE_Y-1);
   int sz = z & (EDIT_CHUNK_Z_COUNT-1);

   if (e->blocks[sz][sy][sx].type != type || e->blocks[sz][sy][sx].rotate != rot) {
      e->blocks[sz][sy][sx].type   = type;
      e->blocks[sz][sy][sx].rotate = rot;

      force_update_for_block(x,y,z);

      update_physics_cache(x,y,z,type,rot);
      logistics_update_block(x,y,z,type,rot);
   }
}

gen_chunk *generate_chunk(int x, int y)
{
   int z_seg;
   int i,j,z;
   int ground_top = 0;
   gen_chunk *gc = malloc(sizeof(*gc));
   int num_trees;
   tree_location trees[MAX_TREES_PER_CHUNK];
   float height_lerp[GEN_CHUNK_SIZE_Y+8][GEN_CHUNK_SIZE_X+8];
   float height_field[GEN_CHUNK_SIZE_Y+8][GEN_CHUNK_SIZE_X+8];
   float height_ore[GEN_CHUNK_SIZE_Y+8][GEN_CHUNK_SIZE_X+8];
   int height_field_int[GEN_CHUNK_SIZE_Y+8][GEN_CHUNK_SIZE_X+8];
   assert(gc);

   ++num_gen_chunk_alloc;

   memset(gc->non_empty, 0, sizeof(gc->non_empty));
   memset(gc->non_empty, 1, (ground_top+Z_SEGMENT_SIZE-1)>>Z_SEGMENT_SIZE_LOG2);

   // @TODO: compute non_empty based on below updates
   // @OPTIMIZE: change mesh builder to check non_empty

   for (j=-4; j < GEN_CHUNK_SIZE_Y+4; ++j)
      for (i=-4; i < GEN_CHUNK_SIZE_X+4; ++i) {
         float ht;
         float weight = (float) stb_linear_remap(stb_perlin_noise3((x+i)/256.0f,(y+j)/256.0f,100,256,256,256), -1.5, 1.5, -4.0f, 5.0f);
         weight = stb_clamp(weight,0,1);
         height_lerp[j+4][i+4] = weight;
         ht = compute_height_field(x+i,y+j, weight);
         assert(ht >= 4);
         height_field[j+4][i+4] = ht;
         height_field_int[j+4][i+4] = (int) height_field[j+4][i+4];
         ground_top = stb_max(ground_top, height_field_int[j+4][i+4]);
         height_ore[j+4][i+4] = stb_perlin_noise3((float)(x+i)+0.5f,(float)(y+j)+0.5f,(float)(x*77+y*31)+0.5f,256,256,256);
      }

   for (z_seg=0; z_seg < NUM_Z_SEGMENTS; ++z_seg) {
      int z0 = z_seg * Z_SEGMENT_SIZE;
      gen_chunk_partial *gcp = &gc->partial[z_seg];
      for (j=0; j < GEN_CHUNK_SIZE_Y; ++j) {
         for (i=0; i < GEN_CHUNK_SIZE_X; ++i) {
            int bt;
            int ht = height_field_int[j+4][i+4];

            int z_stone = stb_clamp(ht-2-z0, 0, Z_SEGMENT_SIZE);
            int z_limit = stb_clamp(ht-z0, 0, Z_SEGMENT_SIZE);

            if (height_lerp[j][i] < 0.5)
               bt = BT_grass;
            else
               bt = BT_sand;
            if (ht > AVG_GROUND+14)
               bt = BT_gravel;

            if (height_ore[j+4][i+4] < -0.5) {
               bt = BT_stone;
               z_limit = stb_clamp(ht+1-z0, 0, Z_SEGMENT_SIZE);
            }

            //bt = (int) stb_lerp(height_lerp[j][i], BT_sand, BT_marble+0.99f);
            assert(z_limit >= 0 && Z_SEGMENT_SIZE - z_limit >= 0);

            memset(&gcp->rotate[j][i][0], 0, Z_SEGMENT_SIZE);
            if (z_limit > 0) {
               memset(&gcp->block[j][i][   0   ], BT_stone, z_stone);
               memset(&gcp->block[j][i][z_stone],  bt     , z_limit-z_stone);
            }
            memset(&gcp->block[j][i][z_limit],     BT_empty    , Z_SEGMENT_SIZE - z_limit);
         }
      }
   }

   for (j=0; j < GEN_CHUNK_SIZE_Y; ++j) {
      for (i=0; i < GEN_CHUNK_SIZE_X; ++i) {
         int ht = height_field_int[j+4][i+4];
         if (height_ore[j+4][i+4] < -0.5) {
            logistics_record_ore(x+i,y+j, ht-2, ht+1, BT_stone);
         }
      }
   }

   num_trees = generate_trees_for_chunk(trees, x, y, MAX_TREES_PER_CHUNK);
   for (i=0; i < num_trees; ++i) {
      int bx = trees[i].x;
      int by = trees[i].y;
      int tx = bx - x;
      int ty = by - y; 
      if (tx >= -4 && tx < GEN_CHUNK_SIZE_X+4 &&
          ty >= -4 && ty < GEN_CHUNK_SIZE_Y+4) {
         if (height_lerp[ty+4][tx+4] < 0.5) {
            uint32 r = flat_noise32_strong(bx, by, 8989);
            int ht = height_field_int[ty+4][tx+4];
            int tree_height = (r % 4) + 8;
            int leaf_bottom = ht + (tree_height>>1);
            int leaf_top = ht + tree_height + (ht + tree_height - leaf_bottom);
            float px,py, dx,dy;
            float tree_width = 3.5;

            leaf_top += 3;
            leaf_bottom += 3;

            r >>= 2;
            tree_width = stb_linear_remap((r&15), 0, 15, 2.5f, 3.9f); r >>= 4;
            dx = (float) ((int) (r & 255) - 128); r >>= 8;
            dy = (float) ((int) (r & 255) - 128); r >>= 8;

            assert(dx >= -128 && dx <= 127);
            assert(dy >= -128 && dy <= 127);

            dx /= 512.0f;
            dy /= 512.0f;

            assert(dx >= -0.25f && dx <= 0.25);
            assert(dy >= -0.25f && dy <= 0.25);

            #if 0
            dx = 0.15f;
            dy = 0.15f;
            #endif


            px = (float) tx;
            py = (float) ty;
            for (z=leaf_bottom; z <= leaf_top; ++z) {
               float radius = tree_width * tree_shape_function(stb_linear_remap(z, leaf_bottom, leaf_top, 0.05f, 0.95f));
               build_disk(gc, px,py, z, radius, BT_leaves);
               px += dx;
               py += dy;
            }

            #if 0
            build_column(gc, tx+1, ty, leaf_ht+2, ht+tree_height+1, BT_leaves);
            build_column(gc, tx, ty+1, leaf_ht+2, ht+tree_height+1, BT_leaves);
            build_column(gc, tx-1, ty, leaf_ht+2, ht+tree_height+1, BT_leaves);
            build_column(gc, tx, ty-1, leaf_ht+2, ht+tree_height+1, BT_leaves);
            #endif

            build_column(gc, tx, ty, ht, ht+tree_height, BT_wood);
         }
      }
   }

   #if 0
   for (j=0; j < GEN_CHUNK_SIZE_Y; ++j)
      for (i=0; i < GEN_CHUNK_SIZE_X; ++i)
         if (i == 0 || i == GEN_CHUNK_SIZE_X-1 || j == 0 || j == GEN_CHUNK_SIZE_Y-1)
            build_column(gc, i,j, height_field_int[j+4][i+4], height_field_int[j+4][i+4]+1, BT_sand);
   #endif

   #if 0
   for (j = -4; j < GEN_CHUNK_SIZE_Y+4; j += 8) {
      for (i = -4; i < GEN_CHUNK_SIZE_X+4; i += 8) {
         uint32 r = flat_noise32_strong(x+i, y+j, 8989);
         int xoff = (r % 5) - 2;
         int yoff = ((r>>8) % 5) - 2;
         if (i+xoff >= -4 && i+xoff < GEN_CHUNK_SIZE_X+4 &&
             j+yoff >= -4 && j+yoff < GEN_CHUNK_SIZE_Y+4) {
            int tx = i+xoff, ty = j+yoff;
            if (height_lerp[ty][tx] < 0.5) {
               int ht = height_field_int[ty][tx];
               int tree_height = (r % 6) + 4;
               int leaf_ht = ht + (tree_height>>1);
               build_column(gc, tx, ty, ht, ht+tree_height, BT_wood);

               build_column(gc, tx+1, ty, leaf_ht+2, ht+tree_height+1, BT_leaves);
               build_column(gc, tx, ty+1, leaf_ht+2, ht+tree_height+1, BT_leaves);
               build_column(gc, tx-1, ty, leaf_ht+2, ht+tree_height+1, BT_leaves);
               build_column(gc, tx, ty-1, leaf_ht+2, ht+tree_height+1, BT_leaves);
            }
         }
      }
   }
   #endif

   for (z=0; z < NUM_EDIT_CHUNK_Z_SEG; ++z) {
      edit_chunk *e = get_edit_chunk(x, y, z * EDIT_CHUNK_Z_COUNT);
      if (e) {
         int k;
         for (k=0; k < EDIT_CHUNK_Z_COUNT; ++k)
            for (j=0; j < GEN_CHUNK_SIZE_Y; ++j)
               for (i=0; i < GEN_CHUNK_SIZE_X; ++i)
                  if (e->blocks[k][j][i].type != BT_no_change) {
                     int ht = z*EDIT_CHUNK_Z_COUNT + k;
                     int zs = ht >> Z_SEGMENT_SIZE_LOG2;
                     int zoff = ht & (Z_SEGMENT_SIZE-1);
                     gc->partial[zs].block[j][i][zoff] = e->blocks[k][j][i].type;
                     gc->partial[zs].rotate[j][i][zoff] = e->blocks[k][j][i].rotate;
                  }
      }
   }

   // compute lighting for every block by weighted average of neighbors

   // loop through every partial chunk separately

   for (z_seg=0; z_seg < NUM_Z_SEGMENTS; ++z_seg) {
      gen_chunk_partial *gcp = &gc->partial[z_seg];
      for (j=0; j < GEN_CHUNK_SIZE_Y; ++j)
         for (i=0; i < GEN_CHUNK_SIZE_X; ++i)
            for (z=0; z < Z_SEGMENT_SIZE; ++z) {
               static uint8 convert_rot[4] = { 0,3,2,1 };
               int type = gcp->block[j][i][z];
               int is_solid = type != 0 && type != BT_picker;
               gcp->lighting[j][i][z] = STBVOX_MAKE_LIGHTING_EXT(is_solid ? 0 : 255, convert_rot[gcp->rotate[j][i][z]]);
            }
   }

   gc->ref_count = 0;
   memset(gc->augmented_ref_count, 0, sizeof(gc->augmented_ref_count));
   monitor_create_gen_chunk(gc);

   return gc;
}

gen_chunk_cache * put_chunk_in_cache(int x, int y, gen_chunk *gc)
{
   int cx = GEN_CHUNK_X_FOR_WORLD_X(x);
   int cy = GEN_CHUNK_Y_FOR_WORLD_Y(y);
   int slot_x = cx & (GEN_CHUNK_CACHE_X-1);
   int slot_y = cy & (GEN_CHUNK_CACHE_Y-1);
   gen_chunk_cache *gcc = &gen_cache[slot_y][slot_x];
   assert(gcc->chunk_x != cx || gcc->chunk_y != cy || gcc->chunk == NULL);
   if (gcc->chunk != NULL)
      free_gen_chunk(gcc, REF_cache);
   gcc->chunk_x = cx;
   gcc->chunk_y = cy;
   gcc->chunk = gc;
   add_ref_count(gc, REF_cache);
   return gcc;
}

void invalidate_gen_chunk_cache(int x, int y)
{
   int cx = GEN_CHUNK_X_FOR_WORLD_X(x);
   int cy = GEN_CHUNK_Y_FOR_WORLD_Y(y);
   int slot_x = cx & (GEN_CHUNK_CACHE_X-1);
   int slot_y = cy & (GEN_CHUNK_CACHE_Y-1);
   gen_chunk_cache *gcc = &gen_cache[slot_y][slot_x];
   if (gcc->chunk_x == cx && gcc->chunk_y == cy) {
      if (gcc->chunk != NULL) {
         free_gen_chunk(gcc, REF_cache);
         gcc->chunk_x = cx+1;
         gcc->chunk_y = 0;
         gcc->chunk = NULL;
      }
   }
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
   uint8 *vertex_build_buffer;
   uint8 *face_buffer;
   uint8 segment_blocktype[66][66][18];
   uint8 segment_lighting[66][66][18];
} build_data;

void copy_chunk_set_to_segment(chunk_set *chunks, int z_seg, build_data *bd)
{
   int j,i,x,y,a;
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
            uint8 *bt = &bd->segment_blocktype[y+y_off][x_off][0];
            uint8 *lt = &bd->segment_lighting [y+y_off][x_off][0];
            for (x=x0; x < x1; ++x) {
               memcpy(bt + sizeof(bd->segment_blocktype[0][0])*x, &gcp->block   [y][x][0], 16);
               memcpy(lt + sizeof(bd->segment_lighting [0][0])*x, &gcp->lighting[y][x][0], 16);
               for (a=0; a < 16; ++a)
                  if ((bt + sizeof(bd->segment_blocktype[0][0])*x)[a] == BT_picker)
                     (bt + sizeof(bd->segment_blocktype[0][0])*x)[a] = BT_empty;
            }
         }
      }
}

void *arena_alloc(arena_chunk ***chunks, size_t size, size_t arena_chunk_size)
{
   void *p;
   arena_chunk **ac = *chunks;
   arena_chunk *cur = ac == NULL ? NULL : stb_arr_last(ac);
   if (stb_arr_len(ac) == 0)
      goto alloc;
   if (cur->in_use + size > cur->capacity) {
      ac=ac;
     alloc:
      arena_chunk_size = stb_max(size, arena_chunk_size);
      cur = malloc(arena_chunk_size + sizeof(arena_chunk)-1);
      cur->capacity = arena_chunk_size;
      cur->in_use = 0;
      stb_arr_push(ac, cur);  
      *chunks = ac;
      cur = stb_arr_last(ac);
   } else {
      cur = cur;
   }

   assert(cur->in_use + size <= cur->capacity);
   p = cur->data + cur->in_use;
   cur->in_use += size;
   return p;
}

// release unused memory from end of last allocation
void arena_release(arena_chunk **chunks, size_t size)
{
   stb_arr_last(chunks)->in_use -= size;
}

void arena_free_all(arena_chunk **chunks)
{
   int i;
   for (i=0; i < stb_arr_len(chunks); ++i)
      free(chunks[i]);
   stb_arr_free(chunks);
}

phys_chunk_run *build_phys_column(mesh_chunk *mc, gen_chunk *gc, int x, int y)
{
   phys_chunk_run *pr = arena_alloc(&mc->allocs, MAX_Z*2, 8192);
   int z,data_off=0;
   int run_length = 1;
   for (z=1; z < MAX_Z; ++z) {
      int prev_type = gc->partial[(z-1)>>4].block[y][x][(z-1)&15] != BT_empty;
      int type = gc->partial[z>>4].block[y][x][z&15] != BT_empty;
      if (type != prev_type) {
         pr[data_off].type = prev_type;
         pr[data_off].length = run_length;
         run_length = 1;
         ++data_off;
      } else   
         ++run_length;
   }
   pr[data_off].type = (gc->partial[(z-1)>>4].block[y][x][(z-1)&15] != BT_empty);
   pr[data_off].length = run_length;
   ++data_off;

   #if _DEBUG
   {
      int rl_sum=0;
      int i;
      for (i=0; i < data_off; ++i)
         rl_sum += pr[i].length;
      assert(rl_sum == MAX_Z);
   }
   #endif

   arena_release(mc->allocs, (MAX_Z-data_off)*sizeof(*pr));
   return pr;
}

void build_phys_chunk(mesh_chunk *mc, chunk_set *chunks, int wx, int wy)
{
   int j,i,x,y;
   mc->allocs = NULL;
   for (j=1; j <= 2; ++j) {
      for (i=1; i <= 2; ++i) {
         int x_off = (i-1) * GEN_CHUNK_SIZE_X;
         int y_off = (j-1) * GEN_CHUNK_SIZE_Y;

         for (y=0; y < GEN_CHUNK_SIZE_Y; ++y)
            for (x=0; x < GEN_CHUNK_SIZE_X; ++x) {
               mc->pc.column[y_off+y][x_off+x] = build_phys_column(mc, chunks->chunk[j][i], x,y);
            }
      }
   }
}

void update_phys_chunk(mesh_chunk *mc, int ex, int ey, int ez, int type)
{
   int i,j;
   arena_chunk **new_chunks = NULL;
   for (j=0; j < MESH_CHUNK_SIZE_Y; ++j) {
      for (i=0; i < MESH_CHUNK_SIZE_X; ++i) {
         phys_chunk_run *pr_old = mc->pc.column[j][i];
         phys_chunk_run *pr_new = arena_alloc(&new_chunks, MAX_Z*2, 8192);
         if (i != ex || j != ey) {
            // copy existing data unchanged
            int len = 0, off=0;
            while (len < MAX_Z) {
               pr_new[off] = pr_old[off];
               len += pr_old[off].length;
               ++off;
            }
            assert(len == MAX_Z);
            arena_release(new_chunks, (MAX_Z-off)*sizeof(*pr_new));
         } else {
            // build new RLE data with 'type' updated
            int z,data_off;
            int run_length;
            uint8 types[MAX_Z];
            int len = 0, off=0;
            while (len < MAX_Z) {
               assert(len+pr_old[off].length <= MAX_Z);
               memset(types+len, pr_old[off].type, pr_old[off].length);
               len += pr_old[off].length;
               ++off;
            }
            types[ez] = type;

            data_off = 0;
            run_length = 1;
            for (z=1; z < MAX_Z; ++z) {
               int prev_type = types[z-1] != BT_empty;
               int type = types[z] != BT_empty;
               if (type != prev_type) {
                  pr_new[data_off].type = prev_type;
                  pr_new[data_off].length = run_length;
                  run_length = 1;
                  ++data_off;
               } else   
                  ++run_length;
            }
            pr_new[data_off].type = types[z-1] != BT_empty;
            pr_new[data_off].length = run_length;
            ++data_off;
            arena_release(new_chunks, (MAX_Z-data_off)*sizeof(*pr_new));
         }
         mc->pc.column[j][i] = pr_new;
      }
   }
   arena_free_all(mc->allocs);
   mc->allocs = new_chunks;
}

void generate_mesh_for_chunk_set(stbvox_mesh_maker *mm, mesh_chunk *mc, vec3i world_coord, chunk_set *chunks, size_t build_size, build_data *bd)
{
   int a,b,z;

   stbvox_input_description *map;

   assert((world_coord.x & (MESH_CHUNK_SIZE_X-1)) == 0);
   assert((world_coord.y & (MESH_CHUNK_SIZE_Y-1)) == 0);

   mc->chunk_x = (world_coord.x >> MESH_CHUNK_SIZE_X_LOG2);
   mc->chunk_y = (world_coord.y >> MESH_CHUNK_SIZE_Y_LOG2);

   stbvox_set_input_stride(mm, 18, 66*18);

   //build_phys_chunk(mc, chunks, world_coord.x, world_coord.y);
   mc->allocs = NULL;

   map = stbvox_get_input_description(mm);
   map->block_tex1_face = tex1_for_blocktype;
   //map->block_tex2_face = tex2_for_blocktype;
   //static unsigned char color_for_blocktype[256][6];
   map->block_geometry = geom_for_blocktype;
   map->block_vheight = vheight_for_blocktype;

   //stbvox_reset_buffers(mm);
   stbvox_set_buffer(mm, 0, 0, bd->vertex_build_buffer, build_size);
   stbvox_set_buffer(mm, 0, 1, bd->face_buffer , build_size>>2);

   map->blocktype = &bd->segment_blocktype[1][1][1]; // this is (0,0,0), but we need to be able to query off the edges
   map->lighting  = &bd->segment_lighting[1][1][1];
   //map->rotate    = &bd->segment_rotate[1][1][1];

   // fill in the top two rows of the buffer
   for (b=0; b < 66; ++b) {
      for (a=0; a < 66; ++a) {
         bd->segment_blocktype[b][a][16] = 0;
         bd->segment_blocktype[b][a][17] = 0;
         bd->segment_lighting [b][a][16] = 255;
         bd->segment_lighting [b][a][17] = 255;
      }
   }

   z = 256-16;  // @TODO use MAX_Z and Z_SEGMENT_SIZE

   for (; z >= 0; z -= 16)  // @TODO use MAX_Z and Z_SEGMENT_SIZE
   {
      int z0 = z;
      int z1 = z+16;
      if (z1 == 256) z1 = 255;  // @TODO use MAX_Z and Z_SEGMENT_SIZE

      copy_chunk_set_to_segment(chunks, z >> 4, bd);   // @TODO use MAX_Z and Z_SEGMENT_SIZE

      map->blocktype = &bd->segment_blocktype[1][1][1-z];
      map->lighting  = &bd->segment_lighting[1][1][1-z];
      //map->rotate    = &bd->segment_rotate[1][1][1-z];

      {
         stbvox_set_input_range(mm, 0,0,z0, 64,64,z1);
         stbvox_set_default_mesh(mm, 0);
         stbvox_make_mesh(mm);
      }

      // copy the bottom two rows of data up to the top
      for (b=0; b < 66; ++b) {
         for (a=0; a < 66; ++a) {
            bd->segment_blocktype[b][a][16] = bd->segment_blocktype[b][a][0];
            bd->segment_blocktype[b][a][17] = bd->segment_blocktype[b][a][1];
            bd->segment_lighting [b][a][16] = bd->segment_lighting [b][a][0];
            bd->segment_lighting [b][a][17] = bd->segment_lighting [b][a][1];
            //bd->segment_rotate   [b][a][16] = bd->segment_rotate   [b][a][0];
            //bd->segment_rotate   [b][a][17] = bd->segment_rotate   [b][a][1];
         }
      }
   }

   stbvox_set_mesh_coordinates(mm, world_coord.x, world_coord.y, world_coord.z+1);
   stbvox_get_transform(mm, mc->transform);

   stbvox_set_input_range(mm, 0,0,0, 64,64,255);
   stbvox_get_bounds(mm, mc->bounds);

   mc->num_quads = stbvox_get_quad_count(mm, 0);
}

void set_mesh_chunk_for_coord(int x, int y, mesh_chunk *new_mc)
{
   int cx = C_MESH_CHUNK_X_FOR_WORLD_X(x);
   int slot_x = cx & (C_MESH_CHUNK_CACHE_X-1);
   int cy = C_MESH_CHUNK_Y_FOR_WORLD_Y(y);
   int slot_y = cy & (C_MESH_CHUNK_CACHE_Y-1);
   mesh_chunk *mc = c_mesh_cache[slot_y][slot_x];
   if (mc && mc->vbuf) {
      free_mesh_chunk(mc);
   }

   c_mesh_cache[slot_y][slot_x] = new_mc;
   c_mesh_cache[slot_y][slot_x]->dirty = False;
}

void force_update_for_block_raw(int x, int y, int z)
{
   int cx = C_MESH_CHUNK_X_FOR_WORLD_X(x);
   int slot_x = cx & (C_MESH_CHUNK_CACHE_X-1);
   int cy = C_MESH_CHUNK_Y_FOR_WORLD_Y(y);
   int slot_y = cy & (C_MESH_CHUNK_CACHE_Y-1);
   mesh_chunk *mc = c_mesh_cache[slot_y][slot_x];
   if (mc && mc->chunk_x == cx && mc->chunk_y == cy)
      mc->dirty = True;
}

void force_update_for_block(int x, int y, int z)
{
   force_update_for_block_raw(x,y,z);
   // @OPTIMIZE: only update them if needed
   force_update_for_block_raw(x+1,y,z);
   force_update_for_block_raw(x-1,y,z);
   force_update_for_block_raw(x,y+1,z);
   force_update_for_block_raw(x,y-1,z);
}

static uint8 vertex_build_buffer[16*1024*1024];
static uint8 face_buffer[4*1024*1024];

static build_data static_build_data;

mesh_chunk *build_mesh_chunk_for_coord(int x, int y)
{
   int cx = C_MESH_CHUNK_X_FOR_WORLD_X(x);
   int slot_x = cx & (C_MESH_CHUNK_CACHE_X-1);
   int cy = C_MESH_CHUNK_Y_FOR_WORLD_Y(y);
   int slot_y = cy & (C_MESH_CHUNK_CACHE_Y-1);
   mesh_chunk *mc = c_mesh_cache[slot_y][slot_x];

   if (mc->vbuf) {
      assert(mc->chunk_x != cx || mc->chunk_y != cy);
      free_mesh_chunk(mc);
      mc = NULL;
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
      static_build_data.vertex_build_buffer = vertex_build_buffer;
      static_build_data.face_buffer = face_buffer;
      generate_mesh_for_chunk_set(&mm, mc, wc, &cs, sizeof(vertex_build_buffer), &static_build_data);
      upload_mesh(mc, vertex_build_buffer, face_buffer);
   }

   return mc;
}

#define MAX_MESH_WORKERS 3

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

#define MAX_GEN_CHUNKS     256

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

typedef struct
{
   SDL_mutex *mutex;
   unsigned char padding[64];
   int  head,tail;
   int count;
   size_t itemsize;
   uint8 *data;
} threadsafe_queue;

void init_threadsafe_queue(threadsafe_queue *tq, int count, size_t size)
{
   tq->mutex = SDL_CreateMutex();
   tq->head = tq->tail = 0;
   tq->count = count;
   tq->itemsize = size;
   tq->data = malloc(size * count); // @TODO cache-align
}

int add_to_queue(threadsafe_queue *tq, void *item)
{
   int retval = False;
   SDL_LockMutex(tq->mutex);
   if ((tq->head+1) % tq->count != tq->tail) {
      memcpy(tq->data + tq->itemsize * tq->head, item, tq->itemsize);
      tq->head = (tq->head+1) % tq->count;
      retval = True;
   }
   SDL_UnlockMutex(tq->mutex);

   return retval;
}

#if 0
int get_from_queue(threadsafe_queue *tq, void *item)
{
   int retval=False;
   SDL_LockMutex(tq->mutex);
   if (tq->head != tq->tail) {
      memcpy(item, tq->data + tq->itemsize * tq->tail, tq->itemsize);
      ++tq->tail;
      if (tq->tail >= tq->count)
         tq->tail = 0;
      retval = True;
   }
   SDL_UnlockMutex(tq->mutex);
   return retval;
}
#endif

int get_from_queue_nonblocking(threadsafe_queue *tq, void *item)
{
   int retval=False;
   if (SDL_TryLockMutex(tq->mutex) == SDL_MUTEX_TIMEDOUT)
      return False;
   if (tq->head != tq->tail) {
      memcpy(item, tq->data + tq->itemsize * tq->tail, tq->itemsize);
      ++tq->tail;
      if (tq->tail >= tq->count)
         tq->tail = 0;
      retval = True;
   }
   SDL_UnlockMutex(tq->mutex);
   return retval;
}

SDL_mutex *ore_update_mutex;
SDL_mutex *requested_mesh_mutex;

requested_mesh rm_list1[MAX_BUILT_MESHES], rm_list2[MAX_BUILT_MESHES], rm_list3[MAX_BUILT_MESHES];

requested_mesh *current_processing_meshes = rm_list1, *most_recent_renderer_meshes = rm_list2, *empty_meshes = rm_list3;
volatile Bool is_renderer_meshes_valid;

requested_mesh *get_requested_mesh_alternate(void)
{
   return empty_meshes;
}

void swap_requested_meshes(void)
{
   requested_mesh *temp;
   SDL_LockMutex(swap_renderer_request_mutex);
   temp = most_recent_renderer_meshes;
   most_recent_renderer_meshes = empty_meshes;
   empty_meshes = temp;
   is_renderer_meshes_valid = True;
   SDL_UnlockMutex(swap_renderer_request_mutex);
   waiter_wake(&manager_monitor);
}

Bool swap_current_processing(void)
{
   Bool swapped = False;
   SDL_LockMutex(swap_renderer_request_mutex);
   if (is_renderer_meshes_valid) {
      requested_mesh *temp;
      temp = most_recent_renderer_meshes;
      most_recent_renderer_meshes = current_processing_meshes;
      current_processing_meshes = temp;
      is_renderer_meshes_valid = False;
      swapped = True;
   }
   SDL_UnlockMutex(swap_renderer_request_mutex);
   return swapped;
}

threadsafe_queue built_meshes;
int get_next_built_mesh(built_mesh *bm)
{
   return get_from_queue_nonblocking(&built_meshes, bm);
}

Bool get_next_task(task *t, int thread_id)
{
   Bool found_task_flag = True;
   int i,n;

   SDL_LockMutex(manager_mutex);

   SDL_LockMutex(swap_renderer_request_mutex);
   if (swap_current_processing()) {
      // delete any requests that are already in the mesh-building stage by rebuilding list in-place
      n=0;
      for (i=0; i < MAX_BUILT_MESHES; ++i) {
         requested_mesh *rm = &current_processing_meshes[i];
         mesh_chunk_status *mcs;
         if (rm->state == RMS_invalid)
            break;

         mcs = get_chunk_status_alloc(rm->x, rm->y, rm->needs_triangles, rm->rebuild_chunks);

         if (mcs->status == CHUNK_STATUS_processing)
            ; // delete
         else {
            mcs->in_new_list = True;
            current_processing_meshes[n++] = *rm;
         }
      }
      if (n < MAX_BUILT_MESHES)
         current_processing_meshes[n].state = RMS_invalid;

      // scan old list for things that we were building but aren't in the list now,
      // and discard them
      for (i=0; i < MAX_BUILT_MESHES; ++i) {
         mesh_chunk_status *mcs;
         requested_mesh *rm = &most_recent_renderer_meshes[i];
         if (rm->state == RMS_invalid)
            break;
         mcs = get_chunk_status(rm->x, rm->y, rm->needs_triangles);
         if (mcs && !mcs->in_new_list) {
            abandon_mesh_chunk_status(mcs);
         }
      }

      // clear flags set in first loop
      for (i=0; i < MAX_BUILT_MESHES; ++i) {
         requested_mesh *rm = &current_processing_meshes[i];
         mesh_chunk_status *mcs;
         if (rm->state == RMS_invalid)
            break;
         mcs = get_chunk_status(rm->x, rm->y, rm->needs_triangles);
         mcs->in_new_list = False;
      }
   }
   SDL_UnlockMutex(swap_renderer_request_mutex);

   for (i=0; i < MAX_BUILT_MESHES; ++i) {
      requested_mesh *rm = &current_processing_meshes[i];

      if (rm->state == RMS_requested) {
         int k,j;
         int valid_chunks=0;
         mesh_chunk_status *mcs = get_chunk_status(rm->x, rm->y, rm->needs_triangles);

         // if the mesh is being rebuilt, we have to invalidate all the corresponding gen chunks--
         // @TODO is to do this properly with timestamps
         if (mcs->rebuild_chunks) {
            for (k=0; k < 4; ++k) {
               for (j=0; j < 4; ++j) {
                  int cx = rm->x + (j-1) * GEN_CHUNK_SIZE_X;
                  int cy = rm->y + (k-1) * GEN_CHUNK_SIZE_Y;
                  invalidate_gen_chunk_cache(cx, cy);
               }
            }
            mcs->rebuild_chunks = False;
         }

         // go through all the chunks and see if they're created, and if not, request them
         if (mcs->status == CHUNK_STATUS_processing)
            continue;

         for (k=0; k < 4; ++k) {
            for (j=0; j < 4; ++j) {
               if (!mcs->chunk_set_valid[k][j]) {
                  int cx = rm->x + (j-1) * GEN_CHUNK_SIZE_X;
                  int cy = rm->y + (k-1) * GEN_CHUNK_SIZE_Y;
                  gen_chunk_cache *gcc = get_gen_chunk_cache_for_coord(cx, cy);
                  if (gcc) {
                     assert(mcs->status == CHUNK_STATUS_nonempty_chunk_set || mcs->status == CHUNK_STATUS_empty_chunk_set);
                     mcs->status = CHUNK_STATUS_nonempty_chunk_set;
                     mcs->cs.chunk[k][j] = gcc->chunk;
                     mcs->chunk_set_valid[k][j] = True;
                     assert(gcc->chunk != 0);
                     add_ref_count(gcc->chunk, REF_mesh_chunk_status);
                     // @TODO
                     //++valid_chunks;
                  } else {
                     if (is_in_progress(cx, cy)) {
                        // it's already in progress, so do nothing
                     } else if (can_start_procgen(cx, cy)) {
                        start_procgen(cx,cy);
                        t->world_x = cx;
                        t->world_y = cy;
                        t->task_type = JOB_generate_terrain;
                        goto found_task;
                     }
                  }
               } else {
                  ++valid_chunks;
               }
            }
         }

         if (valid_chunks == 16) {
            int i,j;
            //mesh_chunk_status *mcs = get_chunk_status(rm->x, rm->y, rm->needs_triangles);
            #if 0
            if (rm->needs_triangles)
               ods("[%d] Accepting: %d,%d %p %d", thread_id, C_MESH_CHUNK_X_FOR_WORLD_X(rm->x), C_MESH_CHUNK_Y_FOR_WORLD_Y(rm->y), mcs, mcs->status);
            #endif
            for (j=0; j < 4; ++j)
               for (i=0; i < 4; ++i)
                  assert(mcs->chunk_set_valid[j][i]);
            if (rm->needs_triangles) {
               t->cs = mcs->cs;
               memset(mcs->chunk_set_valid, 0, sizeof(mcs->chunk_set_valid));
               memset(&mcs->cs, 0, sizeof(mcs->cs));
               mcs->status = CHUNK_STATUS_processing;
               assert(rm->state == RMS_requested);
               rm->state = RMS_invalid;
               t->task_type = JOB_build_mesh;
               t->world_x = rm->x;
               t->world_y = rm->y;
               goto found_task;
            } else {
               mesh_chunk *mc = malloc(sizeof(*mc));
               built_mesh out_mesh;
               memset(mc, 0, sizeof(*mc));
               mc->chunk_x = rm->x >> MESH_CHUNK_SIZE_X_LOG2;
               mc->chunk_y = rm->y >> MESH_CHUNK_SIZE_Y_LOG2;

               build_phys_chunk(mc, &mcs->cs, rm->x, rm->y);

               memset(mcs->chunk_set_valid, 0, sizeof(mcs->chunk_set_valid));
               for (i=0; i < 16; ++i)
                  release_gen_chunk(mcs->cs.chunk[0][i], REF_mesh_chunk_status);
               memset(&mcs->cs, 0, sizeof(mcs->cs));

               mcs->status = CHUNK_STATUS_processing;

               out_mesh.vertex_build_buffer = 0;
               out_mesh.face_buffer  = 0;
               out_mesh.mc = mc;
               out_mesh.mc->has_triangles = False;
               if (!add_to_queue(&built_meshes, &out_mesh))
                  free(out_mesh.mc);
               assert(rm->state == RMS_requested);
               rm->state = RMS_invalid;
            }
         }
      }
   }

   found_task_flag = False;

  found_task:
   SDL_UnlockMutex(manager_mutex);
   return found_task_flag;
}

int num_mesh_workers;

volatile int num_workers_running;
volatile Bool stop_worker_flag;

enum
{
   PROF_waiting,
   PROF_managing,
   PROF_processing,

   PROF__count
};

typedef struct
{
   double time_spent[PROF__count];
   int times_executed[PROF__count];
} thread_timing_data;

thread_timing_data thread_prof[MAX_MESH_WORKERS];

void add_time(int thread_id, Uint64 time, int mode)
{
   assert(thread_id >= 0 && thread_id < MAX_MESH_WORKERS);
   thread_prof[thread_id].time_spent[mode]     += time / (double) SDL_GetPerformanceFrequency();
   thread_prof[thread_id].times_executed[mode] += 1;
}

int mesh_worker_handler(void *data)
{
   int thread_id;
   build_data *bd = malloc(sizeof(*bd));
   size_t vert_buf_size = 16*1024*1024;
   bd->vertex_build_buffer = malloc(vert_buf_size);
   bd->face_buffer = malloc(4*1024*1024);

   SDL_LockMutex(ref_count_mutex);
   thread_id = num_workers_running++;
   SDL_UnlockMutex(ref_count_mutex);

   for(;;) {
      Uint64 start, end;
      Bool did_wait = False;
      task t;

      while (!get_next_task(&t, thread_id)) {
         if (stop_worker_flag) {
            SDL_LockMutex(ref_count_mutex);
            --num_workers_running;
            SDL_UnlockMutex(ref_count_mutex);
            return 0;
         }

         waiter_wait(&manager_monitor);
         did_wait = True;
      }

      start = SDL_GetPerformanceCounter();

      // if we got some work, but we HAD been idle, then wake up the
      // next thread so it can see if it also has work to do
      waiter_wake(&manager_monitor);

      #if 0
      if ((int) data == 2) {
         ods("Task %d\n", t.task_type);
      }
      #endif

      switch (t.task_type) {
         case JOB_build_mesh: {
            stbvox_mesh_maker mm;
            mesh_chunk *mc = malloc(sizeof(*mc));
            built_mesh out_mesh;
            vec3i wc = { t.world_x, t.world_y, 0 };

            memset(mc, 0, sizeof(mc));

            stbvox_init_mesh_maker(&mm);

            mc->chunk_x = t.world_x >> MESH_CHUNK_SIZE_X_LOG2;
            mc->chunk_y = t.world_y >> MESH_CHUNK_SIZE_Y_LOG2;

            generate_mesh_for_chunk_set(&mm, mc, wc, &t.cs, vert_buf_size, bd);

            out_mesh.vertex_build_buffer = malloc(mc->num_quads * 16);
            out_mesh.face_buffer  = malloc(mc->num_quads *  4);

            memcpy(out_mesh.vertex_build_buffer, bd->vertex_build_buffer, mc->num_quads * 16);
            memcpy(out_mesh.face_buffer, bd->face_buffer, mc->num_quads * 4);
            SDL_LockMutex(manager_mutex);
            {
               int i;
               #if 0
               mesh_chunk_status *mcs = get_chunk_status(t.world_x, t.world_y, False);
               if (mcs)
                  ods("[%d] Added built mesh %d,%d %p %d", thread_id, mc->chunk_x, mc->chunk_y, mcs, mcs->status);
               else
                  ods("[%d] Mesh chunk status for %d,%d is missing", thread_id, mc->chunk_x, mc->chunk_y);
               #endif
               for (i=0; i < 16; ++i)
                  release_gen_chunk(t.cs.chunk[0][i], REF_mesh_chunk_status);
            }
            SDL_UnlockMutex(manager_mutex);
            out_mesh.mc = mc;
            out_mesh.mc->has_triangles = True;
            if (!add_to_queue(&built_meshes, &out_mesh)) {
               //ods("Failed to add %d,%d", mc->chunk_x, mc->chunk_y);
               free(out_mesh.vertex_build_buffer);
               free(out_mesh.face_buffer);
               free(out_mesh.mc);
            }
            break;
         }
         case JOB_generate_terrain: {
            gen_chunk *gc;
            gc = generate_chunk(t.world_x, t.world_y);
            assert(gc->ref_count == 0);
            SDL_LockMutex(manager_mutex);
            put_chunk_in_cache(t.world_x, t.world_y, gc);
            end_procgen(t.world_x, t.world_y);
            SDL_UnlockMutex(manager_mutex);
            break;
         }
      }
      end = SDL_GetPerformanceCounter();
      add_time(thread_id, end-start, PROF_processing);
   }
}

Uint64 sim_start;

void stop_manager(void)
{
   double time;
   Uint64 sim_end;
   int i;

   stop_worker_flag = True;

   while (num_workers_running) {
      waiter_wake(&manager_monitor);
      SDL_Delay(10);
   }
   sim_end = SDL_GetPerformanceCounter();

   free_chunk_caches();

   for (i=0; i < MESH_STATUS_X*MESH_STATUS_Y*2; ++i) {
      if (mesh_status[0][0][i].status != CHUNK_STATUS_invalid)
         abandon_mesh_chunk_status(&mesh_status[0][0][i]);
   }

   time = (sim_end - sim_start) / (double) SDL_GetPerformanceFrequency();

   for (i=0; i < num_mesh_workers; ++i) {
      ods("#%d: Process %5.2lf -- Manage %5.2lf -- Wait: %5.2lf\n", i, thread_prof[i].time_spent[PROF_processing]/time,
                                                                       thread_prof[i].time_spent[PROF_managing]/time,
                                                                       thread_prof[i].time_spent[PROF_waiting]/time);
   }
}

void init_mesh_build_threads(void)
{
   int i;
   int num_proc = SDL_GetCPUCount();

   init_WakeableWaiter(&manager_monitor);
   init_threadsafe_queue(&built_meshes  , MAX_BUILT_MESHES, sizeof(built_mesh));
   ref_count_mutex = SDL_CreateMutex();
   ore_update_mutex = SDL_CreateMutex();

   swap_renderer_request_mutex = SDL_CreateMutex();
   manager_mutex = SDL_CreateMutex();

   if (num_proc > 6)
      num_mesh_workers = num_proc-3;
   else if (num_proc > 4)
      num_mesh_workers = 4;
   else 
      num_mesh_workers = num_proc-1;

   if (num_mesh_workers > MAX_MESH_WORKERS)
      num_mesh_workers = MAX_MESH_WORKERS;

   if (num_mesh_workers < 1)
      num_mesh_workers = 1;

   //num_mesh_workers = MAX_MESH_WORKERS;

   sim_start = SDL_GetPerformanceCounter();

   for (i=0; i < num_mesh_workers; ++i)
      SDL_CreateThread(mesh_worker_handler, "mesh worker", (void *) i);
}
