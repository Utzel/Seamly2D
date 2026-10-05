#!/usr/bin/env python3
#
# Builds the avatar body data for the 3D View from the MakeHuman assets that ship with MPFB2.
#
#     python scripts/avatar/build_avatar_data.py <cache dir>
#
# The script downloads the assets it needs from a pinned MPFB2 commit into the cache directory (once), and writes
# src/libs/vgarment/share/avatar/body.dat, which is committed so building Seamly2D never needs the network.
#
# Licenses: the MakeHuman base mesh and targets are CC0 1.0 (MPFB2's LICENSE.ASSETS.md). The JSON files that say
# how targets combine are MPFB2 configuration, GPLv3 like Seamly2D. Only the converted data is committed.
#
# body.dat layout, little-endian, the whole stream compressed the way Qt's qCompress() does it (a big-endian
# uncompressed size followed by a zlib stream), so the C++ side reads it with qUncompress():
#
#     char[4]  "SMBD"
#     u32      format version (1)
#     u32      vertex count, then f32[3 * count] positions in cm, y up, the figure facing +z
#     u32      skin vertex count: the first vertices are the skin, the rest belong to the joint helpers
#     u32      triangle count, then u32[3 * count] skin triangles, counter-clockwise seen from outside
#     u32      joint count, then per joint: name, u32 vertex count, u32[count] vertex indices
#     u32      target count, then per target: name, u32 vertex count, u32[count] vertex indices,
#              i16[3 * count] offsets in 1/100 cm
#
# A name is a u16 byte count followed by UTF-8.

import argparse
import gzip
import json
import struct
import sys
import urllib.request
import zlib
from pathlib import Path

MPFB_REPOSITORY = "makehumancommunity/mpfb2"
MPFB_COMMIT = "d0a32e57a7f915cb2f2b95410e2117648c7bbb7e"
DATA = "src/mpfb/data/"

FORMAT_VERSION = 1

# MakeHuman works in decimetres.
CM_PER_UNIT = 10.0

# Target offsets are stored as 1/100 cm in 16 bits, enough for +-327 cm.
OFFSET_SCALE = 100.0

MEASURE_FOLDERS = ("torso", "arms", "legs", "neck")


def wanted(path):
    """True for the MPFB2 files the avatar needs."""
    if not path.startswith(DATA):
        return path == "LICENSE.ASSETS.md"
    name = path[len(DATA):]
    folder, _, file_name = name.rpartition("/")
    if name in ("3dobjs/base.obj", "targets/target.json", "targets/macrodetails/macro.json",
                "mesh_metadata/basemesh_vertex_groups.json", "mesh_metadata/hm08_config.json"):
        return True
    if not file_name.endswith(".target.gz"):
        return False
    if folder == "targets/macrodetails":
        return True
    if folder == "targets/breast":
        return file_name.startswith("female-")
    if folder in ("targets/" + measure for measure in MEASURE_FOLDERS):
        return file_name.startswith("measure-")
    return False


def download(cache):
    """Fetches the wanted files of the pinned commit into the cache, skipping files already there."""
    tree_url = f"https://api.github.com/repos/{MPFB_REPOSITORY}/git/trees/{MPFB_COMMIT}?recursive=1"
    with urllib.request.urlopen(tree_url) as response:
        tree = json.load(response)
    if tree.get("truncated"):
        sys.exit("The repository listing was truncated, can't tell which files exist.")

    paths = [entry["path"] for entry in tree["tree"] if entry["type"] == "blob" and wanted(entry["path"])]
    total = 0
    for path in paths:
        target = cache / path
        if not target.exists():
            target.parent.mkdir(parents=True, exist_ok=True)
            url = f"https://raw.githubusercontent.com/{MPFB_REPOSITORY}/{MPFB_COMMIT}/{path}"
            with urllib.request.urlopen(url) as response:
                target.write_bytes(response.read())
        total += target.stat().st_size
    print(f"{len(paths)} files, {total / 1e6:.1f} MB in {cache}")
    return paths


def read_base_mesh(path):
    """Positions, skin quads and joint helper vertex sets of the MakeHuman base mesh."""
    positions = []
    faces = {}
    group = None
    for line in path.read_text().splitlines():
        if line.startswith("v "):
            positions.append(tuple(float(value) * CM_PER_UNIT for value in line.split()[1:4]))
        elif line.startswith("g "):
            group = line[2:].strip()
        elif line.startswith("f "):
            faces.setdefault(group, []).append([int(corner.split("/")[0]) - 1 for corner in line.split()[1:]])

    skin_quads = faces["body"]
    joints = {name[len("joint-"):]: sorted({index for face in quads for index in face})
              for name, quads in faces.items() if name.startswith("joint-")}
    return positions, skin_quads, joints


def read_target(path):
    """Vertex index to offset in cm, leaving out zero offsets."""
    offsets = {}
    for line in gzip.open(path, "rt").read().splitlines():
        if line and not line.startswith("#"):
            values = line.split()
            offset = tuple(float(value) * CM_PER_UNIT for value in values[1:4])
            if any(offset):
                offsets[int(values[0])] = offset
    return offsets


def pack_name(name):
    encoded = name.encode("utf-8")
    return struct.pack("<H", len(encoded)) + encoded


def convert(cache, output):
    data = cache / DATA
    positions, skin_quads, joints = read_base_mesh(data / "3dobjs/base.obj")

    # The skin keeps the base mesh numbering; joint helper vertices are appended after it.
    skin_count = 1 + max(index for quad in skin_quads for index in quad)
    kept = list(range(skin_count)) + sorted({index for indices in joints.values() for index in indices})
    new_index = {old: new for new, old in enumerate(kept)}

    triangles = []
    for a, b, c, d in skin_quads:
        triangles += [a, b, c, a, c, d]

    stream = bytearray(b"SMBD")
    stream += struct.pack("<I", FORMAT_VERSION)
    stream += struct.pack("<I", len(kept))
    for old in kept:
        stream += struct.pack("<3f", *positions[old])
    stream += struct.pack("<I", skin_count)
    stream += struct.pack("<I", len(triangles) // 3)
    stream += struct.pack(f"<{len(triangles)}I", *triangles)

    stream += struct.pack("<I", len(joints))
    for name in sorted(joints):
        indices = [new_index[old] for old in joints[name]]
        stream += pack_name(name) + struct.pack("<I", len(indices)) + struct.pack(f"<{len(indices)}I", *indices)

    target_files = sorted((data / "targets").rglob("*.target.gz"))
    stream += struct.pack("<I", len(target_files))
    for path in target_files:
        name = path.relative_to(data / "targets").as_posix()[:-len(".target.gz")]
        offsets = {new_index[old]: offset for old, offset in read_target(path).items() if old in new_index}
        indices = sorted(offsets)
        quantized = []
        for index in indices:
            quantized += [round(value * OFFSET_SCALE) for value in offsets[index]]
        stream += pack_name(name) + struct.pack("<I", len(indices))
        stream += struct.pack(f"<{len(indices)}I", *indices) + struct.pack(f"<{len(quantized)}h", *quantized)

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(struct.pack(">I", len(stream)) + zlib.compress(bytes(stream), 9))
    print(f"{len(kept)} vertices ({skin_count} skin), {len(triangles) // 3} triangles, {len(joints)} joints, "
          f"{len(target_files)} targets: {output} {output.stat().st_size / 1e6:.1f} MB")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cache", type=Path, help="directory to download the MPFB2 assets into")
    parser.add_argument("--download-only", action="store_true", help="stop after downloading")
    args = parser.parse_args()

    download(args.cache)
    if args.download_only:
        return

    repository = Path(__file__).resolve().parents[2]
    convert(args.cache, repository / "src/libs/vgarment/share/avatar/body.dat")


if __name__ == "__main__":
    main()
