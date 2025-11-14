#include "image_util.h"

#include <cstring>

// DDS file support
// See
// https://docs.microsoft.com/en-us/windows/win32/direct3ddds/dx-graphics-dds-pguide
#define FOURCC(a, b, c, d)                                     \
  (((a) & 0xFF) | (((b) & 0xFF) << 8) | (((c) & 0xFF) << 16) | \
   (((d) & 0xFF) << 24))

static const uint32_t kDXT1Magic = FOURCC('D', 'X', 'T', '1');
// static const uint32_t kDXT2Magic = FOURCC('D', 'X', 'T', '2');
static const uint32_t kDXT3Magic = FOURCC('D', 'X', 'T', '3');
// static const uint32_t kDXT4Magic = FOURCC('D', 'X', 'T', '4');
static const uint32_t kDXT5Magic = FOURCC('D', 'X', 'T', '5');

enum DDH_FLAGS {
  DDSD_CAPS = 0x1,
  DDSD_HEIGHT = 0x2,
  DDSD_WIDTH = 0x4,
  DDSD_PITCH = 0x8,
  DDSD_PIXELFORMAT = 0x1000,
  DDSD_MIPMAPCOUNT = 0x20000,
  DDSD_LINEARSIZE = 0x80000,
  DDSD_DEPTH = 0x800000,
};

enum DDH_CAPS {
  DDSCAPS_COMPLEX = 0x08,
  DDSCAPS_TEXTURE = 0x1000,
  DDSCAPS_MIPMAP = 0x400000,
};

enum DDH_CAPS2 {
  DDSCAPS2_CUBEMAP = 0x200,
  DDSCAPS2_CUBEMAP_POSITIVEX = 0x400,
  DDSCAPS2_CUBEMAP_NEGATIVEX = 0x800,
  DDSCAPS2_CUBEMAP_POSITIVEY = 0x1000,
  DDSCAPS2_CUBEMAP_NEGATIVEY = 0x2000,
  DDSCAPS2_CUBEMAP_POSITIVEZ = 0x4000,
  DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x8000,
  DDSCAPS2_VOLUME = 0x200000,
};

enum DDPF_FLAGS {
  DDPF_ALPHAPIXELS = 0x1,
  DDPF_ALPHA = 0x2,
  DDPF_FOURCC = 0x4,
  DDPF_RGB = 0x40,
  DDPF_YUV = 0x200,
  DDPF_LUMINANCE = 0x20000,
};

#pragma pack(push)
#pragma pack(1)
struct DDS_PIXELFORMAT {
  uint32_t dwSize;
  uint32_t dwFlags;
  uint32_t dwFourCC;
  uint32_t dwRGBBitCount;
  uint32_t dwRBitMask;
  uint32_t dwGBitMask;
  uint32_t dwBBitMask;
  uint32_t dwABitMask;
};

struct DDS_HEADER {
  uint32_t dwSize;
  uint32_t dwFlags;
  uint32_t dwHeight;
  uint32_t dwWidth;
  uint32_t dwPitchOrLinearSize;
  uint32_t dwDepth;
  uint32_t dwMipMapCount;
  uint32_t dwReserved1[11];
  DDS_PIXELFORMAT ddspf;
  uint32_t dwCaps;
  uint32_t dwCaps2;
  uint32_t dwCaps3;
  uint32_t dwCaps4;
  uint32_t dwReserved2;
};

struct DDS_FILE {
  uint32_t dwMagic;
  DDS_HEADER header;
};
#pragma pack(pop)

std::shared_ptr<uint8_t[]> RGB565ToRGB88(const void* src, uint32_t src_size) {
  auto ret = std::shared_ptr<uint8_t[]>(new uint8_t[src_size << 1]);

  const auto* src_ptr = reinterpret_cast<const uint16_t*>(src);
  auto* dest_ptr = ret.get();
  for (auto i = 0; i < src_size >> 1; ++i) {
    uint16_t pixel = *src_ptr++;

    uint8_t r5 = (pixel >> 11) & 0x1F;
    uint8_t g6 = (pixel >> 5) & 0x3F;
    uint8_t b5 = pixel & 0x1F;

    *dest_ptr++ = (r5 << 3) | (r5 >> 2);
    *dest_ptr++ = (g6 << 2) | (g6 >> 4);
    *dest_ptr++ = (b5 << 3) | (b5 >> 2);
  }

  return ret;
}

std::shared_ptr<uint8_t[]> AXR5G5B5ToRGB888(const void* src,
                                            uint32_t src_size) {
  auto ret = std::shared_ptr<uint8_t[]>(new uint8_t[src_size << 1]);

  const auto* src_ptr = reinterpret_cast<const uint16_t*>(src);
  auto* dest_ptr = ret.get();
  for (auto i = 0; i < src_size >> 1; ++i) {
    uint16_t pixel = *src_ptr++;

    uint8_t r5 = (pixel >> 10) & 0x1F;
    uint8_t g5 = (pixel >> 5) & 0x1F;
    uint8_t b5 = pixel & 0x1F;

    *dest_ptr++ = (r5 << 3) | (r5 >> 2);
    *dest_ptr++ = (g5 << 3) | (g5 >> 2);
    *dest_ptr++ = (b5 << 3) | (b5 >> 2);
  }

  return ret;
}

std::shared_ptr<uint8_t[]> A1R5G5B5ToRGBA888(const void* src,
                                             uint32_t src_size) {
  auto ret = std::shared_ptr<uint8_t[]>(new uint8_t[src_size << 1]);

  const auto* src_ptr = reinterpret_cast<const uint16_t*>(src);
  auto* dest_ptr = ret.get();
  for (auto i = 0; i < src_size >> 1; ++i) {
    uint16_t pixel = *src_ptr++;

    uint8_t alpha = (pixel >> 15) & 0x01;
    uint8_t r5 = (pixel >> 10) & 0x1F;
    uint8_t g5 = (pixel >> 5) & 0x1F;
    uint8_t b5 = pixel & 0x1F;

    *dest_ptr++ = alpha * 0xFF;
    *dest_ptr++ = (r5 << 3) | (r5 >> 2);
    *dest_ptr++ = (g5 << 3) | (g5 >> 2);
    *dest_ptr++ = (b5 << 3) | (b5 >> 2);
  }

  return ret;
}

std::shared_ptr<uint8_t[]> A4R4G4B4ToRGBA888(const void* src,
                                             uint32_t src_size) {
  auto ret = std::shared_ptr<uint8_t[]>(new uint8_t[src_size << 1]);

  const auto* src_ptr = reinterpret_cast<const uint16_t*>(src);
  auto* dest_ptr = ret.get();
  for (auto i = 0; i < src_size >> 1; ++i) {
    uint16_t pixel = *src_ptr++;

    uint8_t alpha = (pixel >> 12) & 0xF;
    uint8_t red = (pixel >> 8) & 0xF;
    uint8_t green = (pixel >> 4) & 0xF;
    uint8_t blue = pixel & 0xF;

    *dest_ptr++ = (alpha << 4) | (alpha >> 1);
    *dest_ptr++ = (red << 4) | (red >> 1);
    *dest_ptr++ = (green << 4) | (green >> 1);
    *dest_ptr++ = (blue << 4) | (blue >> 1);
  }

  return ret;
}

std::shared_ptr<uint8_t[]> BGRAToRGBA(const void* src, uint32_t src_size) {
  auto ret = std::shared_ptr<uint8_t[]>(new uint8_t[src_size]);

  auto pixel_in = static_cast<const uint8_t*>(src);
  auto pixel_out = ret.get();

  for (uint32_t i = 0; i < src_size / 4; ++i) {
    *pixel_out++ = pixel_in[2];
    *pixel_out++ = pixel_in[1];
    *pixel_out++ = pixel_in[0];
    *pixel_out++ = pixel_in[3];
    pixel_in += 4;
  }

  return ret;
}

std::shared_ptr<uint8_t[]> ABGRToRGBA(const void* src, uint32_t src_size) {
  auto ret = std::shared_ptr<uint8_t[]>(new uint8_t[src_size]);

  auto pixel_in = static_cast<const uint8_t*>(src);
  auto pixel_out = ret.get();

  for (uint32_t i = 0; i < src_size / 4; ++i) {
    *pixel_out++ = pixel_in[3];
    *pixel_out++ = pixel_in[2];
    *pixel_out++ = pixel_in[1];
    *pixel_out++ = pixel_in[0];
    pixel_in += 4;
  }

  return ret;
}

std::shared_ptr<uint8_t[]> ARGBToRGBA(const void* src, uint32_t src_size) {
  auto ret = std::shared_ptr<uint8_t[]>(new uint8_t[src_size]);

  auto pixel_in = static_cast<const uint8_t*>(src);
  auto pixel_out = ret.get();

  for (uint32_t i = 0; i < src_size / 4; ++i) {
    *pixel_out++ = pixel_in[1];
    *pixel_out++ = pixel_in[2];
    *pixel_out++ = pixel_in[3];
    *pixel_out++ = pixel_in[0];
    pixel_in += 4;
  }

  return ret;
}

uint32_t EncodeDDS(std::vector<uint8_t>& encoded_data, const void* input,
                   uint32_t input_len, uint32_t width, uint32_t height,
                   DXTCompression compression) {
  uint32_t fourcc;
  uint32_t block_size;
  uint32_t pixel_format_flags = DDPF_FOURCC;
  switch (compression) {
    case DXTCompression::DXT1:
      fourcc = kDXT1Magic;
      block_size = 8;
      break;
    case DXTCompression::DXT3:
      fourcc = kDXT3Magic;
      block_size = 16;
      pixel_format_flags |= DDPF_ALPHAPIXELS;
      break;
    case DXTCompression::DXT5:
      fourcc = kDXT5Magic;
      block_size = 16;
      pixel_format_flags |= DDPF_ALPHAPIXELS;
      break;
    default:
      return 1;
  }

  uint32_t block_width = (width + 3) / 4;
  uint32_t block_height = (height + 3) / 4;
  uint32_t pitch = block_width * block_size;
  uint32_t expected_data_size = pitch * block_height;

  // TODO: Support volumetric textures.
  uint32_t depth = 0;
  // TODO: Support mipmaps.
  uint32_t mipmam_count = 1;

  uint32_t rgb_bit_count = 0;
  uint32_t r_bit_mask = 0;
  uint32_t g_bit_mask = 0;
  uint32_t b_bit_mask = 0;
  uint32_t a_bit_mask = 0;

  DDS_HEADER header{
      sizeof(DDS_HEADER),
      DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_LINEARSIZE,
      height,
      width,
      pitch,
      depth,
      mipmam_count,
      {0},
      // ddspf
      {
          sizeof(DDS_PIXELFORMAT),
          pixel_format_flags,
          fourcc,
          rgb_bit_count,
          r_bit_mask,
          g_bit_mask,
          b_bit_mask,
          a_bit_mask,
      },
      DDSCAPS_TEXTURE,
      0,
      0,
      0,
      0,
  };

  encoded_data.clear();
  encoded_data.resize(4 + sizeof(header) + input_len);

  auto write_ptr = encoded_data.data();

  *write_ptr++ = 'D';
  *write_ptr++ = 'D';
  *write_ptr++ = 'S';
  *write_ptr++ = ' ';

  memcpy(write_ptr, &header, sizeof(header));
  write_ptr += sizeof(header);

  memcpy(write_ptr, input, input_len);

  return 0;
}
