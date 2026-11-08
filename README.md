# Miniworld - 2d colony simulator

## Build, test and run

Requires CMake 3.21+, Ninja, a C/C++ compiler, FreeType development files,
and SDL3 platform dependencies.

```sh
git submodule update --init --recursive
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./cmake-build-debug/miniworld
```

## CLion

Open the root `CMakeLists.txt`. Enable the `debug` CMake profile, reload,
then run `miniworld`.

[Project notes](docs/project-status.md)

