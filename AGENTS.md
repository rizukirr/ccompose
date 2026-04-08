# Repository Guidelines

## Project Structure & Module Organization
- `src/`: library implementation (`ccompose.c`).
- `include/`: public headers (`ccompose.h`, vendored `clay.h`).
- `tests/`: headless CTest smoke tests (`smoke_test.c`).
- `examples/`: runnable sample app (`demo.c`).
- `renderer/raylib/`: bundled raylib renderer headers/sources.
- `external/clay/`: upstream Clay dependency (treat as vendored; avoid local edits unless updating vendor code intentionally).
- `docs/wiki/`: feature and API usage notes.

## Build, Test, and Development Commands
- Configure: `cmake -S . -B build`
- Build all: `cmake --build build`
- Run tests: `ctest --test-dir build --output-on-failure`
- Run example: `./build/examples/ccompose_demo`
- Headless build (CI-friendly): `cmake -S . -B build -DCCOMPOSE_BACKEND_RAYLIB=OFF`
- Disable optional targets as needed:
  - `-DCCOMPOSE_BUILD_TESTS=OFF`
  - `-DCCOMPOSE_BUILD_EXAMPLES=OFF`

## Coding Style & Naming Conventions
- Language: C11+ (configured via `CCOMPOSE_C_STANDARD`, defaults to `11`).
- Indentation: 4 spaces; no tabs.
- Naming:
  - Public API: `CC_*` (functions/types/macros).
  - Internal file-local symbols: `cc__*` static helpers/state.
  - Files: lowercase snake case (`ccompose.c`, `smoke_test.c`).
- Prefer small, focused functions and direct use of existing Clay/ccompose helpers before adding abstractions.

## Testing Guidelines
- Framework: CTest + C assertions (`assert`).
- Add/extend tests in `tests/` and register them in `CMakeLists.txt` with `add_test(...)`.
- Keep tests deterministic and headless when possible (`CCOMPOSE_NO_BACKEND` path).
- Naming: `<feature>_test.c` or `<scope>_smoke_test.c`.

## Commit & Pull Request Guidelines
- Follow the Lore commit protocol used in this repository:
  - Subject explains **why**.
  - Include trailers when relevant: `Constraint:`, `Rejected:`, `Confidence:`, `Scope-risk:`, `Directive:`, `Tested:`, `Not-tested:`.
- Keep commits small and reversible; avoid mixing refactors with behavior changes.
- PRs should include:
  - concise problem/solution summary,
  - verification evidence (build + `ctest` output),
  - linked issue(s), and
  - screenshots/GIFs for UI-visible example changes.

## Security & Configuration Tips
- Do not commit generated build artifacts from `build/`.
- Prefer `CCOMPOSE_BACKEND_RAYLIB=OFF` for CI/headless environments.
- When touching `external/clay/` or `renderer/raylib/`, document version/source rationale in the PR.
