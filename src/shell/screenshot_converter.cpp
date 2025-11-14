#include "screenshot_converter.h"

#include <iostream>

const TextureFormatInfo kInvalidTextureFormatInfo{};

#define NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A1R5G5B5 0x10
#define NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R5G6B5 0x11
#define NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8R8G8B8 0x12
#define NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X1R5G5B5 0x1C
#define NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A4R4G4B4 0x1D
#define NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X8R8G8B8 0x1E
#define NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8B8G8R8 0x3F
#define NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_B8G8R8A8 0x40
#define NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R8G8B8A8 0x41

// clang-format off
constexpr TextureFormatInfo kTextureFormats[] = {
{SDL_PIXELFORMAT_ABGR8888, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8B8G8R8, 32, false, true, false, "A8B8G8R8"},
{SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R8G8B8A8, 32, false, true, false, "R8G8B8A8"},
{SDL_PIXELFORMAT_ARGB8888, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8R8G8B8, 32, false, true, false, "A8R8G8B8"},
// TOOD: Rename to SDL_PIXELFORMAT_XRGB8888 when the GitHub ubuntu image is
// updated such that SDL2 >= 2.0.14.
{SDL_PIXELFORMAT_RGB888, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X8R8G8B8, 32, false, true, false, "X8R8G8B8"},
{SDL_PIXELFORMAT_BGRA8888, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_B8G8R8A8, 32, false, true, false, "B8G8R8A8"},
{SDL_PIXELFORMAT_RGB565, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R5G6B5, 16, false, true, false, "R5G6B5"},
{SDL_PIXELFORMAT_ARGB1555, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A1R5G5B5, 16, false, true, false, "A1R5G5B5"},
{SDL_PIXELFORMAT_ARGB1555, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X1R5G5B5, 16, false, true, false, "X1R5G5B5"},
{SDL_PIXELFORMAT_ARGB4444, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A4R4G4B4, 16, false, true, false, "A4R4G4B4"},
};
// clang-format on

constexpr int kNumFormats =
    sizeof(kTextureFormats) / sizeof(kTextureFormats[0]);

const TextureFormatInfo& GetTextureFormatInfo(uint32_t nv_texture_format) {
  for (const auto& info : kTextureFormats) {
    if (info.xbox_format == nv_texture_format) {
      return info;
    }
  }
  return kInvalidTextureFormatInfo;
}

SDL_Surface* TextureFormatInfo::Convert(const std::vector<uint8_t>& pixels,
                                        uint32_t width, uint32_t height) const {
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
      0, (int)width, (int)height, xbox_bpp, sdl_format);
  if (!surface) {
    std::cout << "Failed to create source surface" << std::endl;
    return nullptr;
  }

  if (SDL_LockSurface(surface)) {
    std::cout << "Failed to lock source surface" << std::endl;
    SDL_FreeSurface(surface);
    return nullptr;
  }

  memcpy(surface->pixels, pixels.data(), surface->pitch * surface->h);
  SDL_UnlockSurface(surface);

  return surface;
}
