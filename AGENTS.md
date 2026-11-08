# Working style
- Work on small, focused tasks.
- Make code readable.
- Split code into files when it improves readability.
- Keep README.md short and easy for humans to read. Include only essential information.

# Usage efficiency
- Prioritize conserving usage over parallel speed.
- Work solo. Do not spawn sub-agents without my explicit approval.
- Prefer medium reasoning for routine coding; reserve higher effort for difficult design or debugging.
- Keep each task focused. Avoid extra cleanup, benchmarking, or reviews unless needed for the requested outcome.
- Read relevant files and sections; avoid repeatedly loading entire plans, logs, or status documents.
- Avoid duplicate reviews and validation while preserving necessary correctness checks.
- Keep project-status handoffs concise so new milestones can start in fresh chats.

# Git Commit
- Before committing, summarize the changes and validation, then wait for my approval.
- Use short, descriptive commit messages.

# Code
- Use GLM for vectors, coordinates, and geometry.
- Use Flecs as the authoritative actor store and simulation system library. Keep actor components/factories in `src/actors/`, simulation in `src/simulation/`, terrain in `src/world/`, SDL drawing in `src/render/`, and CPU effects in `src/effects/`.
- Keep UI, rendering, and game logic separate. Game logic must not depend on SDL or ImGui.
- Keep application-owned ImGui code in `src/gui/`: windows, menus, styling, input capture, context/backends, and GUI frame handling. Application forwards GUI calls through this layer.

# Build
- Use CMake presets and Ninja.
- Keep build definitions in the root CMakeLists.txt.

# Scope
- Do not modify vendored libraries in lib/ unless requested.
- Leave unrelated local changes untouched.

# Project context
- Read docs/project-status.md for current behavior, validation, and deferred review work.
