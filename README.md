# V8 Build

Prebuilt V8 static libraries for embedding. Builds are automated via GitHub Actions and published as releases.

## Supported Platforms

| Platform | Architecture | Release | Debug |
|----------|--------------|---------|-------|
| Linux | x86_64, aarch64 | Yes | Yes |
| macOS | aarch64 | Yes | Yes |
| Windows | x86_64 | Yes | No |

## Using Prebuilt Binaries

### CMake

```cmake
include(FetchContent)
FetchContent_Declare(
  v8
  GIT_REPOSITORY https://github.com/zyedidia/v8-build.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(v8)

target_link_libraries(your_target PRIVATE v8::v8)
```

To use a debug build:
```bash
cmake -B build -DV8_DEBUG=ON .
```

See `example/` for a complete "hello world" example.

## Building from Source

See the GitHub Actions workflow for full details.

### Prerequisites

- Python 3
- Git
- Platform-specific:
  - **Linux**: pkg-config, libglib2.0-dev
  - **macOS**: Xcode command line tools
  - **Windows**: Visual Studio 2022

### Build

```bash
# Clone V8 and dependencies
python clone.py --v8-version 14.0.365.10

# Build (release)
python build.py --args-gn args.gn --target-cpu x64 --install v8-install

# Build (debug)
python build.py --args-gn args.gn --target-cpu x64 --debug --install v8-install
```

### Custom Build Configuration

The `args.gn` file contains GN build arguments. Flag-dependent arguments (`is_debug`, `target_cpu`, `symbol_level`, etc.) are automatically prepended by `build.py`.

See [V8's GN args](https://v8.dev/docs/build-gn) for available options.

## Output

The install directory contains:
- `libv8_monolith.a` (or `v8_monolith.lib` on Windows)
- `libv8_libbase.a`, `libv8_libplatform.a` (Linux only)
- `include/` - V8 headers
- `args.gn` - Build configuration used
