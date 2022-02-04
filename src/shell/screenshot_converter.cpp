#include "screenshot_converter.h"

#include <iostream>

const TextureFormatInfo kInvalidTextureFormatInfo{};

#define NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8R8G8B8 0x12
#define NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X8R8G8B8 0x1E

// clang-format off
constexpr TextureFormatInfo kTextureFormats[] = {
{SDL_PIXELFORMAT_ARGB8888, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8R8G8B8, 4, false, false, "X8R8G8B8"},
// TOOD: Rename to SDL_PIXELFORMAT_XRGB8888 when the GitHub ubuntu image is
// updated such that SDL2 >= 2.0.14.
{SDL_PIXELFORMAT_RGB888, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X8R8G8B8, 4, false, false, "X8R8G8B8"},
};
// clang-format on

constexpr int kNumFormats =
    sizeof(kTextureFormats) / sizeof(kTextureFormats[0]);

const TextureFormatInfo &GetTextureFormatInfo(uint32_t nv_texture_format) {
  for (const auto &info : kTextureFormats) {
    if (info.xbox_format == nv_texture_format) {
      return info;
    }
  }
  return kInvalidTextureFormatInfo;
}

SDL_Surface *TextureFormatInfo::Convert(const std::vector<uint8_t> &pixels,
                                        uint32_t width, uint32_t height) const {
  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(
      0, (int)width, (int)height, xbox_bpp * 4, sdl_format);
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
