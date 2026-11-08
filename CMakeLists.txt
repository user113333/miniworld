cmake_minimum_required(VERSION 3.21)
project(miniworld LANGUAGES C CXX)

set(SDL_SHARED OFF CACHE BOOL "Build shared SDL libraries")
set(SDL_STATIC ON CACHE BOOL "Build static SDL libraries")
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build SDL test library")
add_subdirectory(lib/sdl EXCLUDE_FROM_ALL)

set(GLM_BUILD_LIBRARY OFF CACHE BOOL "Use header-only GLM")
add_subdirectory(lib/glm EXCLUDE_FROM_ALL)

set(IMGUI_DIR "${PROJECT_SOURCE_DIR}/lib/imgui")
find_package(Freetype REQUIRED)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/misc/freetype/imgui_freetype.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3)
target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_FREETYPE)
target_link_libraries(imgui PRIVATE Freetype::Freetype)

add_library(miniworld_world STATIC
    src/island_generator.cpp
    src/tree_placement.cpp
)
target_include_directories(miniworld_world PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(miniworld_world PUBLIC cxx_std_17)
target_link_libraries(miniworld_world PUBLIC glm::glm)

add_executable(miniworld
    src/main.cpp
    src/application.cpp
    src/application_ui.cpp
    src/map_renderer.cpp
    src/island_demo.cpp
    src/island_controls.cpp
    src/tree_renderer.cpp
)
target_compile_features(miniworld PRIVATE cxx_std_17)
target_link_libraries(miniworld PRIVATE imgui miniworld_world)

configure_file(
    ${PROJECT_SOURCE_DIR}/res/fonts/CozetteVector.ttf
    ${CMAKE_CURRENT_BINARY_DIR}/res/fonts/CozetteVector.ttf
    COPYONLY
)
configure_file(
    ${PROJECT_SOURCE_DIR}/res/fonts/Cozette-LICENSE
    ${CMAKE_CURRENT_BINARY_DIR}/res/fonts/Cozette-LICENSE
    COPYONLY
)

foreach(tree IN ITEMS tree1 tree2 tree3)
    configure_file(
        ${PROJECT_SOURCE_DIR}/res/map/${tree}.png
        ${CMAKE_CURRENT_BINARY_DIR}/res/map/${tree}.png
        COPYONLY
    )
endforeach()

include(CTest)
if(BUILD_TESTING)
    foreach(suite IN ITEMS hex_grid island_generator tree_placement seed_fixtures)
        add_executable(${suite}_tests tests/${suite}_tests.cpp)
        target_link_libraries(${suite}_tests PRIVATE miniworld_world)
        add_test(NAME world.${suite} COMMAND ${suite}_tests)
    endforeach()
    target_compile_definitions(seed_fixtures_tests PRIVATE
        TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
    )
endif()
