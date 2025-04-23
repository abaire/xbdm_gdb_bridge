#!/usr/bin/env python3

# ruff: noqa: T201 `print` found

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from argparse import Namespace
from dataclasses import dataclass
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from typing import Any

from PIL import Image

HEX_CAPTURE = r"(0x[0-9a-fA-F]+)"

# '640 x 480 [pitch = 2560 (0xA00)], at 0x01308000, format 0x7, type: 0x21000001, swizzled: N'
SURFACE_DESCRIPTION_RE = re.compile(r".*format " + HEX_CAPTURE + r", type: " + HEX_CAPTURE + r", swizzled: ([NY])")

SURFACE_FORMAT_X1R5G5B5_Z1R5G5B5 = 0x02
SURFACE_FORMAT_X1R5G5B5_O1R5G5B5 = 0x03
SURFACE_FORMAT_A1R5G5B5 = 0x04
SURFACE_FORMAT_R5G6B5 = 0x05
SURFACE_FORMAT_X8R8G8B8_Z8R8G8B8 = 0x07
SURFACE_FORMAT_X8R8G8B8_O1Z7R8G8B8 = 0x08
SURFACE_FORMAT_X1A7R8G8B8_Z1A7R8G8B8 = 0x09
SURFACE_FORMAT_X1A7R8G8B8_O1A7R8G8B8 = 0x0A
SURFACE_FORMAT_X8R8G8B8_O8R8G8B8 = 0x0B
SURFACE_FORMAT_A8R8G8B8 = 0x0C

NV097_SET_TEXTURE_FORMAT_COLOR_SZ_Y8 = 0x00
NV097_SET_TEXTURE_FORMAT_COLOR_SZ_AY8 = 0x01
NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A1R5G5B5 = 0x02
NV097_SET_TEXTURE_FORMAT_COLOR_SZ_X1R5G5B5 = 0x03
NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A4R4G4B4 = 0x04
NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R5G6B5 = 0x05
NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8R8G8B8 = 0x06
NV097_SET_TEXTURE_FORMAT_COLOR_SZ_X8R8G8B8 = 0x07
NV097_SET_TEXTURE_FORMAT_COLOR_SZ_I8_A8R8G8B8 = 0x0B
NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT1_A1R5G5B5 = 0x0C
NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT23_A8R8G8B8 = 0x0E
NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT45_A8R8G8B8 = 0x0F
NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A1R5G5B5 = 0x10
NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R5G6B5 = 0x11
NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8R8G8B8 = 0x12
NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_Y8 = 0x13
NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R8B8 = 0x16
NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_G8B8 = 0x17
NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8 = 0x19
NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8Y8 = 0x1A
NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_AY8 = 0x1B
NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X1R5G5B5 = 0x1C
NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A4R4G4B4 = 0x1D
NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X8R8G8B8 = 0x1E
NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8 = 0x1F
NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8Y8 = 0x20
NV097_SET_TEXTURE_FORMAT_COLOR_LC_IMAGE_CR8YB8CB8YA8 = 0x24
NV097_SET_TEXTURE_FORMAT_COLOR_LC_IMAGE_YB8CR8YA8CB8 = 0x25
NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R6G5B5 = 0x27
NV097_SET_TEXTURE_FORMAT_COLOR_SZ_G8B8 = 0x28
NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R8B8 = 0x29
NV097_SET_TEXTURE_FORMAT_COLOR_SZ_DEPTH_Y16_FIXED = 0x2C
NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_X8_Y24_FIXED = 0x2E
NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_Y16_FIXED = 0x30
NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_Y16_FLOAT = 0x31
NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_Y16 = 0x35
NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8B8G8R8 = 0x3A
NV097_SET_TEXTURE_FORMAT_COLOR_SZ_B8G8R8A8 = 0x3B
NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R8G8B8A8 = 0x3C
NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8B8G8R8 = 0x3F
NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_B8G8R8A8 = 0x40
NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R8G8B8A8 = 0x41

# Maps raw format to pillow image format.
TEXTURE_FORMAT_8888 = {
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8R8G8B8: "BGRA",
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_X8R8G8B8: "BGRA",
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_I8_A8R8G8B8: "BGRA",
    NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8R8G8B8: "BGRA",
    NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X8R8G8B8: "BGRA",
    NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8B8G8R8: "BGRA",
    NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_B8G8R8A8: "RGBA",
    NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R8G8B8A8: "ABGR",
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8B8G8R8: "BGRA",
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_B8G8R8A8: "RGBA",
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R8G8B8A8: "BGRA",
    NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_X8_Y24_FIXED: "BGRA",
}

TEXTURE_FORMAT_DXT = {
    NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT1_A1R5G5B5,
    NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT23_A8R8G8B8,
    NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT45_A8R8G8B8,
}

SWIZZLED_FORMATS = {
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_Y8,
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_AY8,
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A1R5G5B5,
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_X1R5G5B5,
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A4R4G4B4,
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R5G6B5,
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8R8G8B8,
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_X8R8G8B8,
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_I8_A8R8G8B8,
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8,
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8Y8,
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R6G5B5,
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_G8B8,
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R8B8,
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_DEPTH_Y16_FIXED,
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8B8G8R8,
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_B8G8R8A8,
    NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R8G8B8A8,
}

TEXTURE_BORDER_SOURCE_TEXTURE = 0
TEXTURE_BORDER_SOURCE_COLOR = 1


@dataclass
class TextureFormat:
    """Encapsulates detailed information about a texture."""

    cubemap: bool
    border_source: int
    dimensionality: int
    color_format: int
    mipmap_levels: int
    base_size_u: int
    base_size_v: int
    base_size_p: int

    # NV_PGRAPH_TEXFMT0_DIMENSIONALITY          0x000000C0
    # NV_PGRAPH_TEXFMT0_COLOR                   0x00007F00
    # NV_PGRAPH_TEXFMT0_MIPMAP_LEVELS           0x000F0000
    # NV_PGRAPH_TEXFMT0_BASE_SIZE_U             0x00F00000
    # NV_PGRAPH_TEXFMT0_BASE_SIZE_V             0x0F000000
    # NV_PGRAPH_TEXFMT0_BASE_SIZE_P             0xF0000000

    @property
    def uses_pitch(self) -> bool:
        return self.color_format in (
            NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT1_A1R5G5B5,
            NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT23_A8R8G8B8,
            NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT45_A8R8G8B8,
            NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A1R5G5B5,
            NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R5G6B5,
            NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8R8G8B8,
            NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_Y8,
            NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R8B8,
            NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_G8B8,
            NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_AY8,
            NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X1R5G5B5,
            NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A4R4G4B4,
            NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X8R8G8B8,
            NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8,
            NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8Y8,
            NV097_SET_TEXTURE_FORMAT_COLOR_LC_IMAGE_CR8YB8CB8YA8,
            NV097_SET_TEXTURE_FORMAT_COLOR_LC_IMAGE_YB8CR8YA8CB8,
            NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_X8_Y24_FIXED,
            NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_Y16_FIXED,
            NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_Y16_FLOAT,
            NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_Y16,
            NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8B8G8R8,
            NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_B8G8R8A8,
            NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R8G8B8A8,
        )

    @classmethod
    def from_texture_format(cls, tex_format: int) -> TextureFormat:
        return cls(
            cubemap=((tex_format >> 2) & 0x01) != 0,
            border_source=(tex_format >> 3) & 0x01,
            dimensionality=(tex_format >> 6) & 0x03,
            color_format=(tex_format >> 8) & 0x7F,
            mipmap_levels=(tex_format >> 16) & 0x0F,
            base_size_u=(tex_format >> 20) & 0x0F,
            base_size_v=(tex_format >> 24) & 0x0F,
            base_size_p=(tex_format >> 28) & 0x0F,
        )

def generate_swizzle_masks(width: int, height: int, depth: int) -> tuple[int, int, int]:
    """
    Create masks representing the interleaving of each linear texture dimension (x, y, z).
    These can be used to map linear texture coordinates to a swizzled "Z" offset.
    For example, a 2D 8x32 texture needs 3 bits for x, and 5 bits for y:
    mask_x:  00010101
    mask_y:  11101010
    mask_z:  00000000
    for "Z": yyyxyxyx

    Args:
        width: The width of the texture/volume.
        height: The height of the texture/volume.
        depth: The depth of the texture/volume.

    Returns:
        A tuple containing the swizzle masks (mask_x, mask_y, mask_z).
    """
    x = 0
    y = 0
    z = 0
    bit = 1
    mask_bit = 1
    done = False

    while not done:
        done = True
        # The C code checks `bit < dimension`. This means if the dimension is, say, 8 (binary 1000),
        # it processes bits up to 4 (binary 0100). This effectively allocates bits for dimensions
        # up to the next power of 2 greater than or equal to the dimension.
        # A more common approach for Z-order is to allocate bits based on the *maximum* bits needed
        # across all dimensions (i.e., max(width, height, depth)). The C code's logic seems to
        # interleave bits as long as 'bit' is less than *any* dimension. Let's follow the C logic.

        if bit < width:
            x |= mask_bit
            mask_bit <<= 1
            done = False
        if bit < height:
            y |= mask_bit
            mask_bit <<= 1
            done = False
        if bit < depth:
            z |= mask_bit
            mask_bit <<= 1
            done = False
        bit <<= 1

    # In Python, assert is a statement
    # The check is that all mask bits are mutually exclusive and cover all bits set in the masks.
    if (x ^ y ^ z) != (mask_bit - 1):
        msg = "masks are not mutually exclusive or don't cover all bits"
        raise ValueError(msg)

    return x, y, z

def unswizzle_box(
    src_buf: bytearray | bytes | memoryview, # Swizzled source buffer
    width: int,
    height: int,
    depth: int,
    dst_buf: bytearray | memoryview, # Linear destination buffer
    row_pitch: int, # Bytes per row in dst_buf
    slice_pitch: int, # Bytes per slice (plane) in dst_buf
    bytes_per_pixel: int
):
    """
    Unswizzles raw pixel data from a Z-order (Morton) layout back to a linear layout.

    Args:
        src_buf: The source buffer containing swizzled pixel data.
                 Must be large enough to hold width * height * depth * bytes_per_pixel.
        width, height, depth: Dimensions of the texture/volume.
        dst_buf: The destination buffer for linear pixel data.
        row_pitch: The pitch (stride) of the destination buffer in bytes per row.
        slice_pitch: The pitch (stride) of the destination buffer in bytes per slice.
        bytes_per_pixel: The number of bytes per pixel/voxel.
    """
    if not isinstance(src_buf, (bytes, bytearray, memoryview)):
         print("Warning: src_buf is not a byte-like object. This function expects raw bytes.", file=sys.stderr)
    if not isinstance(dst_buf, (bytearray, memoryview)):
         print("Warning: dst_buf is not a mutable byte-like object (bytearray/memoryview). This function writes raw bytes.", file=sys.stderr)

    mask_x, mask_y, mask_z = generate_swizzle_masks(width, height, depth)

    # Map swizzled texture back to linear texture using swizzle masks.

    # Iterate through linear coordinates (x, y, z) to calculate their swizzled source offsets
    off_z = 0 # Swizzled z-offset (index component)
    # In C, src_buf was calculated using off_y + off_z scaled by bytes_per_pixel outside the y loop.
    # In Python, we calculate the base swizzled offset directly.
    # src_base_offset_for_slice = off_z * bytes_per_pixel # This is wrong, off_z is an index, not byte offset yet.

    for z in range(depth):
        off_y = 0 # Swizzled y-offset (index component)
        # In C, dst_buf was incremented by slice_pitch outside the y loop.
        # In Python, we calculate the linear offset directly using z and slice_pitch.
        dst_slice_start_offset = z * slice_pitch

        # In C, src_tmp was calculated using off_y + off_z scaled by bytes_per_pixel relative to src_buf (start of swizzled buffer).
        # Let's calculate the base swizzled byte offset for the current "linear" row/slice combination based on off_y and off_z.
        src_base_offset_for_row_slice = (off_y + off_z) * bytes_per_pixel


        for y in range(height):
            off_x = 0 # Swizzled x-offset (index component)
            # In C, dst_tmp was calculated using y * row_pitch relative to dst_buf (current slice start).
            # In Python, we calculate the linear offset directly using y and row_pitch.
            dst_row_start_offset = dst_slice_start_offset + y * row_pitch

            # In C, src was calculated using src_tmp + off_x scaled by bytes_per_pixel.
            # The total swizzled source byte offset will be (off_x + off_y + off_z) * bytes_per_pixel
            # Let's calculate the total swizzled byte offset directly.

            for x in range(width):
                # Calculate swizzled source byte offset
                src_byte_offset = src_base_offset_for_row_slice + off_x * bytes_per_pixel
                 # This calculation is simpler than the C code's two-step pointer arithmetic but equivalent.
                 # It's ( (off_y + off_z) * bytes_per_pixel ) + ( off_x * bytes_per_pixel )
                 # = (off_y + off_z + off_x) * bytes_per_pixel
                 # So, the total swizzled index is off_x + off_y + off_z

                src_byte_offset = (off_x + off_y + off_z) * bytes_per_pixel


                # Calculate linear destination byte offset
                dst_byte_offset = dst_row_start_offset + x * bytes_per_pixel


                # Copy bytes_per_pixel bytes from source to destination
                if src_byte_offset + bytes_per_pixel > len(src_buf):
                     print(f"Error: Source buffer too small at offset {src_byte_offset}", file=sys.stderr)
                     return # Or raise an exception
                if dst_byte_offset + bytes_per_pixel > len(dst_buf):
                     print(f"Error: Destination buffer too small at offset {dst_byte_offset}", file=sys.stderr)
                     return # Or raise an exception

                dst_buf[dst_byte_offset : dst_byte_offset + bytes_per_pixel] = \
                    src_buf[src_byte_offset : src_byte_offset + bytes_per_pixel]

                # Increment x offset using the bit trick
                off_x = (off_x - mask_x) & mask_x

            # Increment y offset using the bit trick
            off_y = (off_y - mask_y) & mask_y
            # Update the base swizzled offset for the next row within this slice
            # This is implicitly handled by the recalculation in the next iteration if we follow the direct index calculation.
            # Let's recalculate src_base_offset_for_row_slice based on the updated off_y for clarity.
            src_base_offset_for_row_slice = (off_y + off_z) * bytes_per_pixel


        # Increment z offset using the bit trick
        off_z = (off_z - mask_z) & mask_z
        # Update the base swizzled offset for the next slice
        # This is implicitly handled by the recalculation in the next iteration if we follow the direct index calculation.


def unswizzle_rect(src_buf, width, height, dst_buf, pitch, bpp):
    unswizzle_box(src_buf, width, height, 1, dst_buf, pitch, 0, bpp)


class Processor:
    def __init__(self, *, overwrite_existing: bool = False, output_no_alpha: bool = False):
        self._overwrite_existing = overwrite_existing
        self._output_no_alpha = output_no_alpha

    def process(self, artifact_path: str) -> int:
        artifacts = self._find_artifacts(artifact_path)
        for bin_file, description in artifacts.items():
            self._convert_artifact(os.path.join(artifact_path, bin_file), description)

        return 0

    def _find_artifacts(self, artifact_path: str) -> dict[str, dict[str, Any]]:
        if not os.path.isdir(artifact_path):
            msg = "Artifact path must be a directory containing the output of an ntrc tracer frame"
            raise ValueError(msg)

        def description_path(surface_filename: str) -> str:
            filename = surface_filename[:-3] + "txt"
            return os.path.join(artifact_path, filename)

        def is_surface(filename: str) -> bool:
            return os.path.isfile(description_path(filename))

        def load_description(description_path: str) -> dict[str, Any]:
            with open(description_path) as infile:
                content = infile.read()
                return json.loads(content)

        return {
            item: load_description(description_path(item))
            for item in os.listdir(artifact_path)
            if item.endswith(".bin") and is_surface(item)
        }

    @staticmethod
    def _no_alpha_filename(filename: str) -> str:
        return f"{filename[:-4]}-noalpha.png"

    def _write_png_argb(
        self, output_path: str, bin_path: str, width: int, height: int, pitch: int | None, byte_order: str = "BGRA", *, swizzled: bool = False
    ):
        with open(bin_path, "rb") as infile:
            pixel_data = infile.read()

        if pitch is None:
            pitch = 4 * width

        # A bug in older versions of xbdm may have truncated the bin file early. Pad with 0 bytes.
        expected_bytes = height * pitch
        missing_bytes = expected_bytes - len(pixel_data)
        if missing_bytes > 0:
            pixel_data += b"\0" * missing_bytes

        # if swizzled:
        #     dest_buf = bytearray(len(pixel_data))
        #     unswizzle_rect(pixel_data, width, height, dest_buf, pitch, 8)
        #     pixel_data = dest_buf

        img = Image.frombytes("RGBA", (width, height), pixel_data, "raw", byte_order, pitch)
        img.save(output_path, "PNG")

        if self._output_no_alpha:
            rgb = img.convert("RGB")
            rgb.save(self._no_alpha_filename(output_path), "PNG")

    def _convert_surface(self, output_path: str, bin_path: str, surface_description: dict[str, Any]) -> str | None:
        description_line = surface_description["description"]
        match = SURFACE_DESCRIPTION_RE.match(description_line)
        if not match:
            print(f"WARNING: Failed to parse surface description for {bin_path}: '{description_line}'")
            return None

        surface_format = int(match.group(1), 16)
        # swizzled = match.group(3) == "Y"

        if surface_format in (
            SURFACE_FORMAT_X8R8G8B8_Z8R8G8B8,
            SURFACE_FORMAT_X8R8G8B8_O8R8G8B8,
            SURFACE_FORMAT_X1A7R8G8B8_Z1A7R8G8B8,
            SURFACE_FORMAT_X1A7R8G8B8_O1A7R8G8B8,
            SURFACE_FORMAT_A8R8G8B8,
        ):
            result = self._write_png_argb(
                output_path,
                bin_path,
                surface_description["width"],
                surface_description["height"],
                surface_description["pitch"],
            )
            return output_path if result else None

        print(f"Unimplemented surface format 0x{surface_format:x}")
        return None

    def _convert_texture_with_format(
        self,
        output_path: str,
        bin_path: str,
        tex_format: TextureFormat,
        width: int,
        height: int,
        depth: int,
        pitch: int,
    ) -> str | None:
        # NV097_SET_TEXTURE_FORMAT_COLOR_SZ_Y8 = 0x00
        # NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_Y8 = 0x13
        # NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8 = 0x19
        # NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_AY8 = 0x1B
        # NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8 = 0x1F
        #
        # NV097_SET_TEXTURE_FORMAT_COLOR_SZ_AY8 = 0x01
        # NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8Y8 = 0x1A
        # NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R8B8 = 0x16
        # NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_G8B8 = 0x17
        # NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8Y8 = 0x20
        # NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R6G5B5 = 0x27
        # NV097_SET_TEXTURE_FORMAT_COLOR_SZ_G8B8 = 0x28
        # NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R8B8 = 0x29
        # NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_Y16 = 0x35
        # NV097_SET_TEXTURE_FORMAT_COLOR_SZ_DEPTH_Y16_FIXED = 0x2C
        # NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_Y16_FIXED = 0x30
        #
        # NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_Y16_FLOAT = 0x31
        #
        # NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A1R5G5B5 = 0x02
        # NV097_SET_TEXTURE_FORMAT_COLOR_SZ_X1R5G5B5 = 0x03
        # NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A4R4G4B4 = 0x04
        # NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R5G6B5 = 0x05
        # NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT1_A1R5G5B5 = 0x0C
        # NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A1R5G5B5 = 0x10
        # NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R5G6B5 = 0x11
        # NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X1R5G5B5 = 0x1C
        # NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A4R4G4B4 = 0x1D
        #
        # NV097_SET_TEXTURE_FORMAT_COLOR_LC_IMAGE_CR8YB8CB8YA8 = 0x24
        # NV097_SET_TEXTURE_FORMAT_COLOR_LC_IMAGE_YB8CR8YA8CB8 = 0x25

        if depth != 1:
            msg = "3d textures not yet implemented"
            raise NotImplementedError(msg)

        if tex_format.color_format in TEXTURE_FORMAT_8888:
            for layer in range(depth):
                del layer
                self._write_png_argb(
                    output_path,
                    bin_path,
                    width,
                    height,
                    pitch if tex_format.uses_pitch else None,
                    TEXTURE_FORMAT_8888[tex_format.color_format],
                    swizzled=tex_format.color_format in SWIZZLED_FORMATS,
                )
            return output_path

        print("WARNING: Unsupported format: ", output_path, tex_format, width, height, depth, pitch)
        return None

    def _convert_texture(self, output_path: str, bin_path: str, texture_description: dict[str, Any]) -> str | None:
        return self._convert_texture_with_format(
            output_path,
            bin_path,
            TextureFormat.from_texture_format(texture_description["format"]),
            texture_description["width"],
            texture_description["height"],
            texture_description["depth"],
            texture_description["pitch"],
        )

    def _convert_artifact(self, bin_file, description: dict[str, Any]) -> str | None:
        output_path = os.path.join(os.path.dirname(bin_file), f"{bin_file[:-3]}png")
        if os.path.isfile(output_path) and not self._overwrite_existing:
            return output_path

        if "surface" in description:
            return self._convert_surface(output_path, bin_file, description["surface"])
        if "texture" in description:
            return self._convert_texture(output_path, bin_file, description["texture"])

        print(f"WARNING: Unhandled surface type for {bin_file}:\n{description}\n")
        return None


def _main(args: Namespace) -> int:
    artifact_path = os.path.abspath(os.path.expanduser(args.artifact_path))

    processor = Processor(overwrite_existing=args.force, output_no_alpha=args.no_alpha)
    return processor.process(artifact_path)


if __name__ == "__main__":

    def _parse_args():
        parser = argparse.ArgumentParser()

        parser.add_argument(
            "artifact_path",
            help="Directory containing surface bin and txt files to process.",
        )

        parser.add_argument("--force", "-f", action="store_true", help="Overwrite existing PNG files")
        parser.add_argument(
            "--no-alpha",
            "-N",
            action="store_true",
            help="Produce a copy of any PNG files that include alpha with alpha stripped",
        )

        return parser.parse_args()

    sys.exit(_main(_parse_args()))
