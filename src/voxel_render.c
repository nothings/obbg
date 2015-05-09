#define STB_GLEXT_DECLARE "glext_list.h"
#include "stb_gl.h"
#include "stb_image.h"
#include "stb_glprog.h"

#include "stb.h"
#include "sdl.h"
#include "sdl_thread.h"
#include <math.h>

#include "stb_voxel_render.h"
#include "obbg_data.h"
#include "obbg_funcs.h"

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

int view_distance=256;

float table3[128][3];
float table4[64][4];
GLint tablei[2];
GLuint uniform_loc[STBVOX_UNIFORM_count];

void setup_uniforms(float pos[3])
{
   int i,j;
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

            #if 0
            case STBVOX_UNIFORM_texscale:
               compute_texscale();
               data = texscale;
               break;

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

               // fog color
               table4[3][0] = 0.6f, table4[3][1] = 0.7f, table4[3][2] = 0.9f;
               table4[3][3] = 1.0f / 1860.0f;
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


void render_voxel_world(float campos[3])
{
   float x = campos[0], y = campos[1];
   int qchunk_x, qchunk_y;
   int cam_x, cam_y;
   int i,j, rad;

   cam_x = (int) floor(x);
   cam_y = (int) floor(y);

   qchunk_x = MESH_CHUNK_X_FOR_WORLD_X(cam_x);
   qchunk_y = MESH_CHUNK_Y_FOR_WORLD_Y(cam_y);

   glEnable(GL_ALPHA_TEST);
   glAlphaFunc(GL_GREATER, 0.5);

   stbglUseProgram(main_prog);
   setup_uniforms(campos); // set uniforms to default values inefficiently
   glActiveTextureARB(GL_TEXTURE2_ARB);
   stbglEnableVertexAttribArray(0);

   rad = view_distance >> MESH_CHUNK_SIZE_X_LOG2;

   for (j=-rad; j <= rad; ++j) {
      for (i=-rad; i <= rad; ++i) {
         int cx = qchunk_x + i;
         int cy = qchunk_y + j;
         int slot_x = cx & (MESH_CHUNK_CACHE_X-1);
         int slot_y = cy & (MESH_CHUNK_CACHE_Y-1);
         mesh_chunk *mc = &mesh_cache[slot_y][slot_x];
         if (mc->chunk_x != cx || mc->chunk_y != cy || mc->vbuf == 0) {
            mc = build_mesh_chunk_for_coord(cx * MESH_CHUNK_CACHE_X, cy * MESH_CHUNK_CACHE_Y);
         }
         
         if (mc->num_quads) {
            // @TODO if in range, frustum cull
            stbglUniform3fv(stbgl_find_uniform(main_prog, "transform"), 3, mc->transform[0]);
            glBindBufferARB(GL_ARRAY_BUFFER_ARB, mc->vbuf);
            glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 4, (void*) 0);
            glBindTexture(GL_TEXTURE_BUFFER_ARB, mc->fbuf_tex);

            glDrawArrays(GL_QUADS, 0, mc->num_quads*4);
         }
      }
   }

   stbglDisableVertexAttribArray(0);
   glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
   glActiveTextureARB(GL_TEXTURE0_ARB);

   stbglUseProgram(0);
}
