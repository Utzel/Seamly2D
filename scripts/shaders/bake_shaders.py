#!/usr/bin/env python3
#
# Bakes the compute shaders the 3D View's cloth solver runs on the graphics card.
#
#     python scripts/shaders/bake_shaders.py [path to qsb]
#
# Each GLSL compute shader in src/libs/vgarment/shaders is compiled with Qt's qsb tool (from the Qt Shader Tools
# module) into a .qsb file next to it, holding SPIR-V for Vulkan, HLSL for Direct3D, MSL for Metal and GLSL for
# OpenGL. The .qsb files are committed, so building Seamly2D never needs qsb; run this after changing a shader.
#
# Without a path, qsb is looked for on the PATH. On Windows, with the Windows SDK's fxc, the HLSL is compiled to
# bytecode too, so Direct3D doesn't have to compile it when a drape starts, which takes it seconds; bake the
# committed files that way.

import os
import shutil
import subprocess
import sys
from pathlib import Path

SHADERS = Path(__file__).resolve().parents[2] / "src" / "libs" / "vgarment" / "shaders"

# OpenGL 4.3 and OpenGL ES 3.1 are the first with compute shaders; Direct3D 11 and Metal 1.2 have them.
TARGETS = ["--glsl", "430,310 es", "--hlsl", "50", "--msl", "12"]


def find_fxc():
    """The Windows SDK's shader compiler: on the PATH, or in the newest SDK."""
    fxc = shutil.which("fxc")
    if fxc is not None:
        return Path(fxc)
    kits = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Windows Kits" / "10" / "bin"
    found = sorted(kits.glob("10.*/x64/fxc.exe")) if kits.is_dir() else []
    return found[-1] if found else None


def main():
    qsb = sys.argv[1] if len(sys.argv) > 1 else shutil.which("qsb")
    if qsb is None:
        sys.exit("qsb not found: give its path, it is in the bin directory of a Qt with the Shader Tools module")

    targets = list(TARGETS)
    environment = dict(os.environ)
    fxc = find_fxc()
    if fxc is not None:
        targets.append("--fxc")
        environment["PATH"] = str(fxc.parent) + os.pathsep + environment.get("PATH", "")
    else:
        print("fxc not found: Direct3D will compile the HLSL when a drape starts")

    sources = sorted(SHADERS.glob("*.comp"))
    if not sources:
        sys.exit(f"no compute shaders in {SHADERS}")
    for source in sources:
        baked = source.with_name(source.name + ".qsb")
        # qsb writes the file even when fxc fails, only without the Direct3D bytecode.
        result = subprocess.run([qsb, *targets, "-o", str(baked), str(source)], env=environment,
                                capture_output=True, text=True)
        output = result.stdout + result.stderr
        if result.returncode != 0 or "compilation failed" in output or "returned non-zero" in output:
            sys.exit(f"{source.name} didn't compile:\n{output}")
        print(f"{source.name} -> {baked.name}")


if __name__ == "__main__":
    main()
