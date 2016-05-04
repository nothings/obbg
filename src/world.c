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

static float camera_bounds[2][3] =
{
   { - 0.45f, - 0.45f, - 2.25f },
   {   0.45f,   0.45f,   0.25f },
};


objid player_id;

void world_init(void)
{
   if (program_mode == MODE_single_player) {
      player_id = allocate_player();

      obj[player_id].position.x = 0;
      obj[player_id].position.y = 0;
      obj[player_id].position.z = 80;
   }
}

float square(float x) { return x*x; }

void objspace_to_worldspace(float world[3], objid oid, float cam_x, float cam_y, float cam_z)
{
   float vec[3] = { cam_x, cam_y, cam_z };
   float t[3];
   float s,c;
   s = (float) sin(obj[oid].ang.x*M_PI/180);
   c = (float) cos(obj[oid].ang.x*M_PI/180);

   t[0] = vec[0];
   t[1] = c*vec[1] - s*vec[2];
   t[2] = s*vec[1] + c*vec[2];

   s = (float) sin(obj[oid].ang.z*M_PI/180);
   c = (float) cos(obj[oid].ang.z*M_PI/180);
   world[0] = c*t[0] - s*t[1];
   world[1] = s*t[0] + c*t[1];
   world[2] = t[2];
}

float pending_view_x;
float pending_view_z;

static float view_x_vel = 0;
static float view_z_vel = 0;

void client_view_physics(objid oid, player_controls *con, float dt)
{
   object *o = &obj[oid];
   if (con->flying) {
      view_x_vel *= (float) pow(0.75, dt);
      view_z_vel *= (float) pow(0.75, dt);

      view_x_vel += (pending_view_x - view_x_vel)*dt*60;
      view_z_vel += (pending_view_z - view_z_vel)*dt*60;

      pending_view_x -= view_x_vel * dt;
      pending_view_z -= view_z_vel * dt;

      o->ang.x += view_x_vel * dt;
      o->ang.z += view_z_vel * dt;
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
      o->ang.x = stb_clamp(o->ang.x, -90, 90);
      o->ang.z = (float) fmod(o->ang.z, 360);
      #endif
   }
}

void player_physics(objid oid, player_controls *con, float dt)
{
   int i;
   object *o = &obj[oid];
   float thrust[3] = { 0,0,0 };
   float world_thrust[3];

   // choose direction to apply thrust

   thrust[0] = (con->buttons &  3)== 1 ? EFFECTIVE_ACCEL : (con->buttons &  3)== 2 ? -EFFECTIVE_ACCEL : 0;
   thrust[1] = (con->buttons & 12)== 4 ? EFFECTIVE_ACCEL : (con->buttons & 12)== 8 ? -EFFECTIVE_ACCEL : 0;
   thrust[2] = (con->buttons & 48)==16 ? EFFECTIVE_ACCEL : (con->buttons & 48)==32 ? -EFFECTIVE_ACCEL : 0;

   // @TODO clamp thrust[0] & thrust[1] vector length to EFFECTIVE_ACCEL

   objspace_to_worldspace(world_thrust, oid, thrust[0], thrust[1], 0);
   world_thrust[2] += thrust[2];

   if (!con->flying)
      world_thrust[2] = 0;

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

   {
      float x,y,z;

      x = o->position.x + o->velocity.x * dt;
      y = o->position.y + o->velocity.y * dt;
      z = o->position.z + o->velocity.z * dt;

      if (!con->flying) {
         if (!physics_move_walkable(&o->position, &o->velocity, dt, camera_bounds))
            o->velocity.z -= 20.0f * dt;
      } else {
         o->position.x = x;
         o->position.y = y;
         o->position.z = z;
      }

      #if 0
      if (!collision_test_box(x,y,z,camera_bounds)) {
         camloc[0] = x;
         camloc[1] = y;
         camloc[2] = z;
      } else {
         cam_vel[0] = 0;
         cam_vel[1] = 0;
         cam_vel[2] = 0;
      }
      #endif
   }
}

void process_tick_raw(float dt)
{
   int i;

   for (i=1; i < max_player_id; ++i)
      if (obj[i].valid)
         player_physics((objid) i, &p_input[i], dt);

   light_pos[0] += light_vel[0] * dt;
   light_pos[1] += light_vel[1] * dt;
   light_pos[2] += light_vel[2] * dt;
}
