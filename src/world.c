#include "obbg_funcs.h"
#include <math.h>

#define MAX_VEL  150.0f      // blocks per second
#define ACCEL      6.0f
#define DECEL      3.0f

#define STATIC_FRICTION   DECEL
#define EFFECTIVE_ACCEL   (ACCEL+DECEL)

// dynamic friction:
//
//    if going at MAX_VEL, ACCEL and friction must cancel
//    EFFECTIVE_ACCEL = DECEL + DYNAMIC_FRIC*MAX_VEL
#define DYNAMIC_FRICTION  (ACCEL/(float)MAX_VEL)

extern float light_pos[3];
extern float light_vel[3];

float size_for_type[5][2][3] =
{
   { // OTYPE_none
      { 0 }, { 0 },
   },
   { // OTYPE_player
      { - 0.45f, - 0.45f, - 2.25f },
      {   0.45f,   0.45f,   0.25f },
   },
   { // OTYPE_test
      {  -0.25f, -0.25f, -0.15f },
      {   0.25f,  0.25f,  0.15f },
   },
   { // OTYPE_bounce
      {  -0.15f, -0.15f, -0.15f },
      {   0.15f,  0.15f,  0.15f },
   },
   { // OTYPE_critter
      { - 0.45f, - 0.45f, - 0.25f },
      {   0.45f,   0.45f,   2.25f },
   },
};

objid player_id;

void world_init(void)
{
   if (program_mode == MODE_single_player) {
      player_id = allocate_player();

      obj[player_id].position.x = 0;
      obj[player_id].position.y = 0;
      obj[player_id].position.z = 79;
      obj[player_id].type = OTYPE_player;
   }
}

int create_object(int type, vec location)
{
   int id = allocate_object();
   memset(&obj[id], 0, sizeof(obj[id]));
   obj[id].position = location;
   obj[id].valid = 1;
   obj[id].type = type;
   if (type == OTYPE_critter)
      obj[id].brain = allocate_brain();

   return id;
}

float square(float x) { return x*x; }

void objspace_to_worldspace(float world[3], objid oid, float cam_x, float cam_y, float cam_z, float z_ang_off)
{
   float vec[3] = { cam_x, cam_y, cam_z };
   float t[3];
   float s,c;
   s = (float) sin(obj[oid].ang.x*M_PI/180);
   c = (float) cos(obj[oid].ang.x*M_PI/180);

   t[0] = vec[0];
   t[1] = c*vec[1] - s*vec[2];
   t[2] = s*vec[1] + c*vec[2];

   s = (float) sin((z_ang_off+obj[oid].ang.z)*M_PI/180);
   c = (float) cos((z_ang_off+obj[oid].ang.z)*M_PI/180);
   world[0] = c*t[0] - s*t[1];
   world[1] = s*t[0] + c*t[1];
   world[2] = t[2];
}

void objspace_to_worldspace_flat(float world[3], objid oid, float cam_x, float cam_y)
{
   float vec[3] = { cam_x, cam_y, 0 };
   float s,c;

   s = (float) sin((obj[oid].ang.z)*M_PI/180);
   c = (float) cos((obj[oid].ang.z)*M_PI/180);
   world[0] = c*cam_x - s*cam_y;
   world[1] = s*cam_x + c*cam_y;
   world[2] = 0;
}

float pending_view_x;
float pending_view_z = 180+75;

static float view_x_vel = 0;
static float view_z_vel = 0;

void client_view_physics(objid oid, player_controls *con, float dt)
{
   object *o = &obj[oid];
   if (con->flying) {
      #if 1
      o->ang.x = pending_view_x*0.25f;
      o->ang.z = pending_view_z*0.50f;
      #else
      view_x_vel *= (float) pow(0.75, dt);
      view_z_vel *= (float) pow(0.75, dt);

      view_x_vel += (pending_view_x - view_x_vel)*dt*60;
      view_z_vel += (pending_view_z - view_z_vel)*dt*60;

      pending_view_x -= view_x_vel * dt;
      pending_view_z -= view_z_vel * dt;

      o->ang.x += view_x_vel * dt;
      o->ang.z += view_z_vel * dt;
      #endif


      o->ang.x = stb_clamp(o->ang.x, -90, 90);
      o->ang.z = (float) fmod(o->ang.z, 360);
   } else {
      #if 1
      o->ang.x = pending_view_x*0.25f;
      o->ang.z = pending_view_z*0.50f;
      #else
      o->ang.x += pending_view_x * 0.25f;
      o->ang.z += pending_view_z * 0.50f;
      pending_view_x = 0;
      pending_view_z = 0;
      #endif
      o->ang.x = stb_clamp(o->ang.x, -90, 90);
      o->ang.z = (float) fmod(o->ang.z, 360);
   }
}

#define TIME_TO_MOVE_HEAD_UP_AFTER_STEP_UP  0.35f

#define RUN_SPEED 12.0f
#define BACK_RUN_SPEED 12.0f
#define SIDESTEP_SPEED 8.5f

#define ZERO_TO_MAX_SPEED_TIME  0.5f
#define MAX_PLAYER_ACCEL  (RUN_SPEED/ZERO_TO_MAX_SPEED_TIME)
#define MAX_PLAYER_TURN_ACCEL (RUN_SPEED/0.3f)

void player_physics(objid oid, player_controls *con, float dt)
{
   int i;
   object *o = &obj[oid];

   if (o->iz.t) {
      o->iz.t -= dt/TIME_TO_MOVE_HEAD_UP_AFTER_STEP_UP;
      if (o->iz.t < 0)
         o->iz.t = 0;
   }

   // choose direction to apply thrust


   if (!con->flying) {
      vec change;
      float mag, mag2, fastchange_mag;
      float goal_vel[3];
      float forward_speed = (con->buttons & 12)== 4 ?      RUN_SPEED : (con->buttons & 12)== 8 ? -BACK_RUN_SPEED : 0;
      float side_speed    = (con->buttons &  3)== 1 ? SIDESTEP_SPEED : (con->buttons &  3)== 2 ? -SIDESTEP_SPEED : 0;
      objspace_to_worldspace(goal_vel, oid, side_speed, forward_speed, 0,0);

      mag = (float) sqrt(goal_vel[0]*goal_vel[0] + goal_vel[1]*goal_vel[1]);
      if (mag > RUN_SPEED) {
         goal_vel[0] *= RUN_SPEED/mag;
         goal_vel[1] *= RUN_SPEED/mag;
      }

      mag = (float) sqrt(o->velocity.x * o->velocity.x + o->velocity.y * o->velocity.y);
      mag2 = (float) sqrt(goal_vel[0] * goal_vel[0] + goal_vel[1] * goal_vel[1]);

      fastchange_mag = stb_min(mag, mag2);

      if (mag2 != 0) {
         change.x = (goal_vel[0]*fastchange_mag/mag2) - o->velocity.x;
         change.y = (goal_vel[1]*fastchange_mag/mag2) - o->velocity.y;
      } else {
         change.x = goal_vel[0] - o->velocity.x;
         change.y = goal_vel[1] - o->velocity.y;
      }
      change.z = 0;
      mag = (float) sqrt(change.x * change.x + change.y * change.y);
      if (mag > 0) {
         vec schange;
         schange.x = change.x * MAX_PLAYER_TURN_ACCEL/mag*dt;
         schange.y = change.y * MAX_PLAYER_TURN_ACCEL/mag*dt;
         if (fabs(schange.x) >= fabs(change.x))
            schange.x = change.x;
         if (fabs(schange.y) >= fabs(change.y))
            schange.y = change.y;
         o->velocity.x += schange.x;
         o->velocity.y += schange.y;
      }

      change.x = goal_vel[0] - o->velocity.x;
      change.y = goal_vel[1] - o->velocity.y;
      change.z = 0;
      mag = (float) sqrt(change.x * change.x + change.y * change.y);
      if (mag > MAX_PLAYER_ACCEL) {
         change.x *= MAX_PLAYER_ACCEL/mag;
         change.y *= MAX_PLAYER_ACCEL/mag;
      }

      o->velocity.x += change.x * dt;
      o->velocity.y += change.y * dt;

   } else {
      // @TODO clamp thrust[0] & thrust[1] vector length to EFFECTIVE_ACCEL
      float thrust[3] = { 0,0,0 };
      float world_thrust[3];

      thrust[0] = (con->buttons &  3)== 1 ? EFFECTIVE_ACCEL : (con->buttons &  3)== 2 ? -EFFECTIVE_ACCEL : 0;
      thrust[1] = (con->buttons & 12)== 4 ? EFFECTIVE_ACCEL : (con->buttons & 12)== 8 ? -EFFECTIVE_ACCEL : 0;
      thrust[2] = (con->buttons & 48)==16 ? EFFECTIVE_ACCEL : (con->buttons & 48)==32 ? -EFFECTIVE_ACCEL : 0;

      objspace_to_worldspace(world_thrust, oid, thrust[0], thrust[1], 0, 0);
      world_thrust[2] += thrust[2];

      for (i=0; i < 3; ++i) {
         float acc = world_thrust[i];
         (&o->velocity.x)[i] += acc*dt;
      }

      if (o->velocity.x || o->velocity.y || o->velocity.z)
      {
         float vel = (float) sqrt(square(o->velocity.x)+square(o->velocity.y)+square(o->velocity.z));
         float newvel = vel;
         float dec = STATIC_FRICTION + DYNAMIC_FRICTION*vel;
         newvel = vel - dec*dt;
         if (newvel < 0)
            newvel = 0;
         assert(newvel <= vel);
         assert(newvel/vel >= 0);
         assert(newvel/vel <= 1);
         o->velocity.x *= newvel/vel;
         o->velocity.y *= newvel/vel;
         o->velocity.z *= newvel/vel;
      }
   }

   {
      float x,y,z;

      x = o->position.x + o->velocity.x * dt;
      y = o->position.y + o->velocity.y * dt;
      z = o->position.z + o->velocity.z * dt;

      if (!con->flying) {
         if (!physics_move_walkable(&o->position, &o->velocity, dt, size_for_type[o->type], &o->iz))
            o->velocity.z -= GRAVITY_IN_BLOCKS * dt;
      } else {
         o->position.x = x;
         o->position.y = y;
         o->position.z = z;
      }
   }
}

void ai_set_behavior(path_behavior *pb, object *o)
{
   pb->max_step_down = 2;
   pb->estimate_down_cost = 1;
   pb->step_down_cost[1] = 1;
   pb->step_down_cost[2] = 7;

   pb->max_step_up = 1;
   pb->step_up_cost[1] = 1;
   pb->estimate_up_cost = 1;

   pb->size.x = 1;
   pb->size.y = 1;
   pb->size.z = 3;

   pb->flying = False;
}

Bool ai_can_stand(object *o, vec3i target)
{
   path_behavior pb;
   ai_set_behavior(&pb, o);
   return can_stand(&pb, 0,0,0, target);
}


#define MAX_PATH_LEN  1000
static vec3i full_path[MAX_PATH_LEN];
void ai_pathfind(object *o, vec3i target)
{
   int x0,y0,x1,y1;
   int i,j;
   float (*size)[3] = size_for_type[OTYPE_critter];
   path_behavior pb = { 0 };
   vec3i start;
   int len;

   ai_set_behavior(&pb, o);

   x0 = (int) floor(o->position.x + size[0][0]);
   y0 = (int) floor(o->position.y + size[0][1]);
   x1 = (int) floor(o->position.x + size[1][0]);
   y1 = (int) floor(o->position.y + size[1][1]);

   start.z = (int) floor(o->position.z + size_for_type[OTYPE_critter][0][2] + 0.01f);
   for (j=y0; j <= y1; ++j) {
      for (i=x0; i <= x1; ++i) {
         start.x = i;
         start.y = j;
         if (ai_can_stand(o, start))
            goto findpath;
      }
   }

   if (0) {
     findpath:
      o->brain->target = target;
      len = path_find(&pb, start, target, full_path, MAX_PATH_LEN);
      o->velocity.x = 0;
      o->velocity.y = 0;
      o->velocity.z = 0;

      if (len == 0) {
         o->brain->has_target = False;
      } else {
         int i;
         o->brain->path_length = stb_min(len, MAX_SHORT_PATH);
         for (i=0; i < o->brain->path_length; ++i)
            o->brain->path[i] = full_path[len-1 - i];
         o->brain->path_position = 0;
         o->brain->has_target = True;
      }
   }
}

#define CRITTER_SPEED 4

void ai_tick(object *o)
{
   brain_state *b = o->brain;
   if (!o->on_ground)
      return;

   if (b->has_target) {
      vec3i pos, *cur;
      pos.x = (int) floor(o->position.x);
      pos.y = (int) floor(o->position.y);
      pos.z = (int) floor(o->position.z);
      if (b->path_position+1 < b->path_length) {
         cur = &b->path[b->path_position+1];
         if (pos.x != cur->x || pos.y != cur->y) {
            if (o->velocity.x != 0 || o->velocity.y != 0 || o->velocity.z != 0) {
               if (abs(pos.x - cur->x) > 1 || abs(pos.y - cur->y) > 1) {
                  // got off path
                  b->has_target = False;
                  goto refind;
               }
            }
         } else {
            ++b->path_position;
            if (b->path_position == b->path_length-1) {
               if (pos.x == b->target.x && pos.y == b->target.y) {
                  // @TODO: if z mismatches, wait a bit to see if you get there, if not repathfind
                  b->has_target = False;
                  o->velocity.x = 0;
                  o->velocity.y = 0;
                  o->velocity.z = 0;
                  return;
               } else {
                 refind:
                  ai_pathfind(o, b->target);
                  if (b->has_target == False)
                     return;
               }
            }
         }
      }

      if (b->path_position+1 < b->path_length) {
         vec3i next = b->path[b->path_position+1];
         vec delta;
         delta.x = (next.x + 0.5f) - o->position.x;
         delta.y = (next.y + 0.5f) - o->position.y;
         delta.z = (next.z + 0.35f + 0.01f) - o->position.z;
         vec_norm(&delta, &delta);

         o->velocity.x = delta.x * CRITTER_SPEED;
         o->velocity.y = delta.y * CRITTER_SPEED;
         o->velocity.z = delta.z * CRITTER_SPEED;
      }

      assert(b->path_position+1 < b->path_length);
   }
}

float bouncy[OTYPE__count] = { 0,  0, 0,   0.35f, 0 };
void object_physics(objid oid, float dt)
{
   object *o = &obj[oid];
   switch (o->type) {
      case OTYPE_test:
      case OTYPE_bounce:
         o->on_ground = physics_move_inanimate(&o->position, &o->velocity, dt, size_for_type[o->type], o->on_ground, bouncy[o->type]);
         break;
      case OTYPE_critter:
         ai_tick(o);
         o->on_ground = physics_move_animate(&o->position, &o->velocity, dt, size_for_type[o->type], o->on_ground, bouncy[o->type]);
         break;
   }
}

void process_tick_raw(float dt)
{
   int i;

   logistics_tick();

   for (i=1; i < max_player_id; ++i)
      if (obj[i].valid)
         player_physics((objid) i, &p_input[i], dt);

   for (i=PLAYER_OBJECT_MAX; i < max_obj_id; ++i)
      if (obj[i].valid)
         object_physics((objid) i, dt);

   light_pos[0] += light_vel[0] * dt;
   light_pos[1] += light_vel[1] * dt;
   light_pos[2] += light_vel[2] * dt;
}
