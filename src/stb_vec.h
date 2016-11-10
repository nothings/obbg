// stb_vec.h - public domain C vector library, primarily 3-float-vectors
#ifndef INCLUDE_STB_VEC_H
#define INCLUDE_STB_VEC_H

typedef struct { float x,y,z  ; }  vec;
typedef struct { float x,y,z,w; }  vec4;
typedef struct { short s,t    ; }  vec2s;
typedef struct { float x,y    ; }  vec2;
typedef struct { vec m[3]     ; }  mat3;
typedef struct { vec4 m[4]    ; }  mat4;
typedef struct { float x,y,z,w; }  quat;

typedef float stb_vec_f3[3];
typedef float stb_vec_f4[4];

#ifdef __cplusplus
extern "C" {
#endif

extern vec vec_zero(void);
extern vec vec3(float x, float y, float z);
extern vec vec3v(float *x);
extern void vec_add(vec *d, vec *v0, vec *v1);
extern void vec_addeq(vec *d, vec *v0);
extern void vec_sub(vec *d, vec *v0, vec *v1);
extern void vec_subeq(vec *d, vec *v0);
extern void vec_scale(vec *d, vec *v, float scale);
extern void vec_scaleeq(vec *vec, float scale);

extern void vec_add_scale(vec *dest, vec *v0, vec *v1, float scale);
extern void vec_addeq_scale(vec *dest, vec *src, float scale);
extern void vec_sub_scale(vec *dest, vec *v0, vec *v1, float scale);
extern void vec_subeq_scale(vec *dest, vec *src, float scale);
extern void vec_lerp(vec *dest, vec *a, vec *b, float weight);
extern void vec4_lerp(vec4 *dest, vec4 *a, vec4 *b, float weight);
extern void vec_average(vec *dest, vec *p0, vec *p1);

extern void vec_cross(vec *cross, vec *v0, vec *v1);
extern float vec_dot(vec *v0, vec*v1);
extern float vec_mag2(vec *v);
extern float vec_mag(vec *v);
extern float vec_one_over_mag(vec *v);
extern float vec_dist2(vec *v0, vec *v1);
extern float vec_dist(vec *v0, vec *v1);
extern float vec_norm(vec *dest, vec *src);
extern float vec_normeq(vec *v);
extern int vec_eq(vec *a, vec *b);

extern vec vec_face_normal(vec *verts, int p0, int p1, int p2);
extern void vec_Yup_to_Zup(vec *v);

extern void mat4_identity(mat4 *m);
extern void mat3_identity(mat3 *m);
extern void float44_identity(float m[4][4]);
extern void float33_identity(float m[3][3]);

extern void mat4_mul(mat4 *out, mat4 *m1, mat4 *m2);
extern void mat3_mul(mat3 *out, mat3 *m1, mat3 *m2);
extern void mat3_mul_t(mat3 *out, mat3 *m1, mat3 *m2);
extern void mat3_orthonormalize(mat3 *m);

extern void mat4_vec_mul(vec *out, mat4 *m, vec *in);
extern void mat3_vec_mul(vec *out, mat3 *m, vec *in);
extern void mat3_vec_mul_t(vec *out, mat3 *m, vec *in);

extern void mat3_rotation_around_axis(mat3 *m, int axis, float ang);
extern void mat3_rotation_around_vec(mat3 *m, vec *v, float ang);

extern void vec_rotate_x(vec *dest, vec *src, float ang);
extern void vec_rotate_y(vec *dest, vec *src, float ang);
extern void vec_rotate_z(vec *dest, vec *src, float ang);
extern void vec_rotate_euler_zup_facing_y(vec *dest, vec *src, float x, float y, float z);

extern void quat_lerp(quat *dest, quat *q1, quat *q2, float t);
extern void quat_lerp_normalize(quat *dest, quat *q1, quat *q2, float t);
extern void quat_lerp_neighbor_normalize(quat *dest, quat *q1, quat *q2, float t);
extern void quat_rotation_around_axis(quat *dest, vec *axis, float ang);
extern float quat_get_rotation(vec *axis, quat *src);
extern void quat_from_mat3(quat *dest, mat3 *mat);
extern void mat3_from_quat(mat3 *mat, quat *dest);
extern void quat_mul(quat *dest, quat *q1, quat *q2);
extern void quat_vec_mul(vec *dest, quat *q, vec *v);
extern void quat_normalize(quat *q);
extern void quat_scale_addeq(quat *dest, quat *q, float sc);
extern void quat_identity(quat *q);
extern void quat_invert(quat *q);

extern void float16_transposeeq(float *m);
extern void float16_transpose(float *d, float *m);

extern mat4 *  mat4_v(vec4 m[4]);
extern mat4 *  mat4_f(float m[4][4]);
extern stb_vec_f4 * float44_m(mat4 *m);
extern stb_vec_f4 * float44_v(vec4 m[4]);
extern stb_vec_f4 * float44_16(float m[16]);

extern mat3 *  mat3_v(vec m[3]);
extern mat3 *  mat3_f(float m[3][3]);
extern stb_vec_f3 * float33_m(mat3 *m);
extern stb_vec_f3 * float33_v(vec m[3]);

extern void float44_transpose(float (*vec)[4]);
extern void float44_mul(float (*out)[4], float (*mat1)[4], float (*mat2)[4]);
extern void float44_mul_t(float (*out)[4], float (*mat1)[4], float (*mat2)[4]);
extern void float33_mul(float out[3][3], float mat1[3][3], float mat2[3][3]);
extern void float33_mul_t(float out[3][3], float mat1[3][3], float mat2[3][3]);
extern void float33_orthonormalize(float m[3][3]);
extern void float33_transpose(float (*vec)[3]);

extern void float33_vec_mul(vec *out, float m[3][3], vec *in);
extern void float33_vec_mul_t(vec *out, float m[3][3], vec *in);

extern void float33_rotation_around_x(float mat[3][3], float ang);
extern void float33_rotation_around_y(float mat[3][3], float ang);
extern void float33_rotation_around_z(float mat[3][3], float ang);
extern void float33_rotation_around_axis(float mat[3][3], int axis, float ang);
extern void float33_rotation_around_vec(float mat[3][3], vec *v, float ang);

extern void quat_from_float33(quat *dest, float mat[3][3]);
extern void quat_from_float44(quat *dest, float mat[4][4]);
extern void float33_from_quat(float mat[3][3], quat *q);
extern void float44_from_quat(float mat[4][4], quat *q);
extern void float44_from_quat_vec(float mat[4][4], quat *rot, vec *trans);

#ifdef __cplusplus
}
#endif

#ifdef STB_DEFINE
vec vec_zero(void)
{
   vec v = { 0,0,0 };
   return v;
}

vec vec3(float x, float y, float z)
{
   vec v = { x,y,z };
   return v;
}

vec vec3f(float *p)
{
   vec v = { p[0], p[1], p[2] };
   return v;
}

#define STBVEC_2OP(a,c,post) \
   a->x =         c->x post; \
   a->y =         c->y post; \
   a->z =         c->z post;

#define STBVEC_3OP(a,b,op,c,post) \
   a->x = b->x op c->x post; \
   a->y = b->y op c->y post; \
   a->z = b->z op c->z post;

void vec_add(vec *d, vec *v0, vec *v1)  { STBVEC_3OP(d,v0,+,v1,+0) }
void vec_addeq(vec *d, vec *v0)         { STBVEC_3OP(d, d,+,v0,+0) }
void vec_sub(vec *d, vec *v0, vec *v1)  { STBVEC_3OP(d,v0,-,v1,+0) }
void vec_subeq(vec *d, vec *v0)         { STBVEC_3OP(d, d,-,v0,+0) }
void vec_scale(vec *d, vec *v, float s) { STBVEC_2OP(d,     v ,*s) }
void vec_scaleeq(vec *vec, float s)     { STBVEC_2OP(vec,  vec,*s) }
void vec_add_scale(vec *d, vec *v0, vec *v1, float s) { STBVEC_3OP(d,v0,+,v1,*s) }
void vec_addeq_scale(vec *d, vec *v0, float s)        { STBVEC_3OP(d, d,+,v0,*s) }
void vec_sub_scale(vec *d, vec *v0, vec *v1, float s) { STBVEC_3OP(d,v0,-,v1,*s) }
void vec_subeq_scale(vec *d, vec *v0, float s)        { STBVEC_3OP(d, d,-,v0,*s) }
void vec_average(vec *d, vec *v0, vec *v1)            { vec_lerp(d,v0,v1,0.5f); }

void vec_lerp(vec *d, vec *a, vec *b, float t)
{
   d->x = a->x + t * (b->x - a->x);
   d->y = a->y + t * (b->y - a->y);
   d->z = a->z + t * (b->z - a->z);
}

void vec4_lerp(vec4 *d, vec4 *a, vec4 *b, float t)
{
   vec_lerp((vec *) d, (vec *) a, (vec *) b, t);
   d->w = a->w + t * (b->w - a->w);
}

void vec_cross(vec *cross, vec *v0, vec *v1)
{
   cross->x = v0->y * v1->z - v0->z * v1->y;
   cross->y = v0->z * v1->x - v0->x * v1->z;
   cross->z = v0->x * v1->y - v0->y * v1->x; // right hand rule: i x j = k
}

float vec_dot(vec *v0, vec *v1)
{
   float temp = v0->x * v1->x + v0->y*v1->y + v0->z*v1->z;
   return temp;
}

float vec_mag2(vec *v)
{
   return v->x*v->x + v->y*v->y + v->z*v->z;
}

float vec_dist2(vec *v0, vec *v1)
{
   vec d;
   d.x = v0->x - v1->x;
   d.y = v0->y - v1->y;
   d.z = v0->z - v1->z;
   return d.x*d.x + d.y*d.y + d.z*d.z;
}

float vec_dist(vec *v0, vec *v1)
{
   return (float) sqrt(vec_dist2(v0,v1));
}

float vec_mag(vec *v)
{
   return (float) sqrt(vec_mag2(v));
}

float vec_one_over_mag(vec *v)
{
   return 1/(float) sqrt(vec_mag2(v));
}

float vec_norm(vec *dest, vec *src)
{
   float mag = vec_mag(src);
   vec_scale(dest, src, 1.0f / mag);
   return mag;
}

float vec_normeq(vec *v)
{
   float mag = vec_mag(v);
   vec_scaleeq(v, 1.0f / mag);
   return mag;
}

vec vec_face_normal(vec *verts, int p0, int p1, int p2)
{
   vec n,v0,v1;
   vec_sub(&v0, verts+p2, verts+p0);
   vec_sub(&v1, verts+p1, verts+p0);
   vec_cross(&n, &v0, &v1);
   return n;
}

void vec_Yup_to_Zup(vec *v)
{
   float z = v->z;
   v->z = v->y;
   v->y = -z;
}

// matrix operations are easiest to write with C arrays (e.g.
// can use loops), so those are implemented as the base case and
// the others work by converting (casting) to them)

mat4 *  mat4_v   (vec4 m[4])     { return (mat4  *) m; }
mat4 *  mat4_f   (float m[4][4]) { return (mat4  *) m; }
stb_vec_f4* float44_m(mat4 *m)   { return (stb_vec_f4 *) m; }
stb_vec_f4* float44_v(vec4 m[4]) { return (stb_vec_f4 *) m; }
stb_vec_f4 * float44_16(float m[16]) { return (stb_vec_f4 *) m; }

mat3 *  mat3_v   (vec m[3])      { return (mat3  *) m; }
mat3 *  mat3_f   (float m[3][3]) { return (mat3  *) m; }
stb_vec_f3* float33_m(mat3 *m)   { return (stb_vec_f3 *) m; }
stb_vec_f3* float33_v(vec m[3])  { return (stb_vec_f3 *) m; }

void mat4_identity(mat4 *m) { float44_identity(float44_m(m)); }
void mat3_identity(mat3 *m) { float33_identity(float33_m(m)); }

void float44_identity(float m[4][4])
{
   m[0][0] = 1; m[0][1] = 0; m[0][2] = 0; m[0][3] = 0;
   m[1][0] = 0; m[1][1] = 1; m[1][2] = 0; m[1][3] = 0;
   m[2][0] = 0; m[2][1] = 0; m[2][2] = 1; m[2][3] = 0;
   m[3][0] = 0; m[3][1] = 0; m[3][2] = 0; m[3][3] = 1;
}

void float33_identity(float m[3][3])
{
   m[0][0] = 1; m[0][1] = 0; m[0][2] = 0;
   m[1][0] = 0; m[1][1] = 1; m[1][2] = 0;
   m[2][0] = 0; m[2][1] = 0; m[2][2] = 1;
}

void float33_vec_mul(vec *out, float mat[3][3], vec *v)
{
   vec in;
   if (v == out) { in = *v; v = &in; } // copy if it's in-place
   out->x = mat[0][0] * v->x + mat[0][1] * v->y + mat[0][2] * v->z;
   out->y = mat[1][0] * v->x + mat[1][1] * v->y + mat[1][2] * v->z;
   out->z = mat[2][0] * v->x + mat[2][1] * v->y + mat[2][2] * v->z;
}

void float33_vec_mul_t(vec *out, float mat[3][3], vec *v)
{
   vec in;
   if (v == out) { in = *v; v = &in; } // copy if it's in-place
   out->x = mat[0][0] * v->x + mat[1][0] * v->y + mat[2][0] * v->z;
   out->y = mat[0][1] * v->x + mat[1][1] * v->y + mat[2][1] * v->z;
   out->z = mat[0][2] * v->x + mat[1][2] * v->y + mat[2][2] * v->z;
}

void float44_vec_mul(vec *out, float mat[4][4], vec *v)
{
   vec in;
   if (v == out) { in = *v; v = &in; } // copy if it's in-place
   out->x = mat[0][0] * v->x + mat[0][1] * v->y + mat[0][2] * v->z + mat[0][3];
   out->y = mat[1][0] * v->x + mat[1][1] * v->y + mat[1][2] * v->z + mat[1][3];
   out->z = mat[2][0] * v->x + mat[2][1] * v->y + mat[2][2] * v->z + mat[2][3];
}

void float44_transpose(float (*vec)[4])
{
   int i,j;
   for (j=0; j < 4; ++j)
      for (i=j+1; i < 4; ++i) {
         float t = vec[i][j];
         vec[i][j] = vec[j][i];
         vec[j][i] = t;
      }
}

void float33_transpose(float (*vec)[3])
{
   int i,j;
   for (j=0; j < 3; ++j)
      for (i=j+1; i < 3; ++i) {
         float t = vec[i][j];
         vec[i][j] = vec[j][i];
         vec[j][i] = t;
      }
}

void float16_transposeeq(float *m)
{
   int i,j;
   for (j=0; j < 4; ++j)
      for (i=j+1; i < 4; ++i) {
         float t = m[i*4+j];
         m[i*4+j] = m[j*4+i];
         m[j*4+i] = t;
      }
}

void float16_transpose(float *d, float *m)
{
   int i,j;
   for (j=0; j < 4; ++j)
      for (i=0; i < 4; ++i)
         d[i*4+j] = m[j*4+i];
}

void float44_mul(float (*out)[4], float (*mat1)[4], float (*mat2)[4])
{
   float temp1[4][4], temp2[4][4];
   int i,j;
   if (mat1 == out) { memcpy(temp1, mat1, sizeof(temp1)); mat1 = temp1; }
   if (mat2 == out) { memcpy(temp2, mat2, sizeof(temp2)); mat2 = temp2; }
   for (j=0; j < 4; ++j)
      for (i=0; i < 4; ++i)
         out[j][i] = mat1[0][i]*mat2[j][0]
                   + mat1[1][i]*mat2[j][1]
                   + mat1[2][i]*mat2[j][2]
                   + mat1[3][i]*mat2[j][3];
}

void float44_mul_t(float (*out)[4], float (*mat1)[4], float (*mat2)[4])
{
   float temp1[4][4], temp2[4][4];
   int i,j;
   if (mat1 == out) { memcpy(temp1, mat1, sizeof(temp1)); mat1 = temp1; }
   if (mat2 == out) { memcpy(temp2, mat2, sizeof(temp2)); mat2 = temp2; }
   for (j=0; j < 4; ++j)
      for (i=0; i < 4; ++i)
         out[j][i] = mat1[0][i]*mat2[0][j]
                   + mat1[1][i]*mat2[1][j]
                   + mat1[2][i]*mat2[2][j]
                   + mat1[3][i]*mat2[3][j];
}

void float33_mul(float out[3][3], float mat1[3][3], float mat2[3][3])
{
   int i,j;
   for (j=0; j < 3; ++j)
      for (i=0; i < 3; ++i)
         out[j][i] = mat1[0][i]*mat2[j][0]
                   + mat1[1][i]*mat2[j][1]
                   + mat1[2][i]*mat2[j][2];
}

void float33_mul_t(float out[3][3], float mat1[3][3], float mat2[3][3])
{
   int i,j;
   for (j=0; j < 3; ++j)
      for (i=0; i < 3; ++i)
         out[j][i] = mat1[0][i]*mat2[0][j]
                   + mat1[1][i]*mat2[1][j]
                   + mat1[2][i]*mat2[2][j];
}

void float33_orthonormalize(float mat[3][3])
{
   mat3_orthonormalize(mat3_f(mat));
}

void float33_rotation_around_x(float mat[3][3], float ang)
{
   float s = (float) sin(ang), c = (float) cos(ang);
   mat[0][0] =  1; mat[0][1] =  0; mat[0][2] =  0;
   mat[1][0] =  0; mat[1][1] =  c; mat[1][2] = -s;
   mat[2][0] =  0; mat[2][1] =  s; mat[2][2] =  c;
}

void float33_rotation_around_y(float mat[3][3], float ang)
{
   float s = (float) sin(ang), c = (float) cos(ang);
   mat[0][0] =  c; mat[0][1] =  0; mat[0][2] =  s;
   mat[1][0] =  0; mat[1][1] =  1; mat[1][2] =  0;
   mat[2][0] = -s; mat[2][1] =  0; mat[2][2] =  c;
}

void float33_rotation_around_z(float mat[3][3], float ang)
{
   float s = (float) sin(ang), c = (float) cos(ang);
   mat[0][0] =  c; mat[0][1] = -s; mat[0][2] =  0;
   mat[1][0] =  s; mat[1][1] =  c; mat[1][2] =  0;
   mat[2][0] =  0; mat[2][1] =  0; mat[2][2] =  1;
}

void float33_rotation_around_axis(float mat[3][3], int axis, float ang)
{
   assert(axis >= 0 && axis <= 2);
   switch (axis) {
      case 0: float33_rotation_around_x(mat, ang); break;
      case 1: float33_rotation_around_y(mat, ang); break;
      case 2: float33_rotation_around_z(mat, ang); break;
      default: float33_identity(mat);
   }
}

void float33_rotation_around_vec(float mat[3][3], vec *v, float ang)
{
   float s = (float) sin(ang), c = (float) cos(ang), ic = 1-c;
   vec p;
   vec_norm(&p, v);

   mat[0][0] =  c              +  ic * p.x * p.x ;
   mat[0][1] = ic * p.x * p.y  +   s * p.z       ;
   mat[0][2] = ic * p.x * p.z  -   s * p.y       ;

   mat[1][0] = ic * p.y * p.x  -   s * p.z       ;
   mat[1][1] =  c              +  ic * p.y * p.y ;
   mat[1][2] = ic * p.y * p.z  +   s * p.x       ;

   mat[2][0] = ic * p.z * p.x  +   s * p.y       ;
   mat[2][1] = ic * p.z * p.y  -   s * p.x       ;
   mat[2][2] =  c              +  ic * p.z * p.z ;
}

void mat3_rotation_around_axis(mat3 *m, int axis, float ang)
{
   float33_rotation_around_axis(float33_m(m), axis, ang);
}

void mat3_rotation_around_vec(mat3 *m, vec *v, float ang)
{
   float33_rotation_around_vec(float33_m(m), v, ang);
}

void mat3_orthonormalize(mat3 *m)
{
   vec *v = m->m;
   vec_cross(v+2, v+0, v+1);
   vec_normeq(v+2);
   vec_cross(v+0, v+1, v+2);
   vec_normeq(v+0);
   vec_cross(v+1, v+2, v+0);
}

void mat3_vec_mul(vec *out, mat3 *m, vec *in)  { float33_vec_mul(out, float33_m(m), in); }
void mat3_vec_mul_t(vec *out, mat3 *m, vec *in){ float33_vec_mul_t(out, float33_m(m), in); }
void mat4_vec_mul(vec *out, mat4 *m, vec *in)  { float44_vec_mul(out, float44_m(m), in); }

void mat4_mul  (mat4 *out, mat4 *m1, mat4 *m2) { float44_mul(float44_m(out), float44_m(m1), float44_m(m2)); }
void mat3_mul  (mat3 *out, mat3 *m1, mat3 *m2) { float33_mul(float33_m(out), float33_m(m1), float33_m(m2)); }
void mat3_mul_t(mat3 *out, mat3 *m1, mat3 *m2) { float33_mul_t(float33_m(out), float33_m(m1), float33_m(m2)); }

//@TODO: create direct implementations of these instead of constructing the matrix
void vec_rotate_x(vec *dest, vec *src, float ang)
{
   mat3 m;
   mat3_rotation_around_axis(&m, 0, ang);
   mat3_vec_mul(dest, &m, src);
}

void vec_rotate_y(vec *dest, vec *src, float ang)
{
   mat3 m;
   mat3_rotation_around_axis(&m, 1, ang);
   mat3_vec_mul(dest, &m, src);
}

void vec_rotate_z(vec *dest, vec *src, float ang)
{
   mat3 m;
   mat3_rotation_around_axis(&m, 2, ang);
   mat3_vec_mul(dest, &m, src);
}

void vec_rotate_euler_zup_facing_y(vec *dest, vec *src, float x, float y, float z)
{
   vec_rotate_y(dest, src, y);
   vec_rotate_x(dest, dest, x);
   vec_rotate_z(dest, dest, z);
}

#if 0
// adapted from JGT "Fast Triangle Intersection"
apbool apIntersect_RayTriangle(apvec *start, apvec *dir,
             apvec *v0, apvec *v1, apvec *v2,
		       float *t, apbool backface)
{
   apvec edge1, edge2, tvec, pvec, qvec;
   double det,inv_det,u,v;

   //backface=0;

   // get the spanning vectors
   apvec_Sub(&edge1, v1, v0);
   apvec_Sub(&edge2, v2, v0);

   // determinant is dot of cross
   apvec_Cross(&pvec, dir, &edge2);
   det = apvec_Dot(&pvec, &edge1);

   if (backface) {
      if (det < DET_EPSILON)
         return FALSE;
   } else {
      // if too close to zero, assume parallel (= miss)
      if (det > -DET_EPSILON && det < DET_EPSILON)
        return FALSE;
   }

   inv_det = 1.0 / det;

   // distance from vert0 to ray start
   apvec_Sub(&tvec, start, v0);

   // calculate U determinant and test that barycentric
   u = apvec_Dot(&tvec, &pvec) * inv_det;
   if (u < 0 || u > 1)
     return FALSE;

   // compute V determinant
   apvec_Cross(&qvec, &tvec, &edge1);
   v = apvec_Dot(dir, &qvec) * inv_det;

   // validate barycentric coordinates
   if (v < 0 || u + v > 1)
     return FALSE;

   /* calculate t, ray intersects triangle */
   *t = apvec_Dot(&edge2, &qvec) * inv_det;

   return TRUE;
}
#endif


void quat_lerp(quat *dest, quat *q1, quat *q2, float t)
{
   dest->x = q1->x + t * (q2->x - q1->x);
   dest->y = q1->y + t * (q2->y - q1->y);
   dest->z = q1->z + t * (q2->z - q1->z);
   dest->w = q1->w + t * (q2->w - q1->w);
}

void quat_lerp_normalize(quat *dest, quat *q1, quat *q2, float t)
{
   quat_lerp(dest, q1, q2, t);
   quat_normalize(dest);
}

void quat_lerp_neighbor_normalize(quat *dest, quat *q1, quat *q2, float t)
{
   if (q1->x*q2->x + q1->y*q2->y + q1->z*q2->z + q1->w*q2->w < 0) {
      quat temp = { -q1->x, -q1->y, -q1->z, -q1->w };
      quat_lerp(dest, &temp, q2, t);
   } else
      quat_lerp(dest, q1, q2, t);
   quat_normalize(dest);
}

void quat_rotation_around_axis(quat *dest, vec *axis, float ang)
{
   dest->w = (float) cos(ang/2);
   vec_scale((vec *) dest, axis, (float) sin(ang/2));
}

float quat_get_rotation(vec *axis, quat *src)
{
   float ang = (float) acos(src->w);
   float sine = (float) sin(ang);
   if (sine >= 0.00001) {
      vec_scale(axis, (vec *) src, 1/sine);
      return 2*ang;
   } else {
      float d = (float) sqrt(src->x*src->x + src->y*src->y + src->z*src->z);
      if (d > 0.000001) {
         vec_scale(axis, (vec *) src, 1/d);
      } else {
         axis->x = 1;
         axis->y = axis->z = 0;
      }
      return 0;
   }
}

void quat_from_mat3(quat *dest, mat3 *mat)
{
   quat_from_float33(dest, (float(*)[3]) mat);
}

void mat3_from_quat(mat3 *mat, quat *q)
{
   float33_from_quat((float(*)[3]) mat, q);
}

void quat_mul(quat *dest, quat *q1, quat *q2)
{
   quat temp;
   if (dest == q1) { temp = *q1; q1 = &temp; }
   if (dest == q2) { temp = *q2; q2 = &temp; }

   dest->x = q1->w*q2->x + q1->x*q2->w + q1->y*q2->z - q1->z*q2->y;
   dest->y = q1->w*q2->y - q1->x*q2->z + q1->y*q2->w + q1->z*q2->x;
   dest->z = q1->w*q2->z + q1->x*q2->y - q1->y*q2->x + q1->z*q2->w;
   dest->w = q1->w*q2->w - q1->x*q2->x - q1->y*q2->y - q1->z*q2->z;
}

void quat_vec_mul(vec *dest, quat *q, vec *v)
{
   quat qvec = { v->x, v->y, v->z, 0 };
   quat qinv = { -q->x, -q->y, -q->z, q->w };
   quat temp;

   quat_mul(&temp, q, &qvec);
   quat_mul(&temp, &qinv, &temp);
   dest->x = temp.x;
   dest->y = temp.y;
   dest->z = temp.z;
}

void quat_normalize(quat *q)
{
   float d = (float) sqrt(q->x*q->x + q->y*q->y + q->z*q->z + q->w*q->w);
   if (d >= 0.00001) {
      d = 1/d;
      q->x *= d;
      q->y *= d;
      q->z *= d;
      q->w *= d;
   } else {
      quat_identity(q);
   }
}

void quat_invert(quat *dest)
{
   dest->x = -dest->x;
   dest->y = -dest->y;
   dest->z = -dest->z;
}

void quat_scale_addeq(quat *dest, quat *q, float sc)
{
   dest->x += q->x * sc;
   dest->y += q->y * sc;
   dest->z += q->z * sc;
   dest->w += q->w * sc;
}

void quat_identity(quat *q)
{
   q->x = q->y = q->z = 0;
   q->w = 1;
}

void quat_from_float33(quat *dest, float m[3][3])
{
   dest->x = 1 + m[0][0] - m[1][1] - m[2][2]; if (dest->x < 0) dest->x = 0; else dest->x = (float) sqrt(dest->x)/2;
   dest->y = 1 + m[0][0] + m[1][1] - m[2][2]; if (dest->y < 0) dest->y = 0; else dest->y = (float) sqrt(dest->y)/2;
   dest->z = 1 + m[0][0] - m[1][1] + m[2][2]; if (dest->z < 0) dest->z = 0; else dest->z = (float) sqrt(dest->z)/2;
   dest->w = 1 + m[0][0] + m[1][1] + m[2][2]; if (dest->w < 0) dest->w = 0; else dest->w = (float) sqrt(dest->w)/2;
   if (m[2][1] - m[1][2] < 0) dest->x = -dest->x;
   if (m[0][2] - m[2][0] < 0) dest->y = -dest->y;
   if (m[1][0] - m[0][1] < 0) dest->z = -dest->z;
}

void quat_from_float44(quat *dest, float m[4][4])
{
   dest->x = 1 + m[0][0] - m[1][1] - m[2][2]; if (dest->x < 0) dest->x = 0; else dest->x = (float) sqrt(dest->x)/2;
   dest->y = 1 - m[0][0] + m[1][1] - m[2][2]; if (dest->y < 0) dest->y = 0; else dest->y = (float) sqrt(dest->y)/2;
   dest->z = 1 - m[0][0] - m[1][1] + m[2][2]; if (dest->z < 0) dest->z = 0; else dest->z = (float) sqrt(dest->z)/2;
   dest->w = 1 + m[0][0] + m[1][1] + m[2][2]; if (dest->w < 0) dest->w = 0; else dest->w = (float) sqrt(dest->w)/2;
   if (m[2][1] - m[1][2] < 0) dest->x = -dest->x;
   if (m[0][2] - m[2][0] < 0) dest->y = -dest->y;
   if (m[1][0] - m[0][1] < 0) dest->z = -dest->z;
}

void float33_from_quat(float mat[3][3], quat *q)
{
   float X2,Y2,Z2;      //2*QX, 2*QY, 2*QZ
   float XX2,YY2,ZZ2;   //2*QX*QX, 2*QY*QY, 2*QZ*QZ
   float XY2,XZ2,XW2;   //2*QX*QY, 2*QX*QZ, 2*QX*QW
   float YZ2,YW2,ZW2;   // ...

   X2  = 2.0f * q->x;
   XX2 = X2   * q->x;
   XY2 = X2   * q->y;
   XZ2 = X2   * q->z;
   XW2 = X2   * q->w;

   Y2  = 2.0f * q->y;
   YY2 = Y2   * q->y;
   YZ2 = Y2   * q->z;
   YW2 = Y2   * q->w;
   
   Z2  = 2.0f * q->z;
   ZZ2 = Z2   * q->z;
   ZW2 = Z2   * q->w;
   
   mat[0][0] = 1.0f - YY2 - ZZ2;
   mat[0][1] = XY2  - ZW2;
   mat[0][2] = XZ2  + YW2;

   mat[1][0] = XY2  + ZW2;
   mat[1][1] = 1.0f - XX2 - ZZ2;
   mat[1][2] = YZ2  - XW2;

   mat[2][0] = XZ2  - YW2;
   mat[2][1] = YZ2  + XW2;
   mat[2][2] = 1.0f - XX2 - YY2;
}

void float44_from_quat(float mat[4][4], quat *q)
{
   float m[3][3];
   float33_from_quat(m, q);
   mat[0][0] = m[0][0]; mat[0][1] = m[0][1]; mat[0][2] = m[0][2]; mat[0][3] = 0;
   mat[1][0] = m[1][0]; mat[1][1] = m[1][1]; mat[1][2] = m[1][2]; mat[1][3] = 0;
   mat[2][0] = m[2][0]; mat[2][1] = m[2][1]; mat[2][2] = m[2][2]; mat[2][3] = 0;
   mat[3][0] = mat[3][1] = mat[3][2] = 0;
   mat[3][3] = 1;
}

void float44_from_quat_vec(float mat[4][4], quat *rot, vec *trans)
{
   float m[3][3];
   float33_from_quat(m, rot);
   mat[0][0] = m[0][0]; mat[0][1] = m[0][1]; mat[0][2] = m[0][2]; mat[0][3] = 0;
   mat[1][0] = m[1][0]; mat[1][1] = m[1][1]; mat[1][2] = m[1][2]; mat[1][3] = 0;
   mat[2][0] = m[2][0]; mat[2][1] = m[2][1]; mat[2][2] = m[2][2]; mat[2][3] = 0;
   mat[3][0] = trans->x;
   mat[3][1] = trans->y;
   mat[3][2] = trans->z;
   mat[3][3] = 1;
}

#endif //STB_DEFINE

#undef STB_EXTERN

#endif //INCLUDE_STB_VEC_H

