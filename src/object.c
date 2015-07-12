#define STB_LEAKCHECK_IMPLEMENTATION
#include "stb_leakcheck_sdl.h"

#include "obbg_data.h"

object obj[MAX_OBJECTS];
// player_object obj[PLAYER_OBJECT_MAX];
player_controls p_input[PLAYER_OBJECT_MAX];

objid max_obj_id, max_player_id;

objid allocate_object(void)
{
   int i;
   // @OPTIMIZE free list
   for (i=PLAYER_OBJECT_MAX; i < MAX_OBJECTS; ++i)
      if (!obj[i].valid) {
         if (i >= max_obj_id)
            max_obj_id = (objid) i+1;
         obj[i].valid = True;
         return (objid) i;
      }
   return 0;
}

objid allocate_player(void)
{
   int i;
   // @OPTIMIZE free list
   for (i=1; i < PLAYER_OBJECT_MAX; ++i)
      if (!obj[i].valid) {
         if (i >= max_player_id)
            max_player_id = (objid) i+1;
         obj[i].valid = True;
         return (objid) i;
      }
   return 0;
}
