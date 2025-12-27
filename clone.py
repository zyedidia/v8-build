#!/usr/bin/env python3
"""Clone V8 and its dependencies using depot_tools."""

import os
import subprocess
import sys
import platform

V8_VERSION = os.environ.get("V8_VERSION", "14.2.231.17")


def run(cmd, cwd=None, env=None):
    """Run a command and exit on failure."""
    print(f"==> Running: {' '.join(cmd)}")
    merged_env = os.environ.copy()
    if env:
        merged_env.update(env)
    result = subprocess.run(cmd, cwd=cwd, env=merged_env)
    if result.returncode != 0:
        sys.exit(result.returncode)


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
        fetch_cmd = "fetch.bat"
        gclient_cmd = "gclient.bat"
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

    print("==> V8 clone complete!")


if __name__ == "__main__":
    main()
