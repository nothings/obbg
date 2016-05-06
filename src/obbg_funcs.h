#ifndef INCLUDE_OBBG_FUNCS_H
#define INCLUDE_OBBG_FUNCS_H

#include "obbg_data.h"

extern void ods(char *fmt, ...);
extern void init_voxel_render(int voxtex[2]);
extern void render_voxel_world(float campos[3]);
extern void init_chunk_caches(void);
extern void init_mesh_building(void);
extern void init_mesh_build_threads(void);
extern void s_init_physics_cache(void);
extern void free_mesh_chunk(mesh_chunk *mc);
extern void free_mesh_chunk_physics(mesh_chunk *mc);
extern void world_init(void);

extern void process_tick_raw(float dt);
extern int get_next_built_mesh(built_mesh *bm);
extern requested_mesh *get_requested_mesh_alternate(void);
extern void swap_requested_meshes(void);
extern int get_block(int x, int y, int z);

extern mesh_chunk *build_mesh_chunk_for_coord(int x, int y);
extern void upload_mesh(mesh_chunk *mc, uint8 *vertex_build_buffer, uint8 *face_buffer);
extern void set_mesh_chunk_for_coord(int x, int y, mesh_chunk *new_mc);
extern mesh_chunk *get_mesh_chunk_for_coord(int x, int y);
extern mesh_chunk *get_physics_chunk_for_coord(int x, int y);
//extern int collision_test_box(float x, float y, float z, float bounds[2][3]);
extern int physics_move_walkable(vec *pos, vec *vel, float dt, float size[2][3]);

extern void physics_process_mesh_chunk(mesh_chunk *mc);
extern int physics_set_player_coord(requested_mesh *rm, int max_req, int px, int py);
extern void player_physics(objid oid, player_controls *con, float dt);
extern void force_update_for_block(int x, int y, int z);
extern void change_block(int x, int y, int z, int type);
extern void update_phys_chunk(mesh_chunk *mc, int x, int y, int z, int type);
extern void update_physics_cache(int x, int y, int z, int type);
extern void logistics_update_block(int x, int y, int z, int type);
extern void logistics_init(void);
extern void logistics_debug_render(void);


typedef struct
{
   int bx,by,bz;
   int face;
} RaycastResult;
extern Bool raycast(float x1, float y1, float z1, float x2, float y2, float z2, RaycastResult *res);

extern objid allocate_object(void);
extern objid allocate_player(void);
extern void objspace_to_worldspace(float world[3], objid oid, float cam_x, float cam_y, float cam_z);

typedef struct
{
   uint32 host;
   uint16 port;
} address;

extern Bool net_init(Bool server, int port);
extern Bool net_send(void *buffer, size_t buffer_size, address *addr);
extern int  net_receive(void *buffer, size_t buffer_size, address *addr);
extern void client_view_physics(objid oid, player_controls *con, float dt);
extern void client_net_tick(void);
extern void server_net_tick_pre_physics(void);
extern void server_net_tick_post_physics(void);

#endif
