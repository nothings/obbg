#include "obbg_funcs.h"

#define STB_GLEXT_DECLARE "glext_list.h"
#include "stb_gl.h"
#include "stb_image.h"
#include "stb_glprog.h"

#include "stb.h"
#include "SDL.h"
#include "SDL_thread.h"
#include <math.h>

#include "stb_voxel_render.h"

GLuint picker_prog;
GLuint instance_data_buf;
GLuint instance_data_tex;

void create_picker_buffers(void)
{
   glGenBuffersARB(1, &instance_data_buf);
   glGenTextures(1, &instance_data_tex);
}

static float bone_data[4] = { 0,0,0,0 };

typedef struct
{
   float pos[3];
   float norm[3];
   unsigned char boneweights[8];
} picker_vertex;

static void set_vertex(picker_vertex *pv, float nx, float ny, float nz, float px, float py, float pz, unsigned char boneweights[8])
{
   pv->pos [0] = px, pv->pos [1] = py, pv->pos [2] = pz;
   pv->norm[0] = nx, pv->norm[1] = ny, pv->norm[2] = nz;
   memcpy(pv->boneweights, boneweights, sizeof(pv->boneweights));
}

static int build_picker_box(picker_vertex *pv, float x, float y, float z, float sx, float sy, float sz, unsigned char boneweights1[8], unsigned char boneweights2[8])
{
   float x0,y0,z0,x1,y1,z1;
   sx /=2, sy/=2, sz/=2;
   x0 = x-sx; y0 = y-sy; z0 = z-sz;
   x1 = x+sx; y1 = y+sy; z1 = z+sz;

   set_vertex(pv++, 0,0,-1, x0,y0,z0, boneweights1);
   set_vertex(pv++, 0,0,-1, x1,y0,z0, boneweights1);
   set_vertex(pv++, 0,0,-1, x1,y1,z0, boneweights1);
   set_vertex(pv++, 0,0,-1, x0,y1,z0, boneweights1);

   set_vertex(pv++, 0,0,1, x1,y0,z1, boneweights2);
   set_vertex(pv++, 0,0,1, x0,y0,z1, boneweights2);
   set_vertex(pv++, 0,0,1, x0,y1,z1, boneweights2);
   set_vertex(pv++, 0,0,1, x1,y1,z1, boneweights2);

   set_vertex(pv++, -1,0,0, x0,y1,z1, boneweights2);
   set_vertex(pv++, -1,0,0, x0,y0,z1, boneweights2);
   set_vertex(pv++, -1,0,0, x0,y0,z0, boneweights1);
   set_vertex(pv++, -1,0,0, x0,y1,z0, boneweights1);

   set_vertex(pv++, 1,0,0, x1,y0,z1, boneweights2);
   set_vertex(pv++, 1,0,0, x1,y1,z1, boneweights2);
   set_vertex(pv++, 1,0,0, x1,y1,z0, boneweights1);
   set_vertex(pv++, 1,0,0, x1,y0,z0, boneweights1);

   set_vertex(pv++, 0,-1,0, x0,y0,z1, boneweights2);
   set_vertex(pv++, 0,-1,0, x1,y0,z1, boneweights2);
   set_vertex(pv++, 0,-1,0, x1,y0,z0, boneweights1);
   set_vertex(pv++, 0,-1,0, x0,y0,z0, boneweights1);

   set_vertex(pv++, 0,1,0, x1,y1,z1, boneweights2);
   set_vertex(pv++, 0,1,0, x0,y1,z1, boneweights2);
   set_vertex(pv++, 0,1,0, x0,y1,z0, boneweights1);
   set_vertex(pv++, 0,1,0, x1,y1,z0, boneweights1);

   return 24;
}

static GLuint picker_vbuf;

#pragma warning(disable:4305)
static int picker_vertices=0;
void build_picker(void)
{
   static picker_vertex picker_mesh_storage[1024];
   signed char boneweights[8] = { 0,0,0,0, 0,0,0,0 };
   signed char boneweights2[8] = { 0,0,0,0, 0,0,0,0 };

#if 1
   // bone 1: z coord of joint    (0,1,<2>)
   // bone 2: x coord of grabber  (<3>)
   // bone 3: y coord of grabber  (4,<5>,6)
   // bone 4: z coord of grabber  (<7>)
   picker_vertices += build_picker_box(picker_mesh_storage+picker_vertices, 0.35,0.35,0.35, 0.15,0.15,0.15, boneweights, boneweights);
   boneweights2[2] = 127;
   boneweights2[3] = 63;
   boneweights2[5] = 63;
   picker_vertices += build_picker_box(picker_mesh_storage+picker_vertices, 0.5-0.15-0.05,0.35,0.35, 0.05,0.05,0.05, boneweights, boneweights2);
   picker_vertices += build_picker_box(picker_mesh_storage+picker_vertices, 0.5-0.15,0.35,0.35, 0.15,0.15,0.15, boneweights2, boneweights2);
   boneweights[3] = 127;
   boneweights[5] = 127;
   boneweights[7] = 127;
   picker_vertices += build_picker_box(picker_mesh_storage+picker_vertices, 0.5-0.15,0.35,0.35, 0.05,0.05,0.05, boneweights, boneweights2);
   picker_vertices += build_picker_box(picker_mesh_storage+picker_vertices, 0.5-0.15,0.35,0.35, 0.25,0.25,0.05, boneweights, boneweights);

#else
   picker_vertices += build_picker_box(picker_mesh_storage+picker_vertices, 0,0,0.7, 1.5,0.125,0.125, boneweights, boneweights);
   boneweights[3] = 127;
   boneweights[7] = 127;

   picker_vertices += build_picker_box(picker_mesh_storage+picker_vertices, 0,0,0.575, 0.2,0.2,0.2, boneweights, boneweights);

   boneweights[0] = 127, boneweights[1] = 127;
   picker_vertices += build_picker_box(picker_mesh_storage+picker_vertices, 0,0,0.575-0.175, 0.03f,0.03f,0.15f, boneweights, boneweights);
   boneweights[0] = 127, boneweights[1] = -127;
   picker_vertices += build_picker_box(picker_mesh_storage+picker_vertices, 0,0,0.575-0.175, 0.03f,0.03f,0.15f, boneweights, boneweights);
   boneweights[0] = -127, boneweights[1] = -127;
   picker_vertices += build_picker_box(picker_mesh_storage+picker_vertices, 0,0,0.575-0.175, 0.03f,0.03f,0.15f, boneweights, boneweights);
   boneweights[0] = -127, boneweights[1] = 127;
   picker_vertices += build_picker_box(picker_mesh_storage+picker_vertices, 0,0,0.575-0.175, 0.03f,0.03f,0.15f, boneweights, boneweights);
#endif

   glGenBuffersARB(1, &picker_vbuf);
   glBindBufferARB(GL_ARRAY_BUFFER_ARB, picker_vbuf);
   glBufferDataARB(GL_ARRAY_BUFFER_ARB, picker_vertices * sizeof(picker_vertex), picker_mesh_storage, GL_STATIC_DRAW_ARB);
   glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

typedef struct
{
   float transform[4];
   float bone_weights[4];
} picker_data;

#define MAX_DRAW_PICKERS  20000
static picker_data pickers[MAX_DRAW_PICKERS];
static int num_drawn_pickers;

void upload_picker_buffer(picker_data *pd, int num_picker)
{
   static int first=1;

   glBindBufferARB(GL_TEXTURE_BUFFER_ARB, instance_data_buf);
   glBufferDataARB(GL_TEXTURE_BUFFER_ARB, num_picker*sizeof(*pd), pd, GL_STREAM_DRAW_ARB);
   glBindBufferARB(GL_TEXTURE_BUFFER_ARB, 0);

   if (first) {
      glBindTexture(GL_TEXTURE_BUFFER_ARB, instance_data_tex);
      glTexBufferARB(GL_TEXTURE_BUFFER_ARB, GL_RGBA32F, instance_data_buf);
      glBindTexture(GL_TEXTURE_BUFFER_ARB, 0);
      first=0;
   }
}

void add_draw_picker(float x, float y, float z, int rot, float states[4])
{
   if (num_drawn_pickers < MAX_DRAW_PICKERS) {
      picker_data *pd = &pickers[num_drawn_pickers++];
      pd->transform[0] = x;
      pd->transform[1] = y;
      pd->transform[2] = z;
      pd->transform[3] = (float) rot;
      memcpy(pd->bone_weights, states, sizeof(pd->bone_weights));
   }
}

void draw_picker(float alpha)
{
   int xform_loc  = stbgl_find_uniform(picker_prog, "xform_data");   
   int fogdata    = stbgl_find_uniform(picker_prog, "fogdata");
   int camera_pos = stbgl_find_uniform(picker_prog, "camera_pos");
   int recolor    = stbgl_find_uniform(picker_prog, "recolor");

   float fog_table[4];
   float recolor_value[4] = { 1.0,1.0,1.0,alpha };

   fog_table[0] = 0.6f, fog_table[1] = 0.7f, fog_table[2] = 0.9f;
   fog_table[3] = 1.0f / (view_distance - MESH_CHUNK_SIZE_X);
   fog_table[3] *= fog_table[3];

   glDisable(GL_LIGHTING);
   glDisable(GL_TEXTURE_2D);
   stbglUseProgram(picker_prog);

   upload_picker_buffer(pickers, num_drawn_pickers);
      
   stbglUniform1i(xform_loc, 4);
   glActiveTextureARB(GL_TEXTURE4_ARB);
   glBindTexture(GL_TEXTURE_BUFFER_ARB, instance_data_tex);
   glActiveTextureARB(GL_TEXTURE0_ARB);

   stbglUniform4fv(fogdata   , 1, fog_table);
   stbglUniform3fv(camera_pos, 1, camloc);
   stbglUniform4fv(recolor   , 1, recolor_value);

   glBindBufferARB(GL_ARRAY_BUFFER_ARB, picker_vbuf);
   stbglVertexAttribPointer(0, 3, GL_FLOAT, 0, sizeof(picker_vertex), (void*) 0);
   stbglVertexAttribPointer(1, 3, GL_FLOAT, 0, sizeof(picker_vertex), (void*) 12);
   stbglVertexAttribPointer(2, 4, GL_BYTE, GL_TRUE, sizeof(picker_vertex), (void*) 24);
   stbglVertexAttribPointer(3, 4, GL_BYTE, GL_TRUE, sizeof(picker_vertex), (void*) 28);

   stbglEnableVertexAttribArray(0);
   stbglEnableVertexAttribArray(1);
   stbglEnableVertexAttribArray(2);
   stbglEnableVertexAttribArray(3);

   glDrawArraysInstancedARB(GL_QUADS, 0, picker_vertices, num_drawn_pickers);
   num_drawn_pickers = 0;

   stbglDisableVertexAttribArray(0);
   stbglDisableVertexAttribArray(1);
   stbglDisableVertexAttribArray(2);
   stbglDisableVertexAttribArray(3);
   glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
   stbglUseProgram(0);
   glActiveTextureARB(GL_TEXTURE4_ARB);
   glBindTexture(GL_TEXTURE_BUFFER_ARB, 0);

   glActiveTextureARB(GL_TEXTURE0_ARB);
}

void init_object_render(void)
{
   char *vertex = stb_file("data/picker_vertex_shader.txt", NULL);
   char *fragment = stb_file("data/picker_fragment_shader.txt", NULL);
   char const *binds[] = { "position", "normal", "bone1", "bone2", NULL };
   char error_buffer[1024];
   char const *main_vertex[] = { vertex, NULL };
   char const *main_fragment[] = { fragment, NULL };
   int which_failed;
   picker_prog = stbgl_create_program(main_vertex, main_fragment, binds, error_buffer, sizeof(error_buffer), &which_failed);
   if (picker_prog == 0) {
      char *prog = which_failed == STBGL_FAILURE_STAGE_VERTEX ? vertex : fragment;
      if(prog)
         stb_filewrite("obbg_failed_shader.txt", prog, strlen(prog));
      ods("Compile error for picker shader: %s\n", error_buffer);
      assert(0);
      exit(1);
   }

   create_picker_buffers();
}
