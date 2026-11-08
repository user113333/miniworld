#include "application.h"

#include <SDL3/SDL_main.h>

#include <cstdio>
#include <exception>

int main(int, char**)
{
    try {
        Application app;
        return app.run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Application failed: %s\n", error.what());
        return 1;
    }
}
