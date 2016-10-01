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

GLuint picker_prog, machine_prog;
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
   unsigned char boneweights[12];
} machine_vertex;

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
static int picker_vertices=0;

#pragma warning(disable:4305)
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

static GLuint machine_vbuf;
static machine_vertex machine_mesh_storage[8192];
static uint16 machine_indices[4096*4];
static int num_machine_indices;
static int machine_vertices;

static signed char machine_boneweights[12];
static Bool machine_faceted;

static void mc_faceted_lighting(Bool flag)
{
   machine_faceted = flag;
}

static void mc_rotate_x(float x) // x = -1..1
{
   machine_boneweights[3] = 127;
   machine_boneweights[7] = (int8) (-x * 127);
}
static void mc_rotate_z(float z)
{
   machine_boneweights[3] = 0;
   machine_boneweights[7] = (int8) (z * 127);
}
static void mc_no_rotate(void)
{
   machine_boneweights[3] = 0;
   machine_boneweights[7] = 0;
}

static int8 coded(float v)
{
   return (int8) (stb_linear_remap(v, -1, 1, -127, 127)+0.5);
}

static void mc_rot_center(float x, float y, float z)
{
   machine_boneweights[4] = coded(x);
   machine_boneweights[5] = coded(y);
   machine_boneweights[6] = coded(z);
}

#if 0
   pos.xyz += bone3.xyz * bone_value2.z;
   pos.z   += bone3.w   * bone_value2.w;

   pos.xyz += bone2.xyz * bone_value2.x;
   pos.xyz += bone3.xyz * bone_value2.z;
   pos.z   += bone3.w   * bone_value2.w;
#endif

static void mvertex(float nx, float ny, float nz, float px, float py, float pz)
{
   machine_vertex *mv = &machine_mesh_storage[machine_vertices++];
   mv->pos [0] = px, mv->pos [1] = py, mv->pos [2] = pz;
   if (machine_faceted)
      mv->norm[0] = mv->norm[1] = mv->norm[2] = 0;
   else
      mv->norm[0] = nx, mv->norm[1] = ny, mv->norm[2] = nz;
   memcpy(mv->boneweights, machine_boneweights, sizeof(mv->boneweights));
}

static void mtri(int i, int j, int k)
{
   machine_indices[num_machine_indices++] = i;
   machine_indices[num_machine_indices++] = j;
   machine_indices[num_machine_indices++] = k;
}

static void mquad(int i, int j, int k, int l)
{
   mtri(i,j,k);
   mtri(i,k,l);
}

static void machine_box_fast(float x, float y, float z, float sx, float sy, float sz)
{
   int idx;
   float x0,y0,z0,x1,y1,z1;
   sx /=2, sy/=2, sz/=2;
   x0 = x-sx; y0 = y-sy; z0 = z-sz;
   x1 = x+sx; y1 = y+sy; z1 = z+sz;

   idx = machine_vertices;

   mvertex(0,0,0, x0,y0,z0);
   mvertex(0,0,0, x1,y0,z0);
   mvertex(0,0,0, x0,y1,z0);
   mvertex(0,0,0, x1,y1,z0);
   mvertex(0,0,0, x0,y0,z1);
   mvertex(0,0,0, x1,y0,z1);
   mvertex(0,0,0, x0,y1,z1);
   mvertex(0,0,0, x1,y1,z1);

   mquad(idx+0,idx+1,idx+3,idx+2);
   mquad(idx+5,idx+4,idx+6,idx+7);
   mquad(idx+6,idx+4,idx+0,idx+2);
   mquad(idx+5,idx+7,idx+3,idx+1);
   mquad(idx+4,idx+5,idx+1,idx+0);
   mquad(idx+7,idx+6,idx+2,idx+3);
}

static void machine_box(float x, float y, float z, float sx, float sy, float sz)
{
   int idx, i;
   float x0,y0,z0,x1,y1,z1;
   sx /=2, sy/=2, sz/=2;
   x0 = x-sx; y0 = y-sy; z0 = z-sz;
   x1 = x+sx; y1 = y+sy; z1 = z+sz;

   idx = machine_vertices;

   mvertex(0,0,-1, x0,y0,z0);
   mvertex(0,0,-1, x1,y0,z0);
   mvertex(0,0,-1, x1,y1,z0);
   mvertex(0,0,-1, x0,y1,z0);

   mvertex(0,0,1, x1,y0,z1);
   mvertex(0,0,1, x0,y0,z1);
   mvertex(0,0,1, x0,y1,z1);
   mvertex(0,0,1, x1,y1,z1);

   mvertex(-1,0,0, x0,y1,z1);
   mvertex(-1,0,0, x0,y0,z1);
   mvertex(-1,0,0, x0,y0,z0);
   mvertex(-1,0,0, x0,y1,z0);

   mvertex(1,0,0, x1,y0,z1);
   mvertex(1,0,0, x1,y1,z1);
   mvertex(1,0,0, x1,y1,z0);
   mvertex(1,0,0, x1,y0,z0);

   mvertex(0,-1,0, x0,y0,z1);
   mvertex(0,-1,0, x1,y0,z1);
   mvertex(0,-1,0, x1,y0,z0);
   mvertex(0,-1,0, x0,y0,z0);

   mvertex(0,1,0, x1,y1,z1);
   mvertex(0,1,0, x0,y1,z1);
   mvertex(0,1,0, x0,y1,z0);
   mvertex(0,1,0, x1,y1,z0);

   for (i=0; i < 6; ++i) {
      mquad(idx,idx+1,idx+2,idx+3);
      idx += 4;
   }
}

Bool phase;
#pragma warning(disable:4244)
void machine_cylinder_z(float x, float y, float z, float sx, float sz, int sides)
{
   int i;
   int center = machine_vertices;
   int idx;
   int prev_idx;
   mvertex(0,0,1, x,y,z+sz/2);
   mvertex(0,0,-1, x,y,z-sz/2);
   idx = machine_vertices;
   for (i=0; i < sides; ++i) {
      float cs = cos(3.141592*2/(2*sides)*(2*i+phase));
      float sn = sin(3.141592*2/(2*sides)*(2*i+phase));
      mvertex(cs,sn, 1, x + cs*sx, y + sn*sx, z + sz/2);
      mvertex(cs,sn,-1, x + cs*sx, y + sn*sx, z - sz/2);
   }

   prev_idx = machine_vertices - 2;
   for (i=0; i < sides; ++i) {
      // sides
      mtri(idx, prev_idx, center);
      mtri(prev_idx+1, idx+1, center+1);
      // faces
      mquad(idx, idx+1, prev_idx+1, prev_idx);
      prev_idx = idx;
      idx += 2;
   }
}

void machine_teeth_z(float x, float y, float z, float sx, float sy, float sz, float center_sz, float teeth_width, int sides)
{
   float gear_width_in_radians = teeth_width / sx;

   int idx = machine_vertices;
   int prev_idx,i;

   for (i=0; i < sides; ++i) {
      float nx,ny;
      float cs,sn;
      cs = cos(3.141592*2/(2*sides)*(2*i-1));
      sn = sin(3.141592*2/(2*sides)*(2*i-1));
      mvertex(cs,sn, 0, x + cs*sy, y + sn*sy, z + center_sz/2);
      mvertex(cs,sn, 0, x + cs*sy, y + sn*sy, z - center_sz/2);

      nx = cos(3.141592*2/sides*i-0.45);
      ny = cos(3.141592*2/sides*i-0.45);
      cs = cos(3.141592*2/sides*i - gear_width_in_radians/2);
      sn = sin(3.141592*2/sides*i - gear_width_in_radians/2);
      mvertex(nx,ny, 1, x + cs*sx, y + sn*sx, z + sz/2);
      mvertex(nx,ny, 1, x + cs*sx, y + sn*sx, z - sz/2);

      nx = cos(3.141592*2/sides*i+0.45);
      ny = cos(3.141592*2/sides*i+0.45);
      cs = cos(3.141592*2/sides*i + gear_width_in_radians/2);
      sn = sin(3.141592*2/sides*i + gear_width_in_radians/2);
      mvertex(nx,ny, 1, x + cs*sx, y + sn*sx, z + sz/2);
      mvertex(nx,ny,-1, x + cs*sx, y + sn*sx, z - sz/2);
   }

   prev_idx = machine_vertices-6;
   for (i=0; i < sides; ++i) {
      mquad(prev_idx+5, prev_idx+4, idx+0, idx+1);
      mquad(idx+1,idx+0,idx+2,idx+3);
      mquad(idx+4,idx+5,idx+3,idx+2);

      mquad(idx, prev_idx+4, prev_idx+2 , prev_idx);
      mquad(prev_idx+1, prev_idx+3, prev_idx+5, idx+1);
      prev_idx = idx;
      idx += 6;
   }
}

void build_machine(void)
{
   mc_no_rotate();
   //machine_box(0.5,0.5,0.4, 0.45,0.45,0.4);

   mc_rotate_x(1.0);
   mc_rot_center(0.25,0.25,0.25);
   machine_box_fast(0,0,0, 0.25,0.25,0.25);

   phase = 1;
   mc_rot_center(0.25,0.35,0.5);
   mc_rotate_z(32.0/127);
   machine_cylinder_z(0,0,0, 0.4/4,0.1/2, 10);
   machine_teeth_z(0,0,0,  0.52/4,0.3/4,0.95*0.1/2,0.95*0.1/2, 0.08/4, 10);

   mc_rot_center(0.50,0.35,0.5);
   mc_rotate_z(-32.0/127 * 10/16);
   machine_cylinder_z(0,0,0, 0.4/4,0.1/2, 16);
   machine_teeth_z(0,0,0,  0.52/4,0.3/4,0.95*0.1/2,0.95*0.1/2, 0.08/5, 16);


   mc_faceted_lighting(True);
   phase = 0;

   mc_rot_center(0.25,0.75,0.5);
   mc_rotate_z(32.0/127);
   machine_cylinder_z(0,0,0, 0.4/4,0.1/2, 10);
   machine_teeth_z(0,0,0,  0.52/4,0.3/4,0.1/2,0.1/2, 0.08/4, 10);

   mc_rot_center(0.50,0.75,0.5);
   mc_rotate_z(-32.0/127 * 10/16);
   machine_cylinder_z(0,0,0, 0.4/4,0.1/2, 16);
   machine_teeth_z(0,0,0,  0.52/4,0.3/4,0.1/2,0.1/2, 0.08/5, 16);

#if 0
   mc_rotate_z(1.0);
   mc_rot_center(0.75,0.75,0.25);
   machine_box(0,0,0, 0.25,0.25,0.25);
   mc_rotate_x(-0.5);
   mc_rot_center(0.25,0.75,0.25);
   machine_box(0,0,0, 0.25,0.25,0.25);
   mc_rotate_z(-0.5);
   mc_rot_center(0.75,0.25,0.25);
   machine_box(0,0,0, 0.25,0.25,0.25);
#endif

   glGenBuffersARB(1, &machine_vbuf);
   glBindBufferARB(GL_ARRAY_BUFFER_ARB, machine_vbuf);
   glBufferDataARB(GL_ARRAY_BUFFER_ARB, machine_vertices * sizeof(machine_vertex), machine_mesh_storage, GL_STATIC_DRAW_ARB);
   glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

typedef struct
{
   float transform[4];
   float bone_weights[8];
} instance_data;

#define MAX_DRAW_PICKERS  20000
static instance_data pickers[MAX_DRAW_PICKERS];
static int num_drawn_pickers;

#define MAX_DRAW_MACHINES  120000
static instance_data machines[MAX_DRAW_MACHINES];
static int num_drawn_machines;

void upload_instance_buffer(size_t *machine_offset)
{
   static int first=1;

   size_t picker_size = num_drawn_pickers * sizeof(pickers[0]);
   size_t machine_size = num_drawn_machines * sizeof(machines[0]);
   size_t total_size = picker_size + machine_size;
   *machine_offset = picker_size;

   glBindBufferARB(GL_TEXTURE_BUFFER_ARB, instance_data_buf);
   glBufferDataARB(GL_TEXTURE_BUFFER_ARB, total_size, NULL, GL_STREAM_DRAW_ARB);
   glBufferSubDataARB(GL_TEXTURE_BUFFER_ARB, 0, picker_size, pickers);
   glBufferSubDataARB(GL_TEXTURE_BUFFER_ARB, picker_size, machine_size, machines);
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
      instance_data *pd = &pickers[num_drawn_pickers++];
      pd->transform[0] = x;
      pd->transform[1] = y;
      pd->transform[2] = z;
      pd->transform[3] = (float) rot;
      memcpy(pd->bone_weights, states, sizeof(states[0])*4);
   }
}

void add_draw_machine(float x, float y, float z, int rot, float states[8])
{
   if (num_drawn_machines < MAX_DRAW_MACHINES) {
      instance_data *pd = &machines[num_drawn_machines++];
      pd->transform[0] = x;
      pd->transform[1] = y;
      pd->transform[2] = z;
      pd->transform[3] = (float) rot;
      memcpy(pd->bone_weights, states, sizeof(states[0])*8);
   }
}

void setup_instanced_uniforms(int prog, int instance_offset, float alpha)
{
   int xform_loc  = stbgl_find_uniform(prog, "xform_data");   
   int fogdata    = stbgl_find_uniform(prog, "fogdata");
   int camera_pos = stbgl_find_uniform(prog, "camera_pos");
   int recolor    = stbgl_find_uniform(prog, "recolor");
   int offset     = stbgl_find_uniform(prog, "buffer_start");

   float fog_table[4];
   float recolor_value[4] = { 1.0,1.0,1.0,alpha };

   fog_table[0] = 0.6f, fog_table[1] = 0.7f, fog_table[2] = 0.9f;
   fog_table[3] = 1.0f / (view_distance - MESH_CHUNK_SIZE_X);
   fog_table[3] *= fog_table[3];

   stbglUniform1i(xform_loc, 4);
   glActiveTextureARB(GL_TEXTURE4_ARB);
   glBindTexture(GL_TEXTURE_BUFFER_ARB, instance_data_tex);
   glActiveTextureARB(GL_TEXTURE0_ARB);

   stbglUniform4fv(fogdata   , 1, fog_table);
   stbglUniform3fv(camera_pos, 1, camloc);
   stbglUniform4fv(recolor   , 1, recolor_value);

   if (offset != -1) {
      stbglUniform1i(offset, (instance_offset >> 4));
   }
}

void draw_instanced_flush(float alpha)
{
   size_t machine_offset;
   //num_drawn_pickers=0;
   upload_instance_buffer(&machine_offset);
      
   glDisable(GL_LIGHTING);
   glDisable(GL_TEXTURE_2D);
   stbglUseProgram(picker_prog);
   glTexCoord2f(0,0);

   setup_instanced_uniforms(picker_prog, 0, alpha);

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

   stbglUseProgram(machine_prog);

   setup_instanced_uniforms(machine_prog, machine_offset, alpha);

   glBindBufferARB(GL_ARRAY_BUFFER_ARB, machine_vbuf);
   stbglVertexAttribPointer(0, 3, GL_FLOAT, 0, sizeof(machine_vertex), (void*) 0);
   stbglVertexAttribPointer(1, 3, GL_FLOAT, 0, sizeof(machine_vertex), (void*) 12);
   stbglVertexAttribPointer(2, 4, GL_BYTE, GL_TRUE, sizeof(machine_vertex), (void*) 24);
   stbglVertexAttribPointer(3, 4, GL_BYTE, GL_TRUE, sizeof(machine_vertex), (void*) 28);
   stbglVertexAttribPointer(4, 4, GL_BYTE, GL_TRUE, sizeof(machine_vertex), (void*) 32);

   stbglEnableVertexAttribArray(4);

   glDrawElementsInstancedARB(GL_TRIANGLES, num_machine_indices, GL_UNSIGNED_SHORT, machine_indices, num_drawn_machines);
   num_drawn_machines = 0;

   stbglDisableVertexAttribArray(0);
   stbglDisableVertexAttribArray(1);
   stbglDisableVertexAttribArray(2);
   stbglDisableVertexAttribArray(3);
   stbglDisableVertexAttribArray(4);
   glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
   stbglUseProgram(0);
   glActiveTextureARB(GL_TEXTURE4_ARB);
   glBindTexture(GL_TEXTURE_BUFFER_ARB, 0);

   glActiveTextureARB(GL_TEXTURE0_ARB);
}

GLuint compile_object_shader(char *type)
{
   char *vertex = stb_file(stb_sprintf("data/%s_vertex_shader.txt",type), NULL);
   char *fragment = stb_file(stb_sprintf("data/%s_fragment_shader.txt",type), NULL);
   char const *binds[] = { "position", "normal", "bone1", "bone2", "bone3", NULL };
   char error_buffer[1024];
   char const *main_vertex[] = { vertex, NULL };
   char const *main_fragment[] = { fragment, NULL };
   int which_failed;
   GLuint prog = stbgl_create_program(main_vertex, main_fragment, binds, error_buffer, sizeof(error_buffer), &which_failed);
   if (prog == 0) {
      char *progsrc = which_failed == STBGL_FAILURE_STAGE_VERTEX ? vertex : fragment;
      if(progsrc)
         stb_filewrite("obbg_failed_shader.txt", progsrc, strlen(progsrc));
      ods("Compile error for %s shader: %s\n", type, error_buffer);
      assert(0);
      exit(1);
   }
   return prog;
}

void init_object_render(void)
{
   picker_prog = compile_object_shader("picker");
   machine_prog = compile_object_shader("machine");

   create_picker_buffers();

   build_picker();
   build_machine();
}
