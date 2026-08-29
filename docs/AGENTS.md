# Project Instructions

This project is a custom GTNH server launcher based on Fjord Launcher.

## Project goals

- Windows x64 only.
- GTNH Java 17-25 compatibility must be preserved.
- Java 25 will be used to launch GTNH.
- The launcher will eventually use Nide8 authentication.
- The launcher will have a custom client auto-update system.
- This is a permanently forked version of Fjord Launcher.
- We do not need to maintain compatibility with future Fjord or Prism updates.

## Important constraints

- Do not break Prism/Fjord mmc-pack.json and patches version resolution.
- Do not change GTNH launch semantics unless explicitly requested.
- Preserve Java detection and launch infrastructure.
- Prefer existing Fjord networking/task infrastructure instead of introducing unnecessary new frameworks.
- Build target is Windows x64 MSVC.
- After modifying C++ code, compile the project and fix compilation errors before considering the task complete.

## Development environment

Build:

cmake --build --preset windows_msvc --config Debug

Qt:

F:\QT\6.11.2\msvc2022_64

vcpkg:

D:\PCL\vcpkg