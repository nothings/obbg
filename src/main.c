#define _WIN32_WINNT 0x400
//#define GL_DEBUG
//#define PROFILING

#include <assert.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define STB_DEFINE
#include "stb.h"

#include "obbg_funcs.h"

// stb_gl.h
#define STB_GL_IMPLEMENTATION
#define STB_GLEXT_DEFINE "glext_list.h"
#include "stb_gl.h"

// SDL
#include "SDL.h"
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

#define STB_VEC_IMPLEMENTATION
#include "stb_vec.h"


char *game_name = "obbg";

#define REVERSE_DEPTH


char *dumb_fragment_shader =
   "#version 150 compatibility\n"
   "uniform sampler2DArray tex;\n"
   "void main(){gl_FragColor = gl_Color*texture(tex,gl_TexCoord[0].xyz);}";


extern int load_crn_to_texture(unsigned char *data, size_t length);
extern int load_crn_to_texture_array(int slot, unsigned char *data, size_t length);
extern int load_bitmap_to_texture_array(int slot, unsigned char *data, int w, int h, int wrap, int premul);

GLuint debug_tex, dumb_prog;
GLuint voxel_tex[2];
GLuint sprite_tex;
int debug_render;


typedef struct
{
   float scale;
   char *filename;
} texture_info;

texture_info textures[] =
{
   1,"machinery/conveyor_90_00",
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

   1,"machinery/conveyor_90_01",
   1,"stone/Buffed_marble_pxr128",
   1,"stone/Black_marble_pxr128",
   1,"stone/Blue_marble_pxr128",
   1,"stone/Gray_granite_pxr128",
   1,"metal/Round_mesh_pxr128",
   1,"machinery/conveyor",
   1,"machinery/ore_maker",

   1,"machinery/ore_eater",
   1.0/8,"ground/Beach_sand_pxr128",
   1,"stone/Gray_marble_pxr128",
   0,0,
   0,0,
   0,0,
   0,0,
   0,0,

   1,"machinery/conveyor_90_02", 0,0,  0,0,  0,0, 0,0,  0,0,   0,0,   0,0,
   0,0,   0,0,   0,0, 0,0,   0,0,   0,0,   0,0,   0,0,

   1,"machinery/conveyor_90_03", 0,0,  0,0,  0,0, 0,0,  0,0,   0,0,   0,0,
   0,0,   0,0,   0,0, 0,0,   0,0,   0,0,   0,0,   0,0,

   1,"machinery/conveyor_270_00", 0,0,  0,0,  0,0, 0,0,  0,0,   0,0,   0,0,
   0,0,   0,0,   0,0, 0,0,   0,0,   0,0,   0,0,   0,0,

   1,"machinery/conveyor_270_01", 0,0,  0,0,  0,0, 0,0,  0,0,   0,0,   0,0,
   0,0,   0,0,   0,0, 0,0,   0,0,   0,0,   0,0,   0,0,

   1,"machinery/conveyor_270_02", 0,0,  0,0,  0,0, 0,0,  0,0,   0,0,   0,0,
   0,0,   0,0,   0,0, 0,0,   0,0,   0,0,   0,0,   0,0,

   1,"machinery/conveyor_270_03", 0,0,  0,0,  0,0, 0,0,  0,0,   0,0,   0,0,
   0,0,   0,0,   0,0, 0,0,   0,0,   0,0,   0,0,   0,0,
};

void game_init(void)
{
   init_chunk_caches();
   init_mesh_building();
   init_mesh_build_threads();
   s_init_physics_cache();
   logistics_init();
}

static uint8 blinn_8x8(uint8 x, uint8 y)
{
   uint32 t = x*y + 128;
   return (uint8) ((t + (t >>8)) >> 8);
}

typedef struct
{
   vec pos;
   float size;
   int id;
   vec color;
} sprite;

#define MAX_SPRITES 40000
sprite sprites[MAX_SPRITES];
int num_sprites;

#pragma warning(disable:4244)

#ifdef _MSC_VER
#pragma warning(disable:4305)
#endif
float it_color[20][3] =
{
   { 0,0,0, },
   { 0.1,0.1,0.1 }, // IT_coal
   { 0.6,0.5,0.5 }, // IT_iron_ore
   { 0.6,0.6,0.3 }, // IT_copper_ore
   { 0,0,0 },
   { 0,0,0 },
   { 0,0,0 },
   { 0,0,0 },
   { 1.0,1.0,1.0 }, // IT_iron_bar
   { 0.7,0.7,0.8 }, // IT_iron_gear
   { 0,0,0 },       // IT_steel_plate
   { 1.0,0.0,1.0 }, // IT_conveyor_belt
};

void add_sprite(float x, float y, float z, int id)
{
   sprite *s = &sprites[num_sprites++];
   s->pos.x = x;
   s->pos.y = y;
   s->pos.z = z;
   s->size = 0.25;
   assert(id >= 1 && id < sizeof(it_color)/12);
   s->id = id;
   //s->color.x = it_color[id][0];
   //s->color.y = it_color[id][1];
   //s->color.z = it_color[id][2];
}

void premultiply_alpha(uint8 *pixels, int w, int h)
{
   int i;
   for (i=0; i < w*h; ++i) {
      pixels[i*4+0] = blinn_8x8(pixels[i*4+0], pixels[i*4+3]);
      pixels[i*4+1] = blinn_8x8(pixels[i*4+1], pixels[i*4+3]);
      pixels[i*4+2] = blinn_8x8(pixels[i*4+2], pixels[i*4+3]);
   }
}

GLuint load_sprite(char *filename)
{
   int w,h;
   GLuint tex;
   uint8 *data;

   data = stbi_load(filename, &w, &h, 0, 4);
   assert(data != NULL);
   premultiply_alpha(data, w, h);

   glGenTextures(1, &tex);
   glBindTexture(GL_TEXTURE_2D, tex);

   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
   free(data);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

   glGenerateMipmapEXT(GL_TEXTURE_2D);
   return tex;
}

struct
{
   int itemtype;
   char *filename;
} sprite_filenames[] =
{
   { 0, "shadow" },
//   { IT_conveyor_belt,           "conveyor"           },
//   { IT_splitter,                "splitter"           },
//   { IT_furnace,                 "furnace"            },
//   { IT_iron_gear_maker,         "iron_gear_maker"    },
//   { IT_conveyor_belt_maker,     "conveyor_belt_maker"},
//   { IT_picker,                  "picker"             },
//   { IT_ore_drill,               "drill"              },
//   { IT_balancer,                "balancer"           },
   { IT_coal    ,                "coal"               },
   { IT_iron_ore,                "iron_ore"           },
   { IT_copper_ore,              "copper_ore"         },
   { IT_iron_bar,                "iron_bar"           },
   { IT_iron_gear,               "iron_gear"          },
//   { IT_steel_plate,             "steel_plate"        },
};


void render_init(void)
{
   // @TODO: support non-DXT path
   char **files = stb_readdir_recursive("data", "*.crn");
   int i;

   glGenTextures(2, voxel_tex);

   glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, voxel_tex[0]);
   glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

   for (i=0; i < 11; ++i) {
      glTexImage3DEXT(GL_TEXTURE_2D_ARRAY_EXT, i,
                         GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                         1024>>i,1024>>i,128,0,
                         GL_RGBA,GL_UNSIGNED_BYTE,NULL);
   }

   for (i=0; i < sizeof(textures)/sizeof(textures[0]); ++i) {
      if (textures[i].scale != 0) {
         size_t len;
         char *filename = stb_sprintf("data/pixar/crn/%s.crn", textures[i].filename);
         uint8 *data = stb_file(filename, &len);
         if (data == NULL)
            data = stb_file(stb_sprintf("data/%s.crn", textures[i].filename), &len);
         if (data == NULL) {
            int w,h;
            uint8 *pixels = stbi_load(stb_sprintf("data/%s.jpg", textures[i].filename), &w, &h, 0, 4);
            if (!pixels)
               pixels = stbi_load(stb_sprintf("data/%s.png", textures[i].filename), &w, &h, 0, 4);
            
            if (pixels) {
               load_bitmap_to_texture_array(i, pixels, w, h, 1, 0);
               free(pixels);
            } else
               assert(0);
         } else {
            load_crn_to_texture_array(i, data, len);
            free(data);
         }
      }
   }

   // temporary hack:
   voxel_tex[1] = voxel_tex[0];


   init_voxel_render(voxel_tex);
   init_object_render();

   {
      char const *frag[] = { dumb_fragment_shader, NULL };
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

   glGenTextures(1, &sprite_tex);
   glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, sprite_tex);
   glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

   for (i=0; i < 8; ++i) {
      glTexImage3DEXT(GL_TEXTURE_2D_ARRAY_EXT, i,
                         GL_RGBA,
                         128>>i,128>>i,256,0,
                         GL_RGBA,GL_UNSIGNED_BYTE,NULL);
   }

   for (i=0; i < sizeof(sprite_filenames)/sizeof(sprite_filenames[0]); ++i) {
      char *filename = stb_sprintf("data/sprites/%s.png", sprite_filenames[i].filename);
      int slot = sprite_filenames[i].itemtype;
      int w,h;
      uint8 *pixels = stbi_load(filename, &w, &h, 0, 4);
      if (pixels) {
         premultiply_alpha(pixels, w, h);
         load_bitmap_to_texture_array(slot, pixels, w, h, 0, 1);
         free(pixels);
      } else {
         assert(0);
      }
   }

   init_ui_render();

   #if 0
   for (i=0; i < 500; ++i) {
      sprite *s = &sprites[num_sprites++];
      s->pos.x = stb_frand() * 100 - 50;
      s->pos.y = stb_frand() * 100 - 50;
      s->pos.z = stb_frand() * 50 + 64;
      s->size = stb_rand() & 1 ? 0.5 : 0.125;
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
Bool player_is_vacuuming;

void process_tick(float dt)
{
   dt += pending_dt;
   pending_dt = 0;
   while (dt > 1.0f/240) {
      switch (program_mode) {
         case MODE_server:
            server_net_tick_pre_physics();
            process_tick_raw(1.0f/240);
            server_net_tick_post_physics();
            break;
         case MODE_client:
            client_view_physics(player_id, &client_player_input, dt);
            client_net_tick();
            break;
         case MODE_single_player:
            player_vacuum(player_is_vacuuming, &obj[player_id].position);
            client_view_physics(player_id, &client_player_input, dt);
            p_input[player_id] = client_player_input;
            process_tick_raw(1.0f/240);
            break;
      }

      dt -= 1.0f/240;
   }
   pending_dt += dt;
}

void update_view(float dx, float dy)
{
   // hard-coded mouse sensitivity, not resolution independent?
   pending_view_z -= dx*300;
   pending_view_x -= dy*700;
}

float render_time, tick_time;

int screen_x, screen_y;
int is_synchronous_debug;
int sort_order_for_selected_belt;

size_t mesh_cache_requested_in_use;
int chunk_locations, chunks_considered, chunks_in_frustum;
int quads_considered, quads_rendered;
int chunk_storage_rendered, chunk_storage_considered, chunk_storage_total;
int view_dist_for_display;
int num_threads_active, num_meshes_started, num_meshes_uploaded;
float chunk_server_activity;

float chunk_server_status[32];
int chunk_server_pos;

extern vec3i physics_cache_feedback[64][64];
extern int num_gen_chunk_alloc;


stb_sdict *memstats_table;
typedef struct
{
   char *info;
   size_t total;
   size_t largest;
   size_t count;
} memstats;

void dump_callback(size_t size, char *info)
{
   memstats *ms = stb_sdict_get(memstats_table, info);
   if (ms == NULL) {
      ms = malloc(sizeof(*ms));
      ms->count = 1;
      ms->total = size;
      ms->largest = size;
      ms->info = info;
      stb_sdict_add(memstats_table, info, ms);
   } else {
      ++ms->count;
      ms->total += size;
      ms->largest = stb_max(ms->largest, size);
   }
}

void print_column(char *str, int id)
{
   static float xtab[4] = { 30,90,150,180 };
   float x = xtab[id];
   if (id >= 0 && id <= 2) {
      x -= stb_easy_font_width(str);
   }
   print_string(x, pos_y, str, text_color[0], text_color[1], text_color[2]);
}

void dump_memory(void)
{
   int i;
   char *info;
   memstats *arr=NULL;
   memstats *ms;
   memstats_table = stb_sdict_new(1);
   obbg_malloc_dump(dump_callback);

   stb_sdict_for(memstats_table, i, info, ms) {
      stb_arr_push(arr, *ms);
   }

   qsort(arr, stb_arr_len(arr), sizeof(*arr), stb_intcmp(offsetof(memstats,total)));

   for (i=stb_arr_len(arr)-1; i >= 0; --i) {
      ms = &arr[i];

      print_column(stb_sprintf("%9d", ms->count), 0);
      print_column(stb_sprintf("%9d", ms->total), 1);
      print_column(stb_sprintf("%9d", ms->largest), 2);
      print_column(ms->info, 3);
      print("");
   }
   stb_sdict_delete(memstats_table);
}

struct
{
   float count[32];
   double time[32];
} thread_timing[MAX_MESH_WORKERS];

void update_thread_times(void)
{
   int i,j;
   for (i=0; i < MAX_MESH_WORKERS; ++i) {
      int count[32];
      double time[32];
      query_thread_info(i, count, time);
      for (j=0; j<32; ++j) {
         thread_timing[i].count[j] = count[j];
         thread_timing[i].time[j] = time[j];
      }
   }
}

Bool show_memory;
void draw_stats(void)
{
   static double last_update_time;
   static Bool initialize_update = True;

   int i;

   static Uint64 last_frame_time;
   Uint64 cur_time = SDL_GetPerformanceCounter();
   float chunk_server=0;
   float frame_time = (cur_time - last_frame_time) / (float) SDL_GetPerformanceFrequency();
   last_frame_time = cur_time;

   if (initialize_update) {
      last_update_time = cur_time / (double) SDL_GetPerformanceFrequency();
      initialize_update = False;
   }

   if (cur_time / (double) SDL_GetPerformanceFrequency() >= last_update_time + 1.0f) {
      last_update_time = cur_time / (double) SDL_GetPerformanceFrequency();
      update_thread_times();
   }

   chunk_server_status[chunk_server_pos] = chunk_server_activity;
   chunk_server_pos = (chunk_server_pos+1) %32;

   for (i=0; i < 32; ++i)
      chunk_server += chunk_server_status[i] / 32.0f;

   stb_easy_font_spacing(-0.75);
   pos_y = 10;
   text_color[0] = text_color[1] = text_color[2] = 1.0f;
   print("Frame time: %6.2fms, CPU frame render time: %5.2fms , CPU tick time: %5.2fms", frame_time*1000, render_time*1000, tick_time*1000);
   print("Tris: %4.1fM drawn of %4.1fM in range", 2*quads_rendered/1000000.0f, 2*quads_considered/1000000.0f);
   print("Mesh data: requested in-cache %dMB, total in cache %dMB", mesh_cache_requested_in_use >> 20, c_mesh_cache_in_use >> 20);
   if (debug_render) {
      print("Gen chunks: %4d", num_gen_chunk_alloc);
      for (i=0; i < MAX_MESH_WORKERS; ++i) {
         static char *task[32] = { "mesh", "gencache", "heightf", "fill", "ore", "trees", "edits", "light", "physics", "scan", "idle" };
         char buffer[1024];
         int pos=0,j;
         float total=0;
         pos += sprintf(buffer+pos, "Thread %d - ", i);
         for (j=0; j < 32; ++j)
            total += thread_timing[i].time[j];
         pos += sprintf(buffer+pos, "%4dms - ", (int) (1000*total));
         for (j=0; j < 11; ++j) {
            if (0==strcmp(task[j], "ore")) continue;
            if (0==strcmp(task[j], "edits")) continue;
            if (0==strcmp(task[j], "physics")) continue;
            if (0==strcmp(task[j], "scan")) continue;
            pos += sprintf(buffer+pos, "%s:%3g/%3dms ", task[j], thread_timing[i].count[j], (int) (1000*thread_timing[i].time[j]));
         }
         print(buffer);
      }
      print("Vbuf storage: %dMB in frustum of %dMB in range of %dMB in cache", chunk_storage_rendered>>20, chunk_storage_considered>>20, chunk_storage_total>>20);
      print("Num mesh builds started this frame: %d; num uploaded this frame: %d\n", num_meshes_started, num_meshes_uploaded);
      print("QChunks: %3d in frustum of %3d valid of %3d in range", chunks_in_frustum, chunks_considered, chunk_locations);
      print("Mesh worker threads active: %d", num_threads_active);
      print("View distance: %d blocks", view_dist_for_display);
      print("x=%5.2f, y=%5.2f, z=%5.2f", obj[player_id].position.x, obj[player_id].position.y, obj[player_id].position.z);
      print("Belt sort order: %d", sort_order_for_selected_belt);
      print("%s", glGetString(GL_RENDERER));
      #if 0
      for (i=0; i < 4; ++i)
         print("[ %4d,%4d  %4d,%4d  %4d,%4d  %4d,%4d ]",
                                      physics_cache_feedback[i][0].x, physics_cache_feedback[i][0].y, 
                                      physics_cache_feedback[i][1].x, physics_cache_feedback[i][1].y, 
                                      physics_cache_feedback[i][2].x, physics_cache_feedback[i][2].y, 
                                      physics_cache_feedback[i][3].x, physics_cache_feedback[i][3].y);
      #endif
   }

   if (is_synchronous_debug) {
      text_color[0] = 1.0;
      text_color[1] = 0.5;
      text_color[2] = 0.5;
      print("SLOWNESS: Synchronous debug output is enabled!");
   }

   if (show_memory) {
      dump_memory();
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

Bool third_person=True;
float player_zoom = 1.0f;

int alpha_test_sprites=1;

void render_sprites(void)
{
   int i;
   vec s_off, t_off, shadow_s_off = { 0.5,0,0}, shadow_t_off = { 0,0.5,0 };
   stbglUseProgram(dumb_prog);
   
   glDisable(GL_ALPHA_TEST);
   glDepthMask(GL_FALSE);
   glEnable(GL_BLEND);
   glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
   glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, sprite_tex);
   stbglUniform1i(stbgl_find_uniform(dumb_prog, "tex"), 0);

   objspace_to_worldspace(&s_off.x, player_id, 0.5,0,0, 0);
   objspace_to_worldspace(&t_off.x, player_id, 0,0,0.5, 0);

   glColor3f(1,1,1);


   glPolygonOffset(1,1);
   glEnable(GL_POLYGON_OFFSET_FILL);
   glBegin(GL_QUADS);
   for (i=0; i < num_sprites; ++i) {
      sprite *s = &sprites[i];
      vec p0,p1,p2,p3;

      // draw shadow
      vec_add_scale(&p0, &s->pos, &shadow_s_off, s->size);
      vec_sub_scale(&p1, &s->pos, &shadow_s_off, s->size);
      vec_add_scale(&p2, &p1, &shadow_t_off, s->size);
      vec_add_scale(&p3, &p0, &shadow_t_off, s->size);
      vec_sub_scale(&p0, &p0, &shadow_t_off, s->size);
      vec_sub_scale(&p0, &p1, &shadow_t_off, s->size);
      glTexCoord3f(0,1,0); glVertex3fv(&p0.x);
      glTexCoord3f(1,1,0); glVertex3fv(&p1.x);
      glTexCoord3f(1,0,0); glVertex3fv(&p2.x);
      glTexCoord3f(0,0,0); glVertex3fv(&p3.x);
   }
   glEnd();
   glDisable(GL_POLYGON_OFFSET_FILL);
   glPolygonOffset(0,0);

   if (alpha_test_sprites) {
      glEnable(GL_ALPHA_TEST);
      glAlphaFunc(GL_GREATER, 0.5);
      glDisable(GL_BLEND);
      glDepthMask(GL_TRUE);
   } else {
      glEnable(GL_BLEND);
      glDepthMask(GL_FALSE);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
   }
   glBegin(GL_QUADS);
   for (i=0; i < num_sprites; ++i) {
      sprite *s = &sprites[i];
      vec p0,p1,p2,p3;
      s->pos.z += s->size/2;

      vec_add_scale(&p0, &s->pos, &s_off, s->size);
      vec_sub_scale(&p1, &s->pos, &s_off, s->size);
      vec_add_scale(&p2, &p1, &t_off, s->size);
      vec_add_scale(&p3, &p0, &t_off, s->size);
      vec_subeq_scale(&p0, &t_off, s->size);
      vec_subeq_scale(&p1, &t_off, s->size);
      //glColor3fv(&s->color.x);
      glTexCoord3f(0,1,s->id); glVertex3fv(&p0.x);
      glTexCoord3f(1,1,s->id); glVertex3fv(&p1.x);
      glTexCoord3f(1,0,s->id); glVertex3fv(&p2.x);
      glTexCoord3f(0,0,s->id); glVertex3fv(&p3.x);
   }
   glEnd();
   glDepthMask(GL_TRUE);
   glDisable(GL_ALPHA_TEST);


   stbglUseProgram(0);
}

float smoothed_z_for_rendering(vec *pos, interpolate_z *iz)
{
   float t = iz->t * iz->t;
   return stb_lerp(t, pos->z, iz->old_z);
}

float animation_dt;
float animation_state;
vec left_foot;
vec right_foot;
int left_foot_planted=0;
int right_foot_planted=0;
int left_foot_good, right_foot_good;

vec find_foot_placement(vec poly[5])
{
   float best_dist=99999999.0f, dist;
   vec best_place = poly[0];
   int dz;
   float s,t;
   if (can_place_foot(poly[0], 0.15,0.15))
      return poly[0];
   for (dz=-1; dz <= 1; ++dz) {
      for (s=0; s <= 1; s += 0.25f) {
         for (t=0; t <= 1; t += 0.25f) {
            vec place0, place1, place;
            vec_lerp(&place0, &poly[1], &poly[2], t);
            vec_lerp(&place1, &poly[3], &poly[4], t);
            vec_lerp(&place, &place0, &place1, s);
            place.z += dz;
            if (can_place_foot(place, 0.15f,0.15f)) {
               dist = vec_dist(&place, &poly[0]);
               if (dist < best_dist) {
                  best_dist = dist;
                  best_place = place;
               }
            }
         }
      }
   }
   return best_place;
}

#if 0
#error "TODO"

0. Move position of objects to be on ground/feet

1. IK legs to feet

2. when foot is halfway from previous position to next position,
   compute placement for next position

3. animate foot from old position to halfway cleanly

4. animate foot from halfway to next position cleanly

5. deal with stopping if foot placement was computed before stopping

6. Treat foot as point for sampling

7. Better foot placement search w/o searching

8. Standing still - walking - running states w/ different gaits
8b. Adjustable torso position a la Arma 3

9. Lower & tilt torso when one foot is much lower

10. Head above torso, increase player size to match
#endif

void render_player(vec pos, vec sz, vec ang, float bottom_z, objid player)
{
   //float bottom_z = pos.z - sz.z/2;
      
   float light_diffuse [] = { 1.0f, 1.0f, 1.0f, 1.0f };
   float light_ambient [] = { 0.9f, 0.9f, 0.9f, 1.0f };
   float light_position[] = { 1.0f, 1.0f, 2.0f, 0.0f };
   float mat_red[]  = { 1.0f,0.2f,0.2f,1.0 };
   float mat_specular[] = { 0,0,0,0 };
   float mat_diffuse[]  = { 1.0f,0.9f,0.8f,1.0 };
   float mat_green[] = { 0.4f,0.8f,0.4f,1.0 };
   glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
   glMaterialfv(GL_FRONT, GL_DIFFUSE , mat_diffuse );
   glLightfv(GL_LIGHT0, GL_DIFFUSE , light_diffuse );
   glLightfv(GL_LIGHT0, GL_AMBIENT , light_ambient );
   glLightfv(GL_LIGHT0, GL_POSITION, light_position);
   glDisable(GL_BLEND);

   pos.z += 0.7f;
   sz.z -= 1.4f;

   glEnable(GL_LIGHTING);
   glEnable(GL_LIGHT0);
   glMatrixMode(GL_MODELVIEW);
   glPushMatrix();
   glTranslatef(pos.x,pos.y,pos.z);
   glRotatef(ang.z, 0,0,1);
   glRotatef(ang.x, 1,0,0);
   glRotatef(ang.y, 0,1,0);
   stbgl_drawBox(0,0,0, sz.x,sz.y,sz.z, 1);
   glPopMatrix();

   {
      int i;
      float mag;
      float s,c;
      float y_left,y_right, z_left,z_right;
      vec move_vel = obj[player_id].velocity;
      move_vel.z = 0;

      mag = vec_mag(&move_vel);
      animation_state += animation_dt*20;// * mag * 1.5;
      animation_state = fmod(animation_state, 2*M_PI);

      s = -sin(animation_state);
      c =  cos(animation_state);

      y_right =  c * mag / 20;
      z_right =  s * 0.4f;
      if (z_right < 0) z_right = 0;
      z_right += bottom_z + 0.05;

      y_left =  -c * mag / 20;
      z_left =  -s * 0.4f;
      if (z_left < 0) z_left = 0;
      z_left += bottom_z + 0.05;

      if (animation_state >= 0 && animation_state <= M_PI) {
         if (!right_foot_planted) {
            vec poly[5];

            objspace_to_worldspace_flat(&right_foot.x, player, 0.35f, y_right);
            if (fabs(z_right - floor(z_right)) > 0.1f)
               z_right = floor(z_right) + 0.05;

            objspace_to_worldspace_flat(&poly[0].x, player, 0.35f, 0.0);
            objspace_to_worldspace_flat(&poly[1].x, player, 0.6  ,-0.4);
            objspace_to_worldspace_flat(&poly[2].x, player, 0.6  , 0.4);
            objspace_to_worldspace_flat(&poly[3].x, player, 0.22 ,-0.4);
            objspace_to_worldspace_flat(&poly[4].x, player, 0.22 , 0.4);
            for (i=0; i < 5; ++i) {
               poly[i].x += pos.x;
               poly[i].y += pos.y;
               poly[i].z = z_right;
               vec_addeq_scale(&poly[i], &move_vel, 0.1f); // @TODO tune 5.0f
            }
            right_foot = find_foot_placement(poly);
            right_foot_planted = True;
            right_foot_good = can_place_foot(right_foot, 0.15f,0.15f);
         }

         objspace_to_worldspace_flat(&left_foot.x, player, -0.35f, y_left);
         left_foot.x += pos.x;
         left_foot.y += pos.y;
         left_foot.z = z_left;
         left_foot_planted = False;
         left_foot_good = True;
      } else {
         if (!left_foot_planted) {
            vec poly[5];

            objspace_to_worldspace_flat(&left_foot.x, player, -0.35f, y_left);
            if (fabs(z_left - floor(z_left)) > 0.1f)
               z_left = floor(z_left) + 0.05;

            objspace_to_worldspace_flat(&poly[0].x, player, - 0.35f, 0.0);
            objspace_to_worldspace_flat(&poly[1].x, player, - 0.6  ,-0.4);
            objspace_to_worldspace_flat(&poly[2].x, player, - 0.6  , 0.4);
            objspace_to_worldspace_flat(&poly[3].x, player, - 0.22 ,-0.4);
            objspace_to_worldspace_flat(&poly[4].x, player, - 0.22 , 0.4);
            for (i=0; i < 5; ++i) {
               poly[i].x += pos.x;
               poly[i].y += pos.y;
               poly[i].z += z_left;
               vec_addeq_scale(&poly[i], &move_vel, 0.1f); // @TODO tune 5.0f
            }
            left_foot = find_foot_placement(poly);
            left_foot_planted = True;
            left_foot_good = can_place_foot(left_foot, 0.15f,0.15f);
         }
         objspace_to_worldspace_flat(&right_foot.x, player, 0.35f, y_right);
         right_foot.x += pos.x;
         right_foot.y += pos.y;
         right_foot.z = z_right;
         right_foot_planted = False;
         right_foot_good = True;
      }

      glMaterialfv(GL_FRONT, GL_DIFFUSE , right_foot_good ? mat_diffuse : mat_red);
      glPushMatrix();
      glTranslatef(right_foot.x, right_foot.y, right_foot.z);
      glRotatef(ang.z, 0,0,1);
      stbgl_drawBox(0,0,0, 0.2f,0.4f,0.1f, 1);
      glPopMatrix();

      glMaterialfv(GL_FRONT, GL_DIFFUSE , left_foot_good ? mat_green : mat_red);
      glPushMatrix();
      glTranslatef(left_foot.x, left_foot.y, left_foot.z);
      glRotatef(ang.z, 0,0,1);
      stbgl_drawBox(0,0,0, 0.2f,0.4f,0.1f, 1);
      glPopMatrix();
   }

   glDisable(GL_LIGHTING);
}

void render_objects(void)
{
   int i;
   vec sz;
   vec pos;
   glColor3f(1,1,1);
   glDisable(GL_TEXTURE_2D);
   glDisable(GL_BLEND);

   num_sprites = 0;

   for (i=1; i < max_player_id; ++i) {
      if (obj[i].valid && (i != player_id || third_person)) {
         vec ang;
         vec face;
         vec move;
         float forward;
         float (*size)[3] = size_for_type[OTYPE_player];
         sz.x = size[1][0] - size[0][0];
         sz.y = size[1][1] - size[0][1];
         sz.z = size[1][2] - size[0][2];
         pos.x = obj[i].position.x + (size[1][0] + size[0][0])/2;
         pos.y = obj[i].position.y + (size[1][1] + size[0][1])/2;
         pos.z = smoothed_z_for_rendering(&obj[i].position, &obj[i].iz) + (size[1][2] + size[0][2])/2;
         ang = obj[i].ang;
         ang.x = 0;
         face.x = sin(ang.z * M_PI / 180);
         face.y = -cos(ang.z * M_PI / 180);
         move = obj[i].velocity;
         move.z = 0;
         forward = vec_dot(&move, &face);
         if (forward < 0)
            ang.x = forward * 1.2;
         else
            ang.x = forward * 0.7f;
            
         render_player(pos, sz, ang, obj[i].position.z+size[0][2], i);
      }
   }

   for (i=PLAYER_OBJECT_MAX; i < max_obj_id; ++i) {
      object *o = &obj[i];
      if (o->valid) {
         vec center;
         float (*sz)[3] = size_for_type[o->type];
         if (o->on_ground)
            glColor3f(0.85f,0.95f,1.0f);
         else
            glColor3f(1.0f,1.0f,0.9f);
         if (o->type == OTYPE_critter)
            glColor3f(1.0f,1.0f,0.6f);
         center.x = o->position.x + (sz[1][0]+sz[0][0])/2;
         center.y = o->position.y + (sz[1][1]+sz[0][1])/2;
         center.z = o->position.z + (sz[1][2]+sz[0][2])/2;
         stbgl_drawBox(center.x, center.y, center.z, sz[1][0]-sz[0][0], sz[1][1]-sz[0][1], sz[1][2]-sz[0][2], 1);
      }
   }

   logistics_render();

   draw_instanced_flush(1.0f);

   render_sprites();
}

int face_dir[6][3] = {
   { 1,0,0 },
   { 0,1,0 },
   { -1,0,0 },
   { 0,-1,0 },
   { 0,0,1 },
   { 0,0,-1 },
};

float global_timer;
float third_person_angle=0.0f;

void draw_main(void)
{
   Uint64 start_time, end_time; // render time
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

   camang[0] = obj[player_id].ang.x;
   camang[1] = obj[player_id].ang.y;
   camang[2] = obj[player_id].ang.z;
   if (third_person) {
      objspace_to_worldspace(camloc, player_id, 0,-7,0, third_person_angle);
      camloc[0] += obj[player_id].position.x;
      camloc[1] += obj[player_id].position.y;
      camloc[2] += smoothed_z_for_rendering(&obj[player_id].position, &obj[player_id].iz);
      camang[2] += third_person_angle;
   } else {
      camloc[0] = obj[player_id].position.x;
      camloc[1] = obj[player_id].position.y;
      camloc[2] = smoothed_z_for_rendering(&obj[player_id].position, &obj[player_id].iz);
   }

#if 1
   glRotatef(-camang[0],1,0,0);
   glRotatef(-camang[2],0,0,1);
   glTranslatef(-camloc[0], -camloc[1], -camloc[2]);
#endif

   start_time = SDL_GetPerformanceCounter();
   render_voxel_world(camloc);

   player_zoom = 1;

   {
      int i,j,k;
      float bone_values[8] = { 0 };
      bone_values[5] = global_timer*4;
      bone_values[4] = 1;
      add_draw_machine(-15,1,78, 0, bone_values);
      for (k=0; k < 1; ++k) {
      for (j=0; j < 1; ++j) {
      for (i=0; i < 1; ++i) {
         add_draw_machine(-15+j,1+i,78+k, 0, bone_values);
         bone_values[5] += 0.5;
      }
      }
      bone_values[5] = global_timer*4;
      }
   }
   render_objects();

   end_time = SDL_GetPerformanceCounter();

   glDisable(GL_LIGHTING);

   do_ui_rendering_3d();

   #if 0
   if (debug_render)
      logistics_debug_render();
   #endif

   render_time = (end_time - start_time) / (float) SDL_GetPerformanceFrequency();

   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   gluOrtho2D(0,screen_x,screen_y,0);

   do_ui_rendering_2d();

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   if (debug_tex) {
      stbglUseProgram(dumb_prog);
      glDisable(GL_TEXTURE_2D);
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, debug_tex);
      stbglUniform1i(stbgl_find_uniform(dumb_prog, "tex"), 0);
      glColor3f(1,1,1);
      stbgl_drawRectTCArray(0,0,512,512,0,0,1,1, 0.0);
      stbglUseProgram(0);
   }
   glDisable(GL_TEXTURE_2D);
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   gluOrtho2D(0,screen_x/2,screen_y/2,0);
   draw_stats();

}



#pragma warning(disable:4244; disable:4305; disable:4018)

#define SCALE   2

void error(char *s)
{
   SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", s, NULL);
   exit(0);
}

SDL_mutex *logm;

void ods(char *fmt, ...)
{
   char buffer[1000];
   va_list va;

   if (logm == NULL)
      logm = SDL_CreateMutex();
   va_start(va, fmt);
   vsprintf(buffer, fmt, va);
   va_end(va);

   #ifdef _WIN32
   OutputDebugString(buffer);
   #else
   SDL_LockMutex(logm);
   SDL_Log("%s", buffer);
   SDL_UnlockMutex(logm);
   #endif
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

int global_hack;
int quit;
float slow_motion = 1.0f;

int loopmode(float dt, int real, int in_client)
{
   Uint64 start_time, end_time;
   if (!initialized) return 0;

   if (!real)
      return 0;

   // don't allow more than 6 frames to update at a time
   if (dt > 0.075) dt = 0.075;

   dt *= slow_motion;

   global_timer += dt;

   animation_dt = dt;

   carried_dt += dt;
   while (carried_dt > 1.0/TICKRATE) {
      #if 0
      if (global_hack) {
         tex2_alpha += global_hack / 60.0f;
         if (tex2_alpha < 0) tex2_alpha = 0;
         if (tex2_alpha > 1) tex2_alpha = 1;
      }
      #endif
      //update_input();
      // if the player is dead, stop the sim
      carried_dt -= 1.0/TICKRATE;
   }

   start_time = SDL_GetPerformanceCounter();
   process_tick(dt);
   end_time = SDL_GetPerformanceCounter();
   tick_time = (end_time - start_time) / (float) SDL_GetPerformanceFrequency();

   draw();

   return 0;
}

extern void process_key_down(int k, int s, SDL_Keymod mod);
void process_event(SDL_Event *e)
{
   switch (e->type) {
      case SDL_MOUSEMOTION:
         process_mouse_move(e->motion.xrel, e->motion.yrel);
         break;
      case SDL_MOUSEBUTTONDOWN:
         mouse_down(e->button.button == SDL_BUTTON_LEFT ? -1 : e->button.button == SDL_BUTTON_RIGHT ? 1 : 0);
         break;
      case SDL_MOUSEBUTTONUP:
         mouse_up();
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
         process_key_down(k, s, SDL_GetModState());
         break;
      }
      case SDL_KEYUP: {
         int k = e->key.keysym.sym;
         int s = e->key.keysym.scancode;
         process_key_up(k,s);
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

static cur_mouse_relative = True;
void mouse_relative(Bool relative)
{
   #if 0
   if (relative != cur_mouse_relative) {
      SDL_SetRelativeMouseMode(relative ? SDL_TRUE : SDL_FALSE);
      cur_mouse_relative = relative;
   }
   #endif
}

//extern void prepare_threads(void);

extern float compute_height_field(int x, int y);

Bool networking;

#ifndef SDL_main
#define SDL_main main
#endif

#define SERVER_PORT 4127

void *memory_mutex, *prof_mutex;

//void stbwingraph_main(void)
int SDL_main(int argc, char **argv)
{
   int server_port = SERVER_PORT;
   SDL_Init(SDL_INIT_VIDEO);
   #ifdef _NDEBUG
   SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
   #else
   SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
   #endif

   memory_mutex = SDL_CreateMutex();
   prof_mutex = SDL_CreateMutex();
   if (memory_mutex == NULL || prof_mutex == NULL) error("Couldn't create mutex");


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

   SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1); // @TODO doesn't seem to be necessary
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

   #ifdef PROFILING
   SDL_GL_SetSwapInterval(0);   // disable vsync
   #endif
   game_init();
   render_init();

   //mesh_init();
   world_init();
   load_edits();

   initialized = 1;

   while (!quit) {
      SDL_Event e;
      while (SDL_PollEvent(&e))
         process_event(&e);

      loopmode(getTimestep(0.0166f/8), 1, 1);
   }

   if (networking)
      SDLNet_Quit();

   #ifdef _DEBUG
   //stop_manager();
   #endif

   return 0;
}
