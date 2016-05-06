#include "obbg_funcs.h"
#include "stb_gl.h"

#define ITEMS_PER_BELT_SIDE   4
#define BELT_SIDES   2

#define ITEMS_PER_BELT (ITEMS_PER_BELT_SIDE * BELT_SIDES)

typedef struct
{
   uint8 type;
} beltside_item;

typedef struct
{
   uint8 x_off, y_off, z_off; // 3 bytes
   uint8 dir:2;               // 1 bytes
   uint8 len:6;               // 0 bytes
   beltside_item *items;      // 4 bytes
} belt_run;

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

      f.dir = e->dir;
      f.x_off = x;
      f.y_off = y;
      f.z_off = z;

      split_belt_raw(e, &f, pos);
      assert(e->x_off + face_dir[e->dir][0]*e->len == f.x_off);
      assert(e->y_off + face_dir[e->dir][1]*e->len == f.y_off);

      g.dir = f.dir;
      g.x_off = x + face_dir[dir][0];
      g.y_off = y + face_dir[dir][1];
      g.z_off = z;

      split_belt_raw(&f, &g, 1);
      assert(f.x_off + face_dir[f.dir][0]*f.len == g.x_off);
      assert(f.y_off + face_dir[f.dir][1]*f.len == g.y_off);

      assert(f.len == 1);

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
               merge_run(c, i, j);
               break;  
            }
         }
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
}

int face_orig[4][2] = {
   { 0,0 },
   { 1,0 },
   { 1,1 },
   { 0,1 },
};

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
                  for (a=0; a < stb_arr_len(c->br); ++a) {
                     belt_run *b = &c->br[a];
                     int d0 = b->dir;
                     int d1 = (d0 + 1) & 3;
                     float z = k * LOGI_CHUNK_SIZE_Z + 1.1f + b->z_off;

                     x = (float) s->slice_x * LOGI_CHUNK_SIZE_X + b->x_off;
                     y = (float) s->slice_y * LOGI_CHUNK_SIZE_Y + b->y_off;
                     x += face_orig[b->dir][0];
                     y += face_orig[b->dir][1];

                     //  +------+
                     //  |      |
                     //  |      |
                     //  +------+
                     // (0,0)

                     glBegin(GL_LINE_LOOP);
                        glColor3f(0.75,0.75,0);
                        glVertex3f(x                , y                , z);
                        glVertex3f(x+face_dir[d1][0], y+face_dir[d1][1], z);
                        glColor3f(0.5,1,0);
                        glVertex3f(x+face_dir[d1][0]+face_dir[d0][0]*b->len, y+face_dir[d1][1]+face_dir[d0][1]*b->len, z);
                        glVertex3f(x                +face_dir[d0][0]*b->len, y                +face_dir[d0][1]*b->len, z);
                     glEnd();
                  }
               }
            }
         }
      }
   }
}

void logistics_init(void)
{
   int i,j;
   for (j=0; j < LOGI_CACHE_SIZE; ++j)
      for (i=0; i < LOGI_CACHE_SIZE; ++i)
         logi_world[j][i].slice_x = i+1;
}