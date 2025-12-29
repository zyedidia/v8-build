#!/usr/bin/env python3
"""Clone V8 and its dependencies using depot_tools."""

import os
import subprocess
import sys
import platform

V8_VERSION = os.environ.get("V8_VERSION")
if not V8_VERSION:
    print("Error: V8_VERSION environment variable is not set")
    print("Please set V8_VERSION to the desired V8 version tag (e.g., 14.2.231.17)")
    sys.exit(1)

# On Windows, use locally installed Visual Studio instead of downloading
if platform.system() == "Windows":
    os.environ["DEPOT_TOOLS_WIN_TOOLCHAIN"] = "0"


def run(cmd, cwd=None, env=None):
    """Run a command and exit on failure."""
    print(f"==> Running: {' '.join(cmd)}")
    merged_env = os.environ.copy()
    if env:
        merged_env.update(env)
    result = subprocess.run(cmd, cwd=cwd, env=merged_env)
    if result.returncode != 0:
        sys.exit(result.returncode)


def patch_crel_flag(v8_dir):
    """Remove --allow-experimental-crel flag from BUILD.gn.

    This flag breaks linking with older compiler toolchains.
    """
    build_gn = os.path.join(v8_dir, "build", "config", "compiler", "BUILD.gn")
    if not os.path.exists(build_gn):
        print(f"Warning: {build_gn} not found, skipping crel patch")
        return

    print("==> Removing --allow-experimental-crel flag from BUILD.gn...")
    with open(build_gn, "r") as f:
        content = f.read()

    # Remove the line that adds the crel flag
    patched = content.replace(
        '      cflags += [ "-Wa,--crel,--allow-experimental-crel" ]\n', ""
    )

    if patched != content:
        with open(build_gn, "w") as f:
            f.write(patched)
        print("==> Patched BUILD.gn to remove crel flag")
    else:
        print("==> crel flag not found or already removed")


def main():
    root_dir = os.path.dirname(os.path.abspath(__file__))
    depot_tools_dir = os.path.join(root_dir, "depot_tools")

    # Clone depot_tools if not present
    if not os.path.exists(depot_tools_dir):
        print("==> Cloning depot_tools...")
        run(["git", "clone",
             "https://chromium.googlesource.com/chromium/tools/depot_tools.git",
             depot_tools_dir])
    else:
        print("==> depot_tools already exists, skipping clone")

    # Add depot_tools to PATH
    is_windows = platform.system() == "Windows"
    if is_windows:
        path_sep = ";"
        fetch_cmd = os.path.join(depot_tools_dir, "fetch.bat")
        gclient_cmd = os.path.join(depot_tools_dir, "gclient.bat")
    else:
        path_sep = ":"
        fetch_cmd = "fetch"
        gclient_cmd = "gclient"
    env_path = depot_tools_dir + path_sep + os.environ.get("PATH", "")

    # Fetch V8 if not present
    v8_dir = os.path.join(root_dir, "v8")
    if not os.path.exists(v8_dir):
        print(f"==> Fetching V8...")
        run([fetch_cmd, "v8"], cwd=root_dir, env={"PATH": env_path})

    # Checkout specific version
    print(f"==> Checking out V8 version {V8_VERSION}...")
    run(["git", "checkout", V8_VERSION], cwd=v8_dir)

    # Sync dependencies
    print("==> Syncing dependencies...")
    run([gclient_cmd, "sync", "-D"], cwd=v8_dir, env={"PATH": env_path})

    # Patch BUILD.gn to remove crel flag that breaks older toolchains
    patch_crel_flag(v8_dir)

    print("==> V8 clone complete!")


if __name__ == "__main__":
    main()
