# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Validate!** is a C++20 header-first library for declarative data validation with assertion utilities. MIT license. Version 0.1.0, active development on the `devel` branch; `main` is stable.

## Build & Test

Primary workflow uses the scripts in `scripts/`:

```powershell
# Windows — core library only
.\scripts\build_and_test.ps1

# Windows — with Qt extensions (picks up QTDIR env var automatically, or pass -QtDir)
.\scripts\build_and_test.ps1 -QtDir "C:\Qt\6.9.2\msvc2022_64"

# Force CMake reconfigure (e.g. after changing CMake flags)
.\scripts\build_and_test.ps1 -Reconfigure

# Linux / macOS
./scripts/build_and_test.sh
./scripts/build_and_test.sh Debug /path/to/Qt6
```

Raw CMake is used only for special cases (e.g. custom generators or isolated builds):

```powershell
cmake -B build -G "Ninja Multi-Config"
cmake --build build --config Debug
ctest --test-dir build --output-on-failure
```

Run a single test binary directly (after building):

```powershell
.\build\tests\Debug\test_models.exe --gtest_filter="TestSuite.TestName"
```

## Qt Extensions

Qt support is optional (`VD_EXTENSION_QT_BASE=ON`). Scripts auto-detect Qt from `QTDIR` or an explicit flag. Do not set Qt DLL paths manually — the scripts resolve them at test time. On MSVC, test discovery is deferred to ctest runtime (`PRE_TEST`) to work around the DLL path issue.

## Code Style

Enforced by `.clang-format`. Run `clang-format -i <file>` on changed `.hxx`/`.cxx` files before committing.

- **Column limit**: 140 (wider than typical — accommodates long template chains)
- **Pointer alignment**: Left (`T* ptr`, not `T *ptr`)
- **Indentation**: 4 spaces, no tabs
- **Line endings**: CRLF (Windows-first project)
- **Template declarations**: Always break onto their own line
- **Namespace indentation**: None
- **No short blocks or functions on a single line**

## Project Conventions

- **Header-only preferred** — avoid `.cxx` files when a header-only approach is reasonable
- **File extensions**: source files use `.hxx` / `.cxx`, not `.hpp` / `.cpp`
- **`vd::require`**: assertion helper with `std::format` + `std::source_location`; debug-only variant available
- **`vd::not_null<T*>`**: compile-time validated (constexpr contexts) or terminates at runtime on null
- **CTRE vs `std::regex`**: prefer compile-time `vd::string_rules::regex<"pattern">` (CTRE) when the pattern is known at compile time; use `std::regex` only for runtime patterns

## Tests

GTest 1.14.0 via FetchContent (no separate install needed). 5 core test executables + 3 Qt test executables (built only when `VD_EXTENSION_QT_BASE=ON`). Test files live in `tests/` and are named `test_*.cxx`.

## Dependencies

- C++20 (required), CMake 3.23+
- Qt 6.x (optional, auto-detected from `QTDIR` or script flag)
- GTest 1.14.0 (FetchContent)
- CTRE (bundled in `src/inline_deps/`)
