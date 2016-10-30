#ifndef INCLUDE_OBBG_DATA_H
#define INCLUDE_OBBG_DATA_H

//#define MINIMIZE_MEMORY

#include "stb.h"
#include "stb_vec.h"

#include <stdlib.h>

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

#define MAX_MESH_WORKERS 3

#define MAX_Z                    255
#define MAX_Z_POW2CEIL           256

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

#define C_WORLD_X_FOR_MESH_CHUNK_X(x)   ((x) << MESH_CHUNK_SIZE_X_LOG2)
#define C_WORLD_Y_FOR_MESH_CHUNK_Y(y)   ((y) << MESH_CHUNK_SIZE_Y_LOG2)

#define GEN_CHUNK_SIZE_X_LOG2     5
#define GEN_CHUNK_SIZE_Y_LOG2     5
#define GEN_CHUNK_SIZE_X         (1 << GEN_CHUNK_SIZE_X_LOG2)
#define GEN_CHUNK_SIZE_Y         (1 << GEN_CHUNK_SIZE_Y_LOG2)

#define LOGI_CHUNK_SIZE_X_LOG2      GEN_CHUNK_SIZE_X_LOG2
#define LOGI_CHUNK_SIZE_Y_LOG2      GEN_CHUNK_SIZE_Y_LOG2
#define LOGI_CHUNK_SIZE_Z_LOG2      3
#define LOGI_CHUNK_SIZE_X           (1 << LOGI_CHUNK_SIZE_X_LOG2)
#define LOGI_CHUNK_SIZE_Y           (1 << LOGI_CHUNK_SIZE_Y_LOG2)
#define LOGI_CHUNK_SIZE_Z           (1 << LOGI_CHUNK_SIZE_Z_LOG2)

#define LONG_TICK_LENGTH   12
#define GRAVITY_IN_BLOCKS  20.0f

typedef struct
{
   int x,y,z;
} vec3i;

typedef struct
{
   int x0,y0,x1,y1;
} recti;

typedef union
{
   struct {
      uint16 x:6;
      uint16 y:6;
      uint16 z:4;
   } unpacked;
   uint16 packed;
} chunk_coord;

typedef struct
{
   chunk_coord pos;
   uint8 type;
   uint8 state;
   uint8 item;

   uint8 rot:2;
   uint8 input_is_belt:1;
   uint8 output_is_belt:1;
} render_picker_info; // 6 bytes

typedef struct
{
   chunk_coord pos; // 2 bytes
   uint8 type;           // 1 byte
   uint8 timer;          // 1 byte

   uint8 output;         // 1 byte
   uint8 config;         // 1 byte
   uint8 uses;           // 1 byte
   uint8 input_flags:6;  // 1 byte
   uint8 rot:2;          // 0 bytes
} render_machine_info;  // 8 bytes

typedef struct
{
   chunk_coord pos;   // 2
   uint8 type;             // 1
   uint8 rot:2;            // 1
   uint8 state:6;          // 0
   uint8 slots[2];         // 2
} render_belt_machine_info;  // 6 bytes

typedef struct
{
   chunk_coord pos;           // 2 bytes
   uint8 dir  ;               // 1 bytes             // 2
   uint8 len:6;               // 1 bytes             // 5
   int8  turn:2;              // 0 bytes             // 2
   uint8 type:1;              // 1 bytes             // 1
   int8  end_dz:2;            // 0 bytes             // 2
   uint8 mobile_slots[2];     // 2 bytes             // 7,7    // number of slots that can move including frontmost empty slot
   uint8 *items;
} render_belt_run;

#define IS_ITEM_MOBILE(br,pos,side)  \
      ((pos) < (br)->mobile_slots[side]-1)

// lengths for turning conveyor
#define SHORT_SIDE  2
#define LONG_SIDE   5

enum
{
   RIGHT=0,
   LEFT=1
};

typedef struct
{
   int slice_x,slice_y;
   int chunk_z;
   int num_belts;
   int num_machines;
   int num_belt_machines;
   int num_pickers;
   render_belt_run *belts; // obarr
   render_machine_info *machine;
   render_belt_machine_info *belt_machine;
   render_picker_info *pickers;
} render_logi_chunk;

#define ITEMS_PER_BELT_SIDE   4
#define BELT_SIDES   2
#define ITEMS_PER_BELT (ITEMS_PER_BELT_SIDE * BELT_SIDES)


// block types
enum
{
   BT_empty,
   BT_grass,
   BT_stone,
   BT_sand,
   BT_wood,
   BT_leaves,
   BT_gravel,
   BT_asphalt,
   BT_marble,

   BT_placeable=40,
   BT_conveyor=40,
   BT_conveyor_ramp_up_low,
   BT_conveyor_ramp_up_high,
   BT_conveyor_ramp_down_high,
   BT_conveyor_ramp_down_low,
   BT_conveyor_90_left,
   BT_conveyor_90_right,
   BT_splitter,

   BT_picker = 49,
   BT_machines = 50,
   BT_ore_drill,
   BT_ore_eater,
   BT_furnace,
   BT_iron_gear_maker,
   BT_conveyor_belt_maker,

   BT_belt_machines=250,
   BT_balancer=252,
   BT_no_change=255,
};

enum
{
   IT_empty,
   IT_coal,
   IT_iron_ore,
   IT_copper_ore,
   IT_ore_4,
   IT_ore_5,
   IT_ore_6,
   IT_ore_7,
   IT_iron_bar,
   IT_iron_gear,
   IT_steel_plate,
   IT_conveyor_belt,
   IT_asphalt,
   IT_stone,
   IT_conveyor_ramp_up_low,
   IT_conveyor_ramp_up_high,
   IT_conveyor_ramp_down_low,
   IT_conveyor_ramp_down_high,
   IT_picker,
   IT_conveyor_90_left,
   IT_conveyor_90_right,
   IT_ore_drill,
   IT_furnace,
   IT_iron_gear_maker,
   IT_conveyor_belt_maker,
   IT_splitter,
   IT_balancer,
};


enum {
   FACE_east,
   FACE_north,
   FACE_west,
   FACE_south,
   FACE_up,
   FACE_down
};


#if 0
// physics types
enum
{
   PT_empty,
   PT_solid,
};
#endif

typedef struct
{
   uint8 type;
   uint8 length;
} phys_chunk_run;

typedef struct
{
   uint16 data[MAX_Z_POW2CEIL/16]; // 32 bytes
} pathing_info;

typedef struct
{
   phys_chunk_run *column[MESH_CHUNK_SIZE_Y][MESH_CHUNK_SIZE_X];
   pathing_info pathdata[MESH_CHUNK_SIZE_Y][MESH_CHUNK_SIZE_X];
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

   size_t vbuf_size, fbuf_size;
   size_t total_size;
   Bool placeholder_for_size_info;

   float transform[3][3];
   float bounds[2][3];

   unsigned int vbuf;
   unsigned int fbuf, fbuf_tex;
   int num_quads;
   int has_triangles;

   float priority;

   int dirty;

   phys_chunk pc;
   arena_chunk **allocs;
} mesh_chunk;

typedef struct
{
   float hsz_x, hsz_y;   // half-size in x & y
   float height;         // height in z (origin is at base)

   float eye_z_offset;   // offset from ht to eye position (stored positive, must subtract)
   float torso_base_height; // offset from feet to bottom of torso position
} type_properties;

type_properties type_prop[];
extern Bool third_person;
typedef int32 objid;

typedef struct
{
   float old_z;
   float t;
} interpolate_z;

enum
{
   OTYPE__none,

   OTYPE_player,
   OTYPE_test,
   OTYPE_bounce,
   OTYPE_critter,

   OTYPE__count
};

#define MAX_BRAINS 4000

#define MAX_SHORT_PATH  64
typedef struct
{
   Bool valid;
   Bool has_target;
   vec3i target;
   vec3i path[MAX_SHORT_PATH];
   int path_position;
   int path_length;
} brain_state;

typedef struct
{
   vec position;
   vec ang;
   vec velocity;
   Bool on_ground;
   int type;
   brain_state *brain;

   interpolate_z iz;  // only needed for players

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

typedef struct
{
   vec3i size;
   int max_step_up;
   int max_step_down;
   Bool flying;
   int step_up_cost[8];
   int step_down_cost[8];
   int estimate_up_cost;
   int estimate_down_cost;
} path_behavior;

typedef struct
{
   int8 x,y,z;
   uint8 status:1;
   uint8 dir:3;
   int8 dz:4;
   uint16 cost;
   uint16 estimated_remaining;
} path_node;


#ifdef IS_64_BIT  // this doesn't exist yet and should be a different symbol
#define MAX_OBJECTS        32768
#define PLAYER_OBJECT_MAX   1024
#else
#define MAX_OBJECTS         8192
#define PLAYER_OBJECT_MAX    256
#endif

extern object obj[MAX_OBJECTS];
//extern player_data players[PLAYER_OBJECT_MAX];

extern float texture_scales[256];

//extern mesh_chunk     *c_mesh_cache[C_MESH_CHUNK_CACHE_Y][C_MESH_CHUNK_CACHE_X];

extern objid player_id;
extern objid max_obj_id, max_player_id;
enum
{
   RMS_invalid,
   RMS_requested,
   RMS_finished
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
   Bool needs_triangles;
   Bool rebuild_chunks;
   float priority;
   void *mcs;
} requested_mesh;

extern int face_dir[6][3];
extern float camloc[3];

extern size_t c_mesh_cache_in_use;
extern size_t mesh_cache_requested_in_use;
extern int chunk_locations, chunks_considered, chunks_in_frustum;
extern int quads_considered, quads_rendered;
extern int chunk_storage_rendered, chunk_storage_considered, chunk_storage_total;
extern int view_dist_for_display;
extern int num_threads_active, num_meshes_started, num_meshes_uploaded;
extern unsigned char tex1_for_blocktype[256][6];
extern float logistics_texture_scroll;
extern int selected_block[3];
extern int selected_block_to_create[3];
extern Bool selected_block_valid;
extern int screen_x, screen_y;
extern int debug_render;
extern Bool show_memory;

enum
{
   MODE_single_player,
   MODE_server,
   MODE_client
};

extern int program_mode;
extern player_controls p_input[PLAYER_OBJECT_MAX];
extern int global_hack;
extern int view_distance;

extern void *memory_mutex, *prof_mutex;

#endif
