#include "u_noise.h"
#include "caveview.h"
#include <string.h>
#include <math.h> // floor()

static unsigned char stb_randtab[512] =
{
   23, 125, 161, 52, 103, 117, 70, 37, 247, 101, 203, 169, 124, 126, 44, 123, 
   152, 238, 145, 45, 171, 114, 253, 10, 192, 136, 4, 157, 249, 30, 35, 72, 
   175, 63, 77, 90, 181, 16, 96, 111, 133, 104, 75, 162, 93, 56, 66, 240, 
   8, 50, 84, 229, 49, 210, 173, 239, 141, 1, 87, 18, 2, 198, 143, 57, 
   225, 160, 58, 217, 168, 206, 245, 204, 199, 6, 73, 60, 20, 230, 211, 233, 
   94, 200, 88, 9, 74, 155, 33, 15, 219, 130, 226, 202, 83, 236, 42, 172, 
   165, 218, 55, 222, 46, 107, 98, 154, 109, 67, 196, 178, 127, 158, 13, 243, 
   65, 79, 166, 248, 25, 224, 115, 80, 68, 51, 184, 128, 232, 208, 151, 122, 
   26, 212, 105, 43, 179, 213, 235, 148, 146, 89, 14, 195, 28, 78, 112, 76, 
   250, 47, 24, 251, 140, 108, 186, 190, 228, 170, 183, 139, 39, 188, 244, 246, 
   132, 48, 119, 144, 180, 138, 134, 193, 82, 182, 120, 121, 86, 220, 209, 3, 
   91, 241, 149, 85, 205, 150, 113, 216, 31, 100, 41, 164, 177, 214, 153, 231, 
   38, 71, 185, 174, 97, 201, 29, 95, 7, 92, 54, 254, 191, 118, 34, 221, 
   131, 11, 163, 99, 234, 81, 227, 147, 156, 176, 17, 142, 69, 12, 110, 62, 
   27, 255, 0, 194, 59, 116, 242, 252, 19, 21, 187, 53, 207, 129, 64, 135, 
   61, 40, 167, 237, 102, 223, 106, 159, 197, 189, 215, 137, 36, 32, 22, 5,  

   // and a second copy so we don't need an extra mask or static initializer
   23, 125, 161, 52, 103, 117, 70, 37, 247, 101, 203, 169, 124, 126, 44, 123, 
   152, 238, 145, 45, 171, 114, 253, 10, 192, 136, 4, 157, 249, 30, 35, 72, 
   175, 63, 77, 90, 181, 16, 96, 111, 133, 104, 75, 162, 93, 56, 66, 240, 
   8, 50, 84, 229, 49, 210, 173, 239, 141, 1, 87, 18, 2, 198, 143, 57, 
   225, 160, 58, 217, 168, 206, 245, 204, 199, 6, 73, 60, 20, 230, 211, 233, 
   94, 200, 88, 9, 74, 155, 33, 15, 219, 130, 226, 202, 83, 236, 42, 172, 
   165, 218, 55, 222, 46, 107, 98, 154, 109, 67, 196, 178, 127, 158, 13, 243, 
   65, 79, 166, 248, 25, 224, 115, 80, 68, 51, 184, 128, 232, 208, 151, 122, 
   26, 212, 105, 43, 179, 213, 235, 148, 146, 89, 14, 195, 28, 78, 112, 76, 
   250, 47, 24, 251, 140, 108, 186, 190, 228, 170, 183, 139, 39, 188, 244, 246, 
   132, 48, 119, 144, 180, 138, 134, 193, 82, 182, 120, 121, 86, 220, 209, 3, 
   91, 241, 149, 85, 205, 150, 113, 216, 31, 100, 41, 164, 177, 214, 153, 231, 
   38, 71, 185, 174, 97, 201, 29, 95, 7, 92, 54, 254, 191, 118, 34, 221, 
   131, 11, 163, 99, 234, 81, 227, 147, 156, 176, 17, 142, 69, 12, 110, 62, 
   27, 255, 0, 194, 59, 116, 242, 252, 19, 21, 187, 53, 207, 129, 64, 135, 
   61, 40, 167, 237, 102, 223, 106, 159, 197, 189, 215, 137, 36, 32, 22, 5,  
};

// input is x>>s, y>>s
// wraps as (x>>s) % 65536
// output is 0..65535
int big_noise(int x, int y, int s, uint32 seed)
{
   int r0,r1,r00,r01,r10,r11,x0,y0,x1,y1,r;
   int bx0,by0,bx1,by1;
   //int temp = (seed=0);
   int seed1 = seed & 255;
   int seed2 = (seed >> 4) & 255;
   int seed3 = (seed >> 8) & 255;
   int seed4 = (seed >> 12) & 255;
   int ix = x >> s;
   int iy = y >> s;
   int m = (1 << s)-1;
   x &= m;
   y &= m;
   x0 = (ix & 255);
   y0 = (iy & 255);

   x1 = ix+1;
   y1 = iy+1;
   bx0 = (ix >> 8) & 255;
   by0 = (iy >> 8) & 255;
   bx1 = (x1 >> 8) & 255;
   by1 = (y1 >> 8) & 255;
   x1 &= 255;
   y1 &= 255;

   // it would be "more random" to run these throught the permutation table, but this should be ok
   x0 ^= seed1;
   x1 ^= seed1;
   bx0 ^= seed2;
   bx1 ^= seed2;
   y0 ^= seed3;
   y1 ^= seed3;
   by0 ^= seed4;
   by1 ^= seed4;

   r0 = stb_randtab[x0];
   r1 = stb_randtab[x1];
   r0 = stb_randtab[r0+bx0];
   r1 = stb_randtab[r1+bx1];

   r00 = stb_randtab[r0 + y0];
   r01 = stb_randtab[r0 + y1];
   r10 = stb_randtab[r1 + y0];
   r11 = stb_randtab[r1 + y1];

   r00 = stb_randtab[r00 + by0];
   r01 = stb_randtab[r01 + by1];
   r10 = stb_randtab[r10 + by0];
   r11 = stb_randtab[r11 + by1];

   // weight factor is from 0..2^10-1
   if (s <= 10) {
      x <<= (10-s);
      y <<= (10-s);
   } else {
      x >>= (s-10);
      y >>= (s-10);
   }

   r0 = (r00<<10) + (r01-r00)*y;
   r1 = (r10<<10) + (r11-r10)*y;

   r  = (r0 <<10) + (r1 -r0 )*x;

   return r>>12;
}

#if 1
// input is x>>s, y>>s; output is 0..65535
int fast_noise(int x, int y, int s, int seed)
{
   int r0,r1,r00,r01,r10,r11,x0,y0,x1,y1,r;
   int ix = x >> s;
   int iy = y >> s;
   int m = (1 << s)-1;
   x &= m;
   y &= m;
   x0 = (ix & 255);
   y0 = (iy & 255);
   x1 = x0+1;
   y1 = y0+1;

   seed = seed & 255;
   r0 = stb_randtab[x0^seed];
   r1 = stb_randtab[x1^seed];

   r00 = stb_randtab[r0 + y0];
   r01 = stb_randtab[r0 + y1];
   r10 = stb_randtab[r1 + y0];
   r11 = stb_randtab[r1 + y1];

   // weight factor is from 0..2^10-1
   if (s <= 10) {
      x <<= (10-s);
      y <<= (10-s);
   } else {
      x >>= (s-10);
      y >>= (s-10);
   }

   r0 = (r00<<10) + (r01-r00)*y;
   r1 = (r10<<10) + (r11-r10)*y;
   r  = (r0 <<10) + (r1 -r0 )*x;

   return r>>12;
}
#endif

#if 0 // unused
// input is x, y; output is 0..255; doesn't repeat for 2^16
int flat_noise8(int x, int y, int seed)
{
   int highseed;
   int bx,by,r;
   bx = (x >> 8) & 255;
   by = (y >> 8) & 255;
   x = (x & 255);
   y = (y & 255);

   highseed = (seed >> 8) & 255;
   seed &= 255;
   r = stb_randtab[     x ^ seed];
   r = stb_randtab[r +  (y^highseed)];
   r = stb_randtab[r + bx];
   r = stb_randtab[r + by];
   return r;
}
#endif

#if defined(_MSC_VER) && !defined(_DEBUG)
#define rot(x,k)  _lrotl(x, (k))
#else
#define rot(x,k)  (((x)<<(k)) + ((uint32)(x)>>(32-(k))))
#endif

// input is x,y; output is 0..2^32-1; doesn't repeat
uint32 flat_noise32_weak(int rawx, int rawy, uint32 seed)
{
   // burtle lookup3
   // original was 55 logic/arithmetic ops, critical path 26 instructions
   // modified with #if 0 gives 47 ops, critical path 18
   // but even so it's not decorrelating rawx and rawy enough
   // so we added an extra hash to y
   uint32 c = (uint32) rawx;
   uint32 b = (uint32) rawy;
   uint32 a = (uint32) seed;

   // limited parallelism: can overlap rot and add with previous ops; chain = 12 instructions
   a -= c;  a ^= rot(c, 4);  c += b;
   b -= a;  b ^= rot(a, 6);  a += c;
   c -= b;  c ^= rot(b, 8);  b += a;
   a -= c;  a ^= rot(c,16);  c += b;
   b -= a;  b ^= rot(a,19);  a += c;
   c -= b;  c ^= rot(b, 4);  b += a;

#if 0
   // limited parallelism, can overlap rot() with one previous op; chain = 14 instructions
   c ^= b; c -= rot(b,14);
   a ^= c; a -= rot(c,11);
   b ^= a; b -= rot(a,25);
   c ^= b; c -= rot(b,16);
#endif
   a ^= c; a -= rot(c,4);
   b ^= a; b -= rot(a,14);
   c ^= b; c -= rot(b,24);

   return (int) c;
}

// input is x,y; output is 0..2^32-1; doesn't repeat
uint32 flat_noise32_strong(int rawx, int rawy, uint32 seed)
{
   // burtle lookup3
   // original was 55 logic/arithmetic ops, critical path 26 instructions
   // modified with #if 0 gives 47 ops, critical path 18
   // but even so it's not decorrelating rawx and rawy enough
   // so we added an extra hash to y
   uint32 c = (uint32) rawx;
   uint32 b = (uint32) rawy;
   uint32 a = (uint32) seed;

#if 0
   b = (b+0x7ed55d16) + (b<<12);
   b = (b^0xc761c23c) ^ (b>>19);
   b = (b+0x165667b1) + (b<<5);
   b = (b+0xd3a2646c) ^ (b<<9);
   b = (b+0xfd7046c5) + (b<<3);
   b = (b^0xb55a4f09) ^ (b>>16);

   c = (c+0x7ed55d16) + (c<<12);
   c = (c^0xc761c23c) ^ (c>>19);
   c = (c+0x165667b1) + (c<<5);
   c = (c+0xd3a2646c) ^ (c<<9);
   c = (c+0xfd7046c5) + (c<<3);
   c = (c^0xb55a4f09) ^ (c>>16);
#endif

#if 1
   c -= (c<<6);
   c ^= (c>>17);
   c -= (c<<9);
   c ^= (c<<4);
   c -= (c<<3);
   c ^= (c<<10);
   c ^= (c>>15);

   b -= (b<<6);
   b ^= (b>>17);
   b -= (b<<9);
   b ^= (b<<4);
   b -= (b<<3);
   b ^= (b<<10);
   b ^= (b>>15);
#endif

   // limited parallelism: can overlap rot and add with previous ops; chain = 12 instructions
   a -= c;  a ^= rot(c, 4);  c += b;
   b -= a;  b ^= rot(a, 6);  a += c;
   c -= b;  c ^= rot(b, 8);  b += a;
   a -= c;  a ^= rot(c,16);  c += b;
   b -= a;  b ^= rot(a,19);  a += c;
   c -= b;  c ^= rot(b, 4);  b += a;

#if 1
   // limited parallelism, can overlap rot() with one previous op; chain = 14 instructions
   c ^= b; c -= rot(b,14);
   a ^= c; a -= rot(c,11);
   b ^= a; b -= rot(a,25);
   c ^= b; c -= rot(b,16);
#endif
   a ^= c; a -= rot(c,4);
   b ^= a; b -= rot(a,14);
   c ^= b; c -= rot(b,24);


   a -= c;  a ^= rot(c, 4);  c += b;
   b -= a;  b ^= rot(a, 6);  a += c;
   c -= b;  c ^= rot(b, 8);  b += a;
   a -= c;  a ^= rot(c,16);  c += b;
   b -= a;  b ^= rot(a,19);  a += c;
   c -= b;  c ^= rot(b, 4);  b += a;
   c ^= b; c -= rot(b,14);
   a ^= c; a -= rot(c,11);
   b ^= a; b -= rot(a,25);
   c ^= b; c -= rot(b,16);
   a ^= c; a -= rot(c,4);
   b ^= a; b -= rot(a,14);
   c ^= b; c -= rot(b,24);

   return (int) c;
}

static float stb__perlin_lerp(float a, float b, float t)
{
   return a + (b-a) * t;
}

// different grad function from Perlin's, but easy to modify to match reference
static float stb__perlin_grad(int hash, float x, float y, float z)
{
   static float basis[12][4] =
   {
      {  1, 1, 0 },
      { -1, 1, 0 },
      {  1,-1, 0 },
      { -1,-1, 0 },
      {  1, 0, 1 },
      { -1, 0, 1 },
      {  1, 0,-1 },
      { -1, 0,-1 },
      {  0, 1, 1 },
      {  0,-1, 1 },
      {  0, 1,-1 },
      {  0,-1,-1 },
   };

   // perlin's gradient has 12 cases so some get used 1/16th of the time
   // and some 2/16ths. We reduce bias by changing those fractions
   // to 5/16ths and 6/16ths, and the same 4 cases get the extra weight.
   static unsigned char indices[64] =
   {
      0,1,2,3,4,5,6,7,8,9,10,11,
      //0,9,1,11, // perlin bias
      4,5,10,11, // z-bias
      0,1,2,3,4,5,6,7,8,9,10,11,
      0,1,2,3,4,5,6,7,8,9,10,11,
      0,1,2,3,4,5,6,7,8,9,10,11,
      0,1,2,3,4,5,6,7,8,9,10,11,
   };

   // if you use reference permutation table, change 63 below to 15 to match reference
   float *grad = basis[indices[hash & 63]];

   return grad[0]*x + grad[1]*y + grad[2]*z;
}

float stb_perlin_noise3(float x, float y, float z, int x_wrap, int y_wrap, int z_wrap)
{
   float u,v,w;
   float n000,n001,n010,n011,n100,n101,n110,n111;
   float n00,n01,n10,n11;
   float n0,n1;

   unsigned int x_mask = (x_wrap-1) & 255;
   unsigned int y_mask = (y_wrap-1) & 255;
   unsigned int z_mask = (z_wrap-1) & 255;
   int px = (int) floor(x);
   int py = (int) floor(y);
   int pz = (int) floor(z);
   int x0 = px & x_mask;
   int y0 = py & y_mask;
   int z0 = pz & z_mask;
   int x1 = (px+1) & x_mask;
   int y1 = (py+1) & y_mask;
   int z1 = (pz+1) & z_mask;
   int r0,r1, r00,r01,r10,r11;

   //#define stb__perlin_ease(a)   ((3-2*a)*a*a) 
   #define stb__perlin_ease(a)   (((a*6-15)*a + 10) * a * a * a)

   if (x > -10 && x < -2 && y > 5 && y < 10)
      x = x;

   x -= px; u = stb__perlin_ease(x);
   y -= py; v = stb__perlin_ease(y);
   z -= pz; w = stb__perlin_ease(z);

   r0 = stb_randtab[x0];
   r1 = stb_randtab[x1];

   r00 = stb_randtab[r0+y0];
   r01 = stb_randtab[r0+y1];
   r10 = stb_randtab[r1+y0];
   r11 = stb_randtab[r1+y1];

   n000 = stb__perlin_grad(stb_randtab[r00+z0], x  , y  , z   );
   n001 = stb__perlin_grad(stb_randtab[r00+z1], x  , y  , z-1 );
   n010 = stb__perlin_grad(stb_randtab[r01+z0], x  , y-1, z   );
   n011 = stb__perlin_grad(stb_randtab[r01+z1], x  , y-1, z-1 );
   n100 = stb__perlin_grad(stb_randtab[r10+z0], x-1, y  , z   );
   n101 = stb__perlin_grad(stb_randtab[r10+z1], x-1, y  , z-1 );
   n110 = stb__perlin_grad(stb_randtab[r11+z0], x-1, y-1, z   );
   n111 = stb__perlin_grad(stb_randtab[r11+z1], x-1, y-1, z-1 );

   n00 = stb__perlin_lerp(n000,n100,u);
   n01 = stb__perlin_lerp(n001,n101,u);
   n10 = stb__perlin_lerp(n010,n110,u);
   n11 = stb__perlin_lerp(n011,n111,u);

   n0 = stb__perlin_lerp(n00,n10,v);
   n1 = stb__perlin_lerp(n01,n11,v);

   return stb__perlin_lerp(n0,n1,w);
}

static const uint32 stb__sha256_K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define stb__sha2_blk2(i) (W[i]+=stb__sha2_s1(W[(i-2)&15])+W[(i-7)&15]+stb__sha2_s0(W[(i-15)&15]))
#define stb__sha2_blk0(i) (W[i] = (data[i*4+0]<<24) + (data[i*4+1]<<16) + (data[i*4+2]<<8) + data[i*4+3])
#define stb__sha2_Ch(x,y,z) (z^(x&(y^z)))
#define stb__sha2_Maj(x,y,z) ((x&y)|(z&(x|y)))

#define stb__sha2_a(i) T[(0-i)&7]
#define stb__sha2_b(i) T[(1-i)&7]
#define stb__sha2_c(i) T[(2-i)&7]
#define stb__sha2_d(i) T[(3-i)&7]
#define stb__sha2_e(i) T[(4-i)&7]
#define stb__sha2_f(i) T[(5-i)&7]
#define stb__sha2_g(i) T[(6-i)&7]
#define stb__sha2_h(i) T[(7-i)&7]

#define stb__sha2_R(i)                                                                \
   stb__sha2_h(i) += stb__sha2_S1(stb__sha2_e(i))                                     \
                   + stb__sha2_Ch(stb__sha2_e(i),stb__sha2_f(i),stb__sha2_g(i))       \
                   + stb__sha256_K[i+j]                                               \
                   + (j?stb__sha2_blk2(i):stb__sha2_blk0(i));                         \
	stb__sha2_d(i) += stb__sha2_h(i);                                                  \
   stb__sha2_h(i) += stb__sha2_S0(stb__sha2_a(i))                                     \
                   + stb__sha2_Maj(stb__sha2_a(i),stb__sha2_b(i),stb__sha2_c(i))

// for SHA256
#define stb__sha2_rotrFixed(x,s) (((x) >> (s)) + ((x) << (32-(s))))

#define stb__sha2_S0(x) (stb__sha2_rotrFixed(x, 2)^stb__sha2_rotrFixed(x,13)^stb__sha2_rotrFixed(x,22))
#define stb__sha2_S1(x) (stb__sha2_rotrFixed(x, 6)^stb__sha2_rotrFixed(x,11)^stb__sha2_rotrFixed(x,25))
#define stb__sha2_s0(x) (stb__sha2_rotrFixed(x, 7)^stb__sha2_rotrFixed(x,18)^(x>>3))
#define stb__sha2_s1(x) (stb__sha2_rotrFixed(x,17)^stb__sha2_rotrFixed(x,19)^(x>>10))

void stb_sha256_noise(uint32 result[8], uint32 x, uint32 y, uint32 seed, uint32 param)
{
	uint32 W[16];
	uint32 T[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
   unsigned int j;
   uint8 data[64];
   memset(data, 0, sizeof(data));
   data[ 0] = x >>  0; data[ 8] = seed >>  0; data[ 4] = y >>  0; data[12] = param >>  0;
   data[ 1] = x >>  8; data[ 9] = seed >>  8; data[ 5] = y >>  8; data[13] = param >>  8;
   data[ 2] = x >> 16; data[10] = seed >> 16; data[ 6] = y >> 16; data[14] = param >> 16;
   data[ 3] = x >> 24; data[11] = seed >> 24; data[ 7] = y >> 24; data[15] = param >> 24;

	for (j=0; j<64; j+=16)
	{
		stb__sha2_R( 0); stb__sha2_R( 1); stb__sha2_R( 2); stb__sha2_R( 3);
		stb__sha2_R( 4); stb__sha2_R( 5); stb__sha2_R( 6); stb__sha2_R( 7);
		stb__sha2_R( 8); stb__sha2_R( 9); stb__sha2_R(10); stb__sha2_R(11);
		stb__sha2_R(12); stb__sha2_R(13); stb__sha2_R(14); stb__sha2_R(15);
	}
   result[0] = stb__sha2_a(0);
   result[1] = stb__sha2_b(0);
   result[2] = stb__sha2_c(0);
   result[3] = stb__sha2_d(0);
   result[4] = stb__sha2_e(0);
   result[5] = stb__sha2_f(0);
   result[6] = stb__sha2_g(0);
   result[7] = stb__sha2_h(0);
}
