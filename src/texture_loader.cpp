#include "3rd/crn_decomp.h"

#define STB_GLEXT_DECLARE "glext_list.h"
#include "stb_gl.h"

extern "C" int load_crn_to_texture(unsigned char *data, size_t length)
{
   crnd::crn_level_info level_info;
   crnd::crnd_unpack_context cuc = crnd::crnd_unpack_begin(data, length);

   if (!crnd::crnd_get_level_info(data, length, 0, &level_info))
      return 0;

   size_t size = level_info.m_blocks_x * level_info.m_blocks_y * level_info.m_bytes_per_block;
   unsigned char *output = (unsigned char *) malloc(size);

   for (int level=0; level < 13; ++level) {
      if (!crnd::crnd_get_level_info(data, length, level, &level_info))
         break;
      unsigned int pitch_bytes = level_info.m_blocks_x * level_info.m_bytes_per_block;
      if (!crnd::crnd_unpack_level(cuc, (void **) &output, size, pitch_bytes, level))
         break;

      glCompressedTexImage2D(GL_TEXTURE_2D, level, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                         level_info.m_width, level_info.m_height,
                         0, pitch_bytes*level_info.m_blocks_y, output);
   }

   free(output);

   crnd::crnd_unpack_end(cuc);

   return 1;
}

extern "C" int load_crn_to_texture_array(int slot, unsigned char *data, size_t length)
{
   crnd::crn_level_info level_info;
   crnd::crnd_unpack_context cuc = crnd::crnd_unpack_begin(data, length);

   if (!crnd::crnd_get_level_info(data, length, 0, &level_info))
      return 0;

   size_t size = level_info.m_blocks_x * level_info.m_blocks_y * level_info.m_bytes_per_block;
   unsigned char *output = (unsigned char *) malloc(size);

   for (int level=0; level < 13; ++level) {
      if (!crnd::crnd_get_level_info(data, length, level, &level_info))
         break;
      unsigned int pitch_bytes = level_info.m_blocks_x * level_info.m_bytes_per_block;
      if (!crnd::crnd_unpack_level(cuc, (void **) &output, size, pitch_bytes, level))
         break;
      glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY_EXT, level,
                         0,0,slot,
                         level_info.m_width, level_info.m_height,1,
                         GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                         pitch_bytes*level_info.m_blocks_y, output);
   }

   free(output);

   crnd::crnd_unpack_end(cuc);

   return 1;
}

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

extern "C" int load_bitmap_to_texture_array(int slot, unsigned char *data, int w, int h, int wrap, int premul)
{
   int i;
   unsigned char *old_data = data;
   glTexSubImage3D(GL_TEXTURE_2D_ARRAY_EXT, 0, 0,0,slot, w,h,1, GL_RGBA, GL_UNSIGNED_BYTE, data);

   for (i=1; i < 13 && (w > 1 || h > 1); ++i) {
      int nw = w>>1, nh= h>>1;
      unsigned char *new_data = (unsigned char *) malloc(nw * nh * 4);
      stbir_resize_uint8_srgb_edgemode(old_data, w, h, 0, new_data, nw, nh, 0, 4, 3, premul ? STBIR_FLAG_ALPHA_PREMULTIPLIED : 0, wrap ? STBIR_EDGE_WRAP : STBIR_EDGE_ZERO);
      if (old_data != data) free(old_data);
      old_data = new_data;
      w = nw;
      h = nh;

      glTexSubImage3D(GL_TEXTURE_2D_ARRAY_EXT, i, 0,0,slot, w,h,1, GL_RGBA, GL_UNSIGNED_BYTE, old_data);
   }
   if (old_data != data) free(old_data);

   return 1;
}
