#include "obbg_funcs.h"
#include "SDL.h"
#define STB_GLEXT_DECLARE "glext_list.h"
#include "stb_gl.h"

#pragma warning(disable:4305; disable:4244)
enum
{
   UI_SCREEN_none,
   UI_SCREEN_select
};
int ui_screen=UI_SCREEN_none;

int selected_block[3];
int selected_block_to_create[3];
Bool selected_block_valid;
static int block_rotation;
static int block_choice = 0;
static int mouse_x, mouse_y;
int actionbar_blocktype[9] =
{
   BT_conveyor,
   // BT_conveyor_90_left, BT_conveyor_90_right,
   //BT_conveyor_ramp_up_low, BT_conveyor_ramp_down_high, BT_splitter,
   //BT_stone, BT_asphalt,
   //BT_picker, BT_ore_drill,
};


void rotate_block(void)
{
   int block = get_block(selected_block[0], selected_block[1], selected_block[2]);
   if (block >= BT_placeable) {
      int rot = get_block_rot(selected_block[0], selected_block[1], selected_block[2]);
      rot = (rot+1) & 3;
      block_rotation = rot;
      change_block(selected_block[0], selected_block[1], selected_block[2], block, rot);
   }
}

static int quit;
int hack_ffwd;

void active_control_set(int key)
{
   client_player_input.buttons |= 1 << key;
}

void active_control_clear(int key)
{
   client_player_input.buttons &= ~(1 << key);
}

typedef struct
{
   int y_base, y_size;
   int x_base, x_size, x_spacing;
   int count;
} ui_rect_row;

int sprite_for_blocktype[256];

void get_coordinates(ui_rect_row *row, int item, int *x, int *y)
{
   *x = row->x_base + (row->x_size + row->x_spacing)*item;
   *y = row->y_base;
}

int hit_detect_row(ui_rect_row *row, recti *box)
{
   int i;
   int x_advance = row->x_size + row->x_spacing;
   float y0 = row->y_base;
   float y1 = y0 + row->y_size;
   float x0 = row->x_base;
   float x1 = x0 + row->x_size;

   static recti mbox = { 0,0,0,0 };
   if (box == NULL)
      box = &mbox;

   for (i=0; i < row->count; ++i) {
      if (mouse_x+box->x0 < x1 && mouse_x+box->x1 > x0 &&
          mouse_y+box->y0 < y1 && mouse_y+box->y1 > y0)
         return i;

      x0 += x_advance;
      x1 += x_advance;
   }
   return -1;
}

void draw_ui_row(ui_rect_row *row, int *blockcodes, int hit_item, int choice)
{
   int i;
   int x_advance = row->x_size + row->x_spacing;
   float y0 = row->y_base;
   float y1 = y0 + row->y_size;
   float x0 = row->x_base;
   float x1 = x0 + row->x_size;
   for (i=0; i < row->count; ++i) {
      glDisable(GL_TEXTURE_2D);
      glDisable(GL_BLEND);
      if (choice == i) {
         glColor3f(0.8,0.4,0.8);
         stbgl_drawRect(x0-5,y0-5,x1+5,y1+5);
      }
      if (hit_item==i) {
         glColor3f(1.0,1.0,1.0);
         stbgl_drawRect(x0-3,y0-3,x1+3,y1+3);
      }
      glColor3f(0.9,0.9,0.9);
      stbgl_drawRect(x0,y0,x1,y1);

      if (blockcodes != NULL) {
         int sprite = sprite_for_blocktype[blockcodes[i]];
         if (sprite != 0) {
            glEnable(GL_TEXTURE_2D);
            glEnable(GL_BLEND);
            glBindTexture(GL_TEXTURE_2D, sprite);
            stbgl_drawRectTC(x0,y0,x1,y1, 0,0,1,1);
         }
      }

      x0 += x_advance;
      x1 += x_advance;
   }
}

ui_rect_row ui_actionbar[1];
ui_rect_row ui_inventory[3];

int inventory_blocktype[3][9] =
{
   { BT_stone, BT_asphalt, } ,
   { BT_picker, BT_ore_drill, BT_furnace, BT_iron_gear_maker, BT_conveyor_belt_maker },
   { BT_conveyor, BT_conveyor_90_left, BT_conveyor_90_right,
       BT_conveyor_ramp_up_low, BT_conveyor_ramp_down_high, BT_splitter, },
};

void draw_action_bar(int hit_item)
{
   draw_ui_row(ui_actionbar, actionbar_blocktype, hit_item, block_choice);
}

float left_x_to_center_contents_of_width(float size)
{
   return screen_x/2 - size/2;
}

float top_y_to_center_contents_of_height(float size)
{
   return screen_y/2 - size/2;
}

void compute_ui_actionbar(void)
{
   ui_rect_row *rr = &ui_actionbar[0];
   rr->x_size = 50;
   rr->y_size = 50;
   rr->x_spacing = 8;
   rr->count = 9;
   rr->y_base = screen_y - rr->y_size;
   rr->x_base = left_x_to_center_contents_of_width(rr->x_size * rr->count + rr->x_spacing*(rr->count-1));
}

void compute_ui_inventory(void)
{
   int i;
   ui_rect_row *rr;
   int y_spacing = 8;
   int y_size = 50;
   int advance = y_size + y_spacing;

   for (i=0; i < 3; ++i) {
      ui_inventory[i].x_size = 50;
      ui_inventory[i].y_size = y_size;
      ui_inventory[i].x_spacing = 8;
      ui_inventory[i].count = 9;
   }

   rr = &ui_inventory[0];
   ui_inventory[1].x_base =
   ui_inventory[2].x_base = 
   rr->x_base = left_x_to_center_contents_of_width(rr->x_size * rr->count + rr->x_spacing*(rr->count-1));

   rr->y_base = top_y_to_center_contents_of_height(rr->y_size * 3 + y_spacing*2);
   ui_inventory[1].y_base = ui_inventory[0].y_base + advance;
   ui_inventory[2].y_base = ui_inventory[1].y_base + advance;
}

static Bool dragging = False;
static int mouse_drag_offset_x, mouse_drag_offset_y;
static int drag_item;

void mouse_down(int button)
{
   dragging = False;
   switch (ui_screen) {
      case UI_SCREEN_select: {
         int j;
         for (j=0; j < 3; ++j) {
            int hit = hit_detect_row(&ui_inventory[j], NULL);
            if (hit >= 0) {
               int x,y;
               dragging = True;
               get_coordinates(&ui_inventory[j], hit, &x,&y);
               mouse_drag_offset_x = x - mouse_x;
               mouse_drag_offset_y = y - mouse_y;
               drag_item = inventory_blocktype[j][hit];
            }
         }
         break;
      }
      
      case UI_SCREEN_none:
         if (selected_block_valid) {
            if (button == 1)
               change_block(selected_block[0], selected_block[1], selected_block[2], BT_empty, block_rotation);
            else if (button == -1) {
               int block = actionbar_blocktype[block_choice];
               if (block != BT_empty)
                  change_block(selected_block_to_create[0], selected_block_to_create[1], selected_block_to_create[2], block, block_rotation);
            }
         }
         break;
   }
}

void mouse_up(void)
{
   if (dragging) {
      int hit;
      recti shape;
      shape.x0 = mouse_drag_offset_x;
      shape.y0 = mouse_drag_offset_y;
      shape.x1 = shape.x0 + 32;
      shape.y1 = shape.y0 + 32;
      shape.x0 += 8;
      shape.y0 += 8;
      shape.x1 -= 8;
      shape.y1 -= 8;
      hit = hit_detect_row(ui_actionbar, &shape);
      if (hit >= 0) {
         actionbar_blocktype[hit] = drag_item;
      }
      dragging = False;
   }
}

extern void update_view(float dx, float dy);

static Bool first_mouse=True;

void  process_mouse_move(int dx, int dy)
{
   if (ui_screen != UI_SCREEN_none) {
      mouse_x += dx*2;
      mouse_y += dy*2;
      mouse_x = stb_clamp(mouse_x, 0, screen_x);
      mouse_y = stb_clamp(mouse_y, 0, screen_y);
   } else {
      update_view((float) dx / screen_x, (float) dy / screen_y);
   }
}

void process_key_down(int k, int s, SDL_Keymod mod)
{
   if (k == SDLK_ESCAPE)
      quit = 1;

   // player movement
   if (s == SDL_SCANCODE_D)    active_control_set(0);
   if (s == SDL_SCANCODE_A)    active_control_set(1);
   if (s == SDL_SCANCODE_W)    active_control_set(2);
   if (s == SDL_SCANCODE_S)    active_control_set(3);
   if (k == SDLK_SPACE)        active_control_set(4); 
   if (s == SDL_SCANCODE_LCTRL)active_control_set(5);
   if (s == SDL_SCANCODE_S)    active_control_set(6);
   if (s == SDL_SCANCODE_D)    active_control_set(7);
   if (s == SDL_SCANCODE_F)    client_player_input.flying = !client_player_input.flying;

   // debugging
   if (s == SDL_SCANCODE_H) global_hack = !global_hack;
   if (s == SDL_SCANCODE_P) debug_render = !debug_render;
   if (s == SDL_SCANCODE_C) show_memory = !show_memory;//examine_outstanding_genchunks();

   if (s == SDL_SCANCODE_TAB) {
      ui_screen = UI_SCREEN_select;
      compute_ui_inventory();
   }

   if (ui_screen != UI_SCREEN_none && first_mouse) {
      first_mouse = False;
      mouse_x = screen_x/2;
      mouse_y = screen_y/2;
   }
   if (s == SDL_SCANCODE_R)    rotate_block();
   if (s == SDL_SCANCODE_M)    save_edits();
   if (s == SDL_SCANCODE_O)    hack_ffwd = !hack_ffwd;
   if (k >= '1' && k <= '9')
      block_choice = k-'1';
   //if (k == '6') block_base = BT_conveyor_up_east_low;
   //if (k == '2') global_hack = -1;
   //if (k == '3') obj[player_id].position.x += 65536;
   #if 0
   if (s == SDL_SCANCODE_R) {
      objspace_to_worldspace(light_vel, player_id, 0,32,0);
      memcpy(light_pos, &obj[player_id].position, sizeof(light_pos));
   }
   #endif

   #if 0
   if (game_mode == GAME_editor) {
      switch (k) {
         case SDLK_RIGHT: editor_key(STBTE_scroll_right); break;
         case SDLK_LEFT : editor_key(STBTE_scroll_left ); break;
         case SDLK_UP   : editor_key(STBTE_scroll_up   ); break;
         case SDLK_DOWN : editor_key(STBTE_scroll_down ); break;
      }
      switch (s) {
         case SDL_SCANCODE_S: editor_key(STBTE_tool_select); break;
         case SDL_SCANCODE_B: editor_key(STBTE_tool_brush ); break;
         case SDL_SCANCODE_E: editor_key(STBTE_tool_erase ); break;
         case SDL_SCANCODE_R: editor_key(STBTE_tool_rectangle ); break;
         case SDL_SCANCODE_I: editor_key(STBTE_tool_eyedropper); break;
         case SDL_SCANCODE_L: editor_key(STBTE_tool_link); break;
         case SDL_SCANCODE_G: editor_key(STBTE_act_toggle_grid); break;
      }
      if ((e->key.keysym.mod & KMOD_CTRL) && !(e->key.keysym.mod & ~KMOD_CTRL)) {
         switch (s) {
            case SDL_SCANCODE_X: editor_key(STBTE_act_cut  ); break;
            case SDL_SCANCODE_C: editor_key(STBTE_act_copy ); break;
            case SDL_SCANCODE_V: editor_key(STBTE_act_paste); break;
            case SDL_SCANCODE_Z: editor_key(STBTE_act_undo ); break;
            case SDL_SCANCODE_Y: editor_key(STBTE_act_redo ); break;
         }
      }
   }
   #endif
}

void process_key_up(int k, int s)
{
   if (s == SDL_SCANCODE_D)   active_control_clear(0);
   if (s == SDL_SCANCODE_A)   active_control_clear(1);
   if (s == SDL_SCANCODE_W)   active_control_clear(2);
   if (s == SDL_SCANCODE_S)   active_control_clear(3);
   if (k == SDLK_SPACE)       active_control_clear(4); 
   if (s == SDL_SCANCODE_LCTRL)   active_control_clear(5);
   if (s == SDL_SCANCODE_S)   active_control_clear(6);
   if (s == SDL_SCANCODE_D)   active_control_clear(7);
   if (s == SDL_SCANCODE_TAB) {
      ui_screen = UI_SCREEN_none;
   }
}


GLuint icon_arrow;
void do_ui_rendering_2d(void)
{
   compute_ui_actionbar();

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   glDisable(GL_BLEND);
   glDisable(GL_CULL_FACE);
   glDisable(GL_DEPTH_TEST);

   {
      float cx = screen_x / 2.0f;
      float cy = screen_y / 2.0f;
      glColor3f(1,1,1);
      glBegin(GL_LINES);
      glVertex2f(cx-4,cy); glVertex2f(cx+4,cy);
      glVertex2f(cx,cy-3); glVertex2f(cx,cy+3);
      glEnd();
   }

   switch (ui_screen) {
      case UI_SCREEN_select: {
         int hit;
         int j;
         glColor3f(0.7,0.7,0.7);
         stbgl_drawRect(screen_x*1.0/4.0, screen_y*1.0/4.0, screen_x*3.0/4.0, screen_y*3.0/4.0);
         for (j=0; j < 3; ++j) {
            int ihit = dragging ? -1 : hit_detect_row(&ui_inventory[j], NULL);
            draw_ui_row(&ui_inventory[j], &inventory_blocktype[j][0], ihit, -1);
         }
         hit = -1;
         if (dragging) {
            recti shape;
            shape.x0 = mouse_drag_offset_x;
            shape.y0 = mouse_drag_offset_y;
            shape.x1 = shape.x0 + 32;
            shape.y1 = shape.y0 + 32;
            shape.x0 += 8;
            shape.y0 += 8;
            shape.x1 -= 8;
            shape.y1 -= 8;
            hit = hit_detect_row(&ui_actionbar[0], &shape);
         }
         draw_action_bar(hit);
         break;
      }
      case UI_SCREEN_none: {
         draw_action_bar(-1);
         break;
      }
   }

   if (ui_screen != UI_SCREEN_none) {
      float mx,my;

      glEnable(GL_TEXTURE_2D);
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      if (dragging) {
         glBindTexture(GL_TEXTURE_2D, sprite_for_blocktype[drag_item]);
         mx = mouse_x + mouse_drag_offset_x;
         my = mouse_y + mouse_drag_offset_y;
         stbgl_drawRectTC(mx,my, mx+ui_inventory[0].x_size,my+ui_inventory[0].y_size, 0,0,1,1);
      }

      mx = mouse_x - 5;
      my = mouse_y - 6;

      glBindTexture(GL_TEXTURE_2D, icon_arrow);
      stbgl_drawRectTC(mx,my, mx+32,my+32, 0,0,1,1);
      glDisable(GL_BLEND);
      glDisable(GL_TEXTURE_2D);
   }
}

void do_ui_rendering_3d(void)
{
   int i;
   vec pos[2];
   RaycastResult result;
   // show wireframe of currently 'selected' block
   objspace_to_worldspace(&pos[1].x, player_id, 0,9,0);
   pos[0] = obj[player_id].position;
   pos[1].x += pos[0].x;
   pos[1].y += pos[0].y;
   pos[1].z += pos[0].z;
   selected_block_valid = raycast(pos[0].x, pos[0].y, pos[0].z, pos[1].x, pos[1].y, pos[1].z, &result);
   if (selected_block_valid) {
      for (i=0; i < 3; ++i) {
         selected_block[i] = (&result.bx)[i];
         selected_block_to_create[i] = (&result.bx)[i] + face_dir[result.face][i];
      }
      glColor3f(0.7f,1.0f,0.7f);
      //stbgl_drawBox(selected_block_to_create[0]+0.5f, selected_block_to_create[1]+0.5f, selected_block_to_create[2]+0.5f, 1.2f, 1.2f, 1.2f, 0);
      stbgl_drawBox(selected_block[0]+0.5f, selected_block[1]+0.5f, selected_block[2]+0.5f, 1.2f, 1.2f, 1.2f, 0);
   }
}

void init_ui_render(void)
{
   icon_arrow = load_sprite("data/sprites/icon_arrow.png");

   sprite_for_blocktype[BT_conveyor]                = load_sprite("data/sprites/icon_conveyor.png");
   sprite_for_blocktype[BT_conveyor_90_left]        = load_sprite("data/sprites/icon_conveyor_90_ccw.png");
   sprite_for_blocktype[BT_conveyor_90_right]       = load_sprite("data/sprites/icon_conveyor_90_cw.png");
   sprite_for_blocktype[BT_conveyor_ramp_up_low]    = load_sprite("data/sprites/icon_conveyor_up.png");
   sprite_for_blocktype[BT_conveyor_ramp_down_high] = load_sprite("data/sprites/icon_conveyor_down.png");
   sprite_for_blocktype[BT_splitter]                = load_sprite("data/sprites/icon_splitter.png");
}
