#include "obbg_funcs.h"

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


#define MAX_PATH_NODES   20000

enum
{
   A_open,
   A_closed,
};

path_node nodes[MAX_PATH_NODES];
int node_alloc;
stb_ptrmap *astar_nodes;

static path_node *get_node(int x, int y, int z)
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
   
   n = stb_ptrmap_get(astar_nodes, convert.ptr);
   if (n != NULL)
      assert(n->x == x && n->y == y && n->z == z);
   return n;
}

static path_node *create_node(int x, int y, int z)
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
   
   assert(stb_ptrmap_get(astar_nodes, convert.ptr) == NULL);
   if (node_alloc < MAX_PATH_NODES) {
      n = &nodes[node_alloc++];
      n->x = (int8) x;
      n->y = (int8) y;
      n->z = (int8) z;
      stb_ptrmap_set(astar_nodes, convert.ptr, n);
   }
   return n;
}

static path_node *open_list[MAX_PATH_NODES];
static int num_open;

static void add_to_open_list(path_node *n, int cost)
{
   assert(num_open < MAX_PATH_NODES);
   open_list[num_open++] = n;
   n->status = A_open;
   n->cost = cost;
}

static void update_open_list(path_node *n, int cost)
{
   n->cost = cost;
}

static path_node *get_smallest_open(void)
{
   path_node *n;
   int i;
   int best_cost=open_list[0]->cost, best_i=0;
   for (i=1; i < num_open; ++i) {
      int cost = open_list[i]->cost + open_list[i]->estimated_remaining;
      if (cost < best_cost) {
         best_cost = cost;
         best_i = i;
      }
   }
   n = open_list[best_i];
   open_list[best_i] = open_list[--num_open];   
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
      flat_move_estimate = dx*2 + 3 * (dy-dx);
   }

   if (dz > 0)
      return flat_move_estimate + pb->estimate_up_cost * abs(dz);
   else
      return flat_move_estimate + pb->estimate_down_cost * abs(dz);
}

// returns path length; path array is reversed
int path_find(path_behavior *pb, vec3i start, vec3i dest, vec3i *path, int max_path)
{
   //FILE *f = fopen("c:/x/path.txt", "w");
   static int dx[4] = { 1,0,-1,0 };
   static int dy[4] = { 0,1,0,-1 };
   vec3i relative_dest;
   path_node *n;
   node_alloc = 0;
   num_open = 0;
   if (!can_stand(pb, 0,0,0, start))
      return 0;
   if (!can_stand(pb, 0,0,0, dest))
      return 0;
   if (start.x == dest.x && start.y == dest.y && start.z == dest.z)
      return 0;

   astar_nodes = stb_ptrmap_new();
   n = create_node(0,0,0);
   n->dir = 0;
   n->dz = 0;
   add_to_open_list(n, 0);

   relative_dest.x = dest.x - start.x;
   relative_dest.y = dest.y - start.y;
   relative_dest.z = dest.z - start.z;

   while (num_open > 0) {
      int dz,d;
      n = get_smallest_open();

      if (n->x == relative_dest.x && n->y == relative_dest.y && n->z == relative_dest.z)
         break;

      for (dz= -pb->max_step_down; dz <= pb->max_step_up; ++dz) {
         for (d=0; d < 4; ++d) {
            Bool allowed=False;
            int x = n->x+dx[d];
            int y = n->y+dy[d];
            int z = n->z+dz;
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
            if (allowed) {
               static int shack;
               int cost;
               path_node *m;

               cost = n->cost + 4 + (dz < 0 ? pb->step_down_cost[-dz] : pb->step_up_cost[dz]);

               m = get_node(x,y,z);
               if (shack++ % 64 == 0) SDL_Delay(2);
               assert(m != n);
               if (m == NULL) {
                  m = create_node(x,y,z);
                  if (m == NULL) break;
                  m->dir = d;
                  m->dz = dz;
                  m->estimated_remaining = estimate_distance_lowerbound(pb, m->x+start.x, m->y+start.y, m->z+start.z, dest);
                  add_to_open_list(m, cost);
               } else {
                  if (cost < m->cost) {
                     m->dir = d;
                     m->dz = dz;
                     if (m->status == A_closed)
                        add_to_open_list(m, m->cost);
                     else
                        update_open_list(m, m->cost);
                  }
               }
            }
         }
      }
      n->status = A_closed;
   }

   if (num_open == 0) {
      stb_ptrmap_delete(astar_nodes, NULL);
      astar_nodes = 0;
      return 0;
   } else {
      int i;
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

         m = get_node(x,y,z);
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
      stb_ptrmap_delete(astar_nodes, NULL);
      astar_nodes = 0;

      if (i == max_path)
         return 0;
      else
         return i+1;
   }
}
