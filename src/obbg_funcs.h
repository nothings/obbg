#ifndef INCLUDE_OBBG_FUNCS_H
#define INCLUDE_OBBG_FUNCS_H

#include "obbg_data.h"
#include "sdl.h"

extern void error(char *s);
extern void ods(char *fmt, ...);
extern void examine_outstanding_genchunks(void);
extern void init_voxel_render(int voxtex[2]);
extern void render_voxel_world(float campos[3]);
extern void init_chunk_caches(void);
extern void init_mesh_building(void);
extern void init_mesh_build_threads(void);
extern void s_init_physics_cache(void);
extern void free_mesh_chunk(mesh_chunk *mc);
extern void free_mesh_chunk_physics(mesh_chunk *mc);
extern void world_init(void);
extern void free_physics_cache(void);
typedef void (obbg_malloc_dump_func)(size_t size, char *info);
extern void obbg_malloc_dump(obbg_malloc_dump_func *f);
extern void query_thread_info(int id, int *count, double *time);
extern void init_object_render(void);
extern void force_update_for_block_raw(int x, int y, int z);
extern void process_mouse_move(int dx, int dy);
extern void mouse_up(void);
extern void mouse_down(int button);
extern void process_key_up(int k, int s);
extern uint32 load_sprite(char *filename);
extern void init_ui_render(void);
extern void voxel_draw_block(int x, int y, int z, int blocktype, int rot);
extern int block_has_voxel_geometry(int blocktype);
extern int build_small_mesh(int x, int y, int z, uint8 mesh_geom[4][4][4], uint8 mesh_lighting[4][4][4], int num_quads, uint8* vbuf, uint8 *fbuf, float transform[3][3]);
extern uint8 lighting_with_rotation(int light, int rot);
extern void draw_pickers_flush(float alpha);
extern void mouse_relative(Bool relative);


extern void logistics_tick(void); // move into thread
extern void logistics_render(void); // create copy and render
extern void logistics_debug_render(void); // render from copy
extern Bool logistics_draw_block(int x, int y, int z, int blocktype, int rot); // doesn't access db
extern void logistics_init(void); // create thread
extern void logistics_record_ore(int x, int y, int z1, int z2, int type); // uses locks
extern void logistics_update_block(int x, int y, int z, int type, int rot); // <--- queue up block changes, process in thread
extern float logistics_animation_offset(void);
extern void logistics_render_from_copy(render_logi_chunk **render_copy, float offset); // render from copy

extern void process_tick_raw(float dt);
extern int get_next_built_mesh(built_mesh *bm);
extern requested_mesh *get_requested_mesh_alternate(void);
extern void swap_requested_meshes(void);
extern int get_block(int x, int y, int z);
extern int get_block_rot(int x, int y, int z);
extern void save_edits(void);
extern void load_edits(void);
extern void init_mesh_building(void);
extern void do_ui_rendering_3d(void);
extern void do_ui_rendering_2d(void);

extern mesh_chunk *build_mesh_chunk_for_coord(mesh_chunk *mc, int x, int y);
extern void upload_mesh(mesh_chunk *mc, uint8 *vertex_build_buffer, uint8 *face_buffer);
extern void set_mesh_chunk_for_coord(int x, int y, mesh_chunk *new_mc);
extern mesh_chunk *get_mesh_chunk_for_coord(int x, int y);
extern mesh_chunk *get_physics_chunk_for_coord(int x, int y);
//extern int collision_test_box(float x, float y, float z, float bounds[2][3]);
extern int physics_move_walkable(vec *pos, vec *vel, float dt, float size[2][3]);
extern void build_picker(void);
extern void add_draw_picker(float x, float y, float z, int rot, float states[4]);
extern void finished_caching_mesh_chunk(int x, int y, Bool needs_triangles);

extern void physics_process_mesh_chunk(mesh_chunk *mc);
extern int physics_set_player_coord(requested_mesh *rm, int max_req, int px, int py);
extern void player_physics(objid oid, player_controls *con, float dt);
extern void force_update_for_block(int x, int y, int z);
extern void change_block(int x, int y, int z, int type, int rot);
extern void update_phys_chunk(mesh_chunk *mc, int x, int y, int z, int type);
extern void free_phys_chunk(mesh_chunk *mc);
extern void update_physics_cache(int x, int y, int z, int type, int rot);
extern void add_sprite(float x, float y, float z, int id);
extern void stop_manager(void);

typedef struct
{
   int bx,by,bz;
   int face;
} RaycastResult;
extern Bool raycast(float x1, float y1, float z1, float x2, float y2, float z2, RaycastResult *res);


typedef struct
{
   SDL_mutex *mutex;
   unsigned char padding[64];
   int  head,tail;
   int count;
   size_t itemsize;
   uint8 *data;
} threadsafe_queue;

extern void init_threadsafe_queue(threadsafe_queue *tq, int count, size_t size);
extern int add_to_queue(threadsafe_queue *tq, void *item);
extern int get_from_queue_nonblocking(threadsafe_queue *tq, void *item);


extern objid allocate_object(void);
extern objid allocate_player(void);
extern void objspace_to_worldspace(float world[3], objid oid, float cam_x, float cam_y, float cam_z);

typedef struct
{
   uint32 host;
   uint16 port;
} address;

extern Bool net_init(Bool server, int port);
extern Bool net_send(void *buffer, size_t buffer_size, address *addr);
extern int  net_receive(void *buffer, size_t buffer_size, address *addr);
extern void client_view_physics(objid oid, player_controls *con, float dt);
extern void client_net_tick(void);
extern void server_net_tick_pre_physics(void);
extern void server_net_tick_post_physics(void);

extern void * obbg_malloc(size_t size, char *info);
extern void obbg_free(void *p);
extern void *obbg_realloc(void *p, size_t size, char *info);


#ifdef STB_MALLOC_WRAPPER
  #define STB__PARAMS    , char *file, int line
  #define STB__ARGS      ,       file,     line
#else
  #define STB__PARAMS
  #define STB__ARGS
#endif

// calling this function allocates an empty obarr attached to p
// (whereas NULL isn't attached to anything)
extern void obarr_malloc(void **target, void *context);

// call this function with a non-NULL value to have all successive
// stbs that are created be attached to the associated parent. Note
// that once a given obarr is non-empty, it stays attached to its
// current parent, even if you call this function again.
// it turns the previous value, so you can restore it
extern void* obarr_malloc_parent(void *p);

// simple functions written on top of other functions
#define obarr_empty(a)       (  obarr_len(a) == 0 )
#define obarr_add(a,i)       (  obarr_addn((a),1,(i)) )
#define obarr_push(a,v,i)    ( *obarr_add(a,i)=(v)  )

typedef struct
{
   int len, limit;
   int stb_malloc;
   unsigned int signature;
} ob__arr;

#define obarr_signature      0x51bada7b  // ends with 0123 in decimal

// access the header block stored before the data
#define obarrhead(a)         /*lint --e(826)*/ (((ob__arr *) (a)) - 1)
#define obarrhead2(a)        /*lint --e(826)*/ (((ob__arr *) (a)) - 1)

#ifdef STB_DEBUG
#define obarr_check(a)       assert(!a || obarrhead(a)->signature == obarr_signature)
#define obarr_check2(a)      assert(!a || obarrhead2(a)->signature == obarr_signature)
#else
#define obarr_check(a)       0
#define obarr_check2(a)      0
#endif

// ARRAY LENGTH

// get the array length; special case if pointer is NULL
#define obarr_len(a)         (a ? obarrhead(a)->len : 0)
#define obarr_len2(a)        ((ob__arr *) (a) ? obarrhead2(a)->len : 0)
#define obarr_lastn(a)       (obarr_len(a)-1)

// check whether a given index is valid -- tests 0 <= i < obarr_len(a) 
#define obarr_valid(a,i)     (a ? (int) (i) < obarrhead(a)->len : 0)

// change the array length so is is exactly N entries long, creating
// uninitialized entries as needed
#define obarr_setlen(a,n,i)  \
            (ob__arr_setlen((void **) &(a), sizeof(a[0]), (n), (i)))

// change the array length so that N is a valid index (that is, so
// it is at least N entries long), creating uninitialized entries as needed
#define obarr_makevalid(a,n)  \
            (obarr_len(a) < (n)+1 ? obarr_setlen(a,(n)+1),(a) : (a))

// remove the last element of the array, returning it
#define obarr_pop(a)         ((obarr_check(a), (a))[--obarrhead(a)->len])

// access the last element in the array
#define obarr_last(a)        ((obarr_check(a), (a))[obarr_len(a)-1])

// is iterator at end of list?
#define obarr_end(a,i)       ((i) >= &(a)[obarr_len(a)])

// (internal) change the allocated length of the array
#define obarr__grow(a,n)     (obarr_check(a), obarrhead(a)->len += (n))

// add N new unitialized elements to the end of the array
#define obarr__addn(a,n,i)     /*lint --e(826)*/ \
                               ((obarr_len(a)+(n) > obarrcurmax(a))      \
                                 ? (ob__arr_addlen((void **) &(a),sizeof(*a),(n),i),0) \
                                 : ((obarr__grow(a,n), 0)))

// add N new unitialized elements to the end of the array, and return
// a pointer to the first new one
#define obarr_addn(a,n,i)    (obarr__addn((a),n,i),(a)+obarr_len(a)-(n))

// add N new uninitialized elements starting at index 'i'
#define obarr_insertn(a,i,n) (ob__arr_insertn((void **) &(a), sizeof(*a), i, n))

// insert an element at i
#define obarr_insert(a,i,v)  (ob__arr_insertn((void **) &(a), sizeof(*a), i, n), ((a)[i] = v))

// delete N elements from the middle starting at index 'i'
#define obarr_deleten(a,i,n) (ob__arr_deleten((void **) &(a), sizeof(*a), i, n))

// delete the i'th element
#define obarr_delete(a,i)   obarr_deleten(a,i,1)

// delete the i'th element, swapping down from the end
#define obarr_fastdelete(a,i)  \
   (stb_swap(&a[i], &a[obarrhead(a)->len-1], sizeof(*a)), obarr_pop(a))


// ARRAY STORAGE

// get the array maximum storage; special case if NULL
#define obarrcurmax(a)       (a ? obarrhead(a)->limit : 0)
#define obarrcurmax2(a)      (a ? obarrhead2(a)->limit : 0)

// set the maxlength of the array to n in anticipation of further growth
#define obarr_setsize(a,n)   (obarr_check(a), ob__arr_setsize((void **) &(a),sizeof((a)[0]),n))

// make sure maxlength is large enough for at least N new allocations
#define obarr_atleast(a,n)   (obarr_len(a)+(n) > obarrcurmax(a)      \
                                 ? obarr_setsize((a), (n)) : 0)

// make a copy of a given array (copies contents via 'memcpy'!)
#define obarr_copy(a)        ob__arr_copy(a, sizeof((a)[0]))

// compute the storage needed to store all the elements of the array
#define obarr_storage(a)     (obarr_len(a) * sizeof((a)[0]))

#define obarr_for(v,arr)     for((v)=(arr); (v) < (arr)+obarr_len(arr); ++(v))

// IMPLEMENTATION

extern void obarr_free_(void **p);
extern void *ob__arr_copy_(void *p, int elem_size);
extern void ob__arr_setsize_(void **p, int size, int limit, char *info);
extern void ob__arr_setlen_(void **p, int size, int newlen, char *info);
extern void ob__arr_addlen_(void **p, int size, int addlen, char *info);
extern void ob__arr_deleten_(void **p, int size, int loc, int n);
extern void ob__arr_insertn_(void **p, int size, int loc, int n, char *info);

#define obarr_free(p)            obarr_free_((void **) &(p))
#define ob__arr_copy              ob__arr_copy_

#ifndef STB_MALLOC_WRAPPER
  #define ob__arr_setsize         ob__arr_setsize_
  #define ob__arr_setlen          ob__arr_setlen_
  #define ob__arr_addlen          ob__arr_addlen_
  #define ob__arr_deleten         ob__arr_deleten_
  #define ob__arr_insertn         ob__arr_insertn_
#else
  #define ob__arr_addlen(p,s,n)    ob__arr_addlen_(p,s,n,__FILE__,__LINE__)
  #define ob__arr_setlen(p,s,n)    ob__arr_setlen_(p,s,n,__FILE__,__LINE__)
  #define ob__arr_setsize(p,s,n)   ob__arr_setsize_(p,s,n,__FILE__,__LINE__)
  #define ob__arr_deleten(p,s,i,n) ob__arr_deleten_(p,s,i,n,__FILE__,__LINE__)
  #define ob__arr_insertn(p,s,i,n) ob__arr_insertn_(p,s,i,n,__FILE__,__LINE__)
#endif


#endif
