#define _WIN32_WINNT 0x400
//#define GL_DEBUG

#include <assert.h>
#include <windows.h>

// stb.h
#include "stb_leakcheck_sdl.h"

#define STB_DEFINE
#include "stb.h"

#include "obbg_funcs.h"

// stb_gl.h
#define STB_GL_IMPLEMENTATION
#define STB_GLEXT_DEFINE "glext_list.h"
#include "stb_gl.h"

// SDL
#include "sdl.h"
#include "SDL_opengl.h"
#include "SDL_net.h"

// stb_glprog.h
#define STB_GLPROG_IMPLEMENTATION
#define STB_GLPROG_ARB_DEFINE_EXTENSIONS
#include "stb_glprog.h"

// stb_image.h
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// stb_easy_font.h
#include "stb_easy_font.h" // doesn't require an IMPLEMENTATION

char *game_name = "obbg";


#define REVERSE_DEPTH


char *dumb_fragment_shader =
   "#version 150 compatibility\n"
   "uniform sampler2DArray tex;\n"
   "void main(){gl_FragColor = texture(tex,gl_TexCoord[0].xyz);}";


extern int load_crn_to_texture(unsigned char *data, size_t length);
extern int load_crn_to_texture_array(int slot, unsigned char *data, size_t length);

GLuint debug_tex, dumb_prog;
unsigned int voxel_tex[2];

typedef struct
{
   float scale;
   char *filename;
} texture_info;

texture_info textures[] =
{
   1.0/8,"ground/Beach_sand_pxr128",
   1.0/4,"ground/Bowling_grass_pxr128",
   1,"ground/Dirt_and_gravel_pxr128",
   1,"ground/Fine_gravel_pxr128",
   1.0/2,"ground/Ivy_pxr128",

   1,"ground/Lawn_grass_pxr128",
   1,"ground/Pebbles_in_mortar_pxr128",
   1,"ground/Peetmoss_pxr128",
   1,"ground/Red_gravel_pxr128",
   1,"ground/Street_asphalt_pxr128",

   1,"floor/Wool_carpet_pxr128",
   1,"brick/Pink-brown_painted_pxr128",
   1,"brick/Building_block_pxr128",
   1,"brick/Standard_red_pxr128",
   1,"siding/Diagonal_cedar_pxr128",

   1,"siding/Vertical_redwood_pxr128",
   1,"stone/Gray_marble_pxr128",
   1,"stone/Buffed_marble_pxr128",
   1,"stone/Black_marble_pxr128",
   1,"stone/Blue_marble_pxr128",

   1,"stone/Gray_granite_pxr128",
};

void set_blocktype_texture(int bt, int tex)
{
   int i;
   for (i=0; i < 6; ++i)
      tex1_for_blocktype[bt][i] = tex;
}

void init_mesh_building(void)
{
#if 0
   int i,j;
   for (i=0; i < 256; ++i)
      for (j=0; j < 6; ++j)
         tex1_for_blocktype[i][j] = (uint8) i-1;
#endif

   set_blocktype_texture(BT_sand, 0);
   set_blocktype_texture(BT_grass, 5);
   set_blocktype_texture(BT_gravel, 2);
   set_blocktype_texture(BT_asphalt, 9);
   set_blocktype_texture(BT_wood, 15);
   set_blocktype_texture(BT_marble, 16);
   set_blocktype_texture(BT_stone, 20);
   set_blocktype_texture(BT_leaves, 1);

   // { int i; for (i=0; i < 20; ++i) set_blocktype_texture(i, 0); }
}



static float camera_bounds[2][3];

float texture_scales[256];

void game_init(void)
{
   init_chunk_caches();
   init_mesh_building();
   init_mesh_build_threads();
   s_init_physics_cache();
}

void render_init(void)
{
   // @TODO: support non-DXT path
   char **files = stb_readdir_recursive("data", "*.crn");
   int i;

   camera_bounds[0][0] = - 0.75f;
   camera_bounds[0][1] = - 0.75f;
   camera_bounds[0][2] = - 4.25f;
   camera_bounds[1][0] = + 0.75f;
   camera_bounds[1][1] = + 0.75f;
   camera_bounds[1][2] = + 0.25f;

   glGenTextures(2, voxel_tex);

   glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, voxel_tex[0]);

   for (i=0; i < 11; ++i) {
      glTexImage3DEXT(GL_TEXTURE_2D_ARRAY_EXT, i,
                         GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                         1024>>i,1024>>i,128,0,
                         GL_RGBA,GL_UNSIGNED_BYTE,NULL);
   }

   #if 1
   for (i=0; i < sizeof(textures)/sizeof(textures[0]); ++i) {
      size_t len;
      char *filename = stb_sprintf("data/pixar/crn/%s.crn", textures[i].filename);
      uint8 *data = stb_file(filename, &len);
      load_crn_to_texture_array(i, data, len);
      free(data);
      texture_scales[i] = 1.0f/4;// textures[i].scale;
   }
   #endif

   // temporary hack:
   voxel_tex[1] = voxel_tex[0];

   init_voxel_render(voxel_tex);

   {
      char *frag[] = { dumb_fragment_shader, NULL };
      char error[1024];
      GLuint fragment;
      fragment = stbgl_compile_shader(STBGL_FRAGMENT_SHADER, frag, -1, error, sizeof(error));
      if (!fragment) {
         ods("oops");
         exit(0);
      }
      dumb_prog = stbgl_link_program(0, fragment, NULL, -1, error, sizeof(error));
      
   }

   #if 0
   {
      size_t len;
      unsigned char *data = stb_file(files[0], &len);
      glGenTextures(1, &debug_tex);
      glBindTexture(GL_TEXTURE_2D, debug_tex);
      load_crn_to_texture(data, len);
      free(data);
   }
   #endif
}


static void print_string(float x, float y, char *text, float r, float g, float b)
{
   static char buffer[99999];
   int num_quads;
   
   num_quads = stb_easy_font_print(x, y, text, NULL, buffer, sizeof(buffer));

   glColor3f(r,g,b);
   glEnableClientState(GL_VERTEX_ARRAY);
   glVertexPointer(2, GL_FLOAT, 16, buffer);
   glDrawArrays(GL_QUADS, 0, num_quads*4);
   glDisableClientState(GL_VERTEX_ARRAY);
}

static float text_color[3];
static float pos_x = 10;
static float pos_y = 10;

static void print(char *text, ...)
{
   char buffer[999];
   va_list va;
   va_start(va, text);
   vsprintf(buffer, text, va);
   va_end(va);
   print_string(pos_x, pos_y, buffer, text_color[0], text_color[1], text_color[2]);
   pos_y += 10;
}

//object player = { { 0,0,150 } };


float camang[3], camloc[3] = { 0,-10,80 };

// camera worldspace velocity
float light_pos[3];
float light_vel[3];

float pending_view_x;
float pending_view_z;

player_controls client_player_input;

float pending_dt;

int program_mode = MODE_single_player;

void process_tick(float dt)
{
   dt += pending_dt;
   pending_dt = 0;
   while (dt > 1.0f/60) {
      switch (program_mode) {
         case MODE_server:
            server_net_tick_pre_physics();
            process_tick_raw(1.0f/60);
            server_net_tick_post_physics();
            break;
         case MODE_client:
            client_view_physics(player_id, &client_player_input, dt);
            client_net_tick();
            break;
         case MODE_single_player:
            client_view_physics(player_id, &client_player_input, dt);
            p_input[player_id] = client_player_input;
            process_tick_raw(1.0f/60);
            break;
      }

      dt -= 1.0f/60;
   }
   pending_dt += dt;
}

void update_view(float dx, float dy)
{
   // hard-coded mouse sensitivity, not resolution independent?
   pending_view_z -= dx*300;
   pending_view_x -= dy*700;
}

float render_time;

int screen_x, screen_y;
int is_synchronous_debug;

int chunk_locations, chunks_considered, chunks_in_frustum;
int quads_considered, quads_rendered;
int chunk_storage_rendered, chunk_storage_considered, chunk_storage_total;
int view_dist_for_display;
int num_threads_active, num_meshes_started, num_meshes_uploaded;
float chunk_server_activity;

static Uint64 start_time, end_time; // render time

float chunk_server_status[32];
int chunk_server_pos;

extern vec3i physics_cache_feedback[64][64];

void draw_stats(void)
{
   int i;

   static Uint64 last_frame_time;
   Uint64 cur_time = SDL_GetPerformanceCounter();
   float chunk_server=0;
   float frame_time = (cur_time - last_frame_time) / (float) SDL_GetPerformanceFrequency();
   last_frame_time = cur_time;

   chunk_server_status[chunk_server_pos] = chunk_server_activity;
   chunk_server_pos = (chunk_server_pos+1) %32;

   for (i=0; i < 32; ++i)
      chunk_server += chunk_server_status[i] / 32.0f;

   stb_easy_font_spacing(-0.75);
   pos_y = 10;
   text_color[0] = text_color[1] = text_color[2] = 1.0f;
   print("Frame time: %6.2fms, CPU frame render time: %5.2fms", frame_time*1000, render_time*1000);
   print("Tris: %4.1fM drawn of %4.1fM in range", 2*quads_rendered/1000000.0f, 2*quads_considered/1000000.0f);
   print("Vbuf storage: %dMB in frustum of %dMB in range of %dMB in cache", chunk_storage_rendered>>20, chunk_storage_considered>>20, chunk_storage_total>>20);
   print("Num mesh builds started this frame: %d; num uploaded this frame: %d\n", num_meshes_started, num_meshes_uploaded);
   print("QChunks: %3d in frustum of %3d valid of %3d in range", chunks_in_frustum, chunks_considered, chunk_locations);
   print("Mesh worker threads active: %d", num_threads_active);
   print("View distance: %d blocks", view_dist_for_display);
   print("x=%5.2f, y=%5.2f, z=%5.2f", obj[player_id].position.x, obj[player_id].position.y, obj[player_id].position.z);
   print("%s", glGetString(GL_RENDERER));
   #if 1
   for (i=0; i < 4; ++i)
      print("[ %4d,%4d  %4d,%4d  %4d,%4d  %4d,%4d ]",
                                   physics_cache_feedback[i][0].x, physics_cache_feedback[i][0].y, 
                                   physics_cache_feedback[i][1].x, physics_cache_feedback[i][1].y, 
                                   physics_cache_feedback[i][2].x, physics_cache_feedback[i][2].y, 
                                   physics_cache_feedback[i][3].x, physics_cache_feedback[i][3].y);
   #endif

   if (is_synchronous_debug) {
      text_color[0] = 1.0;
      text_color[1] = 0.5;
      text_color[2] = 0.5;
      print("SLOWNESS: Synchronous debug output is enabled!");
   }
}

void stbgl_drawRectTCArray(float x0, float y0, float x1, float y1, float s0, float t0, float s1, float t1, float i)
{
   glBegin(GL_POLYGON);
      glTexCoord3f(s0,t0,i); glVertex2f(x0,y0);
      glTexCoord3f(s1,t0,i); glVertex2f(x1,y0);
      glTexCoord3f(s1,t1,i); glVertex2f(x1,y1);
      glTexCoord3f(s0,t1,i); glVertex2f(x0,y1);
   glEnd();
}

Bool third_person;
float player_zoom = 1.0f;

void render_objects(void)
{
   int i;
   vec sz;
   vec pos;
   glColor3f(1,1,1);
   glDisable(GL_TEXTURE_2D);
   stbgl_drawBox(light_pos[0], light_pos[1], light_pos[2], 3,3,3, 1);

   for (i=1; i < max_player_id; ++i) {
      if (obj[i].valid && (i != player_id || third_person)) {
         sz.x = camera_bounds[1][0] - camera_bounds[0][0];
         sz.y = camera_bounds[1][1] - camera_bounds[0][1];
         sz.z = camera_bounds[1][2] - camera_bounds[0][2];
         pos.x = obj[i].position.x + (camera_bounds[1][0] + camera_bounds[0][0])/2;
         pos.y = obj[i].position.y + (camera_bounds[1][1] + camera_bounds[0][1])/2;
         pos.z = obj[i].position.z + (camera_bounds[1][2] + camera_bounds[0][2])/2;
         stbgl_drawBox(pos.x,pos.y,pos.z, sz.x,sz.y,sz.z, 1);
      }
   }
}


void draw_main(void)
{
   glEnable(GL_CULL_FACE);
   glDisable(GL_TEXTURE_2D);
   glDisable(GL_LIGHTING);
   glEnable(GL_DEPTH_TEST);
   #ifdef REVERSE_DEPTH
   glDepthFunc(GL_GREATER);
   glClearDepth(0);
   #else
   glDepthFunc(GL_LESS);
   glClearDepth(1);
   #endif
   glDepthMask(GL_TRUE);
   glDisable(GL_SCISSOR_TEST);
   glClearColor(0.6f,0.7f,0.9f,0.0f);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glColor3f(1,1,1);
   glFrontFace(GL_CW);
   glEnable(GL_TEXTURE_2D);
   glDisable(GL_BLEND);


   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();

   #ifdef REVERSE_DEPTH
   stbgl_Perspective(player_zoom, 90, 70, 3000, 1.0/16);
   #else
   stbgl_Perspective(player_zoom, 90, 70, 1.0/16, 3000);
   #endif

   // now compute where the camera should be
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   stbgl_initCamera_zup_facing_y();

   if (third_person) {
      objspace_to_worldspace(camloc, player_id, 0,-5,0);
      camloc[0] += obj[player_id].position.x;
      camloc[1] += obj[player_id].position.y;
      camloc[2] += obj[player_id].position.z;
   } else {
      camloc[0] = obj[player_id].position.x;
      camloc[1] = obj[player_id].position.y;
      camloc[2] = obj[player_id].position.z;
   }

   camang[0] = obj[player_id].ang.x;
   camang[1] = obj[player_id].ang.y;
   camang[2] = obj[player_id].ang.z;
#if 1
   glRotatef(-camang[0],1,0,0);
   glRotatef(-camang[2],0,0,1);
   glTranslatef(-camloc[0], -camloc[1], -camloc[2]);
#endif

   start_time = SDL_GetPerformanceCounter();
   render_voxel_world(camloc);

   player_zoom = 1;

   render_objects();

   end_time = SDL_GetPerformanceCounter();

   glDisable(GL_LIGHTING);
   glDisable(GL_CULL_FACE);
   glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

   if (0) {
      vec pos[2];
      RaycastResult result;
      // show wireframe of currently 'selected' block
      objspace_to_worldspace(&pos[1].x, player_id, 0,5,0);
      pos[0] = obj[player_id].position;
      if (raycast(pos[0].x, pos[0].y, pos[0].z, pos[1].x, pos[1].y, pos[1].z, &result)) {
         glColor3f(0.7f,1.0f,0.7f);
         stbgl_drawBox(result.bx+0.5f, result.by+0.5f, result.bz+0.5f, 1.2f, 1.2f, 1.2f, 0);
      }
   }

   glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

   render_time = (end_time - start_time) / (float) SDL_GetPerformanceFrequency();

   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   gluOrtho2D(0,screen_x/2,screen_y/2,0);
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   glDisable(GL_BLEND);
   glDisable(GL_CULL_FACE);
   glDisable(GL_DEPTH_TEST);

   if (0) {
      stbglUseProgram(dumb_prog);
      glDisable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, voxel_tex[0]);
      stbglUniform1i(stbgl_find_uniform(dumb_prog, "tex"), 0);
      glColor3f(1,1,1);
      stbgl_drawRectTCArray(0,0,512,512,0,0,1,1, 0.0);
      stbglUseProgram(0);
   }


   if (debug_tex) {
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, debug_tex);
      glColor3f(1,1,1);
      stbgl_drawRectTC(0,0,512,512,0,0,1,1);
   }
   glDisable(GL_TEXTURE_2D);
   draw_stats();

}



#pragma warning(disable:4244; disable:4305; disable:4018)

#define SCALE   2

void error(char *s)
{
   SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", s, NULL);
   exit(0);
}

void ods(char *fmt, ...)
{
   char buffer[1000];
   va_list va;
   va_start(va, fmt);
   vsprintf(buffer, fmt, va);
   va_end(va);
   SDL_Log("%s", buffer);
}

#define TICKS_PER_SECOND  60

static SDL_Window *window;

extern void draw_main(void);
extern void process_tick(float dt);
extern void editor_init(void);

void draw(void)
{
   draw_main();
   SDL_GL_SwapWindow(window);
}


static int initialized=0;
static float last_dt;

int screen_x,screen_y;

float carried_dt = 0;
#define TICKRATE 60

float tex2_alpha = 1.0;

int raw_level_time;

float global_timer;
int global_hack;

int loopmode(float dt, int real, int in_client)
{
   if (!initialized) return 0;

   if (!real)
      return 0;

   // don't allow more than 6 frames to update at a time
   if (dt > 0.075) dt = 0.075;

   global_timer += dt;

   carried_dt += dt;
   while (carried_dt > 1.0/TICKRATE) {
      if (global_hack) {
         tex2_alpha += global_hack / 60.0f;
         if (tex2_alpha < 0) tex2_alpha = 0;
         if (tex2_alpha > 1) tex2_alpha = 1;
      }
      //update_input();
      // if the player is dead, stop the sim
      carried_dt -= 1.0/TICKRATE;
   }

   process_tick(dt);

   draw();

   return 0;
}

static int quit;

void active_control_set(int key)
{
   client_player_input.buttons |= 1 << key;
}

void active_control_clear(int key)
{
   client_player_input.buttons &= ~(1 << key);
}

extern void update_view(float dx, float dy);

void  process_sdl_mouse(SDL_Event *e)
{
   update_view((float) e->motion.xrel / screen_x, (float) e->motion.yrel / screen_y);
}

void process_event(SDL_Event *e)
{
   switch (e->type) {
      case SDL_MOUSEMOTION:
         process_sdl_mouse(e);
         break;
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
         break;

      case SDL_QUIT:
         quit = 1;
         break;

      case SDL_WINDOWEVENT:
         switch (e->window.event) {
            case SDL_WINDOWEVENT_SIZE_CHANGED:
               screen_x = e->window.data1;
               screen_y = e->window.data2;
               loopmode(0,1,0);
               break;
         }
         break;

      case SDL_KEYDOWN: {
         int k = e->key.keysym.sym;
         int s = e->key.keysym.scancode;
         SDL_Keymod mod;
         mod = SDL_GetModState();
         if (k == SDLK_ESCAPE)
            quit = 1;

         if (s == SDL_SCANCODE_D)   active_control_set(0);
         if (s == SDL_SCANCODE_A)   active_control_set(1);
         if (s == SDL_SCANCODE_W)   active_control_set(2);
         if (s == SDL_SCANCODE_S)   active_control_set(3);
         if (k == SDLK_SPACE)       active_control_set(4); 
         if (s == SDL_SCANCODE_LCTRL)   active_control_set(5);
         if (s == SDL_SCANCODE_S)   active_control_set(6);
         if (s == SDL_SCANCODE_D)   active_control_set(7);
         if (s == SDL_SCANCODE_F)   client_player_input.flying = !client_player_input.flying;
         if (k == '1') global_hack = !global_hack;
         if (k == '2') global_hack = -1;
         if (k == '3') obj[player_id].position.x += 65536;
         if (s == SDL_SCANCODE_R) {
            objspace_to_worldspace(light_vel, player_id, 0,32,0);
            memcpy(light_pos, &obj[player_id].position, sizeof(light_pos));
         }

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
         break;
      }
      case SDL_KEYUP: {
         int k = e->key.keysym.sym;
         int s = e->key.keysym.scancode;
         if (s == SDL_SCANCODE_D)   active_control_clear(0);
         if (s == SDL_SCANCODE_A)   active_control_clear(1);
         if (s == SDL_SCANCODE_W)   active_control_clear(2);
         if (s == SDL_SCANCODE_S)   active_control_clear(3);
         if (k == SDLK_SPACE)       active_control_clear(4); 
         if (s == SDL_SCANCODE_LCTRL)   active_control_clear(5);
         if (s == SDL_SCANCODE_S)   active_control_clear(6);
         if (s == SDL_SCANCODE_D)   active_control_clear(7);
         break;
      }
   }
}

static SDL_GLContext *context;

static float getTimestep(float minimum_time)
{
   float elapsedTime;
   double thisTime;
   static double lastTime = -1;
   
   if (lastTime == -1)
      lastTime = SDL_GetTicks() / 1000.0 - minimum_time;

   for(;;) {
      thisTime = SDL_GetTicks() / 1000.0;
      elapsedTime = (float) (thisTime - lastTime);
      if (elapsedTime >= minimum_time) {
         lastTime = thisTime;         
         return elapsedTime;
      }
      // @TODO: compute correct delay
      SDL_Delay(1);
   }
}

void APIENTRY gl_debug(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *param)
{
   if (!stb_prefix((char *) message, "Buffer detailed info:")) {
      id=id;
   }
   ods("%s\n", message);
}

int is_synchronous_debug;
void enable_synchronous(void)
{
   glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
   is_synchronous_debug = 1;
}

//extern void prepare_threads(void);

extern float compute_height_field(int x, int y);

Bool networking;

#define SERVER_PORT 4127

//void stbwingraph_main(void)
int SDL_main(int argc, char **argv)
{
   int server_port = SERVER_PORT;
   SDL_Init(SDL_INIT_VIDEO);

   //client_player_input.flying = True;

   if (argc > 1 && !strcmp(argv[1], "--server")) {
      program_mode = MODE_server;
   }
   if (argc > 1 && !strcmp(argv[1], "--client"))
      program_mode = MODE_client;

   if (argc > 2 && !strcmp(argv[1], "--port")) {
      server_port = atoi(argv[2]);
   }

   //prepare_threads();

   SDL_GL_SetAttribute(SDL_GL_RED_SIZE  , 8);
   SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
   SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE , 8);
   SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

   #ifdef GL_DEBUG
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
   #endif

   SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

   #if 0
   screen_x = 1920;
   screen_y = 1080;
   #else
   screen_x = 1280;
   screen_y = 720;
   #endif

   if (program_mode == MODE_server) {
      screen_x = 320;
      screen_y = 200;
   }

   if (1 || program_mode != MODE_server) {
      window = SDL_CreateWindow("obbg", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                      screen_x, screen_y,
                                      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
                                );
      if (!window) error("Couldn't create window");

      context = SDL_GL_CreateContext(window);
      if (!context) error("Couldn't create context");

      SDL_GL_MakeCurrent(window, context); // is this true by default?

      #if 1
      SDL_SetRelativeMouseMode(SDL_TRUE);
      #if defined(_MSC_VER) && _MSC_VER < 1300
      // work around broken behavior in VC6 debugging
      if (IsDebuggerPresent())
         SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");
      #endif
      #endif

      stbgl_initExtensions();

      #ifdef GL_DEBUG
      if (glDebugMessageCallbackARB) {
         glDebugMessageCallbackARB(gl_debug, NULL);

         enable_synchronous();
      }
      #endif

      SDL_GL_SetSwapInterval(1);
   }

   if (program_mode != MODE_single_player)
      networking = net_init(program_mode == MODE_server, server_port);

   game_init();
   render_init();

   //mesh_init();
   world_init();

   initialized = 1;

   while (!quit) {
      SDL_Event e;
      while (SDL_PollEvent(&e))
         process_event(&e);

      loopmode(getTimestep(0.0166f/8), 1, 1);
   }

   if (networking)
      SDLNet_Quit();

   return 0;
}
