#ifndef INCLUDE_OBBG_DATA_H
#define INCLUDE_OBBG_DATA_H

// block types
enum
{
   BT_empty,
   BT_solid,
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

mesh_chunk      mesh_cache[MESH_CHUNK_CACHE_Y][MESH_CHUNK_CACHE_X];

#endif
