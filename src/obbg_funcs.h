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

extern mesh_chunk *build_mesh_chunk_for_coord(int x, int y);
extern void upload_mesh(mesh_chunk *mc, uint8 *vertex_build_buffer, uint8 *face_buffer);
extern void set_mesh_chunk_for_coord(int x, int y, mesh_chunk *new_mc);
extern mesh_chunk *get_mesh_chunk_for_coord(int x, int y);
extern mesh_chunk *get_physics_chunk_for_coord(int x, int y);
//extern int collision_test_box(float x, float y, float z, float bounds[2][3]);
extern int physics_move_walkable(vec *pos, vec *vel, float dt, float size[2][3]);

extern void physics_process_mesh_chunk(mesh_chunk *mc);
extern int physics_set_player_coord(requested_mesh *rm, int max_req, int px, int py);

extern objid allocate_object(void);
extern objid allocate_player(void);
extern void objspace_to_worldspace(float world[3], objid oid, float cam_x, float cam_y, float cam_z);

#endif
