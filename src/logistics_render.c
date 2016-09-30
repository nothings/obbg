#include "obbg_funcs.h"
#include "stb_gl.h"
#include <math.h>

#pragma warning(disable:4244)

static int face_orig[4][2] = {
   { 0,0 },
   { 1,0 },
   { 1,1 },
   { 0,1 },
};


static void draw_one_picker(int x, int y, int z, int rot, float pos, float drop_on_pickup)
{
   float bone_state[4];
   vec base = { 0.35f,0.35f,0.35f };
   float len;

   bone_state[1] = stb_lerp(pos, 0.5, -0.75) - base.x;
   bone_state[2] = 0 - base.y;
   bone_state[3] = stb_lerp(pos, 0.30, 0.50) - base.z + drop_on_pickup;

   len = sqrt(bone_state[1]*bone_state[1] + bone_state[2]*bone_state[2])/2;
   len = sqrt(1*1 - len);
   bone_state[0] = len - base.z;

   add_draw_picker(x+0.5, y+0.5, z, rot, bone_state);
}

static void draw_balancer(int x, int y, int z, int rot, float alpha)
{
   glColor4f(1,1,1,alpha);
   if (alpha <= 1) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   } else {
      glDisable(GL_BLEND);
   }
   glPushMatrix();
   glTranslatef(x+0.5, y+0.5, z);
   glRotatef(90*rot, 0,0,1);
   stbgl_drawBox(0,0,0.75, 1,1,0.5, 1);
   glPopMatrix();
   glDisable(GL_BLEND);
}

void logistics_render_from_copy(render_logi_chunk **arr, float offset)
{
   //float offset = (float) logistics_long_tick / LONG_TICK_LENGTH;// + stb_frand();
   int i,a,e;
   float belt_zoff= -0.5;
   assert(offset >= 0 && offset <= 1);
   for (i=0; i < obarr_len(arr); ++i) {
      render_logi_chunk *c = arr[i];
      if (c != NULL) {
         float base_x = (float) c->slice_x * LOGI_CHUNK_SIZE_X;
         float base_y = (float) c->slice_y * LOGI_CHUNK_SIZE_Y;
         float base_z = (float) c->chunk_z * LOGI_CHUNK_SIZE_Z;
         glMatrixMode(GL_MODELVIEW);
         glDisable(GL_TEXTURE_2D);
         glColor3f(1,1,1);
         for (a=0; a < c->num_machines; ++a) {
            render_machine_info *m = &c->machine[a];
            switch (m->type) {
               case BT_ore_drill:
               case BT_ore_eater:
               case BT_furnace:
               case BT_iron_gear_maker:
               case BT_conveyor_belt_maker:
                  break;
               default:
                  glPushMatrix();
                  glTranslatef(base_x+m->pos.unpacked.x+0.5, base_y+m->pos.unpacked.y+0.5, base_z+m->pos.unpacked.z);
                  glRotatef(90*m->rot, 0,0,1);
                  stbgl_drawBox(0,0,0.25, 1,1,0.5, 1);
                  glPopMatrix();
                  break;
            }
         }
          
         for (a=0; a < c->num_belt_machines; ++a) {
            render_belt_machine_info *m = &c->belt_machine[a];
            switch (m->type) {
               case BT_balancer:
                  draw_balancer(base_x+m->pos.unpacked.x,base_y+m->pos.unpacked.y,base_z+m->pos.unpacked.z, m->rot, 1.0);
                  break;
               default:
                  glPushMatrix();
                  glTranslatef(base_x+m->pos.unpacked.x+0.5, base_y+m->pos.unpacked.y+0.5, base_z+m->pos.unpacked.z);
                  glRotatef(90*m->rot, 0,0,1);
                  stbgl_drawBox(0,0,0.75, 1,1,0.5, 1);
                  glPopMatrix();
                  break;
            }
         }

         for (a=0; a < c->num_pickers; ++a) {
            int b = 0;
            render_picker_info *pi = &c->pickers[a];
            float pos=0;
            float bone_state[4]= {0,0,0,0};
            int state = pi->state;
            int rot = pi->rot;
            float drop_on_pickup;
            

            if (pi->input_is_belt) {
               state = state^2;
               rot = rot^2;
            }

            // state = 0 -> immobile at pickup
            // state = 1 -> animating towards pickup
            // state = 2 -> immobile at dropoff
            // state = 3 -> animating towards dropoff

            if (state == 0) pos = 0;
            if (state == 1) pos = 1-offset - 1.0/LONG_TICK_LENGTH;
            if (state == 2) pos = 1;
            if (state == 3) pos = offset;

            if ((state == 1 || state == 3) && offset < 0.125)
               drop_on_pickup = stb_linear_remap(offset, 0,0.125, -0.15, 0);
            else
               drop_on_pickup = 0.0f;

            #if 1
            {
               vec base = { 0.35f,0.35f,0.35f };
               float len;

               bone_state[1] = stb_lerp(pos, 0.5, -0.75) - base.x;
               bone_state[2] = 0 - base.y;
               bone_state[3] = stb_lerp(pos, 0.30, 0.50) - base.z + drop_on_pickup;

               len = sqrt(bone_state[1]*bone_state[1] + bone_state[2]*bone_state[2])/2;
               len = sqrt(1*1 - len);
               bone_state[0] = len - base.z;
            }
            //add_draw_picker(base_x+pi->pos.unpacked.x+0.5, base_y+pi->pos.unpacked.y+0.5, base_z+pi->pos.unpacked.z,
//                                     rot, pos, drop_on_pickup);

            draw_one_picker(base_x+pi->pos.unpacked.x, base_y+pi->pos.unpacked.y, base_z+pi->pos.unpacked.z,
                            rot, pos, drop_on_pickup);

            //0,0.25,0.20

            #else
            bone_state[0] = (state >= 2 ? 0.06f : 0.08f);
            bone_state[1] = stb_lerp(pos, 0.75, -0.75);
            bone_state[3] = 0.05f + drop_on_pickup;
            //bone_state[3] = (pi->state >= 2 ? 0.05f : -0.05f);
            #endif


            #if 0
            for (b=1; b < 500; ++b) {
               bone_state[0] = fmod(b*0.237,0.5);
               add_draw_picker(base_x+pi->pos.unpacked.x+0.5, base_y+pi->pos.unpacked.y+0.5, base_z+pi->pos.unpacked.z+b,
                               pi->rot, bone_state);
            }
            #endif

            #if 0
            glPushMatrix();
            glTranslatef();
            glRotatef(90*pi->rot, 0,0,1);
            stbgl_drawBox(0,0,0.5, 1.5,0.125,0.125, 1);
            stbgl_drawBox(stb_lerp(pos, 0.75, -0.75),0,0.5-0.125, 0.25,0.25,0.125, 1);
            glPopMatrix();
            #endif

            {
               float mrot[4][2][2] = {
                  {{ 1,0,},{0,1}},
                  {{ 0,-1,},{1,0}},
                  {{-1,0,},{0,-1}},
                  {{ 0,1,},{-1,0}},
               };
               float x = stb_lerp(pi->input_is_belt ? 1-pos : pos, 0.5, -0.5);
               float y = 0;
               float az = 0.25;
               float ax,ay;
               ax = mrot[pi->rot][0][0]*x + mrot[pi->rot][0][1]*y;
               ay = mrot[pi->rot][1][0]*x + mrot[pi->rot][1][1]*y;
               ax += base_x+pi->pos.unpacked.x+0.5;
               ay += base_y+pi->pos.unpacked.y+0.5;
               az += base_z+pi->pos.unpacked.z+0.175;
               if (pi->item != 0)
                  add_sprite(ax, ay, az, pi->item);
            }
         }
         for (a=0; a < c->num_belts; ++a) {
            render_belt_run *b = &c->belts[a];
            if (b->turn == 0) {
               float z = c->chunk_z * LOGI_CHUNK_SIZE_Z + 1.0f + b->pos.unpacked.z + belt_zoff;
               float x1 = (float) c->slice_x * LOGI_CHUNK_SIZE_X + b->pos.unpacked.x;
               float y1 = (float) c->slice_y * LOGI_CHUNK_SIZE_Y + b->pos.unpacked.y;
               float x2,y2;
               int len = b->len * ITEMS_PER_BELT_SIDE;
               int d0 = b->dir;
               int d1 = (d0 + 1) & 3;

               if (b->end_dz > 0)
                  z += 0.125/2;
               else if (b->end_dz < 0)
                  z -= 0.125/4;

               x1 += face_orig[b->dir][0];
               y1 += face_orig[b->dir][1];

               x1 += face_dir[d0][0]*(1.0/ITEMS_PER_BELT_SIDE)*0.5;
               y1 += face_dir[d0][1]*(1.0/ITEMS_PER_BELT_SIDE)*0.5;

               x2 = x1 + face_dir[d1][0]*(0.9f - 0.5/ITEMS_PER_BELT_SIDE);
               x1 = x1 + face_dir[d1][0]*(0.1f + 0.5/ITEMS_PER_BELT_SIDE);

               y2 = y1 + face_dir[d1][1]*(0.9f - 0.5/ITEMS_PER_BELT_SIDE);
               y1 = y1 + face_dir[d1][1]*(0.1f + 0.5/ITEMS_PER_BELT_SIDE);

               for (e=0; e < len; ++e) {
                  float ax,ay,az;
                  if (b->items[e+0] != 0) {
                     ax = x1, ay = y1, az=z;
                     if (IS_ITEM_MOBILE(b,e,RIGHT)) {
                        ax += face_dir[d0][0]*(1.0/ITEMS_PER_BELT_SIDE) * offset;
                        ay += face_dir[d0][1]*(1.0/ITEMS_PER_BELT_SIDE) * offset;
                        az += b->end_dz / 2.0 * (1.0 / ITEMS_PER_BELT_SIDE) * offset;
                     }
                     add_sprite(ax, ay, az, b->items[e+0]);

                  }
                  if (b->items[e+len] != 0) {
                     ax = x2, ay = y2, az=z;
                     if (IS_ITEM_MOBILE(b,e,LEFT)) {
                        ax += face_dir[d0][0]*(1.0/ITEMS_PER_BELT_SIDE) * offset;
                        ay += face_dir[d0][1]*(1.0/ITEMS_PER_BELT_SIDE) * offset;
                        az += b->end_dz / 2.0 * (1.0 / ITEMS_PER_BELT_SIDE) * offset;
                     }
                     add_sprite(ax, ay, az, b->items[e+len]);
                  }
                  x1 += face_dir[d0][0]*(1.0/ITEMS_PER_BELT_SIDE);
                  y1 += face_dir[d0][1]*(1.0/ITEMS_PER_BELT_SIDE);
                  x2 += face_dir[d0][0]*(1.0/ITEMS_PER_BELT_SIDE);
                  y2 += face_dir[d0][1]*(1.0/ITEMS_PER_BELT_SIDE);
                  z += b->end_dz / 2.0 * (1.0 / ITEMS_PER_BELT_SIDE);
               }
            } else {
               float z = c->chunk_z * LOGI_CHUNK_SIZE_Z + 1.0f + b->pos.unpacked.z + belt_zoff;
               float x1 = (float) c->slice_x * LOGI_CHUNK_SIZE_X + b->pos.unpacked.x;
               float y1 = (float) c->slice_y * LOGI_CHUNK_SIZE_Y + b->pos.unpacked.y;
               float x2,y2;
               float ox,oy;
               int d0 = b->dir;
               int d1 = (d0 + 1) & 3;
               int left_len = b->turn > 0 ? SHORT_SIDE : LONG_SIDE;
               int right_len = SHORT_SIDE+LONG_SIDE - left_len;

               x1 += face_orig[b->dir][0];
               y1 += face_orig[b->dir][1];

               ox = x1;
               oy = y1;
               if (b->turn > 0) {
                  ox += face_dir[(b->dir+1)&3][0];
                  oy += face_dir[(b->dir+1)&3][1];
               }

               x1 += face_dir[d0][0]*(1.0/ITEMS_PER_BELT_SIDE)*0.5;
               y1 += face_dir[d0][1]*(1.0/ITEMS_PER_BELT_SIDE)*0.5;

               x2 = x1 + face_dir[d1][0]*(0.9f - 0.5/ITEMS_PER_BELT_SIDE);
               y2 = y1 + face_dir[d1][1]*(0.9f - 0.5/ITEMS_PER_BELT_SIDE);

               x1 = x1 + face_dir[d1][0]*(0.1f + 0.5/ITEMS_PER_BELT_SIDE);
               y1 = y1 + face_dir[d1][1]*(0.1f + 0.5/ITEMS_PER_BELT_SIDE);

               for (e=0; e < right_len; ++e) {
                  if (b->items[0+e] != 0) {
                     float bx,by,ax,ay;
                     float ang = e, s,c;
                     if (IS_ITEM_MOBILE(b,e,RIGHT))
                        ang += offset;
                     ang = (ang / right_len) * 3.141592/2;
                     if (b->turn > 0) ang = -ang;
                     ax = x1-ox;
                     ay = y1-oy;
                     s = sin(ang);
                     c = cos(ang);
                     bx = c*ax + s*ay;
                     by = -s*ax + c*ay;
                     add_sprite(ox+bx, oy+by, z, b->items[0+e]);
                  }
               }
               for (e=0; e < left_len; ++e) {
                  if (b->items[right_len+e] != 0) {
                     float bx,by,ax,ay;
                     float ang = e, s,c;
                     if (IS_ITEM_MOBILE(b,e,LEFT))
                        ang += offset;
                     ang = (ang / left_len) * 3.141592/2;
                     if (b->turn > 0) ang = -ang;
                     ax = x2-ox;
                     ay = y2-oy;
                     s = sin(ang);
                     c = cos(ang);
                     bx = c*ax + s*ay;
                     by = -s*ax + c*ay;
                     add_sprite(ox+bx, oy+by, z, b->items[right_len+e]);
                  }
               }
            }
         }
      }
   }
}

#if 0
void logistics_debug_render(void)
{
   int i,j,k,a;
   for (j=0; j < LOGI_CACHE_SIZE; ++j) {
      for (i=0; i < LOGI_CACHE_SIZE; ++i) {
         logi_slice *s = &logi_world[j][i];
         if (s->slice_x != i+1) {
            float x,y;
            for (k=0; k < stb_arrcount(s->chunk); ++k) {
               logi_chunk *c = s->chunk[k];
               if (c != NULL) {
                  int base_x = s->slice_x * LOGI_CHUNK_SIZE_X;
                  int base_y = s->slice_y * LOGI_CHUNK_SIZE_Y;
                  int base_z = k * LOGI_CHUNK_SIZE_Z;
                  for (a=0; a < obarr_len(c->belts); ++a) {
                     belt_run *b = &c->belts[a];
                     int d0 = b->dir;
                     int d1 = (d0 + 1) & 3;
                     float z = k * LOGI_CHUNK_SIZE_Z + 1.1f + b->pos.unpacked.z;

                     x = (float) base_x + b->pos.unpacked.x;
                     y = (float) base_y + b->pos.unpacked.y;
                     x += face_orig[b->dir][0];
                     y += face_orig[b->dir][1];

                     //  +------+
                     //  |      |
                     //  |      |
                     //  +------+
                     // (0,0)

                     glBegin(GL_LINE_LOOP);
                        glColor3f(0.75,0,0);
                        glVertex3f(x                , y                , z);
                        glVertex3f(x+face_dir[d1][0], y+face_dir[d1][1], z);
                        glColor3f(0.75,0.75,0);
                        glVertex3f(x+face_dir[d1][0]+face_dir[d0][0]*b->len, y+face_dir[d1][1]+face_dir[d0][1]*b->len, z);
                        glVertex3f(x                +face_dir[d0][0]*b->len, y                +face_dir[d0][1]*b->len, z);
                     glEnd();

                     if (b->target_id != TARGET_none) {
                        int ex = base_x + b->pos.unpacked.x + b->len * face_dir[b->dir][0];
                        int ey = base_y + b->pos.unpacked.y + b->len * face_dir[b->dir][1];
                        int ez = base_z + b->pos.unpacked.z, is_neighbor=0;
                        logi_chunk *c = logistics_get_chunk(ex,ey,ez,0);
                        assert(c != NULL);
                        if (c) {
                           float tx,ty,tz;
                           belt_run *t = &c->belts[b->target_id];
                           tx = (float) (ex & ~(LOGI_CHUNK_SIZE_X-1)) + t->x_off + 0.5;
                           ty = (float) (ey & ~(LOGI_CHUNK_SIZE_Y-1)) + t->y_off + 0.5;
                           tz = (float) (ez & ~(LOGI_CHUNK_SIZE_Z-1)) + t->z_off + 1.2f;
                           //tx = ex+0.5, ty = ey+0.5, tz = ez+1.2f;
                           glBegin(GL_LINES);
                           glVertex3f(x-face_orig[b->dir][0]+0.5+face_dir[d0][0]*(b->len-1),
                                      y-face_orig[b->dir][1]+0.5+face_dir[d0][1]*(b->len-1),
                                      z+0.1f);
                           glVertex3f(tx,ty,tz);
                           glEnd();
                        }
                     }
                  }
               }
            }
         }
      }
   }


   if (0) {
      vec pos = obj[player_id].position;
      int x,y,z;
      x = (int) floor(pos.x);
      y = (int) floor(pos.y);
      z = (int) floor(pos.z);

      x &= ~(LOGI_CHUNK_SIZE_X-1);
      y &= ~(LOGI_CHUNK_SIZE_Y-1);
      z &= ~(LOGI_CHUNK_SIZE_Z-1);

      glDisable(GL_TEXTURE_2D);
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      glColor4f(0.2f,0.2f,0.2f,0.2f);
      glDisable(GL_CULL_FACE);
      stbgl_drawBox(x+LOGI_CHUNK_SIZE_X/2, y+LOGI_CHUNK_SIZE_Y/2, z+LOGI_CHUNK_SIZE_Z/2, LOGI_CHUNK_SIZE_X-0.125f, LOGI_CHUNK_SIZE_Y-0.125f, LOGI_CHUNK_SIZE_Z-0.125f, 0);
      glEnable(GL_TEXTURE_2D);
      glDisable(GL_BLEND);
   }
}
#endif

Bool logistics_draw_block(int x, int y, int z, int blocktype, int rot)
{
   switch (blocktype) {
      case BT_picker:
         draw_one_picker(x,y,z,rot, 0.75, 0.0);
         break;
      case BT_balancer:
         draw_balancer(x,y,z,rot, 0.4f);
         break;
   }
   return True;
}
