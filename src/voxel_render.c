//#define SINGLE_THREADED_MESHBUILD

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

static GLuint dyn_vbuf, dyn_fbuf, dyn_fbuf_tex;

static void init_dyn_mesh(void)
{
   glGenBuffersARB(1, &dyn_vbuf);
   glGenBuffersARB(1, &dyn_fbuf);
   glGenTextures(1, &dyn_fbuf_tex);
}

static void upload_dyn_mesh(void *vertex_build_buffer, void *face_buffer, int num_quads)
{
   glBindBufferARB(GL_ARRAY_BUFFER_ARB, dyn_vbuf);
   glBufferDataARB(GL_ARRAY_BUFFER_ARB, 4*4*num_quads, vertex_build_buffer, GL_DYNAMIC_DRAW_ARB);
   glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

   glBindBufferARB(GL_TEXTURE_BUFFER_ARB, dyn_fbuf);
   glBufferDataARB(GL_TEXTURE_BUFFER_ARB, 4*num_quads, face_buffer , GL_DYNAMIC_DRAW_ARB);
   glBindBufferARB(GL_TEXTURE_BUFFER_ARB, 0);

   glBindTexture(GL_TEXTURE_BUFFER_ARB, dyn_fbuf_tex);
   glTexBufferARB(GL_TEXTURE_BUFFER_ARB, GL_RGBA8UI, dyn_fbuf);
   glBindTexture(GL_TEXTURE_BUFFER_ARB, 0);
}

#ifdef MINIMIZE_MEMORY
size_t mesh_cache_max_storage = 1 << 25; // 32 MB
#else
size_t mesh_cache_max_storage = 1 << 30; // 1 GB
#endif

static mesh_chunk *c_mesh_cache[C_MESH_CHUNK_CACHE_Y][C_MESH_CHUNK_CACHE_X];
size_t c_mesh_cache_in_use;

mesh_chunk *create_placeholder_mesh_chunk(int cx, int cy)
{
   mesh_chunk *mc = obbg_malloc(sizeof(*mc), "/mesh/chunk/dummy");
   memset(mc, 0, sizeof(*mc));
   mc->chunk_x = cx;
   mc->chunk_y = cy;
   mc->placeholder_for_size_info = True;
   return mc;
}

void set_mesh_chunk_for_coord(int x, int y, mesh_chunk *new_mc)
{
   int cx = C_MESH_CHUNK_X_FOR_WORLD_X(x);
   int slot_x = cx & (C_MESH_CHUNK_CACHE_X-1);
   int cy = C_MESH_CHUNK_Y_FOR_WORLD_Y(y);
   int slot_y = cy & (C_MESH_CHUNK_CACHE_Y-1);
   mesh_chunk *mc = c_mesh_cache[slot_y][slot_x];
   if (mc) {
      if (!mc->placeholder_for_size_info)
         c_mesh_cache_in_use -= mc->total_size;
      free_mesh_chunk(mc);
   }

   c_mesh_cache[slot_y][slot_x] = new_mc;
   c_mesh_cache[slot_y][slot_x]->dirty = False;
   if (!new_mc->placeholder_for_size_info)
      c_mesh_cache_in_use += new_mc->total_size;
}

void force_update_for_block_raw(int x, int y, int z)
{
   int cx = C_MESH_CHUNK_X_FOR_WORLD_X(x);
   int slot_x = cx & (C_MESH_CHUNK_CACHE_X-1);
   int cy = C_MESH_CHUNK_Y_FOR_WORLD_Y(y);
   int slot_y = cy & (C_MESH_CHUNK_CACHE_Y-1);
   mesh_chunk *mc = c_mesh_cache[slot_y][slot_x];
   if (mc && mc->chunk_x == cx && mc->chunk_y == cy)
      mc->dirty = True;
}

void init_mesh_cache(void)
{
   int i,j;
   for (j=0; j < C_MESH_CHUNK_CACHE_Y; ++j)
      for (i=0; i < C_MESH_CHUNK_CACHE_X; ++i)
         c_mesh_cache[j][i] = 0;
   c_mesh_cache_in_use = 0;
}

mesh_chunk *get_mesh_chunk_for_coord(int x, int y)
{
   int cx = C_MESH_CHUNK_X_FOR_WORLD_X(x);
   int cy = C_MESH_CHUNK_Y_FOR_WORLD_Y(y);
   mesh_chunk *mc = c_mesh_cache[cy & (C_MESH_CHUNK_CACHE_Y-1)][cx & (C_MESH_CHUNK_CACHE_X-1)];

   if (mc && mc->chunk_x == cx && mc->chunk_y == cy)
      return mc;
   else
      return NULL;
}

void free_mesh_chunk_physics(mesh_chunk *mc)
{
   if (mc->allocs) {
      int i;
      for (i=0; i < obarr_len(mc->allocs); ++i)
         obbg_free(mc->allocs[i]);
      obarr_free(mc->allocs);
   }
}

void free_mesh_chunk(mesh_chunk *mc)
{
   if (mc->placeholder_for_size_info) {
      obbg_free(mc);
   } else {
      glDeleteTextures(1, &mc->fbuf_tex);
      glDeleteBuffersARB(1, &mc->vbuf);
      glDeleteBuffersARB(1, &mc->fbuf);

      free_mesh_chunk_physics(mc);
      obbg_free(mc);
   }
}

Bool discard_lowest_priority_from_mesh_cache(float min_priority)
{
   int i,j,worst_i=-1,worst_j;
   float worst_found = 0;
   for (j=0; j < C_MESH_CHUNK_CACHE_Y; ++j) {
      for (i=0; i < C_MESH_CHUNK_CACHE_X; ++i) {
         mesh_chunk *mc = c_mesh_cache[j][i];
         if (mc != NULL && !mc->placeholder_for_size_info && mc->priority > worst_found) {
            worst_found = c_mesh_cache[j][i]->priority;
            worst_i = i;
            worst_j = j;
         }
      }
   }

   if (worst_i < 0)
      return False;
   if (worst_found <= min_priority)
      return False;

   //ods("Free priority %f (%d) to create %f\n", c_mesh_cache[worst_j][worst_i]->priority, c_mesh_cache[worst_j][worst_i]->total_size, min_priority);
   assert(!c_mesh_cache[worst_j][worst_i]->placeholder_for_size_info);
   c_mesh_cache_in_use -= c_mesh_cache[worst_j][worst_i]->total_size;
   free_mesh_chunk(c_mesh_cache[worst_j][worst_i]);
   c_mesh_cache[worst_j][worst_i] = NULL;
   return True;
}



GLuint main_prog;

static GLuint vox_tex[2];

void init_voxel_render(int voxel_tex[2])
{
   char *vertex = stbvox_get_vertex_shader();
   char *fragment = stbvox_get_fragment_shader();
   char const *binds[] = { "attr_vertex", "attr_face", NULL };
   char error_buffer[1024];
   char const *main_vertex[] = { vertex, NULL };
   char const *main_fragment[] = { fragment, NULL };
   int which_failed;
   main_prog = stbgl_create_program(main_vertex, main_fragment, binds, error_buffer, sizeof(error_buffer), &which_failed);
   if (main_prog == 0) {
      char *prog = which_failed == STBGL_FAILURE_STAGE_VERTEX ? vertex : fragment;
      stb_filewrite("obbg_failed_shader.txt", prog, strlen(prog));
      ods("Compile error for main shader: %s\n", error_buffer);
      assert(0);
      exit(1);
   }

   vox_tex[0] = voxel_tex[0];
   vox_tex[1] = voxel_tex[1];

   init_dyn_mesh();
}


#if VIEW_DIST_LOG2 < 11
int view_distance=300;
#else
int view_distance=1800;
#endif

float table3[128][3];
float table4[128][4];
GLint tablei[2];
GLuint uniform_loc[STBVOX_UNIFORM_count];

int tex_anim_offset;
float texture_offsets[128][2];
float logistics_texture_scroll;

float colortable[64][4] =
{
   { 1,1,1,1 },
};

void setup_uniforms(float pos[3], float alpha)
{
   int i,j;
   texture_offsets[22][0] = -logistics_texture_scroll;

   for (i=0; i < STBVOX_UNIFORM_count; ++i) {
      stbvox_uniform_info raw, *ui=&raw;
      stbvox_get_uniform_info(&raw, i);
      uniform_loc[i] = -1;

      #if 0
      if (i == STBVOX_UNIFORM_texscale || i == STBVOX_UNIFORM_texgen || i == STBVOX_UNIFORM_color_table)
         continue;
      #endif

      if (ui) {
         void *data = ui->default_value;
         uniform_loc[i] = stbgl_find_uniform(main_prog, ui->name);
         switch (i) {
            case STBVOX_UNIFORM_face_data:
               tablei[0] = 2;
               data = tablei;
               break;

            case STBVOX_UNIFORM_texanim:
               tablei[0] = 0x0f;
               tablei[1] = tex_anim_offset;
               data = tablei;
               break;

            case STBVOX_UNIFORM_tex_array:
               glActiveTextureARB(GL_TEXTURE0_ARB);
               glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, vox_tex[0]);
               glActiveTextureARB(GL_TEXTURE1_ARB);
               glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, vox_tex[1]);
               glActiveTextureARB(GL_TEXTURE0_ARB);
               tablei[0] = 0;
               tablei[1] = 1;
               data = tablei;
               break;

            case STBVOX_UNIFORM_texscale:
               for (j=0; j < 128; ++j) {
                  table4[j][0] = texture_scales[j];
                  table4[j][1] = 1.0f;
                  table4[j][2] = texture_offsets[j][0];
                  table4[j][3] = texture_offsets[j][1];
               }
               data = table4;
               break;

            case STBVOX_UNIFORM_color_table:
               data = colortable;
               colortable[0][3] = alpha;
               break;

            case STBVOX_UNIFORM_camera_pos:
               data = table3[0];
               table3[0][0] = pos[0];
               table3[0][1] = pos[1];
               table3[0][2] = pos[2];
               table3[0][3] = 1.0f;
               break;

            case STBVOX_UNIFORM_ambient: {
               float bright = 1.0f;
               float amb[3][3];

               //bright = 0.35f;  // when demoing lighting

               // ambient direction is sky-colored upwards
               // "ambient" lighting is from above
               table4[0][0] =  0.3f;
               table4[0][1] = -0.5f;
               table4[0][2] =  0.9f;
               table4[0][3] = 0;

               amb[1][0] = 0.3f; amb[1][1] = 0.3f; amb[1][2] = 0.3f; // dark-grey
               amb[2][0] = 1.0; amb[2][1] = 1.0; amb[2][2] = 1.0; // white

               // convert so (table[1]*dot+table[2]) gives
               // above interpolation
               //     lerp((dot+1)/2, amb[1], amb[2])
               //     amb[1] + (amb[2] - amb[1]) * (dot+1)/2
               //     amb[1] + (amb[2] - amb[1]) * dot/2 + (amb[2]-amb[1])/2

               for (j=0; j < 3; ++j) {
                  table4[1][j] = (amb[2][j] - amb[1][j])/2 * bright;
                  table4[2][j] = (amb[1][j] + amb[2][j])/2 * bright;
               }
               table4[1][3] = 0;
               table4[2][3] = 0;

               // fog color
               table4[3][0] = 0.6f, table4[3][1] = 0.7f, table4[3][2] = 0.9f;
               table4[3][3] = 1.0f / (view_distance - MESH_CHUNK_SIZE_X);
               table4[3][3] *= table4[3][3];

               data = table4;
               break;
            }
         }

         switch (ui->type) {
            case STBVOX_UNIFORM_TYPE_sampler: stbglUniform1iv(uniform_loc[i], ui->array_length, data); break;
            case STBVOX_UNIFORM_TYPE_vec2i:   stbglUniform2iv(uniform_loc[i], ui->array_length, data); break;
            case STBVOX_UNIFORM_TYPE_vec2:    stbglUniform2fv(uniform_loc[i], ui->array_length, data); break;
            case STBVOX_UNIFORM_TYPE_vec3:    stbglUniform3fv(uniform_loc[i], ui->array_length, data); break;
            case STBVOX_UNIFORM_TYPE_vec4:    stbglUniform4fv(uniform_loc[i], ui->array_length, data); break;
         }
      }
   }
}

typedef struct
{
   float x,y,z,w;
} plane;

static plane frustum[6];

static void matd_mul(double out[4][4], double src1[4][4], double src2[4][4])
{
   int i,j,k;
   for (j=0; j < 4; ++j) {
      for (i=0; i < 4; ++i) {
         double t=0;
         for (k=0; k < 4; ++k)
            t += src1[k][i] * src2[j][k];
         out[i][j] = t;
      }
   }
}

// https://fgiesen.wordpress.com/2012/08/31/frustum-planes-from-the-projection-matrix/
static void compute_frustum(void)
{
   int i;
   GLdouble mv[4][4],proj[4][4], mvproj[4][4];
   glGetDoublev(GL_MODELVIEW_MATRIX , mv[0]);
   glGetDoublev(GL_PROJECTION_MATRIX, proj[0]);
   matd_mul(mvproj, proj, mv);
   for (i=0; i < 4; ++i) {
      (&frustum[0].x)[i] = (float) (mvproj[3][i] + mvproj[0][i]);
      (&frustum[1].x)[i] = (float) (mvproj[3][i] - mvproj[0][i]);
      (&frustum[2].x)[i] = (float) (mvproj[3][i] + mvproj[1][i]);
      (&frustum[3].x)[i] = (float) (mvproj[3][i] - mvproj[1][i]);
      (&frustum[4].x)[i] = (float) (mvproj[3][i] + mvproj[2][i]);
      (&frustum[5].x)[i] = (float) (mvproj[3][i] - mvproj[2][i]);
   }   
}

static int test_plane(plane *p, float x0, float y0, float z0, float x1, float y1, float z1)
{
   // return false if the box is entirely behind the plane
   float d=0;
   assert(x0 <= x1 && y0 <= y1 && z0 <= z1);
   if (p->x > 0) d += x1*p->x; else d += x0*p->x;
   if (p->y > 0) d += y1*p->y; else d += y0*p->y;
   if (p->z > 0) d += z1*p->z; else d += z0*p->z;
   return d + p->w >= 0;
}

static int is_box_in_frustum(float *bmin, float *bmax)
{
   int i;
   for (i=0; i < 5; ++i)
      if (!test_plane(&frustum[i], bmin[0], bmin[1], bmin[2], bmax[0], bmax[1], bmax[2]))
         return 0;
   return 1;
}

void compute_mesh_sizes(mesh_chunk *mc)
{
   mc->vbuf_size = mc->num_quads*4*sizeof(uint32);
   mc->fbuf_size = mc->num_quads*sizeof(uint32);
   mc->total_size = mc->fbuf_size + mc->vbuf_size;
}

void upload_mesh(mesh_chunk *mc, uint8 *vertex_build_buffer, uint8 *face_buffer)
{
   glGenBuffersARB(1, &mc->vbuf);
   glBindBufferARB(GL_ARRAY_BUFFER_ARB, mc->vbuf);
   glBufferDataARB(GL_ARRAY_BUFFER_ARB, mc->vbuf_size, vertex_build_buffer, GL_STATIC_DRAW_ARB);
   glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

   glGenBuffersARB(1, &mc->fbuf);
   glBindBufferARB(GL_TEXTURE_BUFFER_ARB, mc->fbuf);
   glBufferDataARB(GL_TEXTURE_BUFFER_ARB, mc->fbuf_size, face_buffer , GL_STATIC_DRAW_ARB);
   glBindBufferARB(GL_TEXTURE_BUFFER_ARB, 0);

   glGenTextures(1, &mc->fbuf_tex);
   glBindTexture(GL_TEXTURE_BUFFER_ARB, mc->fbuf_tex);
   glTexBufferARB(GL_TEXTURE_BUFFER_ARB, GL_RGBA8UI, mc->fbuf);
   glBindTexture(GL_TEXTURE_BUFFER_ARB, 0);
}

extern int num_threads_active, num_meshes_started, num_meshes_uploaded;
extern float light_pos[3];

#define MAX_CONSIDER_MESHES 4096

typedef struct
{
   int x,y;
   Bool dirty;
   float priority;
   mesh_chunk *mc;
} consider_mesh_t;

static consider_mesh_t consider_mesh[MAX_CONSIDER_MESHES];
static requested_mesh physics_mesh[MAX_CONSIDER_MESHES];

#define PRIORITY_unused   (50000.0f*50000.0f*2.0f)
#define PRIORITY_discard  (PRIORITY_unused*2.0f)

void request_mesh_generation(int cam_x, int cam_y)
{
   size_t storage=0;
   int i,j, n=0, m=0, num_phys_req;
   int rad = (view_distance >> MESH_CHUNK_SIZE_X_LOG2) + 1;
   requested_mesh *rm = get_requested_mesh_alternate();
   int qchunk_x = C_MESH_CHUNK_X_FOR_WORLD_X(cam_x);
   int qchunk_y = C_MESH_CHUNK_Y_FOR_WORLD_Y(cam_y);
   int chunk_center_x = MESH_CHUNK_SIZE_X/2;
   int chunk_center_y = MESH_CHUNK_SIZE_Y/2;

   for (j=0; j < C_MESH_CHUNK_CACHE_Y; ++j)
      for (i=0; i < C_MESH_CHUNK_CACHE_X; ++i)
         if (c_mesh_cache[j][i] != NULL)
            c_mesh_cache[j][i]->priority = PRIORITY_unused;

   for (j=-rad; j <= rad; ++j) {
      for (i=-rad; i <= rad; ++i) {
         if (i*i + j*j <= rad*rad) {
            int cx = qchunk_x + i;
            int cy = qchunk_y + j;
            int wx = cx * MESH_CHUNK_SIZE_X;
            int wy = cy * MESH_CHUNK_SIZE_Y;
            int dist_x = (wx + chunk_center_x - cam_x);
            int dist_y = (wy + chunk_center_y - cam_y);
            int slot_x = cx & (C_MESH_CHUNK_CACHE_X-1);
            int slot_y = cy & (C_MESH_CHUNK_CACHE_Y-1);
            mesh_chunk *mc = c_mesh_cache[slot_y][slot_x];
            Bool needs_building = (mc == NULL || mc->chunk_x != cx || mc->chunk_y != cy || mc->dirty);
            assert(n < MAX_CONSIDER_MESHES);
            //assert(mc != NULL && !mc->placeholder_for_size_info);
            consider_mesh[n].mc = needs_building ? NULL : mc;
            //assert(consider_mesh[n].mc != NULL && !consider_mesh[n].mc->placeholder_for_size_info);
            consider_mesh[n].x = wx;
            consider_mesh[n].y = wy;  
            consider_mesh[n].priority = (float) dist_x*dist_x + (float) dist_y*dist_y; //((float) (i+0.15f)*(i+0.15f) + (float) (j+0.21f)*(j+0.21f));
            consider_mesh[n].dirty = needs_building && mc != NULL && mc->dirty; // @TODO: is && mc->dirty redundant?
            if (mc != NULL)
               if (!needs_building) {
                  mc->priority = consider_mesh[n].priority;
                  //assert(mc != NULL && !mc->placeholder_for_size_info);
               } else
                  mc->priority = PRIORITY_discard;
            ++n;
         }
      }
   }

   qsort(consider_mesh, n, sizeof(consider_mesh[0]), stb_floatcmp(offsetof(consider_mesh_t, priority)));

   storage = 0;
   for (i=0; i < n; ++i) {
      if (consider_mesh[i].mc != NULL) {
         size_t new_storage = storage + consider_mesh[i].mc->total_size;
         if (new_storage > mesh_cache_max_storage) {
            n=i;
            break;
         }
         //if (consider_mesh[i].mc->placeholder_for_size_info) ods("Include placeholder %f size %d after %d\n", consider_mesh[i].mc->priority, consider_mesh[i].mc->total_size, storage);
         storage = new_storage;
      }
   }
   mesh_cache_requested_in_use = storage;

   // at this point, mesh consider list is cut off at point that all extent
   // meshes and placeholder meshes in priority order fit in cache and the
   // next in priority order does not.

   // fill out start of requested_meshes with physics meshes, which are always higher priority
   num_phys_req = physics_set_player_coord(physics_mesh, MAX_BUILT_MESHES, cam_x, cam_y);

   i = 0;
   j = 0;
   m = 0;

   // merge sorted arrays
   while (i < n || j < num_phys_req) {
      if (m >= MAX_BUILT_MESHES)
         break;
      while (i < n && !(consider_mesh[i].mc == NULL || consider_mesh[i].mc->placeholder_for_size_info))
         ++i;

      if ((i < n && j < num_phys_req && physics_mesh[j].priority < consider_mesh[i].priority) || i >= n) {
         rm[m++] = physics_mesh[j++];
      } else if (i < n) {
         rm[m].x = consider_mesh[i].x;
         rm[m].y = consider_mesh[i].y;
         rm[m].state = RMS_requested;
         rm[m].needs_triangles = True;
         rm[m].rebuild_chunks = consider_mesh[i].dirty;
         rm[m].priority = consider_mesh[i].priority;
         ++i;
         ++m;
      }
   }

   // zero out the rest of the request list so we don't pass around an explicit length
   for (; m < MAX_BUILT_MESHES; ++m)
      rm[m].state = RMS_invalid;

   swap_requested_meshes();
}

float temp_campos[3];

void render_voxel_world(float campos[3])
{
   int num_build_remaining;
   int distance;
   float x = campos[0], y = campos[1];
   int qchunk_x, qchunk_y;
   int cam_x, cam_y;
   int i,j, rad;
   built_mesh bm;

#ifdef SINGLE_THREADED_MESHBUILD
   num_build_remaining = 1;
#else
   num_build_remaining = 0;
#endif

   cam_x = (int) floor(x);
   cam_y = (int) floor(y);

   request_mesh_generation(cam_x, cam_y);
   memcpy(temp_campos, campos, sizeof(temp_campos));

   glEnable(GL_ALPHA_TEST);
   glAlphaFunc(GL_GREATER, 0.5);

   stbglUseProgram(main_prog);
   setup_uniforms(campos, 1.0f); // set uniforms to default values inefficiently
   glActiveTextureARB(GL_TEXTURE2_ARB);
   stbglEnableVertexAttribArray(0);

   qchunk_x = C_MESH_CHUNK_X_FOR_WORLD_X(cam_x);
   qchunk_y = C_MESH_CHUNK_Y_FOR_WORLD_Y(cam_y);
   rad = view_distance >> MESH_CHUNK_SIZE_X_LOG2;
   view_dist_for_display = view_distance;

   {
      float lighting[2][3] = { { 0,0,0 }, { 0.75,0.75,0.65f } };
      float bright = 32;
      lighting[0][0] = light_pos[0];
      lighting[0][1] = light_pos[1];
      lighting[0][2] = light_pos[2];
      lighting[1][0] *= bright;
      lighting[1][1] *= bright;
      lighting[1][2] *= bright;
      stbglUniform3fv(stbgl_find_uniform(main_prog, "light_source"), 2, lighting[0]);
   }

   quads_rendered = 0;
   quads_considered = 0;
   chunk_storage_rendered = 0;
   chunk_storage_considered = 0;
   chunk_locations = 0;
   chunks_considered = 0;
   chunks_in_frustum = 0;

   compute_frustum();

   for (distance = 0; distance <= rad; ++distance) {
      for (j=-distance; j <= distance; ++j) {
         for (i=-distance; i <= distance; ++i) {
            int cx = qchunk_x + i;
            int cy = qchunk_y + j;
            int slot_x = cx & (C_MESH_CHUNK_CACHE_X-1);
            int slot_y = cy & (C_MESH_CHUNK_CACHE_Y-1);
            mesh_chunk *mc = c_mesh_cache[slot_y][slot_x];

            if (stb_max(abs(i),abs(j)) != distance)
               continue;

            if (i*i + j*j > rad*rad)
               continue;

            if (mc == NULL || mc->placeholder_for_size_info || mc->chunk_x != cx || mc->chunk_y != cy || mc->vbuf == 0) {
               float estimated_bounds[2][3];
               if (num_build_remaining == 0)
                  continue;
               estimated_bounds[0][0] = (float) ( cx    << MESH_CHUNK_SIZE_X_LOG2);
               estimated_bounds[0][1] = (float) ( cy    << MESH_CHUNK_SIZE_Y_LOG2);
               estimated_bounds[0][2] = (float) (0);
               estimated_bounds[1][0] = (float) ((cx+1) << MESH_CHUNK_SIZE_X_LOG2);
               estimated_bounds[1][1] = (float) ((cy+1) << MESH_CHUNK_SIZE_Y_LOG2);
               estimated_bounds[1][2] = (float) (255);
               if (!is_box_in_frustum(estimated_bounds[0], estimated_bounds[1]))
                  continue;
               mc = build_mesh_chunk_for_coord(mc, cx * C_MESH_CHUNK_CACHE_X, cy * C_MESH_CHUNK_CACHE_Y);
               --num_build_remaining;
            }

            ++chunk_locations;

            ++chunks_considered;
            quads_considered += mc->num_quads;
            chunk_storage_considered += mc->num_quads * 20;

            if (mc->num_quads && !mc->placeholder_for_size_info) {
               if (is_box_in_frustum(mc->bounds[0], mc->bounds[1])) {
                  // @TODO if in range, frustum cull
                  stbglUniform3fv(stbgl_find_uniform(main_prog, "transform"), 3, mc->transform[0]);
                  glBindBufferARB(GL_ARRAY_BUFFER_ARB, mc->vbuf);
                  glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 4, (void*) 0);
                  glBindTexture(GL_TEXTURE_BUFFER_ARB, mc->fbuf_tex);

                  glDrawArrays(GL_QUADS, 0, mc->num_quads*4);

                  quads_rendered += mc->num_quads;
                  ++chunks_in_frustum;
                  chunk_storage_rendered += mc->num_quads * 20;
               }
            }
         }
      }
   }

   if (num_build_remaining) {
      for (j=-rad; j <= rad; ++j) {
         for (i=-rad; i <= rad; ++i) {
            int cx = qchunk_x + i;
            int cy = qchunk_y + j;
            int slot_x = cx & (C_MESH_CHUNK_CACHE_X-1);
            int slot_y = cy & (C_MESH_CHUNK_CACHE_Y-1);
            mesh_chunk *mc = c_mesh_cache[slot_y][slot_x];
            if (mc == NULL || mc->placeholder_for_size_info || mc->chunk_x != cx || mc->chunk_y != cy || mc->vbuf == 0 || mc->dirty) {
               mc = build_mesh_chunk_for_coord(mc, cx * C_MESH_CHUNK_CACHE_X, cy * C_MESH_CHUNK_CACHE_Y);
               --num_build_remaining;
               if (num_build_remaining == 0)
                  goto done;
            }
         }
      }
      done:
      ;
   }

   // process new meshes from the job system
   while (get_next_built_mesh(&bm)) {
      if (!bm.mc->has_triangles) { // is it a physics mesh_chunk?
         // server:
         physics_process_mesh_chunk(bm.mc);
         // don't free the physics data below, because the above call copies them
         bm.mc->allocs = NULL;
         finished_caching_mesh_chunk(bm.mc->chunk_x * MESH_CHUNK_SIZE_X, bm.mc->chunk_y * MESH_CHUNK_SIZE_Y, False);
         free_mesh_chunk(bm.mc);
      } else {
         // it's a rendering mesh_chunk
         Bool add_mesh_to_cache = True;
         //s_process_mesh_chunk(bm.mc);
         // client:
         //ods("Received chunk: %d,%d", bm.mc->chunk_x, bm.mc->chunk_y);

         compute_mesh_sizes(bm.mc);

         for(;;) {
            size_t old_size = c_mesh_cache_in_use;
            if (c_mesh_cache_in_use + bm.mc->total_size <= mesh_cache_max_storage)
               break;
            if (!discard_lowest_priority_from_mesh_cache(bm.mc->priority)) {
               add_mesh_to_cache = False;
               break;
            }
            assert(c_mesh_cache_in_use < old_size);
         }

         if (add_mesh_to_cache) {
            upload_mesh(bm.mc, bm.vertex_build_buffer, bm.face_buffer);
            set_mesh_chunk_for_coord(bm.mc->chunk_x * MESH_CHUNK_SIZE_X, bm.mc->chunk_y * MESH_CHUNK_SIZE_Y, bm.mc);
         } else {
            mesh_chunk *mc = create_placeholder_mesh_chunk(bm.mc->chunk_x, bm.mc->chunk_y);
            mc->priority = bm.mc->priority;
            mc->total_size = bm.mc->total_size;
            set_mesh_chunk_for_coord(bm.mc->chunk_x * MESH_CHUNK_SIZE_X, bm.mc->chunk_y * MESH_CHUNK_SIZE_Y, mc   );
         }
         obbg_free(bm.face_buffer);
         obbg_free(bm.vertex_build_buffer);
         finished_caching_mesh_chunk(bm.mc->chunk_x * MESH_CHUNK_SIZE_X, bm.mc->chunk_y * MESH_CHUNK_SIZE_Y, True);
         bm.mc = NULL;
      }
   }

   chunk_storage_total = 0;
   for (j=0; j < C_MESH_CHUNK_CACHE_Y; ++j)
      for (i=0; i < C_MESH_CHUNK_CACHE_X; ++i)
         if (c_mesh_cache[j][i] != NULL && c_mesh_cache[j][i]->vbuf && !c_mesh_cache[j][i]->placeholder_for_size_info)
            chunk_storage_total += c_mesh_cache[j][i]->num_quads * 20;

   stbglDisableVertexAttribArray(0);
   glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
   glActiveTextureARB(GL_TEXTURE0_ARB);

   stbglUseProgram(0);
}

void voxel_draw_block(int x, int y, int z, int blocktype, int rot)
{
   uint32 vertex_build_buffer[96*4];
   uint32 face_buffer[96];
   float transform[3][3];

   int num_quads;
   uint8 mesh_geom[4][4][4] = { 0 };
   uint8 mesh_lighting[4][4][4];

   uint8 light = lighting_with_rotation(255,0);
   memset(mesh_lighting, light, sizeof(mesh_lighting));

   mesh_geom[1][1][1] = blocktype;
   mesh_lighting[1][1][1] = lighting_with_rotation(255,rot);

   num_quads = build_small_mesh(x, y, z, mesh_geom, mesh_lighting, 96, (uint8*) vertex_build_buffer, (uint8*) face_buffer, transform);
   upload_dyn_mesh(vertex_build_buffer, face_buffer, num_quads);

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   stbglUseProgram(main_prog);
   setup_uniforms(temp_campos,0.4f); // set uniforms to default values inefficiently
   glActiveTextureARB(GL_TEXTURE2_ARB);
   stbglEnableVertexAttribArray(0);
   stbglUniform3fv(stbgl_find_uniform(main_prog, "transform"), 3, transform[0]);
   glBindBufferARB(GL_ARRAY_BUFFER_ARB, dyn_vbuf);
   glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 4, (void*) 0);
   glBindTexture(GL_TEXTURE_BUFFER_ARB, dyn_fbuf_tex);
   glDrawArrays(GL_QUADS, 0, num_quads*4);
   stbglUseProgram(0);
   glActiveTextureARB(GL_TEXTURE0_ARB);
   glBindTexture(GL_TEXTURE_BUFFER_ARB, 0);
   glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
   stbglDisableVertexAttribArray(0);
   glDisable(GL_BLEND);
}

