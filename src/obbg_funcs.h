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

extern mesh_chunk *build_mesh_chunk_for_coord(int x, int y);
extern void upload_mesh(mesh_chunk *mc, uint8 *vertex_build_buffer, uint8 *face_buffer);

#endif
