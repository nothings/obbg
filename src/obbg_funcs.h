#ifndef INCLUDE_OBBG_FUNCS_H
#define INCLUDE_OBBG_FUNCS_H

#include "obbg_data.h"

extern void ods(char *fmt, ...);
extern void init_voxel_render(int voxtex[2]);
extern void render_voxel_world(float campos[3]);
extern void init_chunk_caches(void);
extern void init_mesh_building(void);
extern void init_mesh_build_threads(void);
extern int get_next_built_mesh(built_mesh *bm);
extern requested_mesh *get_requested_mesh_alternate(void);
extern void swap_requested_meshes(void);

extern mesh_chunk *build_mesh_chunk_for_coord(int x, int y);
extern void upload_mesh(mesh_chunk *mc, uint8 *vertex_build_buffer, uint8 *face_buffer);
extern void set_mesh_chunk_for_coord(int x, int y, mesh_chunk *new_mc);
extern mesh_chunk *get_mesh_chunk_for_coord(int x, int y);
//extern int collision_test_box(float x, float y, float z, float bounds[2][3]);
extern int physics_move_walkable(float *px, float *py, float *pz, float *vx, float *vy, float *vz, float dt, float size[2][3]);

#endif
