//
//  Server's responsibility:
//
//    - authoratative physics
//    - generate terrain for physics
//    - AI
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

