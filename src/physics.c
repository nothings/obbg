#include "obbg_funcs.h"
#include <math.h>

#define COLLIDE_BLOB_X   8
#define COLLIDE_BLOB_Y   8
#define COLLIDE_BLOB_Z   8

typedef struct
{
   int x,y,z;
   unsigned char data[COLLIDE_BLOB_Y][COLLIDE_BLOB_X][COLLIDE_BLOB_Z];
} collision_geometry;

void gather_collision_geometry(collision_geometry *cg, int base_x, int base_y, int base_z)
{
   int cx0 = MESH_CHUNK_X_FOR_WORLD_X(base_x);
   int cy0 = MESH_CHUNK_Y_FOR_WORLD_Y(base_y);
   int cx1 = MESH_CHUNK_X_FOR_WORLD_X(base_x+COLLIDE_BLOB_X-1)+1;
   int cy1 = MESH_CHUNK_Y_FOR_WORLD_Y(base_y+COLLIDE_BLOB_Y-1)+1;
   int j,i;
   memset(cg, 0, sizeof(*cg));

   #if 0
   if (base_x <= -65 && -65 < base_x+COLLIDE_BLOB_X && base_y <= -25 && -25 < base_y + COLLIDE_BLOB_Y)
      __asm int 3;
   #endif

   cg->x = base_x;
   cg->y = base_y;
   cg->z = base_z;
   for (j=cy0; j < cy1; ++j) {
      for (i=cx0; i < cx1; ++i) {
         int x0 = i << MESH_CHUNK_SIZE_X_LOG2;
         int y0 = j << MESH_CHUNK_SIZE_Y_LOG2;
         mesh_chunk *mc = get_mesh_chunk_for_coord(i,j);
         if (mc != NULL) {
            int a,b;
            int rx1 = x0 + MESH_CHUNK_SIZE_X;
            int ry1 = y0 + MESH_CHUNK_SIZE_Y;
            int rx0 = stb_max(x0, base_x);
            int ry0 = stb_max(y0, base_y);
            rx1 = stb_min(rx1, base_x+COLLIDE_BLOB_X);
            ry1 = stb_min(ry1, base_y+COLLIDE_BLOB_Y);
            for (b=ry0; b < ry1; ++b) {
               for (a=rx0; a < rx1; ++a) {
                  phys_chunk_run *pcr = mc->pc.column[b - y0][a - x0];
                  int z=0;

                  // @TODO: pcr for -65,-25 was not the same pointer that
                  // we saw being created in the mesh chunk creator... what
                  // happened to it?!??!
                  #if 0
                  if (a == -65 && b == -25)
                     __asm int 3;
                  #endif
                  while (z < MAX_Z) {
                     int next_z = z + pcr->length;
                     if (base_z <= next_z && z < base_z + COLLIDE_BLOB_Z) {
                        int z0 = stb_max(base_z, z);
                        int z1 = stb_min(base_z+COLLIDE_BLOB_Z, next_z);
                        int k;
                        for (k=z0; k < z1; ++k)
                           cg->data[b-base_y][a-base_x][k-base_z] = pcr->type;
                     }
                     z = next_z;
                     ++pcr;
                  }
               }
            }
         }
      }
   }
}

//      #1.........|
//      |---v------|
int collision_test_box(collision_geometry *cg, float x, float y, float z, float bounds[2][3])
{
   int i,j,k;

   int x0 = (int) floor(x+bounds[0][0]);
   int y0 = (int) floor(y+bounds[0][1]);
   int z0 = (int) floor(z+bounds[0][2]);
   int x1 = (int) ceil(x+bounds[1][0]);
   int y1 = (int) ceil(y+bounds[1][1]);
   int z1 = (int) ceil(z+bounds[1][2]);

   //collision_geometry cg;

   assert(x1 <= x0 + COLLIDE_BLOB_X && y1 <= y0 + COLLIDE_BLOB_Y && z1 <= z0 + COLLIDE_BLOB_Z);
   
   //gather_collision_geometry(&cg, x0,y0,z0);
   assert(x0 >= cg->x && x1 <= cg->x + COLLIDE_BLOB_X &&
          y0 >= cg->y && y1 <= cg->y + COLLIDE_BLOB_Y &&
          z0 >= cg->z && z1 <= cg->z + COLLIDE_BLOB_Z);

   for (j=y0; j < y1; ++j)
      for (i=x0; i < x1; ++i)
         for (k=z0; k < z1; ++k)
            if (cg->data[j-cg->y][i-cg->x][k-cg->z] != BT_empty)
               return 1;

   return 0;
}

#define Z_EPSILON 0.001f

int physics_move_walkable(float *px, float *py, float *pz, float *pvx, float *pvy, float *pvz, float dt, float size[2][3])
{
   int result=0;
   int ix,iy,iz;
   float x = *px;
   float y = *py;
   float z = *pz;
   float dx = *pvx * dt;
   float dy = *pvy * dt;
   float dz = *pvz * dt;
   collision_geometry cg;

   ix = (int) floor(x);
   iy = (int) floor(y);
   iz = (int) floor(z + size[0][2] + (size[1][2] - size[0][2])/2);

   gather_collision_geometry(&cg, ix - COLLIDE_BLOB_X/2, iy - COLLIDE_BLOB_Y/2, iz - COLLIDE_BLOB_Z/2);

   if (!collision_test_box(&cg, x, y, z - 2*Z_EPSILON, size)) {
      if (collision_test_box(&cg, x+dx, y+dy, z+dz, size)) {
         result = 0;
         if (!collision_test_box(&cg, x+dx, y+0, z+dz, size)) {
            x += dx;
            *pvy = 0;
            z += dz;
         } else if (!collision_test_box(&cg, x+0, y+dy, z+dz, size)) {
            *pvx = 0;
            y += dy;
            z += dz;
         } else if (!collision_test_box(&cg, x+0, y+0, z+dz, size)) {
            *pvx = 0;
            *pvy = 0;
            z += dz;
         } else {
            // move bottom to floor of current voxel
            float min_z = z + dz;
            z = (float) floor(z + size[0][2]) - size[0][2];
            assert(!collision_test_box(&cg, x,y,z,size));
            while (!collision_test_box(&cg, x,y,z-1,size))
               z -= 1;
            assert(z >= min_z - Z_EPSILON);
            *pvz = 0;
            result = 1;
         }
      } else {
         result = 0;
         x += dx;
         y += dy;
         z += dz;
      }
   } else {
      result = 1;
      if (collision_test_box(&cg, x+dx,y+dy,z, size)) {
         // step up?
         if (!collision_test_box(&cg, x+dx,y+dy,z+1, size)) {
            x += dx;
            y += dy;
            z += 1;
         } else {
            *pvx = 0;
            *pvy = 0;
         }
      } else {
         x += dx;
         y += dy;
      }
   }

   *px = x;
   *py = y;
   *pz = z;

   return result;
}
