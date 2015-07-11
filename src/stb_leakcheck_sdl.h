// stb_leakcheck_sdl.h - v0.2 - quick & dirty malloc leak-checking - public domain
// uses SDL mutexes
#ifndef INCLUDE_STB_LEAKCHECK_H
#include <malloc.h>
#endif

#ifdef STB_LEAKCHECK_IMPLEMENTATION
#undef STB_LEAKCHECK_IMPLEMENTATION // don't implenment more than once

#include "sdl_thread.h"

#include <assert.h>
#include <string.h>

static SDL_mutex *alloc_mutex;

// if we've already included leakcheck before, undefine the macros
#ifdef malloc
#undef malloc
#undef free
#undef realloc
#undef strdup
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
typedef struct malloc_info stb_leakcheck_malloc_info;

#define MAGIC   0x3AC1CA14

struct malloc_info
{
   unsigned int magic;
   char *file;
   int line;
   size_t size;
   stb_leakcheck_malloc_info *next,*prev;
};

static stb_leakcheck_malloc_info *mi_head;

void *stb_leakcheck_malloc(size_t sz, char *file, int line)
{
   stb_leakcheck_malloc_info *mi = (stb_leakcheck_malloc_info *) malloc(sz + sizeof(*mi));
   if (mi == NULL) return mi;
   if (alloc_mutex == NULL)
      alloc_mutex = SDL_CreateMutex();
   mi->magic = MAGIC;
   mi->file = file;
   mi->line = line;
   SDL_LockMutex(alloc_mutex);
   mi->next = mi_head;
   if (mi_head)
      mi_head->prev = mi;
   mi->prev = NULL;
   mi->size = (int) sz;
   mi_head = mi;
   SDL_UnlockMutex(alloc_mutex);
   return mi+1;
}

char * stb_leakcheck_strdup(char *s, char *file, int line)
{
   char *t = stb_leakcheck_malloc(strlen(s)+1, file, line);
   if (t)
      strcpy(t, s);
   return t;
}

void stb_leakcheck_free(void *ptr)
{
   if (ptr != NULL) {
      stb_leakcheck_malloc_info *mi = (stb_leakcheck_malloc_info *) ptr - 1;
      mi->size = ~mi->size;
      #ifndef STB_LEAKCHECK_SHOWALL
      SDL_LockMutex(alloc_mutex);
      if (mi->prev == NULL) {
         assert(mi_head == mi);
         mi_head = mi->next;
      } else
         mi->prev->next = mi->next;
      if (mi->next)
         mi->next->prev = mi->prev;
      SDL_UnlockMutex(alloc_mutex);
      free(mi);
      #endif
   }
}

void *stb_leakcheck_realloc(void *ptr, size_t sz, char *file, int line)
{
   if (ptr == NULL) {
      return stb_leakcheck_malloc(sz, file, line);
   } else if (sz == 0) {
      stb_leakcheck_free(ptr);
      return NULL;
   } else {
      stb_leakcheck_malloc_info *mi = (stb_leakcheck_malloc_info *) ptr - 1;
      if (sz <= mi->size)
         return ptr;
      else {
         size_t old_size = mi->size;
         #ifdef STB_LEAKCHECK_REALLOC_PRESERVE_MALLOC_FILELINE
         void *q = stb_leakcheck_malloc(sz, mi->file, mi->line);
         #else
         void *q = stb_leakcheck_malloc(sz, file, line);
         #endif
         if (q) {
            memcpy(q, ptr, old_size < sz ? old_size : sz);
            stb_leakcheck_free(ptr);
         }
         return q;
      }
   }
}

void stb_leakcheck_dumpmem(void)
{
   stb_leakcheck_malloc_info *mi;
   FILE *f = fopen("c:/stb_leakcheck_dump.txt", "w");
   SDL_LockMutex(alloc_mutex);
   mi = mi_head;
   while (mi) {
      if ((ptrdiff_t) mi->size >= 0)
         fprintf(f, "LEAKED: %9d bytes at %p: %s (%4d)\n", mi->size, mi+1, mi->file, mi->line);
      mi = mi->next;
   }
   #ifdef STB_LEAKCHECK_SHOWALL
   mi = mi_head;
   while (mi) {
      if ((ptrdiff_t) mi->size < 0)
         fprintf(f, "FREED : %9d bytes at %p: %s (%4d)\n", ~mi->size, mi+1, mi->file, mi->line);
      mi = mi->next;
   }
   #endif
   SDL_UnlockMutex(alloc_mutex);
   fclose(f);
}
#endif // STB_LEAKCHECK_IMPLEMENTATION

#ifndef INCLUDE_STB_LEAKCHECK_H
#define INCLUDE_STB_LEAKCHECK_H

#ifdef STB_LEAKCHECK_ENABLE
#define malloc(sz)    stb_leakcheck_malloc(sz, __FILE__, __LINE__)
#define free(p)       stb_leakcheck_free(p)
#define realloc(p,sz) stb_leakcheck_realloc(p,sz, __FILE__, __LINE__)
#define strdup(str)   stb_leakcheck_strdup(str, __FILE__, __LINE__)

extern void * stb_leakcheck_malloc(size_t sz, char *file, int line);
extern void * stb_leakcheck_realloc(void *ptr, size_t sz, char *file, int line);
extern void   stb_leakcheck_free(void *ptr);
extern char * stb_leakcheck_strdup(char *s, char *file, int line);
extern void   stb_leakcheck_dumpmem(void);
#endif

#endif // INCLUDE_STB_LEAKCHECK_H
