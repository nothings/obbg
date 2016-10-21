#include "obbg_funcs.h"
#include <math.h>


#define S_PHYSICS_CACHE_X_LOG2  3
#define S_PHYSICS_CACHE_Y_LOG2  3

#define S_PHYSICS_CACHE_X  (1 << S_PHYSICS_CACHE_X_LOG2)
#define S_PHYSICS_CACHE_Y  (1 << S_PHYSICS_CACHE_Y_LOG2)


static mesh_chunk s_phys_cache[S_PHYSICS_CACHE_Y][S_PHYSICS_CACHE_X];
static int player_x, player_y;
vec3i physics_cache_feedback[64][64];

void s_init_physics_cache(void)
{
   int i,j;
   for (j=0; j < S_PHYSICS_CACHE_Y; ++j)
      for (i=0; i < S_PHYSICS_CACHE_X; ++i)
         s_phys_cache[j][i].chunk_x = i+1;
}

void update_physics_cache_feedback(void)
{
   int i,j;
   for (j=0; j < S_PHYSICS_CACHE_Y; ++j) {
      for (i=0; i < S_PHYSICS_CACHE_X; ++i) {
         mesh_chunk *phys_cache_mc = &s_phys_cache[j][i];
         physics_cache_feedback[j][i].x = phys_cache_mc->chunk_x;// - C_MESH_CHUNK_X_FOR_WORLD_X(player_x);
         physics_cache_feedback[j][i].y = phys_cache_mc->chunk_y;// - C_MESH_CHUNK_Y_FOR_WORLD_Y(player_y);
      }
   }
}


int physics_set_player_coord(requested_mesh *rm, int max_req, int px, int py)
{
   int i,j,k,n=0;

   int player_cx, player_cy;

   player_x = px;
   player_y = py;

   player_cx = C_MESH_CHUNK_X_FOR_WORLD_X(player_x);
   player_cy = C_MESH_CHUNK_Y_FOR_WORLD_Y(player_y);

   if (rm != NULL) {
      for (k=0; k < stb_max(S_PHYSICS_CACHE_X, S_PHYSICS_CACHE_Y); ++k) {
         for (j=0; j < S_PHYSICS_CACHE_Y; ++j) {
            for (i=0; i < S_PHYSICS_CACHE_X; ++i) {
               int dy = j - S_PHYSICS_CACHE_Y/2;
               int dx = i - S_PHYSICS_CACHE_X/2;
               if (abs(dx)+abs(dy) == k) {
                  int rx = player_cx + dx;
                  int ry = player_cy + dy;
                  mesh_chunk *phys_cache_mc = &s_phys_cache[ry & (S_PHYSICS_CACHE_Y-1)][rx & (S_PHYSICS_CACHE_X-1)];
                  if (phys_cache_mc->chunk_x != rx || phys_cache_mc->chunk_y != ry) {
                     if (n < max_req) {
                        int dx_w = dx * MESH_CHUNK_SIZE_X;
                        int dy_w = dy * MESH_CHUNK_SIZE_Y;
                        rm[n].x = rx << MESH_CHUNK_SIZE_X_LOG2;
                        rm[n].y = ry << MESH_CHUNK_SIZE_Y_LOG2;
                        rm[n].state = RMS_requested;
                        rm[n].needs_triangles = False;
                        rm[n].priority = (dx_w * dx_w + dy_w * dy_w)/2.0f - 100.0f;
                        ++n;
                     }
                  }
               }
            }
         }
      }
   }
   qsort(rm, n, sizeof(rm[0]), stb_floatcmp(offsetof(requested_mesh, priority)));

   update_physics_cache_feedback();

   return n;
}

void physics_process_mesh_chunk(mesh_chunk *mc)
{
   int player_cx = C_MESH_CHUNK_X_FOR_WORLD_X(player_x);
   int player_cy = C_MESH_CHUNK_Y_FOR_WORLD_Y(player_y);

   // if it's within the player bounds, update it
   if (mc->chunk_x >= player_cx - S_PHYSICS_CACHE_X/2 && mc->chunk_x < player_cx + S_PHYSICS_CACHE_X/2 &&
       mc->chunk_y >= player_cy - S_PHYSICS_CACHE_Y/2 && mc->chunk_y < player_cy + S_PHYSICS_CACHE_Y/2)
   {
      int phys_cache_x = (mc->chunk_x & (S_PHYSICS_CACHE_X-1));
      int phys_cache_y = (mc->chunk_y & (S_PHYSICS_CACHE_Y-1));
      mesh_chunk *phys_cache_mc = &s_phys_cache[phys_cache_y][phys_cache_x];

      free_mesh_chunk_physics(phys_cache_mc);

      *phys_cache_mc = *mc;
   }

   update_physics_cache_feedback();
}

void free_physics_cache(void)
{
   int i,j;
   for (j=0; j < S_PHYSICS_CACHE_Y; ++j)
      for (i=0; i < S_PHYSICS_CACHE_X; ++i)
         free_mesh_chunk_physics(&s_phys_cache[j][i]);
}

mesh_chunk *get_physics_chunk_for_coord(int x, int y)
{
   int cx = C_MESH_CHUNK_X_FOR_WORLD_X(x);
   int cy = C_MESH_CHUNK_Y_FOR_WORLD_Y(y);
   int rx = cx & (S_PHYSICS_CACHE_X-1);
   int ry = cy & (S_PHYSICS_CACHE_Y-1);
   mesh_chunk *mc = &s_phys_cache[ry][rx];
   if (mc->chunk_x == cx && mc->chunk_y == cy)
      return mc;
   return NULL;
}

void update_physics_cache(int x, int y, int z, int type, int rot)
{
   int cx = C_MESH_CHUNK_X_FOR_WORLD_X(x);
   int cy = C_MESH_CHUNK_Y_FOR_WORLD_Y(y);
   int rx = cx & (S_PHYSICS_CACHE_X-1);
   int ry = cy & (S_PHYSICS_CACHE_Y-1);
   mesh_chunk *mc = &s_phys_cache[ry][rx];
   if (mc->chunk_x != cx || mc->chunk_y != cy)
      return;

   update_phys_chunk(mc, x - C_WORLD_X_FOR_MESH_CHUNK_X(cx), y - C_WORLD_Y_FOR_MESH_CHUNK_Y(cy), z, type);
}


#define COLLIDE_BLOB_X   20
#define COLLIDE_BLOB_Y   20
#define COLLIDE_BLOB_Z   20

typedef struct
{
   int x,y,z;
   unsigned char data[COLLIDE_BLOB_Y][COLLIDE_BLOB_X][COLLIDE_BLOB_Z];
} collision_geometry;

static Bool gather_collision_geometry(collision_geometry *cg, int base_x, int base_y, int base_z)
{
   int cx0 = C_MESH_CHUNK_X_FOR_WORLD_X(base_x);
   int cy0 = C_MESH_CHUNK_Y_FOR_WORLD_Y(base_y);
   int cx1 = C_MESH_CHUNK_X_FOR_WORLD_X(base_x+COLLIDE_BLOB_X-1)+1;
   int cy1 = C_MESH_CHUNK_Y_FOR_WORLD_Y(base_y+COLLIDE_BLOB_Y-1)+1;
   int j,i;
   Bool found_bad = False;
   memset(cg, 0, sizeof(*cg));

   cg->x = base_x;
   cg->y = base_y;
   cg->z = base_z;
   for (j=cy0; j < cy1; ++j) {
      for (i=cx0; i < cx1; ++i) {
         int x0 = i << MESH_CHUNK_SIZE_X_LOG2;
         int y0 = j << MESH_CHUNK_SIZE_Y_LOG2;
         mesh_chunk *mc = get_physics_chunk_for_coord(x0,y0);
         if (mc == NULL) {
            found_bad = True;
         } else {
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
   return !found_bad;
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
#define STEP_UP_VELOCITY -0.75

vec find_collision_point(collision_geometry *cg, vec *a, vec *b, float size[2][3])
{
   vec delta = vec_sub(b,a);
   float t0 = 0.0f, t1 = 1.0f;
   int i;
   for (i=0; i < 8; ++i) {
      float t = (t0 + t1) / 2.0f;
      vec m = vec_add_scale(a, &delta, t);
      if (collision_test_box(cg, m.x, m.y, m.z, size))
         t1 = t;
      else
         t0 = t;
   }
   return vec_add_scale(a, &delta, t0);
}

Bool physics_move_inanimate(vec *pos, vec *vel, float dt, float size[2][3], Bool on_ground)
{
   int ix,iy,iz;
   float x = pos->x;
   float y = pos->y;
   float z = pos->z;
   float dx = vel->x * dt;
   float dy = vel->y * dt;
   float dz = vel->z * dt;
   collision_geometry cg;

   ix = (int) floor(x);
   iy = (int) floor(y);
   iz = (int) floor(z + size[0][2] + (size[1][2] - size[0][2])/2);

   if (on_ground) {
      // sliding or immobile
      return True;
   } else {
      Bool started_colliding;
      vec v;
      // free-fall
      v = vec_add_scale(pos, vel, dt);

      gather_collision_geometry(&cg, ix - COLLIDE_BLOB_X/2, iy - COLLIDE_BLOB_Y/2, iz - COLLIDE_BLOB_Z/2);
      started_colliding = collision_test_box(&cg, pos->x, pos->y, pos->z, size);

      // test if end point is non-colliding
      if (collision_test_box(&cg, v.x, v.y, v.z, size)) {
         *pos = find_collision_point(&cg, pos, &v, size);

         // new location shouldn't be colliding
         if (!started_colliding)
            assert(!collision_test_box(&cg, pos->x, pos->y, pos->z, size));

         // test if it's on the ground
         if (collision_test_box(&cg, pos->x, pos->y, pos->z - 2*Z_EPSILON, size)) {
            memset(vel, 0, sizeof(*vel));
            return TRUE;
         }
         vel->x = vel->y = 0;
         if (vel->z > 0)
            vel->z = 0;
         else
            vel->z /= 2.0f;
         return False;
      }
      *pos = v;
      vel->z -= GRAVITY_IN_BLOCKS * dt;
   }
   return False;
}


int physics_move_walkable(vec *pos, vec *vel, float dt, float size[2][3], interpolate_z *lerpz)
{
   int result=0;
   int ix,iy,iz;
   Bool step_up=False;
   float x = pos->x;
   float y = pos->y;
   float z = pos->z;
   float dx = vel->x * dt;
   float dy = vel->y * dt;
   float dz = vel->z * dt;
   collision_geometry cg;

   ix = (int) floor(x);
   iy = (int) floor(y);
   iz = (int) floor(z + size[0][2] + (size[1][2] - size[0][2])/2);

   gather_collision_geometry(&cg, ix - COLLIDE_BLOB_X/2, iy - COLLIDE_BLOB_Y/2, iz - COLLIDE_BLOB_Z/2);

   if (!collision_test_box(&cg, x, y, z - 2*Z_EPSILON, size)) {
      // not in contact w/ the ground
      if (collision_test_box(&cg, x+dx, y+dy, z+dz, size)) {
         result = 0;
         if (!collision_test_box(&cg, x+dx, y+0, z+dz, size)) {
            x += dx;
            vel->y = 0;
            z += dz;
         } else if (!collision_test_box(&cg, x+0, y+dy, z+dz, size)) {
            vel->x = 0;
            y += dy;
            z += dz;
         } else if (!collision_test_box(&cg, x+0, y+0, z+dz, size)) {
            vel->x = 0;
            vel->y = 0;
            z += dz;
         } else if (vel->z > STEP_UP_VELOCITY && !collision_test_box(&cg, x+dx, y+dy, (float) ceil(z), size) && collision_test_box(&cg, x+dx, y+dy, (float) ceil(z)-1, size)) {
            vel->z = 0;
            x += dx;
            y += dy;
            z = (float) ceil(z);
            step_up = True;
         } else if (vel->z > STEP_UP_VELOCITY && !collision_test_box(&cg, x+dx, y   , (float) ceil(z), size) && collision_test_box(&cg, x+dx, y   , (float) ceil(z)-1, size)) {
            vel->z = 0;
            x += dx;
            vel->y = 0;
            z = (float) ceil(z);
            step_up = True;
         } else if (vel->z > STEP_UP_VELOCITY && !collision_test_box(&cg, x   , y+dy, (float) ceil(z), size) && collision_test_box(&cg, x   , y+dy, (float) ceil(z)-1, size)) {
            vel->z = 0;
            vel->x = 0;
            y += dy;
            z = (float) ceil(z);
            step_up = True;
         } else {
            // move bottom to floor of current voxel
            if (dz > 0) {
               float max_z = z + dz - Z_EPSILON;
               assert(!collision_test_box(&cg, x,y,z,size));
               z = (float) ceil(z + size[1][2]) - size[1][2] - Z_EPSILON;
               assert(z <= max_z + Z_EPSILON);
               assert(!collision_test_box(&cg, x,y,z,size));
               while (!collision_test_box(&cg, x,y,z+1,size) && z+1 <= max_z)
                  z += 1;
               assert(z <= max_z + Z_EPSILON);
               vel->z = 0;
               result = 1;
            } else {
               float min_z = z + dz;
               assert(!collision_test_box(&cg, x,y,z,size));
               z = (float) floor(z + size[0][2]) - size[0][2];
               assert(z >= min_z - Z_EPSILON);
               assert(!collision_test_box(&cg, x,y,z,size));
               while (!collision_test_box(&cg, x,y,z-1,size) && z-1 >= min_z)
                  z -= 1;
               assert(z >= min_z - Z_EPSILON);
               vel->z = 0;
               result = 1;
            }
         }
      } else {
         result = 0;
         x += dx;
         y += dy;
         z += dz;
      }
   } else {
      // previous position is currently in contact w/ the ground
      result = 1;

      if (collision_test_box(&cg, x+dx,y+dy,z, size)) {
         // step up?
         if (!collision_test_box(&cg, x+dx,y+dy,z+1, size)) {
            // step up!
            x += dx;
            y += dy;
            z += 1;
            step_up = True;
         } else {
            if (!collision_test_box(&cg, x+dx, y+0, z, size)) {
               x += dx;
               vel->y = 0;
            } else if (!collision_test_box(&cg, x+0, y+dy, z, size)) {
               vel->x = 0;
               y += dy;
            } else if (!collision_test_box(&cg, x+dx, y+0, z+1, size)) {
               x += dx;
               vel->y = 0;
               z += 1;
            } else if (!collision_test_box(&cg, x+0, y+dy, z+1, size)) {
               vel->x = 0;
               y += dy;
               z += 1;
            } else {
               vel->x = 0;
               vel->y = 0;
            }
         }
      } else {
         x += dx;
         y += dy;
      }
   }

   if (step_up) {
      lerpz->old_z = smoothed_z_for_rendering(pos, lerpz);
      lerpz->t = 1;
   }

   pos->x = x;
   pos->y = y;
   pos->z = z;

   return result;
}

Bool raycast(float x1, float y1, float z1, float x2, float y2, float z2, RaycastResult *res)
{
   float t;

   float xm = stb_min(x1,x2);
   float ym = stb_min(y1,y2);
   float zm = stb_min(z1,z2);

   collision_geometry cg;
   gather_collision_geometry(&cg, (int) floor(xm), (int) floor(ym), (int) floor(zm));

   x1 -= cg.x;    x2 -= cg.x;    
   y1 -= cg.y;    y2 -= cg.y;    
   z1 -= cg.z;    z2 -= cg.z;    

   for (t=0; t <= 1.0; t += 1.0/1024) {
      float x = stb_lerp(t, x1, x2);
      float y = stb_lerp(t, y1, y2);
      float z = stb_lerp(t, z1, z2);
      int ix = (int) x, iy = (int) y, iz = (int) z;
      if (cg.data[iy][ix][iz] != BT_empty) {
         res->bx = ix + cg.x;
         res->by = iy + cg.y;
         res->bz = iz + cg.z;

         x -= ix+0.5f;
         y -= iy+0.5f;
         z -= iz+0.5f;
         if (fabs(x) > fabs(y) && fabs(x) > fabs(z))
            res->face = x < 0 ? FACE_west  : FACE_east;
         else if (fabs(y) > fabs(x) && fabs(y) > fabs(z))
            res->face = y < 0 ? FACE_south : FACE_north;
         else
            res->face = z < 0 ? FACE_down  : FACE_up;
         
         return True;
      }
   }

   return False;
}
