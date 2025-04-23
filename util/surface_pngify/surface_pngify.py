#!/usr/bin/env python3

# ruff: noqa: T201 `print` found

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from argparse import Namespace
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from typing import Any

from PIL import Image

HEX_CAPTURE = r"(0x[0-9a-fA-F]+)"

# '640 x 480 [pitch = 2560 (0xA00)], at 0x01308000, format 0x7, type: 0x21000001, swizzled: N'
SURFACE_DESCRIPTION_RE = re.compile(r".*format " + HEX_CAPTURE + r", type: " + HEX_CAPTURE + r", swizzled: ([NY])")

NV097_SET_SURFACE_FORMAT_COLOR_LE_X1R5G5B5_Z1R5G5B5 = 0x01
NV097_SET_SURFACE_FORMAT_COLOR_LE_X1R5G5B5_O1R5G5B5 = 0x02
NV097_SET_SURFACE_FORMAT_COLOR_LE_R5G6B5 = 0x03
NV097_SET_SURFACE_FORMAT_COLOR_LE_X8R8G8B8_Z8R8G8B8 = 0x04
NV097_SET_SURFACE_FORMAT_COLOR_LE_X8R8G8B8_O8R8G8B8 = 0x05
NV097_SET_SURFACE_FORMAT_COLOR_LE_X1A7R8G8B8_Z1A7R8G8B8 = 0x06
NV097_SET_SURFACE_FORMAT_COLOR_LE_X1A7R8G8B8_O1A7R8G8B8 = 0x07
NV097_SET_SURFACE_FORMAT_COLOR_LE_A8R8G8B8 = 0x08
NV097_SET_SURFACE_FORMAT_COLOR_LE_B8 = 0x09
NV097_SET_SURFACE_FORMAT_COLOR_LE_G8B8 = 0x0A
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
        self, output_path: str, bin_path: str, width: int, height: int, pitch: int, byte_order: str = "BGRA"
    ) -> bool:
        with open(bin_path, "rb") as infile:
            pixel_data = infile.read()

        # A bug in older versions of xbdm may have truncated the bin file early. Pad with 0 bytes.
        expected_bytes = height * pitch
        missing_bytes = expected_bytes - len(pixel_data)
        if missing_bytes > 0:
            pixel_data += b"\0" * missing_bytes

        img = Image.frombytes("RGBA", (width, height), pixel_data, "raw", byte_order, pitch)
        img.save(output_path, "PNG")

        if self._output_no_alpha:
            rgb = img.convert("RGB")
            rgb.save(self._no_alpha_filename(output_path), "PNG")

        return True

    def _convert_surface(self, output_path: str, bin_path: str, surface_description: dict[str, Any]) -> str | None:
        description_line = surface_description["description"]
        match = SURFACE_DESCRIPTION_RE.match(description_line)
        if not match:
            print(f"WARNING: Failed to parse surface description for {bin_path}: '{description_line}'")
            return None

        surface_format = int(match.group(1), 16)
        print(f"{bin_path}: {match.group(1)}")
        # swizzled = match.group(3) == "Y"

        if surface_format in (
            NV097_SET_SURFACE_FORMAT_COLOR_LE_X8R8G8B8_Z8R8G8B8,
            NV097_SET_SURFACE_FORMAT_COLOR_LE_X8R8G8B8_O8R8G8B8,
            NV097_SET_SURFACE_FORMAT_COLOR_LE_X1A7R8G8B8_Z1A7R8G8B8,
            NV097_SET_SURFACE_FORMAT_COLOR_LE_X1A7R8G8B8_O1A7R8G8B8,
            NV097_SET_SURFACE_FORMAT_COLOR_LE_A8R8G8B8,
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

    def _convert_texture(self, output_path: str, bin_path: str, texture_description: dict[str, Any]) -> str | None:
        print(output_path, bin_path, texture_description)
        return None

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
