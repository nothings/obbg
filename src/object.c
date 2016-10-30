//#define STB_LEAKCHECK_IMPLEMENTATION
//#include "stb_leakcheck_sdl.h"

#include "obbg_data.h"

object obj[MAX_OBJECTS];
// player_object obj[PLAYER_OBJECT_MAX];
player_controls p_input[PLAYER_OBJECT_MAX];
brain_state brains[MAX_BRAINS];
static int max_brain_id;

type_properties type_prop[5] =
{
   { 0 }, // none
   { 0.45f, 0.45f, 3.5f, 0.25f, 1.4f }, // player
   { 0.25f, 0.25f, 0.30f, 0,0 }, // test
   { 0.15f, 0.15f, 0.30f, 0,0 }, // bounce
   { 0.45f, 0.45f, 2.5f, 0.25f, 1.4f }, // critter
};


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

brain_state *allocate_brain(void)
{
   int i;
   // @OPTIMIZE free list
   for (i=0; i < MAX_BRAINS; ++i)
      if (!brains[i].valid) {
         if (i >= max_brain_id)
            max_brain_id = i+1;
         brains[i].valid = True;
         return &brains[i];
      }
   return 0;
}
