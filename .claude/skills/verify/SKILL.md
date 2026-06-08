---
name: verify
description: Build the project and run all tests using the project's build_and_test scripts; then check clang-format compliance on changed .hxx/.cxx files. Use after making code changes to confirm everything passes before committing.
disable-model-invocation: false
---

## Steps

1. **Build and run tests** using the appropriate script for the platform:
   - Windows (PowerShell): `.\scripts\build_and_test.ps1` (add `-QtDir "..."` if Qt extensions are relevant to the change)
   - Linux/macOS (Bash): `./scripts/build_and_test.sh`
   - Add `-Verbose` / `--verbose` if a test fails and you need full output to diagnose.

2. **Check clang-format** on any `.hxx` or `.cxx` files that were modified. Run:
   ```
   clang-format --dry-run --Werror <changed-files>
   ```
   If violations are found, apply them with `clang-format -i <file>` and note what was fixed.

3. Report the result: pass/fail per test suite and any format violations found.

## Notes

- Do not manually set Qt DLL paths — the scripts handle this automatically.
- If tests fail due to a missing build directory or stale CMake cache, re-run with `-Reconfigure` (Windows) or delete `build/` (Linux).
- Run a single test binary directly for faster iteration: `.\build\tests\Debug\test_<name>.exe --gtest_filter="Suite.Test"`
