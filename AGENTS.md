# Working style
- Work on small, focused tasks.
- Make code readable.
- Split code into files when it improves readability.
- Keep README.md short and easy for humans to read. Include only essential information.

# Git Commit
- Before committing, summarize the changes and validation, then wait for my approval.
- Use short, descriptive commit messages.

# Code
- Use GLM for vectors, coordinates, and geometry.
- Keep UI, rendering, and game logic separate. Game logic must not depend on SDL or ImGui.
- Preserve seeded output during readability-only refactors.

# Build
- Use CMake presets and Ninja.
- Keep build definitions in the root CMakeLists.txt.
- Build after code changes.
- Run affected CTest tests; `ctest --preset debug` runs the full regression suite.

# Scope
- Do not modify vendored libraries in lib/ unless requested.
- Leave unrelated local changes untouched.

# Project context
- Read docs/project-status.md for current behavior, validation, and deferred review work.
