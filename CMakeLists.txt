cmake_minimum_required(VERSION 3.21)
project(miniworld LANGUAGES C CXX)

set(SDL_SHARED OFF CACHE BOOL "Build shared SDL libraries")
set(SDL_STATIC ON CACHE BOOL "Build static SDL libraries")
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build SDL test library")
add_subdirectory(lib/sdl EXCLUDE_FROM_ALL)

add_subdirectory(src)
