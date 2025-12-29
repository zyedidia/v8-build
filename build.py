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
    parser.add_argument("--debug", action="store_true",
                        help="Build debug version (not supported on Windows)")
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


def download_clang(v8_dir):
    """Download Chromium's clang toolchain."""
    root_dir = os.path.dirname(os.path.abspath(__file__))
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


def main():
    args = parse_args()

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
    # Allow override via environment variable for cross-compilation
    target_cpu = os.environ.get("TARGET_CPU") or get_target_cpu()

    # Debug builds not supported on Windows
    is_debug = args.debug and target_os != "win"
    if args.debug and target_os == "win":
        print("Warning: Debug builds not supported on Windows, using release")

    build_type = "debug" if is_debug else "release"
    print(f"==> Building V8 for {target_os}-{target_cpu} ({build_type})...")

    out_dir = f"out.gn/{target_os}-{target_cpu}-{build_type}"
    out_path = os.path.join(v8_dir, out_dir)

    # Base GN arguments for a static embeddable library
    gn_args = [
        f"is_debug={'true' if is_debug else 'false'}",
        f'target_cpu="{target_cpu}"',
        f'v8_target_cpu="{target_cpu}"',
        "is_component_build=false",
        "v8_monolithic=true",
        "v8_use_external_startup_data=false",
        "treat_warnings_as_errors=false",
        "v8_enable_sandbox=false",
        "v8_enable_pointer_compression=false",
        "v8_enable_i18n_support=false",
        "v8_enable_temporal_support=false",
        "enable_rust=false",
        "clang_use_chrome_plugins=false",
        f"symbol_level={'1' if is_debug else '0'}",
        "v8_enable_webassembly=true",
        "is_clang=true",
        "use_custom_libcxx=false",
    ]

    # Download Chromium's clang
    # This avoids Xcode SDK issues on macOS and ensures consistent toolchain
    clang_base_path = download_clang(v8_dir)
    # Use absolute path for clang_base_path
    clang_base_path_abs = os.path.abspath(clang_base_path)
    print(f"==> Using clang at: {clang_base_path_abs}")

    # Verify clang binary exists
    clang_bin = os.path.join(clang_base_path_abs, "Release+Asserts", "bin", "clang")
    if not os.path.isfile(clang_bin):
        print(f"WARNING: Clang binary not found at {clang_bin}")
        print("The clang download may have failed. Build may use system clang instead.")

    gn_args.append(f'clang_base_path="{clang_base_path_abs}"')

    # Platform-specific arguments
    if target_os == "linux":
        host_cpu = get_target_cpu()
        is_cross_compile = target_cpu != host_cpu
        if is_cross_compile:
            print(f"==> Cross-compiling from {host_cpu} to {target_cpu}")
            # Don't use sysroot - it has old libstdc++ without C++20 support
            # The cross-compilation toolchain provides the necessary headers
        gn_args.append("use_sysroot=false")

    # Use sccache if available
    sccache_path = shutil.which("sccache")
    if sccache_path:
        print(f"==> Using sccache: {sccache_path}")
        gn_args.append(f'cc_wrapper="{sccache_path}"')
    else:
        print("==> sccache not found, building without compilation cache")

    gn_args_str = " ".join(gn_args)

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


if __name__ == "__main__":
    main()
