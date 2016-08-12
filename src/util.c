#include "obbg_funcs.h"
#include "sdl_thread.h"

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
   meminfo *m = memlist_head;
   meminfo *p=NULL;
   while (m != NULL) {
      assert((size_t) m->next != 0xfeeefeee);
      assert(m->guard != 0xfeeefeee);
      assert((size_t) m->info != 0xcdcdcdcd);
      assert(m->guard != 0xcdcdcdcd);
      f(m->size, m->info);
      p = m;
      m = m->next;
   }
}
