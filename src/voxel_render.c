//#define SINGLE_THREADED_MESHBUILD

#include "obbg_funcs.h"

#define STB_GLEXT_DECLARE "glext_list.h"
#include "stb_gl.h"
#include "stb_image.h"
#include "stb_glprog.h"

#include "stb.h"
#include "sdl.h"
#include "sdl_thread.h"
#include <math.h>

#include "stb_voxel_render.h"

GLuint main_prog;
static GLuint vox_tex[2];

void init_voxel_render(int voxel_tex[2])
{
   char *binds[] = { "attr_vertex", "attr_face", NULL };
   char *vertex;
   char *fragment;

   vertex = stbvox_get_vertex_shader();
   fragment = stbvox_get_fragment_shader();

   {
      char error_buffer[1024];
      char *main_vertex[] = { vertex, NULL };
      char *main_fragment[] = { fragment, NULL };
      int which_failed;
      main_prog = stbgl_create_program(main_vertex, main_fragment, binds, error_buffer, sizeof(error_buffer), &which_failed);
      if (main_prog == 0) {
         char *prog = which_failed == STBGL_FAILURE_STAGE_VERTEX ? vertex : fragment;
         stb_filewrite("obbg_failed_shader.txt", prog, strlen(prog));
         ods("Compile error for main shader: %s\n", error_buffer);
         assert(0);
         exit(1);
      }
   }

   vox_tex[0] = voxel_tex[0];
   vox_tex[1] = voxel_tex[1];
}

float table3[128][3];
float table4[128][4];
GLint tablei[2];
GLuint uniform_loc[STBVOX_UNIFORM_count];

#if VIEW_DIST_LOG2 < 11
int view_distance=300;
#else
int view_distance=1800;
#endif

float texture_offsets[128][2];

void setup_uniforms(float pos[3])
{
   int i,j;
   texture_offsets[22][0] -= 0.01f;
   texture_offsets[23][1] += 0.01f;
   texture_offsets[24][0] += 0.01f;
   texture_offsets[25][1] -= 0.01f;

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

            #if 0
            case STBVOX_UNIFORM_color_table:
               compute_colortable();
               data = colortable;
               break;
            #endif

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

void upload_mesh(mesh_chunk *mc, uint8 *vertex_build_buffer, uint8 *face_buffer)
{
   glGenBuffersARB(1, &mc->vbuf);
   glBindBufferARB(GL_ARRAY_BUFFER_ARB, mc->vbuf);
   glBufferDataARB(GL_ARRAY_BUFFER_ARB, mc->num_quads*4*sizeof(uint32), vertex_build_buffer, GL_STATIC_DRAW_ARB);
   glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

   glGenBuffersARB(1, &mc->fbuf);
   glBindBufferARB(GL_TEXTURE_BUFFER_ARB, mc->fbuf);
   glBufferDataARB(GL_TEXTURE_BUFFER_ARB, mc->num_quads*sizeof(uint32), face_buffer , GL_STATIC_DRAW_ARB);
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
} consider_mesh_t;

static consider_mesh_t consider_mesh[MAX_CONSIDER_MESHES];

void request_mesh_generation(int qchunk_x, int qchunk_y, int cam_x, int cam_y)
{
   int i,j, n=0, m=0;
   int rad = (view_distance >> MESH_CHUNK_SIZE_X_LOG2) + 1;
   requested_mesh *rm = get_requested_mesh_alternate();

   for (j=-rad; j <= rad; ++j) {
      for (i=-rad; i <= rad; ++i) {
         if (i*i + j*j <= rad*rad) {
            int cx = qchunk_x + i;
            int cy = qchunk_y + j;
            int wx = cx * MESH_CHUNK_SIZE_X;
            int wy = cy * MESH_CHUNK_SIZE_Y;
            int slot_x = cx & (C_MESH_CHUNK_CACHE_X-1);
            int slot_y = cy & (C_MESH_CHUNK_CACHE_Y-1);
            mesh_chunk *mc = c_mesh_cache[slot_y][slot_x];
            if (mc == NULL || mc->chunk_x != cx || mc->chunk_y != cy || mc->vbuf == 0 || mc->dirty) {
               consider_mesh[n].x = wx;
               consider_mesh[n].y = wy;  
               consider_mesh[n].priority = (float) (i*i + j*j);
               consider_mesh[n].dirty = mc && mc->dirty;
               ++n;
            }
         }
      }
   }

   qsort(consider_mesh, n, sizeof(consider_mesh[0]), stb_floatcmp(offsetof(consider_mesh_t, priority)));

   n = stb_min(n, MAX_BUILT_MESHES-16);

   m = physics_set_player_coord(rm, MAX_BUILT_MESHES, cam_x, cam_y);

   for (i=0; i < n && m < MAX_BUILT_MESHES; ++i) {
      rm[m].x = consider_mesh[i].x;
      rm[m].y = consider_mesh[i].y;
      rm[m].state = RMS_requested;
      rm[m].needs_triangles = True;
      rm[m].rebuild_chunks = consider_mesh[i].dirty;
      ++m;
   }
   for (; i < MAX_BUILT_MESHES; ++i)
      rm[i].state = RMS_invalid;

   swap_requested_meshes();
}


void render_voxel_world(float campos[3])
{
   int num_build_remaining;
   int distance;
   float x = campos[0], y = campos[1];
   int qchunk_x, qchunk_y;
   int cam_x, cam_y;
   int i,j, rad;

#ifdef SINGLE_THREADED_MESHBUILD
   num_build_remaining = 1;
#else
   num_build_remaining = 0;
#endif

   cam_x = (int) floor(x);
   cam_y = (int) floor(y);

   qchunk_x = C_MESH_CHUNK_X_FOR_WORLD_X(cam_x);
   qchunk_y = C_MESH_CHUNK_Y_FOR_WORLD_Y(cam_y);

   request_mesh_generation(qchunk_x, qchunk_y, cam_x, cam_y);

   glEnable(GL_ALPHA_TEST);
   glAlphaFunc(GL_GREATER, 0.5);

   stbglUseProgram(main_prog);
   setup_uniforms(campos); // set uniforms to default values inefficiently
   glActiveTextureARB(GL_TEXTURE2_ARB);
   stbglEnableVertexAttribArray(0);

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

            if (mc == NULL || mc->chunk_x != cx || mc->chunk_y != cy || mc->vbuf == 0) {
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
               mc = build_mesh_chunk_for_coord(cx * C_MESH_CHUNK_CACHE_X, cy * C_MESH_CHUNK_CACHE_Y);
               --num_build_remaining;
            }

            ++chunk_locations;

            ++chunks_considered;
            quads_considered += mc->num_quads;
            chunk_storage_considered += mc->num_quads * 20;

            if (mc->num_quads) {
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
            if (mc == NULL || mc->chunk_x != cx || mc->chunk_y != cy || mc->vbuf == 0 || mc->dirty) {
               mc = build_mesh_chunk_for_coord(cx * C_MESH_CHUNK_CACHE_X, cy * C_MESH_CHUNK_CACHE_Y);
               --num_build_remaining;
               if (num_build_remaining == 0)
                  goto done;
            }
         }
      }
      done:
      ;
   }

   {
      built_mesh bm;
      while (get_next_built_mesh(&bm)) {
         if (!bm.mc->has_triangles) {
            // server:
            physics_process_mesh_chunk(bm.mc);
            // don't free the physics data below, because the above call copies them
            bm.mc->allocs = NULL;
            free_mesh_chunk(bm.mc);
         } else {
            //s_process_mesh_chunk(bm.mc);
            // client:
            upload_mesh(bm.mc, bm.vertex_build_buffer, bm.face_buffer);
            set_mesh_chunk_for_coord(bm.mc->chunk_x * MESH_CHUNK_SIZE_X, bm.mc->chunk_y * MESH_CHUNK_SIZE_Y, bm.mc);
            free(bm.face_buffer);
            free(bm.vertex_build_buffer);
            bm.mc = NULL;
         }
      }
   }

   chunk_storage_total = 0;
   for (j=0; j < C_MESH_CHUNK_CACHE_Y; ++j)
      for (i=0; i < C_MESH_CHUNK_CACHE_X; ++i)
         if (c_mesh_cache[j][i] != NULL && c_mesh_cache[j][i]->vbuf)
            chunk_storage_total += c_mesh_cache[j][i]->num_quads * 20;

   stbglDisableVertexAttribArray(0);
   glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
   glActiveTextureARB(GL_TEXTURE0_ARB);

   stbglUseProgram(0);
}
