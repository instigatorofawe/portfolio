cmake_minimum_required(VERSION 3.21)
project(pushfold-lookups VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

include(FetchContent)

# --- OpenMP (parallelizes EquityGenerator::Solve) ---
# Apple's stock clang doesn't ship an OpenMP runtime, so CMake's stock
# find_package fails even when Homebrew's libomp is installed. Retry with hints
# pointing at the brew prefix before giving up.
# Solve() is parallelized with OpenMP, but it's an optional accelerator: without
# it the `#pragma omp` directives are ignored and Solve() runs serially, so we
# don't require it. Apple's stock clang doesn't ship an OpenMP runtime and
# CMake's FindOpenMP can't autodetect Homebrew's libomp; supply the hints up
# front so the single find_package call below can validate them. (Setting hints
# only after a failed call leaves empty cache entries that break reconfigures.)
if(APPLE)
    find_program(BREW_EXECUTABLE brew)
    if(BREW_EXECUTABLE)
        execute_process(
            COMMAND ${BREW_EXECUTABLE} --prefix libomp
            OUTPUT_VARIABLE LIBOMP_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()
    if(LIBOMP_PREFIX AND EXISTS "${LIBOMP_PREFIX}/include/omp.h")
        set(OpenMP_CXX_FLAGS "-Xclang -fopenmp -I${LIBOMP_PREFIX}/include")
        set(OpenMP_CXX_LIB_NAMES "omp")
        set(OpenMP_omp_LIBRARY "${LIBOMP_PREFIX}/lib/libomp.dylib")
    endif()
endif()
find_package(OpenMP)
if(NOT OpenMP_CXX_FOUND)
    message(STATUS "OpenMP not found; EquityGenerator::Solve will run serially. "
                   "On macOS install it with `brew install libomp`.")
endif()

# --- OMPEval (fork with MacOS fixes + CMake support) ---
set(OMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    ompeval
    GIT_REPOSITORY git@github.com:instigatorofawe/OMPEval.git
    GIT_TAG 1c5f82495017f2503b5fa57126c54c28309c3c7f
)
FetchContent_MakeAvailable(ompeval)

# Treat OMPEval's headers as system headers so its warnings don't surface under
# our stricter warning flags below.
set_target_properties(ompeval PROPERTIES SYSTEM ON)

# Stricter warnings for our own (non-dependency) targets.
if(MSVC)
    set(PUSHFOLD_WARNING_FLAGS /W4 /permissive-)
else()
    set(PUSHFOLD_WARNING_FLAGS -Wall -Wextra -Wpedantic)
endif()

# Without OpenMP the `#pragma omp` directives in Solve() are harmless no-ops, but
# -Wall would flag them as unknown pragmas. Suppress that so a serial build stays
# warning-clean.
if(NOT OpenMP_CXX_FOUND AND NOT MSVC)
    list(APPEND PUSHFOLD_WARNING_FLAGS -Wno-unknown-pragmas)
endif()

add_library(pushfold_lookups INTERFACE)
target_include_directories(pushfold_lookups INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(pushfold_lookups INTERFACE cxx_std_23)
target_link_libraries(pushfold_lookups INTERFACE ompeval)
if(OpenMP_CXX_FOUND)
    target_link_libraries(pushfold_lookups INTERFACE OpenMP::OpenMP_CXX)
endif()

add_executable(generate_pushfold_lookups src/generate.cpp)
target_link_libraries(generate_pushfold_lookups PRIVATE pushfold_lookups)
target_compile_options(generate_pushfold_lookups PRIVATE ${PUSHFOLD_WARNING_FLAGS})

# Only build tests when this project is the top-level build.
if(PROJECT_IS_TOP_LEVEL)
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

    # Collect every *Test.cpp from our source dirs. CONFIGURE_DEPENDS re-globs at
    # build time so newly added test files are picked up without re-running cmake.
    # Scope to include/ and src/ rather than the project root: a tree-wide glob
    # would descend into build/_deps and, on case-insensitive filesystems, match
    # OMPEval's own test.cpp.
    file(GLOB_RECURSE pushfold_test_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*Test.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*Test.cpp"
    )

    add_executable(pushfold_lookups_tests ${pushfold_test_sources})
    target_link_libraries(pushfold_lookups_tests PRIVATE
        pushfold_lookups
        GTest::gtest_main
    )
    target_compile_options(pushfold_lookups_tests PRIVATE ${PUSHFOLD_WARNING_FLAGS})
    gtest_discover_tests(pushfold_lookups_tests)

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
    file(GLOB_RECURSE pushfold_bench_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/bench/*.cpp"
    )

    if(pushfold_bench_sources)
        add_executable(pushfold_lookups_bench ${pushfold_bench_sources})
        target_link_libraries(pushfold_lookups_bench PRIVATE
            pushfold_lookups
            benchmark::benchmark_main
        )
        target_compile_options(pushfold_lookups_bench PRIVATE ${PUSHFOLD_WARNING_FLAGS})
    endif()
endif()
