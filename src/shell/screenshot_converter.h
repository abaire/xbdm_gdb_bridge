#ifndef XBDM_GDB_BRIDGE_SCREENSHOTCONVERTER_H
#define XBDM_GDB_BRIDGE_SCREENSHOTCONVERTER_H

#include <SDL.h>

#include <vector>

typedef struct TextureFormatInfo {
  SDL_PixelFormatEnum sdl_format{SDL_PIXELFORMAT_UNKNOWN};
  uint32_t xbox_format{0};
  uint16_t xbox_bpp{4};
  bool xbox_swizzled{false};
  bool xbox_linear{true};
  bool require_conversion{false};
  const char* name{nullptr};

  [[nodiscard]] bool IsValid() const {
    return sdl_format != SDL_PIXELFORMAT_UNKNOWN;
  }

  SDL_Surface* Convert(const std::vector<uint8_t>& pixels, uint32_t width,
                       uint32_t height) const;

} TextureFormatInfo;

const TextureFormatInfo& GetTextureFormatInfo(uint32_t nv_texture_format);

#endif  // XBDM_GDB_BRIDGE_SCREENSHOTCONVERTER_H
