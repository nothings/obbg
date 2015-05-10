#define STB_GLEXT_DECLARE "glext_list.h"
#include "stb_gl.h"
#include "stb_image.h"
#include "stb_glprog.h"

#include "stb.h"
#include "sdl.h"
#include "sdl_thread.h"

#include <math.h>

#include "u_noise.h"
#include "obbg_data.h"

#define STBVOX_CONFIG_MODE  1

#define STBVOX_CONFIG_DISABLE_TEX2
#define STBVOX_CONFIG_OPENGL_MODELVIEW
//#define STBVOX_CONFIG_PREFER_TEXBUFFER
//#define STBVOX_CONFIG_LIGHTING_SIMPLE
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

typedef struct
{
   gen_chunk_partial partial  [NUM_Z_SEGMENTS];
   unsigned char     non_empty[NUM_Z_SEGMENTS];
} gen_chunk;

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

void free_gen_chunk(gen_chunk_cache *gcc)
{
   free(gcc->chunk); // @TODO
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
   int zs;
   int i,j,z;
   int ground_top = 0;
   gen_chunk *gc = malloc(sizeof(*gc));
   float height_field[GEN_CHUNK_SIZE_Y][GEN_CHUNK_SIZE_X];
   int height_field_int[GEN_CHUNK_SIZE_Y][GEN_CHUNK_SIZE_X];

   for (j=0; j < GEN_CHUNK_SIZE_Y; ++j)
      for (i=0; i < GEN_CHUNK_SIZE_X; ++i) {
         float ht = compute_height_field(x+i,y+j);
         height_field[j][i] = ht;
         height_field_int[j][i] = (int) height_field[j][i];
         ground_top = stb_max(ground_top, height_field_int[j][i]);
      }  

   for (z=0; z < 16; ++z) {
      memset(gc->partial[z].block, BT_empty, sizeof(gc->partial[z].block));
      memset(gc->partial[z].lighting, 255, sizeof(gc->partial[z].lighting));
   }

   zs = ground_top >> Z_SEGMENT_SIZE_LOG2;
   memset(gc->non_empty, 0, sizeof(gc->non_empty));
   memset(gc->non_empty, 1, (ground_top+Z_SEGMENT_SIZE-1)>>Z_SEGMENT_SIZE_LOG2);

   for (j=0; j < GEN_CHUNK_SIZE_Y; ++j)
      for (i=0; i < GEN_CHUNK_SIZE_X; ++i) {
         unsigned int val = fast_noise(x+i,y+j,3,777);
         val = (val*16)/65536;
         for (z=0; z < height_field_int[j][i]; ++z)
            gc->partial[z>>4].block[j][i][z&15] = (uint8) (BT_solid + val);
      }

   return gc;
}

gen_chunk *get_gen_chunk_for_coord(int x, int y)
{
   gen_chunk_cache *gcc = get_gen_chunk_cache_for_coord(x,y);
   if (gcc == NULL) {
      int cx = GEN_CHUNK_X_FOR_WORLD_X(x);
      int cy = GEN_CHUNK_Y_FOR_WORLD_Y(y);
      int slot_x = cx & (GEN_CHUNK_CACHE_X-1);
      int slot_y = cy & (GEN_CHUNK_CACHE_Y-1);
      gcc = &gen_cache[slot_y][slot_x];
      if (gcc->chunk != NULL)
         free_gen_chunk(gcc);
      gcc->chunk_x = cx;
      gcc->chunk_y = cy;
      gcc->chunk = generate_chunk(x, y);
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
   gen_chunk *chunk[4][4];
} chunk_set;

typedef struct
{
   int x,y,z;
} vec3i;

uint8 segment_blocktype[66][66][18];
uint8 segment_lighting[66][66][18];

void copy_chunk_set_to_segment(chunk_set *chunks, int z_seg)
{
   int j,i,x,y,z;
   for (j=0; j < 4; ++j)
      for (i=0; i < 4; ++i) {
         int x_off = (i-1) * GEN_CHUNK_SIZE_X + 1;
         int y_off = (j-1) * GEN_CHUNK_SIZE_Y + 1;

         int x0,y0,x1,y1;

         x0 = 0; x1 = GEN_CHUNK_SIZE_X;
         y0 = 0; y1 = GEN_CHUNK_SIZE_Y;

         if (x_off + x0 <  0) x0 = 0 - x_off;
         if (x_off + x1 > 66) x1 = 66 - x_off;
         if (y_off + y0 <  0) y0 = 0 - y_off;
         if (y_off + y1 > 66) y1 = 66 - y_off;

         for (y=y0; y < y1; ++y)
            for (x=x0; x < x1; ++x)
               for (z=0; z < 16; ++z) {
                  segment_blocktype[y+y_off][x+x_off][z] = chunks->chunk[j][i]->partial[z_seg].block[y][x][z];
                  segment_lighting[y+y_off][x+x_off][z] = chunks->chunk[j][i]->partial[z_seg].lighting[y][x][z];
               }
      }
}

void generate_mesh_for_chunk_set(stbvox_mesh_maker *mm, mesh_chunk *mc, vec3i world_coord, chunk_set *chunks, uint8 *build_buffer, size_t build_size, uint8 *face_buffer)
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
   stbvox_set_buffer(mm, 0, 0, build_buffer, build_size);
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

void upload_mesh(mesh_chunk *mc, uint8 *build_buffer, uint8 *face_buffer)
{
   glGenBuffersARB(1, &mc->vbuf);
   glBindBufferARB(GL_ARRAY_BUFFER_ARB, mc->vbuf);
   glBufferDataARB(GL_ARRAY_BUFFER_ARB, mc->num_quads*4*sizeof(uint32), build_buffer, GL_STATIC_DRAW_ARB);
   glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

   glGenBuffersARB(1, &mc->fbuf);
   glBindBufferARB(GL_TEXTURE_BUFFER_ARB, mc->fbuf);
   glBufferDataARB(GL_TEXTURE_BUFFER_ARB, mc->num_quads*sizeof(uint32), face_buffer , GL_STATIC_DRAW_ARB);
   glBindBufferARB(GL_TEXTURE_BUFFER_ARB, 0);

   glGenTextures(1, &mc->fbuf_tex);
   glBindTexture(GL_TEXTURE_BUFFER_ARB, mc->fbuf_tex);
   glTexBufferARB(GL_TEXTURE_BUFFER_ARB, GL_RGBA8UI, mc->fbuf);
   glBindTexture(GL_TEXTURE_BUFFER_ARB, 0);
}

static uint8 build_buffer[64*1024*1024];
static uint8 face_buffer[16*1024*1024];

mesh_chunk *build_mesh_chunk_for_coord(int x, int y)
{
   int cx = MESH_CHUNK_X_FOR_WORLD_X(x) & (MESH_CHUNK_CACHE_X-1);
   int cy = MESH_CHUNK_Y_FOR_WORLD_Y(y) & (MESH_CHUNK_CACHE_Y-1);
   mesh_chunk *mc = &mesh_cache[cy][cx];

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
      generate_mesh_for_chunk_set(&mm, mc, wc, &cs, build_buffer, sizeof(build_buffer), face_buffer);
      upload_mesh(mc, build_buffer, face_buffer);
   }

   return mc;
}
