#include "obbg_funcs.h"
#include "stb_gl.h"
#include <math.h>

#define ITEMS_PER_BELT_SIDE   4
#define BELT_SIDES   2

#define ITEMS_PER_BELT (ITEMS_PER_BELT_SIDE * BELT_SIDES)

#define TARGET_none    0xffff
#define TARGET_unknown 0xfffe

typedef struct
{
   uint8 type;
} beltside_item;

typedef struct
{
   uint8 x_off, y_off, z_off; // 3 bytes
   uint8 dir  ;               // 1 bytes
   uint8 len  ;               // 1 bytes
   uint8 target_is_neighbor;  // 1 bytes
   uint8 mobile_slots[2];     // 2 bytes
   uint16 target_id;          // 2 bytes
   beltside_item *items;      // 4 bytes
} belt_run; // 16 bytes

typedef struct
{
   uint8 type[LOGI_CHUNK_SIZE_Z][LOGI_CHUNK_SIZE_Y][LOGI_CHUNK_SIZE_X];
   belt_run *br; // stb_arr 
} logi_chunk;

typedef struct
{
   int slice_x,slice_y;
   logi_chunk *chunk[MAX_Z_POW2CEIL / LOGI_CHUNK_SIZE_Z];
} logi_slice;

#define LOGI_CACHE_SIZE   128

#define LOGI_CHUNK_MASK_X(x) ((x) & (LOGI_CHUNK_SIZE_X-1))
#define LOGI_CHUNK_MASK_Y(y) ((y) & (LOGI_CHUNK_SIZE_Y-1))
#define LOGI_CHUNK_MASK_Z(z) ((z) & (LOGI_CHUNK_SIZE_Z-1))

//   4K x 4K

//     4K/32 => (2^12 / 2^5) => 2^7
//
//  2^7 * 2^7 * 2^5 * 2^2 => 2^(7+7+5+2) = 2^21 = 2MB

static logi_slice logi_world[LOGI_CACHE_SIZE][LOGI_CACHE_SIZE];

logi_chunk *logistics_get_chunk(int x, int y, int z)
{
   int slice_x = x >> LOGI_CHUNK_SIZE_X_LOG2;
   int slice_y = y >> LOGI_CHUNK_SIZE_Y_LOG2;
   int chunk_z = z >> LOGI_CHUNK_SIZE_Z_LOG2;
   logi_slice *s = &logi_world[slice_y & (LOGI_CACHE_SIZE-1)][slice_x & (LOGI_CACHE_SIZE-1)];
   if (s->slice_x != slice_x || s->slice_y != slice_y)
      return 0;
   return s->chunk[chunk_z];
}

void logistics_free_slice(logi_slice *s, int x, int y)
{
   int i;
   for (i=0; i < stb_arrcount(s->chunk); ++i)
      if (s->chunk[i] != NULL)
         free(s->chunk[i]);
   memset(s, 0, sizeof(*s));
   s->slice_x = x+1;
}

void logistics_create_slice(logi_slice *s, int x, int y)
{
   memset(s, 0, sizeof(*s));
   s->slice_x = x;
   s->slice_y = y;
}

logi_chunk *logistics_get_chunk_alloc(int x, int y, int z)
{
   int slice_x = x >> LOGI_CHUNK_SIZE_X_LOG2;
   int slice_y = y >> LOGI_CHUNK_SIZE_Y_LOG2;
   int chunk_z = z >> LOGI_CHUNK_SIZE_Z_LOG2;
   logi_slice *s = &logi_world[slice_y & (LOGI_CACHE_SIZE-1)][slice_x & (LOGI_CACHE_SIZE-1)];
   logi_chunk *c;
   if (s->slice_x != slice_x || s->slice_y != slice_y) {
      logistics_free_slice(s, slice_x, slice_y);
      logistics_create_slice(s, slice_x, slice_y);
   }
   c = s->chunk[chunk_z];
   if (c == NULL) {
      s->chunk[chunk_z] = c = malloc(sizeof(*c));
      memset(c, 0, sizeof(*c));
   }
   return c;
}

void compute_mobile_slots(belt_run *br)
{
   int j;
   int blocked[2] = { 1,1 };
   if (br->len == 0)
      return;
   br->mobile_slots[0] = br->mobile_slots[1] = 0;
   for (j=br->len*ITEMS_PER_BELT_SIDE-1; j >= 0; --j) {
      if (blocked[0]) {
         if (br->items[j*2+0].type == 0) {
            blocked[0] = 0;
            br->mobile_slots[0] = j;
         }
      }

      if (blocked[1]) {
         if (br->items[j*2+1].type == 0) {
            blocked[1] = 0;
            br->mobile_slots[1] = j;
         }
      }
   }
}

void split_belt_raw(belt_run *a, belt_run *b, int num_a)
{
   b->len = a->len - num_a;
   a->len = num_a;

   b->items = NULL;
   stb_arr_setlen(b->items, b->len * ITEMS_PER_BELT);
   memcpy(b->items,
          a->items + a->len*ITEMS_PER_BELT,
          b->len * ITEMS_PER_BELT * sizeof(b->items[0]));
   stb_arr_setlen(a->items, a->len * ITEMS_PER_BELT);
}

int find_belt_intersection(belt_run *a, int x, int y, int z)
{
   if (a->z_off == z) {
      switch (a->dir) {
         case FACE_east:
            if (y == a->y_off && a->x_off <= x && x < a->x_off + a->len)
               return 1;
            break;
         case FACE_west:
            if (y == a->y_off && a->x_off - a->len < x && x <= a->x_off)
               return 1;
            break;
         case FACE_north:
            if (x == a->x_off && a->y_off <= y && y < a->y_off + a->len)
               return 1;
            break;
         case FACE_south:
            if (x == a->x_off && a->y_off - a->len < y && y <= a->y_off)
               return 1;
            break;
      }
   }
   return 0;
}

void split_belt(logi_chunk *c, int x, int y, int z, int dir)
{
   int i, pos;
   for (i=0; i < stb_arr_len(c->br); ++i) {
      belt_run *a = &c->br[i];
      if (a->z_off == z && a->dir == dir) {
         switch (dir) {
            case FACE_east:
               if (y == a->y_off && a->x_off <= x && x < a->x_off + a->len) {
                  pos = x - a->x_off;
                  goto found;
               }
               break;
            case FACE_west:
               if (y == a->y_off && a->x_off - a->len < x && x <= a->x_off) {
                  pos = a->x_off - x;
                  goto found;
               }
               break;
            case FACE_north:
               if (x == a->x_off && a->y_off <= y && y < a->y_off + a->len) {
                  pos = y - a->y_off;
                  goto found;
               }
               break;
            case FACE_south:
               if (x == a->x_off && a->y_off - a->len < y && y <= a->y_off) {
                  pos = a->y_off - y;
                  goto found;
               }
               break;
         }
      }
   }
   assert(0);
  found:
   // split belt #i which contains x,y,z at 'pos' offset

   {
      belt_run *e,f,g;

      // split run into e, then f, then g, where 'f' contains x,y,z


      e = &c->br[i];
      assert(pos >= 0 && pos < e->len);
      assert(e->x_off + face_dir[e->dir][0]*pos == x);
      assert(e->y_off + face_dir[e->dir][1]*pos == y);

      if (e->len == 1)
         return;
      e->target_id = TARGET_unknown;

      f.dir = e->dir;
      f.x_off = x;
      f.y_off = y;
      f.z_off = z;
      f.target_id = TARGET_unknown;

      split_belt_raw(e, &f, pos);
      assert(e->x_off + face_dir[e->dir][0]*e->len == f.x_off);
      assert(e->y_off + face_dir[e->dir][1]*e->len == f.y_off);

      g.dir = f.dir;
      g.x_off = x + face_dir[dir][0];
      g.y_off = y + face_dir[dir][1];
      g.z_off = z;
      g.target_id = TARGET_unknown;

      split_belt_raw(&f, &g, 1);
      assert(f.x_off + face_dir[f.dir][0]*f.len == g.x_off);
      assert(f.y_off + face_dir[f.dir][1]*f.len == g.y_off);

      assert(f.len == 1);

      compute_mobile_slots(e);
      compute_mobile_slots(&f);
      compute_mobile_slots(&g);

      if (e->len == 0)
         *e = f;
      else
         stb_arr_push(c->br, f);

      if (g.len != 0)
         stb_arr_push(c->br, g);
   }
}

void destroy_belt_raw(logi_chunk *c, int i)
{
   belt_run *b = &c->br[i];
   stb_arr_free(b->items);
   stb_arr_fastdelete(c->br, i);
}

void destroy_belt(logi_chunk *c, int x, int y, int z)
{
   int i;
   for (i=0; i < stb_arr_len(c->br); ++i) {
      belt_run *b = &c->br[i];
      if (b->x_off == x && b->y_off == y && b->z_off == z) {
         destroy_belt_raw(c, i);
         return;
      }
   }
}

int merge_run(logi_chunk *c, int i, int j)
{
   belt_run *a = &c->br[i];
   belt_run *b = &c->br[j];
   beltside_item *bsi;
   assert(a->dir == b->dir);
   assert(a->z_off == b->z_off);
   assert(a->x_off + a->len*face_dir[a->dir][0] == b->x_off);
   assert(a->y_off + a->len*face_dir[a->dir][1] == b->y_off);

   // extend a
   stb_arr_setlen(a->items, (a->len+b->len) * ITEMS_PER_BELT);
   bsi = a->items + a->len*ITEMS_PER_BELT;
   memcpy(bsi, b->items, b->len*ITEMS_PER_BELT*sizeof(b->items[0]));
   a->len += b->len;

   a->target_id = TARGET_unknown;

   // delete b
   destroy_belt_raw(c, j);

   // return the merged belt
   if (i == stb_arr_len(c->br)) // did i get swapped down over j?
      return j;
   else
      return i;
}

// if there is already a belt there, it is one-long
void create_belt(logi_chunk *c, int x, int y, int z, int dir)
{
   int i, j;
   belt_run *a;
   for (i=0; i < stb_arr_len(c->br); ++i)
      if (c->br[i].x_off == x && c->br[i].y_off == y && c->br[i].z_off == z)
         break;
   if (i == stb_arr_len(c->br)) {
      belt_run nb;
      nb.x_off = x;
      nb.y_off = y;
      nb.z_off = z;
      nb.len = 1;
      nb.dir = dir;
      nb.items = NULL;
      stb_arr_setlen(nb.items, 8);
      memset(nb.items, 0, sizeof(nb.items[0]) * stb_arr_len(nb.items));
      stb_arr_push(c->br, nb);
   } else
      c->br[i].dir = dir;

   // now i refers to a 1-long belt; check for other belts to merge
   a = &c->br[i];

   for (j=0; j < stb_arr_len(c->br); ++j) {
      if (j != i) {
         belt_run *b = &c->br[j];
         if (b->dir == dir && b->z_off == a->z_off) {
            if (b->x_off + face_dir[dir][0]*b->len == a->x_off && b->y_off + face_dir[dir][1]*b->len == a->y_off) {
               i = merge_run(c, j, i);
               break;
            }
         }
      }
   }
   a = &c->br[i];

   for (j=0; j < stb_arr_len(c->br); ++j) {
      if (j != i) {
         belt_run *b = &c->br[j];
         if (b->dir == dir && b->z_off == a->z_off) {
            if (a->x_off + face_dir[dir][0]*a->len == b->x_off && a->y_off + face_dir[dir][1]*a->len == b->y_off) {
               i = merge_run(c, i, j);
               break;  
            }
         }
      }
   }

   c->br[i].target_id = TARGET_unknown;
   compute_mobile_slots(&c->br[i]);
}

void logistics_update_chunk(int x, int y, int z)
{
   logi_chunk *c = logistics_get_chunk(x,y,z);
   if (c != NULL) {
      int base_x = x & ~(LOGI_CHUNK_SIZE_X-1);
      int base_y = y & ~(LOGI_CHUNK_SIZE_Y-1);
      int base_z = z & ~(LOGI_CHUNK_SIZE_Z-1);
      int i,j;
      for (i=0; i < stb_arr_len(c->br); ++i) {
         if (1) { //c->br[i].target_id == TARGET_unknown) {
            logi_chunk *d = c;
            belt_run *b = &c->br[i];
            int ex = b->x_off + b->len * face_dir[b->dir][0];
            int ey = b->y_off + b->len * face_dir[b->dir][1];
            int ez = b->z_off, is_neighbor=0;
            if (ex < 0 || ey < 0 || ex >= LOGI_CHUNK_SIZE_X || ey >= LOGI_CHUNK_SIZE_Y) {
               d = logistics_get_chunk(base_x + ex, base_y + ey, z);
               ex = LOGI_CHUNK_MASK_X(ex);
               ey = LOGI_CHUNK_MASK_Y(ey);
               b->target_is_neighbor = 1;
            } else
               b->target_is_neighbor = 0;

            if (d != NULL) {
               for (j=0; j < stb_arr_len(d->br); ++j) {
                  if (find_belt_intersection(&d->br[j], ex,ey,ez)) {
                     b->target_id = j;
                     break;
                  }
               }
               if (j == stb_arr_len(d->br)) {
                  b->target_id = TARGET_none;
               }
            } else
               b->target_id = TARGET_none;
         }
      }
      {
         int i;
         for (i=0; i < stb_arr_len(c->br); ++i)
            assert(c->br[i].target_id != TARGET_unknown);
      }
   }
}

void logistics_update_block(int x, int y, int z, int type)
{
   logi_chunk *c = logistics_get_chunk_alloc(x,y,z);
   int ox = LOGI_CHUNK_MASK_X(x);
   int oy = LOGI_CHUNK_MASK_Y(y);
   int oz = LOGI_CHUNK_MASK_Z(z);
   int oldtype = c->type[oz][oy][ox];

   if (oldtype >= BT_conveyor_east && oldtype <= BT_conveyor_south) {
      split_belt(c, ox,oy,oz, oldtype - BT_conveyor_east);
   }

   c->type[oz][oy][ox] = type;

   if (type >= BT_conveyor_east && type <= BT_conveyor_south) {
      create_belt(c, ox,oy,oz, type-BT_conveyor_east);
   } else if (oldtype >= BT_conveyor_east && oldtype <= BT_conveyor_south) {
      destroy_belt(c, ox,oy,oz);
   }
   logistics_update_chunk(x,y,z);
   logistics_update_chunk(x - LOGI_CHUNK_SIZE_X, y, z);
   logistics_update_chunk(x + LOGI_CHUNK_SIZE_X, y, z);
   logistics_update_chunk(x, y - LOGI_CHUNK_SIZE_Y, z);
   logistics_update_chunk(x, y + LOGI_CHUNK_SIZE_Y, z);
   logistics_update_chunk(x, y, z - LOGI_CHUNK_SIZE_Z);
   logistics_update_chunk(x, y, z + LOGI_CHUNK_SIZE_Z);
}

int face_orig[4][2] = {
   { 0,0 },
   { 1,0 },
   { 1,1 },
   { 0,1 },
};

static int logistics_long_tick;
#define LONG_TICK_LENGTH   16

#pragma warning(disable:4244)
void logistics_render(void)
{
   int i,j,k,a,e;
   for (j=0; j < LOGI_CACHE_SIZE; ++j) {
      for (i=0; i < LOGI_CACHE_SIZE; ++i) {
         logi_slice *s = &logi_world[j][i];
         if (s->slice_x != i+1) {
            for (k=0; k < stb_arrcount(s->chunk); ++k) {
               logi_chunk *c = s->chunk[k];
               if (c != NULL) {
                  for (a=0; a < stb_arr_len(c->br); ++a) {
                     belt_run *b = &c->br[a];
                     float z = k * LOGI_CHUNK_SIZE_Z + 1.0f + b->z_off + 0.125f;
                     float x1 = (float) s->slice_x * LOGI_CHUNK_SIZE_X + b->x_off;
                     float y1 = (float) s->slice_y * LOGI_CHUNK_SIZE_Y + b->y_off;
                     float x2,y2;
                     float offset = (float) logistics_long_tick / LONG_TICK_LENGTH;// + stb_frand();
                     int d0 = b->dir;
                     int d1 = (d0 + 1) & 3;

                     x1 += face_orig[b->dir][0];
                     y1 += face_orig[b->dir][1];

                     x1 += face_dir[d0][0]*(1.0/ITEMS_PER_BELT_SIDE)*0.5;
                     y1 += face_dir[d0][1]*(1.0/ITEMS_PER_BELT_SIDE)*0.5;

                     x2 = x1 + face_dir[d1][0]*(0.8f - 0.5/ITEMS_PER_BELT_SIDE);
                     x1 = x1 + face_dir[d1][0]*(0.2f + 0.5/ITEMS_PER_BELT_SIDE);

                     y2 = y1 + face_dir[d1][1]*(0.8f - 0.5/ITEMS_PER_BELT_SIDE);
                     y1 = y1 + face_dir[d1][1]*(0.2f + 0.5/ITEMS_PER_BELT_SIDE);

                     for (e=0; e < stb_arr_len(b->items); e += 2) {
                        float x,y;
                        if (b->items[e+0].type != 0) {
                           x = x1, y = y1;
                           if (e < b->mobile_slots[0]*2) {
                              x += face_dir[d0][0]*(1.0/ITEMS_PER_BELT_SIDE) * offset;
                              y += face_dir[d0][1]*(1.0/ITEMS_PER_BELT_SIDE) * offset;
                           }
                           add_sprite(x, y, z, b->items[e+0].type);

                        }
                        if (b->items[e+1].type != 0) {
                           x = x2, y = y2;
                           if (e < b->mobile_slots[1]*2) {
                              x += face_dir[d0][0]*(1.0/ITEMS_PER_BELT_SIDE) * offset;
                              y += face_dir[d0][1]*(1.0/ITEMS_PER_BELT_SIDE) * offset;
                           }
                           add_sprite(x, y, z, b->items[e+1].type);
                        }
                        x1 += face_dir[d0][0]*(1.0/ITEMS_PER_BELT_SIDE);
                        y1 += face_dir[d0][1]*(1.0/ITEMS_PER_BELT_SIDE);
                        x2 += face_dir[d0][0]*(1.0/ITEMS_PER_BELT_SIDE);
                        y2 += face_dir[d0][1]*(1.0/ITEMS_PER_BELT_SIDE);
                     }
                  }
               }
            }
         }
      }
   }
}

void logistics_debug_render(void)
{
   int i,j,k,a;
   for (j=0; j < LOGI_CACHE_SIZE; ++j) {
      for (i=0; i < LOGI_CACHE_SIZE; ++i) {
         logi_slice *s = &logi_world[j][i];
         if (s->slice_x != i+1) {
            float x,y;
            for (k=0; k < stb_arrcount(s->chunk); ++k) {
               logi_chunk *c = s->chunk[k];
               if (c != NULL) {
                  int base_x = s->slice_x * LOGI_CHUNK_SIZE_X;
                  int base_y = s->slice_y * LOGI_CHUNK_SIZE_Y;
                  int base_z = k * LOGI_CHUNK_SIZE_Z;
                  for (a=0; a < stb_arr_len(c->br); ++a) {
                     belt_run *b = &c->br[a];
                     int d0 = b->dir;
                     int d1 = (d0 + 1) & 3;
                     float z = k * LOGI_CHUNK_SIZE_Z + 1.1f + b->z_off;

                     x = (float) base_x + b->x_off;
                     y = (float) base_y + b->y_off;
                     x += face_orig[b->dir][0];
                     y += face_orig[b->dir][1];

                     //  +------+
                     //  |      |
                     //  |      |
                     //  +------+
                     // (0,0)

                     glBegin(GL_LINE_LOOP);
                        glColor3f(0.75,0,0);
                        glVertex3f(x                , y                , z);
                        glVertex3f(x+face_dir[d1][0], y+face_dir[d1][1], z);
                        glColor3f(0.75,0.75,0);
                        glVertex3f(x+face_dir[d1][0]+face_dir[d0][0]*b->len, y+face_dir[d1][1]+face_dir[d0][1]*b->len, z);
                        glVertex3f(x                +face_dir[d0][0]*b->len, y                +face_dir[d0][1]*b->len, z);
                     glEnd();

                     if (b->target_id != TARGET_none) {
                        int ex = base_x + b->x_off + b->len * face_dir[b->dir][0];
                        int ey = base_y + b->y_off + b->len * face_dir[b->dir][1];
                        int ez = base_z + b->z_off, is_neighbor=0;
                        logi_chunk *c = logistics_get_chunk(ex,ey,ez);
                        assert(c != NULL);
                        if (c) {
                           float tx,ty,tz;
                           belt_run *t = &c->br[b->target_id];
                           tx = (float) (ex & ~(LOGI_CHUNK_SIZE_X-1)) + t->x_off + 0.5;
                           ty = (float) (ey & ~(LOGI_CHUNK_SIZE_Y-1)) + t->y_off + 0.5;
                           tz = (float) (ez & ~(LOGI_CHUNK_SIZE_Z-1)) + t->z_off + 1.2f;
                           //tx = ex+0.5, ty = ey+0.5, tz = ez+1.2f;
                           glBegin(GL_LINES);
                           glVertex3f(x-face_orig[b->dir][0]+0.5+face_dir[d0][0]*(b->len-1),
                                      y-face_orig[b->dir][1]+0.5+face_dir[d0][1]*(b->len-1),
                                      z+0.1f);
                           glVertex3f(tx,ty,tz);
                           glEnd();
                        }
                     }
                  }
               }
            }
         }
      }
   }


   {
      vec pos = obj[player_id].position;
      int x,y,z;
      x = (int) floor(pos.x);
      y = (int) floor(pos.y);
      z = (int) floor(pos.z);

      x &= ~(LOGI_CHUNK_SIZE_X-1);
      y &= ~(LOGI_CHUNK_SIZE_Y-1);
      z &= ~(LOGI_CHUNK_SIZE_Z-1);

      glDisable(GL_TEXTURE_2D);
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      glColor4f(0.2f,0.2f,0.2f,0.2f);
      glDisable(GL_CULL_FACE);
      stbgl_drawBox(x+LOGI_CHUNK_SIZE_X/2, y+LOGI_CHUNK_SIZE_Y/2, z+LOGI_CHUNK_SIZE_Z/2, LOGI_CHUNK_SIZE_X-0.125f, LOGI_CHUNK_SIZE_Y-0.125f, LOGI_CHUNK_SIZE_Z-0.125f, 0);
      glEnable(GL_TEXTURE_2D);
      glDisable(GL_BLEND);
   }

}

void logistics_init(void)
{
   int i,j;
   for (j=0; j < LOGI_CACHE_SIZE; ++j)
      for (i=0; i < LOGI_CACHE_SIZE; ++i)
         logi_world[j][i].slice_x = i+1;
}

// 7 6 5 4 3 2 1 0
// X X . . X X . .
// X X . X X . . .
// X X X X . . . .

typedef struct
{
   int off_x, off_y, off_z;
   logi_chunk *c;
} target_chunk;

static void get_target_chunk(target_chunk *tc, int bx, int by, int bz, logi_chunk *c, belt_run *br)
{
   if (1 || br->target_is_neighbor) {
      int ex = bx + br->x_off + br->len * face_dir[br->dir][0];
      int ey = by + br->y_off + br->len * face_dir[br->dir][1];
      int ez = bz + br->z_off;
      logi_chunk *c = logistics_get_chunk(ex,ey,ez);
      tc->c = c;
      tc->off_x = (ex & ~(LOGI_CHUNK_SIZE_X-1)) - bx;
      tc->off_y = (ey & ~(LOGI_CHUNK_SIZE_Y-1)) - by;
      tc->off_z = (ez & ~(LOGI_CHUNK_SIZE_Z-1)) - bz;
   }
}

void logistics_chunk_tick(logi_slice *s, int cid)
{
   int i;
   logi_chunk *c = s->chunk[cid];
   assert(c != NULL);

   for (i=0; i < stb_arr_len(c->br); ++i) {
      int j;
      belt_run *br = &c->br[i];
      int len;
      int force_mobile[2] = { 0,0 };

      len = br->len * ITEMS_PER_BELT;
      if (global_hack == -1)
         memset(br->items, 0, sizeof(br->items[0]) * len);

      if (br->target_id != TARGET_none) {
         int turn;
         target_chunk tc;
         belt_run *tb;
         get_target_chunk(&tc, s->slice_x * LOGI_CHUNK_SIZE_X, s->slice_y * LOGI_CHUNK_SIZE_Y, cid * LOGI_CHUNK_SIZE_Z, c, br);
         assert(tc.c != NULL);
         tb = &tc.c->br[br->target_id];
         turn = (tb->dir - br->dir) & 3;
         switch (turn) {
            case 0: {
               // conveyor continues straight ahead
               if (tb->items[0].type == 0) {
                  tb->items[0] = br->items[len-2];
                  br->items[len-2].type = 0;
               }
               if (tb->items[1].type == 0) {
                  tb->items[1] = br->items[len-1];
                  br->items[len-1].type = 0;
               }
               force_mobile[0] = (tb->mobile_slots[0] > 0);
               force_mobile[1] = (tb->mobile_slots[1] > 0);
               break;
            }
            case 1: {
               // e.g. from east to north
               //
               //                 ^  ^
               //  [1] X X X X -> O  O 
               //                 O  O
               //  [0] X X X X -> O  O
               //                 O  O

               int ex = br->x_off + br->len*face_dir[br->dir][0];
               int ey = br->y_off + br->len*face_dir[br->dir][1];
               // offset of ex/ey point on belt is just manhattan distance from ex/ey point from start
               int pos = abs(ex - (tc.off_x + tb->x_off)) + abs(ey - (tc.off_y + tb->y_off));
               int itembase = pos * ITEMS_PER_BELT_SIDE;

               if (br->items[len-2].type != 0) {
                  int itempos = (itembase + (ITEMS_PER_BELT_SIDE/4-1))*2+1;
                  assert(itempos < stb_arr_len(tb->items));
                  if (tb->items[itempos].type == 0) {
                     tb->items[itempos] = br->items[len-2];
                     br->items[len-2].type = 0;
                  }
               }

               if (br->items[len-1].type != 0) {
                  int itempos = (itembase + (ITEMS_PER_BELT_SIDE*3/4-1))*2+1;
                  if (tb->items[itempos].type == 0) {
                     tb->items[itempos] = br->items[len-1];
                     br->items[len-1].type = 0;
                  }
               }
               break;
            }
            case 2:
               // conveyors point in opposite directions
               break;
            case 3: {
               // e.g. from west to north
               //
               //   ^  ^
               //   O  O <- X X X X [0]
               //   O  O
               //   O  O <- X X X X [1]
               //   O  O
               int ex = br->x_off + br->len*face_dir[br->dir][0];
               int ey = br->y_off + br->len*face_dir[br->dir][1];
               // offset of ex/ey point on belt is just manhattan distance from ex/ey point from start
               int pos = abs(ex - (tc.off_x + tb->x_off)) + abs(ey - (tc.off_y + tb->y_off));
               int itembase = pos * ITEMS_PER_BELT_SIDE;

               if (br->items[len-2].type != 0) {
                  int itempos = (itembase + (ITEMS_PER_BELT_SIDE*3/4))*2+0;
                  assert(itempos < stb_arr_len(tb->items));
                  if (tb->items[itempos].type == 0) {
                     tb->items[itempos] = br->items[len-2];
                     br->items[len-2].type = 0;
                  }
               }

               if (br->items[len-1].type != 0) {
                  int itempos = (itembase + (ITEMS_PER_BELT_SIDE/4))*2+0;
                  if (tb->items[itempos].type == 0) {
                     tb->items[itempos] = br->items[len-1];
                     br->items[len-1].type = 0;
                  }
               }
               break;
            }
         }
      }
      len = br->len * ITEMS_PER_BELT_SIDE;

      for (j=len-2; j >= 0; --j)
         if (br->items[j*2+0].type != 0 && br->items[j*2+2].type == 0) {
            br->items[j*2+2] = br->items[j*2+0];
            br->items[j*2+0].type = 0;
         }
      for (j=len-2; j >= 0; --j)
         if (br->items[j*2+1].type != 0 && br->items[j*2+3].type == 0) {
            br->items[j*2+3] = br->items[j*2+1];
            br->items[j*2+1].type = 0;
         }
      for (j=len-1; j >= 0; --j)
         if (br->items[j*2+0].type == 0)
            break;
      br->mobile_slots[0] = j < 0 ? 0 : j;
      if (force_mobile[0]) br->mobile_slots[0] = len;
      for (j=len-1; j >= 0; --j)
         if (br->items[j*2+1].type == 0)
            break;
      br->mobile_slots[1] = j < 0 ? 0 : j;
      if (force_mobile[1]) br->mobile_slots[1] = len;
      if (global_hack) {
         if (stb_rand() % 10 < 2) {
            if (br->items[0].type == 0 && stb_rand() % 9 < 2) br->items[0].type = stb_rand() % 4;
            if (br->items[1].type == 0 && stb_rand() % 9 < 2) br->items[1].type = stb_rand() % 4;
         }
      }
   }
}

void logistics_tick(void)
{
   logistics_texture_scroll += (1.0 / LONG_TICK_LENGTH / ITEMS_PER_BELT_SIDE);
   if (logistics_texture_scroll >= 1.0)
      logistics_texture_scroll -= 1.0;

   if (++logistics_long_tick == LONG_TICK_LENGTH) {
      int i,j,k;
      logistics_long_tick = 0;
      for (j=0; j < LOGI_CACHE_SIZE; ++j) {
         for (i=0; i < LOGI_CACHE_SIZE; ++i) {
            logi_slice *s = &logi_world[j][i];
            if (s->slice_x != i+1) {
               for (k=0; k < stb_arrcount(s->chunk); ++k) {
                  logi_chunk *c = s->chunk[k];
                  if (c != NULL) {
                     logistics_chunk_tick(s, k);
                  }
               }
            }
         }
      }
   }
}
