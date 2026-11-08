# Miniworld - 2d colony simulator

## Build and run

Requires CMake 3.21+, Ninja, a C/C++ compiler, and SDL3 platform development
dependencies (such as X11 or Wayland headers on Linux).

```sh
git submodule update --init --recursive
cmake --preset debug
cmake --build --preset debug
./build/debug/src/miniworld
```

The app displays an ImGui "Hello, world!" window. Use its Quit button or close
the SDL window to exit.

