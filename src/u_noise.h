// perlin-style noise: s is a the number of fractional bits in x&y
extern int fast_noise(int x, int y, int s, int seed); // 0..65535, randomness in high bits
extern int big_noise(int x, int y, int s, unsigned int seed); // 0..65535, randomness in high bits, uses 20 bits of seed
extern float stb_perlin_noise3(float x, float y, float z, int x_wrap, int y_wrap, int z_wrap); // -1 to 1

// discrete parametric noise
extern unsigned int flat_noise32_weak(int x, int y, unsigned int seed); // all bits random
extern unsigned int flat_noise32_strong(int x, int y, unsigned int seed); // all bits random
extern int flat_noise8(int x, int y, int seed); // 8 random bits

extern void stb_sha256_noise(unsigned int result[8], unsigned int  x, unsigned int  y, unsigned int  seed1, unsigned int  seed2); // 256 random bits

