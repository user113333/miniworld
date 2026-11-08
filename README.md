# Miniworld - 2d colony simulator

## Build and run

Requires CMake 3.21+, Ninja, a C/C++ compiler, and SDL3 platform dependencies.

```sh
git submodule update --init --recursive
cmake --preset debug
cmake --build --preset debug
./build/debug/src/miniworld
```

## CLion

Open the root `CMakeLists.txt`. Enable the `debug` CMake profile, reload,
then run `Miniworld`.

