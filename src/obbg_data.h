#ifndef INCLUDE_OBBG_DATA_H
#define INCLUDE_OBBG_DATA_H

#include <assert.h>

typedef int Bool;
#define true   (0==0)
#define false  (0==1)

// block types
enum
{
   BT_empty,

   BT_sand,
   BT_grass,
   BT_gravel,
   BT_asphalt,
   BT_wood,
   BT_marble,
   BT_stone,
   BT_leaves,
};

typedef struct
{
   int chunk_x, chunk_y;

   int vbuf_size, fbuf_size;

   float transform[3][3];
   float bounds[2][3];

   unsigned int vbuf;
   unsigned int fbuf, fbuf_tex;
   int num_quads;
} mesh_chunk;

extern float texture_scales[256];

#define MAX_Z                    255

#define VIEW_DIST_LOG2            11
#define CACHE_RADIUS_LOG2        (VIEW_DIST_LOG2+1)

#define MESH_CHUNK_SIZE_X_LOG2    6
#define MESH_CHUNK_SIZE_Y_LOG2    6
#define MESH_CHUNK_SIZE_X        (1 << MESH_CHUNK_SIZE_X_LOG2)
#define MESH_CHUNK_SIZE_Y        (1 << MESH_CHUNK_SIZE_Y_LOG2)

#define MESH_CHUNK_CACHE_X_LOG2  (CACHE_RADIUS_LOG2 - MESH_CHUNK_SIZE_X_LOG2)
#define MESH_CHUNK_CACHE_Y_LOG2  (CACHE_RADIUS_LOG2 - MESH_CHUNK_SIZE_Y_LOG2)
#define MESH_CHUNK_CACHE_X       (1 << MESH_CHUNK_CACHE_X_LOG2)
#define MESH_CHUNK_CACHE_Y       (1 << MESH_CHUNK_CACHE_Y_LOG2)

#define MESH_CHUNK_X_FOR_WORLD_X(x)   ((x) >> MESH_CHUNK_SIZE_X_LOG2)
#define MESH_CHUNK_Y_FOR_WORLD_Y(y)   ((y) >> MESH_CHUNK_SIZE_Y_LOG2)

extern mesh_chunk      mesh_cache[MESH_CHUNK_CACHE_Y][MESH_CHUNK_CACHE_X];


enum
{
   RMS_invalid,
   
   RMS_requested,

   //RMS_chunks_completed_waiting_for_meshing,
};


#define MAX_BUILT_MESHES   256

typedef struct
{
   mesh_chunk *mc;
   uint8 *vertex_build_buffer; // malloc/free
   uint8 *face_buffer;  // malloc/free
} built_mesh;

typedef struct st_gen_chunk gen_chunk;

typedef struct
{
   gen_chunk *chunk[4][4];
} chunk_set;

typedef struct
{
   int x,y;
   int state;
} requested_mesh;

extern int chunk_locations, chunks_considered, chunks_in_frustum;
extern int quads_considered, quads_rendered;
extern int chunk_storage_rendered, chunk_storage_considered, chunk_storage_total;
extern int view_dist_for_display;
extern int num_threads_active, num_meshes_started, num_meshes_uploaded;
extern unsigned char tex1_for_blocktype[256][6];

#endif
