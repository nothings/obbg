//
//  Server's responsibility:
//
//    - authoratative physics
//    - generate terrain for physics
//    - [[someday: AI]]
//
//  Client's responsibility:
//
//    - predictive physics
//    - generate terrain for rendering
//    - generate terrain for physics
//    X convert generated terrain to meshes
//    - render meshes
//    - poll user input
//
//  Terrain generator's responsibility:
//
//    - maintain gen_chunk cache
//    - generate gen_chunks
//    - convert generated terrain to meshes


#define STB_LEAKCHECK_IMPLEMENTATION
#include "stb_leakcheck_sdl.h"
