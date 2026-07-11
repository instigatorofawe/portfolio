# dsplib

A DSP library with a header-only C++ core and Python bindings (nanobind), built
via [scikit-build-core](https://scikit-build-core.readthedocs.io/).

## Layout

| Path                | What it is                                                        |
| ------------------- | ----------------------------------------------------------------- |
| `include/dsplib/`   | Header-only C++ core (`dsplib::core` INTERFACE target). `*Test.cpp` files are colocated with the headers they cover. |
| `nanobind/`         | The nanobind extension (`_dsplib`) and its stub generation.       |
| `dsplib/`           | The Python package that re-exports the compiled extension.        |
| `bench/`            | Google Benchmark microbenchmarks.                                 |
| `tests/`            | pytest suite exercising the installed Python package.             |
| `stubs/`            | Generated `.pyi` stubs (build artifact; git-ignored).             |

## Python build

The header-only `dsplib::core` is the library itself; the nanobind extension is
one way to exercise and verify it and to make it usable from Python. It builds
through CMake whenever this is the top-level project (including scikit-build
wheel builds).

```sh
pip install -e .          # build + install the extension into the dsplib package
pytest                    # run the Python tests
```

Dev tooling (pyright, black, pytest) lives in the `dev` dependency group:

```sh
pip install --group dev .
pyright                   # type-checks against the generated stub in stubs/
black .
```

## C++ tests & benchmarks

GoogleTest and Google Benchmark are fetched and built **only** for a direct,
top-level CMake configuration — i.e. not when this project is pulled in as a
subdirectory and not during a scikit-build wheel build (`pip install`). Configure
the project directly to get them (nanobind must be importable, e.g. `pip install
nanobind`):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build          # GoogleTest
./build/dsplib_bench            # Google Benchmark
```

Building the CMake project also (re)generates the type stub at
`stubs/dsplib/_dsplib.pyi`, which pyright is configured to consume.
