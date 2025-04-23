#ifndef IMAGE_UTIL_H
#define IMAGE_UTIL_H

#include <cstdint>
#include <memory>
#include <vector>

std::shared_ptr<uint8_t[]> RGB565ToRGB88(const void *src, uint32_t src_size);
std::shared_ptr<uint8_t[]> AXR5G5B5ToRGB888(const void *src, uint32_t src_size);
std::shared_ptr<uint8_t[]> A1R5G5B5ToRGBA888(const void *src,
                                             uint32_t src_size);
std::shared_ptr<uint8_t[]> A4R4G4B4ToRGBA888(const void *src,
                                             uint32_t src_size);

//! Swaps the red and blue channels for a BGRA8888 image.
std::shared_ptr<uint8_t[]> BGRAToRGBA(const void *src, uint32_t src_size);
std::shared_ptr<uint8_t[]> ABGRToRGBA(const void *src, uint32_t src_size);
std::shared_ptr<uint8_t[]> ARGBToRGBA(const void *src, uint32_t src_size);

enum class DXTCompression {
  INVALID,
  DXT1,
  DXT3,
  DXT5,
};

uint32_t EncodeDDS(std::vector<uint8_t> &encoded_data, const void *input,
                   uint32_t input_len, uint32_t width, uint32_t height,
                   DXTCompression compression);

#endif  // IMAGE_UTIL_H
