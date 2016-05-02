#ifndef INCLUDE_OBBG_DATA_H
#define INCLUDE_OBBG_DATA_H

#define MINIMIZE_MEMORY

#include "stb.h"

#include <stdlib.h>
#include <malloc.h>

#include "stb_leakcheck_sdl.h"

#include <assert.h>

typedef int Bool;
#define True   1
#define False  0

#ifdef _MSC_VER
   typedef __int64 int64;
   typedef unsigned __int64 uint64;
#else
   typedef long long int64;
   typedef unsigned long long uint64;
#endif

#define MAX_Z                    255

#ifdef MINIMIZE_MEMORY
#define VIEW_DIST_LOG2            9
#else
#define VIEW_DIST_LOG2            11
#endif

#define C_CACHE_RADIUS_LOG2        (VIEW_DIST_LOG2+1)

#define MESH_CHUNK_SIZE_X_LOG2    6
#define MESH_CHUNK_SIZE_Y_LOG2    6
#define MESH_CHUNK_SIZE_X        (1 << MESH_CHUNK_SIZE_X_LOG2)
#define MESH_CHUNK_SIZE_Y        (1 << MESH_CHUNK_SIZE_Y_LOG2)

#define C_MESH_CHUNK_CACHE_X_LOG2  (C_CACHE_RADIUS_LOG2 - MESH_CHUNK_SIZE_X_LOG2)
#define C_MESH_CHUNK_CACHE_Y_LOG2  (C_CACHE_RADIUS_LOG2 - MESH_CHUNK_SIZE_Y_LOG2)
#define C_MESH_CHUNK_CACHE_X       (1 << C_MESH_CHUNK_CACHE_X_LOG2)
#define C_MESH_CHUNK_CACHE_Y       (1 << C_MESH_CHUNK_CACHE_Y_LOG2)

#define C_MESH_CHUNK_X_FOR_WORLD_X(x)   ((x) >> MESH_CHUNK_SIZE_X_LOG2)
#define C_MESH_CHUNK_Y_FOR_WORLD_Y(y)   ((y) >> MESH_CHUNK_SIZE_Y_LOG2)

typedef struct
{
   int x,y,z;
} vec3i;

typedef struct
{
   float x,y,z;
} vec;

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

// physics types
enum
{
   PT_empty,
   PT_solid,
};

typedef struct
{
   uint8 type;
   uint8 length;
} phys_chunk_run;

typedef struct
{
   phys_chunk_run *column[MESH_CHUNK_SIZE_Y][MESH_CHUNK_SIZE_X];
} phys_chunk;

typedef struct
{
   size_t capacity;
   size_t in_use;
   unsigned char data[1];  
} arena_chunk;

typedef struct
{
   int chunk_x, chunk_y;

   int vbuf_size, fbuf_size;

   float transform[3][3];
   float bounds[2][3];

   unsigned int vbuf;
   unsigned int fbuf, fbuf_tex;
   int num_quads;
   int has_triangles;

   phys_chunk pc;
   arena_chunk **allocs;
} mesh_chunk;

typedef int32 objid;

typedef struct
{
   vec position;
   vec ang;
   vec velocity;

   uint32 valid;
   uint32 sent_fields; // used only as part of the server version history
} object;

typedef struct
{
   uint16 buttons;
   uint16 view_x, view_z;
   uint8  client_frame;
   uint8  reserved;
} player_net_controls; // 8 bytes    (8 * 12 + 28 = 124 bytes/packet, * 30/sec = 3720 bytes/sec)

// buffer 12 inputs at 30hz = 124*30 = 3720 bytes/sec   - can lose 5 packets in a row, imposes an extra 33ms latency
// buffer  5 inputs at 60hz =  68*60 = 4080 bytes/sec   - can lose 4 packets in a row

typedef struct
{
   uint16 buttons;
   Bool flying;   // this becomes a button or a command
   vec   ang;     // this should be redundant to view_x,view_z
} player_controls;

extern player_controls client_player_input;

#ifdef IS_64_BIT  // this doesn't exist yet and should be a different symbol
#define MAX_OBJECTS        32768
#define PLAYER_OBJECT_MAX   1024
#else
#define MAX_OBJECTS         8192
#define PLAYER_OBJECT_MAX    256
#endif

extern object obj[MAX_OBJECTS];

extern float texture_scales[256];

extern mesh_chunk     *c_mesh_cache[C_MESH_CHUNK_CACHE_Y][C_MESH_CHUNK_CACHE_X];

extern objid player_id;
extern objid max_obj_id, max_player_id;
enum
{
   RMS_invalid,
   
   RMS_requested,

   //RMS_chunks_completed_waiting_for_meshing,
};


#define MAX_BUILT_MESHES   (16+16)

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
   Bool needs_triangles;
} requested_mesh;

extern int chunk_locations, chunks_considered, chunks_in_frustum;
extern int quads_considered, quads_rendered;
extern int chunk_storage_rendered, chunk_storage_considered, chunk_storage_total;
extern int view_dist_for_display;
extern int num_threads_active, num_meshes_started, num_meshes_uploaded;
extern unsigned char tex1_for_blocktype[256][6];

extern Bool is_server;
extern player_controls p_input[PLAYER_OBJECT_MAX];
#endif
