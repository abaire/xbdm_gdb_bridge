#!/usr/bin/env python3

"""Fixes up hardware dump surface/texture descriptions from buggy xbdm releases."""

from __future__ import annotations

import argparse
import json
import os
import sys
from argparse import Namespace


def _fix_artifacts(artifact_path: str):
    if not os.path.isdir(artifact_path):
        msg = "Artifact path must be a directory containing the output of an ntrc tracer frame"
        raise ValueError(msg)

    def description_path(surface_filename: str) -> str:
        filename = surface_filename[:-3] + "txt"
        return os.path.join(artifact_path, filename)

    def is_surface(filename: str) -> bool:
        return os.path.isfile(description_path(filename))

    def fix_description(description_path: str):
        with open(description_path) as infile:
            content = infile.read()

            # There was a bug in the description writer that generates invalid JSON.
            if content[0] == "[":
                content = f"{{\n{content[1:]}\n}}"

                if content[3:10] == "texture":
                    fixed = []
                    for line in content.split("\n"):
                        if line.startswith('"format":'):
                            fixed.append(line + ",")
                        elif line.startswith(('"control0"', '"control1"')):
                            accidental_hex_value = int(line[12:], 16)
                            fixed.append(f"{line[:12]}{accidental_hex_value},")
                        elif line.startswith('"control1_hex"'):
                            fixed.append(line[:-1])
                        else:
                            fixed.append(line)
                    content = "\n".join(fixed)

            description = json.loads(content)

        with open(description_path, "w") as outfile:
            json.dump(description, outfile, indent=2)

    for item in os.listdir(artifact_path):
        if not item.endswith(".bin"):
            continue
        if not is_surface(item):
            continue

        fix_description(description_path(item))


def _main(args: Namespace) -> int:
    artifact_path = os.path.abspath(os.path.expanduser(args.artifact_path))

    _fix_artifacts(artifact_path)
    return 0


if __name__ == "__main__":

    def _parse_args():
        parser = argparse.ArgumentParser()

        parser.add_argument(
            "artifact_path",
            help="Directory containing surface bin and txt files to process.",
        )

        return parser.parse_args()

    sys.exit(_main(_parse_args()))
