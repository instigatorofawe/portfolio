# lookups

A C++23 command-line generator that precomputes the equity and matchup
lookup tables the push/fold solvers need, and emits them as source (Rust or
TypeScript). See the [repository root README](../README.md) for the full
build pipeline, cross-language dependencies, and Nx commands; this file
covers only what's specific to this package.

Solving push/fold at runtime means evaluating equities over 169 x 169 (or
169^3 for three-way) hand matchups, which is far too slow to do live in the
browser — this generator does that work once, ahead of time, and the
solvers just read the tables it produces.

## Layout

| Path                              | What it is                                                        |
| ---------------------------------- | ------------------------------------------------------------------- |
| `include/pushfold/`               | Header-only library: combinatorics (`Combinator`, `Binomial`), the `Deck`, `Infosets`, and per-game equity/matchup logic under `Headsup/` and `Threeway/` |
| `include/pushfold/Frontends/`     | `CppFrontend`, `RustFrontend`, `TypeScriptFrontend` — serialize a generator's output as source in each target language |
| `src/Headsup/generate.cpp`        | `generate_headsup_lookups` entry point (heads-up equity + matchup tables, and the hand-label grid) |
| `src/Threeway/generate.cpp`       | `generate_threeway_lookups` entry point (three-way equity + matchup tensors) |
| `bench/GenerateBench.cpp`         | Google Benchmark microbenchmarks for the generators                |
| `*Test.cpp` files (co-located under `include/`) | GoogleTest unit tests, discovered automatically via `CONFIGURE_DEPENDS` glob |

Every header pulls in [OMPEval](https://github.com/instigatorofawe/OMPEval)
(a fork with macOS fixes, fetched at configure time) for hand evaluation.

## Building

```
cmake -S lookups -B lookups/build -DCMAKE_BUILD_TYPE=Release
cmake --build lookups/build --target generate_headsup_lookups
cmake --build lookups/build --target generate_threeway_lookups
```

Both generators take an output directory and a frontend name:

```
lookups/build/generate_headsup_lookups <out-dir> {cpp,rust,ts}
lookups/build/generate_threeway_lookups <out-dir> {cpp,rust,ts}
```

`generate_headsup_lookups` additionally supports a `--hands` flag that emits
just the hand-label grid (used to generate `liuran/src/lib/generated/hands.ts`
without regenerating the full equity tables).

Normally you won't invoke these directly — the Nx targets in `project.json`
(`generate-lookups`, `generate-threeway-lookups`, `generate-hands`) wrap them
with the right output paths, and are what `pnpm lookups` at the repo root
runs.

**The three-way generator is slow — 40+ minutes** even on fast hardware
(enumerating ~819K three-way matchups via OMPEval). It is deliberately kept
out of CI and out of the default Nx build dependency graph — wiring it into
any `dependsOn` chain would mean paying that cost on every cold-cache build.
The repo instead relies on the committed output in
`pushfold/shared/src/generated/threeway/`; only run
`generate-threeway-lookups` (`pnpm lookups:threeway` from the repo root) by
hand when the three-way lookup logic itself changes, and commit the
regenerated tables.

## Testing and benchmarking

Tests and benchmarks are only configured when this directory is the
top-level CMake build (they're skipped when built as part of the parent
workspace):

```
cmake -S lookups -B lookups/build
cmake --build lookups/build
ctest --test-dir lookups/build          # GoogleTest suite
lookups/build/pushfold_lookups_bench    # Google Benchmark
```

## Dependencies

OpenMP is optional and parallelizes `EquityGenerator::Solve`; without it the
`#pragma omp` directives are no-ops and generation runs serially. On macOS,
install it with `brew install libomp` — CMake's `find_package(OpenMP)` can't
autodetect Homebrew's install location on its own, so `CMakeLists.txt`
queries `brew --prefix libomp` and hints the paths itself when present.

GoogleTest and Google Benchmark are fetched via `FetchContent` and only
built for the top-level build (see above).
