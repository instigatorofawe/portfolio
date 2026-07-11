cmake_minimum_required(VERSION 3.21)
project(dsplib VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# --- Eigen (header-only linear algebra) ---
FetchContent_Declare(
    Eigen3
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 5.0.1
)
# Pull in only the header-only library; skip Eigen's tests, docs, and pkg-config
# so MakeAvailable doesn't add a pile of unrelated targets to our build.
set(EIGEN_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(Eigen3)

# Treat Eigen's headers as system headers so they don't trip the stricter
# warning flags we apply to our own targets.
if(TARGET eigen)
    set_target_properties(eigen PROPERTIES SYSTEM ON)
endif()

# --- Core header-only library ---
# Everything under include/ is header-only; downstream targets (the nanobind
# extension, tests, benchmarks) link this INTERFACE target to pick up the
# include path and language standard.
add_library(dsplib_core INTERFACE)
add_library(dsplib::core ALIAS dsplib_core)
target_include_directories(dsplib_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(dsplib_core INTERFACE cxx_std_23)
target_link_libraries(dsplib_core INTERFACE Eigen3::Eigen)

# Eigen-specific optimizations for Release builds
target_compile_definitions(dsplib_core INTERFACE
    $<$<CONFIG:Release>:EIGEN_NO_DEBUG>      # Disable runtime checks in release
    $<$<CONFIG:Release>:EIGEN_FAST_MATH>     # Enable fast math for Eigen in release
    $<$<CONFIG:Release>:EIGEN_DONT_PARALLELIZE>     # Don't parallelize within Eigen
)

# Stricter warnings for our own (non-dependency) targets.
set(DSPLIB_WARNING_FLAGS -Wall -Wextra -Wpedantic)

# --- nanobind bindings ---
# The header-only dsplib::core is the actual library; the Python extension is
# one way to exercise and verify it and to make it usable from Python. Build it
# whenever we're the top-level project (a scikit-build wheel build is top-level
# too), and skip it when dsplib is pulled in as a subdirectory of a larger CMake
# build, which only wants dsplib::core. Building it requires Python and the
# nanobind package to be importable (provided automatically during a scikit-build
# wheel build; for a local `cmake -B build` install nanobind first, e.g.
# `pip install nanobind`).
if(PROJECT_IS_TOP_LEVEL)
    add_subdirectory(nanobind)
endif()

# --- Tests & benchmarks ---
# Only build the C++ test/benchmark tooling when this project is configured
# directly by a developer: skip it when we're a subdirectory of a larger build
# (PROJECT_IS_TOP_LEVEL) and when scikit-build is producing a wheel (SKBUILD).
# That keeps `pip install .` from fetching GoogleTest/Benchmark or compiling
# binaries that never ship in the wheel.
if(PROJECT_IS_TOP_LEVEL AND NOT SKBUILD)
    # --- GoogleTest ---
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )
    # Don't override the parent project's compiler/linker settings on Windows.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    include(GoogleTest)

    # Collect every *Test.cpp under include/. CONFIGURE_DEPENDS re-globs at build
    # time so newly added test files are picked up without re-running cmake.
    file(GLOB_RECURSE dsplib_test_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*Test.cpp"
    )

    if(dsplib_test_sources)
        add_executable(dsplib_tests ${dsplib_test_sources})
        target_link_libraries(dsplib_tests PRIVATE
            dsplib::core
            GTest::gtest_main
        )
        target_compile_options(dsplib_tests PRIVATE ${DSPLIB_WARNING_FLAGS})
        gtest_discover_tests(dsplib_tests)
    endif()

    # --- Google Benchmark ---
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.5
    )
    # We only want the library; skip its own test suite and the GoogleTest it
    # would otherwise pull in for that.
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)

    # Collect every *.cpp under bench/. CONFIGURE_DEPENDS re-globs at build time
    # so newly added benchmark files are picked up without re-running cmake.
    file(GLOB_RECURSE dsplib_bench_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/bench/*.cpp"
    )

    if(dsplib_bench_sources)
        add_executable(dsplib_bench ${dsplib_bench_sources})
        target_link_libraries(dsplib_bench PRIVATE
            dsplib::core
            benchmark::benchmark_main
        )
        target_compile_options(dsplib_bench PRIVATE ${DSPLIB_WARNING_FLAGS})
    endif()
endif()
