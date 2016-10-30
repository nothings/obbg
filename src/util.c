#include "obbg_funcs.h"
#include "SDL_thread.h"

//////////////////////////////////////////////////////////////////////////////
//
//                                obarr
//
//  An obarr is directly useable as a pointer (use the actual type in your
//  definition), but when it resizes, it returns a new pointer and you can't
//  use the old one, so you have to be careful to copy-in-out as necessary.
//
//  Use a NULL pointer as a 0-length array.
//
//     float *my_array = NULL, *temp;
//
//     // add elements on the end one at a time
//     obarr_push(my_array, 0.0f);
//     obarr_push(my_array, 1.0f);
//     obarr_push(my_array, 2.0f);
//
//     assert(my_array[1] == 2.0f);
//
//     // add an uninitialized element at the end, then assign it
//     *obarr_add(my_array) = 3.0f;
//
//     // add three uninitialized elements at the end
//     temp = obarr_addn(my_array,3);
//     temp[0] = 4.0f;
//     temp[1] = 5.0f;
//     temp[2] = 6.0f;
//
//     assert(my_array[5] == 5.0f);
//
//     // remove the last one
//     obarr_pop(my_array);
//
//     assert(obarr_len(my_array) == 6);


#if 0
void ob__arr_malloc(void **target, void *context, char *info)
{
   ob__arr *q = (ob__arr *) obbg_malloc(sizeof(*q), char *info);
   q->len = q->limit = 0;
   q->stb_malloc = 1;
   q->signature = obarr_signature;
   *target = (void *) (q+1);
}

void * ob__arr_copy_(void *p, int elem_size)
{
   ob__arr *q;
   if (p == NULL) return p;
   q = (ob__arr *) ob__arr_malloc(sizeof(*q) + elem_size * obarrhead2(p)->limit);
   obarr_check2(p);
   memcpy(q, obarrhead2(p), sizeof(*q) + elem_size * obarrhead2(p)->len);
   q->stb_malloc = !!ob__arr_context;
   return q+1;
}
#endif

void obarr_free_(void **pp)
{
   void *p = *pp;
   obarr_check2(p);
   if (p) {
      ob__arr *q = obarrhead2(p);
      obbg_free(q);
   }
   *pp = NULL;
}

static void ob__arrsize_(void **pp, int size, int limit, int len, char *info)
{
   void *p = *pp;
   ob__arr *a;
   obarr_check2(p);
   if (p == NULL) {
      if (len == 0 && size == 0) return;
      a = (ob__arr *) obbg_malloc(sizeof(*a) + size*limit, info);
      a->limit = limit;
      a->len   = len;
      a->stb_malloc = 0;
      a->signature = obarr_signature;
   } else {
      a = obarrhead2(p);
      a->len = len;
      if (a->limit < limit) {
         void *p;
         if (a->limit >= 4 && limit < a->limit * 2)
            limit = a->limit * 2;
         p = obbg_realloc(a, sizeof(*a) + limit*size, info);
         if (p) {
            a = (ob__arr *) p;
            a->limit = limit;
         } else {
            // throw an error!
         }
      }
   }
   a->len   = stb_min(a->len, a->limit);
   *pp = a+1;
}

void ob__arr_setsize_(void **pp, int size, int limit, char *info)
{
   void *p = *pp;
   obarr_check2(p);
   ob__arrsize_(pp, size, limit, obarr_len2(p), info);
}

void ob__arr_setlen_(void **pp, int size, int newlen, char *info)
{
   void *p = *pp;
   obarr_check2(p);
   if (obarrcurmax2(p) < newlen || p == NULL) {
      ob__arrsize_(pp, size, newlen, newlen, info);
   } else {
      obarrhead2(p)->len = newlen;
   }
}

void ob__arr_addlen_(void **p, int size, int addlen, char *info)
{
   ob__arr_setlen_(p, size, obarr_len2(*p) + addlen, info);
}

void ob__arr_insertn_(void **pp, int size, int i, int n, char *info)
{
   void *p = *pp;
   if (n) {
      int z;

      if (p == NULL) {
         ob__arr_addlen_(pp, size, n, info);
         return;
      }

      z = obarr_len2(p);
      ob__arr_addlen_(&p, size, i, info);
      memmove((char *) p + (i+n)*size, (char *) p + i*size, size * (z-i));
   }
   *pp = p;
}

void ob__arr_deleten_(void **pp, int size, int i, int n)
{
   void *p = *pp;
   if (n) {
      memmove((char *) p + i*size, (char *) p + (i+n)*size, size * (obarr_len2(p)-i));
      obarrhead2(p)->len -= n;
   }
   *pp = p;
}

#define OBBG_MEMGUARD
typedef struct meminfo_s
{
   char *info;
   struct meminfo_s *prev, *next;
   uint32 size;
   #ifdef OBBG_MEMGUARD
   uint32 guard;
   #endif
} meminfo;

meminfo *memlist_head;
uint32 total_allocs, active_allocs, total_frees;

// call inside mutex
static void check_mem(void*ptr)
{
   #if 0
   int found=0;
   meminfo *m = memlist_head, *p=NULL;
   while (m != NULL) {
      if (m == ptr)
         found=1;
      assert(m->prev == p);
      p = m;
      m = m->next;
   }
   if (ptr != NULL)
      assert(found);
   #endif
}

void * obbg_malloc(size_t size, char *info)
{
   meminfo *m;
   void *p = malloc(size + sizeof(meminfo));
   if (p == NULL) return p;

   m = p;
   m->info = info;
   m->prev = NULL;
   m->size = size;
   #ifdef OBBG_MEMGUARD
   m->guard = 0x98765432;
   #endif

   SDL_LockMutex(memory_mutex);
      check_mem(0);
      m->next = memlist_head;
      if (memlist_head != NULL)
         memlist_head->prev = m;
      memlist_head = m;
      check_mem(0);
   ++total_allocs;
   ++active_allocs;
   SDL_UnlockMutex(memory_mutex);
   return m+1;
}

void obbg_free(void *p)
{
   meminfo *m;
   m = ((meminfo *) p) - 1;
   #ifdef OBBG_MEMGUARD
   assert(m->guard == 0x98765432);
   #endif

   SDL_LockMutex(memory_mutex);
      check_mem(m);

      if (m->prev == NULL) {
         assert(memlist_head == m);
         memlist_head = m->next;
      } else {
         m->prev->next = m->next;
      }
      if (m->next != NULL) {
         m->next->prev = m->prev;
      }
      check_mem(0);

      ++total_frees;
      --active_allocs;
   SDL_UnlockMutex(memory_mutex);

   free(m);
}

void *obbg_realloc(void *p, size_t size, char *info)
{
   if (size == 0) {
      if (p != NULL)
         obbg_free(p);
      return NULL;
   } else if (p == NULL) {
      return obbg_malloc(size, info);
   } else {
      meminfo *q;
      meminfo *m = ((meminfo *) p) - 1;
      #ifdef OBBG_MEMGUARD
      assert(m->guard == 0x98765432);
      #endif
      q = obbg_malloc(size + sizeof(meminfo), m->info);
      if (q == NULL)
         return NULL;
      memcpy(q, p, stb_min(size,m->size));
      obbg_free(p);
      return q;
   }
}

void obbg_malloc_dump(obbg_malloc_dump_func *f)
{
   meminfo *m;
   meminfo *p=NULL;
   SDL_LockMutex(memory_mutex);
   m = memlist_head;
   while (m != NULL) {
      assert((size_t) m->next != 0xfeeefeee);
      assert(m->guard != 0xfeeefeee);
      assert((size_t) m->info != 0xcdcdcdcd);
      assert(m->guard != 0xcdcdcdcd);
      f(m->size, m->info);
      p = m;
      m = m->next;
   }
   SDL_UnlockMutex(memory_mutex);
}

// based on Ken Perlin's two-link IK solver http://mrl.nyu.edu/~perlin/ik/index.html
// original source code http://mrl.nyu.edu/~perlin/ik/ik.java.html
// licensed under whatever Ken Perlin's license was (it is a mystery)
extern int stb_two_link_ik(float mid[3], const float begin[3], const float end[3], const float mid_dir[3], float begin_to_mid_len, float mid_to_end_len);

#include <math.h>
static float stbik__dot(float *p1, float *p2)
{
   return p1[0]*p2[0] + p1[1]*p2[1] + p1[2]*p2[2];
}

static float stbik__len(float *v)
{
   return (float) sqrt(stbik__dot(v,v));
}

static void stbik__normalize(float *v)
{
   float inv_len = 1.0f / stbik__len(v);
   v[0] *= inv_len;
   v[1] *= inv_len;
   v[2] *= inv_len;
}

static void stbik__cross(float *c, float *a, float *b)
{
   c[0] = a[1]*b[2] - a[2]*b[1];
   c[1] = a[2]*b[0] - a[0]*b[2];
   c[2] = a[0]*b[1] - a[1]*b[0];
}

static void stbik__rot(float *dst, float *m, float *src)
{
   dst[0] = stbik__dot(&m[0], src);
   dst[1] = stbik__dot(&m[3], src);
   dst[2] = stbik__dot(&m[6], src);
}

static float stbik__find_d(float a, float b, float c)
{
   float r = (c+(a*a-b*b)/c)/2;
   if (r < 0) return 0;
   if (r > a) return a;
   return r;
}

static float stbik__find_e(float a, float d)
{
   return (float) sqrt(a*a-d*d);
}

static void stbik__define_m(float *m, float *m_inv, float *p, float *d)
{
   float d_dot_x;
   float *x=m_inv+0, *y=m_inv+3, *z=m_inv+6;
   int i;
   for (i=0; i < 3; ++i)
      x[i] = p[i];
   stbik__normalize(x);
   d_dot_x = stbik__dot(d,x);
   for (i=0; i < 3; ++i)
      y[i] = d[i] - d_dot_x * x[i];
   stbik__normalize(y);
   stbik__cross(z,x,y);
   for (i=0; i < 3; ++i) {
      m[i*3+0] = x[i];
      m[i*3+1] = y[i];
      m[i*3+2] = z[i];
   }
}

int stb_two_link_ik(float q[3], const float start[3], const float P[3], const float D[3], float a, float b)
{
   float p[3] = { P[0] - start[0], P[1] - start[1], P[2] - start[2] };
   float *dv = (float *) D;
   float m[9], m_inv[9], d, e, s[3];
   float r[3];

   stbik__define_m(m,m_inv, p,dv);
   stbik__rot(r, m_inv, p);
   d = stbik__find_d(a,b,stbik__len(r));
   e = stbik__find_e(a,d);
   s[0] = d;
   s[1] = e;
   s[2] = 0;
   stbik__rot(q, m, s);
   q[0] += start[0];
   q[1] += start[1];
   q[2] += start[2];
   return (d > 0) && (d < a);
}
