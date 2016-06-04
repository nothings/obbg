#include "obbg_funcs.h"
#include "stb_gl.h"
#include <math.h>
#include "sdl_thread.h"

#define ITEMS_PER_BELT_SIDE   4
#define BELT_SIDES   2

#define ITEMS_PER_BELT (ITEMS_PER_BELT_SIDE * BELT_SIDES)

#define TARGET_none    0x7fff
#define TARGET_unknown 0x7ffe

typedef struct
{
   uint8 type;
} beltside_item;

typedef struct
{
   uint8 x_off, y_off, z_off; // 3 bytes             // 5,5,3
   uint8 dir  ;               // 1 bytes             // 2
   uint8 len:6;               // 1 bytes             // 5
   int8  turn:2;              // 0 bytes             // 2
   uint8 target_is_neighbor:1;// 1 bytes             // 1
   uint8 mark:2;              // 0 bytes             // 2
   int8  end_dz:2;            // 0 bytes             // 2
   int8  input_dz:2;          // 0 bytes             // 2
   uint8 mobile_slots[2];     // 2 bytes             // 7,7
   uint8 last_slot_filled_next_tick[2];  // 2 bytes  // 1,1
   uint16 target_id;          // 2 bytes             // 16
   uint16 input_id;
   beltside_item *items;      // 4 bytes
} belt_run; // 16 bytes

typedef union
{
   struct {
      uint16 x:6;
      uint16 y:6;
      uint16 z:4;
   } unpacked;
   uint16 packed;
} logi_chunk_coord;

typedef struct
{
   logi_chunk_coord pos; // 2 bytes
   uint8 type;           // 1 byte
   uint8 timer;          // 1 byte

   uint8 output;         // 1 byte
   uint8 config;         // 1 byte
   uint8 uses;           // 1 byte
   uint8 input_flags:6;  // 1 byte
   uint8 rot:2;          // 0 bytes
} machine_info;  // 8 bytes

// 32 * 32 * 8 => 2^(5+5+3) = 2^13
typedef struct
{
   logi_chunk_coord pos;
   uint8 type;
   uint8 rot;
   uint8 state;
   uint8 item;

   uint8 input_is_belt:1;
   uint8 output_is_belt:1;
   uint16 input_id;
   uint16 output_id;
} picker_info; // 12

typedef struct
{
   logi_chunk_coord pos;
   uint16 count;
} ore_info;

enum
{
   M_unmarked,
   M_temporary,
   M_permanent
};

typedef struct
{
   uint8 type[LOGI_CHUNK_SIZE_Z][LOGI_CHUNK_SIZE_Y][LOGI_CHUNK_SIZE_X];
   uint8 rot[LOGI_CHUNK_SIZE_Z][LOGI_CHUNK_SIZE_Y][LOGI_CHUNK_SIZE_X];
   belt_run *belts; // stb_arr 
   machine_info *machine;
   picker_info *pickers;
   ore_info *ore;
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




//   HUGE HACK to keep track of ore for now

stb_define_hash_base(STB_noprefix, stb_uidict, STB_nofields, stb_uidict_,stb_uidict_,0.85f,
              uint32,0,1,STB_nocopy,STB_nodelete,STB_nosafe,
              STB_equal,STB_equal,
              return stb_rehash_improved(k);,
              void *,STB_nullvalue,NULL)

stb_uidict *uidict;

typedef struct
{
   uint8 z1,z2,type,padding;
} ore_hack_info;

ore_hack_info ore_hack[500000];
ore_hack_info *next_ohi = ore_hack;

extern SDL_mutex *ore_update_mutex;
void logistics_record_ore(int x, int y, int z1, int z2, int type)
{
   ore_hack_info *ohi;
   SDL_LockMutex(ore_update_mutex);
   if (uidict == NULL) uidict = stb_uidict_create();

   ohi = stb_uidict_get(uidict, y*65536+x);
   if (ohi == NULL) {
      ohi = next_ohi++;//malloc(sizeof(*ohi));
      ohi->z1 = z1;
      ohi->z2 = z2;
      ohi->type = type;
      ohi->padding = 0;
      stb_uidict_add(uidict, y*65536+x, ohi);
   }
   SDL_UnlockMutex(ore_update_mutex);
}


//   4K x 4K

//     4K/32 => (2^12 / 2^5) => 2^7
//
//  2^7 * 2^7 * 2^5 * 2^2 => 2^(7+7+5+2) = 2^21 = 2MB

static logi_slice logi_world[LOGI_CACHE_SIZE][LOGI_CACHE_SIZE];

logi_chunk *logistics_get_chunk(int x, int y, int z, logi_slice **r_s)
{
   int slice_x = x >> LOGI_CHUNK_SIZE_X_LOG2;
   int slice_y = y >> LOGI_CHUNK_SIZE_Y_LOG2;
   int chunk_z = z >> LOGI_CHUNK_SIZE_Z_LOG2;
   logi_slice *s = &logi_world[slice_y & (LOGI_CACHE_SIZE-1)][slice_x & (LOGI_CACHE_SIZE-1)];
   if (s->slice_x != slice_x || s->slice_y != slice_y)
      return 0;
   if (r_s != NULL) *r_s = s;
   return s->chunk[chunk_z];
}

static void logistics_free_chunk(logi_chunk *c)
{
   stb_arr_free(c->machine);
   stb_arr_free(c->ore);
   stb_arr_free(c->pickers);
   stb_arr_free(c->belts);
   free(c);
}

static void logistics_free_slice(logi_slice *s, int x, int y)
{
   int i;
   for (i=0; i < stb_arrcount(s->chunk); ++i)
      if (s->chunk[i] != NULL)
         logistics_free_chunk(s->chunk[i]);
   memset(s, 0, sizeof(*s));
   s->slice_x = x+1;
}

static void logistics_create_slice(logi_slice *s, int x, int y)
{
   memset(s, 0, sizeof(*s));
   s->slice_x = x;
   s->slice_y = y;
}

static logi_chunk *logistics_get_chunk_alloc(int x, int y, int z)
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
      SDL_LockMutex(ore_update_mutex);
      if (uidict != NULL)
      {
         int bx = slice_x << LOGI_CHUNK_SIZE_X_LOG2;
         int by = slice_y << LOGI_CHUNK_SIZE_Y_LOG2;
         int bz = chunk_z << LOGI_CHUNK_SIZE_Z_LOG2;
         int i,j,k;
         for (j=0; j < LOGI_CHUNK_SIZE_Y; ++j) {
            for (i=0; i < LOGI_CHUNK_SIZE_X; ++i) {
               ore_hack_info *ohi = stb_uidict_get(uidict, (y+j)*65536+(x+i));
               if (ohi != NULL) {
                  if (ohi->z1 < bz+LOGI_CHUNK_SIZE_Z && ohi->z2 > bz) {
                     for (k=ohi->z1; k <= ohi->z2; ++k)
                        if (k >= bz && k < bz+LOGI_CHUNK_SIZE_Z)
                           c->type[k-bz][y-by][x-bx] = ohi->type;
                  }
               }
            }
         }
      }
      SDL_UnlockMutex(ore_update_mutex);
   }
   return c;
}

static void compute_mobile_slots(belt_run *br)
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

static void split_belt_raw(belt_run *a, belt_run *b, int num_a)
{
   assert(a->end_dz==0);
   assert(num_a <= a->len);
   b->len = a->len - num_a;
   a->len = num_a;

   b->items = NULL;
   stb_arr_setlen(b->items, b->len * ITEMS_PER_BELT);
   memcpy(b->items,
          a->items + a->len*ITEMS_PER_BELT,
          b->len * ITEMS_PER_BELT * sizeof(b->items[0]));
   stb_arr_setlen(a->items, a->len * ITEMS_PER_BELT);
}

int does_belt_intersect(belt_run *a, int x, int y, int z)
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
   for (i=0; i < stb_arr_len(c->belts); ++i) {
      belt_run *a = &c->belts[i];
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


      e = &c->belts[i];
      assert(pos >= 0 && pos < e->len);
      assert(e->x_off + face_dir[e->dir][0]*pos == x);
      assert(e->y_off + face_dir[e->dir][1]*pos == y);
      assert(e->end_dz==0);

      if (e->len == 1)
         return;
      e->target_id = TARGET_unknown;

      f.dir = e->dir;
      f.x_off = x;
      f.y_off = y;
      f.z_off = z;
      f.target_id = TARGET_unknown;
      f.input_id = TARGET_none;
      f.end_dz = 0;
      f.turn = 0;

      split_belt_raw(e, &f, pos);
      assert(e->x_off + face_dir[e->dir][0]*e->len == f.x_off);
      assert(e->y_off + face_dir[e->dir][1]*e->len == f.y_off);

      g.dir = f.dir;
      g.x_off = x + face_dir[dir][0];
      g.y_off = y + face_dir[dir][1];
      g.z_off = z;
      g.target_id = TARGET_unknown;
      g.input_id = TARGET_none;
      g.end_dz = 0;
      g.turn = 0;

      assert(f.len >= 1);

      split_belt_raw(&f, &g, 1);
      if (g.len != 0) {
         assert(f.x_off + face_dir[f.dir][0]*f.len == g.x_off);
         assert(f.y_off + face_dir[f.dir][1]*f.len == g.y_off);
      }

      assert(f.len == 1);

      compute_mobile_slots(e);
      compute_mobile_slots(&f);
      compute_mobile_slots(&g);

      if (e->len == 0)
         *e = f;
      else
         stb_arr_push(c->belts, f);

      if (g.len != 0)
         stb_arr_push(c->belts, g);
   }
}

void destroy_belt_raw(logi_chunk *c, int i)
{
   belt_run *b = &c->belts[i];
   stb_arr_free(b->items);
   stb_arr_fastdelete(c->belts, i);
}

void destroy_belt(logi_chunk *c, int x, int y, int z)
{
   int i;
   for (i=0; i < stb_arr_len(c->belts); ++i) {
      belt_run *b = &c->belts[i];
      if (b->x_off == x && b->y_off == y && b->z_off == z) {
         assert(b->len == 1);
         destroy_belt_raw(c, i);
         return;
      }
   }
}

int merge_run(logi_chunk *c, int i, int j)
{
   belt_run *a = &c->belts[i];
   belt_run *b = &c->belts[j];
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
   a->input_id = TARGET_none;

   // delete b
   destroy_belt_raw(c, j);

   // return the merged belt
   if (i == stb_arr_len(c->belts)) // did i get swapped down over j?
      return j;
   else
      return i;
}

void create_ramp(logi_chunk *c, int x, int y, int z, int dir, int dz)
{
   belt_run nb;
   nb.x_off = x;
   nb.y_off = y;
   nb.z_off = z;
   nb.len = 2;
   nb.dir = dir;
   nb.turn = 0;
   nb.items = NULL;
   nb.end_dz = dz;
   nb.input_id = TARGET_none;
   nb.target_id = TARGET_unknown;
   stb_arr_setlen(nb.items, ITEMS_PER_BELT*nb.len);
   memset(nb.items, 0, sizeof(nb.items[0]) * stb_arr_len(nb.items));
   stb_arr_push(c->belts, nb);
}

void destroy_ramp_or_turn(logi_chunk *c, int x, int y, int z)
{
   int i;
   for (i=0; i < stb_arr_len(c->belts); ++i) {
      if (c->belts[i].x_off == x && c->belts[i].y_off == y && c->belts[i].z_off == z) {
         assert(c->belts[i].end_dz != 0 || c->belts[i].turn != 0);
         destroy_belt_raw(c,i);
         return;
      }
   }
}

void create_turn(logi_chunk *c, int x, int y, int z, int dir, int turn)
{
   belt_run nb;
   nb.x_off = x;
   nb.y_off = y;
   nb.z_off = z;
   nb.len = 1;
   nb.dir = dir;
   nb.turn = turn;
   nb.items = NULL;
   nb.end_dz = 0;
   nb.input_id = TARGET_none;
   nb.target_id = TARGET_unknown;
   stb_arr_setlen(nb.items, ITEMS_PER_BELT*nb.len);
   memset(nb.items, 0, sizeof(nb.items[0]) * stb_arr_len(nb.items));
   stb_arr_push(c->belts, nb);
}

// if there is already a belt there, it is one-long
void create_belt(logi_chunk *c, int x, int y, int z, int dir)
{
   int i, j;
   belt_run *a;
   for (i=0; i < stb_arr_len(c->belts); ++i)
      if (c->belts[i].x_off == x && c->belts[i].y_off == y && c->belts[i].z_off == z)
         break;
   if (i == stb_arr_len(c->belts)) {
      belt_run nb;
      nb.x_off = x;
      nb.y_off = y;
      nb.z_off = z;
      nb.len = 1;
      nb.dir = dir;
      nb.items = NULL;
      nb.end_dz = 0;
      nb.turn = 0;
      stb_arr_setlen(nb.items, 8);
      memset(nb.items, 0, sizeof(nb.items[0]) * stb_arr_len(nb.items));
      stb_arr_push(c->belts, nb);
   } else
      c->belts[i].dir = dir;

   // now i refers to a 1-long belt; check for other belts to merge
   a = &c->belts[i];

   for (j=0; j < stb_arr_len(c->belts); ++j) {
      if (j != i) {
         belt_run *b = &c->belts[j];
         if (b->end_dz == 0 && b->turn == 0 && b->dir == dir && b->z_off == a->z_off) {
            if (b->x_off + face_dir[dir][0]*b->len == a->x_off && b->y_off + face_dir[dir][1]*b->len == a->y_off) {
               i = merge_run(c, j, i);
               break;
            }
         }
      }
   }
   a = &c->belts[i];

   for (j=0; j < stb_arr_len(c->belts); ++j) {
      if (j != i) {
         belt_run *b = &c->belts[j];
         if (b->end_dz == 0 && b->turn == 0 && b->dir == dir && b->z_off == a->z_off) {
            if (a->x_off + face_dir[dir][0]*a->len == b->x_off && a->y_off + face_dir[dir][1]*a->len == b->y_off) {
               i = merge_run(c, i, j);
               break;  
            }
         }
      }
   }

   c->belts[i].target_id = TARGET_unknown;
   c->belts[i].input_id = TARGET_none;
   compute_mobile_slots(&c->belts[i]);
}

static logi_chunk_coord coord(int x, int y, int z)
{
   logi_chunk_coord lcc;
   lcc.unpacked.x = x;
   lcc.unpacked.y = y;
   lcc.unpacked.z = z;
   return lcc;
}

void create_picker(logi_chunk *c, int ox, int oy, int oz, int type, int rot)
{
   picker_info pi = { 0 };
   pi.pos = coord(ox,oy,oz);
   pi.type = type;
   pi.rot = rot;
   stb_arr_push(c->pickers, pi);
}

int find_picker(logi_chunk *c, int ox, int oy, int oz)
{
   int i;
   logi_chunk_coord sc = coord(ox,oy,oz);
   for (i=0; i < stb_arr_len(c->pickers); ++i)
      if (c->pickers[i].pos.packed == sc.packed)
         return i;
   return -1;
}

void destroy_picker(logi_chunk *c, int ox, int oy, int oz)
{
   int n = find_picker(c, ox,oy,oz);
   if (n >= 0)
      stb_arr_fastdelete(c->pickers, n);
}

int find_ore(logi_chunk *c, int ox, int oy, int oz)
{
   int i;
   logi_chunk_coord sc = coord(ox,oy,oz);
   for (i=0; i < stb_arr_len(c->ore); ++i)
      if (c->ore[i].pos.packed == sc.packed)
         return i;
   return -1;
}

void create_machine(logi_chunk *c, int ox, int oy, int oz, int type, int rot, int bx, int by, int bz)
{
   int ore_z;
   logi_chunk *d;
   machine_info mi = { 0 };
   mi.pos  = coord(ox,oy,oz);
   mi.type = type;
   mi.rot = rot;
   stb_arr_push(c->machine, mi);

   d = logistics_get_chunk_alloc(bx+ox,by+oy,bz+oz-1);
   ore_z = (oz-1) & (LOGI_CHUNK_SIZE_Z-1);
   if (d->type[ore_z][oy][ox] == BT_marble) {
      int id = find_ore(d, ox,oy,ore_z);
      if (id < 0) {
         ore_info ore;
         ore.pos = coord(ox,oy,ore_z);
         ore.count = 5;
         stb_arr_push(d->ore, ore);
      }
   }
}

int find_machine(logi_chunk *c, int ox, int oy, int oz)
{
   int i;
   logi_chunk_coord sc;
   sc.unpacked.x = ox;
   sc.unpacked.y = oy;
   sc.unpacked.z = oz;
   for (i=0; i < stb_arr_len(c->machine); ++i)
      if (c->machine[i].pos.packed == sc.packed)
         return i;
   return -1;
}

void destroy_machine(logi_chunk *c, int ox, int oy, int oz)
{
   int n = find_machine(c, ox,oy,oz);
   if (n >= 0)
      stb_arr_fastdelete(c->machine, n);
}

static int get_belt_id_noramp(int x, int y, int z)
{
   int j;

   logi_chunk *c = logistics_get_chunk(x,y,z, NULL);

   int ox = x & (LOGI_CHUNK_SIZE_X-1);
   int oy = y & (LOGI_CHUNK_SIZE_Y-1);
   int oz = z & (LOGI_CHUNK_SIZE_Z-1);

   if (c != NULL)
      for (j=0; j < stb_arr_len(c->belts); ++j)
         if (c->belts[j].end_dz == 0 && c->belts[j].turn == 0)
            if (does_belt_intersect(&c->belts[j], ox,oy,oz))
               return j;
   return -1;
}

static int get_machine_id(int x, int y, int z)
{
   logi_chunk *c = logistics_get_chunk(x,y,z, NULL);

   int ox = x & (LOGI_CHUNK_SIZE_X-1);
   int oy = y & (LOGI_CHUNK_SIZE_Y-1);
   int oz = z & (LOGI_CHUNK_SIZE_Z-1);

   if (c)
      return find_machine(c, ox,oy,oz);
   else
      return -1;
}

void logistics_update_chunk(int x, int y, int z)
{
   logi_chunk *c = logistics_get_chunk(x,y,z,0);
   if (c != NULL) {
      int base_x = x & ~(LOGI_CHUNK_SIZE_X-1);
      int base_y = y & ~(LOGI_CHUNK_SIZE_Y-1);
      int base_z = z & ~(LOGI_CHUNK_SIZE_Z-1);
      int i,j;
      for (i=0; i < stb_arr_len(c->belts); ++i) {
         if (1) { //c->belts[i].target_id == TARGET_unknown) {
            logi_chunk *d = c;
            belt_run *b = &c->belts[i];
            int outdir = (b->dir + b->turn)&3;
            int ex = b->x_off + b->len * face_dir[outdir][0];
            int ey = b->y_off + b->len * face_dir[outdir][1];
            int ez = b->z_off + b->end_dz, is_neighbor=0;
            if (ex < 0 || ey < 0 || ex >= LOGI_CHUNK_SIZE_X || ey >= LOGI_CHUNK_SIZE_Y || ez < 0 || ez >= LOGI_CHUNK_SIZE_Z) {
               d = logistics_get_chunk(base_x + ex, base_y + ey, base_z + ez,0);
               ex = LOGI_CHUNK_MASK_X(ex);
               ey = LOGI_CHUNK_MASK_Y(ey);
               b->target_is_neighbor = 1;
            } else
               b->target_is_neighbor = 0;

            if (d != NULL) {
               for (j=0; j < stb_arr_len(d->belts); ++j) {
                  // don't feed from conveyor/ramp that points to side of ramp
                  // and don't feed to side of turn
                  if (d->belts[j].dir == outdir || (d->belts[j].turn==0 && d->belts[j].end_dz==0)) {
                     if (does_belt_intersect(&d->belts[j], ex,ey,ez)) {
                        b->target_id = j;
                        if (d->belts[j].dir == outdir) {
                           d->belts[j].input_id = i;
                           d->belts[j].input_dz = b->end_dz;
                        }
                        break;
                     }
                  }
               }
               if (j == stb_arr_len(d->belts)) {
                  b->target_id = TARGET_none;
               }
            } else
               b->target_id = TARGET_none;
         }
      }
      {
         int i;
         for (i=0; i < stb_arr_len(c->belts); ++i)
            assert(c->belts[i].target_id != TARGET_unknown);
      }

      for (i=0; i < stb_arr_len(c->pickers); ++i) {
         picker_info *p = &c->pickers[i];
         int ix = base_x + p->pos.unpacked.x + face_dir[p->rot  ][0];
         int iy = base_y + p->pos.unpacked.y + face_dir[p->rot  ][1];
         int ox = base_x + p->pos.unpacked.x + face_dir[p->rot^2][0];
         int oy = base_y + p->pos.unpacked.y + face_dir[p->rot^2][1];
         int id;

         id = get_belt_id_noramp(ix,iy, base_z + p->pos.unpacked.z-1);
         if (id >= 0) {
            p->input_id = id;
            p->input_is_belt = True;
         } else {
            id = get_machine_id(ix, iy, base_z + p->pos.unpacked.z);
            p->input_id = id >= 0 ? id : TARGET_none;
            p->input_is_belt = False;
         }

         id = get_belt_id_noramp(ox, oy, base_z + p->pos.unpacked.z-1);
         if (id >= 0) {
            p->output_id = id;
            p->output_is_belt = True;
         } else {
            id = get_machine_id(ox, oy, base_z + p->pos.unpacked.z);
            p->output_id = id >= 0 ? id : TARGET_none;
            p->output_is_belt = False;
         } 
      }

      for (i=0; i < stb_arr_len(c->machine); ++i) {
         machine_info *m = &c->machine[i];
         if (m->type == BT_ore_drill) {
            int ore_z = base_z + m->pos.unpacked.z - 1;
            logi_chunk *d = logistics_get_chunk(base_x - m->pos.unpacked.x, base_y + m->pos.unpacked.y, ore_z, NULL);
            if (d && d->type[ore_z & (LOGI_CHUNK_SIZE_Z-1)][m->pos.unpacked.y][m->pos.unpacked.x] == BT_marble) {
               m->input_flags = 1;
            } else
               m->input_flags = 0;
         }
      }
   }
}

#define IS_RAMP_HEAD(x) \
   (    ((x) == BT_conveyor_ramp_up_low    )    \
     || ((x) == BT_conveyor_ramp_down_high )   )

//
//  
//  /

void logistics_update_block_core(int x, int y, int z, int type, int rot)
{
   logi_chunk *c = logistics_get_chunk_alloc(x,y,z);
   int ox = LOGI_CHUNK_MASK_X(x);
   int oy = LOGI_CHUNK_MASK_Y(y);
   int oz = LOGI_CHUNK_MASK_Z(z);
   int oldtype = c->type[oz][oy][ox];

   if (oldtype == BT_conveyor)
      split_belt(c, ox,oy,oz, c->rot[oz][oy][ox]);

   if (oldtype >= BT_picker && oldtype <= BT_picker)
      destroy_picker(c, ox,oy,oz);
   else if (oldtype >= BT_machines)
      destroy_machine(c, ox,oy,oz);
   else if (oldtype == BT_conveyor && type != BT_conveyor)
      destroy_belt(c, ox,oy,oz);

   c->type[oz][oy][ox] = type;
   c->rot[oz][oy][ox] = rot;

   if (type == BT_conveyor)
      create_belt(c, ox,oy,oz, rot);

   if (type >= BT_picker && type <= BT_picker)
      create_picker(c, ox,oy,oz, type,rot);
   else if (type >= BT_machines)
      create_machine(c, ox,oy,oz, type, rot, x-ox,y-oy,z-oz);

   if (type == BT_conveyor_90_left || type == BT_conveyor_90_right) {
      if (oldtype == BT_conveyor_90_left || oldtype == BT_conveyor_90_right) {
         // @TODO changing turn types
         destroy_ramp_or_turn(c,ox,oy,oz);
         create_turn(c, ox,oy,oz, rot, type == BT_conveyor_90_left ? 1 : -1);
      } else {
         create_turn(c, ox,oy,oz, rot, type == BT_conveyor_90_left ? 1 : -1);
      }
   } else if (oldtype == BT_conveyor_90_left || oldtype == BT_conveyor_90_right) {
      destroy_ramp_or_turn(c, ox,oy,oz);
   }

   if (IS_RAMP_HEAD(type)) {
      if (IS_RAMP_HEAD(oldtype)) {
         // @TODO changing ramp types
         destroy_ramp_or_turn(c, ox,oy,oz);
         create_ramp(c, ox,oy,oz, rot, type == BT_conveyor_ramp_up_low ? 1 : -1);
      } else {
         create_ramp(c, ox,oy,oz, rot, type == BT_conveyor_ramp_up_low ? 1 : -1);
      }
   } else if (IS_RAMP_HEAD(oldtype)) {
      destroy_ramp_or_turn(c, ox,oy,oz);
   }

   logistics_update_chunk(x,y,z);
   logistics_update_chunk(x - LOGI_CHUNK_SIZE_X, y, z);
   logistics_update_chunk(x + LOGI_CHUNK_SIZE_X, y, z);
   logistics_update_chunk(x, y - LOGI_CHUNK_SIZE_Y, z);
   logistics_update_chunk(x, y + LOGI_CHUNK_SIZE_Y, z);
   logistics_update_chunk(x, y, z - LOGI_CHUNK_SIZE_Z);
   logistics_update_chunk(x, y, z + LOGI_CHUNK_SIZE_Z);

   logistics_update_chunk(x - LOGI_CHUNK_SIZE_X, y, z - LOGI_CHUNK_SIZE_Z);
   logistics_update_chunk(x - LOGI_CHUNK_SIZE_X, y, z + LOGI_CHUNK_SIZE_Z);
   logistics_update_chunk(x + LOGI_CHUNK_SIZE_X, y, z - LOGI_CHUNK_SIZE_Z);
   logistics_update_chunk(x + LOGI_CHUNK_SIZE_X, y, z + LOGI_CHUNK_SIZE_Z);
   logistics_update_chunk(x, y - LOGI_CHUNK_SIZE_Y, z - LOGI_CHUNK_SIZE_Z);
   logistics_update_chunk(x, y - LOGI_CHUNK_SIZE_Y, z + LOGI_CHUNK_SIZE_Z);
   logistics_update_chunk(x, y + LOGI_CHUNK_SIZE_Y, z - LOGI_CHUNK_SIZE_Z);
   logistics_update_chunk(x, y + LOGI_CHUNK_SIZE_Y, z + LOGI_CHUNK_SIZE_Z);

   if (oldtype == BT_down_marker)
      logistics_update_block_core(x,y,z-1,BT_empty,0);
}

void logistics_update_block(int x, int y, int z, int type, int rot)
{
   if (type == BT_conveyor_ramp_up_low) {
      logistics_update_block_core(x,y,z, BT_down_marker, 0);
      logistics_update_block_core(x,y,z-1,type,rot);
   } else
      logistics_update_block_core(x,y,z,type,rot);
}

int face_orig[4][2] = {
   { 0,0 },
   { 1,0 },
   { 1,1 },
   { 0,1 },
};

static uint32 logistics_ticks;
static uint32 logistics_long_tick;
#define LONG_TICK_LENGTH   12

#pragma warning(disable:4244)
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

static int get_interaction_pos(belt_run *b, int x, int y, int z)
{
   x = x & (LOGI_CHUNK_SIZE_X-1);
   y = y & (LOGI_CHUNK_SIZE_Y-1);
   z = z & (LOGI_CHUNK_SIZE_Z-1);
   return abs(x-b->x_off) + abs(y-b->y_off);
}

static belt_run *get_interaction_belt(int x, int y, int z, int facing, int *pos)
{
   int j;

   int ex = x + face_dir[facing][0];
   int ey = y + face_dir[facing][1];
   int ez = z;

   logi_slice *s;
   logi_chunk *c = logistics_get_chunk(ex,ey,ez, &s);

   int ox = ex & (LOGI_CHUNK_SIZE_X-1);
   int oy = ey & (LOGI_CHUNK_SIZE_Y-1);
   int oz = ez & (LOGI_CHUNK_SIZE_Z-1);

   for (j=0; j < stb_arr_len(c->belts); ++j) {
      if (c->belts[j].end_dz == 0) {
         if (does_belt_intersect(&c->belts[j], ox,oy,oz)) {
            ox = s->slice_x * LOGI_CHUNK_SIZE_X + c->belts[j].x_off;
            oy = s->slice_y * LOGI_CHUNK_SIZE_Y + c->belts[j].y_off;
            oz = (z & ~(LOGI_CHUNK_SIZE_Z-1)) + c->belts[j].z_off;
            *pos = abs(ex - ox) + abs(ey - oy);
            return &c->belts[j];
         }
      }
   }

   return NULL;
}

typedef struct
{
   int off_x, off_y, off_z;
   logi_chunk *c;
   logi_slice *s;
   int cid;
} target_chunk;

// return off_x, off_y, off_z relative to bx,by,bz
static void get_target_chunk(target_chunk *tc, int bx, int by, int bz, logi_chunk *c, belt_run *br)
{
   if (1 || br->target_is_neighbor) {
      int outdir = (br->dir + br->turn) & 3;
      int ex = bx + br->x_off + br->len * face_dir[outdir][0];
      int ey = by + br->y_off + br->len * face_dir[outdir][1];
      int ez = bz + br->z_off + br->end_dz;
      logi_chunk *c = logistics_get_chunk(ex,ey,ez, &tc->s);
      tc->cid = ez >> LOGI_CHUNK_SIZE_Z_LOG2;
      tc->c = c;
      tc->off_x = (ex & ~(LOGI_CHUNK_SIZE_X-1)) - bx;
      tc->off_y = (ey & ~(LOGI_CHUNK_SIZE_Y-1)) - by;
      tc->off_z = (ez & ~(LOGI_CHUNK_SIZE_Z-1)) - bz;
   }
}

static void get_input_chunk(target_chunk *tc, int bx, int by, int bz, logi_chunk *c, belt_run *br)
{
   int ex = bx + br->x_off - face_dir[br->dir][0];
   int ey = by + br->y_off - face_dir[br->dir][1];
   int ez = bz + br->z_off - br->input_dz;
   logi_chunk *nc = logistics_get_chunk(ex,ey,ez, &tc->s);
   tc->c = nc;
   tc->cid = ez >> LOGI_CHUNK_SIZE_Z_LOG2;
}

#define BELT_SLOT_IS_EMPTY_NEXT_TICK(b,slot) \
      ((slot)-2 >= 0 ? b->items[(slot)-2].type == 0 && (slot)-2 < 2*b->mobile_slots[(slot)&1] : !tb->last_slot_filled_next_tick[(slot)&1])

#define SHORT_SIDE  1
#define LONG_SIDE   5

void logistics_belt_turn_tick(logi_slice *s, int cid, belt_run *br)
{
   int j;
   int force_mobile[2] = { 0,0 };
   logi_chunk *c = s->chunk[cid];
   int allow_new_frontmost_to_move[2] = { 0,0 };
   int left_start, left_len, right_start, right_len, right_end, left_end;
   int outdir = (br->dir + br->turn) & 3;

   right_start = 0;
   if (br->turn > 0) {
      right_len = LONG_SIDE;
      left_len = SHORT_SIDE;
   } else {
      right_len = SHORT_SIDE;
      left_len = LONG_SIDE;
   }
   left_start = right_len;
   right_end = right_start+right_len;
   left_end = left_start + left_len;

   if (br->target_id != TARGET_none) {
      int relative_facing;
      target_chunk tc;
      belt_run *tb;
      get_target_chunk(&tc, s->slice_x * LOGI_CHUNK_SIZE_X, s->slice_y * LOGI_CHUNK_SIZE_Y, cid * LOGI_CHUNK_SIZE_Z, c, br);
      assert(tc.c != NULL);
      tb = &tc.c->belts[br->target_id];
      relative_facing = (tb->dir - outdir) & 3;
      switch (relative_facing) {
         case 0: {
            int target_left_start = tb->turn ? (tb->turn > 0 ? LONG_SIDE : SHORT_SIDE) : 1;
            int target_right_start = 0;

            // @TODO handle outputting to turning belt
            // conveyor continues straight ahead
            if (tb->items[target_right_start].type == 0) {
               tb->items[target_right_start] = br->items[right_end-1];
               br->items[right_end-1].type = 0;
            }
            if (tb->items[target_left_start].type == 0) {
               tb->items[target_left_start] = br->items[left_end-1];
               br->items[left_end-1].type = 0;
            }
            force_mobile[0] = (tb->mobile_slots[0] > 0);
            force_mobile[1] = (tb->mobile_slots[1] > 0);

            allow_new_frontmost_to_move[0] = !tb->last_slot_filled_next_tick[0]; // (tb->mobile_slots[0] > 0);
            allow_new_frontmost_to_move[1] = !tb->last_slot_filled_next_tick[1]; // (tb->mobile_slots[0] > 0);
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

            int ex = br->x_off + face_dir[outdir][0];
            int ey = br->y_off + face_dir[outdir][1];
            // offset of ex/ey point on belt is just manhattan distance from ex/ey point from start
            int pos = abs(ex - (tc.off_x + tb->x_off)) + abs(ey - (tc.off_y + tb->y_off));
            int itembase = pos * ITEMS_PER_BELT_SIDE;
            int itempos;
            itempos = (itembase + (ITEMS_PER_BELT_SIDE/4-1))*2+1;

            if (br->items[right_end-1].type != 0) {
               assert(itempos < stb_arr_len(tb->items));
               if (tb->items[itempos].type == 0) {
                  tb->items[itempos] = br->items[right_end-1];
                  br->items[right_end-1].type = 0;
               }
            }
            if (BELT_SLOT_IS_EMPTY_NEXT_TICK(tb,itempos)) 
               allow_new_frontmost_to_move[0] = 1;

            itempos = (itembase + (ITEMS_PER_BELT_SIDE-1))*2+1;
            if (br->items[left_end-1].type != 0) {
               if (tb->items[itempos].type == 0) {
                  tb->items[itempos] = br->items[left_end-1];
                  br->items[left_end-1].type = 0;
               }
            }
            if (BELT_SLOT_IS_EMPTY_NEXT_TICK(tb,itempos)) 
               allow_new_frontmost_to_move[1] = 1;
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
            int ex = br->x_off + face_dir[outdir][0];
            int ey = br->y_off + face_dir[outdir][1];
            // offset of ex/ey point on belt is just manhattan distance from ex/ey point from start
            int pos = abs(ex - (tc.off_x + tb->x_off)) + abs(ey - (tc.off_y + tb->y_off));
            int itembase = pos * ITEMS_PER_BELT_SIDE;
            int itempos = (itembase + (ITEMS_PER_BELT_SIDE-1))*2+0;

            if (br->items[left_end-1].type != 0) {
               assert(itempos < stb_arr_len(tb->items));
               if (tb->items[itempos].type == 0) {
                  tb->items[itempos] = br->items[left_end-1];
                  br->items[left_end-1].type = 0;
               }
            }

            if (BELT_SLOT_IS_EMPTY_NEXT_TICK(tb,itempos)) 
               allow_new_frontmost_to_move[0] = 1;

            itempos = (itembase + (ITEMS_PER_BELT_SIDE/4-1))*2+0;
            if (br->items[right_end-1].type != 0) {
               if (tb->items[itempos].type == 0) {
                  tb->items[itempos] = br->items[right_end-1];
                  br->items[right_end-1].type = 0;
               }
            }
            if (BELT_SLOT_IS_EMPTY_NEXT_TICK(tb,itempos)) 
               allow_new_frontmost_to_move[1] = 1;
            break;
         }
      }
   }

   br->last_slot_filled_next_tick[0] = 0;
   br->last_slot_filled_next_tick[1] = 0;

   // at this moment, item[len-2] has just animated all the way to the farthest
   // position that's still on the belt, i.e. it becomes item[len-1]

   for (j=right_end-2; j >= right_start; --j)
      if (br->items[j].type != 0 && br->items[j+1].type == 0) {
         br->items[j+1] = br->items[j];
         br->items[j].type = 0;
      }
   for (j=left_end-2; j >= left_start; --j)
      if (br->items[j].type != 0 && br->items[j+1].type == 0) {
         br->items[j+1] = br->items[j];
         br->items[j].type = 0;
      }

   // now, we must check if item[len-1] is allowed to move OFF the belt
   // over the next long tick

   if (allow_new_frontmost_to_move[0])
      br->mobile_slots[0] = right_len;
   else {
      for (j=right_len-1; j >= 0; --j)
         if (br->items[right_start+j].type == 0)
            break;
      br->mobile_slots[0] = j < 0 ? 0 : j;
      if (force_mobile[0]) br->mobile_slots[0] = right_len;
   }

   if (allow_new_frontmost_to_move[1])
      br->mobile_slots[1] = left_len;
   else {
      for (j=left_len-1; j >= 0; --j)
         if (br->items[left_start+j].type == 0)
            break;
      br->mobile_slots[1] = j < 0 ? 0 : j;
      if (force_mobile[1]) br->mobile_slots[1] = left_len;
   }

   if (br->mobile_slots[0] == 0 && br->items[right_start].type != 0)
      br->last_slot_filled_next_tick[0] = 1;
   if (br->mobile_slots[1] == 0 && br->items[left_start].type != 0)
      br->last_slot_filled_next_tick[1] = 1;
}

void logistics_belt_tick(logi_slice *s, int cid, belt_run *br)
{
   logi_chunk *c = s->chunk[cid];
   int j;
   int len;
   int force_mobile[2] = { 0,0 };
   int allow_new_frontmost_to_move[2] = { 0,0 };

   len = br->len * ITEMS_PER_BELT;
   if (global_hack == -1) {
      memset(br->items, 0, sizeof(br->items[0]) * len);
      //br->mobile_slots[0] = stb_arr_len(br->items)/2;
      //br->mobile_slots[1] = stb_arr_len(br->items)/2;
      return;
   }

   if (br->turn) {
      logistics_belt_turn_tick(s, cid, br);
      return;
   }

   // at this moment, item[len-1] has just animated all the way off of the belt (if mobile)
   // so, we move item[len-1] out to some other spot

   if (br->target_id != TARGET_none) {
      int turn;
      target_chunk tc;
      belt_run *tb;
      get_target_chunk(&tc, s->slice_x * LOGI_CHUNK_SIZE_X, s->slice_y * LOGI_CHUNK_SIZE_Y, cid * LOGI_CHUNK_SIZE_Z, c, br);
      assert(tc.c != NULL);
      tb = &tc.c->belts[br->target_id];
      turn = (tb->dir - br->dir) & 3;
      switch (turn) {
         case 0: {
            int target_left_start = tb->turn ? (tb->turn > 0 ? LONG_SIDE : SHORT_SIDE) : 1;
            int target_right_start = 0;
            // @TODO handle outputting to turning belt
            // conveyor continues straight ahead
            if (tb->items[target_right_start].type == 0) {
               tb->items[target_right_start] = br->items[len-2];
               br->items[len-2].type = 0;
            }
            if (tb->items[target_left_start].type == 0) {
               tb->items[target_left_start] = br->items[len-1];
               br->items[len-1].type = 0;
            }
            force_mobile[0] = (tb->mobile_slots[0] > 0);
            force_mobile[1] = (tb->mobile_slots[1] > 0);

            allow_new_frontmost_to_move[0] = !tb->last_slot_filled_next_tick[0]; // (tb->mobile_slots[0] > 0);
            allow_new_frontmost_to_move[1] = !tb->last_slot_filled_next_tick[1]; // (tb->mobile_slots[0] > 0);
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
            int itempos;
            itempos = (itembase + (ITEMS_PER_BELT_SIDE/4-1))*2+1;

            if (br->items[len-2].type != 0) {
               assert(itempos < stb_arr_len(tb->items));
               if (tb->items[itempos].type == 0) {
                  tb->items[itempos] = br->items[len-2];
                  br->items[len-2].type = 0;
               }
            }
            if (BELT_SLOT_IS_EMPTY_NEXT_TICK(tb,itempos)) 
               allow_new_frontmost_to_move[0] = 1;

            itempos = (itembase + (ITEMS_PER_BELT_SIDE-1))*2+1;
            if (br->items[len-1].type != 0) {
               if (tb->items[itempos].type == 0) {
                  tb->items[itempos] = br->items[len-1];
                  br->items[len-1].type = 0;
               }
            }
            if (BELT_SLOT_IS_EMPTY_NEXT_TICK(tb,itempos)) 
               allow_new_frontmost_to_move[1] = 1;
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
            int itempos = (itembase + (ITEMS_PER_BELT_SIDE-1))*2+0;

            if (br->items[len-2].type != 0) {
               assert(itempos < stb_arr_len(tb->items));
               if (tb->items[itempos].type == 0) {
                  tb->items[itempos] = br->items[len-2];
                  br->items[len-2].type = 0;
               }
            }

            if (BELT_SLOT_IS_EMPTY_NEXT_TICK(tb,itempos)) 
               allow_new_frontmost_to_move[0] = 1;

            itempos = (itembase + (ITEMS_PER_BELT_SIDE/4-1))*2+0;
            if (br->items[len-1].type != 0) {
               if (tb->items[itempos].type == 0) {
                  tb->items[itempos] = br->items[len-1];
                  br->items[len-1].type = 0;
               }
            }
            if (BELT_SLOT_IS_EMPTY_NEXT_TICK(tb,itempos)) 
               allow_new_frontmost_to_move[1] = 1;
            break;
         }
      }
   }
   len = br->len * ITEMS_PER_BELT_SIDE;

   br->last_slot_filled_next_tick[0] = 0;
   br->last_slot_filled_next_tick[1] = 0;

   // at this moment, item[len-2] has just animated all the way to the farthest
   // position that's still on the belt, i.e. it becomes item[len-1]

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

   // now, we must check if item[len-1] is allowed to move OFF the belt
   // over the next long tick


   if (allow_new_frontmost_to_move[0])
      br->mobile_slots[0] = len;
   else {
      for (j=len-1; j >= 0; --j)
         if (br->items[j*2+0].type == 0)
            break;
      br->mobile_slots[0] = j < 0 ? 0 : j;
      if (force_mobile[0]) br->mobile_slots[0] = len;
   }

   if (allow_new_frontmost_to_move[1])
      br->mobile_slots[1] = len;
   else {
      for (j=len-1; j >= 0; --j)
         if (br->items[j*2+1].type == 0)
            break;
      br->mobile_slots[1] = j < 0 ? 0 : j;
      if (force_mobile[1]) br->mobile_slots[1] = len;
   }

   if (br->mobile_slots[0] == 0 && br->items[0].type != 0)
      br->last_slot_filled_next_tick[0] = 1;
   if (br->mobile_slots[1] == 0 && br->items[1].type != 0)
      br->last_slot_filled_next_tick[1] = 1;

   if (global_hack) {
      if (stb_rand() % 10 < 2) {
         if (br->items[0].type == 0 && stb_rand() % 9 < 2) br->items[0].type = stb_rand() % 4;
         if (br->items[1].type == 0 && stb_rand() % 9 < 2) br->items[1].type = stb_rand() % 4;
      }
   }
}

typedef struct
{
   logi_slice *slice;
   uint16 belt_id;
   uint8 cid;
} belt_ref;

static belt_ref *sorted_ref;

static void visit(belt_ref *ref)
{
   logi_chunk *c = ref->slice->chunk[ref->cid];
   belt_run *br = &c->belts[ref->belt_id];
   if (br->mark == M_temporary) return;
   if (br->mark == M_unmarked) {
      br->mark = M_temporary;
      if (br->target_id != TARGET_none) {
         target_chunk tc;
         belt_ref target;
         get_target_chunk(&tc, ref->slice->slice_x * LOGI_CHUNK_SIZE_X, ref->slice->slice_y * LOGI_CHUNK_SIZE_Y, ref->cid * LOGI_CHUNK_SIZE_Z, c, br);
         target.belt_id = br->target_id;
         target.cid = tc.cid;
         target.slice = tc.s;
         visit(&target);
      }
      if (br->mark == M_temporary) {
         br->mark = M_permanent;
         stb_arr_push(sorted_ref, *ref);
      }

      {
         belt_ref target = *ref;
         while (br->input_id != TARGET_none) {
            target_chunk tc;
            get_input_chunk(&tc,  target.slice->slice_x * LOGI_CHUNK_SIZE_X, target.slice->slice_y * LOGI_CHUNK_SIZE_Y, target.cid * LOGI_CHUNK_SIZE_Z, c, br);
            target.belt_id = br->input_id;
            target.cid = tc.cid;
            target.slice = tc.s;
            c = tc.c;
            br = &c->belts[target.belt_id];
            if (br->mark == M_permanent)
               break;
            br->mark = M_permanent;
            stb_arr_push(sorted_ref, target);
         }
      }
   }
}

extern int selected_block[3];
extern int sort_order;

void remove_item_from_belt(belt_run *b, int slot)
{
   int side,pos;
   assert(slot < stb_arr_len(b->items));
   b->items[slot].type = 0;
   side = slot & 1;
   pos = slot >> 1;
   if (pos > b->mobile_slots[side])
      b->mobile_slots[side] = pos;
}

Bool try_remove_item_from_belt(belt_run *b, int slot, int type1, int type2)
{
   assert(slot < stb_arr_len(b->items));
   if (b->items[slot].type == type1 || b->items[slot].type == type2) {
      remove_item_from_belt(b, slot);
      return True;
   } else
      return False;
}

void add_item_to_belt(belt_run *b, int slot, int type)
{
   int side,pos;
   assert(slot < stb_arr_len(b->items));
   assert(b->items[slot].type == 0);
   b->items[slot].type = type;
   side = slot & 1;
   pos = slot >> 1;
   if (b->mobile_slots[side] == pos) {
      int j;
      for (j=pos-1; j >= 0; --j)
         if (b->items[j*2+side].type == 0)
            break;
      b->mobile_slots[side] = j < 0 ? 0 : j;
   }
}

Bool try_add_item_to_belt(belt_run *b, int slot, int type)
{
   assert(slot < stb_arr_len(b->items));
   if (b->items[slot].type == 0) {
      add_item_to_belt(b, slot, type);
      return True;
   } else
      return False;
}

belt_run *find_belt_slot_for_picker(int belt_id, int ix, int iy, int iz, int rot, int *slot)
{
   logi_chunk *d = logistics_get_chunk(ix, iy, iz, 0);
   belt_run *b = &d->belts[belt_id];
   int pos, side, dist;
   assert(belt_id < stb_arr_len(d->belts));
   pos = get_interaction_pos(b, ix,iy,iz);
   side = ((b->dir - rot)&3) == 1;
   dist = (b->dir == rot) ? 0 : ((b->dir^2) == rot) ? ITEMS_PER_BELT_SIDE-1 : 1;
   *slot = pos * ITEMS_PER_BELT + dist*BELT_SIDES + side;
   assert(*slot < stb_arr_len(b->items));
   return b;
}

void logistics_longtick_chunk_machines(logi_chunk *c, int base_x, int base_y, int base_z)
{
   int m;

   for (m=0; m < stb_arr_len(c->machine); ++m) {
      machine_info *x = &c->machine[m];
      Bool went_to_zero = False;
      if (x->timer) {
         --x->timer;
         went_to_zero = (x->timer == 0);
      }
      if (x->type == BT_ore_eater) {
         if (went_to_zero) {
            x->output = 0;
         }
         if (x->output != 0 && x->timer == 0)
            x->timer = 7;
      }
      if (x->type == BT_ore_drill) {
         if (went_to_zero) {
            assert(x->output == 0);
            x->output = 1 + (stb_rand() % 2);
         }
         if (x->timer == 0 && x->output == 0) {
            x->timer = 7; // start drilling
         }
      }
   }

   for (m=0; m < stb_arr_len(c->pickers); ++m) {
      picker_info *pi = &c->pickers[m];
      Bool went_to_zero = False;
      if (pi->state == 1)
         pi->state = 0;
      if (pi->state == 3)
         pi->state = 2;

      if (pi->state == 0) {
         if (pi->item == 0 && pi->input_id != TARGET_none && pi->state == 0) {
            int ix = base_x + pi->pos.unpacked.x + face_dir[pi->rot  ][0];
            int iy = base_y + pi->pos.unpacked.y + face_dir[pi->rot  ][1];
            int iz = base_z + pi->pos.unpacked.z;
            if (pi->input_is_belt) {
               int slot;
               belt_run *b = find_belt_slot_for_picker(pi->input_id, ix,iy,iz-1, pi->rot, &slot);
               if (b->items[slot].type != 0) {
                  pi->item = b->items[slot].type;
                  remove_item_from_belt(b, slot);
                  pi->state = 3;
               }
            } else {
               logi_chunk *d = logistics_get_chunk(ix, iy, base_z + pi->pos.unpacked.z, 0);
               machine_info *mi;
               assert(pi->input_id < stb_arr_len(d->machine));
               mi = &d->machine[pi->input_id];
               if (mi->output != 0) {
                  pi->item = mi->output;
                  mi->output = 0;
                  pi->state = 3;
               }
            }
         }
      } else if (pi->state == 2) {
         assert(pi->item != 0);
         if (pi->output_id != TARGET_none) {
            int ox = base_x + pi->pos.unpacked.x + face_dir[pi->rot^2][0];
            int oy = base_y + pi->pos.unpacked.y + face_dir[pi->rot^2][1];
            int oz = base_z + pi->pos.unpacked.z;
            assert(pi->item != 0);
            if (pi->output_is_belt) {
               int slot;
               belt_run *b = find_belt_slot_for_picker(pi->output_id, ox,oy,oz-1, pi->rot^2, &slot);
               if (b->items[slot].type == 0) {
                  add_item_to_belt(b, slot, pi->item);
                  pi->item = 0;
                  pi->state = 1;
               }
            } else {
               logi_chunk *d = logistics_get_chunk(ox, oy, base_z + pi->pos.unpacked.z, 0);
               machine_info *mi;
               assert(pi->output_id < stb_arr_len(d->machine));
               mi = &d->machine[pi->output_id];
               if (mi->output == 0) {
                  assert(pi->item);
                  mi->output = pi->item;
                  pi->item = 0;
                  pi->state = 1;
               }
            }
         }
      }
   }
}

void logistics_do_long_tick(void)
{
   int i,j,k,m;
   belt_ref *belts = NULL;

   for (j=0; j < LOGI_CACHE_SIZE; ++j) {
      for (i=0; i < LOGI_CACHE_SIZE; ++i) {
         logi_slice *s = &logi_world[j][i];
         if (s->slice_x != i+1) {
            for (k=0; k < stb_arrcount(s->chunk); ++k) {
               logi_chunk *c = s->chunk[k];
               if (c != NULL) {
                  for (m=0; m < stb_arr_len(c->belts); ++m) {
                     belt_ref br;
                     br.belt_id = m;
                     br.cid = k;
                     br.slice = s;
                     stb_arr_push(belts, br);
                     c->belts[m].mark = M_unmarked;
                  }
               }
            }
         }
      }
   }

   stb_arr_free(sorted_ref);
   sorted_ref = NULL;

   for (i=0; i < stb_arr_len(belts); ++i) {
      belt_run *br = &belts[i].slice->chunk[belts[i].cid]->belts[belts[i].belt_id];
      if (br->mark == M_unmarked) {
         visit(&belts[i]);
      }
   }

   sort_order = -1; // selected belt
   for (i=0; i < stb_arr_len(sorted_ref); ++i) {
      logi_slice *s = sorted_ref[i].slice;
      belt_run *br = &s->chunk[sorted_ref[i].cid]->belts[sorted_ref[i].belt_id];
      logistics_belt_tick(sorted_ref[i].slice, sorted_ref[i].cid, br);
      br->mark = M_unmarked;
      if (1) {
         int base_x = s->slice_x * LOGI_CHUNK_SIZE_X;
         int base_y = s->slice_y * LOGI_CHUNK_SIZE_Y;
         int base_z = sorted_ref[i].cid * LOGI_CHUNK_SIZE_Z;

         if (does_belt_intersect(br, selected_block[0]-base_x, selected_block[1]-base_y, selected_block[2]-base_z))
            sort_order = i;
      }
   }

   stb_arr_free(belts);

   for (j=0; j < LOGI_CACHE_SIZE; ++j) {
      for (i=0; i < LOGI_CACHE_SIZE; ++i) {
         logi_slice *s = &logi_world[j][i];
         if (s->slice_x != i+1) {
            int base_x = s->slice_x * LOGI_CHUNK_SIZE_X;
            int base_y = s->slice_y * LOGI_CHUNK_SIZE_Y;
            for (k=0; k < stb_arrcount(s->chunk); ++k) {
               int base_z = k * LOGI_CHUNK_SIZE_Z;
               logi_chunk *c = s->chunk[k];
               if (c != NULL) {
                  logistics_longtick_chunk_machines(c, base_x, base_y, base_z);
               }
            }
         }
      }
   }
}

extern int tex_anim_offset;
void logistics_tick(void)
{
   logistics_texture_scroll += (1.0f / LONG_TICK_LENGTH / ITEMS_PER_BELT_SIDE) / 4.0f; // texture repeats = 4
   if (logistics_texture_scroll >= 1.0)
      logistics_texture_scroll -= 1.0;

   ++logistics_ticks;
   tex_anim_offset = ((logistics_ticks >> 3) & 3) * 16;

   if (++logistics_long_tick == LONG_TICK_LENGTH) {
      logistics_do_long_tick();
      logistics_long_tick = 0;
   }
}

void logistics_render(void)
{
   float offset = (float) logistics_long_tick / LONG_TICK_LENGTH;// + stb_frand();
   int i,j,k,a,e;
   assert(offset >= 0 && offset <= 1);
   for (j=0; j < LOGI_CACHE_SIZE; ++j) {
      for (i=0; i < LOGI_CACHE_SIZE; ++i) {
         logi_slice *s = &logi_world[j][i];
         if (s->slice_x != i+1) {
            for (k=0; k < stb_arrcount(s->chunk); ++k) {
               logi_chunk *c = s->chunk[k];
               if (c != NULL) {
                  float base_x = (float) s->slice_x * LOGI_CHUNK_SIZE_X;
                  float base_y = (float) s->slice_y * LOGI_CHUNK_SIZE_Y;
                  float base_z = (float) k * LOGI_CHUNK_SIZE_Z;
                  glMatrixMode(GL_MODELVIEW);
                  glDisable(GL_TEXTURE_2D);
                  glColor3f(1,1,1);
                  for (a=0; a < stb_arr_len(c->pickers); ++a) {
                     picker_info *pi = &c->pickers[a];
                     float pos=0;
                     float bone_state[4]= {0,0,0,0};
                     int state = pi->state;
                     int rot = pi->rot;
                     float drop_on_pickup;
                     

                     if (pi->input_is_belt) {
                        state = state^2;
                        rot = rot^2;
                     }

                     // state = 0 -> immobile at pickup
                     // state = 1 -> animating towards pickup
                     // state = 2 -> immobile at dropoff
                     // state = 3 -> animating towards dropoff

                     if (state == 0) pos = 0;
                     if (state == 1) pos = 1-offset - 1.0/LONG_TICK_LENGTH;
                     if (state == 2) pos = 1;
                     if (state == 3) pos = offset;

                     if ((state == 1 || state == 3) && offset < 0.125)
                        drop_on_pickup = stb_linear_remap(offset, 0,0.125, -0.15, 0);
                     else
                        drop_on_pickup = 0.0f;

                     #if 1
                     {
                        vec base = { 0.35f,0.35f,0.35f };
                        float len;

                        bone_state[1] = stb_lerp(pos, 0.5, -0.75) - base.x;
                        bone_state[2] = 0 - base.y;
                        bone_state[3] = stb_lerp(pos, 0.30, 0.50) - base.z + drop_on_pickup;

                        len = sqrt(bone_state[1]*bone_state[1] + bone_state[2]*bone_state[2])/2;
                        len = sqrt(1*1 - len);
                        bone_state[0] = len - base.z;
                     }

                     //0,0.25,0.20

                     #else
                     bone_state[0] = (state >= 2 ? 0.06f : 0.08f);
                     bone_state[1] = stb_lerp(pos, 0.75, -0.75);
                     bone_state[3] = 0.05f + drop_on_pickup;
                     //bone_state[3] = (pi->state >= 2 ? 0.05f : -0.05f);
                     #endif


                     add_draw_picker(base_x+pi->pos.unpacked.x+0.5, base_y+pi->pos.unpacked.y+0.5, base_z+pi->pos.unpacked.z,
                                     rot, bone_state);
                     #if 0
                     for (b=1; b < 500; ++b) {
                        bone_state[0] = fmod(b*0.237,0.2);
                        add_draw_picker(base_x+pi->pos.unpacked.x+0.5, base_y+pi->pos.unpacked.y+0.5, base_z+pi->pos.unpacked.z+b,
                                        pi->rot, bone_state);
                     }
                     #endif

                     #if 0
                     glPushMatrix();
                     glTranslatef();
                     glRotatef(90*pi->rot, 0,0,1);
                     stbgl_drawBox(0,0,0.5, 1.5,0.125,0.125, 1);
                     stbgl_drawBox(stb_lerp(pos, 0.75, -0.75),0,0.5-0.125, 0.25,0.25,0.125, 1);
                     glPopMatrix();
                     #endif

                     {
                        float mrot[4][2][2] = {
                           {{ 1,0,},{0,1}},
                           {{ 0,-1,},{1,0}},
                           {{-1,0,},{0,-1}},
                           {{ 0,1,},{-1,0}},
                        };
                        float x = stb_lerp(pi->input_is_belt ? 1-pos : pos, 0.75, -0.75);
                        float y = 0;
                        float az = 0.25;
                        float ax,ay;
                        ax = mrot[pi->rot][0][0]*x + mrot[pi->rot][0][1]*y;
                        ay = mrot[pi->rot][1][0]*x + mrot[pi->rot][1][1]*y;
                        ax += base_x+pi->pos.unpacked.x+0.5;
                        ay += base_y+pi->pos.unpacked.y+0.5;
                        az += base_z+pi->pos.unpacked.z+0.175;
                        if (pi->item != 0)
                           add_sprite(ax, ay, az, pi->item);
                     }
                  }
                  for (a=0; a < stb_arr_len(c->belts); ++a) {
                     belt_run *b = &c->belts[a];
                     if (b->turn == 0) {
                        float z = k * LOGI_CHUNK_SIZE_Z + 1.0f + b->z_off + 0.125f;
                        float x1 = (float) s->slice_x * LOGI_CHUNK_SIZE_X + b->x_off;
                        float y1 = (float) s->slice_y * LOGI_CHUNK_SIZE_Y + b->y_off;
                        float x2,y2;
                        int d0 = b->dir;
                        int d1 = (d0 + 1) & 3;

                        x1 += face_orig[b->dir][0];
                        y1 += face_orig[b->dir][1];

                        x1 += face_dir[d0][0]*(1.0/ITEMS_PER_BELT_SIDE)*0.5;
                        y1 += face_dir[d0][1]*(1.0/ITEMS_PER_BELT_SIDE)*0.5;

                        x2 = x1 + face_dir[d1][0]*(0.9f - 0.5/ITEMS_PER_BELT_SIDE);
                        x1 = x1 + face_dir[d1][0]*(0.1f + 0.5/ITEMS_PER_BELT_SIDE);

                        y2 = y1 + face_dir[d1][1]*(0.9f - 0.5/ITEMS_PER_BELT_SIDE);
                        y1 = y1 + face_dir[d1][1]*(0.1f + 0.5/ITEMS_PER_BELT_SIDE);

                        for (e=0; e < stb_arr_len(b->items); e += 2) {
                           float ax,ay,az;
                           if (b->items[e+0].type != 0) {
                              ax = x1, ay = y1, az=z;
                              if (e < b->mobile_slots[0]*2) {
                                 ax += face_dir[d0][0]*(1.0/ITEMS_PER_BELT_SIDE) * offset;
                                 ay += face_dir[d0][1]*(1.0/ITEMS_PER_BELT_SIDE) * offset;
                                 az += b->end_dz / 2.0 * (1.0 / ITEMS_PER_BELT_SIDE) * offset;
                              }
                              add_sprite(ax, ay, az, b->items[e+0].type);

                           }
                           if (b->items[e+1].type != 0) {
                              ax = x2, ay = y2, az=z;
                              if (e < b->mobile_slots[1]*2) {
                                 ax += face_dir[d0][0]*(1.0/ITEMS_PER_BELT_SIDE) * offset;
                                 ay += face_dir[d0][1]*(1.0/ITEMS_PER_BELT_SIDE) * offset;
                                 az += b->end_dz / 2.0 * (1.0 / ITEMS_PER_BELT_SIDE) * offset;
                              }
                              add_sprite(ax, ay, az, b->items[e+1].type);
                           }
                           x1 += face_dir[d0][0]*(1.0/ITEMS_PER_BELT_SIDE);
                           y1 += face_dir[d0][1]*(1.0/ITEMS_PER_BELT_SIDE);
                           x2 += face_dir[d0][0]*(1.0/ITEMS_PER_BELT_SIDE);
                           y2 += face_dir[d0][1]*(1.0/ITEMS_PER_BELT_SIDE);
                           z += b->end_dz / 2.0 * (1.0 / ITEMS_PER_BELT_SIDE);
                        }
                     } else {
                        float z = k * LOGI_CHUNK_SIZE_Z + 1.0f + b->z_off + 0.125f;
                        float x1 = (float) s->slice_x * LOGI_CHUNK_SIZE_X + b->x_off;
                        float y1 = (float) s->slice_y * LOGI_CHUNK_SIZE_Y + b->y_off;
                        float x2,y2;
                        float ox,oy;
                        int d0 = b->dir;
                        int d1 = (d0 + 1) & 3;
                        int left_len = b->turn > 0 ? SHORT_SIDE : LONG_SIDE;
                        int right_len = SHORT_SIDE+LONG_SIDE - left_len;

                        x1 += face_orig[b->dir][0];
                        y1 += face_orig[b->dir][1];

                        ox = x1;
                        oy = y1;
                        if (b->turn > 0) {
                           ox += face_dir[(b->dir+1)&3][0];
                           oy += face_dir[(b->dir+1)&3][1];
                        }

                        x1 += face_dir[d0][0]*(1.0/ITEMS_PER_BELT_SIDE)*0.5;
                        y1 += face_dir[d0][1]*(1.0/ITEMS_PER_BELT_SIDE)*0.5;

                        x2 = x1 + face_dir[d1][0]*(0.9f - 0.5/ITEMS_PER_BELT_SIDE);
                        y2 = y1 + face_dir[d1][1]*(0.9f - 0.5/ITEMS_PER_BELT_SIDE);

                        x1 = x1 + face_dir[d1][0]*(0.1f + 0.5/ITEMS_PER_BELT_SIDE);
                        y1 = y1 + face_dir[d1][1]*(0.1f + 0.5/ITEMS_PER_BELT_SIDE);



                        for (e=0; e < right_len; ++e) {
                           if (b->items[0+e].type != 0) {
                              float bx,by,ax,ay;
                              float ang = e, s,c;
                              if (e < b->mobile_slots[0])
                                 ang += offset;
                              ang = (ang / right_len) * 3.141592/2;
                              if (b->turn > 0) ang = -ang;
                              ax = x1-ox;
                              ay = y1-oy;
                              s = sin(ang);
                              c = cos(ang);
                              bx = c*ax + s*ay;
                              by = -s*ax + c*ay;
                              add_sprite(ox+bx, oy+by, z, b->items[0+e].type);
                           }
                        }
                        for (e=0; e < left_len; ++e) {
                           if (b->items[right_len+e].type != 0) {
                              float bx,by,ax,ay;
                              float ang = e, s,c;
                              if (e < b->mobile_slots[1])
                                 ang += offset;
                              ang = (ang / left_len) * 3.141592/2;
                              if (b->turn > 0) ang = -ang;
                              ax = x2-ox;
                              ay = y2-oy;
                              s = sin(ang);
                              c = cos(ang);
                              bx = c*ax + s*ay;
                              by = -s*ax + c*ay;
                              add_sprite(ox+bx, oy+by, z, b->items[right_len+e].type);
                           }
                        }
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
                  for (a=0; a < stb_arr_len(c->belts); ++a) {
                     belt_run *b = &c->belts[a];
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
                        logi_chunk *c = logistics_get_chunk(ex,ey,ez,0);
                        assert(c != NULL);
                        if (c) {
                           float tx,ty,tz;
                           belt_run *t = &c->belts[b->target_id];
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
