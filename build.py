#!/usr/bin/env python3
"""Build V8 as a static library."""

import argparse
import os
import subprocess
import sys
import platform
import shutil

# On Windows, use locally installed Visual Studio instead of downloading
if platform.system() == "Windows":
    os.environ["DEPOT_TOOLS_WIN_TOOLCHAIN"] = "0"


def parse_args():
    parser = argparse.ArgumentParser(description="Build V8 as a static library")
    parser.add_argument("-C", dest="directory", metavar="DIR",
                        help="Build in this directory instead of the script directory")
    parser.add_argument("--debug", action="store_true",
                        help="Build debug version (not supported on Windows)")
    parser.add_argument("--install", dest="install_dir", metavar="DIR",
                        help="Install V8 libraries and headers to this directory")
    parser.add_argument("--target-cpu", choices=["x64", "arm64"],
                        help="Target CPU architecture (for cross-compilation)")
    parser.add_argument("--args-gn", dest="args_gn", metavar="FILE", required=True,
                        help="Path to args.gn file with GN build arguments")
    return parser.parse_args()


def run(cmd, cwd=None, env=None):
    """Run a command and exit on failure."""
    print(f"==> Running: {' '.join(cmd)}")
    merged_env = os.environ.copy()
    if env:
        merged_env.update(env)
    result = subprocess.run(cmd, cwd=cwd, env=merged_env)
    if result.returncode != 0:
        sys.exit(result.returncode)


def get_target_os():
    """Get the target OS name for V8."""
    system = platform.system()
    if system == "Linux":
        return "linux"
    elif system == "Darwin":
        return "mac"
    elif system == "Windows":
        return "win"
    else:
        print(f"Unsupported OS: {system}")
        sys.exit(1)


def get_target_cpu():
    """Get the target CPU architecture for V8."""
    machine = platform.machine().lower()
    if machine in ("x86_64", "amd64"):
        return "x64"
    elif machine in ("aarch64", "arm64"):
        return "arm64"
    else:
        print(f"Unsupported architecture: {machine}")
        sys.exit(1)


def install_sysroot(v8_dir, arch):
    """Install sysroot for cross-compilation."""
    # Map V8 arch names to sysroot arch names
    arch_map = {"arm64": "arm64", "x64": "amd64"}
    sysroot_arch = arch_map.get(arch, arch)

    sysroot_path = os.path.join(v8_dir, "build", "linux", f"debian_bullseye_{sysroot_arch}-sysroot")

    if os.path.isdir(sysroot_path):
        print(f"==> Sysroot already exists: {sysroot_path}")
        return

    print(f"==> Installing sysroot for {sysroot_arch}...")
    script_path = os.path.join(v8_dir, "build", "linux", "sysroot_scripts", "install-sysroot.py")
    run(["python3", script_path, f"--arch={sysroot_arch}"])


def download_clang(v8_dir, root_dir):
    """Download Chromium's clang toolchain."""
    clang_base_path = os.path.join(root_dir, "third_party", "llvm-build")

    # Check if clang is already downloaded
    clang_bin = os.path.join(clang_base_path, "Release+Asserts", "bin", "clang")
    if os.path.isfile(clang_bin):
        print(f"==> Clang already exists: {clang_base_path}")
        return clang_base_path

    print("==> Downloading Chromium's clang...")
    script_path = os.path.join(v8_dir, "tools", "clang", "scripts", "update.py")
    # Run from v8 directory (script expects to be run from there)
    run(["python3", script_path, "--output-dir", clang_base_path], cwd=v8_dir)

    return clang_base_path


def convert_thin_archive(archive_path):
    """Convert a thin archive to a regular archive.

    V8 builds thin archives on Linux which only contain references to object
    files. This converts them to regular archives that are self-contained.
    """
    print(f"==> Converting thin archive: {archive_path}")
    # List members of the thin archive
    result = subprocess.run(["ar", "-t", archive_path],
                            capture_output=True, text=True, check=True)
    members = result.stdout.strip().split('\n')

    # Create a new archive with the actual object files
    new_archive = archive_path + ".new"
    cmd = ["ar", "rvs", new_archive] + members
    subprocess.run(cmd, cwd=os.path.dirname(archive_path), check=True)

    # Replace the original
    shutil.move(new_archive, archive_path)


def install_v8(v8_dir, out_path, install_dir, target_os):
    """Install V8 libraries and headers to the specified directory."""
    print(f"==> Installing V8 to {install_dir}...")

    os.makedirs(install_dir, exist_ok=True)

    obj_dir = os.path.join(out_path, "obj")

    # Copy the monolith library
    if target_os == "win":
        lib_name = "v8_monolith.lib"
    else:
        lib_name = "libv8_monolith.a"

    lib_src = os.path.join(obj_dir, lib_name)
    lib_dst = os.path.join(install_dir, lib_name)
    print(f"    Copying {lib_name}")
    shutil.copy2(lib_src, lib_dst)

    # On Linux, also copy libbase and libplatform (after converting from thin archives)
    if target_os == "linux":
        for extra_lib in ["libv8_libbase.a", "libv8_libplatform.a"]:
            lib_src = os.path.join(obj_dir, extra_lib)
            if os.path.exists(lib_src):
                convert_thin_archive(lib_src)
                lib_dst = os.path.join(install_dir, extra_lib)
                print(f"    Copying {extra_lib}")
                shutil.copy2(lib_src, lib_dst)

    # Copy include directory
    include_src = os.path.join(v8_dir, "include")
    include_dst = os.path.join(install_dir, "include")
    print(f"    Copying include/")
    if os.path.exists(include_dst):
        shutil.rmtree(include_dst)
    shutil.copytree(include_src, include_dst)

    # Copy args.gn so users can see the build configuration
    args_gn_src = os.path.join(out_path, "args.gn")
    args_gn_dst = os.path.join(install_dir, "args.gn")
    if os.path.exists(args_gn_src):
        print(f"    Copying args.gn")
        shutil.copy2(args_gn_src, args_gn_dst)

    print(f"==> Installation complete: {install_dir}")


def main():
    args = parse_args()

    if args.directory:
        root_dir = os.path.abspath(args.directory)
    else:
        root_dir = os.path.dirname(os.path.abspath(__file__))

    v8_dir = os.path.join(root_dir, "v8")
    depot_tools_dir = os.path.join(root_dir, "depot_tools")

    if not os.path.exists(v8_dir):
        print("Error: v8 directory not found. Run clone.py first.")
        sys.exit(1)

    # Add depot_tools to PATH and get command names
    is_windows = platform.system() == "Windows"
    if is_windows:
        path_sep = ";"
        gn_cmd = os.path.join(depot_tools_dir, "gn.bat")
        ninja_cmd = os.path.join(depot_tools_dir, "ninja.bat")
    else:
        path_sep = ":"
        gn_cmd = "gn"
        ninja_cmd = "ninja"
    env_path = depot_tools_dir + path_sep + os.environ.get("PATH", "")

    target_os = get_target_os()
    target_cpu = args.target_cpu or get_target_cpu()

    # Debug builds not supported on Windows
    is_debug = args.debug and target_os != "win"
    if args.debug and target_os == "win":
        print("Warning: Debug builds not supported on Windows, using release")

    build_type = "debug" if is_debug else "release"
    print(f"==> Building V8 for {target_os}-{target_cpu} ({build_type})...")

    out_dir = f"out.gn/{target_os}-{target_cpu}-{build_type}"
    out_path = os.path.join(v8_dir, out_dir)

    # Download Chromium's clang
    # This avoids Xcode SDK issues on macOS and ensures consistent toolchain
    clang_base_path = download_clang(v8_dir, root_dir)
    clang_base_path_abs = os.path.abspath(clang_base_path)
    print(f"==> Using clang at: {clang_base_path_abs}")

    # Verify clang binary exists
    clang_bin = os.path.join(clang_base_path_abs, "Release+Asserts", "bin", "clang")
    if not os.path.isfile(clang_bin):
        print(f"WARNING: Clang binary not found at {clang_bin}")
        print("The clang download may have failed. Build may use system clang instead.")

    # Flag-dependent GN arguments (prepended to args.gn)
    gn_args_prefix = [
        f"is_debug={'true' if is_debug else 'false'}",
        f'target_cpu="{target_cpu}"',
        f'v8_target_cpu="{target_cpu}"',
        f"symbol_level={'1' if is_debug else '0'}",
        f'clang_base_path="{clang_base_path_abs}"',
        "is_component_build=false",
        "v8_monolithic=true",
        "treat_warnings_as_errors=false",
        "clang_use_chrome_plugins=false",
        "is_clang=true",
        "use_custom_libcxx=false",
    ]

    # Platform-specific arguments
    if target_os == "linux":
        host_cpu = get_target_cpu()
        is_cross_compile = target_cpu != host_cpu
        if is_cross_compile:
            print(f"==> Cross-compiling from {host_cpu} to {target_cpu}")
        gn_args_prefix.append("use_sysroot=false")

    # Read args.gn file
    print(f"==> Using args.gn: {args.args_gn}")
    with open(args.args_gn, "r") as f:
        custom_args = f.read()

    # Combine prefix args with custom args
    gn_args_str = "\n".join(gn_args_prefix) + "\n" + custom_args

    # Run gn gen
    print("==> Running gn gen...")
    run([gn_cmd, "gen", out_dir, f"--args={gn_args_str}"],
        cwd=v8_dir, env={"PATH": env_path})

    # Build with ninja
    print("==> Building with ninja...")
    run([ninja_cmd, "-C", out_dir, "v8_monolith"],
        cwd=v8_dir, env={"PATH": env_path})

    print("==> Build complete!")

    # Show output
    if target_os == "win":
        lib_name = "v8_monolith.lib"
    else:
        lib_name = "libv8_monolith.a"

    lib_path = os.path.join(out_path, "obj", lib_name)
    if os.path.exists(lib_path):
        size = os.path.getsize(lib_path)
        print(f"\nStatic library: {lib_path}")
        print(f"Size: {size / (1024*1024):.1f} MB")
    else:
        print(f"\nWarning: Expected library not found at {lib_path}")
        print("Check the output directory for the built library.")

    print(f"Headers location: {os.path.join(v8_dir, 'include')}")

    # Install if requested
    if args.install_dir:
        install_dir = os.path.abspath(args.install_dir)
        install_v8(v8_dir, out_path, install_dir, target_os)


if __name__ == "__main__":
    main()
