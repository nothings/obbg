#include "obbg_funcs.h"
#include <stdlib.h>
#include <math.h>

//#define OLD_PATHFIND

// does the object fit,
// and is it allowed to stand on the object below
Bool can_stand_raw(path_behavior *pb, int x, int y, int z, vec3i start, Bool must_stand)
{
   int i,j,k, z_ground = start.z + z - 1;
   Bool any_ground=False;
   if (start.z + z < 0)
      return False;
   for (j=0; j < pb->size.y; ++j) {
      int ry = (start.y+y+j) & (MESH_CHUNK_SIZE_Y-1);
      for (i=0; i < pb->size.x; ++i) {
         int rx = (start.x+x+i) & (MESH_CHUNK_SIZE_X-1);
         mesh_chunk *mc = get_physics_chunk_for_coord(start.x+x+i, start.y+y+j);
         if (mc == NULL)
            return False;
         for (k=0; k < pb->size.z; ++k) {
            int nz = start.z+z + k;
            if (mc->pc.pathdata[ry][rx].data[nz>>4] & (1 << (nz&15))) {
               return 0;
            }
         }
         if (z_ground < 0 || (mc->pc.pathdata[ry][rx].data[z_ground>>4] & (1 << (z_ground & 15)))) {
            any_ground = True;
         }
      }
   }
   return any_ground || pb->flying || !must_stand;
}

Bool can_stand(path_behavior *pb, int x, int y, int z, vec3i start)
{
   return can_stand_raw(pb,x,y,z,start, True);
}

Bool can_fit(path_behavior *pb, int x, int y, int z, vec3i start)
{
   return can_stand_raw(pb,x,y,z,start, False);
}

Bool can_place_foot(vec location, float x_rad, float y_rad)
{
   int i,j;
   int x0 = (int) floor(location.x - x_rad);
   int x1 = (int) floor(location.x + x_rad);
   int y0 = (int) floor(location.y - y_rad);
   int y1 = (int) floor(location.y + y_rad);
   int z = (int) floor(location.z);
   int z_ground = z-1;
   Bool any_ground = False;

   for (j=y0; j <= y1; ++j) {
      int ry = j & (MESH_CHUNK_SIZE_Y-1);
      for (i=x0; i <= x1; ++i) {
         int rx = i & (MESH_CHUNK_SIZE_X-1);
         mesh_chunk *mc = get_physics_chunk_for_coord(i,j);
         if (mc == NULL)
            return False;
         if (mc->pc.pathdata[ry][rx].data[z>>4] & (1 << (z&15)))
            return False;
         if (z_ground < 0 || (mc->pc.pathdata[ry][rx].data[z_ground>>4] & (1 << (z_ground&15)))) {
            any_ground = True;
         }
      }
   }
   return any_ground;
}


#define MAX_PATH_NODES   5000
#define NUM_PQ_PRIMARY   32
#define PQ_SECONDARY_SPACING  (NUM_PQ_PRIMARY/2)
#define PQ_SECONDARY_SPACING_LOG2  4
#define NUM_PQ_SECONDARY      16

#if (1 << PQ_SECONDARY_SPACING_LOG2) != PQ_SECONDARY_SPACING
#error "mismatched defines"
#endif

enum
{
   A_open,
   A_closed,
};

typedef struct
{
   int16 next,prev;
} path_links;

typedef struct
{
   path_node nodes[MAX_PATH_NODES];
   int node_alloc;
   stb_ptrmap *astar_nodes;

   path_node *open_list[MAX_PATH_NODES];
   int num_open;

   path_links node_link[MAX_PATH_NODES];
   int16 head[NUM_PQ_PRIMARY];
   int16 secondary[NUM_PQ_SECONDARY];
   int primary_base_value;
   int secondary_base_value;
   int num_primary;
   int num_secondary;
   int first_nonempty;
} pathfind_context;

#ifdef OLD_PATHFIND
static void add_to_open_list(pathfind_context *pc, path_node *n, int cost)
{
   assert(pc->num_open < MAX_PATH_NODES);
   pc->open_list[pc->num_open++] = n;
   n->status = A_open;
   n->cost = cost;
}

static void update_open_list(pathfind_context *pc, path_node *n, int cost)
{
   n->cost = cost;
}

static path_node *get_smallest_open(pathfind_context *pc)
{
   path_node *n;
   int i, best_i = -1;
   int best_cost = 9999999;
   if (pc->num_open == 0)
      return NULL;
   for (i=0; i < pc->num_open; ++i) {
      int cost = pc->open_list[i]->cost;
      if (cost < best_cost) {
         best_cost = cost;
         best_i = i;
      }
   }
   assert(best_i >= 0);
   n = pc->open_list[best_i];
   pc->open_list[best_i] = pc->open_list[--pc->num_open];
   return n;
}
#else

static int count_primary(pathfind_context *pc)
{
   int n = 0;
   int i;
   for (i=0; i < NUM_PQ_PRIMARY; ++i) {
      int x = pc->head[i];
      while (x >= 0) {
         ++n;
         if (pc->node_link[x].prev == -1)
            assert(pc->head[i] == x);
         x = pc->node_link[x].next;
      }
   }
   return n;
}

static Bool is_primary_cost(pathfind_context *pc, int cost)
{
   return cost < pc->primary_base_value + NUM_PQ_PRIMARY;
}

static int secondary_list_index(pathfind_context *pc, int cost)
{
   return (cost - pc->secondary_base_value) >> PQ_SECONDARY_SPACING_LOG2;
}

static void add_to_primary_list(pathfind_context *pc, int cost, path_node *n)
{
   int x = n - pc->nodes;
   int list = (cost - pc->primary_base_value);
   assert(list >= 0 && list < NUM_PQ_PRIMARY);

   assert(list >= pc->first_nonempty);

   if (pc->head[list] >= 0)
      pc->node_link[pc->head[list]].prev = x;

   pc->node_link[x].next = pc->head[list];
   pc->node_link[x].prev = -1;
      
   pc->head[list] = x;

   n->cost = cost;

   ++pc->num_primary;
}

static void add_to_secondary_list(pathfind_context *pc, int cost, path_node *n)
{
   int x = n - pc->nodes;
   int list = secondary_list_index(pc, cost);
   assert(list >= 0 && list < NUM_PQ_SECONDARY);

   if (pc->secondary[list] >= 0)
      pc->node_link[pc->secondary[list]].prev = x;

   pc->node_link[x].next = pc->secondary[list];
   pc->node_link[x].prev = -1;
      
   pc->secondary[list] = x;

   n->cost = cost;

   ++pc->num_secondary;
}

static void remove_from_primary_list(pathfind_context *pc, path_node *n)
{
   int x = n - pc->nodes;
   int list;

   list = n->cost - pc->primary_base_value;

   if (pc->node_link[x].prev < 0) {
      assert(pc->head[list] == x);
      pc->head[list] = pc->node_link[x].next;
   } else
      pc->node_link[pc->node_link[x].prev].next = pc->node_link[x].next;

   if (pc->node_link[x].next >= 0)
      pc->node_link[pc->node_link[x].next].prev = pc->node_link[x].prev;

   --pc->num_primary;

   assert(pc->num_primary >= 0 && pc->num_secondary >= 0);
}

static void remove_from_secondary_list(pathfind_context *pc, path_node *n)
{
   int x = n - pc->nodes;
   int list;

   list = secondary_list_index(pc, n->cost);

   if (pc->node_link[x].prev < 0) {
      assert(pc->secondary[list] == x);
      pc->secondary[list] = pc->node_link[x].next;
   } else
      pc->node_link[pc->node_link[x].prev].next = pc->node_link[x].next;

   if (pc->node_link[x].next >= 0)
      pc->node_link[pc->node_link[x].next].prev = pc->node_link[x].prev;

   --pc->num_secondary;
   assert(pc->num_primary >= 0 && pc->num_secondary >= 0);
}

static void add_to_open_list(pathfind_context *pc, path_node *n, int cost)
{
   if (is_primary_cost(pc, cost))
      add_to_primary_list(pc, cost, n);
   else
      add_to_secondary_list(pc, cost, n);
}

static void update_open_list(pathfind_context *pc, path_node *n, int cost)
{
   if (is_primary_cost(pc, n->cost)) {
      // currently on primary list
      remove_from_primary_list(pc, n);
      add_to_primary_list(pc, cost, n);
   } else {
      // currently on secondary list
      if (cost < pc->primary_base_value + NUM_PQ_PRIMARY) {
         // moving to primary list
         remove_from_secondary_list(pc, n);
         add_to_primary_list(pc, cost, n);
      } else {
         int curlist = secondary_list_index(pc, n->cost);
         int list = secondary_list_index(pc, cost);
         if (curlist == list)
            return;

         remove_from_secondary_list(pc, n);
         add_to_secondary_list(pc, cost, n);
      }
   }
}

static path_node *get_smallest_open(pathfind_context *pc)
{
   // first check the existing primaries for a non-empty list
   int i;

   for(;;) {
      if (pc->num_primary > 0) {
         for (i=pc->first_nonempty; i < NUM_PQ_PRIMARY/2; ++i) {
            if (pc->head[i] >= 0) {
               path_node *n = &pc->nodes[pc->head[i]];
               pc->first_nonempty = i;
               remove_from_primary_list(pc, n);
               return n;
            }
         }
      } else {
         if (pc->num_secondary == 0)
            return NULL;
      }
      pc->primary_base_value += NUM_PQ_PRIMARY/2;

      memmove(pc->head, pc->head + NUM_PQ_PRIMARY/2, NUM_PQ_PRIMARY/2 * sizeof(pc->head[0]));
      pc->first_nonempty = 0;
      for (i=NUM_PQ_PRIMARY/2; i < NUM_PQ_PRIMARY; ++i)
         pc->head[i] = -1;

      while (pc->secondary[0] >= 0) {
         path_node *n = &pc->nodes[pc->secondary[0]];
         remove_from_secondary_list(pc, n);
         add_to_primary_list(pc, n->cost, n);
      }

      memmove(pc->secondary, pc->secondary+1, (NUM_PQ_SECONDARY-1) * sizeof(pc->secondary[0]));
      pc->secondary_base_value += PQ_SECONDARY_SPACING;
   }
}
#endif


static path_node *get_node(pathfind_context *pc, int x, int y, int z)
{
   union {
      void *ptr;
      struct { int8 x,y,z; } i;
   } convert;
   path_node *n;

   convert.ptr = NULL;
   convert.i.x = (int8) x;
   convert.i.y = (int8) y;
   convert.i.z = (int8) z;
   
   n = stb_ptrmap_get(pc->astar_nodes, convert.ptr);
   if (n != NULL)
      assert(n->x == x && n->y == y && n->z == z);
   return n;
}

static path_node *create_node(pathfind_context *pc, int x, int y, int z)
{
   union {
      void *ptr;
      struct { int8 x,y,z; } i;
   } convert;
   path_node *n = NULL;

   convert.ptr = NULL;
   convert.i.x = (int8) x;
   convert.i.y = (int8) y;
   convert.i.z = (int8) z;
   
   assert(stb_ptrmap_get(pc->astar_nodes, convert.ptr) == NULL);
   if (pc->node_alloc < MAX_PATH_NODES) {
      n = &pc->nodes[pc->node_alloc++];
      n->x = (int8) x;
      n->y = (int8) y;
      n->z = (int8) z;
      stb_ptrmap_set(pc->astar_nodes, convert.ptr, n);
   }
   return n;
}

static int estimate_distance_lowerbound(path_behavior *pb, int x, int y, int z, vec3i dest)
{
   int dx = abs(x - dest.x);
   int dy = abs(y - dest.y);
   int dz = z - dest.z;
   int flat_move_estimate;

   if (dx > dy) {
      flat_move_estimate = dy*3 + 2 * (dx-dy);
   } else {
      flat_move_estimate = dx*3 + 2 * (dy-dx);
   }

   if (dz > 0)
      return flat_move_estimate + pb->estimate_up_cost * abs(dz);
   else
      return flat_move_estimate + pb->estimate_down_cost * abs(dz);
}


static pathfind_context pc_data;
path_node *debug_nodes;
int debug_node_alloc;



// returns path length; path array is reversed
int path_find(path_behavior *pb, vec3i start, vec3i dest, vec3i *path, int max_path)
{
   pathfind_context *pc = &pc_data; 
   //FILE *f = fopen("c:/x/path.txt", "w");
   static int dx[8] = { 1,0,-1,0, 1,1,-1,-1 };
   static int dy[8] = { 0,1,0,-1, -1,1,-1,1 };
   vec3i relative_dest;
   path_node *n;
   #ifndef OLD_PATHFIND
   int i;
   #endif

   memset(pc, 0, sizeof(*pc));

   if (!can_stand(pb, 0,0,0, start))
      return 0;
   if (!can_stand(pb, 0,0,0, dest))
      return 0;
   if (start.x == dest.x && start.y == dest.y && start.z == dest.z)
      return 0;

   pc->astar_nodes = stb_ptrmap_new();
   n = create_node(pc, 0,0,0);
   n->dir = 0;
   n->dz = 0;
   n->estimated_remaining = estimate_distance_lowerbound(pb, start.x, start.y, start.z, dest);

   #ifndef OLD_PATHFIND
   pc->primary_base_value = n->estimated_remaining;
   pc->secondary_base_value = n->estimated_remaining + NUM_PQ_PRIMARY;
   pc->first_nonempty = 0;
   for (i=0; i < NUM_PQ_PRIMARY; ++i)
      pc->head[i] = -1;
   for (i=0; i < NUM_PQ_SECONDARY; ++i)
      pc->secondary[i] = -1;
   #endif

   add_to_open_list(pc, n, 0 + n->estimated_remaining);

   relative_dest.x = dest.x - start.x;
   relative_dest.y = dest.y - start.y;
   relative_dest.z = dest.z - start.z;

   for(;;) {
      int dz,d;
      n = get_smallest_open(pc);
      if (n == NULL) {
         assert(pc->num_primary == 0 && pc->num_secondary == 0);
         break;
      }

      if (n->x == relative_dest.x && n->y == relative_dest.y && n->z == relative_dest.z)
         break;

      for (dz= -pb->max_step_down; dz <= pb->max_step_up; ++dz) {
         for (d=0; d < 8; ++d) {
            Bool allowed=False;
            int x = n->x+dx[d];
            int y = n->y+dy[d];
            int z = n->z+dz;

            // can't go outside limited range coordinates
            if (x >= 128 || x < -128 || y >= 128 || y < -128 || z >= 128 || z < -128)
               continue;

            // check if fits at destination
            if (can_stand(pb, x,y,z, start)) {
               if (dz < 0) {
                  if (can_fit(pb, x,y, n->z, start))
                     allowed = True;
               } else if (dz > 0) {
                  if (can_fit(pb, n->x, n->y, z, start))
                     allowed = True;
               } else {
                  allowed = True;
               }
            }

            // if diagonal, check that the adjacent orthogonals aren't blocked
            if (allowed && d >= 4) {
               if (dz <= 0) {
                  if (!(can_fit(pb, x,n->y,n->z, start) && can_fit(pb, n->x,y,n->z, start)))
                     allowed = False;
               } else {
                  if (!(can_fit(pb, x,n->y,z, start) && can_fit(pb, n->x,y,z, start)))
                     allowed = False;
               }
            }

            if (allowed) {
               int cost, prev_cost;
               path_node *m;

               prev_cost = n->cost - n->estimated_remaining;

               cost = prev_cost + 2 + (dz < 0 ? pb->step_down_cost[-dz] : pb->step_up_cost[dz]);
               if (d >= 4)
                  cost += 1;

               m = get_node(pc, x,y,z);
               assert(m != n);
               if (m == NULL) {
                  m = create_node(pc,x,y,z);
                  if (m == NULL) break;
                  m->dir = d;
                  m->dz = dz;
                  m->estimated_remaining = estimate_distance_lowerbound(pb, m->x+start.x, m->y+start.y, m->z+start.z, dest);
                  cost += m->estimated_remaining;
                  add_to_open_list(pc, m, cost);
               } else {
                  cost += m->estimated_remaining;
                  if (cost < m->cost) {
                     m->dir = d;
                     m->dz = dz;
                     if (m->status == A_closed)
                        add_to_open_list(pc, m, cost);
                     else
                        update_open_list(pc, m, cost);
                  }
               }
            }
         }
      }
      n->status = A_closed;
   }

   debug_nodes = pc->nodes;
   debug_node_alloc = pc->node_alloc;

   if (n == NULL) {
      stb_ptrmap_delete(pc->astar_nodes, NULL);
      pc->astar_nodes = 0;
      return 0;
   } else {
      int i;
      n->status = A_closed;
      for (i=0; i < max_path; ++i) {
         int x,y,z;
         path_node *m;
         path[i].x = start.x + n->x;
         path[i].y = start.y + n->y;
         path[i].z = start.z + n->z;

         if (n->x == 0 && n->y == 0 && n->z == 0)
            break;

         x = n->x - dx[n->dir];
         y = n->y - dy[n->dir];
         z = n->z - n->dz;

         m = get_node(pc,x,y,z);
         assert(m != NULL);
         assert(m->status == A_closed);
         n = m;

         #if 0
         for (dz= -pb->max_step_up; dz <= pb->max_step_down; ++dz) {
            for (d=0; d < 4; ++d) {
               Bool allowed = True;
               int cost;
               path_node *m = get_node(n->x + dx[d], n->y + dy[d], n->z + dz);
               if (m != NULL) {
                  cost = m->cost + 4 + (dz > 0 ? pb->step_down_cost[dz] : pb->step_up_cost[-dz]);
                  if (cost != n->cost) 
                     continue;

                  if (can_stand(pb, n->x+dx[d], n->y+dy[d], n->z+dz, start)) {
                     if (dz < 0) {
                        if (can_fit(pb, n->x+dx[d], n->y+dy[d], n->z   , start))
                           allowed = True;
                     } else if (dz > 0) {
                        if (can_fit(pb, n->x      , n->y      , n->z+dz, start))
                           allowed = True;
                     } else {
                        allowed = True;
                     }
                  }
                  if (!allowed)
                     continue;
                  n = m;
                  goto ok;
               }
            }
         }
         assert(0);

        ok:
         assert(n != NULL);
         #endif
      }
      stb_ptrmap_delete(pc->astar_nodes, NULL);

      if (i == max_path)
         return 0;
      else
         return i+1;
   }
}
